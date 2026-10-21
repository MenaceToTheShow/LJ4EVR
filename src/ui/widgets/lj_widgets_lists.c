/* lj_widgets_lists.c - registration table for the lists group plus the helpers
 * every class in it shares: allocation from an lj_ctl_desc, font resolution,
 * disabled text, arrows, the dithered track and the embedded classic scrollbar
 * (design 7.3; arrow/thumb geometry per the classic SM_CXVSCROLL = 16). */
#include "ui/widgets/lj_widgets_lists.h"

#include <stdlib.h>
#include <string.h>

const char *const lj_wev_lists_names[LJ_WEV_LISTS_COUNT - LJ_WEV_COUNT] = { "OnScroll", "OnSectionClick" };

int lj_widgets_register_lists(lj_widget_ctor *table, int count)
{
    if (!table || count < LJ_K_COUNT) return -1;
    table[LJ_K_LISTVIEW]      = lj_w_listview_create;
    table[LJ_K_LISTBOX]       = lj_w_listbox_create;
    table[LJ_K_CHECKLISTBOX]  = lj_w_checklistbox_create;
    table[LJ_K_COMBOBOX]      = lj_w_combobox_create;
    table[LJ_K_TREEVIEW]      = lj_w_treeview_create;
    table[LJ_K_STRINGGRID]    = lj_w_stringgrid_create;
    table[LJ_K_DRAWGRID]      = lj_w_drawgrid_create;
    table[LJ_K_PAGECONTROL]   = lj_w_pagecontrol_create;
    table[LJ_K_TABSHEET]      = lj_w_tabsheet_create;
    table[LJ_K_TABCONTROL]    = lj_w_tabcontrol_create;
    table[LJ_K_HEADERCONTROL] = lj_w_headercontrol_create;
    table[LJ_K_MEMO]          = lj_w_memo_create;
    table[LJ_K_RICHEDIT]      = lj_w_richedit_create;
    table[LJ_K_STATUSBAR]     = lj_w_statusbar_create;
    table[LJ_K_PROGRESSBAR]   = lj_w_progressbar_create;
    table[LJ_K_SCROLLBAR]     = lj_w_scrollbar_create;
    table[LJ_K_TRACKBAR]      = lj_w_trackbar_create;
    return 0;
}

static const lj_font *face_of(lj_font_id id, unsigned style)
{
    int bold = (style & LJ_FS_BOLD) != 0;
    switch (id) {
    case LJ_FONT_SANS13B:      return &lj_font_sans13b;
    case LJ_FONT_ARIAL14:      return bold ? &lj_font_arial14b : &lj_font_arial14;
    case LJ_FONT_ARIAL14B:     return &lj_font_arial14b;
    case LJ_FONT_ARIALBLACK18: return &lj_font_arialblack18;
    case LJ_FONT_SANS13:
    case LJ_FONT_OTHER:
    default:                   return bold ? &lj_font_sans13b : &lj_font_sans13;
    }
}

const lj_font *lj_w_font_of(const lj_widget *w)
{
    for (; w; w = w->parent)
        if (w->font) return w->font;
    return &lj_font_sans13;
}

void *lj_w_alloc(size_t size, lj_widget *parent, const lj_ctl_desc *d, const lj_widget_vt *vt, int tabstop_dflt)
{
    lj_widget *w = calloc(1, size);
    if (!w) lj_fatal("lj_w_alloc: %u bytes", (unsigned)size);
    w->vt = vt;
    w->desc = d;
    w->visible = 1; w->enabled = 1; w->tabstop = tabstop_dflt != 0;
    w->color = LJ_CLR_BTNFACE;
    if (d) {
        w->kind = d->kind; w->name = d->name; w->hint = d->hint;
        w->x = d->left; w->y = d->top; w->w = d->width; w->h = d->height;
        w->visible = lj_desc_bool(d->visible, 1) != 0;
        w->enabled = lj_desc_bool(d->enabled, 1) != 0;
        w->tabstop = lj_desc_bool(d->tabstop, tabstop_dflt) != 0;
        w->transparent = lj_desc_bool(d->transparent, 0) != 0;
        w->tab_order = d->tab_order;
        w->align = d->align; w->anchors = d->anchors;
        w->color = lj_desc_rgb(d->color, LJ_CLR_BTNFACE);
        if (d->font != LJ_FONT_INHERIT) w->font = face_of(d->font, d->font_style);
        else if (d->font_style & LJ_FS_BOLD) w->font = &lj_font_sans13b;
    }
    if (parent) lj_widget_add(parent, w);
    return w;
}

void lj_w_text(lj_surface *s, const lj_widget *w, int x, int y, const char *str, uint32_t rgb)
{
    const lj_font *f = lj_w_font_of(w);
    if (!str || !*str) return;
    if (w->enabled) { lj_text(s, f, x, y, str, rgb, 0); return; }
    lj_text(s, f, x + 1, y + 1, str, LJ_CLR_BTNHIGHLIGHT, 0);
    lj_text(s, f, x, y, str, LJ_CLR_BTNSHADOW, 0);
}

void lj_w_emit(lj_widget *w, int ev)
{
    if (w && w->on_event) w->on_event(w, ev, w->ud);
}

