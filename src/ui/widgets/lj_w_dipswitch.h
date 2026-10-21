/* lj_w_dipswitch.h - TDipSwitch (3 uses, 62x24, Kind = TDip10): a DMX address shown
 * in binary on a blue base with white switches; switch i (left to right) = bit i-1,
 * a set bit throws the switch DOWN, towards the white arrow (fixtureconfigurationsettings.gif
 * at address 123, showdipswitch.gif at 0; measured). Digits 1234567890 under the row. */
#ifndef LJ_UI_WIDGETS_W_DIPSWITCH_H
#define LJ_UI_WIDGETS_W_DIPSWITCH_H

#include <stdint.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

typedef struct { lj_widget base; int value, nswitches; uint32_t base_rgb, dip_rgb, text_rgb; } lj_w_dipswitch;

lj_widget *lj_w_dipswitch_create(lj_widget *parent, const lj_ctl_desc *d);   /* Value, BaseColor, DipColor, TextColor, Kind = TDip<n> (default 10) */
int        lj_w_dipswitch_value(const lj_widget *w);
int        lj_w_dipswitch_set_value(lj_widget *w, int v);                      /* masked to the switch count; no event */

#endif /* LJ_UI_WIDGETS_W_DIPSWITCH_H */
