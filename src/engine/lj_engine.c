/* lj_engine.c - the tick (design 2.3, 3.1, 3.2): create/bind, the command array
 * drained at tick start, scheduling, evaluation, merge, post chain, publish.
 * No I/O, no lj_table_*, no allocation after create (ME-2, ME-6). Rule ids in
 * comments are note 11 (E/F/G/P/C) and the council rows. */
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_runner.h"
#include "engine/lj_merge.h"
#include "engine/lj_post.h"
#include "engine/lj_masters.h"
#include "engine/lj_movemacro.h"
#include "engine/lj_genmacro.h"
#include "fixlib/lj_profile.h"
#include "core/lj_log.h"
#include "core/lj_clock.h"

static const uint8_t lj_engine_zero[LJ_CHANS] = { 0 };       /* rank 0 of an unbound engine */

enum { LJ_MM_GAIN = 256 };   /* W3 movement: the Cue macro control's amplitude gain at 100 % (Q8) */
struct lj_engine;
static void lj_engine_mm_ctl(struct lj_engine *e, const lj_cmd *c, uint64_t now);

struct lj_engine {
    lj_frame               frame;
    lj_cmd                 cmds[LJ_ENGINE_CMD_CAP];     /* design 2.3 rule 1: a plain array, appended under lj_lock */
    int                    ncmd;
    const lj_show         *show;
    lj_runner              run[LJ_RUN_COUNT];
    lj_post_ctx            post;
    lj_engine_status       st;
    uint32_t               tick_us;
    const void            *sacn_cfg;                    /* parked by LJ_CMD_SACN_CFG until lj_engine_take_sacn_cfg (ME-23) */
    /* W1 state */
    lj_pairs               pairs;                       /* declared 16-bit pairs (ME-22) */
    const lj_cue          *cue; uint8_t cue_edit; uint16_t cue_pct; uint16_t macro_amp_pct, macro_speed_pct;   /* W3: 0..999 [03 s5.11] */
    struct { uint16_t gain, from, to; uint8_t fade, frozen; uint64_t t0_us; uint32_t ms; } mmc;   /* W3 MACRO_FADE: amplitude gain 0..LJ_MM_GAIN (Fade out / in, Off / On), Freeze (movement track) */
    const lj_bgcue        *bg;
    lj_gm_rank6            gm;                          /* rank 6: the cue's and the editor's generic macro, shapes, engine / slot on (W3 genmacro) */
    const lj_static_slot  *static_slot[LJ_STATIC_RUNNERS];
    lj_mm_state            mm[LJ_FIXTURES];             /* movement-macro indexes, advanced once per tick (ME-35) */
    const lj_movemacro    *mm_src[LJ_FIXTURES]; uint8_t mm_cue[LJ_FIXTURES];   /* the macro the previous tick's merged[] carried per fixture, and whether a cue slot won it */
    lj_gm_fade             gm_fade;
    uint8_t                frozen; uint64_t frozen_since_us;
    uint8_t                seq_int_mode;
    uint32_t               chase_us; uint64_t tap_us[2];  /* global chase period from the last two taps [03 s5.5] (W2: per-slot rates, Save Chase Times) */
    uint32_t               rng;
    uint64_t               now_us;
    /* W2 state (design 10 W2) */
    const uint8_t         *default_out;                 /* rank 0 source: show->default_out at bind, re-read by LJ_CMD_DEFAULT_OUT */
    uint8_t                transparent_mode;            /* enum lj_transparent_mode, applied at LOAD_CUE / LOAD_BG [03 s5.8] */
    uint8_t                flash_latch;                 /* the flash runner was started latched [03 s5.10] */
    uint32_t               ltp_clock;                   /* LJ_STATIC_LTP_COUNTER: activation order of the statics */
    lj_frame              *snap_dst; uint8_t snap_src;  /* SNAPSHOT_REQ: UI-owned frame filled at the end of the next tick */
    uint8_t                blind_val[LJ_CHANS], blind_st[LJ_CHANS];   /* the blind buffer kept across a scene change while blind (LORD_ENGINE-13) */
    struct { uint8_t held, timer, level, haze, fan, firing; uint64_t until_us, t0_us; uint32_t duration_ms, cycle_ms, elapsed_ms; } smoke;   /* rank 7 [11 C12, 04 s12] */
};

lj_engine *lj_engine_create(void)
{
    lj_engine *e = (lj_engine *)calloc(1, sizeof(lj_engine));     /* the one allocation; NULL on failure (no lj_fatal path in engine/) */
    if (e) {
        e->tick_us = LJ_TICK_US_DEFAULT;
        memset(e->post.fixture_of, 0xFF, sizeof e->post.fixture_of);
        memset(e->pairs.lsb_of, 0xFF, sizeof e->pairs.lsb_of);
        e->post.gm = 255; memset(e->post.sub, 255, sizeof e->post.sub);
        e->post.solo_fixture = -1;
        e->cue_pct = 100; e->macro_amp_pct = e->macro_speed_pct = 100;
        e->mmc.gain = e->mmc.to = LJ_MM_GAIN;
        e->rng = LJ_RNG_SEED;
        e->transparent_mode = LJ_TRANSPARENT_DEFAULT;
        memset(e->st.static_slot_of, 0xFF, sizeof e->st.static_slot_of);
        e->st.prefs[LJ_PREF_KEEP_SEQ_BLIND] = LJ_KEEP_CONTENTS_BLIND;
        e->st.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] = LJ_CHASE_GLOBAL_RATE;
        e->st.prefs[LJ_PREF_STATICS_USE_FADETIME] = LJ_STATICS_USE_FADETIME;
        e->smoke.level = LJ_SMOKE_LEVEL_DEFAULT; e->smoke.duration_ms = LJ_SMOKE_DURATION_DEFAULT_MS; e->smoke.cycle_ms = LJ_SMOKE_CYCLE_DEFAULT_MS;
        e->st.gm_engine_on = e->st.gm_slot_on = 1; e->st.mm_amp_pct = e->st.mm_speed_pct = 100;   /* W3 view defaults (the engine track keeps them live) */
    }
    return e;
}

void lj_engine_destroy(lj_engine *e)
{
    free(e);
}

static void lj_engine_rebuild(lj_engine *e)
{
    int f;
    memset(e->post.fixture_of, 0xFF, sizeof e->post.fixture_of);
    if (!e->show) { for (f = 0; f < LJ_FIXTURES; f++) e->post.profile_of[f] = NULL; lj_runner_pairs_build(&e->post, &e->pairs); return; }
    for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_fixture *fx = &e->show->fixture[f];
        const lj_profile *p;
        int ofs, code;
        if (!fx->link) { e->post.profile_of[f] = NULL; continue; }
        if (fx->address < 1) continue;                             /* no DMX address yet (F41 warning state): unpatched; s7, the overlay and pan_winner skip it through lj_fx_code (ME-132) */
        if (fx->kind == LJ_FX_BUILTIN) { p = lj_fixlib_by_id(fx->profile_id); if (p) e->post.profile_of[f] = p; }   /* built-ins resolve here; a GS2 (or test) profile stays from lj_engine_bind_profile */
        p = e->post.profile_of[f];
        if (!p) continue;
        for (ofs = 0; ofs < p->footprint && ofs < LJ_PROFILE_CH; ofs++) {
            if ((code = lj_fx_code(fx, ofs)) < 0) break;       /* ME-132: clipped at the link end, never into the next link */
            if (p->level_disabled[ofs]) continue;              /* LORD_FIXTURES-11: a disabled offset is unpatched, output = default_out */
            e->post.fixture_of[code] = (int16_t)f;
        }
    }
    lj_runner_pairs_build(&e->post, &e->pairs);
}

