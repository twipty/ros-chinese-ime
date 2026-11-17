#!/usr/bin/env python3
"""把两份 imhost 事件链日志做成"参照 vs 被测"的差异报告。

为什么要有这个工具
------------------
排查"IME 不被调用"时，最容易迷失的是**没有正常样本**：只知道我们这边少了什么，
不知道"正常应该发生什么"。所以先在已知正常的输入法（ReactOS 自带日文
mzimeja）上把同一套按键的事件链录下来当参照，再切到自己的 .ime 录一遍，
两份序列相减 —— 缺的那几步就是待修的缺口。

日志由 `imhost.exe ref` 产生（顶层窗口 + 真实 EDIT 控件，子类化记录），
同一次运行里切布局即可产生多段"场景"，用 `======== 场景切换 ... ime=xxx ========`
切分。

用法
----
    python3 tools/ime_chain_diff.py ref.log test.log
    python3 tools/ime_chain_diff.py ref.log test.log --ref-ime mzimeja \
                                                     --test-ime rospinyin
    python3 tools/ime_chain_diff.py --self-test        # 不需要 guest

两个文件各自可能含多段场景；工具按 `--ref-ime` / `--test-ime` 选段，
未指定时用"文件里第一段 / 最后一段"。同一文件也可以两边各用不同场景
（`--ref-file` 与 `--test-file` 都指向它也行）。
"""

from __future__ import annotations

import argparse
import difflib
import re
import sys
from collections import Counter, OrderedDict
from dataclasses import dataclass, field

# [19:23:01.123 +123456ms] 前缀
TS = re.compile(r"^\[\d\d:\d\d:\d\d\.\d{3} \+\d+ms\]\s*")
SCENE = re.compile(r"场景切换 .* ime=(\S+)")
HKL = re.compile(r"场景切换 (\S+) -> (\S+) ime=(\S+)")


@dataclass
class Scene:
    ime: str
    hkl_from: str
    hkl_to: str
    lines: list[str] = field(default_factory=list)


def load_scenes(path: str) -> list[Scene]:
    """按 `场景切换` 行切段。第一条之前的行归入 ime=""（启动噪声）。"""
    scenes: list[Scene] = [Scene(ime="", hkl_from="-", hkl_to="-")]
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = TS.sub("", raw.rstrip("\n"))
            m = HKL.search(line)
            if m is not None:
                scenes.append(Scene(ime=m.group(3), hkl_from=m.group(1),
                                    hkl_to=m.group(2)))
                continue
            scenes[-1].lines.append(line)
    return [s for s in scenes if s.lines]


def pick(scenes: list[Scene], want: str | None, which: str) -> Scene:
    if want:
        for s in scenes:
            if want.lower() in s.ime.lower():
                return s
        raise SystemExit(f"找不到 ime 含 {want!r} 的场景；可选："
                         + ", ".join(s.ime for s in scenes))
    return scenes[0] if which == "first" else scenes[-1]


# ---------------------------------------------------------------- 归一化
# 目标：同一件事在参照与被测里产生**完全相同的字符串**，地址/pid/时间全部去掉。

def norm(line: str) -> str | None:
    if "[edit]" not in line and "[frame]" not in line:
        # 顶层窗口的行只保留与布局相关的，其余（tick/上传/探针）都是噪声
        if "WM_INPUTLANGCHANGE" in line or "WM_INPUTLANGCHANGEREQUEST" in line:
            return line.split("wParam")[0].replace("  ", " ").strip()
        return None

    body = line.split("]", 1)[1].strip() if "] " in line else line

    # 键盘：只留 vk 与"是否被 IME 吞掉"
    m = re.match(r"WM_(SYS)?KEY(DOWN|UP) vk=0x([0-9A-F]{2})", body)
    if m:
        vk = m.group(3)
        special = " <== VK_PROCESSKEY" if "VK_PROCESSKEY" in body else ""
        return f"key-{'down' if m.group(2) == 'DOWN' else 'up'} vk=0x{vk}{special}"

    m = re.match(r"WM_(SYS)?CHAR ch=0x([0-9A-F]+)", body)
    if m:
        return f"char 0x{m.group(2)}"

    m = re.match(r"WM_IME_CHAR ch=0x([0-9A-F]+)", body)
    if m:
        return f"ime-char 0x{m.group(1)}"

    m = re.match(r"WM_IME_SETCONTEXT active=(\d) flags=(0x[0-9A-F]+)", body)
    if m:
        return f"ime-setcontext active={m.group(1)} flags={m.group(2)}"

    m = re.match(r"WM_IME_NOTIFY (\S+?)\(", body)
    if m:
        return f"ime-notify {m.group(1)}"

    if body.startswith("WM_IME_STARTCOMPOSITION"):
        return "ime-startcomposition"
    if body.startswith("WM_IME_ENDCOMPOSITION"):
        return "ime-endcomposition"
    if body.startswith("WM_IME_REQUEST"):
        m = re.search(r"req=(0x[0-9A-F]+)", body)
        return f"ime-request {m.group(1) if m else '?'}"

    if body.startswith("WM_IME_COMPOSITION"):
        m = re.search(r"flags=(0x[0-9A-F]+)", body)
        return f"ime-composition flags={m.group(1) if m else '?'}"

    # 组合串内容：这是最有信息量的一类，必须逐字保留
    m = re.match(r"composition=\[(.*)\]", body)
    if m:
        return f"  composition=[{m.group(1)}]"
    m = re.match(r"result=\[(.*)\]", body)
    if m:
        return f"  result=[{m.group(1)}]"
    m = re.match(r"cursorpos=(-?\d+)", body)
    if m:
        return f"  cursorpos={m.group(1)}"

    if "ImmGetContext -> NULL" in body:
        return "  ImmGetContext(edit) -> NULL"

    if body.startswith("WM_SETFOCUS"):
        return "focus"
    if body.startswith("WM_KILLFOCUS"):
        return "killfocus"
    return None


