# RosPinyin → LiberNT 移植可行性评估

> 状态：**静态评估完成，实机验证未做**。本文基于对 libernt 主线源码树（`main-nt10`，2026-11-17 核查）、官方文档/新闻、一手用户报告，以及本仓库代码与契约检查的交叉核对。
> 前置背景见 [README.md](../README.md)、[M0-REPORT.md](../M0-REPORT.md)、[M1-REPORT.md](../M1-REPORT.md)。本文是独立评估文档，不改动任何构建目标或契约。
> 评估日期：2026-11-17。

---

## 0. 结论速览

**总评：中等偏高可行。** 但性质是「定向改造 + 双目标构建」，不是一键移植；也不是「换平台重写」。

- **最大利好**：liberNT 的 IMM32 栈**完整保留且系 ReactOS 原样继承**，且 `ImmInstallIMEW` 的资源校验字段与本项目 M1 已设置的版本资源**完全吻合**。RosPinyin 与 liberNT 在输入框架层面天然对口。
- **主要阻塞**：① 全仓只有 i686 构建路径；② 安装器的 32 位 HKL 序列化与 WOW64 缺失；③ M0 用 0xF4 蓝屏换来的「不写 Preload」红线与 liberNT `ImmInstallIMEW` 的内部行为正面冲突，必须重验。
- **最大有利底牌**：项目与 ReactOS 内部实现解耦极干净（零私有头、零硬编码 DLL 名、零内联汇编、指针已用 `uintptr_t`），引擎/数据/编码/核心 UI 层几乎可原样搬运。

**分层判定：**

| 层 | 内容 | 判定 | 依据摘要 |
|---|---|---|---|
| ① | 引擎 / 词库数据 / 编码 | 可直接复用 | 纯 C++、零私有头、dat 全 uint32 架构无关 |
| ② | IME 核心 / UI 窗口 | 可直接复用 | 仅 user32/gdi32 公开 API；指针已 64 位化；字体链微调 |
| ③ | IMM32 导出 ABI（16 导出） | 高度吻合 · 待实测 | liberNT imm32 原样继承 ReactOS；版本资源校验字段吻合 |
| ④ | 安装器 / 注册链路 | 需重点改造 | HKL 截断、WOW64 视图、Preload 红线冲突 |
| ⑤ | 构建工具链 / 位宽 | 主要阻塞 | 当前仅 i686，liberNT 主打 amd64/arm64 |
| ⑥ | 实机验证环境 | 需重建 | 现有 VM 验收全绑 32 位 ReactOS |

---

## 1. 评估背景

### 1.1 为什么要评估

RosPinyin 的目标平台写死为 ReactOS 0.4.16+（IMM32 架构）。LiberNT 是 ReactOS 的活跃分支，2026-11-15 宣布切换到**自研内核**，目标抬升到 Windows 11 / NT10 兼容，主打 64 位。这带来一个现实问题：本项目接下来该不该把 liberNT 纳入支持矩阵，成本多大、风险在哪。

### 1.2 LiberNT 是什么（与 RosPinyin 相关的事实）

来源：libernt 官网、GitHub `LiberNT/LiberNT` README、官方新闻与一手用户报告。

- **血统**：官方称其为「ReactOS 的 modified version」；当前开发分支 `main-nt10`；GPL-3.0，继承代码多为 GPL-2.0-or-later。
- **架构**：主打 amd64 / arm64 / riscv64（64-bit first），但**保留 i386**（32-bit x86）——README 明确列出 `./configure.sh -a i386 --gcc`，平台表中 i386 标注为「source build」；另有 WoW64 / FEX 用于跑 32 位 Windows 应用。
- **内核大换血（2026-11-15）**：自研 ntos 内核全面接管进程/线程、文件、设备、PnP、注册表、安全、电源、调度、调试器；内存管理器（NVS）、缓存管理器、图形栈（WDDM + DWM）、引导管理器此前已自研。
- **★ 关键边界**：官方原文明确——**「Most drivers, the window manager and the user-mode libraries are not in this step.」**（多数驱动、窗口管理器和用户态库不在本次替换范围）。即 **win32k / USER / GDI / imm32 等仍沿用 ReactOS 血统**——这是本评估成立的技术基石。
- **IMM32 与 TSF 共存**：源码树同时存在 `base/ctf/`（msctf / msctfime / cicero / ctfmon / msutb）与完整的 `imm32`（见 §2.1）；官方近期提交仍在修 MSCTF 的 composition 行为（如 Google Japanese Input 在 Firefox 地址栏的问题）。TSF 的在研**不等于** IMM32 被取代。
- **一手用户佐证**：第三方体验报告（blog.cloudflare88.eu.org，2026-10-18）称 liberNT「支援简体中文和正体中文，然而系统并未内建中文输入法，需要使用者自行安装」，作者安装了 Rime 输入法并可用。说明第三方 IME 在 liberNT 上有成功先例（该报告未说明 Rime 走 IMM 还是 TSF 路径，故仅作旁证）。

