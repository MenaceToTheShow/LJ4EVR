/* lj_dmxin.c - DMX-in input seam, .vtr tables, DMX-in functions (io/lj_dmxin.h; docs/w5-contracts.md seam 2; track
 * dmxin). UI thread only, except lj_dmxin_post (any thread, under lj_lock). Every packet and file is hostile input
 * (W5-X-HOSTILE): each length, count and index is checked before it is used. Tests: tests/io/test_dmxin.c. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as core/lj_textrec.c) */
#include "io/lj_dmxin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "core/lj_textrec.h"
#include "io/lj_artnet.h"
#include "io/lj_fn.h"
#include "io/lj_net.h"
#include "io/lj_sacn.h"
#include "io/lj_socket.h"

#define LJ_DMXIN_FN_WORD(id, word) word,
const char *const lj_dmxin_fn_words[LJ_DIF_COUNT] = { LJ_DMXIN_FN_LIST(LJ_DMXIN_FN_WORD) };
#undef LJ_DMXIN_FN_WORD

typedef struct { int kind; size_t n; uint32_t ip; uint64_t at; uint8_t pkt[LJ_DMXIN_PKT_MAX]; } lj_di_parked;

static struct {
    lj_dmxin_cfg cfg;
    uint8_t own_cid[16], have_cid;
    lj_socket sock;
    lj_dmxin_frame f;
    uint8_t table[LJ_LINK_CH][256], has_table[LJ_LINK_CH];
    int16_t emu[LJ_DMXIN_EMULATE];                  /* Fingers DMX-in channels 1..48, -1 = released */
    uint8_t prev_out[LJ_LINK_CH], out_valid;
    uint64_t hz_t0; uint32_t hz_n;
    lj_di_parked q[LJ_DMXIN_QUEUE]; int qn;          /* lj_dmxin_post -> lj_dmxin_poll, under lj_lock */
    uint32_t qdropped;
    uint32_t mask;                                   /* ME-177: the Nic's netmask; a datagram from off that subnet is dropped (0 = no Nic set) */
} lj_di = { .sock = LJ_SOCKET_BAD, .emu = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 } };

