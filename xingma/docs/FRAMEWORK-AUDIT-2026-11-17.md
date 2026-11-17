# 框架能力审计（G1–G4 复核）— 2026-11-17

> **审计性质**：对 `framework/`（HEAD = `440cf52`）实读源码后的逐项复核，目的是验证两个框架提交
> （`9627cd3` 关闭 G1–G4、`440cf52` G2 终局形态）是否真正兑现 XINGMA-PLAN 3.4 的依赖承诺。
> **本目录对框架只读不改**（边界规则见 `xingma/README.md`）；审计发现的问题全部在此记录，
> 由框架侧（ros-pinyin）自行采纳修复，形码侧只把它当作接入前置门禁。
>
> **2026-11-17 处置更新**：框架侧已按 §2.1/§2.2 建议完成修复——`ImeToAsciiEx` 改收
> `TRANSMSGLIST`（消息从 `list->TransMsg` 写、容量取 `min(uMsgCount, kMaxEmit)`，
> 附 `offsetof` 布局断言），`reload()` 改 data-first；`check_m1.py` 新增 G1 ABI 与
> reload 顺序断言（现 50/50），`emit_text_test` 增加 TRANSMSGLIST 布局测试（ctest 10/10），
> 真实 MinGW 头交叉检查 12/12。W2 装配门禁仅剩：**VM 词组上屏用例实测**
> （M2 验收单执行后更新本注记）。

## 一、总体结论

| 缺口 | 框架提交声称 | 实读源码结论 | 对形码的影响 |
|---|---|---|---|
| G1 多码点上屏 | 已解决（`imfw::emit_text`） | **逻辑已解决，ABI 有缺陷**（见 2.1）：多消息写入位置与 ReactOS 实际读取位置错位，**上屏文本会错乱/截断** | **阻塞**：W2 装配前必须确认框架已修（5.1 门禁） |
| G2 零码表/热重载 | 终局形态（无参 `create_engine` + `reload_data()`） | **属实**，且形码完全自管数据，无需哨兵约定 | 无阻塞；一处顺序矛盾见 2.2（P2，不阻塞） |
| G3 配置入口 | 已解决（`Scheme::configure`） | **属实**：回调已接入 `ImeConfigure`，成功后触发 `reload_sessions()` | 无阻塞 |
| G4 键策略/UTF-8 组合串 | 已解决（策略下放 + `utf8_to_wide`） | **属实**：策略在 Scheme；组合串契约 UTF-8，CJK 可进组合窗 | 无阻塞 |

验证基线（审计时实测）：`ctest 10/10` 通过、`check_m1.py 47/47` 通过 ——
即现有测试**没有覆盖**下述 2.1 的 ABI 缺陷，这正是它能通过提交检查的原因。

## 二、发现的问题

### 2.1 【P0·阻塞】G1 多消息上屏与 ReactOS TRANSMSGLIST 契约错位

**证据链（ReactOS 官方源码 `win32ss/user/imm32/keymsg.c`，`ImmTranslateMessage`，master HEAD）**：

```c
/* allocate a list */
cbList = offsetof(TRANSMSGLIST, TransMsg) + MSG_COUNT * sizeof(TRANSMSG);
pList = ImmLocalAlloc(0, cbList);
...
pList->uMsgCount = MSG_COUNT;                      /* 容量头，IMM32 预填 0x100 */
kret = pImeDpi->ImeToAsciiEx(vk, ..., pList, 0, hIMC);
if (kret <= 0) goto Quit;
/* post them */
if (kret <= MSG_COUNT)
    ImmPostMessages(hwnd, hIMC, kret, pList->TransMsg);   /* 从 ->TransMsg[0]（偏移 4）读 kret 条 */
```

即：`ImeToAsciiEx` 的缓冲参数**不是裸 `TRANSMSG` 数组**，而是 DDK `immdev.h` 的
`TRANSMSGLIST { UINT uMsgCount; TRANSMSG TransMsg[1]; }`——首字段是 IMM32 预填的容量头，
消息数组从偏移 4 开始；返回值 = 写入的消息条数，IMM32 按该条数从 `->TransMsg[0]` 读回。

**框架现状（`framework/host/imm_entry.cpp`，HEAD `440cf52`）**：
`ImeToAsciiEx` 把缓冲当裸 `TransMsg*` 数组从**偏移 0** 起写（`emit_text(lpTransMsg, ...)` →
`imfw::emit_text(reinterpret_cast<TransMsgSlot*>(messages), kMaxEmitMessages, text)`）。
后果（按 i686 目标，槽 = 12 字节推演）：写入的槽[0] 落在 `uMsgCount` 容量头 + `TransMsg[0]` 前半，
槽[1] 横跨 `TransMsg[0]` 尾部与 `TransMsg[1]` 前部……整体错位一格。IMM32 从 `->TransMsg[0]`
（偏移 4）读回的「消息号」实际是**字符码本身**（如 0x56FD 被当成消息号）、`wParam = 0`——
**单字与词组全部无法送达应用**，发出的是无效消息号。

