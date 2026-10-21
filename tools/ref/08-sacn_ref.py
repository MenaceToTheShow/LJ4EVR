"""sacn_ref.py - ANSI E1.31-2018 reference sender + monitor, Python 3 stdlib only.

Section numbers refer to ANSI E1.31-2018 (CP/2014-1009r6a).
    python sacn_ref.py                                   -> self-test against the standard's Appendix B example
    python sacn_ref.py send <local_ip> <universe> [--unicast <ip>]   -> chase slots 1-8 at 30 Hz
    python sacn_ref.py monitor <local_ip> <universe>                  -> print every packet seen
"""
import socket, struct, sys, time, uuid, threading

PORT = 5568                                 # ACN_SDT_MULTICAST_PORT, App. A
ACN_PID = b"ASC-E1.17\x00\x00\x00"          # 5.3
VECTOR_ROOT_E131_DATA = 0x00000004          # App. A
VECTOR_ROOT_E131_EXTENDED = 0x00000008
VECTOR_E131_DATA_PACKET = 0x00000002
VECTOR_E131_EXTENDED_SYNCHRONIZATION = 0x00000001
VECTOR_E131_EXTENDED_DISCOVERY = 0x00000002
VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST = 0x00000001
VECTOR_DMP_SET_PROPERTY = 0x02
DISCOVERY_UNIVERSE = 64214                  # App. A
DISCOVERY_INTERVAL = 10.0                   # s, App. A
DATA_LOSS_TIMEOUT = 2.5                     # s, App. A
KEEPALIVE = 0.9                             # s, 6.6.2 says 800-1000 ms
OPT_PREVIEW, OPT_TERMINATED, OPT_FORCE_SYNC = 0x80, 0x40, 0x20   # 6.2.6 bits 7, 6, 5


def flags_len(n):                           # 5.4 / 6.1 / 7.1: high nibble 0x7, low 12 bits = PDU length
    assert 0 < n < 4096
    return struct.pack(">H", 0x7000 | n)


def multicast_addr(universe):               # 9.3.1 Table 9-1: 239.255.<hi>.<lo>
    assert 1 <= universe <= 63999 or universe == DISCOVERY_UNIVERSE
    return "239.255.%d.%d" % (universe >> 8, universe & 0xFF)


def root_layer(cid, vector, pdu_len):       # Table 5-1, 38 octets
    return struct.pack(">HH12s", 0x0010, 0x0000, ACN_PID) + flags_len(pdu_len - 16) + struct.pack(">I", vector) + cid


def data_packet(cid, name, universe, slots, seq, priority=100, options=0, sync_addr=0, start_code=0):
    """Table 4-1. slots = bytes, 1..512 data slots. Returns the full UDP payload."""
    assert len(cid) == 16 and 1 <= len(slots) <= 512 and 0 <= priority <= 200
    prop = bytes([start_code]) + bytes(slots)          # 7.7: slot 0 is the START code
    total = 125 + len(prop)                            # octets 0-124 header + 513 property values = 638 (5.4)
    dmp = flags_len(total - 115) + struct.pack(">BBHHH", VECTOR_DMP_SET_PROPERTY, 0xA1, 0x0000, 0x0001, len(prop)) + prop
    frame = (flags_len(total - 38) + struct.pack(">I", VECTOR_E131_DATA_PACKET)
             + name.encode("utf-8")[:63].ljust(64, b"\0")            # 6.2.2: 64 octets, null terminated
             + struct.pack(">BHBBH", priority, sync_addr, seq & 0xFF, options, universe))
    return root_layer(cid, VECTOR_ROOT_E131_DATA, total) + frame + dmp


def sync_packet(cid, sync_addr, seq):       # Table 4-2, 49 octets
    assert sync_addr != 0                   # 6.3.3.1
    frame = flags_len(49 - 38) + struct.pack(">IBHH", VECTOR_E131_EXTENDED_SYNCHRONIZATION, seq & 0xFF, sync_addr, 0)
    return root_layer(cid, VECTOR_ROOT_E131_EXTENDED, 49) + frame


def discovery_packet(cid, name, universes, page=0, last_page=0):   # Table 4-3, 120 + 2n octets
    assert len(universes) <= 512
    ul = b"".join(struct.pack(">H", u) for u in sorted(universes))  # 8.5: sorted
    total = 120 + len(ul)
    disc = flags_len(total - 112) + struct.pack(">IBB", VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST, page, last_page) + ul
    frame = (flags_len(total - 38) + struct.pack(">I", VECTOR_E131_EXTENDED_DISCOVERY)
             + name.encode("utf-8")[:63].ljust(64, b"\0") + b"\0\0\0\0")
    return root_layer(cid, VECTOR_ROOT_E131_EXTENDED, total) + frame + disc


