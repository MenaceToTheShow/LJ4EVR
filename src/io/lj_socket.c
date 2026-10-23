/* lj_socket.c - non-blocking IPv4 UDP / TCP (io/lj_socket.h, docs/w5-contracts.md seam 3). On the OS_ALLOW list of
 * tools/test_layering.py. Test: tests/io/test_socket.c (loopback UDP and TCP, non-blocking empty reads). */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET lj_h;
typedef int lj_socklen;
#define LJ_H(s) ((SOCKET)(s))
#define LJ_WOULDBLOCK(e) ((e) == WSAEWOULDBLOCK)
#define LJ_INPROGRESS(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS)
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int lj_h;
typedef socklen_t lj_socklen;
#define LJ_H(s) ((int)(s))
#define LJ_WOULDBLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN)
#define LJ_INPROGRESS(e) ((e) == EINPROGRESS)
#endif
#include <string.h>
#include "io/lj_socket.h"
#include "io/lj_net.h"
#include "core/lj_log.h"

int lj_socket_error(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void lj_socket_close(lj_socket s)
{
    if (s == LJ_SOCKET_BAD) return;
#ifdef _WIN32
    closesocket(LJ_H(s));
#else
    close(LJ_H(s));
#endif
}

static int lj_sock_nonblock(lj_socket s)
{
#ifdef _WIN32
    u_long on = 1;
    return ioctlsocket(LJ_H(s), FIONBIO, &on) == 0 ? 0 : -1;
#else
    int fl = fcntl(LJ_H(s), F_GETFL, 0);
#ifdef SO_NOSIGPIPE
    { int on = 1; setsockopt(LJ_H(s), SOL_SOCKET, SO_NOSIGPIPE, &on, (lj_socklen)sizeof on); }   /* macOS: no MSG_NOSIGNAL; a dead peer must not SIGPIPE the show */
#endif
    return fl >= 0 && fcntl(LJ_H(s), F_SETFL, fl | O_NONBLOCK) == 0 ? 0 : -1;
#endif
}

static lj_socket lj_sock_wrap(lj_h h)
{
#ifdef _WIN32
    return h == INVALID_SOCKET ? LJ_SOCKET_BAD : (lj_socket)h;
#else
    return h < 0 ? LJ_SOCKET_BAD : (lj_socket)h;
#endif
}

static void lj_sock_addr(struct sockaddr_in *a, uint32_t ip, uint16_t port)
{
    memset(a, 0, sizeof *a);
    a->sin_family = AF_INET; a->sin_addr.s_addr = htonl(ip); a->sin_port = htons(port);
}

static lj_socket lj_sock_new(int type)
{
    lj_socket s;
    if (lj_net_startup() != 0) return LJ_SOCKET_BAD;
    s = lj_sock_wrap(socket(AF_INET, type, 0));
    if (s == LJ_SOCKET_BAD) { lj_log("socket: socket failed: %d", lj_socket_error()); return s; }
    if (lj_sock_nonblock(s) != 0) { lj_log("socket: non-blocking mode failed: %d", lj_socket_error()); lj_socket_close(s); return LJ_SOCKET_BAD; }
    return s;
}

static int lj_sock_bind(lj_socket s, uint32_t ip, uint16_t port, const char *what)
{
    struct sockaddr_in a;
    lj_sock_addr(&a, ip, port);
    if (bind(LJ_H(s), (const struct sockaddr *)&a, (lj_socklen)sizeof a) != 0) {
        lj_log("socket: %s bind %u.%u.%u.%u:%u failed: %d", what, ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u, (unsigned)port, lj_socket_error());
        return -1;
    }
    lj_log("socket: %s bound to %u.%u.%u.%u:%u", what, ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u, (unsigned)lj_socket_port(s));
    return 0;
}

lj_socket lj_socket_udp(uint32_t bind_ip, uint16_t port, unsigned flags)
{
    int on = 1;
    lj_socket s = lj_sock_new(SOCK_DGRAM);
    if (s == LJ_SOCKET_BAD) return s;
    if ((flags & LJ_SOCK_BROADCAST) && setsockopt(LJ_H(s), SOL_SOCKET, SO_BROADCAST, (const char *)&on, (lj_socklen)sizeof on) != 0) lj_log("socket: SO_BROADCAST failed: %d", lj_socket_error());
    if ((flags & LJ_SOCK_REUSE) && setsockopt(LJ_H(s), SOL_SOCKET, SO_REUSEADDR, (const char *)&on, (lj_socklen)sizeof on) != 0) lj_log("socket: SO_REUSEADDR failed: %d", lj_socket_error());
    if (lj_sock_bind(s, bind_ip, port, "udp") != 0) { lj_socket_close(s); return LJ_SOCKET_BAD; }
    return s;
}

int lj_socket_join(lj_socket s, uint32_t group, uint32_t nic_ip)
{
    struct ip_mreq m;
    memset(&m, 0, sizeof m);
    m.imr_multiaddr.s_addr = htonl(group); m.imr_interface.s_addr = htonl(nic_ip);
    if (setsockopt(LJ_H(s), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&m, (lj_socklen)sizeof m) != 0) { lj_log("socket: IP_ADD_MEMBERSHIP failed: %d", lj_socket_error()); return -1; }
    return 0;
}

int lj_socket_recvfrom(lj_socket s, uint8_t *buf, size_t cap, uint32_t *ip, uint16_t *port)
{
    struct sockaddr_in a;
    lj_socklen al = (lj_socklen)sizeof a;
    int n, e;
    if (s == LJ_SOCKET_BAD || !buf || cap == 0 || cap > 65535) return -1;
    memset(&a, 0, sizeof a);
    n = (int)recvfrom(LJ_H(s), (char *)buf, (int)cap, 0, (struct sockaddr *)&a, &al);
    if (n < 0) {
        e = lj_socket_error();
        if (LJ_WOULDBLOCK(e)) return 0;
#ifdef _WIN32
        if (e != WSAEMSGSIZE) return -1;
        n = (int)cap;                                      /* Windows fills the buffer and reports the truncation */
#else
        return -1;
#endif
    }
    if (ip) *ip = ntohl(a.sin_addr.s_addr);
    if (port) *port = ntohs(a.sin_port);
    return n;
}

int lj_socket_sendto(lj_socket s, const uint8_t *buf, size_t n, uint32_t ip, uint16_t port)
{
    struct sockaddr_in a;
    if (s == LJ_SOCKET_BAD || !buf || n > 65507) return -1;
    lj_sock_addr(&a, ip, port);
    return sendto(LJ_H(s), (const char *)buf, (int)n, 0, (const struct sockaddr *)&a, (lj_socklen)sizeof a) == (int)n ? 0 : -1;
}

lj_socket lj_socket_listen(uint32_t bind_ip, uint16_t port, int backlog)
{
    lj_socket s = lj_sock_new(SOCK_STREAM);
    if (s == LJ_SOCKET_BAD) return s;
#ifndef _WIN32
    { int on = 1; setsockopt(LJ_H(s), SOL_SOCKET, SO_REUSEADDR, (const char *)&on, (lj_socklen)sizeof on); }   /* POSIX TIME_WAIT rebind; on Windows it would allow port stealing */
#endif
    if (lj_sock_bind(s, bind_ip, port, "tcp") != 0) { lj_socket_close(s); return LJ_SOCKET_BAD; }
    if (listen(LJ_H(s), backlog > 0 ? backlog : 1) != 0) { lj_log("socket: listen failed: %d", lj_socket_error()); lj_socket_close(s); return LJ_SOCKET_BAD; }
    return s;
}

lj_socket lj_socket_accept(lj_socket s, uint32_t *peer_ip)
{
    struct sockaddr_in a;
    lj_socklen al = (lj_socklen)sizeof a;
    lj_socket c;
    if (s == LJ_SOCKET_BAD) return LJ_SOCKET_BAD;
    memset(&a, 0, sizeof a);
    c = lj_sock_wrap(accept(LJ_H(s), (struct sockaddr *)&a, &al));
    if (c == LJ_SOCKET_BAD) return c;
    if (lj_sock_nonblock(c) != 0) { lj_socket_close(c); return LJ_SOCKET_BAD; }
    if (peer_ip) *peer_ip = ntohl(a.sin_addr.s_addr);
    return c;
}

lj_socket lj_socket_connect(uint32_t ip, uint16_t port)
{
    struct sockaddr_in a;
    lj_socket s = lj_sock_new(SOCK_STREAM);
    if (s == LJ_SOCKET_BAD) return s;
    lj_sock_addr(&a, ip, port);
    if (connect(LJ_H(s), (const struct sockaddr *)&a, (lj_socklen)sizeof a) != 0 && !LJ_INPROGRESS(lj_socket_error())) { lj_socket_close(s); return LJ_SOCKET_BAD; }
    return s;
}

int lj_socket_connected(lj_socket s)
{
    fd_set w, x;
    struct timeval tv;
    int err = 0;
    lj_socklen el = (lj_socklen)sizeof err;
    if (s == LJ_SOCKET_BAD) return -1;
    tv.tv_sec = 0; tv.tv_usec = 0;
    FD_ZERO(&w); FD_ZERO(&x);
    FD_SET(LJ_H(s), &w); FD_SET(LJ_H(s), &x);
    if (select((int)(LJ_H(s) + 1), NULL, &w, &x, &tv) < 0) return -1;
    if (FD_ISSET(LJ_H(s), &x)) return -1;                 /* Windows reports a failed connect in the except set */
    if (!FD_ISSET(LJ_H(s), &w)) return 0;
    if (getsockopt(LJ_H(s), SOL_SOCKET, SO_ERROR, (char *)&err, &el) != 0 || err != 0) return -1;
    return 1;
}

int lj_socket_recv(lj_socket s, uint8_t *buf, size_t cap)
{
    int n;
    if (s == LJ_SOCKET_BAD || !buf || cap == 0 || cap > 65535) return -1;
    n = (int)recv(LJ_H(s), (char *)buf, (int)cap, 0);
    if (n > 0) return n;
    if (n == 0) return -1;                                 /* orderly close by the peer */
    return LJ_WOULDBLOCK(lj_socket_error()) ? 0 : -1;
}

int lj_socket_send(lj_socket s, const uint8_t *buf, size_t n)
{
    int k;
    if (s == LJ_SOCKET_BAD || !buf || n > 65535) return -1;
    if (n == 0) return 0;
#ifdef MSG_NOSIGNAL
    k = (int)send(LJ_H(s), (const char *)buf, (int)n, MSG_NOSIGNAL);   /* a dead peer must not SIGPIPE the show (Linux) */
#else
    k = (int)send(LJ_H(s), (const char *)buf, (int)n, 0);
#endif
    if (k >= 0) return k;
    return LJ_WOULDBLOCK(lj_socket_error()) ? 0 : -1;
}

uint16_t lj_socket_port(lj_socket s)
{
    struct sockaddr_in a;
    lj_socklen al = (lj_socklen)sizeof a;
    if (s == LJ_SOCKET_BAD) return 0;
    memset(&a, 0, sizeof a);
    if (getsockname(LJ_H(s), (struct sockaddr *)&a, &al) != 0) return 0;
    return ntohs(a.sin_port);
}
