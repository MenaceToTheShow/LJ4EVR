/* lj_movemacro.c - 17 s7.3 definition in integers. Shapes in exe order [04 fact-check 11]:
 * 1 Ellipse (cos, sin), 2 Ellipse 2 (sin, cos: ASSUMED), 3 Cos(2a),Sin(a),
 * 4 Sin(a)*|Sin(2a)| (ASSUMED axis), 5 Sin(a)^3 (ASSUMED axis), 6 Stepped Ellipse. */
#include "engine/lj_movemacro.h"
#include "engine/lj_engine_rules.h"
#include "engine/lj_trig_gen.h"

enum { LJ_MM_N = LJ_MOVEMACRO_N_TABLE, LJ_MM_MASK = LJ_MM_N - 1 };

int lj_movemacro_scale(int raw, int pct)
{
    int v = raw * pct / 100;                                   /* floor, "may scale down to 0 as scaled values goes below 1" */
    return v > LJ_MOVEMACRO_AMP_MAX ? LJ_MOVEMACRO_AMP_MAX : (v < 0 ? 0 : v);
}

int lj_movemacro_active(const lj_movemacro *m)
{
    return m && m->type >= 1 && m->type <= 6 && m->fstate[0] != LJ_OFF;
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
    it = (m->type == 1 || m->type == 6) ? ip : lj_mm_incr(lj_movemacro_scale(m->cycle_tilt, speed_pct), tick_us);   /* one common cycle for the ellipses (CHM 3197) */
    if (m->dir) { ip = 0u - ip; it = 0u - it; }                /* CCW decrements; wraps modulo N << 16 */
    s->idx_pan  = (s->idx_pan + ip)  & ((LJ_MM_N << 16) - 1);
    s->idx_tilt = (s->idx_tilt + it) & ((LJ_MM_N << 16) - 1);
}

static int32_t lj_mm_shape(int type, int axis /* 0 pan, 1 tilt */, int i)
{
    int32_t s = lj_sin4096[i & LJ_MM_MASK], c = lj_cos4096[i & LJ_MM_MASK];
    switch (type) {
    case 2:  return axis ? c : s;
    case 3:  return axis ? s : lj_cos4096[(2 * i) & LJ_MM_MASK];
    case 4:  { int32_t s2 = lj_sin4096[(2 * i) & LJ_MM_MASK]; if (s2 < 0) s2 = -s2; return axis ? (int32_t)((int64_t)s * s2 / 32767) : c; }
    case 5:  return axis ? (int32_t)((int64_t)s * s / 32767 * s / 32767) : c;
    default: return axis ? s : c;                              /* 1 and 6: pan = cos, tilt = sin (H26 p132) */
    }
}

static int32_t lj_mm_units(int32_t table, int amp)
{
    int64_t units = (int64_t)amp * 32768 / LJ_MOVEMACRO_AMP_MAX;   /* 127 = half range [17 R11] */
    int64_t v = (int64_t)table * units;
    return (int32_t)((v >= 0 ? v + 16383 : v - 16383) / 32767);    /* nearest */
}

void lj_movemacro_offset(const lj_mm_state *s, const lj_movemacro *m, int amp_pct, int32_t *dpan, int32_t *dtilt)
{
    uint32_t ip, it, delay, step;
    *dpan = *dtilt = 0;
    if (!lj_movemacro_active(m)) return;
    delay = (uint32_t)(LJ_MM_N * m->delay / 256);              /* delay subtracts N * delay_byte / 256 (ASSUMED 3) */
    ip = ((s->idx_pan >> 16) - delay) & LJ_MM_MASK;            /* ME-83: unsigned, wrapped into 0..N-1 before any quantisation */
    it = ((s->idx_tilt >> 16) - delay + (uint32_t)(LJ_MM_N * m->tweak / 256)) & LJ_MM_MASK;   /* tweak on the tilt index (ASSUMED 4) */
    if (m->type == 6 && m->step > 1) { step = m->step; ip = ip / step * step; it = it / step * step; }   /* Stepped Ellipse (CHM 3199): floor in 0..N-1, so idx < delay lands in the last step, not step 0 */
    *dpan  = lj_mm_units(lj_mm_shape(m->type, 0, (int)ip), lj_movemacro_scale(m->amp_pan, amp_pct));
    *dtilt = lj_mm_units(lj_mm_shape(m->type, 1, (int)it), lj_movemacro_scale(m->amp_tilt, amp_pct));
}
