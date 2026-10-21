/* lj_engine_rules.h - every PROPOSAL / UNVERIFIED engine constant, one line each
 * (design 3.3): comment = research note ref + verdict + reference-rig row
 * (12-gap-reference-rig.md). W0 carries the ones the design lists; W1 adds the
 * rest beside them as the runners land. Change a value here, never in a runner. */
#ifndef LJ_ENGINE_ENGINE_RULES_H
#define LJ_ENGINE_ENGINE_RULES_H

#define LJ_TICK_HZ_DEFAULT           40   /* D9, Main.Ini [ENGINE] TickHz default; 11 E-e PROPOSAL, rig M1 */
#define LJ_TICK_US_DEFAULT        25000   /* 1000000 / LJ_TICK_HZ_DEFAULT; 11 E-e PROPOSAL, rig M1 */
#define LJ_SNAP_MS                   50   /* fade_ms <= this snaps; 11 F4 CONFIRMED (== vs <= rig M3) */
#define LJ_FADE_LE_TICK_IS_SNAP       1   /* fade_us <= tick_us snaps; 11 F28 PROPOSAL, rig M3 */
#define LJ_FADE_ROUND_NEAREST         1   /* 11 F3 rounding UNVERIFIED, rig M2: 1 = nearest, ties away from `from`; 0 = truncate toward `from` (from + d*e/dur with C truncation). The two values are what the rig can distinguish. */
#define LJ_TIME_SCALE_AT_TRIG_ONLY    1   /* 11 F9 mid-scene UNVERIFIED, rig M7 */
#define LJ_STOP_HOLDS_FADE            1   /* 11 F14 UNVERIFIED, rig M8 */
#define LJ_STATIC_RELEASE_SNAPS       1   /* 11 F17 UNVERIFIED, rig M9 */
#define LJ_FREEZE_PAUSED_CLOCK        1   /* 11 F20 PROPOSAL, rig M10 */
#define LJ_PAIR16_NON_PT_AS_ONE       1   /* 11 F24 UNVERIFIED, rig M11 */
#define LJ_GM_DOMAIN                255   /* 11 G12 UNVERIFIED, rig M22 */
#define LJ_GM_RANGE_RELATIVE          1   /* 11 G5 DERIVED, rig M21 */
#define LJ_MOVEMACRO_N_TABLE       4096   /* 17 §7.3 ASSUMED, rig 17 §8 item 6; == size of lj_sin4096 */
#define LJ_MOVEMACRO_AMP_MAX        127   /* 17 R11 */
#define LJ_STATICS2_SLOTS_PER_PAGE   20   /* 14 §B.8 item 2 PROPOSAL */
#define LJ_SEQFORM_SHRINK_TO_PANEL1   1   /* design 7.6 / R17 PROPOSAL, rig Q6: Sequence window client becomes 354x106 on show; --explode shows the DFM as designed */

/* W1: LJ_LAYER_ORDER[8] (11 §3.4; 2-4, 6-7 PROPOSAL rig M12-M15) and LJ_POST_ORDER[9]
 * (11 §4.2 PROPOSAL rig M16-M20, S3 before S2 on purpose, 11 G6 rig M21) go here
 * once the LJ_L_* and LJ_POST_* enums exist (design 3.3). */

#endif /* LJ_ENGINE_ENGINE_RULES_H */
