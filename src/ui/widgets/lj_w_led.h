/* lj_w_led.h - the toolbar LED button (note 07 §4.4, backgroundcueicon.gif 109x24
 * measured): a raised button with a 13-px black-ringed dot at (13,12), red #A0160A
 * off / green #00FF00 on, and the caption from x = 25. The dot is state the owner
 * sets (a background cue or statics active); a click is just OnClick. */
#ifndef LJ_UI_WIDGETS_W_LED_H
#define LJ_UI_WIDGETS_W_LED_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_LED_OFF_RGB 0xA0160Au
#define LJ_LED_ON_RGB  0x00FF00u

typedef struct { lj_widget base; const char *caption; unsigned on:1, pressed:1; } lj_w_led;

lj_widget *lj_w_led_create(lj_widget *parent, const lj_ctl_desc *d);   /* caption from the desc; checked = LJ_YES lights it */
void       lj_w_led_set(lj_widget *w, int on);
int        lj_w_led_on(const lj_widget *w);

#endif /* LJ_UI_WIDGETS_W_LED_H */
