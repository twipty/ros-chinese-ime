# 字形分级与全字形开关 · 实施方案

- 状态：**方案评审稿，未实现**。评审日期：2026-11-19。
- 关联：`XINGMA-PLAN.md`（v1.1）、`TABLE-IMPORT-SPEC.md`（v1.1）、`DATA-PACK-SPEC.md`、`docs/font/MINIMAL-FONT-SET-PLAN.md`、`docs/font/FULL-GLYPH-COVERAGE-PLAN.md`
- 交付范围：**本文只做评估与设计，不改任何代码**。Implementation 需另行授权（涉及 `framework/` 与 `tools/install-rospinyin/`，超出 `xingma/` 目录）。
- 已确认决策（2026-11-19）：① 仅出方案文档；② 文档存 `xingma/docs/glyph-tier/`；③ "全字形"开关关闭时按 **Droid Sans Fallback 保底覆盖**口径过滤。
- **后续更新（2026-11-19）**：开关状态决策已变更——由 §4.2 的 HKCU 注册表持久化改为**会话级 `[快]/[全]`、默认 `[快]`、不存注册表**；framework 侧接口也随之调整（`show_all_glyphs()` / `toggle_glyph_mode()` 替代 `Host::can_render`）。**以 `FRAMEWORK-REQ.md` 为准**，本文 §4.2/§4.3/§6 中涉及注册表与接口签名的描述 retirement 为历史设计，不再执行。

---

## 0. 结论速览

| 项 | 结论 |
|---|---|
| 全字库重码 | 86 版重码编码 11.6%、98 超集 **32.3%**；98 版 57% 候选落在重码组，选字负担显著高于 86 版 |
| 字形覆盖 | 86 版 94.96% 候选 Droid 可显示；98 超集仅 **21.78%**，扩展 B–I 全军覆没 |
| 摒弃运行时探测 | **可行**，但需把判定前移到导入期固化到 dat，运行时只读标志；framework 的 `UiCanRender`/`GlyphProbe` 可删除 |
| 三项需求 | 均可通过"dat 加 tier 位 + 一个注册表开关 + 两处提示"实现，不需要运行时 GDI 探测、不需要多字体自绘 |
| 代价 | 分级标志按 Droid 固化，装了全字库字体的用户需手动打开"全字形"开关才能看到扩展区候选——这正是开关的用途 |
| 粗估人日 | 合计 **13–19 人日**（不含评审等待与 VM 实机） |

---

## 1. 重码评估（全字库）

### 1.1 口径与方法

- 数据：`pack.ini.in` 锁定 commit 的上游主表。
  - 86：`KyleBing/rime-wubi86-jidian` @ `4d08110b`，主表 `wubi86_jidian.dict.yaml`，89,259 条。
  - 98：`yanhuacuo/98wubi-tables` @ `6b8b6fb9`，`超集-rime格式码表.txt`（UTF-16LE），109,842 条。
  - 两条目数与 `DATA-PACK-SPEC.md` §4 的"约 8.9 万 / 11.2 万"吻合。
- 重码定义：同一 `code` 映射到 ≥2 个不同候选 `text`。同一 `(code,text)` 去重后统计。
- 覆盖判定：候选 `text` 中每个汉字码点都在 `DroidSansFallback.ttf` 的 cmap 内 = "保底可显示"；纯标点/假名条目不计入覆盖判定（视为可显示）。
- 临时脚本：`~/.workbuddy/tmp/xingma-font-check/measure_duplicates.py`（未入仓）。

### 1.2 重码总览

| 指标 | 86 版（极点） | 98 版（超集） |
|---|---:|---:|
| 条目 | 89,259 | 109,842 |
| 编码数 | 77,170 | 69,779 |
| 候选对（去重） | 89,256 | 109,842 |
| 平均每编码候选 | 1.157 | **1.574** |
| 重码编码 | 8,956（**11.61%**） | 22,511（**32.26%**） |
| 重码组内候选 | 21,042（23.57%） | 62,574（**56.97%**） |
| 最大重码 | 89（假名/标点组，`cojp`） | 48（异体字组，`fpgc`） |

