/* lj_ctl_gobos.c - the Gobos window and its rows (lj_ctl_gobos.h): Index | Rotate mode
 * pair, the gobo index bar, the shake control. */
#include "controls/lj_ctl_gobos.h"

#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

int lj_ctl_c_modepair_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    /* measured from goboindex.gif: two 65x19 flat buttons under the scroll control (130 wide), selected one black with white text; reduced x1.25 = 81x24 (ME-67), rig item */
    static const char *const pair[] = { "Index", "Rotate" };   /* the captions on screen; "Index Gobo" / "Rotate Gobo" are the exe hints (15/01 s6) */
    int j, n = r->buttons ? r->nbuttons : 2, bw = n > 0 ? (LJ_SCROLLCTL_W - 2) / n : 81;
    for (j = 0; j < n; j++) {
        const char *cap = r->buttons ? r->buttons[j].caption : pair[j & 1];
        lj_widget *b = lj_ctl_c_btn(c, lj_window_root(c->win), x + j * bw, y + 1, bw, 24, cap, NULL, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_3DDKSHADOW, 1);
        lj_ctl_c_bind *bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_MODE, j, r->ch);
        b->hint = j == 0 ? r->caption : r->caption2;
        if (bd && r->buttons) bd->btn = &r->buttons[j];
    }
    return 25;
}

int lj_ctl_c_indexbar_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    /* measured from goboindexcontrol.gif 134x22: the bar alone, "nn,n%" caption, hint Gobo Index; reduced x1.25 = 166x25 (ME-67), rig item */
    lj_widget *b = lj_ctl_c_bar(c, lj_window_root(c->win), x, y, 166, 25, NULL, r->hi > r->lo ? r->hi - r->lo : 255);
    b->hint = r->caption;
    lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_INDEX, -1, r->ch);
    return 25;
}

int lj_ctl_c_shake_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    /* measured from goboshake.gif 121x32: shake glyph button (4,3) 21x23, speed bar (28,6) 90x20; reduced x1.25 = 151x41 (ME-67), rig item */
    lj_widget *panel = lj_w_panel_create(lj_window_root(c->win), NULL), *b;
    lj_ctl_c_bind *bd;
    panel->x = x; panel->y = y; panel->w = 151; panel->h = 41;
    b = lj_ctl_c_btn(c, panel, 5, 5, 26, 31, NULL, "shake", LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW, 0);
    b->hint = r->caption;
    bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_SHAKE_BTN, 0, r->ch);
    if (bd) bd->btn = r->nbuttons > 0 ? &r->buttons[0] : NULL;
    b = lj_ctl_c_bar(c, panel, 35, 9, 113, 25, NULL, r->hi > r->lo ? r->hi - r->lo : 255);
    b->hint = r->caption2;
    lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_SHAKE_BAR, -1, r->ch);
    return 41;
}

static void shake_emit(lj_ctl_c_bind *b, int on)
{
    lj_ctl_c *c = b->c;
    const lj_ctl_row *r = b->row;
    lj_ctl_c_bind *bar = NULL;
    int i;
    for (i = 0; i < c->nb; i++) if (c->b[i].row == r && c->b[i].role == LJ_CTL_C_SHAKE_BAR) bar = &c->b[i];
    if (!on) { lj_ctl_c_emit(c, b->ch, lj_ctl_c_scroll_value(c, b->ch)); return; }   /* shake off = back to the gobo the scroll row shows */
    if (r->hi > r->lo && bar) lj_ctl_c_emit(c, b->ch, r->lo + lj_w_dragbar_value(bar->w));
    else lj_ctl_c_emit(c, b->ch, r->nbuttons > 0 ? r->buttons[0].value : r->lo);
}

