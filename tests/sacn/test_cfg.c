/* test_cfg.c - `lj4evr_tests test_sacn_cfg`: lj_sacn_cfg_validate clamps and rejections
 * (ME-24, LORD_ENGINE-15), lj_sacn_cfg_default, lj_sacn_cid_new (v4 bits, distinct),
 * and the Hardware.Ini [SACN] load/save round trip including the missing-CID -1. */
#define _CRT_SECURE_NO_WARNINGS 1   /* strcpy under /W4 /WX on MSVC (as core/lj_ini.c) */
#include <stdio.h>
#include <string.h>
#include "io/lj_sacn.h"
#include "core/lj_ini.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_sacn_cfg(int argc, char **argv);
int lj_test_sacn_cfg(int argc, char **argv)
{
    lj_sacn_cfg c, d;
    uint8_t cid[16];
    lj_ini *ini;
    int i;
    (void)argc; (void)argv;

    /* defaults */
    lj_sacn_cfg_default(&c, "box");
    CHECK(strcmp(c.name, "LJ4EVR box") == 0 && c.priority == 100 && c.ttl == 1 && c.preview == 0 && c.unlock_rate == 0 && c.nic_ipv4 == 0);
    for (i = 0; i < LJ_LINKS; i++) CHECK(c.universe_of_link[i] == i + 1 && c.nunicast[i] == 0);
    CHECK((c.cid[6] & 0xF0) == 0x40 && (c.cid[8] & 0xC0) == 0x80);          /* RFC 4122 v4 */
    lj_sacn_cid_new(cid);
    CHECK(memcmp(cid, c.cid, 16) != 0 && (cid[6] & 0xF0) == 0x40);
    CHECK(lj_sacn_cfg_validate(&c) == NULL);

    /* one row per clamp */
    c.priority = 250;            CHECK(lj_sacn_cfg_validate(&c) == NULL && c.priority == 200);
    c.ttl = 0;                   CHECK(lj_sacn_cfg_validate(&c) == NULL && c.ttl == 1);
    memset(c.name, 'n', 64);     CHECK(lj_sacn_cfg_validate(&c) == NULL && c.name[63] == 0 && c.name[62] == 'n');
    c.nunicast[0] = 9;           CHECK(lj_sacn_cfg_validate(&c) == NULL && c.nunicast[0] == 8);
    c.nunicast[1] = -1;          CHECK(lj_sacn_cfg_validate(&c) == NULL && c.nunicast[1] == 0);
    /* one row per rejection: the message comes back and the link is left unmapped */
    c.universe_of_link[2] = 64000;  CHECK(lj_sacn_cfg_validate(&c) != NULL && c.universe_of_link[2] == 0 && c.universe_of_link[3] == 4);
    c.universe_of_link[2] = LJ_SACN_DISCOVERY_UNIVERSE;  CHECK(lj_sacn_cfg_validate(&c) != NULL && c.universe_of_link[2] == 0);
    c.universe_of_link[2] = 65535;  CHECK(lj_sacn_cfg_validate(&c) != NULL && c.universe_of_link[2] == 0);
    c.universe_of_link[2] = 63999;  CHECK(lj_sacn_cfg_validate(&c) == NULL && c.universe_of_link[2] == 63999);
    c.universe_of_link[3] = 1;      CHECK(lj_sacn_cfg_validate(&c) != NULL && c.universe_of_link[0] == 1 && c.universe_of_link[3] == 0);   /* duplicate: the later link loses */
    c.universe_of_link[3] = 0;      CHECK(lj_sacn_cfg_validate(&c) == NULL);   /* unmapped links are fine, two of them too */
    c.universe_of_link[2] = 0;      CHECK(lj_sacn_cfg_validate(&c) == NULL);
    CHECK(lj_sacn_cfg_validate(NULL) != NULL);

    /* load from an empty ini: defaults, -1 because the CID is absent (the app generates and saves) */
    ini = lj_ini_new();
    CHECK(lj_sacn_cfg_load(ini, &d) == -1);
    CHECK(d.priority == 100 && d.universe_of_link[0] == 1 && d.universe_of_link[3] == 4 && strncmp(d.name, "LJ4EVR ", 7) == 0);

    /* save / load round trip */
    lj_sacn_cfg_default(&c, "box");
    strcpy(c.name, "Caf\xE9 rig");                    /* CP1252 stays CP1252 in the ini */
    c.nic_ipv4 = 0xC0A80A05u;                        /* 192.168.10.5 */
    c.priority = 150; c.ttl = 4; c.preview = 1; c.unlock_rate = 1;
    c.universe_of_link[1] = 7; c.universe_of_link[2] = 0; c.universe_of_link[3] = 63999;
    c.nunicast[0] = 2; c.unicast[0][0] = 0x7F000001u; c.unicast[0][1] = 0x0A000001u;
    c.nunicast[3] = 1; c.unicast[3][0] = 0xC0A80101u;
    lj_sacn_cfg_save(ini, &c);
    CHECK(strcmp(lj_ini_gets(ini, "SACN", "Nic", ""), "192.168.10.5") == 0);
    CHECK(strcmp(lj_ini_gets(ini, "SACN", "Unicast1", ""), "127.0.0.1,10.0.0.1") == 0);
    CHECK(strlen(lj_ini_gets(ini, "SACN", "CID", "")) == 32);
    memset(&d, 0xAA, sizeof d);
    CHECK(lj_sacn_cfg_load(ini, &d) == 0);
    CHECK(memcmp(d.cid, c.cid, 16) == 0 && strcmp(d.name, c.name) == 0 && d.nic_ipv4 == c.nic_ipv4);
    CHECK(d.priority == 150 && d.ttl == 4 && d.preview == 1 && d.unlock_rate == 1);
    for (i = 0; i < LJ_LINKS; i++) CHECK(d.universe_of_link[i] == c.universe_of_link[i] && d.nunicast[i] == c.nunicast[i]);
    CHECK(d.unicast[0][0] == 0x7F000001u && d.unicast[0][1] == 0x0A000001u && d.unicast[3][0] == 0xC0A80101u);

    /* a hand-edited ini: out-of-range values are clamped / unmapped at load (the trust boundary) */
    lj_ini_seti(ini, "SACN", "Priority", 300);
    lj_ini_seti(ini, "SACN", "U2", 70000);
    lj_ini_sets(ini, "SACN", "Unicast4", "1.2.3.4,999.1.1.1,5.6.7.8,x");
    lj_ini_sets(ini, "SACN", "CID", "not-a-cid");
    CHECK(lj_sacn_cfg_load(ini, &d) == -1);         /* bad CID = absent */
    CHECK(d.priority == 200 && d.universe_of_link[1] == 0 && d.universe_of_link[3] == 63999);
    CHECK(d.nunicast[3] == 2 && d.unicast[3][0] == 0x01020304u && d.unicast[3][1] == 0x05060708u);
    lj_ini_free(ini);

    printf("test_sacn_cfg: 5 clamps, 5 rejections, defaults, CID v4, ini round trip, %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
