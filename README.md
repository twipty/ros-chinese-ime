# RosPinyin — ReactOS 简体中文拼音输入法

**项目章程 / Project Charter**
状态：已立项（2026-11-12）｜路线：B（以 mzimeja 为蓝本新写 IMM32 拼音 IME）｜目标平台：ReactOS 0.4.16+（IMM32 架构）
当前阶段：**M2 单字拼音引擎已完成并发布**（ReactOS VM 真实手打端到端上屏，`M2-ACCEPTANCE.md` 15/15 通过；`ctest` 11/11、`check_m1` 59/59）｜最新版本：**alpha0.0.2**（2026-11-18，[Release 说明](CHANGELOG.md)）｜首个公开版本：**alpha0.0.1**（2026-11-17）
仓库：https://github.com/twipty/ros-chinese-ime（public, main）｜代码许可证：**MIT**（D1 已定，2026-11-13）｜第三方与数据分层授权见 `NOTICE.txt`

> **⚠️ 重大前提变更（2026-11-18 19:56，读下文前必看）**
> 立项时把 M0 那次 0xF4 归因为「Preload 预载第三方 IME ⇒ 确定性崩溃」，并据此立了「绝不写 Preload / 绝不注册为默认 IME」的红线。
> 当日两组对照实验（各 3 次启动，均用发版二进制 940,601）**证伪了这个归因**：N 组（在 `Preload` 里但非首项）3/3 活，
> N′ 组（**首项 = 默认输入语言**，走完整激活路径）3/3 活；源码同时显示写 `Preload` 的是平台自己的 `ImmInstallIMEW`、
> 加载者是 `userinit` 而非 csrss。
> **新前提**：风险在「**登录路径上执行可能失败的初始化**」，不在「被 Preload 载入」。
> 因此红线 #2 降级为部署偏好，A1（`DllMain` 零工作）/A1b（`ImeInquire` 可拒绝）保留但理由改为健壮性。
> 完整证据与处置：[`M0-REDLINE-REVIEW.md`](M0-REDLINE-REVIEW.md) §13；实验记录：`m0/vm/PRELOAD-PROBE.md`。
> 下文凡出现「csrss 承载 IMM32」「确定性 0xF4」「绝不写 Preload」的表述，均以本框为准。

## 1. 一句话使命

为 ReactOS 提供一个可原生安装、开源、开箱即用的简体中文拼音输入法，填补 CORE-19268 中「Chinese IME」的缺口。

## 2. 立项依据

- ReactOS 对标 Windows Server 2003，只实现了老式 IMM32 输入框架；TSF/Cicero 整体缺失（CORE-19360/19361/19363），因此搜狗、QQ、微软拼音等现代 TSF 输入法无法安装运行（搜狗安装即崩，CORE-18218）。
- IMM32 栈在 0.4.15/0.4.16 已大幅补完（user32 默认 IME 窗口、EDIT 控件 IME 消息、IME 菜单等）；日语 MZ-IME 已在 ReactOS 上实测可用。**「第三方 IME 能跑」已被上游证明，缺的只是中文 IME 本体**。
- 早期社区测试（南极星、谷歌拼音线索、fireinput 插件等）均因 IMM32 未完成而失败，如今前置条件已改变。

### 路线决策记录

| | 路线 A：移植 freepy | 路线 B：以 mzimeja 为蓝本新写（选定） |
|---|---|---|
| 架构匹配 | IMM32，对口 | IMM32，对口 |
| 代码状态 | Win9x/2000 时代代码，需大量现代化 | 现代 C++，已在 ReactOS 验证 |
| 许可证 | GPL（freepy），词库结构有历史包袱 | 公共领域（mzimeja），继承后自由度最大 |
| 维护性 | 遗留代码，他人难读 | 干净起步，可长期作为社区官方中文 IME |
| 工作量 | 较小但不可控（坑在老代码里） | 中等但可控（参考实现完整） |

