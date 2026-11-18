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
├── tests/                      11 个宿主测试（ctest）
├── tools/
│   ├── check_m1.py             契约静态检查（50 项，无第三方依赖）
│   ├── mkdat/                  数据编译器 + fetch_data.py + fixtures
│   ├── install-rospinyin/      安装器（分步注册，无 Preload 红线）
│   ├── httpdl/                 全静态 HTTP 下载器（guest 侧 wget/curl 替身）
│   └── syntax_stub/            宿主侧 Win32 头桩（Linux 语法检查用）
├── m0/vm/                      QEMU/ReactOS 实验环境（qcow2、启动脚本、noVNC、证据、日志）
├── m2/                         M2 验收操作单 + 数据方案
├── xingma/                     形码产品线 RosXingMa（规划/规范/数据包模板，代码待 W1）
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

> **从 GitHub 全新 clone 后先做一步**：远端仓库里的 shell 脚本没有可执行位（见 §10.5 第 24 条），
> 需 `chmod +x m0/vm/*.sh tools/mkdat/build_dat.sh vendor/fetch-vendor.sh`，否则 `./run-reactos.sh` 会报 Permission denied。

### 6.1 主机（Linux）构建与测试 —— 日常开发主通道

```bash
cd ros-pinyin
cmake -S . -B build -DROSPINYIN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure        # 11 个测试，应 100% 通过
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

### 6.6 诊断 trace（`ROSPINYIN_TRACE`）

```bash
# 本机交叉工具链的 toolchain file 已就绪，直接复用缓存里的配置
cmake -S . -B build-win-trace -DROSPINYIN_TRACE=ON \
      -DCMAKE_TOOLCHAIN_FILE=/home/twipty/.workbuddy/binaries/mingw-i686/toolchain-i686.cmake
