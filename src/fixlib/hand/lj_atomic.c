/* lj_atomic.c - Martin Atomic 3000, transcribed from 15-builtin-profiles/06-atomic-3000.md.
 *   80  ATOMIC   Martin / Atomic 3000 / 4 Channel Mode (Atomic.Rtf): UM-Atomic3000DMX Rev G p.23-24 "DMX protocols", 4-channel mode
 *                (QLC+ Martin-Atomic-3000.qxf Mode 3 and OFL atomic-3000.json agree).
 *   114 ATOMIC-C Martin / Atomic 3000 Color (4 Ch+Scroller) (Atomicc1.rtf): the strobe on 1-4 plus the Atomic Colors scroller "physically
 *                addressed at atomic address+4" (NOTES): 5 colour, 6 fan.
 *   115 A-COLOR  Martin / Atomic 3000 Color Scroller (2 Ch) (Atomicc2.rtf): the scroller alone, 1 colour, 2 fan.
 * The scroller protocol is QLC+ "Mode 3 with Colour and Fan" (Martin's Atomic Colors manual not fetched): values unverified [15/06 s3].
 * Captions are the exe literals of TIntensityControl_Atomic, TColorControl_AtomicC1 / _AtomicC2 [13 s2.6]; TSpecialSettings_AtomicC1 / _C2
 * carry no captions of their own (the "Fan" bar caption is ours: PROPOSAL). No Lamp, Gobo, Beam, Effects or Movement window [15/06 s3].
 * Every button value = the low end of its range (F47). */
#include "fixlib/hand/lj_atomic.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Intensity: TIntensityControl_Atomic (p.23-24) ----
 * ch 1 flash intensity 0-5 blackout, 6-255 min -> max; ch 2 duration 0-255 = 0-650 ms (50 Hz) / 0-530 ms (60 Hz); ch 3 rate 0-5 no flash
 * (single flash with ch 1), 6-255 = 0.5-25 Hz / 0.6-30 Hz ("cross-fixture type compatible with 'strobe' controls", NOTES); ch 4 effects
 * (Mode DIP 4 on) 0-5 none, 6-42 ramp up, 43-85 ramp down, 86-128 ramp up-down, 129-171 random, 172-214 lightning, 215-255 spikes.
 * Arrangement unverified (no screenshot): the caption set is the exe's. */
static const lj_ctl_button lj_at_fx[] = { { "RampUp", 3, 6, 6, 42 }, { "Ramp Down", 3, 43, 43, 85 }, { "Ramp Up&Down", 3, 86, 86, 128 },
                                          { "Random", 3, 129, 129, 171 }, { "Lightning", 3, 172, 172, 214 }, { "Spikes", 3, 215, 215, 255 } };
static const lj_ctl_row lj_at_intensity_rows[] = {
    LJ_FB(0, 1, LJ_HAND_FADE), LJ_BAR(0, "Intensity", 0, 255, 0),
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_BAR(2, "Rate", 0, 255, 0),               /* slow -> fast: not reversed */
    LJ_FB(1, 1, LJ_HAND_SNAP), LJ_BAR(1, "Duration", 0, 255, 0),
    LJ_FB(3, 1, LJ_HAND_SNAP) };                                           /* the effect channel's O/S/F; its buttons are the special group */
static const lj_ctl_row lj_at_intensity_specials[] = { LJ_SPEC(3, "Effects", NULL, 6, 255, lj_at_fx, 6) };   /* group label ours: PROPOSAL */

/* ---- Colors: TColorControl_AtomicC1 / _AtomicC2 "Scroller position" + the factory gel string ("Color palette is for factory default
 * gel string", NOTES). QLC colour: 0 White, 26 Straw, 51 Pale Amber Gold, 77 Orange, 105 Red, 130 Broadway pink, 156 Light lavender,
 * 181 Aquamarine, 207 Green blue, 230 Light green, 255 Blue grass (11 frames). Fan: 0-61 level 4, 62-127 level 3, 128-190 level 2,
 * 191-255 level 1 ("Fan Control: Color scroller fan only", NOTES). */
#define LJ_AT_SCROLL(c) LJ_FB(c, 1, LJ_HAND_FADE), LJ_BAR(c, "Scroller position", 0, 255, 0), LJ_PALR(c, "Color 1", 0)
#define LJ_AT_FAN(c)    LJ_FB(c, 1, LJ_HAND_SNAP), LJ_BAR(c, "Fan", 0, 255, 1)   /* level 4 at 0: reverse so the bar fills toward level 1 (inferred) */
static const lj_ctl_row lj_ac_colors_rows[]  = { LJ_AT_SCROLL(4) };
static const lj_ctl_row lj_ac_ext_rows[]     = { LJ_AT_FAN(5) };
static const lj_ctl_row lj_as_colors_rows[]  = { LJ_AT_SCROLL(0) };
static const lj_ctl_row lj_as_ext_rows[]     = { LJ_AT_FAN(1) };

