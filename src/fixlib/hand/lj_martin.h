/* lj_martin.h - what the W2 Martin hand files share (15/02-15/05): row and palette
 * shorthands over lj_ctl_row / lj_pal_entry, and the rows of the MAC 500-type channel 1
 * (shutter, strobe, pulses, reset, lamp; the ranges of UM-MAC500 Rev. E p.27 that the
 * MAC 250 / 250+ / 300 / Krypton / Entour / Wash / Beam and the MAC 2000 family repeat
 * on their channel 1, intensity on channel 2) [15/03 s2-s7, 15/04 s2-s4]. Bodies in lj_martin.c. */
#ifndef LJ_FIXLIB_HAND_MARTIN_H
#define LJ_FIXLIB_HAND_MARTIN_H

#include "fixlib/lj_profile.h"
#include "fixlib/hand/lj_hand.h"

/* ---- row shorthands: { kind, ch, rows, caption, caption2, lo, hi, reverse, dflt_state, buttons, nbuttons, items, states_allowed } ---- */
#define LJ_FB(ch, span, dflt)           { LJ_W_FADEBTN, ch, span, NULL, NULL, 0, 0, 0, dflt, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_FB_OS(ch)                    { LJ_W_FADEBTN, ch, 1, NULL, NULL, 0, 0, 0, LJ_HAND_OFF, NULL, 0, NULL, LJ_STATES_OFF_SNAP }
#define LJ_BAR(ch, cap, lo, hi, rev)    { LJ_W_DRAGBAR, ch, 1, cap, NULL, lo, hi, rev, 0, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_SCRL(ch, cap, cap2, lo, hi)  { LJ_W_SCROLL, ch, 1, cap, cap2, lo, hi, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_PALR(ch, cap, idx)           { LJ_W_PALETTE, ch, 1, cap, NULL, idx, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }   /* lo = index into lj_profile.palette[] */
#define LJ_ROT(ch, cap, lo, hi, b)      { LJ_W_ROTATION, ch, 1, cap, NULL, lo, hi, 0, 0, b, 3, NULL, LJ_STATES_ALL }      /* b = {CW}, {CCW}, {Stop} (LORD_FIXTURES-10) */
#define LJ_WSCRL(ch, cap, cap2, lo, hi, b) { LJ_W_WHEELSCROLL, ch, 1, cap, cap2, lo, hi, 0, 0, b, 3, NULL, LJ_STATES_ALL }
#define LJ_SHAKE(ch, cap, lo, hi)       { LJ_W_SHAKE, ch, 1, cap, NULL, lo, hi, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_MODE(ch, cap, cap2)          { LJ_W_MODEPAIR, ch, 1, cap, cap2, 0, 0, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_INDEX(ch, cap)               { LJ_W_INDEXBAR, ch, 1, cap, NULL, 0, 255, 0, 0, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_FXBTN(ch, cap, lo, hi, b)    { LJ_W_EFFECT_BTN, ch, 1, cap, NULL, lo, hi, 0, 0, b, 1, NULL, LJ_STATES_ALL }
#define LJ_SPEC(ch, cap, cap2, lo, hi, b, n) { LJ_W_SPECIAL_GROUP, ch, 1, cap, cap2, lo, hi, 0, 0, b, n, NULL, LJ_STATES_ALL }
#define LJ_SPEC_OS(ch, cap, lo, hi, b, n)    { LJ_W_SPECIAL_GROUP, ch, 1, cap, NULL, lo, hi, 0, 0, b, n, NULL, LJ_STATES_OFF_SNAP }   /* stored in the scene, never faded (ME-27) */
#define LJ_RADIO(ch, cap, cap2, lo, hi, b, n, items) { LJ_W_RADIOGROUP_FADER, ch, 1, cap, cap2, lo, hi, 1, 0, b, n, items, LJ_STATES_ALL }   /* fader shows Slw at the bottom: reverse */
#define LJ_LAMPB(ch, cap, cap2, lo, hi, b, n) { LJ_W_LAMP_BTNS, ch, 1, cap, cap2, lo, hi, 0, 0, b, n, NULL, LJ_STATES_OFF_SNAP }
#define LJ_LAMP_NOTE(ch)                { LJ_W_NOTE, ch, 1, "NOTE !\nLamp Off must be held for at least 5 seconds", NULL, 0, 0, 0, 0, NULL, 0, NULL, LJ_STATES_OFF_SNAP }   /* TLampControl_DOO [15/00 s5] */

