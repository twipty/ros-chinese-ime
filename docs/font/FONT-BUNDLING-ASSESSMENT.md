# 随包发布 Noto Sans CJK SC 并设为 RosPinyin UI 默认字体——可行性评估

> **⚠️ 2026-11-19 计划变更**：F 里程碑改为「构建期分级码表 + 全字形开关」
> （`docs/plan/ROADMAP.md` §F）。本文结论中「随包字体作为 UI 默认」已不再是当前选择
> （UI 字体维持系统 Droid Sans Fallback，分级码表按它的 cmap 构建）；字体包需求移交
> F-T4（全字形用户自选安装文津/Jigmo）。本文的许可/注册时序分析仍被 F-T4 引用。

> 状态：**静态评估完成，实机验证未做**。依据：本仓库逐行核查 + 字体上游官方许可页 + ReactOS 官方 JIRA/邮件列表/源码。评估日期：2026-11-18。本文不改动任何构建目标或契约。
> 问题：是否把 Noto Sans CJK SC 字体随本项目一起发布，并作为本项目输入法 UI 的默认显示字体。

---

## 0. 结论速览

**总评：可行，但"全量字体直接随包 + 设为默认"这一朴素方案不推荐；推荐改为"子集化 + 随包 + 安装器注册 + UI 链置顶"的定向方案。**

| 维度 | 判定 | 一句话依据 |
|---|---|---|
| 许可证 | ✅ **可行（有条件）** | SIL OFL 1.1 明确允许随软件捆绑再分发；必须随附版权声明与许可全文；RFN 是 `'Source'` 而非 `'Noto'`——未改名直接捆绑不触发改名义务 |
| 技术兼容性 | ⚠️ **大概率可行，需实机确认** | ReactOS 用 FreeType、GDI `AddFontResourceW` 已实现；但上游 CORE-12748 曾以 "too large and bad in metrics" 拒绝 Noto Sans CJK TTC，OTF/CFF 在 ReactOS 上的渲染质量是最大未知 |
| 体积 | ❌ **全量不可接受** | `NotoSansCJKsc-Regular.otf` ≈ **16 MB**，对比现有发行包 `.ime` 0.9 MB + `.dat` 0.9 MB——字体会占发行包 **约 90%**；子集化可压到 1–3 MB |
| UI 默认字体改造 | ✅ **很小** | `CreateUiFont` 的候选链改一项即可；字形探针复用同一函数，会自动跟着换 |
| 安装/持久化 | ⚠️ **需要新增机制** | `AddFontResource` 仅当前会话有效；要跨重启必须写 `HKLM\...\Fonts` 注册表并复制到 Fonts 目录——这是安装器此前刻意避开的注册表写入类型 |
| 与现有设计的一致性 | ⚠️ **部分冲突** | 当前设计用"字形过滤 + 回退"解决豆腐块，捆绑字体会改变过滤结果；NOTICE §6 明确写着"不打包任何字体" |

**一句话建议**：**做，但用子集化字体、作为可选组件、保留现有回退链；不要全量捆绑、不要删除 Droid Sans Fallback 回退。** 若只是想解决 M2 验收里"扩展 A 区候选显示空心框"的问题，捆绑字体是比"过滤掉候选"更彻底的方案，但要付出体积与安装复杂度。

---

## 1. 现状核查（代码与文档证据）

### 1.1 UI 字体链：已包含 Noto，但不是 CJK OTF 的内部名

`framework/platform/win32/ui/ui_window.cpp:104-113`：

```cpp
// ReactOS ships DroidSansFallback.ttf (covers ~16.5k Hanzi) but only mounts
// the CJK font registry keys under a CJK locale. Try the common faces in order
// and keep the one GDI actually resolved (GetTextFaceW reports substitutions).
const wchar_t* const kFontFaces[] = {
    L"Droid Sans Fallback",
    L"Noto Sans SC",
    L"Microsoft YaHei",
    L"SimSun",
    L"Arial Unicode MS",
};
```

