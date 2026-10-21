/* lj_mac500.h - MAC 500 (exe id 12) and MAC 500 SCRL (31) hand profiles; the window
 * tables are shared with the Roboscan Pro 918 (lj_rs918.c), same DMX protocol [15/01]. */
#ifndef LJ_FIXLIB_HAND_MAC500_H
#define LJ_FIXLIB_HAND_MAC500_H

#include "fixlib/lj_profile.h"

enum { LJ_MAC500_WINDOWS = 8 };   /* Intensity, Movement, Colors, Gobos, Beam, Effects, Lamp Control, Extended: no Levels grid, no Smoke [15/01 s3] */
extern const lj_ctl_window lj_mac500_windows_fix[LJ_MAC500_WINDOWS];    /* gMOd = FIX: TGoboControl_Mac500_2 */
extern const lj_ctl_window lj_mac500_windows_scrl[LJ_MAC500_WINDOWS];   /* gMOd = SCRL: TGoboControl_Mac500S_2 */
extern const lj_profile lj_hand_mac500;        /* 12 Martin / MAC 500 / Mode 2/4 */
extern const lj_profile lj_hand_mac500_scrl;   /* 31 Martin / MAC 500 / SCRL, Mode 2/4 */

#endif /* LJ_FIXLIB_HAND_MAC500_H */
