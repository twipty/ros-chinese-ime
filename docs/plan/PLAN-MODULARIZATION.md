# 模块化与前端抽象的落地计划（方案 B 精简版）

> 状态：**计划稿**（2026-11-18）。只排程与定判据，**不实现、不冻结任何接口**。
> 依据：`FRAMEWORK-ROUTE-A-REVIEW.md`（对三份路线 A 文档的实证评审）§4–§5。
> 那三份文档（`~/Documents/FRAMEWORK-ROUTE-A-*.md`）的定位从本题起改为
> **"完全抽象化那一天的参考资料"，不是本项目的执行计划**。

## 0. 一句话

把"能模块化"和"可完全抽象"拆成两件事：**前者现在做**（便宜、可逆、有验收），
**后者只预留缝**——不定义接口、不承诺 ABI、不写死"前端"这个词。

## 1. 目标与非目标

**要拿到的（可验收）**

| # | 目标 | 判据 |
|---|---|---|
| G1 | 目录树自解释：看路径就知道哪层是平台无关、哪层是 Win32 | `framework/` 下不存在"含义靠猜"的目录名 |
| G2 | 键路由有 Linux 单测 | `route_key` 的分支覆盖现有全部分支，ctest 出现新目标 |
| G3 | 宿主层有一条冒烟测试 | 假 engine/scheme/sink + 固定键序，断言 sink 调用序列；**这是后续任何重构唯一的自动化安全网** |
| G4 | 第二个方案"只加文件夹"接入 | `git diff --stat` 不出现 `framework/` 任何文件 |
| G5 | 未来换前端时改动被限制在 seam 内 | seam 清单里的调用点数量有上限断言（`check_m1` 新增一项） |

**明确不做**

- 不冻结 `Frontend` / `Plugin` ABI（现在只有一个实现，冻结等于猜——这正是三份文档自己点名的 R-1 风险）；
- 不做"把引擎挪出 csrss"（阶段 2）：与"让第三方代码进 csrss"的插件目标价值方向相反，先不碰；
- 不重写 UI 层为平台无关（Win32 窗口代码没有第二个平台消费者，重写是纯成本）。

## 2. 现状基线（`deb4282` 之后，2026-11-18 实测）

```bash
wc -l framework/src/*.cpp framework/include/imfw/*.hpp | tail -1   # 993  可移植层
wc -l framework/host/* framework/ui/*                          | tail -1   # 2584 平台层（Win32）
wc -l ime/*.cpp                                                 | tail -1   # 57   装配
wc -l tests/*.cpp                                               | tail -1   # 1314 测试
python3 tools/check_m1.py | tail -1                                        # 57 项
(cd build && ctest)                                                        # 11 项
```

| 文件 | 行数 | 性质 |
|---|---|---|
| `framework/host/imm_entry.cpp` | 766 | IMM32 入口 + 键路由 + 会话，**最重** |
| `framework/ui/ime_ui.cpp` | 300 | 窗口编排与锚点 |
| `framework/ui/ui_window.cpp` | — | 几何单一 owner |
| `framework/include/imfw/{engine,scheme}.hpp` | 72 / 131 | 可移植契约，**Win32 类型 0 处** |

**接缝现状（这是本计划的地基）**：`framework/host/imm_entry.cpp` 对外部世界的依赖只有四项，
数量都很小，所以抽象成本可控、但**现在不值得为它冻结接口**：

| seam | 现状调用点 | 抽象后要动的半径 |
|---|---|---|
| 引擎（`state->engine->*`） | 6 处 | 全在 `process_key` / `apply_commit` |
| 方案策略（`scheme.*`） | 13 处 | 全在 `process_key` |
| UI 出口（`UiShow/UiHide/UiRefresh/UiCountersSnapshot`） | 5 处 | 全在 `process_key` + `refresh_ui` |
| IMM32 ABI（`imm32_abi.h` + 16 导出） | 1 个文件 | 只在 `imm_entry.cpp` |

## 3. 四个阶段

### X1 目录归位（0.5–1 天，纯 `git mv`）

```
framework/host/{dllmain,imm_entry,ui_server,session_store,imm32_abi,trace_stamp}
        →  framework/platform/win32/imm32/
framework/ui/{ime_ui,ui_window,composition_window,candidate_window,toolbar}
        →  framework/platform/win32/ui/
framework/src/、framework/include/imfw/、pinyin/                         不动
```

**为什么叫 `platform/win32/imm32` 而不是 `frontend/imm32`**：评审里的方案 B 用的是 `frontend/`，
这里改掉。`frontend` 预设了一个尚不存在的抽象层名字，而我方约定"接口不冻结"——
目录名不该比代码先做决定。`platform/win32/` 只陈述事实（这段代码就是 Win32 + IMM32），
将来真出现第二个前端，它作为**并列目录**加进来即可，不需要改造旧名。

