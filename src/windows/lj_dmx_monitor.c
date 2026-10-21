/* lj_dmx_monitor.c - see lj_dmx_monitor.h. The DFM's sg1 TStringGrid is driven as a
 * draw grid (cells = NULL -> OnDrawCell per cell, DefaultDrawing = False as the DFM
 * says) so sg1DrawCell paints the colour table; every other OnXxx is bound (ME-16). */
#define _CRT_SECURE_NO_WARNINGS 1   /* strncat under /W4 /WX on MSVC (as core/lj_ini.c) */
#include "windows/lj_dmx_monitor.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_warnings.h"

#define LJ_DM_COLS 10
#define LJ_DM_ROWS ((LJ_CHANS + LJ_DM_COLS - 1) / LJ_DM_COLS)   /* 205 data rows: 1, 11, 21 ... 2041 */
#define LJ_DM_DRAIN 200                                        /* overwrites posted per 100 ms pass (LJ_ENGINE_CMD_CAP is 256) */
#define LJ_WIN_STAY_ON_TOP 2u                                  /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */

typedef struct {
    lj_window *win, *find_win;
    lj_widget *grid, *fader, *spin, *sbar;
    uint8_t base[LJ_CHANS], cur[LJ_CHANS], layer[LJ_CHANS];
    uint8_t ovr_on[LJ_CHANS / 8], pend_on[LJ_CHANS / 8]; uint16_t pend_val[LJ_CHANS];   /* queued LJ_CMD_OVERWRITE (value, 256 = clear) */
    int pend_any, clear_all_pending;
    unsigned prefs; int overwrite_enabled, debug_layer;
    int sel, last_master, fader_down, loading;
    char status[160];
    lj_dmx_search search; int searching;
} lj_dm_state;
static lj_dm_state dm;

static const char *const lj_ch_class_names[LJ_CH_COUNT] = { "", "Intensity", "Shutter", "Strobe", "Pan", "Tilt", "Color", "Gobo", "Beam", "Effect", "Level", "Speed", "Lamp", "Special", "Switch", "Smoke", "Haze", "Haze fan" };

lj_window *lj_dmx_monitor_window(void) { return dm.win; }
int lj_dmx_monitor_selected(void) { return dm.win ? dm.sel : -1; }
const char *lj_dmx_monitor_status(void) { return dm.status; }
int lj_dmx_monitor_overwrite_enabled(void) { return dm.overwrite_enabled; }
static int bit(const uint8_t *set, int i) { return (set[i >> 3] >> (i & 7)) & 1; }
static void bit_set(uint8_t *set, int i, int on) { if (on) set[i >> 3] |= (uint8_t)(1u << (i & 7)); else set[i >> 3] &= (uint8_t)~(1u << (i & 7)); }

void lj_dmx_monitor_cell_colour(int used, int master, int changed, int overwritten, unsigned prefs, uint32_t *bg, uint32_t *fg)
{
    *bg = LJ_CLR_BTNHIGHLIGHT;                                               /* white = unused [01 s5] */
    if (used && (prefs & LJ_DM_PREF_USED)) *bg = LJ_CLR_BTNFACE;             /* grey = used by a configured fixture */
    if (master && (prefs & LJ_DM_PREF_MASTER)) *bg = LJ_CLR_GREEN;           /* green = the master fixture's channels */
    *fg = LJ_CLR_3DDKSHADOW;                                                 /* black = unchanged since open */
    if (changed && (prefs & LJ_DM_PREF_CHANGES)) *fg = LJ_CLR_RED;           /* red = changed */
    if (overwritten) *fg = 0x008000u;                                        /* green = overwritten (clGreen, distinct from the clLime cell) */
}

