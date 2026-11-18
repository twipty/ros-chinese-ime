# Preload 探针实验（三组对照）

> 目的：把 `0xF4` 的**投递**（Preload → csrss 加载）与**缺陷**（加载期做 UI 初始化 / 失败被吞）分开。
> 设计与判据见 `M0-REDLINE-REVIEW.md` §11。这里只写怎么做。
>
> 硬约束：**本项目禁止程序代替用户发送键盘/鼠标事件**。因此"写 Preload"和"关机重启"这两步必须人手完成，
> 脚本只负责起 VM、保覆盖层、收日志。

## 0. 三组，按顺序跑

| 组 | 用什么 `.ime` | 预期 | 目的 |
|---|---|---|---|
| **N** 负对照 | `alpha0.0.2` 发版二进制（`DllMain` 里仍 `RegisterClassEx`，TRACE=OFF，940,601 B） | **活**（原写"崩"，见下方修正） | 核验"平台确实写了 Preload、且每次登录都载入"，给发版二进制留一份带截图的记录 |
| **R** 拒绝路径 | 探针 `.ime`（`ImeInquire` 直接 `return FALSE`） | 活 | 验证 imm32 能接受 IME 主动拒绝 |
| **T** 处理组 | A1 build（`DllMain` 清空、注册移到 `ImeInquire`） | 活 | 验证修复 |

> **2026-11-18 17:30 修正：N 组的预期从「崩」改为「活」。**
> `/step2` 调的 `ImmInstallIMEW` 自己就写 `HKCU\Keyboard Layout\Preload`（`M0-REDLINE-REVIEW.md` §2.2 已更正），
> 而我们从第一次 `/step2` 起走的就是这条部署路径——N 组要制造的状态**本来就在**，并且随着每一次开机跑了半个多月，一次 0xF4 都没有。
> 所以 N 不再是"复现根因"的实验，而是**核验链条**：用 `reg query "HKCU\Keyboard Layout\Preload"` 把"里面确实有我们"
> 从推断变成截图证据——这正是 `check_m1.py` 第 7 项一直扫不到的那一格。若 N 反而崩了，说明中间有过我们不知道的变化，那才是新信息。
> R / T 两组的价值不变：R 验 imm32 能接受 IME 主动拒绝，T 验"加载期清零"这个修复。

**N 先跑**：它不需要改任何代码，信息量最大。若 N 不崩（现在的预期），说明缺陷不在"加载期注册类"本身——
**这个结果本身就能省掉一轮改代码**，同时把 §9.5 剩下的候选（`RegisterClassEx` 在什么时刻失败、`GlobalAddAtom` 的时机）压成"只在更严苛的加载者下才成立"。

## 1. 环境：`probe-n.sh`（已备好，可直接跑）

宿主机侧全部操作收敛到一个脚本，守卫写死，避免人手漏步：

```bash
VM=/home/twipty/WorkBuddy/2026-11-12-20-02-47/ros-pinyin/m0/vm
"$VM/probe-n.sh" state     # 覆盖层 / 5901 / 基础盘锁 / 串口日志
"$VM/probe-n.sh" prep      # 暂存被测文件 + 建覆盖层（旧 VM 在跑也能执行）
"$VM/probe-n.sh" start     # 起 VM（5901 空闲才允许）
"$VM/probe-n.sh" shot n-01 # 截屏 → evidence/preload-probe/N/n-01.png
"$VM/probe-n.sh" rollback  # 删覆盖层，基础盘零改动
```

已就位的现场：

| 项 | 值 |
|---|---|
| 覆盖层 | `/home/twipty/.workbuddy/tmp/reactos.probe-N.qcow2`（196,736 B，backing=`m0/vm/reactos.qcow2`） |
| 被测二进制 | `/home/twipty/.workbuddy/tmp/build-win-rel/ime/rospinyin.ime` = **940,601 B** `c9dc17c0…3e25feaa` |
| guest 下载路径 | `http://10.0.2.2:8000/probe-N/`（`rospinyin.ime` / `rospinyin.dat` / `install-rospinyin.exe`） |

**`start` 的三条守卫**（缺一条就会写坏已验收的环境）：

