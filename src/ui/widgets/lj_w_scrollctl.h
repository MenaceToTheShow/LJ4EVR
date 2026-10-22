/* lj_w_scrollctl.h - the wheel scroll control (note 07 §3.3, goboscroll.gif 132x33
 * measured; the crop is a 0.8 reduction like the control-window GIFs, ME-67, so
 * every number is x1.25, rig item): a raised group of four 29x30 buttons `◄` `|◄`
 * [swatch] `►|` `►` around a 45x37 black well holding the current 31x30 colour/gobo
 * tile (the 1:1 Colors\ / Gobos\ bitmaps). `◄`/`►` move one whole effect (big),
 * `|◄`/`►|` one DMX step. */
#ifndef LJ_UI_WIDGETS_W_SCROLLCTL_H
#define LJ_UI_WIDGETS_W_SCROLLCTL_H

#include <stdint.h>
#include "gfx/lj_surface.h"
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_SCROLLCTL_W 165
#define LJ_SCROLLCTL_H 41

typedef struct {
    lj_widget base;
    int value, max, big, last;    /* value 0..max (255); big = one effect's DMX span (default 16); last = -2 -1 +1 +2 of the last click */
    const lj_surface *tile;       /* 31x30 tile blitted into the well, NULL = flat swatch_rgb */
    uint32_t swatch_rgb;
    unsigned pressed:3;           /* 1..4 = button held */
} lj_w_scrollctl;

lj_widget *lj_w_scrollctl_create(lj_widget *parent, const lj_ctl_desc *d);   /* position (value), max, step (big) from the desc */
int        lj_w_scrollctl_value(const lj_widget *w);
int        lj_w_scrollctl_set_value(lj_widget *w, int v);                    /* clamps; no event */
void       lj_w_scrollctl_set_tile(lj_widget *w, const lj_surface *tile, uint32_t swatch_rgb);
int        lj_w_scrollctl_last(const lj_widget *w);                          /* which button fired the last OnChange: -2 -1 +1 +2 */

#endif /* LJ_UI_WIDGETS_W_SCROLLCTL_H */
