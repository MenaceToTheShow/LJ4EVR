/* lj_remote.c - the remote protocol, TCP listener and serial port (io/lj_remote.h; note 05 s10.1; W5-REM-1..3).
 * UI thread only: the parser is pure, the transports are non-blocking and read with a byte budget per pass, every
 * action runs through lj_fn_run / lj_fn_run_prog. Tests: tests/io/test_remote.c (fuzz_remote, test_remote,
 * test_serial_pty). */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf under /W4 /WX on MSVC (as io/lj_sacn.c) */
#include <stdio.h>
#include <string.h>
#include "io/lj_remote.h"
#include "io/lj_fn.h"
#include "io/lj_net.h"
#include "io/lj_serial.h"
#include "io/lj_socket.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

const uint32_t lj_remote_bauds[LJ_REMOTE_BAUDS] = { 2400, 4800, 9600, 14400, 19200, 38400, 57600 };   /* rgBaudRate.Items, LJ_RS232.exe TRS232SettingsForm */

enum { LJ_REM_MSGS = 86,          /* frames in one 255-byte chunk, plus one */
       LJ_REM_ACCEPTS = 8 };      /* connections accepted (or refused) per pass: a connect flood cannot hold the 10 ms wake */
#define LJ_REM_LOG_US 1000000u    /* the rate-limited log: one line per second at most, PROPOSAL */

static struct {
    lj_socket lsn;
    struct { lj_socket s; uint8_t on; lj_remote_parser p; } conn[LJ_REMOTE_CONNS];
    lj_serial *ser;
    lj_remote_parser sp, ip;      /* serial, inject */
    lj_remote_query_fn query;
    lj_remote_setter_fn setter;
    uint64_t now_us, log_us;
    uint32_t log_skipped;
    uint8_t logged;
    lj_remote_stats st;
} lj_rem;

/* ---- parser (W5-REM-1) ---- */
void lj_remote_parser_init(lj_remote_parser *p) { if (p) memset(p, 0, sizeof *p); }

int lj_remote_feed(lj_remote_parser *p, const uint8_t *bytes, size_t n, uint64_t now_us, lj_remote_msg *out, int max)
{
    int got = 0;
    size_t i;
    if (!p || (!bytes && n)) return 0;
    if (!out || max < 0) max = 0;
    if (p->have && now_us - p->t0_us > LJ_REMOTE_FRAME_US) { p->have = 0; p->dropped++; }   /* a partial frame older than 0.2 s */
    for (i = 0; i < n; i++) {
        uint8_t b = bytes[i];
        if (p->have && now_us - p->t0_us > LJ_REMOTE_FRAME_US) { p->have = 0; p->dropped++; }
        if (b >= 128) {                                              /* a command byte always starts a frame (resync) */
            if (p->have) p->dropped++;
            p->buf[0] = b; p->have = 1; p->t0_us = now_us;
            continue;
        }
        if (!p->have) { p->dropped++; continue; }                    /* a data byte with no command before it */
        p->buf[p->have++] = b;
        if (p->have < 3) continue;
        p->have = 0; p->frames++;
        if (got >= max) { p->dropped++; continue; }
        out[got].cmd = p->buf[0]; out[got].d1 = p->buf[1]; out[got].d2 = p->buf[2];
        out[got].value = (uint16_t)(p->buf[1] * 128u + p->buf[2]);
        got++;
    }
    return got;
}

/* ---- execution (UI thread) ---- */
void lj_remote_set_query(lj_remote_query_fn fn) { lj_rem.query = fn; }
void lj_remote_set_setter(lj_remote_setter_fn fn) { lj_rem.setter = fn; }

static int lj_rem_ignore(const lj_remote_msg *m, const char *why)
{
    lj_rem.st.ignored++;
    if (lj_rem.logged && lj_rem.now_us - lj_rem.log_us < LJ_REM_LOG_US) { lj_rem.log_skipped++; return -1; }
    if (lj_rem.log_skipped) lj_log("remote: ignored [%u][%u][%u] (%s); %u more ignored since the last line", m->cmd, m->d1, m->d2, why, (unsigned)lj_rem.log_skipped);
    else lj_log("remote: ignored [%u][%u][%u] (%s)", m->cmd, m->d1, m->d2, why);
    lj_rem.logged = 1; lj_rem.log_us = lj_rem.now_us; lj_rem.log_skipped = 0;
    return -1;
}

static int lj_rem_q(enum lj_remote_q q, int n) { return lj_rem.query ? lj_rem.query(q, n) : -1; }

/* ME-168: the set form, idempotent (no toggle against the lagging view) */
static int lj_rem_set(const lj_remote_msg *m, enum lj_remote_q q, int n, int on)
{
    if (!lj_rem.setter || lj_rem.setter(q, n, on) != 0) return lj_rem_ignore(m, "state unknown");
    return 0;
}