void lj_engine_bind(lj_engine *e, const lj_show *show)
{
    e->show = show;
    e->post.show = show;
    e->post.patch = show ? show->patch : NULL;                 /* S1/S5/S6 table until a GLOBAL_PATCH / HTP_GROUPS swap */
    e->default_out = show ? show->default_out : NULL;          /* rank 0 until a DEFAULT_OUT */
    lj_engine_rebuild(e);
    if (e->frame.tick_no == 0) memcpy(e->frame.merged, e->default_out ? e->default_out : lj_engine_zero, LJ_CHANS);   /* before the first tick the output at rest is DefaultOut: a fade trigged at tick 0 starts from it (F3) */
}

void lj_engine_bind_profile(lj_engine *e, int fixture, const lj_profile *p)
{
    if (fixture < 0 || fixture >= LJ_FIXTURES) return;
    e->post.profile_of[fixture] = p;
    lj_engine_rebuild(e);
}

void lj_engine_rebind(lj_engine *e)
{
    lj_engine_bind(e, e->show);
}

int lj_engine_cmd(lj_engine *e, const lj_cmd *cmd)
{
    if (e->ncmd >= LJ_ENGINE_CMD_CAP) {
        e->st.drops++;
        lj_log("engine: command array full (%d), dropped kind %u", LJ_ENGINE_CMD_CAP, (unsigned)cmd->kind);
        return -1;
    }
    e->cmds[e->ncmd++] = *cmd;
    return 0;
}

/* ---- step 1: the commands ---- */

static int lj_engine_slot_id(uint32_t a, int lo, int n) { return (a >= (uint32_t)lo && a < (uint32_t)(lo + n)) ? (int)a - lo : -1; }

static int lj_engine_transparent(const lj_engine *e, int flag)   /* Preferences > Transparent Cues [03 s5.8]: Use honours the saved flag, Force / Ignore override it */
{
    return e->transparent_mode == LJ_TRANSPARENT_FORCE || (e->transparent_mode == LJ_TRANSPARENT_USE && flag);
}

static void lj_engine_load_bg(lj_engine *e, const lj_bgcue *bg, const lj_sequence *const *seqs, uint64_t t)
{
    int k, transparent = bg && lj_engine_transparent(e, bg->transparent);
    e->bg = bg;
    for (k = 0; k < LJ_BG_SLOTS; k++) {
        lj_runner *r = &e->run[LJ_RUN_BG0 + k];
        if (!bg || !bg->slot[k].seq) { if (!transparent) lj_runner_clear(r); continue; }   /* transparent: empty slots do not overwrite [03 s11.3] */
        lj_runner_set_seq(r, seqs ? seqs[k] : NULL, bg->slot[k].seq);   /* LJ_BG_FORWARD_ONLY: set_seq is forward, auto, programmed times */
#if LJ_BG_RESTORE_ONOFF
        r->enabled = bg->slot[k].enabled ? 1 : 0;              /* Restore Sequence on/off state */
#else
        r->enabled = 1;                                        /* default: every slot loads enabled [03 s7] */
#endif
        if (bg->slot[k].mode == LJ_BG_NOLOOP) { r->loops_left = 1; r->end_action = LJ_END_STOP; }   /* -L: stops after the last scene [03 s7] */
        if (bg->slot[k].mode == LJ_BG_AUDIO) r->trig = LJ_TRIG_AUDIO;                                 /* W2: audio analyser trigs */
        lj_runner_start(r, t, e->tick_us, e->frame.merged, 100, &e->rng);
    }
}

static void lj_engine_load_cue(lj_engine *e, const lj_cmd *c, uint64_t t)
{
    const lj_cue *cue = c->cue;
    int k, transparent;
    e->cue = cue; e->cue_edit = (uint8_t)(c->b != 0);
#if LJ_FLASH_RELEASE_ON_LOAD
    lj_runner_clear(&e->run[LJ_RUN_FLASH]); e->flash_latch = 0;   /* Release Flash on Load */
#endif
    if (!cue) { for (k = 0; k < LJ_CUE_SLOTS; k++) lj_runner_clear(&e->run[k]); e->st.cue_number = 0; lj_gm_rank6_load_cue(&e->gm, NULL); return; }   /* New cue: clears the cue control, its macro slot too */
    e->cue_pct = cue->cue_time_pct ? (cue->cue_time_pct > LJ_TIME_PCT_MAX ? LJ_TIME_PCT_MAX : cue->cue_time_pct) : 100;
    transparent = lj_engine_transparent(e, cue->transparent);
    if (cue->save_macro_values) { e->macro_amp_pct = cue->macro_amp_pct; e->macro_speed_pct = cue->macro_speed_pct; }   /* W2: Restore Macro Control Values preference */
    for (k = 0; k < LJ_CUE_SLOTS; k++) {
        lj_runner *r = &e->run[k];
        const lj_cue_slot *s = &cue->slot[k];
        if (!s->seq) { if (!transparent) lj_runner_clear(r); continue; }   /* transparent load: only slots with a sequence overwrite; the others keep their runner, on/off included (LJ_TRANSPARENT_KEEPS_ONOFF) [03 s5.8] */
        lj_runner_set_slot(r, s, c->seq[k]);                   /* seq NULL = load error [03 s5.1] */
        if (!r->seq) continue;
        lj_runner_start(r, t, e->tick_us, e->frame.merged, e->cue_pct, &e->rng);
        if (s->trig_on_load && r->trig != LJ_TRIG_AUTO && r->scenes_per_trig > 1) r->spt_left = (uint8_t)(r->scenes_per_trig - 1);   /* LJ_TRIG_ON_LOAD_RUNS_SPT */
    }
    e->st.cue_number = cue->number;
    if (cue->bg_cue) { if (c->bg) lj_engine_load_bg(e, c->bg, c->bgseq, t); else e->st.pending_bg = cue->bg_cue; }   /* latched bg cue: pinned here or requested (2.3 rule 3); W2: Disable BG Cue Latch preference, BG latch from the cue list / 2532 */
    if (cue->gen_macro) {                                      /* latched generic macro: pinned here or requested (2.3 rule 3, ME-2) */
        lj_gm_rank6_load_cue(&e->gm, c->gm);                   /* NULL until the UI answers pending_genmacro: the old cue's macro stops */
        if (!c->gm) e->st.pending_genmacro = cue->gen_macro;
    } else if (!transparent) lj_gm_rank6_load_cue(&e->gm, NULL);   /* a cue with an empty macro slot clears it (PROPOSAL rig 14 sA.9; a transparent cue keeps it) */
}

