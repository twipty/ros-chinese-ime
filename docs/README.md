# 文档地图（docs/）

> 本仓库的工程文档全部在 `docs/` 下，本文件是**唯一索引**。
> 根目录只允许出现 `README.md`（项目门面）、`LICENSE`、`NOTICE.txt`（法律声明）
> 和 `CMakeLists.txt`（构建入口）——其余 `.md` 一律不进根目录。
> 这套规则由 `tools/check_docs.py` 强制（CI 里跑），新增文档不登记进本索引就会失败。

## 1. 先读什么

| 你要做什么 | 读这一串 |
| --- | --- |
| 第一次接手这个项目 | [项目总览](projectoverall.md)（根目录 `README.md` 是简短门面）→ [HANDOVER §1–§3](handover/HANDOVER.md) → [PLAN §3](plan/PLAN.md) → 当前里程碑报告 |
| 每轮开工前对齐状态 | [PLAN §3 进度](plan/PLAN.md)、[ROADMAP 退出标准](plan/ROADMAP.md)、[HANDOVER §3 状态表](handover/HANDOVER.md) |
| 准备发版 / 打 tag | [HANDOVER §12](handover/HANDOVER.md) + 先写 `docs/releases/<tag>.md` |
| 要动字体、字形、生僻字 | [font/](font/) 整个目录；**先看 [FONT-PLAN-REVIEW](font/FONT-PLAN-REVIEW.md)，那是裁决** |
| 要动框架结构 / 想移植平台 | [reviews/](reviews/) |
| 想验证"某个改动是否合规" | 直接跑 `python3 tools/check_m1.py`（63 项契约断言），不要靠读文档判断 |

## 2. 目录约定

| 目录 | 放什么 | 生命周期 | 使用时机 |
| --- | --- | --- | --- |
| `plan/` | 计划与路线：里程碑拆解、退出标准 | **活文档**，随进度持续改 | 每轮开工必读，改完必更新 |
| `handover/` | 交接手册、变更日志 | 活文档（追加为主） | 开工对齐、收工登记、发版前查流程 |
| `milestones/` | 阶段报告：结论、证据、已知限制 | **只读**，阶段结束后不再改（勘误另起注记） | 回溯"当时为什么这么定" |
| `font/` | 字体与字形专题：评估、两个候选方案、裁决、落地计划 | 阶段内活、定案后只读 | 做 F 里程碑时整目录读 |
| `reviews/` | 跨切面评估：框架路线、平台可移植性 | 只读 | 做架构或移植决策前 |
| `releases/` | 每个 tag 一份发版说明 | 追加，**一个 tag 一个文件** | 由 CI 与版本门禁机器读取，不要改名或移动 |

## 3. 文件索引（全量）

### 项目总览

| 文件 | 内容 |
| --- | --- |
| [projectoverall.md](projectoverall.md) | 原根目录 `README.md` 全文归档：项目章程、立项依据、目标与非目标、技术基线、数据与许可证、M0–M2 历程与框架抽取记录（历史只读，现状以本索引所列活文档为准） |

### plan/ — 计划与路线

| 文件 | 内容 |
| --- | --- |
| [PLAN.md](plan/PLAN.md) | 总计划：M0–M8 里程碑分项、当前状态、F 字形里程碑（§12） |
| [ROADMAP.md](plan/ROADMAP.md) | 路线图与每个里程碑的退出标准（含 M2 首次实测注记） |
| [PLAN-MODULARIZATION.md](plan/PLAN-MODULARIZATION.md) | 框架抽取（引擎宿主 / UI / 工具条独立成可复用模块）的分步计划 |
| [DAT-FORMAT-V2.md](plan/DAT-FORMAT-V2.md) | **码表 dat 统一格式 v2 冻结规范（拼音侧落地文本）**：64B 头 / 24B 条目 / `flags` bit1 = tier、v1 兼容读法、拼音三条校验红线；上位文本见 `xingma/docs/glyph-tier/UNIFIED-DAT-FORMAT.md` |

### handover/ — 交接与历史

| 文件 | 内容 |
| --- | --- |
| [HANDOVER.md](handover/HANDOVER.md) | 交接手册（最大的一本）：状态表、操作手册、踩坑记录 §11.x、发版与验收流程 §12 |
| [CHANGELOG.md](handover/CHANGELOG.md) | 变更日志：按版本记录行为变化、门禁变化、新增文档 |

### milestones/ — 阶段报告

> **路径按当时的结构记，不回填。** 2026-11-18 的 X1 目录归位把 `framework/host/` 与
> `framework/ui/` 移到了 `framework/platform/win32/{imm32,ui}/`；本目录与 `reviews/` 里的
> 旧路径**故意保留原样**，因为它们记录的是"当时代码长什么样"。要看当前路径，查
> `framework/README.md` 或直接用 `git ls-files`。