static unsigned be16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* ---- the sACN data-packet reader (E1.31-2018 5.1-5.5, 6.2, 7.2-7.6; note 08 s5) ---- */
int lj_sacn_rx_parse(const uint8_t *pkt, size_t n, lj_sacn_rx *out)
{
    static const uint8_t pid[12] = { 'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0 };
    unsigned count, total;
    if (!pkt || !out || n < 126) return -1;                                       /* the header through the START code */
    if (be16(pkt) != 0x0010 || be16(pkt + 2) != 0 || memcmp(pkt + 4, pid, 12) != 0) return -1;   /* 5.1-5.3 */
    if (be32(pkt + 18) != 0x00000004) return -1;                                  /* 5.5: VECTOR_ROOT_E131_DATA (0x8 extended: not data, ignored) */
    if (be32(pkt + 40) != 0x00000002) return -1;                                  /* 6.2.1 VECTOR_E131_DATA_PACKET */
    if (pkt[117] != 0x02 || pkt[118] != 0xA1 || be16(pkt + 119) != 0 || be16(pkt + 121) != 1) return -1;   /* 7.2-7.5 */
    count = be16(pkt + 123);
    if (count < 1 || count > LJ_LINK_CH + 1) return -1;                            /* 7.6: 1..513 including the START code */
    total = 125 + count;
    if (n < total) return -1;                                                     /* the slots claimed are present */
    if ((be16(pkt + 16) & 0x0FFF) != total - 16 || (be16(pkt + 38) & 0x0FFF) != total - 38 || (be16(pkt + 115) & 0x0FFF) != total - 115) return -1;   /* the three PDU lengths agree with the count */
    out->universe = (uint16_t)be16(pkt + 113);
    if (out->universe < 1 || out->universe > LJ_SACN_UNIVERSE_MAX) return -1;     /* 6.2.7 */
    if (pkt[108] > LJ_SACN_PRIORITY_MAX) return -1;                               /* 6.2.3: 0..200 */
    if (pkt[125] != 0x00) return -1;                                              /* 0xDD per-slot priority and every other START code: ignored */
    memcpy(out->cid, pkt + 22, 16);
    memcpy(out->name, pkt + 44, 63); out->name[63] = 0;                           /* 6.2.2: NUL not guaranteed (W5-X-TEXT) */
    out->priority = pkt[108]; out->seq = pkt[111]; out->options = pkt[112]; out->start = pkt[125];
    out->n = (uint16_t)(count - 1);
    out->slots = pkt + 126;
    return 0;
}

/* ---- merge (note 08 s5; the rule declared in docs/sacn.md) ---- */
static void lj_di_merge(void)
{
    lj_dmxin_frame *f = &lj_di.f;
    int s, c, top = -1, live_emu = 0;
    uint16_t chans = 0;
    for (s = 0; s < f->nsources; s++) if (f->src[s].priority > top) top = f->src[s].priority;
    for (c = 0; c < LJ_DMXIN_EMULATE; c++) live_emu |= lj_di.emu[c] >= 0;
    f->live = (uint8_t)(f->nsources > 0 || live_emu);
    if (f->live) {
        memset(f->raw, 0, sizeof f->raw);
        for (s = 0; s < f->nsources; s++) {
            const lj_dmxin_source *src = &f->src[s];
            if (src->priority != top) continue;
            if (src->n > chans) chans = src->n;
            for (c = 0; c < src->n; c++) if (src->slots[c] > f->raw[c]) f->raw[c] = src->slots[c];   /* equal priority: HTP per slot */
        }
        for (c = 0; c < LJ_DMXIN_EMULATE; c++) if (lj_di.emu[c] > f->raw[c]) f->raw[c] = (uint8_t)lj_di.emu[c];   /* Fingers emulation: HTP on top (05 s3.3) */
        if (live_emu && chans < LJ_DMXIN_EMULATE) chans = LJ_DMXIN_EMULATE;
        f->channels = chans;
    } else {
        if (lj_di.cfg.loss == LJ_DMXIN_LOSS_ZERO) memset(f->raw, 0, sizeof f->raw);   /* PROPOSAL rig W5-DMXIN-3: hold (default) or zeros */
        f->channels = 0;
    }
    for (c = 0; c < LJ_LINK_CH; c++) f->out[c] = lj_di.has_table[c] ? lj_di.table[c][f->raw[c]] : f->raw[c];
}

static void lj_di_drop(int s)
{
    lj_dmxin_frame *f = &lj_di.f;
    if (s < 0 || s >= f->nsources) return;
    memmove(&f->src[s], &f->src[s + 1], (size_t)(f->nsources - s - 1) * sizeof f->src[0]);
    f->nsources--;
}

static int lj_di_discard(void) { lj_di.f.discarded++; return -1; }

int lj_dmxin_rx(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us)
{
    lj_dmxin_frame *f = &lj_di.f;
    uint8_t cid[16], slots[LJ_LINK_CH], prio, seq = 0, term = 0;
    const uint8_t *data;
    char name[64];
    int s, nslots;
    memset(cid, 0, sizeof cid); name[0] = 0;
    if (kind == LJ_DMXIN_SACN) {
        lj_sacn_rx rx;
        if (lj_sacn_rx_parse(pkt, n, &rx) != 0 || rx.universe != lj_di.cfg.universe) return lj_di_discard();
        if (rx.options & 0x80) return lj_di_discard();                                     /* 6.2.6: Preview_Data, not for live output */
        if (lj_di.have_cid && memcmp(rx.cid, lj_di.own_cid, 16) == 0) return lj_di_discard();   /* our own output (W5-DMXIN-1) */
        memcpy(cid, rx.cid, 16); prio = rx.priority; seq = rx.seq; term = (uint8_t)((rx.options & 0x40) != 0);
        nslots = rx.n; data = rx.slots;
        if (lj_utf8_to_cp1252(name, sizeof name, rx.name) != 0) name[sizeof name - 1] = 0;
    } else if (kind == LJ_DMXIN_ARTNET) {
        uint16_t pa = 0;
        if (lj_artnet_parse_dmx(pkt, n, &pa, &seq, slots, &nslots) != 0 || pa != lj_di.cfg.universe || nslots < 0 || nslots > LJ_LINK_CH) return lj_di_discard();
        prio = LJ_SACN_PRIORITY_DEFAULT;                                                    /* PROPOSAL rig W5-DMXIN-2: Art-Net merges at 100 */
        data = slots;
        { char ip[16]; lj_net_ip_fmt(ip, sizeof ip, src_ip); if (snprintf(name, sizeof name, "Art-Net %s", ip) < 0) name[0] = 0; }   /* ME-194: the one formatter */
    } else return lj_di_discard();
    for (s = 0; s < f->nsources; s++)
        if (f->src[s].kind == kind && (kind == LJ_DMXIN_SACN ? memcmp(f->src[s].cid, cid, 16) == 0 : f->src[s].ip == src_ip)) break;
    if (s < f->nsources && kind == LJ_DMXIN_SACN) {
        int d = (int)(int8_t)(uint8_t)(seq - f->src[s].seq);                               /* 6.7.2: discard when B - A is in (-20, 0] */
        if (d <= 0 && d > -20) return lj_di_discard();
    }
    if (term) {                                                                            /* 6.2.6: one Stream_Terminated ends the source */
        if (s < f->nsources) { lj_di_drop(s); lj_di_merge(); }
        f->packets++;
        return 0;
    }
    if (s == f->nsources) {
        if (f->nsources == LJ_DMXIN_SOURCES) { f->exceeded++; return lj_di_discard(); }    /* "sources exceeded" (E1.31 6.2.3.5 documentation duty) */
        memset(&f->src[s], 0, sizeof f->src[s]);
        f->nsources++;
        f->src[s].kind = (uint8_t)kind; memcpy(f->src[s].cid, cid, 16); f->src[s].ip = src_ip;
    }
    f->src[s].priority = prio; f->src[s].seq = seq; f->src[s].last_us = now_us; f->src[s].ip = src_ip;
    f->src[s].n = (uint16_t)nslots;
    memcpy(f->src[s].slots, data, (size_t)nslots);
    memset(f->src[s].slots + nslots, 0, (size_t)(LJ_LINK_CH - nslots));                   /* fewer than 512 slots: the rest are 0 */
    lj_str_copy(f->src[s].name, sizeof f->src[s].name, name);
    f->packets++; lj_di.hz_n++;
    lj_di_merge();
    return 0;
}

void lj_dmxin_post(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us)
{
    lj_lock();
    if (!pkt || n > LJ_DMXIN_PKT_MAX || lj_di.qn >= LJ_DMXIN_QUEUE) lj_di.qdropped++;
    else { lj_di_parked *p = &lj_di.q[lj_di.qn++]; p->kind = kind; p->n = n; p->ip = src_ip; p->at = now_us; memcpy(p->pkt, pkt, n); }
    lj_unlock();
}

int lj_dmxin_poll(uint64_t now_us)
{
    static lj_di_parked q[LJ_DMXIN_QUEUE];
    static uint8_t buf[LJ_DMXIN_PKT_MAX + 1];
    lj_dmxin_frame *f = &lj_di.f;
    int i, k, qn, changed;
    lj_lock(); qn = lj_di.qn; memcpy(q, lj_di.q, (size_t)qn * sizeof q[0]); lj_di.qn = 0; lj_unlock();
    for (i = 0; i < qn; i++) {
        if (lj_di.mask && ((q[i].ip ^ lj_di.cfg.nic_ipv4) & lj_di.mask) != 0) { f->discarded++; continue; }   /* ME-177: off the Nic's subnet */
        lj_dmxin_rx(q[i].kind, q[i].pkt, q[i].n, q[i].ip, now_us);
    }
    for (k = 0; k < LJ_DMXIN_RX_BUDGET && lj_di.sock != LJ_SOCKET_BAD; k++) {             /* W5-X-THREAD: a flood cannot starve the wake */
        uint32_t ip = 0; uint16_t port = 0;
        int r = lj_socket_recvfrom(lj_di.sock, buf, sizeof buf, &ip, &port);
        if (r <= 0) break;
        if ((size_t)r == sizeof buf) { f->discarded++; continue; }                         /* truncated: longer than any valid packet */
        if (lj_di.mask && ((ip ^ lj_di.cfg.nic_ipv4) & lj_di.mask) != 0) { f->discarded++; continue; }   /* ME-177: the socket binds every interface (multicast); the source decides */
        lj_dmxin_rx(LJ_DMXIN_SACN, buf, (size_t)r, ip, now_us);
    }
    for (i = f->nsources - 1; i >= 0; i--) if (now_us - f->src[i].last_us > LJ_DMXIN_TIMEOUT_US) lj_di_drop(i);   /* 6.7.1; one merge below, so sources lost together hold their last frame */
    if (now_us - lj_di.hz_t0 >= 1000000u) { f->hz = (uint16_t)(lj_di.hz_n > 65535u ? 65535u : lj_di.hz_n); lj_di.hz_n = 0; lj_di.hz_t0 = now_us; }
    lj_di_merge();
    changed = !lj_di.out_valid || memcmp(lj_di.prev_out, f->out, sizeof f->out) != 0;
    memcpy(lj_di.prev_out, f->out, sizeof f->out); lj_di.out_valid = 1;
    return changed;
}

const lj_dmxin_frame *lj_dmxin_frame_get(void) { return &lj_di.f; }

void lj_dmxin_emulate(int ch, int level, uint64_t now_us)
{
    (void)now_us;
    if (ch < 1 || ch > LJ_DMXIN_EMULATE) return;
    lj_di.emu[ch - 1] = (int16_t)(level < 0 ? -1 : level > 255 ? 255 : level);
    lj_di_merge();
}

void lj_dmxin_set_table(int ch, const uint8_t *map256)
{
    if (ch < 1 || ch > LJ_LINK_CH) return;
    lj_di.has_table[ch - 1] = map256 != NULL;
    if (map256) memcpy(lj_di.table[ch - 1], map256, 256);
    lj_di_merge();
}

int lj_dmxin_open(const lj_dmxin_cfg *cfg, const uint8_t own_cid[16])
{
    char a[16];
    lj_dmxin_close();
    if (!cfg) return 0;
    lj_di.cfg = *cfg;
    lj_di.have_cid = own_cid != NULL;
    if (own_cid) memcpy(lj_di.own_cid, own_cid, 16);
    memset(&lj_di.f, 0, sizeof lj_di.f); lj_di.out_valid = 0;
    lj_dmxin_functions_reset();                                                           /* ME-167: the first real frame primes the edge state again */
    lj_di.mask = 0;
    if (cfg->enabled && cfg->nic_ipv4) {                                                  /* ME-177: the Nic's subnet; unknown mask = its /24 (PROPOSAL) */
        lj_nic nic[16];
        int k, nn = lj_net_list_ipv4(nic, (int)(sizeof nic / sizeof nic[0]));
        lj_di.mask = 0xFFFFFF00u;
        for (k = 0; k < nn; k++) if (nic[k].ipv4 == cfg->nic_ipv4 && nic[k].mask) lj_di.mask = nic[k].mask;
    }
    if (!cfg->enabled || cfg->source != LJ_DMXIN_SACN) return 0;                          /* W5-X-OFF; Art-Net input rides the engine's 6454 socket */
    lj_di.sock = lj_socket_udp(0, LJ_SACN_PORT, LJ_SOCK_REUSE);
    if (lj_di.sock == LJ_SOCKET_BAD) { lj_log("dmx in: cannot bind 0.0.0.0:%u", (unsigned)LJ_SACN_PORT); return -1; }
    lj_net_ip_fmt(a, sizeof a, cfg->nic_ipv4);
    if (lj_socket_join(lj_di.sock, 0xEFFF0000u | cfg->universe, cfg->nic_ipv4) != 0) lj_log("dmx in: multicast join of universe %u failed; unicast only", (unsigned)cfg->universe);
    lj_log("dmx in: sACN universe %u on 0.0.0.0:%u, joined on %s", (unsigned)cfg->universe, (unsigned)LJ_SACN_PORT, cfg->nic_ipv4 ? a : "the default interface");
    return 0;
}

void lj_dmxin_close(void)
{
    if (lj_di.sock != LJ_SOCKET_BAD) { lj_socket_close(lj_di.sock); lj_di.sock = LJ_SOCKET_BAD; lj_log("dmx in: sACN input closed"); }
}

/* ---- configuration ---- */
void lj_dmxin_cfg_default(lj_dmxin_cfg *cfg) { if (!cfg) return; memset(cfg, 0, sizeof *cfg); cfg->source = LJ_DMXIN_SACN; cfg->universe = 1; cfg->loss = LJ_DMXIN_LOSS_HOLD; }

void lj_dmxin_cfg_load(lj_ini *ini, lj_dmxin_cfg *cfg)
{
    int v;
    lj_dmxin_cfg_default(cfg);
    if (!cfg || !ini) return;
    cfg->enabled = (uint8_t)(lj_ini_geti(ini, "DMXIN", "Enabled", 0) != 0);
    cfg->source = (uint8_t)(lj_ini_geti(ini, "DMXIN", "Source", 0) == 1 ? LJ_DMXIN_ARTNET : LJ_DMXIN_SACN);
    v = lj_ini_geti(ini, "DMXIN", "Universe", 1);
    cfg->universe = (uint16_t)(v >= 1 && v <= LJ_SACN_UNIVERSE_MAX ? v : 1);
    { const char *nic = lj_ini_gets(ini, "DMXIN", "Nic", "");
      if (lj_net_ip_parse(nic, &cfg->nic_ipv4) != 0) { cfg->nic_ipv4 = 0; if (nic[0]) lj_log("dmxin: Hardware.Ini [DMXIN] Nic is not a dotted IPv4 address: every interface listens"); } }   /* ME-197 */
    cfg->loss = (uint8_t)(lj_ini_geti(ini, "DMXIN", "Loss", 0) == 1 ? LJ_DMXIN_LOSS_ZERO : LJ_DMXIN_LOSS_HOLD);
    cfg->functions_off = (uint8_t)(lj_ini_geti(ini, "DMXIN", "FunctionsOff", 0) != 0);
}

void lj_dmxin_cfg_save(lj_ini *ini, const lj_dmxin_cfg *cfg)
{
    char a[16];
    if (!ini || !cfg) return;
    lj_ini_seti(ini, "DMXIN", "Enabled", cfg->enabled);
    lj_ini_seti(ini, "DMXIN", "Source", cfg->source);
    lj_ini_seti(ini, "DMXIN", "Universe", cfg->universe);
    lj_net_ip_fmt(a, sizeof a, cfg->nic_ipv4);
    lj_ini_sets(ini, "DMXIN", "Nic", cfg->nic_ipv4 ? a : "");
    lj_ini_seti(ini, "DMXIN", "Loss", cfg->loss);
    lj_ini_seti(ini, "DMXIN", "FunctionsOff", cfg->functions_off);
}

const char *lj_dmxin_cfg_validate(const lj_dmxin_cfg *cfg, const uint16_t out_universes[LJ_LINKS])
{
    int i;
    if (!cfg) return "no configuration";
    if (cfg->source == LJ_DMXIN_ARTNET) return cfg->universe >= 1 && cfg->universe <= 32767 ? NULL : "The Art-Net input Port-Address must be 1..32767";
    if (cfg->universe < 1 || cfg->universe > LJ_SACN_UNIVERSE_MAX) return "The DMX-in universe must be 1..63999";
    if (out_universes) for (i = 0; i < LJ_LINKS; i++) if (out_universes[i] == cfg->universe) return "The DMX-in universe is one of the output universes";
    return NULL;
}

/* ---- .vtr value tables (05 s3.3) ---- */
static const char lj_vtr_header[] = "// LightJockey DMX value translation file";   /* line 1 of every LJ2 table (a format fact, D13) */

void lj_vtr_identity(lj_vtr *t) { int i; if (!t) return; memset(t, 0, sizeof *t); for (i = 0; i < 256; i++) t->map[i] = (uint8_t)i; }

void lj_vtr_linear(lj_vtr *t, int in_lo, int in_hi, int out_lo, int out_hi)
{
    int i, span, d;
    if (!t) return;
    in_lo = in_lo < 0 ? 0 : in_lo > 255 ? 255 : in_lo; in_hi = in_hi < 0 ? 0 : in_hi > 255 ? 255 : in_hi;
    out_lo = out_lo < 0 ? 0 : out_lo > 255 ? 255 : out_lo; out_hi = out_hi < 0 ? 0 : out_hi > 255 ? 255 : out_hi;
    if (in_lo > in_hi) { i = in_lo; in_lo = in_hi; in_hi = i; i = out_lo; out_lo = out_hi; out_hi = i; }
    span = in_hi - in_lo; d = out_hi - out_lo;
    for (i = in_lo; i <= in_hi; i++) {
        int num = (i - in_lo) * d;
        t->map[i] = (uint8_t)(span ? out_lo + (num >= 0 ? (num + span / 2) / span : -((-num + span / 2) / span)) : out_lo);   /* integer rounding, half away from zero */
    }
}

static int lj_vtr_err(char *err, size_t cap, int line, const char *why)
{
    if (err && cap) { if (line > 0) { if (snprintf(err, cap, "line %d: %s", line, why) < 0) err[0] = 0; } else lj_str_copy(err, cap, why); }
    return -1;
}

static int lj_vtr_num(const char *s, size_t n, long *v)   /* decimal digits only, 0..255, ERANGE checked */
{
    char tmp[8], *end;
    size_t i;
    if (n == 0 || n >= sizeof tmp) return -1;
    for (i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return -1;
    memcpy(tmp, s, n); tmp[n] = 0;
    errno = 0;
    *v = strtol(tmp, &end, 10);
    return errno == ERANGE || *end || *v < 0 || *v > 255 ? -1 : 0;
}

static int lj_vtr_guid_ok(const char *s)   /* {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} */
{
    static const char pat[] = "{HHHHHHHH-HHHH-HHHH-HHHH-HHHHHHHHHHHH}";
    size_t i;
    if (strlen(s) != sizeof pat - 1) return 0;
    for (i = 0; i < sizeof pat - 1; i++) {
        char c = s[i];
        if (pat[i] == 'H' ? !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) : c != pat[i]) return 0;
    }
    return 1;
}

int lj_vtr_parse(const char *text, size_t n, lj_vtr *out, char *err, size_t cap)
{
    uint8_t seen[256];
    size_t pos = 0;
    int line = 0, have_id = 0, have_type = 0, have_name = 0;
    if (err && cap) err[0] = 0;
    if (!out) return lj_vtr_err(err, cap, 0, "no table");
    lj_vtr_identity(out);                                                                  /* missing entries = 1:1 */
    if (!text) return lj_vtr_err(err, cap, 0, "empty file");
    if (n > LJ_VTR_FILE_MAX) return lj_vtr_err(err, cap, 0, "file too large");
    memset(seen, 0, sizeof seen);
    while (pos < n) {
        size_t len = 0, eq;
        const char *l = text + pos;
        while (pos + len < n && l[len] != '\n') len++;
        pos += len + (pos + len < n);
        line++;
        if (len && l[len - 1] == '\r') len--;
        if (len > LJ_VTR_LINE_MAX) return lj_vtr_err(err, cap, line, "line too long");
        if (memchr(l, 0, len)) return lj_vtr_err(err, cap, line, "NUL byte");
        if (line == 1) {
            if (len != sizeof lj_vtr_header - 1 || memcmp(l, lj_vtr_header, len) != 0) return lj_vtr_err(err, cap, line, "not a LightJockey DMX value translation file");
            continue;
        }
        if (len == 0) continue;
        for (eq = 0; eq < len && l[eq] != '='; eq++) { }
        if (eq == len) return lj_vtr_err(err, cap, line, "no '='");
        if (l[0] >= '0' && l[0] <= '9') {
            long a, b;
            if (lj_vtr_num(l, eq, &a) != 0 || lj_vtr_num(l + eq + 1, len - eq - 1, &b) != 0) return lj_vtr_err(err, cap, line, "entry not n=m with n and m 0..255");
            if (seen[a]) return lj_vtr_err(err, cap, line, "duplicate entry");             /* PROPOSAL rig W5-DMXIN-6: a duplicate n refuses the file */
            seen[a] = 1; out->map[a] = (uint8_t)b;
        } else {
            char *dst = NULL; size_t dcap = 0, vlen = len - eq - 1;
            if (eq == 2 && memcmp(l, "ID", 2) == 0) { dst = out->id; dcap = sizeof out->id; have_id = 1; }
            else if (eq == 4 && memcmp(l, "TYPE", 4) == 0) { dst = out->type; dcap = sizeof out->type; have_type = 1; }
            else if (eq == 4 && memcmp(l, "NAME", 4) == 0) { dst = out->name; dcap = sizeof out->name; have_name = 1; }
            else if (eq == 7 && memcmp(l, "PURPOSE", 7) == 0) { dst = out->purpose; dcap = sizeof out->purpose; }
            else if ((l[0] >= 'A' && l[0] <= 'Z') || (l[0] >= 'a' && l[0] <= 'z')) continue;   /* an unknown key: skipped (a later LJ2 key) */
            else return lj_vtr_err(err, cap, line, "not a key or an n=m entry");
            if (vlen >= dcap) return lj_vtr_err(err, cap, line, "value too long");
            memcpy(dst, l + eq + 1, vlen); dst[vlen] = 0;
        }
    }
    if (line == 0) return lj_vtr_err(err, cap, 0, "empty file");
    if (!have_id || !lj_vtr_guid_ok(out->id)) return lj_vtr_err(err, cap, 0, "ID= missing or not a GUID");
    if (!have_type || !have_name || !out->name[0]) return lj_vtr_err(err, cap, 0, "TYPE= or NAME= missing");
    return 0;
}

int lj_vtr_load(const char *path, lj_vtr *out, char *err, size_t cap)
{
    static char buf[LJ_VTR_FILE_MAX + 1];
    FILE *fp;
    long sz;
    size_t got;
    if (!path || !(fp = fopen(path, "rb"))) return lj_vtr_err(err, cap, 0, "cannot open the file");
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0 || sz > LJ_VTR_FILE_MAX || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return lj_vtr_err(err, cap, 0, "file too large"); }   /* size before the read */
    got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) return lj_vtr_err(err, cap, 0, "read error");
    return lj_vtr_parse(buf, got, out, err, cap);
}

