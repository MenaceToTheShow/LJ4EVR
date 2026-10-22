/* lj_window.h - one window: surface + root widget + menu + dirty rects + caption state (design 2.7).
 * Built from a generated lj_form_desc and a binding table; paints into its own
 * lj_surface including the self-drawn classic frame (design 7.5) unless the
 * platform says the OS draws the frame. Knows nothing about SDL or the engine. */
#ifndef LJ_UI_WINDOW_H
#define LJ_UI_WINDOW_H

#include <stdint.h>
#include "gfx/lj_surface.h"
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

typedef struct lj_window lj_window;                          /* surface + root widget + menu + dirty rects + caption state */

/* Behaviour binding (design 7.6): event = the DFM handler name ("sb7Click",
 * "FormCreate"), fn receives the widget (the root for form events) and ud.
 * The table ends with {NULL, NULL, NULL}. ME-16: lj_window_create() logs once per
 * name that matches nothing in the desc and once per DFM handler no binding
 * covers, and asserts in debug builds. */
typedef struct { const char *event; void (*fn)(lj_widget *w, void *ud); void *ud; } lj_binding;
void lj_handler_noop(lj_widget *w, void *ud);   /* ME-101: the one do-nothing binding (FormCreate, Help, events a behaviour file ignores); test_layering counts copies = 0 */

lj_window *lj_window_create(const lj_form_desc *desc, const lj_binding *bindings);   /* instantiates the tree in DFM order; NULL + log on failure */
void       lj_window_destroy(lj_window *win);                                        /* frees the tree and the surface; NULL is a no-op */
lj_widget *lj_window_find(lj_window *win, const char *dfm_name);                     /* "ESeqNo", "sb7"; NULL when absent */
void       lj_window_event(lj_window *win, const lj_ui_event *ev);                   /* mouse/key/char/timer/focus/expose/resize/close */
void       lj_window_paint(lj_window *win);                                          /* repaints dirty rects into the surface; after it lj_window_dirty() lists what changed */
void       lj_window_set_caption(lj_window *win, const char *caption);               /* runtime captions: "SEQUENCE [0000 No Name]" [F19] */

/* Accessors for platform/sdl and lj4evr_render. */
lj_surface         *lj_window_surface(lj_window *win);           /* the whole window image: frame + menu bar + client */
const lj_form_desc *lj_window_desc(const lj_window *win);
lj_widget          *lj_window_root(lj_window *win);              /* the form's client area widget; its x,y is the client origin inside the surface */
const lj_menu_desc *lj_window_menu(const lj_window *win);        /* the form's main menu or NULL */
void                lj_window_client_rect(const lj_window *win, lj_rect *out);   /* client area inside the surface (after frame + menu bar) */
int                 lj_window_dirty(lj_window *win, lj_rect *out, int cap);      /* surface rects changed by the last lj_window_paint(), cleared on read; returns the count, which may exceed cap (then update everything) */
void                lj_window_invalidate_all(lj_window *win);

/* Caption state the frame painter and the platform share (design 7.3, 7.5). */
typedef struct {
    char text[128];                                               /* CP1252 */
    lj_border_style border; unsigned icons;                       /* from the desc, LJ_BI_* bits resolved (default applied) */
    unsigned active:1, minimized:1, maximized:1, native_frame:1;  /* native_frame: [CHROME] Native=1, the platform draws the frame and the surface is client-only */
    int frame_w, caption_h, menu_h;                               /* resolved frame metrics in px: 4/19/19 sizeable, 3/19/19 dialog, tool-window caption 15 */
} lj_window_caption_state;
const lj_window_caption_state *lj_window_caption(const lj_window *win);
void lj_window_set_active(lj_window *win, int active);           /* repaints the caption bar colour */
void lj_window_set_native_frame(lj_window *win, int native);     /* before the first paint */
int  lj_window_resize(lj_window *win, int client_w, int client_h);   /* reallocates the surface, runs anchors/align; clamps to Constraints; 0 / -1 */

/* Opaque slot for the platform's own handle (SDL_Window*); the ui layer never reads it. */
void  lj_window_set_native(lj_window *win, void *handle);
void *lj_window_native(const lj_window *win);

/* ---- W0 frame track additions (lj_window.c, lj_frame.c, lj_menu.c, lj_dnd.c) ----
 * Coordinates: mouse x,y in an lj_ui_event are SURFACE coordinates, what the SDL
 * window reports for a borderless window; under [CHROME] Native=1 the surface is
 * the client plus our menu bar, so they coincide with client coordinates there.
 * LJ_EV_RESIZE carries the new CLIENT size (the platform subtracts the frame). */