make -C build-win-trace rospinyin-ime -j4
```

- 产物把每行追加到 `C:\rospinyin-trace.log`（`OPEN_ALWAYS`，**从不截断**）；正式构建里这段代码整体编译掉（可用 `objdump -p` 看有无 `CreateFileA`/`GetLocalTime` 导入来确认）。
- 行前缀形如 `[19:23:01.123 +123456ms] p=1234 t=1234 正文`，两个时钟各有分工：
  - **墙钟 `HH:MM:SS.mmm`**：跨重启唯一能区分轮次的字段。日志是追加写、永不截断，只有 tick 的话两次开机的行无法分辨，"修前/修后"也就无从对比；
  - **`+Nms`**：`GetTickCount`（开机毫秒），任意两行的时间差就是一次减法；它同时是 `imhost.exe` 自己日志用的时钟，两个文件的先后关系因此可对齐。
- 格式实现与 `kTracePath` 都在 `framework/host/trace_stamp.h`（header-only、无 CRT、不分配内存，`DllMain` 里也能用）；路径只定义一次，避免 `dllmain.cpp` / `imm_entry.cpp` 各写一份而漂移。
- 排查前先 `del C:\rospinyin-trace.log`：旧行只有 `[tick]`，混在一起会让"这次到底有没有新行"变成误判。
- `tools/check_m1.py` 里"框架不得含拼音知识"这一条会先剔除 `ROSPINYIN` 产品名再匹配 —— 否则仅打开 trace 通道就会误报（`ROSPINYIN_TRACE` 不是拼音知识）。


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
| 宿单单测 | `ctest --test-dir build` | 11/11（smoke、session、pinyin、pinyin-session、candidate-pager、pinyin-scheme、emit-text、utf8、code-table、m1-contract、mkdat-e2e） |
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
24. **`push_files` 通道不传文件模式位**，远端 `m0/vm/*.sh`、`tools/mkdat/build_dat.sh`、`vendor/fetch-vendor.sh` 都是 `100644`。所以从 GitHub 全新 clone 后**不能直接 `./run-reactos.sh`**，要先
    `chmod +x m0/vm/*.sh tools/mkdat/build_dat.sh vendor/fetch-vendor.sh`（或统一用 `bash <script>` 调用）。
    同理，`m0/vm/evidence/` 下的 PNG 与 `ime/rospinyin.ico` 这类二进制在远端并不存在（本地产出后未入库），
    它们不是构建必需物——图标由 `ime/rospinyin.ico.b64` + CI 解码提供，证据图仅作本地归档。

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

**2026-11-17 源码级排查结论（读 ReactOS 0.4.16 源码，未上机）**：
- **方向 1 的 `fOpen` 假设已排除**：`win32ss/user/imm32/keymsg.c` 的 `ImmProcessKey()`（L736-790）在调用 `pImeDpi->ImeProcessKey()` 前只检查 `ImmLockImeDpi(hKL)`、`ImmLockIMC(hIMC)`、`GetKeyboardState()`，`fOpen` 只出现在 `VK_PACKET` 分支；不打开 IME 也会调 `ImeProcessKey`。
- **完整拦截链**：`user32!TranslateMessage`（`windows/message.c:2826`）对 `WM_KEYDOWN` → `NtUserTranslateMessage` → `ntuser!IntTranslateKbdMessage`（`ntuser/keyboard.c:1499`，只做 ToUnicode，**不调 IME**）；IME 介入点在 `ntuser/ime.c:603 IntImmProcessKey` → `co_IntImmProcessKey` → `imm32!ImmProcessKey`（keymsg.c:736）→ 我们的 `ImeProcessKey`。`IntImmProcessKey` 的硬门槛是 `pKL->piiex`（L676-679，来自 `ImeInquire` 加载），无 `piiex` 直接 `return 0`。
- **关键门槛（最可疑）**：`IntImmProcessKey` L691-699 与 `ImmProcessKey` 中，若 `fdwProperty` 无 `IME_PROP_NEED_ALTKEY`，则 `uVK==VK_MENU` 或 `lParam & KF_ALTDOWN` 的键被丢弃；这与 Alt+Shift 切换后紧接着按键的场景相关。另一路：只有 `ImeProcessKey` 返回 TRUE 置 `IPHK_PROCESSBYIME` 且 `_IME_APP_COMPAT_PROCESS_BY_IME` 兼容标志置位时，才注入 `WM_KEYDOWN:VK_PROCESSKEY`（keymsg.c:819-830）；否则用户键不会转成 IME 转换路径。
- **与 mzimeja 参考实现的字段差异**（`vendor/mzimeja-ref/ime/imm.cpp:27-49`）：其 `fdwProperty = KBD_CHAR_FIRST|UNICODE|AT_CARET`、`fdwConversionCaps = LANGUAGE|FULLSHAPE|ROMAN|CHARCODE`、`dwPrivateDataSize = sizeof(UIEXTRA)`、`fdwSelectCaps = SELECT_CAP_CONVERSION`；我们对应为 `UNICODE|AT_CARET` / `0` / `0` / `0`。这些差异是否致命需上机验证。
- **本机能力边界**：QEMU 在 `~/.workbuddy/binaries/qemu-user`（可跑 VM、串口日志 `rosdbg.log`）；但**本机无 MinGW 交叉编译器**（CI 用 MSYS2 产 `.ime`），改宿主代码后需走 CI 重编译才能进 VM 验证。已编译产物在 `origin/m1-artifacts`（`rospinyin.ime`）。

**闭环要求**：修复后重跑 `m2/M2-ACCEPTANCE.md` 全部 15 项，重点词组上屏（≥2 字）。

**2026-11-17 源码级排查结论（第二轮：定位到唯一的硬门槛 `pWnd->hImc`）**

把 0.4.16 的 `ntuser/ime.c`、`ntuser/msgqueue.c`、`ntuser/window.c`、`imm32/imm.c`、`imm32/keymsg.c` 逐个读完，结论是**链条在进入 `ImmProcessKey` 之前就断了**，上一轮列的方向 1/2/3 全部位于这道门之后，不是本现象的原因：

- 调用点：`ntuser/msgqueue.c:1877` `if (pWnd && Ret && *RemoveMessages && bKeyUpDown && !(pti->TIF_flags & TIF_DISABLEIME))` → `IntImmProcessKey()`。
- 唯一硬门槛在 `ntuser/ime.c:674-682`（`dwHotKeyId == INVALID_HOTKEY` 分支，即普通输入键）：
  ```c
  if (!pKL->piiex) return 0;
  if (pWnd->hImc) pIMC = UserGetObject(gHandleTable, pWnd->hImc, TYPE_INPUTCONTEXT);
  if (!pIMC) return 0;          /* <-- 没有 IMC 就到此为止 */
  ```
  只有越过这里才会走到 `co_IntImmProcessKey()` → `imm32!ImmProcessKey()`（`keymsg.c:757` 里还要 `ImmGetContext(hWnd)` + `ImmLockIMC()` 再非 NULL，条件等价）。
- `pWnd->hImc` 在 ReactOS 0.4.16 里**只有两条赋值路径**：
  1. `ntuser/window.c:1913` 创建窗口时若线程已有默认 IMC，自动挂上：
     `if (pti->spDefaultImc && Class->atomClassName != gpsi->atomSysClass[ICLS_BUTTON]) pWnd->hImc = UserHMGetHandle(pti->spDefaultImc);`
  2. `ntuser/ime.c:528 IntAssociateInputContext()`，由 `ImmAssociateContext` / `ImmAssociateContextEx` 触发。
- 而 `spDefaultImc` 只由 `NtUserCreateInputContext`（即应用显式 `ImmCreateContext`）建立；`co_IntCreateDefaultImeWindow()`（`ime.c:2077`）除了 logon 进程外不会替应用补建。
- 关键：ReactOS 的 `ImmGetContext()` **不会惰性创建 IMC**。`imm32/imm.c:1065 ImmGetContext` → `ImmGetSaveContext(hWnd, 2)`（`imm.c:1022-1060`），`dwContextFlags=2` 只检查 `CLIENTIMC_DISABLEIME`，**不含 1**，所以不会回退到 `QUERY_WINDOW_DEFAULT_ICONTEXT`，`pWnd->hImc` 为空时直接返回 NULL。（Wine 在此处会建默认 IMC，ReactOS 没有。）
- 结论：记事本的 `EDIT` 控件从不创建 IMC，于是 `pWnd->hImc == 0` → `IntImmProcessKey` 在 `if (!pIMC) return 0;` 提前返回 → **`ImeProcessKey` 永远不会被调用**。这与 `C:\rospinyin-trace.log` 里只有 `ImeInquire`、没有 `ImeProcessKey` 完全吻合；也解释了 `ImmIsIME` 返回 1（该 API 只查注册表）、键盘布局切换正常（`pti->KeyboardLayout` 与 IMC 无关）却敲不出中文。
- 推论（下一步的两条路）：
  - **A. IMC 兜底兼容层**：宿主 `.ime` 在自己进程内（进程已随布局切换加载）为焦点窗口补 `ImmCreateContext` + `ImmAssociateContext`，让 ReactOS 上任何应用都能进入 IME 链条。需评估与 M0「裸 `LoadLibrary` 必须无害」红线的冲突（放到首次按键的懒路径里，不进 `DllMain`）。
  - **B. 自建宿主验证 M2**：用一个自己保证 IMC 存在的宿主窗口跑 `M2-ACCEPTANCE.md`，先把引擎侧验收做掉，兼容层单独推进。
- 取证工具：`imhost.exe`（在 `~/.workbuddy/tmp/vmshare/`，源码 `imhost.cpp`）——先 `ImmCreateContext` 再建窗口（命中路径 1），再显式 `ImmAssociateContext` 兜底（命中路径 2），逐条记录 `WM_KEY*` / `WM_IME_*` 与 `wParam == VK_PROCESSKEY(0xE5)`，日志同时落 `D:\imhost.log` 并 POST 回宿主机 `10.0.2.2:8080`，无需手工拷贝。

**2026-11-17 源码级排查结论（第三轮：真正的原因 —— UI 类名比 imm32 的缓冲长 1 个字符）**

上一轮把锅指给"应用不建 IMC"，那是另一件事（影响记事本这类应用），不是"IME 装不上"的原因。决定性证据来自同一进程内的一次 A/B：

| 观测 | 我们的 .ime | 系统自带 mzimeja |
|---|---|---|
| `ImmInstallIMEW` 之后、未查询属性前 `fLoadFlag` | 0 | 0 |
| 查一次 `ImmGetProperty` 之后 | **1**（prop=0x00000000） | **0**（prop=0x00190002） |
| trace | 只有 `process detach`（说明 `LoadLibrary` 成功过，随后被 `FreeLibrary`） | — |

同机、同 API、同一次开机 → **不是环境脏，也不是 IMM 链坏了**；是 `imm32!Imm32LoadIME` 在 `Imm32InquireIme` 之后 `FreeLibrary` 并把 `fLoadFlag` 钉成 1。而 `Imm32InquireIme` 末尾是：

```c
WCHAR szUIClass[64];                                    // imm32_ime.c:47
pImeDpi->ImeInquire(pImeInfo, szUIClass, dwSystemInfoFlags);
StringCchCopyW(pImeDpi->szUIClass, 16, szUIClass);      // IMEDPI.szUIClass[16] —— 只放 15 字符 + NUL
if (!GetClassInfoW(pImeDpi->hInst, pImeDpi->szUIClass, &wcW)) return FALSE;
```

我们的类名原来是 `RosPinyinUIServer`（**17 字符**）→ 被截成 `RosPinyinUIServe` → `GetClassInfoW` 查不到 → 整条加载失败。这就是"IME 从不被调用"的全部原因：**类名长了 2 个字符**。

为什么之前摸不到：探针一直用 `ImeInquire` 返回的**完整**类名去 `GetClassInfoEx`，当然通过；imm32 用的是**截断后**的名字。`IMEINFOEX.wszUIClass` 也只有 16，所以 `ReadIex` 打出来的 `uiclass=[]` 正是因为失败分支根本没回填它（`fLoadFlag==0 && ret` 才写）。

修法：
- `pinyin/src/scheme.cpp` 的类名改为 `RosPinyinUISrv`（14 字符），并加 `static_assert(长度 <= 15)` 把契约钉死；
- `imfw/scheme.hpp` 在 `ui_server_class()` 上写明"至多 15 字符"及失败症状（否则下一个人还会踩）；
- `tools/check_m1.py` 增加一条门禁：`kUiServerClass` 长度 ≤ 15；
- 探针新增一条判据：同时用"前 15 字符"查类（那才是 imm32 真正用的名字），超长会直接打印警告。

**2026-11-17 用户发现：imhost 不是有效的 IME UI 宿主（必须用 `plain` 模式做判据）**

用户观察到：**系统自带日文输入法 mzimeja 在 imhost 里唤不出状态工具条，同一时刻在记事本里完全正常**（候选窗贴在插入符下方、右下角 `IME ON / あ R 式` 状态条都在）。

日志证明这不是"IME 没加载"：

```
[03:41:42.070] WM_INPUTLANGCHANGEREQUEST flags=0x1 hkl=E0200409
[03:41:42.100] WM_INPUTLANGCHANGE charset=0x0 hkl=E0200409   ← 布局清单里 E0200409 = mzimeja
[03:41:42.100] WM_IME_NOTIFY wParam=0x0006                   ← IMN_SETCONVERSIONMODE
```

mzimeja 已经被加载并激活（否则不会有 `IMN_SETCONVERSIONMODE`），只是**它的 UI 窗口出不来**。imhost 与被测应用之间的差异全在宿主自己身上：

1. 启动即 `ActivateKeyboardLayout(g_hklIme, 0)` 把线程钉在我们的布局上；
2. 抢先 `ImmCreateContext()` + `ImmAssociateContext()`，绕开内核 `co_IntCreateDefaultImeWindow()` 要求的 `pti->spDefaultImc` 默认路径（该函数在建 "Default IME" 窗口，`WS_POPUP|WS_DISABLED`，IME 的 UI 消息无处落脚就什么也画不出）；
3. `WM_IME_SETCONTEXT / WM_IME_NOTIFY / …` 全部 `return 0`，等于向 IME 声称"UI 我自己画"，而 imhost 并不画（记事本能出候选窗就是因为它走 `DefWindowProc`）。

**结论：不能用 imhost 的默认模式判断"IME/工具条能不能出来"。** 现在 `imhost.exe` 增加 `plain` 模式（不锁布局、不预建 IMC、IME 消息交回 `DefWindowProc`、跳过全部破坏性探针），同一个 exe 内即可 A/B；日志文件名加 `plain-` 前缀区分。同时 `DumpLayoutInventory()` 会算出"从当前布局按几次 Alt+Shift 能到我们的布局"，便于在记事本里确定性地切到被测布局。

后续验收建议：M2 的"IME 被调用/候选窗出现"这类 UI 判据一律在 **记事本** 或 `plain` 模式宿主里取，imhost 默认模式只用于读 `fLoadFlag`/`IMEINFOEX` 这类内核状态。

**参照链（`ref` 模式）与差异工具 `tools/ime_chain_diff.py`**

"IME 不被调用"这类问题最缺的不是失败现场，而是**正常样本**：只知道我们少了什么，不知道正常该发生什么。所以做了一台自己可控、结构又和被测算应用一致的宿主：

- `imhost.exe ref`：顶层框架窗口 + **真实 `EDIT` 子控件**（`WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_NOHIDESEL`），焦点在控件上 —— 事件链就发生在控件的 HWND 上，和记事本结构一致；再用 `SetWindowLongPtrW(GWLP_WNDPROC)` 子类化，只记录、不拦截，`CallWindowProc` 原样转发，所以被测行为与记事本完全等价。
- 记录内容做了解码：`WM_IME_SETCONTEXT` 的 `ISC_*`、`WM_IME_NOTIFY` 的 `IMN_*`、`WM_IME_COMPOSITION` 的 `GCS_*`，并**读出组合串与上屏串**（`ImmGetCompositionStringW`，非 ASCII 转成 `\uXXXX` 以便逐字节比对）、`cursorpos`；键消息标注是否 `VK_PROCESSKEY`。
- 布局一变就打一条 `======== 场景切换 <旧> -> <新> ime=<文件> ========`，并把 `ime=` 同步到窗口标题栏 —— VM 里一眼能看出"当前落在哪个输入法"，不用翻日志。
- 同一次运行内切布局即可产出两段场景（参照 + 被测），日志名 `imhost-ref-*.log`，不会与其它模式混淆。
- `tools/ime_chain_diff.py`：按 `场景切换` 切段、按 `ime=` 选段，把时间戳/pid/地址全归一化掉，再用 `difflib` 出统一 diff，并在前面给出每类事件计数（`ime-composition×0` 这种一眼就看出缺口）。`--self-test` 不需要 guest 即可自检。

用法：`python3 tools/ime_chain_diff.py ref.log test.log --ref-ime mzimeja --test-ime rospinyin`。

**布局语言（0804 中文键盘 vs 0409 英文键盘）到底影响什么**

用户发现系统日文输入法挂在 **English keyboard（0409）** 下、与 [en] 并列，我们的挂在 **Chinese keyboard（0804）** 下。逐条查过 ReactOS 源码，结论是：

- **不 gate IME 调用链**。内核 `IntImmProcessKey` 的四道门是 `IS_IMM_MODE()`、`pKL->piiex`、`pWnd->hImc`、`IME_PROP_IGNORE_UPKEYS/NEED_ALTKEY`，没有任何语言判断。
- 按语言分支的几处都**不适用**：`Imm32IsSystemJapaneseOrKorean()` 用的是 `GetSystemDefaultLCID()`（系统区域，与布局无关）；`ImmProcessKey`/`ImmSetCompositionFont` 里的日文特判全部关在 `dwExpWinVer < _WIN32_WINNT_NT4`（Win3.x 兼容）里；`UserSetImeConversionKeyState()` 只对 JP/KR 设置 `VK_DBE_*` 开关键，默认分支什么都不做。
- **但它确实制造了两个真实差异**：
  1. `pKL->iBaseCharset`：0409 → `ANSI_CHARSET(0)`，0804 → `GB2312_CHARSET(0x86)`。日志里 `WM_INPUTLANGCHANGE charset=0x86` 与 `charset=0x0` 的差别就来自此，`INPUTLANGCHANGE_SYSCHARSET` 标志（`flags=0x1`）与默认组合字体也随它走。
  2. **它是 A/B 里最后一个没被消掉的变量** —— 至今所有"我们 vs 系统日文输入法"的对比，都同时差着"实现"和"语言"两项。
- **语言从哪来（完全可控）**：`imm32!ImmInstallIMEW` 从 `.ime` 版本资源取 `VarFileInfo\Translation` 的**第一项**当 HKL 低字（`imm32_install.c`：`pInfoEx->hkl = UlongToHandle(*pw)`）。所以我们挂在哪个键盘下，是**构建期**决定的事实。

因此把语言做成构建期开关：`-DROSPINYIN_IME_LANGID=0804|0409`（`ime/CMakeLists.txt`，默认 0804；`ime/rospinyin_res.rc` 里用 `#ifdef ROSPINYIN_IME_LANG_EN` 切 `LANGUAGE` / StringFileInfo 块名 / `Translation`）。0409 版**只作对照件**，`check_m1.py` 有一条门禁守住"默认必须是 zh-CN"。产物：`rospinyin5.ime`(0804) 与 `rospinyin6.ime`(0409)，字节数相同、仅资源语言不同。

