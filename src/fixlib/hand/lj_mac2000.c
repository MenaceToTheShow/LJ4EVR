/* lj_mac2000.c - the Martin MAC 2000 family, transcribed from 15-builtin-profiles/04-mac2000-family.md.
 *   68  MAC2000    Martin / MAC 2000 Profile / 16 Bit: martin.com DMX table "MAC 2000 Profile DMX Protocol Version 1, implemented from
 *                  control software version 0.8" (16-bit column, 24 ch) and UM-MAC2000Profile Rev I p.26-29.
 *   84  MAC2000P   Martin / MAC 2000 Performance / 16 Bit: UM-MAC2000Performance Rev E p.40-43 Table 3 (16-bit, 31 ch).
 *   87  MAC2000WG  Martin / MAC 2000 Wash ,GCW / 16 Bit     (graduated colour wheel as colour wheel 2)
 *   88  MAC2000W   Martin / MAC 2000 Wash / 16 Bit          (second colour wheel installed)
 *   117 MAC2000WS  Martin / MAC 2000 Wash (Studio) / 16 Bit (dimmer wheel in place of colour wheel 2)
 *   161 MAC2000WXB Martin / MAC 2000 Wash XB / 16 bit, 162 MAC2000WXB Martin / MAC 2000 Wash XB (Studio) / 16 bit
 *                  UM-MAC2000Wash Rev L p.32-35 (16-bit, 21 ch; QLC Martin-MAC-2000-Wash.qxf "16-bit with Color Wheel 2" /
 *                  "with Dimmer Wheel" agrees); the XB runs the same protocol with the XB lamp (NOTES).
 *   188 MAC2000WS / 189 MAC2000W / 190 MAC2000WXB / 191 MAC2000WXB: the same four with "No Barndoors, 16 Bit": channels 10-15 (barndoors,
 *                  rotation, macros) dropped, footprint 15 (inferred from "no control channels for barndoors", NOTES; TBeamControl_mac2000W_NBD).
 * Captions are the exe literals of TIntensityControl_Mac2000 .. TSpecialSettings_Mac2000W_NBD [13 s2.6]. Every value = the low end
 * of its range (F47); the rotation buttons' value is the slow end (as lj_mac500.c). The random-CMY limit specials (Norm Off Full Min Max,
 * IMG specialcolorfunc.gif) are not exposed in W2 (ponytail: W4 fidelity sweep). */
#include "fixlib/hand/lj_mac2000.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Intensity: TIntensityControl_Mac2000 = the MAC 500 template (LJ_MT_INTENSITY_WIN). Studio: TIntensityControl_Mac2000WS / _WXBS:
 * "The first intensity control operates the primary intensity wheel, while the second intensity control operates the regular intensity" (NOTES). */
#define LJ_M20_SHUTTER_ROWS(name) \
    static const lj_ctl_button lj_m20_sh_##name[] = { { "Shutter Closed", 0, 0, 0, 19 }, { "Shutter Open", 0, 20, 20, 49 } }
LJ_M20_SHUTTER_ROWS(ws);
static const lj_ctl_row lj_ws_intensity_rows[] = {
    LJ_FB(0, 2, LJ_HAND_SNAP),
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_m20_sh_ws, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 50, 72, 1),
    LJ_FB(7, 1, LJ_HAND_FADE), LJ_BAR(7, "Intensity", 0, 255, 0),          /* ch 8 dimmer wheel (p.33) */
    LJ_FB(1, 1, LJ_HAND_FADE), LJ_BAR(1, "Int (Sec)", 0, 255, 0) };        /* ch 2 dimmer blades; default 255 (default_dmx) */

/* ---- Colors ---- */
#define LJ_M20_CMY LJ_FB(2, 1, LJ_HAND_FADE), LJ_BAR(2, "Cyan", 0, 255, 0), LJ_FB(3, 1, LJ_HAND_FADE), LJ_BAR(3, "Magenta", 0, 255, 0), \
                   LJ_FB(4, 1, LJ_HAND_FADE), LJ_BAR(4, "Yellow", 0, 255, 0), LJ_FB(5, 1, LJ_HAND_FADE), LJ_BAR(5, "CTC", 0, 255, 0)   /* ch 6 CTC cold -> warm (0-178 mireds) */
/* Profile ch 7 colour/gobo wheel: continuous 0 white, 20 pos 1 .. 140 pos 7, 160 open; stepped 160-191; 192-217 CW fast->slow; 218-243 CCW slow->fast;
 * 244-247 / 248-251 / 252-255 random CMY fast / medium / slow. */
static const lj_ctl_button lj_pf_colrot[] = { { "Wheel Scroll CW", 6, 192, 192, 217 }, { "Wheel Scroll CCW", 6, 218, 218, 243 }, { "Stop", 6, 0, 0, 0 } };
static const lj_ctl_button lj_m20_colrnd[] = { { "Rnd Slow", 6, 252, 252, 255 }, { "Rnd Med", 6, 248, 248, 251 }, { "Rnd Fast", 6, 244, 244, 247 } };
static const lj_ctl_row lj_m20_colors_specials[] = { LJ_SPEC(6, "Random", NULL, 244, 255, lj_m20_colrnd, 3) };
static const lj_ctl_row lj_pf_colors_rows[] = {        /* TColorControl_MAC2000 */
    LJ_M20_CMY, LJ_FB(6, 1, LJ_HAND_SNAP), LJ_SCRL(6, "Fixed Color", NULL, 0, 160), LJ_ROT(6, "Wheel Scroll Speed", 192, 243, lj_pf_colrot), LJ_PALR(6, "Color 1", 0) };
