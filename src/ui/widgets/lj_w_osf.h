/* lj_w_osf.h - the Off / Snap / Fade fade-state button of every fixture control
 * (note 07 §3.1): raised 48x24, red Off / yellow Snap / green Fade, black text.
 * Left click cycles Off -> Snap -> Fade -> Off, right click the reverse (D10);
 * states_allowed masks out what a channel cannot do (ME-27: Lamp = OFF|SNAP). */
#ifndef LJ_UI_WIDGETS_W_OSF_H
#define LJ_UI_WIDGETS_W_OSF_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

enum { LJ_OSF_OFF = 0, LJ_OSF_SNAP = 1, LJ_OSF_FADE = 2 };
#define LJ_OSF_ALLOW_OFF  1u
#define LJ_OSF_ALLOW_SNAP 2u
#define LJ_OSF_ALLOW_FADE 4u
#define LJ_OSF_ALLOW_ALL  7u

typedef struct { lj_widget base; int state; unsigned allowed; unsigned pressed:1; int press_button; } lj_w_osf;

lj_widget *lj_w_osf_create(lj_widget *parent, const lj_ctl_desc *d);   /* state from desc->position (0..2), all states allowed */
int        lj_w_osf_state(const lj_widget *w);
int        lj_w_osf_set_state(lj_widget *w, int state);                /* -1 when the state is not allowed; no event */
void       lj_w_osf_set_allowed(lj_widget *w, unsigned mask);          /* LJ_OSF_ALLOW_*; moves the state to an allowed one if needed */

#endif /* LJ_UI_WIDGETS_W_OSF_H */
