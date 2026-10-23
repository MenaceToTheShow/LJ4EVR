/* lj_w_radiobutton.c - TRadioButton (7.3, 7.9): 12x12 classic ring vertically centred
 * at the left, caption at +17, focus rect around the caption. Checking one unchecks
 * the other radio buttons of the same parent (the VCL's group rule); only the checked
 * one is a tab stop; arrows move the check to the previous/next sibling. */
#include "ui/widgets/lj_w_radiobutton.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int x, y, ty, tw = lj_text_width(f, c->caption);
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    lj_radio(s, x, y + (w->h - 12) / 2, c->checked);
    ty = y + (w->h - f->height) / 2;
    lj_w_caption_text(s, w, x + 17, ty, c->caption);
    if (w->focused) lj_focus_rect(s, x + 16, ty, tw + 2, f->height);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    return w->enabled ? lj_w_button_mouse((lj_w_cap *)w, m, 1) : 0;
}

static lj_widget *sibling_radio(lj_widget *w, int forward)   /* previous/next usable radio of the same parent, wrapping */
{
    lj_widget *c, *prev = NULL, *first = NULL, *last = NULL;
    if (!w->parent) return NULL;
    for (c = w->parent->first; c; c = c->next) {
        if (c->kind != LJ_K_RADIOBUTTON || !c->visible || !c->enabled) continue;
        if (!first) first = c;
        if (forward && prev == w) return c;
        if (!forward && c == w && prev) return prev;
        prev = c; last = c;
    }
    return forward ? first : last;
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_widget *t;
    if (k->type != LJ_EV_KEY_DOWN) return 0;
    if (k->key == LJ_VK_SPACE) { lj_w_click(w); return 1; }
    if (k->key == LJ_VK_UP || k->key == LJ_VK_LEFT || k->key == LJ_VK_DOWN || k->key == LJ_VK_RIGHT) {
        t = sibling_radio(w, k->key == LJ_VK_DOWN || k->key == LJ_VK_RIGHT);
        if (t && t != w) { lj_w_focus(t); lj_w_click(t); }
        return 1;
    }
    return 0;
}

void lj_w_radiobutton_click(lj_widget *w)
{
    lj_w_cap *c = (lj_w_cap *)w;
    lj_widget *s;
    if (!c->checked) {
        if (w->parent)
            for (s = w->parent->first; s; s = s->next)
                if (s != w && s->kind == LJ_K_RADIOBUTTON && ((lj_w_cap *)s)->checked) {
                    ((lj_w_cap *)s)->checked = 0; s->tabstop = 0; lj_widget_invalidate(s);
                }
        c->checked = 1; w->tabstop = 1;
        lj_widget_invalidate(w);
        if (w->parent && w->parent->kind == LJ_K_RADIOGROUP) lj_w_radiogroup_child_checked(w->parent, w);
    }
    lj_w_emit(w, LJ_WEV_CLICK);
}

int lj_w_radiobutton_checked(const lj_widget *w)
{
    return w && w->kind == LJ_K_RADIOBUTTON ? (int)((const lj_w_cap *)w)->checked : 0;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL, NULL };

lj_widget *lj_w_radiobutton_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cap *c = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 0);
    c->w.kind = LJ_K_RADIOBUTTON;                             /* code-built ones (TRadioGroup items) have no desc */
    if (d) { c->checked = lj_desc_bool(d->checked, 0) != 0; c->w.tabstop = c->checked; }
    return &c->w;
}
