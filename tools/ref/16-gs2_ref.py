#!/usr/bin/env python3
"""16-gs2_ref.py - reference decoder for LightJockey 2 Generic Fixture 2 profiles.

Parses a .GS2 (20,851-byte Delphi record) or the stream-0 payload of a .Udf/.Udo
export into the eleven-tab field set of the "Generic Fixture Profile" dialog,
and serialises it back byte-identically.  Python 3 stdlib only; run with -I and
pass file paths as arguments (the files are untrusted data, never executed).

    python -I 16-gs2_ref.py dump  <file.GS2|file.Udf> [...]
    python -I 16-gs2_ref.py roundtrip <file ...>          # exit 1 on any mismatch
    python -I 16-gs2_ref.py stats <LJ_Profiles.zip|dir> [N]  # library statistics

Layout evidence: 16-gap-gs2-format-decode.md (same folder).  Offsets are from
differential analysis of the 48 shipped profiles + 200 library exports; field
labels are the TScanner2DefsForm control names in LightJockey.exe 2.209.
"""
import glob, os, random, struct, sys, zipfile, zlib
from collections import Counter

SIZE = 20851

# ---- record shapes -----------------------------------------------------------
# ShortString[n] = 1 length byte + n chars; bytes past the length are stale and
# must be kept for a byte-identical round trip (Delphi never clears them).

def sstr(buf, off, n):
    ln = min(buf[off], n)
    return {'text': buf[off + 1:off + 1 + ln].decode('cp1252', 'replace'), 'raw': bytes(buf[off:off + n + 1])}

def put_sstr(out, off, n, v):
    out[off:off + n + 1] = v['raw']  # text edits: rebuild raw yourself (len byte + chars + stale tail)

def dyn(buf, off):       # 17-byte dynamic function record
    return {'enable': buf[off], 'offset': buf[off + 1], 'name': sstr(buf, off + 2, 8),
            'min': buf[off + 11], 'max': buf[off + 12], 'reserved': bytes(buf[off + 13:off + 17])}

def put_dyn(out, off, r):
    out[off] = r['enable']; out[off + 1] = r['offset']; put_sstr(out, off + 2, 8, r['name'])
    out[off + 11] = r['min']; out[off + 12] = r['max']; out[off + 13:off + 17] = r['reserved']

def static(buf, off):    # 87-byte static function record (name only used by specials)
    return {'name': sstr(buf, off, 15), 'enable': buf[off + 16],
            'defined': list(buf[off + 17:off + 49]), 'values': list(buf[off + 49:off + 81]),
            'reserved': bytes(buf[off + 81:off + 87])}

def put_static(out, off, r):
    put_sstr(out, off, 15, r['name']); out[off + 16] = r['enable']
    out[off + 17:off + 49] = bytes(r['defined']); out[off + 49:off + 81] = bytes(r['values'])
    out[off + 81:off + 87] = r['reserved']

def pal(buf, off):       # 57-byte palette entry
    return {'used': buf[off], 'defined_mask': buf[off + 1], 'values': list(buf[off + 2:off + 7]),
            'rgb': (buf[off + 7], buf[off + 8], buf[off + 9]), 'rgb_pad': buf[off + 10],
            'bitmap': sstr(buf, off + 11, 30), 'reserved': bytes(buf[off + 42:off + 57])}

def put_pal(out, off, r):
    out[off] = r['used']; out[off + 1] = r['defined_mask']; out[off + 2:off + 7] = bytes(r['values'])
    out[off + 7:off + 10] = bytes(r['rgb']); out[off + 10] = r['rgb_pad']
    put_sstr(out, off + 11, 30, r['bitmap']); out[off + 42:off + 57] = r['reserved']

def level(buf, off):     # 16-byte level record
    return {'caption': sstr(buf, off, 5), 'invisible': buf[off + 6], 'disabled': buf[off + 7],
            'reserved': bytes(buf[off + 8:off + 16])}

