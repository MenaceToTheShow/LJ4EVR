/* lj_sacn.c - the sACN (ANSI E1.31-2018) sender, the C port of
 * tools/ref/08-sacn_ref.py Sender (design 8.1, 08 s3-s4, s6.2; ME-6, ME-23, ME-24).
 *
 * Sender state is file-static because lj_sacn.h hands out no handle; only the
 * engine thread calls open/set/pump/close (ME-23), so it needs no lock. The UI
 * never calls in here: it posts LJ_CMD_SACN_CFG and the engine thread calls
 * lj_sacn_open() again, which is close-old (3 x Stream_Terminated for every
 * universe no longer mapped) then open-new.
 *
 * Send mechanics (ME-6): lj_sacn_set() copies and marks, lj_sacn_pump() sends at
 * most one packet per universe per call: a pending repeat, else a keep-alive when
 * 900 ms have passed. A universe sent less than LJ_SACN_MIN_INTERVAL_US ago is
 * skipped unless cfg.unlock_rate (ME-24; the latest frame wins on the next pump).
 * sendto failures are logged (once per distinct error) and counted, never fatal
 * (08 s6.2: WinError 10051 on a dead NIC). */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf/strcat under /W4 /WX on MSVC (as core/lj_ini.c) */
#ifdef _WIN32
#define _CRT_RAND_S                 /* rand_s: the CRT's CSPRNG (RtlGenRandom), no bcrypt.h needed */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET lj_sock;
#define LJ_SOCK_BAD   INVALID_SOCKET
#define lj_sock_close closesocket
#define lj_sock_err() WSAGetLastError()
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
typedef int lj_sock;
#define LJ_SOCK_BAD   (-1)
#define lj_sock_close close
#define lj_sock_err() errno
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io/lj_sacn.h"
#include "io/lj_net.h"
#include "core/lj_clock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

enum { LJ_SACN_OPT_TERMINATED = 0x40, LJ_SACN_OPT_PREVIEW = 0x80, LJ_SACN_DISC_CAP = 120 + 2 * LJ_LINKS };

typedef struct { uint8_t slots[LJ_LINK_CH]; uint8_t seq, repeats_left; uint64_t last_sent_us; uint32_t in_window; } lj_sacn_uni;   /* 08 s4: per-universe state, one per link */

static struct {
    int open;
    lj_sock sock;
    lj_sacn_cfg cfg;                /* validated copy; cfg.universe_of_link[i] is the universe link i feeds, 0 = unmapped */
    lj_sacn_uni uni[LJ_LINKS];
    uint64_t next_disc_us, window_us;
    lj_sacn_stats st;
} lj_s;

/* ---- packet builders (pure) ---------------------------------------------------- */