> 反直觉点：liberNT 的内核已是「自研」，但 IME 所依赖的 imm32/user32/win32k 恰好**不是**被替换的部分。不要因为「换了内核」就推断输入法栈也变了。

### 1.3 评估方法

1. 直接读取 libernt 主线源码目录树与关键文件（`imm32/CMakeLists.txt`、`imm32/install.c`），而非仅凭 README 或搜索摘要；
2. 逐字核对 libernt `ImmInstallIMEW` 的校验与写注册表行为，对照本项目安装器与版本资源；
3. 全量扫描本仓库的平台耦合点（构建、头/库依赖、内联汇编、packing、指针宽度、注册表视图、数据/编码、测试）；
4. 交叉引用项目既有 M0 实测红线（0xF4 崩溃）。

---

## 2. LiberNT 侧技术核查（证据链）

### 2.1 IMM32 栈完整存活——核心发现

`subsystems/win/win32k/user/` 下存在 `imm32/`、`ntuser/`、`rtl/`、`user32/`、`winsrv/`。其中 `subsystems/win/win32k/user/imm32/` 文件清单（文件大小单位字节）：

| 文件 | 大小 | 文件 | 大小 |
|---|---:|---|---:|
| `imm.c` | 35,836 | `ime.c` | 45,222 |
| `install.c` | 18,334 | `keymsg.c` | 33,682 |
| `compstr.c` | 34,468 | `softkbd.c` | 62,504 |
| `win3.c` | 58,462 | `win3send.c` | 30,204 |
| `imemenu.c` | 28,163 | `ctf.c` | 31,711 |
| `regword.c` | 15,015 | `utils.c` | 18,231 |
| `imepro.c` | 8,229 | `candidate.c` | 12,195 |
| `guideline.c` | 5,904 | `imm32.spec` | 5,443 |

`ntuser/ime.c` 另有 62,670 字节。文件头显示版权 `Copyright 2020-2025 Katayama Hirofumi MZ`、LICENSE `LGPL-2.1-or-later`、PROJECT `ReactOS IMM32`——即 ReactOS IMM32 原样继承，而非重写或裁剪。

`imm32/CMakeLists.txt` 关键行：

```cmake
spec2def(imm32.dll imm32.spec ADD_IMPORTLIB)
add_library(imm32 MODULE ${SOURCE} imm32.rc)
set_module_type(imm32 win32dll UNICODE ENTRYPOINT ImmDllInitialize 12)
target_link_libraries(imm32 wine win32ksys uuid)
add_importlibs(imm32 advapi32 user32 gdi32 kernel32 ntdll)
add_cd_file(TARGET imm32 DESTINATION reactos/system32 FOR all)
option(IMM_WIN3_SUPPORT "IMM Win 3.x support" ON)
```

结论：liberNT 具备 IMM32 IME 的**完整宿主能力**（安装、加载、消息处理、组合串、候选、软键盘、IME 菜单一应俱全）。IME 位数须与宿主进程匹配这一 NT 架构铁律不变，但宿主能力本身不缺。

### 2.2 `ImmInstallIMEW` 契约逐项核对——利好与隐藏冲突

逐行读取 `imm32/install.c` 后，与本项目逐项对照：

