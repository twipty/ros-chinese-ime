# framework/ — 可复用输入法框架（imfw）

2026-11-16 变更目标落地：把输入法引擎宿主（不含拼音码表与选字逻辑）、UI 窗口层和输入法工具条从 RosPinyin 中独立出来，形成**任何 ReactOS IMM32 输入法都能复用**的框架。拼音实现退化为第一个「插件」；形码产品线 RosXingMa（见 `xingma/XINGMA-PLAN.md`）将是第二个。

2026-11-17 数据所有权下放（G2 终局形态）：**框架不再加载、持有或传递码表**。每个 Scheme 完全自管数据（拼音用 `imfw::CodeTable`、形码用 `ShapeStore`），`create_engine()` 无参；`CodeTable`/`dat_format.hpp` 作为**通用工具类**保留在框架中供各方案按需使用，但框架自身不实例化它们。

## 模块地图

| 子目录 | 内容 | 平台 |
|---|---|---|
| `include/imfw/` | 引擎契约（`engine.hpp`：`Action/Candidate/Result/Engine`，`Result.segments` 携带切分视图，composition/segments 为 UTF-8）、通用码表**工具类**（`code_table.hpp` + `dat_format.hpp`，框架不实例化，供方案选用）、候选分页（`candidate_pager.hpp`）、**Scheme 插件接口**（`scheme.hpp`）、**键策略契约**（`host.hpp`：`KeyEvent`/`KeyResult`/`Host`，见下）、**键意图词汇**（`intent.hpp`：`IntentKind`/`Intent` + `route_key()`，`status: draft`）、会话状态（`session_state.hpp`）、上屏发射（`emit_text.hpp`：每 UTF-16 码单元一条 `WM_IME_CHAR`）、UTF-8 解码（`utf8.hpp`）、直通会话（`session.hpp`）、框架版本 | 纯 C++，Linux 可测 |
| `src/` | 引擎 build id、`CodeTable`（dat 读取 + key 索引，工具类）、`CandidatePager`、`emit_text`（G1 多码点上屏，含非 BMP 代理对）、**`host_key`（键策略：`wants_key` 归属判定 + `handle_key` 按 Intent 分派）**、**`key_route`（X2-c：`route_key()` —— 一个键 = 一个 `Intent`，纯函数，不问 session/engine/host）**、`utf8`（G4 组合串 UTF-8 解码，坏字节按字节兜底） | 纯 C++，Linux 可测 |
| `platform/win32/imm32/` | IMM32 16 导出（`imm_entry.cpp`：只把原生消息翻译成 `KeyEvent`、把 `KeyResult` 翻译回 `TRANSMSGLIST`，判定在 `host_key`；`ImeConfigure` 回调）、`dllmain`（M0 红线：只注册 UI 类）、UI server 窗口类、按 HIMC 的会话存储（含 `reload_sessions()` 热重载，重载前先调 `Scheme::reload_data()`） | Windows/ReactOS |
| `platform/win32/ui/` | 窗口基类 + CJK 字体兜底链、组合串窗口（下划线按 UTF-8 切分宽度）、候选窗、**输入法工具条**（`toolbar.*`，右下角浮动、显示 Scheme 标签） | Windows/ReactOS |

`platform/` 陈述事实而非意图：这部分代码就是 Win32 + IMM32，没有抽象层。
将来真有第二个前端，它作为 `platform/` 下的**并列目录**加进来即可，旧名不用动
（`docs/plan/PLAN-MODULARIZATION.md` §3 说明了为什么不用 `frontend/`）。

## Scheme 插件接口（复用缝）