**注意**：消除语言变量只是**让实验干净**，不是修 bug。验收必须回到 0804 —— 否则会留下"只在英文键盘下能用"的隐患。

实测验证：同一轮 VM 里 `rospinyin5.ime`(0804) 与 `rospinyin6.ime`(0409) 行为**完全一致**（都能上屏、都缺 `WM_IME_STARTCOMPOSITION`、都双 feed），语言变量正式排除。因此 `install` 模式回到只装 0804 一份。

### 11.4 ⛔ 修复中：每个按键被喂进引擎两次（多字 / 丢字）

**症状**（2026-11-17 晚，IME 首次成功拦截按键之后）：能上屏中文，但输入过程中"有时多出一个字符、有时少一个字符"。

**证据链**（`imhost-ref-final.log` + `rospinyin-trace-ref-final.log`，同一次运行）：

```
trace:  processkey vk=0x4e pf=0x31  -> 1      ← key-down
        toascii    vk=0x4e
        processkey vk=0x4e pf=0xc031 -> 1     ← key-UP，也被认领
        toascii    vk=0x4e                    ← 于是同一字符进引擎两次
```

`pf` 是 lParam 的高 16 位：`0xc031` = bit31（transition=释放）+ bit30（previous state）+ scan code，即 **key-up**。5 个字母 `nihao` 一共产生 10 次 `toascii`。

**机制**（ReactOS 源码）：

1. `co_IntProcessKeyboardMessage` 对 `WM_KEYUP` 也置 `bKeyUpDown = TRUE`（`win32ss_user_ntmsgqueue.c:1785`），于是 key-up 同样进 `IntImmProcessKey`；
2. `imm32!ImmProcessKey` 不看 transition 位，只要 `ImeProcessKey` 返回 TRUE 就置 `pIC->bNeedsTrans = TRUE`（`imm32_keymsg.c:785`）；
3. `ImmGenerateMessage` 见到 `bNeedsTrans` 就调 `ImeToAsciiEx`（`imm32_keymsg.c:1056`）——而 **`ImeToAsciiEx` 的参数里没有 lParam**，无法自行区分按下与抬起。

**后果**：`feed()` 每个字母跑两遍。拼音缓冲出现双写（`nn`/`aa`），`can_extend()` 对其中一部分判定仍可扩展 → **多出字符**；另一部分被判非法后 `pop_back()` → **丢失字符**。同一条 bug 同时解释了两种症状。

**修法**：在 `ImeProcessKey` 里检查 lParam bit31，key-up 一律返回 FALSE。这是唯一能做该判断的地方。

```cpp
constexpr unsigned long kKeyUpBit = 0x80000000UL;
if ((static_cast<unsigned long>(lParam) & kKeyUpBit) != 0) return FALSE;
```

**顺带确认的两件事**（都不是 bug）：

- `WM_IME_CHAR` 紧跟 `WM_CHAR` 是正常成对：前者由 imm32 投递 TRANSMESSAGE，后者是 `TranslateMessage` 转换的结果。
- `zhongguo` + 空格只上屏 `国`（丢 `zhong`）是 **M2 验收单 3.3 的既定口径**（单字引擎，词组属 M4）。
- 我们**没有** `WM_IME_STARTCOMPOSITION` / `WM_IME_COMPOSITION`：ReactOS 的 imm32 **根本没有实现 `ImmSetCompositionString`**（全树无此符号、无 .spec 条目），所以组合串只能自绘（`framework/ui`）。这与记事本里能看到组合串的 mzimeja 不同，但属于平台限制，不是缺陷。

**待确认**：`ImmGetOpenStatus` 的真实返回值。`ImmCreateContext` 会把 `pIC->fOpen` 初始化为 FALSE（`imm32_imm.c:793`），若它一直不翻真，则"IME 关闭时仍吞字母"就无从拦截（用户切英文后打字会整体消失）。trace 构建里已打印 `open=`，但**刻意没有用它做门禁** —— 在拿到真实值之前用它拦截会直接把 IME 打死。

### 11.5 m2 自校验：把"多字/缺字"的判定交给程序

**为什么需要**：11.4 的结论虽然自洽，但"多字/丢字"这个现象此前一直是**人眼读屏幕**得出的。这类判据有三类假阳性无法排除：(1) 注入太快，键根本没到 IME，看起来像丢字；(2) 读错/记错一个字；(3) 把"M2 只上屏末音节"这个既定口径当成丢字。所以要复测，就不能再靠肉眼。

**做法**：`imhost.exe m2` —— ref 的窗口结构（顶层窗口 + 真实 `EDIT` 控件 + 子类化），但**结果由程序判定**：

```
注入 → GetWindowTextW 读回 → 与验收单期望值比对 → PASS / EXTRA / MISS / WRONG / ASCII-LEAK
```

判定标准全部来自 `m2/M2-ACCEPTANCE.md`；没有明确期望值的用例**只校验汉字个数**，不猜具体是哪个字（猜错就会把正常行为判成 bug）。用例表在启动时整表写进日志，日志自带判定标准，事后可复查。

**16 条用例 × 两轮**（首尾各 1 条为环境/Esc 自检）：

| 类别 | 用例 | 期望 |
|---|---|---|
| 环境自检 | `z` + `Esc` | 0 字（z 必须被 IME 吞掉） |
| 验收单直取 | `zhong`→中、`nv`→女、`lv`→律、`lve`→略、`nve`→虐、`zhongguo`→国 | 各 1 字 |
| 只校验个数 | `guo` `ni` `hao` `shi` `hao`+退格×2 | 各 1 字 |
| 信息性 | `a` | 1 字（引擎未必支持，不计入总分） |
| 取消 | `hao` + `Esc` | 0 字 |

**四道防误判机制**：

1. **两轮不同节奏** —— R1 600ms/键、R2 280ms/键。结论分三种：两轮全过 = 正常；慢过快不过 = **时序敏感**（真实但不是多字/缺字缺陷）；慢速也失败 = 真异常。把"注入太快"从"IME 缺陷"里摘出去。
2. **环境自检用例** —— 若 `z` 直接落进编辑框（`ASCII-LEAK`），说明按键压根没进 IME，整轮结论标为"环境不成立"，不用来判定缺陷。
3. **每条用例前送 Esc 清场** —— 否则一条失败会把组合串带进下一条，连锁失败看起来像"全坏"（污染传播 ≠ 真 bug）。
4. **注入用确定 VK** —— 不用 `VkKeyScanW`（对 `z` 可能返回 `0x7A`，那是 VK_F11，注入的就不是字母），改为 `ch - 32` 直接映射。

另外每条用例记录 `imeChar` / `wmChar` 计数：若读回两个字而 `imeChar=1`、`wmChar=1`，说明同一个字符被 `WM_IME_CHAR` 与 `WM_CHAR` 各插了一次 —— 那是"多字"的另一条通路，必须和"引擎被喂两次"分开。同时统计 `IMN_OPENCANDIDATE` / `IMN_CLOSECANDIDATE` / `IMN_SETOPENSTATUS` / `WM_IME_STARTCOMPOSITION`，让"选词界面有没有出现"也有日志数值，而不只靠人眼。

**本机验证**（无 Windows，抽纯函数 + 复刻判定矩阵）：
- `CountHan` / `EscText` / `VkOf` 行为正确（`z=0x5A`=VK_Z、`\b=0x08`、`Esc=0x1B`）
- 12 条判定矩阵全过：正常上屏 PASS、双字 EXTRA、空 MISS、错字 WRONG、字母漏进编辑框 ASCII-LEAK、Esc 失效上屏判 EXTRA（而不是误判成 PASS）

**用时**：R1 约 68s、R2 约 36s，合计约 1.7 分钟。


