# P0 前置门禁 · 细化实施计划（RosXingMa 第一阶段）

| 项 | 内容 |
|---|---|
| 文档版本 | 1.1（2026-11-23，P0 收口与判定） |
| 上位依据 | `xingma/XINGMA-PLAN.md` v1.5 第 8 章（阶段 P0） |
| 阶段定位 | **形码能否开工的框架门禁**——不是形码编码阶段，本身不产出 `rosxingma.ime` |
| 责任划分 | 框架侧实现；**形码侧只读复核 + 用例设计 + 就绪度判定** |
| 写操作边界 | 本文及本阶段形码侧全部产物只在 `xingma/` 内；`framework/`、`ime/`、`pinyin/`、根构建与根文档一律不改 |
| 基线 | ros-pinyin `main @ 51653db`（HEAD 与 `origin/main` 一致，2026-11-21 已推送；含 X1/X2-a0 与 pinyin v2 分级）；X1 起点为 `d070d80`（2026-11-18） |
| 质量基线 | `ctest 18/18`（`ROSPINYIN_BUILTIN_TABLE` ON/OFF 两配置各一遍）、`check_m1.py 111/111`、`check_docs.py 7/7`、首屏候选覆盖率 96.8%（2026-11-23 复核） |
| 预计人日 | 2–4 人日（框架侧；形码侧复核约 0.5 人日） |
| 失败后果 | P0 任一门禁未过 → P2（原 W1，引擎与 dat 内核）及之后的编码阶段**不启动** |
| **判定结论** | **P0 五项全部通过（2026-11-23）；P2 放行**。证据见 §3 各门禁实测段与 §5 checklist；形码侧硬约束「不写一行形码 C++」自判定日起解除 |

---

## 1. 为什么需要 P0

### 1.1 现状快照（截至 2026-11-23 收口复核，均已核实）

| 事实 | 证据 |
|---|---|
| M0/M1 完成；M2 VM 验收 **15/15 通过**，`alpha0.0.2` 已发布 | `docs/milestones/m2/M2-ACCEPTANCE.md` §3、`docs/releases/alpha0.0.2.md`；真机输入"中国"观察到连续 `WM_IME_CHAR 0x4E2D / 0x56FD` |
| 框架 G1–G4 已由框架侧解决；G1 TRANSMSGLIST ABI 缺陷已修且有真机证据 | `framework/README.md` 缺口处置记录、`xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` §2.1 |
| **X1 目录归位已提交**：`d070d80 refactor(framework): X1 目录归位，产物字节不变`；`framework/host/*` → `framework/platform/win32/imm32/`，`framework/ui/*` → `framework/platform/win32/ui/`（19 个 `git mv`）；`ime/CMakeLists.txt` 已同步新路径 | `git log --oneline` 可见 `d070d80`；`git status --short` 无 framework 重命名条目 |
| **X2-a0 注入缝已落地**：`SchemeRegistry`（创建/查询分离）+ `scheme_instance()` 变为 alias；toy scheme 由 `-DROSPINYIN_SCHEME=toy` 装配；CI 有「toy 与 pinyin 二进制不得相同」断言 | `framework/include/imfw/scheme_registry.hpp`、`framework/src/scheme_registry.cpp`、`ime/scheme_reg_toy.cpp`、`samples/toy-scheme/`、`.github/workflows/ci.yml`、`tests/{scheme_registry,toy_scheme}_test.cpp`；详见 §3.2 |
| 全局单例未做 per-instance 隔离（**X3 未落地，登记为 P3 风险**）：`g_hInst`、`UiStats g_stats`、`g_last_geometry`/`g_anchor_from_caret`、`WindowRegistry`。`GlyphProbe` 已于 2026-11-20 随探测链一并删除 | 架构审计 §3.8、`docs/plan/ROADMAP.md` X3 行；见 §3.5 |
| M0 红线 #2（禁写 `Preload`）已降级为部署偏好；实质约束改为「登录路径零失败初始化」 | N/N′ 两组各 3 次启动均正常；`docs/milestones/M0-REDLINE-REVIEW.md` §13、`docs/plan/ROADMAP.md` 前提变更框；形码文档已在 2026-11-23 收口时同步（§3.4） |
| `Scheme::ui_server_class()` 有 **≤15 字符硬约束** | `scheme.hpp:48-52` 契约注释；拼音 `RosPinyinUIServer`（17 字符）曾致 IME 从不加载，改为 `RosPinyinUISrv`（14）后恢复 |
| 文档已归位 `docs/{plan,milestones,reviews,font,handover,releases}/`，根目录只留 README | `docs/README.md`、`tools/check_docs.py`（7 项门禁） |
| pinyin v2 dat 分级与 `[快]/[全]` 已落地，形码可直接复用 tier 数据通路与过滤判据 | `docs/plan/DAT-FORMAT-V2.md`、`xingma/docs/glyph-tier/FRAMEWORK-REQ.md` 状态栏；FR-8 的策略字段 `auto_commit` 在形码头（拼音头恒 0），留待形码 P5 |

