/* lj_engine_rules.h - every PROPOSAL / UNVERIFIED engine constant, one line each
 * (design 3.3): comment = research note ref + verdict + reference-rig row
 * (12-gap-reference-rig.md). Change a value here, never in a runner. The layer
 * and post-stage orders are extern arrays (lj_merge.c, lj_post.c) so a test can
 * read the order the engine actually runs. */
#ifndef LJ_ENGINE_ENGINE_RULES_H
#define LJ_ENGINE_ENGINE_RULES_H

#include <stdint.h>

#define LJ_TICK_HZ_DEFAULT           40   /* D9, Main.Ini [ENGINE] TickHz default; 11 E-e PROPOSAL, rig M1 */
#define LJ_TICK_US_DEFAULT        25000   /* 1000000 / LJ_TICK_HZ_DEFAULT; 11 E-e PROPOSAL, rig M1 */
#define LJ_TICK_HZ_MIN               10   /* ME-24 (TickHz): clamped at load so the ~1/s keep-alive stays in the 800-1000 ms window */
#define LJ_TICK_HZ_MAX              200   /* ME-24 (TickHz) */
#define LJ_SNAP_MS                   50   /* fade_ms <= this snaps; 11 F4 CONFIRMED (== vs <= rig M3) */
#define LJ_FADE_LE_TICK_IS_SNAP       1   /* fade_us <= tick_us snaps; 11 F28 PROPOSAL, rig M3 */
#define LJ_FADE_ROUND_NEAREST         1   /* 11 F3 rounding UNVERIFIED, rig M2: 1 = nearest, ties away from `from`; 0 = truncate toward `from` (from + d*e/dur with C truncation). The two values are what the rig can distinguish. */
#define LJ_SNAP_TEST_AFTER_SCALE      1   /* LORD_ENGINE-11: the <= LJ_SNAP_MS test is applied to the time-scaled fade (min(60000, ms * pct / 100)), not the programmed one; rig M7 */
#define LJ_TIME_SCALE_AT_TRIG_ONLY    1   /* 11 F9 mid-scene UNVERIFIED, rig M7 */
#define LJ_STOP_HOLDS_FADE            1   /* 11 F14 UNVERIFIED, rig M8 */
#define LJ_STATIC_RELEASE_SNAPS       1   /* 11 F17 UNVERIFIED, rig M9 */
#define LJ_FREEZE_PAUSED_CLOCK        1   /* 11 F20 PROPOSAL, rig M10 */
#define LJ_PAIR16_NON_PT_AS_ONE       1   /* 11 F24 UNVERIFIED, rig M11 */
#define LJ_GM_DOMAIN                255   /* 11 G12 UNVERIFIED, rig M22 */
#define LJ_GM_RANGE_RELATIVE          1   /* 11 G5 DERIVED, rig M21 */
#define LJ_GM_FADE_MIN_MS           500   /* 11 G11: GM Fade Out/In 0.5-10 s */
#define LJ_GM_FADE_MAX_MS         10000   /* 11 G11 */
#define LJ_GM_FADE_RUNS_FROZEN        1   /* 11 S9 PROPOSAL, rig row (ME-71): masters keep working while frozen; the GM Fade runner is a master move (G11), not a scene fade (F20), so it steps while frozen and unfreeze does not shift its t0 */
#define LJ_SWITCH_UNSCALED            1   /* ME-25 (S3): LJ_CH_SWITCH keeps v unless gm == 0 || sub == 0 (no Fade, unscaled until a master hits 0) */
#define LJ_CATCHUP_BOUNDED            1   /* LORD_ENGINE-23: a late tick runs one pass and re-bases `next = now + tick_us`, never a burst (design 3.1) */
#define LJ_MOVEMACRO_N_TABLE       4096   /* 17 s7.3 ASSUMED, rig 17 s8 item 6; == size of lj_sin4096 */
#define LJ_MOVEMACRO_AMP_MAX        127   /* 17 R11 */
#define LJ_GENMACRO_SHAPE_N        8192   /* 14 sA.3: shape index space, delta = shape[(idx - delay) & 8191] * amp / 32640 */
#define LJ_STATICS2_SLOTS_PER_PAGE   20   /* 14 sB.8 item 2 PROPOSAL (== model/lj_model.h) */
#define LJ_STATIC_RUNNERS            64   /* one runner per active static slot (design 3.2); PROPOSAL: the 65th activation releases the oldest (LTP order), rig 14 sB.8 item 10 */
#define LJ_SEQFORM_SHRINK_TO_PANEL1   1   /* design 7.6 / R17 PROPOSAL, rig Q6: Sequence window client becomes 354x106 on show; --explode shows the DFM as designed */
#define LJ_ENGINE_CMD_CAP           256   /* the command array; a full array logs and drops (design 2.3 rule 1) */
#define LJ_CATCHUP_MAX              100   /* LORD_ENGINE-23: a late tick advances at most this many due scenes per runner in one pass, then logs and re-bases; PROPOSAL */
#define LJ_KEEP_CONTENTS_BLIND        1   /* LORD_ENGINE-13: [SEQ] KeepContentsBlind default; rank 3 = the contribution copied at blind entry (W2: the Main.Ini key) */
#define LJ_STATICS_USE_FADETIME       0   /* 11 F17: [STATICS] Use Fadetime=0 default; 1 = recorded fade time per entry (W2: the Main.Ini key) */
#define LJ_MOVEMACRO_CYCLE_UNIT_US 100000 /* 17 s7.3 ASSUMED (2): the cycle byte 0..127 in deciseconds; PROPOSAL, rig 17 s8 item 2 */
#define LJ_RNG_SEED          0x9E3779B9u  /* random direction [03 s5.5]: xorshift32 with a fixed seed so frame dumps stay deterministic */

