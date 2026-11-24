# GUI 安装器：杀毒误报与 Windows 误装防护评估

> 状态：静态评估 + 官方来源核对，S1 方案补充输入。评估日期：2026-11-23。
> 背景：路线 C 的 GUI 安装器要内嵌 `.ime`+`.dat`、释放到 `%TEMP%`、写注册表。
> 这两件事分别命中杀软启发式规则和「在 Windows 上误装只对 ReactOS 有效的 IME」风险。

## 1. 杀毒软件误报

### 1.1 结论

**无法做到零误报，只能降低概率并让用户可自查。** EV 代码签名证书（每年数千元）对本项目不成立；未签名的开源 GUI 安装器在 Windows 上触发 SmartScreen「未知发布者」与第三方杀软启发式告警是**预期行为**，不是安装器的缺陷。

### 1.2 为什么这个形态特别容易被误报

GUI 安装器的三个行为单独看都正常，组合起来正好是「下载器/释放器（dropper）」的经典特征：

1. 一个 exe 内嵌两个二进制 payload（PE 资源段）；
2. 运行时把 payload 解压释放到临时目录；
3. 写注册表并往系统目录拷贝文件。

7-Zip SFX（路线 B）更甚——SFX stub 本身就是大量恶意软件用过的壳，误报率只会更高。路线 C 的「deflate 压缩 + 自写解压」同样会被启发式视为「加壳/混淆」。**这一点上 A/B/C 都躲不掉，C 只是比 B 少一个第三方 stub 的固有坏名声。**

### 1.3 事实依据（官方/一手来源）

- AntiMicroX、codeDB Lens、WSL Dashboard 等开源项目均公开说明：未签名 exe 触发 SmartScreen「unknown publisher」与杀软启发式误报属预期；EV 证书数千元/年，OV 数百元/年仍需累积信誉；免费 SignPath Foundation 是开源项目的可选路径。
- 共同建议：发布 SHA-256 供用户核对、Source+CI 透明可审计、用户手动加白名单，**不建议用户关闭杀软**。

### 1.4 本项目的缓解措施（纳入改造清单）

| 措施 | 做法 | 成本 | 能解决什么 |
|---|---|---|---|
| M1 透明构建 | release 资产由 GitHub Actions 从 tag 构建，`INFO.txt` 记录 source commit + workflow run | 已有 | 证明「发布包 = 该 tag 的源码」，用户可复核 |
| M2 SHA256 清单 | 现有 `SHA256SUMS.txt` 继续覆盖 GUI exe；release notes 写明核验方法（`certutil -hashfile` / `sha256sum -c`） | 已有 | 用户可确认下载未被替换 |
| M3 误报说明专区 | release notes 与 README 增「杀毒误报说明」段：说明未签名、为何被拦、如何核对 hash、如何加白名单；明确「不要关闭杀软」 | 低（文档） | 降低用户恐慌与误判为病毒 |
| M4 减少可疑特征 | 解压目标用固定 `%TEMP%\rospinyin-<pid>` 并即时清理；payload 不加密不二次混淆；GUI 资源用标准 RCDATA 不用自定义壳 | 低 | 减少「加壳/持久化」特征 |
| M5 免费签名通道（可选后续） | 若项目符合 SignPath Foundation 条件，后续申请免费签名；本轮不阻塞 | 中（申请周期） | 长期降低误报，非本轮范围 |

### 1.5 不做的承诺

- 不承诺「任何杀软都不报」——做不到，也不诚实。
- 不引导用户关闭杀毒软件或 SmartScreen。
- 不购买 EV/OV 证书（超出项目预算与范围）。

## 2. 非白名单平台误装防护（Windows / 未登记兼容分支）

### 2.1 结论

**必须在 GUI 安装按钮按下前加运行时严格平台白名单：当前只登记 `ReactOS`；Windows、`libernt` 等尚未显式登记的兼容分支一律拒绝安装并给出明确提示。** 这是本轮新增的 P0 约束——CLI 版依赖用户自己懂「这是 ReactOS 专用」，GUI 面向最终用户，不能指望用户读文档。Windows 真机如确需安装，只使用 CLI `/step1../step4` 分步方式；GUI 不提供 Windows 安装入口。

### 2.2 为什么是真实高危