int lj_remote_exec(const lj_remote_msg *m)
{
    unsigned v, d1, d2;
    int st;
    enum lj_fn fn;
    if (!m) return -1;
    v = m->value; d1 = m->d1; d2 = m->d2;
    if (m->cmd < 128 || d1 > 127 || d2 > 127 || v != d1 * 128u + d2) return lj_rem_ignore(m, "malformed");
    switch (m->cmd) {
    case LJ_REM_CUELIST: if (v) lj_fn_run_prog(LJ_FN_PROG_CUELIST, v, 1); else lj_fn_run(LJ_FN_CUELIST_STOP, 1); return 0;      /* 0 = stop current */
    case LJ_REM_CUE:     if (v) lj_fn_run_prog(LJ_FN_PROG_CUE, v, 1); else lj_fn_run(LJ_FN_NEW_CLEAR_CUE, 1); return 0;         /* 0 = clear current (the MIDI release row) */
    case LJ_REM_BGCUE:   if (v) lj_fn_run_prog(LJ_FN_PROG_BGCUE, v, 1); else lj_fn_run(LJ_FN_CLEAR_BGCUE, 1); return 0;         /* 0 = clear */
    case LJ_REM_FLASH:   lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, v, v != 0); return 0;                                               /* on until 0 = clear (the release) */
    case LJ_REM_MISC:
        if (d2 > 1) return lj_rem_ignore(m, "out of range");
        switch (d1) {
        case 0: lj_fn_run(LJ_FN_SMOKE_BUTTON_FLASH, (int)d2); return 0;                                  /* smoke off / on: the held Smoke button */
        case 1: return lj_rem_set(m, LJ_REMQ_SMOKE_TIMER, 0, (int)d2);
        case 2: if (d2) lj_fn_run(LJ_FN_BLACKOUT, 1); else lj_fn_run(LJ_FN_BLACKOUT_FLASH, 0); return 0;   /* 1 blackout, 0 restore (LJ_CMD_BLACKOUT b = 0) */
        case 3: lj_fn_run(d2 ? LJ_FN_FADE_OUT : LJ_FN_FADE_IN, 1); return 0;                               /* 0 fade in, 1 fade out: the master-intensity preset time */
        default: return lj_rem_ignore(m, "out of range");
        }
    case LJ_REM_STATIC:
        if (d1 < 1 || d1 > 20 || d2 > 1) return lj_rem_ignore(m, "out of range");
        fn = d1 <= 10 ? (enum lj_fn)(LJ_FN_STATIC_1 + (int)d1 - 1) : (enum lj_fn)(LJ_FN_STATIC_11 + (int)d1 - 11);
        if ((st = lj_rem_q(LJ_REMQ_STATIC, (int)d1)) < 0) return lj_rem_ignore(m, "state unknown");
        if (st == (int)d2) return 0;
        lj_fn_run(fn, !d2 && lj_rem_q(LJ_REMQ_STATIC_FLASH, (int)d1) == 1 ? 0 : 1);   /* off: a Flash entry is released, a Toggle entry pressed again */
        return 0;
    case LJ_REM_CL_GOBACK:
        if (d1 != 1 || d2 < 1 || d2 > 2) return lj_rem_ignore(m, "out of range");
        if (lj_rem_q(LJ_REMQ_CL_MANUAL, 0) != 1) return lj_rem_ignore(m, "not a manual cue list");   /* 05 s10.1: manual cue lists only */
        lj_fn_run(d2 == 1 ? LJ_FN_CUELIST_GO : LJ_FN_CUELIST_BACK, 1);
        return 0;
    case LJ_REM_SLOT:
        if (d1 < 1 || d1 > 12 || d2 > 1) return lj_rem_ignore(m, "out of range");
        return lj_rem_set(m, LJ_REMQ_SLOT, (int)d1, (int)d2);
    case LJ_REM_BGSLOT:
        if (d1 < 1 || d1 > 5 || d2 > 1) return lj_rem_ignore(m, "out of range");
        return lj_rem_set(m, LJ_REMQ_BGSLOT, (int)d1, (int)d2);
    default: return lj_rem_ignore(m, "unknown command");
    }
}

static int lj_rem_run(lj_remote_parser *p, const uint8_t *b, size_t n, uint64_t now_us)
{
    lj_remote_msg m[LJ_REM_MSGS];
    int got, i, done = 0;
    size_t off = 0, k;
    lj_rem.now_us = now_us;
    lj_rem.st.bytes += (uint32_t)n;
    do {
        k = n - off > 255 ? 255 : n - off;
        got = lj_remote_feed(p, b ? b + off : NULL, k, now_us, m, LJ_REM_MSGS);
        for (i = 0; i < got; i++) if (lj_remote_exec(&m[i]) == 0) done++;
        off += k;
    } while (off < n);
    lj_rem.st.frames += (uint32_t)done;
    return done;
}