改动面（已实测，共 3 类）：

| 位置 | 处数 | 说明 |
|---|---|---|
| `ime/CMakeLists.txt` | 9 源文件 + 3 include 目录 | 相对路径 `../framework/host|ui/` |
| `tools/check_m1.py` | 15 处硬编码路径 | 含 `framework/ui/README.md` |
| 文档（`HANDOVER`/`framework/README`/各报告） | 若干 | 只改路径，不改论述 |

**验收（硬）**：功能等价不看测试，看**字节**——用同一工具链重建，`.ime` 与 X1 前逐字节比对，
只允许 4 处差异（PE 时间戳 `0x88`、校验和 `0xD8–0xD9`、调试字段 `0x60A04`）。
这比任何单测都硬：路径改名若漏了某个源文件，链接产物一定变。

**回滚**：`git revert` 单个提交即可，无逻辑变更。

### X2 键路由 + 宿主冒烟（3–5 天，**在 M3 开工前**）

顺序不能反：先有安全网，再搬代码。

1. **先把 `process_key` 从 IMM32 ABI 里剥出来**：`handle_key(Session&, KeyEvent) -> KeyResult`，
   入口函数只做 ABI 转换（`lpbKeyState` → `KeyEvent`，`KeyResult` → `TransMsgList`）。
   这一步不改变行为，但让宿主第一次能脱离 imm32 头文件被测试。
2. **写 `tests/host_smoke_test.cpp`**：假 engine（固定返回序列）、假 sink（记录 `UiShow/UiHide/UiRefresh`
   调用顺序与参数）、假 scheme（字母表 `a-z` + `1-9` 选字 + `=`/`-` 翻页）。
   键序固定为：`zhongguo` → `2`（选字）→ `Backspace`×2 → `Esc` → `Shift`（lone-Shift 切换）。
   断言：sink 调用序列逐项相等 + `commit` 文本序列。**约 120 行，是全天最值钱的一百行。**
3. **抽 `route_key(vk, scheme) -> Intent`**：只搬 421–464 那 44 行的**判定**，
   产出 `enum class Intent { None, SelectDigit(n), PagePrev, PageNext, Backspace, Escape,
   InputChar(c), ToggleLang }`，`process_key` 保留副作用（改 pager、刷 UI、调引擎）。
   验收：`host_smoke` 在 X2 前后调用序列**逐字节一致**；`route_key` 的单测覆盖每个 Intent。

**为什么只搬判定不搬执行**：判定是纯函数（可 Linux 测），执行碰 Win32（测不了）。
把能测的部分先测起来，比引入一个抽象层便宜一个数量级。

### X3 方案可插拔演示（3–5 天，与 M4 并行，可延后）

**不做新 ABI**。`imfw::Scheme` 已经是虚拟类，xingma 已经在同一构建里；
要证明"只加文件夹能接入"，还缺的是一个**最小的第三方示例**：

1. `samples/toy-scheme/`：30 行的 `Scheme` 实现（字母表、假码表、窗口类名、标签）；
2. `-DROSPINYIN_SCHEME=toy` 构建开关（只换 `scheme_reg.cpp` 的注册对象）；
3. `samples/README.md`：接入五步 + 常见坑（窗口类名冲突、`.dat` 格式、`Result.segments` 语义）。

**判据（写死）**：接入 `toy-scheme` 的提交里，`git diff --stat` **不得出现 `framework/` 下任何文件**。
出现即说明框架还漏了耦合点，补框架而不是补示例。

这取代原方案的"阶段 3（运行期插件 / C ABI 工厂）"。**C ABI 只在需要"第三方二进制、单独发版"
时才需要**，那时才冻结——现在做等于给一个还没出现的需求写 ABI。

### X4 前端抽象：只立门，不排期

什么时候才启动（**任一**触发，写死）：

| 触发条件 | 为什么它能触发 |
|---|---|
| 出现第二个前端消费者（非 Win32 / 非进程内 / 独立进程） | 有第二个实现才有比较，才可能冻结接口 |
| `imm_entry.cpp` > 1,200 行 | 单文件已到"改一处要读半天"的规模 |
| csrss 崩溃面成为真实痛感 | 才值得为进程隔离付 IPC 成本 |

触发后**先做**：按 `deb4282` 之后的最新代码重做 MANIFEST 的逐文件处置（原表已失真 +41%~+125%），
并给 `on_key` 补 commit 输出通道（评审 §3.3：IMM32 是 `ImeToAsciiEx` 返回缓冲模型，不是推模型，
缺这条 PR-1 必返工）。