### 1.2 根因链：为什么 `xingma/` 曾零 C++（2026-11-23 已消解）

```
[历史] scheme_instance() 是链接期硬接线（无注入点）
   ├─→ 假 scheme 塞不进测试，宿主层零自动化回归（架构审计 P0-1）
   ├─→ 一个进程无法注册两个 Scheme，per-instance 隔离无法验证（审计 P0-2）
   └─→ 「第二个插件只需换 scheme_reg.cpp」从未被真实代码验证
          └─→ 形码按此假设写了 50–70 人日代码后才发现接缝不成立 = 灾难性返工

[2026-11-23 收口] X2-a0 已落地 SchemeRegistry 注入缝 + toy scheme 实证：
   ├─→ 注入 API 存在，create/query 分离，宿主经注册表查询（P0.2 通过）
   ├─→ toy scheme 装配只改 ime/ + samples/，framework/ 零 diff（P0.2 硬判据）
   └─→ 「第二个插件只需换 scheme_reg.cpp」已被 CI 每次构建验证
          └─→ 根本假设证实；X3 单例隔离仍缺，作为 P3 已知风险单列（不阻塞 P2）
```

因此 P0 的价值是**用最小成本先证伪或证实"第二个方案可接入"这一根本假设**。它不是形式主义门禁：XINGMA-PLAN 3.3 的整个装配模型都建立在这个假设之上。收口结论：假设成立，模型无需调整。

### 1.3 P0 的定位

- P0 通过 = 「框架已具备承载第二个 IME 的条件」被逐条证实；
- P0 不通过 = 形码的装配方案必须在写 C++ 之前调整（例如形码自建宿主层、或先推动框架补齐注入缝）；
- P0 **不改变**形码的产品定义、数据模型、工具链、合规结论。

---

## 2. 边界与责任分工

### 2.1 只读红线

形码侧在 P0 中**只做三件事**：

1. 读框架源码与门禁脚本，核对事实；
2. 设计用例与证据模板（写在 `xingma/docs/phase0/` 内）；
3. 依据判据判定"通过 / 不通过"，并把结论写回 `XINGMA-PLAN.md` 与本文状态栏。

形码侧**不做**：改 `framework/`、`ime/`、`pinyin/`、根 `CMakeLists.txt`、`check_m1.py`、任何根文档；不在框架仓库提交任何 commit。发现缺陷时，按 `xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` 的先例——只在 `xingma/` 内落档，由框架侧自行采纳。

### 2.2 责任矩阵

