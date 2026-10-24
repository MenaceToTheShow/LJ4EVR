#!/usr/bin/env python3
"""ofl2qxf.py - Open Fixture Library fixture JSON -> the QLC+ .qxf subset tools/qxf2c.py reads (W7).
Python 3 stdlib only. Deterministic: the same JSON gives the same bytes.

    python -I tools/ofl2qxf.py IN.json [IN.json ...] --out DIR   # write DIR/<manufacturer>-<fixture>.qxf (inspection)
    python -I tools/ofl2qxf.py --selftest                         # the mapping rules on a synthetic fixture + determinism

tools/qxf2c.py calls convert() in memory; nothing it writes is committed. Inputs are third_party/ofl-fixtures/
(MIT, commit in its SOURCE). The OFL model rules ported here (file:line at that commit):
  channel type      lib/model/CoarseChannel.js channelTypeConstraints (first type whose required capability
                    types occur and whose predicate holds; 'Unknown' otherwise)
  QLC+ group        plugins/qlcplus_4.12.2/export.js getChannelType ('Effect' for anything unlisted)
  wheel type        lib/model/Wheel.js type (most frequent slot type, last wins a tie)
  resolution        CoarseChannel.dmxValueResolution (default 1 + fine aliases), scaleDmxValue (drop low bytes)
  switching alias   SwitchingChannel.defaultChannelKey (the trigger capability holding the trigger's default)
  matrix inserts    Mode._getMatrixChannelKeysFromInsertBlock, Matrix pixel keys / orders
Channel and capability presets are chosen for qxf2c's Group + Preset table (docs/fixtures.md): a value we cannot
read from the JSON gets no preset (blank beats guessed), never an invented one.
"""
import json
import os
import re
import sys
from xml.sax.saxutils import escape, quoteattr

GROUP_OF_TYPE = {"Intensity": "Intensity", "Single Color": "Intensity", "Multi-Color": "Colour", "Pan": "Pan", "Tilt": "Tilt",
                 "Focus": "Beam", "Zoom": "Beam", "Iris": "Beam", "Color Temperature": "Beam", "Gobo": "Gobo", "Prism": "Prism",
                 "Shutter": "Shutter", "Strobe": "Shutter", "Speed": "Speed", "Effect": "Effect", "Fog": "Effect",
                 "Maintenance": "Maintenance", "NoFunction": "Nothing"}
COLOR_PRESET = {"Red": "IntensityRed", "Green": "IntensityGreen", "Blue": "IntensityBlue", "Cyan": "IntensityCyan",
                "Magenta": "IntensityMagenta", "Yellow": "IntensityYellow", "Amber": "IntensityAmber", "White": "IntensityWhite",
                "Warm White": "IntensityWhite", "Cold White": "IntensityWhite", "UV": "IntensityUV", "Lime": "IntensityLime",
                "Indigo": "IntensityIndigo"}
SHUTTER_PRESET = {"Open": "ShutterOpen", "Closed": "ShutterClose", "Strobe": "Strobe", "Pulse": "Pulse", "RampUp": "RampUp", "RampDown": "RampDown"}
RESET_RE = re.compile(r"^\s*(?:fixture |total |full |general )?reset(?: all| fixture)?\s*$", re.I)
LAMP_ON_RE = re.compile(r"^\s*lamp (?:on|strike)\s*$", re.I)
LAMP_OFF_RE = re.compile(r"^\s*lamp off\s*$", re.I)


def wheel_type(wheel):
    types = [s.get("type", "") for s in wheel.get("slots", [])]
    if not types:
        return None
    t = sorted(types, key=types.count)[-1]                  # stable sort by count, pop the last (Wheel.js)
    return "AnimationGobo" if t.startswith("AnimationGobo") else t


