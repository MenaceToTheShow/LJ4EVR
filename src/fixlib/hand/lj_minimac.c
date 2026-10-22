/* lj_minimac.c - Martin MiniMAC family, transcribed from 15-builtin-profiles/07-minimac.md.
 *   54  M-MAC-P Martin / MiniMAC Profile / Mode 2/4 (MiniMacPNotes.Rtf, shutter only)
 *   65  M-MAC-P Martin / MiniMAC Profile / Intensity Mode, DMXMode 2/4 (MiniMacPINotes.Rtf, shutter fade = intensity on ch 1)
 *               martin.com "MiniMAC Profile DMX Protocol Version 2, implemented from firmware version 1.5" mode 4 (10 ch) and
 *               UM-MiniMACProfile Rev H p.40-42.
 *   53  M-MAC-W Martin / MiniMAC Wash / Mode 2/4, 66 M-MAC-W Martin / MiniMAC Wash / Intensity Mode, DMXMode 2/4: martin.com "MiniMAC Wash
 *               DMX Protocol Version 2" mode 4 (8 ch).
 *   69  MAESTRO Martin / MiniMAC Maestro I & II (MaestroNotes.Rtf): QLC+ Martin-MiniMAC-Maestro.qxf Mode 4 (11 ch); the Martin Maestro manual
 *               was not fetched: values unverified.
 * Captions are the exe literals of TIntensityControl_MiniMac / _MiniMacI, TColorControl_MiniMac, TGoboControl_MiniMacP / _MiniMacM,
 * TBeamControl_MiniMacM, TLampControl_MiniMac, TSpecialSettings_MiniMacP / _W / _M [13 s2.6]. Every value = the low end of its range (F47). */
#include "fixlib/hand/lj_minimac.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Intensity, ch 1 (DMXTAB V2): shutter fade disabled 0-19 closed, 20-38 open; fade enabled 0-38 closed -> open (the Intensity Mode
 * personality); 39-49 open; 50-112 strobe fast -> slow; 113-127 open; 128-137 / 138-147 / 148-157 random strobe fast / medium / slow;
 * 158-167 open; 168-177 stand-alone music; 178-187 stand-alone auto; 188-207 open; 208-217 reset; 218-227 open; 228-237 lamp on;
 * 238-247 open; 248-255 lamp off (hold 5 s) = the MAC 500 lamp / reset values (lj_mt_lamp_rows, LJ_MT_RESET_ROWS).
 * TIntensityControl_MiniMacI carries Random Strobe / Slow / Fast ("Med" inferred: short strings are not in the catalogue). */
static const lj_ctl_button lj_mm_shutter[]  = { { "Shutter Closed", 0, 0, 0, 19 }, { "Shutter Open", 0, 20, 20, 49 } };
static const lj_ctl_button lj_mi_shutter[]  = { { "Shutter Closed", 0, 0, 0, 0 }, { "Shutter Open", 0, 39, 39, 49 } };   /* fade enabled: 0-38 is the intensity bar */
static const lj_ctl_button lj_mm_rstrobe[]  = { { "Slow", 0, 148, 148, 157 }, { "Med", 0, 138, 138, 147 }, { "Fast", 0, 128, 128, 137 } };
static const lj_ctl_row lj_mm_intensity_rows[] = {      /* TIntensityControl_MiniMac: shutter only */
    LJ_FB(0, 2, LJ_HAND_SNAP),
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_mm_shutter, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 50, 112, 1) };                                     /* fast -> slow: reverse */
static const lj_ctl_row lj_mi_intensity_rows[] = {      /* TIntensityControl_MiniMacI: intensity on the shutter channel, one O/S/F for all three rows */
    LJ_FB(0, 3, LJ_HAND_FADE),
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_mi_shutter, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 50, 112, 1), LJ_BAR(0, "Intensity", 0, 38, 0) };
static const lj_ctl_row lj_mm_intensity_specials[] = { LJ_SPEC(0, "Random Strobe", NULL, 128, 157, lj_mm_rstrobe, 3) };

