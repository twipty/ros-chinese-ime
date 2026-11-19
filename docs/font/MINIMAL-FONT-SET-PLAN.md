# 最少字体覆盖最多字形——方案（不限定 Noto）

> **⚠️ 2026-11-19 计划变更**：F 里程碑改为「构建期分级码表 + 全字形开关」
> （`docs/plan/ROADMAP.md` §F）。本文的字体选型结论（文津三件套实测 100% / Jigmo 备选）
> **仍然有效并指导 F-T4 字体包**；本文其余的「IME 自绘多字体 + 探针同步改造」部分
> 已随运行时探测路线一并作废。

> 状态：**静态评估 + 本地实测完成，ReactOS 实机未做**。评估日期：2026-11-18。
> **2026-11-18 评估结论：本方案作为实施依据**（实测 100% 覆盖，单一家族，OFL 可随包）。
> ⚠️ 补一条本文件未提的风险：**文津宋体是宋体**，而当前 UI 字体是黑体（Noto Sans SC / Droid），
> "常用字黑体 + 生僻字宋体"的混排后果需要 owner 决策（决策点 D-F1），见 `FONT-PLAN-REVIEW.md` §2.1。
> 问题：不局限 Noto Sans CJK SC，有没有"覆盖尽可能多字形、用尽可能少字体"的方案。
> 本文所有覆盖率来自对真实 `rospinyin.dat` 44,355 码点与真实字体 cmap 的 fontTools 交叉实测。

---

## 0. 结论速览

**有。答案是：用一套"大字符集"字体家族即可 100% 覆盖 `.dat` 全部 44,355 字——但"一套"在物理上是 3 个 TTF（按 Unicode 平面分片），因为单个 TTF 的 glyphID 是 16 位、装不下 10 万字。**

| 方案 | 字体文件数 | 对 dat 覆盖 | 简体字形 | 授权 | 实测 |
|---|---:|---:|---|---|---|
| 现状 Noto 子集 + Droid | 2 | 47.1% | 是 | — | ✅ 实测 |
| 全量 Noto Sans CJK SC | 1 | **61.7%** | 是 | OFL | ✅ 实测 |
| GoNotoKurrent（单文件"全文字体"） | 1 | **27.0%** | 中/日混合 | OFL | ✅ 实测 |
| Surface `chs_boot.ttf` | 1 | 60.1% | 是 | 微软专有 | ✅ 实测 |
| **文津宋体 v2.100 P0+P2+P3** | **3** | **100%** | ✅ 大陆 G 源 | OFL 1.1 | ✅ **实测 44,355/44,355** |
| **Jigmo 2025-09-12 三件套** | **3** | **100%** | ⚠️ 明朝体/未声明简体 | CC0 1.0 | ✅ **实测 44,355/44,355** |

**推荐：文津宋体 v2.100（P0+P2+P3 三件套）。** 理由：实测 100% 覆盖 + 大陆简体字形标准 + OFL 可随包商用 + 有 TTF（TrueType 曲线，比 CFF/OTF 更贴合 ReactOS GDI/FreeType）。Jigmo 同为 100% 且是 CC0（授权最宽松），但明朝体风格、未声明大陆简体字形，简体中文 UI 里可能出现异体字观感。

---

## 1. 关键实测数据

### 1.1 为什么"单个字体"覆盖不全

`.dat` 44,355 字的码点分布（上一轮已实测，此处复述关键项）：
- 基本区 20,924（47.2%）、扩展 A 5,787（13%）、**扩展 B 14,618（33%）**、扩展 C–I 约 2,900、其余为兼容区。
- 非 BMP 字 17,643（39.8%）；基本区外 23,431（52.8%）。

单字体实测（真实 cmap 对 44,355 码点）：

| 字体 | cmap 码点 | 覆盖 dat | 关键短板 |
|---|---:|---:|---|
| 全量 `NotoSansCJK-Regular.ttc`（SC face） | 44,810 | 27,355（61.7%） | 扩展 B 仅 3.4%、G/H/I≈0 |
| `GoNotoKurrent-Regular.ttf`（15.5 MB，Steam 自带） | 37,478 | 11,977（27.0%） | 以常用字为主，扩展区几乎全缺 |
| Surface `chs_boot.ttf`（3.7 MB） | 28,696 | 26,678（60.1%） | 微软专有、不可随包；覆盖近似 Noto 全量 |
| `NotoSansSC-subset.ttf` | 21,287 | 20,891（47.1%） | 扩展区 0 |
| `DroidSansFallback.ttf` | 29,622 | 16,500（37.2%） | 扩展 A 仅 2、扩展 B 0 |

**结论**：所有"单文件"字体最多覆盖到 61.7%，因为 Unicode 汉字已超 10 万，而 TrueType glyphID 上限 65,535——物理上无法把 Ext A–J 全塞进一个 TTF。

