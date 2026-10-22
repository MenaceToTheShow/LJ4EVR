/* lj_movemacro_editor.c - lj_movemacro_editor.h: the Movement Macro window with the Bezier shape editor and its
 * TBezNameForm, Delete Macros (the .bez shapes), Fan Out, Pan/Tilt Macro Auto Delay and Cue Macro Control
 * (04 s4.2-4.6, 03 s5.11, 17). Every writer edits the current scene under lj_lock (seam 5 of docs/w3-contracts.md);
 * Fan Out and Auto Delay then post CURRENT_SEQ through lj_ctx_set_scene, the Movement Macro edits are read in place
 * by the engine (like the Movement control); the Cue Macro Control posts MACRO_CTL / MACRO_FADE. The pure maths is
 * model/lj_movemacro.h (Fan Out, Auto Delay, Bezier) and engine/lj_movemacro.h (the trace). Sizes of the code-built
 * window are PROPOSAL rig (movementmacro.gif / beziershapeeditor.gif are reduced captures, not in 07 s14). */
#include "windows/lj_movemacro_editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl_movement.h"
#include "core/lj_fsinfo.h"
#include "core/lj_lock.h"
#include "core/lj_locale.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_movemacro.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_genmacro.h"
#include "model/lj_movemacro.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_dragbar.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_fader.h"
#include "ui/widgets/lj_w_osf.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */
#define LJ_MM_CAPTION "Movement Macro"   /* exe 0x26AE24 */

static void help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("movement: Help: no help file is shipped (D2)"); }

/* ---------------------------------------------------------------------------------------------------------------
 * shared: the current scene, the selected fixtures that have pan / tilt, their position in readout units (17 s1)
 * ------------------------------------------------------------------------------------------------------------- */
static lj_scene *cur_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    if (!c->show || !c->show->current_seq || !c->show->current_seq->scenes || c->current_scene >= c->show->current_seq->n) return NULL;
    return &c->show->current_seq->scenes[c->current_scene];
}

static const lj_profile *pt_profile(int f)
{
    const lj_show *show = lj_ctx_get()->show;
    const lj_profile *p = show && f >= 0 && f < LJ_FIXTURES && show->fixture[f].link ? lj_show_profile(show, f) : NULL;
    return p && (p->pt16 || p->pt8) ? p : NULL;
}

static int sel_fixtures(int *out)   /* master first, then the other selected fixtures with pan / tilt, by number */
{
    int i, n = 0, m = lj_sel_master();
    if (m >= 0 && pt_profile(m)) out[n++] = m;
    for (i = 0; i < LJ_FIXTURES; i++) if (i != m && lj_sel_is_selected(i) && pt_profile(i)) out[n++] = i;
    return n;
}

static int pt_read(const lj_scene *sc, int f, int32_t *pan, int32_t *tilt)
{
    const lj_profile *p = pt_profile(f);
    const lj_fixture *fx;
    uint8_t o[4];
    int base;
    if (!p || !sc) return -1;
    fx = &lj_ctx_get()->show->fixture[f];
    base = lj_addr_code(fx->link, fx->address);
    lj_pos_patch16(p, o);
    if (!p->pt16) { o[0] = p->pan_ofs; o[2] = p->tilt_ofs; }
    if (base + o[0] >= LJ_CHANS || base + o[1] >= LJ_CHANS || base + o[2] >= LJ_CHANS || base + o[3] >= LJ_CHANS) return -1;
    *pan  = p->pt16 ? lj_pos_from_dmx16((uint16_t)(sc->val[base + o[0]] << 8 | sc->val[base + o[1]])) : lj_pos_from_dmx8(sc->val[base + o[0]]);
    *tilt = p->pt16 ? lj_pos_from_dmx16((uint16_t)(sc->val[base + o[2]] << 8 | sc->val[base + o[3]])) : lj_pos_from_dmx8(sc->val[base + o[2]]);
    return 0;
}

static void put(lj_scene *sc, int ch, uint8_t v) { sc->val[ch] = v; if (sc->state[ch] == LJ_OFF) sc->state[ch] = LJ_SNAP; }
static void pt_write(lj_scene *sc, int f, int32_t pan, int32_t tilt)   /* ME-22: a 16-bit pair is written as one value */
{
    const lj_profile *p = pt_profile(f);
    const lj_fixture *fx;
    uint8_t o[4];
    int base;
    uint16_t pp = lj_pos_to_dmx16(pan), tt = lj_pos_to_dmx16(tilt);
    if (!p || !sc) return;
    fx = &lj_ctx_get()->show->fixture[f];
    base = lj_addr_code(fx->link, fx->address);
    lj_pos_patch16(p, o);
    if (base + o[1] >= LJ_CHANS || base + o[3] >= LJ_CHANS) return;
    if (p->pt16) { put(sc, base + o[0], (uint8_t)(pp >> 8)); put(sc, base + o[1], (uint8_t)pp); put(sc, base + o[2], (uint8_t)(tt >> 8)); put(sc, base + o[3], (uint8_t)tt); }
    else { put(sc, base + p->pan_ofs, (uint8_t)(pp >> 8)); put(sc, base + p->tilt_ofs, (uint8_t)(tt >> 8)); }
    sc->preset[f] = 0;                                       /* a pan / tilt change replaces a preset reference (04 s4.3) */
}

static void scene_modified(void)
{
    lj_ctx *c = lj_ctx_get();
    if (c->show && c->show->current_seq) c->show->current_seq->modified = 1;
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
}

static int32_t px_of(int32_t v, int span) { return (int32_t)(((int64_t)v + 32768) * (span - 1) / 65535); }
static void dot(lj_surface *s, int x, int y, int r, uint32_t rgb) { lj_fill(s, x - r, y - r, 2 * r + 1, 2 * r + 1, rgb); }

/* ---------------------------------------------------------------------------------------------------------------
 * Movement Macro (code-built TMovementMacroForm) + the Bezier shape editor + TBezNameForm
 * ------------------------------------------------------------------------------------------------------------- */
enum { MB_PAN_AMP = 0, MB_PAN_CYC, MB_TILT_AMP, MB_TILT_CYC, MB_DELAY, MB_TWEAK, MB_STEP, MB_COUNT };
enum { MM_CLIENT_W = 441, MM_CLIENT_H = 199, MM_BEZ_W = 199, MM_TRACE = 128 };   /* PROPOSAL rig: movementmacro.gif 447x243 less the frame */
static struct {
    lj_window *win, *name_win;
    lj_form_desc desc;
    lj_widget *type, *bar[MB_COUNT], *osf[6], *dir[4], *tab_macro, *tab_fix, *trace, *canvas;
    lj_widget_vt barvt; void (*bar_paint0)(lj_widget *w, lj_surface *s);
    int fx[LJ_FIXTURES], nfx; uint32_t serial; int filling, show_all, fixture_tab;
    const char *items[LJ_MM_TYPE_BEZ0 + LJ_BEZ_SHAPES_MAX + 1]; int nitems; char item_text[LJ_BEZ_SHAPES_MAX + 1][40];
    int bez_open, drag, save_as; lj_bez edit; lj_bez_table *edit_t; lj_bez_set edit_set;
} mm;

static lj_movemacro *master_macro(lj_scene *sc) { return sc && mm.nfx ? &sc->macro[mm.fx[0]] : NULL; }
static const lj_bez_set *bez_set(void) { return mm.bez_open && mm.edit_t ? &mm.edit_set : lj_ctx_get()->show ? lj_ctx_get()->show->bez : NULL; }
static int sys_type(int t) { return t == LJ_MM_TYPE_ELLIPSE || t == LJ_MM_TYPE_STEPPED_ELLIPSE || t >= LJ_MM_TYPE_BEZ0; }   /* one common cycle (CHM 3197) */

static int mm_bar_value(int i, const lj_movemacro *m)
{
    switch (i) {
    case MB_PAN_AMP: return m->amp_pan;   case MB_PAN_CYC: return m->cycle_pan;
    case MB_TILT_AMP: return m->amp_tilt; case MB_TILT_CYC: return m->cycle_tilt;
    case MB_DELAY: return m->delay;       case MB_TWEAK: return m->tweak;
    default: return m->step ? m->step - 1 : 0;
    }
}
static void pct_text(int byte, char *dst, size_t cap, const char *what) { int t = byte * 1000 / 256; snprintf(dst, cap, "%s %d%c%d%%", what, t / 10, lj_locale_decimal(), t % 10); }
static void cyc_text(const char *what, int byte, char *dst, size_t cap) { int cs = byte * 10; snprintf(dst, cap, "%s %d%c%02d sec", what, cs / 100, lj_locale_decimal(), cs % 100); }   /* cycle byte in 0,1 s (LJ_MOVEMACRO_CYCLE_UNIT_US, PROPOSAL rig 17 s8 item 2) */
static void mm_text(const lj_widget *w, char *dst, size_t cap)
{
    int i, v = lj_w_dragbar_value(w), t = mm.type ? ((const lj_w_combobox *)mm.type)->index : 0;
    for (i = 0; i < MB_COUNT && mm.bar[i] != w; i++) { }
    switch (i) {
    case MB_PAN_AMP:  snprintf(dst, cap, "Pan Amplitude %d", v); break;    /* exe 0x26AECC */
    case MB_TILT_AMP: snprintf(dst, cap, "Tilt Amplitude %d", v); break;
    case MB_PAN_CYC:  cyc_text(sys_type(t) ? "Cycle:" : "Pan Cycle:", v, dst, cap); break;   /* exe 0x26B4CC / 0x26B4DC */
    case MB_TILT_CYC: cyc_text("Tilt Cycle:", v, dst, cap); break;
    case MB_DELAY:    pct_text(v, dst, cap, "Delay"); break;
    case MB_TWEAK:    pct_text(v, dst, cap, "Tweak"); break;
    default:          snprintf(dst, cap, "Macro Stepsize %d", v + 1); break;   /* exe 0x26AF90 */
    }
}
static void mm_bar_paint(lj_widget *w, lj_surface *s)
{
    const lj_w_dragbar *b = (const lj_w_dragbar *)w;
    char text[64];
    int ax, ay, ix = LJ_DRAGBAR_ARROW + 3, iw = w->w - 2 * LJ_DRAGBAR_ARROW - 4, fw;
    mm.bar_paint0(w, s);
    lj_widget_abs(w, &ax, &ay);
    fw = b->max > 0 ? (iw * b->value + b->max / 2) / b->max : 0;
    lj_fill(s, ax + ix, ay + 2, iw, w->h - 4, LJ_CLR_BTNSHADOW);
    lj_fill(s, ax + ix, ay + 2, fw, w->h - 4, b->fill);
    mm_text(w, text, sizeof text);
    lj_w_text_c(s, w, ax + ix, ay, iw, w->h, text, LJ_CLR_3DDKSHADOW, 0);
}