def parse(buf):
    """Validate per 5.1-5.5, 6.5, 7.2-7.5 and return a dict, or raise ValueError."""
    if len(buf) < 48:
        raise ValueError("short")
    pre, post, pid, rfl, rvec = struct.unpack_from(">HH12sHI", buf, 0)
    if pre != 0x10 or post != 0 or pid != ACN_PID or rfl >> 12 != 7:
        raise ValueError("bad root layer")
    cid = buf[22:38]
    ffl, fvec = struct.unpack_from(">HI", buf, 38)
    if rvec == VECTOR_ROOT_E131_DATA and fvec == VECTOR_E131_DATA_PACKET:
        name = buf[44:108].split(b"\0", 1)[0].decode("utf-8", "replace")
        prio, sync, seq, opts, uni = struct.unpack_from(">BHBBH", buf, 108)
        dfl, dvec, typ, first, inc, cnt = struct.unpack_from(">HBBHHH", buf, 115)
        if dvec != VECTOR_DMP_SET_PROPERTY or typ != 0xA1 or first != 0 or inc != 1:
            raise ValueError("bad DMP layer")
        prop = buf[125:125 + cnt]
        return dict(kind="data", cid=cid, name=name, priority=prio, sync=sync, seq=seq, options=opts,
                    universe=uni, start_code=prop[0], slots=prop[1:])
    if rvec == VECTOR_ROOT_E131_EXTENDED and fvec == VECTOR_E131_EXTENDED_SYNCHRONIZATION:
        seq, sync = struct.unpack_from(">BH", buf, 44)
        return dict(kind="sync", cid=cid, seq=seq, sync=sync)
    if rvec == VECTOR_ROOT_E131_EXTENDED and fvec == VECTOR_E131_EXTENDED_DISCOVERY:
        name = buf[44:108].split(b"\0", 1)[0].decode("utf-8", "replace")
        ufl, uvec, page, last = struct.unpack_from(">HIBB", buf, 112)
        n = (ufl & 0xFFF) - 8
        unis = struct.unpack_from(">%dH" % (n // 2), buf, 120)
        return dict(kind="discovery", cid=cid, name=name, page=page, last_page=last, universes=unis)
    raise ValueError("unknown vector")


def out_of_sequence(new, last):             # 6.7.2: signed 8-bit B-A in (-20, 0] -> discard
    d = ((new - last + 128) & 0xFF) - 128
    return -20 < d <= 0


class Sender:
    """One UDP socket for every universe. Handles 6.6.2 suppression/keep-alive, 6.2.6 termination, 4.3 discovery."""

    def __init__(self, local_ip, name="LJ4EVR", cid=None, priority=100, ttl=1, discovery=True):
        self.cid = cid or uuid.uuid4().bytes          # 5.6: should persist for the component's lifetime
        self.name, self.priority = name, priority
        self.discovery = discovery                     # 4.3 is mandatory for sources; off only for tests
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))  # pick NIC
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
        self.unicast = {}                              # universe -> ip (else multicast)
        self._state = {}                               # universe -> [slots, seq, repeats_left, last_send_time]
        self._lock = threading.Lock()
        self._stop = threading.Event()
        threading.Thread(target=self._loop, daemon=True).start()

    def _dest(self, u):
        return (self.unicast.get(u) or multicast_addr(u), PORT)

    def _send(self, payload, dest):
        try:
            self.sock.sendto(payload, dest)
        except OSError as e:                           # NIC down / unreachable (WinError 10051): log, keep running
            print("sACN send failed to %s: %s" % (dest, e), file=sys.stderr)

    def _emit(self, u, st, options=0):
        self._send(data_packet(self.cid, self.name, u, st[0], st[1], self.priority, options), self._dest(u))
        st[1] = (st[1] + 1) & 0xFF                     # 6.2.5: per-universe sequence, +1 every packet
        st[3] = time.monotonic()

    def set(self, universe, slots):
        """Call at any rate; packets go out only on change (+2 repeats) and as keep-alives (6.6.2)."""
        slots = bytes(slots)
        with self._lock:
            st = self._state.setdefault(universe, [b"", 0, 0, 0.0])
            if slots != st[0]:
                st[0], st[2] = slots, 3               # 6.6.2 item 1: three packets before suppression
                self._emit(universe, st); st[2] -= 1

    def _loop(self):
        next_disc = 0.0
        while not self._stop.is_set():
            now = time.monotonic()
            with self._lock:
                for u, st in self._state.items():
                    if st[2] > 0 or now - st[3] >= KEEPALIVE:   # repeats, then 800-1000 ms keep-alive
                        self._emit(u, st); st[2] = max(0, st[2] - 1)
                if self.discovery and now >= next_disc:        # 4.3: every E131_UNIVERSE_DISCOVERY_INTERVAL
                    unis = sorted(self._state)
                    pages = [unis[i:i + 512] for i in range(0, len(unis), 512)] or [[]]
                    for p, chunk in enumerate(pages):
                        self._send(discovery_packet(self.cid, self.name, chunk, p, len(pages) - 1),
                                   (multicast_addr(DISCOVERY_UNIVERSE), PORT))
                    next_disc = now + DISCOVERY_INTERVAL
            time.sleep(0.01)   # ponytail: 100 Hz tick; a real app drives this from its DMX refresh clock

    def close(self):
        self._stop.set()
        with self._lock:
            for u, st in self._state.items():
                for _ in range(3):                             # 6.2.6: three Stream_Terminated packets
                    self._emit(u, st, OPT_TERMINATED)
        self.sock.close()


