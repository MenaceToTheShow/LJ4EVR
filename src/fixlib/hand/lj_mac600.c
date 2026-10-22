/* lj_mac600.c - Martin MAC 600 (exe id 11, tag MAC600, Martin / MAC 600 / Mode 2/4,
 * Mac600Notes.Rtf) and MAC 600 NT (67, MAC600NT, Martin / MAC 600 NT / Mode 2/4,
 * MAC600NTNotes.Rtf), transcribed from 15-builtin-profiles/02-mac600.md: MAC 600 user
 * manual Rev. H p.20-21 and the martin.com DMX table V3 (Mode 4 = 14 channels); the NT
 * from QLC+ Martin-MAC600-NT.qxf Mode 4 (15 channels, LJ release note 2.6 "Corrected the
 * number of DMX channels used by the MAC600NT profile to 15"; no Martin NT table fetched:
 * its values are unverified). Captions are the exe literals of TColorControl_Mac600 /
 * _MAC600NT, TBeamControl_Mac600p / _Mac600NT [13 s2.6]. No Gobo and no Effects window
 * (_NoFunction) [15/02 s3]. Every button value = the low end of its range (F47). */
#include "fixlib/hand/lj_mac600.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Intensity: TIntensityControl_Mac600, ch 1 (DMXTAB-MAC600 V3, UM-MAC600 Rev H p.20) ----
 * The MAC 600 strobe is 50-112 and random strobe 128-187 in three bands; no pulse functions. */
static const lj_ctl_button lj_m6_shutter[] = { { "Shutter Closed", 0, 0, 0, 19 }, { "Shutter Open", 0, 20, 20, 49 } };
static const lj_ctl_button lj_m6_rstrobe[] = { { "Random Strobe Slow", 0, 168, 168, 187 }, { "Random Strobe Med", 0, 148, 148, 167 }, { "Random Strobe Fast", 0, 128, 128, 147 } };   /* exe strings after the Mac600/Ext600C pair [15/02 s2] */
static const lj_ctl_row lj_m6_intensity_rows[] = {
    LJ_FB(0, 2, LJ_HAND_SNAP),
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_m6_shutter, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 50, 112, 1),                                         /* p.20: 50-112 strobe fast -> slow */
    LJ_FB(1, 1, LJ_HAND_FADE),
    LJ_BAR(1, "Intensity", 0, 255, 0) };
static const lj_ctl_row lj_m6_intensity_specials[] = { LJ_SPEC(0, "Random Strobe", NULL, 128, 187, lj_m6_rstrobe, 3) };   /* group label inferred */

/* ---- Colors: TColorControl_Mac600 / _MAC600NT (IMG cmy.gif: one Fade button per CMY bar) ----
 * Colour wheel p.21: 0 white, 40 CTC, 80 red, 120 green, 160 blue (continuous between); 186-214 CW fast->slow,
 * 215-243 CCW slow->fast; 244-247 / 248-251 / 252-255 random CMY fast / medium / slow. */
#define LJ_M6_CMY(c) LJ_FB(c, 1, LJ_HAND_FADE), LJ_BAR(c, "Cyan", 0, 255, 0), LJ_FB(c + 1, 1, LJ_HAND_FADE), LJ_BAR(c + 1, "Magenta", 0, 255, 0), \
                     LJ_FB(c + 2, 1, LJ_HAND_FADE), LJ_BAR(c + 2, "Yellow", 0, 255, 0)
static const lj_ctl_button lj_m6_col_rot[]  = { { "Wheel Scroll CW", 5, 186, 186, 214 }, { "Wheel Scroll CCW", 5, 215, 215, 243 }, { "Stop", 5, 0, 0, 0 } };
static const lj_ctl_button lj_m6_col_rnd[]  = { { "Rnd Slow", 5, 252, 252, 255 }, { "Rnd Med", 5, 248, 248, 251 }, { "Rnd Fast", 5, 244, 244, 247 } };
static const lj_ctl_button lj_nt_col_rot[]  = { { "Wheel Scroll CW", 6, 186, 186, 214 }, { "Wheel Scroll CCW", 6, 215, 215, 243 }, { "Stop", 6, 0, 0, 0 } };
static const lj_ctl_button lj_nt_col_rnd[]  = { { "Rnd Slow", 6, 252, 252, 255 }, { "Rnd Med", 6, 248, 248, 251 }, { "Rnd Fast", 6, 244, 244, 247 } };
static const lj_ctl_row lj_m6_colors_rows[] = {
    LJ_M6_CMY(2),
    LJ_FB(5, 1, LJ_HAND_SNAP), LJ_SCRL(5, "Fixed Color", "Current Color", 0, 160), LJ_ROT(5, "Wheel Scroll Speed", 186, 243, lj_m6_col_rot), LJ_PALR(5, "Color 1", 0) };