/* the Macro Type drop-down: "No Macro", the six system macros, then the Bezier shapes of show->bez [04 s4.4] */
static void fill_types(void)
{
    const lj_bez_set *b = lj_ctx_get()->show ? lj_ctx_get()->show->bez : NULL;
    int i, n = b ? b->n : 0;
    if (n > LJ_BEZ_SHAPES_MAX) n = LJ_BEZ_SHAPES_MAX;
    for (i = 0; i < LJ_MM_TYPE_BEZ0; i++) mm.items[i] = lj_mm_type_names[i];
    for (i = 0; i < n; i++) { lj_str_copy(mm.item_text[i], sizeof mm.item_text[i], b->t[i].name); mm.items[LJ_MM_TYPE_BEZ0 + i] = mm.item_text[i]; }
    mm.nitems = LJ_MM_TYPE_BEZ0 + n;
    if (mm.bez_open) { snprintf(mm.item_text[n], sizeof mm.item_text[n], "%s", mm.edit.name); mm.items[mm.nitems++] = mm.item_text[n]; }   /* "New Shape (4)" (beziershapeeditor.gif) */
    lj_w_combobox_set_items(mm.type, mm.items, mm.nitems);
}

static void mm_fill(void)
{
    lj_scene *sc;
    lj_movemacro m;
    int i, t;
    if (!mm.win) return;
    mm.nfx = sel_fixtures(mm.fx);
    mm.serial = lj_sel_serial();
    lj_lock();
    sc = cur_scene();
    memset(&m, 0, sizeof m);
    if (master_macro(sc)) m = *master_macro(sc);
    lj_unlock();
    mm.filling = 1;
    fill_types();
    t = m.type < mm.nitems ? m.type : -1;                    /* a shape that is gone (deleted, not reloaded): no row, "load error" caption [03 s5.1] */
    ((lj_w_combobox *)mm.type)->index = mm.bez_open ? mm.nitems - 1 : t;
    ((lj_w_combobox *)mm.type)->text = t < 0 ? "load error" : NULL;
    for (i = 0; i < MB_COUNT; i++) { lj_w_dragbar_set_value(mm.bar[i], mm_bar_value(i, &m)); lj_widget_invalidate(mm.bar[i]); }
    for (i = 0; i < 6; i++) lj_w_osf_set_state(mm.osf[i], m.fstate[i]);
    lj_w_speedbutton_set_down(mm.dir[0], !m.dir); lj_w_speedbutton_set_down(mm.dir[1], m.dir != 0);
    lj_w_speedbutton_set_down(mm.dir[2], !m.dir); lj_w_speedbutton_set_down(mm.dir[3], m.dir != 0);
    t = mm.bez_open ? LJ_MM_TYPE_BEZ0 : m.type;
    mm.bar[MB_TILT_CYC]->visible = !sys_type(t);             /* Ellipse / Stepped Ellipse / Bezier: one cycle (bezier editor capture) */
    mm.osf[LJ_MMP_TILT_CYCLE]->visible = mm.dir[2]->visible = mm.dir[3]->visible = mm.bar[MB_TILT_CYC]->visible;
    mm.bar[MB_STEP]->visible = t == LJ_MM_TYPE_STEPPED_ELLIPSE;   /* Stepped Ellipse only [04 s4.4]; it takes the Tweak row (PROPOSAL rig 17 s8 item 8) */
    mm.bar[MB_TWEAK]->visible = !mm.bar[MB_STEP]->visible;
    mm.filling = 0;
    lj_window_invalidate_all(mm.win);
}

/* one edit on every selected fixture's macro in the current scene; the engine reads it in place (design 3.4) */
static void mm_apply(int what, int v)
{
    lj_scene *sc;
    int i;
    if (mm.filling || !mm.nfx) return;
    lj_lock();
    sc = cur_scene();
    for (i = 0; sc && i < mm.nfx; i++) {
        lj_movemacro *m = &sc->macro[mm.fx[i]];
        int k;
        switch (what) {
        case 100: if (!m->type && v) { m->amp_pan = m->amp_tilt = 64; m->cycle_pan = m->cycle_tilt = 50; m->step = LJ_MM_STEP_MAX; }   /* PROPOSAL defaults of a fresh macro */
                  m->type = (uint8_t)v;
                  for (k = 0; k < 6; k++) if (v && m->fstate[k] == LJ_OFF) m->fstate[k] = LJ_SNAP;   /* choosing a type programs it (PROPOSAL) */
                  break;
        case 101: m->dir = (uint8_t)v; break;
        case MB_PAN_AMP: m->amp_pan = (uint8_t)v; break;
        case MB_PAN_CYC: m->cycle_pan = (uint8_t)v; if (sys_type(m->type)) m->cycle_tilt = (uint8_t)v; break;
        case MB_TILT_AMP: m->amp_tilt = (uint8_t)v; break;
        case MB_TILT_CYC: m->cycle_tilt = (uint8_t)v; break;
        case MB_DELAY: m->delay = (uint8_t)v; break;
        case MB_TWEAK: m->tweak = (uint8_t)v; break;
        case MB_STEP: m->step = (uint16_t)(v + 1); break;
        default: if (what >= 200 && what < 206) m->fstate[what - 200] = (uint8_t)v; break;   /* O/S/F of a row */
        }
    }
    if (sc) scene_modified();
    lj_unlock();
    lj_widget_invalidate(mm.trace);
}

/* ---- the trace window: Macro tab = the macro alone around the centre, Fixture tab = overlaid on the master [04 s4.4] ---- */
static void trace_paint(lj_widget *w, lj_surface *s)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc = cur_scene();
    int ax, ay, i, k;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, 0x0000FFu);
    lj_hline(s, ax, ay + w->h / 2, w->w, 0xFF0000u); lj_vline(s, ax + w->w / 2, ay, w->h, 0xFF0000u);   /* red centre cross: movementmacro.gif (1:1) col 69 / row 107 */
    for (k = 0; sc && k < mm.nfx && (k == 0 || mm.show_all); k++) {
        int f = mm.fx[k];
        lj_movemacro m = sc->macro[f];
        lj_mm_state st;
        int32_t bp = 0, bt = 0, p, t;
        if (mm.bez_open && k == 0) { m.type = (uint8_t)LJ_MM_TYPE_BEZ0; if (m.fstate[0] == LJ_OFF) m.fstate[0] = LJ_SNAP; }   /* the shape on the editor */
        if (mm.fixture_tab) pt_read(sc, f, &bp, &bt);
        if (k == 0) for (i = 0; i < LJ_MOVEMACRO_N_TABLE; i += 32) {   /* the figure, 128 points */
            st.idx_pan = st.idx_tilt = (uint32_t)i << 16;
            if (lj_movemacro_pos(&st, &m, mm.bez_open ? &mm.edit_set : bez_set(), 100, bp, bt, &p, &t) == 0)
                lj_fill(s, ax + px_of(p, w->w), ay + px_of(t, w->h), 1, 1, 0x00FFFFu);
        }
        st.idx_pan = (uint32_t)c->view.mm_idx_pan[f] << 16; st.idx_tilt = (uint32_t)c->view.mm_idx_tilt[f] << 16;   /* where the engine is now */
        if (lj_movemacro_pos(&st, &m, mm.bez_open ? &mm.edit_set : bez_set(), 100, bp, bt, &p, &t) == 0 || mm.fixture_tab)
            dot(s, ax + px_of(p, w->w), ay + px_of(t, w->h), 2, k == 0 ? 0xFFFFFFu : 0xFF0000u);   /* master white, the others red (Show All Selected Fixtures) */
    }
}
static const lj_widget_vt trace_vt = { trace_paint, NULL, NULL, NULL };

/* ---- the Bezier canvas [04 s4.5]: square nodes, round handles, drag to reshape; right-click adds a node after the
 * nearest one, right-click on a node removes it (3 minimum), Shift+right-click toggles its segment line / curve
 * (PROPOSAL: LJ2 has a popup for these, its items are not in the captions) ---- */
