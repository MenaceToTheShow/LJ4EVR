/* lj_ctl_extended.c - group box + radios + speed fader columns of the Extended control (note 15/01 s10). */
#include "controls/lj_ctl_extended.h"

#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

enum { LJ_EXT_RADIO_X = 8, LJ_EXT_RADIO_Y = 12, LJ_EXT_RADIO_PITCH = 13, LJ_EXT_RADIO_W = 92, LJ_EXT_FADER_X = 103, LJ_EXT_FADER_Y = 4, LJ_EXT_FADER_H = 73 };   /* measured from extended.gif: radios 5 rows in 81 px, fader x 108-134 inside the box */

static int is_speed(const lj_ctl_elem *e) { return e->btn && e->btn->lo == e->row->lo && e->btn->hi == e->row->hi; }

static void radio_event(lj_widget *w, int ev, void *ud)
{
    lj_ctl_elem *e = (lj_ctl_elem *)ud;
    lj_ctl_win *c = lj_ctl_of(lj_window_of(w));
    int i;
    if (ev != LJ_WEV_CLICK || !e->btn || !c) return;
    if (is_speed(e))
        for (i = 0; i < c->n; i++)                                    /* Speed: the fader's current position inside lo..hi */
            if (c->e[i].role == LJ_CE_FADER && c->e[i].row == e->row) { lj_ctl_write(e, (uint8_t)(e->row->lo + lj_w_fader_value(c->e[i].w))); return; }
    lj_ctl_write(e, e->btn->value);
}

int lj_ctl_extended_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y)
{
    lj_ctl_elem *ge = lj_ctl_elem_add(c, parent, NULL, NULL, LJ_CE_NONE), *fe;
    lj_widget *box, *f;
    int i;
    if (!ge) return 0;
    box = lj_w_groupbox_create(parent, lj_ctl_desc_new(ge, LJ_K_GROUPBOX, row->caption, x, y, LJ_CTL_EXT_BOX_W, LJ_CTL_EXT_BOX_H));
    ge->w = box;
    for (i = 0; i < row->nbuttons; i++) {
        lj_ctl_elem *re = lj_ctl_elem_add(c, box, row, &row->buttons[i], LJ_CE_RADIO);
        lj_widget *r;
        if (!re) break;
        r = lj_w_radiobutton_create(box, lj_ctl_desc_new(re, LJ_K_RADIOBUTTON, row->buttons[i].caption, LJ_EXT_RADIO_X, LJ_EXT_RADIO_Y + i * LJ_EXT_RADIO_PITCH, LJ_EXT_RADIO_W, LJ_EXT_RADIO_PITCH));
        re->w = r; r->on_event = radio_event; r->ud = re;
    }
    f = lj_w_fader_create(box, NULL);
    f->x = LJ_EXT_FADER_X; f->y = LJ_EXT_FADER_Y; f->w = LJ_FADER_W; f->h = LJ_EXT_FADER_H;
    ((lj_w_fader *)f)->max = row->hi > row->lo ? row->hi - row->lo : 1;
    fe = lj_ctl_elem_add(c, f, row, NULL, LJ_CE_FADER);
    if (fe) { f->on_event = lj_ctl_on_event; f->ud = fe; }
    return LJ_CTL_EXT_BOX_H;
}
