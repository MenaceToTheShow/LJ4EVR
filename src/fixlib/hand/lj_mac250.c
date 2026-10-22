/* lj_mac250.c - the Martin MAC 250 family, transcribed from 15-builtin-profiles/03-mac250-family.md.
 * Each profile's exe triple is on its line below. Sources per profile:
 *   28 MAC250 / 47 MAC250+  Martin / Mac 250 | MAC 250+ / Mode 2/4: UM-MAC250 ("MAC 250 and MAC 250+") p.21-23, Mode 4 = 13 ch,
 *                           standard gobo type (the notes set no PERS/GTYP), QLC Martin-MAC250plus.qxf Mode 4 agrees.
 *   39 MAC300               Martin / MAC 300 / Mode 2/4: QLC Martin-MAC300.qxf Mode 4 (13 ch); no Martin manual fetched: unverified.
 *   105 MAC 250KR           Martin / MAC 250 Krypton / 16 Bit: UM-MAC250KryptonEntour Rev K p.36-38, 16 Bt = 14 ch (not 16 Ex).
 *   112 MAC 250EN           Martin / MAC 250 Entour / 16 Bit: same manual p.39-42, 16 Bt = 15 ch (static gobo wheel 2 on ch 7).
 *   133 MAC250W             Martin / MAC 250 Wash / 16 Bit: UM-MAC250Wash Rev G p.27-28, 16 Bt = 13 ch (production protocol).
 *   48 MAC250B              Martin / MAC 250 Beam / 16 bit: OFL mac-250-beam.json "16-Bit" (13 ch: the Wash with the beam-kit wheel
 *                           in place of the colour wheel, same DMX positions); the beam-kit manual was not fetched: unverified.
 *   118 MAC250L             Martin / Mac 250 Laser, prototype rev.d / 16 Bit: no protocol source exists (MAC250LNotes.rtf not shipped).
 * Captions are the exe literals of TColorControl_Mac250 .. TEffectControl_Mac250L [13 s2.6]. Every button value = the low end of
 * its range (F47). Release note (CHM L11921): prism CW/CCW swapped "to match protocol specifications" = 20-79 CCW, 90-149 CW. */
#include "fixlib/hand/lj_mac250.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Colors, MAC 250 / 250+ ch 3 (p.21): continuous 0 white, 12 CTC, 24 yellow 603 .. 144 purple 502; 208-226 CW fast->slow,
 * 227-245 CCW slow->fast; 246-248 / 249-251 / 252-255 random fast / medium / slow. Krypton / Entour ch 3 (p.36 / p.39): 0, 11, .. 143. */
#define LJ_M2_COLROT(c) static const lj_ctl_button lj_m2_colrot_##c[] = { { "Wheel Scroll CW", c, 208, 208, 226 }, { "Wheel Scroll CCW", c, 227, 227, 245 }, { "Stop", c, 0, 0, 0 } }; \
                        static const lj_ctl_button lj_m2_colrnd_##c[] = { { "Rnd Slow", c, 252, 252, 255 }, { "Rnd Med", c, 249, 249, 251 }, { "Rnd Fast", c, 246, 246, 248 } }
