# 变更日志

本项目版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。1.0 之前次版本号可能包含不兼容变更。
`alpha` 前缀的含义：**可在 ReactOS 0.4.16+ 上实机安装并输入单字**，但功能、数据与接口尚未定型。

---

## 未发布

### 取消 rare 概念：低权重入码表 + [快]/[全] 唯一由 tier 决定（2026-11-23）

- **决策**：按 owner 指令，不再在引擎里用 `show_rare` 藏无字频字，改为数据层直接分级——
  有字体覆盖（tier0）但无字频的字自动赋 rank `1,000,000`（低权重，真实 rank 上限 9,900），
  作为普通候选排在末尾；framework 与 pinyin 中 `show_rare`/rare 全部删除，
  `[快]/[全]` 是唯一展示策略，完全按 dat 的 tier 位判定。
- **根因回顾**：之前 zhong 段 108 条中 82 条无频序字被引擎 `show_rare_=false` 拦截，
  `[快]/[全]` 只在 ranked 26 条里过滤（22 vs 26，同为 3 页），所以页数与 Esc 重打都不变。
  这是独立于双路径 bug 的第三个缺陷。
- **数据层**：`tools/mkdat/mkdat.cpp` 给 tier0+无字频赋低权重；指纹
  `8f8cdb33…` → `db0c7bcf…`（1,080,224 B 不变，rank 槽改变；ranked 9,898→17,306，
  unknown 34,457→27,049，tier 分布 16,500/27,855 不变）。新表已用 `build_dat.sh --install` 装机。
- **引擎/框架**：删 `PinyinSession::show_rare_` 与 `set_show_rare()`，`recompute()` 不再分流；
  `renderable_candidates()` 的 fail-open 从「全不可见或过半被剔」收窄为「全不可见」——
  码表 tier1 本就占 27,855/44,355，过半规则会把豆腐块放回 `[快]`，使开关只剩标签差异。
- **验收数值**：zhong 段 `[快]`=40 条 5 页（原 3 页，因 18 个 tier0 无频序字进候选）、
  `[全]`=108 条 12 页；`Ctrl+Shift+G` 即时重算；G 不进拼音串。
- **产物**：`rospinyin.ime` = `ad9edb663ce167368d018034fd8d507092afe1425a06d9a6396a4840d897aa15`
  （950,668 B，BUILTIN_TABLE=OFF / TRACE=OFF）；`rospinyin.dat` 指纹见上（1,080,224 B）。
  两者均已部署到 HTTP 8000，下载校验 200。安装器的 size 校验是「源与目标比对」而非硬编码，
  故 ime 体积变化不需要重建安装器。
- **门禁**：红→绿 TDD（pinyin-session 2 断言 + host-key 2 断言 + host-smoke 2 断言）；
  ctest 双配置 18/18、check_m1 111/111、check_docs 7/7、check_mkdat 全绿后部署。

### 修复 Ctrl+Shift+G 双路径缺口：G 进拼音串 + 候选不即时重建（2026-11-23）

- **现象**：实机按 `Ctrl+Shift+G` 后状态条 `[快]→[全]` 正常翻转，但字母 `G` 被当成
  拼音输入（`zhong`→`zhongg`），候选列表与页数随之错乱（`- 3 /3`）。
- **根因**：IMM32 把一次按键走两遍——`ImeProcessKey → wants_key()` 认领 ModeToggle 并
  翻转模式；`ImeToAsciiEx → handle_key()` 没有 ModeToggle 分支，`route_key()` 把 `G` 当普通字母。
- **修复**：`handle_key()` 在 `route_key()` 前拦截 ModeToggle，不再 `feed('g')`；
  用 `ImeSessionState::raw_candidates`（新增，未过滤候选缓存）立即重建 pager 并刷新 UI，
  兑现 `FRAMEWORK-REQ.md` FR-5「切换后当前组合候选立即重建」。`finish_composition()` 清缓存。
- **验证**：`tests/host_key_test.cpp` 新增双路径断言（G 不进引擎、composition 不变、
  候选即时从 10 重建为 11、UI 即时刷新）；ctest 18/18、check_m1 111/111、check_docs 7/7 全绿。
- **附**：确认运行时字形探测链（`can_render`/`GlyphProbe`/`UiCanRender`/`GetGlyphIndicesW`）
  已全仓库零命中，本问题与探测残留无关。

### pinyin dat v2 分级落地 + framework 探测链移除（2026-11-20，已实现）

上一节留下的「tier 全 0」残留已关闭，并把 `FRAMEWORK-REQ.md` 的 FR-1~FR-6 一次落地：

- **数据层**：新增 `tools/mkdat/export_cmap.py` 与 `tools/mkdat/data/droid_cmap.txt`（29,622 码点，
  从 ReactOS 自带 `DroidSansFallback.ttf` 导出，源字体 sha256 写在文件头）。`mkdat` 新增必填
  `--cmap`，按码点集写 `CharEntry.flags` bit1；`build_dat.sh` 钉死 cmap sha256，产物指纹
  `3bf4d5fa…` → **`8f8cdb33…`**。44,355 字 / 418 音节不变，**tier0=16,500 / tier1=27,855**；
  有频序的 9,898 字里 9,092 是 tier0，首屏 500 字 96.8% 不变（tier 不进排序键）。
  用 `--install` 刷新了 `m0/share/rospinyin.dat`，避免两份副本分叉。
- **引擎**：`PinyinSession::recompute()` 从 `TableEntry.tier == 0` 填 `Candidate.glyph_visible`；
  rank 与 tier 是两个独立维度（常用字也可能需扩展字体，生僻字也可能可绘）。
- **framework**：`Host` 删 `can_render`、加 `show_all_glyphs`/`toggle_glyph_mode`；
  `renderable_candidates` 改按 `glyph_visible` 过滤（`[快]` 只留可见、`[全]` 原样放行、
  全组不可见或过半不可见时 fail-open）；`ImeSessionState.full_glyph` 为 per-HIMC 会话态，
  默认 `[快]`，不写注册表；`Ctrl+Shift+G` 翻转并即时刷新工具栏 `拼:开[快]` / `拼:开[全]`；
  彻底删除 `GlyphProbe` / `UiCanRender` / `GetGlyphIndicesW`（含语法桩声明）。
- **门禁**：ctest 18/18（`ROSPINYIN_BUILTIN_TABLE` ON/OFF 两配置各一遍）、
  check_m1 111/111、check_docs 7/7、mkdat-e2e（中= tier0 / 嗯= tier1 断言）、
  MinGW i686 交叉编译通过；全仓 `can_render|UiCanRender|GlyphProbe|GetGlyphIndicesW`
  grep 无代码残留。
- **边界**：FR-8「唯一可见候选自动上屏」的策略数据 `auto_commit` 属形码码表头，形码尚未开工、
  拼音头恒 0；本次只落实 framework 侧「判据必须作用在过滤后可见集」的契约，不新增上屏通道。

### dat v2 重做：单文件 + Header 80B + 不再兼容 v1（2026-11-20，已实现）

owner 三条指令（① 不考虑 v1 兼容 ② 调整 header ③ 补齐缺口、修正 bug）触发格式重做。
此前的 64B 两文件方案（`ROSPYLEX`）尚未落地过任何文件，因此原地替换，不另起版本号。

- **Header 52→80B**：新增 `entry_size`/`code_size` 自描述尺寸，以及词组两套表的
  `lex_entry_count`/`lex_code_count`/`lex_entry_table_off`/`lex_code_table_off`；
  四张定长表（字 / 音节段 / 词 / 词组键段）首尾相接。**词组并入同一文件，取消第三种 magic**。
- **v1 不再兼容**：`version != 2` 一律拒载，不做「按 tier 0 降级读」——v1 条目没有 tier 位，
  降级读等于骗用户。运行时后果是降级不是失败：外部表被拒即回退 `pinyin/src/scheme.cpp` 的
  内置数组（`kBuiltin[]` 是 C++ 数组，不经 dat）。**`.ime` 与 `.dat` 必须同源发布**。
