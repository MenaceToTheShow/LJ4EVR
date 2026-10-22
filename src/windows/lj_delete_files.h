/* lj_delete_files.h - System > Delete Files: the five multi-select delete dialogs (CHM "Deleting
 * multiple data files", [01 s4], W2): TDeleteSequencesForm "Delete Sequences" (+ Check Used
 * Sequences / Mark Unused Sequences), TDeleteCuesForm "Delete Cues", TDeleteCuelistsForm "Delete
 * Cuelists", TDeleteBGCuesForm "Delete Background Cues", TDeleteGenericMacrosForm "Delete Generic
 * Macros" (LJ_WIN_DELETE_SEQ .. LJ_WIN_DELETE_MACRO, kind = enum lj_rec_kind in that order; the
 * desktop's LJ_ACT_DELETE_FILES arg). Rows are the records on disk (lj_library_list_records);
 * Select All / Clear All / Delete with the exe's confirm prompts (13 s2.2); deleting goes through
 * the deleter the app installs (lj_app_delete_record: the engine is handed NULL first, design 2.3
 * rule 6); without one the file alone is removed (lj_library_delete_record), never a pinned record. */
#ifndef LJ_WINDOWS_DELETE_FILES_H
#define LJ_WINDOWS_DELETE_FILES_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_library.h"
#include "windows/lj_windows.h"

lj_window *lj_delete_files_open(enum lj_rec_kind kind);
void       lj_delete_files_close(enum lj_rec_kind kind);
lj_window *lj_delete_files_window(enum lj_rec_kind kind);
void       lj_delete_files_refresh(enum lj_rec_kind kind);       /* the pending close */

/* the lj_win_table rows (one per LJ_WIN_DELETE_* id) */
lj_window *lj_delete_seq_open(void);   void lj_delete_seq_close(void);   lj_window *lj_delete_seq_get(void);   void lj_delete_seq_refresh(void);
lj_window *lj_delete_cue_open(void);   void lj_delete_cue_close(void);   lj_window *lj_delete_cue_get(void);   void lj_delete_cue_refresh(void);
lj_window *lj_delete_cl_open(void);    void lj_delete_cl_close(void);    lj_window *lj_delete_cl_get(void);    void lj_delete_cl_refresh(void);
lj_window *lj_delete_bg_open(void);    void lj_delete_bg_close(void);    lj_window *lj_delete_bg_get(void);    void lj_delete_bg_refresh(void);
lj_window *lj_delete_macro_open(void); void lj_delete_macro_close(void); lj_window *lj_delete_macro_get(void); void lj_delete_macro_refresh(void);

/* rows (the UI script and tests drive them the way the list does) */
int      lj_delete_files_count(enum lj_rec_kind kind);
uint32_t lj_delete_files_number(enum lj_rec_kind kind, int row);        /* 0 out of range */
int      lj_delete_files_selected(enum lj_rec_kind kind, int row);
void     lj_delete_files_select(enum lj_rec_kind kind, int row, int on);
int      lj_delete_files_in_use(int row);                              /* sequences after Check Used Sequences: 1 = a cue or BG cue references it */
int      lj_delete_files_prompt(enum lj_rec_kind kind, int n, char *line1, size_t cap1, char *line2, size_t cap2);   /* the confirm text for n selected records; 0 / -1 */

int      lj_delete_files_record_name(enum lj_rec_kind kind, uint32_t number, char *name, size_t cap);   /* the record's name from its file ("" when it does not load); Edit Hotkeys' program lists share it */

/* the app's deleter (lj_app_delete_record); NULL = file only */
void     lj_delete_files_set_deleter(int (*fn)(int kind, uint32_t number));
int      lj_delete_files_record(enum lj_rec_kind kind, uint32_t number);   /* through the app's deleter (file + table, engine handed NULL first), else the file only; 0 / -1 */

#endif /* LJ_WINDOWS_DELETE_FILES_H */
