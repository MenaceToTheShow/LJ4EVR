/* test_remote.c - the remote track's rows (docs/w5-contracts.md seam 5). The contract seeds the fuzz row (W5-X-HOSTILE):
 *   fuzz_remote  the note 05 s10.1 example (cue 130 = 129, 1, 2) plus a blackout frame must parse to two messages, then
 *                LJ_FUZZ_ITERS mutated byte streams with random arrival times through lj_remote_feed, and every message
 *                through lj_remote_exec (no bindings, no state query: nothing may crash)
 *   test_remote  one row per command of 05 s10.1 over a synthetic byte stream (the 0 meanings, 134 manual only, the set
 *                commands through a fake state query), the 0.2 s window and resync, out-of-range ignored, the [REMOTE]
 *                keys round trip with hostile values refused, the TCP listener on loopback (frames fire, the fifth
 *                connection refused, 0.0.0.0 refused, nothing open on a default cfg, a flood read within the budget)
 * The pty row is tests/io/test_remote_pty.c. */
#include <stdio.h>
#include <string.h>
#include "io/lj_remote.h"
#include "io/lj_fn.h"
#include "io/lj_socket.h"
#include "core/lj_clock.h"
#include "core/lj_str.h"
#include "lj_fuzz.h"

int lj_test_fuzz_remote(int argc, char **argv)
{
    static const uint8_t seed[] = { 129, 1, 2, 132, 2, 1 };
    uint8_t buf[256];
    lj_remote_parser p;
    lj_remote_msg m[8];
    uint64_t now = 1000000;
    uint32_t s = 0x232u;
    size_t n;
    int i, k, got, bad = 0;
    (void)argc; (void)argv;
    lj_remote_parser_init(&p);
    got = lj_remote_feed(&p, seed, sizeof seed, now, m, 8);
    if (got != 2 || m[0].cmd != LJ_REM_CUE || m[0].value != 130 || m[1].cmd != LJ_REM_MISC || m[1].d1 != 2 || m[1].d2 != 1) {
        printf("PENDING remote: lj_remote_feed does not decode the 05 s10.1 example yet\n");
        return 1;
    }
    for (i = 0; i < LJ_FUZZ_ITERS; i++) {
        memcpy(buf, seed, sizeof seed); n = sizeof seed;
        for (k = (int)(lj_fuzz_next(&s) % 12); k >= 0; k--) lj_fuzz_mutate(buf, &n, sizeof buf, &s);
        now += lj_fuzz_next(&s) % 300000u;                       /* around the 0.2 s frame window */
        got = lj_remote_feed(&p, buf, n, now, m, 8);
        if (got < 0 || got > 8) { if (!bad++) printf("FAIL iteration %d: %d messages for max 8\n", i, got); continue; }
        for (k = 0; k < got; k++) {
            if (m[k].cmd < 128 || m[k].d1 > 127 || m[k].d2 > 127 || m[k].value != m[k].d1 * 128 + m[k].d2) { if (!bad++) printf("FAIL iteration %d: malformed message\n", i); }
            lj_remote_exec(&m[k]);
        }
        if (lj_remote_inject(buf, n, now) < 0) { if (!bad++) printf("FAIL iteration %d: inject refused\n", i); }
    }
    printf("fuzz_remote: %d iterations, %s\n", LJ_FUZZ_ITERS, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}

/* ---- remote track rows (W5-REM-1, -2, -4) ---- */
static int rem_fails;
#define RCHECK(cond) do { if (!(cond)) { rem_fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static struct { int n; enum lj_fn fn; int edge; uint32_t prog; } rem_last;
static int rem_state[8][32];                                      /* [enum lj_remote_q][n] */
static void rem_rec(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)ud; rem_last.n++; rem_last.fn = fn; rem_last.edge = edge; rem_last.prog = prog; }
static int rem_query(enum lj_remote_q q, int n) { return (int)q < 8 && n >= 0 && n < 32 ? rem_state[q][n] : -1; }

