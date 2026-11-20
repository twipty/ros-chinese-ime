# 统一 dat 格式规范（拼音 `rospinyin.dat` 与形码 `shape.dat`）

- 状态：**已冻结，两产品均已同步**（2026-11-20）。xingma 侧定义；拼音侧落地文本为 `docs/plan/DAT-FORMAT-V2.md`（xingma 目录外），已按本文重写并与本文互指。拼音侧的四条裁决回执见 §10。
- 决策来源：两产品均未落地代码（拼音 dat 仍是 v1、v2 仅冻结文档；xingma 零 C++），此刻统一格式的迁移成本为零，避免后续维护两套格式管理。
- 适用版本：拼音与形码**统一采用 v2**。拼音 v1 文件保留兼容读法（§6）。
- 关联：`xingma/XINGMA-PLAN.md` §4.1（形码格式）、`FRAMEWORK-REQ.md` FR-1/FR-8（可见性位消费）、`docs/plan/DAT-FORMAT-V2.md`（拼音现状，xingma 外只读引用）。

---

## 1. 统一原则

| 原则 | 内容 |
|---|---|
| 同构不同义 | 两产品共用**二进制骨架**（Header/Entry/Code/池 的槽位数、偏移、大小、校验方式），字段语义按 `magic` 解释——`magic` 即 tagged union 的 tag |
| 最小并集 | 字段槽位取并集后复用：语义相同的字段（rank、flags）**同偏移真共用**；其余槽位按产品各自解释，不增加第 7 个槽 |
| 公共谓词固定偏移 | `rank`（Entry 槽1）与 `flags`（Entry 槽5）在两产品中偏移一致，使排序/可见性过滤等公共代码无需感知 magic |
| 单一极性 | 可见性位统一按拼音口径：**bit1 = 0 表示 Droid Sans Fallback 保底可绘（可见集），1 表示需额外字体** |
| 确定性 | 同输入逐字节复现；禁时间戳/随机序/哈希种子；小端 32 位字段；定长头 + 定长记录 + NUL 终止 UTF-8 池（无指针、mmap 友好） |

---

## 2. 统一布局总表

```
[Header 80B]
[Entry × entry_count (24B)]           单字 / 编码条目
[Code × code_count (16B)]             音节段 / 编码段
[LexEntry × lex_entry_count (24B)]    词组条目（0 = 无词组表）
[LexCode × lex_code_count (16B)]      音节序列段 / 词组编码段
[string pool]
```

四张定长表首尾相接、无空隙无重叠，Header 逐张给出条数与偏移。两产品共用这套骨架，
区别只有 `magic` 与各槽语义。词组表不用时两张表计数都为 0、偏移退化为同一点，**不占字节**。

### 2.1 Header（80B，两产品相同骨架）

| 偏移 | 字段 | 大小 | 拼音语义 | 形码语义 |
|---|---|---|---|---|
| 0 | `magic[8]` | 8 | `ROSPYDAT` | `ROSSHDAT` |
| 8 | `version` | 4 | **2（唯一版本）** | 2 |
| 12 | `flags` | 4 | 保留，须 0 | 保留，须 0 |
| 16 | `entry_count` | 4 | 单字条目数（原 `char_count`） | Entry 条数 |
| 20 | `code_count` | 4 | 音节数（原 `syl_count`） | Code 条数 |
| 24 | `lex_entry_count` | 4 | 词条数；0 = 无词组表 | 词组条目数；0 = 无 |
| 28 | `lex_code_count` | 4 | 词组键段数 | 词组编码段数 |
| 32 | `entry_table_off` | 4 | 须 = 80 | 须 = 80 |
| 36 | `code_table_off` | 4 | 须 = `entry_table_off + entry_count×entry_size` | 同式 |
| 40 | `lex_entry_table_off` | 4 | 须 = `code_table_off + code_count×code_size` | 同式 |
| 44 | `lex_code_table_off` | 4 | 须 = `lex_entry_table_off + lex_entry_count×entry_size` | 同式 |
| 48 | `pool_off` | 4 | 须 = `lex_code_table_off + lex_code_count×code_size` | 同式 |
| 52 | `pool_size` | 4 | 池字节数；`pool_off + pool_size ≤ 文件大小` | 同 |
| 56 | `name_off` | 4 | 拼音不用，须 0 | 池偏移：显示名，空串非法 |
| 60 | `alphabet_off` | 4 | 拼音不用，须 0 | 池偏移：编码字母表 |
| 64 | `source_off` | 4 | 拼音不用，须 0 | 池偏移：来源指纹，0=无 |
| 68 | `entry_size` | 4 | **须 = 24** | 须 = 24 |
| 72 | `code_size` | 4 | **须 = 16** | 须 = 16 |
| 76 | `max_code_len` | 1 | 拼音不用，须 0 | 1..8 |
| 77 | `auto_commit` | 1 | 拼音不用，须 0 | 0/1/2（Q6 已决：2 为显式逃生口） |
| 78 | `reserved[2]` | 2 | 须 0 | 须 0 |

