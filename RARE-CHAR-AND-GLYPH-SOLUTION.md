# 冷僻字支持与字形渲染方案

> 状态：**静态评估完成，实机验证未做**。依据：本仓库逐行核查 + 已有字体实测数据 + 项目验收文档记录。评估日期：2026-11-18。本文不改动任何代码或契约。
> 问题：(1) 冷僻字能否改善支持？(2) 无字频字批量赋予低权重、排在可选字末尾是否可行？(3) 解决字形渲染的最优方案。

---

## 0. 三个问题的直接回答

| 问题 | 结论 | 一句话理由 |
|---|---|---|
| 冷僻字能否改善支持 | ✅ **能，但要先分清"能输入"和"能显示"** | 数据层已支持 44,355 字；短板全在渲染层与准入策略 |
| 无字频字低权重排末尾 | ⚠️ **方向对、手段错、且大半已实现** | 排序层已天然排末尾；真正卡点是 `show_rare_ = false` 的准入开关，不是权重 |
| 字形渲染最优方案 | ✅ **"修探针 + 改护栏 + 分级准入 + 精确子集"四层组合** | 单纯换字体无效——探针的非 BMP fail-open 会让缺字形的字绕过过滤 |

**核心判断**：这个项目"冷僻字显示成方框"的问题，本质是**三处独立缺陷叠加**，而不是一个可以通过"换个字体"或"赋个低权重"单点解决的问题：

1. **准入缺陷**：无字频字默认根本不进候选列表（`show_rare_ = false`），只在音节一个候选都不剩时整体回退；
2. **探针缺陷**：`GlyphProbe::has()` 对 `codepoint > 0xFFFF` 直接 `return true`，扩展 B 字**从不接受字形检查**，缺字形也照样进候选；
3. **护栏缺陷**："过半被剔则整体放行"的护栏按**整个候选列表**统计，一旦 rare 字占多数，护栏会把刚被探针剔掉的字**再全部放回来**。

第 2、3 条互相抵消：即使修好探针，不动护栏，扩展区 tofu 依然会重新出现。这是本文最重要的技术结论。

---

## 1. 现状证据链（逐层）

### 1.1 数据层：冷僻字"能输入"已经完备

- `pinyin/include/rospinyin/pinyin_session.hpp:56-60` 注释：44,355 条数据中 **34,457 条**来自 Unihan 且无字频，多为 CJK 扩展 A/B。
- `HANDOVER.md:805`：9,898 条有字频，其余 34,457 条中 **68%（23,433 条）落在 U+4E00–U+9FA5 之外**，"多为 U+20000 以上"（扩展 B）。
- `tests/emit_text_test.cpp:76` 用 **U+20BB7（CJK Ext-B）** 验证非 BMP 上屏路径——证明代码明确预期扩展 B 字符存在。
- 结论：**任何合法音节都有候选，包括只有生僻字的音节**（`recompute()` 的 `rare` 回退保证）。输入能力不是短板。

### 1.2 排序层：低权重排末尾"已经做了"

`framework/src/code_table.cpp:191-200`：

```cpp
std::sort(indices.begin(), indices.end(),
          [this](std::size_t a, std::size_t b) {
              const std::uint32_t rank_a = entries_[a].rank;
              const std::uint32_t rank_b = entries_[b].rank;
              if (rank_a != rank_b) return rank_a < rank_b;
              return entries_[a].codepoint < entries_[b].codepoint;
          });
```

- `dat::kUnknownRank = 0xFFFFFFFF`（`framework/include/imfw/dat_format.hpp`）是 `uint32_t` 最大值，按升序排**必然在所有有字频字之后**；同 rank 内再按码点升序，结果稳定可预期。
- `pinyin/src/pinyin_session.cpp:134-136` 同时把无字频字的 `score` 置为 **0**（最低分）：

```cpp
candidate.score = entry.rank == kUnknownRank
                      ? 0
                      : static_cast<int>(1000000u - entry.rank);
```

- 结论：**"批量赋予低权重、排在可选字末尾"在排序与打分两层都已天然成立，无需再实现。** 用户感知不到，是因为下一层的准入把它们藏起来了。

> ⚠️ 附带发现：`Candidate::score`（`framework/include/imfw/engine.hpp:25-28`）目前**没有任何消费者**——`CandidatePager::reset()` 只是 `move`，不排序（`framework/src/candidate_pager.cpp:8-11`）。顺序完全由 `CodeTable` 的 rank 排序决定。因此"改 score 来调顺序"是**无效操作**；若未来要让运行时权重真正生效，必须先让 pager 或引擎按 score 排序，这是另一个话题。