static int32_t cv_val(int px, int span) { return (int32_t)((int64_t)px * 65535 / (span - 1)) - 32768; }
static int cv_hit(lj_widget *w, int x, int y)                /* node * 3 + 0 node, 1 in handle, 2 out handle; -1 */
{
    int i, k;
    for (i = 0; i < mm.edit.n; i++) {
        const lj_bez_node *nd = &mm.edit.node[i];
        const int16_t px[3][2] = { { nd->x, nd->y }, { nd->in_x, nd->in_y }, { nd->out_x, nd->out_y } };
        for (k = 0; k < 3; k++) {
            int dx = px_of(px[k][0], w->w) - x, dy = px_of(px[k][1], w->h) - y;
            if (dx >= -3 && dx <= 3 && dy >= -3 && dy <= 3) return i * 3 + k;
        }
    }
    return -1;
}
static void cv_resample(void)
{
    if (!mm.edit_t) { mm.edit_t = (lj_bez_table *)calloc(1, sizeof *mm.edit_t); if (!mm.edit_t) lj_fatal("movement: out of memory"); }
    if (lj_bez_sample(&mm.edit, mm.edit_t) == 0) { mm.edit_set.n = 1; mm.edit_set.t = mm.edit_t; }
    lj_widget_invalidate(mm.canvas); lj_widget_invalidate(mm.trace);
}
static void canvas_paint(lj_widget *w, lj_surface *s)
{
    int ax, ay, i, j, k;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, 0x0000FFu);
    for (k = 1; k < 8; k++) { lj_vline(s, ax + k * w->w / 8, ay, w->h, 0x4040FFu); lj_hline(s, ax, ay + k * w->h / 8, w->w, 0x4040FFu); }
    for (i = 0; i < mm.edit.n; i++) {
        const lj_bez_node *nd = &mm.edit.node[i];
        for (j = 0; j < 64; j++) { int32_t x, y; if (lj_bez_eval(&mm.edit, i, j, 64, &x, &y) == 0) lj_fill(s, ax + px_of(x, w->w), ay + px_of(y, w->h), 1, 1, 0x00FFFFu); }
        dot(s, ax + px_of(nd->in_x, w->w), ay + px_of(nd->in_y, w->h), 1, 0xFF0000u);
        dot(s, ax + px_of(nd->out_x, w->w), ay + px_of(nd->out_y, w->h), 1, 0xFF0000u);
        lj_fill(s, ax + px_of(nd->x, w->w) - 2, ay + px_of(nd->y, w->h) - 2, 5, 5, 0xFFFF00u);
    }
}
static int canvas_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int h;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (!lj_w_inside(w, m->x, m->y)) return 0;
        h = cv_hit(w, m->x, m->y);
        if (m->button == 1) { mm.drag = h; return 1; }
        if (m->button != 2) return 0;
        if (h >= 0 && h % 3 == 0 && m->shift) mm.edit.node[h / 3].line ^= 1u;
        else if (h >= 0 && h % 3 == 0) { if (mm.edit.n > 3) { memmove(&mm.edit.node[h / 3], &mm.edit.node[h / 3 + 1], sizeof mm.edit.node[0] * (size_t)(mm.edit.n - h / 3 - 1)); mm.edit.n--; } }
        else if (mm.edit.n < LJ_BEZ_NODES_MAX) {
            int i, best = 0; int64_t bd = -1;
            int32_t x = cv_val(m->x, w->w), y = cv_val(m->y, w->h);
            for (i = 0; i < mm.edit.n; i++) { int64_t dx = mm.edit.node[i].x - x, dy = mm.edit.node[i].y - y; if (bd < 0 || dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; } }
            memmove(&mm.edit.node[best + 2], &mm.edit.node[best + 1], sizeof mm.edit.node[0] * (size_t)(mm.edit.n - best - 1));
            mm.edit.node[best + 1].x = mm.edit.node[best + 1].in_x = mm.edit.node[best + 1].out_x = (int16_t)x;
            mm.edit.node[best + 1].y = mm.edit.node[best + 1].in_y = mm.edit.node[best + 1].out_y = (int16_t)y;
            mm.edit.node[best + 1].line = 0;
            mm.edit.n++;
        }
        cv_resample();
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (mm.drag < 0) return 0;
        {
            lj_bez_node *nd = &mm.edit.node[mm.drag / 3];
            int16_t x = (int16_t)lj_pos_clamp(cv_val(m->x, w->w)), y = (int16_t)lj_pos_clamp(cv_val(m->y, w->h));
            if (mm.drag % 3 == 0) { nd->in_x = (int16_t)lj_pos_clamp(nd->in_x + x - nd->x); nd->in_y = (int16_t)lj_pos_clamp(nd->in_y + y - nd->y);   /* a node carries its handles */
                                    nd->out_x = (int16_t)lj_pos_clamp(nd->out_x + x - nd->x); nd->out_y = (int16_t)lj_pos_clamp(nd->out_y + y - nd->y); nd->x = x; nd->y = y; }
            else if (mm.drag % 3 == 1) { nd->in_x = x; nd->in_y = y; }
            else { nd->out_x = x; nd->out_y = y; }
        }
        cv_resample();
        return 1;
    case LJ_EV_MOUSE_UP: case LJ_EV_MOUSE_LEAVE:
        mm.drag = -1;
        return 1;
    default:
        return 0;
    }
}
static const lj_widget_vt canvas_vt = { canvas_paint, canvas_mouse, NULL, NULL };

int lj_movemacro_editor_bez(lj_bez *out) { if (!mm.bez_open || !out) return -1; *out = mm.edit; return 0; }

static void bez_editor(int open)
{
    if (!mm.win || open == mm.bez_open) return;
    mm.bez_open = open;
    mm.canvas->visible = (unsigned)open;
    lj_window_resize(mm.win, MM_CLIENT_W + (open ? MM_BEZ_W : 0), MM_CLIENT_H);   /* "extends the macro window to the right" [04 s4.5] */
    if (open) cv_resample();
    mm_fill();
}

static int bez_dir(char *out, size_t cap) { lj_ctx *c = lj_ctx_get(); return c->lib ? lj_library_path(c->lib, LJ_BEZ_DIR, out, cap) : -1; }

static void bez_new(void)
{
    static const int16_t xy[4][2] = { { 0, -16384 }, { 16384, 0 }, { 0, 16384 }, { -16384, 0 } };   /* the diamond of beziershapeeditor.gif */
    const lj_bez_set *b = lj_ctx_get()->show ? lj_ctx_get()->show->bez : NULL;
    int k;
    memset(&mm.edit, 0, sizeof mm.edit);
    snprintf(mm.edit.name, sizeof mm.edit.name, "New Shape (%d)", (b ? b->n : 0) + 1);   /* exe 0x26C684 "New Shape (" + n + ")" */
    for (k = 0; k < 4; k++) { lj_bez_node *n = &mm.edit.node[k]; n->x = n->in_x = n->out_x = xy[k][0]; n->y = n->in_y = n->out_y = xy[k][1]; n->line = 1; }
    mm.edit.n = 4;
    mm.save_as = 1;
    if (mm.bez_open) { cv_resample(); mm_fill(); } else bez_editor(1);
}

static int bez_edit_existing(int type)                       /* a Bezier type picked: its file into the editor */
{
    const lj_bez_set *b = lj_ctx_get()->show ? lj_ctx_get()->show->bez : NULL;
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP], err[160];
    if (!b || type - LJ_MM_TYPE_BEZ0 >= b->n || bez_dir(dir, sizeof dir) || lj_bez_path(dir, b->t[type - LJ_MM_TYPE_BEZ0].name, path, sizeof path)) return -1;
    if (lj_bez_load(path, &mm.edit, err, sizeof err)) return -1;
    lj_str_copy(mm.edit.name, sizeof mm.edit.name, b->t[type - LJ_MM_TYPE_BEZ0].name);
    mm.save_as = 0;
    if (mm.bez_open) cv_resample(); else bez_editor(1);
    return 0;
}

int lj_movemacro_editor_save_shape(const char *name)        /* Save Shape / Save as new Shape with the name; then the engine's set is reloaded */
{
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
    if (!mm.bez_open || bez_dir(dir, sizeof dir) || lj_fs_mkdir(dir) || lj_bez_path(dir, name, path, sizeof path)) { lj_log("movement: cannot save shape \"%s\"", name ? name : ""); return -1; }
    lj_str_copy(mm.edit.name, sizeof mm.edit.name, name);
    if (lj_bez_save(path, &mm.edit)) return -1;
    mm.save_as = 0;
    lj_desktop_run_action(LJ_ACT_BEZ_RELOAD, 0);             /* the app rebuilds show->bez under lj_lock (lj_app_bez_reload) */
    {   /* the master's type becomes the saved shape */
        const lj_bez_set *b = lj_ctx_get()->show ? lj_ctx_get()->show->bez : NULL;
        int i;
        for (i = 0; b && i < b->n; i++) if (!strcmp(b->t[i].name, name)) { mm_apply(100, LJ_MM_TYPE_BEZ0 + i); break; }
    }
    mm_fill();
    return 0;
}

static void on_name_save(lj_widget *w, void *ud)
{
    const char *t = mm.name_win ? lj_w_edit_text(lj_window_find(mm.name_win, "EName")) : NULL;
    (void)w; (void)ud;
    if (!t || !t[0]) return;
    if (lj_movemacro_editor_save_shape(t) == 0 && mm.name_win) { lj_win_hide(mm.name_win); lj_window_destroy(mm.name_win); mm.name_win = NULL; }
}
static void on_name_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK && mm.name_win) { lj_win_hide(mm.name_win); lj_window_destroy(mm.name_win); mm.name_win = NULL; } }
static void on_name_pref(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    (void)ud;
    if (it && it->name) lj_menu_set_checked(lj_window_menus(lj_window_of(w)), it->name, it->checked != LJ_YES);   /* ponytail: the check shows, Enter already saves */
}
static const lj_binding lj_bezname_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL } /* Save on [Enter]: bSave is the Default button */,
    { "bSaveClick", on_name_save, NULL }, { "PrefsClick", on_name_pref, NULL }, { NULL, NULL, NULL } };

lj_window *lj_movemacro_editor_name_window(void) { return mm.name_win; }

static void name_dialog(void)
{
    lj_widget *e;
    if (mm.name_win) return;
    if ((mm.name_win = lj_window_create(&lj_form_TBEZNAMEFORM, lj_bezname_bindings)) == NULL) return;
    lj_window_set_caption(mm.name_win, mm.save_as ? "Save As New Shape" : "Save Shape");   /* exe 0x26CCEC / 0x26C7E0; the DFM's design caption is 'BezNameForm' */
    if ((e = lj_window_find(mm.name_win, "EName")) != NULL) lj_w_edit_set_text(e, mm.save_as ? "" : mm.edit.name);
    if ((e = lj_window_find(mm.name_win, "bcancel")) != NULL) e->on_event = on_name_cancel;   /* ModalResult = mrCancel in the DFM */
    lj_win_show(mm.name_win, LJ_WIN_STAY_ON_TOP);
}