- **补齐的校验**（每条都有对应用例）：音节/词组键段改为**前缀和 tiling** 并强制校验（原实现空音节
  `first_entry` 写 0，契约根本无法检查）；`rank == 0` 由静默接受改为拒载；`flags` 保留位由不查改为拒载；
  词组表「声明了就必须自洽」（无键、跳词、单音节词均拒）；偏移链改 **64 位算术**防回绕；
  `entry_size`/`code_size` 与记录宽度不符即拒（抓半升级文件）。
- **修正的 bug**：`tests/code_table_test.cpp` 原用 `kVersion + 1` 表示「未知版本」，而它就是下一个合法版本
  ——一旦该版本落地，用例会从「拒载」静默变成「解析」。改为 `0x7F7F`。
- **产物**：`rospinyin.dat` 902,776 → **1,080,224 B**（+177 KB），44,355 字 / 418 音节 / 118 多音 /
  首屏覆盖 96.8% 全部不变（容器变了、载荷没变）；`build_dat.sh` 指纹更新为 `3bf4d5fa…`，
  端到端重建（含原始输入 sha256）逐字节一致。
- **顺带修掉一个已被新格式放大的老坑**：`m0/share/rospinyin.dat` 与 `build/m2-data/` 两份副本一直靠手工
  `cp` 同步（M2-REMAINING / M2-REPORT 都登记过）。v2 落地当天它就真的发作了——`m0/share/` 还是 v1 表，
  v2-only 加载器会拒载并静默回退内置数组，VM 验收会看到"候选突然变少"却查不出原因。
  `build_dat.sh` 新增 `--install`（默认 `m0/share/`，只在指纹闸通过后拷贝），并已用它把该目录刷新为 v2。
- 影响面：`dat_format.hpp`（80B 契约、删 v1 结构与 `HeaderV2`）、`code_table.cpp`（v2-only 加载器）、
  `mkdat.cpp`（80B 头 + 前缀和段）、`code_table_test.cpp`、`check_mkdat.py`、`coverage_report.py`、
  `build_dat.sh`；文档 `DAT-FORMAT-V2.md` 全文重写，统一文本与 `XINGMA-PLAN.md` §4.1 同步为 80B。

### dat v2 七项裁决关闭 + 词组表同骨架方案（2026-11-20，规范 + 头文件契约，零行为变更）

> 本节记录当时的裁决过程；**Header 尺寸与词组承载方式已被上一节取代**，七项裁决结论仍有效。

- 上一版留下的 7 项待裁决**全部关闭**（`DAT-FORMAT-V2.md` §10 现为裁决记录表）：
  ① 共享校验按调用方传入的 magic 做白名单（拼音只认 `ROSPYDAT`，拒绝 `ROSSHDAT`）；
  ② `rank` 合法 = `≥ 1` 或 `== kUnknownRank`（34,457 条无频序字得以保留）；
  ③ 槽 4 归拼音 `alt_off`，不校验为 0（多音字 109 条次读音）；④ 拼音的形码专用 header 字段严格 0；
  ⑤ 词组沿用同一格式（见下）；⑥ v1 表全 `tier = 0` 的降级接受，发行包与 CI 同源换 v2；
  ⑦ 统一文本 §2.1 公式笔误改为 `64 + entry_count × 24`。
- 上位文本同步修订：`xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md` §5 第 1/6/7 条按裁决改写、
  §2.1 第 28 行公式就地修正、§7 标注「已执行」、新增 §10「拼音侧裁决回执」与表族 magic 注册表。
- 第五项（词组）结论：**保持相同格式不等于扩 Entry**——24B 六槽已够表达词条
  （槽0 文本池偏移 / 槽1 rank / 槽2 音节数 / 槽3 读音 / 槽4 保留 / 槽5 flags），
  缺的是「表类型标签」和「第二套键空间」，故取**同骨架 + 第三种 magic `ROSPYLEX` + 独立文件**
  `rospinyin-lexicon.dat`；扩 Header 到 72B 的真单文件方案被否（会连带形码侧返工）。
  新增 `DAT-FORMAT-V2.md` §11：槽语义表、Code 键按字节序二分、校验补充、
  体积量级（10 万词 ≈ 4.5 MB）、M4 落地时序。
- `framework/include/imfw/dat_format.hpp` 增加 `kMagicLexicon`、`LexEntryV2`(24B)、`LexCodeV2`(16B)
  与 static_assert；v1/v2 结构与加载器行为一字未动（无读取方）。

### 码表 dat 统一格式 v2 冻结（2026-11-20，规范 + 头文件契约，零行为变更）

> 本节为首次冻结记录；布局细节已被上方两节迭代取代（Header 52→64→80、词组从独立文件到同文件）。

- owner 裁定：接收 `xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md` 为**新版 dat 的唯一冻结版本**——
  拼音 `rospinyin.dat` 与形码 `shape.dat` 同骨架、分 magic（`ROSPYDAT` / `ROSSHDAT`）。
  拼音侧落地文本为新增的 `docs/plan/DAT-FORMAT-V2.md`：Header 64B、Entry 24B 六槽、
  `flags` bit1 = `tier`（**0 = Droid 保底可绘，1 = 需额外字体**）；两文冲突以统一文本为准。
- `framework/include/imfw/dat_format.hpp` 增加 v2 契约（`kVersionV2`、`HeaderV2` 64B、
  `CharEntryV2` 24B、`kFlagTier`），static_assert 钉住尺寸；**v1 结构与 `kVersion` 原样保留**，
  加载器行为不变（v2 读取随 F-T1 落地）。
- 拼音三条校验红线写入规范：① 拼音入口只认 `ROSPYDAT`（防止形码表被静默误读）；
  ② `rank == kUnknownRank` 合法（44,355 条里 34,457 条无频序）；③ 槽 4 是 `alt_off`，
  不得要求为 0（多音字次读音）。
- 文档口径同步：`PLAN.md` §12、`ROADMAP.md` §F、`M2-REMAINING.md` 的 tier 编号改为 0 = 保底可绘。
- 当时遗留待裁决 7 项，同日全部关闭，见上一节。

### F 里程碑计划变更：分级码表 + 全字形开关（2026-11-19，仅计划文档）

- owner 决策：摒弃「调用输入法时运行时字形探测」路线（原 F0 GDI 探针、F1 多字体探针、
  F4 过滤三态化、D-F1 风格决策），改为**构建期分级码表 + 用户全字形开关**：
  1. `.dat` 按 Droid Sans Fallback 的 cmap 覆盖分两级（tier 0 = 系统保底可绘；
     字段形态最终由 2026-11-20 统一格式冻结为 Entry `flags` bit1），
     mkdat 产出 dat v2；
  2. 输入法新增「全字形」开关：关闭态（默认）候选只含 tier 0、零方框由构建期保证；
     打开态显示全部（需字体支持）；工具栏 `拼:开/关` 旁新增 `快/全` 指示器；
     切换交互按实现难度先实现一种（首选 lone-Ctrl 热键），另一种归状态工具条；
  3. 安装器与发布说明提示「全字形模式需字体支持」；
  4. 开关不写注册表、不写文件、零持久化，IME 重选 / 切换输入法 / 切换上下文即重置回关闭态。
- 附带决策：`GlyphProbe`/`UiCanRender`/`renderable_candidates` 运行时探测链退役
  （X2-f 的 can-render seam 归零）；字体包（≈57 MB）保留但移出关键路径，服务打开开关的用户。
- 文档落地：`ROADMAP.md` §F 重写（F0→F-T1..T4）、`PLAN.md` §12 重写（8-12 → 5-7 人日）、
  `docs/font/` 五份评估文档加变更标注（选型结论仍指导字体包）、
  `M2-REPORT.md`/`M2-REMAINING.md` 的 P1-2 后续口径更新。
