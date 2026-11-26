# RosPinyin 工作分解结构（WBS）

编号规则：`X.Y.Z`。✓=可独立交付/验证。估时单位：人日（业余节奏可 ×2 换算周数）。

## 1. M0 蓝本移植验证（估 12-18 人日）— ✅ 已完成 2026-11-13，见 M0-REPORT.md

- **1.1 验证环境搭建（QEMU/KVM） ✓**
  - 1.1.1 QEMU 建立 WinXP/2003 VM（IMM32 API 行为参照机，装 SDK 样例与调试工具）
  - 1.1.2 QEMU 建立 ReactOS nightly VM（快照基线：装好 VBEMP 显卡驱动 + 记事本可用）
  - 1.1.3 文件传输通道：Host 起 `python3 -m http.server`，Guest 经 `http://10.0.2.2:8000/` 拉取（QEMU 用户网络 SLIRP 内建，无需共享文件夹）；备用：mkisofs 打 ISO 挂载
  - 1.1.4 调试通道固化：`-serial file:rosdbg.log`（ReactOS 默认 DbgPrint 走 COM1）；内核级问题启用 `-s -S` + GDB（RosBE 自带 gdb，连 ntoskrnl 符号）
  - 1.1.5 记录两机版本号（ReactOS build 号入档，验收基线）；启动脚本 `run-reactos.sh`、`run-winxp.sh` 入库
- **1.2 mzimeja 构建 ✓**
  - 1.2.1 拉取源码，阅读 README/BUILD，确定工具链（MinGW-w64 优先）
  - 1.2.2 Windows 侧或交叉编译产出 mzimeja.ime + 安装工具
  - 1.2.3 处理编译告警与链接错误（预期有，老项目常见）
- **1.3 mzimeja @ Windows 参照机 ✓**
  - 1.3.1 安装（注册表 + ImmInstallIME），记事本验证日语输入全流程
  - 1.3.2 留存基线录屏/截图与注册表导出
- **1.4 mzimeja @ ReactOS ✓**
  - 1.4.1 同法安装到 ReactOS nightly
  - 1.4.2 记事本验证：组合窗、候选窗、选字、上屏
  - 1.4.3 重启持久化验证（注册表 + 文件是否保留）
  - 1.4.4 **产出：ReactOS vs WinXP 行为差异清单**（核心交付物，决定 M1/M2 工作量）
- **1.5 上游状态核查**
  - 1.5.1 复核 CORE-19455/20799 现状（热键）——若已修，M3 减一坑
  - 1.5.2 订阅 CORE-19268，向 Katayama 通报立项意图（获取指导，避免闭门造车）

## 2. M1 骨架替换（估 15-20 人日）— ✅ 已完成 2026-11-16，见 M1-REPORT.md

- **2.1 代码库改造 ✓**
  - 2.1.1 复制 mzimeja → rospinyin，确立目录：`engine/`（纯 C++ 核心）、`ime/`（IMM32 接口层）、`ui/`（窗口）、`tools/`（数据工具）、`tests/`
  - 2.1.2 ✅ 许可证已落定（D1，2026-11-13）：本项目 MIT（`LICENSE`）；蓝本 mzimeja v1.0.0.3 为 PDS（`vendor/LICENSE-NOTICE.txt`）；第三方与数据分层授权见 `NOTICE.txt`；源码文件头统一加 MIT 声明
  - 2.1.3 CI 骨架（GitHub Actions：Windows 构建 + 引擎单测）
  - 2.1.4 构建目标：独立安装包（产品形态决策：以第三方软件包分发；CMake 条件分支仅用于本地构建灵活性）
- **2.2 剥离日语逻辑 ✅ 宿主侧已完成 2026-11-13，见 M1-REPORT.md**
  - 2.2.1 ✅ `ime/` 全量新写（不复制蓝本），16 个 IMM32 导出齐全；假名/罗马字/日语词典零残留，由 `tools/check_m1.py` 自动扫描
  - 2.2.2 ✅ 资源全替换：`ime/rospinyin_res.rc` 中文/英文字符串表、布局文本「中文(简体) - RosPinyin」、IDS 1024 供 Layout Display Name 引用；图标留待 M3（WBS 4.5）
  - 2.2.3 ✅ 版本信息：`FILETYPE=0x3 (VFT_DRV)`、`FILESUBTYPE=0xB (VFT2_DRV_INPUTMETHOD)`、`Translation 0x0804/1200`、`OriginalFilename=rospinyin.ime`，静态校验入 CI
