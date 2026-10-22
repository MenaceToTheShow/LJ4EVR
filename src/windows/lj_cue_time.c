/* lj_cue_time.c - TCueTimeForm behaviour and the 13 code-built faders (lj_cue_time.h). */
#include "windows/lj_cue_time.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_windows.h"

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */

/* Fader geometry inside Bevel1 (DFM 657x168). cuetime.gif is a reduced capture (402x201 of a 670x309
 * window, not in 07 s14's 1:1 list), so every number here is laid out on the DFM bevel, not scaled
 * from the GIF: PROPOSAL rig 07 s14 (a 1:1 capture of the Time Control window). */
#define LJ_CT_X0      6    /* PROPOSAL rig 07 s14: first column left */
#define LJ_CT_PITCH  50    /* PROPOSAL rig 07 s14: 13 columns across the 657-px bevel */
#define LJ_CT_COL_W  36    /* PROPOSAL rig 07 s14 */
#define LJ_CT_LBL_Y   3    /* "Cue", "#1".."#12" */
#define LJ_CT_BTN_H  14    /* the up / down end caps */
#define LJ_CT_UP_Y   18
#define LJ_CT_TRK_Y  (LJ_CT_UP_Y + LJ_CT_BTN_H)
#define LJ_CT_BOX_H  17
#define LJ_CT_FOOT   (LJ_CT_BTN_H + 2 + LJ_CT_BOX_H + 5)   /* down cap, gap, value box, bottom margin */
/* the faders fill the client (cuetime.gif: label row to value boxes span the whole window), so the track takes
 * what the window height leaves (PROPOSAL rig 07 s14) */
static int trk_h(const lj_widget *w) { int h = w->h - LJ_CT_TRK_Y - LJ_CT_FOOT; return h < 20 ? 20 : h; }

#define LJ_CT_CYAN   0x00FFFFu   /* the cue fader track (cuetime.gif, colour only) */
#define LJ_CT_LOCKED 0x000000u   /* a slot with its own time ("Locked times") */
#define LJ_CT_FREE   0xC0C0C0u   /* a slot following the cue fader */
#define LJ_CT_UNUSED 0xE0E0E0u   /* an empty slot: light grey [03 s5.6] */
#define LJ_CT_FILL   0x0000FFu
#define LJ_CT_TICK   0x00FF00u   /* the green centre tick = 100 % */

#define LJ_VK_PRIOR 0x21
#define LJ_VK_NEXT  0x22
#define LJ_VK_UP    0x26
#define LJ_VK_DOWN  0x28

typedef struct { lj_widget base; int press; } lj_ct_strip;

static struct { lj_window *win; lj_ct_strip *strip; uint16_t sel; int view, sync; lj_cue shown; char caption[64]; lj_cmd last; int posts; } ct;

static lj_ctx *C(void) { return lj_ctx_get(); }
static lj_cue *cue_rec(void) { lj_ctx *c = C(); return c->show ? c->show->current_cue : NULL; }
static int  form_geti(const char *key, int dflt) { lj_ctx *c = C(); return c->forms_ini ? lj_ini_geti(c->forms_ini, "CUETIMEFORM", key, dflt) : dflt; }
static void form_seti(const char *key, int v) { lj_ctx *c = C(); if (c->forms_ini) lj_ini_seti(c->forms_ini, "CUETIMEFORM", key, v); }
lj_window *lj_cue_time_get(void) { return ct.win; }
int lj_cue_time_last_cmd(lj_cmd *out) { if (out && ct.posts) *out = ct.last; return ct.posts; }

/* ---- the scale: bottom = 1 %, centre = 100 %, top = 999 % (two linear halves; PROPOSAL rig 03 s5.6, the
 * help gives only the range and "100% = CUE") ---- */
