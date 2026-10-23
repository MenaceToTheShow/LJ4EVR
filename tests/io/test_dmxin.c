/* test_dmxin.c - the dmxin track's rows (docs/w5-contracts.md seam 2). The contract seeds two fuzz rows (W5-X-HOSTILE):
 *   fuzz_sacn_rx  a data packet from lj_sacn_pack_data (our own sender, design 8.1) must parse, then LJ_FUZZ_ITERS
 *                 mutations of it through lj_sacn_rx_parse and lj_dmxin_rx; the frame stays within its bounds
 *   fuzz_vtr      a synthetic .vtr text must parse, then mutations through lj_vtr_parse
 * Each fails (FAIL, rc 1) when its seed no longer parses.
 * The track appends its rule rows (W5-DMXIN-1..8) to this file. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen in the ME-172 rows */
#include <stdio.h>
#include <string.h>
#include "io/lj_dmxin.h"
#include "io/lj_sacn.h"
#include "core/lj_str.h"
#include "lj_fuzz.h"

int lj_test_fuzz_sacn_rx(int argc, char **argv)
{
    static uint8_t seed[LJ_SACN_DATA_LEN], buf[LJ_DMXIN_PKT_MAX + 1], slots[LJ_LINK_CH];
    lj_sacn_cfg cfg;
    lj_sacn_rx rx;
    const lj_dmxin_frame *f;
    uint32_t s = 0x5AC17u;
    size_t n;
    int i, k, bad = 0;
    (void)argc; (void)argv;
    lj_sacn_cfg_default(&cfg, "fuzz");
    for (i = 0; i < LJ_LINK_CH; i++) slots[i] = (uint8_t)i;
    if (lj_sacn_pack_data(seed, &cfg, 10, 1, 0, slots) != 0 || lj_sacn_rx_parse(seed, sizeof seed, &rx) != 0 || rx.universe != 10 || rx.n != LJ_LINK_CH || rx.slots[7] != 7) {
        printf("FAIL fuzz_sacn_rx: lj_sacn_rx_parse refuses our own data packet\n");
        return 1;
    }
    if (lj_fuzz_seed(argc, argv, "sacn_data", seed, sizeof seed)) return 0;   /* ME-186 */
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sizeof seed); n = sizeof seed;
        for (k = (int)(lj_fuzz_next(&s) % 8); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        if (lj_sacn_rx_parse(buf, n, &rx) == 0 && (rx.n > LJ_LINK_CH || rx.slots + rx.n > buf + n || rx.name[63] != 0)) { if (!bad++) printf("FAIL iteration %d: parse accepted out-of-bounds fields\n", i); }
        lj_dmxin_rx(LJ_DMXIN_SACN, buf, n, 0x7F000002u, (uint64_t)i * 1000u);
        f = lj_dmxin_frame_get();
        if (f->channels > LJ_LINK_CH || f->nsources < 0 || f->nsources > LJ_DMXIN_SOURCES) { if (!bad++) printf("FAIL iteration %d: frame out of bounds\n", i); }
    }
    printf("fuzz_sacn_rx: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}

int lj_test_fuzz_vtr(int argc, char **argv)
{
    static const char text[] = "// LightJockey DMX value translation file\r\nID={00000000-0000-4000-8000-000000000001}\r\nTYPE=USER\r\nNAME=Fuzz\r\nPURPOSE=synthetic\r\n0=0\r\n128=255\r\n255=0\r\n";
    static uint8_t buf[1024];
    lj_vtr t;
    char err[128];
    uint32_t s = 0x7A7u;
    size_t n;
    int i, k;
    (void)argc; (void)argv;
    if (lj_vtr_parse(text, sizeof text - 1, &t, err, sizeof err) != 0 || t.map[128] != 255 || t.map[1] != 1 || t.map[255] != 0) {
        printf("FAIL fuzz_vtr: lj_vtr_parse refuses the synthetic table\n");
        return 1;
    }
    if (lj_fuzz_seed(argc, argv, "table.vtr", text, sizeof text - 1)) return 0;   /* ME-186 */
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, text, sizeof text - 1); n = sizeof text - 1;
        for (k = (int)(lj_fuzz_next(&s) % 8); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        lj_vtr_parse((const char *)buf, n, &t, err, sizeof err);   /* must not read past n: ASan is the check */
    }
    printf("fuzz_vtr: %d iterations, PASS\n", LJ_FUZZ_ITERS);
    return 0;
}

/* ---- dmxin track rows (W5-DMXIN-1..7, W5-ART-IN) ---- */
#include <stdlib.h>
#include "core/lj_clock.h"
#include "io/lj_artnet.h"
#include "io/lj_fn.h"
#include "io/lj_net.h"
#include "io/lj_socket.h"

