/* lj_cue_form.c - Cue control, Save Cue, List of Cues (lj_cue_form.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows/lj_cue_form.h"

#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_cue.h"
#include "model/lj_library.h"
#include "model/lj_words.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "ui/widgets/lj_w_cuerow.h"
#include "windows/lj_windows.h"

#define LJ_CUE_COLS   9
#define LJ_CUE_BTN_W  30     /* measured from cuecontrol.gif, reduced, rig item: the numbered enable button left of the list */
#define LJ_CUE_HDR_H  17     /* measured from cueslots.gif, rig item */
#define LJ_CUE_CELL   40
#define LJ_CUE_ROW_PITCH LJ_CUEROW_H   /* ~16 px [07 s6]; at the DFM client the 12 rows share 194 - 17 px, so 14 there */

static const char *const lj_cue_col_names[LJ_CUE_COLS] = { "No", "Sequence", "Type", "Scene", "ST", "FT", "Status", "Loop", "Trig" };
static const int lj_cue_col_dflt[LJ_CUE_COLS] = { 30, 160, 115, 45, 38, 38, 45, 45, 40 };   /* forms.ini [CUE] C0..C8 */
static const char *const lj_cue_status_words[] = { "", "", "Stop", "Error", "Disabled", "Edit", "Out" };   /* LJ_SLOT_* [03 s5.1] */

typedef struct { char cell[LJ_CUE_COLS][LJ_CUE_CELL]; uint32_t rgb; uint8_t enabled; } lj_cue_row;

/* ---- the grid widget: header + 12 enable buttons + 12 rows, one paint, one mouse ---- */
typedef struct { lj_widget base; int row_h; lj_column_desc cols[LJ_CUE_COLS]; int widths[LJ_CUE_COLS]; } lj_cue_grid;

static struct {
    lj_window *win;
    lj_cue_grid *grid;
    lj_cue_row row[LJ_CUE_SLOTS];
    uint16_t sel;                           /* selection bitmask, slot i = bit i */
    int col_w[LJ_CUE_COLS];
    unsigned columns;                       /* [CUE] Columns bitmask, tag bits of the Show* menu items */
    int use_colors, bpm, chase_fade;
    char caption[128];
    const lj_engine_status *view;
    lj_cue_form_host host; int has_host;
    lj_cmd last; int has_last;
    int popup_slot;
} s;

static const lj_engine_status lj_zero_view = { 0 };

static lj_ctx *C(void) { return lj_ctx_get(); }
static lj_cue *cue_rec(void) { lj_ctx *c = C(); return c->show ? c->show->current_cue : NULL; }
static const lj_engine_status *view(void) { return s.view ? s.view : &lj_zero_view; }

static void post(lj_cmd *c)
{
    s.last = *c; s.has_last = 1;
    lj_ctx_post(c);
}
static void post3(int kind, uint32_t a, uint32_t b, uint32_t c)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c;
    post(&cmd);
}
static void host_show(lj_window *w, int open) { if (w && s.has_host && s.host.show) s.host.show(s.host.ud, w, open); }
void lj_cue_form_repin(void) { if (s.has_host && s.host.repin) s.host.repin(s.host.ud); }
static void modified(void)
{
    lj_ctx_set_modified(LJ_MOD_CUE, 1);
    lj_cue_form_repin();
}
static int form_geti(const char *key, int dflt) { lj_ctx *c = C(); return c->forms_ini ? lj_ini_geti(c->forms_ini, "CUE", key, dflt) : dflt; }
static void form_seti(const char *key, int v) { lj_ctx *c = C(); if (c->forms_ini) lj_ini_seti(c->forms_ini, "CUE", key, v); }
static int main_geti(const char *key, int dflt) { lj_ctx *c = C(); return c->main_ini ? lj_ini_geti(c->main_ini, "CUE", key, dflt) : dflt; }
static void main_seti(const char *key, int v) { lj_ctx *c = C(); if (c->main_ini) lj_ini_seti(c->main_ini, "CUE", key, v); }

void lj_cue_form_set_host(const lj_cue_form_host *host) { s.has_host = host != NULL; if (host) s.host = *host; }
const lj_cue_form_host *lj_cue_form_get_host(void) { return s.has_host ? &s.host : NULL; }
void lj_cue_form_set_view(const lj_engine_status *v) { s.view = v; }
lj_window *lj_cue_form_get(void) { return s.win; }
int lj_cue_form_last_cmd(lj_cmd *out) { if (out && s.has_last) *out = s.last; return s.has_last; }
void lj_cue_form_select(int slot, int on)
{
    uint16_t bit;
    if (slot >= LJ_CUE_SLOTS) return;
    bit = (uint16_t)(slot < 0 ? 0x0FFFu : 1u << slot);
    s.sel = (uint16_t)(on ? (s.sel | bit) : (s.sel & ~bit));
    if (s.grid) lj_widget_invalidate(&s.grid->base);
}
int lj_cue_form_selected(int slot) { return slot >= 0 && slot < LJ_CUE_SLOTS && ((s.sel >> slot) & 1); }

const int *lj_cue_form_col_widths(void)
{
    char key[4];
    int i;
    for (i = 0; i < LJ_CUE_COLS; i++) {
        snprintf(key, sizeof key, "C%d", i);
        s.col_w[i] = form_geti(key, lj_cue_col_dflt[i]);
        if (s.col_w[i] < 0 || s.col_w[i] > 1000) s.col_w[i] = lj_cue_col_dflt[i];
    }
    return s.col_w;
}

uint32_t lj_cue_form_row_rgb(int status, int scenes, int chase_manual, int link, int use_colors)
{
    switch (status) {                                   /* [07 s3.5], CHM 4977-4993 */
    case LJ_SLOT_EMPTY:      return 0x808080u;          /* the slot label in grey */
    case LJ_SLOT_DISABLED:   return use_colors ? 0x808080u : 0x000000u;
    case LJ_SLOT_LOAD_ERROR: return use_colors ? 0xFF0000u : 0x000000u;
    case LJ_SLOT_EDIT:       return use_colors ? (link ? 0x008000u : 0x0000FFu) : 0x000000u;
    case LJ_SLOT_STOPPED:    return use_colors ? 0x800000u : 0x000000u;
    case LJ_SLOT_OUT:        return use_colors ? 0x800000u : 0x000000u;
    default:                 return use_colors ? (chase_manual ? 0xFF0000u : scenes <= 1 ? 0x800000u : 0x000000u) : 0x000000u;
    }
}

/* the grid's display words (cuecontrol.gif at 8x: Fwd / Auto in every row, ME-75); the record words of model/lj_words.c are unchanged */
static const char *const lj_dir_disp[]  = { "Fwd", "Rev", "Bounce", "Rnd", NULL };
static const char *const lj_trig_disp[] = { "Auto", "Manual", "Audio", NULL };

