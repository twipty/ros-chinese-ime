# M4 可选词组库 `rospinyin-lexicon.dat`

## 定位

词组库是**独立分发的可选数据**，不进 Git、不内嵌 GUI 安装器、不作为 alpha/v0.1 release 的必需资产。主码表 `rospinyin.dat` 仍单独工作（M2 单字行为不变）；把本文件放到同一 `system32` 目录后，输入法在启动时额外加载它。

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
- 词库文件缺失、损坏或 magic/version 不符时静默跳过，不回退主码表、不崩溃。
- 词内字音一致性由 key 保证：key 是 CC-CEDICT 读音去空格后的 `ni3hao3`，运行时用与多音字相同的 `syllable_of` 去声调变成 `nihao`。

## 验收

宿主侧 `ctest` 覆盖：`code-table` 验证词表解析/排序/去声调查询，`pinyin-session` 验证二字词与三字词的候选顺序与整词提交。相对 M2 单字首选率 46.2% 基线，M4 的新增指标是**词组验收样本 2/2 首选命中**；该指标不替代单字统计口径。
