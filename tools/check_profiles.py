#!/usr/bin/env python3
"""check_profiles.py - every exe row id of a tier-1 family has a hand file (design 6.1 item 2, ME-76,
design 10 W2 acceptance 5 "every 15/02-15/05 id has a hand file (CI check by id)"). Python 3 stdlib.

    python -I tools/check_profiles.py --tier 2 3 4 5     # W2: 15/02 MAC 600, 15/03 MAC 250 family, 15/04 MAC 2000 family, 15/05 Roboscan 518/812/218
    python -I tools/check_profiles.py --tier 6 7 8 9     # W3: 15/06 Atomic 3000, 15/07 MiniMAC, 15/08 RoboColor, 15/09 MAC 550/575/700
    python -I tools/check_profiles.py --tier 1 10        # the W1 families (what exe_profiles_check.py --check already enforces)
    python -I tools/check_profiles.py --hand DIR --tier 2   # check a copy of src/fixlib/hand (the "renamed file fails" proof)

Per id of the tier (15-builtin-profiles-index.md, the note's "profiles (exe ids)" column) a profile initialiser
under src/fixlib/hand/*.c must exist (`lj_profile x = { .id = N, .footprint = F, ...` or `= LJ_MACRO(N, ...`, the
regex tools/exe_profiles_check.py uses), carry the exe row's tag, name and mode literals (from the committed
src/fixlib/lj_exe_profiles.c, so CI needs no reference/), and declare the channel count listed below (the Martin
DMX table / manual footprint of the note; a macro whose footprint is a parameter is not checked). Exit 1 names
every finding; exit 0 prints the per-tier counts. This is the CTest row `profiles_ids`.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exe_profiles_check  # noqa: E402  (HAND, OUT)

# id: footprint (None = not checked). Sources: the note named per tier; PROPOSAL marks a footprint no Martin source gives.
TIERS = {   # 15-builtin-profiles-index.md
    1:  ("15/01 MAC 500 / Roboscan 918", {12: 16, 31: 16, 24: 16, 30: 16}),                    # 15/01 s2: 16 for all four
    2:  ("15/02 MAC 600", {11: 14, 67: 15}),                                                   # DMXTAB-MAC600 V3 Mode 4; RN 2.6 NT = 15
    3:  ("15/03 MAC 250 family", {28: 13, 47: 13, 39: 13, 105: 14, 112: 15, 133: 13, 48: 13,  # UM-MAC250, UM-Kr/En Rev K, UM-Wash Rev G, QLC/OFL
                                  118: 18}),                                                   # 118: PROPOSAL (no protocol source)
    4:  ("15/04 MAC 2000 family", {68: 24, 84: 31, 87: 21, 88: 21, 117: 21, 161: 21, 162: 21,  # DMXTAB-MAC2000Profile V1, UM-Perf Rev E, UM-Wash Rev L
                                   188: 15, 189: 15, 190: 15, 191: 15}),                       # NBD: 21 - 6 barndoor channels (inferred)
    5:  ("15/05 Roboscan 518 / 812 / 218", {3: 9, 58: 9, 1: 5, 52: 7, 2: 8, 57: 8,             # DMXTAB-518 V4, DMXTAB-812 V1, QLC 218
                                            41: 5, 36: 5, 33: 14}),                            # via DMX Interface: PROPOSAL
    6:  ("15/06 Atomic 3000", {80: 4, 114: 6, 115: 2}),                                    # UM-Atomic3000DMX Rev G p.23-24 + QLC colour/fan (scroller at address + 4)
    7:  ("15/07 MiniMAC", {54: 10, 65: 10, 53: 8, 66: 8, 69: 11}),                         # DMXTAB-MiniMACProfile / -Wash V2 mode 4; Maestro: QLC Mode 4
    8:  ("15/08 RoboColor", {13: 5, 59: 6, 14: 5, 60: 6, 15: 5, 61: 7,                    # DMXTAB-RoboColorIII V1 (II inferred); Pro 400: QLC 5 / 7
                             37: 6, 40: 6}),                                               # via DMX Interface: PROPOSAL
    9:  ("15/09 MAC 550 / 575 / 700", {110: 21, 155: 19, 116: 23, 140: 16}),              # UM-MAC550 Rev E, UM-MAC575 Rev C, UM-MAC700P Rev G, UM-MAC700W Rev D (16-bit basic)
    10: ("15/10 generics", dict.fromkeys([19, 70, 56, 71, 21, 72, 23, 20, 73, 22,       # DMX Dimmer 1..32
                                          92, 93, 94, 95, 96, 97, 98, 99, 100, 101,     # DMX Switch Pack 1..32
                                          78, 135, 77, 76, 75,                          # RGB Pack x4 A/B, x6, x8, x10
                                          18, 45, 27, 90, 42, 43, 46])),                # Smoke, Hazer, SwitchBox 6, External, GCS 1, GSC 1, USER2
}

INIT = re.compile(r"lj_profile\s+\w+\s*=\s*(?:\{\s*\.id\s*=\s*(\d+)|(LJ_\w+)\(\s*(\d+))")
FOOT = re.compile(r"\.footprint\s*=\s*(\d+)\b")


def exe_rows():
    """id -> (tag, name, mode) from the committed generated table."""
    text = open(exe_profiles_check.OUT, encoding="latin-1").read()
    rows = {}
    for m in re.finditer(r'^\s*\{\s*(\d+),\s*"((?:[^"\\]|\\.)*)",\s*"(?:[^"\\]|\\.)*",\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)",', text, re.M):
        rows[int(m.group(1))] = (m.group(2), m.group(3), m.group(4))
    return rows


def hand_profiles(hand_dir):
    """id -> list of (file, initialiser text incl. the macro body it expands, footprint or None)."""
    out = {}
    for fn in sorted(os.listdir(hand_dir)):
        if not fn.endswith(".c"):
            continue
        text = open(os.path.join(hand_dir, fn), encoding="latin-1").read()
        defs = {d.group(1): d.group(2) for d in re.finditer(r"^#define\s+(\w+)(?:\([^)]*\))?(.*?)(?<!\\)\n", text, re.M | re.S)}
        for m in INIT.finditer(text):
            end = text.find(";\n", m.end())
            body = text[m.start():end if end >= 0 else len(text)]
            pid = int(m.group(1) or m.group(3))
            seen = set()
            for _ in range(4):                       # expand the file's own macros the initialiser names, a few levels deep
                more = [n for n in re.findall(r"\bLJ_\w+", body) if n in defs and n not in seen]
                if not more:
                    break
                for n in more:
                    seen.add(n)
                    body += "\n" + defs[n]
            f = FOOT.search(body)
            out.setdefault(pid, []).append((fn, body, int(f.group(1)) if f else None))
    return out


def cstr(s):
    return '"' + s + '"'


def main(argv):
    hand_dir = exe_profiles_check.HAND
    args = argv[1:]
    if len(args) >= 2 and args[0] == "--hand":
        hand_dir, args = args[1], args[2:]
    if len(args) < 2 or args[0] != "--tier":
        print(__doc__)
        return 2
    tiers = [int(a) for a in args[1:]]
    hand, rows = hand_profiles(hand_dir), exe_rows()
    bad = 0
    for t in tiers:
        name, ids = TIERS[t]
        ok = 0
        for i, want in ids.items():
            found = hand.get(i)
            if not found:
                print("%s: exe id %d has no hand file under %s" % (name, i, hand_dir)); bad += 1; continue
            if len(found) > 1:
                print("%s: exe id %d has %d hand profiles (%s)" % (name, i, len(found), ", ".join(f for f, _, _ in found))); bad += 1; continue
            fn, body, foot = found[0]
            tag, pname, mode = rows.get(i, (None, None, None))
            miss = [w for w, v in (("tag", tag), ("name", pname), ("mode", mode)) if v is not None and v != "" and cstr(v) not in body]
            if pname is None:
                print("%s: exe id %d is not a row of lj_exe_profiles.c" % (name, i)); bad += 1; continue
            if miss:
                print("%s: %s id %d does not carry the exe %s (%s)" % (name, fn, i, "/".join(miss), " / ".join((tag, pname, mode)))); bad += 1; continue
            if want is not None and foot != want:
                print("%s: %s id %d footprint %s, the note gives %d" % (name, fn, i, foot, want)); bad += 1; continue
            ok += 1
        print("check_profiles: tier %d %s: %d of %d ids have a matching hand file" % (t, name, ok, len(ids)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
