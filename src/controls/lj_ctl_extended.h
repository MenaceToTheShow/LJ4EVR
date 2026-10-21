/* lj_ctl_extended.h - the Extended control TSpecialSettings_RS918S (note 15/01 s10,
 * extended.gif 277x81; ME-10): per row one group box with its radios and the speed
 * fader on the right (Pan/Tilt Settings ch 15, Effects Settings ch 16); the Fixture
 * Reset row is an LJ_W_LAMP_BTNS row (lj_ctl_lamp.h). Opened by Fixture toolbar
 * button 7 for the MAC 500 / 918 family. Row encoding: buttons[] = the radios
 * (caption, value, lo..hi); the radio whose lo..hi equals row->lo..hi is "Speed"
 * and emits lo + the fader position. */
#ifndef LJ_CONTROLS_CTL_EXTENDED_H
#define LJ_CONTROLS_CTL_EXTENDED_H

#include "controls/lj_ctl_common.h"

enum { LJ_CTL_EXT_COL_W = 139, LJ_CTL_EXT_BOX_W = 137, LJ_CTL_EXT_BOX_H = 81 };   /* measured from extended.gif: boxes x 1-137 and 139-276, 81 tall */

int lj_ctl_extended_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y);   /* LJ_W_RADIOGROUP_FADER: one column at x; returns the box height */

#endif /* LJ_CONTROLS_CTL_EXTENDED_H */
