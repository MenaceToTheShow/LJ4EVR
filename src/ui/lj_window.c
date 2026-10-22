/* lj_window.c - one window: builds the widget tree from a generated form table in
 * DFM order (= z-order), binds DFM handler names to behaviour (ME-16), owns the
 * surface, the classic frame (lj_frame.c), the menu (lj_menu.c), focus
 * (lj_focus.c), drag and drop (lj_dnd.c), hints and the 100 ms tick, and
 * dispatches input (design 2.7, 7.5, 7.6, 7.9). */
#include "ui/lj_window.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "ui/lj_dnd.h"
#include "ui/lj_focus.h"
#include "ui/lj_frame.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"

#define LJ_DIRTY_CAP   64
#define LJ_HINT_TICKS  5          /* 500 ms at the 100 ms tick (design 7.9) */
#define LJ_CLR_INFOBK  0xFFFFE1u  /* clInfoBk */

/* Per-widget binding record: (event -> handler) pairs of one DFM row. */
typedef struct { int n; struct { int ev; void (*fn)(lj_widget *w, void *ud); void *ud; } e[LJ_EVENTS_MAX]; } lj_wbind;

struct lj_window {
    lj_widget root;                        /* FIRST: lj_window_of() casts the root back to the window */
    const lj_form_desc *desc;
    lj_surface surf;
    lj_window_caption_state cs;
    lj_menu menu;
    lj_dnd dnd;
    const lj_window_host *host;
    void *native;
    lj_widget_ctor ctor[LJ_K_COUNT];
    lj_wbind *binds; int nbinds;
    lj_rect dirty[LJ_DIRTY_CAP]; int ndirty; unsigned dirty_all:1, frame_dirty:1, painted:1;
    lj_widget *capture, *down_w, *hover; int down_button, hover_ticks, mx, my; int frame_btn;
    lj_surface hint_surf; void *hint_handle; unsigned alt_pending:1, eat_char:1;
    lj_mouse_ev last_ev;                   /* lj_window_last_event */
};

/* ---- fonts ---- */

static const lj_font *font_of(lj_font_id id)
{
    switch (id) {
    case LJ_FONT_SANS13B:      return &lj_font_sans13b;
    case LJ_FONT_ARIAL14:      return &lj_font_arial14;
    case LJ_FONT_ARIAL14B:     return &lj_font_arial14b;
    case LJ_FONT_ARIALBLACK18: return &lj_font_arialblack18;
    case LJ_FONT_INHERIT:      return NULL;
    default:                   return &lj_font_sans13;     /* SANS13 and OTHER (no bitmap for that face: MS Sans Serif stands in) */
    }
}

/* ---- the root and the ME-17 stub widget ---- */

static void root_paint(lj_widget *w, lj_surface *s)
{
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
}
static const lj_widget_vt lj_root_vt = { root_paint, NULL, NULL, NULL };

static void stub_paint(lj_widget *w, lj_surface *s)
{
    int ax, ay, tw;
    const char *name = w->kind > 0 && w->kind < LJ_K_COUNT ? lj_ctl_class_names[w->kind] : "?";
    const lj_font *f = w->font ? w->font : &lj_font_sans13;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, LJ_CLR_BTNFACE);
    tw = lj_text_width(f, name);
    if (w->w > 4 && w->h > 4) lj_text(s, f, ax + (w->w - tw) / 2, ay + (w->h - f->height) / 2, name, LJ_CLR_3DDKSHADOW, 0);
}
static const lj_widget_vt lj_stub_vt = { stub_paint, NULL, NULL, NULL };

static lj_widget *stub_ctor(lj_widget *parent, const lj_ctl_desc *d)
{
    return (lj_widget *)lj_w_alloc(sizeof(lj_widget), parent, d, &lj_stub_vt, 0);
}

/* ---- bindings (ME-16) ---- */

static const lj_binding *find_binding(const lj_binding *b, const char *name)
{
    if (!b || !name) return NULL;
    for (; b->event; b++) if (strcmp(b->event, name) == 0) return b;
    return NULL;
}

static int wev_of(const char *on_name)
{
    int i;
    for (i = 0; i < LJ_WEV_COUNT; i++) if (strcmp(lj_wev_names[i], on_name) == 0) return i;
    for (i = 0; i < LJ_WEV_LISTS_COUNT - LJ_WEV_COUNT; i++) if (strcmp(lj_wev_lists_names[i], on_name) == 0) return LJ_WEV_COUNT + i;
    return -1;
}

