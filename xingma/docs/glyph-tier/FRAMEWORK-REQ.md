# 对 framework 的需求：字形分级、会话级模式开关与运行时探测链移除

- 状态：**需求评审稿，待 framework owner 确认**。提出日期：2026-11-19。
- 提出方：`xingma`（形码产品线）。接收方：`framework`（imfw 公共框架）维护方。
- 关联：
  - `xingma/docs/glyph-tier/GLYPH-TIER-PLAN.md`（xingma 侧实施方案与重码/覆盖实测）
  - `xingma/XINGMA-PLAN.md`（v1.1）、`xingma/docs/phase0/PHASE0-PLAN.md`（P0 门禁）
  - `docs/font/MINIMAL-FONT-SET-PLAN.md`（扩展字体方案，独立工程）
- 写边界：**本文件在 `xingma/` 内；`framework/` 当前零改动**。下文所有变更均为「期望 framework 实现的变更」，需 framework owner 授权后另行实施，xingma 侧不自行修改框架文件。
- 相对 `GLYPH-TIER-PLAN.md` 的更新：开关状态由「HKCU 注册表持久化」改为 **会话级 `[快]/[全]`、每次新建输入上下文默认 `[快]`、全程不读不写注册表**（§3 FR-5）。

---

## 0. 一页纸摘要

| ID | 需求一句话 | 主要改动文件 | 优先级 | 验收判据 |
|---|---|---|---|---|
| FR-1 | `Candidate` 携带 2 bit 字形分级 `glyph_tier` | `include/imfw/engine.hpp` | 高 | 默认 0；拼音侧不赋值即等价旧行为 |
| FR-2 | `Host` 以「模式读写」替代 `can_render` 探测 | `include/imfw/host.hpp` | 高 | `can_render` 移除；新增两个 pure virtual，spy 可测 |
| FR-3 | 候选过滤按 tier 而非逐字 GDI 探测 | `src/host_key.cpp` | 高 | 关闭时候选 100% tier0；全组 tier>0 时 fail-open |
| FR-4 | 彻底删除 `GlyphProbe` / `UiCanRender` / `GetGlyphIndicesW` | `ui/ime_ui.*`、`imm32/imm_entry.cpp` | 高 | 全仓库 grep 无残留（注释除外） |
| FR-5 | 会话级 `[快]/[全]` 开关，默认 `[快]`，不存注册表 | `session_state.hpp`、`imm_entry.cpp`、`host_key.cpp` | 高 | 新建 HIMC 即 `[快]`；切换即时生效；零注册表写入 |
| FR-6 | 工具栏 label 反映当前模式；快捷键翻转 | `ui/toolbar.*`、`imm32/ime_ui.cpp`、`host_key.cpp` | 中 | label 在 `形[快]`/`形[全]` 间切换 |
| FR-7 | 拼音侧行为等价、双 IME 同进程无新增全局态 | 全部 | 高 | ctest + `check_m1.py` 全绿；VM 拼音逐屏一致 |

**核心收益**：把「字形能否显示」从**每次候选生成的 GDI 逐字探测**前移为**导入期固化到码表的 2 bit 标志**，运行时只读标志 + 一个会话开关。framework 删除一个单例、一个临界区、一次 `GetGlyphIndicesW` 调用链，是净简化。

---

## 1. 背景与动机

### 1.1 实测依据（详见 GLYPH-TIER-PLAN.md §1）

| 项 | 86 版（极点） | 98 版（超集） |
|---|---:|---:|
| 重码编码占比 | 11.61% | 32.26% |
| Droid 保底可显示候选 | 94.96% | 21.78% |
| 关闭全字形后过滤掉的重码候选 | 8.67% | 77.95% |