int lj_ctl_c_gobos_event(lj_ctl_c_bind *b, int ev)
{
    lj_ctl_c *c = b->c;
    uint8_t v = 0, st = LJ_OFF;
    switch (b->role) {
    case LJ_CTL_C_MODE: {
        const lj_ctl_row *sr = lj_ctl_c_scroll_of_mode(c, b->row);
        int i;
        if (ev != LJ_WEV_CLICK || !sr || sr->ch >= LJ_PROFILE_CH || c->mode[sr->ch] == b->entry) return 1;
        i = lj_ctl_c_scroll_entry(c, sr);                               /* the gobo shown now, read in the old mode */
        c->mode[sr->ch] = (uint8_t)b->entry;
        if (i >= 0) lj_ctl_c_emit(c, sr->ch, lj_ctl_c_entry_value(c, sr, i));   /* re-sent in the new mode's range (15/01 s6) */
        else lj_ctl_c_refresh(c->win);
        return 1; }
    case LJ_CTL_C_INDEX:
        if (ev == LJ_WEV_CHANGE) lj_ctl_c_emit(c, b->ch, b->row->reverse ? b->row->hi - lj_w_dragbar_value(b->w) : b->row->lo + lj_w_dragbar_value(b->w));
        return 1;
    case LJ_CTL_C_SHAKE_BTN:
        if (ev == LJ_WEV_CLICK) shake_emit(b, !(lj_ctl_c_value(c, b->ch, &v, &st) == 0 && v >= b->row->lo && v <= b->row->hi && b->row->hi > b->row->lo));
        return 1;
    case LJ_CTL_C_SHAKE_BAR:
        if (ev == LJ_WEV_CHANGE && lj_ctl_c_value(c, b->ch, &v, &st) == 0 && v >= b->row->lo && v <= b->row->hi) shake_emit(b, 1);
        return 1;
    default: return 0;
    }
}

int lj_ctl_c_gobos_refresh(lj_ctl_c_bind *b)
{
    lj_ctl_c *c = b->c;
    uint8_t v = 0, st = LJ_OFF;
    int have = lj_ctl_c_value(c, b->ch, &v, &st) == 0;
    switch (b->role) {
    case LJ_CTL_C_MODE: {
        const lj_ctl_row *sr = lj_ctl_c_scroll_of_mode(c, b->row);
        uint8_t sch = sr ? sr->ch : b->ch, sv = 0, sst = LJ_OFF;
        int m = sch < LJ_PROFILE_CH ? c->mode[sch] : 0, on;
        if (b->btn && b->btn->hi > b->btn->lo && lj_ctl_c_value(c, sch, &sv, &sst) == 0) {   /* with ranges in the table the channel value says which mode is live */
            int j;
            for (j = 0; j < b->row->nbuttons; j++) if (lj_ctl_c_in(&b->row->buttons[j], sv)) { m = j; if (sch < LJ_PROFILE_CH) c->mode[sch] = (uint8_t)j; }
        }
        on = m == b->entry;
        lj_ctl_c_btn_set_down(b->w, on);
        lj_ctl_c_btn_set_face(b->w, on ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNHIGHLIGHT, on ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);   /* rotationmode.gif: selected black, white text */
        return 1; }
    case LJ_CTL_C_INDEX:
        if (have && v >= b->row->lo && v <= b->row->hi) lj_w_dragbar_set_value(b->w, b->row->reverse ? b->row->hi - v : v - b->row->lo);
        return 1;
    case LJ_CTL_C_SHAKE_BTN:
        lj_ctl_c_btn_set_down(b->w, have && ((b->btn && lj_ctl_c_in(b->btn, v)) || (b->row->hi > b->row->lo && v >= b->row->lo && v <= b->row->hi)));
        return 1;
    case LJ_CTL_C_SHAKE_BAR:
        if (have && b->row->hi > b->row->lo && v >= b->row->lo && v <= b->row->hi) lj_w_dragbar_set_value(b->w, v - b->row->lo);
        return 1;
    default: return 0;
    }
}

lj_window *lj_ctl_gobos_create(const lj_profile *p) { return lj_ctl_c_create(LJ_CTL_GOBOS, p); }
void lj_ctl_gobos_refresh(lj_window *win) { lj_ctl_c_refresh(win); }
void lj_ctl_gobos_destroy(lj_window *win) { lj_ctl_c_destroy(win); }
