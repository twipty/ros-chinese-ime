#!/usr/bin/env python3
"""Build two diagnostic fonts from the ReactOS baseline Droid Sans Fallback.

Both fonts keep the exact family name (``Droid Sans Fallback``), glyph count,
cmap and all non-target glyphs, so they can be dropped in as the system font
with no registration step. They exist ONLY for VM codepoint forensics and are
never shipped in the IME release.

  * proof  -- each target punctuation glyph is redrawn as its 4-hex-digit
              label (U+201C renders as "201C"). A screenshot then reveals the
              exact codepoint reaching Notepad, independent of glyph design.
  * display-- each target punctuation glyph is replaced by the proper Noto
              Sans CJK SC outline (CFF -> quadratic via cu2qu), so curly
              quotes / ellipsis / em-dash render as real Chinese punctuation
              instead of Droid's slashes and dots.

Inputs (fixed, reproducible):
  baseline : m0/probe/reactos/Fonts/DroidSansFallback.ttf
             sha256 82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf
  display  : /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc (face[2] = SC)

Usage:
  python3 tools/fonts/make_probe_fonts.py --out-dir m0/share
"""
import argparse
import hashlib
import sys
from pathlib import Path

from fontTools.ttLib import TTFont, TTCollection
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.cu2quPen import Cu2QuPen

ROOT = Path(__file__).resolve().parents[2]
DROID = ROOT / "m0/probe/reactos/Fonts/DroidSansFallback.ttf"
NOTO_TTC = Path("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc")
DROID_SHA = "82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf"

# target punctuation codepoints = the M3 shared punctuation table's true-CJK set
TARGETS = [
    0x201C, 0x201D, 0x2018, 0x2019, 0x2026, 0x2014, 0x3001, 0xFFE5,
    0x00B7, 0xFF0C, 0x3002, 0xFF1F, 0x003F, 0xFF01, 0xFF5E, 0xFF1B,
    0xFF1A, 0xFF08, 0xFF09, 0x3010, 0x3011, 0x300A, 0x300B,
]

PROOF_NAME = "rospinyin-proof.ttf"
DISPLAY_NAME = "rospinyin-display.ttf"


def sha256(path):
    h = hashlib.sha256()
    h.update(Path(path).read_bytes())
    return h.hexdigest()


def load_baseline():
    if not DROID.exists():
        sys.exit(f"baseline font missing: {DROID}")
    actual = sha256(DROID)
    if actual != DROID_SHA:
        sys.exit(f"baseline font sha256 mismatch: {actual} != {DROID_SHA}")
    return TTFont(DROID)


def build_proof(out_path):
    """Redraw target glyphs as 4-hex-digit labels."""
    font = load_baseline()
    cmap = font.getBestCmap()
    glyphset = font.getGlyphSet()
    glyf = font["glyf"]
    hmtx = font["hmtx"]
    hex_glyph = {ch: cmap[ord(ch)] for ch in "0123456789ABCDEF"}

    for cp in TARGETS:
        gname = cmap.get(cp)
        if gname is None:
            print(f"  proof: U+{cp:04X} not in baseline, skip", file=sys.stderr)
            continue
        pen = TTGlyphPen(glyphset)
        x = 0
        for ch in f"{cp:04X}":
            gn = hex_glyph[ch]
            glyphset[gn].draw(TransformPen(pen, (1, 0, 0, 1, x, 0)))
            x += hmtx[gn][0]
        glyf[gname] = pen.glyph()
        hmtx[gname] = (x, 0)

    font.save(out_path)


def build_display(out_path):
    """Replace target glyphs with Noto Sans CJK SC outlines."""
    if not NOTO_TTC.exists():
        print(f"  display: Noto TTC missing ({NOTO_TTC}), skip", file=sys.stderr)
        return False
    font = load_baseline()
    dcmap = font.getBestCmap()
    dgset = font.getGlyphSet()
    dglyf = font["glyf"]
    dhmtx = font["hmtx"]
    d_upem = font["head"].unitsPerEm

    noto = TTCollection(NOTO_TTC).fonts[2]  # face 2 = Noto Sans CJK SC
    ncmap = noto.getBestCmap()
    ngset = noto.getGlyphSet()
    nhmtx = noto["hmtx"]
    scale = d_upem / noto["head"].unitsPerEm

    for cp in TARGETS:
        dgn, ngn = dcmap.get(cp), ncmap.get(cp)
        if dgn is None or ngn is None:
            print(f"  display: U+{cp:04X} missing (droid={dgn}, noto={ngn}), skip",
                  file=sys.stderr)
            continue
        pen = TTGlyphPen(None)
        cu = Cu2QuPen(pen, max_err=0.5, reverse_direction=True)
        ngset[ngn].draw(TransformPen(cu, (scale, 0, 0, scale, 0, 0)))
        glyph = pen.glyph()
        glyph.recalcBounds(dglyf)
        dglyf[dgn] = glyph
        dhmtx[dgn] = (int(round(nhmtx[ngn][0] * scale)), 0)

    font.save(out_path)
    return True


def verify(path):
    f = TTFont(path)
    names = {r.toUnicode() for r in f["name"].names
             if r.nameID == 1 and r.platformID == 3}
    cm = f.getBestCmap()
    assert "Droid Sans Fallback" in names, f"family changed in {path}"
    assert len(cm) == 29622, f"cmap changed in {path}: {len(cm)}"
    assert f["maxp"].numGlyphs == 43628, f"glyph count changed in {path}"
    return cm


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="m0/share")
    args = ap.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    proof = out / PROOF_NAME
    display = out / DISPLAY_NAME
    build_proof(proof)
    got_display = build_display(display)

    for p in [proof] + ([display] if got_display else []):
        cm = verify(p)
        print(f"{p.name:26s} {p.stat().st_size:>9d} B  "
              f"sha256={sha256(p)}  cmap={len(cm)}  "
              f"U+201C={cm.get(0x201C)}")


if __name__ == "__main__":
    main()
