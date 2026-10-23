#!/usr/bin/env python3
"""fidelity_check.py - gate W4-FID (ME-138): docs/fidelity.md is complete.

Fails (exit 1, one line per finding) when
  - an id F1..F60 or I1..I13 has no table row whose second cell is non-empty (`| F17 | <pair or deferral> |`);
  - the "What v1.0.0 is not" or the "What v1.1.0 is not" section differs byte for byte from README.md's (design 1,
    the sacrifice list; W5-X-SACRIFICE keeps the 1.0.0 paragraph and adds the 1.1.0 one);
  - a capture the file names (`E/<x>.png` = docs/screenshots/w4/entry/<x>.png, or `docs/screenshots/...png`) is missing.
usage: fidelity_check.py [--repo <dir>]          fidelity_check.py --selftest
Python 3, stdlib only.
"""
import argparse
import os
import re
import sys

ROW = re.compile(r'^\|\s*([FI])(\d+)\b[^|]*\|\s*([^|]*?)\s*\|', re.M)
CAPTURE = re.compile(r'`((?:E/|docs/screenshots/)[^`*]+?\.png)`')


def section(text, title):
    """The body under '## <title>' up to the next '## ' heading (a '### ' heading also ends it), or None."""
    m = re.search(r'^## %s\s*$' % re.escape(title), text, re.M)
    if not m:
        return None
    rest = text[m.end():]
    e = re.search(r'^##+ ', rest, re.M)
    return (rest[:e.start()] if e else rest).strip('\r\n')


def check(fid, readme, exists):
    out = []
    have = {}
    for kind, n, cell in ROW.findall(fid):
        if cell:
            have[(kind, int(n))] = 1
    for kind, top in (('F', 60), ('I', 13)):
        for n in range(1, top + 1):
            if (kind, n) not in have:
                out.append('%s%d: no row with a pair or a deferral' % (kind, n))
    for title in ('What v1.0.0 is not', 'What v1.1.0 is not'):   # W5-X-SACRIFICE: one paragraph per version, the old one kept
        a, b = section(readme, title), section(fid, title)
        if a is None:
            out.append('README.md: no "## %s" section' % title)
        elif b != a:
            out.append('docs/fidelity.md: the "%s" section is %s README.md\'s' % (title, 'missing from' if b is None else 'not byte-equal to'))
    for c in sorted(set(CAPTURE.findall(fid))):
        p = 'docs/screenshots/w4/entry/' + c[2:] if c.startswith('E/') else c
        if not exists(p):
            out.append('capture missing: %s' % p)
    return out, len(have)


def selftest():
    readme = '# x\n\n## What v1.0.0 is not\n\nNo audio.\n\n## What v1.1.0 is not\n\nNo rig.\n\n## Docs\n'
    rows = ''.join('| F%d | `E/a.png` | done |\n' % n for n in range(1, 61)) + ''.join('| I%d | x | W5 |\n' % n for n in range(1, 14))
    good = rows + '\n## What v1.0.0 is not\n\nNo audio.\n\n### next\n\n## What v1.1.0 is not\n\nNo rig.\n'
    assert check(good, readme, lambda p: True)[0] == [], check(good, readme, lambda p: True)
    bad = good.replace('| F17 | `E/a.png` |', '| F17 |  |').replace('No audio.', 'No audio')
    errs = check(bad, readme, lambda p: False)[0]
    assert any(e.startswith('F17:') for e in errs) and any('byte-equal' in e for e in errs) and any('capture missing' in e for e in errs), errs
    errs = check(good.replace('No rig.', 'No rig'), readme, lambda p: True)[0]
    assert errs == ['docs/fidelity.md: the "What v1.1.0 is not" section is not byte-equal to README.md\'s'], errs
    print('fidelity_check selftest: passed')
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    rd = lambda p: open(os.path.join(a.repo, p), encoding='cp1252', newline='').read().replace('\r\n', '\n')
    errs, n = check(rd('docs/fidelity.md'), rd('README.md'), lambda p: os.path.exists(os.path.join(a.repo, p)))
    for e in errs:
        print(e)
    print('fidelity_check: %d F/I id(s) with a row of 73, sacrifice paragraphs %s, %d finding(s)'
          % (n, 'equal' if not any(' is not' in e for e in errs) else 'DIFFER', len(errs)))
    return 1 if errs else 0


if __name__ == '__main__':
    sys.exit(main())