| libernt 行为（证据） | RosPinyin 现状 | 判定 |
|---|---|---|
| `Imm32LoadImeFixedInfo` 要求 `dwFileType == VFT_DRV` 且 `dwFileSubtype == VFT2_DRV_INPUTMETHOD`，否则报 "not an IME" | `rospinyin_res.rc` 已设 `FILETYPE=VFT_DRV` / `FILESUBTYPE=VFT2_DRV_INPUTMETHOD` | ✅ 完全吻合 |
| `Imm32LoadImeLangAndDesc` 读 `\VarFileInfo\Translation`，按 LangID+codepage 拼 `\StringFileInfo\%04X%04X\` 找 `FileDescription` | 资源已设 `Translation 0x0804/1200` 并有 zh-CN/en-US 字符串表 | ✅ 吻合；LANGID=0x0804 会落在本项目字符串表 |
| `Imm32AssignNewLayout` 动态分配 HKL（`0xE0XXYYYY`，范围 0xE020–0xE0FF），非固定常量 | 安装器不硬编码 HKL，记录 `ImmInstallIMEW` 返回值（D3 决策） | ✅ 吻合 |
| `Imm32WriteImeLayout` 写 HKLM `SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<HKL %08X>`：`Ime File` / `Layout Text` / `Layout File` | 本项目 `/step2` 调 `ImmInstallIMEW` 后，`/step3` 补写 `Layout Text` | ✅ 键名一致；`Layout File` 由 libernt 按 LANGID 选 kbdus.dll（简中走 default），无冲突 |
| **★ `Imm32WriteImeLayout` 会创建 HKCU `Keyboard Layout\Preload` 并写入序号；`ImmInstallIMEW` 末尾 `LoadKeyboardLayoutW(szImeKey, KLF_REPLACELANG)` 立即激活** | M0 红线：绝不写 Preload、绝不设默认 IME；安装器刻意不写 Preload，uninstall 里还注明 "Deliberately no Preload cleanup: we never wrote one" | ⚠️ **正面冲突**：即使本项目自己不写，`ImmInstallIMEW` 内部也会代写并激活 |
| HKL 序列化用 `StringCchPrintfW(..., L"%08X", HandleToUlong(hKL))`，`szImeKey[20]` | 本项目 `hkl_to_uint` / `%08X` / `text[16]` | ✅ 同为 32 位 HKL 语义，无位宽冲突（见 §3.4 修正） |

**这是本次评估最值得重视的发现**：M0 在 ReactOS 0.4.16 上实测——mzimeja「一次全量注册 + Preload 预载」导致登录路径加载 .ime 连带杀死 csrss → **BSOD STOP 0x000000F4**；据此定下「不写 Preload、逐键分步注册」红线。而 libernt 的 `ImmInstallIMEW` 恰恰**内置了 Preload 写入 + 立即激活**。因此 ReactOS 上验证过的 `/step1`…`/step4` 规避法在 libernt 上**可能失效或行为不同**。这不是「libernt 有 bug」——其 imm32 是对 Windows 行为的忠实实现——而是**本项目为规避 ReactOS 缺陷而设计的部署策略，需要在 libernt 上重新评估必要性**。

### 2.3 内核大换血的影响边界

对 IME 相关的潜在影响逐一判断：

- **win32k / USER / GDI**：官方明确不在替换范围 → IME 窗口、组合串、候选窗的消息链保持 ReactOS 语义。
- **csrss / 登录初始化路径**：本次自研内核重写了进程/线程与会话相关组件。M0 的 0xF4 是「登录时加载已注册 .ime 杀死 csrss」——csrss 相关行为是否随内核重写而改变，**必须从实测确认**，静态无法定论。
- **内存/缓存管理器、图形栈、存储/USB**：与 IMM32 IME 无直接耦合，风险低。
- **不变铁律**：IME 是进程内 DLL，32 位 .ime 无法服务 64 位进程。

---

## 3. RosPinyin 侧平台耦合点核查

### 3.1 有利条件（解耦干净）

- **零 ReactOS 私有头**：全库无 `#include <reactos/>`、`<ndk/>`、`<win32k/>`；唯一系统头是 `<windows.h>`、`<imm.h>`（仅安装器）、`<winsock2.h>`（仅 httpdl）。
- **手写 IME ABI**：`framework/host/imm32_abi.h` 自行声明 16 导出与结构，`IME_PROP_UNICODE=0x00080000` 等用公开常量值，不依赖 `<immdev.h>`。
- **零硬编码 DLL 名 / 零随仓二进制库**：链接只写 target 名（`user32 gdi32 imm32` 等），由 MinGW sysroot 解析。
- **零内联汇编 / 零 `#pragma pack` / 零 PE 头操作 / 零硬编码地址**。
- **静态数据格式架构无关**：`rospinyin.dat` v1 全 uint32 小端定长头（Header 52 / CharEntry 20 / SylEntry 16），`code_table.cpp` 用 `memcpy` 逐字段读，不做结构体整体映射。

### 3.2 已具备的 64 位意识（值得保留）