### 1.3 准入层：真正的卡点

`pinyin/src/pinyin_session.cpp:126-143`：

```cpp
// Two passes: ranked characters first, then (only if that left nothing to
// pick) the Unihan-only ones.
std::vector<Candidate> rare;
for (const auto& entry : entries) {
    Candidate candidate;
    candidate.text.assign(1, static_cast<char32_t>(entry.codepoint));
    candidate.score = entry.rank == kUnknownRank
                          ? 0
                          : static_cast<int>(1000000u - entry.rank);
    if (entry.rank == kUnknownRank && !show_rare_) {
        rare.push_back(std::move(candidate));
    } else {
        candidates_.push_back(std::move(candidate));
    }
}
if (candidates_.empty()) candidates_ = std::move(rare);
```

- `show_rare_ = false`（`pinyin_session.hpp:73`）是默认值，只能通过 `set_show_rare(true)`（`:61`）打开。
- 语义：**"有常用字就完全不显示生僻字；一个都没有才整体回退到生僻字"**，不是"生僻字排末尾"。
- 文档一致记载这一设计：`HANDOVER.md:813`、`CHANGELOG.md:48`、`m2/M2-REPORT.md:84`（"**`set_show_rare()` 无 UI 入口**，生僻字默认不可见"）。
- `m2/M2-REPORT.md:84` 已把"无 UI 入口"列为待办，计划 M3 语言栏菜单项——**开关本身存在，缺的是用户可达入口与更细的策略**。

### 1.4 探针与过滤层：两处缺陷叠加

`framework/ui/ime_ui.cpp:257-281`（`GlyphProbe::has()`）：

```cpp
bool has(char32_t codepoint) {
    // Anything outside the BMP is not in our data set; and a probe that
    // cannot run at all must not hide text.
    if (dc_ == nullptr || font_ == nullptr || codepoint > 0xFFFF) return true;
    ...
    const wchar_t wide = static_cast<wchar_t>(codepoint);
    WORD index = 0;
    const DWORD result =
        GetGlyphIndicesW(dc_, &wide, 1, &index, GGI_MARK_NONEXISTING_GLYPHS);
    const bool present = result != GDI_ERROR && index != 0xFFFF;
```

- **缺陷 A（非 BMP fail-open）**：`codepoint > 0xFFFF` 无条件返回 `true`。注释"Anything outside the BMP is not in our data set"与 `HANDOVER.md:805`（23,433 字多在 U+20000+）**直接矛盾**。后果：全部扩展 B 字**从不探字形**，缺字形也判定为"可渲染"，直通候选窗 → 方框上屏。
- **附带实现问题**：即便去掉 fail-open，`const wchar_t wide = static_cast<wchar_t>(codepoint)` 对非 BMP 是**截断**而非代理对；Windows `wchar_t` 为 16 位，必须传 2 码元。这条路径从未被正确实现过。
- 探针复用 `ui::CreateUiFont(16)`（`:242`），与候选窗实际绘制字体同源——这是**正确的设计**，必须保留。

`framework/host/imm_entry.cpp:353-382`（`renderable_candidates()`）：

```cpp
const std::size_t dropped = input.size() - kept.size();
if (dropped == 0) return input;
if (kept.empty() || kept.size() * 2 < input.size()) {
    // font has no CJK at all — that is a font problem, not a candidate problem
    return input;                 // ← 整体放行
}
return kept;
```

- **缺陷 B（护栏粒度错误）**：护栏按**整个候选列表**判断。若某音节只有 5 个常用字 + 200 个扩展 B 字，且扩展 B 全被剔：`kept = 5, input = 205`，`5*2 < 205` 成立 → **200 个方框字全部放行**。探针白做。
- 结论：**必须把护栏的统计口径从"全部候选"改为"有字频候选"**——"字体连常用字都画不出"才是字体故障；"生僻字画不出"是正常现象，不该触发放行。

### 1.5 字体层：子集覆盖缺口是根因之一

`FONT-BUNDLING-ASSESSMENT.md:46-49` 已用 fontTools 实测：

- `NotoSansSC-subset.ttf` 内部 family 名是 `Noto Sans CJK SC`，**不是**代码里找的 `Noto Sans SC` —— 名字不匹配，装了也选不中；
- cmap 有 21,287 码点、U+4E00–9FA5 覆盖 20,901 字，**常用 CJK 字形齐全**；
- 真正的缺口是**扩展区**：扩展 A 0 字、扩展 B 0 字；
- `DroidSansFallback.ttf`（ReactOS 自带）基本区 16,504 字、扩展 A 仅 2 字 —— 同样覆盖不了扩展 B。

