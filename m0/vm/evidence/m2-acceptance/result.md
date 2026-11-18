# M2 真机验收结果（2026-11-18）

- 执行人：用户（真实键盘手打，**全程未注入按键**；日志自报 `本程序注入按键 0 次`）
- 环境：ReactOS 0.4.16 i386 VM，`rospinyin7.ime` + `rospinyin.dat` + `imhost.exe rec`
- 证据：`2026-11-18-imhost.log.gz`（同一轮运行，入口层与窗口层各 **433 次按下**逐条对齐）
- 结论：**15/15 通过** → `M2-ACCEPTANCE.md` §3 已填实测值，`HANDOVER.md` §11.16 有还原方法

## 产物指纹

| 文件 | 字节 | sha256 |
|---|---|---|
| `rospinyin7.ime` | 967,775 | `15e401cfd581914764b6195632222e04f99a734b71995b277e00719dbebb2597` |
| `rospinyin.dat` | 902,776 | `f28b2606338431d2e7a9adc089f8f053728c2be18061bc066d8fdaba23e9b921` |
| `imhost.exe`（探针） | 945,314 | `4c1d4ca0122f4a3d321b0f5f960d781d94735ff686296d0c7c327b82faabe8ed` |

## 按键与上屏还原

日志里没有"用户输入了什么"这一行，但 `KEY nnnn vk=… DOWN` 与 `WM_IME_CHAR` 都在，
按时间序折叠即可还原（`?` 是 `=`/`-` 的 vk 为 `0xBB`、符号名解析失败）：

```bash
gzip -dc 2026-11-18-imhost.log.gz | python3 -c '
import re,sys
seq=[]
for l in sys.stdin:
    m=re.search(r"KEY \d+ vk=0x([0-9A-F]+)\(([^)]*)\) sc=0x([0-9A-F]+) DOWN",l)
    if m: seq.append(m.group(2))
    mi=re.search(r"WM_IME_CHAR ch=0x([0-9A-F]+)\(\\u([0-9A-Fa-f]+)\)",l)
    if mi: seq.append("=>"+chr(int(mi.group(2),16)))
out,buf=[],[]
for t in seq:
    if t.startswith("=>"): out.append("".join(buf)+"{"+t[2:]+"}"); buf=[]
    else: buf.append(t if len(t)==1 else "["+t+"]")
if buf: out.append("".join(buf))
print(" | ".join(out))'
```

摘录（完整串见 `HANDOVER.md` §11.16）：

```
NV [SPACE]{女} | LV 1{律} | LVE [BACK] E 1{略} | NVE [SPACE]{虐}
HAO [BACK][BACK][BACK] HAO [ESC] | RENZHI [BACK] ICHU 1{人} 1{之} 4{初}
SHI????????????????????7{仕}  ← 翻页后跨页选字
…《陋室铭》《山居秋暝》连续上屏 100+ 字…
[SHIFT] H [BACK] [SHIFT] B [BACK]  ← lone-Shift 切英文，拉丁字符直出
```

## 未归档的项

- 3.9–3.11 的"页码文本"没有逐页截图，结论来自跨页序号命中；对外出验收纪要时建议补一张第 2 页的图。
- `01-notepad-zhong.png` 等 5 张图仍未整理进本目录（`M2-ACCEPTANCE.md` §4 的命名约定）。
