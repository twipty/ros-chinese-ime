# M1 阶段报告（骨架替换）— 2026-11-13

> 状态：**宿主侧代码完成，VM 实测待做**。本报告记录 M1 第一轮推进（WBS 2.2 全部、2.3.2 安装工具源码、2.4.1 直通语义及其宿主可测部分）。
> 前置结论见 [M0-REPORT.md](M0-REPORT.md)，任务总表见 [PLAN.md](PLAN.md)，仓库章程见 [README.md](README.md)。

## 1. 本轮做了什么

M0 结束后任务板剩下 #9（剥离日语逻辑 + 资源/版本信息替换）、#10（中文注册）、#11（空 IME 直通 + 切换压测）。本轮把**不需要 VM 的部分全部落地并自动化验证**：

| 任务 | 本轮成果 | 状态 |
|---|---|---|
| #9 / WBS 2.2 | 新写 IMM32 骨架（16 个导出全实现）、中文资源与版本信息、日语逻辑零残留 | ✅ 完成并有自动化验收 |
| #10 / WBS 2.3.2 | `install-rospinyin.exe` 源码：动态 HKL、分步注册、无 Preload | ✅ 源码完成；VM 分步验证待做 |
| #11 / WBS 2.4.1 | 直通语义（`ImeProcessKey` 全放行、`ImeToAsciiEx` 零消息）+ 引擎层 50 次切换单测 | ✅ 宿主侧完成；VM 压测待做 |

**重要边界**：本机是 Linux，没有 MinGW/Windows SDK，**没有产出可运行的 `rospinyin.ime` 二进制**。Windows/ReactOS 上的编译与安装必须在具备工具链的机器或 GitHub Actions 的 `windows-build` job 上完成。本轮用「宿主 ABI stub 语法编译 + 静态契约检查」把 Windows 侧代码的语法与结构风险压到最低，但它不能替代真实构建与实机验证。

## 2. 新增/修改文件

| 路径 | 作用 |
|---|---|
| `ime/imm32_abi.h` | 自带 IME 侧 ABI 声明（16 个导出 + 结构）。**不 include `imm.h`**，避免 SDK/DDK 头差异；输入上下文用自有 `rpimm::InputContext`，彻底不依赖 `HIMC` |
| `ime/rospinyin.h` | UI 类名、布局文本「中文(简体) - RosPinyin」、IME 文件名、HKL 记录位置（单一真源） |
| `ime/rospinyin.def` | 16 个 IMM32 必需导出 |
| `ime/imm_entry.cpp` | 16 个入口的 M1 直通实现；`ImeInquire` 报 `IME_PROP_UNICODE` + `UI_CAP_2700`、私有数据 0 |
| `ime/dllmain.cpp` | 只注册/注销 UI 窗口类；不读注册表、不加载词典、不建窗口、不开线程（M0 红线 #1） |
| `ime/ui_server.cpp` | UI server 窗口类 + 全量 `DefWindowProcW` 转发，M1 完全惰性 |
| `ime/rospinyin_res.rc` / `resource.h` | 版本资源：`FILETYPE=0x3 (VFT_DRV)`、`FILESUBTYPE=0xB (VFT2_DRV_INPUTMETHOD)`、`Translation 0x0804/1200`、中文/英文字符串表（IDS 1024 供 Layout Display Name 引用） |
| `ime/CMakeLists.txt` | Windows 目标产出 `rospinyin.ime`（无 lib 前缀、无 .dll 后缀），静态链接 libgcc/libstdc++ |
| `engine/include/rospinyin/engine.hpp` | 平台无关引擎契约（`feed`/`backspace`/`cancel`/`on_select` → `Result`） |
| `engine/include/rospinyin/session.hpp` | M1 直通会话；M2 替换为拼音引擎时 API 不变 |
| `engine/src/session.cpp` | 引擎静态库实体（M1 仅一个 build-id 符号） |
| `ui/README.md` | 窗口分层约定与 M2/M3 落点、CJK 字体显式指定约束 |
| `tools/install-rospinyin/install.cpp` | 分步安装/卸载工具（见第 3 节） |
| `tools/check_m1.py` | 25 项 M1 契约检查（无第三方依赖），接入 ctest |
| `tools/syntax_stub/{windows.h,imm.h}` | 仅用于 Linux 语法编译的极简 Win32 类型 stub，不链接、不发布 |
| `tests/session_test.cpp` | 1000 键直通 + 50 次 select/deselect 循环、无状态残留 |
| `CMakeLists.txt` / `tests/CMakeLists.txt` / `.github/workflows/ci.yml` | 引擎改为 STATIC 库、Windows 目标接线、CI 增加语法检查与产物名校验 |

## 3. 关键设计决策

