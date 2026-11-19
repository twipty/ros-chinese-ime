# 变更日志

本项目版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。1.0 之前次版本号可能包含不兼容变更。
`alpha` 前缀的含义：**可在 ReactOS 0.4.16+ 上实机安装并输入单字**，但功能、数据与接口尚未定型。

---

## 未发布

### 数据源归位，缺席即链接错误（X2-e2）

- **`pinyin/host/dat_source_win.cpp` 归 `rospinyin-engine`**（Windows 分支），不再由 `ime/CMakeLists.txt`
  列出来编译。它是拼音方案自己的数据来源，不是装配层的细节。
- **weak 兜底在 Windows 上被编译掉**（`ROSPINYIN_REQUIRE_SYSTEM_TABLE`）：`pinyin/src/dat_source.cpp`
  里那个 `return nullptr` 的 `load_system_table()` 只保留给 Linux 与宿主测试。于是"忘了编真实数据源"
  从**静默降级**变成**链接失败**——实测改动前删掉那一行仍能编出 942,989 B 的 `.ime`（正常 948,481 B），
  装上去只是候选变少、多音字消失，没有任何提示。
- **`ime/CMakeLists.txt` 清理**：连同 X2-e1 留下的临时 include 路径、以及 `if(ROSPINYIN_TRACE)
  target_compile_definitions(rospinyin-ime …)` 一并删除——该目标里已没有读这个宏的 TU，
  留着就是"看起来生效、实际没改一个字节"的开关。
- **`check_m1` 83 → 87 项**：数据源归属（匹配 `target_sources(...)` 这个调用，不匹配路径词）/
  Windows 必须有 `ROSPINYIN_REQUIRE_SYSTEM_TABLE` / 守卫只包住兜底 / 装配目标不再声明 trace。
  变异 4 次全红（含"把宏改成注释"——词还在、调用没了）。
- 对外面：16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序仍完全一致；函数符号 1386 个两边逐个相同。
  i686 拼音 `948,481 → 947,820 B`（−661，全在节对齐）、trace 版 `971,982 → 971,833 B`、玩具不变。
- 遗留：`dat_source_win.cpp` 仍引 `trace_stamp.h`，`rospinyin-engine` 因此带一条指向
  `framework/platform/win32/imm32` 的 include 路径；由 X2-e6 收掉。

### 平台层成为两个库（X2-e1）

- **`framework/platform/win32/CMakeLists.txt`（新）**：`imfw-win32-imm32`（宿主 7 个 TU）与
  `imfw-win32-ui`（窗口 5 个 TU）两个静态库。此前"什么属于宿主层"只存在于 `ime/CMakeLists.txt`
  那张 12 行源码清单里。
- **`ime/CMakeLists.txt` 只剩装配**：scheme 源码 + `rospinyin.def` + `rospinyin_res.rc`，
  一行平台源码都不写。拼音与玩具方案走同一份装配逻辑。
- **两个库互为依赖**（宿主调 `Ui*`，窗口层读 `imfw::g_hInst`）。CMake 允许 STATIC 成环，两条边都写出来；
  **没有**造第三个 "common" 库去把环藏起来。
- **`.def` 不会拉档案成员**：GNU ld 只为未定义引用拉静态库成员，而 16 个导出名是导出条目不是引用，
  首次链接直接 `cannot export ImeProcessKey: symbol not defined` × 16。给宿主库套
  `-Wl,--whole-archive` / `--no-whole-archive`，并写成断言（删掉即红）。
- **`option(ROSPINYIN_TRACE)` 上提到顶层 `CMakeLists.txt`**：读它的是平台库，而 `framework/`
  在 `ime/` 之前配置——留在原处会让 `-DROSPINYIN_TRACE=ON` 只影响装配件，且**无声**。
- **`.ime` 现在可复现**：加 `-Wl,--no-insert-timestamp`。在此之前 ld 把链接时刻写进 PE 头，
  两个构建目录编同一份源码得到**同为 948,481 B 但 sha256 不同**的产物（只差 6 字节：
  `TimeDateStamp`、`OptionalHeader.Checksum` 及 2 个镜像字节）。"同一个哈希"因此只能回答
  "是不是同一个文件"，回答不了"是不是同一次构建"——这正是 `artifact_hashes.py` / `rhash.exe` 要回答的。
  现在两个目录的产物逐字节相同（`134f7fd4…`）。
- **`check_m1` 78 → 83 项**：每个平台 TU 只属一个库（双向 + 先剥 CMake 注释）/ `ime/` 不许出现
  `../framework/platform/win32/` / whole-archive 紧贴宿主库 / trace 宏的声明位置 / 可复现链接。
  变异 4 次全红。