class Fixture:
    def __init__(self, fx):
        self.fx = fx
        self.wheels = fx.get("wheels", {})
        self.chans = dict(fx.get("availableChannels", {}))
        self.fine = {}                                       # fine alias -> (coarse key, 1-based byte index)
        self._expand_templates()
        for key, ch in self.chans.items():
            for i, alias in enumerate(ch.get("fineChannelAliases", [])):
                self.fine[alias] = (key, i + 1)
        self.switch = {}                                     # alias -> (trigger key, [(lo, hi, target)]) in the trigger's resolution
        for key, ch in self.chans.items():
            for lo, hi, cap in self.caps_raw(ch):
                for alias, target in cap.get("switchChannels", {}).items():
                    self.switch.setdefault(alias, (key, []))[1].append((lo, hi, target))

    # ---- matrix: template channels become ordinary channels for every pixel and pixel group key ----
    def pixel_positions(self):
        m = self.fx.get("matrix")
        if not m:
            return {}
        if "pixelKeys" in m:
            struct = m["pixelKeys"]
        else:
            cx, cy, cz = (m["pixelCount"] + [1, 1, 1])[:3]
            axes = [a for a, n in zip("XYZ", (cx, cy, cz)) if n > 1]
            def key(x, y, z):
                if len(axes) == 1:
                    return str(max(x, y, z))
                if len(axes) == 2:
                    return "(%d, %d)" % (x if "X" in axes else y, y if "Y" in axes else z)
                return "(%d, %d, %d)" % (x, y, z)
            struct = [[[key(x, y, z) for x in range(1, cx + 1)] for y in range(1, cy + 1)] for z in range(1, cz + 1)]
        pos = {}
        for z, ys in enumerate(struct):
            for y, xs in enumerate(ys):
                for x, k in enumerate(xs):
                    if k is not None:
                        pos[k] = (x + 1, y + 1, z + 1)
        return pos

    def pixel_keys(self, repeat):
        pos = self.pixel_positions()
        if isinstance(repeat, list):
            return repeat
        if repeat == "eachPixelGroup":
            return list(self.fx.get("matrix", {}).get("pixelGroups", {}))
        natural = sorted(pos, key=lambda k: [(0, int(t), "") if t.isdigit() else (1, 0, t.lower()) for t in re.findall(r"\d+|\D+", k)])
        if repeat == "eachPixelABC":
            return natural
        a = {"X": 0, "Y": 1, "Z": 2}
        o = repeat.replace("eachPixel", "")
        return sorted(natural, key=lambda k: (pos[k][a[o[2]]], pos[k][a[o[1]]], pos[k][a[o[0]]]))

    def _expand_templates(self):
        tpl = self.fx.get("templateChannels")
        if not tpl:
            return
        keys = list(self.pixel_positions()) + list(self.fx.get("matrix", {}).get("pixelGroups", {}))
        for pk in keys:
            for tkey, tch in tpl.items():
                name = tkey.replace("$pixelKey", pk)
                if name not in self.chans:
                    self.chans[name] = json.loads(json.dumps(tch).replace("$pixelKey", pk.replace("\\", "\\\\").replace('"', '\\"')))

    # ---- channel model ----
    @staticmethod
    def res(ch):
        r = {"8bit": 1, "16bit": 2, "24bit": 3}.get(ch.get("dmxValueResolution"))
        return r or 1 + len(ch.get("fineChannelAliases", []))

    def caps_raw(self, ch):
        """[(lo, hi, cap)] in the channel's own resolution."""
        top = 256 ** self.res(ch) - 1
        if "capability" in ch:
            return [(0, top, ch["capability"])]
        return [(c["dmxRange"][0], c["dmxRange"][1], c) for c in ch.get("capabilities", [])]

    def caps8(self, ch):
        """[(lo, hi, cap)] scaled to 8 bit, later ranges clipped so none overlap."""
        shift = 8 * (self.res(ch) - 1)
        out, last = [], -1
        for lo, hi, c in self.caps_raw(ch):
            lo, hi = max(lo >> shift, last + 1), hi >> shift
            if lo <= hi <= 255:
                out.append((lo, hi, c))
                last = hi
        return out

    def default_raw(self, ch):
        """defaultValue in the channel's dmxValueResolution (a percentage of its maximum, floored)."""
        v = ch.get("defaultValue", 0)
        if isinstance(v, str):
            m = re.match(r"^(\d+(?:\.\d+)?)%$", v.strip())
            v = int(float(m.group(1)) / 100 * (256 ** self.res(ch) - 1)) if m else 0
        return v

    def default8(self, ch, byte=0):
        """byte `byte` (0 = coarse) of the default scaled to the channel's full resolution (scaleDmxValue: the last byte repeats)."""
        r, full = self.res(ch), 1 + len(ch.get("fineChannelAliases", []))
        v = self.default_raw(ch)
        b = [(v >> 8 * (r - 1 - i)) & 255 for i in range(r)]
        b = (b + [b[-1]] * full)[:full]
        return b[byte] if 0 <= byte < full else 0

    def cap_wheels(self, ch_key, cap):
        if "wheel" in cap:
            names = cap["wheel"] if isinstance(cap["wheel"], list) else [cap["wheel"]]
        elif "Wheel" in cap.get("type", ""):
            names = [self.chans.get(ch_key, {}).get("name", ch_key)]
        else:
            names = []
        return [self.wheels.get(n) for n in names]

    def chan_type(self, key, ch):
        caps = [c for _, _, c in self.caps_raw(ch)]
        types = [c.get("type") for c in caps]
        def wtypes(c):
            return [wheel_type(w) if w else None for w in self.cap_wheels(key, c)]
        rules = [
            ("Single Color", ["ColorIntensity"], None),
            ("Multi-Color", ["ColorPreset", "WheelSlot"], lambda: all(c.get("type") != "WheelSlot" or (wtypes(c)[:1] == ["Color"]) for c in caps)),
            ("Pan", ["Pan", "PanContinuous"], None), ("Tilt", ["Tilt", "TiltContinuous"], None),
            ("Focus", ["Focus"], None), ("Zoom", ["Zoom"], None), ("Iris", ["Iris", "IrisEffect"], None),
            ("Gobo", ["WheelSlot", "WheelShake"], lambda: all(all(t == "Gobo" for t in wtypes(c)) for c in caps)),
            ("Prism", ["Prism"], None), ("Color Temperature", ["ColorTemperature"], None),
            ("Effect", ["Effect", "EffectParameter", "Frost", "FrostEffect", "SoundSensitivity", "WheelSlot"], None),
            ("Strobe", ["ShutterStrobe"], lambda: any(c.get("type") == "ShutterStrobe" and c.get("shutterEffect") not in ("Open", "Closed") for c in caps)),
            ("Shutter", ["ShutterStrobe", "BladeInsertion", "BladeRotation", "BladeSystemRotation"], None),
            ("Fog", ["Fog", "FogOutput", "FogType"], None),
            ("Speed", ["StrobeSpeed", "StrobeDuration", "PanTiltSpeed", "EffectSpeed", "EffectDuration", "BeamAngle", "BeamPosition",
                       "PrismRotation", "Rotation", "Speed", "Time", "WheelSlotRotation", "WheelRotation", "WheelShake"], None),
            ("Maintenance", ["Maintenance"], None), ("Intensity", ["Intensity", "Generic"], None), ("NoFunction", ["NoFunction"], None)]
        for name, req, pred in rules:
            if any(t in req for t in types) and (pred is None or pred()):
                return name
        return "Unknown"

    def resolve(self, key, depth=0):
        """mode channel key -> (coarse key, byte index) or None."""
        if key in self.chans:
            return key, 0
        if key in self.fine:
            return self.fine[key]
        if key in self.switch and depth < 4:
            trig, caps = self.switch[key]
            dv = self.default_raw(self.chans[trig])
            for lo, hi, target in caps:
                if lo <= dv <= hi:
                    return (None, 0) if target is None else self.resolve(target, depth + 1)   # null target = a NullChannel
        return None

    def mode_keys(self, mode):
        keys = []
        for ref in mode.get("channels", []):
            if isinstance(ref, dict) and ref.get("insert") == "matrixChannels":
                pks = self.pixel_keys(ref["repeatFor"])
                tcs = ref["templateChannels"]
                if ref.get("channelOrder") == "perChannel":
                    keys += [t.replace("$pixelKey", pk) for t in tcs for pk in pks]
                else:
                    keys += [t.replace("$pixelKey", pk) for pk in pks for t in tcs]
            else:
                keys.append(ref)
        return keys