def open_receiver(local_ip, universe):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            # share 5568 with other monitors
    if hasattr(socket, "SO_REUSEPORT"):                                # macOS/BSD: needed for duplicate mcast binds
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    s.bind(("", PORT))                                                  # INADDR_ANY works on all three OSes
    mreq = socket.inet_aton(multicast_addr(universe)) + socket.inet_aton(local_ip)   # struct ip_mreq
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    return s


def monitor(local_ip, universe):
    s = open_receiver(local_ip, universe)
    last_seq, last_seen = {}, {}
    s.settimeout(0.5)
    while True:
        try:
            buf, (ip, _) = s.recvfrom(2048)
        except socket.timeout:
            buf = None
        now = time.monotonic()
        for key, t in list(last_seen.items()):
            if now - t > DATA_LOSS_TIMEOUT:                             # 6.7.1 network data loss
                print("LOST  %s universe %d (2.5 s silence)" % key); del last_seen[key]
        if not buf:
            continue
        try:
            p = parse(buf)
        except ValueError as e:
            print("DROP  %s: %s" % (ip, e)); continue
        if p["kind"] != "data":
            print("%-5s %s %s" % (p["kind"].upper(), ip, {k: v for k, v in p.items() if k not in ("cid", "kind")})); continue
        key = (p["cid"].hex()[:8], p["universe"])
        if key in last_seq and out_of_sequence(p["seq"], last_seq[key]):
            print("OOO   %s seq %d after %d" % (key, p["seq"], last_seq[key])); continue
        last_seq[key] = p["seq"]
        if p["options"] & OPT_TERMINATED:
            print("TERM  %s" % (key,)); last_seen.pop(key, None); continue
        last_seen[key] = now
        print("DATA  %s u%-5d pri %3d seq %3d sc 0x%02X %s%s slots[1..8]=%s" % (
            ip, p["universe"], p["priority"], p["seq"], p["start_code"],
            "PREVIEW " if p["options"] & OPT_PREVIEW else "", p["name"][:16], list(p["slots"][:8])))


def selftest():
    cid = bytes.fromhex("ef07c8dd00644401a3a2459ef8e6143e")   # Appendix B example CID
    pkt = data_packet(cid, "Source_A", 1, bytes(512), 154, sync_addr=7962)
    assert len(pkt) == 638
    assert pkt[16:18] == b"\x72\x6e" and pkt[38:40] == b"\x72\x58" and pkt[115:117] == b"\x72\x0b"   # Table B-1
    assert pkt[108] == 100 and pkt[109:111] == b"\x1f\x1a" and pkt[111] == 154 and pkt[113:115] == b"\x00\x01"
    assert multicast_addr(7962) == "239.255.31.26"            # B.2
    assert multicast_addr(DISCOVERY_UNIVERSE) == "239.255.250.214"
    sp = sync_packet(cid, 7962, 367)
    assert len(sp) == 49 and sp[16:18] == b"\x70\x21" and sp[38:40] == b"\x70\x0b"   # Table B-2
    dp = discovery_packet(cid, "Source_A", [3, 1, 2])
    assert len(dp) == 126 and parse(dp)["universes"] == (1, 2, 3)
    p = parse(pkt); assert p["universe"] == 1 and p["sync"] == 7962 and len(p["slots"]) == 512
    assert out_of_sequence(5, 5) and out_of_sequence(250, 255) and not out_of_sequence(6, 5)
    assert not out_of_sequence(0, 200) and out_of_sequence(100, 110)   # 6.7.2: -200 wraps to +56 -> accept
    # loopback: unicast to 127.0.0.1 through the real Sender
    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); rx.bind(("127.0.0.1", 0)); rx.settimeout(2)
    port = rx.getsockname()[1]
    tx = Sender("127.0.0.1", "selftest", cid, discovery=False)   # multicast via 127.0.0.1 is unreachable on Windows
    tx._dest = lambda u: ("127.0.0.1", port)
    tx.set(1, b"\xff" * 8); time.sleep(0.05)
    got = [parse(rx.recv(2048)) for _ in range(3)]          # change + 2 repeats (6.6.2 item 1)
    assert [g["seq"] for g in got] == [0, 1, 2] and got[0]["slots"][:8] == b"\xff" * 8
    time.sleep(1.0); k = parse(rx.recv(2048)); assert k["seq"] == 3 and k["slots"] == got[0]["slots"]   # keep-alive
    tx.close()
    term = [parse(rx.recv(2048)) for _ in range(3)]
    assert all(t["options"] & OPT_TERMINATED for t in term)
    print("selftest OK")


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a:
        selftest()
    elif a[0] == "monitor":
        monitor(a[1], int(a[2]))
    elif a[0] == "send":
        tx = Sender(a[1], "LJ4EVR")
        if "--unicast" in a:
            tx.unicast[int(a[2])] = a[a.index("--unicast") + 1]
        try:
            i = 0
            while True:
                slots = bytearray(8); slots[i % 8] = 255; tx.set(int(a[2]), slots); i += 1; time.sleep(1 / 30)
        except KeyboardInterrupt:
            tx.close()
