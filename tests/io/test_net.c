/* test_net.c - W6 WIZARD_STYLES-9: lj_net_ip_parse / lj_net_ip_fmt (io/lj_net.h), the one dotted-IPv4 pair. Every
 * form sscanf("%u") used to accept (a sign, leading space, a trailing newline) is refused; *ip is untouched on refusal.
 * ME-197: a hand-edited Hardware.Ini Nic / UnicastN / [DMXIN] Nic that the strict parser refuses logs one line naming
 * the key, and the value stays unset. */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "io/lj_dmxin.h"
#include "io/lj_net.h"
#include "io/lj_sacn.h"

#define NCHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static char net_logged[1024];
static void net_sink(void *ud, const char *line)
{
    size_t n = strlen(net_logged);
    (void)ud;
    if (n + strlen(line) + 2 < sizeof net_logged) { memcpy(net_logged + n, line, strlen(line)); net_logged[n + strlen(line)] = '\n'; net_logged[n + strlen(line) + 1] = 0; }
}

static int refused_logged(void)
{
    static lj_sacn_cfg s;
    lj_dmxin_cfg d;
    lj_ini *ini = lj_ini_new();
    int fails = 0;
    if (!ini) return 1;
    lj_ini_sets(ini, "SACN", "Nic", " 10.0.0.5");                    /* "Nic= 10.0.0.5": the space after '=' */
    lj_ini_sets(ini, "SACN", "Unicast1", "10.0.0.6, 10.0.0.7");      /* the second entry has a space */
    lj_ini_sets(ini, "DMXIN", "Nic", " 10.0.0.5");
    net_logged[0] = 0;
    lj_log_set_sink(net_sink, NULL);
    lj_sacn_cfg_load(ini, &s);
    lj_dmxin_cfg_load(ini, &d);
    lj_log_set_sink(NULL, NULL);
    NCHECK(s.nic_ipv4 == 0 && strstr(net_logged, "[SACN] Nic is not a dotted IPv4 address") != NULL);
    NCHECK(s.nunicast[0] == 1 && s.unicast[0][0] == 0x0A000006u && strstr(net_logged, "[SACN] Unicast1: an entry is not a dotted IPv4 address") != NULL);
    NCHECK(d.nic_ipv4 == 0 && strstr(net_logged, "[DMXIN] Nic is not a dotted IPv4 address") != NULL);
    printf("  refused: Nic= 10.0.0.5 -> nic_ipv4 %u, logged:\n%s", (unsigned)s.nic_ipv4, net_logged);
    net_logged[0] = 0;
    lj_ini_sets(ini, "SACN", "Nic", ""); lj_ini_sets(ini, "SACN", "Unicast1", ""); lj_ini_sets(ini, "DMXIN", "Nic", "");
    lj_log_set_sink(net_sink, NULL);
    lj_sacn_cfg_load(ini, &s);
    lj_dmxin_cfg_load(ini, &d);
    lj_log_set_sink(NULL, NULL);
    NCHECK(strstr(net_logged, "dotted") == NULL);                    /* an empty key is no refusal */
    lj_ini_free(ini);
    return fails;
}

int lj_test_net(int argc, char **argv);
int lj_test_net(int argc, char **argv)
{
    static const char *const bad[] = { "+127.0.0.1", "127.+0.0.1", " 127.0.0.1", "127.0.0.1 ", "127.0.0.1\n", "-1.0.0.1",
                                       "256.1.1.1", "1.2.3", "1.2.3.4.5", "1..2.3", "1.2.3.", ".1.2.3", "0001.2.3.4",
                                       "0x7f.0.0.1", "host", "", "1.2.3.4x" };
    char buf[16], tiny[4];
    uint32_t ip = 0;
    int fails = 0, i;
    (void)argc; (void)argv;
#define NCHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
    NCHECK(lj_net_ip_parse("127.0.0.1", &ip) == 0 && ip == 0x7F000001u);
    NCHECK(lj_net_ip_parse("0.0.0.0", &ip) == 0 && ip == 0);
    NCHECK(lj_net_ip_parse("255.255.255.255", &ip) == 0 && ip == 0xFFFFFFFFu);
    NCHECK(lj_net_ip_parse("192.000.002.010", &ip) == 0 && ip == 0xC000020Au);   /* TEST-NET-1; decimal, never octal */
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        ip = 0x12345678u;
        if (lj_net_ip_parse(bad[i], &ip) != -1 || ip != 0x12345678u) { printf("FAIL: \"%s\" accepted\n", bad[i]); fails++; }
    }
    NCHECK(lj_net_ip_parse(NULL, &ip) == -1);
    lj_net_ip_fmt(buf, sizeof buf, 0xFFFFFFFFu); NCHECK(strcmp(buf, "255.255.255.255") == 0);
    lj_net_ip_fmt(buf, sizeof buf, 0x7F000001u); NCHECK(strcmp(buf, "127.0.0.1") == 0);
    lj_net_ip_fmt(tiny, sizeof tiny, 0x7F000001u); NCHECK(strcmp(tiny, "127") == 0);   /* cut to cap, terminated */
    NCHECK(lj_net_ip_parse(buf, &ip) == 0 && ip == 0x7F000001u);                       /* round trip */
    printf("  ip: %d hostile forms refused (\"+127.0.0.1\" first), fmt round trip\n", (int)(sizeof bad / sizeof bad[0]));
    fails += refused_logged();
    printf("test_net: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
