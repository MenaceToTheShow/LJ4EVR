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
    const lj_cue          *cue; uint8_t cue_edit; uint16_t cue_pct; uint8_t macro_amp_pct, macro_speed_pct;
    const lj_bgcue        *bg;
    const lj_genmacro     *gm; lj_gm_ch gm_ch[LJ_CHANS];
    const lj_static_slot  *static_slot[LJ_STATIC_RUNNERS];
    lj_mm_state            mm[LJ_FIXTURES];             /* movement-macro indexes, advanced once per tick (ME-35) */
    const lj_movemacro    *mm_src[LJ_FIXTURES]; uint8_t mm_cue[LJ_FIXTURES];   /* the macro the previous tick's merged[] carried per fixture, and whether a cue slot won it */
    lj_gm_fade             gm_fade;
    uint8_t                frozen; uint64_t frozen_since_us;
    uint8_t                seq_int_mode;
    uint32_t               chase_us; uint64_t tap_us[2];  /* global chase period from the last two taps [03 s5.5] (W2: per-slot rates, Save Chase Times) */
    uint32_t               rng;
    uint64_t               now_us;
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
        e->rng = LJ_RNG_SEED;
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
        int base, ofs;
        if (!fx->link) { e->post.profile_of[f] = NULL; continue; }
        if (fx->kind == LJ_FX_BUILTIN) { p = lj_fixlib_by_id(fx->profile_id); if (p) e->post.profile_of[f] = p; }   /* built-ins resolve here; a GS2 (or test) profile stays from lj_engine_bind_profile */
        p = e->post.profile_of[f];
        if (!p) continue;
        base = lj_addr_code(fx->link, fx->address);
        for (ofs = 0; ofs < p->footprint && ofs < LJ_PROFILE_CH; ofs++) {
            if (base + ofs >= LJ_CHANS) break;
            if (p->level_disabled[ofs]) continue;              /* LORD_FIXTURES-11: a disabled offset is unpatched, output = default_out */
            e->post.fixture_of[base + ofs] = (int16_t)f;
        }
    }
    lj_runner_pairs_build(&e->post, &e->pairs);
}