| 门禁 | 框架侧（实现/提交） | 形码侧（只读复核） |
|---|---|---|
| P0.1 X1 路径归位收口 | 已完成提交（X1 归位 + 路径同步） | 核对提交已完成、新路径存在、门禁全绿、字节比对 |
| P0.2 注入缝 + toy scheme | 实现 `SchemeRegistry` + toy scheme | 核对注入 API 存在、toy scheme 接入时 `framework/` 零 diff |
| P0.3 G1 词组上屏 | 维护 `emit_text` 与 ABI 断言 | 用 M2 真机证据 + 自造用例设计确认路径；形码自身真机用例推迟到 P3（见 §3.3 的切分说明） |
| P0.4 红线新口径 | 固化 A1/A1b（`DllMain` 零工作、`ImeInquire` 惰性注册且失败返回 FALSE） | 核对断言已改调用点白名单、安装器/`.ime` 契约已更新 |
| P0.5 全局单例 + UI 类名 | X3 per-instance 隔离规划、UI 类名断言 | 核对新方案类名长度、登记隔离缺口为 P3 风险 |

---

## 3. 五项门禁逐条细化

> 每门禁统一结构：**目标 → 现状 → 形码侧动作 → 通过判据 → 失败处置 → 证据留档**。
> 命令中的 `<repo>` 指 `/home/twipty/WorkBuddy/2026-11-12-20-02-47/ros-pinyin`，均只读。

### P0.1 X1 目录归位提交收口

**目标**：确认框架宿主/界面源码已稳定在新路径，形码的 `#include` 与装配清单引用唯一口径。

**现状**：X1 已提交（`d070d80`，2026-11-18 本次编写期间落地）；`ime/CMakeLists.txt` 与 `tools/check_m1.py` 已同步新路径；工作树中 X1 相关改动已清空。本门禁由"推动提交"转为"独立复核"。

**形码侧动作**：
```bash
git -C <repo> status --short                 # 确认 X1 已无 R/RM 条目（已提交）
git -C <repo> log --oneline -5               # 确认 commit 存在
ls <repo>/framework/platform/win32/imm32/    # 应含 dllmain/imm_entry/ui_server/session_store
ls <repo>/framework/platform/win32/ui/       # 应含 ime_ui/ui_window/composition_window/candidate_window/toolbar
```
核对 XINGMA-PLAN §2.2、§3.1、§3.3 与 README 的路径是否已与新布局一致（本次 v1.1 已同步，后续若框架再变须再次同步）。

**通过判据**（全部满足）：
1. `git status --short` 无 `framework/host/` 或 `framework/ui/` 的重命名条目；
2. 新路径下 9 个 `.cpp`（imm32 4：dllmain/imm_entry/session_store/ui_server；ui 5：ime_ui/ui_window/composition_window/candidate_window/toolbar）及对应头文件齐全；
3. `python3 tools/check_m1.py` → **111/111**（2026-11-23 当前基线；P0.1 立门禁时为 63/63）；`ctest --test-dir build --output-on-failure` → **18/18**（立门禁时 11/11）；`python3 tools/check_docs.py` → **7/7**；
4. 旧路径 `framework/host/`、`framework/ui/` 不再存在（除历史文档的"原路径"说明）；
5. 重编译 `.ime` 与归位前逐字节比对，只允许 PE 时间戳 `0x88`、校验和 `0xD8–0xD9`、调试字段 `0x60A04` 三处差异（X1 判据，见 `docs/plan/PLAN-MODULARIZATION.md`）。

**本次实测（2026-11-23 收口复核，只读运行）**：`check_m1.py` **111/111 PASS**；`check_docs.py` **7/7 PASS**；`ctest` **18/18 100%**（`ROSPINYIN_BUILTIN_TABLE` ON/OFF 两配置各一遍）；旧路径 `framework/host`、`framework/ui` 均不存在；新路径 9 个 `.cpp` 齐全。2026-11-18 首测为 63/63、11/11、7/7，两版均通过。**P0.1 判定：通过**（字节比对一项由框架侧 X1 提交"产物字节不变"采信，形码侧只读无法复现归位前构建）。

**失败处置**：X1 提交缺失或字节比对超差 → 不进入 P0.2；形码侧路径保持 v1.1 口径不动，把差异记入本文 §6 风险表。

