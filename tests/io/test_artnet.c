/* test_artnet.c - the artnet track's rows (docs/w5-contracts.md seam 1). The contract seeds the fuzz row (W5-X-HOSTILE):
 *   fuzz_artnet  an ArtDmx from lj_artnet_pack_dmx, an ArtPoll from lj_artnet_pack_poll and a synthetic ArtPollReply
 *                (note 08 s9.4 offsets) must parse, then LJ_FUZZ_ITERS mutations of each through the readers,
 *                lj_artnet_feed and lj_dmxin_rx(LJ_DMXIN_ARTNET)
 * It prints PENDING artnet and fails until the track lands the builders and readers (DISABLED in CMakeLists.txt until
 * then). The track appends its W5-ART-1..6 rows to this file. */
#include <stdio.h>
#include <string.h>
#include "io/lj_artnet.h"
#include "io/lj_dmxin.h"
#include "lj_fuzz.h"
/* artnet track (appended) */
#include <stdlib.h>
#include "vectors.h"
#include "core/lj_clock.h"
#include "core/lj_log.h"
#include "io/lj_socket.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_spinedit.h"
#include "windows/lj_hardware_setup.h"
#include "windows/lj_warnings.h"

static size_t reply_seed(uint8_t *p)   /* 239 bytes: one output port on Port-Address 0x0102 (Net 1, Sub-Net 0, Universe 2) */
{
    memset(p, 0, LJ_ARTNET_REPLY_LEN);
    memcpy(p, "Art-Net", 8);
    p[8] = 0x00; p[9] = 0x21;                      /* OpPollReply 0x2100, little endian */
    p[10] = 127; p[11] = 0; p[12] = 0; p[13] = 2;  /* IP 127.0.0.2, high byte first */
    p[14] = 0x36; p[15] = 0x19;                    /* Port 0x1936 */
    p[18] = 1; p[19] = 0;                          /* NetSwitch, SubSwitch */
    memcpy(p + 26, "fuzz node", 9);                /* PortName[18] */
    memset(p + 44, 'L', 64);                       /* LongName[64], deliberately unterminated (W5-X-TEXT) */
    p[173] = 1;                                    /* NumPorts lo */
    p[174] = 0x80;                                 /* PortTypes[0]: output */
    p[190] = 2;                                    /* SwOut[0] */
    return LJ_ARTNET_REPLY_LEN;
}

