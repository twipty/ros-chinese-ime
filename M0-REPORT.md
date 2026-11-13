# RosPinyin M0 总结报告

**日期**：2026-11-13 ｜ **阶段**：M0 蓝本移植验证（已完成，含范围调整） ｜ **下一阶段**：M1 骨架替换

---

## 1. 结论摘要

M0 原目标是「mzimeja 在 ReactOS 上原样跑通，取得『什么能跑』的第一手基线」。目标达成，但路径与立项设想有三处实质偏差，且都比原计划更有价值：

| # | 结论 | 性质 |
|---|---|---|
| 1 | **第三方 IMM32 IME 在 ReactOS 0.4.16 上确实能工作**：任务栏 EN 指示器可切换，用户实测可输入日文（候选 → 假名/汉字上屏） | ✅ 立项前提被实证 |
| 2 | **「即装即用」部署路径会触发 BSOD 0xF4**（首次重启），且 Install 后的短时窗口内切换输入法曾出现「可切换但输入无效」 | ⚠️ M0 最核心发现 |
| 3 | **蓝本许可证已变更**：上游 2026-01 从公共领域（PDS）改为 GPLv3；我们实测的 1.0.1.1 是 GPLv3 → 已定：蓝本锁 `v1.0.0.3`(PDS)、本项目用 MIT | ✅ D1 已落定 |
| 4 | ReactOS 热键两议题（CORE-19455 / 20799）仍 Unresolved，无 Fix Version → M3 必须保留备用切换键方案 | ℹ️ 上游核查 |

**一句话**：蓝本的**代码架构可参考**，但**部署方式必须重写**、**许可证基线必须重选**。这正是 M0「去风险」的意义。

---

## 2. 交付物清单

| 路径 | 内容 | 用途 |
|---|---|---|
| `m0/vm/reactos.qcow2` | 8G qcow2，已装好 ReactOS 0.4.16 (Build 20260829-release) | M1 起的标准验收机 |
| `m0/vm/reactos.bsod-evidence.qcow2` | 蓝屏现场的完整磁盘副本 | 崩溃复现证据，勿删 |
| `m0/vm/common.sh` | 公共配置：`accel=kvm:tcg` 自动回落、rtl8139 SLIRP、usb-tablet、`-serial file:rosdbg.log`、HMP monitor socket、VNC :1 | 两脚本共用 |
| `m0/vm/run-reactos-install.sh` / `run-reactos.sh` | ISO 安装引导 / 日常启动 | 标准入口 |
| `m0/vm/noVNC-1.5.0/` + websockify | 浏览器直连 VM（`http://127.0.0.1:6080/vnc.html`） | 用户交互式操作 |
| `m0/downloads/ReactOS-0.4.16-i386.iso` | 388MB，SHA256 `6f15ca6e…cbf6855` | 安装介质 |
| `m0/downloads/mzimeja-1.0.1.1-setup.exe` | 2.1MB，SHA256 `59623edf…a8f0ff1d` | 蓝本二进制（GPLv3，仅测试用途） |
| `m0/vm/logs/rosdbg.log` | FreeLoader / 内核调试输出 | 引导与健康检查 |
| `m0/vm/logs/*.png / *-ocr.txt` | 引导链、Live 桌面、Setup 各页、蓝屏的截屏与 OCR 全文 | 实证留档 |
| `m0/extract/inno-full.bin` | Inno Setup 数据流全量解压（RAW LZMA1） | 安装器解剖 |
| `m0/probe/reactos/Fonts/DroidSansFallback.ttf`、`reactos/inf/font.inf` | ISO 解出的字体与字体替换配置 | CJK 字体核实 |
| `m0/probe/mzimeja-v1.0.1.1-LICENSE.txt` | 1.0.1.1 tag 的 GPLv3 全文 | 许可证证据 |
| `m0/share/` + `http.server :8000` | guest 经 `http://10.0.2.2:8000/` 拉文件 | 文件传输通道 |
| `LICENSE` / `NOTICE.txt` | MIT 全文 + 第三方与数据分层授权声明 | D1 落定后的发行合规基线 |
| `vendor/fetch-vendor.sh` | 带 SHA256 校验的蓝本复现脚本（v1.0.0.3 PDS） | 上游源码不入库仍可复现构建 |