int lj_cue_time_pos_of(int pct, int h)
{
    int half = (h - 1) / 2;
    if (h < 3) return 0;
    if (pct < 1) pct = 100;                                   /* "Cue": drawn at the centre */
    if (pct > 999) pct = 999;
    if (pct <= 100) return (pct - 1) * half / 99;
    return half + (pct - 100) * (h - 1 - half) / 899;
}
int lj_cue_time_pct_at(int pos, int h)
{
    int half = (h - 1) / 2;
    if (h < 3) return 100;
    if (pos <= 0) return 1;
    if (pos >= h - 1) return 999;
    if (pos <= half) return 1 + pos * 99 / half;
    return 100 + (pos - half) * 899 / (h - 1 - half);
}

int lj_cue_time_value(int f)
{
    const lj_cue *cue = cue_rec();
    if (!cue || f < 0 || f >= LJ_CUE_TIME_FADERS) return -1;
    return f == 0 ? cue->cue_time_pct : cue->slot[f - 1].time_pct;
}
int lj_cue_time_text(int f, char *dst, size_t cap)
{
    int v = lj_cue_time_value(f), n;
    if (!dst || cap == 0) return -1;
    if (v < 0) { dst[0] = 0; return -1; }
    n = (f == 0 ? v == 100 : v == LJ_TIME_PCT_CUE) ? snprintf(dst, cap, "Cue") : snprintf(dst, cap, "%d%%", v);   /* cuetime.gif: "Cue", "240%" */
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

static void post_time(int f, int pct)
{
    lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_CUE_TIME; c.a = (uint32_t)f; c.b = (uint32_t)pct;
    ct.last = c; ct.posts++;
    lj_ctx_post(&c);
}
static int set_one(lj_cue *cue, int f, int pct)
{
    uint16_t *p = f == 0 ? &cue->cue_time_pct : &cue->slot[f - 1].time_pct;
    int lo = f == 0 ? LJ_TIME_PCT_MIN : LJ_TIME_PCT_CUE;
    if (pct < lo) pct = lo;
    if (pct > LJ_TIME_PCT_MAX) pct = LJ_TIME_PCT_MAX;
    if (*p == pct) return 0;
    lj_lock(); *p = (uint16_t)pct; lj_unlock();
    post_time(f, pct);
    return 1;
}
int lj_cue_time_set(int f, int pct)
{
    lj_cue *cue = cue_rec();
    int i, any;
    if (!cue || f < 0 || f >= LJ_CUE_TIME_FADERS) return -1;
    any = set_one(cue, f, pct);
    if (ct.sync && f > 0 && (ct.sel >> f & 1))                 /* Change > Synchronize Selected Sequences */
        for (i = 1; i < LJ_CUE_TIME_FADERS; i++) if (i != f && (ct.sel >> i & 1)) any |= set_one(cue, i, pct);
    if (any) { lj_ctx_set_modified(LJ_MOD_CUE, 1); lj_cue_form_refresh(); lj_cue_time_refresh(); }
    return 0;
}
void lj_cue_time_select(int f, int on)
{
    if (f < 0 || f >= LJ_CUE_TIME_FADERS) return;
    ct.sel = (uint16_t)(on ? ct.sel | 1u << f : ct.sel & ~(1u << f));
    if (ct.strip) lj_widget_invalidate(&ct.strip->base);
}
static int visible(int f) { return ct.view == 3 || (ct.view == 1 ? f == 0 : f > 0); }   /* View: Cue Only / Sequences Only / Cue and Sequences */
int lj_cue_time_key(int vk)
{
    int step = vk == LJ_VK_UP ? 1 : vk == LJ_VK_DOWN ? -1 : vk == LJ_VK_PRIOR ? 10 : vk == LJ_VK_NEXT ? -10 : 0, f;
    if (!step || !ct.sel) return 0;
    for (f = 0; f < LJ_CUE_TIME_FADERS; f++) if (ct.sel >> f & 1) {
        int v = lj_cue_time_value(f);
        if (v < 0) return 0;
        if (f > 0 && v == LJ_TIME_PCT_CUE) v = 100;            /* stepping off "Cue" starts from as-programmed */
        lj_cue_time_set(f, v + step);
        if (ct.sync && f > 0) break;                           /* the synchronised group moved with the first one */
    }
    return 1;
}

/* ---- the strip widget: paint + mouse over Bevel1 ---- */
static void arrow(lj_surface *s, int x, int y, int up)
{
    int i;
    for (i = 0; i < 4; i++) lj_hline(s, x - i, up ? y + i : y + 3 - i, 2 * i + 1, 0x000000u);
}
static void strip_paint(lj_widget *w, lj_surface *s)
{
    const lj_cue *cue = cue_rec();
    const lj_font *fnt = lj_w_font_of(w);
    int ax, ay, f, th = trk_h(w), dn = LJ_CT_TRK_Y + th, box = dn + LJ_CT_BTN_H + 2;
    lj_widget_abs(w, &ax, &ay);
    for (f = 0; f < LJ_CUE_TIME_FADERS; f++) {
        int x = ax + LJ_CT_X0 + f * LJ_CT_PITCH, used = f == 0 || (cue && cue->slot[f - 1].seq), v = lj_cue_time_value(f), tw, p;
        char lbl[8], val[8];
        uint32_t trk;
        if (!visible(f)) continue;
        snprintf(lbl, sizeof lbl, f == 0 ? "Cue" : "#%d", f);
        tw = lj_text_width(fnt, lbl);
        if (ct.sel >> f & 1) { lj_fill(s, x, ay + LJ_CT_LBL_Y, LJ_CT_COL_W, 13, 0x000080u); lj_text(s, fnt, x + (LJ_CT_COL_W - tw) / 2, ay + LJ_CT_LBL_Y, lbl, 0xFFFFFFu, 0); }
        else lj_text(s, fnt, x + (LJ_CT_COL_W - tw) / 2, ay + LJ_CT_LBL_Y, lbl, 0x000000u, 0);
        lj_button_raised(s, x, ay + LJ_CT_UP_Y, LJ_CT_COL_W, LJ_CT_BTN_H);
        arrow(s, x + LJ_CT_COL_W / 2, ay + LJ_CT_UP_Y + 5, 1);
        lj_button_raised(s, x, ay + dn, LJ_CT_COL_W, LJ_CT_BTN_H);
        arrow(s, x + LJ_CT_COL_W / 2, ay + dn + 5, 0);
        trk = f == 0 ? LJ_CT_CYAN : !used ? LJ_CT_UNUSED : v > 0 ? LJ_CT_LOCKED : LJ_CT_FREE;
        lj_edge_sunken(s, x + 6, ay + LJ_CT_TRK_Y, LJ_CT_COL_W - 12, th, trk);
        if (v > 0 && (f == 0 ? v != 100 : 1)) {               /* the value bar from the centre (100 %) to the value */
            int c = lj_cue_time_pos_of(100, th - 4), q = lj_cue_time_pos_of(v, th - 4), lo = c < q ? c : q, hi = c < q ? q : c;
            lj_fill(s, x + 8, ay + LJ_CT_TRK_Y + 2 + (th - 5 - hi), LJ_CT_COL_W - 16, hi - lo + 1, LJ_CT_FILL);
        }
        p = lj_cue_time_pos_of(100, th - 4);
        lj_hline(s, x + 8, ay + LJ_CT_TRK_Y + 2 + (th - 5 - p), LJ_CT_COL_W - 16, LJ_CT_TICK);
        lj_edge_sunken(s, x, ay + box, LJ_CT_COL_W, LJ_CT_BOX_H, used ? 0xFFFFFFu : 0xC0C0C0u);
        if (lj_cue_time_text(f, val, sizeof val) == 0) lj_text(s, fnt, x + (LJ_CT_COL_W - lj_text_width(fnt, val)) / 2, ay + box + 2, val, 0x000000u, 0);
    }
}
static int strip_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int f, cx, v, th = trk_h(w), dn = LJ_CT_TRK_Y + th;
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_UP;
    lj_widget_set_focus(w);
    f = (m->x - LJ_CT_X0) / LJ_CT_PITCH;
    cx = (m->x - LJ_CT_X0) % LJ_CT_PITCH;
    if (m->x < LJ_CT_X0 || f >= LJ_CUE_TIME_FADERS || cx >= LJ_CT_COL_W || !visible(f)) return 1;
    v = lj_cue_time_value(f);
    if (v < 0) return 1;
    if (f > 0 && v == LJ_TIME_PCT_CUE) v = 100;
    if (m->y < LJ_CT_UP_Y) { if (!(m->ctrl || m->shift)) ct.sel = 0; lj_cue_time_select(f, !(ct.sel >> f & 1)); }   /* the header: select (Ctrl/Shift adds) */
    else if (m->y < LJ_CT_TRK_Y) lj_cue_time_set(f, v + 1);
    else if (m->y < dn) lj_cue_time_set(f, lj_cue_time_pct_at(th - 1 - (m->y - LJ_CT_TRK_Y), th));
    else if (m->y < dn + LJ_CT_BTN_H) lj_cue_time_set(f, v - 1);
    else if (f > 0) lj_cue_time_set(f, LJ_TIME_PCT_CUE);       /* a click on the value box returns the slot to "Cue" (PROPOSAL rig 03 s5.6) */
    lj_widget_invalidate(w);
    return 1;
}
static int strip_key(lj_widget *w, const lj_key_ev *k) { (void)w; return k->type == LJ_EV_KEY_DOWN && lj_cue_time_key(k->key); }
static const lj_widget_vt lj_ct_strip_vt = { strip_paint, strip_mouse, strip_key, NULL };

