/* lj_genmacro_editor.c - the Generic Macro Editor (TCueMacroControlForm), Save Macro (TSaveMacroDialog + TSaveAsMacroDialog)
 * and Select Macro (TLoadMacroList) [14 sA.7-A.9]; lj_genmacro_editor.h.
 *
 * The editor programs ctx->gm_edit, the working record the engine runs live at rank 6 as LJ_GM_SRC_EDITOR (seam 2 of
 * docs/w3-contracts.md): a parameter edit is one model mutation under lj_lock; a new shape (a changed channel set),
 * the restart flag, New / Load / Clear go through LJ_ACT_GENMACRO_APPLY. Rows are the offsets of the master fixture's
 * type; the selected fixtures of that type are programmed together ("only one type of fixture at a time", CHM 5625).
 * Previews are drawn by us (ME-18): no Martin bitmap. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows/lj_genmacro_editor.h"

#include "core/lj_ini.h"
#include "core/lj_locale.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_selection.h"
#include "engine/lj_engine.h"
#include "engine/lj_genmacro.h"
#include "engine/lj_shapes_gen.h"
#include "fixlib/lj_profile.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "io/lj_fn.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_windows.h"

#define GM_STAY_ON_TOP 2u               /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */
/* Preview colours from the CHM display rules [CHM l.5703-5755]: raw shape grey, scaled white, 0 line cyan, base orange,
 * live reference green (PROPOSAL rig 14 sA.7: the exact RGBs are not in a 1:1 capture). */
#define GM_C_RAW   0x808080u
#define GM_C_OUT   0xFFFFFFu
#define GM_C_LINE  0x00FFFFu
#define GM_C_BASE  0xFF8000u
#define GM_C_REF   0x00FF00u
#define GM_C_BG    0x0000FFu   /* macrotrackingmacroout.gif / macrotrackingfixtureout.gif sample #0000FF */
#define GM_C_BAR   0x808080u   /* macroparameterspanel.gif: the bar's track; the value fills white from the left (PROPOSAL rig 14 sA.7) */

static const char *const gm_class[LJ_CH_COUNT] = { "", "Intensity", "Shutter", "Strobe", "Pan", "Tilt", "Color", "Gobo", "Beam", "Effect",
                                                   "Level", "Speed", "Lamp", "Special", "Switch", "Smoke", "Haze", "Haze fan" };   /* as the DMX Output Monitor; LJ2 shows the profile's function text (PROPOSAL rig 14 sA.7) */
static const char *const gm_dir[3] = { ">", "<", "<>" };          /* the Dir column [CHM l.5785] */

static struct {
    lj_window *win;
    int fx[LJ_FIXTURES], nfx;                                    /* the selected fixtures of the master's type, master first */
    const lj_profile *prof; int foot;
    uint8_t sel[LJ_PROFILE_CH]; int cur;                         /* highlighted offsets; the focused row (-1 none) */
    char row[LJ_PROFILE_CH][128]; const char *rows[LJ_PROFILE_CH];
    const char *shape_rows[2][LJ_GM_SHAPES_MAX]; char shape_txt[LJ_GM_SHAPES_MAX][20]; int nshape[2];
    const lj_gm_shapes *listed;                                  /* the table the shape lists were filled from */
    lj_gm_param pan;                                             /* the parameter panel */
    int update, sym, fixture_out, track_fixture, drag;
    uint8_t merged[LJ_CHANS];
    uint32_t sel_serial;
} e = { .cur = -1 };

static lj_ctx *ctx(void) { return lj_ctx_get(); }
static lj_genmacro *gm(void) { return ctx()->gm_edit; }
static const lj_gm_shapes *shapes(void) { return ctx()->gm_shapes && ctx()->gm_shapes->n ? ctx()->gm_shapes : &lj_gm_shapes_builtin; }
static int named(const lj_widget *w, const char *n) { return w && w->name && !strcmp(w->name, n); }
static void apply(void) { lj_desktop_run_action(LJ_ACT_GENMACRO_APPLY, 0); }
static void post(int kind, uint32_t a, uint32_t b) { lj_cmd c; memset(&c, 0, sizeof c); c.kind = (uint8_t)kind; c.a = a; c.b = b; lj_ctx_post(&c); }
static void set_text(lj_window *win, const char *name, const char *t) { lj_widget *w = win ? lj_window_find(win, name) : NULL; if (w) { lj_w_set_caption(w, t); lj_widget_invalidate(w); } }

/* ---- fixtures, offsets, channel codes ---- */
static int fx_code(int f, int ofs)                               /* address code of fixture f's offset, -1 unpatched / outside */
{
    const lj_fixture *x = ctx()->show ? &ctx()->show->fixture[f] : NULL;
    int c;
    if (!x || !x->link) return -1;
    c = lj_addr_code(x->link, x->address) + ofs;
    return c >= 0 && c < LJ_CHANS ? c : -1;
}
static int is_lsb(int ofs)                                       /* 16-bit LSB rows are N/A (CHM 5583) */
{
    const lj_profile *p = e.prof;
    int k;
    if (!p) return 0;
    if (p->pt16 && (ofs == p->pan_ofs + 1 || ofs == p->tilt_ofs + 1)) return 1;
    for (k = 0; k < p->footprint && k < LJ_PROFILE_CH; k++) if (p->pair_lsb[k] == ofs + 1) return 1;
    return 0;
}
static void collect(void)
{
    int m = lj_sel_master(), i;
    e.nfx = 0; e.prof = lj_sel_master_profile(); e.foot = e.prof ? e.prof->footprint : 0;
    if (e.foot > LJ_PROFILE_CH) e.foot = LJ_PROFILE_CH;
    if (m < 0 || !e.prof) { e.foot = 0; return; }
    e.fx[e.nfx++] = m;
    for (i = 0; i < LJ_FIXTURES; i++) if (i != m && lj_sel_is_selected(i) && lj_sel_same_type(m, i)) e.fx[e.nfx++] = i;
}

/* Every (selected fixture, highlighted offset) except LSBs: fn(param, code) under lj_lock; returns how many it touched. */
static int each(void (*fn)(lj_gm_param *p, int code, void *ud), void *ud, int programmed_only)
{
    lj_genmacro *g = gm();
    int f, k, n = 0;
    if (!g) return 0;
    lj_lock();
    for (f = 0; f < e.nfx; f++) for (k = 0; k < e.foot; k++) {
        int c = fx_code(e.fx[f], k);
        if (!e.sel[k] || c < 0 || is_lsb(k) || (programmed_only && !lj_genmacro_ch_used(g, c))) continue;
        fn(&g->ch[c], c, ud); n++;
    }
    lj_unlock();
    return n;
}