int lj_vtr_save(const char *path, const lj_vtr *in)
{
    FILE *fp;
    int i, bad;
    if (!path || !in || !(fp = fopen(path, "wb"))) return -1;
    bad = fprintf(fp, "%s\r\nID=%s\r\nTYPE=%s\r\nNAME=%s\r\nPURPOSE=%s\r\n", lj_vtr_header, in->id, in->type, in->name, in->purpose) < 0;
    for (i = 0; i < 256 && !bad; i++) bad = fprintf(fp, "%d=%d\r\n", i, in->map[i]) < 0;
    if (fclose(fp) != 0 || bad) { lj_log("dmx in: cannot write %s", path); return -1; }
    return 0;
}

int lj_vtr_file_name(char *dst, size_t cap, const lj_vtr *t)
{
    char name[sizeof t->type + sizeof t->name + sizeof t->id + 8];
    size_t i;
    if (!dst || !cap || !t || !t->type[0] || !t->name[0] || !lj_vtr_guid_ok(t->id)) return -1;
    if (snprintf(name, sizeof name, "%s_%s_%s", t->type, t->name, t->id) < 0) name[0] = 0;
    for (i = 0; name[i]; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == ' ' || c == ',' || c == '/' || c == '\\') name[i] = '_';                  /* "Off/on, disable treshold" -> "Off_on__disable_treshold" (LJ2's names) */
        else if (c < 32 || c == 127 || strchr(":*?\"<>|", c)) return -1;
    }
    if (strlen(name) + 4 >= cap || strlen(name) + 4 > 200) return -1;
    if (snprintf(dst, cap, "%s.vtr", name) < 0) dst[0] = 0;
    return 0;
}

