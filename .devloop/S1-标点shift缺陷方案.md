# M3 标点缺陷修复方案（shift 丢失 · 书名号 · 热键重定位）

> **已取代（2026-11-23 18:18）**：本文件为 v1 历史记录，实现依据以
> `.devloop/S1-影响面与方案.md` **v3** 为准。v1 中「凡需 shift 的符号全部失效」的
> 笼统判定已被截图证据修正：中英态门控正常，shift 组合键在英文态下出半角属正确行为；
> v3 只补中文态键码映射与多码点接口，并把 Ctrl+Shift+Space 解绑为 no-op。
> 以下内容仅供追溯，不得作为实现依据。

> 状态：待 owner 审阅，**本轮不改代码**。
> 触发：owner 17:46 实机反报 + 宿主测试判定 + 截图证据。
> 关联：M3 第一批（中文标点）交付后的首个回归缺陷。

## 判定结论（已坐实，不是猜测）

**问题在输入法的键翻译层丢了 shift 语义，不是"提交了中文标点却 fallback 成英文符号"。**

三重证据：

1. **宿主单测全绿**：`punctuation`（23 键全量，含 `?` `!` `<` `>`）与 `host-key`（给定正确字符 → 提交全角码点）都通过。这两段是纯宿主代码，与 IME 二进制无关 —— 只要喂对字符，转换与提交链路就是对的。
2. **截图证明显示正常**：`m0/vm/evidence/m3-punct-issue-20261123-174617.png` 里诗句「空山新雨后**，**」的全角逗号渲染完全正确 —— 同样经 `WM_IME_CHAR` 提交、同一字体路径，系统支持中文标点显示，无 fallback。
3. **代码定位**：`framework/platform/win32/imm32/key_translate.cpp:39-51` 的 `framework_key_char(UINT vk)` 只按虚拟键码返回字符，**签名里没有 shift**：
   - `VK_OEM_2`（`/`，shift 后才是 `?`）不在映射表 → `ch = 0` → `wants_key` 不认领 → 应用直接收到半角 `?`；
   - `VK_OEM_COMMA` / `VK_OEM_PERIOD` 恒返回 `,` / `.` → shift+`,` 本应是 `<`、shift+`.` 应是 `>`，却仍按 `,`/`.` 处理 → 出 `，`/`。`，与未按 shift 一致；
   - `!`（shift+`1`）因数字键不映射符号、`:`（shift+`;`）因 `VK_OEM_1` 未登记，同样失效。
   - 现象模式完全吻合：**凡需要 shift 才产生的符号全部失效，只有不需要 shift 的 `,` `.` 正常**。

## 技术路线

1. **键翻译层引入 shift 感知（核心修复）**
   - 现有 `framework_key_char(UINT vk)` 改为 `framework_key_char(UINT vk, bool shift)`，按 US 布局补齐 OEM 符号键与数字行的 shift 映射。
   - `key_event_from()`（`key_translate.cpp:55-82`）当前第 59 行就算 `event.ch`、第 62 行才算 `shift` —— 需把三个 modifier 的计算提到 `event.ch` 之前，再把 `shift` 传入。
   - 双路径一致性：`ImeProcessKey`（`imm_entry.cpp:216-276`）与 `ImeToAsciiEx`（`:278-291`）共用同一个 `key_event_from(vk, ..., lpbKeyState)`，两条路径天然拿到相同的 shift 与 ch，不会重演"Ctrl+Shift+G 的 G 进拼音串"那类双路径分歧。

2. **书名号替换尖括号（owner 已拍板）**
   - `framework/include/imfw/punctuation.hpp:36-37`：`<` 从 `U+FF1C`（＜）改为 `U+300A`（《），`>` 从 `U+FF1E`（＞）改为 `U+300B`（》）。
   - 同步 `tests/punctuation_test.cpp:54-55` 两条断言。
   - 共享表只此一份，PinyinScheme 一行转发，形码将来接入自动获得书名号 —— 符合 M3 v2 方案的"表归 framework 公共层"约束。