/* ---- palette entries (6.5): Def = DMX value (type code 1); rotating slots add Rot (type code 6) ---- */
#define LJ_PE(dmx, rgb, bmp)        { 1, 0x01, { dmx, 0, 0, 0, 0 }, rgb, bmp }
#define LJ_PE_ROT(idx, rot, bmp)    { 1, 0x03, { idx, rot, 0, 0, 0 }, 0, bmp }
#define LJ_PAL_DEF                  { 0, 0, 0, 0, 0 }
#define LJ_PAL_DEFROT               { 0, 6, 0, 0, 0 }

/* ---- the MAC 500-type channel 1 / 2 (TIntensityControl_Mac500 = _Mac2000 caption set, 15/04 s1) ---- */
extern const lj_ctl_row lj_mt_intensity_rows[5];      /* Snap span 2 (ch 1: Shutter Closed/Open 0-19/20-49, Strobe 50-72), Fade (ch 2 Intensity) */
extern const lj_ctl_row lj_mt_intensity_specials[3];  /* Intensity Pulse 80-119, Random Pulse 191-202, Random Strobe 128-187 */
extern const lj_ctl_row lj_mt_lamp_rows[3];           /* TLampControl_DOO: Lamp On 228-237, Lamp Off 248-255, the 5-second note; OFF|SNAP */
extern const lj_ctl_button lj_mt_reset[1];            /* Reset 208-217 on ch 1 */
extern const lj_ctl_row lj_mt_move_rows[LJ_PROFILE_CH];   /* one "Pan/Tilt Fade" O/S/F per pan offset: a profile points at &lj_mt_move_rows[pan_ofs] (ME-22) */
#define LJ_MT_RESET_ROWS LJ_FB_OS(0), LJ_SPEC_OS(0, "Fixture Reset", 208, 217, lj_mt_reset, 1)   /* stored in the scene behind the warning [15/00 s5] */

#define LJ_NROWS(a) ((int)(sizeof a / sizeof a[0]))
#define LJ_WIN(kind, cap, menu, rows, specials) { kind, cap, menu, rows, LJ_NROWS(rows), specials, LJ_NROWS(specials) }
#define LJ_WIN0(kind, cap, menu, rows)          { kind, cap, menu, rows, LJ_NROWS(rows), NULL, 0 }
#define LJ_MT_INTENSITY_WIN    { LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, lj_mt_intensity_rows, 5, lj_mt_intensity_specials, 3 }
#define LJ_MT_LAMP_WIN         { LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, lj_mt_lamp_rows, 3, NULL, 0 }
#define LJ_MT_MOVE16_WIN(ofs)  { LJ_CTL_MOVEMENT, "Movement 16 Bit", lj_ctl_menu_move, &lj_mt_move_rows[ofs], 1, NULL, 0 }
#define LJ_MT_MOVE8_WIN(ofs)   { LJ_CTL_MOVEMENT, "Movement 8 Bit", lj_ctl_menu_move, &lj_mt_move_rows[ofs], 1, NULL, 0 }   /* 8-bit caption: PROPOSAL, as lj_layout_derive */
#define LJ_EXT_WIN(rows)       { LJ_CTL_EXTENDED, "Extended", NULL, rows, LJ_NROWS(rows), NULL, 0 }   /* opened by Fixture toolbar button 7 [15/01 s3] */
#define LJ_LEVELS_WIN(n)       { LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, lj_hand_level_rows, n, NULL, 0 }

#define LJ_MT_STROBE { 1, 0, "Strobe", 50, 72 }   /* the .strobe dyn of that channel 1; kept out of LJ_MT_CH12 because the MAC 600 differs (no initializer overrides) */
/* The profile fields of that channel 1 / 2 (without .strobe): blackout = shutter closed, lamp on 228, lamp off 248, reset 208 [15/00 s5]. */
#define LJ_MT_CH12 \
    .shutter = { 1, 0, "Shutter", 0, 20 }, .intensity = { 1, 1, "Intens", 0, 255 }, .strobe_off = 20, \
    .blackout = { "", 1, { 1 }, { 0 } }, .lamp_on = { "", 1, { 1 }, { 228 } }, .lamp_off = { "", 1, { 1 }, { 248 } }, .reset = { "", 1, { 1 }, { 208 } }, .use_gm = 1

#endif /* LJ_FIXLIB_HAND_MARTIN_H */
