#!/usr/bin/env python3
# RosPinyin - Simplified Chinese pinyin IME for ReactOS
# SPDX-License-Identifier: MIT
# See LICENSE and NOTICE.txt at the repository root.
"""Pack the four release payloads into a text-only C/RC header.

Route C embeds zlib/deflate-compressed rospinyin.ime, rospinyin.dat, the CLI
installer itself and (since M4) the word list rospinyin-lexicon.dat in the GUI
installer. This script is the build-time half: it reads the exact bytes that
ship in m0/share/, compresses them deterministically (level 9, no timestamps),
and emits `payload_base64.hpp` as source text. No binary is ever added to the
repository.

The generated header serves two consumers:
  * `gui_res.rc` includes it with ROSPINYIN_RC_PAYLOAD_ONLY defined and uses
    the escaped-string macros as four RCDATA blobs;
  * `gui.cpp` includes it normally for sizes, CRCs and source SHA-256 notes.

Why escaped strings for RCDATA: windres stores a bare `0xAB` byte-list item as
a 16-bit word, doubling every payload and making SizeofResource() disagree with
the expected compressed size. A C string literal with `\\xHH` escapes stores
exactly one byte per escape, including embedded NULs (verified with windres).
"""

from __future__ import annotations

import hashlib
import zlib
from pathlib import Path


def byte_macro(lines: list[str], macro: str, data: bytes) -> None:
    """C++ byte-list macro for the host-side payload tests."""
    lines.append(f"#define {macro} \\")
    for start in range(0, len(data), 16):
        chunk = data[start:start + 16]
        text = ", ".join(f"0x{b:02X}" for b in chunk) + ","
        suffix = " \\" if start + 16 < len(data) else ""
        lines.append(f"    {text}{suffix}")
    lines.append("")


def rc_byte_macro(lines: list[str], macro: str, data: bytes) -> None:
    """RCDATA string macro: one byte per \\xHH escape, NUL-safe."""
    lines.append(f"#define {macro} \\")
    for start in range(0, len(data), 16):
        chunk = data[start:start + 16]
        text = "".join(f"\\x{b:02X}" for b in chunk)
        suffix = " \\" if start + 16 < len(data) else ""
        lines.append(f'    "{text}"{suffix}')
    lines.append("")


# M4 windres workaround: a single RCDATA holding 116,814 adjacent string
# literals makes windres's O(n^2) concatenation hang (>11 min, killed).
# Splitting the word list into SEGMENTS smaller RCDATA blobs (each ~14,600
# literals) compiles in ~14 s. The GUI loads each segment and concatenates in
# resource-ID order before inflating. Ids are 104..104+SEGMENTS-1.
LEX_RCDATA_SEGMENTS = 8