def put_level(out, off, r):
    put_sstr(out, off, 5, r['caption']); out[off + 6] = r['invisible']; out[off + 7] = r['disabled']
    out[off + 8:off + 16] = r['reserved']

PALETTE_TYPES = {0: 'Def', 1: 'Alt1', 2: 'Alt2', 3: 'Alt3', 4: 'Alt4', 5: 'Idx', 6: 'Rot', 7: 'CW',
                 8: 'CCW', 9: 'B/O', 10: 'Shk', 11: 'Spd', 12: 'Fix', 13: 'Spl', 14: 'RdS',
                 15: 'RdM', 16: 'RdF', 17: 'Color', 18: 'Stop'}  # LightJockey.exe case table 0..18

# (name, offset, kind, extra) - kinds: u8, u8s (array), i32, s (ShortString[n]), chars (NUL-term buffer),
# dyn, dyns, static, statics, pals, levels, raw
LAYOUT = [
    ('format_version',   0x0000, 'u16', 2),        # 2609 (bytes '1',LF) on 2.6b9+ profiles; 0 or 1 on old ones
    ('zero2',            0x0002, 'raw', 2),
    ('const04',          0x0004, 'u8', 1),         # 0x2E in every sample
    ('last_offset',      0x0005, 'u8', 1),         # channel count - 1 ("Fixture Uses N Channels")
    ('zero6',            0x0006, 'raw', 3),
    ('legacy_name',      0x0009, 's', 30),         # pre-2.6b9 combined manufacturer/model/mode
    ('icon_caption',     0x0028, 's', 10),         # eIconCaption
    ('fixture_id',       0x0033, 's', 15),         # eSelectionId  "Fixture ID"
    ('author',           0x0043, 's', 30),         # eAuthor
    ('notes_rtf',        0x0062, 'chars', 4097),   # Notes (RTF, NUL-terminated, max 4096)
    ('icon_bitmap',      0x1063, 's', 36),         # lBitmapName
    ('default_dmx',      0x1088, 'u8s', 32),       # lvDefaultDMX (Specials tab)
    ('transparent',      0x10A8, 'u8', 1),         # cbTransparent (probable, see note)
    ('reserved_10A9',    0x10A9, 'raw', 202),
    ('shutter',          0x1173, 'dyn', 0),        # min=Closed max=Open
    ('strobe',           0x1184, 'dyn', 0),        # min=Slow max=Fast
    ('strobe_off',       0x1195, 'u8', 1),         # eStrobeOff
    ('intensity',        0x1196, 'dyn', 0),
    ('invert_int_chan',  0x11A7, 'u8', 1),         # cbInvertInt 'Invert entire Intensity channel'
    ('use_gm',           0x11A8, 'u8', 1),         # cbUseGM
    ('blackout_on_gm0',  0x11A9, 'u8', 1),         # cbBOGM
    ('blackout',         0x11AA, 'static', 0),     # lvBO (enable byte is 1 in every sample)
    ('lamp_on',          0x1201, 'static', 0),     # cbLampOn + lvLampOn
    ('lamp_off',         0x1258, 'static', 0),     # cbLampOff + lvLampOff
    ('reset',            0x12AF, 'static', 0),     # cbReset + lvReset (Specials tab)
    ('pt16',             0x1306, 'u8', 1),         # cbPanTilt16
    ('pt8',              0x1307, 'u8', 1),         # cbPanTilt8
    ('pan_offset',       0x1308, 'u8', 1),
    ('tilt_offset',      0x1309, 'u8', 1),
    ('pt_patch',         0x130A, 'u8', 1),         # cbPanTiltPatch
    ('patch_pan_msb',    0x130B, 'u8', 1),
    ('patch_pan_lsb',    0x130C, 'u8', 1),
    ('patch_tilt_msb',   0x130D, 'u8', 1),
    ('patch_tilt_lsb',   0x130E, 'u8', 1),
    ('presets',          0x130F, 'u8', 1),         # cbPresets
    ('macros',           0x1310, 'u8', 1),         # cbMacros
    ('misc_move',        0x1311, 'dyn', 0),        # cbMove1 / eMove1*
    ('color',            0x1322, 'dyns', 4),       # Color 1..4
    ('reserved_1366',    0x1366, 'raw', 17),
    ('color_pal1',       0x1377, 'pals', 20),
    ('color_pal2',       0x17EB, 'pals', 20),
    ('cmy',              0x1C5F, 'u8', 1),         # cbCMY
    ('rgb',              0x1C60, 'u8', 1),         # cbRGB
    ('cmy_picker',       0x1C61, 'u8', 1),         # cbCMYPicker
    ('invert_cmyrgb',    0x1C62, 'u8', 1),         # cbInvertPicker 'Invert CMY/RGB Values' (probable)
    ('cmyrgb',           0x1C63, 'dyns', 3),       # Cyan/Red, Magenta/Green, Yellow/Blue; min=No Color max=Full
    ('reserved_1C96',    0x1C96, 'raw', 17),
    ('gobo',             0x1CA7, 'dyns', 4),       # Gobo 1..4
    ('reserved_1CEB',    0x1CEB, 'raw', 17),
    ('gobo_pal1',        0x1CFC, 'pals', 20),
    ('gobo_pal2',        0x2170, 'pals', 20),
    ('beam_fx',          0x25E4, 'dyns', 8),       # iris, focus, zoom, frost, beam1, beam2, effects1, effects2
    ('fx_pal1',          0x267D, 'pals', 20),
    ('specials',         0x2AF1, 'statics', 32),   # Special 1..32 (name = Function Name)
    ('levels',           0x35D1, 'levels', 32),
    ('pal_types',        0x37D1, 'u8s', 25),       # 5 type codes x (color1, color2, gobo1, gobo2, fx1)
    ('follow_spot',      0x37EA, 'u8', 1),         # cbFollow
    ('fx_pal2',          0x37EB, 'pals', 20),
    ('fx_pal2_types',    0x3C5F, 'u8s', 5),
    ('effects3',         0x3C64, 'dyn', 0),
    ('effects4',         0x3C75, 'dyn', 0),
    ('ov_manufacturer',  0x3C86, 's', 64),         # Offline Visualizer tab eMan
    ('ov_model',         0x3CC7, 's', 64),         # eModel
    ('ov_mode',          0x3D08, 's', 64),         # eMode
    ('reset_time_ms',    0x3D49, 'i32', 4),        # eResetTime "Minimum Reset Time"
    ('lamp_on_time_ms',  0x3D4D, 'i32', 4),        # eLampOnTime "Auto LampOn Time"
    ('invert_int_range', 0x3D51, 'u8', 1),         # cbInvertIntVal 'Invert Intensity value range' (probable)
    ('limit_gm',         0x3D52, 'u8', 1),         # cbLimitGM 'Limit GM to intensity range' (probable)
    ('source_file',      0x3D53, 's', 64),         # file name suggested by Save Profile (>=48 wide; 64 assumed)
    ('reserved_3D94',    0x3D94, 'raw', 128),
    ('ext_gm',           0x3E14, 'u8s', 32),       # lvGMControl, 1 = Enable GM on that offset
    ('rgb_hue',          0x3E34, 'u8', 1),         # cbRGBHue
    ('rgb_sat',          0x3E35, 'u8', 1),         # cbRGBSat
    ('rgb_val',          0x3E36, 'u8', 1),         # cbRGBVal
    ('fixture_name',     0x3E37, 's', 30),         # eFixtureName
    ('manufacturer',     0x3E56, 's', 30),         # cbManufacturer
    ('fixture_mode',     0x3E75, 's', 30),         # eFixtureMode
    ('reserved_3E94',    0x3E94, 'raw', 4831),
]
DYN_NAMES = {'color': ['color1', 'color2', 'color3', 'color4'], 'cmyrgb': ['cyan_red', 'magenta_green', 'yellow_blue'],
             'gobo': ['gobo1', 'gobo2', 'gobo3', 'gobo4'],
             'beam_fx': ['iris', 'focus', 'zoom', 'frost', 'beam1', 'beam2', 'effects1', 'effects2']}
