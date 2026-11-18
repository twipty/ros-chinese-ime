# 架构审计复核（2026-11-18）

> **被复核对象**：`code-audit/ros-pinyin-架构优化分析.md`（存放在项目仓库之外，审计方声明不入库）。
> 审计基线 `main @ 687b3b8`、tree `cd9c8ef`。本文由提交 `f3f270a` 纳入，因此**本文一旦提交，
> HEAD 就不再是 687b3b8**——这是自指失效，不是取证错误：`f3f270a` 只改文档、核心代码零改动，
> 下文所有代码行号仍按 687b3b8 快照有效。（2026-11-18 的 X1 目录归位又把 `framework/host|ui`
> 移到了 `framework/platform/win32/{imm32,ui}`，同样只改路径、不改行号。）
>
> **复核方法**：不看它的结论，只按它给的行号回代码取证。每条都跑命令，不看叙述。
> **复核者立场**：这是"验收审计"，不是"复述审计"。成立的保留，过期的改写，漏的补上。

## 0. 结论

**13 条问题结论：11 条成立、1 条成立但证据部分过期、1 条已过时。**
**12 项量化数字：10 项精确命中，2 项需按口径修正。**

整体判断：**这份审计值得作为治理依据**。它对代码的读法是对的（`scheme_instance()` 是链接期硬接线这一判断尤其准），
不是"通读一遍挑毛病"那种报告。唯一系统性问题出在**它把自己抄成了静态快照**：`imm_entry.cpp` 的行数、
`imb_entry` 的重复路径、`ime/` 的规模，都在同一天的修复中变过，它只追了一部分（文档归位追了，代码修复没追全）。

**三条只有复核才能发现的东西**（见 §3）：两条激活路径的调用顺序相反、计划自身的基线数字已失真、
`ime/version.h` 其实是**第四处**版本声明而不是"收敛"。

## 1. 逐条复核

### P0

| 编号 | 审计结论 | 我的取证 | 判定 |
| --- | --- | --- | --- |
| A-P0-1 | 宿主层零自动化测试 | `tests/CMakeLists.txt` 共 9 个可执行，`target_link_libraries` 只有 `rospinyin-engine` / `imfw-engine` 两种目标；`framework/host`、`framework/ui` **根本没有库目标**可链；`build-win` 的 `ROSPINYIN_BUILD_TESTS:BOOL=OFF` | **成立** |
| A-P0-2 | 全局工厂阻断注入 | `scheme.hpp:129` 声明 `Scheme& scheme_instance();`，`ime/scheme_reg.cpp:14` 返回函数内 `static PinyinScheme`；调用点 `session_store.cpp:34,55`、`ui_server.cpp:26,35`、`imm_entry.cpp`、`ime_ui.cpp` | **成立** |
| A-P0-3 | `imm_entry.cpp` god object | 783 行；职责分区可复现（trace `:48-176`、`emit_text:221`、`refresh_ui:244`、`adopt_result:339`、`process_key:409`、16 个导出 `:506+`）；trace 十进制转换**确实两份**（`:128` 与 `:141`） | **成立，但证据部分过期**（见 §2.1） |

### P1

