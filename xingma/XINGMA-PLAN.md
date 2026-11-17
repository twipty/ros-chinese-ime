# RosXingMa 形码输入法 · 实施计划（正式版 1.0）

> 本文件是 RosXingMa 项目的**唯一实施依据**。开发、评审、验收均以本文件为准。
> 方案调整直接修订本文件并递增版本号；历史版本存入 `xingma/archive/`，不再作为依据。

| 项 | 内容 |
|---|---|
| 版本 | 1.0（正式版） |
| 适用范围 | `rosxingma.ime`、`rosxingma-config.exe`、`rosxingma-table.exe`、可选数据包及其文档 |
| 上游依赖 | ros-pinyin 的共用框架层 `framework/`（命名空间 `imfw`）；拼音产品 `pinyin/` 只读参照 |
| 实施范围 | `xingma/` 目录内的全部代码、工具与文档 |
| 目标平台 | ReactOS 0.4.16+ / Windows（32 位 x86，MinGW 交叉编译） |

**关联文档**

| 文档 | 职责 |
|---|---|
| `xingma/docs/TABLE-LICENSE-RESEARCH.md` | 码表授权尽调、来源许可盘点、分发矩阵 |
| `xingma/docs/TABLE-IMPORT-SPEC.md` | 导入格式语法、校验规则、错误码、反导出 |
| `xingma/docs/DATA-PACK-SPEC.md` | 数据包结构、`pack.ini` 全字段、构建流程 |
| `xingma/README.md` | 目录导览与边界规则 |
| `NOTICE.txt` 第 8 节 | 授权分层声明（MIT 代码 / 可选数据包 / 用户数据） |

---

## 1. 概述

### 1.1 产品定义

RosXingMa 是一个**通用形码输入法**：用户导入任意「字母编码 → 汉字/词」的码表，即可用该方案打字。同一份引擎不作任何编码方案假设——字母表、最大码长、通配规则、是否有词，全部由表数据驱动，因此同一二进制可承载 86 版、98 版、新世纪版，也可承载郑码、仓颉、二笔等任意拉丁字母形码表。

产品**不内置任何码表**。常见方案以「可选数据包」形式单独下载，或由用户从自有来源导入。

**一句话方案**：复用 ros-pinyin 已交付的 IMM32 框架（引擎契约、按键路由、候选分页、窗口、安装范式），形码侧只提供三件事——表容器、会话引擎、方案插件；码表以「不限数量的独立表」形式存放在用户目录，由独立配置工具导入与切换，切换靠「版本号自检 + 组合期延迟换表」热生效。

### 1.2 目标

1. 产出 `rosxingma.ime`，与 `rospinyin.ime` 并存安装、互不干扰，共用框架层源码而不复制。
2. 引擎与编码方案彻底解耦：字母表、最大码长、通配键、单字/词组均由表数据决定。
3. 码表数量不设上限：用户可导入任意多张表，随时增删、重命名、排序、切换。
4. 提供独立的码表管理工具：`rosxingma-config.exe`（GUI）与 `rosxingma-table.exe`（CLI），覆盖导入、预览、校验、导出、移除、切换。
5. 导入兼容主流框架的文本码表：规范 CSV/TSV/空白分隔 TXT、fcitx 5 文本表、ibus-table 源表；Rime 字典由 PC 端脚本转换后导入。数据可反导出，不锁定用户。
6. 以「可导入包 + 预配置清单」形式额外发布 86/98 两套可选数据包，导入时无需手填参数，并强制展示来源与上游权利提示。
7. 命名、目录、注册表、界面文案统一，形码专属文件全部隔离在 `xingma/`，不改动 ros-pinyin 的结构、构建文件与说明。

### 1.3 非目标

- 不改变 `rospinyin.ime` 的行为与数据格式；拼音继续使用 `rospinyin.dat`。
- 不做整句输入、云词库、自动造词、自学习词频。
- 不做拼音与形码混合输入，不做 `z` 键拼音反查。
- **fcitx 4 永久不支持**：`.mb`（二进制与文本形态）与 `[CodeTable]` 配置一律拒绝，仅支持 fcitx 5 文本表。
- `.scel`/`.qcel`、ibus `.db`、`.txt.bz2`、Rime `dict.yaml` 均**不在设备端解析**，只在 PC 端脚本处理。
- 主安装包不含任何码表；新世纪版任何形式都不分发。

### 1.4 关键决策

| # | 决策 | 结论 | 影响面 |
|---|---|---|---|
| D1 | 产品形态 | 同仓库双 IME：`rosxingma.ime` 与 `rospinyin.ime` 并列，共用 `framework/` | 拼音零改动 |
| D2 | 表数量模型 | 不限数量的独立表，不预设固定套数与占位 | 数据模型、切换、工具 |
| D3 | 切换机制 | revision 自检 + 组合期延迟换表，热切换为主 | 会话与存储 |
| D4 | 数据分发 | 主包零码表；86/98 单独发数据包；新世纪不发 | 合规、安装器 |
| D5 | 工具形态 | 独立 `rosxingma-config.exe` + `rosxingma-table.exe`，装程序文件夹 | 安装布局 |
| D6 | Rime 支持边界 | 只给 PC 端脚本 `rime2csv.py`，设备端只识别并引导 | 导入管线 |
| D7 | 编码方案归属 | 全部编码规则在导入/构建期展开进 dat，运行时只做前缀查表 | 引擎复杂度 |

---

## 2. 命名与仓库布局

### 2.1 命名清单（唯一口径）

| 层面 | 名称 | 说明 |
|---|---|---|
| 产品 | `RosXingMa`（显示名「RosXingMa 形码输入法」） | 界面与文案统一使用 |
| IME 模块 | `rosxingma.ime` | 与 `rospinyin.ime` 并列 |
| 配置工具 | `rosxingma-config.exe` | 独立 GUI，装程序文件夹 |
| 命令行工具 | `rosxingma-table.exe` | console 子系统，供脚本与 CI |
| 用户设置 | `HKCU\SOFTWARE\RosXingMa` | 无历史用户，不做旧键迁移 |
| 机器级安装信息 | `HKLM\SOFTWARE\RosXingMa\Install` | 安装目录、工具路径、HKL |
| 用户数据 | `%APPDATA%\RosXingMa\{tables,reports,packs}` | 免管理员权限 |
| 程序文件 | `%ProgramFiles%\RosXingMa\` | 工具、文档、许可证 |
| 引擎构件 | `ShapeTable` / `ShapeSession` / `ShapeScheme` / `shape.dat`（magic `ROSSHDAT`） | 与具体方案解耦 |
| 仓库目录 | `xingma/`；兄弟目录 `framework/`、`pinyin/` | 见 2.2 |
| 数据包标识 | `PackId` = `shape86-jidian`、`shape98-tables` | 仓库只存模板与许可 |
| 发布物 | `RosXingMa-Pack-Shape86-v1.0.0.zip` | 单独下载，不进主安装包 |

### 2.2 仓库布局与依赖方向

```
ros-pinyin/
  framework/    ← 共用框架（imfw）：契约 include/imfw、宿主 host、界面 ui、核心 src
  pinyin/       ← 拼音产品（rospinyin）：Scheme 实现与会话
  ime/          ← 拼音 .ime 的装配件（scheme_reg.cpp、def、资源）
  tools/ tests/ m0/ m2/ vendor/          ← 拼音既有资产
  CMakeLists.txt  NOTICE.txt  LICENSE  README.md  ROADMAP.md
  xingma/       ← 本产品线全部文件
    XINGMA-PLAN.md                       # 本文件
    README.md  archive/
    docs/{TABLE-LICENSE-RESEARCH,TABLE-IMPORT-SPEC,DATA-PACK-SPEC}.md
    packs/{README.md,shape86/,shape98/}   # 模板与许可，无码表数据
    include/roxingma/  src/  host/  res/
    tools/{table-io,table-config,table-convert,mkdat-shape}
    tests/