1. `5901` 被占用 → 说明另有一个 VM 在跑，先正常关掉它（noVNC 窗口会断开，地址不变）；
2. 基础盘被锁 → 覆盖层的 backing 正在被写，快照没有意义；
3. 覆盖层不存在 → 直接起 VM 会写基础盘。

**为什么必须换掉 guest 里那个 `.ime`**：`ROSPINYIN_TRACE=ON` 的构建会在 `DllMain` 里
`CreateFileA("C:\rospinyin-trace.log")` 并写一行 —— 那是**加载期的额外副作用**，
带着它跑 Preload 等于验另一个二进制（见 `M0-REDLINE-REVIEW.md` §11 A4）。
N 组的被测对象与 `alpha0.0.2` 发版资产**同一个文件**，这样结论直接适用于发布物。

## 2. N 组步骤（人手，约 10 分钟 + 最多 2 次重启）

> **2026-11-18 修订（两次实测教训）**
> 1. `m0/share/install-rospinyin.exe` 是 **11-16 的 M1 构建**，`/step1` 只拷 `.ime`、
>    `/status` 只打 `present` 不打字节数，payload 校验与 `.dat` 都是 `0bdee19` 之后才有的。
>    已换成当次构建（**245,224 B** `5dd2ad84…`），共享目录同步更新。
> 2. **`/step2` 会由平台替我们写 Preload**，所以「手工往 Preload 里加一项」这一步是错的，
>    见 §2.2。

### 2.1 先把被测二进制装进去

0. 进桌面后开 **Command Prompt**，进一个干净目录（例：`md C:\probe` / `cd C:\probe`）。
1. 下载（guest 里已有 `wget.exe` 就够；没有就先用浏览器从 `http://10.0.2.2:8000/wget.exe` 取）：
   ```
   wget http://10.0.2.2:8000/probe-N/install-rospinyin.exe
   wget http://10.0.2.2:8000/probe-N/rospinyin.ime
   wget http://10.0.2.2:8000/probe-N/rospinyin.dat
   dir
   ```
   `dir` 要看到 `940601 rospinyin.ime` / `902776 rospinyin.dat` / **`245224 install-rospinyin.exe`**。
   **字节数不对就别往下走**（下载截断比失败更危险）。旧 exe 是 `89394`，见到它就说明没覆盖成功。
2. 拷贝并校验（`.ime` + `.dat` 两份，逐个报字节数）：
   ```
   install-rospinyin.exe /step1
   ```
   正常输出两行 `[step1] ... verified (N bytes)`：`rospinyin.ime` 940601、`rospinyin.dat` 902776。
   若提示 `stale payload ... .old exists` 或 `reboot, then rerun /step1`：
   **重启一次，再跑一遍 `/step1`** —— 说明旧文件还被运行中的 IME 映射着，属预期。
3. 注册布局，拿 HKL：
   ```
   install-rospinyin.exe /step2
   ```
   输出 `[step2] ImmInstallIMEW returned HKL=XXXXXXXX`。**这一步就是投递**，它同时做三件事：
   写 `HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<HKL>`、
   写 `HKCU\Keyboard Layout\Preload`（平台自己写，不是我们）、返回动态 HKL。
   `/step2` 是幂等的：同 `LANGID` 会复用已有 HKL，Preload 里同值也不会重复追加。
4. 核对：
   ```
   install-rospinyin.exe /status
   ```
   必须看到 `rospinyin.ime : present (940601 bytes)`、`rospinyin.dat : present (902776 bytes)`、
   `recorded HKL : XXXXXXXX`、`layout key : present`。

### 2.2 确认 Preload 里已经有我们（只读，不要手写）

5. 打开 `regedit` → `HKEY_CURRENT_USER\Keyboard Layout\Preload`，**截图**。
   预期能看到 `1 = 00000409` 和一条**值为第 3 步那个 HKL** 的项（序号由平台挑，不一定是 `2`）。
   - **已经有了** → 什么都不用做，直接进 6。**不要再手工加一项**：同值加两遍没有意义，
     不同值等于凭空多注册一个 IME，会把对照搞脏。
   - **没有** → 说明 `/step2` 没走到写 Preload 那一支（或这套盘上布局早已存在、平台跳过了）。
     这时才手工追加一项，值为那个 HKL，**只追加到末尾**，并在记录表里注明「手工」。
