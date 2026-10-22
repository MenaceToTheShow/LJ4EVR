/* test_runner_loop.c - `lj4evr_tests test_runner_loop`: lj_runner_loop_step (lj_runner.h), the
 * 03 s5.5 loop rules as a pure function: fwd / rev / bounce / random inside first..last, finite
 * loops, the four end actions (Stop on scene, Sequence out, Goto scene, Link to cue), Edit Cue
 * silencing the link, random staying inside the limits with the fixed LJ_RNG_SEED. */
#include <stdio.h>
#include <string.h>
#include "engine/lj_runner.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void lj_t_runner(lj_runner *r, int first, int last, int loops, int end, int goto_scene, uint32_t link)
{
    lj_runner_clear(r);
    r->seq_no = 1; r->scene = (uint16_t)(first - 1); r->first = (uint16_t)first; r->last = (uint16_t)last; r->loops_left = (uint16_t)loops;
    r->end_action = (uint8_t)end; r->goto_scene = (uint16_t)goto_scene; r->link_cue = link; r->enabled = 1;
}

int lj_test_runner_loop(int argc, char **argv);
int lj_test_runner_loop(int argc, char **argv)
{
    lj_runner r; uint32_t rng = LJ_RNG_SEED, link = 0; int i, rc;
    (void)argc; (void)argv;

    lj_t_runner(&r, 1, 4, 0, LJ_END_STOP, 0, 0);               /* fwd, unlimited: 0 1 2 3 0 */
    for (i = 1; i <= 4; i++) { CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 0); CHECK(r.scene == i % 4); }
    CHECK(r.loop_count == 1 && !r.stopped);

    lj_t_runner(&r, 1, 4, 0, LJ_END_STOP, 0, 0); r.scene = 0;  /* rev from 0 wraps to 3 */
    CHECK(lj_runner_loop_step(&r, LJ_DIR_REV, &rng, 0, &link) == 0 && r.scene == 3);
    CHECK(lj_runner_loop_step(&r, LJ_DIR_REV, &rng, 0, &link) == 0 && r.scene == 2);

    lj_t_runner(&r, 1, 4, 0, LJ_END_STOP, 0, 0); r.scene = 0;  /* bounce */
    { static const int want[] = { 1, 2, 3, 2, 1, 0, 1 }; int ok = 1;   /* 0 1 2 3 2 1 0, then the step past first ends the loop (LJ_BOUNCE_END_AT_FIRST) and restarts at first + 1 */
      for (i = 0; i < 7; i++) { lj_runner_loop_step(&r, LJ_DIR_BOUNCE, &rng, 0, &link); if (r.scene != want[i]) ok = 0; }
      CHECK(ok); CHECK(r.loop_count == 1); }

    lj_t_runner(&r, 2, 5, 1, LJ_END_STOP, 0, 0);               /* limits 2..5 (1-based), one loop, Stop on scene: holds the last scene reached */
    r.scene = 1;
    for (i = 0; i < 3; i++) CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 0);
    CHECK(r.scene == 4);
    rc = lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link);
    CHECK(rc == 1 && r.stopped && r.scene == 4 && r.loop_count == 1);

    lj_t_runner(&r, 1, 2, 1, LJ_END_OUT, 0, 0);                /* Sequence out */
    lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link);
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 1 && r.out);

    lj_t_runner(&r, 1, 2, 1, LJ_END_GOTO, 2, 0);               /* Goto scene 2 (1-based) and stop: rc 2, the caller enters it */
    lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link);
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 2 && r.stopped && r.scene == 1);

    lj_t_runner(&r, 1, 2, 1, LJ_END_LINK, 0, 7);               /* Link to cue 7: stopped, link set, number handed out */
    lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link);
    link = 0;
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 1 && r.stopped && r.link && link == 7);
    lj_t_runner(&r, 1, 2, 1, LJ_END_LINK, 0, 7);               /* Edit Cue: links do not fire */
    lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 1, &link);
    link = 0;
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 1, &link) == 1 && r.stopped && !r.link && link == 0);

    lj_t_runner(&r, 3, 5, 0, LJ_END_STOP, 0, 0);               /* random stays inside 3..5 (0-based 2..4); no loop count in random mode */
    { int ok = 1; uint32_t seed = LJ_RNG_SEED, first;
      lj_runner_loop_step(&r, LJ_DIR_RANDOM, &seed, 0, &link); first = r.scene;
      for (i = 0; i < 200; i++) { if (lj_runner_loop_step(&r, LJ_DIR_RANDOM, &seed, 0, &link) != 0 || r.scene < 2 || r.scene > 4) ok = 0; }
      CHECK(ok); CHECK(r.loop_count == 0 && !r.stopped);
      seed = LJ_RNG_SEED; lj_t_runner(&r, 3, 5, 0, LJ_END_STOP, 0, 0); lj_runner_loop_step(&r, LJ_DIR_RANDOM, &seed, 0, &link);
      CHECK(r.scene == first); }                               /* deterministic from the seed (frame dumps) */

    lj_t_runner(&r, 2, 2, 0, LJ_END_STOP, 0, 0);               /* identical first and last: nothing to advance, status Stop [03 s5.1] */
    CHECK(lj_runner_loop_step(&r, LJ_DIR_FWD, &rng, 0, &link) == 1);

    printf("test_runner_loop: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
