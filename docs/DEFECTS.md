# 缺陷及修复清单

本清单登记 **L1 局部缺陷**：修复范围未超出该缺陷原功能设计影响模块的 bug。
修复范围超出原影响模块（公共层、新增模块、跨模块契约、冻结资产、跨服务）的缺陷归为 **L2**，走 dev-loop S0–S7 完整流程，不登记在本清单。

判据基线：原功能的 S1 改造清单文件集合；实际改动文件超出该集合即升级为 L2。拿不准按 L2 处理。
L1 仍须留最小验证证据（红绿测试或可复现步骤），但不开阶段确认门、不产 S0–S7 报告。

## 登记项

| ID | 发现时间 | 现象 | 级别 | 根因 | 修复范围 | 验证证据 | 状态 |
|---|---|---|---|---|---|---|---|
| DEF-001 | 2026-11-27 | 新安装包「我已阅读并同意许可协议」复选框点击后完全无变化，安装按钮一直禁用 | L1 局部 | `tools/install-rospinyin/gui.cpp:673-676` 用 `BS_CHECKBOX`（手动切换），而 `WM_COMMAND` 分支（703-718）只 `BM_GETCHECK`、不回写 `BM_SETCHECK`，状态永远为未勾选 | `tools/install-rospinyin/gui_logic.hpp`（控件契约）、`tools/install-rospinyin/gui.cpp`（Win32 样式组装与状态同步）、`tests/installer_logic_test.cpp`（宿主测试）——均在 alpha0.1.2 安装器 S1 清单内 | 红：`cmake --build build-host-off --target rospinyin-installer-logic` 报 `agreement_control_spec 不是 gui 的成员`；绿：同命令编译通过后 `ctest` 两种配置各 24/24 通过；重建 EXE `05982db2…` / 3,649,382 B（后由 DEF-002 重建覆盖为 `7d167373…` / 3,556,710 B，复选框修复保留），`sha256sum -c` 6/6 成功；**owner 实机确认六项全通过**：选中→可安装、取消→不可安装、重选→可安装、Tab 聚焦+空格切换、连续/双击不反向、协议窗口开关不影响勾选，按钮门禁与状态文字同步 | 六项验收标准实机确认通过 |
| DEF-002 | 2026-11-27 | 协议中文显示不全；NOTICE 仍有形码残留字样 | L1 局部 | **截图已确认**：中文全部渲染为空白/点/下划线，英文 ASCII 完整、文本连续、滚动正常 → guest 字体缺字（nightly 0.4.17-dev 缺 DroidSansFallback.ttf），**不是 EDIT 截断**；`NOTICE-pinyin.txt:6` 协议拆分时漏删 RosXingMa 那句 | `tools/install-rospinyin/NOTICE-pinyin.txt`（中英双语、删残留）、`tools/install-rospinyin/payload_base64.hpp`（重生成）——均在协议拆分 S1 清单内 | 双语标题存在、英文段全 ASCII、无 xingma/形码 字样；内嵌 NOTICE 与源文件 5,505 B 逐字节一致；`ctest` 两种配置各 24/24；重建 EXE `7d167373…` / 3,556,710 B，`sha256sum -c` 6/6，HTTP Content-Length 已更新；owner 截图来自**旧包**（仍见 `(RosXingMa)` 与 `—` 标题），新包待复验 | 根因已判读，双语规避已部署，待新包实机确认 |
| DEF-003 | 2026-11-27 | 用户可见 NOTICE 第 2 条提及 mzimeja 蓝本（含 GPLv3 版本边界），但该蓝本不分发、不链接、不构建 | L1 局部 | 协议拆分时把工程尽调文档 `vendor/LICENSE-NOTICE.txt` 的内容搬进了用户可见协议，未区分受众；M1 报告明确 `ime/` 代码从零写、蓝本只读参考，故对最终用户无授权告知义务，完整边界说明已在仓库内 `vendor/LICENSE-NOTICE.txt` | `tools/install-rospinyin/NOTICE-pinyin.txt`（删中英第 2 条并顺延编号）、`tools/install-rospinyin/payload_base64.hpp`（重生成）——均在协议拆分 S1 清单内；`NOTICE-xingma.txt` 核查后无此段，未改 | 静态校验：内嵌 NOTICE 与源文件 4,542 B 逐字节一致、无 mzimeja/xingma/形码、英文段纯 ASCII；四个 payload SHA-256 与 SHA256SUMS 一致。17:27 经 owner 授权后重建 EXE `6fc345865d24de5998d6c251ef23c49dc85d2861db2e29bed0166330e23b2adf` / 3,555,686 B，已覆盖 `m0/share` 并同步 SHA256SUMS 第 5 行；`sha256sum -c` 6/6、HTTP Content-Length 一致，两种配置 `ctest` 各 24/24 | 源、生成物与 EXE 已修正部署，待 owner 新包实机复验 |

