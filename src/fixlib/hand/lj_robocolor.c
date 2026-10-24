/* lj_robocolor.c - Martin RoboColor family, transcribed from 15-builtin-profiles/08-robocolor.md.
 *   14  COL-III Martin / RoboColor III / Mode 1, 60 COL-III Martin / RoboColor III / Mode 2: martin.com "RoboColor III DMX protocol V1" and
 *               UM-RoboColorIII Rev C p.19 (mode 1 = 5 ch, PL113 jumper off; mode 2 = 6 ch with colour speed, PL113 pins 1-2).
 *   15  C400    Martin / RoboColor Pro 400 / Mode 3, 61 C400 Martin / RoboColor Pro 400 / Mode 2: QLC+ Martin-Robocolor-Pro-400.qxf ("Mode 1
 *               (Tracking)" 5 ch = LJ's Mode 3 without speed, "Mode 2 (Vector)" 7 ch); no Martin manual fetched: values unverified.
 *   13  COL-II  Martin / RoboColor II / Mode 1, 59 COL-II Martin / RoboColor II / Mode 2; 37 COL-I Martin / RoboColor I / via DMX Interface;
 *   40  COL-II  Martin / RoboColor II / via DMX Interface: no protocol source (Martin's RoboColor II manual and the DMX interface manual were
 *               not located [15/08 s2]). Like the W2 interface Roboscans (lj_roboscan.c) each is a Levels-only profile whose footprint and
 *               classes are the channel set its exe control classes imply: PROPOSAL rig item.
 * Captions are the exe literals of TColorControl_Color2 (Color Head 1-4; the RoboColor III has no class of its own and uses the Color2 set),
 * TIntensityControl_C400, TColorControl_C400, TGoboControl_C400, TLampControl_C400, TSpecialSettings_Color2 / _C400 / _OCol1 [13 s2.6].
 * Every value = the low end of its range (F47).;
 * rotation buttons carry the slow end (lj_profile.h row conventions). */
#include "fixlib/hand/lj_robocolor.h"
#include "fixlib/hand/lj_martin.h"

/* ================= RoboColor III ================= */
/* Ch 1: 0-74 no function; 75-138 strobe fast -> slow; 139-170 remote stand-alone auto; 171-202 music; 203-255 reset.
 * Ch 2-5 colour wheel head 1-4: 0-15 blackout -> white; 16-180 continuous white -> colour 1 .. 11 (15 each); "snaps" 181-184 colour 11,
 * 185-189 colour 10 .. 230-234 colour 1, 235-239 white, 240-255 blackout. "Each RoboColor III head has 11 dichroic colors plus white" (p.19
 * text): 13 positions with blackout. Ch 6 (mode 2) colour speed 0 tracking, 1-255 fast -> slow.
 * IMG multicolorhead1.gif "RoboColor multi-head mode": four head rows (fade, scroll, 13-swatch palette) and a fifth row Off + Strobe bar. */
#define LJ_RC_HEAD(h) LJ_FB(h, 1, LJ_HAND_SNAP), LJ_SCRL(h, "Color Head " #h, NULL, 0, 180), LJ_PALR(h, "Color Head " #h, 0)
static const lj_ctl_row lj_rc_colors_rows[] = { LJ_RC_HEAD(1), LJ_RC_HEAD(2), LJ_RC_HEAD(3), LJ_RC_HEAD(4), LJ_FB(0, 1, LJ_HAND_SNAP), LJ_BAR(0, "Strobe", 75, 138, 1) };
static const lj_ctl_button lj_rc_sa[]    = { { "Sound", 0, 171, 171, 202 }, { "Auto", 0, 139, 139, 170 } };
static const lj_ctl_button lj_rc_reset[] = { { "Reset", 0, 203, 203, 255 } };
#define LJ_RC_EXT_COMMON LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 139, 202, lj_rc_sa, 2), LJ_SPEC_OS(0, "Fixture Reset", 203, 255, lj_rc_reset, 1)
static const char *const lj_rc_speed_items[] = { "Tracking", "Speed", NULL };
static const lj_ctl_button lj_rc_speed[] = { { "Tracking", 5, 0, 0, 0 }, { "Speed", 5, 1, 1, 255 } };
static const lj_ctl_row lj_rc_ext1_rows[] = { LJ_RC_EXT_COMMON };                                   /* TSpecialSettings_Color2 (variant inferred) */
static const lj_ctl_row lj_rc_ext2_rows[] = {                                                       /* TSpecialSettings_Color2_2 */
    LJ_FB(5, 1, LJ_HAND_OFF), LJ_RADIO(5, "Col Speed", "Color Speed", 1, 255, lj_rc_speed, 2, lj_rc_speed_items), LJ_RC_EXT_COMMON };
