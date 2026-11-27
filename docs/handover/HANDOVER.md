# RosPinyin 接手文档（Handover）

> 面向对象：接手的维护者 / 新协作者
> 版本基线：`main` = `d6a03d4`（2026-11-26）；发布线 `v0.1.0` 正式版已发布（tag `93e75f9`）
> 阅读顺序建议：第 1–3 章了解"是什么、到哪了"→ 第 6 章跑起来 → 第 5/8 章读代码 → 第 10 章避开坑
> **2026-11-26 状态快照见 §3**：M0–M3 完成、v0.1 发布；M4 **宿主代码与部署链路已完成**（词库内嵌必装、硬失败，宿主门禁全绿），6080 开发机实机验收已闭环（owner 截图裁定：`zhongwen` 代 `jisuanji`、安装截图覆盖 `/status`）；6081 nightly 对照已通过（`m4-pass-6081-20261126-2224.png`：`cizu` 候选含「词组」），仅 6082 stable 对照待补；M5 可开工。

---

## 1. 项目目标

### 1.1 一句话使命

为 ReactOS 提供一个**可原生安装、开源、开箱即用的简体中文拼音输入法**，填补 ReactOS 长期缺失的中文 IME（上游议题 CORE-19268）。

### 1.2 为什么可行（立项依据）

- ReactOS 对标 Windows Server 2003，只实现了老式 **IMM32** 输入框架；TSF/Cicero 整体缺失（CORE-19360/19361/19363），所以搜狗、微软拼音等现代 TSF 输入法装不上。
- IMM32 栈在 0.4.15/0.4.16 已基本补完；日语 MZ-IME 已被上游证明可在 ReactOS 运行。**"第三方 IMM32 IME 能跑"已验证，缺的只是中文 IME 本体**。
- 技术蓝本锁定 `katahiromz/mzimeja` 的 **v1.0.0.3 tag（公共领域 PDS）**：本仓库代码为 MIT，仅把 mzimeja 当黑盒行为参照（1.0.1.1+ 已转 GPLv3，**不得复制其源码**）。

### 1.3 产品形态

**主形态 = 第三方软件包**（独立仓库、独立发版、安装器写注册表 + `ImmInstallIMEW`），对标"ReactOS 版搜狗"而不是"微软拼音"。
架构上保持 in-tree 友好（`ROSPINYIN_IN_TREE` 开关，引擎平台无关，数据独立成文件），但**不以进入官方 ISO 为起点承诺**。

### 1.4 v0.1 目标（DoD）

1. `rospinyin.ime` 可一键装入 ReactOS nightly（0.4.16+），重启后仍有效；
2. 记事本中可流畅输入单字全拼：组合串、候选窗、数字选字、翻页、回车上屏、Esc 取消、退格编辑；
3. 中英切换可用（Alt+Shift 实测可靠；Ctrl+Space 在本 VM 不可靠）；
4. 中文标点、全角/半角；
5. 跨进程输入状态合理，无崩溃、无 BSoD。

### 1.5 明确不做（非目标）

TSF/Cicero 支持、云候选/账号/皮肤、繁体与其他方言、真实 Windows 上的产品级输入法（Windows VM 仅作对照验证）。

### 1.6 里程碑

| 里程碑 | 内容 | 状态 |
|---|---|---|
| M0 | 蓝本移植验证（QEMU/ReactOS 环境、IMM32 可行性） | ✅ 2026-11-13，见 `M0-REPORT.md` |
| M1 | 骨架替换：16 个 IMM32 导出、分步注册、托盘图标 | ✅ 2026-11-16，见 `M1-REPORT.md` |
| **M2** | **单字拼音引擎（核心里程碑）** | ✅ 2026-11-18 完成（15/15 实机验收）；首页覆盖率 96.8%，首选率 46.2%，见 `docs/milestones/m2/` |
| M3 | 中文体验补齐（标点、全角、Shift 临时英文） | ✅ 2026-11-23 主体完成：中文标点 + shift + 多码点 guest 实机 PASS；`[快]/[全]` 字形开关 P1 完成（2026-11-26）；全角/半角独立开关与 100 字段落实机验收**待确认** |
| M4 | 词组与词频 | ✅ **宿主实现 + 部署链路完成**（2026-11-26）：词库内嵌 GUI/CLI 安装器（第 4 RCDATA）、system32 固定 SHA-256 校验、四类失败拒绝加载；宿主 ctest 两配置 24/24、`check_m1` 120/120。**6080 开发机 + 6081 nightly 实机验收已闭环（owner 裁定），仅 6082 stable 对照待补** |
| M5 | 整句与自学习 | 未开工；前置依赖 M4 落地。文档口径已更正（整句开关为配置持久，非会话级；自写 Viterbi） |
| M6 | 设置 UI | 未开始 |
| M7 | 遗留场景修复（含上游往返） | 未开始 |
| M8 | 收尾与发布 | 未开始（v0.1 已作为独立软件包发布，不等于 M8） |
| 横切与并行项（非 xingma） | 已统一排期：`docs/plan/CROSSCUTTING-SCHEDULE.md`，总计划入口 `PLAN.md` §13；dev-loop「M4 落地」S0 轮次不变 |

> 详细 WBS 见 `docs/plan/PLAN.md`，阶段目标见 `docs/plan/ROADMAP.md`。M4/M5 状态更正见 PLAN §5/§6（2026-11-26）；横切与并行项排期见 PLAN §13 / `CROSSCUTTING-SCHEDULE.md`。

### 1.7 第二条产品线：RosXingMa（形码）

`xingma/` 是复用同一框架的第二款输入法（形码/五笔类）。**当前只完成规划与文档，W1 编码未开始**。
边界规则：只有形码需要的东西放 `xingma/`；拼音也要的属于框架层，留在 `framework/`。形码侧**不得修改**框架结构/构建/文档。

---

## 2. 整体架构

### 2.1 分层

```
┌────────────────────────────────────────────────────────────────────┐
│  应用（记事本 / EDIT 控件）                                        │
└───────────────▲────────────────────────────────────────────────────┘
                │ WM_IME_CHAR（每个 UTF-16 码单元一条）
┌───────────────┴────────────────────────────────────────────────────┐
│  ReactOS IMM32（win32ss/user/imm32）                               │
│    ImmProcessKey → ImeProcessKey                                   │
│    ImmTranslateMessage → ImeToAsciiEx(TRANSMSGLIST*)               │
└───────────────▲────────────────────────────────────────────────────┘
                │ 16 个导出（ime/rospinyin.def）
┌───────────────┴────────────────────────────────────────────────────┐
│  platform/win32/imm32 —— 可复用宿主层（imfw，X2-d 拆 4 个 TU）     │
│    imm_entry.cpp   16 个导出 + 唯一激活序列 activate_session()     │
│    key_translate   原生消息 ↔ KeyEvent；commit → TRANSMSGLIST      │
│    win32_host      Win32Host：键策略的 Win32 半边（Host 实现）     │
│    trace_line      ROSPINYIN_TRACE 通道（一行一次开写关）          │
│    imm32_abi.h     IME 侧 ABI（含 TRANSMSGLIST）                   │
│    session_store   每 HIMC 一个会话 + reload_sessions()            │
│    ui_server/dllmain  UI server 窗口类注册（DllMain 只做这个）     │
├────────────────────────────────────────────────────────────────────┤
│  platform/win32/ui —— 组合串窗 / 候选窗 / 工具条（自绘，无需 DDK） │
├────────────────────────────────────────────────────────────────────┤
│  framework/include/imfw + src —— 纯 C++ 引擎契约与工具             │
│    engine.hpp   Action/Candidate/Result/Engine                     │
│    scheme.hpp   Scheme 插件接口（复用缝）                          │
│    emit_text    多码点上屏 → WM_IME_CHAR 序列                      │
│    candidate_pager / utf8 / code_table / dat_format                │
├────────────────────────────────────────────────────────────────────┤
│  Scheme 插件（每个输入法一个）                                     │
│    pinyin/  —— PinyinScheme + PinyinSession + 自有数据源           │
│    xingma/   —— ShapeScheme（待实现）                              │
└────────────────────────────────────────────────────────────────────┘
```

**关键设计**：框架拥有所有 IME 的公共行为；Scheme 只回答三件事——"这个键是不是我的输入"、"我的引擎是谁"、"我的数据在哪"。

### 2.2 运行时按键链路

```
用户按键
  ↓
IMM32 判定该 HIMC 的 IME 是否打开（fOpen）→ 调用 ImeProcessKey
  ↓
ImeProcessKey(hIMC, vk, lParam, lpbKeyState)
  → should_consume_key()：Ctrl/Alt 一律放行；Scheme::input_char(vk) 命中字母表 → 拦截
  → TRUE = 这个键归 IME
  ↓
ImmTranslateMessage → ImeToAsciiEx(vk, sc, keyState, TRANSMSGLIST*, fuState, hIMC)
  → process_key()：选字键/翻页键（策略来自 Scheme）→ 退格 → Esc → Enter（仅中文+组合非空：RawCommit 原文上屏）→ 字母 feed 给 Engine
  ↓
Engine::feed(cp) → Result{action, composition, segments, candidates, commit_text}
  ↓
Absorb：刷新组合串窗/候选窗（framework/platform/win32/ui）
Commit：emit_text() 逐 UTF-16 码单元生成 TRANSMSG，写入 list->TransMsg
  ↓
返回写入条数 N，IMM32 从 list->TransMsg[0..N) 逐条投递 WM_IME_CHAR 给应用
```

**ABI 铁律（2026-11-17 审计 P0 修复）**：`ImeToAsciiEx` 第 4 参不是裸 `TRANSMSG*`，而是
`TRANSMSGLIST { UINT uMsgCount; TRANSMSG TransMsg[1]; }`。`uMsgCount` 是 IMM32 预填的**容量**，
消息数组从偏移 `sizeof(UINT)` 开始。从偏移 0 写会覆盖容量头并让所有消息错位一格（真机上表现为上屏错乱/丢字）。
现实现：`emit_text()` 从 `list->TransMsg` 写、容量取 `min(uMsgCount, kMaxEmitMessages=32)`。

### 2.3 数据所有权（G2 终局形态）

- **框架不加载、不持有、不传递任何码表**。`create_engine()` 无参。
- 每个 Scheme 完全自管数据：
  - 拼音：`shared_ptr<const imfw::CodeTable>`，构造时读 `%SystemRoot%\system32\rospinyin.dat`，缺失则用内置 ~500 字兜底表；
  - 形码（规划）：`ShapeStore` + 可选数据包。
- 热重载：`ImeConfigure` 成功 → 框架 `reload_sessions()` → **先** `Scheme::reload_data()` **再** `states_.clear()`（data-first，异常时旧会话仍自洽）→ 下一键以新数据重建引擎。

### 2.4 Scheme / Engine 契约

`imfw::Scheme`（`framework/include/imfw/scheme.hpp`，纯 C++，无 Windows 头）：

| 成员 | 作用 |
|---|---|
| `ui_server_class()` / `composition_class()` / `candidate_class()` / `toolbar_class()` | 窗口类名，每个 IME 互不相同，可同进程共存 |
| `toolbar_label()` | 工具条标签，如 `拼` |
| `input_char(vk)` / `is_input_char(cp)` | 输入字母表（拼音：a–z + `'`） |
| `is_selection_key(ch)` | 选字键，默认空格 + 1–9 |
| `page_key(ch)` | 翻页方向 +1/−1/0，默认 `-`/`,` 上页、`=`/`.` 下页 |
| `create_engine()` | 每个 HIMC 一个引擎，数据由 Scheme 自给 |
| `configure(void* parent)` | 接管 `ImeConfigure`；`true` 表示已处理 → 框架重载 |
| `reload_data()` | 数据热重载钩子（默认空） |

`imfw::Engine`（`engine.hpp`）：`feed(char32_t)` / `backspace()` / `cancel()` / `on_select(bool)`，
返回 `Result{action ∈ Commit|Absorb|Passthrough, composition(UTF-8), segments, candidates, commit_text}`。

> **`on_select(true)` 是硬要求**：`ImeSelect(TRUE)` / `ImeSetActiveContext(TRUE)` 必须调到，否则引擎不工作、所有键被静默吞掉。

---

## 3. 当前状态（接手时必读）

> 快照时间 2026-11-26 12:20；基线 `main` = `d6a03d4`。下表取代旧版细目，历史过程见 §11 与 CHANGELOG。

### 3.1 已完成

| 项 | 状态 |
|---|---|
| 框架宿主层（16 导出、键路由、UI 窗口、会话存储） | ✅ M1 完成并实测 |
| 框架 G1–G4 缺口 | ✅ 全部在框架侧解决（多码点上屏、零码表+热重载、配置入口、键策略/UTF-8 组合串） |
| 拼音引擎 + `rospinyin.dat` | ✅ 44,355 字 / 418 音节 / 首屏 96.8%，v2 格式可复现构建（指纹见 `tools/mkdat/build_dat.sh`） |
| M2 端到端实机验收 | ✅ 15/15（`docs/milestones/m2/M2-ACCEPTANCE.md`） |
| M3 中文标点 / Shift / 多码点 | ✅ guest 六键实机 PASS（2026-11-23） |
| P0 GUI 安装器 | ✅ 自包含 payload，平台白名单仅 `ProductName == "ReactOS"`；Windows 真机只走 CLI `/step1..4` |
| P1 `[全]` 字形模式 | ✅ 候选/编码窗按码点平面切文津 P0/P2/P3；6080 扩展区实机确认非豆腐块（2026-11-26）；`Ctrl+Shift+G` 会话级切换 |
| P2 nightly Droid 字体包 | ✅ `tools/droid-font-pack/` 可复现打包 + 安装/卸载；原版字体 SHA-256 固定 |
| P3 Windows GUI 兼容评估 | ✅ 结论：不登记 Windows GUI，CLI 例外保留，`docs/reviews/WINDOWS-GUI-COMPATIBILITY-ASSESSMENT.md` |
| 发布线 | ✅ `alpha0.0.1–0.0.4` prerelease + **`v0.1.0` 正式 release**（tag `93e75f9`，7 资产，prerelease=false） |
| VM 辅助脚本 | ✅ `m0/vm/start-vm.sh [6080\|6081\|6082] [start\|status\|stop]`（本地辅助，`.git/info/exclude`，不入库）；6081 nightly GUI 安装验收通过 |
| 门禁 | ✅ `ctest` 24/24（默认与无内置表两配置）、`check_m1.py` 120/120、`check_docs.py` 7/7、版本门禁 PASS；CI 交叉编译与 PE 导入表断言全绿 |

### 3.2 关键变更与决策（2026-11-26）

- **M4 状态纠错 → 已闭环（宿主侧）**：此前代码与宿主测试完成 ≠ 用户可用（安装器不拷词库、release 不含、guest 步骤无指引，`try_load_lexicon()` 找不到文件直接返回）。2026-11-26 打通部署链路：词库作为第 4 RCDATA 内嵌、安装期 SHA 双校验、加载链四类失败拒绝加载、CI release 含词库与 SHA256SUMS。宿主侧闭环；6080 开发机与 6081 nightly 实机已由 owner 截图验收闭环（6080 安装截图含词库 SHA 校验，词组截图 `zhongwen` 首选「中文」按裁定替代 `jisuanji`；6081 `cizu` 候选含「词组」），仅 6082 milestone 对照仍待补。
- **M4 同阶段追加需求（2026-11-26）：Enter 原文上屏**：中文模式且组合非空时，Enter 由 IME 认领，按用户键入的英文字母原文整串上屏（大小写/撇号原样、不设长度上限、候选为空也上屏），上屏后清空组合；空组合/英文模式/已提交后的第二次 Enter 透传给应用（换行）。链路 `VK_RETURN → KeyKind::Enter → IntentKind::RawCommit → Engine::commit_raw()`，`PinyinSession` 以 `raw_composition_` 并行缓冲保留原文。`tools/seams.py` engine 调用点上限 4→5 是登记新增合法调用点，不是放宽门禁。S0–S3 已批准，宿主两配置 ctest 24/24、`check_m1` 120/120、`check_docs` 7/7、交叉编译通过；实机待 owner 截图验收。
- **M5 整句开关语义更正**：配置项、配置后持久生效（注册表或文件，重启保留），**不是会话级热键**；与 `[快]/[全]` 字形开关（会话级、零持久化）是两套独立开关。技术路线为**自写轻量 Viterbi**，微软拼音/搜狗/libpinyin 仅作效果标杆，不引入外部引擎。
- **M4 落地裁决**：词库内嵌 GUI 安装器；装 `system32\` 与 ime/dat 同目录并按固定 SHA-256 校验；词库缺失/损坏时 IME 拒绝加载（**明确接受 v0.1 存量用户的破坏性升级**）；宿主 + guest 实机双验收。
- **发布口径**：产品定位为独立第三方 IMM32 软件包，已从全部文档剔除「进入 ReactOS 主线/官方树/ISO 预装」目标表述；v0.2 发布本轮不做。
- **安装器协议按产品拆分（2026-11-27）**：GUI 安装器新增版本号显示、协议勾选（默认不勾、勾选后解锁安装按钮）与独立可滚动协议查看窗口、主界面升级叙述（独立 `whatsnew.txt` 内嵌 RCDATA 112）、安装完成后 Yes/No → 10 秒可取消倒计时 → `ExitWindowsEx`+提权重启（失败兜底给 `shutdown /r` 命令）。协议文本按产品分离：MIT `LICENSE` 共用，第三方声明各自独立——`tools/install-rospinyin/NOTICE-pinyin.txt`（删原仓库级 `NOTICE.txt` 第 6 条字体占位与第 8 条 xingma 段，UI 字体依赖提示条保留不变）与 `NOTICE-xingma.txt`（只留形码相关条款与通用条款，供未来 xingma 打包使用）；原仓库级 `NOTICE.txt` 保留不动（25 个源码头注释指向它）。`pack_payload.py --product`（默认 pinyin，选 `NOTICE-<product>.txt`；`--notice` 显式覆盖兼容 CI/诊断）按产品区别内嵌，两类协议互不串味（S2 已做 header 字节级解码验证）。

### 3.3 未完成 / 当前阻塞

| 项 | 状态 | 说明 |
|---|---|---|
| **M4 部署链路** | ✅ 宿主完成 + 6080/6081 实机闭环 / ⏳ 仅 6082 对照待补 | 词库内嵌（第 4 RCDATA）、安装期 size+SHA 双校验、四类失败拒绝加载、`/status` 词库行、CI release 资产；宿主 ctest 24/24、`check_m1` 120/120。6080 证据：`check-install-20261126-2015.png`（词库 verified + 三文件 rhash）、`m4-phrase-6080-20261126-2026.png`（`zhongwen`→「中文」首选，owner 裁定替代 `jisuanji`）；6081 证据：`m4-pass-6081-20261126-2224.png`（nightly 0.4.17-dev，`cizu`→「词组」候选生效） |
| M5 整句与自学习 | ✅ 前置满足，可开工（代码未动） | 词库底座已就绪；文档口径已更正（配置持久开关、自写 Viterbi），代码未开工；6082 对照属并行验收，不阻塞 M5 |
| M3 全角/半角独立开关、100 字段落实机 | ⏳ 待确认 | — |
| M6/M7/M8 | 未开始 | 见 `docs/plan/ROADMAP.md` |
| `xingma/` 形码产品线 | W1 编码未开始 | 框架缺口已全关，可直接复用宿主层 |

### 3.4 当前风险

- **v0.1 存量用户升级破坏性**：新 IME 若词库缺失将拒绝加载；必须随安装器重装/升级带词库，发布说明需写明（owner 已确认接受）。
- **字体强依赖**：`[全]` 模式必须文津 P0/P2/P3 三片齐备，缺任一片显示结果不可预知；nightly 另需 Droid 字体包，均不随 release 分发。
- **guest 手工验收是盲区**：脚本无法注入键鼠，M4 实机、字体显示等必须由接手人手工操作并截图存 `m0/vm/evidence/`。
- **git push 通道**:全局 gitconfig 把 `https://github.com/` 重写到 gh-proxy 且丢认证头，直连 push 静默失败；一律用 `tools/push_via_api.py`，推送后 tree 对账 + `reset --soft` 对齐（详见 §12）。

### 3.5 后续计划

1. M4 落地 S0→S7 宿主侧已完成（方案、TDD 实现、审计、门禁全绿）；**6080 开发机 + 6081 nightly 实机验收已闭环**（owner 截图裁定：`zhongwen` 代替 `jisuanji`、安装截图覆盖 `/status`；6081 用 `cizu` 验词组），证据在 `m0/vm/evidence/`。6082 stable 对照截图仍待补，属并行验收。
2. **M5 前置已满足，可开 M5**：Viterbi 整句解码 + 用户词典（配置持久开关），仍按研发闭环逐阶段过目；建议把 `zhongwenshurufa → 中文输入法` 列为验收用例。
3. 视需要另立发版轮次（v0.2 不在本轮范围）。
4. 按 `docs/plan/CROSSCUTTING-SCHEDULE.md` 的 P1–P14 并行车道推进非 xingma 横切项：P1–P8 与 K0 同窗口，P9–P12 在 M4 闭环后、M6/M5 前，P13–P14 并入 M8；G1–G7 只登记门控，不擅自启动。

### 3.6 接手人必须知道的上下文

- **不要相信「代码写了 = 功能可用」**：判断任何里程碑完成必须走完整链路（安装器 → release 资产 → guest 步骤 → 实机输入），M4 就是反例。
- 基线来自代码与 CI，不来自摘要或旧文档；PLAN/ROADMAP 的状态更正以 2026-11-26 为准。
- 送文件进 guest：`http://10.0.2.2:8000/`（HTTP 8000，`m0/share/` 为根）；回传：`ftp://10.0.2.2:2121`。三机共享通道，`start-vm.sh` 的 `stop` 不关闭 HTTP/FTP。
- 当前仓库有未提交改动（M4 落地轮次 + 前两轮计划/文档更正）：`docs/plan/PLAN.md`、`docs/plan/ROADMAP.md`、`docs/plan/PLAN-MODULARIZATION.md`、`docs/README.md`、`README.md`、`docs/handover/HANDOVER.md`、`tools/mklex/README.md`、`framework/platform/win32/imm32/key_translate.cpp`、`framework/platform/win32/ui/README.md`，新增 `docs/plan/CROSSCUTTING-SCHEDULE.md`、`pinyin/include/rospinyin/lexicon_guard.hpp`、`tests/lexicon_guard_test.cpp`、`tools/mklex/extract_payload.py`；安装器/加载链/CI 共 15 个文件改动、`.devloop/` 产物亦有大量改动。**接手前先确认是否已入库**，不要直接覆盖。
- 待确认项：M3 全角/半角独立开关状态、100 字段落实验收结果、**M4 guest 实机截图**（我出步骤/owner 操作）。CLI 词库写入 / 卸载清理词库已由 S0 拍板「CLI 同步 + 卸载清理」并落地。

---

## 4. 目录结构

```
ros-pinyin/
├── CMakeLists.txt              顶层构建：framework + pinyin + (Windows) ime/安装器/httpdl + mkdat + tests
├── README.md                   项目章程（目标、立项依据、路线决策、非目标）
├── PLAN.md                     工作分解结构（WBS，M0–M8 逐条任务）
├── ROADMAP.md                  里程碑目标与工期
├── M0-REPORT.md / M1-REPORT.md 里程碑验收报告（含实测证据与决策记录）
├── NOTICE.txt                  第三方与数据分层授权（仓库级尽调文档，不随安装器分发）
├── LICENSE                     MIT（pinyin/xingma 两产品共用）
├── tools/install-rospinyin/
│   ├── NOTICE-pinyin.txt       安装器内嵌的 pinyin 协议（无字体占位/xingma 段）
│   ├── NOTICE-xingma.txt       安装器内嵌的 xingma 协议（供未来形码打包）
│   └── whatsnew.txt            主界面升级叙述，发版只改文本不改代码
├── framework/                  ★可复用 IMM32 输入法框架（imfw）
│   ├── README.md               模块地图、Scheme 复用缝、生命周期红线
│   ├── include/imfw/           引擎契约、Scheme 接口、emit_text、candidate_pager、utf8、code_table、dat_format、trace_stamp.hpp
│   ├── src/                    上述纯 C++ 实现（Linux 可单测）
│   ├── platform/win32/CMakeLists.txt  两个静态库：imfw-win32-imm32 / imfw-win32-ui（X2-e1）
│   ├── platform/win32/imm32/   IMM32 16 导出、ABI 头、会话存储、UI server、DllMain
│   └── platform/win32/ui/      组合串窗 / 候选窗 / 工具条 / 窗口基类
├── pinyin/                     ★拼音插件（框架不含拼音知识）
│   ├── include/rospinyin/      pinyin.hpp、pinyin_session.hpp、dat_source.hpp、scheme.hpp、version.hpp
│   ├── src/                    读音归一化、切分、会话、Scheme、宿主兜底数据源
│   └── host/dat_source_win.cpp Win32 数据源（读 system32\rospinyin.dat）
├── ime/                        装配层：rospinyin.def（16 导出）、rospinyin_res.rc、scheme_reg.cpp、图标
├── tests/                      17 个宿主测试（ctest）
├── tools/
│   ├── check_m1.py             契约静态检查（109 项，无第三方依赖）
│   ├── seams.py                五条前端 seam 的调用点计数与上限
│   ├── artifact_hashes.py      部署件对账：size + sha256，回答"我到底放出哪一版"
│   ├── vm_clock.py             guest 时钟对齐：时区 .reg / 手敲 date+time 两个通道
│   ├── mkdat/                  数据编译器 + fetch_data.py + fixtures
│   ├── install-rospinyin/      安装器（分步注册；自身不写 Preload，属部署偏好）
│   ├── rhash/                  guest 侧 SHA-256：ReactOS 没有 sha256sum/certutil
│   ├── httpdl/                 全静态 HTTP 下载器（guest 侧 wget/curl 替身）
│   └── syntax_stub/            宿主侧 Win32 头桩（Linux 语法检查用）
├── m0/vm/                      QEMU/ReactOS 实验环境（qcow2、启动脚本、noVNC、证据、日志）
├── m2/                         M2 验收操作单 + 数据方案
├── xingma/                     形码产品线 RosXingMa（规划/规范/数据包模板，代码待 W1）
├── libernt/                    未纳入版本控制的本地探针材料（.gitignore 外，可忽略）
└── .github/workflows/ci.yml    三 job CI：Linux 测试 / MinGW 构建 / 产物发布
```

---

## 5. 核心模块职责

| 模块 | 关键文件 | 职责 | 平台 |
|---|---|---|---|
| ABI 声明 | `framework/platform/win32/imm32/imm32_abi.h` | 16 导出原型、`TransMsg`/`TransMsgList`、IMEINFO、属性位 | Win |
| 导出实现 | `framework/platform/win32/imm32/imm_entry.cpp` | 16 个导出；`ImeConfigure`/`ImeSelect`/`ImeSetActiveContext`；**唯一的激活序列 `activate_session()`**（对齐 fOpen → `on_select(true)` → 工具条） | Win |
| 键翻译 | `framework/platform/win32/imm32/key_translate.cpp` | vk → `KeyEvent`（含 key-up 位）、`KeyResult` → `TRANSMSGLIST`（`WM_IME_CHAR` 容量与 `TRANSMSGLIST` 偏移规则） | Win |
| 宿主适配器 | `framework/platform/win32/imm32/win32_host.cpp` | `Win32Host`：`is_open`/`set_open`/`show`/`hide`/`can_render` + 两个 trace 钩子 | Win |
| trace 通道 | `framework/platform/win32/imm32/trace_line.cpp` | `ROSPINYIN_TRACE` 一行一写；十进制统一走 `framework/include/imfw/trace_stamp.hpp` 的 `put_uint()` | Win |
| 会话存储 | `framework/platform/win32/imm32/session_store.cpp` | 每 HIMC 一个 `ImeSessionState`；`reload_sessions()` data-first | Win |
| DllMain | `framework/platform/win32/imm32/dllmain.cpp` | **只注册 UI server 窗口类**（M0 红线 #1） | Win |
| UI server | `framework/platform/win32/imm32/ui_server.cpp` | 接收 IMM32 的 UI 消息并转发窗口层 | Win |
| 平台构建 | `framework/platform/win32/CMakeLists.txt` | 两个静态库：`imfw-win32-imm32`（宿主 7 个 TU）/ `imfw-win32-ui`（窗口 5 个 TU）；`ime/` 只装配不编译平台层 | Win |
| 窗口层 | `framework/platform/win32/ui/*` | 组合串（UTF-8 下划线切分）、候选窗（9 条/页）、工具条 | Win |
| 引擎契约 | `framework/include/imfw/engine.hpp` | `Action/Candidate/Result/Engine`，composition 为 UTF-8 | 纯 C++ |
| 上屏发射 | `framework/src/emit_text.cpp` | UTF-32 → UTF-16 码单元 → TRANSMSG 数组（非 BMP 代理对、容量截断不留半对） | 纯 C++ |
| 候选分页 | `framework/src/candidate_pager.cpp` | 页内选字、翻页、页码 | 纯 C++ |
| 码表工具 | `framework/src/code_table.cpp` + `dat_format.hpp` | `rospinyin.dat` v1 读取与 key 索引（框架不实例化，供方案用） | 纯 C++ |
| 拼音引擎 | `pinyin/src/pinyin_session.cpp` | 连续拼音切分（最长匹配 + 回溯 + `'` 硬边界）、字频排序、音节 segments | 纯 C++ |
| 拼音归一化 | `pinyin/src/pinyin.cpp` | `zhōng→zho1ng`、`ü→v`、`ń/ǹ/ḿ`；mkdat 与运行时共用 | 纯 C++ |
| 拼音 Scheme | `pinyin/src/scheme.cpp` | 窗口类名、字母表、数据自持与 `reload_data()` 换代 | 纯 C++ |
| 数据源 | `pinyin/host/dat_source_win.cpp` | 读 `%SystemRoot%\system32\rospinyin.dat`，失败回退内置表 | Win |
| 装配 | `ime/scheme_reg.cpp` | 定义 `imfw::scheme_instance()`（唯一的 Scheme 注入点） | Win |
| 数据编译 | `tools/mkdat/mkdat.cpp` | Unihan + hanziDB → `rospinyin.dat`（确定性输出） | 宿主 |
| 安装器 | `tools/install-rospinyin/install.cpp` | `/step1..4` 分步注册、动态 HKL 记录、`/i` 串行（仅 Windows 参照机） | Win |
| GUI 安装器 | `tools/install-rospinyin/gui.cpp` + `gui_res.rc` | 内嵌压缩 payload、平台白名单闸门、驱动 CLI step1..4；Windows 真机只走 CLI | Win |
| 契约检查 | `tools/check_m1.py` | 109 项静态断言（SPDX、16 导出、加载期调用白名单、ImeInquire 可拒绝、open 状态双入口对齐、工具条贴合工作区、ABI、reload 顺序…） | 宿主 |
| 部署对账 | `tools/artifact_hashes.py` | 各构建产物与共享副本的 size/sha256 对照，标出"同字节数不同内容"与"共享目录不是这一版" | 宿主 |
| guest 侧哈希 | `tools/rhash/rhash.c` | 自带 SHA-256 的小 exe：`rhash.exe [文件...]`，无参数时算 system32 的 `.ime`/`.dat`。只依赖 kernel32+msvcrt | Win |
| guest 时钟 | `tools/vm_clock.py` | 诊断宿主/UTC/guest 显示差值；生成时区 `.reg` 或打印 `date`/`time` 命令（**只打印，不代敲**） | 宿主 |

