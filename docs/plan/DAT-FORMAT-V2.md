# 码表 dat 统一格式 v2 · 拼音侧冻结规范

- 状态：**已冻结（2026-11-20，owner 裁定）**。上位文本（定义方）：
  [`xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md`](../../xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md)。
  本文是**拼音侧落地文本**——把统一骨架按拼音语义逐槽展开，并补上拼音专属的校验红线与兼容读法。
  两文冲突时**以统一文本为准**；改格式须同时改两文并递增版本号。
- 适用范围：`rospinyin.dat` v2（单字表）。形码 `shape.dat`（magic `ROSSHDAT`）用同一骨架、另一套槽语义，定义见上文的形码列；
  拼音词组表 `rospinyin-lexicon.dat`（magic `ROSPYLEX`，M4）同样沿用本骨架，见 §11。
- 消费方：`framework/include/imfw/dat_format.hpp`（结构契约）、`tools/mkdat/`（编译器）、
  `framework/src/code_table.cpp`（运行时读取）、`tools/coverage_report.py`、`tests/check_mkdat.py`。
- 关联：`docs/plan/ROADMAP.md` §F（F-T1 分级码表）、`docs/plan/PLAN.md` §12.1。

---

## 1. 接受记录

2026-11-20 owner 裁定：**采用统一格式（同骨架、分 magic）作为新版 dat 的唯一冻结版本**，
取代此前「拼音、形码各定义一套相似格式」的方案。判定理由：

1. 两产品都还没有 v2 实现（拼音 dat 仍是 v1；形码零 C++），此刻统一的迁移成本为零；
   一旦各自实现再合并，代价是两套 load/emit/检查器同时改。
2. 公共谓词（`rank` 排序、`flags` 可见性过滤）落在**相同偏移**（Entry 槽1 / 槽5），
   排序与过滤代码可以不感知 magic。
3. 拼音不需要的槽位在头里显式置 0 而不是删掉字段——格式只有一份，版本管理与检查器只有一套。

拼音侧因此相对「独立 v2 草案」的变化：Header 52B → **64B**；Entry 24B 六槽（数值与草案相同，
槽序按统一骨架固定）；可见性统一为 **`flags` bit1 = `tier`，0 = 保底可绘**（与草案极性一致）。

**保持不变的红线**：`CodeTable::load_dat` 必须继续加载 v1 文件（仓库内置表资源、CI 产物、M0/M2 VM
资产都是 v1），见 §6。

---

## 2. 二进制布局（拼音读法）

```
[Header 64B][Entry × entry_count (24B)][SylEntry × code_count (16B)][string pool]
```

### 2.1 Header（64B；形码专用字段拼音恒 0）

| 偏移 | 字段 | 大小 | 拼音语义 |
|---|---|---|---|
| 0 | `magic[8]` | 8 | `ROSPYDAT`——**拼音加载器只认它**（见 §5 红线 1） |
| 8 | `version` | 4 | **2**（v1 为 1） |
| 12 | `flags` | 4 | 保留，须 0 |
| 16 | `entry_count` | 4 | 字条目数（原 `char_count`） |
| 20 | `code_count` | 4 | 音节数（原 `syl_count`） |
| 24 | `entry_table_off` | 4 | 须 = 64 |
| 28 | `code_table_off` | 4 | 须 = `64 + entry_count × 24` |
| 32 | `pool_off` | 4 | 须 = `code_table_off + code_count × 16` |
| 36 | `pool_size` | 4 | 池字节数；`pool_off + pool_size ≤ 文件大小` |
| 40 | `name_off` | 4 | 形码专用，拼音须 0 |
| 44 | `alphabet_off` | 4 | 形码专用，拼音须 0 |
| 48 | `source_off` | 4 | 形码专用，拼音须 0（**已裁决 2026-11-20**，见 §10 第 4 项） |
| 52 | `max_code_len` | 1 | 形码专用，拼音须 0 |
| 53 | `auto_commit` | 1 | 形码专用，拼音须 0 |
| 54 | `reserved[10]` | 10 | 须 0 |