- 对外面：16 导出名及顺序、5 个 DLL 依赖、每 DLL 导入函数集**逐条同序**一致。
  i686 拼音 `947,969 → 948,481 B`（+512，一个文件对齐块）；trace 版 `971,982 B` 不变；玩具 `+1,333 B`。
- 顺带修 HANDOVER §4 目录树：`framework/host/`、`framework/ui/` 是 X1 之前的布局。

### 拆 god object（X2-d）

- **`imm_entry.cpp` 613 → 314 行**，按职责拆出三个 TU（`imfw` 宿主层现在是一张能一眼看完的地图）：
  - `key_translate.cpp`（103 行）：vk → `KeyEvent`（key-up 位）、`KeyResult` → `TRANSMSGLIST`
    （`WM_IME_CHAR` 容量与 `TRANSMSGLIST` 偏移规则）。**不再叫 `emit_text`**：它只发射 commit，
    与 `imfw::emit_text` 同名会让读者以为两者是一回事。
  - `win32_host.cpp`（92 行）：`Win32Host` —— 键策略的 Win32 半边，**整个平台层唯一实现 `imfw::Host` 的地方**。
  - `trace_line.cpp`（117 行）：`ROSPINYIN_TRACE` 通道。`#if` 开关换成 `IMFW_TRACE_ON`，
    插桩代码不再自己写 `#if defined(ROSPINYIN_TRACE)`。
- **两条激活路径合成 `activate_session(hIMC)`**：`ImeSelect(TRUE)` 与 `ImeSetActiveContext(TRUE)`
  原本做同样三步但**顺序相反**（U1"首次激活是关的"就是这么来的）。顺序固定为
  对齐 `fOpen` → `on_select(true)` → 工具条。**解除激活没有合并**（一个要销毁会话、一个只隐藏窗口，
  合并是改行为）。
- **trace 的两份手写十进制转换合并**到 `trace_stamp.h` 的 `put_uint()`，并给它补 `cap` 参数，
  让"截断而不是越界"可证。
- **`check_m1` 71 → 77 项**：拆 TU 让一多半 `... in entry` 断言失效（不是行为变了，是文件里没有了）。
  改法不是"拼 4 个 TU 继续匹配"，而是**按归属双向钉**：该有它的 TU 必须有，另一个必须没有。
- 平台层总数 **2511 → 2689 行（+178）**：四个文件各自的许可证头与 namespace 壳。
  `imm_entry.cpp` 少了 299 行，别只看这一个数字。
- 对外面：16 导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；i686 `+821 B`。

### 键路由提取为纯函数（X2-c）

- **新增 `framework/include/imfw/intent.hpp`（75 行）+ `framework/src/key_route.cpp`（70 行）**：
  `Intent route_key(const Scheme&, const KeyEvent&)` —— 一个键翻译成一个 `Intent`
  （`None / Input{ch} / Select{slot} / PagePrev / PageNext / Backspace / Escape`），
  不碰 session / engine / host，也不看 `key_up`/`ctrl`/`alt`/`fOpen`（那是归属判定，归 `wants_key`）。
- **`handle_key` 变成 `switch (intent.kind)`**，并且不再触碰 `scheme`：两个策略问题只问一次，在路由里问。
- **两处与计划不同**：`SelectDigit(n)` 改名 `Select{slot}`（空格不选数字却选第 1 个，旧名会让名字撒谎）；
  `ToggleLang` **删除**（Shift 那条路在 `wants_key`，是归属判定；一个只有"永不 dispatch 的那一层"
  才能产出的意图是陷阱）。
- **新增 `tests/key_route_test.cpp`（22 条断言，`ctest` 16/16）**：每个 Intent + payload、
  "键由方案决定"（`q`/`j`/`k` 的自定义方案，`1` 对它无意义）、分支顺序（同一键被两处声明时选择赢）。
- **判据没动**：`host_smoke` 的 13 条日志一行未改——搬判定时最容易顺手改的就是分支顺序。
- `check_m1` **71/71**（+3；G4 那条从 `host_key.cpp` 改指 `key_route.cpp`）；变异 6 次全红。
  其中一次暴露了我自己新断言的假绿（`"scheme."` 匹配不到 `scheme_instance().`），
  改成"不许有任何策略调用 + `scheme_instance()` 只许一次"。
- 对外面：16 导出 / 5 依赖 / 各自导入函数集**逐个一致**；i686 `946,299 → 947,148 B`（+849）。

### 状态指示器：Shift 切状态，不切布局（2026-11-19）— **未合入，排队 X2-w1**

> **状态：已实现并验证，`main` 上没有。** 在分支 `wip/status-indicator`；按用户"和候选窗补丁
> 一起排期、先把 X2 做完"的决定，排入 PLAN-MODULARIZATION §3「X2-w」，顺序 w1 → w2 → w3。
> 下面记的是那份改动的实测数字，不是 `main` 的。