def cap_text(fx, ch_key, cap):
    t = cap.get("type", "")
    comment = (cap.get("comment") or "").strip()
    if comment and t in ("Generic", "Maintenance", "ColorPreset", "NoFunction"):
        return comment
    camel = lambda w: re.sub(r"(?<=[a-z])(?=[A-Z])", " ", w)
    if t == "ShutterStrobe":
        e = cap.get("shutterEffect", "Strobe")
        base = {"Open": "Shutter open", "Closed": "Shutter closed"}.get(e, camel(e)) + (" random" if cap.get("randomTiming") else "")
    elif t == "WheelSlot":
        base = slot_name(fx, ch_key, cap) or "Wheel slot"
    elif t == "NoFunction":
        base = "No function"
    elif t == "ColorIntensity":
        base = cap.get("color", "Color") + " intensity"
    elif t == "Effect":
        base = cap.get("effectName") or camel(cap.get("effectPreset", "Effect"))
    else:
        base = camel(t)
    if "speedStart" in cap:
        base += " %s to %s" % (cap["speedStart"], cap.get("speedEnd", ""))
    elif "speed" in cap:
        base += " " + str(cap["speed"])
    if comment and comment.lower() not in base.lower():
        base += " " + comment
    return base.strip()


def slot_of(fx, ch_key, cap):
    """the wheel slot dict of a single-slot WheelSlot capability, else None."""
    wheels = fx.cap_wheels(ch_key, cap)
    n = cap.get("slotNumber")
    if len(wheels) != 1 or not wheels[0] or n is None or n != int(n):
        return None
    slots = wheels[0].get("slots", [])
    return slots[int(n) - 1] if 1 <= int(n) <= len(slots) else None


