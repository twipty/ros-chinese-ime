# M2 报告：单字拼音引擎端到端

> 版本基线：`alpha0.0.1`（2026-11-17 发布）＋ 2026-11-17 夜间的 M2 收口改动（未发版，将随 `alpha0.0.2`）
> 环境：ReactOS 0.4.16 i386 / QEMU / 真实键盘手打（禁止程序注入按键）
> 相关文档：`docs/milestones/m2/M2-ACCEPTANCE.md`（15 项操作单）、`docs/milestones/m2/M2-REMAINING.md`（剩余事项）、`HANDOVER.md` §11

## 1. 结论

**主链路已通**：真实按键 → 组合串 → 音节切分 → 候选窗 → 选字上屏，全程在 ReactOS 0.4.16 上、
用真实键盘（非注入）完成，三层记录逐条对齐：

```
入口层（WH_KEYBOARD）DOWN=40 UP=40 | 窗口层 40/40
WM_CHAR=5 | WM_IME_CHAR=5 | 程序注入按键 0 次
上屏序列：你 → 你好 → 你好中 → 你好中国 → 你好中国谁
```

**验收已收口（2026-11-18 12:5x）**：`M2-ACCEPTANCE.md` 的 15 项全部有实测记录，15/15 通过。
依据是 `imhost.log` 中同一轮运行的 433 次按下与 `WM_IME_CHAR` 折叠还原——`v` 键四例、
翻页/跨页选字、Backspace/Esc、100+ 字连打、lone-Shift 中英往返都在里面（§11.16 有映射表）。
M2 的口径因此从"功能可用，验收未完"改为**"功能可用，验收通过"**；`1.0` 仍等 M3 定型与发版工程化。

## 2. 交付物与版本对应

| 资产 | 尺寸 | 说明 |
|---|---|---|
| `rospinyin.ime` | 946,193 B | 16 导出 IMM32 IME；静态链接 CRT，只依赖 kernel32/user32/gdi32/imm32/msvcrt |
| `rospinyin.dat` | 902,776 B | 44,355 字 / 418 音节；9,898 条带字频（默认候选来源） |
| `SHA256SUMS.txt` | — | 两个资产的校验和 |

`alpha0.0.1` 的两个二进制由当时的 `build-win-trace` 产出并已通过下载回验。
**本报告的收口改动（§4 第 7–13 条）未包含在 `alpha0.0.1` 内**，需要重新构建后以 `alpha0.0.2` 发布。

`alpha0.0.2`（2026-11-18 发布，`ROSPINYIN_TRACE=OFF` 构建）：

| 资产 | 字节 | sha256 |
|---|---|---|
| `rospinyin.ime` | 940,601 | `c9dc17c0…3e25feaa` |
| `rospinyin.dat` | 902,776 | `f28b2606…a23e9b21` |

> 真机验收用的是同源码的 `ROSPINYIN_TRACE=ON` 构建（`rospinyin7.ime`，967,775 B `15e401cf…`）——
> 导出表 16 项、导入 DLL、功能路径与发布版一致，只多一份诊断日志。
> 构建产物**不可比特级复现**：MinGW 链接器每次写入不同的 PE 时间戳，
> 因此"发布的二进制 = tag 源码构建"只能靠"发布时现场构建 + 记指纹"保证，不能靠比对字节。


## 3. 已验证的验收项

| 验收口径 | 证据 | 状态 |
|---|---|---|
| 数据加载（44,355 字 / 418 音节） | IME 自报指纹 `dat OK path=... chars=44355 codes=419` | ✅ |
| 组合串 + 音节下划线 | 截图确认 `zhongguo` 两段下划线 | ✅ |
| 候选窗 9 条 + 页码 | 截图确认 | ✅ |
| 空格上屏 | 5 次上屏，`WM_IME_CHAR=5` | ✅ |
| 候选质量（`shui`） | 34 → 7 条：水 谁 税 睡 稅 誰 脽；空心框 20 → 0 | ✅ |
| 无多余字符 / 无丢键 | 40 键对齐 40 消息，`processkey ↔ toascii` 29:29 | ✅ |
| `nv`/`lv`/`lve`/`nve` | 真机：`NV␣`→女、`LV`→律、`LVE⌫E`→略、`NVE␣`→虐 | ✅ |
| 翻页 / 跨页数字选字 / 回翻 | 真机：`=`/`-` 后跨页命中（`7仕`、`4郭`、`1衷`） | ✅ |
| Backspace / Esc | 真机：`HAO⌫⌫⌫` 组合串回退重算；`HAO⎋` 清空且无上屏 | ✅ |
| 100 字压测、EN↔中 往返 | 真机：连打《陋室铭》《山居秋暝》100+ 字无崩溃；lone-`Shift` 切英文后拉丁字符直出、再切回可继续 | ✅ |
| 左到右逐音节上屏 | 真机：`zhongguo` 两段下划线，空格逐音节上屏「中国」 | ✅ |
| 汉字字距（步进） | `font ok face="Droid Sans Fallback" h=17 adv=17`（`adv≈1em`），截图确认不重叠 | ✅ |

## 4. 根因与修复记录

每一条都带回归断言或契约闸门，避免"修了又回来"。

1. **IME 从不被调用**：UI 类名 17 字符被 `IMEINFOEX.szUIClass[16]` 截断 → `fLoadFlag=1`。
   类名收敛到 `RosPinyinUISrv`（≤15），`check_m1.py` 断言长度。
2. **多字 / 丢字**：`WM_KEYUP` 也被喂进 `ImeToAsciiEx`（该路径拿不到 transition 位）。
   `ImeProcessKey` 对 key-up 一律 `FALSE`。
