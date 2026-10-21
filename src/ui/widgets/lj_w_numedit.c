/* lj_w_numedit.c - TNumEdit (Martin's): a TEdit that accepts digits only (a leading '-'
 * when Min < 0), keeps Value clamped to Min..Max and fires OnValueChange. */
#include "ui/widgets/lj_w_numedit.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <stdio.h>

int lj_w_numedit_value(const lj_widget *w)
{
    return w && w->kind == LJ_K_NUMEDIT ? ((const lj_w_edit *)w)->value : 0;
}

int lj_w_numedit_set_value(lj_widget *w, int v)
{
    lj_w_edit *e = (lj_w_edit *)w;
    char buf[16];
    if (!w || w->kind != LJ_K_NUMEDIT) return -1;
    if (v < e->min) v = e->min;
    if (v > e->max) v = e->max;
    snprintf(buf, sizeof buf, "%d", v);
    return lj_w_edit_set_text(w, buf);
}

lj_widget *lj_w_numedit_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_edit *e = (lj_w_edit *)lj_w_edit_create(parent, d);
    e->variant = LJ_W_EDIT_NUM;
    e->w.kind = LJ_K_NUMEDIT;
    if (d) {
        e->min = d->min; e->max = d->max > d->min ? d->max : d->min;
        e->value = d->value < e->min ? e->min : d->value > e->max ? e->max : d->value;
    }
    return &e->w;
}