static void on_delete_shape(int yes, void *ud)
{
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
    (void)ud;
    if (!yes || bez_dir(dir, sizeof dir) || lj_bez_path(dir, mm.edit.name, path, sizeof path)) return;
    if (remove(path) == 0) lj_desktop_run_action(LJ_ACT_BEZ_RELOAD, 0);
    bez_editor(0);
}

static void mm_menu(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    const char *n = it && it->name ? it->name : "";
    (void)ud;
    if (!strcmp(n, "M_Fades")) lj_win_open(LJ_WIN_SEQ_FADES);              /* the Sequence Fades control [04 s1.4] */
    else if (!strcmp(n, "M_ShowAll")) { mm.show_all = !mm.show_all; lj_menu_set_checked(lj_window_menus(mm.win), "M_ShowAll", mm.show_all); lj_widget_invalidate(mm.trace); }
    else if (!strcmp(n, "M_AutoDelay")) lj_win_open(LJ_WIN_AUTO_DELAY);
    else if (!strcmp(n, "M_NewShape")) bez_new();
    else if (!strcmp(n, "M_SaveShape")) { if (mm.bez_open) { if (mm.save_as) name_dialog(); else lj_movemacro_editor_save_shape(mm.edit.name); } }
    else if (!strcmp(n, "M_SaveAs")) { if (mm.bez_open) { mm.save_as = 1; name_dialog(); } }
    else if (!strcmp(n, "M_DeleteShape")) { if (mm.bez_open && !mm.save_as) lj_win_confirm(LJ_MM_CAPTION, "Delete this shape", "", on_delete_shape, NULL); }   /* exe 0x26CA14 */
    else if (!strcmp(n, "M_DeleteShapes")) lj_win_open(LJ_WIN_DELETE_MOVEMACRO);
    else if (!strcmp(n, "M_Help")) help(w, ud);
}

static void mm_widget(lj_widget *w, int ev, void *ud)
{
    const char *n = w->name ? w->name : "";
    int i;
    (void)ud;
    if (mm.filling) return;
    if (w == mm.type && ev == LJ_WEV_CHANGE) {
        int t = ((lj_w_combobox *)w)->index;
        if (mm.bez_open && t == mm.nitems - 1) return;       /* the unsaved shape itself */
        if (t >= LJ_MM_TYPE_BEZ0) { bez_edit_existing(t); }
        else bez_editor(0);
        mm_apply(100, t < 0 ? 0 : t);
        mm_fill();
        return;
    }
    for (i = 0; i < MB_COUNT; i++) if (w == mm.bar[i] && ev == LJ_WEV_CHANGE) { mm_apply(i, lj_w_dragbar_value(w)); lj_widget_invalidate(w); return; }
    if (ev != LJ_WEV_CLICK) return;
    for (i = 0; i < 6; i++) if (w == mm.osf[i]) { mm_apply(200 + i, lj_w_osf_state(w)); return; }
    if (!strcmp(n, "MmCW") || !strcmp(n, "MmCW2")) { mm_apply(101, 0); mm_fill(); }
    else if (!strcmp(n, "MmCCW") || !strcmp(n, "MmCCW2")) { mm_apply(101, 1); mm_fill(); }
    else if (w == mm.tab_macro || w == mm.tab_fix) { mm.fixture_tab = w == mm.tab_fix; lj_widget_invalidate(mm.trace); }
    else if (!strcmp(n, "MmClear")) lj_widget_invalidate(mm.trace);   /* Clear Image: the trace is redrawn from the macro every paint (ponytail: no trail to clear) */
}

/* ---- the code-built form: menu captions from the exe (0x301F04..0x301FD4), "Fades" and "Help" as the capture ---- */
static const lj_menu_item_desc mm_pref_items[] = { { "M_ShowAll", "Show All Selected Fixtures", .on_click = "MenuClick" } };
static const lj_menu_item_desc mm_bez_items[] = {
    { "M_NewShape", "&New Shape", .on_click = "MenuClick" }, { "M_SaveShape", "&Save Shape", .on_click = "MenuClick" },
    { "M_SaveAs", "Save &as new Shape", .on_click = "MenuClick" }, { "N1", "-", .on_click = NULL },
    { "M_DeleteShape", "&Delete Shape", .on_click = "MenuClick" }, { "M_DeleteShapes", "&Delete Shapes", .on_click = "MenuClick" } };
static const lj_menu_item_desc mm_menu_items[] = {
    { "M_Fades", "&Fades", .on_click = "MenuClick" },
    { "M_Preferences", "&Preferences", .children = mm_pref_items, .nchildren = LJ_COUNT_OF(mm_pref_items) },
    { "M_AutoDelay", "&Auto delay", .on_click = "MenuClick" },
    { "M_Bezier", "&B\xE9zier Shapes", .children = mm_bez_items, .nchildren = LJ_COUNT_OF(mm_bez_items) },
    { "M_Help", "&Help", .on_click = "MenuClick" } };
static const lj_menu_desc mm_menus[] = { { "MainMenu1", 1, mm_menu_items, LJ_COUNT_OF(mm_menu_items) } };

/* rows: y of each O/S/F + bar row (client px, measured on movementmacro.gif at its capture scale: PROPOSAL rig) */
static const int mm_row_y[7] = { 2, 29, 55, 83, 109, 141, 167 };
static const lj_ctl_desc d_mm_type_l = { LJ_K_LABEL, "MmTypeLabel", 190, 6, 60, 13, .caption = "Macro Type" };          /* exe 0x26AEB4 */
static const lj_ctl_desc d_mm_type   = { LJ_K_COMBOBOX, "MmType", 252, 2, 188, 21, .style = "csDropDownList", .drop_down_count = 16 };
static const lj_ctl_desc d_mm_trace  = { LJ_K_UNKNOWN, "MmTrace", 3, 2, MM_TRACE, 131, .tag = 0 };   /* .tag: gcc/clang -Wmissing-field-initializers */
static const lj_ctl_desc d_mm_tabm   = { LJ_K_SPEEDBUTTON, "MmTabMacro", 3, 135, 64, 24, .caption = "Macro", .group_index = 3, .down = LJ_YES, .hint = "Track Macro Only", .show_hint = LJ_YES };            /* exe 0x26AE40 / 0x26AE54 */
static const lj_ctl_desc d_mm_tabf   = { LJ_K_SPEEDBUTTON, "MmTabFixture", 67, 135, 64, 24, .caption = "Fixture", .group_index = 3, .hint = "Track Fixture Pan/Tilt", .show_hint = LJ_YES };      /* exe 0x26AE70 / 0x26AE80 */
static const lj_ctl_desc d_mm_clear  = { LJ_K_BUTTON, "MmClear", 3, 161, MM_TRACE, 36, .caption = "Clear Image" };       /* exe 0x26AEA0 */
static const lj_ctl_desc d_mm_canvas = { LJ_K_UNKNOWN, "MmBezier", MM_CLIENT_W + 2, 2, MM_BEZ_W - 4, MM_CLIENT_H - 4, .tag = 0 };

lj_window *lj_movemacro_editor_open(void)
{
    static const char *const names[MB_COUNT] = { "MmPanAmp", "MmPanCycle", "MmTiltAmp", "MmTiltCycle", "MmDelay", "MmTweak", "MmStep" };
    static const int bar_row[MB_COUNT] = { 1, 2, 3, 4, 5, 6, 6 }, bar_max[MB_COUNT] = { LJ_MOVEMACRO_AMP_MAX, 127, LJ_MOVEMACRO_AMP_MAX, 127, 255, 255, LJ_MM_STEP_MAX - 1 };
    static const int osf_row[6] = { 0, 1, 2, 3, 4, 5 };
    static lj_ctl_desc bars[MB_COUNT], osfs[6], dirs[4];
    static const char *const dir_names[4] = { "MmCCW", "MmCW", "MmCCW2", "MmCW2" };
    lj_widget *root;
    int i;
    lj_binding b[] = { { "MenuClick", mm_menu, NULL }, { NULL, NULL, NULL } };
    if (mm.win) return mm.win;
    memset(&mm.desc, 0, sizeof mm.desc);
    mm.desc.name = "MovementMacroForm"; mm.desc.class_name = "TMovementMacroForm"; mm.desc.caption = LJ_MM_CAPTION;
    mm.desc.client_width = MM_CLIENT_W; mm.desc.client_height = MM_CLIENT_H; mm.desc.text_height = 13;
    mm.desc.color = LJ_CLR(LJ_CLR_BTNFACE); mm.desc.font = LJ_FONT_SANS13; mm.desc.font_name = "MS Sans Serif"; mm.desc.font_height = -11;
    mm.desc.border = LJ_BS_SINGLE; mm.desc.icons = LJ_BI_GIVEN | LJ_BI_SYSMENU; mm.desc.style = LJ_FS_STAYONTOP; mm.desc.position = LJ_PO_DESIGNED;
    mm.desc.menus = mm_menus; mm.desc.nmenus = 1; mm.desc.menu = "MainMenu1";
    if ((mm.win = lj_window_create(&mm.desc, b)) == NULL) { lj_log("movement: TMovementMacroForm failed to build"); return NULL; }
    root = lj_window_root(mm.win);
    mm.drag = -1;
    mm.trace = lj_w_alloc(sizeof(lj_widget), root, &d_mm_trace, &trace_vt, 0);
    mm.tab_macro = lj_w_speedbutton_create(root, &d_mm_tabm); mm.tab_macro->on_event = mm_widget;
    mm.tab_fix = lj_w_speedbutton_create(root, &d_mm_tabf); mm.tab_fix->on_event = mm_widget;
    lj_w_button_create(root, &d_mm_clear)->on_event = mm_widget;
    lj_w_label_create(root, &d_mm_type_l);
    mm.type = lj_w_combobox_create(root, &d_mm_type); mm.type->on_event = mm_widget;
    for (i = 0; i < 6; i++) {
        osfs[i] = (lj_ctl_desc){ LJ_K_UNKNOWN, "MmOsf", 137, mm_row_y[osf_row[i]], 48, 22, .tag = 0 };
        mm.osf[i] = lj_w_osf_create(root, &osfs[i]); mm.osf[i]->on_event = mm_widget;
    }
    for (i = 0; i < MB_COUNT; i++) {
        int cyc = i == MB_PAN_CYC || i == MB_TILT_CYC;
        bars[i] = (lj_ctl_desc){ LJ_K_UNKNOWN, names[i], cyc ? 238 : 190, mm_row_y[bar_row[i]], cyc ? 202 : 250, LJ_DRAGBAR_H, .max = bar_max[i] };
        mm.bar[i] = lj_w_dragbar_create(root, &bars[i]); mm.bar[i]->on_event = mm_widget;
        ((lj_w_dragbar *)mm.bar[i])->max = bar_max[i];
        lj_w_dragbar_set_fill(mm.bar[i], cyc ? LJ_CLR_BTNSHADOW : 0xFFFFFFu);   /* white amplitude, grey cycle (movementmacro.gif) */
        if (!mm.bar_paint0) { mm.barvt = *mm.bar[i]->vt; mm.bar_paint0 = mm.bar[i]->vt->paint; mm.barvt.paint = mm_bar_paint; }
        mm.bar[i]->vt = &mm.barvt;
    }
    for (i = 0; i < 4; i++) {                                /* Macro Direction CCW / CW (exe 0x26AF14 / 0x26AF5C); one dir byte for both rows (PROPOSAL rig 17 s8) */
        dirs[i] = (lj_ctl_desc){ LJ_K_SPEEDBUTTON, dir_names[i], 190 + (i & 1) * 23, mm_row_y[i < 2 ? 2 : 4], 22, 22, .group_index = 4 + i / 2,
                                 .caption = i & 1 ? "CW" : "CCW", .hint = i & 1 ? "Macro Direction CW" : "Macro Direction CCW", .show_hint = LJ_YES };   /* RotationCW/CCW.Bmp glyphs: art request (D2) */
        mm.dir[i] = lj_w_speedbutton_create(root, &dirs[i]); mm.dir[i]->on_event = mm_widget;
    }
    mm.canvas = lj_w_alloc(sizeof(lj_widget), root, &d_mm_canvas, &canvas_vt, 0);
    mm.canvas->visible = 0;
    lj_widget_layout_hooks(root);
    mm_fill();
    lj_win_show(mm.win, LJ_WIN_STAY_ON_TOP);
    return mm.win;
}
void lj_movemacro_editor_close(void)
{
    if (mm.name_win) { lj_win_hide(mm.name_win); lj_window_destroy(mm.name_win); }
    if (mm.win) { lj_win_hide(mm.win); lj_window_destroy(mm.win); }
    free(mm.edit_t);
    memset(&mm, 0, sizeof mm);
}
lj_window *lj_movemacro_editor_get(void) { return mm.win; }
void lj_movemacro_editor_refresh(void)
{
    if (!mm.win) return;
    if (lj_sel_serial() != mm.serial) mm_fill();
    else lj_widget_invalidate(mm.trace);                     /* the dot follows the engine's index (ctx->view) */
}

