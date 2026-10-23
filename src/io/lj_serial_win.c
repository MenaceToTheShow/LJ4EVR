/* lj_serial_win.c - the Windows serial backend of io/lj_serial.h (OS_ALLOW: windows.h). CreateFile on \\.\COMn,
 * SetCommState 8N1 with no flow control, COMMTIMEOUTS so ReadFile returns at once. Test: the remote rows on Linux drive
 * the same parser through a pty (tests/io/test_remote.c); this file compiles in the windows-msvc job. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io/lj_serial.h"
#include "core/lj_log.h"

struct lj_serial { HANDLE h; };

static const uint32_t lj_ser_bauds[] = { 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200 };

static int lj_ser_num(const char *name)                              /* "COM1" .. "COM255" -> 1..255, else -1 */
{
    int n = 0, i;
    if (!name || strncmp(name, "COM", 3) != 0 || !name[3] || strlen(name) > 6) return -1;
    for (i = 3; name[i]; i++) { if (name[i] < '0' || name[i] > '9') return -1; n = n * 10 + (name[i] - '0'); }
    return n >= 1 && n <= 255 ? n : -1;
}

int lj_serial_list(lj_serial_port *out, int max)
{
    char name[16], target[512];
    int n = 0, i;
    if (!out || max <= 0) return 0;
    for (i = 1; i <= 255 && n < max && n < LJ_SERIAL_PORTS_MAX; i++) {   /* QueryDosDevice: present ports without opening them (no DTR blip) */
        if (snprintf(name, sizeof name, "COM%d", i) < 0) name[0] = 0;
        if (QueryDosDeviceA(name, target, sizeof target) != 0) { memcpy(out[n].name, name, strlen(name) + 1); n++; }
    }
    return n;
}

lj_serial *lj_serial_open(const char *name, uint32_t baud)
{
    char path[24];
    DCB dcb;
    COMMTIMEOUTS to;
    HANDLE h;
    lj_serial *s;
    size_t i;
    int num = lj_ser_num(name), ok = 0;
    for (i = 0; i < sizeof lj_ser_bauds / sizeof lj_ser_bauds[0]; i++) if (lj_ser_bauds[i] == baud) ok = 1;
    if (num < 0) { lj_log("serial: %.63s is not a COM port name", name ? name : "(null)"); return NULL; }
    if (!ok) { lj_log("serial: %u baud not supported", (unsigned)baud); return NULL; }
    if (snprintf(path, sizeof path, "\\\\.\\COM%d", num) < 0) path[0] = 0;
    h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { lj_log("serial: cannot open COM%d: %lu", num, (unsigned long)GetLastError()); return NULL; }
    memset(&dcb, 0, sizeof dcb);
    dcb.DCBlength = sizeof dcb;
    if (!GetCommState(h, &dcb)) { lj_log("serial: GetCommState COM%d: %lu", num, (unsigned long)GetLastError()); CloseHandle(h); return NULL; }
    dcb.BaudRate = baud; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;   /* 8N1 fixed (05 s10.1) */
    dcb.fBinary = TRUE; dcb.fParity = FALSE; dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE; dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE; dcb.fRtsControl = RTS_CONTROL_ENABLE; dcb.fOutX = FALSE; dcb.fInX = FALSE;
    dcb.fNull = FALSE; dcb.fAbortOnError = FALSE;
    memset(&to, 0, sizeof to);
    to.ReadIntervalTimeout = MAXDWORD;                               /* with both read totals 0: ReadFile returns what is buffered, at once */
    to.WriteTotalTimeoutConstant = 1;                                /* ponytail: a 1 ms write cap instead of overlapped I/O; the remote never writes, extapp sends <= 32 bytes */
    if (!SetCommState(h, &dcb) || !SetCommTimeouts(h, &to)) { lj_log("serial: setting COM%d failed: %lu", num, (unsigned long)GetLastError()); CloseHandle(h); return NULL; }
    if ((s = (lj_serial *)calloc(1, sizeof *s)) == NULL) { CloseHandle(h); return NULL; }
    s->h = h;
    return s;
}

int lj_serial_read(lj_serial *s, uint8_t *buf, size_t cap)
{
    DWORD got = 0;
    if (!s || !buf) return -1;
    if (cap > 65536) cap = 65536;
    if (!ReadFile(s->h, buf, (DWORD)cap, &got, NULL)) return -1;
    return (int)got;
}

int lj_serial_write(lj_serial *s, const uint8_t *buf, size_t n)
{
    DWORD put = 0;
    if (!s || (!buf && n)) return -1;
    if (n > 65536) n = 65536;
    if (!WriteFile(s->h, buf, (DWORD)n, &put, NULL)) return -1;
    return (int)put;
}

void lj_serial_close(lj_serial *s)
{
    if (!s) return;
    CloseHandle(s->h);
    free(s);
}