> 统一文本 §2.1 第 28 行把公式写成 `24 + entry_count×24`，与其 §5 第 4 条
> （`entry_table_off + entry_count × entry_size`）及 `entry_table_off = 64` 矛盾；按 §5 应为
> **`64 + entry_count × 24`**。本文按正确值写，已在 §10 裁决项 7 提请形码侧修正原文。

### 2.2 Entry（24B = 6 槽 × 4B；槽序由统一骨架固定）

| 槽 | 偏移 | 字段（拼音） | 说明 |
|---|---|---|---|
| 0 | 0 | `codepoint` | UTF-32 码点 |
| 1 | 4 | `rank` | 1-based 频序；`kUnknownRank = 0xFFFFFFFF` 表示无频序（**合法值**，见 §5 红线 2） |
| 2 | 8 | `syl_id` | 音节表下标 |
| 3 | 12 | `reading_off` | 池偏移：主读音（带调数字，如 `"zho1ng"`） |
| 4 | 16 | `alt_off` | 池偏移：空格分隔次读音；0 = 无（**拼音拥有此槽**，见 §5 红线 3） |
| 5 | 20 | `flags` | 见 §3 |

形码在同一骨架上把槽 0/2/3/4 解释为 `text_off`/`text_len`/`entry_type`/保留——故实现里这些槽以
「槽位 + 按 magic 解释」的方式记述，拼音侧字段名仅在本文件与拼音代码内使用。

### 2.3 SylEntry（16B = 4 槽；与 v1 字节布局相同）

| 槽 | 偏移 | 字段（拼音） | 说明 |
|---|---|---|---|
| 0 | 0 | `name_off` | 池偏移：音节名（`"zhong"`），查表键 |
| 1 | 4 | `first_char` | 首字条目下标 |
| 2 | 8 | `char_count` | 连续段长度 |
| 3 | 12 | `reserved` | 统一骨架里名为 `flags`（形码放简码标记）；**拼音须 0** |

v2 未改动 SylEntry 的字节布局，v1 结构体可直接复用；槽 3 由「不校验」收紧为「须 0」（仅 v2 表）。

### 2.4 字符串池

NUL 终止 UTF-8。读音字符集仍为 `[a-z0-9']`（`is_dat_reading`）；`ü` 族一律存 `v`
（M2-DATA-PLAN §7）。池内偏移相对池起点。

---

## 3. flags 位布局（统一，冻结）

| 位 | 拼音 | 形码 | 统一语义 |
|---|---|---|---|
| bit0 | 恒 0 | 仅组词（不参与直接上屏） | 按 magic 解释；**公共代码禁止读 bit0** |
| bit1 | `tier` | `tier`（原 `visible`） | **0 = Droid Sans Fallback 保底可绘（可见集）；1 = 需额外字体** |
| bit2..31 | 恒 0 | 恒 0 | 保留；读取器见到非零即整体拒载（宁死不猜） |

- 拼音构建口径：字的码点 **全部** ∈ Droid Sans Fallback cmap ⇒ bit1 = 0，否则 = 1。
- v1 兼容：v1 条目没有 flags ⇒ 一律视为 `tier = 0`（保底可绘），见 §6。
- 极性刻意选「0 = 可绘」：存量主体落 0，v1→v2 兼容读法成为「缺省即 0」，无需任何启发式。
- 拼音两个表的 bit0 都写 0：单字/词组的区分由 **magic** 承担（`ROSPYDAT` / `ROSPYLEX`），不借用 bit0，
  这样「公共代码禁止读 bit0」的公约不需要为拼音开例外（§11）。

---

## 4. 排序契约（不变）

