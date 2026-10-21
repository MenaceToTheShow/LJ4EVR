/* lj_mac500.c - Martin MAC 500 (exe id 12, tag MAC500, Martin / MAC 500 / Mode 2/4) and
 * MAC 500 SCRL (31, MAC500-S, Martin / MAC 500 / SCRL, Mode 2/4), transcribed from
 * 15-builtin-profiles/01-mac500-roboscan918.md: Martin MAC 500 user manual P/N 35000016
 * Rev. E p.27-29 and the martin.com DMX table "V5, implemented from software version
 * 1.8", Mode 4 layout (16 channels; LJ allocates 16 for every one of the four profiles
 * and a fixture set to PSEt 2 ignores 15-16 [15/01 s2]). Captions are the exe literals
 * stored with TIntensityControl_Mac500 .. TSpecialSettings_RS918S [15/01 s4-s10, 13 s2.6].
 * Every button carries value = the low end of its Martin range: F47 "exact DMX value per
 * button is UNVERIFIED", the rig edits that one number. No Martin artwork: gobo tiles are
 * ours and unassigned here (LORD_FIXTURES-9; the slot->gobo mapping is unverified [15/01 s6]). */
#include "fixlib/hand/lj_mac500.h"
#include "fixlib/hand/lj_hand.h"

/* ---- Intensity: TIntensityControl_Mac500 (IMG intensitycontrol.gif 480x98) [15/01 s4] ---- */
static const lj_ctl_button lj_m5_shutter[] = { { "Shutter Closed", 0, 0, 0, 19 }, { "Shutter Open", 0, 20, 20, 49 } };   /* closed emits 0, open emits 20: F47 PROPOSAL */
static const lj_ctl_button lj_m5_pulse[]   = { { "Pulse Open", 0, 80, 80, 99 }, { "Pulse Close", 0, 100, 100, 119 } };
static const lj_ctl_button lj_m5_rpulse[]  = { { "Open Fast", 0, 191, 191, 193 }, { "Open Slow", 0, 194, 194, 196 }, { "Close Fast", 0, 197, 197, 199 }, { "Close Slow", 0, 200, 200, 202 } };
static const lj_ctl_button lj_m5_rstrobe[] = { { "Strobe Slow", 0, 168, 168, 187 }, { "Strobe Med", 0, 148, 148, 167 }, { "Strobe Fast", 0, 128, 128, 147 } };
static const lj_ctl_row lj_m5_intensity_rows[] = {
    { LJ_W_FADEBTN,        0, 2, NULL, NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL },                        /* one Snap spans rows 1-2 (ch 1) */
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_m5_shutter, 2, NULL, LJ_STATES_ALL },
    { LJ_W_DRAGBAR,        0, 1, "Strobe", NULL, 50, 72, 1, 0, NULL, 0, NULL, LJ_STATES_ALL },                             /* 50-72 fast->slow: reverse, 100 % = fastest (unverified) */
    { LJ_W_FADEBTN,        1, 1, NULL, NULL, 0, 0, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_DRAGBAR,        1, 1, "Intensity", NULL, 0, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL } };
static const lj_ctl_row lj_m5_intensity_specials[] = {
    { LJ_W_SPECIAL_GROUP, 0, 1, "Intensity Pulse", "Pulse Off", 80, 119, 0, 0, lj_m5_pulse, 2, NULL, LJ_STATES_ALL },
    { LJ_W_SPECIAL_GROUP, 0, 1, "Random Pulse",    NULL, 0, 0, 0, 0, lj_m5_rpulse, 4, NULL, LJ_STATES_ALL },
    { LJ_W_SPECIAL_GROUP, 0, 1, "Random Strobe",   NULL, 0, 0, 0, 0, lj_m5_rstrobe, 3, NULL, LJ_STATES_ALL } };