`CreateUiFont()`（`:173-196`）逐个 `CreateFontW`，再用 `GetTextFaceW` 校验 GDI 实际解析出的字体名；**只有名字匹配才保留**（`:188`），否则 `DeleteObject` 换下一个（`:189`），最后兜底让 GDI 自选（`:191-195`）。

**关键细节（决定捆绑是否生效）**：
- 链里第二个是 `Noto Sans SC`——这是 **Google Fonts 的产品名**，而 `NotoSansCJKsc-Regular.otf` 的 Internal Family Name 是 **`Noto Sans CJK SC`**。两者字符串不同，`ascii_iequal`（`:115-126`）不会匹配，所以**即便把 CJK OTF 装进系统，现有链也选不中它**。
- **这不是字形覆盖问题**：用 fontTools 实测 `m0/share/NotoSansSC-subset.ttf`，name 表只有 `Noto Sans CJK SC`（family/full）与 `NotoSansCJKsc-Regular`（PostScript），**根本没有 `Noto Sans SC` 这个 family 名**；而其 cmap 有 21,287 个码点、U+4E00–9FA5 覆盖 20,901 字，"你好中国谁水"全部命中——**CJK 字形完全齐全**。名字对不上纯粹是发行版命名差异（Google Fonts 把 `Noto Sans CJK SC` 展示为 `Noto Sans SC`）。
- 该子集字体真正的覆盖缺口是 **CJK 扩展区**：扩展 A（U+3400–4DBF）**0 字**、扩展 B（U+20000+）**0 字**；对比 `DroidSansFallback.ttf` 基本区 16,504 字、扩展 A 仅 2 字。这才是 M2 候选窗空心框的根源——不是"没有 CJK 字形"，而是"没有扩展区字形"。
- 因此"设为默认"至少要改 `kFontFaces`：把 `L"Noto Sans CJK SC"` 置于首位（或保留 `Noto Sans SC` 条目并追加 `Noto Sans CJK SC`）。
- **文档已陈旧**：`HANDOVER.md:376` 把字体链写成 `Microsoft YaHei → SimSun → Noto Sans SC → 系统默认`，与代码里 Droid Sans Fallback 居首的真实顺序不符——捆绑落地时应一并纠正。
- **注册时序耦合**：若字体改为运行时 `AddFontResourceW` 注册（而非安装期写注册表），必须保证 `GlyphProbe` 构造（`ime_ui.cpp:239-246`）时字体已注册——否则探针选中的是回退字体，过滤结果与窗口实际字体不一致。安装期注册可规避此问题。

### 1.2 字形探针与"豆腐块"过滤：与字体链强耦合

`framework/platform/win32/ui/ime_ui.cpp:231-298`：

- `GlyphProbe` 构造时直接调用 `ui::CreateUiFont(16)`（`:242`），用 `GetGlyphIndicesW(..., GGI_MARK_NONEXISTING_GLYPHS)` 逐码点探测（`:274`），按码点缓存（`:263-279`）。
- `UiCanRender()`（`:298`）是唯一查询入口；探针创建失败或码点 > BMP 时**fail-open**返回 true（`:260`），不隐藏文本。
- `framework/platform/win32/imm32/imm_entry.cpp:344-365` 的 `renderable_candidates()` 用它剔除显示不出的候选；并有"全部被剔或过半被剔则整体放行"的护栏（`:349-352` 注释）。

**含义**：换字体 = 换探针字体 = **自动改变候选集**。M2 阶段之所以加过滤，就是因为 `rospinyin.dat` 44,355 字中 34,457 条是 Unihan 生僻字、68% 在扩展 A/B 区（`HANDOVER.md:805`），而本地测试用的 `NotoSansSC-subset.ttf` 只覆盖 U+4E00–U+9FA5。捆绑全量 Noto Sans CJK SC 后扩展区覆盖会改善，但 16 MB 的代价换来的只是少数生僻字可见——**性价比低**。

**字体度量还影响三处 UI**：
- 候选窗 `candidate_window.cpp:109-113` 用 `GetTextExtentPoint32W` 按**实际字体度量**算列宽与行高；
- 组合窗 `composition_window.cpp:55-57` 先是 `text_.size()*9+16` 的**粗略估算**（16px 字体的拉丁假设），`:65` 才用 `GetTextExtentPoint32W`；
- 工具条 `toolbar.cpp:84-90` 同样按实测度量居中，而 `pinyin/src/scheme.cpp:179` 的标签本身就是汉字 `L"拼"`。