/* ---- status line (ME-28: LJ2's "control function of the highlighted channel") ---- */
static void status_update(void)
{
    lj_show *sh = lj_ctx_get()->show;
    int fx = sh ? lj_show_fixture_at(sh, dm.sel) : -1, link = dm.sel / LJ_LINK_CH + 1, addr = dm.sel % LJ_LINK_CH + 1;
    char fn[64] = "not configured";
    if (dm.sel < 0) { dm.status[0] = 0; }
    else {
        if (fx < 0 && sh) { int i; for (i = 0; i < LJ_FIXTURES; i++) { const lj_fixture *f = &sh->fixture[i]; const lj_profile *p = f->link ? lj_fixlib_by_id(f->profile_id) : NULL; int b = f->link && f->address ? lj_addr_code(f->link, f->address) : -1;
                                   if (b >= 0 && dm.sel >= b && dm.sel < b + (p ? p->footprint : 1)) { fx = i; break; } } }   /* the W1 model stub's lj_show_fixture_at returns -1; same rule locally, footprint 1 when no profile is bound */
        if (fx >= 0 && sh) {
            const lj_fixture *f = &sh->fixture[fx];
            const lj_profile *p = lj_fixlib_by_id(f->profile_id);
            int off = dm.sel - lj_addr_code(f->link, f->address);
            const char *cls = (p && off >= 0 && off < LJ_PROFILE_CH && p->channel_class[off] < LJ_CH_COUNT) ? lj_ch_class_names[p->channel_class[off]] : "";
            const char *cap = (p && off >= 0 && off < LJ_PROFILE_CH && p->level_caption[off][0]) ? (const char *)p->level_caption[off] : "";
            snprintf(fn, sizeof fn, "(%d) %s - %s%s%s", fx + 1, f->user_name, cls, cap[0] && cls[0] ? " " : "", cap);
        }
        snprintf(dm.status, sizeof dm.status, "%d:%03d  %s", link, addr, fn);     /* PROPOSAL rig: LJ2's exact status format is unmeasured */
        if (dm.debug_layer) { char l[24]; snprintf(l, sizeof l, "  [layer %u]", dm.layer[dm.sel]); strncat(dm.status, l, sizeof dm.status - strlen(dm.status) - 1); }   /* layer_of behind [DEBUG] (ME-28) */
    }
    if (dm.sbar) lj_w_statusbar_set_text(dm.sbar, 0, dm.status);
}

int lj_dmx_monitor_select(int code)
{
    lj_w_grid *g = (lj_w_grid *)dm.grid;
    if (!dm.win || code < 0 || code >= LJ_CHANS) return -1;
    dm.sel = code;
    g->row = 1 + code / LJ_DM_COLS; g->col = 1 + code % LJ_DM_COLS;
    if (g->row - 1 < g->top_row) g->top_row = g->row - 1;
    status_update();
    lj_widget_invalidate(dm.grid);
    return 0;
}

/* ---- overwrites (S8) ---- */
void lj_dmx_monitor_set_overwrite_enabled(int on) { dm.overwrite_enabled = on != 0; if (dm.win) lj_menu_set_checked(lj_window_menus(dm.win), "M_EnableOverwrite", on != 0); }

static void queue(int code, int value) { dm.pend_val[code] = (uint16_t)value; bit_set(dm.pend_on, code, 1); bit_set(dm.ovr_on, code, value != 256); dm.pend_any = 1; }

int lj_dmx_monitor_overwrite(int code, int value)
{
    if (!dm.win || code < 0 || code >= LJ_CHANS || value < 0 || value > 256) return -1;
    if (!dm.overwrite_enabled && !dm.searching) return -1;
    queue(code, value);
    lj_widget_invalidate(dm.grid);
    return 0;
}

static void drain(void)
{
    lj_cmd c;
    int i, n = 0;
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_OVERWRITE;
    if (dm.clear_all_pending) { c.a = 0xFFFF; c.b = 256; lj_ctx_post(&c); dm.clear_all_pending = 0; }
    if (!dm.pend_any) return;
    for (i = 0; i < LJ_CHANS && n < LJ_DM_DRAIN; i++) {
        if (!bit(dm.pend_on, i)) continue;
        c.a = (uint32_t)i; c.b = dm.pend_val[i]; lj_ctx_post(&c);
        bit_set(dm.pend_on, i, 0); n++;
    }
    for (; i < LJ_CHANS; i++) if (bit(dm.pend_on, i)) return;                 /* more next pass */
    dm.pend_any = 0;
}