LJ_M2_COLROT(2); LJ_M2_COLROT(5);
static const lj_ctl_row lj_m2_colors_rows[] = {      /* TColorControl_Mac250: Current Color scroll + 14-slot palette + wheel rotation */
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Color", NULL, 0, 144), LJ_ROT(2, "Wheel Scroll Speed", 208, 245, lj_m2_colrot_2), LJ_PALR(2, "Color 1", 0) };
static const lj_ctl_row lj_kr_colors_rows[] = {      /* TColorControl_Mac250KR (the Entour shares it, inferred) */
    LJ_FB(2, 1, LJ_HAND_SNAP), LJ_SCRL(2, "Current Color", NULL, 0, 143), LJ_ROT(2, "Wheel Scroll Speed", 208, 245, lj_m2_colrot_2), LJ_PALR(2, "Color 1", 0) };
static const lj_ctl_row lj_m2_colors_specials[] = { LJ_SPEC(2, "Random", NULL, 246, 255, lj_m2_colrnd_2, 3) };

/* CMY wheel fixtures (MAC 300, 250 Wash, 250 Beam): CMY on ch 3-5 (one Fade per bar), the wheel on ch 6.
 * MAC 300 (QLC) and 250 Wash (p.27) colour wheel: 0 white, 26 CTC 5500-2900, 52/51 pink 312, 78/77 UV, 104/102 red 308, 130/128 green 206,
 * 156/152 blue 108 (the Wash values below; the MAC 300 positions differ by one or two and are QLC data); stepped 180-207; 208-226 CW; 227-245 CCW. */
#define LJ_M2_CMY LJ_FB(2, 1, LJ_HAND_FADE), LJ_BAR(2, "Cyan", 0, 255, 0), LJ_FB(3, 1, LJ_HAND_FADE), LJ_BAR(3, "Magenta", 0, 255, 0), \
                  LJ_FB(4, 1, LJ_HAND_FADE), LJ_BAR(4, "Yellow", 0, 255, 0)
static const lj_ctl_row lj_m3_colors_rows[] = {      /* TColorControl_Mac300: Fixed Color + Random + CMY */
    LJ_M2_CMY, LJ_FB(5, 1, LJ_HAND_SNAP), LJ_SCRL(5, "Fixed Color", "Current Color", 0, 156), LJ_ROT(5, "Wheel Scroll Speed", 208, 245, lj_m2_colrot_5), LJ_PALR(5, "Color 1", 0) };
static const lj_ctl_row lj_ww_colors_rows[] = {      /* TColorControl_Mac250W: Color wheel Pos (hint "Colorwheel pos %d") + Wheel Scroll + CMY */
    LJ_M2_CMY, LJ_FB(5, 1, LJ_HAND_SNAP), LJ_SCRL(5, "Color wheel Pos", "Colorwheel pos %d", 0, 179), LJ_ROT(5, "Wheel Scroll Speed", 208, 245, lj_m2_colrot_5), LJ_PALR(5, "Color 1", 0) };
static const lj_ctl_row lj_bm_colors_rows[] = { LJ_M2_CMY };   /* TColorControl_Mac250Beam: CMY bars + random; the wheel is a gobo wheel */
static const lj_ctl_row lj_cmyw_colors_specials[] = { LJ_SPEC(5, "Random", NULL, 246, 255, lj_m2_colrnd_5, 3) };
/* The Wash / Beam "Norm | Full" random-CMY limit specials (ch 3-5: 1-127 minimum, 128-254 maximum) are not exposed: ponytail, W4 sweep. */

/* ---- Gobos ----
 * MAC 250 ch 4 (p.22, standard gobo type): 0-9 open, 10-89 gobo 1-8 (10 each), 90-209 shake gobo 8..1, 210-232 CW, 233-255 CCW wheel rotation;
 * ch 5 gobo rotation 0-2 none, 3-127 CW slow->fast, 128-252 CCW fast->slow, 253-255 none. */
static const lj_ctl_button lj_m2_wheel_rot[] = { { "Gobowheel Rotation CW", 3, 210, 210, 232 }, { "Gobowheel Rotation CCW", 3, 233, 233, 255 }, { "Stop", 3, 0, 0, 0 } };
static const lj_ctl_button lj_mp_wheel_rot[] = { { "Wheel Scroll CW", 3, 210, 210, 232 }, { "Wheel Scroll CCW", 3, 233, 233, 255 }, { "Stop", 3, 0, 0, 0 } };
static const lj_ctl_button lj_m2_gobo_rot[]  = { { "Gobo Rotation CW", 4, 3, 3, 127 }, { "Gobo Rotation CCW", 4, 252, 128, 252 }, { "Stop", 4, 0, 0, 2 } };   /* value = the slow end, as lj_mac500.c */
static const lj_ctl_row lj_m2_gobos_rows[] = {       /* TGoboControl_Mac250: Static Gobo, Gobowheel Rotation CW/CCW/Speed, Rotating Gobo */
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Static Gobo", NULL, 0, 89), LJ_WSCRL(3, "Gobowheel Rotation Speed", NULL, 210, 255, lj_m2_wheel_rot),
    LJ_SHAKE(3, "Goboshake", 90, 209), LJ_PALR(3, "Gobo 1", 2),
    LJ_FB(4, 1, LJ_HAND_FADE), LJ_ROT(4, "Rotating Gobo", 3, 252, lj_m2_gobo_rot) };   /* the class has no Gobo Rotation strings: row caption = its "Rotating Gobo" (inferred) */
static const lj_ctl_row lj_mp_gobos_rows[] = {       /* TGoboControl_Mac250Plus; Index/Rotate needs the indexed gobo type, which the notes do not set: not shown (unverified) */
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Rotating Gobo", NULL, 0, 89), LJ_WSCRL(3, "Wheel Scroll Speed", "Wheel Scroll", 210, 255, lj_mp_wheel_rot),
    LJ_SHAKE(3, "Goboshake", 90, 209), LJ_PALR(3, "Gobo 1", 2),
    LJ_FB(4, 1, LJ_HAND_FADE), LJ_ROT(4, "Gobo Rotation Speed", 3, 252, lj_m2_gobo_rot) };
/* Krypton / Entour ch 4 (p.37 / p.40): 0-4 open; 5-42 gobo 1-7 indexing (index on ch 5); 43-104 open + gobo 1-7 rotation (speed on ch 5);
 * 105-209 rotation with shake gobo 7..1; 210-232 scroll CW; 233-255 scroll CCW. Ch 5 index 0-395 deg or 0-2 none, 3-127 CW, 128-252 CCW, 253-255 none;
 * ch 6 = its LSB (16-bit index: Shift+Ctrl enlarge the step, NOTES). */
static const lj_ctl_row lj_kr_gobos_rows[] = {       /* TGoboControl_Mac250KR */
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Rotating Gobo", NULL, 0, 104), LJ_PALR(3, "Gobo 2", 3), LJ_MODE(4, "Index Gobo", "Rotate Gobo"),
    LJ_WSCRL(3, "Wheel Scroll Speed", "Wheel Scroll", 210, 255, lj_mp_wheel_rot), LJ_SHAKE(3, "Goboshake", 105, 209),
    LJ_FB(4, 1, LJ_HAND_FADE), LJ_INDEX(4, "Gobo Index"), LJ_ROT(4, "Gobo Rotation Speed", 3, 252, lj_m2_gobo_rot) };
/* Entour ch 7 static gobo wheel 2 (p.41): 0-7 open, 8-87 gobo 1-10 (8 each), 88-95 open, 96-205 shake gobo 10..open, 206-230 CW, 231-255 CCW. */
static const lj_ctl_button lj_en_fixed_rot[] = { { "Wheel Scroll CW", 6, 206, 206, 230 }, { "Wheel Scroll CCW", 6, 231, 231, 255 }, { "Stop", 6, 0, 0, 0 } };
static const lj_ctl_row lj_en_gobos_rows[] = {       /* TGoboControl_Mac250EN = the Krypton control + a Fixed Gobo row */
    LJ_FB(3, 1, LJ_HAND_SNAP), LJ_SCRL(3, "Rotating Gobo", NULL, 0, 104), LJ_PALR(3, "Gobo 2", 3), LJ_MODE(4, "Index Gobo", "Rotate Gobo"),
    LJ_WSCRL(3, "Wheel Scroll Speed", "Wheel Scroll", 210, 255, lj_mp_wheel_rot), LJ_SHAKE(3, "Goboshake", 105, 209),
    LJ_FB(4, 1, LJ_HAND_FADE), LJ_INDEX(4, "Gobo Index"), LJ_ROT(4, "Gobo Rotation Speed", 3, 252, lj_m2_gobo_rot),
    LJ_FB(6, 1, LJ_HAND_SNAP), LJ_SCRL(6, "Fixed Gobo", NULL, 0, 87), LJ_WSCRL(6, "Wheel Scroll Speed", "Wheel Scroll", 206, 255, lj_en_fixed_rot),
    LJ_SHAKE(6, "Goboshake", 96, 205), LJ_PALR(6, "Gobo 1", 2) };
/* Beam kit wheel on ch 6 (OFL): 0 open, 26 iris 50 %, 51 iris 25 %, 77 / 102 / 128 / 152 beam gobos; 208-226 CW, 227-245 CCW. */
static const lj_ctl_row lj_bm_gobos_rows[] = {       /* TGoboControl_MAC250Beam: Gobo wheel Pos (hint "Gobowheel pos %d") */
    LJ_FB(5, 1, LJ_HAND_SNAP), LJ_SCRL(5, "Gobo wheel Pos", "Gobowheel pos %d", 0, 179), LJ_ROT(5, "Wheel Scroll Speed", 208, 245, lj_m2_colrot_5), LJ_PALR(5, "Gobo 1", 2) };

/* ---- Beam: TBeamControl_Mac250 / _Mac250KR / _Mac250En = Focus only (infinity -> 2 m); MAC 300 / Wash / Beam = frost 0-255 (class unnamed) ---- */
static const lj_ctl_row lj_m2_beam_rows[] = { LJ_FB(5, 1, LJ_HAND_FADE), LJ_BAR(5, "Focus", 0, 255, 0) };
static const lj_ctl_row lj_kr_beam_rows[] = { LJ_FB(6, 1, LJ_HAND_FADE), LJ_BAR(6, "Focus", 0, 255, 0) };
static const lj_ctl_row lj_en_beam_rows[] = { LJ_FB(7, 1, LJ_HAND_FADE), LJ_BAR(7, "Focus", 0, 255, 0) };
static const lj_ctl_row lj_fr_beam_rows[] = { LJ_FB(6, 1, LJ_HAND_FADE), LJ_BAR(6, "Frost", 0, 255, 0) };

/* ---- Effects: TEffectControl_Mac250 / _Mac250KR / _Mac250EN4: prism (bitmap 3facet) + rotation + Macro 1..8 (IMG effectcontrolspecial.gif) ----
 * Prism (p.22, p.38, p.42): 0-19 off, 20-79 CCW fast->slow, 80-89 no rotation, 90-149 CW slow->fast, 150-215 off, 216-255 macros 1-8 (5 each). */
#define LJ_M2_PRISM(c) \
    static const lj_ctl_button lj_m2_prism_##c[]  = { { "3facet", c, 80, 80, 89 } }; \
    static const lj_ctl_button lj_m2_prot_##c[]   = { { "CW", c, 90, 90, 149 }, { "CCW", c, 79, 20, 79 }, { "Stop", c, 80, 80, 89 } }; \
    static const lj_ctl_button lj_m2_macro_##c[]  = { { "Macro 1", c, 216, 216, 220 }, { "Macro 2", c, 221, 221, 225 }, { "Macro 3", c, 226, 226, 230 }, { "Macro 4", c, 231, 231, 235 }, \
                                                      { "Macro 5", c, 236, 236, 240 }, { "Macro 6", c, 241, 241, 245 }, { "Macro 7", c, 246, 246, 250 }, { "Macro 8", c, 251, 251, 255 } }; \
    static const lj_ctl_row lj_m2_fx_rows_##c[]   = { LJ_FB(c, 1, LJ_HAND_SNAP), LJ_FXBTN(c, "Prism", 0, 89, lj_m2_prism_##c), LJ_ROT(c, "Speed", 20, 149, lj_m2_prot_##c) }; \
    static const lj_ctl_row lj_m2_fx_spec_##c[]   = { LJ_SPEC(c, "Macro", NULL, 216, 255, lj_m2_macro_##c, 8) }   /* group label not visible in the capture: PROPOSAL */
