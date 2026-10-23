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