| 文件 | 内容 |
| --- | --- |
| [M0-REPORT.md](milestones/M0-REPORT.md) | M0 实测报告：蓝本崩溃根因、BSOD 0xF4、上游许可证变更、四条红线的由来 |
| [M0-REDLINE-REVIEW.md](milestones/M0-REDLINE-REVIEW.md) | 四条 M0 红线逐条复核与降级记录（§13） |
| [M1-REPORT.md](milestones/M1-REPORT.md) | M1 报告：16 个 IMM32 导出、安装器分步注册、VM 实机结论 |
| [m2/M2-REPORT.md](milestones/m2/M2-REPORT.md) | M2 报告：结论、交付物、10 条根因修复、已知限制 |
| [m2/M2-ACCEPTANCE.md](milestones/m2/M2-ACCEPTANCE.md) | 15 项真机验收操作单（含判定口径，唯一能在真机暴露 ABI 缺陷的用例在这） |
| [m2/M2-REMAINING.md](milestones/m2/M2-REMAINING.md) | M2 剩余事项与优先级（现全部关闭） |
| [m2/M2-DATA-PLAN.md](milestones/m2/M2-DATA-PLAN.md) | 数据源选型、声调与 `ü` 的规范形、规模核算 |
| [m2/M2-EXIT-GAP.md](milestones/m2/M2-EXIT-GAP.md) | M2 距离准出的逐条对照与三条记录缺口 |

### font/ — 字体与字形专题

| 文件 | 内容 |
| --- | --- |
| [FONT-PLAN-REVIEW.md](font/FONT-PLAN-REVIEW.md) | **裁决**：全量字形（A）与最少字体（B）的取舍，B 取代 A 作实施依据；含待决策点 D-F1。（2026-11-19：F 里程碑改为分级码表+全字形开关，本文阶段表作废，见 plan/ROADMAP.md §F） |
| [MINIMAL-FONT-SET-PLAN.md](font/MINIMAL-FONT-SET-PLAN.md) | 方案 B（实施依据）：最小字体集、实测覆盖率与体积 |
| [FULL-GLYPH-COVERAGE-PLAN.md](font/FULL-GLYPH-COVERAGE-PLAN.md) | 方案 A（已被取代）：全量字形覆盖推算；§6 非 BMP 渲染退路仍有效 |
| [FONT-BUNDLING-ASSESSMENT.md](font/FONT-BUNDLING-ASSESSMENT.md) | 是否捆绑字体的早期评估：体积、授权、P1/P2 风险清单 |
| [RARE-CHAR-AND-GLYPH-SOLUTION.md](font/RARE-CHAR-AND-GLYPH-SOLUTION.md) | 生僻字与字形过滤的完整方案：rank 语义、渲染可见性、入口策略 |

### reviews/ — 跨切面评估

| 文件 | 内容 |
| --- | --- |
| [FRAMEWORK-ROUTE-A-REVIEW.md](reviews/FRAMEWORK-ROUTE-A-REVIEW.md) | 框架路线 A 的评审：可复用宿主 / UI 模块的边界与代价 |
| [PORTABILITY-UPSTREAM-64BIT-ARM64.md](reviews/PORTABILITY-UPSTREAM-64BIT-ARM64.md) | 上游 64 位与 ARM64 可移植性评估（与 libernt 自研分支区分，结论不可互换） |
| [ARCH-AUDIT-VERIFICATION.md](reviews/ARCH-AUDIT-VERIFICATION.md) | 架构审计的逐条复核：13 条结论的判定、3 处需修正、3 条审计漏项、与 X1–X4 的映射 |

### releases/ — 发版说明

| 文件 | 内容 |
| --- | --- |
| [alpha0.0.2.md](releases/alpha0.0.2.md) | alpha0.0.2 的发布说明（CI 用它作为 release notes） |

### 留在原处的子系统文档（不归 docs/）

这些是代码说明书，跟着代码走比跟着文档库走更合理：

- `framework/README.md`、`framework/platform/win32/ui/README.md` — 框架层复用指南
- `pinyin/README.md` — 引擎层说明
- `libernt/PORTABILITY-ASSESSMENT.md` — libernt 自研分支的可移植性评估
- `m0/vm/NOVNC-GUIDE.md`、`m0/vm/PRELOAD-PROBE.md`、`m0/vm/evidence/m2-acceptance/result.md` — 虚拟机操作与实测证据

## 4. 归置规则（新文档该放哪）

| 新文档是什么 | 放这里 |
| --- | --- |
| 会持续更新的计划 / 路线 | `plan/` |
| 操作手册、踩坑记录、变更日志 | `handover/` |
| 某个里程碑结束时的报告 | `milestones/<Mx>/` |
| 字体、字形相关的评估与方案 | `font/` |
| 跨切面的架构 / 平台评估 | `reviews/` |
| 某个 tag 的发版说明 | `releases/<tag>.md`（一个 tag 一个文件） |
| 讲某个代码目录怎么用 | 跟着代码走，放该目录下 `README.md` |

**三条硬规则**

1. 根目录不放文档（`README.md`、`LICENSE`、`NOTICE.txt`、`CMakeLists.txt` 除外）。
2. 新增或移动文档必须同步更新本索引，否则 `tools/check_docs.py` 失败、CI 红。
3. `docs/releases/` 被 `tools/check_release_version.py` 与 CI 的发布 job 直接读取，**不要改名、不要移动**。

## 5. 引用约定

- **markdown 链接**用相对路径（`[X](../milestones/M0-REPORT.md)`），由 `tools/check_docs.py` 校验可解析。
- **行内代码里的路径**统一写仓库根相对形式：`` `docs/milestones/m2/M2-ACCEPTANCE.md` ``，便于全局检索。
- **裸文件名**（如 `HANDOVER.md §11.20`）允许出现在正文里，表示"按名字查"；实际位置以本索引为准。
- 引用小节号（`§11.20`）比引用行号可靠——行号会随编辑漂移。