### DEF-001 详情

- 来源：owner 原话「新安装包问题：”我已阅读并同意许可协议“无法勾选」。
- 期望行为：鼠标单击可切换勾选；Tab 聚焦后空格可切换；勾选状态变化时安装按钮与状态文字同步；未勾选时按钮禁用；连续/双击不反向；协议窗口开关不影响勾选状态。
- 修复实现：`gui_logic.hpp` 新增 `AgreementControlSpec`（`auto_toggle=true`、`keyboard_focusable=true`）；`gui.cpp` 据此创建 `BS_AUTOCHECKBOX | WS_TABSTOP`，其余按钮补 `WS_TABSTOP`，消息循环加 `IsDialogMessageW` 才能在普通窗口里真正走 Tab 链；`WM_COMMAND` 原已每次重读 `BM_GETCHECK` 并同步按钮/文字，保持不变。
- 版本：重建 alpha0.1.2（尚未正式打 tag），不升版本号、不动协议文本/payload/IME 哈希。
- 部署：新 EXE 已覆盖 `m0/share/install-rospinyin-gui.exe`，SHA256SUMS 第 5 行同步为 `05982db25ed33d2e79bf71bbcfb75dacf31da1a04e22e1b8dadacf43d427a2b3`（大小 3,649,382 B），其余 5 个资产哈希未变；后经 DEF-002 重建覆盖为 `7d167373…`，17:27 再经 DEF-003 重建覆盖为 `6fc34586…`，DEF-001 修复在两次重建中保留（gui.cpp / gui_logic.hpp / 测试未再改动）。
- **实机确认（owner 2026-11-27）**：
  - 首批原话：「已成功：1.点击选中同意协议可安装；2.取消选中不可安装；3.重新选中可安装；2.安装结束无干预自动重启」——第 1–3 点覆盖鼠标勾选/取消/重选三态，按钮门禁按预期同步；第 4 点属于安装器「完成后自动重启」需求的实机证据，不是本缺陷项。
  - 补充原话：「tab聚焦和空格切换测试通过，连续点击/双击已通过，协议窗口开关已通过」——覆盖剩余三项。
  - **DEF-001 六条验收标准全部实机通过**；仍未验的是 DEF-002 的双语协议内容（英文段可读、无 xingma，owner 未报告），以及 DEF-003 的 mzimeja 清理尚未进 EXE。
  - 实机包按上下文为 6081 当前部署的 `7d167373…`（DEF-002 重建版，含 DEF-001 修复）；owner 未明确指定 guest 与包哈希。
- dev-loop：本缺陷曾误开一轮（S0 待确认），按 2026-11-27 分流纪律作废，不再推进。

### DEF-002 详情