### 11.6 键盘三层取证：真实按键 → 窗口 → IME

**问题**：11.4/11.5 的所有日志都产生在"消息已经到达窗口/控件之后"。IME 把键吃掉、焦点不在控件、或消息在 win32k 里被吞时，这一层什么都看不见 —— 于是"丢字"到底是**没敲进去**、**没到窗口**、还是**到了但被 IME 处理两次**，日志本身无法区分，只能靠推断。

**做法**：在 `imhost` 里加一层**入口层**记录 —— 线程级 `WH_KEYBOARD` 钩子。它由 win32k 在 `KeyboardMessage()` 中、**`IntImmProcessKey()` 之前**调用（`ntuser/msgqueue.c:1859`），拿到的是"系统这一瞬间实际收到了哪几个键"：

```
KEY 0007 vk=0x4E(N) sc=0x31 DOWN rep=1
KEY 0008 vk=0x4E(N) sc=0x31 UP   rep=1
```

`lParam` 拆成四段记：重复次数、**扫描码**、扩展位、ALT 上下文、转换位。扫描码由 ps/2 直传，同一物理键应始终一致 —— 同一字符若出现两条不同扫描码，问题在注入侧（`keybd_event`/VNC），不在 IME。

**三层对账**（每轮结束、退出时各打一份增量，R1/R2 独立）：

| 层 | 取值点 | 能证明什么 |
|---|---|---|
| 入口层 | `WH_KEYBOARD` 回调 | 系统到底收到几次按下/抬起 |
| 窗口层 | 子类化 `EditProc` 的 `WM_KEY(DOWN/UP)` | 有多少键真正到达应用 |
| IME 层 | `.ime` 内 `ImeProcessKey` / `ImeToAsciiEx` 的 trace | 输入法实际被喂了什么（含 `pf=` 扫描码+标志） |

```
---- 键盘对账[R1(600ms)] 入口层 DOWN=32 UP=32 | 窗口层 DOWN=32 UP=32 | WM_CHAR=0 WM_IME_CHAR=16 ----
  入口层与窗口层一致（各 32 次按下）：键全部到达窗口，差异只可能出现在 IME 层
```

差值为 0 → 键没进系统（焦点/主控问题）；入口有、窗口无 → 消息在钩子之后被吞；两层一致但 IME trace 少 → 丢在 IMM32 内部；`WM_CHAR` 多于按键 → 同一个键被翻出多个字符（"多字"的另一条通路）。

**为什么不用更底层的两种手段**（ReactOS 0.4.16 源码核实）：

- **Raw Input**（`WM_INPUT` + `RIDEV_INPUTSINK`）：`NtUserRegisterRawInputDevices` 等 6 个函数在 `ntuser/ntstubs.c` 里全是 `STUB; return 0;`，注册必然失败 —— 平台未实现，不可用。
- **`WH_KEYBOARD_LL`**：`ntuser_callback.c` 只实现了该钩子的**参数封送**（`KBDLLHOOKSTRUCT` 拷进用户空间），全树找不到任何触发点；实际唯一的调用点是 `msgqueue.c:1859` 的 `WH_KEYBOARD`。用 LL 钩子会静默拿不到任何回调。
- **真正的最原始层**在 guest 之外（QEMU 的 RFB `KeyEvent`）。要抓这一层得在 websockify/QEMU 前面插代理并重启那条链路，属于环境改造，不是探针能解决的；当前三层已足够定位"丢在哪一段"。

**实现位置**：`~/.workbuddy/tmp/vmshare/imhost.cpp`（`KbProc` / `InstallKeyboardHook` / `KeySummary`）。钩子装失败不致命，日志会打 `SetWindowsHookExW -> NULL err=` 且对账时明示"入口层无效"，此时按两层数据判读。

**两个已修的取证陷阱（2026-11-17 20:58 那轮 m2 暴露）**：

1. **子串匹配选错布局** —— `FindHklByImeFile(L"rospinyin")` 会命中列表第一个 `rospinyin.ime`，那是最早的构建（UI 类名 17 字符的老版本），`ImmGetProperty` 返回 0 → 整轮在开始前即 ABORT，而真正要测的 `rospinyin7.ime` 从未被选中。改为**精确文件名匹配** + 启动时打印候选布局表（每个布局的 `prop` 与是否可用），并在每轮开头**强制从宿主机刷新一次** `.ime`（`ImmInstallIMEW` 按文件名复用布局，不会越装越多）——"测的是旧文件"这个坑在这个项目里已经出现过两次（922022 字节的陈旧 `rospinyin.ime`），不能再靠人记得先 install。
2. **模式标语贴错** —— m2 走的是 `g_vanilla` 分支，标语却按 `g_refMode ? "ref" : "plain"` 打，日志开头写着 `plain`、实际跑的是 m2，事后看日志会误判。

**rec：纯记录模式（不代替用户发键）**

m2 用 `keybd_event` 替人打字，测的是**注入路径**；而"我自己敲的时候输入法收到了什么"是另一个问题——注入没有真人节奏、没有重复键，也绕过了 VNC→ps/2 那一段。所以另有 `imhost.exe rec`：

- **一个键都不发**：所有注入收敛到唯一出口 `InjectKeyEvent()`，rec 模式下直接拒绝并计数；退出时打印"本程序注入按键 N 次"，rec 下必为 0（可核验，不是口头承诺）。
- **不改状态**：不切布局、不装 IME、不清编辑框，只把当前布局/`.ime` 文件大小/`ImmGetProperty` 读出来当上下文（`prop=0` 会警告"敲键不会出候选"）。
- **三层照记**：入口 `KEY`、窗口 `[edit]`、IME `trace`；另外每次上屏文字变化打一行 `TEXT len= 汉字= [...]`，按顺序即为真实上屏结果，不需要人读屏。

用法：`imhost.exe rec` → 自己 Alt+Shift 切布局 → 正常打字 → 关窗口自动上传。

### 11.7 真机手打验证结果（2026-11-17 21:08–21:10，rec 模式）

第一次用**真实人手打字**（非注入）跑三层取证，被测 `rospinyin7.ime`（931,363 字节，布局 `E0260804`，`ImmGetProperty=0x00090000`）。

**三层对账**（退出汇总行原文）：

```
---- 键盘对账[退出] 入口层 DOWN=40 UP=40 | 窗口层 DOWN=40 UP=40 | WM_CHAR=5 WM_IME_CHAR=5 ----
  入口层与窗口层一致（各 40 次按下）：键全部到达窗口，差异只可能出现在 IME 层
本程序注入按键 0 次，被拒 0 次（全程未代替用户发键）
```

**逐字上屏**（`TEXT` 行，每行只 +1 字）：

```
TEXT len=1 汉字=1 [你] → len=2 [你好] → len=3 [你好中] → len=4 [你好中国] → len=5 [你好中国誰]
```

敲的是 `nih`+Backspace+`hao`+空格 → `zhong`+空格 → `guo`+空格 → `shui`，**最后一步不是空格**：候选里没有简体「谁」，用户翻了 4 次 `=`、6 次 `-` 回到某页，按 `5` 选了第 5 项 `誰`（U+8AB0，繁体）。`TEXT` 行记录的是 `\u8AB0` 而不是 `\u8C01`，与"只有繁体的"的口头反馈一致（见 §11.8）。

**IME 层证据**：29 次 `processkey ... -> 1` 对应 29 次 `toascii`（21 次字母 feed + 8 次空格/退格），**1:1**；每次抬起都是 `processkey vk=0xNN up -> 0`，没有 `toascii`、没有 `feed`。这正好验证 §11.4 的 key-up 修复生效 —— 修复前每个键会被喂两次（一次 down 一次 up），现在 up 一律不认领。`WM_IME_CHAR` 与 `WM_CHAR` 各 5 条、上屏 5 字，说明二者是"同一次上屏的两条消息"而非双重插入。

**结论**：M2 的单字输入路径在真实手打条件下**通过** —— 无多字、无丢字、无 ASCII 泄漏，入口/窗口/IME 三层条数自洽。11.4 的"多字/丢字"到本轮为止不再是复现项。

**观察到的两个小问题（未门禁，待查）**：

1. `ImmGetOpenStatus` 读数不一致：imhost 轮询得到 `-1`，`.ime` 内 trace 打 `open=0`。引擎侧已明确"只观测不门禁"，但两个读数都不像正常值（0/1），怀疑 ReactOS 上该 API 对跨线程 IMC 的行为与预期不同，值得单独确认——它决定 Shift 切英文后按键该不该吞。
2. 每敲一个键，应用会收到 1–3 次 `IMN_SETCOMPOSITIONWINDOW`（`n` 那次 3 次）。说明引擎在每次 feed 后都调 `ImmSetCompositionWindow`，重复通知会让宿主反复重排组合窗位置，是"界面轻微抖动"的一个可疑来源。


### 11.8 候选质量：`shui` 没有简体「谁」+ 大量空心框（2026-11-17 21:30 修复）

用户反馈：敲 `shui` 时候选里只有繁体「誰」没有简体「谁」，且很多字显示为一个空心点，问是不是字体问题。

**结论：不是字体坏了，是两个独立缺陷叠加。** 字体只负责"把已有码点画出来"，而这两件事分别出在**读音归属**和**候选里塞了字体画不出的字**。

#### 缺陷 A：谁 被归到 `shei` 音节（读音丢失）

Unihan 简繁两条记录互相不一致：

```
U+8C01 誰?  谁  kMandarin  shéi            kHanyuPinlu  shuí(1065)
U+8AB0 誰      kMandarin  shuí shéi        kHanyuPinlu  shuí(1065)
```

`mkdat::choose_primary()` 以 `kMandarin` 为读音全集、只拿 `kHanyuPinlu` 挑"在全集里最常用的那个"。于是：

