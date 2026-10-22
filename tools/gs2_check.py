#!/usr/bin/env python3
"""gs2_check.py - our .GS2 / .Udf / .Udo importer against the oracle (W3 gates W3-ACC-5, ME-9, ME-33).

For every .GS2 / .Udf / .Udo under --ref (the Martin reference folder, developer PC only) this runs
`lj4evr_tests gs2_dump <file>` and compares its flat field list, line by line, with the same list built from
tools/ref/16-gs2_ref.py's parse() (the in-repo copy of the vault oracle; nothing is imported from the vault), plus
the round trip: ours must print "roundtrip ok" and the oracle's serialize(parse(x)) must equal x. One line per file:

    OK   <file>
    DIFF <file>: <first differing field> ours=<...> oracle=<...> (+N more)

Exit 0 when every file matches, 1 on any difference. With --ref empty or missing it prints a SKIP line and exits 0.
The files are read as data and never executed; run with python -I.

    python -I tools/gs2_check.py --tests build/lj4evr_tests.exe --ref D:/lj4evr/reference
"""
import argparse, importlib.util, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('gs2_ref', os.path.join(HERE, 'ref', '16-gs2_ref.py'))
ref = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ref)

EXTS = ('.gs2', '.udf', '.udo')
LEGACY_MANUFACTURER = 'User Definable'   # ME-33 (lj_gs2.h LJ_GS2_LEGACY_MANUFACTURER)


def esc(b):
    """The bytes of a C string as gs2_dump prints them: printable ASCII except backslash literal, the rest \\xhh."""
    b = b.split(b'\0', 1)[0]
    return ''.join(chr(c) if 0x20 <= c < 0x7F and c != 0x5C else '\\x%02x' % c for c in b)


def s(v, n):
    """ShortString[n] text as lj_gs2.c reads it: min(len byte, n) chars, cut at the first NUL."""
    raw = v['raw']
    return esc(raw[1:1 + min(raw[0], n)])


