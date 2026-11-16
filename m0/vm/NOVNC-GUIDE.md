# ReactOS M1 分步注册 — noVNC 人机协作操作单

> **v7（2026-11-16）**：新增 **Alt+Shift 切换实测结论**——在此 ReactOS 0.4.16 VM 上，
> `Alt+Shift` 可**可靠切换**任务栏输入法指示器：红色「中」字图标 ↔ `EN`，托盘菜单
> 显示 `中文(简体) - RosPinyin` 带勾选，确认切换的是真实 IME 布局。而 `Ctrl+Space`
> 不可靠（M1-2.4 压测）。**结论：本项目用 Alt+Shift 作为切换热键**（对应 M3 WBS 4.4
> 的 CORE-19455 备用方案）。证据见 `m0/vm/evidence/m1-altshift/`。
>
> **v6（2026-11-16 第五轮，托盘图标修复已在 VM 实测确认）**：三件事。
>
> 1. **实测通过**：新内嵌图标的 `rospinyin.ime` 已通过 guestOS 命令行下载、覆盖、
>    重启，用户人工确认任务栏托盘出现「中」字图标。证据见
>    `m0/vm/evidence/m1-2.5/`。
> 2. **纠正路径**：系统目录是 **`C:\ReactOS\system32`**，不是 `C:\windows\system32`
>    （`echo %SystemRoot%` 实测为 `C:\ReactOS`）。务必用前者覆盖，否则 copy 报
>    `The system cannot find the path specified.`。
> 3. **noVNC 键位坑**：Playwright `keyboard.type()` 经 noVNC 会**丢掉 Shift 修饰键**，
>    `:`（Shift+;）变 `;`、`>`（Shift+.）变 `.`、`%`（Shift+5）变 `5`、大写字母
>    变小写——这正是 `http://` 被下载器判为非 URL 的元凶（屏幕上 `:` 与 `;` 在
>    点阵字体下几乎无法区分）。自动化时须**逐字符显式按 Shift**：
>    ```python
>    def type_exact(page, s):
>        for ch in s:
>            if ch == ':':   # Shift+;
>                page.keyboard.down('Shift'); page.keyboard.press('Semicolon'); page.keyboard.up('Shift')
>            elif ch == '%': # Shift+5
>                page.keyboard.down('Shift'); page.keyboard.press('Digit5'); page.keyboard.up('Shift')
>            elif ch == '\\': page.keyboard.press('Backslash')
>            elif ch == '/':  page.keyboard.press('Slash')
>            elif ch.isupper():
>                page.keyboard.down('Shift'); page.keyboard.press(ch.lower()); page.keyboard.up('Shift')
>            else: page.keyboard.type(ch)
>    ```
>    另外连续两个 `/` 用 `type('//')` 会被合并成一个，须用两次独立 `press('Slash')`
>    并在中间加延迟。相关脚本：`~/.workbuddy/tmp/novnc_exact_{dl,copy}.py`。
>
> **v5（2026-11-16 第四轮）**：两件事。
>
> 1. **托盘图标修复**：`rospinyin.ime` 现在内嵌 16×16 红色「中」字图标
>    （`RT_GROUP_ICON`）。原因：ReactOS kbswitch 对 IME 布局不画 `[CH]` 字母
>    徽章，而是从 `.ime` 里 `EnumResourceNames(RT_GROUP_ICON)` 提取图标；旧版
>    没有图标资源，所以托盘只有系统绿色铅笔开关、没有布局图标。CI 已加图标
>    资源断言防止回归。
> 2. **新增 guestOS 下载工具**：HTTP 共享里放了 `wget.exe` / `curl.exe`（同一
>    个全静态 MinGW i686 程序的两个名字），只依赖 KERNEL32/msvcrt/WS2_32，
>    可在裸 ReactOS 0.4.16 上直接跑。**guestOS 内的下载/浏览器操作由你本人执行**，
>    我只准备工具和 URL，不远程点浏览器。
>
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

## 本轮操作（v6，已在 VM 实测完成）：让托盘显示「中」字图标

目标：把内网嵌图标的新 `rospinyin.ime` 覆盖掉 system32 里的旧版，让任务栏
显示红色「中」字图标（而不是空白/只有绿色铅笔开关）。

1. guestOS 命令行下载（wget 已在 PATH）：
   ```
   wget http://10.0.2.2:8000/rospinyin.ime -o rospinyin.ime
   ```
   成功标志：`done: 31439 bytes written to rospinyin.ime`。
2. 覆盖 system32（注意是 **`C:\ReactOS`** 不是 `C:\windows`）：
   ```
   copy /y rospinyin.ime C:\ReactOS\system32\
   ```
   成功标志：`1 file(s) copied`；`dir C:\ReactOS\system32\rospinyin.ime` 显示 31439 字节。
3. **重启**（`shutdown -r -t 0` 或开始菜单 → Shutdown → Restart），让 kbswitch
   重新从 .ime 加载图标。
4. 回到桌面后看任务栏托盘：应出现红色「中」字方块图标。**此步已由用户人工确认。**

> 证据：`m0/vm/evidence/m1-2.5/`（download.png / copy.png / desktop_after_reboot.png /
> tray_zoom.png / systemroot_probe.png）。

> 如果之前 step1-4 已完成，注册表无需再动，只换 .ime + 重启即可。
>
> 早期 v4 的重跑 step2 命令仍保留作参考（用新 wget 亦可）：
> ```
> del install-rospinyin.exe rospinyin.ime
> wget http://10.0.2.2:8000/install-rospinyin.exe
> wget http://10.0.2.2:8000/rospinyin.ime
> install-rospinyin.exe /step2
> ```
>
> 参考核对：新 exe 约 87 KB（MSVC 版 125 KB、最早的 dynamic-CRT 版 16 KB），
> 新 ime 约 31 KB。`/step2` 会找到已注册的 HKL（`e0210804`）并用正确的中文串
> 覆盖 Layout Text。任务栏若显示方块「□□□」说明是 CJK 字体缺失（已备好 Noto
> 字体子集），告诉我下一步装字体。

## 连接

浏览器打开（任选其一，宿主机 :6080 为 noVNC 服务）：

```
http://192.168.0.205:6080/vnc.html?autoconnect=1&resize=remote
http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=remote
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

## 第 1.5 步（新）：先把 wget / curl 下载到 guestOS

裸 ReactOS 没有 wget/curl/Firefox 命令行下载能力。先把工具本身弄进去
（这一步需要你在 guestOS 里用浏览器打开下载链接——按约定由你本人操作）：

浏览器访问（本机 noVNC 地址）：
```
http://192.168.0.205:6080
```
如需直接下载文件，可访问下面两个 URL（浏览器会另存到「My Documents」或下载目录）：
```
http://10.0.2.2:8000/wget.exe
http://10.0.2.2:8000/curl.exe
```
把下载到的 `wget.exe` / `curl.exe` 放到你会用的工作目录（例如与 installer 同目录）。
之后在 cmd 里就能用它们下载了，支持：
```
wget http://10.0.2.2:8000/rospinyin.ime
wget http://10.0.2.2:8000/rospinyin.ime -O rospinyin.ime
curl -o rospinyin.ime http://10.0.2.2:8000/rospinyin.ime
```
（只支持 http://，不支持 https://；这是本地测试共享，够用。）

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
