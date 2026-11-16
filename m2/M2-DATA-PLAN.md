# M2 数据工具链可行性报告与 D2 决策建议

> 日期：2026-11-16　状态：预研完成，待 Owner 拍板 D2
> 本文回答两件事：① M2 单字引擎的数据来源是否拿得到、许可证是否合规；② 决策点 D2（字频来源）该怎么定。
> 所有结论均基于**实际下载数据 + 实地解析验证**，非纸面推演。

---

## 1. 结论速览（TL;DR）

| 决策项 | 结论 | 依据 |
|---|---|---|
| 拼音映射源 | ✅ **Unihan 18.0.0 `kMandarin`** 可用，官方直连可达 | 实测下载 8.0 MB / sha256 `4c93ea9c…`，44355 条 |
| 字频源（D2） | ⚠️ **不能直接采用 Jun Da 原始表**（License 限制商用）；建议改用 **hanziDB.csv（MIT）** 或 **Unihan `kHanyuPinlu`** | 官方版权声明全文 + MIT LICENSE 实证 |
| 覆盖率 | ✅ Unihan 覆盖 hanziDB 前 3500 字 **100%**（3500/3500） | 实地交叉验证 |
| 引擎影响 | ⚠️ 声调需从「带调符号」转「数字调」，且**数字插在元音后**（`zhōng`→`zho1ng`），不是音节尾 | 44355 条全量转换验证 |

**D2 推荐**：采用 **hanziDB.csv（MIT）作主字频源**，Unihan `kHanyuPinlu` 作补充/交叉校验；**放弃直接使用 Jun Da 原始表**。理由见第 4 节。

---

## 2. 拼音数据源：Unicode Unihan `kMandarin`

### 2.1 可得性与规格（实测）

- 下载：`https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip`（HTTP 200，无代理可达）
- 体积：**8,340,649 字节**（约 8.0 MB），sha256 `4c93ea9c1f636451729a840978f1667a53886af37ba854fdcce109721c63d43e`
- 版本：**Unicode 18.0.0**，日期 2026-07-31
- 数据文件：`Unihan_Readings.txt`（zip 内 8 个文件之一）
- `kMandarin` 条目数：**44,355** 条；其中 BMP 汉字 **26,712** 条
- 多音字：仅 **109** 条（0.25%）用空格分隔多个读音

### 2.2 License

Unicode License（Unicode, Inc. 版权声明）允许 **use, copy, modify, merge, publish, distribute, and/or sell**，仅要求保留版权与许可声明、注明修改。**与本项目 MIT 兼容**，可自由分发编译后的数据。需在 `NOTICE.txt` 保留 Unicode 版权声明。

### 2.3 关键坑：声调表示（影响引擎设计）

Unihan 的 `kMandarin` 用**带声调符号的拉丁字母**（如 `zhōng`、`lǜ`），不是 `zhong1`/`lü4` 数字调。全量统计用到的带调字符 27 种（ā á ǎ à / ē é ě è / ī í ǐ ì / ō ó ǒ ò / ū ú ǔ ù / ǖ ǘ ǚ ǜ / ü / ḿ ǹ ň）。

实地转换验证（44355 条全跑通）得到三条**必须写进编译器**的规则：

1. **数字调位置 = 紧跟在带调元音之后**，不是音节末尾：
   - `zhōng` → `zho1ng`（1 插在 o 后）✅
   - `guó` → `guo2` ✅
   - 这直接影响 3.1.1「415 合法音节 + 声调变体」的数据结构：应以**无声调音节**为键，声调作独立属性。
2. **ü 保留**：`nǚ` → `nü3`、`lǜ` → `lü4`；引擎输入层须把用户键入的 `v` / `u:` 归一到 `ü`（或统一用 `v` 表示，二选一，建议 M2 定）。
3. **多音字空格分隔**：`嗯` = `ǹg en` → 两个读音；编译器取第一个作主音，其余作次选（对应 PLAN 3.2.1「取最常用音，其余作次选」）。

### 2.4 额外发现：Unihan 自带字频字段 `kHanyuPinlu`

- 条目数 **3,799**，格式如 `U+4E00 kHanyuPinlu yī(32747)`（括号内为频次）
- 优点：与 `kMandarin` 同源、Unicode License 合规、无需外部数据
- 缺点：仅覆盖 3799 字（高频字为主），长尾不足
- 定位：**作字频交叉校验与首选音判定的辅助源**，不作唯一字频源

---

## 3. 字频数据源：D2 调研

### 3.1 Jun Da 原始表 —— ❌ 不可用

官方版权声明（`https://lingua.mtsu.edu/chinese-computing/copyright.html`，1998-2026，页面更新 2010-09-16）原文：

> "The author of the content of these webpages maintains the copyright to the various character frequency/digram lists presented on this web site. They are provided here for **research and teaching/learning purposes only**. Any commercial use of the lists should have **prior written permission from the author**."