int lj_vtr_stock(int i, lj_vtr *out)
{
    if (i < 0 || i >= LJ_VTR_STOCK || !out) return -1;
    memset(out, 0, sizeof *out);
    lj_str_copy(out->id, sizeof out->id, lj_vtr_stock_text[i][0]);
    lj_str_copy(out->type, sizeof out->type, "SYSTEM");
    lj_str_copy(out->name, sizeof out->name, lj_vtr_stock_text[i][1]);
    lj_str_copy(out->purpose, sizeof out->purpose, lj_vtr_stock_text[i][2]);
    memcpy(out->map, lj_vtr_stock_map[i], 256);
    return 0;
}

/* ---- the Available Functions list (exe 0x3B26E9.., 36-byte records, in exe order; captions verbatim, D13) ---- */
#define R(fn, num, cap) { LJ_DIF_##fn, num, cap }
const lj_dmxin_fn_row lj_dmxin_fn_rows[] = {
    R(MASTER_INTENSITY, 0, "Master Intensity"),
    R(INTENSITY_GROUP, 1, "Intensity Gr.1"), R(INTENSITY_GROUP, 2, "Intensity Gr.2"), R(INTENSITY_GROUP, 3, "Intensity Gr.3"), R(INTENSITY_GROUP, 4, "Intensity Gr.4"),
    R(INTENSITY_GROUP, 5, "Intensity Gr.5"), R(INTENSITY_GROUP, 6, "Intensity Gr.6"), R(INTENSITY_GROUP, 7, "Intensity Gr.7"), R(INTENSITY_GROUP, 8, "Intensity Gr.8"),
    R(CUE_BIT_1, 0, "Binary Cue Value 1"), R(CUE_BIT_2, 0, "Binary Cue Value 2"), R(CUE_BIT_4, 0, "Binary Cue Value 4"), R(CUE_BIT_8, 0, "Binary Cue Value 8"),
    R(CUE_BIT_16, 0, "Binary Cue Value 16"), R(CUE_BIT_32, 0, "Binary Cue Value 32"), R(CUE_BIT_64, 0, "Binary Cue Value 64"), R(CUE_BIT_128, 0, "Binary Cue Value 128"),
    R(LATCH_CUE, 0, "Latch Cue"),
    R(CUE_TIME, 0, "Cue Time"),
    R(SEQ_TIME, 1, "Sequence Time 1"), R(SEQ_TIME, 2, "Sequence Time 2"), R(SEQ_TIME, 3, "Sequence Time 3"), R(SEQ_TIME, 4, "Sequence Time 4"),
    R(SEQ_TIME, 5, "Sequence Time 5"), R(SEQ_TIME, 6, "Sequence Time 6"), R(SEQ_TIME, 7, "Sequence Time 7"), R(SEQ_TIME, 8, "Sequence Time 8"),
    R(SEQ_TIME, 9, "Sequence Time 9"), R(SEQ_TIME, 10, "Sequence Time 10"), R(SEQ_TIME, 11, "Sequence Time 11"), R(SEQ_TIME, 12, "Sequence Time 12"),
    R(BLACKOUT, 0, "Blackout"), R(OVERWRITE_ENABLE, 0, "Overwrite Enable"),
    R(SMOKE_BUTTON, 0, "Smoke Button"), R(SMOKE_LEVEL, 0, "Smoke Level"), R(SMOKE_DURATION, 0, "Smoke Duration"), R(SMOKE_CYCLE, 0, "Smoke Cycle"),
    R(HAZE_LEVEL, 0, "Hazer Level"), R(HAZE_FAN, 0, "Hazer Fan"),
    R(SEQ_ONOFF, 1, "Seq 1 On/Off"), R(SEQ_ONOFF, 2, "Seq 2 On/Off"), R(SEQ_ONOFF, 3, "Seq 3 On/Off"), R(SEQ_ONOFF, 4, "Seq 4 On/Off"),
    R(SEQ_ONOFF, 5, "Seq 5 On/Off"), R(SEQ_ONOFF, 6, "Seq 6 On/Off"), R(SEQ_ONOFF, 7, "Seq 7 On/Off"), R(SEQ_ONOFF, 8, "Seq 8 On/Off"),
    R(SEQ_ONOFF, 9, "Seq 9 On/Off"), R(SEQ_ONOFF, 10, "Seq 10 On/Off"), R(SEQ_ONOFF, 11, "Seq 11 On/Off"), R(SEQ_ONOFF, 12, "Seq 12 On/Off"),
    R(SEQ_INTENSITY, 1, "Seq 1 Intensity"), R(SEQ_INTENSITY, 2, "Seq 2 Intensity"), R(SEQ_INTENSITY, 3, "Seq 3 Intensity"), R(SEQ_INTENSITY, 4, "Seq 4 Intensity"),
    R(SEQ_INTENSITY, 5, "Seq 5 Intensity"), R(SEQ_INTENSITY, 6, "Seq 6 Intensity"), R(SEQ_INTENSITY, 7, "Seq 7 Intensity"), R(SEQ_INTENSITY, 8, "Seq 8 Intensity"),
    R(SEQ_INTENSITY, 9, "Seq 9 Intensity"), R(SEQ_INTENSITY, 10, "Seq 10 Intensity"), R(SEQ_INTENSITY, 11, "Seq 11 Intensity"), R(SEQ_INTENSITY, 12, "Seq 12 Intensity"),
    R(MACRO_AMP, 0, "Cue Macro Amplitude"), R(MACRO_SPEED, 0, "Cue Macro Speed"),
    R(CL_GO, 0, "Cuelist Go"), R(CL_BACK, 0, "Cuelist Back"), R(CL_STOP, 0, "Cuelist Stop"), R(CL_STOP_CLEAR, 0, "Cuelist Stop and Clear"),
    R(STATIC_ACTIVATE, 1, "Static 01, Latch"), R(STATIC_ACTIVATE, 2, "Static 02, Latch"), R(STATIC_ACTIVATE, 3, "Static 03, Latch"), R(STATIC_ACTIVATE, 4, "Static 04, Latch"),
    R(STATIC_ACTIVATE, 5, "Static 05, Latch"), R(STATIC_ACTIVATE, 6, "Static 06, Latch"), R(STATIC_ACTIVATE, 7, "Static 07, Latch"), R(STATIC_ACTIVATE, 8, "Static 08, Latch"),
    R(STATIC_ACTIVATE, 9, "Static 09, Latch"), R(STATIC_ACTIVATE, 10, "Static 10, Latch"), R(STATIC_ACTIVATE, 11, "Static 11, Latch"), R(STATIC_ACTIVATE, 12, "Static 12, Latch"),
    R(STATIC_ACTIVATE, 13, "Static 13, Latch"), R(STATIC_ACTIVATE, 14, "Static 14, Latch"), R(STATIC_ACTIVATE, 15, "Static 15, Latch"), R(STATIC_ACTIVATE, 16, "Static 16, Latch"),
    R(STATIC_ACTIVATE, 17, "Static 17, Latch"), R(STATIC_ACTIVATE, 18, "Static 18, Latch"), R(STATIC_ACTIVATE, 19, "Static 19, Latch"), R(STATIC_ACTIVATE, 20, "Static 20, Latch"),
    R(LATCH_PAGE_CUE, 0, "Latch Current Cue from Page"), R(PREV_CUE_PAGE, 0, "Previous Cue Page"), R(NEXT_CUE_PAGE, 0, "Next Cue Page"),
    R(LATCH_PREV_PAGE_CUE, 0, "Latch Previous Cue from Page"), R(LATCH_NEXT_PAGE_CUE, 0, "Latch Next Cue from Page"),
    R(SELECT_PREV_PAGE_CUE, 0, "Select Previous Cue from Page"), R(SELECT_NEXT_PAGE_CUE, 0, "Select Next Cue from Page"),
    R(STATIC_LEVEL, 1, "Static 01, Level"), R(STATIC_LEVEL, 2, "Static 02, Level"), R(STATIC_LEVEL, 3, "Static 03, Level"), R(STATIC_LEVEL, 4, "Static 04, Level"),
    R(STATIC_LEVEL, 5, "Static 05, Level"), R(STATIC_LEVEL, 6, "Static 06, Level"), R(STATIC_LEVEL, 7, "Static 07, Level"), R(STATIC_LEVEL, 8, "Static 08, Level"),
    R(STATIC_LEVEL, 9, "Static 09, Level"), R(STATIC_LEVEL, 10, "Static 10, Level"), R(STATIC_LEVEL, 11, "Static 11, Level"), R(STATIC_LEVEL, 12, "Static 12, Level"),
    R(STATIC_LEVEL, 13, "Static 13, Level"), R(STATIC_LEVEL, 14, "Static 14, Level"), R(STATIC_LEVEL, 15, "Static 15, Level"), R(STATIC_LEVEL, 16, "Static 16, Level"),
    R(STATIC_LEVEL, 17, "Static 17, Level"), R(STATIC_LEVEL, 18, "Static 18, Level"), R(STATIC_LEVEL, 19, "Static 19, Level"), R(STATIC_LEVEL, 20, "Static 20, Level"),
    R(AUDIO_GAIN, 0, "Audio Analyzer, Gain"), R(AUDIO_NOISE, 0, "Audio Analyzer, Noise Threshold"),
    R(HTP_GROUP, 1, "HTP Group 1"), R(HTP_GROUP, 2, "HTP Group 2"), R(HTP_GROUP, 3, "HTP Group 3"), R(HTP_GROUP, 4, "HTP Group 4"),
    R(HTP_GROUP, 5, "HTP Group 5"), R(HTP_GROUP, 6, "HTP Group 6"), R(HTP_GROUP, 7, "HTP Group 7"), R(HTP_GROUP, 8, "HTP Group 8"),
    R(HTP_GROUP, 9, "HTP Group 9"), R(HTP_GROUP, 10, "HTP Group 10"), R(HTP_GROUP, 11, "HTP Group 11"), R(HTP_GROUP, 12, "HTP Group 12"),
    R(HTP_GROUP, 13, "HTP Group 13"), R(HTP_GROUP, 14, "HTP Group 14"), R(HTP_GROUP, 15, "HTP Group 15"), R(HTP_GROUP, 16, "HTP Group 16"),
    R(HTP_GROUP, 17, "HTP Group 17"), R(HTP_GROUP, 18, "HTP Group 18"), R(HTP_GROUP, 19, "HTP Group 19"), R(HTP_GROUP, 20, "HTP Group 20"),
    R(HTP_GROUP, 21, "HTP Group 21"), R(HTP_GROUP, 22, "HTP Group 22"), R(HTP_GROUP, 23, "HTP Group 23"), R(HTP_GROUP, 24, "HTP Group 24"),
    R(TRIG_ALL_MANUAL_DEFAULT, 0, "Trig all manual default"), R(EMULATE_AUDIO_TRIG, 0, "Emulate Audio Trig"),
    R(CUE_BIT_512, 0, "Binary Cue Value 512"), R(CL_GO_ALT, 0, "Cuelist Go (Alt)"),
    R(MACRO_AMP_INV, 0, "Cue Macro Amplitude (Inv)"), R(MACRO_SPEED_INV, 0, "Cue Macro Speed (Inv)"),
};
#undef R
const int lj_dmxin_fn_nrows = (int)(sizeof lj_dmxin_fn_rows / sizeof lj_dmxin_fn_rows[0]);

