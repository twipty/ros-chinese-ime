# 可选数据包规范（RosXingMa Data Packs）

- 适用：86 版（MIT 源）、98 版（Unlicense 源）两份可选数据包
- 关联：`xingma/XINGMA-PLAN.md` 6.5 / 7、`xingma/docs/TABLE-LICENSE-RESEARCH.md`、`NOTICE.txt` 第 8 节
- 工具：`rosxingma-config.exe`（GUI）、`rosxingma-table.exe pack install|verify|uninstall`（CLI）、`xingma/tools/mkdat-shape/`（构建）

---

## 1. 定位

数据包是**可选下载的数据单元**，不是安装程序，也不是主安装包的一部分。它做两件事：

1. 把码表数据装成 `shape.dat`（与用户自己导入的表走完全相同的存储与切换路径）；
2. 通过 `pack.ini` 把「名称、码长、字母表、通配键、许可证、来源、上游提示」等预配置信息一次带进去，用户不需要在界面里手填任何技术参数。

**分发原则**

- 与主安装包**分开授权单元**：各自目录、各自 LICENSE、各自 NOTICE，可整体替换或删除。
- 主安装包、安装向导**都不包含**数据包，也没有「一键获取」按钮；工具不内嵌下载地址。
- 下载得到 `.zip`，解压后是一个目录；配置工具导入这个目录（或其中的 `pack.ini`）。

---

## 2. 包结构

```
RosXingMa-Pack-Shape86-v1.0.0/
├── pack.ini          必需，UTF-8 预配置清单（由仓库的 pack.ini.in 生成）
├── table.csv         必需，规范 CSV（构建期由上游表转换而来）
├── LICENSE           必需，上游许可证全文
├── NOTICE.md         必需，署名/来源/上游权利提示
└── SHA256SUMS        必需，覆盖 table.csv、LICENSE、NOTICE.md
```

- 仓库只保存 `pack.ini.in`（模板）、`LICENSE`、`NOTICE.md`；`pack.ini` 与 `table.csv` 在构建期生成，受 `.gitignore` 约束。原因见 `xingma/packs/README.md`：`DataSha256` 与条目数只有构建时才知道，提前提交等于提交一个错的哈希。
- 数据文件一律使用**规范 CSV**（`code,text,weight,type`）。上游是 Rime/fcitx/ibus 格式的，在**构建期**用 `xingma/tools/table-convert/*.py` 转换（与「Rime 只给脚本、设备端不解析」的决策一致）；设备端只走 W1 的 CSV 读取器，不依赖 W3/W4 的格式适配器。
- 包内文件必须是**相对路径**；不允许 `..`、绝对路径、符号链接、可执行文件。安装器只读不执行。
- 目录名建议 `RosXingMa-Pack-<Name>-v<Version>`，与 `PackId`/`PackVersion` 对应，便于人工核对。

---

## 3. `pack.ini` 规范

### 3.1 语法

- UTF-8（允许 BOM），行尾 `\n` 或 `\r\n`。
- `;` 或 `#` 开头为注释；`[Section]` 段名大小写不敏感；`Key = Value`，等号两侧空白忽略，值不需引号、不做转义、不去除内部空格。
- 重复键：后者覆盖，并记一条警告。
- 未知段/未知键：忽略并警告（向前兼容）；**未知 `FormatVersion` 直接拒绝**。
- 单行上限 4 KB，清单总大小上限 64 KB。

### 3.2 字段

#### `[Pack]`

| 键 | 必填 | 说明 |
|---|---|---|
| `FormatVersion` | 是 | 当前仅 `1` |
| `PackId` | 是 | `[a-z0-9][a-z0-9_-]{0,31}`，全局唯一；用于升级判定与去重 |
| `PackVersion` | 是 | `主.次.修订`，如 `1.0.0`；升级只比大小，不做区间约束 |
| `DisplayName` | 是 | 默认显示名（用户安装后可改名，不影响 `PackId`） |
| `Description` | 否 | 一句话说明，显示在详情页 |
| `Author` | 否 | 打包者/上游作者署名 |
| `Homepage` | 否 | 项目或上游主页 |
| `BuildDate` | 否 | `YYYY-MM-DD` |
| `UpstreamFormat` | 否 | 上游原始格式（`rime` / `fcitx` / `ibus` / `csv`），仅供溯源 |