- `emit_text.hpp` 的 `TransMsgSlot { uint32_t message; uintptr_t wparam; intptr_t lparam; }`，`emit_text.cpp` 用 `static_cast<std::uintptr_t>`。
- `ui_window.cpp` 用 `reinterpret_cast<HWND>(static_cast<intptr_t>(-1))`、`SetWindowLongPtrW/GetWindowLongPtrW` + `LONG_PTR`。
- `imm_entry.cpp` 有 `static_assert` 校验 `TransMsgSlot` ↔ `TransMsg` 大小与 `offsetof(TransMsgList, TransMsg) == sizeof(UINT)`。
- `utf8.cpp` 用 `sizeof(wchar_t) == 2` 编译期适配。

### 3.3 需要改造的点

| # | 问题 | 证据 | 改造方向 |
|---|---|---|---|
| A | **无 64 位构建路径** | CI `MINGW32` + `mingw-w64-i686-*`；`ime/CMakeLists.txt` 无 target 架构选项；全仓无 `-m64`/x86_64 toolchain | 新增 `x86_64-w64-mingw32` 目标，产出 64 位 `.ime`；核对 `.def` 导出在 x64 下的修饰名 |
| B | **WOW64 视图零处理** | 全库无 `KEY_WOW64_*`、`Wow6432Node`、`RegDisableReflectionView`；安装器 `RegOpenKeyExW` 一律默认视图 | 安装器随 `.ime` 一起升到 64 位；若保留 32 位安装器须显式指定视图 |
| C | **System32 路径错配** | `GetSystemDirectoryW`（install.cpp:50、dat_source_win.cpp:26）在 WOW64 32 位进程返回 SysWOW64，而 `kLayoutDisplayName` 硬编码 `system32` | 64 位统一走 native System32，必要时用 `GetNativeSystemDirectoryW` |
| D | **静态 CRT 策略为 ReactOS 特化** | `-static-libgcc -static-libstdc++ -static`；CI 断言不导入 `InitializeCriticalSectionEx`、UCRT 等 | NT10 下该限制可放宽（Vista+ API 已存在）；若走 MSVC 需重引入 `vcruntime140`/UCRT |
| E | **手写 ABI 仅部分被断言覆盖** | `static_assert` 只覆盖 TransMsg 布局；`ImeInfo`、能力位、16 导出签名未全验 | 与 libernt imm32 逐字段核对（§2.2 已核对资源校验字段），补 x64 冒烟 |
| F | **字体链 ReactOS 特化硬编码** | `ui_window.cpp:107-113`：Droid Sans Fallback / Noto Sans SC / Microsoft YaHei / SimSun / Arial Unicode MS | 有 `GetTextFaceW` 校验与最终兜底，降级安全；建议按 Win11 字体集重排 |
| G | **弱符号技巧 MSVC 下失效** | `dat_source.cpp` 的 `RPY_WEAK` 在 MSVC 下为空 | 若引入 MSVC 构建需处理 |
| H | **测试全绑 x86 ReactOS** | `m0/vm/`、`m2/M2-ACCEPTANCE.md`、CI 导入表断言 | 新增 libernt amd64 + i386 冒烟（见 §6） |

### 3.4 对初判的修正：HKL 位宽不是问题

初次代码扫描把安装器 `hkl_to_uint()`（`static_cast<unsigned>(reinterpret_cast<uintptr_t>(hkl))`）与 `%08X` 列为「64 位 HKL 被截断」的 P0 风险。**拿到 libernt `imm32/install.c` 原文后应修正这一判断**：libernt 自身同样用 `HandleToUlong(hKL)` + `L"%08X"` + `szImeKey[20]`，即 IMM32 IME 的 HKL 语义本就是 32 位值（`0xE0XXYYYY`）。本项目与之**一致而非冲突**。真正的安装器风险是 WOW64 视图（B）与 System32 路径（C），以及 Preload 行为差异（§2.2），不是 HKL 位宽本身。

---

## 4. 分层可行性判定

```
① 引擎 / dat / UTF-8      [可直接复用]  纯 C++、零私有头、架构无关
② IME 核心 / UI           [可直接复用]  公开 API、指针已 64 位化、字体链微调
③ IMM32 16 导出 ABI        [高度吻合·待测]  libernt imm32 原样继承 ReactOS
④ 安装器 / 注册链路        [需重点改造]  WOW64 / System32 / Preload 红线重验
⑤ 构建工具链 / 位宽        [主要阻塞]    补 x86_64 目标与 64 位 .ime
⑥ 实机验证环境             [需重建]      libernt 双架构 VM 安装 + 切换压测
```