Entry 按 `(syl_id, rank, codepoint)` 升序；同一音节的字构成连续段
`[first_char, first_char + char_count)`，段内 rank 升序、unknown-rank 殿后。
`flags` 不参与排序——可见性过滤是运行时谓词（F-T2），不改数据顺序。

---

## 5. 加载校验清单（拼音侧，全部必须执行）

统一文本 §5 的骨架校验之外，拼音有三条**红线**（它们决定格式是否真的支持拼音功能，
任何「共用一套代码」的重构都不得绕过）：

**红线 1 — magic 白名单只有一个。** 拼音入口（`CodeTable::load_dat`）**必须拒绝 `ROSSHDAT`**。
统一文本 §5 第 1 条原文写「`magic` ∈ {两个}」，2026-11-20 已按本裁决改为**「等于调用方期望的那一个」**：
把形码表当拼音表读不会报错、只会静默产出垃圾候选（槽 0 的池偏移被当成码点、槽 2 的字节数被当成 `syl_id`）。
共享校验函数做成「按调用方给定 magic 校验」，拼音传 `ROSPYDAT`，形码传 `ROSSHDAT`。

**红线 2 — `rank` 必须允许哨兵值。** 44,355 条里有 34,457 条 `rank == 0xFFFFFFFF`（无频序）。
「rank 合法」= `rank ≥ 1` **或** `rank == kUnknownRank`（统一文本 §5 第 6 条已补此定义）；
若共享校验按「rank > 0」收紧，生僻字整批拒载，输入法直接废掉。

**红线 3 — 槽 4 是拼音的 `alt_off`，不得要求为 0。** 多音字次读音（109 条，含「谁/重/嗯」）
全靠它建别名索引。统一文本把槽 4 的形码语义写成「保留，须 0」，该条**只在形码路径成立**，
统一文本 §5 第 7 条已加此限定。

完整清单：

1. `magic == ROSPYDAT`；`version ∈ {1, 2}`（v1 走 §6 兼容布局）；
2. `header.flags == 0`；
3. `entry_table_off == header_size`（v1 = 52 / v2 = 64）；
4. `code_table_off == entry_table_off + entry_count × entry_size`（v1 = 20 / v2 = 24）；
   `pool_off == code_table_off + code_count × 16`；`pool_off + pool_size ≤ 文件大小`；
5. 每条 SylEntry：`name_off` 可解析、音节名过 `is_dat_reading`；v2 额外要求槽 3 == 0；
6. 每条 Entry：`syl_id < code_count`、主读音可解析且过 `is_dat_reading`、`alt_off` 非 0 时
   逐个次读音过 `is_dat_reading`；`rank` 按红线 2 判定；v2 额外要求 `(flags & ~0x2u) == 0`；
7. 任一失败 → `clear()` + 返回 0，**不部分加载**。

> 实现红线（易踩）：v1 文件只有 52 字节头，**绝不能按 64 字节结构体直接 memcpy**
> （52..63 会读到 Entry 表首字节，`name_off` 变垃圾）。v1 路径只读 52 字节、其余补零；
> v1 的 SylEntry 槽 3 仍按「不校验」保持宽松（历史文件一律写 0，但不要给 v1 加新拒载条件）。

---

## 6. v1 兼容读法（已定，不拒载）

`version == 1`：Header 52B（前 40 字节与 v2 完全对齐；`reserved[3]` 落点恰是 v2 的
`name_off`/`alphabet_off`/`source_off`，v1 要求为 0，两版本兼容）、Entry 20B
（槽 0–4 与 v2 **逐槽同偏移**，无 flags）、SylEntry 16B 不变。

语义：**无 flags ⇒ 全部条目 `tier = 0`（保底可绘）**，等于维持现状，无行为回归。
后果（**2026-11-20 裁决：接受该降级**，见 §10 第 6 项）：旧表 + 新 .ime 时，关闭态不做 tier 过滤——
旧表本就含需扩展字体的字，关闭态仍可能出现方框；换 v2 表即恢复「关闭态零方框」。
落地约束：F-T1 之后随发行包发布的必须是 v2 表（安装器与 CI 产物同源），构建日志须打印
`tier0=/tier1=` 统计，作为「关闭态零方框」的验收依据；发布说明写明旧表回退的观感差异。

