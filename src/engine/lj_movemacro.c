/* lj_movemacro.c - 17 s7.3 definition in integers. Shapes in exe order [04 fact-check 11]:
 * 1 Ellipse (cos, sin), 2 Ellipse 2 (sin, cos: ASSUMED), 3 Cos(2a),Sin(a),
 * 4 Sin(a)*|Sin(2a)| (ASSUMED axis), 5 Sin(a)^3 (ASSUMED axis), 6 Stepped Ellipse; 7+ Bezier shapes of show->bez.
 * tests/vectors.h lj_vec_mm_trace (gen_vectors.py from 17-position_ref.py macro_offset) pins every value. */
#include "engine/lj_movemacro.h"
#include "engine/lj_engine_rules.h"
#include "engine/lj_trig_gen.h"
#include "model/lj_movemacro.h"

enum { LJ_MM_MASK = LJ_MOVEMACRO_N_TABLE - 1 };   /* LJ_MM_N (model/lj_movemacro.h) == LJ_MOVEMACRO_N_TABLE */
_Static_assert(LJ_MM_N == LJ_MOVEMACRO_N_TABLE, "Bezier tables and the trig tables share one size");

int lj_movemacro_scale(int raw, int pct)
{
    int v = raw * pct / 100;                                   /* floor, "may scale down to 0 as scaled values goes below 1" */
    return v > LJ_MOVEMACRO_AMP_MAX ? LJ_MOVEMACRO_AMP_MAX : (v < 0 ? 0 : v);
}

int lj_movemacro_active(const lj_movemacro *m)
{
    return m && m->type != LJ_MM_TYPE_NONE && m->fstate[LJ_MMP_TYPE] != LJ_OFF;   /* W3: Bezier types too; their table comes from show->bez (lj_movemacro_offset_bez) */
}

static uint32_t lj_mm_incr(int cycle_byte, uint32_t tick_us)
{
    uint64_t cycle_us = (uint64_t)cycle_byte * LJ_MOVEMACRO_CYCLE_UNIT_US;   /* PROPOSAL rig 17 s8 item 2 */
    if (cycle_us == 0) return 0;                               /* speed 0 = frozen [03 s5.11] */
    return (uint32_t)(((uint64_t)LJ_MM_N << 16) * tick_us / cycle_us);   /* incr = N * tick / cycle in 16.16 [17 s7.3] */
}

void lj_movemacro_advance(lj_mm_state *s, const lj_movemacro *m, uint32_t tick_us, int speed_pct)
{
    uint32_t ip, it;
    if (!lj_movemacro_active(m)) { s->idx_pan = s->idx_tilt = 0; return; }   /* reset when the previous output carried no macro [17 s7.1] */
    ip = lj_mm_incr(lj_movemacro_scale(m->cycle_pan, speed_pct), tick_us);
    it = (m->type == 1 || m->type == 6 || m->type >= LJ_MM_TYPE_BEZ0) ? ip : lj_mm_incr(lj_movemacro_scale(m->cycle_tilt, speed_pct), tick_us);   /* one common cycle for the ellipses (CHM 3197); Bezier too, so the drawn figure keeps its shape (PROPOSAL rig 17 s8 item 6) */
    if (m->dir) { ip = 0u - ip; it = 0u - it; }                /* CCW decrements; wraps modulo N << 16 */
    s->idx_pan  = (s->idx_pan + ip)  & ((LJ_MM_N << 16) - 1);
    s->idx_tilt = (s->idx_tilt + it) & ((LJ_MM_N << 16) - 1);
}

static int32_t lj_mm_round(int64_t a, int64_t b) { return (int32_t)(a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b)); }   /* b odd: never a tie */

static int32_t lj_mm_shape(int type, int axis /* 0 pan, 1 tilt */, int i)
{
    int32_t s = lj_sin4096[i & LJ_MM_MASK], c = lj_cos4096[i & LJ_MM_MASK];
    switch (type) {
    case 2:  return axis ? c : s;
    case 3:  return axis ? s : lj_cos4096[(2 * i) & LJ_MM_MASK];
    case 4:  { int32_t s2 = lj_sin4096[(2 * i) & LJ_MM_MASK]; if (s2 < 0) s2 = -s2; return axis ? lj_mm_round((int64_t)s * s2, 32767) : c; }   /* nearest (gen_vectors.py builds the same integer table) */
    case 5:  return axis ? lj_mm_round((int64_t)s * s * s, (int64_t)32767 * 32767) : c;
    default: return axis ? s : c;                              /* 1 and 6: pan = cos, tilt = sin (H26 p132) */
    }
}