int lj_test_fuzz_artnet(int argc, char **argv)
{
    static uint8_t seeds[3][LJ_ARTNET_DMX_LEN + 16], buf[LJ_DMXIN_PKT_MAX + 1], slots[LJ_LINK_CH];
    size_t len[3], n;
    lj_artnet_node node;
    uint16_t pa = 0; uint8_t seq = 0; int ns = 0;
    uint32_t s = 0xA27u;
    int i, k, j, bad = 0, pl;
    (void)argc; (void)argv;
    for (i = 0; i < LJ_LINK_CH; i++) slots[i] = (uint8_t)(255 - i);
    pl = lj_artnet_pack_poll(seeds[1], sizeof seeds[1]);
    len[0] = lj_artnet_pack_dmx(seeds[0], 1, 0, 0x0102, slots) == LJ_ARTNET_DMX_LEN ? LJ_ARTNET_DMX_LEN : 0;
    len[1] = pl > 0 ? (size_t)pl : 0;
    len[2] = reply_seed(seeds[2]);
    if (!len[0] || !len[1] || lj_artnet_parse_dmx(seeds[0], len[0], &pa, &seq, slots, &ns) != 0 || pa != 0x0102 || ns != LJ_LINK_CH
        || lj_artnet_opcode(seeds[1], len[1]) != LJ_ARTNET_OP_POLL || lj_artnet_parse_reply(seeds[2], len[2], &node) != 0 || node.nports != 1 || node.port_addr[0] != 0x0102) {
        printf("PENDING artnet: the ArtDmx / ArtPoll / ArtPollReply builders and readers are not in yet\n");
        return 1;
    }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        j = i % 3;
        memcpy(buf, seeds[j], len[j]); n = len[j];
        for (k = (int)(lj_fuzz_next(&s) % 8); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        if (lj_artnet_parse_dmx(buf, n, &pa, &seq, slots, &ns) == 0 && (ns < 2 || ns > LJ_LINK_CH || (ns & 1) || (size_t)ns + LJ_ARTNET_DMX_HDR > n)) { if (!bad++) printf("FAIL iteration %d: ArtDmx length accepted out of range\n", i); }
        if (lj_artnet_parse_reply(buf, n, &node) == 0 && (node.nports > LJ_ARTNET_NODE_PORTS || node.short_name[17] != 0 || node.long_name[63] != 0)) { if (!bad++) printf("FAIL iteration %d: reply fields out of bounds\n", i); }
        lj_artnet_feed(buf, n, 0x7F000002u, (uint64_t)i * 1000u);
        lj_dmxin_rx(LJ_DMXIN_ARTNET, buf, n, 0x7F000002u, (uint64_t)i * 1000u);
    }
    printf("fuzz_artnet: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}

/* ---- artnet track rows (W5-ART-1..6; `lj4evr_tests test_artnet`) ------------------------------------------------------
 *   W5-ART-1  the builders against tools/gen_vectors.py's independent struct.pack packets (tests/vectors.h)
 *   W5-ART-6  the readers: ID, OpCode, ProtVer >= 14, Length even 2..512 and <= bytes present, NumPorts clamp, names bounded
 *   W5-ART-4  Hardware.Ini [ARTNET] defaults, round trip, validation; the Hardware Setup rows: link 2 -> Art-Net, applied,
 *             saved, reopened, read back; Port-Address 0 refused; the status line
 *   W5-ART-2  a node's reply fills the table, our own reply is ignored, it expires after LJ_ARTNET_EXPIRE_POLLS polls
 *   W5-ART-3  400 changing pumps = 400 ArtDmx per subscriber, keep-alives every 900 ms, 0 with no subscriber, the 44 Hz cap
 *   W5-ART-5  6454 pre-bound by the test: lj_artnet_open fails (bound 0) and lj_sacn_open still succeeds
 *   wire      loopback: a real ArtPollReply over UDP subscribes 127.0.0.1, the ArtDmx it receives is byte-exact (127.0.0.2
 *             as our NIC; SKIP where 127.0.0.2 cannot be bound, e.g. macOS) */
static int art_fails;
#define ACHECK(cond) do { if (!(cond)) { art_fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
static void art_quiet(void *ud, const char *line) { (void)ud; (void)line; }

static size_t art_reply(uint8_t *p, uint8_t net, uint8_t sub, uint8_t swout, uint8_t bind)   /* one output port on net:sub:swout */
{
    reply_seed(p);
    p[18] = net; p[19] = sub; p[190] = swout; p[211] = bind;
    return LJ_ARTNET_REPLY_LEN;
}

static void art_builders_readers(void)
{
    static uint8_t pkt[LJ_ARTNET_DMX_LEN + 8], slots[LJ_LINK_CH], got[LJ_LINK_CH];
    lj_artnet_node nd;
    uint16_t pa = 0; uint8_t seq = 0; int ns = 0, i;
    for (i = 0; i < LJ_LINK_CH; i++) slots[i] = (uint8_t)i;
    ACHECK(lj_artnet_pack_dmx(pkt, 1, 0, 0x0102, slots) == LJ_ARTNET_DMX_LEN && memcmp(pkt, lj_vec_artnet_dmx_a, sizeof lj_vec_artnet_dmx_a) == 0);
    for (i = 0; i < LJ_LINK_CH; i++) slots[i] = (uint8_t)(255 - i);
    ACHECK(lj_artnet_pack_dmx(pkt, 255, 3, 32767, slots) == LJ_ARTNET_DMX_LEN && memcmp(pkt, lj_vec_artnet_dmx_b, sizeof lj_vec_artnet_dmx_b) == 0);
    ACHECK(lj_artnet_pack_poll(pkt, sizeof pkt) == (int)sizeof lj_vec_artnet_poll && memcmp(pkt, lj_vec_artnet_poll, sizeof lj_vec_artnet_poll) == 0);
    ACHECK(lj_artnet_pack_poll(pkt, 21) == -1);
    ACHECK(lj_artnet_pack_dmx(pkt, 1, 0, 0, slots) == -1 && lj_artnet_pack_dmx(pkt, 1, 0, 32768, slots) == -1);   /* 0 deprecated, bit 15 reserved */
    /* readers */
    lj_artnet_pack_dmx(pkt, 7, 0, 0x7F12, slots);
    ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN, &pa, &seq, got, &ns) == 0 && pa == 0x7F12 && seq == 7 && ns == 512 && memcmp(got, slots, LJ_LINK_CH) == 0);
    ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN - 1, &pa, &seq, got, &ns) == -1);   /* Length 512 past the bytes present */
    pkt[16] = 0; pkt[17] = 2;
    ACHECK(lj_artnet_parse_dmx(pkt, 20, &pa, &seq, got, &ns) == 0 && ns == 2 && got[0] == 255 && got[1] == 254 && got[2] == 0);   /* short frame, the rest 0 */
    pkt[17] = 3;   ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN, NULL, NULL, NULL, NULL) == -1);   /* odd */
    pkt[17] = 0;   ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN, NULL, NULL, NULL, NULL) == -1);   /* 0 */
    pkt[16] = 2; pkt[17] = 2; ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN + 8, NULL, NULL, NULL, NULL) == -1);   /* 514 */
    pkt[16] = 2; pkt[17] = 0; pkt[11] = 13; ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN, NULL, NULL, NULL, NULL) == -1);   /* ProtVer 13 ignored */
    pkt[11] = 14; pkt[7] = 'x'; ACHECK(lj_artnet_parse_dmx(pkt, LJ_ARTNET_DMX_LEN, NULL, NULL, NULL, NULL) == -1 && lj_artnet_opcode(pkt, LJ_ARTNET_DMX_LEN) == -1);   /* ID */
    pkt[7] = 0; ACHECK(lj_artnet_parse_dmx(pkt, 17, NULL, NULL, NULL, NULL) == -1 && lj_artnet_opcode(pkt, 9) == -1);
    art_reply(pkt, 1, 0, 2, 0);
    ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_LEN, &nd) == 0 && nd.nports == 1 && nd.port_addr[0] == 0x0102 && strcmp(nd.short_name, "fuzz node") == 0 && strlen(nd.long_name) == 63);
    ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_MIN - 1, &nd) == -1 && lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_MIN, &nd) == 0 && nd.bind_index == 0);
    pkt[172] = 0x01; pkt[173] = 0x00; memset(pkt + 174, 0x80, 4);    /* NumPorts 256 -> 4 */
    ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_LEN, &nd) == 0 && nd.nports == 4);
    pkt[175] = 0x40;                                                  /* port 2 input only: not a subscriber */
    ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_LEN, &nd) == 0 && nd.nports == 3);
    memset(pkt + 26, 'S', 18);                                        /* PortName unterminated */
    ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_LEN, &nd) == 0 && strlen(nd.short_name) == 17);
    pkt[9] = 0x20; ACHECK(lj_artnet_parse_reply(pkt, LJ_ARTNET_REPLY_LEN, &nd) == -1);   /* OpPoll, not a reply */
}