static int di_fails;
#define DCHECK(cond) do { if (!(cond)) { di_fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void di_pkt(uint8_t out[LJ_SACN_DATA_LEN], const char *cid_tag, uint16_t universe, uint8_t prio, uint8_t seq, uint8_t options, uint8_t fill)
{
    lj_sacn_cfg cfg;
    uint8_t slots[LJ_LINK_CH];
    memset(&cfg, 0, sizeof cfg);
    lj_str_copy(cfg.name, sizeof cfg.name, cid_tag);
    memset(cfg.cid, cid_tag[0], 16);
    cfg.priority = prio;
    memset(slots, fill, sizeof slots);
    lj_sacn_pack_data(out, &cfg, universe, seq, options, slots);
}
static const uint8_t di_own[16] = { 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O' };
static void di_open(uint16_t universe, uint8_t loss)
{
    lj_dmxin_cfg c;
    lj_dmxin_reset();
    lj_dmxin_cfg_default(&c); c.universe = universe; c.loss = loss;   /* enabled 0: no socket, the rules alone */
    lj_dmxin_open(&c, di_own);
}
static void di_be16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

static void di_rules(void)
{
    static uint8_t p[LJ_SACN_DATA_LEN], q[LJ_SACN_DATA_LEN];
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    lj_sacn_rx rx;
    static const struct { size_t off; uint8_t val; const char *what; } bad[] = {
        { 1, 0x11, "preamble" }, { 3, 1, "post-amble" }, { 4, 'B', "ACN id" }, { 21, 0x08, "root vector (extended)" }, { 43, 0x03, "framing vector" },
        { 117, 0x03, "DMP vector" }, { 118, 0xA0, "address / data type" }, { 120, 1, "first address" }, { 122, 2, "increment" },
        { 125, 0xDD, "START code 0xDD" }, { 108, 201, "priority > 200" } };
    size_t i;
    int k;
    uint64_t t = 1000000;
    /* each discard rule (E1.31 5.1-5.5, 6.2, 7.2-7.6) */
    di_pkt(p, "A", 7, 100, 0, 0, 9);
    DCHECK(lj_sacn_rx_parse(p, sizeof p, &rx) == 0 && rx.universe == 7 && rx.n == 512 && rx.priority == 100 && rx.slots[0] == 9);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { memcpy(q, p, sizeof q); q[bad[i].off] = bad[i].val; if (lj_sacn_rx_parse(q, sizeof q, &rx) == 0) { di_fails++; printf("FAIL rule %s not discarded\n", bad[i].what); } }
    memcpy(q, p, sizeof q); di_be16(q + 123, 0); DCHECK(lj_sacn_rx_parse(q, sizeof q, &rx) != 0);          /* property count 0 */
    memcpy(q, p, sizeof q); di_be16(q + 123, 514); DCHECK(lj_sacn_rx_parse(q, sizeof q, &rx) != 0);        /* 514 */
    DCHECK(lj_sacn_rx_parse(p, sizeof p - 1, &rx) != 0);                                                   /* fewer bytes than claimed */
    memcpy(q, p, sizeof q); di_be16(q + 113, 0); DCHECK(lj_sacn_rx_parse(q, sizeof q, &rx) != 0);          /* universe 0 */
    memcpy(q, p, sizeof q); memset(q + 44, 'x', 64); DCHECK(lj_sacn_rx_parse(q, sizeof q, &rx) == 0 && rx.name[63] == 0 && strlen(rx.name) == 63);   /* unterminated name (W5-X-TEXT) */
    /* a short frame: 10 slots, the rest 0 */
    memcpy(q, p, sizeof q); di_be16(q + 123, 11); di_be16(q + 16, 0x7000 | (136 - 16)); di_be16(q + 38, 0x7000 | (136 - 38)); di_be16(q + 115, 0x7000 | (136 - 115));
    DCHECK(lj_sacn_rx_parse(q, 136, &rx) == 0 && rx.n == 10);
    di_open(7, LJ_DMXIN_LOSS_HOLD);
    DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, 136, 0x7F000002u, t) == 0 && f->raw[9] == 9 && f->raw[10] == 0 && f->channels == 10);
    /* the universe comes from the packet: another universe is not ours */
    di_open(7, 0); di_pkt(q, "A", 8, 100, 0, 0, 5); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) != 0 && f->nsources == 0);
    /* our own CID is ignored */
    di_pkt(q, "O", 7, 100, 0, 0, 5); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) != 0 && f->nsources == 0);
    /* Preview_Data ignored */
    di_pkt(q, "A", 7, 100, 0, 0x80, 5); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) != 0 && f->nsources == 0);
    /* sequence window: B - A in (-20, 0] discarded, -20 accepted (a restarted source) */
    di_pkt(q, "A", 7, 100, 50, 0, 1); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) == 0 && f->raw[0] == 1);
    di_pkt(q, "A", 7, 100, 50, 0, 2); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) != 0 && f->raw[0] == 1);
    di_pkt(q, "A", 7, 100, 31, 0, 3); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) != 0 && f->raw[0] == 1);
    di_pkt(q, "A", 7, 100, 30, 0, 4); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) == 0 && f->raw[0] == 4);
    di_pkt(q, "A", 7, 100, 31, 0, 6); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t) == 0 && f->raw[0] == 6);
    /* highest priority wins; equal priority HTP per slot; at most LJ_DMXIN_SOURCES */
    di_pkt(q, "B", 7, 150, 0, 0, 2); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 2, t) == 0 && f->raw[0] == 2 && f->nsources == 2);
    di_pkt(q, "C", 7, 150, 0, 0, 3); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 3, t) == 0 && f->raw[0] == 3);
    di_pkt(q, "D", 7, 150, 0, 0, 1); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 4, t) == 0 && f->raw[0] == 3 && f->nsources == 4);
    di_pkt(q, "E", 7, 200, 0, 0, 0); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 5, t) != 0 && f->exceeded == 1);
    /* Stream_Terminated ends one source at once */
    di_pkt(q, "C", 7, 150, 1, 0x40, 3); DCHECK(lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 3, t) == 0 && f->nsources == 3 && f->raw[0] == 2);
    /* time-out 2.5 s: the rest go, the loss rule holds the last frame */
    lj_dmxin_poll(t + 2400000u); DCHECK(f->nsources == 3);
    lj_dmxin_poll(t + 2600000u);
    DCHECK(f->nsources == 0 && f->live == 0 && f->raw[0] == 2 && f->channels == 0);
    /* the zeros rule */
    di_open(7, LJ_DMXIN_LOSS_ZERO);
    di_pkt(q, "A", 7, 100, 0, 0, 77); lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t);
    DCHECK(f->raw[100] == 77); lj_dmxin_poll(t + 2600000u); DCHECK(f->raw[100] == 0 && f->out[100] == 0);
    /* refresh rate over a whole second */
    di_open(7, 0);
    lj_dmxin_poll(t);
    for (k = 0; k < 30; k++) { di_pkt(q, "A", 7, 100, (uint8_t)k, 0, 1); lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t + (uint64_t)k * 33000u); }
    lj_dmxin_poll(t + 1000000u);
    DCHECK(f->hz == 30 && f->channels == 512);
    /* the per-channel table from raw to out, and Fingers emulation through the same input */
    di_open(7, 0);
    { uint8_t m[256]; for (k = 0; k < 256; k++) m[k] = (uint8_t)(k < 128 ? 0 : 255); lj_dmxin_set_table(3, m); }
    di_pkt(q, "A", 7, 100, 0, 0, 200); lj_dmxin_rx(LJ_DMXIN_SACN, q, sizeof q, 1, t);
    DCHECK(f->raw[2] == 200 && f->out[2] == 255 && f->out[3] == 200);
    lj_dmxin_emulate(1, 250, t); DCHECK(f->raw[0] == 250);
    lj_dmxin_emulate(3, 10, t); DCHECK(f->out[2] == 255);   /* HTP with the source, then the table */
    di_open(7, 0); lj_dmxin_emulate(48, 100, t); lj_dmxin_emulate(49, 100, t);
    DCHECK(f->live && f->raw[47] == 100 && f->raw[48] == 0 && f->channels == LJ_DMXIN_EMULATE);
    lj_dmxin_emulate(48, -1, t); lj_dmxin_poll(t); DCHECK(!f->live);
    /* the posted queue (the Art-Net path), bounded */
    di_open(7, 0);
    for (k = 0; k < LJ_DMXIN_QUEUE + 3; k++) lj_dmxin_post(LJ_DMXIN_SACN, p, sizeof p, 1, t);
    lj_dmxin_post(LJ_DMXIN_SACN, p, LJ_DMXIN_PKT_MAX + 1, 1, t);
    DCHECK(lj_dmxin_poll(t) == 1 && f->raw[0] == 9);
    /* the configuration */
    {
        lj_dmxin_cfg c, d; uint16_t outs[LJ_LINKS] = { 1, 2, 3, 4 };
        lj_ini *ini = lj_ini_new();
        lj_dmxin_cfg_default(&c);
        DCHECK(!c.enabled && c.source == LJ_DMXIN_SACN && c.universe == 1);   /* W5-X-OFF */
        DCHECK(lj_dmxin_cfg_validate(&c, outs) != NULL);   /* universe 1 is an output universe */
        c.universe = 10; DCHECK(lj_dmxin_cfg_validate(&c, outs) == NULL);
        c.universe = 64000; DCHECK(lj_dmxin_cfg_validate(&c, outs) != NULL);
        c.universe = 10; c.enabled = 1; c.nic_ipv4 = 0x7F000001u; c.loss = LJ_DMXIN_LOSS_ZERO; c.functions_off = 1;
        lj_dmxin_cfg_save(ini, &c);
        lj_dmxin_cfg_load(ini, &d); DCHECK(memcmp(&c, &d, sizeof c) == 0);
        lj_ini_sets(ini, "DMXIN", "Universe", "99999999999"); lj_ini_sets(ini, "DMXIN", "Nic", "1.2.3.999");
        lj_dmxin_cfg_load(ini, &d); DCHECK(d.universe == 1 && d.nic_ipv4 == 0);
        lj_ini_free(ini);
    }
}