- 代码零改动；F0 不再是 M3 前置，M3 即刻解锁。

### X2 收口总验收

- 所有 X2 子项已合入 `main`，PLAN 清单无未勾选项；收口重跑 `ctest` 17/17、
  `check_m1` 109/109、`check_docs` 7/7、`git diff --check` 通过。
- `linux-ci` 与 `win32-cross` 两条 preset 路径均重建通过；两个独立交叉构建目录的
  `.ime` 逐字节相同（953,082 B，sha `f3e33b0a…`）。
- 对外面未变：16 导出名及顺序、5 个导入 DLL 及函数集逐条同序一致；w1/w2/w3 是行为变更，
  字节变化是预期，不是 ABI 回归。
- 真机观感三项（状态指示器、候选窗稳定性、翻页保持）仍待用户在 VM 中确认，不由绿灯代替。

### Seam 清单与调用点上限（X2-f）

- 新增 `tools/seams.py`：统计换前端时要动的 5 条 seam（`engine`、`scheme-policy`、`ui-exit`、
  `imm32-abi`、`can-render`），先剥注释再计数，避免把注释里的路径或名字当成调用点。
  当前计数全部等于上限：4/4、12/12、12/12、16/16、2/2。
- `check_m1.py` 直接 import `seams.py` 的 `over_cap()`，不再复述一套正则：任一 seam 越界即
  报错，数字只能随代码一起改。`check_m1` 108 → **109/109**，`ctest` 17/17、`check_docs` 7/7。
- 5 次变异（各 seam 制造一处越界）全部变红。只加工具与断言，不改运行时代码、不改导出/导入面、
  不改发布产物。

### 翻页状态合入 main（X2-w3）

- `CandidatePager::replace()` 与 `reset()` 分工：`Input/Absorb` 继续打字时用 `replace()` 保留
  当前页（页不存在时 clamp 到最后一页）；提交、Backspace、Esc 仍用 `reset()` 从第 1 页开始。
- `candidate_pager_test` 新增保留页 / clamp / 空候选三组；`host_smoke` 新增"翻到第 2 页后
  继续打字仍在第 2 页"的精确日志段。3 次变异全红。
- `ctest` 17/17、`check_m1` 107 → **108/108**、`check_docs` 7/7。
- 导出/导入面不变；`.ime` 952,485 → 953,082 B，sha `1ced1364…` → `f3e33b0a…`。真机待验。

### 候选窗稳定性合入 main（X2-w2）

- `PinyinSession::recompute()` 只给已完成音节发候选。新增 `completed_syllables()`：
  整串可切分照旧；尾随撇号保留前面的音节；否则取"最长可完整切分的头 + 尾部仍是合法前缀"。
  `nihao` 逐键候选数从 `0,4,0,4,4` 变为 `0,4,4,4,4`。
- **产品决策变更**：旧测试 `trailing separator has no candidates` 被改成"保留 `[xi]` 与「西」"，
  另加 `zhongguo` 无空档 / `zhongg` 保留「中」/ 上屏后尾巴 `g` 留下三组判据。
- 3 次行为变异全红；`ctest` 17/17、`check_m1` 107/107、`check_docs` 7/7。
- 导出/导入面不变；`.ime` 951,461 → 952,485 B，sha `0ac616f1…` → `1ced1364…`。真机待验。

### 状态指示器合入 main（X2-w1）

- 工具栏现在显示 `拼:开` / `拼:关`；托盘红底「中」跟 HKL 走，不随 Shift 变。首次绘制也带
  `fOpen`，键路径每键和 Shift 后各同步一次（`Ctrl+Space` 会绕过我们直接改 `fOpen`）。
- 与 X2-c/d/e 的冲突按"保留新架构、补新行为"解决：两条激活路径仍是 X2-d 的
  `activate_session()`；分支里 5 处 SDK 宏补丁不再带回（X2-e4 已归 stub）。
- `ctest` 16 → **17/17**（新增 `toolbar-text`）、`check_m1` 104 → **107/107**；3 次变异全红。
- 16 导出与 5 个 DLL 的导入函数集逐条同序不变；二进制大小 947,820 → 951,461 B，
  sha256 `b0529131…` → `0ac616f1…`——这是预期中的行为变更。真机同框待验。

### 时间戳格式器成为公共服务（X2-e6）

- `trace_stamp.h` 从 `framework/platform/win32/imm32/` 升格为
  `framework/include/imfw/trace_stamp.hpp`。`dllmain.cpp`、`trace_line.cpp` 与拼音数据源
  统一包含公共头；`kTracePath` 与 `put_uint` 仍只有一份实现，没有复制。
- `pinyin/CMakeLists.txt` 删掉指向宿主 imm32 目录的 include 路径。engine 通过
  `imfw-engine` 的 PUBLIC include 拿到公共头，跨层依赖归零。
- `check_m1` 100 → 104：公共头归属、engine 无平台 include、宿主调用者仍共享、
  私有头不复活。3 次变异全红；另做 1 次"写成注释"控制，确认断言匹配调用而非词。
- 对外面零改动：交叉构建 sha256 仍为 `b0529131…`；trace 版 971,833 B 且三条
  `dat ...` 诊断串仍在。

### 版本只有一个真值（X2-e5，达成 G7）

- `CMakeLists.txt` 不再写死 `project(VERSION 0.0.1)`：配置时从 `ime/version.h` 读取
  MAJOR/MINOR/PATCH 与版本字符串，`PROJECT_VERSION` 现在是 **0.0.2**。字符串与数字
  不一致直接 configure 失败，不再静默回退。
- `.ime` 资源原本就从 `version.h` 展开，未改；`check_release_version.py alpha0.0.2 --binary`
  解析出的 FILEVERSION 与字符串均与 tag 一致。
- `check_m1` 97 → 100：派生关系、禁止第二处 release tag / 裸 FILEVERSION、资源继续用宏。
  4 次变异全红。Markdown 历史叙述与两个发版脚本中的示例字面量是显式白名单，不是漏扫。
- 对外面零改动：交叉构建产物 sha256 仍为 `b0529131…`。

### 平台补丁归位：生产代码不再猜 Win32 常量（X2-e4）

- 五个 UI TU 中 **47 处** `#ifndef <Win32常量> / #define ... / #endif` 全部删除（审计只点名了
  其中 30 处）。真实 `<windows.h>` 一直提供这些宏，补丁在发布构建里从未生效；它们只是
  Linux 宿主语法检查的拐杖。
- `tools/syntax_stub/windows.h` 补齐 25 个窗口样式、消息与 GDI 度量宏。stub 的值仅用于
  语法检查，不进产品；真实值由 MinGW SDK 提供。
- `check_m1` 94 → 97：生产侧结构断言、stub 表面覆盖断言、CI syntax check 覆盖断言；
  3 次变异全红。
- 交叉构建前后 `.ime` 的 sha256 完全相同（`b0529131…`）——补丁本来就是死代码，这是最硬的
  "行为不变"证据。

### 构建配置收敛：presets 是唯一出处（X2-e3）

- **`CMakePresets.json`（新）**：把 CI 与本地反复复述的 `-D` 列表写成 6 个 configure preset、
  6 个 build preset 和 1 个 test preset。CI 现在只写 `cmake --preset linux-ci` /
  `cmake --preset win32-mingw` / `cmake --preset win32-intree` / `cmake --preset win32-toy`；
  `check_m1` 断言这些名字仍在，避免工作流改了 preset 却没人改断言、最后什么都没测。
- **缺目录改为硬失败**：根 `CMakeLists.txt` 的 5 处 `EXISTS` 守卫全部删掉。原来 `tools/rhash`
  目录消失也能 configure 成功，只是产物少一个 exe；现在 `add_subdirectory` 直接报 configure 错误。
  这与 X2-e2 把 weak 兜底编译掉是同一种修法：缺东西必须当场说。