static void lj_engine_freeze(lj_engine *e, int on, uint64_t now)
{
    int i;
    if (on == e->frozen) return;
    e->frozen = e->post.freeze = (uint8_t)on;
    if (on) { e->frozen_since_us = now; return; }
    for (i = 0; i < LJ_RUN_COUNT; i++) {                       /* F20 / ME-13 unfreeze (LJ_FREEZE_PAUSED_CLOCK, rig M10): a fade from before the freeze keeps its remaining time; one trigged while frozen starts at the unfreeze instant from the frozen merged[] (from[] was captured from it at the trig). The literal max(t0 + frozen, unfreeze) would put a frozen-time trig in the future, which 3.5 forbids. */
        lj_runner *r = &e->run[i];
        if (!lj_runner_active(r)) continue;                    /* ME-80: every active runner, a pinned static with a fade included */
        r->fade_t0_us = r->fade_t0_us < e->frozen_since_us ? r->fade_t0_us + (now - e->frozen_since_us) : now;
    }
    if (e->mmc.fade) e->mmc.t0_us = e->mmc.t0_us < e->frozen_since_us ? e->mmc.t0_us + (now - e->frozen_since_us) : now;   /* ME-115: the cue macro control fade keeps its remaining time (design 3.3) */
    /* the GM Fade runner is not shifted: it kept running while frozen (LJ_GM_FADE_RUNS_FROZEN, ME-71) */
}

/* ---- W2 helpers ---- */

/* One tap of the global chase timer (LORD_ENGINE-19, 11 F10, E9): the period is the gap between the last two taps
 * (manual trigs and CHASE_TAP), rounded to 1 ms; every chase-mode slot reads it (UseGlobalChaseRate), or only the
 * tapped slots without that preference. rearm: CHASE_TAP re-arms the chase slots at t + period (a TRIG enters
 * its scene, which does it). */
static void lj_engine_tap(lj_engine *e, uint64_t t, int lo, int n, int rearm)
{
    uint64_t gap;
    int k;
    e->tap_us[0] = e->tap_us[1]; e->tap_us[1] = t;
    if (!e->tap_us[0] || t <= e->tap_us[0]) return;
    gap = (t - e->tap_us[0] + 500u) / 1000u * 1000u;          /* LJ_CHASE_TAP_ROUND_MS */
    if (gap < (uint64_t)LJ_CHASE_MIN_MS * 1000u || gap > (uint64_t)LJ_CHASE_MAX_MS * 1000u) return;   /* above the max: this tap is a new first tap */
    e->chase_us = (uint32_t)gap;
    for (k = 0; k < LJ_CUE_SLOTS; k++) {
        lj_runner *r = &e->run[k];
        if (!r->chase_on || (!e->st.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] && (k < lo || k >= lo + n))) continue;
        r->chase_us = e->chase_us;
        if (rearm) r->scene_due_us = t + r->chase_us;
    }
}

/* Statics 2 scopes (14 sB.3 reading, PROPOSAL rig 14 sB.8 item 3; LJ_STATIC_SOLO_BLOCKS_LATER 0 = Solo reads as Replace):
 * the rule of model lj_static_scope_hits, evaluated on the pinned slots the engine holds rather than show->pages (the
 * engine never follows show->pages; the UI keeps the slots it pinned in place, lj_statics.h ME-91). */
static int lj_engine_static_hits(int scope, uint32_t a, const lj_static_slot *sa, uint32_t o, const lj_static_slot *so)
{
    switch (scope) {
    case LJ_REP_SLOT:  case LJ_REP_SOLO_SLOT:  return a % LJ_STATICS2_SLOTS_MAX == o % LJ_STATICS2_SLOTS_MAX;   /* same slot row, any page */
    case LJ_REP_PAGE:  case LJ_REP_SOLO_PAGE:  return a / LJ_STATICS2_SLOTS_MAX == o / LJ_STATICS2_SLOTS_MAX;
    case LJ_REP_GROUP: case LJ_REP_SOLO_GROUP: return (sa->groups & so->groups) != 0;
    case LJ_REP_ALL:   case LJ_REP_SOLO_ALL:   return 1;
    default:                                   return 0;       /* LJ_REP_ADD (LJ_STATIC_SCOPE_ADD): LTP, no release */
    }
}

static void lj_engine_static_off(lj_engine *e, int k)          /* release: snap back to the layer below (LJ_STATIC_RELEASE_SNAPS, rig M9); the record stays the UI's */
{
    lj_runner_clear(&e->run[LJ_RUN_STATIC0 + k]);
    e->static_slot[k] = NULL; e->st.static_slot_of[k] = 0xFFFF; e->st.static_level[k] = 0;
}

/* Activate global slot a (lj_static_global_index) [03 s8, 14 sB.7]: release what the scope names, then put the slot on top
 * of the LTP list (a re-activation reuses its runner; the 65th releases the oldest, LJ_STATIC_OLDEST_RELEASED). */
static void lj_engine_static_on(lj_engine *e, uint32_t a, const lj_static_slot *st, const lj_sequence *link_seq, uint32_t level, uint32_t scope, uint64_t t)
{
    int k, id = -1;
    lj_runner *r;
    if (!st || a >= (uint32_t)LJ_STATIC_PAGES_MAX * LJ_STATICS2_SLOTS_MAX) return;
    if (scope == 0xFF) scope = st->replace;
    if (level == 0xFFFF) level = st->level_pct;
    if (level > 100) level = 100;
    for (k = 0; k < LJ_STATIC_RUNNERS; k++)
        if (e->static_slot[k] && e->st.static_slot_of[k] != a && lj_engine_static_hits((int)scope, a, st, e->st.static_slot_of[k], e->static_slot[k])) lj_engine_static_off(e, k);
    for (k = 0; k < LJ_STATIC_RUNNERS && id < 0; k++) if (e->static_slot[k] && e->st.static_slot_of[k] == a) id = k;
    for (k = 0; k < LJ_STATIC_RUNNERS && id < 0; k++) if (!e->static_slot[k]) id = k;
    if (id < 0) {                                              /* all 64 runners busy: the oldest activation goes */
        id = 0;
        for (k = 1; k < LJ_STATIC_RUNNERS; k++) if (e->run[LJ_RUN_STATIC0 + k].activated_us < e->run[LJ_RUN_STATIC0 + id].activated_us) id = k;
    }
    r = &e->run[LJ_RUN_STATIC0 + id];
    lj_runner_set_seq(r, st->kind == LJ_STATIC_LINK ? link_seq : NULL, 1);   /* a linked slot evaluates its sequence's scene (NULL = link error: contributes nothing) */
    e->static_slot[id] = st; e->st.static_slot_of[id] = (uint16_t)a; e->st.static_level[id] = (uint8_t)level;
    r->trig = LJ_TRIG_MANUAL; r->running = 1;                  /* running = pinned static */
    r->activated_us = ++e->ltp_clock;                          /* LJ_STATIC_LTP_COUNTER: every activation tops the list [03 s8] */
    r->level_pct = (uint8_t)level; r->scene = st->link_scene;  /* LJ_STATIC_LEVEL_SCALES_ALL in lj_runner_eval */
    memcpy(r->from, e->frame.merged, LJ_CHANS);
    r->fade_t0_us = t;
    r->fade_dur_us = 0;                                        /* F17: snap unless [STATICS] Use Fadetime */
    if (e->st.prefs[LJ_PREF_STATICS_USE_FADETIME] && st->fade_ms > LJ_SNAP_MS) {
        uint64_t ft = (uint64_t)(st->fade_ms > LJ_TIME_MAX_MS ? LJ_TIME_MAX_MS : st->fade_ms) * 1000u;
        if (ft > e->tick_us) r->fade_dur_us = ft;              /* F28, LJ_FADE_LE_TICK_IS_SNAP */
    }
}

