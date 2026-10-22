/* lj_library.h - the user library folder (design 5.1, [01 s8]): LJ2's folder
 * and file names under <userdir>/User/<name>/, DirInfo.Ini [NEXT] numbering
 * (design 5.2), the Default rule (cannot be renamed or deleted, recreated when
 * missing), listing for the User Libraries dialog. Paths are CP1252 bytes
 * (the CRT is UTF-8 on Windows through the manifest, design 2.1); every path
 * buffer is LJ_PATH_CAP. No lock: library calls are UI-thread file I/O. */
#ifndef LJ_MODEL_LIBRARY_H
#define LJ_MODEL_LIBRARY_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_ini.h"

enum { LJ_PATH_CAP = 1024, LJ_LIBNAME_CAP = 64 };

typedef struct { char userdir[LJ_PATH_CAP]; char name[LJ_LIBNAME_CAP]; char root[LJ_PATH_CAP]; } lj_library;   /* root = <userdir>User/<name>/ with a trailing separator */

enum lj_rec_kind { LJ_REC_SEQ = 0, LJ_REC_CUE, LJ_REC_CL, LJ_REC_BG, LJ_REC_MACRO, LJ_REC_KIND_COUNT };   /* DirInfo.Ini [NEXT] Seq= Cue= Cl= Bg= Macro= */

int  lj_library_open(const char *userdir, const char *name, lj_library *out);        /* existing folder; 0 / -1 + log */
int  lj_library_create(const char *userdir, const char *name, lj_library *out);      /* folder + sub-folders (Sequences Cues CueLists CueMacros BGCues UserFixtureDef MIDIIn Presets Palettes) + an empty DirInfo.Ini; 0 / -1 */
int  lj_library_open_default(const char *userdir, lj_library *out);                  /* "Default", created when missing [01 s8] */
int  lj_library_list(const char *userdir, char names[][LJ_LIBNAME_CAP], int max);    /* folders under <userdir>User/; returns the count, -1 on error */
int  lj_library_delete(const char *userdir, const char *name);                       /* refuses "Default" (-1) */
int  lj_library_rename(const char *userdir, const char *from, const char *to);       /* refuses "Default" (-1) */
int  lj_library_is_default(const lj_library *lib);

int  lj_library_path(const lj_library *lib, const char *rel, char *out, size_t cap);              /* root + rel ("Config.dat", "Cues\\CuePageInfo.Dat"); -1 when truncated */
int  lj_library_record_path(const lj_library *lib, enum lj_rec_kind kind, uint32_t number, char *out, size_t cap);   /* "Sequences\\00000012.seq" ... 8-digit names [F28] */
uint32_t lj_library_next_number(lj_library *lib, enum lj_rec_kind kind);                          /* allocates and persists the next [NEXT] number (design 5.2); recovery = max existing + 1 when the key is missing; 0 on error */
int  lj_library_list_records(const lj_library *lib, enum lj_rec_kind kind, uint32_t *numbers, int max);   /* numbers present on disk, ascending; count or -1 */
int  lj_library_delete_record(const lj_library *lib, enum lj_rec_kind kind, uint32_t number);     /* the number is never reused */

/* W1 model additions (the brief: backup [F51], ME-24, the 4-digit display rule [F28]). */
int  lj_library_backup(const lj_library *lib, const char *zip_path, const char *notes);           /* the whole folder into <name>.lj4b (miniz) with notes.txt; 0 / -1 */
int  lj_library_restore(const lj_library *lib, const char *zip_path);                            /* unzips into the existing library (entries with ".." or an absolute path are refused); 0 / -1 */
int  lj_library_tick_hz(lj_ini *main_ini);                                                        /* Main.Ini [ENGINE] TickHz, default 40, clamped to 10..200 (ME-24; engine/lj_engine_rules.h spells the same limits) */
int  lj_number_text(uint32_t number, char *out, size_t cap);                                      /* "0012": the 4-digit display form of a record number [F28]; -1 truncated */


/* ---- W2 additions: the Backup / Restore / User Libraries dialogs ---- */
#define LJ_LIBRARY_BACKUP_EXT ".lj4b"
int  lj_library_backup_notes(const char *zip_path, char *notes, size_t cap);          /* notes.txt out of a .lj4b for the Restore dialog's notes box; 0 / -1 (not a .lj4b, no notes.txt) */
int  lj_library_backup_lists(const char *zip_path, int counts[LJ_REC_KIND_COUNT]);    /* per-kind record counts inside the archive (the Restore dialog's summary); 0 / -1 */
int  lj_library_copy(const char *userdir, const char *from, const char *to);           /* "Create New Library" with "copy files" [01 s8]: creates `to` and copies every file of `from`; 0 / -1 */

/* W2 model track additions (append-only). */
int  lj_library_restore_new(const char *userdir, const char *name, const char *zip_path, lj_library *out);   /* restore a .lj4b into a NEW library: `name`, or "name (2)" .. "name (99)" when taken (collision rule PROPOSAL rig: LJ2 restores over the library [01 s8]); DirInfo.Ini Name set to the new name; every entry is checked before the first byte is written; 0 / -1 (nothing left behind) */
int  lj_library_select(const char *userdir, const char *name);                         /* "Select User Library": <userdir>Startup.ini [USERDIRECTORY] Start=<name>\ (LJ2's key, read at the next start, 18 s2); 0 / -1 */
int  lj_library_used_by_cues(const lj_library *lib, enum lj_rec_kind kind, const uint32_t *numbers, int n, uint8_t *used);   /* Delete Files "Check Used Sequences" [03 s4.4, 06 s4]: used[i] = 1 when a cue on disk references numbers[i] (ascending, as lj_library_list_records returns them): SEQ = a slot, BG = bgcue=, MACRO = genmacro=; count used, -1 on error. Unreadable cues are skipped and logged */

#endif /* LJ_MODEL_LIBRARY_H */