```

**依赖方向单向**：`xingma/` → `framework/`。形码不向 `framework/`、`pinyin/`、`ime/` 添加文件，也不修改它们与根构建脚本。需要框架变化时以「缺口单」提出（见 3.4），由框架侧决定是否采纳。

### 2.3 命名纪律

- 代码、注册表、路径、文件名、界面文案、发布说明中**不出现**旧名（`wubi` / `五笔`，大小写皆算）。
- **例外仅四类**，必须原样保留（改了就是错误的引用）：上游仓库 slug 与 URL、上游数据文件名、上游产品名、商标名（用于避让陈述）。
- 由 `xingma/tools/check_layout.py` 扫描：命中旧名且不落在白名单即失败。

---

## 3. 复用边界

### 3.1 框架提供的契约

ros-pinyin 交付的 `framework/` 已经把「IME 公共部分」抽成可插拔框架，形码直接消费，不复制代码。

| 契约 | 位置 | 内容 |
|---|---|---|
| 引擎接口 | `framework/include/imfw/engine.hpp` | `Engine::feed/backspace/cancel/on_select`；`Result{action, composition, candidates, segments, commit_text}`；`Action{Passthrough, Absorb, Commit}` |
| 方案插件 | `framework/include/imfw/scheme.hpp` | `Scheme`：四类窗口类名、工具栏标签、`input_char` / `is_input_char`、无参 `create_engine`、`reload_data` 数据热重载钩子；`scheme_instance()` 由装配件定义（框架不传递码表，方案完全自管数据） |
| 码表视图 | `framework/include/imfw/code_table.hpp` | `CodeTable`：`load_dat` / `add` / `build_index` / `lookup` / `is_code` / `is_prefix`，**条目为单个码点** |
| 候选分页 | `framework/include/imfw/candidate_pager.hpp` | 每页 9 条、跨页选号、`reset/select/next_page/prev_page` |
| 宿主层 | `framework/host/` | IMM32 全部导出、按键路由、会话存储、`ImeConfigure`、UI server 类注册 |
| 界面层 | `framework/ui/` | 组合窗、候选窗、工具栏；`UiShow(hIMC, text, segments, pager)` / `UiHide` / `UiDestroy` |
| 装配范式 | `ime/scheme_reg.cpp` | 5 行定义 `imfw::scheme_instance()`，其余全为框架源码；其 `CMakeLists.txt` 明确「第二个输入法用自己的 scheme_reg 链接同一框架」 |

**框架已经解决的、形码不需要再提的需求**：

- 按键路由按方案参数化——`input_char(vk)` / `is_input_char(cp)` 由方案提供，宿主不写死字母范围。
- 组合串分段可视化——`Result.segments` 已通用（注释即写明「shape IMEs: typed codes」）。
- 候选分页、选字键、翻页键、退格、Esc 的语义由框架统一处理，形码不得重定义。

### 3.2 形码需实现的构件

| 构件 | 作用 | 不复用框架的原因 |
|---|---|---|
| `ShapeTable` | 多编码、多条目、支持**多字词**的只读表 | 框架 `CodeTable` 条目是单码点（`CharEntry.codepoint`），无法表示词 |
| `ShapeStore` | 多表管理：按 id 加载、LRU 缓存、revision 自检 | 框架 `Store` 只加载 system 目录单一 dat 且首次加载后不再重载 |
| `ShapeSession` | 实现 `imfw::Engine`：编码累积、最长前缀、上屏 | 拼音会话含归一化与音节切分，语义不同 |
| `ShapeScheme` | 实现 `imfw::Scheme`：窗口类名、字母表、引擎构造 | 方案身份与数据来源由形码决定 |
| `ShapeDatLoader` | 从 `%APPDATA%\RosXingMa\tables\` 读 dat | 框架只认系统目录单一文件 |
| `libtableio` | 格式识别、解析、校验、写 dat、反导出 | 框架不含码表编译能力 |

### 3.3 装配模型

`rosxingma.ime` 由「框架源码 + 形码插件 + 形码装配件」链接而成，**框架源码按相对路径引用，不复制**：

```
rosxingma.ime
├── 装配件      xingma/host/scheme_reg_shape.cpp   → 定义 imfw::scheme_instance()
├── 框架宿主     ../framework/host/{dllmain,imm_entry,ui_server,session_store}.cpp
├── 框架界面     ../framework/ui/{ime_ui,ui_window,composition_window,candidate_window,toolbar}.cpp
├── 形码插件     xingma/src/{shape_table,shape_session,shape_store,shape_scheme}.cpp
├── 形码装配     xingma/host/shape_dat_loader.cpp
└── 资源         xingma/host/rosxingma.def, xingma/res/{rosxingma.rc,rosxingma.ico,resource.h}
```

`xingma/CMakeLists.txt` 是**独立顶层工程**：`cmake -S xingma -B build-xingma` 即可配置，框架源码按 `../framework/...` 相对路径直接编译。是否需要被根工程 `add_subdirectory(xingma)` 由框架侧自行决定，形码不依赖、也不改动根 `CMakeLists.txt`。

构建约束沿用拼音侧已验证的做法：输出名恰为 `rosxingma.ime`（无前缀后缀）、`-static` 静态链接运行库（ReactOS 无 libstdc++/libgcc/libwinpthread）、不新增导入 DLL。

### 3.4 框架能力缺口与处置

框架已交付，实读源码后确认四个缺口。**2026-11-16 更新：四个缺口已全部在框架侧解决**（见 `framework/README.md`「框架能力缺口的处置记录」），形码无需自建宿主层，按下表「框架侧落地」接入即可。**2026-11-17 更新：G2 演进为终局形态——框架不再加载/持有/传递码表，`dat_filename()`/`load_fallback()` 哨兵值契约移除，`create_engine()` 无参 + `Scheme::reload_data()` 数据热重载钩子；形码数据完全自管不再需要任何哨兵约定。**

| # | 缺口（原始记录） | 实际影响 | 框架侧落地（已完成） |
|---|---|---|---|
| **G1** | 宿主层只上屏**一个码点**：`process_key` 取 `commit_text[0]`，选字取 `chosen->text[0]`；`emit_char` 只写 `messages[0]` 并返回 1 | **词无法上屏**——候选窗能看到词，选中只输出首字，形码词库等于失效 | `imfw::emit_text()`（`framework/include/imfw/emit_text.hpp`，imfw-engine 内、Linux 单测覆盖）按 UTF-16 码单元逐条写 `WM_IME_CHAR`，非 BMP 走代理对，上限 32 条。选字与 Commit 路径均全量上屏，词组候选完整输出 |
| **G2** | 框架只加载 `GetSystemDirectoryW()\<dat_filename>` 单一 `CodeTable`，且 `Store::table_loaded_` 首次加载后不再重载 | 无法多表、无法热切换、无法存词 | **终局（2026-11-17）**：框架不再加载/持有/传递码表——`create_engine()` 无参，引擎数据由 `ShapeStore` 直接供给；`reload_sessions()` 先调 `Scheme::reload_data()`（形码在此刷新当前表）再丢弃全部会话（`ImeConfigure` 成功后自动调用），配置工具换包即刻生效。通用 `CodeTable` 保留为框架工具类，形码可不使用 |
| **G3** | `ImeConfigure` 恒返回 `FALSE`，语言栏菜单亦未实现 | 用户需从开始菜单或命令行打开配置工具 | `Scheme::configure(void* parent)` 回调已接入 `ImeConfigure`：返回 `true` 即视为已处理并触发 `reload_data()` + 会话重载。形码在回调内拉起配置工具（进程内 UI 或 `CreateProcess` 外部 exe 均可）；开始菜单快捷方式与 `HKLM\...\Install\ConfigExe` 仍照旧写入，作为冗余入口。语言栏菜单项仍留待框架 M6 |
| **G4** | 空格与 `-` `=` `,` `.` 被宿主固定为选字/翻页键，引擎收不到 | 「无候选时空格上屏原文」无法实现；标点也不能进组合串 | 选字/翻页键策略下放 Scheme：`is_selection_key()` / `page_key()`（默认即经典行为，形码按需覆盖）；框架仅在「组合中且有候选」时拦截这些键，否则直达应用。**组合串与 `segments` 契约改为 UTF-8**（`imfw::utf8_to_wide` 解码渲染、坏字节按字节兜底），「未安装码表」中文提示可以放进组合串，不再受 ASCII 限制。「无候选且达最大码长自动提交原文」仍按本计划 4.x 由 `ShapeSession` 用 `Action::Commit` 实现——框架保证此时空格/标点直达应用，两者配合即达成「空格上屏原文」的等效体验 |

> **G1 的降级说明（已作废）**：原计划在框架未补齐时自建宿主层 `xingma/host/imm_entry_shape.cpp`。框架已按上述方案补齐（含 `tests/emit_text_test.cpp` 回归测试与 `check_m1.py` 静态断言），**该降级路径取消**，形码直接复用共享宿主层。
>
> **G4 的连带结论（已放宽）**：组合串原限制为 ASCII（宿主按字节直转宽字符）。框架现按 UTF-8 解码（Windows 侧 wchar 为 UTF-16、非 BMP 走代理对），CJK 提示与形码字根显示均可进组合串；空态提示仍建议由开始菜单、配置工具与首启提示承担。

---

## 4. 数据模型

### 4.1 `shape.dat` v2

沿用 v1 的「定长头 + 定长记录数组 + 字符串池」与字节级确定性，仅改语义与 magic：

```
magic "ROSSHDAT" (8B), version = 2
Header (64B):
  magic[8], version, flags, entry_count, code_count,
  entry_table_off, code_table_off, pool_off, pool_size,
  name_off,            // 池内偏移：显示名，空串非法
  alphabet_off,        // 池内偏移：编码字母表，如 "abcdefghijklmnopqrstuvwxy"
  source_off,          // 池内偏移："sha256:<hex>;format:<csv|tsv|fcitx|ibus|pack>;name:<utf8>"，0=无
  max_code_len,        // 1..8
  reserved[2]
