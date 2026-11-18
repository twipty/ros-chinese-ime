# Preload 探针实验（三组对照）

> 目的：把 `0xF4` 的**投递**（Preload → csrss 加载）与**缺陷**（加载期做 UI 初始化 / 失败被吞）分开。
> 设计与判据见 `M0-REDLINE-REVIEW.md` §11。这里只写怎么做。
>
> 硬约束：**本项目禁止程序代替用户发送键盘/鼠标事件**。因此"写 Preload"和"关机重启"这两步必须人手完成，
> 脚本只负责起 VM、保覆盖层、收日志。

## 0. 三组，按顺序跑

| 组 | 用什么 `.ime` | 预期 | 目的 |
|---|---|---|---|
| **N** 负对照 | 当前 build（`DllMain` 里仍 `RegisterClassEx`） | 崩 | 用本项目自己的代码复现根因 |
| **R** 拒绝路径 | 探针 `.ime`（`ImeInquire` 直接 `return FALSE`） | 活 | 验证 imm32 能接受 IME 主动拒绝 |
| **T** 处理组 | A1 build（`DllMain` 清空、注册移到 `ImeInquire`） | 活 | 验证修复 |

**N 先跑**：它不需要改任何代码，信息量最大。若 N 不崩，说明缺陷不在"加载期注册类"，而要往 §9.5 的另两个候选查——**这个结果本身就能省掉一轮改代码**。

## 1. 安全前提：覆盖层，基础盘零改动

```bash
VM=/home/twipty/WorkBuddy/2026-11-12-20-02-47/ros-pinyin/m0/vm
source "$VM/common.sh"          # 设好 QEMU_HOME 与 LD_LIBRARY_PATH（qemu-img 缺 libaio.so.1 就是漏了这步）
OVL=/home/twipty/.workbuddy/tmp/reactos.probe-N.qcow2   # 每组换一个名字

"$QEMU_HOME/usr/bin/qemu-img" create -f qcow2 -b "$VM/reactos.qcow2" -F qcow2 "$OVL"
```

启动：

```bash
ROSPINYIN_HDD="$OVL" "$VM/run-reactos.sh"
```

**回滚 = 删掉 `$OVL`**。基础盘 `reactos.qcow2` 不会被写一个字节，所以蓝屏再多次也不影响已验收的环境。

## 2. N 组步骤（人手，约 10 分钟）

1. 等进桌面。先确认布局已注册且 HKL：
   ```
   C:\> install-rospinyin.exe /status
   ```
   记下 HKL（本项目实测为 `e0210804`，动态分配，**以 `/status` 输出为准**，不要照抄）。
2. 打开 `regedit`，定位 `HKEY_CURRENT_USER\Keyboard Layout\Preload`。
   截图存证：现有值长什么样（通常是 `1 = 00000409` 之类）。
3. 新建**字符串值**：名称 `2`，数据为第 1 步的 HKL（如 `e0210804`）。
   **只追加，不插首位、不改动原有项** —— 这是最小崩溃面。
4. 截图存证：写完后的 Preload 全部键值。
5. **正常关机**（不要直接断电；hive 需要落地）。
6. 启动 #1 → 能进桌面？能输入？（开记事本敲几个字即可）
7. 启动 #2、#3 重复。**登录路径的问题常常第一次不出现**，三次都要。

### 记录表（复制到 issue 或直接填这里）

| 组 | 启动 | 进桌面 | 能输入 | 备注（蓝屏参数 / 卡在哪一屏） |
|---|---|---|---|---|
| N | #1 | | | |
| N | #2 | | | |
| N | #3 | | | |

**判据（写死，不许事后放宽）**：连续 3 次进桌面且能输入 = 活；**任何一次蓝屏 = 崩**，立刻停，删覆盖层，记录第几次。

## 3. 若崩了，怎么把故事讲完

release 构建默认不往串口打 KDBG（`m0/vm/logs/rosdbg.log` 现在是 **0 字节**就是证据）。要拿栈：

1. 在 `common.sh` 的 `COMMON_ARGS` 里已有 `-serial file`（见该文件注释），额外给 guest 加 `/DEBUG` 启动项；
2. 重跑同一组，`logs/rosdbg.log` 会留下 KDBG 输出；
3. 有栈之后再回 §9.5 的三候选做定位。

**这一步只影响"故事是否完整"，不影响"要不要改"**——三候选被同一组修复覆盖。

## 4. R 组（需要先做一个空壳 .ime）

`ImeInquire` 里 `return FALSE`，其余导出返回 0/NULL，`DllMain` 空函数。挂在同一个 HKL 位置跑 3 次。
目的：验证 ReactOS 的 imm32 在"IME 主动拒绝"时**不会**连带出问题——这是 A1b（失败必须可拒绝）的前提。
若 R 组崩，A1b 的前提不成立，必须另设兜底（例如加载期连 `DisableThreadLibraryCalls` 都不做）。

## 5. T 组（A1 build）

A1 尚未实现。完成后用同法跑 3 次。若 T 崩，说明"清空 DllMain"不是充分修复，回到 §9.5。

## 6. 产物放哪

- 每组 3 张桌面截图 + Preload 截图 → `m0/vm/evidence/preload-probe/<组>/`
- 串口日志（若开了 `/DEBUG`）→ 同目录 `rosdbg.log`
- 结论一行写进 `M0-REDLINE-REVIEW.md` §11 的表格，并把组标记为 崩/活