static void art_cfg(void)
{
    lj_ini *ini = lj_ini_new();
    lj_sacn_cfg c;
    memset(&c, 0, sizeof c);
    lj_artnet_cfg_load(ini, &c);
    ACHECK(c.proto[0] == LJ_OUT_SACN && c.proto[3] == LJ_OUT_SACN && c.artnet_addr[0] == 1 && c.artnet_addr[3] == 4 && c.artnet_in == 0);   /* a v1.0.0 file: sACN everywhere */
    lj_ini_seti(ini, "ARTNET", "Proto2", 1); lj_ini_seti(ini, "ARTNET", "Addr2", 0); lj_ini_seti(ini, "ARTNET", "Proto3", 7); lj_ini_seti(ini, "ARTNET", "In", 40000);
    lj_artnet_cfg_load(ini, &c);
    ACHECK(c.proto[1] == LJ_OUT_ARTNET && c.artnet_addr[1] == 2 && c.proto[2] == LJ_OUT_SACN && c.artnet_in == 0);   /* hostile values fall back */
    c.artnet_addr[1] = 0x0203; c.artnet_in = 9;
    lj_artnet_cfg_save(ini, &c);
    ACHECK(lj_ini_geti(ini, "ARTNET", "Proto2", -1) == 1 && lj_ini_geti(ini, "ARTNET", "Addr2", -1) == 0x0203 && lj_ini_geti(ini, "ARTNET", "In", -1) == 9 && lj_ini_geti(ini, "ARTNET", "Proto1", -1) == 0);
    memset(&c, 0, sizeof c);
    lj_artnet_cfg_load(ini, &c);
    ACHECK(c.proto[1] == LJ_OUT_ARTNET && c.artnet_addr[1] == 0x0203 && c.artnet_in == 9);
    ACHECK(lj_artnet_cfg_validate(&c) == NULL);
    c.proto[0] = LJ_OUT_ARTNET; c.artnet_addr[0] = 0x0203; ACHECK(lj_artnet_cfg_validate(&c) != NULL);   /* duplicate */
    c.artnet_addr[0] = 0; ACHECK(lj_artnet_cfg_validate(&c) != NULL);   /* 0 refused */
    c.proto[0] = LJ_OUT_SACN; ACHECK(lj_artnet_cfg_validate(&c) == NULL);   /* an sACN link's Port-Address is not checked */
    lj_ini_free(ini);
}