static void di_artnet(void)   /* W5-ART-IN: an ArtDmx for the input Port-Address feeds the same seam in one call */
{
    static uint8_t a[LJ_ARTNET_DMX_LEN], slots[LJ_LINK_CH];
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    lj_dmxin_cfg c;
    memset(slots, 42, sizeof slots);
    DCHECK(lj_artnet_pack_dmx(a, 1, 0, 0x0102, slots) == LJ_ARTNET_DMX_LEN);   /* ME-186: a broken builder fails the row, never passes it */
    lj_dmxin_reset(); lj_dmxin_cfg_default(&c); c.source = LJ_DMXIN_ARTNET; c.universe = 0x0102; lj_dmxin_open(&c, NULL);
    lj_dmxin_post(LJ_DMXIN_ARTNET, a, sizeof a, 0x7F000003u, 1);
    DCHECK(lj_dmxin_poll(1) == 1 && f->nsources == 1 && f->raw[511] == 42 && f->src[0].priority == 100);
    a[14] = 3;   /* Port-Address 0x0103: not ours */
    DCHECK(lj_dmxin_rx(LJ_DMXIN_ARTNET, a, sizeof a, 0x7F000003u, 2) != 0);
}

static void di_loopback(void)   /* W5-DMXIN-1: unicast to 5568 on loopback, values in the frame, our own packets ignored */
{
    static uint8_t p[LJ_SACN_DATA_LEN];
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    lj_dmxin_cfg c;
    lj_socket s;
    uint64_t t0;
    lj_net_startup();
    lj_dmxin_reset(); lj_dmxin_cfg_default(&c); c.enabled = 1; c.universe = 10; c.nic_ipv4 = LJ_IP_LOOPBACK;
    if (lj_dmxin_open(&c, di_own) != 0) { printf("SKIP loopback: 5568 not bindable here\n"); return; }
    s = lj_socket_udp(LJ_IP_LOOPBACK, 0, 0);
    DCHECK(s != LJ_SOCKET_BAD);
    di_pkt(p, "O", 10, 100, 0, 0, 99); lj_socket_sendto(s, p, sizeof p, LJ_IP_LOOPBACK, LJ_SACN_PORT);
    di_pkt(p, "L", 10, 100, 0, 0, 33); lj_socket_sendto(s, p, sizeof p, LJ_IP_LOOPBACK, LJ_SACN_PORT);
    t0 = lj_clock_us();
    while (f->nsources == 0 && lj_clock_us() - t0 < 2000000u) lj_dmxin_poll(lj_clock_us());
    DCHECK(f->nsources == 1 && f->raw[0] == 33 && f->src[0].cid[0] == 'L');
    lj_socket_close(s);
    lj_dmxin_close();
}

