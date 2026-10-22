/* lj_mac550.c - Martin MAC 550 / 575 Krypton / 700 Profile / 700 Wash (16-bit basic modes), transcribed from
 * 15-builtin-profiles/09-mac550-575-700.md.
 *   110 MAC550   Martin / MAC 550 / 16 Bit (Mac550Notes.rtf): UM-MAC550 Rev E p.28-32, 16-bit basic = 21 ch.
 *   155 MAC575K  Martin / MAC 575 Krypton / 16 Bit (MAC575KryptonNotes.rtf): UM-MAC575Krypton Rev C p.29-34, 16-bit basic = 19 ch.
 *   116 MAC 700P Martin / MAC 700 Profile / 16 Bit (Mac700PNotes.rtf): UM-MAC700Profile Rev G p.29-34, 16-bit basic = 23 ch
 *                (QLC+ Martin-MAC-700-Profile.qxf "16-bit Basic" agrees value for value).
 *   140 MAC 700W Martin / MAC 700 Wash / 16 Bit (Mac700WNotes.rtf): UM-MAC700Wash Rev D p.23-25, 16-bit basic = 16 ch (QLC+ agrees).
 * Channel 1 / 2 are the MAC 500 template on all four (no _Mac550/575/700 intensity class exists; reset 208-217, lamp on 228-237, lamp off
 * 248-255, p.28 / p.30 / p.29 / p.23), so the Intensity window is LJ_MT_INTENSITY_WIN. The MAC 700 adds 238-242 400 W and 243-247 700 W mode
 * (TLampControl_MAC700, exe strings 400W / 700W after the class). Captions are the exe literals of TColorControl_Mac550 / _MAC700P / _MAC700W,
 * TGoboControl_MAC550 / _MAC575K / _MAC700P, TBeamControl_Mac575K / _Mac700W, TEffectControl_Mac550 / _Mac575K [13 s2.6]; classes without
 * captions of their own (TBeamControl_Mac550 / _Mac700P, TEffectControl_Mac700P) reuse the 575's (inferred). Every value = the low end of
 * its range (F47); rotation buttons carry the slow end (lj_profile.h row conventions). */
#include "fixlib/hand/lj_mac550.h"
#include "fixlib/hand/lj_martin.h"

/* ---- Colors ----
 * Colour wheel (550 ch 3 / 4, 575 ch 3 / 4, 700P ch 6, 700W ch 7): continuous 0 open, 17 slot 1 .. 136 slot 8 (17 each), 153 open;
 * stepped 154-198 (slot 8 -> 1, 5 each); 199-219 CW fast -> slow; 220-240 CCW slow -> fast; 241-245 / 246-250 / 251-255 random fast / med / slow. */
#define LJ_M5_WHEEL(c) \
    static const lj_ctl_button lj_m5_colrot_##c[] = { { "Wheel Scroll CW", c, 199, 199, 219 }, { "Wheel Scroll CCW", c, 220, 220, 240 }, { "Stop", c, 0, 0, 0 } }; \
    static const lj_ctl_button lj_m5_colrnd_##c[] = { { "Rnd Slow", c, 251, 251, 255 }, { "Rnd Med", c, 246, 246, 250 }, { "Rnd Fast", c, 241, 241, 245 } }
LJ_M5_WHEEL(2); LJ_M5_WHEEL(3); LJ_M5_WHEEL(5); LJ_M5_WHEEL(6);
#define LJ_M5_WHEEL_ROWS(c, cap, cap2, palcap, pal) LJ_FB(c, 1, LJ_HAND_SNAP), LJ_SCRL(c, cap, cap2, 0, 153), LJ_ROT(c, "Wheel Scroll Speed", 199, 240, lj_m5_colrot_##c), \
    LJ_PALR(c, palcap, pal)
#define LJ_M5_CMY LJ_FB(2, 1, LJ_HAND_FADE), LJ_BAR(2, "Cyan", 0, 255, 0), LJ_FB(3, 1, LJ_HAND_FADE), LJ_BAR(3, "Magenta", 0, 255, 0), \
                  LJ_FB(4, 1, LJ_HAND_FADE), LJ_BAR(4, "Yellow", 0, 255, 0)
static const lj_ctl_row lj_m5_colors_rows[] = {        /* TColorControl_Mac550: Current Color x2, Rnd Slow / Med / Fast (575 shares it, inferred) */
    LJ_M5_WHEEL_ROWS(2, "Current Color", NULL, "Color 1", 0), LJ_M5_WHEEL_ROWS(3, "Current Color", "Color 2", "Color 2", 1) };
static const lj_ctl_row lj_m5_colors_specials[] = { LJ_SPEC(2, "Random", NULL, 241, 255, lj_m5_colrnd_2, 3), LJ_SPEC(3, "Random", "Color 2", 241, 255, lj_m5_colrnd_3, 3) };
/* MAC 700: macro channel (700P ch 11, 700W ch 9) 136-175 / 176-215 / 216-255 random CMY fast / medium / slow. The random-CMY limit specials
 * (Random CMY, Norm, Full) are not exposed (as the MAC 2000, lj_mac2000.c; W4 sweep). */