98 超集把 Unicode 扩展 A–I 近 8 万字引入码表，而 ReactOS 自带 `DroidSansFallback.ttf` 的 cmap 仅 29,622 码点（扩展 B 以后为 0）。现状 `renderable_candidates` 依赖 `GetGlyphIndicesW` 逐字探测，但 `GlyphProbe::has()` 对 `codepoint > 0xFFFF` 直接 fail-open——**非 BMP 字根本没被探测，这是豆腐块泄漏的口子**。

### 1.2 为什么要 framework 配合

三级判定都跨越 framework 边界，xingma 无法独立实现：

1. `Candidate` 结构在 framework，tier 位要靠它传到过滤层；
2. 过滤函数 `renderable_candidates` 在 framework，探测链要在那里替换；
3. `Host` 接口与 Win32 宿主实现在 framework，模式开关与工具栏刷新要经它透出。

### 1.3 为什么开关不存注册表

用户决策（2026-11-19）：做成 `[快]` / `[全]` 模式切换，**每次启动输入法均为 `[快]`，用户需要时打开**。因此：

- 状态是**会话态**而非用户偏好，不需要跨进程持久化；
- IME 运行时当前对注册表**零依赖**（全仓库 `RegOpenKeyExW` / `RegSetValueExW` 只在安装器 `tools/install-rospinyin/install.cpp`），保持这个特性，卸载无残留；
- 状态放在 `ImeSessionState`（per-HIMC），由 IMM32 管理生命周期，与中英文 `shift_latched` / `open_initialized` 同一套机制，**不新增全局单例**——这一点尤其重要：`docs/FRAMEWORK-AUDIT-2026-11-17.md` 已把 `g_hInst` / `UiStats` / `GlyphProbe` 等单例列为双 IME 同进程冲突风险，不能再加一个。

---

## 2. 术语与不变量

| 术语 | 定义 |
|---|---|
| `glyph_tier` | 2 bit，0=保底（Droid cmap 内有字形），1=扩展区，2=其它非保底；纯标点/假名固定 0。分级在**导入期**固化进 dat，运行时只读 |
| `[快]` | 模式关闭（默认）：只放行 `glyph_tier == 0` 的候选，不出现豆腐块 |
| `[全]` | 模式打开：放行全部 tier；扩展字能否显示取决于用户是否安装扩展字体，framework 不探测、不保证 |
| HIMC | Windows 输入上下文；`ImeSessionState` 每 HIMC 一份，`release_session()` 时销毁 |
| fail-open | 当某编码候选**全部** tier>0 时，关闭模式仍整体放行，避免出现「这个字彻底打不出来」 |

**不变量（framework 必须保持）**：

1. 运行时**不得**重新引入任何字形探测（`GetGlyphIndicesW` / `GetFontUnicodeRanges` 等）；
2. 模式状态**不得**写入注册表、配置文件或任何跨进程持久化存储；
3. `Candidate.glyph_tier` 缺省为 0，未分级的 scheme（拼音、测试替身）行为与改造前一致；
4. 不得新增进程级可变全局状态；窗口类名、UI class 名仍由 Scheme 提供，双 IME 同进程隔离不变。

---

## 3. 需求条目

### FR-1 `Candidate` 携带字形分级

- **现状**：`framework/include/imfw/engine.hpp:23`：
  ```cpp
  struct Candidate {
      std::u32string text;
      int score = 0;
  };
  ```
- **期望**：
  ```cpp
  struct Candidate {
      std::u32string text;
      int score = 0;
      std::uint8_t glyph_tier = 0;  // 0=baseline, 1=extension, 2=other; engines that do not tier leave it 0
  };
  ```
- **动机**：过滤层需要一个 O(1) 可判定的标志，替代逐字 GDI 调用。
- **兼容**：缺省 0；`tests/*` 里构造 `Candidate` 的字面量无需改动；拼音 Engine 不赋值即保持旧行为。
- **验收**：`ctest` 全绿；`Candidate c; assert(c.glyph_tier == 0);` 可在新增单测中验证。

### FR-2 `Host` 以模式读写替代 `can_render`