static void bound_dispatch(lj_widget *w, int ev, void *ud)
{
    lj_wbind *b = (lj_wbind *)ud;
    int i;
    if (!b) return;
    for (i = 0; i < b->n; i++) if (b->e[i].ev == ev) { void (*fn)(lj_widget *, void *) = b->e[i].fn; void *ud2 = b->e[i].ud; fn(w, ud2); return; }   /* one handler per event name; the handler may destroy its window (warning boxes), so nothing of b is read after the call */
}

typedef struct { const char *missing[64]; int n; int complained; } lj_unbound;

static void unbound(lj_unbound *u, const char *form, const char *handler)
{
    int i;
    for (i = 0; i < u->n; i++) if (strcmp(u->missing[i], handler) == 0) return;
    if (u->n < LJ_COUNT_OF(u->missing)) u->missing[u->n++] = handler;
    lj_log("lj_window: %s: no binding for handler %s", form, handler);
    u->complained = 1;
}

static void bind_events(lj_window *win, lj_wbind *b, const lj_event_desc *events, const lj_binding *bindings, lj_unbound *u)
{
    int i;
    b->n = 0;
    for (i = 0; i < LJ_EVENTS_MAX && events[i].event; i++) {
        const lj_binding *bd;
        int ev = wev_of(events[i].event);
        if (ev < 0) { lj_log("lj_window: %s: unknown event %s", win->desc->class_name, events[i].event); continue; }
        if (!bindings) continue;
        bd = find_binding(bindings, events[i].handler);
        if (!bd) { unbound(u, win->desc->class_name, events[i].handler ? events[i].handler : "?"); continue; }
        b->e[b->n].ev = ev; b->e[b->n].fn = bd->fn; b->e[b->n].ud = bd->ud; b->n++;
    }
}

static int desc_has_handler(const lj_ctl_desc *rows, int n, const char *name)
{
    int i, k;
    for (i = 0; i < n; i++) {
        for (k = 0; k < LJ_EVENTS_MAX && rows[i].events[k].event; k++)
            if (rows[i].events[k].handler && strcmp(rows[i].events[k].handler, name) == 0) return 1;
        if (rows[i].children && desc_has_handler(rows[i].children, rows[i].nchildren, name)) return 1;
    }
    return 0;
}

static int menu_has_handler(const lj_menu_item_desc *items, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++) {
        if (items[i].on_click && strcmp(items[i].on_click, name) == 0) return 1;
        if (items[i].children && menu_has_handler(items[i].children, items[i].nchildren, name)) return 1;
    }
    return 0;
}

/* ---- tree construction ---- */

static int effective_show_hint(const lj_ctl_desc *d, int parent_shows)
{
    if (d->show_hint) return d->show_hint == LJ_YES;
    if (d->parent_show_hint == LJ_NO) return 0;
    return parent_shows;
}

static int build(lj_window *win, lj_widget *parent, const lj_ctl_desc *rows, int n, const lj_binding *bindings, lj_unbound *u, int parent_shows, int *idx)
{
    int i;
    for (i = 0; i < n; i++) {
        const lj_ctl_desc *d = &rows[i];
        lj_widget *w = NULL;
        int nonvisual = d->kind >= LJ_K_MAINMENU, shows;
        if (d->kind > 0 && d->kind < LJ_K_COUNT && win->ctor[d->kind]) w = win->ctor[d->kind](parent, d);   /* lj_w_alloc: base fields from d, added to parent */
        if (!w) w = stub_ctor(parent, d);
        if (w->parent != parent) { lj_log("lj_window: %s: %s constructor did not attach the widget", win->desc->class_name, d->name ? d->name : "?"); free(w); return -1; }
        if (!w->font) w->font = parent->font;
        shows = effective_show_hint(d, parent_shows);
        w->hint = shows ? d->hint : NULL;
        if (nonvisual) { w->visible = 0; w->w = 0; w->h = 0; w->tabstop = 0; }
        w->on_event = bound_dispatch; w->ud = &win->binds[*idx];
        bind_events(win, &win->binds[*idx], d->events, bindings, u);
        (*idx)++;
        if (d->children && build(win, w, d->children, d->nchildren, bindings, u, shows, idx) != 0) return -1;
    }
    return 0;
}

static void free_tree(lj_window *win, lj_widget *w)
{
    lj_widget *c, *next;
    for (c = w->first; c; c = next) { next = c->next; free_tree(win, c); }
    w->first = NULL;
    if (w == &win->root) return;
    free(w);                                   /* every class is one calloc (ui/widgets) */
}