/* ---- Find Channel: binary search ---- */
int lj_dmx_search_begin(lj_dmx_search *s, const uint8_t include_link[LJ_LINKS], int exclude_used, int on, int off)
{
    lj_show *sh = lj_ctx_get()->show;
    uint8_t used[LJ_CHANS / 8];
    int i;
    memset(s, 0, sizeof *s);
    memset(used, 0, sizeof used);
    if (sh) for (i = 0; i < LJ_FIXTURES; i++) {                                /* configured = inside a fixture's footprint */
        const lj_fixture *f = &sh->fixture[i];
        const lj_profile *p = f->link ? lj_fixlib_by_id(f->profile_id) : NULL;
        int b = f->link && f->address ? lj_addr_code(f->link, f->address) : 0, k, fp = f->link && f->address ? (p ? p->footprint : 1) : 0;
        for (k = 0; k < fp && b + k < LJ_CHANS; k++) bit_set(used, b + k, 1);
    }
    for (i = 0; i < LJ_CHANS; i++) {
        if (!include_link[i / LJ_LINK_CH]) continue;
        if (exclude_used && bit(used, i)) continue;
        s->cand[s->n++] = (uint16_t)i;
    }
    s->lo = 0; s->hi = s->n; s->on = (uint8_t)on; s->off = (uint8_t)off;
    return s->n ? 0 : -1;
}
int lj_dmx_search_half(const lj_dmx_search *s, int *first, int *last)
{
    int mid = (s->lo + s->hi) / 2;
    if (s->hi - s->lo < 2) return -1;
    *first = s->cand[s->lo] + 1; *last = s->cand[mid - 1] + 1;                 /* 1-based global channel numbers as the dialog labels count them */
    return 0;
}
int lj_dmx_search_answer(lj_dmx_search *s, int yes)
{
    int mid = (s->lo + s->hi) / 2;
    if (s->hi - s->lo < 2) return s->hi - s->lo == 1 ? s->cand[s->lo] : -1;
    s->steps++;
    if (yes) s->hi = mid; else s->lo = mid;
    return s->hi - s->lo == 1 ? s->cand[s->lo] : -1;
}
int lj_dmx_search_apply(const lj_dmx_search *s)
{
    int i, mid = (s->lo + s->hi) / 2;
    if (!dm.win) return -1;
    for (i = 0; i < s->n; i++) queue(s->cand[i], (i >= s->lo && i < mid) ? s->on : s->off);
    lj_widget_invalidate(dm.grid);
    return 0;
}

static void search_step(void);
static void search_result(int yes, void *ud)
{
    int found;
    (void)ud;
    if (!dm.searching) return;
    found = lj_dmx_search_answer(&dm.search, yes);
    if (found >= 0) {
        char msg[64];
        snprintf(msg, sizeof msg, "Channel found: %d:%03d (%d tries)", found / LJ_LINK_CH + 1, found % LJ_LINK_CH + 1, dm.search.steps);   /* PROPOSAL: the exe's result literal is not in the catalogue */
        dm.searching = 0; dm.clear_all_pending = 1; memset(dm.ovr_on, 0, sizeof dm.ovr_on); dm.pend_any = 0; memset(dm.pend_on, 0, sizeof dm.pend_on);
        lj_dmx_monitor_select(found);
        lj_win_message("Find Channel", msg);
        return;
    }
    search_step();
}
static void search_step(void)
{
    char line1[96], line2[48];
    int first, last;
    if (lj_dmx_search_half(&dm.search, &first, &last) != 0) { dm.searching = 0; return; }
    lj_dmx_search_apply(&dm.search);
    snprintf(line1, sizeof line1, "Searching %d channels in pattern from %d to %d", dm.search.hi - dm.search.lo, first, last);   /* exe 0x1FC4CC, the "\n" split over the two labels */
    lj_str_copy(line2, sizeof line2, "Is channel on now ?");
    lj_win_confirm("Find Channel", line1, line2, search_result, NULL);        /* OK = yes, Cancel = no (LJ2's Yes/No MessageBox buttons: rig item) */
}