→ 这正是 CORE-12748 "bad in metrics" 需要实机验证的具体落点：**候选行高、截断、重叠、工具条对齐**都可能随字体度量变化。

### 1.3 发布链路：当前不发布任何字体

- `tools/install-rospinyin/install.cpp:27` 注释："payload is TWO files"；`:43-44` 仅 `kImeFileName` + `kDatFileName`；`:176-184` `/step1` 只拷这两个；`:326-350` `/status` 只报两个；`:383-384` 卸载只删两个。
- GitHub Release `alpha0.0.1` 资产仅 `rospinyin.dat`(902,776 B)、`rospinyin.ime`(946,193 B)、`SHA256SUMS.txt`(160 B)——**无字体**。
- `m0/share/NotoSansSC-subset.ttf`（11 MB，OpenType）是 `docs/milestones/m2/M2-ACCEPTANCE.md:84` 与 `HANDOVER.md:377,805` 提到的 **VM 本地测试资产**；`m0/share/` 被 `.gitignore:31` 排除，**不在发行包里**。
- `NOTICE.txt:49-53` 第 6 节"字体"明确："**不打包任何字体**……发行包若未来捆绑字体，须在此追加对应许可声明。"

> 注意一处文档措辞不一致：`HANDOVER.md:805` 称 `NotoSansSC-subset.ttf` 为"随包字体"，容易被误读成发行包内含字体；实际它只是随 VM 测试目录分发、被 gitignore 的本地资产。捆绑方案落地时应顺手澄清这句话。

### 1.4 安装器的能力边界

`install.cpp` 已具备"拷贝文件到系统目录（`system_path()`，`:70-81`）、被占用时改名 `.old` 退让（`move_aside()`，`:117-124`）、校验字节数、分步 `/step1`…`/step4`、卸载清场"这套机制。新增一个字体文件在**文件搬运层面**不算大改（照 `copy_one()` 加第三个 payload 即可）；但"让字体真正生效"还需要 `AddFontResourceW` + 注册表 `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts` + `WM_FONTCHANGE` 广播——这超出了现有安装器的职责，且与该项目的 M0 红线（尽量少写注册表、分步验证）方向相反。

---

## 2. 字体上游事实

### 2.1 许可证：SIL OFL 1.1，允许随软件捆绑

来源：Google Fonts "Noto Sans Simplified Chinese" 官方许可页、notofonts/noto-cjk 仓库 `Sans/LICENSE`。

- 版权行：`Copyright 2014-2021 Adobe (...), with Reserved Font Name 'Source'`；CTAN 条目标注 `Copyright 2015-2024 Google Inc.` 与 `Copyright 2014-2025 Adobe`。
- **RFN 是 `'Source'`，不是 `'Noto'`**：OFL 条件 3 只禁止**修改版**使用保留字体名。因此：
  - 原样捆绑 Noto Sans CJK SC → 无改名义务；
  - 若做子集化（属于 Modified Version）→ 只要主字体名不含 `'Source'`（例如仍叫 `Noto Sans CJK SC` 或改为 `RosPinyin UI`），也不违反 RFN 条款。
- OFL 条件 2：**每份拷贝必须附带版权声明与本许可全文**（standalone 文本文件或易查看的机器可读元数据均可）。→ 发行包必须新增 `OFL.txt` 之类文件。
- 条件 1：字体不得单独售卖（随软件一起分发/销售允许）；条件 5：字体本身必须始终以 OFL 分发，**不能并入本项目的 MIT 授权单元**。这与 `NOTICE.txt` 现有的"数据/代码分层授权"模式（CC-CEDICT 单独分发）完全一致，照抄即可。

### 2.2 体积：全量 16 MB，子集可降到 1–3 MB