- **统一根变量与 C 标准**：子目录一律 `PROJECT_SOURCE_DIR`；显式 `CMAKE_C_STANDARD 11` +
  `REQUIRED` + `EXTENSIONS OFF`，C 工具不再跟随编译器默认值漂移。
- **`cmake/StaticRuntime.cmake`（新）**：`rospinyin_static_runtime()` /
  `rospinyin_reproducible_pe()` 两个函数集中静态运行时与无 PE 时间戳策略。`.ime`、安装器、
  `rhash.exe`、`wget.exe`/`curl.exe` 全部按调用申请；此前只有 `.ime` 有可复现链接，
  而 `rhash.exe` 正是回答"装的是不是这一版"的工具，它自己却带时间戳。
- **`check_m1` 87 → 94 项**：7 条新断言全部按"匹配调用"写；7 次变异逐个变红。
- 判据：16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序完全一致；toy/trace/in-tree
  三种形态构建通过；两个 Ninja 目录同源码产物逐字节相同。可复现性**不跨生成器**——
  Ninja 与 Makefiles 的链接命令行不同，同源码同大小但 sha 不同，这是边界不是缺陷。

### 数据源归位，缺席即链接错误（X2-e2）

- **`pinyin/host/dat_source_win.cpp` 归 `rospinyin-engine`**（Windows 分支），不再由 `ime/CMakeLists.txt`
  列出来编译。它是拼音方案自己的数据来源，不是装配层的细节。
- **weak 兜底在 Windows 上被编译掉**（`ROSPINYIN_REQUIRE_SYSTEM_TABLE`）：`pinyin/src/dat_source.cpp`
  里那个 `return nullptr` 的 `load_system_table()` 只保留给 Linux 与宿主测试。于是"忘了编真实数据源"
  从**静默降级**变成**链接失败**——实测改动前删掉那一行仍能编出 942,989 B 的 `.ime`（正常 948,481 B），
  装上去只是候选变少、多音字消失，没有任何提示。
- **`ime/CMakeLists.txt` 清理**：连同 X2-e1 留下的临时 include 路径、以及 `if(ROSPINYIN_TRACE)
  target_compile_definitions(rospinyin-ime …)` 一并删除——该目标里已没有读这个宏的 TU，
  留着就是"看起来生效、实际没改一个字节"的开关。
- **`check_m1` 83 → 87 项**：数据源归属（匹配 `target_sources(...)` 这个调用，不匹配路径词）/
  Windows 必须有 `ROSPINYIN_REQUIRE_SYSTEM_TABLE` / 守卫只包住兜底 / 装配目标不再声明 trace。
  变异 4 次全红（含"把宏改成注释"——词还在、调用没了）。
- 对外面：16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序仍完全一致；函数符号 1386 个两边逐个相同。
  i686 拼音 `948,481 → 947,820 B`（−661，全在节对齐）、trace 版 `971,982 → 971,833 B`、玩具不变。
- 遗留：`dat_source_win.cpp` 仍引 `trace_stamp.h`，`rospinyin-engine` 因此带一条指向
  `framework/platform/win32/imm32` 的 include 路径；由 X2-e6 收掉。

### 平台层成为两个库（X2-e1）

- **`framework/platform/win32/CMakeLists.txt`（新）**：`imfw-win32-imm32`（宿主 7 个 TU）与
  `imfw-win32-ui`（窗口 5 个 TU）两个静态库。此前"什么属于宿主层"只存在于 `ime/CMakeLists.txt`
  那张 12 行源码清单里。
- **`ime/CMakeLists.txt` 只剩装配**：scheme 源码 + `rospinyin.def` + `rospinyin_res.rc`，
  一行平台源码都不写。拼音与玩具方案走同一份装配逻辑。
- **两个库互为依赖**（宿主调 `Ui*`，窗口层读 `imfw::g_hInst`）。CMake 允许 STATIC 成环，两条边都写出来；
  **没有**造第三个 "common" 库去把环藏起来。
- **`.def` 不会拉档案成员**：GNU ld 只为未定义引用拉静态库成员，而 16 个导出名是导出条目不是引用，
  首次链接直接 `cannot export ImeProcessKey: symbol not defined` × 16。给宿主库套
  `-Wl,--whole-archive` / `--no-whole-archive`，并写成断言（删掉即红）。
- **`option(ROSPINYIN_TRACE)` 上提到顶层 `CMakeLists.txt`**：读它的是平台库，而 `framework/`
  在 `ime/` 之前配置——留在原处会让 `-DROSPINYIN_TRACE=ON` 只影响装配件，且**无声**。
- **`.ime` 现在可复现**：加 `-Wl,--no-insert-timestamp`。在此之前 ld 把链接时刻写进 PE 头，
  两个构建目录编同一份源码得到**同为 948,481 B 但 sha256 不同**的产物（只差 6 字节：
  `TimeDateStamp`、`OptionalHeader.Checksum` 及 2 个镜像字节）。"同一个哈希"因此只能回答
  "是不是同一个文件"，回答不了"是不是同一次构建"——这正是 `artifact_hashes.py` / `rhash.exe` 要回答的。
  现在两个目录的产物逐字节相同（`134f7fd4…`）。
- **`check_m1` 78 → 83 项**：每个平台 TU 只属一个库（双向 + 先剥 CMake 注释）/ `ime/` 不许出现
  `../framework/platform/win32/` / whole-archive 紧贴宿主库 / trace 宏的声明位置 / 可复现链接。
  变异 4 次全红。
- 对外面：16 导出名及顺序、5 个 DLL 依赖、每 DLL 导入函数集**逐条同序**一致。
  i686 拼音 `947,969 → 948,481 B`（+512，一个文件对齐块）；trace 版 `971,982 B` 不变；玩具 `+1,333 B`。
- 顺带修 HANDOVER §4 目录树：`framework/host/`、`framework/ui/` 是 X1 之前的布局。

### 拆 god object（X2-d）

- **`imm_entry.cpp` 613 → 314 行**，按职责拆出三个 TU（`imfw` 宿主层现在是一张能一眼看完的地图）：
  - `key_translate.cpp`（103 行）：vk → `KeyEvent`（key-up 位）、`KeyResult` → `TRANSMSGLIST`
    （`WM_IME_CHAR` 容量与 `TRANSMSGLIST` 偏移规则）。**不再叫 `emit_text`**：它只发射 commit，
    与 `imfw::emit_text` 同名会让读者以为两者是一回事。
  - `win32_host.cpp`（92 行）：`Win32Host` —— 键策略的 Win32 半边，**整个平台层唯一实现 `imfw::Host` 的地方**。
  - `trace_line.cpp`（117 行）：`ROSPINYIN_TRACE` 通道。`#if` 开关换成 `IMFW_TRACE_ON`，
    插桩代码不再自己写 `#if defined(ROSPINYIN_TRACE)`。
- **两条激活路径合成 `activate_session(hIMC)`**：`ImeSelect(TRUE)` 与 `ImeSetActiveContext(TRUE)`
  原本做同样三步但**顺序相反**（U1"首次激活是关的"就是这么来的）。顺序固定为
  对齐 `fOpen` → `on_select(true)` → 工具条。**解除激活没有合并**（一个要销毁会话、一个只隐藏窗口，
  合并是改行为）。
- **trace 的两份手写十进制转换合并**到 `trace_stamp.h` 的 `put_uint()`，并给它补 `cap` 参数，
  让"截断而不是越界"可证。
- **`check_m1` 71 → 77 项**：拆 TU 让一多半 `... in entry` 断言失效（不是行为变了，是文件里没有了）。
  改法不是"拼 4 个 TU 继续匹配"，而是**按归属双向钉**：该有它的 TU 必须有，另一个必须没有。
- 平台层总数 **2511 → 2689 行（+178）**：四个文件各自的许可证头与 namespace 壳。
  `imm_entry.cpp` 少了 299 行，别只看这一个数字。
