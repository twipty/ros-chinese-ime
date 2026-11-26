# RosPinyin

ReactOS 的开源简体中文拼音输入法 · [文档地图](docs/README.md) · [项目总览](docs/projectoverall.md)

## 项目介绍

RosPinyin 以独立 IMM32 输入法模块 `rospinyin.ime` 和码表 `rospinyin.dat` 分发，在 ReactOS 0.4.16+（i386）上提供开箱即用的全拼中文输入，填补 CORE-19268 中「Chinese IME」的缺口。代码采用 MIT 许可证，数据与第三方组件的授权见 `NOTICE.txt`。产品形态固定为**独立第三方软件包**。

当前主线能力：连续拼音输入、候选窗与翻页、空格/数字选字、`v` 键输入 `ü`、退格与 Esc 编辑、单独按 Shift 切换中英文、中文标点、`[快]/[全]` 字形模式，以及可选的 CC-CEDICT 词组库。0.4.16 i386 真机基础验收 15/15 通过；GUI 安装器在 stable + nightly 双平台完成安装/卸载验收。

## 使用说明

1. 从 Release 下载 `install-rospinyin-gui.exe`（自包含，双击安装/卸载）；诊断时用 `install-rospinyin.exe /step1..4`。GUI 内嵌 `rospinyin.ime` + `rospinyin.dat`，运行 `/status` 可核对 HKL、载荷与字体状态。
2. GUI 安装器只在 `ProductName == "ReactOS"` 时放行；Windows、`libernt` 等未登记平台会拒绝安装。Windows 真机只走 CLI `/step1../step4`。详见 [Windows GUI 兼容评估](docs/reviews/WINDOWS-GUI-COMPATIBILITY-ASSESSMENT.md)。
3. 手工路径的系统目录是 `C:\ReactOS\system32`（不是 `C:\Windows\system32`）：把 `rospinyin.ime` 与 `rospinyin.dat` 放进去，注册布局后重启，用 `Alt+Shift` 切到「中文(简体) - RosPinyin」。
4. 输入拼音：空格/数字选字，`=`/`.` 下一页、`-`/`,` 上一页，`v` 代替 `ü`，Backspace 回退，Esc 取消。单独按一下 `Shift` 切换中英文；`Shift+A` 仍是大写 A。
5. **`[快]` / `[全]` 字形模式**：`[快]` 只显示系统保底字体可绘制的候选；`[全]` 显示全量候选并按码点平面切换文津三片字体渲染扩展区字形（候选窗与编码窗）。`Ctrl+Shift+G` 切换，仅对当前会话生效。**必须三字体齐备才能正常显示，缺少任意一个的后果不可预知**；请从[文津官方发布页](https://github.com/takushun-wu/WenJinMincho/releases/tag/v2.100)下载 TTF 7z 包，校验 SHA256 后安装 P0/P2/P3 三片并重启。该字体不随安装包或 Release 分发；安装器弹窗内写死的 `http://10.0.2.2:8000/...` 是 guest 测试地址，公网用户以官方发布页为准。
6. **nightly（0.4.17-dev）Droid 字体**：nightly 移除了 `DroidSansFallback.ttf`，中文界面会显示 `?`。可选字体包的制作与安装见 [tools/droid-font-pack/README.md](tools/droid-font-pack/README.md)；原版字体哈希固定、不入库、不替换系统已有字体，`/status` 可查状态、`/u` 对称清理。0.4.16 stable 不需要。
7. **词组库（M4，内嵌必装）**：`rospinyin-lexicon.dat` 由 CC-CEDICT 构建（CC-BY-SA），已内嵌 GUI/CLI 安装器并写入 `system32\`，按固定 SHA-256 校验。`nihao` 首选「你好」、`jisuanji` 首选「计算机」；词库缺失/损坏时 IME 拒绝加载（不产候选、不崩溃，v0.1 存量用户为破坏性升级）。构建、许可与部署见 [tools/mklex/README.md](tools/mklex/README.md)。

> M4 已落地（2026-11-26）：词库内嵌安装器（第 4 payload，因 windres O(n²) 拼接拆为 8 段 RCDATA，ID 104–111）+ 固定 SHA-256 校验 + 四类失败拒绝加载；宿主 24/24、`check_m1` 120/120，交叉编译新 GUI 3.44 MB。guest 实机截图待 owner 验收回填。

> 杀毒误报：未签名安装器可能被 SmartScreen/杀软提示「未知发布者」。请用 `SHA256SUMS.txt` 核对下载文件，不要关闭杀软；详见 [alpha0.0.4 发布说明](docs/releases/alpha0.0.4.md)。

## Release

- [全部版本](https://github.com/twipty/ros-chinese-ime/releases)
- [v0.1.0 正式发布说明](docs/releases/v0.1.0.md)（tag 已推送；CI 正式 release 已生成，7 个资产）
- [alpha0.0.4（GUI 安装器候选，已发布 prerelease）](docs/releases/alpha0.0.4.md)
- [alpha0.0.3（2026-11-23）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.3) — [发布说明](docs/releases/alpha0.0.3.md)
- [alpha0.0.2（2026-11-18）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.2) — [发布说明](docs/releases/alpha0.0.2.md)
- [alpha0.0.1（首个公开版，2026-11-17）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.1)
