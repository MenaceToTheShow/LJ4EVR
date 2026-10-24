/* lj_artnet.c - Art-Net 4 output and the input hand-off (io/lj_artnet.h; note 08 s9; docs/artnet.md).
 *
 * Sender state is file-static, like io/lj_sacn.c: only the engine thread calls open / set / pump / close / feed (ME-23),
 * so it needs no lock. Sockets go through io/lj_socket.h (this file includes no OS header). LJ2 had no Art-Net at all
 * (note 05 item 23), so there is no LJ2 behaviour to copy: we are a controller only and never answer ArtPoll. */
#include <stdio.h>
#include <string.h>
#include "io/lj_artnet.h"
#include "io/lj_dmxin.h"
#include "io/lj_net.h"
#include "io/lj_socket.h"
#include "core/lj_log.h"

enum { LJ_ARTNET_POLL_SEND = 22,   /* ID .. OemLo (spec p.18-19); a node accepts >= 14 */
       LJ_ARTNET_RX_CAP = LJ_ARTNET_DMX_LEN + 1 };   /* one past the largest packet we read: a full buffer is a truncated datagram */

typedef struct { uint8_t slots[LJ_LINK_CH]; uint8_t seq, changed, active; uint64_t last_us; uint32_t in_window; } lj_artnet_link;

static struct {
    int open;
    lj_socket sock;
    lj_sacn_cfg cfg;
    uint32_t own_ip, bcast, mask;    /* the NIC address we bound / ignore, its directed broadcast (0 = no ArtPoll), its netmask (ME-176: replies only from that subnet) */
    lj_artnet_link l[LJ_LINKS];
    lj_artnet_node node[LJ_ARTNET_NODES]; int nnodes;
    uint64_t next_poll_us, window_us;
    lj_artnet_stats st;
} la = { .sock = LJ_SOCKET_BAD };

static const uint8_t lj_artnet_id[8] = { 'A', 'r', 't', '-', 'N', 'e', 't', 0 };

/* ---- packet builders and readers (pure) ------------------------------------------------ */

static void lj_artnet_hdr(uint8_t *p, unsigned op) { memcpy(p, lj_artnet_id, 8); p[8] = (uint8_t)op; p[9] = (uint8_t)(op >> 8); p[10] = 0; p[11] = LJ_ARTNET_PROTVER; }

int lj_artnet_pack_dmx(uint8_t out[LJ_ARTNET_DMX_LEN], uint8_t seq, uint8_t physical, uint16_t port_addr, const uint8_t slots[LJ_LINK_CH])
{
    if (!out || !slots || port_addr == 0 || port_addr > LJ_ARTNET_ADDR_MAX) return -1;
    lj_artnet_hdr(out, LJ_ARTNET_OP_DMX);
    out[12] = seq;
    out[13] = physical;
    out[14] = (uint8_t)port_addr;            /* SubUni: Sub-Net << 4 | Universe */
    out[15] = (uint8_t)(port_addr >> 8);     /* Net, bits 14-8 */
    out[16] = LJ_LINK_CH >> 8; out[17] = LJ_LINK_CH & 0xFF;   /* Length high byte first (p.63) */
    memcpy(out + LJ_ARTNET_DMX_HDR, slots, LJ_LINK_CH);
    return LJ_ARTNET_DMX_LEN;
}

int lj_artnet_pack_poll(uint8_t *out, size_t cap)
{
    if (!out || cap < LJ_ARTNET_POLL_SEND) return -1;
    memset(out, 0, LJ_ARTNET_POLL_SEND);
    lj_artnet_hdr(out, LJ_ARTNET_OP_POLL);
    out[12] = 0;                             /* Flags: no reply-on-change, no diagnostics, not targeted. PROPOSAL rig W5-ART-2 */
    out[13] = 0;                             /* DiagPriority: unused with diagnostics off. PROPOSAL rig W5-ART-2 */
    /* 14-17 TargetPortAddress (Targeted Mode off), 18-19 EstaMan 0: PROPOSAL rig W5-ART-7 */
    out[20] = (uint8_t)(LJ_ARTNET_OEM >> 8); out[21] = (uint8_t)LJ_ARTNET_OEM;   /* OemHi, OemLo */
    return LJ_ARTNET_POLL_SEND;
}