/* ---- the grid (sg1DrawCell) ---- */
static void on_draw_cell(lj_widget *w, void *ud)
{
    lj_w_grid *g = (lj_w_grid *)w;
    lj_surface *s = g->draw_surf;
    const lj_font *f = lj_w_font_of(w);
    int c = g->draw_col, r = g->draw_row, x = g->draw_rect.x, y = g->draw_rect.y, code;
    char t[8];
    uint32_t bg, fg;
    (void)ud;
    if (!s) return;
    if (r == 0 || c == 0) {                                                  /* fixed cells: "+0".."+9", row base 1, 11, 21 ... */
        if (r == 0 && c == 0) return;
        if (r == 0) snprintf(t, sizeof t, "+%d", c - 1); else snprintf(t, sizeof t, "%d", (r - 1) * LJ_DM_COLS + 1);
        lj_text(s, f, x + (g->draw_rect.w - lj_text_width(f, t)) / 2, y + (g->draw_rect.h - f->height) / 2, t, LJ_CLR_3DDKSHADOW, 0);
        return;
    }
    code = (r - 1) * LJ_DM_COLS + (c - 1);
    if (code >= LJ_CHANS) { lj_fill(s, x, y, g->draw_rect.w, g->draw_rect.h, LJ_CLR_BTNFACE); return; }
    {
        lj_show *sh = lj_ctx_get()->show;
        int master = lj_sel_master(), used = 0, is_master = 0, i;
        if (sh) for (i = 0; i < LJ_FIXTURES; i++) {
            const lj_fixture *fx = &sh->fixture[i];
            const lj_profile *p = fx->link ? lj_fixlib_by_id(fx->profile_id) : NULL;
            int b = fx->link && fx->address ? lj_addr_code(fx->link, fx->address) : -1;
            if (b >= 0 && code >= b && code < b + (p ? p->footprint : 1)) { used = 1; is_master = (i == master); break; }
        }
        lj_dmx_monitor_cell_colour(used, is_master, dm.cur[code] != dm.base[code], bit(dm.ovr_on, code), dm.prefs, &bg, &fg);
    }
    if (code == dm.sel) { lj_fill(s, x, y, g->draw_rect.w, g->draw_rect.h, LJ_CLR_HIGHLIGHT); fg = LJ_CLR_BTNHIGHLIGHT; }
    else lj_fill(s, x, y, g->draw_rect.w, g->draw_rect.h, bg);
    snprintf(t, sizeof t, "%d", dm.cur[code]);
    lj_text(s, f, x + g->draw_rect.w - 2 - lj_text_width(f, t), y + (g->draw_rect.h - f->height) / 2, t, fg, 0);
}
static void on_select_cell(lj_widget *w, void *ud)
{
    lj_w_grid *g = (lj_w_grid *)w;
    int code = (g->row - 1) * LJ_DM_COLS + (g->col - 1);
    (void)ud;
    if (code >= 0 && code < LJ_CHANS) { dm.sel = code; status_update(); if (!dm.loading) lj_w_spinedit_set_value(dm.spin, dm.cur[code]); }
}

/* ---- the fader (pbValue 47x149) and the value field ---- */
static void on_fader_paint(lj_widget *w, void *ud)
{
    lj_surface *s = lj_window_surface(dm.win);
    int ax, ay, v = dm.sel >= 0 ? dm.cur[dm.sel] : 0, h;
    (void)ud;
    lj_widget_abs(w, &ax, &ay);
    ax += lj_window_root(dm.win)->x; ay += lj_window_root(dm.win)->y;
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_BTNHIGHLIGHT);
    h = (w->h - 2) * v / 255;
    lj_fill(s, ax + 1, ay + w->h - 1 - h, w->w - 2, h, 0x0000FFu);           /* blue fill as the level faders [07 s3.5]; PROPOSAL: the monitor fader itself is unmeasured */
}
static int fader_value(lj_widget *w)
{
    const lj_mouse_ev *m = lj_window_last_event(dm.win);
    int ax, ay, v;
    if (!m) return -1;
    lj_widget_abs(dm.fader, &ax, &ay);
    ax += lj_window_root(dm.win)->x; ay += lj_window_root(dm.win)->y;
    (void)w;
    v = 255 - (m->y - ay) * 255 / (dm.fader->h - 1);
    return v < 0 ? 0 : v > 255 ? 255 : v;
}
static void fader_apply(int v)
{
    if (dm.sel < 0 || v < 0) return;
    dm.loading = 1; lj_w_spinedit_set_value(dm.spin, v); dm.loading = 0;
    if (dm.overwrite_enabled) lj_dmx_monitor_overwrite(dm.sel, v);
    lj_widget_invalidate(dm.fader);
}
static void on_fader_down(lj_widget *w, void *ud) { (void)ud; dm.fader_down = 1; fader_apply(fader_value(w)); }
static void on_fader_move(lj_widget *w, void *ud) { (void)ud; if (dm.fader_down) fader_apply(fader_value(w)); }
static void on_fader_up(lj_widget *w, void *ud) { (void)w; (void)ud; dm.fader_down = 0; }
static void on_value_change(lj_widget *w, void *ud) { (void)ud; if (!dm.loading && dm.sel >= 0 && dm.overwrite_enabled) lj_dmx_monitor_overwrite(dm.sel, lj_w_spinedit_value(w)); }
static void on_clear(lj_widget *w, void *ud) { (void)w; (void)ud; if (dm.sel >= 0 && dm.overwrite_enabled) lj_dmx_monitor_overwrite(dm.sel, 256); }

