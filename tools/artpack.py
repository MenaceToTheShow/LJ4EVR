#!/usr/bin/env python3
"""artpack.py - packs art/glyphs/*.txt, art/icons/*.txt and art/gobos/*.txt into src/gfx/lj_art_gen.c, with
art/lj2names.tsv (our stem -> the LJ2 Source-Images file it stands in for) and art/gels.txt (our palette swatches)
(design 7.7, 7.8; D2).

Each sheet (format: art/glyphs/FORMAT.md) is a palette line then h rows of w palette characters.
The output is lj_art_table[] / lj_art_count of gfx/lj_glyphs.h: one {name, {w, h, px}} row per
file, name = the file stem, pixels 0x00RRGGBB row-major. The bottom-left pixel is the transparency
key lj_blit_rgb() uses, so a sheet whose bottom-left pixel is not the first palette entry ('.') is
refused: a glyph that happens to ink that pixel would turn its own ink transparent.

usage: python -I tools/artpack.py            regenerate src/gfx/lj_art_gen.c
       python -I tools/artpack.py --check    exit 1 if the committed file is stale (CI)
"""
import argparse, hashlib, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DIRS = [os.path.join(ROOT, "art", d) for d in ("glyphs", "icons", "gobos")]
LJ2NAMES = os.path.join(ROOT, "art", "lj2names.tsv")
GELS = os.path.join(ROOT, "art", "gels.txt")
LJ2_FOLDERS = ("Glyphs", "Fixtures", "Colors", "Gobos")
OUT = os.path.join(ROOT, "src", "gfx", "lj_art_gen.c")
MAX_SIDE = 256                                   # icons are ~50 px, glyphs 16/24; anything bigger is a mistake


def read_sheet(path):
    """Returns (name, w, h, [rgb...]) or raises ValueError with file:line."""
    with open(path, "r", encoding="ascii") as f:
        lines = [ln.rstrip("\n") for ln in f if ln.strip()]      # no comment lines: '#' is a palette character
    body = [ln for ln in lines if not ln.startswith("palette ")]
    pal_lines = [ln for ln in lines if ln.startswith("palette ")]
    if len(pal_lines) != 1 or lines[0] != pal_lines[0]: raise ValueError("%s:1: first line must be 'palette .=RRGGBB ...'" % path)
    pal = {}
    for tok in pal_lines[0].split()[1:]:
        if len(tok) != 8 or tok[1] != "=": raise ValueError("%s:1: bad palette entry %r (want c=RRGGBB)" % (path, tok))
        try: pal[tok[0]] = int(tok[2:], 16)
        except ValueError: raise ValueError("%s:1: bad colour in %r" % (path, tok))
    if "." not in pal: raise ValueError("%s:1: palette must define '.', the transparency key" % path)
    if not body: raise ValueError("%s: no pixel rows" % path)
    w, h = len(body[0]), len(body)
    if not 1 <= w <= MAX_SIDE or h > MAX_SIDE: raise ValueError("%s: %dx%d is outside 1..%d" % (path, w, h, MAX_SIDE))
    px = []
    for i, row in enumerate(body):
        if len(row) != w: raise ValueError("%s:%d: row is %d wide, want %d" % (path, i + 2, len(row), w))
        for ch in row:
            if ch not in pal: raise ValueError("%s:%d: %r is not in the palette" % (path, i + 2, ch))
            px.append(pal[ch])
    if body[-1][0] != ".": raise ValueError("%s:%d: bottom-left pixel must be '.', the transparency key" % (path, h + 1))
    return os.path.basename(path)[:-4], w, h, px


def read_lj2names(stems):
    """[(stem, folder, file)]: every stem must be packed, every (folder, file) unique."""
    rows, seen = [], set()
    with open(LJ2NAMES, "r", encoding="cp1252") as f:
        for n, ln in enumerate(f, 1):
            ln = ln.rstrip("\n")
            if not ln or ln.startswith("#") or ln.startswith("stem\t"): continue
            t = ln.split("\t")
            if len(t) != 3 or t[1] not in LJ2_FOLDERS or not t[2]: raise ValueError("%s:%d: want stem<TAB>folder<TAB>file" % (LJ2NAMES, n))
            if t[0] not in stems: raise ValueError("%s:%d: stem %r has no art" % (LJ2NAMES, n, t[0]))
            k = (t[1], t[2].lower())
            if k in seen or t[0] in {r[0] for r in rows}: raise ValueError("%s:%d: duplicate %r" % (LJ2NAMES, n, t[0]))
            seen.add(k); rows.append(tuple(t))
    return rows


def read_gels():
    """[(name, text, [rgb...])]: look = RRGGBB[/RRGGBB...] (1-4 colours) or text:<LABEL>."""
    rows, seen = [], set()
    with open(GELS, "r", encoding="cp1252") as f:
        for n, ln in enumerate(f, 1):
            ln = ln.rstrip("\n")
            if not ln or ln.startswith("#"): continue
            t = ln.split("\t")
            if len(t) != 2 or not t[1]: raise ValueError("%s:%d: want look<TAB>name" % (GELS, n))
            key = "".join(ch for ch in t[1].lower() if ch.isalnum())
            if key in seen: raise ValueError("%s:%d: %r matches an earlier name" % (GELS, n, t[1]))
            seen.add(key)
            if t[0].startswith("text:"):
                if not 1 <= len(t[0]) - 5 <= 6: raise ValueError("%s:%d: label is 1..6 characters" % (GELS, n))
                rows.append((t[1], t[0][5:], [])); continue
            cols = t[0].split("/")
            try:
                if not 1 <= len(cols) <= 4 or any(len(c) != 6 for c in cols): raise ValueError
                rows.append((t[1], None, [int(c, 16) for c in cols]))
            except ValueError: raise ValueError("%s:%d: bad look %r" % (GELS, n, t[0]))
    return rows