/* ---------------------------------------------------------------------------------------------------------------
 * Delete Macros (TDeleteMacrosForm): the .bez shapes of the library with their file time (LORD_FIDELITY-12)
 * ------------------------------------------------------------------------------------------------------------- */
static struct { lj_window *win; char names[LJ_BEZ_SHAPES_MAX][32]; char rows[LJ_BEZ_SHAPES_MAX][64]; const char *items[LJ_BEZ_SHAPES_MAX]; int n; } dm;

static void dm_fill(void)
{
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP], when[24];
    lj_widget *lb = dm.win ? lj_window_find(dm.win, "LB1") : NULL;
    int i;
    dm.n = bez_dir(dir, sizeof dir) ? 0 : lj_bez_list(dir, dm.names, LJ_BEZ_SHAPES_MAX);
    if (dm.n < 0) dm.n = 0;
    for (i = 0; i < dm.n; i++) {
        when[0] = 0;
        if (lj_bez_path(dir, dm.names[i], path, sizeof path) == 0) lj_fs_mtime_text(path, when, sizeof when);
        snprintf(dm.rows[i], sizeof dm.rows[i], "%-24s %s", dm.names[i], when);   /* name + Modified (the Delete Sequences column, PROPOSAL format) */
        dm.items[i] = dm.rows[i];
    }
    if (lb) { lj_w_list_set_items(lb, dm.items, dm.n); lj_widget_invalidate(lb); }
}
static void dm_check_all(int on) { lj_widget *lb = lj_window_find(dm.win, "LB1"); int i; for (i = 0; lb && i < dm.n; i++) lj_w_list_set_checked(lb, i, on); if (lb) lj_widget_invalidate(lb); }
static void dm_all(lj_widget *w, void *ud) { (void)w; (void)ud; dm_check_all(1); }
static void dm_none(lj_widget *w, void *ud) { (void)w; (void)ud; dm_check_all(0); }
static void dm_ok(lj_widget *w, void *ud)
{
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
    lj_widget *lb = lj_window_find(dm.win, "LB1");
    int i, gone = 0;
    (void)w; (void)ud;
    if (lb && bez_dir(dir, sizeof dir) == 0)
        for (i = 0; i < dm.n; i++) if (lj_w_list_checked(lb, i) && lj_bez_path(dir, dm.names[i], path, sizeof path) == 0 && remove(path) == 0) gone++;
    if (gone) lj_desktop_run_action(LJ_ACT_BEZ_RELOAD, 0);   /* the app swaps show->bez under the lock; a macro on a deleted shape shows "load error" (lj_movemacro_pos -1) */
    lj_delete_movemacro_close();
    if (mm.win) mm_fill();
}
static void dm_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_delete_movemacro_close(); }
static const lj_binding lj_dm_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "bOkClick", dm_ok, NULL }, { "bAllClick", dm_all, NULL }, { "bNoneClick", dm_none, NULL }, { "LB1Click", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_delete_movemacro_open(void)
{
    lj_widget *c;
    if (dm.win) return dm.win;
    if ((dm.win = lj_window_create(&lj_form_TDELETEMACROSFORM, lj_dm_bindings)) == NULL) { lj_log("movement: TDeleteMacrosForm failed to build"); return NULL; }
    if ((c = lj_window_find(dm.win, "bCancel")) != NULL) c->on_event = dm_cancel;   /* Kind = bkCancel, no OnClick in the DFM */
    dm_fill();
    lj_win_show(dm.win, LJ_WIN_STAY_ON_TOP);
    return dm.win;
}
void lj_delete_movemacro_close(void) { if (dm.win) { lj_win_hide(dm.win); lj_window_destroy(dm.win); dm.win = NULL; } }
lj_window *lj_delete_movemacro_get(void) { return dm.win; }
void lj_delete_movemacro_refresh(void) { }

/* ---------------------------------------------------------------------------------------------------------------
 * Fan Out (TFanOutForm) [04 s4.2, 17 s5]
 * ------------------------------------------------------------------------------------------------------------- */
static struct {
    lj_window *win; lj_widget *dpan, *dtilt;
    int cust[LJ_FIXTURES], ncust, drag_row; uint32_t serial;
    char rows[LJ_FIXTURES][48]; const char *items[LJ_FIXTURES];
    lj_widget_vt vt; void (*paint0)(lj_widget *w, lj_surface *s);
} fo;

static int fo_order(int *order)                              /* the "Apply Values by" order of the selection */
{
    static const int mode[3] = { LJ_DO_FIXTURE, LJ_DO_ICON_H, LJ_DO_ICON_V };
    int sel[LJ_FIXTURES], n = 0, i, rg = lj_w_radiogroup_index(lj_window_find(fo.win, "rg1"));
    for (i = 0; i < LJ_FIXTURES; i++) if (lj_sel_is_selected(i) && pt_profile(i)) sel[n++] = i;
    if (rg == 3) { for (i = 0; i < fo.ncust; i++) order[i] = fo.cust[i]; return fo.ncust; }   /* Custom: the list's order */
    if (lj_delay_order(lj_ctx_get()->show, sel, n, mode[rg < 0 ? 0 : rg], order)) return 0;
    return n;
}

static void fo_list(void)
{
    const lj_show *show = lj_ctx_get()->show;
    lj_widget *lv = lj_window_find(fo.win, "Lv1");
    int i, j, keep = 0;
    for (i = 0; i < fo.ncust; i++) if (lj_sel_is_selected(fo.cust[i]) && pt_profile(fo.cust[i])) fo.cust[keep++] = fo.cust[i];   /* deselect removes [04 s4.2] */
    fo.ncust = keep;
    for (i = 0; i < LJ_FIXTURES; i++) {                      /* new selections append ("click icons to add") */
        if (!lj_sel_is_selected(i) || !pt_profile(i)) continue;
        for (j = 0; j < fo.ncust && fo.cust[j] != i; j++) { }
        if (j == fo.ncust) fo.cust[fo.ncust++] = i;
    }
    for (i = 0; i < fo.ncust; i++) {
        snprintf(fo.rows[i], sizeof fo.rows[i], "%d\t%s", fo.cust[i] + 1, show ? show->fixture[fo.cust[i]].user_name : "");
        fo.items[i] = fo.rows[i];
    }
    if (lv) { lj_w_listview_set_items(lv, fo.items, fo.ncust); lj_widget_invalidate(lv); }
    fo.serial = lj_sel_serial();
}

static void fo_paint_grid(lj_widget *w, void *ud)            /* pb1Paint: the preview, one red dot per fixture */
{
    lj_window *win = lj_window_of(w);
    lj_surface *s = win ? lj_window_surface(win) : NULL;
    lj_scene *sc = cur_scene();
    int ax, ay, i, order[LJ_FIXTURES], n;
    (void)ud;
    if (!s || !fo.win) return;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, 0x0000FFu);
    lj_hline(s, ax, ay + w->h / 2, w->w, 0xFF0000u); lj_vline(s, ax + w->w / 2, ay, w->h, 0xFF0000u);   /* red cross: fanoutdialog.gif (reduced) blends it with the blue to #AD004A */
    n = fo_order(order);
    for (i = 0; sc && i < n; i++) { int32_t p, t; if (pt_read(sc, order[i], &p, &t) == 0) dot(s, ax + px_of(p, w->w), ay + px_of(t, w->h), 2, 0xFF0000u); }
}