### 1.3 重码分布（每编码候选数 → 编码数）

| 候选数 | 86 版 | 98 版 |
|---:|---:|---:|
| 1 | 68,214（88.4%） | 47,268（67.7%） |
| 2 | 7,001（9.1%） | 13,920（19.9%） |
| 3 | 1,522（2.0%） | 4,740（6.8%） |
| 4 | 313 | 1,911 |
| 5 | 68 | 881 |
| 6 | 13 | 416 |
| 7 | 6 | 237 |
| ≥8 | 33（最大 89） | 406（最大 48） |

- 86 版 88.4% 编码单码单候选，体验接近"盲打"；重码集中在 2 候选（9.1%）。
- 98 版只有 67.7% 单码单候选，**每 3 个编码就有 1 个重码**，且 406 个编码有 ≥8 个候选，键盘选字压力大。
- 86 版最大重码 89 是假名/标点（`クェープオゼマラパビ…`），非汉字重码，不代表中文输入负担；98 版最大重码 48 是异体字组（`㺉𧹲穀穀觳…`），是真实中文重码。

### 1.4 重码 × Droid 覆盖（关闭"全字形"后的效果）

| 指标 | 86 版 | 98 版 |
|---|---:|---:|
| Droid 保底可显示候选 | 84,754/89,256（**94.96%**） | 23,929/109,842（**21.78%**） |
| 全组可显示的编码 | 72,864（94.42%） | 11,614（16.64%） |
| 全组不可显示的编码 | 2,757（3.57%） | 49,579（**71.05%**） |
| 关闭全字形：重码组候选 | 21,042 → 19,218（过滤 8.67%） | 62,574 → 13,800（**过滤 77.95%**） |
| 降级为 ≤1 候选的重码编码 | 1,188 | 19,349 |

**解读**：
- 86 版关闭全字形几乎无感（只过滤 8.67% 重码候选），因为绝大多数字 Droid 都有。
- 98 超集关闭全字形会过滤掉 77.95% 的重码候选——这**恰好是特性**：把 8 万多个扩展区豆腐块候选藏起来，只留 2.4 万个能显示的字。代价是 19,349 个重码编码被降级，部分字在关闭状态下"打不出来"，但打开开关即可恢复。
- 因此开关默认关闭对 86 版无感、对 98 版是必要的豆腐块护栏；两者用同一套机制即可。

---

## 2. 现状架构（改造基线）

### 2.1 当前字形探测链（要被替换）

```
imm_entry.cpp:301  Host::can_render(cp)
        ↓ 实现为
ime_ui.cpp:298     UiCanRender(cp) → GlyphProbe::has(cp)
        ↓ 调用
ime_ui.cpp:274     GetGlyphIndicesW(dc, &wide, 1, &index, GGI_MARK_NONEXISTING_GLYPHS)  [GDI 实时探测 + 缓存]
        ↓ 使用点
host_key.cpp:65    renderable_candidates(host, result.candidates)  [每次候选生成都逐字过滤]
        ↓ 三处调用
host_key.cpp:109 / 216 / 243   （feed / commit 续打 / 其他路径重建 pager）
```

- `GlyphProbe` 是全局单例，带 `CRITICAL_SECTION` + `std::map<char32_t,bool>` 缓存；`codepoint > 0xFFFF` 直接 fail-open（返回 true）——**非 BMP 字根本没被探测，这是豆腐块泄漏的口子**。
- `renderable_candidates` 有护栏：若过滤掉一半以上或全部，判定"字体没有 CJK"并整体放行（fail-open）。
- 该探测是 framework 层能力，拼音与形码共用；删除它会影响拼音的 P1-2 护栏，需一并处理（见 §5 影响面）。

### 2.2 数据与控制面