static const lj_ctl_row lj_pp_colors_rows[] = { LJ_M20_CMY };   /* TColorControl_Mac2000P: CMY + CTC, the Performance has no colour wheel (p.40) */
/* Wash ch 7 colour wheel 1 and ch 8 colour wheel 2 (p.33): continuous 0 white, 32 / 64 / 96 / 128 colours 1-4, 160 white; stepped 161-185;
 * 186-214 CW fast->slow; 215-243 CCW slow->fast; wheel 1 only: 244-255 random CMY. */
static const lj_ctl_button lj_ww_col1rot[] = { { "Wheel Scroll CW", 6, 186, 186, 214 }, { "Wheel Scroll CCW", 6, 215, 215, 243 }, { "Stop", 6, 0, 0, 0 } };
static const lj_ctl_button lj_ww_col2rot[] = { { "Wheel Scroll CW", 7, 186, 186, 214 }, { "Wheel Scroll CCW", 7, 215, 215, 243 }, { "Stop", 7, 0, 0, 0 } };
#define LJ_WW_WHEEL1 LJ_FB(6, 1, LJ_HAND_SNAP), LJ_SCRL(6, "Fixed Color", NULL, 0, 160), LJ_ROT(6, "Wheel Scroll Speed", 186, 243, lj_ww_col1rot), LJ_PALR(6, "Color 1", 0)
static const lj_ctl_row lj_wc_colors_rows[] = {        /* TColorControl_MAC2000WC: two Fixed Color rows, the second "Color 2" */
    LJ_M20_CMY, LJ_WW_WHEEL1,
    LJ_FB(7, 1, LJ_HAND_SNAP), LJ_SCRL(7, "Fixed Color", "Color 2", 0, 160), LJ_ROT(7, "Wheel Scroll Speed", 186, 243, lj_ww_col2rot), LJ_PALR(7, "Color 2", 1) };
static const lj_ctl_row lj_wg_colors_rows[] = {        /* TColorControl_MAC2000WG: wheel 2 graduated, no fixed slots (palette left empty) */
    LJ_M20_CMY, LJ_WW_WHEEL1,
    LJ_FB(7, 1, LJ_HAND_SNAP), LJ_SCRL(7, "Fixed Color", "Color 2", 0, 160), LJ_ROT(7, "Wheel Scroll Speed", 186, 243, lj_ww_col2rot) };
static const lj_ctl_row lj_ws_colors_rows[] = { LJ_M20_CMY, LJ_WW_WHEEL1 };   /* Studio: ch 8 is the dimmer wheel (Intensity window) */

/* ---- Gobos: TGoboControl_MAC2000 (Gobo 1, Gobo 2 rows) / _MAC2000P (Gobo 1 + the animation disc) ----
 * Gobo & function (Profile ch 8 / 11, Performance ch 7): 0-9 open; 10-34 gobo 1-5 indexed (5 each); 35-59 gobo 1-5 continuous rotation;
 * 60-134 indexed shake; 135-209 rotating shake; 210-232 wheel CW; 233-255 CCW. Position / velocity (ch 9 / 12, Perf ch 8): 0-255 index MSB,
 * or 0-2 none, 3-127 CCW slow->fast, 128-252 CW fast->slow, 253-255 none; fine position = the LSB on the next channel. */
#define LJ_M20_GOBO(s) \
    static const lj_ctl_button lj_m20_gscrl_##s[] = { { "Wheel Scroll CW", s, 210, 210, 232 }, { "Wheel Scroll CCW", s, 233, 233, 255 }, { "Stop", s, 0, 0, 0 } }; \
    static const lj_ctl_button lj_m20_grot_##s[]  = { { "Gobo Rotation CW", s + 1, 252, 128, 252 }, { "Gobo Rotation CCW", s + 1, 3, 3, 127 }, { "Stop", s + 1, 0, 0, 2 } }
