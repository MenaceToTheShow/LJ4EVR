/* test_statics.c - `lj4evr_tests test_statics`: Statics 2 through the engine (rank 4, 03 s8, 14 sB.7): the global slot
 * index and caps (ME-30), LTP per channel (activate A then B -> B wins; deactivate B -> A returns; a re-activation
 * tops the list), the per-slot level scaling every value (50 % halves), the PROPOSAL scopes (Add / Replace Slot /
 * Page / Group / All, Solo = Replace), Release Page / Release All, the 65th activation releasing the oldest, the view's
 * static_slot_of / static_level, and Use Fadetime with a freeze across the fade (ME-80: the pinned static is shifted
 * at unfreeze). statics_ltp_reorder.lj4r is the replay side. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_statics.h"
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"

_Static_assert(LJ_STATICS2_SLOTS_PER_PAGE <= LJ_STATICS2_SLOTS_MAX && LJ_STATICS2_SLOTS_MAX == 64, "ME-30 cap");
_Static_assert(LJ_STATIC_GROUPS == 64, "the uint64_t group mask");
_Static_assert(LJ_STATIC_RUNNERS == 64, "64 static runners");

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { T_SLOTS = 70 };
static lj_static_slot s_slot[T_SLOTS];
static lj_scene s_scene[T_SLOTS];
static uint64_t s_now;

static void lj_t_cmd(lj_engine *e, int kind, uint32_t a, uint32_t b, uint32_t c, const lj_static_slot *st)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c; cmd.st = st;
    lj_engine_cmd(e, &cmd);
}
static void lj_t_on(lj_engine *e, int g, uint32_t level, uint32_t scope) { lj_t_cmd(e, LJ_CMD_STATIC_ACTIVATE, (uint32_t)g, level, scope, &s_slot[g < T_SLOTS ? g : 0]); }
static void lj_t_off(lj_engine *e, uint32_t a, uint32_t page) { lj_t_cmd(e, LJ_CMD_STATIC_DEACTIVATE, a, 0, page, NULL); }
static const lj_frame *lj_t_tick(lj_engine *e) { lj_engine_tick(e, s_now); s_now += LJ_TICK_US_DEFAULT; return lj_engine_frame(e); }

/* slot index i: a recorded scene with channel ch at v (Snap), add scope, level 100 */
static lj_static_slot *lj_t_slot(int i, int ch, uint8_t v)
{
    lj_static_slot *s = &s_slot[i];
    memset(s, 0, sizeof *s); memset(&s_scene[i], 0, sizeof s_scene[i]);
    s->kind = LJ_STATIC_RECORDED; s->level_pct = 100; s->replace = LJ_REP_ADD; s->fade_ms = 50; s->scene = &s_scene[i];
    s_scene[i].val[ch] = v; s_scene[i].state[ch] = LJ_SNAP;
    return s;
}

static int lj_t_active(lj_engine *e, int g)                  /* the view reports global slot g on some runner */
{
    lj_engine_status st; int k;
    lj_engine_view(e, &st);
    for (k = 0; k < LJ_STATIC_RUNNERS; k++) if (st.static_slot_of[k] == g && st.static_active[k]) return 1;
    return 0;
}