/* Widget classes come from the three registries of ui/widgets/ (lj_widgets_basic.h,
 * lj_widgets_lists.h, lj_widgets_lj.h): lj_window_create() fills one
 * lj_widget_ctor table[LJ_K_COUNT] from them and builds a kind with no
 * constructor as the paint-only stub (sunken box with the class name, ME-17).
 * A class that implements vt->mouse / vt->key emits its own OnMouse*, OnClick,
 * OnKey* (lj_w_emit); the window emits them only for widgets without one (the
 * root and the stubs), so nothing fires twice. */

/* Platform hit-test (design 7.5): CAPTION -> SDL_HITTEST_DRAGGABLE, RESIZE_* ->
 * SDL_HITTEST_RESIZE_*, everything else SDL_HITTEST_NORMAL so the click reaches
 * lj_window_event (caption buttons, system-menu icon, menu bar, client). */
typedef enum { LJ_HIT_CLIENT = 0, LJ_HIT_CAPTION, LJ_HIT_MENU, LJ_HIT_SYSMENU, LJ_HIT_BUTTON, LJ_HIT_FRAME,
               LJ_HIT_RESIZE_TL, LJ_HIT_RESIZE_T, LJ_HIT_RESIZE_TR, LJ_HIT_RESIZE_R,
               LJ_HIT_RESIZE_BR, LJ_HIT_RESIZE_B, LJ_HIT_RESIZE_BL, LJ_HIT_RESIZE_L } lj_hit;
lj_hit lj_window_hit(const lj_window *win, int x, int y);   /* surface coordinates */

/* Platform callbacks the ui layer needs for popups (menus, hints), drag and drop
 * and the caption buttons. NULL host (tests, lj4evr_render) = headless: popups
 * are built but not shown, drops stay inside the source window. */
enum lj_sys_cmd { LJ_SYS_RESTORE = 0, LJ_SYS_MOVE, LJ_SYS_SIZE, LJ_SYS_MINIMIZE, LJ_SYS_MAXIMIZE, LJ_SYS_CLOSE };
typedef struct {
    void *ud;
    int   (*screen_pos)(void *ud, const lj_window *w, int *x, int *y);                    /* surface origin on screen; -1 unknown */
    void *(*popup_show)(void *ud, lj_window *owner, const lj_surface *s, int sx, int sy); /* borderless popup window showing s at screen x,y; handle or NULL */
    void  (*popup_update)(void *ud, void *handle, const lj_surface *s);                   /* s was repainted (same size) */
    void  (*popup_hide)(void *ud, void *handle);                                          /* destroys the popup window */
    lj_window *(*window_at)(void *ud, int sx, int sy, int *x, int *y);                    /* our window under a screen point and the point in its surface coords; NULL if none */
    void  (*sys_cmd)(void *ud, lj_window *w, int cmd);                                    /* enum lj_sys_cmd from the caption buttons / system menu */
} lj_window_host;
void lj_window_set_host(lj_window *win, const lj_window_host *host);                 /* the struct must outlive the window */
void lj_window_popup_event(lj_window *owner, void *handle, const lj_ui_event *ev);   /* mouse events on a popup the host showed; x,y relative to that popup */
lj_window *lj_window_of(lj_widget *w);                                                /* the window owning w (its root), NULL for a detached tree */
const lj_menu_item_desc *lj_window_menu_item(const lj_window *win);                  /* the item whose OnClick is being dispatched, else NULL */
struct lj_menu;
struct lj_menu *lj_window_menus(lj_window *win);                                      /* the live menu state (ui/lj_menu.h): lj_menu_set_checked/enabled by item name */
struct lj_dnd;
struct lj_dnd *lj_window_dnd(lj_window *win);                                         /* drag-and-drop state, ui/lj_dnd.h */
lj_window *lj_window_at_screen(lj_window *src, int sx, int sy, int *x, int *y);      /* host->window_at, or src itself (sx,sy taken as its surface coords) when headless */
const lj_mouse_ev *lj_window_last_event(const lj_window *win);                       /* the last mouse event lj_window_event() dispatched (surface coords): a click handler reads the button (W0-last-event, O/S/F right-click) */

#endif /* LJ_UI_WINDOW_H */