static void art_ui(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_sacn_cfg cfg;
    lj_window *w;
    const char *msg;
    lj_ini *ini = lj_ini_new();
    memset(&cfg, 0, sizeof cfg);
    cfg.priority = LJ_SACN_PRIORITY_DEFAULT; cfg.ttl = 1; cfg.universe_of_link[0] = 1; cfg.universe_of_link[1] = 2; cfg.universe_of_link[2] = 3; cfg.universe_of_link[3] = 4;
    lj_artnet_cfg_load(NULL, &cfg);
    ctx->sacn = &cfg; ctx->hardware_ini = ini; ctx->userdir[0] = 0;   /* apply writes ./Hardware.Ini (removed below) */
    w = lj_hardware_setup_open();
    ACHECK(w && lj_window_find(w, "cbProto2") && lj_window_find(w, "seArtAddr2"));
    if (!w || !lj_window_find(w, "cbProto2")) { lj_ini_free(ini); ctx->sacn = NULL; ctx->hardware_ini = NULL; return; }
    ACHECK(((lj_w_combobox *)lj_window_find(w, "cbProto2"))->index == 0 && lj_w_spinedit_value(lj_window_find(w, "seArtAddr2")) == 2);
    ((lj_w_combobox *)lj_window_find(w, "cbProto2"))->index = 1;
    lj_w_spinedit_set_value(lj_window_find(w, "seArtAddr2"), 0);
    msg = lj_hardware_setup_apply();
    ACHECK(msg && strstr(msg, "Port-Address") && cfg.proto[1] == LJ_OUT_SACN);   /* refused, nothing applied */
    if (lj_win_message_window()) lj_w_click(lj_window_find(lj_win_message_window(), "BOk"));
    lj_w_spinedit_set_value(lj_window_find(w, "seArtAddr2"), 0x0102);
    msg = lj_hardware_setup_apply();
    ACHECK(msg == NULL && cfg.proto[1] == LJ_OUT_ARTNET && cfg.artnet_addr[1] == 0x0102 && cfg.proto[0] == LJ_OUT_SACN);
    ACHECK(lj_ini_geti(ini, "ARTNET", "Proto2", -1) == 1 && lj_ini_geti(ini, "ARTNET", "Addr2", -1) == 0x0102 && lj_ini_geti(ini, "ARTNET", "Proto1", -1) == 0);
    ctx->view.artnet_bound = 1; ctx->view.artnet_nodes = 2; ctx->view.artnet_pps[1] = 44;
    lj_hardware_setup_refresh();
    ACHECK(strstr(lj_w_caption(lj_window_find(w, "lSacnStatus")), "Art-Net 44 pkt/s, 2 nodes") != NULL && strstr(lj_w_caption(lj_window_find(w, "lSacnStatus")), "U2 ") == NULL);
    printf("  status: %s\n", lj_w_caption(lj_window_find(w, "lSacnStatus")));
    ctx->view.artnet_bound = 0; ctx->view.artnet_last_err = 10048;
    lj_hardware_setup_refresh();
    ACHECK(strstr(lj_w_caption(lj_window_find(w, "lSacnStatus")), "UDP 6454 in use") != NULL);   /* W5-ART-5: shown */
    lj_hardware_setup_close();
    w = lj_hardware_setup_open();                                     /* reopened: the selection reads back */
    ACHECK(w && ((lj_w_combobox *)lj_window_find(w, "cbProto2"))->index == 1 && lj_w_spinedit_value(lj_window_find(w, "seArtAddr2")) == 0x0102 && ((lj_w_combobox *)lj_window_find(w, "cbProto1"))->index == 0);
    lj_hardware_setup_close();
    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->sacn = NULL; ctx->hardware_ini = NULL;
    lj_ini_free(ini);
    remove("Hardware.Ini");
}