/* ---- Colors: TColorControl_MiniMac (Current Color, Rnd Slow / Med / Fast), ch 2 ----
 * Continuous 0-150: 0 white, 12 colour 1 .. 144 colour 12 (12 each); stepped 151-207 (reverse); 208-245 CW fast -> slow; 246-248 / 249-251 /
 * 252-255 random fast / medium / slow. The CW-only scroll has no control: the class carries no Wheel Scroll caption (EXE-CLS); W4 sweep. */
static const lj_ctl_button lj_mm_colrnd[] = { { "Rnd Slow", 1, 252, 252, 255 }, { "Rnd Med", 1, 249, 249, 251 }, { "Rnd Fast", 1, 246, 246, 248 } };
static const lj_ctl_row lj_mm_colors_rows[] = { LJ_FB(1, 1, LJ_HAND_SNAP), LJ_SCRL(1, "Current Color", NULL, 0, 150), LJ_PALR(1, "Color 1", 0) };
static const lj_ctl_row lj_mm_colors_specials[] = { LJ_SPEC(1, "Random", NULL, 246, 255, lj_mm_colrnd, 3) };
/* Profile wheel names (DMXTAB-MiniMACProfile V2); every name is an exe colour-table entry, bitmap assignment inferred. The Wash table gives
 * the same 12 positions unnamed: same slots, no bitmaps. */
#define LJ_MM_SLOTS(n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12) \
    LJ_PE(0, 0xFFFFFF, ""), LJ_PE(12, 0xFFD8A8, n1), LJ_PE(24, 0xFF80B0, n2), LJ_PE(36, 0xFF0000, n3), LJ_PE(48, 0x00C000, n4), LJ_PE(60, 0x00FFFF, n5), \
    LJ_PE(72, 0xFFFF00, n6), LJ_PE(84, 0xFF00FF, n7), LJ_PE(96, 0x0000FF, n8), LJ_PE(108, 0x00A040, n9), LJ_PE(120, 0x8000C0, n10), LJ_PE(132, 0xFF8000, n11), \
    LJ_PE(144, 0x0060FF, n12)
#define LJ_MM_COLOR1 { { LJ_MM_SLOTS("CTC 5500-3400", "Pink 312", "Red 301", "Green 206", "Cyan 104", "Yellow 603", "Magenta 507", "Blue 101", "Green 208", \
                                     "Purple 502", "Orange 306", "Blue 108") }, LJ_PAL_DEF }
#define LJ_MW_COLOR1 { { LJ_MM_SLOTS("", "", "", "", "", "", "", "", "", "", "", "") }, LJ_PAL_DEF }

/* ---- Gobos: TGoboControl_MiniMacP (Rotating Gobo, Rnd Slow / Med / Fast; IMG randomgobo.gif), ch 3 + 4 ----
 * Ch 3: 0-20 open; 21-35 gobo 1 (Fibroid texture glass), 36-50 gobo 2 (Radial dashes), 51-65 gobo 3 (Stardust 2), 66-80 gobo 4 (Atomic 2),
 * 81-95 gobo 5 (Highways 2), 96-110 gobo 6 (Decentered beam), 111-125 gobo 7 (Sunburst 507); 126-245 shake gobo 6 .. 1 slow -> fast;
 * 246-248 / 249-251 / 252-255 random fast / medium / slow. Ch 4: 0-2 none; 3-117 CW slow -> fast; 118-232 CCW fast -> slow; 233-239 none;
 * 240-255 swing slow -> fast (IMG goboswing.gif: the swing control; its bar caption is ours, PROPOSAL). */