int lj_artnet_opcode(const uint8_t *pkt, size_t n)
{
    if (!pkt || n < 10 || memcmp(pkt, lj_artnet_id, 8) != 0) return -1;
    return pkt[8] | pkt[9] << 8;
}

int lj_artnet_parse_dmx(const uint8_t *pkt, size_t n, uint16_t *port_addr, uint8_t *seq, uint8_t slots[LJ_LINK_CH], int *nslots)
{
    unsigned len;
    if (lj_artnet_opcode(pkt, n) != LJ_ARTNET_OP_DMX || n < LJ_ARTNET_DMX_HDR) return -1;
    if ((pkt[10] << 8 | pkt[11]) < LJ_ARTNET_PROTVER) return -1;
    len = (unsigned)pkt[16] << 8 | pkt[17];
    if (len < 2 || len > LJ_LINK_CH || (len & 1) || LJ_ARTNET_DMX_HDR + (size_t)len > n) return -1;
    if (port_addr) *port_addr = (uint16_t)((pkt[15] & 0x7F) << 8 | pkt[14]);
    if (seq) *seq = pkt[12];
    if (slots) { memcpy(slots, pkt + LJ_ARTNET_DMX_HDR, len); memset(slots + len, 0, LJ_LINK_CH - len); }
    if (nslots) *nslots = (int)len;
    return 0;
}

static void lj_artnet_name(char *dst, size_t cap, const uint8_t *src, size_t field)   /* bounded, never assumes a NUL (W5-X-TEXT) */
{
    size_t i;
    for (i = 0; i + 1 < cap && i < field && src[i]; i++) dst[i] = (char)src[i];
    memset(dst + i, 0, cap - i);
}

int lj_artnet_parse_reply(const uint8_t *pkt, size_t n, lj_artnet_node *out)
{
    unsigned np, i, base;
    if (!out || lj_artnet_opcode(pkt, n) != LJ_ARTNET_OP_POLLREPLY || n < LJ_ARTNET_REPLY_MIN) return -1;   /* no ProtVer in a reply: offset 10 is the IP */
    memset(out, 0, sizeof *out);
    out->ip = (uint32_t)pkt[10] << 24 | (uint32_t)pkt[11] << 16 | (uint32_t)pkt[12] << 8 | pkt[13];
    base = (unsigned)(pkt[18] & 0x7F) << 8 | (unsigned)(pkt[19] & 0x0F) << 4;   /* NetSwitch, SubSwitch */
    lj_artnet_name(out->short_name, sizeof out->short_name, pkt + 26, 18);
    lj_artnet_name(out->long_name, sizeof out->long_name, pkt + 44, 64);
    np = (unsigned)pkt[172] << 8 | pkt[173];
    if (np > LJ_ARTNET_NODE_PORTS) np = LJ_ARTNET_NODE_PORTS;
    for (i = 0; i < np; i++)
        if ((pkt[174 + i] & 0x80) && (base | (pkt[190 + i] & 0x0F)) != 0)   /* PortTypes bit 7 = output; SwOut low nibble; Port-Address 0 deprecated */
            out->port_addr[out->nports++] = (uint16_t)(base | (pkt[190 + i] & 0x0F));
    out->bind_index = n > 211 ? pkt[211] : 0;
    return 0;
}

/* ---- the output record ----------------------------------------------------------------------- */

void lj_artnet_cfg_load(lj_ini *ini, lj_sacn_cfg *cfg)
{
    char key[16];
    int i, v;
    if (!cfg) return;
    for (i = 0; i < LJ_LINKS; i++) {
        if (snprintf(key, sizeof key, "Proto%d", i + 1) < 0) key[0] = 0;
        cfg->proto[i] = (uint8_t)(lj_ini_geti(ini, "ARTNET", key, LJ_OUT_SACN) == LJ_OUT_ARTNET ? LJ_OUT_ARTNET : LJ_OUT_SACN);
        if (snprintf(key, sizeof key, "Addr%d", i + 1) < 0) key[0] = 0;
        v = lj_ini_geti(ini, "ARTNET", key, i + 1);
        if (v < 1 || v > LJ_ARTNET_ADDR_MAX) { lj_log("artnet: Hardware.Ini [ARTNET] %s=%d not 1..32767, using %d", key, v, i + 1); v = i + 1; }
        cfg->artnet_addr[i] = (uint16_t)v;   /* default link N -> Port-Address N: PROPOSAL rig W5-ART-4 */
    }
    cfg->artnet_in = 0;                                                 /* ME-178: runtime only, set from [DMXIN] by the app's DMX-in apply; never read */
}