- **语义澄清**：托盘红底「中」跟键盘布局（HKL）走，Shift 只改 `fOpen` → 它**不该变**，此前
  "托盘不跟随"的记录方向有误。状态由我们自己的工具栏显示：`拼:开` / `拼:关`。
- **`Scheme::toolbar_state_label(bool)`**（默认 `nullptr` = 无状态，四个假方案零改动）：
  拼音 `开/关`，玩具 `on/off`。
- **`framework/src/toolbar_text.cpp`（新，可移植）**：`拼` + `:` + 状态。提到可移植层是为了能测——
  窗口要 GDI，字符串不用。新增 `tests/toolbar_text_test.cpp`（`ctest` **16/16**）。
- **`toolbar.cpp`**：宽度改为实测（原来写死 44 px 会把 `拼:开` 裁掉），右边缘固定向左生长；
  文本相同且可见时直接返回，避免每键一次 `InvalidateRect`。
- **`imm_entry.cpp`**：每个键在 `ImeProcessKey` 里拉一次状态（bool 比较），Shift 手势后立刻再拉
  一次。用"拉"而不是给 `Host` 加钩子，是因为 imm32 的 Ctrl+Space 直接改 `fOpen`、根本不进我们 DLL。
- `check_m1` **71/71**（+3：标题必须由 `toolbar_text` 生成且旧直传必须消失、两处同步计数、
  布局切换函数全仓库 0 处）；变异 5 次全红；16 导出 / 5 依赖 / 各自导入函数集全部一致；
  i686 `949,940 B`（+3,641）。
- 真机待验：一次 Shift 后 `拼:开`→`拼:关` 与托盘「中」不变需同框确认。

### 宿主冒烟（X2-b）：一段固定键序 + 一列精确的宿主调用

- **新增 `tests/host_smoke_test.cpp`**（463 行，`ctest` 15/15）：`zhongguo` → `2` → `Backspace`×2 →
  `Esc` → 单独 `Shift`，断言 **13 条 host 调用逐字相等** + commit 序列 `["钟"]` + 引擎收到的操作序列。
- 假件：`ScriptedEngine`（固定返回队列，空了即失败）、`RecordingHost`（一次调用一行日志）、
  `SmokeScheme`（字母表 + 默认选字/翻页键）。驱动层复刻 IMM32 的门：`wants_key` 为真才调
  `handle_key`。
- 断言对象是**调用序列**而不是最终文本——X2-c/X2-d 的验收都是"序列不变"，只比文本查不出
  重排 `show`/`hide`、漏刷 UI、多调引擎这三类回归。
- `check_m1` 68/68：新增"冒烟文件必须存在**且**注册进 ctest"（只存在不注册的测试看起来像覆盖）。
- 真机 Shift 复测：`abc` 直接上屏、`ni` 候选回来 → 切换生效；托盘红底「中」是 ReactOS 通知区的
  布局指示器，不反映 `fOpen`，已记入待排期（让我们自己的工具栏显示 拼/En）。

### 键策略剥离（X2-a）：最忙的 90 行第一次有宿主测试

- **新增 `framework/src/host_key.cpp`（进 `imfw-engine`）**：`handle_key()` 与 `wants_key()`
  是原 `process_key()` / `should_consume_key()` 的逐条搬运。判定不再需要 `<windows.h>`。
- **新增 `framework/include/imfw/host.hpp`**：`KeyEvent`（vk / ch / kind / key_up / ctrl / alt）、
  `KeyResult`（commit）、`Host`（open 读写、show/hide、can_render、2 个默认空诊断钩子）。
  `KeyKind` 让策略里不出现任何 `VK_*` 常量。
- **`ImeSessionState` 搬到 `framework/include/imfw/session_state.hpp`**：它本来就全是可移植
  类型，之前只是和 IMM32 的 store 放在一起，那也是键路径无法脱离 windows.h 的原因之一。
- **`imm_entry.cpp` 只剩适配器**（783 → 609 行）：`key_event_from()` 做 vk→kind/字符翻译，
  `Win32Host` 实现 `Host`。
- **行为不变**：分支与副作用顺序逐条核对，无判定改动；判据换成"PE 对外面不变 + 宿主测试"，
  因为代码换 TU 后字节必然变（拼音版 946,299 B，+4,207 B；16 导出、导入函数表逐个一致）。
- **测试**：新增 `tests/host_key_test.cpp`（35 条断言，假 engine + 假 host），`ctest` **14/14**；
  `check_m1` **67/67**；两次踩到"关键词出现即通过"的假绿，断言改为钉完整调用形式。

### 注入缝（X2-a0）：scheme 可替换，框架不动

