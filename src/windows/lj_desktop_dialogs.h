/* lj_desktop_dialogs.h - W4 window family DESKTOP (docs/w4-contracts.md section 3), owned by the dlg-desktop track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_desktop_dialogs.c and adds what it needs below the contract block.
 *   LJ_WIN_AUTO_ARRANGE  TAutoArrangeForm: desktop popup Auto-Arrange Icons (LJ_ACT_AUTO_ARRANGE) [01 s3.5]
 *   LJ_WIN_DESKTOP_GRID  TGridForm: desktop popup Desktop Grid (LJ_ACT_DESKTOP_GRID) [01 s3.5, F13]
 *   LJ_WIN_ICON_STYLE  TIconStyleForm: desktop popup Select Icon Style (LJ_ACT_ICON_STYLE) [01 s3.5]
 *   LJ_WIN_DESKTOP_IMAGE  TImageSetupForm: desktop popup Desktop Image (LJ_ACT_DESKTOP_IMAGE) [01 s3.5, F13]
 *   LJ_WIN_DESKTOP_TABS  TLayerManagerForm: desktop popup Desktop tabs (LJ_ACT_DESKTOP_TABS) [01 s3.5]
 *   LJ_WIN_NEXTPREV_SELECTION  TNextPrevSelectionForm: right-click prev/next (LJ_ACT_NEXTPREV_SETTINGS) [01 s3.3]
 *   LJ_WIN_CUSTOM_ORDER  TCustomOrderForm: Next/Previous Fixture Selection Custom order [01 s3.3]
 *   LJ_WIN_SAVE_SELECTION  TSaveSelectionForm: Fan Out named orders / Custom order Save [01 s3.3, 17 s5.2]
 *   LJ_WIN_AUTO_LAUNCH  TAutoLaunchForm: Startup Options auto-launch programs, behind the Execute File rule (LORD_C-8) [01 s6.2]
 *   LJ_WIN_HOTKEY_WARNING  TShowHotKeyWarningForm: Edit Hotkeys / Select Function 'Notification !' (Main.Ini [MISC] ShowHotkeyWarning) [01 s7]
 *   LJ_WIN_HARDWARE_WARNING  TShowHardwareWarningForm: startup without transmitter hardware 'Notification !' [12 Q9]
 *   LJ_WIN_LASER_WARNING  TLaserWarningForm: laser output active 'Warning!' Laser(s) emitting! [12 Q9]
 *   LJ_WIN_UNBLOCK_LASERS  TUnblockLasersForm: 'Enable laser output ?' before laser output opens [12 Q9]
 *   LJ_WIN_DEFAULT_OUTPUT  (existing id)
 */
#ifndef LJ_WINDOWS_DESKTOP_DIALOGS_H
#define LJ_WINDOWS_DESKTOP_DIALOGS_H

#include "windows/lj_windows.h"

lj_window *lj_desktop_dlg_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_desktop_dlg_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_desktop_dlg_get(enum lj_win id);       /* NULL when closed */
void       lj_desktop_dlg_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_desktop_dlg_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

#endif /* LJ_WINDOWS_DESKTOP_DIALOGS_H */