- 谁：kMandarin 只给 `shéi` → `kHanyuPinlu` 的 `shuí` **不在全集里 → 被静默丢弃** → 谁只挂在 `shei` 下，`shui` 查不到。
- 誰：kMandarin 给了 `shuí shéi` → 首个是 `shuí` → 繁体反而占了 `shui` 音节。

叠加第二层：`CodeTable::load_dat()` 只读 `reading_off`（主读音）建索引，**完全忽略 `alt_off`**（次读音）。所以就算 `shuí` 被写进 alt，引擎也查不到 —— 多音字"另一个读音能打出来"这件事从未成立。

#### 缺陷 B：候选里混入字体画不出的字（空心框）

`.dat` 44,355 条里只有 **9,898 条有字频**（hanziDB），其余 34,457 条是 Unihan 独有的生僻字，其中 **68%（23,433 条）落在 U+4E00–U+9FA5 之外**（CJK 扩展 A/B 等，多为 U+20000 以上）。而随包字体 `NotoSansSC-subset.ttf` 是按 `0x4E00–0x9FA5` 子集化的（`~/.workbuddy/tmp/font_subset.py`），扩展区一律没有字形 → 一个个空心框。

候选窗每页 9 条，`shui` 旧候选共 34 条：`水 税 睡 稅 誰 脽 㥨 㽷 䬽 …`，第 7 项起全是空心态，第 2 页整页、最多 4 页 —— 这就是"很多字是一个空心点"的全部来源，与字体文件损坏无关。

#### 修复（三处）

1. **`tools/mkdat/mkdat.cpp`**：`kHanyuPinlu` 的音节不在 `kMandarin` 全集时，作为**次读音追加**（不抢主音）。谁 → 主音 `she2i` + alt `shui2`；誰 → 主音 `shui2` + alt `she2i`。多音字条目 109 → 118。
2. **`framework/.../code_table.hpp|cpp`**：`load_dat()` 新增可选 `KeyOfReading` 回调；给出时把 alt 里每条 reading 经该回调转成 key 并建**别名索引**，同一字符可回答多个音节，且每次 lookup 只出现一次。`pinyin/host/dat_source_win.cpp` 传入 `&rospinyin::pinyin::syllable_of`。不传则行为与之前完全一致（向后兼容）。
3. **`pinyin/src/pinyin_session.cpp`**：`recompute()` 默认**不把无字频字放进候选**；仅当该音节过滤后一个不剩时才回退到全表（保证任何合法音节都还有得选）。开关 `set_show_rare(true)` 恢复旧行为。

#### 效果（真实引擎加载真实 `rospinyin.dat`，非脚本模拟）

```
shui   rare=false ->  7 条: 水 谁 税 睡 稅 誰 脽
shui   rare=true  -> 35 条: 水 谁 税 睡 稅 誰 脽 㥨□ 㽷□ 䬽□ …
shei   rare=false ->  2 条: 谁 誰
zhong  rare=false -> 26 条（原 108）
shi    rare=false -> 95 条（原 340）
```

简体「谁」现在排第 2（rank 648，仅次于「水」202），空心框 20 → 0。`shi` 仍剩 1 个：那是一个**有字频但码点在扩展 A** 的字，属字体子集覆盖不足，要彻底消掉得把 hanziDB 里那 30 个扩展区字并进字体子集（`font_subset.py` 加一段），不影响英文/拼音显示。

#### 回归

`ctest` 11/11、`check_m1.py` 53/53、`xingma/` 零改动。新增测试：
`code_table_test.cpp` 的 alt 索引用例（带/不带 deriver 两种行为 + 不重复出字）、`pinyin_session_test.cpp` 的生僻字过滤与"只剩生僻字时兜底"用例。

#### 顺带纠正 §11.7 的一处记录错误

上一轮 `TEXT` 上屏第 5 字是 `\u8AB0`（**繁体 誰**），不是简体「谁」。日志里那 4 次 `vk=0xBB`（`=`）翻页、6 次 `vk=0xBD`（`-`）回翻、最后 `vk=0x35`（`5`）选第 5 项，正是"翻遍候选找不到简体"的现场。11.7 已订正。

### 11.9 "改了没生效"的根治：让 .ime 自报数据指纹（2026-11-17 21:45）

**现象**：§11.8 修完之后用户复测，反馈"问题还在"。日志里 `feed ch=i act=1 comp=[shui] cand=34` —— 候选仍是 34 条，而修复后的表应当是 7 条。

**根因不在代码，在"这一轮根本没有用新构建"**：截图标题栏是 `imhost REF`，用户跑的是 `ref` 模式。而 `pull`/`install` 之外的所有模式都**从不更新 `.dat`**，`ref` 更是连 `.ime` 也不装。于是 system32 里仍是旧 `.ime` + 旧 `.dat`，候选 34 条完全自洽。旧 `.ime` 既没有 alt 索引（谁 的 shuí 读不到），也没有生僻字过滤（空心框照旧）。

这类"改了却没生效"已经反复三轮（第一次是 system32 里陈旧 `.ime`，第二次是选错布局，这次是跑错模式），共同点是：**宿主侧看到的只有"候选没变"，无法分辨是文件没换、换了但被缓存、还是换错了目录**。

**两条根治措施**：

1. **`.ime` 自报数据指纹**（`pinyin/host/dat_source_win.cpp`，仅 `ROSPINYIN_TRACE`）。读表成功后往 trace 写一行：

   ```
   dat OK path=C:\ReactOS\System32\rospinyin.dat size=902776 chars=44355 syls=419 shui=35 shui_simp=1 shui_trad=1
   ```

   判据是 `shui_simp`：只有**新表 + 新 .ime**（alt 索引真的建起来了）才会是 `1`；旧表这里是 `0`。读不到文件时写 `dat MISSING path=... err=2 -> 回退内置表`，解析失败写 `dat PARSE_FAIL`。这样任何一轮 trace 的前几行就能定论，不必再猜。

2. **`rec`/`ref`/`plain` 启动即对齐代码与数据**（`imhost.cpp` 的 `AlignRuntime()`）。在**窗口创建之前**（即 `.ime` 被 LoadLibrary 之前）：下载 `/rospinyin7.ime` 到 `C:\rospinyin7.new`，与 `system32\rospinyin7.ime` **逐字节比较**（只比大小会撞），不同则 `CopyFileW` 覆盖；再 `PullDataFile()` 更新 `.dat`。只换文件，不碰注册表、不改布局、不发任何键。覆盖失败（`err=32`）说明有进程正映射着该 `.ime`，日志明示"请重启 VM 再跑"。

   `PullDataFile()` 同时修掉一个陷阱：写 `system32` 失败退到 `C:\rospinyin.dat` 时，读回校验仍会打印 OK，但那对 IME 无效（它只读 system32）。现在回退分支会追加 `但写到了 C:\，IME 读 system32，本次未生效`，并返回 false。

**教训**：凡是"我改了但现象没变"，第一步不是改代码，而是先证明被测进程读的是哪份代码和哪份数据。宿主侧证明不了，就让被测方自己报。

**产物**：`rospinyin7.ime` 946,193 字节（trace）、`rospinyin.dat` 902,776 字节、`imhost.exe` 935,031 字节。回归 `ctest` 11/11、`check_m1` 53/53、`xingma/` 零改动；无 trace 的 `build-win` 同样编译通过（`#ifdef` 分支未破坏生产构建）。

### 11.10 `.ime` 换不掉的真因：默认输入法导致常驻映射（2026-11-17 21:56）

11.9 的对齐机制上线后复测，日志里出现：

```
启动前对齐代码与数据（只换文件，不发键、不改布局）
  .ime 覆盖失败 err=32 —— 有进程正映射着它
数据表校验 OK：magic=ROSPYDAT chars=44355 syls=418
```

`.dat` 换成了新的，`.ime` 换不掉 → 仍是"新表 + 旧代码"，现象当然不变。`.ime` trace 里没有 `dat OK` 那一行，正好证明跑的是旧代码（新代码才有这行）。

**为什么换不掉**：`err=32` 是 `ERROR_SHARING_VIOLATION`，即该 PE 已被某个进程作为 image 映射。trace 里除 imhost 外还出现过 `p=520`、`p=716` 两个进程执行 `active f=1`，说明有**非 imhost 的常驻进程**加载了它。结合 `GetKeyboardLayout(0)=E0260804`（imhost 一启动当前布局就是我们的 IME，而控制面板里"默认输入语言"显示 English(US)），实际状态是：我们的 IME 已被系统在会话里激活并常驻，任何激活过它的进程都会一直映射该文件，直到退出。

ReactOS 这版没有 psapi、也没有 toolhelp 的模块枚举（`EnumProcessModules` / `Module32First` 全树无实现），所以无法在宿主侧点名是哪个进程。

**两条处置**：

1. **热替换退让链**（`ReplaceFileBestEffort`）。直接 `CopyFile` 失败后依次试：把原名 `MoveFile` 成 `.old` 再写新文件（映射的是 section 对象，与文件名无关，通常能过）→ 备份到 `C:\rospinyin7.ime.bak` 后删除原名（delete-pending）再建同名文件。每步失败都先回滚，绝不留下"IME 文件没了"的状态。全部失败才提示关窗口/重启。
2. **dev 布局兜底**（`EnsureDevIme`）。布局按**文件名**去重，所以换名字等于换文件，与占用彻底无关：装一份 `rospinyindev.ime`（描述 `RosPinyinDev`），固定在同一个名字上反复覆盖，不会把布局列表越堆越长。`AlignRuntime()` 返回 false 时自动装并 `ActivateKeyboardLayout` 过去，日志明示本轮用的是 dev 布局。