- **现状**：`framework/include/imfw/host.hpp:101`：
  ```cpp
  virtual bool can_render(char32_t codepoint) = 0;
  ```
- **期望**（替换该纯虚函数）：
  ```cpp
  // Glyph mode: [快] (false, default) filters tier>0 candidates; [全] (true) keeps all.
  // Session-scoped, never persisted. The Win32 host reads/writes ImeSessionState.
  virtual bool show_all_glyphs() const = 0;
  virtual void toggle_glyph_mode() = 0;
  ```
- **动机**：`can_render(char32_t)` 是「逐字探测」的抽象；模式开关需要的是「一个会话级布尔」，语义完全不同，不能复用。
- **职责划分**：`Host` 是会话状态的**访问器**，不自己持有状态；状态实体在 `ImeSessionState`（FR-5）。Win32 Host 通过 `hIMC_` 定位会话，`toggle_glyph_mode()` 翻转后顺带刷新工具栏（FR-6），可移植层不接触 HIMC。
- **验收**：`tests/host_key_test.cpp` 的 spy Host 只需实现两个返回常量/翻转布尔的方法，不依赖 `<windows.h>`；`check_m1.py` 的接口契约检查同步更新并通过。

### FR-3 候选过滤按 tier 执行

- **现状**：`framework/src/host_key.cpp:65` `renderable_candidates()` 对每个候选的每个 codepoint 调 `host.can_render()`；`:78-86` 有两档 fail-open 护栏。调用点 `:109 / :216 / :243`。
- **期望**（签名不变，内部改判据）：
  ```cpp
  std::vector<Candidate> renderable_candidates(Host& host,
                                               const std::vector<Candidate>& input) {
      if (input.empty()) return input;
      if (host.show_all_glyphs()) return input;          // [全]: no filtering
      std::vector<Candidate> kept;
      for (const auto& c : input)
          if (c.glyph_tier == 0) kept.push_back(c);      // [快]: baseline only
      const auto dropped = input.size() - kept.size();
      if (dropped == 0) return input;
      if (kept.empty() || kept.size() * 2 < input.size()) {
          host.on_candidates_filtered(dropped, input.size(), true);
          return input;                                   // fail-open
      }
      host.on_candidates_filtered(dropped, input.size(), false);
      return kept;
  }
  ```
- **保留**：`on_candidates_filtered(dropped, total, kept_all)` 诊断回调的语义与调用时机不变，trace 日志格式不变。
- **验收**：新增 host_key 单测三组——① `[快]` + 混合 tier → 只剩 tier0；② `[全]` + 混合 tier → 原样返回；③ `[快]` + 全 tier1 → fail-open 原样返回且 `kept_all == true`。

### FR-4 移除运行时探测链

- **期望删除**：
  | 文件 | 删除内容 |
  |---|---|
  | `framework/platform/win32/ui/ime_ui.cpp` | `GlyphProbe` 类（含 `CRITICAL_SECTION`、`std::map<char32_t,bool>` 缓存、`GetGlyphIndicesW` 调用）、`glyph_probe()`、`UiCanRender()` |
  | `framework/platform/win32/imm32/ime_ui.h` | `UiCanRender()` 声明 |
  | `framework/platform/win32/imm32/imm_entry.cpp:301` | `bool can_render(char32_t) override { return imfw::UiCanRender(codepoint); }` |
  | `framework/include/imfw/host.hpp` | `can_render` 声明及其 P1-2 注释 |
- **验收门禁**：`grep -rn "UiCanRender\|GlyphProbe\|GetGlyphIndicesW" framework/ pinyin/ ime/ xingma/` 无代码残留（注释中说明历史沿革的除外）。

### FR-5 会话级 `[快]/[全]` 开关（不存注册表）

- **状态落点**：`framework/include/imfw/session_state.hpp` 的 `ImeSessionState` 增加：
  ```cpp
  // [快] (false, default) vs [全] (true). Per-HIMC, reset on new session;
  // never persisted to registry or disk.
  bool full_glyph = false;
  ```
