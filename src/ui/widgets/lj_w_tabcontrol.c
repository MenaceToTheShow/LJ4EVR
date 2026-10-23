/* lj_w_tabcontrol.c - TTabControl: the tab strip and page frame of
 * lj_w_pagecontrol.c with the tabs from Tabs.Strings and no pages; the DFM
 * children keep their own coordinates, except an alClient child, which a re-layout
 * returns to the display rect (lj_w_tabs_layout). TabIndex selects, OnChange fires. */
#include "ui/widgets/lj_widgets_lists.h"

static const lj_widget_vt tc_vt = { lj_w_tabs_paint, lj_w_tabs_mouse, lj_w_tabs_key, lj_w_tabs_layout, NULL };   /* layout: an alClient child under the strip */

lj_widget *lj_w_tabcontrol_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_tabs *t = lj_w_alloc(sizeof *t, parent, d, &tc_vt, 1);
    t->tab_h = lj_w_font_of(&t->base)->height + 8;
    if (d) {
        t->tabs = d->items; t->ntabs = d->items ? d->nitems : 0;
        if (d->tab_index > 0 && d->tab_index < t->ntabs) t->active = d->tab_index;
    }
    return &t->base;
}
