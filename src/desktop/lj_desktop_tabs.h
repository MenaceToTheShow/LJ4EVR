/* lj_desktop_tabs.h - the desktop area of the main window (design 7.8, [07 s4.7],
 * [01 s2.1, s3.5], F13): the tab strip from Tabs.Ini [TABNAMES], the icon area
 * (clBtnFace, optional wallpaper Stretch/Center, rubber-band selection, the two
 * popups, drop target of the Unassigned list), icon placement persisted through
 * Config.dat (placed x y tab), move mode, Show Programmed Fixtures, icon alignment.
 * Public API: lj_desktop_tab* / place_icon / unassign_icon / popup in lj_desktop.h. */
#ifndef LJ_DESKTOP_DESKTOP_TABS_H
#define LJ_DESKTOP_DESKTOP_TABS_H

#include "desktop/lj_desktop.h"

enum { LJ_TABS_MAX = 32, LJ_ICON_PITCH = 75 };   /* desktop2.gif: 12 icons on a loose grid with ~75 px pitch, reduced, rig item */

void       lj_desktop_tabs_build(lj_widget *root);       /* alClient under root: tab control + icon area */
lj_widget *lj_desktop_icon_area(void);                   /* the widget the tiles live in; its client = icon-area coordinates */
int        lj_desktop_popup_fixture(void);               /* the fixture the open icon popup was raised on, -1 */
void       lj_desktop_move_mode(int on);                 /* Icons > Move All [01 s3.1 move mode] */
int        lj_desktop_move_mode_on(void);
void       lj_desktop_icon_move(int fixture, int on);    /* Fixture Icon > Move Icon */
int        lj_desktop_icon_move_on(int fixture);
void       lj_desktop_show_programmed(int on);           /* Icons > Show Programmed Fixtures (off by default) */
int        lj_desktop_show_programmed_on(void);
void       lj_desktop_unassign_tab(int selected_only);   /* Icons > Unassign Icons > all on current tab / selected icons */
void       lj_desktop_align_icons(int to, const char *how);   /* "Top" "HCenter" "Bottom" "Left" "VCenter" "Right": every selected icon to this one (not icons in move mode) */
void       lj_desktop_auto_place(int by_type);           /* Auto Place All: unplaced fixtures onto the current tab on the LJ_ICON_PITCH grid */
void       lj_desktop_config_save(void);                 /* lj_show_save_config through the library (Config.dat); logs on failure */

#endif /* LJ_DESKTOP_DESKTOP_TABS_H */