static const lj_ctl_button lj_mp_gobornd[] = { { "Rnd Slow", 2, 252, 252, 255 }, { "Rnd Med", 2, 249, 249, 251 }, { "Rnd Fast", 2, 246, 246, 248 } };
static const lj_ctl_button lj_mp_gobrot[] = { { "Gobo Rotation CW", 3, 3, 3, 117 }, { "Gobo Rotation CCW", 3, 232, 118, 232 }, { "Stop", 3, 0, 0, 2 } };
static const lj_ctl_row lj_mp_gobos_rows[] = {
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Rotating Gobo", NULL, 0, 125), LJ_PALR(2, "Rotating Gobo", 2), LJ_SHAKE(2, "Goboshake", 126, 245),
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_ROT(3, "Gobo Rotation Speed", 3, 232, lj_mp_gobrot), LJ_BAR(3, "Gobo Swing", 240, 255, 0) };
static const lj_ctl_row lj_mp_gobos_specials[] = { LJ_SPEC(2, "Random", NULL, 246, 255, lj_mp_gobornd, 3) };
/* Gobo names = the exe gobo table (G74 RadialDashes, G127 StarDust2, G128 Atomic2, G129 HighWays2, G78 DecenteredBeam, G130 SunBurst_507);
 * gobo 1 "Fibroid texture glass" has no table entry. */
#define LJ_MP_GOBO1 { { LJ_PE(0, 0, "Open"), LJ_PE(21, 0, ""), LJ_PE(36, 0, "RadialDashes"), LJ_PE(51, 0, "StarDust2"), LJ_PE(66, 0, "Atomic2"), \
                        LJ_PE(81, 0, "HighWays2"), LJ_PE(96, 0, "DecenteredBeam"), LJ_PE(111, 0, "SunBurst_507") }, LJ_PAL_DEF }

/* ---- Extended: TSpecialSettings_MiniMacP / _MiniMacW (variants inferred, 15/00 s4) ----
 * P/T speed (Profile ch 9, Wash ch 7): 0-2 tracking, 3-245 fast -> slow, 246-248 PtS = SLO, 249-251 PtS = FSt, 252-255 blackout while moving.
 * Effects speed (Profile ch 10, Wash ch 8): 0-2 tracking, 3-251 fast -> slow, 252-255 blackout while moving. Stand-alone on ch 1. */