LJ_M2_PRISM(6); LJ_M2_PRISM(7); LJ_M2_PRISM(8);

/* ---- Extended: TSpecialSettings_Mac250 / _KR / _EN / _Mac300 / _Mac250W [15/00 s4, variants inferred] ----
 * 250 / 250+ / 300 p.23: PT 0-2 tracking, 3-245 fast->slow, 246-248 PTSP NORM, 249-251 PTSP FAST, 252-255 B/O; FX 0-2, 3-245, 246-248 SCUT OFF, 249-251 SCUT ON, 252-255 B/O.
 * Krypton / Entour / Wash (p.38, p.42, p.28): PT 3-242 speed, 243-245 PTSP SLOW, 246-248 NORM, 249-251 FAST, 252-255 B/O; FX as above (252-255 = maximum speed). */
static const char *const lj_m2_pt_items[]  = { "Tracking", "Speed", "Normal", "Fast", "B/O while moving", NULL };
static const char *const lj_kr_pt_items[]  = { "Tracking", "Speed", "Tracking Slow", "Tracking Normal", "Tracking Fast", "B/O while moving", NULL };
static const char *const lj_m2_fx_items[]  = { "Tracking", "Speed", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
#define LJ_M2_FXVALS(f) \
    static const lj_ctl_button lj_m2_fx_##f[] = { { "Tracking", f, 0, 0, 2 }, { "Speed", f, 3, 3, 245 }, { "Shortcuts Disabled", f, 246, 246, 248 }, { "Shortcuts Enabled", f, 249, 249, 251 }, \
                                                  { "B/O while moving", f, 252, 252, 255 } }