void lj_w_arrow(lj_surface *s, int cx, int cy, int dir, uint32_t rgb)
{
    int i;
    for (i = 0; i < 4; i++) {
        int len = 7 - 2 * i;
        switch (dir) {
        case 0:  lj_hline(s, cx - 3 + i, cy - i, len, rgb); break;    /* base at cy, apex at cy-3 */
        case 1:  lj_hline(s, cx - 3 + i, cy + i, len, rgb); break;
        case 2:  lj_vline(s, cx - i, cy - 3 + i, len, rgb); break;
        default: lj_vline(s, cx + i, cy - 3 + i, len, rgb); break;
        }
    }
}

void lj_w_checker(lj_surface *s, int x, int y, int w, int h)
{
    int px, py;
    for (py = 0; py < h; py++)
        for (px = 0; px < w; px++)
            lj_fill(s, x + px, y + py, 1, 1, ((px + py) & 1) ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_BTNFACE);
}

/* ---- embedded scrollbar ---- */
static void sb_geom(const lj_w_sbar *b, int *btn, int *track, int *th_len, int *th_pos)
{
    int len = b->vertical ? b->h : b->w, range = b->max - b->min, page = b->page > 0 ? b->page : 1;
    *btn = len / 2 < LJ_SB_W ? len / 2 : LJ_SB_W;
    *track = len - 2 * *btn;
    *th_len = 0; *th_pos = 0;
    if (range <= 0 || *track <= 0) return;
    *th_len = *track * page / (range + page);
    if (*th_len < 8) *th_len = 8;
    if (*th_len > *track) *th_len = *track;
    *th_pos = (b->pos - b->min) * (*track - *th_len) / range;
}

void lj_w_sbar_paint(lj_surface *s, const lj_w_sbar *b, int ax, int ay, int enabled)
{
    int btn, track, tl, tp, x = ax + b->x, y = ay + b->y;
    uint32_t ink = enabled && b->max > b->min ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
    sb_geom(b, &btn, &track, &tl, &tp);
    if (b->vertical) {
        lj_button_raised(s, x, y, b->w, btn);
        lj_w_arrow(s, x + b->w / 2, y + btn / 2 + 1, 0, ink);
        lj_button_raised(s, x, y + b->h - btn, b->w, btn);
        lj_w_arrow(s, x + b->w / 2, y + b->h - btn + btn / 2 - 2, 1, ink);
        lj_w_checker(s, x, y + btn, b->w, track);
        if (tl) lj_button_raised(s, x, y + btn + tp, b->w, tl);
    } else {
        lj_button_raised(s, x, y, btn, b->h);
        lj_w_arrow(s, x + btn / 2 + 1, y + b->h / 2, 2, ink);
        lj_button_raised(s, x + b->w - btn, y, btn, b->h);
        lj_w_arrow(s, x + b->w - btn + btn / 2 - 2, y + b->h / 2, 3, ink);
        lj_w_checker(s, x + btn, y, track, b->h);
        if (tl) lj_button_raised(s, x + btn + tp, y, tl, b->h);
    }
}

int lj_w_sbar_set(lj_w_sbar *b, int pos)
{
    if (pos > b->max) pos = b->max;
    if (pos < b->min) pos = b->min;
    if (pos == b->pos) return 0;
    b->pos = pos;
    return 1;
}

int lj_w_sbar_mouse(lj_w_sbar *b, const lj_mouse_ev *m)
{
    int btn, track, tl, tp, len = b->vertical ? b->h : b->w;
    int p = b->vertical ? m->y - b->y : m->x - b->x;
    int c = b->vertical ? m->x - b->x : m->y - b->y;
    int cross = b->vertical ? b->w : b->h, page = b->page > 0 ? b->page : 1;
    if (m->type == LJ_EV_MOUSE_UP) { int was = b->drag; b->drag = 0; return was ? 0 : -1; }
    sb_geom(b, &btn, &track, &tl, &tp);
    if (m->type == LJ_EV_MOUSE_MOVE) {
        if (!b->drag || track <= tl) return b->drag ? 0 : -1;
        return lj_w_sbar_set(b, b->min + (p - b->drag_off - btn) * (b->max - b->min) / (track - tl));
    }
    if (c < 0 || c >= cross || p < 0 || p >= len) return -1;
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return 0;
    if (p < btn) return lj_w_sbar_set(b, b->pos - 1);
    if (p >= len - btn) return lj_w_sbar_set(b, b->pos + 1);
    if (!tl) return 0;
    if (p < btn + tp) return lj_w_sbar_set(b, b->pos - page);
    if (p >= btn + tp + tl) return lj_w_sbar_set(b, b->pos + page);
    b->drag = 1; b->drag_off = p - btn - tp;
    return 0;
}

int lj_w_sbar_key(lj_w_sbar *b, int key)
{
    int page = b->page > 0 ? b->page : 1;
    int back = b->vertical ? LJ_VK_UP : LJ_VK_LEFT, fwd = b->vertical ? LJ_VK_DOWN : LJ_VK_RIGHT;
    if (key == back) return lj_w_sbar_set(b, b->pos - 1);
    if (key == fwd) return lj_w_sbar_set(b, b->pos + 1);
    if (key == LJ_VK_PRIOR) return lj_w_sbar_set(b, b->pos - page);
    if (key == LJ_VK_NEXT) return lj_w_sbar_set(b, b->pos + page);
    if (key == LJ_VK_HOME) return lj_w_sbar_set(b, b->min);
    if (key == LJ_VK_END) return lj_w_sbar_set(b, b->max);
    return 0;
}
