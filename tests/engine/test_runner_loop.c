/* test_runner_loop.c - `lj4evr_tests test_runner_loop`: lj_runner_loop_step (lj_runner.h), the 03 s5.5 table
 * walked as a table: direction (fwd / rev / bounce / random) x end action (Stop on scene / Sequence out / Goto
 * scene / Link to cue, Link under Edit Cue) x loop count (0 = unlimited, 1, 2), inside first/last limits 2..4 of a
 * 5-scene sequence. Each row asserts the scene sequence step by step and what the end action left behind. Random:
 * inside the limits, never the same scene twice in a row (LJ_RANDOM_NO_REPEAT, PROPOSAL), no loop count, seeded. */
#include <stdio.h>
#include <string.h>
#include "engine/lj_runner.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { T_END = -1, T_STOP = -2 };   /* want[] markers: the end action fires here / the row ends */

static void lj_t_runner(lj_runner *r, int dir, int first, int last, int loops, int end, int goto_scene, uint32_t link)
{
    lj_cue_slot s;
    static lj_scene scenes[5];
    static lj_sequence seq;
    memset(&s, 0, sizeof s);
    seq.n = 5; seq.scenes = scenes;
    s.seq = 1; s.enabled = 1; s.dir = (uint8_t)dir; s.first = (uint16_t)first; s.last = (uint16_t)last; s.loops = (uint16_t)loops;
    s.end_action = (uint8_t)end; s.goto_scene = (uint16_t)goto_scene; s.link_cue = link;
    lj_runner_set_slot(r, &s, &seq);                           /* the real slot path: limits resolved, counters reset */
    r->scene = (uint16_t)(dir == LJ_DIR_REV ? r->last - 1 : r->first - 1);   /* lj_runner_start's entry scene */
}

