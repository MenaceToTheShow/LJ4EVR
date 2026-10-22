/* lj_ctl_rgbpack.h - the RGB pack control window (LJ_CTL_RGBPACK, caption "Colors", [04 s5.3],
 * [07 s9.9], CHM genericrgbpack.htm; LORD_FIDELITY-18) for the Generic RGB Pack profiles (up to
 * 10 units, R,G,B channel order). Left: one status panel per unit ("Unit n") with an O/S/F
 * letter button, a level bar and the value per colour, and the resulting colour swatch;
 * clicking a panel enables it (Ctrl-click: that one only), a double-click copies its colour into
 * the programming panel ("Get color"). Right: the programming panel, R / G / B faders with a
 * Snap button each, H / S / V faders, Color Dialog (greyed: no OS colour picker, docs/fidelity.md),
 * All / None / Update. Faders and Snap act on the enabled units of every selected fixture of
 * the master's profile through the controls write path (seam 2). rgbcolorcontrol.gif is a
 * reduced capture (not in 07 s14's 1:1 list): every size here is a PROPOSAL rig item. */
#ifndef LJ_CONTROLS_CTL_RGBPACK_H
#define LJ_CONTROLS_CTL_RGBPACK_H

#include <stdint.h>
#include "controls/lj_ctl_common.h"

#define LJ_RGBPACK_UNITS 10

void     lj_ctl_rgbpack_build(lj_ctl_win *c);        /* lj_ctl_open_table's builder for LJ_CTL_RGBPACK (instead of the row renderer) */
void     lj_ctl_rgbpack_repaint(lj_ctl_win *c);      /* the status panels and the programming faders from the current scene */
int      lj_ctl_rgbpack_units(void);                 /* units of the open window (rows / 3), 0 when closed */
uint16_t lj_ctl_rgbpack_enabled(void);               /* bit u = unit u+1 enabled */
void     lj_ctl_rgbpack_enable(int unit, int on, int only);   /* unit 1..10 (0 = all); only = Ctrl-click */
int      lj_ctl_rgbpack_set(int k, int v);           /* programming fader k = 0 R, 1 G, 2 B, 3 H, 4 S, 5 V to v 0..255: writes the enabled units; 0 / -1 */
int      lj_ctl_rgbpack_get_color(int unit);         /* "Get color": unit 1..10 into the R/G/B faders; 0 / -1 */
void     lj_ctl_rgbpack_update(void);                /* Update: the R/G/B faders into every enabled unit */
void     lj_ctl_rgbpack_snap(int k);                 /* Snap under fader k (0..2): that colour's channels of the enabled units to Snap */
void     lj_hsv_to_rgb(int h, int s, int v, uint8_t rgb[3]);   /* integer HSV, every component 0..255 (h: 0 red, 85 green, 170 blue) */

#endif /* LJ_CONTROLS_CTL_RGBPACK_H */