6. **正常关机**（开始菜单 → 关机；不要直接断电，hive 需要落地）。
7. 启动 #1 → 能进桌面？能输入？（开记事本敲 `nihao` 出候选即可）
8. 启动 #2、#3 重复。**登录路径的问题常常第一次不出现**，三次都要。

### 2.2.1 现场记录（2026-11-18 17:24，`evidence/preload-probe/N/N-regedit-preload.png`）

`HKCU\Keyboard Layout\Preload` 实测（用户手工展开，宿主机 `probe-n.sh shot` 留证）：

| 序号 | 值 | 说明 |
|---|---|---|
| 1 | `00000409` | 美式键盘，系统自带 |
| 2 | `e0260804` | **我们自己的旧构建**：`rospinyin7.ime`（931,363 B）的布局（`HANDOVER.md` L753） |
| 3 | `e0200409` | **mzimeja**（ReactOS 的日文 IME）：`HANDOVER.md` L580–581 的 `WM_INPUTLANGCHANGE hkl=E0200409` 实证 |
| 4 | `E0210804` | 本次 `/step2` 注册，与 `/status` 的 `recorded HKL` 一致 |

三条结论：

1. **链条第一环成立**：`E0210804` 确实在表里，平台替我们写的，不是推断。加上 §0 的修正，
   这意味着登录路径上的 `LoadLibrary` 在过去每一次开机都发生过。
2. **这个盘上还住着另一个 IME**：第 3 项是 mzimeja。于是每次登录，除了 `rospinyin.ime`
   还会载入一个第三方 IME；Alt+Shift 的循环里也有它这一站（用户看到"`[中]` 却弹出 mzimeja 状态条"
   就是这个原因，不是我们的工具条画错了）。第 2 项则是我们自己更早的 `rospinyin7.ime`。
   → **负对照的环境不干净**，但方向是更严苛（多一个 IME 同时载入），不改判据。
3. `/u` 的清理缺口是真实存在的：`e0260804` 指向的旧构建从来没被清过。补 `/unpreload` 时要按
   "布局键的 `Ime File` 指向 `rospinyin*.ime`"来扫，**不能只删自己刚写的那一条**。
   而 mzimeja 那一条**不该由我们删**（不是我们写的，属于系统/用户的既有布局）。

**实测（18:16，`N-ime-dir.png`）**：`C:\ReactOS\System32` 里 `.ime` 共 9 个：

| 文件 | 字节 | 日期 | 说明 |
|---|---|---|---|
| `mzimeja.ime` | 587,072 | 11/12 02:09 | **与 M0 解剖那颗同尺寸**（`m0/extract/pe_0_0x76ff5a.bin` = 587,072 B） |
| `rospinyin.ime` | 940,601 | 11/18 00:52 | 发版二进制 |
| `rospinyin7.ime` | 967,775 | 11/17 20:57 | 旧构建，Preload 第 2 项（`e0260804`）多半指它 |
| `rospinyin2/4/5/6.ime`、`new2.ime` | 923,594 / 928,759 / 929,030 ×2 / 923,594 | 11/16–11/17 | 热替换退让链留下的中间产物 |

由此三点：

1. **不是悬空条目**：mzimeja 的 DLL 就在盘上，尺寸与 M0 崩掉那颗完全一致，极可能是同一个二进制。
2. 因此「mzimeja 躺在 Preload 里、机器照样开机」是**对"Preload 一载就崩"的反例**。
   用户 18:22 实测：`C:\Program Files\mzimeja` 的配置与词典**都还在**——所以不是"`Init` 提前返回"能解释掉的，
   它走的是和 M0 同一份安装、同一个 587,072 B 的 DLL，而机器一直在正常开机。
   → 剩下的区别只有**加载时机/加载者**：M0 那次它是安装后紧接着被激活（很可能还抢了系统默认输入语言），
   在桌面/窗口站就绪之前被载入；现在它只是 Preload 里的第 3 项，由 userinit 在桌面就绪之后载入。
   **这指向一个新假设：致命变量是"成为默认/最早加载"，不是"进入 Preload"。**（待验证，见 §3 之后的待办）