**方法论沉淀**：headless QEMU 验证流程已存入用户级技能 `~/.workbuddy/skills/qemu-headless-vm-verify/SKILL.md`。

---

## 3. 环境与验证方法

- **QEMU 11.1.3**（Arch 包用户态解包至 `~/.workbuddy/binaries/qemu-user/`，沙箱无 sudo / 无 `/dev/kvm` → 走 **TCG**），SeaBIOS 1.17.0；`qemu-img create -f qcow2 8G`。
- **headless 验证链**（绕开 X11）：HMP `screendump` → PPM → ffmpeg → PNG → Pillow 颜色/像素差分析 → tesseract（`TESSDATA_PREFIX` 指向本地 `eng.traineddata`）OCR。
- **两个曾误判的坑**：
  - 串口/debugcon 长时间 0 字节 → 并非机器没跑，而是 Arch 版 SeaBIOS 未编串口调试；`rosdbg.log` 出现 FreeLoader 输出（`fixme: DiskGetPartitionEntry() unimplemented for RAW`，空盘正常）即证明引导链已通。
  - 手写 VNC RFB 抓帧反复失败（ServerInit bpp=0、无 framebuffer update）→ 纯协议层红鲱鱼，**HMP screendump 才是正解**。
- **实证链**：SeaBIOS → FreeLoader → ReactOS Live 桌面（Recycle Bin / Command Prompt / Install ReactOS / Read Me）→ Setup 欢迎页 → 第一阶段（分区+复制）→ Restart → 从硬盘 `C:\ReactOS` 引导 → 第二阶段 "Finishing the Installation" → **安装版桌面**。串口日志全程零 err/fail。

---

## 4. 实测发现

### 4.1 CJK 字体（用户提出核实）

- ISO 自带 `DroidSansFallback.ttf`（3.08MB），实测覆盖 **CJK 统一汉字 16504 字 + 假名 180 字**，「拼音输入法测试」等常用字全覆盖；另有 FSEX301-L2（日文点阵）等。
- 但 `font.inf` 的 `[Font.CJK.Reg]` 段（把 MS Shell Dlg / MS UI Gothic 等 7 个 UI 字体替换到 Droid Sans Fallback）**只在安装 locale 为 CP932/936/949/950 时挂载**。English (US) 安装的 UI 走 Latin 替换链（MS Shell Dlg → Tahoma）。
- **M1 设计约束**：候选窗/状态栏必须**显式指定 Droid Sans Fallback**，不可依赖系统字体回退（ReactOS GDI 回退不完善，即 CORE-12748 讨论的问题）。M8 发行工程需带字体检测/提示。

### 4.2 安装 mzimeja → 首次重启 BSOD

- **STOP 0x000000F4 — CRITICAL_OBJECT_TERMINATION**，挂在 ntoskrnl.exe：关键用户进程（csrss/winlogon 级）意外退出。
- **解释**：ReactOS 的 IMM32 服务端位于 csrss；登录路径加载已注册的 .ime DLL，DLL 或其依赖在 ReactOS 上不兼容时连带杀死 csrss → 整机蓝屏。这与立项时预判的头号风险完全吻合，现由「预测」升级为「实证」。
- **恢复**：`system_reset` 后系统正常进桌面；`reg query` 显示 `E010` 布局键与 Preload 均为空，仅剩孤儿 `system32\mzimeja.ime`（曾误查 HKLM 路径，正确路径是 `HKCU\Keyboard Layout\Preload`）。推测注册表被回滚（LastKnownGood 类机制）或安装器注册未完全落盘。
- **二次观察**：复活后用户报告「任务栏点 EN 可切换，但输入无效」，随后更新为**输入法实际可用、可输入日文**。说明 IME 引擎本身能在 ReactOS 跑通，崩溃集中在**注册/预载路径与登录初始化时序**。

### 4.3 安装器解剖（宿主侧，Inno Setup）