/* ---- W2 constants (design 10 W2; appended) ---- */
#define LJ_TIME_PCT_MIN               1   /* 03 s5.6: the 13 cue time faders 1..999 %, 0 = CUE (slots) / 100 = as programmed */
#define LJ_TIME_PCT_MAX             999   /* 03 s5.6 */
#define LJ_TIME_PCT_CUE               0   /* the slot fader value that defers to the CUE fader */
#define LJ_CHASE_MIN_MS               1   /* E9: the tap timer resolution since 2.3; a chase period below the tick still re-arms per tick (PROPOSAL, rig M3) */
#define LJ_CHASE_MAX_MS           60000   /* LJ_TIME_MAX_MS: a tap gap above this is a new first tap, not a period (PROPOSAL, rig M3) */
#define LJ_CHASE_FADE_IS_PERIOD       1   /* 11 F10: fade := chase period unless [CUE] DisableChaseFade=1; not scaled by the cue time */
#define LJ_CHASE_GLOBAL_RATE          1   /* 03 s5.5 / 11 F10: one global chase rate carried across cue loads (TCurrentCue M_UseGlobalChaseRate default ON, PROPOSAL rig M3) */
#define LJ_CHASE_KEEPS_DIR            1   /* RN 2.6 b6: a chased slot keeps its saved loop direction */
#define LJ_LOOP_RANDOM_NO_COUNT       1   /* 03 s5.5: loop count not available in random mode (a loops= value is ignored, the validator rejects it) */
#define LJ_BOUNCE_END_AT_FIRST        1   /* 03 s5.5: 1->2->3->4->3->2->1->2; one loop = first .. last .. first (the end action fires on reaching first going back); PROPOSAL rig 03 Unverified */
#define LJ_LOOP_LIMITS_CLAMP          1   /* first/last outside 1..n clamp to the sequence (a cue referencing a shortened sequence); PROPOSAL rig */
#define LJ_SPT_FORWARD_AUTO           1   /* 11 F11: Scenes/Trig extras run forward with programmed times, trigs ignored until done */
#define LJ_TRANSPARENT_DEFAULT        0   /* enum lj_transparent_mode LJ_TRANSPARENT_USE: honour the cue's saved flag [03 s5.8] */
#define LJ_TRANSPARENT_KEEPS_ONOFF    1   /* RN 2.2 b4: loading a transparent cue does not reset on/off states of unaffected slots */
#define LJ_FLASH_LATCH_DEFAULT        0   /* TCurrentCue M_StickyFlashSequences "Latch Flash Sequences" default OFF (PROPOSAL rig) */
#define LJ_FLASH_RELEASE_ON_LOAD      0   /* TCurrentCue M_FlashAutoRelease "Release Flash on Load" default OFF (PROPOSAL rig) */
#define LJ_FLASH_ONE_AT_A_TIME        1   /* 03 s5.10: only one sequence may be flashed at any given time */
#define LJ_BG_FORWARD_ONLY            1   /* 03 s7: background slots always run forward; NOLOOP stops after the last scene */
#define LJ_BG_SLOT5_OVER_1            1   /* 03 s7: inside the BG cue, the higher slot number wins */
#define LJ_BG_RESTORE_ONOFF           0   /* TBGCueForm M_RestoreState "Restore Sequence on/off state" default OFF: every slot loads enabled [03 s7] */
#define LJ_STATIC_LTP_PER_CHANNEL     1   /* 03 s8: latest activated entry wins per channel; every activation (incl. level-driven) tops the list */
#define LJ_STATIC_LEVEL_SCALES_ALL    1   /* 03 s8: the per-entry level scales every value of the entry regardless of channel class */
#define LJ_STATIC_LEVEL_DEFAULT_PCT 100   /* [STATICS] DefaultLevel=255 -> 100 % (Default Levels at 100 %) */
#define LJ_STATIC_SOLO_BLOCKS_LATER   0   /* 14 sB.3 reading: Solo scopes = Replace scopes in W2; 1 would suppress later activations until released; PROPOSAL rig 14 sB.8 item 3 */
#define LJ_STATIC_SCOPE_ADD           0   /* enum lj_replace LJ_REP_ADD = the old Merge mode: LTP, no release; PROPOSAL rig 14 sB.8 item 3 */
#define LJ_STATIC_OLDEST_RELEASED     1   /* the 65th activation releases the oldest active static (LJ_STATIC_RUNNERS); PROPOSAL rig 14 sB.8 item 10 */
#define LJ_STATIC_TRANSPARENT_IS_ADD  1   /* a Transparent slot type (14 sB.4) contributes only its programmed channels, which every static already does: no engine effect in W2; PROPOSAL rig 14 sB.8 item 6 */
#define LJ_SMOKE_RANK_ABOVE_BG        1   /* 11 C12 pair 3: smoke > BG cue, rig M15 */
#define LJ_SMOKE_RANK_ABOVE_GENMACRO  1   /* 11 C12: PROPOSAL smoke > generic macro, rig M15 */
#define LJ_SMOKE_HAZE_OVERRIDE_GT0    1   /* 11 C12: haze / fan override scenes as soon as the level is > 0; the smoke button overrides only while firing */
#define LJ_SMOKE_BUTTON_USES_LEVEL    0   /* [SMOKE] UseLevel=0: the Smoke button sends 255, 1 = the Level fader (04 s12 "Smoke Button Use Level") */
#define LJ_SMOKE_CYCLE_MIN_MS      5000   /* 04 s12 / 11 E30: cycle fader 5 s .. 60 min in 32 steps */
#define LJ_SMOKE_CYCLE_MAX_MS   3600000   /* 04 s12 */
#define LJ_SMOKE_CYCLE_STEPS         32   /* 04 s12; the step table is the Smoke Control window's (PROPOSAL: linear in log time, rig) */
#define LJ_SMOKE_DURATION_MAX_MS  60000   /* PROPOSAL: the Duration fader's ceiling (the cycle minus nothing is a continuous fire); rig */
#define LJ_SMOKE_RESET_ON_SMOKE       0   /* [SMOKE] Reset=1 shipped means the preference "Reset cycle on smoke"; the engine restarts the cycle on a button press when set */
#define LJ_SEQINT_UNASSIGNED        255   /* 03 s5.9: a slot without a sequence-intensity fader counts as 100 % = 255 of LJ_GM_DOMAIN */
#define LJ_SEQINT_ON_PROGRAMMED       1   /* 03 s5.9: HTP / LoTP compare programmed scene values x SI, output = the winner's faded value x SI (LORD_ENGINE-17) */
#define LJ_SNAPSHOT_RATE_MIN_HZ       1   /* 03 s4.7: real-time recording 1..20 scenes/s, 2..999 scenes */
#define LJ_SNAPSHOT_RATE_MAX_HZ      20   /* 03 s4.7; E13: the sampler tops out at 20 Hz, so a snapshot is one SNAPSHOT_REQ per sample tick */
#define LJ_SNAPSHOT_SCENES_MAX      999   /* 03 s4.7 */
#define LJ_HTP_MASTERS_DEFAULT        0   /* TGlobalPatchForm M_HTPgroupsaffectedgyGM "Enable HTP group master intensity control" default OFF [11 P8] */