3. **`rospinyin7` 已由用户 18:22 在控制面板移除**：`HKLM\...\Keyboard Layouts\e0260804` 与 Preload 项同时消失，
   Preload 重新编号为 `1=00000409 / 2=e0210804 / 3=e0200409`（`N-after-remove.png`）。
   顺带证明：ReactOS 的控制面板**自己会清 Preload**——`/unpreload` 的价值因此变成"不依赖用户去控制面板点两下"，
   而不是"系统做不到"。
4. 顺手发现：System32 里躺着 **6 个历史 `rospinyin*.ime`**（约 4.7 MB），全是热替换退让链的中间产物。
   平时无害，但会让"改完 `.ime` 为什么没生效"这类排查更难认 → 并入 `/u` 的清理范围。

### 2.3 为什么「写 Preload」这一步从本组的动作里删掉了

不是省事，是**写不写不由我们决定**。ReactOS 0.4.16 的源码里两处写死在平台上：

- **写**：`win32ss/user/imm32/install.c` 的 `Imm32WriteImeLayout()`（L256–341）
  在写完 `HKLM\...\Keyboard Layouts\<HKL>` 之后，直接
  `RegCreateKeyW(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", ...)`，
  再 `RegSetValueExW(hkeyPreload, szPreloadNumber, ...)` 追加 `szImeKey`。
  入口就是 `ImmInstallIMEW` —— 我们 `/step2` 调的正是它。
- **读**：`win32ss/user/user32/windows/input.c` 的 `IntLoadPreloadKeyboardLayouts()`（L145 起）
  遍历 `HKCU\Keyboard Layout\Preload` 的 `1..1000`，逐项 `LoadKeyboardLayoutW(szValue, uFlags)`，
  首项带 `KLF_ACTIVATE`。调用点 `win32ss/user/user32/misc/logon.c`
  的 `UpdatePerUserSystemParameters()`（L104–111）。

所以红线 #2「部署过程不得写 Preload」在 ReactOS 上**从来没有被真正做到过**：
`check_m1` 第 7 项扫的是我们自己的源码，平台替我们写的那次不在扫描范围内。
这也意味着 N 组的条件很可能**在过去两天的每一次开机里都已经成立过**——
若第 5 步截图证实 Preload 里早就有我们的 HKL，那么 N 组要补的就只是
"换成发版二进制之后，再确认 3 次"。

### 记录表（复制到 issue 或直接填这里）

| 组 | 启动 | 进桌面 | 能输入 | 备注（蓝屏参数 / 卡在哪一屏） |
|---|---|---|---|---|
| N | #1 | ✅ 17:39（`N-boot1.png`） | ✅ 18:09（`N-imebar.png`：`你好`＋候选 `1/4`） | 托盘 `EN`→`中`；无蓝屏 |
| N | #2 | ✅ 18:33（`N-boot2.png`） | ✅ 18:35（`N-boot2-input.png`：`你好`＋候选 `1/4`） | 托盘 `EN`；列表 3 项（`rospinyin7` 已不在）；无蓝屏 |
| N | #3 | ✅ 18:38（`N-boot3.png`） | ✅ 18:45（`N-boot3-input.png`：`你好你好`＋候选 `1/4`） | 托盘 `EN`；无蓝屏；系统时间 10:45 AM（=UTC，非本地） |

**判据（写死，不许事后放宽）**：连续 3 次进桌面且能输入 = 活；**任何一次蓝屏 = 崩**，
立刻停手，把蓝屏参数抄进备注（`0xF4(` 后四个十六进制数），然后 `probe-n.sh rollback`。

蓝屏时用手机拍屏或宿主机 `probe-n.sh shot n-bsod` 留一张（`screendump` 在 bugcheck 画面上仍然有效，
但若 VM 已 `-no-shutdown` 之外的方式退出就来不及了，所以**先拍照、再操作**）。

### 结论（2026-11-18 18:45，3/3 通过）

**N 组判定：活。** 发版二进制（940,601 B `c9dc17c0…`）在 Preload 登录路径上被
`userinit → IntLoadPreloadKeyboardLayouts → LoadKeyboardLayoutW → LoadLibrary` 载入，
连续 3 次进桌面、3 次可输入，无蓝屏、无 0xF4。