/* ---- menu, timer, close ---- */
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void on_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(dm.win);
    lj_menu_rt *rt = it ? lj_menu_rt_find(lj_window_menus(dm.win), it) : NULL;
    unsigned flag = 0;
    (void)w; (void)ud;
    if (!it || !rt) return;
    rt->checked = !rt->checked;
    if (strcmp(it->name, "M_ShowChanges") == 0) flag = LJ_DM_PREF_CHANGES;
    else if (strcmp(it->name, "M_ShowUsed") == 0) flag = LJ_DM_PREF_USED;
    else if (strcmp(it->name, "M_HighlightMaster") == 0) flag = LJ_DM_PREF_MASTER;
    else if (strcmp(it->name, "M_AutoScroll") == 0) flag = LJ_DM_PREF_AUTOSCROLL;
    else if (strcmp(it->name, "M_EnableOverwrite") == 0) { dm.overwrite_enabled = rt->checked; return; }
    if (rt->checked) dm.prefs |= flag; else dm.prefs &= ~flag;
    lj_widget_invalidate(dm.grid);
}
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_dmx_monitor_refresh(); }
static void on_form_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_dmx_monitor_close(); }

static void find_noop_close(lj_widget *w, void *ud) { (void)w; (void)ud; if (dm.find_win) { lj_win_hide(dm.find_win); lj_window_destroy(dm.find_win); dm.find_win = NULL; } }
static void on_find_start(lj_widget *w, void *ud)
{
    uint8_t inc[LJ_LINKS];
    int on = lj_w_spinedit_value(lj_window_find(dm.find_win, "seOn")), off = lj_w_spinedit_value(lj_window_find(dm.find_win, "seOff"));
    int excl = lj_w_checkbox_checked(lj_window_find(dm.find_win, "cbExcludeUsed"));
    (void)w; (void)ud;
    inc[0] = (uint8_t)lj_w_checkbox_checked(lj_window_find(dm.find_win, "cbLink1")); inc[1] = (uint8_t)lj_w_checkbox_checked(lj_window_find(dm.find_win, "cbLink2"));
    inc[2] = (uint8_t)lj_w_checkbox_checked(lj_window_find(dm.find_win, "cbLink3")); inc[3] = (uint8_t)lj_w_checkbox_checked(lj_window_find(dm.find_win, "cbLink4"));
    find_noop_close(NULL, NULL);
    if (lj_dmx_search_begin(&dm.search, inc, excl, on, off) != 0) { lj_win_message("Find Channel", "No channels to search"); return; }
    dm.searching = 1;
    search_step();
}
static void on_find_channel(lj_widget *w, void *ud)
{
    static const lj_binding b[] = { { "FormClose", find_noop_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "bStartClick", on_find_start, NULL }, { "bCancelClick", find_noop_close, NULL }, { "Help1Click", noop, NULL }, { NULL, NULL, NULL } };
    (void)w; (void)ud;
    if (dm.find_win) return;
    dm.find_win = lj_window_create(&lj_form_TFINDCHANNELFORM, b);
    if (dm.find_win) lj_win_show(dm.find_win, LJ_WIN_STAY_ON_TOP);
}

