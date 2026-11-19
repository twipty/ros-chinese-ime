# 统一 dat 格式规范（拼音 `rospinyin.dat` 与形码 `shape.dat`）

- 状态：**xingma 侧定义，待拼音/framework owner 同步**（2026-11-20）。拼音侧的对应规范 `docs/plan/DAT-FORMAT-V2.md`（xingma 目录外）需按本文在定稿前同步修订；本文不修改该文件。
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
[Header 64B][Entry × entry_count (24B each)][Code × code_count (16B each)][string pool]
```

### 2.1 Header（64B，两产品相同骨架）

| 偏移 | 字段 | 大小 | 拼音语义 | 形码语义 |
|---|---|---|---|---|
| 0 | `magic[8]` | 8 | `ROSPYDAT` | `ROSSHDAT` |
| 8 | `version` | 4 | 2（v1 为 1） | 2 |
| 12 | `flags` | 4 | 保留，须 0 | 保留，须 0 |
| 16 | `entry_count` | 4 | 原 `char_count`：CharEntryV2 条数 | Entry 条数 |
| 20 | `code_count` | 4 | 原 `syl_count`：SylEntry 条数 | Code 条数 |
| 24 | `entry_table_off` | 4 | 原 `char_table_off`，须 = 64 | Entry 表偏移，须 = 64 |
| 28 | `code_table_off` | 4 | 原 `syl_table_off`，须 = 24 + entry_count×24 | 同式 |
| 32 | `pool_off` | 4 | 须 = code_table_off + code_count×16 | 同式 |
| 36 | `pool_size` | 4 | 池字节数；pool_off + pool_size ≤ 文件大小 | 同 |
| 40 | `name_off` | 4 | 拼音不用，须 0 | 池偏移：显示名，空串非法 |
| 44 | `alphabet_off` | 4 | 拼音不用，须 0 | 池偏移：编码字母表 |
| 48 | `source_off` | 4 | 拼音不用，须 0 | 池偏移：来源指纹，0=无 |
| 52 | `max_code_len` | 1 | 拼音不用，须 0 | 1..8 |
| 53 | `auto_commit` | 1 | 拼音不用，须 0 | 0/1/2（Q6 已决：2 保留为显式逃生口） |
| 54 | `reserved[10]` | 10 | 须 0 | 须 0 |

> 拼音侧相对旧定义的变化：`Header` 由 52B 扩到 64B，`reserved[3]`（@40..51）被 name_off/alphabet_off/source_off/max_code_len/auto_commit 占用。因拼音 v2 代码未实现，此变更零成本；拼音 v1 文件按 §6 兼容读法处理。

### 2.2 Entry（24B = 6 槽 × 4B，两产品相同骨架）

| 槽 | 偏移 | 拼音语义 | 形码语义 | 关系 |
|---|---|---|---|---|
| 0 | 0 | `codepoint`（UTF-32 码点） | `text_off`（池偏移，UTF-8 文本） | 按 magic 解释 |
| 1 | 4 | `rank`（1-based 频序） | `rank`（越小越靠前） | **真共用，同偏移** |
| 2 | 8 | `syl_id`（音节表下标） | `text_len`（文本字节数） | 按 magic 解释 |
| 3 | 12 | `reading_off`（主读音池偏移） | `entry_type`（0=单字, 1=词） | 按 magic 解释 |
| 4 | 16 | `alt_off`（次读音池偏移，0=无） | 保留，须 0 | 按 magic 解释 |
| 5 | 20 | `flags`（见 §3） | `flags`（见 §3） | **真共用，同偏移** |

> 形码侧相对旧定义的变化：`Entry` 由 20B 扩到 24B（原 flags 在槽4，现移到槽5；新增槽4 保留位）。字段重排仅为对齐公共谓词偏移，语义不变。

### 2.3 Code / SylEntry（16B = 4 槽 × 4B，语义一一对应）

| 槽 | 偏移 | 拼音 `SylEntry` | 形码 `Code` | 关系 |
|---|---|---|---|---|
| 0 | 0 | `name_off`（音节名池偏移） | `code_off`（编码串池偏移） | 统一名 `key_off`，按 magic 解释 |
| 1 | 4 | `first_char`（字表首下标） | `first_entry`（Entry 首下标） | 统一名 `first_entry` |
| 2 | 8 | `char_count`（连续段长度） | `entry_count`（连续段长度） | 统一名 `entry_count` |
| 3 | 12 | `reserved`（须 0） | `flags`（bit0 一级简码…bit4 含识别码） | 统一名 `flags`，拼音须 0 |

### 2.4 字符串池

两产品相同：NUL 终止 UTF-8；拼音读音字符集 `[a-z0-9']`（`ü` 存 `v`）；形码文本与编码串为任意 UTF-8。

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

1. `magic` ∈ {`ROSPYDAT`, `ROSSHDAT`}；
2. `version` ∈ {1, 2}；v2 用本文布局，v1 用 §6 兼容布局；
3. `header.flags == 0`；
4. `entry_table_off == header_size`（v1=52 / v2=64）；`code_table_off == entry_table_off + entry_count × entry_size`（v1=20 / v2=24）；`pool_off == code_table_off + code_count × 16`；`pool_off + pool_size ≤ 文件大小`；
5. 每个 Code：槽0 池偏移可解析、键串合法（拼音过 `is_dat_reading`，形码编码串字符 ∈ `alphabet`）；
6. 每个 Entry：槽1 `rank` 合法、槽5 `flags` 除 bit0/bit1 外全 0；
7. 拼音额外：`syl_id < code_count`、读音偏移合法；形码额外：`text_off/text_len` 池范围合法、`entry_type` ∈ {0,1}、`max_code_len` ∈ 1..8、`auto_commit` ∈ 0..2；
8. 任一失败 → `clear()` + 返回 0，不部分加载。

> `entry_size` 与 `header_size` 按 version 分支，与旧 `DAT-FORMAT-V2.md` §4 第4条的思路一致，扩展为两个尺寸都分支；不允许出现「v1 一套校验、v2 另一套校验」的代码漂移——共用主体，仅尺寸常量与产品专属字段分支。

---

## 6. v1 兼容读法（拼音）

- `version == 1` 的文件：Header 52B、Entry 20B（codepoint, rank, syl_id, reading_off, alt_off，**无 flags**）、SylEntry 16B 不变。
- 语义：无 flags ⇒ 所有条目 `tier = 0`（保底可绘）＝ 维持现状，无行为回归。
- 读法：version=1 时槽5 flags 视为 0、槽4 仍为 `alt_off`；version=2 时按 §2.2。两条路径共用一个加载函数，仅尺寸与槽4/槽5 解释分支。
- 形码无 v1 文件（未开工），不需要兼容路径。

---

## 7. 需 framework / 拼音 owner 同步的改动（本文不执行，仅列出）

> 以下文件均在 `xingma/` 外，本文作者无权修改，列出供 owner 执行：

| 文件 | 改动 |
|---|---|
| `docs/plan/DAT-FORMAT-V2.md` | Header 52B→64B（§3.1）、Entry 槽位重排（§3.2）、极性统一（§3.2 位表反转）、§2 同族公约中「极性相反」表述删除、§5 v1 兼容读法保留并补 header_size 分支、§7 消费方清单同步 |
| `framework/include/imfw/dat_format.hpp` | Header 结构扩 64B、CharEntryV2 槽位重排、flags 位名统一为 `tier`、保留 v1 结构 |
| `framework/src/code_table.cpp` | 校验按 §5；header_size/entry_size 按 version 分支；公共过滤只读 bit1 |
| `tools/mkdat/mkdat.cpp` | 产出 64B Header、24B Entry；tier 写入口径 `∈cmap ⇒ bit1=0` |
| `tools/coverage_report.py` / `tests/check_mkdat.py` | 24B Entry 解析、tier0 口径（保底）报告 |
| `framework/include/imfw/engine.hpp` | `Candidate` 字段由 FR-1 决定；谓词 `glyph_visible = (tier == 0)` |

## 8. xingma 侧同步点（本文作者执行）

- `XINGMA-PLAN.md` §4.1：shape.dat 升级为统一格式 v2（Header 64B/Entry 24B/Code 16B），`visible` 改名 `tier` 且极性反转。
- `FRAMEWORK-REQ.md` FR-1/FR-3/FR-8：可见性字段语义对齐新极性（bool 谓词不变）。
- `GLYPH-TIER-PLAN.md`：合并后模型段同步字段名与极性。
- 待建 `shape_dat_format.hpp` / `check_shape_dat.py`：按本文骨架实现（槽位复用、tier 极性、v1 无兼容需求）。

---

## 9. 取舍与风险

| 项 | 说明 |
|---|---|
| 槽位复用可读性 | Entry 槽0/2/3/4 按 magic 解释，需靠字段名注释与 `magic` 断言防止误用；收益是 rank/flags 固定偏移，公共过滤代码无需分支 |
| bit0 语义分叉 | 拼音恒 0 / 形码仅组词，偏移一致但含义不同；公共代码禁止读 bit0，隔离在各自引擎 |
| 拼音 v1 兼容 | 需 header_size 分支；`DAT-FORMAT-V2.md` 原「header 与 v1 完全相同」的表述随之作废，由 owner 同步 |
| 跨目录同步 | 本文只写 xingma/；§7 的同步若遗漏，会出现「shape.dat 按新格式、rospinyin.dat 按旧格式」的不一致——需在 framework 开工前确认 §7 全部落定 |