/* ---- parameter edits: "not possible to program parameters before a macro shape has been assigned" (CHM 5631) ---- */
enum { P_BASE = 0, P_BASE_ON, P_AMP_POS, P_AMP_NEG, P_CYCLE, P_DIR, P_DELAY };
static void set_one(lj_gm_param *p, int code, void *ud)
{
    (void)code;
    switch (*(int *)ud) {
    case P_BASE: p->base = e.pan.base; break;
    case P_BASE_ON: p->base_on = e.pan.base_on; break;
    case P_AMP_POS: p->amp_pos = e.pan.amp_pos; if (e.sym) p->amp_neg = e.pan.amp_pos; break;
    case P_AMP_NEG: p->amp_neg = e.pan.amp_neg; if (e.sym) p->amp_pos = e.pan.amp_neg; break;
    case P_CYCLE: p->cycle_ds = e.pan.cycle_ds; break;
    case P_DIR: p->dir = e.pan.dir; break;
    default: p->delay = e.pan.delay; break;
    }
}
static void set_param(int which, int force) { if (e.update || force) each(set_one, &which, 1); lj_genmacro_editor_refresh(); }

static void shape_one(lj_gm_param *p, int code, void *ud)
{
    int keep = lj_genmacro_ch_used(gm(), code);
    if (!keep) { *p = e.pan; }                                   /* a fresh channel takes the panel (the CHM default is base enabled at 0) */
    p->shape = (uint8_t)*(int *)ud;
}
static void assign_shape(int shape)
{
    if (shape < 0 || shape >= shapes()->n || !e.update) return;  /* Macro Parameter Update disabled = browse shapes only (CHM 5843) */
    if (each(shape_one, &shape, 0)) apply();
    lj_genmacro_editor_refresh();
}
static void clear_one(lj_gm_param *p, int code, void *ud) { (void)code; (void)ud; memset(p, 0, sizeof *p); }

static void load_panel(void)                                     /* the focused row of the master into the panel */
{
    const lj_genmacro *g = gm();
    int c = e.cur >= 0 && e.nfx ? fx_code(e.fx[0], e.cur) : -1;
    if (g && c >= 0 && lj_genmacro_ch_used(g, c)) e.pan = g->ch[c];
}

/* ---- text ---- */
static void time_text(char *d, size_t cap, uint16_t ds) { snprintf(d, cap, "%u%c%u", ds / 10u, lj_locale_decimal(), ds % 10u); }   /* D6: host decimal separator */
static void fill_rows(void)
{
    const lj_genmacro *g = gm();
    int k;
    for (k = 0; k < e.foot; k++) {
        int c = e.nfx ? fx_code(e.fx[0], k) : -1, cls = e.prof->channel_class[k] < LJ_CH_COUNT ? e.prof->channel_class[k] : 0;
        const char *fn = gm_class[cls];
        char t[16], base[8];
        const lj_gm_param *p = g && c >= 0 ? &g->ch[c] : NULL;
        int out = c >= 0 ? e.merged[c] : 0;
        if (is_lsb(k)) snprintf(e.row[k], sizeof e.row[k], "%d\t%s (16 bit LSB)\t%d\tN/A", k, fn, out);
        else if (!p || !lj_genmacro_ch_used(g, c)) snprintf(e.row[k], sizeof e.row[k], "%d\t%s%s\t%d", k, fn, is_lsb(k + 1) ? " (16 bit MSB)" : "", out);
        else {
            time_text(t, sizeof t, p->cycle_ds);
            if (p->base_on) snprintf(base, sizeof base, "%d", p->base); else lj_str_copy(base, sizeof base, "off");
            snprintf(e.row[k], sizeof e.row[k], "%d\t%s%s\t%d\t%s\t%s\t%d\t%d\t%s\t%s\t%d", k, fn, is_lsb(k + 1) ? " (16 bit MSB)" : "", out,
                     p->shape < shapes()->n ? shapes()->name[p->shape] : "", base, p->amp_pos, p->amp_neg, gm_dir[p->dir <= LJ_DIR_BOUNCE ? p->dir : 0], t, p->delay);
        }
        e.rows[k] = e.row[k];
    }
}
static void fill_shapes(void)                                    /* Built-in shapes / User-shapes: the table in file order, no sorting [14 sA.4] */
{
    const lj_gm_shapes *sh = shapes();
    lj_widget *b = lj_window_find(e.win, "lvBuiltIn"), *u = lj_window_find(e.win, "lvUser");
    int i;
    e.nshape[0] = e.nshape[1] = 0;
    for (i = 0; i < sh->n; i++) {
        lj_str_copy(e.shape_txt[i], sizeof e.shape_txt[i], sh->name[i]);
        e.shape_rows[i < sh->nb ? 0 : 1][e.nshape[i < sh->nb ? 0 : 1]++] = e.shape_txt[i];
    }
    if (b) lj_w_listview_set_items(b, e.shape_rows[0], e.nshape[0]);
    if (u) lj_w_listview_set_items(u, e.shape_rows[1], e.nshape[1]);
    e.listed = sh;
}

int lj_genmacro_editor_caption(char *dst, size_t cap)            /* "Generic Macro Editor [0001 name]", "[]" while unsaved (F19; the bracket content UNVERIFIED, 14 Unverified 10) */
{
    const lj_genmacro *g = gm();
    char no[12];
    int k;
    if (g && g->number && lj_number_text(g->number, no, sizeof no) == 0) k = snprintf(dst, cap, "Generic Macro Editor [%s %s]", no, g->name);
    else k = snprintf(dst, cap, "Generic Macro Editor []");
    return k < 0 || (size_t)k >= cap ? -1 : 0;
}

/* the macro's readout: delta min / max over the shape (the -186/186 of genericmacroeditor.gif) */
static void minmax(const lj_gm_param *p, int *lo, int *hi)
{
    int k;
    *lo = 0; *hi = 0;
    for (k = 0; k < LJ_GM_SAMPLES; k++) {
        int d = lj_genmacro_delta(shapes(), p, (uint32_t)k << 16);
        if (d < *lo) *lo = d;
        if (d > *hi) *hi = d;
    }
}

void lj_genmacro_editor_reprofile(void) { if (e.win) { e.sel_serial = lj_sel_serial() + 1u; lj_genmacro_editor_refresh(); } }

