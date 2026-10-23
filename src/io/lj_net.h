/* lj_net.h - NIC enumeration for the Hardware Setup NIC combo and the default
 * sACN source name (design 8.1, 08 s6.2): GetAdaptersAddresses / getifaddrs,
 * the only other platform split in io/. lj_net.c is on the OS-header allowlist. */
#ifndef LJ_IO_NET_H
#define LJ_IO_NET_H

#include <stddef.h>
#include <stdint.h>

typedef struct { char name[64]; uint32_t ipv4, mask; uint8_t up, loopback; } lj_nic;   /* name CP1252 (converted from the OS), addresses host byte order */

int lj_net_list_ipv4(lj_nic *out, int max);              /* returns the count (loopback included, flagged), -1 on error */
int lj_net_hostname(char *dst, size_t cap);              /* gethostname as CP1252; "host" when unavailable; 0 / -1 */
int lj_net_startup(void);                                /* WSAStartup on Windows (idempotent); 0 / -1 */
void lj_net_cleanup(void);
/* W6 WIZARD_STYLES-9: the one dotted-IPv4 parser and formatter (tools/test_layering.py counts any other copy). Strict:
 * four groups of 1-3 decimal digits, each 0..255, three dots, nothing before, between or after (no sign, no space, no
 * hex). Host byte order. *ip is written only on success. */
int  lj_net_ip_parse(const char *s, uint32_t *ip);       /* 0 / -1 */
void lj_net_ip_fmt(char *dst, size_t cap, uint32_t ip);  /* "a.b.c.d"; 16 bytes always fit; cut to cap */

#endif /* LJ_IO_NET_H */