/* ---- Movement: TPanTilt_16Bit, pan 11/12, tilt 13/14; the grid comes from pt16 / pan_ofs / tilt_ofs ---- */
static const lj_ctl_row lj_m5_movement_rows[] = {
    { LJ_W_FADEBTN, 10, 1, "Pan/Tilt Fade", NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL } };   /* one O/S/F for the pan/tilt pair (ME-22) */

/* ---- Colors: TColorControl_Mac500 (IMG fixturepalette.gif) [15/01 s5] ----
 * W_ROTATION buttons = {CW lo..hi}, {CCW lo..hi}, {Stop value}; the bar maps speed inside the pressed range (LORD_FIXTURES-10). */
static const lj_ctl_button lj_m5_col1_rot[] = { { "Wheel Scroll CW", 2, 185, 185, 215 }, { "Wheel Scroll CCW", 2, 216, 216, 245 }, { "Stop", 2, 0, 0, 0 } };
static const lj_ctl_button lj_m5_col2_rot[] = { { "Wheel Scroll CW", 3, 216, 216, 230 }, { "Wheel Scroll CCW", 3, 231, 231, 245 }, { "Stop", 3, 0, 0, 0 } };   /* ch 4 "216-245 continuous rotation CW/CCW": the split at 230/231 is UNVERIFIED */
static const lj_ctl_button lj_m5_col_rnd[]  = { { "Rnd Slow", 3, 252, 252, 255 }, { "Rnd Med", 3, 249, 249, 251 }, { "Rnd Fast", 3, 246, 246, 248 } };
static const lj_ctl_row lj_m5_colors_rows[] = {
    { LJ_W_FADEBTN,  2, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_SCROLL,   2, 1, "Current Color", "Color Wheel 1", 0, 148, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },   /* solid = 0,16,..,144; split inside the continuous range */
    { LJ_W_ROTATION, 2, 1, "Wheel Scroll Speed", NULL, 185, 245, 0, 0, lj_m5_col1_rot, 3, NULL, LJ_STATES_ALL },
    { LJ_W_PALETTE,  2, 1, "Color 1", NULL, 0, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },                      /* lo = index into lj_profile.palette[] */
    { LJ_W_FADEBTN,  3, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_SCROLL,   3, 1, "Current Color", "Color Wheel 2", 0, 148, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_ROTATION, 3, 1, "Wheel Scroll Speed", NULL, 216, 245, 0, 0, lj_m5_col2_rot, 3, NULL, LJ_STATES_ALL },
    { LJ_W_PALETTE,  3, 1, "Color 2", NULL, 1, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL } };
static const lj_ctl_row lj_m5_colors_specials[] = {
    { LJ_W_SPECIAL_GROUP, 3, 1, "Random", NULL, 246, 255, 0, 0, lj_m5_col_rnd, 3, NULL, LJ_STATES_ALL } };

/* ---- Gobos: TGoboControl_Mac500_2 (FIX) / TGoboControl_Mac500S_2 (SCRL) (IMG goboindex.gif) [15/01 s6] ---- */
static const lj_ctl_button lj_m5_rgobo_rot[]  = { { "Gobo Rotation CW", 5, 3, 3, 127 }, { "Gobo Rotation CCW", 5, 251, 128, 251 }, { "Stop", 5, 0, 0, 2 } };   /* CW slow->fast, CCW fast->slow */
static const lj_ctl_button lj_m5_sgobo_scrl[] = { { "Wheel Scroll CW", 6, 208, 208, 231 }, { "Wheel Scroll CCW", 6, 232, 232, 255 }, { "Stop", 6, 0, 0, 0 } };
#define LJ_M5_ROTATING_ROWS \
    { LJ_W_FADEBTN,  4, 1, NULL, NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL }, \
    { LJ_W_SCROLL,   4, 1, "Rotating Gobo", NULL, 0, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },             /* Index mode writes 56-155, Rotate mode 156-255 (palette 3 Def / Rot) */ \
    { LJ_W_PALETTE,  4, 1, "Gobo 2", NULL, 3, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }, \
    { LJ_W_MODEPAIR, 5, 1, "Index Gobo", "Rotate Gobo", 0, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },          /* shown as Index | Rotate (IMG rotationmode.gif) */ \
    { LJ_W_FADEBTN,  5, 1, NULL, NULL, 0, 0, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL }, \
    { LJ_W_INDEXBAR, 5, 1, "Gobo Index", NULL, 0, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },               /* 127 = default */ \
    { LJ_W_ROTATION, 5, 1, "Gobo Rotation Speed", NULL, 3, 251, 0, 0, lj_m5_rgobo_rot, 3, NULL, LJ_STATES_ALL }