| 编号 | 审计结论 | 我的取证 | 判定 |
| --- | --- | --- | --- |
| A-P1-4 | "framework 库"名不副实 | `framework/CMakeLists.txt` 只 `add_library(imfw-engine ...)` 5 个 `src/*.cpp`；`ime/CMakeLists.txt:26-35` 用 **9 个** `../framework/{host,ui}/*.cpp` + 1 个 `../pinyin/host/dat_source_win.cpp` 直接编进 `.ime` | **成立** |
| A-P1-5 | pinyin 插件在 Windows 上不自洽 | `pinyin/CMakeLists.txt` 的 `rospinyin-engine` = 4 个 cpp，**不含** `dat_source_win.cpp`；`dat_source.cpp:21` 的 `load_system_table()` 是 `__attribute__((weak))` 返回 `nullptr` | **成立**，且值得补充一层：弱符号只在 GNU/Clang 下成立，`RPY_WEAK` 在 MSVC 分支展开为**空**（`dat_source.cpp:13`），即变成普通定义、重复符号直接报 LNK2005。当前工具链是 MinGW，所以现实风险正是**静默退化**——漏链 `dat_source_win.cpp` 时 IME 能启动、候选只剩内置 109 条，没有任何断言会响 |
| A-P1-6 | 构建配置碎片化 | `EXISTS` 守卫 4 处（`CMakeLists.txt:45,49,53,57`）；`CMAKE_SOURCE_DIR`（`tests:56,57,66,68`）与 `PROJECT_SOURCE_DIR`（`ime:41-44`）混用；只设 `CMAKE_CXX_STANDARD`、C 标准未设；无 `CMakePresets.json`；静态链接策略 3 处（`ime:83`、`install-rospinyin:24`、`httpdl`）；`build-win` 测试 OFF 而 `build-win-trace-en` 测试 ON（**漂移实测确认**）；5 个 build 目录合计 32.9 MB | **成立** |
| A-P1-7 | 数据与版本的双真值 | `kBuiltin` 实测 **109 条**（审计写 108）；版本实测 **4 处**而非 3 处（见 §2.2） | **成立，数字需修正** |
| A-P1-8 | 全局可变状态与双 IME 共存冲突 | `g_hInst`（`ime_host.h:17` 声明 / `dllmain.cpp:21` 定义 / `dllmain.cpp:123` 赋值）、`g_stats`（`ui_window.cpp:165` 无锁 `++`）、`g_last_geometry`+`g_anchor_from_caret`（`ime_ui.cpp:94-95`）、`GlyphProbe` 单例（`ime_ui.cpp:292`）、`WindowRegistry` 单例 | **成立** |

### P2

| 编号 | 审计结论 | 我的取证 | 判定 |
| --- | --- | --- | --- |
| A-P2-9 | 字体与候选正确性耦合 | `GlyphProbe`（`ime_ui.cpp:237-298`）、`UiCanRender`（`:298`）、`renderable_candidates`（`imm_entry.cpp:351+`，含"全部/过半被剔则放行"双护栏） | **成立**。补充：这不是性能问题（审计已指出），也不是"可选项"——**它是产品行为依赖运行环境**，所以 X2 的冒烟测试必须能把 `UiCanRender` 换掉，否则测试在 CI 上恒为"全部可渲染" |
| A-P2-10 | `.dat` 全量同步加载 | `dat_source_win.cpp:82+` `CreateFileW` + 一次读入；全文件无 `mmap`/`MapViewOfFile`/异步路径 | **成立** |
| A-P2-11 | 生产代码里的平台补丁 | `ui_window.cpp` 的 `#ifndef` **精确 28 处**（与审计一致）；`ime_ui.cpp:27-32` 重定义 `SM_CXSCREEN/SM_CYSCREEN` | **成立** |
| A-P2-12 | `check_m1` 是静态分析不是行为测试 | `check_m1.py` 589 行、63 项，全部基于源码文本/调用点 | **成立**。"函数被正确调用"与"调用结果正确"确实是两件事 |
| A-P2-13 | 文档补丁式叠加 | 根目录已只剩 `README.md`，`docs/` 六分类 + `tools/check_docs.py` | **已过时**。审计自己已按新基线改写，但它把这条留在"P2 问题"里没降级——现在它不是问题 |

### 数字对照

