/* lj_genmacro.h - generic DMX macros, layer rank 6 (design 3.4, 3.6; 14 sA.3):
 * per channel a 16.16 index into an 8192-point shape, advanced once per tick,
 * delta = shape[(idx - delay) & 8191] * amp / 32640 on the reference (base or
 * the merged value of ranks 0-5), clamped 0..255 (16-bit on a declared pair).
 *
 * Two sources run at rank 6 (docs/w3-contracts.md seam 2): the cue's macro (LOAD_CUE.gm, or LOAD_GENMACRO
 * c = LJ_GM_SRC_CUE after a pending_genmacro request) and the Generic Macro Editor's working record
 * (LOAD_GENMACRO c = LJ_GM_SRC_EDITOR), applied after the cue's (LJ_GENMACRO_EDITOR_AFTER_CUE). Each source has
 * its own channel indexes; the shape table is the pinned GM_SHAPES pointer or the regenerated built-ins. */
#ifndef LJ_ENGINE_GENMACRO_H
#define LJ_ENGINE_GENMACRO_H

#include <stdint.h>
#include "engine/lj_engine.h"

typedef struct { uint32_t idx; uint8_t back; } lj_gm_ch;   /* 16.16 index 0..8191, back = bounce running backwards */

/* ---- W3 (docs/w3-contracts.md): the pure per-channel step and the shape table seam (genmacro track) ----
 * The shapes come from a lj_gm_shapes table: the regenerated built-ins (engine/lj_shapes_gen.h, the 66 shapes in
 * Macros.Mac file order, our integer samples) or the user's override pinned by LJ_CMD_GM_SHAPES (ME-34). Both
 * functions are pure (no engine state) so tests/vectors.h rows call them directly. */
int16_t lj_genmacro_shape_in(const lj_gm_shapes *sh, int shape, int k);   /* sh NULL or empty = lj_gm_shapes_builtin; shape >= n = 0; k wraps mod 8192 */
/* One channel's rank-6 value for the 16.16 index idx: s = shape[(idx >> 16) - delay], delta = s * Amp+ / 32640 for
 * s >= 0 and s * Amp- / 32640 for s < 0 (nearest), out = clamp(ref + delta). ref is the base when base_on, else the
 * merged value the caller passes. wide = 1: ref and the result are the 16-bit pair value and delta is scaled by
 * LJ_GENMACRO_16BIT_SCALE; returns 0..255 or 0..65535. */
int32_t lj_genmacro_step(const lj_gm_shapes *sh, const lj_gm_param *p, uint32_t idx, int32_t ref, int wide);
int32_t lj_genmacro_delta(const lj_gm_shapes *sh, const lj_gm_param *p, uint32_t idx);   /* the unclamped delta -255..255 of step (the editor's Min/Max readout) */
/* One tick of index advance for every programmed channel of m (cycle 0 = static; fwd / rev wrap, bounce reflects). */
void    lj_genmacro_advance(const lj_genmacro *m, lj_gm_ch st[LJ_CHANS], uint32_t tick_us);

/* ---- the rank-6 engine state (lj_engine.c holds one; zero-initialised = engine on, slot on, no macro, built-ins) ---- */
typedef struct {
    const lj_genmacro *m[2];             /* [LJ_GM_SRC_CUE], [LJ_GM_SRC_EDITOR]: pinned by the UI (design 2.3 rule 2) */
    lj_gm_ch st[2][LJ_CHANS];            /* per source, per channel index (CHM 5565: per channel, reset only by restart) */
    const lj_gm_shapes *shapes;          /* GM_SHAPES: NULL = lj_gm_shapes_builtin */
    uint8_t engine_off, slot_off;        /* Generic Macro Engine Off / the cue's macro slot off (GENMACRO_ON b = 0) */
} lj_gm_rank6;
void lj_gm_rank6_cmd(lj_gm_rank6 *g, const lj_cmd *c);                    /* LOAD_GENMACRO, GENMACRO_ON, GENMACRO_RESTART, GM_SHAPES */
void lj_gm_rank6_load_cue(lj_gm_rank6 *g, const lj_genmacro *m);         /* LOAD_CUE: the cue's pinned macro (NULL clears); restart if its flag is set [CHM 5841] */
void lj_gm_rank6_tick(lj_gm_rank6 *g, const lj_post_ctx *c, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS], uint32_t tick_us);   /* step 4 after lj_merge: apply, then advance */
void lj_gm_rank6_view(const lj_gm_rank6 *g, lj_engine_status *s);       /* gm_engine_on gm_slot_on gm_cue_number gm_editor_number */

#endif /* LJ_ENGINE_GENMACRO_H */
