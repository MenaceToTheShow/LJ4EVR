/* lj_net.c - NIC enumeration and the host name for the sACN sender (design 8.1,
 * 08 s6.2): GetAdaptersAddresses on Windows, getifaddrs elsewhere. Names come
 * back from the OS as UTF-16 / UTF-8 and are converted to CP1252 here (design 2.1). */
#define _DEFAULT_SOURCE 1   /* glibc under -std=c11: IFF_* of <net/if.h> and gethostname() need the default feature set */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "io/lj_net.h"
#include "core/lj_str.h"

#ifdef _WIN32
static int lj_wsa_up;

int lj_net_startup(void)
{
    WSADATA w;
    if (lj_wsa_up) return 0;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return -1;
    lj_wsa_up = 1;
    return 0;
}

void lj_net_cleanup(void)
{
    if (lj_wsa_up) WSACleanup();
    lj_wsa_up = 0;
}

int lj_net_list_ipv4(lj_nic *out, int max)
{
    IP_ADAPTER_ADDRESSES *list = NULL, *a;
    ULONG len = 16 * 1024, rc;
    int n = 0, tries;
    if (!out || max <= 0 || lj_net_startup() != 0) return -1;
    for (tries = 0; tries < 3; tries++) {                            /* the buffer may need to grow once (Microsoft's own sample loops) */
        list = (IP_ADAPTER_ADDRESSES *)malloc(len);
        if (!list) return -1;
        rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, list, &len);
        if (rc != ERROR_BUFFER_OVERFLOW) break;
        free(list); list = NULL;
    }
    if (rc != NO_ERROR) { free(list); return -1; }
    for (a = list; a && n < max; a = a->Next) {
        IP_ADAPTER_UNICAST_ADDRESS *u;
        for (u = a->FirstUnicastAddress; u && n < max; u = u->Next) {
            char utf8[192];
            const struct sockaddr_in *sa = (const struct sockaddr_in *)u->Address.lpSockaddr;
            lj_nic *nic = &out[n];
            if (sa->sin_family != AF_INET) continue;
            memset(nic, 0, sizeof *nic);
            if (WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, utf8, sizeof utf8, NULL, NULL) == 0) utf8[0] = 0;
            lj_utf8_to_cp1252(nic->name, sizeof nic->name, utf8);
            nic->ipv4 = ntohl(sa->sin_addr.s_addr);
            nic->mask = u->OnLinkPrefixLength >= 32 ? 0xFFFFFFFFu : u->OnLinkPrefixLength == 0 ? 0 : 0xFFFFFFFFu << (32 - u->OnLinkPrefixLength);
            nic->up = a->OperStatus == IfOperStatusUp;
            nic->loopback = a->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
            n++;
        }
    }
    free(list);
    return n;
}

#else

int lj_net_startup(void) { return 0; }
void lj_net_cleanup(void) { }

int lj_net_list_ipv4(lj_nic *out, int max)
{
    struct ifaddrs *list, *a;
    int n = 0;
    if (!out || max <= 0 || getifaddrs(&list) != 0) return -1;
    for (a = list; a && n < max; a = a->ifa_next) {
        lj_nic *nic = &out[n];
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        memset(nic, 0, sizeof *nic);
        lj_utf8_to_cp1252(nic->name, sizeof nic->name, a->ifa_name);
        nic->ipv4 = ntohl(((const struct sockaddr_in *)a->ifa_addr)->sin_addr.s_addr);
        nic->mask = a->ifa_netmask ? ntohl(((const struct sockaddr_in *)a->ifa_netmask)->sin_addr.s_addr) : 0;
        nic->up = (a->ifa_flags & IFF_UP) != 0 && (a->ifa_flags & IFF_RUNNING) != 0;
        nic->loopback = (a->ifa_flags & IFF_LOOPBACK) != 0;
        n++;
    }
    freeifaddrs(list);
    return n;
}

#endif

int lj_net_hostname(char *dst, size_t cap)
{
    char raw[256];
    if (lj_net_startup() != 0 || gethostname(raw, sizeof raw) != 0 || !raw[0]) { lj_str_copy(dst, cap, "host"); return -1; }
    raw[sizeof raw - 1] = 0;
    if (lj_utf8_to_cp1252(dst, cap, raw) != 0 && !dst[0]) { lj_str_copy(dst, cap, "host"); return -1; }   /* malformed bytes: '?' per code point is fine, an empty result is not */
    return 0;
}
