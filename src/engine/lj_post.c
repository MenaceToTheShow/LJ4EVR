/* lj_post.c - S1-S9 (design 3.5, 11 s4.2). LJ_POST_ORDER is S1, S3, S2, S4-S9:
 * masters scale the un-inverted intensity, then the profile inverts (11 G6
 * UNVERIFIED, rig M21); Example 3 and 6 of 11 s5.2 hold only in this order.
 * Every stage is a pure function of the ctx and v; S7 reads the frame. */
#include <string.h>
#include "engine/lj_post.h"
#include "engine/lj_masters.h"

const uint8_t LJ_POST_ORDER[LJ_POST_COUNT] = { LJ_POST_S1_PATCH_SRC, LJ_POST_S3_MASTERS, LJ_POST_S2_PROFILE_INVERT, LJ_POST_S4_BLACKOUT, LJ_POST_S5_HTP,
                                               LJ_POST_S6_PATCH_INV_MINMAX, LJ_POST_S7_PANTILT, LJ_POST_S8_OVERWRITE, LJ_POST_S9_FREEZE_OFFLINE };

/* ---- shared lookups ---- */

const lj_profile *lj_post_profile(const lj_post_ctx *c, int ch, int *ofs, const lj_fixture **fxp)
{
    int f;
    const lj_fixture *fx;
    if (ch < 0 || ch >= LJ_CHANS || !c->show) return NULL;
    f = c->fixture_of[ch];
    if (f < 0) return NULL;
    fx = &c->show->fixture[f];
    *ofs = ch - lj_addr_code(fx->link, fx->address);
    if (fxp) *fxp = fx;
    return c->profile_of[f];
}

int lj_post_class(const lj_post_ctx *c, int ch)
{
    int ofs;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, NULL);
    return p ? p->channel_class[ofs] : LJ_CH_NONE;
}

int lj_post_pair_lsb(const lj_post_ctx *c, int ch)
{
    int ofs;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, NULL);
    if (!p) return -1;
    if (p->pt16 && (ofs == p->pan_ofs || ofs == p->tilt_ofs)) return ch + 1 < LJ_CHANS ? ch + 1 : -1;   /* 16 s4.4: MSB at ofs, LSB at ofs+1 */
    if (p->pair_lsb[ofs]) return ch - ofs + p->pair_lsb[ofs] - 1;                                     /* LJ_PAIR16_NON_PT_AS_ONE, rig M11 */
    return -1;
}

int lj_post_is_lsb(const lj_post_ctx *c, int ch)
{
    int ofs, j;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, NULL);
    if (!p) return 0;
    if (p->pt16 && (ofs == p->pan_ofs + 1 || ofs == p->tilt_ofs + 1)) return 1;
    for (j = 0; j < LJ_PROFILE_CH; j++) if (p->pair_lsb[j] == ofs + 1) return 1;
    return 0;
}

static int lj_post_sub(const lj_post_ctx *c, const lj_fixture *fx, int *mode)
{
    int g = fx->intensity_group;                               /* 0 = none, 1..8 */
    if (g < 1 || g > LJ_SUBMASTERS) { *mode = LJ_MASTER_PROPORTIONAL; return 255; }
    *mode = c->show->submaster_mode[g - 1];
    return c->sub[g - 1];
}

int lj_post_master_k(const lj_post_ctx *c, const lj_fixture *fx)
{
    int mode, sub = lj_post_sub(c, fx, &mode);
    return lj_master_factor(sub, c->gm, mode);
}

/* ---- the stages ---- */