#define LJ_M7_RCMY(m) static const lj_ctl_button lj_m7_rcmy_##m[] = { { "Rnd Slow", m, 216, 216, 255 }, { "Rnd Med", m, 176, 176, 215 }, { "Rnd Fast", m, 136, 136, 175 } }
LJ_M7_RCMY(10); LJ_M7_RCMY(8);
static const lj_ctl_row lj_7p_colors_rows[] = { LJ_M5_CMY, LJ_M5_WHEEL_ROWS(5, "Fixed Color", "Current Color", "Color 1", 0) };   /* TColorControl_MAC700P */
static const lj_ctl_row lj_7p_colors_specials[] = { LJ_SPEC(5, "Random Color (Colorwheel)", NULL, 241, 255, lj_m5_colrnd_5, 3),
                                                    LJ_SPEC(10, "Random Color (CMY)", NULL, 136, 255, lj_m7_rcmy_10, 3) };
static const lj_ctl_row lj_7w_colors_rows[] = { LJ_M5_CMY, LJ_FB(5, 1, LJ_HAND_FADE), LJ_BAR(5, "CTC", 0, 255, 0),   /* ch 6 CTC white -> CTC; bar caption ours */
                                                LJ_M5_WHEEL_ROWS(6, "Fixed Color", "Current Color", "Color 1", 0) };       /* TColorControl_MAC700W */
static const lj_ctl_row lj_7w_colors_specials[] = { LJ_SPEC(6, "Random Color (Colorwheel)", NULL, 241, 255, lj_m5_colrnd_6, 3),
                                                    LJ_SPEC(8, "Random Color (CMY)", NULL, 136, 255, lj_m7_rcmy_8, 3) };

/* ---- Gobos ----
 * Rotating wheel (550 / 575 ch 5, 700P ch 7): 0-11 open; 12-35 gobo 1-6 indexed (4 each); 36-59 gobo 1-6 continuous rotation; 60-131 indexed +
 * shake; 132-203 rotating + shake (gobo 6 -> 1); 204-229 CW scroll slow -> fast; 230-255 CCW scroll fast -> slow. Rotation (next channel): index
 * 0-255, or 0-2 none, 3-127 CW slow -> fast, 128-252 CCW fast -> slow, 253-255 none; the fine index is the LSB on the channel after. */
#define LJ_M5_GOBO(s) \
    static const lj_ctl_button lj_m5_gscrl_##s[] = { { "Rotating Wheel Scroll CW", s, 204, 204, 229 }, { "Rotating Wheel Scroll CCW", s, 230, 230, 255 }, { "Stop", s, 0, 0, 0 } }; \
    static const lj_ctl_button lj_m5_grot_##s[]  = { { "Gobo Rotation CW", s + 1, 3, 3, 127 }, { "Gobo Rotation CCW", s + 1, 252, 128, 252 }, { "Stop", s + 1, 0, 0, 2 } }