static lj_sacn_cfg art_open_cfg(uint32_t nic)
{
    lj_sacn_cfg c;
    lj_sacn_cfg_default(&c, "test");
    lj_artnet_cfg_load(NULL, &c);
    c.nic_ipv4 = nic;
    c.proto[1] = LJ_OUT_ARTNET; c.artnet_addr[1] = 2;                  /* link 2 -> Port-Address 2 */
    c.proto[2] = LJ_OUT_ARTNET; c.artnet_addr[2] = 3;                  /* link 3 -> Port-Address 3, no subscriber */
    return c;
}

static void art_discovery_and_rate(void)
{
    static uint8_t r[LJ_ARTNET_REPLY_LEN], slots[LJ_LINK_CH];
    lj_sacn_cfg c = art_open_cfg(LJ_IP_LOOPBACK);
    lj_artnet_stats st;
    lj_artnet_node nd[4];
    uint64_t t = 1000000u;
    int i;
    if (lj_artnet_open(&c) != 0) { printf("SKIP discovery / rate: cannot bind 127.0.0.1:6454 (another Art-Net program?)\n"); lj_artnet_close(); return; }
    lj_artnet_stats_get(&st); ACHECK(st.bound == 1);
    art_reply(r, 0, 0, 2, 0);
    lj_artnet_feed(r, sizeof r, LJ_IP_LOOPBACK, t);                   /* our own address: ignored */
    ACHECK(lj_artnet_nodes(nd, 4) == 0);
    lj_artnet_feed(r, sizeof r, 0x7F000002u, t);
    lj_artnet_feed(r, sizeof r, 0x7F000002u, t);                      /* the same node again: one row */
    ACHECK(lj_artnet_nodes(nd, 4) == 1 && nd[0].ip == 0x7F000002u && nd[0].port_addr[0] == 2);
    r[211] = 2; lj_artnet_feed(r, sizeof r, 0x7F000002u, t); r[211] = 0;   /* a second BindIndex page: a second row, same host */
    ACHECK(lj_artnet_nodes(nd, 4) == 2);
    /* expiry: polls at t, +2.5 s, +5 s leave the rows; the 4th poll (3 unanswered) drops them */
    for (i = 0; i < 3; i++) { lj_artnet_pump(t + (uint64_t)i * LJ_ARTNET_POLL_US); ACHECK(lj_artnet_nodes(nd, 4) == 2); }
    lj_artnet_pump(t + 3u * LJ_ARTNET_POLL_US);
    ACHECK(lj_artnet_nodes(nd, 4) == 0);
    lj_artnet_stats_get(&st); ACHECK(st.polls == 4 && st.nodes == 0);
    printf("  discovery: %u polls, %u replies, %u discarded, expiry after %d polls\n", (unsigned)st.polls, (unsigned)st.replies, (unsigned)st.discarded, LJ_ARTNET_EXPIRE_POLLS);
    /* rate (test_rate shape): 400 changing pumps 25 ms apart = 400 ArtDmx to the one subscribed host (two pages, one packet) */
    lj_artnet_close(); c = art_open_cfg(LJ_IP_LOOPBACK);
    if (lj_artnet_open(&c) != 0) { ACHECK(0); return; }
    t = 1000000u;
    for (i = 0; i < 400; i++, t += 25000u) {
        memset(slots, i & 0xFF, sizeof slots); slots[0] = (uint8_t)(i >> 8) ^ 0x55;
        lj_artnet_feed(r, sizeof r, 0x7F000002u, t); r[211] = 2; lj_artnet_feed(r, sizeof r, 0x7F000002u, t); r[211] = 0;
        lj_artnet_set(1, slots); lj_artnet_set(2, slots); lj_artnet_set(0, slots);   /* link 1 is sACN: ignored here */
        lj_artnet_pump(t);
    }
    lj_artnet_stats_get(&st);
    ACHECK(st.packets[1] == 400 && st.packets[2] == 0 && st.packets[0] == 0);   /* no subscriber = no ArtDmx */
    for (i = 0; i < 200; i++, t += 10000u) { lj_artnet_feed(r, sizeof r, 0x7F000002u, t); lj_artnet_set(1, slots); lj_artnet_pump(t); }   /* 2 s unchanged */
    lj_artnet_stats_get(&st);
    ACHECK(st.packets[1] == 402);                                      /* keep-alives at 900 ms */
    printf("  rate: 400 changes -> %u ArtDmx, then %u keep-alives in 2 s; link 3 (no subscriber) %u\n", 400u, (unsigned)(st.packets[1] - 400), (unsigned)st.packets[2]);
    for (i = 0; i < 100; i++, t += 10000u) { slots[1] = (uint8_t)i; lj_artnet_feed(r, sizeof r, 0x7F000002u, t); lj_artnet_set(1, slots); lj_artnet_pump(t); }   /* 100 changes at 100 Hz */
    lj_artnet_stats_get(&st);
    ACHECK(st.packets[1] - 402 == 34);                                 /* the 44 Hz cap: with 10 ms pumps one every 30 ms over 990 ms */
    lj_artnet_close(); c = art_open_cfg(LJ_IP_LOOPBACK); c.unlock_rate = 1;
    if (lj_artnet_open(&c) != 0) { ACHECK(0); return; }
    for (i = 0; i < 100; i++, t += 10000u) { slots[1] = (uint8_t)(i + 1); lj_artnet_feed(r, sizeof r, 0x7F000002u, t); lj_artnet_set(1, slots); lj_artnet_pump(t); }
    lj_artnet_stats_get(&st);
    ACHECK(st.packets[1] == 100);                                      /* unlock_rate: every change */
    lj_artnet_close();
    lj_artnet_stats_get(&st); ACHECK(st.bound == 0);
}