int lj_post_s1_patch_src(const lj_post_ctx *c, int link, int ch, int v)
{
    const lj_patch_ch *pc;
    int in;
    (void)link;
    if (!c->patch) return v;
    pc = &c->patch[ch];
    if (pc->src_kind != LJ_SRC_DMX_IN || !c->overwrite_enable) return v;   /* DMX-in gated by Overwrite Enable [02 s7.1]; the DMX Out source is copied in lj_post_run */
    if (pc->src_link < 1 || pc->src_link > LJ_LINKS || pc->src_ch < 1 || pc->src_ch > LJ_LINK_CH) return v;
    in = c->dmx_in[lj_addr_code(pc->src_link, pc->src_ch)];
    switch (pc->in_mode) {                                     /* 11 S1 modes */
    case LJ_IN_LOTP:  return in < v ? in : v;
    case LJ_IN_HTP:   return in > v ? in : v;
    case LJ_IN_GT0:   return in > 0 ? in : v;
    case LJ_IN_LT255: return in < 255 ? in : v;
    default:          return in;                               /* Absolute */
    }
}

int lj_post_s3_masters(const lj_post_ctx *c, int link, int ch, int v)
{
    int ofs, cls, mode, sub, k;
    const lj_fixture *fx;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, &fx);
    (void)link;
    if (!p || fx->ignore_gm) return v;                         /* 11 G3: Ignore Grand Master */
    if (fx->ch_gm_off >> ofs & 1u) return v;                   /* ME-32: Channel Configuration "Disable Grand Master Control" on this offset */
    cls = p->channel_class[ofs];
    sub = lj_post_sub(c, fx, &mode);
    if (cls == LJ_CH_SWITCH) return (c->gm == 0 || sub == 0) ? 0 : v;   /* ME-25 (S3): LJ_SWITCH_UNSCALED */
    if (cls == LJ_CH_STROBE && fx->link_strobe_gm && (c->gm == 0 || sub == 0)) return p->strobe_off;   /* 11 G7: Link Strobe/GM at 0 */
    k = lj_master_factor(sub, c->gm, mode);                    /* LORD_ENGINE-18: one factor per mode */
    if (cls == LJ_CH_INTENSITY && p->use_gm)                   /* 11 G3 Enable Grand Master/Intensity; P14: the dedicated channel's setting wins */
        return lj_master_scale(v, k, p->limit_gm, p->intensity.min, p->intensity.max);   /* 11 G4, G5 (range-relative, rig M21) */
    if (p->ext_gm[ofs]) return lj_master_scale(v, k, 0, 0, 255);   /* Extended GM: standard scaling (CHM L10817) */
    return v;
}

int lj_post_s2_profile_invert(const lj_post_ctx *c, int link, int ch, int v)
{
    int ofs;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, NULL);
    (void)link;
    if (!p || !p->invert_int || p->channel_class[ofs] != LJ_CH_INTENSITY) return v;
    if (p->invert_int_range) {                                 /* 11 P11: range, "regardless of actual source" */
        int lo = p->intensity.min, hi = p->intensity.max;
        return (v >= lo && v <= hi) ? lo + hi - v : v;
    }
    return 255 - v;                                            /* 11 P11: entire channel */
}

int lj_post_s4_blackout(const lj_post_ctx *c, int link, int ch, int v)
{
    int ofs, active;
    const lj_fixture *fx;
    const lj_profile *p = lj_post_profile(c, ch, &ofs, &fx);
    (void)link;
    if (!p || fx->ignore_bo) return v;                         /* 11 P15: Ignore Blackout */
    active = c->blackout || (c->solo && c->fixture_of[ch] != c->solo_fixture)   /* 11 P16: solo = blackout of the unselected */
          || (c->gm == 0 && p->bo_at_gm0);                     /* 11 G7: Blackout when Grand Master at 0 */
    if (!active) return v;
    if (p->blackout.enabled && p->blackout.defined[ofs]) return p->blackout.value[ofs];   /* literal profile blackout values (CHM L10441), after S3 so never master-scaled (rig M17) */
    if (p->shutter.enabled && ofs == p->shutter.offset) return p->shutter.min;           /* shutter closed = the shutter function's Close value (16 s3.1 Shutter Offset/Close/Open) */
    return v;
}