## 4. "预留完全抽象化"的三个机制

不是靠现在写接口，而是靠三件**不花钱**的事，让将来那次抽象不必推倒重来：

| # | 机制 | 现在做什么 | 将来省什么 |
|---|---|---|---|
| M1 | **Intent 中间语言** | X2 把路由判定收敛为一个枚举 + 纯函数 | 换前端时，Intent 是两端共用的稳定词汇；否则每个前端各写一套键判定 |
| M2 | **seam 清单 + 上限断言** | `tools/seams.py` 统计 4 条 seam 的调用点数，`check_m1` 加一项：任一 seam 超过当前值即报错 | 耦合只能减少不能增加。某天调用点从 6 涨到 20，就是抽象的必要性证据，而不是靠争论 |
| M3 | **接口状态标记** | 头文件里给每个契约标 `// status: freeze | candidate | draft`（`Engine`/`Scheme` = candidate；新 `Intent` = draft） | "冻结"变成显式的、可检索的事，而不是口头承诺；`check_m1` 可断言"没有 draft 接口被导出到 DLL" |

三条都不改运行时代码，M2/M3 各半天。

## 5. 排程（插在 ROADMAP 的哪里）

| 阶段 | 放在 | 工期 | 对 v0.1 的影响 |
|---|---|---|---|
| X1 目录归位 | **M2 收口后立刻**（现在） | 0.5–1 天 | 0 |
| X2 路由 + 冒烟 | **M3 开工前** | 3–5 天 | +约 1 周 |
| X3 玩具方案 | 与 M4 并行（或推迟到 M7） | 3–5 天 | 0（不占关键路径） |
| X4 前端抽象 | 未排期，等门 | — | 未知 |

**X2 为什么必须抢在 M3 之前**：M3 要加中文标点、全角/半角、Shift 临时英文、
Ctrl+Space 与备用热键——**全部改在 `process_key` 里**。先有冒烟测试，M3 每加一个键就有一条回归；
反过来先做 M3，那 92 行会再长一节，之后重构要重新建立"改前行为"的基线。

**总代价 1–2 周，v0.1 推迟约一周**——与方案 A 的 2–4 个月不是同一个量级。
这个交换的理由只有一句：M3 之后宿主层只会更长，改起来只会更贵。

## 6. 风险

| 风险 | 处置 |
|---|---|
| X1 漏改路径导致链接了旧文件 | 字节比对兜底（只允许 4 处差异），不通过即回滚 |
| X2 剥 `handle_key` 时改坏 ABI（`TransMsgList` 语义） | 先只做"搬运 + 包装"，不动任何判定；字节比对 + 真机 `nihao` 冒烟各一次 |
| 冒烟测试把实现细节写死，反而阻碍后续改动 | 断言对象是 **sink 调用序列 + commit 文本**，不碰内部状态；Intent 单测才断言分支 |
| X3 示例沦为"框架里加 if" | 判据写死：`framework/` 零 diff，否则补框架 |
| 计划本身被当成"已经冻结的设计" | 本文开头声明 + M3 状态标记；X4 的门写死 |

## 7. 与三份路线 A 文档的关系

- 保留价值最高的是：**接口清单式思维**（MAPPING 的 16 导出逐个处置）、**PR 切分纪律**、
  **四层回滚方案**。将来 X4 触发时直接复用。
- 不能直接用的是：**阶段顺序**（1→2→3 的依赖链是错的，3 不需要 1/2）、**文件规模快照**（两天失真 +125%）、
  **零改动红线**（已被 M2 的 `commit_text` 打破）。
- 若哪天要重做那份清单：先跑 `tools/snapshot_sizes.py`（新增，输出字节/行数与指纹），
  把数字变成"可重生成的产物"，不要手抄。

## 8. 待办（按 PR 粒度，勾完即计划完成）

- [ ] X1-a `git mv` + `ime/CMakeLists.txt` 12 行路径 + `check_m1` 15 处 + 文档路径
- [ ] X1-b 字节比对报告（4 处允许差异）写入 `HANDOVER`
- [ ] X2-a `handle_key` 剥离 + `KeyEvent`/`KeyResult` 结构（行为不变）
- [ ] X2-b `tests/host_smoke_test.cpp`（假 engine/scheme/sink，5 段键序）
- [ ] X2-c `route_key` + `Intent` 单测；`host_smoke` 序列不变
- [ ] X3-a `samples/toy-scheme/` + `ROSPINYIN_SCHEME` 开关 + 接入文档
- [ ] M2 `tools/seams.py` + `check_m1` 新增 seam 上限项
- [ ] M3 头文件 `status:` 标记 + 断言"draft 接口不进 DLL 导出"