- 对外面：16 导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；i686 `+821 B`。
- **真机已验**（2026-11-19）：`dir` = 947,969 B，记事本 `nihao` → 9 条候选、页码 `1/4`、无残窗，
  选字 / 翻页 / Backspace / Esc / Shift 逐项正常。证据 `m0/vm/evidence/x2d/`。
- 注意：之后 X2-e1/e2 只动构建组织，尚未上机（共享目录备有 `rospinyin-e2.ime`）。

### 键路由提取为纯函数（X2-c）

- **新增 `framework/include/imfw/intent.hpp`（75 行）+ `framework/src/key_route.cpp`（70 行）**：
  `Intent route_key(const Scheme&, const KeyEvent&)` —— 一个键翻译成一个 `Intent`
  （`None / Input{ch} / Select{slot} / PagePrev / PageNext / Backspace / Escape`），
  不碰 session / engine / host，也不看 `key_up`/`ctrl`/`alt`/`fOpen`（那是归属判定，归 `wants_key`）。
- **`handle_key` 变成 `switch (intent.kind)`**，并且不再触碰 `scheme`：两个策略问题只问一次，在路由里问。
- **两处与计划不同**：`SelectDigit(n)` 改名 `Select{slot}`（空格不选数字却选第 1 个，旧名会让名字撒谎）；
  `ToggleLang` **删除**（Shift 那条路在 `wants_key`，是归属判定；一个只有"永不 dispatch 的那一层"
  才能产出的意图是陷阱）。
- **新增 `tests/key_route_test.cpp`（22 条断言，`ctest` 16/16）**：每个 Intent + payload、
  "键由方案决定"（`q`/`j`/`k` 的自定义方案，`1` 对它无意义）、分支顺序（同一键被两处声明时选择赢）。
- **判据没动**：`host_smoke` 的 13 条日志一行未改——搬判定时最容易顺手改的就是分支顺序。
- `check_m1` **71/71**（+3；G4 那条从 `host_key.cpp` 改指 `key_route.cpp`）；变异 6 次全红。
  其中一次暴露了我自己新断言的假绿（`"scheme."` 匹配不到 `scheme_instance().`），
  改成"不许有任何策略调用 + `scheme_instance()` 只许一次"。
- 对外面：16 导出 / 5 依赖 / 各自导入函数集**逐个一致**；i686 `946,299 → 947,148 B`（+849）。

### 状态指示器：Shift 切状态，不切布局（2026-11-19）— 已合入，见上方 X2-w1

> **状态：已实现并验证，`main` 上没有。** 在分支 `wip/status-indicator`；按用户"和候选窗补丁
> 一起排期、先把 X2 做完"的决定，排入 PLAN-MODULARIZATION §3「X2-w」，顺序 w1 → w2 → w3。
> 下面记的是那份改动的实测数字，不是 `main` 的。

- **语义澄清**：托盘红底「中」跟键盘布局（HKL）走，Shift 只改 `fOpen` → 它**不该变**，此前
  "托盘不跟随"的记录方向有误。状态由我们自己的工具栏显示：`拼:开` / `拼:关`。
- **`Scheme::toolbar_state_label(bool)`**（默认 `nullptr` = 无状态，四个假方案零改动）：
  拼音 `开/关`，玩具 `on/off`。
- **`framework/src/toolbar_text.cpp`（新，可移植）**：`拼` + `:` + 状态。提到可移植层是为了能测——
  窗口要 GDI，字符串不用。新增 `tests/toolbar_text_test.cpp`（`ctest` **16/16**）。
- **`toolbar.cpp`**：宽度改为实测（原来写死 44 px 会把 `拼:开` 裁掉），右边缘固定向左生长；
  文本相同且可见时直接返回，避免每键一次 `InvalidateRect`。
- **`imm_entry.cpp`**：每个键在 `ImeProcessKey` 里拉一次状态（bool 比较），Shift 手势后立刻再拉
  一次。用"拉"而不是给 `Host` 加钩子，是因为 imm32 的 Ctrl+Space 直接改 `fOpen`、根本不进我们 DLL。
- `check_m1` **71/71**（+3：标题必须由 `toolbar_text` 生成且旧直传必须消失、两处同步计数、
  布局切换函数全仓库 0 处）；变异 5 次全红；16 导出 / 5 依赖 / 各自导入函数集全部一致；
  i686 `949,940 B`（+3,641）。
- 真机待验：一次 Shift 后 `拼:开`→`拼:关` 与托盘「中」不变需同框确认。

### 宿主冒烟（X2-b）：一段固定键序 + 一列精确的宿主调用

- **新增 `tests/host_smoke_test.cpp`**（463 行，`ctest` 15/15）：`zhongguo` → `2` → `Backspace`×2 →
  `Esc` → 单独 `Shift`，断言 **13 条 host 调用逐字相等** + commit 序列 `["钟"]` + 引擎收到的操作序列。
- 假件：`ScriptedEngine`（固定返回队列，空了即失败）、`RecordingHost`（一次调用一行日志）、
  `SmokeScheme`（字母表 + 默认选字/翻页键）。驱动层复刻 IMM32 的门：`wants_key` 为真才调
  `handle_key`。
- 断言对象是**调用序列**而不是最终文本——X2-c/X2-d 的验收都是"序列不变"，只比文本查不出
  重排 `show`/`hide`、漏刷 UI、多调引擎这三类回归。
- `check_m1` 68/68：新增"冒烟文件必须存在**且**注册进 ctest"（只存在不注册的测试看起来像覆盖）。
- 真机 Shift 复测：`abc` 直接上屏、`ni` 候选回来 → 切换生效；托盘红底「中」是 ReactOS 通知区的
  布局指示器，不反映 `fOpen`，已记入待排期（让我们自己的工具栏显示 拼/En）。

### 键策略剥离（X2-a）：最忙的 90 行第一次有宿主测试

- **新增 `framework/src/host_key.cpp`（进 `imfw-engine`）**：`handle_key()` 与 `wants_key()`
  是原 `process_key()` / `should_consume_key()` 的逐条搬运。判定不再需要 `<windows.h>`。
- **新增 `framework/include/imfw/host.hpp`**：`KeyEvent`（vk / ch / kind / key_up / ctrl / alt）、
  `KeyResult`（commit）、`Host`（open 读写、show/hide、can_render、2 个默认空诊断钩子）。
  `KeyKind` 让策略里不出现任何 `VK_*` 常量。
- **`ImeSessionState` 搬到 `framework/include/imfw/session_state.hpp`**：它本来就全是可移植
  类型，之前只是和 IMM32 的 store 放在一起，那也是键路径无法脱离 windows.h 的原因之一。
- **`imm_entry.cpp` 只剩适配器**（783 → 609 行）：`key_event_from()` 做 vk→kind/字符翻译，
  `Win32Host` 实现 `Host`。
- **行为不变**：分支与副作用顺序逐条核对，无判定改动；判据换成"PE 对外面不变 + 宿主测试"，
  因为代码换 TU 后字节必然变（拼音版 946,299 B，+4,207 B；16 导出、导入函数表逐个一致）。
- **测试**：新增 `tests/host_key_test.cpp`（35 条断言，假 engine + 假 host），`ctest` **14/14**；
  `check_m1` **67/67**；两次踩到"关键词出现即通过"的假绿，断言改为钉完整调用形式。

### 注入缝（X2-a0）：scheme 可替换，框架不动

- **`imfw::SchemeRegistry`**（`framework/include/imfw/scheme_registry.hpp`）：把 `scheme_instance()`
  拆成"创建"（装配件提供 `scheme_factory()`）与"查询"（框架 `get()`，结果缓存一次）两半；
  `install(Scheme*)` 是测试/宿主的注入点。`scheme_instance()` 保留为别名，**10 处调用点零改动**。
  解决审计 A-P0-2：此前换一个方案只能换掉框架所依赖的那个符号再重编。