#### `[License]`

| 键 | 必填 | 说明 |
|---|---|---|
| `License` | 是 | SPDX 标识：`MIT` / `Unlicense` |
| `LicenseFile` | 是 | 包内相对路径，通常 `LICENSE` |
| `SourceUrl` | 是 | 上游仓库/发布页 URL |
| `SourceCommit` | 否 | 固定 commit（40 位或缩略），构建脚本写入 |
| `UpstreamNotice` | 否 | 上游权利提示，**安装与详情页原样展示**，不得改写 |
| `Redistributable` | 是 | `yes` / `no`；为 `no` 的包不得发布 |
| `Warranty` | 是 | 固定 `none`：本项目不对数据作任何担保 |

#### `[Table]`

| 键 | 必填 | 说明 |
|---|---|---|
| `SourceFormat` | 是 | 本规范固定 `csv` |
| `DataFile` | 是 | 包内相对路径，通常 `table.csv` |
| `DataSha256` | 是 | 数据文件 SHA-256（小写 hex），不符即拒绝 |
| `EntryCountExpect` | 否 | 期望条目数，偏差 >1% 报警告（防截断/半包） |
| `MaxCodeLen` | 是 | 1–8；与数据中实际最大码长冲突即**报错** |
| `Alphabet` | 是 | 编码字母表（如 `abcdefghijklmnopqrstuvwxy`） |
| `Wildcard` | 否 | 通配键；留空表示按「`z` 不在字母表中则 `z` 为通配」推导 |
| `Columns` | 否 | 字段映射，如 `text,code,weight`；缺省按规范 CSV 列序 |

#### `[Install]`

| 键 | 必填 | 说明 |
|---|---|---|
| `ActivateIfNone` | 否 | `yes`：当前无表时安装后自动设为当前（仍以用户点「安装」为同意） |
| `DefaultOrder` | 否 | 建议列表排序位，缺省 `100` |
| `ConsentRequired` | 是 | 固定 `yes` |
| `RetainSource` | 否 | `yes` 时在 `packs\<PackId>\` 留存 CSV 副本（默认 `no`） |

### 3.3 校验与拒绝

安装前按序执行，任一步失败即中止且不写任何文件/注册表：

1. `FormatVersion` 已知、必填键齐全、`PackId`/`PackVersion` 格式合法。
2. `DataFile`/`LicenseFile` 为包内相对路径，且真实路径在包目录内（防目录穿越）。
3. `table.csv`、`LICENSE`、`NOTICE.md` 的哈希与 `SHA256SUMS` 一致；`table.csv` 哈希与 `DataSha256` 一致。
4. CSV 解析后：条目数 > 0；实际最大码长 ≤ `MaxCodeLen`；所有编码字符 ⊆ `Alphabet`。
5. `Redistributable=yes`、`License` 在允许清单（`MIT`/`Unlicense`）内。

> `pack.ini` 自身不参与校验（无签名，自校验无意义）。因此**来源可信性靠 HTTPS 下载 + 用户可肉眼审查包内 CSV** 保证，这一点在 NOTICE 中如实说明，不宣称「已签名/已验证」。

---

## 4. 两份实际数据包

以下为 v1 发布内容，与仓库内 `xingma/packs/<id>/pack.ini.in` 一致。`DataSha256` 与 `EntryCount` 由构建脚本写入（下例以占位符表示），发布时以 `SHA256SUMS` 为准。

### 4.1 86 版（MIT 源）

```ini
[Pack]
FormatVersion = 1
PackId = shape86-jidian
PackVersion = 1.0.0
DisplayName = 86 版形码（极点）
Description = 86 版形码表，含单字与词组，共约 8.9 万条。
Author = Kyle Bing（上游） / RosXingMa 项目（打包）
Homepage = https://github.com/KyleBing/rime-wubi86-jidian
BuildDate = 2026-11-16
UpstreamFormat = rime

