#!/usr/bin/env python3
"""qxf2c.py - fixture definitions -> src/fixlib/lj_profiles_gen*.c (design 6.1 item 1, verdicts ME-8 and ME-26;
W7 sources, compact descriptors and the authoring lint). Python 3 stdlib only (xml.etree).

    python -I tools/qxf2c.py             # write lj_profiles_gen.c + lj_profiles_gen_<k>.c and tools/qxf2c_overflow.txt
    python -I tools/qxf2c.py --check     # CI: regenerate in memory, exit 1 on drift or a bad map line
    python -I tools/qxf2c.py --selftest  # determinism + the mapping rules over one in-repo QXF and a synthetic one
    python -I tools/qxf2c.py --lint F.qxf [F.qxf ...]   # one authored file standalone: exit 1 and one line per problem
    python -I tools/qxf2c.py --lint                     # every fixtures/authored/<group>/*.qxf (CTest qxf2c_lint_authored)

Inputs: tools/qxf_map.tsv (file, mode, id; tools/qxf_map_sync.py appends to it), src/fixlib/lj_exe_profiles.c (the
ids a map line may name) and the sources a map file names (docs/fixtures.md "W7"):
  Martin-MAC500.qxf                         third_party/qlcplus-fixtures/ (QLC+, Apache-2.0, commit in its SOURCE)
  third_party/ofl-fixtures/martin/x.json    Open Fixture Library (MIT), converted in memory by tools/ofl2qxf.py
  fixtures/authored/<group>/x.qxf           LJ4EVR-authored from the manufacturer's manual (MIT)
Output: per mapped mode one compact byte string (the encoding is the comment above lj_qxf_build in
src/fixlib/lj_profile.c) in a lj_profiles_gen_<k>.c part, one lj_qxf_desc row in lj_profiles_gen.c; lj_fixlib_by_id()
builds the lj_profile on the heap at first use and lj_layout_derive() its windows (tier 2, design 6.3).

The Group + Preset -> class / dyn / palette rules are the table in docs/fixtures.md; this file is their one
implementation. Palettes keep the first 20 candidate capabilities per wheel (LJ_PAL_ENTRIES); every wheel with more
is listed in tools/qxf2c_overflow.txt, which generate-check prints.
"""
import glob
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ofl2qxf  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QXF_DIR = os.path.join(ROOT, "third_party", "qlcplus-fixtures")
OFL_DIR = os.path.join(ROOT, "third_party", "ofl-fixtures")
AUTH_DIR = os.path.join(ROOT, "fixtures", "authored")
MAP = os.path.join(ROOT, "tools", "qxf_map.tsv")
GEN_DIR = os.path.join(ROOT, "src", "fixlib")
OUT = os.path.join(GEN_DIR, "lj_profiles_gen.c")
OVERFLOW = os.path.join(ROOT, "tools", "qxf2c_overflow.txt")
EXE = os.path.join(ROOT, "src", "fixlib", "lj_exe_profiles.c")
NS = "{http://www.qlcplus.org/FixtureDefinition}"
CH = 32            # LJ_PROFILE_CH
PAL = 20           # LJ_PAL_ENTRIES
EXTRA = 1000       # first extra id (ME-8)
PART_MODES = 400   # modes per lj_profiles_gen_<k>.c (MSVC is happy far beyond; keeps each file near 1 MB)
NAME_CAP = {"tag": 15, "manufacturer": 30, "name": 60, "mode": 30, "dyn": 8, "level": 5}

# Capability presets that make a palette entry (plus untyped ones whose text is not a motion word).
PAL_PRESETS = {"ColorMacro", "ColorDoubleMacro", "ColorWheelIndex", "GoboMacro", "PrismEffectOn", "PrismEffectOff", "GenericPicture"}
MOTION_RE = re.compile(r"rotat|\bCW\b|\bCCW\b|shake|random|chase|scroll|spin|strobe|pulse|reset|lamp|macro|stop", re.I)
RGB_PRESET = {"IntensityRed": 0, "IntensityGreen": 1, "IntensityBlue": 2}
CMY_PRESET = {"IntensityCyan": 0, "IntensityMagenta": 1, "IntensityYellow": 2}
HSV_PRESET = {"IntensityHue": 0, "IntensitySaturation": 1, "IntensityValue": 2}


def rotation(caps):
    """a Pan / Tilt group channel that spins (continuous rotation) is not a position channel (W8D-6)."""
    return any((pr or "").startswith("Rotation") or re.search(r"rotat", txt, re.I) for mn, mx, pr, r, txt in caps)

# The W7 groups (docs/fixtures.md): display manufacturer (the name the products carry, OFL's manufacturers.json),
# OFL directory, and the spellings a catalog / authored directory may use (lower case, letters and digits only).
GROUPS = [("Martin", "martin", ["martin"]), ("Elation", "elation", ["elation"]),
          ("Chauvet DJ", "chauvet-dj", ["chauvetdj"]), ("Chauvet Professional", "chauvet-professional", ["chauvetpro", "chauvetprofessional"]),
          ("Clay Paky", "clay-paky", ["claypaky"]), ("GLP", "glp", ["glp"]), ("Acme", "acme", ["acme"]), ("Ayrton", "ayrton", ["ayrton"]),
          ("ETC", "etc", ["etc"]), ("Robe", "robe", ["robe"]), ("High End Systems", "high-end-systems", ["hes", "highendsystems", "highend"]),
          ("Vari-Lite", "vari-lite", ["varilite"]), ("American DJ", "american-dj", ["adj", "americandj"]), ("SGM", "sgm", ["sgm"])]