static const lj_ctl_window lj_rc_windows1[] = { LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_rc_colors_rows), LJ_EXT_WIN(lj_rc_ext1_rows) };
static const lj_ctl_window lj_rc_windows2[] = { LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_rc_colors_rows), LJ_EXT_WIN(lj_rc_ext2_rows) };
/* The snap positions; the gel names are not in the manual: colour slots unnamed and unassigned (as the Roboscan 812). */
#define LJ_RC_COLOR1 { { LJ_PE(240, 0x000000, "Black"), LJ_PE(235, 0xFFFFFF, "White"), LJ_PE(230, 0, ""), LJ_PE(225, 0, ""), LJ_PE(220, 0, ""), LJ_PE(215, 0, ""), \
                         LJ_PE(210, 0, ""), LJ_PE(205, 0, ""), LJ_PE(200, 0, ""), LJ_PE(195, 0, ""), LJ_PE(190, 0, ""), LJ_PE(185, 0, ""), LJ_PE(181, 0, "") }, LJ_PAL_DEF }
/* Blackout: the colour wheels to blackout ("Blackout uses Color Wheels", NOTES); no dimmer, so Grand Master 0 % = blackout. */
#define LJ_RC_COMMON .tag = "COL-III", .manufacturer = "Martin", .name = "RoboColor III", .fixture_id = "", .strobe = { 1, 0, "Strobe", 75, 138 }, .strobe_off = 0, \
    .blackout = { "", 1, { 0, 1, 1, 1, 1 }, { 0, 240, 240, 240, 240 } }, .bo_at_gm0 = 1, .reset = { "", 1, { 1 }, { 203 } }, \
    .color = { { 1, 1, "Head 1", 0, 180 }, { 1, 2, "Head 2", 0, 180 }, { 1, 3, "Head 3", 0, 180 }, { 1, 4, "Head 4", 0, 180 } }, .palette = { LJ_RC_COLOR1 }
static const lj_profile lj_rc_m1 = { .id = 14, .footprint = 5, .mode = "Mode 1", LJ_RC_COMMON,
    /* 1 strobe / stand-alone / reset, 2-5 colour heads 1-4 */
    .channel_class = { LJ_CH_STROBE, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR }, .level_invisible = { 1, 1, 1, 1, 1 },
    .windows = lj_rc_windows1, .nwindows = LJ_NROWS(lj_rc_windows1),
    .notes = "DMX mode 1, no speed control, PL113 jumper off. Blackout uses the colour wheels. 5 DMX addresses." };
static const lj_profile lj_rc_m2 = { .id = 60, .footprint = 6, .mode = "Mode 2", LJ_RC_COMMON,
    /* 1-5 as mode 1, 6 colour speed */
    .channel_class = { LJ_CH_STROBE, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_SPEED }, .level_invisible = { 1, 1, 1, 1, 1, 1 },
    .windows = lj_rc_windows2, .nwindows = LJ_NROWS(lj_rc_windows2),
    .notes = "DMX mode 2, speed control, PL113 jumper on pins 1 and 2. Blackout uses the colour wheels. 6 DMX addresses." };