int lj_fan_out_apply(void)                                   /* the Delta bars / Center Delta on Home onto the current scene */
{
    int order[LJ_FIXTURES], n = fo.win ? fo_order(order) : 0, i, rc;
    int32_t pan[LJ_FIXTURES], tilt[LJ_FIXTURES];
    lj_scene *sc;
    if (n < 2) return -1;                                    /* "all selected fixtures (2 or more)" */
    lj_lock();
    sc = cur_scene();
    for (i = 0; sc && i < n; i++) if (pt_read(sc, order[i], &pan[order[i]], &tilt[order[i]])) { pan[order[i]] = tilt[order[i]] = 0; }
    rc = sc ? lj_fan_out(pan, tilt, order, n, lj_w_dragbar_value(fo.dpan), lj_w_dragbar_value(fo.dtilt), lj_w_checkbox_checked(lj_window_find(fo.win, "cbCenter"))) : -1;
    for (i = 0; rc == 0 && i < n; i++) pt_write(sc, order[i], pan[order[i]], tilt[order[i]]);
    if (rc == 0) scene_modified();
    lj_unlock();
    if (rc == 0) lj_ctx_set_scene(lj_ctx_get()->current_scene);   /* CURRENT_SEQ (seam 5) */
    lj_window_invalidate_all(fo.win);
    return rc;
}

static void fo_spread(int axis)
{
    int order[LJ_FIXTURES], n = fo_order(order), i;
    int32_t pan[LJ_FIXTURES], tilt[LJ_FIXTURES];
    lj_scene *sc;
    if (n < 2) return;
    lj_lock();
    sc = cur_scene();
    for (i = 0; sc && i < n; i++) if (pt_read(sc, order[i], &pan[order[i]], &tilt[order[i]])) pan[order[i]] = tilt[order[i]] = 0;
    if (sc && lj_fan_spread(axis ? tilt : pan, order, n) == 0) { for (i = 0; i < n; i++) pt_write(sc, order[i], pan[order[i]], tilt[order[i]]); scene_modified(); }
    lj_unlock();
    lj_ctx_set_scene(lj_ctx_get()->current_scene);
    lj_window_invalidate_all(fo.win);
}

static void fo_bar_paint(lj_widget *w, lj_surface *s)
{
    char text[32];
    int ax, ay, ix = LJ_DRAGBAR_ARROW + 3, iw = w->w - 2 * LJ_DRAGBAR_ARROW - 4, fw;
    const lj_w_dragbar *b = (const lj_w_dragbar *)w;
    fo.paint0(w, s);
    lj_widget_abs(w, &ax, &ay);
    fw = (iw * b->value + b->max / 2) / b->max;
    lj_fill(s, ax + ix, ay + 2, iw, w->h - 4, LJ_CLR_BTNSHADOW);
    lj_fill(s, ax + ix, ay + 2, fw, w->h - 4, b->fill);
    snprintf(text, sizeof text, "%s %d", w == fo.dpan ? "Delta Pan" : "Delta Tilt", b->value);   /* 04 s4.2 names; the bar value x 256 = readout units (17 s5, ASSUMED) */
    lj_w_text_c(s, w, ax + ix, ay, iw, w->h, text, LJ_CLR_3DDKSHADOW, 0);
}
static void fo_bar(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_CHANGE) { lj_fan_out_apply(); lj_widget_invalidate(w); } }
static void fo_pan(lj_widget *w, void *ud) { (void)w; (void)ud; fo_spread(0); }
static void fo_tilt(lj_widget *w, void *ud) { (void)w; (void)ud; fo_spread(1); }
static void fo_rg(lj_widget *w, void *ud) { (void)w; (void)ud; lj_window_invalidate_all(fo.win); }
static void fo_clear(lj_widget *w, void *ud) { (void)w; (void)ud; fo.ncust = 0; fo_list(); }   /* Clear Custom Selection: rebuilt from the selection */
static void fo_start_drag(lj_widget *w, void *ud) { (void)ud; fo.drag_row = lj_w_listview_index(w); }
static void fo_drop(lj_widget *w, void *ud)                  /* drag lines to reorder [04 s4.2] */
{
    lj_window *win = lj_window_of(w);
    const lj_mouse_ev *m = win ? lj_window_last_event(win) : NULL;
    int ax, ay, to, f;
    (void)ud;
    if (!m || fo.drag_row < 0 || fo.drag_row >= fo.ncust) return;
    lj_widget_abs(w, &ax, &ay);
    to = lj_w_listview_row_at(w, m->y - ay - lj_window_root(win)->y);
    if (to < 0) to = fo.ncust - 1;
    f = fo.cust[fo.drag_row];
    if (to > fo.drag_row) memmove(&fo.cust[fo.drag_row], &fo.cust[fo.drag_row + 1], sizeof fo.cust[0] * (size_t)(to - fo.drag_row));
    else memmove(&fo.cust[to + 1], &fo.cust[to], sizeof fo.cust[0] * (size_t)(fo.drag_row - to));
    fo.cust[to] = f;
    fo.drag_row = -1;
    fo_list();
}
int lj_fan_out_custom_move(int from, int to)                 /* the drag-and-drop path the tests and the UI script drive */
{
    if (!fo.win || from < 0 || from >= fo.ncust || to < 0 || to >= fo.ncust) return -1;
    fo.drag_row = from;
    {
        int f = fo.cust[from];
        if (to > from) memmove(&fo.cust[from], &fo.cust[from + 1], sizeof fo.cust[0] * (size_t)(to - from));
        else memmove(&fo.cust[to + 1], &fo.cust[to], sizeof fo.cust[0] * (size_t)(from - to));
        fo.cust[to] = f;
    }
    fo.drag_row = -1;
    fo_list();
    return 0;
}
static const lj_binding lj_fo_bindings[] = {
    { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "Lv1DragDrop", fo_drop, NULL }, { "Lv1DragOver", lj_handler_noop, NULL }, { "Lv1StartDrag", fo_start_drag, NULL },
    { "sbNewClick", fo_clear, NULL }, { "sbDeleteClick", fo_clear, NULL }, { "cb1Change", lj_handler_noop, NULL },   /* ponytail: saved custom selections (TSaveSelectionForm) are not kept; the list is per session */
    { "bPanClick", fo_pan, NULL }, { "bTiltClick", fo_tilt, NULL }, { "rg1Click", fo_rg, NULL }, { "pb1Paint", fo_paint_grid, NULL },
    { "DeleteAllCustomSettings1Click", fo_clear, NULL }, { "help1Click", help, NULL },
    { NULL, NULL, NULL } };
/* the two Delta bars beside the Spread buttons in Panel1 (code-built in LJ2: fanoutdialog.gif, PROPOSAL rig) */
static const lj_ctl_desc d_fo_dpan  = { LJ_K_UNKNOWN, "DeltaPan", 2, 4, 222, LJ_DRAGBAR_H, .max = 127 };
static const lj_ctl_desc d_fo_dtilt = { LJ_K_UNKNOWN, "DeltaTilt", 2, 32, 222, LJ_DRAGBAR_H, .max = 127 };

lj_window *lj_fan_out_open(void)
{
    lj_widget *p1;
    if (fo.win) return fo.win;
    if ((fo.win = lj_window_create(&lj_form_TFANOUTFORM, lj_fo_bindings)) == NULL) { lj_log("movement: TFanOutForm failed to build"); return NULL; }
    fo.drag_row = -1;
    if ((p1 = lj_window_find(fo.win, "Panel1")) != NULL) {
        fo.dpan = lj_w_dragbar_create(p1, &d_fo_dpan); fo.dtilt = lj_w_dragbar_create(p1, &d_fo_dtilt);
        ((lj_w_dragbar *)fo.dpan)->max = ((lj_w_dragbar *)fo.dtilt)->max = 127;   /* 127 x 256 = 32512, the end fixtures near the edges (PROPOSAL rig 17 s5) */
        fo.dpan->on_event = fo.dtilt->on_event = fo_bar;
        fo.vt = *fo.dpan->vt; fo.paint0 = fo.dpan->vt->paint; fo.vt.paint = fo_bar_paint; fo.dpan->vt = fo.dtilt->vt = &fo.vt;
    }
    fo_list();
    lj_win_show(fo.win, LJ_WIN_STAY_ON_TOP);
    return fo.win;
}
void lj_fan_out_close(void) { if (fo.win) { lj_win_hide(fo.win); lj_window_destroy(fo.win); fo.win = NULL; fo.dpan = fo.dtilt = NULL; } }
lj_window *lj_fan_out_get(void) { return fo.win; }
void lj_fan_out_refresh(void) { if (fo.win && lj_sel_serial() != fo.serial) { fo_list(); lj_window_invalidate_all(fo.win); } }

/* ---------------------------------------------------------------------------------------------------------------
 * Pan/Tilt Macro Auto Delay (TAutoDelayForm) [04 s4.6, 17 s6]
 * ------------------------------------------------------------------------------------------------------------- */
static struct { lj_window *win; lj_widget *vmax, *vcount, *vseg; int max_delay, count, segments; uint32_t seed, serial; char t[3][12]; } ad;   /* t: any int (gcc -Wformat-truncation) */