static const lj_ctl_row lj_m6_colors_specials[] = { LJ_SPEC(5, "Random", NULL, 244, 255, lj_m6_col_rnd, 3) };
static const lj_ctl_row lj_nt_colors_rows[] = {
    LJ_M6_CMY(2),
    LJ_FB(5, 1, LJ_HAND_FADE), LJ_BAR(5, "CTC", 0, 255, 0),                 /* QLC ch 6 CTC cold -> warm; which window LJ shows it in is unverified [15/02 s2]: Colors, PROPOSAL */
    LJ_FB(6, 1, LJ_HAND_SNAP), LJ_SCRL(6, "Fixed Color", "Current Color", 0, 160), LJ_ROT(6, "Wheel Scroll Speed", 186, 243, lj_nt_col_rot), LJ_PALR(6, "Color 1", 0) };
static const lj_ctl_row lj_nt_colors_specials[] = { LJ_SPEC(6, "Random", NULL, 244, 255, lj_nt_col_rnd, 3) };

/* ---- Beam: TBeamControl_Mac600p (Beam1, Beam2, Frost) / TBeamControl_Mac600NT (Beam Shaper, Frost) ----
 * MAC 600 p.21: ch 7 beam shaper 1 (0 open, 1-255 left -> right); ch 8 beam shaper 2 3-170 and frost 171-255 on one channel. */
static const lj_ctl_row lj_m6_beam_rows[] = {
    LJ_FB(6, 1, LJ_HAND_FADE), LJ_BAR(6, "Beam1", 0, 255, 0),
    LJ_FB(7, 2, LJ_HAND_FADE), LJ_BAR(7, "Beam2", 3, 170, 0), LJ_BAR(7, "Frost", 171, 255, 0) };   /* one fade button for the shared channel (inferred) */
static const lj_ctl_row lj_nt_beam_rows[] = {
    LJ_FB(7, 1, LJ_HAND_FADE), LJ_BAR(7, "Beam Shaper", 0, 255, 0),         /* QLC ch 8: 0 open, 1-255 left -> right */
    LJ_FB(8, 1, LJ_HAND_FADE), LJ_BAR(8, "Frost", 0, 255, 0) };             /* QLC ch 9: no frost -> full */

/* ---- Extended: TSpecialSettings_Mac600 / _Mac600NT [15/00 s4] ----
 * MAC 600 p.21 ch 13: 0-2 tracking, 3-245 fast->slow, 246-248 PTSP NORM, 249-251 PTSP FAST, 252-255 blackout while moving;
 * ch 14: 0-2 tracking, 3-239 fast->slow, 240-242 MOdE NORM, 243-245 MOdE STUd, 246-248 SCUT OFF, 249-251 SCUT ON, 252-255 fast / B/O.
 * The NT (QLC ch 14 / 15) has no studio mode (its notes lack SPEC/MOdE): 3-245 speed, 246-248 SCUT OFF, 249-251 SCUT ON. Variants inferred. */
static const char *const lj_m6_pt_items[] = { "Tracking", "Speed", "Normal", "Fast", "B/O while moving", NULL };
static const char *const lj_m6_fx_items[] = { "Tracking", "Speed", "Mode=Normal", "Mode=Studio", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
static const char *const lj_nt_fx_items[] = { "Tracking", "Speed", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
#define LJ_M6_PT_VALS(c) { "Tracking", c, 0, 0, 2 }, { "Speed", c, 3, 3, 245 }, { "Normal", c, 246, 246, 248 }, { "Fast", c, 249, 249, 251 }, { "B/O while moving", c, 252, 252, 255 }
static const lj_ctl_button lj_m6_pt_vals[] = { LJ_M6_PT_VALS(12) };
static const lj_ctl_button lj_nt_pt_vals[] = { LJ_M6_PT_VALS(13) };
static const lj_ctl_button lj_m6_fx_vals[] = { { "Tracking", 13, 0, 0, 2 }, { "Speed", 13, 3, 3, 239 }, { "Mode=Normal", 13, 240, 240, 242 }, { "Mode=Studio", 13, 243, 243, 245 },
                                               { "Shortcuts Disabled", 13, 246, 246, 248 }, { "Shortcuts Enabled", 13, 249, 249, 251 }, { "B/O while moving", 13, 252, 252, 255 } };
static const lj_ctl_button lj_nt_fx_vals[] = { { "Tracking", 14, 0, 0, 2 }, { "Speed", 14, 3, 3, 245 }, { "Shortcuts Disabled", 14, 246, 246, 248 }, { "Shortcuts Enabled", 14, 249, 249, 251 },
                                               { "B/O while moving", 14, 252, 252, 255 } };
static const lj_ctl_row lj_m6_extended_rows[] = {
    LJ_FB(12, 1, LJ_HAND_OFF), LJ_RADIO(12, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, lj_m6_pt_vals, 5, lj_m6_pt_items),
    LJ_FB(13, 1, LJ_HAND_OFF), LJ_RADIO(13, "Effects Settings", "Effect Speed", 3, 239, lj_m6_fx_vals, 7, lj_m6_fx_items),
    LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_nt_extended_rows[] = {
    LJ_FB(13, 1, LJ_HAND_OFF), LJ_RADIO(13, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, lj_nt_pt_vals, 5, lj_m6_pt_items),
    LJ_FB(14, 1, LJ_HAND_OFF), LJ_RADIO(14, "Effects Settings", "Effect Speed", 3, 245, lj_nt_fx_vals, 5, lj_nt_fx_items),
    LJ_MT_RESET_ROWS };

static const lj_ctl_window lj_m6_windows[] = {
    LJ_WIN(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_m6_intensity_rows, lj_m6_intensity_specials), LJ_MT_MOVE16_WIN(8),
    LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_m6_colors_rows, lj_m6_colors_specials), LJ_WIN0(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_m6_beam_rows),
    LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m6_extended_rows) };
static const lj_ctl_window lj_nt_windows[] = {
    LJ_WIN(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_m6_intensity_rows, lj_m6_intensity_specials), LJ_MT_MOVE16_WIN(9),
    LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_nt_colors_rows, lj_nt_colors_specials), LJ_WIN0(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_nt_beam_rows),
    LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_nt_extended_rows) };

