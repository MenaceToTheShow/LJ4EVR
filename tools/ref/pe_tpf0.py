#!/usr/bin/env python3
"""pe_tpf0.py - W6 LORD_FIDELITY-12: the TPF0 form streams in a Delphi exe's RT_RCDATA resources as DFM text, in the
flattened dfm_text form tools/dfm2c.py reads: strings '..' with #n for control bytes and single apostrophes, lists ('a' 'b'), sets [a, b],
collections <<K=v K=v> <..>>, binary {the first 20 bytes in hex}... (as the 208 LightJockey.exe forms were extracted).

usage: pe_tpf0.py <exe> <out dir> [NAME ...]    NAME = an RCDATA resource name (TLJSOUNDAPP); default: every TPF0 one
       pe_tpf0.py --selftest                     a synthetic stream decodes; truncated and oversized ones are refused
Writes <out dir>/<NAME>.dfm.txt (UTF-8, as the dfm_text extraction that tools/dfm2c.py reads). The exe is untrusted input: it is read, never run, and every offset, length and
count is checked before use; a bad file stops with a message and writes nothing. For the reference/ tree only
(gitignored): the output is Martin's form data and never enters git. Python 3, stdlib only.
"""
import os
import struct
import sys

RT_RCDATA = 10


class Bad(Exception):
    pass


def u16(b, o):
    if o < 0 or o + 2 > len(b):
        raise Bad("read past the end at %d" % o)
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    if o < 0 or o + 4 > len(b):
        raise Bad("read past the end at %d" % o)
    return struct.unpack_from("<I", b, o)[0]


# ----------------------------------------------------------------------------- PE resources
def rcdata(pe):
    """{resource name: bytes} of the named RT_RCDATA entries (first language of each)."""
    if pe[:2] != b"MZ":
        raise Bad("not an MZ executable")
    nt = u32(pe, 0x3C)
    if pe[nt:nt + 4] != b"PE\0\0":
        raise Bad("no PE header")
    nsec, optsz = u16(pe, nt + 6), u16(pe, nt + 20)
    opt = nt + 24
    magic = u16(pe, opt)
    ddir = opt + (96 if magic == 0x10B else 112 if magic == 0x20B else -1)
    if ddir < opt:
        raise Bad("unknown optional header 0x%X" % magic)
    if 96 + 3 * 8 > optsz and magic == 0x10B or 112 + 3 * 8 > optsz and magic == 0x20B:
        raise Bad("optional header has no resource directory")
    rsrc_rva = u32(pe, ddir + 2 * 8)
    secs = []
    for i in range(nsec):
        s = opt + optsz + 40 * i
        secs.append((u32(pe, s + 12), u32(pe, s + 8), u32(pe, s + 16), u32(pe, s + 20)))   # rva, vsize, rawsize, rawptr

    def off(rva, n):
        for va, vs, rs, rp in secs:
            if va <= rva and rva + n <= va + max(vs, rs) and rva - va + n <= rs:
                o = rp + rva - va
                if o + n > len(pe):
                    break
                return o
        raise Bad("RVA 0x%X (+%d) outside every section" % (rva, n))

    base = off(rsrc_rva, 16)

    def entries(d):
        n = u16(pe, base + d + 12) + u16(pe, base + d + 14)
        if n > 4096:
            raise Bad("resource directory with %d entries" % n)
        return [(u32(pe, base + d + 16 + 8 * i), u32(pe, base + d + 20 + 8 * i)) for i in range(n)]

    def name(v):
        if not v & 0x80000000:
            return None
        o = base + (v & 0x7FFFFFFF)
        n = u16(pe, o)
        if o + 2 + 2 * n > len(pe):
            raise Bad("resource name past the end")
        return pe[o + 2:o + 2 + 2 * n].decode("utf-16-le", "replace")

    out = {}
    for tid, tdir in entries(0):
        if tid != RT_RCDATA or not tdir & 0x80000000:
            continue
        for nid, ndir in entries(tdir & 0x7FFFFFFF):
            nm = name(nid)
            if nm is None or not ndir & 0x80000000:
                continue
            langs = entries(ndir & 0x7FFFFFFF)
            if not langs or langs[0][1] & 0x80000000:
                continue
            de = base + langs[0][1]
            rva, size = u32(pe, de), u32(pe, de + 4)
            if size > 16 << 20:
                raise Bad("resource %s of %d bytes" % (nm, size))
            o = off(rva, size)
            out[nm] = pe[o:o + size]
    return out


