/* lj_fsinfo.h - file-system facts the windows show (W3 movement, LORD_FIDELITY-12): a file's modification time as
 * the Delete dialogs' "Modified" column text, and creating one folder (the MacroShapes folder of a library). */
#ifndef LJ_CORE_FSINFO_H
#define LJ_CORE_FSINFO_H

#include <stddef.h>

/* "dd-mm-yyyy hh:mm" in local time (PROPOSAL format: LJ2's column text is not in any capture); "" and -1 when the
 * file cannot be stat'ed */
int lj_fs_mtime_text(const char *path, char *out, size_t cap);
int lj_fs_mkdir(const char *path);   /* 0 when created or already there, -1 otherwise */
int lj_fs_exists(const char *path);  /* 1 when the file opens for reading, else 0 (ME-101: was static in model/lj_show.c) */

#endif /* LJ_CORE_FSINFO_H */
