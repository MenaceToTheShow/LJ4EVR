/* lj_widgets_lj.h - the LJ-specific widget group (design 7.2): the four custom/extra
 * Delphi classes TUpDown, TUdButton, TSpinEdit, TDipSwitch and the code-built LJ2
 * controls (O/S/F, drag bar, scroll control, palette tile, vertical fader, LED
 * button, fixture icon tile, cue grid row, statics slot button).
 *
 * Every class is lj_widget *lj_w_<class>_create(parent, desc): ONE calloc
 * (lj_w_alloc, shared with the lists group) with lj_widget as its first member,
 * appended to parent when parent != NULL; free() releases it. The code-built
 * classes take a hand-filled lj_ctl_desc (left/top/width/height, caption, colours)
 * the same way. Mouse coordinates arrive relative to the widget; a drag (drag bar,
 * fader) expects the window to keep sending MOUSE_MOVE/UP until the button goes
 * up (Delphi mouse capture), as the other groups assume. */
#ifndef LJ_UI_WIDGETS_WIDGETS_LJ_H
#define LJ_UI_WIDGETS_WIDGETS_LJ_H

#include <stdint.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"
#include "ui/widgets/lj_widgets_lists.h"      /* lj_w_alloc, lj_w_font_of, lj_w_emit, lj_widget_ctor, LJ_VK_* */
#include "ui/widgets/lj_w_updown.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "ui/widgets/lj_w_spinedit.h"
#include "ui/widgets/lj_w_dipswitch.h"
#include "ui/widgets/lj_w_osf.h"
#include "ui/widgets/lj_w_dragbar.h"
#include "ui/widgets/lj_w_scrollctl.h"
#include "ui/widgets/lj_w_palette.h"
#include "ui/widgets/lj_w_fader.h"
#include "ui/widgets/lj_w_led.h"
#include "ui/widgets/lj_w_fixicon.h"
#include "ui/widgets/lj_w_cuerow.h"
#include "ui/widgets/lj_w_slotbtn.h"

#define LJ_VK_BACK 0x08

int lj_widgets_register_lj(lj_widget_ctor *table, int count);   /* UPDOWN, UDBUTTON, SPINEDIT, DIPSWITCH; -1 if count < LJ_K_COUNT */

/* Shared helpers for the lj_w_*.c files of this group (lj_widgets_lj.c). */
int  lj_w_inside(const lj_widget *w, int x, int y);
void lj_w_button(lj_surface *s, int x, int y, int w, int h, uint32_t face, int pressed);   /* raised per 7.3, or pressed = the sunken edge; face filled */
void lj_w_tri(lj_surface *s, int x, int y, int n, int dir, uint32_t rgb);                 /* solid triangle, apex at x,y, n rows/cols (widths 1,3,..2n-1); dir 0 apex up, 1 apex down, 2 apex left, 3 apex right */
void lj_w_spin_pair(lj_surface *s, const lj_widget *w, int x, int y, int bw, int bh, int pressed);   /* two stacked arrow buttons (TUpDown/TUdButton/TSpinEdit); pressed 0/1 top/2 bottom */
void lj_w_text_c(lj_surface *s, const lj_widget *w, int x, int y, int bw, int bh, const char *str, uint32_t rgb, int accel);   /* centred in the box; disabled rule applied */
void lj_w_text_fit(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb, int maxw);                /* left-aligned, characters dropped from the end until it fits */

#endif /* LJ_UI_WIDGETS_WIDGETS_LJ_H */
