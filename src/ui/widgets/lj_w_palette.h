/* lj_w_palette.h - one palette tile (note 07 §3.3): 31x30, the wheel-position bitmap
 * (or a flat colour) under a 1-px grey frame, a thick black frame when selected. */
#ifndef LJ_UI_WIDGETS_W_PALETTE_H
#define LJ_UI_WIDGETS_W_PALETTE_H

#include <stdint.h>
#include "gfx/lj_surface.h"
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_PALETTE_W 31
#define LJ_PALETTE_H 30

typedef struct { lj_widget base; const lj_surface *tile; uint32_t swatch_rgb; int index; unsigned selected:1; } lj_w_palette;

lj_widget *lj_w_palette_create(lj_widget *parent, const lj_ctl_desc *d);   /* tag = index, color = swatch */
void       lj_w_palette_set_tile(lj_widget *w, const lj_surface *tile, uint32_t swatch_rgb);
void       lj_w_palette_select(lj_widget *w, int on);                       /* the owner clears the other tiles of the row */
int        lj_w_palette_selected(const lj_widget *w);

#endif /* LJ_UI_WIDGETS_W_PALETTE_H */