- **`imfw::SchemeRegistry`**（`framework/include/imfw/scheme_registry.hpp`）：把 `scheme_instance()`
  拆成"创建"（装配件提供 `scheme_factory()`）与"查询"（框架 `get()`，结果缓存一次）两半；
  `install(Scheme*)` 是测试/宿主的注入点。`scheme_instance()` 保留为别名，**10 处调用点零改动**。
  解决审计 A-P0-2：此前换一个方案只能换掉框架所依赖的那个符号再重编。
- **toy scheme**（`samples/toy-scheme/`）：字母表 `a-z`、每键两个候选、零数据纯 ASCII，
  作注入缝首个消费者；`-DROSPINYIN_SCHEME=toy` 编出玩具版 `rospinyin.ime`（870,475 B，
  不可发布，configure 时有明确警告）。该提交 `framework/` 零 diff——G4 判据提前达成，
  X3 只剩打包与文档。
- **门禁**：`check_m1` 65 项（装配件断言改为查定义形式，堵掉"注释里出现关键词即通过"的假绿）；
  CI 语法检查覆盖玩具路径，MinGW job 多一个 `build-toy` 并断言两个 `.ime` 不是同一份字节。
  `ctest` 13/13（新增 `scheme-registry`、`toy-scheme`）。

### 架构审计复核与 X 计划扩容

- **新增 `docs/reviews/ARCH-AUDIT-VERIFICATION.md`**：对一份外部架构审计（13 条问题结论）逐条回代码取证。
  结论：**11 条成立、1 条成立但证据过期、1 条已过时**；量化数字 12 项中 10 项精确。
  补了审计漏掉的 3 条（两条激活路径顺序相反、计划自身基线数字失真、`ime/version.h` 是第四处版本声明）。
- **X1–X4 计划扩容**（`docs/plan/PLAN-MODULARIZATION.md`）：
  - X2 从"剥键路由 + 冒烟"扩为"安全网 + 构建收敛"，工期 3-5 天 → **6-8 天**；
    新增注入缝（`SchemeRegistry` + toy scheme，原属 X3）、拆 god object、库目标、presets、版本收敛。
  - X3 从"造玩具方案"降级为"接入演练 + per-instance 隔离"。
  - ROADMAP 的 X 表与总工期同步：v0.1 推迟约 1 周 → **约 1.5 周**。
- **一处顺序修正（本次唯一改了审计建议顺序的地方）**：审计把"Win32 层独立成库 + presets"排在
  冒烟测试之后是正确的，原计划想放 X1 之后是错的——**X1 的验收判据是字节比对，拆库后字节必然变，
  判据当场失效**，无法区分"拆库导致的差异"和"拆错了导致的差异"。所以构建收敛必须在 `host_smoke`
  之后，用行为判据验收。

### 结构调整（X1 目录归位）

- **`framework/host/` → `framework/platform/win32/imm32/`（9 个文件）、`framework/ui/` →
  `framework/platform/win32/ui/`（10 个文件）**，全部 `git mv`，零逻辑变更。
  目录名陈述事实（这段代码就是 Win32 + IMM32），不用 `frontend/` 那种预设抽象的名字。
- **验收看字节而非测试**：同工具链干净构建前后各一次，`.ime` 940,888 B 完全一致，
  仅 3 个区域不同（`0x88` 时间戳、`0xD8` 校验和、`0x60A04` 导出表时间戳），全部是链接器戳。
- **新增 `tools/compare_ime.py`**：按 PE 结构（数据目录 #0 / #6）定位链接器戳字段，
  不硬编码偏移；判据本身做过 4 次变异验证（翻转代码字节、截断、交换源文件顺序、不动）。
- **修正计划中的技术错误**：`0x60A04` 不是"调试字段"——该镜像没有调试目录，
  它是 `IMAGE_EXPORT_DIRECTORY.TimeDateStamp`。
- **修复一处长期失效的 CI 门禁**：`IME source syntax check` 因 `tools/syntax_stub/imm.h`
  缺 `HIMC`/`ImmGetOpenStatus`/`ImmSetOpenStatus` 一直失败，挡住 engine-smoke 后续全部步骤，
  并使 `rospinyin-m2-data` artifact 从未上传、`publish-artifacts-branch` 永久连带失败。
  补全 stub 后恢复（`windows.h` 补 15 个符号），并做了 3 次变异验证。

### 仓库结构

- **文档全部收进 `docs/`，根目录只留 `README.md`（+ `LICENSE`、`NOTICE.txt`、`CMakeLists.txt`）**。
  16 个根目录 `.md` 按「谁在什么时候读」分为六类：`plan/`（每轮必读的活文档）、
  `handover/`（交接与变更登记）、`milestones/`（阶段报告，只读）、`font/`（字体与字形专题）、
  `reviews/`（跨切面评估）、`releases/`（发版说明，机器读取）。`docs/README.md` 是唯一索引。
