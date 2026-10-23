/* lj_serial_win.c - the Windows serial backend of io/lj_serial.h (OS_ALLOW: windows.h). W5 CONTRACT STUB: the remote
 * track replaces every body (docs/w5-contracts.md seam 5). Until then no port is listed or opened. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "io/lj_serial.h"

struct lj_serial { HANDLE h; };

int        lj_serial_list(lj_serial_port *out, int max) { (void)out; (void)max; return 0; }
lj_serial *lj_serial_open(const char *name, uint32_t baud) { (void)name; (void)baud; return NULL; }
int        lj_serial_read(lj_serial *s, uint8_t *buf, size_t cap) { (void)s; (void)buf; (void)cap; return -1; }
int        lj_serial_write(lj_serial *s, const uint8_t *buf, size_t n) { (void)s; (void)buf; (void)n; return -1; }
void       lj_serial_close(lj_serial *s) { (void)s; }
