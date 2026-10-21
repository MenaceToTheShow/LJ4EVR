/* lj_desktop_tabs.c - tab strip + icon area (lj_desktop_tabs.h). */
#include "desktop/lj_desktop_tabs.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_fixture_icons.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_selection.h"
#include "desktop/lj_unassigned.h"
#include "gfx/lj_draw.h"
#include "model/lj_show.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_fixicon.h"

typedef struct { lj_widget base; int band, bx0, by0, bx1, by1; } lj_area;   /* band: rubber band in progress (coords relative to the area) */

static lj_widget   *lj_tabs;
static lj_area     *lj_the_area;
static lj_widget_vt lj_tabs_vt; static void (*lj_tabs_layout_orig)(lj_widget *);
static char         lj_tab_name[LJ_TABS_MAX][64];
static const char  *lj_tab_ptr[LJ_TABS_MAX];
static int          lj_ntabs, lj_cur_tab;
static int          lj_move_all, lj_show_prog, lj_popup_fixture = -1;
static uint8_t      lj_icon_move[LJ_FIXTURES];
static const lj_surface *lj_wall; static int lj_wall_stretch, lj_wall_center;

static const lj_ctl_desc lj_tabs_desc = { LJ_K_TABCONTROL, "desktop_tabs", 0, 0, 791, 400, .align = LJ_AL_CLIENT, .font = LJ_FONT_ARIAL14, .tabstop = LJ_NO, .events = { { "OnChange", "desktop_tabs_change" } } };
static const lj_ctl_desc lj_area_desc = { LJ_K_UNKNOWN, "desktop", 0, 0, 100, 100, .font = LJ_FONT_ARIAL14 };

/* ---- wallpaper + band paint ---- */
static void area_paint(lj_widget *w, lj_surface *s)
{
    lj_area *a = (lj_area *)w;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    if (lj_wall && lj_wall->px) {
        if (lj_wall_stretch) {                        /* nearest-neighbour stretch to the area [F13]; integer only */
            int x, y;
            for (y = 0; y < w->h; y++)
                for (x = 0; x < w->w; x++)
                    lj_fill(s, ax + x, ay + y, 1, 1, lj_wall->px[(y * lj_wall->h / w->h) * lj_wall->w + x * lj_wall->w / w->w]);   /* ponytail: per-pixel fill; a row blit if large wallpapers crawl */
        } else {
            int ox = lj_wall_center ? (w->w - lj_wall->w) / 2 : 0, oy = lj_wall_center ? (w->h - lj_wall->h) / 2 : 0;
            lj_blit_rgb(s, ax + ox, ay + oy, lj_wall, 0);
        }
    }
    if (a->band) {                                    /* "drag a dotted square" [01 s3.2]: the classic focus-rect dots */
        int x0 = a->bx0 < a->bx1 ? a->bx0 : a->bx1, y0 = a->by0 < a->by1 ? a->by0 : a->by1;
        int bw = a->bx0 < a->bx1 ? a->bx1 - a->bx0 : a->bx0 - a->bx1, bh = a->by0 < a->by1 ? a->by1 - a->by0 : a->by0 - a->by1;
        if (bw > 1 && bh > 1) lj_focus_rect(s, ax + x0, ay + y0, bw, bh);
    }
}

static int area_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_area *a = (lj_area *)w;
    int ax, ay;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button == 1) { a->band = 1; a->bx0 = a->bx1 = m->x; a->by0 = a->by1 = m->y; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (a->band) { a->bx1 = m->x; a->by1 = m->y; lj_widget_invalidate(w); }
        return 1;
    case LJ_EV_MOUSE_UP:
        if (m->button == 1 && a->band) {
            a->band = 0;
            lj_sel_band(a->bx0, a->by0, m->x, m->y, m->ctrl);
            lj_widget_invalidate(w);
        } else if (m->button == 2 && m->x >= 0 && m->y >= 0 && m->x < w->w && m->y < w->h) {
            lj_widget_abs(w, &ax, &ay);
            lj_desktop_popup(LJ_POP_DESKTOP, ax + m->x, ay + m->y, -1);
        }
        return 1;
    case LJ_EV_MOUSE_LEAVE:
        return 1;
    default:
        return 0;
    }
}

static void area_event(lj_widget *w, int ev, void *ud)   /* drop target of the Unassigned list [01 s3.7] */
{
    int kind, number, x, y;
    (void)ud;
    if (ev != LJ_WEV_DRAG_DROP) return;
    if (lj_dnd_info(lj_window_of(w), &kind, &number, &x, &y) != 0 || kind != LJ_DND_FIXTURE) return;
    if (lj_desktop_place_icon(number, x - LJ_FIXICON_W / 2, y - LJ_FIXICON_H / 2) == 0) lj_unassigned_refresh();
}

static const lj_widget_vt lj_area_vt = { area_paint, area_mouse, NULL, NULL };

