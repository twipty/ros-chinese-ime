#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""RosXingMa 数据包打包与校验工具（stdlib only）。

用法:
  make_pack.py build  --pack xingma/packs/shape86 --csv build/shape86.csv \
                      --out dist/RosXingMa-Pack-Shape86 --version 1.0.0 [--zip]
  make_pack.py verify --dir dist/RosXingMa-Pack-Shape86 [--quiet]

设计要点:
  * 只填 4 个构建期占位符: PACK_VERSION / BUILD_DATE / DATA_SHA256 / ENTRY_COUNT。
    MaxCodeLen / Alphabet / DataFile 是声明值，脚本只校验不改写——声明与实际
    不符时直接失败，绝不"自动纠正"成看起来合法的包。
  * 输出确定性: SHA256SUMS 按文件名排序，zip 内条目时间戳固定为 1980-01-01，
    同样输入两次构建可字节比对。
"""

import argparse
import configparser
import csv
import hashlib
import io
import re
import sys
import zipfile
from pathlib import Path

REQUIRED = {
    "Pack": ["FormatVersion", "PackId", "PackVersion", "DisplayName"],
    "License": ["License", "LicenseFile", "SourceUrl", "Redistributable", "Warranty"],
    "Table": ["SourceFormat", "DataFile", "DataSha256", "MaxCodeLen", "Alphabet"],
    "Install": ["ConsentRequired"],
}
ALLOWED_LICENSES = {"MIT", "Unlicense"}
ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,31}$")


def die(msg, code=2):
    print(f"错误: {msg}", file=sys.stderr)
    sys.exit(code)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_ini(path: Path) -> configparser.ConfigParser:
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.optionxform = str
    try:
        cp.read(path, encoding="utf-8-sig")
    except configparser.Error as exc:
        die(f"清单解析失败 {path}: {exc}")
    return cp


def csv_stats(path: Path):
    """返回 (entries, code_count, max_len, alphabet, duplicates, rejected)。"""
    text = path.read_text(encoding="utf-8-sig")
    lines = [ln for ln in text.splitlines() if ln.strip() and not ln.lstrip().startswith("#")]
    if not lines:
        die("CSV 为空")

    sample = "\n".join(lines[:20])
    delimiter = ","
    try:
        delimiter = csv.Sniffer().sniff(sample, delimiters=",\t;").delimiter
    except csv.Error:
        pass

    reader = csv.reader(io.StringIO("\n".join(lines)), delimiter=delimiter)
    rows = list(reader)
    if rows and rows[0] and rows[0][0].strip().lower() in ("code", "编码"):
        rows = rows[1:]

    seen = set()
    rejected = 0
    alphabet = set()
    max_len = 0
    codes = set()
    accepted = 0

    for row in rows:
        if len(row) < 2:
            rejected += 1
            continue
        code = row[0].strip().lower()
        text = row[1].strip()
        if not code or not text or not re.fullmatch(r"[a-z]+", code):
            rejected += 1
            continue
        accepted += 1
        seen.add((code, text))
        codes.add(code)
        max_len = max(max_len, len(code))
        alphabet.update(code)

    entries = len(seen)
    duplicates = accepted - entries
    return entries, len(codes), max_len, "".join(sorted(alphabet)), duplicates, rejected


def cmd_build(args):
    pack_dir = Path(args.pack)
    tpl = pack_dir / "pack.ini.in"
    if not tpl.is_file():
        die(f"找不到模板 {tpl}")

    csv_path = Path(args.csv)
    if not csv_path.is_file():
        die(f"找不到数据文件 {csv_path}")

    entries, code_count, max_len, alphabet, dups, rejected = csv_stats(csv_path)
    if entries == 0:
        die("CSV 解析后无有效条目")

    cp_probe = read_ini(tpl)
    decl_len = int(cp_probe.get("Table", "MaxCodeLen"))
    decl_alpha = cp_probe.get("Table", "Alphabet").strip()
    if max_len > decl_len:
        die(f"数据最大码长 {max_len} 超过声明 MaxCodeLen={decl_len}")
    extra = sorted(set(alphabet) - set(decl_alpha))
    if extra:
        die(f"数据含声明字母表外的字符: {''.join(extra)}")

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    data_path = out_dir / cp_probe.get("Table", "DataFile").strip()
    data_path.write_bytes(csv_path.read_bytes())

    data_sha = sha256_file(data_path)
    raw = tpl.read_text(encoding="utf-8")
    fills = {
        "{{PACK_VERSION}}": args.version,
        "{{BUILD_DATE}}": args.date,
        "{{DATA_SHA256}}": data_sha,
        "{{ENTRY_COUNT}}": str(entries),
    }
    for key, val in fills.items():
        raw = raw.replace(key, val)
    leftover = re.findall(r"\{\{[A-Z_]+\}\}", raw)
    if leftover:
        die(f"模板仍有未填充占位符: {sorted(set(leftover))}")
    (out_dir / "pack.ini").write_text(raw, encoding="utf-8")

    for name in ("LICENSE", "NOTICE.md"):
        src = pack_dir / name
        if not src.is_file():
            die(f"数据包缺少必需文件 {src}")
        (out_dir / name).write_bytes(src.read_bytes())

    targets = sorted([c for c in out_dir.iterdir() if c.name != "SHA256SUMS" and c.is_file()])
    sums = "\n".join(f"{sha256_file(p)}  {p.name}" for p in targets) + "\n"
    (out_dir / "SHA256SUMS").write_text(sums, encoding="utf-8")

    print(f"构建完成: {out_dir}")
    print(f"  条目 {entries}  编码 {code_count}  最大码长 {max_len}  字母表 {len(alphabet)} 字符")
    print(f"  去重 {dups}  跳过 {rejected}  数据 sha256 {data_sha[:16]}…")

    if args.zip:
        zpath = out_dir.with_suffix(".zip")
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as zf:
            for p in sorted(out_dir.rglob("*")):
                if p.is_dir():
                    continue
                info = zipfile.ZipInfo(str(Path(out_dir.name) / p.relative_to(out_dir)))
                info.date_time = (1980, 1, 1, 0, 0, 0)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                zf.writestr(info, p.read_bytes())
        zsha = sha256_file(zpath)
        Path(str(zpath) + ".sha256").write_text(f"{zsha}  {zpath.name}\n", encoding="utf-8")
        print(f"  打包 {zpath.name}  sha256 {zsha[:16]}…")
    return 0


def cmd_verify(args):
    d = Path(args.dir)
    if not d.is_dir():
        die(f"不是目录: {d}")

    ini = d / "pack.ini"
    if not ini.is_file():
        die(f"缺少 pack.ini: {d}")
    cp = read_ini(ini)

    problems = []
    for sec, keys in REQUIRED.items():
        if not cp.has_section(sec):
            problems.append(f"缺少段 [{sec}]")
            continue
        for k in keys:
            if not cp.has_option(sec, k) or not cp.get(sec, k).strip():
                problems.append(f"缺少 {sec}.{k}")

    if problems:
        for p in problems:
            print(f"  ✗ {p}")
        return 3

    if cp.get("Pack", "FormatVersion").strip() != "1":
        problems.append(f"未知 FormatVersion={cp.get('Pack','FormatVersion')}")
    if not ID_RE.match(cp.get("Pack", "PackId")):
        problems.append(f"PackId 非法: {cp.get('Pack','PackId')}")
    if cp.get("License", "License") not in ALLOWED_LICENSES:
        problems.append(f"不允许的许可证: {cp.get('License','License')}")
    if cp.get("License", "Redistributable").lower() != "yes":
        problems.append("Redistributable 必须为 yes")
    if cp.get("Table", "DataFile") != "table.csv":
        problems.append("DataFile 必须为 table.csv")

    for key in ("DataFile", "LicenseFile"):
        rel = cp.get("Table" if key == "DataFile" else "License", key)
        target = (d / rel).resolve()
        try:
            target.relative_to(d.resolve())
        except ValueError:
            problems.append(f"{key} 越出包目录: {rel}")
            continue
        if not target.is_file():
            problems.append(f"{key} 不存在: {rel}")

    sums = {}
    sums_file = d / "SHA256SUMS"
    if not sums_file.is_file():
        problems.append("缺少 SHA256SUMS")
    else:
        for line in sums_file.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            parts = line.split("  ", 1)
            if len(parts) != 2:
                problems.append(f"SHA256SUMS 行格式错误: {line}")
                continue
            sums[parts[1].strip()] = parts[0].strip()

    data_rel = cp.get("Table", "DataFile")
    if data_rel in sums:
        actual = sha256_file(d / data_rel)
        if actual != sums[data_rel]:
            problems.append(f"{data_rel} 与 SHA256SUMS 不符")
        if actual != cp.get("Table", "DataSha256").strip():
            problems.append(f"{data_rel} 与 DataSha256 不符")
    else:
        problems.append(f"SHA256SUMS 未覆盖 {data_rel}")

    entries, code_count, max_len, alphabet, _, _ = csv_stats(d / data_rel)
    decl_len = int(cp.get("Table", "MaxCodeLen"))
    if max_len > decl_len:
        problems.append(f"实际码长 {max_len} > 声明 {decl_len}")
    extra = sorted(set(alphabet) - set(cp.get("Table", "Alphabet")))
    if extra:
        problems.append(f"编码含声明外字符: {''.join(extra)}")

    expect = cp.get("Table", "EntryCountExpect", fallback="").strip()
    if expect.isdigit() and entries:
        drift = abs(entries - int(expect)) / max(entries, 1)
        if drift > 0.01:
            problems.append(f"条目数偏差 {drift:.1%}: 实际 {entries} / 声明 {expect}")

    if problems:
        for p in problems:
            print(f"  ✗ {p}")
        return 3

    if not args.quiet:
        print(f"校验通过: {cp.get('Pack','DisplayName')} v{cp.get('Pack','PackVersion')}")
        print(f"  条目 {entries}  编码 {code_count}  最大码长 {max_len}  许可 {cp.get('License','License')}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="RosXingMa 数据包打包/校验")
    sub = ap.add_subparsers(dest="cmd", required=True)

    b = sub.add_parser("build", help="由 pack.ini.in + CSV 生成数据包目录")
    b.add_argument("--pack", required=True, help="xingma/packs/<id> 目录（含 pack.ini.in）")
    b.add_argument("--csv", required=True, help="规范 CSV 数据文件")
    b.add_argument("--out", required=True, help="输出目录")
    b.add_argument("--version", required=True, help="包版本，如 1.0.0")
    b.add_argument("--date", default=None, help="构建日期 YYYY-MM-DD，缺省取当天")
    b.add_argument("--zip", action="store_true", help="同时生成确定性 zip 与 .sha256")
    b.set_defaults(func=cmd_build)

    v = sub.add_parser("verify", help="校验已生成的数据包")
    v.add_argument("--dir", required=True)
    v.add_argument("--quiet", action="store_true")
    v.set_defaults(func=cmd_verify)

    args = ap.parse_args(argv)
    if args.cmd == "build" and not args.date:
        from datetime import date
        args.date = date.today().isoformat()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
