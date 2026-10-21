/* lj_w_fixicon.h - the fixture icon tile of the desktop (design 7.8, note 07 §4.7;
 * unselectedfixture.gif 63x53 measured): a raised tile whose 15-px caption bar reads
 * "(n) UserName" in white on blue (unselected), bright red (master), maroon (slave)
 * or grey (move mode), a green/cyan square when programmed, the fixture bitmap
 * centred below. Paint only: the desktop owns selection and drag. */
#ifndef LJ_UI_WIDGETS_W_FIXICON_H
#define LJ_UI_WIDGETS_W_FIXICON_H

#include "gfx/lj_surface.h"
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

#define LJ_FIXICON_W 63
#define LJ_FIXICON_H 53
#define LJ_FIXICON_CAP_H 15
enum { LJ_FIX_UNSELECTED = 0, LJ_FIX_MASTER, LJ_FIX_SLAVE, LJ_FIX_MOVE };
enum { LJ_FIX_PROG_NONE = 0, LJ_FIX_PROG_SCENE, LJ_FIX_PROG_SEQ };

typedef struct { lj_widget base; int number; const char *user_name; int state, programmed; const lj_surface *bitmap; } lj_w_fixicon;

lj_widget *lj_w_fixicon_create(lj_widget *parent, const lj_ctl_desc *d);   /* tag = number, caption = user name */
void       lj_w_fixicon_set(lj_widget *w, int state, int programmed, const lj_surface *bitmap);

#endif /* LJ_UI_WIDGETS_W_FIXICON_H */
