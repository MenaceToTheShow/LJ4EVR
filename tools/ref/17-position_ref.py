"""17-position_ref.py - reference maths for LightJockey 2 pan/tilt handling.

Companion to 17-gap-position-maths.md. Python 3 stdlib only; run with
    python -I 17-position_ref.py
Every number tagged ASSUMED is not in any LJ2 source; every assert that uses a
help-file example cites it. Sources (abbreviated as in the note):
  CHM  = D:/lj4evr/reference/installer/lj-Help_chm_2.209.txt (line numbers)
  H26  = D:/lj4evr/reference/manuals/lj-Help2_6b6.txt (PDF page numbers)
  IMG  = D:/lj4evr/reference/screenshots/lj-Help_chm_images/<name>.gif
  INI  = D:/lj4evr/reference/installer/extracted_2.209/app/User/Default/Main.Ini
"""
from fractions import Fraction
import math
import random

# ---------------------------------------------------------------------------
# 1. Coordinate model
# ---------------------------------------------------------------------------
HALF = 32768          # readout 0 == DMX16 32768 (ASSUMED: "Homed (50% pan, 50% tilt)" H26 p135)
LO, HI = -32768, 32767


def clamp(v):
    return max(LO, min(HI, v))


def readout_to_dmx16(v):
    """Signed readout (grid coordinate) -> unsigned 16-bit DMX pair value."""
    assert LO <= v <= HI
    return v + HALF


def dmx16_to_readout(d):
    assert 0 <= d <= 65535
    return d - HALF


def split_msb_lsb(d):
    return d >> 8, d & 0xFF


def join_msb_lsb(msb, lsb):
    return (msb << 8) | lsb


def readout_to_dmx8(v, mode="truncate"):
    """8-bit fixture output. ASSUMED: the MSB of the internal 16-bit value.
    mode='truncate' (v>>8) or 'round' - which one LJ2 uses is unverified
    (CHM 3109: small relative moves are 'rounded off and possibly ignored')."""
    d = readout_to_dmx16(v)
    if mode == "truncate":
        return d >> 8
    return min(255, (d + 128) >> 8)


def dmx8_to_readout(x):
    """Inverse for an 8-bit fixture: LSB is 0 (IMG joystickmovement: -13824, -9984)."""
    return (x << 8) - HALF


HOME = (0, 0)  # readout; = DMX16 (32768, 32768) = MSB 128 / LSB 0; = DMX8 128


# ---------------------------------------------------------------------------
# 2. Resolution and arrow/drag steps
# ---------------------------------------------------------------------------
RES16_DEFAULT = 32    # INI [MOVEMENT] Res16Bit=32
RES8_DEFAULT = 255    # INI [MOVEMENT] Res8Bit=255 (meaning unverified)
RES16_MAX = 256       # RN 2.5 "maximum resolution factor for built-in 16 bit fixtures from 128 to 256"


def arrow_step(v, direction, res):
    """One arrow click / one drag step moves `res` readout units (ASSUMED unit)."""
    assert direction in (-1, 1)
    return clamp(v + direction * res)


def joystick_velocity(dx, dy, res):
    """Touch/Joystick mode: 'distance from center is combined with the
    resolution parameter to define a speed' (CHM 3083). ASSUMED product,
    units per tick; tick unknown."""
    return dx * res, dy * res


# ---------------------------------------------------------------------------
# 3. Absolute vs relative
# ---------------------------------------------------------------------------
def absolute_apply(target, fixtures):
    """All selected fixtures get the same (pan, tilt) (CHM 3105)."""
    return {f: target for f in fixtures}


def relative_apply(master_old, master_new, positions):
    """All selected fixtures move by the master's delta (CHM 3105-3107)."""
    dp = master_new[0] - master_old[0]
    dt = master_new[1] - master_old[1]
    return {f: (clamp(p + dp), clamp(t + dt)) for f, (p, t) in positions.items()}


# ---------------------------------------------------------------------------
# 4. Position Options (edit-time, on the readout) and transmit-time invert/swap
# ---------------------------------------------------------------------------
def mirror_pan(pan, tilt):
    return clamp(-pan), tilt          # "mirrored over the vertical centerline (pan = 0)" CHM 3115


def mirror_tilt(pan, tilt):
    return pan, clamp(-tilt)          # CHM 3117


def swap_pan_tilt(pan, tilt):
    return tilt, pan                  # CHM 3119


