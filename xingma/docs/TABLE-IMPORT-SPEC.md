# 码表导入格式规范（RosXingMa Table Import Spec）

- 适用：`rosxingma-config.exe`（GUI 导入向导）、`rosxingma-table.exe`（CLI）、`xingma/tools/table-convert/`（PC 端脚本）
- 版本：v1.1（对应 `xingma/XINGMA-PLAN.md` 正式版 1.0 与 `shape.dat` v2；Rime 仅 PC 端脚本、fcitx 4 不支持）
- 原则：**宽松识别、严格校验、绝不静默丢数据**。任何被跳过的行都必须能解释原因并导出。

---

## 1. 统一中间模型

所有格式都先转换为同一种中间记录，再交给 dat 写入器：

```c
struct RawEntry {
    std::string code;     // 已小写；仅含字母表内字符
    std::u32string text;  // 单字或词
    int32_t rank;         // 越小越靠前（已归一化）
    uint8_t type;         // 0=单字 1=词
    uint8_t flags;        // bit0=禁止参与输入（仅组词用）
    uint32_t src_line;    // 源行号，用于报错定位
};
```

归一化后按 `(code, rank, text)` 排序，同 `(code,text)` 保留最小 rank，写入 `shape.dat` v2 的 `Code[]` + `Entry[]` + 字符串池。

**角色分工**

| 环节 | 谁来做 | 为什么 |
|---|---|---|
| 规范 CSV / TSV / 空白 TXT | 设备端 `libtableio`（C++） | 首选交换格式，零依赖 |
| fcitx 5 文本（`[Table]` + `[Data]`） | 设备端 | 纯文本，解析成本低 |
| ibus-table 源表（`BEGIN_DEFINITION`） | 设备端 | 纯文本，跳过字根提示段即可 |
| **Rime `*.dict.yaml`** | **PC 端 `xingma/tools/table-convert/rime2csv.py`** | 需 YAML 子集 + `import_tables` 递归合并；设备端引入 YAML 解析不划算（决策：只给脚本） |
| ibus `.db`（sqlite）、`.txt.bz2` | PC 端脚本 | IME 侧不引入 sqlite/bzip2 |
| fcitx4 二进制 `.mb` | —— | **永久不支持**（私有二进制格式，不做反解析） |

> 设备端**不做** Rime 解析，但**要识别**（见第 7 节）：识别为 Rime 时明确提示「请先用 `rime2csv.py` 转换」，而不是抛「格式未知」。

---

## 2. 规范 CSV（首选交换格式）

```csv
# 以 # 开头的行是注释；空行忽略
code,text,weight,type
a,工,1000,single
a,戈,999,single
aa,式,900,single
tgfh,自行车,500,word
```

| 列 | 必填 | 规则 |
|---|---|---|
| `code` | 是 | 1–`max_code_len` 个字母（默认仅 `[a-z]`）；自动小写；含字母表外字符 → 该行拒绝 |
| `text` | 是 | UTF-8，1–32 个码点；禁止 NUL 与控制字符（`\t`/`\n` 由引号包裹时也禁止） |
| `weight` | 否 | 非负整数；**越大越靠前**；缺省 = 行序（先出现者优先） |
| `type` | 否 | `single` / `word` / `auto`；缺省 `auto` 按码点数推断（1=单字，≥2=词） |

**解析细则**

- RFC 4180：`"` 包裹、`""` 转义、字段内允许逗号与换行；未加引号时不做转义。
- 表头可选：若首行含 `code` 与 `text`（大小写不敏感）则识别为表头。
- 列序猜测：无表头时若「第二列像编码、第一列像文本」则自动交换（兼容「字 码」顺序）。
- 多编码同一字：直接写多行（如 `a,工` 与 `aaaa,工`），这是简码的正常表达方式。
- 编码中的 `z`：若该表字母表不含 `z`，`z` 保留为通配键，出现在 `code` 中 → 该行拒绝并报告（可用 `--allow-wildcard-code` 放宽为「仅前缀通配」）。

---

## 3. TSV / 空白分隔 TXT

- 分隔符：`\t` 优先；否则连续空格。同一文件内必须一致（混用即报错并给出首个冲突行号）。
- 列序：`code <ws> text [<ws> weight]`，或 `text <ws> code [<ws> weight]`（自动判定）。
- 注释：行首 `#` 或 `;`（仅当该行不含合法编码时按注释处理，避免误删）。
- 常见来源：极点五笔导出、98 表 `Wubi98-Unicode15.1.txt`。

---

## 4. fcitx 5 文本码表