/* ================= RoboColor Pro 400 (QLC+ data) ================= */
/* Ch 1: 0-3 strobe off; 4-7 strobe off / fan low; 8-199 strobe fast -> slow; 200-218 remote music; 219-238 remote auto; 239-248 reset;
 * 249-252 lamp on; 253-255 lamp off (mode 3 only: "Mode 2 ... No Lamp Off command", NOTES). Ch 2 dimmer 0-10 light off, 11-237, 238-255 on.
 * Ch 3 colour 1 / ch 4 colour 2: 0-156 scroll (26 each), 157-255 fixed positions. Ch 5 beam ("Gobo"): 0 wash, 51 wide, 102 medium,
 * 153 narrow, 204-255 blackout. Mode 2: ch 6 colour speed, ch 7 dimmer speed, 0-255 fast -> slow. */
static const lj_ctl_row lj_c4_intensity_rows[] = {     /* TIntensityControl_C400 (no captions of its own: the MAC 500 bar captions, inferred) */
    LJ_FB(0, 1, LJ_HAND_SNAP), LJ_BAR(0, "Strobe", 8, 199, 1), LJ_FB(1, 1, LJ_HAND_FADE), LJ_BAR(1, "Intensity", 0, 255, 0) };
static const lj_ctl_row lj_c4_colors_rows[] = {        /* TColorControl_C400: two Current Color scrolls (hints C400COLORSCROLL1 / 2) */
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Color", NULL, 0, 156), LJ_PALR(2, "Color 1", 0),
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Current Color", NULL, 0, 156), LJ_PALR(3, "Color 2", 1) };
static const lj_ctl_row lj_c4_gobos_rows[] = { LJ_FB(4, 1, LJ_HAND_SNAP), LJ_SCRL(4, "Current Gobo", NULL, 0, 204), LJ_PALR(4, "Gobo 1", 2) };   /* TGoboControl_C400 */
static const lj_ctl_button lj_c4_lamp3[] = { { "Lamp On", 0, 249, 249, 252 }, { "Lamp Off", 0, 253, 253, 255 }, { "Fan Lo", 0, 4, 4, 7 } };
static const lj_ctl_button lj_c4_lamp2[] = { { "Lamp On", 0, 249, 249, 252 }, { "Fan Lo", 0, 4, 4, 7 } };
static const lj_ctl_row lj_c4_lamp3_rows[] = { LJ_FB_OS(0), LJ_LAMPB(0, "Lamp On", "Lamp Off", 4, 255, lj_c4_lamp3, 3) };   /* TLampControl_C400 (layout inferred) */
static const lj_ctl_row lj_c4_lamp2_rows[] = { LJ_FB_OS(0), LJ_LAMPB(0, "Lamp On", NULL, 4, 252, lj_c4_lamp2, 2) };
static const lj_ctl_button lj_c4_sa[]    = { { "Sound", 0, 200, 200, 218 }, { "Auto", 0, 219, 219, 238 } };
static const lj_ctl_button lj_c4_reset[] = { { "Reset", 0, 239, 239, 248 } };
#define LJ_C4_EXT_COMMON LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 200, 238, lj_c4_sa, 2), LJ_SPEC_OS(0, "Fixture Reset", 239, 248, lj_c4_reset, 1)
static const char *const lj_c4_speed_items[] = { "Speed", NULL };
static const lj_ctl_button lj_c4_cspeed[] = { { "Speed", 5, 0, 0, 255 } };
static const lj_ctl_button lj_c4_dspeed[] = { { "Speed", 6, 0, 0, 255 } };
static const lj_ctl_row lj_c4_ext3_rows[] = { LJ_C4_EXT_COMMON };                                  /* TSpecialSettings_C400 */
static const lj_ctl_row lj_c4_ext2_rows[] = {                                                      /* TSpecialSettings_C400_2 */
    LJ_FB(5, 1, LJ_HAND_OFF), LJ_RADIO(5, "Col Speed", "Color Speed", 0, 255, lj_c4_cspeed, 1, lj_c4_speed_items),
    LJ_FB(6, 1, LJ_HAND_OFF), LJ_RADIO(6, "Dimmer Speed", "Dimmer Speed", 0, 255, lj_c4_dspeed, 1, lj_c4_speed_items), LJ_C4_EXT_COMMON };
#define LJ_C4_WINDOWS(name, lamp, ext) static const lj_ctl_window name[] = { LJ_WIN0(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_c4_intensity_rows), \
    LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_c4_colors_rows), LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_c4_gobos_rows), \
    LJ_WIN0(LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lamp), LJ_EXT_WIN(ext) }
