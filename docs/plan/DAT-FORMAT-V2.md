# 码表 dat 格式 v2 · 拼音侧冻结规范

- 状态：**已冻结并已实现（2026-11-20 修订 B，owner 裁定）**。上位文本（定义方）：
  [`xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md`](../../xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md)。
  本文是**拼音侧落地文本**——把统一骨架按拼音语义逐槽展开，并补上拼音专属的校验红线。
  两文冲突时**以统一文本为准**；改格式须同时改两文。
- 适用范围：`rospinyin.dat` v2，**一个文件同时承载单字表与词组表**。形码 `shape.dat`（magic `ROSSHDAT`）
  用同一骨架、另一套槽语义，定义见上文的形码列。
- 消费方：`framework/include/imfw/dat_format.hpp`（结构契约）、`tools/mkdat/`（编译器）、
  `framework/src/code_table.cpp`（运行时读取）、`tools/coverage_report.py`、`tests/check_mkdat.py`。
- 关联：`docs/plan/ROADMAP.md` §F（F-T1 分级码表）、`docs/plan/PLAN.md` §12.1。

---

## 1. 修订 B 的三条变更（2026-11-20）

修订 A（同日更早）冻结的是「64B 头 + 拼音/词组分两个文件、各用一套 magic，并兼容读 v1」。
owner 随后给出三条指令，本版据此重做，**修订 A 的相关条款全部作废**：

| # | 指令 | 落地结果 |
|---|---|---|
| 1 | **不考虑与 v1 兼容性** | v1（52B 头 / 20B 条目）**不再可读**：`load_dat` 直接拒载，不做「按 tier 0 降级读」。理由见 §1.1 |
| 2 | **调整 header** | Header 52→64→**80B**：加自描述的 `entry_size`/`code_size`，加**词组两套表**的计数与偏移。原先「词组另立文件 + 第三种 magic `ROSPYLEX`」的方案作废——Header 既然要改，就把第二套表放进来（§1.2） |
| 3 | **补齐缺口，修正 bug** | 按代码事实逐条核过（§12）：段区间改为**前缀和 tiling** 并强制校验；`rank == 0` 由「静默接受」改为拒载；`flags` 保留位由「不查」改为拒载；词组表「声明了就必须自洽」；`kVersion + 1` 冒充「未知版本」的用例改为永不复用的版本号 |

### 1.1 为什么可以不要 v1

v1 唯一的消费者是「安装目录里那份 `rospinyin.dat`」，而**内置表不是 .dat**：
`pinyin/src/scheme.cpp` 的 `kBuiltin[]` 是编译进二进制的 C++ 数组。`load_dat` 读失败时
`pinyin/host/dat_source_win.cpp` 回退到内置表（候选变少，功能还在）。所以丢 v1 的代价是
「旧安装包里的旧表读不了 → 回落内置表」，不是「输入法起不来」。

真正的理由不是「省代码」：v1 条目**没有 tier 位**。一个声称支持全字形开关的 IME 去读 v1 表，
只能在「全部当可绘」（关闭态出方框）和「全部当需字体」（关闭态几乎无候选）之间二选一，
两种都是骗用户。**宁可明确拒载并要求换表**。

### 1.2 为什么词组进同一个文件

修订 A 把词组挡在外面，理由是「Header 只有一个码表指针，扩 Header 会连带形码侧返工」。
owner 已接受这次返工，于是：

- 词条**不需要第 7 个槽**（24B 六槽够用，见 §2.3），所以 `entry_size` 仍是 24；
- 缺的只是**第二套码表指针**（音节序列 → 词），加 4 个 u32 即可；
- 换来的是：一个文件、一份校验、一次安装；**不再需要第三种 magic**，也不需要
  「按 magic 判表类型」这种自述位（自述位写错就静默误读，见 §10 第 5 项）。

---

## 2. 二进制布局

```
[Header 80B][CharEntry × entry_count (24B)][SylEntry × code_count (16B)]
            [LexEntry × lex_entry_count (24B)][LexCode × lex_code_count (16B)][pool]
```