#define LJ_M2_EXT(p, f) \
    static const lj_ctl_button lj_m2_pt_##p[] = { { "Tracking", p, 0, 0, 2 }, { "Speed", p, 3, 3, 245 }, { "Normal", p, 246, 246, 248 }, { "Fast", p, 249, 249, 251 }, { "B/O while moving", p, 252, 252, 255 } }; \
    static const lj_ctl_row lj_m2_ext_##p[] = { LJ_FB(p, 1, LJ_HAND_OFF), LJ_RADIO(p, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 245, lj_m2_pt_##p, 5, lj_m2_pt_items), \
                                                LJ_FB(f, 1, LJ_HAND_OFF), LJ_RADIO(f, "Effects Settings", "Effect Speed", 3, 245, lj_m2_fx_##f, 5, lj_m2_fx_items), LJ_MT_RESET_ROWS }
#define LJ_KR_EXT(p, f) \
    static const lj_ctl_button lj_kr_pt_##p[] = { { "Tracking", p, 0, 0, 2 }, { "Speed", p, 3, 3, 242 }, { "Tracking Slow", p, 243, 243, 245 }, { "Tracking Normal", p, 246, 246, 248 }, \
                                                  { "Tracking Fast", p, 249, 249, 251 }, { "B/O while moving", p, 252, 252, 255 } }; \
    static const lj_ctl_row lj_kr_ext_##p[] = { LJ_FB(p, 1, LJ_HAND_OFF), LJ_RADIO(p, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 242, lj_kr_pt_##p, 6, lj_kr_pt_items), \
                                                LJ_FB(f, 1, LJ_HAND_OFF), LJ_RADIO(f, "Effects Settings", "Effect Speed", 3, 245, lj_m2_fx_##f, 5, lj_m2_fx_items), LJ_MT_RESET_ROWS }
LJ_M2_FXVALS(12); LJ_M2_FXVALS(13); LJ_M2_FXVALS(14);
LJ_M2_EXT(11, 12); LJ_KR_EXT(11, 12); LJ_KR_EXT(12, 13); LJ_KR_EXT(13, 14);

/* ---- windows ---- */
#define LJ_M2_COLORS(rows, spec) LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, rows, spec)
#define LJ_M2_GOBOS(rows)        LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, rows)
#define LJ_M2_BEAM(rows)         LJ_WIN0(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, rows)
#define LJ_M2_FX(c)              LJ_WIN(LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_m2_fx_rows_##c, lj_m2_fx_spec_##c)
static const lj_ctl_window lj_m2_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(7), LJ_M2_COLORS(lj_m2_colors_rows, lj_m2_colors_specials), LJ_M2_GOBOS(lj_m2_gobos_rows),
                                               LJ_M2_BEAM(lj_m2_beam_rows), LJ_M2_FX(6), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m2_ext_11) };
