/* lj_roboscan.c - Martin Roboscans, transcribed from 15-builtin-profiles/05-roboscan-518-812-218.md.
 *   3  518  Martin / Roboscan 518 / Mode 3, 58 518 Martin / Roboscan 518 / Mode 2: martin.com DMX table "RoboScan Pro 518 DMX Protocol
 *          Version 4, implemented from EPROM version 6.4" and UM-RoboScanPro518 Rev D p.19-21: Mode 3 = 9 ch, 16-bit pan/tilt tracking
 *          (PL432 jumper 4-5); Mode 2 = 9 ch, 8-bit pan/tilt + two speed channels (jumper 5-6).
 *   1  812  Martin / Roboscan 812 / Mode 1, 52 812 Martin / Roboscan 812 / Mode 2: martin.com "812 DMX Protocol V1" and UM-RoboScan812
 *          Rev C p.17-18: Mode 1 = 5 ch, Mode 2 = 7 ch (+ movement and colour/gobo speed).
 *   2  218  Martin / Roboscan 218-II / Mode 3, 57 218 Martin / Roboscan 218-II / Mode 2: QLC+ Martin-Roboscan-218.qxf (8 ch each); no Martin
 *          manual fetched: values unverified.
 *   41 218  Martin / Roboscan 218 / via DMX Interface, 36 805 Martin / Roboscan 805 / via DMX Interface, 33 1220IIR Martin / Roboscan 1220IIR /
 *          via DMX Interface: the channel map is the Martin DMX Interface's, whose manual was not fetched [15/05 s5].
 * Captions are the exe literals of TIntensityControl_518 .. TSpecialSettings_218_2, TLampControl_DO / _DOO / _DOOF [13 s2.6, 15/00 s4-s5].
 * Every value = the low end of its range (F47). */
#include "fixlib/hand/lj_roboscan.h"
#include "fixlib/hand/lj_martin.h"

/* ================= Roboscan 518 ================= */
/* Ch 1: 0-5 default, 6-10 fan low, 11-138 strobe fast->slow, 139-170 stand-alone auto, 171-202 stand-alone music, 203-235 reset, 236-252 lamp on,
 * 253-255 lamp off (colour wheel at UV pass and effects wheel at CTC, hold 5 s). Ch 2: 0-14 light off, 15-239 0 -> 100 %, 240-255 light on.
 * "On the Roboscan 518 it is not possible to program a strobe AND a lamp command into the same scene" (CHM offsnapfade.htm): one channel, one state. */
static const lj_ctl_row lj_r5_intensity_rows[] = {     /* TIntensityControl_518: no shutter button (light off is ch 2) */
    LJ_FB(0, 1, LJ_HAND_SNAP), LJ_BAR(0, "Strobe", 11, 138, 1), LJ_FB(1, 1, LJ_HAND_FADE), LJ_BAR(1, "Intensity", 15, 239, 0) };
/* Ch 3 colours (4 DMX steps): 0 white, 8 pink, 16 magenta, 24 primary red, 32 flame red, 40 dark orange, 48 orange, 56 yellow, 64 light green,
 * 72 fern green, 80 turquoise, 88 cyan, 96 light blue, 104 dark lavender, 112 dark blue, 120 UV pass, 128 multi colour 1, 136 multi colour 2;
 * 140-197 CW fast->slow; 198-255 CCW slow->fast. */