int lj_dmxin_fn_caption(const lj_dmxin_assign *a, char *dst, size_t cap)
{
    int i;
    if (!dst || !cap) return -1;
    dst[0] = 0;
    if (!a || a->fn == LJ_DIF_NONE || a->fn >= LJ_DIF_COUNT) return 0;
    switch (a->fn) {                                                                       /* the dragged programs (dmxinflashseq.gif: "Flash Seq 0014 <name>"; the window appends the name) */
    case LJ_DIF_FLASH_SEQ:       { if (snprintf(dst, cap, "Flash Seq %04u", (unsigned)a->num) < 0) dst[0] = 0; } return 0;
    case LJ_DIF_LATCH_CUE_N:     { if (snprintf(dst, cap, "Cue %04u", (unsigned)a->num) < 0) dst[0] = 0; } return 0;       /* PROPOSAL rig W5-DMXIN-5: the Flash Seq pattern */
    case LJ_DIF_LATCH_CUELIST_N: { if (snprintf(dst, cap, "CueList %04u", (unsigned)a->num) < 0) dst[0] = 0; } return 0;
    case LJ_DIF_LATCH_BGCUE_N:   { if (snprintf(dst, cap, "BG Cue %04u", (unsigned)a->num) < 0) dst[0] = 0; } return 0;
    default: break;
    }
    for (i = 0; i < lj_dmxin_fn_nrows; i++) if (lj_dmxin_fn_rows[i].fn == a->fn && lj_dmxin_fn_rows[i].num == a->num) return lj_str_copy(dst, cap, lj_dmxin_fn_rows[i].caption);
    return -1;
}