Entry (20B): text_off, text_len, rank, entry_type, flags
  // text: 池内 UTF-8 字符串，单字或词；text_len 为字节数
  // rank: 越小越靠前（导入期由各来源权重归一化）
  // entry_type: 0=单字, 1=词
  // flags: bit0 = 仅用于组词，不参与直接上屏（fcitx `^` / ibus 特殊条目）
Code (16B): code_off, first_entry, entry_count, flags
  // code: 池内编码串；对应 Entry 按 (rank, text 码点) 升序且连续
  // flags: bit0 一级简码 / bit1 二级 / bit2 三级 / bit3 全码 / bit4 含识别码（导入期推断，仅作界面提示）
static_assert(Header == 64, Entry == 20, Code == 16)
```

`ShapeTable::load_dat()` 必须校验：magic、version、`max_code_len ∈ [1,8]`、`alphabet` 非空且全为 ASCII 字母、各偏移不越界、`count × size` 不溢出、字符串池 NUL 终止、`Code` 严格升序不重复、`first_entry + entry_count ≤ entry_count`。任一失败整体拒绝，**不做部分加载**。

设计取舍：

- **编码规则全部构建期展开**（全码、简码、识别码、词码），运行时不持有字根表与笔画表，DLL 保持最小。
- **无 `scheme` 枚举**：显示名是自由字符串，用户可导入「我的86改」「公司98定制」而不受固定名约束。
- **`source_off` 记录来源与哈希**：用于「查看来源与许可」、去重与可复现。

### 4.2 多表存储

```
%APPDATA%\RosXingMa\
  tables\<id>.dat            ← 每张表一个 dat
  tables\<id>.source.txt     ← 可选：用户原始文件副本（默认不留）
  reports\<id>.log           ← 导入报告（总行/有效/去重/跳过明细）
  packs\<PackId>\LICENSE     ← 数据包随附许可证留存
```

- 表数量上限由磁盘与注册表决定，不设人为上限；单表上限：源文件 64 MB / 200 万行。
- **id 规则** `[a-z0-9][a-z0-9_-]{0,31}`：默认由「名称 slug + 内容 sha256 前 4 位」生成（如 `shape86-a1b2`），冲突追加 `-2`；CLI 支持 `--id` 以便脚本化。
- **文件与注册表成对存在**：有登记无文件 → 标「缺失」，加载时跳过并尝试下一张；有文件无登记 → 视为「未登记」，工具提供「扫描并登记」。

### 4.3 注册表

```
HKCU\SOFTWARE\RosXingMa\
  Settings\
    ActiveTable       REG_SZ    当前表 id；空串 = 无表
    Revision          REG_DWORD 表集合或当前选择变化时 +1（自检依据）
    SwitchMode        REG_DWORD 0=auto（默认）/ 1=仅下次加载生效
    AutoCommitUnique  REG_DWORD 唯一候选自动上屏，默认 0
    EmitRawOnNoMatch  REG_DWORD 无候选且码长已满时上屏原文，默认 1
    FirstRunShown     REG_DWORD 首启合规提示是否已读
  Tables\<id>\
    Name / Path / Sha256 / EntryCount / CodeCount / MaxCodeLen / Alphabet
    SourceKind        pack | import | demo
    SourceFormat      csv | tsv | fcitx | ibus | pack
    SourceName / OriginPath / ImportedAt / Order
    License / LicensePath
    PackId / PackVersion / SourceUrl / UpstreamNotice   （仅 pack）