static void ad_show(void)
{
    char n[16];
    int f[LJ_FIXTURES], st = lj_w_radiogroup_index(lj_window_find(ad.win, "RGStyle"));
    lj_widget *w;
    snprintf(ad.t[0], sizeof ad.t[0], "%d", ad.max_delay); snprintf(ad.t[1], sizeof ad.t[1], "%d", ad.count); snprintf(ad.t[2], sizeof ad.t[2], "%d", ad.segments);
    if (ad.vmax) lj_w_edit_set_text(ad.vmax, ad.t[0]);
    if (ad.vcount) { lj_w_edit_set_text(ad.vcount, ad.t[1]); ad.vcount->enabled = st == LJ_MMD_CIRCULAR; }
    if (ad.vseg) { lj_w_edit_set_text(ad.vseg, ad.t[2]); ad.vseg->enabled = st == LJ_MMD_SEGMENTS; }
    snprintf(n, sizeof n, "%d ", sel_fixtures(f));           /* "N Fixtures Selected" (LFixMax, DFM '100 ') */
    if ((w = lj_window_find(ad.win, "LFixMax")) != NULL) lj_w_set_caption(w, n);
    if ((w = lj_window_find(ad.win, "UdIncrement")) != NULL) w->enabled = st == LJ_MMD_CIRCULAR;   /* CheckControls */
    if ((w = lj_window_find(ad.win, "UdSegments")) != NULL) w->enabled = st == LJ_MMD_SEGMENTS;
    ad.serial = lj_sel_serial();
    lj_window_invalidate_all(ad.win);
}
static void ad_check(lj_widget *w, void *ud) { (void)w; (void)ud; ad_show(); }
static int ad_step(int v, lj_widget *w, int lo, int hi) { v += lj_w_udbutton_delta(w); return v < lo ? lo : v > hi ? hi : v; }
static void ad_max(lj_widget *w, void *ud) { (void)ud; ad.max_delay = ad_step(ad.max_delay, w, 1, 256); ad_show(); }   /* 1-256, 256 = 100 % [04 s4.6] */
static void ad_inc(lj_widget *w, void *ud) { (void)ud; ad.count = ad_step(ad.count, w, 1, LJ_FIXTURES); ad_show(); }
static void ad_seg(lj_widget *w, void *ud) { (void)ud; ad.segments = ad_step(ad.segments, w, 1, LJ_FIXTURES); ad_show(); }

int lj_auto_delay_apply(void)                                /* Apply: delays into the current scene's macros of the selection, in delay order */
{
    static const int order_of[6] = { LJ_DO_FIXTURE, LJ_DO_ICON_H, LJ_DO_ICON_V, LJ_DO_FIXTURE_REV, LJ_DO_ICON_H_REV, LJ_DO_ICON_V_REV };   /* RGOrder items -> enum lj_delay_order */
    int sel[LJ_FIXTURES], order[LJ_FIXTURES], n, i, rc, ro, st;
    uint8_t d[LJ_FIXTURES];
    lj_scene *sc;
    if (!ad.win) return -1;
    n = sel_fixtures(sel);
    ro = lj_w_radiogroup_index(lj_window_find(ad.win, "RGOrder")); st = lj_w_radiogroup_index(lj_window_find(ad.win, "RGStyle"));
    if (n < 1 || lj_delay_order(lj_ctx_get()->show, sel, n, order_of[ro < 0 ? 0 : ro], order)) return -1;
    if (lj_mm_auto_delay(n, st < 0 ? 0 : st, ad.max_delay, ad.count, ad.segments, &ad.seed, d)) return -1;
    lj_lock();
    sc = cur_scene();
    for (i = 0; sc && i < n; i++) {
        lj_movemacro *m = &sc->macro[order[i]];
        m->delay = d[i];
        if (m->fstate[LJ_MMP_DELAY] == LJ_OFF) m->fstate[LJ_MMP_DELAY] = LJ_SNAP;   /* "delays need not live in the same sequence as the other macro parameters" [04 s4.6] */
    }
    rc = sc ? 0 : -1;
    if (sc) scene_modified();
    lj_unlock();
    if (rc == 0) lj_ctx_set_scene(lj_ctx_get()->current_scene);
    if (mm.win) mm_fill();
    return rc;
}
static void ad_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_auto_delay_apply(); }
static const lj_binding lj_ad_bindings[] = {
    { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", ad_check, NULL },
    { "CheckControls", ad_check, NULL }, { "UdSegmentsChange", ad_seg, NULL }, { "UdIncrementChange", ad_inc, NULL }, { "udMaxDelayChange", ad_max, NULL },
    { "bOkClick", ad_ok, NULL }, { "Help1Click", help, NULL }, { NULL, NULL, NULL } };
/* the value boxes between each label and its TUdButton (code-built in LJ2: autodelay.gif, PROPOSAL rig) */
/* read-only edits left of each TUdButton (autodelay.gif: white boxes, greyed when the style does not use them); x / width PROPOSAL rig, the GIF is reduced */
static const lj_ctl_desc d_ad_max = { LJ_K_EDIT, "AdMax", 58, 4, 40, 21, .max_length = 4, .font_inherited = 1 };
static const lj_ctl_desc d_ad_cnt = { LJ_K_EDIT, "AdCount", 224, 4, 34, 21, .max_length = 4, .font_inherited = 1 };
static const lj_ctl_desc d_ad_seg = { LJ_K_EDIT, "AdSegments", 329, 4, 34, 21, .max_length = 4, .font_inherited = 1 };

lj_window *lj_auto_delay_open(void)
{
    lj_widget *p2;
    if (ad.win) return ad.win;
    if ((ad.win = lj_window_create(&lj_form_TAUTODELAYFORM, lj_ad_bindings)) == NULL) { lj_log("movement: TAutoDelayForm failed to build"); return NULL; }
    if (!ad.max_delay) { ad.max_delay = 256; ad.count = 3; ad.segments = 3; ad.seed = 0x9E3779B9u; }   /* PROPOSAL defaults (the CHM examples) */
    if ((p2 = lj_window_find(ad.win, "Panel2")) != NULL) {
        ad.vmax = lj_w_edit_create(p2, &d_ad_max); ad.vcount = lj_w_edit_create(p2, &d_ad_cnt); ad.vseg = lj_w_edit_create(p2, &d_ad_seg);
        ((lj_w_edit *)ad.vmax)->readonly = ((lj_w_edit *)ad.vcount)->readonly = ((lj_w_edit *)ad.vseg)->readonly = 1;   /* the arrows set them */
    }
    ad_show();
    lj_win_show(ad.win, LJ_WIN_STAY_ON_TOP);
    return ad.win;
}
void lj_auto_delay_close(void) { if (ad.win) { lj_win_hide(ad.win); lj_window_destroy(ad.win); ad.win = NULL; ad.vmax = ad.vcount = ad.vseg = NULL; } }
lj_window *lj_auto_delay_get(void) { return ad.win; }
void lj_auto_delay_refresh(void) { if (ad.win && lj_sel_serial() != ad.serial) ad_show(); }
int lj_auto_delay_set(int max_delay, int count, int segments) { ad.max_delay = max_delay; ad.count = count; ad.segments = segments; if (ad.win) ad_show(); return 0; }

/* ---------------------------------------------------------------------------------------------------------------
 * Cue Macro Control (TCueMacroForm) [03 s5.11]: amplitude / speed of the cue's movement macros
 * ------------------------------------------------------------------------------------------------------------- */
static struct { lj_window *win; lj_widget *amp, *speed; int range999, link /* 0 none, 1 amplitude, 2 cue time */, fade_ms; int amp_pct, speed_pct, frozen; } cm;
static const struct { const char *text; int ms; } cm_times[] = { { "0.5 sec", 500 }, { "1 sec", 1000 }, { "2 sec", 2000 }, { "3 sec", 3000 }, { "5 sec", 5000 }, { "10 sec", 10000 } };   /* PROPOSAL rig 03 s5.11: the list is filled at runtime in LJ2 */
static char cm_time_text[LJ_COUNT_OF(cm_times)][12]; static const char *cm_time_items[LJ_COUNT_OF(cm_times)];

static void cm_post(int kind, int a, int b)
{
    lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = (uint8_t)kind; c.a = (uint32_t)a; c.b = (uint32_t)b;
    lj_ctx_post(&c);
}
static void cm_values(void)                                  /* MACRO_CTL with the two scales; saved with the cue when it keeps them [03 s5.11] */
{
    lj_ctx *c = lj_ctx_get();
    cm_post(LJ_CMD_MACRO_CTL, cm.amp_pct, cm.speed_pct);
    lj_lock();
    if (c->show && c->show->current_cue && c->show->current_cue->save_macro_values) {
        c->show->current_cue->macro_amp_pct = (uint16_t)cm.amp_pct; c->show->current_cue->macro_speed_pct = (uint16_t)cm.speed_pct;
        lj_ctx_set_modified(LJ_MOD_CUE, 1);
    }
    lj_unlock();
}
int lj_cue_macro_control_set(int amp_pct, int speed_pct)
{
    int hi = cm.range999 ? LJ_MOVEMACRO_PCT_MAX : 100, lo = cm.range999 ? 1 : 0;
    cm.amp_pct = amp_pct < lo ? lo : amp_pct > hi ? hi : amp_pct;
    cm.speed_pct = speed_pct < lo ? lo : speed_pct > hi ? hi : speed_pct;
    if (cm.link == 1) cm.speed_pct = cm.amp_pct;              /* Speed Fader Link: To Amplitude */
    if (cm.amp) { lj_w_fader_set_value(cm.amp, cm.amp_pct); lj_w_fader_set_value(cm.speed, cm.speed_pct); lj_widget_invalidate(cm.amp); lj_widget_invalidate(cm.speed); }
    cm_values();
    return 0;
}
static void cm_text(int v, char *buf, size_t cap) { snprintf(buf, cap, "%d%%", v); }
static void cm_fader(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev != LJ_WEV_CHANGE) return;
    if (w == cm.amp) lj_cue_macro_control_set(lj_w_fader_value(w), cm.link == 1 ? lj_w_fader_value(w) : cm.speed_pct);
    else if (cm.link != 1) lj_cue_macro_control_set(cm.amp_pct, lj_w_fader_value(w));
}
static void cm_fade(lj_widget *w, void *ud)                  /* MacroFadeClick: Fade Out / Fade In over the time, Macros Off / On */
{
    const char *n = w && w->name ? w->name : "";
    (void)ud;
    if (!strcmp(n, "bFadeOut")) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_FADE_OUT, cm.fade_ms);
    else if (!strcmp(n, "bFadeIn")) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_FADE_IN, cm.fade_ms);
    else if (!strcmp(n, "bMacroOff")) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_OFF, 0);
    else if (!strcmp(n, "bMacroOn")) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_ON, 0);
}
static void cm_freeze_set(int on) { cm.frozen = on; cm_post(LJ_CMD_MACRO_FADE, on ? LJ_MM_FREEZE : LJ_MM_RUN, 0); if (cm.win) lj_w_speedbutton_set_down(lj_window_find(cm.win, "sbFreezeMacro"), on); }
static void cm_freeze(lj_widget *w, void *ud) { (void)ud; cm_freeze_set(lj_w_speedbutton_down(w)); }   /* Freeze = speed 0 [03 s5.11] */
static void cm_time(lj_widget *w, void *ud) { int i = ((lj_w_combobox *)w)->index; (void)ud; if (i >= 0 && i < (int)LJ_COUNT_OF(cm_times)) cm.fade_ms = cm_times[i].ms; }
static void cm_menu(lj_widget *w, void *ud)
{
    lj_window *win = lj_window_of(w);
    const lj_menu_item_desc *it = lj_window_menu_item(win);
    const char *n = it && it->name ? it->name : "";
    struct lj_menu *mn = lj_window_menus(win);
    (void)ud;
    if (!strcmp(n, "M_Pct_1") || !strcmp(n, "M_Pct_2")) {   /* Fader Range 0-100% / 1-999% */
        cm.range999 = n[6] == '2';
        lj_menu_set_checked(mn, "M_Pct_1", !cm.range999); lj_menu_set_checked(mn, "M_Pct_2", cm.range999);
        ((lj_w_fader *)cm.amp)->max = ((lj_w_fader *)cm.speed)->max = cm.range999 ? LJ_MOVEMACRO_PCT_MAX : 100;
        lj_cue_macro_control_set(cm.amp_pct, cm.speed_pct);
    } else if (!strcmp(n, "M_LinkNone") || !strcmp(n, "M_LinkAmp") || !strcmp(n, "M_LinkCueTime")) {
        cm.link = n[6] == 'N' ? 0 : n[6] == 'A' ? 1 : 2;
        lj_menu_set_checked(mn, "M_LinkNone", cm.link == 0); lj_menu_set_checked(mn, "M_LinkAmp", cm.link == 1); lj_menu_set_checked(mn, "M_LinkCueTime", cm.link == 2);
        if (cm.link == 2 && lj_ctx_get()->show && lj_ctx_get()->show->current_cue) lj_cue_macro_control_set(cm.amp_pct, lj_ctx_get()->show->current_cue->cue_time_pct);   /* To CueTime: the cue time fader's % (PROPOSAL: followed when the window opens or the link is chosen) */
        else lj_cue_macro_control_set(cm.amp_pct, cm.speed_pct);
    } else if (!strcmp(n, "M_FaderPreset") || !strcmp(n, "M_ShowPct")) {
        lj_menu_set_checked(mn, n, it->checked != LJ_YES);   /* ponytail: Allow Fader Preset / Show Values as % keep their check; the faders always read % */
    }
}
static const lj_binding lj_cm_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "MacroFadeClick", cm_fade, NULL }, { "sbFreezeMacroMouseUp", cm_freeze, NULL }, { "cbFadeTimeChange", cm_time, NULL },
    { "Range1Click", lj_handler_noop, NULL }, { "M_PctClick", cm_menu, NULL }, { "M_LinkSpeedFaderClick", lj_handler_noop, NULL }, { "M_LinkCueClick", cm_menu, NULL },
    { "OptionsClick", cm_menu, NULL }, { "Help1Click", help, NULL }, { NULL, NULL, NULL } };
