/* lj_restore_data.h - Data Restore, the club "show-saver" snapshot ([06 s4.2], CHM "Using the data restore option",
 * W4-DLG-2). Our format, not LJ2's LightJockey.Bin (docs/file-formats.md "Restore data"): one file
 * <userdir>RestoreData.lj4d per installation = a .lj4b of the library (lj_library_backup) whose notes.txt is the
 * manifest "LJ4 restore 1" / library="<name>" / created=<ISO time>. Restore Program Data sets Startup.ini
 * [RESTORELIB] RESTORE=1 (LJ2's key [06 s2]); the next start checks the whole archive (lj_library_backup_check),
 * erases the library and extracts it before Main.Ini is read (app/lj_startup.c). The password is a prompt, not a
 * security boundary: an FNV-1a hash in Startup.ini [RESTORELIB] Pass= that anyone with the file can clear. */
#ifndef LJ_MODEL_RESTORE_DATA_H
#define LJ_MODEL_RESTORE_DATA_H

#include <stddef.h>
#include "model/lj_library.h"

#define LJ_RESTORE_DATA_FILE "RestoreData.lj4d"   /* PROPOSAL name (LJ2: LightJockey.Bin in the installation folder); .lj4d, not .lj4r, the replay scenario extension (ME-146) */
enum { LJ_RESTORE_FORMAT = 1, LJ_RESTORE_PASS_MAX = 16 };   /* MaxLength = 16 of TNewPasswordForm Edit1 / Edit2 and TSystemPrefsForm ePassword */

int lj_restore_data_path(const char *userdir, char *out, size_t cap);                 /* <userdir>RestoreData.lj4d; -1 truncated */
int lj_restore_data_exists(const char *userdir);                                       /* 1 / 0 */
int lj_restore_data_create(const lj_library *lib, const char *path);                   /* the snapshot through <path>.tmp then renamed over path; 0 / -1 (path untouched) */
int lj_restore_data_info(const char *path, char *library, size_t cap);                 /* the manifest's library name (checked like a folder name); 0 / -1 (no manifest, newer format, bad name) */
int lj_restore_data_apply(const char *userdir, const char *path, lj_library *out);     /* info, create/open the library, pass 1 over every entry and CRC, then erase + extract; 0 / -1 (nothing erased when the archive fails pass 1) */
int lj_restore_data_request(const char *userdir, int on);                               /* Startup.ini [RESTORELIB] RESTORE=1 / 0; 0 / -1 */
int lj_restore_data_requested(const char *userdir);                                     /* 1 when RESTORE=1 */
int lj_restore_password_set(const char *userdir, const char *pw);                     /* "" or NULL clears; 0 / -1 (longer than 16) */
int lj_restore_password_isset(const char *userdir);                                     /* 1 / 0 */
int lj_restore_password_check(const char *userdir, const char *pw);                   /* 1 = matches (or none set), 0 = wrong */

#endif /* LJ_MODEL_RESTORE_DATA_H */