# ----------------------------------------------------------------------------- TPF0 -> text
class Reader:
    def __init__(self, b):
        self.b, self.i, self.depth = b, 0, 0

    def take(self, n):
        if n < 0 or self.i + n > len(self.b):
            raise Bad("stream truncated at %d (+%d)" % (self.i, n))
        v = self.b[self.i:self.i + n]
        self.i += n
        return v

    def byte(self):
        return self.take(1)[0]

    def peek(self):
        if self.i >= len(self.b):
            raise Bad("stream truncated at %d" % self.i)
        return self.b[self.i]

    def sstr(self):
        return self.take(self.byte()).decode("cp1252", "replace")

    def lstr(self, width=1):
        n = struct.unpack("<I", self.take(4))[0]
        if n > 1 << 24:
            raise Bad("string of %d units" % n)
        raw = self.take(n * width)
        return raw.decode("utf-16-le" if width == 2 else "cp1252", "replace")


def quote(s):
    """string literal as the dfm_text extraction wrote it and dfm2c.scan_str reads it: printable runs in quotes, an
    apostrophe kept single (not Delphi's doubled form), control characters as #n."""
    out, run = [], ""
    for ch in s:
        c = ord(ch)
        if c < 32:
            if run:
                out.append("'" + run + "'")
                run = ""
            out.append("#%d" % c)
        else:
            run += ch if c < 256 else "?"
    if run or not out:
        out.append("'" + run + "'")
    return "".join(out)


def value(r):
    t = r.byte()
    if t == 0:
        raise Bad("vaNull where a value is expected")
    if t == 1:                                            # vaList
        items = []
        while r.peek() != 0:
            items.append(value(r))
        r.byte()
        return "(" + " ".join(items) + ")"
    if t == 2:
        return str(struct.unpack("<b", r.take(1))[0])
    if t == 3:
        return str(struct.unpack("<h", r.take(2))[0])
    if t == 4:
        return str(struct.unpack("<i", r.take(4))[0])
    if t == 5:                                            # vaExtended, 80-bit
        raw = r.take(10)
        mant, se = struct.unpack("<QH", raw)
        if mant == 0:
            return "0.000000000000000000"
        v = mant * 2.0 ** ((se & 0x7FFF) - 16383 - 63)
        return "%.18f" % (-v if se & 0x8000 else v)
    if t in (6, 7):                                       # vaString, vaIdent
        s = r.sstr()
        return quote(s) if t == 6 else s
    if t == 8:
        return "False"
    if t == 9:
        return "True"
    if t == 10:                                           # vaBinary
        n = struct.unpack("<I", r.take(4))[0]
        data = r.take(n)
        return "{" + data[:20].hex() + "}" + ("..." if n > 20 else "")
    if t == 11:                                           # vaSet
        items = []
        while True:
            s = r.sstr()
            if not s:
                break
            items.append(s)
        return "[" + ", ".join(items) + "]"
    if t == 12:
        return quote(r.lstr())
    if t == 13:
        return "nil"
    if t == 14:                                           # vaCollection
        items = []
        while r.peek() != 0:
            if r.peek() in (2, 3, 4):
                value(r)                                  # the item index (ignored in the flattened form)
            if r.byte() != 1:
                raise Bad("collection item without vaList at %d" % (r.i - 1))
            props = []
            while r.peek() != 0:
                k = r.sstr()
                props.append("%s=%s" % (k, value(r)))
            r.byte()
            items.append("<" + " ".join(props) + ">")
        r.byte()
        return "<" + " ".join(items) + ">"
    if t == 15:
        return "%g" % struct.unpack("<f", r.take(4))[0]
    if t == 16:
        return "%.4f" % (struct.unpack("<q", r.take(8))[0] / 10000)
    if t == 17:
        return "%g" % struct.unpack("<d", r.take(8))[0]
    if t == 18:
        return quote(r.lstr(2))
    if t == 19:
        return str(struct.unpack("<q", r.take(8))[0])
    if t == 20:
        return quote(r.lstr().encode("cp1252", "replace").decode("utf-8", "replace"))
    raise Bad("unknown value type %d at %d" % (t, r.i - 1))