#define LJ_M20_GOBO_ROWS(s, cap, pal) \
    LJ_FB(s, 1, LJ_HAND_SNAP), LJ_SCRL(s, cap, NULL, 0, 59), LJ_PALR(s, cap, pal), LJ_MODE(s + 1, "Index Gobo", "Rotate Gobo"), \
    LJ_WSCRL(s, "Wheel Scroll Speed", "Wheel Scroll", 210, 255, lj_m20_gscrl_##s), LJ_SHAKE(s, "Goboshake", 60, 209), \
    LJ_FB(s + 1, 1, LJ_HAND_FADE), LJ_INDEX(s + 1, "Gobo Index"), LJ_ROT(s + 1, "Gobo Rotation Speed", 3, 252, lj_m20_grot_##s)
LJ_M20_GOBO(7); LJ_M20_GOBO(10); LJ_M20_GOBO(6);
static const lj_ctl_row lj_pf_gobos_rows[] = { LJ_M20_GOBO_ROWS(7, "Gobo 1", 2), LJ_M20_GOBO_ROWS(10, "Gobo 2", 3) };

/* Performance animation wheel (p.41): ch 10 0-9 no effect (disc out), 10-19 vertical index, 20-29 horizontal index, 30-39 vertical rotation,
 * 40-49 horizontal rotation, 50-139 scroll V -> H indexing, 140-229 scroll H -> V rotation, 236-255 macros 1-5 (4 each);
 * ch 11 index 0-255, or 0-2 static, 3-127 CCW slow->fast, 128-252 CW fast->slow. */
static const lj_ctl_button lj_pp_anim_out[]   = { { "Animation Disc Out", 9, 0, 0, 9 } };
static const lj_ctl_button lj_pp_anim_macro[] = { { "Macro 1", 9, 236, 236, 239 }, { "Macro 2", 9, 240, 240, 243 }, { "Macro 3", 9, 244, 244, 247 }, { "Macro 4", 9, 248, 248, 251 }, { "Macro 5", 9, 252, 252, 255 } };
static const lj_ctl_button lj_pp_anim_rot[]   = { { "Rotation CW", 10, 252, 128, 252 }, { "Rotation CCW", 10, 3, 3, 127 }, { "Stop", 10, 0, 0, 2 } };
static const lj_ctl_row lj_pp_gobos_rows[] = {
    LJ_M20_GOBO_ROWS(6, "Gobo 1", 2),

    LJ_FB(9, 1, LJ_HAND_SNAP), LJ_BAR(9, "Pos.Index:", 50, 139, 0), LJ_BAR(9, "Pos.Rotation:", 140, 229, 0),
    LJ_FB(10, 1, LJ_HAND_FADE), LJ_INDEX(10, "Animation Disc Index"), LJ_ROT(10, "Rotation Speed", 3, 252, lj_pp_anim_rot) };
static const lj_ctl_row lj_pp_gobos_specials[] = { LJ_SPEC(9, "Animation Disc Out", NULL, 0, 9, lj_pp_anim_out, 1), LJ_SPEC(9, "Macro", NULL, 236, 255, lj_pp_anim_macro, 5) };

/* ---- Effects ----
 * Profile ch 14 (DMX table): 0 open; 1-175 variable frost; 176-183 / 184-191 effect 1 / 2 indexed; 192-199 / 200-207 effect 1 / 2 continuous;
 * 208-215 open; 216-255 macros 1-8 (5 each). Ch 15 position 0-255, or 0-2 none, 3-127 CCW slow->fast, 128-252 CW fast->slow. IMG effectscontrol1.gif. */
static const lj_ctl_button lj_pf_fx_rot[]   = { { "Rotation CW", 14, 252, 128, 252 }, { "Rotation CCW", 14, 3, 3, 127 }, { "Stop", 14, 0, 0, 2 } };
static const lj_ctl_button lj_pf_fx_macro[] = { { "Macro 1", 13, 216, 216, 220 }, { "Macro 2", 13, 221, 221, 225 }, { "Macro 3", 13, 226, 226, 230 }, { "Macro 4", 13, 231, 231, 235 },
                                                { "Macro 5", 13, 236, 236, 240 }, { "Macro 6", 13, 241, 241, 245 }, { "Macro 7", 13, 246, 246, 250 }, { "Macro 8", 13, 251, 251, 255 } };
static const lj_ctl_row lj_pf_fx_rows[] = {
    LJ_FB(13, 1, LJ_HAND_SNAP), LJ_SCRL(13, "Effects", NULL, 0, 207), LJ_PALR(13, "Effects", 4), LJ_MODE(14, "Index", "Rotate"),
    LJ_FB(14, 1, LJ_HAND_SNAP), LJ_INDEX(14, "Effect Index"), LJ_ROT(14, "Rotation Speed", 3, 252, lj_pf_fx_rot) };
static const lj_ctl_row lj_pf_fx_specials[] = { LJ_SPEC(13, "Macro ", NULL, 216, 255, lj_pf_fx_macro, 8) };   /* "Macro " = the exe literal; "Effect Macros combines Rotating Gobo and Rotating Effects" (NOTES); in-UI placement unverified */
/* Performance ch 12 (p.41): 0 open; 1-234 variable frost; 235-242 effect 1; 243-250 effect 2; 251-255 open. TEffectControl_Mac2000P: "Effects" only. */
static const lj_ctl_row lj_pp_fx_rows[] = { LJ_FB(11, 1, LJ_HAND_SNAP), LJ_SCRL(11, "Effects", NULL, 0, 250), LJ_PALR(11, "Effects", 4) };

/* ---- Beam ----
 * Iris (Profile ch 16, Performance ch 13): 0-199 open -> closed; 200-215 closed; 216-229 pulse opening; 230-243 pulse closing;
 * 244-246 / 247-249 / 250-252 / 253-255 random pulse opening fast / slow, closing fast / slow. Focus infinity -> near, zoom flood -> spot.
 * IMG beam2.gif "MAC 2000 beam control with iris pulse control": Iris, Focus, Zoom, Frost. */
#define LJ_M20_IRIS(c) \
    static const lj_ctl_button lj_m20_ip_##c[] = { { "Open", c, 216, 216, 229 }, { "Close", c, 230, 230, 243 } }; \
    static const lj_ctl_button lj_m20_rp_##c[] = { { "Open Fast", c, 244, 244, 246 }, { "Open Slow", c, 247, 247, 249 }, { "Close Fast", c, 250, 250, 252 }, { "Close Slow", c, 253, 253, 255 } }; \
    static const lj_ctl_row lj_m20_beam_spec_##c[] = { LJ_SPEC(c, "Iris Pulse", "Pulse Off", 216, 243, lj_m20_ip_##c, 2), LJ_SPEC(c, "Random Iris Pulse", NULL, 244, 255, lj_m20_rp_##c, 4) }
LJ_M20_IRIS(15); LJ_M20_IRIS(12);
static const lj_ctl_row lj_pf_beam_rows[] = {
    LJ_FB(15, 1, LJ_HAND_FADE), LJ_BAR(15, "Iris", 0, 199, 0), LJ_FB(16, 1, LJ_HAND_FADE), LJ_BAR(16, "Focus", 0, 255, 0),
    LJ_FB(17, 1, LJ_HAND_FADE), LJ_BAR(17, "Zoom", 0, 255, 0), LJ_FB(13, 1, LJ_HAND_FADE), LJ_BAR(13, "Frost", 1, 175, 0) };   /* "Variable frost cannot be combined with Effects" (NOTES) */
/* Performance framing shutters (p.41-42): ch 17/19/21/23 shutter 1-4 out -> in, ch 18/20/22/24 angle (0-126 -, 127-128 parallel, 129-255 +),
 * ch 25 rotate framing (right -> centre -> left). Bar captions are ours (PROPOSAL; the exe names only "Rotation Angle" and "Synchronize Shapers",
 * whose toggle is a UI feature not modelled here). */
static const lj_ctl_row lj_pp_beam_rows[] = {
    LJ_FB(12, 1, LJ_HAND_FADE), LJ_BAR(12, "Iris", 0, 199, 0), LJ_FB(14, 1, LJ_HAND_FADE), LJ_BAR(14, "Focus", 0, 255, 0),
    LJ_FB(15, 1, LJ_HAND_FADE), LJ_BAR(15, "Zoom", 0, 255, 0), LJ_FB(11, 1, LJ_HAND_FADE), LJ_BAR(11, "Frost", 1, 234, 0),
    LJ_FB(16, 1, LJ_HAND_FADE), LJ_BAR(16, "Shaper 1", 0, 255, 0), LJ_FB(17, 1, LJ_HAND_FADE), LJ_BAR(17, "Angle 1", 0, 255, 0),
    LJ_FB(18, 1, LJ_HAND_FADE), LJ_BAR(18, "Shaper 2", 0, 255, 0), LJ_FB(19, 1, LJ_HAND_FADE), LJ_BAR(19, "Angle 2", 0, 255, 0),
    LJ_FB(20, 1, LJ_HAND_FADE), LJ_BAR(20, "Shaper 3", 0, 255, 0), LJ_FB(21, 1, LJ_HAND_FADE), LJ_BAR(21, "Angle 3", 0, 255, 0),
    LJ_FB(22, 1, LJ_HAND_FADE), LJ_BAR(22, "Shaper 4", 0, 255, 0), LJ_FB(23, 1, LJ_HAND_FADE), LJ_BAR(23, "Angle 4", 0, 255, 0),
    LJ_FB(24, 1, LJ_HAND_FADE), LJ_BAR(24, "Rotation Angle", 0, 255, 0) };
/* Wash (p.33-34): ch 9 zoom 0-244 wide -> narrow, 252-255 open / hypermode; ch 10-13 barndoors upper, lower, left, right out -> in; ch 14 rotate
 * barndoors (127 = centre); ch 15 macros 21-40 all doors 50 %, 61-80 large 80 % / small 0 %, 101-120 small 80 % / large 0 %. */
static const lj_ctl_button lj_ww_zoom_open[] = { { "Open", 8, 252, 252, 255 } };
static const lj_ctl_button lj_ww_bd_center[] = { { "Center", 13, 127, 127, 127 } };
static const lj_ctl_button lj_ww_bd_macro[]  = { { "All 50%", 14, 21, 21, 40 }, { "Large 80%", 14, 61, 61, 80 }, { "Small 80%", 14, 101, 101, 120 } };   /* captions ours: PROPOSAL */
static const lj_ctl_row lj_ww_beam_rows[] = {           /* TBeamControl_Mac2000W */
    LJ_FB(8, 1, LJ_HAND_FADE), LJ_BAR(8, "Zoom", 0, 244, 0),
    LJ_FB(9, 1, LJ_HAND_FADE), LJ_BAR(9, "Barndoor (Upper)", 0, 255, 0), LJ_FB(10, 1, LJ_HAND_FADE), LJ_BAR(10, "Barndoor (Lower)", 0, 255, 0),
    LJ_FB(11, 1, LJ_HAND_FADE), LJ_BAR(11, "Barndoor (Left)", 0, 255, 0), LJ_FB(12, 1, LJ_HAND_FADE), LJ_BAR(12, "Barndoor (Right)", 0, 255, 0),
    LJ_FB(13, 1, LJ_HAND_FADE), LJ_BAR(13, "Barndoor Rotation", 0, 255, 0), LJ_FB(14, 1, LJ_HAND_SNAP) };
static const lj_ctl_row lj_ww_beam_specials[] = { LJ_SPEC(8, "Zoom", NULL, 252, 255, lj_ww_zoom_open, 1), LJ_SPEC(13, "Barndoor Rotation", NULL, 127, 127, lj_ww_bd_center, 1),
                                                  LJ_SPEC(14, "Barndoor Macros", NULL, 21, 120, lj_ww_bd_macro, 3) };
static const lj_ctl_row lj_nb_beam_rows[] = { LJ_FB(8, 1, LJ_HAND_FADE), LJ_BAR(8, "Zoom", 0, 244, 0) };   /* TBeamControl_mac2000W_NBD: Zoom, Open */
static const lj_ctl_row lj_nb_beam_specials[] = { LJ_SPEC(8, "Zoom", NULL, 252, 255, lj_ww_zoom_open, 1) };

/* ---- Extended: TSpecialSettings_Mac2000 / _Mac2000P / _Mac2000W / _Mac2000W_NBD [15/00 s4, variants chosen from the speed channel] ----
 * Effects speed, every model: 0-2 tracking, 3-239 fast->slow, 240-242 studio off, 243-245 studio on, 246-248 SCUT OFF, 249-251 SCUT ON, 252-255 fast. */
static const char *const lj_m20_fx_items[] = { "Tracking", "Speed", "Tracking - Studio Mode Disabled", "Tracking - Studio Mode Enabled", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
static const char *const lj_pf_pt_items[]  = { "Tracking", "Speed", "Tracking Slow", "Tracking Normal", "Tracking Fast", "B/O while moving", NULL };
static const char *const lj_pp_pt_items[]  = { "Tracking", "Speed", "Tracking slow", "Tracking normal", "Tracking fast", "Fan regulated", "Fan full", NULL };   /* p.43 ch 30 matches this exe variant */
static const char *const lj_ww_pt_items[]  = { "Tracking", "Speed", "Tracking Slow", "Tracking Normal", "Tracking Fast", NULL };   /* p.34: no blackout range */
#define LJ_M20_FX(f) static const lj_ctl_button lj_m20_fx_##f[] = { { "Tracking", f, 0, 0, 2 }, { "Speed", f, 3, 3, 239 }, \
    { "Tracking - Studio Mode Disabled", f, 240, 240, 242 }, { "Tracking - Studio Mode Enabled", f, 243, 243, 245 }, { "Shortcuts Disabled", f, 246, 246, 248 }, \
    { "Shortcuts Enabled", f, 249, 249, 251 }, { "B/O while moving", f, 252, 252, 255 } }
LJ_M20_FX(23); LJ_M20_FX(30); LJ_M20_FX(20); LJ_M20_FX(14);
/* Profile ch 23: 0-2, 3-242, 243-245 PTSP SLOW, 246-248 NORM, 249-251 FAST, 252-255 blackout. */
static const lj_ctl_button lj_pf_pt[] = { { "Tracking", 22, 0, 0, 2 }, { "Speed", 22, 3, 3, 242 }, { "Tracking Slow", 22, 243, 243, 245 }, { "Tracking Normal", 22, 246, 246, 248 },
                                          { "Tracking Fast", 22, 249, 249, 251 }, { "B/O while moving", 22, 252, 252, 255 } };
/* Performance ch 30: 0-2, 3-236, 237-239 slow, 240-242 normal, 243-245 fast, 246-248 fan regulated, 249-251 fan full, 252-255 blackout (not in the exe variant). */
static const lj_ctl_button lj_pp_pt[] = { { "Tracking", 29, 0, 0, 2 }, { "Speed", 29, 3, 3, 236 }, { "Tracking slow", 29, 237, 237, 239 }, { "Tracking normal", 29, 240, 240, 242 },
                                          { "Tracking fast", 29, 243, 243, 245 }, { "Fan regulated", 29, 246, 246, 248 }, { "Fan full", 29, 249, 249, 251 } };
/* Wash ch 20 (NBD ch 14): 0-2, 3-236, 237-239 slow, 240-242 normal, 243-245 fast. */
#define LJ_WW_PT(p) static const lj_ctl_button lj_ww_pt_##p[] = { { "Tracking", p, 0, 0, 2 }, { "Speed", p, 3, 3, 236 }, { "Tracking Slow", p, 237, 237, 239 }, \
    { "Tracking Normal", p, 240, 240, 242 }, { "Tracking Fast", p, 243, 243, 245 } }
LJ_WW_PT(19); LJ_WW_PT(13);
static const lj_ctl_row lj_pf_ext_rows[] = {
    LJ_FB(22, 1, LJ_HAND_OFF), LJ_RADIO(22, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 242, lj_pf_pt, 6, lj_pf_pt_items),
    LJ_FB(23, 1, LJ_HAND_OFF), LJ_RADIO(23, "Effects Settings", "Effect Speed", 3, 239, lj_m20_fx_23, 7, lj_m20_fx_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_pp_ext_rows[] = {
    LJ_FB(29, 1, LJ_HAND_OFF), LJ_RADIO(29, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 236, lj_pp_pt, 7, lj_pp_pt_items),
    LJ_FB(30, 1, LJ_HAND_OFF), LJ_RADIO(30, "Effects Settings", "Effect Speed", 3, 239, lj_m20_fx_30, 7, lj_m20_fx_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_ww_ext_rows[] = {
    LJ_FB(19, 1, LJ_HAND_OFF), LJ_RADIO(19, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 236, lj_ww_pt_19, 5, lj_ww_pt_items),
    LJ_FB(20, 1, LJ_HAND_OFF), LJ_RADIO(20, "Effects Settings", "Effect Speed", 3, 239, lj_m20_fx_20, 7, lj_m20_fx_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_nb_ext_rows[] = {
    LJ_FB(13, 1, LJ_HAND_OFF), LJ_RADIO(13, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 236, lj_ww_pt_13, 5, lj_ww_pt_items),
    LJ_FB(14, 1, LJ_HAND_OFF), LJ_RADIO(14, "Effects Settings", "Effect Speed", 3, 239, lj_m20_fx_14, 7, lj_m20_fx_items), LJ_MT_RESET_ROWS };

/* ---- windows ---- */
#define LJ_M20_COLORS(rows)        LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, rows, lj_m20_colors_specials)
#define LJ_M20_WS_INTENSITY        { LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_ws_intensity_rows, LJ_NROWS(lj_ws_intensity_rows), lj_mt_intensity_specials, 3 }
static const lj_ctl_window lj_pf_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(18), LJ_M20_COLORS(lj_pf_colors_rows), LJ_WIN0(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_pf_gobos_rows),
                                               LJ_WIN(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_pf_beam_rows, lj_m20_beam_spec_15), LJ_WIN(LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_std, lj_pf_fx_rows, lj_pf_fx_specials),
                                               LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_pf_ext_rows) };
static const lj_ctl_window lj_pp_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(25), LJ_WIN0(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_pp_colors_rows),
                                               LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_pp_gobos_rows, lj_pp_gobos_specials),
                                               LJ_WIN(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_pp_beam_rows, lj_m20_beam_spec_12), LJ_WIN0(LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_pp_fx_rows),
                                               LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_pp_ext_rows) };
#define LJ_WW_WINDOWS(name, intensity, colors, move, beam, beamspec, ext) \
    static const lj_ctl_window name[] = { intensity, LJ_MT_MOVE16_WIN(move), LJ_M20_COLORS(colors), LJ_WIN(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, beam, beamspec), LJ_MT_LAMP_WIN, LJ_EXT_WIN(ext) }
LJ_WW_WINDOWS(lj_wc_windows, LJ_MT_INTENSITY_WIN, lj_wc_colors_rows, 15, lj_ww_beam_rows, lj_ww_beam_specials, lj_ww_ext_rows);
LJ_WW_WINDOWS(lj_wg_windows, LJ_MT_INTENSITY_WIN, lj_wg_colors_rows, 15, lj_ww_beam_rows, lj_ww_beam_specials, lj_ww_ext_rows);
LJ_WW_WINDOWS(lj_ws_windows, LJ_M20_WS_INTENSITY, lj_ws_colors_rows, 15, lj_ww_beam_rows, lj_ww_beam_specials, lj_ww_ext_rows);
LJ_WW_WINDOWS(lj_nbc_windows, LJ_MT_INTENSITY_WIN, lj_wc_colors_rows, 9, lj_nb_beam_rows, lj_nb_beam_specials, lj_nb_ext_rows);
LJ_WW_WINDOWS(lj_nbs_windows, LJ_M20_WS_INTENSITY, lj_ws_colors_rows, 9, lj_nb_beam_rows, lj_nb_beam_specials, lj_nb_ext_rows);

/* ---- palettes (6.5): names from the DMX table / manuals; bitmaps = our tiles under those names (LORD_FIXTURES-9), PROPOSAL per slot ---- */
/* Profile colour/gobo wheel: open, pos 1 Red 308, 2 Green 201, 3 Blue 111, 4 UV, 5 Triple Cone, 6 Highways, 7 Dots (DMX table ch 7). */
#define LJ_PF_COLOR1 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(20, 0xFF2000, "Red 308"), LJ_PE(40, 0x00C060, "Green 201"), LJ_PE(60, 0x2040FF, "Blue 111"), LJ_PE(80, 0x6000FF, "UVPass"), \
                         LJ_PE(100, 0, "TripleCones"), LJ_PE(120, 0, "Highways"), LJ_PE(140, 0, "Dots") }, LJ_PAL_DEF }
