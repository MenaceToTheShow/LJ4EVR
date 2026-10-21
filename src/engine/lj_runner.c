/* lj_runner.c - scheduling (design 3.2 step 2), scene evaluation (step 3, 3.3).
 * Rule ids in comments are note 11 (E/F/G/P/C) and note 03 s5.5. */
#include <string.h>
#include "engine/lj_runner.h"
#include "engine/lj_post.h"
#include "core/lj_log.h"

void lj_runner_pairs_build(const lj_post_ctx *c, lj_pairs *p)
{
    int ch;
    memset(p->is_lsb, 0, sizeof p->is_lsb);
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int l = lj_post_pair_lsb(c, ch);
        p->lsb_of[ch] = (int16_t)l;
        if (l >= 0) p->is_lsb[l] = 1;
    }
}

void lj_runner_clear(lj_runner *r) { memset(r, 0, sizeof *r); }

static void lj_runner_limits(lj_runner *r)
{
    int n = r->seq ? r->seq->n : 0;
    int first = r->first ? r->first - 1 : 0, last = r->last ? r->last - 1 : n - 1;   /* 1-based in the slot, 0 = none */
    if (n < 1) { r->first = r->last = 0; return; }
    if (last > n - 1) last = n - 1;
    if (first > last) first = last;
    r->first = (uint16_t)(first + 1); r->last = (uint16_t)(last + 1);                /* kept 1-based, always resolved */
}

void lj_runner_set_slot(lj_runner *r, const lj_cue_slot *s, const lj_sequence *seq)
{
    lj_runner_clear(r);
    r->seq_no = s->seq; r->seq = seq;
    r->enabled = s->enabled ? 1 : 0;
    r->trig = s->trig; r->dir = s->dir; r->end_action = s->end_action;
    r->scenes_per_trig = s->scenes_per_trig ? s->scenes_per_trig : 1;
    r->audio_per_trig = s->audio_trigs_per_trig ? s->audio_trigs_per_trig : 1;
    r->first = s->first; r->last = s->last; r->loops_left = s->loops;             /* loops_left = the finite loop count, loop_count = done */
    r->goto_scene = s->goto_scene; r->link_cue = s->link_cue;
    r->time_pct = s->time_pct; r->chase_us = s->chase_ms * 1000u;
    r->seq_int = 255; r->level_pct = 100;
    lj_runner_limits(r);
}

void lj_runner_set_seq(lj_runner *r, const lj_sequence *seq, uint32_t seq_no)
{
    lj_runner_clear(r);
    r->seq_no = seq_no ? seq_no : 1; r->seq = seq;
    r->enabled = 1; r->trig = LJ_TRIG_AUTO; r->dir = LJ_DIR_FWD; r->end_action = LJ_END_STOP;
    r->scenes_per_trig = 1; r->audio_per_trig = 1;
    r->seq_int = 255; r->level_pct = 100;
    lj_runner_limits(r);
}

int lj_runner_active(const lj_runner *r)
{
    return r->seq_no != 0 && (r->seq != NULL || r->running) && r->enabled && !r->edit && !r->out;   /* a static slot pins a scene, not a sequence: its running bit marks it */
}

static uint32_t lj_runner_rng(uint32_t *s)
{
    uint32_t x = *s ? *s : LJ_RNG_SEED;                        /* xorshift32 */
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}

static uint32_t lj_runner_scaled_us(uint32_t ms, int pct)
{
    uint32_t v = (uint32_t)((uint64_t)ms * (uint32_t)pct / 100u);   /* min(60000, ms * pct / 100) [11 F8, F9] */
    if (v > LJ_TIME_MAX_MS) v = LJ_TIME_MAX_MS;
    return v * 1000u;
}

