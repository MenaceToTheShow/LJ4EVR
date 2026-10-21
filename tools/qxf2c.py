#!/usr/bin/env python3
"""qxf2c.py - QLC+ fixture definitions -> src/fixlib/lj_profiles_gen.c (design 6.1 item 1,
verdicts ME-8 and ME-26). Python 3 stdlib only (xml.etree).

    python -I tools/qxf2c.py             # write lj_profiles_gen.c and tools/qxf2c_overflow.txt
    python -I tools/qxf2c.py --check     # CI: regenerate in memory, exit 1 on drift or a bad map line
    python -I tools/qxf2c.py --selftest  # determinism + the mapping rules over one in-repo QXF and a synthetic one

Inputs: tools/qxf_map.tsv (file, mode, id), third_party/qlcplus-fixtures/*.qxf (Apache-2.0,
commit in third_party/qlcplus-fixtures/SOURCE), src/fixlib/lj_exe_profiles.c (the ids a map
line may name). One `static lj_profile` per mapped mode; the control-window tables are not
generated here, lj_layout_derive() builds them at first use (tier 2, design 6.3).

The Group + Preset -> class / dyn / palette rules are the table in docs/fixtures.md; this
file is their one implementation. Palettes keep the first 20 candidate capabilities per
wheel (LJ_PAL_ENTRIES); every wheel with more is listed in tools/qxf2c_overflow.txt, which
generate-check prints.
"""
import hashlib
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QXF_DIR = os.path.join(ROOT, "third_party", "qlcplus-fixtures")
MAP = os.path.join(ROOT, "tools", "qxf_map.tsv")
OUT = os.path.join(ROOT, "src", "fixlib", "lj_profiles_gen.c")
OVERFLOW = os.path.join(ROOT, "tools", "qxf2c_overflow.txt")
EXE = os.path.join(ROOT, "src", "fixlib", "lj_exe_profiles.c")
NS = "{http://www.qlcplus.org/FixtureDefinition}"
CH = 32            # LJ_PROFILE_CH
PAL = 20           # LJ_PAL_ENTRIES
EXTRA = 1000       # first extra id (ME-8)
NAME_CAP = {"tag": 15, "manufacturer": 30, "name": 60, "mode": 30, "dyn": 8, "level": 5}

# Capability presets that make a palette entry (plus untyped ones whose text is not a motion word).
PAL_PRESETS = {"ColorMacro", "ColorDoubleMacro", "ColorWheelIndex", "GoboMacro", "PrismEffectOn", "PrismEffectOff", "GenericPicture"}
MOTION_RE = re.compile(r"rotat|\bCW\b|\bCCW\b|shake|random|chase|scroll|spin|strobe|pulse|reset|lamp|macro|stop", re.I)
RGB_PRESET = {"IntensityRed": 0, "IntensityGreen": 1, "IntensityBlue": 2}
CMY_PRESET = {"IntensityCyan": 0, "IntensityMagenta": 1, "IntensityYellow": 2}
HSV_PRESET = {"IntensityHue": 0, "IntensitySaturation": 1, "IntensityValue": 2}


def cp1252(s):
    return s.encode("cp1252", "replace")


def cstr(s, cap=None):
    """A C string literal of the CP1252 bytes of s (octal escapes above 0x7e), cut to cap bytes."""
    b = cp1252(s)
    if cap is not None:
        b = b[:cap]
    out = []
    for c in b:
        if c == 0x22:
            out.append('\\"')
        elif c == 0x5C:
            out.append("\\\\")
        elif 0x20 <= c < 0x7F:
            out.append(chr(c))
        else:
            out.append("\\%03o" % c)
    return '"' + "".join(out) + '"'


def load_qxf(path):
    root = ET.parse(path).getroot()
    chans = {}
    for ch in root.findall(NS + "Channel"):
        g = ch.find(NS + "Group")
        caps = [(int(c.get("Min")), int(c.get("Max")), c.get("Preset"), c.get("Res1"), (c.text or "").strip())
                for c in ch.findall(NS + "Capability")]
        chans[ch.get("Name")] = {"name": ch.get("Name"), "group": g.text if g is not None else None,
                                 "byte": int(g.get("Byte", "0")) if g is not None else 0, "preset": ch.get("Preset"),
                                 "default": int(ch.get("Default", "0")), "caps": caps}
    modes = {}
    for m in root.findall(NS + "Mode"):
        rows = sorted((int(c.get("Number")), c.text) for c in m.findall(NS + "Channel"))
        modes[m.get("Name")] = [chans[name] for _, name in rows]
    text = lambda tag: (root.findtext(NS + tag) or "").strip()
    author = root.find(NS + "Creator")
    author = (author.findtext(NS + "Author") or "").strip() if author is not None else ""
    return {"manufacturer": text("Manufacturer"), "model": text("Model"), "author": author, "modes": modes}