- `Candidate` 结构：`framework/include/imfw/engine.hpp` 仅 `{ u32string text; int score; }`——**没有分级字段**，需扩展。
- `Host` 接口：`framework/include/imfw/host.hpp:101` `virtual bool can_render(char32_t) = 0;`——是运行时探测的抽象，删除后需给 Host 一个"当前策略"来源。
- `Scheme` 接口：`framework/include/imfw/scheme.hpp` 有 `configure()` / `reload_data()` 挂钩，但**没有运行时设置读取接口**——开关状态需要一个读取点。
- `shape.dat` v2（XINGMA-PLAN §4.1，尚未实现）：
  - `Entry (20B): text_off, text_len, rank, entry_type, flags`；`flags` 目前 bit0=仅组词，`bit1` 起空闲。
  - `Code (16B)` flags bit0–bit4 已用于简码级别提示。
  - → **Entry.flags 的 bit1–bit2 可作为字形分级位**（见 §3.1）。
- `RawEntry`（TABLE-IMPORT-SPEC §1）：已有 `uint8_t flags`，bit0 占用，可扩展。
- 安装器：`tools/install-rospinyin/install.cpp`，分步 `/step1`–`/step4` + `/status` + `/u`，写 `HKCU\SOFTWARE\RosPinyin\Install`；有 `log_error` 与 `MessageBox` 先例，可复用做提示。
- 配置工具：`rosxingma-config.exe`（规划中，W3/P4），导入向导与主界面都还没实现——提示机制可在规划时一次性纳入。

---

## 3. 需求 1：导入期字形分级，形成分级码表

### 3.1 分级模型

在 `shape.dat` 的每条 `Entry` 上打 **2 bit 的 `glyph_tier`**（存 `Entry.flags` bit1–bit2）：

| tier | 含义 | 判定规则 |
|---:|---|---|
| 0 | **保底**（Droid 有字形） | 候选 `text` 的每个汉字码点都在 Droid cmap 内 |
| 1 | **需扩展字体** | 含扩展 A/B/C/D/E/F/G/H/I 或兼容表意补充区的字 |
| 2 | **其它非保底** | 含兼容补充区、非汉字特殊符号但非纯标点者 |

纯标点/假名/西文候选固定 tier 0（本来就能显示）。

> 为什么用 2 bit 而不是布尔：保留"扩展区"与"其它"的区分，未来若按字库家族细化（如仅文津 P2 覆盖）不必改 dat 结构。tier 只表示"相对 Droid 的覆盖情况"，**不代表字形在任何字体下都能显示**。

### 3.2 dat 头汇总（供安装器提示用）

在 `shape.dat` v3 Header 新增（复用现有 `reserved[3]` 中的两个，保持 52B 不变，或在 v3 扩展区写入）：

```
uint32_t tier0_entry_count;   // tier=0 的 Entry 数
uint32_t tier12_entry_count;  // tier=1/2 的 Entry 数（即"未覆盖字形候选"数）
```

安装器读这两个数即可给出"本码表有 N 条候选需扩展字体支持"的提示，无需解析字符串池。

### 3.3 分级时机与责任方

| 环节 | 动作 |
|---|---|
| PC 端构建（`make_pack.py` / `rime2csv.py`） | 可选：生成 CSV 时预打 tier 列（离线、可复现） |
| 设备端导入（`libtableio` → `dat_writer`） | **权威分级点**：读 CSV 时按内置 Droid cmap 位图给 `RawEntry.flags` 打 tier，写入 dat |
| 字形表来源 | 一份精简的 cmap 位图（约 29,622 bit ≈ 3.7 KB）作为常量表编译进 `libtableio`，或随工具分发 `droid_cmap.bin` |

**为什么设备端是权威分级点**：用户可在设备上导入任意 CSV（不一定是打包表），分级必须在设备端完成才能覆盖所有来源。PC 端预打 tier 只作为加速与离线提示，设备端以自身 cmap 位图为准重新计算。

**cmap 位图来源可复现**：由 `m0/probe/reactos/Fonts/DroidSansFallback.ttf`（ReactOS ISO 自带，Apache-2.0）经 `fontTools` 导出为排序码点表，纳入 `check_shape_dat.py` 的一致性检查。

