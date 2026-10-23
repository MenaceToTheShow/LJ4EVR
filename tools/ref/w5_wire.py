"""w5_wire.py - loopback peers for the W5 acceptance walk (docs/w5-integration.md "Acceptance"). Ours, not a vault copy.
Python 3 stdlib only; run with `python -I`. Everything binds 127.0.0.x: no LAN, no multicast join, no broadcast off-host.

    w5_wire.py prep <userdir>                         the scratch W5Acc library, Hardware.Ini, tone.wav, extapp.ini
    w5_wire.py node <secs> [--answer <secs>]          Art-Net node on 127.0.0.2:6454 (Port-Address 2) + sACN unicast
                                                      listener on 127.0.0.2:5568; answers ArtPoll for --answer seconds,
                                                      then stays silent and keeps counting; prints a JSON summary
    w5_wire.py send sacn|artnet <universe> <step>...  40 Hz frames to 127.0.0.1 (sACN 5568 / Art-Net 6454) from
                                                      127.0.0.2; step = <t_ms>:<ch>=<v>[,<ch>=<v>...] or <t_ms>:stop
    w5_wire.py tcp-send <port> <step>...              connects to 127.0.0.1:<port> (retrying every 0.2 s while refused);
                                                      step = <t_ms after connect>:<b> <b> <b> or <t_ms>:flood=<n> (n random
                                                      bytes, none 128..136) or <t_ms>:close
    w5_wire.py tcp-recv <port> <secs> [--close <s>]   listens on 127.0.0.1:<port>, prints every byte received with its
                                                      time; --close drops the peer and stops listening after s seconds
"""
import importlib.util, json, os, random, socket, struct, sys, time, wave

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("sacn_ref", os.path.join(HERE, "08-sacn_ref.py"))
sacn_ref = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sacn_ref)

NODE_IP, APP_IP = "127.0.0.2", "127.0.0.1"
ARTNET_PORT, SACN_PORT = 6454, 5568
CID = bytes.fromhex("5735a1c0e1ad4bd58d6f00000000aced")   # the walk's sender CID (not the app's)


def log(*a):
    print("%8.3f" % (time.monotonic() - T0), *a, flush=True)


T0 = time.monotonic()


# ---- Art-Net (spec v1.4 / Art-Net 4: ArtPoll 0x2000, ArtPollReply 0x2100, ArtDmx 0x5000) ----
def artdmx(port_address, seq, slots):
    return (b"Art-Net\0" + struct.pack("<H", 0x5000) + bytes([0, 14, seq & 0xFF, 0, port_address & 0xFF,
            (port_address >> 8) & 0x7F]) + struct.pack(">H", len(slots)) + bytes(slots))


def artpollreply(ip, port_address):
    p = bytearray(239)
    p[0:8] = b"Art-Net\0"
    p[8:10] = struct.pack("<H", 0x2100)
    p[10:14] = socket.inet_aton(ip)
    p[14:16] = struct.pack("<H", ARTNET_PORT)
    p[18], p[19] = (port_address >> 8) & 0x7F, (port_address >> 4) & 0x0F   # NetSwitch, SubSwitch
    p[26:26 + 9] = b"w5 node\0\0"
    p[44:44 + 14] = b"W5 test node\0\0"
    p[173] = 1                                                               # NumPorts lo
    p[174] = 0x80                                                            # PortTypes[0]: output
    p[190] = port_address & 0x0F                                             # SwOut[0]
    return bytes(p)


