/* lj_w_tabsheet.c - TTabSheet: a page of a TPageControl. A plain clBtnFace
 * container; its caption is the tab's label; the page control places it and
 * sets visible (lj_w_tabs_layout). Children are instantiated by the window. */
#include "ui/widgets/lj_widgets_lists.h"

static void ts_paint(lj_widget *w, lj_surface *s)
{
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
}

static const lj_widget_vt ts_vt = { ts_paint, NULL, NULL, NULL, NULL };

lj_widget *lj_w_tabsheet_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_tabsheet *t = lj_w_alloc(sizeof *t, parent, d, &ts_vt, 0);
    t->caption = d ? d->caption : NULL;
    if (parent && (parent->kind == LJ_K_PAGECONTROL) && lj_w_tabs_count(parent) > 1) t->base.visible = 0;   /* only the active page shows; layout decides */
    return &t->base;
}