- **2.3 中文注册 ✓**（源码完成，VM 分步验证 2026-11-16 已完成，见 M1-REPORT.md 第二轮更新）
  - 2.3.1 ✅ **由 `ImmInstallIMEW` 动态分配 HKL 并记录返回值**（D3，不硬编码 0xE0100804）；注册表只写 `HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\<HKL>` 的 Layout Text / Layout Display Name；**逐键分步注册 + 每步重启验证**；~~禁止写 Preload 第一项（M0 的 0xF4 红线）~~ → **2026-11-18 降为部署偏好**：写 `Preload` 的是平台 `ImmInstallIMEW` 内部（无法选择），且 N/N′ 两组 6 次启动均活，见 `M0-REDLINE-REVIEW.md` §13；保留的实质约束是「登录路径不做可能失败的初始化」
  - 2.3.2 ✅ 安装工具 `tools/install-rospinyin/install.cpp`：`/step1`…`/step4` / `/i` / `/status` / `/u`
  - 2.3.3 ✅ 验证 kbswitch 出现简体中文项并切换（VM 实测：`Chinese (Simplified) - RosPinyin`，HKL=`e0210804`；托盘「中」字图标 + Alt+Shift 切换均已确认，见 `m0/vm/evidence/m1-2.5/`、`m1-altshift/`）
- **2.4 空输入法直通 ✓**（宿主侧完成，VM 压测 2026-11-16 已完成，见 M1-REPORT.md 第二轮更新）
  - 2.4.1 ✅ `ImeProcessKey` 返回 FALSE 全直通、`ImeToAsciiEx` 零消息、`ImeInquire/ImeSelect/ImeSetActiveContext/NotifyIME` 返回正确；宿主 `g++ -fsyntax-only` 零告警
  - 2.4.2 ✅ 引擎层 50 次 select/deselect 循环单测（`tests/session_test.cpp`）；✅ ReactOS VM 内 50 次激活/停用压测完成、无崩溃无泄漏（证据 `m0/vm/evidence/m1-2.4/`）
  - 2.4.3 ⬜ 双机（XP/ReactOS）行为比对记录 —— **环境所限搁置**：本机 Linux 无 Windows 参照机，且 M0 阶段已决定跳过 XP VM；非技术缺口。若日后具备 Windows 环境再补。

## 3. M2 单字引擎（估 22-30 人日）★核心里程碑 — ✅ 完成（2026-11-18，验收 15/15，`alpha0.0.2` 已发布）

> 准出复核（2026-11-18）：`docs/milestones/m2/M2-ACCEPTANCE.md` 15/15；P0/P1/P2 全绿；
> 唯一量化退出标准首测——**前 500 高频字首页覆盖 96.8%（≥95% 达标）**，由
> `tools/coverage_report.py` 产出（已接进 CI）。同期发现**首选率仅 46.2%**，归给 M4/M5 的排序工作。
> 四条记录缺口（截图未归档、3.9–3.11 无逐页图、版本号漂移已修源码、dat 数字两套）见 `docs/milestones/m2/M2-EXIT-GAP.md`。

> 进展：D2 已拍板 A；共用归一化库、`mkdat` 编译器、`rospinyin.dat` v1 格式、运行时表与单字引擎骨架已落地。2026-11-16 完成 M2 剩余工程部分：候选分页器、`rospinyin.dat` 延迟加载、按 HIMC 会话管理、IMM32 键处理（组合/翻页/数字选字/上屏/回删/Esc）以及组合串窗口与候选窗（`ui/`）；宿主 ctest **7/7**、语法检查通过。实测加载真实 dat 后 `zhongguo` → `[zhong][guo]` / 首选 `国`（U+56FD），`nv` → 女、`lv` → 律、`lve` → 略、`nve` → 虐。ReactOS 记事本验收操作单见 `docs/milestones/m2/M2-ACCEPTANCE.md`，待 CI 产出新 `.ime` 后在 VM 执行（见 `docs/milestones/m2/M2-DATA-PLAN.md` 第 8/9 节）。2026-11-16 变更目标：框架抽取完成（见 3.6），M2 工程部分全部落地，宿主 ctest 8/8。