/* ---- tabs ---- */
static void tabs_layout(lj_widget *w)
{
    lj_w_tabs *t = (lj_w_tabs *)w;
    if (lj_tabs_layout_orig) lj_tabs_layout_orig(w);
    if (!lj_the_area) return;
    lj_the_area->base.x = LJ_TAB_PAD; lj_the_area->base.y = t->tab_h + LJ_TAB_PAD;   /* inside the page frame, as a TTabSheet sits (ME-43) */
    lj_the_area->base.w = w->w - 2 * LJ_TAB_PAD; lj_the_area->base.h = w->h - t->tab_h - 2 * LJ_TAB_PAD;
    if (lj_the_area->base.w < 0) lj_the_area->base.w = 0;
    if (lj_the_area->base.h < 0) lj_the_area->base.h = 0;
}

static void tabs_event(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev == LJ_WEV_CHANGE) { lj_cur_tab = ((lj_w_tabs *)w)->active; lj_desktop_icons_refresh(); }
}

static void tabs_load(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP];
    lj_ini *ini = NULL;
    int i;
    lj_ntabs = 0;
    if (ctx->lib && lj_library_path(ctx->lib, "Tabs.Ini", path, sizeof path) == 0) ini = lj_ini_load(path);
    if (ini) {
        int n = lj_ini_geti(ini, "TABS", "Count", 1);
        for (i = 0; i < n && i < LJ_TABS_MAX; i++) {
            char key[16];
            snprintf(key, sizeof key, "TAB%d", i);
            lj_str_copy(lj_tab_name[i], sizeof lj_tab_name[i], lj_ini_gets(ini, "TABNAMES", key, ""));
            if (lj_tab_name[i][0]) lj_ntabs++;
        }
        lj_ini_free(ini);
    }
    if (lj_ntabs == 0) { lj_str_copy(lj_tab_name[0], sizeof lj_tab_name[0], "Default"); lj_ntabs = 1; }   /* the shipped library: [TABS] Count=1 [TABNAMES] TAB0=Default */
    for (i = 0; i < lj_ntabs; i++) lj_tab_ptr[i] = lj_tab_name[i];
}

void lj_desktop_tabs_build(lj_widget *root)
{
    lj_w_tabs *t;
    tabs_load();
    lj_tabs = lj_w_tabcontrol_create(root, &lj_tabs_desc);
    t = (lj_w_tabs *)lj_tabs;
    t->tabs = lj_tab_ptr; t->ntabs = lj_ntabs; t->active = 0;
    lj_tabs_vt = *lj_tabs->vt; lj_tabs_layout_orig = lj_tabs->vt->layout; lj_tabs_vt.layout = tabs_layout;
    lj_tabs->vt = &lj_tabs_vt;
    lj_tabs->on_event = tabs_event;
    lj_the_area = lj_w_alloc(sizeof *lj_the_area, lj_tabs, &lj_area_desc, &lj_area_vt, 0);
    lj_the_area->base.on_event = area_event;
    lj_cur_tab = 0;
    lj_fixture_icons_build(&lj_the_area->base);
}

lj_widget *lj_desktop_icon_area(void) { return lj_the_area ? &lj_the_area->base : NULL; }
int  lj_desktop_tab(void) { return lj_cur_tab; }
int  lj_desktop_tab_count(void) { return lj_ntabs; }
void lj_desktop_tab_set(int tab)
{
    if (tab < 0 || tab >= lj_ntabs || !lj_tabs) return;
    lj_cur_tab = tab;
    lj_w_tabs_select(lj_tabs, tab);   /* fires OnChange -> tabs_event */
    lj_desktop_icons_refresh();
}

void lj_desktop_set_wallpaper(const lj_surface *img, int stretch, int center)
{
    lj_wall = img; lj_wall_stretch = stretch; lj_wall_center = center;
    if (lj_the_area) lj_widget_invalidate(&lj_the_area->base);
}

/* ---- placement ---- */
void lj_desktop_config_save(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    if (ctx->show && ctx->lib && lj_show_save_config(ctx->show, ctx->lib) != 0) lj_log("desktop: Config.dat not saved");
}

int lj_desktop_place_icon(int fixture, int x, int y)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_fixture *f;
    if (!ctx->show || fixture < 0 || fixture >= LJ_FIXTURES || !ctx->show->fixture[fixture].link) return -1;
    f = &ctx->show->fixture[fixture];
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    f->placed = 1; f->tab = (uint8_t)lj_cur_tab; f->icon_x = (int16_t)x; f->icon_y = (int16_t)y;
    lj_desktop_icons_refresh();
    lj_desktop_config_save();
    return 0;
}

void lj_desktop_unassign_icon(int fixture)
{
    lj_ctx *ctx = lj_ctx_get();
    if (!ctx->show || fixture < 0 || fixture >= LJ_FIXTURES) return;
    ctx->show->fixture[fixture].placed = 0;
    lj_sel_set(fixture, 0, 0);                      /* "A fixture is not programmable until it has been placed on the desktop" */
    lj_desktop_icons_refresh();
    lj_desktop_config_save();
    lj_unassigned_refresh();
}