四张定长表首尾相接、无空隙无重叠；`pool_off` 必须正好落在最后一张表之后。

### 2.1 Header（80B）

| 偏移 | 字段 | 拼音语义 |
|---|---|---|
| 0 | `magic[8]` | `ROSPYDAT`——**拼音加载器只认它**（§5 红线 1） |
| 8 | `version` | **2**（唯一版本；1 已拒载） |
| 12 | `flags` | 保留，须 0 |
| 16 | `entry_count` | 单字条目数 |
| 20 | `code_count` | 音节数 |
| 24 | `lex_entry_count` | 词条数；**0 = 无词组表** |
| 28 | `lex_code_count` | 音节序列键数 |
| 32 | `entry_table_off` | 须 = 80 |
| 36 | `code_table_off` | 须 = `entry_table_off + entry_count × entry_size` |
| 40 | `lex_entry_table_off` | 须 = `code_table_off + code_count × code_size` |
| 44 | `lex_code_table_off` | 须 = `lex_entry_table_off + lex_entry_count × entry_size` |
| 48 | `pool_off` | 须 = `lex_code_table_off + lex_code_count × code_size` |
| 52 | `pool_size` | 池字节数；`pool_off + pool_size ≤ 文件大小` |
| 56 | `name_off` | 形码专用，拼音须 0 |
| 60 | `alphabet_off` | 形码专用，拼音须 0 |
| 64 | `source_off` | 形码专用，拼音须 0（§10 第 4 项） |
| 68 | `entry_size` | 须 = 24（单字与词条同宽） |
| 72 | `code_size` | 须 = 16（音节与词组键同宽） |
| 76 | `max_code_len` | 形码专用，拼音须 0 |
| 77 | `auto_commit` | 形码专用，拼音须 0 |
| 78 | `reserved[2]` | 须 0 |

**自描述尺寸的用处**：`entry_size`/`code_size` 与 24/16 不符即拒载，能抓住「版本号换了、记录宽度没换」
的半升级文件——这类文件若按新宽度硬读，第一条记录之后全是垃圾，而错在偏移上的报错通常指向别处。

### 2.2 CharEntry（24B = 6 槽）

| 槽 | 偏移 | 字段 | 说明 |
|---|---|---|---|
| 0 | 0 | `codepoint` | UTF-32 码点 |
| 1 | 4 | `rank` | 1-based 频序；**`kUnknownRank`（0xFFFFFFFF）合法**（§5 红线 2） |
| 2 | 8 | `syl_id` | 音节表下标 |
| 3 | 12 | `reading_off` | 池偏移：主读音（带调数字，如 `"zho1ng"`） |
| 4 | 16 | `alt_off` | 池偏移：空格分隔次读音；0 = 无（**拼音拥有此槽**，§5 红线 3） |
| 5 | 20 | `flags` | bit1 = `tier`，见 §3 |

### 2.3 LexEntry（24B，与 CharEntry 同宽不同义）

| 槽 | 字段 | 说明 |
|---|---|---|
| 0 | `text_off` | 池偏移：词文本 UTF-8，NUL 终止（如 `"你好"`），空串非法 |
| 1 | `rank` | 同 CharEntry |
| 2 | `syl_count` | 音节数，**≥ 2**（1 个音节是字，不是词） |
| 3 | `reading_off` | 池偏移：`"ni3 hao3"`；**0 = 由 Code 键派生**（内置词表按 0 写，池里不重复存读音） |
| 4 | `alt_off` | 预留，须 0（词级异读用「同一个词挂到另一个 Code 段」表达） |
| 5 | `flags` | bit1 = `tier`：**词内任一字**需额外字体即 1；其余位须 0 |

### 2.4 SylEntry（16B）/ LexCode（16B）

| 槽 | SylEntry | LexCode |
|---|---|---|
| 0 | `name_off`：音节名（`"zhong"`） | `key_off`：拼接键（`"ni3hao3"`） |
| 1 | `first_entry`：首字条目下标 | `first_entry`：首词条下标 |
| 2 | `entry_count`：段长 | `entry_count`：段长 |
| 3 | `flags`：拼音须 0 | `flags`：须 0（预留：前缀/简拼标记） |