- **新增门禁 `tools/check_docs.py`（7 项，已接入 CI）**：根目录不得有多余 markdown、
  `docs/` 下每个文档必须登记在索引里、相对链接必须可解析、链接不得闭合两次、
  `docs/releases/` 命名须匹配 tag、子系统文档须贴着代码。整理若没有门禁，
  下一份报告还会落在 `README.md` 旁边。
- 引用同步：14 条 markdown 链接按新位置重算、59 处行内路径加 `docs/` 前缀；
  `tools/check_release_version.py` 与 CI 的发布 job 改读 `docs/releases/<tag>.md`。

### 发版工程化

- **版本号收敛到单一真相源**：新增 `ime/version.h`，`rospinyin_res.rc` 从这里展开
  `FILEVERSION` / `FileVersion` / `ProductVersion`。此前资源里写死 `0, 0, 1, 0` 与
  `"alpha0.0.1"`，**`alpha0.0.2` 发布后仍如此**——文件属性显示的版本是错的，
  而"发布的二进制 = 该 tag 的构建"当时无法检验。
- **新增版本门禁** `tools/check_release_version.py <tag> [--binary X.ime]`：校验
  tag ↔ `ime/version.h` ↔ **产物 PE 里的 `FILEVERSION`**（自行解析 `.rsrc`，无第三方依赖），
  并要求 `docs/releases/<tag>.md` 已存在。反例实测可判红（拿 rc 仍是 0.0.1 的旧产物跑，
  输出 `binary=0.0.1.0`）。
- **CI 由 tag 触发发版**（P2-1）：`on.push.tags: ['alpha*','beta*']` → `release` job，
  `needs: [engine-smoke, windows-build]`，用**本次运行的产物**发布 `.ime` / `.dat` /
  `install-rospinyin.exe` / `INFO.txt` / `SHA256SUMS.txt`，发布后把资产下载回来
  `sha256sum -c`。手工上传不再是路径；`tools/make_release.py` 降为兜底。
- 顺带核实：按 CI 步骤本地装配一遍，`.dat` 的 sha256 与 `alpha0.0.2` 已发布资产一致，
  说明 CI 从固定源编译的数据表可重现发布物。

### 门禁

- **契约断言从「黑名单扫词」改为「调用白名单」**（`tools/check_m1.py`，59 → 63 项）：
  旧的 `installer never writes Preload` 扫的是源码里的 `"Preload"` 一词，
  而写注册表的是平台；把写入写成 `L"Keyboard Layout\\Pre" L"load"` 即可绕过（实测旧断言 0 命中）。
  现改为 5 条结构断言：`DLL_PROCESS_ATTACH` 调用名白名单、类注册只调 `RegisterClassExW`、
  `ImeInquire` 有拒绝分支、安装器注册表写入限于两键白名单、注册表路径必须由
  `kKeyboardLayouts` 拼出。每条都做过反向验证（改坏必 FAIL）。

### 前提变更（文档层面，不改运行时代码）

- **M0 那次 `0xF4` 的归因被推翻，红线 #2 降级为部署偏好**。立项时把崩溃归因为
  「`Preload` 预载第三方 IME ⇒ 确定性蓝屏」，并据此立下「绝不写 `Preload` / 绝不注册为默认 IME」。
  2026-11-18 的取证否掉了这个归因：
  - 写 `Preload` 的是平台 `ImmInstallIMEW` 内部的 `Imm32WriteImeLayout()`（ReactOS 0.4.16
    `win32ss/user/imm32/install.c`），**没有 opt-out**，我们写不写不由自己决定；
  - 登录路径的加载者是 `userinit.exe`（`SetUserSettings → UpdatePerUserSystemParameters →
    IntLoadPreloadKeyboardLayouts → LoadKeyboardLayoutW`），不是 csrss；
  - 两组对照（各 3 次启动，均用本版发布的 940,601 二进制）：**N 组**（在 `Preload` 里但非首项）3/3 活、
    **N′ 组**（首项 = 默认输入语言，走 `KLF_ACTIVATE → ImeSelect`）3/3 活。
  → 风险在「登录路径上执行可能失败的初始化」，不在「被 `Preload` 载入」。`DllMain` 零工作与
  `ImeInquire` 可拒绝两条**保留**，理由改为健壮性（对 liberNT / x64 / 别的 imm32 实现仍必要）。
- 受影响文档：`README.md`、`M0-REPORT.md`、`M1-REPORT.md`、`HANDOVER.md`（新增 §11.18）、
  `ROADMAP.md`、`PLAN.md`、`libernt/PORTABILITY-ASSESSMENT.md`、
  `PORTABILITY-UPSTREAM-64BIT-ARM64.md`（P1-D → P2-D）、`m0/vm/PRELOAD-PROBE.md`。
  全部为**事实与理由的修订**，不涉及二进制行为；`alpha0.0.2` 发布物不受影响。

### 修复

