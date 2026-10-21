/* lj_w_checkbox.c - TCheckBox (7.3): 13x13 sunken square vertically centred at the left
 * (right for Alignment = taLeftJustify), 7x7 check glyph, caption at +17, dotted focus
 * rect around the caption; click or Space toggles (7.9). */
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *c = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int x, y, bx, tx, by, ty, tw = lj_text_width(f, c->caption);
    int box_right = w->desc && w->desc->alignment == LJ_TA_LEFT;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, w->color);
    bx = box_right ? x + w->w - 13 : x;
    by = y + (w->h - 13) / 2;
    tx = box_right ? x : x + 17;
    ty = y + (w->h - f->height) / 2;
    if (w->enabled) {
        lj_checkbox(s, bx, by, c->checked);
    } else {
        lj_edge_sunken(s, bx, by, 13, 13, LJ_CLR_BTNFACE);
        if (c->checked) lj_check_glyph(s, bx + 3, by + 3, LJ_CLR_BTNSHADOW);
    }
    lj_w_caption_text(s, w, tx, ty, c->caption);
    if (w->focused) lj_focus_rect(s, tx - 1, ty, tw + 2, f->height);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    return w->enabled ? lj_w_button_mouse((lj_w_cap *)w, m, 1) : 0;
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_SPACE) { lj_w_click(w); return 1; }
    return 0;
}

void lj_w_checkbox_set_checked(lj_widget *w, int on)
{
    lj_w_cap *c = (lj_w_cap *)w;
    if (!w || w->kind != LJ_K_CHECKBOX || (unsigned)(on != 0) == c->checked) return;
    c->checked = on != 0;
    lj_widget_invalidate(w);
    lj_w_emit(w, LJ_WEV_CLICK);
}

void lj_w_checkbox_click(lj_widget *w)
{
    lj_w_checkbox_set_checked(w, !((lj_w_cap *)w)->checked);
}

int lj_w_checkbox_checked(const lj_widget *w)
{
    return w && w->kind == LJ_K_CHECKBOX ? (int)((const lj_w_cap *)w)->checked : 0;
}

static const lj_widget_vt vt = { paint, mouse, key, NULL };

lj_widget *lj_w_checkbox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cap *c = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 1);
    if (d) c->checked = lj_desc_bool(d->checked, 0) || (d->state && strcmp(d->state, "cbChecked") == 0);
    return &c->w;
}
