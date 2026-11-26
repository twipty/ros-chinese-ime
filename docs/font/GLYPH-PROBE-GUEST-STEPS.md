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
rhash --sha256 C:\glyph-probe.exe
```

核对 SHA256（当前构建，2026-11-26 P1 实现版：平面映射改为转发 framework 单一真相源，
测试点仍为已分配码点 U+2A700/U+2B740；**探测行为与上一版完全一致**，仅链接的头文件来源变了）：
`ce5f70a96bc8756512df9c460d6d8b516d126d7f5215d0db53bba1b1ac772466`

> 上一版 `2bf04d6682af0a0e9a8ee5e68becbfb0f7a0ce2b5fb67d7d7cd519204f1fa215`
> 的测试点 U+2A6E0/U+2B81E 落在 Unicode 未分配区，那两行 NONE 无意义、无法回答 ExtC 覆盖。
> 如果你手上是上一版哈希，请重新下载本版重跑。

> **重要：非 BMP 码点不能靠 GetGlyphIndicesW 检测。** 上一版虽已用 UTF-16 代理对，
> 但仍用 `GetGlyphIndicesW` 查询——ReactOS 源码明确注释该函数**不支持代理对**
> （逐 WCHAR 拆分查高/低代理项），导致所有扩展 B/C 格再次显示假 `NONE`。
> 另外 advance width 也无效：文津是等宽字体，所有字形（含 notdef）宽度都是 1000。
> 当前版改用**像素渲染对比**：用同一字体把码点和 U+10FFFF（非字符，任何字体都没有）
> 画到离屏位图比像素（ReactOS `TextOutW` 支持代理对）。结果文件头有一行
> `pixel_selftest=OK/FAIL`——**必须看到 OK**，否则说明离屏渲染不可靠、非 BMP 数据无效。
> 若你之前跑出的矩阵里 WJ-P2/WJ-P3 扩展区全是 NONE，那是旧版假阴性，请用新版重跑。

> **如果双击报「缺少 DLL / 无法启动」**：那一定拿到旧版了。旧版动态链接了
> `libgcc_s_dw2-1.dll` + `libstdc++-6.dll`（MinGW 的 C++ 运行时），ReactOS 系统里
> 没有这两个 DLL。当前版本已用 `-static` 静态链接，只依赖系统自带的
> `GDI32/KERNEL32/msvcrt/USER32`（与已实机跑通的安装器 GUI 完全一致）。
> 重新下载并用上面的 SHA256 核对即可。这不涉及任何相对路径问题——结果文件
> 路径由 `GetModuleFileNameW` 取 exe 所在目录，是绝对路径。

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

### 第 4 步：回传证据（四种方式任选）

结果文件是纯文本（约 1 KB），四种方式任选其一即可。截图**都要发**
（要能看清每列字符和 HAS/NONE，用于判断字形是否真是方框）。

**方式 A · 直接贴文本**：记事本打开 `C:\glyph-probe-result.txt`，
`Ctrl+A` → `Ctrl+C`，粘贴到 WorkBuddy 对话框发给我。

**方式 B · FTP 上传（全自动，推荐）**：宿主已起 FTP 接收服务（匿名可写，
`10.0.2.2:2121`，被动端口 60000–60099）。HTTP 目录挂了一个极简上传器
`ftpput.exe`，下载后双击即把 `C:\glyph-probe-result.txt` 传到宿主，弹窗显示 OK 即成功：

```bat
cd \
wget http://10.0.2.2:8000/ftpput.exe
rhash --sha256 C:\ftpput.exe
ftpput.exe
```

核对 SHA256：`7fd17575c01c844b8ec3bea977072f3d9efbad43ef554696af6a6518e45f5697`。
双击 `ftpput.exe`（或命令行直接敲 `ftpput.exe`）——它读 `C:\glyph-probe-result.txt`，
连 FTP 服务上传；成功弹 `OK - uploaded`，失败弹具体原因（连不上 / 读不到文件等）。
上传后**告诉我一声**，我去宿主 `m0/vm/uploads/glyph-probe-result.txt` 取回并核对哈希。
- 传别的文件：`ftpput.exe "C:\other\file.txt"`（远程文件名取 basename）。
- 需要先有结果文件（第 3 步已生成）；上传器不依赖字体，纯 WinSock，静态链接，
  只导入 `KERNEL32/USER32/WS2_32/msvcrt`（ReactOS 自带），已实机验证协议序列。
- 若弹「Cannot connect to 10.0.2.2:2121」：宿主 FTP 服务没起，跟我说我重启它。

**方式 C · 串口重定向（无需网络）**：VM 已把 guest 的 COM1 映射到宿主文件
`m0/vm/logs/rosdbg.log`。在 guest 命令行执行：

```bat
type C:\glyph-probe-result.txt > COM1
```

然后告诉我一声，我直接读宿主那份日志。注意这是**追加**写，重跑前我会先清空。

**方式 D · HTTP 上传**：仅 wget 不支持 POST，需我在宿主另起接收端点；
方式 B 已覆盖此需求，通常不必用。

无论哪种方式，都请同时回传结果文件的 SHA256，便于核对是否传完整：

```bat
rhash --sha256 C:\glyph-probe-result.txt
```

## 数据怎么判读（AI 会据此回答 Q1/Q2）

| 观察 | 结论 | 后续 P1 路线 |
|---|---|---|
| `Plane 2` 列在 U+20000/U+2A700/U+2B740 行是 `HAS`，截图非方框 | Q1 成立：单族名可画本平面字形 | 候选窗可按平面加族名到字体链 |
| U+2A700（ExtC 起点）/ U+2B740（ExtD 起点）行全 `NONE` | 文津 P2 真实缺少 ExtC/ExtD 覆盖 | 另开 S0 评估补字体，不在本轮范围 |
| `Plane 0` 列在 U+20000 行也是 `HAS` | Q2 成立：GDI 自动跨族名回退 | 最小改法——只把文津族名加进 `kFontFaces` |
| `Plane 0` 列在 U+20000 行是 `NONE` | Q2 不成立：必须按平面分别选字体 | 按码点分段 `SelectObject` 自绘（改动更大，另开 S0） |
| 全部 `API?` 且截图全是方框 | GDI 探测 API 与回退都不可用 | 需换方案（如字体二进制嵌入 + 自绘），另开 S0 |

拿到你的截图 + 结果 txt + 哈希后，我会把数据落到 S2 报告，回答 Q1/Q2，
再决定后续实现路线（并重新走 S0→S1 确认）。