结论：选 B；freepy 仅作词典结构与交互设计参考。

> ⚠️ **M0 核查修正（2026-11-13）**：上表「公共领域」对 mzimeja **历史版本**成立，但上游已于 2026-01-12 改为 **GPLv3**，我们实测的 1.0.1.1 即为 GPLv3。路线 B 仍成立，但必须锁定公共领域时代的 `v1.0.0.3` tag 作为源码蓝本（或接受项目整体 GPLv3）——见第 9 节与 `M0-REPORT.md`。

### 产品形态决策

**主形态：第三方软件包**（独立仓库、独立发版、GitHub Release + RAPPS 分发、安装器写注册表 + ImmInstallIME）。

理由：ReactOS 官方 ISO 至今不自带任何 CJK IME 本体（日语 MZ-IME 也是用户手动安装），「官方预装输入法」没有先例且不受本项目控制；输入法迭代节奏（词库、修复、调优）远快于 ReactOS 半年-一年的发版周期；独立分发使 CC-CEDICT 等数据的许可证合规更干净。对标关系：做 ReactOS 版的「搜狗」，不是「微软拼音」。

**系统组件化为演化选项（非起点承诺）**：架构上保持 in-tree 友好——引擎平台无关、只用公开 IMM32 API、CMake 同时产出「独立安装包」与「ReactOS modules/ 可选模块」两种构建目标、词库数据独立成文件（均为 M1 起的既有约束，零额外成本）。若 v1.0 质量过硬且上游认可，可评估以 `modules/optional` 进官方构建树（呼应 CORE-12748 可选包设想），乃至中文版 ISO 预装（受 bootcd 体积约束，需上游点头）；即便如此，软件包渠道仍保留，nightly 用户需要独立更新。

## 3. 目标与非目标

### v0.1 目标（Definition of Done）

1. `rospinyin.ime` 可通过安装脚本一键装入 ReactOS nightly（0.4.16+），重启后仍然有效；
2. 记事本（EDIT 控件）中可流畅输入单字全拼：组合串显示、候选窗、数字键选字、翻页、回车上屏、Esc 取消、退格编辑；
3. 中英切换可用（Ctrl+Space；若上游 CORE-19455 未修复，提供备用键位），Shift 临时英文；
4. 中文标点、全角/半角切换；
5. 输入状态跨进程保持合理行为，无崩溃、无 BSoD。

### 非目标（明确排除）

- TSF/Cicero 支持（持续跟踪上游，不在本项目实现）；
- 云候选、账号系统、皮肤引擎；
- 繁体中文 / 其他变体（v0.2+ 再评估）；
- 在真实 Windows 上做产品级输入法——Windows VM 仅作开发期对照验证。

## 4. 技术基线