def read_map():
    rows = []
    for ln, line in enumerate(open(MAP, encoding="ascii"), 1):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if len(f) != 3 or not f[2].isdigit():
            raise SystemExit("qxf_map.tsv:%d: expected file<TAB>mode<TAB>id" % ln)
        rows.append((f[0], f[1], int(f[2])))
    return rows


def exe_rows():
    """id -> (tag, manufacturer, name, mode) from the committed lj_exe_profiles.c."""
    rows = {}
    for m in re.finditer(r'^\s*\{\s*(\d+),\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",', open(EXE, encoding="latin-1").read(), re.M):
        rows[int(m.group(1))] = (m.group(2), m.group(3), m.group(4), m.group(5))
    return rows


class Profile:
    def __init__(self, ident, chans, src):
        self.ident, self.src = ident, src
        self.footprint = len(chans)
        self.cls = ["LJ_CH_NONE"] * CH
        self.dyn = {}                    # slot -> (ofs, name, min, max)
        self.pal = {}                    # palette index 0..5 -> [(dmx, rgb)]
        self.overflow = []               # (channel name, count)
        self.sfunc = {}                  # "lamp_on"/"lamp_off"/"reset"/"blackout" -> (ofs, value)
        self.flags = {}                  # pt16 pt8 pan_ofs tilt_ofs cmy_on rgb_on hsv patch
        self.pair_lsb = [0] * CH
        self.level_caption = [""] * CH
        self.invisible = [0] * CH
        self.disabled = [0] * CH
        self.default_dmx = [c["default"] for c in chans] + [0] * (CH - len(chans))
        self.convert(chans)

    def slot(self, base, n):
        for k in range(n):
            key = "%s[%d]" % (base, k) if n > 1 else base
            if key not in self.dyn:
                return key, k
        return None, None

    def setdyn(self, key, ofs, name, lo, hi):
        self.dyn[key] = (ofs, name, lo, hi)
        self.invisible[ofs] = 1

    def palette_from(self, idx, caps, chname):
        cand = [(mn, int(rgb[1:], 16) if rgb and re.match(r"^#[0-9a-fA-F]{6}$", rgb) else 0) for mn, mx, preset, rgb, txt in caps
                if preset in PAL_PRESETS or (preset is None and not MOTION_RE.search(txt))]   # Res1 "#rrggbb" -> 0x00RRGGBB (gfx order)
        if len(cand) > PAL:
            self.overflow.append((chname, len(cand)))
        if cand:
            self.pal[idx] = cand[:PAL]

    def convert(self, chans):
        prev_coarse = {}       # (group) -> ofs of the last byte-0 channel, for byte-1 pairs
        pan = tilt = None
        pan_fine = tilt_fine = None
        for ofs, c in enumerate(chans):
            p, g, name, caps = c["preset"] or "", c["group"] or "", c["name"], c["caps"]
            fine = c["byte"] == 1 or p.endswith("Fine")
            self.level_caption[ofs] = name
            cls = "LJ_CH_LEVEL"
            if p == "NoFunction" or g == "Nothing":
                cls = "LJ_CH_NONE"; self.invisible[ofs] = 1; self.disabled[ofs] = 1
            elif p.startswith("Position") or g in ("Pan", "Tilt"):
                is_pan = p.startswith("PositionPan") or g == "Pan"
                cls = "LJ_CH_PAN" if is_pan else "LJ_CH_TILT"
                if fine:
                    if is_pan: pan_fine = ofs
                    else: tilt_fine = ofs
                else:
                    if is_pan: pan = ofs
                    else: tilt = ofs
                self.invisible[ofs] = 1
            elif p in RGB_PRESET or p in CMY_PRESET:
                k = RGB_PRESET.get(p, CMY_PRESET.get(p))
                self.flags["rgb_on" if p in RGB_PRESET else "cmy_on"] = 1
                self.setdyn("cmy[%d]" % k, ofs, name, 0, 255)
            elif p in HSV_PRESET:
                self.flags.setdefault("hsv", [0, 0, 0])[HSV_PRESET[p]] = 1
            elif p.startswith("Intensity") or g == "Intensity":
                if fine:
                    cls = "LJ_CH_INTENSITY"
                elif "intensity" not in self.dyn and (p in ("IntensityDimmer", "IntensityMasterDimmer") or not p):
                    cls = "LJ_CH_INTENSITY"; self.setdyn("intensity", ofs, name, 0, 255)
            elif p.startswith("Shutter") and "Iris" in p or (g == "Beam" and "iris" in name.lower()):
                cls = "LJ_CH_BEAM"
                if not fine: self.setdyn("iris", ofs, name, 0, 255)
            elif g == "Shutter" or p.startswith("Shutter"):
                closed = [mn for mn, mx, pr, r, t in caps if pr == "ShutterClose"]
                opened = [mn for mn, mx, pr, r, t in caps if pr == "ShutterOpen"]
                strobe = [(mn, mx) for mn, mx, pr, r, t in caps if pr and pr.startswith("Strobe") and "Random" not in pr]
                if closed and opened and "shutter" not in self.dyn:
                    cls = "LJ_CH_SHUTTER"; self.setdyn("shutter", ofs, name, closed[0], opened[0])
                    self.sfunc.setdefault("blackout", (ofs, closed[0]))
                else:
                    cls = "LJ_CH_STROBE"
                if strobe and "strobe" not in self.dyn:
                    self.setdyn("strobe", ofs, name, strobe[0][0], strobe[0][1])
                    if "strobe_off" not in self.flags and opened: self.flags["strobe_off"] = opened[0]
            elif p.startswith("BeamFocus") or (g == "Beam" and "focus" in name.lower()):
                cls = "LJ_CH_BEAM"
                if not fine: self.setdyn("focus", ofs, name, 255 if p == "BeamFocusFarNear" else 0, 0 if p == "BeamFocusFarNear" else 255)
            elif p.startswith("BeamZoom") or (g == "Beam" and "zoom" in name.lower()):
                cls = "LJ_CH_BEAM"
                if not fine: self.setdyn("zoom", ofs, name, 255 if p == "BeamZoomBigSmall" else 0, 0 if p == "BeamZoomBigSmall" else 255)
            elif g == "Beam":
                cls = "LJ_CH_BEAM"
                key = "frost" if "frost" in name.lower() and "frost" not in self.dyn else self.slot("beam", 2)[0]
                if key and not fine: self.setdyn(key, ofs, name, 0, 255)
            elif g == "Colour" or p.startswith("Color"):
                cls = "LJ_CH_COLOR"
                if not fine:
                    key, k = self.slot("color", 4)
                    if key:
                        self.setdyn(key, ofs, name, 0, 255)
                        if k < 2: self.palette_from(k, caps, name)
            elif g == "Gobo" or p.startswith("Gobo"):
                cls = "LJ_CH_GOBO"
                if not fine:
                    key, k = self.slot("gobo", 4)
                    if key:
                        self.setdyn(key, ofs, name, 0, 255)
                        if k < 2 and not p.startswith("GoboIndex"): self.palette_from(2 + k, caps, name)
            elif g in ("Effect", "Prism") or p.startswith("Prism"):
                cls = "LJ_CH_EFFECT"
                if not fine:
                    key, k = self.slot("effects", 4)
                    if key:
                        self.setdyn(key, ofs, name, 0, 255)
                        if k < 2: self.palette_from(4 + k, caps, name)
            elif g == "Speed" or p.startswith("Speed"):
                cls = "LJ_CH_SPEED"; self.invisible[ofs] = 1
            elif g == "Maintenance" or p.startswith("Maintenance"):
                cls = "LJ_CH_SPECIAL"
            self.cls[ofs] = cls
            if fine and g in prev_coarse and cls not in ("LJ_CH_PAN", "LJ_CH_TILT"):
                self.pair_lsb[prev_coarse[g]] = 1 + ofs              # non-pan/tilt 16-bit pair (LJ_PAIR16_NON_PT_AS_ONE)
                self.invisible[ofs] = 1
            elif not fine:
                prev_coarse[g] = ofs
            for mn, mx, pr, r, t in caps:                             # statics mined from any channel
                if pr == "ResetAll": self.sfunc.setdefault("reset", (ofs, mn))
                elif pr == "LampOn": self.sfunc.setdefault("lamp_on", (ofs, mn))
                elif pr == "LampOff": self.sfunc.setdefault("lamp_off", (ofs, mn))
        if pan is not None and tilt is not None:
            self.flags["pan_ofs"], self.flags["tilt_ofs"] = pan, tilt
            if pan_fine is not None and tilt_fine is not None:
                self.flags["pt16"] = 1
                if pan_fine != pan + 1 or tilt_fine != tilt + 1:      # LJ computes adjacent MSB/LSB pairs, then re-routes [02 s5.3]
                    self.flags["patch"] = (pan, pan_fine, tilt, tilt_fine)
            else:
                self.flags["pt8"] = 1
        if "blackout" not in self.sfunc and "intensity" in self.dyn:
            self.sfunc["blackout"] = (self.dyn["intensity"][0], 0)

    def render(self):
        o = self.ident
        asc = lambda s: s.encode("ascii", "replace").decode("ascii")        # comments stay ASCII: cl /utf-8 rejects stray bytes
        L = ["/* %s mode %s: QLC+ %s, Apache-2.0, modified: converted (author %s) */" % (asc(self.src[0]), asc(self.src[1]), self.src[2], asc(self.src[3] or "unknown")),
             "static lj_profile lj_qxf_%d = {" % o["id"],
             "    .id = %d, .tag = %s, .manufacturer = %s, .name = %s, .mode = %s," % (o["id"], cstr(o["tag"], NAME_CAP["tag"]), cstr(o["manufacturer"], NAME_CAP["manufacturer"]), cstr(o["name"], NAME_CAP["name"]), cstr(o["mode"], NAME_CAP["mode"])),
             "    .footprint = %d," % self.footprint,
             "    .channel_class = { %s }," % ", ".join(self.cls[:self.footprint])]
        for key in sorted(self.dyn):
            ofs, name, lo, hi = self.dyn[key]
            L.append("    .%s = { 1, %d, %s, %d, %d }," % (key, ofs, cstr(name, NAME_CAP["dyn"]), lo, hi))
        if "strobe_off" in self.flags:
            L.append("    .strobe_off = %d," % self.flags["strobe_off"])
        for key in ("blackout", "lamp_on", "lamp_off", "reset"):
            if key in self.sfunc:
                ofs, v = self.sfunc[key]
                L.append("    .%s = { \"\", 1, { [%d] = 1 }, { [%d] = %d } }," % (key, ofs, ofs, v))
        if self.pal:
            P = []
            for idx in range(6):
                if idx in self.pal:
                    ents = ", ".join("{ 1, 0x01, { %d }, 0x%06X, \"\" }" % (dmx, rgb) for dmx, rgb in self.pal[idx])
                    P.append("{ { %s }, { 0 } }" % ents)
                else:
                    P.append("{ { { 0 } }, { 0 } }")
            L.append("    .palette = { %s }," % ", ".join(P))
        f = self.flags
        flagbits = ["use_gm = 1"] + ["%s = %d" % (k, f[k]) for k in ("pt16", "pt8", "pan_ofs", "tilt_ofs", "cmy_on", "rgb_on") if k in f]
        if "patch" in f:
            flagbits.append("pt_patch_on = 1, .pt_patch = { %d, %d, %d, %d }" % f["patch"])
        if "hsv" in f:
            flagbits.append("hsv = { %d, %d, %d }" % tuple(f["hsv"]))
        if "pt16" in f or "pt8" in f:
            flagbits.append("presets = 1, .macros = 1, .follow_spot = 1")
        L.append("    ." + ", .".join(flagbits) + ",")
        if any(self.default_dmx):
            L.append("    .default_dmx = { %s }," % ", ".join(str(v) for v in self.default_dmx[:self.footprint]))
        L.append("    .level_caption = { %s }," % ", ".join(cstr(s, NAME_CAP["level"]) for s in self.level_caption[:self.footprint]))
        L.append("    .level_invisible = { %s }, .level_disabled = { %s }," % (", ".join(str(v) for v in self.invisible[:self.footprint]), ", ".join(str(v) for v in self.disabled[:self.footprint])))
        if any(self.pair_lsb):
            L.append("    .pair_lsb = { %s }," % ", ".join(str(v) for v in self.pair_lsb[:self.footprint]))
        L.append("    .notes = %s," % cstr("Converted from QLC+ %s (%s). Tier 2: control windows derived by the Generic Fixture 2 rules." % (self.src[0], self.src[1])))
        L.append("};")
        return "\n".join(L)