/* one row's text from the cue record + the view [03 s5.1] */
static void build_row(int i, lj_cue_row *r)
{
    const lj_cue *cue = cue_rec();
    const lj_engine_status *v = view();
    const lj_cue_slot *sl = cue ? &cue->slot[i] : NULL;
    const lj_sequence *seq = NULL;
    int status = v->slot_status[i], scenes = v->slot_scenes[i], scene = v->slot_scene[i];
    uint32_t pct;
    char key[12];
    memset(r, 0, sizeof *r);
    r->enabled = (uint8_t)(sl ? sl->enabled : 1);
    if (!sl || sl->seq == 0) {
        snprintf(key, sizeof key, "SlotName%d", i);
        lj_str_copy(r->cell[1], LJ_CUE_CELL, C()->forms_ini ? lj_ini_gets(C()->forms_ini, "CUE", key, "") : "");
        r->rgb = lj_cue_form_row_rgb(LJ_SLOT_EMPTY, 0, 0, 0, s.use_colors);
        return;
    }
    if (C()->show) seq = (const lj_sequence *)lj_table_get(&C()->show->sequences, sl->seq);
    if (status == LJ_SLOT_EMPTY) status = sl->enabled ? LJ_SLOT_RUNNING : LJ_SLOT_DISABLED;   /* not loaded in the engine yet: show the record */
    if (!seq && status != LJ_SLOT_LOAD_ERROR && v->slot_status[i] == LJ_SLOT_EMPTY) status = LJ_SLOT_LOAD_ERROR;
    if (seq && scenes == 0) { scenes = seq->n; scene = 1; }
    snprintf(r->cell[0], LJ_CUE_CELL, "%04u", (unsigned)sl->seq);
    lj_str_copy(r->cell[1], LJ_CUE_CELL, status == LJ_SLOT_LOAD_ERROR ? "load error" : seq ? seq->name : "");
    lj_str_copy(r->cell[2], LJ_CUE_CELL, seq ? seq->type : "");
    if (scenes <= 1) lj_str_copy(r->cell[3], LJ_CUE_CELL, "Static");      /* cuecontrol.gif */
    else snprintf(r->cell[3], LJ_CUE_CELL, "%d:%d", scene, scenes);
    pct = v->slot_time_pct[i] ? v->slot_time_pct[i] : sl->time_pct ? sl->time_pct : cue->cue_time_pct;
    if (seq && seq->scenes && scene >= 1 && scene <= seq->n) {
        const lj_scene *sc = &seq->scenes[scene - 1];
        uint32_t st = sc->scene_ms * pct / 100u, ft = sc->fade_ms * pct / 100u;    /* 11 F8-F9, capped as the engine caps */
        if (st > LJ_TIME_MAX_MS) st = LJ_TIME_MAX_MS;
        if (ft > LJ_TIME_MAX_MS) ft = LJ_TIME_MAX_MS;
        if (s.bpm) snprintf(r->cell[4], LJ_CUE_CELL, "%u", st ? 60000u / st : 0u);
        else lj_fmt_time(r->cell[4], LJ_CUE_CELL, st);                              /* s:hh with the colon, like the Sequence window (cuecontrol.gif 1:00 / 0:05, ME-75) */
        lj_fmt_time(r->cell[5], LJ_CUE_CELL, ft);
    }
    lj_str_copy(r->cell[6], LJ_CUE_CELL, status >= 0 && status < LJ_COUNT_OF(lj_cue_status_words) ? lj_cue_status_words[status] : "");
    lj_str_copy(r->cell[7], LJ_CUE_CELL, lj_word(lj_dir_disp, sl->dir));
    lj_str_copy(r->cell[8], LJ_CUE_CELL, lj_word(lj_trig_disp, sl->trig));
    r->rgb = lj_cue_form_row_rgb(status, scenes, v->slot_chase[i] && sl->trig == LJ_TRIG_MANUAL, sl->end_action == LJ_END_LINK, s.use_colors);
}

int lj_cue_form_cell(int slot, int col, char *dst, size_t cap)
{
    if (slot < 0 || slot >= LJ_CUE_SLOTS || col < 0 || col >= LJ_CUE_COLS || !dst) return -1;
    lj_str_copy(dst, cap, s.row[slot].cell[col]);
    return 0;
}

int lj_cue_form_caption(char *dst, size_t cap)
{
    const lj_cue *cue = cue_rec();
    const lj_engine_status *v = view();
    int edit = v->cue_edit, i;
#if LJ_CUE_EDIT_PREFIX_RULE
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (v->slot_status[i] == LJ_SLOT_EDIT) edit = 1;
#else
    (void)i;
#endif
    return lj_win_caption_cue(dst, cap, cue ? cue->number : 0, cue ? cue->name : "", edit);
}

/* ---- grid paint / mouse ---- */
static int grid_visible_w(const lj_cue_grid *g, int i) { return (s.columns >> i & 1) ? g->widths[i] : 0; }

static void grid_paint(lj_widget *w, lj_surface *sf)
{
    lj_cue_grid *g = (lj_cue_grid *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i, k, x, y, vis[LJ_CUE_COLS];
    char num[4];
    lj_widget_abs(w, &ax, &ay);
    lj_fill(sf, ax, ay, w->w, w->h, LJ_CLR_BTNFACE);
    for (i = 0; i < LJ_CUE_COLS; i++) vis[i] = grid_visible_w(g, i);
    lj_w_header_paint(sf, w, ax + LJ_CUE_BTN_W, ay, w->w - LJ_CUE_BTN_W, LJ_CUE_HDR_H, g->cols, vis, LJ_CUE_COLS, 0);
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        const lj_cue_row *r = &s.row[i];
        int selected = (s.sel >> i) & 1, ty;
        uint32_t rgb = selected ? LJ_CLR_BTNHIGHLIGHT : r->rgb;
        y = ay + LJ_CUE_HDR_H + i * g->row_h;
        lj_w_button(sf, ax, y, LJ_CUE_BTN_W, g->row_h, r->enabled ? LJ_CLR_GREEN : LJ_CLR_RED, !r->enabled);   /* green raised / red depressed (CHM 4937) */
        snprintf(num, sizeof num, "%d", i + 1);
        lj_w_text_c(sf, w, ax + !r->enabled, y + !r->enabled, LJ_CUE_BTN_W, g->row_h, num, LJ_CLR_3DDKSHADOW, 0);
        lj_fill(sf, ax + LJ_CUE_BTN_W, y, w->w - LJ_CUE_BTN_W, g->row_h, selected ? LJ_CLR_HIGHLIGHT : LJ_CLR_BTNHIGHLIGHT);   /* navy selection [07 s6] */
        ty = y + (g->row_h - f->height) / 2;
        for (k = 0, x = ax + LJ_CUE_BTN_W; k < LJ_CUE_COLS && x < ax + w->w; x += vis[k], k++)
            if (vis[k] > 4) lj_w_text_fit(sf, w, x + 2, ty, r->cell[k], rgb, vis[k] - 4);
    }
}

static int grid_row_at(const lj_cue_grid *g, int y)
{
    int r = (y - LJ_CUE_HDR_H) / (g->row_h > 0 ? g->row_h : 1);
    return y >= LJ_CUE_HDR_H && r < LJ_CUE_SLOTS ? r : -1;
}

static void set_enabled(int i, int on)
{
    lj_cue *cue = cue_rec();
    if (!cue) return;
    lj_lock();
    cue->slot[i].enabled = (uint8_t)(on != 0);
    lj_unlock();
    post3(LJ_CMD_ENABLE, (uint32_t)(LJ_RUN_SLOT0 + i), (uint32_t)(on != 0), 0);
    lj_ctx_set_modified(LJ_MOD_CUE, 1);
    s.row[i].enabled = (uint8_t)(on != 0);
    if (s.grid) lj_widget_invalidate(&s.grid->base);
}

static void slot_popup(int slot, int x, int y);

static int grid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_cue_grid *g = (lj_cue_grid *)w;
    int row = grid_row_at(g, m->y);
    if (m->type != LJ_EV_MOUSE_DOWN || row < 0 || !lj_w_inside(w, m->x, m->y)) return m->type == LJ_EV_MOUSE_DOWN;
    if (m->x < LJ_CUE_BTN_W) {
        if (m->button == 1) set_enabled(row, !s.row[row].enabled);
        return 1;
    }
    if (m->button == 1 && m->clicks == 2) {
        if (form_geti("DBCLICKSEQ", 1) && cue_rec() && cue_rec()->slot[row].seq) lj_log("lj_cue_form: DoubleClick to Edit Sequence %04u: the sequence loader is W2", (unsigned)cue_rec()->slot[row].seq);
        return 1;
    }
    if (m->ctrl) s.sel = (uint16_t)(s.sel ^ (1u << row));
    else if (!((s.sel >> row) & 1) || m->button == 1) s.sel = (uint16_t)(1u << row);
    lj_widget_invalidate(w);
    if (m->button == 2) slot_popup(row, m->x, m->y);
    return 1;
}

