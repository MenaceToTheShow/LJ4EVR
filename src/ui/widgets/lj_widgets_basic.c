/* lj_widgets_basic.c - registration table for the basic group and the helpers
 * its classes share: captioned allocation, text colour, focus with OnEnter/OnExit,
 * the two mouse protocols, TControl.Click and the Alt+letter dialog char (7.9). */
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "core/lj_str.h"
#include "gfx/lj_draw.h"

int lj_widgets_register_basic(lj_widget_ctor *table, int count)
{
    if (!table || count < LJ_K_COUNT) return -1;
    table[LJ_K_LABEL]       = lj_w_label_create;
    table[LJ_K_PANEL]       = lj_w_panel_create;
    table[LJ_K_EDIT]        = lj_w_edit_create;
    table[LJ_K_MASKEDIT]    = lj_w_maskedit_create;
    table[LJ_K_NUMEDIT]     = lj_w_numedit_create;
    table[LJ_K_BUTTON]      = lj_w_button_create;
    table[LJ_K_BITBTN]      = lj_w_bitbtn_create;
    table[LJ_K_SPEEDBUTTON] = lj_w_speedbutton_create;
    table[LJ_K_CHECKBOX]    = lj_w_checkbox_create;
    table[LJ_K_RADIOBUTTON] = lj_w_radiobutton_create;
    table[LJ_K_RADIOGROUP]  = lj_w_radiogroup_create;
    table[LJ_K_GROUPBOX]    = lj_w_groupbox_create;
    table[LJ_K_BEVEL]       = lj_w_bevel_create;
    table[LJ_K_SHAPE]       = lj_w_shape_create;
    table[LJ_K_STATICTEXT]  = lj_w_statictext_create;
    table[LJ_K_IMAGE]       = lj_w_image_create;
    table[LJ_K_PAINTBOX]    = lj_w_paintbox_create;
    table[LJ_K_SPLITTER]    = lj_w_splitter_create;
    table[LJ_K_ODPANEL2]    = lj_w_odpanel2_create;
    table[LJ_K_ODPANEL]     = lj_w_odpanel_create;
    return 0;
}

static int captioned(int kind)
{
    switch (kind) {
    case LJ_K_LABEL: case LJ_K_PANEL: case LJ_K_BUTTON: case LJ_K_BITBTN: case LJ_K_SPEEDBUTTON:
    case LJ_K_CHECKBOX: case LJ_K_RADIOBUTTON: case LJ_K_RADIOGROUP: case LJ_K_GROUPBOX:
    case LJ_K_STATICTEXT: case LJ_K_ODPANEL: return 1;
    default: return 0;
    }
}

void *lj_w_cap_new(lj_widget *parent, const lj_ctl_desc *d, const lj_widget_vt *vt, size_t size, int tabstop)
{
    lj_w_cap *c = lj_w_alloc(size, parent, d, vt, tabstop);
    if (d) {
        /* ParentColor: the DFM writes it only when it differs from the class default
         * (TLabel YES, every other class NO, measured over the 208 forms); a given Color wins. */
        int dflt = d->kind == LJ_K_LABEL;
        if (!(d->color & LJ_COLOR_SET) && parent && lj_desc_bool(d->parent_color, dflt)) c->w.color = parent->color;
        if (d->caption) lj_str_copy(c->caption, sizeof c->caption, d->caption);
    } else if (parent) {
        c->w.color = parent->color;
    }
    return c;
}

uint32_t lj_w_fg(const lj_widget *w)
{
    return w->desc ? lj_desc_rgb(w->desc->font_color, LJ_CLR_3DDKSHADOW) : LJ_CLR_3DDKSHADOW;
}

void lj_w_caption_text(lj_surface *s, const lj_widget *w, int x, int y, const char *str)
{
    if (!str || !*str) return;
    if (w->enabled) lj_text(s, lj_w_font_of(w), x, y, str, lj_w_fg(w), 1);
    else lj_text_disabled(s, lj_w_font_of(w), x, y, str, 1);
}

void lj_w_focus(lj_widget *w)
{
    lj_widget *old;
    if (!w || w->focused || !w->visible || !w->enabled) return;
    old = lj_widget_set_focus(w);
    if (old) lj_w_emit(old, LJ_WEV_EXIT);
    lj_w_emit(w, LJ_WEV_ENTER);
}

static int inside(const lj_widget *w, const lj_mouse_ev *m)
{
    return m->x >= 0 && m->y >= 0 && m->x < w->w && m->y < w->h;
}