1. **新写，不复制蓝本**。`vendor/mzimeja-ref/` 仍是只读参考树（不入库、不构建、不分发）。本轮所有 `ime/` 代码从零写，只对齐 IMM32 的公开 ABI；`tools/check_m1.py` 会扫描假名、罗马字、`mzimeja`/`MZ-IME`/作者名等残留，保证 MIT 代码树干净。
2. **动态 HKL（D3）**。`ImmInstallIMEW` 的返回值被记到 `HKCU\SOFTWARE\RosPinyin\Install`，后续注册按 `%08X` 反查 `HKLM\SYSTEM\...\Keyboard Layouts\<HKL>`；没有硬编码 `0xE0100804`。
3. **不碰 Preload（M0 红线 #2）**。安装器不写 `HKCU\Keyboard Layout\Preload`、不调用 `ActivateKeyboardLayout`/`LoadKeyboardLayoutW`，也不把本 IME 设为默认布局。检查脚本在剥离注释后扫描这些调用，注释里的说明不算绕过。
4. **分步注册（M0 红线 #4）**：`/step1` 只复制文件 → `/step2` 只调 `ImmInstallIMEW` → `/step3` 只写 `Layout Text` → `/step4` 只写 `Layout Display Name`。`/i` 一键连跑仅限 Windows 参照机；**ReactOS 必须每步重启验证**，以便任何一步崩掉都能立刻定位。
5. **DllMain 无害（M0 红线 #1）**。登录路径会加载已注册 .ime；M0 的 0xF4 就出在这一环。M1 的 `DllMain` 只做窗口类注册，且 `ImeInquire` 明确忽略 `IME_SYSINFO_WINLOGON`。
6. **版本资源一次性做对**（WBS 2.2.3）。`VFT_DRV` + `VFT2_DRV_INPUTMETHOD` + zh-CN/Unicode 是 `ImmLoadLayout` 校验的常见翻车点；字符串表 ID 1024 让注册表用 `@%SystemRoot%\system32\rospinyin.ime,-1024` 引用，避免本地化文本散落。
7. **Linux 也能验收 Windows 代码**。`tools/syntax_stub` + `g++ -fsyntax-only` 让 4 个 Windows 源文件在 Linux CI 上做零链接语法编译；`check_m1.py` 把导出表、版本字段、红线、命名一致性变成可回归的断言。

## 4. 验证结果（本轮实测）

```
$ g++ -std=c++17 -Wall -Wextra -fsyntax-only -Itools/syntax_stub -Iengine/include \
      ime/dllmain.cpp ime/imm_entry.cpp ime/ui_server.cpp tools/install-rospinyin/install.cpp
(exit 0, no warnings)

$ python3 tools/check_m1.py
25/25 checks passed

$ cmake -S . -B build -DROSPINYIN_BUILD_TESTS=ON && cmake --build build --parallel
$ ctest --test-dir build --output-on-failure
1/3 smoke ......... Passed
2/3 session ....... Passed   (1000 keys, 50 switches)
3/3 m1-contract ... Passed
100% tests passed out of 3
```

CI 同步更新：Linux job 增加语法编译 + M1 契约；Windows job 现在构建 `rospinyin.ime` 与 `install-rospinyin.exe`（standalone 和 in-tree 两种 configure），并检查产物名。

## 5. 尚未完成（M1 退出标准的另一半）

| 项 | 需要什么 | 归属 |
|---|---|---|
| 真实 Windows 构建 | `windows-latest` CI 或本地 MinGW-w64/MSVC 编译，确认 `.rc`/`.def` 与 `wmain` 链接 | 待 CI 结果 |
| 裸 LoadLibrary 无害验证 | 在 **Windows 参照机**上 `LoadLibraryW(L"rospinyin.ime")` + `FreeLibrary`，确认不崩；再进 ReactOS | M0 红线 #1 验收 |
| #10 VM 分步注册 | ReactOS VM 里依次 `/step1`…`/step4`，每步重启、导出注册表、确认 kbswitch 出现「中文(简体) - RosPinyin」且可切换 | 任务 #10 |
| #11 VM 切换压测 | ReactOS 记事本中激活/停用 50 次，无崩溃无泄漏，普通按键直通；双机行为比对 | 任务 #11 |
| 救援路径 | 若某步触发 0xF4，记录是哪一步，并用 `/u` + 安全模式恢复（沿用 M0 的 qcow2 快照） | 风险预案 |

ReactOS 侧建议操作（沿用 M0 的 QEMU 脚本与 HTTP 共享）：

```
# host: 先在 Windows CI/参照机编出 rospinyin.ime 与 install-rospinyin.exe，放入 m0/share/
python3 -m http.server 8000 --bind 0.0.0.0 --directory m0/share
# guest: 下载到同一目录，然后严格分步（每步之后重启一次）
wget http://10.0.2.2:8000/rospinyin.ime
wget http://10.0.2.2:8000/install-rospinyin.exe
install-rospinyin.exe /status
install-rospinyin.exe /step1     # 复制文件 → 重启 → 确认能进桌面
install-rospinyin.exe /step2     # ImmInstallIMEW → 重启 → 确认能进桌面
install-rospinyin.exe /step3     # Layout Text → 重启 → 确认能进桌面
install-rospinyin.exe /step4     # Layout Display Name → 重启 → 任务栏切换
```

## 6. 已知风险与待验证点

- **`windres` / MSVC `rc.exe` 对 UTF-8 `.rc` 的处理**：已加 `#pragma code_page(65001)`；若中文字符串在 MSVC 下变成乱码，改为 UTF-16LE 带 BOM 的 `.rc` 或转义序列（不改布局文本的字节内容，检查脚本会继续校验一致性）。
- **`std::swprintf(wchar_t*, size_t, ...)`** 在 MSVC 2015+ / MinGW-w64 可用；若某个老工具链缺失，改用 `StringCchPrintfW`。
- **ReactOS 的 `ImmInstallIMEW` 行为**：M0 只解剖了 mzimeja 安装器的调用，未实测系统返回值格式；`/step2` 会把返回值原样打印并写注册表，便于现场核对。
- **codepage 1200 的 Translation 块**：Windows 常见 IME 用 `040904B0`（en-US/Unicode），本项目用 `080404B0`（zh-CN/Unicode）；若 ReactOS 的 `ImmLoadLayout` 只认前者，需要补一个 en-US 块（不改 FILETYPE/FILESUBTYPE）。
- **CI 的 Windows job 尚未实际跑过**（远端仓库 push 后触发）；Linux job 的三项检查已在本机复现通过。

---

_下一步：把本轮改动提交并推送到 `twipty/ros-chinese-ime`，观察 Windows CI；随后进入 VM 分步注册与切换压测。_
