/* lj_w_dragbar.h - the horizontal value control of the fixture controls (note 07 §3.2,
 * scrollbar.gif 170x23 measured): raised panel, 25-px arrow zones at both ends,
 * sunken bar with a fill from the left proportional to value and the centred
 * caption "Intensity 45,9%" (host-locale decimal, D6). Left-click arrows step by 1,
 * right-click = min / max, press-and-drag in the bar sets the value. */
#ifndef LJ_UI_WIDGETS_W_DRAGBAR_H
#define LJ_UI_WIDGETS_W_DRAGBAR_H

#include <stdint.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_DRAGBAR_H     23
#define LJ_DRAGBAR_ARROW 25      /* arrow zone width at each end */

typedef struct {
    lj_widget base;
    const char *caption;          /* "Intensity", NULL = percentage only; points into the caller's table */
    int value, max;               /* 0..max, max default 255 */
    uint32_t fill;                /* fill colour: white for plain bars, cyan/magenta/yellow for CMY (colorcontrol.gif) */
    unsigned pressed:2, dragging:1;
} lj_w_dragbar;

lj_widget *lj_w_dragbar_create(lj_widget *parent, const lj_ctl_desc *d);   /* caption, position (value), max from the desc */
int        lj_w_dragbar_value(const lj_widget *w);
int        lj_w_dragbar_set_value(lj_widget *w, int v);                    /* clamps; no event */
void       lj_w_dragbar_set_fill(lj_widget *w, uint32_t rgb);
int        lj_w_dragbar_text(const lj_widget *w, char *dst, size_t cap);   /* the caption as painted, "Intensity 45,9%" */

#endif /* LJ_UI_WIDGETS_W_DRAGBAR_H */
