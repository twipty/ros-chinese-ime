# RosPinyin

ReactOS 的开源简体中文拼音输入法 · [文档地图](docs/README.md) · [项目总览](docs/projectoverall.md)

## 项目介绍

RosPinyin 以独立 IMM32 输入法模块 `rospinyin.ime` 和码表 `rospinyin.dat` 分发，目标是在 ReactOS 0.4.16+（i386）上提供开箱即用的全拼中文输入，填补 CORE-19268 中「Chinese IME」的缺口。代码采用 MIT 许可证，数据与第三方组件的授权见 `NOTICE.txt`。

已发布版（alpha0.0.2）支持连续拼音输入、候选窗与翻页、空格/数字选字、`v` 键输入 `ü`、退格与 Esc 编辑、单独按 Shift 切换中英文；在 ReactOS 0.4.16 i386 真机验收 15/15 通过，目前只支持单字上屏。

完整的项目章程、立项依据、决策记录与开发历程见 [项目总览](docs/projectoverall.md) 和 [文档地图](docs/README.md)。

## 使用说明

以 alpha0.0.4 候选版为例（ReactOS 0.4.16+ i386）：

1. 从 Release 下载 `install-rospinyin-gui.exe`（自包含，双击安装/卸载）；需要诊断时用 `install-rospinyin.exe /step1..4`。两个 `.exe` 都已内嵌或分发 `rospinyin.ime` + `rospinyin.dat`。
2. GUI 安装器只在 `ProductName == "ReactOS"` 时放行；Windows、`libernt` 等未登记平台会拒绝安装。Windows 真机请使用 CLI `/step1../step4` 分步方式，GUI 不支持 Windows 安装。
3. 若走 CLI 或手工路径，系统目录是 `C:\ReactOS\system32`，不是 `C:\Windows\system32`：

   ```bat
   copy /y rospinyin.ime C:\ReactOS\system32\
   copy /y rospinyin.dat C:\ReactOS\system32\
   ```

4. 注册输入法布局（GUI 会调用 CLI step1..4，或手工用 `tools/install-rospinyin` / `ImmInstallIMEW`），然后重启；用 `Alt+Shift` 切到「中文(简体) - RosPinyin」。
5. 直接输入拼音即可选字：空格/数字选字，`=`/`.` 下一页、`-`/`,` 上一页，`v` 代替 `ü`，Backspace 回退，Esc 取消。
6. **中英文切换**：单独按一下 `Shift` 在中/英之间切换；英文态按键直接上屏，`Shift+A` 仍是正常的大写 A。
7. **`[快]` / `[全]` 字形模式**：`[快]` 是默认模式，只显示系统保底字体可绘制的候选；`[全]` 显示全部候选，包含需要额外中文字体支持的生僻字。按 `Ctrl+Shift+G` 切换，状态显示在工具栏；该设置只对当前输入法会话生效，重选输入法后恢复 `[快]`。`[全]` 模式会按码点平面切换文津三片字体渲染扩展区字形（候选窗与编码窗）。**必须三字体齐备才能正常显示，缺少任意一个的后果不可预知**；请装齐 P0/P2/P3 并重启后再使用 `[全]`。该字体不随安装包附带，请从[文津官方发布页](https://github.com/takushun-wu/WenJinMincho/releases/tag/v2.100)下载 TTF 版 7z 包（解压后得到 P0/P2/P3 三个平面分片），按官方 SHA256 校验后安装并重启（详见 alpha0.0.4 发布说明）。注意：已发布安装器弹窗内写死的 `http://10.0.2.2:8000/...` 是测试期 guest 内网地址，公网用户请以官方发布页为准。在 ReactOS nightly（0.4.17-dev）上还需先自行安装 Droid 字体，否则包括安装器在内的中文界面都会显示 `?`，0.4.16 正式版自带该字体。

> 杀毒误报：未签名安装器可能被 SmartScreen/杀软提示「未知发布者」。请用 `SHA256SUMS.txt` 核对下载文件，不要关闭杀软；详细指引见 [alpha0.0.4 发布说明](docs/releases/alpha0.0.4.md)。

## Release

- [全部版本](https://github.com/twipty/ros-chinese-ime/releases)
- [alpha0.0.4（GUI 安装器候选，待 tag）](docs/releases/alpha0.0.4.md)
- [alpha0.0.3（最新已发布，2026-11-23）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.3) — [发布说明](docs/releases/alpha0.0.3.md)
- [alpha0.0.2（2026-11-18）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.2) — [发布说明](docs/releases/alpha0.0.2.md)
- [alpha0.0.1（首个公开版，2026-11-17）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.1)