3. **Ctrl+Shift+Space 重定位（owner 已拍板方向，细节待确认）**
   - 方向：不再作为中英切换（与单 Shift 完全等价），**预留给后续的全角/半角切换专用**。
   - 本轮**不实现**全角/半角切换（owner 原话"后续"），只做键位解绑/标注。预留期间的按键行为见下方"待确认决策点"。

## 实现流程

1. **红灯测试（先写断言，不动实现）**
   - 新增 Windows-free 的键码映射纯函数测试：全量断言 15 类按键 × shift 两态（见"改造清单"为何要抽公共函数）。
   - 改 `punctuation_test.cpp` 的 `<` `>` 两条断言为书名号码点。
   - 按热键处置结论调整 `host_key_test.cpp` / `host_smoke_test.cpp` 中 LanguageToggle 相关断言。
2. **绿灯实现**：按技术路线 1-3 落地。
3. **门禁**：ctest 双配置（BUILTIN_TABLE ON/OFF）、check_m1、check_docs、check_mkdat、seams cap。确认 dat 指纹 `db0c7bcf` 与 zhong 快 5 / 全 12 页不回退。
4. **重建部署**：release `.ime`（BUILTIN_TABLE=OFF / TRACE=OFF）→ HTTP 8000。
5. **实机验收**：owner 重装重启后验全部标点；我抓图取证。

## 架构图

```
物理按键 (vk + shift 状态)
        │
        ▼
key_event_from ──► framework_key_char(vk, shift)   ← 本轮修复点（丢 shift 的根源）
        │                    └─ 数字行/ OEM 1,2,3,4,5,6,7 / MINUS / PLUS
        ▼
KeyEvent{ vk, ch, ctrl, alt, key_up, kind }
        │
        ├──► wants_key()        空组合认领标点
        └──► handle_key()
                 └──► route_key(scheme, event, composing)
                            ├── composing → 翻页（, . - = 不变）
                            └── idle → IntentKind::Punctuation(ch)
                                          └──► fullwidth_punctuation(ch)  ← 本轮改书名号
                                                    └──► emit_commit → WM_IME_CHAR → 上屏
```

两条 IMM32 路径都进同一个 `key_event_from`，修复集中在单点。

## 改造清单

| 文件 | 改动 | 说明 |
|---|---|---|
| `framework/include/imfw/key_char.hpp`（新增） | Windows-free 纯函数 `char32_t key_char(unsigned vk, bool shift)` | **可宿主单测的关键**：`key_translate.cpp` 在 `framework/platform/win32/` 下、依赖 Windows 头，当前无宿主测试。抽成公共纯函数（用与 Win32 一致的数值 vk 常量，不含 `<windows.h>`）后才能 TDD。与 `punctuation.hpp` 同套路。 |
| `framework/platform/win32/imm32/key_translate.cpp` | `framework_key_char` 改为调用新纯函数并传 shift；`key_event_from` 调整 modifier 计算顺序 | 平台层只保留 vk 语义，映射逻辑下沉到可测的纯函数。 |
| `framework/include/imfw/punctuation.hpp` | `<` `>` → 《》 | 共享表唯一改动。 |
| `tests/punctuation_test.cpp` | 两条断言改书名号 | - |
| `tests/key_char_test.cpp`（新增） | 全量 shift 映射断言 + CMake 注册 | 红灯核心。 |
| `tests/host_key_test.cpp` / `host_smoke_test.cpp` | 按热键处置结论调整 | 若解绑则删/改 LanguageToggle 断言。 |
| `framework/include/imfw/host.hpp` / `key_translate.cpp` / `imm_entry.cpp` | 按热键处置结论调整 LanguageToggle 识别与消费 | 若解绑，删 `KeyKind::LanguageToggle` 及相关分支。 |
| `framework/src/toolbar_text.cpp` / `tests/toolbar_text_test.cpp` | 无需改 | 中英态显示仍按 `fOpen`，本轮不碰工具栏。 |
| `docs/handover/CHANGELOG.md` | 新增缺陷修复条目 | 文档同步。 |
| `docs/plan/ROADMAP.md` | 记录热键预留用途 | 若解绑，注明 Ctrl+Shift+Space 预留给全角/半角切换。 |