/* Layer ranks (design 3.4, 11 s3.4; 2-4, 6-7 PROPOSAL rig M12-M15). layer_of[ch] carries the rank that won. */
enum lj_layer { LJ_L_DEFAULT = 0, LJ_L_CUE, LJ_L_FLASH, LJ_L_CURRENT, LJ_L_STATICS, LJ_L_BGCUE, LJ_L_GENMACRO, LJ_L_SMOKE, LJ_L_COUNT };
extern const uint8_t LJ_LAYER_ORDER[LJ_L_COUNT];   /* lowest to highest; defined in lj_merge.c */

/* Post stages (design 3.5; names are note 11's stage numbers, the array is the order:
 * S1, S3, S2, S4-S9; 11 s4.2 PROPOSAL rig M16-M20. S3 runs before S2 on purpose:
 * masters scale the un-inverted intensity, then the profile inverts (11 G6
 * UNVERIFIED, rig M21); Example 6 (100-200 inverted, limit on) yields DMX
 * 100/150/200 at GM 100/50/0 % only in this order. */
enum lj_post_stage { LJ_POST_S1_PATCH_SRC = 0, LJ_POST_S2_PROFILE_INVERT, LJ_POST_S3_MASTERS, LJ_POST_S4_BLACKOUT, LJ_POST_S5_HTP,
                     LJ_POST_S6_PATCH_INV_MINMAX, LJ_POST_S7_PANTILT, LJ_POST_S8_OVERWRITE, LJ_POST_S9_FREEZE_OFFLINE, LJ_POST_COUNT };
extern const uint8_t LJ_POST_ORDER[LJ_POST_COUNT];   /* defined in lj_post.c */

/* Sub-master modes (11 G2, 03 s9.1): stored per sub-master in lj_show.submaster_mode[]. */
enum lj_master_mode { LJ_MASTER_PROPORTIONAL = 0 /* v * sub * gm / (255*255) */, LJ_MASTER_LOTP /* v * min(sub, gm) / 255 */,
                      LJ_MASTER_HTP /* v * max(sub, gm) / 255 */, LJ_MASTER_NO_EFFECT /* v * sub / 255 */ };
/* Sequence-intensity modes (03 s5.9, cue control Preferences, all 12 slots): programmed x SI selects, faded x SI outputs (LORD_ENGINE-17). */
enum lj_seq_int_mode { LJ_SEQINT_DEFAULT = 0, LJ_SEQINT_HTP, LJ_SEQINT_LOTP };

#endif /* LJ_ENGINE_ENGINE_RULES_H */