SIZES = {'dyn': 17, 'static': 87, 'pal': 57, 'level': 16}


def parse(buf):
    assert len(buf) == SIZE, 'not a GS2 record: %d bytes' % len(buf)
    p = {}
    for name, off, kind, n in LAYOUT:
        if kind == 'u8': p[name] = buf[off]
        elif kind == 'u16': p[name] = struct.unpack_from('<H', buf, off)[0]
        elif kind == 'u8s': p[name] = list(buf[off:off + n])
        elif kind == 'i32': p[name] = struct.unpack_from('<i', buf, off)[0]
        elif kind == 's': p[name] = sstr(buf, off, n)
        elif kind == 'chars':
            raw = bytes(buf[off:off + n]); p[name] = {'text': raw.split(b'\0', 1)[0].decode('cp1252', 'replace'), 'raw': raw}
        elif kind == 'raw': p[name] = bytes(buf[off:off + n])
        elif kind == 'dyn': p[name] = dyn(buf, off)
        elif kind == 'dyns': p[name] = {k: dyn(buf, off + 17 * i) for i, k in enumerate(DYN_NAMES[name])}
        elif kind == 'static': p[name] = static(buf, off)
        elif kind == 'statics': p[name] = [static(buf, off + 87 * i) for i in range(n)]
        elif kind == 'pals': p[name] = [pal(buf, off + 57 * i) for i in range(n)]
        elif kind == 'levels': p[name] = [level(buf, off + 16 * i) for i in range(n)]
    return p