void lj_artnet_cfg_save(lj_ini *ini, const lj_sacn_cfg *cfg)
{
    char key[16];
    int i;
    if (!ini || !cfg) return;
    for (i = 0; i < LJ_LINKS; i++) {
        { if (snprintf(key, sizeof key, "Proto%d", i + 1) < 0) key[0] = 0; } lj_ini_seti(ini, "ARTNET", key, cfg->proto[i]);
        { if (snprintf(key, sizeof key, "Addr%d", i + 1) < 0) key[0] = 0; }  lj_ini_seti(ini, "ARTNET", key, cfg->artnet_addr[i]);
    }
    /* ME-178: no In key: artnet_in is derived from [DMXIN] at run time, and an older file's In is never read */
}

const char *lj_artnet_cfg_validate(const lj_sacn_cfg *cfg)
{
    int i, j;
    if (!cfg) return "no config";
    for (i = 0; i < LJ_LINKS; i++) {
        if (cfg->proto[i] != LJ_OUT_ARTNET) continue;
        if (cfg->artnet_addr[i] == 0 || cfg->artnet_addr[i] > LJ_ARTNET_ADDR_MAX) return "Art-Net Port-Address must be 1..32767";
        for (j = 0; j < i; j++) if (cfg->proto[j] == LJ_OUT_ARTNET && cfg->artnet_addr[j] == cfg->artnet_addr[i]) return "duplicate Art-Net Port-Address";
    }
    if (cfg->artnet_in > LJ_ARTNET_ADDR_MAX) return "Art-Net input Port-Address must be 0..32767";
    return NULL;
}

/* ---- the wire ------------------------------------------------------------------------------------- */

static void lj_artnet_send(const uint8_t *pkt, size_t n, uint32_t ip)
{
    if (lj_socket_sendto(la.sock, pkt, n, ip, LJ_ARTNET_PORT) == 0) return;
    la.st.send_errors++;
    if (lj_socket_error() != la.st.last_err) lj_log("artnet: sendto %u.%u.%u.%u failed: %d", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, lj_socket_error());
    la.st.last_err = lj_socket_error();
}