static const lj_ctl_row lj_m5_gobos_fix_rows[] = {
    { LJ_W_FADEBTN, 6, 1, NULL, NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_SCROLL,  6, 1, "Static Gobo", NULL, 0, 89, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },                 /* stepped 0-9 open, 10-19 gobo 1 .. 80-89 gobo 9: solid steps only */
    { LJ_W_PALETTE, 6, 1, "Gobo 1", NULL, 2, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },
    LJ_M5_ROTATING_ROWS };
static const lj_ctl_row lj_m5_gobos_scrl_rows[] = {
    { LJ_W_FADEBTN,     6, 1, NULL, NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_SCROLL,      6, 1, "Static Gobo", NULL, 0, 180, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },            /* continuous 0 open, 20 gobo 1 .. 180 gobo 9, split steps between */
    { LJ_W_WHEELSCROLL, 6, 1, "Wheel Scroll Speed", "Fixed Goboweel Scroll", 208, 255, 0, 0, lj_m5_sgobo_scrl, 3, NULL, LJ_STATES_ALL },   /* exe typo "Goboweel" verbatim [D13] */
    { LJ_W_SHAKE,       6, 1, "Goboshake (Speed = EffectSettings)", NULL, 181, 207, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },   /* 181-183 gobo 9 .. 205-207 gobo 1; speed = ch 16 */
    { LJ_W_PALETTE,     6, 1, "Gobo 1", NULL, 2, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },
    LJ_M5_ROTATING_ROWS };

/* ---- Beam: TBeamControl_Mac500 (IMG beam.gif 206x77) [15/01 s7] ---- */
static const lj_ctl_button lj_m5_iris_pulse[]  = { { "Open", 8, 216, 216, 229 }, { "Close", 8, 230, 230, 243 } };
static const lj_ctl_button lj_m5_iris_rpulse[] = { { "Open Fast", 8, 244, 244, 246 }, { "Open Slow", 8, 247, 247, 249 }, { "Close Fast", 8, 250, 250, 252 }, { "Close Slow", 8, 253, 253, 255 } };
static const lj_ctl_row lj_m5_beam_rows[] = {
    { LJ_W_FADEBTN, 8, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_DRAGBAR, 8, 1, "Iris", NULL, 0, 199, 0, 0, NULL, 0, NULL, LJ_STATES_ALL },      /* open -> close; icons IrisOpen / IrisClosed at the ends */
    { LJ_W_FADEBTN, 7, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_DRAGBAR, 7, 1, "Focus", NULL, 0, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL } };   /* far -> near; icons Near / Far */
static const lj_ctl_row lj_m5_beam_specials[] = {
    { LJ_W_SPECIAL_GROUP, 8, 1, "Iris Pulse",        "Pulse Off", 216, 243, 0, 0, lj_m5_iris_pulse, 2, NULL, LJ_STATES_ALL },
    { LJ_W_SPECIAL_GROUP, 8, 1, "Random Iris Pulse", NULL, 0, 0, 0, 0, lj_m5_iris_rpulse, 4, NULL, LJ_STATES_ALL } };   /* block layout inferred from the MAC 2000 beam2.gif */

