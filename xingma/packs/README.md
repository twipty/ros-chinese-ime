# xingma/packs/ — 可选数据包（清单与许可证，不含数据）

本目录存放 86 / 98 两份**可选数据包**的骨架：预配置清单模板、上游许可证原文、署名与免责文本。
**码表数据不进入仓库**：`table.csv` 由构建脚本按固定 commit 拉取并转换，`.gitignore` 已排除。

## 目录内容

```
xingma/packs/
  shape86/
    pack.ini.in   预配置清单模板（{{...}} 由构建期填充）
    LICENSE       上游 MIT 许可证原文（Kyle Bing）
    NOTICE.md     署名、来源、上游权利提示、免责
  shape98/
    pack.ini.in
    LICENSE       上游 Unlicense 原文
    NOTICE.md
```

## 为什么是 `pack.ini.in` 而不是 `pack.ini`

`pack.ini` 里有两个构建期才能确定的值：数据文件的 SHA-256 与条目数。若把 `pack.ini` 直接提交，哈希会与实际数据不符，等于自带一个错误。因此：

- 仓库保存 `pack.ini.in`（模板）；
- `xingma/tools/mkdat-shape/make_pack.py` 在构建时填充 `{{DATA_SHA256}}`、`{{ENTRY_COUNT}}`、`{{MAX_CODE_LEN}}`、`{{ALPHABET}}`、`{{BUILD_DATE}}`、`{{PACK_VERSION}}`，输出 `dist/RosXingMa-Pack-<Name>/pack.ini`。

## 构建与发布

```bash
# 上游若是 Rime dict.yaml，先用 PC 端脚本转成规范 CSV（设备端不解析 Rime）
python3 xingma/tools/table-convert/rime2csv.py rime/shape86.dict.yaml -o build/shape86.csv

# 已实现：由模板 + CSV 生成数据包目录（含 pack.ini / SHA256SUMS / 确定性 zip）
python3 xingma/tools/mkdat-shape/make_pack.py build \
  --pack xingma/packs/shape86 --csv build/shape86.csv \
  --out dist/RosXingMa-Pack-Shape86 --version 1.0.0 --zip

# 校验已生成的包（哈希、必填项、码长/字母表一致性、条目数偏差）
python3 xingma/tools/mkdat-shape/make_pack.py verify --dir dist/RosXingMa-Pack-Shape86
```

`fetch_tables.py`（按固定 commit 拉取上游并校验哈希）与 `build_packs.sh`（一键全流程）属 W5 交付，尚未实现。

发布清单与合规检查见 `xingma/docs/DATA-PACK-SPEC.md` 第 8 节。

## 边界

- 不发布新世纪版数据包（无可用授权）。
- 不含字根键位图、助记口诀、王码名称/图标。
- 主安装包不包含本目录产物，安装向导无下载入口。
