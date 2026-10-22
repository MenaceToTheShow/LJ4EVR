/* lj_martin.c - the shared Martin rows of lj_martin.h. Same values as the MAC 500's
 * channel 1 / 2 in lj_mac500.c (UM-MAC500 Rev. E p.27, martin.com DMX table V5), repeated
 * by UM-MAC250 p.21, UM-MAC250KryptonEntour Rev K p.36 / p.39, UM-MAC250Wash Rev G p.27,
 * DMXTAB-MAC2000Profile V1 ch 1, UM-MAC2000Performance Rev E p.40 and UM-MAC2000Wash
 * Rev L p.32 (the 73-79 "lamp power reduced" band is the E-ballast variant; nothing
 * here writes it). Every value = the low end of its range until F47 is verified. */
#include "fixlib/hand/lj_martin.h"

static const lj_ctl_button lj_mt_shutter[] = { { "Shutter Closed", 0, 0, 0, 19 }, { "Shutter Open", 0, 20, 20, 49 } };   /* F47 PROPOSAL, as lj_mac500.c */
static const lj_ctl_button lj_mt_pulse[]   = { { "Pulse Open", 0, 80, 80, 99 }, { "Pulse Close", 0, 100, 100, 119 } };
static const lj_ctl_button lj_mt_rpulse[]  = { { "Open Fast", 0, 191, 191, 193 }, { "Open Slow", 0, 194, 194, 196 }, { "Close Fast", 0, 197, 197, 199 }, { "Close Slow", 0, 200, 200, 202 } };
static const lj_ctl_button lj_mt_rstrobe[] = { { "Strobe Slow", 0, 168, 168, 187 }, { "Strobe Med", 0, 148, 148, 167 }, { "Strobe Fast", 0, 128, 128, 147 } };

const lj_ctl_row lj_mt_intensity_rows[5] = {
    LJ_FB(0, 2, LJ_HAND_SNAP),                                                                                   /* one Snap spans Shutter + Strobe (ch 1) */
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", 0, 49, 0, 0, lj_mt_shutter, 2, NULL, LJ_STATES_ALL },
    LJ_BAR(0, "Strobe", 50, 72, 1),                                                                              /* fast -> slow: reverse */
    LJ_FB(1, 1, LJ_HAND_FADE),
    LJ_BAR(1, "Intensity", 0, 255, 0) };
const lj_ctl_row lj_mt_intensity_specials[3] = {
    LJ_SPEC(0, "Intensity Pulse", "Pulse Off", 80, 119, lj_mt_pulse, 2),
    LJ_SPEC(0, "Random Pulse", NULL, 191, 202, lj_mt_rpulse, 4),
    LJ_SPEC(0, "Random Strobe", NULL, 128, 187, lj_mt_rstrobe, 3) };

static const lj_ctl_button lj_mt_lamp[] = { { "Lamp On", 0, 228, 228, 237 }, { "Lamp Off", 0, 248, 248, 255 } };   /* lamp off: confirmation, hold > 5 s; captions as lj_mac500.c (the bulb glyphs) */
const lj_ctl_row lj_mt_lamp_rows[3] = { LJ_FB_OS(0), LJ_LAMPB(0, "Lamp On", "Lamp Off", 228, 255, lj_mt_lamp, 2), LJ_LAMP_NOTE(0) };

const lj_ctl_button lj_mt_reset[1] = { { "Reset", 0, 208, 208, 217 } };

#define LJ_MV(n) { LJ_W_FADEBTN, n, 1, "Pan/Tilt Fade", NULL, 0, 0, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_ALL }
const lj_ctl_row lj_mt_move_rows[LJ_PROFILE_CH] = {
    LJ_MV(0), LJ_MV(1), LJ_MV(2), LJ_MV(3), LJ_MV(4), LJ_MV(5), LJ_MV(6), LJ_MV(7), LJ_MV(8), LJ_MV(9), LJ_MV(10), LJ_MV(11), LJ_MV(12), LJ_MV(13), LJ_MV(14), LJ_MV(15),
    LJ_MV(16), LJ_MV(17), LJ_MV(18), LJ_MV(19), LJ_MV(20), LJ_MV(21), LJ_MV(22), LJ_MV(23), LJ_MV(24), LJ_MV(25), LJ_MV(26), LJ_MV(27), LJ_MV(28), LJ_MV(29), LJ_MV(30), LJ_MV(31) };