> **fcitx 4 不支持**（决策已定，不列为待办）。fcitx4 的产物是 `txt2mb` 编译出的二进制 `.mb` + `[CodeTable]` 配置，反解析私有二进制性价比极低；`.mb` 一律拒绝（见第 7、11 节）。

### 4.1 文件构成

| 版本 | 数据文件 | 配置文件 | 结论 |
|---|---|---|---|
| fcitx 4 | `*.mb`（`txt2mb` 编译产物，**二进制**） | `*.conf` 的 `[CodeTable]` 段 | ❌ **不支持**（含二进制与文本 `.mb`） |
| fcitx 5 | `*.txt`（`[Table]` + `[Data]`） | `*.conf` 的 `[Table]` 段 | ✅ 支持，推荐来源 |

> 用户在 fcitx4 里的**文本**源表若能以 `.txt` 导出，通常可被本解析器读入（语法同源），但这不是承诺的支持面；`.mb` 与 `[CodeTable]` 一律拒绝并提示改用文本表。

### 4.2 文本语法

```ini
;fcitx 码表文件
键码=abcdefghijklmnopqrstuvwxy
码长=4
规避字符=;iuv
拼音=@
拼音长度=12
[组词规则]
e2=p11+p12+p21+p22
[数据]
a 工
a 戈
@a 阿
^abc 某根
```

**头部键**（中文键为历史原因，同时接受英文别名）

| 中文键 | 英文别名 | 用途 |
|---|---|---|
| `键码` | `KeyName` / `Alphabet` | 编码字母表 → `alphabet` |
| `码长` | `MaxCodeLength` | 最大码长 → `max_code_len` |
| `规避字符` | `ForbiddenChars` | 记录但不参与导入（仅提示） |
| `拼音` | `PinyinKey` | 该前缀行是拼音反查条目 → **跳过并计数** |
| `拼音长度` | `PinyinLength` | 同上 |

**段名**：`[数据]` / `[Data]` 为数据段；`[组词规则]` / `[Rule]` / 其它段一律跳过（不实现自动组词）。

**数据行**：`code<空白>text[<空白>freq]`

- 行首 `@`：拼音条目（跳过，计入 `skipped_pinyin`）。
- 行首 `^`：仅用于组词的字根条目（导入但置 `flags.bit0=1`，不参与输入候选）。
- 空行、`;`/`#` 开头行忽略。
- fcitx5 的 `[Data]` 行可能是制表符分隔；与空格分隔等价处理。
- fcitx5 `.conf` 的 `[Table]` 段可提供 `Name`、`MaxCodeLength`、`Language`；存在时优先于数据文件推断。

**编码判定**：fcitx 文本表为 UTF-8；无 BOM 时按 UTF-8 严格解码，失败则尝试 GB18030 并给出警告。

---

## 5. ibus-table 源表

### 5.1 结构

```
### 注释以三个井号开头（单个 # 也可能是数据，需谨慎）
### File header must not be modified
### This file must be encoded into UTF-8.
BEGIN_DEFINITION
UUID = 00000000-0000-0000-0000-000000000000
SERIAL_NUMBER = 20240101
LICENSE = Unlicense
NAME = MyShape
NAME.zh_CN = 我的形码
LANGUAGES = zh_CN
AUTHOR = someone
MAX_KEY_LENGTH = 4
VALID_INPUT_CHARS = abcdefghijklmnopqrstuvwxy
AUTO_SELECT = FALSE
AUTO_COMMIT = FALSE
PINYIN_MODE = FALSE
SINGLE_WILDCARD_CHAR = z
END_DEFINITION

BEGIN_CHAR_PROMPTS_DEFINITION
a 工
b 子
END_CHAR_PROMPTS_DEFINITION

BEGIN_TABLE
a	工	52175
aa	式	51234
tgfh	自行车	2
END_TABLE
```

### 5.2 解析规则

| 段 | 处理 |
|---|---|
| `BEGIN_DEFINITION` / `END_DEFINITION` | `KEY = VALUE`，也接受 `KEY == VALUE`（值追加）；键名大小写不敏感 |
| `BEGIN_TABLE` / `END_TABLE` | 数据行 `tabkey<TAB>phrase<TAB>freq`；分隔符为制表符（兼容多空格） |
| `BEGIN_CHAR_PROMPTS_DEFINITION` | **跳过**。字根提示可能含助记口诀，涉版权且体积无收益 |
| 其它 `BEGIN_*` 段 | 跳过 |

**字段映射**

