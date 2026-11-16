# ReactOS M1 分步注册 — noVNC 人机协作操作单

> **v4（2026-11-16 第三轮）**：构建工具链从 MSVC 切换为 **MinGW-w64 i686**，
> 彻底解决 `the procedure entry point initializecriticalsectionex could not be
> located in the dynamic link library kernel32.dll`。
> 根因：ReactOS 0.4.16 的 `kernel32.dll` 在构建期就排除了这个 Vista+ 导出，
> 而 MSVC 的静态 CRT 偏偏要导入它——装多少 MSVC 运行库都补不上。
> MinGW 产物只依赖 kernel32 / user32 / advapi32 / imm32 / msvcrt（全是 ReactOS
> 自带系统 DLL），所有导入符号已逐项对照 ReactOS 0.4.16 源码核实；CI 已加
> 导入表断言防止回归。
>
> v3（同日）：修复 kbswitch 乱码——MSVC 在 en-US CI 上把无 BOM 的 UTF-8 源码按
> CP1252 编译，中文串在编译期就坏了（改用 GCC 后从根上不再有此问题）。
> v2（同日）：修复 vcruntime140.dll 缺失。

## 本轮操作：重新下载 MinGW 版并重跑 step2（无需重启）

在现有 cmd 里逐行执行：

```
del install-rospinyin.exe rospinyin.ime
wget http://10.0.2.2:8000/install-rospinyin.exe
wget http://10.0.2.2:8000/rospinyin.ime
install-rospinyin.exe /step2
```

- 新 exe 约 **87 KB**（v3 的 MSVC 版 125 KB、最早的 dynamic-CRT 版 16 KB），可用 `dir` 核对。
- 这条 `/step2` 会找到之前注册的 HKL（`e0210804`）并**用正确的中文串覆盖 Layout Text**，
  同时不再使用那个坏掉的 MSVC 产物。
- 任务栏语言指示器应随即刷新。两种结果都有意义：
  - 显示「中文(简体) - RosPinyin」→ 入口点与编码问题都解决，进入下面第 3 步流程；
  - 显示方块「□□□」→ 串已正确但 ReactOS 没有 CJK 字体（Tahoma 无汉字字形），
    告诉我，我已备好 Noto 字体子集，下一步装字体。

## 连接

浏览器打开：

```
http://192.168.0.205:6080/vnc.html?autoconnect=1&resize=remote
```

看到 ReactOS 0.4.16 桌面（800x600，左列一排图标 + 底部任务栏）即连接成功。

## 第 0 步：打开命令提示符

双击桌面左侧列最下方的 **Command Prompt** 图标，出现黑色控制台窗口。

## 第 1 步：删除旧文件并下载新版（在 cmd 里逐行执行）

```
del install-rospinyin.exe rospinyin.ime
wget http://10.0.2.2:8000/install-rospinyin.exe
wget http://10.0.2.2:8000/rospinyin.ime
```

成功标志：两行都出现 `saved as ...` 或类似提示，无 404/超时。
新 exe 约 87 KB、ime 约 30 KB，可用 `dir` 确认。

## 第 2 步：安装器自检

```
install-rospinyin.exe /status
```

预期：打印文件 / HKL / 布局键状态。因为之前跑过 step2，HKL 应显示 `e0210804`。

## 第 3 步：分步注册（每步之后都要重启一次）

> 每一步执行完后，在 cmd 里输入 `exit` 关窗口，然后用开始菜单
> Shutdown → Restart 重启。重启回到桌面后重新打开 cmd 继续。

| 顺序 | 命令 | 做什么 | 重启后确认什么 |
|------|------|--------|----------------|
| 1 | `install-rospinyin.exe /step1` | 复制 rospinyin.ime 到 system32 | 系统正常进桌面，无 0xF4 蓝屏 |
| 2 | `install-rospinyin.exe /step2` | ImmInstallIMEW 动态注册 | 正常进桌面；任务栏语言指示器可切换出「中文(简体) - RosPinyin」 |
| 3 | `install-rospinyin.exe /step3` | 写 Keyboard Layouts Layout Text | 正常进桌面，无异常 |
| 4 | `install-rospinyin.exe /step4` | 写 Layout Display Name | kbswitch 菜单里出现「中文(简体) - RosPinyin」并可切换 |

每步之间任何一次重启若出现 0xF4 蓝屏或卡死，立即停下告诉我。

## 反馈方式

每完成一步，直接在这里告诉我「step N 的输出是 ...」或「重启后桌面正常」，
我会用 HMP screendump 抓图核对，并随时排查异常。