def pack(ime: Path, dat: Path, cli: Path, output: Path,
         lex: Path | None = None) -> None:
    specs = [
        ("IME", "Ime", 101, ime),
        ("DAT", "Dat", 102, dat),
        ("CLI", "Cli", 103, cli),
    ]
    if lex is not None:
        # M4: the word list is the fourth payload. Resource id 104 keeps the
        # existing IME hash frozen (the first three IDs never change).
        specs.append(("LEX", "Lex", 104, lex))
    payloads = []
    for label, cpp_name, resource_id, path in specs:
        raw = path.read_bytes()
        compressed = zlib.compress(raw, 9)
        if zlib.decompress(compressed) != raw:
            raise RuntimeError(f"{label}: compression round-trip failed")
        payloads.append(
            {
                "label": label,
                "cpp": cpp_name,
                "id": resource_id,
                "raw": raw,
                "compressed": compressed,
                "crc": zlib.crc32(raw) & 0xFFFFFFFF,
                "sha": hashlib.sha256(raw).hexdigest(),
                "compressed_sha": hashlib.sha256(compressed).hexdigest(),
            }
        )

    lines = [
        "// RosPinyin - Simplified Chinese pinyin IME for ReactOS",
        "// SPDX-License-Identifier: MIT",
        "// See LICENSE and NOTICE.txt at the repository root.",
        "//",
        "// GENERATED FILE - do not edit by hand.",
        "// Regenerate with: tools/install-rospinyin/pack_payload.py",
        "// Text-encoded compressed payload bytes; no binary is stored in git.",
        "",
        "#ifndef ROSPINYIN_INSTALLER_PAYLOAD_DATA_HPP",
        "#define ROSPINYIN_INSTALLER_PAYLOAD_DATA_HPP",
        "",
    ]
    for p in payloads:
        lines.append(f"#define ROSPINYIN_IDR_{p['label']}_PAYLOAD {p['id']}")
    # M4: the word list occupies SEGMENTS consecutive resource ids.
    lines.append(f"#define ROSPINYIN_LEX_RCDATA_SEGMENTS {LEX_RCDATA_SEGMENTS}")
    lines.append("")

    for p in payloads:
        byte_macro(lines, f"ROSPINYIN_{p['label']}_PAYLOAD_BYTES",
                   p["compressed"])

    lines.append("#ifdef ROSPINYIN_RC_PAYLOAD_ONLY")
    lines.append("")
    for p in payloads:
        if p["label"] == "LEX":
            # Split into SEGMENTS RCDATA blobs so windres does not concatenate
            # 116,814 string literals into one giant token (O(n^2), hangs).
            data = p["compressed"]
            span = (len(data) + LEX_RCDATA_SEGMENTS - 1) // LEX_RCDATA_SEGMENTS
            for seg in range(LEX_RCDATA_SEGMENTS):
                piece = data[seg * span:(seg + 1) * span]
                if piece:
                    rc_byte_macro(
                        lines,
                        f"ROSPINYIN_LEX_PAYLOAD_RC_BYTES_{seg}",
                        piece)
        else:
            rc_byte_macro(lines, f"ROSPINYIN_{p['label']}_PAYLOAD_RC_BYTES",
                          p["compressed"])
    lines.append("#endif  // ROSPINYIN_RC_PAYLOAD_ONLY")
    lines.append("")

    lines.extend(
        [
            "#ifndef ROSPINYIN_RC_PAYLOAD_ONLY",
            "#include <cstddef>",
            "#include <cstdint>",
            "",
            "namespace rospinyin {",
            "namespace installer {",
            "",
        ]
    )
    for p in payloads:
        name = p["cpp"]
        lines.extend(
            [
                f"// source SHA-256: {p['sha']}",
                f"inline constexpr std::size_t k{name}CompressedSize = "
                f"{len(p['compressed'])};",
                f"inline constexpr std::size_t k{name}UncompressedSize = "
                f"{len(p['raw'])};",
                f"inline constexpr std::uint32_t k{name}Crc32 = "
                f"0x{p['crc']:08X}u;",
                f"inline constexpr const char* k{name}CompressedSha256 = "
                f"\"{p['compressed_sha']}\";",
                "",
            ]
        )
    lines.extend(
        [
            "}  // namespace installer",
            "}  // namespace rospinyin",
            "",
            "#endif  // !ROSPINYIN_RC_PAYLOAD_ONLY",
            "",
            "#endif  // ROSPINYIN_INSTALLER_PAYLOAD_DATA_HPP",
            "",
        ]
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    for p in payloads:
        print(
            f"{p['label']}: {len(p['raw'])} -> {len(p['compressed'])} bytes "
            f"(crc32={p['crc']:08X}, sha256={p['sha']})"
        )
    print(f"wrote {output}")


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ime", type=Path, default=root / "m0/share/rospinyin.ime")
    parser.add_argument("--dat", type=Path, default=root / "m0/share/rospinyin.dat")
    parser.add_argument(
        "--cli",
        type=Path,
        default=root / "m0/share/install-rospinyin.exe",
    )
    parser.add_argument(
        "--lex",
        type=Path,
        default=root / "m0/share/rospinyin-lexicon.dat",
        help="M4 word list to embed as the 4th payload",
    )
    parser.add_argument(
        "--no-lex",
        action="store_true",
        help="build the legacy 3-payload header (for compatibility checks)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "tools/install-rospinyin/payload_base64.hpp",
    )
    args = parser.parse_args()
    pack(args.ime, args.dat, args.cli, args.output,
         None if args.no_lex else args.lex)


if __name__ == "__main__":
    main()