int lj_post_s5_htp(const lj_post_ctx *c, int link, int ch, int v)
{
    uint32_t mask;
    int k, ofs;
    const lj_fixture *fx;
    const lj_profile *p;
    (void)link;
    if (!c->patch) return v;
    mask = c->patch[ch].htp_mask;
    if (!mask) return v;
    p = lj_post_profile(c, ch, &ofs, &fx);
    if (p && (lj_post_pair_lsb(c, ch) >= 0 || lj_post_is_lsb(c, ch))) return v;   /* ME-22: a bytewise max on a pair is meaningless (PROPOSAL, rig M11) */
    for (k = 0; k < LJ_HTP_GROUPS; k++) {
        int g;
        if (!(mask & (1u << k))) continue;
        g = c->htp_level[k + 1];
        if (c->htp_masters && p) g = lj_master_scale(g, lj_post_master_k(c, fx), 0, 0, 255);   /* 11 P8: exempt unless enabled */
        if (g > v) v = g;
    }
    return v;
}

int lj_post_s6_patch_inv_minmax(const lj_post_ctx *c, int link, int ch, int v)
{
    const lj_patch_ch *pc;
    (void)link;
    if (!c->patch) return v;
    pc = &c->patch[ch];
    if (pc->invert) v = 255 - v;                               /* 11 P4: invert before min/max */
    if (pc->max && v > pc->max) v = pc->max;                   /* 11 P5; 0 = no limit */
    if (v < pc->min) v = pc->min;
    return v;
}

int lj_post_s8_overwrite(const lj_post_ctx *c, int link, int ch, int v)
{
    (void)link;
    return c->ovr_on[ch] ? c->ovr_val[ch] : v;                 /* 11 P17, C13: after every patch */
}

int lj_post_s9_freeze_offline(const lj_post_ctx *c, int link, int ch, int v)
{
    (void)c; (void)link; (void)ch;
    return v;                                                  /* freeze holds the S0 inputs in lj_engine_tick; offline is read by the engine thread (lj_engine_is_offline) */
}

static int lj_post_get(uint8_t out[LJ_LINKS][LJ_LINK_CH], int code) { return out[code / LJ_LINK_CH][code % LJ_LINK_CH]; }
static void lj_post_put(uint8_t out[LJ_LINKS][LJ_LINK_CH], int code, int v) { if (code >= 0 && code < LJ_CHANS) out[code / LJ_LINK_CH][code % LJ_LINK_CH] = (uint8_t)v; }

void lj_post_s7_pairs(const lj_post_ctx *c, uint8_t out[LJ_LINKS][LJ_LINK_CH])
{
    int f;
    if (!c->show) return;
    for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_fixture *fx = &c->show->fixture[f];
        const lj_profile *p = c->profile_of[f];
        int base, pan, tilt, t;
        if (!fx->link || !p || !(p->pt16 || p->pt8)) continue;
        base = lj_addr_code(fx->link, fx->address);
        if (p->pt16) {
            if (base + p->pan_ofs + 1 >= LJ_CHANS || base + p->tilt_ofs + 1 >= LJ_CHANS) continue;
            pan  = lj_post_get(out, base + p->pan_ofs)  * 256 + lj_post_get(out, base + p->pan_ofs + 1);
            tilt = lj_post_get(out, base + p->tilt_ofs) * 256 + lj_post_get(out, base + p->tilt_ofs + 1);
            if (fx->invert_pan)  pan  = 65535 - pan;             /* 17 s4.2: invert on the whole pair, before swap */
            if (fx->invert_tilt) tilt = 65535 - tilt;
            if (fx->swap_pt) { t = pan; pan = tilt; tilt = t; }
            if (p->pt_patch_on) {                                /* 17 s4.3: pan MSB, pan LSB, tilt MSB, tilt LSB offsets */
                lj_post_put(out, base + p->pt_patch[0], pan >> 8);  lj_post_put(out, base + p->pt_patch[1], pan & 255);
                lj_post_put(out, base + p->pt_patch[2], tilt >> 8); lj_post_put(out, base + p->pt_patch[3], tilt & 255);
            } else {
                lj_post_put(out, base + p->pan_ofs, pan >> 8);   lj_post_put(out, base + p->pan_ofs + 1, pan & 255);
                lj_post_put(out, base + p->tilt_ofs, tilt >> 8); lj_post_put(out, base + p->tilt_ofs + 1, tilt & 255);
            }
        } else {
            if (base + p->pan_ofs >= LJ_CHANS || base + p->tilt_ofs >= LJ_CHANS) continue;
            pan  = lj_post_get(out, base + p->pan_ofs);
            tilt = lj_post_get(out, base + p->tilt_ofs);
            if (fx->invert_pan)  pan  = 255 - pan;
            if (fx->invert_tilt) tilt = 255 - tilt;
            if (fx->swap_pt) { t = pan; pan = tilt; tilt = t; }
            lj_post_put(out, base + p->pan_ofs, pan);
            lj_post_put(out, base + p->tilt_ofs, tilt);
        }
    }
}