**总体可行性 = 中高。** ①② 占代码主体且零改造；③ 有强静态证据；④⑥ 是确定要做的工程活；⑤ 是明确的构建补齐。没有任何一层需要「重写引擎」或「换成 TSF」。

---

## 5. 风险清单

| 级别 | 风险 | 依据 | 缓解 / 验证 |
|---|---|---|---|
| **P0-A** | Preload 红线失效：`ImmInstallIMEW` 内部写 Preload + 立即激活，可能重现 0xF4 | §2.2 + M0 实测 | 阶段 0 探针：i386 libernt VM 上分步调用并重启，观察是否崩溃；若崩，改用手工写注册表绕开 `ImmInstallIMEW`（M0 已验证手工分步路径可行） |
| **P0-B** | 64 位 IME 缺失 | §3.3-A | 新增 MINGW64 目标；先验 amd64 裸 LoadLibrary 无害，再进登录路径 |
| **P1-C** | WOW64 注册表视图 / System32 错配 | §3.3-B/C | 安装器升至 64 位；native System32；加 `/status` 自检 |
| **P1-D** | IMM32 ABI 细节在 64 位下不一致 | §3.3-E | 逐字段核对 libernt 的 `imm.h`/`immdev.h` 与 16 导出签名；补 `static_assert` 与 x64 冒烟 |
| **P1-E** | 自研内核改变了 csrss/登录加载时序，M0 结论不再适用 | §2.3 | 静态无法定论，必须实机；这是阶段 0 探针的核心观测点 |
| **P2-F** | 字体链、静态 CRT、弱符号等习惯性假设 | §3.3-D/F/G | 按 NT10 环境放宽/重排；非阻塞，可后置 |

---

> **评审补注（2026-11-18，见 `M0-REDLINE-REVIEW.md` §8）**：P0-A 的缓解方向需要调整顺序。
> 由于 `ImmInstallIMEW` 内部就写 Preload（§2.2），"不写 Preload"在 libernt 上**不是我们可控制的动作**，
> 绕开平台 API 会把成本转到"长期维护手工注册分支 + 卸载留下悬空 Preload 条目"（`install.cpp:400`
> 现有注释"we never wrote one"在 libernt 上会变成错的）。
> 正确顺序是**先把 `DllMain` 清空、`RegisterUIServerClass` 移到 `ImeInquire` 惰性注册**，把"登录路径加载无害"
> 变成二进制的结构属性，再跑探针；探针从"验证 Preload 是否致命"变成"验证我们的 DLL 是否经得起被登录加载"。
> 绕开 `ImmInstallIMEW` 保留为兜底，不作为首选分支。阶段 2 的相应分支在阶段 0 通过后可直接删除。

> **评审补注之二（2026-11-18 19:56，见 `M0-REDLINE-REVIEW.md` §13）**：上面的"先加固再探针"已被实验支持，
> 且 P0-A 的定级可以下调。在 ReactOS 上做完了两组对照（各 3 次启动，均用发版二进制 940,601）：
> N 组（在 `Preload` 里但非首项，仅加载）3/3 活；N′ 组（**首项 = 本 IME**，即"默认输入语言"，
> 走 `KLF_ACTIVATE` → `ImeSelect` → 建 UI 的完整激活路径）也是 3/3 活。
> 也就是说 **"被登录路径加载并被激活"这件事对 struct 上干净的二进制不致命**；
> P0-A 不再表述为"可能重现 0xF4"，而是"**若 `DllMain` 里仍有会失败的初始化，新平台可能把它吞成不可见状态**"。
> 缓解不变（A1 空 `DllMain` + A1b `ImeInquire` 可拒绝），但它已是**纯健壮性**动作，不含"防蓝屏"含义；
> 阶段 0 的判据相应从"是否崩"改为"① 能否加载 ② `ImeSelect` 后能否输入 ③ 状态是否自洽"。

## 6. 建议移植路径（分阶段 + 门禁）

