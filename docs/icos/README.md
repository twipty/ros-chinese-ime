# 输入法图标用法（ico/）

`ico/` 存放两款输入法的成品图标：`rospinyin.ico` / `rosxingma.ico` 供 IME 模块资源使用，
六个 PNG 供界面层按场景取用。**总项目图标（rose-im）暂不使用，未纳入本目录。**

## 1. 文件清单

| 文件 | 尺寸 | 用途 |
| --- | --- | --- |
| `ico/rospinyin.ico` | 单帧 16×16 RGBA | RosePinyin 的 IME 模块图标资源（任务托盘 / 语言栏） |
| `ico/rosxingma.ico` | 单帧 16×16 RGBA | RoseXingma 的 IME 模块图标资源（任务托盘 / 语言栏） |
| `ico/pinyin-tray-16.png` | 16×16 RGBA | 任务托盘、指示器 |
| `ico/pinyin-toolbar-24.png` | 24×24 RGBA | 工具条 |
| `ico/pinyin-button-48.png` | 48×48 RGBA | 工具按钮 |
| `ico/pinyin-512.png` | 512×512 RGBA | 大尺寸展示：关于框、官网、文档插图 |
| `ico/xingma-tray-16.png` | 16×16 RGBA | 任务托盘、指示器 |
| `ico/xingma-toolbar-24.png` | 24×24 RGBA | 工具条 |
| `ico/xingma-button-48.png` | 48×48 RGBA | 工具按钮 |
| `ico/xingma-512.png` | 512×512 RGBA | 大尺寸展示：关于框、官网、文档插图 |

16 / 24 / 48 三个尺寸保持绘制时的原始大小，未做二次缩放；512 版本由同一掩膜放大导出，
不是另画的大图。八个 PNG 与两枚 ICO 的 16px 帧均来自同一掩膜，不存在另画的小尺寸版本。

## 2. ICO 规格与兼容性

两枚 ICO **与原 `ime/rospinyin.ico` 规格完全一致**：仅含一个 16×16 帧、32 位 RGBA、
透明底。现有 `ime/rospinyin_res.rc` 通过 `IDI_ROSPINYIN ICON "rospinyin.ico"` 引用图标，
ReactOS `kbswitch` 从 IME 模块提取第一个 `RT_GROUP_ICON` 作为布局徽标；单帧 16×16 正是
这条链路原本假定的规格，不要改成多尺寸 ICO，否则托盘取到的帧顺序会变。

本目录是新图标的提交位置，**不覆盖 `ime/rospinyin.ico`**。需要切换时，把
`ico/rospinyin.ico` 拷到 `ime/` 同名文件再重建 IME；RoseXingma 的 ICO 待其 IME 模块接入
资源脚本后按同样方式引用。

## 3. 图形与色值

| 产品 | 主色 | 图形 |
| --- | --- | --- |
| RosePinyin | `#B80259` 玫红 | 侧视垂冠玫瑰，花茎向上拱起后向右垂下，外轮廓略成 P |
| RoseXingma | `#7C17DF` 亮紫 | 俯视四叶玫瑰，夸张花萼张成 X，中心保留卷心负形 |

白色负形铺在品牌色圆角容器上（圆角半径 20%）。参考图仅作结构与色值依据，原图水印已擦除。

## 4. 使用约束

- 按场景选对应尺寸，不要把 48px 或 512px 图标缩到托盘使用；小尺寸必须取自同一掩膜。
- 512 PNG 仅用于展示场景；仍需 16 / 24 / 48 的界面图标时，用对应尺寸文件，不要拿 512 缩放替代。
- 不改主色：RosePinyin 只用玫红，RoseXingma 只用亮紫；色互换会破坏产品辨识。
- 不加描边、渐变、投影、纹理，不在图标内写中文或完整单词。
- 不把参考图原文件当作图标交付。
- 新增或替换图标时同步更新本文件与 `docs/README.md` 索引，否则 `tools/check_docs.py` 失败。