/* ---- DmxIn.dat (design 5.2 grammar, "LJ4 dmxin 1"; PROPOSAL name) ---- */
void lj_dmxin_map_init(lj_dmxin_map *m) { if (m) memset(m, 0, sizeof *m); }

static int lj_di_num_ok(int fn, unsigned long num)   /* ME-172: a record number for the program rows, else the function's own row range (none = 0) */
{
    int i, max = 0;
    if (fn == LJ_DIF_FLASH_SEQ || fn == LJ_DIF_LATCH_CUE_N || fn == LJ_DIF_LATCH_CUELIST_N || fn == LJ_DIF_LATCH_BGCUE_N) return num >= 1 && num <= 99999999ul;
    for (i = 0; i < lj_dmxin_fn_nrows; i++) if (lj_dmxin_fn_rows[i].fn == fn && lj_dmxin_fn_rows[i].num > max) max = lj_dmxin_fn_rows[i].num;
    return max ? num >= 1 && num <= (unsigned long)max : num == 0;
}

static int lj_di_split(const char *v, unsigned long *ch, const char **rest)   /* "<ch 1..512> <rest>" */
{
    char *end;
    errno = 0;
    *ch = strtoul(v, &end, 10);
    if (errno == ERANGE || end == v || *ch < 1 || *ch > LJ_LINK_CH || (*end && *end != ' ')) return -1;
    *rest = *end ? end + 1 : end;
    return 0;
}

int lj_dmxin_map_load(const char *path, lj_dmxin_map *out, char *err, size_t cap)
{
    static char val[LJ_VTR_LINE_MAX * 8];
    char key[16];
    lj_rec *r;
    int fmt, rc = 0, i, k = 0;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    lj_dmxin_map_init(out);
    if (!path) return -1;
    {   FILE *fp = fopen(path, "rb"); if (!fp) { for (i = 1; i <= LJ_LINK_CH; i++) lj_dmxin_set_table(i, NULL); return 0; } fclose(fp); }   /* missing = empty map */
    if (!(r = lj_rec_open_read(path, "dmxin", &fmt))) return lj_vtr_err(err, cap, 0, "not a DMX-in assignment file");
    while (rc == 0 && (k = lj_rec_next(r, key, sizeof key, val, sizeof val)) == 1) {
        unsigned long ch, num;
        const char *rest;
        char *end;
        if (strcmp(key, "time_mode") == 0) { out->time_mode = (uint8_t)(strcmp(val, "1") == 0 ? 1 : strcmp(val, "2") == 0 ? 2 : strcmp(val, "3") == 0 ? 3 : 0); continue; }
        if (strcmp(key, "cue0_clears") == 0) { out->cue0_clears = (uint8_t)(strcmp(val, "1") == 0); continue; }
        if (strcmp(key, "ch") != 0 && strcmp(key, "tid") != 0 && strcmp(key, "ttype") != 0 && strcmp(key, "tname") != 0 && strcmp(key, "tpurpose") != 0 && strcmp(key, "tmap") != 0) continue;
        if (lj_di_split(val, &ch, &rest) != 0) { rc = lj_vtr_err(err, cap, 0, "bad channel number"); break; }
        if (strcmp(key, "ch") == 0) {                                                      /* "ch=12 htp_group 3" */
            char word[32];
            size_t wl = strcspn(rest, " ");
            if (wl == 0 || wl >= sizeof word) { rc = lj_vtr_err(err, cap, 0, "bad function word"); break; }
            memcpy(word, rest, wl); word[wl] = 0;
            for (i = 1; i < LJ_DIF_COUNT && strcmp(lj_dmxin_fn_words[i], word) != 0; i++) { }
            if (i == LJ_DIF_COUNT) continue;                                               /* an unknown word: a later version's function, skipped */
            end = NULL; errno = 0; num = rest[wl] ? strtoul(rest + wl + 1, &end, 10) : 0;
            if (errno == ERANGE || (end && (end == rest + wl + 1 || *end)) || !lj_di_num_ok(i, num)) { rc = lj_vtr_err(err, cap, 0, "function number out of range"); break; }   /* ME-172: idx << 8 never aliases */
            out->ch[ch - 1].fn = (uint8_t)i; out->ch[ch - 1].num = (uint32_t)num;
        } else {
            lj_dmxin_assign *a = &out->ch[ch - 1];
            if (!a->has_table) { lj_vtr_identity(&a->table); a->has_table = 1; }
            if (strcmp(key, "tid") == 0) rc = lj_str_copy(a->table.id, sizeof a->table.id, rest);
            else if (strcmp(key, "ttype") == 0) rc = lj_str_copy(a->table.type, sizeof a->table.type, rest);
            else if (strcmp(key, "tname") == 0) rc = lj_str_copy(a->table.name, sizeof a->table.name, rest);
            else if (strcmp(key, "tpurpose") == 0) rc = lj_str_copy(a->table.purpose, sizeof a->table.purpose, rest);
            else {                                                                         /* "tmap=12 v0 v1 .. v255" */
                const char *p = rest;
                for (i = 0; i < 256 && rc == 0; i++) {
                    long v;
                    size_t l = strcspn(p, " ");
                    if (lj_vtr_num(p, l, &v) != 0) rc = -1; else { a->table.map[i] = (uint8_t)v; p += l + (p[l] == ' '); }
                }
                if (rc == 0 && *p) rc = -1;
            }
            if (rc != 0) { lj_vtr_err(err, cap, 0, "bad table line"); break; }
        }
    }
    if (rc == 0 && k < 0) rc = lj_vtr_err(err, cap, 0, "read error");
    lj_rec_close(r);
    if (rc != 0) { lj_dmxin_map_init(out); return -1; }
    for (i = 0; i < LJ_LINK_CH; i++) lj_dmxin_set_table(i + 1, out->ch[i].has_table ? out->ch[i].table.map : NULL);   /* the saved map is the live one */
    return 0;
}