static void sys_fire(lj_widget *root, void *ud)
{
    lj_window *win = (lj_window *)ud;
    const lj_menu_item_desc *it = win->menu.firing;
    (void)root;
    if (it && win->host && win->host->sys_cmd) win->host->sys_cmd(win->host->ud, win, it->tag);
}

static void layout_surface(lj_window *win, int cw, int ch)
{
    int sw, sh;
    lj_frame_metrics(&win->cs);
    sw = cw + 2 * win->cs.frame_w;
    sh = ch + 2 * win->cs.frame_w + win->cs.caption_h + win->cs.menu_h;
    if (win->surf.w != sw || win->surf.h != sh) { lj_surface_free(&win->surf); lj_surface_init(&win->surf, sw, sh); }
    win->root.x = win->cs.frame_w;
    win->root.y = win->cs.frame_w + win->cs.caption_h;
    win->menu.bar_x = win->cs.frame_w; win->menu.bar_y = win->root.y; win->menu.bar_w = cw;
    win->root.y += win->cs.menu_h;
    win->dirty_all = 1;
}

lj_window *lj_window_create(const lj_form_desc *desc, const lj_binding *bindings)
{
    lj_window *win;
    lj_unbound u = { {0}, 0, 0 };
    const lj_menu_desc *bar;
    int idx = 1, i, count;
    if (!desc) return NULL;
    win = (lj_window *)calloc(1, sizeof *win);
    if (!win) lj_fatal("lj_window: out of memory");
    win->desc = desc;
    win->frame_btn = LJ_FBTN_NONE;
    lj_widgets_register_basic(win->ctor, LJ_K_COUNT);
    lj_widgets_register_lists(win->ctor, LJ_K_COUNT);
    lj_widgets_register_lj(win->ctor, LJ_K_COUNT);

    /* root = the client area */
    win->root.vt = &lj_root_vt; win->root.name = desc->name; win->root.visible = 1; win->root.enabled = 1;
    win->root.w = desc->client_width; win->root.h = desc->client_height;
    win->root.color = lj_desc_rgb(desc->color, LJ_CLR_BTNFACE);
    win->root.font = font_of(desc->font) ? font_of(desc->font) : &lj_font_sans13;
    win->root.hint = lj_desc_bool(desc->show_hint, 0) ? desc->hint : NULL;

    /* caption state */
    lj_str_copy(win->cs.text, sizeof win->cs.text, desc->caption ? desc->caption : "");
    win->cs.border = desc->border;
    win->cs.icons = (desc->icons & LJ_BI_GIVEN) ? (desc->icons & 0xFu) : (LJ_BI_SYSMENU | LJ_BI_MINIMIZE | LJ_BI_MAXIMIZE);
    if (desc->border == LJ_BS_NONE) win->cs.icons = 0;
    bar = desc->menu ? lj_form_find_menu(desc, desc->menu) : NULL;
    win->cs.menu_h = bar ? LJ_MENU_BAR_H : 0;

    /* bindings + tree */
    count = lj_form_count_ctls(desc) + 1;
    win->binds = (lj_wbind *)calloc((size_t)count, sizeof *win->binds);
    if (!win->binds) lj_fatal("lj_window: out of memory");
    win->nbinds = count;
    win->root.on_event = bound_dispatch; win->root.ud = &win->binds[0];
    bind_events(win, &win->binds[0], desc->events, bindings, &u);
    if (build(win, &win->root, desc->ctls, desc->n, bindings, &u, lj_desc_bool(desc->show_hint, 0), &idx) != 0) {
        lj_log("lj_window: %s: tree build failed", desc->class_name);
        lj_window_destroy(win);
        return NULL;
    }

    /* menus: the form's main menu + the classic system menu, OnClick names bound by handler name */
    lj_menu_init(&win->menu, win, desc, bar, &lj_frame_sysmenu);
    for (i = 0; i < win->menu.nrt; i++) {
        lj_menu_rt *rt = &win->menu.rt[i];
        const lj_binding *bd;
        if (rt->item >= lj_frame_sysmenu.items && rt->item < lj_frame_sysmenu.items + lj_frame_sysmenu.n) { rt->fn = sys_fire; rt->ud = win; continue; }
        if (!rt->item->on_click || !bindings) continue;
        bd = find_binding(bindings, rt->item->on_click);
        if (!bd) { unbound(&u, desc->class_name, rt->item->on_click); continue; }
        rt->fn = bd->fn; rt->ud = bd->ud;
    }
    /* ME-16, the other direction: a binding that names nothing in the form */
    for (i = 0; bindings && bindings[i].event; i++) {
        const char *name = bindings[i].event;
        int found = desc_has_handler(desc->ctls, desc->n, name), k;
        for (k = 0; !found && k < LJ_EVENTS_MAX && desc->events[k].event; k++) found = desc->events[k].handler && strcmp(desc->events[k].handler, name) == 0;
        for (k = 0; !found && k < desc->nmenus; k++) found = menu_has_handler(desc->menus[k].items, desc->menus[k].n, name);
        if (!found) { lj_log("lj_window: %s: binding %s matches no DFM handler", desc->class_name, name); u.complained = 1; }
    }
    assert(!u.complained && "ME-16: every DFM handler bound and every binding used");

    layout_surface(win, desc->client_width, desc->client_height);
    lj_widget_layout_hooks(&win->root);
    bound_dispatch(&win->root, LJ_WEV_CREATE, win->root.ud);
    return win;
}