| 项 | 审计值 | 实测 | 说明 |
| --- | --- | --- | --- |
| `framework/src` + `include/imfw` | 993 | **993** | 一致 |
| `framework/host` + `framework/ui` | 2697 | **2697** | 一致 |
| `imm_entry.cpp` | 783 | **783** | 一致 |
| `ime/` | 272 | **303**（全目录）/ **19**（`ime/*.cpp`） | **基线前进，不是口径差异**：审计读的是 `7c57fbc`；`2feab71` 新增 `ime/version.h`（+30 行）并微调 `rospinyin_res.rc`，实测 cpp/h/rc/def 合计 167 → 198。原稿误记为"口径不同、把 `.ico.b64` 算进去了"，2026-11-18 经审计方指出后修正 |
| `tests/` | 1314 | **1314** | 一致 |
| `check_m1.py` | 589 | **589** | 一致 |
| ctest | 11 | **11** | 一致 |
| 受控文件 | 191 | **191** | 一致 |
| build 目录 | ~33 MB | **32.9 MB** | 一致 |
| `#ifndef` 重定义 | 28 | **28** | 一致 |
| `kBuiltin` 条目 | 108 | **109** | 差 1 |
| 测试可执行 | 9 | **9** | 一致 |

## 2. 需要修正的三处

### 2.1 A-P0-3 的"三段逐字重复"已部分不成立

审计说 `ImeSelect`（`:574-601`）与 `ImeSetActiveContext`（`:642-661`）的
`acquire_session` → `on_select(true)` → `UiToolbarShow` **三段近乎逐字重复**。

**`AlignOpenStatus`（`:626-638`）是两处共用的函数**，`ImeSelect:593` 与
`ImeSetActiveContext:651` 都调它。

> **原稿此处写"审计读的是修复前的代码，最大的一块已经被抽走"——这条被审计方驳回，我接受。**
> 它的原文列的是 `acquire_session` → `on_select(true)` → `UiToolbarShow` **三段**，本来就没把
> `AlignOpenStatus` 算进重复段；U1 抽取之后这三段仍然逐字重复。所以"三段重复"成立，
> 是我的归因错了——这里不是"证据过期"，是**我误读了它的原文**。
> 准确表述应为：**三段序列重复，且其中两段的相对顺序在两条激活路径中相反**。

**剩下的重复是真的**，但只有 4 步、约 8 行：

```cpp
ImeSessionState* state = imfw::acquire_session(hIMC);
if (state != nullptr) AlignOpenStatus(hIMC, *state);
if (state != nullptr && state->engine != nullptr) state->engine->on_select(true);
imfw::UiToolbarShow(hIMC);
```

结论仍是要抽（抽成 `activate_session(hIMC)`），但工作量从"消除 28 行重复"降为"消除一处 4 步序列"。
**这恰恰说明审计的价值有时效性**：它是对的，但代码比报告跑得快。

### 2.2 版本不是 3 处，是 4 处

审计说"仍有三个并存版本"，并把 `ime/version.h` 列为"已修复项"。实测：

| 位置 | 值 | 用途 |
| --- | --- | --- |
| `CMakeLists.txt:3` | `0.0.1` | `project(VERSION)`，无人校验 |
| `pinyin/include/rospinyin/version.hpp:9` | `0.0.2-m2-dev` | 插件开发版本 |
| `framework/include/imfw/version.hpp:14` | `0.1.0-imfw` | 框架开发版本 |
| `ime/version.h` | `alpha0.0.2` | **发版二进制**，被 `check_release_version.py` + CI 校验 |

`ime/version.h` 不是"收敛"，是**新增了第四处**——它解决了"发布的二进制版本是错的"，
但没解决"四个地方各说各话"。而且 `CMakeLists.txt` 那个 `0.0.1` 是最危险的：
它是 `project(VERSION)`，将来一旦有人拿 `${PROJECT_VERSION}` 命名产物，文件名会带错版本。

**判定：问题成立，且比审计描述的更值得修。**

### 2.3 `ime/*.cpp` 从 57 行变成 19 行，说明计划自身的基线也失真

`PLAN-MODULARIZATION.md §2` 的基线快照写 `wc -l ime/*.cpp # 57 装配`。实测 **19 行**（`ime/` 只剩
`scheme_reg.cpp` 一个 cpp）。同一份计划里 `framework/host + ui` 记的是 2584，实测 **2697**（+113）。

