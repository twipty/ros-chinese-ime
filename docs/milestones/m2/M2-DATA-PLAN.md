# M2 数据工具链可行性报告与 D2 决策建议

> 日期：2026-11-16　状态：**D2 已拍板 = A**（hanziDB MIT 主源 + Unihan kHanyuPinlu 辅源）；预研完成，进入实现阶段
> 本文回答两件事：① M2 单字引擎的数据来源是否拿得到、许可证是否合规；② 决策点 D2（字频来源）该怎么定。
> 所有结论均基于**实际下载数据 + 实地解析验证**，非纸面推演。
>
> **2026-11-16 Owner 追加约束**：实际输入中 `ü` 一律用 `v` 键（如 `nv`=女、`lv`=律、`lve`=略）。
> 数据层须把 `ü`/`ǖǘǚǜ` 归一为 `v`，使输入层的 `v` 与数据层一一对应。详见第 7 节。

---

## 1. 结论速览（TL;DR）

| 决策项 | 结论 | 依据 |
|---|---|---|
| 拼音映射源 | ✅ **Unihan 18.0.0 `kMandarin`** 可用，官方直连可达 | 实测下载 8.0 MB / sha256 `4c93ea9c…`，44355 条 |
| 字频源（D2） | ✅ **已定 A**：hanziDB.csv（MIT）主源 + Unihan `kHanyuPinlu` 辅源；Jun Da 原始表因 License 限制商用**排除** | 官方版权声明全文 + MIT LICENSE 实证 + Owner 拍板 |
| 覆盖率 | ✅ Unihan 覆盖 hanziDB 前 3500 字 **100%**（3500/3500） | 实地交叉验证 |
| 引擎影响 | ⚠️ 声调需从「带调符号」转「数字调」，且**数字插在元音后**（`zhōng`→`zho1ng`），不是音节尾 | 44355 条全量转换验证 |
| 输入映射 | ✅ `ü` 用 `v` 键输入：`nǚ`→`nv3`、`lǜ`→`lv4`；数据层与输入层统一用 `v` | Owner 追加约束 + 418 个无声调音节实证 |

**D2 决定（2026-11-16 Owner 拍板）= A**：采用 **hanziDB.csv（MIT）作主字频源**，Unihan `kHanyuPinlu` 作补充/交叉校验；**放弃直接使用 Jun Da 原始表**。理由见第 4 节。

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
2. **ü 归一到 v（已定）**：`nǚ` → `nv3`、`lǜ` → `lv4`、`lüè` → `lve4`；数据层与输入层统一用 `v`，不做 `u:` 变体。详见第 7 节。
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

**备选（记录备查，未采用）**：Tatoeba 自算字频或仅用 `kHanyuPinlu`。代价是长尾字排序质量下降或需额外语料处理。

**Owner 决定（2026-11-16）：D2 = A**。hanziDB MIT 主源 + Unihan `kHanyuPinlu` 辅源，已拍板；下方第 7 节的 `v` 键约束同时生效。

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

原始数据不入库（体积与可再获取性考虑）；M2 实现时由 `tools/mkdat/fetch_data.py` 按固定 URL + sha256 拉取，保证可复现：
- `Unihan.zip`：`https://www.unicode.org/Public/18.0.0/ucd/Unihan.zip`，sha256 `4c93ea9c1f636451729a840978f1667a53886af37ba854fdcce109721c63d43e`
- `hanziDB.csv`：`https://raw.githubusercontent.com/rtang18/hanziDB.csv/master/data/hanziDB.csv`，sha256 `03f809543a2bd0d32c8f65f98567cbaa01527f2eefbdb0ea214a1abddc0b03a0`

---

## 7. `v` 键映射约束（Owner 追加，已定）

### 7.1 规则

实际拼音输入中 `ü` 一律用 `v` 键，不做 `u:` / `yu` 之类变体。数据层与输入层**统一用 `v` 表示 `ü`**：

| 规范读音 | 数字调（数据层） | 无声调音节（切分键） | 用户键入 |
|---|---|---|---|
| `nǚ` | `nv3` | `nv` | `nv` |
| `lǜ` | `lv4` | `lv` | `lv` |
| `lüè` | `lve4` | `lve` | `lve` |
| `nüè` | `nve4` | `nve` | `nve` |

实地验证：Unihan 18.0.0 的 44,355 条 `kMandarin` 全量转换后得到 **418 个无声调音节**，其中含 `v` 的恰好 4 个：`lv` / `lve` / `nv` / `nve`（与 PLAN「415 左右合法音节」量级吻合）。两个数据源里都不出现字面 `v`，`v` 纯属输入层约定，因此归一化是**单向无损**的（数据层 `ü` → `v`，输入层 `v` 直接匹配）。

> 实现期勘误记录：mkdat 初版用 `std::set` 存放多音字读音，把 Unihan「首选音在前」的顺序打乱（如 `嗯` 的 `ǹg en` 变成按字母序取 `en`），导致音节数一度变成 417。
> 已改为**保序去重**，输出恢复为 418，与预研一致。另补齐 hanziDB 独有的 `ń`（U+0144）→`n2` 映射。