**证据留档**：命令输出粘贴到 `xingma/docs/phase0/evidence/P0.1.md`（本报告未预建该文件，判定时新建）。

---

### P0.2 Scheme 注入缝 + toy scheme（最关键门禁）

**目标**：证实第二个 Scheme 可以在**不改框架**的前提下接入，把"插件接缝"从文档假设变成被真实代码验证的事实。

**现状（2026-11-23 已落地）**：`framework/include/imfw/scheme_registry.hpp` 声明 `SchemeRegistry`（`install()` 注入、`get()` 查询、`installed()` 查询是否已注入，创建与查询分离）；`framework/src/scheme_registry.cpp` 实现，`scheme_instance()` 已成为 `get()` 的别名，宿主/UI 全部经注册表查询；`ime/scheme_reg.cpp` 退化为三行 `scheme_factory()`；`ime/scheme_reg_toy.cpp` + `samples/toy-scheme/` 提供第二个消费者；`ime/CMakeLists.txt` 以 `-DROSPINYIN_SCHEME=pinyin|toy` 选择装配分支；`tests/scheme_registry_test.cpp` 与 `tests/toy_scheme_test.cpp` 覆盖注入与 toy 端到端。X2-a0 已关闭。

**框架侧交付（已完成）**：`SchemeRegistry` 注入缝 + toy scheme + CI 常驻验证。`check_m1.py` 有 X2-a0 静态断言；`.github/workflows/ci.yml` 的 win32-toy job 除构建外还断言 toy 与 pinyin 的 `.ime` **不得字节相同**（防止选项被忽略却假绿）。

**形码侧动作**：
```bash
grep -rn "SchemeRegistry\|scheme_instance" <repo>/framework/include/imfw/ <repo>/framework/platform/win32/imm32/
grep -rn "toy" <repo>/framework/ <repo>/samples/ <repo>/tests/ 2>/dev/null
```
核对：注入 API 是否存在、创建/查询是否分离、toy scheme 是否可作为独立装配件接入。

**通过判据**（全部满足）：
1. `framework/include/imfw/` 下存在注入 API 声明，且 `scheme_instance()` 不再被宿主/UI 直接硬调用（改为经注册表查询）；
2. toy scheme 可独立装配出一个最小 `.ime`（或在测试中注入），并能触发一次 `feed` → 候选 → 上屏；
3. **接入 toy scheme 时 `framework/` 下 `git diff` 为空**——这是 `PLAN-MODULARIZATION.md:118` 的硬判据；
4. 形码侧可据此确认：`xingma/host/scheme_reg_shape.cpp` 的装配模型（XINGMA-PLAN §3.3）无需框架改动即可成立；
5. `ctest` 全绿、`check_m1.py` 全绿（2026-11-23 基线 18/18、111/111）。

**本次实测（2026-11-23 收口复核，只读）**：
- 注入 API：`SchemeRegistry::install/get/installed` 存在；`scheme_instance()` 在 `framework/src/scheme_registry.cpp` 中定义为 `get()` 的别名；宿主与 UI 的调用点（imm_entry / ui_server / session_store / host_key / ime_ui）全部经 `scheme_instance()` 查询，无第二处 `scheme_factory()` 硬调用。
- toy scheme：`ime/scheme_reg_toy.cpp` 提供 `scheme_factory()` 返回 `ToyScheme`；`samples/toy-scheme/` 提供完整实现；`ime/CMakeLists.txt:26-31` 选择 toy 分支；`tests/toy_scheme_test.cpp` 验证 `feed` → 候选 → 上屏链路。
- 零 diff 硬判据：toy 装配改的是 `ime/` 与 `samples/`，`framework/` 无需任何改动即可容纳第二个 Scheme；CI win32-toy job 与 `check_m1.py` 的 X2-a0 断言常驻守这条线。
- 环境限制：本机 Linux 构建中根 `CMakeLists.txt:110 if(WIN32) add_subdirectory(ime)` 不纳入 `ime/` 目标，`-DROSPINYIN_SCHEME=toy` 不会被消费、不产出 `.ime`；`.ime` 装配实证以 CI（MSYS2/MinGW）为准，非门禁失败。

