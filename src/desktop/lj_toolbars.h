/* lj_toolbars.h - the three toolbar rows of the main window (design 7.7, [07 s4.3-4.6],
 * [01 s2.3], F6-F9): 30-px rows, 25x25 speed buttons with our glyphs, bold Seq / Cue /
 * C-L labels, the name fields that turn yellow, the X:Y counter, LED buttons, the
 * fixture toolbar enable rules. Public API: lj_desktop_toolbar_* / lj_desktop_seqcue_fields
 * in lj_desktop.h. Widget names (docs/ui-script.md): tb_seqcue_1.., tb_cuelist_1..,
 * tb_fixture_1.. (F9 order, 1 Intensity .. 9 Reset, 10 Hide/Show, 11 Groups, 12 All,
 * 13 None, 14 Inclusive/Exclusive, 15 Previous, 16 Next, 17 Solo, 18 Follow spot,
 * 19 OnLine/OffLine, 20 Freeze, 21 Smoke, 22 Blackout, 23 Master Intensity),
 * tb_seqname / tb_cuename / tb_clname, sb_seqcounter. */
#ifndef LJ_DESKTOP_TOOLBARS_H
#define LJ_DESKTOP_TOOLBARS_H

#include "ui/lj_window.h"

enum { LJ_TB_ROW_H = 30, LJ_TB_BTN = 25, LJ_TB_BTN_Y = 2 };   /* [07 s4.3]: 30-px rows, 25x25 TSpeedButtons; measured from toolandstatusbars.gif, rig item */

void lj_toolbars_build(lj_window *win, lj_widget *root);   /* three alTop panels under root, in row order */

#endif /* LJ_DESKTOP_TOOLBARS_H */