另外，hanziDB 的 10,000 行里有 2 行（rank 8220 `裏` U+F9E7、rank 9019 `秊` U+F995，均为兼容表意文字）**pinyin 字段为空**，mkdat 会跳过并各报一条 warning；这是预期的脏数据过滤，不是编译错误。因此最终带排名的字数为 9,898，与预研交叉验证的 9,898/9,900 一致。

### 7.2 声调符号 → 数字调映射表（编译器与引擎共用）

数字**紧跟带调元音**（不是音节尾），`ü` 族直接并到 `v`：

```
声调1（阴平）: ā→a1 ē→e1 ī→i1 ō→o1 ū→u1 ǖ→v1
声调2（阳平）: á→a2 é→e2 í→i2 ó→o2 ú→u2 ǘ→v2 ń→n2 ḿ→m2
声调3（上声）: ǎ→a3 ě→e3 ǐ→i3 ǒ→o3 ǔ→u3 ǚ→v3 ň→n3
声调4（去声）: à→a4 è→e4 ì→i4 ò→o4 ù→u4 ǜ→v4 ǹ→n4
轻声/无调  : ü→v
```

`ḿ` / `ń` / `ň` / `ǹ` 是音节性鼻音：Unihan 收 `嗯`=`ǹg en`，hanziDB 收 `嗯`=`ń`。
归一后分别为 `m2` / `n2` / `n3` / `n4g`，无声调音节 `m` / `n` / `n` / `ng`。
`ń`（U+0144）是实地跑 mkdat 时发现 hanziDB 用到、而 Unihan 未出现的字符，已补进映射表。

### 7.3 实现落点

- `engine/src/pinyin.cpp`：`marked_to_numeric()` / `to_canonical()` / `syllable_of()` 共用同一张映射表；
- `tools/mkdat/mkdat.cpp`：编译 `rospinyin.dat` 时把 kMandarin / hanziDB pinyin 都过一遍该映射，确保数据层只存 `v`；
- `tools/mkdat/fetch_data.py`：固定 URL + sha256 拉取原始数据；
- 单测：`tests/pinyin_test.cpp` 覆盖 `nǚ→nv3`、`lǜ→lv4`、`lüè→lve4`、`zhōng→zho1ng`、`ǹg→n4g`。

---

## 8. 实现落地状态（2026-11-16）

D2=A 拍板后已把本报告的规则全部落到代码，并实地编译/验证：

| 落点 | 文件 | 状态 |
|---|---|---|
| 共用归一化 | `engine/include/rospinyin/pinyin.hpp`、`engine/src/pinyin.cpp` | ✅ 带调→数字调、`ü` 族→`v`、音节提取，编译进 `rospinyin-engine`，mkdat 与引擎共用同一实现 |
| dat 格式 | `engine/include/rospinyin/dat_format.hpp` | ✅ v1，定长头 + 两定长表 + 字符串池，mmap 友好，小端 32 位 |
| 编译器 | `tools/mkdat/mkdat.cpp` + `CMakeLists.txt` | ✅ 解析 Unihan（`kMandarin`/`kHanyuPinlu`）+ hanziDB CSV（`charcter` 容错），多音字保序、pinlu 定首选音 |
| 可复现取数 | `tools/mkdat/fetch_data.py`、`tools/mkdat/build_dat.sh` | ✅ 固定 URL + sha256，一键出 `rospinyin.dat` |
| 运行时读取 | `engine/include/rospinyin/table.hpp`、`engine/src/table.cpp` | ✅ 全边界检查加载 dat；按音节查字，按 rank 排序 |
| 引擎骨架 | `engine/include/rospinyin/pinyin_session.hpp`、`engine/src/pinyin_session.cpp` | ✅ 最长匹配+回溯切分（含 `'` 硬边界、连续输入）、`v` 键候选、空格/数字上屏、退格/Esc |
| 测试 | `tests/pinyin_test.cpp`、`pinyin_session_test.cpp`、`check_mkdat.py` | ✅ 宿主 `ctest` **6/6 通过**（含 M1 三项回归） |

实地编译结果（Unihan 18.0.0 + hanziDB.csv）：

```
mkdat: 44355 chars, 418 syllables, 8928-byte string pool
mkdat: warnings=2 skipped=0          # 两个兼容表意文字 pinyin 为空，预期过滤
mkdat stats: ranked=9898 unknown=34457 polyphonic=109
rospinyin.dat = 882 KB（目标 < 2 MB ✅）
两次独立编译 sha256 一致（字节级可复现）
```

端到端验证：加载真实 dat 后 `zhongguo` → 切分为 `[zhong][guo]`，末音节候选首选 `国`（U+56FD）；
`nv` → 女（U+5973，rank 224）、`lv` → 律（U+5F8B，rank 526）、`lve` → 略（U+7565，rank 704）。

版本标识已从 `0.0.1-m1` 升为 `0.0.2-m2-dev`（`version.hpp` 与 `ime/rospinyin_res.rc` 同步）。

---

