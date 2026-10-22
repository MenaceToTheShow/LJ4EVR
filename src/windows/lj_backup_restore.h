/* lj_backup_restore.h - Backup User Library (TLibraryBackupForm 569x257) and Restore User Library
 * (TLibraryRestoreForm 715x299) [01 s8, F51, design 5.2]: the open library zipped to
 * <userdir><name>.lj4b with the Notes box as notes.txt (model lj_library_backup), and a .lj4b
 * unzipped back into the open library (lj_library_restore) after the confirm prompt, then the
 * "Re-start LightJockey ?" prompt (LJ_ACT_RESTART), as restoring into the active library restarts
 * LJ2. The Restore form's TDriveComboBox / TDirectoryListBox / TFileListBox have no widget yet
 * (ME-17, ui owner): the two folder controls are hidden and the file list is a code-built TListBox
 * over FileList's rect listing <userdir><library>.lj4b for every library that has one. */
#ifndef LJ_WINDOWS_BACKUP_RESTORE_H
#define LJ_WINDOWS_BACKUP_RESTORE_H

#include <stddef.h>
#include "windows/lj_windows.h"

lj_window *lj_backup_open(void);      /* LJ_WIN_BACKUP */
void       lj_backup_close(void);
lj_window *lj_backup_window(void);
void       lj_backup_refresh(void);
int        lj_backup_path(char *out, size_t cap);          /* <userdir><library name>.lj4b; -1 truncated / no library */
int        lj_backup_set_notes(const char *text);          /* the Notes box; -1 when closed */
int        lj_backup_create(const char *path);             /* writes the backup now (no prompt); NULL = lj_backup_path; 0 / -1 (the error box shown) */

lj_window *lj_restore_open(void);     /* LJ_WIN_RESTORE */
void       lj_restore_close(void);
lj_window *lj_restore_window(void);
void       lj_restore_refresh(void);
int        lj_restore_set_file(const char *path);          /* picks a .lj4b: notes into Re1, labels filled; -1 when not readable */
int        lj_restore_run(void);                           /* bRestore: the confirm prompt; 0 / -1 (no file) */

#endif /* LJ_WINDOWS_BACKUP_RESTORE_H */