/* ---- Effects: TEffectControl_Mac500 (IMG effectcontrol4.gif 246x70, single prism) [15/01 s8] ---- */
static const lj_ctl_button lj_m5_prism[]     = { { "Prism", 9, 80, 80, 89 } };                      /* pressed = prism in, no rotation; released writes lo = 0 (prism off 0-19) */
static const lj_ctl_button lj_m5_prism_rot[] = { { "CW", 9, 20, 20, 79 }, { "CCW", 9, 90, 90, 149 }, { "Stop", 9, 80, 80, 89 } };   /* MAC 500 table; the 918 table swaps CW/CCW (which LJ shows is unverified) */
static const lj_ctl_row lj_m5_effects_rows[] = {
    { LJ_W_FADEBTN,    9, 1, NULL, NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_EFFECT_BTN, 9, 1, "Prism", NULL, 0, 89, 0, 0, lj_m5_prism, 1, NULL, LJ_STATES_ALL },
    { LJ_W_ROTATION,   9, 1, "Speed", NULL, 20, 149, 0, 0, lj_m5_prism_rot, 3, NULL, LJ_STATES_ALL } };   /* macros 1-8 (221-255) are not exposed by this class */

/* ---- Lamp Control: TLampControl_Mac500, variant DOO (IMG lampcontrol.gif 133x100) [15/01 s9, 15/00 s5] ---- */
static const lj_ctl_button lj_m5_lamp[] = { { "Lamp On", 0, 228, 228, 237 }, { "Lamp Off", 0, 248, 248, 255 } };   /* exe palette strings 0xAB830/0xAB868; lj_ctl_lamp draws the bulbs, "Off" = confirm (lj_ctl_common.h) */   /* lamp off: confirmation dialog, hold > 5 s */
static const lj_ctl_row lj_m5_lamp_rows[] = {
    { LJ_W_FADEBTN,   0, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_OFF_SNAP },          /* ME-27: the Lamp window never reaches Fade */
    { LJ_W_LAMP_BTNS, 0, 1, "Lamp On", "Lamp Off", 228, 255, 0, 0, lj_m5_lamp, 2, NULL, LJ_STATES_OFF_SNAP },
    { LJ_W_NOTE,      0, 1, "NOTE !\nLamp Off must be held for at least 5 seconds", NULL, 0, 0, 0, 0, NULL, 0, NULL, LJ_STATES_OFF_SNAP } };

/* ---- Extended: TSpecialSettings_RS918S (IMG extended.gif 277x81 "Roboscan 918-S speed controls") [15/01 s10, 15/00 s4] ----
 * W_RADIOGROUP_FADER: items = the radio captions, buttons = the value each radio writes (the "Speed" radio takes the fader, lo..hi). */