/* Gobo wheel 1: Jet Fan, Pipe Dreams, Double Worms, Inverted King Star, Limbo; wheel 2: Limbo Donut, Triangle Cones, Congo Star, Space Sun, Waves. */
#define LJ_M20_GPAL(a, b, c, d, e) { { LJ_PE_ROT(0, 0, "Open"), LJ_PE_ROT(10, 35, a), LJ_PE_ROT(15, 40, b), LJ_PE_ROT(20, 45, c), LJ_PE_ROT(25, 50, d), LJ_PE_ROT(30, 55, e) }, LJ_PAL_DEFROT }
#define LJ_PF_GOBO1 LJ_M20_GPAL("jetfan", "pipedreams", "DoubleWorms", "invertedkingstar", "Limbo")
#define LJ_PF_GOBO2 LJ_M20_GPAL("limbodoughnut", "trianglecones", "congostar", "spacesun", "WAVES")
#define LJ_PF_FX1   { { LJ_PE_ROT(0, 0, "Open"), LJ_PE_ROT(176, 192, "3Facet"), LJ_PE_ROT(184, 200, "Textured") }, LJ_PAL_DEFROT }   /* IMG effectscontrol1.gif: open, 3-facet prism, textured */
#define LJ_PP_FX1   { { LJ_PE(0, 0, "Open"), LJ_PE(235, 0, "Effect 1"), LJ_PE(243, 0, "Effect 2") }, LJ_PAL_DEF }
/* Performance gobo wheel names: not in the extracted Table 3 (unnamed slots). */
#define LJ_PP_GOBO1 LJ_M20_GPAL("", "", "", "", "")
/* Wash colour wheel 1 / 2 (QLC names; manual "multi color" wheel): green, blue, UV, half minus green / red, orange, green, blue. */
#define LJ_WW_COLOR1 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(32, 0x00C000, ""), LJ_PE(64, 0x0000FF, ""), LJ_PE(96, 0x6000FF, "UVPass"), LJ_PE(128, 0xFFC0FF, "") }, LJ_PAL_DEF }
#define LJ_WW_COLOR2 { { LJ_PE(0, 0xFFFFFF, ""), LJ_PE(32, 0xFF0000, "Primary Red"), LJ_PE(64, 0xFF8000, ""), LJ_PE(96, 0x00C000, ""), LJ_PE(128, 0x0000FF, "") }, LJ_PAL_DEF }

