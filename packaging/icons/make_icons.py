#!/usr/bin/env python3
"""make_icons.py - draws the LJ4EVR icon (our own art, D2: no Martin bitmaps).

A moving-head silhouette on a dark disc: base, yoke, tilted head, beam. Drawn at
1024 px and resampled down. Writes, next to this script:
  lj4evr-256.png, lj4evr-32.png   (CPack / docs)
  lj4evr.ico                      (NSIS MUI icon + Windows exe resource; 16..256)
  lj4evr.icns                     (macOS bundle icon; Pillow's ICNS writer, no iconutil)

usage: python -I make_icons.py [--check]   (--check: regenerate to memory and compare bytes)
"""
import io
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
S = 1024
DISC, BODY, BEAM, LENS = (28, 30, 38), (214, 216, 222), (255, 214, 64), (255, 240, 160)


def draw_master():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((16, 16, S - 16, S - 16), fill=DISC)
    # beam: a fan from the lens up-right, under the body
    d.polygon([(560, 390), (930, 60), (990, 180)], fill=BEAM + (90,))
    d.polygon([(560, 390), (900, 90), (960, 190)], fill=BEAM + (140,))
    # base + stem
    d.rounded_rectangle((300, 820, 724, 930), radius=40, fill=BODY)
    d.rectangle((472, 700, 552, 830), fill=BODY)
    # yoke: two arms on a shaft
    d.rounded_rectangle((330, 640, 694, 720), radius=36, fill=BODY)
    d.rounded_rectangle((330, 400, 410, 700), radius=36, fill=BODY)
    d.rounded_rectangle((614, 400, 694, 700), radius=36, fill=BODY)
    # head: a tilted box between the arms, rotated about its centre
    head = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    hd = ImageDraw.Draw(head)
    hd.rounded_rectangle((412, 380, 612, 640), radius=50, fill=BODY)
    hd.ellipse((452, 400, 572, 470), fill=LENS)
    head = head.rotate(-35, center=(512, 510), resample=Image.BICUBIC)
    im.alpha_composite(head)
    return im


def render():
    master = draw_master()
    out = {}
    for n in (16, 32, 48, 64, 128, 256, 512):
        out[n] = master.resize((n, n), Image.LANCZOS)
    return out


def encode(sizes):
    files = {}
    for n in (32, 256):
        b = io.BytesIO(); sizes[n].save(b, "PNG", optimize=True); files[f"lj4evr-{n}.png"] = b.getvalue()
    b = io.BytesIO()
    sizes[256].save(b, "ICO", sizes=[(n, n) for n in (16, 32, 48, 64, 128, 256)],
                    append_images=[sizes[n] for n in (16, 32, 48, 64, 128)])
    files["lj4evr.ico"] = b.getvalue()
    b = io.BytesIO()
    sizes[512].save(b, "ICNS", append_images=[sizes[n] for n in (16, 32, 128, 256)])
    files["lj4evr.icns"] = b.getvalue()
    return files


def main(argv):
    files = encode(render())
    if "--check" in argv:
        bad = [k for k, v in files.items() if not os.path.exists(os.path.join(HERE, k))
               or open(os.path.join(HERE, k), "rb").read() != v]
        print("icons stale: " + ", ".join(bad) if bad else "icons current")
        return 1 if bad else 0
    for k, v in files.items():
        with open(os.path.join(HERE, k), "wb") as f:
            f.write(v)
        print(f"{k}: {len(v)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