**P0.2 判定：通过**。路线 A/B 的历史分支已无需裁决（X2-a0 已落地）。

**证据留档**：`xingma/docs/phase0/evidence/P0.2.md`。

---

### P0.3 G1 词组上屏验证（切分为两段）

**目标**：确认"一次 `commit_text` 提交 ≥2 个字符"在多码点消息链路上完整上屏，且形码自己的宿主/窗口类名组合不吃亏。

**现状**：G1 TRANSMSGLIST ABI 缺陷已于 2026-11-17 修复；M2 VM 验收 15/15 通过，真机输入"中国"观察到连续 `WM_IME_CHAR 0x4E2D / 0x56FD`。但 M2 是拼音**单字引擎逐音节提交**——每次 `commit_text` 只有 1 个码点，走的是同一 `emit_text` 路径却未覆盖"单次提交多字"的场景。

**关键切分（避免循环依赖）**：
- **P0.3a（本阶段门禁）**：形码侧只读复核框架证据 + 设计形码词组用例。通过即可放行 P2。
- **P0.3b（推迟到 P3 装配阶段）**：等 `rosxingma.ime` 装配完成后，用形码自己的类名/宿主跑真机词组用例。

> 若把 P0.3b 也放进 P0，会形成"要先有形码代码才能验证形码能否开工"的死循环。切分理由记录在此，供后续评审追溯。

**形码侧动作**（P0.3a）：
1. 读 `framework/include/imfw/emit_text.hpp` 与 `framework/platform/win32/imm32/imm_entry.cpp` 的 `ImeToAsciiEx` 实现，确认消息从 `list->TransMsg` 写、容量取 `min(uMsgCount, kMaxEmit)`、有 `offsetof` 断言；
2. 读 `M2-ACCEPTANCE.md` §3 与 `m0/vm/evidence/m2-acceptance/result.md`，确认真机证据链完整；
3. 按下方模板设计 P0.3b 用例（写入 `xingma/tests/` 的测试计划，本阶段只设计不实现）。

**P0.3b 用例设计模板**（P3 阶段执行）：

| 用例 | 输入 | 期望 | 验证点 |
|---|---|---|---|
| C1 双字词组 | 导入自造 2 字表 → 输入编码 → 选第 1 候选 | 记事本出现 2 字，`WM_IME_CHAR` 连续 2 条且码点正确 | 消息从偏移 4 写、不被 IMM32 误读 |
| C2 三字词组 | 自造 3 字表 → 选词 | 3 字完整 | 非 BMP 代理对（如有）不截断 |
| C3 混合 | 单字 + 多字连续输入 | 逐条正确，无错位/丢字 | 组合期不残留候选 |
| C4 容量边界 | 构造接近 `kMaxEmitMessages=32` 的长词组 | 容量截断不产生半代理对 | `min(uMsgCount, kMaxEmit)` 生效 |

**通过判据（P0.3a）**：框架 ABI 契约断言存在且 M2 真机证据链完整；P0.3b 用例表已写入形码测试计划（本细化文档即为其落档）。

**本次实测（2026-11-23 收口复核，只读）**：`framework/include/imfw/emit_text.hpp` 与 `emit_text.cpp` 存在；`key_translate.cpp` 有 `static_assert(offsetof(rpimm_list, TransMsg) == sizeof(UINT))` 与 `static_assert(sizeof(TransMsgSlot) == sizeof(rpimm::TransMsg))`；`check_m1.py` 有「G1 ABI: ImeToAsciiEx takes TRANSMSGLIST」断言；`docs/milestones/m2/M2-ACCEPTANCE.md` §3 与 `m0/vm/evidence/m2-acceptance/result.md` 两份证据文件在（真机 `WM_IME_CHAR 0x4E2D / 0x56FD`）；上方 C1–C4 用例表已落档。**P0.3a 判定：通过**；P0.3b 仍按设计推迟 P3，用形码自身类名跑真机用例，未过不得发布形码。