void lj_genmacro_editor_refresh(void)
{
    lj_ctx *c = ctx();
    lj_widget *lv;
    char t[96];
    int lo, hi;
    if (!e.win) return;
    if (e.listed != shapes()) fill_shapes();
    if (e.sel_serial != lj_sel_serial()) { collect(); e.sel_serial = lj_sel_serial(); }
    if (c->engine) { const lj_frame *f; lj_lock(); f = lj_engine_frame(c->engine); if (f) memcpy(e.merged, f->merged, sizeof e.merged); lj_unlock(); }
    fill_rows();
    if ((lv = lj_window_find(e.win, "lvChannels")) != NULL) { int keep = lj_w_listview_index(lv); lj_w_listview_set_items(lv, e.rows, e.foot); ((lj_w_listview *)lv)->index = keep; }
    if (lj_genmacro_editor_caption(t, sizeof t) == 0) lj_window_set_caption(e.win, t);
    set_text(e.win, "lFixType", e.prof ? e.prof->name : "");
    snprintf(t, sizeof t, "%d", e.nfx); set_text(e.win, "lFixCount", t);
    snprintf(t, sizeof t, "%d", e.foot); set_text(e.win, "lChanCount", t);     /* PROPOSAL rig 14 sA.7: the offsets of the type */
    set_text(e.win, "lMacro", e.pan.shape < shapes()->n ? shapes()->name[e.pan.shape] : "");
    if (e.pan.cycle_ds) { char v[16]; time_text(v, sizeof v, e.pan.cycle_ds); snprintf(t, sizeof t, "Cycle time: %s seconds", v); }   /* EXE 0x33A8EC "Cycle time: %.1f seconds" (genericmacroeditor.gif) */
    else lj_str_copy(t, sizeof t, "Cycle time: never");                         /* EXE 0x33A8D0 */
    set_text(e.win, "lTime", t);
    minmax(&e.pan, &lo, &hi);
    snprintf(t, sizeof t, "Min/Max : %d/%d", lo, hi); set_text(e.win, "lMinMax", t);   /* EXE 0x3399F8 "Min/Max : %d/%d" (genericmacroeditor.gif) */
    if (c->engine && (lv = lj_window_find(e.win, "sbDisableMacros")) != NULL && lj_w_speedbutton_down(lv) != !c->view.gm_engine_on) lj_w_speedbutton_set_down(lv, !c->view.gm_engine_on);   /* the engine's state (hotkeys too); headless: as the DFM */
    if ((lv = lj_window_find(e.win, "sbRestart")) != NULL && gm() && lj_w_speedbutton_down(lv) != (gm()->restart_on_load != 0)) lj_w_speedbutton_set_down(lv, gm()->restart_on_load);
    lj_window_invalidate_all(e.win);
}

/* ---- painting (ours, ME-18) ---- */
static void abs_of(lj_widget *w, int *x, int *y) { lj_widget_abs(w, x, y); }   /* surface coordinates: the paint origin and the mouse events' frame */
static void span(lj_surface *s, int x, int y0, int y1, int t, uint32_t rgb)   /* one column of a trace: joins the previous sample, t px thick */
{
    int lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0;
    lj_fill(s, x, lo - t / 2, 1, hi - lo + t, rgb);
}
static void on_pb1_paint(lj_widget *w, void *ud)                 /* PB1 129x129: Macro Out (raw grey, scaled white, cyan 0) or Fixture Out (255 / 50 % / 0, base / reference) */
{
    lj_surface *s = lj_window_surface(e.win);
    lj_gm_param p = e.pan;
    int ax, ay, x, h = w->h, mid = w->h / 2, ref = 128;
    (void)ud;
    abs_of(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, h, GM_C_BG);
    if (e.nfx && e.cur >= 0 && fx_code(e.fx[0], e.cur) >= 0) ref = e.merged[fx_code(e.fx[0], e.cur)];
    if (!e.fixture_out) {
        int pr = 0, pd = 0;
        lj_hline(s, ax, ay + mid, w->w, GM_C_LINE); lj_vline(s, ax + w->w / 2, ay, h, GM_C_LINE);   /* macrotrackingmacroout.gif: cyan 0 line and centre line */
        for (x = 0; x < w->w; x++) {
            int k = x * LJ_GM_SAMPLES / w->w, raw = lj_genmacro_shape_in(shapes(), p.shape, k), d;
            lj_gm_param q = p; q.delay = 0;
            d = lj_genmacro_delta(shapes(), &q, (uint32_t)k << 16);
            raw = ay + mid - raw * (mid - 1) / LJ_GM_FULL; d = ay + mid - d * (mid - 1) / 255;
            if (!x) { pr = raw; pd = d; }
            span(s, ax + x, pr, raw, 3, GM_C_RAW);                  /* the shape 3 px thick, the scaled output 1 px (measured from macrotrackingmacroout.gif, 1:1) */
            span(s, ax + x, pd, d, 1, GM_C_OUT);
            pr = raw; pd = d;
        }
    } else {
        lj_hline(s, ax, ay, w->w, GM_C_LINE); lj_hline(s, ax, ay + mid, w->w, GM_C_LINE); lj_hline(s, ax, ay + h - 1, w->w, GM_C_LINE);
        lj_hline(s, ax, ay + h - 1 - (p.base_on ? p.base : ref) * (h - 1) / 255, w->w, p.base_on ? GM_C_BASE : GM_C_REF);
        for (x = 0; x < w->w; x++) {
            int v = lj_genmacro_step(shapes(), &p, (uint32_t)(x * LJ_GM_SAMPLES / w->w) << 16, ref, 0);
            lj_fill(s, ax + x, ay + h - 1 - v * (h - 1) / 255, 1, 1, GM_C_OUT);
        }
    }
}