由此确定的三件事：

1. **红线 #2 的原始前提在本项目上不成立。** 它来自"参照项目安装即蓝屏"，而把同一套结构
   （`DllMain` 里 `RegisterClassEx` + 早于应用进程加载）换成我们自己的二进制跑 3 次，
   结果是活得很好。前提既然不成立，红线 #2 不该以"确定性风险"的名义继续继承。
2. **这条结论只覆盖我们的二进制。** mzimeja 是否安全是另一条证据链（它在同一盘 Preload 第 2 项、
   配置词典齐全、多次开机不死，见 `M0-REDLINE-REVIEW.md` §9.4），两条不能互相顶替。
3. **登录路径不是我们的风险面，但仍是 liberNT 的余量。** A1（`DllMain` 清空）的理由从
   "修一个已复现的崩溃"降为"给别的 imm32 实现 / 64 位 / 不同加载者留余量"，
   见 `M0-REDLINE-REVIEW.md` §8 的 B′ 写法。

**N 组跑完不删覆盖层**：它保留了"装好发版二进制 + Preload 已写"的现场，
T 组可以直接在它上面换 `.ime` 复跑，省一次下载与注册。
N′（设为默认输入语言）也从这里接：基线快照 `reactos.probe-N.pre-default.qcow2`
（11,206,656 B，sha256 `dd1cc9c4…3687e5`）已在关机态取好。

## 3. 若崩了，怎么把故事讲完

release 构建默认不往串口打 KDBG（`m0/vm/logs/rosdbg.log` 现在是 **0 字节**就是证据）。要拿栈：

1. 在 `common.sh` 的 `COMMON_ARGS` 里已有 `-serial file`（见该文件注释），另加
   `-no-shutdown -no-reboot`（让 QEMU 停在 bugcheck 画面而不是关机）；
2. guest 的 `C:\boot.ini`（或 FreeLoader 的 `freeldr.ini`）加 `/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200`；
3. 重跑同一组，`logs/rosdbg.log` 会留下 KDBG 输出（含 bugcheck 与调用栈）；
4. 有栈之后再回 §9.5 的三候选做定位。

**这一步只影响"故事是否完整"，不影响"要不要改"**——三候选被同一组修复覆盖。
代价是每轮多一次引导，所以只在 N 组崩了之后做，不要提前做。

## 4. R 组（需要先做一个空壳 .ime）

`ImeInquire` 里 `return FALSE`，其余导出返回 0/NULL，`DllMain` 空函数。挂在同一个 HKL 位置跑 3 次。
目的：验证 ReactOS 的 imm32 在"IME 主动拒绝"时**不会**连带出问题——这是 A1b（失败必须可拒绝）的前提。
若 R 组崩，A1b 的前提不成立，必须另设兜底（例如加载期连 `DisableThreadLibraryCalls` 都不做）。

## 5. T 组（A1 build）

A1 尚未实现。完成后用同法跑 3 次。若 T 崩，说明"清空 DllMain"不是充分修复，回到 §9.5。

## 6. N′ 组：把我们的 IME 设为**默认输入语言**（裁决"激活"与"加载"）

**为什么加这一组**：N 组只证明了"进 Preload 无害"，而 M0 的 mzimeja 当年很可能不只是进了 Preload，
还被设成了默认/被激活。源码把"更早加载"这条路堵掉了（见下），于是唯一还没试过的变量是**登录期被激活**。

```c
/* win32ss/user/user32/windows/input.c, IntLoadPreloadKeyboardLayouts() */
if (nNumber == 1) /* The first entry is for default keyboard layout */
    uFlags = KLF_SUBSTITUTE_OK | KLF_ACTIVATE | KLF_RESET;     // ← 只有首项多这三个
else
    uFlags = KLF_SUBSTITUTE_OK | KLF_NOTELLSHELL | KLF_REPLACELANG;
...
if (hDefaultKL) SystemParametersInfoW(SPI_SETDEFAULTINPUTLANG, 0, &hDefaultKL, 0);
```