v2 成为现行后 `mkdat` 不再产出 v1；v1 读取路径**仅**由 `load_dat` 保留，
host 测试的手工 dat 构造可继续用 v1（现有测试样例即 v1 形态），两版本都要有用例。

---

## 7. 体积与性能（记录）

Header +12B、Entry +4B/条：44,355 字 ⇒ 约 +173 KB（902,776 B → ≈1.05 MB），
仍远低于 2 MB 目标；32 位进程常驻内存同量增加。mmap 友好、定长记录、无指针，性质不变。

---

## 8. 消费方改动清单（实施时勾选）

| # | 落点 | 改动 | 状态 |
|---|---|---|---|
| 1 | `framework/include/imfw/dat_format.hpp` | `kVersionV2`、`HeaderV2`(64B)、`CharEntryV2`(24B) + static_assert、`kFlagTier`；**v1 结构与 `kVersion` 原样保留** | ☑（2026-11-20） |
| 2 | `framework/src/code_table.cpp` | 按 version 选 header/entry 尺寸；v2 校验链含 §5 红线与第 6 条；v1 路径原样 | ☐ |
| 3 | `framework/include/imfw/code_table.hpp` | `TableEntry` 增 tier 口径；`lookup` 结果携带 | ☐ |
| 4 | `tools/mkdat/mkdat.cpp` | 产出 64B 头 / 24B 条目；吃 cmap 码点集写 bit1；统计行 `tier0=/tier1=` | ☐ |
| 5 | `tools/mkdat/build_dat.sh` | 指纹断言更新为 v2 产物 sha256 | ☐ |
| 6 | `tests/code_table_test.cpp` | v1 兼容用例（全 tier0）+ v2 用例（bit1、保留位非零拒载、rank 哨兵、alt 槽）；「未知版本」用例已改用 `kVersionV2 + 1`，`ROSSHDAT` 拒载已按 magic 名固定（2026-11-20 已做） | ☐ |
| 7 | `tools/coverage_report.py` / `tests/check_mkdat.py` | 24B 条目解析；`entry_count`/`code_count` 改名；tier0 口径报告 | ☐ |
| 8 | `pinyin/src/scheme.cpp`（内置表） | 随 F-T1 重新产出（v2 或继续 v1 均可加载） | ☐ |

---

## 9. 本规范的效力

- 本文与 `dat_format.hpp` 的 static_assert 是拼音侧的两个同位锚点：改布局必须同时改两处，
  ctest 钉住结构尺寸（`HeaderV2 == 64`、`CharEntryV2 == 24`、v1 三结构尺寸不变）。
- `flags` 位只许增补、不许重排；bit1 的语义（保底可绘 = 0）是拼音关闭态过滤与形码 tier 的
  同族锚点，任一侧改动都算破坏同族公约。
- 拼音词组（M4）另立 `rospinyin-lexicon.dat`（`ROSPYLEX`），**沿用同一骨架**，Entry 不扩充；
  槽语义与索引契约见 §11（裁决 2026-11-20）。

---

## 10. 裁决记录（2026-11-20，七项全部关闭）

