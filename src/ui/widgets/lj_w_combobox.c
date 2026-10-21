/* lj_w_combobox.c - TComboBox: 21-px sunken clWindow box, 16-px raised drop button
 * with the down arrow, csDropDown shows Text, csDropDownList shows the selected
 * item (navy when focused). The drop list is an lj_w_list hosted through the
 * popup hook (design 7.9) or, without one, added as the root's last child. */
#include "ui/widgets/lj_widgets_lists.h"

#include <stdlib.h>
#include <string.h>

#define CB_BTN 16

static int btn_x(const lj_widget *w) { return w->w - 2 - CB_BTN; }

static void cb_paint(lj_widget *w, lj_surface *s)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    int ax, ay, bx, bh = w->h - 4;
    const lj_font *f = lj_w_font_of(w);
    const char *txt = c->index >= 0 && c->index < c->n ? c->items[c->index] : (c->droplist_style ? NULL : c->text);
    int sel = c->droplist_style && w->focused && !c->drop;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->enabled ? w->color : LJ_CLR_BTNFACE);
    bx = ax + btn_x(w);
    if (c->drop) { lj_fill(s, bx, ay + 2, CB_BTN, bh, LJ_CLR_BTNFACE); lj_bevel1(s, bx, ay + 2, CB_BTN, bh, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT); }
    else lj_button_raised(s, bx, ay + 2, CB_BTN, bh);
    lj_w_arrow(s, bx + CB_BTN / 2, ay + 2 + bh / 2 - 1 + (c->drop ? 1 : 0), 1, w->enabled ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW);
    if (sel) lj_fill(s, ax + 3, ay + 3, btn_x(w) - 4, w->h - 6, LJ_CLR_HIGHLIGHT);
    if (txt) lj_w_text(s, w, ax + 4, ay + (w->h - f->height) / 2, txt, sel ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
    if (sel) lj_focus_rect(s, ax + 3, ay + 3, btn_x(w) - 4, w->h - 6);
}

static void select_index(lj_w_combobox *c, int i)
{
    if (i < 0 || i >= c->n || i == c->index) return;
    c->index = i;
    lj_widget_invalidate(&c->base);
    lj_w_emit(&c->base, LJ_WEV_CLICK);
    lj_w_emit(&c->base, LJ_WEV_CHANGE);
}

/* the drop list's vt: list paint, pick on mouse-up, Return/Escape */
static int drop_mouse(lj_widget *list, const lj_mouse_ev *m)
{
    lj_w_list *l = (lj_w_list *)list;
    int row = lj_w_list_row_at(list, m->y);
    int on_sb = l->n * l->item_h > list->h - 4 && m->x >= l->sb.x;
    if (on_sb || l->sb.drag) row = -1;
    if (m->type == LJ_EV_MOUSE_MOVE && row >= 0 && row != l->index) { l->index = row; lj_widget_invalidate(list); return 1; }
    if (m->type == LJ_EV_MOUSE_UP && m->button == 1 && row >= 0) { l->index = row; lj_w_combobox_close(l->owner, 1); return 1; }
    return lj_w_list_mouse(list, m);
}
static int drop_key(lj_widget *list, const lj_key_ev *k)
{
    lj_w_list *l = (lj_w_list *)list;
    if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_RETURN) { lj_w_combobox_close(l->owner, 1); return 1; }
    if (k->type == LJ_EV_KEY_DOWN && (k->key == LJ_VK_ESCAPE || k->key == LJ_VK_F4)) { lj_w_combobox_close(l->owner, 0); return 1; }
    return lj_w_list_key(list, k);
}
static const lj_widget_vt drop_vt = { lj_w_list_paint, drop_mouse, drop_key, NULL };

