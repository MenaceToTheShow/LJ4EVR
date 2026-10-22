/* lj_selection.c - the desktop click rules of [01 s3.2] (CHM "Selecting fixtures on the desktop"). */
#include "desktop/lj_selection.h"

#include <string.h>
#include "fixlib/lj_profile.h"
#include "model/lj_show.h"

static uint8_t  lj_selected[LJ_FIXTURES];
static int      lj_master = -1, lj_mode = LJ_SEL_INCLUSIVE, lj_solo;
static uint32_t lj_serial;

static int used(int i)
{
    const lj_ctx *ctx = lj_ctx_get();
    return i >= 0 && i < LJ_FIXTURES && ctx->show && ctx->show->fixture[i].link != 0;
}

static void bump(void) { lj_serial++; lj_desktop_icons_refresh(); lj_desktop_toolbar_update(); }

int lj_sel_master(void) { return lj_master; }
int lj_sel_is_selected(int i) { return i >= 0 && i < LJ_FIXTURES ? lj_selected[i] : 0; }
int lj_sel_mode(void) { return lj_mode; }
void lj_sel_mode_set(enum lj_sel_mode mode) { lj_mode = mode == LJ_SEL_EXCLUSIVE ? LJ_SEL_EXCLUSIVE : LJ_SEL_INCLUSIVE; bump(); }
int lj_sel_solo(void) { return lj_solo; }
void lj_sel_solo_set(int on) { lj_solo = on != 0; bump(); }
uint32_t lj_sel_serial(void) { return lj_serial; }

int lj_sel_count(void)
{
    int i, n = 0;
    for (i = 0; i < LJ_FIXTURES; i++) n += lj_selected[i];
    return n;
}

static void pick_master(void)   /* the master left the selection: the lowest selected fixture takes over, else none */
{
    int i;
    lj_master = -1;
    for (i = 0; i < LJ_FIXTURES; i++) if (lj_selected[i]) { lj_master = i; return; }
}

void lj_sel_set(int i, int selected, int make_master)
{
    if (!used(i)) return;
    lj_selected[i] = (uint8_t)(selected != 0);
    if (selected && (make_master || lj_master < 0)) lj_master = i;   /* "Only one fixture can be master fixture at any time" */
    else if (!selected && lj_master == i) pick_master();
    bump();
}

static void clear_all(void) { memset(lj_selected, 0, sizeof lj_selected); lj_master = -1; }

void lj_sel_click(int i, int shift, int ctrl)
{
    if (!used(i)) return;
    if (ctrl) { clear_all(); lj_selected[i] = 1; lj_master = i; bump(); return; }      /* Ctrl+click: solo fixture select */
    if (shift) { lj_selected[i] = 1; lj_master = i; bump(); return; }                 /* Shift+click: force master */
    if (lj_selected[i]) { lj_selected[i] = 0; if (lj_master == i) pick_master(); bump(); return; }   /* a selected icon de-selects */
    if (lj_mode == LJ_SEL_EXCLUSIVE) clear_all();
    lj_selected[i] = 1; lj_master = i;                                                 /* "This fixture now becomes the 'Master fixture'" */
    bump();
}

void lj_sel_band(int x0, int y0, int x1, int y1, int ctrl)
{
    int i, caught = 0, first = -1;
    lj_rect r;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    if (ctrl || lj_mode == LJ_SEL_EXCLUSIVE) clear_all();
    for (i = 0; i < LJ_FIXTURES; i++) {
        int in;
        if (!used(i) || !lj_ctx_get()->show->fixture[i].placed) continue;
        lj_desktop_icon_rect(i, &r);
        if (r.w <= 0) continue;
        in = ctrl ? r.x >= x0 && r.y >= y0 && r.x + r.w <= x1 && r.y + r.h <= y1                /* Ctrl: "within rubber band frame only" */
                  : r.x < x1 && r.x + r.w > x0 && r.y < y1 && r.y + r.h > y0;
        if (!in) continue;
        lj_selected[i] = 1; caught++;
        if (first < 0) first = i;
    }
    if (!caught) clear_all();                              /* "Rubber-band empty desktop deselects all" */
    else if (lj_master < 0 || !lj_selected[lj_master]) lj_master = first;
    bump();
}

void lj_sel_all(void)
{
    int i;
    for (i = 0; i < LJ_FIXTURES; i++) if (used(i)) lj_selected[i] = 1;
    if (lj_master < 0 || !lj_selected[lj_master]) pick_master();
    bump();
}

void lj_sel_none(void) { clear_all(); bump(); }

static void step(int dir)   /* [01 s3.3] default rule: by fixture number from the master; nothing selected -> lowest (Next) / highest (Previous) */
{
    int i, n = LJ_FIXTURES, from = lj_master, pick = -1;
    for (i = 1; i <= n; i++) {
        int k = from < 0 ? (dir > 0 ? i - 1 : n - i) : ((from + dir * i) % n + n) % n;
        if (used(k)) { pick = k; break; }
    }
    if (pick < 0) return;
    clear_all();
    lj_selected[pick] = 1; lj_master = pick;
    bump();
}
void lj_sel_next(void) { step(1); }
void lj_sel_prev(void) { step(-1); }

const lj_profile *lj_sel_master_profile(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    if (!used(lj_master)) return NULL;
    return lj_show_profile(ctx->show, lj_master);                      /* built-in or GS2 (ME-110) */
}

int lj_sel_same_type(int a, int b)
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_fixture *fa, *fb;
    if (!used(a) || !used(b)) return 0;
    fa = &ctx->show->fixture[a]; fb = &ctx->show->fixture[b];
    return fa->kind == fb->kind && fa->profile_id == fb->profile_id && strcmp(fa->gs2_file, fb->gs2_file) == 0;
}

void lj_sel_reset(void) { clear_all(); lj_mode = LJ_SEL_INCLUSIVE; lj_solo = 0; bump(); }