static const lj_widget_vt lj_cue_grid_vt = { grid_paint, grid_mouse, NULL, NULL };

static void grid_layout(void)
{
    lj_widget *root = lj_window_root(s.win), *panel = lj_window_find(s.win, "Panel1");
    int avail;
    if (!s.grid || !root) return;
    avail = (panel ? panel->y : root->h) - LJ_CUE_HDR_H;
    s.grid->base.x = 0; s.grid->base.y = 0; s.grid->base.w = root->w;
    s.grid->row_h = avail / LJ_CUE_SLOTS;
    if (s.grid->row_h > LJ_CUE_ROW_PITCH) s.grid->row_h = LJ_CUE_ROW_PITCH;
    if (s.grid->row_h < 8) s.grid->row_h = 8;
    s.grid->base.h = LJ_CUE_HDR_H + LJ_CUE_SLOTS * s.grid->row_h;
    lj_widget_invalidate(&s.grid->base);
}

/* ---- popup list (shared) ---- */
typedef struct { lj_widget base; const char *const *items; int n; int px, py, pw, ph, hot; void (*pick)(int, void *); void *ud; } lj_cue_popup;

static void popup_paint(lj_widget *w, lj_surface *sf)
{
    lj_cue_popup *p = (lj_cue_popup *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, i;
    lj_widget_abs(w, &ax, &ay);
    ax += p->px; ay += p->py;
    lj_fill(sf, ax, ay, p->pw, p->ph, LJ_CLR_BTNHIGHLIGHT);
    lj_bevel1(sf, ax, ay, p->pw, p->ph, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW);
    for (i = 0; i < p->n; i++) {
        int y = ay + 2 + i * LJ_MENU_ITEM_H, hot = i == p->hot;
        if (hot) lj_fill(sf, ax + 1, y, p->pw - 2, LJ_MENU_ITEM_H, LJ_CLR_HIGHLIGHT);
        lj_w_text(sf, w, ax + 6, y + (LJ_MENU_ITEM_H - f->height) / 2, p->items[i], hot ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW);
    }
}
static int popup_row(const lj_cue_popup *p, int x, int y)
{
    int r;
    if (x < p->px || x >= p->px + p->pw || y < p->py + 2 || y >= p->py + p->ph - 2) return -1;
    r = (y - p->py - 2) / LJ_MENU_ITEM_H;
    return r < p->n ? r : -1;
}
static int popup_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_cue_popup *p = (lj_cue_popup *)w;
    int r = popup_row(p, m->x, m->y);
    if (m->type == LJ_EV_MOUSE_MOVE) { if (r != p->hot) { p->hot = r; lj_widget_invalidate(w); } return 1; }
    if (m->type != LJ_EV_MOUSE_DOWN) return 1;
    w->visible = 0;
    lj_window_invalidate_all(lj_window_of(w));
    if (m->button == 1 && r >= 0 && p->pick) p->pick(r, p->ud);
    return 1;
}
static int popup_key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_ESCAPE) { w->visible = 0; lj_window_invalidate_all(lj_window_of(w)); return 1; }
    return 0;
}
static const lj_widget_vt lj_cue_popup_vt = { popup_paint, popup_mouse, popup_key, NULL };

void lj_cue_popup_open(lj_window *win, const char *const *items, int n, int x, int y, void (*pick)(int i, void *ud), void *ud)
{
    lj_widget *root = lj_window_root(win), *w;
    lj_cue_popup *p;
    const lj_font *f;
    int i, tw;
    if (!root || n <= 0) return;
    w = lj_window_find(win, "lj_cue_popup");
    if (!w) { w = lj_w_alloc(sizeof(lj_cue_popup), root, NULL, &lj_cue_popup_vt, 1); w->name = "lj_cue_popup"; }
    p = (lj_cue_popup *)w;
    f = lj_w_font_of(w);
    for (i = 0, tw = 60; i < n; i++) if (lj_text_width(f, items[i]) + 12 > tw) tw = lj_text_width(f, items[i]) + 12;
    p->items = items; p->n = n; p->pick = pick; p->ud = ud; p->hot = -1;
    p->pw = tw; p->ph = n * LJ_MENU_ITEM_H + 4;
    p->px = x + p->pw > root->w ? root->w - p->pw : x; p->py = y + p->ph > root->h ? root->h - p->ph : y;
    if (p->px < 0) p->px = 0;
    if (p->py < 0) p->py = 0;
    w->x = 0; w->y = 0; w->w = root->w; w->h = root->h; w->visible = 1;
    lj_widget_set_focus(w);
    lj_widget_invalidate(w);
}
int lj_cue_popup_is_open(lj_window *win) { lj_widget *w = lj_window_find(win, "lj_cue_popup"); return w && w->visible; }

/* ---- slot options popup [03 s5.5]: what W1 runs; Link to Cue waits for W2 ---- */
static const char *const lj_slot_items[] = {
    "Auto Trig", "Manual Trig",
    "Forward", "Reverse", "Bounce", "Random",
    "First Scene +1", "First Scene -1", "Last Scene +1", "Last Scene -1", "Loops +1", "Loops -1",
    "Stop on scene", "Sequence out", "Goto Scene", "Link to Cue (W2)",
    "Time: CUE", "Time: 50%", "Time: 200%",
    "Clear Slot(s)" };

static void for_selected(int fallback, void (*fn)(lj_cue_slot *sl, int slot, int arg), int arg)
{
    lj_cue *cue = cue_rec();
    int i, any = 0;
    if (!cue) return;
    lj_lock();
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (((s.sel >> i) & 1) || (fallback >= 0 && i == fallback && !s.sel)) { fn(&cue->slot[i], i, arg); any = 1; }
    lj_unlock();
    if (any) modified();
}
static void op_trig(lj_cue_slot *sl, int i, int a) { (void)i; sl->trig = (uint8_t)a; }
static void op_dir(lj_cue_slot *sl, int i, int a) { (void)i; sl->dir = (uint8_t)a; }
static void op_first(lj_cue_slot *sl, int i, int a) { int v = sl->first + a; (void)i; sl->first = (uint16_t)(v < 0 ? 0 : v > LJ_SCENES_MAX ? LJ_SCENES_MAX : v); }
static void op_last(lj_cue_slot *sl, int i, int a) { int v = sl->last + a; (void)i; sl->last = (uint16_t)(v < 0 ? 0 : v > LJ_SCENES_MAX ? LJ_SCENES_MAX : v); }
static void op_loops(lj_cue_slot *sl, int i, int a) { int v = sl->loops + a; (void)i; if (sl->dir != LJ_DIR_RANDOM) sl->loops = (uint16_t)(v < 0 ? 0 : v > 9999 ? 9999 : v); }   /* no loop count in random [03 s5.5] */
static void op_end(lj_cue_slot *sl, int i, int a) { (void)i; sl->end_action = (uint8_t)a; }
static void op_time(lj_cue_slot *sl, int i, int a) { (void)i; sl->time_pct = (uint16_t)a; }
/* commands for the slots an op touched, posted after the lock is released (lj_ctx_post takes it) */
static void post_scope(int fallback, int kind, uint32_t b, uint32_t c, int only_manual)
{
    const lj_cue *cue = cue_rec();
    int i;
    for (i = 0; i < LJ_CUE_SLOTS; i++)
        if ((((s.sel >> i) & 1) || (fallback >= 0 && i == fallback && !s.sel)) && cue && (!only_manual || cue->slot[i].trig == LJ_TRIG_MANUAL))
            post3(kind, (uint32_t)(LJ_RUN_SLOT0 + i), b, c);
}
static void op_clear(lj_cue_slot *sl, int i, int a) { (void)i; (void)a; memset(sl, 0, sizeof *sl); }