static const lj_binding lj_dm_bindings[] = {
    { "FormActivate", noop, NULL }, { "FormClose", on_form_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormResize", noop, NULL },
    { "sg1DrawCell", on_draw_cell, NULL }, { "sg1MouseDown", noop, NULL }, { "sg1MouseMove", noop, NULL }, { "sg1MouseUp", noop, NULL },
    { "sg1MouseWheelDown", noop, NULL }, { "sg1MouseWheelUp", noop, NULL }, { "sg1SelectCell", on_select_cell, NULL },
    { "pbValueMouseDown", on_fader_down, NULL }, { "pbValueMouseMove", on_fader_move, NULL }, { "pbValueMouseUp", on_fader_up, NULL }, { "pbValuePaint", on_fader_paint, NULL },
    { "sevalueChange", on_value_change, NULL }, { "sbClearClick", on_clear, NULL },
    { "MPrefsClick", on_prefs, NULL }, { "FindChannel1Click", on_find_channel, NULL }, { "Help1Click", noop, NULL }, { "Timer1Timer", on_timer, NULL },
    { NULL, NULL, NULL }
};

static void copy_frame(void)
{
    lj_ctx *ctx = lj_ctx_get();
    const lj_frame *fr;
    if (!ctx->engine) return;
    lj_lock();
    fr = lj_engine_frame(ctx->engine);
    if (fr) { memcpy(dm.cur, fr->out, sizeof dm.cur); if (dm.debug_layer) memcpy(dm.layer, fr->layer_of, sizeof dm.layer); }
    lj_unlock();
}

void lj_dmx_monitor_refresh(void)
{
    lj_show *sh = lj_ctx_get()->show;
    int master;
    if (!dm.win) return;
    copy_frame();
    drain();
    master = lj_sel_master();
    if ((dm.prefs & LJ_DM_PREF_AUTOSCROLL) && master != dm.last_master && sh && master >= 0 && sh->fixture[master].link) {
        ((lj_w_grid *)dm.grid)->top_row = lj_addr_code(sh->fixture[master].link, sh->fixture[master].address) / LJ_DM_COLS;
        dm.last_master = master;
    }
    status_update();
    lj_widget_invalidate(dm.grid);
    lj_widget_invalidate(dm.fader);
}

lj_window *lj_dmx_monitor_open(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_w_grid *g;
    if (dm.win) return dm.win;
    memset(&dm, 0, sizeof dm);
    dm.win = lj_window_create(&lj_form_TDMXOUTFORM, lj_dm_bindings);
    if (!dm.win) { lj_log("dmx monitor: cannot create TDMXOutForm"); return NULL; }
    dm.grid = lj_window_find(dm.win, "sg1"); dm.fader = lj_window_find(dm.win, "pbValue"); dm.spin = lj_window_find(dm.win, "sevalue"); dm.sbar = lj_window_find(dm.win, "SBar");
    if (!dm.grid || !dm.fader || !dm.spin) { lj_dmx_monitor_close(); return NULL; }
    g = (lj_w_grid *)dm.grid;
    g->cells = NULL;                                                         /* DefaultDrawing = False: every cell through sg1DrawCell */
    g->rows = 1 + LJ_DM_ROWS; g->cols = 1 + LJ_DM_COLS;
    g->row_select = 0;
    dm.prefs = LJ_DM_PREF_CHANGES | LJ_DM_PREF_USED | LJ_DM_PREF_MASTER | LJ_DM_PREF_AUTOSCROLL;   /* the DFM's Checked = True items */
    dm.sel = -1; dm.last_master = -1;
    dm.debug_layer = ctx->main_ini ? lj_ini_geti(ctx->main_ini, "DEBUG", "MonitorLayer", 0) : 0;
    copy_frame();
    memcpy(dm.base, dm.cur, sizeof dm.base);                                 /* "unchanged since open" */
    lj_dmx_monitor_select(0);
    lj_win_show(dm.win, LJ_WIN_STAY_ON_TOP);
    return dm.win;
}

void lj_dmx_monitor_close(void)
{
    lj_cmd c;
    if (!dm.win) return;
    find_noop_close(NULL, NULL);
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_OVERWRITE; c.a = 0xFFFF; c.b = 256;
    lj_ctx_post(&c);                                                         /* every overwrite clears when the monitor closes [01 s5] */
    lj_win_hide(dm.win);
    lj_window_destroy(dm.win);
    memset(&dm, 0, sizeof dm);
}