static const char *const lj_m5_pt_items[] = { "Tracking", "Speed", "Normal", "Fast", "B/O while moving", NULL };
static const lj_ctl_button lj_m5_pt_vals[]  = { { "Tracking", 14, 0, 0, 2 }, { "Speed", 14, 3, 3, 245 }, { "Normal", 14, 246, 246, 248 }, { "Fast", 14, 249, 249, 251 }, { "B/O while moving", 14, 252, 252, 255 } };
static const char *const lj_m5_fx_items[] = { "Tracking", "Speed", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
static const lj_ctl_button lj_m5_fx_vals[]  = { { "Tracking", 15, 0, 0, 2 }, { "Speed", 15, 3, 3, 239 }, { "Shortcuts Disabled", 15, 249, 249, 251 }, { "Shortcuts Enabled", 15, 246, 246, 248 }, { "B/O while moving", 15, 252, 252, 255 } };
static const lj_ctl_button lj_m5_reset[]    = { { "Reset", 0, 208, 208, 217 } };
static const lj_ctl_row lj_m5_extended_rows[] = {
    { LJ_W_FADEBTN,          14, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_RADIOGROUP_FADER, 14, 1, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, 1, 0, lj_m5_pt_vals, 5, lj_m5_pt_items, LJ_STATES_ALL },   /* fader shows Slw at the bottom: 245 = slow, reverse */
    { LJ_W_FADEBTN,          15, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_ALL },
    { LJ_W_RADIOGROUP_FADER, 15, 1, "Effects Settings", "Effect Speed", 3, 239, 1, 0, lj_m5_fx_vals, 5, lj_m5_fx_items, LJ_STATES_ALL },
    { LJ_W_FADEBTN,           0, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_OFF_SNAP },   /* reset is stored in the scene behind the warning: Off/Snap only (ME-27) */
    { LJ_W_SPECIAL_GROUP,     0, 1, "Fixture Reset", NULL, 208, 217, 0, 0, lj_m5_reset, 1, NULL, LJ_STATES_OFF_SNAP } };
/* The V5 studio-mode radios (Mode=Normal | Mode=Studio) exist in the exe as another variant; whether these profiles show them is unverified [15/01 s10]: not shown. */

#define LJ_M5_COMMON_WINDOWS \
    { LJ_CTL_INTENSITY, "Intensity",       lj_ctl_menu_std,  lj_m5_intensity_rows, 5, lj_m5_intensity_specials, 3 }, \
    { LJ_CTL_MOVEMENT,  "Movement 16 Bit", lj_ctl_menu_move, lj_m5_movement_rows,  1, NULL, 0 }, \
    { LJ_CTL_COLORS,    "Colors",          lj_ctl_menu_std,  lj_m5_colors_rows,    8, lj_m5_colors_specials, 1 }
#define LJ_M5_TAIL_WINDOWS \
    { LJ_CTL_BEAM,      "Beam",         lj_ctl_menu_std,  lj_m5_beam_rows,     4, lj_m5_beam_specials, 2 }, \
    { LJ_CTL_EFFECTS,   "Effects",      lj_ctl_menu_fx1,  lj_m5_effects_rows,  3, NULL, 0 }, \
    { LJ_CTL_LAMP,      "Lamp Control", lj_ctl_menu_help, lj_m5_lamp_rows,     3, NULL, 0 }, \
    { LJ_CTL_EXTENDED,  "Extended",     NULL,             lj_m5_extended_rows, 6, NULL, 0 }   /* opened by Fixture toolbar button 7 (the Levels icon) [15/01 s3] */
const lj_ctl_window lj_mac500_windows_fix[LJ_MAC500_WINDOWS] = {
    LJ_M5_COMMON_WINDOWS, { LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_m5_gobos_fix_rows, 10, NULL, 0 }, LJ_M5_TAIL_WINDOWS };
const lj_ctl_window lj_mac500_windows_scrl[LJ_MAC500_WINDOWS] = {
    LJ_M5_COMMON_WINDOWS, { LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_m5_gobos_scrl_rows, 12, NULL, 0 }, LJ_M5_TAIL_WINDOWS };

/* ---- Palettes (6.5). Gel names are the exe colour-table facts [15/00 s3]; the RGB values are ours.
 * Color 1 slots 1-6 are RGB blocks and 7-10 the bitmaps Purple 502, Blue 101, Pink 312, Cyan 401
 * (IMG configurefixturepalette.gif); Color 2 names from the DMX table (Blue 104 has no exe bitmap: unverified).
 * Gobo slots: Def = DMX value of the position; rotating gobos carry Rot (code 6) = the continuous-rotation value. */
#define LJ_PAL_RGB(dmx, rgb, bmp) { 1, 0x01, { dmx, 0, 0, 0, 0 }, rgb, bmp }
#define LJ_M5_COLOR1 { { LJ_PAL_RGB(0, 0xFFFFFF, ""), LJ_PAL_RGB(16, 0x0000FF, ""), LJ_PAL_RGB(32, 0xFF0000, ""), LJ_PAL_RGB(48, 0xFF00FF, ""), \
                       LJ_PAL_RGB(64, 0x00FF00, ""), LJ_PAL_RGB(80, 0xFFFF00, ""), LJ_PAL_RGB(96, 0x8000FF, "Purple 502"), LJ_PAL_RGB(112, 0x0000FF, "Blue 101"), \
                       LJ_PAL_RGB(128, 0xFFAAFF, "Pink 312"), LJ_PAL_RGB(144, 0x55FFFF, "Cyan 401") }, { 0, 0, 0, 0, 0 } }
