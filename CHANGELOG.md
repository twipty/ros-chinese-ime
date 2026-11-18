# 变更日志

本项目版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。1.0 之前次版本号可能包含不兼容变更。
`alpha` 前缀的含义：**可在 ReactOS 0.4.16+ 上实机安装并输入单字**，但功能、数据与接口尚未定型。

---

## 未发布（alpha0.0.2 候选）

### 新增

- **Shift 切中英**：单独按一下 Shift（不夹带其它键）在中/英之间切换；英文态下所有键归应用。
  `Shift+A` 仍是大写 A，不会切成英文（lone-Shift 锁存）。
- `uigeom` 诊断行：`comp=/cand=` 实际几何 + `anchor=` + `caret=0/1`（锚点是否退化成鼠标位置）。

### 修复

- **`zhongguo` 只剩 guo 的候选**：候选取 `syllables_.back()`（从右往左），且上屏后清空整个组合串，
  等于把前面打的丢掉。改为**左到右逐音节上屏**：候选给第一个未转换音节，选中后只吃掉首音节，
  余下（`guo`）留在组合串里继续转 → 连按空格得到「中国」。
- **组合窗被裁到第一个音节的宽度**：`CompositionWindow::update()` 量了文本、写了缓存的宽高，
  却从不 `SetWindowPos`。几何收敛到唯一出口 `place_windows()` → `UiWindow::apply_geometry()`，
  位置尺寸一起下发、无变化整条跳过；组合窗首音节黑体+黑下划线、余下音节灰体+浅灰下划线。
- **中英状态被焦点切换重置**：`AlignOpenStatus()` 从"每次激活"改成"每会话一次"。
- **候选窗跟着上屏的字往右爬**：锚点原来每次 `UiShow` 都重取插入符，上屏一个字插入符就右移。
  改为**一次组合只取一次锚点**；x 的右边界收口也从"按实测宽度"改成固定预留 300，
  公式不再含任何窗口宽度，打字和翻页都不会再改 x。
- imhost 编译缺 `-static` 导致 guest 端缺 `libgcc_s_dw2-1.dll` 起不来（工具侧，不在发布物内）。
- imhost 输入框汉字重叠（字距"过窄"）：真因是选到的字体没有中文字形、步进仍按拉丁宽度算
  （`adv≈0.5em`，字压字），不是字号问题；EDIT 也没有字间距 API。
  改为**按实测步进选字体**（`adv >= height*0.85`，候选逐个量）→ 命中 `Droid Sans Fallback`，
  `adv=17=1em`；输入框字号 19→17、label 12→14；找不到时打 `font FAILED` 而不是静默退化。
  另留兜底开关 `imhost.exe font:<face> size:<10–40>`（工具侧，不在发布物内）。

### 验收

- **M2 验收单 15/15 通过（2026-11-18）**：真机手打一轮 433 键，入口层与窗口层逐条对齐、程序注入 0 次；
  `nv/lv/lve/nve`、跨页选字、Backspace/Esc、100+ 字连打、lone-Shift 中英往返均有日志实据。
  逐项实测见 `m2/M2-ACCEPTANCE.md` §3，还原方法见 `HANDOVER.md` §11.16。

---

## alpha0.0.1 — 2026-11-17

首个公开版本。M2「单字拼音引擎」在 ReactOS 0.4.16 i386 上以**真实键盘手打**（非程序注入）端到端通过。

### 可用能力

- `ImmInstallIMEW` 安装为独立布局「中文(简体) - RosPinyin」，`Alt+Shift` 可切换，托盘显示「中」图标
- 组合串输入：连续音节、音节下划线、`'` 硬分隔（`xi'an` → `[xi][an]`）
- 候选窗：每页 9 条，`-`/`,` 上一页、`=`/`.` 下一页、页码 `n/m`
- 选字上屏：空格选首选，`1`–`9` 选当前页候选，翻页后数字按当前页解释
- 编辑键：Backspace 回退组合串，Esc 取消不上屏
- `v` 键输入 ü：`nv`→女、`lv`→律、`lve`→略、`nve`→虐
- 数据表 44,355 字 / 419 音节（hanziDB MIT 主源 + Unicode Unihan 辅源，字节级可复现）；候选默认只呈现有字频的 9,898 字

### 修复

- **IME 从不被加载**（致命）：UI 服务窗口类名 `RosPinyinUIServer` 长 17 字符，被 `imm32` 的 `szUIClass[16]` 静默截断成 `RosPinyinUIServe`，随后按截断名 `GetClassInfoW` 失败 → 卸载 DLL 并置 `fLoadFlag=1`。类名收敛为 `RosPinyinUISrv`（≤15 字符，附 `static_assert` 与门禁检查）。
- **多字 / 丢字**：`co_IntProcessKeyboardMessage` 对 `WM_KEYUP` 也置 `bKeyUpDown`，而 `ImmProcessKey` 不看 transition 位、`ImeToAsciiEx` 又拿不到 lParam，导致每个键被喂两次。`ImeProcessKey` 现在对 key-up 一律返回 `FALSE`。
- **简体「谁」打不出来**：Unihan 里 U+8C01 `kMandarin` 只有 `shéi`，语料高频的 `shuí` 不在全集内被丢弃，`shui` 音节被繁体「誰」占据。`mkdat` 改为把 `kHanyuPinlu` 的音作为**次读音**保留；`CodeTable::load_dat` 增加可选回调，为次读音建别名索引（同一字可回答多个音节且不重复出字）。
- **候选窗出现空心方框**：44,355 条数据中 34,457 条是 Unihan 独有生僻字，其中 68% 在随包字体子集（`U+4E00–U+9FA5`）之外。`PinyinSession` 默认过滤无字频字（该音节过滤后为空则回退全表），可用 `set_show_rare(true)` 恢复。
- 修正 `rospinyin_res.rc` 版本资源与实际版本不符的问题，版本号统一为 `alpha0.0.1`。