- 来源：owner 原话「协议中文字显示不全，推测中文支持较差，请优化协议内容，notice部分准备中英双语版本。此外仍有残留的xingma文字」。
- 中文显示不全：**owner 截图已判读**。图中 `SOFTWARE.`、`================ NOTICE ================`、`MIT License ( LICENSE ).`、`ReactOS (GPLv2-or-later)`、`katahiromz/mzimeja v1.0.0.3 ... (PDS).` 等 ASCII 文本完整可读，滚动条位置正常、文本连续；对应中文行全部渲染为空白、孤立标点和下划线。结论是 **guest 字体缺字**（nightly 0.4.17-dev 缺 DroidSansFallback.ttf，见 alpha0.1.2 release 说明的已知问题），**不是 EDIT 控件截断**，因此本轮的双语规避有效，不需要改窗口布局。
- 同一截图仍可见 `(RosXingMa)` 和 `RosPinyin — Third-party notices /` 的 `—` 标题，说明 owner 截的是**旧包**，不是已部署的 `7d167373…` 新包；新包待重新下载复验。
- 修复实现：`NOTICE-pinyin.txt` 改为 `[中文]` + `[English]` 两段（中文在前），英文段纯 ASCII，缺字体时仍可读；删除第 6 行 RosXingMa 残留；`NOTICE-xingma.txt` 保留给形码独立产品。`payload_base64.hpp` 用 `pack_payload.py` 默认参数重生成，四个 payload 的 SHA-256 与 SHA256SUMS 一致。
- **额外发现**：仓库里旧的 `payload_base64.hpp` 内嵌的 CLI 压缩数据是过期的（287,911 B），`m0/share/install-rospinyin.exe` 当前版本压缩后为 193,189 B（sha `c070c197…`）。重生成同步到发布资产，这是 EXE 从 3,649,382 B 降到 3,556,710 B 的原因，不是数据丢失。
- 部署：新 EXE SHA256 `7d167373f6821e78b56466e7df6eb164adcab1b646c846174b350e45307521ee`，已覆盖 `m0/share/install-rospinyin-gui.exe` 并同步 SHA256SUMS 第 5 行。
- dev-loop：L1，未开轮次。

### DEF-003 详情

- 来源：owner 给出新包协议两张截图后提问「notice 里为什么要提及 mzimeja？」。
- 结论：mzimeja v1.0.0.3 是 M1 阶段的只读架构蓝本（IMM32 状态机、UI 框架、`.def` 导出、CMake 布局），上游 2026-01-12 改 GPLv3，写进 NOTICE 的原意是留一条「只用 PDS 版、不复制 GPLv3 源码」的工程尽调边界。但该蓝本按 `vendor/LICENSE-NOTICE.txt` 明示「NOT built / NOT linked / NOT shipped」，`docs/milestones/M1-REPORT.md:82` 确认 `ime/` 代码全部新写，`tools/check_m1.py` 还会扫 `mzimeja` 残留——对用户没有告知义务，内容属于工程尽调误入用户协议，与 DEF-002 的 xingma 残留同类。
- 修复实现：删除 `NOTICE-pinyin.txt` 中英文第 2 条（mzimeja 段落），后续条款顺延编号（中文 3→2、4→3、5→4、6→5；英文同），第 3 条内「见第 3 条」改为「见第 2 条」。`NOTICE-xingma.txt` 全文核查无 mzimeja 段落，未改。
- 生成物：`pack_payload.py` 默认参数重生成 `payload_base64.hpp`；内嵌 NOTICE 4,542 B 与源文件逐字节一致，四个 payload 哈希（IME `87c8f3cc…` / DAT `db0c7bc…` / CLI `c070c197…` / LEX `a89ec5e5…`）与 SHA256SUMS 一致。
- **初始交付边界（owner 16:37 指令）**：「把修改生成链上的 notice 修正，不重建 EXE，下次生成和重建时一并测试」。该边界在 17:27 被 owner「执行并提交，完成后推动下一阶段」追加授权取代，见下条。
- **后续推进（2026-11-27 17:27）**：重建 `cmake --build build-cross --target install-rospinyin-gui`，新 EXE SHA256 `6fc345865d24de5998d6c251ef23c49dc85d2861db2e29bed0166330e23b2adf`、3,555,686 B；已覆盖 `m0/share/install-rospinyin-gui.exe`，SHA256SUMS 第 5 行同步；`sha256sum -c` 6/6，HTTP `Content-Length: 3555686` 与磁盘、下载流哈希三方一致。二进制内 `mzimeja` / `RosXingMa` / `xingma` / `形码` 均不存在，`[中文]` / `[English]` / `MIT License` 标记存在；`ctest` 两种配置各 24/24。
- dev-loop：L1，未开轮次。
