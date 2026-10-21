/* lj_menu.h - menu bar, drop-downs, popup menus and the system menu (design 7.3, 7.9).
 * One lj_menu per window, embedded in lj_window and driven by lj_window.c. Every
 * open level is its own surface that the host shows as a borderless popup window
 * at screen coordinates; headless (no host) the levels are still built and
 * navigable, so the logic is testable without SDL. Metrics: bar 19 px, items
 * 17 px, separators 9 px, 20-px check gutter, 3-px raised border, MS Sans Serif. */
#ifndef LJ_UI_MENU_H
#define LJ_UI_MENU_H

#include <stddef.h>
#include "ui/lj_window.h"

#define LJ_MENU_LEVELS 4
#define LJ_MENU_BAR_H  19
#define LJ_MENU_ITEM_H 17
#define LJ_MENU_SEP_H  9

/* Runtime state per item: the DFM gives the initial checked/enabled/visible,
 * behaviour code flips them by name; fn is the bound OnClick (ME-16). */
typedef struct { const lj_menu_item_desc *item; void (*fn)(lj_widget *w, void *ud); void *ud; unsigned checked:1, enabled:1, visible:1; } lj_menu_rt;

typedef struct { const lj_menu_item_desc *items; int n; int sel; int x, y; lj_surface s; void *handle; int short_x; } lj_menu_level;   /* sel -1 = none; x,y screen */

typedef struct lj_menu {
    lj_window *owner; const lj_window_host *host;
    const lj_menu_desc *bar; int bar_x, bar_y, bar_w;            /* the bar band on the owner surface; bar NULL = no main menu */
    int bar_sel; unsigned bar_active:1, bar_open:1, dirty:1;     /* bar_open: lv[0] is bar item bar_sel's drop-down; dirty: the bar band needs repainting */
    lj_menu_level lv[LJ_MENU_LEVELS]; int depth;
    lj_menu_rt *rt; int nrt;
    const lj_menu_item_desc *firing;
} lj_menu;

int  lj_menu_init(lj_menu *m, lj_window *owner, const lj_form_desc *form, const lj_menu_desc *bar, const lj_menu_desc *extra);   /* extra = the system menu or NULL; 0 / -1 */
void lj_menu_free(lj_menu *m);
lj_menu_rt *lj_menu_rt_find(lj_menu *m, const lj_menu_item_desc *item);
lj_menu_rt *lj_menu_rt_named(lj_menu *m, const char *name);
int  lj_menu_set_checked(lj_menu *m, const char *name, int v);   /* -1 when the name is unknown */
int  lj_menu_set_enabled(lj_menu *m, const char *name, int v);
int  lj_menu_set_visible(lj_menu *m, const char *name, int v);

void lj_menu_bar_paint(lj_menu *m, lj_surface *s);               /* the bar band at bar_x, bar_y, bar_w; clears dirty */
int  lj_menu_bar_item_at(const lj_menu *m, int x, int y);        /* surface coords; -1 */
int  lj_menu_bar_item_rect(const lj_menu *m, int i, lj_rect *r); /* -1 when i is not a visible item */
int  lj_menu_bar_mouse(lj_menu *m, const lj_mouse_ev *ev);       /* mouse in the bar band (surface coords); 1 consumed */
int  lj_menu_popup_mouse(lj_menu *m, void *handle, const lj_mouse_ev *ev);   /* mouse on an open level (coords relative to it); 1 consumed */
int  lj_menu_key(lj_menu *m, const lj_key_ev *k);                /* navigation while open or bar-active; 1 consumed */
int  lj_menu_alt_letter(lj_menu *m, int ch);                     /* Alt+letter: opens that bar item; 1 when matched */
int  lj_menu_shortcut(lj_menu *m, const lj_key_ev *k);           /* TShortCut words of the bar's items; 1 when fired */
void lj_menu_activate_bar(lj_menu *m);                           /* Alt release / F10: keyboard mode on the first item, or closes when already active */
void lj_menu_open_bar(lj_menu *m, int i);                        /* drops bar item i down */
void lj_menu_popup(lj_menu *m, const lj_menu_desc *d, int sx, int sy);   /* a TPopupMenu / the system menu at screen x,y */
void lj_menu_close(lj_menu *m);                                  /* every level and the bar highlight */
int  lj_menu_is_open(const lj_menu *m);                          /* a level is open or the bar is in keyboard mode */
int  lj_menu_shortcut_text(int shortcut, char *out, size_t cap); /* Delphi ShortCutToText: "Shift+Ctrl+S", "F5"; 0 / -1 when 0 */
int  lj_menu_accel(const char *caption);                         /* the character after '&', lower-cased; 0 when none */

#endif /* LJ_UI_MENU_H */