int lj_w_combobox_drop(lj_widget *w)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    lj_w_list *l;
    lj_widget *root;
    int ax, ay, rows;
    if (c->drop || c->n <= 0 || !w->enabled) return -1;
    rows = c->n < c->drop_count ? c->n : c->drop_count;
    l = lj_w_alloc(sizeof *l, NULL, NULL, &drop_vt, 0);
    l->items = c->items; l->n = c->n; l->index = c->index; l->item_h = c->item_h; l->owner = w;
    l->base.color = LJ_CLR_BTNHIGHLIGHT;
    l->base.w = w->w; l->base.h = rows * c->item_h + 4;
    lj_widget_abs(w, &ax, &ay);
    for (root = w; root->parent; root = root->parent) { }
    ay += w->h;
    if (ay + l->base.h > root->y + root->h && ay - w->h - l->base.h >= root->y) ay -= w->h + l->base.h;   /* no room below: open upwards */
    l->sb.pos = c->index >= 0 ? c->index : 0;
    l->sb.x = w->w - 2 - LJ_SB_W;
    c->drop = &l->base;
    c->hooked = c->hook.open && c->hook.open(w, &l->base, ax, ay, c->hook.ud) == 0;
    if (!c->hooked) {
        l->base.x = ax - root->x; l->base.y = ay - root->y;
        lj_widget_add(root, &l->base);
        lj_widget_invalidate(root);
    }
    lj_widget_set_focus(w);
    lj_widget_invalidate(w);
    return 0;
}

void lj_w_combobox_close(lj_widget *w, int pick)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    lj_widget *list = c->drop, *root;
    int picked;
    if (!list) return;
    picked = ((lj_w_list *)list)->index;
    c->drop = NULL;
    if (c->hooked) { if (c->hook.close) c->hook.close(w, list, c->hook.ud); }
    else {
        for (root = list; root->parent; root = root->parent) { }
        lj_widget_remove(list);
        lj_widget_invalidate(root);
    }
    free(list);
    lj_widget_invalidate(w);
    if (pick) select_index(c, picked);
}

static int cb_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    if (!w->enabled) return 0;
    if (m->type == LJ_EV_MOUSE_WHEEL) { select_index(c, c->index - m->wheel < 0 ? 0 : (c->index - m->wheel >= c->n ? c->n - 1 : c->index - m->wheel)); return 1; }
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return 0;
    lj_widget_set_focus(w);
    if (c->drop) lj_w_combobox_close(w, 0);
    else if (c->droplist_style || m->x >= btn_x(w)) lj_w_combobox_drop(w);
    return 1;
}

static int cb_key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    if (k->type != LJ_EV_KEY_DOWN || !w->enabled) return 0;
    if (c->drop) return c->drop->vt->key(c->drop, k);
    if (k->key == LJ_VK_F4 || (k->key == LJ_VK_DOWN && k->alt)) return lj_w_combobox_drop(w) == 0;
    switch (k->key) {
    case LJ_VK_UP:   select_index(c, c->index > 0 ? c->index - 1 : 0); return 1;
    case LJ_VK_DOWN: select_index(c, c->index + 1); return 1;
    case LJ_VK_HOME: select_index(c, 0); return 1;
    case LJ_VK_END:  select_index(c, c->n - 1); return 1;
    default: return 0;
    }
}

static const lj_widget_vt cb_vt = { cb_paint, cb_mouse, cb_key, NULL };

lj_widget *lj_w_combobox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_combobox *c = lj_w_alloc(sizeof *c, parent, d, &cb_vt, 1);
    c->index = -1; c->item_h = 13; c->drop_count = 8;
    c->base.color = LJ_CLR_BTNHIGHLIGHT;
    if (d) {
        c->base.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        c->items = d->items; c->n = d->items ? d->nitems : 0;
        c->text = d->text;
        if (d->item_height > 0) c->item_h = d->item_height;
        if (d->drop_down_count > 0) c->drop_count = d->drop_down_count;
        if (d->item_index >= 0 && d->item_index < c->n) c->index = d->item_index;
        c->droplist_style = d->style && strcmp(d->style, "csDropDownList") == 0;
    }
    return &c->base;
}

void lj_w_combobox_set_popup_hook(lj_widget *w, const lj_w_popup_hook *hook)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    if (hook) c->hook = *hook;
    else memset(&c->hook, 0, sizeof c->hook);
}

void lj_w_combobox_set_items(lj_widget *w, const char *const *items, int n)
{
    lj_w_combobox *c = (lj_w_combobox *)w;
    if (c->drop) lj_w_combobox_close(w, 0);
    c->items = items; c->n = items ? n : 0; c->index = -1;
    lj_widget_invalidate(w);
}