static const lj_ctl_button lj_r5_colrot[] = { { "Wheel Scroll CW", 2, 140, 140, 197 }, { "Wheel Scroll CCW", 2, 198, 198, 255 }, { "Stop", 2, 0, 0, 0 } };
static const lj_ctl_row lj_r5_colors_rows[] = {        /* TColorControl_518 */
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Color", NULL, 0, 136), LJ_ROT(2, "Wheel Scroll Speed", 140, 255, lj_r5_colrot), LJ_PALR(2, "Color 1", 0) };
/* Ch 4 rotating gobos: 0 open; gobo n = 24 values CCW fast->slow, 3 static, 24 CW slow->fast (1-24 / 25-27 / 28-51 gobo 1 .. 205-228 / 229-231 /
 * 232-255 gobo 5). Selection and rotation share the channel: the palette slot carries Def = static, Rot = slowest CW (per-gobo speed bar: W4 sweep). */
static const lj_ctl_row lj_r5_gobos_rows[] = {         /* TGoboControl_518: Current Gobo, Rotating Gobo */
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Rotating Gobo", "Current Gobo", 0, 229), LJ_PALR(3, "Gobo 1", 2) };
/* Ch 5 effects wheel: 0-63 open, 64-127 CTC 5500-3400 K, 128-191 frost, 192-255 3-facet prism (IMG effectcontrol3.gif: palette of 4). */
static const lj_ctl_row lj_r5_fx_rows[] = { LJ_FB(4, 1, LJ_HAND_SNAP), LJ_SCRL(4, "Current Effect", NULL, 0, 192), LJ_PALR(4, "Effects", 4) };
static const lj_ctl_button lj_r5_lamp[] = { { "Lamp On", 0, 236, 236, 252 }, { "Lamp Off", 0, 253, 253, 255 }, { "Fan Lo", 0, 6, 6, 10 } };   /* TLampControl_DOOF */
static const lj_ctl_row lj_r5_lamp_rows[] = { LJ_FB_OS(0), LJ_LAMPB(0, "Lamp On", "Lamp Off", 6, 255, lj_r5_lamp, 3), LJ_LAMP_NOTE(0) };
static const lj_ctl_button lj_r5_sa[]    = { { "Sound", 0, 171, 171, 202 }, { "Auto", 0, 139, 139, 170 } };
static const lj_ctl_button lj_r5_reset[] = { { "Reset", 0, 203, 203, 235 } };
#define LJ_R5_EXT_COMMON LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 139, 202, lj_r5_sa, 2), LJ_SPEC_OS(0, "Fixture Reset", 203, 235, lj_r5_reset, 1)
/* Mode 2 ch 8: 0 tracking, 1-251 fast->slow, 252-255 blackout while moving; ch 9 colour / dimmer speed, same ranges (252-255 = B/O while colour moves, fast dimmer). */
static const char *const lj_r5_speed_items[] = { "Tracking", "Speed", "B/O while moving", NULL };
static const lj_ctl_button lj_r5_pt[]  = { { "Tracking", 7, 0, 0, 0 }, { "Speed", 7, 1, 1, 251 }, { "B/O while moving", 7, 252, 252, 255 } };
static const lj_ctl_button lj_r5_col[] = { { "Tracking", 8, 0, 0, 0 }, { "Speed", 8, 1, 1, 251 }, { "B/O while moving", 8, 252, 252, 255 } };
static const lj_ctl_row lj_r5_ext3_rows[] = { LJ_R5_EXT_COMMON };          /* TSpecialSettings_518 */
static const lj_ctl_row lj_r5_ext2_rows[] = {                               /* TSpecialSettings_518_2 */
    LJ_FB(7, 1, LJ_HAND_OFF), LJ_RADIO(7, "Pan/Tilt Settings", "Pan/Tilt Speed", 1, 251, lj_r5_pt, 3, lj_r5_speed_items),
    LJ_FB(8, 1, LJ_HAND_OFF), LJ_RADIO(8, "Col Speed", "Dimmer Speed", 1, 251, lj_r5_col, 3, lj_r5_speed_items),   /* group captions: exe strings, pairing inferred */
    LJ_R5_EXT_COMMON };
#define LJ_R5_WINDOWS(name, move, ext) static const lj_ctl_window name[] = { LJ_WIN0(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_r5_intensity_rows), move, \
    LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_r5_colors_rows), LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_r5_gobos_rows), \
    LJ_WIN0(LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_r5_fx_rows), LJ_WIN0(LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lj_r5_lamp_rows), LJ_EXT_WIN(ext) }