### 效果对比（真实引擎 + 真实数据）

| 输入 | 修复前 | 修复后 |
|---|---|---|
| `shui` | 34 条，首选「水」，简体「谁」不存在，20 个空心框 | **7 条**：水 谁 税 睡 稅 誰 脽，简体「谁」第 2 位，0 个空心框 |
| `zhong` | 108 条 | 26 条 |
| `shi` | 340 条 | 95 条 |

### 已知限制

- 只支持**单字**上屏，没有词组与词频（M4）；`zhongguo` 目前只能逐字选
- 没有中文标点、全角/半角转换、Shift 临时英文（M3）
- 语言栏菜单项（属性 / 关于）为空（M6）
- `ImmGetOpenStatus` 在 ReactOS 上的读数异常（`-1` / `0` 混杂），Shift 切英文后的按键行为尚未验证
- 组合串窗口位置在每次 `feed` 都会重置一次（`ImmSetCompositionWindow` 调用偏多），快速输入时可能看到轻微跳动
- 无安装器，当前需手工复制 `.ime` / `.dat` 到 `C:\ReactOS\system32\` 并用 `ImmInstallIMEW` 注册
- 仅在 ReactOS 0.4.16 i386（QEMU）上验证；未在真实硬件、Windows、x64 上测试

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 53/53、`xingma/`（形码产品线）零改动。

---

## 未发布（`alpha0.0.2` 候选）— 2026-11-17

M2 收口改动。**不改数据格式、不改 IMM32 ABI**，`alpha0.0.1` 的数据表可直接沿用。

### 修复

- **`ImmGetOpenStatus` 读数"异常"**：`-1` 是宿主程序的 `g_himc == NULL` 哨兵（恰等于 `TRUE`），
  `0` 是 ReactOS 建输入上下文时的出厂值（`imm32_imm.c` `pIC->fOpen = FALSE`，激活路径从不回置）。
  现在 `ImeSetActiveContext` 激活时把 OS 侧状态对齐为 `TRUE`，trace 记录 `openalign before=/after=`；
  键路由仍不依赖该值（契约闸门已加：`should_consume_key` 内不得出现 `OpenStatus`）。
- **组合串窗口被裁切**：窗口量完文本只更新了缓存宽高，从未 `SetWindowPos`，
  `zhongguo` 仍按 `zhong` 的宽度绘制。几何改为单一 owner（`place_windows()` → `apply_geometry()`）。
- **快速输入时窗口跳动**：每次引擎更新都 `InvalidateRect + ShowWindow + SetWindowPos`。
  现在只有几何真正变化才下发、只有窗口隐藏时才 `ShowWindow`、只有内容变化才重绘。
  顺带查清：`IMN_SETCOMPOSITIONWINDOW` 每键 1–3 次是 ReactOS 的 `EDIT_SetCaretPos` 发的
  （`edit.c`，仅 `__REACTOS__` 分支），不是引擎行为，IME 侧无法减少。
- **安装器只拷贝 `.ime`**：装完缺 `rospinyin.dat` → 候选窗空白。`/step1` 现在两份都拷并校验字节数；
  文件被映射（`ERROR_SHARING_VIOLATION`）时先改名 `.old` 再写，`/status` 报告两份，`/u` 两份都删。
- **扩展 A 区字符显示为方框**：随包字体子集到 `U+9FA5` 为止，`shi` 里有一个**有字频**的扩展 A 字。
  候选进分页器前按 `GetGlyphIndicesW` 过滤（按码点缓存）；探针不可用或过半被剔时整体放行，
  避免字体异常时把候选清空。

### 工程

- `tools/check_m1.py`：53 → **57** 项，新增 4 条 M2 不变量（组合窗几何 owner、重绘条件化、
  安装器两份 payload、open 状态不得作为门禁）。
- 新增 `m2/M2-REPORT.md`；`m2/M2-REMAINING.md` 更新为"仅剩真机验收单"。
- trace 新增 `wmove=`（真正改变窗口几何的次数）与 `wrep=`（重绘请求次数）、
  `cand-filter`（字形过滤剔除数）。

### 已知限制（未变）

- 单字上屏；无中文标点/全半角/Shift 临时英文（M3）
- 字体子集不含扩展 A 区，该区候选被过滤（不出现，而不是显示方框）
- 安装器仍是命令行工具，ReactOS 上需 `/step1`…`/step4` 分步 + 重启

### 回归基线

`ctest` 11/11、`tools/check_m1.py` 57/57、`xingma/` 零改动。

---

## 版本号约定

- `alpha0.0.x`：M2 阶段，单字可用，数据与接口可能随时调整
- `0.1.0`：M3 完成（标点、全半角、临时英文）
- `0.2.0`：M4 完成（词组与词频）
- `1.0.0`：达到 README §3 的 v0.1 Definition of Done