static const lj_ctl_window lj_mp_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(7), LJ_M2_COLORS(lj_m2_colors_rows, lj_m2_colors_specials), LJ_M2_GOBOS(lj_mp_gobos_rows),
                                               LJ_M2_BEAM(lj_m2_beam_rows), LJ_M2_FX(6), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m2_ext_11) };
static const lj_ctl_window lj_m3_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(7), LJ_M2_COLORS(lj_m3_colors_rows, lj_cmyw_colors_specials), LJ_M2_BEAM(lj_fr_beam_rows),
                                               LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m2_ext_11) };
static const lj_ctl_window lj_kr_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(8), LJ_M2_COLORS(lj_kr_colors_rows, lj_m2_colors_specials), LJ_M2_GOBOS(lj_kr_gobos_rows),
                                               LJ_M2_BEAM(lj_kr_beam_rows), LJ_M2_FX(7), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_kr_ext_12) };
static const lj_ctl_window lj_en_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(9), LJ_M2_COLORS(lj_kr_colors_rows, lj_m2_colors_specials), LJ_M2_GOBOS(lj_en_gobos_rows),
                                               LJ_M2_BEAM(lj_en_beam_rows), LJ_M2_FX(8), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_kr_ext_13) };
static const lj_ctl_window lj_ww_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(7), LJ_M2_COLORS(lj_ww_colors_rows, lj_cmyw_colors_specials), LJ_M2_BEAM(lj_fr_beam_rows),
                                               LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_kr_ext_11) };
static const lj_ctl_window lj_bm_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(7), LJ_M2_COLORS(lj_bm_colors_rows, lj_cmyw_colors_specials), LJ_M2_GOBOS(lj_bm_gobos_rows),
                                               LJ_M2_BEAM(lj_fr_beam_rows), LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_kr_ext_11) };
static const lj_ctl_window lj_la_windows[] = { LJ_LEVELS_WIN(18) };

/* ---- palettes (6.5): gel names from the manuals, spelled as the exe colour table where it has them [15/00 s3]; RGB ours ---- */
#define LJ_M2_COL12(s) LJ_PE(0, 0xFFFFFF, ""), LJ_PE(1 * s, 0xFFD8A8, ""), LJ_PE(2 * s, 0xFFE000, "Yellow 603"), LJ_PE(3 * s, 0x60A0FF, ""), LJ_PE(4 * s, 0xFFAAFF, "Pink 312"), \
                       LJ_PE(5 * s, 0x00C000, "Green 206"), LJ_PE(6 * s, 0x4060FF, "Blue 108"), LJ_PE(7 * s, 0xFF0000, "Red 301"), LJ_PE(8 * s, 0xFF00C0, "Magenta 507"), \
                       LJ_PE(9 * s, 0x0000FF, "Blue 101"), LJ_PE(10 * s, 0xFF8000, "Orange 306")   /* white, CTC (gel unnamed), Y603, Blue 104 (no exe bitmap), .. orange 306 */