static void lj_be16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void lj_be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void lj_sacn_root(uint8_t *out, const lj_sacn_cfg *cfg, uint32_t vector, int total)   /* Table 5-1, 38 octets */
{
    static const uint8_t pid[12] = { 'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0 };
    lj_be16(out, 0x0010); lj_be16(out + 2, 0);
    memcpy(out + 4, pid, 12);
    lj_be16(out + 16, 0x7000 | (unsigned)(total - 16));
    lj_be32(out + 18, vector);
    memcpy(out + 22, cfg->cid, 16);
}

static void lj_sacn_name_utf8(uint8_t out[64], const char *name_cp1252)   /* 6.2.2: 64 octets, NUL-terminated, at most 63 bytes, cut on a UTF-8 character boundary (ME-84) */
{
    char u[LJ_SACN_NAME_CAP * 3 + 1];
    size_t n;
    lj_cp1252_to_utf8(u, sizeof u, name_cp1252);
    n = strlen(u);
    if (n > 63) { n = 63; while (n > 0 && ((unsigned char)u[n] & 0xC0) == 0x80) n--; }   /* u[n] is the first byte left out: never cut inside a character */
    memset(out, 0, 64);
    memcpy(out, u, n);
}

int lj_sacn_pack_data(uint8_t out[LJ_SACN_DATA_LEN], const lj_sacn_cfg *cfg, uint16_t universe, uint8_t seq, uint8_t options, const uint8_t slots[LJ_LINK_CH])
{
    if (!out || !cfg || !slots || universe == 0 || universe > LJ_SACN_UNIVERSE_MAX) return -1;
    lj_sacn_root(out, cfg, 0x00000004, LJ_SACN_DATA_LEN);
    lj_be16(out + 38, 0x7000 | (LJ_SACN_DATA_LEN - 38));          /* framing, Table 4-1 */
    lj_be32(out + 40, 0x00000002);
    lj_sacn_name_utf8(out + 44, cfg->name);
    out[108] = cfg->priority;
    lj_be16(out + 109, 0);                                         /* sync address: never used (design 8.1 skips sync) */
    out[111] = seq;
    out[112] = options;
    lj_be16(out + 113, universe);
    lj_be16(out + 115, 0x7000 | (LJ_SACN_DATA_LEN - 115));        /* DMP */
    out[117] = 0x02; out[118] = 0xA1;
    lj_be16(out + 119, 0); lj_be16(out + 121, 1); lj_be16(out + 123, LJ_LINK_CH + 1);
    out[125] = 0;                                                  /* START code */
    memcpy(out + 126, slots, LJ_LINK_CH);
    return 0;
}

int lj_sacn_pack_discovery(uint8_t *out, size_t cap, const lj_sacn_cfg *cfg, const uint16_t *universes, int n, uint8_t page, uint8_t last_page)
{
    int total = 120 + 2 * n, i;
    if (!out || !cfg || n < 0 || n > 512 || (n && !universes) || cap < (size_t)total) return -1;
    lj_sacn_root(out, cfg, 0x00000008, total);
    lj_be16(out + 38, 0x7000 | (unsigned)(total - 38));
    lj_be32(out + 40, 0x00000002);
    lj_sacn_name_utf8(out + 44, cfg->name);
    memset(out + 108, 0, 4);                                       /* reserved */
    lj_be16(out + 112, 0x7000 | (unsigned)(total - 112));
    lj_be32(out + 114, 0x00000001);
    out[118] = page; out[119] = last_page;
    for (i = 0; i < n; i++) lj_be16(out + 120 + 2 * i, universes[i]);
    return total;
}

/* ---- config ---------------------------------------------------------------------- */

const char *lj_sacn_cfg_validate(lj_sacn_cfg *cfg)
{
    const char *why = NULL;
    int i, j;
    if (!cfg) return "no config";
    if (cfg->priority > LJ_SACN_PRIORITY_MAX) cfg->priority = LJ_SACN_PRIORITY_MAX;   /* 6.2.3 */
    if (cfg->ttl == 0) cfg->ttl = LJ_SACN_TTL_DEFAULT;
    cfg->name[LJ_SACN_NAME_CAP - 1] = 0;
    for (i = 0; i < LJ_LINKS; i++) {
        if (cfg->nunicast[i] < 0) cfg->nunicast[i] = 0;
        if (cfg->nunicast[i] > LJ_SACN_UNICAST_MAX) cfg->nunicast[i] = LJ_SACN_UNICAST_MAX;
        if (cfg->universe_of_link[i] > LJ_SACN_UNIVERSE_MAX) {                        /* 64214 and 65280+ included; the link is left unmapped (LORD_ENGINE-15) */
            if (!why) why = "universe not 1..63999";
            cfg->universe_of_link[i] = 0;
        }
        for (j = 0; j < i; j++)
            if (cfg->universe_of_link[i] && cfg->universe_of_link[i] == cfg->universe_of_link[j]) {
                if (!why) why = "duplicate universe";
                cfg->universe_of_link[i] = 0;
            }
    }
    return why;
}

void lj_sacn_cid_new(uint8_t cid[16])
{
    int ok = 0;
#ifdef _WIN32
    unsigned r[4];
    int i;
    for (i = 0, ok = 1; i < 4; i++) ok &= rand_s(&r[i]) == 0;
    if (ok) memcpy(cid, r, 16);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) { ok = fread(cid, 1, 16, f) == 16; fclose(f); }
#endif
    if (!ok) {                                                     /* no entropy source: clock bits, still unique enough for one host */
        uint64_t t = lj_clock_us() * 6364136223846793005ull + 1442695040888963407ull, u = t * 6364136223846793005ull + (uint64_t)(size_t)cid;
        memcpy(cid, &t, 8); memcpy(cid + 8, &u, 8);
    }
    cid[6] = (uint8_t)((cid[6] & 0x0F) | 0x40);                    /* RFC 4122 version 4 */
    cid[8] = (uint8_t)((cid[8] & 0x3F) | 0x80);                    /* variant 10 */
}