static void lj_engine_flash(lj_engine *e, const lj_cmd *c, uint64_t t)   /* 03 s5.10, C2: one sequence at a time, forward, programmed times */
{
    lj_runner *r = &e->run[LJ_RUN_FLASH];
    if (c->b && c->seq[0]) {
        if (c->c && e->flash_latch && r->seq == c->seq[0]) { lj_runner_clear(r); e->flash_latch = 0; return; }   /* latched: flashed again releases it */
        lj_runner_set_seq(r, c->seq[0], c->seq[0]->number);
        r->flash = 1;
        lj_runner_start(r, t, e->tick_us, e->frame.merged, 100, &e->rng);
        e->flash_latch = (uint8_t)(c->c != 0);
        return;
    }
    if (c->c && e->flash_latch) return;                        /* the key release of a latched flash: it stays */
    lj_runner_clear(r); e->flash_latch = 0;                    /* release, or Release latched Flash (b 0 c 0) */
}

static uint8_t lj_engine_u8(uint32_t v) { return (uint8_t)(v > 255 ? 255 : v); }

static void lj_engine_smoke_cmd(lj_engine *e, const lj_cmd *c, uint64_t t)   /* 04 s12, 11 C12 */
{
    switch (c->a) {
    case LJ_SMOKE_BUTTON:
        if (c->b) {
            e->smoke.held = 1;
            e->smoke.until_us = c->c ? t + (uint64_t)(c->c > LJ_SMOKE_DURATION_MAX_MS ? LJ_SMOKE_DURATION_MAX_MS : c->c) * 1000u : 0;   /* toggle X seconds, else while held */
#if LJ_SMOKE_RESET_ON_SMOKE
            e->smoke.t0_us = t;                                /* Reset cycle on smoke */
#endif
        } else if (!e->smoke.until_us) e->smoke.held = 0;      /* LJ_SMOKE_RELEASE_ENDS_HELD */
        break;
    case LJ_SMOKE_TIMER:       e->smoke.timer = (uint8_t)(c->b != 0); if (c->b) e->smoke.t0_us = t; break;   /* the cycle starts at the enable (PROPOSAL rig 04 s12) */
    case LJ_SMOKE_LEVEL:       e->smoke.level = lj_engine_u8(c->b); break;
    case LJ_SMOKE_DURATION_MS: e->smoke.duration_ms = c->b > LJ_SMOKE_DURATION_MAX_MS ? LJ_SMOKE_DURATION_MAX_MS : c->b; break;
    case LJ_SMOKE_CYCLE_MS:    e->smoke.cycle_ms = c->b < LJ_SMOKE_CYCLE_MIN_MS ? LJ_SMOKE_CYCLE_MIN_MS : c->b > LJ_SMOKE_CYCLE_MAX_MS ? LJ_SMOKE_CYCLE_MAX_MS : c->b; break;
    case LJ_SMOKE_RESET:       e->smoke.t0_us = t; break;
    case LJ_SMOKE_HAZE_LEVEL:  e->smoke.haze = lj_engine_u8(c->b); break;
    case LJ_SMOKE_FAN_LEVEL:   e->smoke.fan = lj_engine_u8(c->b); break;
    default: break;
    }
}

/* Rank 7 [11 C12, ME-25]: SMOKE channels at the smoke level while the button / timer fires, HAZE / HAZE_FAN at the hazer
 * levels as soon as they are > 0 (LJ_SMOKE_HAZE_OVERRIDE_GT0); every other class is untouched. */
static void lj_engine_smoke(lj_engine *e, uint64_t now, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch;
    uint8_t smoke_v;
    if (e->smoke.held && e->smoke.until_us && now >= e->smoke.until_us) e->smoke.held = 0;
    e->smoke.elapsed_ms = e->smoke.cycle_ms && now > e->smoke.t0_us ? (uint32_t)((now - e->smoke.t0_us) / 1000u % e->smoke.cycle_ms) : 0;
    e->smoke.firing = (uint8_t)(e->smoke.held || (e->smoke.timer && e->smoke.elapsed_ms < e->smoke.duration_ms));
#if LJ_SMOKE_BUTTON_USES_LEVEL
    smoke_v = e->smoke.level;
#else
    smoke_v = e->smoke.held ? 255 : e->smoke.level;            /* the button sends 255, the timer the Level fader */
#endif
    if (!e->show || !(e->smoke.firing || e->smoke.haze || e->smoke.fan)) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int cls = e->post.fixture_of[ch] < 0 ? LJ_CH_NONE : lj_post_class(&e->post, ch);
        int v = cls == LJ_CH_SMOKE && e->smoke.firing ? smoke_v : cls == LJ_CH_HAZE && e->smoke.haze ? e->smoke.haze : cls == LJ_CH_HAZE_FAN && e->smoke.fan ? e->smoke.fan : -1;
        if (v >= 0) { merged[ch] = (uint8_t)v; layer_of[ch] = LJ_L_SMOKE; }
    }
}

/* W5: one audio trig (LJ_TRIG_AUDIO_EVENT) to every cue-slot and BG runner in audio trig mode; "Audio-Trigs / Trig" counts [05 s9.1]. */
static void lj_engine_audio_trig(lj_engine *e, uint64_t t)
{
    int k;
    for (k = 0; k < LJ_RUN_STATIC0; k++) {
        lj_runner *r = &e->run[k];
        uint32_t link;
        if (k == LJ_RUN_FLASH || k == LJ_RUN_CURRENT || r->trig != LJ_TRIG_AUDIO) continue;
        if (!r->audio_left || r->audio_left > r->audio_per_trig) r->audio_left = r->audio_per_trig;
        if (--r->audio_left) continue;
        link = lj_runner_trig(r, 0xFF, 0, t, e->tick_us, e->frame.merged, k < LJ_CUE_SLOTS ? e->cue_pct : 100, &e->rng, e->cue_edit);
        if (link) e->st.pending_link_cue = link;
    }
}

