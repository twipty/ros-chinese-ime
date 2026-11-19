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
| G5 | 未来换前端时改动被限制在 seam 内 | seam 清单里的调用点数量有上限断言（`check_m1` 新增一项；X2-f 达成，5 条 seam 当前均等于上限） |
| G6 | 构建有单一入口 | `cmake --preset linux-ci` / `--preset win32-mingw` 两条路径，零手工 `-D` 即复现 CI 与发版 |
| G7 | 版本只有一个真值 | 仓库内发版版本号只出现在 `ime/version.h`；其余位置出现即 CI 失败（X2-e5 达成，白名单见 §3 执行记录） |

**明确不做**

- 不冻结 `Frontend` / `Plugin` ABI（现在只有一个实现，冻结等于猜——这正是三份文档自己点名的 R-1 风险）；
- 不做"把引擎挪出 csrss"（阶段 2）：与"让第三方代码进 csrss"的插件目标价值方向相反，先不碰；
- 不重写 UI 层为平台无关（Win32 窗口代码没有第二个平台消费者，重写是纯成本）；
- **不做 `.dat` 的 mmap/异步/预加载**（审计 A-P2-10）：属性能，归 M4/M5，与结构治理目标不同；
- **不重做 `kBuiltin` 的生成管线**（审计 A-P1-7 的数据部分）：M4 扩码表必然重做，现在做是重复劳动。

## 2. 现状基线（2026-11-19 12:4x 实测，X2-c 之后，`main` 上）

> **这批数字会失真**：上一版快照写 `ime/*.cpp = 57`，实测 19。而且**跨分支的数字不可比**——
> 12:2x 那版（可移植层 1642）测的是 `wip/status-indicator` 的树，比 `main` 多一份工具栏状态改动。
> 所以每项都附生成命令：**用命令，别引用本文的数字**。

```bash
wc -l framework/src/*.cpp framework/include/imfw/*.hpp | tail -1   # 1728  可移植层（X2-c：intent.hpp + key_route.cpp 进来，路由从 host_key 搬走）
wc -l framework/platform/win32/imm32/* framework/platform/win32/ui/* | tail -1  # 2689  平台层（Win32）→ 0 测试（X2-d 后 +178，见下）
wc -l ime/*.cpp                                       | tail -1   # 46    装配（含玩具装配件，不进产品构建）
wc -l tests/*.cpp samples/toy-scheme/*.cpp | tail -1   # 2737  测试（全 host-side）
python3 tools/check_m1.py | tail -1                               # 77 项 契约断言（静态）
(cd build && ctest -N | tail -1)                                  # 16 项 全在可移植层
```

| 文件 | 行数 | 性质 |
|---|---|---|
| `framework/platform/win32/imm32/imm_entry.cpp` | **314** | 只剩 16 个导出 + 唯一的激活序列 `activate_session()`（783 → 613（X2-a/c）→ 314（X2-d）） |
| `framework/platform/win32/imm32/key_translate.cpp` | 103 | vk → `KeyEvent`、`KeyResult` → `TRANSMSGLIST`（X2-d 拆出） |
| `framework/platform/win32/imm32/win32_host.cpp` | 92 | `Win32Host`：`Host` 的 Win32 实现（X2-d 拆出） |
| `framework/platform/win32/imm32/trace_line.cpp` | 117 | `ROSPINYIN_TRACE` 通道（X2-d 拆出） |
| `framework/platform/win32/ui/ime_ui.cpp` | 300 | 窗口编排、锚点、`GlyphProbe` 字形探针 |
| `framework/src/key_route.cpp` + `include/imfw/intent.hpp` | 70 + 75 | X2-c 的路由层：`route_key()` 纯函数 + `Intent` 词汇（`status: draft`） |
| `framework/platform/win32/ui/ui_window.cpp` | — | 几何单一 owner；开头 28 处 `#ifndef` 重定义 Win32 常量 |
| `framework/include/imfw/{engine,scheme}.hpp` | 72 / 136 | 可移植契约，**Win32 类型 0 处** |

**接缝现状（这是本计划的地基）**：`imm_entry.cpp` 对外部世界的依赖只有五项，数量都很小，
所以抽象成本可控、但**现在不值得为它冻结接口**：

| seam | 现状调用点 | 抽象后要动的半径 | 备注 |
|---|---|---|---|
| 引擎（`state->engine->*`） | `host_key.cpp` 内 | 一个 TU | 引擎由 `scheme_instance().create_engine()` 产出 |
| 方案策略（`scheme.*`） | `key_route.cpp`（判定）+ `host_key.cpp`（执行） | 两个 TU，各一次 | X2-c 后"一个键是什么"只问一次 |
| UI 出口（`UiShow/UiHide/UiCanRender/UiCounters*`） | `win32_host.cpp` | **一个 TU**（X2-d 前散在 600 行里） | 全部经 `imfw::Host`，策略层零 Win32 |
| IMM32 ABI（`imm32_abi.h` + 16 导出） | 1 个文件 | 只在 `imm_entry.cpp` | X2-d 后这个文件只剩导出与激活 |
| **字形能力（`UiCanRender`）** | 1 处，`Win32Host::can_render` | `renderable_candidates`（`host_key.cpp`）只经 `Host` 问 | **审计 A-P2-9 新增**：不替换它，CI 上的冒烟测试恒为"全部可渲染"，测不出字体相关回归 |

**结构缺口（审计复核后新增，原计划没覆盖）**

| 缺口 | 证据 | 为什么它属于本计划 |
|---|---|---|
| 键路由不可测（**已修，X2-a**） | `process_key` / `should_consume_key` 曾整个埋在一个读 `lpbKeyState`、调 `ImmGetOpenStatus`、写 `TRANSMSGLIST` 的 TU 里，**宿主侧 0 覆盖**；现在判定在 `framework/src/host_key.cpp`，`tests/host_key_test.cpp` 用假 engine + 假 host 跑 | M2 报告里"字母翻倍 / 候选窗冻结 / 多字丢字"全是这几个 `if`；没有宿主测试，改 X2-c 的路由就是盲改 |
| 无注入点（**已修，X2-a0**） | `scheme_instance()` 曾是链接期硬接线（`scheme.hpp:129` → `scheme_reg.cpp:14`）；第二方案靠**换一个 `scheme_reg.cpp` 再编译**（`ime/CMakeLists.txt:7` 注释自述）。现在装配件只提供 `scheme_factory()`，框架经 `SchemeRegistry::get()` 查询 | G3 要假 scheme、G4 要第二个方案，两者都过不去 |
| 库边界不一致 | `framework/CMakeLists.txt` 只构建 `imfw-engine`（5 cpp）；`framework/host`+`ui` 2697 行**无库目标**，由 `ime/CMakeLists.txt:26-35` 用 9 条相对路径直接编进 `.ime` | G1 说"目录自解释"，但没有库边界时目录只是名字 |
| 构建碎片 | `EXISTS` 守卫 4 处、`CMAKE_SOURCE_DIR`/`PROJECT_SOURCE_DIR` 混用、C 标准未设、无 `CMakePresets.json`、静态链接策略 3 份、`build-win` 与 `build-win-trace-en` 测试开关漂移 | G6；且 X 阶段的验收要靠可复现构建才有意义 |
| 版本四处 | `CMakeLists.txt:3`=`0.0.1`、`pinyin/version.hpp:9`=`0.0.2-m2-dev`、`imfw/version.hpp:14`=`0.1.0-imfw`、`ime/version.h`=`alpha0.0.2` | G7；`0.0.1` 这个最危险（`project(VERSION)`，一旦被用于产物命名就带错版本） |

