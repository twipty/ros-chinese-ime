# RosPinyin alpha0.1.1 — M4 词组库收口预发布

本版基于 v0.1.0 之后的 M4 收口提交。输入法主体与 v2 单字码表**未变**；本版只修正词组库的部署与加载：GUI 安装器内嵌词库必装、安装期与运行期双重 SHA-256 校验、加载失败不再静默退回单字。

## 下载与校验

| 文件 | 用途 |
|---|---|
| `install-rospinyin-gui.exe` | **首选**：自包含 GUI 安装器，双击安装/卸载；词库已内嵌，无需单独下载 |
| `install-rospinyin.exe` | 诊断 CLI：`/step1..4`、`/status`、`/u`、`/fontinstall <DROID.TTF>`；**不内嵌词库字节** |
| `rospinyin.ime` / `rospinyin.dat` | 输入法主体与 v2 单字码表，必须同源同版 |
| `rospinyin-lexicon.dat` | 词组库：**仅手工 CLI / wget+copy 诊断路径需要**；正常 GUI 安装不需要 |
| `rhash.exe` / `SHA256SUMS.txt` | 校验工具与本 tag 实际资产的 SHA-256，下载后逐项核对 |

GUI 安装器只在 ReactOS 上放行；Windows 真机请用 CLI `/step1..4`。词库资产为 **6,530,601 字节**，SHA-256 `a89ec5e5…c1c6a`（109,428 词，源自固定哈希快照的 CC-CEDICT，CC-BY-SA）。

## 词组库部署（本版核心）

- GUI 安装器把 IME / DAT / 词库 / CLI 四个 payload 压缩后内嵌；词库因 windres 单宏拼接性能问题拆为 **8 段 RCDATA（ID 104–111）**，运行时拼回后逐段校验 CRC32 与整体 SHA-256。
- `/step1` 按 **ime → dat → lex** 顺序写入 `C:\reactos\system32\`，三文件均做源/目标 size + SHA-256 比对；任一步失败即整体失败，不会留下「只有单字」的半安装。
- 运行期四类失败（缺失 / 读取失败 / 结构损坏 / 哈希不符）时 `load_system_table()` 返回 `nullptr`，release 构建**不回退内置表、不产候选**，而不是静默退回单字模式。
- `/status` 显示词库状态与哈希；`install-rospinyin.exe /u` 卸载时一并清理词库。
- **手工 CLI 路径必须把词库与 CLI 放在同一目录**：CLI 从自身所在目录读三个源文件；GUI 一键路径不需要，因为词库已由 GUI 释放到临时目录。

## 安装与升级

1. 运行 `install-rospinyin-gui.exe` 按提示安装；或按 `tools/install-rospinyin/README.md` 走 CLI 分步安装。
2. 安装完成后**重启系统**（IMM32 需重新加载布局与 `.ime`）。
3. `Alt+Shift` 切到「中文(简体) - RosPinyin」，记事本输入 `nihao` → 「你好」、`zhongwen` → 「中文」验证词组候选。
4. 卸载用 GUI「卸载」按钮或 `install-rospinyin.exe /u`；安装器只清理自己登记的内容，系统自带字体不会被删除。

**破坏性升级提醒**：v0.1.0 及更早的安装没有词库，覆盖安装后必须重启；若词库未正确写入，IME 将不产候选而非退回单字。owner 已接受该行为。

## 字体依赖（可选，`[全]` 模式）

`[快]` 模式只显示 ReactOS 自带 Droid Sans Fallback 可绘制的候选，无需额外字体。`Ctrl+Shift+G` 切到 `[全]` 后会放入扩展区生僻字，按码点平面使用文津明朝三片字体（P0/P2/P3）渲染，**必须三片齐备，缺少任意一个的后果不可预知**。

字体包不随本 release 分发，请从[文津官方发布页](https://github.com/takushun-wu/WenJinMincho/releases/tag/v2.100)下载并逐片校验后安装，重启再切 `[全]`；详见 `docs/releases/alpha0.0.4.md`。nightly 0.4.17-dev 上游镜像缺少 `DroidSansFallback.ttf`，中文界面可能显示 `?`，可选 Droid 字体包见 `tools/droid-font-pack/README.md`；stable 0.4.16 无需处理。

## 已知限制

- 词组匹配仍是**整词精确查表**：`zhongwen` 命中「中文」，继续输入 `shurufa` 后整词 key 未命中，候选窗回退首音节单字，不会拼出「中文输入法」整句。M5 的 Viterbi 整句解码尚未开工；当前请逐词上屏。
- 「组合非空时按 Enter 以英文字母原文上屏」是 M4 同阶段追加需求，独立排在 M5 前，本版**尚未包含**。
- 未签名安装器可能被 SmartScreen / 杀软判为「未知发布者」，属预期误报；请先按 `SHA256SUMS.txt` 核验，不要关闭杀毒软件。
- GUI 弹窗中的 `http://10.0.2.2:8000/...` 是 guest 测试地址，不是公网下载地址。