int lj_dmxin_map_save(const char *path, const lj_dmxin_map *in)
{
    char line[256 * 4 + 1];
    lj_rec *r;
    int i, k, bad = 0;
    if (!path || !in || !(r = lj_rec_open_write(path, "dmxin", 1))) return -1;
    bad |= lj_rec_write(r, "time_mode", "%d", in->time_mode) != 0;
    bad |= lj_rec_write(r, "cue0_clears", "%d", in->cue0_clears) != 0;
    for (i = 0; i < LJ_LINK_CH && !bad; i++) {
        const lj_dmxin_assign *a = &in->ch[i];
        if (a->fn != LJ_DIF_NONE && a->fn < LJ_DIF_COUNT) bad |= lj_rec_write(r, "ch", "%d %s %u", i + 1, lj_dmxin_fn_words[a->fn], (unsigned)a->num) != 0;
        if (!a->has_table) continue;
        bad |= lj_rec_write(r, "tid", "%d %s", i + 1, a->table.id) != 0;
        bad |= lj_rec_write(r, "ttype", "%d %s", i + 1, a->table.type) != 0;
        bad |= lj_rec_write(r, "tname", "%d %s", i + 1, a->table.name) != 0;
        bad |= lj_rec_write(r, "tpurpose", "%d %s", i + 1, a->table.purpose) != 0;
        for (k = 0, line[0] = 0; k < 256; k++) { if (snprintf(line + strlen(line), sizeof line - strlen(line), k ? " %d" : "%d", a->table.map[k]) < 0) (line + strlen(line))[0] = 0; }
        bad |= lj_rec_write(r, "tmap", "%d %s", i + 1, line) != 0;
    }
    if (lj_rec_close(r) != 0 || bad) { lj_log("dmx in: cannot write %s", path); return -1; }
    return 0;
}

/* ---- the functions (05 s3.2), UI thread ---- */
static struct { uint8_t prev[LJ_LINK_CH], primed, latched, overwrite, overwrite_set, fo_seen; uint32_t cue, loaded; int16_t last_time[1 + LJ_DMXIN_SEQ_TIMES]; } lj_df = { .overwrite = 1, .loaded = 0xFFFFFFFFu };

static int lj_df_time_due(int idx, int v, int mode)   /* Cue Time Control: always, or when the value moved 1 / 2 / 3 steps from the last applied */
{
    int last = lj_df.last_time[idx];
    if (last < 0 || mode == LJ_DMXIN_TIME_ALWAYS || (v > last ? v - last : last - v) >= mode) { lj_df.last_time[idx] = (int16_t)v; return 1; }
    return 0;
}

static void lj_df_level(enum lj_fn fn, uint32_t idx, int v) { lj_fn_run_prog(fn, idx << 8 | (uint32_t)v, 1); }