void lj_window_destroy(lj_window *win)
{
    if (!win) return;
    bound_dispatch(&win->root, LJ_WEV_DESTROY, win->root.ud);
    if (win->hint_handle && win->host && win->host->popup_hide) win->host->popup_hide(win->host->ud, win->hint_handle);
    lj_surface_free(&win->hint_surf);
    lj_menu_free(&win->menu);
    free_tree(win, &win->root);
    free(win->binds);
    lj_surface_free(&win->surf);
    free(win);
}

/* ---- accessors ---- */

lj_widget *lj_window_find(lj_window *win, const char *dfm_name) { return win ? lj_widget_find(&win->root, dfm_name) : NULL; }
lj_surface *lj_window_surface(lj_window *win) { return win ? &win->surf : NULL; }
const lj_form_desc *lj_window_desc(const lj_window *win) { return win ? win->desc : NULL; }
lj_widget *lj_window_root(lj_window *win) { return win ? &win->root : NULL; }
const lj_menu_desc *lj_window_menu(const lj_window *win) { return win ? win->menu.bar : NULL; }
const lj_window_caption_state *lj_window_caption(const lj_window *win) { return win ? &win->cs : NULL; }
void lj_window_set_native(lj_window *win, void *handle) { if (win) win->native = handle; }
void *lj_window_native(const lj_window *win) { return win ? win->native : NULL; }
void lj_window_set_host(lj_window *win, const lj_window_host *host) { if (win) { win->host = host; win->menu.host = host; } }
const lj_menu_item_desc *lj_window_menu_item(const lj_window *win) { return win ? win->menu.firing : NULL; }
struct lj_dnd *lj_window_dnd(lj_window *win) { return win ? &win->dnd : NULL; }
struct lj_menu *lj_window_menus(lj_window *win) { return win ? &win->menu : NULL; }
const lj_mouse_ev *lj_window_last_event(const lj_window *win) { return win ? &win->last_ev : NULL; }

lj_window *lj_window_of(lj_widget *w)
{
    if (!w) return NULL;
    while (w->parent) w = w->parent;
    return w->vt == &lj_root_vt ? (lj_window *)w : NULL;
}

void lj_window_client_rect(const lj_window *win, lj_rect *out)
{
    if (!win || !out) return;
    out->x = win->root.x; out->y = win->root.y; out->w = win->root.w; out->h = win->root.h;
}

void lj_window_invalidate_all(lj_window *win) { if (win) win->dirty_all = 1; }

int lj_window_dirty(lj_window *win, lj_rect *out, int cap)
{
    int n, i;
    if (!win) return 0;
    n = win->ndirty;
    for (i = 0; out && i < n && i < cap; i++) out[i] = win->dirty[i];
    win->ndirty = 0;
    return n;
}

void lj_window_set_caption(lj_window *win, const char *caption)
{
    if (!win) return;
    lj_str_copy(win->cs.text, sizeof win->cs.text, caption ? caption : "");
    win->frame_dirty = 1;
}

void lj_window_set_active(lj_window *win, int active)
{
    if (!win || win->cs.active == (active ? 1u : 0u)) return;
    win->cs.active = active ? 1u : 0u;
    win->frame_dirty = 1;
}

void lj_window_set_native_frame(lj_window *win, int native)
{
    if (!win) return;
    win->cs.native_frame = native ? 1u : 0u;
    layout_surface(win, win->root.w, win->root.h);
}

