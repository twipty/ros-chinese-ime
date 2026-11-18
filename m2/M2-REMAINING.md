# M2 剩余事项清单

> 基线：`alpha0.0.1`（2026-11-17）｜状态：**M2 已完成（2026-11-18 12:5x），仅 P2-1 发版工程化未动**
> 已确认可用：真实键盘输入 → 组合串 → 候选窗 → 选字上屏（`nihao`/`zhong`/`guo`/`shui`），
> 三层记录（入口 `WH_KEYBOARD` / 窗口消息 / IME trace）逐条对齐，无多余字符、无丢键。

优先级定义：**P0** = 不解决就不能宣布 M2 完成；**P1** = 能宣布完成但用户会碰到；**P2** = 工程化/文档。

进度：**P0-1 / P0-2 / P0-3 / P1-1 / P1-2 / P1-3 / P2-2 / P2-3 已完成**；**P2-1 未动**（发版工程化，不影响 M2 判定）。
真机三轮反馈（`zhongguo` 只显示 zhong、Shift 不切中英、翻页弹窗位移、汉字字距重叠）已全部修复并复验，
见 `HANDOVER.md` §11.13–§11.16。

---

## P0：M2 收口（3 项）

### ✅ P0-1 跑完 `M2-ACCEPTANCE.md` 的 15 项（已完成 2026-11-18 12:5x）

**结论：15/15 通过**，用户确认"显示已正常"。依据是 `vmshare/imhost.log` 同一轮运行的
键序 + `WM_IME_CHAR` 折叠还原（433 次按下，入口层与窗口层逐条对齐，程序注入 0 次）。
逐项实测值已填进 `M2-ACCEPTANCE.md` §3；还原方法与映射表见 `HANDOVER.md` §11.16。
其中 `nv/lv/lve/nve` 四例、跨页选字、`Backspace`/`Esc`、100+ 字连打、lone-Shift 中英往返
都有日志实据；3.9–3.11 的页码文本未逐页截图，结论来自跨页序号命中（对外出验收纪要时建议补图）。

下表保留原始用例，便于后续回归时照着再跑一遍：

| 项 | 输入 | 期望 | 为什么值得单独跑 |
|---|---|---|---|
| 3.1 | `zhong` | 组合串 + 候选首项「中」 | 单音节基线 |
| 3.2 | `zhongguo` | 两段下划线、候选首项「国」 | **多音节切分**：`zhong`+`guo` 是否被正确切开而非贪婪匹配 |
| 3.3 | 空格 | 上屏「国」 | 上屏后组合串/候选窗必须消失 |
| 3.4–3.7 | `nv`/`lv`/`lve`/`nve` | 女/律/略/虐 | `v`→`ü` 的四条边界（`lve`/`nve` 带韵母 `üe`） |
| 3.8 | `shi` 后按 `2` | 当前页第 2 候选 | 数字键必须按当前页解释，不是全局第 2 |
| 3.9–3.11 | `=` → `1` → `-` | 翻页、跨页选字、回翻 | `shi` 有 95 条候选 = 11 页，翻页路径最容易出错 |
| 3.12 | `hao` + Backspace×2 | 组合串变 `ha`，候选刷新 | 回退后候选必须重算 |
| 3.13 | `hao` + Esc | 清空且**不上屏** | Esc 被误当提交是常见 bug |
| 3.14 | 连续 100 字母 | 无崩溃、无残窗、候选窗跟随插入符 | 长会话下的状态泄漏 |
| 3.15 | 切 EN 再切回中 | 组合串清空、可重新输入 | 跨布局的状态清理 |

**做法**（已执行）：`imhost.exe rec` 手打，一轮一份日志。实测值已填进 `M2-ACCEPTANCE.md` §3，
`M2-REPORT.md` §3 的 ⬜ 已改 ✅。截图归档 `m0/vm/evidence/m2-acceptance/` 这一步仍空着——
日志在，图没整理，不影响判定。

### ✅ P0-2 `ImmGetOpenStatus` 读数异常（已完成 2026-11-17）