#define LJ_M2_COLOR1 { { LJ_M2_COL12(12), LJ_PE(132, 0x40C040, "Green 202"), LJ_PE(144, 0x8000FF, "Purple 502") }, LJ_PAL_DEF }             /* MAC 250 / 250+ p.21: 13 positions */
#define LJ_KR_COLOR1 { { LJ_M2_COL12(11), LJ_PE(121, 0x006000, "Dark Green"), LJ_PE(132, 0x8000FF, "Purple 502"), LJ_PE(143, 0xFFFFFF, "") }, LJ_PAL_DEF }   /* Krypton / Entour p.36: 14 */
#define LJ_M2_CMYWHEEL(a, b, c, d, e, f) { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(a, 0xFFC080, "CTC 5500-2900"), LJ_PE(b, 0xFFAAFF, "Pink 312"), LJ_PE(c, 0x6000FF, "UVPass"), \
                                            LJ_PE(d, 0xFF2000, "Red 308"), LJ_PE(e, 0x00C000, "Green 206"), LJ_PE(f, 0x4060FF, "Blue 108") }, LJ_PAL_DEF }
/* Gobo slot names: our schematic tiles under the manuals' layout names (LORD_FIXTURES-9), PROPOSAL per slot. MAC 250 gobos: not in the extracted manual text. */
#define LJ_M2_GOBO1 { { LJ_PE(0, 0, "Open"), LJ_PE(10, 0, ""), LJ_PE(20, 0, ""), LJ_PE(30, 0, ""), LJ_PE(40, 0, ""), LJ_PE(50, 0, ""), LJ_PE(60, 0, ""), LJ_PE(70, 0, ""), LJ_PE(80, 0, "") }, LJ_PAL_DEF }
/* Krypton / Entour rotating wheel (p.19 Table 2 / 3): Eclipse, Fat bar, Fan, Happy Yins, Twisted triangle, Splash (Entour: Glass limbo), Aflobet. Def = index, Rot = rotation. */
#define LJ_KR_GOBO2(g6) { { LJ_PE_ROT(0, 43, "Open"), LJ_PE_ROT(5, 51, "Eclipse"), LJ_PE_ROT(11, 59, "Fat bar"), LJ_PE_ROT(16, 66, "Fan"), LJ_PE_ROT(21, 74, "Happy Yins"), \
                            LJ_PE_ROT(26, 82, "Twisted triangle"), LJ_PE_ROT(31, 90, g6), LJ_PE_ROT(36, 97, "Aflobet") }, LJ_PAL_DEFROT }
/* Entour fixed wheel (p.21 Table 4): Oriental fire, Wiggles, Pear drops, Brush strokes, Lashes, Splash, Crazed, Dibs, Sharp breakup (large), Zeds-a-leaping. */
#define LJ_EN_GOBO1 { { LJ_PE(0, 0, "Open"), LJ_PE(8, 0, "Oriental fire"), LJ_PE(16, 0, "Wiggles"), LJ_PE(24, 0, "Pear drops"), LJ_PE(32, 0, "Brush strokes"), LJ_PE(40, 0, "Lashes"), \
                        LJ_PE(48, 0, "Splash"), LJ_PE(56, 0, "Crazed"), LJ_PE(64, 0, "Dibs"), LJ_PE(72, 0, "Sharp breakup (large)"), LJ_PE(80, 0, "Zeds-a-leaping") }, LJ_PAL_DEF }
#define LJ_BM_GOBO1 { { LJ_PE(0, 0, "Open"), LJ_PE(26, 0, "Iris 50%"), LJ_PE(51, 0, "Iris 25%"), LJ_PE(77, 0, "Beam two dots"), LJ_PE(102, 0, "Beam three dots"), \
                        LJ_PE(128, 0, "Beam two bars 10"), LJ_PE(152, 0, "Beamcone 15") }, LJ_PAL_DEF }   /* OFL slot names */

/* ---- profiles. Channel classes per offset are on each line; pt16 pair = (pan_ofs, pan_ofs + 1) MSB, LSB. ---- */
#define LJ_M2_COMMON .manufacturer = "Martin", .fixture_id = "", LJ_MT_CH12, .strobe = LJ_MT_STROBE, .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1
#define LJ_M2_PRISM_FX(c) .effects = { { 1, c, "Prism", 0, 149 } }
#define LJ_INV13 .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }

