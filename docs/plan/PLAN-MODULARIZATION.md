# 模块化与前端抽象的落地计划（方案 B 精简版）

> 状态：**计划稿**（2026-11-18，v2：并入架构审计复核结论）。只排程与定判据，**不实现、不冻结任何接口**。
> 依据：`FRAMEWORK-ROUTE-A-REVIEW.md`（路线 A 三份文档的实证评审）§4–§5，
> 以及 [`ARCH-AUDIT-VERIFICATION.md`](../reviews/ARCH-AUDIT-VERIFICATION.md)（2026-11-18 架构审计的逐条复核）。
> 路线 A 三份文档（`~/Documents/FRAMEWORK-ROUTE-A-*.md`）的定位是
> **"完全抽象化那一天的参考资料"，不是本项目的执行计划**。

## 0. 一句话

把"能模块化"和"可完全抽象"拆成两件事：**前者现在做**（便宜、可逆、有验收），
**后者只预留缝**——不定义接口、不承诺 ABI、不写死"前端"这个词。

审计复核后，这个判断不变，但**"前者"的范围要扩大**：原计划只覆盖"目录 + 键路由 + 玩具方案"，
漏掉了审计点出的三件事——**注入缝、库边界、构建收敛**。它们不是新目标，是同一个目标的另一半：
"能模块化"如果没有库边界和注入点，就只是目录好看。

## 1. 目标与非目标

**要拿到的（可验收）**

| # | 目标 | 判据 |
|---|---|---|
| G1 | 目录树自解释：看路径就知道哪层是平台无关、哪层是 Win32 | `framework/` 下不存在"含义靠猜"的目录名 |
| G2 | 键路由有 Linux 单测 | `route_key` 的分支覆盖现有全部分支，ctest 出现新目标 |
| G3 | 宿主层有一条冒烟测试 | 假 engine/scheme/sink + 固定键序，断言 sink 调用序列；**这是后续任何重构唯一的自动化安全网** |
| G4 | 第二个方案"只加文件夹"接入 | `git diff --stat` 不出现 `framework/` 任何文件 |
| G5 | 未来换前端时改动被限制在 seam 内 | seam 清单里的调用点数量有上限断言（`check_m1` 新增一项） |
| G6 | 构建有单一入口 | `cmake --preset linux-ci` / `--preset win32-mingw` 两条路径，零手工 `-D` 即复现 CI 与发版 |
| G7 | 版本只有一个真值 | 仓库内发版版本号只出现在 `ime/version.h`；其余位置出现即 CI 失败 |

**明确不做**

- 不冻结 `Frontend` / `Plugin` ABI（现在只有一个实现，冻结等于猜——这正是三份文档自己点名的 R-1 风险）；
- 不做"把引擎挪出 csrss"（阶段 2）：与"让第三方代码进 csrss"的插件目标价值方向相反，先不碰；
- 不重写 UI 层为平台无关（Win32 窗口代码没有第二个平台消费者，重写是纯成本）；
- **不做 `.dat` 的 mmap/异步/预加载**（审计 A-P2-10）：属性能，归 M4/M5，与结构治理目标不同；
- **不重做 `kBuiltin` 的生成管线**（审计 A-P1-7 的数据部分）：M4 扩码表必然重做，现在做是重复劳动。

## 2. 现状基线（2026-11-18 21:5x 实测，`687b3b8` 之后）

> **这批数字会失真**：上一版快照写 `ime/*.cpp = 57`，今天实测 19；`framework/host+ui` 上一版记 2584，
> 现在 2697（+113，一天之内）。所以每项都附生成命令——**用命令，别引用本文的数字**。

```bash
wc -l framework/src/*.cpp framework/include/imfw/*.hpp | tail -1   # 993   可移植层
wc -l framework/host/* framework/ui/*                 | tail -1   # 2697  平台层（Win32）→ 0 测试
wc -l ime/*.cpp                                       | tail -1   # 19    装配
wc -l tests/*.cpp                                     | tail -1   # 1314  测试（全 host-side）
python3 tools/check_m1.py | tail -1                               # 63 项 契约断言（静态）
(cd build && ctest -N | tail -1)                                  # 11 项 全在可移植层
```