- **生命周期语义**（需 framework owner 确认，见 §6 FR-D1）：
  - `acquire_session(hIMC)` 新建会话 → `full_glyph = false`（`[快]`），满足「每次启动输入法均为关闭」；
  - `release_session(hIMC)` / `reload_sessions()` 销毁会话 → 下次重建回到 `[快]`；
  - 同一 HIMC 内切换后保持，直到会话销毁；不同 HIMC 相互独立。
- **Win32 Host 实现**（`imm_entry.cpp`）：
  ```cpp
  bool show_all_glyphs() const override {
      auto* s = imfw::acquire_session(hIMC_);
      return s != nullptr && s->full_glyph;
  }
  void toggle_glyph_mode() override {
      auto* s = imfw::acquire_session(hIMC_);
      if (s == nullptr) return;
      s->full_glyph = !s->full_glyph;
      imfw::UiToolbarRefresh(hIMC_);      // 见 FR-6
  }
  ```
- **切换入口**：在 `host_key.cpp` 的 `wants_key()` 识别模式切换键（建议 `Ctrl+Shift+G`，走现有 `KeyEvent.ctrl` / `KeyEvent.vk`，不消耗其它应用快捷键）→ 调 `host.toggle_glyph_mode()`。切换键由 framework 定义还是 Scheme 提供，见 §6 FR-D2。
- **验收**：
  - 新会话首次候选 100% tier0；
  - 切换后**当前正在组合的候选立即重建**（`handle_key` 中 `state.pager.reset(renderable_candidates(...))` 已有此路径，切换后复用）；
  - 全仓库无 `RegOpenKeyExW` / `RegSetValueExW` / `CreateFile` 新增调用（IME 运行时仍零持久化写入）。

### FR-6 工具栏显示模式与即时刷新

- **现状**：`framework/platform/win32/ui/ime_ui.cpp:212`：
  ```cpp
  ctx->toolbar.update(g_hInst, scheme.toolbar_class(), scheme.toolbar_label());
  ```
  `Toolbar::update()` 的 label 来自 Scheme 静态值，`UiToolbarShow()` 在 IME 选中时调用一次。
- **期望**：
  1. `ime_ui.h` 新增 `void UiToolbarRefresh(rpimm::InputContext hIMC);`，读取当前会话 `full_glyph`，把 label 拼成 `scheme.toolbar_label() + (full ? L"[全]" : L"[快]")`，再调 `ctx->toolbar.update(...)`；
  2. `UiToolbarShow()` 改为调用 `UiToolbarRefresh()`（避免两处拼接逻辑）；
  3. `UiWindow` 现有的 `InvalidateRect` + `dock_bottom_right()` 已足够刷新，**不需要**新增鼠标消息处理（见 §6 FR-D2）。
- **约束**：label 总长控制在工具栏当前 44px 宽度内；若 `形[快]` 超出，允许把 `kToolbarWidth` 从 44 调到 56（`toolbar.cpp:33`），不影响其它窗口。
- **验收**：切换后 toolbar 文本在 `形[快]` / `形[全]` 间即时变化；新建会话回到 `形[快]`。

### FR-7 拼音侧等价与双 IME 隔离

- **拼音侧**：Engine 不填 `glyph_tier`（恒 0）→ FR-3 的 `[快]` 过滤对拼音无效果，`[全]` 也无效果；模式切换键对拼音可无操作或同样翻转但不影响候选（推荐：同样翻转、同样显示 label，保持行为一致）。
- **回归风险**：删除 P1-2 探测后，拼音侧不再有「字体没有 CJK 时整体放行」的兜底。见 §6 FR-D3——这是一个**需要明确接受的取舍**，不能默认声称「完全等价」。
- **双 IME**：`full_glyph` 是 per-HIMC 字段，不是进程全局；两个 IME DLL 同进程时各自的会话互不干扰，不引入新单例。
- **验收**：拼音 `ctest` 全绿、`check_m1.py` 全绿；VM 中拼音常用 3500 字候选逐屏与改造前一致；双 IME 同进程加载 + 各自切换模式，互不串扰。

