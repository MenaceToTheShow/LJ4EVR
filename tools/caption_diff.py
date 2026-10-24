#!/usr/bin/env python3
"""caption_diff.py - W4 acceptance 1 (design 10 W4, F55): the runtime captions of every implemented form against
13-dfm-captions.tsv.

Input 1 is a dump written by the UI script verb `dump_captions <path>` (src/app/lj_uiscript.c): one line per runtime
Caption / Text / Hint of every widget and menu item of each open window, "Class<TAB>object<TAB>property<TAB>value",
CP1252. Input 2 is the research table 13-dfm-captions.tsv (form, dfm_line, object, class, property, value, in_chm;
the captions of the 208 DFMs, facts, D2/D13). A form is "implemented" when the dump has it.

For every tsv row of an implemented form (property Caption, Hint or Text; TAction rows skipped: an action's caption
reaches the screen through the menu item or button that links it):
  differs  the dump has the object and property with another value
  absent   the dump has no such object/property (a Hint not shown because ShowHint is off, a stub, a code-built page)
Each difference must be named in tools/caption_diff_excl.tsv (form, object, property, the docs/fidelity.md row that
explains it, e.g. a runtime caption of F19); --check fails on any difference without an exclusion row. Absent rows
are counted, not gated (the captions track decides per row).

W6 LORD_FIDELITY-12: --dfm <FORM.dfm.txt> (repeatable) adds the Caption / Hint / Text rows of a DFM text that the research
table does not carry (the side-exe forms TLJSoundApp, TLJMediaForm, TRS232SettingsForm, extracted by tools/ref/pe_tpf0.py).

usage: caption_diff.py --dump <dump.tsv> [--tsv <13-dfm-captions.tsv>] [--dfm <FORM.dfm.txt>]... [--excl tools/caption_diff_excl.tsv] [--check] [-v]
       caption_diff.py --selftest        (a planted bad caption must fail; no reference data needed)
Python 3.12, stdlib only.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TSV = os.path.join(HERE, "..", "reference", "research", "13-dfm-captions.tsv")   # copied from the design vault's notes/research
DEFAULT_EXCL = os.path.join(HERE, "caption_diff_excl.tsv")
PROPS = ("Caption", "Hint", "Text")


def read_tsv(lines):
    """13-dfm-captions.tsv rows -> {(FORM, object, property): value}"""
    out = {}
    for line in lines:
        p = line.rstrip("\r\n").split("\t")
        if len(p) < 6 or p[0] in ("form", "") or p[0].startswith("#") or p[4] not in PROPS or p[3] == "TAction":
            continue
        out[(p[0].upper(), p[2], p[4])] = p[5]
    return out


def read_dfm(text, form):
    """the Caption / Hint / Text string properties of every object of a DFM text -> {(FORM, object, property): value}"""
    sys.path.insert(0, HERE)
    import dfm2c
    out = {}

    def walk(o):
        for k, v in o.props:
            if k in PROPS and v[0] == "str" and o.cls != "TAction":
                out[(form.upper(), o.name, k)] = v[1].decode("cp1252")
        for c in o.children:
            walk(c)
    walk(dfm2c.parse_dfm(text))
    return out


def read_dump(lines):
    """dump lines -> {(FORM, object, property): value}, forms"""
    out, forms = {}, set()
    for line in lines:
        p = line.rstrip("\r\n").split("\t")
        if len(p) < 4:
            continue
        key = (p[0].upper(), p[1], p[2])
        forms.add(key[0])
        out.setdefault(key, "\t".join(p[3:]))
    return out, forms


def read_excl(lines):
    out = {}
    for line in lines:
        if not line.strip() or line.startswith("#"):
            continue
        p = line.rstrip("\r\n").split("\t")
        if len(p) >= 4 and p[0] != "form":
            out[(p[0].upper(), p[1], p[2])] = p[3]
    return out


def compare(ref, dump, forms, excl):
    """-> (differs [(key, want, got)], excluded [(key, row)], absent [key], per-form counts)"""
    differs, excluded, absent, per = [], [], [], {}
    for key, want in sorted(ref.items()):
        if key[0] not in forms:
            continue
        got = dump.get(key)
        if got is None:
            absent.append(key)
            continue
        if got == want:
            continue
        ex = excl.get(key) or excl.get((key[0], "*", key[2]))
        if ex:
            excluded.append((key, ex))
            continue
        differs.append((key, want, got))
        per[key[0]] = per.get(key[0], 0) + 1
    return differs, excluded, absent, per


def selftest():
    ref = read_tsv(["form\tdfm_line\tobject\tclass\tproperty\tvalue\tin_chm",
                    "TCLOSEORSAVEFORM\t6\tCloseOrSaveForm\tTCloseOrSaveForm\tCaption\tClose Lightjockey ?\tno",
                    "TCLOSEORSAVEFORM\t79\tLabel4\tTLabel\tCaption\tClose LightJockey Now ?\tno",
                    "TCLOSEORSAVEFORM\t80\tacX\tTAction\tCaption\tnever compared\t",
                    "TSAVECUEFORM\t9\tSaveCueForm\tTSaveCueForm\tCaption\tSave Cue\tyes"])
    good, forms = read_dump(["TCloseOrSaveForm\tCloseOrSaveForm\tCaption\tClose Lightjockey ?",
                             "TCloseOrSaveForm\tLabel4\tCaption\tClose LightJockey Now ?"])
    d, e, a, _ = compare(ref, good, forms, {})
    assert not d and not e and not a, (d, e, a)
    bad, forms = read_dump(["TCloseOrSaveForm\tCloseOrSaveForm\tCaption\tClose LightJockey ?",    # planted: the typo "fixed"
                            "TCloseOrSaveForm\tLabel4\tCaption\tClose LightJockey Now ?"])
    d, e, a, per = compare(ref, bad, forms, {})
    assert len(d) == 1 and d[0][0] == ("TCLOSEORSAVEFORM", "CloseOrSaveForm", "Caption") and per == {"TCLOSEORSAVEFORM": 1}, d
    d, e, a, _ = compare(ref, bad, forms, read_excl(["TCLOSEORSAVEFORM\tCloseOrSaveForm\tCaption\tF55 (selftest)"]))
    assert not d and len(e) == 1, (d, e)
    d, e, a, _ = compare(ref, {("TCLOSEORSAVEFORM", "CloseOrSaveForm", "Caption"): "Close Lightjockey ?"}, {"TCLOSEORSAVEFORM"}, {})
    assert a == [("TCLOSEORSAVEFORM", "Label4", "Caption")] and not d, a
    dfm = read_dfm("object F: TF\n  Caption = 'Audio'\n  object L: TLabel\n    Caption = 'Alway'\n    Hint = 'x|y'\n  end\n"
                   "  object A: TAction\n    Caption = 'never'\n  end\nend\n", "TF")
    assert dfm == {("TF", "F", "Caption"): "Audio", ("TF", "L", "Caption"): "Alway", ("TF", "L", "Hint"): "x|y"}, dfm
    print("caption_diff --selftest: OK")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump")
    ap.add_argument("--tsv", default=DEFAULT_TSV)
    ap.add_argument("--dfm", action="append", default=[])
    ap.add_argument("--excl", default=DEFAULT_EXCL)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("-v", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.dump:
        ap.error("--dump is required")
    if not os.path.exists(a.tsv):
        print("caption_diff: SKIP, no %s (developer PC only)" % a.tsv)
        return 0
    ref = read_tsv(open(a.tsv, encoding="cp1252").read().splitlines())
    for p in a.dfm:
        ref.update(read_dfm(open(p, encoding="utf-8").read(), os.path.basename(p).split(".")[0]))
    dump, forms = read_dump(open(a.dump, encoding="cp1252").read().splitlines())
    excl = read_excl(open(a.excl, encoding="cp1252").read().splitlines()) if os.path.exists(a.excl) else {}
    differs, excluded, absent, per = compare(ref, dump, forms, excl)
    ref_forms = {k[0] for k in ref}
    if a.v:
        for key, want, got in differs:
            print("DIFF %s.%s %s: DFM %r, runtime %r" % (key[0], key[1], key[2], want, got))
        for key in absent:
            print("ABSENT %s.%s %s" % key)
    for f in sorted(per):
        print("  %s: %d difference(s)" % (f, per[f]))
    print("caption_diff: %d implemented form(s) of %d, %d difference(s), %d excluded by a fidelity row, %d absent at runtime"
          % (len(forms & ref_forms), len(ref_forms), len(differs), len(excluded), len(absent)))
    return 1 if a.check and differs else 0


if __name__ == "__main__":
    sys.exit(main())