def convert_all(files=None, commit="unknown"):
    """-> (c text, overflow text, findings). files: optional {name: parsed qxf} for the selftest."""
    rows = read_map()
    exe = exe_rows()
    findings, profiles, extras, seen = [], [], [], set()
    cache = dict(files or {})
    for fname, mode, pid in rows:
        if pid in seen:
            findings.append("%s %s: id %d mapped twice" % (fname, mode, pid))
        seen.add(pid)
        if pid < EXTRA and pid not in exe:
            findings.append("%s %s: id %d has no exe row" % (fname, mode, pid))
        if fname not in cache:
            path = os.path.join(QXF_DIR, fname)
            if not os.path.isfile(path):
                findings.append("%s: not in third_party/qlcplus-fixtures" % fname); continue
            cache[fname] = load_qxf(path)
        q = cache[fname]
        if mode not in q["modes"]:
            findings.append("%s: no mode %r (has %s)" % (fname, mode, ", ".join(sorted(q["modes"])))); continue
        chans = q["modes"][mode]
        if len(chans) > CH:
            findings.append("%s %s: %d channels > %d" % (fname, mode, len(chans), CH)); continue
        if pid < EXTRA:
            tag, man, name, md = exe[pid]
        else:
            tag, man, name, md = q["model"].upper(), q["manufacturer"], q["model"], mode
            extras.append((pid, tag, man, name, md))
        ident = {"id": pid, "tag": tag, "manufacturer": man, "name": name, "mode": md}
        profiles.append(Profile(ident, chans, (fname, mode, commit, q["author"])))
    profiles.sort(key=lambda p: p.ident["id"])
    extras.sort()
    h = hashlib.sha256(open(MAP, "rb").read()).hexdigest()[:16]
    out = ["/* lj_profiles_gen.c - generated by tools/qxf2c.py; do not edit.",
           " * Inputs: tools/qxf_map.tsv (sha256 %s), third_party/qlcplus-fixtures (*.qxf) at QLC+ commit %s" % (h, commit),
           " * (Apache-2.0, see third_party/qlcplus-fixtures/LICENSE and NOTICE). One static lj_profile per mapped mode, tier 2:",
           " * identity = the exe row the map names (extras >= 1000 keep the QLC+ names); windows = NULL until lj_layout_derive()",
           " * runs at the first lj_fixlib_by_id() (6.3). Regenerate: python -I tools/qxf2c.py; verify: --check. */",
           '#include "fixlib/lj_profiles_gen.h"', ""]
    out += [p.render() + "\n" for p in profiles]
    out.append("lj_profile *const lj_qxf_profiles[] = { %s };" % (", ".join("&lj_qxf_%d" % p.ident["id"] for p in profiles) or "NULL"))
    out.append("const int lj_qxf_count = %d;" % len(profiles))
    out.append("const lj_exe_row lj_qxf_extra_rows[] = { %s };" % (", ".join("{ %d, %s, %s, %s, %s, \"\", LJ_BIND_QXF }" % (i, cstr(t, 15), cstr(m, 30), cstr(n, 60), cstr(d, 30)) for i, t, m, n, d in extras) or "{ 0, \"\", \"\", \"\", \"\", \"\", LJ_BIND_NONE }"))
    out.append("const int lj_qxf_extra_count = %d;" % len(extras))
    ov = ["# qxf2c.py: wheels with more than %d palette candidates, first %d kept (ME-26). Regenerated with lj_profiles_gen.c." % (PAL, PAL)]
    for p in profiles:
        for chname, n in p.overflow:
            ov.append("%s\t%s\t%d\t%s\t%d" % (p.src[0], p.src[1], p.ident["id"], chname, n))
    return "\n".join(out) + "\n", "\n".join(ov) + "\n", findings