static int bar_of(const lj_widget *w) { return named(w, "pbAmpPos") ? 1 : named(w, "pbAmpNeg") ? 2 : named(w, "pbSpeed") ? 3 : 0; }
/* Cycle time bar: 0 = static, else 0.4 .. 595 s on a square law, "resolution is much higher at lower time values" (CHM 5833; PROPOSAL rig 14 sA.2) */
static uint16_t cycle_at(int pos, int w) { long t = w > 1 ? (long)pos * 1000 / (w - 1) : 0; return (uint16_t)(t <= 0 ? 0 : LJ_GENMACRO_CYCLE_MIN_DS + t * t * (LJ_GENMACRO_CYCLE_MAX_DS - LJ_GENMACRO_CYCLE_MIN_DS) / 1000000); }
static int cycle_pos(uint16_t ds, int w) { int x; for (x = 0; x < w; x++) if (cycle_at(x, w) >= ds) return x; return w - 1; }
static void on_bar_paint(lj_widget *w, void *ud)
{
    lj_surface *s = lj_window_surface(e.win);
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, b = bar_of(w), x;
    char t[40];
    (void)ud;
    abs_of(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, GM_C_BAR);
    if (b == 3) {
        char v[16];
        x = cycle_pos(e.pan.cycle_ds, w->w);
        if (e.pan.cycle_ds) { time_text(v, sizeof v, e.pan.cycle_ds); snprintf(t, sizeof t, "Cycle time: %s sec", v); }   /* EXE 0x33ABC8 "Cycle time: %.1f sec" */
        else lj_str_copy(t, sizeof t, "Cycle time: never");                                                                   /* EXE 0x33ABAC */
    } else { int v = b == 1 ? e.pan.amp_pos : e.pan.amp_neg; x = (v + 255) * (w->w - 1) / 510; snprintf(t, sizeof t, "%s%d", b == 1 ? "Pos Amplitude " : "Neg Amplitude ", v); }   /* EXE 0x33AB7C / 0x33AB94 */
    lj_fill(s, ax, ay + 1, x + 1, w->h - 2, 0xFFFFFFu);   /* white from the left: Pos 180 fills 86 %, Neg 82 66 % in macroparameterspanel.gif = (v + 255) / 510 */
    lj_text(s, f, ax + 6, ay + (w->h - f->height) / 2, t, 0x000000u, 0);   /* PROPOSAL rig 14 sA.7: 6 px from the reduced macroparameterspanel.gif */
}
static void bar_mouse(lj_widget *w)
{
    const lj_mouse_ev *m = lj_window_last_event(e.win);
    int ax, ay, x, b = bar_of(w);
    if (!m || !b) return;
    abs_of(w, &ax, &ay);
    x = m->x - ax; x = x < 0 ? 0 : x >= w->w ? w->w - 1 : x;
    if (b == 3) { e.pan.cycle_ds = cycle_at(x, w->w); set_param(P_CYCLE, 0); }
    else { int v = w->w > 1 ? x * 510 / (w->w - 1) - 255 : 0; if (b == 1) { e.pan.amp_pos = (int16_t)v; if (e.sym) e.pan.amp_neg = (int16_t)v; set_param(P_AMP_POS, 0); } else { e.pan.amp_neg = (int16_t)v; if (e.sym) e.pan.amp_pos = (int16_t)v; set_param(P_AMP_NEG, 0); } }
}
static void on_bar_down(lj_widget *w, void *ud) { (void)ud; e.drag = bar_of(w); bar_mouse(w); }
static void on_bar_move(lj_widget *w, void *ud) { (void)ud; if (e.drag) bar_mouse(w); }
static void on_bar_up(lj_widget *w, void *ud) { (void)w; (void)ud; e.drag = 0; }

/* ---- parameter controls ---- */
static void on_step(lj_widget *w, void *ud)                      /* sbPosAmpDec/Inc, sbNegAmpDec/Inc, sbSpeedDec/Inc: one unit (ponytail: no auto-repeat while held) */
{
    int d = w && w->name && strstr(w->name, "Inc") ? 1 : -1;
    (void)ud;
    if (!w || !w->name) return;
    if (strstr(w->name, "PosAmp")) { int v = e.pan.amp_pos + d; e.pan.amp_pos = (int16_t)(v < -255 ? -255 : v > 255 ? 255 : v); if (e.sym) e.pan.amp_neg = e.pan.amp_pos; set_param(P_AMP_POS, 0); }
    else if (strstr(w->name, "NegAmp")) { int v = e.pan.amp_neg + d; e.pan.amp_neg = (int16_t)(v < -255 ? -255 : v > 255 ? 255 : v); if (e.sym) e.pan.amp_pos = e.pan.amp_neg; set_param(P_AMP_NEG, 0); }
    else { int v = e.pan.cycle_ds + d; e.pan.cycle_ds = (uint16_t)(v < 0 ? 0 : v > LJ_GENMACRO_CYCLE_MAX_DS ? LJ_GENMACRO_CYCLE_MAX_DS : v); set_param(P_CYCLE, 0); }
}
static void on_set(lj_widget *w, void *ud)                       /* the four Set buttons write the panel value to the highlighted channels */
{
    (void)ud;
    if (named(w, "sbSetAmpPos")) set_param(P_AMP_POS, 1);
    else if (named(w, "sbSetAmpNeg")) set_param(P_AMP_NEG, 1);
    else if (named(w, "sbSetSpeed")) set_param(P_CYCLE, 1);
    else { set_param(P_BASE_ON, 1); set_param(P_BASE, 1); }
}
/* the edit's text, clamped: OnChange fires before TNumEdit.Value follows the text (lj_w_numedit_set_value -> lj_w_edit_set_text) */
static int num_of(const lj_widget *w, int lo, int hi) { const char *t = w ? lj_w_edit_text(w) : NULL; long v = t ? strtol(t, NULL, 10) : lo; return (int)(v < lo ? lo : v > hi ? hi : v); }
static void on_base_change(lj_widget *w, void *ud) { (void)ud; e.pan.base = (uint8_t)num_of(w, 0, 255); set_param(P_BASE, 0); }
static void on_base_off(lj_widget *w, void *ud) { (void)ud; e.pan.base_on = (uint8_t)!lj_w_speedbutton_down(w); set_param(P_BASE_ON, 0); }
static void on_dir(lj_widget *w, void *ud)
{
    (void)ud;
    e.pan.dir = (uint8_t)(named(w, "sbDirectionRev") ? LJ_DIR_REV : named(w, "sbDirectionBounce") ? LJ_DIR_BOUNCE : LJ_DIR_FWD);
    set_param(P_DIR, 0);
}
static void on_restart_flag(lj_widget *w, void *ud) { (void)ud; if (gm()) { lj_lock(); gm()->restart_on_load = (uint8_t)lj_w_speedbutton_down(w); lj_unlock(); apply(); } }
static void on_update(lj_widget *w, void *ud) { (void)ud; e.update = lj_w_speedbutton_down(w); }
static void on_sym(lj_widget *w, void *ud) { (void)ud; e.sym = lj_w_checkbox_checked(w); }
static void on_track(lj_widget *w, void *ud) { (void)ud; e.track_fixture = named(w, "sbTrackFixture"); }   /* ponytail: the tracking dots need the engine's per-channel index in the view */
static void on_out_mode(lj_widget *w, void *ud) { (void)ud; e.fixture_out = named(w, "sbFixtureOut"); lj_genmacro_editor_refresh(); }

/* ---- lists ---- */
static void on_shape_select(lj_widget *w, void *ud)
{
    int i = lj_w_listview_index(w);
    (void)ud;
    if (i < 0) return;
    if (named(w, "lvUser")) i += shapes()->nb;
    e.pan.shape = (uint8_t)i;
    assign_shape(i);
}
static void on_channel_select(lj_widget *w, void *ud)            /* a click highlights that row (ponytail: Ctrl / Shift multi-select rides Select all) */
{
    int i = lj_w_listview_index(w);
    (void)ud;
    if (i < 0 || i >= e.foot) return;
    memset(e.sel, 0, sizeof e.sel); e.sel[i] = 1; e.cur = i;
    load_panel();
    lj_genmacro_editor_refresh();
}
static void on_select_all(lj_widget *w, void *ud) { int k; (void)ud; for (k = 0; k < e.foot; k++) e.sel[k] = (uint8_t)named(w, "sbSelectAll"); lj_genmacro_editor_refresh(); }

