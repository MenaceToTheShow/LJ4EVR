/* lj_runner.h - runner scheduling and scene evaluation (design 3.2 steps 2-3,
 * 3.3 fade maths, 11 F1-F28, 03 s5.5): one lj_runner per cue slot, flash,
 * current sequence, background slot and active static. The engine owns the
 * arrays and the clock; these functions never allocate or do I/O. */
#ifndef LJ_ENGINE_RUNNER_H
#define LJ_ENGINE_RUNNER_H

#include <stdint.h>
#include "engine/lj_engine.h"
#include "engine/lj_movemacro.h"

typedef struct { int16_t lsb_of[LJ_CHANS]; uint8_t is_lsb[LJ_CHANS]; } lj_pairs;   /* declared 16-bit pairs (ME-22), built at bind */
void lj_runner_pairs_build(const lj_post_ctx *c, lj_pairs *p);

void lj_runner_clear(lj_runner *r);                                                        /* empty slot */
void lj_runner_set_slot(lj_runner *r, const lj_cue_slot *s, const lj_sequence *seq);       /* a cue slot's options [03 s5.5]; seq NULL = load error */
void lj_runner_set_seq(lj_runner *r, const lj_sequence *seq, uint32_t seq_no);             /* flash / current / background: forward, auto, programmed times */
int  lj_runner_active(const lj_runner *r);                                                 /* contributes this tick: pinned record (or an activated static), enabled, not Edit/Out */
void lj_runner_start(lj_runner *r, uint64_t t, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng);   /* enter the first scene per direction, Trig on Load */
void lj_runner_enter(lj_runner *r, uint16_t scene, uint64_t t, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct);  /* from[] = merged, scaled times, snap test, scene_due = t + st [11 F3 F6 F8 F9 F10 F16] */
uint32_t lj_runner_trig(lj_runner *r, int dir, int scenes, uint64_t at, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng, int cue_edit);   /* manual / audio trig; dir 0xFF = programmed [11 F11 F12]; returns a Link cue number (0 = none) */
uint32_t lj_runner_schedule(lj_runner *r, uint64_t now, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct, uint32_t *rng, int cue_edit);   /* auto trigs due by now; returns a Link cue number (0 = none) [11 E10 F13] */
void lj_runner_stop(lj_runner *r);                                                         /* holds [11 F14] */
void lj_runner_go(lj_runner *r, uint64_t now, uint32_t tick_us, const uint8_t merged[LJ_CHANS], uint16_t cue_pct);   /* restarts the current scene from the live output (PROPOSAL, rig M8) */
void lj_runner_eval(lj_runner *r, const lj_scene *sc, uint64_t now, const lj_pairs *pairs, const lj_post_ctx *c, const lj_mm_state *mm /* LJ_FIXTURES or NULL */, int amp_pct);   /* val[]/st[] for this tick */
int  lj_runner_status(const lj_runner *r);                                                 /* LJ_SLOT_* for the view */

#endif /* LJ_ENGINE_RUNNER_H */