def serialize(p):
    out = bytearray(SIZE)
    for name, off, kind, n in LAYOUT:
        v = p[name]
        if kind == 'u8': out[off] = v
        elif kind == 'u16': struct.pack_into('<H', out, off, v)
        elif kind == 'u8s': out[off:off + n] = bytes(v)
        elif kind == 'i32': struct.pack_into('<i', out, off, v)
        elif kind == 's': put_sstr(out, off, n, v)
        elif kind in ('chars', 'raw'): out[off:off + n] = v['raw'] if kind == 'chars' else v
        elif kind == 'dyn': put_dyn(out, off, v)
        elif kind == 'dyns':
            for i, k in enumerate(DYN_NAMES[name]): put_dyn(out, off + 17 * i, v[k])
        elif kind == 'static': put_static(out, off, v)
        elif kind == 'statics':
            for i, r in enumerate(v): put_static(out, off + 87 * i, r)
        elif kind == 'pals':
            for i, r in enumerate(v): put_pal(out, off + 57 * i, r)
        elif kind == 'levels':
            for i, r in enumerate(v): put_level(out, off + 16 * i, r)
    return bytes(out)


# ---- .Udf/.Udo container -----------------------------------------------------
def udf_streams(data):
    """Back-to-back zlib streams, then a 36-byte footer [crc32][file count][1][table size][data size][1][12 zero]."""
    body, foot = data[:-36], data[-36:]
    assert struct.unpack_from('<I', foot)[0] == zlib.crc32(body), 'footer CRC mismatch'
    streams, pos = [], 0
    while pos < len(body):
        d = zlib.decompressobj(15)
        streams.append(d.decompress(body[pos:]))
        pos = len(body) - len(d.unused_data)
    return streams


def load(path_or_bytes, name=''):
    data = path_or_bytes if isinstance(path_or_bytes, bytes) else open(path_or_bytes, 'rb').read()
    n = (name or str(path_or_bytes)).lower()
    if n.endswith(('.udf', '.udo')): return udf_streams(data)[0]
    return data


