/* lj_w_button.c - TButton: the classic raised push button (7.3). Black 1-px outer frame
 * when focused or when Default and no other button has the focus; pressed look while
 * the left button is held inside; caption centred, +1,+1 when pressed; dotted focus
 * rect 3 px inside the bevel; Space clicks (7.9). TBitBtn reuses all of it. */
#include "ui/widgets/lj_w_button.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "gfx/lj_draw.h"

int lj_w_button_is_default(const lj_widget *w)
{
    const lj_ctl_desc *d = w->desc;
    if (!d) return 0;
    if (d->default_btn) return d->default_btn == LJ_YES;
    return d->kind_str && (strcmp(d->kind_str, "bkOK") == 0 || strcmp(d->kind_str, "bkYes") == 0);
}

static int shows_default_frame(const lj_widget *w)
{
    const lj_widget *root, *f;
    if (w->focused) return 1;
    if (!lj_w_button_is_default(w)) return 0;
    for (root = w; root->parent; root = root->parent) { }
    f = lj_widget_focused((lj_widget *)root);
    return !(f && f != w && (f->kind == LJ_K_BUTTON || f->kind == LJ_K_BITBTN));
}

void lj_w_button_paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *b = (lj_w_cap *)w;
    const lj_font *f = lj_w_font_of(w);
    int x, y, ww = w->w, hh = w->h, down = b->pressed && b->hover, tw;
    lj_widget_abs(w, &x, &y);
    if (shows_default_frame(w)) {
        lj_bevel1(s, x, y, ww, hh, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW);
        x++; y++; ww -= 2; hh -= 2;
    }
    if (down) lj_button_pressed(s, x, y, ww, hh); else lj_button_raised(s, x, y, ww, hh);
    tw = lj_text_width(f, b->caption);
    lj_w_caption_text(s, w, x + (ww - tw) / 2 + down, y + (hh - f->height) / 2 + down, b->caption);
    if (w->focused) lj_focus_rect(s, x + 3, y + 3, ww - 6, hh - 6);
}

int lj_w_button_mouse_ev(lj_widget *w, const lj_mouse_ev *m)
{
    return lj_w_button_mouse((lj_w_cap *)w, m, 1);
}

int lj_w_button_key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_SPACE) { lj_w_click(w); return 1; }
    return 0;
}

static const lj_widget_vt vt = { lj_w_button_paint, lj_w_button_mouse_ev, lj_w_button_key, NULL };

lj_widget *lj_w_button_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 1);
}
