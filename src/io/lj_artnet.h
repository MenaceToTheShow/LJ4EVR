/* lj_artnet.h - Art-Net 4 output beside sACN (design 1 D14, 10 W5; note 08 s9; docs/w5-contracts.md seam 1; track artnet).
 *
 * The same Sender shape as io/lj_sacn.h (design 8.1, ME-6): lj_artnet_set copies and marks, lj_artnet_pump sends at most
 * one ArtDmx per subscriber per link per call (the change, else a keep-alive every LJ_SACN_KEEPALIVE_US), under the
 * same ~44 Hz cap and cfg->unlock_rate (ME-24). It shares the sACN frame tap: the engine thread (platform/sdl/lj_sdl.c)
 * hands frame->out[link] to lj_artnet_set for every link whose lj_sacn_cfg.proto is LJ_OUT_ARTNET and to lj_sacn_set
 * for the rest, then pumps both. The per-link choice and Port-Address live in the one output record, lj_sacn_cfg
 * (Hardware.Ini [ARTNET], additive), so LJ_CMD_SACN_CFG carries both and only the engine thread touches sender state
 * (ME-23, W5-X-ENGINE).
 *
 * Network (note 08 s9, spec p.10-11, p.16, p.62-64): one UDP socket (io/lj_socket.h) bound to cfg->nic_ipv4:6454 with
 * SO_BROADCAST, opened only while a link selects Art-Net or cfg->artnet_in != 0 (W5-X-OFF); ArtPoll every
 * LJ_ARTNET_POLL_US to the NIC's directed broadcast (ipv4 | ~mask from lj_nic, never 255.255.255.255); each
 * ArtPollReply updates a bounded subscriber table (universe -> [ip]) from NetSwitch / SubSwitch / SwOut; a node not heard
 * for LJ_ARTNET_EXPIRE_POLLS polls expires; packets from our own address are ignored; ArtDmx is unicast to each subscriber
 * of its Port-Address only, never broadcast, and not sent at all with no subscriber. A bind failure (another Art-Net
 * program owns 6454) is logged and shown through the stats, never fatal; sACN is unaffected (W5-ART-5).
 *
 * Input (W5-ART-IN): the pump drains the socket with a byte budget; an ArtDmx whose Port-Address equals
 * cfg->artnet_in and whose source is not us is handed whole to lj_dmxin_post(LJ_DMXIN_ARTNET, ...) (io/lj_dmxin.h),
 * which the UI thread parses. Every reader is hostile-input code (W5-ART-6): ID, OpCode, ProtVer >= 14, the length
 * before every field, ArtDmx Length even 2..512 and <= the bytes present; names bounded, never assumed terminated.
 *
 * Credit (W5-ART-7, spec p.9): "Art-Net(TM) Designed by and Copyright Artistic Licence" in docs/user-guide.md and help. */
#ifndef LJ_IO_ARTNET_H
#define LJ_IO_ARTNET_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_types.h"
#include "io/lj_sacn.h"

enum {
    LJ_ARTNET_PORT = 6454, LJ_ARTNET_PROTVER = 14,
    LJ_ARTNET_OP_POLL = 0x2000, LJ_ARTNET_OP_POLLREPLY = 0x2100, LJ_ARTNET_OP_DMX = 0x5000,
    LJ_ARTNET_DMX_HDR = 18, LJ_ARTNET_DMX_LEN = 18 + 512, LJ_ARTNET_POLL_LEN = 14, LJ_ARTNET_REPLY_MIN = 207 /* through SwOut + AcnPriority..BindIp: PROPOSAL rig W5-ART-6, older nodes send 207-239 */,
    LJ_ARTNET_REPLY_LEN = 239, LJ_ARTNET_ADDR_MAX = 32767,
    LJ_ARTNET_NODES = 32,          /* subscriber table rows, PROPOSAL rig W5-ART-2 */
    LJ_ARTNET_NODE_PORTS = 4,      /* SwOut[4] of one ArtPollReply (BindIndex pages are separate rows) */
    LJ_ARTNET_EXPIRE_POLLS = 3,    /* a node not heard for 3 polls expires at the next one (7.5-10 s after its last reply), PROPOSAL rig W5-ART-2 */
    LJ_ARTNET_RX_BUDGET = 64,      /* datagrams drained per pump (W5-X-THREAD), PROPOSAL */
    LJ_ARTNET_ROWS_PER_IP = 4      /* ME-176: subscriber rows one source address may hold (BindIndex pages), PROPOSAL rig W5-ART-2 */
};
#define LJ_ARTNET_POLL_US      2500000u   /* spec p.16: every 2.5 to 3 s */
#define LJ_ARTNET_OEM           0x00FFu   /* OemUnknown; PROPOSAL rig W5-ART-7 (Joey applies for an OemCode, or this stays with its reason) */