词组的键是**字符串**而不是音节 id 序列：这样词组表不依赖单字表的音节编号，两份表可独立重建、
独立更新，不会因为单字表重排音节而整表作废。

### 2.5 字符串池

NUL 终止 UTF-8。读音/键的字符集为 `[a-z0-9']`（`is_dat_reading`）；`ü` 族一律存 `v`。
池内偏移相对池起点。

---

## 3. flags 位布局（统一，冻结）

| 位 | 拼音 | 形码 | 统一语义 |
|---|---|---|---|
| bit0 | 恒 0 | 仅组词（不参与直接上屏） | 按 magic 解释；**公共代码禁止读 bit0** |
| bit1 | `tier` | `tier` | **0 = Droid Sans Fallback 保底可绘（可见集）；1 = 需额外字体** |
| bit2..31 | 恒 0 | 恒 0 | 保留；读取器见到非零即整体拒载（宁死不猜） |

- 拼音构建口径：字的码点 **全部** ∈ Droid Sans Fallback cmap ⇒ bit1 = 0，否则 = 1。
  cmap 由 [`tools/mkdat/export_cmap.py`](../../tools/mkdat/export_cmap.py) 从 ReactOS 自带字体导出为
  [`tools/mkdat/data/droid_cmap.txt`](../../tools/mkdat/data/droid_cmap.txt)（29,622 码点，
  sha256 钉在 `build_dat.sh`），mkdat 构建期写入。**2026-11-20 分级已落实**：
  44,355 字中 tier0 = 16,500、tier1 = 27,855；有频序的 9,898 字里 9,092 是 tier0，
  首屏 500 字覆盖 96.8% 不受影响（tier 不进排序键）。
- 拼音不借 bit0 区分字/词：区分由**所在的表**承担（§2.3），所以 bit0 在拼音两张表里都是 0，
  「公共代码禁止读 bit0」这条公约不需要为拼音开例外。

---

## 4. 排序与分段契约

### 4.1 单字表

CharEntry 按 `(syl_id, rank, codepoint)` 升序；同一音节的字构成连续段
`[first_entry, first_entry + entry_count)`，段内 rank 升序、unknown-rank 殿后。

**段必须严格 tiling**：音节 i 的 `first_entry` == 前 i 个音节 `entry_count` 之和，
末段末尾 == `header.entry_count`。mkdat 用前缀和写入（空音节也写正确位置），加载器用一个
累加器校验。修订 A 之前空音节的 `first_entry` 是 0，导致这条契约根本无法被检查——
现在它是「一个计数器就能验」的硬约束。

### 4.2 词组表

LexCode 段按**键串字节序**升序（不是按池偏移），运行时可对键池二分；段内 LexEntry 按
`(rank, 文本码点序列)` 升序且连续；段同样严格 tiling（从 0 开始、首尾相接、末尾等于
`lex_entry_count`）。`tier` 不进排序键。

---

## 5. 加载校验清单（`CodeTable::load_dat`，全部必须执行）

统一文本 §5 的骨架校验之外，拼音有三条**红线**（决定格式是否真的支持拼音功能，
任何「共用一套代码」的重构都不得绕过）：

**红线 1 — magic 白名单只有一个。** 拼音入口**必须拒绝 `ROSSHDAT`**：把形码表当拼音表读不会报错，
只会静默产出垃圾候选（槽 0 的池偏移被当成码点、槽 2 的字节数被当成 `syl_id`）。
共享校验做成「按调用方给定 magic 校验」，拼音传 `ROSPYDAT`，形码传 `ROSSHDAT`。

**红线 2 — `rank` 必须允许哨兵值。** 44,355 条里 34,457 条 `rank == 0xFFFFFFFF`（无频序）。
「rank 合法」= `rank ≥ 1` **或** `rank == kUnknownRank`。反过来 `rank == 0` 是**非法**的
（既不是频序也不是哨兵），拒载。