```

- 不写 `Keyboard Layout\Preload`、不设默认布局、不硬编码 HKL（沿用 M0 红线）。
- **写入顺序固定**：表文件 → `Tables\<id>` → `ActiveTable` → `Revision++`。任何一步失败即回滚说明，绝不让运行侧读到半成品。

### 4.4 排序与权重语义

各来源权重语义一致（数值越大越靠前），统一 `rank = 65535 - min(weight, 65535)`；无权重列时 `rank = 行号` 保序。同一 `(code, text)` 多行/多来源重复 → 取最小 rank。编码内条目按 `(rank, text 码点)` 升序，保证候选顺序可复现。

---

## 5. 运行时设计

### 5.1 `ShapeSession`

实现 `imfw::Engine`，规则由表参数驱动：

| 参数 | 来源 | 示例 |
|---|---|---|
| `max_code_len` | dat 头 | 形码 4、仓颉 5、郑码 4 |
| `alphabet` | dat 头 | `a`–`y`；仓颉含 `z` |
| `wildcard` | 推导：`z` 不在 `alphabet` 中则 `z` 为通配，否则无 | 形码 `z`；仓颉无 |

行为规则：

1. 组合串长度受 `max_code_len` 约束：达到上限后不再追加（返回不含新字符的 `Result`）。注意宿主在 `should_consume_key` 阶段已按字母表判定吃键，因此该键不会落到应用——这与拼音侧行为一致，缓解见 5.4。
2. 每次输入后取**存在精确候选的最长前缀**：`q` 有简码即显示；续打 `r` 后 `qr` 无候选则回退显示 `q` 的候选，组合串仍显示 `qr`。
3. 空格、`1`–`9`、翻页键、退格、Esc 由框架处理，引擎不重复实现；引擎只负责 `feed` / `backspace` / `cancel` 的状态。
4. 无候选且组合已达 `max_code_len`：按 `EmitRawOnNoMatch` 提交原文（`Action::Commit`），避免用户输入丢失。这是「空格上屏原文」的替代路径（见 G4；框架已保证无候选时空格/标点直达应用）。
5. 唯一候选：码长已满且仅 1 个候选时按 `AutoCommitUnique` 决定是否直接提交，默认关闭。
6. 通配键：先按普通字母参与查表；W4 实现运行时区间扫描（遍历前缀全部编码做后缀匹配），**不做构建期笛卡尔展开**。
7. 上屏多字词依赖 G1；**G1 已在框架侧解决**（`imfw::emit_text`，见 3.4），`ShapeSession` 完整返回词的 `commit_text` 即可全量上屏，无需降级。

### 5.2 `ShapeScheme`

| 方法 | 返回值 | 理由 |
|---|---|---|
| `ui_server_class()` | `L"RosXingMaUI"` | 与拼音窗口类名互不冲突，两个 IME 可同进程共存 |
| `composition_class()` / `candidate_class()` / `toolbar_class()` | `L"RosXingMaComp"` / `L"RosXingMaCand"` / `L"RosXingMaBar"` | 同上 |
| `toolbar_label()` | `L"形"` | 固定单字标签，与拼音的「拼」区分 |
| `input_char(vk)` | 字母键 → 小写码点，其余 `0` | 大小写归一由此完成 |
| `is_input_char(cp)` | 当前表字母表内为 `true`；**无表时恒 `false`** | 无表即透传，不吞键 |
| `is_selection_key()` / `page_key()` | 不覆盖（用框架默认） | G4：默认空格+数字选字、`-`/`=`/`,`/`.` 翻页即形码所需 |
| `create_engine()` | `ShapeSession`（从 `ShapeStore` 取当前表；框架不再传任何表，G2 终局契约） | 见 G2 |
| `reload_data()` | 通知 `ShapeStore` 重读 revision/当前表 | G2：`reload_sessions()` 先调它再清会话，配置工具换包即刻生效 |
| `configure(parent)` | 拉起配置工具后返回 `true` | G3：接管 `ImeConfigure`，成功后框架自动 `reload_data()` + `reload_sessions()` |

### 5.3 表切换

IMM32 的 IME DLL 按进程加载，各进程持有各自的 `ShapeStore`，不存在跨进程共享内存。因此切换不是「一次原子操作覆盖全局」，而是「每个进程在确定时机发现 revision 变化」：

1. **revision 协议（seqlock 读）**：`R1 = Revision` → 读 `ActiveTable` → `R2 = Revision`，`R1 == R2` 才采用，不一致重试一次，仍不一致本次不换。
2. **自检时机**（仅两处，均不在按键热路径上）：
   - `on_select(true)`：框架在 `ImeSelect` 与 `ImeSetActiveContext` 激活时调用，幂等，适合做一次比对。
   - **组合起点**：`composing == false` 且收到首个字母键时比对内存中的 `cached_revision_`。这是覆盖「应用长期持有 HIMC、不重新激活」场景的关键。
   - 不依赖 `WM_SETTINGCHANGE` 广播（IMM32 创建的窗口在 ReactOS 上能否收到未验证）。
3. **换表流程**（`CRITICAL_SECTION` 内）：
   1. `composing == true` → 只记 `pending_revision_`，等本次组合结束再换，避免已打的码被新表解释成别的字。
   2. 解析目标 id → `Path` / `Sha256`；id 为空 → `current_ = nullptr`（无表）。
   3. 缓存命中直接用；未命中则加载并校验；失败则标记该表损坏、保持原表并留日志，**不静默换到别的表**。
   4. 原子替换 `current_` / `active_id_` / `cached_revision_`；旧表由 `shared_ptr` 保活到最后一个会话释放。
4. **缓存**：`std::map<id, shared_ptr<const ShapeTable>>` + LRU 上限 2（当前表 + 上一张），仅淘汰 `use_count()==1` 的项；淘汰后再切回重新读盘（1–3 MB，毫秒级）。
5. **生效预期**（写进界面文案，不夸大）：同窗口组合已结束 → 下一次按键生效；其它已打开程序 → 切回该窗口输入时生效；极少数不响应消息且长期不重新激活的程序 → 需重启该程序，`SwitchMode=1` 可固定为「只在下一次加载生效」。

### 5.4 交互与界面

- 组合窗显示编码串；`segments` 传整串（不做拼音式逐段高亮）。
- 候选窗 1–9 编号，`-` `=` `,` `.` 翻页，沿用框架实现。
- **无表状态**：不吞键（`is_input_char` 返回 `false`），不弹组合窗；通过首启提示与配置工具空状态引导。这是刻意的——吞键却不显示任何内容会让用户以为键盘坏了。
- 工具栏显示固定标签「形」；当前表名在配置工具中查看与切换。
- **已知限制**：达最大码长后继续按字母键，该键被 IME 吞掉（不改变组合串、不产生输出）。v1 不做特殊提示；W4 评估「再按字母即上屏首候选」作为缓解，若实现须补一条 host 测试。
- 详情页与提示文案统一使用「码表」而非方案专名，避免与商标冲突。

### 5.5 安装布局

IMM32 只在系统目录按文件名查找 `.ime`，因此 `rosxingma.ime` 必须进 `system32`；其余组件一律不进。安装器由形码自带 `xingma/tools/install/`，**不改动拼音的 `tools/install-rospinyin/`**。

| 组件 | 位置 | 写入需管理员 |
|---|---|---|
| `rosxingma.ime` | `%SystemRoot%\system32\` | 是（仅安装/卸载） |
| `rosxingma-config.exe` / `rosxingma-table.exe` | `%ProgramFiles%\RosXingMa\` | 是（安装）/ 否（运行） |
| `LICENSE.txt` / `NOTICE.txt` / `README.txt` / `docs\*.md` | `%ProgramFiles%\RosXingMa\` | 是（安装） |
| 用户数据 | `%APPDATA%\RosXingMa\{tables,reports,packs}` | 否 |
| 开始菜单 | 「程序 → RosXingMa」下「码表管理」「命令行帮助」 | 是 |

要点：

1. **路径靠注册表，不靠约定**：安装器写 `HKLM\SOFTWARE\RosXingMa\Install`（`InstallDir`、`ConfigExe`、`TableExe`、`HKL`），配置工具、卸载器都从这里读，装到任意盘符都不出错。
2. 不写 `PATH`、不注册 COM、不做文件关联；除 `.ime` 外不向 `system32` 写任何文件。
3. **卸载**：删 `system32\rosxingma.ime`、`%ProgramFiles%\RosXingMa`、`HKLM\SOFTWARE\RosXingMa`、`HKCU\SOFTWARE\RosXingMa`、反注册 HKL；`%APPDATA%\RosXingMa` 默认**保留**，卸载页提供「同时删除我的码表」勾选且默认不勾。
4. **便携模式**（可选）：`rosxingma-config.exe --portable` 把表读写限定在 exe 同目录 `tables\`，用于在他人机器上试表不留痕；`.ime` 仍走正式安装。

---

## 6. 码表工具链

### 6.1 配置工具 `rosxingma-config.exe`

**定位**：独立 Win32 GUI（仅 user32/comctl32/shell32/advapi32，无 MFC/Qt/第三方库），运行期免管理员（只写 `HKCU` 与 `%APPDATA%`）。**不链接引擎**，只链接 `libtableio`，避免把会话逻辑带进界面。

```
┌ RosXingMa 码表管理 ─────────────────────────────────────────────┐
│ 当前生效：[ 86 版形码（极点）        ▼ ]   [设为当前]  [刷新]    │
├───────────────────────────────────────────────────────────────┤
│ 名称              │ 码长 │ 编码数 │ 条目数 │ 格式 │ 许可证 │ 状态 │
│ 86 版形码（极点） │  4   │ 25,631 │ 89,272 │ fcitx│ MIT    │ 生效 │
│ 我的98定制        │  4   │ 28,104 │112,584 │ csv  │ 自述   │      │
│ 仓颉五代          │  5   │ 41,220 │ 98,110 │ ibus │ 自述   │缺文件│
├───────────────────────────────────────────────────────────────┤
│ [导入码表…] [导出…] [重命名] [移除] [上移] [下移] [详情]        │
│ 提示：切换后新开窗口即生效；极少数程序需重启。                  │
└───────────────────────────────────────────────────────────────┘
```

**导入向导（两步）**

1. **选文件**：`GetOpenFileNameW`，过滤器 `*.csv;*.tsv;*.txt;*.ibus-table;*.yaml;*.dat;*.ini`，支持多选批量导入与整目录拖放（目录含 `pack.ini` 即按数据包处理）。
2. **预览与确认**：自动判定格式（可下拉改判）、编码、分隔符；统计总行/有效/去重/跳过/编码数/最大码长/字母表；前 20 条样例（`编码 → 候选`）；告警列表；名称与许可证输入框（许可证提示「请填写您持有该码表的授权依据」）；「另存原始文件副本」勾选。确认后进度条与结果摘要（耗时、写入路径、是否设为当前）。

**关键交互**

- 内容哈希与已有表相同 → 提示「同内容表已存在」，提供「覆盖 / 另存为」。
- 移除当前表 → 禁止直接删，先要求改选或切到「无表」。
- 缺文件/损坏 → 状态列标红，详情页给修复建议（重新导入 / 指向新路径 / 移除登记）。
- 首启无表 → 空状态引导 + 合规提示（「本输入法不附带码表，请导入您合法持有的码表」），仅出现一次。
- 详情页：名称、id、路径、SHA-256、格式、来源文件、导入时间、条目数、许可证、dat 头自检结果、上游提示原文（数据包表常驻可查）。

**实现要点**：`ListView`（`LVS_REPORT`），目标 ReactOS 构建缺 comctl32 版本时降级 `ListBox` + 详情区；耗时操作用工作线程或分段 + `PumpMessages`，避免「未响应」；宽字符 API + UTF-8 落盘；中文路径用 `\\?\` 兼容。

### 6.2 命令行工具 `rosxingma-table.exe`

GUI 子系统程序做输出重定向不可靠，故另出 console 版本，与 GUI 共享 `libtableio`：

```
rosxingma-table list
rosxingma-table import <file> [--name N] [--id ID] [--format auto|csv|tsv|fcitx|ibus]
                             [--dry-run] [--force] [--activate] [--keep-source]
                             [--report <path>] [--rejects <path>]