int lj_remote_inject(const uint8_t *bytes, size_t n, uint64_t now_us)
{
    if (!bytes && n) return -1;
    return lj_rem_run(&lj_rem.ip, bytes, n, now_us);
}

/* ---- settings: Hardware.Ini [REMOTE] (ours, additive) ---- */
int lj_remote_ip_parse(const char *s, uint32_t *out)
{
    unsigned a, b, c, d; char tail;
    if (!s || !out || sscanf(s, "%3u.%3u.%3u.%3u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return -1;
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}
static void lj_rem_ip_fmt(char *dst, size_t cap, uint32_t ip) { if (snprintf(dst, cap, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u) < 0) dst[0] = 0; }

static int lj_rem_baud_ok(uint32_t b) { int i; for (i = 0; i < LJ_REMOTE_BAUDS; i++) if (lj_remote_bauds[i] == b) return 1; return 0; }

void lj_remote_cfg_default(lj_remote_cfg *c)
{
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->tcp_ip = LJ_IP_LOOPBACK; c->tcp_port = LJ_REMOTE_TCP_PORT; c->baud = LJ_REMOTE_BAUD_DEFAULT;
    lj_str_copy(c->serial_port, sizeof c->serial_port, "COM1");
}

void lj_remote_cfg_load(lj_ini *ini, lj_remote_cfg *c)
{
    const char *s;
    int v;
    uint32_t ip;
    if (!c) return;
    lj_remote_cfg_default(c);
    if (!ini) return;
    c->tcp_on = (uint8_t)(lj_ini_geti(ini, "REMOTE", "TcpEnabled", 0) != 0);
    if (lj_remote_ip_parse(lj_ini_gets(ini, "REMOTE", "TcpIp", ""), &ip) == 0) c->tcp_ip = ip;   /* checked again against the NICs at open */
    v = lj_ini_geti(ini, "REMOTE", "TcpPort", LJ_REMOTE_TCP_PORT);
    if (v >= 1 && v <= 65535) c->tcp_port = (uint16_t)v;
    c->serial_on = (uint8_t)(lj_ini_geti(ini, "REMOTE", "SerialEnabled", 0) != 0);
    s = lj_ini_gets(ini, "REMOTE", "SerialPort", "");
    if (s && *s && strlen(s) < sizeof c->serial_port) lj_str_copy(c->serial_port, sizeof c->serial_port, s);   /* the backend validates the name at open */
    v = lj_ini_geti(ini, "REMOTE", "Baud", LJ_REMOTE_BAUD_DEFAULT);
    if (v > 0 && lj_rem_baud_ok((uint32_t)v)) c->baud = (uint32_t)v;
}

void lj_remote_cfg_save(lj_ini *ini, const lj_remote_cfg *c)
{
    char ip[16];
    if (!ini || !c) return;
    lj_rem_ip_fmt(ip, sizeof ip, c->tcp_ip);
    lj_ini_seti(ini, "REMOTE", "TcpEnabled", c->tcp_on != 0);
    lj_ini_sets(ini, "REMOTE", "TcpIp", ip);
    lj_ini_seti(ini, "REMOTE", "TcpPort", c->tcp_port);
    lj_ini_seti(ini, "REMOTE", "SerialEnabled", c->serial_on != 0);
    lj_ini_sets(ini, "REMOTE", "SerialPort", c->serial_port);
    lj_ini_seti(ini, "REMOTE", "Baud", (int)c->baud);
}

/* ---- transports (W5-REM-2, -3) ---- */
static int lj_rem_ip_allowed(uint32_t ip)                            /* 127.0.0.1, or the address of a local interface that is up: never 0.0.0.0 */
{
    lj_nic nic[16];
    int n, i;
    if (ip == LJ_IP_LOOPBACK) return 1;
    n = lj_net_list_ipv4(nic, 16);
    for (i = 0; i < n; i++) if (nic[i].ipv4 == ip && nic[i].up && ip != 0) return 1;
    return 0;
}

static void lj_rem_conn_close(int k)
{
    lj_socket_close(lj_rem.conn[k].s);
    lj_rem.conn[k].on = 0; lj_rem.conn[k].s = LJ_SOCKET_BAD;
    if (lj_rem.st.conns > 0) lj_rem.st.conns--;
}

void lj_remote_close(void)
{
    int k;
    for (k = 0; k < LJ_REMOTE_CONNS; k++) if (lj_rem.conn[k].on) lj_rem_conn_close(k);
    if (lj_rem.st.tcp_open) { lj_socket_close(lj_rem.lsn); lj_log("remote: TCP listener closed"); }
    if (lj_rem.ser) { lj_serial_close(lj_rem.ser); lj_log("remote: serial port closed"); }
    lj_rem.ser = NULL; lj_rem.lsn = LJ_SOCKET_BAD;
    lj_rem.st.tcp_open = 0; lj_rem.st.serial_open = 0; lj_rem.st.tcp_port = 0; lj_rem.st.conns = 0;
    lj_remote_parser_init(&lj_rem.sp);
}

int lj_remote_open(const lj_remote_cfg *c)
{
    char ip[16];
    int rc = 0;
    lj_remote_close();
    if (!c) return 0;
    if (c->tcp_on) {
        lj_rem_ip_fmt(ip, sizeof ip, c->tcp_ip);
        if (!lj_rem_ip_allowed(c->tcp_ip)) { lj_log("remote: %s is not 127.0.0.1 or a local interface: TCP not listening", ip); rc = -1; }
        else if ((lj_rem.lsn = lj_socket_listen(c->tcp_ip, c->tcp_port, 4 * LJ_REMOTE_CONNS)) == LJ_SOCKET_BAD) rc = -1;   /* backlog past the cap: an extra client is accepted and refused, never dropped at SYN (macOS) */
        else {
            lj_rem.st.tcp_open = 1; lj_rem.st.tcp_port = lj_socket_port(lj_rem.lsn);
            lj_log("remote: TCP listening on %s:%u (no authentication%s)", ip, (unsigned)lj_rem.st.tcp_port, c->tcp_ip == LJ_IP_LOOPBACK ? ", this computer only" : ": anyone on that network can drive the show");
        }
    }
    if (c->serial_on) {
        if ((lj_rem.ser = lj_serial_open(c->serial_port, c->baud)) == NULL) rc = -1;
        else { lj_rem.st.serial_open = 1; lj_log("remote: serial %s open, %u baud 8N1", c->serial_port, (unsigned)c->baud); }
    }
    return rc;
}

int lj_remote_poll(uint64_t now_us)
{
    uint8_t buf[256];
    int k, n, budget, done = 0;
    lj_rem.now_us = now_us;
    if (lj_rem.st.tcp_open) {
        lj_socket a;
        uint32_t peer = 0;
        for (n = 0; n < LJ_REM_ACCEPTS && (a = lj_socket_accept(lj_rem.lsn, &peer)) != LJ_SOCKET_BAD; n++) {
            char ip[16];
            for (k = 0; k < LJ_REMOTE_CONNS && lj_rem.conn[k].on; k++) {}
            lj_rem_ip_fmt(ip, sizeof ip, peer);
            if (k == LJ_REMOTE_CONNS) {                              /* bounded: the extra one is closed at once */
                lj_socket_close(a); lj_rem.st.refused++;
                if (lj_rem.st.refused <= 8 || (lj_rem.st.refused & 255u) == 0) lj_log("remote: TCP connection from %s refused (%d open, %u refused)", ip, LJ_REMOTE_CONNS, (unsigned)lj_rem.st.refused);
                continue;
            }
            lj_rem.conn[k].s = a; lj_rem.conn[k].on = 1; lj_remote_parser_init(&lj_rem.conn[k].p); lj_rem.st.conns++;
            lj_log("remote: TCP connection %d from %s", k + 1, ip);
        }
        for (k = 0; k < LJ_REMOTE_CONNS; k++) {
            if (!lj_rem.conn[k].on) continue;
            for (budget = LJ_REMOTE_BUDGET; budget > 0; budget -= n) {
                n = lj_socket_recv(lj_rem.conn[k].s, buf, (size_t)(budget < (int)sizeof buf ? budget : (int)sizeof buf));
                if (n < 0) { lj_log("remote: TCP connection %d closed", k + 1); lj_rem_conn_close(k); break; }
                if (n == 0) break;
                done += lj_rem_run(&lj_rem.conn[k].p, buf, (size_t)n, now_us);
            }
            if (lj_rem.conn[k].on) lj_rem_run(&lj_rem.conn[k].p, NULL, 0, now_us);   /* the 0.2 s expiry with no new bytes */
        }
    }
    if (lj_rem.ser) {
        for (budget = LJ_REMOTE_BUDGET; budget > 0; budget -= n) {
            n = lj_serial_read(lj_rem.ser, buf, (size_t)(budget < (int)sizeof buf ? budget : (int)sizeof buf));
            if (n < 0) { lj_log("remote: serial port lost"); lj_serial_close(lj_rem.ser); lj_rem.ser = NULL; lj_rem.st.serial_open = 0; break; }
            if (n == 0) break;
            done += lj_rem_run(&lj_rem.sp, buf, (size_t)n, now_us);
        }
        if (lj_rem.ser) lj_rem_run(&lj_rem.sp, NULL, 0, now_us);
    }
    return done;
}

void lj_remote_get_stats(lj_remote_stats *out) { if (out) *out = lj_rem.st; }