int lj_window_resize(lj_window *win, int client_w, int client_h)
{
    const lj_form_desc *d;
    int extra_w, extra_h;
    if (!win || client_w < 0 || client_h < 0) return -1;
    d = win->desc;
    extra_w = 2 * win->cs.frame_w; extra_h = 2 * win->cs.frame_w + win->cs.caption_h + win->cs.menu_h;
    if (d->min_width  > 0 && client_w + extra_w < d->min_width)  client_w = d->min_width - extra_w;
    if (d->max_width  > 0 && client_w + extra_w > d->max_width)  client_w = d->max_width - extra_w;
    if (d->min_height > 0 && client_h + extra_h < d->min_height) client_h = d->min_height - extra_h;
    if (d->max_height > 0 && client_h + extra_h > d->max_height) client_h = d->max_height - extra_h;
    if (client_w < 0) client_w = 0;
    if (client_h < 0) client_h = 0;
    if (client_w == win->root.w && client_h == win->root.h) return 0;   /* the align pass runs only on a client-size change (ME-41) */
    layout_surface(win, client_w, client_h);
    lj_widget_resize(&win->root, client_w, client_h);
    return 0;
}

lj_hit lj_window_hit(const lj_window *win, int x, int y)
{
    if (!win) return LJ_HIT_CLIENT;
    return lj_frame_hit(&win->cs, win->surf.w, win->surf.h, x, y);
}

lj_window *lj_window_at_screen(lj_window *src, int sx, int sy, int *x, int *y)
{
    if (src && src->host && src->host->window_at) return src->host->window_at(src->host->ud, sx, sy, x, y);
    if (x) *x = sx;
    if (y) *y = sy;
    return src;
}

static void screen_of(lj_window *win, int x, int y, int *sx, int *sy)
{
    int ox = 0, oy = 0;
    if (win->host && win->host->screen_pos) win->host->screen_pos(win->host->ud, win, &ox, &oy);
    *sx = ox + x; *sy = oy + y;
}

/* ---- hints ---- */

static void hint_hide(lj_window *win)
{
    if (win->hint_handle && win->host && win->host->popup_hide) win->host->popup_hide(win->host->ud, win->hint_handle);
    win->hint_handle = NULL;
    lj_surface_free(&win->hint_surf);
    win->hover_ticks = 0;
}

static void hint_show(lj_window *win)
{
    char text[256];
    const lj_font *f = &lj_font_sans13;
    int tw, sx, sy;
    char *bar;
    if (!win->hover || !win->hover->hint || win->hint_surf.px) return;
    lj_str_copy(text, sizeof text, win->hover->hint);
    if ((bar = strchr(text, '|')) != NULL) *bar = 0;      /* GetShortHint */
    if (!text[0]) return;
    tw = lj_text_width(f, text);
    if (lj_surface_init(&win->hint_surf, tw + 6, f->height + 4) != 0) return;
    lj_fill(&win->hint_surf, 0, 0, win->hint_surf.w, win->hint_surf.h, LJ_CLR_INFOBK);
    lj_bevel1(&win->hint_surf, 0, 0, win->hint_surf.w, win->hint_surf.h, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW);
    lj_text(&win->hint_surf, f, 3, 2, text, LJ_CLR_3DDKSHADOW, 0);
    screen_of(win, win->mx, win->my + 20, &sx, &sy);
    if (win->host && win->host->popup_show) win->hint_handle = win->host->popup_show(win->host->ud, win, &win->hint_surf, sx, sy);
}

/* ---- events ---- */

static void fire(lj_widget *w, int ev) { if (w && w->on_event) w->on_event(w, ev, w->ud); }

static lj_widget *widget_at(lj_window *win, int x, int y) { return lj_widget_hit(&win->root, x - win->root.x, y - win->root.y); }

static const lj_menu_desc *popup_for(lj_window *win, lj_widget *w, lj_widget **owner)
{
    for (; w && w != &win->root; w = w->parent)
        if (w->desc && w->desc->popup_menu) { *owner = lj_window_find(win, w->desc->popup_menu); return lj_form_find_menu(win->desc, w->desc->popup_menu); }
    if (win->desc->popup_menu) { *owner = lj_window_find(win, win->desc->popup_menu); return lj_form_find_menu(win->desc, win->desc->popup_menu); }
    *owner = NULL;
    return NULL;
}