/* sends one frame; 1 when exactly fn / edge / prog ran (LJ_FN_NONE: nothing ran) */
static uint64_t rem_now = 5000000;
static int rem_cmd(int c, int d1, int d2, enum lj_fn fn, int edge, uint32_t prog)
{
    uint8_t b[3];
    b[0] = (uint8_t)c; b[1] = (uint8_t)d1; b[2] = (uint8_t)d2;
    memset(&rem_last, 0, sizeof rem_last);
    lj_remote_inject(b, 3, rem_now += 1000);
    if (fn == LJ_FN_NONE) return rem_last.n == 0;
    return rem_last.n == 1 && rem_last.fn == fn && rem_last.edge == edge && rem_last.prog == prog;
}

static void rem_tcp(void)
{
    static const uint8_t cue130[] = { 129, 1, 2 };
    static uint8_t flood[LJ_REMOTE_BUDGET * 3];
    lj_remote_cfg c;
    lj_remote_stats st;
    lj_socket cl[LJ_REMOTE_CONNS + 1];
    uint64_t t0;
    uint32_t b0;
    int k, fired = 0, n;
    lj_remote_cfg_default(&c);
    RCHECK(lj_remote_open(&c) == 0);                                   /* W5-X-OFF: a default cfg opens nothing */
    lj_remote_get_stats(&st); RCHECK(!st.tcp_open && !st.serial_open);
    c.tcp_on = 1; c.tcp_ip = 0;                                        /* 0.0.0.0 is never bound */
    RCHECK(lj_remote_open(&c) == -1);
    lj_remote_get_stats(&st); RCHECK(!st.tcp_open);
    c.tcp_ip = LJ_IP_LOOPBACK; c.tcp_port = 0;                         /* ephemeral port: no clash on a CI runner */
    RCHECK(lj_remote_open(&c) == 0);
    lj_remote_get_stats(&st); RCHECK(st.tcp_open && st.tcp_port != 0);
    if (!st.tcp_open) return;
    for (k = 0; k <= LJ_REMOTE_CONNS; k++) { cl[k] = lj_socket_connect(LJ_IP_LOOPBACK, st.tcp_port); RCHECK(cl[k] != LJ_SOCKET_BAD); }
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 2000000u; ) {
        lj_remote_poll(lj_clock_us());
        lj_remote_get_stats(&st);
        if (st.conns == LJ_REMOTE_CONNS && st.refused >= 1) break;
    }
    RCHECK(st.conns == LJ_REMOTE_CONNS); RCHECK(st.refused == 1);     /* bounded: the fifth is closed at once */
    memset(&rem_last, 0, sizeof rem_last);
    for (k = 0; k <= LJ_REMOTE_CONNS; k++) if (cl[k] != LJ_SOCKET_BAD) lj_socket_send(cl[k], cue130, sizeof cue130);
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 2000000u && fired < LJ_REMOTE_CONNS; ) fired += lj_remote_poll(lj_clock_us());
    RCHECK(fired == LJ_REMOTE_CONNS);                                 /* each open connection has its own frame state */
    RCHECK(rem_last.fn == LJ_FN_PROG_CUE && rem_last.prog == 130);
    lj_socket_close(cl[0]);                                            /* a peer close is seen and the slot freed */
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 2000000u; ) { lj_remote_poll(lj_clock_us()); lj_remote_get_stats(&st); if (st.conns == LJ_REMOTE_CONNS - 1) break; }
    RCHECK(st.conns == LJ_REMOTE_CONNS - 1);
    for (k = 0; k < (int)sizeof flood; k++) flood[k] = (uint8_t)(k % 3 == 0 ? 132 : k % 3 == 1 ? 3 : 0);   /* fade in, repeated */
    n = lj_socket_send(cl[1], flood, sizeof flood);
    RCHECK(n > LJ_REMOTE_BUDGET);
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 50000u; ) {}          /* let the bytes arrive */
    lj_remote_get_stats(&st); b0 = st.bytes;
    lj_remote_poll(lj_clock_us());
    lj_remote_get_stats(&st); RCHECK(st.bytes > b0 && st.bytes - b0 <= LJ_REMOTE_BUDGET);   /* W5-X-THREAD: one pass, one budget */
    for (k = 1; k <= LJ_REMOTE_CONNS; k++) lj_socket_close(cl[k]);
    lj_remote_close();
    lj_remote_get_stats(&st); RCHECK(!st.tcp_open && st.conns == 0);
}