| 文件 | 行数 | 性质 |
|---|---|---|
| `framework/host/imm_entry.cpp` | 783 | IMM32 入口 + 键路由 + 会话，**最重**（6 类职责混在一个 TU） |
| `framework/ui/ime_ui.cpp` | 300 | 窗口编排、锚点、`GlyphProbe` 字形探针 |
| `framework/ui/ui_window.cpp` | — | 几何单一 owner；开头 28 处 `#ifndef` 重定义 Win32 常量 |
| `framework/include/imfw/{engine,scheme}.hpp` | 72 / 131 | 可移植契约，**Win32 类型 0 处** |

**接缝现状（这是本计划的地基）**：`imm_entry.cpp` 对外部世界的依赖只有五项，数量都很小，
所以抽象成本可控、但**现在不值得为它冻结接口**：

| seam | 现状调用点 | 抽象后要动的半径 | 备注 |
|---|---|---|---|
| 引擎（`state->engine->*`） | 6 处 | 全在 `process_key` / `apply_commit` | 引擎由 `scheme_instance().create_engine()` 产出 |
| 方案策略（`scheme.*`） | 13 处 | 全在 `process_key` | |
| UI 出口（`UiShow/UiHide/UiRefresh/UiCountersSnapshot`） | 5 处 | 全在 `process_key` + `refresh_ui` | |
| IMM32 ABI（`imm32_abi.h` + 16 导出） | 1 个文件 | 只在 `imm_entry.cpp` | |
| **字形能力（`UiCanRender`）** | 2 处 | `renderable_candidates`（`imm_entry.cpp:351+`） | **审计 A-P2-9 新增**：不替换它，CI 上的冒烟测试恒为"全部可渲染"，测不出字体相关回归 |

**结构缺口（审计复核后新增，原计划没覆盖）**

| 缺口 | 证据 | 为什么它属于本计划 |
|---|---|---|
| 无注入点 | `scheme_instance()` 是链接期硬接线（`scheme.hpp:129` → `scheme_reg.cpp:14`）；第二方案靠**换一个 `scheme_reg.cpp` 再编译**（`ime/CMakeLists.txt:7` 注释自述） | G3 要假 scheme、G4 要第二个方案，两者都过不去 |
| 库边界不一致 | `framework/CMakeLists.txt` 只构建 `imfw-engine`（5 cpp）；`framework/host`+`ui` 2697 行**无库目标**，由 `ime/CMakeLists.txt:26-35` 用 9 条相对路径直接编进 `.ime` | G1 说"目录自解释"，但没有库边界时目录只是名字 |
| 构建碎片 | `EXISTS` 守卫 4 处、`CMAKE_SOURCE_DIR`/`PROJECT_SOURCE_DIR` 混用、C 标准未设、无 `CMakePresets.json`、静态链接策略 3 份、`build-win` 与 `build-win-trace-en` 测试开关漂移 | G6；且 X 阶段的验收要靠可复现构建才有意义 |
| 版本四处 | `CMakeLists.txt:3`=`0.0.1`、`pinyin/version.hpp:9`=`0.0.2-m2-dev`、`imfw/version.hpp:14`=`0.1.0-imfw`、`ime/version.h`=`alpha0.0.2` | G7；`0.0.1` 这个最危险（`project(VERSION)`，一旦被用于产物命名就带错版本） |

## 3. 四个阶段

### X1 目录归位（0.5–1 天，纯 `git mv`）

```
framework/host/{dllmain,imm_entry,ui_server,session_store,imm32_abi,trace_stamp}
        →  framework/platform/win32/imm32/
framework/ui/{ime_ui,ui_window,composition_window,candidate_window,toolbar}
        →  framework/platform/win32/ui/
framework/src/、framework/include/imfw/、pinyin/                         不动
```

**为什么叫 `platform/win32/imm32` 而不是 `frontend/imm32`**：评审里的方案 B 用的是 `frontend/`，
这里改掉。`frontend` 预设了一个尚不存在的抽象层名字，而我方约定"接口不冻结"——
目录名不该比代码先做决定。`platform/win32/` 只陈述事实（这段代码就是 Win32 + IMM32），
将来真出现第二个前端，它作为**并列目录**加进来即可，不需要改造旧名。