即：**"默认输入语言" ≡ `Preload\1`**。加载者、加载点、加载时刻都不变（还是 `userinit`、还是桌面就绪之后），
变的只有"是否 `KLF_ACTIVATE`/`KLF_RESET`"以及是否顺手 `SPI_SETDEFAULTINPUTLANG`。
所以本组测的是**激活路径**（`ImeSelect` → 建 UI），不是"更早载入"——后者在 ReactOS 上不存在。

**前置：先给覆盖层留副本。** 若真的崩成循环开机，`rollback` 会把 N 组现场一起丢掉：

```bash
cp /home/twipty/.workbuddy/tmp/reactos.probe-N.qcow2 \
   /home/twipty/.workbuddy/tmp/reactos.probe-N.pre-default.qcow2   # 11 MB，VM 关机时做
```

**已实测的切换（2026-11-18 19:21）**：U1/U3 的验证用的是 940,888，验完这样换回发版态：

```bash
mv reactos.probe-N.qcow2 reactos.probe-N.u1u3.qcow2     # 留档，不删
cp reactos.probe-N.pre-default.qcow2 reactos.probe-N.qcow2
```

两份都是独立覆盖层（各自的 backing 都指向 `reactos.qcow2`），可以同时存在。
U1/U3 现场留在 `reactos.probe-N.u1u3.qcow2`，需要复看时把名字换回来即可。

**操作（人手）**：

0. 先跑 `install-rospinyin.exe /status`，确认 `rospinyin.ime` 是 **940601**。
   本组必须跑发版二进制；若显示 940888，说明覆盖层还是 U1/U3 验证态，先换回 `pre-default`。
1. 控制面板 → 区域和语言 → 把「中文(简体) - RosPinyin」设为**默认输入语言**；
   等价做法（改的是同一处，源码已证「默认 ≡ `Preload\1`」）：
   ```
   reg add "HKCU\Keyboard Layout\Preload" /v 1 /t REG_SZ /d E0210804 /f
   reg add "HKCU\Keyboard Layout\Preload" /v 2 /t REG_SZ /d 00000409 /f
   ```
2. 回 regedit 确认 `Preload\1` 已变成 `E0210804`（`00000409` 退到第 2 项）。**截图留证**。
3. 正常关机，重启 3 次。

**现场记录（2026-11-18 19:32，`N/nprime-preload.png`）**：`HKCU\Keyboard Layout\Preload` 实测

| 序号 | 值 | 归属 |
|---|---|---|
| 1 | `e0210804` | **本 IME —— 已成为默认输入语言** |
| 2 | `e0200409` | mzimeja |
| 3 | `00000409` | 美式键盘（US） |

顺序与上面两条 `reg add` 的建议值不同（2/3 对调，像走了控制面板设默认），但**不影响判据**：
`IntLoadPreloadKeyboardLayouts` 只给**第 1 项**加 `KLF_ACTIVATE | KLF_RESET`，
其余一律 `KLF_NOTELLSHELL | KLF_REPLACELANG`。第 1 项是我们，本组条件成立。
被测二进制经 `/status` 确认为 **940601**（发版态，非 U1/U3 的 940888）。

**补充证据（2026-11-18 19:36，`N/nprime-preload2.png`）**：上一张 regedit 的值列看得清、但 `/status`
那一行没在画面里。补截一张把两件事放进同一屏，直接读到：

```
c:\probe>install-rospinyin.exe /status
rospinyin.ime : present (940601 bytes)
rospinyin.dat : present (902776 bytes)
recorded HKL  : E0210804
layout key    : present
```

前置条件（发版二进制 + `Preload\1` 为本 IME）至此两项均为屏幕实证，不再依赖推断。
（这张的 regedit 窗口被挪偏了，值列只露出 `e02…`；值以 19:32 那张为准。）

| 组 | 启动 | 进桌面 | 能输入 | 备注 |
|---|---|---|---|---|
| N′ | #1 | ✅ 19:43（`nprime-boot1.png`） | 不要求 | 无蓝屏；托盘指示器为 **`中`**（红底），与 N 组默认态的 `EN` 不同 |
| N′ | #2 | ✅ 19:49（`nprime-boot2.png`） | 不要求 | 无蓝屏；托盘仍为 `中`。**注**：此前有一次启动在第 49 秒被关机（未到桌面），不计入，见下 |
| N′ | #3 | ✅ 19:56（`nprime-boot3.png`） | 不要求 | 无蓝屏；托盘 `中`；CPU 版本页脚可见 `ReactOS 0.4.16-x86` |