> **为什么是 80B（2026-11-20 修订 B）**：拼音放弃 v1 兼容后，Header 不必再镜像旧 52B 布局，
> 于是把词组的两张表（条数 + 偏移）直接放进 Header——**一个文件承载字表与词表，取消第三套 magic**。
> `entry_size`/`code_size` 是自描述字段：与 24/16 不符即拒载，用来抓「版本号换了、记录宽度没换」
> 的半升级文件（这类文件按新宽度硬读，第一条之后全是垃圾）。

### 2.2 Entry（24B = 6 槽 × 4B，两产品相同骨架）

| 槽 | 偏移 | 拼音语义 | 形码语义 | 关系 |
|---|---|---|---|---|
| 0 | 0 | `codepoint`（UTF-32 码点） | `text_off`（池偏移，UTF-8 文本） | 按 magic 解释 |
| 1 | 4 | `rank`（1-based 频序） | `rank`（越小越靠前） | **真共用，同偏移** |
| 2 | 8 | `syl_id`（音节表下标） | `text_len`（文本字节数） | 按 magic 解释 |
| 3 | 12 | `reading_off`（主读音池偏移） | `entry_type`（0=单字, 1=词） | 按 magic 解释 |
| 4 | 16 | `alt_off`（次读音池偏移，0=无） | 保留，须 0 | 按 magic 解释 |
| 5 | 20 | `flags`（见 §3） | `flags`（见 §3） | **真共用，同偏移** |

> 形码侧相对旧定义的变化：`Entry` 由 20B 扩到 24B（原 flags 在槽4，现移到槽5；新增槽4 保留位）。
> 字段重排仅为对齐公共谓词偏移，语义不变。

### 2.3 Code / SylEntry（16B = 4 槽 × 4B，语义一一对应）

| 槽 | 偏移 | 拼音 `SylEntry` | 形码 `Code` | 关系 |
|---|---|---|---|---|
| 0 | 0 | `name_off`（音节名池偏移） | `code_off`（编码串池偏移） | 统一名 `key_off`，按 magic 解释 |
| 1 | 4 | `first_entry`（首字条目下标） | `first_entry`（Entry 首下标） | **真共用** |
| 2 | 8 | `entry_count`（段长） | `entry_count`（段长） | **真共用** |
| 3 | 12 | `flags`（拼音须 0） | `flags`（bit0 一级简码…bit4 含识别码） | 统一名 `flags` |

### 2.4 词组表（LexEntry 24B / LexCode 16B，拼音侧既已定义）

词组条目与 `Entry` **同宽不同义**——`entry_size` 只有一个值，不允许两张表不同宽：

| 槽 | 拼音 `LexEntry` | 说明 |
|---|---|---|
| 0 | `text_off` | 池偏移：词文本 UTF-8，空串非法 |
| 1 | `rank` | 与 Entry 同偏移同语义 |
| 2 | `syl_count` | 音节数，≥ 2 |
| 3 | `reading_off` | 池偏移 `"ni3 hao3"`；**0 = 由 LexCode 键派生** |
| 4 | `alt_off` | 预留，须 0 |
| 5 | `flags` | bit1 = `tier`（词内任一字需字体即 1），其余须 0 |

`LexCode` 与 `Code` 同宽：`key_off`（拼接键 `"ni3hao3"`）/ `first_entry` / `entry_count` / `flags`（须 0）。
键用字符串而非音节 id，使词组表不依赖字表的音节编号。形码侧暂不使用这两张表（计数置 0）。

### 2.5 字符串池

两产品相同：NUL 终止 UTF-8；拼音读音与词组键字符集 `[a-z0-9']`（`ü` 存 `v`）；形码文本与编码串为任意 UTF-8。

---

## 3. flags 位布局（统一，冻结）

| 位 | 拼音 | 形码 | 统一语义 |
|---|---|---|---|
| bit0 | 须 0 | 仅组词（不参与直接上屏） | 按 magic 解释；拼音恒 0 |
| bit1 | `tier` | `tier`（原字段名 `visible`，本次统一改名并反转极性） | **0 = Droid 保底可绘／可见；1 = 需额外字体** |
| bit2..31 | 须 0 | 须 0 | 保留；非零即整体拒载（宁死不猜） |

