/* lj_ctl_lamp.h - the Lamp control (note 15/01 s9, 07 s9.6, lampcontrol.gif 133x100):
 * value buttons that program lamp on / lamp off / reset into the current scene
 * (lamp off and reset behind the "Confirm" box, strings verbatim from 13 s2.2)
 * and the white note box. Also renders the Extended window's Fixture Reset row. */
#ifndef LJ_CONTROLS_CTL_LAMP_H
#define LJ_CONTROLS_CTL_LAMP_H

#include "controls/lj_ctl_common.h"

#define LJ_CTL_CONFIRM_CAPTION "Confirm"
#define LJ_CTL_LAMP_OFF_TEXT   "Lamp Power Off\n(Warning : this command will be stored in the current scene)"   /* exe 0x131490 */
#define LJ_CTL_RESET_TEXT      "Reset Fixtures\n(Warning : this command will be stored in the current scene)"   /* exe 0x265834 */

int lj_ctl_lamp_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w);   /* LJ_W_LAMP_BTNS (a caption with "Off" or "Reset" confirms first), LJ_W_NOTE; row height */

#endif /* LJ_CONTROLS_CTL_LAMP_H */