def slot_name(fx, ch_key, cap):
    s = slot_of(fx, ch_key, cap)
    if s:
        return s.get("name") or {"Open": "Open", "Closed": "Closed"}.get(s.get("type"), "%s %d" % (s.get("type", "Slot"), int(cap["slotNumber"])))
    if "slotNumberStart" in cap:
        return "Wheel scroll %s to %s" % (fmt_num(cap["slotNumberStart"]), fmt_num(cap["slotNumberEnd"]))
    if "slotNumber" in cap:
        return "Split slot %s" % fmt_num(cap["slotNumber"])
    return None


def fmt_num(v):
    return str(int(v)) if v == int(v) else str(v)


def cap_preset(fx, ch_key, group, cap):
    """-> (preset, res1, res2) for qxf2c, or (None, None, None)."""
    t = cap.get("type")
    hexes = lambda cs: [c for c in cs if re.match(r"^#[0-9a-fA-F]{6}$", c)]
    if t == "ShutterStrobe":
        e = SHUTTER_PRESET.get(cap.get("shutterEffect"))
        if e in (None, "ShutterOpen", "ShutterClose"):
            return e, None, None
        fast_slow = str(cap.get("speedStart", "")).lower() == "fast" and str(cap.get("speedEnd", "")).lower() == "slow"
        return e + ("Random" if e == "Strobe" and cap.get("randomTiming") else "") + ("FastToSlow" if fast_slow else "SlowToFast"), None, None
    if t == "StrobeSpeed":
        return "StrobeSlowToFast", None, None
    if t == "ColorPreset":
        cs = hexes(cap.get("colors", []))
        if len(cs) == 2:
            return "ColorDoubleMacro", cs[0], cs[1]
        return "ColorMacro", (cs[0] if cs else None), None
    if t == "WheelSlot":
        s = slot_of(fx, ch_key, cap)
        if s is None:
            n = cap.get("slotNumber")
            wheels = fx.cap_wheels(ch_key, cap)
            if n is not None and n != int(n) and len(wheels) == 1 and wheels[0]:       # a split colour between two slots
                sl = wheels[0].get("slots", [])
                a, b = int(n), int(n) + 1
                if 1 <= a and b <= len(sl) and sl[a - 1].get("type") == "Color" and sl[b - 1].get("type") == "Color":
                    ca, cb = hexes(sl[a - 1].get("colors", [])), hexes(sl[b - 1].get("colors", []))
                    if ca and cb:
                        return "ColorDoubleMacro", ca[0], cb[0]
            return None, None, None
        st = s.get("type")
        if st == "Color":
            cs = hexes(s.get("colors", []))
            return "ColorMacro", (cs[0] if cs else None), None
        if st == "Open":
            return ("ColorMacro", "#ffffff", None) if group == "Colour" else ("GoboMacro", None, None) if group == "Gobo" else (None, None, None)
        if st == "Gobo":
            return "GoboMacro", None, None
        if st == "Prism":
            return "PrismEffectOn", None, None
        return None, None, None
    if t == "Prism":
        return "PrismEffectOn", None, None
    if t == "Maintenance":
        c = cap.get("comment", "")
        return ("ResetAll" if RESET_RE.match(c) else "LampOn" if LAMP_ON_RE.match(c) else "LampOff" if LAMP_OFF_RE.match(c) else None), None, None
    return None, None, None