int lj_artnet_open(const lj_sacn_cfg *cfg)
{
    static lj_artnet_node keep[LJ_ARTNET_NODES];                       /* engine thread only (ME-23) */
    lj_nic nic[16];
    const char *why;
    int i, j, k, need = 0, nn, nkeep = 0;
    uint64_t keep_poll = 0;
    if (la.open && cfg && cfg->nic_ipv4 == la.cfg.nic_ipv4) {           /* LORD_ENGINE-10: same NIC, same port 6454: the subscribers stay, the poll clock runs on */
        nkeep = la.nnodes; memcpy(keep, la.node, (size_t)nkeep * sizeof *keep); keep_poll = la.next_poll_us;
    }
    lj_artnet_close();
    if (!cfg) return 0;
    la.cfg = *cfg;
    why = lj_artnet_cfg_validate(cfg);
    if (why) lj_log("artnet: config: %s (the offending link stays silent)", why);
    for (i = 0; i < LJ_LINKS; i++) {                                    /* the same rule as validate, per link: a bad or duplicate address is not sent */
        uint16_t a = cfg->artnet_addr[i];
        int ok = cfg->proto[i] == LJ_OUT_ARTNET && a >= 1 && a <= LJ_ARTNET_ADDR_MAX;
        for (j = 0; j < i && ok; j++) if (la.l[j].active && cfg->artnet_addr[j] == a) ok = 0;
        la.l[i].active = (uint8_t)ok;
        la.l[i].seq = 1;
        need |= ok;
    }
    if (cfg->artnet_in > LJ_ARTNET_ADDR_MAX) la.cfg.artnet_in = 0;
    if (!need && !la.cfg.artnet_in) return 0;                          /* W5-X-OFF: nothing opens 6454 */
    if (lj_net_startup() != 0) return -1;
    la.own_ip = cfg->nic_ipv4;
    nn = lj_net_list_ipv4(nic, (int)(sizeof nic / sizeof nic[0]));
    for (k = 0; k < nn; k++)
        if (cfg->nic_ipv4 ? nic[k].ipv4 == cfg->nic_ipv4 : (nic[k].up && !nic[k].loopback)) {   /* 0.0.0.0 = the first up interface (polled there, bound to any) */
            la.own_ip = nic[k].ipv4;
            if (nic[k].mask) { la.bcast = nic[k].ipv4 | ~nic[k].mask; la.mask = nic[k].mask; }   /* directed broadcast, never 255.255.255.255 (p.16) */
            break;
        }
    if (!la.mask && la.own_ip >> 24 == 127) la.mask = 0xFF000000u;   /* a loopback alias (127.0.0.2) is not listed: its subnet is 127/8 (RFC 1122) */
    if (!la.bcast) lj_log("artnet: no netmask for the chosen interface: ArtPoll off%s", la.mask ? "" : ", no node can subscribe");
    la.sock = lj_socket_udp(cfg->nic_ipv4, LJ_ARTNET_PORT, LJ_SOCK_BROADCAST);
    if (la.sock == LJ_SOCKET_BAD) {                                     /* W5-ART-5: another Art-Net program owns 6454; logged by the seam, shown in the status line */
        la.st.last_err = lj_socket_error();
        lj_log("artnet: cannot bind UDP %d (another Art-Net program?): Art-Net off, sACN unaffected", LJ_ARTNET_PORT);
        return -1;
    }
    la.st.bound = 1;
    la.open = 1;
    memcpy(la.node, keep, (size_t)nkeep * sizeof *keep); la.nnodes = nkeep; la.st.nodes = (uint32_t)nkeep; la.next_poll_us = keep_poll;
    return 0;
}

void lj_artnet_close(void)
{
    lj_socket_close(la.sock);
    memset(&la, 0, sizeof la);
    la.sock = LJ_SOCKET_BAD;
}

void lj_artnet_set(int link, const uint8_t slots[LJ_LINK_CH])
{
    if (!la.open || !slots || link < 0 || link >= LJ_LINKS || !la.l[link].active) return;
    if (memcmp(la.l[link].slots, slots, LJ_LINK_CH) != 0) { memcpy(la.l[link].slots, slots, LJ_LINK_CH); la.l[link].changed = 1; }
}

void lj_artnet_feed(const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us)
{
    lj_artnet_node nd;
    uint16_t pa;
    int i, op = lj_artnet_opcode(pkt, n);
    if (op < 0 || (la.own_ip && src_ip == la.own_ip)) { la.st.discarded++; return; }   /* not Art-Net, or our own ArtPoll / ArtDmx */
    if (op == LJ_ARTNET_OP_POLLREPLY) {
        int same = 0;
        if (!la.mask || ((src_ip ^ la.own_ip) & la.mask) != 0) { la.st.discarded++; return; }   /* ME-176: off-subnet = a spoof that would make us a unicast reflector */
        if (lj_artnet_parse_reply(pkt, n, &nd) != 0) { la.st.discarded++; return; }
        nd.ip = src_ip;
        la.st.replies++;
        for (i = 0; i < la.nnodes && (la.node[i].ip != src_ip || la.node[i].bind_index != nd.bind_index); i++) same += la.node[i].ip == src_ip;
        if (i == la.nnodes) {
            if (la.nnodes == LJ_ARTNET_NODES || same >= LJ_ARTNET_ROWS_PER_IP) { la.st.discarded++; return; }   /* table full: the node waits for an expiry; one host cannot fill it */
            la.nnodes++;
        }
        la.node[i] = nd;                                                /* missed = 0: heard */
    } else if (op == LJ_ARTNET_OP_DMX) {
        if (lj_artnet_parse_dmx(pkt, n, &pa, NULL, NULL, NULL) != 0) { la.st.discarded++; return; }
        if (la.mask && ((src_ip ^ la.own_ip) & la.mask) != 0) { la.st.discarded++; return; }   /* ME-177: input only from the bound subnet */
        if (la.cfg.artnet_in && pa == la.cfg.artnet_in) lj_dmxin_post(LJ_DMXIN_ARTNET, pkt, n, src_ip, now_us);   /* W5-ART-IN: the UI thread parses it again */
    }
    la.st.nodes = (uint32_t)la.nnodes;
}

