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
int lj_fs_replace(const char *tmp, const char *path);
/* ME-169: an MP Load File path (a .Cl2 line, the editor, the player) is untrusted text. -1 before any file call for a
 * UNC or device-namespace start (two separators), a ':' other than a drive letter's, a device name as any component's
 * stem (CON PRN AUX NUL COM0-9 LPT0-9 CONIN$ CONOUT$) or a name not ending in .wav; else 0 with out = path, or
 * root + path when path is relative (root = the library root with its separator; NULL = as given) */
int lj_fs_media_path(const char *root, const char *path, char *out, size_t cap);
extern unsigned lj_fs_exists_calls;   /* ME-169: counts lj_fs_exists, so test_cuelist proves a refused path never reaches it */   /* ME-139: remove path, rename tmp over it; 0, or -1 with tmp KEPT and its name logged (never the only copy deleted) */

#endif /* LJ_CORE_FSINFO_H */