**为何现有验证没抓到**：
- M1 VM 实测是 passthrough（从不写消息），50 次切换压力全过；
- M2 VM 验收单（`m2/M2-ACCEPTANCE.md`）**尚未执行**（全部 ☐）；
- Linux 单测只测 `imfw::emit_text` 的编码逻辑（数组语义正确），不触达 IMM32 缓冲布局；
- `check_m1.py` 只断言字符串存在，不校验 ABI 布局。

M1 时代的 `emit_char` 同样从偏移 0 写（写 `messages[0]` 后返回 1），但 M1 VM 实测走的是
passthrough 路径，从未触达该代码，因此历史实测通过与本缺陷并存。

**并行工作区观察（2026-11-17 上午）**：审计期间工作区曾短暂出现 framework 两文件
（`imm32_abi.h`/`imm_entry.cpp`）内容与本节修复建议一致但措辞不同的未提交改动，
随后恢复 HEAD。与框架侧并行工作可能存在——采纳修复时请先 `git status`/`git log`
核对是否已由框架侧提交，避免重复实施。

**修复建议（框架侧，三处小改）——✅ 已由框架侧采纳实施（2026-11-17 下午）**：
1. `framework/host/imm32_abi.h`：已补 `struct TransMsgList { UINT uMsgCount; TransMsg TransMsg[1]; };`，
   `ImeToAsciiEx` 签名第 4 参已改为 `rpimm::TransMsgList*`；
2. `framework/host/imm_entry.cpp`：`emit_text` 已改为从 `list->TransMsg` 起写、容量取
   `min(list->uMsgCount, imfw::kMaxEmitMessages)`；已加
   `static_assert(offsetof(rpimm::TransMsgList, TransMsg) == sizeof(UINT), ...)`；
3. `tools/check_m1.py`：G1 断言已同步（新增 2 条 ABI 契约检查，共 50 条）。
4. 剩余事项：**M2 VM 验收单仍须重跑**——词组上屏（≥2 字）是唯一能在真机暴露本缺陷的用例。

> **审计勘误**：本文件早前一版把后果描述为「文本错位一格、错字输出」，经按 12 字节槽
> 逐字节推演修正为现文——由于错位量不足一个完整槽，IMM32 读到的 `uMsg` 字段实为
> 字符码本身，属于**更严重的整串不可达**，结论方向不变（P0、阻塞 W2）。

### 2.2 【P2·不阻塞】G2 `reload_sessions()` 顺序：代码与文档矛盾 —— ✅ 已统一（data-first）

- **代码**（`framework/host/session_store.cpp` `Store::reload()`）：
  先 `states_.clear()` 后 `scheme_instance().reload_data()`；
- **文档/注释/提交信息**（`session_store.hpp`、`scheme.hpp`、`framework/README.md`、
  XINGMA-PLAN 3.4/5.2、`440cf52` 提交信息、`check_m1.py` 检查名）：
  均写「先 `reload_data()` 再清会话」。

对形码的实际影响：无论哪种顺序，下一键都会以新数据重建引擎（`acquire` 时重建），
**功能等价、不阻塞**。框架侧已按文档口径统一为 data-first（`reload_data()` 先行、
`states_.clear()` 随后），并新增 `check_m1.py` 顺序断言防回归。

### 2.3 【P3·备忘】其余核对项（均通过）

- G1 编码逻辑本身正确：UTF-16 码单元逐条、非 BMP 代理对、容量截断不留半对（`tests/emit_text_test.cpp` 覆盖）。
- G2 无参 `create_engine()` + `Scheme::reload_data()` 钩子契约清晰；`CodeTable`/`dat_format.hpp` 已降级为纯工具类。
- G3 `ImeConfigure` → `scheme.configure(parent)` → 成功后 `reload_sessions()`，默认维持 `FALSE`（拼音行为不变）。
- G4 `should_consume_key` 仅在「组合中且有候选」拦截选字/翻页键；组合串 UTF-8 契约 + 坏字节逐字节兜底；`ui/composition_window.cpp` 用 `utf8_to_wide` 计算下划线宽度。
- `kMaxEmitMessages = 32` 远小于 DDK 256 上限（修复 2.1 后还应受 `uMsgCount` 容量约束）。

## 三、形码侧行动项

| # | 行动 | 关联 | 阻塞 | 状态 |
|---|---|---|---|---|
| A1 | XINGMA-PLAN 3.4 G1 行补「ABI 缺陷」注记，指向本文 2.1 | §2.1 | — | ✅ 已完成 |
| A2 | W2 验收标准增列「框架 G1 ABI 修复已在 VM 词组上屏用例中验证」 | §2.1 | **是** | ⏳ 代码修复已落地，待 VM 词组用例实测 |
| A3 | 风险表新增「框架 G1 ABI 缺陷未修即装配」风险 | §2.1 | — | ✅ 已完成 |
| A4 | 5.2 `reload_data()` 措辞改为不依赖框架内部顺序 | §2.2 | — | ✅ 已完成（框架已统一 data-first） |
| A5 | 待决事项 Q5 立项：G1 ABI 修复责任方与时机（框架侧） | §2.1 | — | ✅ 已决：框架侧当日修复（本次提交） |

> 本审计只读框架源码并记录问题；P0/P2 均已由框架侧在 ros-pinyin 仓库内修复
> （`imm32_abi.h` + `imm_entry.cpp` + `session_store.cpp` + `check_m1.py` + `emit_text_test.cpp`）。