- `-1` 是 `imhost` 的 `g_himc == NULL` 哨兵值（恰好等于 `TRUE`，误导性极强），**不是** IMM32 的返回值；
- `open=0` 是 ReactOS 的出厂状态：`imm32_imm.c` 建上下文时 `pIC->fOpen = FALSE`，
  `ImmSetActiveContext`/`ImmInstallIMEW` 都不回置；键路由（`ImmProcessKey`）也不看这个值；
- 修法：`ImeSetActiveContext(fFlag=TRUE)` 时对齐为 `TRUE`（`AlignOpenStatus()`），
  trace 打 `openalign before=/after=`；`check_m1.py` 禁止把它当门禁。

详见 `HANDOVER.md` §11.11。

### ✅ P0-3 组合窗位置抖动（已完成 2026-11-17）

- **原猜测错误**：不是引擎在调 `ImmSetCompositionWindow`（全仓 0 处调用），
  而是 ReactOS 的 `EDIT_SetCaretPos` 每次移动光标都调（`edit.c`，仅 `__REACTOS__` 分支，
  不判断是否在组合中），一次按键 1–3 次 → 通知数量无法从 IME 侧降低；
- **真正的 bug 在我们窗口上**：组合窗只改缓存宽高、从不 `SetWindowPos`（长串被裁），
  且每次通知都 `InvalidateRect + ShowWindow`；
- 修法：几何单一 owner（`place_windows()` → `apply_geometry()`，变化才下发）、
  `show()` 只在隐藏时调用、重绘以内容变化为前提；trace 增加 `wmove=` / `wrep=` 计数。

详见 `HANDOVER.md` §11.12。

---

## P1：可用性（3 项）

### ✅ P1-1 安装器（已完成 2026-11-17）

`/step1` 现在拷 **两个**文件（`.ime` + `.dat`）并逐字节校验；文件被映射时先改名 `.old`
再写新文件；`/status` 分别报告，`/u` 两份都删。仍保持 M0 红线（不写 Preload、HKL 动态）。

### ✅ P1-2 字体覆盖到扩展 A 区（已完成 2026-11-17）

不扩字体（11 MB 子集只到 `U+9FA5`，实测扩展 A 区 0 字），改为运行时按字形过滤：
`UiCanRender()` 用候选窗同一 HFONT 调 `GetGlyphIndicesW`（按码点缓存），
在进分页器**之前**剔除，避免数字键与行号错位；探针不可用或过半被剔时整体放行。

### ✅ P1-3 生僻字可发现性（已完成 2026-11-17）

`set_show_rare(true)` 的语义与代价写进 `HANDOVER.md` §11.13：默认关闭，打开后候选会暴涨
且多数落在子集外（被 P1-2 过滤）。M3 再给语言栏菜单项。

---

## P2：工程化与文档（3 项）

### ⬜ P2-1 CI 直接产出 release 资产

现在 `.ime`/`.dat` 是本地构建后手工上传。CI 已有三个 job（Linux 测试 + MinGW i686 +
VM 资产暂存），可加一个 tag 触发的 release job，保证"发布的二进制 = 该 tag 的源码构建"。
**未动**——它不影响 M2 判定，建议与 `alpha0.0.2` 的发版一起做。

### ✅ P2-2 `M2-REPORT.md`（已完成 2026-11-17）

`m2/M2-REPORT.md`：结论与判定口径、交付物与版本对应、已验证项与待验项、
10 条根因修复记录、已知限制、门禁与复现命令。

### ✅ P2-3 版本口径与回归基线（已完成 2026-11-17）

- `check_m1.py` 保留原名，定位写成"**长期契约闸门**"（README §11 与 HANDOVER §12 都改了）；
- 数字统一到实际值：`ctest` 11/11、`check_m1` **57/57**（原写 50/50、25 项）；现行 **63/63**（2026-11-18 改白名单后 +4）；
- 新增 4 条 M2 不变量：组合窗几何 owner、重绘条件化、安装器两份文件、open 状态不当门禁。

---

## U：真机新发现（2026-11-18，N 组期间，用户现场报告）

