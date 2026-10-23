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
void       lj_desktop_config_save(void);                 /* lj_show_save_config through the library (Config.dat); logs on failure; snaps the placed icons first when the grid snaps */

/* ---- W4 dlg-desktop (F13, [01 s3.5]): the Tabs / Grid / Image / Auto-Arrange dialogs act through these; every change is saved
 * to the library's Tabs.Ini at once ([TABS] [TABNAMES] [IMAGE] are LJ2's; [GRID] Use VSize HSize Snap SnapCenter is ours, PROPOSAL) */
const char *lj_desktop_tab_name(int tab);                /* "" out of range */
int         lj_desktop_tab_add(const char *name);        /* appended; 0 / -1 (LJ_TABS_MAX, empty name) */
int         lj_desktop_tab_rename(int tab, const char *name);
int         lj_desktop_tab_delete(int tab);              /* its icons are unassigned, later tabs shift down; the last tab stays (-1) */
typedef struct { int on, vsize, hsize, snap, snap_center; } lj_desktop_grid;   /* TGridForm: Use Grid, Grid V-Size / H-Size 8..64, Snap Fixture Icons to Grid, Snap Icon Center / Left/Top */
void        lj_desktop_grid_get(lj_desktop_grid *g);
void        lj_desktop_grid_set(const lj_desktop_grid *g);   /* clamps 8..64, snaps the placed icons when Snap is on, repaints, saves */
int         lj_desktop_grid_snap(int v, int size, int icon_extent, int center);   /* one axis: the nearest grid line for Left/Top, or the icon centre on it */
void        lj_desktop_tabs_set_bmp_loader(int (*load)(const char *path, lj_surface *out));   /* the app's SDL_LoadBMP (gfx has no decoder, 7.8); applies Tabs.Ini [IMAGE] File= at once */
int         lj_desktop_image_set(const char *file, int stretch, int center);   /* "" / NULL = Clear Image; decodes through the loader, then saves [IMAGE]; -1 when it does not decode (nothing changes) */
const char *lj_desktop_image_file(void);                 /* the [IMAGE] File= in force, "" = none */
const lj_surface *lj_desktop_image(void);                /* the decoded wallpaper, NULL = none */
void        lj_desktop_image_flags(int *stretch, int *center);
void        lj_desktop_auto_arrange(int by_type);        /* TAutoArrangeForm: every placed icon of the current tab re-laid on the LJ_ICON_PITCH grid by fixture number or type */

#endif /* LJ_DESKTOP_DESKTOP_TABS_H */