---

## 6. 构建与运行

> **从 GitHub 全新 clone 后先做一步**：远端仓库里的 shell 脚本没有可执行位（见 §10.5 第 24 条），
> 需 `chmod +x m0/vm/*.sh tools/mkdat/build_dat.sh vendor/fetch-vendor.sh`，否则 `./run-reactos.sh` 会报 Permission denied。

### 6.1 主机（Linux）构建与测试 —— 日常开发主通道

```bash
cd ros-pinyin
cmake --preset linux-ci
cmake --build --preset linux-ci --parallel
ctest --preset linux-ci                           # 17 个测试，应 100% 通过
python3 tools/check_m1.py                          # 109 项契约检查，应 109/109
```

`CMakePresets.json` 是构建配置的唯一出处（X2-e3）：CI、README 和本节的命令都走 preset，
不再各自复述一遍 `-D` 列表。preset 名由 `check_m1` 断言，改名会让 Linux job 直接红。

- 宿主构建**不编译** Windows 专有部分（`ime/`、安装器、`httpdl`、`framework/platform/win32/imm32`、`framework/platform/win32/ui`），只测纯 C++ 层。
- 产物：`build/tests/rospinyin-*`、`build/tools/mkdat/mkdat`。

### 6.2 数据构建（`rospinyin.dat`）

```bash
cmake --build build --target mkdat
bash tools/mkdat/build_dat.sh            # 默认输出 build/m2-data/rospinyin.dat
```

- 输入源由 `tools/mkdat/fetch_data.py` 按 **sha256 固定版本**拉取（Unihan 18.0.0 `kMandarin` + hanziDB 字频），可离线复用已下载文件。
- 输出确定性（同输入字节级一致），44,355 字 / 418 音节。
- 数据格式见 `framework/include/imfw/dat_format.hpp` 与 `docs/plan/DAT-FORMAT-V2.md`（magic `ROSPYDAT`、**v2 唯一版本**、80B header + CharEntry[24] + SylEntry[16] + 词组两表（可为空）+ 字符串池，无指针、可 mmap）。**v1 表已不可读**：旧表会被拒载并回退内置数组，故 `.ime` 与 `.dat` 必须同源发布。

### 6.3 Win32 / ReactOS 目标（MinGW-w64 i686）

CI 上（`.github/workflows/ci.yml`）跑三套配置，全部来自 `CMakePresets.json`（X2-e3）：

```bash
# 在 MSYS2 的 MINGW32 shell 里（g++/windres 就是 i686 的那套）
cmake --preset win32-mingw          # 可发布形态，build-standalone/
cmake --preset win32-intree         # ROSPINYIN_IN_TREE=ON，不建安装器与 rhash
cmake --preset win32-toy            # ROSPINYIN_SCHEME=toy，注入缝的常驻证明
cmake --build --preset win32-mingw --parallel
cmake --build --preset win32-intree --parallel
cmake --build --preset win32-toy --parallel
```

**没有 MINGW32 shell 的机器**（本机就是这种）用交叉 preset：

```bash
export ROSPINYIN_MINGW_TOOLCHAIN=/path/to/toolchain-i686.cmake   # CC/CXX/RC 都指向 i686-w64-mingw32-*
cmake --preset win32-cross          # build-cross/
cmake --build --preset win32-cross --parallel
```

`win32-trace` 是加 `ROSPINYIN_TRACE=ON` 的同一形态（`build-trace/`），只在排查时用，不进发布。

**工具链硬约束**（历史踩坑换来的）：

- 必须 **MinGW-w64 i686**，不能用 MSVC：MSVC 静态 CRT 会导入 `kernel32!InitializeCriticalSectionEx`，ReactOS 0.4.16 没有该导出（装任何运行库都补不上）。
- 产物只允许依赖 `kernel32 / user32 / advapi32 / imm32 / msvcrt`；CI 用 `objdump -p` 断言拒绝 `vcruntime140 / api-ms-win-crt / libstdc++-6 / libgcc_s / libwinpthread`。
- 因此**禁止 `std::mutex` / `std::thread`**，一律用 Win32 `CRITICAL_SECTION`。
- 图标以 base64 形式入库（`ime/rospinyin.ico.b64`），CI 构建前解码成 `.ico` 再交给 windres（`push_files` 只能传文本）。

### 6.4 安装到 ReactOS

> **发布渠道纪律（决策 2026-11-19，2026-11-23 扩充）**：下一个 release **必须以安装器版本发布**。
> 终态首选 `install-rospinyin-gui.exe`（双击安装/卸载；平台白名单当前只放行 ReactOS）；
> 当前已验证的稳妥路径仍是 `install-rospinyin.exe /step1..4`——它的 `/step1` 把 `.ime + .dat`
> 作为同一个带 size 校验的载荷拷贝，二者同生同死，不存在"装了 ime 没带 dat"的半安装态。
> Windows 真机安装只使用 CLI `/step1../step4`，GUI 不会放行。
> 下面的 wget+copy 手工路径**降级为备选**，仅用于诊断/开发机场景；用它时装漏 `.dat`
> 会进入空表异常态（字母上屏进组合串、候选恒空、退格/Esc 退出——见
> `tests/builtin_gate_test.cpp`），不再有内置表兜底（release 构建已去除）。

在 guestOS 命令行执行（**系统目录是 `C:\ReactOS\system32`，不是 `C:\windows\system32`**）：

```bat
cd \
wget http://192.168.0.204:8000/rospinyin.ime
wget http://192.168.0.204:8000/rospinyin.dat
copy /y rospinyin.ime C:\ReactOS\system32\
copy /y rospinyin.dat C:\ReactOS\system32\
install-rospinyin.exe /step1
install-rospinyin.exe /step2      REM ImmInstallIMEW，HKL 记录到 HKCU\SOFTWARE\RosPinyin\Install
install-rospinyin.exe /step3      REM Layout Text
install-rospinyin.exe /step4      REM Layout Display Name
```

- 分步注册是 **M0 红线 #4**：一次只写一个键，便于定位 BSOD；`/i` 串行模式**只允许在 Windows 参照机**使用。
- 改完 `.ime` 必须**重启 guest**，IMM32 才会卸载旧 DLL。
- 实测 HKL 形如 `e0210804`（动态获取，**不要硬编码** `0xE0100804`）。
- 切换键：**Alt+Shift**（Ctrl+Space 在本 VM 不可靠）。

#### 6.4.1 更新已装的 `.ime`：`err=32` 的标准解法（2026-11-19 实测）

**症状**：`copy /y rospinyin.ime C:\ReactOS\system32\` 报「另一个程序正在使用此文件，进程无法访问」，
或 `CopyFile` 返回 `32`（`ERROR_SHARING_VIOLATION`）。旧文件覆盖不掉，于是"我换了文件但现象没变"。

**根因**：我们的 IME 一旦被设为**默认输入语言**，会话就常驻映射这份 PE；只要它还是默认，
任何覆盖都会 32。这不是权限问题，改 ACL、换管理员身份都没用（详见 §11.10）。

**解法（最短路径，已实测）**——先让系统把默认输入语言交出去，覆盖，再交回来：

1. 控制面板 → 键盘 / 区域和语言 → **默认输入语言**临时改成「英语(美国)」（或把拼音从列表里移走），确定。
2. `copy /y rospinyin.ime C:\ReactOS\system32\`（`.dat` 同理）。此时应直接成功，不再报 32。
3. 把默认输入语言切回「中文(简体) - RosPinyin」。
4. **重启 guest**。第 3 步只让 win32k 重新加载新文件，**不会**重载已经映射了旧 image 的进程
   （记事本这类），所以不重启就可能仍在跑旧代码。

**为什么不用 `install-rospinyin.exe /step1`**：它的退让链是「改名 aside 再写」，
依赖"运行中的 image 可以改名"——NT6 允许，ReactOS 上未必（用户 2026-11-19 手测 `ren` 也失败）。
退让链全失败时它返回 `kNeedsReboot` 并提示重启，等于多绕一步。只想换文件就走上面的路；
要走安装器也行，但**别指望它能省掉"解除默认"这一步**。

**装没装上的判据**（别只看时间戳）：`dir C:\ReactOS\system32\rospinyin.ime` 的字节数要和构建产物一致
（`main` 上的 X2-e1 版是 **948,481** B；X2-d 版 947,969 B，trace 版 971,982 B）。
`rospinyin.dat` 本轮未变（902,776 B）。

**sha256 现在可以用了**（X2-e1 起）：链接加了 `-Wl,--no-insert-timestamp`，同一份源码在两个不同构建目录
编出来的 `.ime` 是**逐字节相同**的（实测 sha256 `134f7fd4…`，948,481 B）。在这之前 ld 会把链接时刻写进
PE 头，两份产物只差 6 个字节、sha256 全不一样——那时哈希只能回答"是不是同一个文件"，
回答不了"是不是同一次构建"。

**版本对账**：`python3 tools/artifact_hashes.py --expect <构建产物的路径片段>` 会把仓库里所有 `.ime/.dat`
和共享目录副本一起算 sha256，标出「共享目录那份是不是 ★」「哪些文件同字节数但内容不同」。
本仓库实测就有 **940,888 B 三份不同内容、929,030 B 两份不同内容** —— 所以"大小一样"从来不是证据。

**guest 侧核对**：ReactOS 没有 `sha256sum`/`certutil`/PowerShell，字节数是唯一现成的判据，而它不够。
所以随包给一个 `rhash.exe`（`tools/rhash/rhash.c`，自带 SHA-256，只依赖 kernel32+msvcrt）：

```bat
wget http://10.0.2.2:8000/rhash.exe
rhash.exe                                  REM 默认算 system32\rospinyin.ime 与 .dat
rhash.exe C:\ReactOS\system32\rospinyin.ime
REM 输出：<64 hex>  <bytes>  <path>   —— 与 artifact_hashes.py 打印的 sha256 逐字符比
```

它用自己的 SHA-256 而不是 CryptoAPI：ReactOS 的 CryptoAPI 实现不全，且引进来就多一条
`advapi32` 依赖。`check_m1.py` 钉住三点：必须有初始常量（证明是自己算的）、源码不得出现
`Crypt`、不得用 `%llu`（msvcrt 不保证，见 §11.4）。三处各变异一次都变红。

### 6.5 VM 运行与观察

```bash
cd m0/vm
./start-vm.sh 6080        # 一键起开发机（QEMU + noVNC 桥 + HTTP/FTP，幂等）
./start-vm.sh 6081        # nightly 验收机（reactos-nightly.qcow2，VNC 5902）
./start-vm.sh 6082        # stable 验收机（reactos-stable.qcow2，VNC 5903）
./start-vm.sh 6081 status # 只看状态；stop 只停该机 QEMU，不动共享 HTTP/FTP
# 旧入口 ./start-6080.sh 等价于 ./start-vm.sh 6080（兼容包装）
```

- **三档 profile**（`start-vm.sh`，本地辅助脚本，`.git/info/exclude` 不入库）：6080=`reactos.qcow2`/VNC 5901；6081=`reactos-nightly.qcow2`/VNC 5902；6082=`reactos-stable.qcow2`/VNC 5903。每档独立 monitor socket、pidfile、日志与 `-name`，按磁盘名区分进程。
- 关键参数在 `m0/vm/common.sh`：`-m 512 -machine pc,accel=kvm:tcg`、`rtl8139` + SLIRP 用户网络、`usb-tablet`、`-serial file:logs/rosdbg*.log`、`-monitor unix:monitor*.sock`、`-vnc 127.0.0.1:<display>`；可由 `ROSPINYIN_HDD`/`_VNC_DISPLAY`/`_MONITOR`/`_VM_NAME`/`_SERIAL_LOG` 覆盖（默认保持 6080 兼容）。
- **沙箱坑**：Bash 工具里 `nohup` 起的 QEMU 会在工具调用结束时收到 SIGHUP 退出；需用受管后台任务启动脚本。用户真机终端直接跑脚本无此问题。
- 三条观察通道：
  1. **VNC** `127.0.0.1:5901`（display `:1`）；
  2. **noVNC**（浏览器）：`python -m websockify --web noVNC-1.5.0 6080 127.0.0.1:5901`，访问 `http://<host>:6080/vnc.html`；或直接跑 `m0/vm/noVNC-1.5.0/utils/novnc_proxy`；
  3. **串口日志** `m0/vm/logs/rosdbg.log`（ReactOS KDBG/DbgPrint 默认走 COM1）。
- **传文件**：宿主机 `python3 -m http.server 8000 --bind 0.0.0.0 --directory m0/share`，guest 访问 `http://10.0.2.2:8000/`（SLIRP 内建）。
- **截屏**：`hmp "screendump out.ppm"`（`common.sh` 提供 `hmp` 函数，走 monitor.sock）。
- **guest 时钟**：QEMU 默认 `-rtc base=utc`，CMOS 存 UTC；guest 若把时区留成 UTC-08:00，
  任务栏会比宿主本地慢 16 小时，`dir` 的日期全错（2026-11-19 实测：宿主 14:36，guest 显示前一天 22:30）。
  用 `python3 tools/vm_clock.py` 看差值，`--reg` 生成时区 `.reg`（guest 导入后重启，持久），
  `--guest` 打印手敲的 `date`/`time` 两行（临时，重启即失效）。**脚本只打印，不代敲。**
  不要用 `-rtc base=localtime` 去"修"：那会让 CMOS 存本地时间、guest 再减自己的时区，错得更隐蔽。
- **控制**：`hmp "quit"` 优雅关机；若 monitor.sock 失效，`kill -TERM <qemu pid>`。
- ⚠️ 这些服务（QEMU / websockify / http.server）**交接时应确认已停止**（本次交接已全部关闭并核对端口 5901/8000/6080 无监听）。

### 6.6 诊断 trace（`ROSPINYIN_TRACE`）

```bash
# 本机交叉工具链的 toolchain file 已就绪，直接复用缓存里的配置
cmake -S . -B build-win-trace -DROSPINYIN_TRACE=ON \
      -DCMAKE_TOOLCHAIN_FILE=/home/twipty/.workbuddy/binaries/mingw-i686/toolchain-i686.cmake
make -C build-win-trace rospinyin-ime -j4
```

- 产物把每行追加到 `C:\rospinyin-trace.log`（`OPEN_ALWAYS`，**从不截断**）；正式构建里这段代码整体编译掉（可用 `objdump -p` 看有无 `CreateFileA`/`GetLocalTime` 导入来确认）。
- 行前缀形如 `[19:23:01.123 +123456ms] p=1234 t=1234 正文`，两个时钟各有分工：
  - **墙钟 `HH:MM:SS.mmm`**：跨重启唯一能区分轮次的字段。日志是追加写、永不截断，只有 tick 的话两次开机的行无法分辨，"修前/修后"也就无从对比；
  - **`+Nms`**：`GetTickCount`（开机毫秒），任意两行的时间差就是一次减法；它同时是 `imhost.exe` 自己日志用的时钟，两个文件的先后关系因此可对齐。
- 格式实现与 `kTracePath` 都在 `framework/platform/win32/imm32/trace_stamp.h`（header-only、无 CRT、不分配内存，`DllMain` 里也能用）；路径只定义一次，避免 `dllmain.cpp` / `imm_entry.cpp` 各写一份而漂移。
- 排查前先 `del C:\rospinyin-trace.log`：旧行只有 `[tick]`，混在一起会让"这次到底有没有新行"变成误判。
- `tools/check_m1.py` 里"框架不得含拼音知识"这一条会先剔除 `ROSPINYIN` 产品名再匹配 —— 否则仅打开 trace 通道就会误报（`ROSPINYIN_TRACE` 不是拼音知识）。


---

## 7. 配置与环境依赖

### 7.1 工具链

| 需求 | 说明 |
|---|---|
| CMake ≥ 3.16 | 顶层 `CMakeLists.txt` 声明 |
| C++17 编译器 | 宿主 g++ / clang++；Windows 目标 MinGW-w64 i686 |
| Python 3 | `check_m1.py`、`fetch_data.py`、`make_pack.py`（标准库即可，无第三方依赖） |
| MinGW-w64 i686 + ninja + windres | 仅打 Win32 产物 |
| QEMU（i386） | `~/.workbuddy/binaries/qemu-user/usr/bin/qemu-system-i386`，`common.sh` 用 `QEMU_HOME` 覆盖 |
| noVNC 1.5.0 / websockify | 浏览器观察 VM（`m0/vm/noVNC-1.5.0`，`novnc.tar.gz` 可复原） |
| tesseract（可选） | 截图 OCR 辅助自动化；英文需 `eng.traineddata`，用 `TESSDATA_PREFIX` 指到自备目录 |

### 7.2 环境变量

| 变量 | 用途 |
|---|---|
| `TMPDIR` | 本机 `/tmp` 只有 10MB 且只读，**必须**指向 `~/.workbuddy/tmp`（`.bashrc` 已导出） |
| `ZIG_GLOBAL_CACHE_DIR` / `ZIG_LOCAL_CACHE_DIR` | 用 zig 做 MinGW 头交叉检查时指向可写目录 |
| `TESSDATA_PREFIX` | OCR 语言模型目录 |
| `QEMU_HOME` | 覆盖 QEMU 安装前缀 |

### 7.3 数据与注册表

- 数据文件：`C:\ReactOS\system32\rospinyin.dat`（缺失时用内置 ~500 字兜底表，仍可输入常用字）。
- 安装信息：`HKCU\SOFTWARE\RosPinyin\Install`（动态 HKL、安装路径）。
- 布局键：`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<HKL:08X>`（`Layout Text` / `Layout Display Name`）。
- 布局名唯一口径：`ime/rospinyin.h` 里的「中文(简体) - RosPinyin」，DLL/资源/安装器三处必须一致（`check_m1.py` 会查）。

### 7.4 字体

