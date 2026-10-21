/* lj_masters.c - S3 factor and scaling (11 G1-G5, LORD_ENGINE-18), GM fade runner (11 G11). */
#include "engine/lj_masters.h"
#include "engine/lj_engine.h"

int lj_master_factor(int sub, int gm, int mode)
{
    switch (mode) {                                            /* 11 G2: Proportional sub*gm, LoTP min, HTP max, No Effect sub */
    case LJ_MASTER_LOTP:      return sub < gm ? sub : gm;
    case LJ_MASTER_HTP:       return sub > gm ? sub : gm;
    case LJ_MASTER_NO_EFFECT: return sub;
    default:                  return (sub * gm + 127) / 255;   /* LJ_GM_DOMAIN 255, nearest (11 G13 UNVERIFIED, rig M22) */
    }
}

int lj_master_scale(int v, int k, int limit, int lo, int hi)
{
    if (limit) {                                               /* 11 G4 limit to range, G5 range-relative (LJ_GM_RANGE_RELATIVE, rig M21) */
        if (v < lo || v > hi) return v;
        return lo + ((v - lo) * k + 127) / 255;
    }
    return (v * k + 127) / 255;                                /* 11 G1 proportional */
}

void lj_gm_fade_start(lj_gm_fade *f, int in, uint32_t ms, uint8_t gm_now, uint64_t now_us)
{
    if (f->mode) return;                                       /* 11 G11: "selecting another time will not affect the currently active fade" */
    if (ms < LJ_GM_FADE_MIN_MS) ms = LJ_GM_FADE_MIN_MS;
    if (ms > LJ_GM_FADE_MAX_MS) ms = LJ_GM_FADE_MAX_MS;
    f->from = gm_now;
    if (in) { f->to = f->restore ? f->restore : 255; }
    else    { f->to = 0; f->restore = gm_now; }
    f->mode = (uint8_t)(in ? 2 : 1);
    f->t0_us = now_us;
    f->dur_us = (uint64_t)ms * 1000u;
}

int lj_gm_fade_step(lj_gm_fade *f, uint64_t now_us, uint8_t *gm)
{
    if (!f->mode) return 0;
    *gm = (uint8_t)lj_fade_value(f->from, f->to, now_us, f->t0_us, f->dur_us);
    if (now_us >= f->t0_us + f->dur_us) f->mode = 0;
    return f->mode;
}

void lj_gm_fade_cancel(lj_gm_fade *f) { f->mode = 0; }
