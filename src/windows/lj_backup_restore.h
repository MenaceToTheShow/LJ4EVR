/* lj_backup_restore.h - Backup User Library (TLibraryBackupForm 569x257) and Restore User Library
 * (TLibraryRestoreForm 715x299) [01 s8, F51, design 5.2]: the open library zipped to
 * <userdir><name>.lj4b with the Notes box as notes.txt (model lj_library_backup), and a .lj4b
 * unzipped back into the open library (lj_library_restore) after the confirm prompt, then the
 * "Re-start LightJockey ?" prompt (LJ_ACT_RESTART), as restoring into the active library restarts
 * LJ2. The Restore form's TDirectoryListBox / TFileListBox have no widget (ME-17): code-built TListBoxes
 * over their rects carry the folder tree (W4-DLG-1, from <userdir>) and the *.lj4b files of the folder
 * picked; the drive combo stays hidden. A restore replaces the library (lj_library_replace). */
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

/* W4 dlg-files: the Restore folder tree */
int lj_restore_set_dir(const char *dir);   /* the folder the tree and the file list show; -1 empty or too long */
int lj_restore_dir_row(int row);           /* a tree row: 0..ancestors-1 = that ancestor (0 = the root), then the sub-folders; -1 out of range */

#endif /* LJ_WINDOWS_BACKUP_RESTORE_H */