def pan_360(pan, tilt, pan_range_deg):
    """'clicking the button will, if possible move pan a full 360 degree rotation'
    (CHM 3121). ASSUMED: +360 first, else -360, else unchanged; needs the
    profile's pan range in degrees (built-in profiles only, H26 p137 note)."""
    if pan_range_deg <= 360:
        return pan, tilt
    units = round(65536 * 360 / pan_range_deg)
    for s in (1, -1):
        if LO <= pan + s * units <= HI:
            return pan + s * units, tilt
    return pan, tilt


def transmit(pan16, tilt16, invert_pan=False, invert_tilt=False, swap=False):
    """Fixture-configuration flags, applied 'just before pan and tilt DMX values
    are transmitted' (CHM 2063). ASSUMED: invert = 65535 - value on the full
    16-bit pair; swap exchanges the pairs; order invert-then-swap."""
    if invert_pan:
        pan16 = 65535 - pan16
    if invert_tilt:
        tilt16 = 65535 - tilt16
    if swap:
        pan16, tilt16 = tilt16, pan16
    return pan16, tilt16


def patch_16bit(pan16, tilt16, pan_ofs, tilt_ofs, patch=None):
    """CHM 10497-10587 (thepantilttab.htm): calculations happen in an adjacent
    MSB/LSB pair at pan_ofs/pan_ofs+1 and tilt_ofs/tilt_ofs+1; `patch` maps
    {'pan_msb':..,'pan_lsb':..,'tilt_msb':..,'tilt_lsb':..} to final offsets.
    Returns {offset: dmx8}."""
    pm, pl = split_msb_lsb(pan16)
    tm, tl = split_msb_lsb(tilt16)
    if patch is None:
        patch = {"pan_msb": pan_ofs, "pan_lsb": pan_ofs + 1,
                 "tilt_msb": tilt_ofs, "tilt_lsb": tilt_ofs + 1}
    return {patch["pan_msb"]: pm, patch["pan_lsb"]: pl,
            patch["tilt_msb"]: tm, patch["tilt_lsb"]: tl}


# ---------------------------------------------------------------------------
# 5. Fan out
# ---------------------------------------------------------------------------
DELTA_UNIT = 256   # ASSUMED: Delta Pan/Tilt bar value x 256 = readout units (fits IMG fanoutdialog)


def fan_out_offsets(n, delta):
    """Symmetric offsets for n>=2 fixtures in apply order. Odd n: middle fixture
    is the centre; even n: virtual centre (CHM 3131). End fixtures sit at
    +-delta*DELTA_UNIT (ASSUMED from the 5-fixture screenshot)."""
    assert n >= 2
    mid = Fraction(n - 1, 2)
    return [int(round(delta * DELTA_UNIT * (Fraction(i) - mid) / mid)) for i in range(n)]