LJ_M5_GOBO(4); LJ_M5_GOBO(6);
#define LJ_M5_GOBO_ROWS(s) \
    LJ_FB(s, 1, LJ_HAND_SNAP), LJ_SCRL(s, "Rotating Gobo", NULL, 0, 59), LJ_PALR(s, "Rotating Gobo", 2), LJ_MODE(s + 1, "Index Gobo", "Rotate Gobo"), \
    LJ_WSCRL(s, "Rotating Wheel Scroll Speed", "Wheel Scroll", 204, 255, lj_m5_gscrl_##s), LJ_SHAKE(s, "Goboshake", 60, 203), \
    LJ_FB(s + 1, 1, LJ_HAND_FADE), LJ_INDEX(s + 1, "Gobo Index"), LJ_ROT(s + 1, "Gobo Rotation Speed", 3, 252, lj_m5_grot_##s)
/* Static wheel (550 / 575 ch 8, 700P ch 10): continuous 0 open, 11 slot 1 .. 99 slot 9 (11 each), 110-112 open; stepped 113-202; 203-221 CW
 * fast -> slow; 222-240 CCW slow -> fast; 241-245 / 246-250 / 251-255 random gobo fast / medium / slow. */
#define LJ_M5_STATIC(g, cw, ccw) \
    static const lj_ctl_button lj_m5_sscrl_##g[] = { { cw, g, 203, 203, 221 }, { ccw, g, 222, 222, 240 }, { "Stop", g, 0, 0, 0 } }; \
    static const lj_ctl_button lj_m5_srnd_##g[]  = { { "Random Gobo Slow", g, 251, 251, 255 }, { "Random Gobo Medium", g, 246, 246, 250 }, { "Random Gobo Fast", g, 241, 241, 245 } }
LJ_M5_STATIC(7, "Static Wheel Scroll CW", "Static Wheel Scroll CCW"); LJ_M5_STATIC(9, "Wheel Scroll CW", "Wheel Scroll CCW");
#define LJ_M5_STATIC_ROWS(g, cap, cap2, speed, scroll) \
    LJ_FB(g, 1, LJ_HAND_SNAP), LJ_SCRL(g, cap, cap2, 0, 112), LJ_WSCRL(g, speed, scroll, 203, 240, lj_m5_sscrl_##g), LJ_PALR(g, cap, 3)
/* Animation wheel (550 ch 10 / 11, 700P ch 12 / 13): 0-9 open (disc out); 10-49 vertical / horizontal index / rotation; 50-139 angled index;
 * 140-229 angled rotation; 230-235 open; 236-255 macros 1-5 (4 each). Angle / speed: index 0-255, or 0-2 none, 3-127 CCW slow -> fast,
 * 128-252 CW fast -> slow (550 p.30-31; the 700P values are the 550's, inferred: QLC lists the two channels without ranges). */
#define LJ_M5_ANIM(a) \
    static const lj_ctl_button lj_m5_aout_##a[] = { { "Animation Disc Out", a, 0, 0, 9 } }; \
    static const lj_ctl_button lj_m5_amac_##a[] = { { "Macro 1", a, 236, 236, 239 }, { "Macro 2", a, 240, 240, 243 }, { "Macro 3", a, 244, 244, 247 }, \
                                                    { "Macro 4", a, 248, 248, 251 }, { "Macro 5", a, 252, 252, 255 } }; \
    static const lj_ctl_button lj_m5_arot_##a[] = { { "Rotation CW", a + 1, 252, 128, 252 }, { "Rotation CCW", a + 1, 3, 3, 127 }, { "Stop", a + 1, 0, 0, 2 } }
LJ_M5_ANIM(9); LJ_M5_ANIM(11);
#define LJ_M5_ANIM_ROWS(a) \
    LJ_FB(a, 1, LJ_HAND_SNAP), LJ_BAR(a, "Pos.Index:", 50, 139, 0), LJ_BAR(a, "Pos.Rotation:", 140, 229, 0), \
    LJ_FB(a + 1, 1, LJ_HAND_FADE), LJ_INDEX(a + 1, "Animation Disc Index"), LJ_ROT(a + 1, "Rotation Speed", 3, 252, lj_m5_arot_##a)
#define LJ_M5_ANIM_SPEC(a) LJ_SPEC(a, "Animation Disc Out", NULL, 0, 9, lj_m5_aout_##a, 1), LJ_SPEC(a, "Macro", NULL, 236, 255, lj_m5_amac_##a, 5)
static const lj_ctl_row lj_m5_gobos_rows[] = {          /* TGoboControl_MAC550: rotating, static, animation disc */
    LJ_M5_GOBO_ROWS(4), LJ_M5_STATIC_ROWS(7, "Static Gobo", NULL, "Static Wheel Scroll Speed", "Static Wheel Scroll"), LJ_M5_ANIM_ROWS(9) };
static const lj_ctl_row lj_m5_gobos_specials[] = { LJ_SPEC(7, "Random Gobo", NULL, 241, 255, lj_m5_srnd_7, 3), LJ_M5_ANIM_SPEC(9) };
static const lj_ctl_row lj_m7_gobos_rows[] = {          /* TGoboControl_MAC575K: no Animation Disc strings */
    LJ_M5_GOBO_ROWS(4), LJ_M5_STATIC_ROWS(7, "Static Gobo", NULL, "Static Wheel Scroll Speed", "Static Wheel Scroll") };
static const lj_ctl_row lj_m7_gobos_specials[] = { LJ_SPEC(7, "Random Gobo", NULL, 241, 255, lj_m5_srnd_7, 3) };
static const lj_ctl_row lj_7p_gobos_rows[] = {          /* TGoboControl_MAC700P: Current Gobo / Gobo Wheel / Wheel Scroll for the static wheel; rotating + animation as the 550 (inferred) */
    LJ_M5_GOBO_ROWS(6), LJ_M5_STATIC_ROWS(9, "Current Gobo", "Gobo Wheel", "Wheel Scroll Speed", "Wheel Scroll"), LJ_M5_ANIM_ROWS(11) };
static const lj_ctl_row lj_7p_gobos_specials[] = { LJ_SPEC(9, "Random Gobo", NULL, 241, 255, lj_m5_srnd_9, 3), LJ_M5_ANIM_SPEC(11) };

/* ---- Effects: prism (550 ch 12, 575 ch 10, 700P ch 14): 0-19 out; 20-79 CCW fast -> slow; 80-89 in, no rotation; 90-149 CW slow -> fast;
 * 150-215 out (575 / 700P: 150-255); 550 only: 216-255 prism macros 1-8 (5 each). TEffectControl_Mac550: 3facet + Macro 1..8; _Mac575K: 3facet. */
#define LJ_M5_PRISM(c) \
    static const lj_ctl_button lj_m5_prism_##c[] = { { "3facet", c, 80, 80, 89 } }; \
    static const lj_ctl_button lj_m5_prot_##c[]  = { { "CW", c, 90, 90, 149 }, { "CCW", c, 79, 20, 79 }, { "Stop", c, 80, 80, 89 } }; \
    static const lj_ctl_row lj_m5_fx_rows_##c[]  = { LJ_FB(c, 1, LJ_HAND_SNAP), LJ_FXBTN(c, "Prism", 0, 89, lj_m5_prism_##c), LJ_ROT(c, "Speed", 20, 149, lj_m5_prot_##c) }
LJ_M5_PRISM(11); LJ_M5_PRISM(9); LJ_M5_PRISM(13);
static const lj_ctl_button lj_m5_pmacro[] = { { "Macro 1", 11, 216, 216, 220 }, { "Macro 2", 11, 221, 221, 225 }, { "Macro 3", 11, 226, 226, 230 }, { "Macro 4", 11, 231, 231, 235 },
                                              { "Macro 5", 11, 236, 236, 240 }, { "Macro 6", 11, 241, 241, 245 }, { "Macro 7", 11, 246, 246, 250 }, { "Macro 8", 11, 251, 251, 255 } };
static const lj_ctl_row lj_m5_fx_specials[] = { LJ_SPEC(11, "Macro", NULL, 216, 255, lj_m5_pmacro, 8) };   /* group label not visible in a capture: PROPOSAL */

/* ---- Beam: iris (550 ch 13, 575 ch 11, 700P ch 15): 0-199 open -> closed; 200-215 closed; 216-229 opening pulse; 230-243 closing pulse;
 * 244-246 / 247-249 random opening fast / slow; 250-252 / 253-255 random closing fast / slow. Focus infinity -> near, zoom flood -> spot.
 * TBeamControl_Mac575K: Iris, Focus, Zoom, Iris Pulse, Random Iris Pulse, Open, Close, Open Fast / Slow, Close Fast / Slow. */
#define LJ_M5_BEAM(c) \
    static const lj_ctl_button lj_m5_ip_##c[] = { { "Open", c, 216, 216, 229 }, { "Close", c, 230, 230, 243 } }; \
    static const lj_ctl_button lj_m5_rp_##c[] = { { "Open Fast", c, 244, 244, 246 }, { "Open Slow", c, 247, 247, 249 }, { "Close Fast", c, 250, 250, 252 }, { "Close Slow", c, 253, 253, 255 } }; \
    static const lj_ctl_row lj_m5_beam_rows_##c[] = { LJ_FB(c, 1, LJ_HAND_FADE), LJ_BAR(c, "Iris", 0, 199, 0), LJ_FB(c + 1, 1, LJ_HAND_FADE), LJ_BAR(c + 1, "Focus", 0, 255, 0), \
                                                      LJ_FB(c + 2, 1, LJ_HAND_FADE), LJ_BAR(c + 2, "Zoom", 0, 255, 0) }; \
    static const lj_ctl_row lj_m5_beam_spec_##c[] = { LJ_SPEC(c, "Iris Pulse", "Pulse Off", 216, 243, lj_m5_ip_##c, 2), LJ_SPEC(c, "Random Iris Pulse", NULL, 244, 255, lj_m5_rp_##c, 4) }
LJ_M5_BEAM(12); LJ_M5_BEAM(10); LJ_M5_BEAM(14);
/* MAC 700 Wash ch 8 beam shaper: 0 open; 1-255 indexed angle, or with the macro channel (ch 9) at 56-95 rotation: 1-127 CCW fast -> slow, 128 none,
 * 129-255 CW slow -> fast; ch 10 zoom. TBeamControl_Mac700W: Beam Shaper Out, Indexing | Rotating, Rotation CW / CCW / Speed, Index. The mode
 * lives on the macro channel, not on the shaper channel, so Indexing | Rotating is a special group writing ch 9 (not an LJ_W_MODEPAIR). */
static const lj_ctl_button lj_7w_bsrot[]  = { { "Rotation CW", 7, 129, 129, 255 }, { "Rotation CCW", 7, 127, 1, 127 }, { "Stop", 7, 128, 128, 128 } };
static const lj_ctl_button lj_7w_bsout[]  = { { "Beam Shaper Out", 7, 0, 0, 0 } };
static const lj_ctl_button lj_7w_bsmode[] = { { "Indexing", 8, 0, 0, 15 }, { "Rotating", 8, 56, 56, 95 } };
static const lj_ctl_row lj_7w_beam_rows[] = {
    LJ_FB(7, 1, LJ_HAND_FADE), { LJ_W_INDEXBAR, 7, 1, "Index", NULL, 1, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }, LJ_ROT(7, "Rotation Speed", 1, 255, lj_7w_bsrot),
    LJ_FB(8, 1, LJ_HAND_SNAP), LJ_FB(9, 1, LJ_HAND_FADE), LJ_BAR(9, "Zoom", 0, 255, 0) };
static const lj_ctl_row lj_7w_beam_specials[] = { LJ_SPEC(7, "Beam Shaper Out", NULL, 0, 0, lj_7w_bsout, 1), LJ_SPEC(8, "Beam Shaper", NULL, 0, 95, lj_7w_bsmode, 2) };   /* group label ours */

/* ---- Lamp: TLampControl_DOO (550 / 575); TLampControl_MAC700 adds 400W 238-242 and 700W 243-247 ("400 W mode active until 700 W value is
 * selected"; shutter closed > 10 s also drops to 400 W). */
static const lj_ctl_button lj_m7_lamp[] = { { "Lamp On", 0, 228, 228, 237 }, { "Lamp Off", 0, 248, 248, 255 }, { "400W", 0, 238, 238, 242 }, { "700W", 0, 243, 243, 247 } };
static const lj_ctl_row lj_m7_lamp_rows[] = { LJ_FB_OS(0), LJ_LAMPB(0, "Lamp On", "Lamp Off", 228, 255, lj_m7_lamp, 4), LJ_LAMP_NOTE(0) };

/* ---- Extended: TSpecialSettings_Mac550 / _Mac575K / _Mac700P / _Mac700W ("Color/Gobo macro controls located in extended controls", NOTES).
 * P/T speed (550 ch 20, 575 ch 18, 700P ch 22, 700W ch 15): 0-2 tracking, 3-242 fast -> slow, 243-245 PTSP SLOW, 246-248 NORM, 249-251 FAST,
 * 252-255 blackout while moving. Effects speed (550 ch 21, 575 ch 19, 700P ch 23, 700W ch 16): the manuals give per-effect sub-tables; the
 * radio uses the colour-speed one (0-2 tracking, 3-245, 246-248 SCUT OFF, 249-251 SCUT ON, 252-255 blackout while moving): inferred.
 * Macros (550 / 575 ch 9): 0-15 none; 16-55 static gobo shake; 56-95 colour 1 shake; 96-135 colour 2 shake; 136-175 gobo + colour 1;
 * 176-215 gobo + colour 2; 216-255 gobo + both colours, each slow -> fast. 700P ch 11: 16-55 static gobo shake, 56-95 colour shake, 96-135 both
 * (136-255 random CMY: the Colors window). 700W ch 9: 16-55 colour shake, 56-95 beam shaper rotation, 96-135 both. Item captions ours: PROPOSAL. */
static const char *const lj_m5_pt_items[] = { "Tracking", "Speed", "Tracking Slow", "Tracking Normal", "Tracking Fast", "B/O while moving", NULL };
static const char *const lj_m5_fx_items[] = { "Tracking", "Speed", "Shortcuts Disabled", "Shortcuts Enabled", "B/O while moving", NULL };
static const char *const lj_m5_mc_items[] = { "No Macro", "Gobo 2 Shake", "Color 1 Shake", "Color 2 Shake", "Gobo 2 + Color 1", "Gobo 2 + Color 2", "Gobo 2 + Colors", NULL };
static const char *const lj_7p_mc_items[] = { "No Macro", "Gobo 2 Shake", "Color Shake", "Gobo 2 + Color Shake", NULL };
static const char *const lj_7w_mc_items[] = { "No Macro", "Color Shake", "Shaper Rotation", "Shake + Rotation", NULL };
#define LJ_M5_SPEEDS(p, f) \
    static const lj_ctl_button lj_m5_pt_##p[] = { { "Tracking", p, 0, 0, 2 }, { "Speed", p, 3, 3, 242 }, { "Tracking Slow", p, 243, 243, 245 }, \
                                                  { "Tracking Normal", p, 246, 246, 248 }, { "Tracking Fast", p, 249, 249, 251 }, { "B/O while moving", p, 252, 252, 255 } }; \
    static const lj_ctl_button lj_m5_fx_##f[] = { { "Tracking", f, 0, 0, 2 }, { "Speed", f, 3, 3, 245 }, { "Shortcuts Disabled", f, 246, 246, 248 }, \
                                                  { "Shortcuts Enabled", f, 249, 249, 251 }, { "B/O while moving", f, 252, 252, 255 } }