static int32_t lj_mm_units(int32_t table, int amp)
{
    /* round(table / 32767 * amp * 32768 / 127) exactly as 17-position_ref.py macro_offset (127 = half range [17 R11]);
     * the denominator 127 * 32767 is odd, so there is never a tie and nearest == Python's round() */
    return lj_mm_round((int64_t)table * amp * 32768, (int64_t)LJ_MOVEMACRO_AMP_MAX * 32767);
}

void lj_movemacro_offset(const lj_mm_state *s, const lj_movemacro *m, int amp_pct, int32_t *dpan, int32_t *dtilt)
{
    lj_movemacro_offset_bez(s, m, NULL, amp_pct, dpan, dtilt);
}

void lj_movemacro_offset_bez(const lj_mm_state *s, const lj_movemacro *m, const lj_bez_set *bez, int amp_pct, int32_t *dpan, int32_t *dtilt)
{
    uint32_t ip, it, delay, step;
    const lj_bez_table *t = NULL;
    *dpan = *dtilt = 0;
    if (!lj_movemacro_active(m)) return;
    if (m->type >= LJ_MM_TYPE_BEZ0) {                         /* "to the engine a Bezier macro is just two value arrays" [04 s4.5] */
        if (!bez || !bez->t || m->type - LJ_MM_TYPE_BEZ0 >= bez->n) return;   /* shape not loaded: inactive */
        t = &bez->t[m->type - LJ_MM_TYPE_BEZ0];
    }
    delay = (uint32_t)(LJ_MM_N * m->delay / 256);              /* delay subtracts N * delay_byte / 256 (ASSUMED 3) */
    ip = ((s->idx_pan >> 16) - delay) & LJ_MM_MASK;            /* ME-83: unsigned, wrapped into 0..N-1 before any quantisation */
    it = ((s->idx_tilt >> 16) - delay + (uint32_t)(LJ_MM_N * m->tweak / 256)) & LJ_MM_MASK;   /* tweak on the tilt index (ASSUMED 4) */
    if (m->type == 6 && m->step > 1) { step = m->step; ip = ip / step * step; it = it / step * step; }   /* Stepped Ellipse (CHM 3199): floor in 0..N-1, so idx < delay lands in the last step, not step 0 */
    *dpan  = lj_mm_units(t ? t->pan[ip]  : lj_mm_shape(m->type, 0, (int)ip), lj_movemacro_scale(m->amp_pan, amp_pct));
    *dtilt = lj_mm_units(t ? t->tilt[it] : lj_mm_shape(m->type, 1, (int)it), lj_movemacro_scale(m->amp_tilt, amp_pct));
}

/* ---- W3 contract body (lj_movemacro.h): system types through lj_movemacro_offset; Bezier types are the movement track's ---- */
int lj_movemacro_pos(const lj_mm_state *s, const lj_movemacro *m, const lj_bez_set *bez, int amp_pct,
                     int32_t base_pan, int32_t base_tilt, int32_t *pan, int32_t *tilt)
{
    int32_t dp, dt;
    *pan = base_pan; *tilt = base_tilt;
    if (!lj_movemacro_active(m)) return -1;
    if (m->type >= LJ_MM_TYPE_BEZ0 && (!bez || !bez->t || m->type - LJ_MM_TYPE_BEZ0 >= bez->n)) return -1;
    lj_movemacro_offset_bez(s, m, bez, amp_pct, &dp, &dt);
    dp += base_pan; dt += base_tilt;
    *pan  = dp < -32768 ? -32768 : (dp > 32767 ? 32767 : dp);
    *tilt = dt < -32768 ? -32768 : (dt > 32767 ? 32767 : dt);
    return 0;
}