/* ---- profiles ---- */
#define LJ_M20_COMMON .manufacturer = "Martin", .fixture_id = "", LJ_MT_CH12, .strobe = LJ_MT_STROBE, .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1, \
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1
#define LJ_R(n, c) LJ_R##n(c)
#define LJ_R1(c) c
#define LJ_R2(c) c, c
#define LJ_R4(c) c, c, c, c

static const lj_profile lj_pf_mac2000 = { .id = 68, .footprint = 24, .tag = "MAC2000", .name = "MAC 2000 Profile", .mode = "16 Bit", LJ_M20_COMMON,
    /* 1 shutter, 2 intensity, 3-5 CMY, 6 CTC, 7 colour/gobo wheel, 8-10 gobo 1 (sel, MSB, LSB), 11-13 gobo 2, 14-15 effect wheel, 16 iris, 17 focus,
     * 18 zoom, 19-22 pan/tilt 16 bit, 23-24 speed (DMX table V1) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R4(LJ_CH_COLOR), LJ_CH_COLOR, LJ_R(2, LJ_CH_GOBO), LJ_CH_GOBO, LJ_R(2, LJ_CH_GOBO), LJ_CH_GOBO,
                       LJ_R2(LJ_CH_EFFECT), LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pair_lsb = { [8] = 1 + 9, [11] = 1 + 12 },                                  /* gobo 1 / 2 index: MSB ch 9 / 12, LSB ch 10 / 13 */
    .color = { { 1, 6, "Color 1", 0, 160 }, { 0 }, { 1, 5, "CTC", 0, 255 } }, .gobo = { { 1, 7, "Gobo 1", 0, 59 }, { 1, 10, "Gobo 2", 0, 59 }, { 1, 8, "Index 1", 0, 255 }, { 1, 11, "Index 2", 0, 255 } },
    .effects = { { 1, 13, "Effects", 0, 207 }, { 0 }, { 1, 14, "Fx Index", 0, 255 } }, .iris = { 1, 15, "Iris", 0, 199 }, .focus = { 1, 16, "Focus", 0, 255 }, .zoom = { 1, 17, "Zoom", 0, 255 },
    .frost = { 1, 13, "Frost", 1, 175 }, .palette = { LJ_PF_COLOR1, { { { 0 } }, LJ_PAL_DEF }, LJ_PF_GOBO1, LJ_PF_GOBO2, LJ_PF_FX1 }, .pan_ofs = 18, .tilt_ofs = 20,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1) }, .windows = lj_pf_windows, .nwindows = LJ_NROWS(lj_pf_windows),
    .notes = "Gobo rotation and gobo index share control channels and cannot be programmed independently; Shift+Ctrl enlarge the index step. The 700 W reduction only works on "
             "fixtures with an electronic ballast. Variable frost cannot be combined with Effects. Effect Macros combine rotating gobo and rotating effects. 24 DMX addresses." };
