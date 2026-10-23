/* lj_serial_posix.c - the macOS / Linux serial backend of io/lj_serial.h (OS_ALLOW: termios.h, fcntl.h, unistd.h).
 * open O_NONBLOCK | O_NOCTTY on a /dev tty only (a hand-edited Hardware.Ini cannot point it at a file), termios raw
 * 8N1 with no flow control, VMIN = VTIME = 0. Test: test_serial_pty (tests/io/test_remote.c) on a pty. */
#define _DEFAULT_SOURCE 1   /* glibc under -std=c11: cfmakeraw and the B* baud constants */
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io/lj_serial.h"
#include "core/lj_log.h"

struct lj_serial { int fd; struct termios saved; };

static const struct { uint32_t baud; speed_t sp; } lj_ser_bauds[] = {
    { 1200, B1200 }, { 2400, B2400 }, { 4800, B4800 }, { 9600, B9600 },
#ifdef B14400
    { 14400, B14400 },                                               /* macOS has it; Linux termios does not (refused there, logged) */
#endif
    { 19200, B19200 }, { 38400, B38400 }, { 57600, B57600 }, { 115200, B115200 } };

int lj_serial_list(lj_serial_port *out, int max)
{
    static const char *const pre[] = { "ttyUSB", "ttyACM", "cu.", "ttyS" };   /* USB adapters first; ttyS* are listed only 0..3 (Linux makes 32 of them) */
    DIR *d;
    struct dirent *e;
    size_t p;
    int n = 0;
    if (!out || max <= 0) return 0;
    if (max > LJ_SERIAL_PORTS_MAX) max = LJ_SERIAL_PORTS_MAX;
    for (p = 0; p < sizeof pre / sizeof pre[0]; p++) {
        if ((d = opendir("/dev")) == NULL) return -1;
        while (n < max && (e = readdir(d)) != NULL) {
            size_t l = strlen(e->d_name), k = strlen(pre[p]);
            if (l <= k || l + 5 >= LJ_SERIAL_NAME || strncmp(e->d_name, pre[p], k) != 0) continue;
            if (p == 3 && (l != 5 || e->d_name[4] < '0' || e->d_name[4] > '3')) continue;
            snprintf(out[n].name, LJ_SERIAL_NAME, "/dev/%s", e->d_name);
            n++;
        }
        closedir(d);
    }
    return n;
}

lj_serial *lj_serial_open(const char *name, uint32_t baud)
{
    struct termios t;
    lj_serial *s;
    size_t i;
    speed_t sp = 0;
    int fd, ok = 0;
    for (i = 0; i < sizeof lj_ser_bauds / sizeof lj_ser_bauds[0]; i++) if (lj_ser_bauds[i].baud == baud) { sp = lj_ser_bauds[i].sp; ok = 1; }
    if (!name || strncmp(name, "/dev/", 5) != 0 || strstr(name, "..") || strlen(name) >= LJ_SERIAL_NAME) { lj_log("serial: %.63s is not a /dev device", name ? name : "(null)"); return NULL; }
    if (!ok) { lj_log("serial: %u baud not supported", (unsigned)baud); return NULL; }
    if ((fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) { lj_log("serial: cannot open %s: %d", name, errno); return NULL; }
    if (!isatty(fd) || tcgetattr(fd, &t) != 0) { lj_log("serial: %s is not a terminal device", name); close(fd); return NULL; }
    if ((s = (lj_serial *)calloc(1, sizeof *s)) == NULL) { close(fd); return NULL; }
    s->fd = fd; s->saved = t;
    cfmakeraw(&t);
    t.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CSIZE);
    t.c_cflag |= CS8 | CLOCAL | CREAD;                               /* 8N1 fixed (05 s10.1) */
#ifdef CRTSCTS
    t.c_cflag &= (tcflag_t)~CRTSCTS;
#endif
    t.c_iflag &= (tcflag_t)~(IXON | IXOFF | IXANY);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    if (cfsetispeed(&t, sp) != 0 || cfsetospeed(&t, sp) != 0 || tcsetattr(fd, TCSANOW, &t) != 0) { lj_log("serial: setting %s failed: %d", name, errno); close(fd); free(s); return NULL; }
    return s;
}

int lj_serial_read(lj_serial *s, uint8_t *buf, size_t cap)
{
    ssize_t n;
    if (!s || !buf) return -1;
    if (cap > 65536) cap = 65536;
    n = read(s->fd, buf, cap);
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 0 : -1;
    return (int)n;
}

int lj_serial_write(lj_serial *s, const uint8_t *buf, size_t n)
{
    ssize_t k;
    if (!s || (!buf && n)) return -1;
    if (n > 65536) n = 65536;
    k = write(s->fd, buf, n);
    if (k < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 0 : -1;
    return (int)k;
}

void lj_serial_close(lj_serial *s)
{
    if (!s) return;
    tcsetattr(s->fd, TCSANOW, &s->saved);
    close(s->fd);
    free(s);
}
