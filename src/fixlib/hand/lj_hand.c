/* lj_hand.c - shared menus and Levels rows for the tier-1 hand files (lj_hand.h). */
#include "fixlib/hand/lj_hand.h"

const char *const lj_ctl_menu_std[]  = { "UserPalette", "Preferences", "Fades", "Help", NULL };
const char *const lj_ctl_menu_fx1[]  = { "UserPalette", "Fades", "Help", NULL };
const char *const lj_ctl_menu_rgb[]  = { "Palette", "Preferences", "Fades", "Help", NULL };
const char *const lj_ctl_menu_help[] = { "Help", NULL };
const char *const lj_ctl_menu_move[] = { "Presets", "Preferences", "Fades", "Macro", "Options", "Help", NULL };

#define LJ_LV(n) { LJ_W_LEVEL_FADER, (n) - 1, 1, "CH" #n, NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL }
#define LJ_SW(n) { LJ_W_LEVEL_FADER, (n) - 1, 1, "CH" #n, NULL, 0, 255, 0, LJ_HAND_SNAP, NULL, 0, NULL, LJ_STATES_OFF_SNAP }
const lj_ctl_row lj_hand_level_rows[LJ_PROFILE_CH]  = {
    LJ_LV(1), LJ_LV(2), LJ_LV(3), LJ_LV(4), LJ_LV(5), LJ_LV(6), LJ_LV(7), LJ_LV(8), LJ_LV(9), LJ_LV(10), LJ_LV(11), LJ_LV(12), LJ_LV(13), LJ_LV(14), LJ_LV(15), LJ_LV(16), LJ_LV(17), LJ_LV(18), LJ_LV(19), LJ_LV(20), LJ_LV(21), LJ_LV(22), LJ_LV(23), LJ_LV(24), LJ_LV(25), LJ_LV(26), LJ_LV(27), LJ_LV(28), LJ_LV(29), LJ_LV(30), LJ_LV(31), LJ_LV(32) };
const lj_ctl_row lj_hand_switch_rows[LJ_PROFILE_CH] = {
    LJ_SW(1), LJ_SW(2), LJ_SW(3), LJ_SW(4), LJ_SW(5), LJ_SW(6), LJ_SW(7), LJ_SW(8), LJ_SW(9), LJ_SW(10), LJ_SW(11), LJ_SW(12), LJ_SW(13), LJ_SW(14), LJ_SW(15), LJ_SW(16), LJ_SW(17), LJ_SW(18), LJ_SW(19), LJ_SW(20), LJ_SW(21), LJ_SW(22), LJ_SW(23), LJ_SW(24), LJ_SW(25), LJ_SW(26), LJ_SW(27), LJ_SW(28), LJ_SW(29), LJ_SW(30), LJ_SW(31), LJ_SW(32) };
