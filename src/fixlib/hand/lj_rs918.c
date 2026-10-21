/* lj_rs918.c - Martin Roboscan Pro 918 (exe id 24, tag RS918, Martin / Roboscan Pro 918 /
 * Mode 2/4) and 918 SCRL (30, RS918-S, Martin / Roboscan Pro 918 / SCRL, Mode 2/4),
 * transcribed from 15-builtin-profiles/01-mac500-roboscan918.md: Roboscan Pro 918 user
 * manual Rev. C p.23-25 and the martin.com DMX table V2, which carry the same 16 channels
 * and values as the MAC 500 (the only textual differences: ch 1 73-79 "lamp power reduced"
 * is MAC 500 E only, and the ch 10 prism direction labels are swapped) [15/01 s2]. The exe
 * has no *Control_RS918 class: the 918 profiles reuse the Mac500 control classes and
 * TSpecialSettings_RS918S [15/01 s3], so the full window tables live in lj_mac500.c and are
 * referenced here; "Roboscan 918 uses 16 addresses" (CHM fixtureconfiguration.htm). */
#include "fixlib/hand/lj_rs918.h"
#include "fixlib/hand/lj_mac500.h"

#define LJ_R9_PROFILE(id_, tag_, mode_, windows_, gobo1_scrl_, notes_) { \
    .id = id_, .tag = tag_, .manufacturer = "Martin", .name = "Roboscan Pro 918", .mode = mode_, .fixture_id = "", .format_version = 0, \
    .footprint = 16, \
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_EFFECT, \
                       LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED }, \
    .shutter = { 1, 0, "Shutter", 0, 20 }, .strobe = { 1, 0, "Strobe", 50, 72 }, .intensity = { 1, 1, "Intens", 0, 255 }, \
    .color = { { 1, 2, "Color 1", 0, 148 }, { 1, 3, "Color 2", 0, 148 } }, .gobo = { { 1, 6, "Gobo 1", 0, gobo1_scrl_ ? 180 : 89 }, { 1, 4, "Gobo 2", 0, 255 }, { 1, 5, "Index", 0, 255 } }, \
    .iris = { 1, 8, "Iris", 0, 199 }, .focus = { 1, 7, "Focus", 0, 255 }, .effects = { { 1, 9, "Prism", 0, 149 } }, .strobe_off = 20, \
    .blackout = { "", 1, { 1 }, { 0 } }, .lamp_on = { "", 1, { 1 }, { 228 } }, .lamp_off = { "", 1, { 1 }, { 248 } }, .reset = { "", 1, { 1 }, { 208 } }, \
    .palette = { { { { 1, 1, { 0 }, 0xFFFFFF, "" }, { 1, 1, { 16 }, 0x0000FF, "" }, { 1, 1, { 32 }, 0xFF0000, "" }, { 1, 1, { 48 }, 0xFF00FF, "" }, { 1, 1, { 64 }, 0x00FF00, "" }, \
                     { 1, 1, { 80 }, 0xFFFF00, "" }, { 1, 1, { 96 }, 0x8000FF, "Purple 502" }, { 1, 1, { 112 }, 0x0000FF, "Blue 101" }, { 1, 1, { 128 }, 0xFFAAFF, "Pink 312" }, { 1, 1, { 144 }, 0x55FFFF, "Cyan 401" } }, { 0 } }, \
                 { { { 1, 1, { 0 }, 0xFFFFFF, "" }, { 1, 1, { 16 }, 0xFFE0C0, "CTC 3200-4100" }, { 1, 1, { 32 }, 0xD0E0FF, "CTC 3200-5600" }, { 1, 1, { 48 }, 0x0000FF, "" }, { 1, 1, { 64 }, 0x4060FF, "Blue 108" }, \
                     { 1, 1, { 80 }, 0x00C000, "Green 206" }, { 1, 1, { 96 }, 0xFF2000, "Red 308" }, { 1, 1, { 112 }, 0xFFE000, "Yellow 603" }, { 1, 1, { 128 }, 0xFFC080, "CTC 5500-2900" }, { 1, 1, { 144 }, 0xFFE8C8, "CTC 5500-4200" } }, { 0 } }, \
                 { { { 1, 1, { 0 }, 0, "Open" }, { 1, 1, { gobo1_scrl_ ? 20 : 10 }, 0, "Cone" }, { 1, 1, { gobo1_scrl_ ? 40 : 20 }, 0, "Holes" }, { 1, 1, { gobo1_scrl_ ? 60 : 30 }, 0, "Lotus" }, { 1, 1, { gobo1_scrl_ ? 80 : 40 }, 0, "Bricks" }, \
                     { 1, 1, { gobo1_scrl_ ? 100 : 50 }, 0, "Clouds" }, { 1, 1, { gobo1_scrl_ ? 120 : 60 }, 0, "Machine" }, { 1, 1, { gobo1_scrl_ ? 140 : 70 }, 0, "BreakupBambus" }, { 1, 1, { gobo1_scrl_ ? 160 : 80 }, 0, "BreakupThreads" }, { 1, 1, { gobo1_scrl_ ? 180 : 90 }, 0, "Pling" } }, { 0 } }, \
                 { { { 1, 3, { 0, 0 }, 0, "Open" }, { 1, 3, { 56, 236 }, 0, "Triangle" }, { 1, 3, { 76, 216 }, 0, "Thinbar" }, { 1, 3, { 96, 196 }, 0, "Fan" }, { 1, 3, { 116, 176 }, 0, "ThinBars" }, { 1, 3, { 136, 156 }, 0, "Gridball" } }, { 0, 6 } } }, \
    .use_gm = 1, .pt16 = 1, .pan_ofs = 10, .tilt_ofs = 12, .presets = 1, .macros = 1, .follow_spot = 1, \
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, \
    .windows = windows_, .nwindows = LJ_MAC500_WINDOWS, .notes = notes_ }
/* Palettes = the MAC 500 defaults (same wheels per the two DMX tables); the 918 manual's own gobo layout page was not
 * text-extractable, so the 918 default gobos are unverified [15/01 s6]. Gel RGB values are ours. */

const lj_profile lj_hand_rs918 = LJ_R9_PROFILE(24, "RS918", "Mode 2/4", lj_mac500_windows_fix, 0,
    "Fixed gobo wheel without scroll (SPEC/GMOd = FIX; scroll implemented in the 918 from version 1.0). Gobo rotation and gobo index share channel 6. "
    "Fixture settings: PSEt = 2 or 4, PTSP = FAST, SPEC/dLOF = on, SPEC/dRES = on, SPEC/MOdE = NORM, SPEC/GMOd = FIX. 16 DMX addresses.");
const lj_profile lj_hand_rs918_scrl = LJ_R9_PROFILE(30, "RS918-S", "SCRL, Mode 2/4", lj_mac500_windows_scrl, 1,
    "Continuous scroll on the fixed gobo wheel (SPEC/GMOd = SCRL); gobo shake speed follows the Effects Settings channel. "
    "Fixture settings: PSEt = 2 or 4, PTSP = FAST, SPEC/dLOF = on, SPEC/dRES = on, SPEC/MOdE = NORM, SPEC/GMOd = SCRL. 16 DMX addresses.");