static void slot_pick(int item, void *ud)
{
    int slot = *(int *)ud;
    switch (item) {
    case 0: case 1:   for_selected(slot, op_trig, item == 0 ? LJ_TRIG_AUTO : LJ_TRIG_MANUAL); break;
    case 2: case 3: case 4: case 5: for_selected(slot, op_dir, item - 2); break;
    case 6: case 7:   for_selected(slot, op_first, item == 6 ? 1 : -1); break;
    case 8: case 9:   for_selected(slot, op_last, item == 8 ? 1 : -1); break;
    case 10: case 11: for_selected(slot, op_loops, item == 10 ? 1 : -1); break;
    case 12: case 13: case 14: for_selected(slot, op_end, item == 12 ? LJ_END_STOP : item == 13 ? LJ_END_OUT : LJ_END_GOTO); break;
    case 15:          lj_log("lj_cue_form: Link to Cue is W2"); break;
    case 16: case 17: case 18: for_selected(slot, op_time, item == 16 ? 0 : item == 17 ? 50 : 200); post_scope(slot, LJ_CMD_TIME, item == 16 ? 0u : item == 17 ? 50u : 200u, 0, 0); break;
    case 19:          for_selected(slot, op_clear, 0); break;
    default: break;
    }
    lj_cue_form_refresh();
}

static void slot_popup(int slot, int x, int y)
{
    s.popup_slot = slot;
    lj_cue_popup_open(s.win, lj_slot_items, LJ_COUNT_OF(lj_slot_items), x, y, slot_pick, &s.popup_slot);
}

/* ---- the cue record ---- */
int lj_cue_form_add_sequence(uint32_t seq_no)
{
    lj_cue *cue = cue_rec();
    int i;
    if (!cue || seq_no == 0) return -1;
    for (i = 0; i < LJ_CUE_SLOTS && cue->slot[i].seq; i++) { }   /* ponytail: lj_cue_find_free once the model body lands; the scan is three lines */
    if (i == LJ_CUE_SLOTS) { lj_log("lj_cue_form: Add Sequence to Current Cue: no free slot"); return -1; }
    lj_lock();
    memset(&cue->slot[i], 0, sizeof cue->slot[i]);
    cue->slot[i].seq = seq_no; cue->slot[i].enabled = 1;        /* trig auto, dir fwd, time CUE, no limits [03 s5.5 defaults] */
    lj_unlock();
    modified();
    lj_cue_form_refresh();
    return i + 1;
}

void lj_cue_form_new_cue(void)
{
    lj_cue *cue = cue_rec();
    if (!cue) return;
    lj_lock();
    memset(cue, 0, sizeof *cue);                                  /* the record stays (the engine may pin it, 2.3 rule 6); its contents go */
    cue->cue_time_pct = 100; cue->macro_amp_pct = 100; cue->macro_speed_pct = 100;
    lj_unlock();
    C()->cue_saved = 0;
    s.sel = 0;
    post3(LJ_CMD_TIME, 0, 100, 0);
    modified();
    lj_cue_form_refresh();
}

void lj_cue_form_set_cue_time(uint16_t pct)
{
    lj_cue *cue = cue_rec();
    if (!cue) return;
    if (pct < 1) pct = 1;
    if (pct > 999) pct = 999;
    lj_lock();
    cue->cue_time_pct = pct;
    lj_unlock();
    post3(LJ_CMD_TIME, 0, pct, 0);
    lj_ctx_set_modified(LJ_MOD_CUE, 1);
    lj_cue_form_refresh();
}

int lj_cue_form_save(int mode, uint32_t number)
{
    lj_ctx *c = C();
    lj_cue *cue = cue_rec(), *rec;
    char path[LJ_PATH_CAP];
    if (!cue || !c->lib) return -1;
    if (mode == 0) number = cue->number;
    else if (mode == 1) number = lj_library_next_number(c->lib, LJ_REC_CUE);
    if (number == 0 || number > LJ_NUMBER_MAX) { lj_log("lj_cue_form: save: no cue number"); return -1; }
    if (lj_library_record_path(c->lib, LJ_REC_CUE, number, path, sizeof path) != 0) return -1;
    cue->number = number;
    if (lj_cue_save(path, cue) != 0) { lj_log("lj_cue_form: save %s failed", path); return -1; }
    rec = (lj_cue *)lj_table_get(&c->show->cues, number);
    lj_lock();
    if (!rec) { rec = lj_cue_new_rec(); lj_table_put(&c->show->cues, number, rec); }
    *rec = *cue;                                                   /* a copy: New Cue clears the working record, not the library's */
    lj_unlock();
    c->cue_saved = 1;
    lj_ctx_set_modified(LJ_MOD_CUE, 0);
    lj_cue_form_refresh();
    return 0;
}

/* ---- bottom row buttons [03 s5.4] ---- */
static int any_sel(void) { return s.sel != 0; }
static int slot_in_scope(int i) { return !s.sel || (s.sel >> i & 1); }

static void trig_mode(int want_trig, uint32_t dir)
{
    const lj_cue *cue = cue_rec();
    int i;
    for (i = 0; i < LJ_CUE_SLOTS; i++)
        if (cue && cue->slot[i].seq && slot_in_scope(i) && cue->slot[i].trig == want_trig) post3(LJ_CMD_TRIG, (uint32_t)(LJ_RUN_SLOT0 + i), dir, 0);
}
static void go_stop(int kind)
{
    int i;
    if (!any_sel()) { post3(kind, LJ_RUN_ALL, 0, 0); return; }
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (s.sel >> i & 1) post3(kind, (uint32_t)(LJ_RUN_SLOT0 + i), 0, 0);
}
static void op_enable(lj_cue_slot *sl, int i, int a) { (void)i; sl->enabled = (uint8_t)a; }
static void enable_selected(int on) { for_selected(-1, op_enable, on); post_scope(-1, LJ_CMD_ENABLE, (uint32_t)on, 0, 0); }

static void select_all_toggle(void)
{
    lj_widget *b = lj_window_find(s.win, "sbSelect");
    s.sel = any_sel() ? 0 : 0x0FFF;
    if (b) lj_w_set_caption(b, any_sel() ? "Deselect All" : "Select All");   /* cuecontrol.gif, exe string 0x3518AC */
    if (s.grid) lj_widget_invalidate(&s.grid->base);
}

static void sb_down(lj_widget *w, void *ud)
{
    const char *n = w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "sbSelect")) select_all_toggle();
    else if (!strcmp(n, "sbTrigRev")) trig_mode(LJ_TRIG_MANUAL, LJ_DIR_REV);
    else if (!strcmp(n, "sbTrigFwd")) trig_mode(LJ_TRIG_MANUAL, LJ_DIR_FWD);
    else if (!strcmp(n, "sbTrigRnd")) trig_mode(LJ_TRIG_MANUAL, LJ_DIR_RANDOM);
    else if (!strcmp(n, "sbTrigAll")) trig_mode(LJ_TRIG_AUTO, 0xFF);
    else if (!strcmp(n, "sbStop")) go_stop(LJ_CMD_STOP);
    else if (!strcmp(n, "sbGo")) go_stop(LJ_CMD_GO);
    else if (!strcmp(n, "sbAutoTrig")) for_selected(-1, op_trig, LJ_TRIG_AUTO);
    else if (!strcmp(n, "sbManualTrig")) for_selected(-1, op_trig, LJ_TRIG_MANUAL);
    else if (!strcmp(n, "sbMusic")) for_selected(-1, op_trig, LJ_TRIG_AUDIO);
    else if (!strcmp(n, "sbDisable")) enable_selected(0);
    else if (!strcmp(n, "sbEnable")) enable_selected(1);
    lj_cue_form_refresh();
}

