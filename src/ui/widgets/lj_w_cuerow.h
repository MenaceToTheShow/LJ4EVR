/* lj_w_cuerow.h - one row of the Cue control grid (note 07 §6, design 7.7): ~16 px,
 * cells No/Sequence/Type/Scene/ST/FT/Status/Loop/Trig at the forms.ini C0..C8 widths,
 * text in the status colour (black/maroon/red/grey/blue/green), navy #000080 with
 * white text when selected. Paint only: the grid owns selection and the slot buttons. */
#ifndef LJ_UI_WIDGETS_W_CUEROW_H
#define LJ_UI_WIDGETS_W_CUEROW_H

#include <stdint.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_CUEROW_H 16

typedef struct {
    lj_widget base;
    const char *const *cells; int ncells;   /* caller-owned strings, one per column */
    const int *col_w;                       /* caller-owned widths, ncells entries */
    uint32_t text_rgb;                      /* status colour, default black */
    unsigned selected:1;
} lj_w_cuerow;

lj_widget *lj_w_cuerow_create(lj_widget *parent, const lj_ctl_desc *d);
void       lj_w_cuerow_set(lj_widget *w, const char *const *cells, const int *col_w, int n, uint32_t text_rgb, int selected);

#endif /* LJ_UI_WIDGETS_W_CUEROW_H */