# ---- reporting ---------------------------------------------------------------
def fmt_dyn(r, lo='min', hi='max'):
    if not r['enable']: return '-'
    s = 'ofs %d %s %d %s %d' % (r['offset'], lo, r['min'], hi, r['max'])
    return s + (' "%s"' % r['name']['text'] if r['name']['text'] else '')

def fmt_static(r):
    if not r['enable']: return '-'
    return ' '.join('%d=%d' % (i, r['values'][i]) for i in range(32) if r['defined'][i]) or '(no values)'

def dump(p):
    s = lambda k: p[k]['text']
    print('Fixture Info: manufacturer=%r name=%r mode=%r id=%r author=%r caption=%r icon=%r legacy=%r file=%r channels=%d version=%d transparent=%d'
          % (s('manufacturer'), s('fixture_name'), s('fixture_mode'), s('fixture_id'), s('author'), s('icon_caption'),
             s('icon_bitmap'), s('legacy_name'), s('source_file'), p['last_offset'] + 1, p['format_version'], p['transparent']))
    print('  notes: %d chars RTF' % len(p['notes_rtf']['text']))
    print('Intensity & Lamp: shutter[%s] strobe[%s off=%d] intensity[%s] useGM=%d BOGM=%d invert_chan=%d invert_range=%d limit_gm=%d'
          % (fmt_dyn(p['shutter'], 'closed', 'open'), fmt_dyn(p['strobe'], 'slow', 'fast'), p['strobe_off'],
             fmt_dyn(p['intensity']), p['use_gm'], p['blackout_on_gm0'], p['invert_int_chan'], p['invert_int_range'], p['limit_gm']))
    print('  blackout[%s] lamp_on[%s] lamp_off[%s] lamp_on_time=%dms' % (fmt_static(p['blackout']), fmt_static(p['lamp_on']), fmt_static(p['lamp_off']), p['lamp_on_time_ms']))
    print('Pan & Tilt: 16bit=%d 8bit=%d pan=%d tilt=%d patch=%d (pMSB %d pLSB %d tMSB %d tLSB %d) presets=%d macros=%d follow=%d misc[%s]'
          % (p['pt16'], p['pt8'], p['pan_offset'], p['tilt_offset'], p['pt_patch'], p['patch_pan_msb'], p['patch_pan_lsb'],
             p['patch_tilt_msb'], p['patch_tilt_lsb'], p['presets'], p['macros'], p['follow_spot'], fmt_dyn(p['misc_move'])))
    print('Fixed Color: ' + ' '.join('%s[%s]' % (k, fmt_dyn(r)) for k, r in p['color'].items()))
    print('CMY & RGB: cmy=%d rgb=%d picker=%d invert=%d hue=%d sat=%d val=%d ' % (p['cmy'], p['rgb'], p['cmy_picker'], p['invert_cmyrgb'], p['rgb_hue'], p['rgb_sat'], p['rgb_val'])
          + ' '.join('%s[%s]' % (k, fmt_dyn(r, 'none', 'full')) for k, r in p['cmyrgb'].items()))
    print('Gobo: ' + ' '.join('%s[%s]' % (k, fmt_dyn(r)) for k, r in p['gobo'].items()))
    bf = p['beam_fx']
    print('Beam: ' + ' '.join('%s[%s]' % (k, fmt_dyn(bf[k])) for k in ('iris', 'focus', 'zoom', 'frost', 'beam1', 'beam2')))
    print('Effects: ' + ' '.join('%s[%s]' % (k, fmt_dyn(bf[k])) for k in ('effects1', 'effects2')) + ' effects3[%s] effects4[%s]' % (fmt_dyn(p['effects3']), fmt_dyn(p['effects4'])))
    types = p['pal_types'] + p['fx_pal2_types']
    for i, k in enumerate(('color_pal1', 'color_pal2', 'gobo_pal1', 'gobo_pal2', 'fx_pal1', 'fx_pal2')):
        ents = [e for e in p[k] if e['used']]
        if not ents: continue
        print('  %s types=%s' % (k, [PALETTE_TYPES.get(t, t) for t in types[5 * i:5 * i + 5]]))
        for j, e in enumerate(p[k]):
            if not e['used']: continue
            vals = ['%d' % e['values'][b] if e['defined_mask'] >> b & 1 else '-' for b in range(5)]
            print('    %2d: %s rgb=%s bmp=%r' % (j, ' '.join(vals), e['rgb'], e['bitmap']['text']))
    print('Specials: reset[%s] reset_time=%dms' % (fmt_static(p['reset']), p['reset_time_ms']))
    for i, r in enumerate(p['specials']):
        if r['enable'] or r['name']['text']: print('  special %2d %-16r %s' % (i + 1, r['name']['text'], fmt_static(r)))
    print('  ext_gm=%s default_dmx=%s' % ([i for i, v in enumerate(p['ext_gm']) if v], {i: v for i, v in enumerate(p['default_dmx']) if v}))
    print('Levels: ' + ' '.join('%d:%s%s%s' % (i, l['caption']['text'] or '""', '' if not l['invisible'] else '/inv', '' if not l['disabled'] else '/dis')
                               for i, l in enumerate(p['levels']) if not l['disabled'] or l['caption']['text']))
    print('Offline Visualizer: %r / %r / %r' % (s('ov_manufacturer'), s('ov_model'), s('ov_mode')))


