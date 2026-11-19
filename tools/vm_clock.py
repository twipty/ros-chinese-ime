#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Align the ReactOS guest clock (and keep file timestamps meaningful).

Measured 2026-11-19: host 14:36 (+08:00), guest taskbar 10:30 PM *previous
day*.  14:36+08 == 06:36 UTC, and 22:30 == 06:36 - 8h, so the guest is not
drifting -- its time zone is UTC-08:00 while the RTC holds UTC (QEMU's
default `-rtc base=utc`).  A drifting clock is the expensive reading of the
same symptom: it makes `dir` dates lie and turns "is this the build I just
made?" into guesswork.

Two fixes, both user-executed (nothing here types into the guest):

  * persistent -- set the guest time zone to UTC+08:00.  Run
      python3 tools/vm_clock.py --reg
    then double-click the generated .reg inside the guest and reboot.
    The RTC stays UTC, which is what Windows-family systems expect.

  * temporary -- set the local time by hand in the guest.  Run
      python3 tools/vm_clock.py --guest
    for the exact `date` / `time` lines.  Lost on the next reboot, because
    the RTC goes back to UTC and the wrong zone re-applies itself.

Do NOT "fix" this with `-rtc base=localtime`: that writes local time into
CMOS while the guest still subtracts its own zone, so the error survives
in a less obvious form.
"""

from __future__ import annotations

import argparse

import sys
from datetime import datetime, timezone, timedelta
from pathlib import Path

DEFAULT_SHARE = Path.home() / ".workbuddy" / "tmp" / "vmshare"
# Windows keeps UTC = local + Bias, so UTC+08:00 is Bias = -480.
CN_BIAS_MINUTES = -8 * 60


def bias_dword(minutes: int) -> str:
    return f"{(minutes & 0xFFFFFFFF):08x}"


def reg_body(bias_min: int) -> str:
    return "\r\n".join([
        "Windows Registry Editor Version 5.00",
        "",
        r"[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\TimeZoneInformation]",
        f'"Bias"=dword:{bias_dword(bias_min)}',
        f'"ActiveTimeBias"=dword:{bias_dword(bias_min)}',
        '"StandardName"="China Standard Time"',
        '"StandardBias"=dword:00000000',
        '"DaylightName"="China Standard Time"',
        '"DaylightBias"=dword:00000000',
        "",
    ])


def write_reg(path: Path) -> None:
    # regedit wants UTF-16LE with BOM; UTF-8 makes ReactOS silently skip it.
    path.write_bytes(b"\xff\xfe" + reg_body(CN_BIAS_MINUTES).encode("utf-16-le"))


def main() -> int:
    ap = argparse.ArgumentParser(description="Align the ReactOS guest clock.")
    ap.add_argument("--guest", action="store_true", help="print the date/time lines to type in the guest")
    ap.add_argument("--reg", action="store_true", help="write timezone-cn.reg into the shared folder")
    ap.add_argument("--share-dir", default=str(DEFAULT_SHARE))
    ap.add_argument("--assume-bias", type=int, default=-8 * 60,
                    help="guest zone offset in minutes seen on screen (default: -480, i.e. UTC-8)")
    args = ap.parse_args()

    now = datetime.now().astimezone()
    utc = now.astimezone(timezone.utc)
    offset = now.utcoffset() or timedelta(0)

    if args.guest:
        # `date` and `time` take the value inline; a bare `time` prompts twice.
        print(f"date {now.strftime('%m-%d-%Y')}")
        print(f"time {now.strftime('%H:%M:%S')}")
        print("")
        print("# type these two lines in the guest cmd; if `date` rejects the")
        print("# 4-digit year, retry as " + now.strftime('%m-%d-%y'))
        return 0

    if args.reg:
        share = Path(args.share_dir).expanduser()
        if not share.is_dir():
            print(f"[error] shared folder not found: {share}", file=sys.stderr)
            return 2
        out = share / "timezone-cn.reg"
        write_reg(out)
        print(f"已写 {out}  ({out.stat().st_size} B, UTF-16LE)")
        print("guest 内：wget http://10.0.2.2:8000/timezone-cn.reg → 双击导入 → 重启")
        print("重启后任务栏时间应为上方报告的「宿主本地」值。")
        return 0

    host_off = f"{int(offset.total_seconds() // 3600):+03d}:{int(abs(offset.total_seconds()) // 60) % 60:02d}"
    guest_seen = utc + timedelta(minutes=args.assume_bias)
    drift = now.replace(tzinfo=None) - guest_seen.replace(tzinfo=None)

    print(f"宿主本地   {now.strftime('%Y-%m-%d %H:%M:%S')}  ({host_off})")
    print(f"宿主 UTC   {utc.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"CMOS/RTC   {utc.strftime('%Y-%m-%d %H:%M:%S')}   (QEMU 默认 -rtc base=utc，CMOS 存 UTC)")
    print(f"guest 应显示 = 宿主本地 = {now.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"按 --assume-bias {args.assume_bias:+d} 分钟推算，guest 现在显示 "
          f"{guest_seen.strftime('%Y-%m-%d %H:%M:%S')}，差 {drift}")
    print("")
    print("判定：差 ≈ 整数小时 → 时区设错（持久修：--reg）；差得零碎、还在变大 → 真漂移（重启 VM）。")
    print("      两者都会让 `dir` 的文件日期不可信，所以判版本永远看字节数（见 tools/artifact_hashes.py）。")
    print("")
    print("下一步：")
    print("  python3 tools/vm_clock.py --reg     # 生成时区 .reg，guest 导入后重启（推荐）")
    print("  python3 tools/vm_clock.py --guest   # 或临时手敲 date/time，重启后失效")
    return 0


if __name__ == "__main__":
    sys.exit(main())
