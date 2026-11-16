# ⚠ 存档文件：RosXingMa 实施计划 v2.3（草案，不再维护）

> **本文件不得作为实施依据。**
>
> - 现行依据：[`../XINGMA-PLAN.md`](../XINGMA-PLAN.md)（正式版 1.0）
> - 保留原因：正式版删除了版本演进记录、术语演进表与「接口需求 F1–F5」清单，
>   并按 ros-pinyin 框架层的实际交付重写了复用边界（改为「框架能力缺口 G1–G4」）。
> - 查证早期取舍（三槽位模型的废弃、命名迁移、目录搬迁）时，以本文件为准。

---

# RosXingMa 实施计划：复用 RosPinyin 引擎的统一形码输入法（不限数量码表 + 独立码表配置工具）

- 状态：**已存档**（原草案 v2.3，替代 v1「三槽位」方案、v2 命名与 v2.2 平铺布局）
- 基线：本仓库 M2 工作树（`rospinyin-engine` 静态库 + `rospinyin.ime` + `rospinyin.dat` v1）
- 相关文档：`xingma/docs/TABLE-LICENSE-RESEARCH.md`（授权尽调）、`xingma/docs/TABLE-IMPORT-SPEC.md`（导入格式规范）、`xingma/docs/DATA-PACK-SPEC.md`（数据包规范）、`NOTICE.txt`
- v2 变更摘要：① 取消「86/98/新世纪三槽位」硬编码，改为**不限数量的统一形码表**；② 新增**独立码表配置工具**（导入/配置/切换/移除）作为一等交付物；③ 导入器支持 **CSV 与 fcitx 5 / ibus-table 等框架的文本码表**；④ 切换机制改为「**revision 自检 + 组合期延迟换表**，热切换为主、重启为兜底」；⑤ **86/98 以「可导入包 + 预配置清单」形式额外发布**（v2.1，见 4.4.1）。
- v2.3 变更摘要：⑨ **形码文件独立成目录 `xingma/`**，与拼音产品文件物理隔离；形码只消费 ros-pinyin 抽出的核心框架，**不修改其结构、构建文件与说明文档**，所需接口以「需求清单」形式提出（见 3.2、第 6 节）。
- v2.2 变更摘要：⑥ **全部构件改名**——产品 `RosXingMa`、`rosxingma.ime`、`rosxingma-config.exe`、`rosxingma-table.exe`、`HKCU\SOFTWARE\RosXingMa`、`%APPDATA%\RosXingMa`、`xingma/packs/shape86|shape98`，代码、文档、注册表、界面均不再出现旧名（见 1.1）；⑦ **配置工具为独立 exe，与 IME 分开放置**——IME 进 `system32`，工具进程序文件夹（见 4.10）；⑧ **Rime 只给 PC 端脚本**、**fcitx 4 不支持**（见 4.4 与决策点）。

---

## 0. 目标与非目标

**目标**

1. 在不破坏现有拼音 IME 的前提下，新增 `rosxingma.ime`（显示名「RosXingMa 形码输入法」），与 `rospinyin.ime` 共用 ros-pinyin 抽出的核心框架层源码（`framework/`，重构中）。
2. **引擎与具体码表解耦**：字母表、最大码长、通配键全部由码表数据驱动，同一引擎可跑 86/98/新世纪 三套表，也可跑郑码、仓颉、二笔等任意「拉丁字母 → 汉字」形码表。
3. **码表数量不设上限**：用户可导入任意多张表，随时增删、重命名、排序、切换；不存在「槽位」概念，也不需要为「新世纪」预留占位。
4. **提供独立的码表配置工具** `rosxingma-config.exe`（GUI）与 `rosxingma-table.exe`（CLI），负责导入、预览、校验、导出、移除、切换。
5. **导入兼容主流框架**：设备端直接支持规范 CSV/TSV/TXT、fcitx 5 文本码表、ibus-table 源表；**Rime `dict.yaml` 只由 PC 端脚本转换**后导入；并能反导出为规范 CSV 与 fcitx/ibus 格式，避免数据锁定。
6. **额外发布 86/98 可选数据包**：以「可导入包 + 预配置清单（`pack.ini`）」形式单独下载，用户导入时无需手填名称/码长/字母表/许可，并强制展示来源与上游权利提示（见 4.4.1）。
7. 最大程度复用「编码无关骨架」：引擎契约、候选分页、dat 二进制布局与确定性构建、IME 的 16 个导出与上屏链路、UI 候选/组合窗、测试框架。
8. **命名统一为 `RosXingMa`**：所有二进制、注册表键、目录、文档、界面文案一致；旧名只作为上游仓库 slug 与商标陈述存在，不再出现在本项目构件中。

**非目标（本期不做）**

- 不改变 `rospinyin.ime` 的行为与数据格式；拼音继续用 `rospinyin.dat` v1。
- 不做整句输入、云词库、自动造词/自学习词频（沿用 PLAN 的 M5/M6 规划）。
- 不做拼音与形码混合输入（形码打不出时切回拼音输入法），不做 `z` 键拼音反查（可在 W6 之后评估）。
- **fcitx 4 永久不支持**（二进制 `.mb` 与 `[CodeTable]` 配置均不解析，也不做「文本 `.mb`」兼容）；`.scel`/`.qcel`、ibus `.db`、`.txt.bz2` 只在 PC 端脚本处理，不在 IME 进程内解析；**Rime `dict.yaml` 不在设备端解析**（见 4.4）。
- **主安装包不含任何码表**；86/98 仅以可选数据包单独发布（见 4.4.1、4.9），新世纪任何形式都不分发。

---

## 1. 复用边界

| 组件 | 决策 | 理由 |
|---|---|---|
| `Engine` / `Result` / `Action` / `Candidate` | **直接复用** | 输入是 Unicode 码点，输出是组合串+候选+上屏文本，不含任何拼音语义 |
| `CandidatePager`（9 候选、跨页选号） | **直接复用** | 纯逻辑，与编码方式无关 |
| dat 的「定长头+记录数组+字符串池、小端 32 位、字节级确定性」范式 | **结构复用** | 语义从「音节→同音字」换成「编码→候选字/词」 |
| `PinyinTable::load_dat` 的边界检查与 `build_index` 排序思路 | **复用实现范式** | 新写 `ShapeTable`，沿用校验严格性与 rank 升序规则 |
| IME 层 16 个导出、`ImeProcessKey`/`ImeToAsciiEx` 分流、`WM_IME_CHAR` 上屏 | **复用（共享源码）** | 只有「字母→码点」「选字键」「码长上限」需要由表参数化 |
| 框架层组合窗、候选窗、字体兜底、避屏布局 | **直接复用** | 组合窗显示编码串，候选窗显示汉字，参数化后通用 |
| `pinyin.cpp` 归一化、`segment_syllables`/`can_extend` | **弃用（形码侧）** | 形码无带调字母、无音节切分；`v` 是普通字根键 |
| `PinyinSession` | **替换为 `ShapeSession`** | 编码核心必须重写 |
| `PinyinTable` | **替换为 `ShapeTable`** | 表结构语义不同，但加载/校验/排序范式相同 |