#define LJ_M5_SPEED_ROWS(p, f) \
    LJ_FB(p, 1, LJ_HAND_OFF), LJ_RADIO(p, "Pan/Tilt Settings", "Pan/Tilt Speed", 3, 242, lj_m5_pt_##p, 6, lj_m5_pt_items), \
    LJ_FB(f, 1, LJ_HAND_OFF), LJ_RADIO(f, "Effects Settings", "Effect Speed", 3, 245, lj_m5_fx_##f, 5, lj_m5_fx_items)
#define LJ_M5_MACRO_ROW(m, b, n, items) LJ_FB(m, 1, LJ_HAND_OFF), LJ_RADIO(m, "Gobo/Color Macros", NULL, 0, 0, b, n, items)   /* lo = hi: no radio takes the fader */
LJ_M5_SPEEDS(19, 20); LJ_M5_SPEEDS(17, 18); LJ_M5_SPEEDS(21, 22); LJ_M5_SPEEDS(14, 15);
static const lj_ctl_button lj_m5_mc[] = { { "No Macro", 8, 0, 0, 15 }, { "Gobo 2 Shake", 8, 16, 16, 55 }, { "Color 1 Shake", 8, 56, 56, 95 }, { "Color 2 Shake", 8, 96, 96, 135 },
                                          { "Gobo 2 + Color 1", 8, 136, 136, 175 }, { "Gobo 2 + Color 2", 8, 176, 176, 215 }, { "Gobo 2 + Colors", 8, 216, 216, 255 } };
static const lj_ctl_button lj_7p_mc[] = { { "No Macro", 10, 0, 0, 15 }, { "Gobo 2 Shake", 10, 16, 16, 55 }, { "Color Shake", 10, 56, 56, 95 }, { "Gobo 2 + Color Shake", 10, 96, 96, 135 } };
static const lj_ctl_button lj_7w_mc[] = { { "No Macro", 8, 0, 0, 15 }, { "Color Shake", 8, 16, 16, 55 }, { "Shaper Rotation", 8, 56, 56, 95 }, { "Shake + Rotation", 8, 96, 96, 135 } };
static const lj_ctl_row lj_m5_ext_rows[] = { LJ_M5_SPEED_ROWS(19, 20), LJ_M5_MACRO_ROW(8, lj_m5_mc, 7, lj_m5_mc_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_m7_ext_rows[] = { LJ_M5_SPEED_ROWS(17, 18), LJ_M5_MACRO_ROW(8, lj_m5_mc, 7, lj_m5_mc_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_7p_ext_rows[] = { LJ_M5_SPEED_ROWS(21, 22), LJ_M5_MACRO_ROW(10, lj_7p_mc, 4, lj_7p_mc_items), LJ_MT_RESET_ROWS };
static const lj_ctl_row lj_7w_ext_rows[] = { LJ_M5_SPEED_ROWS(14, 15), LJ_M5_MACRO_ROW(8, lj_7w_mc, 4, lj_7w_mc_items), LJ_MT_RESET_ROWS };

/* ---- windows ---- */
#define LJ_M5_FXWIN(c, spec) LJ_WIN(LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_m5_fx_rows_##c, spec)
#define LJ_M5_BEAMWIN(c)     LJ_WIN(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_m5_beam_rows_##c, lj_m5_beam_spec_##c)
static const lj_ctl_row lj_m5_no_specials[1] = { LJ_FB(9, 1, LJ_HAND_SNAP) };   /* placeholder never read: the 575 / 700P effects windows pass nspecials = 0 */
static const lj_ctl_window lj_m5_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(15), LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_m5_colors_rows, lj_m5_colors_specials),
    LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_m5_gobos_rows, lj_m5_gobos_specials), LJ_M5_BEAMWIN(12), LJ_M5_FXWIN(11, lj_m5_fx_specials),
    LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m5_ext_rows) };
static const lj_ctl_window lj_m7_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(13), LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_m5_colors_rows, lj_m5_colors_specials),
    LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_m7_gobos_rows, lj_m7_gobos_specials), LJ_M5_BEAMWIN(10),
    { LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_m5_fx_rows_9, LJ_NROWS(lj_m5_fx_rows_9), lj_m5_no_specials, 0 }, LJ_MT_LAMP_WIN, LJ_EXT_WIN(lj_m7_ext_rows) };
