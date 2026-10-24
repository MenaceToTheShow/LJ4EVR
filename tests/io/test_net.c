/* test_net.c - W6 WIZARD_STYLES-9: lj_net_ip_parse / lj_net_ip_fmt (io/lj_net.h), the one dotted-IPv4 pair. Every
 * form sscanf("%u") used to accept (a sign, leading space, a trailing newline) is refused; *ip is untouched on refusal. */
#include <stdio.h>
#include <string.h>
#include "io/lj_net.h"

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
    printf("test_net: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
