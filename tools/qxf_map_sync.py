#!/usr/bin/env python3
"""qxf_map_sync.py - append a tools/qxf_map.tsv line for every source mode not mapped yet (W7, docs/fixtures.md).
Python 3 stdlib only.

    python -I tools/qxf_map_sync.py           # append the new lines (existing lines are never touched)
    python -I tools/qxf_map_sync.py --check   # CI: exit 1 when a sync would append anything
    python -I tools/qxf_map_sync.py --selftest

Sources, in precedence order (qxf2c.all_sources): fixtures/authored (LJ4EVR), third_party/ofl-fixtures (Open
Fixture Library), third_party/qlcplus-fixtures (QLC+); only the 14 W7 groups (qxf2c.GROUPS). Precedence and dedup
(hand > authored > OFL > QLC+) work per (group, model, footprint): model = the source's Model, lower case, letters and
digits only, the group's name dropped from its front; a mode is skipped when a source of the same or higher precedence
(another file, an existing map line or a hand profile of the exe table) already owns that key. Modes wider than 32
channels are never mapped (tools/fixture_coverage.py lists them). New ids start at the next free id >= 1000 and are
given in sorted order of (manufacturer, model, mode, file), so a sync is deterministic; an id is never reused.
"""
import datetime
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qxf2c  # noqa: E402
import check_profiles  # noqa: E402

RANK = {"hand": 0, "authored": 1, "ofl": 2, "qlc": 3}


def model_key(group, model):
    m = qxf2c.alnum(model)
    for word in (qxf2c.alnum(group), qxf2c.alnum(group.split()[0])):
        if m.startswith(word) and len(m) > len(word):
            m = m[len(word):]
    return m


def scan(sources=None):
    """-> [(file, group, model, [(mode, channel count)])] for every source of a W7 group, precedence order."""
    out, problems = [], []
    for fname in sources if sources is not None else qxf2c.all_sources():
        try:
            q = qxf2c.load_source(fname, problems)
        except Exception as e:                                  # a broken upstream file is reported, never fatal
            problems.append("%s: %s" % (fname, e))
            continue
        g = qxf2c.manufacturer_of(q, fname)
        if g is None:
            continue
        out.append((fname, g, q["model"], [(m, len(q["modes"][m])) for m in q["mode_order"] if m in q["modes"]]))
    return out, problems


def plan(rows, scanned, exe, hand_fp):
    """-> new (file, mode, manufacturer, model) lines, unsorted. rows = the existing map lines."""
    mapped = {(f, m) for f, m, _ in rows}
    owner = {}                                                  # (group, model key, footprint) -> (rank, file)
    info = {f: (g, mdl, dict(ms)) for f, g, mdl, ms in scanned}
    for f, m, pid in rows:
        if f in info and m in info[f][2]:
            g, mdl, ms = info[f]
            owner.setdefault((g, model_key(g, mdl), ms[m]), (RANK[qxf2c.source_kind(f)], f))
    for pid, fp in hand_fp.items():
        if pid in exe:
            _, man, name, _ = exe[pid]
            g = qxf2c.group_of(man)
            if g:
                owner[(g, model_key(g, name), fp)] = (RANK["hand"], "hand")
    new = []
    for f, g, mdl, ms in scanned:
        rank = RANK[qxf2c.source_kind(f)]
        for m, n in ms:
            if (f, m) in mapped or n > qxf2c.CH or n < 1 or "\t" in m or "\n" in m:
                continue
            key = (g, model_key(g, mdl), n)
            o = owner.get(key)
            if o and o[1] != f and o[0] <= rank:
                continue                                        # an equal or higher source already owns this fixture + footprint
            owner[key] = (rank, f)
            new.append((f, m, g, mdl))
    return new


def sync(check=False):
    rows = qxf2c.read_map()
    scanned, problems = scan()
    hand_fp = {}
    for _, (_, ids) in check_profiles.TIERS.items():
        hand_fp.update({i: fp for i, fp in ids.items() if fp})
    new = plan(rows, scanned, qxf2c.exe_rows(), hand_fp)
    new.sort(key=lambda r: (r[2].lower(), r[3].lower(), r[1].lower(), r[0]))
    nxt = max([qxf2c.EXTRA - 1] + [pid for _, _, pid in rows]) + 1
    lines = ["%s\t%s\t%d" % (f, m, nxt + i) for i, (f, m, _, _) in enumerate(new)]
    for p in problems:
        print("note: %s" % p)
    if check:
        for ln in lines[:20]:
            print("unmapped: " + ln)
        print("qxf_map_sync --check: %d map line(s), %d unmapped mode(s) <= %d channels" % (len(rows), len(lines), qxf2c.CH))
        return 1 if lines else 0
    if lines:
        with open(qxf2c.MAP, "a", encoding="utf-8", newline="\n") as f:
            f.write("# W7 tools/qxf_map_sync.py %s: %d lines, ids %d-%d (append-only; precedence authored > OFL > QLC+)\n"
                    % (datetime.date.today().isoformat(), len(lines), nxt, nxt + len(lines) - 1))
            f.write("\n".join(lines) + "\n")
    print("qxf_map_sync: %d line(s) appended, %d map line(s) now" % (len(lines), len(rows) + len(lines)))
    return 0


def selftest():
    exe = {12: ("MAC500", "Martin", "MAC 500", "Mode 2/4")}
    scanned = [("fixtures/authored/martin/a.qxf", "Martin", "MAC 700 Wash", [("Basic", 16)]),
               ("third_party/ofl-fixtures/martin/mac-500.json", "Martin", "MAC 500", [("Mode 4", 16), ("Mode 2", 14)]),
               ("third_party/ofl-fixtures/martin/mac-700-wash.json", "Martin", "MAC 700 Wash", [("16-bit", 16), ("8-bit", 12)]),
               ("Martin-MAC-700-Wash.qxf", "Martin", "Martin MAC 700 Wash", [("16-bit Basic", 16), ("Ext", 40), ("8 bit", 12), ("Mini", 3)]),
               ("Martin-Other.qxf", "Martin", "Other", [("A", 5)])]
    rows = [("Martin-Other.qxf", "A", 1000)]
    new = plan(rows, scanned, exe, {12: 16})
    got = [(f.split("/")[-1], m) for f, m, _, _ in new]
    assert got == [("a.qxf", "Basic"), ("mac-500.json", "Mode 2"), ("mac-700-wash.json", "8-bit"), ("Martin-MAC-700-Wash.qxf", "Mini")], got
    assert model_key("Martin", "Martin MAC 700 Wash") == model_key("Martin", "MAC 700 Wash") == "mac700wash"
    assert model_key("Chauvet DJ", "Chauvet Hive") == "hive" and model_key("ETC", "ETCetera") == "etera"
    print("qxf_map_sync selftest: ok (hand footprint wins, authored over OFL over QLC+, > 32 skipped, mapped lines kept)")
    return 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    sys.exit(sync("--check" in sys.argv))