→ 字体层现状决定了：**扩展区字在任何现有字体下都是方框**；过滤是唯一防线，而这道防线目前有上述两处漏洞。

---

## 2. 方案设计（四层，按优先级）

### 第 1 层（P0，必做，零字体成本）：修复非 BMP 探针

文件：`framework/ui/ime_ui.cpp`

- 删除 `codepoint > 0xFFFF` 的 fail-open 分支；
- 对非 BMP 用 UTF-16 代理对构造 `wchar_t[2]`，调用 `GetGlyphIndicesW(dc, pair, 2, indices, GGI_MARK_NONEXISTING_GLYPHS)`；
- 判定：`result == 2 && indices[0] != 0xFFFF && indices[1] != 0xFFFF`；
- 保留 `dc_ == nullptr || font_ == nullptr` 的 fail-open（探针整体不可用时不能隐藏文本）；
- 缓存键保持 `char32_t`，代理对只在调用点构造；
- 单测：Linux 侧无法调 GDI，但可把"码点 → 代理对"拆成纯函数并加 `tests/` 用例（与 `emit_text_test.cpp` 同构）。

**收益**：扩展 B 字第一次真正接受字形检查。这是所有后续方案的前提。

### 第 2 层（P0，与第 1 层同批）：护栏按"有字频候选"统计

文件：`framework/include/imfw/engine.hpp`、`pinyin/src/pinyin_session.cpp`、`framework/host/imm_entry.cpp`

- 给 `Candidate` 增加一个标志位，例如 `bool rare = false`（或 `flags` 位域，为将来更多状态留位）；
- `PinyinSession::recompute()` 按 `entry.rank == kUnknownRank` 设置该标志；
- `renderable_candidates()` 拆两组统计：
  - **common（rare=false）**：若全被剔 → 说明字体连常用汉字都没有 → 整体放行（原护栏语义，但只看常用字）；
  - **rare（rare=true）**：被剔即被剔，**不再触发放行**；
- 结果：字体缺扩展区字形时，rare 字被安静剔除，常用候选完整保留，候选窗不再出现方框。

> 这一层是**反直觉但必需**的：只修探针不改护栏，tofu 会从后门回来。两层必须同批上线。

### 第 3 层（P1，体验）：rare 字分级准入 + 可见性标识

文件：`pinyin/include/rospinyin/pinyin_session.hpp`、`pinyin/src/pinyin_session.cpp`、`framework/ui/candidate_window.cpp`

把布尔 `show_rare_` 升级为分级策略（保持旧 API 兼容）：

| 策略 | 语义 | 适用 |
|---|---|---|
| `Off`（默认） | 维持现状：只在音节无任何候选时整体回退 | 普通用户，候选窗干净 |
| `TailLimited`（建议新默认或轻量档） | 在常用候选**之后**追加每音节 N 个（建议 3–5）rare 字 | 想偶尔打生僻字，又不希望候选爆炸 |
| `All`（原 `set_show_rare(true)`） | 全部 rare 按码点顺序排在后面 | 专业用户 / 明确查字场景 |

配套：
- 候选窗对 `rare` 候选加**视觉标识**（如前置 `·` 或不同颜色），让用户知道"这是罕见字、可能显示不全"——对应 `FONT-BUNDLING-ASSESSMENT.md` 方案 C 的思路，零成本消除困惑；
- 语言栏菜单暴露策略切换入口（兑现 `m2/M2-REPORT.md:84` 的 M3 待办）；
- `TailLimited` 的 N 必须**小**：`shi` 一类音节 rare 字可达数百，全放进去会淹没常用字并让翻页失效。

**注意**：`TailLimited` 依赖第 2 层的 rare 标志；三、四层不可拆开单独上。

### 第 4 层（P2，可选，治本）：按 `.dat` 实际字符集精确子集化字体

文件：遵循 `FONT-BUNDLING-ASSESSMENT.md` 方案 A 的落地清单（子集脚本入仓、OFL 合规、安装器可选开关、UI `kFontFaces` 置顶 `Noto Sans CJK SC`）

- **目标不是"全量 16 MB"**，而是按 `.dat` 的 9,898 个有字频字 + UI ASCII/标点做精确子集（目标 ≤3 MB）；
- 若确需覆盖部分扩展 A 常用字，可额外加入白名单；**扩展 B 全量（数万字）会让子集体积反弹，不建议**——继续靠第 1–3 层的过滤/标识处理；
- 必须实机验证 CORE-12748 的 "bad in metrics" 风险（候选行高、截断、工具条对齐）；
- 必须随附 OFL 全文与版权声明，字体作为独立授权单元，不并入 MIT。