static const char *const lj_mm_pt_items[] = { "Tracking", "Speed", "Tracking Slow", "Tracking Fast", "B/O while moving", NULL };
static const char *const lj_mm_fx_items[] = { "Tracking", "Speed", "B/O while moving", NULL };
#define LJ_MM_EXT(name, p, f) \
    static const lj_ctl_button lj_mm_pt_##p[] = { { "Tracking", p, 0, 0, 2 }, { "Speed", p, 3, 3, 245 }, { "Tracking Slow", p, 246, 246, 248 }, \
                                                  { "Tracking Fast", p, 249, 249, 251 }, { "B/O while moving", p, 252, 252, 255 } }; \
    static const lj_ctl_button lj_mm_fx_##f[] = { { "Tracking", f, 0, 0, 2 }, { "Speed", f, 3, 3, 251 }, { "B/O while moving", f, 252, 252, 255 } }; \
    static const lj_ctl_row name[] = { LJ_FB(p, 1, LJ_HAND_OFF), LJ_RADIO(p, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, lj_mm_pt_##p, 5, lj_mm_pt_items), \
        LJ_FB(f, 1, LJ_HAND_OFF), LJ_RADIO(f, "Effect Settings", "Effect Speed", 3, 251, lj_mm_fx_##f, 3, lj_mm_fx_items), \
        LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 168, 187, lj_mm_sa, 2), LJ_SPEC_OS(0, "Fixture Reset", 208, 217, lj_mt_reset, 1) }
static const lj_ctl_button lj_mm_sa[] = { { "Sound", 0, 168, 168, 177 }, { "Auto", 0, 178, 178, 187 } };
LJ_MM_EXT(lj_mp_ext_rows, 8, 9);
LJ_MM_EXT(lj_mw_ext_rows, 6, 7);

/* ---- Maestro (QLC Mode 4): 1 shutter / strobe / reset / lamp (0-19 closed, 20-49 open, 50-112 strobe, 128-157 random strobe, 208-217 reset,
 * 228-237 lamp on, 248-255 lamp off); 2 image selection 0-19 / 20-39 / 40-59 / 60-79 images 1-4 indexed, 80-159 the same continuous,
 * 160-198 CW rainbow, 199-255 CCW rainbow; 3 image rotation 0-2 none, 3-127 CW, 128-252 CCW, 253-255 none; 4 fine index; 5 focus;
 * 6-9 pan/tilt 16 bit; 10 p/t speed (0-2, 3-245, 246-248 tracking slow, 249-251 tracking fast, 252-255 B/O); 11 image and focus speed
 * (0-2, 3-251, 252-255 B/O). TGoboControl_MiniMacM: Gobo 1, Index Gobo | Rotate Gobo, Wheel Scroll CW / CCW / Speed, Gobo Index,
 * Gobo Rotation CW / CCW / Speed. Stand-alone Auto: the MiniMAC value (178-187), inferred ("the Auto stand-alone command activates the
 * scenes programmed into the Maestro", NOTES; QLC lists 158-207 as shutter open). */
static const lj_ctl_button lj_mo_wscrl[]  = { { "Wheel Scroll CW", 1, 160, 160, 198 }, { "Wheel Scroll CCW", 1, 199, 199, 255 }, { "Stop", 1, 0, 0, 0 } };
static const lj_ctl_button lj_mo_rot[]    = { { "Gobo Rotation CW", 2, 3, 3, 127 }, { "Gobo Rotation CCW", 2, 252, 128, 252 }, { "Stop", 2, 0, 0, 2 } };
static const lj_ctl_row lj_mo_gobos_rows[] = {
    LJ_FB(1, 1, LJ_HAND_SNAP), LJ_SCRL(1, "Gobo 1", NULL, 0, 159), LJ_PALR(1, "Gobo 1", 2), LJ_MODE(2, "Index Gobo", "Rotate Gobo"),
    LJ_WSCRL(1, "Wheel Scroll Speed", "Wheel Scroll", 160, 255, lj_mo_wscrl),
    LJ_FB(2, 1, LJ_HAND_FADE), LJ_INDEX(2, "Gobo Index"), LJ_ROT(2, "Gobo Rotation Speed", 3, 252, lj_mo_rot) };
#define LJ_MO_GOBO1 { { LJ_PE_ROT(0, 80, ""), LJ_PE_ROT(20, 100, ""), LJ_PE_ROT(40, 120, ""), LJ_PE_ROT(60, 140, "") }, LJ_PAL_DEFROT }   /* images unnamed: customise per fixture (NOTES) */
static const lj_ctl_row lj_mo_beam_rows[] = { LJ_FB(4, 1, LJ_HAND_FADE), LJ_BAR(4, "Focus", 0, 255, 0) };
static const char *const lj_mo_pt_items[] = { "Tracking", "Speed", "Tracking Slow", "Tracking Fast", "B/O while moving", NULL };
static const lj_ctl_button lj_mo_pt[]  = { { "Tracking", 9, 0, 0, 2 }, { "Speed", 9, 3, 3, 245 }, { "Tracking Slow", 9, 246, 246, 248 }, { "Tracking Fast", 9, 249, 249, 251 },
                                           { "B/O while moving", 9, 252, 252, 255 } };
static const lj_ctl_button lj_mo_fx[]  = { { "Tracking", 10, 0, 0, 2 }, { "Speed", 10, 3, 3, 251 }, { "B/O while moving", 10, 252, 252, 255 } };
static const lj_ctl_button lj_mo_sa[]  = { { "Auto", 0, 178, 178, 187 } };
static const lj_ctl_row lj_mo_ext_rows[] = {
    LJ_FB(9, 1, LJ_HAND_OFF), LJ_RADIO(9, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, lj_mo_pt, 5, lj_mo_pt_items),
    LJ_FB(10, 1, LJ_HAND_OFF), LJ_RADIO(10, "Image/Focus Speed", "Effect Speed", 3, 251, lj_mo_fx, 3, lj_mm_fx_items),
    LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Stand Alone", 178, 187, lj_mo_sa, 1), LJ_SPEC_OS(0, "Fixture Reset", 208, 217, lj_mt_reset, 1) };

/* ---- windows ---- */
#define LJ_MM_INT(rows)  LJ_WIN(LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, rows, lj_mm_intensity_specials)
#define LJ_MM_COLORS     LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_mm_colors_rows, lj_mm_colors_specials)
static const lj_ctl_window lj_mp_windows[] = { LJ_MM_INT(lj_mm_intensity_rows), LJ_MT_MOVE16_WIN(4), LJ_MM_COLORS,
    LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_mp_gobos_rows, lj_mp_gobos_specials), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_mp_ext_rows) };