3. **简体「谁」打不出**：Unihan 的 `U+8C01` 只有 `shéi`，`shuí` 落在 `kHanyuPinlu`。
   `mkdat` 把辅源音节保留为 alt 读音，`CodeTable::load_dat` 建别名索引。
4. **候选空心框**：44,355 字里 34,457 条是无字频生僻字。`PinyinSession` 默认只放有字频的字，
   该音节为空时才回退全表。
5. **"改了没生效"**：`.dat` 换了但 `.ime` 没换（反之亦然）→ 两类文件都做启动自动对齐，
   并由 IME 自报数据指纹（路径/大小/字数/语义探针 `shui`）。
6. **`.ime` 无法原地覆盖**：被 image 映射时 `CopyFile` 失败（err=32）。
   退让链：`MoveFile` 改名 → 备份后删除 → 同名重建；仍不行则自动切 `rospinyindev.ime` 布局。
7. **`ImmGetOpenStatus` 读数"异常"（P0-2）**：`-1` 是 `imhost` 的 `g_himc==NULL` 哨兵，
   `0` 是 ReactOS 建上下文时的出厂值（`imm32_imm.c` `pIC->fOpen = FALSE`，激活路径从不回置）。
   键路由不看这个值。修法：`ImeSetActiveContext` 时对齐为 `TRUE` 并 trace `openalign`；
   契约闸门禁止把它当门禁。详见 HANDOVER §11.11。
8. **组合窗跳动（P0-3）**：通知来自 ReactOS 的 EDIT——`EDIT_SetCaretPos` 每次移动光标都调
   `ImmSetCompositionWindow`（仅 `__REACTOS__` 分支，不判断是否在组合中），一次按键 1–3 次。
   IME 侧无法减少，但我们自己的窗口有两个真 bug：组合窗只改缓存宽高、从不 `SetWindowPos`
   （长串被裁），以及每次通知都无条件重绘。现在几何只有一个 owner、只在变化时下发，
   重绘以内容变化为前提。详见 HANDOVER §11.12。
9. **安装器只拷 `.ime`（P1-1）**：装完没有 `.dat`，候选窗空白，且症状与"数据没生效"同形。
   现在 `/step1` 拷两份 + 字节数校验，被占用时改名退让，`/u` 两份都删。
10. **扩展 A 区方框（P1-2）**：随包字体子集只到 `U+9FA5`，而 `shi` 里有一个**有字频**的扩展 A 字。
    改为进分页器前按 `GetGlyphIndicesW` 过滤，探针不可用或"过半被剔"时整体放行。详见 HANDOVER §11.13。
    （2026-11-19 后续：F 计划变更后该运行时过滤将由构建期分级码表 + 全字形开关取代，见 `docs/plan/ROADMAP.md` §F。）
11. **`zhongguo` 只显示 zhong、候选只有 guo（真机第一轮反馈）**：`recompute()` 取 `syllables_.back()`
    从右往左给候选，且上屏后清空整个组合串。改为左到右：候选给首个未转换音节，
    选中后只吃掉首音节（新增 `Result commit_text()`），余下留着重切分。详见 HANDOVER §11.14。
12. **Shift 不能切中英**：ReactOS 的 imm32 不给 `VK_SHIFT` 注册热键、激活路径也不回置 `fOpen`，
    IME 不认领就等于没这个功能。改为 lone-Shift 锁存（夹带其它键即清位），
    `open` 从"只能观测"变成"IME 自己的状态"，`should_consume_key()` 首行按它门禁。
13. **汉字粘在一起（真机第三轮反馈）**：不是字距太紧而是**步进重叠**——`CreateFontW`
    拿到的字体没有中文字形，字形由替换字体画、步进仍按拉丁宽度算（实测 `adv≈0.5h`）。
    EDIT 无字间距 API，所以改为"量步进选字体"（`adv >= height*0.85`），
    最终命中 `Droid Sans Fallback`（`adv=17 = 1em`），字号同步 19→17。详见 HANDOVER §11.15。

## 5. 已知限制

| 限制 | 影响 | 后续 |
|---|---|---|
| `IMN_SETCOMPOSITIONWINDOW` 仍每键 1–3 次 | 只影响 `cfCompForm`，我们不读；不再引起重绘 | 属 ReactOS 行为，记录即可 |
| 字体子集不含扩展 A 区 | 该区候选被过滤，"能打但看见方框"变成"不出现" | 需要扩字体或换字体覆盖策略 |
| `set_show_rare()` 无 UI 入口 | 生僻字默认不可见 | 由 2026-11-19 F 计划变更的全字形开关（含快/全指示器）取代语言栏菜单方案，见 `docs/plan/ROADMAP.md` §F |
| 安装器是命令行的，且 ReactOS 需逐步重启 | 不适合分发给普通用户 | P1-1 后续做 GUI/一键 |
| `m0/share/rospinyin.dat` 与 `build/m2-data/` 两份副本靠手工同步 | 容易再次"改了没生效" | **已闭合**：`build_dat.sh --install`（2026-11-20）在指纹闸后自动拷贝，不再手工 `cp` |
| CI 未自动产出 release 资产 | "发布的二进制 = 该 tag 源码"靠人工保证 | P2-1 |

## 6. 门禁与复现

```bash
# 主机侧
cmake --build build --parallel && ctest --test-dir build      # 11/11
python3 tools/check_m1.py                                     # 63/63（长期契约闸门）
# Windows 产物
cmake --build build-win-trace --parallel                       # rospinyin.ime + rospinyin.dat
# VM 内（guestOS 命令行，系统目录是 C:\ReactOS\system32）
install-rospinyin.exe /status
install-rospinyin.exe /step1        # 拷 .ime + .dat，校验字节数
```

真机验收按 `docs/milestones/m2/M2-ACCEPTANCE.md` 执行，`imhost.exe rec` 只记录不注入。
