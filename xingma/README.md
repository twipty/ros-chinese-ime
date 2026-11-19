# xingma/ — 形码输入法产品线（RosXingMa）

本目录只放**形码输入法专属**的东西，与 ros-pinyin 拼音产品的文件物理分开。
形码复用 ros-pinyin 抽出的核心框架，但**不修改它的结构、构建文件与说明文档**——那套重构由 ros-pinyin 侧定稿。

产品名、二进制名、注册表键等唯一口径见 `XINGMA-PLAN.md` 第 2.1 节（`RosXingMa`）。

## 目录内容

| 路径 | 内容 | 状态 |
|---|---|---|
| `XINGMA-PLAN.md` | 实施计划（**正式版 1.1**，唯一实施依据；新增 P0 前置门禁，原 W0–W5 顺延为 P1–P6） | 现行 |
| `docs/phase0/PHASE0-PLAN.md` | 第一阶段 P0 细化：框架门禁、注入缝、VM 词组上屏、红线新口径 | 现行 |
| `docs/glyph-tier/GLYPH-TIER-PLAN.md` | 字形分级与全字形开关方案：重码评估、分级码表、开关、未覆盖提示（**评审稿，未实现**） | 待评审 |
| `docs/glyph-tier/FRAMEWORK-REQ.md` | 对 framework 的需求：tier 字段、会话级 `[快]/[全]`（不存注册表）、探测链移除、接口草案与门禁 | 待框架方评审 |
| `archive/` | 历史草案存档，仅作追溯，不作依据 | 存档 |
| `docs/TABLE-LICENSE-RESEARCH.md` | 码表授权尽调：法律定性、13 个来源盘点、分发矩阵 | 已完成 |
| `docs/TABLE-IMPORT-SPEC.md` | 导入格式规范：CSV/TSV/fcitx5/ibus 语法、嗅探、校验、反导出 | 已完成（v1.1） |
| `docs/DATA-PACK-SPEC.md` | 可选数据包规范：`pack.ini` 字段、校验顺序、两份实际清单 | 已完成 |
| `packs/` | 86/98 数据包的模板与许可（**不含码表数据**） | 已完成 |
| `tools/mkdat-shape/make_pack.py` | 数据包构建/校验脚本（仅标准库） | 已完成，自测通过 |
| `include/roxingma/`、`src/` | `shape_table`、`shape_session`、`shape_store`、`shape_scheme`、`shape_dat_format` | 待 P2（原 W1） |
| `host/`、`res/` | `scheme_reg_shape`、`shape_dat_loader`、`shape_demo_table`、`rosxingma.def`、资源 | 待 P2/P3（原 W1/W2） |
| `tools/table-io/`、`tools/table-config/`、`tools/table-convert/` | 格式读写库、配置工具 GUI/CLI、PC 端转换脚本 | 待 P2–P5（原 W1–W4） |
| `tests/` | 形码引擎、切换、格式一致性、数据包测试 | 待 P2–P4（原 W1–W3） |
| `CMakeLists.txt` | 形码全部构建目标 | 待 P2（原 W1） |

## 边界规则

**放这里的判据**：只有形码需要 → `xingma/`；拼音也需要 → 属于框架层，留在 ros-pinyin 侧。

```
ros-pinyin/
  framework/   ← 共用核心框架（include/imfw、src）              ← ros-pinyin 维护，形码只读
                 host/ui 已于 X1 归位到 platform/win32/{imm32,ui}/
  pinyin/      ← 拼音产品（include/rospinyin、src）             ← ros-pinyin 维护
  xingma/      ← 本产品线，全部形码文件在此                       ← 本计划维护
```

**依赖方向单向**：`xingma/` → `framework/`。形码不往 `framework/`、`pinyin/` 里加文件，也不改根 `CMakeLists.txt`。

框架已交付 `imfw::Scheme` 插件契约，形码通过 `xingma/host/scheme_reg_shape.cpp` 挂一个实现进去，
宿主与界面源码直接按相对路径链接、不复制。G1–G4 已由框架侧解决（2026-11-16，见 `XINGMA-PLAN.md` 3.4）；
**2026-11-17 框架审计**发现 G1 多消息上屏存在 TRANSMSGLIST ABI 缺陷与 G2 重载顺序文档矛盾，证据见 `docs/FRAMEWORK-AUDIT-2026-11-17.md`；
**2026-11-18 现状**：G1 ABI 缺陷已修 & M2 VM 验收 15/15 通过，M0 红线 #2 降级为部署偏好（登录路径零失败初始化），
X1 目录归位已提交（2026-11-18）、X2-a0 注入缝待落地——形码开工前须逐条过 P0 门禁，见 `docs/phase0/PHASE0-PLAN.md`。
本目录对框架只读不改，缺陷与缺口由框架侧修复。

## 为什么不直接改 ros-pinyin

- 那套结构由 ros-pinyin 侧维护，两边同时改同一批文件必然冲突；
- 形码的框架需求 G1–G4 已由框架侧交付（含 G1 ABI 缺陷的修复待办，均记录在 `docs/FRAMEWORK-AUDIT-2026-11-17.md`）；
- 形码整目录可独立评审、独立发布、整目录删除，不牵动拼音产品。

## 合规提醒

- 本目录**不得**出现任何真实码表数据（`.csv`/`.txt`/`.dat` 中的字码对照）。
  `packs/*/table.csv`、`packs/*/pack.ini` 均由构建生成且被 `.gitignore` 排除。
- 测试 fixture 必须自造（假编码），不得截取真实码表片段。
- `packs/` 下的 `LICENSE` 是上游许可**原文**，不要改写其内容。
- 授权声明写在仓库根 `NOTICE.txt` 第 8 节（该节是形码专属内容，需保留）。

## 常用命令

在仓库根目录执行：

```bash
# 构建并校验一个数据包（入参须是已转换好的规范 CSV）
python3 xingma/tools/mkdat-shape/make_pack.py build \
  --pack xingma/packs/shape86 --csv build/shape86.csv \
  --out dist/RosXingMa-Pack-Shape86 --version 1.0.0 --zip

python3 xingma/tools/mkdat-shape/make_pack.py verify \
  --dir dist/RosXingMa-Pack-Shape86
```