static void open_sysmenu(lj_window *win)
{
    int sx, sy;
    const lj_window_caption_state *cs = &win->cs;
    int sizeable = cs->border == LJ_BS_SIZEABLE || cs->border == LJ_BS_SIZETOOLWIN;
    lj_menu_set_enabled(&win->menu, "SysRestore", cs->maximized || cs->minimized);
    lj_menu_set_enabled(&win->menu, "SysMove", !cs->maximized);
    lj_menu_set_enabled(&win->menu, "SysSize", sizeable && !cs->maximized);
    lj_menu_set_enabled(&win->menu, "SysMinimize", (cs->icons & LJ_BI_MINIMIZE) != 0);
    lj_menu_set_enabled(&win->menu, "SysMaximize", (cs->icons & LJ_BI_MAXIMIZE) != 0 && !cs->maximized);
    screen_of(win, cs->frame_w, cs->frame_w + cs->caption_h, &sx, &sy);
    lj_menu_popup(&win->menu, &lj_frame_sysmenu, sx, sy);
}

static void mouse(lj_window *win, const lj_mouse_ev *m)
{
    lj_hit hit;
    lj_widget *target;
    lj_mouse_ev rel = *m;
    int ax, ay, plain = 0, sx = 0, sy = 0;
    win->mx = m->x; win->my = m->y; win->last_ev = *m;
    if (m->type != LJ_EV_MOUSE_MOVE) hint_hide(win);
    if (m->type == LJ_EV_MOUSE_LEAVE) { win->hover = NULL; hint_hide(win); return; }

    if (lj_menu_is_open(&win->menu)) {
        if (lj_menu_bar_item_at(&win->menu, m->x, m->y) >= 0 || m->type == LJ_EV_MOUSE_MOVE) { lj_menu_bar_mouse(&win->menu, m); return; }
        if (m->type == LJ_EV_MOUSE_DOWN) { lj_menu_close(&win->menu); return; }
        return;
    }
    if (m->type == LJ_EV_MOUSE_UP && win->frame_btn != LJ_FBTN_NONE) {   /* released over the pressed caption button? */
        int b = win->frame_btn;
        win->frame_btn = LJ_FBTN_NONE; win->frame_dirty = 1;
        if (lj_frame_button_at(&win->cs, win->surf.w, m->x, m->y) == b && win->host && win->host->sys_cmd)
            win->host->sys_cmd(win->host->ud, win, b == LJ_FBTN_CLOSE ? LJ_SYS_CLOSE : b == LJ_FBTN_MIN ? LJ_SYS_MINIMIZE : win->cs.maximized ? LJ_SYS_RESTORE : LJ_SYS_MAXIMIZE);
        return;
    }
    hit = win->capture ? LJ_HIT_CLIENT : lj_window_hit(win, m->x, m->y);
    switch (hit) {
    case LJ_HIT_BUTTON:
        if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) { win->frame_btn = lj_frame_button_at(&win->cs, win->surf.w, m->x, m->y); win->frame_dirty = 1; }
        return;
    case LJ_HIT_SYSMENU:
        if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) open_sysmenu(win);
        return;
    case LJ_HIT_CAPTION:
        if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1 && m->clicks == 2 && (win->cs.border == LJ_BS_SIZEABLE || win->cs.border == LJ_BS_SIZETOOLWIN)
            && (win->cs.icons & LJ_BI_MAXIMIZE) && win->host && win->host->sys_cmd)
            win->host->sys_cmd(win->host->ud, win, win->cs.maximized ? LJ_SYS_RESTORE : LJ_SYS_MAXIMIZE);
        return;
    case LJ_HIT_MENU:
        lj_menu_bar_mouse(&win->menu, m);
        return;
    case LJ_HIT_FRAME: case LJ_HIT_RESIZE_TL: case LJ_HIT_RESIZE_T: case LJ_HIT_RESIZE_TR: case LJ_HIT_RESIZE_R:
    case LJ_HIT_RESIZE_BR: case LJ_HIT_RESIZE_B: case LJ_HIT_RESIZE_BL: case LJ_HIT_RESIZE_L:
        return;
    case LJ_HIT_CLIENT:
    default:
        break;
    }

    target = win->capture ? win->capture : widget_at(win, m->x, m->y);
    if (m->type == LJ_EV_MOUSE_MOVE && !win->capture) {
        if (target != win->hover) { win->hover = target; hint_hide(win); }
    }
    if (win->dnd.active) {
        screen_of(win, m->x, m->y, &sx, &sy);
        if (m->type == LJ_EV_MOUSE_MOVE) lj_dnd_move(win, sx, sy);
        else if (m->type == LJ_EV_MOUSE_UP) lj_dnd_end(win, sx, sy, 1);
    }
    if (!target) return;
    lj_widget_abs(target, &ax, &ay);
    rel.x = m->x - ax; rel.y = m->y - ay;
    if (m->type == LJ_EV_MOUSE_DOWN) {
        win->capture = target; win->down_w = target; win->down_button = m->button;
        if (target->tabstop && target->enabled && target->visible) lj_focus_set(target);
    }
    if (m->type == LJ_EV_MOUSE_WHEEL && target->enabled && m->wheel) fire(target, m->wheel < 0 ? LJ_WEV_MOUSE_WHEEL_DOWN : LJ_WEV_MOUSE_WHEEL_UP);   /* ME-74: the VCL's OnMouseWheelDown/Up, before the widget's own wheel handling */
    if (target->enabled && target->vt && target->vt->mouse) target->vt->mouse(target, &rel);
    else if (target->enabled) plain = 1;   /* root / stub: the window emits the OnMouse* events itself */
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (plain) fire(target, m->clicks == 2 && m->button == 1 ? LJ_WEV_DBLCLICK : LJ_WEV_MOUSE_DOWN);
        break;
    case LJ_EV_MOUSE_UP: {
        lj_widget *under = widget_at(win, m->x, m->y);
        win->capture = NULL;
        if (plain) fire(target, LJ_WEV_MOUSE_UP);
        if (under == target && win->down_w == target) {
            if (m->button == 1 && plain) fire(target, LJ_WEV_CLICK);
            if (m->button == 2) {
                lj_widget *owner;
                const lj_menu_desc *pm = popup_for(win, target, &owner);
                if (pm) { fire(owner, LJ_WEV_POPUP); screen_of(win, m->x, m->y, &sx, &sy); lj_menu_popup(&win->menu, pm, sx, sy); }
            }
        }
        win->down_w = NULL;
        break; }
    case LJ_EV_MOUSE_MOVE:  if (plain) fire(target, LJ_WEV_MOUSE_MOVE); break;
    case LJ_EV_MOUSE_WHEEL: if (plain) fire(target, LJ_WEV_MOUSE_WHEEL); break;
    default: break;
    }
}