**失败处置**：ABI 断言缺失或 M2 证据链断裂 → P0 不通过，形码侧只记录、推动框架侧补证。

**证据留档**：`xingma/docs/phase0/evidence/P0.3.md`；P0.3b 结果留档到 P3 的 VM 证据目录。

---

### P0.4 M0 红线新口径落地

**目标**：确认形码安装器与 `.ime` 按"登录路径零失败初始化"口径设计，而不是照抄已降级的旧红线文本。

**现状**：N/N′ 两组各 3 次启动证明——`ImmInstallIMEW` 由平台自己写 `Preload`，同一个 `.ime` 留在 `Preload` 里可一直开机；风险在"登录路径上执行可能失败的初始化"，不在"被 Preload 载入"。实质约束改为 A1/A1b：`DllMain` 零工作、UI 类注册移到 `ImeInquire` 惰性注册且失败时 `return FALSE`。`check_m1.py` 第 7 项已从字符串黑名单改为调用点白名单（59→63，2026-11-23 当前 111/111）。

**形码侧动作**：
1. 核对 `XINGMA-PLAN.md` §4.3 注册表段、§5.5 安装布局、§9.3 CI 检查、§10 验收标准已按新口径表述——**2026-11-23 收口时已补齐**：§4.3「不写 `Preload`」改为部署偏好并引 M0 红线降级结论；§9.3 CI 检查改以 A1/A1b 调用点白名单为主、不写 `Preload` 降为偏好；§10 验收 1 同步；
2. 读 `framework/include/imfw/scheme.hpp:47-53` 与拼音 `scheme.cpp` 的 UI 类注册方式，确认形码 `ShapeScheme` 设计遵循"惰性注册 + 失败返回 FALSE"——**已在 §5.2 表格后新增「登录路径契约（P0.4 / A1b）」段**：窗口类由框架在 `ImeInquire` 惰性注册、失败即返回 FALSE、`DllMain` 零工作；
3. 核对形码安装器设计不写 `Preload` 第一项、不硬编码 HKL（动态分配），默认仅部署偏好——§4.3、§5.5 与 §10 均为该口径。

**通过判据**（全部满足）：
1. `check_m1.py` 111/111（立门禁时 63/63）中与登录路径相关的断言是**调用点白名单**而非字符串扫描；
2. 形码侧设计文档中不再出现"绝不写 Preload"作为硬红线的表述（保留为部署偏好说明）——2026-11-23 收口已改；
3. `ShapeScheme` 设计含 `ImeInquire` 惰性注册与失败返回 FALSE 条款——§5.2 已补；
4. 安装器设计含动态 HKL、不写 `Preload` 第一项——§4.3 / §10 已明确。

**本次实测（2026-11-23 收口复核）**：`check_m1.py` 111/111 全绿；`XINGMA-PLAN.md` 全文 grep「Preload」仅剩 §4.3 部署偏好说明、§9.3「不作为静态硬红线」、§10 偏好表述与本文历史说明，无硬红线残留。**P0.4 判定：通过**（形码侧文档缺口已在 `xingma/` 内自行补齐，未改框架文件）。

**失败处置**：口径未更新 → 形码侧在 `xingma/` 内补齐引用；框架侧断言问题只记录不修改。

**证据留档**：`xingma/docs/phase0/evidence/P0.4.md`。

---

### P0.5 全局单例 / per-instance 隔离 + UI 类名硬约束

**目标**：确认双 IME 同进程共存的设计目标有框架侧路线，且形码类名不会踩 ReactOS 的 15 字符截断坑。

