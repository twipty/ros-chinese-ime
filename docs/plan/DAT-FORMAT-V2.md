# 码表 dat 统一格式 v2 · 拼音侧冻结规范

- 状态：**已冻结（2026-11-20，owner 裁定）**。上位文本（定义方）：
  [`xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md`](../../xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md)。
  本文是**拼音侧落地文本**——把统一骨架按拼音语义逐槽展开，并补上拼音专属的校验红线与兼容读法。
  两文冲突时**以统一文本为准**；改格式须同时改两文并递增版本号。
- 适用范围：`rospinyin.dat` v2。形码 `shape.dat`（magic `ROSSHDAT`）用同一骨架、另一套槽语义，定义见上文的形码列。
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
| 48 | `source_off` | 4 | 形码专用，拼音须 0（见 §10 裁决项 4） |
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
统一文本 §5 第 1 条写的是共享校验可识别两个 magic；但把形码表当拼音表读不会报错、
只会静默产出垃圾候选（槽 0 的池偏移被当成码点、槽 2 的字节数被当成 `syl_id`）。
共享校验函数可以做成「按调用方给定 magic 校验」，拼音传 `ROSPYDAT`，形码传 `ROSSHDAT`。

**红线 2 — `rank` 必须允许哨兵值。** 44,355 条里有 34,457 条 `rank == 0xFFFFFFFF`（无频序）。
「rank 合法」= `rank ≥ 1` **或** `rank == kUnknownRank`；若共享校验按「rank > 0」收紧，
生僻字整批拒载，输入法直接废掉。

**红线 3 — 槽 4 是拼音的 `alt_off`，不得要求为 0。** 多音字次读音（109 条，含「谁/重/嗯」）
全靠它建别名索引。统一文本把槽 4 的形码语义写成「保留，须 0」，这条只对形码成立。

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
后果（已接受）：旧表 + 新 .ime 时，关闭态不做 tier 过滤——旧表本就含需扩展字体的字，
关闭态仍可能出现方框；安装器换新表即恢复「关闭态零方框」。（见 §10 裁决项 6。）

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
| 6 | `tests/code_table_test.cpp` | v1 兼容用例（全 tier0）+ v2 用例（bit1、保留位非零拒载、rank 哨兵、alt 槽） | ☐ |
| 7 | `tools/coverage_report.py` / `tests/check_mkdat.py` | 24B 条目解析；`entry_count`/`code_count` 改名；tier0 口径报告 | ☐ |
| 8 | `pinyin/src/scheme.cpp`（内置表） | 随 F-T1 重新产出（v2 或继续 v1 均可加载） | ☐ |

---

## 9. 本规范的效力

- 本文与 `dat_format.hpp` 的 static_assert 是拼音侧的两个同位锚点：改布局必须同时改两处，
  ctest 钉住结构尺寸（`HeaderV2 == 64`、`CharEntryV2 == 24`、v1 三结构尺寸不变）。
- `flags` 位只许增补、不许重排；bit1 的语义（保底可绘 = 0）是拼音关闭态过滤与形码 tier 的
  同族锚点，任一侧改动都算破坏同族公约。
- 拼音词组（M4）另立 `rospinyin-lexicon.dat`，不在本格式里扩槽（见 §10 裁决项 5）。

---

## 10. 待 owner 裁决项（本版先按「建议」口径冻结，裁决后可改）

| # | 问题 | 影响 | 本版建议（已写入上文） |
|---|---|---|---|
| 1 | 统一文本 §5 第 1 条让共享校验接受两个 magic | 拼音可能把 `shape.dat` 当拼音表静默误读 | 共享校验按调用方传 magic；拼音只认 `ROSPYDAT` |
| 2 | 统一文本 §5 第 6 条「`rank` 合法」未定义 | 若按 `rank > 0` 校验，34,457 个无频序字整批丢失 | 允许 `kUnknownRank` 哨兵 |
| 3 | 统一文本 §2.2 把槽 4 的形码语义写成「保留，须 0」 | 若被实现成全局校验，多音字次读音全废 | 槽 4 属拼音 `alt_off`，不校验为 0 |
| 4 | 拼音的 `name_off`/`alphabet_off`/`source_off`/`max_code_len`/`auto_commit` 强制 0 | 拼音失去 dat 内嵌数据版本/来源指纹的位置（溯源仍可靠 NOTICE + 构建日志 + sha256） | 暂严格 0；若要用 `source_off` 存 `unihan-18.0.0;hanziadb-mit` 之类，请裁决 |
| 5 | 统一 Entry 的槽 0/2/3 已被拼音单字语义占满 | 拼音词组不可能塞进同一 `rospinyin.dat`，M4 必须走独立文件 | 维持 M4 独立 `rospinyin-lexicon.dat`（若复用本骨架则需第三种 magic） |
| 6 | v1 旧表在新 IME 下全部 `tier = 0` | 关闭态不保证零方框（旧表含需扩展字体的字），与现状等价 | 接受降级；安装器/CI 换 v2 表即可 |
| 7 | 统一文本 §2.1 公式笔误（`24 + entry_count×24`） | 与 `entry_table_off = 64` 自相矛盾 | 按 `64 + entry_count×24`；请形码侧修正原文 |