- **toy scheme**（`samples/toy-scheme/`）：字母表 `a-z`、每键两个候选、零数据纯 ASCII，
  作注入缝首个消费者；`-DROSPINYIN_SCHEME=toy` 编出玩具版 `rospinyin.ime`（870,475 B，
  不可发布，configure 时有明确警告）。该提交 `framework/` 零 diff——G4 判据提前达成，
  X3 只剩打包与文档。
- **门禁**：`check_m1` 65 项（装配件断言改为查定义形式，堵掉"注释里出现关键词即通过"的假绿）；
  CI 语法检查覆盖玩具路径，MinGW job 多一个 `build-toy` 并断言两个 `.ime` 不是同一份字节。
  `ctest` 13/13（新增 `scheme-registry`、`toy-scheme`）。

### 架构审计复核与 X 计划扩容

- **新增 `docs/reviews/ARCH-AUDIT-VERIFICATION.md`**：对一份外部架构审计（13 条问题结论）逐条回代码取证。
  结论：**11 条成立、1 条成立但证据过期、1 条已过时**；量化数字 12 项中 10 项精确。
  补了审计漏掉的 3 条（两条激活路径顺序相反、计划自身基线数字失真、`ime/version.h` 是第四处版本声明）。
- **X1–X4 计划扩容**（`docs/plan/PLAN-MODULARIZATION.md`）：
  - X2 从"剥键路由 + 冒烟"扩为"安全网 + 构建收敛"，工期 3-5 天 → **6-8 天**；
    新增注入缝（`SchemeRegistry` + toy scheme，原属 X3）、拆 god object、库目标、presets、版本收敛。
  - X3 从"造玩具方案"降级为"接入演练 + per-instance 隔离"。
  - ROADMAP 的 X 表与总工期同步：v0.1 推迟约 1 周 → **约 1.5 周**。
- **一处顺序修正（本次唯一改了审计建议顺序的地方）**：审计把"Win32 层独立成库 + presets"排在
  冒烟测试之后是正确的，原计划想放 X1 之后是错的——**X1 的验收判据是字节比对，拆库后字节必然变，
  判据当场失效**，无法区分"拆库导致的差异"和"拆错了导致的差异"。所以构建收敛必须在 `host_smoke`
  之后，用行为判据验收。

### 结构调整（X1 目录归位）

- **`framework/host/` → `framework/platform/win32/imm32/`（9 个文件）、`framework/ui/` →
  `framework/platform/win32/ui/`（10 个文件）**，全部 `git mv`，零逻辑变更。
  目录名陈述事实（这段代码就是 Win32 + IMM32），不用 `frontend/` 那种预设抽象的名字。
- **验收看字节而非测试**：同工具链干净构建前后各一次，`.ime` 940,888 B 完全一致，
  仅 3 个区域不同（`0x88` 时间戳、`0xD8` 校验和、`0x60A04` 导出表时间戳），全部是链接器戳。
- **新增 `tools/compare_ime.py`**：按 PE 结构（数据目录 #0 / #6）定位链接器戳字段，
  不硬编码偏移；判据本身做过 4 次变异验证（翻转代码字节、截断、交换源文件顺序、不动）。
- **修正计划中的技术错误**：`0x60A04` 不是"调试字段"——该镜像没有调试目录，
  它是 `IMAGE_EXPORT_DIRECTORY.TimeDateStamp`。
- **修复一处长期失效的 CI 门禁**：`IME source syntax check` 因 `tools/syntax_stub/imm.h`
  缺 `HIMC`/`ImmGetOpenStatus`/`ImmSetOpenStatus` 一直失败，挡住 engine-smoke 后续全部步骤，
  并使 `rospinyin-m2-data` artifact 从未上传、`publish-artifacts-branch` 永久连带失败。
  补全 stub 后恢复（`windows.h` 补 15 个符号），并做了 3 次变异验证。

### 仓库结构

- **文档全部收进 `docs/`，根目录只留 `README.md`（+ `LICENSE`、`NOTICE.txt`、`CMakeLists.txt`）**。
  16 个根目录 `.md` 按「谁在什么时候读」分为六类：`plan/`（每轮必读的活文档）、
  `handover/`（交接与变更登记）、`milestones/`（阶段报告，只读）、`font/`（字体与字形专题）、
  `reviews/`（跨切面评估）、`releases/`（发版说明，机器读取）。`docs/README.md` 是唯一索引。
- **新增门禁 `tools/check_docs.py`（7 项，已接入 CI）**：根目录不得有多余 markdown、
  `docs/` 下每个文档必须登记在索引里、相对链接必须可解析、链接不得闭合两次、
  `docs/releases/` 命名须匹配 tag、子系统文档须贴着代码。整理若没有门禁，
  下一份报告还会落在 `README.md` 旁边。
- 引用同步：14 条 markdown 链接按新位置重算、59 处行内路径加 `docs/` 前缀；
  `tools/check_release_version.py` 与 CI 的发布 job 改读 `docs/releases/<tag>.md`。

### 发版工程化

- **版本号收敛到单一真相源**：新增 `ime/version.h`，`rospinyin_res.rc` 从这里展开
  `FILEVERSION` / `FileVersion` / `ProductVersion`。此前资源里写死 `0, 0, 1, 0` 与
  `"alpha0.0.1"`，**`alpha0.0.2` 发布后仍如此**——文件属性显示的版本是错的，
  而"发布的二进制 = 该 tag 的构建"当时无法检验。
- **新增版本门禁** `tools/check_release_version.py <tag> [--binary X.ime]`：校验
  tag ↔ `ime/version.h` ↔ **产物 PE 里的 `FILEVERSION`**（自行解析 `.rsrc`，无第三方依赖），
  并要求 `docs/releases/<tag>.md` 已存在。反例实测可判红（拿 rc 仍是 0.0.1 的旧产物跑，
  输出 `binary=0.0.1.0`）。
- **CI 由 tag 触发发版**（P2-1）：`on.push.tags: ['alpha*','beta*']` → `release` job，
  `needs: [engine-smoke, windows-build]`，用**本次运行的产物**发布 `.ime` / `.dat` /
  `install-rospinyin.exe` / `INFO.txt` / `SHA256SUMS.txt`，发布后把资产下载回来
  `sha256sum -c`。手工上传不再是路径；`tools/make_release.py` 降为兜底。
- 顺带核实：按 CI 步骤本地装配一遍，`.dat` 的 sha256 与 `alpha0.0.2` 已发布资产一致，
  说明 CI 从固定源编译的数据表可重现发布物。

### 门禁

- **契约断言从「黑名单扫词」改为「调用白名单」**（`tools/check_m1.py`，59 → 63 项）：
  旧的 `installer never writes Preload` 扫的是源码里的 `"Preload"` 一词，
  而写注册表的是平台；把写入写成 `L"Keyboard Layout\\Pre" L"load"` 即可绕过（实测旧断言 0 命中）。
  现改为 5 条结构断言：`DLL_PROCESS_ATTACH` 调用名白名单、类注册只调 `RegisterClassExW`、
  `ImeInquire` 有拒绝分支、安装器注册表写入限于两键白名单、注册表路径必须由
  `kKeyboardLayouts` 拼出。每条都做过反向验证（改坏必 FAIL）。

### 前提变更（文档层面，不改运行时代码）