/* ---- menus ---- */
static void sync_checks(void)
{
    lj_menu *m = ct.win ? lj_window_menus(ct.win) : NULL;
    static const char *const ovr[4] = { "OVR_Always", "OVR_Min1", "OVR_Min2", "OVR_Min3" };
    lj_ctx *c = C();
    int i, d = c->main_ini ? lj_ini_geti(c->main_ini, "MISC", "CUETIME_REMOTE_DELTA", 1) : 1;   /* exe key; [MISC] by the literal pool order (PROPOSAL rig 13 s6); "default 1" [03 s5.6] */
    if (!m) return;
    lj_menu_set_checked(m, "CueOnly1", ct.view == 1);
    lj_menu_set_checked(m, "SequencesOnly1", ct.view == 2);
    lj_menu_set_checked(m, "CueandSequences1", ct.view == 3);
    lj_menu_set_checked(m, "SequenceTitles1", form_geti("Titles", 1));
    lj_menu_set_checked(m, "SelectedSequences1", ct.sync);
    lj_menu_set_checked(m, "OneSequence1", !ct.sync);
    for (i = 0; i < 4; i++) lj_menu_set_checked(m, ovr[i], i == d);
}
static const lj_menu_item_desc *item(void) { return ct.win ? lj_window_menu_item(ct.win) : NULL; }
static void on_view(lj_widget *w, void *ud) { const lj_menu_item_desc *it = item(); (void)w; (void)ud; if (it && it->tag >= 1 && it->tag <= 3) { ct.view = it->tag; form_seti("View", ct.view); } sync_checks(); if (ct.strip) lj_widget_invalidate(&ct.strip->base); }
static void on_titles(lj_widget *w, void *ud) { (void)w; (void)ud; form_seti("Titles", !form_geti("Titles", 1)); sync_checks(); }   /* persisted; the title display itself is UNVERIFIED (cuetime.gif shows #n with it checked) */
static void on_changes(lj_widget *w, void *ud) { const lj_menu_item_desc *it = item(); (void)w; (void)ud; if (it && it->name) { ct.sync = !strcmp(it->name, "SelectedSequences1"); form_seti("Sync", ct.sync); } sync_checks(); }
static void on_ovr(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = item();
    lj_ctx *c = C();
    (void)w; (void)ud;
    if (it && it->name && c->main_ini) lj_ini_seti(c->main_ini, "MISC", "CUETIME_REMOTE_DELTA", !strcmp(it->name, "OVR_Always") ? 0 : it->name[7] - '0');
    sync_checks();
}
static void defaults(int cue_fader, int slots)
{
    int f;
    uint16_t keep = ct.sel; int sync = ct.sync;
    ct.sync = 0;                                                   /* one command per fader */
    if (cue_fader) lj_cue_time_set(0, 100);
    if (slots) for (f = 1; f < LJ_CUE_TIME_FADERS; f++) lj_cue_time_set(f, LJ_TIME_PCT_CUE);
    ct.sel = keep; ct.sync = sync;
}
static void on_dflt_cue(lj_widget *w, void *ud) { (void)w; (void)ud; defaults(1, 0); }
static void on_dflt_seq(lj_widget *w, void *ud) { (void)w; (void)ud; defaults(0, 1); }
static void on_dflt_all(lj_widget *w, void *ud) { (void)w; (void)ud; defaults(1, 1); }
static void on_show(lj_widget *w, void *ud) { (void)w; (void)ud; sync_checks(); lj_cue_time_refresh(); }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

