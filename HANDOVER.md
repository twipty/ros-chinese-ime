# RosPinyin 接手文档（Handover）

> 面向对象：接手的维护者 / 新协作者
> 版本基线：`main` = `3accb57`（2026-11-17）
> 阅读顺序建议：第 1–3 章了解"是什么、到哪了"→ 第 6 章跑起来 → 第 5/8 章读代码 → 第 10 章避开坑

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
| **M2** | **单字拼音引擎（核心里程碑）** | 🚧 进行中，宿主侧完成 + 数据链路就绪；**VM 按键拦截待解**（见 §11.1） |
| M3 | 中文体验补齐（标点、全角、Shift 临时英文） | 未开始 |
| M4 | 词组与词频 | 未开始 |
| M5 | 整句与自学习 | 未开始 |
| M6 | 设置 UI | 未开始 |
| M7 | 遗留场景修复（含上游往返） | 未开始 |
| M8 | 收尾与发布 | 未开始 |

> 详细 WBS 见 `PLAN.md`，阶段目标见 `ROADMAP.md`。

### 1.7 第二条产品线：RosXingMa（形码）

`xingma/` 是复用同一框架的第二款输入法（形码/五笔类）。**当前只完成规划与文档，W1 编码未开始**。
边界规则：只有形码需要的东西放 `xingma/`；拼音也要的属于框架层，留在 `framework/`。形码侧**不得修改**框架结构/构建/文档。

---

## 2. 整体架构

### 2.1 分层