static void sb_up(lj_widget *w, void *ud)
{
    const char *n = w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "sbChaseMan")) post_scope(-1, LJ_CMD_CHASE, (uint32_t)lj_w_speedbutton_down(w), (uint32_t)s.chase_fade, 1);   /* chase: manual slots only [03 s5.5] */
    else if (!strcmp(n, "sbNewCue")) lj_cue_form_new_cue();
    else if (!strcmp(n, "sbSaveCue")) { if (lj_save_cue_get()) lj_save_cue_close(); else lj_save_cue_open(); }
    else if (!strcmp(n, "sbGetCue")) { if (lj_list_cue_get()) lj_list_cue_close(); else lj_list_cue_open(); }
    else if (!strcmp(n, "sbLoop")) { int i; for (i = 0; i < LJ_CUE_SLOTS && !((s.sel >> i) & 1); i++) { } slot_popup(i < LJ_CUE_SLOTS ? i : 0, w->x, 150); }
    else if (!strcmp(n, "sbCueTime")) lj_log("lj_cue_form: Cue Time Control window is W2 (lj_cue_form_set_cue_time carries the fader)");
    else lj_log("lj_cue_form: %s is W2", n);                      /* BGCue / generic macro group [03 s5.4] */
}

static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void w2(lj_widget *w, void *ud) { (void)ud; lj_log("lj_cue_form: %s: W2", w->name ? w->name : "?"); }

/* ---- menus ---- */
static const struct { const char *item, *key; int main; } lj_cue_prefs[] = {
    { "M_DragSequences", "DragSeq", 0 }, { "M_UseColors", "Use Colors", 0 }, { "M_DCSequenceEdit", "DBCLICKSEQ", 0 },
    { "M_StickyFlashSequences", "StickyFlash", 0 }, { "M_FlashAutoRelease", "Flash Auto Release", 0 },
    { "M_HotkeyChangeDirection", "HotkeyChangeSeqDir", 1 }, { "M_UseGlobalChaseRate", "UseGlobalChaseRate", 0 },
    { "M_DisableBGCueLatch", "BGCueLatch", 0 }, { "M_AutoRelease", "Auto Release", 0 }, { "M_PreserveEnabledState", "PreserveEnabledState", 0 },
    { "M_SaveChaseRates", "SaveChaseRate", 0 }, { "M_DisableChaseFade", "DisableChaseFade", 1 }, { "M_AllowRemoteOnOff", "AllowRemoteOnOff", 0 },
    { "M_RestoreMacroScale", "RestoreMacroScale", 0 }, { "M_ViewAdvanced", "ViewAdvanced", 0 },
};
static const char *const lj_cue_trans_items[] = { "M_UseTransparentCues", "M_ForceTransparentCues", "M_IgnoreTransparenCues" };
static const char *const lj_cue_int_items[] = { "M_CueIntDefault", "M_CueIntHTP", "M_CueIntLTP" };

static int pref_get(int i) { return lj_cue_prefs[i].main ? main_geti(lj_cue_prefs[i].key, 0) : form_geti(lj_cue_prefs[i].key, 0); }
static void pref_set(int i, int v) { if (lj_cue_prefs[i].main) main_seti(lj_cue_prefs[i].key, v); else form_seti(lj_cue_prefs[i].key, v); }

static void apply_advanced(void)
{
    lj_widget *p1 = lj_window_find(s.win, "Panel1"), *p2 = lj_window_find(s.win, "pOptions2"), *root = lj_window_root(s.win);
    int on = form_geti("ViewAdvanced", 1);
    if (!p1 || !p2 || !root) return;
    p2->visible = on != 0;
    p1->h = on ? 60 : 31;                                           /* AutoSize panel: one or two 29-px rows + bevels (DFM) */
    p1->y = root->h - p1->h;
    grid_layout();
    lj_window_invalidate_all(s.win);
}

static void sync_checks(void)
{
    lj_menu *m = lj_window_menus(s.win);
    int i, t;
    for (i = 0; i < LJ_COUNT_OF(lj_cue_prefs); i++) lj_menu_set_checked(m, lj_cue_prefs[i].item, pref_get(i));
    t = form_geti("Transparent", 0);
    for (i = 0; i < 3; i++) lj_menu_set_checked(m, lj_cue_trans_items[i], i == t);
    t = form_geti("SeqIntMode", 0);
    for (i = 0; i < 3; i++) lj_menu_set_checked(m, lj_cue_int_items[i], i == t);
    lj_menu_set_checked(m, "M_ShowasBPM", s.bpm);
    for (i = 0; i < 10; i++) {
        static const char *const names[10] = { "ShowSequenceNumber1", "ShowSequenceName1", "ShowSequenceType1", "ShowScene1", "ShowSceneime1", "ShowFadetime1", "ShowStatus1", "ShowLoop1", "ShowTrig1", "ShowLock1" };
        lj_menu_set_checked(m, names[i], (s.columns >> i) & 1);
    }
}

static void prefs_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    int i;
    (void)w; (void)ud;
    if (!it || !it->name) return;
    for (i = 0; i < LJ_COUNT_OF(lj_cue_prefs); i++) if (!strcmp(it->name, lj_cue_prefs[i].item)) { pref_set(i, !pref_get(i)); break; }
    for (i = 0; i < 3; i++) if (!strcmp(it->name, lj_cue_trans_items[i])) form_seti("Transparent", i);
    for (i = 0; i < 3; i++) if (!strcmp(it->name, lj_cue_int_items[i])) { form_seti("SeqIntMode", i); post3(LJ_CMD_SEQ_INT, 0, (uint32_t)i, 0); }
    s.use_colors = form_geti("Use Colors", 1);
    s.chase_fade = !main_geti("DisableChaseFade", 0);
    if (!strcmp(it->name, "M_ViewAdvanced")) apply_advanced();
    sync_checks();
    lj_cue_form_refresh();
}
static void prefs_open(lj_widget *w, void *ud) { (void)w; (void)ud; sync_checks(); }
static void bpm_click(lj_widget *w, void *ud) { (void)w; (void)ud; s.bpm = !s.bpm; form_seti("DisplayBPM", s.bpm); sync_checks(); lj_cue_form_refresh(); }
static void show_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    (void)w; (void)ud;
    if (!it) return;
    s.columns ^= (unsigned)it->tag;
    form_seti("Columns", (int)s.columns);
    sync_checks();
    if (s.grid) lj_widget_invalidate(&s.grid->base);
}
static void default_view(lj_widget *w, void *ud)
{
    int i;
    char key[4];
    (void)w; (void)ud;
    for (i = 0; i < LJ_CUE_COLS; i++) { snprintf(key, sizeof key, "C%d", i); form_seti(key, lj_cue_col_dflt[i]); }
    lj_cue_form_col_widths();
    if (s.grid) memcpy(s.grid->widths, s.col_w, sizeof s.col_w);
    s.columns = 65535;
    form_seti("Columns", 65535);
    sync_checks();
    if (s.grid) lj_widget_invalidate(&s.grid->base);
}
static void default_columns(lj_widget *w, void *ud) { (void)w; (void)ud; s.columns = 65535; form_seti("Columns", 65535); sync_checks(); if (s.grid) lj_widget_invalidate(&s.grid->base); }
static void menu_options(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    (void)w; (void)ud;
    if (!it) return;
    if (it->tag == 1) lj_cue_form_new_cue();
    else if (it->tag == 2) { s.sel = 0x0FFF; for_selected(-1, op_clear, 0); s.sel = 0; lj_cue_form_refresh(); }
}
static void on_resize(lj_widget *w, void *ud) { (void)w; (void)ud; grid_layout(); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; }