### 1.2 全覆盖字体家族实测（决定性）

**文津宋体 v2.100**（下载 `WenJinMincho-TTF.7z`，23.6 MB 压缩，SHA256 `22ab30a4...` 与官方一致，解压为 3 个 TTF）：

| TTF | cmap | 大小 | 覆盖 dat | 负责区块 |
|---|---:|---:|---:|---|
| `WenJinMinchoP0-Regular.ttf` | 31,529 | 21.8 MB | 26,915（60.7%） | 基本区 20,924/20,924、扩展 A 5,787/5,787（均 100%） |
| `WenJinMinchoP2-Regular.ttf` | 61,650 | 29.2 MB | 16,304（36.8%） | 扩展 B/C/D/E/F/I **全部 100%** |
| `WenJinMinchoP3-Regular.ttf` | 13,558 | 6.1 MB | 1,369（3.1%） | 扩展 G 1,123/1,123、H 231/231（100%） |
| **三文件并集** | — | **57.1 MB** | **44,355/44,355 = 100%** | **零缺口** |

**Jigmo 2025-09-12**（下载 `Jigmo-20250912.zip`，33,559,738 B，CC0）：

| TTF | cmap | 大小 | 负责区块 |
|---|---:|---:|---|
| `Jigmo.ttf` | 50,845 | 30.0 MB | BMP + SMP（基本区/扩展 A/兼容） |
| `Jigmo2.ttf` | 63,047 | 35.7 MB | SIP（扩展 B–F、I） |
| `Jigmo3.ttf` | 13,529 | 7.5 MB | TIP（扩展 G/H/J） |
| **三文件并集** | — | **73.2 MB** | **44,355/44,355 = 100%，零缺口** |

> 两款字体都只需"一个字体家族"（用户感知是一个字体），但物理上各 3 个文件、按 Unicode 平面分片——这是 TrueType 16 位 glyphID 上限的硬约束，不是字体作者的选择。

---

## 2. 推荐方案：文津宋体三件套

### 2.1 为什么是它

1. **实测 100% 覆盖**，含 `.dat` 里全部扩展 B（14,618 字）与 G/H/I。
2. **大陆简体字形**：文津宋体以思源宋体 + GlyphWiki + 改版 Kage 生成，" mainly presented in China mainland glyphs standard (G source)"——对简体中文 IME 是关键，避免 Jigmo 那种"字对但形状是异体"的观感。
3. **OFL 1.1**：允许随软件捆绑、嵌入、商用分发；需附 `LICENSE.md` 全文与版权声明；RFN 为 `WenJin Mincho/文津宋体/…`，本项目不修改字形则无需改名；若日后子集化/改字形需避开 RFN。
4. **TrueType 曲线（TTF）**：ReactOS 的 GDI + FreeType 对 TrueType `glyf` 的支持比 CFF/OTF 更成熟，降低 CORE-12748 "bad in metrics" 风险（仍需实机确认）。

### 2.2 需要注意的是非技术风险

- **W 版（Windows GDI 兼容版）自 2026-02-16 停更**，v2.100 只发标准版。W 版解决的是 Uniscribe 下引号/标点变西文的问题；ReactOS 用的不是 Uniscribe，影响有限，但标点宽度仍需实机看。
- 作者明确警告 **GitCode 等平台未经授权搬运**，下载必须走官方 GitHub/Gitee/百度网盘渠道并固定 SHA256。

### 2.3 备选：Jigmo（CC0）

- 授权最宽松（CC0，无署名/无 RFN 义务），实测同样 100%。
- 但：明朝体（宋体风格，与 UI 常见黑体不统一）、页面未声明大陆简体字形、3 文件共 73.2 MB（比文津大 28%）。
- 适合作为"授权零负担"路线，若实机发现文津宋体的 OFL 合规或字形风格不可接受则切换。

---

## 3. 工程落地（与上一份方案的关键差异）

上一份 `FULL-GLYPH-COVERAGE-PLAN.md` 走的是"Noto + Plangothic 双家族叠加"，本方案改为"单一大字符集家族三分片"，更简洁：

### 3.1 字体链配置（唯一真相源）

在代码里定义单一字体链常量，供探针与绘制共用：

```cpp
// framework/platform/win32/ui/ui_window.cpp（示意）
static const wchar_t* const kGlyphFontFiles[] = {
    L"WenJinMinchoP0-Regular.ttf",  // BMP/SMP：基本区+扩展A
    L"WenJinMinchoP2-Regular.ttf",  // SIP：扩展B-F、I
    L"WenJinMinchoP3-Regular.ttf",  // TIP：扩展G、H
};
```