void lj_runner_enter(lj_runner *r, uint16_t scene, uint64_t t, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct)
{
    const lj_scene *sc;
    uint32_t st_us, ft_us;
    int pct;
    if (!r->seq || r->seq->n == 0) return;
    if (scene >= r->seq->n) scene = (uint16_t)(r->seq->n - 1);
    r->scene = scene;
    sc = &r->seq->scenes[scene];
    memcpy(r->from, merged, LJ_CHANS);                         /* F3, F6: a fade starts from the live output, an interrupted one from where it got to */
    pct = r->time_pct ? r->time_pct : (cue_pct ? cue_pct : 100);   /* F9: slot fader, else the cue fader; applied at trig (LJ_TIME_SCALE_AT_TRIG_ONLY) */
    st_us = lj_runner_scaled_us(sc->scene_ms, pct);
    ft_us = lj_runner_scaled_us(sc->fade_ms, pct);
    if (r->chase_on && r->chase_us) {                          /* F10: chase period = scene time, fade := period unless disabled, not scaled */
        st_us = r->chase_us;
        ft_us = r->chase_fade ? r->chase_us : 0;
    }
    if (st_us == 0) st_us = tick_us;
    r->fade_t0_us = t;                                         /* F16: fade time read at trig */
#if LJ_SNAP_TEST_AFTER_SCALE
    r->fade_dur_us = (ft_us <= (uint32_t)LJ_SNAP_MS * 1000u || (LJ_FADE_LE_TICK_IS_SNAP && ft_us <= tick_us)) ? 0 : ft_us;   /* F4, F28 (LORD_ENGINE-11, rig M3/M7) */
#else
    r->fade_dur_us = (sc->fade_ms <= LJ_SNAP_MS || (LJ_FADE_LE_TICK_IS_SNAP && ft_us <= tick_us)) ? 0 : ft_us;
#endif
    r->scene_due_us = t + st_us;                               /* E10: absolute timeline */
}

/* Advance one scene in `dir` with the loop rules of 03 s5.5; returns 1 when an end action
 * ended the run (the runner is stopped / out / linked), 0 otherwise. */
static int lj_runner_advance(lj_runner *r, int dir, uint32_t *rng, int cue_edit, uint32_t *link_out)
{
    int first = r->first - 1, last = r->last - 1, n = last - first + 1, s = r->scene, at_end = 0;
    if (n <= 1) return 1;                                      /* identical first and last scene: nothing to advance, status Stop [03 s5.1] */
    switch (dir) {
    case LJ_DIR_REV:
        if (s > first) s--; else at_end = 1;
        break;
    case LJ_DIR_BOUNCE:                                        /* 1->2->3->4->3->2->1->2 */
        if (!r->bounce_back) { if (s < last) s++; else { r->bounce_back = 1; s--; } }
        else { if (s > first) s--; else at_end = 1; }
        break;
    case LJ_DIR_RANDOM:
        s = first + (int)(lj_runner_rng(rng) % (uint32_t)n);  /* loop count not available in random mode */
        break;
    default:
        if (s < last) s++; else at_end = 1;
        break;
    }
    if (at_end) {
        r->loop_count++;
        if (r->loops_left && r->loop_count >= r->loops_left) {   /* finite loops done: end action */
            switch (r->end_action) {
            case LJ_END_OUT:  r->out = 1; return 1;              /* Sequence out: the slot disables itself */
            case LJ_END_GOTO: r->scene = (uint16_t)(r->goto_scene ? r->goto_scene - 1 : 0); r->stopped = 1; return 2;   /* Goto scene and stop (the caller enters it) */
            case LJ_END_LINK: r->stopped = 1; if (!cue_edit) { r->link = 1; *link_out = r->link_cue; } return 1;   /* Link to cue: Stop + pending_link_cue (2.3 rule 3); Edit Cue: links do not fire */
            default:          r->stopped = 1; return 1;          /* Stop on scene: hold the last scene */
            }
        }
        if (dir == LJ_DIR_REV) s = last;
        else if (dir == LJ_DIR_BOUNCE) { r->bounce_back = 0; s = first + 1; }
        else s = first;
    }
    r->scene = (uint16_t)s;
    return 0;
}

