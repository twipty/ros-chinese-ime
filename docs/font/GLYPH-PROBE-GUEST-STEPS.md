# P1 验证：guest 内 GDI 字体回退探测（手工步骤）

> 用途：回答「装了文津宋体，`[全]` 模式候选窗能否显示扩展区字形」的前置问题。
> 本轮**只验证、不实现**——探测工具不改输入法任何代码。
> 纪律：guest 键鼠由用户手工操作，AI 不注入。本页就是要你在 guest 里做完的步骤。

## 背景（为什么要先做这个）

文津宋体 v2.100 的三个 TTF 是**三个不同族名**：

- `WenJinMinchoP0-Regular.ttf` → `WenJin Mincho Plane 0`（基本区 + 扩展 A）
- `WenJinMinchoP2-Regular.ttf` → `WenJin Mincho Plane 2`（扩展 B–F/I）
- `WenJinMinchoP3-Regular.ttf` → `WenJin Mincho Plane 3`（扩展 G/H）

候选窗当前字体链里没有这些族名。但**在改造候选窗之前**，必须先在真实 ReactOS GDI 上确认两件事：

- **Q1**：单个族名（如 `Plane 2`）能不能画出扩展 B/C 字形？
- **Q2**：GDI 会不会自动跨族名回退（用 `Plane 0` 族名画 `Plane 2` 的码点）？
  如果不会，候选窗就必须按码点平面分别选字体。

## 你要做的（stable guest，6080 或干净快照均可）

### 第 1 步：下载工具与字体

在 guest 里开命令行（`cmd`），依次执行：

```bat
cd \
wget http://10.0.2.2:8000/glyph-probe.exe
```

字体三件套如果 guest 里还没有，从本地 HTTP 取（注意：官方包是 7z，这里是已经解压好的裸 TTF）：

```bat
mkdir C:\fonts
cd C:\fonts
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP0-Regular.ttf
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP2-Regular.ttf
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/WenJinMinchoP3-Regular.ttf
```

校验（有 rhash 的话）：

```bat
wget http://10.0.2.2:8000/fonts/wenjin-v2.100/SHA256SUMS.txt
rhash -c SHA256SUMS.txt
```

三条都 OK 才继续。

### 第 2 步：安装三个字体

在资源管理器里进入 `C:\fonts`，**逐个**右键每个 TTF →「安装」（Install）。
三个都装完后**重启系统**（GDI 要重新枚举字体族名）。

> nightly（0.4.17-dev）注意：先按既有流程补装 Droid Sans Fallback 并重启，
> 否则包括本探测工具在内的中文界面都会显示 `?`。stable 0.4.16 自带 Droid，无需处理。

### 第 3 步：运行探测工具

重启后，双击 `C:\glyph-probe.exe`。会弹出一个窗口：

- 每**行**是一个测试码点（BMP 中字、扩展 A/B/D/G/H 边界）。
- 每**列**是一个候选字体族名（SystemDefault / WJ-P0 / WJ-P2 / WJ-P3）。
- 每格画出该字符，后面跟一个三态判定：
  - `HAS` = GDI 报告有字形；
  - `NONE` = GDI 报告无字形（大概率画成方框/`?`）；
  - `API?` = 该 OS 缺少 `GetGlyphIndicesW`，已降级到 UnicodeRanges 判断（此时看截图更准）。

工具同时在 exe 同目录生成 `glyph-probe-result.txt`（制表符分隔，可核对）。

### 第 4 步：回传证据

1. **截图**：把探测窗口整张截下来（要能看清每列的字符和 HAS/NONE）。
2. **结果文件**：把 `C:\glyph-probe-result.txt` 传回宿主（可放到
   `http://10.0.2.2:8000/` 下载目录，或用你习惯的方式交回）。
3. **哈希**：在 guest 里对结果文件算 SHA256 并一起回传：

```bat
rhash --sha256 C:\glyph-probe-result.txt
```

## 数据怎么判读（AI 会据此回答 Q1/Q2）

| 观察 | 结论 | 后续 P1 路线 |
|---|---|---|
| `Plane 2` 列在 U+20000/U+2A6E0 行全是 `HAS`，截图非方框 | Q1 成立：单族名可画本平面字形 | 候选窗可按平面加族名到字体链 |
| `Plane 0` 列在 U+20000 行也是 `HAS` | Q2 成立：GDI 自动跨族名回退 | 最小改法——只把文津族名加进 `kFontFaces` |
| `Plane 0` 列在 U+20000 行是 `NONE` | Q2 不成立：必须按平面分别选字体 | 按码点分段 `SelectObject` 自绘（改动更大，另开 S0） |
| 全部 `API?` 且截图全是方框 | GDI 探测 API 与回退都不可用 | 需换方案（如字体二进制嵌入 + 自绘），另开 S0 |

拿到你的截图 + 结果 txt + 哈希后，我会把数据落到 S2 报告，回答 Q1/Q2，
再决定后续实现路线（并重新走 S0→S1 确认）。