| 字段 | 用途 |
|---|---|
| `NAME` / `NAME.<lang>` | 默认显示名（`NAME.zh_CN` 优先） |
| `LICENSE` | 预填「授权依据」输入框（仅作提示，不校验） |
| `MAX_KEY_LENGTH` | `max_code_len`；与数据最大值不一致时取较大者并警告 |
| `VALID_INPUT_CHARS` | `alphabet`（去重、去非字母） |
| `SINGLE_WILDCARD_CHAR` / `MULTI_WILDCARD_CHAR` | 记录；本期不支持多字符通配，给出警告 |
| `UUID` / `SERIAL_NUMBER` | 存入 `SourceName`，便于溯源 |

**不支持**：`RULES`（自动组词规则）、`USER_CAN_DEFINE_PHRASE`、`.db`（sqlite）。

**.db 转换（PC 端）**：`ibus-table-createdb` 产物为 sqlite，表结构含 `main.db`/`phrases` 等；脚本用 `sqlite3` 标准库读取 `phrases(tabkeys, phrase, freq)` 导出规范 CSV 后导入。

---

## 6. Rime `*.dict.yaml`（仅 PC 端脚本）

**决策：设备端不解析 Rime，只识别并引导。** 原因是 YAML 需要子集解析器 + `import_tables` 递归合并 + 权重语义，成本与风险都高于「让用户先跑一条命令」。

```yaml
name: shape86
version: "1.0"
sort: by_weight
columns:
  - text
  - code
  - weight
...
工	a	1098420
式	aa	1098418
自行车	tgfh	500
```

**设备端行为（`libtableio`）**

- 嗅探阶段识别为 Rime（见第 7 节）后，**不解析内容**，直接返回退出码 `6`，提示：
  `检测到 Rime 字典，请先运行 xingma/tools/table-convert/rime2csv.py <dict.yaml> -o table.csv，再导入 table.csv`。
- 配置工具 GUI 在同样的判定下弹出该提示，提供「复制命令」按钮；**不**内嵌 Python，**不**静默跳过。

**PC 端脚本 `rime2csv.py`**（仅标准库，Python 3.8+）

- 读取 `columns`（缺省 `[text, code, weight]`），支持任意列序。
- `import_tables` 递归合并（循环引用检测、去重、权重保留最大值）。
- `sort: by_weight` 按权重降序；无权重按行序。
- 输出规范 CSV（`code,text,weight,type`），带 BOM 可选、`--report` 输出统计与告警。
- 不支持的语法（patch、锚点/别名、多行字符串）明确报错并指出行号，不猜。

---

## 7. 格式嗅探

按顺序判定，命中即停（`--format` 可覆盖）：

1. **魔数/二进制**：`BZh` → bz2（拒绝，提示用脚本）；`SQLite format 3` → sqlite（拒绝，提示用脚本）；`.mb` 任意形态 → fcitx4（**直接拒绝**，提示改用 fcitx5 文本表）。
2. **内容签名**：
   - 含 `BEGIN_DEFINITION` → ibus-table
   - 含 `[数据]` / `[Data]` / `键码=` / `码长=` → fcitx 文本（按 4.2 解析）
   - 含 `columns:` 且含 `...` 行 → Rime（**识别后拒绝**，引导 `rime2csv.py`）
   - 含 `---` 开头的 YAML 文档头 → 同 Rime 处理
3. **扩展名**：`.csv` / `.tsv` / `.txt` / `.yaml`（`.mb` 直接走第 1 条拒绝）。
4. **分隔符统计**：首个非注释行里 `,` / `\t` / 连续空格 的列数一致性，取最稳定者。
5. 全部失败 → 退出码 6（不支持的格式），报告里给出「看起来像什么」的猜测与实际前 3 行。

**编码嗅探**：BOM（UTF-8/UTF-16LE/BE）→ 严格 UTF-8 校验 → GB18030；三者皆失败则拒绝（绝不带乱码入库）。

---

## 8. 权重归一化

| 来源 | 原语义 | 归一化 |
|---|---|---|
| 规范 CSV `weight` | 大 = 靠前 | `rank = 65535 - clamp(weight,0,65535)` |
| fcitx `freq` | 大 = 靠前 | 同上 |
| ibus/SCIM `freq` | 大 = 靠前（上限 65535） | 同上 |
| Rime `by_weight`（PC 端脚本内处理） | 大 = 靠前 | 同上 |
| 无权重列 | 行序 | `rank = 行号`（先出现者优先） |

同一 `(code, text)` 多次出现：取最小 rank。同一 `code` 下候选按 `(rank, text 码点)` 升序，保证跨平台确定性。

---