static int lj_artnet_subscribed(const lj_artnet_node *nd, uint16_t addr)
{
    int m;
    for (m = 0; m < nd->nports && m < LJ_ARTNET_NODE_PORTS; m++) if (nd->port_addr[m] == addr) return 1;
    return 0;
}

void lj_artnet_pump(uint64_t now_us)
{
    static uint8_t pkt[LJ_ARTNET_RX_CAP];
    uint32_t ip;
    uint16_t port;
    int i, j, k, m, n, sent;
    if (!la.open) return;
    if (now_us >= la.next_poll_us) {                                     /* every 2.5 s (p.16); expiry counts polls */
        for (i = 0; i < la.nnodes; )
            if (la.node[i].missed >= LJ_ARTNET_EXPIRE_POLLS) la.node[i] = la.node[--la.nnodes];
            else la.node[i++].missed++;
        if (la.bcast && (n = lj_artnet_pack_poll(pkt, sizeof pkt)) > 0) lj_artnet_send(pkt, (size_t)n, la.bcast);
        la.st.polls++;                                                  /* poll cycles (the expiry clock), sent or not */
        la.next_poll_us = now_us + LJ_ARTNET_POLL_US;
    }
    for (k = 0; k < LJ_ARTNET_RX_BUDGET && (n = lj_socket_recvfrom(la.sock, pkt, sizeof pkt, &ip, &port)) > 0; k++)
        if ((size_t)n < sizeof pkt) lj_artnet_feed(pkt, (size_t)n, ip, now_us);   /* a full buffer is longer than any packet we read */
    for (i = 0; i < LJ_LINKS; i++) {
        lj_artnet_link *l = &la.l[i];
        if (!l->active) continue;
        if (!l->changed && now_us - l->last_us < LJ_SACN_KEEPALIVE_US) continue;
        if (!la.cfg.unlock_rate && now_us - l->last_us < LJ_SACN_MIN_INTERVAL_US) continue;   /* ME-24, latest frame wins */
        lj_artnet_pack_dmx(pkt, l->seq, (uint8_t)i /* Physical = link - 1, PROPOSAL rig W5-ART-1 */, la.cfg.artnet_addr[i], l->slots);
        for (j = sent = 0; j < la.nnodes; j++) {
            if (!lj_artnet_subscribed(&la.node[j], la.cfg.artnet_addr[i])) continue;
            for (m = 0; m < j && !(la.node[m].ip == la.node[j].ip && lj_artnet_subscribed(&la.node[m], la.cfg.artnet_addr[i])); m++) {}
            if (m < j) continue;                                        /* one packet per host: BindIndex pages share the address */
            lj_artnet_send(pkt, LJ_ARTNET_DMX_LEN, la.node[j].ip);
            sent++;
        }
        if (!sent) continue;                                            /* p.63: no subscriber = no ArtDmx, and never broadcast */
        l->seq = (uint8_t)(l->seq == 255 ? 1 : l->seq + 1);             /* 1..255, 0 never sent */
        l->changed = 0;
        l->last_us = now_us;
        la.st.packets[i] += (uint32_t)sent;
        l->in_window += (uint32_t)sent;
    }
    if (now_us - la.window_us >= 1000000u) {
        for (i = 0; i < LJ_LINKS; i++) { la.st.pps[i] = la.l[i].in_window; la.l[i].in_window = 0; }
        la.window_us = now_us;
    }
    la.st.nodes = (uint32_t)la.nnodes;
}

void lj_artnet_stats_get(lj_artnet_stats *out) { if (out) *out = la.st; }

int lj_artnet_nodes(lj_artnet_node *out, int max)
{
    int n = la.nnodes < max ? la.nnodes : max;
    if (!out || n <= 0) return 0;
    memcpy(out, la.node, (size_t)n * sizeof *out);
    return n;
}