static const lj_profile lj_m2_mac250 = { .id = 28, .footprint = 13, .tag = "MAC250", .name = "Mac 250", .mode = "Mode 2/4", LJ_M2_COMMON,
    /* 1 shutter, 2 dimmer, 3 colour, 4 gobo, 5 gobo rotation, 6 focus, 7 prism, 8-11 pan/tilt, 12-13 speed (p.21-23) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .color = { { 1, 2, "Color 1", 0, 144 } }, .gobo = { { 1, 3, "Gobo 1", 0, 89 }, { 0 }, { 1, 4, "Rotate", 0, 255 } }, .focus = { 1, 5, "Focus", 0, 255 }, LJ_M2_PRISM_FX(6),
    .palette = { LJ_M2_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_M2_GOBO1 }, .pan_ofs = 7, .tilt_ofs = 9, LJ_INV13,
    .windows = lj_m2_windows, .nwindows = LJ_NROWS(lj_m2_windows),
    .notes = "Fixture settings: PSEt/MOdE = 2 or 4, PERS/dLOF = on, PERS/FTYP = PRIS, PERS/dRES = on. Mode 4 uses 13 DMX addresses. Reset and lamp off need CTC, prism in and the open gobo when DMX reset / lamp off are disabled in the menu." };
static const lj_profile lj_m2_mac250p = { .id = 47, .footprint = 13, .tag = "MAC250+", .name = "MAC 250+", .mode = "Mode 2/4", LJ_M2_COMMON,
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .color = { { 1, 2, "Color 1", 0, 144 } }, .gobo = { { 1, 3, "Gobo 1", 0, 89 }, { 0 }, { 1, 4, "Rotate", 0, 255 } }, .focus = { 1, 5, "Focus", 0, 255 }, LJ_M2_PRISM_FX(6),
    .palette = { LJ_M2_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_M2_GOBO1 }, .pan_ofs = 7, .tilt_ofs = 9, LJ_INV13,
    .windows = lj_mp_windows, .nwindows = LJ_NROWS(lj_mp_windows),
    .notes = "Fixture settings: PSEt/MOdE = 2 or 4, PERS/dLOF = on, PERS/FTYP = PRIS, PERS/dRES = on. Mode 4 uses 13 DMX addresses. Clicking the active rotation direction again stops the rotation (2.5)." };
static const lj_profile lj_m3_mac300 = { .id = 39, .footprint = 13, .tag = "MAC300", .name = "MAC 300", .mode = "Mode 2/4", LJ_M2_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 colour, 7 frost, 8-11 pan/tilt, 12-13 speed (QLC Mode 4) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1,
    .color = { { 1, 5, "Color 1", 0, 156 } }, .frost = { 1, 6, "Frost", 0, 255 }, .palette = { LJ_M2_CMYWHEEL(26, 52, 78, 104, 130, 156) }, .pan_ofs = 7, .tilt_ofs = 9, LJ_INV13,
    .windows = lj_m3_windows, .nwindows = LJ_NROWS(lj_m3_windows),
    .notes = "Fixture settings: PSEt/MOdE = 2 or 4, PERS/dLOF = on, PERS/dRES = on. Mode 4 uses 13 DMX addresses (layout from the QLC+ definition, unverified against a Martin table)." };
static const lj_profile lj_kr_mac250kr = { .id = 105, .footprint = 14, .tag = "MAC 250KR", .name = "MAC 250 Krypton", .mode = "16 Bit", LJ_M2_COMMON,
    /* 1 shutter, 2 dimmer, 3 colour, 4 gobo, 5 gobo rotation / index MSB, 6 its LSB, 7 focus, 8 prism, 9-12 pan/tilt, 13-14 speed (p.36-38) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pair_lsb = { [4] = 1 + 5 },                                         /* gobo index 16 bit: ch 5 MSB, ch 6 LSB */
    .color = { { 1, 2, "Color 1", 0, 143 } }, .gobo = { { 0 }, { 1, 3, "Gobo 2", 0, 104 }, { 1, 4, "Index", 0, 255 } }, .focus = { 1, 6, "Focus", 0, 255 }, LJ_M2_PRISM_FX(7),
    .palette = { LJ_KR_COLOR1, { { { 0 } }, LJ_PAL_DEF }, { { { 0 } }, LJ_PAL_DEF }, LJ_KR_GOBO2("Splash") }, .pan_ofs = 8, .tilt_ofs = 10,
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, .windows = lj_kr_windows, .nwindows = LJ_NROWS(lj_kr_windows),
    .notes = "16 bit DMX mode (not 16 bit extended mode). Gobo rotation and gobo index share control channels and cannot be programmed independently; Shift+Ctrl enlarge the index step. "
             "Fixture settings: PSET = 16BT, dLOF = ON, dRES = ON. 14 DMX addresses." };
