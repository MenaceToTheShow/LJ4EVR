/* lj_save_dialogs.h - Save Sequence (TSaveSeqForm, [03 s4.2], [F28]): the number is
 * taken from lj_library_next_number on the first save and never changes; Save
 * Sequence overwrites the file (the [PREFS] WarnOWSeq prompt first), Save as New
 * Sequence takes a fresh number and leaves the original intact. Save options and
 * the Preferences menu persist in Main.Ini [SAVESEQFORM] (PROPOSAL: LJ2's own
 * section for this dialog is unknown; [SEQQUICK] UseOptions is LJ2's key for
 * "Quicksave use Save Options"). Save Cue (TSaveCueForm) is windows-b's and may
 * be appended here. */
#ifndef LJ_WINDOWS_SAVE_DIALOGS_H
#define LJ_WINDOWS_SAVE_DIALOGS_H

#include <stdint.h>
#include "windows/lj_windows.h"

lj_window *lj_save_seq_open(void);
void       lj_save_seq_close(void);
lj_window *lj_save_seq_get(void);
void       lj_save_seq_refresh(void);     /* fields from the model (number, name, type) and the pending close */

int  lj_save_seq_do(int as_new);          /* the save with the dialog's name/type/options; 0 / -1; a pending overwrite prompt counts as 0 */
void lj_save_seq_quick(void);             /* Quick Save Sequence: no dialog; a never-saved sequence opens the dialog instead */

#endif /* LJ_WINDOWS_SAVE_DIALOGS_H */