LJ_C4_WINDOWS(lj_c4_windows3, lj_c4_lamp3_rows, lj_c4_ext3_rows);
LJ_C4_WINDOWS(lj_c4_windows2, lj_c4_lamp2_rows, lj_c4_ext2_rows);
/* Fixed positions (QLC names; CTC names spelled as the exe colour table where it has them). RGB ours. */
#define LJ_C4_COLOR1 { { LJ_PE(242, 0xFFFFFF, ""), LJ_PE(228, 0xFFFF00, ""), LJ_PE(215, 0x40A040, ""), LJ_PE(200, 0xFF3000, ""), LJ_PE(186, 0x00FFFF, ""), \
                         LJ_PE(172, 0x00C0A0, ""), LJ_PE(157, 0x8000C0, "") }, LJ_PAL_DEF }
#define LJ_C4_COLOR2 { { LJ_PE(242, 0xFFFFFF, ""), LJ_PE(228, 0xFFA0C0, ""), LJ_PE(215, 0x80FF80, ""), LJ_PE(200, 0xFF00FF, ""), LJ_PE(186, 0x8060C0, ""), \
                         LJ_PE(172, 0xFFD8A8, "CTC 5500-3400"), LJ_PE(157, 0xC0D8FF, "") }, LJ_PAL_DEF }
#define LJ_C4_GOBO1  { { LJ_PE(0, 0, "Open"), LJ_PE(51, 0, ""), LJ_PE(102, 0, ""), LJ_PE(153, 0, ""), LJ_PE(204, 0, "Blackout") }, LJ_PAL_DEF }   /* wash, wide, medium, narrow beam, blackout */
#define LJ_C4_COMMON .tag = "C400", .manufacturer = "Martin", .name = "RoboColor Pro 400", .fixture_id = "", .strobe = { 1, 0, "Strobe", 8, 199 }, .strobe_off = 0, \
    .intensity = { 1, 1, "Intens", 0, 255 }, .use_gm = 1, .blackout = { "", 1, { 0, 1 }, { 0, 0 } }, .lamp_on = { "", 1, { 1 }, { 249 } }, .reset = { "", 1, { 1 }, { 239 } }, \
    .color = { { 1, 2, "Color 1", 0, 156 }, { 1, 3, "Color 2", 0, 156 } }, .gobo = { { 1, 4, "Gobo", 0, 204 } }, \
    .palette = { LJ_C4_COLOR1, LJ_C4_COLOR2, LJ_C4_GOBO1 }
static const lj_profile lj_c4_m3 = { .id = 15, .footprint = 5, .mode = "Mode 3", LJ_C4_COMMON, .lamp_off = { "", 1, { 1 }, { 253 } },
    /* 1 strobe / fan / stand-alone / reset / lamp, 2 dimmer, 3 colour 1, 4 colour 2, 5 beam */
    .channel_class = { LJ_CH_STROBE, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_GOBO }, .level_invisible = { 1, 1, 1, 1, 1 },
    .windows = lj_c4_windows3, .nwindows = LJ_NROWS(lj_c4_windows3),
    .notes = "DMX mode 3, no speed control, lamp off command enabled: PL11 jumper on pins 5 and 6. 5 DMX addresses." };