- **3.1 引擎库 `pinyin-core` ✓**
  - 3.1.1 音节数据结构（415 左右合法音节 + 声调变体）
  - 3.1.2 音节切分器：最长匹配 + 歧义回溯（xian/xi'an、fangan/fan'gan 类）
  - 3.1.3 字候选生成：切分结果 → 同音字集合
  - 3.1.4 频度排序：字频表驱动（决策点 D2）
  - 3.1.5 引擎 API 定型（纯 C++，无平台依赖：`Input → Candidates/Commit`）
  - 3.1.6 单元测试：切分歧义 100+ 用例、常用字覆盖率、排序稳定性
- **3.2 数据工具链 ✓**
  - 3.2.1 下载/解析 Unihan `kMandarin`（含多音字处理：取最常用音，其余作次选）
  - 3.2.2 字频来源整合（**D2 已定 A**：hanziDB.csv MIT 主源 + Unihan kHanyuPinlu 辅源；Jun Da 原始表因商用限制排除；`ü`/`v` 归一化）
  - 3.2.3 编译器 `mkdat`：产出紧凑 `rospinyin.dat`（目标 < 2MB，mmap 友好）
  - 3.2.4 数据校验工具（音节完整性、多音字抽样）
- **3.3 IME UI ✅（2026-11-16 落地）**
  - 3.3.1 组合串窗口：拼音串 + 音节切分下划线、跟随插入符（`GetCaretPos` + `ClientToScreen`）
  - 3.3.2 候选窗：9 候选 + 编号、页码提示、跟随光标、屏幕边缘翻转避让
  - 3.3.3 键位：数字 1-9 选字（经 `CandidatePager::select`，翻页后按当前页解释）、`-`/`=`（或 `,`/`.`）翻页、空格首选上屏
  - 3.3.4 编辑键：退格逐字回删、Esc 取消组合
  - 3.3.5 字体：`Droid Sans Fallback → Noto Sans SC → Microsoft YaHei → SimSun → Arial Unicode MS` 兜底链，用 `GetTextFaceW` 验证 GDI 实际解析结果
- **3.4 IMM32 集成 ✅（2026-11-16 落地，方案调整）**
  - 3.4.1 ImeProcessKey 只判按键归属（Side-effect free）；Ctrl/Alt 不消费，应用快捷键不受影响
  - 3.4.2 ⚠️ 原计划的 `ImmSetCompositionString` / `NI_` 常量路径**改为自绘窗口**：ReactOS 的 IMM32 DDK 候选列表结构不完整，自绘避免依赖；组合串经 `UiShow()` 渲染
  - 3.4.3 ⚠️ 候选窗同样由 `UiShow()` / `UiHide()` 驱动，不经 `ImmNotifyIME`
  - 3.4.4 上屏：生成 `WM_IME_CHAR` TRANSMESSAGE（单字候选均为 BMP）
  - 3.4.5 焦点切换：`ImeSetActiveContext` 激活/隐藏；`ImeSelect(FALSE)` 销毁窗口与会话
  - 3.4.6 会话激活：`ImeSelect(TRUE)` / `ImeSetActiveContext(TRUE)` 必须调用 `on_select(true)`，否则引擎拒绝 `feed()`（已加 contract 断言防回归）
- **3.5 ReactOS 记事本验收 🧪（操作单就绪，待 VM 执行）**
  - 3.5.1 全流程测试操作单：`docs/milestones/m2/M2-ACCEPTANCE.md`（15 项用例）
  - 3.5.2 长时间输入会话（30 分钟）稳定性
  - 3.5.3 **v0.1 预览版发布**（提前收集社区反馈，不等 M3）
