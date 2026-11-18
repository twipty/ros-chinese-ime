# RosPinyin → ReactOS 上游 64 位 / ARM64 兼容性评估

> 状态：**静态评估完成，实机验证未做**。本文基于 ReactOS 官方 wiki / JIRA / 官方发布公告（一手来源）、主流技术报道（二手佐证），以及本仓库代码与契约的逐行核对。
> 范围说明：本文评估的两个对象是 **(1) ReactOS 官方主线的后续 64 位实现（AMD64/x64）** 与 **(2) ReactOS 官方主线的 ARM64（AArch64）移植**。这与已有的 [`libernt/PORTABILITY-ASSESSMENT.md`](libernt/PORTABILITY-ASSESSMENT.md) 不同——libernt 是**自研内核分支**，其 64 位成熟度、WoW64/FEX 能力、ImmInstallIMEW 行为均与上游不同，结论不可互换。
> 评估日期：2026-11-17。本文不改动任何构建目标或契约。

---

## 0. 结论速览

| 目标 | 判定 | 一句话依据 |
|---|---|---|
| **① ReactOS 上游后续 64 位（AMD64）** | **中等偏高可行，但非「重新构建即可」** | 代码层 90% 可复用（纯 C++、指针已 64 位化、数据格式架构无关）；但有 **1 个已用 64 位 g++ 复现的编译失败断言**、**0 条 64 位构建路径**、**0 例上游 x64 上的 IME 注册/加载实测**。 |
| **② ReactOS 上游 ARM64** | **代码层中等偏高可移植，平台层当前不可验证；不建议纳入近期支持矩阵** | 上游 ARM64 于 2026-05 刚「启动到桌面」，**没有任何可用应用、没有 Win32 程序跑通、没有 IME 子系统证据**（CORE-18200 仍 Open / Unresolved / 无 Fix Version）。瓶颈在上游平台，不在本项目代码。 |

**最重要的三个认知：**

1. **上游 64 位 ≠ libernt 64 位。** 上游 AMD64 移植**没有 WoW64**（官方 wiki 原文：*WoW64 is not implemented yet, so you cannot use 32-bit programs*）。因此在目标①里，libernt 评估中的「WOW64 注册表视图 / System32 重定向」风险**基本不适用**（但若走 Windows 参照机或未来上游补 WoW64，仍会回来）——但换来了更硬的一条铁律：**进程全是 64 位，必须提供原生 64 位 `rospinyin.ime`，32 位版本一行都用不上**。
2. **ARM64 的阻塞是「宿主还没有应用」，不是「本项目不可移植」。** 上游 ARM64 目前只能证明内核+桌面起来，连记事本都跑不了，因此**不存在可加载输入法 DLL 的用户态环境**；在这种平台上讨论 IME 兼容性只能停留在 ABI/二进制层。
3. **现有 CI 从未覆盖 64 位真正会走的编译路径。** Linux 语法检查不定义 `_WIN32`（`tools/syntax_stub/windows.h:60` 只 `#define WINAPI`），因此 `imm_entry.cpp:226` 的 `offsetof` 断言被跳过；已用 64 位 g++ 从 stdin 实测该断言为 `(8 == 4)`、**必然失败**。更需注意：既有 libernt 文档 §3.2 把这条断言列进了「已具备的 64 位意识」，**属误判**，不能照抄。

**分层判定（两项并列）：**

| 层 | 内容 | ①上游 AMD64 | ②上游 ARM64 |
|---|---|---|---|
| ① 引擎 / 词库数据 / 编码 | 纯 C++、dat 全 uint32 小端 | 可直接复用 | 可直接复用（ARM64 亦小端） |
| ② IME 核心 / UI 窗口 | 公开 API、指针已 64 位化 | 可复用，**1 处硬编译阻塞** | 可复用，**同 1 处 + MSVC 弱符号问题** |
| ③ IMM32 16 导出 ABI | 手写 ABI、仅部分断言覆盖 | 结构吻合·零实测 | 结构吻合·零实测·调用约定待核 |
| ④ 安装器 / 注册链路 | HKL 32 位语义、Preload 红线 | 需 64 位原生重写·红线待重验 | 同左·且平台无从验证 |
| ⑤ 构建工具链 / 位宽 | 仅 i686 MinGW + 静态 CRT | **主要阻塞**：无 x64 目标 | **主要阻塞**：无 ARM64 目标 + 工具链未定 |
| ⑥ 实机验证环境 | 全部绑 32 位 ReactOS | 需新建 x64 环境 | **当前不存在可用环境** |