**红线 3 — 槽 4 是拼音的 `alt_off`，不得要求为 0。** 多音字次读音（118 条，含「谁/重/嗯」）
全靠它建别名索引。统一文本里「槽 4 保留须 0」那条**只对形码成立**。

完整清单（顺序即实现顺序，先便宜后昂贵）：

1. `magic == ROSPYDAT`；`version == 2`（其它版本一律拒，不做降级读）；
2. `header.flags == 0`；五个形码专用字段（`name_off`/`alphabet_off`/`source_off`/`max_code_len`/`auto_commit`）
   与 `reserved[2]` 全 0；
3. `entry_size == 24`、`code_size == 16`；
4. 偏移链：`entry_table_off == 80` → `code_table_off` → `lex_entry_table_off` → `lex_code_table_off`
   → `pool_off` 逐段首尾相接；`pool_off + pool_size ≤ 文件大小`（64 位算术，防回绕）；
5. 每条 SylEntry：`flags == 0`、`first_entry` 等于累加器、`name_off` 可解析且过 `is_dat_reading`；
   末了累加器必须等于 `entry_count`；
6. 每条 LexCode：`flags == 0`、`first_entry` 等于累加器、`entry_count ≥ 1`、`key_off` 可解析且过 `is_dat_reading`；
7. 每条 LexEntry：`flags` 除 bit1 外全 0、`alt_off == 0`、`rank != 0`、`syl_count ≥ 2`、
   `text_off` 可解析且非空；`reading_off != 0` 时可解析、空格分段数 == `syl_count`；
   词组键段末了累加器必须等于 `lex_entry_count`；
8. `lex_entry_count == 0 ⇒ lex_code_count == 0`；反之 `lex_code_count == 0 ⇒ lex_entry_count == 0`
   （有词无键 = 词永远取不到，是有 bug 的表）；
9. 每条 CharEntry：`syl_id < code_count`、`rank != 0`、`(flags & ~0x2) == 0`、
   `reading_off` 可解析且过 `is_dat_reading`、`alt_off != 0` 时逐个次读音过 `is_dat_reading`；
10. 任一失败 → `clear()` + 返回 0，**不部分加载**（先全量校验，再动 `entries_`）。

> 实现红线（易踩）：偏移链必须用 **64 位**算术。32 位下 `entry_count × 24` 在 44 万条量级就会回绕，
> 而回绕后的偏移看起来「落在文件内」，校验会通过。

---

## 6. v1：不再支持（接线与后果）

- `version != 2` 一律拒载，包括 v1。
- 运行时的后果是**降级而不是失败**：`pinyin/host/dat_source_win.cpp` 读表失败即回退到
  `pinyin/src/scheme.cpp` 的 `kBuiltin[]`（编译进二进制的数组，不经过 dat）。表现为候选明显变少、
  多音字消失（`shui` 打不出「谁」）。
- **发布纪律**：`.ime` 与 `rospinyin.dat` 必须同源同版发布。旧 `.ime`（v1 读法）配新表会被拒、
  回落内置表；新 `.ime` 配旧表同理。安装器 `/step1` 是两文件同拷，这条约束天然满足，
  但**手工替换其中一个文件就会破**，发布说明须写明。
- 仓库里**没有**受版本控制的 `.dat` 资产（`m0/share/` 是本机产物）：表由
  `tools/mkdat/build_dat.sh` 从钉死 sha256 的原始输入重建，指纹写在脚本里。
- **本机刷新用 `build_dat.sh --install`**（默认目标 `m0/share/`）。别手工 `cp`：副本分叉过一次——
  v2 落地时 `m0/share/` 仍留着 v1 表，正好落进上面那条「新 `.ime` 配旧表」的坑里。
  `--install` 只在指纹闸通过后拷贝，所以错表进不了暂存目录。

---

## 7. 体积与性能

| | 旧（v1） | 现在（v2） |
|---|---|---|
| 文件 | 902,776 B | **1,080,224 B**（+177 KB，+19.6%） |
| Header | 52 B | 80 B |
| 每条字 | 20 B | 24 B |
| 44,355 字 + 418 音节 + 8,936 B 池 | — | 80 + 1,064,520 + 6,688 + 8,936 |

