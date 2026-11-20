# 全量字形收录方案（不限字体体积）

> **⚠️ 2026-11-19 计划变更**：F 里程碑改为「构建期分级码表 + 全字形开关」
> （`docs/plan/ROADMAP.md` §F），本文的「IME 自绘双字体 + 探针改造」路线随运行时探测
> 一并作废；§6 的 IDS/码点标签退路不再是候选窗的兜底形态（改为：关闭态不出现方框、
> 打开态方框是文档化预期）。

> 状态：**静态评估 + 本地实测完成，ReactOS 实机未做**。评估日期：2026-11-18。
> **2026-11-18 评估结论：本方案已被 `MINIMAL-FONT-SET-PLAN.md` 取代作为实施依据**
> （那份对 44,355 码点做了实测、拿到 100%，本方案的核心组合只到"推算 ≥99%"，Plangothic 未下载验证）。
> 但本文件有两段仍有效并被吸收进计划：**§6 非 BMP 渲染的退路（IDS/码点标签）**、
> **§4 字形风格论证（黑体连续 vs 宋体/明朝体混排）**。见 `FONT-PLAN-REVIEW.md` §2。
> 问题：不考虑随包字体大小，完整收录所有字库中存在的字形，给出方案。
> 本文的覆盖率数字全部来自**本地真实 `.dat` 与真实字体文件的 fontTools 交叉实测**，不是估算。

---

## 0. 结论速览

**总评：朴素方案（把全量 Noto Sans CJK SC 打进包）达不到目标——实测只覆盖 `.dat` 的 61.7%，仍有 17,000 字是豆腐块。要做"完整收录"，必须换成"全覆盖字体 + IME 自绘多字体 + 探针同步改造"的组合方案。**

| 方案 | 对 `.dat` 44,355 字覆盖 | 简体字形 | 授权 | 判定 |
|---|---|---|---|---|
| 现状（Noto 子集 + Droid） | **47.1%**（20,895 字） | 是 | — | 基线 |
| 全量 Noto Sans CJK SC | **61.7%**（27,355 字） | 是 | OFL 1.1 | ❌ 远不够，缺 17,000 字 |
| 全量 Noto + **Plangothic** 叠加 | **≈100%**（见 §4 实测推算） | 是（基于思源黑体） | OFL 1.1 | ✅ **推荐** |
| **Jigmo**（单套 Ext A–J 全覆盖） | **≈100%** | ⚠️ 明朝体、简体字形未确认 | CC0 1.0 | ⚠️ 备选，风格不统一 |

**一句话建议**：主字体保留简体风格的全量 `Noto Sans CJK SC`（管基本区+扩展 A），叠加 **Plangothic**（简体、基于思源黑体，专补扩展 B–J），在 IME 候选窗做**逐字符双字体自绘**；不要指望系统 FontLink，也不要试图把两个字体合并成一个（TrueType glyph id 16 位上限，Noto 已用满 65,535）。

---

## 1. 实测：`.dat` 到底含哪些字（这是所有决策的基础）

数据：`m0/share/rospinyin.dat`（分析时为 902,776 B / 20 B 条；2026-11-20 格式改 v2 后为 1,080,224 B / 24 B 条，见 `DAT-FORMAT-V2.md`。结论不受影响），解析 `dat_format.hpp` 布局后逐条取 `codepoint`。

| 区块 | 码点数 | 占比 |
|---|---:|---:|
| CJK 基本区 U+4E00–9FA5 | 20,924 | 47.2% |
| **扩展 A** U+3400–4DBF | 5,787 | 13.0% |
| **扩展 B** U+20000–2A6DF | 14,618 | 33.0% |
| 扩展 C | 506 | 1.1% |
| 扩展 D | 73 | 0.2% |
| 扩展 E | 870 | 2.0% |
| 扩展 F | 122 | 0.3% |
| 扩展 G | 1,123 | 2.5% |
| 扩展 H | 231 | 0.5% |
| 扩展 I | 99 | 0.2% |
| 兼容表意文字 + 补充 | 2 | ~0% |