---

## 4. 改造前后数据流

```
【改造前】每次候选生成
Engine::feed() → Candidate{text, score}
        ↓ host_key.cpp:65 renderable_candidates()
  逐候选逐 codepoint → Host::can_render(cp)
        ↓ imm_entry.cpp:301
  UiCanRender(cp) → GlyphProbe::has(cp) → GetGlyphIndicesW()   ← GDI 实时探测 + 全局缓存
        ↓ 逐字布尔
  CandidatePager → candidate_window TextOutW

【改造后】导入期一次 + 运行时只读
【导入期】CSV → libtableio 按 Droid cmap 位图打 tier → shape.dat v3 Entry.flags bit1-2
        ↓
【运行时】Engine::feed() → Candidate{text, score, glyph_tier}
        ↓ host_key.cpp renderable_candidates()
  Host::show_all_glyphs()?  ── [全] → 原样放行
                           └─ [快] → 只留 glyph_tier==0（全组 tier>0 则 fail-open）
        ↓
  CandidatePager → candidate_window TextOutW（单一字体，不变）

【模式切换】Ctrl+Shift+G → wants_key() → Host::toggle_glyph_mode()
        → ImeSessionState.full_glyph 翻转 → UiToolbarRefresh() 更新 label
        → 下一次 handle_key 重建 pager（当前组合即时生效）
```

---

## 5. 影响面清单（framework 侧）

| 文件 | 改动 | 大小 |
|---|---|---|
| `include/imfw/engine.hpp` | `Candidate` 加 `glyph_tier` | 1 行 |
| `include/imfw/host.hpp` | 删 `can_render`，加 `show_all_glyphs` / `toggle_glyph_mode` | ~4 行 |
| `include/imfw/session_state.hpp` | 加 `full_glyph` | 3 行 |
| `src/host_key.cpp` | `renderable_candidates` 改判据；`wants_key` 加切换键 | ~20 行 |
| `platform/win32/ui/ime_ui.cpp` | 删 `GlyphProbe` / `UiCanRender`；`UiToolbarRefresh` | −30/+15 行 |
| `platform/win32/imm32/ime_ui.h` | 删 `UiCanRender` 声明；加 `UiToolbarRefresh` | 2 行 |
| `platform/win32/imm32/imm_entry.cpp` | 删 `can_render` 实现；加两个模式方法 | ~12 行 |
| `platform/win32/ui/toolbar.cpp` | 仅当 label 超宽时调 `kToolbarWidth` | 1 行（可选） |
| `tests/host_key_test.cpp` | spy 换方法；加三组 tier 过滤单测 | ~40 行 |
| `tools/check_m1.py` | 接口契约断言同步 | 少量 |

**不在本需求内**（xingma / 其它团队负责）：dat v3 格式与 tier 位、导入期分级器与 Droid cmap 位图、`check_shape_dat.py`、配置工具与 CLI 报告、安装器提示、扩展字体打包。

---

## 6. 风险与待确认决策