static const lj_ctl_window lj_7p_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(17), LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_7p_colors_rows, lj_7p_colors_specials),
    LJ_WIN(LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, lj_7p_gobos_rows, lj_7p_gobos_specials), LJ_M5_BEAMWIN(14),
    { LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_fx1, lj_m5_fx_rows_13, LJ_NROWS(lj_m5_fx_rows_13), lj_m5_no_specials, 0 },
    LJ_WIN0(LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lj_m7_lamp_rows), LJ_EXT_WIN(lj_7p_ext_rows) };
static const lj_ctl_window lj_7w_windows[] = { LJ_MT_INTENSITY_WIN, LJ_MT_MOVE16_WIN(10), LJ_WIN(LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, lj_7w_colors_rows, lj_7w_colors_specials),
    LJ_WIN(LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, lj_7w_beam_rows, lj_7w_beam_specials), LJ_WIN0(LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lj_m7_lamp_rows),
    LJ_EXT_WIN(lj_7w_ext_rows) };

/* ---- palettes (6.5): names from the manuals' DMX tables; bitmaps = our tiles under the exe colour / gobo table names (LORD_FIXTURES-9),
 * assignment inferred per slot [15/09 s2-s5]. RGB ours. ---- */
#define LJ_M5_CSLOTS(r1, b1, r2, b2, r3, b3, r4, b4, r5, b5, r6, b6, r7, b7, r8, b8) \
    LJ_PE(0, 0xFFFFFF, ""), LJ_PE(17, r1, b1), LJ_PE(34, r2, b2), LJ_PE(51, r3, b3), LJ_PE(68, r4, b4), LJ_PE(85, r5, b5), LJ_PE(102, r6, b6), LJ_PE(119, r7, b7), LJ_PE(136, r8, b8)
