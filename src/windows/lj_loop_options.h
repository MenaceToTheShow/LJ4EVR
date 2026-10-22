/* lj_loop_options.h - the Sequence slot loop options dialog (TLoopToolsForm "Loop Options",
 * 353x247, [03 s5.5], forms.ini [LOOPFORM] Top). Every radio, edit and check box maps to a
 * field of the working cue's lj_cue_slot (lj_loop_options_fields); the DFM has no Ok button,
 * so each change is the Ok: the dialog writes the slot(s) under lj_lock and posts one
 * LJ_CMD_LOOP_OPTS per target slot (slot = the pinned cue->slot[i]). Targets = the slots
 * highlighted in the Cue control, else slot 1. Closing without a change posts nothing. */
#ifndef LJ_WINDOWS_LOOP_OPTIONS_H
#define LJ_WINDOWS_LOOP_OPTIONS_H

#include <stddef.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

/* one row per control that edits the slot: DFM name, the lj_cue_slot member, its range */
enum lj_loop_field { LJ_LF_SPT = 0, LJ_LF_ATPT, LJ_LF_TRIG_ON_LOAD, LJ_LF_DIR, LJ_LF_FIRST, LJ_LF_LAST, LJ_LF_LOOPS,
                     LJ_LF_END, LJ_LF_GOTO, LJ_LF_LINK, LJ_LF_COUNT };
typedef struct { const char *ctl; enum lj_loop_field field; size_t offset, size; long lo, hi; } lj_loop_field_desc;
const lj_loop_field_desc *lj_loop_options_fields(int *n);   /* the table (tests walk it: every control maps to a member) */

lj_window *lj_loop_options_open(void);
void       lj_loop_options_close(void);
lj_window *lj_loop_options_get(void);
void       lj_loop_options_refresh(void);                   /* re-reads the slot when the Cue control's target changed */
int        lj_loop_options_target(void);                    /* first target slot 0..11 */
int        lj_loop_options_last_cmd(lj_cmd *out);           /* number of commands posted since open (tests); *out = the last */
void       lj_loop_options_bind_fns(void);                  /* LJ_FN_CUE_LOOP_CONTROL ("Cue Loop Control" hotkey) */

#endif /* LJ_WINDOWS_LOOP_OPTIONS_H */
