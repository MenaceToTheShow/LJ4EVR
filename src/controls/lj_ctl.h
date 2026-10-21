/* lj_ctl.h - the fixture control windows (design 2.9, 6.3, 6.4, [04]): Intensity,
 * Movement, Colors, Gobos, Beam, Effects, Levels, Lamp, Extended (and W2's
 * Generic / RGB pack), each built from the master fixture's lj_ctl_window
 * table (fixlib) by the common row renderers, and bound to the current scene
 * through the O/S/F <-> channel API below. Code-built windows persist their
 * position in Main.Ini sections [INTENSITY] [MOVEMENT] [MACRO] ... (7.10).
 * Bodies: lj_ctl_common.c (open/close/refresh, row renderers, lj_prog_*),
 * lj_ctl_intensity.c ... lj_ctl_extended.c (per-kind specifics).
 *
 * Seam 2 (controls <-> current scene): a control reads and writes
 * ctx->show->current_seq->scenes[ctx->current_scene] under lj_lock through
 * lj_prog_get / lj_prog_set*; the engine's current-sequence runner reads the
 * same scene by pointer, so a change is on the wire at the next tick. Fade
 * state lives on the scene channel, so every button bound to that channel in
 * any open control repaints from one state (ME-27). lj_ctl_refresh() repaints
 * on the 100 ms timer and whenever lj_sel_serial() changed. */
#ifndef LJ_CONTROLS_CTL_H
#define LJ_CONTROLS_CTL_H

#include <stdint.h>
#include "desktop/lj_desktop.h"
#include "fixlib/lj_profile.h"
#include "ui/lj_window.h"

enum { LJ_PROG_KEEP = 255 };   /* lj_prog_set* state: leave the channel's fade state as it is */

lj_window *lj_ctl_open(enum lj_ctl_kind kind);             /* from lj_sel_master_profile()'s table; NULL + log when the master has no such window (toolbar button disabled, F9) or no master */
void       lj_ctl_close(enum lj_ctl_kind kind);
void       lj_ctl_close_all(void);
void       lj_ctl_toggle(enum lj_ctl_kind kind);           /* the toolbar button / hotkey: open if closed, else close */
void       lj_ctl_open_only(enum lj_ctl_kind kind);        /* right-click rule F9: open this one, close the others */
lj_window *lj_ctl_window_get(enum lj_ctl_kind kind);       /* NULL when closed */
void       lj_ctl_refresh(void);                           /* the 100 ms hook: master changed -> rebuild from the new table; else repaint values/states from the current scene */
const char *lj_ctl_caption(enum lj_ctl_kind kind, const lj_profile *p);   /* the table's caption ("Intensity", "Movement 16 Bit" ...) */

/* common row renderers (lj_ctl_common.c): build the widgets of one lj_ctl_row inside parent at (x, y, w); return the row height in px */
int  lj_ctl_row_build(lj_window *win, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w);
void lj_ctl_row_refresh(lj_widget *row_root);              /* repaint one built row from the current scene */
int  lj_ctl_value_text(char *dst, size_t cap, const lj_ctl_row *row, uint8_t v);   /* "Intensity 100,0 %" with the host decimal separator (D6, ME-52), or the raw value per row kind */

/* O/S/F <-> current scene channel binding (seam 2); ch = address code 0..2047 */
int  lj_prog_get(uint16_t ch, uint8_t *value, uint8_t *state);                    /* 0 / -1 when there is no current sequence */
void lj_prog_set(uint16_t ch, uint8_t value, uint8_t state);                      /* under lj_lock; state LJ_OFF/LJ_SNAP/LJ_FADE or LJ_PROG_KEEP; marks Seq. Modified */
void lj_prog_set16(uint16_t ch_msb, uint16_t ch_lsb, uint16_t value, uint8_t state);
void lj_prog_set_sel(uint8_t offset, uint8_t value, uint8_t state);               /* selection-wide: every selected fixture with the master's profile id, at base + offset */
void lj_prog_set_sel16(uint8_t msb_ofs, uint8_t lsb_ofs, uint16_t value, uint8_t state);
void lj_prog_set_state_sel(uint8_t offset, uint8_t state);                        /* the O/S/F button: state only, selection-wide */
int  lj_prog_cycle_state(uint16_t ch, int dir, uint8_t states_allowed);           /* D10: left (dir +1) Off->Snap->Fade->Off, right (dir -1) reverse; skips states not in the mask (ME-27); returns the new state */
int  lj_prog_master_ch(uint8_t offset);                                           /* address code of the master's channel at offset, -1 without a master */

#endif /* LJ_CONTROLS_CTL_H */
