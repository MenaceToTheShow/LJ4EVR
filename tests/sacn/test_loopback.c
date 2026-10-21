/* test_loopback.c - `lj4evr_tests test_sacn_loopback`: the real socket path with a
 * receiver bound to 127.0.0.1:5568 (loopback cannot carry multicast on Windows, 08 s6.2,
 * so every destination is unicast 127.0.0.1) and the real clock:
 *   - open: 3 announce packets per universe across 3 pumps, sequence 0,1,2;
 *   - a change: 3 packets across 3 pumps with the new slots, the other universe silent;
 *   - keep-alive after ~0.9 s, same slots, sequence still incrementing;
 *   - remap (ME-23): 3 x Stream_Terminated for the dropped universe arrive before the
 *     first packet of the new one; the kept universe keeps its sequence;
 *   - close: 3 x Stream_Terminated per universe. Runs in CI on all three OSes. */
#define _POSIX_C_SOURCE 200809L   /* nanosleep under -std=c11 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#endif
#include <stdio.h>
#include <string.h>
#include "io/lj_sacn.h"
#include "io/lj_net.h"
#include "core/lj_clock.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct { uint16_t universe; uint8_t seq, options; uint8_t slot1; } lj_rx_pkt;

#ifdef _WIN32
static SOCKET lj_rx;
static void lj_sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
static int lj_rx;
static void lj_sleep_ms(int ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (long)(ms % 1000) * 1000000L; nanosleep(&ts, NULL); }
#endif

static int lj_rx_open(void)
{
    struct sockaddr_in a;
    int one = 1;
    lj_net_startup();
    lj_rx = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(lj_rx, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);   /* share 5568 with a monitor on the same box */
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(LJ_SACN_PORT); a.sin_addr.s_addr = htonl(0x7F000001u);
    return bind(lj_rx, (const struct sockaddr *)&a, sizeof a) == 0 ? 0 : -1;
}

static int lj_rx_recv(lj_rx_pkt *out, int timeout_ms)    /* 1 = packet, 0 = timeout */
{
    uint8_t buf[700];
    fd_set fds;
    struct timeval tv;
    int n;
    FD_ZERO(&fds); FD_SET(lj_rx, &fds);
    tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select((int)lj_rx + 1, &fds, NULL, NULL, &tv) <= 0) return 0;
    n = (int)recv(lj_rx, (char *)buf, sizeof buf, 0);
    if (n != LJ_SACN_DATA_LEN || memcmp(buf + 4, "ASC-E1.17", 9) != 0) return 0;
    out->universe = (uint16_t)(buf[113] << 8 | buf[114]);
    out->seq = buf[111]; out->options = buf[112]; out->slot1 = buf[126];
    return 1;
}

static int lj_rx_drain(lj_rx_pkt *out, int cap)          /* every packet already queued, up to cap */
{
    int n = 0;
    while (n < cap && lj_rx_recv(&out[n], 50)) n++;
    return n;
}

static int lj_count(const lj_rx_pkt *p, int n, uint16_t universe, uint8_t options)
{
    int i, c = 0;
    for (i = 0; i < n; i++) if (p[i].universe == universe && (p[i].options & 0x40) == options) c++;
    return c;
}

static void lj_pump3(uint8_t *slots)                      /* three pumps 25 ms apart (above the 22.7 ms cap) */
{
    int i;
    for (i = 0; i < 3; i++) { if (slots) lj_sacn_set(1, slots); lj_sacn_pump(lj_clock_us()); lj_sleep_ms(25); }
}

