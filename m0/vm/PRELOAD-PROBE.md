# Preload 探针实验（三组对照）

> 目的：把 `0xF4` 的**投递**（Preload → csrss 加载）与**缺陷**（加载期做 UI 初始化 / 失败被吞）分开。
> 设计与判据见 `M0-REDLINE-REVIEW.md` §11。这里只写怎么做。
>
> 硬约束：**本项目禁止程序代替用户发送键盘/鼠标事件**。因此"写 Preload"和"关机重启"这两步必须人手完成，
> 脚本只负责起 VM、保覆盖层、收日志。

## 0. 三组，按顺序跑

| 组 | 用什么 `.ime` | 预期 | 目的 |
|---|---|---|---|
| **N** 负对照 | `alpha0.0.2` 发版二进制（`DllMain` 里仍 `RegisterClassEx`，TRACE=OFF，940,601 B） | 崩 | 用本项目自己的代码复现根因，且结论直接适用于发布物 |
| **R** 拒绝路径 | 探针 `.ime`（`ImeInquire` 直接 `return FALSE`） | 活 | 验证 imm32 能接受 IME 主动拒绝 |
| **T** 处理组 | A1 build（`DllMain` 清空、注册移到 `ImeInquire`） | 活 | 验证修复 |

**N 先跑**：它不需要改任何代码，信息量最大。若 N 不崩，说明缺陷不在"加载期注册类"，而要往 §9.5 的另两个候选查——**这个结果本身就能省掉一轮改代码**。

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
| N | #1 | | | |
| N | #2 | | | |
| N | #3 | | | |

**判据（写死，不许事后放宽）**：连续 3 次进桌面且能输入 = 活；**任何一次蓝屏 = 崩**，
立刻停手，把蓝屏参数抄进备注（`0xF4(` 后四个十六进制数），然后 `probe-n.sh rollback`。

蓝屏时用手机拍屏或宿主机 `probe-n.sh shot n-bsod` 留一张（`screendump` 在 bugcheck 画面上仍然有效，
但若 VM 已 `-no-shutdown` 之外的方式退出就来不及了，所以**先拍照、再操作**）。

**N 组跑完不删覆盖层**：它保留了"装好发版二进制 + Preload 已写"的现场，
T 组可以直接在它上面换 `.ime` 复跑，省一次下载与注册。

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

## 6. 产物放哪

- `probe-n.sh shot <名字>` 直接写到 `m0/vm/evidence/preload-probe/N/<名字>.png`（PPM 自动转 PNG 后删除）；
  建议命名 `n-boot1` / `n-boot2` / `n-boot3` / `n-preload` / `n-bsod`。
- 串口日志（若开了 `/DEBUG`）→ 同目录 `rosdbg.log`
- 结论一行写进 `M0-REDLINE-REVIEW.md` §11 的表格，并把组标记为 崩/活