/* the two faders inside Panel2 (Ampl.) / Panel3 (Speed), code-built in LJ2 (cuemacrocontrol.gif, PROPOSAL rig) */
static const lj_ctl_desc d_cm_amp   = { LJ_K_UNKNOWN, "FaderAmp", 2, 18, LJ_FADER_W, 166, .max = 100, .position = 100 };
static const lj_ctl_desc d_cm_speed = { LJ_K_UNKNOWN, "FaderSpeed", 2, 18, LJ_FADER_W, 166, .max = 100, .position = 100 };

lj_window *lj_cue_macro_control_open(void)
{
    const lj_ctx *c = lj_ctx_get();
    lj_widget *p, *cb;
    size_t i;
    if (cm.win) return cm.win;
    if ((cm.win = lj_window_create(&lj_form_TCUEMACROFORM, lj_cm_bindings)) == NULL) { lj_log("movement: TCueMacroForm failed to build"); return NULL; }
    if (!cm.fade_ms) { cm.fade_ms = 1000; cm.amp_pct = cm.speed_pct = 100; }
    if (c->show && c->show->current_cue && c->show->current_cue->save_macro_values) { cm.amp_pct = c->show->current_cue->macro_amp_pct; cm.speed_pct = c->show->current_cue->macro_speed_pct; }
    if ((p = lj_window_find(cm.win, "Panel2")) != NULL) { cm.amp = lj_w_fader_create(p, &d_cm_amp); cm.amp->on_event = cm_fader; ((lj_w_fader *)cm.amp)->text = cm_text; }
    if ((p = lj_window_find(cm.win, "Panel3")) != NULL) { cm.speed = lj_w_fader_create(p, &d_cm_speed); cm.speed->on_event = cm_fader; ((lj_w_fader *)cm.speed)->text = cm_text; }
    if (!cm.amp || !cm.speed) { lj_cue_macro_control_close(); return NULL; }
    ((lj_w_fader *)cm.amp)->max = ((lj_w_fader *)cm.speed)->max = cm.range999 ? LJ_MOVEMACRO_PCT_MAX : 100;
    lj_w_fader_set_value(cm.amp, cm.amp_pct); lj_w_fader_set_value(cm.speed, cm.speed_pct);
    for (i = 0; i < LJ_COUNT_OF(cm_times); i++) {             /* host decimal separator in displayed text (D6) */
        lj_str_copy(cm_time_text[i], sizeof cm_time_text[i], cm_times[i].text);
        if (cm_time_text[i][1] == '.') cm_time_text[i][1] = lj_locale_decimal();
        cm_time_items[i] = cm_time_text[i];
        if (cm_times[i].ms == cm.fade_ms) { if ((cb = lj_window_find(cm.win, "cbFadeTime")) != NULL) ((lj_w_combobox *)cb)->index = (int)i; }
    }
    if ((cb = lj_window_find(cm.win, "cbFadeTime")) != NULL) { int keep = ((lj_w_combobox *)cb)->index; lj_w_combobox_set_items(cb, cm_time_items, (int)LJ_COUNT_OF(cm_times)); ((lj_w_combobox *)cb)->index = keep; }
    lj_menu_set_checked(lj_window_menus(cm.win), "M_Pct_1", !cm.range999); lj_menu_set_checked(lj_window_menus(cm.win), "M_Pct_2", cm.range999);
    lj_menu_set_checked(lj_window_menus(cm.win), "M_LinkNone", cm.link == 0);
    lj_w_speedbutton_set_down(lj_window_find(cm.win, "sbFreezeMacro"), cm.frozen);
    lj_win_show(cm.win, LJ_WIN_STAY_ON_TOP);
    return cm.win;
}
void lj_cue_macro_control_close(void) { if (cm.win) { lj_win_hide(cm.win); lj_window_destroy(cm.win); cm.win = NULL; cm.amp = cm.speed = NULL; } }
lj_window *lj_cue_macro_control_get(void) { return cm.win; }
void lj_cue_macro_control_refresh(void) { }

/* ---- the hotkeys of 01 s7 that drive the Cue macro control (Pan/Tilt Macro: ...) ---- */
static void fn_ptm(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    switch (fn) {
    case LJ_FN_PT_MACRO_FADE_IN:  if (edge) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_FADE_IN, cm.fade_ms ? cm.fade_ms : 1000); break;
    case LJ_FN_PT_MACRO_FADEOUT:  if (edge) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_FADE_OUT, cm.fade_ms ? cm.fade_ms : 1000); break;
    case LJ_FN_PT_MACRO_OFF:      if (edge) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_OFF, 0); break;
    case LJ_FN_PT_MACRO_ON:       if (edge) cm_post(LJ_CMD_MACRO_FADE, LJ_MM_ON, 0); break;
    case LJ_FN_PT_MACRO_RESET_AMPLITUDE: if (edge) lj_cue_macro_control_set(100, cm.amp ? cm.speed_pct : 100); break;
    case LJ_FN_PT_MACRO_RESET_SPEED:     if (edge) lj_cue_macro_control_set(cm.amp ? cm.amp_pct : 100, 100); break;
    case LJ_FN_PT_MACRO_FREEZE:   if (edge) cm_freeze_set(1); break;
    case LJ_FN_PT_MACRO_RUN:      if (edge) cm_freeze_set(0); break;
    case LJ_FN_PT_MACRO_TOGGLE_FREEZE: if (edge) cm_freeze_set(!cm.frozen); break;
    case LJ_FN_PT_MACRO_FREEZE_FLASH:  cm_freeze_set(edge != 0); break;   /* *Freeze: held */
    default: break;
    }
}
void lj_movemacro_editor_bind_fns(void)
{
    static const enum lj_fn fns[] = { LJ_FN_PT_MACRO_FADE_IN, LJ_FN_PT_MACRO_FADEOUT, LJ_FN_PT_MACRO_OFF, LJ_FN_PT_MACRO_ON, LJ_FN_PT_MACRO_RESET_AMPLITUDE,
                                      LJ_FN_PT_MACRO_RESET_SPEED, LJ_FN_PT_MACRO_FREEZE, LJ_FN_PT_MACRO_RUN, LJ_FN_PT_MACRO_TOGGLE_FREEZE, LJ_FN_PT_MACRO_FREEZE_FLASH };
    size_t i;
    for (i = 0; i < LJ_COUNT_OF(fns); i++) lj_fn_bind(fns[i], fn_ptm, NULL);
}
