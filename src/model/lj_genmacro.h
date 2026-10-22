/* lj_genmacro.h - CueMacros\NNNNNNNN.CMA (design 5.2: "LJ4 macro 1", num name
 * restart, "ch l:a shape=7 bon=1 base=100 amp=45,45 cycle=168 dir=fwd delay=0"
 * per programmed channel; cycle in deciseconds as the struct holds it, 14 sA.2).
 * The engine runs it at rank 6 (engine/lj_genmacro.h).
 *
 * W3 (docs/w3-contracts.md): the shape-table record and the Macros.Mac loader of
 * ME-34 (a user's own Macros.Mac / UserMacros.Mac in the library's override
 * folder replaces the regenerated built-ins; nothing from reference/ ships), the
 * "programmed channel" rule, and the editor's Auto Delay arithmetic [14 sA.5].
 * Two W1 defects the genmacro track fixes in lj_genmacro.c: _save skips every
 * channel with shape 0 ('2*Sine' is a real shape: use lj_genmacro_ch_used), and
 * _validate rejects bounce (14 sA.2 allows fwd / rev / bounce). */
#ifndef LJ_MODEL_GENMACRO_H
#define LJ_MODEL_GENMACRO_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_model.h"

int          lj_genmacro_load(const char *path, lj_genmacro *out, char *err, size_t cap);
int          lj_genmacro_save(const char *path, const lj_genmacro *in);
const char  *lj_genmacro_validate(const lj_genmacro *in);                 /* NULL = valid: dir fwd/rev/bounce, delay <= 8191, cycle_ds <= LJ_GM_CYCLE_MAX_DS, |amp| <= 255, shape < LJ_GM_SHAPES_MAX, name NUL-terminated */
lj_genmacro *lj_genmacro_new_rec(void);                                   /* heap, zeroed (free with free()) */

/* ---- W3 ---- */
enum { LJ_GM_SAMPLES = 8192 /* per shape [14 sA.3]; == engine LJ_GENMACRO_SHAPE_N */, LJ_GM_SHAPES = 66 /* Macros.Mac records [14 sA.6] */,
       LJ_GM_SHAPES_MAX = 255 /* lj_gm_param.shape is a byte: built-ins 0..nb-1, then the User-shapes tab */,
       LJ_GM_FULL = 32640 /* sample full scale = 127.5 x 256 [14 sA.6] */, LJ_GM_DELAY_MAX = 8191, LJ_GM_CYCLE_MAX_DS = 5950 /* 595 s [14 sA.2] */ };
int lj_genmacro_ch_used(const lj_genmacro *m, int ch);   /* the channel is programmed: shape, base or a parameter set (what _save writes and the editor lists) */

/* The shape table the engine reads: the regenerated built-ins (engine/lj_shapes_gen.h lj_gm_shapes_builtin), or a
 * user's override loaded here and handed over pinned with LJ_CMD_GM_SHAPES (ME-34). Names are ShortString[15]. */
typedef struct { uint16_t n, nb;                           /* total shapes, of which built-in (the Built-in shapes tab); n - nb = User-shapes */
                 const int16_t *s;                         /* n * LJ_GM_SAMPLES samples, +-LJ_GM_FULL */
                 char name[LJ_GM_SHAPES_MAX][16]; } lj_gm_shapes;
/* Macros.Mac layout [14 sA.6]: int32 version 1, int32 count, 4096 reserved, then count x 16415-byte records
 * (ShortString[15] name, 15 unknown, 8192 x int16 LE). Every length and offset is checked (an untrusted file);
 * user_mac NULL or missing = no user shapes. The loader allocates s; lj_gm_shapes_free releases it. 0 / -1 + err. */
int  lj_gm_shapes_load(const char *macros_mac, const char *user_mac, lj_gm_shapes *out, char *err, size_t cap);
void lj_gm_shapes_free(lj_gm_shapes *sh);
#define LJ_GM_OVERRIDE_DIR "Override"   /* <library>\Override\Macros.Mac, UserMacros.Mac (PROPOSAL folder name, ME-34; docs/fidelity.md row) */

/* Auto Delay [14 sA.5]: Delays tab (style, From / To, Use full delay cycle values) and Delay Order tab. n fixtures in
 * their delay order get out[k] in 0..8191; To < From wraps "start value->8191->0->end value"; Random draws from *seed
 * (xorshift32, deterministic for tests). 0 / -1 (n < 1, groups < 2 for circular / segments). */
enum lj_gm_delay_style { LJ_GMD_LINEAR = 0, LJ_GMD_CIRCULAR, LJ_GMD_SEGMENTS, LJ_GMD_RANDOM };
enum lj_delay_order { LJ_DO_FIXTURE = 0, LJ_DO_FIXTURE_REV, LJ_DO_ICON_H, LJ_DO_ICON_H_REV, LJ_DO_ICON_V, LJ_DO_ICON_V_REV };   /* Delay Order radios, shared with the movement Auto Delay [17 s6] */
int  lj_genmacro_auto_delay(int n, int style, int from, int to, int full_cycle, int groups, uint32_t *seed, uint16_t *out);

#endif /* LJ_MODEL_GENMACRO_H */
