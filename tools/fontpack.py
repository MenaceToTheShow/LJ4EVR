#!/usr/bin/env python3
"""fontpack.py - packs art/fonts/<name>.txt into src/gfx/lj_fonts_gen.c (design 7.4 step 4).

Each sheet (format: art/fonts/FORMAT.md) is validated against tests/fonts/advances_<name>.tsv:
cell height/ascent/descent/overhang and all 224 advances must match the harvest, every row
must be exactly <advance> wide and <height> rows tall. Any mismatch is an error and nothing
is written. The output is the exact lj_font layout of src/gfx/lj_font.h: glyph g = c - 32 is
a standalone 1-bit bitmap, rows MSB-first, stride (advance + 7) / 8, at bits + offset[g].

usage: python -I tools/fontpack.py              regenerate src/gfx/lj_fonts_gen.c
       python -I tools/fontpack.py --check      exit 1 if the committed file is stale (CI)
       python -I tools/fontpack.py --skeleton <name>   write art/fonts/<name>.txt (all '.') from the harvest
"""
import argparse, hashlib, os, sys, unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
ART = os.path.join(ROOT, "art", "fonts")
ADV = os.path.join(ROOT, "tests", "fonts")
OUT = os.path.join(ROOT, "src", "gfx", "lj_fonts_gen.c")


def read_advances(name):
    path = os.path.join(ADV, "advances_%s.tsv" % name)
    with open(path, "r", encoding="cp1252") as f:
        names = next(f).rstrip("\n").split("\t")
        vals = dict(zip(names, next(f).rstrip("\n").split("\t")))
        adv = [int(a) for c, a in (ln.rstrip("\n").split("\t") for ln in f if ln.strip())]
    if len(adv) != 224: raise ValueError("%s: %d advances, want 224" % (path, len(adv)))
    return {k: int(vals[k]) for k in ("height", "ascent", "descent", "overhang")}, adv, path


def glyph_name(code):
    if code == 32: return "space"
    if 33 <= code < 127: return chr(code)
    if code == 127: return "delete"
    try: return unicodedata.name(bytes([code]).decode("cp1252")).lower().replace(" ", "_")
    except (UnicodeDecodeError, ValueError): return "undefined"


def write_skeleton(name):
    m, adv, _ = read_advances(name)
    path = os.path.join(ART, name + ".txt")
    if os.path.exists(path): raise FileExistsError(path + " exists; delete it yourself to regenerate")
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("font %s height %d ascent %d descent %d overhang %d\n\n" % (name, m["height"], m["ascent"], m["descent"], m["overhang"]))
        for g in range(224):
            f.write("glyph %d %d %s\n" % (g + 32, adv[g], glyph_name(g + 32)))
            f.write(("." * adv[g] + "\n") * m["height"] + "\n")
    print("wrote", path)


def read_sheet(path):
    """Returns (name, metrics, [(advance, rows)] * 224). Raises ValueError with file:line on any slip."""
    with open(path, "r", encoding="ascii") as f:
        lines = f.read().split("\n")
    i, n = 0, len(lines)
    def err(msg): raise ValueError("%s:%d: %s" % (path, i + 1, msg))
    while i < n and (not lines[i].strip() or lines[i].startswith("#")): i += 1
    hd = lines[i].split() if i < n else []
    if len(hd) != 10 or hd[0] != "font" or hd[2:9:2] != ["height", "ascent", "descent", "overhang"]: err("bad font header")
    name, m = hd[1], {k: int(v) for k, v in zip(hd[2::2], hd[3::2])}
    i += 1
    glyphs = []
    for code in range(32, 256):
        while i < n and (not lines[i].strip() or lines[i].startswith("#")): i += 1
        hd = lines[i].split() if i < n else []
        if len(hd) < 3 or hd[0] != "glyph": err("expected 'glyph %d ...'" % code)
        if int(hd[1]) != code: err("glyph code %s, expected %d (codes 32..255 in order)" % (hd[1], code))
        adv = int(hd[2]); i += 1
        rows = lines[i:i + m["height"]]
        if len(rows) != m["height"]: err("glyph %d: fewer than %d rows" % (code, m["height"]))
        for r, row in enumerate(rows):
            if len(row) != adv or row.strip("#.") != "": raise ValueError("%s:%d: glyph %d row %d must be %d of '#'/'.', got %r" % (path, i + r + 1, code, r, adv, row))
        i += m["height"]
        glyphs.append((adv, rows))
    return name, m, glyphs