def node(secs, answer):
    a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); a.bind((NODE_IP, ARTNET_PORT)); a.setblocking(False)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind((NODE_IP, SACN_PORT)); s.setblocking(False)
    polls, dmx, sacn, replies = [], [], [], 0
    end = T0 + secs
    import select
    while time.monotonic() < end:
        r, _, _ = select.select([a, s], [], [], 0.05)
        now = time.monotonic() - T0
        if a in r:
            buf, src = a.recvfrom(2048)
            if buf[:8] == b"Art-Net\0" and len(buf) >= 10:
                op = struct.unpack_from("<H", buf, 8)[0]
                if op == 0x2000:
                    polls.append((round(now, 3), src[0], len(buf)))
                    if now < answer:
                        a.sendto(artpollreply(NODE_IP, 2), (src[0], ARTNET_PORT)); replies += 1
                elif op == 0x5000 and len(buf) >= 18:
                    pa = buf[14] | (buf[15] << 8)
                    dmx.append((round(now, 3), src[0], buf[12], pa, struct.unpack_from(">H", buf, 16)[0], buf[18], len(buf)))
        if s in r:
            buf, src = s.recvfrom(2048)
            try:
                d = sacn_ref.parse(buf)
                if d["kind"] == "data":
                    sacn.append((round(now, 3), d["universe"], d["slots"][0] if d["slots"] else -1, d["options"]))
            except ValueError:
                pass
    def rate(rows, t0, t1):
        n = sum(1 for x in rows if t0 <= x[0] < t1)
        return round(n / (t1 - t0), 1) if t1 > t0 else 0
    gaps = [round(dmx[i][0] - dmx[i - 1][0], 3) for i in range(1, len(dmx))]
    out = dict(
        artpoll=dict(count=len(polls), first=polls[:2], intervals=[round(polls[i][0] - polls[i - 1][0], 2) for i in range(1, len(polls))],
                     replies_sent=replies, answer_until=answer),
        artdmx=dict(count=len(dmx), universes=sorted({x[3] for x in dmx}), lengths=sorted({x[6] for x in dmx}),
                    seq_zero=sum(1 for x in dmx if x[2] == 0), first=dmx[:1], last=dmx[-1:],
                    slot1_values=[x[5] for x in dmx][:400:20], gaps_over_100ms=[g for g in gaps if g > 0.1][:20],
                    per_second=[sum(1 for x in dmx if k <= x[0] < k + 1) for k in range(int(secs))],
                    after_answer_plus_8s=sum(1 for x in dmx if x[0] >= answer + 8.0)),
        sacn=dict(count=len(sacn), universes=sorted({x[1] for x in sacn}), slot1_last=sacn[-1][2] if sacn else None,
                  per_second=[sum(1 for x in sacn if k <= x[0] < k + 1) for k in range(int(secs))]))
    print(json.dumps(out, indent=1), flush=True)


# ---- senders ----
def parse_steps(steps):
    out = []
    for st in steps:
        t, _, rest = st.partition(":")
        out.append((int(t) / 1000.0, rest))
    return out


def send(proto, universe, steps):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind((NODE_IP, 0))
    slots, seq, sent, k, steps = bytearray(512), 0, 0, 0, parse_steps(steps)
    dst = (APP_IP, SACN_PORT if proto == "sacn" else ARTNET_PORT)
    while True:
        now = time.monotonic() - T0
        while k < len(steps) and steps[k][0] <= now:
            if steps[k][1] == "stop":
                log(proto, "stop after", sent, "packets"); return
            for pair in steps[k][1].split(","):
                ch, v = pair.split("="); slots[int(ch) - 1] = int(v)
            log(proto, "u%d" % universe, steps[k][1]); k += 1
        seq, sent = (seq + 1) & 0xFF, sent + 1
        if proto == "sacn":
            pkt = sacn_ref.data_packet(CID, "W5 walk", universe, slots, seq)
        else:
            pkt = artdmx(universe, seq or 1, slots)
        try:
            s.sendto(pkt, dst)
        except OSError:                                   # Windows: ICMP port unreachable from a closed port, keep going
            pass
        time.sleep(max(0.0, 0.025 - (time.monotonic() - T0 - now)))


def tcp_send(port, steps):
    refused = 0
    while True:                                           # retry until the listener exists (W5-X-OFF: refused before)
        try:
            c = socket.create_connection((APP_IP, port), timeout=2); break
        except OSError:
            refused += 1; time.sleep(0.2)
    t0 = time.monotonic()
    log("connected", c.getsockname()[0], "->", APP_IP, port, "after", refused, "refused attempt(s)")
    for t, what in parse_steps(steps):
        time.sleep(max(0, t - (time.monotonic() - t0)))
        if what == "close":
            c.close(); log("closed"); return
        if what.startswith("flood="):
            n = int(what[6:]); rnd = random.Random(5)          # never 128..136, so no valid command can start
            c.sendall(bytes(rnd.choice([rnd.randrange(128), rnd.randrange(137, 256)]) for _ in range(n))); log("flood", n, "bytes")
        else:
            b = bytes(int(x) for x in what.split()); c.sendall(b); log("sent", list(b))
    c.close()