## 9. 校验规则（导入即拒，不留半成品）

| # | 检查 | 失败处理 |
|---|---|---|
| 1 | `code` 非空、长度 ≤ `max_code_len`、字符 ⊆ `alphabet` | 该行拒绝，计入 rejects |
| 2 | `text` 非空、≤32 码点、无控制字符 | 该行拒绝 |
| 3 | 解析后有效条目 = 0 | **整体中止**（退出码 3） |
| 4 | `alphabet` 去重后 < 2 个字符 | 整体中止（多半是解析错段） |
| 5 | `max_code_len` ∉ [1,8] | 整体中止 |
| 6 | 单行 > 64 KB | 该行拒绝 + 告警 |
| 7 | 总行 > 200 万 或 源文件 > 64 MB | 整体中止 |
| 8 | 编码含通配键位置非法 | 该行拒绝 |
| 9 | 同 `(code,text)` 重复 | 去重，保留最小 rank，计数 |

**导入报告**（`reports\<id>.log` + GUI 摘要）

```
源文件：/home/u/shape86.txt
格式：fcitx5（自动判定）  编码：UTF-8  分隔符：TAB
总行：89,272  有效：89,150  去重：96  跳过：26
最大码长：4  字母表：25 字符  编码数：25,631  单字：21,004  词：68,146
跳过明细（前 20）：
  行 31：编码含非法字符 '1'
  行 205：文本为空
  ...
耗时：0.42 s
```

---

## 10. 反导出

| 目标 | 说明 |
|---|---|
| 规范 CSV | 全量、保序（按 `code, rank`），四列 `code,text,weight,type`；`weight = 65535 - rank` |
| fcitx 文本 | 输出 `键码/码长/[数据]` 结构；`freq = 65535 - rank` |
| ibus-table 源表 | 输出 `BEGIN_DEFINITION`（NAME/LICENSE/MAX_KEY_LENGTH/VALID_INPUT_CHARS）+ `BEGIN_TABLE` |

导出目的：**不锁定用户数据**。导出结果应能被对应框架直接回读（W4 验收项）。

---

## 11. 错误码

| 码 | 含义 | 典型原因 |
|---|---|---|
| 0 | 成功 | — |
| 1 | 用法错误 | 参数缺失/冲突 |
| 2 | 解析失败 | 结构错乱、编码无法识别 |
| 3 | 校验失败 | 有效条目为 0、码长非法 |
| 4 | IO 失败 | 读不到文件、目标目录不可写 |
| 5 | 已存在 | 同 id 或同内容已导入（需 `--force`） |
| 6 | 不支持的格式 | 二进制 `.mb`、`.db`、bz2；**识别为 Rime 字典**（提示先跑 `rime2csv.py`） |
| 7 | 权限/路径问题 | `%APPDATA%` 不可写 |

---

## 12. 安全边界

- 导入数据视为**不可信输入**：流式解析、单行上限、总量上限、拒绝路径穿越（`id` 白名单 `[a-z0-9][a-z0-9_-]{0,31}`）。
- 不执行任何来自源文件的格式化串/模板/脚本；不做正则替换。
- 不写 `system32`；只写 `%APPDATA%\RosXingMa\`；不需要管理员。
- dat 写入走「临时文件 → `FlushFileBuffers` → `MoveFileExW(REPLACE_EXISTING)`」，中途失败不留半成品。
- 解析失败**绝不**部分写入注册表；注册表顺序为 `Tables\<id>` → `ActiveTable` → `Revision`。

---

## 13. 测试 fixture 规范

仓库内 **不得** 出现任何真实码表（哪怕片段）。`xingma/tools/table-config/tests/fixtures/` 与 `tests/fixtures/` 全部自造：

| 文件 | 内容 |
|---|---|
| `demo.csv` | 约 60 条自造编码（`a 一`、`ab 二`…），覆盖单字/词/多编码/重复 |
| `demo.tsv` | 同数据 TSV 形态 |
| `demo.fcitx.txt` | 模拟 fcitx 头部（`键码`/`码长`/`[数据]`）+ `@`/`^` 前缀行 |
| `demo.ibus.txt` | `BEGIN_DEFINITION` / `BEGIN_TABLE` 结构 |
| `demo.dict.yaml` | `columns: [text, code, weight]` + `...` 数据段 |
| `demo.gb18030.txt` | 用于编码嗅探测试（内容同为自造） |

`table_io_test.cpp` 的核心断言：**五种格式解析出的 `(code,text,rank)` 集合完全一致**。真实码表只在用户本机手工验证，不进仓库、不进 CI。