LJ_R5_WINDOWS(lj_r5_windows3, LJ_MT_MOVE16_WIN(5), lj_r5_ext3_rows);
LJ_R5_WINDOWS(lj_r5_windows2, LJ_MT_MOVE8_WIN(5), lj_r5_ext2_rows);
#define LJ_R5_COLOR1 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(8, 0xFFA0C0, ""), LJ_PE(16, 0xFF00C0, ""), LJ_PE(24, 0xFF0000, "Primary Red"), LJ_PE(32, 0xFF3000, ""), LJ_PE(40, 0xFF6000, ""), \
                         LJ_PE(48, 0xFF9000, ""), LJ_PE(56, 0xFFE000, ""), LJ_PE(64, 0x80FF80, ""), LJ_PE(72, 0x40A040, ""), LJ_PE(80, 0x00C0A0, ""), LJ_PE(88, 0x00FFFF, ""), \
                         LJ_PE(96, 0x80C0FF, ""), LJ_PE(104, 0x8060C0, ""), LJ_PE(112, 0x0000A0, ""), LJ_PE(120, 0x6000FF, "UVPass"), LJ_PE(128, 0, "518 Multi1"), LJ_PE(136, 0, "518 Multi2") }, LJ_PAL_DEF }
#define LJ_R5_GOBO1  { { LJ_PE_ROT(0, 0, "Open"), LJ_PE_ROT(25, 28, ""), LJ_PE_ROT(76, 79, ""), LJ_PE_ROT(127, 130, ""), LJ_PE_ROT(178, 181, ""), LJ_PE_ROT(229, 232, "") }, LJ_PAL_DEFROT }   /* gobo names not in the extracted manual */
#define LJ_R5_FX1    { { LJ_PE(0, 0, "Open"), LJ_PE(64, 0, "CTC 5500-3400"), LJ_PE(128, 0, "Frost"), LJ_PE(192, 0, "3Facet") }, LJ_PAL_DEF }   /* the exe gobo-table names of this wheel [15/05 s2] */
#define LJ_R5_COMMON .tag = "518", .manufacturer = "Martin", .name = "Roboscan 518", .fixture_id = "", \
    .strobe = { 1, 0, "Strobe", 11, 138 }, .intensity = { 1, 1, "Intens", 15, 239 }, .strobe_off = 0, .use_gm = 1, \
    .blackout = { "", 1, { 0, 1 }, { 0, 0 } }, .lamp_on = { "", 1, { 1 }, { 236 } }, .lamp_off = { "", 1, { 1 }, { 253 } }, .reset = { "", 1, { 1 }, { 203 } }, \
    .color = { { 1, 2, "Color 1", 0, 136 } }, .gobo = { { 1, 3, "Gobo", 0, 229 } }, .effects = { { 1, 4, "Effects", 0, 192 } }, \
    .palette = { LJ_R5_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_R5_GOBO1, { { { 0 } }, LJ_PAL_DEF }, LJ_R5_FX1 }, .pan_ofs = 5, .presets = 1, .macros = 1, .follow_spot = 1, \
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }   /* blackout = intensity light off (ch 2, inferred: the 518 has no shutter) */

static const lj_profile lj_r5_m3 = { .id = 3, .footprint = 9, .mode = "Mode 3", LJ_R5_COMMON,
    /* 1 strobe / stand-alone / reset / lamp, 2 intensity, 3 colour, 4 gobo, 5 effect, 6-9 pan MSB, LSB, tilt MSB, LSB */
    .channel_class = { LJ_CH_STROBE, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT },
    .pt16 = 1, .tilt_ofs = 7, .windows = lj_r5_windows3, .nwindows = LJ_NROWS(lj_r5_windows3),
    .notes = "Personality Mode 3, 16 bit pan/tilt tracking, PL432 jumper on pins 4 and 5. Fixture stand-alone modes use the shutter channel (special). 9 DMX addresses." };
