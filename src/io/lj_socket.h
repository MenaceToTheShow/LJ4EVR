/* lj_socket.h - the W5 socket seam (docs/w5-contracts.md seam 3): non-blocking IPv4 UDP and TCP for Art-Net
 * (io/lj_artnet.c), the sACN sender (io/lj_sacn.c, W6 WIZARD_STYLES-10) and receiver (io/lj_dmxin.c), the remote
 * listener (io/lj_remote.c) and the External Application TCP client (io/lj_extapp.c). lj_socket.c is the one socket
 * file on the tools/test_layering.py OS_ALLOW list (W5-X-OS). Every handle is non-blocking, so no call here
 * blocks the 10 ms UI wake or the engine tick (W5-X-THREAD). Addresses and ports are host byte order. A handle is
 * used by one thread only (the module that opened it says which). Every bind logs one line naming the address and
 * port (W5-X-OFF); nothing in this file opens anything by itself. */
#ifndef LJ_IO_SOCKET_H
#define LJ_IO_SOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef intptr_t lj_socket;                  /* SOCKET on Windows, an fd elsewhere */
#define LJ_SOCKET_BAD ((lj_socket)-1)
#define LJ_IP_LOOPBACK 0x7F000001u           /* 127.0.0.1: the default bind of every listener (brief item 5) */
enum { LJ_SOCK_BROADCAST = 1 /* SO_BROADCAST (Art-Net directed broadcast) */, LJ_SOCK_REUSE = 2 /* SO_REUSEADDR (sACN 5568 beside another receiver) */ };

lj_socket lj_socket_udp(uint32_t bind_ip, uint16_t port, unsigned flags);   /* socket + flags + bind (port 0 = ephemeral); LJ_SOCKET_BAD + log */
int  lj_socket_join(lj_socket s, uint32_t group, uint32_t nic_ip);          /* IP_ADD_MEMBERSHIP of group on nic_ip (0 = default); 0 / -1 + log */
int  lj_socket_mcast_out(lj_socket s, uint32_t nic_ip, uint8_t ttl);        /* sender: IP_MULTICAST_IF nic_ip (0 = the OS default, left unset) and IP_MULTICAST_TTL; 0 / -1 + log per option */
int  lj_socket_recvfrom(lj_socket s, uint8_t *buf, size_t cap, uint32_t *ip, uint16_t *port);   /* one datagram: its length (a longer one returns cap, truncated: size the buffer one past the largest valid packet), 0 = nothing pending, -1 = error */
int  lj_socket_sendto(lj_socket s, const uint8_t *buf, size_t n, uint32_t ip, uint16_t port);   /* 0 / -1 (lj_socket_error has the code; never fatal) */

lj_socket lj_socket_listen(uint32_t bind_ip, uint16_t port, int backlog);   /* TCP listener; LJ_SOCKET_BAD + log */
lj_socket lj_socket_accept(lj_socket s, uint32_t *peer_ip);                 /* a pending connection, made non-blocking; LJ_SOCKET_BAD when none */
lj_socket lj_socket_connect(uint32_t ip, uint16_t port);                    /* starts a non-blocking connect; LJ_SOCKET_BAD on an immediate failure */
int  lj_socket_connected(lj_socket s);                                      /* 1 connected, 0 still connecting, -1 failed */
int  lj_socket_recv(lj_socket s, uint8_t *buf, size_t cap);                 /* TCP bytes read, 0 = nothing pending, -1 = closed by the peer or error */
int  lj_socket_send(lj_socket s, const uint8_t *buf, size_t n);             /* TCP bytes sent (may be < n), 0 = would block, -1 = error */
uint16_t lj_socket_port(lj_socket s);                                       /* the bound local port (tests bind port 0), 0 on error */
void lj_socket_close(lj_socket s);                                          /* LJ_SOCKET_BAD is a no-op */
int  lj_socket_error(void);                                                 /* the last WSAGetLastError / errno */

#endif /* LJ_IO_SOCKET_H */