/* sbNewMacroClick is shared by Clear Offset(s), Clear fixture(s) and New Macro [DFM Panel6] */
static void on_new(lj_widget *w, void *ud)
{
    lj_genmacro *g = gm();
    (void)ud;
    if (!g) return;
    if (named(w, "sbClearCh")) { if (each(clear_one, NULL, 0)) apply(); }
    else if (named(w, "sbClearFixture")) {
        uint8_t keep[LJ_PROFILE_CH];
        memcpy(keep, e.sel, sizeof keep); memset(e.sel, 1, sizeof e.sel);
        if (each(clear_one, NULL, 0)) apply();
        memcpy(e.sel, keep, sizeof keep);
    } else { lj_lock(); memset(g, 0, sizeof *g); lj_unlock(); apply(); }   /* New Macro: an unnumbered empty record */
    lj_genmacro_editor_refresh();
}
static void on_restart(lj_widget *w, void *ud) { (void)w; (void)ud; post(LJ_CMD_GENMACRO_RESTART, LJ_GM_SRC_EDITOR, 0); }
static void on_disable(lj_widget *w, void *ud) { (void)ud; post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_ENGINE, (uint32_t)!lj_w_speedbutton_down(w)); }   /* hint "Disable Macro Engine" */
static void on_save_btn(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_toggle(LJ_WIN_SAVE_GENMACRO); }
static void on_load_btn(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_toggle(LJ_WIN_SELECT_GENMACRO); }

/* ---- Delays tab [14 sA.5] ---- */
static void on_delay_change(lj_widget *w, void *ud) { (void)ud; if (named(w, "eDelay")) { e.pan.delay = (uint16_t)num_of(w, 0, LJ_GM_DELAY_MAX); set_param(P_DELAY, 0); } }
static void on_clear_delay(lj_widget *w, void *ud) { lj_widget *d = lj_window_find(e.win, "eDelay"); (void)w; (void)ud; e.pan.delay = 0; if (d) lj_w_numedit_set_value(d, 0); set_param(P_DELAY, 1); }
static void on_full_delay(lj_widget *w, void *ud) { lj_widget *to = lj_window_find(e.win, "eDelayEnd"); (void)ud; if (to) { to->enabled = !lj_w_checkbox_checked(w); lj_widget_invalidate(to); } }   /* "disables To" */
static int nval(const char *name, int dflt) { lj_widget *w = lj_window_find(e.win, name); return w ? num_of(w, 0, LJ_GM_DELAY_MAX) : dflt; }
static int rval(const char *name) { lj_widget *w = lj_window_find(e.win, name); return w ? lj_w_radiogroup_index(w) : 0; }
static int cval_in(lj_window *win, const char *name) { lj_widget *w = win ? lj_window_find(win, name) : NULL; return w ? lj_w_checkbox_checked(w) : 0; }
static int cval(const char *name) { return cval_in(e.win, name); }
static void on_apply_delay(lj_widget *w, void *ud)               /* Apply Auto Delay (both tabs): one value per fixture in the Delay Order, every highlighted programmed offset */
{
    static const int styles[4] = { LJ_GMD_LINEAR, LJ_GMD_CIRCULAR, LJ_GMD_SEGMENTS, LJ_GMD_RANDOM };
    lj_genmacro *g = gm();
    uint16_t d[LJ_FIXTURES];
    uint32_t seed = (uint32_t)lj_sel_serial() * 2654435761u + 1u;
    int st = rval("rgDelayStyle"), order = rval("rgDelayOrder"), f, k, groups;
    (void)w; (void)ud;
    if (!g || !e.nfx) return;
    st = styles[st >= 0 && st < 4 ? st : 0];
    groups = st == LJ_GMD_CIRCULAR ? nval("eCircularCount", 2) : nval("eSegmentCount", 2);
    if (order > 1) lj_log("generic macro auto delay: icon position orders use the fixture order (ponytail: needs the desktop icon positions)");
    if (lj_genmacro_auto_delay(e.nfx, st, nval("eDelayStart", 0), nval("eDelayEnd", LJ_GM_DELAY_MAX), cval("cbUseFullDelay"), groups, &seed, d) != 0) return;
    lj_lock();
    for (f = 0; f < e.nfx; f++) {
        int idx = (order % 2) ? e.nfx - 1 - f : f;                 /* Fixture Number / Fixture Number Reversed */
        for (k = 0; k < e.foot; k++) { int c = fx_code(e.fx[idx], k); if (e.sel[k] && c >= 0 && !is_lsb(k) && lj_genmacro_ch_used(g, c)) g->ch[c].delay = d[f]; }
    }
    lj_unlock();
    lj_genmacro_editor_refresh();
}