| # | 议题 | 裁决 | 落地 |
|---|---|---|---|
| 1 | 共享校验是否识别两个 magic | **否**。校验函数按调用方传入的 magic 做白名单：拼音只认 `ROSPYDAT`，形码只认 `ROSSHDAT` | 本文 §5 红线 1；统一文本 §5 第 1 条已改 |
| 2 | `rank` 的合法性定义 | **`rank ≥ 1` 或 `rank == kUnknownRank`**；哨兵是合法值，不得按 `> 0` 收紧 | 本文 §5 红线 2；统一文本 §5 第 6 条已补定义 |
| 3 | 槽 4 的语义归属 | **拼音是 `alt_off`，不校验为 0**；形码「保留，须 0」只在形码路径生效 | 本文 §5 红线 3；统一文本 §5 第 7 条已加限定 |
| 4 | 拼音的 `name_off`/`alphabet_off`/`source_off`/`max_code_len`/`auto_commit` | **严格 0**：拼音 dat 不自述版本与来源，溯源仍靠 `NOTICE.txt` + 构建日志 + 发行包 sha256。若日后 `/status` 要读 dat 自身版本，单独立项放开 `source_off`，不在此处预埋 | 本文 §2.1；`dat_format.hpp` 注释 |
| 5 | 词组能否进同一格式 | **能，且 Entry 不用扩充**：同骨架、第三种 magic `ROSPYLEX`、独立文件；`Header`/`Entry`/`Code` 尺寸一律不动 | 本文 §11；`dat_format.hpp` 的 `kMagicLexicon`/`LexEntryV2`/`LexCodeV2` |
| 6 | v1 表在新 IME 下全部 `tier = 0` | **接受降级**：关闭态不保证零方框，与现状等价；换 v2 表即恢复。发行包与 CI 必须同源换 v2，构建日志打 `tier0=/tier1=` | 本文 §6 |
| 7 | 统一文本 §2.1 公式笔误 | **按 `64 + entry_count × 24`**；统一文本已就地修正 | 统一文本 §2.1 第 28 行 |

结论：**§5 的三条红线不再是「建议」，是硬约束**；以后做「共用一套 load/emit」的重构，缩掉任一条都算改格式。

---

## 11. 词组表（M4 `rospinyin-lexicon.dat`）如何沿用同一格式

### 11.1 结论先行

- **Entry 不需要扩充。** 24B 六槽能完整表达一个词组条目，不需要第 7 槽。
- 真正缺的从来不是「每条更宽」，而是两件与宽度无关的事：
  1. **表类型标签**——单字表槽 0 是码点，词组表槽 0 是池偏移；同一个 magic 下无法自证，误读不报错；
  2. **第二套索引**——单字表按「音节 → 字」分段，词组表要按「音节序列 → 词」分段，
     而 Header 只有一个 `code_table_off` / `code_count`。
- 所以「保持相同格式」的正确落法是 **同一骨架 + 第三种 magic**：结构零改动，只多一个 tag。
  扩 Entry（要第 7 槽）和扩 Header（要第二个码表指针）都不是必需品，后者还会把形码侧拖下水。

### 11.2 三种方案与取舍

| 方案 | 做法 | 结构改动 | 代价 / 结论 |
|---|---|---|---|
| **A（采纳）** | 独立文件 `rospinyin-lexicon.dat`，magic `ROSPYLEX`，Header 64B / Entry 24B / Code 16B 全沿用 | **无** | 家族多一个 magic、加载器多一组常量；与「magic 即 tag」原则完全同构 |
| B | 同一 magic `ROSPYDAT`，`header.flags` bit0 = 表类型 | 无，但要放宽 §5 第 2 条的 `flags == 0` | 类型是文件自述，写错即静默误读（magic 错配则是结构上不可能）；省一个 magic 不值这个风险 |
| C | 真单文件，Header 扩到 72B（加 `lex_code_count` / `lex_code_table_off`） | Header 64→72（Entry 仍 24） | 形码侧必须同步改 Header 否则不再同骨架：`shape_dat_format.hpp`、导入器、两文重冻结全要返工。唯一收益是「少一个文件」 |

不取 C 的判据：统一格式的全部收益来自**公共谓词固定偏移 + 一套校验代码**，而 C 为了省一个文件去改
`header`，恰好在收益最集中的地方制造分叉。A 把复杂度留在「多一个常量」这一层，代价最小。