rosxingma-table export <id> --out <file.csv> [--format csv|fcitx|ibus]
rosxingma-table activate <id> | --none
rosxingma-table rename <id> <new-name>
rosxingma-table remove <id> [--force]
rosxingma-table reorder <id> <order>
rosxingma-table verify <id>
rosxingma-table diagnose
rosxingma-table scan
rosxingma-table pack install <dir|pack.ini> [--dry-run] [--activate]
rosxingma-table pack verify  <dir|pack.ini>
rosxingma-table pack uninstall <PackId>
```

退出码：`0` 成功 / `1` 用法错误 / `2` 解析失败 / `3` 校验失败 / `4` IO 失败 / `5` 已存在（需 `--force`）/ `6` 不支持的格式 / `7` 无权限或目录不可写。`--dry-run` 与真实导入走同一代码路径，保证预览与实际一致。

### 6.3 格式支持

| 格式 | 识别方式 | 设备端内建 | PC 端脚本 | 备注 |
|---|---|---|---|---|
| 规范 CSV（RFC 4180） | `.csv` / 首行含逗号表头 | ✅ | — | 首选交换格式 |
| TSV / 空白分隔 TXT | `.tsv` / `.txt` | ✅ | — | 极点系与 98 表常见 |
| fcitx 5 文本表 | 含 `[Table]` / `[Data]` / `[数据]` / `键码=` | ✅ | — | 中英键名均接受 |
| ibus-table 源表 | 含 `BEGIN_DEFINITION` | ✅ | — | 忽略 `CHAR_PROMPTS` |
| Rime `*.dict.yaml` | 含 `columns:` 与 `...` 数据段 | ❌ **只识别不解析**，返回 6 并提示命令 | ✅ `rime2csv.py` | `import_tables` 由脚本递归合并 |
| ibus `.db`（sqlite） | `SQLite format 3` | ❌ | ✅ `ibusdb2csv.py` | 设备端不链 sqlite |
| `.txt.bz2` | `BZh` 头 | ❌ | ✅ `bz2csv.py` | 设备端无 bzip2 |
| fcitx 4（`.mb` 任意形态 / `[CodeTable]`） | — | ❌ **永久不支持** | — | 私有二进制，不做反解析 |

「只识别、不解析」是刻意的：直接报「格式未知」会让用户以为文件损坏，明确告诉他跑哪条命令才能解决。

### 6.4 导入管线

```
源文件（CSV/TSV/TXT/fcitx5/ibus；Rime 先由 PC 端脚本转 CSV）
   │ ① 格式判定：二进制魔数 → 内容签名 → 扩展名 → 分隔符统计
   │ ② 流式解析 → 中间记录 {code, text, weight, type}
   │ ③ 归一化：编码小写、权重→rank、去重、按 (code, rank, text) 排序
   │ ④ 校验：字母表/码长/控制字符/行数上限；失败整体拒绝
   ▼