static int lj_post_stage(const lj_post_ctx *c, int stage, int link, int ch, int v)
{
    switch (stage) {
    case LJ_POST_S1_PATCH_SRC:        return lj_post_s1_patch_src(c, link, ch, v);
    case LJ_POST_S3_MASTERS:          return lj_post_s3_masters(c, link, ch, v);
    case LJ_POST_S2_PROFILE_INVERT:   return lj_post_s2_profile_invert(c, link, ch, v);
    case LJ_POST_S4_BLACKOUT:         return lj_post_s4_blackout(c, link, ch, v);
    case LJ_POST_S5_HTP:              return lj_post_s5_htp(c, link, ch, v);
    case LJ_POST_S6_PATCH_INV_MINMAX: return lj_post_s6_patch_inv_minmax(c, link, ch, v);
    case LJ_POST_S8_OVERWRITE:        return lj_post_s8_overwrite(c, link, ch, v);
    case LJ_POST_S9_FREEZE_OFFLINE:   return lj_post_s9_freeze_offline(c, link, ch, v);
    default: return v;                                     /* S7 runs on the frame */
    }
}

void lj_post_run(const lj_post_ctx *c, const uint8_t merged[LJ_CHANS], uint8_t out[LJ_LINKS][LJ_LINK_CH])
{
    int ch, i, s7 = 0;
    while (s7 < LJ_POST_COUNT && LJ_POST_ORDER[s7] != LJ_POST_S7_PANTILT) s7++;
    for (ch = 0; ch < LJ_CHANS; ch++) {                        /* the per-channel stages before S7 */
        int link = ch / LJ_LINK_CH, v = merged[ch];
        if (c->patch) {
            const lj_patch_ch *pc = &c->patch[ch];        /* S1 output-channel source: the source's S0 value (PROPOSAL, rig M16) */
            if (pc->src_kind == LJ_SRC_DMX_OUT && pc->src_link >= 1 && pc->src_link <= LJ_LINKS && pc->src_ch >= 1 && pc->src_ch <= LJ_LINK_CH)
                v = merged[lj_addr_code(pc->src_link, pc->src_ch)];
        }
        for (i = 0; i < s7; i++) v = lj_post_stage(c, LJ_POST_ORDER[i], link, ch, v);
        out[link][ch % LJ_LINK_CH] = (uint8_t)v;
    }
    lj_post_s7_pairs(c, out);                                  /* S7 on the frame */
    for (ch = 0; ch < LJ_CHANS; ch++) {                        /* the stages after S7 (S8 overwrite, S9) */
        int link = ch / LJ_LINK_CH, v = out[link][ch % LJ_LINK_CH];
        for (i = s7 + 1; i < LJ_POST_COUNT; i++) v = lj_post_stage(c, LJ_POST_ORDER[i], link, ch, v);
        out[link][ch % LJ_LINK_CH] = (uint8_t)v;
    }
}