def summary(p):
    """One row of booleans/counts for library statistics."""
    pals = [sum(1 for e in p[k] if e['used']) for k in ('color_pal1', 'color_pal2', 'gobo_pal1', 'gobo_pal2', 'fx_pal1', 'fx_pal2')]
    types = p['pal_types'] + p['fx_pal2_types']
    return {
        'channels': p['last_offset'] + 1, 'pt16': p['pt16'], 'pt8': p['pt8'], 'pt_patch': p['pt_patch'],
        'presets': p['presets'], 'macros': p['macros'], 'follow': p['follow_spot'], 'misc_move': p['misc_move']['enable'],
        'intensity': p['intensity']['enable'], 'shutter': p['shutter']['enable'], 'strobe': p['strobe']['enable'],
        'use_gm': p['use_gm'], 'bogm': p['blackout_on_gm0'], 'lamp_on': p['lamp_on']['enable'], 'lamp_off': p['lamp_off']['enable'],
        'reset': p['reset']['enable'], 'cmy': p['cmy'], 'rgb': p['rgb'], 'cmy_picker': p['cmy_picker'],
        'hsv': int(bool(p['rgb_hue'] or p['rgb_sat'] or p['rgb_val'])), 'ext_gm': int(any(p['ext_gm'])),
        'specials': sum(1 for r in p['specials'] if r['enable']), 'palettes_used': sum(1 for n in pals if n),
        'palette_entries': sum(pals), 'multi_type_palettes': sum(1 for i in range(6) if any(types[5 * i + 1:5 * i + 5])),
        'ov_link': int(bool(p['ov_model']['text'])), 'default_dmx': int(any(p['default_dmx'])),
        'rtf_notes': int(bool(p['notes_rtf']['text'])), 'icon_bitmap': int(bool(p['icon_bitmap']['text'])),
        'color_ctl': sum(r['enable'] for r in p['color'].values()), 'gobo_ctl': sum(r['enable'] for r in p['gobo'].values()),
        'beam_ctl': sum(p['beam_fx'][k]['enable'] for k in ('iris', 'focus', 'zoom', 'frost', 'beam1', 'beam2')),
        'fx_ctl': p['beam_fx']['effects1']['enable'] + p['beam_fx']['effects2']['enable'] + p['effects3']['enable'] + p['effects4']['enable'],
        'transparent': p['transparent'], 'invert_int_chan': p['invert_int_chan'], 'invert_cmyrgb': p['invert_cmyrgb'],
        'invert_int_range': p['invert_int_range'], 'limit_gm': p['limit_gm'], 'has_specials': int(any(r['enable'] for r in p['specials'])),
        'palettes_gt2': int(sum(1 for n in pals if n) > 2), 'version_2609': int(p['format_version'] == 2609),
        'reserved_nonzero': int(any(p[k].strip(b'\0') for k in ('reserved_10A9', 'reserved_1366', 'reserved_1C96', 'reserved_1CEB', 'reserved_3D94', 'reserved_3E94'))),
        'subrecord_reserved_nonzero': int(any(r['reserved'].strip(b'\0') for r in
            p['specials'] + [p['blackout'], p['lamp_on'], p['lamp_off'], p['reset'], p['shutter'], p['strobe'], p['intensity'], p['misc_move'], p['effects3'], p['effects4']]
            + list(p['color'].values()) + list(p['cmyrgb'].values()) + list(p['gobo'].values()) + list(p['beam_fx'].values()) + p['levels']
            + [e for k in ('color_pal1', 'color_pal2', 'gobo_pal1', 'gobo_pal2', 'fx_pal1', 'fx_pal2') for e in p[k]])),
    }