static void key(lj_window *win, const lj_key_ev *k)
{
    lj_widget *focused = lj_widget_focused(&win->root), *next;
    int consumed = 0, has_key;
    if (k->type == LJ_EV_KEY_DOWN) { hint_hide(win); win->eat_char = 0; }
    if (k->type == LJ_EV_CHAR && win->eat_char) { win->eat_char = 0; return; }   /* the text event of a key the menu took */
    if (lj_menu_is_open(&win->menu)) { lj_menu_key(&win->menu, k); win->eat_char = k->type == LJ_EV_KEY_DOWN; return; }
    has_key = focused && focused->enabled && focused->vt && focused->vt->key;
    if (k->type == LJ_EV_KEY_UP) {
        if (k->key == 0x12 && win->alt_pending) { win->alt_pending = 0; lj_menu_activate_bar(&win->menu); return; }
        if (has_key) focused->vt->key(focused, k); else fire(focused, LJ_WEV_KEY_UP);
        return;
    }
    if (k->type == LJ_EV_CHAR) {
        if (has_key) focused->vt->key(focused, k); else fire(focused, LJ_WEV_KEY_PRESS);
        return;
    }
    /* key down */
    if (k->key == 0x12) { win->alt_pending = 1; return; }        /* Alt alone: the bar activates on release */
    win->alt_pending = 0;
    if (k->alt && !k->ctrl && k->key >= 'A' && k->key <= 'Z') {
        if (lj_menu_alt_letter(&win->menu, k->key) || lj_w_dialog_char(&win->root, k->key + 32)) { win->eat_char = 1; return; }
    }
    if (k->key == 0x79 && !k->shift && !k->ctrl && !k->alt) { lj_menu_activate_bar(&win->menu); return; }   /* F10 */
    if (lj_menu_shortcut(&win->menu, k)) { win->eat_char = 1; return; }
    if (lj_desc_bool(win->desc->key_preview, 0)) fire(&win->root, LJ_WEV_KEY_DOWN);
    if (has_key) consumed = focused->vt->key(focused, k);
    if (!consumed) {
        switch (k->key) {
        case 0x09:   /* Tab / Shift+Tab */
            next = lj_focus_next(&win->root, focused, k->shift);
            if (next) lj_focus_set(next);
            return;
        case 0x0D: lj_focus_click(lj_focus_default(&win->root)); break;
        case 0x1B: lj_focus_click(lj_focus_cancel(&win->root)); break;
        default: break;
        }
    }
    if (!has_key) fire(focused, LJ_WEV_KEY_DOWN);
}