**现状**：
- 全局单例未隔离：`g_hInst`、`UiStats g_stats`（无锁自增）、`g_last_geometry`/`g_anchor_from_caret`、`WindowRegistry`；与 D1"两 IME 并存互不干扰"冲突。X3（2–3 天）承担 per-instance 隔离，排在 M4 并行或推迟。`GlyphProbe` 已于 2026-11-20 随字形探测链一并删除（少一个单例）。**X3 未落地是 P3 已知风险，不阻塞 P2。**
- UI 类名：`scheme.hpp` 明确 ≤15 字符；拼音曾因 17 字符导致 IME 从不加载。

**形码侧动作**：
1. 核对 XINGMA-PLAN §5.2 的四个窗口类名与 UI server 类名长度：`RosXingMaUI`(11) / `RosXingMaComp`(13) / `RosXingMaCand`(13) / `RosXingMaBar`(12)——**均 ≤15**，符合约束；
2. 确认形码 `ShapeScheme` 实现计划含 `static_assert` 断言类名长度；
3. 读 `docs/plan/PLAN-MODULARIZATION.md` X3 段，登记"全局单例未隔离"为 P3（原 W2）集成风险：两 IME 同进程加载时，工具栏标签/候选窗位置可能互相干扰；
4. 形码侧自身状态全部 per-session（`ShapeStore` 每进程实例、`ShapeSession` 每 HIMC 实例），不新增全局单例。

**通过判据**：
1. 形码四个类名长度均 ≤15，且实现计划含 `static_assert`；
2. X3 隔离计划已在框架路线图中登记（已核实：`ROADMAP.md` X3 行）；
3. 形码无新增全局单例，状态归属写明在 §5.1/§5.3 设计里；
4. P3 风险登记已写入主计划风险表（v1.1 已新增）。

**失败处置**：类名超长 → 立即在 `xingma/` 内改名（如 `RosXingMaBar` → `RosXMBar`）；隔离缺口 → 作为已知限制写进 P3 验收，不阻塞 P2。

**证据留档**：`xingma/docs/phase0/evidence/P0.5.md`。

---

## 4. 形码侧交付物清单（本阶段实际产出）

| 产物 | 位置 | 状态 |
|---|---|---|
| 主计划 v1.5（阶段重排 + 现状同步 + P0 判定） | `xingma/XINGMA-PLAN.md` | 2026-11-23 收口完成 |
| 目录导览同步 | `xingma/README.md` | 2026-11-23 收口完成 |
| P0 细化文档（本文 v1.1） | `xingma/docs/phase0/PHASE0-PLAN.md` | 2026-11-23 收口完成 |
| P0.3b 词组用例表（已含于本文 §3.3） | 本文 | 完成；执行留档到 P3 |
| 五项门禁判定证据（§3 各门禁「本次实测」段） | 本文 §3.1–§3.5 | 2026-11-23 完成；证据索引见附录 A |

---

## 5. 开工条件 Checklist（P0 → P2 的 gate）

历史约束：在以下每一项打勾前，**不写一行形码 C++**。2026-11-23 已全部打勾，约束解除，P2 放行。

- [x] P0.1：X1 已提交（`d070d80`）；新路径齐全；111/111、18/18、7/7 实测全绿；字节比对按提交信息采信（见 §3.1 实测段）
- [x] P0.2：`SchemeRegistry` 注入缝存在；toy scheme 接入时 `framework/` 零 diff；CI 常驻断言实证第二个方案可接入（见 §3.2）
- [x] P0.3a：G1 ABI 断言存在；M2 真机证据完整；P0.3b C1–C4 用例表已落档（见 §3.3）
- [x] P0.4：登录路径新口径已固化（A1/A1b）；形码 §4.3/§9.3/§10 已改部署偏好表述、§5.2 已补惰性注册条款；无旧红线表述（见 §3.4）
- [x] P0.5：四个类名 11/13/13/12 ≤15 且 §5.2 含 `static_assert`；X3 隔离已登记为 P3 风险；形码无新增全局单例（见 §3.5）
- [x] 主计划 v1.5 的状态栏、风险表、交叉引用与本文一致（2026-11-23 收口同步）