### 3.4 兼容性

- `shape.dat` v2 → v3：Header 里 version 升到 3；旧 v2 dat 由导入器检测到"无 tier"时按"全部 tier 0"处理（保守：不隐藏任何字，退回当前行为），并提示用户"码表未分级，建议重新导入"。
- `RawEntry.flags` bit1/bit2 在 v2 中未定义，旧工具忽略即可，前向兼容。

---

## 4. 需求 2：全字形开关

### 4.1 语义（已确认）

| 状态 | 行为 |
|---|---|
| **关闭（默认）** | 候选只保留 `tier == 0`（Droid 保底覆盖），绝无豆腐块 |
| **打开** | 显示全部 tier，包括扩展区字；若用户未装扩展字体，这些字显示为豆腐块——由开关提示文案负责告知"需字体支持" |

开关不探测系统字体（避免重新引入运行时探测），只按 dat 固化的 tier 过滤。

### 4.2 状态存储

- 注册表：`HKCU\SOFTWARE\RosXingMa\Settings` 下 `AllGlyphs`（REG_DWORD，0/1），缺省 0。
- 为什么 HKCU：每个 Windows 用户独立；安装器用 HKLM 写机器级信息、HKCU 写 HKL（见 install.cpp），此处沿用 HKCU。
- 配置工具写、IME 读；两侧用同一常量定义（放 `framework/include/imfw/` 的公共头，或 xingma 与配置工具共享的 `rosxingma_settings.hpp`）。

### 4.3 运行时链（替换 §2.1 的探测链）

```
Host::can_render(cp)              → 删除
host_key.cpp renderable_candidates() → 改为 read tier from Candidate.flags
        │
        ├─ 开关关闭 → 过滤掉 tier>0 的候选（保留 fail-open 护栏：全被过滤时整体放行）
        └─ 开关打开 → 不过滤
        ↓
CandidatePager → candidate_window.cpp 单一字体 TextOutW（不变）
```

- `Candidate` 结构加 `uint8_t glyph_tier`；（若不想动 framework 的通用 `Candidate`）可由形码 Scheme 在 `create_engine()` 时把 tier 塞进 `score` 的高位，framework 只认得分阈值——**但推荐直接加字段**，语义清晰、拼音侧 tier 恒 0 无影响。
- `Engine::feed()` 产出候选时携带 tier（从 `ShapeTable` 查 Entry.flags）。
- `Host` 新增 `virtual bool show_all_glyphs() const = 0;`（或 `virtual uint8_t glyph_tier_floor() const`），由 `imm_entry.cpp` 实现读注册表并缓存（**一次读取 + 变更时刷新**，不是每键读注册表）。
- `configure()` / `reload_data()` 成功后刷新缓存 + 重建 pager。

### 4.4 为什么这样能摒弃探测

- 判定完全来自 dat 中固化的 tier，不再调 `GetGlyphIndicesW`。
- `GlyphProbe`、`UiCanRender`、imm_entry 的 `can_render` 回调实现、`Host::can_render` 纯虚接口、`host_key.cpp` 逐字探测循环 —— 全部可删除。
- 拼音侧：`Candidate.glyph_tier` 默认为 0 → 新 `renderable_candidates` 对拼音无过滤效果，行为等价；删除探测不影响拼音。

### 4.5 开关与 fail-open 护栏

- 保留现有护栏精神：若开关关闭导致某编码候选**全被过滤**，则整体放行（退回显示 tier>0 候选），避免"这个字彻底打不出来"；同时通过 UI 提示"该字需开启全字形"。
- 这与"默认关闭=绝无豆腐块"有张力：全组都是扩展区字时，关闭状态要么空白、要么豆腐块。方案选择**放行 + 提示**（可用性优先），并在提示中说明原因。

---

## 5. 需求 3：安装器与导入工具的未覆盖提示

### 5.1 安装输入法时