/* ---- menus and popups ---- */
static void on_menu_toggle(lj_widget *w, void *ud)               /* Clear Macro on Editor Close / Slow Macrodisplay Update: Main.Ini [MACRO] (PROPOSAL keys) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(e.win);
    lj_menu_rt *rt = it ? lj_menu_rt_find(lj_window_menus(e.win), it) : NULL;
    (void)w; (void)ud;
    if (!rt) return;
    rt->checked = !rt->checked;
    if (ctx()->main_ini && it->name) lj_ini_seti(ctx()->main_ini, "MACRO", !strcmp(it->name, "M_ClearOnClose") ? "ClearOnClose" : "SlowUpdate", rt->checked);
}
static void on_random_by(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(e.win);
    int by_fx = it && it->name && !strcmp(it->name, "M_RandomByFixture");
    (void)w; (void)ud;
    lj_menu_set_checked(lj_window_menus(e.win), "M_RandomByOffset", !by_fx); lj_menu_set_checked(lj_window_menus(e.win), "M_RandomByFixture", by_fx);
    if (ctx()->main_ini) lj_ini_seti(ctx()->main_ini, "MACRO", "RandomByFixture", by_fx);   /* PROPOSAL key; the delay is per fixture either way here */
}
static void on_reload_shapes(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_GM_SHAPES_RELOAD, 0); }
static void on_pop_channel(lj_widget *w, void *ud)               /* Get Base Value / Positive / Negative Amplitude from current sequence */
{
    const lj_menu_item_desc *it = lj_window_menu_item(e.win);
    const lj_sequence *sq = ctx()->show ? ctx()->show->current_seq : NULL;
    int c = e.nfx && e.cur >= 0 ? fx_code(e.fx[0], e.cur) : -1, v, sc = ctx()->view.cur_scene;
    (void)w; (void)ud;
    if (!it || !it->name || c < 0 || !sq || !sq->scenes || sc >= sq->n) return;
    v = sq->scenes[sc].val[c];
    if (!strcmp(it->name, "popGetBaseValue")) { e.pan.base = (uint8_t)v; e.pan.base_on = 1; set_param(P_BASE, 0); set_param(P_BASE_ON, 0); }
    else if (!strcmp(it->name, "popGetPosAmp")) { e.pan.amp_pos = (int16_t)v; set_param(P_AMP_POS, 0); }
    else { e.pan.amp_neg = (int16_t)v; set_param(P_AMP_NEG, 0); }
}
static void on_lv_pop(lj_widget *w, void *ud)                    /* Update to Parameters (row -> panel) / Update with Parameters (panel -> highlighted rows) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(e.win);
    int i;
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "pUpdateToParams")) { load_panel(); lj_genmacro_editor_refresh(); return; }
    for (i = P_BASE; i <= P_DELAY; i++) set_param(i, 1);
}
static void on_close(lj_widget *w, void *ud)
{
    lj_menu_rt *rt = lj_menu_rt_named(lj_window_menus(e.win), "M_ClearOnClose");
    (void)w; (void)ud;
    if (rt && rt->checked && gm()) { lj_lock(); memset(gm(), 0, sizeof *gm()); lj_unlock(); apply(); }
    lj_genmacro_editor_close();
}

static const lj_binding gm_bindings[] = {
    { "bApplyDelayClick", on_apply_delay, NULL }, { "bRestartClick", on_restart, NULL }, { "cbAutoDelayClick", lj_handler_noop, NULL }, { "cbDelayClick", on_full_delay, NULL },
    { "cbSymClick", on_sym, NULL }, { "chSelectClick", on_select_all, NULL }, { "eBaseChange", on_base_change, NULL }, { "eCountChange", lj_handler_noop, NULL },
    { "eDelayChange", on_delay_change, NULL }, { "eDelayValueChange", lj_handler_noop, NULL }, { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL },
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "lvChannelsDblClick", lj_handler_noop, NULL }, { "lvChannelsSelectItem", on_channel_select, NULL }, { "lvMacrosDblClick", on_shape_select, NULL },
    { "lvMacroSelectItem", on_shape_select, NULL }, { "lvpopClick", on_lv_pop, NULL }, { "M_RandomByClick", on_random_by, NULL },
    { "MenuToggleClick", on_menu_toggle, NULL }, { "PB1Paint", on_pb1_paint, NULL }, { "pbBarMouseDown", on_bar_down, NULL },
    { "pbBarMouseMove", on_bar_move, NULL }, { "pbBarMouseUp", on_bar_up, NULL }, { "PBPaint", on_bar_paint, NULL },
    { "popChannelClick", on_pop_channel, NULL }, { "ReloadUserShapes1Click", on_reload_shapes, NULL }, { "rgDelayStyleClick", lj_handler_noop, NULL },
    { "sbBaseOffMouseUp", on_base_off, NULL }, { "sbClearDelayClick", on_clear_delay, NULL }, { "sbDirectionMouseUp", on_dir, NULL },
    { "sbDisableMacrosClick", on_disable, NULL }, { "sbLoadMouseUp", on_load_btn, NULL }, { "sbMacroOutMouseUp", on_out_mode, NULL },
    { "sbMouseDown", lj_handler_noop, NULL }, { "sbNewMacroClick", on_new, NULL }, { "sbPosAmpDecMouseUp", on_step, NULL },
    { "sbRestartMouseUp", on_restart_flag, NULL }, { "sbSaveMacroMouseUp", on_save_btn, NULL }, { "sbTrackChnlMouseDown", on_track, NULL },
    { "sbUpdateMouseUp", on_update, NULL }, { "SetClick", on_set, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_genmacro_editor_open(void)
{
    lj_widget *w;
    if (e.win) return e.win;
    memset(&e, 0, sizeof e);
    e.cur = -1; e.update = 1;                                    /* sbUpdate Down = True in the DFM */
    e.pan.base_on = 1;                                           /* "base-value enabled, value = 0" (CHM 5811) */
    if ((e.win = lj_window_create(&lj_form_TCUEMACROCONTROLFORM, gm_bindings)) == NULL) { lj_log("genmacro editor: cannot create TCueMacroControlForm"); return NULL; }
    if ((w = lj_window_find(e.win, "sbBaseOff")) != NULL) lj_w_speedbutton_set_down(w, 0);
    lj_menu_set_checked(lj_window_menus(e.win), "M_RandomByFixture", lj_ini_geti(ctx()->main_ini, "MACRO", "RandomByFixture", 0));
    lj_menu_set_checked(lj_window_menus(e.win), "M_RandomByOffset", ctx()->main_ini ? !lj_ini_geti(ctx()->main_ini, "MACRO", "RandomByFixture", 0) : 1);
    lj_menu_set_checked(lj_window_menus(e.win), "M_ClearOnClose", lj_ini_geti(ctx()->main_ini, "MACRO", "ClearOnClose", 0));
    collect(); e.sel_serial = lj_sel_serial();
    fill_shapes();
    lj_genmacro_editor_refresh();
    lj_win_show(e.win, GM_STAY_ON_TOP);
    return e.win;
}
void lj_genmacro_editor_close(void) { if (!e.win) return; lj_win_hide(e.win); lj_window_destroy(e.win); e.win = NULL; }
lj_window *lj_genmacro_editor_get(void) { return e.win; }

/* ---- Save Macro (TSaveMacroDialog 326x120) and Save as Macro# (TSaveAsMacroDialog 220x70) [14 sA.8] ---- */
static struct { lj_window *win, *as; } sv;
static int sv_write(uint32_t number)
{
    lj_genmacro *g = gm(), *rec;
    char path[LJ_PATH_CAP];
    lj_widget *n = sv.win ? lj_window_find(sv.win, "eMacroName") : NULL;
    const char *bad;
    if (!g || !ctx()->lib || !number) return -1;
    if (number > LJ_MACROS_MAX) { lj_win_message("Save Macro", "Error : Maximum number of Macros exceeded"); return -1; }   /* EXE 0x1f9ee0 */
    if ((rec = lj_genmacro_new_rec()) == NULL) return -1;
    lj_lock(); *rec = *g; lj_unlock();
    rec->number = number;
    if (n && lj_w_edit_text(n)) lj_str_copy(rec->name, sizeof rec->name, lj_w_edit_text(n));
    if (!rec->name[0]) snprintf(rec->name, sizeof rec->name, "Macro #%u", (unsigned)number);   /* EXE 0x1f9ec8 "Macro #%d" */
    if ((bad = lj_genmacro_validate(rec)) != NULL || lj_library_record_path(ctx()->lib, LJ_REC_MACRO, number, path, sizeof path) != 0 || lj_genmacro_save(path, rec) != 0) {
        lj_log("save macro %04u: %s", (unsigned)number, bad ? bad : "not written");
        free(rec); return -1;
    }
    lj_lock(); g->number = number; lj_str_copy(g->name, sizeof g->name, rec->name); lj_unlock();
    free(rec);
    apply();
    if (sv.win && cval_in(sv.win, "cbAddMacro")) lj_desktop_run_action(LJ_ACT_GENMACRO_TO_CUE, (int)number);
    if (sv.win && (cval_in(sv.win, "cbNewCue") || cval_in(sv.win, "cbAutosaveCue"))) lj_log("save macro: Create new Cue / Autosave Current Cue not wired yet (ponytail: the cue save path is the cue window's)");
    if (sv.win && cval_in(sv.win, "cbClearAfterSave")) { lj_lock(); memset(g, 0, sizeof *g); lj_unlock(); apply(); }
    lj_select_genmacro_refresh();
    return 0;
}