int lj_test_statics(int argc, char **argv);
int lj_test_statics(int argc, char **argv)
{
    static lj_profile dim;
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    lj_engine *e = lj_engine_create();
    const lj_frame *f;
    lj_engine_status st;
    int i, k, n;
    (void)argc; (void)argv;
    CHECK(lj_static_global_index(1, 1) == 0);
    CHECK(lj_static_global_index(1, LJ_STATICS2_SLOTS_PER_PAGE) == LJ_STATICS2_SLOTS_PER_PAGE - 1);
    CHECK(lj_static_global_index(2, 1) == LJ_STATICS2_SLOTS_MAX);
    CHECK(lj_static_global_index(LJ_STATIC_PAGES_MAX, LJ_STATICS2_SLOTS_MAX) == LJ_STATIC_PAGES_MAX * LJ_STATICS2_SLOTS_MAX - 1);
    CHECK(lj_static_global_index(1, LJ_STATICS2_SLOTS_MAX + 1) == -1);
    CHECK(lj_static_global_index(0, 1) == -1 && lj_static_global_index(1, 0) == -1 && lj_static_global_index(65, 1) == -1 && lj_static_global_index(1, 65) == -1);
    if (!show || !e) { printf("FAIL alloc\n"); free(show); lj_engine_destroy(e); return 1; }

    memset(&dim, 0, sizeof dim); dim.footprint = 1; dim.channel_class[0] = LJ_CH_INTENSITY; dim.use_gm = 1; dim.intensity.enabled = 1; dim.intensity.max = 255;
    for (i = 0; i < 4; i++) { show->fixture[i].kind = LJ_FX_BUILTIN; show->fixture[i].link = 1; show->fixture[i].address = (uint16_t)(i + 1); }
    lj_engine_bind(e, show);
    for (i = 0; i < 4; i++) lj_engine_bind_profile(e, i, &dim);

    /* LTP: A (1.1) = 100 then B (1.2) = 200 on channel 1:1 */
    lj_t_slot(0, 0, 100); lj_t_slot(1, 0, 200);
    lj_t_on(e, 0, 0xFFFF, 0xFF); f = lj_t_tick(e);
    CHECK(f->merged[0] == 100 && f->layer_of[0] == LJ_L_STATICS);
    lj_t_on(e, 1, 0xFFFF, 0xFF); f = lj_t_tick(e);
    CHECK(f->merged[0] == 200);                                /* B activated later wins */
    lj_t_off(e, 1, 0); f = lj_t_tick(e);
    CHECK(f->merged[0] == 100 && !lj_t_active(e, 1));          /* deactivate B: A returns (deactivation reorders) */
    lj_t_on(e, 1, 50, 0xFF); f = lj_t_tick(e);
    CHECK(f->merged[0] == 100);                                /* level 50 % halves B's 200 */
    lj_engine_view(e, &st);
    for (k = 0, n = 0; k < LJ_STATIC_RUNNERS; k++) if (st.static_slot_of[k] == 1) { n++; CHECK(st.static_level[k] == 50); }
    CHECK(n == 1 && st.statics_any);
    lj_t_on(e, 0, 30, 0xFF); f = lj_t_tick(e);
    CHECK(f->merged[0] == 30);                                 /* re-activating A (at 30 %) tops the list again; one runner per slot (re-activation reuses it) */
    lj_engine_view(e, &st);
    for (k = 0, n = 0; k < LJ_STATIC_RUNNERS; k++) n += st.static_slot_of[k] == 0;
    CHECK(n == 1);
    lj_t_off(e, LJ_RUN_ALL, 0); f = lj_t_tick(e);              /* Release All: rank 0, snapped (LJ_STATIC_RELEASE_SNAPS) */
    CHECK(f->merged[0] == 0 && f->layer_of[0] == LJ_L_DEFAULT && !lj_t_active(e, 0) && !lj_t_active(e, 1));

    /* scopes (PROPOSAL rig 14 sB.8 item 3): 1.1 = global 0, 1.2 = 1, 2.1 = 64, 2.2 = 65 */
    lj_t_slot(0, 0, 10); lj_t_slot(1, 1, 11); lj_t_slot(64, 2, 12); lj_t_slot(65, 3, 13);
    lj_t_on(e, 0, 0xFFFF, 0xFF); lj_t_on(e, 1, 0xFFFF, 0xFF);
    lj_t_on(e, 64, 0xFFFF, LJ_REP_SLOT); f = lj_t_tick(e);    /* Replace Slot: same slot row on any page (1.1 goes, 1.2 stays) */
    CHECK(!lj_t_active(e, 0) && lj_t_active(e, 1) && lj_t_active(e, 64) && f->merged[0] == 0 && f->merged[1] == 11 && f->merged[2] == 12);
    lj_t_on(e, 65, 0xFFFF, LJ_REP_PAGE); lj_t_tick(e);         /* Replace Page: 2.1 goes, 1.2 stays */
    CHECK(!lj_t_active(e, 64) && lj_t_active(e, 1) && lj_t_active(e, 65));
    lj_t_on(e, 0, 0xFFFF, LJ_REP_SOLO_ALL); lj_t_tick(e);      /* Solo All = Replace All (LJ_STATIC_SOLO_BLOCKS_LATER 0) */
    CHECK(lj_t_active(e, 0) && !lj_t_active(e, 1) && !lj_t_active(e, 65));
    lj_t_on(e, 1, 0xFFFF, LJ_REP_ADD); lj_t_tick(e);           /* Add: nothing released */
    CHECK(lj_t_active(e, 0) && lj_t_active(e, 1));
    s_slot[0].groups = 1; s_slot[1].groups = 2; s_slot[64].groups = 3;   /* group bits */
    lj_t_on(e, 64, 0xFFFF, LJ_REP_GROUP); lj_t_tick(e);        /* Replace Group: shares a bit with both */
    CHECK(!lj_t_active(e, 0) && !lj_t_active(e, 1) && lj_t_active(e, 64));
    s_slot[65].replace = LJ_REP_PAGE;                          /* 0xFF = the slot's own scope */
    lj_t_on(e, 0, 0xFFFF, 0xFF); lj_t_on(e, 65, 0xFFFF, 0xFF); lj_t_tick(e);
    CHECK(lj_t_active(e, 0) && !lj_t_active(e, 64) && lj_t_active(e, 65));
    lj_t_off(e, LJ_RUN_ALL, 2); lj_t_tick(e);                  /* Release Page 2 */
    CHECK(lj_t_active(e, 0) && !lj_t_active(e, 65));
    lj_t_off(e, LJ_RUN_ALL, 0); lj_t_tick(e);

    /* the 65th activation releases the oldest (LJ_STATIC_OLDEST_RELEASED, rig 14 sB.8 item 10) */
    for (i = 0; i < 65; i++) { lj_t_slot(i, i % 4, (uint8_t)(i + 1)); lj_t_on(e, i, 0xFFFF, 0xFF); }
    lj_t_tick(e);
    CHECK(!lj_t_active(e, 0) && lj_t_active(e, 1) && lj_t_active(e, 64));
    lj_engine_view(e, &st);
    for (k = 0, n = 0; k < LJ_STATIC_RUNNERS; k++) n += st.static_active[k];
    CHECK(n == LJ_STATIC_RUNNERS);
    lj_t_off(e, LJ_RUN_ALL, 0); lj_t_tick(e);
    CHECK(!lj_t_active(e, 64));
    /* bad input is ignored: a slot index past 64 pages, a NULL record */
    lj_t_cmd(e, LJ_CMD_STATIC_ACTIVATE, LJ_STATIC_PAGES_MAX * LJ_STATICS2_SLOTS_MAX, 0xFFFF, 0xFF, &s_slot[0]);
    lj_t_cmd(e, LJ_CMD_STATIC_ACTIVATE, 3, 0xFFFF, 0xFF, NULL);
    lj_t_tick(e); lj_engine_view(e, &st);
    CHECK(!st.statics_any);

    /* Use Fadetime + freeze (ME-80): 1.5 fades 0 -> 200 over 1000 ms; frozen 1 s at 500 ms; resumes from 100 */
    lj_t_slot(4, 1, 200); s_scene[4].state[1] = LJ_FADE; s_slot[4].fade_ms = 1000;
    lj_t_cmd(e, LJ_CMD_PREFS, LJ_PREF_STATICS_USE_FADETIME, 1, 0, NULL);
    s_now = 10000000;
    lj_t_on(e, 4, 0xFFFF, 0xFF);
    for (i = 0; i < 21; i++) f = lj_t_tick(e);                 /* last tick at t0 + 500 ms */
    CHECK(f->merged[1] == 100);
    lj_t_cmd(e, LJ_CMD_FREEZE, 0, 1, 0, NULL);
    for (i = 0; i < 40; i++) f = lj_t_tick(e);                 /* 1 s frozen: held */
    CHECK(f->merged[1] == 100);
    lj_t_cmd(e, LJ_CMD_FREEZE, 0, 0, 0, NULL);
    for (i = 0; i < 10; i++) f = lj_t_tick(e);                 /* 250 ms after unfreeze: 750 ms into the fade */
    CHECK(f->merged[1] >= 145 && f->merged[1] <= 155);
    lj_t_cmd(e, LJ_CMD_PREFS, LJ_PREF_STATICS_USE_FADETIME, 0, 0, NULL);
    lj_t_off(e, 4, 0); lj_t_on(e, 4, 0xFFFF, 0xFF); f = lj_t_tick(e);
    CHECK(f->merged[1] == 200);                                /* default: every value snaps (F17) */

    lj_engine_bind(e, NULL); lj_engine_destroy(e); free(show);
    printf("test_statics: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