shape.dat v2（临时文件 → fsync → 原子 rename）
   │ ⑤ 写 Tables\<id> → 写 ActiveTable → Revision++
   ▼
（可选）广播 WM_SETTINGCHANGE("Software\\RosXingMa") 加速发现
```

容错与约束：

- 无表头时猜测列序；若「第一列不像编码、第二列像」自动对调（兼容「字在前、码在后」）。
- 编码嗅探：UTF-8（含 BOM）/ UTF-16LE/BE（BOM）/ GB18030（`MultiByteToWideChar(936)`），失败明确报错而不是乱码入库。
- 单行 ≤ 64 KB，总行 ≤ 200 万，有效条目为 0 → 拒绝。
- 非法行不静默丢弃：写 `reports\<id>.log`，界面展示前 20 条并可导出 `rejects.csv`。
- 反导出支持 CSV / fcitx 5 / ibus，保证数据可迁移。

### 6.5 可选数据包

数据包不是安装程序，而是**一个可被配置工具直接导入的目录**，自带预配置信息：

```
RosXingMa-Pack-Shape86-v1.0.0/
  pack.ini      预配置清单（UTF-8，INI 风格）
  table.csv     规范 CSV（构建期由上游固定 commit 转换）
  LICENSE       上游许可证全文
  NOTICE.md     署名、来源、上游权利提示
  SHA256SUMS    包内所有文件哈希