**取舍**：若目标只是"不再出现方框" → **只做第 1+2 层即可，零体积、零许可、零安装器改动**。第 4 层是"让更多生僻字真的看得见"的增强项，不是修 bug 的必需项。

---

## 3. 决策矩阵：目标 → 最小改动组合

| 用户目标 | 最小组合 | 说明 |
|---|---|---|
| 消除候选窗/上屏的方框 | **第 1 + 2 层** | 修探针 + 改护栏，改 3 个文件，无需字体 |
| 让生僻字"可选但不碍事" | 第 1 + 2 + 3 层 | 加 rare 标志与 `TailLimited` 策略 + UI 标识 |
| 让扩展 A 常用生僻字真正显示 | 第 1 + 2 + 3 + 4 层 | 再叠加精确子集字体，需实机验证度量 |
| 只是不想让用户困惑 | 单独第 3 层的"标识"部分 | 沿用 `FONT-BUNDLING-ASSESSMENT.md` 方案 C |

---

## 4. 改动文件清单（按层）

**第 1 层**
- `framework/ui/ime_ui.cpp` — 非 BMP 代理对探测；拆出可测的纯函数。

**第 2 层**
- `framework/include/imfw/engine.hpp` — `Candidate` 加 `rare` 标志。
- `pinyin/src/pinyin_session.cpp` — `recompute()` 设置 `rare` 标志。
- `framework/host/imm_entry.cpp` — `renderable_candidates()` 分组护栏。

**第 3 层**
- `pinyin/include/rospinyin/pinyin_session.hpp` / `pinyin/src/pinyin_session.cpp` — `RarePolicy` 分级与尾部限量。
- `framework/ui/candidate_window.cpp` — rare 标识渲染。
- `ime/` 或语言栏菜单装配处 — 策略切换入口（兑现 M3 待办）。

**第 4 层（可选）**
- 见 `FONT-BUNDLING-ASSESSMENT.md` §7 完整清单（UI 链、安装器、CI、NOTICE/OFL、测试闸门）。

**测试**
- `tests/` 新增：rank 排序不变式、rare 标志传播、尾部限量、代理对构造；
- `m2/M2-ACCEPTANCE.md` 增补：扩展 B 音节（`biǎo` 类）候选窗无方框、上屏码点正确、`TailLimited` 翻页仍可用。

---

## 5. 关键不变量（实施时必须守住）

1. **上屏路径不动**：`emit_text()` 已正确处理非 BMP 代理对（`emit_text_test.cpp` 覆盖），问题只在候选窗显示与过滤，不要改上屏逻辑。
2. **探针与绘制字体同源**：`GlyphProbe` 必须继续复用 `CreateUiFont()`；若字体改为运行时注册，须保证探针构造前字体已注册（`FONT-BUNDLING-ASSESSMENT.md:52`）。
3. **探针整体失败仍 fail-open**：`dc_/font_` 为空时不能隐藏文本。
4. **常用字优先**：任何策略下，有字频字都必须排在 rare 字之前，且不得因 rare 字过多而被挤出首屏。
5. **排序不变式**：`CodeTable` 的 rank 升序 + 码点升序保持不变，这是"低权重排末尾"的成立基础。

---

## 6. 证据来源

**本仓库**：`pinyin/include/rospinyin/pinyin_session.hpp:56-73`、`pinyin/src/pinyin_session.cpp:114-144`、`framework/src/code_table.cpp:191-200`、`framework/include/imfw/dat_format.hpp`（`kUnknownRank`）、`framework/include/imfw/engine.hpp:25-28`、`framework/src/candidate_pager.cpp:8-47`、`framework/ui/ime_ui.cpp:231-298`、`framework/host/imm_entry.cpp:344-382,402,451,491`、`framework/ui/candidate_window.cpp:33-47,65-149`、`tests/emit_text_test.cpp:73-82`、`tests/pinyin_session_test.cpp:257-260`、`tests/candidate_pager_test.cpp`、`HANDOVER.md:404,805,813,974`、`CHANGELOG.md:48`、`m2/M2-REPORT.md:84`、`m2/M2-REMAINING.md:76`、`m2/M2-ACCEPTANCE.md:12,83-86`、`FONT-BUNDLING-ASSESSMENT.md:46-52,139,164-167`。

> 所有结论均为静态代码 + 项目文档证据；探针的实际 GDI 行为、字体度量、候选窗观感必须由 ReactOS VM 实机验证收口。`.dat` 未入库，码点分布引用自 `HANDOVER.md:805` 的项目记录，未逐字节复算。