审计没有发现这一点，因为它比对的是另一套数。但这正好印证审计 §1 的观点：
**"文件规模快照两天就失真"**——这份计划自己也中了。所以 §2 的基线表必须改成"可重生成的数字"
（`tools/snapshot_sizes.py` 这个建议**不是审计提的**，出自
`docs/plan/PLAN-MODULARIZATION.md` 与 `docs/reviews/FRAMEWORK-ROUTE-A-REVIEW.md`；
原稿把这笔账记在审计头上，2026-11-18 已更正。方向对，但优先级低，见 §5）。

## 3. 审计漏掉的三条

### N-1 两条激活路径的调用顺序**相反**

这是复核时才看见的，审计把它们当成"重复"处理，实际上它们**不一致**：

```
ImeSelect            : on_select(true) → AlignOpenStatus → UiToolbarShow
ImeSetActiveContext  : AlignOpenStatus → on_select(true) → UiToolbarShow
```

`AlignOpenStatus` 会调 `ImmSetOpenStatus`。如果引擎的 `on_select(true)` 里读了开合状态
（现状没读，但这是引擎契约里允许做的事），两条路径就会得到不同的起点。
**今天 U1 那个"首次激活 IME 是关的"bug，本质就是这两条路径的顺序与守卫不一致导致的。**
抽 `activate_session()` 时顺手统一顺序，成本几乎为零；不统一的话，这个 bug 会以别的形式再来一次。

### N-2 审计的"X1/X2/X3 一项都没落地"应更精确

它说"8 项未勾、0 项完成"。实测确实 8 项全空。但**没落在计划上的东西还有更多**：
审计自己提出的 A-P1-4/5/6（库边界、插件自洽、构建收敛）在 X1–X4 里**根本没有对应条目**——
X1 只是 `git mv`，不动构建；X1 的验收（字节比对）恰好可以复用给"拆库目标"，但计划没这么写。
也就是说：**问题清单比计划条目多**，这是本次要补的核心（见 §4）。

### N-3 "假 scheme"在现有代码下无法实现

计划 `PLAN-MODULARIZATION.md §3 X2` 第 2 步写"假 scheme（字母表 `a-z` + `1-9` 选字…）"。
但 `session_store.cpp:34` 直接调 `scheme_instance().create_engine()`，没有 setter、没有注册表、
没有上下文对象。`ime/CMakeLists.txt:7` 的注释说得很清楚：第二个方案靠**换一个 `scheme_reg.cpp` 再编译一次**。

所以 **X2-b 按现计划一开工就卡住**：假 scheme 塞不进去，除非改 framework。
而"改 framework 加注入点"正是 A-P0-2，它必须成为 X2 的第一步，而不是 X3 的附属品。
这条不改，X2 的 3–5 天工期是假的。

## 4. 纳入 X1–X4 的映射

逐条对应见 `PLAN-MODULARIZATION.md §3`。这里只给"哪个问题进了哪个阶段"的索引：

| 审计编号 | 问题 | 归入 | 做法 |
| --- | --- | --- | --- |
| A-P0-2 | 全局工厂阻断注入 | **X2-a0**（新增，X2 第一步） | Scheme 注册表 + 上下文，toy scheme 作首个消费者 |
| A-P0-1 | 宿主层零测试 | X2-b | 假 engine/sink/scheme + 键序断言 |
| A-P0-3 | god object | X2-d（新增）+ N-1 | 按职责拆 TU；两条激活路径合并为 `activate_session()` |
| A-P1-4 | 库边界不一致 | X1-b（新增） | `platform/win32/{imm32,ui}` 库目标，`ime/` 只 link |
| A-P1-5 | 插件不自洽 | X1-c（新增） | `dat_source_win.cpp` 归 `rospinyin-engine` + 启动期断言 |
| A-P1-6 | 构建碎片 | X1-d（新增） | presets + `EXISTS` 硬失败 + 统一根变量 + C 标准 + 静态策略收敛 |
| A-P1-7 | 版本四处 | X1-e（新增） | 收敛到 `ime/version.h` 一处 + CI 断言其余三处不再出现版本号 |
| A-P1-8 | 全局可变状态 | X3-b（新增） | per-instance 上下文；G4 判据要求同进程可共存 |
| A-P2-9 | 字体耦合 | X2-b | `UiCanRender` 作为第 5 条 seam，测试里可替换 |
| A-P2-11 | 平台补丁 28 处 | X1-d | 与 `syntax_stub` 分离，生产代码不再 `#ifndef` 重定义 |
| A-P2-12 | 闸门是静态分析 | 不换 | 保留契约定位，由 X2-b 补行为测试，二者不互相替代 |
| A-P2-13 | 文档叠加 | 已完成 | 9bc9bcf 已解，本条关闭 |
| A-P2-10 | `.dat` 全量加载 | **不纳入** | 属性能，归 M4/M5；与结构治理目标不同 |
| A-P1-7 | `kBuiltin` 109 条双真值 | **不纳入** | M4 扩码表时必然重做数据管线，现在改是重复劳动 |