def chan_preset(fx, key, ch, ctype, caps):
    types = {c.get("type") for _, _, c in caps}
    if ctype == "Single Color":
        col = next((c.get("color") for _, _, c in caps if c.get("type") == "ColorIntensity"), None)
        return COLOR_PRESET.get(col)
    if ctype == "Intensity":
        return "IntensityMasterDimmer" if "Intensity" in types else None
    if ctype in ("Pan", "Tilt"):
        return ("PositionPan" if ctype == "Pan" else "PositionTilt") if ctype in types else None
    if ctype == "Focus":
        f = next((c for _, _, c in caps if c.get("type") == "Focus"), {})
        a, b = str(f.get("distanceStart", "")).lower(), str(f.get("distanceEnd", "")).lower()
        return "BeamFocusNearFar" if (a, b) == ("near", "far") else "BeamFocusFarNear" if (a, b) == ("far", "near") else None
    if ctype == "Zoom":
        z = next((c for _, _, c in caps if c.get("type") == "Zoom"), {})
        a, b = str(z.get("angleStart", "")).lower(), str(z.get("angleEnd", "")).lower()
        num = lambda s: float(s[:-3]) if re.match(r"^\d+(?:\.\d+)?deg$", s) else None
        if (a, b) == ("narrow", "wide") or (num(a) is not None and num(b) is not None and num(a) < num(b)):
            return "BeamZoomSmallBig"
        if (a, b) == ("wide", "narrow") or (num(a) is not None and num(b) is not None and num(a) > num(b)):
            return "BeamZoomBigSmall"
        return None
    if ctype == "Iris":
        i = next((c for _, _, c in caps if c.get("type") == "Iris"), None)
        if i is None:
            return None
        a, b = str(i.get("openPercentStart", "")).lower(), str(i.get("openPercentEnd", "")).lower()
        return "ShutterIrisMaxToMin" if (a, b) == ("open", "closed") else "ShutterIrisMinToMax"
    return None