def fan_out(positions, order, delta_pan, delta_tilt, center_on_home=False):
    """positions: {fixture: (pan, tilt)}; order: list of fixtures (Fixture Number /
    Icon H / Icon V / Custom). Centre = home when center_on_home, else the
    middle fixture's (or the mean of the two middle fixtures') current
    position (ASSUMED reading of 'modifying ... by symmetrical amounts')."""
    n = len(order)
    if center_on_home:
        cp, ct = HOME
    else:
        a, b = order[(n - 1) // 2], order[n // 2]
        cp = (positions[a][0] + positions[b][0]) // 2
        ct = (positions[a][1] + positions[b][1]) // 2
    op = fan_out_offsets(n, delta_pan)
    ot = fan_out_offsets(n, delta_tilt)
    return {f: (clamp(cp + op[i]), clamp(ct + ot[i])) for i, f in enumerate(order)}


def spread(positions, order, axis):
    """Spread button: 'distributing the pan and tilt delta from the actual
    position of the first and last fixture in the selection order' (CHM 3135).
    ASSUMED reading: linear interpolation between first and last on one axis."""
    n = len(order)
    first, last = positions[order[0]][axis], positions[order[-1]][axis]
    out = dict(positions)
    for i, f in enumerate(order):
        v = first + (last - first) * i // (n - 1)
        p = list(out[f])
        p[axis] = clamp(v)
        out[f] = tuple(p)
    return out


# ---------------------------------------------------------------------------
# 6. Auto delay
# ---------------------------------------------------------------------------
DELAY_FULL = 256   # Max Delay 1..256, 256 = 100% = 0% (CHM 3265)


def auto_delay(n, style, max_delay=256, count=None, segments=None, rng=None):
    """Returns one delay value per fixture in delay order, in 'Max Delay units'
    (0..255; 256 == 0). ASSUMED: integer floor; stored as a byte."""
    assert 1 <= max_delay <= 256
    if style == "linear":
        d = [max_delay * k // n for k in range(n)]
    elif style == "circular":
        d = [max_delay * (k % count) // count for k in range(n)]
    elif style == "segments":
        d = [max_delay * (k * segments // n) // segments for k in range(n)]
    elif style == "random":
        rng = rng or random.Random()
        d = [rng.randrange(max_delay) for _ in range(n)]
    else:
        raise ValueError(style)
    return [x % DELAY_FULL for x in d]


def delay_pct(d):
    return d * 100 / DELAY_FULL


def delay_order(fixtures, mode, icon_pos=None):
    """mode: 'number' | 'icon_h' | 'icon_v', optionally + '_rev' (DFM
    TAUTODELAYFORM RGOrder items). Ties on icon position: ASSUMED stable by
    fixture number."""
    key = {"number": lambda f: f,
           "icon_h": lambda f: (icon_pos[f][0], f),
           "icon_v": lambda f: (icon_pos[f][1], f)}[mode.replace("_rev", "")]
    out = sorted(fixtures, key=key)
    return out[::-1] if mode.endswith("_rev") else out


# ---------------------------------------------------------------------------
# 7. Movement macro engine
# ---------------------------------------------------------------------------
N_TABLE = 4096   # ASSUMED: "at step size 512 the ellipse only contains 8 pan and tilt positions" (CHM 3199) -> 8*512
AMP_HALF_RANGE = 127   # "127 represents half a complete pan or tilt movement range" (H26 p132)


def system_tables(name, n=N_TABLE):
    """Two arrays of -1..+1 (H26 p132). Only 'System Ellipse' is described in
    the help (pan=cos, tilt=sin). The other formulas are read off the exe's
    type names (04 fact-check claim 11) and are ASSUMED."""
    a = [2 * math.pi * i / n for i in range(n)]
    if name in ("System Ellipse", "System Stepped Ellipse"):
        return [math.cos(x) for x in a], [math.sin(x) for x in a]
    if name == "System Ellipse 2":            # ASSUMED: ellipse run with the other phase
        return [math.sin(x) for x in a], [math.cos(x) for x in a]
    if name == "System Cos(2*a),Sin(a)":
        return [math.cos(2 * x) for x in a], [math.sin(x) for x in a]
    if name == "System Sin(a)*|Sin(2a)|":     # ASSUMED axis assignment
        return [math.cos(x) for x in a], [math.sin(x) * abs(math.sin(2 * x)) for x in a]
    if name == "System Sin(a)^3":             # ASSUMED axis assignment
        return [math.cos(x) for x in a], [math.sin(x) ** 3 for x in a]
    raise KeyError(name)


def index_increment(cycle_s, tick_hz, n=N_TABLE):
    """Index steps per engine tick so that one cycle takes cycle_s seconds.
    tick_hz is the unknown engine tick (10-completeness-critique GAP 1)."""
    return n / (cycle_s * tick_hz)


def delay_to_index(delay_byte, n=N_TABLE):
    return n * delay_byte // DELAY_FULL


def stepped_points(step, n=N_TABLE):
    return n // step


def quantise(idx, step, n=N_TABLE):
    return (int(idx) // step * step) % n


def amp_to_units(amp):
    """Amplitude 0..127 -> readout units of the macro's peak (ASSUMED linear: 127 -> 32768)."""
    return amp * HALF / AMP_HALF_RANGE


def macro_offset(tables, idx_pan, idx_tilt, amp_pan, amp_tilt, step=1, n=N_TABLE):
    pan_t, tilt_t = tables
    ip = quantise(idx_pan, step, n)
    it = quantise(idx_tilt, step, n)
    return (int(round(pan_t[ip] * amp_to_units(amp_pan))),
            int(round(tilt_t[it] * amp_to_units(amp_tilt))))


def fixture_output(base, offset):
    """'these pan and tilt values are combined with the regular pan and tilt'
    (H26 p132); clipping at the edges ('cut off') = clamp (ASSUMED)."""
    return clamp(base[0] + offset[0]), clamp(base[1] + offset[1])


def macro_tick(idx, incr, direction, n=N_TABLE):
    """direction +1 = CW, -1 = CCW (H26 p132 'decrements if the direction is set to CCW')."""
    return (idx + direction * incr) % n


def effective_index(idx, delay_byte, tweak_index=0, n=N_TABLE):
    """Delay 'applies a constant value to the index(es)'; Tweak 'add a constant
    value to one of the indexes only' (H26 p133). ASSUMED: delay is subtracted
    (as the generic engine documents, CHM 5571) and tweak lands on the tilt index."""
    d = (idx - delay_to_index(delay_byte, n)) % n
    return d, (d + tweak_index) % n


# ---------------------------------------------------------------------------
# 8. Cue macro control scaling
# ---------------------------------------------------------------------------
RAW_MAX = 127   # "values will never exceed the maximum value 127 for amplitude or speed" (CHM 5191)


def scale_raw(raw, pct):
    """Amplitude or speed byte scaled by the cue macro fader (0-100% or 1-999%).
    'may scale down to a value of 0 as scaled values goes below 1' -> floor (ASSUMED)."""
    return max(0, min(RAW_MAX, int(raw * pct / 100)))


def dmx_in_pct(value):
    """DMX-in override 'scale from 0%-100.0%' (CHM 8987). ASSUMED linear value/255."""
    return value / 255 * 100


# ---------------------------------------------------------------------------
# Self-check against the help's own numbers
# ---------------------------------------------------------------------------
def demo():
    # IMG movement2: "Movement 16 Bit", readout -16256 : 17120, Resolution 32
    pan, tilt = -16256, 17120
    assert readout_to_dmx16(pan) == 16512 and split_msb_lsb(16512) == (64, 128)
    assert readout_to_dmx16(tilt) == 49888 and split_msb_lsb(49888) == (194, 224)
    assert pan % 32 == 0 and tilt % 32 == 0           # multiples of Resolution 32
    assert dmx16_to_readout(join_msb_lsb(64, 128)) == pan
    assert readout_to_dmx16(HOME[0]) == 32768 and split_msb_lsb(32768) == (128, 0)
    # IMG joystickmovement: "Movement 8 Bit", readout -13824 : -9984
    assert readout_to_dmx8(-13824) == 74 and readout_to_dmx8(-9984) == 89
    assert dmx8_to_readout(74) == -13824 and dmx8_to_readout(128) == 0
    # Resolution: 508 clicks of 32 from home reach -16256
    v = 0
    for _ in range(508):
        v = arrow_step(v, -1, 32)
    assert v == -16256
    assert arrow_step(HI, 1, 256) == HI
    # Relative: master moved 50 left -> every fixture 50 left (CHM 3107)
    r = relative_apply((0, 0), (-50, 0), {"a": (1000, 2000), "b": (-32768, 5)})
    assert r == {"a": (950, 2000), "b": (-32768, 5)}
    # Position options
    assert mirror_pan(-1234, 1234) == (1234, 1234)
    assert mirror_tilt(-1234, 1234) == (-1234, -1234)
    assert swap_pan_tilt(-1234, 1234) == (1234, -1234)
    assert mirror_pan(-32768, 0) == (32767, 0)         # clamp at the asymmetric edge
    assert pan_360(0, 0, 540) == (0, 0)                 # +43691 and -43691 both leave the range from home
    assert pan_360(-20000, 0, 540) == (-20000 + 43691, 0)
    assert pan_360(0, 0, 360) == (0, 0)
    # Transmit-time invert/swap and the four patch examples (CHM 10497-10587)
    assert transmit(0, 65535, invert_pan=True) == (65535, 65535)
    assert transmit(1, 2, swap=True) == (2, 1)
    p16, t16 = readout_to_dmx16(-16256), readout_to_dmx16(17120)
    assert patch_16bit(p16, t16, 10, 15) == {10: 64, 11: 128, 15: 194, 16: 224}                  # Example 1
    assert patch_16bit(p16, t16, 10, 15, {"pan_msb": 11, "pan_lsb": 10, "tilt_msb": 16, "tilt_lsb": 15}) \
        == {11: 64, 10: 128, 16: 194, 15: 224}                                                   # Example 2
    assert patch_16bit(p16, t16, 4, 13, {"pan_msb": 4, "pan_lsb": 13, "tilt_msb": 5, "tilt_lsb": 14}) \
        == {4: 64, 13: 128, 5: 194, 14: 224}                                                     # Example 3
    assert patch_16bit(p16, t16, 21, 23, {"pan_msb": 4, "pan_lsb": 13, "tilt_msb": 7, "tilt_lsb": 16}) \
        == {4: 64, 13: 128, 7: 194, 16: 224}                                                     # Example 4
    # Fan out: IMG fanoutdialog, 5 fixtures, Delta Pan 95, Delta Tilt 86, Center Delta on Home.
    # Measured dot positions (pixels, grid half-width 49.5 px = full 16-bit half range):
    # pan rel -0.737 -0.369 0 +0.364 +0.737 ; tilt step 16.5 px vs pan step 18.2 px (ratio 0.906 = 86/95)
    offs = fan_out_offsets(5, 95)
    assert offs == [-24320, -12160, 0, 12160, 24320]
    assert abs(offs[4] / HALF - 0.737) < 0.02
    assert abs(fan_out_offsets(5, 86)[4] / fan_out_offsets(5, 95)[4] - 16.5 / 18.2) < 0.01
    assert fan_out_offsets(4, 95) == [-24320, -8107, 8107, 24320]          # even count: virtual centre
    assert fan_out_offsets(2, 10) == [-2560, 2560]
    fo = fan_out({"a": (5, 5), "b": (6, 6), "c": (7, 7)}, ["a", "b", "c"], 95, 86, center_on_home=True)
    assert fo["b"] == (0, 0) and fo["a"] == (-24320, -22016) and fo["c"] == (24320, 22016)
    sp = spread({"a": (-1000, 0), "b": (0, 0), "c": (2000, 0)}, ["a", "b", "c"], 0)
    assert sp["b"] == (500, 0)
    # Auto delay: CHM 3275-3285 (4 fixtures linear), 3287-3299 (6 fixtures circular count 3),
    # 3301-3313 (6 fixtures segments 3)
    assert [round(delay_pct(d)) for d in auto_delay(4, "linear")] == [0, 25, 50, 75]
    assert [int(delay_pct(d)) for d in auto_delay(6, "circular", count=3)] == [0, 33, 66, 0, 33, 66]
    assert [int(delay_pct(d)) for d in auto_delay(6, "segments", segments=3)] == [0, 0, 33, 33, 66, 66]
    assert len(set(auto_delay(4, "linear"))) == 4            # "two fixtures will never get the same delay value"
    assert auto_delay(4, "linear", max_delay=128) == [0, 32, 64, 96]   # 128 = 50% max
    # IMG autodelay_256/128/64: 8 fixtures on a System Ellipse spaced 45 / 22.5 / 11.25 degrees
    for md, deg in ((256, 45), (128, 22.5), (64, 11.25)):
        d = auto_delay(8, "linear", max_delay=md)
        assert all(abs((d[k] - d[k - 1]) * 360 / DELAY_FULL - deg) < 1e-9 for k in range(1, 8))
    assert all(0 <= x < 256 for x in auto_delay(50, "random", max_delay=64, rng=random.Random(1)))
    assert delay_order([3, 1, 2], "number_rev") == [3, 2, 1]
    assert delay_order([1, 2, 3], "icon_h", {1: (50, 0), 2: (10, 0), 3: (10, 9)}) == [2, 3, 1]
    # Macro engine
    assert stepped_points(512) == 8                     # CHM 3199
    assert stepped_points(1) == N_TABLE
    pan_t, tilt_t = system_tables("System Ellipse")
    assert abs(pan_t[0] - 1) < 1e-9 and abs(tilt_t[N_TABLE // 4] - 1) < 1e-9
    # homed fixture, max amplitude -> full-size circle touching the edges (H26 p132)
    o = macro_offset((pan_t, tilt_t), 0, 0, 127, 127)
    assert fixture_output(HOME, o) == (32767, 0)        # clamp hits the asymmetric +edge
    o = macro_offset((pan_t, tilt_t), N_TABLE // 2, N_TABLE // 2, 127, 127)
    assert fixture_output(HOME, o) == (-32768, 0)
    # tilt cycle = 2 x pan cycle on the ellipse -> figure eight: tilt index runs at half speed
    ip = index_increment(2.0, 100.0)
    it = index_increment(4.0, 100.0)
    assert ip == 2 * it and ip == N_TABLE / 200
    # delay 25 % puts the fixture a quarter cycle back
    assert effective_index(1024, 64)[0] == 0
    assert effective_index(0, 64)[0] == N_TABLE - 1024
    assert macro_tick(N_TABLE - 1, 1, 1) == 0 and macro_tick(0, 1, -1) == N_TABLE - 1
    # Cue macro control (IMG cuemacrocontrol: Ampl 45 %, Speed 490 %; CHM 5191 clamp at 127)
    assert scale_raw(127, 45) == 57
    assert scale_raw(127, 490) == 127
    assert scale_raw(100, 0.5) == 0
    assert scale_raw(60, 100) == 60
    assert scale_raw(60, 0) == 0                        # Freeze == speed 0 %
    assert abs(dmx_in_pct(255) - 100) < 1e-9
    print("17-position_ref.py: all checks passed")


if __name__ == "__main__":
    demo()
