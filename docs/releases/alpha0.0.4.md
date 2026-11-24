# RosPinyin alpha0.0.4

第四个公开版本。这一版只改**安装与分发形态**：新增自包含 GUI 安装器，把 `rospinyin.ime` + `rospinyin.dat` 压缩后内嵌进单个 exe，双击即可安装/卸载；输入法主体与中文标点行为与 alpha0.0.3 一致。

## 下载

| 文件 | 用途 |
|---|---|
| `install-rospinyin-gui.exe` | **新增·首选**：自包含 GUI 安装器，双击后点「安装」或「卸载」 |
| `install-rospinyin.exe` | 开发者/诊断用 CLI，`/step1..4` 分步安装；Windows 真机只用这条路径 |
| `rospinyin.ime` / `rospinyin.dat` | 输入法主体与 v2 码表，必须同版同装 |
| `rhash.exe` | guest 内 SHA-256 校验用 |
| `SHA256SUMS.txt` | **以本 tag 的实际资产为准**，下载后逐项核对 |

## 自包含 GUI 安装器

- 三个 payload（`rospinyin.ime`、`rospinyin.dat`、CLI 安装器）以 deflate 压缩后作为 RCDATA 编进 GUI exe；运行时释放到 `%TEMP%` 唯一子目录，解压后 SHA256 对比 + 长度 + CRC32 校验通过才安装，结束即清理。
- 安装完成后明确提示**重启系统**（IMM32 需重新加载布局与 `.ime`）；卸载完成也提示重启后生效。
- 安装按钮严格校验平台：当前白名单只有 `ProductName == "ReactOS"`（大小写不敏感）。Windows、`libernt` 等未登记平台会拒绝安装并弹提示；后续兼容分支必须先实机验证并显式登记。
- 卸载按钮不受平台闸门限制，便于误装后清理。
- CLI 版 `/step1..4` 行为不变，继续供开发者诊断；**Windows 真机安装只使用 CLI 分步方式**，GUI 不提供 Windows 安装入口。
- 红线不变：不写 Preload、不设默认输入法；GUI 只驱动 CLI 的既有 step1..4 实现，不另写注册表逻辑。

## 杀毒误报说明

未签名的开源安装器会释放文件并写注册表，可能被 SmartScreen 或杀软判为「未知发布者 / 可疑安装器」。这是**预期误报概率**，不是安装器缺陷；本版不承诺零误报，也不建议关闭杀毒软件。

请这样核验：

```bat
certutil -hashfile install-rospinyin-gui.exe SHA256
```

把输出与 `SHA256SUMS.txt` 逐位比对；一致仍被拦截时，可基于该 hash 与仓库源码/CI 记录添加白名单。发布资产由 GitHub Actions 从本 tag 构建，`INFO.txt` 记录 source commit 与 workflow run。

## `[全]` 模式字体（可选，HTTP 分发）

`[快]` 模式只显示 ReactOS 自带 Droid Sans Fallback 可绘制的候选，无需额外字体。
按 `Ctrl+Shift+G` 切到 `[全]` 后会放入扩展 A/B/C… 的生僻字。GUI 安装器在主界面常驻字体提示，
安装成功后也会再次提示下载位置。

**重要：当前 `[全]` 模式仍会有很多豆腐块，装了字体也不能完全消除。** 虽然安装器提示该模式需要
额外中文字体，但**输入法本身目前还不支持按字体渲染这些扩展区字形**——候选窗仍走系统默认 UI 字体，
装了文津宋体也不会让生僻字自动显示出来。字体提示只是告知「这是需要字体的模式」，不代表装上就能看见。
需要扩展区汉字可见的用户请把它当作**已知限制**而非解决方案；后续版本才会做候选窗的字体切换支持。

字体包**不随安装器附带、不进 GitHub Release**（约 57 MB），只放在本地 HTTP 分发目录：

```bat
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/SHA256SUMS.txt
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP0-Regular.ttf
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP2-Regular.ttf
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP3-Regular.ttf
```

- 三个 TTF 是**同一字体族按 Unicode 平面拆分**：P0 覆盖基本区+扩展 A，P2 覆盖扩展 B–F/I，
  P3 覆盖扩展 G/H；**三者缺一不可**，只装一个仍会在其他平面看到方框。
- 先用同目录 `SHA256SUMS.txt` 核对三个文件的 hash，再逐个安装字体，最后**重启系统**。
- 这是**补充字体，不替换 Droid Sans Fallback**；`[快]` 模式的保底绘制不受影响。
- 离线/公网取源见 `m0/share/fonts/wenjin-v2.100/SOURCE.txt` 记录的上游发布页
  （WenJinMincho v2.100，SIL OFL 1.1，许可全文在同目录 `LICENSE.txt`）。

## 实机验证（stable + nightly 双平台）

本版 GUI 安装器已在两个全新安装的 ReactOS guest 上实机验证，对照只差 OS 版本：

| 平台 | OS | 验证结果 |
|---|---|---|
| **stable** | ReactOS 0.4.16 正式版 | GUI 安装 → 重启 → 中文标点冒烟（`中文输入OK！……、《》`）→ GUI 卸载 → 重启，全程通过；输入法指示器「拼」/「EN」切换正确。 |
| **nightly** | 0.4.17-dev-1398-gdbbf9a2 | GUI 安装 → 重启 → 中文标点冒烟通过，中文正常渲染（非 `?` 方框）。 |

**nightly 用户注意：需自行安装 Droid 字体。** 上游 nightly 镜像（0.4.17-dev）已移除
`DroidSansFallback.ttf`，且 `font.inf` 不再注册它，但 UI 字体替换链仍指向该字体——结果是任何
中文界面（包括本安装器）都会显示成 `?` 方框。0.4.16 正式版自带该字体，无需处理。nightly 上
请先自行安装 Droid Sans Fallback 并重启，再运行安装器；字体二进制不随本 release 分发，可从
对应 ReactOS 0.4.16 镜像或可信来源取得后安装。

## 已知限制

- **`[全]` 模式仍有很多豆腐块**：输入法当前不支持按字体渲染扩展区字形，装了字体也不完全生效，详见上文。
- nightly（0.4.17-dev）缺 Droid 字体需自行补装，否则所有中文界面显示 `?`，详见上文。
- `.ime` 与 `.dat` 必须同源同版；v1 dat 完全不兼容。
- 中文标点行为与 alpha0.0.3 相同。
