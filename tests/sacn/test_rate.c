/* test_rate.c - `lj4evr_tests test_sacn_rate`: the send mechanics of design 8.1 (ME-6)
 * and the E1.11 rate cap (ME-24) with a fake clock, counted through lj_sacn_stats:
 *   - a new universe announces itself with 3 packets, one per pump;
 *   - 400 changing ticks at 40 Hz -> 400 packets (not 800), then exactly 2 repeats,
 *     then keep-alives every 900 ms (none at 800 ms);
 *   - at TickHz 80 the wire sees 40 packets/s per universe, 80 with unlock_rate;
 *   - at TickHz 50 without unlock the sender never exceeds 44/s (25/s with a phase-locked clock).
 * The socket is real (unicast to 127.0.0.1, nobody listening); only the clock is fake. */
#include <stdio.h>
#include <string.h>
#include "io/lj_sacn.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint64_t lj_now;                       /* the fake clock, microseconds */
static uint8_t lj_slots[LJ_LINK_CH];

static uint32_t lj_packets(void) { lj_sacn_stats st; lj_sacn_stats_get(&st); return st.packets[0]; }

static uint32_t lj_run(int ticks, uint32_t tick_us, int change)   /* ticks x (set [changing] + pump) at tick_us; returns packets sent */
{
    uint32_t before = lj_packets();
    int i;
    for (i = 0; i < ticks; i++) {
        if (change) lj_slots[i % LJ_LINK_CH] = (uint8_t)(lj_slots[i % LJ_LINK_CH] + 1);
        lj_sacn_set(1, lj_slots);
        lj_sacn_pump(lj_now);
        lj_now += tick_us;
    }
    return lj_packets() - before;
}

int lj_test_sacn_rate(int argc, char **argv);
int lj_test_sacn_rate(int argc, char **argv)
{
    lj_sacn_cfg cfg;
    lj_sacn_stats st;
    uint32_t n;
    int i;
    (void)argc; (void)argv;

    lj_sacn_cfg_default(&cfg, "rate");
    memset(cfg.universe_of_link, 0, sizeof cfg.universe_of_link);
    cfg.universe_of_link[0] = 1;
    cfg.nunicast[0] = 1; cfg.unicast[0][0] = 0x7F000001u;        /* unicast 127.0.0.1: sendto succeeds on every CI OS without a NIC */
    CHECK(lj_sacn_open(&cfg) == 0);
    lj_now = 1000000u;
    memset(lj_slots, 0, sizeof lj_slots);

    /* announce: 3 packets across 3 pumps, nothing on the 4th */
    CHECK(lj_run(1, 25000, 0) == 1);
    CHECK(lj_run(1, 25000, 0) == 1);
    CHECK(lj_run(1, 25000, 0) == 1);
    CHECK(lj_run(1, 25000, 0) == 0);
    lj_sacn_stats_get(&st);
    CHECK(st.discovery == 1 && st.universe[0] == 1 && st.terminated == 0);

    /* 400 changing ticks at 40 Hz -> 400 packets, not 800 (ME-6) */
    n = lj_run(400, 25000, 1);
    CHECK(n == 400);
    printf("  400 changing ticks at 40 Hz: %u packets\n", n);
    lj_sacn_stats_get(&st);
    CHECK(st.pps[0] == 40);                                       /* the last whole second of the fade */

    /* change stops: exactly 2 repeats, then silence until 900 ms */
    CHECK(lj_run(2, 25000, 0) == 2);
    CHECK(lj_run(35, 25000, 0) == 0);                             /* 25..875 ms after the last repeat: nothing */
    CHECK(lj_run(1, 25000, 0) == 1);                              /* 900 ms: the keep-alive */
    CHECK(lj_run(35, 25000, 0) == 0);
    CHECK(lj_run(1, 25000, 0) == 1);                              /* next keep-alive 900 ms later */
    lj_sacn_stats_get(&st);
    CHECK(st.pps[0] == 5);                                        /* the last whole second: the fade's last 3 ticks + the 2 repeats */

    /* ME-24: TickHz 80 -> 40 packets/s (every other tick is < 22727 us after the last send) */
    n = lj_run(400, 12500, 1);
    CHECK(n == 200);
    lj_sacn_stats_get(&st);
    CHECK(st.pps[0] == 40);
    printf("  400 changing ticks at 80 Hz, unlock_rate 0: %u packets, pps %u\n", n, st.pps[0]);

    /* TickHz 50 without unlock: never 50, never 80 (acceptance 6) */
    n = lj_run(500, 20000, 1);
    CHECK(n <= 440 && n < 500);
    printf("  500 changing ticks at 50 Hz, unlock_rate 0: %u packets (%u/s)\n", n, n / 10);

    /* unlock_rate: a remap keeps universe 1 (no termination, sequence carries on) and 80 Hz goes through */
    cfg.unlock_rate = 1;
    CHECK(lj_sacn_open(&cfg) == 0);
    lj_sacn_stats_get(&st);
    CHECK(st.terminated == 0);
    n = lj_run(400, 12500, 1);
    CHECK(n == 400);
    lj_sacn_stats_get(&st);
    CHECK(st.pps[0] == 80);
    printf("  400 changing ticks at 80 Hz, unlock_rate 1: %u packets, pps %u\n", n, st.pps[0]);

    /* close: 3 x Stream_Terminated for the one mapped universe; then everything is a no-op */
    lj_sacn_close();
    lj_sacn_stats_get(&st);
    CHECK(st.terminated == 3);
    for (i = 0; i < 3; i++) lj_sacn_pump(lj_now += 25000);
    lj_sacn_close();
    lj_sacn_stats_get(&st);
    CHECK(st.terminated == 3 && st.universe[0] == 0);
    printf("test_sacn_rate: %u send errors (last %d), %d failure(s)\n", st.send_errors, st.last_err, fails);
    return fails ? 1 : 0;
}