- **3.6 框架抽取：引擎宿主 / UI / 工具条可复用化 ✅（2026-11-16 变更目标）**
  - 3.6.1 目录重组：`framework/`（imfw：engine 契约、CodeTable/dat、CandidatePager、IMM32 host、UI 窗口）+ `pinyin/`（拼音插件）+ `ime/`（装配）；`engine/`、`ui/` 消失
  - 3.6.2 Scheme 插件接口（`imfw/scheme.hpp`）：字母表/码表名/窗口类名/工具条标签/引擎工厂/兜底表；`scheme_instance()` 由框架定义，装配模块提供 `scheme_factory()`（X2-a0 注入缝，见 `PLAN-MODULARIZATION.md`）
  - 3.6.3 `Result.segments` 通用切分视图：组合串下划线对拼音/形码通用
  - 3.6.4 通用 `CodeTable`：键名改由 dat 自带 SylEntry 名提供，框架不含拼音知识（check_m1 新增防污染断言 + `pinyin-scheme` 契约测试）
  - 3.6.5 **输入法工具条**（`framework/platform/win32/ui/toolbar.*`）：右下角浮动、Scheme 标签（RosPinyin 显示「拼」）、`ImeSelect` 驱动显隐；M3 再扩展中/英与全/半角
  - 3.6.6 验证：宿主 ctest 8/8；ziglang(x86-windows-gnu) 真实 MinGW 头交叉语法检查 12/12；真实 dat 链路复验（zhongguo→国、nv/lv/lve/nve）通过

## 4. M3 中文体验补齐（估 10-14 人日）

- **4.1 中文标点 ✓**：，。？！：；（「」等映射表；中文标点开/关状态联动；标点也走候选或直接上屏策略定夺
- **4.2 全半角/中英状态 ✓**：状态机补全；各状态记忆与恢复（应用内/全局策略）
- **4.3 Shift 临时英文 ✓**：Shift 按下时组合串直通英文上屏
- **4.4 Ctrl+Space 主热键**：实测 CORE-19455 修复与否；未修则启用备用热键并在文档注明，向 ReactOS 报最小复现
- **4.5 状态窗口与图标 ✓**：状态窗（中/英、全/半角提示）；kbswitch/托盘图标（16x16 正确显示——早期 JIRA 记录过图标显示 bug）
- **4.6 v0.1「墨」正式发布 ✓**：安装包（inf 或脚本）、GitHub Release、README、已知问题清单、CORE-19268 汇报帖

## 5. M4 词组与词频（估 14-18 人日）

- **5.1 CC-CEDICT 整合 ✓**：解析 → 简体词条 + 拼音（带声调数字）→ 音节序列索引；许可证合规（CC-BY-SA 署名 + 独立文件分发，决策点 D4 复核）
- **5.2 词库编译 ✓**：`rospinyin-lexicon.dat`（独立于主数据，可单独更新）；二字词优先裁剪控制体积
- **5.3 词组候选 ✓**：词优先、单字兜底的混合排序；词内字音一致性校验
- **5.4 词频调优**：默认词频表 + 简单平滑；常用词测试集（500 句）自动评估首命中率（未做，仍属 M4+）
- **5.5 验收 ✓**：nihao→你好、jisuanji→计算机（宿主测试 2/2 首选命中）；500 句语料评估尚未开展，不宣称 ≥80%。实现见 `tools/mklex/README.md`

## 6. M5 整句与自学习（估 16-24 人日）

- **6.1 音节全切分 ✓**：格子图（lattice）+ 全部合法切分路径
- **6.2 Viterbi 解码 ✓**：词频 + 字频 + 二元相邻概率打分；单/多候选回退
- **6.3 修改体验**：整句候选回退到词/字候选的操作流
- **6.4 用户词典 ✓**：上屏记录 → 用户词频累积；持久化（文件，独立于系统注册表，防 registry heal 意外）；导出/导入
- **6.5 性能**：整句解码 < 50ms（ReactOS 老硬件假设，500MHz 级也要可用）
- **6.6 验收 ✓**：日常短句平均击键减少 ≥ 30%（对比纯单字模式）；重启后自学习保留
- **6.7 v0.2「联」发布 ✓**

