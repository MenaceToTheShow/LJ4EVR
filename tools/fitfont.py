#!/usr/bin/env python3
"""fitfont.py - exact/least-squares fit of per-character advances from widths.tsv alone
(design 7.4 step 3, the cross-check on fontharvest.c).

Every AutoSize label row is one linear equation: sum of the advances of the caption's
characters (after the VCL's '&' rule) = the DFM Width - the face's overhang (1 for a
synthesized-bold raster face, else 0; from the advances file header). The normal equations are reduced
over exact rationals; a character whose column is a pivot with no free-variable dependency
is DETERMINED by the corpus and its fitted advance is compared with the harvested one.
Characters absent from (or only ever co-occurring in) the corpus cannot be determined
and are reported as coverage, not as mismatches.

usage: python -I tools/fitfont.py [--face "MS Sans Serif"] [--height -11] [--style 0]
                                  [--widths tests/fonts/widths.tsv] [--advances tests/fonts/advances_sans13.tsv]
exit 0 = every determined character matches the harvest; 1 = a mismatch (or an inconsistent corpus).
"""
import argparse, os, sys
from fractions import Fraction


def caption_chars(cap):
    """The glyphs Delphi measures: '&x' -> x, '&&' -> '&'; an empty caption measures as ' '."""
    out, i = [], 0
    while i < len(cap):
        if cap[i] == "&" and i + 1 < len(cap):
            if cap[i + 1] == "&": out.append("&")
            else: out.append(cap[i + 1])
            i += 2
        elif cap[i] == "&":
            i += 1
        else:
            out.append(cap[i]); i += 1
    return out or [" "]


def read_widths(path, face, height, style):
    eqs = []
    with open(path, "r", encoding="cp1252", newline="\n") as f:
        next(f)
        for line in f:
            form, obj, fc, h, st, cap, w = line.rstrip("\n").split("\t")
            if fc == face and int(h) == height and (int(st) & 3) == style:
                eqs.append((caption_chars(cap), int(w), form + "." + obj))
    return eqs


def read_advances(path):
    """Returns ({char: advance}, overhang). overhang = tmOverhang, added once per string by
    DrawText for a synthesized-bold raster face (see fontharvest.c)."""
    with open(path, "r", encoding="cp1252") as f:
        names = next(f).rstrip("\n").split("\t")
        vals = dict(zip(names, next(f).rstrip("\n").split("\t")))
        return {chr(int(c)): int(a) for c, a in (ln.split("\t") for ln in f if ln.strip())}, int(vals["overhang"])


def fit(eqs):
    """Returns ({char: Fraction}, inconsistent_count, free_chars). Normal equations, exact RREF."""
    chars = sorted({c for cs, _, _ in eqs for c in cs})
    idx = {c: i for i, c in enumerate(chars)}
    n = len(chars)
    ata = [[Fraction(0)] * (n + 1) for _ in range(n)]
    for cs, w, _ in eqs:
        row = [0] * n
        for c in cs: row[idx[c]] += 1
        for i in range(n):
            if row[i]:
                for j in range(n):
                    if row[j]: ata[i][j] += row[i] * row[j]
                ata[i][n] += row[i] * w
    pivots = []
    r = 0
    for c in range(n):
        p = next((i for i in range(r, n) if ata[i][c] != 0), None)
        if p is None: continue
        ata[r], ata[p] = ata[p], ata[r]
        pv = ata[r][c]
        ata[r] = [x / pv for x in ata[r]]
        for i in range(n):
            if i != r and ata[i][c] != 0:
                k = ata[i][c]
                ata[i] = [a - k * b for a, b in zip(ata[i], ata[r])]
        pivots.append((r, c)); r += 1
    pivot_cols = {c for _, c in pivots}
    free = [chars[c] for c in range(n) if c not in pivot_cols]
    determined = {}
    for r, c in pivots:
        if all(ata[r][j] == 0 for j in range(n) if j != c): determined[chars[c]] = ata[r][n]
    bad = sum(1 for cs, w, _ in eqs if all(c in determined for c in cs) and sum(determined[c] for c in cs) != w)
    return determined, bad, free


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--face", default="MS Sans Serif")
    ap.add_argument("--height", type=int, default=-11)
    ap.add_argument("--style", type=int, default=0)
    ap.add_argument("--widths", default=os.path.join(here, "..", "tests", "fonts", "widths.tsv"))
    ap.add_argument("--advances", default=os.path.join(here, "..", "tests", "fonts", "advances_sans13.tsv"))
    a = ap.parse_args()
    harvested, overhang = read_advances(a.advances)
    eqs = [(cs, w - overhang, who) for cs, w, who in read_widths(a.widths, a.face, a.height, a.style)]
    determined, bad, free = fit(eqs)
    seen = {c for cs, _, _ in eqs for c in cs}
    mism = [(c, v, harvested.get(c)) for c, v in sorted(determined.items()) if v != harvested.get(c)]
    print("%s %d style %d: %d equations, %d distinct chars in corpus, %d determined, %d undetermined (co-occur only), %d of 224 glyphs absent"
          % (a.face, a.height, a.style, len(eqs), len(seen), len(determined), len(seen) - len(determined), 224 - len(seen)))
    print("determined advances matching the harvest: %d / %d; inconsistent equations: %d" % (len(determined) - len(mism), len(determined), bad))
    for c, v, h in mism: print("  MISMATCH %r fitted %s harvested %s" % (c, v, h))
    undet = sorted(seen - set(determined))
    if undet: print("  undetermined:", "".join(undet), "(free:", "".join(sorted(free)) + ")")
    return 1 if (mism or bad) else 0


if __name__ == "__main__":
    sys.exit(main())