#define LJ_M5_COLOR2 { { LJ_PAL_RGB(0, 0xFFFFFF, ""), LJ_PAL_RGB(16, 0xFFE0C0, "CTC 3200-4100"), LJ_PAL_RGB(32, 0xD0E0FF, "CTC 3200-5600"), LJ_PAL_RGB(48, 0x0000FF, ""), \
                       LJ_PAL_RGB(64, 0x4060FF, "Blue 108"), LJ_PAL_RGB(80, 0x00C000, "Green 206"), LJ_PAL_RGB(96, 0xFF2000, "Red 308"), LJ_PAL_RGB(112, 0xFFE000, "Yellow 603"), \
                       LJ_PAL_RGB(128, 0xFFC080, "CTC 5500-2900"), LJ_PAL_RGB(144, 0xFFE8C8, "CTC 5500-4200") }, { 0, 0, 0, 0, 0 } }
/* Gobo bitmaps = our schematic tiles under the manual's default-layout names (UM-MAC500 p.18: static 1-9 Cone, Dots, Lotus, Bricks,
 * Clouds, Machine, Bamboo, Threads, Pling; rotating 1-5 Triangle, Bar, Fan, Thin bars, Grid ball), spelled as the exe gobo table where
 * it has the name (Holes, BreakupBambus, BreakupThreads, Thinbar, ThinBars, Gridball) [15/00 s3]. PROPOSAL per slot: the slot->gobo
 * assignment LJ shows is unverified [15/01 s6, LORD_FIXTURES-9]. */
#define LJ_M5_GOBO1_FIX  { { LJ_PAL_RGB(0, 0, "Open"), LJ_PAL_RGB(10, 0, "Cone"), LJ_PAL_RGB(20, 0, "Holes"), LJ_PAL_RGB(30, 0, "Lotus"), LJ_PAL_RGB(40, 0, "Bricks"), \
                           LJ_PAL_RGB(50, 0, "Clouds"), LJ_PAL_RGB(60, 0, "Machine"), LJ_PAL_RGB(70, 0, "BreakupBambus"), LJ_PAL_RGB(80, 0, "BreakupThreads"), LJ_PAL_RGB(90, 0, "Pling") }, { 0, 0, 0, 0, 0 } }   /* open, gobo 1..9 (stepped) */
#define LJ_M5_GOBO1_SCRL { { LJ_PAL_RGB(0, 0, "Open"), LJ_PAL_RGB(20, 0, "Cone"), LJ_PAL_RGB(40, 0, "Holes"), LJ_PAL_RGB(60, 0, "Lotus"), LJ_PAL_RGB(80, 0, "Bricks"), \
                           LJ_PAL_RGB(100, 0, "Clouds"), LJ_PAL_RGB(120, 0, "Machine"), LJ_PAL_RGB(140, 0, "BreakupBambus"), LJ_PAL_RGB(160, 0, "BreakupThreads"), LJ_PAL_RGB(180, 0, "Pling") }, { 0, 0, 0, 0, 0 } }   /* open, gobo 1..9 (continuous) */
