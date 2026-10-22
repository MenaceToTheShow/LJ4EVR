/* lj_ctl_intensity.c - shutter toggle and specials block of the Intensity control (note 15/01 s4, 07 s9.5). */
#include "controls/lj_ctl_intensity.h"

#include "core/lj_log.h"

/* Shutter Closed / Shutter Open: one black button whose caption follows the channel
 * (open reads only at the exact open value [02 s5.3]); closed emits row->lo, open
 * emits LJ_CTL_SHUTTER_OPEN unless the row carries two buttons {closed, open}. */
static void shutter_event(lj_widget *w, int ev, void *ud)
{
    lj_ctl_elem *e = (lj_ctl_elem *)ud;
    uint8_t v = 0, st = 0;
    int ch = lj_prog_master_ch(e->row->ch);
    (void)w;
    if (ev != LJ_WEV_CLICK || ch < 0 || lj_prog_get((uint16_t)ch, &v, &st) != 0) return;
    lj_ctl_write(e, v == e->hi ? e->lo : e->hi);
}

static int build_shutter(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    lj_widget *b = lj_ctl_button_new(c, parent, row, NULL, LJ_CE_SHUTTER, row->caption, x, y, w, LJ_CTL_ROW_H, 1);   /* black bar, measured from intensitycontrol.gif x 43-177, reduced x1.25 (LJ_CTL_BAR_W), rig item */
    lj_ctl_elem *e = b ? (lj_ctl_elem *)b->ud : NULL;
    if (!e) return 0;
    e->lo = row->buttons && row->nbuttons > 0 ? row->buttons[0].value : row->lo;
    e->hi = row->buttons && row->nbuttons > 1 ? row->buttons[1].value : (uint8_t)LJ_CTL_SHUTTER_OPEN;
    b->on_event = shutter_event;
    return LJ_CTL_ROW_H;
}

/* label | black buttons [| drag bar captioned caption2] across x .. x + w (measured from intensitycontrol.gif: label 183-244, buttons from 247, 3-px gaps; reduced x1.25 in lj_ctl_common.h, rig item) */
static int build_special(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    int bx = x + (LJ_CTL_SPEC_BTN_X - LJ_CTL_SPEC_X), avail = x + w - bx, slots = row->nbuttons + (row->caption2 ? 1 : 0), bw, i;
    if (slots <= 0 || avail < slots) return 0;
    bw = (avail - (slots - 1) * LJ_CTL_GAP) / slots;
    lj_ctl_label_new(c, parent, row->caption, x, y, LJ_CTL_SPEC_LBL_W, LJ_CTL_ROW_H, 0);
    for (i = 0; i < row->nbuttons; i++)
        lj_ctl_button_new(c, parent, row, &row->buttons[i], LJ_CE_BUTTON, row->buttons[i].caption, bx + i * (bw + LJ_CTL_GAP), y, bw, LJ_CTL_ROW_H, 1);
    if (row->caption2)
        lj_ctl_dragbar_new(c, parent, row, NULL, bx + row->nbuttons * (bw + LJ_CTL_GAP), y, avail - row->nbuttons * (bw + LJ_CTL_GAP));   /* the speed bar over lo..hi; ponytail: the GIF reads "Pulse Off" (caption2) at 0 and the speed otherwise, which needs a caption-at-zero mode in ui's drag bar - percentage only until then */
    return LJ_CTL_ROW_H;
}

int lj_ctl_intensity_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    switch (row->kind) {
    case LJ_W_SHUTTER_TOGGLE: return build_shutter(c, parent, row, x, y, w);
    case LJ_W_SPECIAL_GROUP:  return build_special(c, parent, row, x, y, w);
    default: return 0;
    }
}

void lj_ctl_intensity_prefs(lj_ctl_win *c)
{
    int i;
    if (!c || !c->win) return;
    c->specials_shown = !c->specials_shown;
    for (i = 0; i < c->n; i++) if (c->e[i].special) { c->e[i].w->visible = c->specials_shown; lj_widget_invalidate(c->e[i].w); }
    lj_window_resize(c->win, c->specials_shown ? c->cw_full : c->cw_plain, c->desc.client_height);
    lj_window_invalidate_all(c->win);
}