static const lj_profile lj_pp_mac2000p = { .id = 84, .footprint = 31, .tag = "MAC2000P", .name = "MAC 2000 Performance", .mode = "16 Bit", LJ_M20_COMMON,
    /* 1 shutter, 2 intensity, 3-5 CMY, 6 CTC, 7-9 gobo 1 (sel, MSB, LSB), 10-11 animation wheel, 12 effect, 13 iris, 14 framing macros, 15 focus, 16 zoom,
     * 17-24 framing shutters, 25 framing rotation, 26-29 pan/tilt 16 bit, 30-31 speed (p.40-43) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R4(LJ_CH_COLOR), LJ_R2(LJ_CH_GOBO), LJ_CH_GOBO, LJ_R2(LJ_CH_EFFECT), LJ_CH_EFFECT, LJ_CH_BEAM, LJ_CH_SPECIAL, LJ_CH_BEAM, LJ_CH_BEAM,
                       LJ_R4(LJ_CH_BEAM), LJ_R4(LJ_CH_BEAM), LJ_CH_BEAM, LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED },
    .pair_lsb = { [7] = 1 + 8 },
    .color = { { 0 }, { 0 }, { 1, 5, "CTC", 0, 255 } }, .gobo = { { 1, 6, "Gobo 1", 0, 59 }, { 0 }, { 1, 7, "Index 1", 0, 255 }, { 1, 10, "Anim", 0, 255 } },
    .effects = { { 1, 11, "Effects", 0, 250 } }, .iris = { 1, 12, "Iris", 0, 199 }, .focus = { 1, 14, "Focus", 0, 255 }, .zoom = { 1, 15, "Zoom", 0, 255 },
    .frost = { 1, 11, "Frost", 1, 234 }, .palette = { { { { 0 } }, LJ_PAL_DEF }, { { { 0 } }, LJ_PAL_DEF }, LJ_PP_GOBO1, { { { 0 } }, LJ_PAL_DEF }, LJ_PP_FX1 }, .pan_ofs = 25, .tilt_ofs = 27,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R2(1), 1 }, .windows = lj_pp_windows, .nwindows = LJ_NROWS(lj_pp_windows),
    .notes = "Production DMX protocol version 1.0. Gobo rotation and gobo index share control channels. The 700 W reduction only works on fixtures with an electronic ballast. 31 DMX addresses." };

/* Wash family: 1 shutter, 2 intensity (dimmer blades), 3-5 CMY, 6 CTC, 7 colour wheel 1, 8 colour wheel 2 / graduated wheel / dimmer wheel, 9 zoom,
 * [10-13 barndoors, 14 barndoor rotation, 15 barndoor macros,] then pan/tilt 16 bit and the two speed channels (p.32-35). */
#define LJ_WW_CLASSES(c8) LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R4(LJ_CH_COLOR), LJ_CH_COLOR, c8, LJ_CH_BEAM
#define LJ_WW_BD          LJ_R4(LJ_CH_BEAM), LJ_CH_BEAM, LJ_CH_SPECIAL
#define LJ_WW_TAIL        LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED
#define LJ_WW_COMMON      LJ_M20_COMMON, .zoom = { 1, 8, "Zoom", 0, 244 }, .beam = { { 1, 9, "Barndoor", 0, 255 } }
#define LJ_WW_COLOR_WC    .color = { { 1, 6, "Color 1", 0, 160 }, { 1, 7, "Color 2", 0, 160 }, { 1, 5, "CTC", 0, 255 } }, .palette = { LJ_WW_COLOR1, LJ_WW_COLOR2 }
#define LJ_WW_COLOR_WG    .color = { { 1, 6, "Color 1", 0, 160 }, { 1, 7, "Color 2", 0, 160 }, { 1, 5, "CTC", 0, 255 } }, .palette = { LJ_WW_COLOR1 }
/* Studio: ch 8 dimmer wheel = the primary intensity (INTENSITY class, Grand Master); the blades on ch 2 = the secondary, default 255, left unscaled
 * (LEVEL class without ext_gm) so the master does not apply twice: PROPOSAL rig item. */
#define LJ_WW_STUDIO      .color = { { 1, 6, "Color 1", 0, 160 }, { 0 }, { 1, 5, "CTC", 0, 255 } }, .palette = { LJ_WW_COLOR1 }, .intensity = { 1, 7, "Intens", 0, 255 }, .default_dmx = { [1] = 255 }
#define LJ_WW_MT_CH12_STUDIO .shutter = { 1, 0, "Shutter", 0, 20 }, .strobe = LJ_MT_STROBE, .strobe_off = 20, .blackout = { "", 1, { 1 }, { 0 } }, \
    .lamp_on = { "", 1, { 1 }, { 228 } }, .lamp_off = { "", 1, { 1 }, { 248 } }, .reset = { "", 1, { 1 }, { 208 } }, .use_gm = 1, .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1, \
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1, .zoom = { 1, 8, "Zoom", 0, 244 }, .manufacturer = "Martin", .fixture_id = ""
#define LJ_WW21(id_, tag_, name_, mode_, colors_, windows_, notes_) { .id = id_, .footprint = 21, .tag = tag_, .name = name_, .mode = mode_, LJ_WW_COMMON, colors_, \
    .channel_class = { LJ_WW_CLASSES(LJ_CH_COLOR), LJ_WW_BD, LJ_WW_TAIL }, .pan_ofs = 15, .tilt_ofs = 17, \
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), 1 }, .windows = windows_, .nwindows = LJ_NROWS(windows_), .notes = notes_ }
#define LJ_WS21(id_, tag_, name_, mode_, notes_) { .id = id_, .footprint = 21, .tag = tag_, .name = name_, .mode = mode_, LJ_WW_MT_CH12_STUDIO, .beam = { { 1, 9, "Barndoor", 0, 255 } }, LJ_WW_STUDIO, \
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_LEVEL, LJ_R4(LJ_CH_COLOR), LJ_CH_COLOR, LJ_CH_INTENSITY, LJ_CH_BEAM, LJ_WW_BD, LJ_WW_TAIL }, .pan_ofs = 15, .tilt_ofs = 17, \
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), 1 }, .windows = lj_ws_windows, .nwindows = LJ_NROWS(lj_ws_windows), .notes = notes_ }
#define LJ_NB15(id_, tag_, name_, notes_) { .id = id_, .footprint = 15, .tag = tag_, .name = name_, .mode = "No Barndoors, 16 Bit", LJ_M20_COMMON, .zoom = { 1, 8, "Zoom", 0, 244 }, LJ_WW_COLOR_WC, \
    .channel_class = { LJ_WW_CLASSES(LJ_CH_COLOR), LJ_WW_TAIL }, .pan_ofs = 9, .tilt_ofs = 11, \
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R2(1), 1 }, .windows = lj_nbc_windows, .nwindows = LJ_NROWS(lj_nbc_windows), .notes = notes_ }
#define LJ_NS15(id_, tag_, name_, notes_) { .id = id_, .footprint = 15, .tag = tag_, .name = name_, .mode = "No Barndoors, 16 Bit", LJ_WW_MT_CH12_STUDIO, LJ_WW_STUDIO, \
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_LEVEL, LJ_R4(LJ_CH_COLOR), LJ_CH_COLOR, LJ_CH_INTENSITY, LJ_CH_BEAM, LJ_WW_TAIL }, .pan_ofs = 9, .tilt_ofs = 11, \
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R2(1), 1 }, .windows = lj_nbs_windows, .nwindows = LJ_NROWS(lj_nbs_windows), .notes = notes_ }
#define LJ_WW_RANDOM "Color Control Random Colors (CMY only): Norm, Off, Full, Min, Max limit the saturation of each CMY channel. "
#define LJ_WW_STUDIO_NOTE "The first intensity control operates the primary intensity (dimmer) wheel, the second the regular intensity; the secondary intensity defaults to 255. "