- **非 BMP（> U+FFFF）：17,643 字，占 39.8%**
- 基本区之外：23,431 字，占 **52.8%**——"汉字表一半以上是扩展区字"，这解释了为什么子集字体必然大量豆腐块。
- 数据里已有 **扩展 G/H/I**（1,453 字），但**没有扩展 J**（Unicode 17 新增）——数据表源自 Unihan 18.0，扩展 J 的字大多没有 `kMandarin`，见 §7。

> 实测脚本方法见 §9，数据可复现。

---

## 2. 实测：现有字体覆盖多少

| 字体 | cmap 码点 | 基本区 | 扩展 A | 扩展 B+ | 对 dat 总覆盖 |
|---|---:|---:|---:|---:|---:|
| `NotoSansSC-subset.ttf`（11 MB，随 VM 测试） | 21,287 | 20,891/20,924 (99.8%) | **0/5,787** | **0** | — |
| `DroidSansFallback.ttf`（ReactOS 自带） | 29,622 | 16,497/20,924 (78.8%) | 2/5,787 | 0 | — |
| **两者并集** | — | — | — | — | **20,895/44,355 = 47.1%** |

缺 23,460 字，分布：扩展 B 14,618、扩展 A 5,785、扩展 G 1,123、扩展 E 870、扩展 C 506、扩展 H 231、扩展 F 122、扩展 I 99、扩展 D 73、基本区 32、兼容补充 1。

**这就是 M2 验收看到一屏豆腐块的定量原因。**

---

## 3. 实测：全量 Noto Sans CJK SC 也不够（关键结论）

系统内 `/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc`（10 个 face，每 face cmap 44,810 码点），逐 face 对 44,355 字实测：

| 区块 | 全量 Noto 覆盖 |
|---|---:|
| 基本区 | 20,909/20,924（99.9%） |
| 扩展 A | 5,785/5,787（100%） |
| **扩展 B** | **494/14,618（3.4%）** |
| 扩展 C | 45/506（8.9%） |
| 扩展 D | 10/73（13.7%） |
| 扩展 E | 109/870（12.5%） |
| 扩展 F | 0/122（0%） |
| 扩展 G | 2/1,123（0.2%） |
| 扩展 H / I | 0 |
| **总计** | **27,355/44,355 = 61.7%** |

**仍缺 17,000 字**：扩展 B 14,124、扩展 G 1,121、扩展 E 761、扩展 C 461、扩展 H 231、扩展 F 122、扩展 I 99、扩展 D 63、基本区 15、扩展 A 2、兼容补充 1。

> 10 个 face（JP/KR/SC/TC/HK + 5 个 Mono）cmap 完全相同，换 face 无收益。
> 这与 Google Fonts 官方 "44,806 characters" 一致——**全量 Noto 的覆盖上限就在这里**，再大的 Noto 包也变不出扩展区字。

**结论**：用户设想的"全量 Noto 一次解决"在数字上不成立。要覆盖到那 17,000 字，只有两条路：换 Ext A–J 全集的字体，或多字体叠加。

---

## 4. 字体选型：能覆盖全部扩展区的候选（现行事实，2026-11）