int lj_test_dmxin_loopback(int argc, char **argv)   /* its own CTest row: it binds 5568 (RESOURCE_LOCK sacn_port) */
{
    (void)argc; (void)argv;
    di_fails = 0;
    di_loopback();
    lj_dmxin_reset();
    printf("dmxin_loopback: %s\n", di_fails ? "FAIL" : "PASS");
    return di_fails ? 1 : 0;
}

int lj_test_dmxin_rules(int argc, char **argv)
{
    (void)argc; (void)argv;
    di_fails = 0;
    di_rules(); di_artnet();
    lj_dmxin_reset();
    printf("dmxin_rules: %s\n", di_fails ? "FAIL" : "PASS");
    return di_fails ? 1 : 0;
}

/* ---- .vtr and DmxIn.dat (W5-DMXIN-6, -7) ---- */
static int di_vtr_bad(const char *text, const char *why)
{
    lj_vtr t;
    char err[128];
    if (lj_vtr_parse(text, strlen(text), &t, err, sizeof err) == 0) { printf("FAIL vtr accepted: %s\n", why); return 1; }
    return 0;
}

int lj_test_dmxin_vtr(int argc, char **argv)
{
    static const char hdr[] = "// LightJockey DMX value translation file\r\nID={00000000-0000-4000-8000-000000000001}\r\nTYPE=USER\r\nNAME=Test\r\n";
    static char big[LJ_VTR_FILE_MAX + 64], text[1024];
    static lj_dmxin_map m, m2;
    char err[128], name[256], path[512];
    const char *dir = argc > 1 ? argv[1] : ".";
    lj_vtr t, u;
    int i;
    di_fails = 0;
    /* parse: header, keys, entries, missing = 1:1 */
    if (snprintf(text, sizeof text, "%sPURPOSE=p\r\n0=10\r\n255=0\r\n", hdr) < 0) text[0] = 0;
    DCHECK(lj_vtr_parse(text, strlen(text), &t, err, sizeof err) == 0 && t.map[0] == 10 && t.map[1] == 1 && t.map[255] == 0 && strcmp(t.name, "Test") == 0 && strcmp(t.purpose, "p") == 0);
    /* the limits */
    { if (snprintf(text, sizeof text, "%s5=1\r\n5=2\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "duplicate n");
    { if (snprintf(text, sizeof text, "%s256=1\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "n 256");
    { if (snprintf(text, sizeof text, "%s1=256\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "m 256");
    { if (snprintf(text, sizeof text, "%s99999999999999999999=1\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "ERANGE");
    { if (snprintf(text, sizeof text, "%s-1=1\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "negative");
    { if (snprintf(text, sizeof text, "%s1=+1\r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "sign");
    { if (snprintf(text, sizeof text, "%s1=1 \r\n", hdr) < 0) text[0] = 0; } di_fails += di_vtr_bad(text, "trailing blank");
    di_fails += di_vtr_bad("LightJockey value table\r\nID={00000000-0000-4000-8000-000000000001}\r\nTYPE=A\r\nNAME=B\r\n", "header");
    di_fails += di_vtr_bad("// LightJockey DMX value translation file\r\nID=1234\r\nTYPE=A\r\nNAME=B\r\n", "ID not a GUID");
    di_fails += di_vtr_bad("// LightJockey DMX value translation file\r\nID={00000000-0000-4000-8000-000000000001}\r\nNAME=B\r\n", "no TYPE");
    di_fails += di_vtr_bad("", "empty");
    memset(big, 'A', sizeof big); memcpy(big, hdr, sizeof hdr - 1); big[sizeof big - 1] = 0;
    di_fails += di_vtr_bad(big, "file too large");
    { if (snprintf(text, sizeof text, "%sNAME=", hdr) < 0) text[0] = 0; } { size_t l = strlen(text); memset(text + l, 'n', 300); text[l + 300] = 0; } di_fails += di_vtr_bad(text, "line too long");
    { if (snprintf(text, sizeof text, "%sNAME=", hdr) < 0) text[0] = 0; } { size_t l = strlen(text); memset(text + l, 'n', 100); text[l + 100] = 0; } di_fails += di_vtr_bad(text, "NAME longer than 63");
    /* file names (LJ2's spelling) */
    lj_vtr_identity(&u); lj_str_copy(u.type, sizeof u.type, "SYSTEM"); lj_str_copy(u.name, sizeof u.name, "Off/on, disable treshold"); lj_str_copy(u.id, sizeof u.id, "{FC78008E-C9B8-4A92-B204-AF8D693CC6A6}");
    DCHECK(lj_vtr_file_name(name, sizeof name, &u) == 0 && strcmp(name, "SYSTEM_Off_on__disable_treshold_{FC78008E-C9B8-4A92-B204-AF8D693CC6A6}.vtr") == 0);
    lj_str_copy(u.name, sizeof u.name, "a:b"); DCHECK(lj_vtr_file_name(name, sizeof name, &u) != 0);
    lj_str_copy(u.name, sizeof u.name, "Strobe (regular)"); lj_str_copy(u.type, sizeof u.type, "USER");
    /* the linear-range helper (integer) */
    lj_vtr_linear(&u, 0, 255, 50, 72); DCHECK(u.map[0] == 50 && u.map[255] == 72 && u.map[128] == 61);
    lj_vtr_linear(&u, 10, 10, 200, 0); DCHECK(u.map[10] == 200);
    lj_vtr_linear(&u, 255, 0, 0, 255); DCHECK(u.map[0] == 255 && u.map[255] == 0);
    /* round trip through a file */
    DCHECK(lj_vtr_file_name(name, sizeof name, &u) == 0);
    if (snprintf(path, sizeof path, "%s/%s", dir, name) < 0) path[0] = 0;
    DCHECK(lj_vtr_save(path, &u) == 0 && lj_vtr_load(path, &t, err, sizeof err) == 0 && memcmp(t.map, u.map, 256) == 0 && strcmp(t.name, u.name) == 0 && strcmp(t.id, u.id) == 0 && strcmp(t.type, u.type) == 0);
    remove(path);
    /* the SYSTEM tables (regenerated, io/lj_vtr_gen.c) */
    DCHECK(lj_vtr_stock(0, &t) == 0 && strcmp(t.name, "Linear") == 0 && t.map[77] == 77 && strcmp(t.type, "SYSTEM") == 0);
    DCHECK(lj_vtr_stock(3, &t) == 0 && t.map[127] == 0 && t.map[128] == 255);
    DCHECK(lj_vtr_stock(2, &t) == 0 && t.map[5] == 0 && t.map[6] == 1 && t.map[255] == 255);
    DCHECK(lj_vtr_stock(LJ_VTR_STOCK, &t) != 0 && lj_vtr_stock(-1, &t) != 0);
    for (i = 0; i < LJ_VTR_STOCK; i++) DCHECK(lj_vtr_stock(i, &t) == 0 && lj_vtr_file_name(name, sizeof name, &t) == 0);
    /* DmxIn.dat round trip with a copied table */
    lj_dmxin_map_init(&m);
    m.time_mode = LJ_DMXIN_TIME_CHANGE2; m.cue0_clears = 1;
    m.ch[0].fn = LJ_DIF_HTP_GROUP; m.ch[0].num = 3;
    m.ch[511].fn = LJ_DIF_FLASH_SEQ; m.ch[511].num = 14;
    m.ch[4].has_table = 1; lj_vtr_stock(5, &m.ch[4].table);
    if (snprintf(path, sizeof path, "%s/DmxIn_test.dat", dir) < 0) path[0] = 0;
    DCHECK(lj_dmxin_map_save(path, &m) == 0 && lj_dmxin_map_load(path, &m2, err, sizeof err) == 0);
    DCHECK(memcmp(&m, &m2, sizeof m) == 0);
    { const lj_dmxin_frame *f = lj_dmxin_frame_get(); lj_dmxin_reset(); lj_dmxin_map_load(path, &m2, err, sizeof err); lj_dmxin_emulate(5, 128, 0); DCHECK(f->out[4] == m.ch[4].table.map[128]); }
    { FILE *fp = fopen(path, "ab"); if (fp) { fputs("ch=513 htp_group 1\n", fp); fclose(fp); } }
    DCHECK(lj_dmxin_map_load(path, &m2, err, sizeof err) != 0 && m2.ch[0].fn == LJ_DIF_NONE);   /* a bad channel refuses the file, the map stays empty */
    remove(path);
    if (snprintf(path, sizeof path, "%s/DmxIn_missing.dat", dir) < 0) path[0] = 0;
    DCHECK(lj_dmxin_map_load(path, &m2, err, sizeof err) == 0 && m2.ch[0].fn == LJ_DIF_NONE);
    {
        lj_dmxin_assign a; char cap[64];
        memset(&a, 0, sizeof a); a.fn = LJ_DIF_HTP_GROUP; a.num = 3; DCHECK(lj_dmxin_fn_caption(&a, cap, sizeof cap) == 0 && strcmp(cap, "HTP Group 3") == 0);
        a.fn = LJ_DIF_FLASH_SEQ; a.num = 14; DCHECK(lj_dmxin_fn_caption(&a, cap, sizeof cap) == 0 && strcmp(cap, "Flash Seq 0014") == 0);
    }
    lj_dmxin_reset();
    printf("dmxin_vtr: %s\n", di_fails ? "FAIL" : "PASS");
    return di_fails ? 1 : 0;
}

/* ---- the functions (W5-DMXIN-5): synthetic frames, every call recorded ---- */
static struct { int n; enum lj_fn fn[64]; int edge[64]; uint32_t prog[64]; } di_log;
static void di_rec(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)ud; if (di_log.n < 64) { di_log.fn[di_log.n] = fn; di_log.edge[di_log.n] = edge; di_log.prog[di_log.n] = prog; di_log.n++; } }
static int di_has(enum lj_fn fn, uint32_t prog) { int i; for (i = 0; i < di_log.n; i++) if (di_log.fn[i] == fn && di_log.prog[i] == prog && di_log.edge[i]) return 1; return 0; }
static int di_count(enum lj_fn fn) { int i, n = 0; for (i = 0; i < di_log.n; i++) n += di_log.fn[i] == fn && di_log.edge[i]; return n; }
static void di_frame(const lj_dmxin_map *m, uint8_t *in, int ch, int v, int off) { in[ch - 1] = (uint8_t)v; di_log.n = 0; lj_dmxin_functions(m, in, off); }

int lj_test_dmxin_functions(int argc, char **argv)
{
    static uint8_t in[LJ_LINK_CH];
    static lj_dmxin_map mm;
    lj_dmxin_map *m = &mm;
    enum lj_fn f;
    uint32_t cue = 0;
    (void)argc; (void)argv;
    di_fails = 0;
    for (f = (enum lj_fn)1; f < LJ_FN_COUNT; f = (enum lj_fn)(f + 1)) lj_fn_bind(f, di_rec, NULL);
    lj_dmxin_functions_reset(); memset(in, 0, sizeof in); lj_dmxin_map_init(m);
    /* binary cue + Latch Cue: nothing loads before the latch; cue 0 clears with the preference */
    m->ch[0].fn = LJ_DIF_CUE_BIT_1; m->ch[1].fn = LJ_DIF_CUE_BIT_4; m->ch[2].fn = LJ_DIF_CUE_BIT_512; m->ch[3].fn = LJ_DIF_LATCH_CUE; m->cue0_clears = 1;
    di_frame(m, in, 1, 0, 0);
    di_frame(m, in, 1, 200, 0); di_frame(m, in, 2, 128, 0); DCHECK(di_log.n == 0 && !lj_dmxin_remote_cue(&cue));
    di_frame(m, in, 4, 255, 0); DCHECK(di_has(LJ_FN_PROG_CUE, 5) && lj_dmxin_remote_cue(&cue) && cue == 5);
    di_frame(m, in, 3, 255, 0); DCHECK(di_has(LJ_FN_PROG_CUE, 517));
    di_frame(m, in, 3, 127, 0); DCHECK(di_has(LJ_FN_PROG_CUE, 5));
    di_frame(m, in, 1, 0, 0); di_frame(m, in, 2, 0, 0); DCHECK(di_count(LJ_FN_NEW_CLEAR_CUE) == 1);
    di_frame(m, in, 4, 0, 0); DCHECK(!lj_dmxin_remote_cue(&cue));
    /* rising edges 127 -> 128 only */
    lj_dmxin_map_init(m); lj_dmxin_functions_reset(); memset(in, 0, sizeof in);
    m->ch[9].fn = LJ_DIF_CL_GO; m->ch[10].fn = LJ_DIF_NEXT_CUE_PAGE; m->ch[11].fn = LJ_DIF_EMULATE_AUDIO_TRIG; m->ch[12].fn = LJ_DIF_TRIG_ALL_MANUAL_DEFAULT;
    di_frame(m, in, 10, 127, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 0);
    di_frame(m, in, 10, 128, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 1);
    di_frame(m, in, 10, 255, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 0);
    di_frame(m, in, 11, 200, 0); DCHECK(di_count(LJ_FN_NEXT_CUE_PAGE) == 1);
    di_frame(m, in, 12, 200, 0); DCHECK(di_count(LJ_FN_EMULATE_AUDIO_TRIG) == 1);
    di_frame(m, in, 13, 200, 0); DCHECK(di_count(LJ_FN_TRIG_ALL_MANUAL_DEFAULT) == 1);
    /* Cuelist Go (Alt): the three transitions */
    lj_dmxin_map_init(m); lj_dmxin_functions_reset(); memset(in, 0, sizeof in);
    m->ch[0].fn = LJ_DIF_CL_GO_ALT;
    di_frame(m, in, 1, 0, 0);
    di_frame(m, in, 1, 150, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 1);   /* 0-127 -> 128-255 */
    di_frame(m, in, 1, 220, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 1);   /* 128-191 -> 192-255 */
    di_frame(m, in, 1, 160, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 1);   /* 192-255 -> 128-191 */
    di_frame(m, in, 1, 170, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 0);
    di_frame(m, in, 1, 100, 0); DCHECK(di_count(LJ_FN_CUELIST_GO) == 0);
    /* statics >= 50 %, flash > 127 on / < 128 off, levels on change */
    lj_dmxin_map_init(m); lj_dmxin_functions_reset(); memset(in, 0, sizeof in);
    m->ch[0].fn = LJ_DIF_STATIC_ACTIVATE; m->ch[0].num = 7; m->ch[1].fn = LJ_DIF_FLASH_SEQ; m->ch[1].num = 14; m->ch[2].fn = LJ_DIF_HTP_GROUP; m->ch[2].num = 3;
    m->ch[3].fn = LJ_DIF_MASTER_INTENSITY; m->ch[4].fn = LJ_DIF_BLACKOUT; m->ch[5].fn = LJ_DIF_MACRO_AMP_INV;
    di_frame(m, in, 1, 0, 0);
    di_frame(m, in, 1, 127, 0); DCHECK(di_count(LJ_FN_DMXIN_STATIC) == 0);
    di_frame(m, in, 1, 128, 0); DCHECK(di_has(LJ_FN_DMXIN_STATIC, 7u << 8 | 1u));
    di_frame(m, in, 2, 128, 0); DCHECK(di_log.n == 1 && di_log.fn[0] == LJ_FN_PROG_SEQ_FLASH && di_log.prog[0] == 14 && di_log.edge[0] == 1);
    di_frame(m, in, 2, 127, 0); DCHECK(di_log.n == 1 && di_log.fn[0] == LJ_FN_PROG_SEQ_FLASH && di_log.edge[0] == 0);
    di_frame(m, in, 3, 99, 0); DCHECK(di_has(LJ_FN_DMXIN_HTP, 3u << 8 | 99u));
    di_frame(m, in, 4, 200, 0); DCHECK(di_has(LJ_FN_DMXIN_MASTER, 200u));
    di_frame(m, in, 5, 200, 0); DCHECK(di_has(LJ_FN_DMXIN_BLACKOUT, 1u));
    di_frame(m, in, 6, 55, 0); DCHECK(di_has(LJ_FN_DMXIN_MACRO, 200u));
    /* Overwrite Enable: follows its channel while assigned, back to 1 when unassigned */
    lj_dmxin_map_init(m); lj_dmxin_functions_reset(); memset(in, 0, sizeof in);
    m->ch[7].fn = LJ_DIF_OVERWRITE_ENABLE;
    di_frame(m, in, 8, 0, 0); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 0u));
    di_frame(m, in, 8, 200, 0); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 1u));
    di_frame(m, in, 8, 0, 0); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 0u));
    m->ch[7].fn = LJ_DIF_NONE; di_frame(m, in, 8, 0, 0); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 1u));
    /* functions off: nothing runs, the overwrite gate opens (the Global Patch overrides keep working) */
    m->ch[7].fn = LJ_DIF_OVERWRITE_ENABLE; m->ch[9].fn = LJ_DIF_CL_GO;
    di_frame(m, in, 8, 0, 0);
    di_frame(m, in, 10, 255, 1); DCHECK(di_count(LJ_FN_CUELIST_GO) == 0 && di_has(LJ_FN_DMXIN_OVERWRITE, 1u));
    /* Cue Time Control: on a change of 2 steps */
    lj_dmxin_map_init(m); lj_dmxin_functions_reset(); memset(in, 0, sizeof in);
    m->ch[0].fn = LJ_DIF_CUE_TIME; m->time_mode = LJ_DMXIN_TIME_CHANGE2;
    di_frame(m, in, 1, 100, 0); DCHECK(di_count(LJ_FN_DMXIN_TIME) == 1);
    di_frame(m, in, 1, 101, 0); DCHECK(di_count(LJ_FN_DMXIN_TIME) == 0);
    di_frame(m, in, 1, 102, 0); DCHECK(di_has(LJ_FN_DMXIN_TIME, 102u));
    m->time_mode = LJ_DMXIN_TIME_ALWAYS; di_frame(m, in, 1, 103, 0); DCHECK(di_count(LJ_FN_DMXIN_TIME) == 1);
    /* ME-167: the functions run only on a frame someone sent (the UI pass's lj_dmxin_functions_step). Master Intensity on
     * ch 14, Overwrite Enable on ch 20 */
    {
        static uint8_t a[LJ_ARTNET_DMX_LEN], sl[LJ_LINK_CH];
        lj_dmxin_cfg cf;
        lj_dmxin_map_init(m); m->ch[13].fn = LJ_DIF_MASTER_INTENSITY; m->ch[19].fn = LJ_DIF_OVERWRITE_ENABLE;
        lj_dmxin_reset(); lj_dmxin_cfg_default(&cf); lj_dmxin_open(&cf, NULL); di_log.n = 0;
        lj_dmxin_functions_step(m, lj_dmxin_poll(1000), 0); lj_dmxin_functions_step(m, lj_dmxin_poll(2000), 0);
        DCHECK(di_log.n == 0);                                                     /* DMX-in off, no packet: the GM keeps its level (was 0: black stage) */
        cf.enabled = 1; cf.source = LJ_DMXIN_ARTNET; cf.universe = 0x0102; lj_dmxin_open(&cf, NULL);
        lj_dmxin_functions_step(m, lj_dmxin_poll(3000), 0); DCHECK(di_log.n == 0);   /* on, nothing received yet */
        memset(sl, 0, sizeof sl); sl[13] = 200;                                    /* GM 200, the Overwrite Enable channel at 0 */
        DCHECK(lj_artnet_pack_dmx(a, 1, 0, 0x0102, sl) == LJ_ARTNET_DMX_LEN);
        lj_dmxin_post(LJ_DMXIN_ARTNET, a, sizeof a, 0x7F000003u, 4000);
        lj_dmxin_functions_step(m, lj_dmxin_poll(4000), 0);
        DCHECK(di_has(LJ_FN_DMXIN_MASTER, 200u) && di_has(LJ_FN_DMXIN_OVERWRITE, 0u));
        /* disable / enable (the apply posts the gate open): the reopened input re-primes, so the same frame closes it again */
        lj_dmxin_open(&cf, NULL); di_log.n = 0;
        lj_dmxin_post(LJ_DMXIN_ARTNET, a, sizeof a, 0x7F000003u, 5000);
        lj_dmxin_functions_step(m, lj_dmxin_poll(5000), 0);
        DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 0u));
        /* Disable DMX-in functions on a static frame opens the gate at once; enabling them closes it again */
        di_log.n = 0; lj_dmxin_functions_step(m, lj_dmxin_poll(6000), 1); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 1u));
        di_log.n = 0; lj_dmxin_functions_step(m, lj_dmxin_poll(7000), 0); DCHECK(di_has(LJ_FN_DMXIN_OVERWRITE, 0u));
        di_log.n = 0; lj_dmxin_functions_step(m, lj_dmxin_poll(8000), 0); DCHECK(di_log.n == 0);   /* static, unchanged: nothing */
        /* ME-177: with a Nic set, a datagram from off its subnet changes nothing (the input binds every interface) */
        cf.nic_ipv4 = 0x7F000001u; lj_dmxin_open(&cf, NULL); di_log.n = 0;
        lj_dmxin_post(LJ_DMXIN_ARTNET, a, sizeof a, 0xCB007109u, 9000);           /* 203.0.113.9 (TEST-NET-3) */
        lj_dmxin_functions_step(m, lj_dmxin_poll(9000), 0);
        DCHECK(di_log.n == 0 && !lj_dmxin_frame_get()->live && lj_dmxin_frame_get()->discarded == 1);
        lj_dmxin_post(LJ_DMXIN_ARTNET, a, sizeof a, 0x7F000003u, 10000);          /* on the Nic's subnet: taken */
        lj_dmxin_functions_step(m, lj_dmxin_poll(10000), 0);
        DCHECK(lj_dmxin_frame_get()->live && di_has(LJ_FN_DMXIN_MASTER, 200u));
        lj_dmxin_close(); lj_dmxin_reset();
    }
    /* ME-172: DmxIn.dat numbers outside the function's own range are refused (htp_group 16777219 aliased group 3) */
    {
        static const char *const bad[] = { "ch=12 htp_group 16777219", "ch=12 htp_group 25", "ch=12 htp_group 0", "ch=3 static_activate 21",
                                           "ch=3 seq_onoff 13", "ch=14 master_intensity 1", "ch=5 flash_seq 0", "ch=5 htp_group 3x" };
        char err[128];
        size_t k;
        FILE *fp;
        for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
            if ((fp = fopen("test_dmxin_alias.dat", "wb")) != NULL) { fprintf(fp, "LJ4 dmxin 1\n%s\n", bad[k]); fclose(fp); }
            if (lj_dmxin_map_load("test_dmxin_alias.dat", m, err, sizeof err) != -1) { DCHECK(0); printf("  accepted: %s\n", bad[k]); }
        }
        if ((fp = fopen("test_dmxin_alias.dat", "wb")) != NULL) { fputs("LJ4 dmxin 1\nch=12 htp_group 24\nch=14 master_intensity 0\nch=5 flash_seq 99999999\n", fp); fclose(fp); }
        DCHECK(lj_dmxin_map_load("test_dmxin_alias.dat", m, err, sizeof err) == 0 && m->ch[11].num == 24 && m->ch[4].num == 99999999u);
        remove("test_dmxin_alias.dat");
    }
    lj_fn_unbind_all(); lj_dmxin_functions_reset();
    printf("dmxin_functions: %s\n", di_fails ? "FAIL" : "PASS");
    return di_fails ? 1 : 0;
}
