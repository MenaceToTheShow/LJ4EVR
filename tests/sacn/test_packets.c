/* test_packets.c - `lj4evr_tests test_sacn_packets`: lj_sacn_pack_data / lj_sacn_pack_discovery
 * byte-exact against tools/ref/08-sacn_ref.py (E1.31-2018 Appendix B, tests/vectors.h):
 * the 638-byte data packet, the Stream_Terminated variant, the discovery packet, the
 * CP1252 -> UTF-8 source name, and the rejected arguments. */
#define _CRT_SECURE_NO_WARNINGS 1   /* strcpy under /W4 /WX on MSVC (as core/lj_ini.c) */
#include <stdio.h>
#include <string.h>
#include "io/lj_sacn.h"
#include "vectors.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_first_diff(const uint8_t *a, const uint8_t *b, int n)
{
    int i;
    for (i = 0; i < n; i++) if (a[i] != b[i]) return i;
    return -1;
}

int lj_test_sacn_packets(int argc, char **argv);
int lj_test_sacn_packets(int argc, char **argv)
{
    lj_sacn_cfg cfg;
    uint8_t pkt[LJ_SACN_DATA_LEN], want[LJ_SACN_DATA_LEN], disc[160], slots[LJ_LINK_CH];
    uint16_t list[3] = { 1, 2, 3 };
    int i, d;
    (void)argc; (void)argv;

    memset(&cfg, 0, sizeof cfg);
    memcpy(cfg.cid, lj_vec_sacn_cid, 16);
    strcpy(cfg.name, "Source_A");
    cfg.priority = 100;
    memset(slots, 0, sizeof slots);

    /* Table B-1: universe 1, seq 154, priority 100, 512 zero slots. The vector carries sync address 7962;
     * design 8.1 never sends sync, so octets 109-110 are 0 here and 0x1f1a there; everything else byte-exact. */
    CHECK(lj_sacn_pack_data(pkt, &cfg, 1, 154, 0, slots) == 0);
    memcpy(want, lj_vec_sacn_data_b1, sizeof want);
    CHECK(want[109] == 0x1f && want[110] == 0x1a && pkt[109] == 0 && pkt[110] == 0);
    want[109] = want[110] = 0;
    d = lj_first_diff(pkt, want, LJ_SACN_DATA_LEN);
    CHECK(d == -1);
    if (d >= 0) printf("  B-1 differs at octet %d: %02x vs %02x\n", d, pkt[d], want[d]);
    CHECK(pkt[16] == 0x72 && pkt[17] == 0x6e && pkt[38] == 0x72 && pkt[39] == 0x58 && pkt[115] == 0x72 && pkt[116] == 0x0b);

    /* the oracle's chase packet: name LJ4EVR, universe 7962, seq 0, slots 1..8 = 1..8 */
    strcpy(cfg.name, "LJ4EVR");
    for (i = 0; i < 8; i++) slots[i] = (uint8_t)(i + 1);
    CHECK(lj_sacn_pack_data(pkt, &cfg, 7962, 0, 0, slots) == 0);
    d = lj_first_diff(pkt, lj_vec_sacn_data_lj, LJ_SACN_DATA_LEN);
    CHECK(d == -1);
    if (d >= 0) printf("  chase differs at octet %d: %02x vs %02x\n", d, pkt[d], lj_vec_sacn_data_lj[d]);

    /* Stream_Terminated (6.2.6): options 0x40, seq 3 */
    CHECK(lj_sacn_pack_data(pkt, &cfg, 7962, 3, 0x40, slots) == 0);
    d = lj_first_diff(pkt, lj_vec_sacn_data_term, LJ_SACN_DATA_LEN);
    CHECK(d == -1 && pkt[112] == 0x40 && pkt[111] == 3);

    /* discovery {3,1,2} -> the caller sorts -> 1,2,3; 126 octets */
    strcpy(cfg.name, "Source_A");
    CHECK(lj_sacn_pack_discovery(disc, sizeof disc, &cfg, list, 3, 0, 0) == 126);
    d = lj_first_diff(disc, lj_vec_sacn_disc, 126);
    CHECK(d == -1);
    if (d >= 0) printf("  discovery differs at octet %d: %02x vs %02x\n", d, disc[d], lj_vec_sacn_disc[d]);
    CHECK(lj_sacn_pack_discovery(disc, 125, &cfg, list, 3, 0, 0) == -1);      /* cap too small */
    CHECK(lj_sacn_pack_discovery(disc, sizeof disc, &cfg, NULL, 0, 0, 0) == 120);   /* empty list is legal */

    /* source name: CP1252 0xE9 (e acute) -> UTF-8 C3 A9; a 63-char name keeps the NUL at octet 107 */
    strcpy(cfg.name, "Caf\xE9");
    CHECK(lj_sacn_pack_data(pkt, &cfg, 1, 0, 0, slots) == 0);
    CHECK(memcmp(pkt + 44, "Caf\xC3\xA9\0", 6) == 0);
    memset(cfg.name, 'x', 63); cfg.name[63] = 0;
    CHECK(lj_sacn_pack_data(pkt, &cfg, 1, 0, 0, slots) == 0);
    CHECK(pkt[106] == 'x' && pkt[107] == 0 && pkt[108] == 100);

    /* rejected arguments */
    CHECK(lj_sacn_pack_data(pkt, &cfg, 0, 0, 0, slots) == -1);
    CHECK(lj_sacn_pack_data(pkt, &cfg, 64000, 0, 0, slots) == -1);
    CHECK(lj_sacn_pack_data(pkt, &cfg, LJ_SACN_DISCOVERY_UNIVERSE, 0, 0, slots) == -1);
    CHECK(lj_sacn_pack_data(pkt, NULL, 1, 0, 0, slots) == -1);

    printf("test_sacn_packets: B-1 data (638), chase, terminated, discovery (126) byte-exact, %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