**全部打勾（2026-11-23）**：已在 `XINGMA-PLAN.md` 第 8 章标注"P0 已通过（日期 + 证据索引）"，**P2 开工放行**。唯一附条件项 P0.3b（形码自身词组真机用例）按切分设计推迟到 P3，未通过不得发布形码。

---

## 6. 风险与回退

| 风险 | 触发条件 | 处置 |
|---|---|---|
| ~~X1 提交后仍有路径漂移~~ | ~~并行工作区继续改动框架路径~~ | **已消解**（2026-11-23）：P0.1 复核新路径齐全、门禁全绿 |
| ~~注入缝排期靠后~~ | ~~X2-a0 未排进近期迭代~~ | **已消解**（2026-11-23）：X2-a0 已落地并通过 P0.2；路线 B 不再需要 |
| 并行工作区冲突 | 框架侧与形码侧同时改同一文件 | 遵守只读红线；按审计先例只在 `xingma/` 落档，不参与框架仓库提交 |
| 形码自身词组用例未覆盖 | P0.3b 推迟到 P3 | 在 P3 验收判据中强制保留（本文 §3.3 C1–C4）；未通过不得发布 |
| 双 IME 全局状态干扰 | X3 未落地即做双 IME 集成 | **P3 已知限制**：P0.5 不阻塞 P2；P3 验收以"单 IME 场景"为基线，双 IME 场景单列 |

---

## 7. 人日与排期

| 项 | 责任侧 | 人日 |
|---|---|---|
| P0.1 X1 收口 | 框架侧 | 0.5 |
| P0.2 注入缝 + toy scheme（X2-a0） | 框架侧 | 1–1.5 |
| P0.3a G1 证据复核 + 用例设计 | 形码侧 | 0.5 |
| P0.4 红线口径固化 | 框架侧 + 形码侧引用同步 | 0.5 |
| P0.5 类名/隔离核验 | 形码侧 | 0.25 |
| **合计** | | **2.75–3.25（约 2–4）** |

P0 与 P1（原 W0，立项与合规，已实质完成）并行；P0 通过是 P2 的硬门禁。

---

## 附录 A 证据索引

| 主题 | 位置 |
|---|---|
| G1 ABI 缺陷审计与修复 | `xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` §2.1 |
| M2 VM 验收结果 | `docs/milestones/m2/M2-ACCEPTANCE.md` §3、`m0/vm/evidence/m2-acceptance/result.md` |
| 框架缺口处置记录 | `framework/README.md` |
| Scheme 契约（含 15 字符硬限） | `framework/include/imfw/scheme.hpp:47-53,128-150` |
| 注入缝实现与验证 | `framework/include/imfw/scheme_registry.hpp`、`framework/src/scheme_registry.cpp`、`ime/scheme_reg_toy.cpp`、`samples/toy-scheme/`、`tests/{scheme_registry,toy_scheme}_test.cpp`、`.github/workflows/ci.yml` win32-toy job、`tools/check_m1.py` X2-a0 断言 |
| 注入缝计划 | `docs/plan/PLAN-MODULARIZATION.md` X2-a0、X3 |
| 路线图与阶段顺序 | `docs/plan/ROADMAP.md` X1/X2/X3 行 |
| M0 红线降级 | `docs/milestones/M0-REDLINE-REVIEW.md` §13、`docs/plan/ROADMAP.md` 前提变更框 |
| 架构审计原始发现 | `code-audit/ros-pinyin-架构优化分析.md`（仓库外）、`docs/reviews/ARCH-AUDIT-VERIFICATION.md` §6 |
| 质量门禁脚本 | `tools/check_m1.py`、`tools/check_docs.py`、`tools/coverage_report.py` |

> 所有路径相对 ros-pinyin 仓库根，除 `xingma/**` 与 `code-audit/**`。
> 本阶段形码侧对以上框架文件全部只读。