/* ---- open / close / refresh ---- */
static const lj_binding lj_cue_bindings[] = {
    { "sbMouseDown", sb_down, NULL }, { "sbMouseUp", sb_up, NULL }, { "sbDeleteBGCueMouseUp", w2, NULL }, { "sbDeleteMacroClick", w2, NULL },
    { "eBGCueChange", noop, NULL }, { "eBGCueDragDrop", w2, NULL }, { "eBGCueDragOver", noop, NULL },
    { "eMacroChange", noop, NULL }, { "eMacroDblClick", w2, NULL }, { "eMacroDragDrop", w2, NULL }, { "eMacroDragOver", noop, NULL },
    { "MenuOptions", menu_options, NULL }, { "DefaultView1Click", default_view, NULL }, { "DefaultVisibleColumns1Click", default_columns, NULL },
    { "ShowClick", show_click, NULL }, { "PrefsClick", prefs_click, NULL }, { "SlotsNames1Click", w2, NULL }, { "M_ShowasBPMClick", bpm_click, NULL },
    { "Preferences1Click", prefs_open, NULL }, { "CueBuilder1Click", w2, NULL }, { "Help1Click", noop, NULL },
    { "FormActivate", noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL },
    { "FormHide", noop, NULL }, { "FormResize", on_resize, NULL }, { "FormShow", noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_cue_form_open(void)
{
    lj_widget *root;
    int i;
    if (s.win) return s.win;
    s.win = lj_window_create(&lj_form_TCURRENTCUE, lj_cue_bindings);
    if (!s.win) { lj_log("lj_cue_form: TCurrentCue failed to build"); return NULL; }
    root = lj_window_root(s.win);
    s.columns = (unsigned)form_geti("Columns", 65535);
    s.use_colors = form_geti("Use Colors", 1);
    s.bpm = form_geti("DisplayBPM", 0);
    s.chase_fade = !main_geti("DisableChaseFade", 0);
    lj_cue_form_col_widths();
    s.grid = lj_w_alloc(sizeof(lj_cue_grid), root, NULL, &lj_cue_grid_vt, 0);
    s.grid->base.name = "lj_cue_grid";
    for (i = 0; i < LJ_CUE_COLS; i++) { s.grid->cols[i].caption = lj_cue_col_names[i]; s.grid->cols[i].width = s.col_w[i]; s.grid->cols[i].image_index = -1; s.grid->widths[i] = s.col_w[i]; }
    apply_advanced();
    sync_checks();
    lj_cue_form_refresh();
    host_show(s.win, 1);
    return s.win;
}

void lj_cue_form_close(void)
{
    if (!s.win) return;
    host_show(s.win, 0);
    lj_window_destroy(s.win);
    s.win = NULL; s.grid = NULL; s.sel = 0;
}

void lj_cue_form_refresh(void)
{
    lj_cue_row rows[LJ_CUE_SLOTS];
    char cap[128];
    int i;
    for (i = 0; i < LJ_CUE_SLOTS; i++) build_row(i, &rows[i]);
    if (memcmp(rows, s.row, sizeof rows) != 0) { memcpy(s.row, rows, sizeof rows); if (s.grid) lj_widget_invalidate(&s.grid->base); }
    if (!s.win) return;
    if (lj_cue_form_caption(cap, sizeof cap) == 0 && strcmp(cap, s.caption) != 0) { lj_str_copy(s.caption, sizeof s.caption, cap); lj_window_set_caption(s.win, cap); }
}

/* ---- hotkeys (io/lj_actions.h cue-control rows) ---- */
static void fn_cue(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    int i;
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn == LJ_FN_TOGGLE_CUE_CONTROL) lj_win_toggle(LJ_WIN_CUE_FORM);
    else if (fn == LJ_FN_ALL_SEQUENCES_STOP) post3(LJ_CMD_STOP, LJ_RUN_ALL, 0, 0);
    else if (fn == LJ_FN_ALL_SEQUENCES_GO) post3(LJ_CMD_GO, LJ_RUN_ALL, 0, 0);
    else if (fn == LJ_FN_NEW_CLEAR_CUE) lj_cue_form_new_cue();
    else if (fn == LJ_FN_TOGGLE_SELECT_ALL) { if (s.win) select_all_toggle(); else s.sel = s.sel ? 0 : 0x0FFF; }
    else if (fn == LJ_FN_ALL_SLOTS_OFF || fn == LJ_FN_ALL_SLOTS_ON) { uint16_t keep = s.sel; s.sel = 0x0FFF; enable_selected(fn == LJ_FN_ALL_SLOTS_ON); s.sel = keep; }
    else if (fn == LJ_FN_QUICK_SAVE_CUE) lj_cue_form_save(cue_rec() && cue_rec()->number ? 0 : 1, 0);
    else for (i = 0; i < LJ_CUE_SLOTS; i++) {
        if (fn == (enum lj_fn)(LJ_FN_SEQ_STOP_1 + i)) post3(LJ_CMD_STOP, (uint32_t)(LJ_RUN_SLOT0 + i), 0, 0);
        else if (fn == (enum lj_fn)(LJ_FN_SEQ_GO_1 + i)) post3(LJ_CMD_GO, (uint32_t)(LJ_RUN_SLOT0 + i), 0, 0);
        else if (fn == (enum lj_fn)(LJ_FN_TOGGLE_SLOT_ENABLE_1 + i)) set_enabled(i, !s.row[i].enabled);
        else if (fn == (enum lj_fn)(LJ_FN_TOGGLE_SLOT_SELECTION_1 + i)) lj_cue_form_select(i, !lj_cue_form_selected(i));
        else continue;
        break;
    }
    lj_cue_form_refresh();
}

void lj_cue_form_bind_fns(void)
{
    static const enum lj_fn one[] = { LJ_FN_TOGGLE_CUE_CONTROL, LJ_FN_ALL_SEQUENCES_STOP, LJ_FN_ALL_SEQUENCES_GO, LJ_FN_NEW_CLEAR_CUE,
                                      LJ_FN_TOGGLE_SELECT_ALL, LJ_FN_ALL_SLOTS_OFF, LJ_FN_ALL_SLOTS_ON, LJ_FN_QUICK_SAVE_CUE };
    int i;
    for (i = 0; i < LJ_COUNT_OF(one); i++) lj_fn_bind(one[i], fn_cue, NULL);
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        lj_fn_bind((enum lj_fn)(LJ_FN_SEQ_STOP_1 + i), fn_cue, NULL);
        lj_fn_bind((enum lj_fn)(LJ_FN_SEQ_GO_1 + i), fn_cue, NULL);
        lj_fn_bind((enum lj_fn)(LJ_FN_TOGGLE_SLOT_ENABLE_1 + i), fn_cue, NULL);
        lj_fn_bind((enum lj_fn)(LJ_FN_TOGGLE_SLOT_SELECTION_1 + i), fn_cue, NULL);
    }
}

/* ======================= Save Cue (TSaveCueForm, TSaveAsCueForm) [03 s5.7] ======================= */
static struct { lj_window *win, *as; } sv;