static const lj_profile lj_en_mac250en = { .id = 112, .footprint = 15, .tag = "MAC 250EN", .name = "MAC 250 Entour", .mode = "16 Bit", LJ_M2_COMMON,
    /* as the Krypton with 7 = static gobo wheel 2, then 8 focus, 9 prism, 10-13 pan/tilt, 14-15 speed (p.39-42) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pair_lsb = { [4] = 1 + 5 },
    .color = { { 1, 2, "Color 1", 0, 143 } }, .gobo = { { 1, 6, "Gobo 1", 0, 87 }, { 1, 3, "Gobo 2", 0, 104 }, { 1, 4, "Index", 0, 255 } }, .focus = { 1, 7, "Focus", 0, 255 }, LJ_M2_PRISM_FX(8),
    .palette = { LJ_KR_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_EN_GOBO1, LJ_KR_GOBO2("Glass limbo") }, .pan_ofs = 9, .tilt_ofs = 11,
    .level_invisible = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, .windows = lj_en_windows, .nwindows = LJ_NROWS(lj_en_windows),
    .notes = "16 bit DMX mode (not 16 bit extended mode). Gobo rotation and gobo index share control channels and cannot be programmed independently. "
             "Fixture settings: PSET = 16BT, PERS/GTYP = SCRL, dLOF = ON, dRES = ON. 15 DMX addresses." };
static const lj_profile lj_ww_mac250w = { .id = 133, .footprint = 13, .tag = "MAC250W", .name = "MAC 250 Wash", .mode = "16 Bit", LJ_M2_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 colour, 7 frost, 8-11 pan/tilt, 12-13 speed (p.27-28) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1,
    .color = { { 1, 5, "Color 1", 0, 179 } }, .frost = { 1, 6, "Frost", 0, 255 }, .palette = { LJ_M2_CMYWHEEL(26, 51, 77, 102, 128, 152) }, .pan_ofs = 7, .tilt_ofs = 9, LJ_INV13,
    .windows = lj_ww_windows, .nwindows = LJ_NROWS(lj_ww_windows),
    .notes = "Production DMX protocol. Fixture settings: PSET = 16BT, PERS/dRES = ON, PERS/dLOF = ON. 13 DMX addresses. Reset and lamp off need CMY at 230-232 when disabled in the menu." };
static const lj_profile lj_bm_mac250b = { .id = 48, .footprint = 13, .tag = "MAC250B", .name = "MAC 250 Beam", .mode = "16 bit", LJ_M2_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 gobo (beam-kit) wheel, 7 frost, 8-11 pan/tilt, 12-13 speed (OFL 16-Bit) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_COLOR, LJ_CH_GOBO, LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1,
    .gobo = { { 1, 5, "Gobo 1", 0, 179 } }, .frost = { 1, 6, "Frost", 0, 255 }, .palette = { { { { 0 } }, LJ_PAL_DEF }, { { { 0 } }, LJ_PAL_DEF }, LJ_BM_GOBO1 },
    .pan_ofs = 7, .tilt_ofs = 9, LJ_INV13, .windows = lj_bm_windows, .nwindows = LJ_NROWS(lj_bm_windows),
    .notes = "MAC 250 Wash with the beam kit: the colour wheel carries beam gobos. Fixture settings: PSET = 16BT, PERS/dRES = ON, PERS/dLOF = ON. 13 DMX addresses (OFL layout, unverified)." };
/* Mac 250 Laser prototype: no DMX protocol source (MAC250LNotes.rtf not shipped; TEffectControl_Mac250L names Animation Bank#, Animation#, Size,
 * Speed, 3facet). Levels-only over 18 channels = the Krypton's 14 + those four: PROPOSAL rig item (footprint and layout unverified). */
static const lj_profile lj_la_mac250l = { .id = 118, .footprint = 18, .tag = "MAC250L", .name = "Mac 250 Laser, prototype rev.d", .mode = "16 Bit",
    .manufacturer = "Martin", .fixture_id = "", .channel_class = { LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL,
                                                                   LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL, LJ_CH_LEVEL },
    .blackout = { "", 1, { 0 }, { 0 } }, .windows = lj_la_windows, .nwindows = 1,
    .notes = "Prototype profile (rev.d) with no published DMX protocol: generic level control of 18 channels until the layout is verified." };

const lj_profile *const lj_hand_mac250s[LJ_MAC250_COUNT] = { &lj_m2_mac250, &lj_m2_mac250p, &lj_m3_mac300, &lj_kr_mac250kr, &lj_en_mac250en, &lj_ww_mac250w, &lj_bm_mac250b, &lj_la_mac250l };