/* ---- Palette: the wheel's white, CTC (5600 K -> 3400 K, exe name CTC 5500-3400), red, green, blue (p.21 chart); RGB ours, bitmap per slot inferred ---- */
#define LJ_M6_COLOR1 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(40, 0xFFD8A8, "CTC 5500-3400"), LJ_PE(80, 0xFF0000, "Primary Red"), LJ_PE(120, 0x00C000, ""), LJ_PE(160, 0x0000FF, "") }, LJ_PAL_DEF }

/* Lamp: TLampControl_DOO, 228-237 on / 248-255 off (lamp off with SPEC/dLOF on, which the notes require). Blackout: shutter closed 0-19
 * (CHM masterintensitycontrol.htm: fading in from blackout does not work on a MAC 600). */
#define LJ_M6_COMMON \
    .manufacturer = "Martin", .fixture_id = "", LJ_MT_CH12, .strobe = { 1, 0, "Strobe", 50, 112 }, \
    .cmy_on = 1, .cmy_picker = 1, .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1, .palette = { LJ_M6_COLOR1 }

static const lj_profile lj_m6_mac600 = { .id = 11, .footprint = 14, .tag = "MAC600", .name = "MAC 600", .mode = "Mode 2/4", LJ_M6_COMMON,
    /* 1 shutter, 2 intensity, 3-5 CMY, 6 colour, 7 beam shaper 1, 8 beam shaper 2 / frost, 9-12 pan/tilt 16 bit, 13-14 speed (p.20-21) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_BEAM, LJ_CH_BEAM,
                       LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .color = { { 1, 5, "Color 1", 0, 160 } },
    .beam = { { 1, 6, "Beam1", 0, 255 }, { 1, 7, "Beam2", 3, 170 } }, .frost = { 1, 7, "Frost", 171, 255 }, .pan_ofs = 8, .tilt_ofs = 10,
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, .windows = lj_m6_windows, .nwindows = LJ_NROWS(lj_m6_windows),
    .notes = "Fixture settings: PSEt = 2 or 4, PTSP = FAST, EFSP = FAST, SPEC/TRAC = MOd1, SPEC/SCUT = on, SPEC/dLOF = on, SPEC/dRES = on, SPEC/MOdE = NORM. "
             "Mode 4 uses 14 DMX addresses. Blackout closes the shutter, so a fade in from blackout snaps." };
static const lj_profile lj_m6_mac600nt = { .id = 67, .footprint = 15, .tag = "MAC600NT", .name = "MAC 600 NT", .mode = "Mode 2/4", LJ_M6_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 CTC, 7 colour, 8 beam shaper, 9 frost, 10-13 pan/tilt 16 bit, 14-15 speed (QLC Mode 4) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_BEAM, LJ_CH_BEAM,
                       LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .color = { { 1, 6, "Color 1", 0, 160 }, { 1, 5, "CTC", 0, 255 } },
    .beam = { { 1, 7, "Shaper", 0, 255 } }, .frost = { 1, 8, "Frost", 0, 255 }, .pan_ofs = 9, .tilt_ofs = 11,
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, .windows = lj_nt_windows, .nwindows = LJ_NROWS(lj_nt_windows),
    .notes = "Fixture settings: PSEt = 2 or 4, SPEC/TRAC = MOd1, SPEC/SCUT = on, SPEC/dLOF = on, SPEC/dRES = on. Mode 4 uses 15 DMX addresses (CTC on channel 6)." };

const lj_profile *const lj_hand_mac600s[LJ_MAC600_COUNT] = { &lj_m6_mac600, &lj_m6_mac600nt };