#define LJ_PAL_ROTB(idx, rot, bmp) { 1, 0x03, { idx, rot, 0, 0, 0 }, 0, bmp }
#define LJ_M5_GOBO2 { { LJ_PAL_ROTB(0, 0, "Open"), LJ_PAL_ROTB(56, 236, "Triangle"), LJ_PAL_ROTB(76, 216, "Thinbar"), LJ_PAL_ROTB(96, 196, "Fan"), LJ_PAL_ROTB(116, 176, "ThinBars"), LJ_PAL_ROTB(136, 156, "Gridball") }, { 0, 6, 0, 0, 0 } }   /* open, rotating gobo 1..5: Def = index, Rot = rotation (ch 5) */

/* ---- The profile. Channel classes per offset: 1 shutter, 2 intensity, 3-4 colour, 5-7 gobo, 8-9 beam, 10 prism,
 * 11-14 pan/tilt 16 bit, 15-16 speed [15/01 s2]. gs2_raw NULL = built-in. */
#define LJ_M5_PROFILE(id_, tag_, name_, mode_, windows_, gobo1_, notes_) { \
    .id = id_, .tag = tag_, .manufacturer = "Martin", .name = name_, .mode = mode_, .fixture_id = "", .format_version = 0, \
    .footprint = 16, \
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_EFFECT, \
                       LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED }, \
    .shutter = { 1, 0, "Shutter", 0, 20 }, .strobe = { 1, 0, "Strobe", 50, 72 }, .intensity = { 1, 1, "Intens", 0, 255 }, \
    .color = { { 1, 2, "Color 1", 0, 148 }, { 1, 3, "Color 2", 0, 148 } }, .gobo = { { 1, 6, "Gobo 1", 0, 89 }, { 1, 4, "Gobo 2", 0, 255 }, { 1, 5, "Index", 0, 255 } }, \
    .iris = { 1, 8, "Iris", 0, 199 }, .focus = { 1, 7, "Focus", 0, 255 }, .effects = { { 1, 9, "Prism", 0, 149 } }, .strobe_off = 20, \
    .blackout = { "", 1, { 1 }, { 0 } }, .lamp_on = { "", 1, { 1 }, { 228 } }, .lamp_off = { "", 1, { 1 }, { 248 } }, .reset = { "", 1, { 1 }, { 208 } }, \
    .palette = { LJ_M5_COLOR1, LJ_M5_COLOR2, gobo1_, LJ_M5_GOBO2 }, \
    .use_gm = 1, .pt16 = 1, .pan_ofs = 10, .tilt_ofs = 12, .presets = 1, .macros = 1, .follow_spot = 1, \
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, \
    .windows = windows_, .nwindows = LJ_MAC500_WINDOWS, .notes = notes_ }
/* default_dmx stays 0: LJ assumes 0 unless the profile defines defaults, and nothing in writing gives the MAC 500 any [15/00 s5]. */

const lj_profile lj_hand_mac500 = LJ_M5_PROFILE(12, "MAC500", "MAC 500", "Mode 2/4", lj_mac500_windows_fix, LJ_M5_GOBO1_FIX,
    "Fixed gobo wheel without scroll (SPEC/GMOd = FIX). Gobo rotation and gobo index share channel 6 and cannot be programmed independently. "
    "Fixture settings: PSEt = 2 or 4, PTSP = FAST, SPEC/FTYP = PRIS, SPEC/dLOF = on, SPEC/dRES = on, SPEC/MOdE = NORM, SPEC/GMOd = FIX. 16 DMX addresses.");
const lj_profile lj_hand_mac500_scrl = LJ_M5_PROFILE(31, "MAC500-S", "MAC 500", "SCRL, Mode 2/4", lj_mac500_windows_scrl, LJ_M5_GOBO1_SCRL,
    "Continuous scroll on the fixed gobo wheel (SPEC/GMOd = SCRL, MAC 500 software 1.7+); gobo shake speed follows the Effects Settings channel. "
    "Fixture settings: PSEt = 2 or 4, PTSP = FAST, SPEC/FTYP = PRIS, SPEC/dLOF = on, SPEC/dRES = on, SPEC/MOdE = NORM, SPEC/GMOd = SCRL. 16 DMX addresses.");