三条都出现在 `alpha0.0.2` 发版二进制的正常使用中，与 Preload / 蓝屏无关。
按"能不能改、该谁改"分成三类，不要混在 M2 观察项里。

### 验证记录（2026-11-18 19:11，`m0/vm/evidence/preload-probe/N/u1u3-verified.png`）

U1 + U3 已改并真机验证，构建 **940,888 B**（`ed661fbe…29651e`）：

- **U1 ✅**：重启后**第一次** `Alt+Shift` 切到 RosPinyin，未按 Shift，直接敲 `nihao`
  → 候选窗立刻展开（`1/4`，9 项）。用户原话"首次切换后即可输入中文，无需再人工启动"。
- **U3 ✅**：【拼】完整出现在任务栏**上方**。像素校验：截图里工具条顶边 y≈542，
  即 `work.bottom(572) - 22 - 8`，说明 `SPI_GETWORKAREA` 在 ReactOS 上返回了正确的工作区，
  走的是主路径而非全屏兜底。

一个附带的强证据：**U1 修好之后托盘仍显示 `EN`**，而实际已在中文态。
`fOpen` 已被 `AlignOpenStatus` 设成 `TRUE`，指示器却没跟上——这进一步坐实 U2 是平台侧
（指示器不读 `fOpen`，或刷新滞后），不是我们 OpenStatus 管理的延伸问题。

### ✅ U1 开机首次切到本 IME 时是「关闭」状态，要人工开一次（已改 2026-11-18，真机已验证）

**现象**：开机后第一次 `Alt+Shift` 切到 RosPinyin，托盘仍是 `EN`，直接敲键不出候选；
按一下 Shift（或点工具条）之后才进入中文态，此后正常。

**根因**：`ImmGetOpenStatus` 的 `fOpen` 只在 `ImeSetActiveContext(TRUE)` 里被对齐
（`framework/host/imm_entry.cpp` L625–645 → `AlignOpenStatus` L600），
而 `ImeSelect(TRUE)`（L564–583）只调 `on_select(true)` + `UiToolbarShow`，**不碰 open 状态**。
ReactOS 在"首次切到一个布局"时只保证走 `ImeSelect`，`ImeSetActiveContext` 不保证先到；
于是首次激活落在没有对齐的那条路径上，`fOpen` 保持 `IntCreateInputContext` 时的 `FALSE`。

这与 P0-2 修的是同一个错位，只是 P0-2 的补丁挂在了**两条路径里的一条**上。
"once per session" 的语义没错（不能在每次激活时覆盖用户按过的 Shift），错在首次可能不走那条路。

**修法**：把 `AlignOpenStatus` 提到 `ImeSelect(TRUE)` 也调用的位置，仍由
`state.open_initialized` 保证整会话只对齐一次。改动约 3 行，不动状态机语义。

**判据**：开机 → `Alt+Shift` 切到本 IME → 直接敲 `nihao` 出候选，中途不按 Shift。
`/status` 或 trace 里首帧应见 `openalign before=0 after=1`。

### U2 语言指示器显示上一个 IME 的状态（跨 IME，判断为平台侧）

**现象**（用户原话整理）：`[EN]` 下能输入中文 → 再 `Alt+Shift` 切到日文，状态栏显示 `[中]`，
日文 IME 本身也是关闭态 → 再切回 `[EN]`，之后就正常了，切到谁就是谁的开/关状态。

**判读**：**两个不同 IME 表现一致**（我们的显示成 mzimeja 的状态、mzimeja 显示成我们的），
这不是单个 IME 的缺陷。指向 ReactOS 的指示器刷新链：
`GetKeyboardLayout(0)` 在切换瞬间仍返回旧 HKL，或 `NtUserNotifyIMEStatus` 的到达晚于指示器重绘。
"切回 EN 一次就恢复正常"符合这个解释——经过一个**没有 IME 的布局**，指示器的缓存被清掉了。

**处置：先不改我们的代码。** 我们没有权限替 mzimeja 发通知，也不该为了迁就指示器而在
失活时乱发 `ImmSetOpenStatus`（那会制造新的错位，正是 P0-2 注释里警告过的事）。