def oracle_lines(p, roundtrip_ok):
    L = []
    add = L.append
    ver = p['format_version']
    add('format_version %d' % ver)
    add('footprint %d' % (p['last_offset'] + 1))
    if ver == 2609:
        add('manufacturer ' + s(p['manufacturer'], 30)); add('name ' + s(p['fixture_name'], 30)); add('mode ' + s(p['fixture_mode'], 30))
    else:                                                     # ME-33, PROPOSAL 16 s10
        add('manufacturer ' + LEGACY_MANUFACTURER); add('name ' + s(p['legacy_name'], 30)); add('mode ')
    add('fixture_id ' + s(p['fixture_id'], 15)); add('author ' + s(p['author'], 30)); add('icon_caption ' + s(p['icon_caption'], 10))
    add('icon_bitmap ' + s(p['icon_bitmap'], 36))
    add('transparent %d' % p['transparent'])
    add('notes ' + esc(p['notes_rtf']['raw']))
    bf = p['beam_fx']
    dyns = [('shutter', p['shutter']), ('strobe', p['strobe']), ('intensity', p['intensity']), ('misc_move', p['misc_move'])]
    dyns += [('color%d' % (i + 1), p['color']['color%d' % (i + 1)]) for i in range(4)]
    dyns += [(k, p['cmyrgb'][k]) for k in ('cyan_red', 'magenta_green', 'yellow_blue')]
    dyns += [('gobo%d' % (i + 1), p['gobo']['gobo%d' % (i + 1)]) for i in range(4)]
    dyns += [(k, bf[k]) for k in ('iris', 'focus', 'zoom', 'frost', 'beam1', 'beam2', 'effects1', 'effects2')]
    dyns += [('effects3', p['effects3']), ('effects4', p['effects4'])]
    for k, d in dyns:
        add('%s %d %d %d %d' % (k, d['enable'], d['offset'], d['min'], d['max']))
        add('%s.name %s' % (k, s(d['name'], 8)))
    add('strobe_off %d' % p['strobe_off']); add('use_gm %d' % p['use_gm']); add('blackout_on_gm0 %d' % p['blackout_on_gm0'])
    add('invert_int_chan %d' % p['invert_int_chan']); add('invert_int_range %d' % p['invert_int_range']); add('limit_gm %d' % p['limit_gm'])
    arr = lambda k, a: add(k + ''.join(' %d' % x for x in a))
    arr('ext_gm', p['ext_gm']); arr('default_dmx', p['default_dmx'])
    stats = [('blackout', p['blackout']), ('lamp_on', p['lamp_on']), ('lamp_off', p['lamp_off']), ('reset', p['reset'])]
    stats += [('special%d' % (i + 1), r) for i, r in enumerate(p['specials'])]
    for k, r in stats:
        add('%s.name %s' % (k, s(r['name'], 15))); add('%s.enabled %d' % (k, r['enable']))
        arr(k + '.defined', r['defined']); arr(k + '.values', r['values'])
    for k in ('pt16', 'pt8', 'pan_offset', 'tilt_offset', 'pt_patch'): add('%s %d' % (k, p[k]))
    arr('patch', [p['patch_pan_msb'], p['patch_pan_lsb'], p['patch_tilt_msb'], p['patch_tilt_lsb']])
    for k in ('presets', 'macros', 'follow_spot', 'cmy', 'rgb', 'cmy_picker', 'invert_cmyrgb'): add('%s %d' % (k, p[k]))
    arr('hsv', [p['rgb_hue'], p['rgb_sat'], p['rgb_val']])
    for i, l in enumerate(p['levels']):
        add('level%d %s' % (i, s(l['caption'], 5))); add('level%d.flags %d %d' % (i, l['invisible'], l['disabled']))
    add('lamp_on_time_ms %d' % p['lamp_on_time_ms']); add('reset_time_ms %d' % p['reset_time_ms'])
    types = p['pal_types'] + p['fx_pal2_types']
    for i, k in enumerate(('color_pal1', 'color_pal2', 'gobo_pal1', 'gobo_pal2', 'fx_pal1', 'fx_pal2')):
        arr('pal%d.types' % i, types[5 * i:5 * i + 5])
        for j, e in enumerate(p[k]):
            r, g, b = e['rgb']
            add('pal%d.%d %d %d %s %06X' % (i, j, e['used'], e['defined_mask'], ' '.join('%d' % v for v in e['values']), r << 16 | g << 8 | b))
            add('pal%d.%d.bitmap %s' % (i, j, s(e['bitmap'], 30)))
    add('roundtrip ' + ('ok' if roundtrip_ok else 'FAIL'))
    return L


def check(tests, path):
    try:
        raw = ref.load(path)
        want = oracle_lines(ref.parse(raw), ref.serialize(ref.parse(raw)) == raw)
    except (AssertionError, Exception) as e:  # the oracle refuses: ours must refuse with the same message
        want = ['error %s' % e]
    got = subprocess.run([tests, 'gs2_dump', path], capture_output=True).stdout.decode('cp1252').splitlines()
    diffs = [(i, a, b) for i, (a, b) in enumerate(zip(got, want)) if a != b]
    if len(got) != len(want): diffs.append((min(len(got), len(want)), '<%d lines>' % len(got), '<%d lines>' % len(want)))
    if not diffs: return 'OK   %s' % path, 0
    i, a, b = diffs[0]
    return 'DIFF %s: line %d ours=%r oracle=%r (+%d more)' % (path, i + 1, a[:120], b[:120], len(diffs) - 1), 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tests', required=True, help='path to lj4evr_tests')
    ap.add_argument('--ref', default='', help='LJ4EVR_REFERENCE_DIR (empty = SKIP)')
    ap.add_argument('files', nargs='*', help='explicit files instead of walking --ref')
    a = ap.parse_args()
    files = a.files
    if not files:
        if not a.ref or not os.path.isdir(a.ref):
            print('SKIP: LJ4EVR_REFERENCE_DIR unset'); return 0
        files = sorted(os.path.join(d, f) for d, _, fs in os.walk(a.ref) for f in fs if f.lower().endswith(EXTS))
    bad = 0
    for f in files:
        line, b = check(a.tests, f)
        print(line); bad += b
    print('%d files, %d with differences' % (len(files), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