static const lj_profile lj_r5_m2 = { .id = 58, .footprint = 9, .mode = "Mode 2", LJ_R5_COMMON,
    /* 1-5 as Mode 3, 6 pan, 7 tilt, 8 pan/tilt speed, 9 colour / dimmer speed */
    .channel_class = { LJ_CH_STROBE, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pt8 = 1, .tilt_ofs = 6, .windows = lj_r5_windows2, .nwindows = LJ_NROWS(lj_r5_windows2),
    .notes = "Mode 2, 8 bit pan/tilt tracking / speed, PL432 jumper on pins 5 and 6. Fixture stand-alone modes use the shutter channel (special). 9 DMX addresses." };

/* ================= Roboscan 812 ================= */
/* Ch 1: 0-10 default, 11-74 gobo strobe fast->slow, 75-138 colour strobe fast->slow, 139-170 stand-alone auto, 171-202 music, 203-235 reset,
 * 236-255 lamp on (lamp off "is not implemented in the DMX protocol", UM-812 Rev C). Ch 2 colours: 0-15 black -> white, continuous to colour 1 at 30
 * .. colour 11 at 180 (15 each), 180-239 stepped, 240-255 black. Ch 3 gobos: same layout, 0 closed, 15 open, gobo 1-11 at 30..180, 240-255 closed.
 * No intensity channel: blackout and Grand Master 0 select the blackout gobo (CHM blackoutrestore.htm, masterintensitycontrol.htm). */
static const lj_ctl_row lj_r8_colors_rows[] = {        /* TColorControl_812 + the "Color Strobe" bar (TColorControl_NoFunction strings) */
    LJ_FB(1, 1, LJ_HAND_SNAP), LJ_SCRL(1, "Current Color", NULL, 0, 180), LJ_PALR(1, "Color 1", 0), LJ_FB(0, 1, LJ_HAND_SNAP), LJ_BAR(0, "Color Strobe", 75, 138, 1) };
static const lj_ctl_row lj_r8_gobos_rows[] = {         /* TGoboControl_812 + "Gobo Strobe" */
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Gobo", NULL, 0, 180), LJ_PALR(2, "Gobo 1", 2), LJ_FB(0, 1, LJ_HAND_SNAP), LJ_BAR(0, "Gobo Strobe", 11, 74, 1) };
static const lj_ctl_button lj_r8_lamp[] = { { "Strike Lamp", 0, 236, 236, 255 } };   /* TLampControl_DO: Off/Snap + Strike Lamp, no lamp off */
static const lj_ctl_row lj_r8_lamp_rows[] = { LJ_FB_OS(0), LJ_LAMPB(0, "Strike Lamp", NULL, 236, 255, lj_r8_lamp, 1) };
/* Mode 2 ch 6 movement speed, ch 7 colour / gobo speed: 0 tracking, 1-255 fast->slow. */
static const char *const lj_r8_speed_items[] = { "Tracking", "Speed", NULL };
static const lj_ctl_button lj_r8_pt[] = { { "Tracking", 5, 0, 0, 0 }, { "Speed", 5, 1, 1, 255 } };
static const lj_ctl_button lj_r8_cg[] = { { "Tracking", 6, 0, 0, 0 }, { "Speed", 6, 1, 1, 255 } };
static const lj_ctl_row lj_r8_ext1_rows[] = { LJ_R5_EXT_COMMON };          /* TSpecialSettings_812: the same ch 1 stand-alone / reset ranges as the 518 */
static const lj_ctl_row lj_r8_ext2_rows[] = {                               /* TSpecialSettings_812_2 */
    LJ_FB(5, 1, LJ_HAND_OFF), LJ_RADIO(5, "Pan/Tilt Settings", "P/T Speed", 1, 255, lj_r8_pt, 2, lj_r8_speed_items),
    LJ_FB(6, 1, LJ_HAND_OFF), LJ_RADIO(6, "C/G Speed", "Color/Gobo Speed", 1, 255, lj_r8_cg, 2, lj_r8_speed_items),
    LJ_R5_EXT_COMMON };
#define LJ_R8_WINDOWS(name, ext) static const lj_ctl_window name[] = { LJ_MT_MOVE8_WIN(3), LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_r8_colors_rows), \
    LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_r8_gobos_rows), LJ_WIN0(LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lj_r8_lamp_rows), LJ_EXT_WIN(ext) }