改动面（已实测）：

| 位置 | 处数 | 说明 |
|---|---|---|
| `ime/CMakeLists.txt` | 9 条源文件路径 + 3 条 include 目录 | 相对路径 `../framework/host|ui/` |
| `pinyin/host/dat_source_win.cpp` | 1 条 include | 跨层引 `trace_stamp.h`（见 X2-e） |
| `tools/check_m1.py` | 15 处硬编码路径 | 含 `framework/ui/README.md` |
| 文档 | 若干 | 只改路径，不改论述（`tools/check_docs.py` 会盯着链接） |

**验收（硬）**：功能等价不看测试，看**字节**——用同一工具链重建，`.ime` 与 X1 前逐字节比对，
只允许 4 处差异（PE 时间戳 `0x88`、校验和 `0xD8–0xD9`、调试字段 `0x60A04`）。
这比任何单测都硬：路径改名若漏了某个源文件，链接产物一定变。

**回滚**：`git revert` 单个提交即可，无逻辑变更。

---

### X2 安全网 + 构建收敛（6–8 天，**在 M3 开工前**）

原计划 X2 只有"剥 `handle_key` + 冒烟 + 路由"。审计复核后扩成五步，
**顺序是硬的**——理由在下面每一步里。

**X2-a0 注入缝 + toy scheme（1–1.5 天）**

`Scheme& scheme_instance()` 换成可注册的上下文：`imfw::SchemeRegistry`（进程内一个实例，
但**创建与查询分离**，允许在测试里 `install(fake)`）。toy scheme 同时落地，
作为注入缝的第一个消费者（这本来排在 X3）。

> **为什么必须排第一**：X2-b 要"假 scheme"，但现在的代码里塞不进去
> （`session_store.cpp:34` 直接调全局工厂）。这条不先做，后面每一步都要改 framework 才能测，
> 而 X3 的判据（`framework/` 零 diff）会在最后一步把之前的偷懒全部打回去。

判据：`-DROSPINYIN_SCHEME=toy` 能构建出可跑的 `.ime`；且该提交 `git diff --stat` **不出现 `framework/`**
（G4 判据提前达成，X3 只剩打包与文档）。

**X2-a `handle_key` 剥离（0.5 天）**

把 `process_key` 从 IMM32 ABI 里剥出来：`handle_key(Session&, KeyEvent) -> KeyResult`，
入口函数只做 ABI 转换（`lpbKeyState` → `KeyEvent`，`KeyResult` → `TransMsgList`）。
这一步不改变行为，但让宿主第一次能脱离 imm32 头文件被测试。

**X2-b 宿主冒烟测试（1.5–2 天）**

`tests/host_smoke_test.cpp`：假 engine（固定返回序列）、假 sink（记录 `UiShow/UiHide/UiRefresh`
调用顺序与参数）、假 scheme（字母表 `a-z` + `1-9` 选字 + `=`/`-` 翻页）、
**假 `UiCanRender`（恒真，或按码点表返回）**。
键序固定为：`zhongguo` → `2`（选字）→ `Backspace`×2 → `Esc` → `Shift`（lone-Shift 切换）。
断言：sink 调用序列逐项相等 + `commit` 文本序列。**约 120 行，是全天最值钱的一百行。**

**X2-c `route_key` + `Intent`（0.5 天）**

只搬判定（`imm_entry.cpp:421-464` 附近那 44 行），产出
`enum class Intent { None, SelectDigit(n), PagePrev, PageNext, Backspace, Escape, InputChar(c), ToggleLang }`，
`process_key` 保留副作用（改 pager、刷 UI、调引擎）。
验收：`host_smoke` 在 X2-c 前后调用序列**逐字节一致**；`route_key` 的单测覆盖每个 Intent。

**X2-d 拆 god object（1–1.5 天）**

