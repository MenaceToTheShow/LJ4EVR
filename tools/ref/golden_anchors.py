#!/usr/bin/env python3
"""golden_anchors.py - W6 ME-162 / ME-161-3DLIGHT: the REVIEWED.tsv rows of a golden re-render.

For every golden under tests/ui/golden that differs from git revision --since, the pixel diff: how many pixels went
(192,192,192) -> (223,223,223) (the caption buttons' inner top-left line, ME-161-3DLIGHT) and every other colour pair
with its bounding box. For every form whose newest REVIEWED.tsv row still begins "W4-GOLD DFM-only render" (the ME-162
templates), the DFM anchors probed in the golden: each top-level control that draws an edge at its DFM corner (TPanel
bevel or border, TBevel box / line, TButton / TBitBtn, the sunken list / edit family, TShape rectangle) is looked up at
client origin + (Left, Top) and must carry its edge colour there; and the CHM capture: a GIF of a help topic that names
the form's caption with the golden's aspect ratio (within 3%), with its scale, or "no CHM capture".

usage: golden_anchors.py --ref <reference dir> [--since HEAD] [--date 2026-10-23] [--reviewer "W6 fidelity"]
                         [--note <tsv: form, text>] [--write]
Without --write the rows are printed; --write appends them to tests/ui/golden/REVIEWED.tsv (earlier rows untouched).
The reference tree is read as data only (DFM text, help text, GIF headers); nothing of it is written anywhere.
Python 3, stdlib only.
"""
import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import dfm2c  # noqa: E402  (the repo's DFM text parser)

GOLDEN = os.path.join(REPO, "tests", "ui", "golden")
REVIEWED = os.path.join(GOLDEN, "REVIEWED.tsv")
TEMPLATE = "W4-GOLD DFM-only render"
FACE, LIGHT = 0xC0C0C0, 0xDFDFDF
SUNKEN = {"TEdit", "TMemo", "TListBox", "TComboBox", "TRichEdit", "TListView", "TTreeView", "TStringGrid", "TDrawGrid",
          "TCheckListBox", "TMaskEdit", "TNumEdit", "TSpinEdit"}


def read_bmp(data):
    """24 / 32-bit BI_RGB BMP bytes -> (w, h, rows[y][x] = 0xRRGGBB); every offset checked."""
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("not a BMP")
    off, w, h = int.from_bytes(data[10:14], "little"), int.from_bytes(data[18:22], "little", signed=True), \
        int.from_bytes(data[22:26], "little", signed=True)
    bpp = int.from_bytes(data[28:30], "little")
    if bpp not in (24, 32) or w <= 0 or h == 0 or w > 8192 or abs(h) > 8192:
        raise ValueError("unsupported BMP %dx%d %d bpp" % (w, h, bpp))
    bp, stride = bpp // 8, (w * bpp // 8 + 3) & ~3
    if off + stride * abs(h) > len(data):
        raise ValueError("truncated BMP")
    rows = []
    for r in range(abs(h)):
        y = abs(h) - 1 - r if h > 0 else r
        base = off + y * stride
        rows.append([data[base + bp * x + 2] << 16 | data[base + bp * x + 1] << 8 | data[base + bp * x] for x in range(w)])
    return w, abs(h), rows


def gif_size(path):
    path = path.replace("%20", " ")
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        hdr = f.read(10)
    if len(hdr) < 10 or hdr[:3] != b"GIF":
        return None
    return int.from_bytes(hdr[6:8], "little"), int.from_bytes(hdr[8:10], "little")


def reviewed_rows():
    rows = []
    for line in open(REVIEWED, encoding="cp1252").read().splitlines():
        p = line.split("\t")
        if len(p) >= 4 and p[0] != "form":
            rows.append(p)
    return rows


def newest(rows):
    last = {}
    for p in rows:
        last[p[0]] = p
    return last