static const lj_binding lj_ct_bindings[] = {
    { "ViewClick", on_view, NULL }, { "SequenceTitles1Click", on_titles, NULL }, { "ChangesClick", on_changes, NULL },
    { "OverrideOptions1Click", noop, NULL }, { "OVR_Click", on_ovr, NULL },
    { "Cue1Click", on_dflt_cue, NULL }, { "Sequences1Click", on_dflt_seq, NULL }, { "All1Click", on_dflt_all, NULL }, { "Help1Click", noop, NULL },
    { "FormActivate", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormHide", noop, NULL }, { "FormShow", on_show, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_cue_time_open(void)
{
    lj_widget *bevel, *root, *grab;
    if (ct.win) return ct.win;
    ct.win = lj_window_create(&lj_form_TCUETIMEFORM, lj_ct_bindings);
    if (!ct.win) { lj_log("lj_cue_time: TCueTimeForm failed to build"); return NULL; }
    root = lj_window_root(ct.win);
    bevel = lj_window_find(ct.win, "Bevel1");
    if ((grab = lj_window_find(ct.win, "eFocusGrabber")) != NULL) grab->visible = 0;   /* LJ2 hides it under the faders; the strip takes the keys */
    ct.strip = (lj_ct_strip *)lj_w_alloc(sizeof(lj_ct_strip), root, NULL, &lj_ct_strip_vt, 1);
    ct.strip->base.name = "lj_cue_time_strip";
    ct.strip->base.transparent = 1;                                /* Bevel1's raised edge shows round the faders */
    ct.strip->base.align = LJ_AL_CLIENT;
    if (bevel) bevel->align = LJ_AL_CLIENT;                        /* the raised bevel follows the client as the faders do */
    lj_widget_layout(root);
    ct.view = form_geti("View", 3);
    if (ct.view < 1 || ct.view > 3) ct.view = 3;
    ct.sync = form_geti("Sync", 1);
    ct.posts = 0;
    memset(&ct.shown, 0, sizeof ct.shown); ct.caption[0] = 0;
    lj_widget_set_focus(&ct.strip->base);                          /* eFocusGrabber's role: the arrows reach the faders */
    sync_checks();
    lj_cue_time_refresh();
    lj_win_show(ct.win, LJ_WIN_STAY_ON_TOP);                       /* FormStyle = fsStayOnTop */
    return ct.win;
}
void lj_cue_time_close(void)
{
    if (!ct.win) return;
    lj_win_hide(ct.win);
    lj_window_destroy(ct.win);
    ct.win = NULL; ct.strip = NULL;
}
void lj_cue_time_refresh(void)
{
    const lj_cue *cue = cue_rec();
    char cap[64];
    if (!ct.win || !cue) return;
    if (memcmp(&ct.shown, cue, sizeof *cue) != 0) { ct.shown = *cue; if (ct.strip) lj_widget_invalidate(&ct.strip->base); }
    snprintf(cap, sizeof cap, "[%04u %s]", (unsigned)cue->number, cue->name[0] ? cue->name : "No name");   /* cuetime.gif: "[0000 No name]" */
    if (strcmp(cap, ct.caption) != 0) { lj_str_copy(ct.caption, sizeof ct.caption, cap); lj_window_set_caption(ct.win, cap); }
}

static void fn_time(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)fn; (void)prog; (void)ud; if (edge) lj_win_toggle(LJ_WIN_CUE_TIME); }
void lj_cue_time_bind_fns(void) { lj_fn_bind(LJ_FN_CUE_TIME_CONTROL, fn_time, NULL); }
