# pinyin/ — 拼音引擎插件

RosPinyin 的全部拼音知识都在这里；框架（`framework/`）不含任何拼音逻辑。

| 文件 | 内容 |
|---|---|
| `include/rospinyin/pinyin.hpp` + `src/pinyin.cpp` | 读音归一化：带调符号 → 声调数字紧随韵母（`zhōng → zho1ng`）、`ü/ǖǘǚǜ → v`（D2=A + v 键规则）、`ń/ǹ/ḿ` 音节鼻音；mkdat 与运行时共用 |
| `include/rospinyin/pinyin_session.hpp` + `src/pinyin_session.cpp` | `PinyinSession : imfw::Engine`：连续拼音切分（最长匹配 + 回溯 + `'` 硬边界）、候选按字频排序、`Result.segments` 携带音节视图 |
| `include/rospinyin/dat_source.hpp` + `src/dat_source.cpp` + `host/dat_source_win.cpp` | **方案自管数据**（G2 终局）：优先读系统目录 `rospinyin.dat`（Win32 实现随 .ime 编译），缺失/损坏时回退内置兜底表（宿主测试用弱符号兜底）；`reload_data()` 热重载换代 |
| `include/rospinyin/scheme.hpp` + `src/scheme.cpp` | `PinyinScheme : imfw::Scheme`：`a–z` + `'` 字母表（VK_OEM_7）、窗口类名 `RosPinyin*`、工具条标签 `拼`、自持 `shared_ptr<const CodeTable>`（构造时加载、`reload_data()` 换代，旧会话经 shared_ptr 存活）、内置兜底码表（dat 缺失时常用 ~500 字可用） |
| `include/rospinyin/version.hpp` | RosPinyin 产品版本（与 `ime/rospinyin_res.rc` 同步） |

框架不加载/不持有/不传递码表——数据完全归本插件所有（`framework/README.md`「数据所有权」）。复用者只需仿照 `scheme.cpp` + `dat_source.*` 实现另一套 `imfw::Scheme`（详见 `framework/README.md`）。