static void lj_engine_apply(lj_engine *e, const lj_cmd *c, uint64_t now)
{
    uint64_t t = c->at_us && c->at_us <= now ? c->at_us : now;   /* UI timestamp (chase taps at 1 ms, E9); never later than this tick */
    int id, k;
    switch (c->kind) {
    case LJ_CMD_LOAD_CUE: lj_engine_load_cue(e, c, t); break;
    case LJ_CMD_LOAD_BG: lj_engine_load_bg(e, c->bg, c->bgseq, t); break;
    case LJ_CMD_LOAD_GENMACRO: case LJ_CMD_GENMACRO_ON: case LJ_CMD_GENMACRO_RESTART: case LJ_CMD_GM_SHAPES: lj_gm_rank6_cmd(&e->gm, c); break;   /* W3 genmacro */
    case LJ_CMD_CURRENT_SEQ: {
        lj_runner *r = &e->run[LJ_RUN_CURRENT];
        int was_blind = r->blind;
        if (!c->seq[0]) { lj_runner_clear(r); break; }
        if (r->seq != c->seq[0] || r->scene != c->a || !r->seq_no) {
            uint8_t blind = (uint8_t)r->blind;
            if (blind) { memcpy(e->blind_val, r->val, LJ_CHANS); memcpy(e->blind_st, r->st, LJ_CHANS); }   /* a scene change while blind keeps the buffer */
            lj_runner_set_seq(r, c->seq[0], c->seq[0]->number);
            r->trig = LJ_TRIG_MANUAL;                          /* the editor shows a scene; playback (Go) makes it auto */
            lj_runner_enter(r, (uint16_t)c->a, t, e->tick_us, e->frame.merged, 100);
            r->blind = blind;
            if (blind) { memcpy(r->val, e->blind_val, LJ_CHANS); memcpy(r->st, e->blind_st, LJ_CHANS); }
        }
        r->blind = (c->b != 0);
        if (r->blind && !was_blind && !e->st.prefs[LJ_PREF_KEEP_SEQ_BLIND]) memset(r->st, LJ_OFF, LJ_CHANS);   /* F21, LORD_ENGINE-13: eval is skipped while blind, so val[]/st[] are the buffer; [PREFS] KeepSeqBlind=0 empties rank 3 */
        break;
    }
    case LJ_CMD_GO: case LJ_CMD_STOP: {
        int go = c->kind == LJ_CMD_GO;
        if (c->a == LJ_RUN_ALL) { for (k = 0; k < LJ_CUE_SLOTS; k++) { if (go) lj_runner_go(&e->run[k], t, e->tick_us, e->frame.merged, e->cue_pct); else lj_runner_stop(&e->run[k]); } break; }
        if (c->a >= LJ_RUN_STATIC0) break;                     /* LORD_ENGINE-7: static runners belong to STATIC / STATIC_ACTIVATE (the cue list posts GO / STOP / TRIG) */
        if (c->a == LJ_RUN_CURRENT) {
            lj_runner *r = &e->run[LJ_RUN_CURRENT];
            if (go) { r->trig = LJ_TRIG_AUTO; lj_runner_go(r, t, e->tick_us, e->frame.merged, 100); }
            else { r->trig = LJ_TRIG_MANUAL; r->stopped = 0; }   /* Sequence control Stop: hold the scene, keep editing */
            break;
        }
        if (go) lj_runner_go(&e->run[c->a], t, e->tick_us, e->frame.merged, c->a < LJ_CUE_SLOTS ? e->cue_pct : 100);
        else lj_runner_stop(&e->run[c->a]);
        break;
    }
    case LJ_CMD_TRIG: {
        int lo = 0, n = LJ_CUE_SLOTS;
        if (c->b == LJ_TRIG_AUDIO_EVENT) { if (c->a == LJ_RUN_ALL) lj_engine_audio_trig(e, t); break; }   /* W5 audio */
        if (c->a != LJ_RUN_ALL) { if (c->a >= LJ_RUN_STATIC0) break; lo = (int)c->a; n = 1; }   /* LORD_ENGINE-7 */
        if (c->a == LJ_RUN_ALL || c->a < LJ_CUE_SLOTS) lj_engine_tap(e, t, lo, n, 0);   /* F10: the cue's trigs only (the Sequence window's Trig is no chase tap, ME-93); period = the last two (E9: 1 ms) */
        for (k = lo; k < lo + n; k++) {
            if (e->st.prefs[LJ_PREF_HOTKEY_TRIG_CHANGES_DIR] && c->b <= LJ_DIR_RANDOM && k < LJ_CUE_SLOTS) e->run[k].dir = (uint8_t)c->b;   /* [CUE] HotkeyChangeSeqDir: a trig with a direction sets the slot's */
            uint32_t link = lj_runner_trig(&e->run[k], (int)c->b, (int)c->c, t, e->tick_us, e->frame.merged, k < LJ_CUE_SLOTS ? e->cue_pct : 100, &e->rng, e->cue_edit);
            if (link) e->st.pending_link_cue = link;
        }
        break;
    }
    case LJ_CMD_ENABLE:
        if ((id = lj_engine_slot_id(c->a, LJ_RUN_SLOT0, LJ_CUE_SLOTS)) < 0) break;
        e->run[id].enabled = (c->b != 0); e->run[id].edit = (c->c == 1); e->run[id].out = (c->c == 2);
        break;
    case LJ_CMD_TIME:
        if (c->a == 0) e->cue_pct = (uint16_t)(c->b ? (c->b > LJ_TIME_PCT_MAX ? LJ_TIME_PCT_MAX : c->b) : 100);   /* the cue fader; 0 = CUE [03 s5.6] */
        else if (c->a <= LJ_CUE_SLOTS) e->run[c->a - 1].time_pct = (uint16_t)(c->b > LJ_TIME_PCT_MAX ? LJ_TIME_PCT_MAX : c->b);   /* 0 = CUE: the cue fader drives it */   /* applied at the next trig (LJ_TIME_SCALE_AT_TRIG_ONLY, rig M7) */
        break;
    case LJ_CMD_CHASE:
        if ((id = lj_engine_slot_id(c->a, LJ_RUN_SLOT0, LJ_CUE_SLOTS)) < 0) break;
        e->run[id].chase_on = (c->b != 0); e->run[id].chase_fade = (uint8_t)(c->c != 0 && !e->st.prefs[LJ_PREF_DISABLE_CHASE_FADE]);   /* LJ_CHASE_FADE_IS_PERIOD */
        if (e->run[id].chase_on) {
            if (e->chase_us && (e->st.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] || !e->run[id].chase_us)) e->run[id].chase_us = e->chase_us;   /* the global rate carries across cue loads [03 s5.5] */
            if (e->run[id].chase_us) e->run[id].scene_due_us = t + e->run[id].chase_us;
        }
        break;
    case LJ_CMD_SET_MASTER:
        if (c->a == 0) { e->post.gm = (uint8_t)(c->b > 255 ? 255 : c->b); lj_gm_fade_cancel(&e->gm_fade); }
        else if (c->a <= LJ_SUBMASTERS) e->post.sub[c->a - 1] = (uint8_t)(c->b > 255 ? 255 : c->b);   /* c (mode) is the UI's: lj_show.submaster_mode[] is read through the ctx */
        break;
    case LJ_CMD_GM_FADE: lj_gm_fade_start(&e->gm_fade, c->a != 0, c->b, e->post.gm, t); break;   /* 11 G11 */
    case LJ_CMD_BLACKOUT: e->post.blackout = (uint8_t)(c->b != 0); break;
    case LJ_CMD_FREEZE: lj_engine_freeze(e, c->b != 0, t); break;
    case LJ_CMD_OFFLINE: e->post.offline = (uint8_t)(c->b != 0); break;   /* S9: computed, not sent (lj_engine_is_offline) */
    case LJ_CMD_SOLO: e->post.solo = (uint8_t)(c->b != 0); e->post.solo_fixture = (int)c->a; break;   /* W2: solo of a selection set */
    case LJ_CMD_STATIC:                                        /* W1 form: a = global slot (page 1 = runner order), scope Add, the slot's level */
        if (c->b && c->st) lj_engine_static_on(e, c->a, c->st, c->seq[0], 0xFFFF, LJ_REP_ADD, t);
        else for (k = 0; k < LJ_STATIC_RUNNERS; k++) if (e->static_slot[k] && e->st.static_slot_of[k] == c->a) lj_engine_static_off(e, k);
        break;
    case LJ_CMD_FLASH: lj_engine_flash(e, c, t); break;
    case LJ_CMD_OVERWRITE:
        if (c->a == 0xFFFF) { memset(e->post.ovr_on, 0, sizeof e->post.ovr_on); break; }
        if (c->a >= LJ_CHANS) break;
        if (c->b >= 256) e->post.ovr_on[c->a] = 0; else { e->post.ovr_on[c->a] = 1; e->post.ovr_val[c->a] = (uint8_t)c->b; }
        break;
    case LJ_CMD_HTP_GROUP: if (c->a >= 1 && c->a <= LJ_HTP_GROUPS) e->post.htp_level[c->a] = (uint8_t)(c->b > 255 ? 255 : c->b); break;
    case LJ_CMD_SEQ_INT:
        if (c->a == 0) e->seq_int_mode = (uint8_t)c->b;
        else if (c->a <= LJ_CUE_SLOTS) e->run[c->a - 1].seq_int = (uint8_t)(c->b > 255 ? 255 : c->b);
        break;
    case LJ_CMD_MACRO_CTL: e->macro_amp_pct = (uint16_t)(c->a > LJ_MOVEMACRO_PCT_MAX ? LJ_MOVEMACRO_PCT_MAX : c->a); e->macro_speed_pct = (uint16_t)(c->b > LJ_MOVEMACRO_PCT_MAX ? LJ_MOVEMACRO_PCT_MAX : c->b); break;   /* W3: 1-999 % */
    case LJ_CMD_MACRO_FADE: lj_engine_mm_ctl(e, c, t); break;   /* W3 movement: the Cue macro control buttons [03 s5.11] */
    case LJ_CMD_DISABLE_FADE: e->run[LJ_RUN_CURRENT].disable_fade = (c->b != 0); break;   /* F15 */
    case LJ_CMD_SACN_CFG: e->sacn_cfg = c->sacn; break;
    case LJ_CMD_DMX_IN:                                        /* W5: the UI's merged + translated input frame into the S1 source */
        if (c->a < 1 || c->a > LJ_LINKS) break;
        if (c->dmx_in) memcpy(&e->post.dmx_in[(c->a - 1) * LJ_LINK_CH], c->dmx_in, LJ_LINK_CH);
        else memset(&e->post.dmx_in[(c->a - 1) * LJ_LINK_CH], 0, LJ_LINK_CH);
        break;
    case LJ_CMD_OVERWRITE_ENABLE: e->post.overwrite_enable = (uint8_t)(c->b != 0); break;   /* W5: S1 gate */       /* ME-23: collected by the engine thread after the tick */
    /* ---- W2 kinds ---- */
    case LJ_CMD_BG_ON:
        for (k = 0; k < LJ_BG_SLOTS; k++) if (c->a == LJ_RUN_ALL || c->a == (uint32_t)(LJ_RUN_BG0 + k)) e->run[LJ_RUN_BG0 + k].enabled = (c->b != 0);
        break;
    case LJ_CMD_STATIC_ACTIVATE:
        if (c->c == LJ_STATIC_REPIN) {                         /* the UI replaced or removed the linked sequence: the held slot reads the new record (or nothing) */
            for (k = 0; k < LJ_STATIC_RUNNERS; k++)
                if (e->static_slot[k] && e->st.static_slot_of[k] == c->a && e->static_slot[k]->kind == LJ_STATIC_LINK) e->run[LJ_RUN_STATIC0 + k].seq = c->seq[0];
            break;
        }
        lj_engine_static_on(e, c->a, c->st, c->seq[0], c->b, c->c, t);
        break;
    case LJ_CMD_STATIC_DEACTIVATE:                             /* one slot; a = LJ_RUN_ALL: Release All (c 0) or Release Page c */
        for (k = 0; k < LJ_STATIC_RUNNERS; k++) {
            uint32_t o = e->st.static_slot_of[k];
            if (e->static_slot[k] && (c->a == LJ_RUN_ALL ? (!c->c || o / LJ_STATICS2_SLOTS_MAX == c->c - 1) : o == c->a)) lj_engine_static_off(e, k);
        }
        break;
    case LJ_CMD_SMOKE: lj_engine_smoke_cmd(e, c, t); break;
    case LJ_CMD_TRANSPARENT: if (c->b <= LJ_TRANSPARENT_IGNORE) e->transparent_mode = (uint8_t)c->b; break;
    case LJ_CMD_CHASE_TAP:
        if (c->a == LJ_RUN_ALL) lj_engine_tap(e, t, 0, LJ_CUE_SLOTS, 1);
        else if (c->a < LJ_CUE_SLOTS) lj_engine_tap(e, t, (int)c->a, 1, 1);
        break;
    case LJ_CMD_LOOP_OPTS:
        if ((id = lj_engine_slot_id(c->a, LJ_RUN_SLOT0, LJ_CUE_SLOTS)) >= 0 && c->slot && e->run[id].seq_no) lj_runner_set_loop(&e->run[id], c->slot);
        break;
    case LJ_CMD_HTP_GROUPS: if (c->patch) e->post.patch = c->patch; e->post.htp_masters = (uint8_t)(c->b != 0); break;   /* S5 [11 P8] */
    case LJ_CMD_GLOBAL_PATCH: if (c->patch) e->post.patch = c->patch; break;   /* S1 / S6 */
    case LJ_CMD_DEFAULT_OUT: e->default_out = c->default_out ? c->default_out : (e->show ? e->show->default_out : NULL); break;   /* rank 0 */
    case LJ_CMD_SNAPSHOT_REQ: e->snap_dst = c->snapshot; e->snap_src = (uint8_t)c->a; break;
    case LJ_CMD_PREFS:
        if (c->a >= LJ_PREF_COUNT) break;
        e->st.prefs[c->a] = (uint8_t)(c->b != 0);
        if (c->a == LJ_PREF_DISABLE_CHASE_FADE) for (k = 0; k < LJ_CUE_SLOTS; k++) e->run[k].chase_fade = (uint8_t)(c->b == 0);
        break;
    default: break;
    }
}