[License]
License = MIT
LicenseFile = LICENSE
SourceUrl = https://github.com/KyleBing/rime-wubi86-jidian
SourceCommit = 4d08110b8080f6577dad733ada7e5ae5c5e878fd
UpstreamNotice = 本表源自「极点五笔」码表。上游维护者于 2026-10-29 将仓库许可改为 MIT，但其仅能许可自身有权部分，更上游的权利完整性未获担保。本数据包按原样提供，不作任何担保。
Redistributable = yes
Warranty = none

[Table]
SourceFormat = csv
DataFile = table.csv
DataSha256 = {{DATA_SHA256}}
EntryCountExpect = {{ENTRY_COUNT}}
MaxCodeLen = 4
Alphabet = abcdefghijklmnopqrstuvwxy
Wildcard = z

[Install]
ActivateIfNone = yes
DefaultOrder = 10
ConsentRequired = yes
RetainSource = no
```

> `SourceCommit` 为 2026-11-16 抓取的仓库 HEAD；许可证变更本身发生在更早的提交（2026-10-29）。打包以 HEAD 为准，保证可复现。

### 4.2 98 版（Unlicense 源）

```ini
[Pack]
FormatVersion = 1
PackId = shape98-tables
PackVersion = 1.0.0
DisplayName = 98 版形码
Description = 98 版形码表，含单字与词组，共约 11.2 万条。
Author = yanhuacuo（上游） / RosXingMa 项目（打包）
Homepage = https://github.com/yanhuacuo/98wubi-tables
BuildDate = 2026-11-16
UpstreamFormat = rime

[License]
License = Unlicense
LicenseFile = LICENSE
SourceUrl = https://github.com/yanhuacuo/98wubi-tables
SourceCommit = 6b8b6fb9d3c34e0d5e3b17211e1f1c100e7eb697
UpstreamNotice = 上游作者于 2024-01-20 自行将码表置于 Unlicense（公有领域）。权利链相对清晰，但仍建议使用者自行确认对自身用途的适用性。
Redistributable = yes
Warranty = none

[Table]
SourceFormat = csv
DataFile = table.csv
DataSha256 = {{DATA_SHA256}}
EntryCountExpect = {{ENTRY_COUNT}}
MaxCodeLen = 4
Alphabet = abcdefghijklmnopqrstuvwxy
Wildcard = z

[Install]
ActivateIfNone = yes
DefaultOrder = 20
ConsentRequired = yes
RetainSource = no
```

> 98 版的 Unlicense 由作者本人于 2024-01-20 提交 `4dbcaa65` 加入；打包基准为 2026-11-16 的 HEAD。

### 4.3 不发布的部分

| 项 | 原因 |
|---|---|
| 新世纪版数据包 | 公开来源或为转载者给的 GPL，或明写「版权归王码公司、请勿商用」 |
| 字根键位图、助记口诀 | 与码表同源的权利客体，且体积/价值比低 |
| 任何 `王码`/`五笔字型` 名称、图标、安装包资源 | 商标风险 |

---

## 5. 构建流程

```
xingma/tools/mkdat-shape/
  tables.toml        固定 URL + commit + sha256 + 转换规则（源数据不入库）
  fetch_tables.py    下载并校验上游哈希            ← W5 待实现
  make_pack.py       上游 CSV → 填充模板 → SHA256SUMS → zip   ← 已实现（本次）
  build_packs.sh     一键全流程                  ← W5 待实现
```

> `make_pack.py` 已可独立使用（`build` / `verify` 两个子命令），当前入参是**已转换好的规范 CSV**；上游下载与 Rime→CSV 转换两步在 W5 补齐。

步骤：

1. `fetch_tables.py` 按 `tables.toml` 拉取固定 commit 的上游文件，校验 sha256（不匹配即失败，绝不「拉到什么用什么」）。
2. `convert` 阶段把上游表转成规范 CSV：统一小写编码、权重归一化、去重、按 `(code, text)` 排序。转换必须**确定性**（两次构建字节一致），CI 校验。
3. `make_pack.py` 把 `xingma/packs/<id>/pack.ini.in` 复制到 `dist/RosXingMa-Pack-<Name>/pack.ini`，只填四个构建期占位符：`{{PACK_VERSION}}`、`{{BUILD_DATE}}`、`{{DATA_SHA256}}`、`{{ENTRY_COUNT}}`。`MaxCodeLen`/`Alphabet`/`DataFile` 是**声明值**，不允许构建脚本改写——若实际数据与之不符（码长超限、编码越出字母表），构建直接失败，避免把错误参数「自动纠正」进包里。随后生成 `SHA256SUMS`。
4. 打包 zip（存储或压缩均可，zip 仅作运输容器），同时输出 `<zip>.sha256`。
5. 人工抽检：随机 20 条 `code,text` 与上游原表比对；抽查 10 个简码字、5 个词组。

产物：

```
dist/
  RosXingMa-Pack-Shape86-v1.0.0.zip
  RosXingMa-Pack-Shape86-v1.0.0.zip.sha256
  RosXingMa-Pack-Shape98-v1.0.0.zip
  RosXingMa-Pack-Shape98-v1.0.0.zip.sha256