static void rem_cfg(void)
{
    lj_ini *ini = lj_ini_new();
    lj_remote_cfg c, d;
    lj_remote_cfg_load(NULL, &c);
    RCHECK(!c.tcp_on && !c.serial_on && c.tcp_ip == LJ_IP_LOOPBACK && c.tcp_port == LJ_REMOTE_TCP_PORT && c.baud == 9600 && !strcmp(c.serial_port, "COM1"));
    c.tcp_on = 1; c.serial_on = 1; c.tcp_port = 9001; c.baud = 57600; lj_str_copy(c.serial_port, sizeof c.serial_port, "COM3"); c.tcp_ip = 0x0A000001u;
    lj_remote_cfg_save(ini, &c);
    lj_remote_cfg_load(ini, &d);
    RCHECK(d.tcp_on && d.serial_on && d.tcp_port == 9001 && d.baud == 57600 && !strcmp(d.serial_port, "COM3") && d.tcp_ip == 0x0A000001u);
    RCHECK(!strcmp(lj_ini_gets(ini, "REMOTE", "TcpIp", ""), "10.0.0.1"));
    lj_ini_seti(ini, "REMOTE", "TcpPort", 70000); lj_ini_seti(ini, "REMOTE", "Baud", 1234); lj_ini_sets(ini, "REMOTE", "TcpIp", "999.1.1.1");
    lj_ini_sets(ini, "REMOTE", "SerialPort", "COM1 1111111111222222222233333333334444444444555555555566666666667777777777");
    lj_remote_cfg_load(ini, &d);                                       /* hostile values fall back to the defaults */
    RCHECK(d.tcp_port == LJ_REMOTE_TCP_PORT && d.baud == 9600 && d.tcp_ip == LJ_IP_LOOPBACK && !strcmp(d.serial_port, "COM1"));
    lj_ini_free(ini);
}