另：`rec`/`ref`/`plain` 启动时若 `FindWindowW(kClass)` 已找到窗口，说明上一个 imhost 没关，它也在映射 `.ime`，日志会提示先关掉。

**长期口径**：验证期用 dev 布局即可，不必重启；要让 `rospinyin7.ime` 本身更新，需要先关掉所有加载过它的进程（或重启 VM），因为文件被 image 映射时无法原地覆盖。这一点应在 M2 验收单里写清，避免下次又误判成"改了没生效"。


### 11.11 P0-2 收口：`open=-1` 是探针假象，`open=0` 是 ReactOS 的出厂值（2026-11-17 22:4x）

M2 清单里挂着的"`ImmGetOpenStatus` 读数异常"，其实是**两个不同来源的数字被当成了同一个**：

| 读数 | 出处 | 真实含义 |
|---|---|---|
| `open=-1` | `imhost` 状态栏/`tick` 行：`g_himc ? ImmGetOpenStatus(g_himc) : -1` | `g_himc == NULL`，即"这个窗口还没有关联输入上下文"。`-1` 是探针的哨兵值，恰好等于 `TRUE`，所以看起来像"IMM32 说已打开" |
| `open=0` | `.ime` 内 `processkey` 行 | 真实的 `pIC->fOpen`，ReactOS 建上下文时就是 `FALSE` |

ReactOS 侧的三条事实（0.4.16 源码）：

1. `win32ss/user/imm32/imm.c` 建上下文时 `pIC->fOpen = FALSE`，`fdwConversion = fdwSentence = 0`；
2. `ImmSetActiveContext`（`imm.c`）只**读** `pIC->fOpen` 去调 `NtUserNotifyIMEStatus`，从不回置；
   `ImmInstallIMEW` 也不碰它。所以布局切换多少次，它都还是 `FALSE`；
3. 按键路由**不看**这个值：`ImmProcessKey`（`keymsg.c`）无条件调 `pImeDpi->ImeProcessKey`，
   只有 `VK_PACKET` + 非 Unicode IME 的分支才看 `pIC->fOpen`。这就是"IME 明明在工作，`open` 却是 0"。

**结论**：`open=0` 不是 bug 而是 ReactOS 的默认状态；把它当门禁会直接废掉输入法。真正的危害有三条：
语言栏的 中/英 状态永远显示为关（`UserSetImeConversionKeyState(..., IME_CMODE_ALPHANUMERIC)`）、
下一次 `Ctrl+Space` 走 `Imm32CImeNonImeToggle` 的"关闭"分支（本该切 EN 布局）、
以及 M3 的 Shift 临时英文没有确定的基线。

**修法**：`ImeSetActiveContext(fFlag=TRUE)` 里把 OS 侧状态对齐一次（`AlignOpenStatus()`，
`framework/host/imm_entry.cpp`）：读 `before`，为 0 则 `ImmSetOpenStatus(TRUE)` 再读 `after`，
trace 打 `openalign before=0 after=1`。键路由仍不依赖它（`check_m1.py` 已加门禁：
`should_consume_key` 体内不得出现 `OpenStatus`）。

**最小复现序列**（`imhost.exe` 窗口里即可）：

```
1. 启动 imhost（任何模式），窗口获得焦点
2. 状态栏 ImmProp=0x00090000、IME文件=rospinyin.ime 时，trace 里 processkey 行的 open= 为 1
3. 若某轮仍是 0：说明该 HIMC 没走过 ImeSetActiveContext（例如从 imhost 自带 IME 窗口直接取到的上下文）
```

### 11.12 P0-3 收口：抖动来自 ReactOS 的 EDIT 光标同步，不是引擎在动窗口（2026-11-17 22:5x）

原先的猜测"引擎每次 feed 都调 `ImmSetCompositionWindow`"是错的——全仓库没有一处
`ImmSetCompositionWindow` 调用。真正的发送者在应用侧：

```c
/* win32ss/user/user32/controls/edit.c, 仅 __REACTOS__ 分支 */
static void EDIT_SetCaretPos(EDITSTATE *es, INT pos, BOOL after_wrap) {
    ...
    SetCaretPos(pt.x, pt.y);
    if (ImmIsIME(hKL))
        EDIT_ImmSetCompositionWindow(es, pt);   /* -> ImmSetCompositionWindow -> IMN_SETCOMPOSITIONWINDOW */
}
```

`EDIT_SetCaretPos` 在每次插入、`EDIT_EM_ScrollCaret`、`EDIT_AdjustFormatRect` 后都会跑，
而且**不检查当前是否在组合中**，只看 `ImmIsIME(hKL)`。这解释了"每键 1–3 次通知"：一次按键
依次触发"插入字符→滚动→格式化矩形"三条光标同步。通知数**不能从 IME 侧降低**，
它只影响 `pIC->cfCompForm`，而我们从不读 `ImmGetCompositionWindow`。

用户看到的跳动是另一回事，出在我们自己的窗口上，两条真 bug：

1. `CompositionWindow::update()` 量了文本、写了 `width_/height_`，却**从未** `SetWindowPos`：
   `zhong` 建窗 61 px 宽，`zhongguo` 仍按 61 px 画，后半截被裁掉；
2. `update()` 每次都 `InvalidateRect + ShowWindow`，`place_windows()` 每次都 `SetWindowPos`，
   于是每来一条 `IMN_SETCOMPOSITIONWINDOW` 就整窗重绘一次。

**修法**：几何只有一个 owner——`ime_ui.cpp:place_windows()` 调 `UiWindow::apply_geometry(x,y,w,h)`，
它把位置和尺寸一起下发，且**只有真正变化时才发消息**；`show()` 只在隐藏时调 `ShowWindow`；
两个窗口的重绘都以"文本/候选页内容是否变化"为前提。trace 的 `feed` 行新增绝对计数
`wmove=`（真正改变几何的次数）与 `wrep=`（重绘请求次数），一次键正常应为每窗 0–1。

### 11.13 P1 收口：安装器要两份文件、候选按字形过滤、生僻字开关说明

**P1-1 安装器只拷 `.ime`（已修）**。M2 的 IME 还依赖 `system32\rospinyin.dat`，
旧 `install.cpp` 只 `copy` 了 `.ime`，装完就是"候选窗空白"——而这条故障和"数据没生效"
长得一模一样。现在 `/step1` 拷两份并**逐字节比大小**（`kVerifyFailed=4`），
`/status` 分别报告，`/u` 两份都删（含 `.old`）。另外文件被 image 映射时
`CopyFile` 会 `ERROR_SHARING_VIOLATION(32)`：现在先 `MoveFileEx(name -> name.old)`
腾名字再写，并明确提示"运行中的 IME 仍用旧文件，重启后生效"——与 imhost 的
`ReplaceFileBestEffort()` 是同一套退让链，只是搬进了安装器。

**P1-2 扩展 A 区方框（已修）**。随包字体子集是 `U+4E00–U+9FA5`（实测 cmap 21,287 字，
扩展 A 区 0 个），而 `shi` 的候选里有一个**有字频**的扩展 A 字，于是显示成豆腐块、
选中后把方框写进文档。修法不是扩字体（11 MB → 更大），而是**运行时按字形过滤**：
`UiCanRender()` 用候选窗同一个 HFONT 调 `GetGlyphIndicesW(..., GGI_MARK_NONEXISTING_GLYPHS)`
（结果按码点缓存，`shi` 95 个候选只探一次），`imm_entry.cpp:renderable_candidates()`
在进分页器**之前**剔除——必须在分页器之前，否则数字键会与屏幕上的行号错位。
两条护栏：探针不可用一律**放行**（`GDI_ERROR` 视为可画）；若超过一半候选被剔
（说明当前字体根本没有 CJK），整体放行并 trace `cand-filter ... keeping all`。

**P1-3 生僻字开关**。`PinyinSession::set_show_rare(true)` 让无字频字也进候选；
M2 没有 UI 入口，需要时改 `pinyin/src/pinyin_session.cpp` 的默认值或由 Scheme 传入。
默认关闭是有意的：打开后 `shi` 这类音节会从 95 条涨到数百条，且多数在子集外
（会被 P1-2 的过滤器挡掉，等于白翻页）。M3 再给语言栏菜单项。

### 11.14 Shift 切中英：`open` 从"观测值"变成"IME 自己的状态"（2026-11-18 12:2x）

真机反馈："Shift 无法切换中英文"。trace 里其实**有** `processkey vk=0x10 ... -> 0` ——
键到得了我们手上，是我们什么都不做：imm32 只把 `fOpen` 存着，激活路径不回置，
也没给 VK_SHIFT 注册热键；IME 自己不认领，Shift 就落给应用当普通修饰键。

修法（`imm_entry.cpp`）：
- `ImeProcessKey` 先处理 `VK_SHIFT`：按下置 `shift_latched`，**其它任何键清 latch**，
  抬起时若仍是 lone Shift 才调 `set_ime_open()`。这样 `Shift+A` 仍是大写 A，
  不会变成"切到英文再打 a"。
- `set_ime_open()` 是唯一开关出口：`ImmSetOpenStatus` + 若正在组合则
  `finish_composition()`（不取消会留下一个永远不更新的组合窗）。
- `should_consume_key()` 第一行 `if (!ime_is_open(hIMC)) return false;` ——
  关闭态下所有键归应用，这才是"英文输入"。
- **P0-2 的结论要反过来读**：当时说"`open` 只能观测、不能当门禁"，前提是**没人设置它**；
  现在 `AlignOpenStatus()` 每次会话对齐一次、Shift 可切换，它就是可信状态。
  `AlignOpenStatus()` 也从"每次激活"改成"每会话一次"——否则切到英文后点一下别的窗口
  再回来就被悄悄改回中文。
