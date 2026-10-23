/* lj_remote.c - the remote protocol, TCP listener and serial port (io/lj_remote.h). W5 CONTRACT STUB: the remote track
 * replaces every body (docs/w5-contracts.md seam 5). Until then nothing listens and every frame is dropped. */
#include <string.h>
#include "io/lj_remote.h"
#include "io/lj_socket.h"
#include "core/lj_str.h"

void lj_remote_parser_init(lj_remote_parser *p) { if (p) memset(p, 0, sizeof *p); }
int  lj_remote_feed(lj_remote_parser *p, const uint8_t *bytes, size_t n, uint64_t now_us, lj_remote_msg *out, int max) { (void)p; (void)bytes; (void)n; (void)now_us; (void)out; (void)max; return 0; }
int  lj_remote_exec(const lj_remote_msg *m) { (void)m; return -1; }

void lj_remote_cfg_default(lj_remote_cfg *c)
{
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->tcp_ip = LJ_IP_LOOPBACK; c->tcp_port = LJ_REMOTE_TCP_PORT; c->baud = LJ_REMOTE_BAUD_DEFAULT;
    lj_str_copy(c->serial_port, sizeof c->serial_port, "COM1");
}
void lj_remote_cfg_load(lj_ini *hardware_ini, lj_remote_cfg *c) { (void)hardware_ini; lj_remote_cfg_default(c); }
void lj_remote_cfg_save(lj_ini *hardware_ini, const lj_remote_cfg *c) { (void)hardware_ini; (void)c; }
int  lj_remote_open(const lj_remote_cfg *c) { (void)c; return 0; }
void lj_remote_close(void) {}
int  lj_remote_poll(uint64_t now_us) { (void)now_us; return 0; }
int  lj_remote_inject(const uint8_t *bytes, size_t n, uint64_t now_us) { (void)bytes; (void)n; (void)now_us; return -1; }