- **M0 那次 `0xF4` 的归因被推翻，红线 #2 降级为部署偏好**。立项时把崩溃归因为
  「`Preload` 预载第三方 IME ⇒ 确定性蓝屏」，并据此立下「绝不写 `Preload` / 绝不注册为默认 IME」。
  2026-11-18 的取证否掉了这个归因：
  - 写 `Preload` 的是平台 `ImmInstallIMEW` 内部的 `Imm32WriteImeLayout()`（ReactOS 0.4.16
    `win32ss/user/imm32/install.c`），**没有 opt-out**，我们写不写不由自己决定；
  - 登录路径的加载者是 `userinit.exe`（`SetUserSettings → UpdatePerUserSystemParameters →
    IntLoadPreloadKeyboardLayouts → LoadKeyboardLayoutW`），不是 csrss；
  - 两组对照（各 3 次启动，均用本版发布的 940,601 二进制）：**N 组**（在 `Preload` 里但非首项）3/3 活、
    **N′ 组**（首项 = 默认输入语言，走 `KLF_ACTIVATE → ImeSelect`）3/3 活。
  → 风险在「登录路径上执行可能失败的初始化」，不在「被 `Preload` 载入」。`DllMain` 零工作与
  `ImeInquire` 可拒绝两条**保留**，理由改为健壮性（对 liberNT / x64 / 别的 imm32 实现仍必要）。
- 受影响文档：`README.md`、`M0-REPORT.md`、`M1-REPORT.md`、`HANDOVER.md`（新增 §11.18）、
  `ROADMAP.md`、`PLAN.md`、`libernt/PORTABILITY-ASSESSMENT.md`、
  `PORTABILITY-UPSTREAM-64BIT-ARM64.md`（P1-D → P2-D）、`m0/vm/PRELOAD-PROBE.md`。
  全部为**事实与理由的修订**，不涉及二进制行为；`alpha0.0.2` 发布物不受影响。

### 修复

- **首次切到本 IME 是「关闭」态**（真机反馈 U1）：`AlignOpenStatus` 原先只挂在
  `ImeSetActiveContext` 上，而 ReactOS 首次切到一个布局时可能只调 `ImeSelect`，
  于是 `ImmGetOpenStatus()` 停在 `IntCreateInputContext` 设的 `FALSE`——托盘显示 `EN`，
  用户按第一下 Shift 是「打开输入法」而不是「切中英」。现在两个入口都对齐，
  仍由 `open_initialized` 保证整会话只对齐一次（否则会覆盖用户自己按过的 Shift）。
- **工具条被任务栏挡住**（真机反馈 U3）：`dock_bottom_right()` 用 `SM_CYSCREEN` 全屏高定位，
  600 高的屏上算出 `y=570`，而任务栏顶边约 572，22px 的工具条有 20px 落在任务栏之下。
  改用 `SPI_GETWORKAREA` 的工作区矩形；任务栏在侧边或自动隐藏时也随之正确。
  附带一条实测结论：**ReactOS 上 `HWND_TOPMOST` 不保证压过任务栏**（任务栏同为 topmost
  且 z 序更高），所以只能靠避让，不能靠层级。

### 工程

- `tools/check_m1.py` 新增 2 条契约断言（57 → 59）：`AlignOpenStatus` 必须同时挂在
  `ImeSelect` 与 `ImeSetActiveContext`；工具条必须用 `SPI_GETWORKAREA` 而非 `SM_CYSCREEN` 定位。
- `m0/vm/probe-n.sh` 补上文档承诺却一直没实现的 `stop` 子命令（走 HMP `quit` 优雅退出，
  不 `kill`，保证覆盖层 flush），并把 `clean` 作为 `rollback` 的别名。

---

## alpha0.0.2 — 2026-11-18

第二个公开版本。M2 验收单 **15/15 通过**；真机三轮反馈（多音节只转首字、Shift 不切中英、
候选窗跟着上屏的字往右爬、汉字字距重叠）全部修掉。

### 下载

| 文件 | 大小 | sha256 |
|---|---|---|
| `rospinyin.ime` | 940,601 B | `c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa` |
| `rospinyin.dat` | 902,776 B | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
| `SHA256SUMS.txt` | 160 B | — |

> `rospinyin.ime` 由本 tag 的源码以 `ROSPINYIN_TRACE=OFF` 构建（MinGW i686，Release）。
> 真机验收用的是同源码的 `ROSPINYIN_TRACE=ON` 构建（967,775 B）——两者导出表（16 个 IMM32 接口）、
> 导入 DLL、功能路径都相同，只差一份写到 `C:\rospinyin-trace.log` 的诊断日志。

### 新增

- **`tools/coverage_report.py`**：算出 ROADMAP 里那条一直没人算的 M2 退出标准——
  **前 500 高频字首页覆盖率**。直接读 `.dat`，按引擎真实排序（`1000000 - rank`，
  同音节内 rank 升序）计算，不重新实现排序。实测 **96.8%（484/500，达标 ≥95%）**，
  但**首选率仅 46.2%**（前 100 是 83%、前 1000 首页 93.5%）——"空格即出"只对最常用的一百来字成立。
  已接进 CI 作回归门限（`--fail-under 95`）。
- **`docs/milestones/m2/M2-EXIT-GAP.md`**：M2 距离准出的逐条对照。结论是**准出成立**，
  另列 4 条记录缺口（验收截图未归档、3.9–3.11 无逐页图、版本号漂移、dat 两套数字）与 3 条已知限制。
- **`FONT-PLAN-REVIEW.md`**：两份字形方案的评估。`MINIMAL-FONT-SET-PLAN.md`（实测 100%）
  取代 `FULL-GLYPH-COVERAGE-PLAN.md`（推算 ≥99%）作为实施依据，后者的"非 BMP 退路"与
  "字形风格论证"被吸收。新增里程碑 **F 字形与字体**（F0 GDI 探针 → D-F1 风格决策 →
  F1/F2 绘制改造 → F3 字体包 → F4 过滤三态化），已进 `ROADMAP.md` / `PLAN.md`。

- **Shift 切中英**：单独按一下 Shift（不夹带其它键）在中/英之间切换；英文态下所有键归应用。
  `Shift+A` 仍是大写 A，不会切成英文（lone-Shift 锁存）。
- `uigeom` 诊断行：`comp=/cand=` 实际几何 + `anchor=` + `caret=0/1`（锚点是否退化成鼠标位置）。

### 修复

- **`zhongguo` 只剩 guo 的候选**：候选取 `syllables_.back()`（从右往左），且上屏后清空整个组合串，
  等于把前面打的丢掉。改为**左到右逐音节上屏**：候选给第一个未转换音节，选中后只吃掉首音节，
  余下（`guo`）留在组合串里继续转 → 连按空格得到「中国」。
- **组合窗被裁到第一个音节的宽度**：`CompositionWindow::update()` 量了文本、写了缓存的宽高，
  却从不 `SetWindowPos`。几何收敛到唯一出口 `place_windows()` → `UiWindow::apply_geometry()`，
  位置尺寸一起下发、无变化整条跳过；组合窗首音节黑体+黑下划线、余下音节灰体+浅灰下划线。
- **中英状态被焦点切换重置**：`AlignOpenStatus()` 从"每次激活"改成"每会话一次"。
- **候选窗跟着上屏的字往右爬**：锚点原来每次 `UiShow` 都重取插入符，上屏一个字插入符就右移。
  改为**一次组合只取一次锚点**；x 的右边界收口也从"按实测宽度"改成固定预留 300，
  公式不再含任何窗口宽度，打字和翻页都不会再改 x。
- imhost 编译缺 `-static` 导致 guest 端缺 `libgcc_s_dw2-1.dll` 起不来（工具侧，不在发布物内）。
- imhost 输入框汉字重叠（字距"过窄"）：真因是选到的字体没有中文字形、步进仍按拉丁宽度算
  （`adv≈0.5em`，字压字），不是字号问题；EDIT 也没有字间距 API。
  改为**按实测步进选字体**（`adv >= height*0.85`，候选逐个量）→ 命中 `Droid Sans Fallback`，
  `adv=17=1em`；输入框字号 19→17、label 12→14；找不到时打 `font FAILED` 而不是静默退化。
  另留兜底开关 `imhost.exe font:<face> size:<10–40>`（工具侧，不在发布物内）。

### 验收

