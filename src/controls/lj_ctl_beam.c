/* lj_ctl_beam.c - the Beam window and the LJ_W_DRAGBAR row (lj_ctl_beam.h): [icon] bar [icon]
 * lines of 21 px, iris / focus / zoom / frost (beam.gif 206x77, beam2.gif). */
#include "controls/lj_ctl_beam.h"

#include <string.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

int lj_ctl_c_dragbar_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    /* measured from beam.gif: left icon at client (40,2) 19x16, bar to x = 177, right icon at (178,2); 21-px line pitch */
    lj_widget *root = lj_window_root(c->win), *b, *icon;
    static const struct { const char *caption, *left, *right; } icons[] = {   /* 15/00 s2: IrisOpen / IrisClosed, Far / Near, ZoomWide / ZoomNarrow at the bar ends; ours */
        { "Iris", "irisopen", "irisclosed" }, { "Focus", "far", "near" }, { "Zoom", "zoomwide", "zoomnarrow" } };
    const char *left = r->items && r->items[0] ? r->items[0] : NULL, *rightg = r->items && r->items[0] && r->items[1] ? r->items[1] : NULL;
    int bx = x, bw = 161, i;
    if (!r->items && r->caption)
        for (i = 0; i < LJ_COUNT_OF(icons); i++) if (strcmp(icons[i].caption, r->caption) == 0) { left = icons[i].left; rightg = icons[i].right; }
    if (left) {
        icon = lj_ctl_c_btn(c, root, x, y + 1, 22, 19, NULL, left, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNHIGHLIGHT, 1);
        icon->enabled = 0; bx += 22; bw -= 22;
    }
    if (rightg) {
        icon = lj_ctl_c_btn(c, root, x + 139, y + 1, 22, 19, NULL, rightg, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNHIGHLIGHT, 1);
        icon->enabled = 0; bw -= 22;
    }
    b = lj_ctl_c_bar(c, root, bx, y, bw, 21, r->caption, r->hi > r->lo ? r->hi - r->lo : 255);
    lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_BEAM_BAR, -1, r->ch);
    return 20;
}

int lj_ctl_c_beam_event(lj_ctl_c_bind *b, int ev)
{
    if (b->role != LJ_CTL_C_BEAM_BAR) return 0;
    if (ev == LJ_WEV_CHANGE) lj_ctl_c_emit(b->c, b->ch, b->row->reverse ? b->row->hi - lj_w_dragbar_value(b->w) : b->row->lo + lj_w_dragbar_value(b->w));
    return 1;
}

int lj_ctl_c_beam_refresh(lj_ctl_c_bind *b)
{
    uint8_t v = 0, st = LJ_OFF;
    if (b->role != LJ_CTL_C_BEAM_BAR) return 0;
    if (lj_ctl_c_value(b->c, b->ch, &v, &st) == 0 && v >= b->row->lo && v <= b->row->hi)
        lj_w_dragbar_set_value(b->w, b->row->reverse ? b->row->hi - v : v - b->row->lo);
    return 1;
}

lj_window *lj_ctl_beam_create(const lj_profile *p) { return lj_ctl_c_create(LJ_CTL_BEAM, p); }
void lj_ctl_beam_refresh(lj_window *win) { lj_ctl_c_refresh(win); }
void lj_ctl_beam_destroy(lj_window *win) { lj_ctl_c_destroy(win); }