def obj(r, lines, ind):
    r.depth += 1
    if r.depth > 64:
        raise Bad("objects nested deeper than 64")
    kw, pos = "object", ""
    if r.peek() & 0xF0 == 0xF0:
        flags = r.byte() & 0x0F
        kw = "inherited" if flags & 1 else "inline" if flags & 4 else "object"
        if flags & 2:
            pos = " [%s]" % value(r)
    cls, name = r.sstr(), r.sstr()
    lines.append("%s%s %s: %s%s" % ("  " * ind, kw, name, cls, pos))
    while r.peek() != 0:
        k = r.sstr()
        lines.append("%s%s = %s" % ("  " * (ind + 1), k, value(r)))
    r.byte()
    while r.peek() != 0:
        obj(r, lines, ind + 1)
    r.byte()
    lines.append("%send" % ("  " * ind))
    r.depth -= 1


def tpf0_text(b):
    if len(b) > 16 << 20:
        raise Bad("stream of %d bytes" % len(b))
    if b[:4] != b"TPF0":
        raise Bad("no TPF0 signature")
    r = Reader(b)
    r.i = 4
    lines = []
    obj(r, lines, 0)
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------- self check
def sstr(s):
    return bytes([len(s)]) + s.encode("cp1252")


def selftest():
    s = b"TPF0" + sstr("TLJSoundApp") + sstr("LJSoundApp")
    s += sstr("Caption") + b"\x06" + sstr("Audio 'A'\r") + sstr("Width") + b"\x03" + struct.pack("<h", 305)
    s += sstr("Items.Strings") + b"\x01\x06" + sstr("a") + b"\x06" + sstr("b") + b"\x00"
    s += sstr("Font.Style") + b"\x0b" + sstr("fsBold") + b"\x00" + sstr("Glyph.Data") + b"\x0a" + struct.pack("<I", 24) + bytes(range(24))
    s += sstr("Columns") + b"\x0e\x01" + sstr("Caption") + b"\x06" + sstr("L #") + sstr("Width") + b"\x02\x20" + b"\x00\x00"
    s += b"\x00"                                                         # end of the form's properties
    s += sstr("TLabel") + sstr("Label1") + sstr("Visible") + b"\x08" + b"\x00" + b"\x00\x00"   # child, no children
    s += b"\x00"                                                         # end of the form's children
    t = tpf0_text(s)
    want = ("object LJSoundApp: TLJSoundApp\n  Caption = 'Audio 'A''#13\n  Width = 305\n  Items.Strings = ('a' 'b')\n"
            "  Font.Style = [fsBold]\n  Glyph.Data = {000102030405060708090a0b0c0d0e0f10111213}...\n"
            "  Columns = <<Caption='L #' Width=32>>\n  object Label1: TLabel\n    Visible = False\n  end\nend\n")
    assert t == want, t
    for bad in (s[:-3], s[:40], b"TPF0" + sstr("T") + sstr("x") + sstr("A") + b"\x0a" + struct.pack("<I", 1 << 30), b"XPF0" + s[4:]):
        try:
            tpf0_text(bad)
        except Bad:
            continue
        raise AssertionError("accepted a bad stream: %r" % bad[:24])
    try:
        rcdata(b"MZ" + b"\0" * 58 + struct.pack("<I", 1 << 20))
        raise AssertionError("accepted a PE header past the end")
    except Bad:
        pass
    print("pe_tpf0 --selftest: OK")
    return 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) < 3:
        print(__doc__)
        return 2
    try:
        res = rcdata(open(argv[1], "rb").read())
        want = [n.upper() for n in argv[3:]]
        done = {}
        for nm, data in sorted(res.items()):
            if data[:4] == b"TPF0" and (not want or nm.upper() in want):
                done[nm] = tpf0_text(data)
    except Bad as e:
        print("pe_tpf0: %s: %s" % (argv[1], e))
        return 1
    for n in want:
        if n not in {k.upper() for k in done}:
            print("pe_tpf0: %s has no TPF0 resource %s" % (argv[1], n))
            return 1
    for nm, text in done.items():
        p = os.path.join(argv[2], nm + ".dfm.txt")
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("pe_tpf0: %s -> %s (%d lines)" % (nm, p, text.count("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
