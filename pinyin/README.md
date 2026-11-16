# pinyin/ — 拼音引擎插件

RosPinyin 的全部拼音知识都在这里；框架（`framework/`）不含任何拼音逻辑。

| 文件 | 内容 |
|---|---|
| `include/rospinyin/pinyin.hpp` + `src/pinyin.cpp` | 读音归一化：带调符号 → 声调数字紧随韵母（`zhōng → zho1ng`）、`ü/ǖǘǚǜ → v`（D2=A + v 键规则）、`ń/ǹ/ḿ` 音节鼻音；mkdat 与运行时共用 |
| `include/rospinyin/pinyin_session.hpp` + `src/pinyin_session.cpp` | `PinyinSession : imfw::Engine`：连续拼音切分（最长匹配 + 回溯 + `'` 硬边界）、候选按字频排序、`Result.segments` 携带音节视图 |
| `include/rospinyin/scheme.hpp` + `src/scheme.cpp` | `PinyinScheme : imfw::Scheme`：`a–z` + `'` 字母表（VK_OEM_7）、`L"rospinyin.dat"`、窗口类名 `RosPinyin*`、工具条标签 `拼`、内置兜底码表（dat 缺失时常用 ~500 字可用） |
| `include/rospinyin/version.hpp` | RosPinyin 产品版本（与 `ime/rospinyin_res.rc` 同步） |

复用者只需仿照 `scheme.cpp` 实现另一套 `imfw::Scheme`（详见 `framework/README.md`）。