def tcp_recv(port, secs, close_after):
    ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ls.bind((APP_IP, port)); ls.listen(4)
    ls.settimeout(0.1)
    conns, end = [], T0 + secs
    while time.monotonic() < end:
        if close_after and time.monotonic() - T0 >= close_after:
            for c in conns: c.close()
            ls.close(); log("receiver closed (dead peer from here)"); conns = []; close_after = 0
            while time.monotonic() < end: time.sleep(0.1)
            break
        try:
            c, src = ls.accept(); c.setblocking(False); conns.append(c); log("accepted from", src[0])
        except (socket.timeout, OSError):
            pass
        for c in conns:
            try:
                b = c.recv(4096)
                if b: log("recv", b.hex(" "), repr(b))
            except BlockingIOError:
                pass


# ---- the scratch library (our own text, design 5.2 grammar; docs/file-formats.md) ----
def prep(ud):
    lib = os.path.join(ud, "User", "W5Acc")
    for sub in ("Sequences", "Cues", "CueLists", "DmxIn", "Hotkeys"):
        os.makedirs(os.path.join(lib, sub), exist_ok=True)
    def put(path, text):
        with open(path, "w", encoding="cp1252", newline="\r\n") as f: f.write(text)
    put(os.path.join(ud, "Hardware.Ini"), "[SACN]\nNic=127.0.0.1\nName=LJ4EVR\nCID=5735a1c0e1ad4bd58d6f00000000a001\nPriority=100\nTTL=1\nPreview=0\nUnlockRate=0\n"
        "U1=1\nUnicast1=127.0.0.2\nU2=2\nUnicast2=\nU3=3\nUnicast3=\nU4=4\nUnicast4=\n")
    put(os.path.join(lib, "Config.dat"), "LJ4 config 1\n"
        "fx 1 kind=builtin id=19 gs2= link=1 addr=1 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=DIM1 placed=1 x=40 y=40 tab=0 sub=0\n"
        "fx 2 kind=builtin id=19 gs2= link=2 addr=1 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=DIM2 placed=1 x=110 y=40 tab=0 sub=0\n"
        "fx 3 kind=builtin id=90 gs2= link=3 addr=1 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=EXTAPP placed=1 x=180 y=40 tab=0 sub=0\n")
    seqs = {1: ("Fade", ["scene st=1000 ft=3000\nch 1:1=255:F 2:1=255:F"]),
            2: ("Half", ["scene st=1000 ft=0\nch 1:1=128:S"]),
            3: ("Steps", ["scene st=60000 ft=0\nch 1:1=%d:S" % v for v in (40, 80, 120, 160)]),
            4: ("Thirty", ["scene st=1000 ft=0\nch 1:1=30:S"]),
            5: ("Go", ["scene st=1000 ft=0\nch 1:1=200:S"]),
            6: ("ExtApp", ["scene st=1000 ft=0\nch 1:1=10:S\next 3 cmd=TRIG!1", "scene st=1000 ft=0\nch 1:1=20:S\next 3 cmd=TRIG!2",
                           "scene st=1000 ft=0\nch 1:1=30:S\next 3 cmd=V#X7F", "scene st=1000 ft=0\nch 1:1=40:S\next 3 cmd=V#X7F"]),
            7: ("MP", ["scene st=1000 ft=0\nch 1:1=99:S"])}
    for n, (name, scenes) in seqs.items():
        put(os.path.join(lib, "Sequences", "%08d.seq" % n), "LJ4 seq 1\nnum=%d\nname=%s\ntype=\nsync=0 gst=0 gft=0\nscenes=%d\n%s\n"
            % (n, name, len(scenes), "\n".join(scenes)))
    cues = {1: (1, "auto", 1), 2: (5, "auto", 1), 3: (7, "auto", 1), 4: (3, "audio", 2), 5: (2, "auto", 1), 6: (6, "auto", 1), 130: (4, "auto", 1)}
    for n, (seq, trig, atpt) in cues.items():
        put(os.path.join(lib, "Cues", "%08d.cue" % n), "LJ4 cue 1\nnum=%d\nname=Cue%d\ntype=\ncuetime=100\nmacro=100,100 savemacro=0\n"
            "transparent=0\nbgcue=0 genmacro=0\nslot 1 seq=%d on=1 trig=%s dir=fwd spt=1 atpt=%d tol=0 first=0 last=0 loops=0 end=stop "
            "goto=0 link=0 time=0 chase=0\n" % (n, n, seq, trig, atpt))
    L = "line %d adv=%s cmd=%s param=%d pstr=%s ref=0 cue=%d trig=0 comment=\n"
    cls = {1: ("Manual", [("manual", "none", 0, "", 2)]),
           2: ("MP", [("auto", "mp_load_file", 0, "tone.wav", 0), ("auto", "mp_play", 0, "", 0), ("auto", "mp_wait", 2000, "", 0),
                      ("auto", "none", 0, "", 3), ("auto", "mp_wait", 3800, "", 0)]),
           3: ("Fade", [("auto", "none", 0, "", 1)]),
           4: ("ExtApp", [("auto", "none", 0, "", 6)]),
           5: ("Audio", [("auto", "none", 0, "", 4)]),
           6: ("Refused", [("auto", "mp_load_file", 0, "clip.mp3", 0), ("auto", "mp_load_file", 0, "clip.avi", 0)])}
    for name, head in (("clip.mp3", b"ID3\x04\x00\x00\x00\x00\x00\x00"), ("clip.avi", b"RIFF\x04\x00\x00\x00AVI ")):
        with open(os.path.join(ud, name), "wb") as f: f.write(head)    # our own 10 / 12 bytes: refused by extension
    for n, (name, lines) in cls.items():
        put(os.path.join(lib, "CueLists", "%08d.Cl2" % n), "LJ4 cl 1\nnum=%d\nname=%s\ntracking=1\n" % (n, name)
            + "".join(L % ((i + 1,) + ln) for i, ln in enumerate(lines)))
    put(os.path.join(lib, "DirInfo.Ini"), "[INFO]\nName=W5Acc\n[NEXT]\nSeq=8\nCue=131\nCl=6\nMacro=1\n")
    put(os.path.join(lib, "Hotkeys", "Setup1.Hot"), "LJ4 hot 1\nname=W5\nkey vk=69 shift=0 ctrl=0 scrl=0 fn=LJ_FN_EMULATE_AUDIO_TRIG\n")
    put(os.path.join(lib, "GlobalPatch.GPA"), "LJ4 patch 1\nch 1:1 src=in sl=1 sc=2 mode=htp inv=0 min=0 max=255 htp=0\n"
        "ch 1:3 src=in sl=1 sc=3 mode=abs inv=0 min=0 max=255 htp=0\n")
    put(os.path.join(ud, "extapp.ini"), "[IDENTIFIERS]\nNAME=W5 walk receiver\nCOMMANDINTERFACE=ZSTRING\n[LJ4EVR]\nTRANSPORT=TCP\n"
        "HOST=127.0.0.1\nPORT=7000\n")
    with wave.open(os.path.join(ud, "tone.wav"), "wb") as w:     # 4 s of a 440 Hz-ish integer triangle, 44.1 kHz mono 16-bit
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
        w.writeframes(b"".join(struct.pack("<h", ((i * 1280) % 40000) - 20000 >> 3) for i in range(4 * 44100)))
    print("prep: W5Acc library, Hardware.Ini, extapp.ini, tone.wav under", "<userdir>")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["prep"]: prep(a[1])
    elif a[:1] == ["node"]: node(float(a[1]), float(a[a.index("--answer") + 1]) if "--answer" in a else 1e9)
    elif a[:1] == ["send"]: send(a[1], int(a[2]), a[3:])
    elif a[:1] == ["tcp-send"]: tcp_send(int(a[1]), a[2:])
    elif a[:1] == ["tcp-recv"]: tcp_recv(int(a[1]), float(a[2]), float(a[a.index("--close") + 1]) if "--close" in a else 0)
    else: print(__doc__); sys.exit(2)