void lj_runner_start(lj_runner *r, uint64_t t, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng)
{
    uint16_t s;
    if (!r->seq || !r->seq->n) return;
    lj_runner_limits(r);
    r->loop_count = 0; r->bounce_back = 0; r->spt_left = 0; r->stopped = 0; r->link = 0; r->out = 0;
    switch (r->dir) {
    case LJ_DIR_REV:    s = (uint16_t)(r->last - 1); break;
    case LJ_DIR_RANDOM: s = (uint16_t)(r->first - 1 + (int)(lj_runner_rng(rng) % (uint32_t)(r->last - r->first + 1))); break;
    default:            s = (uint16_t)(r->first - 1); break;
    }
    lj_runner_enter(r, s, t, tick_us, merged, cue_pct);        /* W2: Trig on Load (lj_cue_slot.trig_on_load, scenes-per-trig > 1) [03 s5.5] */
}

uint32_t lj_runner_trig(lj_runner *r, int dir, int scenes, uint64_t at, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng, int cue_edit)
{
    uint32_t link = 0;
    int rc;
    if (!lj_runner_active(r) || r->stopped || r->seq->n <= 1) return 0;   /* F14: stopped slots ignore every trig source */
    if (r->spt_left) return 0;                                 /* F11: no re-trig until the extra scenes are done */
    if (dir == 0xFF) dir = r->dir;
    rc = lj_runner_advance(r, dir, rng, cue_edit, &link);
    if (rc == 1) return link;
    lj_runner_enter(r, r->scene, at, tick_us, merged, cue_pct);
    if (!scenes) scenes = r->scenes_per_trig;
    r->spt_left = (uint8_t)(scenes > 1 ? scenes - 1 : 0);
    return link;
}

uint32_t lj_runner_schedule(lj_runner *r, uint64_t now, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng, int cue_edit)
{
    uint32_t link = 0;
    int n = 0, rc;
    if (!lj_runner_active(r) || r->stopped || r->seq->n <= 1) return 0;
    if (!(r->trig == LJ_TRIG_AUTO || r->chase_on || r->spt_left)) return 0;   /* F13: auto; F10: chase; F11: extra scenes auto-timed */
    while (now >= r->scene_due_us && n < LJ_CATCHUP_MAX) {    /* E10: scene_due += scaled time, drift-free */
        uint64_t t = r->scene_due_us;
        if (r->spt_left) { rc = lj_runner_advance(r, LJ_DIR_FWD, rng, cue_edit, &link); r->spt_left--; }   /* F11: extra scenes forward */
        else rc = lj_runner_advance(r, r->dir, rng, cue_edit, &link);
        if (rc == 1) break;
        lj_runner_enter(r, r->scene, t, tick_us, merged, cue_pct);
        if (rc == 2) break;                                    /* Goto scene: entered, then stopped */
        n++;
    }
    if (n >= LJ_CATCHUP_MAX) {                                 /* LORD_ENGINE-23: bounded catch-up, re-base */
        r->scene_due_us = now + tick_us;
        lj_log("engine: runner caught up %d scenes in one tick, re-based", n);
    }
    return link;
}

void lj_runner_stop(lj_runner *r) { r->stopped = 1; }           /* F14 + LJ_STOP_HOLDS_FADE: eval is skipped while stopped, so val[]/st[] hold (rig M8) */

void lj_runner_go(lj_runner *r, uint64_t now, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct)
{
    if (!r->seq) return;
    r->stopped = 0; r->link = 0;
    lj_runner_enter(r, r->scene, now, tick_us, merged, cue_pct);   /* PROPOSAL rig M8: Go restarts the held scene from the live output */
}