void lj_engine_bind(lj_engine *e, const lj_show *show)
{
    e->show = show;
    e->post.show = show;
    lj_engine_rebuild(e);
    if (e->frame.tick_no == 0) memcpy(e->frame.merged, show ? show->default_out : lj_engine_zero, LJ_CHANS);   /* before the first tick the output at rest is DefaultOut: a fade trigged at tick 0 starts from it (F3) */
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

static void lj_engine_load_bg(lj_engine *e, const lj_bgcue *bg, const lj_sequence *const *seqs, uint64_t t)
{
    int k;
    e->bg = bg;
    for (k = 0; k < LJ_BG_SLOTS; k++) {
        lj_runner *r = &e->run[LJ_RUN_BG0 + k];
        if (!bg || !bg->slot[k].seq) { if (!bg || !bg->transparent) lj_runner_clear(r); continue; }   /* transparent: empty slots do not overwrite [03 s11.3] */
        lj_runner_set_seq(r, seqs ? seqs[k] : NULL, bg->slot[k].seq);
        r->enabled = bg->slot[k].enabled ? 1 : 0;
        if (bg->slot[k].mode == LJ_BG_NOLOOP) { r->loops_left = 1; r->end_action = LJ_END_STOP; }   /* -L: stops after the last scene [03 s7] */
        if (bg->slot[k].mode == LJ_BG_AUDIO) r->trig = LJ_TRIG_AUDIO;                                 /* W2: audio analyser trigs */
        lj_runner_start(r, t, e->tick_us, e->frame.merged, 100, &e->rng);
    }
}

static void lj_engine_load_cue(lj_engine *e, const lj_cmd *c, uint64_t t)
{
    const lj_cue *cue = c->cue;
    int k;
    e->cue = cue; e->cue_edit = (uint8_t)(c->b != 0);
    if (!cue) { for (k = 0; k < LJ_CUE_SLOTS; k++) lj_runner_clear(&e->run[k]); e->st.cue_number = 0; return; }   /* New cue: clears the cue control */
    e->cue_pct = cue->cue_time_pct ? cue->cue_time_pct : 100;
    if (cue->save_macro_values) { e->macro_amp_pct = cue->macro_amp_pct; e->macro_speed_pct = cue->macro_speed_pct; }   /* W2: Restore Macro Control Values preference */
    for (k = 0; k < LJ_CUE_SLOTS; k++) {
        lj_runner *r = &e->run[k];
        const lj_cue_slot *s = &cue->slot[k];
        if (!s->seq) { if (!cue->transparent) lj_runner_clear(r); continue; }   /* transparent cue: only slots with a sequence overwrite [03 s5.8] */
        lj_runner_set_slot(r, s, c->seq[k]);                   /* seq NULL = load error [03 s5.1] */
        if (r->seq) lj_runner_start(r, t, e->tick_us, e->frame.merged, e->cue_pct, &e->rng);
    }
    e->st.cue_number = cue->number;
    if (cue->bg_cue) { if (c->bg) lj_engine_load_bg(e, c->bg, c->bgseq, t); else e->st.pending_bg = cue->bg_cue; }   /* latched bg cue: pinned here or requested (2.3 rule 3); W2: Disable BG Cue Latch preference, BG latch from the cue list / 2532 */
    if (cue->gen_macro) {
        if (c->gm) { e->gm = c->gm; if (c->gm->restart_on_load) lj_genmacro_restart(e->gm_ch); }   /* 14 sA.3 (7): restart on cue load if flagged */
        else e->st.pending_genmacro = cue->gen_macro;
    }
}

static void lj_engine_freeze(lj_engine *e, int on, uint64_t now)
{
    int i;
    if (on == e->frozen) return;
    e->frozen = e->post.freeze = (uint8_t)on;
    if (on) { e->frozen_since_us = now; return; }
    for (i = 0; i < LJ_RUN_COUNT; i++) {                       /* F20 / ME-13 unfreeze (LJ_FREEZE_PAUSED_CLOCK, rig M10): a fade from before the freeze keeps its remaining time; one trigged while frozen starts at the unfreeze instant from the frozen merged[] (from[] was captured from it at the trig). The literal max(t0 + frozen, unfreeze) would put a frozen-time trig in the future, which 3.5 forbids. */
        lj_runner *r = &e->run[i];
        if (!r->seq) continue;
        r->fade_t0_us = r->fade_t0_us < e->frozen_since_us ? r->fade_t0_us + (now - e->frozen_since_us) : now;
    }
    if (e->gm_fade.mode) e->gm_fade.t0_us += now - e->frozen_since_us;
}

static void lj_engine_apply(lj_engine *e, const lj_cmd *c, uint64_t now)
{
    uint64_t t = c->at_us && c->at_us <= now ? c->at_us : now;   /* UI timestamp (chase taps at 1 ms, E9); never later than this tick */
    int id, k;
    switch (c->kind) {
    case LJ_CMD_LOAD_CUE: lj_engine_load_cue(e, c, t); break;
    case LJ_CMD_LOAD_BG: lj_engine_load_bg(e, c->bg, c->bgseq, t); break;
    case LJ_CMD_LOAD_GENMACRO: e->gm = c->gm; if (c->b || (c->gm && c->gm->restart_on_load)) lj_genmacro_restart(e->gm_ch); break;
    case LJ_CMD_CURRENT_SEQ: {
        lj_runner *r = &e->run[LJ_RUN_CURRENT];
        int was_blind = r->blind;
        if (!c->seq[0]) { lj_runner_clear(r); break; }
        if (r->seq != c->seq[0] || r->scene != c->a || !r->seq_no) {
            uint8_t blind = (uint8_t)r->blind;
            lj_runner_set_seq(r, c->seq[0], c->seq[0]->number);
            r->trig = LJ_TRIG_MANUAL;                          /* the editor shows a scene; playback (Go) makes it auto */
            lj_runner_enter(r, (uint16_t)c->a, t, e->tick_us, e->frame.merged, 100);
            r->blind = blind;
        }
        r->blind = (c->b != 0);
        if (r->blind && !was_blind && !LJ_KEEP_CONTENTS_BLIND) memset(r->st, LJ_OFF, LJ_CHANS);   /* F21, LORD_ENGINE-13: eval is skipped while blind, so val[]/st[] are the buffer */
        break;
    }
    case LJ_CMD_GO: case LJ_CMD_STOP: {
        int go = c->kind == LJ_CMD_GO;
        if (c->a == LJ_RUN_ALL) { for (k = 0; k < LJ_CUE_SLOTS; k++) { if (go) lj_runner_go(&e->run[k], t, e->tick_us, e->frame.merged, e->cue_pct); else lj_runner_stop(&e->run[k]); } break; }
        if (c->a >= LJ_RUN_COUNT) break;
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
        if (c->a != LJ_RUN_ALL) { if (c->a >= LJ_RUN_COUNT) break; lo = (int)c->a; n = 1; }
        e->tap_us[0] = e->tap_us[1]; e->tap_us[1] = t;         /* F10: chase period = the last two manual trigs (E9: 1 ms) */
        if (e->tap_us[0] && t > e->tap_us[0] && t - e->tap_us[0] <= (uint64_t)LJ_TIME_MAX_MS * 1000u) {
            e->chase_us = (uint32_t)(t - e->tap_us[0]);
            for (k = 0; k < LJ_CUE_SLOTS; k++) if (e->run[k].chase_on) e->run[k].chase_us = e->chase_us;   /* W2: Use global chase rate preference, per-slot rates */
        }
        for (k = lo; k < lo + n; k++) {
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
        if (c->a == 0) e->cue_pct = (uint16_t)(c->b ? c->b : 100);   /* the cue fader; 0 = CUE */
        else if (c->a <= LJ_CUE_SLOTS) e->run[c->a - 1].time_pct = (uint16_t)c->b;   /* applied at the next trig (LJ_TIME_SCALE_AT_TRIG_ONLY, rig M7) */
        break;
    case LJ_CMD_CHASE:
        if ((id = lj_engine_slot_id(c->a, LJ_RUN_SLOT0, LJ_CUE_SLOTS)) < 0) break;
        e->run[id].chase_on = (c->b != 0); e->run[id].chase_fade = (c->c != 0);
        if (e->run[id].chase_on) { if (!e->run[id].chase_us) e->run[id].chase_us = e->chase_us; if (e->run[id].chase_us) e->run[id].scene_due_us = t + e->run[id].chase_us; }
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
    case LJ_CMD_STATIC: {
        lj_runner *r;
        id = (int)c->a;                                        /* the static runner index 0..LJ_STATIC_RUNNERS-1 (not LJ_RUN_STATIC0 + i: 19 + i would be ambiguous) */
        if (id < 0 || id >= LJ_STATIC_RUNNERS) break;
        r = &e->run[LJ_RUN_STATIC0 + id];
        if (!c->b || !c->st) { lj_runner_clear(r); e->static_slot[id] = NULL; break; }   /* release: snap back to the layer below (LJ_STATIC_RELEASE_SNAPS, rig M9) */
        e->static_slot[id] = c->st;
        lj_runner_set_seq(r, c->st->kind == LJ_STATIC_LINK ? c->seq[0] : NULL, 1);
        r->trig = LJ_TRIG_MANUAL; r->activated_us = t; r->running = 1;   /* LTP order: every activation puts the entry on top [03 s8]; running = pinned static */
        r->level_pct = c->st->level_pct; r->scene = c->st->link_scene;
        memcpy(r->from, e->frame.merged, LJ_CHANS);
        r->fade_t0_us = t;
        r->fade_dur_us = LJ_STATICS_USE_FADETIME && c->st->fade_ms > LJ_SNAP_MS ? (uint64_t)c->st->fade_ms * 1000u : 0;   /* F17: snap unless Statics Use Fadetimes */
        break;                                                 /* W2: replace modes (14 sB.7), the 65th activation */
    }
    case LJ_CMD_FLASH: {
        lj_runner *r = &e->run[LJ_RUN_FLASH];
        if (!c->b || !c->seq[0]) { lj_runner_clear(r); break; }
        lj_runner_set_seq(r, c->seq[0], c->seq[0]->number);    /* C2: forward, programmed times, above the cue */
        r->flash = 1;
        lj_runner_start(r, t, e->tick_us, e->frame.merged, 100, &e->rng);
        break;
    }
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
    case LJ_CMD_MACRO_CTL: e->macro_amp_pct = (uint8_t)(c->a > 255 ? 255 : c->a); e->macro_speed_pct = (uint8_t)(c->b > 255 ? 255 : c->b); break;
    case LJ_CMD_DISABLE_FADE: e->run[LJ_RUN_CURRENT].disable_fade = (c->b != 0); break;   /* F15 */
    case LJ_CMD_SACN_CFG: e->sacn_cfg = c->sacn; break;       /* ME-23: collected by the engine thread after the tick */
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
    ch = lj_addr_code(fx->link, fx->address) + p->pan_ofs;
    if (ch >= LJ_CHANS) return -1;
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
        s->cur_scene_ms = (uint32_t)((cur->scene_due_us - cur->fade_t0_us) / 1000u);
        s->cur_scene_elapsed_ms = (uint32_t)((el > cur->scene_due_us - cur->fade_t0_us ? cur->scene_due_us - cur->fade_t0_us : el) / 1000u);
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
}

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
        for (k = 0; k < LJ_FIXTURES; k++) lj_movemacro_advance(&e->mm[k], e->mm_src[k], e->tick_us, e->mm_cue[k] ? e->macro_speed_pct : 100);   /* step 2: once per tick (ME-35) */
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
            if (r->stopped && i < LJ_RUN_STATIC0) continue;    /* F14 + LJ_STOP_HOLDS_FADE: val[]/st[] hold (rig M8) */
            if (i == LJ_RUN_CURRENT && r->blind) continue;     /* F21: the blind buffer */
            lj_runner_eval(r, lj_engine_scene_of(e, i), now_us, &e->pairs, &e->post, i < LJ_RUN_STATIC0 ? e->mm : NULL, i < LJ_CUE_SLOTS ? e->macro_amp_pct : 100);
        }
        lj_merge(e->show ? e->show->default_out : lj_engine_zero, e->run, &e->post, e->seq_int_mode, f->merged, f->layer_of);
        if (e->gm && e->show) lj_genmacro_apply(e->gm, e->gm_ch, &e->post, f->merged, f->layer_of);   /* rank 6 */
        if (e->gm) lj_genmacro_advance(e->gm, e->gm_ch, e->tick_us);   /* index for the next tick: a (re)started macro shows index 0 first [14 sA.3 (7)] */
        if (e->show) for (k = 0; k < LJ_FIXTURES; k++) {        /* remember the macro each fixture's output carried, for the next tick's index advance [17 s7.1] */
            int is_cue, w = lj_engine_pan_winner(e, k, &is_cue);
            const lj_scene *sc = w >= 0 ? lj_engine_scene_of(e, w) : NULL;
            e->mm_src[k] = sc && lj_movemacro_active(&sc->macro[k]) ? &sc->macro[k] : NULL;
            e->mm_cue[k] = (uint8_t)is_cue;
        }
    }
    lj_post_run(&e->post, f->merged, f->out);                  /* step 5: S1-S9 */
    f->tick_no++;                                              /* step 6: publish */
    lj_engine_fill_view(e, now_us);
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

void lj_engine_set_sacn_stats(lj_engine *e, const uint32_t pps[LJ_LINKS], int last_err)
{
    memcpy(e->st.sacn_pps, pps, sizeof e->st.sacn_pps);
    e->st.sacn_last_err = last_err;
}