| ID | 决策点 | 现状与建议 |
|---|---|---|
| **FR-D1** | 会话粒度是否为 per-HIMC（切窗口即回 `[快]`） | 建议 per-HIMC：与 IMM32 会话生命周期一致、零全局态。若要求「同一应用内跨窗口保持」，需改放 Host 侧的 per-process 映射，会重新引入共享可变状态，需权衡审计里的双 IME 风险 |
| **FR-D2** | 切换入口：快捷键 vs 工具栏点按 | 建议**快捷键 + label 显示**（本需求 FR-5/FR-6）：`UiWindow::wnd_proc` 当前无 `WM_LBUTTONDOWN` 分发，点按需给基类加 `on_click` 虚方法与命中测试，改动更大。若用户坚持「点按开关」，作为后续增强单列 |
| **FR-D3** | 拼音 P1-2 护栏删除的代价 | 实测 Droid 对拼音 dat 44,355 字覆盖约 37%，但日常候选集中在常用字（基本区覆盖约 79%、3500 常用字全覆盖）。建议删除探测，并以「VM 常用 3500 字逐屏一致」为验收；若不可接受，需为拼音单独保留探测（违背「摒弃探测」目标，不推荐） |
| **FR-D4** | 一步删探测 vs 两步（先加 tier 后删） | 建议**一步**：保留 `can_render` 过渡会让两套判定并存、`check_m1.py` 契约双倍维护。若 framework 排期紧，可分步但需明确删除日期 |
| **FR-D5** | 切换键具体键值与是否 Scheme 可配 | 建议 framework 固定 `Ctrl+Shift+G` 并把 `KeyKind::ModeToggle` 加入枚举；若多个 scheme 各自占用快捷键，则改为 `Scheme::mode_toggle_key()` 虚方法（默认 0 = 不启用） |

---

## 7. 验收门禁

| 门禁 | 判据 |
|---|---|
| G-FR-1 接口契约 | `check_m1.py` 全绿；`Candidate.glyph_tier` 默认 0；spy Host 可编译 |
| G-FR-2 过滤语义 | host_key 三组单测通过（混合 tier / `[全]` / 全 tier>0 fail-open） |
| G-FR-3 探测清除 | §FR-4 的 grep 门禁无残留 |
| G-FR-4 会话开关 | 新会话默认 `[快]`；切换即时生效；跨 HIMC 独立；无注册表/文件写入 |
| G-FR-5 工具栏 | label 在 `形[快]`/`形[全]` 间即时切换，宽度不溢出 |
| G-FR-6 拼音回归 | 拼音 ctest 全绿、`check_m1.py` 全绿；VM 常用字逐屏一致 |
| G-FR-7 双 IME | 两 IME 同进程加载，各自切换模式互不串扰 |
| G-FR-8 端到端（配合 xingma） | 86 表默认全可显示；98 表默认无豆腐块、`[全]` 后扩展候选出现（装字体才验证字形） |

---

## 8. 人日估算（framework 侧）

| 子项 | 人日 |
|---|---:|
| FR-1/FR-2 接口契约与 spy/HKCU 契约更新 | 0.5–1 |
| FR-3 过滤链重构 + 三组单测 | 1–1.5 |
| FR-4 探测链删除 + grep 门禁 | 0.5–1 |
| FR-5 会话开关 + 切换键 + Win32 Host 实现 | 1.5–2 |
| FR-6 工具栏刷新 + 宽度调整 | 0.5–1 |
| FR-7 回归验证 + VM 冒烟 | 1–2 |
| **合计（framework）** | **5–8.5** |

xingma 侧（dat v3、分级器、工具、提示、文档）另计 8–10.5 人日，合计与 `GLYPH-TIER-PLAN.md` 的 13–19 人日口径一致。

---

## 9. 交接清单（framework owner 开工前）

1. 确认 §6 五个决策点，尤其 **FR-D1**（per-HIMC 语义）与 **FR-D3**（拼音护栏取舍）；
2. 确认 `Candidate.glyph_tier` 加字段（vs 复用 `score` 高位）——本需求推荐**加字段**，语义清晰且拼音零影响；
3. 确认切换键 `Ctrl+Shift+G` 不与 ReactOS 系统快捷键或其它 IME 冲突；
4. 确认 `check_m1.py` 的接口契约断言由谁更新；
5. framework 开工与 xingma 的 dat v3 / 导入器并行，但 **G-FR-8 端到端验收需等 xingma 侧产出分级 dat**。

> 任何 framework 侧实现都不得绕过 §2 的四条不变量。若某项需求与现有 M0/M1/M2 门禁冲突，请先反馈再改，不要静默降级。
