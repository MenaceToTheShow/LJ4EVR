#!/usr/bin/env python3
"""widthcorpus.py - the font width corpus and the ME-18 font census (design 7.4 step 1).

Reads reference/installer/exe_resources/dfm_text/*.dfm.txt (Martin-copyrighted, never
committed; captions, geometry and font facts are reproduced as facts) and writes

  tests/fonts/widths.tsv       form  object  face  height  style  caption  width
                               one row per AutoSize TLabel (AutoSize defaults True;
                               a label with AutoSize = False or WordWrap = True is skipped)
  tests/fonts/font_census.tsv  face  height  style  controls  classes
                               every distinct font over every visual control (the form itself included)

Font resolution per control: its own Font.Name/Height/Style, else the nearest ancestor
that has a Font, else the form's Font, else MS Sans Serif -11 regular.
Style bits: fsBold 1, fsItalic 2, fsUnderline 4, fsStrikeOut 8.

usage: python -I tools/widthcorpus.py [--ref <dfm_text dir>] [--out tests/fonts]
"""
import argparse, glob, os, re, sys

NONVISUAL = {"TMainMenu", "TPopupMenu", "TMenuItem", "TTimer", "TAction", "TActionList", "TOpenDialog",
             "TSaveDialog", "TColorDialog", "TOpenPictureDialog", "TImageList", "TFontDialog", "TPrintDialog"}
STYLE_BITS = {"fsBold": 1, "fsItalic": 2, "fsUnderline": 4, "fsStrikeOut": 8}
DEFAULT_FONT = ("MS Sans Serif", -11, 0)
OBJ_RE = re.compile(r"^\s*(object|inherited|inline)\s+(\w+)\s*:\s*(\w+)\s*$")
PROP_RE = re.compile(r"^\s*([\w.]+)\s*=\s*(.*?)\s*$")


def caption_text(v):
    # The dfm_text converter already unescaped '' to ', so the value is 'literal' with no codes.
    m = re.match(r"^'(.*)'$", v)
    if m: return m.group(1)
    out, i = "", 0            # the raw Delphi form, in case a file still carries #NN codes
    while i < len(v):
        if v[i] == "'":
            j = v.index("'", i + 1); out += v[i + 1:j]; i = j + 1
        elif v[i] == "#":
            m = re.match(r"#(\d+)", v[i:]); out += chr(int(m.group(1))); i += m.end()
        else:
            i += 1
    return out


def parse_form(path):
    """Yields (form_class, obj_name, obj_class, props) for every object, props = dict of raw values."""
    stack = []   # [name, cls, props]
    form_cls = None
    with open(path, "r", encoding="cp1252", newline="") as f:
        for line in f:
            line = line.rstrip("\r\n")
            m = OBJ_RE.match(line)
            if m:
                stack.append([m.group(2), m.group(3), {}])
                if form_cls is None: form_cls = m.group(3)
                continue
            if line.strip() == "end" and stack:
                name, cls, props = stack.pop()
                yield form_cls, name, cls, props, [s[2] for s in stack]
                continue
            m = PROP_RE.match(line)
            if m and stack:
                stack[-1][2][m.group(1)] = m.group(2)


def font_of(props, ancestors):
    """ancestors: outermost first; each is a props dict."""
    face = height = style = None
    for p in [props] + ancestors[::-1]:
        if face is None and "Font.Name" in p: face = caption_text(p["Font.Name"])
        if height is None and "Font.Height" in p: height = int(p["Font.Height"])
        if style is None and "Font.Style" in p:
            style = sum(STYLE_BITS[s] for s in re.findall(r"fs\w+", p["Font.Style"]))
        if face is not None and height is not None and style is not None: break
    return (face if face is not None else DEFAULT_FONT[0],
            height if height is not None else DEFAULT_FONT[1],
            style if style is not None else DEFAULT_FONT[2])


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--ref", default=os.path.join(here, "..", "reference", "installer", "exe_resources", "dfm_text"))
    ap.add_argument("--out", default=os.path.join(here, "..", "tests", "fonts"))
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(a.ref, "*.dfm.txt")))
    if not files:
        print("no .dfm.txt under", a.ref, file=sys.stderr); return 1
    rows, census, partial = [], {}, []
    for path in files:
        for form_cls, name, cls, props, anc in parse_form(path):
            if cls in NONVISUAL: continue
            face, height, style = font_of(props, anc)
            if "ParentFont" in props and props["ParentFont"] == "False" and not all(k in props for k in ("Font.Name", "Font.Height", "Font.Style")):
                partial.append("%s.%s (%s)" % (form_cls, name, cls))
            key = (face, height, style)
            c = census.setdefault(key, [0, {}])
            c[0] += 1; c[1][cls] = c[1].get(cls, 0) + 1
            if cls != "TLabel" or props.get("AutoSize") == "False" or props.get("WordWrap") == "True": continue
            if "Caption" not in props or "Width" not in props: continue
            rows.append((form_cls, name, face, height, style, caption_text(props["Caption"]), int(props["Width"])))
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "widths.tsv"), "w", encoding="cp1252", newline="\n") as f:
        f.write("form\tobject\tface\theight\tstyle\tcaption\twidth\n")
        for r in rows: f.write("\t".join(str(x) for x in r) + "\n")
    with open(os.path.join(a.out, "font_census.tsv"), "w", encoding="cp1252", newline="\n") as f:
        f.write("face\theight\tstyle\tcontrols\tclasses\n")
        for key in sorted(census, key=lambda k: (k[0], -k[1], k[2])):
            n, classes = census[key]
            f.write("%s\t%d\t%d\t%d\t%s\n" % (key[0], key[1], key[2], n,
                    " ".join("%s:%d" % kv for kv in sorted(classes.items(), key=lambda kv: -kv[1]))))
    print("%d forms, %d AutoSize TLabel rows, %d distinct fonts" % (len(files), len(rows), len(census)))
    if partial: print("ParentFont=False with a partial Font (inherits the rest):", len(partial), partial[:5])
    return 0


if __name__ == "__main__":
    sys.exit(main())