def main(argv):
    cmd, args = argv[1], argv[2:]
    if cmd == 'dump':
        for f in args:
            print('==', f); dump(parse(load(f)))
    elif cmd == 'roundtrip':
        bad = 0
        for f in args:
            raw = load(f); ok = serialize(parse(raw)) == raw; bad += not ok
            print('%s %s' % ('OK  ' if ok else 'FAIL', f))
        print('%d files, %d mismatches' % (len(args), bad)); sys.exit(1 if bad else 0)
    elif cmd == 'stats':
        src = args[0]; limit = int(args[1]) if len(args) > 1 else None
        if src.lower().endswith('.zip'):
            z = zipfile.ZipFile(src); names = [n for n in z.namelist() if n.lower().endswith(('.udf', '.udo', '.gs2'))]
            rd = lambda n: z.read(n)
        else:
            names = [f for f in glob.glob(os.path.join(src, '**', '*'), recursive=True) if f.lower().endswith(('.udf', '.udo', '.gs2'))]
            rd = lambda n: open(n, 'rb').read()
        if limit: random.seed(16); names = random.sample(names, limit)
        tot = Counter(); n = 0; bad = []; sizes = Counter(); typecodes = Counter(); mask = Counter()
        for name in names:
            try: raw = load(rd(name), name)
            except Exception as e: bad.append((name, str(e))); continue
            sizes[len(raw)] += 1
            if len(raw) != SIZE: bad.append((name, 'size %d' % len(raw))); continue
            p = parse(raw)
            if serialize(p) != raw: bad.append((name, 'roundtrip')); continue
            n += 1
            for k, v in summary(p).items(): tot[k] += v
            for t in p['pal_types'] + p['fx_pal2_types']: typecodes[t] += 1
            for k in ('color_pal1', 'color_pal2', 'gobo_pal1', 'gobo_pal2', 'fx_pal1', 'fx_pal2'):
                for e in p[k]:
                    if e['used']: mask[e['defined_mask']] += 1
        print('profiles parsed and round-tripped: %d of %d (stream-0 sizes %s)' % (n, len(names), dict(sizes)))
        for k, v in sorted(tot.items()): print('  %-20s %6d  (%.1f%%)' % (k, v, 100.0 * v / max(n, 1)))
        print('  palette type codes:', {PALETTE_TYPES.get(k, k): v for k, v in sorted(typecodes.items())})
        print('  palette defined masks:', dict(sorted(mask.items())))
        if bad: print('  problems:', bad[:20], '... %d total' % len(bad))
    else:
        print(__doc__)


if __name__ == '__main__':
    main(sys.argv)
