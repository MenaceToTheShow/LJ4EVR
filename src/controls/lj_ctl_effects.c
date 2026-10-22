/* lj_ctl_effects.c - the Effects window and the LJ_W_EFFECT_BTN row (lj_ctl_effects.h):
 * the single-effect button of effectcontrol4.gif (prism in / out). */
#include "controls/lj_ctl_effects.h"

#include "gfx/lj_draw.h"
#include "gfx/lj_glyphs.h"
#include "ui/widgets/lj_widgets_lj.h"

int lj_ctl_c_effectbtn_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y, int *right)
{
    /* measured from effectcontrol4.gif: black 27x27 button at client (43,3), the rotation panel from x = 72; reduced x1.25 = 34x34 at (54,4) (ME-67), rig item */
    lj_widget *b = lj_ctl_c_btn(c, lj_window_root(c->win), x + 5, y + 4, 34, 34, r->caption, r->items ? r->items[0] : NULL, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNHIGHLIGHT, 0);
    lj_ctl_c_bind *bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_FXBTN, 0, r->ch);
    if (bd) bd->btn = r->nbuttons > 0 ? &r->buttons[0] : NULL;
    b->hint = r->caption;
    *right = x + 5 + 34 + 2;
    return 41;
}

int lj_ctl_c_effects_event(lj_ctl_c_bind *b, int ev)
{
    uint8_t v = 0, st = LJ_OFF;
    int in;
    if (b->role != LJ_CTL_C_FXBTN) return 0;
    if (ev != LJ_WEV_CLICK || !b->btn) return 1;
    in = lj_ctl_c_value(b->c, b->ch, &v, &st) == 0 && lj_ctl_c_in(b->btn, v);
    lj_ctl_c_emit(b->c, b->ch, in ? b->row->lo : b->btn->value);
    return 1;
}

int lj_ctl_c_effects_refresh(lj_ctl_c_bind *b)
{
    uint8_t v = 0, st = LJ_OFF;
    if (b->role != LJ_CTL_C_FXBTN) return 0;
    lj_ctl_c_btn_set_down(b->w, b->btn && lj_ctl_c_value(b->c, b->ch, &v, &st) == 0 && lj_ctl_c_in(b->btn, v));
    return 1;
}

lj_window *lj_ctl_effects_create(const lj_profile *p) { return lj_ctl_c_create(LJ_CTL_EFFECTS, p); }
void lj_ctl_effects_refresh(lj_window *win) { lj_ctl_c_refresh(win); }
void lj_ctl_effects_destroy(lj_window *win) { lj_ctl_c_destroy(win); }