**不改**：`rospinyin.dat`（`db0c7bcf`）、tier 分级、拼音引擎、形码侧、UI 布局、安装器/CI。

### 键码映射表（US 布局，需实机确认 guest 为 EN 布局）

| vk | 键 | 无 shift | shift |
|---|---|---|---|
| 0x30–0x39 | 数字行 | `0`–`9` | `) ! @ # $ % ^ & * (` |
| 0xBD | `-` | `-` | `_` |
| 0xBB | `=` | `=` | `+` |
| 0xBC | `,` | `,`（翻页） | `<` → 《 |
| 0xBE | `.` | `.`（翻页） | `>` → 》 |
| 0xBA | `;` | `;` | `:` |
| 0xBF | `/` | `/` | `?` |
| 0xC0 | `` ` `` | `` ` `` | `~` |
| 0xDB / 0xDC / 0xDD | `[` `\` `]` | `[` `\` `]` | `{` `\|` `}` |
| 0xDE | `'` | `'` | `"` |
| 0x20 / 0x6D / 0x6B | 空格 / 小键盘 `-` `+` | 原值 | 原值 |

## 静态盲区

- **键盘布局假设**：上表是 US 布局。guest 右下角当前显示 `EN`，符合；若用户切到别的布局，shift 映射会错位 —— 方案不处理非 US 布局，文档注明。
- **`lpbKeyState` 的 shift 位**：两条 IMM32 路径都从 imm32 拿到同一份键盘状态数组。框架已按此设计（Ctrl+Shift+G 的 ctrl/shift 判定在用），但 shift+标点的新用法仍需实机确认 imm32 在 `ImeToAsciiEx` 时是否保留了 shift 位。若否，需改用 `ToUnicodeEx` 直接取字符 —— 这是备选路线，不在本轮首选方案内。
- **书名号字体覆盖**：U+300A/U+300B 是上屏文本（走系统字体，不走 dat 的 tier 分级）。Droid Sans Fallback 覆盖情况需实机看截图确认；若显示豆腐块，再评估是否改回尖括号或换字体。
- **组合态按 shift+数字**：映射后 `shift+1` 的 ch 变成 `!`，组合中不会选字、也不出标点（标点分支要求 idle）。这是合理行为，但需在红灯测试里钉死断言，防止回归成"选字"或"误上屏"。

## 实现约束

- **不碰** dat 指纹 `db0c7bcf`、tier 分级、拼音引擎、形码侧、UI 布局、安装器/CI。
- `fullwidth_punctuation()` 仍是唯一共享表；书名号改动只此一份，禁止在 scheme 里另写映射。
- 不改 `route_key(scheme, event, composing)` 的纯函数签名与 X2-c 分层契约；X2-c 的 check_m1 签名断言保持现状。
- 若 seams cap 因新增调用点超限，必须在 `tools/seams.py` 写明是 M3 新增的合法调用点，不得裸放宽。
- 发布构建仍须 `BUILTIN_TABLE=OFF / TRACE=OFF`（win 实机不编内建兜底表）。

## 待确认决策点（热键细节）

owner 已定"Ctrl+Shift+Space 留作后续全角/半角切换专门使用"。本轮**不实现**全角/半角切换，因此预留期间的按键行为需要拍板：

| 选项 | 本轮行为 | 代价 / 收益 |
|---|---|---|
| **A（推荐）：解绑为 no-op** | 删除 LanguageToggle 的中英切换语义；按键被 IME 吞掉但不产生动作，代码/文档标注"预留给全角/半角切换" | 彻底消除与单 Shift 的冗余；但用户按它暂时没反应，需文档说明 |
| B：保留现状，仅文档标注 | 仍切中英，文档标注未来用途 | 零代码改动；但冗余问题没解决，实机仍与单 Shift 一样 |
| C：本轮就实现全角/半角切换 | 把 `punct_fullwidth` 从 `fOpen` 解耦，Ctrl+Shift+Space 切它 | 键位立即有用；但超出本轮"只修 shift"范围，需重开 S0 补问开关语义/工具栏显示 |

请 owner 在 A / B / C 中选一个，我再进入实现。