LJ_R8_WINDOWS(lj_r8_windows1, lj_r8_ext1_rows);
LJ_R8_WINDOWS(lj_r8_windows2, lj_r8_ext2_rows);
/* "13 positions on the color wheel: 11 with saturated dichroic colors, open white, and blackout"; "11 gobos on the gobo wheel plus open and blackout"
 * (UM-812 Rev C, Color Wheel / Gobos). The 11 colours and gobos are unnamed in Martin's tables: slot bitmaps unassigned. */
#define LJ_R8_SLOTS(c0, b0, c1, b1) LJ_PE(0, c0, b0), LJ_PE(15, c1, b1), LJ_PE(30, 0, ""), LJ_PE(45, 0, ""), LJ_PE(60, 0, ""), LJ_PE(75, 0, ""), LJ_PE(90, 0, ""), \
                                    LJ_PE(105, 0, ""), LJ_PE(120, 0, ""), LJ_PE(135, 0, ""), LJ_PE(150, 0, ""), LJ_PE(165, 0, ""), LJ_PE(180, 0, "")
#define LJ_R8_COLOR1 { { LJ_R8_SLOTS(0x000000, "Black", 0xFFFFFF, "White") }, LJ_PAL_DEF }
#define LJ_R8_GOBO1  { { LJ_R8_SLOTS(0, "Blackout", 0, "Open") }, LJ_PAL_DEF }
#define LJ_R8_COMMON .tag = "812", .manufacturer = "Martin", .name = "Roboscan 812", .fixture_id = "", \
    .blackout = { "", 1, { 0, 0, 1 }, { 0, 0, 255 } }, .bo_at_gm0 = 1, .lamp_on = { "", 1, { 1 }, { 236 } }, .reset = { "", 1, { 1 }, { 203 } }, \
    .color = { { 1, 1, "Color 1", 0, 180 } }, .gobo = { { 1, 2, "Gobo", 0, 180 } }, .palette = { LJ_R8_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_R8_GOBO1 }, \
    .pt8 = 1, .pan_ofs = 3, .tilt_ofs = 4, .presets = 1, .macros = 1, .follow_spot = 1
static const lj_profile lj_r8_m1 = { .id = 1, .footprint = 5, .mode = "Mode 1", LJ_R8_COMMON,
    /* 1 strobe / stand-alone / reset / lamp on, 2 colour, 3 gobo, 4 pan, 5 tilt */
    .channel_class = { LJ_CH_STROBE, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_TILT }, .level_invisible = { 1, 1, 1, 1, 1 },
    .windows = lj_r8_windows1, .nwindows = LJ_NROWS(lj_r8_windows1),
    .notes = "Mode 1: PL11 no jumper on pins 4 and 5. The fixture uses the gobo wheel for blackout. 5 DMX addresses." };
static const lj_profile lj_r8_m2 = { .id = 52, .footprint = 7, .mode = "Mode 2", LJ_R8_COMMON,
    /* 1-5 as Mode 1, 6 movement speed, 7 colour / gobo speed */
    .channel_class = { LJ_CH_STROBE, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED }, .level_invisible = { 1, 1, 1, 1, 1, 1, 1 },
    .windows = lj_r8_windows2, .nwindows = LJ_NROWS(lj_r8_windows2),
    .notes = "Mode 2 allows access to the speed channels: PL11 jumper on pins 4 and 5. The fixture uses the gobo wheel for blackout. 7 DMX addresses." };