int lj_dmxin_functions(const lj_dmxin_map *m, const uint8_t in[LJ_LINK_CH], int functions_off)
{
    uint32_t bits = 0;
    int c, fired = 0, have_latch = 0, latch = 0, have_ow = 0, bitsum = 0;
    if (!m || !in) return 0;
    if (!lj_df.primed) { int i; memcpy(lj_df.prev, in, sizeof lj_df.prev); for (i = 0; i <= LJ_DMXIN_SEQ_TIMES; i++) lj_df.last_time[i] = -1; }
    if (functions_off) {                                                                   /* the Global Patch overrides stay: the gate opens (05 s3.1) */
        if (!lj_df.overwrite) { lj_df_level(LJ_FN_DMXIN_OVERWRITE, 0, 1); lj_df.overwrite = 1; fired++; }
        memcpy(lj_df.prev, in, sizeof lj_df.prev); lj_df.primed = 1;
        return fired;
    }
    for (c = 0; c < LJ_LINK_CH; c++) {
        const lj_dmxin_assign *a = &m->ch[c];
        int v = in[c], p = lj_df.prev[c], first = !lj_df.primed;
        int on = v > 127, was = p > 127, rise = on && (!was || first), changed = v != p || first;
        uint32_t n = a->num;
        switch (a->fn) {
        case LJ_DIF_NONE: break;
        case LJ_DIF_CUE_BIT_1: case LJ_DIF_CUE_BIT_2: case LJ_DIF_CUE_BIT_4: case LJ_DIF_CUE_BIT_8:
        case LJ_DIF_CUE_BIT_16: case LJ_DIF_CUE_BIT_32: case LJ_DIF_CUE_BIT_64: case LJ_DIF_CUE_BIT_128:
            bitsum = 1; if (on) bits |= 1u << (a->fn - LJ_DIF_CUE_BIT_1); break;
        case LJ_DIF_CUE_BIT_512: bitsum = 1; if (on) bits |= 512u; break;              /* RN 2.5 b1 */
        case LJ_DIF_LATCH_CUE: have_latch = 1; latch |= on; break;
        case LJ_DIF_LATCH_PAGE_CUE:       if (rise) { lj_fn_run(LJ_FN_LOAD_SELECTED_CUE_PAGE, 1); fired++; } break;
        case LJ_DIF_PREV_CUE_PAGE:        if (rise) { lj_fn_run(LJ_FN_PREVIOUS_CUE_PAGE, 1); fired++; } break;
        case LJ_DIF_NEXT_CUE_PAGE:        if (rise) { lj_fn_run(LJ_FN_NEXT_CUE_PAGE, 1); fired++; } break;
        case LJ_DIF_LATCH_PREV_PAGE_CUE:  if (rise) { lj_fn_run(LJ_FN_CUE_PAGE_LATCH_PREVIOUS, 1); fired++; } break;
        case LJ_DIF_LATCH_NEXT_PAGE_CUE:  if (rise) { lj_fn_run(LJ_FN_CUE_PAGE_LATCH_NEXT, 1); fired++; } break;
        case LJ_DIF_SELECT_PREV_PAGE_CUE: if (rise) { lj_fn_run(LJ_FN_CUE_PAGE_SELECT_PREVIOUS, 1); fired++; } break;
        case LJ_DIF_SELECT_NEXT_PAGE_CUE: if (rise) { lj_fn_run(LJ_FN_CUE_PAGE_SELECT_NEXT, 1); fired++; } break;
        case LJ_DIF_CL_GO:                if (rise) { lj_fn_run(LJ_FN_CUELIST_GO, 1); fired++; } break;
        case LJ_DIF_CL_BACK:              if (rise) { lj_fn_run(LJ_FN_CUELIST_BACK, 1); fired++; } break;
        case LJ_DIF_CL_STOP:              if (rise) { lj_fn_run(LJ_FN_CUELIST_STOP, 1); fired++; } break;
        case LJ_DIF_CL_STOP_CLEAR:        if (rise) { lj_fn_run(LJ_FN_CUELIST_STOP_AND_CLEAR, 1); fired++; } break;
        case LJ_DIF_CL_GO_ALT:                                                             /* 0-127 -> 128-255; 128-191 -> 192-255; 192-255 -> 128-191 */
            if (!first && ((p <= 127 && v >= 128) || (p >= 128 && p <= 191 && v >= 192) || (p >= 192 && v >= 128 && v <= 191))) { lj_fn_run(LJ_FN_CUELIST_GO, 1); fired++; }
            break;
        case LJ_DIF_TRIG_ALL_MANUAL_DEFAULT:
            if (rise) { lj_fn_run(LJ_FN_TRIG_ALL_MANUAL_DEFAULT, 1); lj_fn_run(LJ_FN_TRIG_ALL_MANUAL_DEFAULT, 0); fired++; }   /* one trig per rising edge */
            break;
        case LJ_DIF_EMULATE_AUDIO_TRIG:   if (rise) { lj_fn_run(LJ_FN_EMULATE_AUDIO_TRIG, 1); fired++; } break;
        case LJ_DIF_FLASH_SEQ:            if (on != was || (first && on)) { lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, n, on); fired++; } break;   /* > 127 on, < 128 off */
        case LJ_DIF_LATCH_CUE_N:          if (rise) { lj_fn_run_prog(LJ_FN_PROG_CUE, n, 1); fired++; } break;
        case LJ_DIF_LATCH_CUELIST_N:      if (rise) { lj_fn_run_prog(LJ_FN_PROG_CUELIST, n, 1); fired++; } break;
        case LJ_DIF_LATCH_BGCUE_N:        if (rise) { lj_fn_run_prog(LJ_FN_PROG_BGCUE, n, 1); fired++; } break;
        case LJ_DIF_STATIC_ACTIVATE: {                                                     /* >= 50 %: flash and merge modes disregarded (05 s3.2) */
            int st = v >= 128, sp = p >= 128;
            if (st != sp || first) { lj_df_level(LJ_FN_DMXIN_STATIC, n, st); fired++; }
            break; }
        case LJ_DIF_SEQ_ONOFF:            if (on != was || first) { lj_df_level(LJ_FN_DMXIN_SEQ_ONOFF, n, on); fired++; } break;
        case LJ_DIF_BLACKOUT:             if (on != was || first) { lj_df_level(LJ_FN_DMXIN_BLACKOUT, 0, on); fired++; } break;
        case LJ_DIF_OVERWRITE_ENABLE:
            have_ow = 1;
            if (on != lj_df.overwrite || !lj_df.overwrite_set) { lj_df_level(LJ_FN_DMXIN_OVERWRITE, 0, on); lj_df.overwrite = (uint8_t)on; lj_df.overwrite_set = 1; fired++; }
            break;
        case LJ_DIF_SMOKE_BUTTON:         if (on != was || first) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_BUTTON, on); fired++; } break;
        case LJ_DIF_SMOKE_LEVEL:          if (changed) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_LEVEL, v); fired++; } break;
        case LJ_DIF_SMOKE_DURATION:       if (changed) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_DURATION, v); fired++; } break;
        case LJ_DIF_SMOKE_CYCLE:          if (changed) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_CYCLE, v); fired++; } break;
        case LJ_DIF_HAZE_LEVEL:           if (changed) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_HAZE, v); fired++; } break;
        case LJ_DIF_HAZE_FAN:             if (changed) { lj_df_level(LJ_FN_DMXIN_SMOKE, LJ_DMXIN_SMOKE_FAN, v); fired++; } break;
        case LJ_DIF_HTP_GROUP:            if (changed) { lj_df_level(LJ_FN_DMXIN_HTP, n, v); fired++; } break;
        case LJ_DIF_STATIC_LEVEL:         if (changed) { lj_df_level(LJ_FN_DMXIN_STATIC_LEVEL, n, v); fired++; } break;
        case LJ_DIF_SEQ_INTENSITY:        if (changed) { lj_df_level(LJ_FN_DMXIN_SEQ_INT, n, v); fired++; } break;
        case LJ_DIF_MASTER_INTENSITY:     if (changed) { lj_df_level(LJ_FN_DMXIN_MASTER, 0, v); fired++; } break;
        case LJ_DIF_INTENSITY_GROUP:      if (changed) { lj_df_level(LJ_FN_DMXIN_MASTER, n, v); fired++; } break;
        case LJ_DIF_MACRO_AMP:            if (changed) { lj_df_level(LJ_FN_DMXIN_MACRO, 0, v); fired++; } break;
        case LJ_DIF_MACRO_SPEED:          if (changed) { lj_df_level(LJ_FN_DMXIN_MACRO, 1, v); fired++; } break;
        case LJ_DIF_MACRO_AMP_INV:        if (changed) { lj_df_level(LJ_FN_DMXIN_MACRO, 0, 255 - v); fired++; } break;   /* RN 2.6 b4 */
        case LJ_DIF_MACRO_SPEED_INV:      if (changed) { lj_df_level(LJ_FN_DMXIN_MACRO, 1, 255 - v); fired++; } break;
        case LJ_DIF_CUE_TIME:             if (changed && lj_df_time_due(0, v, m->time_mode)) { lj_df_level(LJ_FN_DMXIN_TIME, 0, v); fired++; } break;
        case LJ_DIF_SEQ_TIME:             if (changed && n >= 1 && n <= LJ_DMXIN_SEQ_TIMES && lj_df_time_due((int)n, v, m->time_mode)) { lj_df_level(LJ_FN_DMXIN_TIME, n, v); fired++; } break;
        case LJ_DIF_AUDIO_GAIN:           if (changed) { lj_df_level(LJ_FN_DMXIN_AUDIO, 0, v); fired++; } break;
        case LJ_DIF_AUDIO_NOISE:          if (changed) { lj_df_level(LJ_FN_DMXIN_AUDIO, 1, v); fired++; } break;
        default: break;
        }
    }
    if (!have_ow && !lj_df.overwrite) { lj_df_level(LJ_FN_DMXIN_OVERWRITE, 0, 1); lj_df.overwrite = 1; fired++; }   /* always on while unassigned */
    if (!have_ow) lj_df.overwrite_set = 0;
    lj_df.cue = bitsum ? bits : 0;
    if (have_latch && latch && bitsum && (lj_df.cue != lj_df.loaded || !lj_df.latched)) {   /* "without the Latch Cue function it is NOT possible to load cues via DMX in" */
        if (lj_df.cue) { lj_fn_run_prog(LJ_FN_PROG_CUE, lj_df.cue, 1); fired++; }
        else if (m->cue0_clears) { lj_fn_run(LJ_FN_NEW_CLEAR_CUE, 1); fired++; }           /* Preferences > Cue Latch > Clear Cue on Cue# 0 */
        lj_df.loaded = lj_df.cue;
    }
    lj_df.latched = (uint8_t)(have_latch && latch);
    if (!lj_df.latched) lj_df.loaded = 0xFFFFFFFFu;
    memcpy(lj_df.prev, in, sizeof lj_df.prev); lj_df.primed = 1;
    return fired;
}

int lj_dmxin_remote_cue(uint32_t *cue)
{
    if (!lj_df.latched || (!lj_df.cue && lj_df.loaded == 0xFFFFFFFFu)) return 0;
    if (cue) *cue = lj_df.cue;
    return 1;
}

void lj_dmxin_functions_reset(void) { memset(&lj_df, 0, sizeof lj_df); lj_df.overwrite = 1; lj_df.loaded = 0xFFFFFFFFu; }

int lj_dmxin_functions_step(const lj_dmxin_map *m, int changed, int functions_off)
{
    uint8_t fo = (uint8_t)(functions_off ? 2 : 1);
    int run = lj_di.f.live && (changed || lj_df.fo_seen != fo);                           /* ME-167: only a frame someone sent; once more on a functions_off change */
    lj_df.fo_seen = fo;
    return run ? lj_dmxin_functions(m, lj_di.f.out, functions_off) : 0;
}

void lj_dmxin_reset(void)
{
    int i;
    lj_dmxin_close();
    memset(&lj_di.f, 0, sizeof lj_di.f);
    memset(lj_di.has_table, 0, sizeof lj_di.has_table);
    for (i = 0; i < LJ_DMXIN_EMULATE; i++) lj_di.emu[i] = -1;
    lj_di.out_valid = 0; lj_di.qn = 0; lj_di.hz_n = 0; lj_di.hz_t0 = 0; lj_di.have_cid = 0;
    lj_dmxin_cfg_default(&lj_di.cfg);
    lj_dmxin_functions_reset();
}