- `NotoSansCJKsc-Regular.otf` ≈ **16 MB**（CTAN 文件清单：16–17 MB；另一来源 15.7 MB）；全套 TTC（NotoSansCJK.ttc / SourceHanSans.ttc）**109 MB**。
- 子集化（pyftsubset / fonttools）是把 CJK 字体从 16 MB 压到 1–3 MB 的成熟做法；对 CFF/OTF 需用 `--desubroutinize` 等参数，并保留 OpenType layout features。
- 本项目已有一个**本地子集实例**：`m0/share/NotoSansSC-subset.ttf` 11 MB，只覆盖 U+4E00–U+9FA5（`HANDOVER.md:805`）。这说明 11 MB 对"常用字"仍偏大——真正按 `.dat` 的 9,898 个有字频字 + UI ASCII/标点做精确子集，体积可显著低于此。

### 2.3 ReactOS 侧：能加载，但历史上有过负面结论

- **能加载**：`gdi32` 的 `AddFontResourceW` 已实现（`lib/gdi32/objects/font.c:741-744`，转调 `NtGdiAddFontResource`）；ReactOS 集成 FreeType，JIRA CORE-9566 讨论中开发者明确表示 "FreeType does and it's incorporated in ROS"，即 OTF 格式在理论上可解析。Windows API 文档也把 `.otf`（PostScript OpenType）列为 `AddFontResourceW` 支持格式。
- **持久化**：`AddFontResource` 仅当前会话有效，重启后消失；要跨重启必须复制到字体目录并写 `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`（Microsoft Learn 原文）。ReactOS 社区有 fontinst.exe / FontReg 工具做这件事（`reactos.org/forum/viewtopic.php?t=15777`），说明该路径在 ReactOS 上被实践过。
- **⚠️ 负面先例（必须重视）**：[CORE-12748 "Create the CJK Package (optional)"](https://jira.reactos.org/browse/CORE-12748)（2007 年提出、2017 年活跃、**至今 Untriaged / Unresolved / 无 Fix Version**）中，Katayama（ReactOS IMM32 主要维护者）先提议加入 `NotoSansCJK.ttc`，随后在同一 ticket 改口："**'NotoSansCJK.ttc' and 'SourceHanSans.ttc' fonts were too large and bad in metrics**"；其自建 CJK Package 安装包 60 MB，最终没有进入官方 ISO。这是上游维护者对 Noto Sans CJK 的**明确负面判断**，理由是体积与字体度量（metrics），不是许可证。
- 引申风险：**"bad in metrics"** 可能直接影响 UI——候选窗/组合窗按像素高度排布（`CreateUiFont(pixel_height)`），字体度量不合规可能造成候选行高异常、截断或重叠。这必须在 ReactOS VM 实机验证，静态无法排除。

---

## 3. 分层可行性判定

| 层 | 判定 | 依据 |
|---|---|---|
| ① 许可证合规 | ✅ 可行 | OFL 1.1 允许捆绑；附 OFL 全文 + 版权声明；独立授权单元，不动 MIT 代码 |
| ② UI 代码改造 | ✅ 很小 | `kFontFaces` 置顶一项；探针复用 `CreateUiFont` 自动跟随；需核对 family name 字符串 |
| ③ 安装/持久化 | ⚠️ 需新增机制 | 拷贝+校验可复用 `copy_one()`；但 `AddFontResourceW` + `HKLM\...\Fonts` + 广播是新逻辑，与"少写注册表"红线相抵 |
| ④ 字体渲染质量 | ⚠️ 最大未知 | ReactOS FreeType 能解析 OTF，但 CORE-12748 明确记录 "bad in metrics"；需实机 |
| ⑤ 体积/分发 | ❌ 全量不可接受 / ✅ 子集可行 | 16 MB vs 现包 1.8 MB；子集 1–3 MB 可接受；建议独立可选包 |
| ⑥ 与现有过滤策略的关系 | ⚠️ 需重新设计 | 候选过滤依赖探针结果；捆绑字体会改变候选集；"过半被剔则放行"护栏需重测 |

**总评 = 中等可行。** 没有许可证死锁，也没有架构性障碍；主要成本在"子集化字体生产 + 安装器持久化 + 实机度量验证"。若目标是"让 UI 更好看"——收益有限且需实机确认；若目标是"消除扩展区空心框"——捆绑全量字体有效，但 16 MB 代价高，更聪明的做法是按 `.dat` 实际字符集做精确子集。

---

## 4. 风险清单

| 级别 | 风险 | 缓解 / 验证 |
|---|---|---|
| **P0-1** | ReactOS 上 OTF/CFF 字体度量异常（CORE-12748 "bad in metrics"），导致候选窗/组合窗布局破坏 | 实机前不放行；在 VM 中对比 Droid Sans Fallback 与 Noto 的候选行高、截断、重叠；有问题则保留 Droid 为首选 |
| **P0-2** | OFL 合规遗漏：未随附许可全文/版权声明，或子集改名误用 'Source' | 发行包内置 `OFL.txt`；子集主字体名避开 'Source'；NOTICE §6 改写并列入字体条目 |
| **P1-3** | 安装器写 `HKLM\...\Fonts` 与 M0"少写注册表"红线冲突，且需管理员权限 | 字体做**可选组件**（独立安装包或 `/with-font` 开关），不进默认 `/step1`…`/step4`；写键逐项验证 |
| **P1-4** | 字体文件被映射/占用导致升级失败（同 `.ime`/`.dat` 的 sharing violation） | 复用 `move_aside()` 改名 `.old` 机制；`/status` 增加字体状态上报 |
| **P1-5** | 全量 16 MB 使发行包膨胀 ~10 倍，RAPPS/ISO 分发体积失控 | 只发子集（目标 ≤3 MB）；或作为独立可选资产，不放进主发行包 |
| **P1-6** | UI family name 字符串不匹配（`Noto Sans SC` vs `Noto Sans CJK SC`），装了也选不中 | `kFontFaces` 同时列出两个候选名；用 `GetTextFaceW` 实测确认实际返回的 family name |
| **P2-7** | 字形过滤候选集变化：捆绑后更多生僻字可见，可能重新引入"选字上屏得到怪字" | 重跑 `docs/milestones/m2/M2-ACCEPTANCE.md`；保留"过半被剔则放行"护栏；核对 `renderable_candidates()` 行为 |
| **P2-8** | 文档措辞不一致：NOTICE §6"不打包字体" vs HANDOVER:805"随包字体"；HANDOVER:376 的字体链顺序也与代码不符 | 落地时同步改写；明确区分"VM 测试资产"与"发行包内容" |
| **P1-9** | **测试闸门完全缺失**：`tools/check_m1.py` 无任何字体/字形断言（`:36-46` 扫描后缀不含字体；`:371-385` 只有 `kDatFileName` 对称检查）；`tests/CMakeLists.txt:5-71` 的 11 个 ctest 无字体/UI 渲染测试；`docs/milestones/m2/M2-ACCEPTANCE.md:83-86` 字体问题只靠 VM 人工看方框 | 新增 `check_m1.py` 静态断言（kFontFaces 含目标 family name、安装器字体清单对称性）；M2 验收单加字体度量项；真实渲染只能靠 VM |
| **P1-10** | **子集化不可复现**：现有 `NotoSansSC-subset.ttf` 的生成脚本 `font_subset.py` 在 `~/.workbuddy/tmp/`、未入仓（`HANDOVER.md:825`），无法按固定输入重建；11 MB 体积也偏大 | 把子集化脚本纳入 `tools/`，固定上游 URL + SHA256 + 字符集来源，产出目标 ≤3 MB（沿用 mkdat 的可复现模式） |

---

## 5. 建议路径（分方案 + 门禁）

### 方案 A（推荐）：子集化 + 可选组件 + UI 置顶

| 阶段 | 动作 | 门禁 |
|---|---|---|
| **0. 定字符集** | 从 `rospinyin.dat` 导出 9,898 个有字频字 + UI 用 ASCII/标点/全角标点 → 生成 `font_chars.txt`；**把生成脚本从 `~/.workbuddy/tmp/` 纳入 `tools/`** | 字符集与脚本都可复现（固定 URL + SHA256，沿用 `tools/mkdat/build_dat.sh:25-31` 的模式） |
| **1. 生产子集字体** | 用 `pyftsubset` 对 `NotoSansCJKsc-Regular.otf` 子集化；主字体名避开 'Source'；目标 ≤3 MB | 体积达标；`GetGlyphIndicesW` 对 9,898 字全部命中 |
| **2. 合规打包** | 新增 `OFL.txt`（许可全文 + Adobe/Google 版权行）；改写 NOTICE §6 为"字体单独授权单元" | 每份字体拷贝旁都有许可文件；MIT 授权单元不含字体 |
| **3. UI 置顶（可回退）** | `kFontFaces` 置顶 `L"Noto Sans CJK SC"`，保留 `Droid Sans Fallback` 为第二；同时保留 `Noto Sans SC` 条目 | Linux 语法检查 + ReactOS VM 实测 `GetTextFaceW` 返回预期名 |
| **4. 安装器（可选开关）** | 新增 `/with-font`：拷贝字体到 Fonts 目录、`AddFontResourceW`、写 `HKLM\...\Fonts`、`WM_FONTCHANGE` 广播；`/u` 对称清理 | 分步验证；重启后字体仍在；无 0xF4 类崩溃 |
| **5. 实机验收** | ReactOS VM 对比有无字体的候选窗：行高、截断、重叠、扩展区字可见性；≥50 次切换压测 | 对齐 `docs/milestones/m2/M2-ACCEPTANCE.md`，无度量回归 |

### 方案 B（不推荐，仅记录）：全量 OTF 直接随包

- 16 MB 字体 + 必须的动态安装 + CORE-12748 的 "too large / bad in metrics" 前科 → 收益（少数生僻字可见）远低于成本。
- 若一定要做，至少作为**独立可选资产**发布，不并入主 `.ime` + `.dat` 发行包。

### 方案 C（零成本替代）：不捆绑，改进过滤提示

- 保留现状，仅把"被字体过滤掉的字"在候选窗给出视觉提示（如计数/省略号），避免用户误以为候选系统缺字。
- 无许可、体积、安装器风险；不解决"生僻字上屏"本身，但消除困惑。

---

## 6. 未决问题（必须实机回答）

1. ReactOS 0.4.16 上 `NotoSansCJKsc-Regular.otf`（CFF/outline）能否被 `AddFontResourceW` 成功加载？`GetTextFaceW` 返回什么 family name？
2. CORE-12748 的 "bad in metrics" 在 2026 年的 ReactOS（0.4.16 + 更新后的 GDI/FreeType）是否仍然存在？候选窗行高是否异常？
3. 写 `HKLM\...\Fonts` + 复制字体到 Fonts 目录，是否触发与 M0 0xF4 类似的登录期加载问题？
4. 子集化字体在 ReactOS FreeType 上的渲染质量（小字号、低 DPI）是否可接受？
5. 捆绑字体后 `renderable_candidates()` 的候选集变化是否引入新的"怪字上屏"？

---

## 7. 最小改动文件清单（落地清单）

按层分组，供实施时逐项核对（行号为评估时状态）：

**UI 层**
- `framework/platform/win32/ui/ui_window.cpp` — 必改：`kFontFaces` 置顶 `L"Noto Sans CJK SC"`，保留 `L"Noto Sans SC"` 与 `Droid Sans Fallback` 回退；更新 `:104-106` 注释。
- `framework/platform/win32/ui/ui_window.h` — 更新 `:20-23` 的字体选择注释。
- `framework/platform/win32/ui/ime_ui.cpp` — 通常无需改（探针复用 `CreateUiFont`）；若走运行时注册，须保证 `GlyphProbe` 构造（`:239-246`）前字体已注册。
- `framework/platform/win32/ui/README.md` — 更新字体链描述与已知限制。
- `HANDOVER.md` — 纠正 `:376` 的字体链顺序，澄清 `:805` 的"随包字体"措辞。

**打包安装层**
- `tools/install-rospinyin/install.cpp` — 加 `kFontFileName`；`/step1`、`/status`、uninstall、usage 各加一份（仿 `:371-385` 的 `kDatFileName` 处理）；新增 `AddFontResourceW` + `HKLM\...\Fonts` + `WM_FONTCHANGE`（建议可选开关）。
- `.github/workflows/ci.yml` — stage（`:123-145`）与 publish 文件清单（`:194-196`、`:205-211`）加字体；若需构建则加子集化 job。
- `.gitignore` — `m0/share/` 被 `:31` 整体排除；字体正式入库需开例外或新建 `tools/font/` 路径。
- 新增：子集字体文件（≤3 MB）+ **入仓的可复现生成脚本**（现有 `font_subset.py` 在仓库外）。

**文档合规层**
- `NOTICE.txt` — 改写 §6（`:49-53`）为字体独立授权单元，追加 SIL OFL 1.1 版权行与来源。
- 新增 `OFL.txt`（许可全文 + Adobe/Google 版权行，OFL 条件 2 要求随附）。
- `README.md` — §5 表（`:67-72`）加字体行，声明与主包授权分离。
- `PLAN.md` — 更新 `:70`、`:137` 的字体计划表述。

**测试层**
- `tools/check_m1.py` — 新增静态断言：`kFontFaces` 含目标 family name、安装器字体清单对称性。当前**无任何**字体闸门。
- `docs/milestones/m2/M2-ACCEPTANCE.md` — `:83-86` 增补字体度量验收项（行高/截断/重叠/扩展区可见性）。
- （维护收益低）`tests/CMakeLists.txt` — 11 个 ctest 无字体/渲染测试；真实字体渲染无法在 Linux 覆盖。

> 字体改动**不涉及** `.ico`/`.rc`/布局显示名（`ime/rospinyin_res.rc:24,57,74-75`、`ime/rospinyin.h:13-16`），`CI` 的 ICON 资源闸门（`:172-188`）无需调整。

---

## 8. 证据来源

**本仓库**：`framework/platform/win32/ui/ui_window.cpp:104-113,115-126,173-196,231`、`framework/platform/win32/ui/ui_window.h:20-23`、`framework/platform/win32/ui/candidate_window.cpp:109-113`、`framework/platform/win32/ui/composition_window.cpp:55-57,65`、`framework/platform/win32/ui/toolbar.cpp:84-90`、`pinyin/src/scheme.cpp:179`、`framework/platform/win32/ui/ime_ui.cpp:231-298`、`framework/platform/win32/imm32/imm_entry.cpp:344-382,402`、`tools/install-rospinyin/install.cpp:27,43-44,70-81,117-124,176-203,326-350,371-385`、`tools/check_m1.py:36-46,371-385`、`tests/CMakeLists.txt:5-71`、`.github/workflows/ci.yml:39-49,123-145,172-188,194-196,205-211`、`NOTICE.txt:43-53`、`HANDOVER.md:376-377,805,825`、`docs/milestones/m2/M2-ACCEPTANCE.md:83-86`、`.gitignore:31`、`ime/rospinyin_res.rc`、`ime/rospinyin.h`、GitHub Release `alpha0.0.1` 资产清单。

**字体上游**：[Google Fonts — Noto Sans SC 许可页](https://fonts.google.com/specimen/Noto+Sans+SC/license)（SIL OFL 1.1 全文、版权行、RFN 'Source'）；[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)；[CTAN notocjksc 文件清单](https://ctan.org/tex-archive/fonts/notocjksc)（字体 16–17 MB、TTC 109 MB）；[SIL OFL 官网](https://openfontlicense.org/)。

**ReactOS 上游**：[CORE-12748](https://jira.reactos.org/browse/CORE-12748)（"too large and bad in metrics"、CJK Package 60 MB、Unresolved）；[CORE-9566](https://jira.reactos.org/browse/CORE-9566)（FreeType 已集成）；[gdi32 font.c](https://git.reactos.org/?p=reactos.git;a=blob;f=reactos/lib/gdi32/objects/font.c)（`AddFontResourceW` 已实现）；[Microsoft Learn AddFontResourceW](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-addfontresourcew)（会话限制 + 注册表持久化）；[ReactOS 字体安装论坛帖](https://reactos.org/forum/viewtopic.php?t=15777)。

> 所有"可行"结论均为静态 + 上游证据；字体度量的核心风险必须由 ReactOS VM 实机验证收口。