**一句话方案**：把 `rospinyin-engine` 当成「编码无关的候选引擎骨架」，新增 `ShapeSession` + `ShapeTable` + `shape.dat` v2；表集合以「id + 元数据」形式存在注册表与 `%APPDATA%\RosXingMa\tables\`，数量不限；全新写一个独立配置工具完成导入/切换；切换靠「注册表 revision + 焦点/组合起点自检 + 组合期延迟换表」实现热生效，极端场景回退到重启。

### 1.1 命名清单（现行唯一口径）

| 层面 | 名称 | 说明 |
|---|---|---|
| 产品 | `RosXingMa`（显示名「RosXingMa 形码输入法」） | 中英文界面与实际文案统一用此名 |
| IME 模块 | `rosxingma.ime` | 与 `rospinyin.ime` 并列，共享框架层源码 |
| 配置工具（GUI） | `rosxingma-config.exe` | **独立 exe**，装到程序文件夹（见 4.10） |
| 命令行工具 | `rosxingma-table.exe` | console 子系统，供脚本 / CI / 无人值守 |
| 用户设置 | `HKCU\SOFTWARE\RosXingMa` | 无历史用户，不做旧键迁移 |
| 机器级安装信息 | `HKLM\SOFTWARE\RosXingMa\Install` | 安装目录、配置工具路径、HKL |
| 用户数据 | `%APPDATA%\RosXingMa\{tables,reports,packs}` | 免管理员权限 |
| 程序文件 | `%ProgramFiles%\RosXingMa\` | 配置工具、CLI、文档、许可证 |
| 引擎构件 | `ShapeSession` / `ShapeTable` / `shape_dat_format.hpp` / `shape.dat`（magic `ROSSHDAT`） | 与具体编码方案解耦 |
| 仓库目录 | `xingma/`（形码产品线全部文件）；兄弟目录为 `framework/`（共用框架）与 `pinyin/`（拼音产品） | 见 3.2；形码不改 `framework/`、`pinyin/` 与根目录构建脚本 |
| 数据包目录 | `xingma/packs/shape86`、`xingma/packs/shape98`；`PackId` = `shape86-jidian`、`shape98-tables` | 仓库只存模板与许可，数据构建期生成 |
| 发布物 | `RosXingMa-Pack-Shape86-v1.0.0.zip` | 单独下载，不进主安装包 |

### 1.2 禁用词与事实引用例外

- 代码、注册表、路径、文件名、界面文案、发布说明：**不出现**旧名（`wubi` / `五笔`，大小写皆算）。
- 例外仅四类，且必须**原样保留**（改了就是错误的引用）：上游仓库 slug 与 URL（`KyleBing/rime-wubi86-jidian`、`yanhuacuo/98wubi-tables` 等）、上游数据文件名（如 `Wubi98-Unicode15.1.txt`）、上游产品名（`极点五笔`）、商标名（`五笔字型`）。
- CI 落地：`tools/check_m1.py` 增加禁用词扫描——命中旧名且不落在上述白名单（URL / slug / 上游文件名 / 引号包裹的专名）即失败。

### 1.3 术语变更（v1/v2 → 现行）

| v1/v2 旧术语 | 现行术语 | 说明 |
|---|---|---|
| 槽位 `*-86`/`*-98`/`*-21` | 表 `id`（`[a-z0-9][a-z0-9_-]{0,31}`） | 数量不限，无固定槽位 |
| `ActiveSlot` | `ActiveTable` | 存表 id |
| 独立导入小工具 | `rosxingma-table.exe`（CLI）+ `rosxingma-config.exe`（GUI） | 导入能力并入配置工具，不再单独发小工具 |
| `mkdat-shape` | `mkdat-shape` | 构建可选数据包时复用同一 `dat_writer` |

---

## 2. 现状依据（关键事实，均已核验源码）

> **路径口径**：下表的行号与路径取自 ros-pinyin **重构前**的树（`engine/` `ime/` `ui/`）。
> 该仓库正在重构为 `framework/`（共用框架）+ `pinyin/`（拼音产品），构建文件与说明尚未定稿；
> 文件位置对应关系为 `engine/include/rospinyin/*` → `framework/include/imfw/*`（`table.hpp` → `code_table.hpp`）、
> `engine/src/*` → `framework/src/*`、`ime/*` → `framework/host/*`、`ui/*` → `framework/ui/*`、
> 拼音专有部分 → `pinyin/{include,src}/`。**事实不变，路径与行号需在其定稿后复核一次**；本计划不代改该重构。

| 事实 | 文件 |
|---|---|
| `Engine` 契约：`feed(char32_t)` / `backspace` / `cancel` / `on_select`，`Result{action, composition, candidates, commit_text}` | `engine/include/rospinyin/engine.hpp:19-50` |
| `Candidate` 为 `u32string` + `int score`；score = `1000000 - rank` | `engine.hpp:25-28`、`engine/src/pinyin_session.cpp:96-104` |
| dat v1：`ROSPYDAT` / version 1，`[Header 52B][CharEntry 20B][SylEntry 16B][pool]`，字段全为 32 位小端 | `engine/include/rospinyin/dat_format.hpp:29-67` |
| `PinyinTable` 公开 API：`load_dat` / `add` / `clear` / `build_index` / `lookup` / `is_syllable` / `is_prefix` | `engine/include/rospinyin/table.hpp:29-57` |
| 运行时表是**进程级单例**，每个 HIMC 的 `PinyinSession` 持有 `const PinyinTable&` 引用 | `ime/session_store.hpp:17-27`、`ime/session_store.cpp:19-61` |
| 数据加载硬编码 `GetSystemDirectoryW() + "\rospinyin.dat"`，失败回退 110 条内置表 | `ime/dat_loader.cpp:151-186` |
| `ImeConfigure` 直接 `return FALSE`，无任何设置存储 | `ime/imm_entry.cpp:241-244` |
| `ImeSelect` 已存在且是唯一「激活/失活」钩子，可挂自检 | `ime/imm_entry.cpp:256-266` |
| UI server 窗口**只有类注册**（`CS_IME`），窗口由 IMM32 按 `ImeInquire` 返回的类名创建；代码未自行建窗 | `ime/ui_server.cpp:12-48` |
| 注册表仅安装器写 `HKCU\SOFTWARE\RosPinyin\Install\HKL`；无用户偏好设置 | `tools/install-rospinyin/install.cpp`、`rospinyin.h:20` |
| 上屏走 `WM_IME_CHAR` TRANSMSG，不用 `GCS_RESULTSTR` | `ime/imm_entry.cpp:120` |
| 线程同步刻意用 `CRITICAL_SECTION` 而非 `std::mutex`（避免 libwinpthread 进 PE） | `ime/session_store.cpp:1-6` |
| 测试为手写 `check()` + 每文件 `main()`；`check_mkdat.py` 做字节级确定性校验 | `tests/CMakeLists.txt`、`tests/check_mkdat.py` |

> **关键结论（决定切换方案）**：IMM32 下 IME DLL 是**按进程加载**的，`Store` 是进程内静态单例，不存在跨进程共享内存。因此「切换码表」不是一次原子操作能覆盖全局的，必须设计成「每个进程各自在确定的时机发现 revision 变化并换表」。

---

## 3. 产品形态与架构决策

### 3.1 形态选择

| 选项 | 做法 | 结论 |
|---|---|---|
| A. 单 IME 内切拼音/形码 | 一个 `rospinyin.ime` 承载两种引擎 | ❌ 破坏现有注册表语义，会话内混两种引擎，风险高 |
| **B. 同仓库双 IME（采用）** | 编译期开关生成 `rosxingma.ime`，与拼音共享框架层源码，拼音零改动 | ✅ 复用最大化、互不干扰 |
| C. fork 一份宿主层改形码 | 独立复制 | ❌ ~500 行 IMM32 plumbing 重复，必然漂移 |

### 3.2 目录边界与复用方式

两条约束同时成立：

1. **形码的一切文件都在 `xingma/` 下**，与拼音产品文件物理分开，便于单独评审、单独发布、整目录删除。
2. **复用 ros-pinyin 抽出的核心框架，但不修改它的结构、构建文件与说明文档**——这套重构由 ros-pinyin 侧定稿，形码侧只消费其接口。

```
ros-pinyin/
  framework/   ← 共用核心框架（include/imfw、src、host、ui）   ← ros-pinyin 重构中，以其定稿为准
  pinyin/      ← 拼音产品（include/rospinyin、src）
  tools/ tests/ m0/ m2/ vendor/ ...        ← 拼音既有资产
  CMakeLists.txt  NOTICE.txt  LICENSE  README.md  ROADMAP.md  ← 拼音维护，形码不改
  xingma/      ← 本产品线：只有形码需要的东西
    XINGMA-PLAN.md   README.md
    docs/{TABLE-LICENSE-RESEARCH,TABLE-IMPORT-SPEC,DATA-PACK-SPEC}.md
    packs/{README.md,shape86/,shape98/}          ← 数据包模板与许可，无码表数据
    tools/mkdat-shape/make_pack.py               ← 已实现
    include/roxingma/{shape_table,shape_session,shape_dat_format}.hpp   (W1)
    src/{shape_table,shape_session}.cpp                                 (W1)
    host/{shape_dat_loader.*,shape_demo_table.hpp,engine_factory_shape.cpp}  (W1–W2)
    tools/{table-io,table-config,table-convert}                          (W1–W4)
    tests/  CMakeLists.txt                                              (W1–W3)
```

**依赖方向**：`xingma/` → `framework/`，单向。形码不往 `framework/`、`pinyin/` 里加文件，也不改它们的构建脚本。

**复用点与需要框架方提供的缝**（形码侧只提需求，由 ros-pinyin 定稿时实现）：

| 角色 | 框架层现状（重构中） | 形码侧需要什么 |
|---|---|---|
| 引擎契约 | `framework/include/imfw/engine.hpp`（`imfw::Engine`） | `ShapeSession` 实现同一接口，形码直接 include 该头文件 |
| 会话状态与存储 | `framework/host/session_store.*` 目前持有 `PinyinSession` 与 `PinyinTable` | 一个**引擎/表构造注入点**（工厂函数或注册），使 `xingma` 能在不改 `session_store` 的前提下换实现 |
| 码表容器 | `framework/include/imfw/code_table.hpp` | 形码用独立 `ShapeTable`（多表、按 id、动态路径），不复用单表加载 |
| 数据文件定位 | `framework/host/dat_loader.*` 按 `GetSystemDirectoryW()` 找单个文件 | 形码从 `%APPDATA%\RosXingMa\tables\<id>.dat` 读取，需要定位策略可替换 |
| 候选/组合窗 | `framework/ui/*`，目前假定「拼音音节」分段 | `UiShow` 的分段参数通用化（字符区间），形码传整串或空 |
| IME 导出层 | `framework/host/imm_entry.cpp` | 按键分流需按「当前表的字母表/最大码长」判断，而不是写死 a–z/4 码 |

> 这些是**接口需求**，不是形码要提交的补丁。若框架方最终不提供某项缝（例如坚持 `session_store` 只服务拼音），降级方案是形码自建 `xingma/host/` 的会话存储与 `dat` 加载，只复用 `imfw::Engine` 契约与 `framework/ui` 的窗口类——复用面缩小，但不产生跨产品耦合。

## 4. 核心设计

### 4.1 ShapeSession：由表参数驱动的「最长精确前缀」会话

与拼音最大的不同是**规则参数化**，会话不再假设 4 码、a–y、`v`=ü：

| 参数 | 来源 | 取值示例 |
|---|---|---|
| `max_code_len` | `shape.dat` 头 | 形码 4、仓颉 5、郑码 4、二笔 4 |
| `alphabet` | `shape.dat` 池内字符串 | 形码 `abcdefghijklmnopqrstuvwxy`；仓颉 `abcdefghijklmnpqrstuvwxyz` |
| `wildcard` | 推导：若 `z` 不在 `alphabet` 中则 `z` 为通配键，否则无 | 形码 `z`；仓颉无 |
| 选字键 | 固定 `1`–`9`（数字不进字母表） | — |
| 上屏键 | 空格、数字、标点 | — |

行为规则：

1. 只接受 `alphabet` 内的字母，且 `composition_.size() < max_code_len`；超出或非法字符**不吞键**（交给应用）。
2. 每输入一码，取**存在精确候选的最长前缀**查表；例如 86 版 `q` 有简码 → 显示；续打 `r` 后 `qr` 无候选则回退显示 `q` 的候选。
3. 空格：有候选 → 上屏第 1 候选；无候选 → 按 `EmitRawOnNoMatch`（默认开）上屏原始编码串，避免用户输入丢失。
4. `1`–`9` → 经 `CandidatePager` 选字上屏；`Esc` → 清空；退格 → 删一码重算。
5. 通配键（若有）：W1 先按普通字母处理（无候选即不产生候选）；W4 实现运行时区间扫描（`kz` → 遍历 `k` 前缀的全部编码做后缀匹配），**不做构建期笛卡尔展开**。
6. 「唯一候选自动上屏」：`max_code_len` 已满且只有 1 个候选时按设置决定是否直接提交，默认关闭。

> 取舍：**所有编码规则（全码、简码、识别码、词码）在构建/导入期展开进 dat**，运行时只做前缀查表。好处是引擎不需要字根表、笔画表、末笔字型数据，DLL 保持极小；代价是 dat 略大（可接受）。

### 4.2 shape.dat v2：通用形码容器

沿用 v1 的「定长头 + 两个定长记录数组 + NUL 结尾字符串池」与确定性构建，仅改语义与 magic：

```
magic "ROSSHDAT" (8B), version = 2
Header (64B):
  magic[8], version, flags, entry_count, code_count,
  entry_table_off, code_table_off, pool_off, pool_size,
  name_off,          // 池内偏移：显示名 "86 版形码（极点）"，空串非法
  alphabet_off,      // 池内偏移：编码字母表 "abc...y"
  source_off,        // 池内偏移："sha256:<hex>;format:<csv|tsv|fcitx|ibus|pack>;name:<utf8>"，0=无（Rime 只在数据包 UpstreamFormat 出现）
  max_code_len,      // 1–8
  reserved[2]
Entry (20B): text_off, text_len, rank, entry_type, flags
  // text_off 指向池内 UTF-8 字符串（单字或词），text_len 为字节数
  // rank: 越小越靠前（导入期已由各来源权重归一化）
  // entry_type: 0=单字, 1=词；flags bit0 = 仅用于组词（fcitx `^` / ibus 特殊条目不参与输入）
Code  (16B): code_off, first_entry, entry_count, flags
  // code_off 指向池内编码串；Entry 按 (rank, text 码点) 升序且必须连续
  // flags: bit0 一级简码 / bit1 二级 / bit2 三级 / bit3 全码 / bit4 含识别码（导入时按码长与表内位置推断，仅作 UI 提示）
static_assert(Header == 64, Entry == 20, Code == 16)
```

- `load_dat()` 必须校验：魔数、版本、`max_code_len∈[1,8]`、`alphabet` 非空且全为 ASCII 字母、偏移不越界、`count×size` 不溢出、字符串池 NUL 终止、`Code` 严格升序且不重复、`first_entry+entry_count ≤ entry_count`。
- **不再有 `scheme` 枚举**：显示名是自由字符串，用户可以导入「我的86改」「公司98定制」而不被槽位名约束。
- `source_off` 记录来源与 SHA-256，用于「查看来源与许可」、去重与可复现。

### 4.3 码表存储模型：不限数量

**目录布局**

```
%APPDATA%\RosXingMa\
  tables\<id>.dat         ← 每张表一个 dat（1–3 MB）
  tables\<id>.source.txt  ← 可选：保留用户原始文件副本（默认关，导入时勾选）
  reports\<id>.log        ← 导入报告（行数/去重/跳过明细）
  cache\lru               ← 可选：最近使用记录（也可只放注册表）
```

- 表数量上限由「注册表子键数 + 磁盘」决定，不设人为上限；单表上限沿用导入器限制（200 万行 / 64 MB 源文件）。
- **id 规则**：`[a-z0-9][a-z0-9_-]{0,31}`。默认由「名称 slug + 内容 sha256 前 4 位」生成（如 `shape86-a1b2`），冲突自动加 `-2`；CLI 可用 `--id` 指定以便脚本化。
- 文件与注册表**成对存在**：注册表有条目但文件丢失 → 配置工具标红「缺失」，IME 启动时跳过并尝试下一个；文件存在但无注册表项 → 视为「未登记」，配置工具提供「扫描并登记」。

**注册表布局**

```
HKCU\SOFTWARE\RosXingMa\
  Settings\
    ActiveTable      REG_SZ    当前表 id；空串 = 无表（首次启动）
    Revision         REG_DWORD 每次表集合或当前选择变化 +1（seqlock 依据）
    SwitchMode       REG_DWORD 0=auto（默认，见 4.7）/ 1=仅下次加载
    AutoCommitUnique REG_DWORD 默认 0
    EmitRawOnNoMatch REG_DWORD 默认 1
    FirstRunShown    REG_DWORD 首启合规提示是否已读
  Tables\<id>\
    Name             REG_SZ    显示名
    Path             REG_SZ    dat 绝对路径（默认 %APPDATA%\RosXingMa\tables\<id>.dat）
    Sha256           REG_SZ    小写 hex
    EntryCount       REG_DWORD
    CodeCount        REG_DWORD
    MaxCodeLen       REG_DWORD
    Alphabet         REG_SZ
    SourceKind       REG_SZ    pack | import | demo
    SourceFormat     REG_SZ    csv | tsv | fcitx | ibus | rime | pack
    SourceName       REG_SZ    原始文件名
    License          REG_SZ    许可证标识（pack 来自 manifest；用户导入为自述）
    LicensePath      REG_SZ    pack 随附许可证文本的本地留存路径（见 4.4.1）
    PackId           REG_SZ    数据包标识（仅 SourceKind=pack）
    PackVersion      REG_SZ    数据包版本（仅 pack）
    SourceUrl        REG_SZ    上游来源 URL（仅 pack）
    UpstreamNotice   REG_SZ    上游权利瑕疵/使用限制提示（仅 pack，导入时展示并留存）
    OriginPath       REG_SZ    原始文件完整路径（用于溯源）
    ImportedAt       REG_SZ    ISO 8601
    Order            REG_DWORD 列表排序
```

- 不写 `HKCU\Keyboard Layout\Preload`、不设默认布局、不硬编码 HKL——M0 四条红线继续有效。
- 配置工具变更顺序固定：**先写 `Tables\<id>` 与文件，最后写 `ActiveTable` 并自增 `Revision`**，保证 IME 不会读到半成品。

### 4.4 导入管线与格式兼容

**统一管线**（导入即编译，运行时零解析）：

```
源文件（CSV/TSV/TXT/fcitx 5 文本/ibus 源表；Rime 先由 PC 端脚本转成 CSV）
   │ ① 格式判定（扩展名 + 内容签名，可被 --format 覆盖）
   │ ② 流式解析 → 中间记录 {code, text, weight, type}
   │ ③ 归一化：编码小写、权重→rank、去重、按 (code, rank, text) 排序
   │ ④ 校验：字母表/码长/控制字符/行数上限；失败即整体拒绝
   ▼
shape.dat v2（临时文件 → fsync → 原子 rename）
   │ ⑤ 写 Tables\<id> 元数据 → 写 ActiveTable → Revision++
   ▼
（可选）广播 WM_SETTINGCHANGE("Software\\RosXingMa")
```

**格式支持矩阵**（详细语法见 `xingma/docs/TABLE-IMPORT-SPEC.md`）

| 格式 | 识别 | 设备端（IME/配置工具内建） | PC 端脚本 | 备注 |
|---|---|---|---|---|
| 规范 CSV（RFC 4180） | `.csv` / 内容含 `,` 与表头 | ✅ W1 | — | 首选交换格式 |
| TSV / 空白分隔 TXT | `.tsv` / `.txt` | ✅ W1 | — | 极点、98 表常见 |
| fcitx 5 文本码表 | `.txt`，含 `[Table]`/`[Data]` 或 `[数据]` | ✅ W3 | — | 中文键名 `键码/码长` 与英文键名都接受 |
| ibus-table 源表 | `.txt` / `.ibus-table`，含 `BEGIN_DEFINITION` | ✅ W3 | — | 忽略 `CHAR_PROMPTS`（字根提示） |
| **Rime `*.dict.yaml`** | `.yaml` / `.dict.yaml` | ❌ **只识别、不解析**（返回 6 + 引导） | ✅ W4 `rime2csv.py` | `import_tables` 由脚本递归合并 |
| ibus `.db`（sqlite） | `SQLite format 3` | ❌ | ✅ W4 | 设备端不链 sqlite |
| **fcitx 4（`.mb` 二进制或文本）** | `.mb` 任意形态 / `[CodeTable]` | ❌ **永久不支持** | —— | 私有二进制，不做反解析 |
| `.txt.bz2` | `BZh` 头 | ❌ | ✅ W4 | 设备端无 bzip2 |

> 「只识别、不解析」是刻意的：直接报「格式未知」会让用户以为文件坏了，明确告诉他跑哪条命令才解决问题。

**权重归一化**：各来源语义一致（数值越大越靠前），统一 `rank = 65535 - min(weight, 65535)`；无权重字段时 `rank = 行号`（保序）。同一 `(code,text)` 多来源/多行重复 → 取最小 rank。

**容错**：

- 无表头时猜测列序；若「第一列不像编码、第二列像」则自动交换（兼容「字在前、码在后」）。
- 分隔符嗅探：逗号 / 制表符 / 连续空格。
- 编码嗅探：UTF-8（含 BOM）/ UTF-16LE/BE（BOM）/ GB18030（`MultiByteToWideChar(936)`），失败明确报错而不是乱码入库。
- 单行 ≤ 64 KB；总行 ≤ 200 万；解析后有效条目 = 0 → 拒绝。
- 非法行不静默丢弃：写 `reports\<id>.log`，配置工具展示前 20 条并可导出 `rejects.csv`。

### 4.4.1 可选数据包：可导入包 + 预配置

86（MIT 源）与 98（Unlicense 源）以**独立数据包**形式额外发布。数据包不是「安装程序」，而是**一个可被配置工具直接导入的目录**，自带预配置信息，用户双击导入即可，无需在界面里手填名称、码长、字母表。

**包形态**

```
RosXingMa-Pack-Shape86-v1.0.0/
  pack.ini          ← 预配置清单（UTF-8，INI 风格）
  table.csv         ← 码表数据（构建时从上游取，固定 commit + sha256）
  LICENSE           ← 上游许可证全文（MIT）
  NOTICE.md         ← 署名、来源、上游权利提示
  SHA256SUMS        ← 包内所有文件的哈希
```

- 分发：整体打成一个 `.zip` 供下载（人人可解压、可审查）；**工具不做 zip 解析**，导入时选择解压后的目录或其中的 `pack.ini`（选择器同时接受两者）。
- CLI：`rosxingma-table pack install <dir|pack.ini> [--dry-run] [--activate]`。
- 拖放：把整个目录拖到配置工具窗口即等同「选择该目录导入」。
- 为什么不用自定义单文件容器：目录形态可读、可 diff、许可证文本单独可见、无需引入解压/自定义二进制解析；代价只是「先解压」一步。

**预配置信息（`pack.ini` 完整字段）**

| 段 | 键 | 必填 | 作用 |
|---|---|---|---|
| `[Pack]` | `FormatVersion` | 是 | 清单版本，当前 `1`；未知版本拒绝 |
| | `PackId` / `PackVersion` | 是 | 包标识与版本，用于升级判定与去重 |
| | `DisplayName` / `Description` | 是 / 否 | 列表显示名与说明（用户可改名，但默认取此值） |
| | `Author` / `Homepage` / `BuildDate` | 否 | 署名与溯源 |
| `[License]` | `License` | 是 | SPDX 标识（`MIT` / `Unlicense`） |
| | `LicenseFile` / `SourceUrl` / `SourceCommit` | 是 / 是 / 否 | 许可证文本、上游地址、固定 commit |
| | `UpstreamNotice` | 否 | 上游权利提示（如「源自极点码表，权利完整性未获担保」），**必须原样展示** |
| | `Redistributable` / `Warranty` | 是 | `yes`/`no`、`none`（本项目不提供担保） |
| `[Table]` | `SourceFormat` / `DataFile` | 是 | `csv`/`tsv`/`fcitx`/`ibus`/`rime` + 相对路径 |
| | `DataSha256` | 是 | 数据文件哈希，不匹配即拒绝安装 |
| | `EntryCountExpect` | 否 | 条目数期望值，偏差 >1% 报警告（防截断） |
| | `MaxCodeLen` / `Alphabet` / `Wildcard` | 是 | 预配置参数；与实际数据冲突时**报错而不是静默覆盖** |
| | `Columns` | 否 | 字段映射（如 `text,code,weight`），覆盖自动嗅探 |
| `[Install]` | `DefaultOrder` / `ActivateIfNone` | 否 | 建议排序位；无当前表时是否自动选为当前 |
| | `ConsentRequired` | 是 | 固定 `yes`：安装前必须经用户确认 |
| | `RetainSource` | 否 | 是否在 `packs\<PackId>\` 留存数据副本（默认 `no`） |

**安装流程**

```
选择包目录 / pack.ini
  → ① 解析清单，校验必填项、FormatVersion、路径必须为包内相对路径（拒绝 .. 与绝对路径）
  → ② 校验 SHA256SUMS 与 DataSha256
  → ③ 按 Columns/SourceFormat 解析并按 MaxCodeLen/Alphabet 校验（冲突即失败）
  → ④ 同意屏：显示 DisplayName、Author、License、SourceUrl、UpstreamNotice 原文、
            「第三方数据、不作权利担保」声明 → 用户勾选同意
  → ⑤ 写 shape.dat（原子 rename）+ Tables\<id>（含 PackId/PackVersion/SourceUrl/UpstreamNotice）
        + 留存 LICENSE 到 %APPDATA%\RosXingMa\packs\<PackId>\LICENSE
  → ⑥ 若 ActivateIfNone 且当前无表，或用户勾选「安装后设为当前」→ 写 ActiveTable + Revision++
```

**升级 / 冲突 / 卸载**

- 同 `PackId` 且 `PackVersion` 更高 → 列表显示「可升级」，升级走同一流程并保留用户改过的显示名与 `Order`。
- 同 `PackId` 同版本 → 提示「已安装」；同内容不同 `PackId` → 提示可能重复，允许并存。
- 卸载：删 `tables\<id>.dat`、`Tables\<id>`、`packs\<PackId>\`；若为当前表，先要求改选或切到「无表」。
- `UpstreamNotice` 在「详情」页常驻可查，不因安装完成而消失。

**合规边界（与 4.9 一致）**

- 数据包**单独下载、单独授权单元**，不与主安装包合并；各自带 LICENSE 与 NOTICE。
- 主安装包与安装向导**不含**数据包，也不提供「一键获取」；配置工具不内嵌下载地址。
- 包内不得含字根键位图、助记口诀、王码名称/图标等非数据资产。
- 详细规范与两份包的实际清单见 `xingma/docs/DATA-PACK-SPEC.md`。

### 4.5 码表配置工具 `rosxingma-config.exe`

**定位**：**独立 `rosxingma-config.exe`**（无 MFC/Qt/第三方库，仅 user32/comctl32/shell32/advapi32），装到 `%ProgramFiles%\RosXingMa\`，**不进 system32、不与 `.ime` 同目录**（见 4.10）。`ImeConfigure` 与托盘菜单都从注册表取路径启动它，替换现有 `return FALSE`。运行期免管理员权限（只写 HKCU 与 `%APPDATA%`）。

**主界面布局**

```
┌ RosXingMa 码表管理 ─────────────────────────────────────────────┐
│ 当前生效：[ 86 版形码（极点）        ▼ ]   [设为当前]  [刷新]    │
├───────────────────────────────────────────────────────────────┤
│ 名称           │ 码长 │ 编码数 │ 条目数 │ 格式 │ 许可证 │ 状态 │
│ 86 版形码（极点）  │  4  │ 25,631 │ 89,272 │ fcitx│ MIT    │ 生效 │
│ 我的98定制     │  4  │ 28,104 │ 112,584│ csv  │ 自述   │      │
│ 仓颉五代       │  5  │ 41,220 │ 98,110 │ ibus │ 自述   │ 缺文件│
├───────────────────────────────────────────────────────────────┤
│ [导入码表…] [导出…] [重命名] [移除] [上移] [下移] [详情]        │
│ 提示：切换后新开窗口即生效；极少数程序需重启。                  │
└───────────────────────────────────────────────────────────────┘
```

**导入向导（两步）**

1. **选文件**：`GetOpenFileNameW`，过滤器含 `*.csv;*.tsv;*.txt;*.mb;*.ibus-table;*.yaml;*.dat`，可多选（批量导入）。
2. **预览与确认**：显示
   - 自动判定格式（可下拉改判）、编码（UTF-8/GB18030…）、分隔符；
   - 统计：总行 / 有效 / 去重 / 跳过 / 编码数 / 最大码长 / 字母表；
   - 前 20 条样例（`编码 → 候选`）；
   - 告警（如「检测到 `,` 在编码中」「有 3 行编码超长」）；
   - 名称输入框（默认取文件名/表内 `Name`）、许可证输入框（默认空，提示「请填写您持有该码表的授权依据」）；
   - 「另存原始文件副本」勾选。
   点「导入」后进度条 + 结果摘要（耗时、写入路径、是否设为当前）。

**关键交互**

- 冲突：内容 SHA-256 与已有表相同 → 提示「已存在同名/同内容表」，提供「覆盖 / 另存为」；`id` 冲突自动加后缀。
- 移除当前表：禁止直接删，先要求选择新的当前表，或切到「无表」状态。
- 缺文件/损坏：状态列标红，详情页给出修复建议（重新导入 / 指向新路径 / 移除登记）。
- 首启：无任何表时显示空状态引导 + 合规提示（「本输入法不附带码表，请导入您合法持有的码表」），只出现一次（`FirstRunShown`）。
- 详情页：名称、id、路径、SHA-256、格式、来源文件、导入时间、条目数、许可证、dat 头字段自检结果。

**实现要点**

- 列表用 `ListView`（`LVS_REPORT`）；若目标 ReactOS 构建缺 comctl32 版本，降级为 `ListBox` + 详情区（W2 早测）。
- 所有耗时操作（解析、写盘）在 UI 线程外或分段执行并 `PumpMessages`，避免「未响应」。
- 字符串一律宽字符 + `UTF-8` 落盘；文件名含中文需 `\\?\` 长路径兼容。
- 配置工具**不链接 engine**（只链接 `libtableio`），避免把会话逻辑带进 GUI。

### 4.6 命令行工具 `rosxingma-table.exe`

GUI 程序做控制台重定向体验差，因此与 GUI 共享 `libtableio` 另出一个 console 子系统小工具，供脚本、CI、无人值守部署使用：

```
rosxingma-table list                          # 表格化列出所有表，* 标当前
rosxingma-table import <file> [--name N] [--id ID] [--format auto|csv|tsv|fcitx|ibus]
                            [--dry-run] [--force] [--activate] [--keep-source]
                            [--report <path>] [--rejects <path>]
rosxingma-table export <id> --out <file.csv> [--format csv|fcitx|ibus]
rosxingma-table activate <id> | --none
rosxingma-table rename <id> <new-name>
rosxingma-table remove <id> [--force]
rosxingma-table reorder <id> <order>
rosxingma-table verify <id>                 # 校验 dat 与元数据一致性
rosxingma-table diagnose                    # 打印数据目录、注册表快照、revision
rosxingma-table scan                        # 扫描 tables 目录，登记未入册的 dat
```

退出码：`0` 成功 / `1` 用法错误 / `2` 解析失败 / `3` 校验失败（无有效条目）/ `4` IO 失败 / `5` 已存在（需 `--force`）/ `6` 不支持的格式 / `7` 无权限或目录不可写。`--dry-run` 与 `import` 共享同一解析路径，保证预览与实际导入结果一致。

### 4.7 切换：revision 自检 + 组合期延迟换表（热切换为主，重启兜底）

IMM32 的现实：每个宿主进程各自加载一份 IME DLL，各自持有 `Store`。跨进程无法原子换表，因此采用**「版本号 + 时机自检」**而不是「全局广播即生效」。

1. **revision 协议（seqlock 读法）**
   - 配置工具写 `ActiveTable` 后 `Revision++`（写入顺序恒定：表元数据 → ActiveTable → Revision）。
   - IME 读：`R1 = Revision` → 读 `ActiveTable` → `R2 = Revision`；`R1 == R2` 才采用，否则重试一次，仍不一致就本次不换。
2. **自检时机（只有这三处，均不在按键热路径上）**
   - `ImeSelect(hIMC, TRUE)`：窗口获得焦点/输入法被激活时（框架宿主层，重构前为 `ime/imm_entry.cpp:256`）。
   - **组合起点**：`composing == false` 且收到首个字母键时，做一次轻量比对（内存中的 `cached_revision_` vs 注册表）。这是覆盖「应用长期持有 HIMC、不重新 ImeSelect」场景的关键。
   - `WM_SETTINGCHANGE`：若 UI server 窗口能收到广播，则立即触发；**仅作加速，不作依赖**（IMM32 创建的窗口是否为顶层窗口在 ReactOS 上未验证）。
3. **换表流程（在 `CRITICAL_SECTION` 内）**
   1. 若 `composing == true` → **不立刻换**，只记录 `pending_revision_`，等本次组合结束（上屏/取消）后再换。避免用户正在打的码被新表解释成别的字。
   2. 解析目标 id → `Path`/`Sha256`；id 为空 → `current_ = nullptr`（无表态）。
   3. 缓存命中直接用；未命中则加载 dat（校验失败 → 标记该表损坏，尝试回退到「上一张可用表」并向用户提示，而不是静默换到某张表）。
   4. 原子替换 `current_` / `active_id_` / `cached_revision_`；旧表由 `shared_ptr` 保活至最后一个会话释放。
4. **缓存策略**：`std::map<id, shared_ptr<const ShapeTable>>` + LRU 上限 2 张（当前表常驻、上一张保留以便来回切）。超限时仅淘汰 `use_count()==1` 的项；淘汰后再切回会重新读盘（1–3 MB，毫秒级）。
5. **生效预期（写进 UI 文案，不夸大）**
   - 同一窗口、组合已结束：下一次按键即生效（自检发生在组合起点）。
   - 其它已打开的程序：切回该窗口输入时生效（`ImeSelect`）。
   - 极少数不响应消息、且长期不重新激活输入法的程序：需要重启该程序；`SwitchMode=1` 可强制「只在下一次加载生效」，行为更可预测。
6. **降级路径**：若 `ImeSelect` 自检在 ReactOS 上被证明不可靠（M2 早测项），则退回 v1 的保守策略——设置页显示「重启后生效」，`SwitchMode` 默认改为 1。这就是「热切换不可行则重启生效」的开关。

### 4.8 与 IME 的集成点

- **按键分流（框架需求 F4）**：需要按「当前表的字母表 + `max_code_len`」判断是否吃键，而不是写死 a–z/4 码。若框架不提供该参数化，形码侧在会话层拦截：不属于当前表字母表的字符一律透传（`Action::Passthrough`），已达码长的键不再追加。
- **`ImeConfigure`（框架需求 F5 的轻量替代）**：形码侧只需要框架把该回调交给一个可配置的入口。做法是读 `HKLM\SOFTWARE\RosXingMa\Install\ConfigExe` → `ShellExecuteW`，**不写死绝对路径**（程序文件夹随安装盘符变化），读不到或启动失败则弹 `MessageBox` 指路。
- 组合窗：显示编码串 + 剩余可用码长提示（可选）；候选窗编号 1–9、`-`/`+` 翻页沿用。
- 无表态：组合窗显示「未安装码表」，按键**不吞**（让字符直接进应用），不崩溃。
- 状态显示：托盘菜单第一项显示当前表名；表多于 5 张时只列前 5 + 「更多…（打开码表管理）」。

### 4.9 合规：双轨分发（结论未变，工具边界更明确）

授权尽调见 `xingma/docs/TABLE-LICENSE-RESEARCH.md`，要点：

- 形码编码方案相关发明专利均已届满；但**字根表、字码对照表受著作权保护**，专利到期不影响著作权（唐懋宽案）。
- 可用许可：86 版 `KyleBing/rime-wubi86-jidian`（MIT，2026-10-29 起，源自极点表，权利链有残余风险）；98 版 `yanhuacuo/98wubi-tables`（Unlicense，作者本人 2024-01-20 加入）；**新世纪版无可用的宽松授权**。
- 「五笔字型」在第 9/42 类未见有效注册，但有在审申请；产品名与界面避让。

| 轨道 | 内容 | 约束 |
|---|---|---|
| **A. 默认（零数据风险）** | 只含 MIT 代码、格式规范、配置工具；**不含任何码表**；首启显示合规提示 | 用户导入自有码表 |
| **B. 可选数据包** | 86、98 各自单独下载、单独授权单元、单独署名 | 明示「第三方数据、不作权利担保」、可整体替换 |

**永不随包**：新世纪码表、字根键位图、助记口诀、王码名称/图标/安装包资源、指向未授权码表的直链。

**产品行为红线**：不自动下载、不在安装向导「一键获取」、不做「检测到无码表正在下载」；导入器保持工具中立（不内置、不推荐、不提供下载入口）；**仓库内测试 fixture 必须全部自造**（绝不放真实码表片段）。

### 4.10 安装布局：IME 与工具分开放置

IMM32 只在**系统目录**里按文件名找 `.ime`（`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<KLID>\` 的 `IME file`），所以 `rosxingma.ime` 只能装进 `system32`；**其余组件一律不进 system32**，配置工具与 CLI 独立放在程序文件夹。

| 组件 | 位置 | 写入需管理员 | 说明 |
|---|---|---|---|
| `rosxingma.ime` | `%SystemRoot%\system32\rosxingma.ime` | 是（仅安装/卸载时） | 受 IMM32 加载机制所限 |
| `rosxingma-config.exe` | `%ProgramFiles%\RosXingMa\` | 是（安装） / 否（运行） | **独立 exe，与 IME 分开** |
| `rosxingma-table.exe` | `%ProgramFiles%\RosXingMa\` | 同上 | CLI，供脚本 / CI |
| `LICENSE.txt` / `NOTICE.txt` / `README.txt` | `%ProgramFiles%\RosXingMa\` | 同上 | 许可与说明随程序分发 |
| `docs\*.md` | `%ProgramFiles%\RosXingMa\docs\` | 同上 | 导入规范、数据包说明、授权尽调 |
| 用户数据 | `%APPDATA%\RosXingMa\{tables,reports,packs}` | 否 | 安装器只建空目录 |
| 开始菜单 | 「所有用户 → 程序 → RosXingMa」下两个快捷方式（码表管理 / 命令行帮助） | 是 | ReactOS 为 `%ALLUSERSPROFILE%\Start Menu\Programs\RosXingMa` |

落地要点：

1. **路径定位靠注册表，不靠约定**：安装器写 `HKLM\SOFTWARE\RosXingMa\Install`（`InstallDir`、`ConfigExe`、`TableExe`、`HKL`）。`ImeConfigure`、托盘菜单、卸载器都从这里读，用户装到 `D:\Apps\RosXingMa` 也不出错。
2. **不写 `PATH`、不注册 COM、不做文件关联**；除 `.ime` 外不向 `system32` 写任何文件（`check_m1.py` 增加静态检查项）。
3. **卸载**：删 `system32\rosxingma.ime`、整个 `%ProgramFiles%\RosXingMa`、`HKLM\SOFTWARE\RosXingMa`、`HKCU\SOFTWARE\RosXingMa`、反注册 HKL；`%APPDATA%\RosXingMa` 默认**保留**（卸载页提供「同时删除我的码表」勾选，默认不勾）。
4. **便携试用（可选，W3 内做）**：`rosxingma-config.exe --portable` 把表读写限定在 exe 同目录 `tables\`，用于在别人机器上试表不留痕；`.ime` 仍走正式安装。

---

## 5. 分阶段实施（WBS）

| 阶段 | 名称 | 关键任务 | 交付物 / 验收 | 人日 |
|---|---|---|---|---|
| **W0** | 立项与合规 | 双轨策略定稿；86/98 数据包署名与免责文本；「新世纪不分发」决策记录；命名与文案避让 | `xingma/docs/TABLE-LICENSE-RESEARCH.md`（已完成）、`xingma/docs/DATA-PACK-SPEC.md`（本次新增） | 2–4 |
| **W1** | 引擎与 dat 内核 | `shape_dat_format.hpp`、`ShapeTable`、`ShapeSession`；`libtableio` 的 CSV/TSV 读取 + `dat_writer`；`mkdat-shape`；自造样例表跑通全链路 | `ctest` 全绿；CSV→dat 字节级确定性；自造表可查表上屏 | 12–18 |
| **W2** | IME/UI 集成 | `rosxingma.ime` 构建（依赖框架侧的 F1–F5 接口，见第 6 节；未提供项走形码侧降级）；组合/候选窗对接；**安装器 `/xingma` 模式（.ime 进 system32、工具进程序文件夹，见 4.10）**；`HKLM\SOFTWARE\RosXingMa\Install` 写入 | VM 中可安装、无表态不崩、导入演示表后可输入上屏；开始菜单与 `ImeConfigure` 都能拉起配置工具 | 9–13 |
| **W3** | 配置工具与切换 | `rosxingma-config.exe`（列表/导入向导/预览/详情/移除/排序）、`rosxingma-table.exe`（CLI）、注册表 `Tables\<id>`、revision 自检、延迟换表、LRU 缓存、托盘菜单；**数据包安装流程（清单解析、哈希校验、同意屏、升级/卸载）** | 导入多张表并来回切换；安装 86/98 数据包一次点击完成；组合中途切换不丢字；CLI 全子命令可用 | 16–22 |
| **W4** | 格式兼容与完整度 | fcitx 5 文本、ibus-table 适配器；**Rime/db/bz2 的 PC 端转换脚本**（设备端只识别拒绝）；dat→CSV/fcitx/ibus 反导出；`z` 通配；唯一候选自动上屏；标点/全半角；`ImeGetImeMenuItems` 评估 | 四类真实格式各导入一份并正确上屏；Rime 文件得到明确引导而非「格式未知」；导出可被 fcitx 5 / ibus 回读 | 10–15 |
| **W5** | 验收与发布 | VM 端到端、CI 扩展（禁用词扫描）、NOTICE/README/ROADMAP、**构建并发布 86/98 数据包（pack.ini + LICENSE + NOTICE + SHA256SUMS）**、安装包与发布说明 | 主包无码表可安装；两份数据包各自可导入、可卸载、可溯源；**全仓库无旧名残留**；文档齐备 | 8–12 |
| | | | **合计** | **57–84 人日** |

> 人日为粗估，不含法务沟通等待。相较 v2 的 58–86，v2.2 减少 1 人日：去掉设备端 Rime 解析（−3），增加安装布局拆分与 `HKLM` 安装信息（+1），W2 微调（+1）。

---

## 6. 文件级改动清单

**已就位（`xingma/`，本次搬迁完成）**

```
xingma/XINGMA-PLAN.md                 # 本文档
xingma/README.md                      # 目录导览与边界规则
xingma/docs/{TABLE-LICENSE-RESEARCH,TABLE-IMPORT-SPEC,DATA-PACK-SPEC}.md
xingma/packs/{README.md,shape86/*,shape98/*}    # 仅模板+许可，无码表数据
xingma/tools/mkdat-shape/make_pack.py
```

**新增（全部在 `xingma/` 内，镜像 `pinyin/` 的 include/src 分层）**

```
xingma/CMakeLists.txt                 # xingma-engine / rosxingma.ime / 两个 exe / libtableio / tests
xingma/include/roxingma/shape_dat_format.hpp
xingma/include/roxingma/shape_table.hpp
xingma/include/roxingma/shape_session.hpp
xingma/src/shape_table.cpp
xingma/src/shape_session.cpp
xingma/host/engine_factory_shape.cpp  # 向框架层注册形码引擎实现（若框架无注入点则自建会话存储）
xingma/host/shape_dat_loader.hpp / .cpp
xingma/host/shape_demo_table.hpp      # 自造演示编码表（与任何真实方案无关，仅自检/CI）
xingma/tools/table-io/{CMakeLists.txt,format_detect.*,csv_reader.*,fcitx_reader.*,
                       ibus_reader.*,pack_reader.*,dat_writer.*,dat_reader.*,
                       report.*,utf.*}            # 无 rime_reader：设备端不解析 Rime
xingma/tools/table-config/{CMakeLists.txt,main_gui.cpp,main_cli.cpp,ui_main.*,ui_import.*,
                          ui_pack.*,cli.cpp,registry_store.*,resource.h,app.rc,app.manifest}
xingma/tools/table-config/tests/{cli_test.py,fixtures/*}          # fixture 全部自造
xingma/tools/mkdat-shape/{CMakeLists.txt,mkdat_shape.cpp,tables.toml,build_packs.sh,
                          fetch_tables.py}
xingma/tools/table-convert/{rime2csv.py,ibusdb2csv.py,bz2csv.py,
                            export_fcitx.py,export_ibus.py,README.md}  # PC 端，仅标准库
xingma/tools/check_shape_dat.py
xingma/tests/{shape_table_test,shape_session_test,shape_switch_test,
              table_io_test,pack_test}.cpp
xingma/tests/CMakeLists.txt
```

**对框架层的接口需求（由 ros-pinyin 侧在其重构中决定，形码不代改）**

| # | 需求 | 不做会怎样 | 形码侧降级方案 |
|---|---|---|---|
| F1 | 引擎/表的构造注入点（`std::unique_ptr<imfw::Engine>` 可外部提供） | 无法在 `rosxingma.ime` 里用 `ShapeSession` | 形码自建 `xingma/host/session_store`，只复用 `imfw::Engine` 契约 |
| F2 | `dat` 定位策略可替换（不只 `GetSystemDirectoryW()`） | 形码只能把表放 system32 | `shape_dat_loader` 自行解析 `%APPDATA%`，绕开框架加载器 |
| F3 | `UiShow` 的分段参数通用化（字符区间，而非音节区间） | 形码组合串下划线位置可能不对 | 形码传整串区间，牺牲逐段高亮 |
| F4 | 按键分流按「当前表字母表/最大码长」判断 | 形码多码长表会误吃键 | 形码侧在 `should_consume_key` 之外再拦一层 |
| F5 | 安装器支持 `/xingma` 模式（装 `.ime`、不装拼音表） | 需要单独写形码安装器 | 形码自带 `xingma/tools/install` |

> 五条都只是**需求**：能由框架方顺手提供最好，任一项不提供都有明确降级路径，不构成阻塞。
> 这也是把形码放进独立目录的直接收益——框架与产品可以各自演进、互不牵制。

---

## 7. 测试策略

全部沿用「手写 check + ctest」零依赖框架；GUI 部分靠 CLI 与截图人工验收。

| 测试 | 内容 |
|---|---|
| `shape_table_test.cpp` | 加载/校验：非法 magic/版本/`max_code_len=0`/字母表含非字母/偏移越界/Code 非升序 → 全部拒绝；`lookup` 排序；`is_prefix` 边界（最长码、无后继） |
| `shape_session_test.cpp` | `make_table + feed_string + 断言`：逐码输入、最长前缀回退、1–9 选字、空格上屏、无候选时空格上屏原文、退格、Esc、码长达上限不再吃键、通配键关闭态行为 |
| `table_io_test.cpp` | 同一份**自造**数据分别写成 CSV/TSV/fcitx5/ibus 四种文本 → 解析出的 `(code,text,rank)` 集合完全一致；权重归一化；无表头列序猜测；GB18030 与 UTF-16 BOM；超长行/非法行进入 rejects；**Rime 与 `.mb` 文件返回退出码 6 且提示命令**（不是「格式未知」） |
| `shape_switch_test.cpp` | seqlock：写入过程中读到不一致 revision 会重试；组合中切换 → 不丢字、组合结束后才换表；LRU 淘汰后切回可重新加载；缺文件/损坏表的降级顺序 |
| `check_shape_dat.py` | 解码 dat v2、断言头字段、两次构建字节一致、Entry 区间连续且与 Code 对应 |
| `pack_test.cpp` | 清单必填项缺失/`FormatVersion` 未知 → 拒绝；`DataFile` 为绝对路径或含 `..` → 拒绝；`DataSha256` 不符 → 拒绝；`MaxCodeLen`/`Alphabet` 与数据冲突 → 报错而非覆盖；`EntryCountExpect` 偏差 >1% → 警告；`SHA256SUMS` 缺项/多项 → 报告 |
| `cli_test.py` | `rosxingma-table` 全子命令：导入→list→activate→export→remove 闭环；`pack install/verify/uninstall`；`--dry-run` 与真实导入结果一致；退出码矩阵 |
| 契约静态检查 | 扩展 `check_m1.py`：不得写 Preload、不得硬编码 HKL、DllMain 不做事、配置工具不得写 system32、**除 `.ime` 外安装器不得向 system32 写文件** |
| 目录边界 | `xingma/tools/check_layout.py`（形码自带，不改 ros-pinyin 的 `check_m1.py`）：`xingma/` 外任何文件出现 `xingma/` 路径即失败；`xingma/` 内出现对 `framework/`、`pinyin/` 的写操作或非 `<imfw/...>` 形式的 include 即失败 |
| 命名一致性 | `check_names.py`（并入 `check_m1.py`）：全仓库扫旧名（`wubi`/`五笔`）与旧路径，白名单仅 URL/slug/商标；命中即失败 |
| VM 端到端（无表） | 安装 → 无表提示 → 按键不吞、不崩溃 → 打开配置工具 → 空状态引导 |
| VM 端到端（数据包） | 解压 → 导入 86 包（同意屏 → 安装 → 自动成为当前表）→ 记事本输入 → 卸载 → 状态回到「无表」→ 导入 98 包重复 |
| VM 端到端（有表） | 导入 fcitx5/ibus/CSV 各一张 → 切换 → 记事本输入 → 候选/翻页/上屏 → 连续切换 50 次（复用 M1 压测脚本） |
| VM 端到端（安装布局） | 安装到非系统盘路径 → `.ime` 在 system32、工具在程序文件夹 → 开始菜单快捷方式与 `ImeConfigure` 都能拉起配置工具 → 卸载后两处均清空、用户数据保留 |

---

## 8. 风险与缓解

| 风险 | 等级 | 缓解 |
|---|---|---|
| 热切换在个别宿主进程中不生效 | **中** | 三处自检（ImeSelect / 组合起点 / 广播）；`SwitchMode=1` 提供「重启生效」确定性模式；文案明确「新开窗口生效」 |
| IMM32 每进程独立，无法全局原子换表 | 高（已通过设计消解） | 不做全局广播依赖；revision 自检；组合期延迟换表避免错字 |
| 配置工具 GUI 在 ReactOS 上的控件兼容性 | 中 | 只用 user32/comctl32 基础控件；W2 早测 ListView，退化到 ListBox；不做自绘 |
| 多格式解析的正确性（各框架格式有历史变体） | 中 | 解析器宽松但校验严格；预览页展示统计与样例，用户可肉眼确认；rejects 可导出 |
| 表数量多导致内存/启动开销 | 低 | 按需加载 + LRU 上限 2；启动只读注册表元数据，不读 dat |
| 用户导入侵权码表 | 低（责任转移） | 首启提示；导入时要求填「授权依据」；MIT 不覆盖用户数据；工具中立 |
| 86 MIT 表存在上游权利瑕疵 | 中 | 仅作可选数据包、单独署名、不作担保、可替换；发布前律师复核 |
| dat 损坏/缺失导致静默错码 | 中 | 加载即校验；失败标损坏并提示；不回退到任意表，宁可「无表态」 |
| 双 IME 共享源码重构引入拼音回归 | 中 | 改动前后跑全套既有 ctest；条件编译关闭时二进制行为不变；`check_m1.py` 继续全绿 |
| 改名残留（旧名出现在文案/路径/资源里） | 中 | 命名清单（1.1）为唯一口径；`check_m1.py` 禁用词扫描进 CI；发布前全仓库 grep 复核 |
| 配置工具装在程序文件夹后，`ImeConfigure` 找不到它 | 低 | 路径走 `HKLM\SOFTWARE\RosXingMa\Install\ConfigExe`，不硬编码；读不到时弹 MessageBox 指路（文案含开始菜单位置） |

---

## 9. 验收标准（Definition of Done）

1. `rosxingma.ime` 可在 ReactOS 0.4.16+ 通过现有安装器范式安装，动态分配 HKL，不写 Preload。
2. **无表状态可用**：全新安装无任何码表，启动不崩溃、按键不吞、组合窗提示「未安装码表」。
3. **不限数量**：可导入 ≥5 张表（含不同码长 4/5），列表正确显示、可重命名/排序/移除，无「槽位」概念残留。
4. **配置工具闭环**：GUI 与 CLI 均可完成 导入 → 预览 → 切换 → 导出 → 移除；`--dry-run` 无副作用；`rejects.csv` 可导出。
5. **格式兼容**：CSV、fcitx 5 文本、ibus-table 各至少一份真实表导入成功并正确上屏；**Rime 文件得到「请先跑 rime2csv.py」的明确引导**，`rime2csv.py` 转换后的 CSV 可导入；fcitx4 `.mb` 与 `.db`/bz2 返回退出码 6；反导出文件可被 fcitx 5 / ibus 回读。
6. **数据包闭环**：86、98 两份数据包各自可被配置工具一次导入（清单解析 → 哈希校验 → 同意屏 → 安装），安装后自动带名称/码长/字母表，无需手填；可卸载、可升级、许可证与上游提示常驻可查；包内数据哈希不符时拒绝安装。
7. **切换行为**：同窗口下一次按键生效；切回其它窗口输入时生效；组合中途切换不丢字、不残留候选、不崩溃；`SwitchMode=1` 时明确提示重启。
8. **降级可预期**：dat 缺失/损坏 → 标损坏 + 提示 + 无表态，不静默错码。
9. 既有拼音 IME 全部 ctest 与 M1 契约检查保持通过。
10. `NOTICE.txt` 明确区分「MIT 代码 / 可选数据包 / 用户导入数据」，并声明用户导入数据不在 MIT 覆盖范围；仓库内无任何真实码表数据（`xingma/packs/*/table.csv` 由构建脚本生成且被 gitignore）。
11. **命名统一**：全仓库（代码、资源、文档、界面文案）无旧名残留，白名单仅上游仓库 slug/URL、`极点五笔`、`五笔字型` 三类事实引用；`check_m1.py` 命名扫描全绿。
12. **安装布局**：`.ime` 在 `system32`、`rosxingma-config.exe` 与 `rosxingma-table.exe` 在 `%ProgramFiles%\RosXingMa\`；除 `.ime` 外 system32 无新增文件；卸载清理干净且默认保留用户码表。
13. **目录隔离**：形码专属文件全部位于 `xingma/`，形码不修改 ros-pinyin 的结构、构建文件与说明文档（`framework/`、`pinyin/`、根 `CMakeLists.txt`、`NOTICE.txt`、`README.md` 均无因形码而起的改动，`NOTICE.txt` 第 8 节除外——该节是形码授权声明，需保留）；`xingma/README.md` 的目录表与实际文件一致。

---

## 10. 决策点与已定事项

**已定**

1. **产品命名**：`RosXingMa` —— `rosxingma.ime`、`rosxingma-config.exe`、`rosxingma-table.exe`、`HKCU\SOFTWARE\RosXingMa`、`%APPDATA%\RosXingMa`；全仓库改名，见 1.1 命名清单。
2. **发布 86/98 可选数据包**：以「可导入包 + 预配置清单」形式单独下载，不与主包合并授权单元（见 4.4.1 与 `xingma/docs/DATA-PACK-SPEC.md`）。
3. **Rime `dict.yaml`**：只给 PC 端脚本 `xingma/tools/table-convert/rime2csv.py`，设备端不解析、只识别并引导（见 4.4、`xingma/docs/TABLE-IMPORT-SPEC.md` 第 6 节）。
4. **fcitx 4**：不支持，`.mb`（二进制与文本）与 `[CodeTable]` 一律拒绝；仅支持 fcitx 5 文本。永久排除，不再作为待办。
5. **配置工具形态**：独立 `rosxingma-config.exe`，**与 IME 分开放置**在 `%ProgramFiles%\RosXingMa\`，不进 system32、不并入系统属性页（见 4.10）。

**待定**

1. **数据包下载渠道**：随 Release 附件（GitHub Releases）还是单独站点；两者都不进主安装包，仅发布位置不同。
2. **便携模式优先级**：`rosxingma-config.exe --portable`（表放 exe 同目录）是否 W3 必做，还是延到 W5。