**一次作废的启动（19:45:22–19:46:11，49 秒）**：`start` 后第 49 秒 QEMU 退出，串口日志 0 字节、
stderr 只有 TCG 回落，说明**不是蓝屏**（蓝屏不会让 QEMU 退出，`-no-shutdown` 未开时只有 guest 主动
poweroff 才会）。当时尚未到达桌面（TCG 下 3–4 分钟），因此本次判**不计入**，也不构成对判据的任何一侧证据；
重试后正常起来，即上表 #2。教训：人手操作与脚本启动会互相踩，**关机要在「你确认的画面」上做**，
否则会把下一轮正在启动的那次一起关掉。

> 19:52 用户确认：中间确实手动关闭过一次 VM，与上述判断一致。该次作废成立。

**判据**：
- **崩** → "登录期被激活"是致命变量。A 类不变量重写为"**绝不成为默认输入语言 / 登录期绝不被激活**"，
  而"不写 Preload"降为策略偏好（它既不必要也不充分）。
- **活** → 连"默认 + 激活"都不致命，M0 的因由就只能落在 mzimeja 自身的 `DllMain`（配置/词典/类注册）上，
  我们的代码根本不在那个风险面上 → 红线 #2 可以正式降级，A1/A1b 变成纯粹的移植健壮性措施。

### 结论（2026-11-18 19:56，N′ 3/3 通过 → 判「活」）

三次启动全部进桌面、无 0xF4、无蓝屏；三次托盘均为红底 `中`（`Preload\1` = 本 IME，
`KLF_ACTIVATE` 在登录期稳定生效，变量持续有效而非一次性）。

按预先固定的判据，**落入"活"分支**：

- 连"**登录期被激活**"（`ImeSelect` → 建 UI server 窗口 + 工具条，即我们最担心那条路）
  在登录路径上都不致命；
- N 组（仅加载，3/3 活）+ N′ 组（加载 + 激活，3/3 活）合起来，把"投递方式"这一整类嫌疑清空；
- M0 那次的因由只剩一处未排除项：**mzimeja 自己的 `DllMain` 行为**（读配置 → 映射 7.7 MB 词典
  → `GlobalAddAtom` → `RegisterClassEx`，且失败被吞 `return TRUE`，见 `M0-REDLINE-REVIEW.md` §9.2），
  我们的代码不在那个风险面上。

**因此红线 #2 的旧形态（"绝不写 Preload / 绝不注册为默认 IME"）正式降级**，
A 类不变量改为"**登录路径上不做任何可能失败的事**"（A1/A1b），处置见 `M0-REDLINE-REVIEW.md` §13。

**合规说明**：本组是**故意违反红线 #2**（"绝不注册为默认 IME / Preload 第一项"）的一次受控实验，
只允许在覆盖层上做，完成后必须把 `00000409` 排回第 1 项并删除副本，结论写进
`M0-REDLINE-REVIEW.md` §9.4。副作用：默认布局变中文，所有新进程的首选布局都是它。
本轮**基础盘 `reactos.qcow2` 全程未被占用、零改动**，整改只需丢弃覆盖层：
`reactos.probe-N.qcow2`（N′ 态）与 `reactos.probe-N.prime-base.qcow2`（N′ 基线）都属实验副本，
保留只为复跑或给 T 组用；不再需要时删掉即等于"把 `00000409` 排回第 1 项"。

## 7. 产物放哪

- `probe-n.sh shot <名字>` 直接写到 `m0/vm/evidence/preload-probe/N/<名字>.png`（PPM 自动转 PNG 后删除）；
  建议命名 `n-boot1` / `n-boot2` / `n-boot3` / `n-preload` / `n-bsod`。
- 串口日志（若开了 `/DEBUG`）→ 同目录 `rosdbg.log`
- 结论一行写进 `M0-REDLINE-REVIEW.md` §11 的表格，并把组标记为 崩/活