## 3. 四个阶段

### X1 目录归位（0.5–1 天，纯 `git mv`）— ✅ 2026-11-18 完成

```
framework/host/{dllmain,imm_entry,ui_server,session_store,imm32_abi,trace_stamp}
        →  framework/platform/win32/imm32/        （已执行，9 个文件）
framework/ui/{ime_ui,ui_window,composition_window,candidate_window,toolbar}
        →  framework/platform/win32/ui/           （已执行，10 个文件，含 README）
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
| `pinyin/host/dat_source_win.cpp` | 1 条 include | 跨层引 `trace_stamp.h`。X2-e2 把文件收进 `rospinyin-engine` 后，这条路径由 `pinyin/CMakeLists.txt` 的 Windows 分支声明，X2-e6 收掉 |
| `tools/check_m1.py` | 15 处硬编码路径 | 含 `framework/ui/README.md` |
| 文档 | 若干 | 只改路径，不改论述（`tools/check_docs.py` 会盯着链接） |

**验收（硬）**：功能等价不看测试，看**字节**——用同一工具链重建，`.ime` 与 X1 前逐字节比对，
只允许 3 个区域差异：
- `0x88` PE TimeDateStamp（链接器时钟）
- `0xD8` CheckSum（由全镜像派生）
- `0x60A04` **导出表** `IMAGE_EXPORT_DIRECTORY.TimeDateStamp`（也是时钟的副本）

> 原稿写的是"4 处差异，调试字段 `0x60A04`"，**执行时发现是错的**：该镜像的数据目录 #6
> （Debug）全为 0，根本没有调试目录；`0x60A00` 是 `.edata` 节起点，`0x60A04` 是导出目录里的
> 时间戳字段。这不影响结论（判据仍然是"只允许链接器戳不同"），但工具必须按 PE 结构定位这些字段，
> 不能硬编码偏移——`tools/compare_ime.py` 就是这么写的。

这比任何单测都硬：路径改名若漏了某个源文件，链接产物一定变。

**回滚**：`git revert` 单个提交即可，无逻辑变更。

**✅ 执行记录（2026-11-18 22:0x）**

| 项 | 计划预估 | 实际 |
|---|---|---|
| 改名文件 | `host/` 9 个 + `ui/` 10 个 | 一致（19 个，`git mv` 全为 rename） |
| `ime/CMakeLists.txt` | 9 源 + 3 include | 一致 |
| `tools/check_m1.py` | 15 处 | **16 处**（多一条 `dat_loader.hpp/.cpp` 的存在性断言，它也是带目录的路径） |
| 未预料到的第 6 处 | — | `.github/workflows/ci.yml` 的 `-fsyntax-only` 步骤直接列了 9 条路径 |
| 字节比对 | 4 处差异 | **3 个区域**（见上），940,888 B 完全一致，`VERDICT: equivalent` |

**顺带修掉的门禁失效**：执行时发现 `IME source syntax check` 这条 CI 步骤**一直是红的**
（`HIMC` 未声明，HEAD 上也一样），它挡住了 engine-smoke 后面所有步骤，连带 `rospinyin-m2-data`
artifact 从未上传，使 `publish-artifacts-branch` 永久失败。补全 `tools/syntax_stub/{imm,windows}.h`
后恢复，并做了 3 次变异验证。详见 `docs/handover/HANDOVER.md` §11.23。

---

### X2 安全网 + 构建收敛（6–8 天，**在 M3 开工前**）

原计划 X2 只有"剥 `handle_key` + 冒烟 + 路由"。审计复核后扩成五步，
**顺序是硬的**——理由在下面每一步里。

**X2-a0 注入缝 + toy scheme（1–1.5 天）— ✅ 2026-11-18 完成**

`Scheme& scheme_instance()` 换成可注册的上下文：`imfw::SchemeRegistry`（进程内一个实例，
但**创建与查询分离**，允许在测试里 `install(fake)`）。toy scheme 同时落地，
作为注入缝的第一个消费者（这本来排在 X3）。

> **为什么必须排第一**：X2-b 要"假 scheme"，但现在的代码里塞不进去
> （`session_store.cpp:34` 直接调全局工厂）。这条不先做，后面每一步都要改 framework 才能测，
> 而 X3 的判据（`framework/` 零 diff）会在最后一步把之前的偷懒全部打回去。

判据：`-DROSPINYIN_SCHEME=toy` 能构建出可跑的 `.ime`；且该提交 `git diff --stat` **不出现 `framework/`**
（G4 判据提前达成，X3 只剩打包与文档）。

**✅ 执行记录（2026-11-18 22:5x）**

| 项 | 计划 | 实际 |
|---|---|---|
| 创建/查询分离 | `SchemeRegistry` + `install(fake)` | 一致：装配件给 `scheme_factory()`，框架给 `get()` |
| 调用点改动 | — | **0 处**：`scheme_instance()` 保留为 `get()` 的别名 |
| toy scheme 位置 | `tests/fixtures/`（允许） | `samples/toy-scheme/`——它要证明的是"第二个方案只加文件夹"，放 tests 会读成"只给测试用" |
| `framework/` 零 diff | 该提交 | **拆成两个提交**：加缝的提交必然改 `framework/`，接入 toy 的提交是 0（详见下） |
| 门禁 | — | `ctest` 13/13、`check_m1` 65/65、`check_docs` 7/7 |
| 玩具 `.ime` | 能构建 | 870,475 B；16 个 IMM32 导出、导入表与拼音版一致 |
| 真机冒烟 | "可跑" | **未做**（不能代用户敲键）。待用户有手时：装玩具版 → 敲 `abc` → 空格上屏 `abc`、`abc`+`2` 上屏 `abc!` |

**判据拆开写，因为它和现实互斥。** "该提交 `framework/` 零 diff"与"引入注入缝"不可能同时成立：
缝不在共享核心里，第二个方案就只能复制一份注册表。审计的原话是"**接入 toy scheme 时**
`framework/` 零 diff"——按这个读，判据成立，而且正是要问的问题（消费者能不能不动框架）。
于是：

| 提交 | `framework/` |
|---|---|
| `feat(framework): SchemeRegistry 注入缝` | 有（2 新文件 + 3 处注释/CMake）——这就是缝 |
| `feat(samples): toy scheme 作首个消费者` | **0** |

**执行中发现的两件事**：

1. `check_m1` 原来断言 `"scheme_instance" in ime/scheme_reg.cpp`——改完后这个词仍在文件里，
   在我写的注释里。门禁靠词匹配就会被注释骗，断言已改为查定义形式 `Scheme& scheme_instance`。
2. 注册表**不能**用静态初始化器绑定（那会让工作在 loader lock 里发生），
   所以走"第一次查询时问工厂"，DllMain 的行为与改动前逐字一致。

**X2-a `handle_key` 剥离（0.5 天）— ✅ 2026-11-18 完成**

把 `process_key` 从 IMM32 ABI 里剥出来：`handle_key(Session&, KeyEvent) -> KeyResult`，
入口函数只做 ABI 转换（`lpbKeyState` → `KeyEvent`，`KeyResult` → `TransMsgList`）。
这一步不改变行为，但让宿主第一次能脱离 imm32 头文件被测试。

**✅ 执行记录（2026-11-18 23:0x）**

| 项 | 计划 | 实际 |
|---|---|---|
| 策略位置 | 从 `imm_entry.cpp` 剥出 | `framework/src/host_key.cpp`（254 行）进 `imfw-engine` |
| 接口 | `KeyEvent` / `KeyResult` | 另加 `imfw::Host`（5 个方法 + 2 个诊断钩子），`KeyKind` 让策略不写任何 VK 常量 |
| 入口 | 只做 ABI 转换 | `ImeProcessKey`/`ImeToAsciiEx` 各剩构造事件 + 调策略 + 回填 |
| 调用点 | — | `process_key`→`handle_key`、`should_consume_key`→`wants_key` 各 1 处，无其他调用方 |
| 行为 | 不变 | 分支逐条搬，**没有任何判定被改动**（见下"没改什么"） |
| 判据 | — | 字节判据不适用（代码换 TU + 虚调用），改为宿主测试 + PE 面不变 |
| 门禁 | — | `ctest` **14/14**（新增 `host-key`，35 条断言）、`check_m1` **67/67**（+2）、`check_docs` 7/7 |
| 拼音版 `.ime` | — | 946,299 B（+4,207 B）；16 导出、导入函数表**逐个一致** |
| 玩具版 `.ime` | — | 874,682 B（+4,207 B，同一份框架） |
| trace 构建 | — | `-DROSPINYIN_TRACE=ON` 交叉构建通过（971,995 B）——诊断钩子真的编进 PE 了 |

**为什么这条能把"最忙的一百行"从零覆盖拉起来。** 剥之前 `process_key` 读 256 字节
`lpbKeyState`、调 `ImmGetOpenStatus`、开我们自己的窗口、往 `TRANSMSGLIST` 里写——四件平台事
包着一个判定，于是**判定本身只能靠往 ReactOS 里敲键来测**。M2 报告里的每一个 bug（字母翻倍、
候选窗冻结、"多字/丢字"）都是这 90 行里某个 `if` 的事。现在平台事全在适配器，判定在
`imfw-engine`，`tests/host_key_test.cpp` 用假 engine + 假 host 直接跑。

**没改什么（这是"搬运"的判据）。** 逐条对过：`wants_key` 的 `if` 序列与
`should_consume_key` 完全同序（唯一形式变化是 `ch == 0 → vk == VK_BACK || VK_ESCAPE`
拆成 `kind == Backspace || Escape` 先判，因为适配器已经把这两键映射成 kind，而它们在
`framework_key_char` 里本来就产出 0）；`handle_key` 与 `process_key` 的五个分支
（select / page / backspace / escape / feed+switch）与副作用顺序一致。`ImeProcessKey`
里 Shift 分支、key-up 分支、latch 清除三处的先后顺序也未动。

**三个必须留在原处的东西**（想搬但搬不了，原因值得记）：

1. **lone-Shift 的 latch 只能在"归属"路径里。** `ImeProcessKey` 返回 `FALSE` 时
   IMM32 根本不会调 `ImeToAsciiEx`，所以把切换挪进 `handle_key` 会让 Shift 永远不生效。
   这就是 `wants_key` 收 `ImeSessionState&` 而不是 `const&` 的唯一原因，头文件里写明了。
2. **key-up 位只有 `ImeProcessKey` 看得见。** `ImeToAsciiEx` 的 ABI 里没有 `lParam`，
   所以 `key_event_from(vk, 0, ...)` 在那一侧恒为 `key_up = false`——这正是"归属侧必须拦"
   的原因，不是巧合。
3. **trace 不能留在策略里。** `feed` 那行要 `UiCountersSnapshot()`（窗口移动/重绘计数），
   属于宿主；`cand-filter` 那行是策略产生的诊断。做法是 `Host` 上两个默认空实现的钩子
   （`on_feed` / `on_candidates_filtered`），Win32 宿主转成 `IMFW_TRACE`，宿主测试什么都不做
   ——**默认空实现让假宿主不必知道 trace 存在**。

**变异验证（6 次策略 + 2 次门禁）。** 策略侧：不拦 key-up / 关态仍吞键 / Shift 闩不清 /
翻页无效也刷 UI / 丢弃过半不兜底 / 退格后永远 composing——全部变红，且报的是对应那条断言。
门禁侧：`host.hpp` 加一行 `<windows.h>`（X2-a 可移植性断言红）、`ImeProcessKey` 不再调
`wants_key`（红）。

**又一次假绿，同一个老坑。** 第一版门禁写的是 `"imfw::wants_key(" in process_body`。
把普通分支那处调用删掉，**检查仍然 66/66 全绿**——因为 Shift 分支里还有一处同样的调用。
这和 §11.24 的 `"scheme_instance" in 文件` 是同一类错误：**断言问"某个词出现过吗"，
就永远测不出"某处调用没了"**。改成钉住两处的完整调用形式，再变异，两次都红。

**X2-b 宿主冒烟测试（1.5–2 天）**

`tests/host_smoke_test.cpp`：假 engine（固定返回序列）、假 sink（记录 `UiShow/UiHide/UiRefresh`
调用顺序与参数）、假 scheme（字母表 `a-z` + `1-9` 选字 + `=`/`-` 翻页）、
**假 `UiCanRender`（恒真，或按码点表返回）**。
键序固定为：`zhongguo` → `2`（选字）→ `Backspace`×2 → `Esc` → `Shift`（lone-Shift 切换）。
断言：sink 调用序列逐项相等 + `commit` 文本序列。**约 120 行，是全天最值钱的一百行。**

**✅ 执行记录（2026-11-19 11:0x）**

| 项 | 计划 | 实际 |
|---|---|---|
| 文件 | `tests/host_smoke_test.cpp` | 同名，463 行（注释占一半）；断言的对象是**调用日志**，不是单个判定 |
| 假件 | engine / sink / scheme / can_render | `ScriptedEngine`（每个操作从队列取一条固定 `Result`，队列空=失败）、`RecordingHost`（一次可观测调用一行）、`SmokeScheme`（字母表 + 默认选字/翻页键） |
| 键序 | `zhongguo` → `2` → `Backspace`×2 → `Esc` → `Shift` | 一致；驱动层复刻 IMM32 的门——`wants_key` 为真才调 `handle_key` |
| 断言 1 | sink 序列逐项相等 | 13 条日志逐字比对；失败时打印**第一处差异**（期望/实际两侧原文），而不是"序列不等" |
| 断言 2 | commit 文本序列 | `["钟"]`；并额外钉住引擎收到的操作序列（`feed`×8、`commit:钟`、`backspace`×2、`cancel`×1） |
| 额外 1 | — | 队列空即标记 `underrun`：宿主多喂一次/多退一次就红 |
| 额外 2 | — | 关态或无组合时，键既不进引擎也不产生 host 调用（关掉 English 失效这类回归只能在这里现形） |
| 额外 3 | — | P1-2 两条护栏各一例：丢 1/2 则保留其余；丢 2/3 则整表保留（字体问题不是候选问题） |
| 门禁 | — | `ctest` **15/15**、`check_m1` **68/68**（+1）、`check_docs` 7/7 |
| 变异 | — | 4 次全红：退格后一律 `hide` / 提交后不 `refresh_ui` / 关态不写 `fOpen` / 关态仍吞键 |

**为什么断言"日志"而不是"最终文本"。** X2-c 与 X2-d 的验收都是"调用序列不变"。只断言最终
文本的话，重排 `show`/`hide`、少刷一次 UI、多调一次引擎全都查不出来——而这三种恰恰就是
"候选窗冻结""字母翻倍"那类 bug 的形状。日志还顺带把 `backspace` 到空组合时"仍 show 但候选为 0"
这个状态钉住了：**那正是候选窗闪烁的机制**，见「待排期」。

**状态指示器：Shift 切的是 IME 状态，不是键盘布局（0.5 天，插单）— 已合入 X2-w1**

用户 2026-11-19 定的语义，取代原先"托盘应该跟着变"的错误假设：

- 托盘那个红底「中」是 ReactOS 通知区的**布局**指示器（跟 HKL 走）。Shift 只改 `fOpen`，
  不动 HKL，所以**它必须不变**——之前把它记成"体验缺口"是我判错了对象。
- 状态只能由我们自己的工具栏承载：`拼` → `拼:开` / `拼:关`。
- 状态来源是 `Scheme`（语言是方案的事），不是框架硬编码；`Host` **不加钩子**——`set_open`
  已经是那个钩子的位置，而 `fOpen` 还会被 imm32 的 Ctrl+Space 绕过，所以同步走"键路径拉取"
  而不是"变更时推送"。

**✅ 执行记录（2026-11-19 11:2x）**

| 项 | 计划 | 实际 |
|---|---|---|
| 契约 | `Scheme::toolbar_state_label(bool)` | 默认返回 `nullptr`（无状态的方案保持裸标签），四个假方案零改动；拼音给 `开/关`，玩具给 `on/off` |
| 拼装 | 留在 Win32 层 | 提到 `framework/src/toolbar_text.cpp`（25 行，可移植）→ 新增 `toolbar-text` 测试钉住 `拼:开`/`拼:关` |
| 刷新 | 给 `Host` 加钩子 | **不加**：`ImeProcessKey` 里每个键拉一次（bool 比较，未变则零 Win32 调用），Shift 手势后立刻再拉一次 |
| 宽度 | 固定 44 px | 实测宽度：`拼:开` 在 44 px 里会被裁，改成 `GetTextExtentPoint32W` 量出来 + 上下限；右边缘固定、向左生长 |
| 幂等 | — | `Toolbar::update` 文本相同且可见时直接返回，避免每键一次 `InvalidateRect`（P0-3 刚拆掉的那场重绘风暴） |
| 门禁 | — | `ctest` **16/16**、`check_m1` **71/71**（+3） |
| 变异 | — | 5 次全红：状态恒为开 / 丢冒号 / 无状态也拼冒号 / 退回静态标签 / 删掉 Shift 后那次同步 / 假装切布局（`ActivateKeyboardLayout`） |
| 对外面 | 不变 | 16 导出名顺序逐个一致、5 个 DLL 依赖一致、**每个 DLL 的导入函数集逐个一致**（没有引入任何新 API）；i686 `949,940 B`（+3,641） |

> **入口状态（2026-11-19 12:2x）**：以上改动**不在 `main` 上**，在分支 `wip/status-indicator`
> 的一个提交里；上表数字是在那个分支上测的。按用户"和候选窗补丁一起排期、先把 X2 做完"的决定，
> 排入 **X2-w1**（见下）。当时 `main` 的门禁是 X2-b 的数字（`ctest` 15/15、`check_m1` 68/68）；
> 合入后当前数字见 §3「X2-w 合入记录」。
>
> **合并时会撞（X2-d 已落地，撞点已确认）**：该分支改的是 `ImeProcessKey` 里两处
> `UiToolbarSetOpen`，而 X2-d 已经把该函数从 `imm_entry.cpp` 里清出去——**新的落点是
> `imm_entry.cpp` 的 `ImeProcessKey`（这个函数仍在原文件，只是不含任何判定逻辑）**，
> 而它调的 `Win32Host::set_open`/`is_open` 在 `win32_host.cpp`。也就是说 w1 的 hunk 会**跨两个文件**：
> 调用点在 `imm_entry.cpp`，状态写入在 `win32_host.cpp`。合入时按这两处重落，不要照抄旧 hunk。

**✅ X2-c `route_key` + `Intent`（2026-11-19 12:4x）**

计划：只搬判定，`handle_key` 保留副作用（改 pager、刷 UI、调引擎）。
验收：`host_smoke` 调用序列逐字节一致；`route_key` 单测覆盖每个 Intent。

| 项 | 计划 | 实际 |
|---|---|---|
| 放置 | 未定 | 新 TU `framework/src/key_route.cpp`（70 行）+ `include/imfw/intent.hpp`（75 行），进 `imfw-engine` → **可移植、可单测** |
| 词汇 | `SelectDigit(n)` / `ToggleLang` | **`Select{slot}`**：空格不选数字却选第 1 个，叫 `SelectDigit` 是让名字撒谎；`slot` 仍是页内相对索引（第 2 页按 1 选第 10 个）<br>**删掉 `ToggleLang`**：Shift 那条路是**归属**判定（key-up 必须答"不是我的"），不是 dispatch。一个只有"承诺永不 dispatch 的那一层"才能产出的意图，是留给下一个读者的陷阱 |
| 签名 | — | `Intent route_key(const Scheme&, const KeyEvent&)`，**纯函数**：不碰 session / engine / host，也不看 `key_up`/`ctrl`/`alt`/`fOpen`（那是归属问题，问两遍就会有两种答案） |
| `handle_key` | 保留副作用 | 变成 `switch (intent.kind)`，并且**不再碰 `scheme`**（`check_m1` 钉住：`scheme_instance()` 只许出现 1 次，就是传给 `route_key` 的那次） |
| 顺序 | — | 选择 → 翻页 → 文本，与搬之前逐条相同；`OverlapScheme`（同一键两处都声明）专门钉"选择仍然赢" |
| 门禁 | — | `ctest` **16/16**（+`key-route`，22 条断言）、`check_m1` **71/71**（+3，且 G4 那条从 `host_key.cpp` 改指 `key_route.cpp`）、`check_docs` 7/7 |
| 行为 | 序列逐字节一致 | `host_smoke` 13 条日志一行未改（`route_key` 的所有判定与原 `if` 链同序同操作数） |
| 变异 | — | 6 次全红：slot 起点 0→1 / 翻页提到选择之前 / 去掉 `is_input_char` 守卫 / enum 加一个 kind 不给分支 / `route_key` 换签名 / `handle_key` 重新问 scheme（两种写法各一次） |
| 对外面 | — | 16 导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；i686 `946,299 → 947,148 B`（+849），玩具版 `874,682 → 875,019 B`（+337） |

**一次自己抓到的假绿。** 第二条新断言最初写的是"`handle_key` 里不许出现 `scheme.`"。做变异时我加的
违规代码是 `scheme_instance().page_key(...)` —— 它**不含** `scheme.`（中间隔着下划线），71/71 全绿。
这与 X2-a0 两次是同一个形状：**断言"某个词出现过/没出现过"就测不出"某处调用还在"**。改成
"不许有任何策略调用（`page_key(` / `is_selection_key(` / `input_char(`）+ `scheme_instance()` 只许一次"，
再用两种写法各变异一次，都红了。

**✅ X2-d 拆 god object（2026-11-19 13:1x）**

计划：`imm_entry.cpp` 按 6 类职责拆 TU（trace / 键翻译与 `emit_text` / UI 刷新 / 候选过滤 /
键路由 / 16 个导出）；合并两条激活路径；合并 trace 的两份十进制转换。
验收：`host_smoke` 序列不变 + 真机 `nihao` 冒烟一次；**不看字节**。

| 项 | 计划 | 实际 |
|---|---|---|
| TU 数 | 6 | **4**：`key_translate` / `win32_host` / `trace_line` + `imm_entry`。计划里的"候选过滤"与"键路由"两行在 X2-a、X2-c 已经走了（`renderable_candidates` 进 `host_key.cpp`、`route_key` 进 `key_route.cpp`）——**计划里的行号是 783 行那版的，按行号找职责已经过期** |
| `imm_entry.cpp` | 783 → ? | 613 → **314** 行（只剩 16 导出 + `activate_session`）。平台层总数 2511 → **2689**（+178：四个文件各自的许可证头 + namespace 壳）；**这才是拆分的真实成本，不藏** |
| 激活路径 | 合并为 `activate_session(hIMC)` | 合并，顺序固定为 `AlignOpenStatus` → `on_select(true)` → `UiToolbarShow`（原来两个入口点**顺序相反**，U1 就是这么来的）<br>**解除激活不合并**：`ImeSelect(FALSE)` 要 `UiDestroy`+`release_session`，`ImeSetActiveContext(FALSE)` 只 `UiHide`，合并就是改行为 |
| trace 十进制 | 合并两份（pid / tid） | 都走 `trace_stamp.h` 的 `put_uint()`；给它补了 `cap` 参数，把"截断而不是越界"变成参数化保证 |
| 判据改造 | 未提 | `check_m1` 里一多半断言原文是 `... in entry`（`entry` 就是 imm_entry.cpp）。拆完不是"变红因为行为变了"，而是"变红因为文件里没有了"——**换成 4 TU 拼接会让"搬走了"和"多了一份拷贝"再也分不出来**，所以按归属改：某 TU 必须有、另一个必须没有 |
| 新增断言 | — | 3 条（净 +6 项）：入口文件 ≤400 行且不含 `class Win32Host`/`trace::Line`/`framework_key_char`/`offsetof(`；激活序列只在 `activate_session` 里；trace 只有一个十进制格式化器（`put_uint(` 在、`% 10` 不在） |
| 门禁 | — | `ctest` 16/16（可移植层一行未动，`host_smoke` 输出逐字一致）、`check_m1` **77/77**、`check_docs` 7/7 |
| 变异 | — | 6 次全红：入口点抄回 `AlignOpenStatus` / `ImeSetActiveContext` 绕过 `activate_session` / vk 表回落 / 手写十进制回归 / `set_open` 改名 / 文件长到 405 行 |
| 对外面 | 不看字节 | 16 导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；i686 `947,148 → 947,969 B`（+821，只记录不判定） |
| 真机验收 | `nihao` 冒烟一次 | **通过**（2026-11-19 14:3x，用户手动操作，结论「X2D 确认全部正常」）：`dir` = **947,969 B**；组合串 5 字母、候选 9 条（首选「你」）、页码 `1/4`、无残窗；选字 / 翻页 / Backspace / Esc / Shift 逐项正常。证据 `m0/vm/evidence/x2d/`（整屏 + 候选窗 2×）。抓图用 QEMU monitor `screendump`，不代用户敲键 |

**一处 trace 文本变了**：`ImeSelect` 里 `select state=… engine=…` 改成 `activate state=…`，
因为这条现在由共享路径打印；前面的 `select imc=…` / `active f=…` 行仍在，日志仍能分清入口点。

**✅ X2-e1 平台层成为两个库（2026-11-19 15:0x）**

计划：`platform/win32/{imm32,ui}` 两个 `add_library`，`ime/` 只 `target_link_libraries`。
验收：导出面与导入面逐个一致（**不看字节**）。

| 项 | 计划 | 实际 |
|---|---|---|
| 库划分 | imm32 / ui 两个 | `imfw-win32-imm32`（7 TU）/ `imfw-win32-ui`（5 TU），定义在 `framework/platform/win32/CMakeLists.txt`；`ime/CMakeLists.txt` 现在只剩 scheme + `.def` + `.rc` |
| 依赖方向 | 未提 | **两个库互为依赖**（宿主调 `Ui*`，窗口层读 `g_hInst`）。CMake 允许 STATIC 成环，两条边都写出来并在注释里说明；**没有**造第三个 "common" 库去把环藏起来 |
| 链接 | — | 宿主库必须 `-Wl,--whole-archive`：ld 只为**未定义引用**拉档案成员，而 `.def` 里的 16 个名字是导出条目不是引用。第一次链接报 16 次 `cannot export …: symbol not defined`，这是实测不是推测 |
| trace 宏 | — | `option(ROSPINYIN_TRACE)` 从 `ime/CMakeLists.txt` 上提到顶层：读它的是平台库，而 `framework/` 在 `ime/` 之前配置，留在原处会**静默变成只影响装配件** |
| 临时妥协 | — | `dat_source_win.cpp` 仍由 `ime/` 编译（它 `#include "trace_stamp.h"`），所以 `ime/` 暂留一条 imm32 的 include 路径，注释写明 X2-e2 带走；该路径不进任何库目标 |
| 新增断言 | — | 5 条（83/83）：每个 TU 只属一个库（双向 + 先剥 CMake 注释）/ `ime/` 不许出现 `../framework/platform/win32/` / whole-archive 紧贴宿主库 / trace 宏在根目录声明且 `ime/` 没有 / `.ime` 可复现链接 |
| 变异 | — | 4 次全红：TU 挪进另一个库 / 同一 TU 进两个库 / 删 whole-archive / trace 选项挪回 `ime/` |
| 可复现链接 | 未列 | 验收时发现两个构建目录的产物**同大小不同 sha256**（差 6 字节：`TimeDateStamp` + `Checksum` + 2 个镜像字节）。加 `-Wl,--no-insert-timestamp` 后逐字节相同（`134f7fd4…`）。不修的话哈希只能证明"同一个文件"，证明不了"同一次构建" |
| 门禁 | — | `ctest` 16/16（可移植层一行未动）、`check_m1` **83/83**（+5）、`check_docs` 7/7 |
| 对外面 | 逐个一致 | 16 导出名及顺序一致；5 个 DLL 依赖一致；每 DLL 导入函数集**逐条同序**一致（GDI32 12 / IMM32 2 / KERNEL32 59 / msvcrt 43 / USER32 22）。i686 拼音 `947,969 → 948,481 B`（+512 = 一个文件对齐块）、trace 版 `971,982 B` 不变、玩具 `875,019 → 876,352 B` |

**顺带修的文档漂移**：HANDOVER §4 目录树里 `framework/host/`、`framework/ui/` 是 X1 之前的布局，
X1 改了代码与 `check_m1` 的 15 处路径却漏了这棵树。

**✅ X2-e2 数据源归位 + 链接期符号断言（2026-11-19 15:1x）**

计划：`dat_source_win.cpp` 纳入 `rospinyin-engine` 的 Windows 配置；可执行文件加"链接期符号存在断言"
（`load_system_table` 非 weak 版本）。验收：同一次删除必须**链接失败**。

| 项 | 计划 | 实际 |
|---|---|---|
| 归属 | 进 `rospinyin-engine` | `pinyin/CMakeLists.txt` 的 `if(WIN32)` 里 `target_sources(rospinyin-engine PRIVATE host/dat_source_win.cpp)`；`ime/CMakeLists.txt` 一行平台/插件源码都不再列 |
| 链接期断言 | 符号存在 | 不用额外断言，改成**结构上不可能**：Windows 下 `ROSPINYIN_REQUIRE_SYSTEM_TABLE` 把 weak 兜底编译掉，缺实现就是 `undefined reference to rospinyin::load_system_table()` |
| 改动前实测 | — | 把该文件从 `ime/` 清单删掉，`cmake --build` **退出码 0**，产物 **942,989 B**（正常 948,481 B）——装上去只是候选变少、多音字消失，无任何提示。这就是 A-P1-5 说的"插件不自洽" |
| 改动后实测 | — | 同一次删除 → `ld: undefined reference to 'rospinyin::load_system_table()'`，`make` 退出码 2；再把宏也去掉 → 又成功（942,989 B），证明宏就是那个把静默降级变成编译错误的开关 |
| 顺带清理 | — | `ime/CMakeLists.txt` 删掉 X2-e1 的临时 include 路径，以及 `if(ROSPINYIN_TRACE) target_compile_definitions(rospinyin-ime …)`——该目标已无 TU 读这个宏，留着是"看起来生效"的死开关 |
| 新增断言 | — | 4 条（83 → 87）：数据源归属（匹配 `target_sources(...)` **调用**，不匹配路径词）/ Windows 必须有 REQUIRE 宏（跨行匹配）/ 守卫只包住兜底（守卫内必须有 `load_system_table`、不许有 `load_pinyin_table`）/ 装配目标不许出现 `ROSPINYIN_TRACE` |
| 变异 | — | 4 次全红：文件挪回 `ime/` 清单 / REQUIRE 宏改成注释（**词还在、调用没了**）/ 去掉守卫 / trace define 加回装配目标 |
| 遗留 | 未列 | `dat_source_win.cpp` 仍引 `trace_stamp.h` → `rospinyin-engine` 带一条指向 `framework/platform/win32/imm32` 的 include 路径，交 X2-e6 |
| 门禁 | — | `ctest` 16/16、`check_m1` **87/87**（+4）、`check_docs` 7/7 |
| 对外面 | 逐个一致 | 16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序**完全一致**；函数符号 1386 个两边逐个相同。i686 拼音 `948,481 → 947,820 B`（−661，`.text` −64 / `.rdata` +32，其余为节对齐）、trace 版 `971,982 → 971,833 B`、玩具 `876,352 B` 不变 |

**✅ X2-e3 构建配置收敛（2026-11-19 16:4x）**

计划：`CMakePresets.json` + `EXISTS` 硬失败 + 统一根变量 + C 标准 + 静态策略收敛（A-P1-6）。

| 项 | 计划 | 实际 |
|---|---|---|
| 配置定义 | presets | `CMakePresets.json`：6 个 configure（含 `linux-ci` / `win32-mingw` / `win32-trace` / `win32-toy` / `win32-intree` / `win32-cross`）+ 6 个 build + 1 个 test；CI 四段 YAML `-D` 列表改为 preset 名，断言钉住名字 |
| 缺目录 | 硬失败 | 根文件 5 处 `EXISTS` 守卫全删；`add_subdirectory` 缺目录直接 configure 失败。与 X2-e2 的 weak 兜底同型：少一块不能静默成功 |
| 根变量 | 统一 | 子目录一律 `PROJECT_SOURCE_DIR`，`check_m1` 扫全部 `CMakeLists.txt` 禁止 `${CMAKE_SOURCE_DIR}` |
| C 语言等级 | 显式 | `CMAKE_C_STANDARD 11` + `REQUIRED` + `EXTENSIONS OFF`（实测 `-std=c11`），C 工具不再跟随编译器默认 |
| 静态策略 | 收敛 | `cmake/StaticRuntime.cmake` 两个函数；5 个已发布 PE 逐个按调用申请，禁止复述字面 flag。此前 `httpdl` 只有 `-static`、没有可复现头，四个工具哈希都不可信 |
| 新增断言 | — | 7 条（87 → 94）：无 `EXISTS` / C 标准三行 / 根变量 / 策略中枢存在 / 每个 PE 两条调用 / 不许复述 flag / presets + CI 走 preset；7 次变异全红 |
| 门禁 | — | `ctest` 16/16、`check_m1` **94/94**、`check_docs` 7/7 |
| 对外面 | 逐个一致 | 16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序完全一致；toy 876,352 B、trace 971,833 B、in-tree 947,820 B 且不产安装器与 `rhash.exe`。两个 Ninja 目录同源码产物逐字节相同；跨生成器（Ninja vs Makefiles）哈希不同，是已记录的边界 |

**✅ X2-e4 平台补丁归位（2026-11-19 17:0x）**

计划："`ui_window.cpp` 那 28 处 `#ifndef` 与 `ime_ui.cpp:27-32` 的重定义，移到 `syntax_stub` 侧，
生产代码不再猜常量值"（A-P2-11）。实际范围是 **47 处 / 5 个 UI TU**，不是审计点名的 30 处。

| 项 | 计划 | 实际 |
|---|---|---|
| 生产侧 | 不再猜常量值 | 5 个 UI TU 共删 47 块（ui_window 28、toolbar 9、candidate 5、composition 3、ime_ui 2）；include guard 不是 SDK 补丁，保留 |
| stub 侧 | 承接补丁 | `tools/syntax_stub/windows.h` 新增 25 个宏；已有的刷子 / `SM_*` / `SPI_GETWORKAREA` / `HWND_TOPMOST` 不重复定义 |
| 新增断言 | — | 3 条（94 → 97）：生产侧禁止"常量被自己的 `#ifndef` 包住再 `#define`"（含 `#if !defined`）/ stub 表面覆盖 UI 用到的 SDK 标识符 / CI syntax check 必须包含 stub 与 5 个 UI TU |
| 变异 | — | 3 次全红：恢复 `WS_POPUP` 补丁 / 删 stub 的 `TRANSPARENT` / CI 命令里改掉 `toolbar.cpp` |
| 门禁 | — | `ctest` 16/16、`check_m1` **97/97**、`check_docs` 7/7；宿主 syntax check 17 个 TU 全过 |
| 对外面 | 不变 | 交叉构建前后 sha256 同为 `b0529131…`（947,820 B），连字节都没变——这些 `#ifndef` 在真实 SDK 下从未生效 |

**✅ X2-e5 版本收敛（2026-11-19 17:2x，达成 G7）**

计划："只留 `ime/version.h`；`project(VERSION)` 改为无关值或从 `version.h` 读；断言其余位置不得出现四位版本号"。

| 项 | 计划 | 实际 |
|---|---|---|
| CMake | 从 version.h 读 | `file(READ)` 读 MAJOR/MINOR/PATCH 与 `VERSION_STRING`；`PROJECT_VERSION` 由 `0.0.1` 变为 **0.0.2**；字符串与数字不一致直接 `FATAL_ERROR`。资源文件原本就引用 `version.h`，一行未改 |
| 断言 | 其余位置再出现即失败 | 3 条（97 → 100）：CMake 派生且字符串自洽 / 代码与构建文件禁止 `alphaN.N.N` 与裸 `FILEVERSION` 元组 / `.rc` 仍展开宏。白名单只有真值与两个发版脚本；Markdown 历史叙述不扫 |
| 变异 | — | 4 次全红：写回字面 `project(VERSION 0.0.1)` / 无关脚本写 `alpha0.0.2` / `.rc` 写裸 `0,0,2,0` / `version.h` 字符串改 `alpha0.0.3` 而数字不动 |
| 门禁 | — | `ctest` 16/16、`check_m1` **100/100**、`check_docs` 7/7；`check_release_version.py alpha0.0.2 --binary` 全过 |
| 对外面 | 不变 | 交叉构建前后 sha256 同为 `b0529131…`；资源解析 `FILEVERSION=0.0.2.0`、字符串 `alpha0.0.2`，与 tag 一致 |

**✅ X2-e6 跨层 include 归零（2026-11-19 17:4x）**

计划："`dat_source_win.cpp` 不再引宿主私有头，`trace_stamp.h` 降为 `imfw` 内部可见或复制最小接口"（复核 N / X1 表）。

| 项 | 计划 | 实际 |
|---|---|---|
| 归属 | 不再引私有头 | `git mv` 到 `framework/include/imfw/trace_stamp.hpp`，三处调用者统一 `#include <imfw/trace_stamp.hpp>`；没有复制实现，`kTracePath` / `put_uint` 仍只有一份 |
| CMake | 断开 engine → platform 边 | 删掉 `pinyin/CMakeLists.txt` 中指向 `framework/platform/win32/imm32` 的 include 路径；靠 `imfw-engine` 的 PUBLIC include 传递 |
| 新增断言 | — | 4 条（100 → 104）：数据源包含公共头 / engine 无平台 include / 宿主调用者仍包含 / 私有头不复活且唯一格式器在公共头；X2-d 旧断言同步改路径 |
| 变异 | — | 3 次红（旧 include 回来 / 删 dllmain include / 重建私有头）+ 1 次控制（include 路径写成注释仍绿，证明匹配调用而非词） |
| 门禁 | — | `ctest` 16/16、`check_m1` **104/104**、`check_docs` 7/7；17 TU 语法检查全过 |
| 对外面 | 不变 | 交叉构建 sha256 仍为 `b0529131…`（947,820 B），依赖面 5 个 DLL 不变；trace 版 971,833 B，三条 `dat ...` 诊断串仍在 |

**✅ X2-f seam 上限落地（2026-11-19 18:4x，达成 G5）**

计划："seam 清单里的调用点数量有上限断言"。X2-f 只交付清单和闸门，不抽新接口。

| 项 | 计划 | 实际 |
|---|---|---|
| 清单 | 5 条 seam | `tools/seams.py`: `engine` 4/4、`scheme-policy` 12/12、`ui-exit` 12/12、`imm32-abi` 16/16、`can-render` 2/2；计数全部等于上限 |
| 计数正确性 | 调用点而非词 | `strip_comments()` 先删 `/* */` 与 `//`，注释里出现的路径/旧名字不计；与 X2-a 词匹配假绿同型防御 |
| 闸门 | `check_m1` 新增一项 | `check_m1.py` 用 `importlib` 加载同一个 `seams.py`，只调 `over_cap(root)`，不复制第二份正则；失败信息列 `name=count>cap` |
| 变异 | — | 5 次全红：逐条制造一个越界，每次点名对应 seam |
| 门禁 | — | `ctest` 17/17、`check_m1` 108 → **109/109**、`check_docs` 7/7 |
| 对外面 | 不变 | 零运行时改动：16 导出、5 个 DLL 的导入函数集、`.ime` 字节均不变（X2-w3 的 `f3e33b0a…` 仍是当前基线） |

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

### X2-w 临时需求批：三条语义变更（排在 X2 收口后、X3 之前）

用户 2026-11-19 12:2x 的决定："和上次挂起的临时需求的一起排期，先把 X2 做完"。
这三条**全部改行为**，而 X2-c/d 的验收判据恰恰是"行为逐字不变"——所以它们**不能插在重构中间**：
一旦同时有"重构"和"语义变更"两个原因能让日志变，出问题就分不清是谁。

| 编号 | 项 | 现在的状态 | 会动的判据（必须连带改） | 完成判据 |
|---|---|---|---|---|
| **X2-w1** | 状态指示器 `拼:开` / `拼:关` | **已合入 main**（cherry-pick 自 `wip/status-indicator`）；冲突按"保留 X2-d 的 `activate_session()`、丢掉分支里的 SDK 宏补丁"解决 | `check_m1` 104→107；`ctest` 16→17（`toolbar-text`）；导出/导入面**不变** | 真机**同框待验**：工具栏 `拼:开`→`拼:关`，托盘「中」不动 |
| **X2-w2** | 候选窗稳定性（不再每跨音节闪一次） | **已合入 main**（cherry-pick 自 `wip/candidate-window-stability`，无冲突）；旧断言 `trailing separator has no candidates` 已连同产品决策一起改 | `check_m1` 不变 107；`pinyin-session` 新增三组判据；导出/导入面不变 | 真机**待验**：`nihao` 逐键候选数 `0,4,4,4,4`，输入不再闪 |
| **X2-w3** | 候选窗翻页重置 | **已合入 main**；新增 `CandidatePager::replace()`，打字保留页并 clamp，边界事件仍 `reset()` | `check_m1` 107→108；`host_smoke` 新增精确日志段；导出/导入面不变 | 真机**待验**：翻到第 2 页后继续打字页码保持 |

**✅ X2-w 合入记录：w1（2026-11-19 17:5x）**

| 项 | 实际 |
|---|---|
| 冲突 | 3 处：模块地图合并 X2-c/d/e 与 w1；`imm_entry.cpp` 保留 `activate_session()` 并给首次 `UiToolbarShow` 传 `fOpen`；`toolbar.cpp` 丢掉分支里 5 处 SDK 宏补丁（X2-e4 已归 stub） |
| 门禁 | `ctest` **17/17**（新增 `toolbar-text`）、`check_m1` 104 → **107/107**、`check_docs` 7/7；17 TU 语法检查全过 |
| 变异 | 3 次全红：标题写死 `L"拼"` / 删一处键路径同步 / 插入 `ActivateKeyboardLayout` |
| 对外面 | 16 导出、5 DLL 导入函数集逐条同序不变；`.ime` 947,820 → 951,461 B，sha `b0529131…` → `0ac616f1…`（行为变更，预期） |
| 真机 | 待用户同框确认：Shift 后工具栏切字、托盘「中」不动 |

**✅ X2-w 合入记录：w2（2026-11-19 18:0x）**

| 项 | 实际 |
|---|---|
| 冲突 | 无，cherry-pick 干净。产品决策变更：旧断言 `trailing separator has no candidates` 改成保留 `[xi]` 与「西」，另加 `zhongguo` 无空档 / `zhongg` 保留「中」/ 上屏后尾巴 `g` 留下 |
| 量化 | `nihao` 逐键候选数 `0,4,0,4,4` → `0,4,4,4,4`；探针实测 `zhongguo` 中途不再掉到 0 |
| 门禁 | `ctest` 17/17、`check_m1` 107/107、`check_docs` 7/7 |
| 变异 | 3 次全红：退回整串切分 / 去撇号分支 / 去最长头搜索 |
| 对外面 | 16 导出、5 DLL 导入函数集逐条同序不变；`.ime` 951,461 → 952,485 B，sha `0ac616f1…` → `1ced1364…` |
| 真机 | 待用户连续输入 `nihao` 确认不再闪 |

**✅ X2-w 合入记录：w3（2026-11-19 18:2x）**

| 项 | 实际 |
|---|---|
| 做法 | 新增 `CandidatePager::replace()`：保留当前页，页越界时 clamp；`Input/Absorb` 用它，`finish_composition` / 提交 / Backspace 仍用 `reset()` |
| 判据 | `candidate_pager_test` 三组（保留页 / 短列表 clamp / 空列表）；`host_smoke` 精确日志段：20 候选 → 翻页 → 打字仍 `page=2/3` → 缩到 5 条时 `page=1/1`，翻页键不调用 engine |
| 门禁 | `ctest` 17/17、`check_m1` 107 → **108/108**、`check_docs` 7/7 |
| 变异 | 3 次全红：打字改回 `reset()` / `replace()` 无条件归零 / 去掉 clamp |
| 对外面 | 16 导出、5 DLL 导入函数集逐条同序不变；`.ime` 952,485 → 953,082 B，sha `1ced1364…` → `f3e33b0a…` |
| 真机 | 待用户翻页后继续打字确认页码保持 |

**顺序写死：w1 → w2 → w3**。理由不是重要性，是**先把两个"已经写完的"清出队列**：
w1 和 w2 各自压着一份已验证的改动（分支），放着越久与 X2-d 的拆分偏离越远；w3 是新增的十行，
放在最后不占任何人。

**准入条件（每条都要过）**：合入前在当时代码上重跑 `ctest` 全绿 + `check_m1` 全绿 + 变异至少 2 次变红；
合入后立刻真机看一眼——这三条的症状全在 UI 上，测试只能证明"序列对了"，证明不了"看起来对了"。

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
| M2 | **seam 清单 + 上限断言** | **已落地（X2-f）**：`tools/seams.py` 统计 5 条 seam 的调用点数（含 `UiCanRender`），`check_m1` 直接 import `over_cap()`：任一 seam 超上限即报错 | 耦合只能减少不能增加；某天从 6 涨到 20 就是抽象的必要性证据 |
| M3 | **接口状态标记** | 头文件给每个契约标 `// status: freeze \| candidate \| draft`（`Engine`/`Scheme` = candidate；`Intent` = draft） | "冻结"变成显式、可检索的事；`check_m1` 可断言"draft 接口不进 DLL 导出" |

三条都不改运行时代码，M1/M2 各半天。

## 5. 排程（插在 ROADMAP 的哪里）

| 阶段 | 放在 | 工期 | 对 v0.1 的影响 |
|---|---|---|---|
| X1 目录归位 | **M2 收口后立刻**（现在） | 0.5–1 天 | 0 |
| X2 安全网 + 构建收敛 | **M3 开工前**（硬约束） | 6–8 天 | +约 1.5 周 |
| X2-w 临时需求批（3 条语义变更） | **X2 收口后、X3 之前** | 约 1 天 | 0（但必须先于 M3 的面板改动） |
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

- [x] X1-a `git mv` + `ime/CMakeLists.txt` 路径 + `check_m1` 15 处 + 文档路径（审计 A-P1-4 的目录部分）
- [x] X1-b 字节比对报告（3 个链接器戳区域，`tools/compare_ime.py`）写入 `docs/handover/HANDOVER.md` §11.23

**X2 — 安全网 + 构建收敛**

- [x] X2-a0 `SchemeRegistry` 注入缝 + toy scheme（审计 A-P0-2；达成 G4）— 2026-11-18，见 §3 X2-a0 执行记录
- [x] X2-a `handle_key` 剥离 + `KeyEvent`/`KeyResult`（行为不变）— 2026-11-18，见 §3 X2-a 执行记录
- [x] X2-b `tests/host_smoke_test.cpp`（假 engine/scheme/sink/can_render，5 段键序）（审计 A-P0-1）— 2026-11-19，见 §3 X2-b 执行记录
- [x] X2-c `route_key` + `Intent` 单测；`host_smoke` 序列不变 — 2026-11-19，见 §3 X2-c 执行记录
- [x] X2-d `imm_entry` 按职责拆 TU（实际 4 个，候选过滤在 X2-a 已走）+ 合并两条激活路径 + 合并 trace 十进制转换（审计 A-P0-3、复核 N-1）
- [x] X2-e1 `platform/win32/{imm32,ui}` 库目标 + `ime` 只 link（审计 A-P1-4）— 2026-11-19，见 §3 X2-e1 执行记录
- [x] X2-e2 `dat_source_win.cpp` 归 `rospinyin-engine` + 链接期符号断言（审计 A-P1-5）— 见 §3「X2-e2」
- [x] X2-e3 `CMakePresets.json` + `EXISTS` 硬失败 + 统一根变量 + C 标准 + 静态策略收敛（审计 A-P1-6）— 2026-11-19，见 §3 X2-e3 执行记录
- [x] X2-e4 28 处 `#ifndef` 平台补丁归位（审计 A-P2-11；实际 47 处 / 5 个 TU）— 2026-11-19，见 §3 X2-e4 执行记录
- [x] X2-e5 版本收敛到 `ime/version.h` 一处 + CI 断言（审计 A-P1-7 版本部分；达成 G7）— 2026-11-19，见 §3 X2-e5 执行记录
- [x] X2-e6 `dat_source_win.cpp` 不再引宿主私有头（复核 N / X1 表）— 2026-11-19，见 §3 X2-e6 执行记录
- [x] X2-f `tools/seams.py` + `check_m1` 新增 seam 上限项（含 `UiCanRender`，达成 G5）— 2026-11-19，见 §3 X2-f seam 上限落地
- [x] X2-w1/w2/w3 临时需求批（状态指示器 / 候选窗稳定性 / 翻页重置）— 三条均已合入 main，见 §3「X2-w 合入记录」；真机观感待验

**待排期 → 已排期（2026-11-19 12:2x：三条一起进 X2-w，排在 X2 收口后、X3 之前）**

> 从"记账"升级为"排了号"：下面三条都有编号、有顺序、有准入条件，见 §3「X2-w 临时需求批」。
> 之前只有"未提交，在 `git stash`"这种状态，等于靠记忆管理——两个 `stash` 一个都记不住的代价是重做一遍。

- [x] **X2-w2 候选窗稳定性**：`recompute()` 只给"已完成音节"发候选，否则每跨一个音节边界候选窗闪一次
  （`nihao` 的候选数：`0,4,0,4,4` → `0,4,4,4,4`）。**已合入 main**；旧断言连同产品决策一起改，
  3 次行为变异全红，真机待验。
- [x] **X2-w1 状态指示器**：托盘那个红底「中」是 ReactOS 通知区的**布局**指示器（跟 HKL 走，不跟 `fOpen` 走），
  用户 2026-11-19 明确它**不该**跟着 Shift 变；状态改由我们自己的工具栏显示 `拼:开` / `拼:关`。
  **已合入 main**，见 §3「X2-w 合入记录」；真机同框待验。
- [x] **X2-w3 候选窗翻页重置**：`handle_key` 每键 `pager.reset()`，翻到第 2 页后继续打字被踢回第 1 页（约 10 行）。
  **已合入 main**：`replace()` 保留页并 clamp，`reset()` 留给边界事件；3 次变异全红，真机待验。

**X3 — 可插拔验收**

- [ ] X3-a `samples/toy-scheme/` + `samples/README.md` + 从零接入演练
- [ ] X3-b per-instance 隔离：`g_hInst` / `g_stats` / `g_last_geometry` / `GlyphProbe` / `WindowRegistry`（审计 A-P1-8）

**X4 — 等门**

- [ ] M3（机制）头文件 `status:` 标记 + 断言"draft 接口不进 DLL 导出"