def convert(fx_json, manufacturer, src, commit):
    """OFL fixture dict -> (qxf text, [problem]). src = the path under third_party/ofl-fixtures shown in the comment."""
    fx = Fixture(fx_json)
    problems = []
    emitted, order = {}, []                                   # qxf channel name -> xml lines
    modes = []
    for mode in fx_json.get("modes", []):
        names, bad = [], None
        for n, key in enumerate(fx.mode_keys(mode)):
            if key is None:
                name = "Unused %d" % (n + 1)
                if name not in emitted:
                    emitted[name] = ['  <Channel Name=%s>' % quoteattr(name), '   <Group Byte="0">Nothing</Group>',
                                     '   <Capability Min="0" Max="255">No function</Capability>', '  </Channel>']
                    order.append(name)
                names.append(name)
                continue
            r = fx.resolve(key)
            if r is None:
                bad = "channel %r does not resolve" % key
                break
            if key not in emitted:
                emitted[key] = channel_xml(fx, key, r[0], r[1]) if r[0] is not None else [
                    '  <Channel Name=%s>' % quoteattr(key), '   <Group Byte="0">Nothing</Group>',
                    '   <Capability Min="0" Max="255">No function</Capability>', '  </Channel>']
                order.append(key)
            names.append(key)
        if bad:
            problems.append("%s mode %r: %s, mode skipped" % (src, mode.get("name"), bad))
            continue
        modes.append((mode.get("name") or mode.get("shortName") or "Mode %d" % (len(modes) + 1), names))
    authors = ", ".join(fx_json.get("meta", {}).get("authors", []))
    L = ['<?xml version="1.0" encoding="UTF-8"?>', "<!DOCTYPE FixtureDefinition>",
         '<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition">',
         " <!-- Converted by LJ4EVR tools/ofl2qxf.py from Open Fixture Library %s at commit %s (MIT); modified: converted -->" % (escape(src).replace("--", "- -"), commit),
         " <Creator>", "  <Name>LJ4EVR ofl2qxf.py</Name>", "  <Version>OFL %s</Version>" % commit[:7], "  <Author>%s</Author>" % escape(authors), " </Creator>",
         " <Manufacturer>%s</Manufacturer>" % escape(manufacturer), " <Model>%s</Model>" % escape(fx_json.get("name", "")),
         " <Type>%s</Type>" % escape((fx_json.get("categories") or ["Other"])[0])]
    for name in order:
        L += emitted[name]
    for mname, names in modes:
        L.append(" <Mode Name=%s>" % quoteattr(mname))
        L += ['  <Channel Number="%d">%s</Channel>' % (i, escape(n)) for i, n in enumerate(names)]
        L.append(" </Mode>")
    L.append("</FixtureDefinition>")
    return "\n".join(L) + "\n", problems


def channel_xml(fx, qname, key, byte):
    ch = fx.chans[key]
    caps = fx.caps8(ch)
    ctype = fx.chan_type(key, ch)
    group = GROUP_OF_TYPE.get(ctype, "Effect")
    preset = chan_preset(fx, key, ch, ctype, caps)
    if ctype == "Intensity" and preset is None:
        group = None                                          # Generic only: an ungrouped level channel, never the dimmer
    if ctype in ("Pan", "Tilt") and preset is None:
        group = "Effect"                                      # continuous rotation only: not a position channel
    attrs = "Name=%s" % quoteattr(qname)
    d = fx.default8(ch, byte)
    if d:
        attrs += ' Default="%d"' % d
    if byte >= 2:                                             # 24-bit: QLC+ has no Byte 2 (export.js fakes a Maintenance channel)
        return ['  <Channel %s>' % attrs, '   <Group Byte="0">Maintenance</Group>',
                '   <Capability Min="0" Max="255">Fine^%d adjustment for %s</Capability>' % (byte, escape(key)), '  </Channel>']
    if byte == 1:
        if preset and (preset.startswith("Position") or preset.startswith("Intensity")):
            attrs += ' Preset="%sFine"' % preset
        L = ['  <Channel %s>' % attrs]
        if group:
            L.append('   <Group Byte="1">%s</Group>' % group)
        return L + ['   <Capability Min="0" Max="255">Fine adjustment for %s</Capability>' % escape(key), '  </Channel>']
    if preset:
        attrs += ' Preset="%s"' % preset
    L = ['  <Channel %s>' % attrs]
    if group:
        L.append('   <Group Byte="0">%s</Group>' % group)
    for lo, hi, c in caps:
        p, r1, r2 = cap_preset(fx, key, group, c)
        a = 'Min="%d" Max="%d"' % (lo, hi)
        if p:
            a += ' Preset="%s"' % p
        if r1:
            a += ' Res1="%s"' % r1.lower()
        if r2:
            a += ' Res2="%s"' % r2.lower()
        L.append('   <Capability %s>%s</Capability>' % (a, escape(cap_text(fx, key, c))))
    return L + ['  </Channel>']