/* 550 wheel 1: Red 308, Magenta 507, Purple 502 IAD, Blue 104, Green 206, Yellow 601, Orange 306M, Purple 509; wheel 2: Green 208, Blue 102, Pink 312,
 * Red 301, Yellow 603, Half Minus Green, CTC 5500-4200, CTC 3200-4100 (p.28-29). */
#define LJ_M5_COLOR1 { { LJ_M5_CSLOTS(0xFF2000, "Red 308", 0xFF00C0, "Magenta 507", 0x8000C0, "Purple 502 IAD", 0x0060FF, "Blue 104", 0x00C000, "Green 206", \
                                      0xFFE000, "Yellow 601", 0xFF8000, "Orange 306M", 0xA000FF, "Purple 509") }, LJ_PAL_DEF }
#define LJ_M5_COLOR2 { { LJ_M5_CSLOTS(0x00A040, "Green 208", 0x0040FF, "Blue 102", 0xFF80B0, "Pink 312", 0xFF0000, "Red 301", 0xFFFF00, "Yellow 603", \
                                      0xFFC0FF, "CTC \xBDMinusGreen", 0xFFE8D0, "CTC 5500-4200", 0xFFC890, "CTC 3200-4100") }, LJ_PAL_DEF }   /* \xBD = CP1252 one-half, the exe name */
/* 575 wheel 1: Medium blue, Light green, Deep orange, Light yellow, Pink, Magenta, Deep blue, Red; wheel 2: Green, Purple, Sky blue, Deep golden
 * amber, Aqua green, Deep purple, Light blue, CTC 5500-4200 K (p.30-31): generic names, colour blocks without bitmaps. */
#define LJ_M7_COLOR1 { { LJ_M5_CSLOTS(0x2060FF, "", 0x80FF80, "", 0xFF6000, "", 0xFFFF80, "", 0xFF80B0, "", 0xFF00FF, "", 0x0000A0, "", 0xFF0000, "") }, LJ_PAL_DEF }
#define LJ_M7_COLOR2 { { LJ_M5_CSLOTS(0x00C000, "", 0x8000C0, "", 0x80C0FF, "", 0xFFB000, "", 0x40E0C0, "", 0x6000A0, "", 0xA0C0FF, "", 0xFFE8D0, "CTC 5500-4200") }, LJ_PAL_DEF }
/* 700 wheel: Blue 110, Green 206 IAD, Pink 312, Orange 306M, Half minus green, CTC 3200-4100 K, then CTC 5500-2800 K (Profile) or UV (Wash), Red 308. */
#define LJ_7X_COLOR1(r7, b7) { { LJ_M5_CSLOTS(0x0040FF, "", 0x00C000, "Green 206", 0xFF80B0, "Pink 312", 0xFF8000, "Orange 306M", 0xFFC0FF, "CTC \xBDMinusGreen", \
                                              0xFFC890, "CTC 3200-4100", r7, b7, 0xFF2000, "Red 308") }, LJ_PAL_DEF }
