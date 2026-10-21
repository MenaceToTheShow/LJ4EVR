/* lj_hand.h - what every tier-1 hand file shares: the fade-state byte values, the
 * window menus of 15/00 s2 and the generic Levels rows (15/10 s2). Bodies in lj_hand.c. */
#ifndef LJ_FIXLIB_HAND_HAND_H
#define LJ_FIXLIB_HAND_HAND_H

#include "fixlib/lj_profile.h"

enum { LJ_HAND_OFF = 0, LJ_HAND_SNAP = 1, LJ_HAND_FADE = 2 };   /* = lj_fstate in model/lj_model.h, which fixlib may not include (layer graph) */

extern const char *const lj_ctl_menu_std[];    /* UserPalette Preferences Fades Help [15/00 s2] */
extern const char *const lj_ctl_menu_fx1[];    /* UserPalette Fades Help: Effects window of a single-effect fixture [15/00 s2] */
extern const char *const lj_ctl_menu_help[];   /* Help: Lamp Control [15/00 s2] */
extern const char *const lj_ctl_menu_move[];   /* Presets Preferences Fades Macro Options Help [design 6.4] */

/* Levels windows: one LJ_W_LEVEL_FADER row per channel, caption CH1..CH32, each row owns
 * its O/S/F button below the fader (no LJ_W_FADEBTN rows in a Levels table) [15/10 s2]. */
extern const lj_ctl_row lj_hand_level_rows[LJ_PROFILE_CH];    /* Fade default, all three states */
extern const lj_ctl_row lj_hand_switch_rows[LJ_PROFILE_CH];   /* on/off buttons: Snap default, OFF|SNAP only (ME-25, ME-27) */

#endif /* LJ_FIXLIB_HAND_HAND_H */
