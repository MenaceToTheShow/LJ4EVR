/* lj_serial.h - a non-blocking serial port for the remote (io/lj_remote.h) and External Application output
 * (io/lj_extapp.h) (note 05 s10; docs/w5-contracts.md seam 5; track remote). One backend per OS, each on the
 * tools/test_layering.py OS_ALLOW list (W5-X-OS): lj_serial_win.c (CreateFile / SetCommState / COMMTIMEOUTS with
 * ReadIntervalTimeout = MAXDWORD so ReadFile returns at once) and lj_serial_posix.c (open O_NONBLOCK | O_NOCTTY,
 * termios raw); CMake compiles exactly one. Format fixed 8 data bits, 1 stop bit, no parity (05 s10.1). A port is
 * used by one thread (the UI loop); nothing opens a port by itself (W5-X-OFF). The Linux CI tests drive the posix
 * backend through a pty (W5-X-CI). */
#ifndef LJ_IO_SERIAL_H
#define LJ_IO_SERIAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct lj_serial lj_serial;
enum { LJ_SERIAL_NAME = 64, LJ_SERIAL_PORTS_MAX = 32 };
typedef struct { char name[LJ_SERIAL_NAME]; } lj_serial_port;   /* "COM1" .. / "/dev/ttyUSB0" ..; CP1252 */

int        lj_serial_list(lj_serial_port *out, int max);          /* the ports present (Windows: COM1..COM9 that open, as LJ2's COM 1-9; POSIX: /dev/tty.* and /dev/ttyS*, ttyUSB*, ttyACM*); count, -1 on error */
lj_serial *lj_serial_open(const char *name, uint32_t baud);       /* 8N1, non-blocking; NULL + log (an unsupported baud is refused) */
int        lj_serial_read(lj_serial *s, uint8_t *buf, size_t cap);   /* bytes read, 0 = none pending, -1 = error (the port went away) */
int        lj_serial_write(lj_serial *s, const uint8_t *buf, size_t n);   /* bytes written (may be < n), -1 = error */
void       lj_serial_close(lj_serial *s);                         /* NULL is a no-op */

#endif /* LJ_IO_SERIAL_H */