static const lj_profile lj_wg_mac2000wg = LJ_WW21(87, "MAC2000WG", "MAC 2000 Wash ,GCW", "16 Bit", LJ_WW_COLOR_WG, lj_wg_windows, LJ_WW_RANDOM "Graduated colour wheel installed as colour wheel 2. 21 DMX addresses.");
static const lj_profile lj_wc_mac2000w  = LJ_WW21(88, "MAC2000W", "MAC 2000 Wash", "16 Bit", LJ_WW_COLOR_WC, lj_wc_windows, LJ_WW_RANDOM "Second colour wheel installed. 21 DMX addresses.");
static const lj_profile lj_ws_mac2000ws = LJ_WS21(117, "MAC2000WS", "MAC 2000 Wash (Studio)", "16 Bit", LJ_WW_RANDOM LJ_WW_STUDIO_NOTE "21 DMX addresses.");
static const lj_profile lj_xb_mac2000wxb = LJ_WW21(161, "MAC2000WXB", "MAC 2000 Wash XB", "16 bit", LJ_WW_COLOR_WC, lj_wc_windows, LJ_WW_RANDOM "XB lamp; same protocol as the MAC 2000 Wash with colour wheel 2. 21 DMX addresses.");
static const lj_profile lj_xs_mac2000wxbs = LJ_WS21(162, "MAC2000WXB", "MAC 2000 Wash XB (Studio)", "16 bit", LJ_WW_RANDOM LJ_WW_STUDIO_NOTE "XB lamp. 21 DMX addresses.");
static const lj_profile lj_ns_mac2000ws_nbd = LJ_NS15(188, "MAC2000WS", "MAC 2000 Wash (Studio)", LJ_WW_STUDIO_NOTE "No control channels for barndoors. 15 DMX addresses (inferred).");
static const lj_profile lj_nb_mac2000w_nbd  = LJ_NB15(189, "MAC2000W", "MAC 2000 Wash", LJ_WW_RANDOM "No control channels for barndoors. 15 DMX addresses (inferred).");
static const lj_profile lj_ns_mac2000wxbs_nbd = LJ_NS15(190, "MAC2000WXB", "MAC 2000 Wash XB (Studio)", LJ_WW_STUDIO_NOTE "XB lamp, no control channels for barndoors. 15 DMX addresses (inferred).");
static const lj_profile lj_nb_mac2000wxb_nbd  = LJ_NB15(191, "MAC2000WXB", "MAC 2000 Wash XB", LJ_WW_RANDOM "XB lamp, no control channels for barndoors. 15 DMX addresses (inferred).");

const lj_profile *const lj_hand_mac2000s[LJ_MAC2000_COUNT] = { &lj_pf_mac2000, &lj_pp_mac2000p, &lj_wg_mac2000wg, &lj_wc_mac2000w, &lj_ws_mac2000ws, &lj_xb_mac2000wxb,
                                                               &lj_xs_mac2000wxbs, &lj_ns_mac2000ws_nbd, &lj_nb_mac2000w_nbd, &lj_ns_mac2000wxbs_nbd, &lj_nb_mac2000wxb_nbd };