## 7. M6 设置 UI（估 8-12 人日）

- **7.1 设置对话框 ✓**：模糊音开关（z/zh、c/ch、s/sh、l/n、an/ang、en/eng、in/ing、f/h 可选）；候选数 3/5/9；翻页键方案；中英切换键
- **7.2 持久化 ✓**：注册表存储，读写走 ReactOS 兼容路径
- **7.3 引擎联动 ✓**：设置即时生效（重启 IME 上下文即可，不需重登录）
- **7.4 入口 ✓**：kbswitch 右键菜单 / 控制面板项（依 ReactOS 习惯定）

## 8. M7 遗留场景修复（估 10-20 人日，含上游往返时间）

- **8.1 控制台 IME**：评估上游 conime.exe 通路（CORE-18923 状态）；IME 侧 Console IME 接口适配；cmd 内输入验收；不可行则文档明确记录原因
- **8.2 RichEdit ✓**：写字板验证；IME 消息路径问题修复（ReactOS 自带 riched 系列可上游 PR）
- **8.3 次要控件 ✓**：文件对话框文件名框、组合框 edit 区、列表框内联编辑
- **8.4 三方应用抽查 ✓**：旧版 Firefox、Notepad2、PuTTY 等；兼容性矩阵入档
- **8.6 v0.3「全」发布 ✓**

## 9. M8 收尾与发布（估 10-16 人日）

- **9.1 全量回归 ✓**：nightly 最新版回归测试；异常退出（断电/杀进程）后词库与设置不损坏；连续 8 小时挂机无泄漏
- **9.2 文档 ✓**：安装/使用/构建/词典格式/架构五篇，中英双语
- **9.3 发行工程 ✓**：与 Unifont 字体包捆绑或字体检测提示（缺字体是新手第一大坑）；卸载支持
- **9.4 发布渠道 ✓**：GitHub Release、RAPPS 收录申请、CORE-19268 汇报、ReactOS 论坛中文区、社区新闻
- **9.5 维护模式交接 ✓**：Issue 模板、贡献指南、词库更新流程脚本化
- **9.6 v1.0「正」发布 ✓**

## 10. 贯穿性工作（非里程碑）

- **10.1 上游跟踪**：CORE-19268/18950/19455/20799/18923 状态月检；ReactOS nightly 破坏性变更适配
- **10.2 质量基线**：引擎单测覆盖率 ≥ 80%；每次发布前双机（XP/ReactOS）冒烟测试清单执行
- **10.3 社区运营**：每里程碑一篇进展帖（中英双语，JIRA + 论坛）；反馈 issue 分诊

---

## 估时汇总

| 里程碑 | 人日 | 累计 |
|---|---|---|
| M0 移植验证 | 12-18 | 12-18 |
| M1 骨架替换 | 15-20 | 27-38 |
| M2 单字引擎 | 22-30 | 49-68 |
| M3 体验补齐（v0.1） | 10-14 | 59-82 |
| M4 词组词频 | 14-18 | 73-100 |
| M5 整句学习（v0.2） | 16-24 | 89-124 |
| M6 设置 UI | 8-12 | 97-136 |
| M7 场景修复（v0.3） | 10-20 | 107-156 |
| M8 收尾发布（v1.0） | 10-16 | 117-172 |

> 单人业余节奏（每周 8-10 人时）约 5-7 个月到 v1.0；全职约 3.5-4 个月。
> 关键风险：M0 差异清单决定 M1/M2 实际工作量；CORE-19455 决定切换体验；上游 Console IME 进度决定 M7 范围。

## 11. 状态工具条与软键盘（后补，2026-11-16）★待排期