int lj_test_sacn_loopback(int argc, char **argv);
int lj_test_sacn_loopback(int argc, char **argv)
{
    lj_sacn_cfg cfg;
    lj_sacn_stats st;
    lj_rx_pkt p[32];
    uint8_t slots[LJ_LINK_CH];
    int n, i, first7, last_term2;
    (void)argc; (void)argv;

    if (lj_rx_open() != 0) { printf("test_sacn_loopback: cannot bind 127.0.0.1:%d\n", LJ_SACN_PORT); return 1; }
    lj_sacn_cfg_default(&cfg, "loop");
    memset(cfg.universe_of_link, 0, sizeof cfg.universe_of_link);
    cfg.universe_of_link[0] = 1; cfg.nunicast[0] = 1; cfg.unicast[0][0] = 0x7F000001u;
    cfg.universe_of_link[1] = 2; cfg.nunicast[1] = 1; cfg.unicast[1][0] = 0x7F000001u;
    CHECK(lj_sacn_open(&cfg) == 0);

    /* announce: 3 per universe, sequence 0,1,2 */
    lj_pump3(NULL);
    n = lj_rx_drain(p, 32);
    CHECK(n == 6 && lj_count(p, n, 1, 0) == 3 && lj_count(p, n, 2, 0) == 3);
    for (i = 0; i < n; i++) CHECK(p[i].seq == (i / 2) && p[i].slot1 == 0);

    /* change on universe 1: 3 packets across 3 pumps, universe 2 silent */
    memset(slots, 0, sizeof slots); slots[0] = 200;
    lj_pump3(slots);
    n = lj_rx_drain(p, 32);
    CHECK(n == 3 && lj_count(p, n, 1, 0) == 3 && lj_count(p, n, 2, 0) == 0);
    for (i = 0; i < n; i++) CHECK(p[i].slot1 == 200 && p[i].seq == 3 + i);

    /* keep-alive after ~0.9 s: one per universe, same slots, sequence continues */
    CHECK(lj_rx_drain(p, 32) == 0);
    lj_sleep_ms(900);
    lj_sacn_pump(lj_clock_us());
    n = lj_rx_drain(p, 32);
    CHECK(n == 2 && lj_count(p, n, 1, 0) == 1 && lj_count(p, n, 2, 0) == 1);
    for (i = 0; i < n; i++) CHECK(p[i].universe == 1 ? (p[i].slot1 == 200 && p[i].seq == 6) : (p[i].slot1 == 0 && p[i].seq == 3));

    /* remap link 2 from universe 2 to 7 (ME-23): 3 x Stream_Terminated for 2 before the first packet of 7; 1 untouched */
    cfg.universe_of_link[1] = 7;
    CHECK(lj_sacn_open(&cfg) == 0);
    lj_pump3(NULL);
    n = lj_rx_drain(p, 32);
    CHECK(lj_count(p, n, 2, 0x40) == 3 && lj_count(p, n, 7, 0) == 3 && lj_count(p, n, 1, 0) == 0);
    first7 = n; last_term2 = -1;
    for (i = 0; i < n; i++) { if (p[i].universe == 7 && i < first7) first7 = i; if (p[i].universe == 2 && (p[i].options & 0x40)) last_term2 = i; }
    CHECK(last_term2 >= 0 && last_term2 < first7);
    for (i = 0; i < n; i++) if (p[i].universe == 7) CHECK(p[i].seq == i - first7);
    lj_sacn_stats_get(&st);
    CHECK(st.terminated == 3 && st.universe[1] == 7);

    /* close: 3 x Stream_Terminated per mapped universe (1 and 7), sequence continues on 1 */
    lj_sacn_close();
    n = lj_rx_drain(p, 32);
    CHECK(n == 6 && lj_count(p, n, 1, 0x40) == 3 && lj_count(p, n, 7, 0x40) == 3);
    for (i = 0; i < n; i++) if (p[i].universe == 1) CHECK(p[i].seq >= 7);
    lj_sacn_stats_get(&st);
    CHECK(st.terminated == 9);
    CHECK(lj_rx_drain(p, 32) == 0);

#ifdef _WIN32
    closesocket(lj_rx);
#else
    close(lj_rx);
#endif
    printf("test_sacn_loopback: announce 6, change 3, keep-alive 2, remap 3 term + 3 new, close 6 term; %u send errors; %d failure(s)\n", st.send_errors, fails);
    return fails ? 1 : 0;
}
