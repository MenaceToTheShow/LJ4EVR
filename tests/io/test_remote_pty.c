/* test_remote_pty.c - `lj4evr_tests test_serial_pty` (W5-REM-3, W5-X-CI): the POSIX serial backend on a pseudo terminal.
 * The remote opens the pty's slave as its serial port (8N1, 9600, non-blocking); bytes written to the master reach the
 * same frame parser and fire the cue load; an empty read returns at once; a non-tty path and a bad baud are refused.
 * Windows has no pty: the row prints SKIP there (CMake registers it on UNIX only). */
#ifndef _WIN32
#define _XOPEN_SOURCE 600   /* posix_openpt / grantpt / unlockpt / ptsname */
#define _DEFAULT_SOURCE 1
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include "io/lj_remote.h"
#include "io/lj_serial.h"
#include "io/lj_fn.h"
#include "core/lj_clock.h"
#include "core/lj_str.h"

int lj_test_serial_pty(int argc, char **argv);

#ifdef _WIN32
int lj_test_serial_pty(int argc, char **argv) { (void)argc; (void)argv; printf("test_serial_pty: SKIP (no pty on Windows)\n"); return 0; }
#else
static int pty_fails, pty_hits; static uint32_t pty_prog;
#define PCHECK(cond) do { if (!(cond)) { pty_fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
static void pty_rec(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)fn; (void)edge; (void)ud; pty_hits++; pty_prog = prog; }

int lj_test_serial_pty(int argc, char **argv)
{
    static const uint8_t cue130[] = { 129, 1, 2 };
    lj_remote_cfg c;
    lj_remote_stats st;
    lj_serial *s;
    uint8_t buf[8];
    uint64_t t0;
    const char *slave;
    int m;
    (void)argc; (void)argv;
    PCHECK(lj_serial_open("/etc/hosts", 9600) == NULL);                /* not under /dev */
    PCHECK(lj_serial_open("/dev/null", 9600) == NULL);                 /* not a terminal */
    PCHECK(lj_serial_open("/dev/../etc/hosts", 9600) == NULL);
    m = posix_openpt(O_RDWR | O_NOCTTY);
    if (m < 0 || grantpt(m) != 0 || unlockpt(m) != 0 || (slave = ptsname(m)) == NULL) { printf("FAIL no pty\n"); return 1; }
    PCHECK(lj_serial_open(slave, 1234) == NULL);                       /* unsupported baud */
    s = lj_serial_open(slave, 9600);
    PCHECK(s != NULL);
    if (s) { PCHECK(lj_serial_read(s, buf, sizeof buf) == 0); lj_serial_close(s); }   /* nothing pending: returns at once */
    lj_fn_bind(LJ_FN_PROG_CUE, pty_rec, NULL);
    lj_remote_cfg_default(&c);
    c.serial_on = 1; c.baud = 9600; lj_str_copy(c.serial_port, sizeof c.serial_port, slave);
    PCHECK(lj_remote_open(&c) == 0);
    lj_remote_get_stats(&st); PCHECK(st.serial_open && !st.tcp_open);
    PCHECK(write(m, cue130, sizeof cue130) == (ssize_t)sizeof cue130);
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 2000000u && !pty_hits; ) lj_remote_poll(lj_clock_us());
    PCHECK(pty_hits == 1 && pty_prog == 130);
    lj_remote_close();
    lj_remote_get_stats(&st); PCHECK(!st.serial_open);
    close(m);
    lj_fn_unbind_all();
    printf("test_serial_pty: %s\n", pty_fails ? "FAIL" : "PASS");
    return pty_fails ? 1 : 0;
}
#endif