static void art_bind_fail(void)
{
    lj_socket blocker = lj_socket_udp(LJ_IP_LOOPBACK, LJ_ARTNET_PORT, 0);
    lj_sacn_cfg c = art_open_cfg(LJ_IP_LOOPBACK);
    lj_artnet_stats st;
    if (blocker == LJ_SOCKET_BAD) { printf("SKIP bind failure: 127.0.0.1:6454 already taken\n"); return; }
    ACHECK(lj_artnet_open(&c) == -1);
    lj_artnet_stats_get(&st); ACHECK(st.bound == 0);
    printf("  bind failure: Art-Net off (error %d), sACN still opens\n", st.last_err);
    lj_artnet_pump(1000000u);                                          /* closed: no-op */
    ACHECK(lj_sacn_open(&c) == 0);                                     /* sACN unaffected */
    lj_sacn_close();
    lj_artnet_close();
    lj_socket_close(blocker);
    c.proto[1] = c.proto[2] = LJ_OUT_SACN;
    ACHECK(lj_artnet_open(&c) == 0);                                   /* W5-X-OFF: no Art-Net link, artnet_in 0 = nothing bound */
    lj_artnet_stats_get(&st); ACHECK(st.bound == 0);
    lj_artnet_close();
}

static void art_wire(void)
{
    static uint8_t buf[LJ_ARTNET_DMX_LEN + 1], want[LJ_ARTNET_DMX_LEN], r[LJ_ARTNET_REPLY_LEN], slots[LJ_LINK_CH];
    lj_socket node = lj_socket_udp(LJ_IP_LOOPBACK, LJ_ARTNET_PORT, 0);   /* the "node" on 127.0.0.1:6454 */
    lj_sacn_cfg c = art_open_cfg(0x7F000002u);                          /* our NIC 127.0.0.2 */
    lj_artnet_node nd[2];
    uint32_t ip = 0; uint16_t port = 0;
    uint64_t t0;
    int n = 0, i;
    if (node == LJ_SOCKET_BAD) { printf("SKIP wire: 127.0.0.1:6454 taken\n"); return; }
    if (lj_artnet_open(&c) != 0) { printf("SKIP wire: cannot bind 127.0.0.2 (no 127/8 alias on this OS)\n"); lj_artnet_close(); lj_socket_close(node); return; }
    art_reply(r, 0, 0, 2, 0);
    ACHECK(lj_socket_sendto(node, r, sizeof r, 0x7F000002u, LJ_ARTNET_PORT) == 0);   /* a real ArtPollReply over UDP */
    for (t0 = lj_clock_us(); lj_artnet_nodes(nd, 2) == 0 && lj_clock_us() - t0 < 2000000u; ) lj_artnet_pump(1000u);   /* now 1 ms: no keep-alive due yet */
    ACHECK(lj_artnet_nodes(nd, 2) == 1 && nd[0].ip == LJ_IP_LOOPBACK);
    for (i = 0; i < LJ_LINK_CH; i++) slots[i] = (uint8_t)(i * 7);
    lj_artnet_set(1, slots);
    lj_artnet_pump(2000000u);
    for (t0 = lj_clock_us(); (n = lj_socket_recvfrom(node, buf, sizeof buf, &ip, &port)) == 0 && lj_clock_us() - t0 < 2000000u; ) {}
    lj_artnet_pack_dmx(want, 1, 1, 2, slots);                          /* seq 1, Physical 1 (link 2), Port-Address 2 */
    ACHECK(n == LJ_ARTNET_DMX_LEN && memcmp(buf, want, LJ_ARTNET_DMX_LEN) == 0 && ip == 0x7F000002u && port == LJ_ARTNET_PORT);
    printf("  wire: %d-byte ArtDmx from %u.%u.%u.%u:%u, %s\n", n, ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, (unsigned)port, n == LJ_ARTNET_DMX_LEN && memcmp(buf, want, LJ_ARTNET_DMX_LEN) == 0 ? "byte-exact" : "MISMATCH");
    lj_artnet_close();
    lj_socket_close(node);
}

int lj_test_artnet(int argc, char **argv);
int lj_test_artnet(int argc, char **argv)
{
    (void)argc; (void)argv;
    lj_log_set_sink(art_quiet, NULL);
    art_builders_readers();
    art_cfg();
    art_ui();
    art_discovery_and_rate();
    art_bind_fail();
    art_wire();
    lj_warnings_close_all();
    printf("test_artnet: %s\n", art_fails ? "FAIL" : "PASS");
    return art_fails ? 1 : 0;
}