void lj_desktop_unassign_tab(int selected_only)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &ctx->show->fixture[i];
        if (f->link && f->placed && f->tab == lj_cur_tab && (!selected_only || lj_sel_is_selected(i))) lj_desktop_unassign_icon(i);
    }
}

void lj_desktop_auto_place(int by_type)
{
    lj_ctx *ctx = lj_ctx_get();
    int order[LJ_FIXTURES], n = 0, i, j, cols, k = 0, placed_before = 0;
    if (!ctx->show) return;
    for (i = 0; i < LJ_FIXTURES; i++) {
        if (!ctx->show->fixture[i].link) continue;
        if (ctx->show->fixture[i].placed) { if (ctx->show->fixture[i].tab == lj_cur_tab) placed_before++; continue; }
        order[n++] = i;
    }
    if (by_type)   /* insertion sort by profile id, then number */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && ctx->show->fixture[order[j - 1]].profile_id > ctx->show->fixture[order[j]].profile_id; j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
    cols = lj_the_area && lj_the_area->base.w >= LJ_ICON_PITCH ? lj_the_area->base.w / LJ_ICON_PITCH : 8;
    for (i = 0; i < n; i++, k++) {
        int slot = placed_before + k;
        lj_fixture *f = &ctx->show->fixture[order[i]];
        f->placed = 1; f->tab = (uint8_t)lj_cur_tab;
        f->icon_x = (int16_t)(4 + (slot % cols) * LJ_ICON_PITCH); f->icon_y = (int16_t)(4 + (slot / cols) * LJ_ICON_PITCH);
    }
    lj_desktop_icons_refresh();
    lj_desktop_config_save();
    lj_unassigned_refresh();
}

void lj_desktop_align_icons(int to, const char *how)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_rect r, t;
    int i;
    if (!ctx->show || to < 0 || to >= LJ_FIXTURES) return;
    lj_desktop_icon_rect(to, &t);
    if (t.w <= 0) return;
    for (i = 0; i < LJ_FIXTURES; i++) {
        lj_fixture *f = &ctx->show->fixture[i];
        if (i == to || !lj_sel_is_selected(i) || !f->placed || lj_icon_move[i]) continue;   /* "alignment will not work on icons that are in 'move' mode" */
        lj_desktop_icon_rect(i, &r);
        if (r.w <= 0) continue;
        if (strcmp(how, "Top") == 0) f->icon_y = (int16_t)t.y;
        else if (strcmp(how, "HCenter") == 0) f->icon_x = (int16_t)(t.x + (t.w - r.w) / 2);
        else if (strcmp(how, "Bottom") == 0) f->icon_y = (int16_t)(t.y + t.h - r.h);
        else if (strcmp(how, "Left") == 0) f->icon_x = (int16_t)t.x;
        else if (strcmp(how, "VCenter") == 0) f->icon_y = (int16_t)(t.y + (t.h - r.h) / 2);
        else if (strcmp(how, "Right") == 0) f->icon_x = (int16_t)(t.x + t.w - r.w);
    }
    lj_desktop_icons_refresh();
    lj_desktop_config_save();
}

/* ---- modes + popups ---- */
void lj_desktop_move_mode(int on) { lj_move_all = on != 0; lj_desktop_icons_refresh(); }
int  lj_desktop_move_mode_on(void) { return lj_move_all; }
void lj_desktop_icon_move(int fixture, int on) { if (fixture >= 0 && fixture < LJ_FIXTURES) { lj_icon_move[fixture] = (uint8_t)(on != 0); lj_desktop_icons_refresh(); } }
int  lj_desktop_icon_move_on(int fixture) { return fixture >= 0 && fixture < LJ_FIXTURES ? lj_icon_move[fixture] : 0; }
void lj_desktop_show_programmed(int on) { lj_show_prog = on != 0; lj_desktop_icons_refresh(); }
int  lj_desktop_show_programmed_on(void) { return lj_show_prog; }
int  lj_desktop_popup_fixture(void) { return lj_popup_fixture; }

void lj_desktop_popup(enum lj_desktop_popup which, int x, int y, int fixture)   /* x,y = client coordinates of the main window */
{
    lj_window *win = lj_desktop_window();
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    const lj_menu_desc *d = lj_form_find_menu(win ? lj_window_desc(win) : NULL, which == LJ_POP_ICON ? "PopIcon" : "PopDesktop");
    lj_rect cr;
    int sx = 0, sy = 0;
    if (!m || !d) return;
    lj_popup_fixture = which == LJ_POP_ICON ? fixture : -1;
    lj_main_menu_sync(win);
    lj_window_client_rect(win, &cr);
    if (m->host && m->host->screen_pos) m->host->screen_pos(m->host->ud, win, &sx, &sy);
    lj_menu_popup(m, d, sx + cr.x + x, sy + cr.y + y);
}