判定：项目代码为 MIT（允许商用），附带 Jun Da 数据会引入「商用需书面许可」的限制，**与 MIT 目标冲突**。第三方站点（如 RECATOOLS）已在 2026-11-03 因此把 Jun Da 表替换为自算语料表，佐证该限制的现实影响。**结论：不直接采用。**

### 3.2 hanziDB.csv —— ✅ 推荐主字频源

- 仓库：`https://github.com/rtang18/hanziDB.csv`（fork 自 ruddfawcett/hanziDB），**MIT License**（Copyright (c) 2017 Rudd Fawcett，实测 LICENSE 全文确认）
- 数据：`data/hanziDB.csv`，**541 KB，10,000 字**（简体）
- 列：`frequency_rank, charcter[pinyin], definition, radical, radical_code, stroke_count, hsk_level, general_standard_num`
  - 注意原表列名 `charcter` 拼写有误，编译器需容错
- 频次基于 Jun Da 排名，但**衍生数据以 MIT 发布**——这是关键：MIT 授权的是这份 CSV 文件本身。
- ⚠️ **合规风险提示**：hanziDB 的 MIT 覆盖的是其整理/衍生工作；底层排名源自 Jun Da 的限制性声明。严格来说存在「上游数据许可传导」的灰色地带。缓解方案见第 4 节。

### 3.3 其他备选（记录备查）

- **Tatoeba 语料自算**（RECATOOLS 2026-11 做法）：用开放语料自行统计，CC BY 2.0 FR 署名即可，完全合规，但需引入语料下载与统计步骤。
- **Unihan `kHanyuPinlu`**：合规、同源，但仅 3799 字，长尾不足。

---

## 4. D2 决策建议

**推荐：主字频源 = hanziDB.csv（MIT），辅以 Unihan kHanyuPinlu 交叉校验；在 NOTICE.txt 中同时标注 Jun Da 原始来源与 hanziDB 的 MIT 声明。**

理由链：

1. **合规优先**：直接用 Jun Da 原始表与 MIT 冲突，不可接受。
2. **hanziDB 是 MIT 发布的衍生数据**，且正是为「按字频排序」整理的现成 CSV，工程成本最低。
3. **覆盖率验证通过**：Unihan 覆盖 hanziDB 全部前 3500 常用字（100%），两源可无缝 join（`char` ↔ `kMandarin`）。
4. **风险可控**：hanziDB 的上游传导风险通过「NOTICE 双署名 + 数据可替换设计」降到最低——字频数据编译成独立 `rospinyin.dat`，未来若需替换为自算语料表，只需换输入文件、不动引擎代码（对应 PLAN 3.2.3「可单独更新」的设计）。

**备选（若 Owner 偏好最保守合规）**：直接用 Tatoeba 自算字频或仅用 `kHanyuPinlu`，放弃 hanziDB。代价是长尾字排序质量下降或需额外语料处理。

**请 Owner 拍板**：D2 = （A）hanziDB MIT 主源 + kHanyuPinlu 辅源【推荐】／（B）纯 Unihan kHanyuPinlu／（C）Tatoeba 自算。

---

## 5. 对 M2 后续工作的影响（已验证的工程约束）

| M2 项 | 影响 |
|---|---|
| 3.1.1 音节数据结构 | 以「无声调音节」为键 + 声调独立属性；全量得到 1,396 个带调音节模式，需归一到无声调约 400+ 个（与 PLAN「415 合法音节」量级吻合） |
| 3.1.2 音节切分器 | `v`/`u:`→`ü` 归一化前置；多音字取首选音 |
| 3.2.1 Unihan 解析 | 需实现带调符号→数字调转换器（规则见 2.3），本次已用 44355 条验证可行 |
| 3.2.2 字频整合 | 按 char join hanziDB rank + Unihan 拼音；`charcter` 列名容错 |
| 3.2.3 mkdat | 输入 = Unihan_Readings.txt + hanziDB.csv（+ 可选 kHanyuPinlu），输出紧凑 dat；目标 < 2 MB 可达 |
| NOTICE.txt | 需新增 Unicode License 声明 + hanziDB MIT 署名 + Jun Da 来源标注 |

---

## 6. 资产清单（本次预研产物，均在 `~/.workbuddy/tmp/`）

| 文件 | 说明 |
|---|---|
| `Unihan.zip` | Unicode 18.0.0 官方包，8.0 MB，sha256 已记录 |
| `hanziDB.csv` | MIT 字频表，10,000 字 |
| 声调转换与覆盖验证 | 一次性 Python 脚本（第 2.3 / 2.4 / 3.3 节的实地验证） |

原始数据不入库（体积与可再获取性考虑）；M2 实现时由构建脚本按固定 URL + sha256 拉取，保证可复现。
