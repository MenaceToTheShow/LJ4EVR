/* lj_seq_form.h - the Sequence control (TSequenceForm, design 7.6, [03 s4], [07 s5]):
 * caption "SEQUENCE [0000 No Name]" [F19], the X:Y scene counter, Add / Insert /
 * Delete / Delete All [F29], prev / next / first / last, scene and fade time edits
 * in LJ2's s:hh text with the up/down pulses, Synchronize / Global Scene Time /
 * Global Fade Time, Disable Fade, Blind, playback, the W_PROGRESS2 bars
 * (LORD_FIDELITY-14) and the current-scene <-> engine binding (LJ_CMD_CURRENT_SEQ).
 * The model actions below work without the window (hotkeys, the toolbar, tests);
 * every mutation of the current sequence happens under lj_lock and is followed by
 * one LJ_CMD_CURRENT_SEQ so the engine re-pins the scene (seam 1). */
#ifndef LJ_WINDOWS_SEQ_FORM_H
#define LJ_WINDOWS_SEQ_FORM_H

#include <stddef.h>
#include <stdint.h>
#include "windows/lj_windows.h"

#define LJ_SEQFORM_SHRINK_TO_PANEL1 1   /* PROPOSAL rig (design 7.6 shared rig row, 07 s5 Unverified): on open the client becomes Panel1's 354x106 */
#define LJ_SEQ_TIME_STEP_MS 50          /* PROPOSAL rig: one up/down pulse = 0.05 s, the default fade quantum [03 s3.2] */

lj_window *lj_seq_form_open(void);
void       lj_seq_form_close(void);
lj_window *lj_seq_form_get(void);
void       lj_seq_form_refresh(void);                /* the 100 ms hook: progress bars from the engine's current runner, counter while playing, pending close */
void       lj_seq_form_update(void);                 /* fields, caption, counter and the toolbar from the model (after a save / load elsewhere) */
void       lj_seq_form_shrink(int on);               /* --explode passes 0 before the first open */
void       lj_seq_form_bind_fns(void);               /* the Sequence group of lj_fn (hotkeys); once at startup */

/* model actions [F29]; 0 / -1 */
int  lj_seq_form_add_scene(void);                    /* append a copy of the current scene, becomes current */
int  lj_seq_form_insert_scene(void);                 /* one copy of the current scene before it (the dialog's default) */
int  lj_seq_form_delete_scene(void);                 /* the current scene; the last one left is reset to all-off */
int  lj_seq_form_delete_all(void);                   /* one all-off scene */
int  lj_seq_form_delete_range(int first, int last);  /* W4 Delete Scene(s): 0-based inclusive, clamped to the sequence; all = one all-off scene */
int  lj_seq_form_insert_copies(int count);           /* W4 Insert: count copies of the current scene before it [03 s4.3]; the first copy becomes current */
int  lj_seq_form_insert_from(const lj_sequence *src, int first, int last);   /* W4 Insert: src scenes first..last (0-based; NULL = the current sequence) before the current scene */
int  lj_seq_form_goto(int index);                    /* 0-based; wraps at both ends */
int  lj_seq_form_set_times(uint32_t scene_ms, uint32_t fade_ms);   /* current scene; Synchronize / Global rules applied; clamped to LJ_TIME_MAX_MS */
void lj_seq_form_new(int force);                     /* "New Sequence": force = no "Current Sequence was modified" prompt */
int  lj_seq_form_load(uint32_t number, int force);   /* Sequences\NNNNNNNN.seq into show->current_seq; 0 / -1 (a prompt pending counts as 0) */
void lj_seq_form_post_current(void);                 /* LJ_CMD_CURRENT_SEQ: seq[0] = show->current_seq, a = current scene, b = blind */
void lj_seq_form_mark_modified(void);                /* seq->modified + the status bar flag */
int  lj_seq_time_parse(const char *text, uint32_t *ms);   /* "2:55" / "2,55" / "2.55" -> 2550; -1 when no digits */

/* the W_PROGRESS2 bars: blue = scene, red = fade [03 s4.1]; refresh feeds them, tests drive them directly */
void lj_seq_form_progress(uint32_t scene_elapsed_ms, uint32_t scene_ms, uint32_t fade_elapsed_ms, uint32_t fade_ms);

#endif /* LJ_WINDOWS_SEQ_FORM_H */