def pack(sheet_path):
    name, m, glyphs = read_sheet(sheet_path)
    hm, hadv, adv_path = read_advances(name)
    bad = ["%s %d != harvested %d" % (k, m[k], hm[k]) for k in hm if m[k] != hm[k]]
    bad += ["glyph %d advance %d != harvested %d" % (g + 32, glyphs[g][0], hadv[g]) for g in range(224) if glyphs[g][0] != hadv[g]]
    if bad: raise ValueError("%s: %s" % (sheet_path, "; ".join(bad[:10])))
    bits, offsets = bytearray(), []
    for adv, rows in glyphs:
        offsets.append(len(bits))
        stride = (adv + 7) // 8
        for row in rows:
            v = int(row.replace("#", "1").replace(".", "0").ljust(stride * 8, "0"), 2) if adv else 0
            bits += v.to_bytes(stride, "big")
    if len(bits) > 65535: raise ValueError("%s: %d bitmap bytes do not fit uint16 offsets" % (sheet_path, len(bits)))
    inked = sum(1 for adv, rows in glyphs if any("#" in r for r in rows))
    return name, m, [g[0] for g in glyphs], offsets, bytes(bits), inked, (sheet_path, adv_path)


def sha(path):
    with open(path, "rb") as f: return hashlib.sha256(f.read()).hexdigest()[:16]


def fmt_array(vals, per_line):
    return "\n".join("    " + ", ".join(str(v) for v in vals[i:i + per_line]) + "," for i in range(0, len(vals), per_line))


def generate(sheets):
    fonts = [pack(p) for p in sheets]
    out = ["/* lj_fonts_gen.c - generated by tools/fontpack.py; do not edit. Inputs (sha256 prefix):"]
    for *_, (sp, ap) in fonts:
        out.append(" *   %s %s" % (os.path.relpath(sp, ROOT).replace("\\", "/"), sha(sp)))
        out.append(" *   %s %s" % (os.path.relpath(ap, ROOT).replace("\\", "/"), sha(ap)))
    out += [" * Glyph pixels are original pixel art (art/fonts/FORMAT.md); metrics are harvested facts. */",
            "#include \"gfx/lj_font.h\"", ""]
    for name, m, adv, off, bits, inked, _ in fonts:
        out.append("/* %s: cell %d = ascent %d + descent %d, overhang %d, %d of 224 glyphs inked */" % (name, m["height"], m["ascent"], m["descent"], m["overhang"], inked))
        out.append("static const uint8_t lj_%s_adv[LJ_FONT_GLYPHS] = {\n%s\n};" % (name, fmt_array(adv, 32)))
        out.append("static const uint16_t lj_%s_off[LJ_FONT_GLYPHS] = {\n%s\n};" % (name, fmt_array(off, 16)))
        out.append("static const uint8_t lj_%s_bits[%d] = {\n%s\n};" % (name, len(bits), fmt_array(["0x%02x" % b for b in bits], 24)))
        # ponytail: lj_font has no overhang field yet; add ".overhang = %d," here when it does (see docs/fonts.md)
        out.append("const lj_font lj_font_%s = { .height = %d, .ascent = %d, .descent = %d, /* overhang %d */\n"
                   "    .advance = lj_%s_adv, .offset = lj_%s_off, .bits = lj_%s_bits };\n"
                   % (name, m["height"], m["ascent"], m["descent"], m["overhang"], name, name, name))
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--skeleton", metavar="NAME")
    a = ap.parse_args()
    if a.skeleton:
        write_skeleton(a.skeleton); return 0
    sheets = sorted(os.path.join(ART, f) for f in os.listdir(ART) if f.endswith(".txt"))
    if not sheets: print("no sheets in", ART, file=sys.stderr); return 1
    text = generate(sheets)
    if a.check:
        try:
            with open(OUT, "r", encoding="ascii", newline="\n") as f: cur = f.read()
        except OSError: cur = None
        if cur != text: print("STALE: %s differs from the sheets; run tools/fontpack.py" % OUT, file=sys.stderr); return 1
        print("%s is current (%d fonts)" % (OUT, len(sheets))); return 0
    with open(OUT, "w", encoding="ascii", newline="\n") as f: f.write(text)
    print("wrote %s (%d fonts: %s)" % (OUT, len(sheets), ", ".join(os.path.basename(s)[:-4] for s in sheets)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as e:
        print("fontpack: %s" % e, file=sys.stderr); sys.exit(1)