- **首次切到本 IME 是「关闭」态**（真机反馈 U1）：`AlignOpenStatus` 原先只挂在
  `ImeSetActiveContext` 上，而 ReactOS 首次切到一个布局时可能只调 `ImeSelect`，
  于是 `ImmGetOpenStatus()` 停在 `IntCreateInputContext` 设的 `FALSE`——托盘显示 `EN`，
  用户按第一下 Shift 是「打开输入法」而不是「切中英」。现在两个入口都对齐，
  仍由 `open_initialized` 保证整会话只对齐一次（否则会覆盖用户自己按过的 Shift）。
- **工具条被任务栏挡住**（真机反馈 U3）：`dock_bottom_right()` 用 `SM_CYSCREEN` 全屏高定位，
  600 高的屏上算出 `y=570`，而任务栏顶边约 572，22px 的工具条有 20px 落在任务栏之下。
  改用 `SPI_GETWORKAREA` 的工作区矩形；任务栏在侧边或自动隐藏时也随之正确。
  附带一条实测结论：**ReactOS 上 `HWND_TOPMOST` 不保证压过任务栏**（任务栏同为 topmost
  且 z 序更高），所以只能靠避让，不能靠层级。

### 工程

- `tools/check_m1.py` 新增 2 条契约断言（57 → 59）：`AlignOpenStatus` 必须同时挂在
  `ImeSelect` 与 `ImeSetActiveContext`；工具条必须用 `SPI_GETWORKAREA` 而非 `SM_CYSCREEN` 定位。
- `m0/vm/probe-n.sh` 补上文档承诺却一直没实现的 `stop` 子命令（走 HMP `quit` 优雅退出，
  不 `kill`，保证覆盖层 flush），并把 `clean` 作为 `rollback` 的别名。

---

## alpha0.0.2 — 2026-11-18

第二个公开版本。M2 验收单 **15/15 通过**；真机三轮反馈（多音节只转首字、Shift 不切中英、
候选窗跟着上屏的字往右爬、汉字字距重叠）全部修掉。

### 下载

| 文件 | 大小 | sha256 |
|---|---|---|
| `rospinyin.ime` | 940,601 B | `c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa` |
| `rospinyin.dat` | 902,776 B | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
| `SHA256SUMS.txt` | 160 B | — |

> `rospinyin.ime` 由本 tag 的源码以 `ROSPINYIN_TRACE=OFF` 构建（MinGW i686，Release）。
> 真机验收用的是同源码的 `ROSPINYIN_TRACE=ON` 构建（967,775 B）——两者导出表（16 个 IMM32 接口）、
> 导入 DLL、功能路径都相同，只差一份写到 `C:\rospinyin-trace.log` 的诊断日志。

### 新增

- **`tools/coverage_report.py`**：算出 ROADMAP 里那条一直没人算的 M2 退出标准——
  **前 500 高频字首页覆盖率**。直接读 `.dat`，按引擎真实排序（`1000000 - rank`，
  同音节内 rank 升序）计算，不重新实现排序。实测 **96.8%（484/500，达标 ≥95%）**，
  但**首选率仅 46.2%**（前 100 是 83%、前 1000 首页 93.5%）——"空格即出"只对最常用的一百来字成立。
  已接进 CI 作回归门限（`--fail-under 95`）。
- **`docs/milestones/m2/M2-EXIT-GAP.md`**：M2 距离准出的逐条对照。结论是**准出成立**，
  另列 4 条记录缺口（验收截图未归档、3.9–3.11 无逐页图、版本号漂移、dat 两套数字）与 3 条已知限制。
- **`FONT-PLAN-REVIEW.md`**：两份字形方案的评估。`MINIMAL-FONT-SET-PLAN.md`（实测 100%）
  取代 `FULL-GLYPH-COVERAGE-PLAN.md`（推算 ≥99%）作为实施依据，后者的"非 BMP 退路"与
  "字形风格论证"被吸收。新增里程碑 **F 字形与字体**（F0 GDI 探针 → D-F1 风格决策 →
  F1/F2 绘制改造 → F3 字体包 → F4 过滤三态化），已进 `ROADMAP.md` / `PLAN.md`。

- **Shift 切中英**：单独按一下 Shift（不夹带其它键）在中/英之间切换；英文态下所有键归应用。
  `Shift+A` 仍是大写 A，不会切成英文（lone-Shift 锁存）。
- `uigeom` 诊断行：`comp=/cand=` 实际几何 + `anchor=` + `caret=0/1`（锚点是否退化成鼠标位置）。

### 修复

- **`zhongguo` 只剩 guo 的候选**：候选取 `syllables_.back()`（从右往左），且上屏后清空整个组合串，
  等于把前面打的丢掉。改为**左到右逐音节上屏**：候选给第一个未转换音节，选中后只吃掉首音节，
  余下（`guo`）留在组合串里继续转 → 连按空格得到「中国」。