def cstr(b):
    return '"' + "".join(chr(c) if 32 <= c < 127 and c not in (34, 92, 63) else "\\%03o" % c for c in b) + '"'


def sha(path):
    with open(path, "rb") as f: return hashlib.sha256(f.read()).hexdigest()[:16]


def cname(name):
    return "".join(c if c.isalnum() else "_" for c in name)


def generate():
    sheets = sorted(os.path.join(d, f) for d in DIRS if os.path.isdir(d) for f in os.listdir(d) if f.endswith(".txt"))
    arts, seen = [], {}
    for p in sheets:
        name, w, h, px = read_sheet(p)
        if name in seen: raise ValueError("%s: name %r already used by %s" % (p, name, seen[name]))
        seen[name] = p
        arts.append((name, w, h, px, p))
    lj2 = read_lj2names(seen)
    gels = read_gels()
    out = ["/* lj_art_gen.c - generated by tools/artpack.py; do not edit. Inputs (sha256 prefix):"]
    out += [" *   %s %s" % (os.path.relpath(p, ROOT).replace("\\", "/"), sha(p)) for p in [a[4] for a in arts] + [LJ2NAMES, GELS]]
    out += [" * Original pixel art (art/glyphs/FORMAT.md), 0x00RRGGBB rows (C<n> names a colour); the bottom-left pixel is the",
            " * transparency key. px is const data: lj_art_entry.img.px is uint32_t* for lj_surface's sake",
            " * and is never written (gfx/lj_glyphs.h). */",
            "#include \"gfx/lj_glyphs.h\"", ""]
    freq = {}
    for a in arts:
        for v in a[3]: freq[v] = freq.get(v, 0) + 1
    order = sorted(freq, key=lambda v: (-freq[v], v))
    short = {v: "C%d" % k for k, v in enumerate(order)}   # C0 = the commonest colour: keeps the generated file small
    out += ["#define %s 0x%06Xu" % (short[v], v) for v in order]
    out.append("")
    for name, w, h, px, _ in arts:
        rows = "\n".join("    " + ",".join(short[v] for v in px[y * w:(y + 1) * w]) + "," for y in range(h))
        out.append("static const uint32_t lj_art_%s[%d] = {   /* %dx%d */\n%s\n};" % (cname(name), w * h, w, h, rows))
    out.append("")
    out.append("const lj_art_entry lj_art_table[%d] = {" % max(1, len(arts)))
    out += ['    { "%s", { %d, %d, (uint32_t *)lj_art_%s } },' % (name, w, h, cname(name)) for name, w, h, _, _ in arts]
    if not arts: out.append('    { "", { 0, 0, (uint32_t *)0 } },')
    out.append("};")
    out.append("const int lj_art_count = %d;" % len(arts))
    out.append("")
    out.append("const lj_art_lj2 lj_art_lj2_table[%d] = {" % max(1, len(lj2)))
    out += ["    { %s, %s, %s }," % tuple(cstr(x.encode("cp1252")) for x in r) for r in lj2]
    out.append("};")
    out.append("const int lj_art_lj2_count = %d;" % len(lj2))
    out.append("")
    out.append("const lj_gel lj_gel_table[%d] = {" % max(1, len(gels)))
    out += ["    { %s, %s, %d, { %s } }," % (cstr(n.encode("cp1252")), cstr(t.encode("cp1252")) if t else "NULL", len(c), ", ".join("0x%06Xu" % v for v in c) or "0")
            for n, t, c in gels]
    out.append("};")
    out.append("const int lj_gel_count = %d;" % len(gels))
    out.append("")
    return "\n".join(out), arts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    text, arts = generate()
    if a.check:
        try:
            with open(OUT, "r", encoding="ascii", newline="\n") as f: cur = f.read()
        except OSError: cur = None
        if cur != text: print("STALE: %s differs from art/; run tools/artpack.py" % OUT, file=sys.stderr); return 1
        print("%s is current (%d entries)" % (OUT, len(arts))); census(arts); return 0
    with open(OUT, "w", encoding="ascii", newline="\n") as f: f.write(text)
    print("wrote %s (%d entries, %d pixels)" % (OUT, len(arts), sum(len(px) for *_, px, _ in arts)))
    census(arts)
    return 0


def census(arts):
    """W4-ART counts: LJ2 Glyphs names covered (target 210), fixture icons (58), gobo tiles, gel rows."""
    lj2 = read_lj2names({a[0] for a in arts})
    per = lambda d: sum(1 for a in arts if os.path.basename(os.path.dirname(a[4])) == d)
    print("census: %d LJ2 Glyphs names -> our stems, %d glyph sheets, %d icons, %d gobo tiles, %d gel rows"
          % (sum(1 for r in lj2 if r[1] == "Glyphs"), per("glyphs"), per("icons"), per("gobos"), len(read_gels())))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as e:
        print("artpack: %s" % e, file=sys.stderr); sys.exit(1)
