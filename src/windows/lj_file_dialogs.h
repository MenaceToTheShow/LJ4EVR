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

#endif /* LJ_WINDOWS_FILE_DIALOGS_H */