static const lj_ctl_window lj_pi_windows[] = { LJ_MM_INT(lj_mi_intensity_rows), LJ_MT_MOVE16_WIN(4), LJ_MM_COLORS,
    LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_mp_gobos_rows, lj_mp_gobos_specials), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_mp_ext_rows) };
static const lj_ctl_window lj_mw_windows[] = { LJ_MM_INT(lj_mm_intensity_rows), LJ_MT_MOVE16_WIN(2), LJ_MM_COLORS, LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_mw_ext_rows) };
static const lj_ctl_window lj_wi_windows[] = { LJ_MM_INT(lj_mi_intensity_rows), LJ_MT_MOVE16_WIN(2), LJ_MM_COLORS, LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_mw_ext_rows) };
static const lj_ctl_window lj_mo_windows[] = { LJ_MM_INT(lj_mm_intensity_rows), LJ_MT_MOVE16_WIN(5), LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_mo_gobos_rows),
    LJ_WIN0(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_mo_beam_rows), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_mo_ext_rows) };

/* ---- profiles ----
 * Shutter-only personalities: no intensity channel, Grand Master 0 % = blackout (shutter closed) (CHM L7163). Intensity Mode: ch 1 is the
 * intensity (0-38) and the Grand Master scales only that range (limit_gm), so strobe / lamp / reset values pass unscaled. */
#define LJ_MM_CH1 .manufacturer = "Martin", .fixture_id = "", .shutter = { 1, 0, "Shutter", 0, 20 }, .strobe = { 1, 0, "Strobe", 50, 112 }, .strobe_off = 20, \
    .blackout = { "", 1, { 1 }, { 0 } }, .lamp_on = { "", 1, { 1 }, { 228 } }, .lamp_off = { "", 1, { 1 }, { 248 } }, .reset = { "", 1, { 1 }, { 208 } }, \
    .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1
#define LJ_MM_SHUTTER_ONLY .bo_at_gm0 = 1
#define LJ_MM_INTENSITY    .intensity = { 1, 0, "Intens", 0, 38 }, .use_gm = 1, .limit_gm = 1
#define LJ_MP_FIELDS .color = { { 1, 1, "Color 1", 0, 150 } }, .gobo = { { 1, 2, "Gobo 1", 0, 125 } }, .palette = { LJ_MM_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_MP_GOBO1 }, \
    .pan_ofs = 4, .tilt_ofs = 6, .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
#define LJ_MW_FIELDS .color = { { 1, 1, "Color 1", 0, 150 } }, .palette = { LJ_MW_COLOR1 }, .pan_ofs = 2, .tilt_ofs = 4, .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1 }
/* Profile: 1 shutter, 2 colour, 3 gobo, 4 gobo rotation / swing, 5-8 pan/tilt 16 bit, 9 p/t speed, 10 effects speed. Wash: 1, 2, 3-6 pan/tilt, 7, 8. */
#define LJ_MP_CLASSES(c1) c1, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED
#define LJ_MW_CLASSES(c1) c1, LJ_CH_COLOR, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED
#define LJ_MM_DISPLAY "Fixture display settings: Pro = 2 or 4 (DMX protocol), PEr/LoF = On (DMX lamp on), PEr/RES = On (DMX reset on). "
#define LJ_MM_SHUTTER_NOTE "Intensity control disabled, shutter control only: programs are not compatible with the intensity-enabled personality. " \
    "Software 1.5 or later: PEr/Int/DiS. "
#define LJ_MM_INT_NOTE "Intensity control enabled (fixture software 1.5 or later, PEr/Int/EnA): programs are not compatible with the shutter-only personality. "