/* ================= Roboscan 218-II (QLC+ data) ================= */
/* Ch 1: 0-5 closed, 6-80 open, 81-208 strobe fast->slow, 209-230 stand-alone auto, 231-252 stand-alone music, 253-255 reset.
 * Ch 3 colours / ch 4 gobos: 17 continuous steps of 11 from 0 (white / open) to 187, stepped back 187-238, 239-255 white / open. */
static const lj_ctl_button lj_r2_shutter[] = { { "Shutter Closed", 0, 0, 0, 5 }, { "Shutter Open", 0, 6, 6, 80 } };
static const lj_ctl_row lj_r2_intensity_rows[] = {     /* TIntensityControl_218 */
    LJ_FB(0, 2, LJ_HAND_SNAP), { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 80, 0, 0, lj_r2_shutter, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 81, 208, 1), LJ_FB(1, 1, LJ_HAND_FADE), LJ_BAR(1, "Intensity", 0, 255, 0) };
static const lj_ctl_row lj_r2_colors_rows[] = { LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Color", NULL, 0, 187), LJ_PALR(2, "Color 1", 0) };   /* TColorControl_218 */
static const lj_ctl_row lj_r2_gobos_rows[]  = { LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Current Gobo", NULL, 0, 187), LJ_PALR(3, "Gobo 1", 2) };     /* TGoboControl_218 */
static const lj_ctl_button lj_r2_sa[]    = { { "Sound", 0, 231, 231, 252 }, { "Auto", 0, 209, 209, 230 } };
static const lj_ctl_button lj_r2_reset[] = { { "Reset", 0, 253, 253, 255 } };
#define LJ_R2_EXT_COMMON LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 209, 252, lj_r2_sa, 2), LJ_SPEC_OS(0, "Fixture Reset", 253, 255, lj_r2_reset, 1)
/* Mode 2 ch 7 movement speed: 0 tracking, 1-251 fast->slow, 252-255 blackout while moving; ch 8 colour / gobo / dimmer speed 0-251, 252-255 blackout. */
static const char *const lj_r2_cg_items[] = { "Speed", "B/O while moving", NULL };
static const lj_ctl_button lj_r2_pt[] = { { "Tracking", 6, 0, 0, 0 }, { "Speed", 6, 1, 1, 251 }, { "B/O while moving", 6, 252, 252, 255 } };
static const lj_ctl_button lj_r2_cg[] = { { "Speed", 7, 0, 0, 251 }, { "B/O while moving", 7, 252, 252, 255 } };
static const lj_ctl_row lj_r2_ext3_rows[] = { LJ_R2_EXT_COMMON };          /* TSpecialSettings_218 */
static const lj_ctl_row lj_r2_ext2_rows[] = {                               /* TSpecialSettings_218_2 */
    LJ_FB(6, 1, LJ_HAND_OFF), LJ_RADIO(6, "Pan/Tilt Settings", "P/T Speed", 1, 251, lj_r2_pt, 3, lj_r5_speed_items),
    LJ_FB(7, 1, LJ_HAND_OFF), LJ_RADIO(7, "C/G Speed", "Color/Gobo Speed", 0, 251, lj_r2_cg, 2, lj_r2_cg_items),
    LJ_R2_EXT_COMMON };
#define LJ_R2_WINDOWS(name, move, ext) static const lj_ctl_window name[] = { LJ_WIN0(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_r2_intensity_rows), move, \
    LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_r2_colors_rows), LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_r2_gobos_rows), LJ_EXT_WIN(ext) }