**要坐实归因，需要一次 N″ 对照**：把我们的布局从 Preload 里摘掉，只留
`00000409` + `e0200409`(mzimeja)，重启后在同一路径上复现。
若只剩两个布局仍然错位 → 平台 bug，写进 `PORTABILITY-UPSTREAM-64BIT-ARM64.md` 的上游观察；
若不错位 → 说明三个以上布局参与时才触发，仍属平台，但要在 liberNT 移植时列为已知差异。
成本约 20 分钟，可在 N′ 之后顺手做。

### ✅ U3 工具条【拼】被任务栏挡住（已改 2026-11-18，真机已验证）

**现象**：右下角的【拼】只露出顶部一条，其余被系统任务栏盖住。

**根因**（已定位到行）：`framework/ui/toolbar.cpp` L59–72 `dock_bottom_right()` 用的是
**全屏**尺寸而不是**工作区**：

```cpp
const int screen_h = GetSystemMetrics(SM_CYSCREEN);
if (screen_h > 0) y = screen_h - kToolbarHeight - kScreenMargin;   // 600-22-8 = 570
```

600 高的屏上任务栏顶边约 572，于是 22px 高的工具条有 20px 落在任务栏之下。
`HWND_TOPMOST` 没能救回来——ReactOS 的任务栏同样是 topmost 且 z 序在我们之上，
**"设了 TOPMOST 就一定压在任务栏上面"在 ReactOS 上不成立**，这条要记住。

**修法**：改用工作区矩形，一处替换即可：

```cpp
RECT work{};
if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
    y = work.bottom - kToolbarHeight - kScreenMargin;
```

副产品：任务栏在左侧/右侧、或自动隐藏时也自动正确（x 同理改用 `work.right`）。
改动约 6 行，属纯定位修正，不影响任何状态逻辑，可以独立提交、独立回归。

**判据**：`N-boot4` 截图里【拼】完整可见，且与任务栏之间留 `kScreenMargin` 的缝。

---

## 观察项（不阻塞，但需记录）

| 项 | 说明 |
|---|---|
| `codes=419` vs 表头 `syls=418` | 差的 1 个是 alt 别名键（不在音节表里）。指纹行已把标签从 `syls=` 改为 `codes=`，但 `M2-DATA-PLAN.md` 未说明这个口径 |
| `.ime` 被常驻进程映射 | 验证期靠 `rospinyindev.ime` 换名绕过；安装器已加改名退让，但用户更新时仍需重启 |
| `m0/share/` 与 `build/m2-data/` 数据一致性 | 两处都有 `rospinyin.dat`，靠手工 `cp` 同步，容易再次出现"改了没生效" |
| `IMN_SETCOMPOSITIONWINDOW` 每键 1–3 次 | ReactOS `EDIT_SetCaretPos` 的行为，属系统侧；我们不再因此重绘 |

---

## 建议顺序

P0-1 已全绿（15/15），`alpha0.0.2` 已于 2026-11-18 发布（`ROSPINYIN_TRACE=OFF` 构建，
资产指纹见 `HANDOVER.md` §11.17）。剩下的顺序：

1. **U1 + U3**（共约 9 行，两个独立小改）—— 都是真机每天会撞到的观感问题，且都不碰状态机语义；
   U1 改完正好被 N′ 顺带验证（N′ 本来就是"登录期被激活"，能直接看出首次是否已开）；
2. **P2-1**（CI 产出 release 资产）—— 本次发版仍是"本地构建 + 脚本上传"，
   靠人记住"发布的二进制 = 该 tag 源码构建"；让 tag 触发 CI 构建才能真正保证；
3. 归档截图进 `m0/vm/evidence/m2-acceptance/`（日志已齐，缺图）；
4. 进 M3：语言栏菜单、生僻字开关入口、冷僻字支持（见 `RARE-CHAR-AND-GLYPH-SOLUTION.md`）。

U2 不进这个序列：它是平台侧，需要 N″ 对照才能坐实归因，做不做都不改我们的代码。
