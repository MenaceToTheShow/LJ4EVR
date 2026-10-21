/* lj_movemacro.h - movement macros (design 3.6, 17 s7): per fixture two 16.16
 * indexes into the LJ_MOVEMACRO_N_TABLE-point sine/cosine tables, advanced once
 * per tick in step 2 (ME-35) from the macro the previous tick's merged[] carried
 * for that fixture, reset when it carried none [17 s7.1]; runners only read it. */
#ifndef LJ_ENGINE_MOVEMACRO_H
#define LJ_ENGINE_MOVEMACRO_H

#include <stdint.h>
#include "model/lj_model.h"

typedef struct { uint32_t idx_pan, idx_tilt; } lj_mm_state;   /* 16.16 table index, wraps at LJ_MOVEMACRO_N_TABLE << 16 */

int  lj_movemacro_active(const lj_movemacro *m);                                                     /* type 1..6 with the type's fade state not Off */
void lj_movemacro_advance(lj_mm_state *s, const lj_movemacro *m, uint32_t tick_us, int speed_pct);  /* m NULL = reset; speed_pct = cue macro control (100 = raw) [03 s5.11] */
void lj_movemacro_offset(const lj_mm_state *s, const lj_movemacro *m, int amp_pct, int32_t *dpan, int32_t *dtilt);   /* 16-bit units: +-32768 at amplitude 127 [17 s7.3] */
int  lj_movemacro_scale(int raw, int pct);                                                           /* floor(raw * pct / 100) clamped 0..127 [17 s8] */

#endif /* LJ_ENGINE_MOVEMACRO_H */