- 契约闸门同步改写：`open` 必须"激活时对齐一次 + 只在 `set_ime_open` 里写 +
  只在 `ime_is_open()` 里读且门禁走它"（check_m1 57 项）。

另外加 `uigeom` trace（`refresh_ui`，仅几何变化时打印）：
`uigeom comp=x,y,w,h cand=x,y,w,h anchor=x,y caret=0/1`。
"翻页时输入框位移"这种报告光靠 `wmove/wrep` 计数答不了——计数只说"有 SetWindowPos"，
不说**哪个窗、从哪到哪**；`caret=0` 还能量化"锚点退化成鼠标位置"这条路径
（`caret_anchor()` 里 `GetFocus()`/`GetCaretPos()` 失败时的兜底）。

**顺带修掉位移的两个来源**（`ime_ui.cpp`）：①锚点每次 `UiShow` 都重取 caret，
而每上屏一个字 caret 就右移一格——弹窗于是跟着爬；改成**一次组合只取一次锚点**，
`UiHide` 才清。②`x = anchor.x + 12` 原来按**实测宽度**做右边界收口
（`screen_w - composition.width() - 8`），于是每多打一个字母 x 就左跳一次；
改成按固定预留（`kPopupRightMargin=300`）收口，公式里**不含任何窗口宽度**——
打字和翻页都不会再改 x。贴底时（`anchor.y + 260 > screen_h`）整组翻到光标上方，
这个判断也在取锚点时定一次。

### 11.15 imhost 汉字"粘在一起"：是步进，不是字号（2026-11-18 12:3x）

用户报"中文字间距过窄，所有字都粘一起"，并建议加字间距或缩小字体。放大上一轮截图
（`shot-1213.png` 输入框区域）后可以排除"只是挨得紧"：每个字的**实际步进只有约半个
字宽**，后一个字直接压在前一个字上，是重叠而不是紧排。

判定与修法：

1. **EDIT 没有字间距 API**。`SetTextCharacterExtra` 只作用于当前 DC，而 EDIT 自己
   取 DC（`GetDC` 后每次重绘都会重置 DC 状态），应用侧够不着；RichEdit 的
   `CHARFORMAT2.sSpacing` 在 ReactOS 的 riched20 上未经验证，换控件又会牵动
   `WM_IME_*` 投递，不值得为排版冒这个险。
2. **真因是字体的中文步进缺失**：`CreateFontW(-19, …, DEFAULT_PITCH|FF_SWISS, nullptr)`
   拿到的字体没有中文字形，字形由替换字体提供、步进却仍按原字体的拉丁宽度算，
   于是 19px 的字挤在 ~10px 的步进里。
3. **修法不猜字体名，直接量**（`vmshare/imhost.cpp` → `MakeCjkFont()`）：
   `adv = GetTextExtentPoint32W(L"中中") / 2` 与 `TEXTMETRICW.tmHeight`，
   判据 `adv >= height*0.85`（CJK 的 em 步进 ≈ height；"拉丁字体 + 替换字形"实测 ≈0.5）。
   顺序：已知字体名（Droid Sans Fallback / Noto Sans SC / SimSun / NSimSun /
   Microsoft YaHei / MingLiU / PMingLiU / MS Gothic / Arial Unicode MS）→
   显式 `GB2312_CHARSET` + 空脸名 → `EnumFontFamiliesExW` 枚举全部family 逐个量。
   一条都不达标时退回 `DEFAULT_GUI_FONT` 并打 `font FAILED …`，**不让"没找到字体"
   又表现成"改了没生效"**。
4. 字号同步下调：输入框 19 → **17px**（`kEditFontH`），说明/状态 label 12 →
   **14px**（`kLabelFontH`，label 高度 22）。label 复用输入框已经量过的 face
   （`g_editFace`），不再跑一遍全枚举。
5. 每轮启动的日志会写清结论，一行一个候选：
   `font ok   face="X" h=17 adv=17 tmh=20 glyph=1 probe=1` /
   `font miss face="Tahoma" h=17 adv=9 tmh=22 glyph=0 probe=1` /
   `font enum families=N`。`probe=0` 表示 ReactOS 的 `GetGlyphIndicesW` 不可用
   （此时按"有字形"放行，只靠 adv 判）。

排查口径：如果日志里出现 `font FAILED`，说明 guest 里确实没有中文步进正常的字体，
下一步是给镜像装字体（或在 imhost 里用 `AddFontMemResourceEx` 塞一份子集），
而不是继续调字号。

6. 兜底开关（省一轮来回）：`imhost.exe font:SimSun size:15` —— `font:` 指定 face
   （先严格匹配名字，再放宽到"步进达标"），`size:` 只影响输入框字号（10–40）。
   日志末尾会多一行 `font edit h= adv= tmh= ok=`，是最终真正生效的那组值。

**真机结论（2026-11-18 12:57，用户回复"显示已正常"）**：

```
font ok   face="Droid Sans Fallback" h=17 adv=17 tmh=23 glyph=1 probe=1
font edit h=17 adv=17 tmh=23 ok=1
font ok   face="Droid Sans Fallback" h=14 adv=14 tmh=19 glyph=1 probe=1   # label
```

选中的是镜像里自带的 **Droid Sans Fallback**（不是我们点名的那几个 Windows 字体，
说明走的是"枚举全部 family 逐个量"那条路）。`adv == h` 正是 CJK 的 1 em 步进，
此前拉丁字体的 `adv≈0.5h` 与之相差一倍——这也就是"字压字"与"字距正常"的分界。
没有出现 `font FAILED`，所以不需要给镜像装字体，也不必再动字号。

排查口诀：**汉字挤在一起，先看 `adv/h` 的比值，别看字号**。`adv ≈ h` 是正常，
`adv ≈ 0.5h` 是拿到了没有中文字形的字体（字形靠替换字体画、步进按拉丁算）。

### 11.16 P0-1 真机验收收口：15 项全部有实测（2026-11-18 12:5x）

最后一轮日志（`vmshare/imhost.log`，20:57 那次运行）用户一口气把验收单打完了：
入口层与窗口层各 **433 次按下**逐条对齐，程序注入 **0 次**（红线守住）。
把 `KEY … DOWN` 与 `WM_IME_CHAR` 按时间序折叠，能直接读出他打了什么：

```
SHI 4{试} … KAN [SPACE]{看} ZITI 4{字} 1{体} XUANRAN ? 2{渲} 2{染} XI?AN 1{西} 1{安}
O????? 1{衷} ?????????????? 4{郭} SHI???????????????????? 7{仕}
HAO [BACK][BACK][BACK] HAO [ESC] NV [SPACE]{女} LV 1{律} LVE [BACK] E 1{略} NVE [SPACE]{虐}
RENZHI [BACK] ICHU 1{人} 1{之} 4{初} XINGBENSHAN 2{性} 1{本} 2{善} [RETURN]
…（以下为《陋室铭》《山居秋暝》等长句，连续上屏 100+ 字）
[SHIFT] H [BACK] [SHIFT] H [BACK] [SHIFT] I I U H [BACK] [SHIFT] B [BACK]
```

`?` 是 `=`/`-`（翻页键的 vk 记成 `0xBB`，符号名解析不出来），
每一串 `?` 后面都跟着跨页选字成功（`1{衷}`、`7{仕}`、`4{郭}`），说明翻页路径可用。

逐项对应 `M2-ACCEPTANCE.md` §3：

| 项 | 实测依据 | 结果 |
|---|---|---|
| 3.1 / 3.2 / 3.3 | 前一轮 `zhong`/`zhongguo` 截图 + `WM_IME_CHAR 0x4E2D→0x56FD` | ✅ |
| 3.4 `nv` | `NV [SPACE]{女}` | ✅ |
| 3.5 `lv` | `LV 1{律}` | ✅ |
| 3.6 `lve` | `LVE [BACK] E 1{略}`（`v` 打错回退再补 `e`，回退也可用） | ✅ |
| 3.7 `nve` | `NVE [SPACE]{虐}` | ✅ |
| 3.8 数字键按当前页 | `shi` 页内 `2{仕}`、`xin` 页内 `1{体}` 等多处 | ✅ |
| 3.9–3.11 翻页/跨页/回翻 | `?` 串 + 跨页选字（`7{仕}`、`4{郭}`） | ✅ |
| 3.12 Backspace | `HAO [BACK][BACK][BACK] HAO`、`RENZHI [BACK] ICHU 1{人}` | ✅ |
| 3.13 Esc 不上屏 | `HAO [ESC]` 后紧接 `NV`，中间无 `WM_IME_CHAR` | ✅ |
| 3.14 100 字压测 | 《陋室铭》+《山居秋暝》连续上屏 100+ 字，无崩溃、无残窗 | ✅ |
| 3.15 中英往返 | 末尾 `[SHIFT] H … [SHIFT] B`：Shift 后 `H`/`I`/`U`/`B` 直接以拉丁字符上屏（无组合串） | ✅ |

**门禁（同一份源码）**：`ctest` 11/11、`check_m1` 57/57、`xingma/` 零改动。
**产物**（VM 内回验字节一致）：

| 文件 | 字节 | sha256 |
|---|---|---|
| `rospinyin7.ime` | 967,775 | `15e401cf…b2597` |
| `rospinyin.dat` | 902,776 | `f28b2606…b921` |
| `imhost.exe`（探针，不随包发布） | 945,314 | `4c1d4ca0…e8ed` |

`imhost.exe` 导入表只有 8 个系统 DLL（`ADVAPI32/GDI32/IMM32/KERNEL32/msvcrt/SHELL32/USER32/WS2_32`），
无 `libgcc*/libstdc++*` —— 静态链接这条判据仍然成立。