/* Rotating gobos: Def = indexed slot, Rot = the continuous-rotation slot. */
#define LJ_M5_GPAL(a, b, c, d, e, f) { { LJ_PE_ROT(0, 0, "Open"), LJ_PE_ROT(12, 36, a), LJ_PE_ROT(16, 40, b), LJ_PE_ROT(20, 44, c), LJ_PE_ROT(24, 48, d), \
                                         LJ_PE_ROT(28, 52, e), LJ_PE_ROT(32, 56, f) }, LJ_PAL_DEFROT }
#define LJ_M5_SPAL(a, b, c, d, e, f, g, h, i) { { LJ_PE(0, 0, "Open"), LJ_PE(11, 0, a), LJ_PE(22, 0, b), LJ_PE(33, 0, c), LJ_PE(44, 0, d), LJ_PE(55, 0, e), \
                                                  LJ_PE(66, 0, f), LJ_PE(77, 0, g), LJ_PE(88, 0, h), LJ_PE(99, 0, i) }, LJ_PAL_DEF }
#define LJ_M5_GOBO1 LJ_M5_GPAL("Fire Sun", "Rotator", "Indigo Waves", "Limbo", "Water 4", "Flames 6")
#define LJ_M5_GOBO2 LJ_M5_SPAL("Crackle", "Triangles 2", "Tye Dye", "Splodge", "Ninestar", "bio", "Leaf Breakup", "Zig Zags", "Two Tone")
#define LJ_M7_GOBO1 LJ_M5_GPAL("Laserdots", "Ovals", "3-rings", "Fractal", "Redeye", "BlueRipple")
#define LJ_M7_GOBO2 LJ_M5_SPAL("Inspiral", "SpiralDrops", "RadialCircles", "Whirlpool", "", "DNA", "RadialBreakup", "Warpspeed", "Starfield")   /* Triangles: trangle / Triangles3 ambiguous */
#define LJ_7P_GOBO1 LJ_M5_GPAL("", "RadialCircles", "FusedDichro002", "Milkyway", "Water 4", "Flames 6")                                       /* Spiral: ambiguous */
#define LJ_7P_GOBO2 LJ_M5_SPAL("Crackle", "Triangles 2", "Tye Dye", "Globo", "Worms", "bio", "Leaf Breakup", "Whirlpool", "Two Tone")

/* ---- profiles ---- */
#define LJ_M5_COMMON .manufacturer = "Martin", .fixture_id = "", LJ_MT_CH12, .strobe = LJ_MT_STROBE, .pt16 = 1, .presets = 1, .macros = 1, .follow_spot = 1
#define LJ_M5_NO16X "Does not work with the fixture in 16 bit extended mode. "
#define LJ_R(n, c) LJ_R##n(c)
#define LJ_R1(c) c
#define LJ_R2(c) c, c
#define LJ_R4(c) c, c, c, c
#define LJ_M5_PT LJ_CH_PAN, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_TILT, LJ_CH_SPEED, LJ_CH_SPEED

static const lj_profile lj_m5_mac550 = { .id = 110, .footprint = 21, .tag = "MAC550", .name = "MAC 550", .mode = "16 Bit", LJ_M5_COMMON,
    /* 1 shutter, 2 dimmer, 3-4 colour wheels, 5-7 rotating gobo (sel, MSB, LSB), 8 static gobo, 9 gobo / colour macros, 10-11 animation wheel,
     * 12 prism, 13 iris, 14 focus, 15 zoom, 16-19 pan/tilt 16 bit, 20-21 speed (p.28-32 basic column) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R2(LJ_CH_COLOR), LJ_R4(LJ_CH_GOBO), LJ_CH_SPECIAL, LJ_R2(LJ_CH_GOBO), LJ_CH_EFFECT,
                       LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_BEAM, LJ_M5_PT },
    .pair_lsb = { [5] = 1 + 6 },                                                 /* rotating gobo index: MSB ch 6, LSB ch 7 */
    .color = { { 1, 2, "Color 1", 0, 153 }, { 1, 3, "Color 2", 0, 153 } }, .gobo = { { 1, 4, "Gobo 1", 0, 59 }, { 1, 7, "Gobo 2", 0, 112 }, { 1, 5, "Index 1", 0, 255 }, { 1, 10, "Anim", 0, 255 } },
    .effects = { { 1, 11, "Prism", 0, 149 } }, .iris = { 1, 12, "Iris", 0, 199 }, .focus = { 1, 13, "Focus", 0, 255 }, .zoom = { 1, 14, "Zoom", 0, 255 },
    .palette = { LJ_M5_COLOR1, LJ_M5_COLOR2, LJ_M5_GOBO1, LJ_M5_GOBO2 }, .pan_ofs = 15, .tilt_ofs = 17,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), 1 }, .windows = lj_m5_windows, .nwindows = LJ_NROWS(lj_m5_windows),
    .notes = LJ_M5_NO16X "Colour / gobo macros are in the Extended control. Fixture menu: PSEt/MOdE = 1 (16 bit), SPEC/dLOF = on, SPEC/dRES = on. "
             "Reset and lamp off disabled in the menu work only with both colour wheels on slot 1, the prism in without rotation and both gobo wheels open. 21 DMX addresses." };