int lj_test_remote(int argc, char **argv)
{
    static const enum lj_fn fns[] = { LJ_FN_PROG_CUELIST, LJ_FN_CUELIST_STOP, LJ_FN_PROG_CUE, LJ_FN_NEW_CLEAR_CUE, LJ_FN_PROG_BGCUE, LJ_FN_CLEAR_BGCUE,
        LJ_FN_PROG_SEQ_FLASH, LJ_FN_SMOKE_BUTTON_FLASH, LJ_FN_TOGGLE_SMOKE_TIMER, LJ_FN_BLACKOUT, LJ_FN_BLACKOUT_FLASH, LJ_FN_FADE_IN, LJ_FN_FADE_OUT,
        LJ_FN_CUELIST_GO, LJ_FN_CUELIST_BACK };
    lj_remote_stats st;
    lj_remote_parser p;
    lj_remote_msg m[4];
    uint32_t ign;
    size_t i;
    int k;
    (void)argc; (void)argv;
    for (i = 0; i < sizeof fns / sizeof fns[0]; i++) lj_fn_bind(fns[i], rem_rec, NULL);
    for (k = LJ_FN_STATIC_1; k <= LJ_FN_STATIC_10; k++) lj_fn_bind((enum lj_fn)k, rem_rec, NULL);
    for (k = LJ_FN_STATIC_11; k <= LJ_FN_STATIC_20; k++) lj_fn_bind((enum lj_fn)k, rem_rec, NULL);
    for (k = 0; k < 12; k++) lj_fn_bind((enum lj_fn)(LJ_FN_TOGGLE_SLOT_ENABLE_1 + k), rem_rec, NULL);
    for (k = 0; k < 5; k++) lj_fn_bind((enum lj_fn)(LJ_FN_BGCUE_TOGGLE_SLOT_1 + k), rem_rec, NULL);

    /* 128..131 with the 0 meanings; value = d1 * 128 + d2 */
    RCHECK(rem_cmd(128, 0, 7, LJ_FN_PROG_CUELIST, 1, 7));
    RCHECK(rem_cmd(128, 0, 0, LJ_FN_CUELIST_STOP, 1, 0));
    RCHECK(rem_cmd(129, 1, 2, LJ_FN_PROG_CUE, 1, 130));              /* the help's example: cue 130 = 129, 1, 2 */
    RCHECK(rem_cmd(129, 127, 127, LJ_FN_PROG_CUE, 1, 16383));         /* the "127" bug fixed in RN 2.6 b1 */
    RCHECK(rem_cmd(129, 0, 0, LJ_FN_NEW_CLEAR_CUE, 1, 0));
    RCHECK(rem_cmd(130, 0, 3, LJ_FN_PROG_BGCUE, 1, 3));
    RCHECK(rem_cmd(130, 0, 0, LJ_FN_CLEAR_BGCUE, 1, 0));
    RCHECK(rem_cmd(131, 0, 9, LJ_FN_PROG_SEQ_FLASH, 1, 9));
    RCHECK(rem_cmd(131, 0, 0, LJ_FN_PROG_SEQ_FLASH, 0, 0));
    /* 132 */
    RCHECK(rem_cmd(132, 0, 1, LJ_FN_SMOKE_BUTTON_FLASH, 1, 0));
    RCHECK(rem_cmd(132, 0, 0, LJ_FN_SMOKE_BUTTON_FLASH, 0, 0));
    RCHECK(rem_cmd(132, 1, 1, LJ_FN_NONE, 0, 0));                     /* no state query (headless): ignored */
    lj_remote_set_query(rem_query);
    rem_state[LJ_REMQ_SMOKE_TIMER][0] = 0;
    RCHECK(rem_cmd(132, 1, 1, LJ_FN_TOGGLE_SMOKE_TIMER, 1, 0));
    rem_state[LJ_REMQ_SMOKE_TIMER][0] = 1;
    RCHECK(rem_cmd(132, 1, 1, LJ_FN_NONE, 0, 0));                     /* already on: nothing */
    RCHECK(rem_cmd(132, 1, 0, LJ_FN_TOGGLE_SMOKE_TIMER, 1, 0));
    RCHECK(rem_cmd(132, 2, 1, LJ_FN_BLACKOUT, 1, 0));
    RCHECK(rem_cmd(132, 2, 0, LJ_FN_BLACKOUT_FLASH, 0, 0));            /* restore = LJ_CMD_BLACKOUT b 0 */
    RCHECK(rem_cmd(132, 3, 0, LJ_FN_FADE_IN, 1, 0));
    RCHECK(rem_cmd(132, 3, 1, LJ_FN_FADE_OUT, 1, 0));
    RCHECK(rem_cmd(132, 4, 0, LJ_FN_NONE, 0, 0));
    RCHECK(rem_cmd(132, 2, 2, LJ_FN_NONE, 0, 0));
    /* 133 statics */
    rem_state[LJ_REMQ_STATIC][3] = 0;
    RCHECK(rem_cmd(133, 3, 1, LJ_FN_STATIC_3, 1, 0));
    rem_state[LJ_REMQ_STATIC][3] = 1;
    RCHECK(rem_cmd(133, 3, 1, LJ_FN_NONE, 0, 0));
    RCHECK(rem_cmd(133, 3, 0, LJ_FN_STATIC_3, 1, 0));                 /* a Toggle entry: pressed again */
    rem_state[LJ_REMQ_STATIC][15] = 1; rem_state[LJ_REMQ_STATIC_FLASH][15] = 1;
    RCHECK(rem_cmd(133, 15, 0, LJ_FN_STATIC_15, 0, 0));               /* a Flash entry: released */
    RCHECK(rem_cmd(133, 21, 1, LJ_FN_NONE, 0, 0));
    RCHECK(rem_cmd(133, 0, 1, LJ_FN_NONE, 0, 0));
    /* 134 Go / Back, manual cue lists only */
    rem_state[LJ_REMQ_CL_MANUAL][0] = 0;
    RCHECK(rem_cmd(134, 1, 1, LJ_FN_NONE, 0, 0));
    rem_state[LJ_REMQ_CL_MANUAL][0] = 1;
    RCHECK(rem_cmd(134, 1, 1, LJ_FN_CUELIST_GO, 1, 0));
    RCHECK(rem_cmd(134, 1, 2, LJ_FN_CUELIST_BACK, 1, 0));
    RCHECK(rem_cmd(134, 1, 3, LJ_FN_NONE, 0, 0));
    RCHECK(rem_cmd(134, 2, 1, LJ_FN_NONE, 0, 0));
    /* 135 cue slots, 136 BG slots */
    rem_state[LJ_REMQ_SLOT][12] = 1;
    RCHECK(rem_cmd(135, 12, 0, (enum lj_fn)(LJ_FN_TOGGLE_SLOT_ENABLE_1 + 11), 1, 0));
    RCHECK(rem_cmd(135, 12, 1, LJ_FN_NONE, 0, 0));
    RCHECK(rem_cmd(135, 13, 1, LJ_FN_NONE, 0, 0));
    rem_state[LJ_REMQ_BGSLOT][5] = 0;
    RCHECK(rem_cmd(136, 5, 1, (enum lj_fn)(LJ_FN_BGCUE_TOGGLE_SLOT_1 + 4), 1, 0));
    RCHECK(rem_cmd(136, 6, 1, LJ_FN_NONE, 0, 0));
    /* unknown commands ignored and counted (the log line is rate-limited to one per second) */
    lj_remote_get_stats(&st); ign = st.ignored;
    for (k = 137; k < 256; k++) RCHECK(rem_cmd(k, 0, 1, LJ_FN_NONE, 0, 0));
    lj_remote_get_stats(&st); RCHECK(st.ignored - ign == 119);

    /* framing: resync on a byte >= 128, stray data dropped, the 0.2 s window */
    lj_remote_parser_init(&p);
    { static const uint8_t b[] = { 5, 6, 129, 1, 130, 0, 4 };
      RCHECK(lj_remote_feed(&p, b, sizeof b, 1000, m, 4) == 1 && m[0].cmd == 130 && m[0].value == 4 && p.dropped == 3); }
    { static const uint8_t b[] = { 129, 0 }, c2[] = { 9 };
      RCHECK(lj_remote_feed(&p, b, 2, 10000, m, 4) == 0);
      RCHECK(lj_remote_feed(&p, c2, 1, 10000 + LJ_REMOTE_FRAME_US, m, 4) == 1 && m[0].value == 9);          /* within 0.2 s */
      RCHECK(lj_remote_feed(&p, b, 2, 20000000, m, 4) == 0);
      RCHECK(lj_remote_feed(&p, c2, 1, 20000000 + LJ_REMOTE_FRAME_US + 1, m, 4) == 0 && p.have == 0); }      /* late: dropped */
    { static const uint8_t b[] = { 128, 0, 1, 128, 0, 2, 128, 0, 3 };
      RCHECK(lj_remote_feed(&p, b, sizeof b, 30000000, m, 2) == 2); }                                       /* max honoured, the rest consumed */
    RCHECK(lj_remote_inject(NULL, 3, 0) == -1);

    rem_cfg();
    rem_tcp();
    lj_remote_set_query(NULL);
    lj_fn_unbind_all();
    printf("test_remote: %s\n", rem_fails ? "FAIL" : "PASS");
    return rem_fails ? 1 : 0;
}