static const lj_profile lj_c4_m2 = { .id = 61, .footprint = 7, .mode = "Mode 2", LJ_C4_COMMON,
    /* 1-5 as mode 3, 6 colour speed, 7 dimmer speed */
    .channel_class = { LJ_CH_STROBE, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_SPEED, LJ_CH_SPEED }, .level_invisible = { 1, 1, 1, 1, 1, 1, 1 },
    .windows = lj_c4_windows2, .nwindows = LJ_NROWS(lj_c4_windows2),
    .notes = "DMX mode 2, speed control, no lamp off command: PL11 jumper on pins 4 and 5. 7 DMX addresses." };

/* ================= no protocol source: Levels only, PROPOSAL rig item ================= */
/* RoboColor II (TIntensityControl_Color2 Off/Snap + Lamp On / Off on the halogen lamp, TColorControl_Color2 heads 1-4): the III's channel set
 * (the index's "same protocol family", inferred) = function, heads 1-4 [, colour speed]. Via the DMX interface (TIntensityControl_OColor1 /
 * _OColor2 lamp, TColorControl_OColor1 / _OColor2 heads 1-4 + Speed, TSpecialSettings_OCol1 / _OColor2 Col Speed): lamp, heads 1-4, speed = 6. */
static const lj_ctl_window lj_r2_windows5[] = { LJ_LEVELS_WIN(5) };
static const lj_ctl_window lj_r2_windows6[] = { LJ_LEVELS_WIN(6) };
#define LJ_RO_COMMON .manufacturer = "Martin", .fixture_id = ""
static const lj_profile lj_r2_m1 = { .id = 13, .footprint = 5, .tag = "COL-II", .name = "RoboColor II", .mode = "Mode 1", LJ_RO_COMMON,
    .channel_class = { LJ_CH_SPECIAL, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR }, .windows = lj_r2_windows5, .nwindows = 1,
    .notes = "DMX mode 1, no speed control, PL113 jumper off. Does not apply to a RoboColor II running through the DMX interface. "
             "Generic level control until the RoboColor II channel map is verified." };
static const lj_profile lj_r2_m2 = { .id = 59, .footprint = 6, .tag = "COL-II", .name = "RoboColor II", .mode = "Mode 2", LJ_RO_COMMON,
    .channel_class = { LJ_CH_SPECIAL, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_SPEED }, .windows = lj_r2_windows6, .nwindows = 1,
    .notes = "DMX mode 2, speed control, PL113 jumper on pins 1 and 2. Does not apply to a RoboColor II running through the DMX interface. "
             "Generic level control until the RoboColor II channel map is verified." };
static const lj_profile lj_oi_col1 = { .id = 37, .footprint = 6, .tag = "COL-I", .name = "RoboColor I", .mode = "via DMX Interface", LJ_RO_COMMON,
    .channel_class = { LJ_CH_LAMP, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_SPEED }, .windows = lj_r2_windows6, .nwindows = 1,
    .notes = "RoboColor I through the Martin DMX interface, interface software 7 or higher: product dipswitch = 2 (binary), full DMX mode (dipswitch 10 on). "
             "Generic level control until the interface channel map is verified." };
static const lj_profile lj_oi_col2 = { .id = 40, .footprint = 6, .tag = "COL-II", .name = "RoboColor II", .mode = "via DMX Interface", LJ_RO_COMMON,
    .channel_class = { LJ_CH_LAMP, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_SPEED }, .windows = lj_r2_windows6, .nwindows = 1,
    .notes = "The old RoboColor II (MK I) through the Martin DMX interface, interface software 7 or higher: product dipswitch = 3 (binary), "
             "full DMX mode (dipswitch 10 on). Generic level control until the interface channel map is verified." };

const lj_profile *const lj_hand_robocolors[LJ_ROBOCOLOR_COUNT] = { &lj_r2_m1, &lj_r2_m2, &lj_rc_m1, &lj_rc_m2, &lj_c4_m3, &lj_c4_m2, &lj_oi_col1, &lj_oi_col2 };