### 11.3 A 方案的记录定义（`ROSPYLEX`，v2）

**Header**：与 §2.1 逐字段相同，只有 `magic` 换成 `ROSPYLEX`；`version = 2`；
`entry_count` = 词条数，`code_count` = 音节序列键数；五个形码专用字段
（`name_off`/`alphabet_off`/`source_off`/`max_code_len`/`auto_commit`）与 `reserved` 仍须 0
（拼音不用 `max_code_len`：音节数在 Entry 槽 2 逐条给，不在表级声明）。

**Entry（24B，六槽不扩充）**

| 槽 | 字段 | 说明 |
|---|---|---|
| 0 | `text_off` | 池偏移：词文本 UTF-8，NUL 终止（如 `"你好"`）；空串非法 |
| 1 | `rank` | 与单字表同义，越小越靠前；`kUnknownRank` 合法 |
| 2 | `syl_count` | 音节数，**≥ 2**；用于「已输入音节数」与简拼判定 |
| 3 | `reading_off` | 池偏移：规范读音 `"ni3 hao3"`（空格分隔）。**0 = 由 Code 键派生**（内置词表按 0 写，池里不再重复存一份读音，10 万词省约 1 MB）；非 0 时必须与键自洽 |
| 4 | `alt_off` | 预留，须 0（词级异读用「同一个词挂到另一个 Code 段」表达，不占此槽） |
| 5 | `flags` | bit1 = `tier`：**词内任一字**需额外字体即 1；bit0 与高位须 0（单字/词组的区分由 magic 承担，见 §3） |

**Code（16B，与 SylEntry 同布局，语义 = 音节序列）**

| 槽 | 字段 | 说明 |
|---|---|---|
| 0 | `key_off` | 池偏移：拼接键 `"ni3hao3"`（无分隔、带调数字，字符集仍 `[a-z0-9']`） |
| 1 | `first_entry` | 首词条下标 |
| 2 | `entry_count` | 段长 |
| 3 | `reserved` | 须 0（预留：前缀/简拼标记） |

键用「带调数字拼接」而不是音节 id 序列，是为了让词组表**不依赖单字表的音节编号**——
两份表可独立重建、独立更新，不会因为单字表重排音节而整表作废。

### 11.4 排序与查找契约

- Code 段按**键串字节序**升序（不是按池偏移），运行时在键池上二分：先比较键串再落到段；
  这让「输入音节序列 → 精确匹配」是 `O(log n)`，`key_off` 只作为取串句柄。
- 段内 Entry 按 `(rank, 词文本码点序列)` 升序且连续，`first_entry`/`entry_count` 与段边界严格一致。
- `tier` 不进排序键（与 §4 一致），可见性过滤仍是运行时谓词。
- 前缀输入（`ni` 尚未选完 `hao`）由引擎在键池上做前缀扫描，格式层不额外建索引。

### 11.5 校验补充（在 §5 清单之上，仅 `ROSPYLEX`）

1. `magic == ROSPYLEX`（不接受 `ROSPYDAT`——反过来也一样，两个表不能互读）；
2. `text_off` 可解析、非空、UTF-8 合法；`syl_count ≥ 2`；`alt_off == 0`；
3. `reading_off` 非 0 时：可解析、按空格切分后段数 == `syl_count`、每段过 `is_dat_reading`；
   为 0 时以 Code 键为准；
4. 每个 Code：`first_entry + 段长 ≤ header.entry_count`，且相邻段首尾相接、无重叠无空隙；
5. 构建期等价性（运行期不做，避免 `O(n·len)` 扫描）：**Code 键串 == 该段读音去空格**，
   由 `tests/check_mkdat.py` 在构建门禁里逐段核对；运行期只校验格式本身。

### 11.6 体积量级（记录）