- 数据流格式：`zlb\x1a` + RAW LZMA1（`dict_size=0x00800000, lc=3, lp=0, pb=2`，偏移 9 起），全量解出 `inno-full.bin`。
- 文件：`mzimeja.ime`（x86/x64）、`ime_setup32/64.exe`、`imepad.exe`、`verinfo.exe`、五套词典（basic/name/postal/kanji/radical）。
- **注册链**（`ime_setup32.exe` 字符串）：`ImmInstallIMEW` → 写 `HKLM\SYSTEM\...\Keyboard Layouts\<HKL>`（IME File / Layout Text=MZ-IME / Layout Display Name）→ 写 **`Keyboard Layout\Preload`**（把第三方 DLL 推进登录路径，即蓝屏嫌疑最大的一步）→ 写 `SOFTWARE\Katayama Hirofumi MZ\mzimeja`（词典路径）。
- **HKL 由系统动态生成**（`%08X` 格式化输出），**不是固定常量** → 原 README 的 D3「硬编码 0xE0100804」需修正为「让 `ImmInstallIME` 分配，记录返回值」。

### 4.4 对 M1 的设计红线（已写入 README 第 9 节）

1. IME DLL 先在 Windows 参照机验证「纯 LoadLibrary 无害」后再进 ReactOS；
2. **绝不注册为默认 IME / Preload 第一项**，只作可切换布局追加；
3. 安装器自带 Uninstall + 安全模式救援文档；
4. 注册逐键分步验证（每次只加一键并重启确认），避免一次全量注册后无法定位崩溃点。

---

## 5. 上游核查（Task 1.5）

### 5.1 mzimeja 仓库活跃度

- 最后提交 **2026-11-12**（M0 期间），近两周密集修复标点查词典、括号符号化、浏览器 IME UI 焦点、五段/サ変/カ変动词否定形、词典增强；作者持续开发中。
- 正在引入 Vibrato（Rust 形态分析引擎，MIT/Apache-2.0）作可选转换后端。
- `v1.0.1.1` tag = commit `3c292fa3f`（2026-11-12T05:04:14Z）。

### 5.2 ★ 许可证时间线（M1 开工前硬约束）

| 时间 | 事件 | 证据 |
|---|---|---|
| 2016-04-02 ~ 2026-01-11 | `LICENSE.txt`（375B）= **Public Domain Software (PDS)**，「Copyright (C) 2016-2017 Katayama Hirofumi MZ」 | commit `a9a46fa6c` 版全文 |
| 2026-01-04 12:01 / 01-05 11:01 | README.md 加入 Vibrato 集成说明与 `- MZ-IMEja: GPLv3` 声明（**此时 LICENSE.txt 仍是 PDS，声明互相矛盾**） | commits `1c78388fa`、`7d77977dc` |
| **2026-01-12 22:30** | `update LICENSE.txt as GPL v3`：换成 35824B 的 GPLv3 全文 | commit `fbc986d71` |
| 2026-01-31 `v1.0.0.7` 起 | 各 tag 均为 GPLv3 | — |
| 2026-11-12 `v1.0.1.1` | **GPLv3**（即我们下载并实测的安装包版本） | `m0/probe/mzimeja-v1.0.1.1-LICENSE.txt` |

- **最后一个无歧义的 PDS 版本：`v1.0.0.3`（2026-01-03）**（LICENSE.txt = PDS，README 无任何 GPL 声明）。其 `ime/` 骨架完整：`imm.cpp` / `process.cpp` / `input.cpp` / `ui.cpp` / `uicand.cpp` / `uicomp.cpp` / `uistate.cpp` / `mzimeja.def` / `CMakeLists.txt` 等，且**不含 Vibrato**，比新版更简洁，正好适合 M1 的骨架替换。
- `v1.0.0.4`（01-04 01:08）、`v1.0.0.5`（01-04 05:38）也在 README 改动前，但保守取 `v1.0.0.3`。
- **M0 期间下载的 1.0.1.1 二进制仅用于黑盒测试，不构成衍生作品**；但若 M1 从其源码复制任何代码，项目将受 GPLv3 约束。

> 此结论**推翻**了 README 早前「1.0.1.1 内嵌 README 称 MIT、可视为 MIT/PD」的假设。D1 已于 2026-11-13 落定：**蓝本 v1.0.0.3（PDS） + 本项目代码 MIT**，见第 6.2 节与 `NOTICE.txt`。

### 5.3 ReactOS JIRA