/* ---- the scene a runner evaluates (sequence runners by index, statics by slot) ---- */
static const lj_scene *lj_engine_scene_of(const lj_engine *e, int i)
{
    const lj_runner *r = &e->run[i];
    if (i >= LJ_RUN_STATIC0) {
        const lj_static_slot *s = e->static_slot[i - LJ_RUN_STATIC0];
        if (!s) return NULL;
        if (s->kind == LJ_STATIC_LINK) return r->seq && r->scene < r->seq->n ? &r->seq->scenes[r->scene] : NULL;
        return s->scene;
    }
    return r->seq && r->scene < r->seq->n ? &r->seq->scenes[r->scene] : NULL;
}

/* Which runner won fixture f's pan channel in merged[]/layer_of[] (for the movement-macro
 * index rule, 17 s7.1); returns the runner index or -1. */
static int lj_engine_pan_winner(const lj_engine *e, int f, int *is_cue)
{
    const lj_fixture *fx = &e->show->fixture[f];
    const lj_profile *p = e->post.profile_of[f];
    int ch, k;
    *is_cue = 0;
    if (!fx->link || !p || !(p->pt16 || p->pt8)) return -1;
    if ((ch = lj_fx_code(fx, p->pan_ofs)) < 0) return -1;     /* ME-132 */
    switch (e->frame.layer_of[ch]) {
    case LJ_L_CUE:     *is_cue = 1; for (k = LJ_CUE_SLOTS - 1; k >= 0; k--) if (lj_runner_active(&e->run[k]) && e->run[k].st[ch] != LJ_OFF) return k; return -1;
    case LJ_L_FLASH:   return LJ_RUN_FLASH;
    case LJ_L_CURRENT: return LJ_RUN_CURRENT;
    case LJ_L_BGCUE:   for (k = LJ_BG_SLOTS - 1; k >= 0; k--) if (lj_runner_active(&e->run[LJ_RUN_BG0 + k]) && e->run[LJ_RUN_BG0 + k].st[ch] != LJ_OFF) return LJ_RUN_BG0 + k; return -1;
    default:           return -1;                              /* statics carry no macro in W1 */
    }
}