void lj_sacn_cfg_default(lj_sacn_cfg *cfg, const char *host)
{
    int i;
    memset(cfg, 0, sizeof *cfg);
    snprintf(cfg->name, sizeof cfg->name, "LJ4EVR %s", host ? host : "host");
    cfg->priority = LJ_SACN_PRIORITY_DEFAULT;
    cfg->ttl = LJ_SACN_TTL_DEFAULT;
    for (i = 0; i < LJ_LINKS; i++) cfg->universe_of_link[i] = (uint16_t)(i + 1);
    lj_sacn_cid_new(cfg->cid);
}

static int lj_ip_parse(const char *s, uint32_t *out)                /* "a.b.c.d" -> host order; 0 / -1 */
{
    unsigned a, b, c, d; char tail;
    if (!s || sscanf(s, "%3u.%3u.%3u.%3u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return -1;
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

static void lj_ip_fmt(char *dst, size_t cap, uint32_t ip) { snprintf(dst, cap, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }

static int lj_hex_nibble(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

int lj_sacn_cfg_load(lj_ini *ini, lj_sacn_cfg *cfg)
{
    char host[LJ_SACN_NAME_CAP], key[16], list[LJ_SACN_UNICAST_MAX * 16];
    const char *s;
    int i, have_cid = 0;
    lj_net_hostname(host, sizeof host);
    lj_sacn_cfg_default(cfg, host);
    if (!ini) return -1;
    lj_ip_parse(lj_ini_gets(ini, "SACN", "Nic", ""), &cfg->nic_ipv4);
    lj_str_copy(cfg->name, sizeof cfg->name, lj_ini_gets(ini, "SACN", "Name", cfg->name));
    s = lj_ini_gets(ini, "SACN", "CID", "");
    if (strlen(s) == 32) {
        uint8_t cid[16];
        for (i = 0, have_cid = 1; i < 16 && have_cid; i++) {
            int hi = lj_hex_nibble(s[2 * i]), lo = lj_hex_nibble(s[2 * i + 1]);
            if (hi < 0 || lo < 0) have_cid = 0; else cid[i] = (uint8_t)(hi << 4 | lo);
        }
        if (have_cid) memcpy(cfg->cid, cid, 16);
    }
    i = lj_ini_geti(ini, "SACN", "Priority", LJ_SACN_PRIORITY_DEFAULT);
    cfg->priority = (uint8_t)(i < 0 ? 0 : i > 255 ? 255 : i);             /* validate clamps to 200 */
    i = lj_ini_geti(ini, "SACN", "TTL", LJ_SACN_TTL_DEFAULT);
    cfg->ttl = (uint8_t)(i < 1 ? 1 : i > 255 ? 255 : i);
    cfg->preview = (uint8_t)(lj_ini_geti(ini, "SACN", "Preview", 0) != 0);
    cfg->unlock_rate = (uint8_t)(lj_ini_geti(ini, "SACN", "UnlockRate", 0) != 0);
    for (i = 0; i < LJ_LINKS; i++) {
        char *p, *q;
        int v;
        snprintf(key, sizeof key, "U%d", i + 1);
        v = lj_ini_geti(ini, "SACN", key, i + 1);
        cfg->universe_of_link[i] = (uint16_t)(v < 0 || v > 0xFFFF ? 0xFFFF : v);        /* out of range -> validate unmaps it */
        snprintf(key, sizeof key, "Unicast%d", i + 1);
        lj_str_copy(list, sizeof list, lj_ini_gets(ini, "SACN", key, ""));
        cfg->nunicast[i] = 0;
        for (p = list; *p && cfg->nunicast[i] < LJ_SACN_UNICAST_MAX; p = q) {
            q = strchr(p, ',');
            if (q) *q++ = 0; else q = p + strlen(p);
            if (lj_ip_parse(p, &cfg->unicast[i][cfg->nunicast[i]]) == 0) cfg->nunicast[i]++;
        }
    }
    s = lj_sacn_cfg_validate(cfg);
    if (s) lj_log("sacn: Hardware.Ini [SACN]: %s (link left unmapped)", s);
    return have_cid ? 0 : -1;
}

void lj_sacn_cfg_save(lj_ini *ini, const lj_sacn_cfg *cfg)
{
    char buf[LJ_SACN_UNICAST_MAX * 16], key[16], ip[16];
    int i, k;
    lj_ip_fmt(buf, sizeof buf, cfg->nic_ipv4);
    lj_ini_sets(ini, "SACN", "Nic", buf);
    lj_ini_sets(ini, "SACN", "Name", cfg->name);
    for (i = 0; i < 16; i++) snprintf(buf + 2 * i, sizeof buf - 2 * (size_t)i, "%02x", cfg->cid[i]);
    lj_ini_sets(ini, "SACN", "CID", buf);
    lj_ini_seti(ini, "SACN", "Priority", cfg->priority);
    lj_ini_seti(ini, "SACN", "TTL", cfg->ttl);
    lj_ini_seti(ini, "SACN", "Preview", cfg->preview);
    lj_ini_seti(ini, "SACN", "UnlockRate", cfg->unlock_rate);
    for (i = 0; i < LJ_LINKS; i++) {
        snprintf(key, sizeof key, "U%d", i + 1);
        lj_ini_seti(ini, "SACN", key, cfg->universe_of_link[i]);
        snprintf(key, sizeof key, "Unicast%d", i + 1);
        buf[0] = 0;
        for (k = 0; k < cfg->nunicast[i] && k < LJ_SACN_UNICAST_MAX; k++) {
            lj_ip_fmt(ip, sizeof ip, cfg->unicast[i][k]);
            if (k) strcat(buf, ",");
            strcat(buf, ip);                                       /* 8 x 16 fits buf */
        }
        lj_ini_sets(ini, "SACN", key, buf);
    }
}

/* ---- the wire ------------------------------------------------------------------------ */

static void lj_sacn_sendto(const uint8_t *pkt, int len, uint32_t ip_host)
{
    struct sockaddr_in to;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons(LJ_SACN_PORT);
    to.sin_addr.s_addr = htonl(ip_host);
    if (sendto(lj_s.sock, (const char *)pkt, len, 0, (const struct sockaddr *)&to, sizeof to) < 0) {
        int err = lj_sock_err();
        lj_s.st.send_errors++;
        if (err != lj_s.st.last_err) { char d[16]; lj_ip_fmt(d, sizeof d, ip_host); lj_log("sacn: sendto %s failed: %d", d, err); }
        lj_s.st.last_err = err;
    }
}

static uint32_t lj_sacn_mcast(uint16_t universe) { return 0xEFFF0000u | universe; }   /* 239.255.<hi>.<lo>, 9.3.1 */

static void lj_sacn_emit(int link, uint8_t options)                 /* one data packet for link's universe, to every destination; 6.2.5 seq +1 */
{
    uint8_t pkt[LJ_SACN_DATA_LEN];
    lj_sacn_uni *u = &lj_s.uni[link];
    uint16_t universe = lj_s.cfg.universe_of_link[link];
    int k;
    if (lj_s.cfg.preview) options |= LJ_SACN_OPT_PREVIEW;
    lj_sacn_pack_data(pkt, &lj_s.cfg, universe, u->seq, options, u->slots);
    if (lj_s.cfg.nunicast[link] > 0) for (k = 0; k < lj_s.cfg.nunicast[link]; k++) lj_sacn_sendto(pkt, LJ_SACN_DATA_LEN, lj_s.cfg.unicast[link][k]);
    else lj_sacn_sendto(pkt, LJ_SACN_DATA_LEN, lj_sacn_mcast(universe));
    u->seq++;
    lj_s.st.packets[link]++;
    u->in_window++;
    if (options & LJ_SACN_OPT_TERMINATED) lj_s.st.terminated++;
}

static void lj_sacn_terminate(int link)                              /* 6.2.6: three Stream_Terminated packets */
{
    int k;
    for (k = 0; k < LJ_SACN_REPEATS; k++) lj_sacn_emit(link, LJ_SACN_OPT_TERMINATED);
}

static int lj_sacn_link_of(const lj_sacn_cfg *cfg, uint16_t universe)
{
    int i;
    if (universe) for (i = 0; i < LJ_LINKS; i++) if (cfg->universe_of_link[i] == universe) return i;
    return -1;
}

int lj_sacn_open(const lj_sacn_cfg *in)
{
    lj_sacn_cfg cfg;
    lj_sacn_uni old[LJ_LINKS];
    lj_sacn_cfg oldcfg;
    const char *why;
    struct in_addr nic;
    int i, was_open = lj_s.open, ttl;
    if (!in) return -1;
    cfg = *in;
    why = lj_sacn_cfg_validate(&cfg);
    if (why) lj_log("sacn: config: %s (link left unmapped)", why);
    for (i = 0; i < LJ_LINKS; i++) if (cfg.proto[i] == LJ_OUT_ARTNET) cfg.universe_of_link[i] = 0;   /* W5 artnet: an Art-Net link is unmapped here (close-old terminates its universe) */
    if (lj_net_startup() != 0) return -1;

    /* close-old (ME-23): terminate every universe the new map drops, then drop the socket */
    memcpy(old, lj_s.uni, sizeof old);
    oldcfg = lj_s.cfg;
    if (was_open) {
        for (i = 0; i < LJ_LINKS; i++)
            if (oldcfg.universe_of_link[i] && lj_sacn_link_of(&cfg, oldcfg.universe_of_link[i]) < 0) lj_sacn_terminate(i);
        lj_sock_close(lj_s.sock);
        lj_s.open = 0;
    }

    /* open-new: one UDP socket, NIC by in_addr, TTL, no bind (08 s6.2) */
    lj_s.sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (lj_s.sock == LJ_SOCK_BAD) { lj_log("sacn: socket failed: %d", lj_sock_err()); memset(&lj_s, 0, sizeof lj_s); lj_s.sock = LJ_SOCK_BAD; return -1; }
    nic.s_addr = htonl(cfg.nic_ipv4);                              /* 0.0.0.0 = the OS default interface */
    if (cfg.nic_ipv4 && setsockopt(lj_s.sock, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&nic, sizeof nic) < 0) lj_log("sacn: IP_MULTICAST_IF failed: %d", lj_sock_err());
    ttl = cfg.ttl;
#ifdef _WIN32
    if (setsockopt(lj_s.sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof ttl) < 0) lj_log("sacn: IP_MULTICAST_TTL failed: %d", lj_sock_err());
#else
    { unsigned char t8 = (unsigned char)ttl; if (setsockopt(lj_s.sock, IPPROTO_IP, IP_MULTICAST_TTL, &t8, sizeof t8) < 0) lj_log("sacn: IP_MULTICAST_TTL failed: %d", lj_sock_err()); }
#endif
    lj_s.cfg = cfg;
    memset(lj_s.uni, 0, sizeof lj_s.uni);
    for (i = 0; i < LJ_LINKS; i++) {
        int o = was_open ? lj_sacn_link_of(&oldcfg, cfg.universe_of_link[i]) : -1;
        if (o >= 0) lj_s.uni[i] = old[o];                          /* a universe that stays mapped keeps its slots and sequence */
        else if (cfg.universe_of_link[i]) lj_s.uni[i].repeats_left = LJ_SACN_REPEATS;   /* a new universe announces itself on the next pumps */
        lj_s.uni[i].in_window = 0;
    }
    memset(lj_s.st.packets, 0, sizeof lj_s.st.packets);
    memset(lj_s.st.pps, 0, sizeof lj_s.st.pps);
    lj_s.st.nic_ipv4 = cfg.nic_ipv4;
    lj_s.next_disc_us = 0;                                         /* discovery on the first pump */
    lj_s.open = 1;
    return 0;
}

void lj_sacn_set(int universe, const uint8_t slots[LJ_LINK_CH])
{
    int link;
    if (!lj_s.open || !slots || universe <= 0 || universe > LJ_SACN_UNIVERSE_MAX) return;
    link = lj_sacn_link_of(&lj_s.cfg, (uint16_t)universe);
    if (link < 0) return;
    if (memcmp(lj_s.uni[link].slots, slots, LJ_LINK_CH) != 0) {
        memcpy(lj_s.uni[link].slots, slots, LJ_LINK_CH);
        lj_s.uni[link].repeats_left = LJ_SACN_REPEATS;              /* 6.6.2: the change plus two repeats */
    }
}

void lj_sacn_pump(uint64_t now_us)
{
    int i, n = 0;
    if (!lj_s.open) return;
    for (i = 0; i < LJ_LINKS; i++) {
        lj_sacn_uni *u = &lj_s.uni[i];
        if (!lj_s.cfg.universe_of_link[i]) continue;
        if (u->repeats_left == 0 && now_us - u->last_sent_us < LJ_SACN_KEEPALIVE_US) continue;
        if (!lj_s.cfg.unlock_rate && now_us - u->last_sent_us < LJ_SACN_MIN_INTERVAL_US) continue;   /* ME-24: E1.11 cap, latest frame wins */
        lj_sacn_emit(i, 0);
        u->last_sent_us = now_us;
        if (u->repeats_left) u->repeats_left--;
    }
    if (now_us >= lj_s.next_disc_us) {                             /* 4.3: every 10 s, mapped universes sorted ascending (8.5) */
        uint16_t list[LJ_LINKS], t;
        uint8_t pkt[LJ_SACN_DISC_CAP];
        int j, len;
        for (i = 0; i < LJ_LINKS; i++) if (lj_s.cfg.universe_of_link[i]) list[n++] = lj_s.cfg.universe_of_link[i];
        for (i = 1; i < n; i++) for (j = i; j > 0 && list[j - 1] > list[j]; j--) { t = list[j]; list[j] = list[j - 1]; list[j - 1] = t; }
        len = lj_sacn_pack_discovery(pkt, sizeof pkt, &lj_s.cfg, list, n, 0, 0);
        if (len > 0) { lj_sacn_sendto(pkt, len, lj_sacn_mcast(LJ_SACN_DISCOVERY_UNIVERSE)); lj_s.st.discovery++; }
        lj_s.next_disc_us = now_us + LJ_SACN_DISCOVERY_US;
    }
    if (now_us - lj_s.window_us >= 1000000u) {                     /* packets in the last whole second, for the Hardware Setup status line */
        for (i = 0; i < LJ_LINKS; i++) { lj_s.st.pps[i] = lj_s.uni[i].in_window; lj_s.uni[i].in_window = 0; }
        lj_s.window_us = now_us;
    }
}

void lj_sacn_close(void)
{
    int i;
    if (!lj_s.open) return;
    for (i = 0; i < LJ_LINKS; i++) if (lj_s.cfg.universe_of_link[i]) lj_sacn_terminate(i);
    lj_sock_close(lj_s.sock);
    lj_s.sock = LJ_SOCK_BAD;
    lj_s.open = 0;
}

void lj_sacn_stats_get(lj_sacn_stats *out)
{
    int i;
    *out = lj_s.st;
    for (i = 0; i < LJ_LINKS; i++) out->universe[i] = lj_s.open ? lj_s.cfg.universe_of_link[i] : 0;
}
