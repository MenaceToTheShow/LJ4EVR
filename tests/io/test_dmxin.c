/* test_dmxin.c - the dmxin track's rows (docs/w5-contracts.md seam 2). The contract seeds two fuzz rows (W5-X-HOSTILE):
 *   fuzz_sacn_rx  a data packet from lj_sacn_pack_data (our own sender, design 8.1) must parse, then LJ_FUZZ_ITERS
 *                 mutations of it through lj_sacn_rx_parse and lj_dmxin_rx; the frame stays within its bounds
 *   fuzz_vtr      a synthetic .vtr text must parse, then mutations through lj_vtr_parse
 * Both print PENDING dmxin and fail until the track lands the parsers (CMakeLists.txt marks them DISABLED until then).
 * The track appends its rule rows (W5-DMXIN-1..8) to this file. */
#include <stdio.h>
#include <string.h>
#include "io/lj_dmxin.h"
#include "io/lj_sacn.h"
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
        printf("PENDING dmxin: lj_sacn_rx_parse does not accept our own data packet yet\n");
        return 1;
    }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sizeof seed); n = sizeof seed;
        for (k = (int)(lj_fuzz_next(&s) % 8); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        if (lj_sacn_rx_parse(buf, n, &rx) == 0 && (rx.n < 1 || rx.n > LJ_LINK_CH || rx.slots + rx.n > buf + n || rx.name[63] != 0)) { if (!bad++) printf("FAIL iteration %d: parse accepted out-of-bounds fields\n", i); }
        lj_dmxin_rx(LJ_DMXIN_SACN, buf, n, 0x7F000002u, (uint64_t)i * 1000u);
        f = lj_dmxin_frame_get();
        if (f->channels > LJ_LINK_CH || f->nsources < 0 || f->nsources > LJ_DMXIN_SOURCES) { if (!bad++) printf("FAIL iteration %d: frame out of bounds\n", i); }
    }
    printf("fuzz_sacn_rx: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}

int lj_test_fuzz_vtr(int argc, char **argv)
{
    static const char text[] = "LightJockey value table\r\nID={00000000-0000-4000-8000-000000000001}\r\nTYPE=USER\r\nNAME=Fuzz\r\nPURPOSE=synthetic\r\n0=0\r\n128=255\r\n255=0\r\n";
    static uint8_t buf[1024];
    lj_vtr t;
    char err[128];
    uint32_t s = 0x7A7u;
    size_t n;
    int i, k;
    (void)argc; (void)argv;
    if (lj_vtr_parse(text, sizeof text - 1, &t, err, sizeof err) != 0 || t.map[128] != 255 || t.map[1] != 1 || t.map[255] != 0) {
        printf("PENDING dmxin: lj_vtr_parse does not accept the synthetic table yet (the track sets the header line to the reference format)\n");
        return 1;
    }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, text, sizeof text - 1); n = sizeof text - 1;
        for (k = (int)(lj_fuzz_next(&s) % 8); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        lj_vtr_parse((const char *)buf, n, &t, err, sizeof err);   /* must not read past n: ASan is the check */
    }
    printf("fuzz_vtr: %d iterations, PASS\n", LJ_FUZZ_ITERS);
    return 0;
}