GUI 的「安装」按钮内部就是 step1..4：把 i686 `.ime`+`.dat` 拷进 `GetSystemDirectoryW()`、调 `ImmInstallIMEW()` 注册布局、写 Layout Text/DisplayName。

- 在 **ReactOS 0.4.16**：已验证路径，红线受控。
- 在 **Windows（任意版本）**：`GetSystemDirectoryW()` 返回 `C:\Windows\System32`，`ImmInstallIMEW` 会真实注册一个外部 IME。这个 `.ime` **从未在 Windows 上验证过**（项目文档明确「只在 ReactOS 0.4.16 验证过，未测 Windows」），可能造成：宿主注册表被写入一条指向无效 IME 的布局、系统输入法列表污染、甚至加载失败导致应用异常。这是用户机器上真实的副作用，不能靠「提示一句」了事。

### 2.3 判据（官方一手来源）

ReactOS 开发者 hbelusca 在官方论坛给出、且 ReactOS 自带 `cmd ver.c` 实际使用的判据：

```
HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion
  ProductName (REG_SZ) == "ReactOS"
```

来源：
- https://reactos.org/forum/viewtopic.php?t=16863 （hbelusca 2018 回帖，明确该值）
- https://github.com/Andrej123456789/reactos/blob/master/base/shell/cmd/ver.c （ReactOS 自身读该键构造版本串）

补充：操作系统开发者明确表示**不会承诺**一个「保证永远存在」的探测标志（如 ntdll 里的 is_reactos），所以判据要设计成**白名单式**——只有确认是已登记并验证过的平台才放行，读不到/不确定一律拒绝，而不是反过来「不是 Windows 就放行」。用户本轮明确要求：后续兼容分支（例如 `libernt`）在登记前同样拒绝；登记动作必须伴随实机验证证据，不允许仅凭产品名字符串先加白名单。

### 2.4 闸门设计（纳入改造清单）

| 项 | 设计 |
|---|---|
| 位置 | GUI「安装」按钮回调（及任何一键安装入口）的最前面；卸载按钮**不受限**（误装到非白名单平台时要能卸载清理） |
| 判据 | `RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", ...)` 返回 REG_SZ，且值经 `is_supported_product_name()` 命中**已登记白名单**；当前白名单仅 `L"ReactOS"`（大小写不敏感）。`libernt` 等兼容分支必须先实机验证并显式登记，未登记即拒绝 |
| 失败分支 | 读不到键 / 值不是字符串 / 未命中白名单（含 Windows、`libernt`、空串）→ 一律拒绝安装，弹对话框：「当前系统未列入本安装器已验证平台白名单（ProductName=...）。为保护您的系统，已取消安装。Windows 真机请使用命令行 /step1../step4 方式。」 |
| 可测性 | 判据抽成宿主可测纯函数 `is_supported_product_name(const std::wstring&)`（输入注册表读到的值，输出 bool），注册表读取留在 Win32 适配层；宿主单测覆盖 `"ReactOS"` / `"reactos"` / `"Windows..."` / `"libernt"` / 空串 / 类型不符 / 未登记项；白名单表驱动，便于验证未来登记新分支后生效 |
| 红线关系 | 只读注册表，不写；不触碰 `check_m1.py` 的任何禁写路径 |
| CLI 例外 | GUI 的闸门**不改变 CLI `/step1../step4` 行为**；Windows 真机安装只走这条命令行分步路径，供开发者/诊断使用，GUI 仍拒绝安装 |

### 2.5 与 S0/S1 的衔接

- 修订验收标准 **AC8**：GUI 安装按钮以严格白名单判定，当前仅 `ReactOS` 放行；Windows、`libernt` 等未登记平台一律拒绝并弹提示；宿主纯函数测试覆盖放行/拒绝全分支。
- 修订验收标准 **AC9**：release notes 与 README 含「杀毒误报说明」段（hash 核验步骤 + 勿关闭杀软），并写明 Windows 真机安装只使用 CLI `/step1../step4`，GUI 不提供 Windows 安装入口。
- 改造清单 C12（白名单纯函数 + GUI 适配）、C13（误报说明与 Windows CLI 路径文档）按上述要求更新。

## 3. 对路线选择的影响

无。两点都不改变 A/B/C 的对比结论；C 依然是推荐路线。它们补充的是「同一个 GUI 安装器还必须具备的安全与文档属性」，属于 S1 的增量约束，不是重新选型。