def events(scene: Scene) -> list[str]:
    out = []
    for line in scene.lines:
        e = norm(line)
        if e is not None:
            out.append(e)
    return out


def diff(ref: list[str], test: list[str]) -> str:
    lines = difflib.unified_diff(ref, test, fromfile="参照(正常输入法)",
                                 tofile="被测(我们的 .ime)", lineterm="", n=3)
    return "\n".join(lines)


def summary(ev: list[str]) -> str:
    c = Counter(e.strip().split(" ")[0] for e in ev if e.strip())
    return "  ".join(f"{k}×{v}" for k, v in sorted(c.items()))


# ---------------------------------------------------------------- self-test

SAMPLE_REF = """[10:00:00.000 +1000ms] ======== 场景切换 (nil) -> 0xE0200409 ime=mzimeja.ime ========
[10:00:00.001 +1001ms] [edit] WM_SETFOCUS
[10:00:00.002 +1002ms] [edit] WM_IME_SETCONTEXT active=1 flags=0xC000000F[COMPOSITION|GUIDELINE|ALLCANDIDATE]
[10:00:00.003 +1003ms] [edit] WM_IME_NOTIFY IMN_SETCONVERSIONMODE(0x6) lParam=0x0
[10:00:01.000 +2000ms] [edit] WM_KEYDOWN vk=0x4E(N) rep=1  <== VK_PROCESSKEY
[10:00:01.010 +2010ms] [edit] WM_IME_STARTCOMPOSITION
[10:00:01.020 +2020ms] [edit] WM_IME_COMPOSITION flags=0x0000041C[COMPSTR|COMPATTR|CURSORPOS|DELTASTART]
[10:00:01.021 +2021ms] [edit]   composition=[\\uFF4E] (2 字节)
[10:00:01.030 +2030ms] [edit] WM_IME_NOTIFY IMN_OPENCANDIDATE(0x5) lParam=0x1
[10:00:02.000 +3000ms] [edit] WM_KEYDOWN vk=0x20(SPACE) rep=1  <== VK_PROCESSKEY
[10:00:02.010 +3010ms] [edit] WM_IME_COMPOSITION flags=0x00000800[RESULTSTR|]
[10:00:02.011 +3011ms] [edit]   result=[\\u306B] (2 字节)
[10:00:02.020 +3020ms] [edit] WM_IME_ENDCOMPOSITION
"""

SAMPLE_TEST = """[10:05:00.000 +4000ms] ======== 场景切换 0xE0200409 -> 0xE02A0804 ime=rospinyin5.ime ========
[10:05:00.001 +4001ms] [edit] WM_SETFOCUS
[10:05:00.002 +4002ms] [edit] WM_IME_SETCONTEXT active=1 flags=0xC000000F[COMPOSITION|GUIDELINE|ALLCANDIDATE]
[10:05:01.000 +5000ms] [edit] WM_KEYDOWN vk=0x4E(N) rep=1
[10:05:01.002 +5002ms] [edit] WM_CHAR ch=0x6E
"""


def self_test() -> int:
    import tempfile
    import os

    d = tempfile.mkdtemp()
    p1 = os.path.join(d, "ref.log")
    p2 = os.path.join(d, "test.log")
    open(p1, "w", encoding="utf-8").write(SAMPLE_REF)
    open(p2, "w", encoding="utf-8").write(SAMPLE_TEST)

    r = events(pick(load_scenes(p1), "mzimeja", "first"))
    t = events(pick(load_scenes(p2), "rospinyin", "first"))
    assert "key-down vk=0x4E <== VK_PROCESSKEY" in r, r
    assert "ime-startcomposition" in r
    assert "  composition=[\\uFF4E]" in r
    assert "key-down vk=0x4E" in t and "key-down vk=0x4E <== VK_PROCESSKEY" not in t
    d2 = diff(r, t)
    assert "+key-down vk=0x4E" in d2 and "-ime-startcomposition" in d2, d2
    print("self-test OK")
    print(f"参照事件 {len(r)} 条：{summary(r)}")
    print(f"被测事件 {len(t)} 条：{summary(t)}")
    print()
    print(d2)
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ref", nargs="?", help="参照日志（日文输入法那段场景在里面）")
    ap.add_argument("test", nargs="?", help="被测日志（我们的 .ime 那段场景）")
    ap.add_argument("--ref-ime", default=None, help="参照场景的 ime 关键字，如 mzimeja")
    ap.add_argument("--test-ime", default=None, help="被测场景的 ime 关键字，如 rospinyin")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()
    if not args.ref or not args.test:
        ap.print_help()
        return 2

    rs = load_scenes(args.ref)
    ts = load_scenes(args.test)
    r = pick(rs, args.ref_ime, "first")
    t = pick(ts, args.test_ime, "last")

    print(f"参照：{args.ref}  场景 ime={r.ime or '(启动)'}")
    print(f"被测：{args.test} 场景 ime={t.ime or '(启动)'}")
    print()
    rev, tev = events(r), events(t)
    print(f"参照事件 {len(rev)} 条：{summary(rev)}")
    print(f"被测事件 {len(tev)} 条：{summary(tev)}")
    print()
    d = diff(rev, tev)
    if not d.strip():
        print("两条事件链完全一致（去掉地址/时间后）。")
    else:
        print(d)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