LJ_R2_WINDOWS(lj_r2_windows3, LJ_MT_MOVE16_WIN(4), lj_r2_ext3_rows);
LJ_R2_WINDOWS(lj_r2_windows2, LJ_MT_MOVE8_WIN(4), lj_r2_ext2_rows);
/* Slot names (QLC): colours spelled as the exe colour table where it has them (Primary Red, 218 Multi1, 218 Multi2); gobos as the exe gobo table
 * (Pin, V-Bar, H-Bar, Cross, Arrow, Triangls, Holes, Bells, Cone, Cones, Phone, ThinBars, Window, Turbine). */
#define LJ_R2_COLOR1 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(11, 0xFF3000, ""), LJ_PE(22, 0x80C0FF, ""), LJ_PE(33, 0x40A040, ""), LJ_PE(44, 0xFFE000, ""), LJ_PE(55, 0x00C000, ""), \
                         LJ_PE(66, 0xC080C0, ""), LJ_PE(77, 0x0000A0, ""), LJ_PE(88, 0x00C0FF, ""), LJ_PE(99, 0xFF0000, "Primary Red"), LJ_PE(110, 0xFFA040, ""), LJ_PE(121, 0x80FF80, ""), \
                         LJ_PE(132, 0xFFB000, ""), LJ_PE(143, 0xFFA0C0, ""), LJ_PE(154, 0x8060C0, ""), LJ_PE(165, 0xFF6000, ""), LJ_PE(176, 0, "218 Multi1"), LJ_PE(187, 0, "218 Multi2") }, LJ_PAL_DEF }
#define LJ_R2_GOBO1  { { LJ_PE(0, 0, "Open"), LJ_PE(11, 0, "Half"), LJ_PE(22, 0, "Dot"), LJ_PE(33, 0, "Pin"), LJ_PE(44, 0, "V-Bar"), LJ_PE(55, 0, "H-Bar"), LJ_PE(66, 0, "Cross"), \
                         LJ_PE(77, 0, "Arrow"), LJ_PE(88, 0, "Triangls"), LJ_PE(99, 0, "Star"), LJ_PE(110, 0, "Holes"), LJ_PE(121, 0, "Bells"), LJ_PE(132, 0, "Cone"), LJ_PE(143, 0, "Cones"), \
                         LJ_PE(154, 0, "Phone"), LJ_PE(165, 0, "ThinBars"), LJ_PE(176, 0, "Window"), LJ_PE(187, 0, "Turbine") }, LJ_PAL_DEF }
#define LJ_R2_COMMON .tag = "218", .manufacturer = "Martin", .name = "Roboscan 218-II", .fixture_id = "", \
    .shutter = { 1, 0, "Shutter", 0, 6 }, .strobe = { 1, 0, "Strobe", 81, 208 }, .intensity = { 1, 1, "Intens", 0, 255 }, .strobe_off = 6, .use_gm = 1, \
    .blackout = { "", 1, { 1 }, { 0 } }, .reset = { "", 1, { 1 }, { 253 } }, .color = { { 1, 2, "Color 1", 0, 187 } }, .gobo = { { 1, 3, "Gobo 1", 0, 187 } }, \
    .palette = { LJ_R2_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_R2_GOBO1 }, .pan_ofs = 4, .presets = 1, .macros = 1, .follow_spot = 1, .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1 }
static const lj_profile lj_r2_m3 = { .id = 2, .footprint = 8, .mode = "Mode 3", LJ_R2_COMMON,
    /* 1 shutter / strobe / stand-alone / reset, 2 dimmer, 3 colour, 4 gobo, 5-8 pan, pan fine, tilt, tilt fine */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT },
    .pt16 = 1, .tilt_ofs = 6, .windows = lj_r2_windows3, .nwindows = LJ_NROWS(lj_r2_windows3),
    .notes = "Does NOT apply to a Roboscan 218 running via the DMX interface. Mode 3, 16 bit pan/tilt tracking, PL432 jumper on pins 4 and 5. "
             "Fixture stand-alone modes use the shutter channel (special). 8 DMX addresses." };
