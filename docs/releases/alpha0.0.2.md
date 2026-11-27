# RosPinyin alpha0.0.2

第二个公开版本。这一版把 **M2「单字拼音引擎」收口**了，交互问题已按用户反馈修正。

## 下载

| 文件 | 大小 | sha256 |
|---|---|---|
| `rospinyin.ime` | 940,601 B | `c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa` |
| `rospinyin.dat` | 902,776 B | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
| `SHA256SUMS.txt` | — | 校验值 |

`rospinyin.dat` 与 `alpha0.0.1` **完全相同**——这一版没有动数据层。

## 这版修的四条

1. **`zhongguo` 只转出「中」，候选只有 `guo` 的字** — 候选取了最后一个音节、上屏后又清空整个组合串，等于把前面打的丢掉。改为**左到右逐音节上屏**：选中只吃掉首音节，剩下的 `guo` 留在组合串里继续转，连按空格得到「中国」。
2. **Shift 不能切中英文** — ReactOS 的 `imm32` 不给 `VK_SHIFT` 注册热键，激活时也不回置 `fOpen`，IME 自己不认领就等于没这个功能。现在实现了 **lone-Shift 切换**（按下置位、夹带其它键即取消、抬起时才切），`Shift+A` 仍然是正常的大写 A。
3. **翻页时候选窗/输入框往右爬** — 锚点原来每次显示都重新取一次插入符，而上屏一个字插入符就右移一格。改成**一次组合只取一次锚点**；右边界收口也从"按实测宽度"改成固定预留，公式里不再含窗口宽度。
4. **汉字粘在一起（字距"过窄"）** — 不是字距问题，是**步进重叠**：选到的字体没有中文字形，字形由替换字体画、步进却按拉丁字母宽度算（实测 `adv≈0.5em`）。EDIT 控件也没有字间距 API。改为**按实测步进挑字体**（`adv >= height*0.85`，逐个候选量），字号 19→17。本机命中 `Droid Sans Fallback`，`adv=17=1em`。

另外：组合窗不再被裁到第一个音节的宽度、安装器会一并拷 `.dat`、候选按字形过滤掉没有字形的生僻字。

## 安装（仍是手工三步）

ReactOS 0.4.16（i386）命令行里：

```bat
wget https://github.com/twipty/ros-chinese-ime/releases/download/alpha0.0.2/rospinyin.ime
wget https://github.com/twipty/ros-chinese-ime/releases/download/alpha0.0.2/rospinyin.dat
copy /y rospinyin.ime C:\ReactOS\system32\
copy /y rospinyin.dat C:\ReactOS\system32\
```

注意系统目录是 **`C:\ReactOS\system32`**，不是 `C:\windows\system32`。**两个文件都要拷**——只拷 `.ime` 会没有候选字。

注册布局：跑仓库 `tools/` 里的安装辅助，或自己调 `ImmInstallIMEW(L"C:\\ReactOS\\system32\\rospinyin.ime", L"中文(简体) - RosPinyin")`，然后重启。重启后 `Alt+Shift` 切到「中文(简体) - RosPinyin」，托盘出现红色「中」字即可输入。单独按一下 `Shift` 可在中/英之间切换。

## 已知限制 ⚠️

- **只能单字上屏**，没有词组和词频（M4）：`zhongguo` 得逐字选
- **没有中文标点**、全角/半角
- 语言栏的「属性 / 关于」菜单是空的（M6）
- 需要**手工安装**，没有图形安装器
- 仅支持 ReactOS 0.4.16 i386；真实硬件 / x64 / Windows 不在支持范围
- 极长的候选列表（如 `shi` 有 11 页）翻页只能靠 `=` / `-`
- 候选窗跟随插入符时，字母与汉字宽度不同会有几像素的横向抖动（不影响选字）

## 许可证

代码 MIT（见 [LICENSE](https://github.com/twipty/ros-chinese-ime/blob/main/LICENSE)），数据与第三方组件的分层授权见 [NOTICE.txt](https://github.com/twipty/ros-chinese-ime/blob/main/NOTICE.txt)。