10 万词、平均 2.2 字、约 5 万个键：条目 2.4 MB + Code 0.8 MB + 池（文本 ≈0.8 MB、键 ≈0.5 MB）≈ **4.5 MB**。
独立文件的好处正在此处：单字表（≈1 MB）常驻、词组表可按词频阈值裁剪后再装（首版 3 万词 ≈1.4 MB），
两者互不牵连；若塞进同一个文件，任何裁剪都会迫使单字表重新发布。

### 11.7 落地时序

M4 开工前只需在 `dat_format.hpp` 落三个符号（`kMagicLexicon`、`LexEntryV2`、`LexCodeV2`，均已随本次冻结加入），
`mkdat` 增加 `--lexicon` 产物模式，加载器增加 `load_lexicon_dat`；`rospinyin.dat` 与本节零耦合。

---

## 12. 兼容性审计（2026-11-20，按代码事实核过，非按文档推断）

| # | 场景 | 代码事实 | 结论 |
|---|---|---|---|
| 1 | 存量 v1 表（内置表 / CI 产物 / M0·M2 VM 资产） | `load_dat` 校验 `magic == ROSPYDAT` 且 `version == 1`；三个表偏移按 52B 头 + 20B 条目算 | **可读，无回归**；全部条目语义 `tier = 0` |
| 2 | 今天的加载器遇到 v2 表 | `code_table.cpp` 在第 57 行先判 `version != kVersion` 就返回 0，早于任何偏移运算 | **明确拒载，不会半读**（不是静默误读） |
| 3 | 把 v1 文件按 64B 头读 | v1 路径只 `read_record<dat::Header>`（52B） | 不存在风险；已写成实现红线（§5 尾注） |
| 4 | `shape.dat` 被当拼音表读 | `memcmp(magic, kMagic, 8)`；本次新增按名钉住的用例（`kMagicShape` → 拒载） | **已封**（红线 1 有测试） |
| 5 | 拼音表被当形码表读 | 同一 magic 机制；形码加载器尚未存在 | 机制已定，**形码实现时须自测**（形码侧责任） |
| 6 | 旧 tier 编号 1/2 的残留 | 全仓 `*.cpp/*.hpp` 零引用 `tier`；文档已统一 0 = 可绘 | 无冲突 |
| 7 | `mkdat` 与加载器的版本口径 | `mkdat` 仍写 `header.version = kVersion`（=1） | **今天一致**；F-T1 必须在同一提交里改 `mkdat` + loader + `build_dat.sh` 指纹 |
| 8 | 「未知版本」用例 | 原用 `kVersion + 1`(=2)，v2 一旦可读会从「拒载」变成「解析」 | **已修**：改为 `kVersionV2 + 1`，并新增 `ROSSHDAT` 拒载用例 |
| 9 | 词组表与单字表互读 | 两个 magic 互不接受 | 不互读；`ROSPYLEX` 尚无读取方 |
| 10 | `flags` 保留位策略 | 读侧「见非零即拒」，写侧恒 0（拼音两表的 bit0 都是 0） | 无歧义 |

**残留欠账（按时间排序，不是缺陷）**：

1. **v2 读取尚未实现**：今天任何 v2 表都不可用，发行包必须继续发 v1 表，直到 F-T1 在同一提交里切换
   `mkdat` / `load_dat` / 指纹断言三处——三处不同步就会出现「新表旧 IME」或反之。
2. **形码侧没有加载器与校验脚本**：§10 的回执只保证两份**文档**一致；形码实现时须自行满足
   统一文本 §5 第 6/7 条（`rank` 哨兵、槽 4 的归属）。
3. **v1 表的 tier 全 0**（裁决 6）：换成 v2 表之前，关闭态「零方框」无法保证。
4. **Header 的 `reserved[10]` 没有演进空间**：将来要加表级字段只能 bump `version`。这是刻意的取舍
   （宁可贵一次版本，也不要「半保留字段」），不是遗漏；词组需求已经用第三种 magic 消化掉了。
