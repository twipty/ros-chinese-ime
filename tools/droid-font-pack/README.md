# 可选 Droid 字体包（仅 ReactOS nightly）

## 为什么需要

ReactOS 0.4.16 正式版自带 `DroidSansFallback.ttf`；0.4.17-dev nightly 移除了该文件与 `font.inf` 注册项，但 UI 字体替换链仍指向这个字体族。结果：包括 RosPinyin GUI 安装器在内的中文界面全部显示 `?` 方框，`[快]` 模式的保底字形也随之失效。

这是 OS 发行版差异，**不是安装器缺陷**。本包是独立、可选的分发物：不进 Git 仓库、不随 GUI 安装器内嵌、不作为 GitHub Release 资产、不替换系统已有字体。

## 原版字体约束

| 项 | 值 |
|---|---|
| 文件名 | `DroidSansFallback.ttf`（guest 侧固定为 8.3 名 `DROID.TTF`） |
| 大小 | 3,081,908 字节 |
| SHA-256 | `82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf` |
| 许可 | Apache-2.0（ReactOS 0.4.16 随附版本） |

来源：ReactOS 0.4.16 官方 ISO 的 `Fonts` 目录，或从该 ISO 取出的可信副本。不要从不明站点取同名文件；哈希不符时脚本直接拒绝打包。

## 宿主侧制作

```bash
python3 tools/droid-font-pack/build_pack.py \
  --ttf /path/to/DroidSansFallback.ttf \
  --out /tmp/droid-font-pack.zip
```

产物含 `DROID.TTF`、`DROID.REG`、`UNREG.REG`、`README.TXT`、`SOURCE.TXT`、`SHA256SUMS.TXT`，时间戳固定、可复现。zip 与原字体都不入库。

## guest 侧安装（用户手工操作）

1. 下载 zip 后用 `rhash --sha256 DROID.TTF` 与 `SHA256SUMS.TXT` 核对。
2. 方式 A（推荐）：把 `DROID.TTF` 放到 `C:\`，命令行执行
   `install-rospinyin.exe /fontinstall DROID.TTF`。工具校验大小后写字体目录、
   注册 `Fonts` 值、写入所有权标记，并提示重启。
3. 方式 B（手工兜底）：复制 `DROID.TTF` 到 `C:\ReactOS\Fonts\`，用 regedit 导入
   `DROID.REG`，重启。该 reg 同时写入所有权标记，保证后续 `/u` 能对称清理。
4. 重启后运行 `install-rospinyin.exe /status`，确认：
   `droid font : present (installed by RosPinyin font pack)` 与
   `droid font reg: present`；GUI 中文不再是 `?`，`[快]` 模式无方框。

**不替换原字体**：若系统已有该文件且未标记为本包安装，`/fontinstall` 拒绝覆盖并返回校验失败码；stable 0.4.16 不需要也无需安装本包。

## 清理

`install-rospinyin.exe /u` 只在所有权标记存在时删除字体文件、`Fonts` 注册值和标记，并打印 `optional Droid font pack removed`；否则打印 `Droid font left untouched`，系统字体保留。手工路径可用 `UNREG.REG` 清理注册表，仍需自行删除字体文件。

## 验证范围

- 6081 nightly：安装本包 → 重启 → GUI 中文非 `?`、`[快]` 模式无方框。
- 6080 stable（0.4.16）：**不安装本包**，确认原有中文显示与安装/卸载流程无回归。
- guest 上的复制、导入与重启均由用户手工完成，宿主侧不做键鼠注入。