`imm_entry.cpp` 783 行按 6 类职责拆 TU：trace（`:48-176`）、键翻译与 `emit_text`（`:178-242`）、
UI 刷新（`:244-271`）、候选过滤（`:344-382`）、键路由（`:409-500`）、16 个导出（`:506+`）。
顺手两件事：

- **合并两条激活路径为 `activate_session(hIMC)`**（`ImeSelect:581-594` 与 `ImeSetActiveContext:650-655`）。
  注意它们**当前顺序相反**（`on_select` 与 `AlignOpenStatus` 互换），统一顺序
  ——今天 U1 那个"首次激活是关的"bug 就是这类分散路径造成的（见复核文档 N-1）。
- trace 的十进制转换有两份（`:128` 与 `:141`），合并。

判据：仅 `host_smoke` 序列不变 + 真机 `nihao` 冒烟一次；**不看字节**（拆 TU 本就该改字节）。

**X2-e 构建收敛（1.5–2 天）**

| 子项 | 出处 | 做法 |
|---|---|---|
| Win32 层成为库目标 | A-P1-4 | `platform/win32/{imm32,ui}` 两个 `add_library`，`ime/` 只 `target_link_libraries` |
| 插件自洽 | A-P1-5 | `dat_source_win.cpp` 纳入 `rospinyin-engine` 的 Windows 配置；可执行文件加**链接期符号存在断言**（`load_system_table` 非 weak 版本） |
| presets + 硬失败 | A-P1-6 | `CMakePresets.json`（`linux-ci` / `win32-mingw` / `win32-trace`）；`EXISTS` 守卫改硬失败；统一 `PROJECT_SOURCE_DIR`；显式 C 标准；静态链接策略收成 `cmake/StaticRuntime.cmake` |
| 平台补丁归位 | A-P2-11 | `ui_window.cpp` 那 28 处 `#ifndef` 与 `ime_ui.cpp:27-32` 的重定义，移到 `syntax_stub` 侧或加条件编译隔离，生产代码不再"猜常量值" |
| 版本收敛 | A-P1-7 版本部分 | 只留 `ime/version.h`；`CMakeLists.txt` 的 `project(VERSION)` 改为 `${IMFW_...}` 无关值或从 `version.h` 读；`check_docs.py` 或 `check_release_version.py` 加断言：其余三处不得出现四位版本号 |
| 跨层 include | 复核 N（X1 表） | `pinyin/host/dat_source_win.cpp` 不再引宿主私有头，`trace_stamp.h` 降为 `imfw` 内部可见或复制最小接口 |

> **为什么构建收敛排在冒烟测试之后，而原计划（以及审计原顺序）想放在 X1 之后**：
> X1 的验收判据是**字节比对**。一旦拆库目标、改链接顺序，字节**必然**变——判据当场失效，
> 你无法区分"拆库导致的"和"拆错了导致的"。只有行为测试能兜住这类改动。
> **判据失效的地方不能放改动**，这是本次吸收审计时唯一改了它顺序的地方。

---

### X3 方案可插拔验收（2–3 天，与 M4 并行，可延后）

X2-a0 落地后，X3 从"造玩具"降级为"验收 + 补隔离"：

**X3-a 示例打包与接入演练（1 天）**

1. `samples/toy-scheme/`（若 X2-a0 放在 `tests/fixtures/`，这里只做示例化与注释）；
2. `samples/README.md`：接入五步 + 常见坑（窗口类名冲突、`.dat` 格式、`Result.segments` 语义）；
3. 演练：按文档从零接入一次，记录"只加文件夹"的实测 diff。

**判据（写死）**：演练提交里 `git diff --stat` **不得出现 `framework/` 下任何文件**。
出现即说明框架还漏了耦合点，补框架而不是补示例。

**X3-b per-instance 隔离（1–2 天）**

审计 A-P1-8：`g_hInst`、`g_stats`、`g_last_geometry`/`g_anchor_from_caret`、`GlyphProbe`、
`WindowRegistry` 五处全局可变状态，与"两个 IME DLL 可同进程共存"的设计宣称冲突。
X3 就是那句宣称的验收场——**同进程加载两个 scheme，看窗口类名、统计、锚点是否互踩**。

