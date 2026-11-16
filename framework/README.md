# framework/ — 可复用输入法框架（imfw）

2026-11-16 变更目标落地：把输入法引擎宿主（不含拼音码表与选字逻辑）、UI 窗口层和输入法工具条从 RosPinyin 中独立出来，形成**任何 ReactOS IMM32 输入法都能复用**的框架。拼音实现退化为第一个「插件」；形码产品线 RosXingMa（见 `xingma/XINGMA-PLAN.md`）将是第二个。

## 模块地图

| 子目录 | 内容 | 平台 |
|---|---|---|
| `include/imfw/` | 引擎契约（`engine.hpp`：`Action/Candidate/Result/Engine`，`Result.segments` 携带切分视图）、通用码表（`code_table.hpp` + `dat_format.hpp`）、候选分页（`candidate_pager.hpp`）、**Scheme 插件接口**（`scheme.hpp`）、直通会话（`session.hpp`）、框架版本 | 纯 C++，Linux 可测 |
| `src/` | 引擎 build id、`CodeTable`（dat 读取 + key 索引）、`CandidatePager` | 纯 C++，Linux 可测 |
| `host/` | IMM32 16 导出（`imm_entry.cpp`，通用键处理：翻页/选字/退格/Esc/上屏）、`dllmain`（M0 红线：只注册 UI 类）、UI server 窗口类、按 HIMC 的会话存储（Win32 `CRITICAL_SECTION`）、dat 延迟加载 | Windows/ReactOS |
| `ui/` | 窗口基类 + CJK 字体兜底链、组合串窗口、候选窗、**输入法工具条**（`toolbar.*`，右下角浮动、显示 Scheme 标签） | Windows/ReactOS |

## Scheme 插件接口（复用缝）

```cpp
class imfw::Scheme {
    const wchar_t* dat_filename() const;        // 码表文件名，如 L"rospinyin.dat"
    const wchar_t* ui_server_class() const;     // IMM32 注册的 UI server 类名
    const wchar_t* composition_class() const;   // 组合串窗口类名
    const wchar_t* candidate_class() const;     // 候选窗类名
    const wchar_t* toolbar_class() const;       // 工具条类名
    const wchar_t* toolbar_label() const;       // 工具条标签，如 L"拼"
    char32_t input_char(uint32_t vk) const;      // 输入法字母表：VK→码点，0=非输入键
    bool is_input_char(char32_t cp) const;
    unique_ptr<Engine> create_engine(const CodeTable&) const;  // 每 HIMC 一个引擎
    bool load_fallback(CodeTable&) const;       // dat 缺失时的兜底码表
};
Scheme& imfw::scheme_instance();                // 由装配模块定义（ime/scheme_reg.cpp）
```

框架拥有所有 IME 的公共行为：数字/空格选字（经 `CandidatePager`，翻页后数字选当前页）、`-`/`=`/`,`/`.` 翻页、退格、Esc 取消、`WM_IME_CHAR` 上屏、组合串/候选窗渲染与插入符跟随、工具条生命周期。Scheme 只回答「这个键是不是我的输入」「我的码表文件叫什么」「我的引擎是谁」。

## 如何接入一个新的输入法（以 RosXingMa 形码产品线为例）

1. **实现 Scheme**：`ShapeScheme : imfw::Scheme`（`input_char` 限方案字母表、`dat_filename()`、`toolbar_label()`、`create_engine` 返回自己的引擎；形码多表管理自带 `ShapeStore`，忽略框架传入的 CodeTable，见 `xingma/XINGMA-PLAN.md` 3.4 G2）。
2. **实现 Engine**：`imfw::Engine` 的 `feed/backspace/cancel/on_select`，`Result.segments` 填入已敲的形码（组合串窗口会自动画下划线）；候选按 `Result.candidates` 输出即可复用整套翻页/选字。
3. **词组/多码点上屏已支持**：`emit_text()` 按码点逐条生成 `WM_IME_CHAR`（UTF-16 码单元，非 BMP 走代理对；上限 32 条/调用），候选文本不再截断首字（xingma 缺口 G1 已补齐）。
4. **装配 .ime**：仿照 `ime/`：`scheme_reg.cpp`（定义 `scheme_instance()`）+ 品牌资源（def/rc/图标）+ CMake 链接 `framework` 的 host/ui 源码。窗口类名互不相同，两个 IME 可在同一进程共存。

## 生命周期与红线（对所有复用者生效）

- `DllMain` 只注册 UI server 窗口类（M0 红线 #1）；`scheme_instance()` 在此只构造平凡静态对象。
- dat 加载、引擎构造、窗口创建全部发生在第一次按键（`session_store`/`ime_ui` 的函数内静态）。
- 锁一律用 Win32 `CRITICAL_SECTION`，禁止 `std::mutex`（MinGW POSIX 线程模型会把 `libwinpthread` 带进 PE，CI 导入表断言会拒绝）。
- `tools/check_m1.py` 静态检查框架核心不含拼音知识，防止复用缝被悄悄污染。

## 与拼音插件的边界

拼音相关的一切都在 `pinyin/`：读音归一化（`ü→v`、声调数字紧随韵母）、音节切分（最长匹配 + 回溯）、`PinyinSession`、`PinyinScheme` 与内置兜底码表。框架侧 grep 不到 "pinyin"。
