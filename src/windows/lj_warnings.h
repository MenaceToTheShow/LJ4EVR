/* lj_warnings.h - the red warning boxes, the close prompt and the two message/confirm
 * dialogs (design 2.9, [03 s9], [13 s2.2], 12 Q9, F14, LORD_FIDELITY-12).
 * Forms: TSTATUSWARNINGFORM (LStatus rewritten per state: BLACKOUT ACTIVE /
 * FADE-OUT ACTIVE / FADE-IN ACTIVE), TFREEZEWARNFORM ("Frozen !"),
 * TOFFLINEWARNINGFORM ("DMX Output is Offline"), TCLOSEORSAVEFORM ("Close
 * Lightjockey ?", red with the orange panel), TCONFIRMFORM ("Please Confirm..").
 * Bodies of lj_win_warning / lj_win_close_prompt / lj_win_message live here. */
#ifndef LJ_WINDOWS_WARNINGS_H
#define LJ_WINDOWS_WARNINGS_H

#include "windows/lj_windows.h"

lj_window  *lj_warnings_window(enum lj_warning w);                 /* the open box for that state, NULL when hidden */
lj_window  *lj_win_warnings_any(void);                             /* the first open red box (lj_win_get(LJ_WIN_WARNINGS)), NULL when none */
const char *lj_warnings_status_text(enum lj_warning w);            /* the LStatus / Label1 text table, verbatim exe strings */
void        lj_warnings_center(const lj_window *box, int *x, int *y);   /* centre on the main window (lj_desktop_window + host->pos); -1 fallback = the design Left/Top */
void        lj_warnings_close_all(void);

/* TCONFIRMFORM with OK/Cancel ("Remove Selected Fixtures ?" and friends); result on the UI thread, 1 = OK */
lj_window  *lj_win_confirm(const char *caption, const char *line1, const char *line2, void (*on_result)(int yes, void *ud), void *ud);
lj_window  *lj_win_message_window(void);                          /* the open lj_win_message / lj_win_confirm box, NULL when none */
lj_window  *lj_win_close_prompt_window(void);                      /* the open TCLOSEORSAVEFORM, NULL when none */

/* ---- W4 dlg-desktop (appended): the LStatus table's source tags, the two 'Notification !' boxes and the laser pair; the
 * DESKTOP family (lj_desktop_dialogs.c) dispatches LJ_WIN_HOTKEY_WARNING / HARDWARE_WARNING / LASER_WARNING / UNBLOCK_LASERS
 * here. lj_win_message_window() also answers these boxes when no confirm box is open (assert_prompt, shot). */
const char *lj_warnings_status_source(enum lj_warning w);          /* "exe 0x18DFE0 ..." / "DFM TFREEZEWARNFORM Label1" */
lj_window  *lj_warnings_w4_open(enum lj_win id);
lj_window  *lj_warnings_w4_get(enum lj_win id);
void        lj_warnings_w4_close(enum lj_win id);
void        lj_warnings_w4_refresh(enum lj_win id);                /* the 100 ms poll: Edit Hotkeys / Select Function opening, no sACN universe mapped, laser demand / emission */
int         lj_laser_fixture(int fixture);                         /* 1 = a laser profile (118 MAC250L, "Laser" in the name) */
int         lj_laser_enabled(void);                                /* 1 after 'Enable laser output ?' Yes, 0 = blocked (start-up state, or clicked on the warning) */

#endif /* LJ_WINDOWS_WARNINGS_H */