判据：同进程双 scheme 冒烟（各打 3 个键）互不干扰；`g_*` 要么并入会话上下文，要么显式加锁 + 注释说明为何全局。

这取代原方案的"阶段 3（运行期插件 / C ABI 工厂）"。**C ABI 只在需要"第三方二进制、单独发版"
时才需要**，那时才冻结——现在做等于给一个还没出现的需求写 ABI。

---

### X4 前端抽象：只立门，不排期

什么时候才启动（**任一**触发，写死）：

| 触发条件 | 为什么它能触发 |
|---|---|
| 出现第二个前端消费者（非 Win32 / 非进程内 / 独立进程） | 有第二个实现才有比较，才可能冻结接口 |
| `imm_entry.cpp` > 1,200 行 | 单文件已到"改一处要读半天"的规模（X2-d 拆完后按最大 TU 计） |
| csrss 崩溃面成为真实痛感 | 才值得为进程隔离付 IPC 成本 |

触发后**先做**：按当时最新代码重做 MANIFEST 的逐文件处置（原表已失真 +41%~+125%），
并给 `on_key` 补 commit 输出通道（评审 §3.3：IMM32 是 `ImeToAsciiEx` 返回缓冲模型，不是推模型，
缺这条 PR-1 必返工）。

## 4. "预留完全抽象化"的三个机制

不是靠现在写接口，而是靠三件**不花钱**的事，让将来那次抽象不必推倒重来：

| # | 机制 | 现在做什么 | 将来省什么 |
|---|---|---|---|
| M1 | **Intent 中间语言** | X2-c 把路由判定收敛为一个枚举 + 纯函数 | 换前端时，Intent 是两端共用的稳定词汇 |
| M2 | **seam 清单 + 上限断言** | `tools/seams.py` 统计 5 条 seam 的调用点数（含 `UiCanRender`），`check_m1` 加一项：任一 seam 超上限即报错 | 耦合只能减少不能增加；某天从 6 涨到 20 就是抽象的必要性证据 |
| M3 | **接口状态标记** | 头文件给每个契约标 `// status: freeze \| candidate \| draft`（`Engine`/`Scheme` = candidate；`Intent` = draft） | "冻结"变成显式、可检索的事；`check_m1` 可断言"draft 接口不进 DLL 导出" |

三条都不改运行时代码，M1/M2 各半天。

## 5. 排程（插在 ROADMAP 的哪里）

| 阶段 | 放在 | 工期 | 对 v0.1 的影响 |
|---|---|---|---|
| X1 目录归位 | **M2 收口后立刻**（现在） | 0.5–1 天 | 0 |
| X2 安全网 + 构建收敛 | **M3 开工前**（硬约束） | 6–8 天 | +约 1.5 周 |
| X3 可插拔验收 + 实例隔离 | 与 M4 并行（或推迟到 M7） | 2–3 天 | 0（不占关键路径） |
| X4 前端抽象 | 未排期，等门 | — | 未知 |

**X2 为什么必须抢在 M3 之前**：M3 要加中文标点、全角/半角、Shift 临时英文、
Ctrl+Space 与备用热键——**全部改在 `process_key` 里**。先有冒烟测试，M3 每加一个键就有一条回归；
反过来先做 M3，那段代码会再长一节，之后重构要重新建立"改前行为"的基线。

**总代价 2–2.5 周**（原估 1–2 周；吸收审计的注入缝 + 构建收敛后上调），v0.1 推迟约 1.5 周。
这个交换的理由只有一句：M3 之后宿主层只会更长，改起来只会更贵。

## 6. 风险