void lj_runner_eval(lj_runner *r, const lj_scene *sc, uint64_t now, const lj_pairs *pairs, const lj_post_ctx *c, const lj_mm_state *mm, int amp_pct)
{
    int ch, f;
    memset(r->st, LJ_OFF, LJ_CHANS);
    if (!sc) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int s = sc->state[ch], lsb, snap;
        uint32_t v;
        if (s == LJ_OFF || pairs->is_lsb[ch] || c->fixture_of[ch] < 0) continue;   /* F1 Off = transparent; LORD_FIXTURES-11 unpatched offsets stay silent */
        lsb = pairs->lsb_of[ch];
        snap = s == LJ_SNAP || r->fade_dur_us == 0 || r->disable_fade;             /* F2; F4/F28 snap; F15 Disable Fade (current sequence only) */
        if (lsb >= 0) {                                        /* ME-22, F24: the pair is one 16-bit value with one state */
            uint32_t to = (uint32_t)sc->val[ch] * 256u + sc->val[lsb], from = (uint32_t)r->from[ch] * 256u + r->from[lsb];
            v = snap ? to : lj_fade_value(from, to, now, r->fade_t0_us, r->fade_dur_us);   /* F3 */
            if (r->level_pct < 100) v = (v * r->level_pct + 50) / 100;                     /* G10: statics level scales every value */
            r->val[ch] = (uint8_t)(v >> 8); r->val[lsb] = (uint8_t)v;
            r->st[ch] = r->st[lsb] = (uint8_t)s;
        } else {
            v = snap ? sc->val[ch] : lj_fade_value(r->from[ch], sc->val[ch], now, r->fade_t0_us, r->fade_dur_us);
            if (r->level_pct < 100) v = (v * r->level_pct + 50) / 100;
            r->val[ch] = (uint8_t)v;
            r->st[ch] = (uint8_t)s;
        }
    }
    if (!mm || !c->show) return;
    for (f = 0; f < LJ_FIXTURES; f++) {                        /* F25, C9: the movement macro overlays this scene's pan/tilt, pre-merge */
        const lj_movemacro *m = &sc->macro[f];
        const lj_profile *p = c->profile_of[f];
        const lj_fixture *fx = &c->show->fixture[f];
        int32_t dp, dt, base, pan, tilt;
        if (!lj_movemacro_active(m) || !p || !fx->link || !(p->pt16 || p->pt8)) continue;
        base = lj_addr_code(fx->link, fx->address);
        pan = base + p->pan_ofs; tilt = base + p->tilt_ofs;
        if (pan + 1 >= LJ_CHANS || tilt + 1 >= LJ_CHANS) continue;
        lj_movemacro_offset(&mm[f], m, amp_pct, &dp, &dt);
        if (p->pt16) {
            int32_t v;
            if (r->st[pan] != LJ_OFF)  { v = r->val[pan] * 256 + r->val[pan + 1] + dp;   if (v < 0) v = 0; if (v > 65535) v = 65535; r->val[pan] = (uint8_t)(v >> 8); r->val[pan + 1] = (uint8_t)v; }
            if (r->st[tilt] != LJ_OFF) { v = r->val[tilt] * 256 + r->val[tilt + 1] + dt; if (v < 0) v = 0; if (v > 65535) v = 65535; r->val[tilt] = (uint8_t)(v >> 8); r->val[tilt + 1] = (uint8_t)v; }
        } else {
            int32_t v;
            if (r->st[pan] != LJ_OFF)  { v = r->val[pan] + dp / 256;  if (v < 0) v = 0; if (v > 255) v = 255; r->val[pan] = (uint8_t)v; }
            if (r->st[tilt] != LJ_OFF) { v = r->val[tilt] + dt / 256; if (v < 0) v = 0; if (v > 255) v = 255; r->val[tilt] = (uint8_t)v; }
        }
    }
}

int lj_runner_status(const lj_runner *r)
{
    if (!r->seq_no) return LJ_SLOT_EMPTY;
    if (!r->seq) return LJ_SLOT_LOAD_ERROR;                    /* [03 s5.1] "load error" */
    if (!r->enabled) return LJ_SLOT_DISABLED;
    if (r->edit) return LJ_SLOT_EDIT;
    if (r->out) return LJ_SLOT_OUT;
    if (r->stopped || r->seq->n <= 1 || r->first == r->last) return LJ_SLOT_STOPPED;   /* Stop: by the user, a static sequence, identical limits, or the end action (Link shows Stop + the runner's link flag; no LJ_SLOT_LINK in the contract) */
    return LJ_SLOT_RUNNING;
}