| 候选 | 覆盖范围 | 格式 | 简体字形 | 授权 | 体积 | 链接 |
|---|---|---|---|---|---|---|
| **Plangothic** V2.9.5795 | 基于思源黑体，补 Ext B–J 全部 | TTF（P1 19.5 MB + P2 11.9 MB） | ✅ 简体中文字形、基于思源黑体 | SIL OFL 1.1 | ≈31 MB | [releases](https://github.com/Losketch/Plangothic-Project/releases/tag/V2.9.5795) |
| **Jigmo** 2025-09-12 | Unicode 17，Ext A–J **全部 102,998 汉字** | TTF（拆 3 个：BMP+SMP / SIP / TIP） | ⚠️ 明朝体；未声明大陆简体字形 | **CC0 1.0** | 33.9 MB（zip） | [官网](https://kamichikoichi.github.io/jigmo/) |
| HanaMin（花園明朝） | Ext A–F 全部 | TTF（A/B/C 多档） | ⚠️ 明朝体、日文字形 | GlyphWiki 许可 | 数十 MB | HanaMinAFDKO |
| WenJin Mincho | Unicode 17 汉字全集 | TTF/OTF | 明体 | 开源 | 大 | github.com/takushun-wu/WenJinMincho |

### 为什么推荐"Noto Sans CJK SC + Plangothic"而不是单用 Jigmo

1. **字形风格一致**：Plangothic 本身就是给思源黑体补扩展区的，简体字形；Noto Sans CJK SC 与思源黑体同源。两者叠加后候选窗不会出现"常用字黑体、生僻字明朝体"的突兀切换。
2. **Jigmo 是明朝体且页面未承诺大陆简体字形**——简体中文 IME 里出现日文/繁体风格的异体字，用户会觉得"打出来是错字"。
3. **Jigmo 拆 3 个 TTF 按平面分**，GDI 不会自动跨 TTF 回退，仍要解决多字体叠加，并没有简化架构。
4. **授权都允许随包**：OFL 1.1（Plangothic，需附 OFL 全文；RFN 限制同前评估）与 CC0（Jigmo，无义务）都不构成障碍。

### 推荐组合的覆盖推算

- Noto Sans CJK SC 覆盖基本区 99.9% + 扩展 A 100% + 少量扩展区 = 27,355 字；
- Plangothic 补的是"思源黑体缺失的扩展 B–J"，与 Noto 的缺口高度重合；
- **两者叠加预计覆盖 44,355 字的 ≥99%**（剩余为个别兼容表意文字及基本区 15 个冷码点）；
- ⚠️ 这是**推算**，必须用 §9 方法对真实 Plangothic TTF 再跑一次交叉实测才能写进验收单——本次未下载 Plangothic 字体文件。

---

## 5. 推荐方案架构（四部分）

### 5.1 字体随包与注册

- 主字体：`NotoSansCJKsc-Regular.ttf`（从 TTC 抽出 SC face，或直接用 Region 包 `18_NotoSansSC.zip`）。
- 补充字体：`PlangothicP1-Regular.ttf` + `PlangothicP2-Regular.ttf`。
- 安装器新增 payload 与持久化：`AddFontResourceW` + 复制进 Fonts 目录 + 写 `HKLM\...\CurrentVersion\Fonts` + `WM_FONTCHANGE` 广播（沿用 `FONT-BUNDLING-ASSESSMENT.md` 方案 A 的合规与安装路径）。
- 授权文件：`OFL.txt`（Noto + Plangothic 两份版权行与许可全文）；注意两者都不可并入项目 MIT 授权单元。
- **不要**试图用 fontTools 把两个字体 merge 成一个：TrueType `glyphID` 是 16 位，Noto 已占满 65,535，无法再容纳 Plangothic 的字形。

### 5.2 逐字符双字体自绘（核心改动）

现有 `framework/platform/win32/ui/candidate_window.cpp:133-149` 的 `on_paint()` 用一次 `TextOutW` 画整行、一个 HDC 只有一个 `font_`。叠加字体必须在这里分段：

1. 候选行的每个字符（先 UTF-32 解码，非 BMP 走代理对）逐个问"哪个字体有字形"；
2. 连续落在同一字体的字符合并成一段，对每段 `SelectObject(该字体)` + `TextOutW`；
3. 用 `GetTextExtentPoint32W` 逐段累加 advance，保证列宽正确（现有 `:107-113` 的度量逻辑改为按段累加）；
4. 组合窗 `composition_window.cpp` 若也显示汉字，同样处理；工具条标签 `拼` 在基本区，可不动。

### 5.3 字形探针改造（与自绘共用同一判定）

`framework/platform/win32/ui/ime_ui.cpp:237-298` 的 `GlyphProbe` 从"单字体"改成"字体链"：

- `has(codepoint)` 按 `Noto → Plangothic` 顺序问，任一命中即 `true`；
- **修掉非 BMP fail-open**（`:260`）：用 UTF-16 代理对 `wchar_t[2]` 调 `GetGlyphIndicesW(dc, pair, 2, indices, GGI_MARK_NONEXISTING_GLYPHS)`，判定 `result == 2 && indices[0] != 0xFFFF && indices[1] != 0xFFFF`；
- 保留 `dc_/font_` 为空时 fail-open（探针整体不可用不能隐藏文本）；
- 探针的字体顺序与自绘的字体顺序必须**来自同一份配置**，否则"过滤说能画、绘制画成方框"。

### 5.4 过滤护栏与准入策略

沿用 `RARE-CHAR-AND-GLYPH-SOLUTION.md` 的 P0/P1 结论，但要按"字体已全覆盖"重新定位：

- **护栏必须保留**：即使双字体，仍有约 1% 缺口 + 字体未注册/加载失败的场景；
- **护栏口径改为只看有字频候选**（`common`），rare 被剔不再触发整体放行；
- `show_rare` 可放心默认 `TailLimited`（尾部 3–5 个）——字体几乎全覆盖，rare 字不再等于豆腐块；
- 候选窗对 rare 字仍保留标识（罕见字提示），因为"能显示"不等于"用户认识"。

---

## 6. 渲染链的另一半：ReactOS GDI 对非 BMP 的支持

字库覆盖解决了"字符→字形"，但 39.8% 的非 BMP 字还要过 GDI 这一关：

- `candidate_window.cpp:33-47` 的 `utf32_to_wide()` 已正确生成代理对；
- `emit_text.cpp` 已按 UTF-16 码单元上屏、`emit_text_test.cpp:73-82` 用 U+20BB7（Ext-B）覆盖；
- **未知项**：ReactOS 0.4.16 的 `GetGlyphIndicesW` / `TextOutW` 对代理对与 SIP/TIP 平面字符是否真能取到字形（`ime_ui.cpp:258-260` 当初直接 fail-open，正是因为不敢依赖它）；
- **门禁**：必须在 ReactOS VM 用包含 Ext-B/C/G 的候选逐字截图验证，不能靠 Linux 单测推断。若 GDI 代理对渲染不可用，退路是候选窗自绘时把非 BMP 字画成**表意文字描述序列（IDS）**或码点标签（如 `U+20BB7`），至少不出现静默方框。

---

## 7. 数据层边界：拼音输入的上限由 Unihan 决定，不由字体决定

- `.dat` 44,355 字来自 Unihan 18.0 `kMandarin` + hanziDB；字体全覆盖只能让**已收录的 44,355 字**显示出来。
- 若要把 Jigmo 的 102,998 字**全部**变成可输入的候选，必须扩 `.dat`，但扩展 G/H/I/J 的大量字**没有 `kMandarin`**，无读音就无法拼音输入——需要另找读音源（如 `kHanyuPinlu` 覆盖范围有限）或改用笔画/部件输入方案。
- 结论：**"所有字形可显示"可达；"所有字形可用拼音输入"受数据约束，需单独扩表工程**。本方案解决前者。

---

## 8. 风险与门禁

| 级别 | 风险 | 缓解 / 验证 |
|---|---|---|
| P0 | Plangothic 在 ReactOS FreeType 上的渲染质量（CORE-12748 "bad in metrics"） | VM 实测候选行高/截断/重叠；异常则保留 Droid 为常用字首选 |
| P0 | ReactOS GDI 代理对/扩展区平面渲染不可用（§6） | VM 逐字截图验证 Ext-B/C/G；不可用则 IDS/码点兜底 |
| P0 | 双字体未注册成功 → 候选过滤与绘制错乱 | 安装器分步校验 `AddFontResourceW` 返回 + `/status` 上报字体状态 |
| P1 | ~31 MB 字体随包使 RAPPS/ISO 分发膨胀 | 用户已明确不考虑体积；仍建议做独立可选安装包 |
| P1 | 字体加载顺序与探针顺序不一致 | 两者共用单一字体链配置常量 |
| P1 | OFL 合规（两份许可全文、RFN 不含 'Source' 的变体名） | 随包 `OFL.txt`；子集体改名避开 RFN |
| P2 | rare 字默认放开后候选暴涨 | `TailLimited` 限量 + rare 标识 |

**必须实机回答**：① Plangothic 真实覆盖率（按 §9 方法跑一次）；② 双字体在 ReactOS 的度量与代理对渲染；③ `AddFontResourceW` 对 TTF 三件套的持久化是否触发 M0 类加载问题。

---

## 9. 实测方法（可复现）

1. `.dat` 码点：按 `framework/include/imfw/dat_format.hpp` 解析 Header → 44,355 × `entry_size`（v2 为 24 B）`CharEntry`，取每个 `codepoint` 字段。
2. 字体 cmap：`fontTools.ttLib.TTFont(path, lazy=True)`（TTC 用 `TTCollection`），合并所有 `table.isUnicode()` 的 `cmap.keys()`。
3. 覆盖率：对每个 dat 码点查 `cp in cmap`，按 `BLOCKS` 区间统计。
4. 本次数据文件：
   - `m0/share/rospinyin.dat`
   - `m0/share/NotoSansSC-subset.ttf`
   - `m0/probe/reactos/Fonts/DroidSansFallback.ttf`
   - `/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc`
5. 临时脚本：`~/.workbuddy/tmp/analyze_dat_coverage.py`、`~/.workbuddy/tmp/measure_full_noto.py`（本次评估用，未入仓）。

---

## 10. 落地清单（按层）

**字体与打包**
- 新增 Noto Sans CJK SC TTF + Plangothic P1/P2 TTF（固定 URL + SHA256，可复现下载脚本入仓，仿 `tools/mkdat/fetch_data.py`）。
- `tools/install-rospinyin/install.cpp`：`kFontFileName[]` 数组（3 个）、`/step1` 拷贝、`/status` 上报、`/u` 清理、`AddFontResourceW` + 注册表 + `WM_FONTCHANGE`。
- `NOTICE.txt` §6 改写；新增 `OFL.txt`；`README.md` §5 字体表。

**UI 与引擎**
- `framework/platform/win32/ui/ui_window.cpp`：`kFontFaces` 置顶 `Noto Sans CJK SC`；新增字体链配置常量供探针与自绘共用。
- `framework/platform/win32/ui/candidate_window.cpp`：`on_paint()` 逐字符分段双字体绘制；度量按段累加。
- `framework/platform/win32/ui/composition_window.cpp`：同上（若显示汉字）。
- `framework/platform/win32/ui/ime_ui.cpp`：`GlyphProbe` 改多字体链 + 修非 BMP 代理对探测。
- `framework/include/imfw/engine.hpp`：`Candidate` 加 `rare` 标志。
- `framework/platform/win32/imm32/imm_entry.cpp`：`renderable_candidates()` 护栏按 common 统计。
- `pinyin/src/pinyin_session.cpp`：`RarePolicy` 分级 + rare 标志传播。

**测试与验收**
- `tests/`：UTF-32→代理对纯函数单测、rare 标志传播、尾部限量。
- `tools/check_m1.py`：字体清单对称性、family name 断言。
- `docs/milestones/m2/M2-ACCEPTANCE.md`：扩展区逐字截图验收项（Ext-B/C/G 各取 5 字）、双字体度量对比。

**不在本次范围**：`emit_text()` 上屏链（已支持代理对，勿动）；`.dat` 扩表（§7，独立工程）。