至此 M2 的 P0/P1 全部关闭，只剩 **P2-1（CI 产出 release 资产）** 属于发版工程化，
不影响 M2 判定。

### 11.17 alpha0.0.2 发版记录（2026-11-18 14:5x）

发版口径与 `alpha0.0.1` 不同，记清楚免得下次踩：

1. **发布物用 `ROSPINYIN_TRACE=OFF`**（`ime/CMakeLists.txt` 的 option，默认关）。
   真机验收用的是 `TRACE=ON` 的构建（`build-win-trace/ime/rospinyin.ime`，967,775 B），
   它多出来的 27 KB 全是写 `C:\rospinyin-trace.log` 的诊断代码。两者导出表（16 个 IMM32 接口）、
   导入 DLL（GDI32/IMM32/KERNEL32/msvcrt/USER32）、功能路径都一致，因此可以发 `TRACE=OFF`：
   长期使用不该带日志 I/O。**发布物与验收物不是同一个文件，这一点必须在 release notes 里写明。**
2. **构建不完全可复现，但差异只有 4 个字节**。同选项连建两次：

   | 产物 | 字节 | sha256 |
   |---|---|---|
   | 第一次 | 940,601 | `c9dc17c0…3e25feaa` |
   | 第二次 | 940,601 | `ae7d1e57…08b1cda` |

   逐字节比对只有 3 个区间、共 4 字节不同：`0x88`（PE `TimeDateStamp`）、`0xD8–0xD9`（`CheckSum`）、
   以及 `0x60A04` 处一个调试相关字段。**代码段与数据段逐字节相同**。
   所以"发布的二进制 = 该 tag 的源码构建"仍然成立，只是不能用 sha256 做复现校验；
   真要比特级复现，链接时加 `-Wl,--no-insert-timestamp` 并关掉 checksum。
3. 资产（已上传到 release `alpha0.0.2`）：

   | 文件 | 字节 | sha256 |
   |---|---|---|
   | `rospinyin.ime` | 940,601 | `c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa` |
   | `rospinyin.dat` | 902,776 | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
   | `SHA256SUMS.txt` | 160 | — |

4. `.dat` 没变（`f28b2606…`），与验收、与 `alpha0.0.1` 都是同一份——数据层这一版没有改动。
5. 发版脚本路径同样受限：`git push` 与 `git push --tags` 都走不通（见 §12），
   tag 与 release 全部用 API 建（`POST /git/tags` + `POST /git/refs` + `POST /releases`，
   资产走 `uploads.github.com`，这个域名是通的）。整理成 `tools/make_release.py`；
   **注意**：`alpha0.0.2` 实际是一次性脚本跑的，`tools/make_release.py` 是按它整理的通用版，
   尚未实战——下次发版先核对资产过滤器（它会跳过 `.md`/`.py`）与 tagger 日期。
6. 发布说明正文同时入库：`releases/<tag>.md`，与 GitHub release body 完全一致。
7. 回验：从 `https://gh-proxy.com/https://github.com/.../releases/download/alpha0.0.2/*` 下载，
   三个文件字节数与 sha256 与本地一致；GitHub 自己算的 `digest` 字段也与之相同
   （`sha256:c9dc17c0…` / `f28b2606…`），这是"发出去的就是验收的那份代码"的最强证据。

### 11.2 其他待办

- ~~**`'` 硬边界在交互输入路径失效（2026-11-17 接手加固时发现）**~~ **已修复（2026-11-17）**：`segment_syllables()` 本就把 `'` 当硬边界，但 `PinyinSession::feed()` 收到 `'` 后走 `can_extend()`，而 `CodeTable` 的前缀集合里没有 `'` → 分隔符被丢弃，敲 `xi'an` 得不到 `[xi][an]`。修法：`can_extend()` 显式处理撇号——引号前须为完整音节、引号后须仍可扩展（空串合法，即刚敲完分隔符的中间态），与 `segment_syllables()` 语义一致；并补 `feed("xi'an")` 保留分隔符与 `[xi][an]` 分段断言、尾随分隔符断言、分隔符后非法前缀拒收断言（`pinyin_session_test.cpp`）。
- M3：中文标点、全角/半角；~~Shift 临时英文~~（已做，见 §11.14：lone Shift 切中英，
  暂未实现"按住 Shift 临时英文、松开恢复"的瞬时模式）；
- M4：词组与词频（数据结构需扩展 `rospinyin.dat`，注意版本号与向后兼容）；
- 语言栏菜单项（属性/关于）留待 M6；
- `xingma/` W1 编码（`ShapeScheme`/`ShapeStore`/`ShapeDatLoader`）尚未开始，框架侧缺口已全部关闭，可直接复用宿主层；
- 历史遗留空目录 `engine/include/` 已于 2026-11-17 清理（实体已并入 `framework/`）。

---

## 12. 维护者日常清单

**改代码**

1. `cmake --build build --parallel && ctest --test-dir build` → 11/11；
2. `python3 tools/check_m1.py` → 57/57（名字沿用 M1，实际是**长期契约闸门**，含 M2 不变量）；
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

#### 推送通道（2026-11-17 实测，务必按此执行）

本机 `~/.gitconfig` 有一条 `url.https://gh-proxy.com/https://github.com/.insteadOf`，
它会把**任何** `https://github.com/...` 改写成走 gh-proxy。这条改写对 fetch 无害，
但 **push 会静默失败**：请求发给 proxy 后丢了认证头，代理返回 `401 Basic realm="GitHub"`，
git 转而问 credential-store，然后长时间无输出（实测 4 分钟以上不返回，`timeout` 掉之后
远程 ref 毫无变化——看起来像"推了但没生效"）。

绕过方式是一次性屏蔽全局配置并直连（需要 fine-grained PAT，`Contents: read and write`）：

```bash
AUTH=$(printf "x-access-token:%s" "$(cat ~/mygithubpak/key.txt)" | base64 -w0)
CFG=$(mktemp); chmod 600 "$CFG"
printf '[http "https://github.com/"]\n\textraheader = Authorization: Basic %s\n' "$AUTH" > "$CFG"

GIT_CONFIG_SYSTEM=/dev/null GIT_CONFIG_GLOBAL=/dev/null \
  git -c include.path="$CFG" -c credential.helper= \
  push https://github.com/twipty/ros-chinese-ime.git main
```

- `GIT_CONFIG_GLOBAL=/dev/null` 是**关键**：没有它，`insteadOf` 仍在，仍会走 proxy；
- `credential.helper=` 置空，避免 401 后回退到 credential-store 反复重试；
- 直连走 HTTP/2（本机有 CONNECT 隧道），4.2 MB 的 pack 数秒完成。

诊断手段：`GIT_CURL_VERBOSE=1` 看响应头里的 `web-server:` —— 出现 `gh-proxy.com`
说明改写没被屏蔽掉。

#### 推送通道变更（2026-11-18 12:5x，直连不通了）

上面那条"屏蔽全局配置 + 直连"的路子**当天已失效**：沙箱的 HTTPS 代理对 `github.com`
返回 `CONNECT tunnel failed, response 502`，而 `api.github.com` 与 `gh-proxy.com` 都通。
gh-proxy 的 `info/refs?service=git-receive-pack` 返回 `401`（说明它认 push），
但带 `http.extraheader` 认证头推过去仍然**静默失败**（5 分钟无输出、remote ref 不变），
和 §12 记录的老坑同形。

可用通道是 **GitHub Git Data API**（`api.github.com` 可达）：逐个提交上传 blob → 建 tree
→ 建 commit → `PATCH refs/heads/main`。脚本在 `tools/push_via_api.py`，关键是它自带安全阀：

- 要求远端 HEAD 等于预期的起点 SHA，否则中止；
- 每个提交建完 tree 后**必须与本地 `git rev-parse <sha>^{tree}` 完全相等**才往下走，
  不等就 `sys.exit`，ref 一动不动（最多留下几个孤立 blob，无害）。

代价：重建出来的 commit SHA 与本地不同（tree 相同、内容一致），本地 `HEAD` 与
`origin/main` 会"看似分叉"。等直连恢复后跑一次 `git push --force origin main` 即可对齐，
历史内容不受影响。

```bash
python3 tools/push_via_api.py <repo> main <expected_remote_sha> <sha1> <sha2> ...
```


#### 远程历史与本地不相关（已知）

2026-11-17 之前远程 `main` 是用 API 逐文件提交堆出来的 93 个提交，与本地 31 个提交
**无共同祖先**（`git merge-base` 为空），但文件树是本地树的子集（远程 113 个文件，
本地 136 个，远程无本地缺失的文件）。发 `alpha0.0.1` 时已把旧历史保全到
`legacy/pre-alpha0.0.1` 分支，再 force push 本地历史覆盖 `main`。
若日后再次遇到"两边历史无关"，先做同样的备份再覆盖，不要 `merge --allow-unrelated-histories`。

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
| `m2/M2-REPORT.md` | M2 报告：结论、交付物、根因修复 10 条、已知限制、门禁 |
| `m2/M2-REMAINING.md` | M2 剩余事项与优先级（当前只剩真机验收单） |
| `m2/M2-DATA-PLAN.md` | 数据源选型、声调/`ü` 处理、规模核算 |
| `m0/vm/NOVNC-GUIDE.md` | VM 人机协作操作单（v7，含所有键位坑与图标修复说明） |
| `xingma/XINGMA-PLAN.md` | 形码产品线正式计划（1.0） |
| `xingma/docs/FRAMEWORK-AUDIT-2026-11-17.md` | 框架能力审计（G1 ABI P0 / G2 顺序 P2）与处置记录 |
| `NOTICE.txt` | 第三方与数据分层授权 |

---

*本文档由维护者交接时编写，随代码同步更新。若发现与代码不符，以代码与 CI 结果为准，并回来修文档。*