## 9. M2 剩余部分落地（2026-11-16）

| 落点 | 文件 | 状态 |
|---|---|---|
| 候选分页 | `engine/include/rospinyin/candidate_pager.hpp`、`engine/src/candidate_pager.cpp`、`tests/candidate_pager_test.cpp` | ✅ 9 候选/页、翻页、数字按当前页解释、越界安全 |
| dat 延迟加载 | `ime/dat_loader.hpp/.cpp` | ✅ `GetSystemDirectoryW` 定位 `rospinyin.dat`；损坏/缺失时回退内置表；不在 DllMain 调用 |
| 会话管理 | `ime/session_store.hpp/.cpp` | ✅ 按 HIMC 持有 `PinyinSession` + `CandidatePager`；Win32 `CRITICAL_SECTION`，不引入 `libwinpthread` |
| UI 桥接 | `ime/ime_ui.h`、`ui/ime_ui.cpp` | ✅ `UiShow`/`UiHide`/`UiDestroy`，函数内静态 store，惰性建窗 |
| 窗口层 | `ui/ui_window.*`、`ui/composition_window.*`、`ui/candidate_window.*` | ✅ 组合串音节下划线、候选窗 9 项编号与页码、跟随插入符、边缘避让、CJK 字体兜底链 |
| IMM32 入口 | `ime/imm_entry.cpp` | ✅ M1 passthrough → M2 键处理；`ImeSelect(TRUE)` 调用 `on_select(true)`（关键集成修复） |
| 构建/CI | `ime/CMakeLists.txt`、`tools/syntax_stub/windows.h`、`.github/workflows/ci.yml`、`tools/check_m1.py` | ✅ 接入 engine 与 UI 源、扩展语法桩、CI 生成并发布 dat、contract 断言随 M2 语义更新 |
| VM 验收 | `docs/milestones/m2/M2-ACCEPTANCE.md`、`m0/share/rospinyin.dat` | 🧪 操作单与数据就位，待新 `.ime` 产出后执行 |

验证结果：

```text
宿主 g++ -fsyntax-only（ime + ui + installer）: PASS
cmake build: PASS
ctest: 7/7 PASS（smoke, session, pinyin, pinyin-session, candidate-pager, m1-contract, mkdat-e2e）
真实 dat 加载: 44355 entries
zhongguo -> syl=2 cand=136 top=U+56FD(国)
nv       -> syl=1 cand=18  top=U+5973(女)
lv       -> syl=1 cand=122 top=U+5F8B(律)
lve      -> syl=1 cand=26  top=U+7565(略)
nve      -> syl=1 cand=8   top=U+8650(虐)
zhong    -> syl=1 cand=108 top=U+4E2D(中)
rospinyin.dat: 902768 bytes, sha256 ce7ca6e0185c5cbd09146f77dbf16ebe29a2f44ba52990fa6a34af1466b9ddfd
```

> ⚠️ **上面这段是 2026-11-16 的历史运行日志**，不是当前指纹。此后做过一次数据修订，
> 现在的表是 **902,776 B / sha256 `f28b2606338431d2…a23e9b21`**（`alpha0.0.2` 发布物）。
> 两个数字都真实存在过，引用时别拿错；`tools/mkdat/build_dat.sh` 现在会在末尾断言这个指纹。

> 关键教训：`PinyinSession::feed()` 在 `on_select(true)` 之前是 no-op。
> M2 IMM32 集成最初漏掉 `ImeSelect(TRUE)` 里的激活调用，导致「IME 吃掉
> 拼音键但永远不出组合串」。已在 `tools/check_m1.py` 加静态断言防回归。

## 10. 路径补注（2026-11-16 框架抽取）

本文写作时的路径在 2026-11-16 的框架抽取重构后发生如下迁移，功能不变：

| 旧路径 | 新路径 |
|---|---|
| `engine/include/rospinyin/engine.hpp`、`engine/src/session.cpp` | `framework/include/imfw/engine.hpp`、`framework/src/engine.cpp`（含 `Result.segments`） |
| `engine/include/rospinyin/dat_format.hpp` | `framework/include/imfw/dat_format.hpp`（通用码表格式） |
| `engine/include/rospinyin/table.hpp`、`engine/src/table.cpp` | `framework/include/imfw/code_table.hpp`、`framework/src/code_table.cpp`（`CodeTable`，键名由 dat SylEntry 提供） |
| `engine/include/rospinyin/candidate_pager.hpp` 等 | `framework/include/imfw/candidate_pager.hpp` |
| `engine/include/rospinyin/pinyin*.hpp`、`engine/src/pinyin*.cpp` | `pinyin/include/rospinyin/`、`pinyin/src/` |
| `ime/imm_entry.cpp` 等 host 文件 | `framework/host/` |
| `ui/*.cpp` 组合串/候选窗 | `framework/ui/` |
| `ime/dat_loader.cpp` 的内置码表 | `pinyin/src/scheme.cpp`（`PinyinScheme::load_fallback`） |

`mkdat` 仍按本文第 4-7 节的格式与规则编译 `rospinyin.dat`，dat 内容未变。