```

- 分发：整体打包为 `.zip`；**工具不解析 zip**，导入时选择解压后的目录或其中的 `pack.ini`（选择器接受两者，也支持目录拖放）。理由是避免在 ReactOS 侧引入解压实现，代价只是「先解压」一步。
- 清单按 `[Pack]` / `[License]` / `[Table]` / `[Install]` 四段声明标识、版本、许可、`DataSha256`、`EntryCountExpect`、`MaxCodeLen`、`Alphabet`、`Wildcard`、`ConsentRequired` 等，全字段见 `docs/DATA-PACK-SPEC.md`。
- **安装流程**：解析清单 → 校验路径（拒绝 `..` 与绝对路径）→ 校验 `SHA256SUMS` 与 `DataSha256` → 按声明值解析校验（与实际数据冲突即失败，**不静默覆盖**）→ 同意屏（显示名称、作者、许可、来源 URL、上游提示原文、「第三方数据不作权利担保」勾选）→ 写 dat 与注册表、留存 `LICENSE` → 按需设为当前表。
- **升级/冲突/卸载**：同 `PackId` 更高版本 → 显示「可升级」并保留用户改过的名称与排序；同版本 → 提示已安装；同内容不同 `PackId` → 提示可能重复但允许并存；卸载删 dat、注册表项与许可留存，若为当前表须先改选。
- 仓库只提交 `pack.ini.in`、`NOTICE.md`、`LICENSE`、`.gitignore`；数据文件由构建脚本生成且不入库。**仓库存模板而非成品清单**，是因为 `DataSha256` 与条目数只有构建时才知道，提前提交等于提交一个错误哈希。

---

## 7. 合规

授权尽调结论见 `docs/TABLE-LICENSE-RESEARCH.md`，此处只固化执行口径。

**法律定性**：形码编码方案相关发明专利均已届满；但**字根表与字码对照表受著作权保护**，专利到期不影响著作权。「五笔字型」在第 9/42 类未见有效注册，但有在审申请，产品名与界面避让。

**许可现状**：86 版上游已声明 MIT（源自极点表，权利链有残余风险）；98 版由表作者本人以 Unlicense 发布（权利链最清晰）；新世纪版无任何可用宽松授权。

| 轨道 | 内容 | 约束 |
|---|---|---|
| A. 默认 | 只含 MIT 代码、格式规范、工具；不含任何码表；首启合规提示 | 用户导入自有码表 |
| B. 可选数据包 | 86、98 各自单独下载、单独授权单元、单独署名 | 明示「第三方数据、不作权利担保」、可整体替换 |

**永不随包**：新世纪码表、字根键位图、助记口诀、原厂名称/图标/安装包资源、指向未授权码表的直链。

**产品行为红线**：不自动下载、不在安装向导「一键获取」、不做「检测到无码表正在下载」；导入器保持中立（不内置、不推荐、不提供下载入口）；**仓库内测试 fixture 必须全部自造**，绝不放真实码表片段。

**授权分层**（已在 `NOTICE.txt` 第 8 节固化）：MIT 覆盖代码与格式规范；数据包各自独立授权；用户导入数据不在 MIT 覆盖范围。

> 本节为工程级尽调结论，不构成法律意见。商业发布前须经执业律师复核「码表是否构成作品」的定性。

---

## 8. 实施计划

| 阶段 | 名称 | 关键任务 | 完成判据 | 人日 |
|---|---|---|---|---|
| **W0** | 立项与合规 | 双轨分发定稿；数据包署名与免责文本；「新世纪不分发」决策记录；命名与文案避让 | `docs/` 三份规范与 `NOTICE.txt` 第 8 节齐备，无待决法务项 | 1–2 |
| **W1** | 引擎与 dat 内核 | `shape_dat_format.hpp`、`ShapeTable`、`ShapeSession`；`libtableio` 的 CSV/TSV 读取与 `dat_writer`；`mkdat-shape`；自造样例表跑通全链路 | `ctest` 全绿；CSV→dat 两次构建字节一致；自造表可查表并在 host 测试中上屏（含词） | 12–16 |
| **W2** | 装配与宿主集成 | `rosxingma.ime` 装配（scheme_reg、def、资源）；`ShapeScheme`、`ShapeStore`、`ShapeDatLoader`（G1 已由框架解决，直接复用共享宿主层，无自建 `imm_entry_shape`）；自带安装器（`.ime` 进 system32、工具进程序文件夹）与 `HKLM` 安装信息 | VM 中可安装；无表不吞键不崩溃；导入演示表后可输入、可选词上屏；开始菜单能拉起配置工具 | 6–9 |
| **W3** | 配置工具与切换 | `rosxingma-config.exe`（列表/导入向导/预览/详情/移除/排序）、`rosxingma-table.exe`（全子命令）、注册表模型、revision 自检与延迟换表、LRU；数据包安装流程 | 导入多张表并来回切换；装数据包一次点击完成；组合中途切换不丢字；CLI 退出码矩阵符合规范 | 14–20 |
| **W4** | 格式兼容与完整度 | fcitx 5、ibus-table 适配器；PC 端 `rime2csv.py` / `ibusdb2csv.py` / `bz2csv.py`；反导出；`z` 通配；唯一候选自动上屏；标点与全半角 | 三类文本格式各导入一份真实表并正确上屏；Rime 文件得到明确引导；导出可被 fcitx 5 / ibus 回读 | 10–15 |
| **W5** | 验收与发布 | VM 端到端、CI 扩展、文档定稿、构建并发布 86/98 数据包、安装包与发布说明 | 主包无码表可安装；两份数据包可导入/卸载/溯源；全仓库无旧名残留 | 7–11 |
| | | | **合计** | **50–73 人日** |

> 人日为粗估，不含法务沟通等待。G1–G4 已全部由框架侧解决（2026-11-16，见 3.4），W2 不再含缺口浮动。

**阶段顺序依赖**：W1 完成前 W2 无法装配（缺 `ShapeTable`）；W4 的适配器依赖 W3 的 `libtableio` 稳定。

---

## 9. 工程规约

### 9.1 文件清单

**已就位**

```
xingma/XINGMA-PLAN.md                          # 本文件（正式版）
xingma/README.md                               # 目录导览与边界规则
xingma/archive/XINGMA-PLAN-v2.3-DRAFT.md       # 历史草案（存档）
xingma/docs/{TABLE-LICENSE-RESEARCH,TABLE-IMPORT-SPEC,DATA-PACK-SPEC}.md
xingma/packs/{README.md,shape86/*,shape98/*}   # 仅模板与许可
xingma/tools/mkdat-shape/make_pack.py          # 打包与校验
```

**待新增**

```
xingma/CMakeLists.txt                          # rosxingma.ime / 两个 exe / libtableio / tests
xingma/include/roxingma/shape_dat_format.hpp
xingma/include/roxingma/shape_table.hpp
xingma/include/roxingma/shape_session.hpp
xingma/include/roxingma/shape_store.hpp
xingma/include/roxingma/shape_scheme.hpp
xingma/src/{shape_table,shape_session,shape_store,shape_scheme}.cpp
xingma/host/scheme_reg_shape.cpp               # 定义 imfw::scheme_instance()
xingma/host/shape_dat_loader.{hpp,cpp}
xingma/host/shape_demo_table.hpp               # 自造演示编码表，仅自检/CI
xingma/host/rosxingma.def                      # 16 个 IMM32 导出
xingma/res/{resource.h,rosxingma.rc,rosxingma.ico}
xingma/tools/table-io/{CMakeLists.txt,format_detect.*,csv_reader.*,fcitx_reader.*,
                       ibus_reader.*,pack_reader.*,dat_writer.*,dat_reader.*,
                       report.*,utf.*}         # 无 rime_reader
xingma/tools/table-config/{CMakeLists.txt,main_gui.cpp,main_cli.cpp,ui_main.*,
                          ui_import.*,ui_pack.*,cli.cpp,registry_store.*,resource.h,app.rc}
xingma/tools/table-config/tests/{cli_test.py,fixtures/*}
xingma/tools/table-convert/{rime2csv.py,ibusdb2csv.py,bz2csv.py,
                            export_fcitx.py,export_ibus.py,README.md}
xingma/tools/mkdat-shape/{CMakeLists.txt,mkdat_shape.cpp,tables.toml,
                          fetch_tables.py,build_packs.sh}
xingma/tools/install/{CMakeLists.txt,install.cpp,resource.h,install.rc}   # 形码自带安装器
xingma/tools/{check_shape_dat.py,check_layout.py}
xingma/tests/{shape_table_test,shape_session_test,shape_switch_test,
              table_io_test,pack_test}.cpp + CMakeLists.txt
```

**对仓库其余部分的影响**：无。`xingma/CMakeLists.txt` 为独立顶层工程，安装器在 `xingma/tools/install/`；`framework/`、`pinyin/`、`ime/`、根 `CMakeLists.txt` 与根文档均不由形码改动。

### 9.2 测试

沿用「手写 `check()` + ctest」的零依赖风格；GUI 靠 CLI 与人工截图验收。

| 测试 | 内容 |
|---|---|
| `shape_table_test` | 非法 magic / 版本 / `max_code_len=0` / 字母表含非字母 / 偏移越界 / `Code` 非升序 → 全部拒绝；`lookup` 排序；`is_prefix` 边界（最长码、无后继）；词条目与单字条目共存 |
| `shape_session_test` | 逐码输入、最长前缀回退、达码长不再吃键、无候选达码长自动上屏原文、唯一候选自动上屏开关、退格、取消、通配键关闭态、**多字词 `commit_text` 完整性** |
| `shape_switch_test` | seqlock 读到不一致时重试、组合中切换不换表、组合结束后换表、LRU 淘汰后可重载、缺文件/损坏表的降级顺序 |
| `table_io_test` | 同一份**自造**数据写成 CSV/TSV/fcitx5/ibus 四种文本 → `(code,text,rank)` 集合完全一致；权重归一化；无表头列序猜测；GB18030 与 UTF-16 BOM；超长行与非法行进入 rejects；Rime 与 `.mb` 返回退出码 6 且给出命令提示 |
| `check_shape_dat.py` | 解码 dat v2、断言头字段、两次构建字节一致、Entry 区间连续且与 Code 对应 |
| `pack_test` | 必填项缺失 / `FormatVersion` 未知 / 绝对路径或 `..` / `DataSha256` 不符 → 拒绝；声明值与数据冲突 → 报错而非覆盖；条目数偏差 >1% → 警告；`SHA256SUMS` 缺项 → 报告 |
| `cli_test.py` | `rosxingma-table` 全子命令闭环；`pack install/verify/uninstall`；`--dry-run` 与真实导入一致；退出码矩阵 |
| VM 端到端（无表） | 安装 → 无表不吞键不崩溃 → 开始菜单拉起配置工具 → 空状态引导 |
| VM 端到端（数据包） | 解压 → 导入 86 包（同意屏 → 安装 → 自动成为当前表）→ 记事本输入 → 卸载 → 回到无表 → 导入 98 包重复 |
| VM 端到端（多表） | 导入 fcitx5/ibus/CSV 各一张 → 切换 → 输入/翻页/上屏 → 连续切换 50 次 |
| VM 端到端（安装布局） | 装到非系统盘 → `.ime` 在 system32、工具在程序文件夹 → 两处入口都能拉起工具 → 卸载后清空且用户数据保留 |

### 9.3 CI 检查

| 检查 | 内容 |
|---|---|
| 契约静态检查 | 不写 `Preload`、不硬编码 HKL、`DllMain` 不做事、配置工具不写 `system32`、除 `.ime` 外安装器不向 `system32` 写文件 |
| 目录边界 | `xingma/tools/check_layout.py`：`xingma/` 之外出现 `xingma/` 路径即失败；`xingma/` 内对 `framework/`、`pinyin/` 出现写操作或非 `<imfw/...>` 形式的 include 即失败 |
| 命名一致性 | 同脚本扫描旧名与旧路径，白名单仅四类事实引用 |
| 确定性 | `check_shape_dat.py` 与两次构建哈希比对；数据包构建产物 zip 字节一致（固定时间戳） |
| 回归 | 既有拼音 ctest 与 M1 契约检查全绿；关闭 `ROSXINGMA` 时拼音二进制行为不变 |

---

## 10. 验收标准

1. `rosxingma.ime` 可经形码自带安装器装到 ReactOS 0.4.16+，动态分配 HKL，不写 `Preload`；拼音安装器不受影响。
2. **无表可用**：全新安装无任何码表时启动不崩溃、按键不吞、界面给出明确引导。
3. **不限数量**：可导入 ≥5 张表（含不同码长 4/5），列表正确显示，可重命名、排序、移除。
4. **工具闭环**：GUI 与 CLI 均能完成 导入 → 预览 → 切换 → 导出 → 移除；`--dry-run` 无副作用；`rejects.csv` 可导出。
5. **格式兼容**：CSV、fcitx 5、ibus-table 各至少一份真实表导入成功并正确上屏；Rime 文件得到明确命令引导且转换后的 CSV 可导入；fcitx4 `.mb` 与 `.db`、`.bz2` 返回退出码 6；反导出可被 fcitx 5 / ibus 回读。
6. **词组能力**：多字词可从候选窗选中并**完整上屏**（G1 已解决，非截断）。
7. **数据包闭环**：86、98 两份各自可一次导入（清单 → 哈希 → 同意屏 → 安装），安装后自带名称/码长/字母表无需手填；可卸载、可升级；许可证与上游提示常驻可查；哈希不符拒绝安装。
8. **切换行为**：同窗口下一次按键生效；切回其它窗口输入时生效；组合中途切换不丢字、不残留候选、不崩溃；`SwitchMode=1` 时明确提示重启。
9. **降级可预期**：dat 缺失或损坏 → 标损坏 + 提示 + 保持原表，不静默错码。
10. 既有拼音 IME 的全部 ctest 与契约检查保持通过。
11. `NOTICE.txt` 明确区分「MIT 代码 / 可选数据包 / 用户导入数据」，仓库内无任何真实码表数据。
12. **命名统一**：全仓库无旧名残留，白名单仅四类事实引用，扫描全绿。
13. **安装布局**：`.ime` 在 `system32`，两个工具在 `%ProgramFiles%\RosXingMa\`；除 `.ime` 外 `system32` 无新增文件；卸载干净且默认保留用户码表。
14. **目录隔离**：形码专属文件全部在 `xingma/`；`framework/`、`pinyin/`、根构建脚本无因形码而起的改动；`xingma/README.md` 的目录表与实际一致。

---

## 11. 风险与缓解

| 风险 | 等级 | 缓解 |
|---|---|---|
| ~~G1 未解决导致词只能显示不能上屏~~ | ~~高~~（**已消解**） | G1 已由框架 `imfw::emit_text` 解决并有单测与 `check_m1.py` 断言（2026-11-16） |
| 热切换在个别宿主进程中不生效 | 中 | 两处自检（`on_select` / 组合起点）+ 组合期延迟换表；`SwitchMode=1` 提供确定性模式；文案不夸大 |
| IMM32 每进程独立，无法全局原子换表 | 高（已由设计消解） | 不做广播依赖；revision 自检；切换只在安全时机发生 |
| 配置工具在 ReactOS 上的控件兼容性 | 中 | 仅用 user32/comctl32 基础控件；W2 早测 `ListView`，必要时退 `ListBox`；不自绘 |
| 多格式解析的历史变体 | 中 | 解析宽松、校验严格；预览页展示统计与样例供肉眼确认；rejects 可导出 |
| 表多导致内存/启动开销 | 低 | 按需加载 + LRU 上限 2；启动只读注册表元数据，不读 dat |
| 用户导入侵权码表 | 低（责任转移） | 首启提示；导入要求填授权依据；工具中立；MIT 不覆盖用户数据 |
| 86 表上游权利瑕疵 | 中 | 仅作可选数据包、单独署名、不作担保、可替换；发布前律师复核 |
| dat 损坏导致静默错码 | 中 | 加载即全量校验；失败标损坏并保持原表，宁可「无表态」 |
| 装配引入拼音回归 | 中 | 形码独立 target；关闭开关时拼音二进制不变；改动前后跑全套 ctest |

---

## 12. 待决事项

| # | 事项 | 备选 | 影响 |
|---|---|---|---|
| Q1 | ~~G1 的责任方~~ | ~~框架侧扩展 / 形码自建宿主层~~ | **已决（2026-11-16）**：框架侧 `imfw::emit_text`，形码直接复用共享宿主层 |
| Q2 | 数据包下载渠道 | GitHub Releases 附件 / 独立站点 | 均不进主安装包，只影响发布位置与更新指引 |
| Q3 | 便携模式优先级 | W3 必做 / 延至 W5 | 影响工具参数与测试矩阵 |
| Q4 | ~~是否请求框架开放 `ImeConfigure` 回调~~ | ~~请求（体验最佳） / 维持开始菜单入口~~ | **已决（2026-11-16）**：框架已开放 `Scheme::configure`，形码 W2/W3 接入，开始菜单入口保留为冗余 |

---

## 附录 A 术语

| 术语 | 含义 |
|---|---|
| 形码 | 以「字母编码 → 汉字/词」为核心规则的输入方案，含字根式（形码）与笔画式 |
| 码表 / 表 | 一份完整的「编码 → 候选」数据集合，以 `shape.dat` 形式存放 |
| 表 id | 表的稳定标识，`[a-z0-9][a-z0-9_-]{0,31}`，用于文件与注册表配对 |
| 全码 / 简码 | 一个字的完整编码 / 可提前选中该字的较短编码 |
| 识别码 | 为区分同码字而附在末尾的补充码，本项目在导入期展开为独立编码 |
| 组合串 | 用户已输入但未上屏的编码串 |
| 上屏 | 把候选文本送入应用程序 |
| 数据包 | 带预配置清单的码表分发包，可被配置工具直接导入 |

## 附录 B 文档维护约定

- 本文件为唯一实施依据；章节编号稳定，新增内容追加到末尾或明确子节，避免打乱既有交叉引用。
- 版本号递增规则：结构性调整（目标、模型、阶段）升次版本；文字修订升修订号。
- 每次修订同步检查 `xingma/README.md`、`docs/*.md`、`NOTICE.txt` 中对本文件章节号的引用。