- 代码蓝本：[katahiromz/mzimeja](https://github.com/katahiromz/mzimeja) **v1.0.0.3 tag（公共领域 PDS，2026-01-03）**，Win2000/2003 IMM32 IME，ReactOS 已验证；1.0.1.1 及之后为 GPLv3，仅作黑盒行为参照、不得复制源码（**D1 已定**：蓝本锁此 tag，本项目代码 MIT）；
- 工程参考：freepy（词典结构与注册方式）、[katahiromz/ImeStudy](https://github.com/katahiromz/ImeStudy)（IMM32 内幕文档）、Microsoft SampleIME（官方样例）；
- 架构：IMM32 IME DLL（`rospinyin.ime`）+ 词库数据（`rospinyin.dat`）+ 注册/安装工具；
- 输入引擎设计为**平台无关的纯 C++ 库**，可在任意平台跑单元测试，GUI 层薄封装；
- 双环境验证法：WinXP/2003 VM（IMM32 API 文档齐全、易调试）作开发对照 → 每个 milestone 在 ReactOS nightly 上验收。

## 5. 数据与许可证

| 内容 | 来源 | 许可证 | 备注 |
|---|---|---|---|
| IME 代码 | 自研（蓝本 mzimeja v1.0.0.3 PDS） | **MIT**（D1 已定） | OSI 认证 + 跨法域可执行 + 与 ReactOS GPLv2+ 兼容；见 `LICENSE` 与 `NOTICE.txt` |
| 字级拼音映射 | Unicode Unihan `kMandarin` | Unicode License（自由使用） | 唯一权威来源 |
| 字频 | **已定 D2=A**：hanziDB.csv（MIT，基于 Jun Da 排名）主源 + Unihan `kHanyuPinlu` 辅源；Jun Da 原始表因商用限制已排除 | hanziDB MIT + Unihan License 双署名 | 决策见 `m2/M2-DATA-PLAN.md`；`ü` 用 `v` 键输入，数据层统一归一 |
| 词组词典（v0.2） | CC-CEDICT | CC-BY-SA | 若采用须与主代码分开分发（GPL/CC-BY-SA 单独打包） |

## 6. 上游协调

- 跟踪的 JIRA：CORE-19268（CJK 总纲）、CORE-18950（中文支持总纲）、CORE-19455（Ctrl+Space 热键，Reopened）、CORE-20799（键盘 NLS 表，阻塞 19455）、CORE-18923（Console IME）、CORE-12748（CJK 字体包）、CORE-20142（IME 菜单，已修，参考其修法）。
- 原则：ReactOS 侧 bug 用最小可复现样例报 JIRA；IMM32 实现层问题同时报 katahiromz/ImeStudy（Katayama 是该层唯一活跃开发者）；
- 目标：v0.1 发布时将 IME 包作为附件挂到 CORE-19268（沿用 social-ime 先例），争取官方提及与 RAPPS 收录。

## 7. 角色与协作

- Owner / 架构（默认：立项人）；
- 可拆分方向：输入引擎、UI 窗口、数据工具链、QA；
- 初期单人全栈可行；文档与可复现构建（本仓库）即降低后续接手门槛。

## 8. 关键决策点

| 编号 | 决策 | 默认倾向 | 决定时机 |
|---|---|---|---|
| D1 | 代码许可证 | ~~公共领域~~ → **MIT** | ✅ **已定（2026-11-13）**：蓝本锁定 PDS 的 `v1.0.0.3`，本项目代码用 MIT；理由见 `NOTICE.txt` 第 1 条与 `M0-REPORT.md` 第 6.2 节 |
| D2 | 字频数据来源 | ~~Jun Da 原始表~~（License 限研究/教学、商用需书面许可，与 MIT 冲突，已排除）→ **hanziDB.csv（MIT）主源 + Unihan `kHanyuPinlu` 辅源** | ✅ **已定 A（2026-11-16）**；附带约束：`ü` 输入用 `v` 键，数据层统一归一（见 `m2/M2-DATA-PLAN.md` 第 7 节） |
| D3 | IME HKL 布局 ID | ~~硬编码 0xE0100804~~ → **由 `ImmInstallIME` 动态分配并记录返回值**（M0 解剖实证：mzimeja 用 `%08X` 接收系统分配值）；0xE0100804 仅作冲突排查参考 | M1 前（结论已定） |
| D4 | v0.1 是否含词组 | 不含（单字流畅 + 连续输入即可） | M2 末 |
| D5 | 是否以 modules/optional 进官方树 | 仅评估不承诺（v1.0 后视质量与上游意愿） | v1.0 后 |

## 9. M0 实测结论（2026-11-13）

### 蓝本 mzimeja 1.0.1.1 在 ReactOS 0.4.16 上开箱即崩（实测）

- **现象**：mzimeja 安装成功 → 重启 → BSOD **STOP 0x000000F4**（CRITICAL_OBJECT_TERMINATION，挂在 ntoskrnl.exe）。复位后系统可正常引导（注册表被回滚，仅剩孤儿 `system32\mzimeja.ime`）；随后用户实测**任务栏 EN 可切换、输入法实际可用并可输入日文**（中途曾出现「可切换但输入无效」的短时窗口）。→ 引擎本体在 ReactOS 上能跑，问题集中在**注册/预载路径与登录初始化时序**。蓝屏现场存证：`m0/vm/reactos.bsod-evidence.qcow2`，截图与 OCR 在 `m0/vm/logs/bsod*`。
- **解释（2026-11-18 修正，重大前提变更）**：0xF4 = 关键用户进程意外退出。立项时把它读成「ReactOS 的 IMM32 服务端位于 csrss，登录路径加载不兼容的 .ime 连带杀死 csrss → 整机蓝屏」，并据此认定「Preload 预载 = 确定性崩溃」。**这个读法已被两处取证推翻**：
  - **加载者是 `userinit.exe`，不是 csrss**：`userinit!wWinMain → SetUserSettings → UpdatePerUserSystemParameters → IntLoadPreloadKeyboardLayouts → LoadKeyboardLayoutW`（ReactOS 0.4.16 源码）；
  - **同一个 `.ime` 留在 `Preload` 里可以一直开机**：本盘连续多次启动无 0xF4（`m0/vm/PRELOAD-PROBE.md`）。
  - 结论：**「安装即蓝屏」不能作为「Preload 有害」的证据**，它只说明「那次安装后的加载期行为出了问题」。
- **致伤机制重判 → 定判（2026-11-18 19:56）**：读崩溃时被注册的那个二进制与其参考源码后，机制**不是 Preload 的投递**，而是它在 `DLL_PROCESS_ATTACH` 里做 UI 初始化（注册表 + 词典映射 + 原子表 + `RegisterClassEx`）**且把失败吞成 `return TRUE`**。当日完成两组对照（各 3 次启动，均用发版二进制 940,601）：

  | 组 | 变量 | 结果 |
  |---|---|---|
  | N | 在 `Preload` 里但**非首项**（仅加载） | **3/3 活** |
  | N′ | **首项 = 本 IME**（默认输入语言，走 `KLF_ACTIVATE` → `ImeSelect` → 建 UI） | **3/3 活** |

  → **投递方式与登录期激活都不致命**，红线 #2 因此降级为部署偏好，约束改为「登录路径上不做任何可能失败的事」。完整证据与处置见 `M0-REDLINE-REVIEW.md` §13。
- **安装器解剖**（宿主侧完成，见 `m0/extract/`）：mzimeja setup = Inno Setup（2018-06-14）；数据流为 `zlb\x1a` + RAW LZMA1。内含 mzimeja.ime（x86/x64）、ime_setup32/64.exe、imepad.exe、verinfo.exe、五套词典。ime_setup32 逻辑：`ImmInstallIMEW` → 写 `SYSTEM\...\Keyboard Layouts\<HKL>`（IME File/Layout Text=MZ-IME/Layout Display Name）→ 写 `Keyboard Layout\Preload` → 写 `SOFTWARE\Katayama Hirofumi MZ\mzimeja`（字典路径等）。HKL 由系统动态生成（`%08X`），非固定常量。
- **设计红线（M0 实测触发 → 2026-11-18 按新前提修订）**：
  1. 拼音 IME DLL 必须先在 Windows 参照机验证「纯 LoadLibrary 无害」后再进 ReactOS；
  2. ~~**绝不把拼音 IME 注册为默认 IME / Preload 第一项**~~ → **已降级为部署偏好**（N/N′ 两组 6 次启动均活，且平台自己会写 Preload）。保留下来的约束是它的实质内容：**登录路径上不做任何可能失败的事**（`DllMain` 零工作 + `ImeInquire` 失败必须 `return FALSE`）；
  3. 安装器自带 Uninstall + 安全模式救援文档（mzimeja 的 Install/Uninstall 双按钮模式可借鉴）——**并且升级为必须清理悬空布局项**：`/u` 要按「布局键 `Ime File` 指向 `rospinyin*.ime`」扫全表，而不是只删自己刚写的那一条（本盘已存在历史遗留项，实测）；
  4. 注册步骤逐键分步验证（每次只加一个键并重启确认），避免一次全量注册后无法定位崩溃点。
- **M0 核心结论（修订版）**：蓝本**代码可参考**（架构、词典结构、UI 流程），第三方 IMM32 IME 在 ReactOS 上**确实能工作**；其「一次全量注册 + Preload 预载」的即装即用在当时触发了 0xF4，但 2026-11-18 的对照表明**触发条件是加载期行为，不是 Preload 预载这条路径本身**。M1 起重写的注册与加载链路照旧保留（分步注册 + `DllMain` 零工作），理由从「防 Preload 蓝屏」改为「**登录路径不留静默失败**」。

### 上游状态核查（2026-11-13，Task 1.5）

- **mzimeja 上游高度活跃**：最后提交 2026-11-12，近两周密集修复（标点查词典、括号符号化、浏览器 IME UI 焦点、五段/サ変/カ変动词否定形、词典增强）。正在引入 Vibrato（Rust 形态分析引擎，MIT/Apache-2.0）作可选转换后端。
- **★ 许可证变更（GitHub API 逐 tag 核实，M1 开工硬约束）**：上游 2026-01-05 在 README.md 写入 `MZ-IMEja: GPLv3` 声明，2026-01-12 22:30 以 commit `fbc986d71` 把 `LICENSE.txt`（原 375B 的 **Public Domain Software** 声明）换成 35824B 的 **GPLv3 全文**。
  - 我们实测的 **`v1.0.1.1` = GPLv3**（不是早前推测的 MIT/PD）；`v1.0.0.7`（2026-01-31）起所有 tag 均为 GPLv3。
  - **最后一个无歧义的公共领域版本是 `v1.0.0.3`（2026-01-03）**：LICENSE.txt = PDS、README 无 GPL 声明、`ime/` 骨架完整且不含 Vibrato。
  - 因此「路线 B 依据公共领域蓝本」**只能用 `v1.0.0.3` 源码**实现；若改用 1.0.1.1 源码，本项目整体受 GPLv3 约束。M0 下载的 1.0.1.1 二进制仅作黑盒测试，不构成衍生。→ **D1 已定**：蓝本 `v1.0.0.3` + 本项目 MIT，详见 `M0-REPORT.md` 第 5.2 节。
- **ReactOS JIRA**：CORE-19268（CJK Season 2）仍 Unresolved/Major、无 Fix Version，中文 IME 无排期；**CORE-19455（IME 热键）Reopened/Unresolved、无 Fix Version**（Alt+`VK_KANJI` 不生效，`ImmProcessKey` 不识别 Alt）；**CORE-20799（键盘 NLS 表）Unresolved** 且 blocks 19455。→ 热键短期内不会修，**M3 备用切换键方案不能砍**。
- 对 M1 的操作建议：以 `vendor/fetch-vendor.sh` 复现 mzimeja **`v1.0.0.3`** 只读参考树（非 master，带 SHA256 校验，上游源码不入库）；IME 核心引擎（romanic conversion/dic/UI）借鉴，部署层（Inno/ime_setup/ImmInstallIME 路径）按 M0 红线全部重写。

> 完整的实测记录、证据清单、范围偏差与 M1 交接见 **[M0-REPORT.md](M0-REPORT.md)**。

## 10. M1 进展（2026-11-13）

骨架替换的宿主侧工作已完成，详见 **[M1-REPORT.md](M1-REPORT.md)**：

- `ime/` 从零新写 IMM32 骨架：16 个导出全部实现，M1 语义为「空 IME 直通」（`ImeProcessKey` 全放行、`ImeToAsciiEx` 零消息）；`DllMain` 只注册窗口类，保证裸 `LoadLibrary` 在登录路径无害（M0 红线 #1）。
- 中文资源与版本信息落定：`FILETYPE=VFT_DRV` / `FILESUBTYPE=VFT2_DRV_INPUTMETHOD`、`Translation 0x0804/1200`、布局文本「中文(简体) - RosPinyin」。
- `tools/install-rospinyin/install.cpp` 按 M0 红线重写注册链路：**动态 HKL**、**安装器自身不写 `Preload`**（注意：`ImmInstallIMEW` 内部会写，见 `M0-REDLINE-REVIEW.md` §12；我们不写 ≠ 注册表里没有）、**`/step1`…`/step4` 逐键分步注册**（ReactOS 每步重启验证）、自带 `/status` 与 `/u`。
- 引擎契约 `engine/include/rospinyin/engine.hpp` 定型，M2 替换实现时 API 不变；`ui/` 给出窗口分层约定。
- 质量闸门：`tools/check_m1.py`（长期契约闸门，当前 57 项：日语残留、导出表、版本字段、M0 红线、命名一致性、M2 不变量）+ Linux 下用 `tools/syntax_stub` 对 Windows 源码做零链接语法编译；本机 `ctest` 11/11 通过。

**尚未完成（此为 2026-11-13 历史状态，后续已更新）**：真实 Windows/MinGW 构建产物、Windows 参照机裸加载验证、ReactOS VM 分步注册与 50 次切换压测。本机为 Linux 无 Windows 工具链，**当前仓库不含可运行的 `rospinyin.ime` 二进制**；Windows CI job 已接线，推送后应先确认其产物。→ 见 [M1-REPORT.md](M1-REPORT.md) 第二轮更新与下文第 11 节。

## 11. M2 进展（2026-11-16）

- **数据决策 D2=A 已落地**：hanziDB.csv（MIT）主源 + Unihan `kHanyuPinlu` 辅源；`ü` 统一用 `v` 键输入。原始数据固定 URL + SHA256，一键编译 `rospinyin.dat`（44,355 字 / 418 音节 / 902,768 B，字节级可复现）。
- **引擎**：共用拼音归一化库、`mkdat` 编译器、运行时表、`PinyinSession`（最长匹配+回溯切分、连续输入、音节下划线数据）、`CandidatePager`（9 候选、翻页、数字选字）。
- **IMM32 + UI**：`ime/imm_entry.cpp` 已实现真实键处理（组合、翻页、选字上屏、退格、Esc）；`ui/` 提供组合串窗口与候选窗，字体带 CJK 兜底链；窗口惰性创建，不在 `DllMain` 建窗。
- **质量闸门**：Linux 语法检查（`ime/` + `ui/` + 安装器）通过；`ctest` **7/7**；真实 dat 端到端验证 `zhongguo → 国`、`nv/lv/lve/nve` 命中预期字。
- **验收准备**：操作单 `m2/M2-ACCEPTANCE.md` 与数据 `m0/share/rospinyin.dat` 已就位，待 CI 产出含 M2 UI 的新 `.ime` 后在 ReactOS VM 执行。
- M1 的 passthrough 契约断言已随 M2 语义更新（保留注册/资源/DllMain 红线，新增会话激活断言），详见 `tools/check_m1.py`。

### M2 打通记录（2026-11-17，alpha0.0.1）

真实键盘手打（非程序注入）已端到端通过，`imhost` 三层记录（入口 `WH_KEYBOARD` / 窗口消息 / IME trace）逐条对齐：

```
入口层 DOWN=40 UP=40 | 窗口层 40/40 | WM_CHAR=5 WM_IME_CHAR=5 | 注入按键 0 次
上屏：你 → 你好 → 你好中 → 你好中国 → 你好中国谁
```

途中修掉的四个真问题（均带回归断言）：

1. **IME 从不被调用**：UI 类名 `RosPinyinUIServer` 17 字符，被 `imm32` 的 `szUIClass[16]` 截断后 `GetClassInfoW` 失败 → `fLoadFlag=1`。类名收敛到 `RosPinyinUISrv`（≤15，`static_assert` 兜底）。
2. **多字/丢字**：`WM_KEYUP` 也被喂进 `ImeToAsciiEx`（该路径无 lParam，看不到 transition 位）。`ImeProcessKey` 对 key-up 一律返回 `FALSE`。
3. **简体「谁」打不出**：Unihan 中 U+8C01 `kMandarin` 只有 `shéi`，`shuí` 被丢弃；且引擎从不读次读音字段。`mkdat` 改为把 `kHanyuPinlu` 的音作为次读音保留，`CodeTable::load_dat` 增加 alt 别名索引。
4. **候选里的空心框**：44,355 字里 34,457 条是 Unihan 独有生僻字（68% 在字体子集外）。`PinyinSession` 默认只放有字频的字（9,898 条），该音节为空时回退全表。

修复效果（真实引擎 + 真实数据）：`shui` 34 条 → **7 条「水 谁 税 睡 稅 誰 脽」**，简体「谁」第 2 位，空心框 20 → 0。回归 `ctest` 11/11、`check_m1` 57/57。


## 11.5 框架抽取：引擎宿主 / UI / 工具条独立为可复用模块（2026-11-16 变更目标）

应 Owner 要求，输入法框架（**不含**拼音码表与拼音选字逻辑）已从 RosPinyin 中独立出来，供形码输入法（RosXingMa，规划见 `xingma/XINGMA-PLAN.md`）等其他输入法复用。目录重组：

```
framework/   # imfw 可复用框架：engine 契约 + CodeTable/dat 格式工具类 + CandidatePager
             #   host/  IMM32 16 导出（通用键处理）、DllMain、UI server、会话存储（框架不含码表）
             #   ui/    窗口基类 + CJK 字体链、组合串窗口、候选窗、输入法工具条（新增）
pinyin/      # 拼音插件：归一化、音节切分、PinyinSession、PinyinScheme、内置兜底码表、自管数据源
ime/         # rospinyin.ime 装配：scheme_reg + 品牌 def/rc/图标（其余全部来自 framework/）
```

- **Scheme 插件接口**（`framework/include/imfw/scheme.hpp`）是复用缝：字母表、窗口类名、工具条标签、选字/翻页键策略（默认经典行为）、**无参引擎工厂 + `reload_data()` 数据热重载钩子**、`ImeConfigure` 回调。框架拥有退格/Esc/上屏与全部窗口渲染。
- **数据完全归方案自管**（2026-11-17 G2 终局）：框架不加载、不持有、不传递码表；`CodeTable`/dat 格式保留为通用工具类（拼音在用），形码用自己的 `ShapeStore`。拼音的数据源在 `pinyin/`（系统目录 .dat + 内置兜底表）。
- `Result.segments`（`imfw/engine.hpp`）让组合串窗口对拼音与形码通用（下划线按引擎切分画）；组合串与 segments 契约为 UTF-8，CJK 可直接渲染。
- 通用 `CodeTable` 改用 dat 自带键名表索引，框架侧不再有 `pinyin` 字样（`tools/check_m1.py` 新增防污染断言）。
- 窗口类按 Scheme 命名并按名去重注册，两个 IME DLL 可同进程共存。
- **xingma 缺口 G1–G4 已全部在框架侧解决**（2026-11-16；G2 于 2026-11-17 演进为「框架零码表」终局形态，详见 `framework/README.md`）：`imfw::emit_text` 多码点/非 BMP 上屏、无参 `create_engine` + `reload_data()` 热重载、`Scheme::configure` 接管 `ImeConfigure`、选字/翻页键策略下放 Scheme + 组合串 UTF-8 化。形码无需自建宿主层。
- 宿主 ctest **11/11**（含 `emit-text`、`utf8`、`code-table` 回归测试）；真实 MinGW 头交叉语法检查（clang i686-w64-windows-gnu + mingw 头）12/12；真实 dat 链路复验通过。
- 复用指南见 **[framework/README.md](framework/README.md)**。