- **组合窗被裁到第一个音节的宽度**：`CompositionWindow::update()` 量了文本、写了缓存的宽高，
  却从不 `SetWindowPos`。几何收敛到唯一出口 `place_windows()` → `UiWindow::apply_geometry()`，
  位置尺寸一起下发、无变化整条跳过；组合窗首音节黑体+黑下划线、余下音节灰体+浅灰下划线。
- **中英状态被焦点切换重置**：`AlignOpenStatus()` 从"每次激活"改成"每会话一次"。
- **候选窗跟着上屏的字往右爬**：锚点原来每次 `UiShow` 都重取插入符，上屏一个字插入符就右移。
  改为**一次组合只取一次锚点**；x 的右边界收口也从"按实测宽度"改成固定预留 300，
  公式不再含任何窗口宽度，打字和翻页都不会再改 x。
- imhost 编译缺 `-static` 导致 guest 端缺 `libgcc_s_dw2-1.dll` 起不来（工具侧，不在发布物内）。
- imhost 输入框汉字重叠（字距"过窄"）：真因是选到的字体没有中文字形、步进仍按拉丁宽度算
  （`adv≈0.5em`，字压字），不是字号问题；EDIT 也没有字间距 API。
  改为**按实测步进选字体**（`adv >= height*0.85`，候选逐个量）→ 命中 `Droid Sans Fallback`，
  `adv=17=1em`；输入框字号 19→17、label 12→14；找不到时打 `font FAILED` 而不是静默退化。
  另留兜底开关 `imhost.exe font:<face> size:<10–40>`（工具侧，不在发布物内）。

### 验收

- **M2 验收单 15/15 通过（2026-11-18）**：真机手打一轮 433 键，入口层与窗口层逐条对齐、程序注入 0 次；
  `nv/lv/lve/nve`、跨页选字、Backspace/Esc、100+ 字连打、lone-Shift 中英往返均有日志实据。
  逐项实测见 `docs/milestones/m2/M2-ACCEPTANCE.md` §3，还原方法见 `HANDOVER.md` §11.16。

---

## alpha0.0.1 — 2026-11-17

首个公开版本。M2「单字拼音引擎」在 ReactOS 0.4.16 i386 上以**真实键盘手打**（非程序注入）端到端通过。

### 可用能力

- `ImmInstallIMEW` 安装为独立布局「中文(简体) - RosPinyin」，`Alt+Shift` 可切换，托盘显示「中」图标
- 组合串输入：连续音节、音节下划线、`'` 硬分隔（`xi'an` → `[xi][an]`）
- 候选窗：每页 9 条，`-`/`,` 上一页、`=`/`.` 下一页、页码 `n/m`
- 选字上屏：空格选首选，`1`–`9` 选当前页候选，翻页后数字按当前页解释
- 编辑键：Backspace 回退组合串，Esc 取消不上屏
- `v` 键输入 ü：`nv`→女、`lv`→律、`lve`→略、`nve`→虐
- 数据表 44,355 字 / 419 音节（hanziDB MIT 主源 + Unicode Unihan 辅源，字节级可复现）；候选默认只呈现有字频的 9,898 字

### 修复

- **IME 从不被加载**（致命）：UI 服务窗口类名 `RosPinyinUIServer` 长 17 字符，被 `imm32` 的 `szUIClass[16]` 静默截断成 `RosPinyinUIServe`，随后按截断名 `GetClassInfoW` 失败 → 卸载 DLL 并置 `fLoadFlag=1`。类名收敛为 `RosPinyinUISrv`（≤15 字符，附 `static_assert` 与门禁检查）。
- **多字 / 丢字**：`co_IntProcessKeyboardMessage` 对 `WM_KEYUP` 也置 `bKeyUpDown`，而 `ImmProcessKey` 不看 transition 位、`ImeToAsciiEx` 又拿不到 lParam，导致每个键被喂两次。`ImeProcessKey` 现在对 key-up 一律返回 `FALSE`。
- **简体「谁」打不出来**：Unihan 里 U+8C01 `kMandarin` 只有 `shéi`，语料高频的 `shuí` 不在全集内被丢弃，`shui` 音节被繁体「誰」占据。`mkdat` 改为把 `kHanyuPinlu` 的音作为**次读音**保留；`CodeTable::load_dat` 增加可选回调，为次读音建别名索引（同一字可回答多个音节且不重复出字）。
- **候选窗出现空心方框**：44,355 条数据中 34,457 条是 Unihan 独有生僻字，其中 68% 在随包字体子集（`U+4E00–U+9FA5`）之外。`PinyinSession` 默认过滤无字频字（该音节过滤后为空则回退全表），可用 `set_show_rare(true)` 恢复。
- 修正 `rospinyin_res.rc` 版本资源与实际版本不符的问题，版本号统一为 `alpha0.0.1`。