static const lj_profile lj_r2_m2 = { .id = 57, .footprint = 8, .mode = "Mode 2", LJ_R2_COMMON,
    /* 1-4 as Mode 3, 5 pan, 6 tilt, 7 movement speed, 8 colour / gobo / dimmer speed */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pt8 = 1, .tilt_ofs = 5, .windows = lj_r2_windows2, .nwindows = LJ_NROWS(lj_r2_windows2),
    .notes = "Mode 2, 8 bit pan/tilt tracking / speed, PL432 jumper on pins 5 and 6. 8 DMX addresses." };

/* ================= via the Martin DMX Interface ================= */
/* The interface translates DMX to the Martin protocol (odd start addresses only, PDF 2.6b6 p.325); its channel map was not fetched. Until it is,
 * each entry is a Levels-only profile whose footprint and classes are the channel set its exe control classes imply: PROPOSAL rig item.
 *   218 MK I (TIntensityControl_O218, TColorControl_O218, TGoboControl_O218): shutter, colour, gobo, pan, tilt = 5.
 *   805 (TIntensityControl_O805 with Lamp On / Off, TColorControl_O805, TGoboControl_O805): intensity / lamp, colour, gobo, pan, tilt = 5.
 *   1220IIR (TIntensityControl_O1220h + FII Speed, TColorControl_O1220 + Color Speed, TGoboControl_O1220R static + rotating + speeds,
 *   TBeamControl_O1220 iris / focus / frost, TEffectControl_O1220): 14. */
static const lj_ctl_window lj_oi5_windows[]  = { LJ_LEVELS_WIN(5) };
static const lj_ctl_window lj_oi14_windows[] = { LJ_LEVELS_WIN(14) };
#define LJ_OI_COMMON .manufacturer = "Martin", .mode = "via DMX Interface", .fixture_id = "", .blackout = { "", 1, { 1 }, { 0 } }
static const lj_profile lj_oi_218 = { .id = 41, .footprint = 5, .tag = "218", .name = "Roboscan 218", LJ_OI_COMMON,
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_TILT }, .windows = lj_oi5_windows, .nwindows = 1,
    .notes = "Roboscan 218 (MK I) through the Martin DMX interface, interface software 7 or higher: product dipswitch = 11 (binary), reduced DMX mode (dipswitch 10 off). "
             "Generic level control until the interface channel map is verified." };
static const lj_profile lj_oi_805 = { .id = 36, .footprint = 5, .tag = "805", .name = "Roboscan 805", LJ_OI_COMMON,
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_TILT }, .windows = lj_oi5_windows, .nwindows = 1,
    .notes = "Roboscan 805 through the Martin DMX interface, interface software 7 or higher: product dipswitch = 5 (binary), reduced DMX mode (dipswitch 10 off). "
             "Generic level control until the interface channel map is verified." };
static const lj_profile lj_oi_1220 = { .id = 33, .footprint = 14, .tag = "1220IIR", .name = "Roboscan 1220IIR", LJ_OI_COMMON,
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_SPEED, LJ_CH_COLOR, LJ_CH_SPEED, LJ_CH_GOBO, LJ_CH_SPEED, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_TILT },
    .windows = lj_oi14_windows, .nwindows = 1,
    .notes = "Roboscan 1220 II R through the Martin DMX interface, interface software 7 or higher: product dipswitch = 14 (binary), full DMX mode (dipswitch 10 on). "
             "Generic level control until the interface channel map is verified." };

const lj_profile *const lj_hand_roboscans[LJ_ROBOSCAN_COUNT] = { &lj_r5_m3, &lj_r5_m2, &lj_r8_m1, &lj_r8_m2, &lj_r2_m3, &lj_r2_m2, &lj_oi_218, &lj_oi_805, &lj_oi_1220 };