## 5. 复核对审计的两点保留意见

1. **它把"顺序 0"写成"注入缝 + toy scheme 一起做"，我同意，理由要改**：它说这是"解锁一切的前置"，
   但真正的理由是 **X2 的假 scheme 需要它**（N-3）。toy scheme 只是注入缝的第一个消费者，
   顺手把 X3 的判据（`framework/` 零 diff）提前达成。
2. **`tools/snapshot_sizes.py` 我不排期**（出处是 `PLAN-MODULARIZATION` 与
   `FRAMEWORK-ROUTE-A-REVIEW`，不是审计报告）。理由：行数是"可重生成的数字"这点对，但
   手工抄错行数的代价 < 写一个脚本再维护它；更好的办法是**把基线数字写进 CI 的输出**
   （`ctest` 与 `check_m1` 已经在打印项数，把行数并进去即可）。这条记为"可选"。

---

**复核用到的命令**（可重放）：

```bash
wc -l framework/src/*.cpp framework/include/imfw/*.hpp | tail -1      # 993
wc -l framework/host/* framework/ui/* | tail -1                       # 2697
grep -n 'scheme_instance' framework/include/imfw/scheme.hpp ime/scheme_reg.cpp
grep -n 'add_library\|target_link' framework/CMakeLists.txt pinyin/CMakeLists.txt
cd build && ctest -N | tail -1                                        # 11
python3 tools/check_m1.py | tail -1                                   # 63
```

---

## 6. 被审计方确认（2026-11-18 22:00 CST）

**确认人**：`code-audit/ros-pinyin-架构优化分析.md` 的作者。逐条回代码取证，未采信复核叙述。

> **本节由审计方撰写，原文保留。** 它指出的 4 处问题（§2.1 归因、`ime/` 数字归因、
> `snapshot_sizes.py` 出处、基线自指失效）经我逐条验证，**全部成立**，已直接改在正文
> 对应的位置（§1 表格、§2.1、§5、文首基线声明），不是只在这里挂一条批评。

> **★ 路径迁移提示（22:02 实测）**：确认时工作树出现**在途未提交的 X1 目录归位**（19 个文件已暂存、`git mv` 纯改名 0 行改动）：
> `framework/host/*` → `framework/platform/win32/imm32/*`，`framework/ui/*` → `framework/platform/win32/ui/*`。
> 本文与审计报告正文引用的旧路径均按审计基线 `687b3b8` 记录；**行号在新路径下完全不变**（已实测 `imm_entry.cpp:585/593/651/653` 四处关键证据一致）。X1 提交后，按映射替换路径即可，结论与行号无需改。

### 6.1 接受且纳入修正（4 项）