int lj_w_plain_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:  lj_w_emit(w, m->clicks == 2 ? LJ_WEV_DBLCLICK : LJ_WEV_MOUSE_DOWN); break;
    case LJ_EV_MOUSE_UP:    lj_w_emit(w, LJ_WEV_MOUSE_UP); if (m->button == 1 && inside(w, m)) lj_w_emit(w, LJ_WEV_CLICK); break;
    case LJ_EV_MOUSE_MOVE:  lj_w_emit(w, LJ_WEV_MOUSE_MOVE); break;
    case LJ_EV_MOUSE_WHEEL: lj_w_emit(w, LJ_WEV_MOUSE_WHEEL); break;
    default: break;
    }
    return w->on_event != NULL;
}

int lj_w_button_mouse(lj_w_cap *b, const lj_mouse_ev *m, int takes_focus)
{
    lj_widget *w = &b->w;
    int in = inside(w, m);
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        lj_w_emit(w, m->clicks == 2 ? LJ_WEV_DBLCLICK : LJ_WEV_MOUSE_DOWN);
        if (m->button != 1) return 1;
        if (takes_focus) lj_w_focus(w);
        b->pressed = 1; b->hover = 1; lj_widget_invalidate(w);
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (b->hover != (unsigned)(in != 0)) { b->hover = in != 0; lj_widget_invalidate(w); }
        lj_w_emit(w, LJ_WEV_MOUSE_MOVE);
        return 1;
    case LJ_EV_MOUSE_LEAVE:
        if (b->hover) { b->hover = 0; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_UP:
        lj_w_emit(w, LJ_WEV_MOUSE_UP);
        if (m->button != 1 || !b->pressed) return 1;
        b->pressed = 0; b->hover = in != 0; lj_widget_invalidate(w);
        if (in) lj_w_click(w);
        return 1;
    case LJ_EV_MOUSE_WHEEL:
        lj_w_emit(w, LJ_WEV_MOUSE_WHEEL);
        return w->on_event != NULL;
    default:
        return 0;
    }
}

void lj_w_click(lj_widget *w)
{
    if (!w || !w->enabled) return;
    switch (w->kind) {
    case LJ_K_CHECKBOX:    lj_w_checkbox_click(w); return;
    case LJ_K_RADIOBUTTON: lj_w_radiobutton_click(w); return;
    case LJ_K_SPEEDBUTTON: lj_w_speedbutton_click(w); return;
    default:               lj_w_emit(w, LJ_WEV_CLICK); return;
    }
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int has_accel(const char *cap, int ch)
{
    for (; cap && cap[0] && cap[1]; cap++) {
        if (cap[0] != '&') continue;
        if (cap[1] == '&') { cap++; continue; }
        return lower((unsigned char)cap[1]) == lower(ch);
    }
    return 0;
}

static lj_widget *first_tabstop(lj_widget *w)
{
    lj_widget *c, *hit;
    for (c = w->first; c; c = c->next) {
        if (c->visible && c->enabled && c->tabstop) return c;
        if ((hit = first_tabstop(c)) != NULL) return hit;
    }
    return NULL;
}

static int dialog_char(lj_widget *root, lj_widget *w, int ch)
{
    lj_widget *c, *t;
    if (!w->visible) return 0;
    if (w->enabled && has_accel(lj_w_caption(w), ch)) {
        switch (w->kind) {
        case LJ_K_BUTTON: case LJ_K_BITBTN: case LJ_K_CHECKBOX: case LJ_K_RADIOBUTTON: case LJ_K_SPEEDBUTTON:
            if (w->tabstop) lj_w_focus(w);
            lj_w_click(w); return 1;
        case LJ_K_LABEL:
            if (w->desc && w->desc->focus_control && (t = lj_widget_find(root, w->desc->focus_control)) != NULL) { lj_w_focus(t); return 1; }
            break;
        case LJ_K_GROUPBOX: case LJ_K_RADIOGROUP:
            if ((t = first_tabstop(w)) != NULL) { lj_w_focus(t); return 1; }
            break;
        default: break;
        }
    }
    for (c = w->first; c; c = c->next)
        if (dialog_char(root, c, ch)) return 1;
    return 0;
}

int lj_w_dialog_char(lj_widget *root, int ch)
{
    return root && ch ? dialog_char(root, root, ch) : 0;
}

const char *lj_w_caption(const lj_widget *w)
{
    return w && captioned(w->kind) ? ((const lj_w_cap *)w)->caption : NULL;
}

int lj_w_set_caption(lj_widget *w, const char *s)
{
    if (!w || !captioned(w->kind)) return -1;
    lj_str_copy(((lj_w_cap *)w)->caption, LJ_W_TEXT_CAP, s ? s : "");
    lj_widget_invalidate(w);
    return 0;
}
