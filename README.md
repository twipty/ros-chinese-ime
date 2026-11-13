# RosPinyin — ReactOS 简体中文拼音输入法

**项目章程 / Project Charter**
状态：已立项（2026-11-12）｜路线：B（以 mzimeja 为蓝本新写 IMM32 拼音 IME）｜目标平台：ReactOS 0.4.16+（IMM32 架构）
当前阶段：**M1 骨架替换**（宿主侧代码完成，VM 实测待做，见 [M1-REPORT.md](M1-REPORT.md)）
仓库：https://github.com/twipty/ros-chinese-ime（public, main）｜代码许可证：**MIT**（D1 已定，2026-11-13）｜第三方与数据分层授权见 `NOTICE.txt`

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
| 字频 | Jun Da 现代汉语字频表，或开放语料自算 | 待核（决策点 D2） | 不可用时回退：Unihan + 手工分级频度 |
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
| D2 | 字频数据来源 | Jun Da（核 License 后定） | M2 前 |
| D3 | IME HKL 布局 ID | ~~硬编码 0xE0100804~~ → **由 `ImmInstallIME` 动态分配并记录返回值**（M0 解剖实证：mzimeja 用 `%08X` 接收系统分配值）；0xE0100804 仅作冲突排查参考 | M1 前（结论已定） |
| D4 | v0.1 是否含词组 | 不含（单字流畅 + 连续输入即可） | M2 末 |
| D5 | 是否以 modules/optional 进官方树 | 仅评估不承诺（v1.0 后视质量与上游意愿） | v1.0 后 |

## 9. M0 实测结论（2026-11-13）

### 蓝本 mzimeja 1.0.1.1 在 ReactOS 0.4.16 上开箱即崩（实测）

- **现象**：mzimeja 安装成功 → 重启 → BSOD **STOP 0x000000F4**（CRITICAL_OBJECT_TERMINATION，挂在 ntoskrnl.exe）。复位后系统可正常引导（注册表被回滚，仅剩孤儿 `system32\mzimeja.ime`）；随后用户实测**任务栏 EN 可切换、输入法实际可用并可输入日文**（中途曾出现「可切换但输入无效」的短时窗口）。→ 引擎本体在 ReactOS 上能跑，问题集中在**注册/预载路径与登录初始化时序**。蓝屏现场存证：`m0/vm/reactos.bsod-evidence.qcow2`，截图与 OCR 在 `m0/vm/logs/bsod*`。
- **解释**：0xF4 = 关键用户进程（csrss/winlogon 级）意外退出。ReactOS 的 IMM32 服务端位于 csrss：登录路径会加载已注册的 .ime DLL，DLL/注册在 ReactOS 上不兼容时连带杀死 csrss → 整机蓝屏。**这与立项时预判的头号风险完全吻合，现已从「预测」升级为「实证」。**
- **安装器解剖**（宿主侧完成，见 `m0/extract/`）：mzimeja setup = Inno Setup（2018-06-14）；数据流为 `zlb\x1a` + RAW LZMA1。内含 mzimeja.ime（x86/x64）、ime_setup32/64.exe、imepad.exe、verinfo.exe、五套词典。ime_setup32 逻辑：`ImmInstallIMEW` → 写 `SYSTEM\...\Keyboard Layouts\<HKL>`（IME File/Layout Text=MZ-IME/Layout Display Name）→ 写 `Keyboard Layout\Preload` → 写 `SOFTWARE\Katayama Hirofumi MZ\mzimeja`（字典路径等）。HKL 由系统动态生成（`%08X`），非固定常量。
- **对 M1 的设计红线（本实测直接触发）**：
  1. 拼音 IME DLL 必须先在 Windows 参照机验证「纯 LoadLibrary 无害」后再进 ReactOS；
  2. **绝不把拼音 IME 注册为默认 IME / Preload 第一项**——只作可切换布局追加（崩溃面最小化）；
  3. 安装器自带 Uninstall + 安全模式救援文档（mzimeja 的 Install/Uninstall 双按钮模式可借鉴）；
  4. 注册步骤逐键分步验证（每次只加一个键并重启确认），避免一次全量注册后无法定位崩溃点。
- **M0 核心结论**：蓝本**代码可参考**（架构、词典结构、UI 流程），第三方 IMM32 IME 在 ReactOS 上**确实能工作**；但其「一次全量注册 + Preload 预载」的即装即用部署路径存在**确定性 0xF4 崩溃风险**，不可照搬。M1 起按上述红线重写注册与加载链路。

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
- `tools/install-rospinyin/install.cpp` 按 M0 红线重写注册链路：**动态 HKL**、**不写 Preload**、**`/step1`…`/step4` 逐键分步注册**（ReactOS 每步重启验证）、自带 `/status` 与 `/u`。
- 引擎契约 `engine/include/rospinyin/engine.hpp` 定型，M2 替换实现时 API 不变；`ui/` 给出窗口分层约定。
- 质量闸门：`tools/check_m1.py`（25 项：日语残留、导出表、版本字段、M0 红线、命名一致性）+ Linux 下用 `tools/syntax_stub` 对 Windows 源码做零链接语法编译；本机 `ctest` 3/3 通过。

**尚未完成**：真实 Windows/MinGW 构建产物、Windows 参照机裸加载验证、ReactOS VM 分步注册与 50 次切换压测。本机为 Linux 无 Windows 工具链，**当前仓库不含可运行的 `rospinyin.ime` 二进制**；Windows CI job 已接线，推送后应先确认其产物。