| 风险 | 处置 |
|---|---|
| X1 漏改路径导致链接了旧文件 | 字节比对兜底（只允许 4 处差异），不通过即回滚 |
| **X2-e 拆库后字节判据失效** | 换成行为判据：`host_smoke` 序列不变 + 真机 `nihao` 冒烟；**不要**用新字节去比旧字节 |
| X2-a 剥 `handle_key` 时改坏 ABI（`TransMsgList` 语义） | 先只做"搬运 + 包装"，不动任何判定；`uMsgCount` 是容量头，消息从 `TransMsg[0]` 起写 —— 这里写错会整体错位并丢最后一条消息 |
| X2-e 的 `EXISTS` 改硬失败把 CI 挂掉 | in-tree job 也走这套 CMake，改完必须两条路径各跑一次 |
| 冒烟测试把实现细节写死，反而阻碍后续改动 | 断言对象是 **sink 调用序列 + commit 文本**，不碰内部状态；Intent 单测才断言分支 |
| X3 示例沦为"框架里加 if" | 判据写死：`framework/` 零 diff，否则补框架 |
| 版本收敛误伤 | 只删"带四位版本号的字符串"，保留各模块自述的语义版本（如 `0.1.0-imfw` 改为 `imfw/1`） |
| 计划本身被当成"已经冻结的设计" | 本文开头声明 + 状态标记；X4 的门写死 |

## 7. 与三份路线 A 文档的关系

- 保留价值最高的是：**接口清单式思维**（MAPPING 的 16 导出逐个处置）、**PR 切分纪律**、
  **四层回滚方案**。将来 X4 触发时直接复用。
- 不能直接用的是：**阶段顺序**（1→2→3 的依赖链是错的，3 不需要 1/2）、**文件规模快照**（两天失真 +125%）、
  **零改动红线**（已被 M2 的 `commit_text` 打破）。
- 若哪天要重做那份清单：先跑 `tools/snapshot_sizes.py`（新增，输出字节/行数与指纹），
  把数字变成"可重生成的产物"，不要手抄。**优先级低**（复核文档 §5.2 有保留意见），
  更便宜的做法是把行数并进 `ctest`/`check_m1` 的既有输出。

## 8. 待办（按 PR 粒度，勾完即计划完成）

**X1 — 目录归位**

- [ ] X1-a `git mv` + `ime/CMakeLists.txt` 路径 + `check_m1` 15 处 + 文档路径（审计 A-P1-4 的目录部分）
- [ ] X1-b 字节比对报告（4 处允许差异）写入 `docs/handover/HANDOVER.md`

**X2 — 安全网 + 构建收敛**

- [ ] X2-a0 `SchemeRegistry` 注入缝 + toy scheme（审计 A-P0-2；达成 G4）
- [ ] X2-a `handle_key` 剥离 + `KeyEvent`/`KeyResult`（行为不变）
- [ ] X2-b `tests/host_smoke_test.cpp`（假 engine/scheme/sink/can_render，5 段键序）（审计 A-P0-1）
- [ ] X2-c `route_key` + `Intent` 单测；`host_smoke` 序列不变
- [ ] X2-d `imm_entry` 按 6 类职责拆 TU + 合并两条激活路径 + 合并 trace 十进制转换（审计 A-P0-3、复核 N-1）
- [ ] X2-e1 `platform/win32/{imm32,ui}` 库目标 + `ime` 只 link（审计 A-P1-4）
- [ ] X2-e2 `dat_source_win.cpp` 归 `rospinyin-engine` + 链接期符号断言（审计 A-P1-5）
- [ ] X2-e3 `CMakePresets.json` + `EXISTS` 硬失败 + 统一根变量 + C 标准 + 静态策略收敛（审计 A-P1-6）
- [ ] X2-e4 28 处 `#ifndef` 平台补丁归位（审计 A-P2-11）
- [ ] X2-e5 版本收敛到 `ime/version.h` 一处 + CI 断言（审计 A-P1-7 版本部分；达成 G7）
- [ ] X2-e6 `dat_source_win.cpp` 不再引宿主私有头（复核 N / X1 表）
- [ ] X2-f `tools/seams.py` + `check_m1` 新增 seam 上限项（含 `UiCanRender`）

**X3 — 可插拔验收**

- [ ] X3-a `samples/toy-scheme/` + `samples/README.md` + 从零接入演练
- [ ] X3-b per-instance 隔离：`g_hInst` / `g_stats` / `g_last_geometry` / `GlyphProbe` / `WindowRegistry`（审计 A-P1-8）

**X4 — 等门**

- [ ] M3（机制）头文件 `status:` 标记 + 断言"draft 接口不进 DLL 导出"
