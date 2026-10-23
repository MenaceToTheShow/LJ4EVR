/* fuzz_artnet.c - libFuzzer target over the Art-Net readers (io/lj_artnet.h; W5-ART-6, W5-X-HOSTILE; CMake -DLJ_FUZZ=ON,
 * clang). One datagram per input through lj_artnet_parse_dmx, lj_artnet_parse_reply and lj_artnet_feed (the subscriber
 * table and the ArtDmx input hand-off), the path a received packet takes on the engine thread. Seeds: the ArtDmx / ArtPoll
 * vectors of tests/vectors.h and the 239-byte reply of tests/io/test_artnet.c; -max_len=600. */
#include <string.h>
#include "io/lj_artnet.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static uint8_t slots[LJ_LINK_CH];
    static uint64_t now;
    lj_artnet_node nd;
    uint16_t pa; uint8_t seq; int ns;
    if (lj_artnet_parse_dmx(data, size, &pa, &seq, slots, &ns) == 0 && (ns < 2 || ns > LJ_LINK_CH || (ns & 1) || (size_t)ns + LJ_ARTNET_DMX_HDR > size)) __builtin_trap();
    if (lj_artnet_parse_reply(data, size, &nd) == 0 && (nd.nports > LJ_ARTNET_NODE_PORTS || nd.short_name[17] || nd.long_name[63])) __builtin_trap();
    lj_artnet_feed(data, size, 0x7F000002u, now += 1000u);
    return 0;
}