int lj_test_runner_loop(int argc, char **argv);
int lj_test_runner_loop(int argc, char **argv)
{
    /* 0-based scenes; limits 2..4 (1-based) = 1..3 */
    static const struct { int dir, loops; int want[12]; } rows[] = {
        { LJ_DIR_FWD,    0, { 2, 3, 1, 2, 3, 1, 2, T_STOP } },
        { LJ_DIR_FWD,    1, { 2, 3, T_END } },
        { LJ_DIR_FWD,    2, { 2, 3, 1, 2, 3, T_END } },
        { LJ_DIR_REV,    0, { 2, 1, 3, 2, 1, 3, T_STOP } },
        { LJ_DIR_REV,    1, { 2, 1, T_END } },
        { LJ_DIR_REV,    2, { 2, 1, 3, 2, 1, T_END } },
        { LJ_DIR_BOUNCE, 0, { 2, 3, 2, 1, 2, 3, 2, 1, 2, T_STOP } },   /* 1->2->3->4->3->2->1->2: first is not repeated at the turn */
        { LJ_DIR_BOUNCE, 1, { 2, 3, 2, 1, T_END } },                   /* LJ_BOUNCE_END_AT_FIRST: one loop = first .. last .. first */
        { LJ_DIR_BOUNCE, 2, { 2, 3, 2, 1, 2, 3, 2, 1, T_END } },
    };
    static const int ends[] = { LJ_END_STOP, LJ_END_OUT, LJ_END_GOTO, LJ_END_LINK, LJ_END_LINK /* + Edit Cue */ };
    lj_runner r; uint32_t rng = LJ_RNG_SEED, link; int i, j, e, rc, nrows = 0;
    (void)argc; (void)argv;

    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        for (e = 0; e < (int)(sizeof ends / sizeof ends[0]); e++) {
            int edit = e == 4, held;
            lj_t_runner(&r, rows[i].dir, 2, 4, rows[i].loops, ends[e], 5, 9);
            for (j = 0; rows[i].want[j] >= 0; j++) {
                link = 0;
                rc = lj_runner_loop_step(&r, rows[i].dir, &rng, edit, &link);
                if (rc != 0 || r.scene != rows[i].want[j]) { fails++; printf("FAIL row %d end %d step %d: rc %d scene %u want %d\n", i, e, j, rc, r.scene, rows[i].want[j]); break; }
            }
            nrows++;
            if (rows[i].want[j] == T_STOP) { CHECK(!r.stopped && !r.out && r.loop_count >= 1); continue; }   /* unlimited: the end action never fires */
            held = r.scene; link = 0;
            rc = lj_runner_loop_step(&r, rows[i].dir, &rng, edit, &link);
            CHECK(r.loop_count == rows[i].loops);
            switch (ends[e]) {
            case LJ_END_STOP: CHECK(rc == 1 && r.stopped && r.scene == held); break;                 /* Stop on scene: holds the last scene reached */
            case LJ_END_OUT:  CHECK(rc == 1 && r.out && r.scene == held); break;                     /* Sequence out: the slot disables itself */
            case LJ_END_GOTO: CHECK(rc == 2 && r.stopped && r.scene == 4); break;                    /* Goto scene 5 and stop (the caller enters it) */
            default:
                if (edit) CHECK(rc == 1 && r.stopped && !r.link && link == 0);                       /* Edit Cue: the link does not fire */
                else      CHECK(rc == 1 && r.stopped && r.link && link == 9);                        /* Link to cue 9: stop + pending_link_cue */
                break;
            }
        }
    }
    CHECK(nrows == 45);

    /* random: inside 2..4, never twice in a row, loops ignored (LJ_LOOP_RANDOM_NO_COUNT), every end action inert, seeded */
    for (e = 0; e < 4; e++) {
        int ok = 1, prev, seen[5] = { 0 };
        uint32_t seed = LJ_RNG_SEED;
        lj_t_runner(&r, LJ_DIR_RANDOM, 2, 4, 2, ends[e], 5, 9);
        CHECK(r.loops_left == 0);
        prev = r.scene;
        for (i = 0; i < 300; i++) {
            link = 0;
            if (lj_runner_loop_step(&r, LJ_DIR_RANDOM, &seed, 0, &link) != 0 || r.scene < 1 || r.scene > 3 || r.scene == prev || link) ok = 0;
            seen[r.scene]++; prev = r.scene;
        }
        CHECK(ok && seen[1] && seen[2] && seen[3] && r.loop_count == 0 && !r.stopped && !r.out);
    }
    { uint32_t s1 = LJ_RNG_SEED, s2 = LJ_RNG_SEED; uint16_t a;   /* deterministic from the seed (frame dumps) */
      lj_t_runner(&r, LJ_DIR_RANDOM, 1, 5, 0, LJ_END_STOP, 0, 0); lj_runner_loop_step(&r, LJ_DIR_RANDOM, &s1, 0, &link); a = r.scene;
      lj_t_runner(&r, LJ_DIR_RANDOM, 1, 5, 0, LJ_END_STOP, 0, 0); lj_runner_loop_step(&r, LJ_DIR_RANDOM, &s2, 0, &link); CHECK(r.scene == a); }
    { int ok = 1;                                              /* two scenes: random alternates */
      lj_t_runner(&r, LJ_DIR_RANDOM, 2, 3, 0, LJ_END_STOP, 0, 0);
      for (i = 0; i < 20; i++) { int p = r.scene; lj_runner_loop_step(&r, LJ_DIR_RANDOM, &rng, 0, &link); if (r.scene == p) ok = 0; }
      CHECK(ok); }

    /* identical first and last: nothing to advance, status Stop [03 s5.1] */
    lj_t_runner(&r, LJ_DIR_FWD, 2, 2, 0, LJ_END_STOP, 0, 0);
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 1);
    /* limits changed under a running slot (LOOP_OPTS): a scene outside steps to the edge (LJ_LOOP_OUTSIDE_ENTERS_EDGE), no loop counted */
    lj_t_runner(&r, LJ_DIR_FWD, 2, 4, 1, LJ_END_STOP, 0, 0); r.scene = 0;
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 0 && r.scene == 1 && r.loop_count == 0);
    lj_t_runner(&r, LJ_DIR_REV, 2, 4, 1, LJ_END_STOP, 0, 0); r.scene = 4;
    CHECK(lj_runner_loop_step(&r, LJ_DIR_REV, &rng, 0, &link) == 0 && r.scene == 3 && r.loop_count == 0);
    /* lj_runner_set_loop keeps the running scene and resets the counters */
    lj_t_runner(&r, LJ_DIR_FWD, 2, 4, 2, LJ_END_STOP, 0, 0);
    for (i = 0; i < 4; i++) lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link);
    CHECK(r.loop_count == 1 && r.scene == 2);
    { lj_cue_slot s; memset(&s, 0, sizeof s); s.dir = LJ_DIR_REV; s.first = 1; s.last = 5; s.loops = 1;
      lj_runner_set_loop(&r, &s);
      CHECK(r.scene == 2 && r.loop_count == 0 && r.dir == LJ_DIR_REV && r.first == 1 && r.last == 5 && r.loops_left == 1); }

    printf("test_runner_loop: %d rows, %d failure(s)\n", nrows, fails);
    return fails ? 1 : 0;
}