```
┌──────────────────────────────────────────────────────────────┐
│  应用（记事本 / EDIT 控件）                                    │
└───────────────▲──────────────────────────────────────────────┘
                │ WM_IME_CHAR（每个 UTF-16 码单元一条）
┌───────────────┴──────────────────────────────────────────────┐
│  ReactOS IMM32（win32ss/user/imm32）                          │
│    ImmProcessKey → ImeProcessKey                              │
│    ImmTranslateMessage → ImeToAsciiEx(TRANSMSGLIST*)          │
└───────────────▲──────────────────────────────────────────────┘
                │ 16 个导出（ime/rospinyin.def）
┌───────────────┴──────────────────────────────────────────────┐
│  framework/host —— 可复用宿主层（imfw）                        │
│    imm_entry.cpp   通用键处理：退格/Esc/选字/翻页/上屏          │
│    imm32_abi.h     IME 侧 ABI（含 TRANSMSGLIST）              │
│    session_store   每 HIMC 一个会话 + reload_sessions()        │
│    ui_server/dllmain  UI server 窗口类注册（DllMain 只做这个）  │
├──────────────────────────────────────────────────────────────┤
│  framework/ui —— 组合串窗 / 候选窗 / 工具条（自绘，不依赖 DDK）  │
├──────────────────────────────────────────────────────────────┤
│  framework/include/imfw + src —— 纯 C++ 引擎契约与工具          │
│    engine.hpp   Action/Candidate/Result/Engine                 │
│    scheme.hpp   Scheme 插件接口（复用缝）                       │
│    emit_text    多码点上屏 → WM_IME_CHAR 序列                   │
│    candidate_pager / utf8 / code_table / dat_format            │
├──────────────────────────────────────────────────────────────┤
│  Scheme 插件（每个输入法一个）                                  │
│    pinyin/  —— PinyinScheme + PinyinSession + 自有数据源        │
│    xingma/   —— ShapeScheme（待实现）                          │
└──────────────────────────────────────────────────────────────┘
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
  → process_key()：选字键/翻页键（策略来自 Scheme）→ 退格 → Esc → 字母 feed 给 Engine
  ↓
Engine::feed(cp) → Result{action, composition, segments, candidates, commit_text}
  ↓
Absorb：刷新组合串窗/候选窗（framework/ui）
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

| 项 | 状态 |
|---|---|
| 框架宿主层（16 导出、键路由、UI 窗口、会话存储） | ✅ 完成并实测（M1） |
| 框架 G1–G4 缺口 | ✅ 全部在框架侧解决（多码点上屏、零码表+热重载、配置入口、键策略/UTF-8 组合串） |
| 审计 P0（TRANSMSGLIST ABI）+ P2（reload 顺序） | ✅ 2026-11-17 修复，`check_m1.py` 新增 3 条断言防回归 |
| 拼音引擎（切分、候选、`v` 键、声调转换） | ✅ 宿主单测全覆盖 |
| `rospinyin.dat` 数据链路（Unihan 18.0 + hanziDB 字频 → mkdat） | ✅ 44,355 字 / 418 音节，可复现构建 |
| Win32 产物 CI（MinGW i686 → `m1-artifacts` 分支） | ✅ 三 job 全绿 |
| **VM 内 M2 端到端验收** | ⛔ **阻塞**：托盘红色「中」可见，但按键未被 IME 拦截（字母直通记事本），15 项用例无法开跑 |

阻塞点详情与排查方向见 §11.1。

---

## 4. 目录结构

```
ros-pinyin/
├── CMakeLists.txt              顶层构建：framework + pinyin + (Windows) ime/安装器/httpdl + mkdat + tests
├── README.md                   项目章程（目标、立项依据、路线决策、非目标）
├── PLAN.md                     工作分解结构（WBS，M0–M8 逐条任务）
├── ROADMAP.md                  里程碑目标与工期
├── M0-REPORT.md / M1-REPORT.md 里程碑验收报告（含实测证据与决策记录）
├── NOTICE.txt                  第三方与数据分层授权
├── LICENSE                     MIT
├── framework/                  ★可复用 IMM32 输入法框架（imfw）
│   ├── README.md               模块地图、Scheme 复用缝、生命周期红线
│   ├── include/imfw/           引擎契约、Scheme 接口、emit_text、candidate_pager、utf8、code_table、dat_format
│   ├── src/                    上述纯 C++ 实现（Linux 可单测）
│   ├── host/                   IMM32 16 导出、ABI 头、会话存储、UI server、DllMain
│   └── ui/                     组合串窗 / 候选窗 / 工具条 / 窗口基类
├── pinyin/                     ★拼音插件（框架不含拼音知识）
│   ├── include/rospinyin/      pinyin.hpp、pinyin_session.hpp、dat_source.hpp、scheme.hpp、version.hpp
│   ├── src/                    读音归一化、切分、会话、Scheme、宿主兜底数据源
│   └── host/dat_source_win.cpp Win32 数据源（读 system32\rospinyin.dat）
├── ime/                        装配层：rospinyin.def（16 导出）、rospinyin_res.rc、scheme_reg.cpp、图标
├── tests/                      10 个宿主测试（ctest）
├── tools/
│   ├── check_m1.py             契约静态检查（50 项，无第三方依赖）
│   ├── mkdat/                  数据编译器 + fetch_data.py + fixtures
│   ├── install-rospinyin/      安装器（分步注册，无 Preload 红线）
│   ├── httpdl/                 全静态 HTTP 下载器（guest 侧 wget/curl 替身）
│   └── syntax_stub/            宿主侧 Win32 头桩（Linux 语法检查用）
├── m0/vm/                      QEMU/ReactOS 实验环境（qcow2、启动脚本、noVNC、证据、日志）
├── m2/                         M2 验收操作单 + 数据方案
├── xingma/                     形码产品线 RosXingMa（规划/规范/数据包模板，代码待 W1）
├── engine/include/             （历史遗留空目录，实体已并入 framework/）
├── libernt/                    未纳入版本控制的本地探针材料（.gitignore 外，可忽略）
└── .github/workflows/ci.yml    三 job CI：Linux 测试 / MinGW 构建 / 产物发布
```

---

## 5. 核心模块职责

| 模块 | 关键文件 | 职责 | 平台 |
|---|---|---|---|
| ABI 声明 | `framework/host/imm32_abi.h` | 16 导出原型、`TransMsg`/`TransMsgList`、IMEINFO、属性位 | Win |
| 导出实现 | `framework/host/imm_entry.cpp` | 键路由、选字/翻页、上屏（`emit_text`）、`ImeConfigure`/`ImeSelect`/`ImeSetActiveContext` | Win |
| 会话存储 | `framework/host/session_store.cpp` | 每 HIMC 一个 `ImeSessionState`；`reload_sessions()` data-first | Win |
| DllMain | `framework/host/dllmain.cpp` | **只注册 UI server 窗口类**（M0 红线 #1） | Win |
| UI server | `framework/host/ui_server.cpp` | 接收 IMM32 的 UI 消息并转发窗口层 | Win |
| 窗口层 | `framework/ui/*` | 组合串（UTF-8 下划线切分）、候选窗（9 条/页）、工具条 | Win |
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
| 契约检查 | `tools/check_m1.py` | 50 项静态断言（SPDX、16 导出、DllMain 无害、无 Preload、ABI、reload 顺序…） | 宿主 |

---

## 6. 构建与运行

### 6.1 主机（Linux）构建与测试 —— 日常开发主通道

```bash
cd ros-pinyin
cmake -S . -B build -DROSPINYIN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure        # 10 个测试，应 100% 通过
python3 tools/check_m1.py                          # 50 项契约检查，应 50/50
```

- 宿主构建**不编译** Windows 专有部分（`ime/`、安装器、`httpdl`、`framework/host`、`framework/ui`），只测纯 C++ 层。
- 产物：`build/tests/rospinyin-*`、`build/tools/mkdat/mkdat`。

### 6.2 数据构建（`rospinyin.dat`）

```bash
cmake --build build --target mkdat
bash tools/mkdat/build_dat.sh            # 默认输出 build/m2-data/rospinyin.dat
```

- 输入源由 `tools/mkdat/fetch_data.py` 按 **sha256 固定版本**拉取（Unihan 18.0.0 `kMandarin` + hanziDB 字频），可离线复用已下载文件。
- 输出确定性（同输入字节级一致），44,355 字 / 418 音节。
- 数据格式见 `framework/include/imfw/dat_format.hpp`（magic `ROSPYDAT`、v1、header + CharEntry[] + SylEntry[] + 字符串池，无指针、可 mmap）。

### 6.3 Win32 / ReactOS 目标（MinGW-w64 i686）

CI 上（`.github/workflows/ci.yml`）跑两套配置：

```bash
cmake -S . -B build-standalone -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres -DROSPINYIN_BUILD_TESTS=OFF
cmake -S . -B build-intree -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres \
      -DROSPINYIN_IN_TREE=ON -DROSPINYIN_BUILD_TESTS=OFF
cmake --build build-standalone --parallel
cmake --build build-intree --parallel
```

**工具链硬约束**（历史踩坑换来的）：

- 必须 **MinGW-w64 i686**，不能用 MSVC：MSVC 静态 CRT 会导入 `kernel32!InitializeCriticalSectionEx`，ReactOS 0.4.16 没有该导出（装任何运行库都补不上）。
- 产物只允许依赖 `kernel32 / user32 / advapi32 / imm32 / msvcrt`；CI 用 `objdump -p` 断言拒绝 `vcruntime140 / api-ms-win-crt / libstdc++-6 / libgcc_s / libwinpthread`。
- 因此**禁止 `std::mutex` / `std::thread`**，一律用 Win32 `CRITICAL_SECTION`。
- 图标以 base64 形式入库（`ime/rospinyin.ico.b64`），CI 构建前解码成 `.ico` 再交给 windres（`push_files` 只能传文本）。

### 6.4 安装到 ReactOS

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

### 6.5 VM 运行与观察

```bash
cd m0/vm
./run-reactos.sh          # 从硬盘引导，日常入口
# 或 ./run-reactos-install.sh  （带安装 ISO，首次装系统用）
```

- 关键参数在 `m0/vm/common.sh`：`-m 512 -machine pc,accel=kvm:tcg`、`rtl8139` + SLIRP 用户网络、`usb-tablet`、`-serial file:logs/rosdbg.log`、`-monitor unix:monitor.sock`、`-vnc 127.0.0.1:1`。
- 三条观察通道：
  1. **VNC** `127.0.0.1:5901`（display `:1`）；
  2. **noVNC**（浏览器）：`python -m websockify --web noVNC-1.5.0 6080 127.0.0.1:5901`，访问 `http://<host>:6080/vnc.html`；或直接跑 `m0/vm/noVNC-1.5.0/utils/novnc_proxy`；
  3. **串口日志** `m0/vm/logs/rosdbg.log`（ReactOS KDBG/DbgPrint 默认走 COM1）。
- **传文件**：宿主机 `python3 -m http.server 8000 --bind 0.0.0.0 --directory m0/share`，guest 访问 `http://10.0.2.2:8000/`（SLIRP 内建）。
- **截屏**：`hmp "screendump out.ppm"`（`common.sh` 提供 `hmp` 函数，走 monitor.sock）。
- **控制**：`hmp "quit"` 优雅关机；若 monitor.sock 失效，`kill -TERM <qemu pid>`。
- ⚠️ 这些服务（QEMU / websockify / http.server）**交接时应确认已停止**（本次交接已全部关闭并核对端口 5901/8000/6080 无监听）。

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

UI 窗口有一条 CJK 字体兜底链（见 `framework/ui/ui_window.cpp`）：`Microsoft YaHei → SimSun → Noto Sans SC → 系统默认`。
VM 证据用 `m0/share/NotoSansSC-subset.ttf`。

---

## 8. 关键流程

### 8.1 一次按键（最常打交道的路径）

1. IMM32 判 IME 打开 → `ImeProcessKey` → `should_consume_key()`：
   - Ctrl/Alt 按下 → 放行（应用快捷键可用）；
   - `Scheme::input_char(vk)` 命中字母表 → 返回 TRUE；
   - 退格/Esc → 仅组合中才拦截；
   - 选字/翻页键 → 仅"组合中且有候选"才拦截。
2. `ImeToAsciiEx` → `process_key()`：
   - 选字键 → `pager.select(digit)` 取候选 → `finish_composition()` → `emit_text()`；
   - 翻页键 → `pager.next/prev_page()` → 刷新 UI，返回 0；
   - 退格 → `engine->backspace()`；
   - Esc → `finish_composition()`；
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

`.github/workflows/ci.yml` 三个 job：

| job | 内容 |
|---|---|
| `Engine tests + M1 contract (Linux)` | ① 宿主侧 Win32 语法检查（`g++ -fsyntax-only` + `tools/syntax_stub`）② cmake 构建 ③ ctest ④ 构建 `rospinyin.dat` ⑤ 上传 `rospinyin-m2-data` |
| `MinGW i686 / ReactOS targets` | msys2 MINGW32 下两套配置构建 + PE 导入表断言 + 暂存 `stage/` + 上传 `rospinyin-m1-win32` |
| `Publish VM test binaries` | 仅 `main` push 触发：把二进制与数据提交到 **`m1-artifacts` 分支**（force push，最新一次覆盖），供 VM 通过 GitHub contents API 拉取 |

`m1-artifacts` 分支里的 `INFO.txt` 记录 `source commit` 与构建时间 —— **VM 验收前先核对它等于你期望的提交**。

### 8.6 VM 验收（M2）

按 `m2/M2-ACCEPTANCE.md` 执行：产物入 system32 → 重启 → Alt+Shift 切到「中」→ 记事本 → 15 项用例（含词组上屏 ≥2 字，这是唯一能在真机暴露 TRANSMSGLIST ABI 缺陷的用例）。结果回填表格并截图存 `m0/vm/evidence/`。

---

## 9. 测试与质量门禁

| 门禁 | 命令 | 通过标准 |
|---|---|---|
| 宿单单测 | `ctest --test-dir build` | 10/10（smoke、session、pinyin、pinyin-session、candidate-pager、pinyin-scheme、emit-text、utf8、m1-contract、mkdat-e2e） |
| 契约静态检查 | `python3 tools/check_m1.py` | 50/50（SPDX、16 导出、DllMain 无害、无 Preload、TRANSMSGLIST ABI、data-first reload、框架不含拼音知识…） |
| Win32 语法检查 | CI 的第一步，见 §6.3 | 0 error（用 `tools/syntax_stub` 提供的 Windows 头） |
| PE 导入表 | CI `objdump -p` | 不得出现 Vista+ 导出或非系统 DLL |
| 真机验收 | `m2/M2-ACCEPTANCE.md` | 15 项全绿 |

> 本地若装了 `ziglang`，可用真实 MinGW 头做交叉验证（比语法桩更可信）：
> `zig c++ -target x86-windows-gnu -c -o /tmp/x.o -I framework/host -I framework/include ...`
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

**闭环要求**：修复后重跑 `m2/M2-ACCEPTANCE.md` 全部 15 项，重点词组上屏（≥2 字）。

### 11.2 其他待办

- M3：中文标点、全角/半角、Shift 临时英文；
- M4：词组与词频（数据结构需扩展 `rospinyin.dat`，注意版本号与向后兼容）；
- 语言栏菜单项（属性/关于）留待 M6；
- `xingma/` W1 编码（`ShapeScheme`/`ShapeStore`/`ShapeDatLoader`）尚未开始，框架侧缺口已全部关闭，可直接复用宿主层；
- 历史遗留空目录 `engine/include/` 可清理（实体已并入 `framework/`）。

---

## 12. 维护者日常清单

**改代码**

1. `cmake --build build --parallel && ctest --test-dir build` → 10/10；
2. `python3 tools/check_m1.py` → 50/50；
3. 动了 `framework/host` 或 `framework/ui`：跑 CI 那步宿主语法检查（§6.3），确认 `-Wchanges-meaning` / ABI 断言不炸；
4. 动了 ABI：本地用 zig 真实 MinGW 头编译一次 `framework/host/imm_entry.cpp`（`-target x86-windows-gnu -c`）。

**加一个输入法（如形码）**

1. 实现 `imfw::Scheme` + `imfw::Engine`（参考 `pinyin/src/scheme.cpp`、`pinyin_session.cpp`）；
2. 装配 `scheme_reg.cpp` 定义 `scheme_instance()`；
3. 窗口类名、工具条标签、`.def`、资源、CMake 目标仿 `ime/`；
4. 数据自管（框架不传表），并在 `reload_data()` 里实现换代。

**发版/验收**

1. push 到 `main` → CI 三个 job 绿 → `m1-artifacts` 更新（核对 `INFO.txt` 的 source commit）；
2. VM 拉取产物 → 重启 → 跑 `m2/M2-ACCEPTANCE.md`；
3. 更新 `M2-REPORT.md`（若新建）与 `PLAN.md`/`ROADMAP.md` 状态。

**交接/收尾**

- 关闭 QEMU、websockify、http.server（见 §10.5 的 systemd-run 方法），确认 5901/8000/6080 无监听；
- VM 证据归档到 `m0/vm/evidence/`，并更新当日工作日志。

---

## 13. 参考文档索引

| 文档 | 内容 |
|---|---|
| `README.md` | 项目章程：使命、立项依据、路线 A/B 决策、产品形态 |
| `PLAN.md` | WBS 全量任务与工期 |
| `ROADMAP.md` | 里程碑目标 |
| `M0-REPORT.md` | ReactOS/QEMU 环境、IMM32 可行性、BSOD 根因、许可证时间线 |
| `M1-REPORT.md` | 16 导出骨架、分步注册、动态 HKL、托盘图标修复、MinGW 切换 |
| `framework/README.md` | 框架模块地图、Scheme 复用缝、G1–G4 处置、生命周期红线 |
| `pinyin/README.md` | 拼音插件文件职责与数据所有权 |
| `m2/M2-ACCEPTANCE.md` | 15 项真机验收操作单（含判定口径） |
| `m2/M2-DATA-PLAN.md` | 数据源选型、声调/`ü` 处理、规模核算 |
| `m0/vm/NOVNC-GUIDE.md` | VM 人机协作操作单（v7，含所有键位坑与图标修复说明） |
| `xingma/XINGMA-PLAN.md` | 形码产品线正式计划（1.0） |
| `xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` | 框架能力审计（G1 ABI P0 / G2 顺序 P2）与处置记录 |
| `NOTICE.txt` | 第三方与数据分层授权 |

---

*本文档由维护者交接时编写，随代码同步更新。若发现与代码不符，以代码与 CI 结果为准，并回来修文档。*
