# Windows GUI 安装兼容性评估（P3，仅评估）

> 状态：**静态评估完成，Windows 真机未做**。本文不改变任何白名单、安装行为或发布资产；GUI 平台闸门仍为 `ProductName == "ReactOS"`，Windows 真机仍只走 CLI `/step1..4`。

## 1. 结论

- **不把 Windows 登记进 GUI 白名单。** 现有白名单 `kSupportedProductNames[] = {L"ReactOS"}`（`tools/install-rospinyin/platform_guard.hpp:22`）保持不变；未登记平台默认拒绝。
- 理由：`ProductName` 只能说明是 Windows，不能证明本项目的 IMM32 注册链路、系统目录、重启后布局持久化与卸载行为在 Windows 上成立。没有 Windows 实机证据时放行，等于把「能点安装」误当成「安装可用」。
- **CLI 例外继续保留**：`install-rospinyin.exe /step1../step4` 仍供 Windows 真机分步诊断使用（`/i` 仅作参考机一次性入口），因为 CLI 把每一步暴露给用户，失败点可定位；GUI 的批量动作没有这种可观测性。
- 项目形态固定为独立软件包。

## 2. 误报风险

| 场景 | 风险 | 现状与处置 |
|---|---|---|
| SmartScreen / 未知发布者 | 未签名 exe 可能被拦截 | 这是**预期误报**，不是缺陷；发布说明要求用 `SHA256SUMS.txt` 核对，不建议关闭杀软（`docs/releases/alpha0.0.4.md:24-34`） |
| 杀软把「释放文件 + 写注册表」判为安装器行为 | 可能删除临时释放的 `.ime`/CLI，导致 step1 后载荷缺失 | GUI 先解压到 `%TEMP%` 唯一子目录，逐项做 SHA256/长度/CRC32 校验，结束清理；失败返回 `PayloadCorrupt`，提示重下并核对哈希（`install_steps.hpp:37-45,120-129`） |
| 把 Windows 误判为 ReactOS | 如果按 `ProductName` 前缀或宽松匹配放行，可能在 Windows 上写入 `C:\ReactOS\system32` 不存在的路径 | 现实现用大小写不敏感的**全等**比较（`platform_guard.hpp:26-51`），并已有「前缀-only match rejected」测试；不得放宽 |

## 3. 注册表与系统副作用

- IME 安装只追加一个可选择的键盘布局，**不写 Preload、不设默认输入法**（M0 红线；`tools/check_m1.py` 以三键 allowlist 约束 `install.cpp` 的注册表写入）。Windows 上若放宽 GUI，用户会误以为安装器会处理默认布局与语言栏差异，但实现并没有做这些适配。
- `/u` 只删本项目写入的布局键、HKCU 记录与载荷；可选 Droid 字体包只在存在 `DroidFontOwned` 标记时清理，系统自带字体不动（`install.cpp` 的 font-install/uninstall 路径）。这些语义都基于 ReactOS 路径约定，Windows 上未验证。
- Windows 的 UAC、已签名驱动策略、输入法语言栏与 `ImmInstallIMEW` 返回值语义可能与 ReactOS 不同；没有实机日志前不能声称兼容。

## 4. 若未来要登记 Windows，最小验证清单

1. 实机 Windows（32/64 位各一）运行 GUI 安装器：白名单放行来源判定、解压校验、step1..4 每步退出码、重启后布局是否持久、候选窗是否出现。
2. 对照 CLI `/step1..4` 与 `/status` 输出，确认 GUI 与 CLI 写同一组注册表值、无额外副作用。
3. 卸载后重启：布局键、HKCU 记录、`.ime`/`.dat` 与 `.old` 残留全部清除，系统输入法列表无孤儿项。
4. 记录杀软/SmartScreen 实际拦截与放行步骤；发布说明补充对应 hash 核验指引。
5. 只有在上述证据归档后，才在 `kSupportedProductNames` 增加显式项并同步 `installer_logic_test.cpp` 的 allowlist 测试；本轮不做。

## 5. 边界

- 不改白名单、不改 CLI 路径、不改 CI 发布资产。
- guest 内的安装/切换/打字仍由用户手工完成，宿主侧不做键鼠注入。
