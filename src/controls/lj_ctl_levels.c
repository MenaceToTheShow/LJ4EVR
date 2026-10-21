/* lj_ctl_levels.c - one Levels column: caption, fader, O/S/F (note 07 s9.7; ME-25 for switch packs via lj_ctl_states). */
#include "controls/lj_ctl_levels.h"

#include <stdio.h>
#include "core/lj_str.h"
#include "ui/widgets/lj_widgets_lj.h"

enum { LJ_LV_CAP_Y = 2, LJ_LV_FADER_Y = 16, LJ_LV_FADER_H = 162, LJ_LV_OSF_Y = 182, LJ_LV_OSF_H = 20 };   /* measured from levelcontrol.gif: captions y 44-56, faders 57-218, fade buttons 221-240, client top 41 */

int lj_ctl_levels_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y)
{
    lj_widget *f = lj_w_fader_create(parent, NULL), *lbl;
    lj_ctl_elem *e;
    const char *caption = row->caption;
    f->x = x; f->y = y + LJ_LV_FADER_Y; f->w = LJ_FADER_W; f->h = LJ_LV_FADER_H;
    ((lj_w_fader *)f)->max = row->hi > row->lo ? row->hi - row->lo : 255;
    e = lj_ctl_elem_add(c, f, row, NULL, LJ_CE_FADER);
    if (!e) return 0;
    if (row->hi <= row->lo) { e->lo = 0; e->hi = 255; }
    f->on_event = lj_ctl_on_event; f->ud = e;
    lj_ctl_osf_new(c, parent, row, x, y + LJ_LV_OSF_Y, LJ_FADER_W - 1, LJ_LV_OSF_H);
    if (!caption && c->profile && row->ch < LJ_PROFILE_CH && c->profile->level_caption[row->ch][0]) {
        lj_str_copy(e->text, sizeof e->text, (const char *)c->profile->level_caption[row->ch]);
        caption = e->text;
    } else if (!caption) {
        snprintf(e->text, sizeof e->text, "CH%d", row->ch + 1);   /* the profile editor's default caption (note 07 s9.7 "CH8") */
        caption = e->text;
    }
    lbl = lj_ctl_label_new(c, parent, caption, x, y + LJ_LV_CAP_Y, LJ_FADER_W, 13, 1);
    (void)lbl;
    return LJ_LV_OSF_Y + LJ_LV_OSF_H;
}
