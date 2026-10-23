/* lj_w_speedbutton.c - TSpeedButton (7.3): raised, pressed while held inside, latched
 * (Down with GroupIndex) = pressed edge over the white/#C0C0C0 checkerboard; Flat = no
 * edge until hover (1-px raised), 1-px lowered when pressed or down. Never takes the
 * focus. GroupIndex/AllowAllUp exactly as TSpeedButton.SetDown/UpdateExclusive. */
#include "ui/widgets/lj_w_speedbutton.h"
#include "ui/widgets/lj_widgets_basic.h"

#include "gfx/lj_draw.h"

typedef struct { lj_w_cap c; int group; unsigned flat:1, allow_all_up:1; } lj_w_sb;

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_sb *b = (lj_w_sb *)w;
    int x, y, pressed = b->c.pressed && b->c.hover, latched = b->c.down && b->group, off = pressed || latched;
    lj_widget_abs(w, &x, &y);
    if (b->flat) {
        lj_fill(s, x, y, w->w, w->h, w->color);
        if (off) lj_bevel1(s, x, y, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
        else if (b->c.hover && w->enabled) lj_bevel1(s, x, y, w->w, w->h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
    } else if (pressed) {
        lj_button_pressed(s, x, y, w->w, w->h);
    } else if (latched) {
        lj_button_latched(s, x, y, w->w, w->h);
    } else {
        lj_button_raised(s, x, y, w->w, w->h);
    }
    lj_w_glyph_caption(s, w, x + off, y + off, w->w, w->h, b->c.caption);
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    return w->enabled ? lj_w_button_mouse((lj_w_cap *)w, m, 0) : 0;
}

void lj_w_speedbutton_set_down(lj_widget *w, int down)
{
    lj_w_sb *b = (lj_w_sb *)w;
    lj_widget *c;
    if (!w || w->kind != LJ_K_SPEEDBUTTON || !b->group || (unsigned)(down != 0) == b->c.down) return;
    if (b->c.down && !b->allow_all_up) return;
    b->c.down = down != 0;
    lj_widget_invalidate(w);
    if (!down || !w->parent) return;
    for (c = w->parent->first; c; c = c->next)                 /* UpdateExclusive: the rest of the group goes up */
        if (c != w && c->kind == LJ_K_SPEEDBUTTON && ((lj_w_sb *)c)->group == b->group && ((lj_w_sb *)c)->c.down) {
            ((lj_w_sb *)c)->c.down = 0;
            lj_widget_invalidate(c);
        }
}

void lj_w_speedbutton_click(lj_widget *w)
{
    lj_w_speedbutton_set_down(w, !((lj_w_sb *)w)->c.down);
    lj_w_emit(w, LJ_WEV_CLICK);
}

int lj_w_speedbutton_down(const lj_widget *w)
{
    return w && w->kind == LJ_K_SPEEDBUTTON ? (int)((const lj_w_sb *)w)->c.down : 0;
}

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_speedbutton_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_sb *b = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_sb), 0);
    if (d) {
        b->group = d->group_index;
        b->flat = lj_desc_bool(d->flat, 0) != 0;
        b->allow_all_up = lj_desc_bool(d->allow_all_up, 0) != 0;
        b->c.down = b->group && lj_desc_bool(d->down, 0);
    }
    return &b->c.w;
}