- `install.cpp` 的 `/step1`（拷贝 dat 后）读 dat 头汇总：
  - 若 `tier12_entry_count == 0`：无提示。
  - 若 > 0：`MessageBox`/stderr 提示"本码表含 N 条候选需要扩展字体支持；默认已隐藏，可在输入法设置中开启'全字形'（需自行安装全字库字体，如文津宋体）"。
  - 不阻断安装（这是可用性提示，不是错误）。
- `/status` 增加一行 `GlyphTier: tier0=X, tier12=Y, AllGlyphs=0/1`。
- 文案与 `NOTICE.txt` §6 字体声明保持一致：**不捆绑字体**，提示指向 `docs/font/MINIMAL-FONT-SET-PLAN.md` 的推荐字体。

### 5.2 导入码表时

- `rosxingma-config.exe` 导入向导的"预览/确认"页：
  - 显示 `总候选 / 保底覆盖 / 需扩展字体` 三档计数与百分比。
  - 对 tier>0 的候选给"罕见字提示"标识（与 XINGMA-PLAN §6.3 的 rare 标识合并为一套，避免两个概念打架）。
  - 若 tier12 占比 > 50%（如 98 超集），弹二次确认："该表大部分字在保底字体下无法显示，建议开启全字形并安装扩展字体"。
- `rosxingma-table.exe` CLI：
  - `import --report` 输出分级统计（机器可读 JSON 可选）。
  - 退出码矩阵增加"有未覆盖字形"的告警码（但导入成功仍返回 0，告警走 stderr）。
- 提示必须"可解释、可导出"——延续 TABLE-IMPORT-SPEC §原则，每个被分级为 tier>0 的条目都能按源行号追溯。

---

## 6. 影响面清单

| 层 | 文件 | 改动 |
|---|---|---|
| 框架契约 | `framework/include/imfw/engine.hpp` | `Candidate` 加 `glyph_tier` |
| 框架契约 | `framework/include/imfw/host.hpp` | 删 `can_render`，加 `show_all_glyphs()` / `glyph_tier_floor()` |
| 框架逻辑 | `framework/src/host_key.cpp` | `renderable_candidates` 改读 tier；删逐字 GDI 探测循环 |
| 框架 UI | `framework/platform/win32/ui/ime_ui.cpp` | 删 `GlyphProbe` / `UiCanRender` |
| 框架 UI 头 | `framework/platform/win32/imm32/ime_ui.h` | 删 `UiCanRender` 声明 |
| 宿主 | `framework/platform/win32/imm32/imm_entry.cpp` | 删 `can_render` 回调实现；加开关读取与缓存 |
| 数据格式 | `xingma/include/roxingma/shape_dat_format.hpp`（待建） | v3 Header 汇总计数 + Entry.flags tier 位 |
| 导入 | `xingma/src/libtableio`（待建）、`dat_writer` | 打 tier；Droid cmap 位图常量；v2 兼容 |
| 工具 | `xingma/tools/mkdat-shape/make_pack.py`、`rime2csv.py` | 可选预打 tier + 统计报告 |
| 工具 | `xingma/tools/check_shape_dat.py`（待建） | 校验 tier 位与 cmap 一致性 |
| 配置工具 | `rosxingma-config.exe`（W3/P4） | 全字形开关 UI + 导入提示 |
| CLI | `rosxingma-table.exe`（W3/P4） | 分级统计报告 + 告警 |
| 安装器 | `tools/install-rospinyin/install.cpp` | dat 头读取 + 提示 + `/status` 扩展 |
| 文档 | `XINGMA-PLAN.md`、`TABLE-IMPORT-SPEC.md`、`DATA-PACK-SPEC.md`、`NOTICE.txt` §6 | 同步 v3/分级/开关/提示 |

> 这是**框架级改动**：framework 文件虽多，但删除探测链是净简化；风险点是拼音侧的等价性验证（ctest + M1 契约 + VM 冒烟）。

---

## 7. 门禁与验收

