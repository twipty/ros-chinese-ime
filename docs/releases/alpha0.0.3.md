# RosPinyin alpha0.0.3

第三个公开版本。这一版把 **M3 的中文标点体验补齐并逐键实机验收**：标点表从「机械全角化」收敛为「只留真中文标点」，补齐 shift 键码映射与多码点上屏，引号按会话交替，并在 ReactOS 0.4.16 guest 上手打逐键确认。

## 下载

本版**以安装器为首选路径**，资产由 CI 从本 tag 源码构建并附带 `SHA256SUMS.txt`（下载后用随包校验值核对，勿用旧版 hash）：

| 文件 | 用途 |
|---|---|
| `install-rospinyin.exe` | **首选**：`/step1..4` 分步安装，`/step1` 一次拷齐 `.ime + .dat` 并做 size 校验 |
| `rospinyin.ime` | 输入法主体（release 构建：`BUILTIN_TABLE=OFF` / `TRACE=OFF`） |
| `rospinyin.dat` | v2 码表，须与 `.ime` 同版本；**不可只装 `.ime`** |
| `rhash.exe` | guest 内校验用 |
| `SHA256SUMS.txt` | 以上资产的真实 sha256，以下载到的文件为准 |

> 兼容性提醒：dat **v1 完全不兼容**（`version != 2` 直接拒载）。`.ime` 与 `.dat` 必须同源同版发布，混用旧表会静默回退到内置数组、候选大幅变少。

## 安装（安装器优先）

在 ReactOS 0.4.16（i386）命令行：

```bat
install-rospinyin.exe /step1
install-rospinyin.exe /step2
```

`/step1` 把 `.ime + .dat` 作为同一带 size 校验的载荷拷贝，避免「装了 ime 没带 dat」的半安装态；`/step2` 注册布局并移除旧布局，完成后重启。系统目录是 **`C:\ReactOS\system32`**，不是 `C:\windows\system32`。

wget + copy 手工路径降级为诊断/开发机备选，步骤见 [HANDOVER §6.4](../handover/HANDOVER.md)。

## 这版做了什么

1. **中文标点表收敛**：`imfw::fullwidth_punctuation()` 从 24 键机械全角映射收敛为 20 键真中文标点。新增 `——`（U+2014×2）、`￥`（U+FFE5）、`、`（U+3001）、`·`（U+00B7 间隔号）；`"`/`'` 改为按 per-HIMC 会话 parity 交替 `“”`/`‘’`（支持嵌套、英文态与组合中不推进）；移除 `{ } / - + =` 六个纯全角形态，中文态回退系统半角。该表是 framework 层唯一一份，形码侧将来一行复用。
2. **shift 键码映射与多码点上屏**：新增 Windows-free 纯函数 `imfw::key_char(vk, shift)`，补齐中文态 shift 组合；标点接口改 `char32_t → std::u32string`，`Intent` 增加 `text` 字段承载 `……`/`——` 等多码点，避免双码点被 parity 截断。
3. **码表分级与 [快]/[全]**：删除 rare 概念，tier0 无字频字赋低权重 rank `1,000,000` 作为普通候选排末尾；`[快]/[全]` 唯一由 dat 的 tier 位决定，`Ctrl+Shift+G` 即时重算。dat 指纹 `db0c7bcf…`（1,080,224 B，44,355 字 / 418 音节 / 118 多音）。
4. **热键与状态**：`Ctrl+Shift+Space` 解绑为吞键 no-op（与单 Shift 重复），保留 `KeyKind::LanguageToggle` 供后续全角/半角专用；工具栏显示中英态与 `[快]/[全]`。主热键 `Ctrl+Space` 因上游 **CORE-19455** 在 ReactOS 不可靠，实测以 `Alt+Shift` 切换。

## 验收状态

- **宿主侧**：ctest 双配置（`BUILTIN_TABLE` ON/OFF）各 20/20；`check_m1` 111/111；`check_docs` 7/7；seams 调用点全在 cap；mkdat 端到端 OK。
- **guest 实机全项 PASS**：中文态 `……`/`？`/`《`、英文态 `^`/`?`/`<` 六键；引号交替 `‘’`×3 / `“”`×3；四个新键 `——`/`￥`/`、`/`·`。截图与逐键取证见仓库 `m0/vm/evidence/`（`m3-punct-accept-6lines.png`、`now-font-quotes-zoom.png`、`now-fourkeys-zoom.png`）。
- **平台层盲区闭合**：Droid Sans Fallback 缺弯引号字形，曾把码点画成斜纹；换文津宋体后实机确认四个引号码点与四个新键全部渲染正常——上屏码点一直正确，异常纯属字体字形缺失。

## 已知限制

- 全角/半角独立开关尚未实现，当前中文标点态跟随 fOpen；`[全]` 模式放行 tier1 字，能否显示取决于宿主字体覆盖。
- 内置表兜底已从 release 构建移除（空表异常态契约见 `tests/builtin_gate_test.cpp`），务必 `.ime` 与 `.dat` 同装。
- 仍在 alpha：M3 的 100 字段落实机验收、M4 词组与词频等尚未完成。