typedef struct { uint32_t ip; uint16_t port_addr[LJ_ARTNET_NODE_PORTS]; uint8_t nports; uint8_t missed; char short_name[18]; char long_name[64]; uint8_t bind_index; } lj_artnet_node;   /* names CP1252, NUL-terminated after a bounded copy (W5-X-TEXT) */
typedef struct { uint32_t packets[LJ_LINKS], pps[LJ_LINKS]; uint32_t nodes, polls, replies, discarded, send_errors; int last_err; uint8_t bound; } lj_artnet_stats;   /* per link; bound = the 6454 socket is open */

/* ---- sender (engine thread only, ME-23) ---- */
int  lj_artnet_open(const lj_sacn_cfg *cfg);                          /* close-old then open-new on a copy (the node table and poll clock kept when the NIC is unchanged, LORD_ENGINE-10); no Art-Net link and artnet_in 0 = closed, returns 0; -1 on a bind failure (logged, stats.bound 0) */
void lj_artnet_set(int link, const uint8_t slots[LJ_LINK_CH]);        /* link 0..LJ_LINKS-1; copies, marks a change; never sends; a link not on Art-Net is ignored */
void lj_artnet_pump(uint64_t now_us);                                 /* ArtPoll when due; drain received packets (budget); per link at most one ArtDmx per subscriber (change, else keep-alive); expiry */
void lj_artnet_close(void);                                           /* closes the socket and forgets the nodes; no-op when closed */
void lj_artnet_stats_get(lj_artnet_stats *out);
int  lj_artnet_nodes(lj_artnet_node *out, int max);                   /* tests: a copy of the subscriber table (engine thread) */
void lj_artnet_feed(const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us);   /* one received datagram as the pump handles it (tests, the artnet fuzz row) */

/* bind_index (appended by the artnet track): ArtPollReply BindIndex (offset 211, 0 when the reply is shorter); one table
 * row per (source address, BindIndex), so a node with more than four ports keeps every page. The row's ip is the
 * datagram's source address, never the packet's IP field (a reply cannot point our ArtDmx at a third host). */

/* ---- packet builders and readers (pure; tests/io/test_artnet.c against gen_vectors.py byte vectors, W5-ART-1) ---- */
int lj_artnet_pack_dmx(uint8_t out[LJ_ARTNET_DMX_LEN], uint8_t seq, uint8_t physical, uint16_t port_addr, const uint8_t slots[LJ_LINK_CH]);   /* 530 or -1 (port_addr 0 or > 32767) */
int lj_artnet_pack_poll(uint8_t *out, size_t cap);                    /* the ArtPoll we send (Flags, DiagPriority PROPOSAL); its length or -1 */
int lj_artnet_opcode(const uint8_t *pkt, size_t n);                   /* the OpCode when ID matches and n >= 10, else -1 */
int lj_artnet_parse_dmx(const uint8_t *pkt, size_t n, uint16_t *port_addr, uint8_t *seq, uint8_t slots[LJ_LINK_CH], int *nslots);   /* 0 / -1 (ProtVer < 14, Length odd / 0 / > 512 / past n); slots past nslots are 0 */
int lj_artnet_parse_reply(const uint8_t *pkt, size_t n, lj_artnet_node *out);   /* 0 / -1 (short, wrong op); names bounded */

/* ---- the output record (Hardware.Ini [ARTNET] Proto1..4 Addr1..4, ours, additive: W5-X-FORMAT; artnet_in is runtime only, ME-178) ---- */
void lj_artnet_cfg_load(lj_ini *hardware_ini, lj_sacn_cfg *cfg);      /* missing keys = sACN on every link, Port-Address = link number, input off */
void lj_artnet_cfg_save(lj_ini *hardware_ini, const lj_sacn_cfg *cfg);
const char *lj_artnet_cfg_validate(const lj_sacn_cfg *cfg);           /* NULL = usable; Port-Address 0 / > 32767 on an Art-Net link, a duplicate Art-Net Port-Address */

#endif /* LJ_IO_ARTNET_H */