| 门禁 | 判据 |
|---|---|
| G-T1 分级正确性 | 对 86/98 两份真实表导入后，`check_shape_dat.py` 复核 tier 与 fontTools cmap 一致率 100%；v2 兼容路径通过 |
| G-T2 确定性 | 同一 CSV 两次导入字节一致（tier 位稳定） |
| G-T3 开关语义 | 关闭时候选 100% 为 tier0；打开时候选数 = 原始候选数；切换后 `reload_data` 生效 |
| G-T4 探测清除 | 全仓库 `grep` 无 `UiCanRender` / `GlyphProbe` / `GetGlyphIndicesW` 残留（注释除外） |
| G-T5 拼音回归 | 拼音 ctest 全绿、`check_m1.py` 全绿；VM 中拼音候选与改造前逐屏一致（tier 恒 0） |
| G-T6 提示 | 安装器对 98 表给出计数提示且不阻断；导入界面对 tier12>50% 给二次确认；CLI `--report` 输出分级统计 |
| G-T7 VM 实机 | ReactOS VM：86 表默认全可显示；98 表默认无豆腐块、开全字形后扩展字可显示（需装字体才验证字形，未装则验证"显示为豆腐块但开关生效"） |

---

## 8. 人日估算

| 子项 | 人日 |
|---|---:|
| 重码评估与字形分级器（含 cmap 位图、v2 兼容） | 3–4 |
| dat v3 格式与 `check_shape_dat.py` | 2–3 |
| 全字形开关（注册表、Host/Scheme 接口、运行时链重构） | 3–4 |
| 安装器提示 + 配置工具/CLI 提示 | 2–3 |
| 文档同步 + 门禁验证 + VM 冒烟 | 3–5 |
| **合计** | **13–19** |

不含：框架 X2-a0 注入缝、X3 全局单例隔离（P0 前置，见 `phase0/PHASE0-PLAN.md`）；全字库字体下载与随包（见 `docs/font/MINIMAL-FONT-SET-PLAN.md`，独立工程）。

---

## 9. 风险与决策点

| 风险/决策 | 级别 | 处置 |
|---|---|---|
| 删除 `Host::can_render` 影响拼音 P1-2 护栏 | 高 | G-T5 严格回归；拼音 `Candidate.glyph_tier` 恒 0，逻辑等价；必要时拼音侧保留一个恒 true 实现过渡 |
| 分级按 Droid 固化，换字体后失真 | 中 | 这是开关存在的原因；文案明确"全字形需字体支持"，不声称分级随字体变化 |
| 关闭时全组 tier>0 导致"字打不出来" | 中 | fail-open 放行 + UI 提示（§4.5），可用性优先 |
| 98 版关全字形过滤 77.95% 重码候选 | 中 | 属预期（藏豆腐块）；用分级统计让用户理解"表很大但保底可用字有限" |
| tier 只有 2 bit，未来字体家族细化不够 | 低 | bit3 预留；真需要时升 dat v4，导入器重算 |
| 配置工具/CLI 尚未实现（W3/P4） | 中 | 提示机制在规划中一次性纳入；实现排期跟随 P4/P5，本方案只定接口与判据 |
| 安装器提示文案需与 NOTICE §6 一致 | 低 | 提示指向推荐字体文档，不捆绑、不暗含授权承诺 |

**待 owner 决策**：
1. 是否接受"删 framework 探测链"作为本方案的一部分（还是分两步：先加 tier 后删探测）。
2. 全组 tier>0 时，选 fail-open 放行（本文默认）还是彻底隐藏。
3. `Candidate.glyph_tier` 加字段 vs 复用 `score` 高位（本文推荐加字段）。

---

## 10. 复现方法

1. `git clone --depth 1 <SourceUrl>` 两份上游到临时目录，校验 HEAD == `pack.ini.in` 的 `SourceCommit`。
2. fontTools 读 `DroidSansFallback.ttf` cmap（1 个 Unicode 子表，29,622 码点）。
3. 按 §1.1 口径解析两份表，按 `code` 聚合候选，统计重码与覆盖。
4. 临时脚本：`~/.workbuddy/tmp/xingma-font-check/measure_duplicates.py`、`dup-final.txt`（未入仓）。