> 缺口说明：传统中文输入法普遍在桌面常驻一条可交互的「状态工具条 / 状态条」，一键切换中/英、全角/半角、中文/英文标点，并提供软键盘入口以输入特殊符号。现有 WBS 4.1/4.2 覆盖了状态机、4.5 覆盖了「状态窗（提示）」，但**没有把状态做成可点击的按钮工具条，也完全没有软键盘**，且 4.1 的标点开关缺少可见的切换入口。本章补齐这一交互层，逻辑复用 4.x，不重复造轮子。

- **11.1 状态工具条窗口 ⬜**（`ui/toolbar_window.*`，估 4-6 人日）
  - 11.1.1 常驻工具条窗口（独立 `WS_EX_TOOLWINDOW`，或复用 `ui_server` 窗口类扩展）；仅在 IME 被选中后创建，**绝不在 `DllMain` 里建窗**（A1：登录路径上零工作；2026-11-18 后这条的论据是"不留静默失败"，不再挂 0xF4，见 `M0-REDLINE-REVIEW.md` §13）
  - 11.1.2 按钮布局：中/英、全/半角、中文/英文标点、软键盘、设置菜单入口；显式字体链（Droid Sans Fallback 优先）与 DPI 自适应
  - 11.1.3 点击 → 驱动 4.1/4.2 状态机即时切换、重绘按钮图标，并与 4.5 托盘图标及候选窗提示保持同步
  - 11.1.4 位置记忆、屏幕边缘避让、可隐藏/显示（开关并入 M6 WBS 7.1 设置项）
- **11.2 软键盘 ⬜**（`ui/soft_keyboard.*`，估 3-5 人日）
  - 11.2.1 工具条「软键盘」按钮弹出模拟键盘窗（标准 PC 布局）；点击键位经 `WM_IME_CHAR` / `GCS_RESULTSTR` 上屏，Shift/锁定态可用，Esc 关闭
  - 11.2.2 符号页：中文标点（复用 4.1 映射表）、希腊字母、日文假名、数学/制表符等常见特殊符号集
  - 11.2.3 记事本（EDIT 控件）实测上屏；写字板/RichEdit 等控件在 M7 WBS 8.2 复测
- **11.3 状态同步与持久化 ⬜**（估 2-3 人日）
  - 11.3.1 工具条 ↔ 4.2 状态机 ↔ 7.2 注册表持久化三向一致；切换即时生效、跨会话保留
  - 11.3.2 异常退出（断电/杀进程）后按钮状态与配置不损坏，纳入 M8 WBS 9.1 回归清单
- **11.4 验收与文档 ⬜**（估 1-2 人日）
  - 11.4.1 ReactOS nightly 逐按钮验收：全/半角、中/英标点、软键盘上屏、焦点切换不丢状态、连续操作无崩溃无泄漏
  - 11.4.2 README 增补「状态工具条与软键盘」章节并附截图

> 本章为后补范围，估时约 10-16 人日，未计入上文「估时汇总」表；若纳入，v1.0 总计由 117-172 调整为约 127-188 人日。

## 12. F 字形与字体（估 5-7 人日；2026-11-19 计划变更，F-T1/T2 建议与 M3 同期）

> **2026-11-19 变更（owner 决策）**：摒弃「调用输入法时运行时字形探测」路线
> （原 12.1 F0 探针 + 12.3 探针改造 + 12.5 过滤三态化），改为**构建期分级码表 +
> 用户全字形开关**——「哪些字画得出来」在构建期按 UI 字体的 cmap 算好存进码表，
> 运行时只做布尔判断，不再每键查 GDI。原 F0（VM GDI 能力探针）取消：它的结论只
> 服务 IDS 兜底，兜底已随探测一并摒弃。变更理由与阶段表见 `ROADMAP.md` §F。
> 12.2（D-F1 字体风格）推迟到字体包有真实需求时拍板；字体选型结论
> （`MINIMAL-FONT-SET-PLAN.md` 实测 100%）仍指导 12.4。