# Chauvet's QLC+ files all say "Chauvet": these product lines are Chauvet Professional, the rest Chauvet DJ.
CHAUVET_PRO_RE = re.compile(r"^(?:Chauvet\s+)?(?:COLORado|Maverick|Ovation|Rogue|Legend|STRIKE|Color\s*STRIKE|onAir|Epix|Nexus|Well|Amhaze|Professional)\b", re.I)


def alnum(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def group_of(alias):
    """a group spelling (directory, catalog stem, manufacturer string) -> display manufacturer, or None."""
    a = alnum(alias)
    for display, ofl, aliases in GROUPS:
        if a == alnum(display) or a == alnum(ofl) or a in aliases:
            return display
    return None


def manufacturer_of(q, fname):
    """the display manufacturer of a parsed source file (the tree folder), or None outside the W7 groups."""
    if fname.startswith("fixtures/authored/"):
        return group_of(fname.split("/")[2])
    m = q["manufacturer"]
    if alnum(m) == "chauvet":
        return "Chauvet Professional" if CHAUVET_PRO_RE.match(q["model"]) else "Chauvet DJ"
    return group_of(m)


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
        elif c == 0x3F:
            out.append("\\077")                              # no '??' trigraph pairs
        elif 0x20 <= c < 0x7F:
            out.append(chr(c))
        else:
            out.append("\\%03o" % c)
    return '"' + "".join(out) + '"'


def parse_qxf(root):
    chans = {}
    for ch in root.findall(NS + "Channel"):
        g = ch.find(NS + "Group")
        caps = [(int(c.get("Min")), int(c.get("Max")), c.get("Preset"), c.get("Res1"), (c.text or "").strip())
                for c in ch.findall(NS + "Capability")]
        chans[ch.get("Name")] = {"name": ch.get("Name"), "group": g.text if g is not None else None,
                                 "byte": int(g.get("Byte", "0")) if g is not None else 0, "preset": ch.get("Preset"),
                                 "default": int(ch.get("Default", "0")), "caps": caps}
    modes, heads = {}, {}
    for m in root.findall(NS + "Mode"):
        rows = sorted((int(c.get("Number")), c.text) for c in m.findall(NS + "Channel"))
        modes[m.get("Name")] = [chans[name] for _, name in rows]
        heads[m.get("Name")] = [{int(c.text) for c in h.findall(NS + "Channel")} for h in m.findall(NS + "Head")]
    text = lambda tag: (root.findtext(NS + tag) or "").strip()
    author = root.find(NS + "Creator")
    author = (author.findtext(NS + "Author") or "").strip() if author is not None else ""
    return {"manufacturer": text("Manufacturer"), "model": text("Model"), "author": author, "modes": modes, "heads": heads,
            "mode_order": [m.get("Name") for m in root.findall(NS + "Mode")]}


def load_qxf(path):
    return parse_qxf(ET.parse(path).getroot())


def commit_of(src_file):
    if os.path.isfile(src_file):
        m = re.search(r"\b[0-9a-f]{40}\b", open(src_file, encoding="ascii").read())
        if m:
            return m.group(0)
    return "unknown"


_ofl_names = None


def ofl_manufacturer(d):
    global _ofl_names
    if _ofl_names is None:
        with open(os.path.join(OFL_DIR, "manufacturers.json"), encoding="utf-8") as f:
            _ofl_names = {k: v["name"] for k, v in json.load(f).items() if isinstance(v, dict) and "name" in v}
    return _ofl_names.get(d, d)


def source_path(fname):
    """map file column -> path: a bare name is a QLC+ file, anything with '/' is relative to the repo root."""
    return os.path.join(QXF_DIR, fname) if "/" not in fname else os.path.join(ROOT, *fname.split("/"))


def source_kind(fname):
    return "authored" if fname.startswith("fixtures/authored/") else "ofl" if fname.endswith(".json") else "qlc"


def source_label(fname):
    k = source_kind(fname)
    if k == "qlc":
        return "QLC+ " + fname
    if k == "ofl":
        return "Open Fixture Library " + fname[len("third_party/ofl-fixtures/"):]
    return "LJ4EVR " + fname


def load_source(fname, problems=None):
    """-> parsed fixture dict (load_qxf shape) for a map file column; OFL JSON goes through ofl2qxf in memory."""
    path = source_path(fname)
    if fname.endswith(".json"):
        d = fname.split("/")[-2]
        with open(path, encoding="utf-8") as f:
            text, probs = ofl2qxf.convert(json.load(f), ofl_manufacturer(d), fname[len("third_party/ofl-fixtures/"):], commit_of(os.path.join(OFL_DIR, "SOURCE")))
        if problems is not None:
            problems.extend(probs)
        return parse_qxf(ET.fromstring(text.encode("utf-8")))
    return load_qxf(path)


def all_sources():
    """every source file a map line may name, in precedence order (authored, OFL, QLC+), sorted inside each."""
    rel = lambda p: os.path.relpath(p, ROOT).replace(os.sep, "/")
    auth = sorted(rel(p) for p in glob.glob(os.path.join(AUTH_DIR, "*", "*.qxf")))
    ofl = sorted(rel(p) for p in glob.glob(os.path.join(OFL_DIR, "*", "*.json")))
    qlc = sorted(os.path.basename(p) for p in glob.glob(os.path.join(QXF_DIR, "*.qxf")))
    return auth + ofl + qlc


def read_map():
    rows = []
    for ln, line in enumerate(open(MAP, encoding="utf-8"), 1):
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
    def __init__(self, ident, chans, src, heads=()):
        self.ident, self.src, self.heads = ident, src, heads    # heads: the mode's QLC+ <Head> channel sets (offsets)
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
        self.ext_gm = [0] * CH
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
        pos = []               # positional pan / tilt channels (ofs, is_pan, fine), bound after the loop
        dimmers = []           # coarse dimmer channels (ofs, preset), intensity bound after the loop
        for ofs, c in enumerate(chans):
            p, g, name, caps = c["preset"] or "", c["group"] or "", c["name"], c["caps"]
            fine = c["byte"] == 1 or p.endswith("Fine")
            self.level_caption[ofs] = name
            cls = "LJ_CH_LEVEL"
            if p == "NoFunction" or g == "Nothing":
                cls = "LJ_CH_NONE"; self.invisible[ofs] = 1; self.disabled[ofs] = 1
            elif re.match(r"Position(Pan|Tilt)(Fine)?$", p) or (g in ("Pan", "Tilt") and not p and not rotation(caps)):   # W8D-6: positional only
                is_pan = p.startswith("PositionPan") or g == "Pan"
                cls = "LJ_CH_PAN" if is_pan else "LJ_CH_TILT"
                pos.append((ofs, is_pan, fine))
                self.invisible[ofs] = 1
            elif p in RGB_PRESET or p in CMY_PRESET:                 # W8D-5 (e, f): the first kind and the first triple bind, the rest stay Levels
                k = RGB_PRESET.get(p, CMY_PRESET.get(p))
                kind, other = ("rgb_on", "cmy_on") if p in RGB_PRESET else ("cmy_on", "rgb_on")
                if self.ident["id"] == 170 or (other not in self.flags and "cmy[%d]" % k not in self.dyn):   # id 170 (MAC 401 Dual RGB-1) keeps its binding (W8D-3)
                    self.flags[kind] = 1
                    self.setdyn("cmy[%d]" % k, ofs, name, 0, 255)
            elif p in HSV_PRESET:
                self.flags.setdefault("hsv", [0, 0, 0])[HSV_PRESET[p]] = 1
            elif p.startswith("Intensity") or g == "Intensity":
                if fine:
                    cls = "LJ_CH_INTENSITY"
                elif p in ("IntensityDimmer", "IntensityMasterDimmer") or not p:
                    dimmers.append((ofs, p))
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
        self.bind_intensity(chans, dimmers)
        pan, tilt, pan_fine, tilt_fine = self.bind_position(pos)
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

    def bind_intensity(self, chans, dimmers):
        """W8D-6: intensity on an IntensityMasterDimmer outside every head, else on the first dimmer; every other coarse
        dimmer (an IntensityDimmer or IntensityMasterDimmer preset) gets ext_gm, so the Grand Master and blackout reach every
        head [RN L.459]. An untyped Intensity-group channel (a fan, a QLC+ 3 colour) can take the intensity, never ext_gm."""
        in_head = set().union(*self.heads)
        master = [d for d in dimmers if d[1] == "IntensityMasterDimmer" and d[0] not in in_head]
        pick = (master or dimmers or [None])[0]
        for d in dimmers:
            if d is pick:
                self.cls[d[0]] = "LJ_CH_INTENSITY"
                self.setdyn("intensity", d[0], chans[d[0]]["name"], 0, 255)
            elif d[1]:
                self.ext_gm[d[0]] = 1

    def bind_position(self, pos):
        """W8D-6 and Q6 (fidelity row W8-8): the first positional coarse pan and tilt (head 1) drive Movement, each with
        the first fine after it; every other positional channel is a Levels fader ("Pan2", "Tilt2" when an axis
        repeats) with its fine paired to it. A lone pan or tilt binds nothing and stays visible."""
        coarse = {a: [o for o, ip, f in pos if ip == a and not f] for a in (True, False)}
        bound = {a: coarse[a][0] for a in coarse} if coarse[True] and coarse[False] else {}
        fine_of, last = {}, {}                                    # fine ofs -> the coarse ofs it follows
        for o, a, f in pos:
            if not f:
                last[a] = o
            elif a in last and last[a] not in fine_of.values():
                fine_of[o] = last[a]
        bfine = {a: next((o for o, c in fine_of.items() if c == bound.get(a)), None) for a in (True, False)}
        for o, a, f in pos:
            if o == bound.get(a) or o == bfine[a]:
                continue
            self.cls[o] = "LJ_CH_LEVEL"
            if f and o in fine_of:
                self.pair_lsb[fine_of[o]] = 1 + o
                continue
            self.invisible[o] = 0
            if not f and len(coarse[a]) > 1:
                self.level_caption[o] = "%s%d" % ("Pan" if a else "Tilt", coarse[a].index(o) + 1)
        return bound.get(True), bound.get(False), bfine[True], bfine[False]

    # dyn slots in the order of lj_qxf_build's table (src/fixlib/lj_profile.c)
    DYN_SLOTS = ["shutter", "strobe", "intensity", "color[0]", "color[1]", "color[2]", "color[3]", "cmy[0]", "cmy[1]", "cmy[2]",
                 "gobo[0]", "gobo[1]", "gobo[2]", "gobo[3]", "iris", "focus", "zoom", "frost", "beam[0]", "beam[1]",
                 "effects[0]", "effects[1]", "effects[2]", "effects[3]"]
    SFUNCS = ["blackout", "lamp_on", "lamp_off", "reset"]

    def encode(self):
        """the byte string lj_qxf_build decodes (format: the comment above it in lj_profile.c)."""
        b = [self.footprint]
        for ofs in range(self.footprint):
            cap = cp1252(self.level_caption[ofs])[:NAME_CAP["level"]]
            cls = self.cls[ofs]
            b += [CLASSES.index(cls), self.invisible[ofs] | self.disabled[ofs] << 1 | self.ext_gm[ofs] << 2, self.default_dmx[ofs], self.pair_lsb[ofs], len(cap)] + list(cap)
        b.append(len(self.dyn))
        for key in sorted(self.dyn, key=self.DYN_SLOTS.index):
            ofs, name, lo, hi = self.dyn[key]
            nm = cp1252(name)[:NAME_CAP["dyn"]]
            b += [self.DYN_SLOTS.index(key), ofs, lo, hi, len(nm)] + list(nm)
        f = self.flags
        hsv = f.get("hsv", [0, 0, 0])
        b.append(f.get("pt16", 0) | f.get("pt8", 0) << 1 | f.get("cmy_on", 0) << 2 | f.get("rgb_on", 0) << 3 | ("patch" in f) << 4
                 | hsv[0] << 5 | hsv[1] << 6 | hsv[2] << 7)
        b += [f.get("pan_ofs", 0), f.get("tilt_ofs", 0)] + list(f.get("patch", (0, 0, 0, 0)))
        b += [1, f["strobe_off"]] if "strobe_off" in f else [0, 0]
        sf = [k for k in self.SFUNCS if k in self.sfunc]
        b.append(len(sf))
        for k in sf:
            b += [self.SFUNCS.index(k)] + list(self.sfunc[k])
        b.append(len(self.pal))
        for idx in sorted(self.pal):
            b += [idx, len(self.pal[idx])]
            for dmx, rgb in self.pal[idx]:
                b += [dmx, rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255]
        assert all(0 <= v <= 255 for v in b), b
        return b


CLASSES = ["LJ_CH_NONE", "LJ_CH_INTENSITY", "LJ_CH_SHUTTER", "LJ_CH_STROBE", "LJ_CH_PAN", "LJ_CH_TILT", "LJ_CH_COLOR", "LJ_CH_GOBO",
           "LJ_CH_BEAM", "LJ_CH_EFFECT", "LJ_CH_LEVEL", "LJ_CH_SPEED", "LJ_CH_LAMP", "LJ_CH_SPECIAL"]    # enum lj_ch_class order


def asc(s):
    return s.encode("ascii", "replace").decode("ascii").replace("*/", "* /")   # comments stay ASCII: cl /utf-8 rejects stray bytes


def convert_all(files=None):
    """-> ({path: c text}, overflow text, findings). files: optional {name: parsed qxf} for the selftest."""
    rows = read_map()
    exe = exe_rows()
    findings, profiles, seen = [], [], set()
    cache = dict(files or {})
    commits = {"qlc": commit_of(os.path.join(QXF_DIR, "SOURCE")), "ofl": commit_of(os.path.join(OFL_DIR, "SOURCE")), "authored": "-"}
    licence = {"qlc": "Apache-2.0", "ofl": "MIT", "authored": "MIT"}
    for fname, mode, pid in rows:
        if pid in seen:
            findings.append("%s %s: id %d mapped twice" % (fname, mode, pid))
        seen.add(pid)
        if pid < EXTRA and pid not in exe:
            findings.append("%s %s: id %d has no exe row" % (fname, mode, pid))
        if fname not in cache:
            if not os.path.isfile(source_path(fname)):
                findings.append("%s: no such source file" % fname); continue
            cache[fname] = load_source(fname)
        q = cache[fname]
        if mode not in q["modes"]:
            findings.append("%s: no mode %r (has %s)" % (fname, mode, ", ".join(sorted(q["modes"])))); continue
        chans = q["modes"][mode]
        if len(chans) > CH:
            findings.append("%s %s: %d channels > %d" % (fname, mode, len(chans), CH)); continue
        if pid < EXTRA:
            tag, man, name, md = exe[pid]
        else:
            man = manufacturer_of(q, fname) or q["manufacturer"]
            tag, name, md = q["model"].upper(), q["model"], mode
        ident = {"id": pid, "tag": tag, "manufacturer": man, "name": name, "mode": md}
        k = source_kind(fname)
        profiles.append(Profile(ident, chans, (fname, mode, commits[k], q["author"], licence[k]), q["heads"].get(mode, [])))
    profiles.sort(key=lambda p: p.ident["id"])
    h = hashlib.sha256(open(MAP, "rb").read()).hexdigest()[:16]
    parts, descs, extras = [], [], []
    for k in range((len(profiles) + PART_MODES - 1) // PART_MODES):
        L = ["/* lj_profiles_gen_%d.c - generated by tools/qxf2c.py; do not edit. Part %d of the tier-2 profile bytes:" % (k, k),
             " * one encoded profile per mapped mode (format: the comment above lj_qxf_build in src/fixlib/lj_profile.c),",
             " * each with its source, licence and \"modified: converted\". */",
             '#include "fixlib/lj_profiles_gen.h"', "", "const uint8_t lj_qxf_part_%d[] = {" % k]
        at = 0
        for p in profiles[k * PART_MODES:(k + 1) * PART_MODES]:
            o, src = p.ident, p.src
            L.append("    /* id %d at %d: %s mode %s, %s at %s, %s, modified: converted (author %s) */"
                     % (o["id"], at, asc(source_label(src[0])), asc(src[1]), source_kind(src[0]), src[2][:12], src[4], asc(src[3] or "unknown")))
            enc = p.encode()
            for i in range(0, len(enc), 24):
                L.append("    " + ", ".join(str(v) for v in enc[i:i + 24]) + ",")
            descs.append((o, k, at, p.src))
            at += len(enc)
        L.append("};")
        parts.append("\n".join(L) + "\n")
        if at > 0xFFFFFF:
            findings.append("part %d is %d bytes" % (k, at))
    for o, k, at, src in descs:
        if o["id"] >= EXTRA:
            extras.append(o)
    out = ["/* lj_profiles_gen.c - generated by tools/qxf2c.py; do not edit.",
           " * Inputs: tools/qxf_map.tsv (sha256 %s), third_party/qlcplus-fixtures (QLC+ %s, Apache-2.0)," % (h, commits["qlc"]),
           " * third_party/ofl-fixtures (Open Fixture Library %s, MIT), fixtures/authored (LJ4EVR, MIT); see the" % commits["ofl"],
           " * LICENSE / NOTICE / SOURCE files there. One lj_qxf_desc per mapped mode, sorted by id, its bytes in the parts",
           " * lj_profiles_gen_<k>.c; lj_fixlib_by_id() builds the lj_profile on the heap at first use, identity = the exe",
           " * row the map names (extras >= 1000 keep the source's names under the group manufacturer), windows from",
           " * lj_layout_derive() (6.3). Regenerate: python -I tools/qxf2c.py; verify: --check. */",
           '#include "fixlib/lj_profiles_gen.h"', ""]
    out += ["extern const uint8_t lj_qxf_part_%d[];" % k for k in range(len(parts))]
    out.append("const uint8_t *const lj_qxf_parts[] = { %s };" % (", ".join("lj_qxf_part_%d" % k for k in range(len(parts))) or "NULL"))
    out.append("")
    out.append("const lj_qxf_desc lj_qxf_descs[] = {")
    for o, k, at, src in descs:
        out.append("    { %d, %d, %d, %s, %s, %s, %s, %s, %s }," % (o["id"], k, at, cstr(o["tag"], NAME_CAP["tag"]), cstr(o["manufacturer"], NAME_CAP["manufacturer"]),
                                                              cstr(o["name"], NAME_CAP["name"]), cstr(o["mode"], NAME_CAP["mode"]), cstr(source_label(src[0])), cstr(src[1])))
    if not descs:
        out.append("    { 0, 0, 0, \"\", \"\", \"\", \"\", \"\", \"\" },")
    out.append("};")
    out.append("const int lj_qxf_count = %d;" % len(descs))
    out.append("lj_profile *lj_qxf_built[%d];" % max(1, len(descs)))
    out.append("")
    out.append("const lj_exe_row lj_qxf_extra_rows[] = {")
    for o in extras:
        out.append("    { %d, %s, %s, %s, %s, \"\", LJ_BIND_QXF }," % (o["id"], cstr(o["tag"], 15), cstr(o["manufacturer"], 30), cstr(o["name"], 60), cstr(o["mode"], 30)))
    if not extras:
        out.append("    { 0, \"\", \"\", \"\", \"\", \"\", LJ_BIND_NONE },")
    out.append("};")
    out.append("const int lj_qxf_extra_count = %d;" % len(extras))
    files_out = {OUT: "\n".join(out) + "\n"}
    for k, text in enumerate(parts):
        files_out[os.path.join(GEN_DIR, "lj_profiles_gen_%d.c" % k)] = text
    ov = ["# qxf2c.py: wheels with more than %d palette candidates, first %d kept (ME-26). Regenerated with lj_profiles_gen.c." % (PAL, PAL)]
    for p in profiles:
        for chname, n in p.overflow:
            ov.append("%s\t%s\t%d\t%s\t%d" % (p.src[0], p.src[1], p.ident["id"], chname, n))
    return files_out, "\n".join(ov) + "\n", findings


# ---- --lint: one authored file standalone ----
LINT_GROUPS = {"Intensity", "Colour", "Gobo", "Pan", "Tilt", "Shutter", "Beam", "Prism", "Effect", "Speed", "Maintenance", "Nothing"}
LINT_CHANNEL_CHILD = {"Group", "Capability", "Colour"}
LINT_TOP = {"Creator", "Manufacturer", "Model", "Type", "Channel", "Mode", "Physical"}


def lint(path):
    """-> [problem] for one .qxf (an authored file: fixtures/authored/README.md)."""
    P = []
    try:
        raw = open(path, "rb").read()
        root = ET.fromstring(raw)
    except (OSError, ET.ParseError) as e:
        return ["%s: not well-formed XML: %s" % (path, e)]
    if root.tag != NS + "FixtureDefinition":
        return ["%s: root element is %s, not FixtureDefinition in the QLC+ namespace" % (path, root.tag)]
    for el in root:
        if el.tag.replace(NS, "") not in LINT_TOP:
            P.append("%s: unknown element <%s>" % (path, el.tag.replace(NS, "")))
    for tag in ("Manufacturer", "Model"):
        if not (root.findtext(NS + tag) or "").strip():
            P.append("%s: <%s> missing or empty" % (path, tag))
    cr = root.find(NS + "Creator")
    if cr is None or not (cr.findtext(NS + "Author") or "").strip() or not (cr.findtext(NS + "Name") or "").strip():
        P.append("%s: <Creator> needs <Name> and <Author> (the manual citation, README)" % path)
    if not re.search(rb"https?://(?!www\.qlcplus\.org/FixtureDefinition)", raw):
        P.append("%s: no manual URL (cite the manual's title, revision and URL in <Creator> and a comment)" % path)
    chans = {}
    for ch in root.findall(NS + "Channel"):
        name = ch.get("Name")
        where = "%s: channel %r" % (path, name)
        if not name:
            P.append("%s: a <Channel> without Name" % path); continue
        if name in chans:
            P.append("%s: defined twice" % where)
        g = ch.find(NS + "Group")
        byte = 0
        if g is not None:
            if (g.text or "").strip() not in LINT_GROUPS:
                P.append("%s: unknown Group %r" % (where, g.text))
            if g.get("Byte", "0") not in ("0", "1"):
                P.append("%s: Group Byte %r is not 0 or 1" % (where, g.get("Byte")))
            byte = 1 if g.get("Byte") == "1" else 0
        elif not ch.get("Preset"):
            P.append("%s: neither Group nor Preset" % where)
        for el in ch:
            if el.tag.replace(NS, "") not in LINT_CHANNEL_CHILD:
                P.append("%s: unknown element <%s>" % (where, el.tag.replace(NS, "")))
        d = ch.get("Default", "0")
        if not d.isdigit() or int(d) > 255:
            P.append("%s: Default %r outside 0-255" % (where, d))
        last = -1
        for c in ch.findall(NS + "Capability"):
            mn, mx = c.get("Min", ""), c.get("Max", "")
            if not (mn.isdigit() and mx.isdigit()):
                P.append("%s: capability %r Min/Max not integers" % (where, (c.text or "").strip())); continue
            mn, mx = int(mn), int(mx)
            if not (0 <= mn <= mx <= 255):
                P.append("%s: capability %d-%d outside 0-255 or reversed" % (where, mn, mx))
            if mn <= last:
                P.append("%s: capability %d-%d overlaps or precedes the one ending at %d" % (where, mn, mx, last))
            last = max(last, mx)
            if c.get("Res1") and c.get("Preset") in ("ColorMacro", "ColorDoubleMacro") and not re.match(r"^#[0-9a-fA-F]{6}$", c.get("Res1")):
                P.append("%s: capability %d-%d Res1 %r is not #rrggbb" % (where, mn, mx, c.get("Res1")))
        fine = byte == 1 or (ch.get("Preset") or "").endswith("Fine")
        chans[name] = (g.text.strip() if g is not None and g.text else None, fine, ch.get("Preset") or "")
    modes = root.findall(NS + "Mode")
    if not modes:
        P.append("%s: no <Mode>" % path)
    seen_modes = set()
    for m in modes:
        mname = m.get("Name")
        where = "%s: mode %r" % (path, mname)
        if not mname or mname in seen_modes:
            P.append("%s: missing or duplicate mode name" % where)
        seen_modes.add(mname)
        nums, names = [], []
        for c in m.findall(NS + "Channel"):
            n = c.get("Number", "")
            if not n.isdigit():
                P.append("%s: channel Number %r" % (where, n)); continue
            nums.append(int(n))
            ref = (c.text or "").strip()
            if ref not in chans:
                P.append("%s: channel %s refers to undefined %r" % (where, n, ref))
            names.append((int(n), ref))
        if sorted(nums) != list(range(len(nums))):
            P.append("%s: channel numbers are not 0..%d, each once" % (where, len(nums) - 1))
        if len(nums) > CH:
            print("%s: note: %d channels > %d, not shipped (listed in docs/fixtures/coverage.md)" % (where, len(nums), CH))
        coarse = set()
        for _, ref in sorted(names):
            if ref not in chans:
                continue
            g, fine, preset = chans[ref]
            if fine:
                want = preset[:-4] if preset.endswith("Fine") else None
                ok = any((want and chans[r][2] == want) or (not want and chans[r][0] == g and not chans[r][1]) for r in coarse)
                if not ok:
                    P.append("%s: 16-bit fine channel %r has no coarse %s channel before it" % (where, ref, want or g))
            else:
                coarse.add(ref)
    return P


def selftest():
    mac = load_qxf(os.path.join(QXF_DIR, "Martin-MAC500.qxf"))
    p = Profile({"id": 12, "tag": "MAC500", "manufacturer": "Martin", "name": "MAC 500", "mode": "Mode 2/4"}, mac["modes"]["DMX4"], ("Martin-MAC500.qxf", "DMX4", "x", "t", "Apache-2.0"))
    assert p.footprint == 16 and p.cls[0] == "LJ_CH_SHUTTER" and p.cls[1] == "LJ_CH_INTENSITY" and p.cls[10] == "LJ_CH_PAN" and p.cls[15] == "LJ_CH_SPEED", p.cls
    assert p.dyn["shutter"] == (0, "Shutter", 0, 20) and p.dyn["strobe"][2:] == (50, 72) and p.flags["pt16"] == 1 and "patch" not in p.flags
    assert p.sfunc["lamp_on"] == (0, 228) and p.sfunc["reset"] == (0, 208) and p.sfunc["blackout"] == (0, 0)
    assert len(p.pal[0]) == PAL and p.overflow and p.overflow[0][0] == "Color1"
    assert p.pal[0][0] == (0, 0xFFFFFF) and p.pal[0][1] == (1, 0xFFFFFF)       # ColorDoubleMacro keeps its first colour
    e = p.encode()
    assert e[0] == 16 and e[1:6] == [CLASSES.index("LJ_CH_SHUTTER"), 1, 0, 0, 5] and bytes(e[6:11]) == b"Shutt", e[:12]
    synthetic = ('<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition"><Manufacturer>Test</Manufacturer><Model>Synth</Model>'
                 '<Creator><Name>LJ4EVR from Synth manual rev A https://example.invalid/synth.pdf</Name><Author>t</Author></Creator>'
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
    lint_ok = lint(tmp)
    with open(tmp, "w", encoding="utf-8") as f:                 # the lint's problem kinds, one each
        f.write(synthetic.replace('Min="0" Max="9"', 'Min="0" Max="300"').replace('<Channel Number="3">PanF</Channel>', '<Channel Number="3">Gone</Channel>')
                .replace("https://example.invalid/synth.pdf", "").replace('<Channel Name="Dim" Preset="IntensityDimmer"/>', '<Channel Name="Dim" Preset="IntensityDimmer"/><Channel Name="F2"><Group Byte="1">Gobo</Group></Channel>')
                .replace('<Channel Number="7">Nix</Channel>', '<Channel Number="7">Nix</Channel><Channel Number="9">F2</Channel>'))
    lint_bad = lint(tmp)
    os.remove(tmp)
    assert lint_ok == [], lint_ok
    kinds = ["no manual URL", "outside 0-255", "overlaps", "undefined 'Gone'", "not 0..8", "16-bit fine channel 'F2'"]
    for k in kinds:
        assert any(k in x for x in lint_bad), (k, lint_bad)
    ident = {"id": 1999, "tag": "SYNTH", "manufacturer": "Test", "name": "Synth", "mode": "M"}
    a = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", "", "MIT")).encode()
    b = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", "", "MIT")).encode()
    assert a == b, "conversion is not deterministic"
    s = Profile(ident, q["modes"]["M"], ("s.qxf", "M", "x", "", "MIT"))
    assert s.flags["patch"] == (1, 3, 2, 4) and s.flags["pt16"] == 1, s.flags       # non-adjacent LSBs -> pan/tilt patch
    assert s.dyn["focus"] == (6, "Focus", 255, 0) and s.cls[7] == "LJ_CH_NONE" and s.disabled[7] == 1
    assert len(s.pal[0]) == PAL and s.overflow == [("Wheel", 25)]
    assert manufacturer_of({"manufacturer": "Chauvet", "model": "Rogue R2 Wash"}, "x.qxf") == "Chauvet Professional"
    assert manufacturer_of({"manufacturer": "Chauvet", "model": "Intimidator Spot 355"}, "x.qxf") == "Chauvet DJ"
    assert manufacturer_of({"manufacturer": "x", "model": "y"}, "fixtures/authored/hes/a.qxf") == "High End Systems" and group_of("ADJ") == "American DJ"
    # W8D-5 (e, f), W8D-6, Q6: a two-head mover with a spinning pan, a CMY channel after RGB and a trailing master
    ch = lambda n, pr: '<Channel Name="%s" Preset="%s"/>' % (n, pr)
    names = [("Pan1", "PositionPan"), ("PanF1", "PositionPanFine"), ("Tilt1", "PositionTilt"), ("TiltF1", "PositionTiltFine"), ("Spin", ""),
             ("R1", "IntensityRed"), ("G1", "IntensityGreen"), ("B1", "IntensityBlue"), ("Dim1", "IntensityDimmer"), ("Pan2", "PositionPan"),
             ("PanF2", "PositionPanFine"), ("Tilt2", "PositionTilt"), ("R2", "IntensityRed"), ("G2", "IntensityGreen"), ("B2", "IntensityBlue"),
             ("Cyan", "IntensityCyan"), ("Dim2", "IntensityDimmer"), ("Master", "IntensityMasterDimmer")]
    two = ('<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition"><Manufacturer>Test</Manufacturer><Model>Two</Model>'
           + "".join(ch(n, pr) for n, pr in names if pr)
           + '<Channel Name="Spin"><Group Byte="0">Pan</Group><Capability Min="0" Max="255" Preset="RotationClockwise">CW</Capability></Channel>'
           + '<Mode Name="M">' + "".join('<Channel Number="%d">%s</Channel>' % (i, n) for i, (n, pr) in enumerate(names))
           + '<Head>' + "".join('<Channel>%d</Channel>' % i for i in (0, 1, 2, 3, 5, 6, 7, 8)) + '</Head>'
           + '<Head>' + "".join('<Channel>%d</Channel>' % i for i in range(9, 17)) + '</Head></Mode></FixtureDefinition>')
    q2 = parse_qxf(ET.fromstring(two))
    t = Profile(ident, q2["modes"]["M"], ("t.qxf", "M", "x", "", "MIT"), q2["heads"]["M"])
    assert t.flags["pan_ofs"] == 0 and t.flags["tilt_ofs"] == 2 and t.flags["pt16"] == 1 and "patch" not in t.flags, t.flags   # head 1, first wins
    assert t.cls[4] == "LJ_CH_LEVEL" and not t.invisible[4]                                    # continuous rotation: a Levels fader
    assert t.cls[9] == t.cls[11] == "LJ_CH_LEVEL" and t.level_caption[9] == "Pan2" and t.level_caption[11] == "Tilt2" and not t.invisible[9]
    assert t.pair_lsb[9] == 11 and t.invisible[10] == 1                                       # head 2's pan fine pairs with its pan
    assert [t.dyn["cmy[%d]" % k][0] for k in range(3)] == [5, 6, 7] and not any(t.invisible[12:16]) and "cmy_on" not in t.flags
    assert t.dyn["intensity"][0] == 17 and t.ext_gm[8] == t.ext_gm[16] == 1 and t.ext_gm[17] == 0, t.ext_gm[:18]
    named = {}
    for fname, mode in (("Robe-Robin-Tetra-1.qxf", "1-Wash"), ("American-DJ-Quad-Scan-LED.qxf", "28 Channels mode"), ("American-DJ-Asteroid-1200.qxf", "Standard")):
        r = load_qxf(os.path.join(QXF_DIR, fname))
        named[fname] = Profile(ident, r["modes"][mode], (fname, mode, "x", "", "MIT"), r["heads"][mode])
    tetra, quad, ast = named.values()
    assert tetra.dyn["intensity"][0] == 24 and tetra.ext_gm[13] == tetra.ext_gm[21] == 1 and tetra.dyn["cmy[0]"][0] == 7   # id 2950
    assert quad.flags["pan_ofs"] == 0 and quad.dyn["intensity"][0] == 5 and quad.ext_gm[12] == quad.ext_gm[19] == quad.ext_gm[26] == 1   # id 1468
    assert ast.flags["pan_ofs"] == 0 and ast.flags["tilt_ofs"] == 2 and "patch" not in ast.flags and ast.cls[4] == "LJ_CH_LEVEL"   # id 1060
    c1 = convert_all()
    c2 = convert_all()
    assert c1[0] == c2[0] and c1[1] == c2[1], "convert_all is not deterministic"
    print("qxf2c selftest: ok (MAC500 DMX4 footprint 16, Color1 overflow %d, synthetic patch + reversed focus + NoFunction, lint %d kinds, Chauvet split,"
          " W8D-5/6 binding rules on a two-head mode and ids 2950 / 1468 / 1060)"
          % (p.overflow[0][1], len(kinds)))
    return 0


def main():
    if "--selftest" in sys.argv:
        return selftest()
    if "--lint" in sys.argv:
        files = [a for a in sys.argv[1:] if a != "--lint"] or sorted(glob.glob(os.path.join(AUTH_DIR, "*", "*.qxf")))   # none named: every authored file
        n = 0
        for f in files:
            for prob in lint(f):
                print(prob)
                n += 1
        print("qxf2c --lint: %d file(s), %d problem(s)" % (len(files), n))
        return 1 if n else 0
    files, ov, findings = convert_all()
    stale = [p for p in glob.glob(os.path.join(GEN_DIR, "lj_profiles_gen_*.c")) if p not in files]
    if "--check" in sys.argv:
        for f in findings:
            print(f)
        for path, text in sorted(files.items()) + [(OVERFLOW, ov)]:
            if not os.path.isfile(path) or open(path, encoding="utf-8").read().replace("\r\n", "\n") != text:
                findings.append("%s differs from a regeneration (run python -I tools/qxf2c.py)" % os.path.relpath(path, ROOT))
                print(findings[-1])
        for path in stale:
            findings.append("%s is not generated any more (delete it)" % os.path.relpath(path, ROOT))
            print(findings[-1])
        print("qxf2c --check: %d finding(s), %d map line(s), %d part(s); palette overflow list:" % (len(findings), len(read_map()), len(files) - 1))
        sys.stdout.write(ov)
        return 1 if findings else 0
    if findings:
        for f in findings:
            print(f)
        return 1
    for path in stale:
        os.remove(path)
    for path, text in sorted(files.items()) + [(OVERFLOW, ov)]:
        with open(path, "w", encoding="utf-8", newline="\n") as f:          # the .c files are ASCII (octal escapes, asc() comments)
            f.write(text)
        print("wrote %s" % os.path.relpath(path, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