| 议题 | 状态 | 影响 |
|---|---|---|
| CORE-19268 CJK Season 2 | Unresolved / Major，无 Fix Version；含 TSF/CUAS/CJK IME/Console IME 等 7 项，中文 IME 无排期 | 本项目的填空价值不变 |
| **CORE-19455** IME Hotkeys won't work correctly | **Reopened / Unresolved**，无 Fix Version；Alt+`VK_KANJI` 等不生效，`ImmProcessKey` 不识别 Alt | M3 主热键 Ctrl+Space 需实测 + **备用键方案不能砍** |
| **CORE-20799** input system has to use keyboard NLS table | **Unresolved** / New Feature，blocks CORE-19455，relates to CORE-19268 | 同上，短期不会修 |
| CORE-18950 / 18923 / 12748 / 20142 | 维持立项时判断（中文支持总纲 / Console IME 桩 / CJK 字体包 / IME 菜单已修可参考） | 见 ROADMAP M7、M8 |

---

## 6. 范围偏差与 M1 交接

### 6.1 相对原 PLAN 1.x 的偏差（均已评估，不阻塞）

| 原计划 | 实际 | 理由 |
|---|---|---|
| 1.1.1 建 WinXP/2003 参照 VM | 未建 | 用户决定跳过实机验证，引用上游证据 + 宿主侧解剖替代 |
| 1.2 自行构建 mzimeja | 解包官方 1.0.1.1 二进制 | 更快拿到可运行基线与安装逻辑 |
| 1.3 Windows 参照行为 | 由安装器解剖得出注册链/文件清单 | 跳过实机 |
| 1.4 产出「ReactOS vs WinXP 行为差异清单」 | 改为「ReactOS 单侧行为记录 + 安装器解剖 + 蓝屏根因」 | 差异清单的双机基线缺失，M1 需要时再补 |
| 1.5.2 向 Katayama 通报立项意图 | 未做 | D1 已澄清，可在 M1 骨架成形后联系，顺带说明锁定 PDS 快照的原因 |

### 6.2 决策点状态

| 编号 | 原倾向 | M0 后状态 |
|---|---|---|
| **D1 代码许可证** | 公共领域 | ✅ **已定（2026-11-13）**：蓝本锁 `v1.0.0.3`（PDS），本项目代码采用 **MIT**。选 MIT 而非纯 PD/CC0：OSI 认证、跨法域可执行、与 ReactOS GPLv2+ 合并确定兼容、含免责条款；Apache-2.0 因与 GPLv2 不兼容被排除。见 `LICENSE` / `NOTICE.txt` |
| D2 字频来源 | Jun Da（核 License 后定） | 不变，M2 前 |
| **D3 HKL** | 硬编码 `0xE0100804` | ⚠️ **修正**：改为 `ImmInstallIME` 动态分配 HKL 并记录返回值；硬编码值仅作冲突排查参考 |
| D4 v0.1 是否含词组 | 不含 | 不变 |
| D5 进官方树 | 仅评估 | 不变 |

### 6.3 M1 入口建议

1. ~~先定 D1~~ **D1 已定**：蓝本 `v1.0.0.3`（PDS，只读参考）+ 本项目代码 MIT；蓝本源码已落地 `vendor/mzimeja-ref/`（本地，不入库），可用 `vendor/fetch-vendor.sh` 复现。
2. 按 2.1 建立仓库骨架（`engine/` `ime/` `ui/` `tools/` `tests/`，CMake 双构建目标，GitHub Actions 骨架），蓝本源码作为 `vendor/mzimeja-ref/` **只读参考**入库并注明来源 tag 与许可证。
3. 严格按第 4.4 节四条红线实现 2.3 中文注册与 2.4 空输入法直通，在现有 `reactos.qcow2` 上验证（勿再用证据盘）。

---

## 附录：关键复现命令

```bash
# 启动（沙箱内需 run_in_background 保活，普通 nohup/setsid 会被收割）
cd ros-pinyin/m0/vm && ./run-reactos.sh

# HMP 截屏 → PNG → OCR
printf 'screendump logs/shot.ppm\n' | socat - unix-connect:monitor.sock
ffmpeg -y -loglevel error -i logs/shot.ppm logs/shot.png
TESSDATA_PREFIX=logs tesseract logs/shot.png logs/shot -l eng --psm 6

# 键盘驱动 / 复位
printf 'sendkey ret\n'   | socat - unix-connect:monitor.sock
printf 'system_reset\n'  | socat - unix-connect:monitor.sock

# 网页交互
python -m websockify --web noVNC-1.5.0 6080 127.0.0.1:5901   # → :6080/vnc.html
```