static void sv_fill(void)
{
    const lj_genmacro *g = gm();
    char no[12];
    lj_widget *w;
    if (!sv.win) return;
    if (!g || !g->number || lj_number_text(g->number, no, sizeof no) != 0) no[0] = 0;
    if ((w = lj_window_find(sv.win, "eMacroNo")) != NULL) lj_w_edit_set_text(w, no);
    if ((w = lj_window_find(sv.win, "eMacroName")) != NULL) lj_w_edit_set_text(w, g ? g->name : "");
    if ((w = lj_window_find(sv.win, "Bsave")) != NULL) { w->enabled = g && g->number; lj_widget_invalidate(w); }   /* disabled until the macro has a number */
    lj_menu_set_checked(lj_window_menus(sv.win), "M_AutoClose", lj_ini_geti(lj_ctx_get()->forms_ini, "SAVEMACRO", "AutoClose", 1));
    lj_menu_set_checked(lj_window_menus(sv.win), "M_EnterSave", lj_ini_geti(lj_ctx_get()->forms_ini, "SAVEMACRO", "SaveOnEnter", 0));
}
static void sv_after(void) { if (lj_ini_geti(lj_ctx_get()->forms_ini, "SAVEMACRO", "AutoClose", 1)) lj_save_genmacro_close(); else sv_fill(); }
static void as_close(void) { if (sv.as) { lj_win_hide(sv.as); lj_window_destroy(sv.as); sv.as = NULL; } }
static uint32_t as_number(void) { lj_widget *n = sv.as ? lj_window_find(sv.as, "eMacroNo") : NULL; const char *t = n ? lj_w_edit_text(n) : NULL; long v = t && *t ? strtol(t, NULL, 10) : 0; return v >= 1 && v <= LJ_MACROS_MAX ? (uint32_t)v : 0; }
static void as_change(lj_widget *w, void *ud) { lj_widget *b = sv.as ? lj_window_find(sv.as, "bSave") : NULL; (void)w; (void)ud; if (b) { b->enabled = as_number() != 0; lj_widget_invalidate(b); } }
static void as_save(lj_widget *w, void *ud) { uint32_t n = as_number(); (void)w; (void)ud; if (n && sv_write(n) == 0) { as_close(); sv_after(); } }
static void as_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) as_close(); }   /* BitBtn2 (bkCancel): no DFM event */
static const lj_binding as_bindings[] = { { "bSaveClick", as_save, NULL }, { "eMacroNoChange", as_change, NULL }, { "FormActivate", lj_handler_noop, NULL },
                                          { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static void sv_click(lj_widget *w, void *ud)                     /* BSaveClick: Save Macro / Save New Macro / Save as Macro # */
{
    const lj_genmacro *g = gm();
    lj_widget *x;
    (void)ud;
    if (!g || !ctx()->lib) return;
    if (named(w, "bSaveAsMacro")) {
        if (!sv.as && (sv.as = lj_window_create(&lj_form_TSAVEASMACRODIALOG, as_bindings)) != NULL) {
            if ((x = lj_window_find(sv.as, "eMacroName")) != NULL) lj_w_edit_set_text(x, g->name);
            if ((x = lj_window_find(sv.as, "BitBtn2")) != NULL) { x->on_event = as_cancel; x->ud = NULL; }
            as_change(NULL, NULL);
            lj_win_show(sv.as, GM_STAY_ON_TOP);
        }
        return;
    }
    if (sv_write(named(w, "Bsave") && g->number ? g->number : lj_library_next_number(ctx()->lib, LJ_REC_MACRO)) == 0) sv_after();
}
static void sv_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sv.win);
    (void)w; (void)ud;
    if (!it || !it->name || !ctx()->forms_ini) return;
    if (!strcmp(it->name, "M_AutoClose")) lj_ini_seti(ctx()->forms_ini, "SAVEMACRO", "AutoClose", !lj_ini_geti(lj_ctx_get()->forms_ini, "SAVEMACRO", "AutoClose", 1));
    else if (!strcmp(it->name, "M_EnterSave")) lj_ini_seti(ctx()->forms_ini, "SAVEMACRO", "SaveOnEnter", !lj_ini_geti(lj_ctx_get()->forms_ini, "SAVEMACRO", "SaveOnEnter", 0));
    sv_fill();
}
static void sv_close_ev(lj_widget *w, void *ud) { (void)w; (void)ud; lj_save_genmacro_close(); }
static const lj_binding sv_bindings[] = { { "BSaveClick", sv_click, NULL }, { "PrefsClick", sv_prefs, NULL }, { "Help1Click", lj_handler_noop, NULL },
                                          { "FormActivate", lj_handler_noop, NULL }, { "FormClose", sv_close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL },
                                          { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
lj_window *lj_save_genmacro_open(void)
{
    if (sv.win) return sv.win;
    if ((sv.win = lj_window_create(&lj_form_TSAVEMACRODIALOG, sv_bindings)) == NULL) { lj_log("genmacro editor: cannot create TSaveMacroDialog"); return NULL; }
    sv_fill();
    lj_win_show(sv.win, GM_STAY_ON_TOP);
    return sv.win;
}
void lj_save_genmacro_close(void) { as_close(); if (!sv.win) return; lj_win_hide(sv.win); lj_window_destroy(sv.win); sv.win = NULL; }
lj_window *lj_save_genmacro_get(void) { return sv.win; }
void lj_save_genmacro_refresh(void) { }

/* ---- Select Macro (TLoadMacroList): No | Macro Name in the library's CueMacros, popup Edit / Add to Cue / Delete [14 sA.8] ---- */
enum { SL_MAX = 1024 };   /* ponytail: the list shows the first 1024 macros of the library (LJ2 allows 9999) */
static struct { lj_window *win; uint32_t no[SL_MAX]; char txt[SL_MAX][44]; const char *rows[SL_MAX]; int n; } sl;
void lj_select_genmacro_refresh(void)
{
    static lj_genmacro tmp;
    char path[LJ_PATH_CAP], err[128], no[12];
    lj_widget *lv;
    int i;
    if (!sl.win) return;
    sl.n = ctx()->lib ? lj_library_list_records(ctx()->lib, LJ_REC_MACRO, sl.no, SL_MAX) : 0;
    if (sl.n < 0) sl.n = 0;
    for (i = 0; i < sl.n; i++) {
        const char *name = "";
        if (lj_library_record_path(ctx()->lib, LJ_REC_MACRO, sl.no[i], path, sizeof path) == 0 && lj_genmacro_load(path, &tmp, err, sizeof err) == 0) name = tmp.name;
        if (lj_number_text(sl.no[i], no, sizeof no) != 0) no[0] = 0;
        snprintf(sl.txt[i], sizeof sl.txt[i], "%s\t%s", no, name);
        sl.rows[i] = sl.txt[i];
    }
    if ((lv = lj_window_find(sl.win, "lvMacros")) != NULL) lj_w_listview_set_items(lv, sl.rows, sl.n);
}
static uint32_t sl_current(void) { lj_widget *lv = sl.win ? lj_window_find(sl.win, "lvMacros") : NULL; int i = lv ? lj_w_listview_index(lv) : -1; return i >= 0 && i < sl.n ? sl.no[i] : 0; }
static void sl_edit(uint32_t n) { if (!n) return; lj_desktop_run_action(LJ_ACT_LOAD_GENMACRO, (int)n); lj_win_open(LJ_WIN_GENMACRO_EDITOR); lj_genmacro_editor_refresh(); }
static void sl_change(lj_widget *w, void *ud)                    /* Browse to Editor / Browse to Cue: a highlighted row loads at once */
{
    uint32_t n = sl_current();
    (void)w; (void)ud;
    if (n && lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToEditor", 0)) sl_edit(n);
    if (n && lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToCue", 0)) lj_desktop_run_action(LJ_ACT_GENMACRO_TO_CUE, (int)n);
}
static void sl_dbl(lj_widget *w, void *ud) { (void)w; (void)ud; if (!lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToEditor", 0) && !lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToCue", 0)) sl_edit(sl_current()); }   /* "sends to the editor when neither browse option is on" */
static void sl_pop(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sl.win);
    uint32_t n = sl_current();
    (void)w; (void)ud;
    if (!it || !it->name || !n) return;
    if (!strcmp(it->name, "popEdit")) sl_edit(n);
    else if (!strcmp(it->name, "popAddToCue")) lj_desktop_run_action(LJ_ACT_GENMACRO_TO_CUE, (int)n);
    else if (!strcmp(it->name, "popDeleteMacro") && ctx()->lib) { if (lj_library_delete_record(ctx()->lib, LJ_REC_MACRO, n) != 0) lj_log("delete macro %04u failed", (unsigned)n); lj_select_genmacro_refresh(); }
}
static void sl_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sl.win);
    const char *key;
    (void)w; (void)ud;
    if (!it || !it->name || !ctx()->main_ini) return;
    key = !strcmp(it->name, "M_BrowseToEdit") ? "BrowseToEditor" : "BrowseToCue";
    lj_ini_seti(ctx()->main_ini, "MACROLIST", key, !lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", key, 0));
    lj_menu_set_checked(lj_window_menus(sl.win), it->name, lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", key, 0));
}
static void sl_close_ev(lj_widget *w, void *ud) { (void)w; (void)ud; lj_select_genmacro_close(); }
static const lj_binding sl_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", sl_close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDeactivate", lj_handler_noop, NULL },
    { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL }, { "lvMacrosChange", sl_change, NULL },
    { "lvMacrosColumnClick", lj_handler_noop, NULL } /* ponytail: rows stay in number order */, { "lvMacrosCompare", lj_handler_noop, NULL }, { "lvMacrosDblClick", sl_dbl, NULL },
    { "M_PrefsClick", sl_prefs, NULL }, { "popLVMacrosPopup", lj_handler_noop, NULL }, { "popMacroClick", sl_pop, NULL },
    { NULL, NULL, NULL } };
