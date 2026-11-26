# M4 词组库 `rospinyin-lexicon.dat`

## 定位

词组库是**内嵌必装数据**（2026-11-26 M4 落地）：二进制不进 Git，但由 `tools/install-rospinyin/pack_payload.py` 作为第 4 个 RCDATA payload 内嵌进 GUI 安装器，与 `rospinyin.ime` / `rospinyin.dat` / CLI 一同写入 `system32\`。GUI 与 CLI 安装器都写它、校验固定 SHA-256、卸载时清理它；缺失或损坏时 IME 拒绝加载（不产候选、不崩溃）。

> **破坏性升级警示**：v0.1.0 存量安装只有 ime+dat、没有词库。覆盖安装后必须重启；若词库未正确写入，IME 将不产任何候选而非退回单字。owner 已接受该口径。

词库二进制仅作为独立 release 资产另行发布，供 wget+copy 诊断降级路径使用；正常安装路径一律走安装器内嵌 payload。

- 格式：dat v2，`ROSPYDAT`，version 2；字符表/音节表计数均为 0，只有 `LexEntry` / `LexCode` 两表与字符串池。
- 许可证：脚本 MIT；词条来自 **CC-CEDICT（CC-BY-SA）**，必须与主项目分开分发并保留署名。
- 数据来源固定快照：MDBG `cedict_1_0_ts_utf-8_mdbg.txt.gz`，SHA-256 `9594e10b88f5dde3003133f20f216ff9ae66e9f4441ee94f13b56d14e4043118`。

## 构建

```bash
python3 tools/mklex/build_lexicon_dat.py --fetch \
  --out m0/share/rospinyin-lexicon.dat
```

也可传本地同哈希的 `.gz`。脚本拒绝哈希不符的快照；`data/weights.txt` 只给少量验收常用词显式 rank（你好/计算机等），其余按首次出现顺序给 rank。输出是确定性的：同一输入逐字节一致。

当前快照构建结果：109,428 词 / 102,105 个键 / 6,530,601 字节，SHA-256 `a89ec5e500c47c121687e9789fade9b8187c1ec2b69d25d5e82f1a680cfc1c6a`。

## 运行时行为

- `nihao` → 首选「你好」；`jisuanji` → 首选「计算机」；提交时整词覆盖全部音节。
- 词组候选排在单字之前（score 带词档次），单字仍是兜底；`[快]/[全]` 的字形过滤继续按 dat tier 位工作。
- 词库缺失 / 截断 / 结构损坏 / SHA-256 不符四类失败时，`load_system_table()` 返回 nullptr；release 构建（`ROSPINYIN_REQUIRE_SYSTEM_TABLE`）不回退内置表、返回空 `CodeTable`（不崩溃、不产候选），trace 写明失败原因。宿主/诊断配置保持内置表回退，保证两配置 ctest 全绿。

## 部署与校验

- 构建期：`pack_payload.py` 读 `m0/share/rospinyin-lexicon.dat`，zlib-9 压缩后写入 `payload_base64.hpp`（C++ 字节宏 + RCDATA 转义串）；CI 不重生成，头文件是入库唯一真源。词库的 RC 转义串拆为 8 段（`_0.._7`，资源 ID 104–111）以规避 windres 对单个 116k 字面量字符串的 O(n²) 卡死；GUI 按 ID 顺序拼接后解压。
- 安装期：GUI 解压 4 个 payload 到私有 `%TEMP%`，CLI `/step1` 逐一做 size + SHA-256 双校验后才写注册表；词库额外与固定值 `a89ec5e5…c1c6a` 比对。
- 运行期：`lexicon_guard.hpp` 的 `classify()` 纯函数判定四态，宿主 ctest 覆盖；IME 只在 `Ok` 时加载词组。
- 回验：`check_m1.py` 断言 4 个 RCDATA 与固定哈希；`extract_payload.py` 从生成头还原词库，CI release 上传并 `sha256sum -c`。
- `/status` 显示词库存在状态与 `lexicon sha : … (OK/MISMATCH)`。
- 词内字音一致性由 key 保证：key 是 CC-CEDICT 读音去空格后的 `ni3hao3`，运行时用与多音字相同的 `syllable_of` 去声调变成 `nihao`。

## 验收

宿主侧 `ctest` 覆盖：`code-table` 验证词表解析/排序/去声调查询，`pinyin-session` 验证二字词与三字词的候选顺序与整词提交。相对 M2 单字首选率 46.2% 基线，M4 的新增指标是**词组验收样本 2/2 首选命中**；该指标不替代单字统计口径。
