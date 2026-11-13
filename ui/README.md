# ui/ — IME 窗口层（M1 占位）

RosPinyin 的窗口层按三层组织，本目录在 M1 只保留分层约定，真正的窗口代码从 M2 起落地：

| 层 | 位置 | 何时实现 | 内容 |
|---|---|---|---|
| UI server 窗口类 | `ime/ui_server.cpp` | **M1 已实现** | 仅注册 ImeInquire 返回的类名并转发 `DefWindowProcW`；不做任何绘制、不访问输入上下文（M0 的 0xF4 红线） |
| 组合串窗口 | `ui/composition_window.*` | M2 (WBS 3.3.1) | 拼音串 + 音节切分下划线 + 光标 |
| 候选窗 | `ui/candidate_window.*` | M2 (WBS 3.3.2) | 9 候选、编号、翻页、跟随光标、边缘避让 |
| 状态窗与图标 | `ui/status_window.*` | M3 (WBS 4.5) | 中/英、全/半角、托盘图标 16x16 |

约束（贯穿各层）：

- 窗口创建只能发生在 IMM32 已选中该 IME 之后，**绝不在 `DllMain` 里创建窗口**；
- 字体必须显式指定可覆盖 CJK 的字体并带兜底链（M0 核实：ReactOS 的 `DroidSansFallback.ttf` 覆盖 16504 汉字，但 `[Font.CJK.Reg]` 只在 CJK locale 下挂载）；
- 所有窗口过程对未知的 `WM_IME_*` 一律 `DefWindowProcW`，不得抛异常或访问未初始化的 HIMC。
