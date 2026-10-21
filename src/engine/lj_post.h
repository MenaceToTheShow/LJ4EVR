/* lj_post.h - post-processing S1-S9 (design 3.5): nine functions
 * int lj_post_Sn(const lj_post_ctx*, int link, int ch, int v) called in
 * LJ_POST_ORDER[] order (S1, S3, S2, S4-S9) per channel, then lj_post_run
 * splits merged[] into out[4][512]. S7 works on the 16-bit pair, so it takes
 * the whole frame (lj_post_s7_pairs) after the per-channel stages. */
#ifndef LJ_ENGINE_POST_H
#define LJ_ENGINE_POST_H

#include <stdint.h>
#include "engine/lj_engine.h"

int lj_post_s1_patch_src(const lj_post_ctx *c, int link, int ch, int v);      /* output-channel source copies the source's S0 value (PROPOSAL, rig M16); DMX-in modes gated by Overwrite Enable [02 s7.1] */
int lj_post_s3_masters(const lj_post_ctx *c, int link, int ch, int v);        /* runs second: GM/sub per enum lj_master_mode on intensity-class and ext_gm channels; LJ_GM_RANGE_RELATIVE limit; LJ_SWITCH_UNSCALED [11 G1-G5, ME-25] */
int lj_post_s2_profile_invert(const lj_post_ctx *c, int link, int ch, int v); /* runs third: 255 - v, or lo + hi - v inside [lo, hi] [11 P11] */
int lj_post_s4_blackout(const lj_post_ctx *c, int link, int ch, int v);       /* profile blackout values or shutter closed; solo; Blackout when GM at 0; Ignore Blackout [11 P15, P16] rig M17 */
int lj_post_s5_htp(const lj_post_ctx *c, int link, int ch, int v);            /* v = max(v, g_k); g_k scaled by the masters only with htp_masters [11 P8]; skips 16-bit pair members (ME-22 PROPOSAL, rig M11) */
int lj_post_s6_patch_inv_minmax(const lj_post_ctx *c, int link, int ch, int v);   /* invert, then clamp min/max [11 P4, P5] */
int lj_post_s8_overwrite(const lj_post_ctx *c, int link, int ch, int v);      /* DMX Output Monitor overwrites [11 P17] rig M19 */
int lj_post_s9_freeze_offline(const lj_post_ctx *c, int link, int ch, int v); /* freeze: S0 held, S1-S8 still run; offline: computed, not sent (engine thread) */
void lj_post_s7_pairs(const lj_post_ctx *c, uint8_t out[LJ_LINKS][LJ_LINK_CH]);   /* pan/tilt: invert 65535 - v on the pair, swap, MSB/LSB split, the profile patch [11 P10, 17 s4.2]; from[] and S7 reassemble (ME-22) */

void lj_post_run(const lj_post_ctx *c, const uint8_t merged[LJ_CHANS], uint8_t out[LJ_LINKS][LJ_LINK_CH]);   /* the whole chain in LJ_POST_ORDER; the S1 output-channel source copy happens here (it needs the source's S0 value, which the per-channel signature cannot carry) */

/* ---- W1 engine additions: the profile lookups the stages, the merge and the runners share (bodies in lj_post.c) ---- */
const lj_profile *lj_post_profile(const lj_post_ctx *c, int ch, int *ofs, const lj_fixture **fx);   /* NULL when unpatched; ofs = ch - the fixture's base address code */
int  lj_post_class(const lj_post_ctx *c, int ch);        /* enum lj_ch_class of the channel, LJ_CH_NONE when unpatched */
int  lj_post_pair_lsb(const lj_post_ctx *c, int ch);     /* the LSB channel of a declared 16-bit MSB (pt16 pan/tilt at ofs+1, or pair_lsb[]), else -1 (ME-22) */
int  lj_post_is_lsb(const lj_post_ctx *c, int ch);       /* 1 when ch is the LSB member of a declared pair */
int  lj_post_master_k(const lj_post_ctx *c, const lj_fixture *fx);   /* the S3 factor 0..255 for the fixture's sub-master group (LORD_ENGINE-18) */

#endif /* LJ_ENGINE_POST_H */