- **M2 验收单 15/15 通过（2026-11-18）**：真机手打一轮 433 键，入口层与窗口层逐条对齐、程序注入 0 次；
  `nv/lv/lve/nve`、跨页选字、Backspace/Esc、100+ 字连打、lone-Shift 中英往返均有日志实据。
  逐项实测见 `docs/milestones/m2/M2-ACCEPTANCE.md` §3，还原方法见 `HANDOVER.md` §11.16。

---

## alpha0.0.1 — 2026-11-17

首个公开版本。M2「单字拼音引擎」在 ReactOS 0.4.16 i386 上以**真实键盘手打**（非程序注入）端到端通过。

### 可用能力

- `ImmInstallIMEW` 安装为独立布局「中文(简体) - RosPinyin」，`Alt+Shift` 可切换，托盘显示「中」图标
- 组合串输入：连续音节、音节下划线、`'` 硬分隔（`xi'an` → `[xi][an]`）
- 候选窗：每页 9 条，`-`/`,` 上一页、`=`/`.` 下一页、页码 `n/m`
- 选字上屏：空格选首选，`1`–`9` 选当前页候选，翻页后数字按当前页解释
- 编辑键：Backspace 回退组合串，Esc 取消不上屏
- `v` 键输入 ü：`nv`→女、`lv`→律、`lve`→略、`nve`→虐
- 数据表 44,355 字 / 419 音节（hanziDB MIT 主源 + Unicode Unihan 辅源，字节级可复现）；候选默认只呈现有字频的 9,898 字

### 修复

- **IME 从不被加载**（致命）：UI 服务窗口类名 `RosPinyinUIServer` 长 17 字符，被 `imm32` 的 `szUIClass[16]` 静默截断成 `RosPinyinUIServe`，随后按截断名 `GetClassInfoW` 失败 → 卸载 DLL 并置 `fLoadFlag=1`。类名收敛为 `RosPinyinUISrv`（≤15 字符，附 `static_assert` 与门禁检查）。
- **多字 / 丢字**：`co_IntProcessKeyboardMessage` 对 `WM_KEYUP` 也置 `bKeyUpDown`，而 `ImmProcessKey` 不看 transition 位、`ImeToAsciiEx` 又拿不到 lParam，导致每个键被喂两次。`ImeProcessKey` 现在对 key-up 一律返回 `FALSE`。
- **简体「谁」打不出来**：Unihan 里 U+8C01 `kMandarin` 只有 `shéi`，语料高频的 `shuí` 不在全集内被丢弃，`shui` 音节被繁体「誰」占据。`mkdat` 改为把 `kHanyuPinlu` 的音作为**次读音**保留；`CodeTable::load_dat` 增加可选回调，为次读音建别名索引（同一字可回答多个音节且不重复出字）。
- **候选窗出现空心方框**：44,355 条数据中 34,457 条是 Unihan 独有生僻字，其中 68% 在随包字体子集（`U+4E00–U+9FA5`）之外。`PinyinSession` 默认过滤无字频字（该音节过滤后为空则回退全表），可用 `set_show_rare(true)` 恢复。
- 修正 `rospinyin_res.rc` 版本资源与实际版本不符的问题，版本号统一为 `alpha0.0.1`。

### 效果对比（真实引擎 + 真实数据）

| 输入 | 修复前 | 修复后 |
|---|---|---|
| `shui` | 34 条，首选「水」，简体「谁」不存在，20 个空心框 | **7 条**：水 谁 税 睡 稅 誰 脽，简体「谁」第 2 位，0 个空心框 |
| `zhong` | 108 条 | 26 条 |
| `shi` | 340 条 | 95 条 |

### 已知限制

- 只支持**单字**上屏，没有词组与词频（M4）；`zhongguo` 目前只能逐字选
- 没有中文标点、全角/半角转换、Shift 临时英文（M3）
- 语言栏菜单项（属性 / 关于）为空（M6）
- `ImmGetOpenStatus` 在 ReactOS 上的读数异常（`-1` / `0` 混杂），Shift 切英文后的按键行为尚未验证
- 组合串窗口位置在每次 `feed` 都会重置一次（`ImmSetCompositionWindow` 调用偏多），快速输入时可能看到轻微跳动
- 无安装器，当前需手工复制 `.ime` / `.dat` 到 `C:\ReactOS\system32\` 并用 `ImmInstallIMEW` 注册
- 仅在 ReactOS 0.4.16 i386（QEMU）上验证；未在真实硬件、Windows、x64 上测试

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 53/53、`xingma/`（形码产品线）零改动。

---

## 未发布（`alpha0.0.2` 候选）— 2026-11-17

M2 收口改动。**不改数据格式、不改 IMM32 ABI**，`alpha0.0.1` 的数据表可直接沿用。

### 修复

- **`ImmGetOpenStatus` 读数"异常"**：`-1` 是宿主程序的 `g_himc == NULL` 哨兵（恰等于 `TRUE`），
  `0` 是 ReactOS 建输入上下文时的出厂值（`imm32_imm.c` `pIC->fOpen = FALSE`，激活路径从不回置）。
  现在 `ImeSetActiveContext` 激活时把 OS 侧状态对齐为 `TRUE`，trace 记录 `openalign before=/after=`；
  键路由仍不依赖该值（契约闸门已加：`should_consume_key` 内不得出现 `OpenStatus`）。
- **组合串窗口被裁切**：窗口量完文本只更新了缓存宽高，从未 `SetWindowPos`，
  `zhongguo` 仍按 `zhong` 的宽度绘制。几何改为单一 owner（`place_windows()` → `apply_geometry()`）。
- **快速输入时窗口跳动**：每次引擎更新都 `InvalidateRect + ShowWindow + SetWindowPos`。
  现在只有几何真正变化才下发、只有窗口隐藏时才 `ShowWindow`、只有内容变化才重绘。
  顺带查清：`IMN_SETCOMPOSITIONWINDOW` 每键 1–3 次是 ReactOS 的 `EDIT_SetCaretPos` 发的
  （`edit.c`，仅 `__REACTOS__` 分支），不是引擎行为，IME 侧无法减少。
- **安装器只拷贝 `.ime`**：装完缺 `rospinyin.dat` → 候选窗空白。`/step1` 现在两份都拷并校验字节数；
  文件被映射（`ERROR_SHARING_VIOLATION`）时先改名 `.old` 再写，`/status` 报告两份，`/u` 两份都删。
- **扩展 A 区字符显示为方框**：随包字体子集到 `U+9FA5` 为止，`shi` 里有一个**有字频**的扩展 A 字。
  候选进分页器前按 `GetGlyphIndicesW` 过滤（按码点缓存）；探针不可用或过半被剔时整体放行，
  避免字体异常时把候选清空。

### 工程

- `tools/check_m1.py`：53 → **57** 项，新增 4 条 M2 不变量（组合窗几何 owner、重绘条件化、
  安装器两份 payload、open 状态不得作为门禁）。
- 新增 `docs/milestones/m2/M2-REPORT.md`；`docs/milestones/m2/M2-REMAINING.md` 更新为"仅剩真机验收单"。
- trace 新增 `wmove=`（真正改变窗口几何的次数）与 `wrep=`（重绘请求次数）、
  `cand-filter`（字形过滤剔除数）。

### 已知限制（未变）

- 单字上屏；无中文标点/全半角/Shift 临时英文（M3）
- 字体子集不含扩展 A 区，该区候选被过滤（不出现，而不是显示方框）
- 安装器仍是命令行工具，ReactOS 上需 `/step1`…`/step4` 分步 + 重启

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 57/57、`xingma/` 零改动。

---

## 版本号约定

- `alpha0.0.x`：M2 阶段，单字可用，数据与接口可能随时调整
- `0.1.0`：M3 完成（标点、全半角、临时英文）
- `0.2.0`：M4 完成（词组与词频）
- `1.0.0`：达到 README §3 的 v0.1 Definition of Done