- **不要**把三文件合并成一个：glyphID 16 位上限，合并会溢出；文津官方也提供 TTC 整合包（`WenJinMincho-TTC.7z`），但 TTC 只是把三个字体打包分发，GDI 仍按 face 分别加载，不减少"分片"本质。
- 候选窗绘制：**按字符码点选对应平面的字体**（`cp <= 0xFFFF`→P0，`0x20000–0x2FFFF`→P2，`0x30000+`→P3），一次 `SelectObject` 一段连续同字体字符 + `TextOutW`，度量按段累加。
- 字形探针 `GlyphProbe`：对同一码点按上述规则选对应字体，用 UTF-16 代理对调 `GetGlyphIndicesW`；**修掉 `codepoint > 0xFFFF` 直接 fail-open 的旧逻辑**。

### 3.2 数据层不变

- `.dat` 44,355 字已含全部可拼音输入的字，字体覆盖到 100% 后，**所有已有候选都能显示**。
- 字体覆盖 ≠ 可输入字集：扩展 J 等无 `kMandarin` 的字无法拼音输入，属独立扩表工程（见上一份报告 §7）。
- `show_rare` 可默认 `TailLimited`（尾部 3–5 个 rare）——字体不再产生豆腐块，rare 字可以安全放开。

### 3.3 打包与安装

- 三 TTF 共 57.1 MB（用户已明确不考虑体积）；建议做**独立可选安装包**，不塞进主 `.ime`+`.dat` 发行包。
- 安装器：`AddFontResourceW`（三个文件）+ 复制到 Fonts 目录 + 写 `HKLM\...\Fonts` + `WM_FONTCHANGE`；`/status` 上报三个字体状态；`/u` 对称清理。
- 下载脚本入仓：固定官方 URL + SHA256（仿 `tools/mkdat/fetch_data.py`），确保可复现。
- 授权：随包 `OFL.txt`（文津 `LICENSE.md` 全文），`NOTICE.txt` §6 改写为"字体独立授权单元"。

---

## 4. 风险与实机门禁

| 级别 | 风险 | 缓解 / 验证 |
|---|---|---|
| P0 | ReactOS GDI 对代理对/SIP/TIP 平面字符的渲染是否可用 | VM 逐字截图：各平面取 5 字（U+4E00 / U+20000 / U+30000）；不可用则 IDS/`U+XXXX` 标签兜底 |
| P0 | 三字体未全部注册 → 扩展区仍豆腐块 | 安装器逐个校验 `AddFontResourceW` 返回；`/status` 三态上报 |
| P1 | 字体度量异常（CORE-12748 前科） | VM 对比候选行高/截断/重叠；TTC 五号字体不同度量需分别测 |
| P1 | 标点宽度（W 版停更后的 Uniscribe 问题） | ReactOS 非 Uniscribe，实测确认全角标点；异常则用 C 版（缩窄间距） |
| P1 | OFL 合规 / 下载渠道 | 官方渠道 + SHA256；随附 LICENSE；不改名 |
| P2 | 用户误以为"装了字体就能打所有字" | 候选窗 rare 标识；README 说明字形覆盖≠可输入 |

**必须实机回答**：① 三平面字符在 ReactOS GDI 是否真能画出字形；② 57 MB 字体安装是否触发 M0 类加载/登录期问题；③ 候选窗逐字符分段绘制的度量是否稳定。

---

## 5. 实测方法与数据可复现

- `.dat` 码点：`m0/share/rospinyin.dat`，按 `dat_format.hpp` 解析 Header + 44,355×20 B `CharEntry`。
- 字体：
  - 文津宋体 v2.100 `WenJinMincho-TTF.7z`，SHA256 `22ab30a4d175f9aa0da3cc4303fedb666081c9f6bdbce267b8d9572f94231f49`（官方一致）。
  - Jigmo 2025-09-12 `Jigmo-20250912.zip`，35,559,738 B（与官网大小一致）；本地实测 SHA256 `5744c7386d129475d87607ca66d043c8793c65448adeaedc921b6931890e5d0b`——**与官网页面标注的 `2fb963ee…` 不一致**，疑为官网页面标注更新滞后，落地时须重新核对官方校验值。
- 方法：`fontTools.ttLib.TTFont(path, lazy=True)`，合并所有 `table.isUnicode()` 的 `cmap.keys()`，对 44,355 码点求交集。
- 临时脚本：`~/.workbuddy/tmp/analyze_dat_coverage.py`（dat 分布）、`min_font_set_cover.py`（单字体对比）、`measure_bigfonts.py`（大字符集字体）；字体文件在 `~/.workbuddy/tmp/wenjin/`、`~/.workbuddy/tmp/jigmo/`（临时，未入仓）。

---

## 6. 一句话建议

**要"最少字体、最多字形"，就上 `文津宋体 v2.100` 的 P0+P2+P3 三件套——实测 100% 覆盖 `.dat`、大陆简体字形、OFL 可随包；把三平面分片作为单一字体链管理，候选窗按码点选对应分片自绘。Jigmo 三件套是 CC0 的等价备选（同样 100%），但字形风格不统一。**