def selftest():
    fx = {"name": "Synth Spot", "categories": ["Moving Head"], "meta": {"authors": ["t"]},
          "wheels": {"Color Wheel": {"slots": [{"type": "Open"}, {"type": "Color", "name": "Red", "colors": ["#ff0000"]},
                                               {"type": "Color", "name": "Blue", "colors": ["#0000ff"]}]},
                     "Gobo Wheel": {"slots": [{"type": "Open"}, {"type": "Gobo", "name": "Dots"}]}},
          "availableChannels": {
              "Pan": {"fineChannelAliases": ["Pan fine"], "capability": {"type": "Pan", "angleStart": "0deg", "angleEnd": "540deg"}},
              "Tilt": {"fineChannelAliases": ["Tilt fine"], "capability": {"type": "Tilt", "angleStart": "0deg", "angleEnd": "270deg"}},
              "Dimmer": {"fineChannelAliases": ["Dimmer fine"], "defaultValue": "100%", "capability": {"type": "Intensity"}},
              "Shutter": {"capabilities": [{"dmxRange": [0, 9], "type": "ShutterStrobe", "shutterEffect": "Closed"},
                                           {"dmxRange": [10, 19], "type": "ShutterStrobe", "shutterEffect": "Open"},
                                           {"dmxRange": [20, 255], "type": "ShutterStrobe", "shutterEffect": "Strobe", "speedStart": "slow", "speedEnd": "fast"}]},
              "Color Wheel": {"capabilities": [{"dmxRange": [0, 9], "type": "WheelSlot", "slotNumber": 1}, {"dmxRange": [10, 19], "type": "WheelSlot", "slotNumber": 2},
                                               {"dmxRange": [20, 29], "type": "WheelSlot", "slotNumber": 2.5}, {"dmxRange": [30, 39], "type": "WheelSlot", "slotNumber": 3},
                                               {"dmxRange": [40, 255], "type": "WheelRotation", "speedStart": "slow CW", "speedEnd": "fast CW"}]},
              "Gobo Wheel": {"capabilities": [{"dmxRange": [0, 127], "type": "WheelSlot", "slotNumber": 1}, {"dmxRange": [128, 255], "type": "WheelSlot", "slotNumber": 2}]},
              "Focus": {"capability": {"type": "Focus", "distanceStart": "far", "distanceEnd": "near"}},
              "Red": {"capability": {"type": "ColorIntensity", "color": "Red"}},
              "Gen": {"capability": {"type": "Generic"}},
              "Control": {"capabilities": [{"dmxRange": [0, 99], "type": "NoFunction"}, {"dmxRange": [100, 119], "type": "Maintenance", "comment": "Reset"},
                                           {"dmxRange": [120, 139], "type": "Maintenance", "comment": "Lamp on"},
                                           {"dmxRange": [140, 159], "type": "Maintenance", "comment": "Lamp off"},
                                           {"dmxRange": [160, 255], "type": "Maintenance", "comment": "Reset pan/tilt"}]},
              "Mode": {"capabilities": [{"dmxRange": [0, 127], "type": "Generic", "switchChannels": {"Wheel A": "Color Wheel"}},
                                        {"dmxRange": [128, 255], "type": "Generic", "switchChannels": {"Wheel A": "Gobo Wheel"}}]}},
          "modes": [{"name": "Basic", "channels": ["Pan", "Tilt", "Pan fine", "Tilt fine", "Dimmer", "Dimmer fine", "Shutter", "Wheel A", "Mode", "Focus", "Red", "Gen", "Control", None]},
                    {"name": "Broken", "channels": ["Nope"]}]}
    a, pa = convert(fx, "Test", "test/synth.json", "0" * 40)
    b, _ = convert(json.loads(json.dumps(fx)), "Test", "test/synth.json", "0" * 40)
    assert a == b, "not deterministic"
    assert pa == ["test/synth.json mode 'Broken': channel 'Nope' does not resolve, mode skipped"], pa
    for want in ('<Channel Name="Pan" Preset="PositionPan">', '<Channel Name="Pan fine" Preset="PositionPanFine">', '<Group Byte="1">Pan</Group>',
                 '<Channel Name="Dimmer" Default="255" Preset="IntensityMasterDimmer">', '<Channel Name="Dimmer fine" Default="255" Preset="IntensityMasterDimmerFine">',
                 '<Capability Min="0" Max="9" Preset="ShutterClose">Shutter closed</Capability>',
                 '<Capability Min="20" Max="255" Preset="StrobeSlowToFast">Strobe slow to fast</Capability>',
                 '<Channel Name="Wheel A">', '<Capability Min="0" Max="9" Preset="ColorMacro" Res1="#ffffff">Open</Capability>',     # alias -> the default target (Mode = 0)
                 '<Capability Min="10" Max="19" Preset="ColorMacro" Res1="#ff0000">Red</Capability>',
                 '<Capability Min="20" Max="29" Preset="ColorDoubleMacro" Res1="#ff0000" Res2="#0000ff">Split slot 2.5</Capability>',
                 '<Capability Min="40" Max="255">Wheel Rotation slow CW to fast CW</Capability>',
                 '<Channel Name="Focus" Preset="BeamFocusFarNear">', '<Channel Name="Red" Preset="IntensityRed">',
                 '<Channel Name="Gen">\n   <Capability Min="0" Max="255">Generic</Capability>',
                 '<Capability Min="100" Max="119" Preset="ResetAll">Reset</Capability>', '<Capability Min="120" Max="139" Preset="LampOn">Lamp on</Capability>',
                 '<Capability Min="140" Max="159" Preset="LampOff">Lamp off</Capability>', '<Capability Min="160" Max="255">Reset pan/tilt</Capability>',
                 '<Channel Name="Unused 14">', '<Channel Number="13">Unused 14</Channel>'):
        assert want in a, want
    assert '<Mode Name="Broken">' not in a
    # 16-bit ranges scale to 8 bit without overlap; a matrix insert expands per pixel
    m = {"name": "Bar", "categories": ["Pixel Bar"], "meta": {"authors": []}, "matrix": {"pixelCount": [3, 1, 1]},
         "availableChannels": {"Dim": {"fineChannelAliases": ["Dim fine"], "capabilities": [{"dmxRange": [0, 1000], "type": "Intensity"}, {"dmxRange": [1001, 65535], "type": "Generic"}]}},
         "templateChannels": {"Red $pixelKey": {"capability": {"type": "ColorIntensity", "color": "Red"}}},
         "modes": [{"name": "Pix", "channels": ["Dim", {"insert": "matrixChannels", "repeatFor": "eachPixelABC", "channelOrder": "perPixel", "templateChannels": ["Red $pixelKey"]}]}]}
    t, p = convert(m, "Test", "test/bar.json", "0" * 40)
    assert not p and '<Capability Min="0" Max="3">Intensity</Capability>' in t and '<Capability Min="4" Max="255">Generic</Capability>' in t, t
    assert '<Channel Number="3">Red 3</Channel>' in t and '<Channel Name="Red 2" Preset="IntensityRed">' in t
    print("ofl2qxf selftest: ok (presets, fine channels, switching default, split colour, 16-bit scaling, matrix insert, unresolved mode skipped)")
    return 0


def main():
    if "--selftest" in sys.argv:
        return selftest()
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--out" not in sys.argv or not args:
        print(__doc__)
        return 2
    out = sys.argv[sys.argv.index("--out") + 1]
    args = [a for a in args if a != out]
    os.makedirs(out, exist_ok=True)
    rc = 0
    for path in args:
        with open(path, encoding="utf-8") as f:
            fx = json.load(f)
        man = os.path.basename(os.path.dirname(os.path.abspath(path)))
        text, problems = convert(fx, man, man + "/" + os.path.basename(path), "local")
        for p in problems:
            print(p)
            rc = 1
        dst = os.path.join(out, "%s-%s.qxf" % (man, os.path.splitext(os.path.basename(path))[0]))
        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("wrote %s" % dst)
    return rc


if __name__ == "__main__":
    sys.exit(main())