static const lj_profile lj_m5_mac575 = { .id = 155, .footprint = 19, .tag = "MAC575K", .name = "MAC 575 Krypton", .mode = "16 Bit", LJ_M5_COMMON,
    /* 1 shutter, 2 dimmer, 3-4 colour wheels, 5-7 rotating gobo, 8 static gobo, 9 macros, 10 prism, 11 iris, 12 focus, 13 zoom, 14-17 pan/tilt, 18-19 speed */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R2(LJ_CH_COLOR), LJ_R4(LJ_CH_GOBO), LJ_CH_SPECIAL, LJ_CH_EFFECT, LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_BEAM, LJ_M5_PT },
    .pair_lsb = { [5] = 1 + 6 },
    .color = { { 1, 2, "Color 1", 0, 153 }, { 1, 3, "Color 2", 0, 153 } }, .gobo = { { 1, 4, "Gobo 1", 0, 59 }, { 1, 7, "Gobo 2", 0, 112 }, { 1, 5, "Index 1", 0, 255 } },
    .effects = { { 1, 9, "Prism", 0, 149 } }, .iris = { 1, 10, "Iris", 0, 199 }, .focus = { 1, 11, "Focus", 0, 255 }, .zoom = { 1, 12, "Zoom", 0, 255 },
    .palette = { LJ_M7_COLOR1, LJ_M7_COLOR2, LJ_M7_GOBO1, LJ_M7_GOBO2 }, .pan_ofs = 13, .tilt_ofs = 15,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R2(1), 1 }, .windows = lj_m7_windows, .nwindows = LJ_NROWS(lj_m7_windows),
    .notes = LJ_M5_NO16X "Colour / gobo macros are in the Extended control. Reset and lamp off disabled in the menu work only with both colour wheels "
             "on slot 1, the prism on without rotation and both gobo wheels open. 19 DMX addresses." };
static const lj_profile lj_m5_mac700p = { .id = 116, .footprint = 23, .tag = "MAC 700P", .name = "MAC 700 Profile", .mode = "16 Bit", LJ_M5_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 colour wheel, 7-9 rotating gobo, 10 static gobo, 11 macros / random CMY, 12-13 animation wheel, 14 prism,
     * 15 iris, 16 focus, 17 zoom, 18-21 pan/tilt, 22-23 speed */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R4(LJ_CH_COLOR), LJ_R4(LJ_CH_GOBO), LJ_CH_SPECIAL, LJ_R2(LJ_CH_GOBO), LJ_CH_EFFECT,
                       LJ_CH_BEAM, LJ_CH_BEAM, LJ_CH_BEAM, LJ_M5_PT },
    .pair_lsb = { [7] = 1 + 8 },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1,
    .color = { { 1, 5, "Color 1", 0, 153 } }, .gobo = { { 1, 6, "Gobo 1", 0, 59 }, { 1, 9, "Gobo 2", 0, 112 }, { 1, 7, "Index 1", 0, 255 }, { 1, 12, "Anim", 0, 255 } },
    .effects = { { 1, 13, "Prism", 0, 149 } }, .iris = { 1, 14, "Iris", 0, 199 }, .focus = { 1, 15, "Focus", 0, 255 }, .zoom = { 1, 16, "Zoom", 0, 255 },
    .palette = { LJ_7X_COLOR1(0xFFD8A8, "CTC 5500-2800"), { { { 0 } }, LJ_PAL_DEF }, LJ_7P_GOBO1, LJ_7P_GOBO2 }, .pan_ofs = 17, .tilt_ofs = 19,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R2(1), 1 }, .windows = lj_7p_windows, .nwindows = LJ_NROWS(lj_7p_windows),
    .notes = LJ_M5_NO16X "Colour / gobo macros are in the Extended control. Fixture menu: PSET/16BT, PERS/DLOF = ON, PERS/DRES = ON. The lamp drops to 400 W "
             "after 10 seconds with the shutter closed; the 400W / 700W buttons are on the Lamp control. 23 DMX addresses." };
static const lj_profile lj_m5_mac700w = { .id = 140, .footprint = 16, .tag = "MAC 700W", .name = "MAC 700 Wash", .mode = "16 Bit", LJ_M5_COMMON,
    /* 1 shutter, 2 dimmer, 3-5 CMY, 6 CTC, 7 colour wheel, 8 beam shaper, 9 macros / random CMY, 10 zoom, 11-14 pan/tilt, 15-16 speed (p.23-25) */
    .channel_class = { LJ_CH_SHUTTER, LJ_CH_INTENSITY, LJ_R4(LJ_CH_COLOR), LJ_CH_COLOR, LJ_CH_BEAM, LJ_CH_SPECIAL, LJ_CH_BEAM, LJ_M5_PT },
    .cmy = { { 1, 2, "Cyan", 0, 255 }, { 1, 3, "Magenta", 0, 255 }, { 1, 4, "Yellow", 0, 255 } }, .cmy_on = 1, .cmy_picker = 1,
    .color = { { 1, 6, "Color 1", 0, 153 }, { 0 }, { 1, 5, "CTC", 0, 255 } }, .beam = { { 1, 7, "Shaper", 0, 255 } }, .zoom = { 1, 9, "Zoom", 0, 255 },
    .palette = { LJ_7X_COLOR1(0x6000FF, "UVPass") }, .pan_ofs = 10, .tilt_ofs = 12,
    .level_invisible = { LJ_R4(1), LJ_R4(1), LJ_R4(1), LJ_R4(1) }, .windows = lj_7w_windows, .nwindows = LJ_NROWS(lj_7w_windows),
    .notes = LJ_M5_NO16X "Fixture menu: PSET/16BT, PERS/DLOF = ON, PERS/DRES = ON. The beam shaper rotates only with the macro channel set to rotation "
             "(Beam control: Indexing / Rotating). The lamp drops to 400 W after 10 seconds with the shutter closed. 16 DMX addresses." };

const lj_profile *const lj_hand_mac550s[LJ_MAC550_COUNT] = { &lj_m5_mac550, &lj_m5_mac575, &lj_m5_mac700p, &lj_m5_mac700w };