def commit_sha():
    src = os.path.join(QXF_DIR, "SOURCE")
    if os.path.isfile(src):
        m = re.search(r"\b[0-9a-f]{40}\b", open(src, encoding="ascii").read())
        if m:
            return m.group(0)
    return "unknown"


def selftest():
    mac = load_qxf(os.path.join(QXF_DIR, "Martin-MAC500.qxf"))
    p = Profile({"id": 12, "tag": "MAC500", "manufacturer": "Martin", "name": "MAC 500", "mode": "Mode 2/4"}, mac["modes"]["DMX4"], ("Martin-MAC500.qxf", "DMX4", "x", "t"))
    assert p.footprint == 16 and p.cls[0] == "LJ_CH_SHUTTER" and p.cls[1] == "LJ_CH_INTENSITY" and p.cls[10] == "LJ_CH_PAN" and p.cls[15] == "LJ_CH_SPEED", p.cls
    assert p.dyn["shutter"] == (0, "Shutter", 0, 20) and p.dyn["strobe"][2:] == (50, 72) and p.flags["pt16"] == 1 and "patch" not in p.flags
    assert p.sfunc["lamp_on"] == (0, 228) and p.sfunc["reset"] == (0, 208) and p.sfunc["blackout"] == (0, 0)
    assert len(p.pal[0]) == PAL and p.overflow and p.overflow[0][0] == "Color1"
    assert p.pal[0][0] == (0, 0xFFFFFF) and p.pal[0][1] == (1, 0xFFFFFF)       # ColorDoubleMacro keeps its first colour
    synthetic = ('<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition"><Manufacturer>Test</Manufacturer><Model>Synth</Model>'
                 '<Channel Name="Dim" Preset="IntensityDimmer"/><Channel Name="Pan" Preset="PositionPan"/><Channel Name="Tilt" Preset="PositionTilt"/>'
                 '<Channel Name="PanF" Preset="PositionPanFine"/><Channel Name="TiltF" Preset="PositionTiltFine"/>'
                 '<Channel Name="Wheel"><Group Byte="0">Colour</Group>' + "".join('<Capability Min="%d" Max="%d" Preset="ColorMacro" Res1="#0000%02x">c%d</Capability>' % (i * 10, i * 10 + 9, i, i) for i in range(25)) + '</Channel>'
                 '<Channel Name="Focus" Preset="BeamFocusFarNear"/><Channel Name="Nix" Preset="NoFunction"/>'
                 '<Mode Name="M"><Channel Number="0">Dim</Channel><Channel Number="1">Pan</Channel><Channel Number="2">Tilt</Channel><Channel Number="3">PanF</Channel>'
                 '<Channel Number="4">TiltF</Channel><Channel Number="5">Wheel</Channel><Channel Number="6">Focus</Channel><Channel Number="7">Nix</Channel></Mode></FixtureDefinition>')
    tmp = os.path.join(os.environ.get("TEMP", os.environ.get("TMPDIR", "/tmp")), "lj_qxf2c_selftest.qxf")
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(synthetic)
    q = load_qxf(tmp)
    os.remove(tmp)
    ident = {"id": 1999, "tag": "SYNTH", "manufacturer": "Test", "name": "Synth", "mode": "M"}
    a = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", "")).render()
    b = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", "")).render()
    assert a == b, "conversion is not deterministic"
    s = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", ""))
    assert s.flags["patch"] == (1, 3, 2, 4) and s.flags["pt16"] == 1, s.flags       # non-adjacent LSBs -> pan/tilt patch
    assert s.dyn["focus"] == (6, "Focus", 255, 0) and s.cls[7] == "LJ_CH_NONE" and s.disabled[7] == 1
    assert len(s.pal[0]) == PAL and s.overflow == [("Wheel", 25)]
    assert ".pt_patch = { 1, 3, 2, 4 }" in a and ".focus = { 1, 6, \"Focus\", 255, 0 }" in a
    c1 = convert_all(commit="x")
    c2 = convert_all(commit="x")
    assert c1[0] == c2[0] and c1[1] == c2[1], "convert_all is not deterministic"
    print("qxf2c selftest: ok (MAC500 DMX4 footprint 16, Color1 overflow %d, synthetic patch + reversed focus + NoFunction)" % p.overflow[0][1])
    return 0


def main():
    if "--selftest" in sys.argv:
        return selftest()
    c, ov, findings = convert_all(commit=commit_sha())
    if "--check" in sys.argv:
        for f in findings:
            print(f)
        for path, text in ((OUT, c), (OVERFLOW, ov)):
            if not os.path.isfile(path) or open(path, encoding="latin-1").read().replace("\r\n", "\n") != text:
                findings.append("%s differs from a regeneration (run python -I tools/qxf2c.py)" % os.path.relpath(path, ROOT))
                print(findings[-1])
        print("qxf2c --check: %d finding(s); palette overflow list:" % len(findings))
        sys.stdout.write(ov)
        return 1 if findings else 0
    if findings:
        for f in findings:
            print(f)
        return 1
    for path, text in ((OUT, c), (OVERFLOW, ov)):
        with open(path, "w", encoding="latin-1", newline="\n") as f:
            f.write(text)
        print("wrote %s" % os.path.relpath(path, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
