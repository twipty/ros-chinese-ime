# framework/platform/win32/ui/ — 输入法窗口层（可复用）

框架的窗口层，任何装配出的 IME 共用（窗口类名由各 IME 的 Scheme 提供）。M2 已落地组合串窗口与候选窗；工具条为 2026-11-16 变更新增的可复用模块：

| 层 | 位置 | 状态 | 内容 |
|---|---|---|---|
| UI server 窗口类 | `framework/platform/win32/imm32/ui_server.cpp` | **M1 已实现** | 注册 ImeInquire 返回的类名（来自 Scheme）并转发 `DefWindowProcW`；不绘制、不访问输入上下文（M0 的 0xF4 红线） |
| 窗口基类 / 字体 | `framework/platform/win32/ui/ui_window.*` | **M2 已实现** | 按类名惰性注册（两个 IME DLL 可同进程共存）、无激活置顶弹窗、`Droid Sans Fallback → Noto Sans SC → Microsoft YaHei → SimSun → Arial Unicode MS` 字体兜底链（`GetTextFaceW` 验证 GDI 实际解析结果） |
| 组合串窗口 | `framework/platform/win32/ui/composition_window.*` | **M2 已实现** | 显示原始输入串，按 `Result.segments` 画切分下划线（拼音 `[zhong][guo]`，形码同理） |
| 候选窗 | `framework/platform/win32/ui/candidate_window.*` | **M2 已实现** | 9 候选编号、`-`/`+` 翻页提示、按字体实际度量动态改宽、UTF-32 → UTF-16 转换 |
| **输入法工具条** | `framework/platform/win32/ui/toolbar.*` | **2026-11-16 新增** | 右下角浮动、置顶不抢焦点、显示 Scheme 标签（RosPinyin 为 `拼`）；`ImeSelect(TRUE/FALSE)` 控制显示/隐藏；中/英与 `[快]/[全]` 指示已随 M3 落地，全角/半角独立开关仍待实现 |
| 状态窗与菜单 | 预留 | M3 | 工具条点击菜单、状态指示扩展（WBS 4.5） |

## 生命周期约束

- 窗口类注册与窗口创建都只发生在 `UiShow()` / `UiToolbarShow()`（由 IMM32 按键/选择入口调用），**绝不在 `DllMain` 里创建窗口**；
- 每个 HIMC 的窗口状态存在 `ime_ui.cpp` 的函数内静态 `UiStore` 中，用 Win32 `CRITICAL_SECTION` 保护（不用 `std::mutex`，避免 MinGW 把 `libwinpthread` 带进 PE）；
- `ImeSelect(FALSE)` → `UiDestroy()` 销毁窗口，`ImeSetActiveContext(FALSE)` / 上屏 / Esc → `UiHide()` 只隐藏不销毁；
- 所有窗口过程对未知的 `WM_IME_*` 一律 `DefWindowProcW`，不抛异常、不访问未初始化的 HIMC；
- 组合串窗口跟随插入符（`GetCaretPos` + `ClientToScreen`），候选窗在其下方；越界时向左/向上翻转并夹到屏幕内；工具条固定停泊在主屏右下角。

## 已知限制

- ReactOS 英文 locale 下 `[Font.CJK.Reg]` 不挂载，`GetTextFaceW` 可能回退到无 CJK 字形的字体。若候选显示为方框，需在 guest 安装带 CJK 字形的字体（见 `m0/vm/NOVNC-GUIDE.md`）。
- 未使用 IMM32 的 DDK 候选列表结构（`IMECANDIDATELIST`），而是自绘窗口，避免依赖 ReactOS 未完整实现的 DDK 接口。