```cpp
class imfw::Scheme {
    const wchar_t* ui_server_class() const;     // IMM32 注册的 UI server 类名
    const wchar_t* composition_class() const;   // 组合串窗口类名
    const wchar_t* candidate_class() const;     // 候选窗类名
    const wchar_t* toolbar_class() const;       // 工具条类名
    const wchar_t* toolbar_label() const;       // 工具条标签，如 L"拼"
    char32_t input_char(uint32_t vk) const;      // 输入法字母表：VK→码点，0=非输入键
    bool is_input_char(char32_t cp) const;
    bool is_selection_key(char32_t ch) const;    // 选字键（默认：空格 + 数字 1-9，可覆盖）
    int page_key(char32_t ch) const;             // 翻页方向：+1/-1/0（默认：- , 上一页，= . 下一页）
    unique_ptr<Engine> create_engine() const;    // 每 HIMC 一个引擎，数据由方案自给
    void reload_data() const;                    // 数据热重载钩子（默认空；ImeConfigure 成功/重载前调用）
    bool configure(void* parent) const;          // ImeConfigure 回调（G3，默认拒绝）
};
Scheme& imfw::scheme_instance();                // 框架定义：经 SchemeRegistry 查询（X2-a0）
Scheme* imfw::scheme_factory();                  // 由装配模块定义（ime/scheme_reg*.cpp）
```

**注入缝（X2-a0，审计 A-P0-2）**：`scheme_instance()` 不再是"由装配件定义的符号"，而是框架对 `imfw::SchemeRegistry` 的查询：

- **创建侧** `scheme_factory()` 由装配件提供，决定"这个二进制是哪个输入法"（`ime/scheme_reg.cpp` = 拼音，`ime/scheme_reg_toy.cpp` = 玩具方案）；
- **查询侧** `SchemeRegistry::get()` 先看有没有 `install()` 过的方案，没有再问工厂，工厂结果缓存一次；
- `install(Scheme*)` 是给测试/宿主用的注入点——塞假 scheme 不必改框架、也不必链接一个懂拼音的装配件。

改造前"换个方案"只有两条路：换一个 `scheme_reg.cpp` 重编，或者改框架。现在换成 `-DROSPINYIN_SCHEME=toy`，`samples/toy-scheme/` 就是那个只加了一个文件夹的第二个方案（判据 G4）。

**键策略与宿主分离（X2-a）**：`process_key()` 曾经把判定和四件平台事（读 `lpbKeyState`、
`ImmGetOpenStatus`、开窗口、写 `TRANSMSGLIST`）混在一个 TU 里，结果是 IME 里执行最多的
90 行**宿主侧零覆盖**。现在：

| 层 | 位置 | 职责 |
|---|---|---|
| 契约 | `include/imfw/host.hpp` | `KeyEvent`（vk / ch / kind / key_up / ctrl / alt）、`KeyResult`（commit）、`Host`（open 读写、show/hide、can_render、2 个默认空诊断钩子） |
| 策略 | `src/host_key.cpp` | `wants_key()` 回答"这个键归不归我"，`handle_key()` 执行选字/翻页/退格/Esc/喂键 |
| 适配器 | `platform/win32/imm32/imm_entry.cpp` | `key_event_from()` 做 vk→`KeyKind`/字符翻译；`Win32Host` 实现 `Host` |

`KeyKind` 的存在是为了让策略里**不出现任何 `VK_*` 常量**（`<windows.h>` 进不来），
`wants_key` 收非 const 的会话是因为 lone-Shift 的闩必须在归属路径里翻——`ImeProcessKey`
返回 `FALSE` 时 IMM32 不会调 `ImeToAsciiEx`，把切换挪进 `handle_key` 会让 Shift 永远不生效。
`tests/host_key_test.cpp` 用假 engine + 假 host 跑全部判定分支，不需要 `<windows.h>`、不需要 GUI。

框架拥有所有 IME 的公共行为：选字/翻页（键位策略由 Scheme 定义，默认数字/空格选字、`-`/`=`/`,`/`.` 翻页；仅在「组合中且有候选」时拦截，其余场合这些键直达应用）、退格、Esc 取消、`WM_IME_CHAR` 上屏（多码点、非 BMP 代理对）、组合串/候选窗渲染与插入符跟随、工具条生命周期。Scheme 回答「这个键是不是我的输入」「我的引擎是谁」，并**完全拥有自己的数据**——框架里没有码表对象。

## 框架能力缺口（xingma 缺口单）的处置记录

四个缺口（`xingma/XINGMA-PLAN.md` 3.4）全部在框架侧解决，形码产品线无需自建宿主层：