static const lj_profile lj_mm_profile = { .id = 54, .footprint = 10, .tag = "M-MAC-P", .name = "MiniMAC Profile", .mode = "Mode 2/4", LJ_MM_CH1, LJ_MM_SHUTTER_ONLY, LJ_MP_FIELDS,
    .channel_class = { LJ_MP_CLASSES(LJ_CH_SHUTTER) }, .windows = lj_mp_windows, .nwindows = LJ_NROWS(lj_mp_windows),
    .notes = LJ_MM_SHUTTER_NOTE LJ_MM_DISPLAY "PEr/TyP = Pro (profile). Mode 4 uses 10 DMX addresses." };
static const lj_profile lj_mm_profile_i = { .id = 65, .footprint = 10, .tag = "M-MAC-P", .name = "MiniMAC Profile", .mode = "Intensity Mode, DMXMode 2/4", LJ_MM_CH1,
    LJ_MM_INTENSITY, LJ_MP_FIELDS, .channel_class = { LJ_MP_CLASSES(LJ_CH_INTENSITY) }, .windows = lj_pi_windows, .nwindows = LJ_NROWS(lj_pi_windows),
    .notes = LJ_MM_INT_NOTE LJ_MM_DISPLAY "PEr/TyP = Pro (profile). Mode 4 uses 10 DMX addresses." };
static const lj_profile lj_mm_wash = { .id = 53, .footprint = 8, .tag = "M-MAC-W", .name = "MiniMAC Wash", .mode = "Mode 2/4", LJ_MM_CH1, LJ_MM_SHUTTER_ONLY, LJ_MW_FIELDS,
    .channel_class = { LJ_MW_CLASSES(LJ_CH_SHUTTER) }, .windows = lj_mw_windows, .nwindows = LJ_NROWS(lj_mw_windows),
    .notes = LJ_MM_SHUTTER_NOTE LJ_MM_DISPLAY "PEr/TyP = FrE (fresnel / wash). Mode 4 uses 8 DMX addresses." };
static const lj_profile lj_mm_wash_i = { .id = 66, .footprint = 8, .tag = "M-MAC-W", .name = "MiniMAC Wash", .mode = "Intensity Mode, DMXMode 2/4", LJ_MM_CH1,
    LJ_MM_INTENSITY, LJ_MW_FIELDS, .channel_class = { LJ_MW_CLASSES(LJ_CH_INTENSITY) }, .windows = lj_wi_windows, .nwindows = LJ_NROWS(lj_wi_windows),
    .notes = LJ_MM_INT_NOTE LJ_MM_DISPLAY "PEr/TyP = FrE (fresnel / wash). Mode 4 uses 8 DMX addresses." };
static const lj_profile lj_mm_maestro = { .id = 69, .footprint = 11, .tag = "MAESTRO", .name = "MiniMAC Maestro I & II", .mode = "", LJ_MM_CH1, LJ_MM_SHUTTER_ONLY,
    /* 1 shutter, 2 image, 3-4 image rotation / index MSB + LSB, 5 focus, 6-9 pan/tilt 16 bit, 10 p/t speed, 11 image and focus speed */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pair_lsb = { [2] = 1 + 3 }, .gobo = { { 1, 1, "Gobo 1", 0, 159 }, { 0 }, { 1, 2, "Index 1", 0, 255 } }, .focus = { 1, 4, "Focus", 0, 255 },
    .palette = { { { { 0 } }, LJ_PAL_DEF }, { { { 0 } }, LJ_PAL_DEF }, LJ_MO_GOBO1 }, .pan_ofs = 5, .tilt_ofs = 7,
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, .windows = lj_mo_windows, .nwindows = LJ_NROWS(lj_mo_windows),
    .notes = "To customise the gobo palette, right-click the fixture icon and select Customize Fixture. Shift and Ctrl enlarge the gobo index step. "
             "The Maestro has no built-in random stand-alone; the Auto stand-alone command runs the scenes programmed through the fixture display. 11 DMX addresses." };

const lj_profile *const lj_hand_minimacs[LJ_MINIMAC_COUNT] = { &lj_mm_profile, &lj_mm_profile_i, &lj_mm_wash, &lj_mm_wash_i, &lj_mm_maestro };