仍远低于 2 MB 目标；定长记录、无指针、mmap 友好。词组表为 0 条时只占 0 字节
（两张表都为空，`pool_off` 直接接在音节表后面）。
10 万词的词组表量级估算见 §11.4。

---

## 8. 消费方改动清单（本次全部落地）

| # | 落点 | 改动 | 状态 |
|---|---|---|---|
| 1 | `framework/include/imfw/dat_format.hpp` | `kVersion = 2`；`Header` 80B（含词组两表）；`CharEntry` 24B + `flags`；`SylEntry` 槽名统一为 `first_entry`/`entry_count`/`flags`；新增 `LexEntry`/`LexCode`；删掉 v1 结构与 `HeaderV2`/`CharEntryV2`/`kMagicLexicon`；static_assert 钉尺寸 + 两张表同宽 | ☑ |
| 2 | `framework/src/code_table.cpp` | v2-only 加载器：尺寸自检、四段偏移链、段 tiling、`rank != 0`、flags 掩码、词组表结构校验 | ☑ |
| 3 | `tools/mkdat/mkdat.cpp` | 产 80B 头 / 24B 条目；音节段改前缀和；词组表计数 0 | ☑ |
| 3a | `tools/mkdat/export_cmap.py`、`data/droid_cmap.txt` | 从 ReactOS 自带字体导出 29,622 码点；入库、可复现 | ☑ |
| 3b | `tools/mkdat/mkdat.cpp`（分级） | 新增 `--cmap`；按码点集写 `flags` bit1；`--stats` 报 tier 分布 | ☑ |
| 4 | `tests/code_table_test.cpp` | blob 构造器改 80B/24B；新增 v1 拒载、版本号、尺寸不符、rank 0、保留位、tiling、词组表（合法/无键/跳词/单音节）用例 | ☑ |
| 5 | `tests/check_mkdat.py`、`tools/coverage_report.py` | 新 struct 与偏移链；tier 统计行 + 中/嗯 分级断言 | ☑ |
| 6 | `tools/mkdat/build_dat.sh` | cmap sha256 钉死；产物指纹更新为分级表 `8f8cdb33…` | ☑ |
| 7 | M4（词组索引/查询 API） | **不在本次**：加载器只校验结构，不建索引 | ☐ |

---

## 9. 本规范的效力

- 本文与 `dat_format.hpp` 的 static_assert 是拼音侧的两个同位锚点：改布局必须同时改两处。
- `flags` 位只许增补、不许重排；bit1 的语义（保底可绘 = 0）是拼音关闭态过滤与形码 tier 的
  同族锚点，任一侧改动都算破坏同族公约。
- 词条不加宽：`entry_size` 只有 24 一个值，字与词共用宽度。要加第 7 槽必须新版本，
  不允许「两张表不同宽」。

---

## 10. 裁决记录（2026-11-20，全部关闭）

| # | 议题 | 裁决 | 落地 |
|---|---|---|---|
| 1 | 共享校验是否识别两个 magic | **否**。按调用方传入的 magic 做白名单 | §5 红线 1；统一文本 §5 第 1 条已改 |
| 2 | `rank` 的合法性 | **`≥ 1` 或哨兵**；`rank == 0` 非法 | §5 红线 2；加载器已实现并被用例钉住 |
| 3 | 槽 4 归属 | **拼音 `alt_off`，不校验为 0**；形码「保留须 0」只在形码路径 | §5 红线 3 |
| 4 | 拼音的形码专用 header 字段 | **严格 0**：拼音 dat 不自述版本与来源，溯源靠 `NOTICE.txt` + 构建日志 + 发行包 sha256 | §2.1；加载器第 2 条 |
| 5 | 词组承载 | **同一个文件**（修订 B 改判）：Header 加两套表，Entry 不加宽，取消 `ROSPYLEX` | §1.2、§2.3 |
| 6 | v1 兼容 | **不兼容**（修订 B 改判）：v1 条目没有 tier 位，降级读等于骗用户 | §1.1、§6 |
| 7 | 统一文本公式笔误 | 修订 A 定为 `64 + entry_count × 24`；本次 header 变 80B 后为 **`80 + entry_count × 24`** | 统一文本 §2.1 |