def diff(since):
    """{golden name: (old rows or None, new rows)} for every golden that differs from `since` (or is new)."""
    out = {}
    names = subprocess.run(["git", "-C", REPO, "status", "--porcelain", "--", "tests/ui/golden"], capture_output=True, text=True).stdout
    names += subprocess.run(["git", "-C", REPO, "diff", "--name-only", since, "--", "tests/ui/golden"], capture_output=True, text=True).stdout
    for line in sorted(set(names.splitlines())):
        rel = line[3:] if line[:2].strip() in ("M", "??", "A") and line[2:3] == " " else line
        if not rel.endswith(".bmp") or not os.path.exists(os.path.join(REPO, rel)):
            continue
        name = os.path.basename(rel)[:-4]
        new = read_bmp(open(os.path.join(REPO, rel), "rb").read())
        old = subprocess.run(["git", "-C", REPO, "show", "%s:%s" % (since, rel)], capture_output=True)
        out[name] = (read_bmp(old.stdout) if old.returncode == 0 else None, new)
    return out


def classify(old, new):
    if old is None:
        return "new golden, no previous"
    if old[:2] != new[:2]:
        return "resized %dx%d -> %dx%d" % (old[0], old[1], new[0], new[1])
    light, pairs, box = 0, {}, None
    for y in range(new[1]):
        for x in range(new[0]):
            a, b = old[2][y][x], new[2][y][x]
            if a == b:
                continue
            if a == FACE and b == LIGHT:
                light += 1
                continue
            k = "#%06X->#%06X" % (a, b)
            pairs[k] = pairs.get(k, 0) + 1
            box = [x, y, x, y] if box is None else [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
    s = "%d px changed vs %s" % (light + sum(pairs.values()), "the previous golden")
    s += ": %d (192,192,192)->(223,223,223) on the caption buttons' inner top-left lines (ME-161-3DLIGHT, movement2.gif)" % light
    if pairs:
        s += "; %d other px in x,y %d,%d-%d,%d (%s)" % (sum(pairs.values()), box[0], box[1], box[2], box[3],
                                                     ", ".join("%s %d" % kv for kv in sorted(pairs.items())))
    else:
        s += "; nothing else moved"
    return s


def ident(o, key, dflt):
    v = o.get(key)
    return v[1] if v and v[0] in ("ident", "int") else dflt


def edge(o):
    """the colour a control paints at its DFM top-left pixel, or None when it draws no edge there"""
    c = o.cls
    if c == "TPanel":
        bo = ident(o, "BevelOuter", "bvRaised")
        if ident(o, "BorderStyle", "bsNone") == "bsSingle":
            return {0x000000}                  # the single border sits outside the bevel
        if bo == "bvRaised":
            return {0xFFFFFF}
        if bo == "bvLowered":
            return {0x808080}
        return None
    if c == "TBevel":
        sh, st = ident(o, "Shape", "bsBox"), ident(o, "Style", "bsLowered")
        if sh in ("bsBox", "bsFrame", "bsTopLine", "bsLeftLine"):
            return {0x808080} if st == "bsLowered" else {0xFFFFFF}
        return None
    if c in ("TButton", "TBitBtn"):
        return {0xFFFFFF, 0x000000}            # raised, or the default button's black frame
    if c in SUNKEN:
        return None if ident(o, "BorderStyle", "bsSingle") == "bsNone" else {0x808080}
    if c == "TShape" and ident(o, "Shape", "stRectangle") == "stRectangle":
        return {0x000000}
    return None


def origin(row_text, w, h):
    m = re.search(r"DFM ClientWidth (\d+) \+ 2x(\d+) frame by ClientHeight (\d+)", row_text)
    if m:
        cw, f, ch = int(m.group(1)), int(m.group(2)), int(m.group(3))
    else:
        m, mf = re.search(r"client (\d+)x(\d+) \(derived", row_text), re.search(r"frame (\d+) px", row_text)
        if not m or not mf:
            return None
        cw, ch, f = int(m.group(1)), int(m.group(2)), int(mf.group(1))
    return f, h - ch - f


def probe(form, dfm_path, row_text, img):
    w, h, px = img
    if not os.path.exists(dfm_path):
        return "no DFM text on this PC"
    root = dfm2c.parse_dfm(open(dfm_path, encoding="cp1252").read())
    o = origin(row_text, w, h)
    if o is None:
        return "client origin not stated in the template row"
    ox, oy = o
    hits, out = 0, []
    for c in root.children:
        want = edge(c)
        if want is None or ident(c, "Visible", "True") == "False":
            continue
        L, T = ident(c, "Left", None), ident(c, "Top", None)
        if not isinstance(L, int) or not isinstance(T, int):
            continue
        x, y = ox + L, oy + T
        if not (0 <= x < w and 0 <= y < h):
            continue
        got = px[y][x]
        ok = got in want
        hits += ok
        out.append("%s %s (%d,%d) at (%d,%d) #%06X%s" % (c.name, c.cls, L, T, x, y, got, "" if ok else " MISS"))
        if len(out) == 4:
            break
    head = "client origin (%d,%d)" % (ox, oy)
    if not out:
        return head + ", no top-level control draws an edge at its DFM corner (labels, flat buttons, group boxes only)"
    return head + "; " + "; ".join(out) + "; %d of %d on the edge colour" % (hits, len(out))


def topics(ref):
    path = os.path.join(ref, "installer", "lj-Help_chm_2.209.txt")
    if not os.path.exists(path):
        return []
    return [(t, re.findall(r"\[IMG:([^\]]+)\]", t)) for t in open(path, encoding="cp1252", errors="replace").read().split("######## ")]


def chm(ref, tops, caption, w, h):
    """a GIF of a help topic whose title holds the caption (or the caption the title), at least half the golden's width,
    with its aspect ratio within 3%"""
    if not caption or len(caption) < 4:
        return "no CHM capture (caption too short to search)"
    best, cl = None, caption.replace("&", "").rstrip(" .:").lower()
    for text, imgs in tops:
        title = text.split("  (", 1)[0].strip().lower()
        if not title or not (cl in title or title in cl):
            continue
        for g in imgs:
            sz = gif_size(os.path.join(ref, "screenshots", "lj-Help_chm_images", g))
            if not sz or sz[0] > w + 2 or sz[1] > h + 2 or sz[0] * 2 < w:
                continue
            err = abs(sz[0] / sz[1] - w / h) / (w / h)
            if err <= 0.03 and (best is None or err < best[0]):
                best = (err, g, sz)
    if not best:
        return "no CHM capture (no GIF of a help topic titled \"%s\" has the golden's aspect within 3%%)" % caption
    g, sz = best[1], best[2]
    if sz == (w, h):
        return "CHM %s %dx%d = 1:1" % (g, sz[0], sz[1])
    return "CHM %s %dx%d, scale %.2f (reduced: layout only, note 07 s14)" % (g, sz[0], sz[1], sz[0] / w)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True)
    ap.add_argument("--since", default="HEAD")
    ap.add_argument("--date", default="2026-10-23")
    ap.add_argument("--reviewer", default="W6 fidelity")
    ap.add_argument("--note")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    rows = reviewed_rows()
    last = newest(rows)
    notes = {}
    if a.note:
        for line in open(a.note, encoding="cp1252").read().splitlines():
            p = line.split("\t", 1)
            if len(p) == 2:
                notes[p[0]] = p[1]
    changed = diff(a.since)
    tops = topics(a.ref)
    out, ntemplate = [], 0
    for name in sorted(set(changed) | {f for f, p in last.items() if p[3].startswith(TEMPLATE)}):
        old, new = changed.get(name, (None, None))
        if new is None:
            new = read_bmp(open(os.path.join(GOLDEN, name + ".bmp"), "rb").read())
        parts = []
        if name in last and last[name][3].startswith(TEMPLATE):
            ntemplate += 1
            dfm = os.path.join(a.ref, "installer", "exe_resources", "dfm_text", name + ".dfm.txt")
            cap = None
            if os.path.exists(dfm):
                v = dfm2c.parse_dfm(open(dfm, encoding="cp1252").read()).get("Caption")
                cap = v[1] if v and v[0] == "str" else None
                cap = cap.decode("cp1252") if isinstance(cap, bytes) else cap      # dfm2c keeps strings as CP1252 bytes
            parts.append("ME-162 DFM anchors probed in %s.bmp (%dx%d): %s" % (name, new[0], new[1], probe(name, dfm, last[name][3], new)))
            parts.append(chm(a.ref, tops, cap, new[0], new[1]))
        if name in changed:
            parts.append("re-rendered: " + classify(old, new))
        if name in notes:
            parts.append(notes[name])
        out.append("\t".join((name, a.date, a.reviewer, "; ".join(parts))))
    for line in out:
        print(line)
    print("golden_anchors: %d row(s), %d ME-162 template form(s), %d re-rendered golden(s)" % (len(out), ntemplate, len(changed)), file=sys.stderr)
    if a.write:
        with open(REVIEWED, "a", encoding="cp1252", newline="\n") as f:
            for line in out:
                f.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