| 复核项 | 确认 | 取证结果 |
|---|---|---|
| **N-1 两条激活路径顺序相反** | **接受，这是我漏掉的真发现** | `ImeSelect`: `on_select(true)`(`:585`) → `AlignOpenStatus`(`:593`)；`ImeSetActiveContext`: `AlignOpenStatus`(`:651`) → `on_select(true)`(`:653`)。顺序确实相反。我的"三段逐字重复"表述应改为"两步序列重复 + 一步顺序不一致"。 |
| **§2.2 版本是 4 处不是 3 处** | **接受** | 实测四处：`CMakeLists.txt:3`=0.0.1、`pinyin/version.hpp:9`=0.0.2-m2-dev、`imfw/version.hpp:14`=0.1.0-imfw、`ime/version.h:28`=alpha0.0.2。`ime/version.h` 解决了"发版二进制版本错误"，但没解决"四处各说各话"；且 `project(VERSION)` 那个 0.0.1 确实最危险。 |
| **§1 数字修正 `kBuiltin` 109** | **接受** | `scheme.cpp:29-211` 实测 109 条，我写 108，差 1。 |
| **A-P1-5 补充：MSVC 下 weak symbol 不是"静默退化"而是 LNK2005** | **接受** | `dat_source.cpp:13` 的 `RPY_WEAK` 在 MSVC 分支为空 → 普通定义，与强符号重复。当前 MinGW 工具链下的现实风险确为静默退化，两种情形应分开写。 |
| **A-P2-9 补充：`UiCanRender` 需作为第 5 条 seam 可替换** | **接受** | 否则 X2 冒烟测试在 CI 上恒为"全部可渲染"，测不到字体过滤路径。 |

### 6.2 需要澄清，不改变结论（3 项）

1. **§2.1 说"审计读的是修复前的代码"——不准确。** 我的报告 §3.3 原文列的是 `acquire_session` → `on_select(true)` → `UiToolbarShow` **三段**，本就没把 `AlignOpenStatus` 算进重复段；U1 抽取后这三段仍是逐字重复。所以"三段重复"判断成立，复核说的"最大一块被抽走"是它对我原文的误读。**但** N-1 指出的"两段顺序相反"我确实漏了，这条照单接受。修正后的准确表述：**"三段序列重复，且其中两段的相对顺序在两条激活路径中相反"**。

2. **数字对照表 `ime/` 272→303 的归因错误。** 复核写"口径不同，审计把 `.ico.b64` 算进去了"——实测差异来自提交 `2feab71` 新增 `ime/version.h`（+30 行）及 `rospinyin_res.rc`（+9/-4）。我审计时的 272 是 `7c57fbc` 的正确读数，不是口径问题。复核的命令 `wc -l ime/*` 与其引用的"9 个 cpp"也自相矛盾（`ime/*.cpp` 实测 19 行、仅 1 个文件）。建议数字对照表把该项改注为**"审计基线 272；新基线 303（version.h +30、rc +5），非口径差异"**。

3. **§5 第 2 点"`tools/snapshot_sizes.py` 是审计建议的"——出处归属有误。** 该建议的真实出处是 `docs/plan/PLAN-MODULARIZATION.md:272` 与 `docs/reviews/FRAMEWORK-ROUTE-A-REVIEW.md:214`，我的审计报告从未提过这个脚本。不过"不排期、把行数并进 CI 输出"的判断我认同，这条按"可选"处理无异议。

### 6.3 基线声明需自修正

复核第 4 行称"687b3b8、cd9c8ef —— 与本仓库当前 HEAD 一致"。**该文件本身由提交 `f3f270a` 纳入，因此它一旦提交，当前 HEAD 就不再是 687b3b8**（实测 HEAD=`f3f270a697075c586c91ed83ad6dc5ac7f0d52a9`）。这是自指失效，不是取证错误——`f3f270a` 只改文档、核心代码零改动，复核的全部代码行号仍有效。建议措辞改为：

> 审计基线 `main @ 687b3b8`、tree `cd9c8ef`；本文纳入提交后 HEAD 前进至 `f3f270a`（仅文档变更，核心代码未变），所有代码行号仍按 687b3b8 快照有效。

### 6.4 总体确认

**确认这份复核成立，可作为治理依据。** 13 条问题判定我接受 11 条、澄清 2 条（§2.1 归因、数字 272 归因），无一条需要推翻。N-1（激活顺序相反）是本次复核最有价值的增量，已回写我的审计报告。`code-audit/ros-pinyin-架构优化分析.md` 将同步更新上述四项接受项与基线说明。