static void lj_engine_fill_view(lj_engine *e, uint64_t now)
{
    lj_engine_status *s = &e->st;
    int k;
    const lj_runner *cur = &e->run[LJ_RUN_CURRENT];
    s->statics_any = s->bg_any = 0;
    for (k = 0; k < LJ_CUE_SLOTS; k++) {
        const lj_runner *r = &e->run[k];
        s->slot_status[k] = (uint8_t)lj_runner_status(r);
        s->slot_scene[k] = (uint16_t)(r->seq ? r->scene + 1 : 0); s->slot_scenes[k] = (uint16_t)(r->seq ? r->seq->n : 0);
        s->slot_time_pct[k] = r->time_pct; s->slot_loops[k] = r->loop_count; s->slot_chase[k] = (uint8_t)r->chase_on;
    }
    s->chase_ms = e->chase_us / 1000u;
    s->cur_scene = (uint16_t)(cur->seq ? cur->scene + 1 : 0); s->cur_scenes = (uint16_t)(cur->seq ? cur->seq->n : 0);
    if (cur->seq) {
        uint64_t el = now > cur->fade_t0_us ? now - cur->fade_t0_us : 0;
        uint64_t span = cur->scene_due_us > cur->fade_t0_us ? cur->scene_due_us - cur->fade_t0_us : 0;   /* LORD_ENGINE-9: an unfreeze moves fade_t0 past a held scene's due time */
        s->cur_scene_ms = (uint32_t)(span / 1000u);
        s->cur_scene_elapsed_ms = (uint32_t)((el > span ? span : el) / 1000u);
        s->cur_fade_ms = (uint32_t)(cur->fade_dur_us / 1000u);
        s->cur_fade_elapsed_ms = (uint32_t)((el > cur->fade_dur_us ? cur->fade_dur_us : el) / 1000u);
    } else s->cur_scene_ms = s->cur_scene_elapsed_ms = s->cur_fade_ms = s->cur_fade_elapsed_ms = 0;
    s->cur_running = (uint8_t)(cur->seq && cur->trig == LJ_TRIG_AUTO && !cur->stopped); s->cur_dir = cur->dir; s->cur_blind = (uint8_t)cur->blind;
    s->flash_status = (uint8_t)lj_runner_status(&e->run[LJ_RUN_FLASH]);
    for (k = 0; k < LJ_BG_SLOTS; k++) {
        const lj_runner *r = &e->run[LJ_RUN_BG0 + k];
        s->bg_status[k] = (uint8_t)lj_runner_status(r);
        s->bg_scene[k] = (uint16_t)(r->seq ? r->scene + 1 : 0); s->bg_scenes[k] = (uint16_t)(r->seq ? r->seq->n : 0);
        if (lj_runner_active(r)) s->bg_any = 1;
    }
    for (k = 0; k < LJ_STATIC_RUNNERS; k++) { s->static_active[k] = (uint8_t)lj_runner_active(&e->run[LJ_RUN_STATIC0 + k]); if (s->static_active[k]) s->statics_any = 1; }
    s->gm = e->post.gm; memcpy(s->sub, e->post.sub, sizeof s->sub);
    s->gm_fade = e->gm_fade.mode;
    s->blackout = e->post.blackout; s->freeze = e->post.freeze; s->offline = e->post.offline; s->solo = e->post.solo;
    s->cue_edit = e->cue_edit;
    s->tick_no = e->frame.tick_no;
    s->bg_number = e->bg ? e->bg->number : 0;                  /* W2 view fields */
    s->cue_number = e->cue ? e->cue->number : 0;               /* the pinned record: a Save New Cue numbers it in place (W2 acceptance) */
    for (k = 0; k < LJ_BG_SLOTS; k++) s->bg_enabled[k] = (uint8_t)e->run[LJ_RUN_BG0 + k].enabled;
    s->chase_tap_us = e->chase_us;
    s->flash_latched = (uint8_t)(e->flash_latch && lj_runner_active(&e->run[LJ_RUN_FLASH]));
    s->flash_seq = lj_runner_active(&e->run[LJ_RUN_FLASH]) && e->run[LJ_RUN_FLASH].seq ? e->run[LJ_RUN_FLASH].seq->number : 0;
    s->transparent_mode = e->transparent_mode;
    s->smoke_active = e->smoke.firing; s->haze_level = e->smoke.haze; s->fan_level = e->smoke.fan; s->smoke_timer = e->smoke.timer;
    s->smoke_cycle_elapsed_ms = e->smoke.elapsed_ms;
}

/* ---- W3 movement: the Cue macro control's Fade out / in, Macros Off / On, Freeze / Run [03 s5.11] (MACRO_FADE) ---- */
static void lj_engine_mm_step(lj_engine *e, uint64_t now)
{
    uint64_t el;
    if (!e->mmc.fade) return;
    el = now > e->mmc.t0_us ? (now - e->mmc.t0_us) / 1000u : 0;
    if (el >= e->mmc.ms) { e->mmc.gain = e->mmc.to; e->mmc.fade = 0; return; }
    e->mmc.gain = (uint16_t)((int64_t)e->mmc.from + ((int64_t)e->mmc.to - e->mmc.from) * (int64_t)el / e->mmc.ms);   /* LJ_MOVEMACRO_FADE_LINEAR */
}
static void lj_engine_mm_ctl(lj_engine *e, const lj_cmd *c, uint64_t now)
{
    switch (c->a) {
    case LJ_MM_FADE_OUT: case LJ_MM_FADE_IN:
        lj_engine_mm_step(e, now);
        e->mmc.from = e->mmc.gain; e->mmc.to = c->a == LJ_MM_FADE_IN ? LJ_MM_GAIN : 0;
        e->mmc.t0_us = now; e->mmc.ms = c->b;
        e->mmc.fade = (uint8_t)(c->b && e->mmc.from != e->mmc.to ? (c->a == LJ_MM_FADE_OUT ? 1 : 2) : 0);
        if (!e->mmc.fade) e->mmc.gain = e->mmc.to;
        break;
    case LJ_MM_OFF: case LJ_MM_ON: e->mmc.fade = 0; e->mmc.gain = e->mmc.to = c->a == LJ_MM_ON ? LJ_MM_GAIN : 0; break;   /* instant */
    case LJ_MM_FREEZE: e->mmc.frozen = 1; break;           /* speed 0 [03 s5.11] */
    case LJ_MM_RUN: e->mmc.frozen = 0; break;
    default: break;
    }
}
static int lj_engine_mm_amp(const lj_engine *e) { return (int)((uint32_t)e->macro_amp_pct * e->mmc.gain / LJ_MM_GAIN); }   /* the scale in force: floor */
static int lj_engine_mm_speed(const lj_engine *e) { return e->mmc.frozen ? 0 : e->macro_speed_pct; }

