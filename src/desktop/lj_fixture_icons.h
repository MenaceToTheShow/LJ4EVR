/* lj_fixture_icons.h - the fixture icon tiles of the desktop (design 7.8, [07 s4.7],
 * [01 s3.1], F11): "(n) UserName" caption bar blue / bright red master / maroon slave /
 * grey move mode, the programmed square (option, off by default), our 50-px family
 * silhouette from art/icons, Fixed Size (63x53) or Image Size. One widget per fixture,
 * child of the icon area, named icon<n> (n = fixture number, docs/ui-script.md).
 * Public API: lj_desktop_icons_refresh / icon_at / icon_rect in lj_desktop.h. */
#ifndef LJ_DESKTOP_FIXTURE_ICONS_H
#define LJ_DESKTOP_FIXTURE_ICONS_H

#include "desktop/lj_desktop.h"
#include "fixlib/lj_profile.h"

void              lj_fixture_icons_build(lj_widget *area);              /* the icon area these tiles live in */
lj_widget        *lj_fixture_icon_widget(int fixture);                  /* NULL when not built */
const lj_surface *lj_fixture_icon_bitmap(const lj_profile *p);          /* family silhouette: "movinghead" when the profile has a Movement window, else "dimmer" */
int               lj_desktop_icon_programmed(int fixture);              /* LJ_FIX_PROG_NONE / _SCENE / _SQ of lj_w_fixicon.h for the current sequence */
int               lj_desktop_icon_style(void);                          /* 0 Fixed Size, 1 Image Size [F13] */
void              lj_desktop_icon_style_set(int image_size);

#endif /* LJ_DESKTOP_FIXTURE_ICONS_H */
