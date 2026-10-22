/* lj_w_fader.h - the vertical fader of the Levels and Master Intensity windows
 * (note 07 §9.7, §10; levelcontrol.gif measured): a 32-px raised column with a ▲
 * button, a track whose white-to-black gradient is covered from the TOP by blue
 * #0000FF down to the value (0% = all blue, 100% = all gradient), the percentage
 * in white at the foot, and a ▼ button. */
#ifndef LJ_UI_WIDGETS_W_FADER_H
#define LJ_UI_WIDGETS_W_FADER_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_FADER_W      32
#define LJ_FADER_BTN_H  12
#define LJ_FADER_TRACK_X 3
#define LJ_FADER_TRACK_W 25
#define LJ_FADER_TRACK_Y 16
#define LJ_FADER_CHROME  32      /* rows not in the track: top 16 + bottom 16 */

typedef struct { lj_widget base; int value, max; unsigned pressed:2, dragging:1;
                 void (*text)(int value, char *buf, size_t cap); } lj_w_fader;   /* value 0..max (255); text NULL = "NN%" readout, else the owner formats it (Smoke Control "FF" / "10s") */

lj_widget *lj_w_fader_create(lj_widget *parent, const lj_ctl_desc *d);   /* position (value), max from the desc; height from the desc (track = height - 32) */
int        lj_w_fader_value(const lj_widget *w);
int        lj_w_fader_set_value(lj_widget *w, int v);                    /* clamps; no event */

#endif /* LJ_UI_WIDGETS_W_FADER_H */
