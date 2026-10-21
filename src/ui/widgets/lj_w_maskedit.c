/* lj_w_maskedit.c - TMaskEdit: a fixed-length overwrite TEdit driven by EditMask
 * ("!999;1; " in every LJ2 use): mask chars 0/9/# digit, L/l letter, A/a alnum, C/c
 * any, \x literal, ! > < directives; the third field is the blank character. */
#include "ui/widgets/lj_w_maskedit.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>

static void setup(lj_w_edit *e, const char *mask, const char *text)
{
    const char *p, *semi = strchr(mask, ';');
    int n = 0;
    e->blank = '_';
    if (semi && (semi = strchr(semi + 1, ';')) != NULL && semi[1]) e->blank = semi[1];
    for (p = mask; *p && *p != ';' && n < 255; p++) {
        char cls;
        switch (*p) {
        case '!': case '>': case '<': continue;
        case '\\': if (!p[1]) continue; p++; cls = 0; break;
        case '0': case '9': case '#': cls = 'd'; break;
        case 'L': case 'l': cls = 'a'; break;
        case 'A': case 'a': cls = 'n'; break;
        case 'C': case 'c': cls = 'c'; break;
        default: cls = 0; break;
        }
        e->mclass[n] = cls;
        e->text[n] = cls ? e->blank : *p;
        n++;
    }
    e->text[n] = 0;
    e->len = e->max_length = n;
    if (text) lj_w_edit_set_text(&e->w, text);
}

lj_widget *lj_w_maskedit_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_edit *e = (lj_w_edit *)lj_w_edit_create(parent, d);
    e->w.kind = LJ_K_MASKEDIT;
    if (d && d->edit_mask && d->edit_mask[0]) {
        e->variant = LJ_W_EDIT_MASK;
        setup(e, d->edit_mask, d->text);
    }
    return &e->w;
}
