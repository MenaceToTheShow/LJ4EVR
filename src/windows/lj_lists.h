/* lj_lists.h - List of Sequences ([03 s4.4], CHM "List of sequences"): a code-built
 * form (LJ2 built it at runtime; there is no DFM) with the report list Seq No /
 * Sequence Name / Types / Scenes / Modified, click-to-sort, double-click or
 * popup "Edit Sequence" to load, Drag Mode (the lj_dnd source for cue slots and
 * the desktop: kind LJ_REC_SEQ, number), Browse to Edit, Browse to Cue. Caption
 * "Select Sequence (Drag To Target)" per mode [13 exe strings]. The List of Cues
 * (TCueListForm, windows-b) may be appended here. */
#ifndef LJ_WINDOWS_LISTS_H
#define LJ_WINDOWS_LISTS_H

#include <stdint.h>
#include "windows/lj_windows.h"

#define LJ_DND_SEQUENCE LJ_REC_SEQ       /* lj_dnd_begin kind for a sequence number (enum lj_rec_kind) */
enum lj_seqlist_mode { LJ_SEQLIST_DRAG = 0, LJ_SEQLIST_BROWSE_EDIT, LJ_SEQLIST_BROWSE_CUE };

lj_window *lj_seqlist_open(void);
void       lj_seqlist_close(void);
lj_window *lj_seqlist_get(void);
void       lj_seqlist_refresh(void);                 /* pending close */
void       lj_seqlist_rebuild(void);                 /* re-reads Sequences\ (after a save or delete); no-op while closed */
void       lj_seqlist_sort(int col);                 /* 0 Seq No, 1 Sequence Name, 2 Types, 3 Scenes, 4 Modified; ascending */
void       lj_seqlist_mode_set(enum lj_seqlist_mode mode);
int        lj_seqlist_count(void);
uint32_t   lj_seqlist_number_at(int row);            /* 0 when out of range */
int        lj_seqlist_selected(void);                /* row or -1 */

#endif /* LJ_WINDOWS_LISTS_H */