- **UI 兜底链**（见 `framework/platform/win32/ui/ui_window.cpp`）：`Microsoft YaHei → SimSun → Noto Sans SC → 系统默认`；VM 证据用 `m0/share/NotoSansSC-subset.ttf`。
- **`[快]/[全]` 字形模式**：`[快]` 只显示 tier 0（Droid 保底可绘）；`[全]` 显示全量并按码点平面切换文津明朝 P0/P2/P3 渲染。`Ctrl+Shift+G` 切换，**会话级、零持久化**。**必须三字体齐备才能正常显示，缺少任意一个的后果不可预知**。
- **文津字体不随包分发**：从[官方发布页](https://github.com/takushun-wu/WenJinMincho/releases/tag/v2.100)下载 `WenJinMincho-TTF.7z`（SHA-256 `22ab30a4…`），解压得 `WenJinMinchoP0/P2/P3-Regular.ttf`（哈希见 `docs/releases/alpha0.0.4.md`），逐个安装并重启。
- **nightly Droid 包**：0.4.17-dev 缺 `DroidSansFallback.ttf` 致中文显示 `?`；`tools/droid-font-pack/build_pack.py` 用固定原版字体（SHA-256 `82a74d0e…`）生成包，`install-rospinyin.exe /fontinstall DROID.TTF` 安装，`/u` 仅清理本包副本。0.4.16 stable 不需要。
- 注意区分：字形开关 `[快]/[全]` 是会话级；M5 整句开关是**配置持久**（见 §3.2）。

---

## 8. 关键流程

### 8.1 一次按键（最常打交道的路径）

1. IMM32 判 IME 打开 → `ImeProcessKey` → `should_consume_key()`：
   - Ctrl/Alt 按下 → 放行（应用快捷键可用）；
   - `Scheme::input_char(vk)` 命中字母表 → 返回 TRUE；
   - 退格/Esc → 仅组合中才拦截；
   - Enter → 仅"中文模式 + 组合非空"才拦截（原文上屏）；空组合/英文模式透传给应用换行；
   - 选字/翻页键 → 仅"组合中且有候选"才拦截。
2. `ImeToAsciiEx` → `process_key()`：
   - 选字键 → `pager.select(digit)` 取候选 → `finish_composition()` → `emit_text()`；
   - 翻页键 → `pager.next/prev_page()` → 刷新 UI，返回 0；
   - 退格 → `engine->backspace()`；
   - Esc → `finish_composition()`；
   - Enter → `engine->commit_raw(session.raw_composition())`：`Result.action=Commit`，按原文提交并清空组合（大小写/撇号原样，候选为空也上屏；第二次 Enter 透传）；
   - 字母 → `engine->feed(cp)`，按 `Result.action` 分派。
3. `Commit` → `emit_text()` 写入 `list->TransMsg`，返回条数 → IMM32 逐条投递 `WM_IME_CHAR`。

### 8.2 上屏（多码点）

`imfw::emit_text(slots, capacity, u32text)`：按 UTF-16 码单元拆，非 BMP 生成代理对，容量不足时**不留半个代理对**，上限 `kMaxEmitMessages = 32`；宿主层再与 IMM32 给的 `uMsgCount` 取最小。同一逻辑在 Linux 单测覆盖（`tests/emit_text_test.cpp`），宿主层只做布局断言 + 转发。

### 8.3 选字与翻页

`CandidatePager` 持有当前页候选；`select(index)` 的 index 是**页内相对下标**（所以第 2 页按 `1` 选的是第 10 个而不是第 1 个）。键位策略由 Scheme 给（默认数字/空格选字，`-`/`,`/`=`/`.` 翻页），框架只负责在合适的时机拦截。

### 8.4 配置与热重载

`ImeConfigure` → `Scheme::configure(parent)`：
- 返回 `false`（默认）→ 拒绝，行为同旧版；
- 返回 `true` → 框架 `reload_sessions()`：**先** `scheme.reload_data()`（重读 .dat、换代 shared_ptr）→ **再** `states_.clear()` → 下一键以新数据重建引擎。旧会话持有的 `shared_ptr` 保证换表瞬间不悬空。

### 8.5 CI 与产物发布

`.github/workflows/ci.yml` 四个 job：

| job | 内容 |
|---|---|
| `engine-smoke` | 宿主侧 Win32 语法检查（`g++ -fsyntax-only` + `tools/syntax_stub`）+ cmake 构建 + ctest + 构建 `rospinyin.dat` + 上传数据 |
| `windows-build` | msys2 MINGW32 下两套配置构建（含 `ROSPINYIN_BUILTIN_TABLE=OFF`）+ PE 导入表断言 + 暂存 `stage/` + 上传产物 |
| `publish-artifacts-branch` | 仅 `main` push 触发：把二进制与数据提交到 **`m1-artifacts` 分支**（force push，最新一次覆盖），供 VM 拉取 |
| `release` | **tag push 即发版**：tags 匹配 `alpha*`/`beta*`/`v*`，用本次运行的产物建 release 并回验哈希；`alpha*`/`beta*` 为 prerelease，`v*` 为正式版 |

`m1-artifacts` 分支里的 `INFO.txt` 记录 `source commit` 与构建时间 —— **VM 验收前先核对它等于你期望的提交**。
发版纪律：先写 `docs/releases/<tag>.md` 再打 tag（`check_release_version.py` 强制），tag 推送后 CI 自动建 release；不存在手工上传路径。

**版本上限纪律（owner 2026-11-26 定）**：本项目版本号（tag 与 `ime/version.h`）**不得超过当前所使用测试 guest 的最大 ReactOS 版本号**。上限从 `m0/vm/*.sh` 中 guest ISO 文件名实时解析——当前 6081 nightly 为 `reactos-bootcd-0.4.17-dev-1398-gdbbf9a2`，6080/6082 为 `ReactOS-0.4.16-i386`，取最大值 `0.4.17`；由 `check_release_version.py` 作为硬门禁校验。guest 升级后上限自动跟随，不另写第二份版本常量。

### 8.6 VM 验收（M2）

按 `docs/milestones/m2/M2-ACCEPTANCE.md` 执行：产物入 system32 → 重启 → Alt+Shift 切到「中」→ 记事本 → 15 项用例（含词组上屏 ≥2 字，这是唯一能在真机暴露 TRANSMSGLIST ABI 缺陷的用例）。结果回填表格并截图存 `m0/vm/evidence/`。

---

## 9. 测试与质量门禁

| 门禁 | 命令 | 通过标准 |
|---|---|---|
| 宿主单测 | `ctest --preset linux-ci`（或 `ctest --test-dir build` / `build-host-off`） | **24/24**（两配置各 24；新增 lexicon-guard 四态测试。旧记录 20/20 是未重配的陈旧 build 目录读数） |
| 契约静态检查 | `python3 tools/check_m1.py --root .` | **120/120**（SPDX、16 导出、加载期调用白名单、TRANSMSGLIST ABI、data-first reload；M4 新增 4-RCDATA、lexicon_guard 固定值、加载链硬失败、安装器三文件对称断言） |
| 文档门禁 | `python3 tools/check_docs.py .` | **7/7**（根目录仅 README、索引完整、相对链接可达、release 命名匹配） |
| 版本门禁 | `python3 tools/check_release_version.py <tag>` | 全绿（version.h 与 tag 一致、发布说明存在） |
| Win32 语法检查 | CI 的第一步，见 §6.3 | 0 error（用 `tools/syntax_stub` 提供的 Windows 头） |
| PE 导入表 | CI `objdump -p` | 不得出现 Vista+ 导出或非系统 DLL |
| 真机验收 | `docs/milestones/m2/M2-ACCEPTANCE.md` | 15 项全绿 |

> 本地若装了 `ziglang`，可用真实 MinGW 头做交叉验证（比语法桩更可信）：
> `zig c++ -target x86-windows-gnu -c -o /tmp/x.o -I framework/platform/win32/imm32 -I framework/include ...`
> 注意：`-fsyntax-only` 在 zig 驱动下会因中间文件重命名失败，**必须用 `-c`**。

---

## 10. 常见坑与注意事项（血泪版）

### 10.1 ABI / 布局

1. **`ImeToAsciiEx` 收 `TRANSMSGLIST`，不是裸数组**。容量头在偏移 0，消息从偏移 4 开始；返回值 = 写入条数。写偏移 0 = 真机全乱。
2. `TRANSMSGLIST` 成员名沿用了 DDK 的 `TransMsg`（与同名 struct 冲突），GCC 会报 `-Wchanges-meaning`（**clang 不报，CI 的 g++ 会**）。头文件里用 `#pragma GCC diagnostic` 精确压制，不要改名。
3. `offsetof(TransMsgList, TransMsg) == 4` 只在 **ILP32** 成立；LP64 主机上 `WPARAM` 是 8 字节，offset 变 8。断言必须局限在 Win32 目标，否则 Linux 语法检查必挂。
4. `TRANSMESSAGE` 是 12 字节（UINT/WPARAM/LPARAM 各 4），与 `imfw::TransMsgSlot` 靠 `static_assert` 绑定。

### 10.2 Windows / ReactOS 特有限制

5. **DllMain 只能注册 UI server 窗口类**（M0 红线 #1）；任何构造引擎/建窗口/读文件都可能引发加载期死锁或 BSOD。
6. **不要 `std::mutex`**：MinGW 的 POSIX 线程模型会把 `libwinpthread` 拖进 PE，裸 ReactOS 没有该 DLL。
7. **不要 MSVC**：静态 CRT 导入 `InitializeCriticalSectionEx`，ReactOS 0.4.16 缺此导出（表现为装完就报"找不到入口点"）。
8. MSVC 需要 `/utf-8`（源码 UTF-8 无 BOM 且含中文串）；windres 需要 `-c 65001`（不认 `#pragma code_page`）。
9. 系统目录是 `C:\ReactOS\system32`，写错路径 copy 会报 "cannot find the path specified"。
10. 改 `.ime` 后**必须重启** guest，IMM32 不会热替换已加载的 DLL。
11. `Ctrl+Space` 在本 VM 切换不可靠，用 **Alt+Shift**。

### 10.3 构建 / 数据

12. `rospinyin.dat` 的输入必须走 `fetch_data.py`（sha256 锁定），不要手工替换文件——mkdat 输出确定性依赖版本一致。
13. `tools/mkdat/mkdat` 是宿主工具，**不链接任何 Windows 头**；别把它挪进 `if(WIN32)`。
14. 图标是 base64 入库 + CI 解码，直接提交 `.ico` 二进制会在 `push_files` 通道里丢失。

### 10.4 VM / 自动化

15. VNC 键盘注入丢 Shift：`:` 要 `Shift+Semicolon`、`/` 要独立 `Slash`、连续 `//` 会合并成一个（需切换 keysym 或加延迟）。
16. 截图 OCR：系统 tesseract 常只有 `chi_sim`，英文识别需自备 `eng.traineddata` 并设 `TESSDATA_PREFIX`。
17. `vncdotool` 连 QEMU VNC 可能被拒；现成方案是 `~/.workbuddy/tmp/rfb_client.py`（最小 RFB 3.8 客户端，raw encoding）。
18. monitor.sock 可能是**陈旧文件**（connect 报 ConnectionRefused）——别据此判断 VM 没跑，用 `ss -tln | grep 5901` 核实。

### 10.5 仓库 / 环境

19. **沙箱内 `git push` 不可用**：`~/.git-credentials` 是空文件，且 `~/.gitconfig` 有 `url.<gh-proxy>.insteadof https://github.com/` 重写。推远端走 **GitHub 连接器 MCP `push_files`**（见 `~/.workbuddy/tmp/ros_gh_sync.py` 的做法：读 `CODEBUDDY_MCP_CONFIG` 的 MCP 端点，逐文件派生 blob SHA 校验）。
20. **怀疑"改动被回滚"时先算 patch-id**：远端历史曾被外部同步机制重写/重放，本地与远端的提交哈希不同但补丁可能完全一致（`git patch-id --stable` 对比）。本次交接就靠这招排除了"文件丢失"的误判。
21. 本地与远端可能存在**无关历史**（`git merge-base` 为空）。此时不要 `push`（会要求 force），以远端为发布真源、本地提交保留即可。
22. `/tmp` 只有 10MB 且只读：任何临时文件写 `~/.workbuddy/tmp`（`TMPDIR` 已在 `.bashrc` 导出）。
23. 沙箱是独立 PID 命名空间，**看不到宿主的 QEMU/服务进程**。需要操作它们时用：
    `XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus systemd-run --user --pipe --quiet -- <cmd>`
    （该命令运行在宿主用户会话里，`ps`/`kill` 都生效）。
24. **`push_files` 通道不传文件模式位**，远端 `m0/vm/*.sh`、`tools/mkdat/build_dat.sh`、`vendor/fetch-vendor.sh` 都是 `100644`。所以从 GitHub 全新 clone 后**不能直接 `./run-reactos.sh`**，要先
    `chmod +x m0/vm/*.sh tools/mkdat/build_dat.sh vendor/fetch-vendor.sh`（或统一用 `bash <script>` 调用）。
    同理，`m0/vm/evidence/` 下的 PNG 与 `ime/rospinyin.ico` 这类二进制在远端并不存在（本地产出后未入库），
    它们不是构建必需物——图标由 `ime/rospinyin.ico.b64` + CI 解码提供，证据图仅作本地归档。

---

## 11. 已知问题与下一步

### 11.1 ⛔ M2 阻塞：VM 内按键未被 IME 拦截

**现象**：Alt+Shift 切到 `中文(简体) - RosPinyin`，托盘红色「中」可见，但记事本里敲 `zhong` 直接上屏英文字母，无组合串、无候选窗。

**已排除**：
- `.ime` / `.dat` 确实在 `C:\ReactOS\system32`（字节数与产物一致）；
- 重启后布局仍在、托盘图标在、切换正常 → IME 被 IMM32 正常加载；
- TRANSMSGLIST ABI 缺陷（2026-11-17 已修）与本现象无关——那条路径只在"写消息"时才触发，而现在连 `ImeProcessKey` 都没走到。

**待查方向**（建议按序）：
1. ReactOS `ImmProcessKey` 的调用条件：`fOpen`（IME 是否打开）、`ImmIsIME`、键盘布局的 IME 标志；
2. `ImeInquire` 返回的 `fdwProperty`（当前 `IME_PROP_UNICODE|IME_PROP_AT_CARET`）是否让 ReactOS 判定为"可处理按键"；
3. `ImeSelect(TRUE)` / `ImeSetActiveContext` 是否真的被调到（可在实现里加 `DbgPrint`，看 `rosdbg.log`）；
4. ReactOS 侧 `win32ss/user/imm32` 源码里 `ImeProcessKey` 的实际调用点与条件。

**2026-11-17 源码级排查结论（读 ReactOS 0.4.16 源码，未上机）**：
- **方向 1 的 `fOpen` 假设已排除**：`win32ss/user/imm32/keymsg.c` 的 `ImmProcessKey()`（L736-790）在调用 `pImeDpi->ImeProcessKey()` 前只检查 `ImmLockImeDpi(hKL)`、`ImmLockIMC(hIMC)`、`GetKeyboardState()`，`fOpen` 只出现在 `VK_PACKET` 分支；不打开 IME 也会调 `ImeProcessKey`。
- **完整拦截链**：`user32!TranslateMessage`（`windows/message.c:2826`）对 `WM_KEYDOWN` → `NtUserTranslateMessage` → `ntuser!IntTranslateKbdMessage`（`ntuser/keyboard.c:1499`，只做 ToUnicode，**不调 IME**）；IME 介入点在 `ntuser/ime.c:603 IntImmProcessKey` → `co_IntImmProcessKey` → `imm32!ImmProcessKey`（keymsg.c:736）→ 我们的 `ImeProcessKey`。`IntImmProcessKey` 的硬门槛是 `pKL->piiex`（L676-679，来自 `ImeInquire` 加载），无 `piiex` 直接 `return 0`。
- **关键门槛（最可疑）**：`IntImmProcessKey` L691-699 与 `ImmProcessKey` 中，若 `fdwProperty` 无 `IME_PROP_NEED_ALTKEY`，则 `uVK==VK_MENU` 或 `lParam & KF_ALTDOWN` 的键被丢弃；这与 Alt+Shift 切换后紧接着按键的场景相关。另一路：只有 `ImeProcessKey` 返回 TRUE 置 `IPHK_PROCESSBYIME` 且 `_IME_APP_COMPAT_PROCESS_BY_IME` 兼容标志置位时，才注入 `WM_KEYDOWN:VK_PROCESSKEY`（keymsg.c:819-830）；否则用户键不会转成 IME 转换路径。
- **与 mzimeja 参考实现的字段差异**（`vendor/mzimeja-ref/ime/imm.cpp:27-49`）：其 `fdwProperty = KBD_CHAR_FIRST|UNICODE|AT_CARET`、`fdwConversionCaps = LANGUAGE|FULLSHAPE|ROMAN|CHARCODE`、`dwPrivateDataSize = sizeof(UIEXTRA)`、`fdwSelectCaps = SELECT_CAP_CONVERSION`；我们对应为 `UNICODE|AT_CARET` / `0` / `0` / `0`。这些差异是否致命需上机验证。
- **本机能力边界**：QEMU 在 `~/.workbuddy/binaries/qemu-user`（可跑 VM、串口日志 `rosdbg.log`）；但**本机无 MinGW 交叉编译器**（CI 用 MSYS2 产 `.ime`），改宿主代码后需走 CI 重编译才能进 VM 验证。已编译产物在 `origin/m1-artifacts`（`rospinyin.ime`）。

**闭环要求**：修复后重跑 `docs/milestones/m2/M2-ACCEPTANCE.md` 全部 15 项，重点词组上屏（≥2 字）。

**2026-11-17 源码级排查结论（第二轮：定位到唯一的硬门槛 `pWnd->hImc`）**

把 0.4.16 的 `ntuser/ime.c`、`ntuser/msgqueue.c`、`ntuser/window.c`、`imm32/imm.c`、`imm32/keymsg.c` 逐个读完，结论是**链条在进入 `ImmProcessKey` 之前就断了**，上一轮列的方向 1/2/3 全部位于这道门之后，不是本现象的原因：

- 调用点：`ntuser/msgqueue.c:1877` `if (pWnd && Ret && *RemoveMessages && bKeyUpDown && !(pti->TIF_flags & TIF_DISABLEIME))` → `IntImmProcessKey()`。
- 唯一硬门槛在 `ntuser/ime.c:674-682`（`dwHotKeyId == INVALID_HOTKEY` 分支，即普通输入键）：
  ```c
  if (!pKL->piiex) return 0;
  if (pWnd->hImc) pIMC = UserGetObject(gHandleTable, pWnd->hImc, TYPE_INPUTCONTEXT);
  if (!pIMC) return 0;          /* <-- 没有 IMC 就到此为止 */
  ```
  只有越过这里才会走到 `co_IntImmProcessKey()` → `imm32!ImmProcessKey()`（`keymsg.c:757` 里还要 `ImmGetContext(hWnd)` + `ImmLockIMC()` 再非 NULL，条件等价）。
- `pWnd->hImc` 在 ReactOS 0.4.16 里**只有两条赋值路径**：
  1. `ntuser/window.c:1913` 创建窗口时若线程已有默认 IMC，自动挂上：
     `if (pti->spDefaultImc && Class->atomClassName != gpsi->atomSysClass[ICLS_BUTTON]) pWnd->hImc = UserHMGetHandle(pti->spDefaultImc);`
  2. `ntuser/ime.c:528 IntAssociateInputContext()`，由 `ImmAssociateContext` / `ImmAssociateContextEx` 触发。
- 而 `spDefaultImc` 只由 `NtUserCreateInputContext`（即应用显式 `ImmCreateContext`）建立；`co_IntCreateDefaultImeWindow()`（`ime.c:2077`）除了 logon 进程外不会替应用补建。
- 关键：ReactOS 的 `ImmGetContext()` **不会惰性创建 IMC**。`imm32/imm.c:1065 ImmGetContext` → `ImmGetSaveContext(hWnd, 2)`（`imm.c:1022-1060`），`dwContextFlags=2` 只检查 `CLIENTIMC_DISABLEIME`，**不含 1**，所以不会回退到 `QUERY_WINDOW_DEFAULT_ICONTEXT`，`pWnd->hImc` 为空时直接返回 NULL。（Wine 在此处会建默认 IMC，ReactOS 没有。）
- 结论：记事本的 `EDIT` 控件从不创建 IMC，于是 `pWnd->hImc == 0` → `IntImmProcessKey` 在 `if (!pIMC) return 0;` 提前返回 → **`ImeProcessKey` 永远不会被调用**。这与 `C:\rospinyin-trace.log` 里只有 `ImeInquire`、没有 `ImeProcessKey` 完全吻合；也解释了 `ImmIsIME` 返回 1（该 API 只查注册表）、键盘布局切换正常（`pti->KeyboardLayout` 与 IMC 无关）却敲不出中文。
- 推论（下一步的两条路）：
  - **A. IMC 兜底兼容层**：宿主 `.ime` 在自己进程内（进程已随布局切换加载）为焦点窗口补 `ImmCreateContext` + `ImmAssociateContext`，让 ReactOS 上任何应用都能进入 IME 链条。需评估与 M0「裸 `LoadLibrary` 必须无害」红线的冲突（放到首次按键的懒路径里，不进 `DllMain`）。
  - **B. 自建宿主验证 M2**：用一个自己保证 IMC 存在的宿主窗口跑 `M2-ACCEPTANCE.md`，先把引擎侧验收做掉，兼容层单独推进。
- 取证工具：`imhost.exe`（在 `~/.workbuddy/tmp/vmshare/`，源码 `imhost.cpp`）——先 `ImmCreateContext` 再建窗口（命中路径 1），再显式 `ImmAssociateContext` 兜底（命中路径 2），逐条记录 `WM_KEY*` / `WM_IME_*` 与 `wParam == VK_PROCESSKEY(0xE5)`，日志同时落 `D:\imhost.log` 并 POST 回宿主机 `10.0.2.2:8080`，无需手工拷贝。

**2026-11-17 源码级排查结论（第三轮：真正的原因 —— UI 类名比 imm32 的缓冲长 1 个字符）**

上一轮把锅指给"应用不建 IMC"，那是另一件事（影响记事本这类应用），不是"IME 装不上"的原因。决定性证据来自同一进程内的一次 A/B：

| 观测 | 我们的 .ime | 系统自带 mzimeja |
|---|---|---|
| `ImmInstallIMEW` 之后、未查询属性前 `fLoadFlag` | 0 | 0 |
| 查一次 `ImmGetProperty` 之后 | **1**（prop=0x00000000） | **0**（prop=0x00190002） |
| trace | 只有 `process detach`（说明 `LoadLibrary` 成功过，随后被 `FreeLibrary`） | — |

同机、同 API、同一次开机 → **不是环境脏，也不是 IMM 链坏了**；是 `imm32!Imm32LoadIME` 在 `Imm32InquireIme` 之后 `FreeLibrary` 并把 `fLoadFlag` 钉成 1。而 `Imm32InquireIme` 末尾是：

```c
WCHAR szUIClass[64];                                    // imm32_ime.c:47
pImeDpi->ImeInquire(pImeInfo, szUIClass, dwSystemInfoFlags);
StringCchCopyW(pImeDpi->szUIClass, 16, szUIClass);      // IMEDPI.szUIClass[16] —— 只放 15 字符 + NUL
if (!GetClassInfoW(pImeDpi->hInst, pImeDpi->szUIClass, &wcW)) return FALSE;
```

我们的类名原来是 `RosPinyinUIServer`（**17 字符**）→ 被截成 `RosPinyinUIServe` → `GetClassInfoW` 查不到 → 整条加载失败。这就是"IME 从不被调用"的全部原因：**类名长了 2 个字符**。

为什么之前摸不到：探针一直用 `ImeInquire` 返回的**完整**类名去 `GetClassInfoEx`，当然通过；imm32 用的是**截断后**的名字。`IMEINFOEX.wszUIClass` 也只有 16，所以 `ReadIex` 打出来的 `uiclass=[]` 正是因为失败分支根本没回填它（`fLoadFlag==0 && ret` 才写）。

修法：
- `pinyin/src/scheme.cpp` 的类名改为 `RosPinyinUISrv`（14 字符），并加 `static_assert(长度 <= 15)` 把契约钉死；
- `imfw/scheme.hpp` 在 `ui_server_class()` 上写明"至多 15 字符"及失败症状（否则下一个人还会踩）；
- `tools/check_m1.py` 增加一条门禁：`kUiServerClass` 长度 ≤ 15；
- 探针新增一条判据：同时用"前 15 字符"查类（那才是 imm32 真正用的名字），超长会直接打印警告。

**2026-11-17 用户发现：imhost 不是有效的 IME UI 宿主（必须用 `plain` 模式做判据）**

用户观察到：**系统自带日文输入法 mzimeja 在 imhost 里唤不出状态工具条，同一时刻在记事本里完全正常**（候选窗贴在插入符下方、右下角 `IME ON / あ R 式` 状态条都在）。

日志证明这不是"IME 没加载"：

```
[03:41:42.070] WM_INPUTLANGCHANGEREQUEST flags=0x1 hkl=E0200409
[03:41:42.100] WM_INPUTLANGCHANGE charset=0x0 hkl=E0200409   ← 布局清单里 E0200409 = mzimeja
[03:41:42.100] WM_IME_NOTIFY wParam=0x0006                   ← IMN_SETCONVERSIONMODE
```

mzimeja 已经被加载并激活（否则不会有 `IMN_SETCONVERSIONMODE`），只是**它的 UI 窗口出不来**。imhost 与被测应用之间的差异全在宿主自己身上：

1. 启动即 `ActivateKeyboardLayout(g_hklIme, 0)` 把线程钉在我们的布局上；
2. 抢先 `ImmCreateContext()` + `ImmAssociateContext()`，绕开内核 `co_IntCreateDefaultImeWindow()` 要求的 `pti->spDefaultImc` 默认路径（该函数在建 "Default IME" 窗口，`WS_POPUP|WS_DISABLED`，IME 的 UI 消息无处落脚就什么也画不出）；
3. `WM_IME_SETCONTEXT / WM_IME_NOTIFY / …` 全部 `return 0`，等于向 IME 声称"UI 我自己画"，而 imhost 并不画（记事本能出候选窗就是因为它走 `DefWindowProc`）。

**结论：不能用 imhost 的默认模式判断"IME/工具条能不能出来"。** 现在 `imhost.exe` 增加 `plain` 模式（不锁布局、不预建 IMC、IME 消息交回 `DefWindowProc`、跳过全部破坏性探针），同一个 exe 内即可 A/B；日志文件名加 `plain-` 前缀区分。同时 `DumpLayoutInventory()` 会算出"从当前布局按几次 Alt+Shift 能到我们的布局"，便于在记事本里确定性地切到被测布局。

后续验收建议：M2 的"IME 被调用/候选窗出现"这类 UI 判据一律在 **记事本** 或 `plain` 模式宿主里取，imhost 默认模式只用于读 `fLoadFlag`/`IMEINFOEX` 这类内核状态。

**参照链（`ref` 模式）与差异工具 `tools/ime_chain_diff.py`**

"IME 不被调用"这类问题最缺的不是失败现场，而是**正常样本**：只知道我们少了什么，不知道正常该发生什么。所以做了一台自己可控、结构又和被测算应用一致的宿主：

- `imhost.exe ref`：顶层框架窗口 + **真实 `EDIT` 子控件**（`WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_NOHIDESEL`），焦点在控件上 —— 事件链就发生在控件的 HWND 上，和记事本结构一致；再用 `SetWindowLongPtrW(GWLP_WNDPROC)` 子类化，只记录、不拦截，`CallWindowProc` 原样转发，所以被测行为与记事本完全等价。
- 记录内容做了解码：`WM_IME_SETCONTEXT` 的 `ISC_*`、`WM_IME_NOTIFY` 的 `IMN_*`、`WM_IME_COMPOSITION` 的 `GCS_*`，并**读出组合串与上屏串**（`ImmGetCompositionStringW`，非 ASCII 转成 `\uXXXX` 以便逐字节比对）、`cursorpos`；键消息标注是否 `VK_PROCESSKEY`。
- 布局一变就打一条 `======== 场景切换 <旧> -> <新> ime=<文件> ========`，并把 `ime=` 同步到窗口标题栏 —— VM 里一眼能看出"当前落在哪个输入法"，不用翻日志。
- 同一次运行内切布局即可产出两段场景（参照 + 被测），日志名 `imhost-ref-*.log`，不会与其它模式混淆。
- `tools/ime_chain_diff.py`：按 `场景切换` 切段、按 `ime=` 选段，把时间戳/pid/地址全归一化掉，再用 `difflib` 出统一 diff，并在前面给出每类事件计数（`ime-composition×0` 这种一眼就看出缺口）。`--self-test` 不需要 guest 即可自检。

用法：`python3 tools/ime_chain_diff.py ref.log test.log --ref-ime mzimeja --test-ime rospinyin`。

**布局语言（0804 中文键盘 vs 0409 英文键盘）到底影响什么**

用户发现系统日文输入法挂在 **English keyboard（0409）** 下、与 [en] 并列，我们的挂在 **Chinese keyboard（0804）** 下。逐条查过 ReactOS 源码，结论是：

- **不 gate IME 调用链**。内核 `IntImmProcessKey` 的四道门是 `IS_IMM_MODE()`、`pKL->piiex`、`pWnd->hImc`、`IME_PROP_IGNORE_UPKEYS/NEED_ALTKEY`，没有任何语言判断。
- 按语言分支的几处都**不适用**：`Imm32IsSystemJapaneseOrKorean()` 用的是 `GetSystemDefaultLCID()`（系统区域，与布局无关）；`ImmProcessKey`/`ImmSetCompositionFont` 里的日文特判全部关在 `dwExpWinVer < _WIN32_WINNT_NT4`（Win3.x 兼容）里；`UserSetImeConversionKeyState()` 只对 JP/KR 设置 `VK_DBE_*` 开关键，默认分支什么都不做。
- **但它确实制造了两个真实差异**：
  1. `pKL->iBaseCharset`：0409 → `ANSI_CHARSET(0)`，0804 → `GB2312_CHARSET(0x86)`。日志里 `WM_INPUTLANGCHANGE charset=0x86` 与 `charset=0x0` 的差别就来自此，`INPUTLANGCHANGE_SYSCHARSET` 标志（`flags=0x1`）与默认组合字体也随它走。
  2. **它是 A/B 里最后一个没被消掉的变量** —— 至今所有"我们 vs 系统日文输入法"的对比，都同时差着"实现"和"语言"两项。
- **语言从哪来（完全可控）**：`imm32!ImmInstallIMEW` 从 `.ime` 版本资源取 `VarFileInfo\Translation` 的**第一项**当 HKL 低字（`imm32_install.c`：`pInfoEx->hkl = UlongToHandle(*pw)`）。所以我们挂在哪个键盘下，是**构建期**决定的事实。

因此把语言做成构建期开关：`-DROSPINYIN_IME_LANGID=0804|0409`（`ime/CMakeLists.txt`，默认 0804；`ime/rospinyin_res.rc` 里用 `#ifdef ROSPINYIN_IME_LANG_EN` 切 `LANGUAGE` / StringFileInfo 块名 / `Translation`）。0409 版**只作对照件**，`check_m1.py` 有一条门禁守住"默认必须是 zh-CN"。产物：`rospinyin5.ime`(0804) 与 `rospinyin6.ime`(0409)，字节数相同、仅资源语言不同。

**注意**：消除语言变量只是**让实验干净**，不是修 bug。验收必须回到 0804 —— 否则会留下"只在英文键盘下能用"的隐患。

实测验证：同一轮 VM 里 `rospinyin5.ime`(0804) 与 `rospinyin6.ime`(0409) 行为**完全一致**（都能上屏、都缺 `WM_IME_STARTCOMPOSITION`、都双 feed），语言变量正式排除。因此 `install` 模式回到只装 0804 一份。

### 11.4 ⛔ 修复中：每个按键被喂进引擎两次（多字 / 丢字）

**症状**（2026-11-17 晚，IME 首次成功拦截按键之后）：能上屏中文，但输入过程中"有时多出一个字符、有时少一个字符"。

**证据链**（`imhost-ref-final.log` + `rospinyin-trace-ref-final.log`，同一次运行）：

```
trace:  processkey vk=0x4e pf=0x31  -> 1      ← key-down
        toascii    vk=0x4e
        processkey vk=0x4e pf=0xc031 -> 1     ← key-UP，也被认领
        toascii    vk=0x4e                    ← 于是同一字符进引擎两次
```

`pf` 是 lParam 的高 16 位：`0xc031` = bit31（transition=释放）+ bit30（previous state）+ scan code，即 **key-up**。5 个字母 `nihao` 一共产生 10 次 `toascii`。

**机制**（ReactOS 源码）：

1. `co_IntProcessKeyboardMessage` 对 `WM_KEYUP` 也置 `bKeyUpDown = TRUE`（`win32ss_user_ntmsgqueue.c:1785`），于是 key-up 同样进 `IntImmProcessKey`；
2. `imm32!ImmProcessKey` 不看 transition 位，只要 `ImeProcessKey` 返回 TRUE 就置 `pIC->bNeedsTrans = TRUE`（`imm32_keymsg.c:785`）；
3. `ImmGenerateMessage` 见到 `bNeedsTrans` 就调 `ImeToAsciiEx`（`imm32_keymsg.c:1056`）——而 **`ImeToAsciiEx` 的参数里没有 lParam**，无法自行区分按下与抬起。

**后果**：`feed()` 每个字母跑两遍。拼音缓冲出现双写（`nn`/`aa`），`can_extend()` 对其中一部分判定仍可扩展 → **多出字符**；另一部分被判非法后 `pop_back()` → **丢失字符**。同一条 bug 同时解释了两种症状。

**修法**：在 `ImeProcessKey` 里检查 lParam bit31，key-up 一律返回 FALSE。这是唯一能做该判断的地方。

```cpp
constexpr unsigned long kKeyUpBit = 0x80000000UL;
if ((static_cast<unsigned long>(lParam) & kKeyUpBit) != 0) return FALSE;
```

**顺带确认的两件事**（都不是 bug）：

- `WM_IME_CHAR` 紧跟 `WM_CHAR` 是正常成对：前者由 imm32 投递 TRANSMESSAGE，后者是 `TranslateMessage` 转换的结果。
- `zhongguo` + 空格只上屏 `国`（丢 `zhong`）是 **M2 验收单 3.3 的既定口径**（单字引擎，词组属 M4）。
- 我们**没有** `WM_IME_STARTCOMPOSITION` / `WM_IME_COMPOSITION`：ReactOS 的 imm32 **根本没有实现 `ImmSetCompositionString`**（全树无此符号、无 .spec 条目），所以组合串只能自绘（`framework/platform/win32/ui`）。这与记事本里能看到组合串的 mzimeja 不同，但属于平台限制，不是缺陷。

**待确认**：`ImmGetOpenStatus` 的真实返回值。`ImmCreateContext` 会把 `pIC->fOpen` 初始化为 FALSE（`imm32_imm.c:793`），若它一直不翻真，则"IME 关闭时仍吞字母"就无从拦截（用户切英文后打字会整体消失）。trace 构建里已打印 `open=`，但**刻意没有用它做门禁** —— 在拿到真实值之前用它拦截会直接把 IME 打死。

### 11.5 m2 自校验：把"多字/缺字"的判定交给程序

**为什么需要**：11.4 的结论虽然自洽，但"多字/丢字"这个现象此前一直是**人眼读屏幕**得出的。这类判据有三类假阳性无法排除：(1) 注入太快，键根本没到 IME，看起来像丢字；(2) 读错/记错一个字；(3) 把"M2 只上屏末音节"这个既定口径当成丢字。所以要复测，就不能再靠肉眼。

**做法**：`imhost.exe m2` —— ref 的窗口结构（顶层窗口 + 真实 `EDIT` 控件 + 子类化），但**结果由程序判定**：

```
注入 → GetWindowTextW 读回 → 与验收单期望值比对 → PASS / EXTRA / MISS / WRONG / ASCII-LEAK
```

判定标准全部来自 `docs/milestones/m2/M2-ACCEPTANCE.md`；没有明确期望值的用例**只校验汉字个数**，不猜具体是哪个字（猜错就会把正常行为判成 bug）。用例表在启动时整表写进日志，日志自带判定标准，事后可复查。

**16 条用例 × 两轮**（首尾各 1 条为环境/Esc 自检）：

| 类别 | 用例 | 期望 |
|---|---|---|
| 环境自检 | `z` + `Esc` | 0 字（z 必须被 IME 吞掉） |
| 验收单直取 | `zhong`→中、`nv`→女、`lv`→律、`lve`→略、`nve`→虐、`zhongguo`→国 | 各 1 字 |
| 只校验个数 | `guo` `ni` `hao` `shi` `hao`+退格×2 | 各 1 字 |
| 信息性 | `a` | 1 字（引擎未必支持，不计入总分） |
| 取消 | `hao` + `Esc` | 0 字 |

**四道防误判机制**：

1. **两轮不同节奏** —— R1 600ms/键、R2 280ms/键。结论分三种：两轮全过 = 正常；慢过快不过 = **时序敏感**（真实但不是多字/缺字缺陷）；慢速也失败 = 真异常。把"注入太快"从"IME 缺陷"里摘出去。
2. **环境自检用例** —— 若 `z` 直接落进编辑框（`ASCII-LEAK`），说明按键压根没进 IME，整轮结论标为"环境不成立"，不用来判定缺陷。
3. **每条用例前送 Esc 清场** —— 否则一条失败会把组合串带进下一条，连锁失败看起来像"全坏"（污染传播 ≠ 真 bug）。
4. **注入用确定 VK** —— 不用 `VkKeyScanW`（对 `z` 可能返回 `0x7A`，那是 VK_F11，注入的就不是字母），改为 `ch - 32` 直接映射。

另外每条用例记录 `imeChar` / `wmChar` 计数：若读回两个字而 `imeChar=1`、`wmChar=1`，说明同一个字符被 `WM_IME_CHAR` 与 `WM_CHAR` 各插了一次 —— 那是"多字"的另一条通路，必须和"引擎被喂两次"分开。同时统计 `IMN_OPENCANDIDATE` / `IMN_CLOSECANDIDATE` / `IMN_SETOPENSTATUS` / `WM_IME_STARTCOMPOSITION`，让"选词界面有没有出现"也有日志数值，而不只靠人眼。

**本机验证**（无 Windows，抽纯函数 + 复刻判定矩阵）：
- `CountHan` / `EscText` / `VkOf` 行为正确（`z=0x5A`=VK_Z、`\b=0x08`、`Esc=0x1B`）
- 12 条判定矩阵全过：正常上屏 PASS、双字 EXTRA、空 MISS、错字 WRONG、字母漏进编辑框 ASCII-LEAK、Esc 失效上屏判 EXTRA（而不是误判成 PASS）

**用时**：R1 约 68s、R2 约 36s，合计约 1.7 分钟。


### 11.6 键盘三层取证：真实按键 → 窗口 → IME

**问题**：11.4/11.5 的所有日志都产生在"消息已经到达窗口/控件之后"。IME 把键吃掉、焦点不在控件、或消息在 win32k 里被吞时，这一层什么都看不见 —— 于是"丢字"到底是**没敲进去**、**没到窗口**、还是**到了但被 IME 处理两次**，日志本身无法区分，只能靠推断。

**做法**：在 `imhost` 里加一层**入口层**记录 —— 线程级 `WH_KEYBOARD` 钩子。它由 win32k 在 `KeyboardMessage()` 中、**`IntImmProcessKey()` 之前**调用（`ntuser/msgqueue.c:1859`），拿到的是"系统这一瞬间实际收到了哪几个键"：

```
KEY 0007 vk=0x4E(N) sc=0x31 DOWN rep=1
KEY 0008 vk=0x4E(N) sc=0x31 UP   rep=1
```

`lParam` 拆成四段记：重复次数、**扫描码**、扩展位、ALT 上下文、转换位。扫描码由 ps/2 直传，同一物理键应始终一致 —— 同一字符若出现两条不同扫描码，问题在注入侧（`keybd_event`/VNC），不在 IME。

**三层对账**（每轮结束、退出时各打一份增量，R1/R2 独立）：

| 层 | 取值点 | 能证明什么 |
|---|---|---|
| 入口层 | `WH_KEYBOARD` 回调 | 系统到底收到几次按下/抬起 |
| 窗口层 | 子类化 `EditProc` 的 `WM_KEY(DOWN/UP)` | 有多少键真正到达应用 |
| IME 层 | `.ime` 内 `ImeProcessKey` / `ImeToAsciiEx` 的 trace | 输入法实际被喂了什么（含 `pf=` 扫描码+标志） |

```
---- 键盘对账[R1(600ms)] 入口层 DOWN=32 UP=32 | 窗口层 DOWN=32 UP=32 | WM_CHAR=0 WM_IME_CHAR=16 ----
  入口层与窗口层一致（各 32 次按下）：键全部到达窗口，差异只可能出现在 IME 层
```

差值为 0 → 键没进系统（焦点/主控问题）；入口有、窗口无 → 消息在钩子之后被吞；两层一致但 IME trace 少 → 丢在 IMM32 内部；`WM_CHAR` 多于按键 → 同一个键被翻出多个字符（"多字"的另一条通路）。

**为什么不用更底层的两种手段**（ReactOS 0.4.16 源码核实）：

- **Raw Input**（`WM_INPUT` + `RIDEV_INPUTSINK`）：`NtUserRegisterRawInputDevices` 等 6 个函数在 `ntuser/ntstubs.c` 里全是 `STUB; return 0;`，注册必然失败 —— 平台未实现，不可用。
- **`WH_KEYBOARD_LL`**：`ntuser_callback.c` 只实现了该钩子的**参数封送**（`KBDLLHOOKSTRUCT` 拷进用户空间），全树找不到任何触发点；实际唯一的调用点是 `msgqueue.c:1859` 的 `WH_KEYBOARD`。用 LL 钩子会静默拿不到任何回调。
- **真正的最原始层**在 guest 之外（QEMU 的 RFB `KeyEvent`）。要抓这一层得在 websockify/QEMU 前面插代理并重启那条链路，属于环境改造，不是探针能解决的；当前三层已足够定位"丢在哪一段"。

**实现位置**：`~/.workbuddy/tmp/vmshare/imhost.cpp`（`KbProc` / `InstallKeyboardHook` / `KeySummary`）。钩子装失败不致命，日志会打 `SetWindowsHookExW -> NULL err=` 且对账时明示"入口层无效"，此时按两层数据判读。

**两个已修的取证陷阱（2026-11-17 20:58 那轮 m2 暴露）**：

1. **子串匹配选错布局** —— `FindHklByImeFile(L"rospinyin")` 会命中列表第一个 `rospinyin.ime`，那是最早的构建（UI 类名 17 字符的老版本），`ImmGetProperty` 返回 0 → 整轮在开始前即 ABORT，而真正要测的 `rospinyin7.ime` 从未被选中。改为**精确文件名匹配** + 启动时打印候选布局表（每个布局的 `prop` 与是否可用），并在每轮开头**强制从宿主机刷新一次** `.ime`（`ImmInstallIMEW` 按文件名复用布局，不会越装越多）——"测的是旧文件"这个坑在这个项目里已经出现过两次（922022 字节的陈旧 `rospinyin.ime`），不能再靠人记得先 install。
2. **模式标语贴错** —— m2 走的是 `g_vanilla` 分支，标语却按 `g_refMode ? "ref" : "plain"` 打，日志开头写着 `plain`、实际跑的是 m2，事后看日志会误判。

**rec：纯记录模式（不代替用户发键）**

m2 用 `keybd_event` 替人打字，测的是**注入路径**；而"我自己敲的时候输入法收到了什么"是另一个问题——注入没有真人节奏、没有重复键，也绕过了 VNC→ps/2 那一段。所以另有 `imhost.exe rec`：

- **一个键都不发**：所有注入收敛到唯一出口 `InjectKeyEvent()`，rec 模式下直接拒绝并计数；退出时打印"本程序注入按键 N 次"，rec 下必为 0（可核验，不是口头承诺）。
- **不改状态**：不切布局、不装 IME、不清编辑框，只把当前布局/`.ime` 文件大小/`ImmGetProperty` 读出来当上下文（`prop=0` 会警告"敲键不会出候选"）。
- **三层照记**：入口 `KEY`、窗口 `[edit]`、IME `trace`；另外每次上屏文字变化打一行 `TEXT len= 汉字= [...]`，按顺序即为真实上屏结果，不需要人读屏。

用法：`imhost.exe rec` → 自己 Alt+Shift 切布局 → 正常打字 → 关窗口自动上传。

### 11.7 真机手打验证结果（2026-11-17 21:08–21:10，rec 模式）

第一次用**真实人手打字**（非注入）跑三层取证，被测 `rospinyin7.ime`（931,363 字节，布局 `E0260804`，`ImmGetProperty=0x00090000`）。

**三层对账**（退出汇总行原文）：

```
---- 键盘对账[退出] 入口层 DOWN=40 UP=40 | 窗口层 DOWN=40 UP=40 | WM_CHAR=5 WM_IME_CHAR=5 ----
  入口层与窗口层一致（各 40 次按下）：键全部到达窗口，差异只可能出现在 IME 层
本程序注入按键 0 次，被拒 0 次（全程未代替用户发键）
```

**逐字上屏**（`TEXT` 行，每行只 +1 字）：

```
TEXT len=1 汉字=1 [你] → len=2 [你好] → len=3 [你好中] → len=4 [你好中国] → len=5 [你好中国誰]
```

敲的是 `nih`+Backspace+`hao`+空格 → `zhong`+空格 → `guo`+空格 → `shui`，**最后一步不是空格**：候选里没有简体「谁」，用户翻了 4 次 `=`、6 次 `-` 回到某页，按 `5` 选了第 5 项 `誰`（U+8AB0，繁体）。`TEXT` 行记录的是 `\u8AB0` 而不是 `\u8C01`，与"只有繁体的"的口头反馈一致（见 §11.8）。

**IME 层证据**：29 次 `processkey ... -> 1` 对应 29 次 `toascii`（21 次字母 feed + 8 次空格/退格），**1:1**；每次抬起都是 `processkey vk=0xNN up -> 0`，没有 `toascii`、没有 `feed`。这正好验证 §11.4 的 key-up 修复生效 —— 修复前每个键会被喂两次（一次 down 一次 up），现在 up 一律不认领。`WM_IME_CHAR` 与 `WM_CHAR` 各 5 条、上屏 5 字，说明二者是"同一次上屏的两条消息"而非双重插入。

**结论**：M2 的单字输入路径在真实手打条件下**通过** —— 无多字、无丢字、无 ASCII 泄漏，入口/窗口/IME 三层条数自洽。11.4 的"多字/丢字"到本轮为止不再是复现项。

**观察到的两个小问题（未门禁，待查）**：

1. `ImmGetOpenStatus` 读数不一致：imhost 轮询得到 `-1`，`.ime` 内 trace 打 `open=0`。引擎侧已明确"只观测不门禁"，但两个读数都不像正常值（0/1），怀疑 ReactOS 上该 API 对跨线程 IMC 的行为与预期不同，值得单独确认——它决定 Shift 切英文后按键该不该吞。
2. 每敲一个键，应用会收到 1–3 次 `IMN_SETCOMPOSITIONWINDOW`（`n` 那次 3 次）。说明引擎在每次 feed 后都调 `ImmSetCompositionWindow`，重复通知会让宿主反复重排组合窗位置，是"界面轻微抖动"的一个可疑来源。


### 11.8 候选质量：`shui` 没有简体「谁」+ 大量空心框（2026-11-17 21:30 修复）

用户反馈：敲 `shui` 时候选里只有繁体「誰」没有简体「谁」，且很多字显示为一个空心点，问是不是字体问题。

**结论：不是字体坏了，是两个独立缺陷叠加。** 字体只负责"把已有码点画出来"，而这两件事分别出在**读音归属**和**候选里塞了字体画不出的字**。

#### 缺陷 A：谁 被归到 `shei` 音节（读音丢失）

Unihan 简繁两条记录互相不一致：

```
U+8C01 誰?  谁  kMandarin  shéi            kHanyuPinlu  shuí(1065)
U+8AB0 誰      kMandarin  shuí shéi        kHanyuPinlu  shuí(1065)
```

`mkdat::choose_primary()` 以 `kMandarin` 为读音全集、只拿 `kHanyuPinlu` 挑"在全集里最常用的那个"。于是：

- 谁：kMandarin 只给 `shéi` → `kHanyuPinlu` 的 `shuí` **不在全集里 → 被静默丢弃** → 谁只挂在 `shei` 下，`shui` 查不到。
- 誰：kMandarin 给了 `shuí shéi` → 首个是 `shuí` → 繁体反而占了 `shui` 音节。

叠加第二层：`CodeTable::load_dat()` 只读 `reading_off`（主读音）建索引，**完全忽略 `alt_off`**（次读音）。所以就算 `shuí` 被写进 alt，引擎也查不到 —— 多音字"另一个读音能打出来"这件事从未成立。

#### 缺陷 B：候选里混入字体画不出的字（空心框）

`.dat` 44,355 条里只有 **9,898 条有字频**（hanziDB），其余 34,457 条是 Unihan 独有的生僻字，其中 **68%（23,433 条）落在 U+4E00–U+9FA5 之外**（CJK 扩展 A/B 等，多为 U+20000 以上）。而随包字体 `NotoSansSC-subset.ttf` 是按 `0x4E00–0x9FA5` 子集化的（`~/.workbuddy/tmp/font_subset.py`），扩展区一律没有字形 → 一个个空心框。

候选窗每页 9 条，`shui` 旧候选共 34 条：`水 税 睡 稅 誰 脽 㥨 㽷 䬽 …`，第 7 项起全是空心态，第 2 页整页、最多 4 页 —— 这就是"很多字是一个空心点"的全部来源，与字体文件损坏无关。

#### 修复（三处）

1. **`tools/mkdat/mkdat.cpp`**：`kHanyuPinlu` 的音节不在 `kMandarin` 全集时，作为**次读音追加**（不抢主音）。谁 → 主音 `she2i` + alt `shui2`；誰 → 主音 `shui2` + alt `she2i`。多音字条目 109 → 118。
2. **`framework/.../code_table.hpp|cpp`**：`load_dat()` 新增可选 `KeyOfReading` 回调；给出时把 alt 里每条 reading 经该回调转成 key 并建**别名索引**，同一字符可回答多个音节，且每次 lookup 只出现一次。`pinyin/host/dat_source_win.cpp` 传入 `&rospinyin::pinyin::syllable_of`。不传则行为与之前完全一致（向后兼容）。
3. **`pinyin/src/pinyin_session.cpp`**：`recompute()` 默认**不把无字频字放进候选**；仅当该音节过滤后一个不剩时才回退到全表（保证任何合法音节都还有得选）。开关 `set_show_rare(true)` 恢复旧行为。

#### 效果（真实引擎加载真实 `rospinyin.dat`，非脚本模拟）

```
shui   rare=false ->  7 条: 水 谁 税 睡 稅 誰 脽
shui   rare=true  -> 35 条: 水 谁 税 睡 稅 誰 脽 㥨□ 㽷□ 䬽□ …
shei   rare=false ->  2 条: 谁 誰
zhong  rare=false -> 26 条（原 108）
shi    rare=false -> 95 条（原 340）
```

简体「谁」现在排第 2（rank 648，仅次于「水」202），空心框 20 → 0。`shi` 仍剩 1 个：那是一个**有字频但码点在扩展 A** 的字，属字体子集覆盖不足，要彻底消掉得把 hanziDB 里那 30 个扩展区字并进字体子集（`font_subset.py` 加一段），不影响英文/拼音显示。

#### 回归

`ctest` 11/11、`check_m1.py` 53/53、`xingma/` 零改动。新增测试：
`code_table_test.cpp` 的 alt 索引用例（带/不带 deriver 两种行为 + 不重复出字）、`pinyin_session_test.cpp` 的生僻字过滤与"只剩生僻字时兜底"用例。

#### 顺带纠正 §11.7 的一处记录错误

上一轮 `TEXT` 上屏第 5 字是 `\u8AB0`（**繁体 誰**），不是简体「谁」。日志里那 4 次 `vk=0xBB`（`=`）翻页、6 次 `vk=0xBD`（`-`）回翻、最后 `vk=0x35`（`5`）选第 5 项，正是"翻遍候选找不到简体"的现场。11.7 已订正。

### 11.9 "改了没生效"的根治：让 .ime 自报数据指纹（2026-11-17 21:45）

**现象**：§11.8 修完之后用户复测，反馈"问题还在"。日志里 `feed ch=i act=1 comp=[shui] cand=34` —— 候选仍是 34 条，而修复后的表应当是 7 条。

**根因不在代码，在"这一轮根本没有用新构建"**：截图标题栏是 `imhost REF`，用户跑的是 `ref` 模式。而 `pull`/`install` 之外的所有模式都**从不更新 `.dat`**，`ref` 更是连 `.ime` 也不装。于是 system32 里仍是旧 `.ime` + 旧 `.dat`，候选 34 条完全自洽。旧 `.ime` 既没有 alt 索引（谁 的 shuí 读不到），也没有生僻字过滤（空心框照旧）。

这类"改了却没生效"已经反复三轮（第一次是 system32 里陈旧 `.ime`，第二次是选错布局，这次是跑错模式），共同点是：**宿主侧看到的只有"候选没变"，无法分辨是文件没换、换了但被缓存、还是换错了目录**。

**两条根治措施**：

1. **`.ime` 自报数据指纹**（`pinyin/host/dat_source_win.cpp`，仅 `ROSPINYIN_TRACE`）。读表成功后往 trace 写一行：

   ```
   dat OK path=C:\ReactOS\System32\rospinyin.dat size=902776 chars=44355 syls=419 shui=35 shui_simp=1 shui_trad=1
   ```

   判据是 `shui_simp`：只有**新表 + 新 .ime**（alt 索引真的建起来了）才会是 `1`；旧表这里是 `0`。读不到文件时写 `dat MISSING path=... err=2 -> 回退内置表`，解析失败写 `dat PARSE_FAIL`。这样任何一轮 trace 的前几行就能定论，不必再猜。

2. **`rec`/`ref`/`plain` 启动即对齐代码与数据**（`imhost.cpp` 的 `AlignRuntime()`）。在**窗口创建之前**（即 `.ime` 被 LoadLibrary 之前）：下载 `/rospinyin7.ime` 到 `C:\rospinyin7.new`，与 `system32\rospinyin7.ime` **逐字节比较**（只比大小会撞），不同则 `CopyFileW` 覆盖；再 `PullDataFile()` 更新 `.dat`。只换文件，不碰注册表、不改布局、不发任何键。覆盖失败（`err=32`）说明有进程正映射着该 `.ime`，日志明示"请重启 VM 再跑"。

   `PullDataFile()` 同时修掉一个陷阱：写 `system32` 失败退到 `C:\rospinyin.dat` 时，读回校验仍会打印 OK，但那对 IME 无效（它只读 system32）。现在回退分支会追加 `但写到了 C:\，IME 读 system32，本次未生效`，并返回 false。

**教训**：凡是"我改了但现象没变"，第一步不是改代码，而是先证明被测进程读的是哪份代码和哪份数据。宿主侧证明不了，就让被测方自己报。

**产物**：`rospinyin7.ime` 946,193 字节（trace）、`rospinyin.dat` 902,776 字节、`imhost.exe` 935,031 字节。回归 `ctest` 11/11、`check_m1` 53/53、`xingma/` 零改动；无 trace 的 `build-win` 同样编译通过（`#ifdef` 分支未破坏生产构建）。

### 11.10 `.ime` 换不掉的真因：默认输入法导致常驻映射（2026-11-17 21:56）

11.9 的对齐机制上线后复测，日志里出现：

```
启动前对齐代码与数据（只换文件，不发键、不改布局）
  .ime 覆盖失败 err=32 —— 有进程正映射着它
数据表校验 OK：magic=ROSPYDAT chars=44355 syls=418
```

`.dat` 换成了新的，`.ime` 换不掉 → 仍是"新表 + 旧代码"，现象当然不变。`.ime` trace 里没有 `dat OK` 那一行，正好证明跑的是旧代码（新代码才有这行）。

**为什么换不掉**：`err=32` 是 `ERROR_SHARING_VIOLATION`，即该 PE 已被某个进程作为 image 映射。trace 里除 imhost 外还出现过 `p=520`、`p=716` 两个进程执行 `active f=1`，说明有**非 imhost 的常驻进程**加载了它。结合 `GetKeyboardLayout(0)=E0260804`（imhost 一启动当前布局就是我们的 IME，而控制面板里"默认输入语言"显示 English(US)），实际状态是：我们的 IME 已被系统在会话里激活并常驻，任何激活过它的进程都会一直映射该文件，直到退出。

ReactOS 这版没有 psapi、也没有 toolhelp 的模块枚举（`EnumProcessModules` / `Module32First` 全树无实现），所以无法在宿主侧点名是哪个进程。

**两条处置**：

1. **热替换退让链**（`ReplaceFileBestEffort`）。直接 `CopyFile` 失败后依次试：把原名 `MoveFile` 成 `.old` 再写新文件（映射的是 section 对象，与文件名无关，通常能过）→ 备份到 `C:\rospinyin7.ime.bak` 后删除原名（delete-pending）再建同名文件。每步失败都先回滚，绝不留下"IME 文件没了"的状态。全部失败才提示关窗口/重启。
2. **dev 布局兜底**（`EnsureDevIme`）。布局按**文件名**去重，所以换名字等于换文件，与占用彻底无关：装一份 `rospinyindev.ime`（描述 `RosPinyinDev`），固定在同一个名字上反复覆盖，不会把布局列表越堆越长。`AlignRuntime()` 返回 false 时自动装并 `ActivateKeyboardLayout` 过去，日志明示本轮用的是 dev 布局。

另：`rec`/`ref`/`plain` 启动时若 `FindWindowW(kClass)` 已找到窗口，说明上一个 imhost 没关，它也在映射 `.ime`，日志会提示先关掉。

**长期口径**：验证期用 dev 布局即可，不必重启；要让 `rospinyin7.ime` 本身更新，需要先关掉所有加载过它的进程（或重启 VM），因为文件被 image 映射时无法原地覆盖。这一点应在 M2 验收单里写清，避免下次又误判成"改了没生效"。

**2026-11-19 补实测（用户真机操作，已固化为 §6.4.1）**：上面"关掉所有加载过它的进程"在 ReactOS 上**做不到点名**——
`EnumProcessModules` / `Module32First` 全树无实现，无法知道是哪个进程在映射，重启前也无从逐一关闭。
可操作的等价动作是**把默认输入语言临时换走**：控制面板把默认从「中文(简体) - RosPinyin」改成「英语(美国)」，
系统随即释放常驻映射，此时 `copy /y` 直接成功；再把默认切回，最后重启 guest 让已映射旧 image 的进程重建。
用户本轮就是这么绕过去的（先切默认、再覆盖）。**注意 `install-rospinyin.exe /step1` 不能替代这一步**：
它的 move-aside 依赖"运行中文件可改名"，ReactOS 上该前提不成立，失败后只会提示重启。


### 11.11 P0-2 收口：`open=-1` 是探针假象，`open=0` 是 ReactOS 的出厂值（2026-11-17 22:4x）

M2 清单里挂着的"`ImmGetOpenStatus` 读数异常"，其实是**两个不同来源的数字被当成了同一个**：

| 读数 | 出处 | 真实含义 |
|---|---|---|
| `open=-1` | `imhost` 状态栏/`tick` 行：`g_himc ? ImmGetOpenStatus(g_himc) : -1` | `g_himc == NULL`，即"这个窗口还没有关联输入上下文"。`-1` 是探针的哨兵值，恰好等于 `TRUE`，所以看起来像"IMM32 说已打开" |
| `open=0` | `.ime` 内 `processkey` 行 | 真实的 `pIC->fOpen`，ReactOS 建上下文时就是 `FALSE` |

ReactOS 侧的三条事实（0.4.16 源码）：

1. `win32ss/user/imm32/imm.c` 建上下文时 `pIC->fOpen = FALSE`，`fdwConversion = fdwSentence = 0`；
2. `ImmSetActiveContext`（`imm.c`）只**读** `pIC->fOpen` 去调 `NtUserNotifyIMEStatus`，从不回置；
   `ImmInstallIMEW` 也不碰它。所以布局切换多少次，它都还是 `FALSE`；
3. 按键路由**不看**这个值：`ImmProcessKey`（`keymsg.c`）无条件调 `pImeDpi->ImeProcessKey`，
   只有 `VK_PACKET` + 非 Unicode IME 的分支才看 `pIC->fOpen`。这就是"IME 明明在工作，`open` 却是 0"。

**结论**：`open=0` 不是 bug 而是 ReactOS 的默认状态；把它当门禁会直接废掉输入法。真正的危害有三条：
语言栏的 中/英 状态永远显示为关（`UserSetImeConversionKeyState(..., IME_CMODE_ALPHANUMERIC)`）、
下一次 `Ctrl+Space` 走 `Imm32CImeNonImeToggle` 的"关闭"分支（本该切 EN 布局）、
以及 M3 的 Shift 临时英文没有确定的基线。

**修法**：`ImeSetActiveContext(fFlag=TRUE)` 里把 OS 侧状态对齐一次（`AlignOpenStatus()`，
`framework/platform/win32/imm32/imm_entry.cpp`）：读 `before`，为 0 则 `ImmSetOpenStatus(TRUE)` 再读 `after`，
trace 打 `openalign before=0 after=1`。键路由仍不依赖它（`check_m1.py` 已加门禁：
`should_consume_key` 体内不得出现 `OpenStatus`）。

**最小复现序列**（`imhost.exe` 窗口里即可）：

```
1. 启动 imhost（任何模式），窗口获得焦点
2. 状态栏 ImmProp=0x00090000、IME文件=rospinyin.ime 时，trace 里 processkey 行的 open= 为 1
3. 若某轮仍是 0：说明该 HIMC 没走过 ImeSetActiveContext（例如从 imhost 自带 IME 窗口直接取到的上下文）
```

### 11.12 P0-3 收口：抖动来自 ReactOS 的 EDIT 光标同步，不是引擎在动窗口（2026-11-17 22:5x）

原先的猜测"引擎每次 feed 都调 `ImmSetCompositionWindow`"是错的——全仓库没有一处
`ImmSetCompositionWindow` 调用。真正的发送者在应用侧：

```c
/* win32ss/user/user32/controls/edit.c, 仅 __REACTOS__ 分支 */
static void EDIT_SetCaretPos(EDITSTATE *es, INT pos, BOOL after_wrap) {
    ...
    SetCaretPos(pt.x, pt.y);
    if (ImmIsIME(hKL))
        EDIT_ImmSetCompositionWindow(es, pt);   /* -> ImmSetCompositionWindow -> IMN_SETCOMPOSITIONWINDOW */
}
```

`EDIT_SetCaretPos` 在每次插入、`EDIT_EM_ScrollCaret`、`EDIT_AdjustFormatRect` 后都会跑，
而且**不检查当前是否在组合中**，只看 `ImmIsIME(hKL)`。这解释了"每键 1–3 次通知"：一次按键
依次触发"插入字符→滚动→格式化矩形"三条光标同步。通知数**不能从 IME 侧降低**，
它只影响 `pIC->cfCompForm`，而我们从不读 `ImmGetCompositionWindow`。

用户看到的跳动是另一回事，出在我们自己的窗口上，两条真 bug：

1. `CompositionWindow::update()` 量了文本、写了 `width_/height_`，却**从未** `SetWindowPos`：
   `zhong` 建窗 61 px 宽，`zhongguo` 仍按 61 px 画，后半截被裁掉；
2. `update()` 每次都 `InvalidateRect + ShowWindow`，`place_windows()` 每次都 `SetWindowPos`，
   于是每来一条 `IMN_SETCOMPOSITIONWINDOW` 就整窗重绘一次。

**修法**：几何只有一个 owner——`ime_ui.cpp:place_windows()` 调 `UiWindow::apply_geometry(x,y,w,h)`，
它把位置和尺寸一起下发，且**只有真正变化时才发消息**；`show()` 只在隐藏时调 `ShowWindow`；
两个窗口的重绘都以"文本/候选页内容是否变化"为前提。trace 的 `feed` 行新增绝对计数
`wmove=`（真正改变几何的次数）与 `wrep=`（重绘请求次数），一次键正常应为每窗 0–1。

### 11.13 P1 收口：安装器要两份文件、候选按字形过滤、生僻字开关说明

**P1-1 安装器只拷 `.ime`（已修）**。M2 的 IME 还依赖 `system32\rospinyin.dat`，
旧 `install.cpp` 只 `copy` 了 `.ime`，装完就是"候选窗空白"——而这条故障和"数据没生效"
长得一模一样。现在 `/step1` 拷两份并**逐字节比大小**（`kVerifyFailed=4`），
`/status` 分别报告，`/u` 两份都删（含 `.old`）。另外文件被 image 映射时
`CopyFile` 会 `ERROR_SHARING_VIOLATION(32)`：现在先 `MoveFileEx(name -> name.old)`
腾名字再写，并明确提示"运行中的 IME 仍用旧文件，重启后生效"——与 imhost 的
`ReplaceFileBestEffort()` 是同一套退让链，只是搬进了安装器。

**P1-2 扩展 A 区方框（已修）**。随包字体子集是 `U+4E00–U+9FA5`（实测 cmap 21,287 字，
扩展 A 区 0 个），而 `shi` 的候选里有一个**有字频**的扩展 A 字，于是显示成豆腐块、
选中后把方框写进文档。修法不是扩字体（11 MB → 更大），而是**运行时按字形过滤**：
`UiCanRender()` 用候选窗同一个 HFONT 调 `GetGlyphIndicesW(..., GGI_MARK_NONEXISTING_GLYPHS)`
（结果按码点缓存，`shi` 95 个候选只探一次），`imm_entry.cpp:renderable_candidates()`
在进分页器**之前**剔除——必须在分页器之前，否则数字键会与屏幕上的行号错位。
两条护栏：探针不可用一律**放行**（`GDI_ERROR` 视为可画）；若超过一半候选被剔
（说明当前字体根本没有 CJK），整体放行并 trace `cand-filter ... keeping all`。

**P1-3 生僻字开关**。`PinyinSession::set_show_rare(true)` 让无字频字也进候选；
M2 没有 UI 入口，需要时改 `pinyin/src/pinyin_session.cpp` 的默认值或由 Scheme 传入。
默认关闭是有意的：打开后 `shi` 这类音节会从 95 条涨到数百条，且多数在子集外
（会被 P1-2 的过滤器挡掉，等于白翻页）。M3 再给语言栏菜单项。

### 11.14 Shift 切中英：`open` 从"观测值"变成"IME 自己的状态"（2026-11-18 12:2x）

真机反馈："Shift 无法切换中英文"。trace 里其实**有** `processkey vk=0x10 ... -> 0` ——
键到得了我们手上，是我们什么都不做：imm32 只把 `fOpen` 存着，激活路径不回置，
也没给 VK_SHIFT 注册热键；IME 自己不认领，Shift 就落给应用当普通修饰键。

修法（`imm_entry.cpp`）：
- `ImeProcessKey` 先处理 `VK_SHIFT`：按下置 `shift_latched`，**其它任何键清 latch**，
  抬起时若仍是 lone Shift 才调 `set_ime_open()`。这样 `Shift+A` 仍是大写 A，
  不会变成"切到英文再打 a"。
- `set_ime_open()` 是唯一开关出口：`ImmSetOpenStatus` + 若正在组合则
  `finish_composition()`（不取消会留下一个永远不更新的组合窗）。
- `should_consume_key()` 第一行 `if (!ime_is_open(hIMC)) return false;` ——
  关闭态下所有键归应用，这才是"英文输入"。
- **P0-2 的结论要反过来读**：当时说"`open` 只能观测、不能当门禁"，前提是**没人设置它**；
  现在 `AlignOpenStatus()` 每次会话对齐一次、Shift 可切换，它就是可信状态。
  `AlignOpenStatus()` 也从"每次激活"改成"每会话一次"——否则切到英文后点一下别的窗口
  再回来就被悄悄改回中文。
- 契约闸门同步改写：`open` 必须"激活时对齐一次 + 只在 `set_ime_open` 里写 +
  只在 `ime_is_open()` 里读且门禁走它"（check_m1 57 项）。

另外加 `uigeom` trace（`refresh_ui`，仅几何变化时打印）：
`uigeom comp=x,y,w,h cand=x,y,w,h anchor=x,y caret=0/1`。
"翻页时输入框位移"这种报告光靠 `wmove/wrep` 计数答不了——计数只说"有 SetWindowPos"，
不说**哪个窗、从哪到哪**；`caret=0` 还能量化"锚点退化成鼠标位置"这条路径
（`caret_anchor()` 里 `GetFocus()`/`GetCaretPos()` 失败时的兜底）。

**顺带修掉位移的两个来源**（`ime_ui.cpp`）：①锚点每次 `UiShow` 都重取 caret，
而每上屏一个字 caret 就右移一格——弹窗于是跟着爬；改成**一次组合只取一次锚点**，
`UiHide` 才清。②`x = anchor.x + 12` 原来按**实测宽度**做右边界收口
（`screen_w - composition.width() - 8`），于是每多打一个字母 x 就左跳一次；
改成按固定预留（`kPopupRightMargin=300`）收口，公式里**不含任何窗口宽度**——
打字和翻页都不会再改 x。贴底时（`anchor.y + 260 > screen_h`）整组翻到光标上方，
这个判断也在取锚点时定一次。

### 11.15 imhost 汉字"粘在一起"：是步进，不是字号（2026-11-18 12:3x）

用户报"中文字间距过窄，所有字都粘一起"，并建议加字间距或缩小字体。放大上一轮截图
（`shot-1213.png` 输入框区域）后可以排除"只是挨得紧"：每个字的**实际步进只有约半个
字宽**，后一个字直接压在前一个字上，是重叠而不是紧排。

判定与修法：

1. **EDIT 没有字间距 API**。`SetTextCharacterExtra` 只作用于当前 DC，而 EDIT 自己
   取 DC（`GetDC` 后每次重绘都会重置 DC 状态），应用侧够不着；RichEdit 的
   `CHARFORMAT2.sSpacing` 在 ReactOS 的 riched20 上未经验证，换控件又会牵动
   `WM_IME_*` 投递，不值得为排版冒这个险。
2. **真因是字体的中文步进缺失**：`CreateFontW(-19, …, DEFAULT_PITCH|FF_SWISS, nullptr)`
   拿到的字体没有中文字形，字形由替换字体提供、步进却仍按原字体的拉丁宽度算，
   于是 19px 的字挤在 ~10px 的步进里。
3. **修法不猜字体名，直接量**（`vmshare/imhost.cpp` → `MakeCjkFont()`）：
   `adv = GetTextExtentPoint32W(L"中中") / 2` 与 `TEXTMETRICW.tmHeight`，
   判据 `adv >= height*0.85`（CJK 的 em 步进 ≈ height；"拉丁字体 + 替换字形"实测 ≈0.5）。
   顺序：已知字体名（Droid Sans Fallback / Noto Sans SC / SimSun / NSimSun /
   Microsoft YaHei / MingLiU / PMingLiU / MS Gothic / Arial Unicode MS）→
   显式 `GB2312_CHARSET` + 空脸名 → `EnumFontFamiliesExW` 枚举全部family 逐个量。
   一条都不达标时退回 `DEFAULT_GUI_FONT` 并打 `font FAILED …`，**不让"没找到字体"
   又表现成"改了没生效"**。
4. 字号同步下调：输入框 19 → **17px**（`kEditFontH`），说明/状态 label 12 →
   **14px**（`kLabelFontH`，label 高度 22）。label 复用输入框已经量过的 face
   （`g_editFace`），不再跑一遍全枚举。
5. 每轮启动的日志会写清结论，一行一个候选：
   `font ok   face="X" h=17 adv=17 tmh=20 glyph=1 probe=1` /
   `font miss face="Tahoma" h=17 adv=9 tmh=22 glyph=0 probe=1` /
   `font enum families=N`。`probe=0` 表示 ReactOS 的 `GetGlyphIndicesW` 不可用
   （此时按"有字形"放行，只靠 adv 判）。

排查口径：如果日志里出现 `font FAILED`，说明 guest 里确实没有中文步进正常的字体，
下一步是给镜像装字体（或在 imhost 里用 `AddFontMemResourceEx` 塞一份子集），
而不是继续调字号。

6. 兜底开关（省一轮来回）：`imhost.exe font:SimSun size:15` —— `font:` 指定 face
   （先严格匹配名字，再放宽到"步进达标"），`size:` 只影响输入框字号（10–40）。
   日志末尾会多一行 `font edit h= adv= tmh= ok=`，是最终真正生效的那组值。

**真机结论（2026-11-18 12:57，用户回复"显示已正常"）**：

```
font ok   face="Droid Sans Fallback" h=17 adv=17 tmh=23 glyph=1 probe=1
font edit h=17 adv=17 tmh=23 ok=1
font ok   face="Droid Sans Fallback" h=14 adv=14 tmh=19 glyph=1 probe=1   # label
```

选中的是镜像里自带的 **Droid Sans Fallback**（不是我们点名的那几个 Windows 字体，
说明走的是"枚举全部 family 逐个量"那条路）。`adv == h` 正是 CJK 的 1 em 步进，
此前拉丁字体的 `adv≈0.5h` 与之相差一倍——这也就是"字压字"与"字距正常"的分界。
没有出现 `font FAILED`，所以不需要给镜像装字体，也不必再动字号。

排查口诀：**汉字挤在一起，先看 `adv/h` 的比值，别看字号**。`adv ≈ h` 是正常，
`adv ≈ 0.5h` 是拿到了没有中文字形的字体（字形靠替换字体画、步进按拉丁算）。

### 11.16 P0-1 真机验收收口：15 项全部有实测（2026-11-18 12:5x）

最后一轮日志（`vmshare/imhost.log`，20:57 那次运行）用户一口气把验收单打完了：
入口层与窗口层各 **433 次按下**逐条对齐，程序注入 **0 次**（红线守住）。
把 `KEY … DOWN` 与 `WM_IME_CHAR` 按时间序折叠，能直接读出他打了什么：

```
SHI 4{试} … KAN [SPACE]{看} ZITI 4{字} 1{体} XUANRAN ? 2{渲} 2{染} XI?AN 1{西} 1{安}
O????? 1{衷} ?????????????? 4{郭} SHI???????????????????? 7{仕}
HAO [BACK][BACK][BACK] HAO [ESC] NV [SPACE]{女} LV 1{律} LVE [BACK] E 1{略} NVE [SPACE]{虐}
RENZHI [BACK] ICHU 1{人} 1{之} 4{初} XINGBENSHAN 2{性} 1{本} 2{善} [RETURN]
…（以下为《陋室铭》《山居秋暝》等长句，连续上屏 100+ 字）
[SHIFT] H [BACK] [SHIFT] H [BACK] [SHIFT] I I U H [BACK] [SHIFT] B [BACK]
```

`?` 是 `=`/`-`（翻页键的 vk 记成 `0xBB`，符号名解析不出来），
每一串 `?` 后面都跟着跨页选字成功（`1{衷}`、`7{仕}`、`4{郭}`），说明翻页路径可用。

逐项对应 `M2-ACCEPTANCE.md` §3：

| 项 | 实测依据 | 结果 |
|---|---|---|
| 3.1 / 3.2 / 3.3 | 前一轮 `zhong`/`zhongguo` 截图 + `WM_IME_CHAR 0x4E2D→0x56FD` | ✅ |
| 3.4 `nv` | `NV [SPACE]{女}` | ✅ |
| 3.5 `lv` | `LV 1{律}` | ✅ |
| 3.6 `lve` | `LVE [BACK] E 1{略}`（`v` 打错回退再补 `e`，回退也可用） | ✅ |
| 3.7 `nve` | `NVE [SPACE]{虐}` | ✅ |
| 3.8 数字键按当前页 | `shi` 页内 `2{仕}`、`xin` 页内 `1{体}` 等多处 | ✅ |
| 3.9–3.11 翻页/跨页/回翻 | `?` 串 + 跨页选字（`7{仕}`、`4{郭}`） | ✅ |
| 3.12 Backspace | `HAO [BACK][BACK][BACK] HAO`、`RENZHI [BACK] ICHU 1{人}` | ✅ |
| 3.13 Esc 不上屏 | `HAO [ESC]` 后紧接 `NV`，中间无 `WM_IME_CHAR` | ✅ |
| 3.14 100 字压测 | 《陋室铭》+《山居秋暝》连续上屏 100+ 字，无崩溃、无残窗 | ✅ |
| 3.15 中英往返 | 末尾 `[SHIFT] H … [SHIFT] B`：Shift 后 `H`/`I`/`U`/`B` 直接以拉丁字符上屏（无组合串） | ✅ |

**门禁（同一份源码）**：`ctest` 11/11、`check_m1` 57/57、`xingma/` 零改动。
**产物**（VM 内回验字节一致）：

| 文件 | 字节 | sha256 |
|---|---|---|
| `rospinyin7.ime` | 967,775 | `15e401cf…b2597` |
| `rospinyin.dat` | 902,776 | `f28b2606…b921` |
| `imhost.exe`（探针，不随包发布） | 945,314 | `4c1d4ca0…e8ed` |

`imhost.exe` 导入表只有 8 个系统 DLL（`ADVAPI32/GDI32/IMM32/KERNEL32/msvcrt/SHELL32/USER32/WS2_32`），
无 `libgcc*/libstdc++*` —— 静态链接这条判据仍然成立。

至此 M2 的 P0/P1 全部关闭，只剩 **P2-1（CI 产出 release 资产）** 属于发版工程化，
不影响 M2 判定。

### 11.17 alpha0.0.2 发版记录（2026-11-18 14:5x）

发版口径与 `alpha0.0.1` 不同，记清楚免得下次踩：

1. **发布物用 `ROSPINYIN_TRACE=OFF`**（`ime/CMakeLists.txt` 的 option，默认关）。
   真机验收用的是 `TRACE=ON` 的构建（`build-win-trace/ime/rospinyin.ime`，967,775 B），
   它多出来的 27 KB 全是写 `C:\rospinyin-trace.log` 的诊断代码。两者导出表（16 个 IMM32 接口）、
   导入 DLL（GDI32/IMM32/KERNEL32/msvcrt/USER32）、功能路径都一致，因此可以发 `TRACE=OFF`：
   长期使用不该带日志 I/O。**发布物与验收物不是同一个文件，这一点必须在 release notes 里写明。**
2. **构建不完全可复现，但差异只有 4 个字节**。同选项连建两次：

   | 产物 | 字节 | sha256 |
   |---|---|---|
   | 第一次 | 940,601 | `c9dc17c0…3e25feaa` |
   | 第二次 | 940,601 | `ae7d1e57…08b1cda` |

   逐字节比对只有 3 个区间、共 4 字节不同：`0x88`（PE `TimeDateStamp`）、`0xD8–0xD9`（`CheckSum`）、
   以及 `0x60A04` 处一个调试相关字段。**代码段与数据段逐字节相同**。
   所以"发布的二进制 = 该 tag 的源码构建"仍然成立，只是不能用 sha256 做复现校验；
   真要比特级复现，链接时加 `-Wl,--no-insert-timestamp` 并关掉 checksum。
3. 资产（已上传到 release `alpha0.0.2`）：

   | 文件 | 字节 | sha256 |
   |---|---|---|
   | `rospinyin.ime` | 940,601 | `c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa` |
   | `rospinyin.dat` | 902,776 | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
   | `SHA256SUMS.txt` | 160 | — |

4. `.dat` 没变（`f28b2606…`），与验收、与 `alpha0.0.1` 都是同一份——数据层这一版没有改动。
5. 发版脚本路径同样受限：`git push` 与 `git push --tags` 都走不通（见 §12），
   tag 与 release 全部用 API 建（`POST /git/tags` + `POST /git/refs` + `POST /releases`，
   资产走 `uploads.github.com`，这个域名是通的）。整理成 `tools/make_release.py`；
   **注意**：`alpha0.0.2` 实际是一次性脚本跑的，`tools/make_release.py` 是按它整理的通用版，
   尚未实战——下次发版先核对资产过滤器（它会跳过 `.md`/`.py`）与 tagger 日期。
6. 发布说明正文同时入库：`docs/releases/<tag>.md`，与 GitHub release body 完全一致。
7. 回验：从 `https://gh-proxy.com/https://github.com/.../releases/download/alpha0.0.2/*` 下载，
   三个文件字节数与 sha256 与本地一致；GitHub 自己算的 `digest` 字段也与之相同
   （`sha256:c9dc17c0…` / `f28b2606…`），这是"发出去的就是验收的那份代码"的最强证据。

### 11.18 重大前提变更：0xF4 的归因被推翻，红线 #2 降级（2026-11-18 19:56）

**这是立项以来第一次推翻「立项依据级」的前提，接手时必读。** 项目的整个「分步注册 + 不碰 Preload」
部署策略都建立在 M0 那次 `0xF4` 的解释之上；该解释现在已知是错的。

**旧说法**：mzimeja「一次全量注册 + `Preload` 预载」⇒ ReactOS 登录路径把关不兼容的 `.ime` 载进
csrss ⇒ 确定性 `0xF4` ⇒ 因此红线「绝不写 Preload / 绝不注册为默认 IME」。

**新事实**（三条独立证据）：

1. **写 `Preload` 的不是安装器，是平台**：ReactOS 0.4.16 `win32ss/user/imm32/install.c`
   `Imm32WriteImeLayout()`（L256–341）在 `ImmInstallIMEW` 内部自己
   `RegCreateKeyW(HKCU, "Keyboard Layout\Preload")` + `RegSetValueExW`，**没有 opt-out**。
   也就是说「不写 Preload」这件事从来不在我们手里——而 `Preload` 里一直有我们。
2. **加载者是 `userinit.exe`，不是 csrss**：`userinit!wWinMain → SetUserSettings →
   UpdatePerUserSystemParameters → IntLoadPreloadKeyboardLayouts → LoadKeyboardLayoutW`
   （`base/system/userinit/userinit.c` L452/670；`user32/windows/input.c` L145）。
   且 `0xF4` 要求进程调用过 `RtlSetProcessIsCritical`，`userinit` 没有，所以
   「userinit 死 → 0xF4」这条因果还缺一环（记作待解，见 `M0-REDLINE-REVIEW.md` §9.3）。
3. **两组对照实验（2026-11-18，覆盖层，各 3 次启动，均用发版二进制 940,601）**：

   | 组 | 变量 | 结果 |
   |---|---|---|
   | N | 在 `Preload` 里但**非首项**（仅加载） | **3/3 活** |
   | N′ | **首项 = 默认输入语言**（`KLF_ACTIVATE` → `ImeSelect` → 建 UI server 窗口 + 工具条） | **3/3 活** |

   N′ 三次托盘都是红底`中`、N 组是`EN`——变量确实生效，不是空转。

**结论**：致伤机制在「**登录路径上执行可能失败的初始化，并把失败吞掉**」，不在「被 Preload 载入」。
红线 #2 的落地形态（一条字符串禁令）**既不必要、也不充分、还不可执行**，降级为**部署偏好**；
A1（`DllMain` 零工作）与 A1b（`ImeInquire` 失败必须 `return FALSE`）**保留但换论据**——
不再防蓝屏，而是「登录路径上不留静默失败」，对 liberNT / x64 / 别的 imm32 实现仍是必要的健壮性措施。

**已知的不对称**（别过度引用本结论）：N′ 证伪的是「**我们的**二进制在登录路径被激活会崩」，
**不是**「mzimeja 的 `DllMain` 就是 M0 的凶手」。后者仍只是推断（时间上最先、行为上不同、
失败被吞已被 `objdump` 证实），没有做过直接指向它的分离实验。

**证据与全文**：`M0-REDLINE-REVIEW.md` §13（13.1 结果 / 13.2 判据兑现 / 13.3 换对象 /
13.4 仍未知 / 13.5 对移植）；操作与截图 `m0/vm/PRELOAD-PROBE.md` §2、§6，
截图在 `m0/vm/evidence/preload-probe/N/`。

**覆盖面层**：两组实验全程在 qcow2 覆盖层上做，基础盘 `reactos.qcow2` 零改动；
`/u` 与「整改」= 丢弃覆盖层。

### 11.19 门禁从「黑名单扫词」改成「调用白名单」（2026-11-18 21:0x）

**动机**：`check_m1.py` 里那条 `installer never writes Preload` 一直在扫源码里的
`"Preload"` 这个词。前提变更之后它保护的是**平台自己会做的事**（`ImmInstallIME` 内部
`Imm32WriteImeLayout` 写 Preload），而我们真正想守的只有「别让自己成为默认输入语言」。
更糟的是黑名单天生可绕过：实测把写入写成 `L"Keyboard Layout\\Pre" L"load"` 之后，
旧断言里 `"Preload" in code` 命中 **0 次**，闸门全绿——拼接字符串就能穿过去。

**改法**（`tools/check_m1.py` §6、§7，1 条换 5 条，59 → 63）：

| 断言 | 形状 | 守的是什么 |
|---|---|---|
| `DLL_PROCESS_ATTACH` 只调类注册白名单 | 调用名集合 ⊆ `{DisableThreadLibraryCalls, RegisterUIServerClass}` | A1：加载期不做别的事 |
| `RegisterUIServerClass` 只调 `RegisterClassExW` | 调用名集合 ⊆ 单元素 | A1 的纵深：被调函数自己也得干净 |
| `ImeInquire` 可拒绝且不碰加载/注册 API | 函数体含 `return FALSE` + 危险调用集合为空 | A1b：拒绝优于半初始化 |
| 安装器注册表写入限于两键白名单 | 每个 `Reg*Key*` 的 (hive, subkey) ∈ 白名单 | 任何新写入，不论叫什么名字，都会失败 |
| 布局子键只能由 `kKeyboardLayouts` 拼出 | 所有带反斜杠格式串的 `swprintf` 第 4 参必须等于它 | 手工拼路径也到不了 Preload |

`#if defined(ROSPINYIN_TRACE)` 里的代码会在扫描前整段剥掉（`strip_trace_blocks`），
因为那部分在发布构建里根本不存在，拿它判红是假的。

**反向验证 6 次**（每条都故意改坏，确认 FAIL，改完还原）：

```
attach 加 LoadLibraryA                 → FAIL  unexpected: ['LoadLibraryA']
类注册加 CreateWindowExW               → FAIL  unexpected: ['CreateWindowExW']
ImeInquire 的 return FALSE 改成 TRUE   → FAIL  no rejection path
拼接字符串写 Preload（旧断言 0 命中）  → FAIL  [('HKEY_CURRENT_USER', 'L"Keyboard Layout\\Pre" L"load"')]
加 SystemParametersInfoW               → FAIL  SystemParametersInfo
uninstall 里第二处 swprintf 换前缀     → FAIL  prefixes: ['L"Keyboard Layout"', 'kKeyboardLayouts']
```

最后一条是过程中自己抓到的漏洞：第一版只检查「存在一处 swprintf 用了
`kKeyboardLayouts`」，于是把 **uninstall 里第二处**换掉仍然 PASS——`sub` 在两个函数里
各建一次。改成「每一处都必须等于它」才闭合。

**教训（通用）**：断言要用**白名单 + 调用点**，不要用关键词黑名单。黑名单只能证明
「某个词没出现」，白名单能证明「只有这些事可能发生」。后者才是行为契约。

### 11.20 P2-1 落地：tag 即发版，并由门禁盯住版本（2026-11-18 21:1x）

**做之前先发现的真问题**：`ime/rospinyin_res.rc` 里 `FILEVERSION` 还是 `0, 0, 1, 0`，
`FileVersion` / `ProductVersion` 两个字符串还是 `"alpha0.0.1"`——而 `alpha0.0.2` 已经发布了。
也就是说，用户在文件属性里看到的版本是错的，而"发布的二进制 = 该 tag 的构建"这句话
当时**无法被检验**。根因不是谁忘了改，而是**没有任何东西比较这两者**。

**改了三处：**

1. **版本单一真相源** `ime/version.h`：`MAJOR/MINOR/PATCH/BUILD` + `ROSPINYIN_VERSION_STRING`
   + 给 rc 用的 `ROSPINYIN_VERSION_COMMA`。`rospinyin_res.rc` 改为 `#include "version.h"`，
   `FILEVERSION` 与两个字符串都从这里展开。只有一个地方要改，就没有"改了两处漏一处"。
2. **版本门禁** `tools/check_release_version.py <tag> [--binary X.ime]`：校验
   tag ↔ `version.h` ↔ **产物里的 `FILEVERSION`**（自己解析 PE 的 `.rsrc` /
   `VS_VERSIONINFO` / `VS_FIXEDFILEINFO`，无第三方依赖）。`docs/releases/<tag>.md` 必须存在。
   反例实测：拿 U1/U3 版（rc 仍 0.0.1）跑 → `artifact FILEVERSION ... binary=0.0.1.0` 判红。
3. **CI release job**：`on.push.tags: alpha*/beta*` 触发；`needs: [engine-smoke, windows-build]`，
   所以门禁红了就发不出去。产物装配后 `gh release create/edit` + `upload --clobber`
   （可重跑），最后**把资产下载回来 `sha256sum -c`** —— 这一步原来是手工做的（alpha0.0.2
   下载回验三件），现在成了流程的一部分。

**顺带验证的一件事**：本地按 CI 的装配步骤跑一遍，`.dat` 的 sha256 是
`f28b2606338431d2…`，与 `alpha0.0.2` 已发布的资产**完全一致**。说明 CI 从固定源编译的
数据表确实能重现发布物，这条以前只是相信。

**遗留**：`alpha0.0.2` 的资产仍是手工上传的那份（版本资源 0.0.1）；不打算重发，
下一次发版会自带 0.0.x 的正确版本号。若要补，`gh release upload --clobber` 即可，
但 tag 不变而资产变了，反而破坏"资产 = tag 构建"的可追溯性，所以不做。

### 11.21 文档归位：根目录只留 README（2026-11-18 21:2x）

**问题不是文件多，是"没有规则说明新文件该放哪"**。根目录堆了 16 个 `.md`，
每写一份报告就多一个同级文件；即使这次手工整理干净，下一份报告照样会落在
`README.md` 旁边，而且没有人会发现。

**结构**（`docs/README.md` 是唯一索引，已登记全量文件）：

```
docs/
├── README.md      文档地图：先读什么 + 全量索引 + 归置规则
├── plan/          PLAN / ROADMAP / PLAN-MODULARIZATION      活文档，每轮必读
├── handover/      HANDOVER / CHANGELOG                      活文档，交接与登记
├── milestones/    M0-REPORT / M0-REDLINE-REVIEW / M1-REPORT + m2/  只读，回溯用
├── font/          5 份字体与字形专题（FONT-PLAN-REVIEW 是裁决）
├── reviews/       框架路线、上游 64/ARM64 可移植性
└── releases/      <tag>.md，被 CI 与版本门禁直接读取
```

**留在根目录的四个东西，理由各不相同**：`README.md`（门面）、`CMakeLists.txt`
（构建入口）、`LICENSE` / `NOTICE.txt`（法律声明——**25 个源码文件头部写着
"See LICENSE and NOTICE.txt at the repository root"**，搬走就要改 25 处注释，
而法律指针指向错位置比多两个文件糟糕得多）。

**引用怎么改的**：14 条 markdown 链接按"新位置"重算相对路径（链接方与目标方都
可能移动），59 处行内代码里的根相对路径加 `docs/` 前缀。**裸文件名不改**——
正文里写"见 `HANDOVER.md §11.20`"是在说名字，不是路径，索引负责回答"它现在在哪"。

**新门禁 `tools/check_docs.py`（7 项，已进 CI）**：根目录无多余 md、`docs/` 下
每个文件都在索引里、所有相对链接可解析、链接不被闭合两次、`docs/releases/` 命名
符合 tag、子系统文档仍贴着代码。**推演**：没有这道门，这次整理只是一次性劳动。

**过程中踩的坑（值得记）**：迁移脚本的正则 `\]\(([^)\s]+)` 里那个 `)` 是**分组的
右括号**，不是字面量——于是 14 条链接全变成 `](x.md))`。`check_docs` 的链接检查
**抓不到**它（目标照样能解析，多出的括号只是文本），是我肉眼在 `README.md` 第 5 行
看出来的。已补一条"链接不得闭合两次"的断言把这个 bug 类型钉住。
**教训**：链接检查器只能验"指向对不对"，验不了"括号配没配平"；改写正文的正则，
要先在一条已知样本上验证 group 边界。

**顺手修的第二个坑**（这次改动 21 个文件改名才暴露）：`tools/push_via_api.py` 用
`git diff --name-status` 决定要往远端 tree 里写哪些路径，而它**默认开着 rename 检测**——
一次改名被压成一行 `R100 old new`，脚本只取 `row[-1]`（新路径），旧路径永远不会从
`base_tree` 里被删掉。结果是远端 tree 里旧文件一个没少，一致性安全阀当场拦下
（`远端 bdce2bb vs 本地 af2b712`），ref 未动。修法是加 `--no-renames`，让改名退化成
一删一增。**安全阀的价值正在于此**：它不判断"对不对"，只判断"远端重建出来的是不是你
本地那棵树"，于是脚本的缺陷变成了当场失败而不是静默的半吊子推送。

### 11.22 架构审计的复核与 X 计划扩容（2026-11-18 21:4x）

外部送来一份架构审计（`code-audit/ros-pinyin-架构优化分析.md`，放在仓库之外）。
它给了 13 条问题结论、一套量化基线、一个四步落地顺序。**我们没有直接照抄进计划**，
而是逐条回代码取证，结论写在 [`docs/reviews/ARCH-AUDIT-VERIFICATION.md`](../reviews/ARCH-AUDIT-VERIFICATION.md)。

**复核结论**：11 条成立、1 条证据过期（P0-3 的"三段逐字重复"）、1 条已过时（P2 文档叠加）。
12 项数字 10 项精确命中，差的两个是 `kBuiltin` 108→**109** 和 `ime/` 口径（272 是整目录含 `.ico.b64`，
`ime/*.cpp` 只有 **19** 行）。**这份审计的质量是可信的**——它读出了 `scheme_instance()` 是链接期硬接线，
这一点连我们自己写计划时都没意识到（见下）。

**复核才看得见的三条（审计没提）**

1. **两条激活路径的调用顺序相反**：`ImeSelect` 是 `on_select → AlignOpenStatus → ToolbarShow`，
   `ImeSetActiveContext` 是 `AlignOpenStatus → on_select → ToolbarShow`。审计把它们当"重复"，
   实际是**不一致**——而今天 U1 那个"首次激活 IME 是关的"bug 正是这类分散路径的产物。
   抽 `activate_session()` 时顺手统一，成本近乎为零。
2. **计划自己的基线数字也失真了**：`PLAN-MODULARIZATION §2` 写 `ime/*.cpp = 57`，实测 19；
   `framework/platform/win32/imm32+ui` 记 2584，实测 2697（+113，一天之内）。审计批评"快照两天就失真"，
   而它没注意到被批评的对象是同一份计划。已把 §2 改成"每项都附生成命令，别引用数字"。
3. **`ime/version.h` 不是"版本收敛"，是第四处并行真值**：审计把它算作已修复项，
   实际现在有 4 处（`CMakeLists 0.0.1` / `pinyin 0.0.2-m2-dev` / `imfw 0.1.0-imfw` / `ime/version.h alpha0.0.2`）。
   最危险的是 `CMakeLists.txt:3` 的 `project(VERSION 0.0.1)`——一旦有人拿 `${PROJECT_VERSION}` 命名产物，
   文件名就带错版本。已进 X2-e5。

**纳入 X1–X4 时唯一发现的顺序问题**

审计建议"注入缝 + toy scheme"排顺序 0、"Win32 独立成库 + presets"排顺序 2。前一条我们同意但**理由要改**：
不是笼统的"解锁一切"，而是 **X2 要的假 scheme 根本塞不进去**——`session_store.cpp:34` 直接调全局工厂，
`ime/CMakeLists.txt:7` 自己注释着"第二个方案靠换一个 `scheme_reg.cpp` 再编译"。这条不先做，
X2 的 3-5 天工期是假的。

后一条（构建收敛）我们**同意它排在冒烟之后，但原计划想放在 X1 之后是错的**，理由值得单独记：

> **X1 的验收判据是字节比对**（`.ime` 逐字节，只允许 4 处 PE 差异）。
> 一旦拆库目标、改链接顺序，字节**必然**变——判据当场失效，你无法区分"拆库导致的"和"拆错了导致的"。
> **判据失效的地方不能放改动。**

所以 X2 扩成 6-8 天：注入缝 → 剥 `handle_key` → 冒烟 → `route_key` → 拆 TU → 构建收敛（库目标/presets/
版本收敛/平台补丁归位）。X3 相应降级为"接入演练 + per-instance 隔离"。

**没纳入的两条**（理由写在计划 §1 非目标）：`.dat` 全量加载（性能，归 M4/M5）、
`kBuiltin` 109 条双真值（M4 扩码表必然重做数据管线，现在改是重复劳动）。

### 11.23 X1 目录归位：19 个文件改名，产物字节不变（2026-11-18 22:1x）

`framework/host/` → `framework/platform/win32/imm32/`（9 个文件），
`framework/ui/` → `framework/platform/win32/ui/`（10 个，含 README）。全部 `git mv`，
历史保留。纯结构改动，零逻辑变更。

**判据不是测试，是字节。** 用同一交叉工具链（`toolchain-i686.cmake`、Unix Makefiles、
Release、`-O3 -DNDEBUG`）在改动前后各做一次**干净构建**，比对 `.ime`：

| | 改动前 | 改动后 |
|---|---|---|
| 大小 | 940,888 B | 940,888 B |
| sha256 | `4345c97f…` | `945d1b3e…` |
| 差异区域 | 3 个，全部是链接器戳 | |

```
0x000088  TimeDateStamp (linker clock)
0x0000D8  CheckSum (derived from image)
0x060A04  export dir TimeDateStamp (copy of the clock)
VERDICT: equivalent
```

三个差异是**任何两次构建之间都会有的**：先用 `build-win`（另一个目录、另一时刻构建）
与基线比对，得到的也是同样三个区域、同样的字段。判据因此是"除链接器戳外逐字节相同"。

**判据必须先被验证，否则"通过"可能只是它不会报警。** 四次变异：

| 变异 | 预期 | 结果 |
|---|---|---|
| 翻转 `.text` 里一个字节 | FAIL | ✅ `0x010000 **payload**` |
| 截断 1 字节 | FAIL | ✅ `SIZE MISMATCH` |
| 交换 `CMakeLists.txt` 里两个源文件顺序 | FAIL | ✅ `.eh_frame` 重排，多处 payload |
| 不动 | PASS | ✅ `equivalent` |

第三条是这次的关键：源文件**顺序**变化会让异常处理表重排，产物字节改变——
所以"漏了一个源文件"与"顺序变了"都能被抓住，判据不松。

**计划里有个技术描述是错的。** 原稿写"只允许 4 处差异（…调试字段 `0x60A04`）"。
实测该镜像**没有调试目录**（数据目录 #6 全为 0）；`0x60A00` 是 `.edata` 节起点，
`0x60A04` 是 `IMAGE_EXPORT_DIRECTORY.TimeDateStamp`，链接器把同一个时钟值也写在那里。
结论不变（仍然是"只允许链接器戳不同"），但工具必须按 PE 结构定位，
于是 `tools/compare_ime.py` 走数据目录 #0 与 #6 找字段，不硬编码偏移。

**改动面比计划预估多两处**：

| 位置 | 计划 | 实际 |
|---|---|---|
| `ime/CMakeLists.txt` | 9 源 + 3 include | 一致 |
| `tools/check_m1.py` | 15 处 | 16 处（多一条 `dat_loader.hpp/.cpp` 存在性断言） |
| `pinyin/host/dat_source_win.cpp` | 1 条 include | 0 处需要改——它引的是裸文件名 `trace_stamp.h`，靠 include 目录解析 |
| `.github/workflows/ci.yml` | 未列 | **9 处**：`-fsyntax-only` 步骤直接列了全部源文件路径 |
| 文档 | "若干" | 活文档 38 处路径；计划与历史报告 30 处**保留原样**（见下） |

**文档不是全改。** 判断依据是"读者会不会拿它去找文件"：

- 改：`HANDOVER`、`PLAN`、`ROADMAP` 的任务行、`framework/README`、`ui/README`、
  `libernt/PORTABILITY-ASSESSMENT`、`docs/font/*`（F 里程碑待执行，路径必须准）
- 不改：里程碑报告（`M0-REPORT`/`M1-REPORT`/`m2/*`）与评审（`reviews/*`）——
  它们是**当时的快照**，回填路径会让"当时代码长什么样"失去证据价值
- 保留自定义：`PLAN-MODULARIZATION` §3 的映射图就是 X1 的任务定义，
  只在旁边标注"已执行"，不改写

`HANDOVER` 里那张 ASCII 架构图因为路径变长要重新排。第一次写的重排脚本把边框行
和内容行一视同仁，给纯边框行也加了缩进空格，整张图当场散架——回滚后改成区分
"只含 `─▲┴┬┼` 的线行"与"内容行"，宽度统一 70（CJK 按 2 计）才对齐。

#### 顺带发现：一条 CI 门禁长期失效，并掩盖了另一条

改完路径跑 CI 同款语法检查（`g++ -fsyntax-only` + `tools/syntax_stub`），报
`HIMC 未声明`。**先确认这是不是我改坏的**：用 `git worktree` 在 `f3f270a` 上跑同一命令，
同样失败——**改动前就是红的**。

查远端 Actions：**最近 8 次运行全部 failure**（历史共 107 次）。失败链是：

```
engine-smoke 第 3 步「IME source syntax check」失败（HIMC 未声明）
  → 该 job 后续 Configure/Build/Test/Upload 全部跳过
  → artifact rospinyin-m2-data 从未上传
  → publish-artifacts-branch 第 4 步 download-artifact 报 "Artifact not found"
```

也就是说：`windows-build` 一直是成功的，**坏掉的只有 Linux 那条线**，
而它的第一步就断了，后面的 ctest、覆盖率门禁、文档门禁在 CI 上从未真正跑过。
我们这一路说的"门禁全绿"一直只是本机成立。

根因：`AlignOpenStatus()`（U1 修复）让 `imm_entry.cpp` 开始调用 `ImmGetOpenStatus`/
`ImmSetOpenStatus`，但 `tools/syntax_stub/imm.h` 是当初为 `install-rospinyin.exe`
写的，只声明了 `ImmInstallIMEW`。它的注释还写着"不会有分歧"——这个假设被后来的改动打破了。

补齐 stub（`imm.h` 加 `HIMC` + 2 个函数；`windows.h` 加 GDI 字形探针、文件属性、
`SystemParametersInfoW`、`VK_SHIFT`、stock brush 等 15 个符号）后通过，并做了三次变异验证：
拼错符号 FAIL、少传参数 FAIL、`const wchar_t*` 传给 `HIMC` FAIL。

顺带一个一致性问题：`LTGRAY_BRUSH` 在 `candidate_window.cpp` / `toolbar.cpp` /
`ui_window.cpp` 里都有 `#ifndef` 兜底，只有 `composition_window.cpp` 漏了。
四处各自打补丁这件事本身就是"stub 不完整"没人发现的证据——
stub 补全后这些兜底都成了死代码，但保留它们无害（真实 Windows 构建里 `#ifndef` 不生效）。

**教训**：门禁红着比没有门禁更糟——它会让人以为"CI 在管这件事"，
而实际上第一步就断了，后面每一条都只是在列表里排队。
排查时应先区分"真红"与"连带红"：`publish-artifacts-branch` 的失败本身不是 bug，
它只是 engine-smoke 失败的后果。

**修复结果：这个仓库第一次 CI 全绿。** 推送后 run `44323972260`（`4e150a5`）
三个 job 全部 success，且**每一步都真的执行了**（不是跳过）：

| job | 步骤 |
|---|---|
| engine-smoke | 语法检查 → Configure → Build → Test → **Build rospinyin.dat** → 覆盖率 → 文档门禁，10/10 |
| windows-build | 两套 configure（standalone / in-tree）→ 构建 → PE 导入表校验，11/11 |
| publish-artifacts-branch | 两次 download-artifact → 图标资源校验 → 提交二进制，6/6 |

其中 `Build rospinyin.dat from pinned sources` 与 `First-page coverage` 是
**首次在 CI 上真正运行**——此前它们在列表里排队，从来没轮到过。
历史 107 次运行中，这是第一次 `conclusion == success`。

### 11.24 X2-a0 注入缝：scheme 从"链接期符号"变成"注册表查询"（2026-11-18 22:5x）

审计 A-P0-2 的原话是"所谓插件实际是链接期硬接线"。这判断是准的，而且比它说的更糟一点：

`scheme_instance()` 在 `imfw/scheme.hpp` 里只是**声明**，实现必须由装配件给
（`ime/scheme_reg.cpp`）。所以"换方案"从来不是加一个文件，而是**换掉框架所依赖的那个符号**
——`ime/CMakeLists.txt:7` 的注释自己写着"第二个输入法用自己的 scheme_reg 再编译"。
X2-b 要假 scheme，按这条路径走下去每一次都得改框架，而 X3 的判据（`framework/` 零 diff）
会在最后一步把前面的偷懒全部打回。

**做法**：`imfw::SchemeRegistry` 把原来融在一个符号里的两半拆开。

| 半 | 谁提供 | 何时发生 |
|---|---|---|
| 创建 `scheme_factory()` | 装配件（`ime/scheme_reg.cpp` / `ime/scheme_reg_toy.cpp`） | 第一次查询时，函数内静态 |
| 查询 `SchemeRegistry::get()` | 框架（`imfw-engine`） | 先看 `install()` 过的，再问工厂并缓存 |
| 注入 `install(Scheme*)` | 测试 / 宿主 | 想什么时候换就什么时候换 |

`scheme_instance()` 保留为 `get()` 的别名，所以**10 处调用点一字未改**
（`session_store.cpp:34,55`、`ui_server.cpp:26,35`、`imm_entry.cpp:299,414,519,547`、
`ime_ui.cpp:176,211`）。这是有意的：这次只证明"工厂可换"，把调用点改成语义更直白的名字
属于 X2-d 拆 TU 时顺手做的事，混在一起会让回归面变大。

**注册表放哪：只能放 `framework`。** 三个候选位置都试过：

| 位置 | 问题 |
|---|---|
| `ime/` | 宿主测试要按相对路径去链接"装配 Windows .ime 的目录"；第二个方案还得复制一份注册表 |
| `tests/` | 只有测试能用，产品代码不能——等于没注入 |
| `framework/`（`imfw-engine`） | 无 Windows 依赖、与 `Scheme` 同层、第二个方案直接复用 ✅ |

**计划里那条判据和现实对不上，处理方式是拆提交而不是改判据。**
`PLAN-MODULARIZATION` X2-a0 写的是"该提交 `git diff --stat` **不出现 `framework/`**"。
但"可注入"这件事**只能在共享核心里做**——缝不在 `imfw-engine` 里，第二个方案就必须自己复制
一份注册表，那 G4 就永远达不到。所以拆成两个提交：

| 提交 | 内容 | `framework/` diff |
|---|---|---|
| `feat(framework): SchemeRegistry 注入缝` | 2 个新文件 + 3 处注释/CMake | 有——这就是缝本身 |
| `feat(samples): toy scheme 作首个消费者` | `samples/` + `ime/scheme_reg_toy.cpp` + CMake 选项 + CI | **0** |

审计原话是"**接入 toy scheme 时** `framework/` 零 diff"，判据按这个读就成立，而且它是
真正想问的问题：**消费者**能不能不动框架。把"加缝"也算进去，这条判据会退化成
"框架永远不许改善"，那不是它的意思。

**顺带发现一个假绿。** `check_m1` 原来断言 `"scheme_instance" in ime/scheme_reg.cpp`。
改完之后这个字符串仍在文件里——在我写的注释里（"本文件不再定义 `scheme_instance()`"）。
**门禁靠词匹配，注释就能让它绿。** 断言改为查定义形式 `Scheme& scheme_instance`
（以及必须存在的 `Scheme* scheme_factory()`），并用一次变异确认它会红。

**DllMain 红线没动。** 唯一的顾虑是"绑定发生在什么时候"。这里没有用静态初始化器
（那会让工作发生在 loader lock 里），而是让 `get()` 在第一次查询时才调工厂：
guarded static 从 `ime/scheme_reg.cpp` 搬到了 `get()` 内部，**触发点还是
`RegisterUIServerClass` 里那次 `scheme_instance()`**，装载期做的事情与改动前完全一样。

**证据**

| 项 | 结果 |
|---|---|
| `ctest` | 13/13（新增 `scheme-registry`、`toy-scheme` 两个目标） |
| `check_m1` | 65/65（+2 新增，1 条改严） |
| `check_docs` | 7/7 |
| 拼音版 `.ime`（i686 交叉） | 942,092 B（+1,204 B：注册表 + 一条 `fputs` 诊断路径） |
| 玩具版 `.ime` | 870,475 B（无拼音引擎与内置表） |
| 两者的导出表 | 16 个 IMM32 导出齐全，与改动前一致 |
| 两者的导入表 | 仍是 GDI32 / IMM32 / KERNEL32 / msvcrt / USER32——**未新增 DLL 依赖** |

变异验证三次，全部变红：忽略 `install()`（注入断言红）、不缓存工厂结果（缓存断言红）、
玩具方案不做小写归一（字母表断言红）。

**CI `44329215082`（`40d22ea`）全绿，三个 job 的每一步都真执行**，其中
`Configure (toy scheme)` / `Build (toy scheme)` / 导入表校验里的"两个 `.ime` 不得字节相同"
是新加的步骤——它们第一次跑就通过了，说明玩具分支在 MSYS2 的 MinGW 上与本地交叉工具链
行为一致。

**没验证的：真机。** 本机不能代用户敲键（红线），所以"可跑"目前只到可加载级证据
（导出/导入/资源齐备、两版二进制不同）。真机冒烟的最小步骤写在
`PLAN-MODULARIZATION` X2-a0 的执行记录里，等用户有手时补一次。

**三条教训**

1. **判据是照着理想路径写的，执行时可能和目标互斥。** 这时要说清楚"哪一半让步、
   为什么"，而不是两边都不做、也不是偷偷把判据改松。这次让步的是"这一条提交"，
   不是"这条判据"。
2. **门禁靠词匹配就会被注释骗。** 判据要钉在**定义形式**上，而不是关键词出现与否。
   这个仓库为此已经改过一次方向（审计前 `check_m1` 是"黑名单扫词"，改注释即误判，
   后来换成了调用点白名单）；这次是同一个病在另一处的发作。
3. **注入缝的价值不是"能换"，是"换的时候不用碰调用方"。** 所以别名
   （`scheme_instance` → `get()`）比"顺手把 10 处调用点改成新名字"更值钱。

> **勘误（同一次提交内发现）**：本节的"10 处"是对的，`feat(framework)` 那条提交信息与
> `CHANGELOG` 的初稿都写成了 6 处——6 是计划 §2 接缝表里"引擎调用点"的数字，
> 被顺手抄了过来。真实数字以 `grep -rn 'scheme_instance()' framework/platform | wc -l`
> 为准（imm_entry 4、session_store 2、ui_server 2、ime_ui 2）。

### 11.25 X2-a 键策略剥离：最忙的 90 行第一次有宿主测试（2026-11-18 23:0x）

`process_key()` 是 IME 里执行次数最多的函数，而它此前**宿主侧覆盖为零**。原因不是没人想测，
是它写不出来测试：一个函数里同时读 256 字节 `lpbKeyState`、问 `ImmGetOpenStatus`、开自己的
窗口、往 `TRANSMSGLIST` 里写 WM_IME_CHAR。四件平台事包着一个判定，判定本身就被钉死在
ReactOS 的键盘上。M2 报告里的每个 bug —— 字母翻倍、候选窗冻结、"多出字 / 丢字" —— 都是这
90 行里某个 `if` 的事；它们全靠真机 trace 逐条回溯。

**做法**：把判定搬进可移植核心，平台事留在适配器。

| 层 | 文件 | 内容 |
|---|---|---|
| 契约 | `framework/include/imfw/host.hpp` | `KeyEvent`（vk / ch / kind / key_up / ctrl / alt）、`KeyResult`（commit）、`Host`（open 读写、show/hide、can_render、2 个诊断钩子） |
| 状态 | `framework/include/imfw/session_state.hpp` | `ImeSessionState` 从 IMM32 侧搬出——它本来就全是可移植类型，以前只是"和 store 放一起" |
| 策略 | `framework/src/host_key.cpp`（254 行，进 `imfw-engine`） | `wants_key()`（原 `should_consume_key`）+ `handle_key()`（原 `process_key`） |
| 适配器 | `imm_entry.cpp`（783 → 609 行） | `key_event_from()` 做 vk→`KeyKind`/字符翻译；`Win32Host` 实现 `Host` |

**为什么不是"再包一层函数"。** 只把签名换成 `handle_key(Session&, KeyEvent)` 而仍调
`ImmGetOpenStatus`、`UiShow`，测试照样得链接 windows.h 与 UI 库——那不是可测，只是改名。
必须把**四个副作用**都抽成 `Host`，宿主才可能是个假对象。

**三个搬不动的东西，原因比结论重要**：

1. **lone-Shift 的 latch 必须留在"归属"路径。** `ImeProcessKey` 返回 `FALSE` 时 IMM32 不会
   调 `ImeToAsciiEx`。把切换挪进 `handle_key`，Shift 就永远不生效——而这一点只有真机才看得
   出来。所以 `wants_key` 收 `ImeSessionState&`（非 const），头文件里写明"唯一的原因是 Shift"。
2. **key-up 位只有归属侧看得见。** `ImeToAsciiEx` 的 ABI 没有 `lParam`，所以适配器那一侧
   构造事件时传 0，`key_up` 恒 false。这与"归属侧必须拦 key-up"是同一件事的两面。
3. **trace 不能跟着策略走。** `feed` 那一行要 `UiCountersSnapshot()`（窗口移动/重绘计数），
   是宿主的数据；`cand-filter` 那一行才是策略产生的。做法是 `Host` 上两个**默认空实现**的
   钩子：Win32 宿主转成 `IMFW_TRACE`，宿主测试一个字都不用写（默认实现让假宿主不必知道
   trace 存在）。`-DROSPINYIN_TRACE=ON` 的交叉构建通过，证明这些钩子真的编进 PE 了。

**判据换了：这条不能用字节。** X1/X2-a0 能拿"产物字节不变"当判据，是因为它们不改变机器码。
这次代码换了 TU、多了虚调用，字节**必然**变（+4,207 B）。所以判据是两件事：
**PE 的对外面不变**（16 个导出名逐个一致、导入函数表逐个一致、DLL 依赖仍是
GDI32/IMM32/KERNEL32/msvcrt/USER32）+ **行为由宿主测试钉住**。

**证据**

| 项 | 结果 |
|---|---|
| `ctest` | **14/14**，新增 `host-key`（35 条断言，无 `<windows.h>`） |
| `check_m1` | **67/67**（+2：策略可移植；Shift 也走同一条策略。5 条改严） |
| `check_docs` | 7/7 |
| 拼音版 `.ime` | 946,299 B；16 导出、导入函数表与改动前**逐个一致** |
| 玩具版 `.ime` | 874,682 B（同一份框架，验证玩具方案没被落下） |
| trace 版 `.ime` | 971,995 B，`-DROSPINYIN_TRACE=ON` 构建通过 |
| `imm_entry.cpp` | 783 → **609** 行；策略移到 254 行的 `host_key.cpp` |
| CI | run `44333478437`：Linux 测试、MinGW i686 构建、产物发布三个 job 全 success（`host-key` 在 ctest 里一起跑） |

**变异验证 8 次，全部变红。** 策略 6 次：不拦 key-up、关态仍吞键、Shift 闩不清、翻页无效也
刷 UI、丢弃过半不兜底、退格后永远 composing —— 每条都报出对应的那句断言。门禁 2 次：给
`host.hpp` 加一行 `<windows.h>`、把 `ImeProcessKey` 的 `wants_key` 调用删掉。

**第二次踩同一个假绿的坑。** 门禁第一版写 `"imfw::wants_key(" in process_body`。删掉普通
分支那处调用后，**检查仍是 66/66 全绿**——因为 Shift 分支里还有一处一模一样的调用。这与
§11.24 的 `"scheme_instance" in 文件` 是同一类错误，值得当规则写下来：

> **断言问"这个词出现过吗"，就测不出"这一处调用没了"。** 要么钉完整调用形式，要么数出现
> 次数，要么钉定义/结构化形状。`check_m1` 里凡是 `in body` 的断言，都要问一遍
> "另一处相同调用会不会让它继续绿"。

**没做**：真机冒烟（不能代用户敲键）。补一次的最小步骤：装拼音版 → 敲 `nihao` → 候选出现 →
`1` 上屏 → `Esc` 清空 → 按一下 Shift 再敲 `a`，上屏应是 ASCII `a`（说明关态生效）。
这一步不通过，X2-b 的 `host_smoke` 就没有基线。

### 11.26 X2-b 宿主冒烟：把"调用序列"变成可断言的（2026-11-19 11:0x）

**真机 Shift 四步的结果，先说结论：切换本身是对的，用户看不到它。**

| 步骤 | 用户操作 | 结果 | 判读 |
|---|---|---|---|
| 1 | `Esc` 清空 | ✅ | 取消而不是提交空串 |
| 2 | 单独按 Shift | 托盘徽标**没变** | 见下 |
| 3 | 敲 `abc` | ✅ 直接上屏、不弹候选 | 关态生效——**这一条才是切换是否成功的判据** |
| 4 | 再按一次 Shift | 仍显示「中」 | 与第 2 步同因 |
| 5 | 敲 `ni` | ✅ 候选窗回来 | 开态恢复 |

第 3、5 步合起来证明 `fOpen` 真的翻了，第 2、4 步只证明**那个徽标不反映 `fOpen`**。
截图放大后两个控件可以分清：

- **红底「中」在通知区**（和音量、网络图标并排）——这是 ReactOS 自己的输入法/布局指示器，
  跟**键盘布局（HKL）**走。我们从开机起就是中文布局，所以它一直是「中」，按 Shift 不会动。
- **灰底「拼」在任务栏上方**——这才是我们自己的 `toolbar`（`toolbar.cpp`），标签来自
  `Scheme::toolbar_label()`，**一个静态字符串**。

也就是说：`set_ime_open()` 调了 `ImmSetOpenStatus`（`imm_entry.cpp:265`），OS 侧状态确实写了，
但**我们没有任何一处 UI 把这件事显示出来**。这不是 X2-b 的缺陷，是一个独立的 UX 缺口
（**已合入 main**，见 §11.36；合入前记的「未合入」状态已过期。托盘「中」本来就不该动——它跟键盘布局走，而 Shift 不切布局）。

**X2-b 做的事**：`tests/host_smoke_test.cpp`（463 行）。

| 件 | 实现 | 为什么这样 |
|---|---|---|
| engine | `ScriptedEngine`：每个操作从队列取一条固定 `Result` | 断言的对象必须是**宿主**，引擎的回答只能是常量 |
| host | `RecordingHost`：一次可观测调用一行日志（`show comp=… segs=… cands=… page=…` / `hide` / `set_open(0)`） | 用户能看到的就是这些参数 |
| scheme | `SmokeScheme`：字母表 + 默认选字/翻页键 | 与 `host_key_test` 同源，避免"测试专用策略" |
| driver | 复刻 IMM32 的门：`wants_key` 为真才调 `handle_key` | 少了这层，X2-c 若把判定搬到错误的位置也测不出来 |

键序就是计划里那五段：`zhongguo` → `2` → `Backspace`×2 → `Esc` → 单独 Shift。断言三条：

1. **13 条 host 调用逐字相等**（含 `hide`、`set_open(0)`），失败时打印第一处差异的两侧原文；
2. **commit 序列** `["钟"]`，且引擎收到的操作恰好是 `feed`×8 + `commit:钟` + `backspace`×2 + `cancel`；
3. 引擎队列空即 `underrun`——宿主多喂一次、多退一次立刻红。

额外两条护栏各一例：关态/无组合时"键不入引擎、不产生 host 调用"；P1-2 的"丢一半保留"与
"丢 2/3 整表保留"。

**为什么断言日志而不是最终文本。** X2-c（`route_key`/`Intent`）和 X2-d（拆 TU）的验收都是
"调用序列不变"。只断言最终文本，重排 `show`/`hide`、少刷一次 UI、多调一次引擎全查不出来——
而这三种正是"候选窗冻结""字母翻倍"那类 bug 的形状。

**证据**：`ctest` 15/15、`check_m1` 68/68（新增：文件与注册必须同时在位——只存在不注册的测试
文件比没有更糟，看着像覆盖）、`check_docs` 7/7。变异 4 次全红：退格后一律 `hide`（第 9 条调用
从 `show` 变 `hide`）、提交后不 `refresh_ui`（第 8 条缺失）、关态不写 `fOpen`（少一条
`set_open(0)` + 关态仍吞键）、关态仍吞键。

**顺手钉住的一件事**：退格到组合为空（`comp=g`，没有候选）时宿主**仍然 `show`**，由候选窗自己
按空 pager 收起。这条日志现在被写进期望序列里，因为它就是"候选窗一闪一闪"的机制——将来改
候选窗稳定性时，这行日志会变，改动会被迫显式面对它。

### 11.27 状态指示器：Shift 切的是 IME 状态，不是键盘布局（2026-11-19 11:3x，**已合入 X2-w1**）

> **历史快照**：下面这组 `ctest` 16/16、`check_m1` 71/71 是当初在 `wip/status-indicator`
> 分支上、尚未与 X2-d/e 合并时的判据，**不是现在的 `main`**。合入记录与当前判据见 §11.36
> （`ctest` 17/17、`check_m1` 107/107）。保留原始数字是为了让"当时证明了什么"可追溯。

§11.26 把"按 Shift 托盘不变"记成 UX 缺口，方向记错了一半。用户给的模型是：

> 按 Shift 切换的不应该是输入法/键盘布局，应该是输入法的状态。托盘里的[中]应该就是不变，
> 变的应该是输入法工具栏里的状态指示器，例如默认 `[拼:开]`，按 Shift 之后 `[拼:关]`。

按这个模型复核代码，**语义本来就对，缺的只是显示**：

| 事实 | 证据 |
|---|---|
| Shift 只写 `fOpen`，不动 HKL | 全仓库 `ActivateKeyboardLayout` / `LoadKeyboardLayout` **0 处**（新增静态断言钉住）；`Win32Host::set_open` 只调 `ImmSetOpenStatus` |
| 所以托盘「中」不变是**正确行为** | 红底「中」跟 HKL 走；它的不动，恰好证明我们没在切布局 |
| 我们自己的工具栏此前是静态串 | `UiToolbarShow` 直接传 `Scheme::toolbar_label()`，一个常量「拼」 |

**改法（四处，都是小改动）**

| 位置 | 改动 |
|---|---|
| `imfw/scheme.hpp` | 新增 `toolbar_state_label(bool open)`，默认 `nullptr`（无状态的方案保持裸标签）→ 四个假方案零改动 |
| `framework/src/toolbar_text.cpp`（新，25 行，可移植） | `拼` + `:` + `开/关`。**提到可移植层是为了能测**：窗口要 GDI，字符串不用 |
| `toolbar.cpp` | 宽度实测（`GetTextExtentPoint32W` + 上下限）：`拼:开` 在原来写死的 44 px 里会被裁；右边缘固定、向左生长；文本相同且可见时**直接返回**，否则每键一次 `InvalidateRect` 就是 P0-3 刚拆掉的重绘风暴 |
| `imm_entry.cpp` | `ImeProcessKey` 每个键拉一次状态（bool 比较），Shift 手势后立刻再拉一次 |

**为什么"拉"而不是"推"。** 直觉做法是给 `Host` 加 `on_open_changed` 钩子，但 `fOpen` 有第二条
改写路径：imm32 的 Ctrl+Space 直接 `ImmSetOpenStatus`，**根本不进我们 DLL**。推送式钩子只能覆盖
我们自己的 Shift，Ctrl+Space 之后工具栏会一直停在旧状态。键路径拉取的代价是一个 `ImmGetOpenStatus`
加一次 bool 比较，换来"无论谁改的状态，下一个键就纠正"。

**判据**：`ctest` **16/16**（新增 `toolbar-text`，钉 `拼:开`/`拼:关`/自定义 `仓:ON`/无状态裸标签
四条）、`check_m1` **71/71**（新增三条：标题必须由 `toolbar_text` 生成**且**旧的 `toolbar_label()`
直传必须消失——两侧都钉，否则"某处还保留旧调用"会让断言继续绿；键路径两处同步的**计数**；
布局切换函数全仓库 0 处）。变异 5 次全红：状态恒为开 / 丢冒号 / 无状态也拼冒号 / 退回静态标签 /
删掉 Shift 后那次同步 / 假装调 `ActivateKeyboardLayout`。

**对外面**：16 个导出名及其顺序逐个一致、5 个 DLL 依赖一致、**每个 DLL 的导入函数集逐个一致**
（没有引入任何新 API，所以没有新的 ReactOS 侧风险）；i686 拼音版 `949,940 B`（+3,641）、
玩具版 `877,973 B`。

**未验的部分**：真机。用户按一次 Shift，工具栏应从 `拼:开` 变 `拼:关`、托盘「中」保持不动——
这两个"一个变、一个不变"必须同框看见，单独看任一都不足以判定。


### 11.28 键路由 = 一个纯函数（X2-c，2026-11-19 12:4x）

X2-a 把键策略从 `imm_entry.cpp` 搬到了 `framework/src/host_key.cpp`，但 `handle_key()`
仍是"边判定边干活"：选字分支一旦命中就直接 `commit_text`，翻页分支命中就直接动 pager。
后果是**想问一个键是什么意思，必须先把引擎和宿主都造出来**——`host_key_test.cpp` 为此写了一个
`FakeEngine` + `SpyHost`，`host_smoke_test.cpp` 更是整段键序。

X2-c 把它劈成两半：

| 文件 | 职责 | 依赖 |
|---|---|---|
| `framework/src/key_route.cpp` + `include/imfw/intent.hpp`（新） | `route_key(scheme, event) -> Intent`：这个键是什么 | 只有 `Scheme`。**不问 session / engine / host**，也不看 `key_up`/`ctrl`/`alt`/`fOpen` |
| `framework/src/host_key.cpp` | `handle_key`：拿到 `Intent` 之后做什么 | 照旧（engine + host） |

**词汇表**（`intent.hpp`，`status: draft`）：`None / Input{ch} / Select{slot} / PagePrev / PageNext /
Backspace / Escape`。两处与计划不同，都是执行时发现计划的名字会撒谎：

- `SelectDigit(n)` → `Select{slot}`：空格不是数字，但它选第 1 个。`slot` 仍是**页内相对**索引，
  所以第 2 页按 `1` 选的是第 10 个——这条由 `CandidatePager` 保证，路由只负责把键翻译成槽位。
- `ToggleLang` **删掉**：Shift 切中英确实是"意图"，但产生它的那条路在 `wants_key()`
  （key-up 必须回答"不是我的"，闩只在那一层）。一个**只有"承诺永不 dispatch 的那一层"才能产出**的
  意图，是留给下一个读者的陷阱。等真有第二个前端要"请求切换"，再加。

**判据（这是 X2-c 的全部意义所在）**：`host_smoke` 的 13 条日志**一行未改**。搬判定时最容易犯的错
是顺手把分支顺序调了（选择/翻页都声明同一键时，谁赢？），`tests/key_route_test.cpp` 里的
`OverlapScheme` 专门钉这一条：`'5'` 被两处都声明 → 仍然是 `Select`。

**一次自己抓到的假绿（第三次同类）**。新加的 `check_m1` 断言最初写"`handle_key` 里不许出现
`scheme.`"。变异时我塞进去的违规代码是 `scheme_instance().page_key(...)`——它**不含** `scheme.`
（中间隔着下划线），71/71 依然全绿。这与 §11.24 的两次是同一个形状：**凡"某词出现过/没出现过"
的断言，都要问"另一种写法会不会绕过去"**。改法是不再匹配词，而是匹配"策略调用本身"：
`page_key(` / `is_selection_key(` / `input_char(` 一个都不许有，且 `scheme_instance()` 只许出现一次
（就是传给 `route_key` 的那次）。两种写法各变异一次，都红。

**对外面**：16 个导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；
i686 拼音版 `946,299 → 947,148 B`（+849），玩具版 `874,682 → 875,019 B`（+337）。
新增的 849 字节全是"多一个 TU + 一次 switch"，没有任何新 Win32 API 调用。

### 11.29 X2-d 拆 god object：613 行的入口文件变成 4 个 TU，两条激活路径合成一条（2026-11-19 13:1x）

`imm_entry.cpp` 之前一个文件装五种东西：trace 通道、vk 翻译、`TRANSMSGLIST` 上屏、
`Host` 的 Win32 实现、16 个导出。要回答"这个键是怎么被吃掉的"，得把 613 行从头读到尾。
拆完（**总数 613 → 782 行 / 7 个文件**，增长全是每个文件自己的许可证头与 namespace 壳）：

| 文件 | 行数 | 只负责 |
|---|---|---|
| `imm_entry.cpp` | **314**（613 → 314） | 16 个导出 + 唯一的激活序列 `activate_session()` |
| `key_translate.cpp` | 103 | vk → `KeyEvent`（含 key-up 位）、`KeyResult` → `TRANSMSGLIST` |
| `win32_host.cpp` | 92 | `Win32Host`：`Host` 的 Win32 实现（唯二调用 `ImmGet/SetOpenStatus` 的地方之一） |
| `trace_line.cpp` | 117 | `ROSPINYIN_TRACE` 通道 |

**与计划的一处偏离，如实记**：计划写"按 6 类职责拆 TU"，其中"候选过滤（`:344-382`）"这一类
在 X2-a 就已经搬进 `framework/src/host_key.cpp` 的 `renderable_candidates()` 了（那是可移植逻辑，
不该留在平台层）。所以今天只拆出 4 个 TU，不是 6 个——**计划里的行号是 783 行那版的**，
按行号找职责这件事本身已经过期了。

**合并两条激活路径。** `ImeSelect(TRUE)` 与 `ImeSetActiveContext(TRUE)` 原本做同样三步，
但**顺序相反**：一个先 `on_select` 再对齐 `fOpen`，另一个对齐 `fOpen` 再 `on_select`。
U1（"第一次切到本布局时是关的"）就是这么来的——一段必须按同一顺序发生的事，写了两份。
现在统一到 `activate_session()`，顺序是：

1. `AlignOpenStatus`：它决定工具条接下来显示什么；
2. `on_select(true)`：引擎契约要求它在任何 `feed()` 之前；
3. `UiToolbarShow`：渲染第 1 步建立的状态。

**解除激活没有合并**：`ImeSelect(FALSE)` 要 `UiDestroy` + `release_session`，
`ImeSetActiveContext(FALSE)` 只 `UiHide`（输入上下文要跨焦点变化活下来）。合并那是改行为，不是重构。

**trace 的十进制转换合并。** `commit()` 里 pid 与 tid 各有一份手写除十循环，现在都走
`trace_stamp.h` 的 `put_uint()`；顺手给它加了 `cap` 参数，让"截断而不是越界"从一句注释变成
一个有参数的保证（`len + 1 < cap`，与原来手写循环的 `n + 2 < sizeof(out)` 同义）。

**判据切换的代价（这次最值得记的一条）。** `check_m1.py` 里一多半断言原文是 `... in entry`，
`entry` 就是 `imm_entry.cpp`。拆完之后这些断言**会直接变红**——但它们红的原因是"文件里没有了"，
不是"行为变了"。于是有两种改法，而它们不等价：

- 把 `entry` 换成 4 个 TU 的拼接 `entry_all` → 断言继续绿，但"搬走了"和"多了一份拷贝"再也分不出来；
- 按**归属**改：某个 TU 必须**有**，另一个必须**没有**。

选的是后者，新增了三条 X2-d 断言（变异 6 次全红）：

| 断言 | 双向钉法 |
|---|---|
| `imm_entry.cpp` 是入口文件 | ≤ 400 行 **且** 里面不许出现 `class Win32Host`、`trace::Line`、`framework_key_char`、`offsetof(`；同时 `class Win32Host` 必须在 `win32_host.h` |
| 激活序列只存在一次 | `activate_session` 必须含 `AlignOpenStatus(` / `on_select(true)` / `UiToolbarShow(`；两个入口点必须含 `activate_session(hIMC)` **且**都不许含那三个调用 |
| trace 只有一个十进制格式化器 | `trace_line.cpp` 里 `put_uint(` 必须在、`% 10` 不许有；`trace_stamp.h` 的函数签名必须带 `cap` |

**门禁**：`ctest` 16/16（可移植层一行未动，`host_smoke` 输出逐字一致）、`check_m1` **77/77**（+6）、
`check_docs` 7/7。变异 6 次全红：入口点抄回一遍 `AlignOpenStatus` / `ImeSetActiveContext` 绕过
`activate_session` / vk 表回落到 `imm_entry.cpp` / trace 手写十进制回归 / `set_open` 改名 /
文件重新长到 405 行。

**对外面**：16 个导出名及顺序、5 个 DLL 依赖、每个 DLL 的导入函数集**逐个一致**；i686 拼音版
`947,148 → 947,969 B`（+821）。按计划 X2-d **不看字节**（拆 TU 本就该改字节，跨 TU 少掉的内联
与每文件多出的节对齐都在这 821 里），这一栏只是留个记录。

**一处 trace 文本变了**：`ImeSelect` 里原来的 `select state=… engine=…` 改成了
`activate state=… engine=…`，因为这条现在由共享路径打印、两个入口点都会打。前面的
`select imc=…` / `active f=…` 行还在，所以一份日志仍能分清是哪个入口点。

**真机验收（2026-11-19 14:3x，用户手动操作，结论「X2D 确认全部正常」）。**
覆盖 `.ime` 时先撞了 `err=32`，按 §6.4.1 走的四步（默认输入语言切到英语(美国) → `copy /y` →
切回 RosPinyin → 重启 guest）。装没装上用两个互相独立的判据确认：

- `dir C:\ReactOS\system32\rospinyin.ime` = **947,969 B**（X2-c 是 947,148，X2-a0 是 942,092，
  这个大小只对 X2-d 无 trace 版成立）；
- 记事本敲 `nihao`：组合串 5 个字母、候选 **9 条**（首选「你」）、页码 `1/4`、无残窗；
  随后选字、翻页、Backspace、Esc、Shift 开关状态逐项试过均正常。

证据：`m0/vm/evidence/x2d/nihao-full.png`（整屏，含上面那条 `dir` 输出）与
`m0/vm/evidence/x2d/nihao-candidates.png`（候选窗 2× LANCZOS）。

**抓图方法**（本机唯一合规的观察手段）：QEMU monitor 的 `screendump`，经 `m0/vm/monitor.sock`
出 PPM 再转 PNG，全程不碰键盘鼠标。红线是不代用户发键盘事件，所以"敲 `nihao`"只能由用户完成。

**一处容易误判**：`dir` 里那个文件日期是 `11/18/2026 10:11 PM`，比构建时间（11-19 13:08）还早。
原因是 guest 时区是 UTC−08:00 而 QEMU 默认 `-rtc base=utc`（见 `tools/vm_clock.py`），
**判版本只能看字节数或 `rhash.exe` 的 sha256，不要看日期**。

**真机验的是 X2-d，不是 main 当前的头部。** X2-e1/e2 只动了构建组织（库目标、数据源归属、
去链接时间戳），导出/导入面与 1386 个函数符号逐个未变，但它们**确实还没上过机**。
要验 X2-e2 版说一声——共享目录里备着 `rospinyin-e2.ime`（947,820 B，sha256 `93ab887d…`）。

---

### 11.30 X2-e1 平台层成为两个库：`ime/` 从"编译 12 个 cpp"变成"装配"（2026-11-19 15:0x）

在这之前，"什么是 IMM32 宿主层"只存在于 `ime/CMakeLists.txt` 那张 12 行的源码清单里，
而"什么属于窗口层"只存在于目录名里——两者没有共同的事实来源。现在平台层是两个静态库：

| 库 | TU | 只负责 |
|---|---|---|
| `imfw-win32-imm32` | `dllmain` / `imm_entry` / `key_translate` / `trace_line` / `win32_host` / `ui_server` / `session_store`（7 个） | 16 导出、键翻译、trace、会话存储、UI server 窗口 |
| `imfw-win32-ui` | `ime_ui` / `ui_window` / `composition_window` / `candidate_window` / `toolbar`（5 个） | 组合串、候选、工具条、窗口基类 |

`ime/CMakeLists.txt` 现在只剩 scheme 源码 + `rospinyin.def` + `rospinyin_res.rc`，一行平台源码都不写。

**两个库互相依赖，这是事实而不是设计缺陷。** 宿主调 `UiShow/UiHide/UiToolbarShow/UiCanRender`，
窗口层读 `imfw::g_hInst`（`dllmain.cpp` 定义）和 Scheme 的窗口类名。CMake 允许 STATIC 库成环
（链接时把整个连通分量重复一遍），所以两条 `target_link_libraries` 都写了，并在注释里说明为什么。
**没有**为了消环去造第三个 "common" 库——那样它的唯一内容就是一个全局变量，等于把环藏起来。

**踩到的第一个真问题：`.def` 不会把静态库的成员拉进来。** 第一次链接直接失败 16 次：

```
ld: cannot export ImeProcessKey: symbol not defined
```

因为 GNU ld 只为**未定义引用**去拉档案成员，而 `rospinyin.def` 里的 16 个名字是**导出条目、不是引用**——
这 16 个入口由 imm32.dll 在运行时调用，模块内部没有任何东西引用它们。解法是给宿主库套
`-Wl,--whole-archive` / `-Wl,--no-whole-archive`。窗口库不需要（宿主会调它，是普通未定义引用）。
这条已写成断言：删掉 whole-archive 会红，而不是等到链接期才发现。

**`ROSPINYIN_TRACE` 的声明位置也跟着挪了。** 读这个宏的是平台库，而 `framework/` 在 `ime/` 之前配置——
选项留在 `ime/CMakeLists.txt` 里的话，库侧永远看不到它，`-DROSPINYIN_TRACE=ON` 会**静默地只影响装配件**。
所以选项上提到顶层 `CMakeLists.txt`，并加了断言钉住"根目录有、`ime/` 没有"。

**一处临时妥协，如实记**：`pinyin/host/dat_source_win.cpp` 仍由 `ime/` 编译，它 `#include "trace_stamp.h"`
（宿主私有头），所以 `ime/` 暂时还留了一条 `framework/platform/win32/imm32` 的 include 路径，
注释写明 X2-e2 会把它带走。这条路径不进任何库目标。

**判据**（这次仍然不看字节，但把对外面钉得更细）：

- 16 个导出名**及顺序**与 X2-d 产物逐字一致；5 个 DLL 依赖一致；每个 DLL 的导入函数集**逐条、同序**一致
  （GDI32 12 / IMM32 2 / KERNEL32 59 / msvcrt 43 / USER32 22）。
- i686 拼音版 `947,969 → 948,481 B`（+512，正好一个文件对齐块：对象顺序变了，节对齐跟着变）。
  trace 版 `971,982 B` 不变（同一量化块内），玩具版 `875,019 → 876,352 B`（+1,333）。
- `ctest` 16/16（可移植层一行未动）、`check_m1` **83/83**（+5）、`check_docs` 7/7。
- 变异 4 次全红：把一个 TU 挪进另一个库 / 同一个 TU 进两个库 / 删掉 whole-archive / 把 trace 选项挪回 `ime/`。

**顺带加了一件本以为不属于这里的事：可复现链接。** 验收时用两个构建目录各编一次，产物**同为 948,481 B
但 sha256 不同**——只差 6 个字节：`0x88` 的 `TimeDateStamp`、`0xd8` 的 `OptionalHeader.Checksum`，
另两字节是同一时间戳的镜像。也就是说在这之前，"同一个哈希"只能回答"是不是同一个文件"，
回答不了"是不是同一次构建"，而这恰恰是 `artifact_hashes.py` 和 `rhash.exe` 存在的理由。
加上 `-Wl,--no-insert-timestamp` 后，两个目录编出的 `.ime` **逐字节相同**（`134f7fd4…`，948,481 B），
并写了断言钉住（删掉即红）。ReactOS 不读这个字段，版本资源也不读。

**新增的 5 条断言（前 4 条按归属双向钉）**：

| 断言 | 双向钉法 |
|---|---|
| 每个平台 TU 只属于一个库 | 7 个宿主 TU 必须在 `imfw-win32-imm32` 块**且不在** `imfw-win32-ui` 块；5 个窗口 TU 反之。先剥掉 CMake 注释，否则注释里提到的路径会让"只是被提及"的文件也算归属 |
| `ime/` 只装配 | `ime/CMakeLists.txt` 里不许出现 `../framework/platform/win32/`，且必须同时链接两个库 |
| `.def` 能把档案成员拉进来 | `--whole-archive` 必须紧贴 `imfw-win32-imm32`，且必须有配对的 `--no-whole-archive` |
| trace 宏在读取它的库之前声明 | 顶层 `CMakeLists.txt` 必须有 `option(ROSPINYIN_TRACE`，且 `ime/CMakeLists.txt` 必须没有 |
| `.ime` 可复现链接 | `ime/CMakeLists.txt` 必须有 `--no-insert-timestamp`（没有它，两个目录编出的同一份源码 sha256 不同） |

**顺带修了一处文档漂移**：HANDOVER §4 的目录树里 `framework/host/` 与 `framework/ui/` 还是 X1 之前的布局
（真实路径是 `framework/platform/win32/{imm32,ui}/`，从 X1 起就是）。X1 改了代码与 `check_m1` 的 15 处路径，
漏了这棵树。

---

### 11.31 X2-e2 数据源归位：`load_system_table()` 缺席现在是链接错误（2026-11-19 15:1x）

**这一步为什么值得单独做。** 从 M2 起，`pinyin/host/dat_source_win.cpp` 只是 `ime/CMakeLists.txt`
那张源码清单里的一行。也就是说"拼音数据来自 system32 的 `.dat`"这件事，只由装配层的构建文件保证；
而 `pinyin/src/dat_source.cpp` 里始终存在一个 weak 的 `load_system_table()` 兜底。两者一起构成了
一个静默失败：把那一行删掉，链接照常成功，产物照样能装能用，只是候选变少、多音字消失。

先量了一遍再动手：**改动之前**把 `../pinyin/host/dat_source_win.cpp` 从 `ime/CMakeLists.txt` 删掉，
`cmake --build` **退出码 0**，产物 **942,989 B**（正常是 948,481 B）。这 5.5 KB 是两个世界之间唯一的
证据，而没有任何东西会去读它。审计 A-P1-5 说的"插件自洽"就是这件事。

**做法（两半，缺一半都没用）**

| 位置 | 改动 |
|---|---|
| `pinyin/CMakeLists.txt` | `if(WIN32)` 里 `target_sources(rospinyin-engine PRIVATE host/dat_source_win.cpp)`，并 `target_compile_definitions(... ROSPINYIN_REQUIRE_SYSTEM_TABLE)` |
| `pinyin/src/dat_source.cpp` | weak 兜底包进 `#if !defined(ROSPINYIN_REQUIRE_SYSTEM_TABLE)`；`load_pinyin_table()` 留在守卫外 |
| `ime/CMakeLists.txt` | 删掉 `../pinyin/host/dat_source_win.cpp`、X2-e1 留下的那条临时 include 路径，以及 `if(ROSPINYIN_TRACE) target_compile_definitions(rospinyin-ime …)`——该目标里已经没有读这个宏的 TU |

**判据：同一次删除，现在必须链接失败**（保留宏）：

```
ld: ../pinyin/librospinyin-engine.a(dat_source.cpp.obj):dat_source.cpp:(.text+0x68e):
    undefined reference to `rospinyin::load_system_table()'
make[3]: *** [ime/CMakeFiles/rospinyin-ime.dir/build.make:120: ime/rospinyin.ime] 错误 1
```

再把宏也去掉，链接又成功（942,989 B）——那就是改动前的世界，也是"为什么非要有这个宏"的证据。

**对外面**：16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序仍**完全一致**
（GDI32 12 / IMM32 2 / KERNEL32 59 / msvcrt 43 / USER32 22）；函数符号集合两边各 1386 个，
逐个相同，没有多也没有少。i686 拼音 948,481 → **947,820 B**（−661：`.text` −64、`.rdata` +32，
其余是节对齐累积，不是代码增减）；trace 版 971,982 → 971,833 B（`dat …` 行仍在，
`strings` 里 `dat MISSING` / `dat TRUNCATED` 仍命中）；玩具版 876,352 B 不变（玩具不链接 `rospinyin-engine`）。

**一处跨层依赖没还，只是跟着文件搬了家**：`dat_source_win.cpp` 仍 `#include "trace_stamp.h"`，
所以 `rospinyin-engine` 现在带着一条指向 `framework/platform/win32/imm32` 的 include 路径（Windows 分支）。
这是 X2-e6 的活，CMake 注释里写明了。把它留在 `ime/` 等于同一个洞换个位置，不如跟着文件走、记在正在做的那一格上。

**新增 4 条断言（83 → 87）**，都按"匹配调用、不匹配词"的规矩写：

| 断言 | 钉法 |
|---|---|
| 数据源属于 engine | `target_sources(rospinyin-engine PRIVATE host/dat_source_win.cpp)` 这个**调用**必须在 `pinyin/CMakeLists.txt`；`ime/CMakeLists.txt`（剥注释）不许出现 `dat_source_win` |
| Windows 必须有真实现 | `target_compile_definitions(rospinyin-engine PRIVATE ROSPINYIN_REQUIRE_SYSTEM_TABLE)` 必须存在（允许跨行） |
| 守卫只包住兜底 | `#if !defined(…)` 与 `#endif` 之间必须有 `load_system_table`、不许有 `load_pinyin_table` |
| 装配目标不再声明 trace | `ime/CMakeLists.txt`（剥注释）不许出现 `ROSPINYIN_TRACE` |

变异 4 次全红：文件挪回 `ime/` 的清单 / REQUIRE 宏改成注释（词还在、调用没了）/ 去掉守卫 /
把 trace define 加回装配目标。

门禁：`ctest` 16/16、`check_m1` **87/87**、`check_docs` 7/7。

---

### 11.32 X2-e3 构建配置收敛：presets 一处定义，缺目录不再是静默成功（2026-11-19 16:4x）

审计 A-P1-6 列了五件事，它们的共同点不是"构建文件难看"，而是**同一个事实被多处复述**。
复述就会漂移，而漂移在这个项目里已经有过样本：`build-win` 的测试是 OFF，
`build-win-trace-en` 是 ON，两个目录并排放着，没人知道哪个才是"对的构建"。

| 事项 | 之前 | 现在 |
|---|---|---|
| 配置定义 | CI 的 YAML 里 4 段 `-D` 列表，本地靠 shell 历史 | `CMakePresets.json` 6 个 configure preset + 6 个 build preset + 1 个 test preset，CI 只写 `cmake --preset win32-mingw` |
| 缺目录 | 5 处 `EXISTS` 守卫：目录不在就静默少编一块 | 裸 `add_subdirectory`，缺目录是 configure 失败 |
| 根变量 | `tests/` 用 `CMAKE_SOURCE_DIR`，`ime/`、`pinyin/` 用 `PROJECT_SOURCE_DIR` | 一律 `PROJECT_SOURCE_DIR` |
| C 语言等级 | 只钉 C++17，C 跟着编译器默认走（GCC 11 是 gnu17，更早是 gnu11） | `CMAKE_C_STANDARD 11` + `REQUIRED` + `EXTENSIONS OFF`（实测 `-std=c11`） |
| 静态运行时 | 4 个文件各写一遍，`httpdl` 还只写了一半 | `cmake/StaticRuntime.cmake` 两个函数：`rospinyin_static_runtime()` / `rospinyin_reproducible_pe()` |

**为什么 `EXISTS` 必须去掉，而不是"改成警告"。** 它的形状与 X2-e2 那个洞完全一样：X2-e2 里，
数据源没编进来时 weak 兜底让链接成功，产物 942,989 B（正常 948,481 B），装上去只是候选变少；
这里，`tools/rhash` 目录没了，configure 成功、产物少一个 `rhash.exe`，而 CI 的
`find ... -name rhash.exe` 会拿到空字符串——`test -n "$RHASH"` 恰好会红，但那是**另一条**断言
替它挡的，不是构建本身说的话。构建该说的是"这个目录必须存在"，`add_subdirectory` 天生会说这句。

**静态策略收拢后顺手补了一个漏洞**：`--no-insert-timestamp` 之前只给了 `.ime`。
`install-rospinyin.exe`、`rhash.exe`、`wget.exe`/`curl.exe` 仍带链接时间戳，
于是"同一个哈希 = 同一次构建"这句话对四个工具都不成立——而 `rhash.exe` 正是 guest 侧
用来回答这个问题的工具，它自己却不可复现。现在四个 PE 都拿到了。
实测：两个不同构建目录的 `rhash.exe`、`wget.exe` 各自 sha256 相同；与 X2-e2 那版比，
`rhash.exe` 只差 **5 个字节**（`TimeDateStamp` + `Checksum` + 镜像字节）。

**presets 落地时的一个真发现：可复现性不跨生成器。**
`win32-*` preset 用 Ninja，而本机旧的 `build-x2a1-pinyin` 是 Unix Makefiles。
两份产物**同为 947,820 B，sha256 不同**（`b0529131…` vs `93ab887d…`），差异 14,567 字节、散布 122 个 4 KB 块。
原因是生成器决定链接命令行：Makefiles 先把目标文件打成 `objects.a` 再 whole-archive，
Ninja 直接列 `.obj`。代码一模一样，布局不一样。
**这不是缺陷，是边界**：把生成器写进 preset 正是为了让"同一份源码 → 同一个哈希"有确定的含义。
两个 Ninja 目录实测逐字节相同（`b0529131…`），这条才成立。

**判据**（对外面一律不看字节）：
- 16 导出名及顺序、5 个 DLL、每 DLL 导入函数集逐条同序，与 X2-e2 产物**完全一致**；
- 四个 PE 的依赖面：`.ime` GDI32/IMM32/KERNEL32/msvcrt/USER32；`install-rospinyin.exe`
  ADVAPI32/IMM32/KERNEL32/msvcrt；`rhash.exe` KERNEL32/msvcrt；`wget.exe` KERNEL32/msvcrt/WS2_32。
  无 `vcruntime140` / `api-ms-win-crt` / `libstdc++-6` / `libgcc_s` / `libwinpthread`；
- 三种形态构建通过：toy 876,352 B（不变）、trace 971,833 B（`dat MISSING`/`dat TRUNCATED` 仍命中）、
  in-tree 947,820 B 且 **不产** `install-rospinyin.exe` / `rhash.exe`（只留 httpdl + mkdat）；
- `ctest` 16/16、`check_m1` 87 → **94**（+7）、`check_docs` 7/7。

**新增 7 条断言**（全部按"匹配调用"写，且 7 次变异逐个变红）：

| 断言 | 钉法 | 变异（都红） |
|---|---|---|
| 没有 `EXISTS` 守卫 | 根 `CMakeLists.txt` 剥注释后不含 `EXISTS` | 加回 `EXISTS` 守卫 |
| C 语言等级显式 | 三行 `CMAKE_C_STANDARD` 必须在根文件 | 删掉三行 |
| 根变量统一 | 扫全部 `CMakeLists.txt`，`${CMAKE_SOURCE_DIR}` 不许出现 | `tests/` 改回一处 |
| 策略中枢存在 | `cmake/StaticRuntime.cmake` 定义两个函数 + 根文件 `include()` 它 | — |
| 每个 PE 都调用两条策略 | 5 个 (文件, 目标) 对逐个匹配 `rospinyin_static_runtime(<t>` 与 `rospinyin_reproducible_pe(<t>` | 删 `ros-curl` 的可复现调用 |
| 不许复述 flag | 上述文件剥注释后不许出现 `-static-libstdc++` / `-static-libgcc` / `--no-insert-timestamp` | 在 `ime/` 写回字面 flag |
| presets + CI 走 preset | JSON 必含 `linux-ci` / `win32-mingw` / `win32-trace`，且 YAML 里有 `cmake --preset linux-ci`、`cmake --preset win32-mingw` | preset 改名 / CI 去掉 `--preset` |

顺带把 `.cmake` 加进 `check_m1` 的 SPDX 扫描范围——新文件 `cmake/StaticRuntime.cmake` 因此
和其他源文件受同一条规则约束。

---

### 11.33 X2-e4 平台补丁归位：生产代码不再猜 Win32 常量（2026-11-19 17:0x）

审计 A-P2-11 点名的是 `ui_window.cpp` 的 28 处和 `ime_ui.cpp` 的 2 处。实际扫下来是
**47 处、5 个 TU**：`ui_window.cpp` 28、`toolbar.cpp` 9、`candidate_window.cpp` 5、
`composition_window.cpp` 3、`ime_ui.cpp` 2。只清理被点名的 30 处不叫"生产代码不再猜常量值"，
所以五个文件一起清。

这些块全部是同一个形状：

```cpp
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
```

它们属于 `winuser.h` / `wingdi.h`，真实 MinGW 构建里 `<windows.h>` 早就定义好了，
`#ifndef` 一支**从未生效**——存在的唯一理由是让 Linux 上那个瘦 `windows.h` stub 能通过
`-fsyntax-only`。这是 stub 的责任，不是产品的责任。

| 项 | 处理 |
|---|---|
| 生产侧 | 5 个 UI TU 删掉全部 47 块；普通 include guard（`trace_stamp.h`、`version.h` 等）不是 SDK 补丁，保留 |
| stub 侧 | `tools/syntax_stub/windows.h` 补 25 个宏；刷子、`SM_*`、`SPI_GETWORKAREA`、`HWND_TOPMOST` 已有，不重复定义。stub 的值仍是 illustrative，真正的值只由真实 SDK 背书 |
| 断言 | 3 条（94 → 97）：生产 TU 不得出现"常量被自己的 `#ifndef` 包住再 `#define`"这个结构（含 `#if !defined(...)` 变体）；UI 代码用到的 SDK 标识符必须在 stub 的 `#define` / `using` / `struct` 表面里；CI 的 syntax check 必须真的包含 stub 与五个 UI TU |
| 变异 | 3 次全红：恢复一处 `WS_POPUP` 补丁 / 删掉 stub 的 `TRANSPARENT` / 把 `toolbar.cpp` 从 CI 命令里改成不存在的名字 |
| 门禁 | `ctest` 16/16、`check_m1` **97/97**、`check_docs` 7/7；宿主 syntax check 17 个 TU 全过 |

**对外面零改动，而且是可证的**：交叉构建前后 `rospinyin.ime` 的 sha256 都是
`b0529131b605...`（947,820 B）——因为这些补丁在真实构建里从未生效。这比"导出面一致"
更强：连一个字节都没动。

---

### 11.34 X2-e5 版本单一真值：CMake 不再说谎（2026-11-19 17:2x）

G7 的定义是"仓库内发版版本号只出现在 `ime/version.h`；其余位置出现即 CI 失败"。
改动前的实测反例：`CMakeLists.txt` 写着 `project(VERSION 0.0.1)`，而 `.ime` 资源来自
`version.h` 的 `0,0,2` / `alpha0.0.2`。`PROJECT_VERSION` 全仓零消费者，所以这个假真值
不报错、不参与产物，只会在某天给产物命名时把 0.0.2 叫成 0.0.1。

| 项 | 处理 |
|---|---|
| CMake | `project()` 之前 `file(READ ime/version.h)`，正则读 `MAJOR/MINOR/PATCH` 与 `VERSION_STRING`；`PROJECT_VERSION` 现在是 **0.0.2**。字符串与数字不一致直接 `FATAL_ERROR`，不静默回退 |
| 资源 | `ime/rospinyin_res.rc` 一行未改：它本来就 `#include "version.h"` 并用 `ROSPINYIN_VERSION_COMMA` / `_STRING` 展开 |
| 断言 | 3 条（97 → 100）：CMake 派生真值且字符串与数字一致 / 代码与构建文件中不许再出现 `alphaN.N.N` 或裸 `FILEVERSION` 元组 / `.rc` 必须继续展开宏 |
| 扫描白名单 | `ime/version.h`（唯一真值）、`tools/check_release_version.py` 与 `tools/make_release.py`（两个天生知道 tag 的发版脚本，里面的字面量是示例）。Markdown 历史叙述不扫——那里引用旧版本是事实，不是真值 |
| 变异 | 4 次全红：`project(VERSION 0.0.1)` 写回 / 在无关的 `artifact_hashes.py` 写 `alpha0.0.2` / `.rc` 的 `FILEVERSION` 写成裸数字 / `version.h` 的字符串改成 `alpha0.0.3` 而数字不动 |
| 门禁 | `ctest` 16/16、`check_m1` **100/100**、`check_docs` 7/7；`check_release_version.py alpha0.0.2 --binary …` 全过 |

**对外面零改动**：交叉构建前后 sha256 仍为 `b0529131…`。版本资源解析出的
`FILEVERSION=0.0.2.0`、字符串 `alpha0.0.2`，与 tag 完全一致；改的是"谁有资格说版本号"，
不是版本号本身。

---

### 11.35 X2-e6 跨层 include 归零：时间戳格式器成为 imfw 公共服务（2026-11-19 17:4x）

X2-e2 把 `dat_source_win.cpp` 搬进 `rospinyin-engine`，但文件里还留着
`#include "trace_stamp.h"`——那是一个位于 `framework/platform/win32/imm32/` 的宿主私有头。
于是 `pinyin/CMakeLists.txt` 必须给 engine 加一条指向宿主层的 include 路径。
**文件搬家了，债只是跟着搬了家**（当时已如实记在 §11.31）。

X2-e6 不复制实现：`trace_stamp.h` 整份升格为 `framework/include/imfw/trace_stamp.hpp`
（header-only、inline，`kTracePath` 与 `put_uint` 仍只有一份）。engine 通过
`rospinyin-engine PUBLIC imfw-engine` 的 include 路径拿到它，宿主两个调用者也改用同一路径。

| 项 | 处理 |
|---|---|
| 文件 | `git mv` 到公共 include；普通 include guard 改名，注释说明"为什么是公共服务"；私有路径不再存在 |
| CMake | 删掉 `pinyin/CMakeLists.txt` 里指向 `framework/platform/win32/imm32` 的 `target_include_directories` |
| 断言 | 4 条（100 → 104）：数据源包含公共头且不含旧头 / engine 不带平台 include 路径 / 宿主两个调用者仍包含公共头 / 私有头不复活且唯一格式器在公共头里。X2-d 的"单一十进制格式化器"断言同步改指向公共头 |
| 变异 | 3 次红（改回旧 include / 删 dllmain 的 include / 重建私有头）+ 1 次控制：把 include 路径写成注释仍绿——证明断言匹配的是调用，不是词 |
| 门禁 | `ctest` 16/16、`check_m1` **104/104**、`check_docs` 7/7；17 个 TU 的宿主语法检查全过 |

**对外面仍零改动**：交叉构建前后 `.ime` sha256 都是 `b0529131…`（947,820 B），
依赖面仍是 GDI32 / IMM32 / KERNEL32 / msvcrt / USER32。trace 版 971,833 B，
`dat MISSING` / `dat TRUNCATED` / `dat OK` 三条诊断串仍在——搬的是头的归属，
不是 trace 的行为。公共头里的 `<windows.h>` 只在 `ROSPINYIN_TRACE` 分支内出现，
Linux 默认构建看不到它。

---

### 11.36 X2-w1 合入：工具栏显示 `拼:开` / `拼:关`（2026-11-19 17:5x）

分支 `wip/status-indicator` 落后于 X2-c/d/e 的 main，cherry-pick 撞了 3 处：

| 冲突 | 处理 |
|---|---|
| `framework/README.md` 模块地图 | 合并两边：保留 X2-c/d/e 的 `intent.hpp` / `key_route` / 两个平台库 / `trace_stamp.hpp`，补上 `toolbar_text` 与 `拼:开`/`拼:关` 描述 |
| `imm_entry.cpp` 两条激活路径 | **保留 X2-d 的 `activate_session()`**，不回退成分支里两条重复代码；但把首次 `UiToolbarShow` 改成带 `fOpen` 的新签名——否则第一次绘制会显示旧静态标签 |
| `toolbar.cpp` 宏补丁 | 删除分支里那 5 处 `#ifndef`；X2-e4 已把全部 SDK 常量归到 syntax_stub，产品代码不再猜值 |

键路径上的两处 `UiToolbarSetOpen()`（每条消息一次、Shift 手势后立刻一次）随 cherry-pick
自动合入，计数断言仍要求两处都在——`Ctrl+Space` 由 imm32 直接改 `fOpen`，不进我们的 DLL，
所以只能"拉"不能"推"。`ActivateKeyboardLayout` / `LoadKeyboardLayout` 在框架、拼音和装配层
仍为 0 处：托盘红底「中」跟布局走，必须不动。

| 项 | 结果 |
|---|---|
| 门禁 | `ctest` **17/17**（新增 `toolbar-text`）、`check_m1` 104 → **107/107**、`check_docs` 7/7；17 个 TU 宿主语法检查全过 |
| 变异 | 3 次全红：标题不带 `fOpen`（写死 `L"拼"`）/ 删掉一处键路径同步 / 在某 TU 插入 `ActivateKeyboardLayout` |
| 对外面 | 16 导出名、5 个 DLL、每 DLL 导入函数集逐条同序**完全一致**（GDI32 12 / IMM32 2 / KERNEL32 59 / msvcrt 43 / USER32 22）。行为变了，字节必然变：`b0529131…` → `0ac616f1…`，947,820 → **951,461 B**；`compare_ime.py` 判 NOT equivalent 是预期的，不是回归 |
| 真机 | **待用户操作**：同一屏里按 Shift，工具栏应 `拼:开` → `拼:关`，托盘红底「中」不动；不要代敲键鼠 |

---

### 11.37 X2-w2 合入：候选窗不再每跨音节闪一次（2026-11-19 18:0x）

根因在 `PinyinSession::recompute()`：它要求整串 composition 可被 `segment_syllables()` 切分。
敲到 `nih`（`[ni]` + 未完成的 `h`）或 `zhongg` 时整串切不开 → `syllables_` 清空 →
候选归零 → `CandidateWindow::update` 命中 `pager.empty()` 后 `hide()`。于是每个音节边界
都闪一次。

修法是新增 `completed_syllables()`，只把**已完成**的音节交给候选：整串可切分就原样返回；
末尾撇号单独处理（`xi'` 仍给 `[xi]`）；否则取"最长可完整切分的头 + 尾部仍是合法前缀"。
未完成的尾巴不进入 `segments`，所以组合窗依旧把它画成灰色 pending。

**这是产品决策变更**：旧的 `tests/pinyin_session_test.cpp` 有一条
`trailing separator has no candidates` 把闪一下钉成"正确行为"。本次连断言一起改成
"尾随分隔符保留已完成音节与候选"，并增加 `zhongguo` 全程无空档、`zhongg` 保留「中」、
上屏后尾巴 `g` 留下三组判据。不说明这一点，看起来就像"测试改了才过"。

| 项 | 结果 |
|---|---|
| 量化 | `nihao` 逐键候选数 `0,4,0,4,4` → **`0,4,4,4,4`**；`zhongguo` 在 `zhongg` / `zhonggu` 仍保留 2 条候选；`xi'` 保留「西」 |
| 门禁 | `ctest` **17/17**、`check_m1` 107/107、`check_docs` 7/7 |
| 变异 | 3 次全红：退回 `segment_syllables()` / 去掉撇号分支 / 去掉最长头搜索 |
| 对外面 | 16 导出与 5 个 DLL 的导入函数集逐条同序不变；行为变了，`.ime` 951,461 → **952,485 B**，sha `0ac616f1…` → `1ced1364…` |
| 真机 | **待用户操作**：连续输入 `nihao` 时候选列表不再消失再出现 |

§11.26 曾写"将来改候选窗稳定性时，`host_smoke` 那行『退格到空仍 show、候选 0』日志会变"，
这次实测**没有变**：`host_smoke` 注入的是假 engine，不经过 `PinyinSession`。那条日志仍然有效，
但不再是 w2 的判据；判据是上面的 `pinyin-session` 测试与逐键候选数。

---

### 11.38 X2-w3 合入：打字不再把翻页踢回第一页（2026-11-19 18:2x）

`handle_key` 的 `Input/Absorb` 分支每个字母都调用 `pager.reset(candidates)`，而 `reset()`
无条件 `page_ = 0`。于是翻到第 2 页后再敲一个字母，页码立刻回到 `1/N`。

修法是区分两种"换候选"：

| 调用 | 语义 | 场景 |
|---|---|---|
| `reset()` | 新会话，回到第 1 页 | `finish_composition`（Esc / 关 IME）、提交后收尾、Backspace |
| `replace()`（新） | 同一会话的候选刷新，保留当前页并 clamp | 继续打字（`Input/Absorb`） |

clamp 是必要的：新候选列表可能更短，留一个越界 `page_` 会让 `page_begin()` 指到无效位置。
`replace()` 在 `page_count()==0` 时归零，否则把超出范围的页码压到最后一页。

| 项 | 结果 |
|---|---|
| 判据 | `candidate_pager_test` 新增保留页 / 短列表 clamp / 空列表三组；`host_smoke` 新增一段：20 候选 → `=` 翻到第 2 页 → 再敲两个字母仍显示 `page=2/3` → 候选缩到 5 条时显示 `page=1/1`，且翻页键没有调用 engine |
| 门禁 | `ctest` 17/17、`check_m1` 107 → **108/108**、`check_docs` 7/7 |
| 变异 | 3 次全红：打字路径改回 `reset()` / `replace()` 无条件归零 / 去掉 clamp |
| 对外面 | 16 导出与 5 个 DLL 的导入函数集逐条同序不变；`.ime` 952,485 → **953,082 B**，sha `1ced1364…` → `f3e33b0a…` |
| 真机 | **待用户操作**：候选多于 9 条时翻到第 2 页，继续打字看页码是否保持 |

三条 X2-w 临时需求至此全部合入；它们的共同点是"测试能证明序列，证明不了观感"，
所以三条的完成判据都保留一行**真机待验**，不由构建绿灯代替。

---

### 11.39 X2-f 收口：seam 清单变成可执行的耦合上限（2026-11-19 18:0x）

X2 的最后一项不是抽接口，而是**把"将来换前端要改多少地方"变成数字**。
计划 §4 的机制 M2 一直写着"seam 清单 + 上限断言"，X2-f 把它落地：

```bash
python3 tools/seams.py            # 打印五条 seam 的调用点计数与上限
python3 tools/check_m1.py         # import 同一个 over_cap()，越界即失败
```

| seam | 统计的调用形式 | 上限/当前 |
|---|---|---|
| `engine` | `state.engine->` / `session.engine->` | 4 / 4 |
| `scheme-policy` | `scheme.` / `scheme_instance()`（`key_route.cpp` + `host_key.cpp`） | 12 / 12 |
| `ui-exit` | `Ui...(`（`imm_entry.cpp` + `win32_host.cpp`） | 12 / 12 |
| `imm32-abi` | `WINAPI Ime...(` / `WINAPI NotifyIME(` | 16 / 16 |
| `can-render` | `.can_render(` / `UiCanRender(` | 2 / 2 |

`strip_comments()` 先删 `/* */` 与 `//`：注释里写过的路径、旧接口名、审计引用都不算调用点，
否则"文档说了某函数"也会被算成耦合（与 X2-a 那次词匹配假绿同类错误）。

`check_m1.py` 用 `importlib` 动态加载 `seams.py`，只调用 `over_cap(root)`；
门禁里**不复制第二份正则**，否则清单漂移时两份会一起错。断言放在 `tools/check_m1.py`，
失败信息直接列出 `name=count>cap`。

| 项 | 结果 |
|---|---|
| 计数 | 五条全部等于上限，说明当前耦合量已冻结，后续只能减不能悄悄涨 |
| 变异 | 5 次全红：逐条改一个 TU 制造越界，断言每次都点名对应 seam |
| 门禁 | `ctest` 17/17、`check_m1` 108 → **109/109**、`check_docs` 7/7 |
| 对外面 | 零运行时改动：16 导出、5 个 DLL 导入函数集、`.ime` 字节均不变 |

这里**故意没有抽新接口**：五条 seam 现在都很小（最大 16 处且是 ABI 入口，本来就不能少），
为一个还不存在的第二前端冻结 ABI 只会增加维护面。真正触发抽象的条件写在计划 §4 的 X4：
出现第二个前端消费者、`imm_entry.cpp` 超过 1,200 行、或 csrss 崩溃成为实际痛感。
X2-f 交付的是**触发时的数据**，不是提前交出的接口。

---

### 11.40 X2 总验收：构建、门禁、对外面与交付边界（2026-11-19 18:1x）

X2 全部子项（a0/a/b/c/d/e1–e6/f/w1–w3）均已合入 `main`，PLAN 清单无未勾选项。
收口时在同一棵工作树上重跑：

| 项 | 结果 |
|---|---|
| 门禁 | `ctest --preset linux-ci` **17/17**；`python3 tools/check_m1.py` **109/109**；`python3 tools/check_docs.py` **7/7**；`git diff --check` 通过 |
| presets | `cmake --preset linux-ci` / `cmake --build --preset win32-cross --parallel` 均通过。`win32-cross` 须先 `export ROSPINYIN_MINGW_TOOLCHAIN=/path/to/toolchain-i686.cmake`（见 §6.3）；两个独立目录 `build-cross` 与 `build-final-verify` 的 `.ime` 逐字节相同 |
| 产物 | 当前 `.ime` **953,082 B**，sha256 `f3e33b0ac218…e787`；与 w3 合入时一致，X2-f 未改运行时代码 |
| 对外面 | 16 个导出名及顺序与 X2-e3 基线逐项一致；5 个导入 DLL 及函数集逐条同序一致：GDI32 12 / IMM32 2 / KERNEL32 59 / msvcrt 43 / USER32 22。行为变了但 ABI 面没变 |
| 文档 | CHANGELOG 有 X2-e3/e4/e5/e6、X2-w1/w2/w3、X2-f 与本节；HANDOVER §11.32–11.40 有对应执行记录；PLAN §3 与清单全部勾选 |

**未由本次总验收代替的事项**：w1（Shift 后 `拼:开` ↔ `拼:关`、托盘「中」不动）、
w2（连续输入 `nihao` 候选窗不闪）、w3（翻到第 2 页后继续打字页码保持）三项的真机观感。
测试能证明调用序列与页码状态，证明不了"看起来对"；按红线不代用户发键鼠事件，
这三项仍以用户在 ReactOS VM 中的同框确认作准。

---

### 11.2 其他待办

- ~~**`'` 硬边界在交互输入路径失效（2026-11-17 接手加固时发现）**~~ **已修复（2026-11-17）**：`segment_syllables()` 本就把 `'` 当硬边界，但 `PinyinSession::feed()` 收到 `'` 后走 `can_extend()`，而 `CodeTable` 的前缀集合里没有 `'` → 分隔符被丢弃，敲 `xi'an` 得不到 `[xi][an]`。修法：`can_extend()` 显式处理撇号——引号前须为完整音节、引号后须仍可扩展（空串合法，即刚敲完分隔符的中间态），与 `segment_syllables()` 语义一致；并补 `feed("xi'an")` 保留分隔符与 `[xi][an]` 分段断言、尾随分隔符断言、分隔符后非法前缀拒收断言（`pinyin_session_test.cpp`）。
- ~~M3：中文标点、全角/半角~~：中文标点与 Shift 已做（§11.14）；**全角/半角独立开关待确认**，100 字段落实机验收待做；
- ~~**M4 部署链路**~~ **宿主侧已闭环（2026-11-26）**：词库内嵌 GUI/CLI 安装器（第 4 RCDATA）、system32 固定 SHA-256 校验、四类失败拒绝加载、CI release 含词库与 SHA256SUMS；宿主 ctest 24/24、check_m1 120/120。**6080、6081 实机已由 owner 截图验收闭环，仅 6082 stable 对照待补**，见 §3.3 与 `.devloop/`；
- **M5 前置已满足（6080/6081 实机闭环），可开工**：自写 Viterbi 整句解码 + 用户词典；整句开关为配置持久（非会话级），见 §3.2；
- 语言栏菜单项（属性/关于）留待 M6；
- `xingma/` W1 编码（`ShapeScheme`/`ShapeStore`/`ShapeDatLoader`）尚未开始，框架侧缺口已全部关闭，可直接复用宿主层；
- 历史遗留空目录 `engine/include/` 已于 2026-11-17 清理（实体已并入 `framework/`）。

---

## 12. 维护者日常清单

**改代码**

1. `cmake --build build --parallel && ctest --test-dir build` → **24/24**（`build-host-off` 同为 24/24）；
2. `python3 tools/check_m1.py --root .` → **120/120**（名字沿用 M1，实际是**长期契约闸门**，含 M2/M4 不变量）；
3. `python3 tools/check_docs.py .` → **7/7**（改文档必跑）；
4. 动了 `framework/platform/win32/imm32` 或 `framework/platform/win32/ui`：跑 CI 那步宿主语法检查（§6.3），确认 `-Wchanges-meaning` / ABI 断言不炸；
5. 动了 ABI：本地用 zig 真实 MinGW 头编译一次 `framework/platform/win32/imm32/imm_entry.cpp`（`-target x86-windows-gnu -c`）。

**加一个输入法（如形码）**

1. 实现 `imfw::Scheme` + `imfw::Engine`（参考 `pinyin/src/scheme.cpp`、`pinyin_session.cpp`）；
2. 装配 `scheme_reg.cpp` 定义 `scheme_instance()`；
3. 窗口类名、工具条标签、`.def`、资源、CMake 目标仿 `ime/`；
4. 数据自管（框架不传表），并在 `reload_data()` 里实现换代。

**发版/验收**

1. **发版前一个提交**：写 `docs/releases/<tag>.md`，同时把 `ime/version.h` 的四个数字与
   `ROSPINYIN_VERSION_STRING` 一起 bump。`python3 tools/check_release_version.py <tag>`
   必须全绿——它同时校验发布说明是否存在，所以"先写说明再打 tag"是被强制的，不是靠记性。
2. **打 tag 并 push 那个 tag**：CI 的 `release` job 用**本次运行的产物**建 release
   （`.ime` / `.dat` / `install-rospinyin.exe` / `install-rospinyin-gui.exe` / `rhash.exe` / `INFO.txt` / `SHA256SUMS.txt`），
   再把资产下载回来逐个 `sha256sum -c`。手工上传已不是路径；
   `tools/make_release.py` 只在 CI 不可用时兜底（它同时负责打 tag）。
   `alpha*`/`beta*` 为 prerelease，`v*` 为正式版；tag 已存在时需删除旧 tag ref 再重建才能重跑 CI。
3. VM 拉取产物 → 重启 → 跑 `docs/milestones/m2/M2-ACCEPTANCE.md`；
4. 更新 `M2-REPORT.md`（若新建）与 `docs/plan/PLAN.md`/`docs/plan/ROADMAP.md` 状态。

#### 推送通道（2026-11-17 实测，务必按此执行）

本机 `~/.gitconfig` 有一条 `url.https://gh-proxy.com/https://github.com/.insteadOf`，
它会把**任何** `https://github.com/...` 改写成走 gh-proxy。这条改写对 fetch 无害，
但 **push 会静默失败**：请求发给 proxy 后丢了认证头，代理返回 `401 Basic realm="GitHub"`，
git 转而问 credential-store，然后长时间无输出（实测 4 分钟以上不返回，`timeout` 掉之后
远程 ref 毫无变化——看起来像"推了但没生效"）。

绕过方式是一次性屏蔽全局配置并直连（需要 fine-grained PAT，`Contents: read and write`）：

```bash
AUTH=$(printf "x-access-token:%s" "$(cat ~/mygithubpak/key.txt)" | base64 -w0)
CFG=$(mktemp); chmod 600 "$CFG"
printf '[http "https://github.com/"]\n\textraheader = Authorization: Basic %s\n' "$AUTH" > "$CFG"

GIT_CONFIG_SYSTEM=/dev/null GIT_CONFIG_GLOBAL=/dev/null \
  git -c include.path="$CFG" -c credential.helper= \
  push https://github.com/twipty/ros-chinese-ime.git main
```

- `GIT_CONFIG_GLOBAL=/dev/null` 是**关键**：没有它，`insteadOf` 仍在，仍会走 proxy；
- `credential.helper=` 置空，避免 401 后回退到 credential-store 反复重试；
- 直连走 HTTP/2（本机有 CONNECT 隧道），4.2 MB 的 pack 数秒完成。

诊断手段：`GIT_CURL_VERBOSE=1` 看响应头里的 `web-server:` —— 出现 `gh-proxy.com`
说明改写没被屏蔽掉。

#### 推送通道变更（2026-11-18 12:5x，直连不通了）

上面那条"屏蔽全局配置 + 直连"的路子**当天已失效**：沙箱的 HTTPS 代理对 `github.com`
返回 `CONNECT tunnel failed, response 502`，而 `api.github.com` 与 `gh-proxy.com` 都通。
gh-proxy 的 `info/refs?service=git-receive-pack` 返回 `401`（说明它认 push），
但带 `http.extraheader` 认证头推过去仍然**静默失败**（5 分钟无输出、remote ref 不变），
和 §12 记录的老坑同形。

可用通道是 **GitHub Git Data API**（`api.github.com` 可达）：逐个提交上传 blob → 建 tree
→ 建 commit → `PATCH refs/heads/main`。脚本在 `tools/push_via_api.py`，关键是它自带安全阀：

- 要求远端 HEAD 等于预期的起点 SHA，否则中止；
- 每个提交建完 tree 后**必须与本地 `git rev-parse <sha>^{tree}` 完全相等**才往下走，
  不等就 `sys.exit`，ref 一动不动（最多留下几个孤立 blob，无害）。

代价：重建出来的 commit SHA 与本地不同（tree 相同、内容一致），本地 `HEAD` 与
`origin/main` 会"看似分叉"。等直连恢复后跑一次 `git push --force origin main` 即可对齐，
历史内容不受影响。

```bash
python3 tools/push_via_api.py <repo> main <expected_remote_sha> <sha1> <sha2> ...
```


#### 远程历史与本地不相关（已知）

2026-11-17 之前远程 `main` 是用 API 逐文件提交堆出来的 93 个提交，与本地 31 个提交
**无共同祖先**（`git merge-base` 为空），但文件树是本地树的子集（远程 113 个文件，
本地 136 个，远程无本地缺失的文件）。发 `alpha0.0.1` 时已把旧历史保全到
`legacy/pre-alpha0.0.1` 分支，再 force push 本地历史覆盖 `main`。
若日后再次遇到"两边历史无关"，先做同样的备份再覆盖，不要 `merge --allow-unrelated-histories`。

**交接/收尾**

- 关闭 QEMU、websockify、http.server（见 §10.5 的 systemd-run 方法），确认 5901/8000/6080 无监听；
- VM 证据归档到 `m0/vm/evidence/`，并更新当日工作日志。

---

## 13. 参考文档索引

| 文档 | 内容 |
|---|---|
| `README.md` | 项目章程：使命、立项依据、路线 A/B 决策、产品形态、安装与字体依赖 |
| `docs/plan/PLAN.md` | WBS 全量任务与工期；§5 M4 状态更正、§6 M5 开关语义更正（2026-11-26） |
| `docs/plan/ROADMAP.md` | 里程碑目标；M5 段含前置依赖与开关语义更正 |
| `docs/releases/v0.1.0.md` | v0.1.0 正式发布说明（安装、使用、字体依赖；已同步线上 release 正文） |
| `docs/reviews/WINDOWS-GUI-COMPATIBILITY-ASSESSMENT.md` | P3 Windows GUI 兼容评估：不登记白名单、CLI 例外保留 |
| `tools/droid-font-pack/README.md` | nightly Droid 字体包：制作、安装、清理与验证范围 |
| `tools/mklex/README.md` | M4 词组库 `rospinyin-lexicon.dat`：构建、许可、运行时行为与内嵌必装部署链（宿主已落地，6080/6081 实机已验，仅 6082 对照待补） |
| `M0-REPORT.md` | ReactOS/QEMU 环境、IMM32 可行性、BSOD 根因、许可证时间线 |
| `M1-REPORT.md` | 16 导出骨架、分步注册、动态 HKL、托盘图标修复、MinGW 切换 |
| `framework/README.md` | 框架模块地图、Scheme 复用缝、G1–G4 处置、生命周期红线 |
| `pinyin/README.md` | 拼音插件文件职责与数据所有权 |
| `docs/milestones/m2/M2-ACCEPTANCE.md` | 15 项真机验收操作单（含判定口径） |
| `docs/milestones/m2/M2-REPORT.md` | M2 报告：结论、交付物、根因修复 10 条、已知限制、门禁 |
| `docs/milestones/m2/M2-REMAINING.md` | M2 剩余事项与优先级（当前只剩真机验收单） |
| `docs/milestones/m2/M2-DATA-PLAN.md` | 数据源选型、声调/`ü` 处理、规模核算 |
| `m0/vm/NOVNC-GUIDE.md` | VM 人机协作操作单（v7，含所有键位坑与图标修复说明） |
| `xingma/XINGMA-PLAN.md` | 形码产品线正式计划（1.0） |
| `xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` | 框架能力审计（G1 ABI P0 / G2 顺序 P2）与处置记录 |
| `NOTICE.txt` | 第三方与数据分层授权 |

---

*本文档由维护者交接时编写，随代码同步更新。若发现与代码不符，以代码与 CI 结果为准，并回来修文档。*
