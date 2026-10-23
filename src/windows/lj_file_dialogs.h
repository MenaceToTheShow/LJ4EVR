/* lj_file_dialogs.h - W4 window family FILES (docs/w4-contracts.md section 3), owned by the dlg-files track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_file_dialogs.c and adds what it needs below the contract block.
 *   LJ_WIN_BACKUP_PROGRESS  TBackupForm: Backup User Library progress 'Backing Up' [01 s8]
 *   LJ_WIN_RESTORE_LIB_PROGRESS  TRestoreLibProgressForm: Restore User Library progress [01 s8]
 *   LJ_WIN_COPYING_FILES  TCopyingFilesForm: User Libraries copy progress [01 s8]
 *   LJ_WIN_RENAME_FILE  TRenameFileForm: User Libraries Rename [01 s8]
 *   LJ_WIN_PROGRESS  TProgressForm: generic file progress [01 s8]
 *   LJ_WIN_PRINT_WAIT  TPrintWaitForm: print spool wait (Print stays greyed: fidelity row) [01 s8]
 *   LJ_WIN_RESTORE_DATA  TRestoreDataForm: System > Restore Program Data (LJ_ACT_RESTORE_DATA), System Preferences Data Restore tab [06 s4.2]
 *   LJ_WIN_CREATE_DATA  TCreateDataForm: Creating Auto Restore Data (System Preferences Data Restore) [06 s4.2]
 *   LJ_WIN_COMPRESS_RESTORE  TCompressRestoreDataForm: Compressing Restore Data progress [06 s4.2]
 *   LJ_WIN_RESTORE_PROGRESS  TRestoreProgressForm: Restoring Files progress [06 s4.2]
 *   LJ_WIN_NEW_PASSWORD  TNewPasswordForm: System Preferences Data Restore Enter New Password [06 s4.2]
 *   LJ_WIN_UPDATE  TUpdateForm: first start on an older record format 'Dataformat has Changed' [F56, 12 Q9]
 *   LJ_WIN_UPDATE_FILE  TUpdateFileForm: format update progress 'Updating Files' [F56]
 *   LJ_WIN_UPDATE_PROGRAM  TUpdateProgramForm: installation update progress [F56]
 *   LJ_WIN_UPDATE_BGCUES  TUpdateBGCuesForm: older BG cue files 'File Format Update Needed' [F56]
 *   LJ_WIN_UPDATE_CL2  TUpdateCl2Form: older .Cl2 'Datafile format update required' [F56, 12 Q9]
 *   LJ_WIN_PATCH_UPDATE  TPatchUpdateForm: older patch 'Update to Patch Configuration' [F56, 12 Q9]
 *   LJ_WIN_REBUILD_SETUP  TRebuildSetupForm: setup rebuild 'Please Wait...' [F56]
 *   LJ_WIN_INIT_FORM  TInitForm1: first-start splash TInitForm1 [F56]
 *   LJ_WIN_PLEASE_WAIT  (existing id)
 */
#ifndef LJ_WINDOWS_FILE_DIALOGS_H
#define LJ_WINDOWS_FILE_DIALOGS_H

#include "windows/lj_windows.h"

lj_window *lj_file_dlg_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_file_dlg_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_file_dlg_get(enum lj_win id);       /* NULL when closed */
void       lj_file_dlg_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_file_dlg_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */


/* ---- dlg-files additions (below the contract block) ---- */
enum { LJ_FILE_STATUS_LINES = 64, LJ_FILE_SELFDESTRUCT_S = 10 };   /* PROPOSAL rig: the countdown start is unseen (Timer1 has the default 1000 ms) */
#define LJ_FILE_SELFDESTRUCT_FMT "This window will selfdestruct in %d seconds (click anywhere in list to keep open)"   /* exe 0x21A6E8, 12 Q9 */
void lj_file_dlg_status_add(const char *line);                    /* one Startup Status line kept (the last LJ_FILE_STATUS_LINES); app/lj_startup.c feeds it during the load */
int  lj_file_dlg_status_rows(const char *const **rows);           /* the kept lines, oldest first; count */
int  lj_file_dlg_startup(void);                                   /* the start checks (F56, W4-DLG-2): the corrupt-restore prompt when RESTORE=1 survived lj_startup_library, then one update dialog per group of older LJ4 records (.Cl2 -> TUpdateCl2Form unless Main.Ini [UPDATE] NoCl2Reminder=1, .bgc -> TUpdateBGCuesForm, patch -> TPatchUpdateForm, any other kind -> TUpdateForm); M_RestoreProgramData shown only when restore data exists. Returns the windows / prompts opened. lj_file_dlg_bind_fns runs it once when a host is installed; tests call it headless */
void lj_file_dlg_progress(enum lj_win id, const char *label, const char *text, int pos);   /* a progress box: opens it when closed, label's caption = text (both non-NULL), its bar = pos; pos < 0 closes it */
void lj_file_dlg_new_password(void);                              /* System Preferences New Password: the change prompt when a password is set, then TNewPasswordForm */
int  lj_file_dlg_unlock(const char *pw);                          /* the Data Restore tab's Password OK: 0 = right (or none set), -1 = "Incorrect Password" shown */
int  lj_file_dlg_unlocked(void);                                  /* 1 after a right password (or one just set) in this run */
lj_window *lj_file_dlg_rename(const char *from, void (*done)(int ok, void *ud), void *ud);   /* TRenameFileForm for User Libraries Rename: eFileName = from, OK renames the library, then done(1, ud) */

#endif /* LJ_WINDOWS_FILE_DIALOGS_H */