```

---

## 6. 安装、升级、卸载

### 6.1 安装（用户视角）

1. 下载 zip → 解压 → 打开「RosXingMa 码表管理」→「导入码表…」。
2. 选择解压出的目录或其中的 `pack.ini`（文件选择器同时接受两者；也可直接把目录拖到窗口）。
3. 预览页预填好名称、码长、字母表、条目数，并显示：
   - 许可证、作者、来源 URL、固定 commit；
   - `UpstreamNotice` 原文（不得折叠隐藏）；
   - 「第三方数据，本项目不对其权利完整性作担保」声明 + 同意勾选。
4. 点「安装」→ 写 `shape.dat` 与元数据 → 若当前无表且 `ActivateIfNone=yes`，提示并设为当前。

### 6.2 升级

- 同 `PackId`，`PackVersion` 更大 → 列表出现「可升级」，点击走同一流程，保留用户改过的显示名与 `Order`。
- 同版本 → 提示已安装；更小版本 → 默认拒绝，`--allow-downgrade` 可强制。

### 6.3 卸载

删 `tables\<id>.dat`、`Tables\<id>`、`packs\<PackId>\`。若被卸载的是当前表，要求先改选其它表或切到「无表」，不允许留下悬空指针。

---

## 7. 副本留存与溯源

- 默认不留存 CSV（省空间）；`RetainSource=yes` 或安装时勾选「保留源文件」则写 `%APPDATA%\RosXingMa\packs\<PackId>\table.csv`。
- `LICENSE` 与 `NOTICE.md` **一律留存**到 `packs\<PackId>\`，并在「详情」页提供「打开许可证」入口。
- 元数据在注册表中保留 `PackId`、`PackVersion`、`SourceUrl`、`UpstreamNotice`，卸载前一直可查。

---

## 8. 发布清单（Checklist）

- [ ] `tables.toml` 中 URL/commit/sha256 已固定，且注释写明取证日期
- [ ] `LICENSE` 为上游原文（未改写、未精简）
- [ ] `NOTICE.md` 含：上游仓库、commit、许可证、上游权利提示、打包者、与主项目的关系（仅数据、非代码）
- [ ] `pack.ini` 必填项齐全，`DataSha256`/`EntryCountExpect` 与实际一致
- [ ] 两次构建产物字节一致（CI）
- [ ] 抽检 20 条编码 + 10 个简码/词组
- [ ] zip 与 `.sha256` 一并上传，Release 说明中写明「第三方数据、不作担保」
- [ ] 主安装包内**不含**该 zip，安装向导无下载入口
- [ ] 仓库中仅提交 `xingma/packs/<id>/{pack.ini.in,NOTICE.md,LICENSE}`；`table.csv` 与生成的 `pack.ini` 在 `.gitignore`

---

## 9. 与主包的关系（合规表述）

| 资产 | 主包 | 数据包 | 授权 |
|---|---|---|---|
| `rosxingma.ime` / 配置工具 / 格式规范 | ✅ | — | MIT |
| `pack.ini` 规范、构建脚本 | ✅ | ✅（脚本） | MIT |
| 码表数据（`table.csv`） | ❌ | ✅ | 各自 LICENSE（MIT / Unlicense） |
| 新世纪码表 | ❌ | ❌ | 不分发 |

一句话：**主包是 MIT 的软件，数据包是第三方数据；两者的授权单元严格分离，删除数据包不影响软件运行**（软件会退化为「无表」状态并给出提示）。
