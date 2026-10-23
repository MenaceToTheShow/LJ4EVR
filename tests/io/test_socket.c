/* test_socket.c - `lj4evr_tests test_socket`: the W5 socket seam (io/lj_socket.h) on loopback only (W5-X-CI): a UDP
 * datagram both ways with the sender's address, an empty non-blocking read returning 0, an oversize datagram
 * truncated to the buffer, TCP listen / connect / accept / send / recv, the peer's close seen as -1. */
#include <stdio.h>
#include <string.h>
#include "io/lj_socket.h"
#include "core/lj_clock.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int wait_recvfrom(lj_socket s, uint8_t *buf, size_t cap, uint32_t *ip, uint16_t *port)
{
    uint64_t t0 = lj_clock_us();
    int n;
    while ((n = lj_socket_recvfrom(s, buf, cap, ip, port)) == 0 && lj_clock_us() - t0 < 2000000u) {}
    return n;
}

static int wait_recv(lj_socket s, uint8_t *buf, size_t cap)
{
    uint64_t t0 = lj_clock_us();
    int n;
    while ((n = lj_socket_recv(s, buf, cap)) == 0 && lj_clock_us() - t0 < 2000000u) {}
    return n;
}

int lj_test_socket(int argc, char **argv)
{
    uint8_t buf[64], big[200];
    uint32_t ip = 0; uint16_t port = 0, pa, pb;
    lj_socket a, b, l, c, s = LJ_SOCKET_BAD;
    uint64_t t0;
    int n, k;
    (void)argc; (void)argv;
    a = lj_socket_udp(LJ_IP_LOOPBACK, 0, 0);
    b = lj_socket_udp(LJ_IP_LOOPBACK, 0, LJ_SOCK_REUSE);
    CHECK(a != LJ_SOCKET_BAD && b != LJ_SOCKET_BAD);
    pa = lj_socket_port(a); pb = lj_socket_port(b);
    CHECK(pa != 0 && pb != 0 && pa != pb);
    CHECK(lj_socket_recvfrom(b, buf, sizeof buf, &ip, &port) == 0);           /* nothing pending: 0, never blocks */
    CHECK(lj_socket_sendto(a, (const uint8_t *)"hello", 5, LJ_IP_LOOPBACK, pb) == 0);
    n = wait_recvfrom(b, buf, sizeof buf, &ip, &port);
    CHECK(n == 5 && memcmp(buf, "hello", 5) == 0 && ip == LJ_IP_LOOPBACK && port == pa);
    memset(big, 0x5A, sizeof big);
    CHECK(lj_socket_sendto(b, big, sizeof big, LJ_IP_LOOPBACK, pa) == 0);
    n = wait_recvfrom(a, buf, 32, NULL, NULL);                               /* longer than the buffer: cap, truncated (the parsers size one past the largest valid packet) */
    CHECK(n == 32 && buf[31] == 0x5A);
    CHECK(lj_socket_recvfrom(a, buf, sizeof buf, NULL, NULL) == 0);           /* the rest of that datagram is gone */
    lj_socket_close(a); lj_socket_close(b);

    l = lj_socket_listen(LJ_IP_LOOPBACK, 0, 2);
    CHECK(l != LJ_SOCKET_BAD);
    CHECK(lj_socket_accept(l, NULL) == LJ_SOCKET_BAD);                        /* none pending */
    c = lj_socket_connect(LJ_IP_LOOPBACK, lj_socket_port(l));
    CHECK(c != LJ_SOCKET_BAD);
    for (t0 = lj_clock_us(); s == LJ_SOCKET_BAD && lj_clock_us() - t0 < 2000000u;) s = lj_socket_accept(l, &ip);
    CHECK(s != LJ_SOCKET_BAD && ip == LJ_IP_LOOPBACK);
    for (t0 = lj_clock_us(); (k = lj_socket_connected(c)) == 0 && lj_clock_us() - t0 < 2000000u;) {}
    CHECK(k == 1);
    CHECK(lj_socket_recv(s, buf, sizeof buf) == 0);                           /* connected, nothing sent yet */
    CHECK(lj_socket_send(c, (const uint8_t *)"\x81\x01\x02", 3) == 3);        /* the remote's cue 130 frame (05 s10.1) */
    n = wait_recv(s, buf, sizeof buf);
    CHECK(n == 3 && buf[0] == 0x81 && buf[1] == 1 && buf[2] == 2);
    lj_socket_close(c);
    CHECK(wait_recv(s, buf, sizeof buf) == -1);                               /* the peer closed */
    lj_socket_close(s); lj_socket_close(l);
    lj_socket_close(LJ_SOCKET_BAD);                                           /* no-op */
    CHECK(lj_socket_connected(LJ_SOCKET_BAD) == -1 && lj_socket_port(LJ_SOCKET_BAD) == 0);
    printf("test_socket: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