static const lj_ctl_window lj_at_windows[] = { LJ_WIN(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_at_intensity_rows, lj_at_intensity_specials) };
static const lj_ctl_window lj_ac_windows[] = { LJ_WIN(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_at_intensity_rows, lj_at_intensity_specials),
                                               LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_ac_colors_rows), LJ_EXT_WIN(lj_ac_ext_rows) };
static const lj_ctl_window lj_as_windows[] = { LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_as_colors_rows), LJ_EXT_WIN(lj_as_ext_rows) };

/* RGB ours (art/gels.txt names); no gel bitmaps in the exe table for this string: bitmap names left empty */
#define LJ_AT_GELS { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(26, 0xFFE8A0, ""), LJ_PE(51, 0xFFC860, ""), LJ_PE(77, 0xFF8000, ""), LJ_PE(105, 0xFF0000, ""), \
                       LJ_PE(130, 0xFF60A0, ""), LJ_PE(156, 0xC0A0FF, ""), LJ_PE(181, 0x40E0C0, ""), LJ_PE(207, 0x00A0A0, ""), LJ_PE(230, 0x80FF80, ""), \
                       LJ_PE(255, 0x40C040, "") }, LJ_PAL_DEF }

/* Blackout: flash intensity 0 (ch 1; inferred, the only way). Grand Master scales ch 1. */
#define LJ_AT_COMMON .manufacturer = "Martin", .fixture_id = "", .intensity = { 1, 0, "Intens", 0, 255 }, .strobe = { 1, 2, "Rate", 6, 255 }, .strobe_off = 0, \
    .effects = { { 1, 3, "Effects", 0, 255 } }, .use_gm = 1, .blackout = { "", 1, { 1 }, { 0 } }
#define LJ_AT_CLASSES LJ_CH_INTENSITY, LJ_CH_SPECIAL, LJ_CH_STROBE, LJ_CH_EFFECT   /* 1 intensity, 2 duration, 3 rate, 4 effects */
#define LJ_AT_RATE_NOTE "Rate control: cross-fixture type compatible with strobe controls, 0.5 - 25 Hz at 50 Hz mains, 0.6 - 30 Hz at 60 Hz mains. " \
    "Duration 0 - 650 ms at 50 Hz mains, 0 - 530 ms at 60 Hz mains. Effects need Mode DIP switch 4 on. "

static const lj_profile lj_at_atomic = { .id = 80, .footprint = 4, .tag = "ATOMIC", .name = "Atomic 3000", .mode = "4 Channel Mode", LJ_AT_COMMON,
    .channel_class = { LJ_AT_CLASSES }, .level_invisible = { 1, 1, 1, 1 }, .windows = lj_at_windows, .nwindows = LJ_NROWS(lj_at_windows),
    .notes = LJ_AT_RATE_NOTE "4-channel DMX mode, 4 DMX addresses." };
static const lj_profile lj_at_atomic_c = { .id = 114, .footprint = 6, .tag = "ATOMIC-C", .name = "Atomic 3000 Color (4 Ch+Scroller)", .mode = "", LJ_AT_COMMON,
    .channel_class = { LJ_AT_CLASSES, LJ_CH_COLOR, LJ_CH_SPECIAL }, .color = { { 1, 4, "Color 1", 0, 255 } }, .palette = { LJ_AT_GELS },
    .level_invisible = { 1, 1, 1, 1, 1, 1 }, .windows = lj_ac_windows, .nwindows = LJ_NROWS(lj_ac_windows),
    .notes = "Two physical units in one profile: address the Color Scroller at the Atomic address + 4. The colour palette is the factory default gel string. "
             LJ_AT_RATE_NOTE "Fan control: the colour scroller fan only. 6 DMX addresses." };
static const lj_profile lj_at_scroller = { .id = 115, .footprint = 2, .tag = "A-COLOR", .name = "Atomic 3000 Color Scroller (2 Ch)", .mode = "", .manufacturer = "Martin",
    .fixture_id = "", .channel_class = { LJ_CH_COLOR, LJ_CH_SPECIAL }, .color = { { 1, 0, "Color 1", 0, 255 } }, .palette = { LJ_AT_GELS },
    .level_invisible = { 1, 1 }, .windows = lj_as_windows, .nwindows = LJ_NROWS(lj_as_windows),
    .notes = "For the Atomic 3000 Color Scroller unit only. The colour palette is the factory default gel string. 2 DMX addresses." };

const lj_profile *const lj_hand_atomics[LJ_ATOMIC_COUNT] = { &lj_at_atomic, &lj_at_atomic_c, &lj_at_scroller };