- **12.1 F-T1 分级码表（dat v2）**（估 1.5-2 人日）
  - 12.1.1 `tools/` 新增 cmap 提取脚本（fontTools，Linux 宿主一次性）：从 Droid Sans Fallback
    提取覆盖码点集，入库 fixtures（单一真相源）
  - 12.1.2 mkdat 吃码点集，Entry 第 6 槽 `flags` bit1 = tier（**0 = 系统保底可绘，1 = 需额外字体**；
    统一格式见 `DAT-FORMAT-V2.md`：Header **80B** / Entry 24B / 词组两表，与形码同骨架）。
    **格式本体已于 2026-11-20 落地**（dat v2 读写、80B 头、词组表位；v1 不再兼容，旧表被拒后回退内置表）——
    本项只剩「把 cmap 分级数据灌进 bit1」这一件事
  - 12.1.3 同输入逐字节复现；两级字数统计入构建日志与 EXIT-GAP 口径
- **12.2 F-T2 全字形开关 + 探测链退役**（估 1-1.5 人日）
  - 12.2.1 会话级布尔（默认关、不持久化——不写注册表不写文件）；重置时机三处：IME 重选、
    切换输入法、切换上下文（据此放 `on_select`/激活路径，一行赋值）
  - 12.2.2 候选过滤语义：关闭态候选只含 tier 0（rare/无 rank 维持现行为），打开态不过滤；
    行为变更点记录：原「tier 1 且有 rank」的字由「显示后剔除」改为「关闭态不显示」
  - 12.2.3 `GlyphProbe`/`UiCanRender`/`Win32Host::can_render`/`renderable_candidates` 运行时探测链退役
    （X2-f 的 can-render seam 2/2 归零；`tools/seams.py` 与 `check_m1` 同步删项）
- **12.3 F-T3 快/全指示器与切换**（估 1.5-2 人日；排在 M3 键位定型之后）
  - 12.3.1 `scheme.hpp` 加全字形指示虚函数（默认空 → toy scheme 零改动）；`toolbar_text()` 扩展
    `快/全` 段（`拼:开`/`快` 或 `拼:开`/`全`），toolbar_text_test 扩展；toolbar 宽度重算
    （44px 实测边界已裁过 `拼:开`，5 全角字符需实测）
  - 12.3.2 切换交互先做一种：**首选 lone-Ctrl 热键**（与 lone-Shift 同构，键路由/冒烟基建复用；
    ReactOS imm32 热键注册有 CORE-19455 前科，须 VM 实测，失败则退工具栏点击）；备选：工具栏点击
    （`WM_NCHITTEST` 返 `MA_NOACTIVATE` 防抢编辑器焦点）；未实现的那种归 PLAN §11 状态工具条
  - 12.3.3 与 Ctrl+Space 的判定次序写明：先组合键（Ctrl+字母）后 lone-Ctrl，避免吞掉系统热键
- **12.4 F-T4 安装器提示 + 字体包**（估 1-1.5 人日，与 M3/M4 并行）
  - 12.4.1 安装器 `/status` 与发布说明写明「全字形模式需字体支持」；alpha0.0.3 说明按 HANDOVER
    §6.4 纪律（安装器首选）+ 全字形提示；README 已知问题同步
  - 12.4.2 独立可选字体包（≈57 MB，文津/Jigmo 三件套，不进主发行包）；`AddFontResourceW` +
    Fonts 键 + `WM_FONTCHANGE`；**只许在安装器里注册**（`check_m1` §6 白名单）；`/status` 三态、
    `/u` 对称清理
- **12.5 回归口径**（并入上述各项）
  - 12.5.1 `coverage_report.py` 适配 tier：tier 0 覆盖率单独报告，M2 的 96.8% 门限按 tier 0 口径复核
  - 12.5.2 同步 `RARE-CHAR-AND-GLYPH-SOLUTION.md`（第 1/2 层被 F-T2 取代、第 3 层被开关吸收、
    第 4 层并入 12.4）与 `docs/milestones/m2/M2-REMAINING.md` 的 P1-2 口径

退出标准（改）：关闭态（默认）候选集 ⊆ tier 0，零方框由构建期保证；打开态全字可达、
方框为文档化预期；`快/全` 指示器与开关一致、三重置时机后回关闭态；开关零持久化；
探测链删除后 ctest/check_m1 全绿。