---

## 1. 上游现状核查（一手来源）

### 1.1 「ReactOS 官方演进路线的后续 64 位实现」= AMD64 移植

官方 wiki 的 `ReactOS ports` 导航把 AMD64 列为 **In active development**（IA-32 与 AMD64 两档），ARM64 列为 **In slow development**。[`reactos.org/wiki/ReactOS_ports`](https://reactos.org/wiki/ReactOS_ports)

[`reactos.org/wiki/AMD64`](https://reactos.org/wiki/AMD64) 的 Status 段（页面最新修订 2026-02-14）原文要点：

- 代码**已完整合并进 master**（不再是 bringup 分支）；
- 可用 **VS 2015–2022 的 64 位开发环境**编译；
- 也能用 x64 GCC/Clang 编译，但 *"it will probably not boot"*；
- master 在 **VirtualBox / KVM 上可启动到桌面**，需选 WS2003 x64 并开启 I/O APIC；
- buildbot 测试需注释掉部分测试才跑完，**WIP**；
- **WoW64 is not implemented yet, so you cannot use 32-bit programs**。

Completion Status（粗略估计）：freeldr 100%、hal 99%、ntoskrnl/ke 99%、mm 95%、rtl 95%、crt 99%——**这份清单全是内核侧，不含 win32k/user32/imm32**。即「64 位内核基本可用」不能自动推出「64 位 IMM32 输入法栈可用」。

官方 0.4.16 于 **2026-08-29** 发布（[官网首页](https://reactos.org/) / [发布公告](https://reactos.org/)），发布镜像仍以 x86 为主；主流报道亦指出「开发以 32 位 x86 优先，64 位来自独立的 AMD64 移植」（[BetaNews](https://betanews.com/article/reactos-free-open-source-windows-alternative)）。

> **对本项目意味着**：目标①是真实存在且在推进的上游路线，但**成熟度是"能启动到桌面的 alpha"**，不是可替代 32 位的生产平台。IME 所需的 IMM32 在 64 位上是否有任何实测，公开资料中**找不到一例**。

### 1.2 ARM64：2026-05 首次启动，仍属概念验证

一手事实：

- **[CORE-18200 "ReactOS ARM64 port bring up"](https://jira.reactos.org/browse/CORE-18200)**：Epic / **Status: Open** / **Resolution: Unresolved** / **Fix Version: None** / Priority: Major；创建 2022-05-25，最后更新 2025-07-04；GitHub CI builder 于 `0.4.15-dev-4608-g8b55d71` 加入。
- 官方仓库 wiki 页为 [`reactos.org/wiki/AArch64`](https://reactos.org/wiki/AArch64)（被 Anubis 反爬保护，正文未能直取；JIRA 描述与官方推文可交叉印证）。
- **2026-05-26** 官方账号宣布首次 ARM64 启动成功：贡献者 **Ahmed Arif（@sidihmeed）** 独立投入约 **8 个月**；构建标记 **0.4.15-arm64-exp**；要求 **UEFI + GICv2/v3 + ARMv8-A+**；在 **QEMU 与树莓派 5** 上启动到桌面。官方明确称为 **experimental / proof-of-concept**（[The Register](https://www.theregister.com/os-platforms/2026/05/28/reactos-brings-its-windows-nt-tribute-act-to-arm64/5247768)、[官方推文转述](https://joho-todai.com/en/reactos-arm64-first-boot-success)）。
- 二手报道一致指出：桌面截图**没有任何可用应用**，**没有 Win32 二进制被测试过**，驱动仅 UART / 简单 framebuffer / 基础存储，NTFS 写入不可靠，WoW64/x86 模拟缺失，无稳定版发布时间表（[LavX](https://news.lavx.hu/article/reactos-boots-on-arm64-a-milestone-for-the-open-source-windows-nt-clone)、[yukako](https://yukako.org/archive/20260527-476-reactos-achieves-experimental-arm64-boot.html)）。
- 官方 CPU 支持表中 AArch64  SoC 大量标注 **Not tested**；`ReactOS_ports` FAQ 明确：「目前在 32 位 Windows 程序跑在 ARM 版 ReactOS 上没有工作开展」。

> **对本项目意味着**：ARM64 上**没有可运行记事本的用户态环境**，因此无法做本项目 M2 验收单里的任何一项（组合串、候选窗、选字上屏）。虽然二进制层面的可移植性问题可以静态分析，但**任何"兼容"结论都无法被证伪或证实**。这是一个纯上游瓶颈。

### 1.3 IMM32 是可移植性的正面前提

CORE-11700（IMM32 总纲）记录主要 IMM32 函数已实现并进入内核态输入上下文阶段；[CORE-20142](https://jira.reactos.org/si/jira.issueviews:issue-html/CORE-20142/CORE-20142.html)（IME 菜单）已在 0.4.16 标记 Resolved/Fixed。IMM32 属于 **win32k/user32 用户态库**，而官方 AMD64 移植中「大部分代码在 amd64 与 i386 间共享」（wiki 原文），因此**没有理由认为 IMM32 在 64 位上被架构性排除**——但同样**没有证据表明它已被验证**。这是本项目风险评估里最大的"未知"，不是"已知障碍"。

---

## 2. 本项目平台耦合点核查

### 2.1 有利条件（解耦非常干净）

逐项复核确认（证据均为本地文件）：

- **零 ReactOS 私有头**：全库唯一系统头是 `<windows.h>`（`<imm.h>` 仅安装器、`<winsock2.h>` 仅 httpdl）。手写 ABI `framework/host/imm32_abi.h:5-13` 明确「不依赖 immdev.h」。
- **零内联汇编 / 零 PE 头操作 / 零硬编码地址 / 零 `#pragma pack`**（全仓 grep 无命中）。
- **数据格式架构无关**：`framework/include/imfw/dat_format.hpp:33-63` 全为 `uint32_t` 字段，三个 `static_assert` 锁定 52/20/16 字节；`framework/src/code_table.cpp:28-29` 用 `memcpy` 逐字段读，**不做结构体整体映射**，因而无对齐陷阱。注释虽写 *"target is 32-bit ReactOS x86"*（`:10`），但格式本身在小端 64 位上同样成立（ARM64 Windows 体系为小端）。
- **指针已按宽度中性处理**：
  - `framework/include/imfw/emit_text.hpp:28-29` 用 `uintptr_t` / `intptr_t`；`framework/src/emit_text.cpp:19,29,34` 用 `static_cast<std::uintptr_t>`；
  - `framework/ui/ui_window.cpp:49` 用 `intptr_t(-1)` 表示 `HWND_TOPMOST`，`:224` 用 `LONG_PTR`，`:250` 用 `uintptr_t` 回转；
  - `framework/host/imm_entry.cpp:440,463` 用 `reinterpret_cast<std::uintptr_t>(hIMC)`；
  - `framework/src/utf8.cpp:16` 用 `sizeof(wchar_t)==2` 编译期适配；
  - `framework/host/imm32_abi.h:39-43` 的 `TransMsg` 直接用 `WPARAM`/`LPARAM`，**会随目标自动变成 8 字节**——这是正确的写法。子代理已用 64 位 g++ 实测：`TransMsgSlot` 与 `TransMsg` 在 LP64 下均 `sizeof==24`、`wParam` 偏移 8，**与 DDK 一致**；`imm_entry.cpp:217-219` 的 `sizeof` 断言在 64 位仍成立（只有 `:226` 的 `offsetof` 断言有问题）。
- **导出名天然适配 64 位**：`ime/rospinyin.def:8-26` 用无修饰名导出 16 个函数。x64 与 ARM64 都只有统一调用约定、无 stdcall 修饰，因此 `.def` 在两种 64 位目标上**不需要改名**（这点比多数老 IME 省事）。

### 2.2 需要改造/核实的点（按优先级）

| # | 问题 | 证据（文件:行号） | 影响 |
|---|---|---|---|
| **A** | **`_WIN32` 守卫在 64 位下触发错误断言** | `framework/host/imm_entry.cpp:221-229`：`#if defined(_WIN32) \|\| defined(__i686__) …` 内断言 `offsetof(TransMsgList, TransMsg) == sizeof(UINT)`。`_WIN32` 在 x64/ARM64 MinGW 与 MSVC 下**同样定义**；64 位下 `TransMsg` 因 `WPARAM/LPARAM` 8 字节对齐从偏移 8 开始，而 `sizeof(UINT)==4`。**已用 64 位 g++ `-fsyntax-only` 实测断言为 `(8 == 4)`，编译硬失败**。代码注释自己也承认 "On LP64 hosts … the offset grows"，但守卫没排除 64 位。注意**运行时逻辑本身在 64 位是对的**（IMM32 与本项目同布局），坏的只是断言。 | **P0**：x64/ARM64 构建第一步就失败 |
| **B** | **零 64 位构建路径** | `.github/workflows/ci.yml:58-69` 仅 `MINGW32` + `mingw-w64-i686-*`；`:98-122` 的 PE 导入表断言（禁 UCRT/`InitializeCriticalSectionEx`）是 i686/ReactOS 0.4.16 特化；根 `CMakeLists.txt:9-10` 只有 `ROSPINYIN_IN_TREE` / build tests 两个选项，无架构选项；全仓无 `-m64`/x86_64 toolchain。 | **P0**：无原生 `.ime`；在上游 64 位（无 WoW64）上 **32 位 DLL 根本无法加载** |
| **C** | **IMM32 ABI 静态校验覆盖不足** | `imm32_abi.h:28-36` 的 `ImeInfo` 全 DWORD（架构无关，好）；`:84-121` 的 16 导出签名仅由编译器自查；唯一跨模块断言在 `imm_entry.cpp:217,226`，且 `:226` 受 A 项错误守卫。无对 `imm.h`/`immdev.h` 真头的 `static_assert(offsetof/sizeof)`。 | **P1**：x64/ARM64 下字段/签名偏差可能静默错 |
| **D** | **注册链路 64 位零实测 + M0 红线待重验** | `tools/install-rospinyin/install.cpp:42,50` 硬编码 `@%SystemRoot%\system32\...` 与 `GetSystemDirectoryW`；M0 在 0.4.16 上实测「一次全量注册 + Preload」→ **BSOD 0xF4**，故定下「不写 Preload / 分步注册」红线。上游 x64 上 `ImmInstallIMEW` 行为、是否存在同类崩溃**完全未知**。 | **P1**：可能重现登录期崩溃 |
| **E** | **工具链战略冲突** | 上游 AMD64 官方以 **MSVC x64** 为可用构建路径（GCC/Clang 版 "probably not boot"）；本项目 CI 用 MinGW 静态 CRT 策略，正是为了绕开 MSVC CRT 导入 `InitializeCriticalSectionEx`（ReactOS 默认导出版本 0x502 无此 API，`ci.yml:52-57`）。而 `pinyin/src/dat_source.cpp:12-14` 的 `RPY_WEAK` 在 MSVC 下为空宏，`framework/host/imm_entry.cpp:56-58` 的 `#pragma GCC diagnostic` 在 MSVC 下无效。 | **P1**：若走 MSVC 需重做 CRT/弱符号策略 |
| **F** | **陈旧注释/文档表述** | `imm32_abi.h:13`「all pointer-shaped parameters are 32-bit on our x86 target」；`dat_format.hpp:10`「target is 32-bit ReactOS x86」。均为注释，不影响运行，但会误导后续移植者。 | **P2**：非阻塞，随改造一并更新 |
| **G** | **字体链 ReactOS 特化** | `framework/ui/ui_window.cpp` 的 CJK 回退链（Droid Sans Fallback / Noto Sans SC / 微软雅黑 / 宋体）。有 `GetTextFaceW` 校验与最终兜底，降级安全。 | **P2**：非架构相关，可后置 |
| **H** | **验收环境全绑 32 位** | `m0/vm/common.sh:8,11,22` 写死 `qemu-system-i386` / `ReactOS-0.4.16-i386.iso` / `-machine pc`；`m2/M2-ACCEPTANCE.md:4,24,30-31` 全 ReactOS 0.4.16 + `C:\ReactOS\system32`；CI 只有 i686 job。ARM64 需换 `qemu-system-aarch64 -machine virt` + ARM64 镜像（仓库内无）。 | **P1**：需新建 x64 环境；②arm64 为 P0（环境不存在） |
| **I** | **`ROSPINYIN_TRACE` 构建实际无法编译** | `framework/host/imm_entry.cpp:507` 在 trace 分支引用 `HIMC` 与 `ImmGetOpenStatus`，但全仓无 `#include <imm.h>`（`imm32_abi.h:21-24` 刻意不引入；`tools/syntax_stub/imm.h` 也没有）。现有 CI 不开 trace 故漏掉。 | **P1**：64 位冒烟取证依赖 trace，会先卡在这里 |
| **J** | **trace 路径指针被当 32 位截断** | `framework/host/dllmain.cpp:80-81` 把 `HINSTANCE` 截到低 32 位且只打印 8 位 hex；`imm_entry.cpp:70` `TraceLine::h(unsigned long)` 形参只收 32 位，`:439-440`、`:462-463` 把 `uintptr_t(hIMC)` 强转 `unsigned long`。 | **P2**：仅 trace 构建日志失真，不影响功能，但会误导 64 位排错 |
| **K** | **`LPARAM` 32 位截断隐患** | `imm_entry.cpp:494` `(static_cast<unsigned long>(lParam) & kKeyUpBit) != 0`（`kKeyUpBit=0x80000000UL`，`:179`）；`:511` 的 `HIWORD(lParam)` 同理。LPARAM 在 64 位为 8 字节；key-up 位恰在 bit 31（低半内）故**当前功能正常**，属隐患而非现 bug。 | **P2**：保持低 32 位语义即可，建议显式掩码 |
| **L** | **MSVC 路径重复定义（比原先判断更具体）** | `ime/CMakeLists.txt:35,47` 同时链接 `pinyin/host/dat_source_win.cpp`（强 `load_system_table`）与 `rospinyin-engine`（含 `dat_source.cpp` 的弱 fallback）。MinGW 下弱符号被强符号覆盖没事；**MSVC 下 `RPY_WEAK` 为空宏 → 重复定义链接错误**。 | **P1**：若走 MSVC（含 MSVC ARM64）会直接链接失败 |
| **M** | **mkdat 写侧直接内存拷结构体、无 byteswap** | `tools/mkdat/mkdat.cpp:558-569` 用 `reinterpret_cast<const unsigned char*>` 直接写 `Header`/`CharEntry`/`SylEntry`，无 `htole32`。x64 与 ARM64 Windows/ReactOS 均为小端，**实际无碍**；但这是显式假设，不是自动适配。 | **P2**：信息性，仅在大端宿主重生成 dat 时才有问题 |

> 关于 **WOW64**：全仓唯一处理 WOW64 的代码在 `vendor/mzimeja-ref/ime_setup/Wow64.h` 等**只读参考树**里，项目自身代码零命中（安装器 `install.cpp:90-91,106,132,246` 全是默认视图）。在**上游 64 位被控环境**上这不是缺陷——上游无 WoW64，也就不需要注册表视图切换；若将来上游补 WoW64/ARM64EC，或安装器以 32 位身份跑在 x64 Windows 参照机上，`GetSystemDirectoryW` 返回 SysWOW64（`:50`、`pinyin/host/dat_source_win.cpp:84`）与硬编码 `%SystemRoot%\system32`（`:42`）的错配就会变成 P1。在 libernt（已有 WoW64/FEX）上它已经是 P1。

---

## 3. 分层可行性判定

```
① 引擎 / dat / UTF-8     [①x64 可直接复用 | ②arm64 可直接复用]   纯 C++ / uint32 小端 / memcpy 逐字段
② IME 核心 / UI          [①x64 可复用·1 处硬阻塞 | ②arm64 同 + MSVC]  A 项断言 + E 项工具链
③ IMM32 16 导出 ABI      [两目标均：结构吻合·零实测]              ImeInfo 全 DWORD；TransMsg 自动随宽
④ 安装器 / 注册链路      [两目标均：需原生重写·红线待验]          上游无 WoW64 → 反而更简单
⑤ 构建工具链 / 位宽      [两目标均：主要阻塞]                     无 x64 / 无 arm64 目标
⑥ 实机验证环境           [①x64 需新建 | ②arm64 当前不存在]         上游 arm64 无可用应用
```

**①上游 AMD64 总评 = 中等偏高。** ①②占代码主体且几乎零改造；A 项是一次明确的编译修复，B 项是一次明确的构建补齐；C/D/H 是确定要做的工程活与实测。**没有任何一层需要重写引擎或换 TSF**。真实不确定性集中在「上游 64 位的 IMM32 栈是否已可用」——这一点静态无法定论。

**②上游 ARM64 总评 = 代码层中等偏高、平台层不可验证。** 若仅看二进制可移植性，其改造面与 x64 几乎相同（同一 A 项断言、同一工具链补齐、额外的 MSVC 弱符号处理）。但**上游平台尚不能运行任何 Win32 应用**，这意味着第 ④⑥ 层不只是"待办"，而是"条件不具备"。**建议把它放在观察列表，而不是 v1.0 支持矩阵。**

---

## 4. 风险清单

| 级别 | 风险 | 适用范围 | 缓解 / 验证 |
|---|---|---|---|
| **P0-A** | `_WIN32` 守卫导致 offsetof 断言在 64 位编译失败（§2.2-A）；**64 位 g++ 已实测 `(8==4)`** | ①+② | 守卫改为 `#if defined(_WIN32) && !defined(_WIN64)` 或以 `sizeof(void*)==4` 判定；补 LP64 对应断言 `offsetof == 8`；**CI 加带 `_WIN32` 定义的 64 位语法检查**堵住盲区 |
| **P0-B** | 无原生 64 位 `.ime`（§2.2-B）；上游无 WoW64 → 32 位 DLL 无法加载 | ①+② | 新增交叉目标；先在宿主侧拿到可加载的 64 位产物 |
| **P0-G** | ②arm64 工具链 / 宿主 / 镜像三重缺失（§2.2-H）：无 aarch64 交叉配置，MSYS2 `windows-latest` 无 `aarch64-w64-mingw32` 包，`m0/vm/` 只有 i386 镜像与 machine type | ② | 观察期只做跨架构语法编译；实机等上游门禁 |
| **P1-C** | IMM32 16 导出 / ImeInfo / TransMsgList 在 64 位无完整断言（§2.2-C） | ①+② | 以目标平台真头补 `static_assert(offsetof/sizeof)`；x64 冒烟 |
| **P1-D** | 注册链路 `ImmInstallIMEW` / Preload 行为在上游 x64 未验证，可能重现 0xF4（§2.2-D） | ①+② | 阶段探针：分步调用 + 重启 + 切换压测；必要时手工写键绕开 |
| **P1-E** | 工具链：上游 AMD64 官方 MSVC 优先，与本项目 MinGW 静态 CRT 相冲；**MSVC 下弱符号变重复定义**（§2.2-E/L） | ①+②（②更甚） | 先定工具链；MSVC 路需处理 `RPY_WEAK` 重复定义 / `#pragma GCC diagnostic` / CRT 导入断言 |
| **P1-F** | 上游 ARM64 无可用用户态/应用，IME 无法验证（§1.2） | ② | 门禁：CORE-18200 达到「user-mode apps work」且有 IMM32 启用证据后再启动 |
| **P1-I** | `ROSPINYIN_TRACE` 构建缺 `<imm.h>` 实际无法编译（§2.2-I），卡住 64 位取证 | ①+② | 先修 include 或补齐 trace stub；把 trace 构建加入 CI 矩阵 |
| **P1-W** | WOW64 视图 / System32 错配：上游被控环境不触发，但 x64 Windows 参照机与未来 WoW64/ARM64EC 会触发（§2.2 尾注） | ①（条件性）+② | 安装器随 `.ime` 升位；用 `GetNativeSystemDirectoryW`；加 `/status` 自检 |
| **P2-J/K/M** | trace 指针截断、`LPARAM` 低 32 位假设、mkdat 无 byteswap（§2.2-J/K/M） | ①+② | 非阻塞；显式掩码 / 保持小端生成流程即可 |
| **P2-G** | 陈旧注释、字体链特化（§2.2-F/G） | ①+② | 随改造更新，不阻塞 |

---

## 5. 建议路径（分阶段 + 门禁）

### 目标①：ReactOS 上游 AMD64

| 阶段 | 目标 | 关键动作 | 完成门禁 |
|---|---|---|---|
| **0. 宿主侧排雷** | 消除确定阻塞 | 修 §2.2-A 的守卫与断言；把 `offsetof` 断言参数化（32 位 4 / 64 位 8）；补 16 导出签名的头文件级 `static_assert`；修 §2.2-I 的 trace include；**CI 语法检查增加 `_WIN32` 定义与 64 位 trace 构建**堵住盲区 | Linux `ctest` 全绿 + MinGW 头文件语法检查覆盖 x64 + trace 构建可编译 |
| **1. 64 位产物** | 打通构建 | 新增 `x86_64-w64-mingw32`（或既定 MSVC 方案）CMake/CI 目标；重审 PE 导入断言（64 位应允许更多 Vista+ API） | 产出 64 位 `rospinyin.ime`；在 x64 Windows 参照机上裸 `LoadLibrary`/`FreeLibrary` 无害 |
| **2. 上游 x64 探针** | 验证注册与加载 | 在**上游**（非 libernt）x64 构建上分步注册 + 重启 + 切换；观测是否重现 0xF4；记录 `ImmInstallIMEW` 是否写 Preload | 确认 ABI 能加载、登录路径不崩、可获得的 IMM32 功能边界 |
| **3. 双机验收** | 功能闭环 | x64 + i386 各跑 `m2/M2-ACCEPTANCE.md`；≥50 次切换压测 | 对齐既有验收单，新增 upstream-x64 章节与证据 |

> 顺序理由：阶段 0 是纯宿主侧、成本最低且必然要做；先把「编译失败」变成「可构建的产物」，再谈注册与实机。探针必须在**上游**做，因为 libernt 的 `ImmInstallIMEW` 已经证明与本项目红线冲突，其结论不可套用。

### 目标②：ReactOS 上游 ARM64

| 阶段 | 目标 | 关键动作 | 完成门禁 |
|---|---|---|---|
| **观察** | 保持可移植性，不投入实机 | 修共享的 A 项；让代码在 `aarch64-w64-mingw32` / clang `--target=aarch64-windows-msvc` 下**语法编译**通过；处理 `RPY_WEAK` 在 MSVC 下为空的问题 | 宿主侧跨架构语法检查加入 CI（无需运行） |
| **启动条件** | 等上游成熟 | 跟踪 CORE-18200 与 AArch64 wiki；确认出现「用户态 Win32 应用可运行」+「IMM32 已启用」的证据 | 门禁满足才进入阶段 1 |
| **1–3** | 复用目标①路径 | 待门禁满足后，按 x64 的 0→3 阶段重跑（工具链换 ARM64） | 同 x64 门禁 |

---

## 6. 与目标①之外的既有评估的关系

| 维度 | `libernt/PORTABILITY-ASSESSMENT.md`（旧） | 本文目标①（上游 AMD64） | 本文目标②（上游 ARM64） |
|---|---|---|---|
| 内核 | 自研 ntos；win32k/user/imm32 沿用 ReactOS | 上游内核；AMD64 完成度 ke/mm/rtl 95–99%，无 WoW64 | 上游内核刚启动，HAL/MM/中断未完备 |
| IMM32 | `imm32/install.c` 已逐行核对，明确写 Preload + 立即激活 | 行为未核；**很可能是同一份 ReactOS 代码**，但无实测 | 无证据 |
| WoW64 | 有 WoW64/FEX → 注册表视图/System32 是 P1 | **无 WoW64 → 该风险消失**，但必须原生 64 位 | 无 WoW64，且平台无应用 |
| 64 位成熟度 | amd64/arm64/riscv64 主打，有第三方 IME（Rime）成功先例 | 能启动到桌面，alpha，无 IME 先例 | 概念验证，无应用 |
| 本项目改造面 | 中高，方向明确 | 中高，方向明确但实测未知 | 代码层同左，平台层条件不具备 |

**两处需要纠正旧文档的判断：**
1. **§3.2「已具备的 64 位意识」把 `imm_entry.cpp:226` 的 `offsetof` 断言列为有利条件——这是误判。** 64 位 g++ 实测该断言失败（`(8==4)`），且 Linux CI 因不定义 `_WIN32` 从未跑到它。它应归入阻塞项（本文 §2.2-A / P0-A）。
2. **旧文档的 WOW64 视图风险（§3.3-B/C）在上游 64 位被控环境下不触发**，因为上游没有 WoW64；但其 System32/HKL 分析对 x64 Windows 参照机和未来的 ARM64EC/WoW64 仍有效（本文 §2.2 尾注 / P1-W）。

**结论：不要互相引用判定。** 可跨目标复用的是 §2 的代码层证据、§2.2-A 的编译修复与 §2.2-I 的 trace 修复。

---

## 7. 证据来源

**上游一手：**
- [ReactOS 官网首页](https://reactos.org/)（0.4.16 发布 2026-08-29）
- [ReactOS ports wiki](https://reactos.org/wiki/ReactOS_ports)（AMD64 = In active development；ARM64 = In slow development；FAQ）
- [AMD64 wiki](https://reactos.org/wiki/AMD64)（Status / WoW64 / 完成度表，最新修订 2026-02-14）
- [CORE-18200](https://jira.reactos.org/browse/CORE-18200)（ARM64 Epic，Open/Unresolved/无 Fix Version）
- [CORE-11700](https://jira.reactos.org/browse/CORE-11700)、[CORE-20142](https://jira.reactos.org/si/jira.issueviews:issue-html/CORE-20142/CORE-20142.html)（IMM32 栈状态）
- [Supported Hardware/CPU wiki](https://reactos.org/wiki/index.php?title=Supported_Hardware/CPU)（AArch64 SoC 状态）

**上游二手（仅作佐证）：**
- [The Register, 2026-05-28](https://www.theregister.com/os-platforms/2026/05/28/reactos-brings-its-windows-nt-tribute-act-to-arm64/5247768)
- [LavX News](https://news.lavx.hu/article/reactos-boots-on-arm64-a-milestone-for-the-open-source-windows-nt-clone)、[yukako](https://yukako.org/archive/20260527-476-reactos-achieves-experimental-arm64-boot.html)
- [BetaNews](https://betanews.com/article/reactos-free-open-source-windows-alternative)（0.4.16 与 32/64 位定位）

**本项目（本地核查）：**
`README.md`、`libernt/PORTABILITY-ASSESSMENT.md`、`.github/workflows/ci.yml`、`tools/syntax_stub/windows.h`、`CMakeLists.txt`、`ime/CMakeLists.txt`、`framework/host/imm32_abi.h`、`framework/host/imm_entry.cpp`、`framework/host/dllmain.cpp`、`framework/include/imfw/emit_text.hpp`、`framework/include/imfw/dat_format.hpp`、`framework/src/code_table.cpp`、`framework/src/emit_text.cpp`、`framework/src/utf8.cpp`、`framework/ui/ui_window.cpp`、`ime/rospinyin.def`、`pinyin/src/dat_source.cpp`、`pinyin/host/dat_source_win.cpp`、`tools/install-rospinyin/install.cpp`、`tools/mkdat/mkdat.cpp`、`m0/vm/common.sh`、`m2/M2-ACCEPTANCE.md`。

> 所有「吻合/可复用」结论均为静态核查；必须在目标平台上以实机注册、加载、切换测试收口。ARM64 目标在当前上游状态下**无法收口**。