static lj_widget *timer_widget(lj_widget *w, int id)
{
    lj_widget *c, *hit;
    if (w->kind == LJ_K_TIMER && w->desc && w->desc->tag == id) return w;
    for (c = w->first; c; c = c->next) if ((hit = timer_widget(c, id)) != NULL) return hit;
    return NULL;
}

void lj_window_event(lj_window *win, const lj_ui_event *ev)
{
    if (!win || !ev) return;
    switch (ev->type) {
    case LJ_EV_MOUSE_DOWN: case LJ_EV_MOUSE_UP: case LJ_EV_MOUSE_MOVE: case LJ_EV_MOUSE_WHEEL: case LJ_EV_MOUSE_LEAVE:
        mouse(win, &ev->u.mouse);
        break;
    case LJ_EV_KEY_DOWN: case LJ_EV_KEY_UP: case LJ_EV_CHAR:
        key(win, &ev->u.key);
        break;
    case LJ_EV_TIMER:
        if (ev->u.timer.id == 0) {
            if (win->hover && win->hover->hint && !win->hint_handle && !win->hint_surf.px && ++win->hover_ticks >= LJ_HINT_TICKS) hint_show(win);
        } else fire(timer_widget(&win->root, ev->u.timer.id), LJ_WEV_TIMER);
        break;
    case LJ_EV_FOCUS_IN:
        lj_window_set_active(win, 1);
        fire(&win->root, LJ_WEV_ACTIVATE);
        break;
    case LJ_EV_FOCUS_OUT:
        lj_window_set_active(win, 0);
        lj_menu_close(&win->menu);
        hint_hide(win);
        win->alt_pending = 0;
        fire(&win->root, LJ_WEV_DEACTIVATE);
        break;
    case LJ_EV_EXPOSE:
        win->dirty_all = 1;
        break;
    case LJ_EV_RESIZE:   /* u.size = the new client size; a no-op when the platform already applied it */
        if (ev->u.size.w != win->root.w || ev->u.size.h != win->root.h) lj_window_resize(win, ev->u.size.w, ev->u.size.h);
        fire(&win->root, LJ_WEV_RESIZE);
        break;
    case LJ_EV_CLOSE:
        fire(&win->root, LJ_WEV_CLOSE);
        break;
    default:
        break;
    }
}

void lj_window_popup_event(lj_window *owner, void *handle, const lj_ui_event *ev)
{
    if (!owner || !ev || !handle) return;
    if (handle == owner->hint_handle) { if (ev->type == LJ_EV_MOUSE_DOWN) hint_hide(owner); return; }
    if (ev->type >= LJ_EV_MOUSE_DOWN && ev->type <= LJ_EV_MOUSE_LEAVE) lj_menu_popup_mouse(&owner->menu, handle, &ev->u.mouse);
}

/* ---- painting ---- */

static void paint_tree(lj_widget *w, lj_surface *s)
{
    lj_widget *c;
    if (!w->visible) return;
    if (w->vt && w->vt->paint) w->vt->paint(w, s);
    for (c = w->first; c; c = c->next) paint_tree(c, s);
}

void lj_window_paint(lj_window *win)
{
    int n;
    if (!win) return;
    n = lj_widget_collect_dirty(&win->root, win->dirty, LJ_DIRTY_CAP);
    if (win->menu.dirty && n < LJ_DIRTY_CAP) { win->dirty[n].x = win->menu.bar_x; win->dirty[n].y = win->menu.bar_y; win->dirty[n].w = win->menu.bar_w; win->dirty[n].h = LJ_MENU_BAR_H; }
    if (win->menu.dirty) n++;
    if (win->dirty_all || win->frame_dirty || !win->painted || n > LJ_DIRTY_CAP) {
        win->dirty[0].x = 0; win->dirty[0].y = 0; win->dirty[0].w = win->surf.w; win->dirty[0].h = win->surf.h;
        n = 1;
    }
    win->ndirty = n;
    if (n == 0) return;
    /* ponytail: any dirt repaints the whole window in z-order (a covered widget
     * must be overpainted by what covers it); per-rect clipping if 100 ms ticks ever show up in a profile */
    win->dirty_all = 0; win->frame_dirty = 0; win->painted = 1;
    lj_frame_paint(&win->surf, &win->cs, win->frame_btn);
    if (win->cs.menu_h) lj_menu_bar_paint(&win->menu, &win->surf);
    paint_tree(&win->root, &win->surf);
}