### 效果对比（真实引擎 + 真实数据）

| 输入 | 修复前 | 修复后 |
|---|---|---|
| `shui` | 34 条，首选「水」，简体「谁」不存在，20 个空心框 | **7 条**：水 谁 税 睡 稅 誰 脽，简体「谁」第 2 位，0 个空心框 |
| `zhong` | 108 条 | 26 条 |
| `shi` | 340 条 | 95 条 |

### 已知限制

- 只支持**单字**上屏，没有词组与词频（M4）；`zhongguo` 目前只能逐字选
- 没有中文标点、全角/半角转换、Shift 临时英文（M3）
- 语言栏菜单项（属性 / 关于）为空（M6）
- `ImmGetOpenStatus` 在 ReactOS 上的读数异常（`-1` / `0` 混杂），Shift 切英文后的按键行为尚未验证
- 组合串窗口位置在每次 `feed` 都会重置一次（`ImmSetCompositionWindow` 调用偏多），快速输入时可能看到轻微跳动
- 无安装器，当前需手工复制 `.ime` / `.dat` 到 `C:\ReactOS\system32\` 并用 `ImmInstallIMEW` 注册
- 仅在 ReactOS 0.4.16 i386（QEMU）上验证；未在真实硬件、Windows、x64 上测试

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 53/53、`xingma/`（形码产品线）零改动。

---

## 未发布（`alpha0.0.2` 候选）— 2026-11-17

M2 收口改动。**不改数据格式、不改 IMM32 ABI**，`alpha0.0.1` 的数据表可直接沿用。

### 修复

- **`ImmGetOpenStatus` 读数"异常"**：`-1` 是宿主程序的 `g_himc == NULL` 哨兵（恰等于 `TRUE`），
  `0` 是 ReactOS 建输入上下文时的出厂值（`imm32_imm.c` `pIC->fOpen = FALSE`，激活路径从不回置）。
  现在 `ImeSetActiveContext` 激活时把 OS 侧状态对齐为 `TRUE`，trace 记录 `openalign before=/after=`；
  键路由仍不依赖该值（契约闸门已加：`should_consume_key` 内不得出现 `OpenStatus`）。
- **组合串窗口被裁切**：窗口量完文本只更新了缓存宽高，从未 `SetWindowPos`，
  `zhongguo` 仍按 `zhong` 的宽度绘制。几何改为单一 owner（`place_windows()` → `apply_geometry()`）。
- **快速输入时窗口跳动**：每次引擎更新都 `InvalidateRect + ShowWindow + SetWindowPos`。
  现在只有几何真正变化才下发、只有窗口隐藏时才 `ShowWindow`、只有内容变化才重绘。
  顺带查清：`IMN_SETCOMPOSITIONWINDOW` 每键 1–3 次是 ReactOS 的 `EDIT_SetCaretPos` 发的
  （`edit.c`，仅 `__REACTOS__` 分支），不是引擎行为，IME 侧无法减少。
- **安装器只拷贝 `.ime`**：装完缺 `rospinyin.dat` → 候选窗空白。`/step1` 现在两份都拷并校验字节数；
  文件被映射（`ERROR_SHARING_VIOLATION`）时先改名 `.old` 再写，`/status` 报告两份，`/u` 两份都删。
- **扩展 A 区字符显示为方框**：随包字体子集到 `U+9FA5` 为止，`shi` 里有一个**有字频**的扩展 A 字。
  候选进分页器前按 `GetGlyphIndicesW` 过滤（按码点缓存）；探针不可用或过半被剔时整体放行，
  避免字体异常时把候选清空。

### 工程

- `tools/check_m1.py`：53 → **57** 项，新增 4 条 M2 不变量（组合窗几何 owner、重绘条件化、
  安装器两份 payload、open 状态不得作为门禁）。
- 新增 `docs/milestones/m2/M2-REPORT.md`；`docs/milestones/m2/M2-REMAINING.md` 更新为"仅剩真机验收单"。
- trace 新增 `wmove=`（真正改变窗口几何的次数）与 `wrep=`（重绘请求次数）、
  `cand-filter`（字形过滤剔除数）。

### 已知限制（未变）

- 单字上屏；无中文标点/全半角/Shift 临时英文（M3）
- 字体子集不含扩展 A 区，该区候选被过滤（不出现，而不是显示方框）
- 安装器仍是命令行工具，ReactOS 上需 `/step1`…`/step4` 分步 + 重启

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 57/57、`xingma/` 零改动。

---

## 版本号约定

- `alpha0.0.x`：M2 阶段，单字可用，数据与接口可能随时调整
- `0.1.0`：M3 完成（标点、全半角、临时英文）
- `0.2.0`：M4 完成（词组与词频）
- `1.0.0`：达到 README §3 的 v0.1 Definition of Done