- **G1 多码点上屏**：`imfw::emit_text()`（`emit_text.hpp/.cpp`，imfw-engine 内、Linux 可测）按 UTF-16 码单元逐条生成 `WM_IME_CHAR`，非 BMP 走代理对，上限 32 条且不产生半个代理对。宿主层经 `static_assert` 保证 `TransMsgSlot` 与 `TRANSMESSAGE` 布局兼容后转发。
- **G2 多表/热切换**（2026-11-17 终局形态）：**框架不持有任何码表**。曾有过的 `dat_filename()`/`load_fallback()` 哨兵值契约已被移除；`create_engine()` 无参，引擎的数据由 Scheme 自己给；`reload_sessions()` 先调 `Scheme::reload_data()` 让方案刷新数据、再丢弃全部会话（`ImeConfigure` 成功后自动调用），配置工具改包/换表即刻生效。`CodeTable`/`dat_format.hpp` 保留为通用工具类（拼音在用），形码用自己的 `ShapeStore`。
- **G3 配置入口**：`Scheme::configure(void* parent)` 回调接进 `ImeConfigure`；返回 `true` 即视为已处理并触发重载，默认维持 `FALSE`。语言栏菜单项仍留待 M6。
- **G4 键策略与组合串**：选字/翻页键下放 Scheme（默认保持原行为）；组合串与 `segments` 契约为 UTF-8（ASCII 不变），框架统一经 `imfw::utf8_to_wide` 解码渲染，坏字节按字节兜底不丢数据。「无候选时空格上屏原文」继续由引擎侧实现（`Action::Commit` 原文），框架保证此时空格/标点直达应用。

## 如何接入一个新的输入法（以 RosXingMa 形码产品线为例）

1. **实现 Scheme**：`ShapeScheme : imfw::Scheme`（`input_char` 限方案字母表；`create_engine()` 里构造引擎并把它接到自己的 `ShapeStore` 数据上——框架不传任何表，见 `xingma/XINGMA-PLAN.md` 3.4 G2）。
2. **实现 Engine**：`imfw::Engine` 的 `feed/backspace/cancel/on_select`，`Result.segments` 填入已敲的形码（组合串窗口会自动画下划线）；候选按 `Result.candidates` 输出即可复用整套翻页/选字。
3. **词组/多码点上屏已支持**：`emit_text()` 按码点逐条生成 `WM_IME_CHAR`（UTF-16 码单元，非 BMP 走代理对；上限 32 条/调用），候选文本不再截断首字（xingma 缺口 G1 已补齐）。
4. **按需覆盖键策略与配置**：`is_selection_key()`/`page_key()` 默认即经典行为，形码如需自定翻页键直接覆盖；`configure()` 返回 `true` 即接管输入法面板的「属性」入口（G3），随后框架自动调 `reload_data()` + 重建会话。
5. **装配 .ime**：仿照 `ime/`：`scheme_reg.cpp`（定义 `scheme_factory()`，返回本方案的静态实例）+ 品牌资源（def/rc/图标）+ CMake 链接 `framework` 的 host/ui 源码。窗口类名互不相同，两个 IME 可在同一进程共存。

## 生命周期与红线（对所有复用者生效）

- `DllMain` 只注册 UI server 窗口类（M0 红线 #1）；`scheme_instance()` 在此只构造平凡静态对象——经注册表工厂构造，构造点仍是同一个函数内静态，装载期不做别的。
- 引擎构造、窗口创建全部发生在第一次按键（`session_store`/`ime_ui` 的函数内静态）；数据加载是 Scheme 自己的事（拼音在其构造与 `reload_data()` 里，同样不在 DllMain）。
- 锁一律用 Win32 `CRITICAL_SECTION`，禁止 `std::mutex`（MinGW POSIX 线程模型会把 `libwinpthread` 带进 PE，CI 导入表断言会拒绝）。
- `tools/check_m1.py` 静态检查框架核心不含拼音知识，防止复用缝被悄悄污染。

## 与拼音插件的边界

拼音相关的一切都在 `pinyin/`：读音归一化（`ü→v`、声调数字紧随韵母）、音节切分（最长匹配 + 回溯）、`PinyinSession`、`PinyinScheme`、内置兜底码表与**自己的数据源**（`pinyin/host/dat_source_win.cpp` 读系统目录 .dat，`pinyin/src/dat_source.cpp` 提供宿主测试兜底）。框架侧 grep 不到 "pinyin"，也没有任何码表加载代码。