lj_window *lj_select_genmacro_open(void)
{
    if (sl.win) return sl.win;
    if ((sl.win = lj_window_create(&lj_form_TLOADMACROLIST, sl_bindings)) == NULL) { lj_log("genmacro editor: cannot create TLoadMacroList"); return NULL; }
    lj_menu_set_checked(lj_window_menus(sl.win), "M_BrowseToEdit", lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToEditor", 0));
    lj_menu_set_checked(lj_window_menus(sl.win), "M_BrowsetoCue", lj_ini_geti(lj_ctx_get()->main_ini, "MACROLIST", "BrowseToCue", 0));
    lj_select_genmacro_refresh();
    lj_win_show(sl.win, GM_STAY_ON_TOP);
    return sl.win;
}
void lj_select_genmacro_close(void) { if (!sl.win) return; lj_win_hide(sl.win); lj_window_destroy(sl.win); sl.win = NULL; }
lj_window *lj_select_genmacro_get(void) { return sl.win; }

/* ---- hotkeys: Generic Macro Functions [01 s7, 05 s11.2, 14 sA.9] ---- */
static void fn_gm(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    const lj_engine_status *v = &ctx()->view;
    (void)prog; (void)ud;
    if (!edge) return;
    switch (fn) {
    case LJ_FN_GENMACRO_ENGINE_OFF:    post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_ENGINE, 0); break;
    case LJ_FN_GENMACRO_ENGINE_ON:     post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_ENGINE, 1); break;
    case LJ_FN_GENMACRO_ENGINE_TOGGLE: post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_ENGINE, !v->gm_engine_on); break;
    case LJ_FN_GENMACRO_SLOT_OFF:      post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_CUE, 0); break;
    case LJ_FN_GENMACRO_SLOT_ON:       post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_CUE, 1); break;
    case LJ_FN_GENMACRO_SLOT_TOGGLE:   post(LJ_CMD_GENMACRO_ON, LJ_GM_SRC_CUE, !v->gm_slot_on); break;
    case LJ_FN_GENMACRO_TOGGLE_EDITOR: lj_win_toggle(LJ_WIN_GENMACRO_EDITOR); break;
    case LJ_FN_GENMACRO_TOGGLE_LIST:   lj_win_toggle(LJ_WIN_SELECT_GENMACRO); break;
    case LJ_FN_GENMACRO_CLEAR_EDITOR:  if (gm()) { lj_lock(); memset(gm(), 0, sizeof *gm()); lj_unlock(); apply(); lj_genmacro_editor_refresh(); } break;
    default: break;
    }
}
void lj_genmacro_editor_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_GENMACRO_ENGINE_OFF, LJ_FN_GENMACRO_ENGINE_ON, LJ_FN_GENMACRO_ENGINE_TOGGLE, LJ_FN_GENMACRO_TOGGLE_EDITOR,
                                       LJ_FN_GENMACRO_TOGGLE_LIST, LJ_FN_GENMACRO_CLEAR_EDITOR, LJ_FN_GENMACRO_SLOT_OFF, LJ_FN_GENMACRO_SLOT_ON, LJ_FN_GENMACRO_SLOT_TOGGLE };
    int i;
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_gm, NULL);
}