| 阶段 | 目标 | 关键动作 | 完成门禁 |
|---|---|---|---|
| **0. 低成本探针** | 用最小成本定性 | 走 libernt **i386** 源码构建；现有 32 位 `.ime` 不动引擎，执行 `ImmInstallIME` 注册 + 重启 + 切换压测 | 确认 ① ABI 能加载 ② Preload/0xF4 是否重现 ③ csrss 登录时序。产出：探针报告，决定是否需绕开 `ImmInstallIMEW` |
| **1. 64 位构建** | 打通产物 | 新增 `x86_64-w64-mingw32` CMake/CI 目标；产出 64 位 `rospinyin.ime`；CI 增 x64 PE 导入表断言（此时应允许 UCRT/Vista+ API） | 64 位 `.ime` 在 libernt amd64 上裸 `LoadLibrary`/`FreeLibrary` 无害 |
| **2. 安装器改造** | 部署正确 | 安装器升 64 位；修 native System32、显式注册表视图；按阶段 0 结论决定是否手工分步写键绕开 Preload | `/step1`…`/step4`（或新方案）在 amd64 上逐步注册成功、无 Preload 崩溃 |
| **3. 双机验收** | 功能闭环 | libernt amd64 + i386 各跑记事本输入（组合串/候选/选字/翻页/Esc/退格）、≥50 次切换压测 | 对齐 `m2/M2-ACCEPTANCE.md`，新增 libernt 章节与证据截图 |

> 顺序理由：阶段 0 用现成产物先排除最大的不确定性（Preload 红线），避免在错误的部署假设上先花大力气做 64 位构建。若阶段 0 发现 libernt 上 `ImmInstallIMEW` 安全，则阶段 2 大幅简化。

---

## 7. 许可证合规

- RosPinyin 代码 **MIT**，libernt **GPL-3.0**（继承代码多为 GPL-2.0+）。作为**独立第三方软件包**分发不构成衍生作品，**无冲突**——与项目现有「主形态：第三方软件包」定位一致。
- 保持 `in-tree 友好`（引擎平台无关、只走公开 IMM32 API、CMake 可选模块目标）是既有约束，移植不破坏这一点。
- **不承诺**直接以 `modules/optional` 并入 libernt 构建树；一旦 in-tree 则需按 GPL 合规重新评估（对应 README D5「仅评估不承诺」）。

---

## 8. 未决问题与验证清单

以下为静态评估无法定论、必须实机回答的问题：

1. libernt i386 / amd64 上，`ImmInstallIMEW` 写 Preload + 立即激活是否会重现 0xF4？（阶段 0 核心）
2. libernt 自研内核后，csrss / winlogon 的 IME 预载时序与 ReactOS 0.4.16 是否不同？
3. 64 位进程加载 64 位 `.ime` 时，手写 ABI 的 `ImeInfo` / 16 导出签名是否与 libernt imm32 逐字段一致？
4. `Layout Display Name` 的 `@%SystemRoot%\system32\rospinyin.ime,-1024` 在 libernt（SystemRoot 可能为 `C:\Windows`）与 WDDM/DWM 环境下是否正常解析？
5. 简中 LANGID 下 libernt 选 `kbdus.dll` 作 `Layout File`，与中文候选/标点输入是否有副作用？
6. 字体链在 Win11 风格字体集下的实际渲染效果？

---

## 9. 证据来源

- **libernt 官方**：`https://libernt.com/`；`https://libernt.com/news/libernt-in-house-kernel/`（2026-11-15 内核新闻，含「window manager and user-mode libraries are not in this step」原文）。
- **libernt 源码**（GitHub `LiberNT/LiberNT`，分支 `main-nt10`，2026-11-17 读取）：
  - `subsystems/win/win32k/user/imm32/`（目录清单与文件大小）
  - `subsystems/win/win32k/user/imm32/CMakeLists.txt`
  - `subsystems/win/win32k/user/imm32/install.c`（`Imm32LoadImeFixedInfo` / `Imm32LoadImeLangAndDesc` / `Imm32AssignNewLayout` / `Imm32WriteImeLayout` / `ImmInstallIMEW`）
  - `subsystems/win/win32k/user/ntuser/ime.c`、`base/ctf/`（目录清单）
- **一手用户报告**：`https://blog.cloudflare88.eu.org/posts/libre-nt-introduction`（2026-10-18，中文输入法/Rime 体验）。
- **本仓库**：`README.md`、`M0-REPORT.md`（0xF4 实测与红线）、`M1-REPORT.md`、`tools/check_m1.py`、`.github/workflows/ci.yml`、`CMakeLists.txt`、`ime/CMakeLists.txt`、`framework/host/imm32_abi.h`、`framework/host/imm_entry.cpp`、`framework/include/imfw/emit_text.hpp`、`framework/src/utf8.cpp`、`framework/ui/ui_window.cpp`、`tools/install-rospinyin/install.cpp`、`pinyin/host/dat_source_win.cpp`、`framework/src/code_table.cpp`。

> 本文证据均为静态核查；任何「吻合」结论都需在 libernt VM 上以实机注册/加载/切换测试收口。
