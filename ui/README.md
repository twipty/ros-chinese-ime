# ui/ — IME 窗口层（M2 已落地）

RosPinyin 的窗口层分三层组织。M1 只保留分层约定，M2 已把组合串窗口与候选窗落地：

| 层 | 位置 | 状态 | 内容 |
|---|---|---|---|
| UI server 窗口类 | `ime/ui_server.cpp` | **M1 已实现** | 注册 ImeInquire 返回的类名并转发 `DefWindowProcW`；不绘制、不访问输入上下文（M0 的 0xF4 红线） |
| 窗口基类 / 字体 | `ui/ui_window.*` | **M2 已实现** | 两个 UI 类的惰性注册、无激活置顶弹窗、`Droid Sans Fallback → Noto Sans SC → Microsoft YaHei → SimSun → Arial Unicode MS` 字体兜底链（用 `GetTextFaceW` 验证 GDI 实际解析结果，避免静默替换成无 CJK 字形的字体） |
| 组合串窗口 | `ui/composition_window.*` | **M2 已实现** | 显示规范化拼音串，按引擎 `syllables()` 画音节下划线（如 `zhongguo` → `[zhong][guo]`） |
| 候选窗 | `ui/candidate_window.*` | **M2 已实现** | 9 候选编号、`-`/`+` 翻页提示、按字体实际度量动态改宽、UTF-32 → UTF-16 显示转换 |
| 状态窗与图标 | `ui/status_window.*` | M3 (WBS 4.5) | 中/英、全/半角；托盘图标已由 `.ime` 的 `RT_GROUP_ICON` 提供（M1-2.5） |

## 生命周期约束

- `RegisterUiWindowClasses()` 与窗口创建都只发生在 `UiShow()`（由 IMM32 按键入口调用），**绝不在 `DllMain` 里创建窗口**；
- 每个 HIMC 的窗口状态存在 `ui/ime_ui.cpp` 的函数内静态 `UiStore` 中，用 Win32 `CRITICAL_SECTION` 保护（不用 `std::mutex`，避免 MinGW 把 `libwinpthread` 带进 PE）；
- `ImeSelect(FALSE)` → `UiDestroy()` 销毁窗口，`ImeSetActiveContext(FALSE)` / 上屏 / Esc → `UiHide()` 只隐藏不销毁，便于复用；
- 所有窗口过程对未知的 `WM_IME_*` 一律 `DefWindowProcW`，不抛异常、不访问未初始化的 HIMC；
- 组合串窗口跟随插入符（`GetCaretPos` + `ClientToScreen`），候选窗在其下方；越界时向左/向上翻转并夹到屏幕内。

## 已知限制

- ReactOS 英文 locale 下 `[Font.CJK.Reg]` 不挂载，`GetTextFaceW` 可能回退到无 CJK 字形的字体。若候选显示为方框，需在 guest 安装带 CJK 字形的字体（见 `m0/vm/NOVNC-GUIDE.md`）。
- 未使用 IMM32 的 DDK 候选列表结构（`IMECANDIDATELIST`），而是自绘窗口，避免依赖 ReactOS 未完整实现的 DDK 接口。