**极性变更说明**：形码原定义 `visible=1` 为可见，与拼音相反。本次统一按拼音口径改为 `tier=0` 可见。形码为全新构建、每条显式写入，反转零成本；导入器写位取反，引擎读位后转 `glyph_visible = (tier == 0)` 供 `Candidate` 使用（`FRAMEWORK-REQ.md` FR-1 的 bool 谓词保持不变）。

**bit0 注**：bit0 在两产品语义不同（拼音恒 0 / 形码仅组词），但偏移一致。公共过滤代码只读 bit1，不读 bit0，故不构成冲突。

---

## 4. 排序契约（按产品，骨架一致）

- 拼音：Entry 按 `(syl_id, rank, codepoint)` 升序，同一音节的字构成连续段 `[first_entry, first_entry + entry_count)`。
- 形码：同一 Code 段内 Entry 按 `(rank, text 码点)` 升序且连续。
- 共同约束：段内连续、`first_entry`/`entry_count` 与段边界严格一致、加载时全量校验。

---

## 5. 加载校验清单（统一骨架，两产品共用一套代码）

1. `magic` **等于调用方期望的那一个**（拼音 `ROSPYDAT`、形码 `ROSSHDAT`）；共享校验把期望 magic 作为参数传入，**不得**接受另一产品的 magic。理由：按 `ROSSHDAT` 读成拼音表不会报错，只会静默产出垃圾候选（槽0 的池偏移被当成码点、槽2 的字节数被当成 `syl_id`），失败必须发生在加载期而不是候选列表里；
2. `version == 2`（唯一版本；v1 已停止支持，见 §6）；
3. `header.flags == 0`；
4. `entry_table_off == header_size`（80）；`code_table_off == entry_table_off + entry_count × entry_size`；`lex_entry_table_off == code_table_off + code_count × code_size`；`lex_code_table_off == lex_entry_table_off + lex_entry_count × entry_size`；`pool_off == lex_code_table_off + lex_code_count × code_size`；`pool_off + pool_size ≤ 文件大小`（**64 位算术**，32 位会回绕）；`entry_size`/`code_size` 须等于 24/16；
5. 每个 Code：槽0 池偏移可解析、键串合法（拼音过 `is_dat_reading`，形码编码串字符 ∈ `alphabet`）；
6. 每个 Entry：槽1 `rank` 合法、槽5 `flags` 除 bit0/bit1 外全 0。**`rank` 合法 = `rank ≥ 1` 或 `rank == 0xFFFFFFFF`**（无频序哨兵，`kUnknownRank`）；拼音 44,355 条里 34,457 条取哨兵，按 `rank > 0` 收紧会让生僻字整批拒载；
7. 拼音额外：`syl_id < code_count`、读音偏移合法；**槽4 = `alt_off`，不得校验为 0**（多音字次读音 109 条，含「谁/重/嗯」，靠它建别名索引；本文 §2.2「保留，须 0」只对形码成立）；形码额外：`text_off/text_len` 池范围合法、`entry_type` ∈ {0,1}、`max_code_len` ∈ 1..8、`auto_commit` ∈ 0..2；
8. 任一失败 → `clear()` + 返回 0，不部分加载。

> `entry_size` 与 `header_size` 按 version 分支，与旧 `DAT-FORMAT-V2.md` §4 第4条的思路一致，扩展为两个尺寸都分支；不允许出现「v1 一套校验、v2 另一套校验」的代码漂移——共用主体，仅尺寸常量与产品专属字段分支。

---

## 6. v1：已停止支持（拼音，2026-11-20 修订 B）

- `version == 1`（Header 52B、Entry 20B、无 flags）**一律拒载**，不做降级读。
- 理由：v1 条目**没有 tier 位**。支持全字形开关的 IME 去读 v1 表，只能在「全部当可绘」（关闭态出方框）
  与「全部当需字体」（关闭态几乎无候选）之间二选一，两种都是骗用户。宁可明确拒载并要求换表。
- 后果是**降级而不是失败**：拼音内置表是编译进二进制的 C++ 数组（`pinyin/src/scheme.cpp` 的
  `kBuiltin[]`），不经 dat；外部 `rospinyin.dat` 读失败即回退内置表（候选变少、多音字消失）。
- **发布纪律**：`.ime` 与 `rospinyin.dat` 必须同源同版发布；安装器 `/step1` 两文件同拷天然满足，
  手工只替换其一会破。
- 形码无历史文件（未开工），本来就不需要兼容路径。

---

## 7. 需 framework / 拼音 owner 同步的改动（本文不执行，仅列出）