void lj_engine_tick(lj_engine *e, uint64_t now_us)
{
    lj_frame *f = &e->frame;
    uint64_t t_start = lj_clock_us(), t_end;
    int i, k;
    e->now_us = now_us;
    for (i = 0; i < e->ncmd; i++) lj_engine_apply(e, &e->cmds[i], now_us);   /* step 1: drain */
    e->ncmd = 0;
    lj_gm_fade_step(&e->gm_fade, now_us, &e->post.gm);         /* G11: the GM fade runner (a master move, not an S0 input) */
    if (!e->frozen) {                                          /* F20: freeze pauses fade and macro calculation */
        lj_engine_mm_step(e, now_us);                          /* W3: the cue macro control fade, frozen with the engine (F20) */
        for (k = 0; k < LJ_FIXTURES; k++) lj_movemacro_advance(&e->mm[k], e->mm_src[k], e->tick_us, e->mm_cue[k] ? lj_engine_mm_speed(e) : 100);   /* step 2: once per tick (ME-35) */
    }
    for (i = 0; i < LJ_RUN_COUNT; i++) {                       /* step 2: trig scheduling, even when frozen (F20, E18) */
        uint32_t link;
        if (i >= LJ_RUN_STATIC0) break;                        /* statics do not trig */
        link = lj_runner_schedule(&e->run[i], now_us, e->tick_us, f->merged, i < LJ_CUE_SLOTS ? e->cue_pct : 100, &e->rng, e->cue_edit);
        if (link) e->st.pending_link_cue = link;               /* 2.3 rule 3 */
    }
    if (!e->frozen) {                                          /* step 3: evaluate; step 4: merge; frozen: merged[] holds (S9) */
        for (i = 0; i < LJ_RUN_COUNT; i++) {
            lj_runner *r = &e->run[i];
            if (!lj_runner_active(r)) continue;
            if (r->stopped && !r->ended && i < LJ_RUN_STATIC0) continue;   /* F14 + LJ_STOP_HOLDS_FADE: a user Stop holds val[]/st[] (rig M8); an end-action stop keeps evaluating */
            if (i == LJ_RUN_CURRENT && r->blind) continue;     /* F21: the blind buffer */
            lj_runner_eval(r, lj_engine_scene_of(e, i), now_us, &e->pairs, &e->post, i < LJ_RUN_STATIC0 ? e->mm : NULL, i < LJ_CUE_SLOTS ? lj_engine_mm_amp(e) : 100);
        }
        lj_merge(e->default_out ? e->default_out : lj_engine_zero, e->run, &e->post, e->seq_int_mode, f->merged, f->layer_of);
        lj_gm_rank6_tick(&e->gm, &e->post, f->merged, f->layer_of, e->tick_us);   /* rank 6 over ranks 0-5, then the index advance (lj_genmacro.c) */
        lj_engine_smoke(e, now_us, f->merged, f->layer_of);    /* rank 7 above rank 6 (LJ_SMOKE_RANK_ABOVE_GENMACRO, rig M15) */
        if (e->show) for (k = 0; k < LJ_FIXTURES; k++) {        /* remember the macro each fixture's output carried, for the next tick's index advance [17 s7.1] */
            int is_cue, w = lj_engine_pan_winner(e, k, &is_cue);
            const lj_scene *sc = w >= 0 ? lj_engine_scene_of(e, w) : NULL;
            e->mm_src[k] = sc && lj_movemacro_active(&sc->macro[k]) ? &sc->macro[k] : NULL;
            e->mm_cue[k] = (uint8_t)is_cue;
        }
    }
    lj_post_run(&e->post, f->merged, f->out);                  /* step 5: S1-S9 */
    f->tick_no++;                                              /* step 6: publish */
    if (e->snap_dst) { *e->snap_dst = *f; e->st.snapshot_tick = f->tick_no; e->snap_dst = NULL; }   /* SNAPSHOT_REQ: a copy, no I/O (LJ_SNAPSHOT_WHOLE_FRAME; snap_src is the UI's) */
    lj_engine_fill_view(e, now_us);
    lj_gm_rank6_view(&e->gm, &e->st);                          /* W3 genmacro view: engine / slot on, the cue's and the editor's numbers */
    e->st.mm_amp_pct = (uint16_t)lj_engine_mm_amp(e); e->st.mm_speed_pct = (uint16_t)lj_engine_mm_speed(e);   /* W3 movement view */
    e->st.mm_fade = e->mmc.fade; e->st.mm_frozen = e->mmc.frozen;
    for (k = 0; k < LJ_FIXTURES; k++) { e->st.mm_idx_pan[k] = (uint16_t)(e->mm[k].idx_pan >> 16); e->st.mm_idx_tilt[k] = (uint16_t)(e->mm[k].idx_tilt >> 16); }
    t_end = lj_clock_us();
    e->st.tick_us_last = (uint32_t)(t_end - t_start);
    if (e->st.tick_us_last > e->st.tick_us_max) e->st.tick_us_max = e->st.tick_us_last;
}

const lj_frame *lj_engine_frame(const lj_engine *e)
{
    return &e->frame;
}

void lj_engine_view(lj_engine *e, lj_engine_status *out)
{
    *out = e->st;
    e->st.pending_link_cue = e->st.pending_bg = e->st.pending_genmacro = 0;   /* handed out once (design 2.3 rule 3) */
}

const lj_runner *lj_engine_runner(const lj_engine *e, int id)
{
    return (id >= 0 && id < LJ_RUN_COUNT) ? &e->run[id] : NULL;
}

const lj_post_ctx *lj_engine_post_ctx(const lj_engine *e) { return &e->post; }

uint32_t lj_engine_tick_us(const lj_engine *e) { return e->tick_us; }

void lj_engine_set_tick_us(lj_engine *e, uint32_t tick_us)
{
    if (tick_us < 1000000u / LJ_TICK_HZ_MAX) tick_us = 1000000u / LJ_TICK_HZ_MAX;   /* ME-24 */
    if (tick_us > 1000000u / LJ_TICK_HZ_MIN) tick_us = 1000000u / LJ_TICK_HZ_MIN;
    e->tick_us = tick_us;
}

int lj_engine_is_offline(const lj_engine *e) { return e->post.offline; }

const void *lj_engine_take_sacn_cfg(lj_engine *e)
{
    const void *c = e->sacn_cfg;
    e->sacn_cfg = NULL;
    return c;
}

void lj_engine_note_overrun(lj_engine *e) { e->st.overruns++; }

void lj_engine_set_sacn_stats(lj_engine *e, const uint32_t pps[LJ_LINKS], int last_err)
{
    memcpy(e->st.sacn_pps, pps, sizeof e->st.sacn_pps);
    e->st.sacn_last_err = last_err;
}