static int sv_geti(const char *key, int dflt) { lj_ctx *c = C(); return c->forms_ini ? lj_ini_geti(c->forms_ini, "SAVECUEFORM", key, dflt) : dflt; }
static void sv_seti(const char *key, int v) { lj_ctx *c = C(); if (c->forms_ini) lj_ini_seti(c->forms_ini, "SAVECUEFORM", key, v); }

static void as_changed(lj_widget *w, void *ud);
static void as_save(lj_widget *w, void *ud);
static void as_activate(lj_widget *w, void *ud);
static const lj_binding as_bindings[] = {
    { "eCueNoChange", as_changed, NULL }, { "bSaveClick", as_save, NULL }, { "FormActivate", as_activate, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { NULL, NULL, NULL } };

static void sv_fill(void)
{
    const lj_cue *cue = cue_rec();
    lj_widget *w;
    char num[8];
    if (!sv.win) return;
    snprintf(num, sizeof num, "%04u", cue ? (unsigned)cue->number : 0u);
    if ((w = lj_window_find(sv.win, "CueNo")) != NULL) lj_w_edit_set_text(w, cue && cue->number ? num : "");   /* blank until the first save */
    if ((w = lj_window_find(sv.win, "ECueName")) != NULL && !w->focused) lj_w_edit_set_text(w, cue ? cue->name : "");   /* FormActivate re-runs this: a name being typed is not overwritten */
    if ((w = lj_window_find(sv.win, "Bsave")) != NULL) { w->enabled = cue && cue->number; lj_widget_invalidate(w); }
    if ((w = lj_window_find(sv.win, "cbTransparent")) != NULL) lj_w_checkbox_set_checked(w, cue ? cue->transparent : sv_geti("SaveTransparent", 0));
    if ((w = lj_window_find(sv.win, "cbSaveMacro")) != NULL) lj_w_checkbox_set_checked(w, sv_geti("SaveMacro", 0));
    if ((w = lj_window_find(sv.win, "cbClearSave")) != NULL) lj_w_checkbox_set_checked(w, sv_geti("ClearSave", 0));
    lj_menu_set_checked(lj_window_menus(sv.win), "AutoClose", sv_geti("AutoClose", 1));
    lj_menu_set_checked(lj_window_menus(sv.win), "EnterSave", sv_geti("SaveOnEnter", 1));
}

static void sv_take_fields(void)
{
    lj_cue *cue = cue_rec();
    lj_widget *w;
    if (!cue || !sv.win) return;
    lj_lock();
    if ((w = lj_window_find(sv.win, "ECueName")) != NULL) lj_str_copy(cue->name, sizeof cue->name, lj_w_edit_text(w));
    if ((w = lj_window_find(sv.win, "cbTransparent")) != NULL) cue->transparent = (uint8_t)lj_w_checkbox_checked(w);
    if ((w = lj_window_find(sv.win, "cbSaveMacro")) != NULL) cue->save_macro_values = (uint8_t)lj_w_checkbox_checked(w);
    lj_unlock();
}

static void sv_after_save(void)
{
    lj_widget *w;
    if (!sv.win) return;
    if ((w = lj_window_find(sv.win, "cbClearSave")) != NULL && lj_w_checkbox_checked(w)) lj_cue_form_new_cue();
    if (sv_geti("AutoClose", 1)) lj_save_cue_close(); else sv_fill();
}

static void sv_save_click(lj_widget *w, void *ud)
{
    const char *n = w->name ? w->name : "";
    (void)ud;
    sv_take_fields();
    if (!strcmp(n, "bSaveAs")) { if (!sv.as) { sv.as = lj_window_create(&lj_form_TSAVEASCUEFORM, as_bindings); host_show(sv.as, 1); } return; }
    if (lj_cue_form_save(!strcmp(n, "Bsave") ? 0 : 1, 0) == 0) sv_after_save();
}
static void sv_check_click(lj_widget *w, void *ud)
{
    const char *n = w->name ? w->name : "";
    (void)ud;
    sv_seti(!strcmp(n, "cbTransparent") ? "SaveTransparent" : "SaveMacro", lj_w_checkbox_checked(w));   /* persists between sessions [RN v2.5 b5] */
}
static void sv_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sv.win);
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "AutoClose")) sv_seti("AutoClose", !sv_geti("AutoClose", 1));
    else if (!strcmp(it->name, "EnterSave")) sv_seti("SaveOnEnter", !sv_geti("SaveOnEnter", 1));
    sv_fill();
}
static void sv_keydown(lj_widget *w, void *ud) { (void)w; (void)ud; }   /* Save on [Enter]: the key is not readable from a binding until W0-last-event lands */
static void sv_close(lj_widget *w, void *ud)
{
    lj_widget *cb;
    (void)w; (void)ud;
    if (sv.win && (cb = lj_window_find(sv.win, "cbClearSave")) != NULL) sv_seti("ClearSave", lj_w_checkbox_checked(cb));
}