> 以下文件均在 `xingma/` 外。**2026-11-20 已由 framework/拼音 owner 全部执行**（提交 `30a63fa`），
> 表中内容保留为变更记录；拼音侧的具体落地口径见 `docs/plan/DAT-FORMAT-V2.md`，两文冲突时仍以本文为准。

| 文件 | 改动 |
|---|---|
| `docs/plan/DAT-FORMAT-V2.md` | 已按修订 B 重写：Header 80B（含词组两表）、词组同文件、v1 不再兼容、校验清单与审计 |
| `framework/include/imfw/dat_format.hpp` | Header 结构 80B（含词组两表、`entry_size`/`code_size`）、Entry 24B、flags 位名统一为 `tier`、**删除 v1 结构** |
| `framework/src/code_table.cpp` | 校验按 §5；header_size/entry_size 按 version 分支；公共过滤只读 bit1 |
| `tools/mkdat/mkdat.cpp` | 产出 80B Header、24B Entry、16B Code；词组表计数 0；tier 写入口径 `∈cmap ⇒ bit1=0` |
| `tools/coverage_report.py` / `tests/check_mkdat.py` | 24B Entry 解析、tier0 口径（保底）报告 |
| `framework/include/imfw/engine.hpp` | `Candidate` 字段由 FR-1 决定；谓词 `glyph_visible = (tier == 0)` |

## 8. xingma 侧同步点（本文作者执行）

- `XINGMA-PLAN.md` §4.1：shape.dat 升级为统一格式 v2（Header **80B**/Entry 24B/Code 16B/词组两表），`visible` 改名 `tier` 且极性反转。
- `FRAMEWORK-REQ.md` FR-1/FR-3/FR-8：可见性字段语义对齐新极性（bool 谓词不变）。
- `GLYPH-TIER-PLAN.md`：合并后模型段同步字段名与极性。
- 待建 `shape_dat_format.hpp` / `check_shape_dat.py`：按本文骨架实现（槽位复用、tier 极性、v1 无兼容需求）。

---

## 9. 取舍与风险

| 项 | 说明 |
|---|---|
| 槽位复用可读性 | Entry 槽0/2/3/4 按 magic 解释，需靠字段名注释与 `magic` 断言防止误用；收益是 rank/flags 固定偏移，公共过滤代码无需分支 |
| bit0 语义分叉 | 拼音恒 0 / 形码仅组词，偏移一致但含义不同；公共代码禁止读 bit0，隔离在各自引擎 |
| 拼音 v1 兼容 | **已放弃**（2026-11-20 修订 B）：v1 条目没有 tier 位，降级读等于骗用户；旧表被拒后运行时回退内置数组 |
| 跨目录同步 | 本文只写 xingma/；§7 的同步若遗漏，会出现「shape.dat 按新格式、rospinyin.dat 按旧格式」的不一致——需在 framework 开工前确认 §7 全部落定 |

---

## 10. 拼音侧裁决回执（2026-11-20，owner 裁定）

拼音落地时发现本文有四处按形码视角写死、直接照做会破坏拼音功能，owner 裁定如下（本文已同步修订，不另立第二套口径）：

| # | 原文位置 | 原文 | 裁定 |
|---|---|---|---|
| 1 | §5 第 1 条 | `magic` ∈ {两个 magic} | **改为按调用方传入的期望 magic 校验**；拼音只认 `ROSPYDAT`，形码只认 `ROSSHDAT` |
| 2 | §5 第 6 条 | 「`rank` 合法」未定义 | **明文定义**：`rank ≥ 1` 或 `rank == 0xFFFFFFFF`；哨兵是合法值 |
| 3 | §2.2 槽 4 | 「保留，须 0」被当成全局校验 | **只在形码路径成立**；拼音槽 4 是 `alt_off`，不得校验为 0 |
| 4 | §2.1 第 28 行 | `24 + entry_count×24` | **公式笔误，已改为 `80 + entry_count×24`**（`entry_table_off = 80` 与该式必须自洽） |

**表族 magic 注册表**（同骨架，按 magic 分语义）：

| magic | 产品 | 说明 |
|---|---|---|
| `ROSPYDAT` | 拼音（字表 + 词组表，同一文件） | Header 80B / Entry 24B / Code 16B；槽0 = 码点（词组表里 = `text_off`） |
| `ROSSHDAT` | 形码表 | 同骨架；槽0 = `text_off` |

> **只有两种 magic**：2026-11-20 修订 B 把拼音词组表并入同一个文件（Header 加两套表），
> 原计划的第三种 magic `ROSPYLEX` 与「`header.flags` 类型位」方案一并取消——前者多一套加载入口，
> 后者让「表类型」变成文件自述位（写错即静默误读）。
