"""Decode Martin LightJockey 2 Macros.Mac (generic DMX macro shape tables).

Usage:  python -I 14-macros_mac_decode.py "D:/lj4evr/reference/installer/extracted_2.209/app/Macros.Mac"

Layout (derived in 14-gap-generic-macros-and-statics2.md, section A.6):
  file header  8 B : int32 LE version (=1), int32 LE record count (=66), then 4096 zero bytes
  record    16415 B: ShortString[15] name (len byte + 15 chars), 15 unknown bytes, 8192 x int16 LE samples
  sample scale     : 0 .. +-32640 (= 127.5 * 256); 2*Sine == 32640*sin(pi*k/8192), Sine == 32640*sin(2*pi*k/8192)
The file is read as data only.
"""
import math
import struct
import sys

HDR, REC, NSAMP = 4104, 16415, 8192


def decode(path):
    d = open(path, "rb").read()
    ver, n = struct.unpack_from("<II", d, 0)
    assert ver == 1 and HDR + n * REC == len(d), (ver, n, len(d))
    assert not any(d[8:HDR]), "reserved header bytes are not all zero"
    out = []
    for i in range(n):
        r = HDR + i * REC
        name = d[r + 1:r + 1 + d[r]].decode("latin1")
        tbl = struct.unpack_from("<%dh" % NSAMP, d, r + 31)
        out.append((name, d[r + 16:r + 31], tbl))
    return out


def demo(path):
    recs = decode(path)
    assert len(recs) == 66
    names = [r[0] for r in recs]
    assert names[0] == "2*Sine" and names[7] == "Sine" and names[42] == "Square"
    t = recs[0][2]
    assert max(abs(t[k] - round(32640 * math.sin(math.pi * k / NSAMP))) for k in range(NSAMP)) == 0
    t = recs[7][2]
    assert max(abs(t[k] - round(32640 * math.sin(2 * math.pi * k / NSAMP))) for k in range(NSAMP)) == 0
    assert set(recs[42][2]) == {32640, -32640}
    for i, (name, _, tbl) in enumerate(recs):
        print("%2d %-15s min=%6d max=%6d" % (i, name, min(tbl), max(tbl)))
    print("ok")


if __name__ == "__main__":
    demo(sys.argv[1])