static const lj_binding sv_bindings[] = {
    { "cbTransparentClick", sv_check_click, NULL }, { "BsaveClick", sv_save_click, NULL }, { "PrefsClick", sv_prefs, NULL }, { "Help1Click", noop, NULL },
    { "FormClose", sv_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormHide", noop, NULL },
    { "FormKeyDown", sv_keydown, NULL }, { "FormShow", noop, NULL }, { NULL, NULL, NULL } };

/* Save As Cue# */
static void as_changed(lj_widget *w, void *ud)
{
    lj_widget *b = lj_window_find(sv.as, "bSave");
    long n = strtol(lj_w_edit_text(w), NULL, 10);
    (void)ud;
    if (b) { b->enabled = n > 0 && n <= 9999; lj_widget_invalidate(b); }
}
static void as_save(lj_widget *w, void *ud)
{
    lj_widget *e = lj_window_find(sv.as, "eCueNo");
    long n = e ? strtol(lj_w_edit_text(e), NULL, 10) : 0;
    (void)w; (void)ud;
    if (n <= 0) return;
    if (lj_cue_form_save(2, (uint32_t)n) == 0) { host_show(sv.as, 0); lj_window_destroy(sv.as); sv.as = NULL; sv_after_save(); }   /* overwrites silently [F28] */
}
static void as_activate(lj_widget *w, void *ud)
{
    lj_widget *e;
    (void)w; (void)ud;
    if (sv.as && (e = lj_window_find(sv.as, "ECueName")) != NULL) lj_w_edit_set_text(e, cue_rec() ? cue_rec()->name : "");
}

lj_window *lj_save_cue_open(void)
{
    if (sv.win) return sv.win;
    sv.win = lj_window_create(&lj_form_TSAVECUEFORM, sv_bindings);
    if (!sv.win) { lj_log("lj_cue_form: TSaveCueForm failed to build"); return NULL; }
    sv_fill();
    host_show(sv.win, 1);
    return sv.win;
}
void lj_save_cue_close(void)
{
    if (sv.as) { host_show(sv.as, 0); lj_window_destroy(sv.as); sv.as = NULL; }
    if (!sv.win) return;
    sv_close(NULL, NULL);
    host_show(sv.win, 0);
    lj_window_destroy(sv.win);
    sv.win = NULL;
}
lj_window *lj_save_cue_get(void) { return sv.win; }

/* ======================= List of Cues (TAdvancedCueForm) [03 s5.8] ======================= */
static struct {
    lj_window *win;
    char **items; uint32_t *numbers; int n;
    void (*lv_event)(lj_widget *w, int ev, void *ud); void *lv_ud;   /* the window's own dispatch, wrapped for the drag source */
} lc;

static void lc_free_items(void)
{
    int i;
    for (i = 0; i < lc.n; i++) free(lc.items[i]);
    free(lc.items); free(lc.numbers);
    lc.items = NULL; lc.numbers = NULL; lc.n = 0;
}

static int lc_selected_number(uint32_t *out)
{
    lj_widget *lv = lc.win ? lj_window_find(lc.win, "lvGlobal") : NULL;
    int i = lv ? ((lj_w_listview *)lv)->index : -1;
    if (i < 0 || i >= lc.n) return -1;
    *out = lc.numbers[i];
    return 0;
}
static void lc_load(uint32_t number, int edit)
{
    if (number == 0) return;
    if (!s.has_host || !s.host.load_cue) { lj_log("lj_cue_form: load cue %04u: no loader (headless)", (unsigned)number); return; }
    if (s.host.load_cue(s.host.ud, number, edit) != 0) lj_log("lj_cue_form: load cue %04u failed", (unsigned)number);
}

void lj_list_cue_refresh(void)
{
    lj_ctx *c = C();
    lj_widget *lv;
    uint32_t nums[9999];
    int n = 0, i;
    if (!lc.win) return;
    lc_free_items();
    if (c->lib) n = lj_library_list_records(c->lib, LJ_REC_CUE, nums, LJ_COUNT_OF(nums));
    if (n < 0) n = 0;
    lc.items = (char **)calloc((size_t)(n ? n : 1), sizeof *lc.items);
    lc.numbers = (uint32_t *)calloc((size_t)(n ? n : 1), sizeof *lc.numbers);
    if (!lc.items || !lc.numbers) lj_fatal("lj_list_cue_refresh: out of memory");
    for (i = 0; i < n; i++) {
        lj_cue *rec = (lj_cue *)lj_table_get(&c->show->cues, nums[i]);
        char path[LJ_PATH_CAP], err[128];
        if (!rec && lj_library_record_path(c->lib, LJ_REC_CUE, nums[i], path, sizeof path) == 0) {
            rec = lj_cue_new_rec();
            if (lj_cue_load(path, rec, err, sizeof err) == 0) { lj_lock(); lj_table_put(&c->show->cues, nums[i], rec); lj_unlock(); }
            else { lj_cue_free(rec); rec = NULL; }
        }
        lc.items[i] = (char *)malloc(80);
        if (!lc.items[i]) lj_fatal("lj_list_cue_refresh: out of memory");
        snprintf(lc.items[i], 80, "%s%04u\t%s\t", rec && rec->transparent ? "(T)" : "", (unsigned)nums[i], rec ? rec->name : "load error");   /* columns Cue# | Cue Name | Modified; transparent cues prefixed "(T)" [03 s5.8] */
        lc.numbers[i] = nums[i];
    }
    lc.n = n;
    if ((lv = lj_window_find(lc.win, "lvGlobal")) != NULL) lj_w_listview_set_items(lv, (const char *const *)lc.items, n);
}

static void lc_lv_event(lj_widget *w, int ev, void *ud)
{
    uint32_t number;
    (void)ud;
    if (lc.lv_event) lc.lv_event(w, ev, lc.lv_ud);
    if (ev == LJ_WEV_CLICK && lc_selected_number(&number) == 0) lj_dnd_begin(w, LJ_REC_CUE, (int)number);   /* drag source: DragMode = dmAutomatic */
}
static void lc_dblclick(lj_widget *w, void *ud) { uint32_t n; (void)w; (void)ud; if (lc_selected_number(&n) == 0) lc_load(n, 0); }
static void lc_pop_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lc.win);
    uint32_t n;
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "P_LoadCue")) { if (lc_selected_number(&n) == 0) lc_load(n, 0); }
    else if (!strcmp(it->name, "P_EditCue")) { if (lc_selected_number(&n) == 0) lc_load(n, 1); }
    else lj_log("lj_cue_form: %s: W2", it->name);
}
static void lc_num_change(lj_widget *w, void *ud)
{
    long n = strtol(lj_w_edit_text(w), NULL, 10);
    lj_widget *b;
    (void)ud;
    if ((b = lj_window_find(lc.win, "bGlobalGo")) != NULL) { b->enabled = n > 0; lj_widget_invalidate(b); }
    if ((b = lj_window_find(lc.win, "bCueNumGo")) != NULL) { b->enabled = n > 0; lj_widget_invalidate(b); }
}
static void lc_go(lj_widget *w, void *ud)
{
    lj_widget *e = lj_window_find(lc.win, "eGlobalCueNum");
    long n = e ? strtol(lj_w_edit_text(e), NULL, 10) : 0;
    (void)w; (void)ud;
    if (n > 0) lc_load((uint32_t)n, 0);
}
static void lc_show(lj_widget *w, void *ud) { (void)w; (void)ud; lj_list_cue_refresh(); }

static const lj_binding lc_bindings[] = {
    { "eGlobalCueNumChange", lc_num_change, NULL }, { "lvGlobalChange", noop, NULL }, { "lvPagesChange", noop, NULL }, { "TC1Change", noop, NULL }, { "TC1Changing", noop, NULL },
    { "bCueNumGoClick", lc_go, NULL }, { "bGoCueClick", lc_go, NULL }, { "lvGlobalColumnClick", noop, NULL }, { "lvGlobalCompare", noop, NULL },
    { "lvGlobalDblClick", lc_dblclick, NULL }, { "lvPagesDblClick", w2, NULL }, { "lvPagesDragDrop", w2, NULL }, { "lvPagesDragOver", noop, NULL },
    { "lvGlobalExit", noop, NULL }, { "lvPagesExit", noop, NULL },
    { "sbNextMouseDown", w2, NULL }, { "sbNextPageMouseDown", w2, NULL }, { "sbPageGoMouseDown", w2, NULL }, { "sbPrevMouseDown", w2, NULL }, { "sbPrevPageMouseDown", w2, NULL },
    { "sbPageButtonsMouseUp", noop, NULL }, { "Splitter1Moved", noop, NULL }, { "GlobalPopClick", lc_pop_click, NULL }, { "PagesPopClick", w2, NULL },
    { "GlobalColumnClick", w2, NULL }, { "GlobalSortClick", w2, NULL }, { "Help1Click", noop, NULL }, { "LoadPages1Click", w2, NULL },
    { "M_GlobalColumnsClick", noop, NULL }, { "M_GlobalColumnsDefaultClick", w2, NULL }, { "M_PagesClick", noop, NULL }, { "PageMenuClick", w2, NULL },
    { "PrefsClick", w2, NULL }, { "SavePages1Click", w2, NULL }, { "Sort1Click", noop, NULL },
    { "FormActivate", noop, NULL }, { "FormClose", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormDeactivate", noop, NULL },
    { "FormHide", noop, NULL }, { "FormKeyPress", noop, NULL }, { "FormResize", noop, NULL }, { "FormShow", lc_show, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_list_cue_open(void)
{
    lj_widget *lv;
    if (lc.win) return lc.win;
    lc.win = lj_window_create(&lj_form_TADVANCEDCUEFORM, lc_bindings);
    if (!lc.win) { lj_log("lj_cue_form: TAdvancedCueForm failed to build"); return NULL; }
    if ((lv = lj_window_find(lc.win, "lvGlobal")) != NULL) { lc.lv_event = lv->on_event; lc.lv_ud = lv->ud; lv->on_event = lc_lv_event; }
    lj_list_cue_refresh();
    host_show(lc.win, 1);
    return lc.win;
}
void lj_list_cue_close(void)
{
    if (!lc.win) return;
    host_show(lc.win, 0);
    lj_window_destroy(lc.win);
    lc.win = NULL;
    lc_free_items();
}
lj_window *lj_list_cue_get(void) { return lc.win; }