---

## 11. 词组表的使用约定（M4 待办）

### 11.1 键与读音

键 = 词读音去掉空格（`"ni3 hao3"` → `"ni3hao3"`）。构建期必须保证「键 == 读音去空格」；
运行期不重算（`O(n·len)` 没有收益），只在 `LexEntry.reading_off == 0` 时按「键去声调」回显。

### 11.2 查找

按音节序列查词 = 在键池上二分段。前缀输入（`ni` 还没打 `hao`）由引擎做键池前缀扫描，
格式层不另建索引。

### 11.3 tier 聚合

词的 `tier` 由构建期算：词内任一字 bit1 = 1 ⇒ 词 bit1 = 1。运行时不再逐字回查，
这样「关闭态零方框」对词组同样成立。

### 11.4 体积量级

10 万词、平均 2.2 字、约 5 万个键：词条 2.4 MB + 键 0.8 MB + 池（文本 ≈0.8 MB + 键 ≈0.5 MB）
≈ **4.5 MB**。首版若只收 3 万词约 1.4 MB。单文件意味着**改词表就要重发整表**——
这是接受「一个文件」时买下的代价；若将来词表迭代频率远高于字表，再评估拆分（须新版本）。

---

## 12. 兼容性审计（2026-11-20 修订 B，按代码事实核过）

| # | 场景 | 代码事实 | 结论 |
|---|---|---|---|
| 1 | 旧 v1 表被新 IME 读取 | `version != 2` → 返回 0；`dat_source_win.cpp` 回退 `kBuiltin[]` | **明确拒载 + 降级回退**，不半读、不崩溃 |
| 2 | 新 v2 表被旧 IME（v1 读法）读取 | 旧代码判 `version != 1` → 返回 0 | 同上（旧 IME 也回退）。**发布必须同源** |
| 3 | 内置表路径 | `kBuiltin[]` 是 C++ 数组，不经 dat | 完全不受格式变更影响 |
| 4 | `shape.dat` 被当拼音表读 | `memcmp(magic)`；用例按 `kMagicShape` 名钉住 | 已封 |
| 5 | 半升级文件（版本对、宽度错） | `entry_size`/`code_size` 自检先于记录遍历 | 已封（2 条用例） |
| 6 | 段区间错位 | 前缀和 tiling，加载器一个累加器校验 | 已封（单字与词组各 1 条用例） |
| 7 | 「未知版本」用例的语义漂移 | 原 `kVersion + 1` 恰是下一个合法版本 | 改为 `0x7F7F`，永不复用 |
| 8 | `mkdat` 与加载器口径 | 同一 `dat::Header` 结构体、同一套 static_assert | 结构上不可能不一致 |
| 9 | 端到端可复现 | `build_dat.sh` 全流程（原始输入 + cmap 均 sha256 钉死）产出 `8f8cdb33…` | 与手工产物逐字节一致 |
| 10 | 数据未变 | 44,355 字 / 418 音节 / 118 多音 / 首屏覆盖 96.8% | 与 M2 基线一致：容器变了、载荷没变 |

**残留（不是缺陷，是时序）**：

1. ~~tier 全 0~~ **已关闭（2026-11-20）**：cmap 分级数据已入库并写入 dat，
   tier0=16,500 / tier1=27,855；运行时按 tier 过滤由 framework 的
   `renderable_candidates` + `[快]/[全]` 承担（FR-2/FR-3/FR-5）。
2. **词组表无读取方**：加载器只校验结构；索引与查询属 M4。
3. **形码侧要跟**：`shape.dat` 的字节图与尺寸表在统一文本里已同步为 80B，但
   `XINGMA-PLAN.md` §4.1 与 `FRAMEWORK-REQ.md` 需形码侧按统一文本 §7 自检。
