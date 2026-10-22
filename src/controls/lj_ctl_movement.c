/* lj_ctl_movement.c - the Movement control (lj_ctl_movement.h). Integer-only position
 * maths after tools/ref/17-position_ref.py; the window is code-built against the
 * widget API (design 7.7) with every number measured from movement2.gif /
 * joystickmovement.gif (rig item) or tagged PROPOSAL. */
#include "controls/lj_ctl_movement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

/* ---------------------------------------------------------------------------
 * 17 s1-s3: coordinate model, resolution, relative movement
 * ------------------------------------------------------------------------- */
int32_t  lj_pos_clamp(int32_t v) { return v < LJ_POS_LO ? LJ_POS_LO : v > LJ_POS_HI ? LJ_POS_HI : v; }
uint16_t lj_pos_to_dmx16(int32_t v) { return (uint16_t)(lj_pos_clamp(v) + LJ_POS_HALF); }
int32_t  lj_pos_from_dmx16(uint16_t d) { return (int32_t)d - LJ_POS_HALF; }
uint8_t  lj_pos_to_dmx8(int32_t v) { return (uint8_t)(lj_pos_to_dmx16(v) >> 8); }
int32_t  lj_pos_from_dmx8(uint8_t x) { return ((int32_t)x << 8) - LJ_POS_HALF; }
int32_t  lj_pos_step(int32_t v, int dir, int res) { return lj_pos_clamp(v + (dir < 0 ? -1 : 1) * res); }
int32_t  lj_pos_relative(int32_t v, int32_t delta) { return lj_pos_clamp(v + delta); }
int32_t  lj_pos_snap8(int32_t v) { int32_t i = (lj_pos_clamp(v) + LJ_POS_HALF + 128) >> 8; return ((i > 255 ? 255 : i) << 8) - LJ_POS_HALF; }

void lj_pos_patch16(const lj_profile *p, uint8_t ofs[4])
{
    if (p->pt_patch_on) { memcpy(ofs, p->pt_patch, 4); return; }
    ofs[0] = p->pan_ofs; ofs[1] = (uint8_t)(p->pan_ofs + 1); ofs[2] = p->tilt_ofs; ofs[3] = (uint8_t)(p->tilt_ofs + 1);
}

/* ---------------------------------------------------------------------------
 * 17 s5-s8: fan out, auto delay, macro index, cue macro scaling
 * ------------------------------------------------------------------------- */
int32_t lj_pos_fanout_offset(int n, int delta, int i)
{
    int32_t num, den = n - 1, mag;
    if (n < 2 || i < 0 || i >= n) return -1;
    num = (int32_t)delta * LJ_POS_DELTA_UNIT * (2 * i - den);   /* delta*256*(i - m)/m with m = den/2; round half away (no ties below n = 513) */
    mag = (2 * (num < 0 ? -num : num) + den) / (2 * den);
    return num < 0 ? -mag : mag;
}

int lj_pos_auto_delay(enum lj_pos_delay_style style, int n, int max_delay, int param, int k)
{
    int d;
    if (n < 1 || k < 0 || k >= n || max_delay < 1 || max_delay > LJ_POS_DELAY_FULL) return -1;
    switch (style) {
    case LJ_POS_DELAY_LINEAR:   d = max_delay * k / n; break;
    case LJ_POS_DELAY_CIRCULAR: if (param < 1) return -1; d = max_delay * (k % param) / param; break;
    case LJ_POS_DELAY_SEGMENTS: if (param < 1) return -1; d = max_delay * (k * param / n) / param; break;
    default: return -1;
    }
    return d % LJ_POS_DELAY_FULL;
}

int lj_pos_delay_index(int delay_byte) { return LJ_POS_N_TABLE * delay_byte / LJ_POS_DELAY_FULL; }

static int mod_table(int v) { return ((v % LJ_POS_N_TABLE) + LJ_POS_N_TABLE) % LJ_POS_N_TABLE; }

void lj_pos_effective_index(int idx, int delay_byte, int tweak, int *pan_i, int *tilt_i)
{
    int d = mod_table(idx - lj_pos_delay_index(delay_byte));
    *pan_i = d; *tilt_i = mod_table(d + tweak);
}

int lj_pos_quantise(int idx, int step) { return step < 1 ? -1 : mod_table(idx / step * step); }

int lj_pos_scale_raw(int raw, int pct)
{
    int v = raw * pct / 100;
    return v < 0 ? 0 : v > LJ_POS_RAW_MAX ? LJ_POS_RAW_MAX : v;
}

/* ---------------------------------------------------------------------------
 * the control
 * ------------------------------------------------------------------------- */
#define LJ_MV_NOT_AVAIL "Not available in LJ4EVR"
/* LORD_FIDELITY-9: the Movement window caption, one tagged constant: "Movement 8 Bit" / "Movement 16 Bit" by the master's
 * pan/tilt width (IMG joystickmovement / movement2; the exe also carries "Position 16 Bit", 17 s1.1, unverified) */
static const char *const lj_mv_caption[2] = { "Movement 8 Bit", "Movement 16 Bit" };
#define LJ_MV_SECTION   "MOVEMENT"

typedef struct lj_mv lj_mv;
typedef struct { lj_widget w; lj_mv *mv; int drag, lx, ly; } lj_mv_grid;     /* the field; also the window -> lj_mv link (lj_window_find "MvGrid") */
typedef struct { lj_widget w; lj_mv *mv; } lj_mv_readout;

struct lj_mv {
    lj_form_desc desc;
    const lj_profile *p; int bits16;
    int master, fx[LJ_FIXTURES], nfx;
    int absolute, res, lock_pan, lock_tilt;
    int32_t pan, tilt; int preset;                       /* the master's readout and preset reference as last read */
    lj_window *win, *presets;
    lj_mv_grid *grid;
    lj_widget *readout, *bres, *babs, *bpan_fade, *btilt_fade;
    lj_widget_vt resvt; void (*res_paint0)(lj_widget *w, lj_surface *s);
    char preset_text[LJ_PRESETS][20]; const char *preset_items[LJ_PRESETS]; int preset_no[LJ_PRESETS]; int npresets;
};

static lj_mv *mv_of(lj_window *win)
{
    lj_widget *g = lj_window_find(win, "MvGrid");
    return g ? ((lj_mv_grid *)g)->mv : NULL;
}

static lj_scene *scene_of(void)
{
    lj_ctx *c = lj_ctx_get();
    if (!c->show || !c->show->current_seq || !c->show->current_seq->scenes || c->current_scene >= c->show->current_seq->n) return NULL;
    return &c->show->current_seq->scenes[c->current_scene];
}

static int fixture_base(const lj_mv *mv, int f)        /* address code of the fixture's channel 1, -1 when unused or another profile */
{
    const lj_show *show = lj_ctx_get()->show;
    const lj_fixture *fx;
    if (!show || f < 0 || f >= LJ_FIXTURES) return -1;
    fx = &show->fixture[f];
    if (fx->link == 0 || fx->profile_id != mv->p->id) return -1;
    return lj_addr_code(fx->link, fx->address);
}

static int read_pos(const lj_mv *mv, const lj_scene *sc, int f, int32_t *pan, int32_t *tilt)
{
    int base = fixture_base(mv, f);
    uint8_t ofs[4];
    if (base < 0) return -1;
    if (mv->bits16) {
        lj_pos_patch16(mv->p, ofs);
        if (base + ofs[1] >= LJ_CHANS || base + ofs[3] >= LJ_CHANS) return -1;
        *pan = lj_pos_from_dmx16((uint16_t)((sc->val[base + ofs[0]] << 8) | sc->val[base + ofs[1]]));
        *tilt = lj_pos_from_dmx16((uint16_t)((sc->val[base + ofs[2]] << 8) | sc->val[base + ofs[3]]));
    } else {
        if (base + mv->p->pan_ofs >= LJ_CHANS || base + mv->p->tilt_ofs >= LJ_CHANS) return -1;
        *pan = lj_pos_from_dmx8(sc->val[base + mv->p->pan_ofs]);
        *tilt = lj_pos_from_dmx8(sc->val[base + mv->p->tilt_ofs]);
    }
    return 0;
}

static void put_byte(lj_scene *sc, int ch, uint8_t v)
{
    sc->val[ch] = v;
    if (sc->state[ch] == LJ_OFF) sc->state[ch] = LJ_SNAP;   /* PROPOSAL: a programmed position activates an Off channel as Snap (the Home hotkey "sets fade state to snap", 04 s4.1) */
}

/* which: 1 = pan, 2 = tilt; also drops the fixture's preset reference (a pan/tilt change replaces it with physical values, 04 s4.3) */
static void write_pos(const lj_mv *mv, lj_scene *sc, int f, int32_t pan, int32_t tilt, int which)
{
    int base = fixture_base(mv, f);
    uint8_t ofs[4];
    if (base < 0) return;
    if (mv->bits16) {
        uint16_t p16 = lj_pos_to_dmx16(pan), t16 = lj_pos_to_dmx16(tilt);
        lj_pos_patch16(mv->p, ofs);
        if (base + ofs[1] >= LJ_CHANS || base + ofs[3] >= LJ_CHANS) return;
        if (which & 1) { put_byte(sc, base + ofs[0], (uint8_t)(p16 >> 8)); put_byte(sc, base + ofs[1], (uint8_t)(p16 & 0xFF)); }
        if (which & 2) { put_byte(sc, base + ofs[2], (uint8_t)(t16 >> 8)); put_byte(sc, base + ofs[3], (uint8_t)(t16 & 0xFF)); }
    } else {
        if (base + mv->p->pan_ofs >= LJ_CHANS || base + mv->p->tilt_ofs >= LJ_CHANS) return;
        if (which & 1) put_byte(sc, base + mv->p->pan_ofs, lj_pos_to_dmx8(pan));
        if (which & 2) put_byte(sc, base + mv->p->tilt_ofs, lj_pos_to_dmx8(tilt));
    }
    sc->preset[f] = 0;
}

static void write_state(const lj_mv *mv, lj_scene *sc, int f, int which, uint8_t state)
{
    int base = fixture_base(mv, f);
    uint8_t ofs[4];
    if (base < 0) return;
    lj_pos_patch16(mv->p, ofs);
    if (!mv->bits16) { ofs[0] = ofs[1] = mv->p->pan_ofs; ofs[2] = ofs[3] = mv->p->tilt_ofs; }
    if (base + ofs[1] >= LJ_CHANS || base + ofs[3] >= LJ_CHANS) return;
    if (which & 1) sc->state[base + ofs[0]] = sc->state[base + ofs[1]] = state;
    if (which & 2) sc->state[base + ofs[2]] = sc->state[base + ofs[3]] = state;
}

static void mark_modified(void)
{
    lj_ctx *c = lj_ctx_get();
    if (c->show && c->show->current_seq) c->show->current_seq->modified = 1;
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
}

static void repaint(lj_mv *mv)
{
    lj_widget_invalidate(&mv->grid->w);
    lj_widget_invalidate(mv->readout);
}

/* the master's state from the scene into the window */
static void reload(lj_mv *mv)
{
    lj_scene *sc = scene_of();
    int base;
    uint8_t ofs[4];
    mv->preset = 0;
    if (!sc || read_pos(mv, sc, mv->master, &mv->pan, &mv->tilt) != 0) { mv->pan = mv->tilt = 0; repaint(mv); return; }
    mv->preset = sc->preset[mv->master];
    base = fixture_base(mv, mv->master);
    lj_pos_patch16(mv->p, ofs);
    if (!mv->bits16) { ofs[0] = mv->p->pan_ofs; ofs[2] = mv->p->tilt_ofs; }
    lj_w_osf_set_state(mv->bpan_fade, sc->state[base + ofs[0]]);
    lj_w_osf_set_state(mv->btilt_fade, sc->state[base + ofs[2]]);
    repaint(mv);
}

/* the one move path: locks, Absolute / Relative, the 8-bit snap, every selected fixture of the profile */
static void move_to(lj_mv *mv, int32_t pan, int32_t tilt, int force_absolute)
{
    lj_scene *sc;
    int32_t dp, dt, p, t;
    int i, which = 3;
    if (!force_absolute) {
        if (mv->lock_pan) { pan = mv->pan; which &= ~1; }
        if (mv->lock_tilt) { tilt = mv->tilt; which &= ~2; }
    }
    pan = lj_pos_clamp(pan); tilt = lj_pos_clamp(tilt);
    if (!mv->bits16) { pan = lj_pos_snap8(pan); tilt = lj_pos_snap8(tilt); }
    if (!which || mv->master < 0) return;
    dp = pan - mv->pan; dt = tilt - mv->tilt;
    lj_lock();
    sc = scene_of();
    if (sc) {
        for (i = 0; i < mv->nfx; i++) {
            if (mv->absolute || force_absolute || mv->fx[i] == mv->master) write_pos(mv, sc, mv->fx[i], pan, tilt, which);
            else if (read_pos(mv, sc, mv->fx[i], &p, &t) == 0) write_pos(mv, sc, mv->fx[i], lj_pos_relative(p, dp), lj_pos_relative(t, dt), which);
        }
        mark_modified();
    }
    lj_unlock();
    reload(mv);
}

static void set_states(lj_mv *mv, int which, uint8_t state)
{
    lj_scene *sc;
    int i;
    lj_lock();
    sc = scene_of();
    if (sc) { for (i = 0; i < mv->nfx; i++) write_state(mv, sc, mv->fx[i], which, state); mark_modified(); }
    lj_unlock();
}

static void ini_save(const lj_mv *mv)
{
    lj_ini *ini = lj_ctx_get()->main_ini;
    if (!ini) return;
    lj_ini_seti(ini, LJ_MV_SECTION, "Abs", mv->absolute);
    lj_ini_seti(ini, LJ_MV_SECTION, mv->bits16 ? "Res16Bit" : "Res8Bit", mv->res);
}

/* ---- the grid widget: #8080FF field, #000080 lines every 20 px from the centre, #FF0000 crosshair, white dot [07 s8] ---- */
static void dot_to_px(int32_t v, int *px) { *px = (int)(((int32_t)(v + LJ_POS_HALF) * (LJ_MV_GRID_W - 1)) / 65535); }
static int32_t px_to_pos(int px) { return lj_pos_clamp((int32_t)(((int32_t)px * 65535 + (LJ_MV_GRID_W - 1) / 2) / (LJ_MV_GRID_W - 1)) - LJ_POS_HALF); }

static void grid_paint(lj_widget *w, lj_surface *s)
{
    static const int dot_half[11] = { 2, 3, 4, 4, 5, 5, 5, 4, 4, 3, 2 };   /* an 11-px round dot (~12 px in movement2.gif) */
    lj_mv *mv = ((lj_mv_grid *)w)->mv;
    int ax, ay, k, cx, cy, c = LJ_MV_GRID_W / 2;
    lj_widget_abs(w, &ax, &ay);
    lj_bevel1(s, ax, ay, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);   /* the 1-px sunken edge around the field */
    ax++; ay++;
    lj_fill(s, ax, ay, LJ_MV_GRID_W, LJ_MV_GRID_W, LJ_MV_CLR_GRID);
    for (k = LJ_MV_GRID_STEP; c + k < LJ_MV_GRID_W; k += LJ_MV_GRID_STEP) {
        lj_vline(s, ax + c - k, ay, LJ_MV_GRID_W, LJ_MV_CLR_LINE); lj_vline(s, ax + c + k, ay, LJ_MV_GRID_W, LJ_MV_CLR_LINE);
        lj_hline(s, ax, ay + c - k, LJ_MV_GRID_W, LJ_MV_CLR_LINE); lj_hline(s, ax, ay + c + k, LJ_MV_GRID_W, LJ_MV_CLR_LINE);
    }
    lj_vline(s, ax + c, ay, LJ_MV_GRID_W, LJ_MV_CLR_CROSS);
    lj_hline(s, ax, ay + c, LJ_MV_GRID_W, LJ_MV_CLR_CROSS);
    dot_to_px(mv->pan, &cx); dot_to_px(mv->tilt, &cy);
    for (k = 0; k < 11; k++) lj_fill(s, ax + cx - dot_half[k], ay + cy - LJ_MV_DOT_R + k, 2 * dot_half[k] + 1, 1, LJ_CLR_BTNHIGHLIGHT);
}

static int grid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_mv_grid *g = (lj_mv_grid *)w;
    lj_mv *mv = g->mv;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (!lj_w_inside(w, m->x, m->y)) return 0;
        if (m->button == 2) { move_to(mv, px_to_pos(m->x - 1), px_to_pos(m->y - 1), 1); return 1; }   /* Goto Position: always absolute (CHM 3107) */
        if (m->button != 1) return 0;
        g->drag = 1; g->lx = m->x; g->ly = m->y;
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (!g->drag) return 0;
        if (m->x != g->lx || m->y != g->ly) {   /* each pixel of drag = Resolution readout units (CHM 3057; the cursor reset of positioncontrol.htm is the platform's) */
            move_to(mv, mv->pan + (int32_t)(m->x - g->lx) * mv->res, mv->tilt + (int32_t)(m->y - g->ly) * mv->res, 0);
            g->lx = m->x; g->ly = m->y;
        }
        return 1;
    case LJ_EV_MOUSE_UP: case LJ_EV_MOUSE_LEAVE:
        g->drag = 0;
        return 1;
    default:
        return 0;
    }
}
static const lj_widget_vt grid_vt = { grid_paint, grid_mouse, NULL, NULL };

/* ---- the readout under the grid: sunken, "-16256 : 17120" or the preset ---- */
int lj_ctl_movement_readout(lj_window *win, char *dst, size_t cap)
{
    lj_mv *mv = mv_of(win);
    if (!mv || !cap) return -1;
    if (mv->preset) return snprintf(dst, cap, "Preset %03d", mv->preset) < (int)cap ? 0 : -1;   /* PROPOSAL: the model stores no preset names, so the number stands in for the name */
    return snprintf(dst, cap, "%d : %d", (int)mv->pan, (int)mv->tilt) < (int)cap ? 0 : -1;
}

static void readout_paint(lj_widget *w, lj_surface *s)
{
    lj_mv *mv = ((lj_mv_readout *)w)->mv;
    char text[32];
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    lj_edge_sunken(s, ax, ay, w->w, w->h, w->color);
    lj_ctl_movement_readout(mv->win, text, sizeof text);
    lj_w_text_c(s, w, ax, ay, w->w, w->h, text, LJ_CLR_3DDKSHADOW, 0);
}
static const lj_widget_vt readout_vt = { readout_paint, NULL, NULL, NULL };

/* ---- coloured push buttons (Clr Macro yellow) and the blue arrow pad ---- */
static void cbtn_paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *b = (lj_w_cap *)w;
    int ax, ay, p = b->pressed && b->hover;
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, w->color, p);
    lj_w_text_c(s, w, ax + p, ay + p, w->w, w->h, b->caption, LJ_CLR_3DDKSHADOW, 0);
}
static int cbtn_mouse(lj_widget *w, const lj_mouse_ev *m) { return w->enabled ? lj_w_button_mouse((lj_w_cap *)w, m, 0) : 0; }
static const lj_widget_vt cbtn_vt = { cbtn_paint, cbtn_mouse, NULL, NULL };

static void arrow_paint(lj_widget *w, lj_surface *s)
{
    lj_w_cap *b = (lj_w_cap *)w;
    int ax, ay, p = b->pressed && b->hover, cx, cy, dir = w->desc ? w->desc->tag : -1;
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, w->color, p);
    cx = ax + w->w / 2 + p; cy = ay + w->h / 2 + p;
    if (dir >= 0) {   /* arrows: 7-row solid triangle, blue (ptarrows.gif) */
        static const int ox[4] = { 0, 0, -3, 3 }, oy[4] = { -3, 3, 0, 0 };
        lj_w_tri(s, cx + ox[dir], cy + oy[dir], 7, dir, 0x0000FFu);
    } else {          /* home: the four-way cross (pthome.gif) */
        lj_w_tri(s, cx, cy - 6, 4, 0, 0x0000FFu); lj_w_tri(s, cx, cy + 6, 4, 1, 0x0000FFu);
        lj_w_tri(s, cx - 6, cy, 4, 2, 0x0000FFu); lj_w_tri(s, cx + 6, cy, 4, 3, 0x0000FFu);
        lj_fill(s, cx - 1, cy - 6, 3, 13, 0x0000FFu); lj_fill(s, cx - 6, cy - 1, 13, 3, 0x0000FFu);
    }
}
static const lj_widget_vt arrow_vt = { arrow_paint, cbtn_mouse, NULL, NULL };

/* ---- the Resolution bar: lj_w_dragbar's mouse/key, our text "Resolution 32" (integer, not a percentage: IMG ptresolution) ---- */
static void res_paint(lj_widget *w, lj_surface *s)
{
    lj_w_dragbar *b = (lj_w_dragbar *)w;
    lj_mv *mv = (lj_mv *)w->ud;
    char text[32];
    int ax, ay, ix = LJ_DRAGBAR_ARROW + 3, iw = w->w - 2 * LJ_DRAGBAR_ARROW - 4, fw;
    mv->res_paint0(w, s);
    lj_widget_abs(w, &ax, &ay);
    fw = (iw * b->value + b->max / 2) / b->max;
    lj_fill(s, ax + ix, ay + 2, iw, w->h - 4, LJ_CLR_BTNSHADOW);
    lj_fill(s, ax + ix, ay + 2, fw, w->h - 4, b->fill);
    snprintf(text, sizeof text, "Resolution %d", mv->res);
    lj_w_text_c(s, w, ax + ix, ay, iw, w->h, text, LJ_CLR_3DDKSHADOW, 0);
}

static int presets_selected(lj_mv *mv);

/* ---- widget events ---- */
static void on_widget(lj_widget *w, int ev, void *ud)
{
    lj_mv *mv = ud;
    const char *n = w->name ? w->name : "";
    if (ev == LJ_WEV_CHANGE && strcmp(n, "MvRes") == 0) {
        mv->res = lj_w_dragbar_value(w) < 1 ? 1 : lj_w_dragbar_value(w);
        ini_save(mv);
        return;
    }
    if (ev != LJ_WEV_CLICK) return;
    if (strcmp(n, "MvUp") == 0)        move_to(mv, mv->pan, lj_pos_step(mv->tilt, -1, mv->res), 0);   /* positive tilt = down on screen [17 s1.2] */
    else if (strcmp(n, "MvDown") == 0) move_to(mv, mv->pan, lj_pos_step(mv->tilt, 1, mv->res), 0);
    else if (strcmp(n, "MvLeft") == 0) move_to(mv, lj_pos_step(mv->pan, -1, mv->res), mv->tilt, 0);
    else if (strcmp(n, "MvRight") == 0) move_to(mv, lj_pos_step(mv->pan, 1, mv->res), mv->tilt, 0);
    else if (strcmp(n, "MvHome") == 0) lj_ctl_movement_home(mv->win);
    else if (strcmp(n, "MvAbs") == 0)  lj_ctl_movement_set_absolute(mv->win, !mv->absolute);
    else if (strcmp(n, "MvLockPan") == 0 || strcmp(n, "MvLockTilt") == 0)
        lj_ctl_movement_lock(mv->win, lj_w_speedbutton_down(lj_window_find(mv->win, "MvLockPan")), lj_w_speedbutton_down(lj_window_find(mv->win, "MvLockTilt")));
    else if (strcmp(n, "MvPanFade") == 0)  set_states(mv, 1, (uint8_t)lj_w_osf_state(w));
    else if (strcmp(n, "MvTiltFade") == 0) set_states(mv, 2, (uint8_t)lj_w_osf_state(w));
    else if (strcmp(n, "MvApply") == 0) { int k = mv->presets ? presets_selected(mv) : -1; if (k > 0) lj_ctl_movement_preset_apply(mv->win, k); }   /* the row highlighted in the Position Presets list (CHM positionpresets) */
    else if (strcmp(n, "MvClear") == 0) lj_ctl_movement_preset_clear(mv->win);
    else if (strcmp(n, "MvClrMacro") == 0) {   /* W3: clears the movement macro of the selected fixtures from the scene */
        lj_scene *sc; int i;
        lj_lock(); sc = scene_of();
        if (sc) { for (i = 0; i < mv->nfx; i++) memset(&sc->macro[mv->fx[i]], 0, sizeof sc->macro[0]); mark_modified(); }
        lj_unlock();
    }
}

static void on_menu(lj_widget *w, void *ud)
{
    lj_mv *mv = ud;
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    const char *n = it && it->name ? it->name : "";
    if (strcmp(n, "M_Presets") == 0) lj_ctl_movement_presets_open(mv->win);
    else if (strcmp(n, "M_Help") == 0) lj_log("movement: Help: no help file is shipped (D2)");
    else if (strcmp(n, "M_Macro") == 0) lj_win_open(LJ_WIN_MOVEMACRO_EDITOR);   /* W3 movement [04 s4.4] */
    else if (strcmp(n, "M_FanOut") == 0) lj_win_open(LJ_WIN_FAN_OUT);          /* W3 movement [04 s4.2] */
}

/* ---- the menu: top-level captions verbatim [04 s1.6]; accelerators and the children's captions follow the help text (PROPOSAL, the control is code-built) ---- */
static const lj_menu_item_desc mv_pref_items[] = {
    { "M_Goto", "&Goto Position", .on_click = "MenuClick", .radio_item = LJ_YES, .checked = LJ_YES },
    { "M_LockCursor", "&Lock Cursor in Grid", .on_click = "MenuClick", .radio_item = LJ_YES, .enabled = LJ_NO, .hint = LJ_MV_NOT_AVAIL },
};
static const lj_menu_item_desc mv_options_items[] = {
    { "M_PosOptions", "&Position Options", .on_click = "MenuClick", .enabled = LJ_NO, .hint = LJ_MV_NOT_AVAIL },   /* W2: TMOVEMENTOPTIONSFORM */
    { "M_FanOut", "&Fan Out", .on_click = "MenuClick" },                                                          /* W3 movement: TFANOUTFORM (lj_movemacro_editor.c) */
};
static const lj_menu_item_desc mv_menu_items[] = {
    { "M_Presets", "&Presets", .on_click = "MenuClick" },
    { "M_Preferences", "P&references", .children = mv_pref_items, .nchildren = 2 },
    { "M_Fades", "&Fades", .on_click = "MenuClick", .enabled = LJ_NO, .hint = LJ_MV_NOT_AVAIL },   /* W2: the sequence fades control [04 s1.4] */
    { "M_Macro", "&Macro", .on_click = "MenuClick" },                                             /* W3 movement: the Movement Macro window */
    { "M_Options", "&Options", .children = mv_options_items, .nchildren = 2 },
    { "M_Help", "&Help", .on_click = "MenuClick" },
};
static const lj_menu_desc mv_menus[] = { { "MainMenu1", 1, mv_menu_items, LJ_COUNT_OF(mv_menu_items) } };

/* ---- widget descs: client px measured from movement2.gif / joystickmovement.gif (surface minus the 3/19/19 frame), rig item ---- */
static const lj_ctl_desc d_panel   = { LJ_K_PANEL, "MvPanel", LJ_MV_PANEL_X, LJ_MV_PANEL_Y, LJ_MV_PANEL_W, LJ_MV_PANEL_W, .bevel_outer = LJ_BV_RAISED };
static const lj_ctl_desc d_grid    = { LJ_K_UNKNOWN, "MvGrid", LJ_MV_GRID_X - LJ_MV_PANEL_X - 1, LJ_MV_GRID_Y - LJ_MV_PANEL_Y - 1, LJ_MV_GRID_W + 2, LJ_MV_GRID_W + 2, .tag = 0 };
static const lj_ctl_desc d_readout = { LJ_K_UNKNOWN, "MvReadout", 2, 211, 115, 23, .tag = 0 };
static const lj_ctl_desc d_clrmac  = { LJ_K_BUTTON, "MvClrMacro", 153, 211, 56, 24, .caption = "Clr Macro", .color = LJ_CLR(0xFFFF00) };   /* clearmacroicon.gif 56x24 */
static const lj_ctl_desc d_group   = { LJ_K_GROUPBOX, "MvPresetGroup", 216, 2, 106, 42, .caption = "Position Preset" };               /* movementpreset.gif 106x42 */
static const lj_ctl_desc d_apply   = { LJ_K_BUTTON, "MvApply", 2, 16, 50, 24, .caption = "Apply" };
static const lj_ctl_desc d_clear   = { LJ_K_BUTTON, "MvClear", 54, 16, 50, 24, .caption = "Clear" };
static const lj_ctl_desc d_abs     = { LJ_K_BUTTON, "MvAbs", 217, 49, 104, 24, .caption = "Absolute Position" };                      /* ptabsrel.gif 103x23 */
static const lj_ctl_desc d_lockpan = { LJ_K_SPEEDBUTTON, "MvLockPan", 217, 78, 51, 20, .caption = "Pan", .group_index = 1, .allow_all_up = LJ_YES };    /* lockpt.gif 105x20; padlock glyph: art request */
static const lj_ctl_desc d_locktlt = { LJ_K_SPEEDBUTTON, "MvLockTilt", 270, 78, 51, 20, .caption = "Tilt", .group_index = 2, .allow_all_up = LJ_YES };
static const lj_ctl_desc d_res     = { LJ_K_UNKNOWN, "MvRes", 216, 100, 106, 24, .caption = "Resolution" };                           /* ptresolution.gif 106x24 */
static const lj_ctl_desc d_arrows[5] = {   /* 3x3 pad of 32x25 buttons at (221,131), 96x75 (joystickmovement.gif); tag = lj_w_tri dir, -1 home */
    { LJ_K_BUTTON, "MvUp",    253, 131, 32, 25, .tag = 0 },
    { LJ_K_BUTTON, "MvDown",  253, 181, 32, 25, .tag = 1 },
    { LJ_K_BUTTON, "MvLeft",  221, 156, 32, 25, .tag = 2 },
    { LJ_K_BUTTON, "MvRight", 285, 156, 32, 25, .tag = 3 },
    { LJ_K_BUTTON, "MvHome",  253, 156, 32, 25, .tag = -1 },
};

static lj_widget *hook(lj_widget *w, lj_mv *mv) { w->on_event = on_widget; w->ud = mv; return w; }

static lj_widget *osf(lj_widget *parent, lj_mv *mv, const char *name, int x)   /* Pan Fade / Tilt Fade: 50x23 green buttons at y 212 (joystickmovement.gif) */
{
    lj_widget *w = lj_w_osf_create(parent, NULL);
    w->name = name; w->x = x; w->y = 212; w->w = 50; w->h = 23;
    return hook(w, mv);
}

lj_window *lj_ctl_movement_create(const lj_profile *p)
{
    const lj_ctl_window *tab = p ? lj_profile_window(p, LJ_CTL_MOVEMENT) : NULL;
    lj_ini *ini = lj_ctx_get()->main_ini;
    lj_mv *mv;
    lj_widget *root, *panel, *group, *w;
    int i;
    if (!p || (!p->pt16 && !p->pt8)) { lj_log("movement: no pan/tilt in profile %u", p ? (unsigned)p->id : 0u); return NULL; }
    mv = calloc(1, sizeof *mv);
    if (!mv) lj_fatal("movement: out of memory");
    mv->p = p; mv->bits16 = p->pt16 != 0; mv->master = -1;
    mv->res = ini ? lj_ini_geti(ini, LJ_MV_SECTION, mv->bits16 ? "Res16Bit" : "Res8Bit", mv->bits16 ? LJ_POS_RES16_DEFAULT : LJ_POS_RES8_DEFAULT)
                  : mv->bits16 ? LJ_POS_RES16_DEFAULT : LJ_POS_RES8_DEFAULT;
    if (mv->res < 1) mv->res = 1;
    if (mv->res > (mv->bits16 ? LJ_POS_RES16_MAX : LJ_POS_RES8_MAX)) mv->res = mv->bits16 ? LJ_POS_RES16_MAX : LJ_POS_RES8_MAX;
    mv->absolute = ini ? lj_ini_geti(ini, LJ_MV_SECTION, "Abs", 1) != 0 : 1;

    mv->desc.name = "MovementControl"; mv->desc.class_name = "TMovement";   /* lj_formsini_section -> MOVEMENT, the section lj_ctl_common's chrome table names (LJ2 keeps it in Main.Ini, 7.10) */
    mv->desc.caption = tab && tab->caption ? tab->caption : lj_mv_caption[mv->bits16 != 0];
    mv->desc.client_width = LJ_MV_CLIENT_W; mv->desc.client_height = LJ_MV_CLIENT_H; mv->desc.text_height = 13;
    mv->desc.color = LJ_CLR(LJ_CLR_BTNFACE); mv->desc.font = LJ_FONT_SANS13; mv->desc.font_name = "MS Sans Serif"; mv->desc.font_height = -11;
    mv->desc.border = LJ_BS_SINGLE; mv->desc.icons = LJ_BI_GIVEN | LJ_BI_SYSMENU; mv->desc.style = LJ_FS_STAYONTOP; mv->desc.position = LJ_PO_DESIGNED;
    mv->desc.menus = mv_menus; mv->desc.nmenus = 1; mv->desc.menu = "MainMenu1";
    {
        lj_binding b[] = { { "MenuClick", on_menu, mv }, { NULL, NULL, NULL } };
        mv->win = lj_window_create(&mv->desc, b);
    }
    if (!mv->win) { free(mv); return NULL; }
    root = lj_window_root(mv->win);

    panel = lj_w_panel_create(root, &d_panel);
    mv->grid = lj_w_alloc(sizeof *mv->grid, panel, &d_grid, &grid_vt, 0);
    mv->grid->mv = mv;
    mv->readout = lj_w_alloc(sizeof(lj_mv_readout), root, &d_readout, &readout_vt, 0);
    ((lj_mv_readout *)mv->readout)->mv = mv;
    hook(lj_w_cap_new(root, &d_clrmac, &cbtn_vt, sizeof(lj_w_cap), 0), mv);
    group = lj_w_groupbox_create(root, &d_group);
    hook(lj_w_button_create(group, &d_apply), mv);
    hook(lj_w_button_create(group, &d_clear), mv);
    mv->babs = hook(lj_w_button_create(root, &d_abs), mv);
    hook(lj_w_speedbutton_create(root, &d_lockpan), mv);
    hook(lj_w_speedbutton_create(root, &d_locktlt), mv);
    mv->bres = hook(lj_w_dragbar_create(root, &d_res), mv);
    ((lj_w_dragbar *)mv->bres)->max = mv->bits16 ? LJ_POS_RES16_MAX : LJ_POS_RES8_MAX;
    lj_w_dragbar_set_value(mv->bres, mv->res);
    lj_w_dragbar_set_fill(mv->bres, 0x00FFFFu);                 /* cyan fill (ptresolution.gif) */
    mv->resvt = *mv->bres->vt; mv->res_paint0 = mv->bres->vt->paint; mv->resvt.paint = res_paint; mv->bres->vt = &mv->resvt;
    for (i = 0; i < 5; i++) { w = lj_w_cap_new(root, &d_arrows[i], &arrow_vt, sizeof(lj_w_cap), 0); hook(w, mv); }
    mv->bpan_fade = osf(root, mv, "MvPanFade", 216);
    mv->btilt_fade = osf(root, mv, "MvTiltFade", 270);
    lj_ctl_movement_set_absolute(mv->win, mv->absolute);
    lj_widget_layout_hooks(root);
    reload(mv);
    return mv->win;
}

void lj_ctl_movement_destroy(lj_window *win)
{
    lj_mv *mv = mv_of(win);
    if (!mv) return;
    if (mv->presets) lj_window_destroy(mv->presets);
    lj_window_destroy(win);
    free(mv);
}

void lj_ctl_movement_select(lj_window *win, int master, const int *fixtures, int n)
{
    lj_mv *mv = mv_of(win);
    int i;
    if (!mv) return;
    mv->master = master >= 0 && master < LJ_FIXTURES ? master : -1;
    mv->nfx = 0;
    if (mv->master >= 0) mv->fx[mv->nfx++] = mv->master;
    for (i = 0; i < n && mv->nfx < LJ_FIXTURES; i++)
        if (fixtures[i] >= 0 && fixtures[i] < LJ_FIXTURES && fixtures[i] != mv->master) mv->fx[mv->nfx++] = fixtures[i];
    reload(mv);
}

void lj_ctl_movement_select_from_desktop(lj_window *win)
{
    int fx[LJ_FIXTURES], n = 0, i;
    for (i = 0; i < LJ_FIXTURES; i++) if (lj_sel_is_selected(i)) fx[n++] = i;
    lj_ctl_movement_select(win, lj_sel_master(), fx, n);
}

void lj_ctl_movement_refresh(lj_window *win) { lj_mv *mv = mv_of(win); if (mv) reload(mv); }

int lj_ctl_movement_position(lj_window *win, int32_t *pan, int32_t *tilt)
{
    lj_mv *mv = mv_of(win);
    if (!mv || mv->master < 0 || !scene_of() || fixture_base(mv, mv->master) < 0) return -1;
    *pan = mv->pan; *tilt = mv->tilt;
    return 0;
}

void lj_ctl_movement_set(lj_window *win, int32_t pan, int32_t tilt) { lj_mv *mv = mv_of(win); if (mv) move_to(mv, pan, tilt, 0); }
void lj_ctl_movement_home(lj_window *win) { lj_mv *mv = mv_of(win); if (mv) move_to(mv, 0, 0, 1); }   /* "Click home to center" (CHM 3045); always absolute (CHM 3107) */
int  lj_ctl_movement_resolution(lj_window *win) { lj_mv *mv = mv_of(win); return mv ? mv->res : -1; }

void lj_ctl_movement_set_resolution(lj_window *win, int res)
{
    lj_mv *mv = mv_of(win);
    if (!mv) return;
    lj_w_dragbar_set_value(mv->bres, res);
    mv->res = lj_w_dragbar_value(mv->bres) < 1 ? 1 : lj_w_dragbar_value(mv->bres);
    ini_save(mv);
}

int lj_ctl_movement_absolute(lj_window *win) { lj_mv *mv = mv_of(win); return mv ? mv->absolute : -1; }

void lj_ctl_movement_set_absolute(lj_window *win, int absolute)
{
    lj_mv *mv = mv_of(win);
    if (!mv) return;
    mv->absolute = absolute != 0;
    lj_w_set_caption(mv->babs, mv->absolute ? "Absolute Position" : "Relative Movement");   /* exe 0x2e9ac8 / 0x2e9ae4 */
    ini_save(mv);
}

void lj_ctl_movement_lock(lj_window *win, int pan_locked, int tilt_locked)
{
    lj_mv *mv = mv_of(win);
    if (!mv) return;
    mv->lock_pan = pan_locked != 0; mv->lock_tilt = tilt_locked != 0;
    lj_w_speedbutton_set_down(lj_window_find(win, "MvLockPan"), mv->lock_pan);
    lj_w_speedbutton_set_down(lj_window_find(win, "MvLockTilt"), mv->lock_tilt);
}

/* ---- position presets [04 s4.3, F36]: show->preset[fixture][n-1], references in scene->preset[fixture] ---- */
static int in_set(const lj_mv *mv, int f) { int i; for (i = 0; i < mv->nfx; i++) if (mv->fx[i] == f) return 1; return 0; }

int lj_ctl_movement_preset_record(lj_window *win, int n, int filter)
{
    lj_mv *mv = mv_of(win);
    lj_show *show = lj_ctx_get()->show;
    lj_scene *sc;
    int f, rc = 0;
    int32_t pan, tilt;
    if (!mv || n < 1 || n > LJ_PRESETS || !show || mv->master < 0) return -1;
    lj_lock();
    sc = scene_of();
    if (!sc) rc = -1;
    else for (f = 0; f < LJ_FIXTURES; f++) {
        lj_preset *pr = &show->preset[f][n - 1];
        int take = filter == 2 ? 1 : filter == 1 ? in_set(mv, f) : f == mv->master;
        if (show->fixture[f].link == 0) continue;
        if (take && read_pos(mv, sc, f, &pan, &tilt) == 0) { pr->used = 1; pr->pan = (int16_t)pan; pr->tilt = (int16_t)tilt; }
        else if (!pr->used) { pr->used = 1; pr->pan = pr->tilt = LJ_POS_LO; }   /* excluded fixtures get top/left on first save (CHM positionpresets) */
    }
    lj_unlock();
    return rc;
}

int lj_ctl_movement_preset_apply(lj_window *win, int n)
{
    lj_mv *mv = mv_of(win);
    lj_show *show = lj_ctx_get()->show;
    lj_scene *sc;
    int i, rc = -1;
    if (!mv || n < 1 || n > LJ_PRESETS || !show) return -1;
    lj_lock();
    sc = scene_of();
    if (sc) for (i = 0; i < mv->nfx; i++) {
        const lj_preset *pr = &show->preset[mv->fx[i]][n - 1];
        if (!pr->used || fixture_base(mv, mv->fx[i]) < 0) continue;
        write_pos(mv, sc, mv->fx[i], pr->pan, pr->tilt, 3);
        sc->preset[mv->fx[i]] = (uint8_t)n;
        rc = 0;
    }
    if (rc == 0) mark_modified();
    lj_unlock();
    reload(mv);
    return rc;
}

void lj_ctl_movement_preset_clear(lj_window *win)
{
    lj_mv *mv = mv_of(win);
    lj_scene *sc;
    int i;
    if (!mv) return;
    lj_lock();
    sc = scene_of();
    if (sc) { for (i = 0; i < mv->nfx; i++) sc->preset[mv->fx[i]] = 0; mark_modified(); }
    lj_unlock();
    reload(mv);
}

int lj_ctl_movement_preset_delete(int n)
{
    lj_show *show = lj_ctx_get()->show;
    int f;
    if (n < 1 || n > LJ_PRESETS || !show) return -1;
    lj_lock();
    for (f = 0; f < LJ_FIXTURES; f++) memset(&show->preset[f][n - 1], 0, sizeof show->preset[0][0]);
    lj_unlock();
    return 0;
}

/* ---- the Position Presets dialog: TPRESETFORM bound ---- */
extern const lj_form_desc lj_form_TPRESETFORM;

static int preset_used(const lj_show *show, int n)
{
    int f;
    for (f = 0; f < LJ_FIXTURES; f++) if (show->fixture[f].link && show->preset[f][n - 1].used) return 1;
    return 0;
}

static void presets_fill(lj_mv *mv)
{
    const lj_show *show = lj_ctx_get()->show;
    lj_widget *lv = lj_window_find(mv->presets, "lv");
    char text[32];
    int n, keep = lv ? ((lj_w_listview *)lv)->index : -1;
    mv->npresets = 0;
    for (n = 1; show && n <= LJ_PRESETS; n++) {
        if (!preset_used(show, n)) continue;
        snprintf(mv->preset_text[mv->npresets], sizeof mv->preset_text[0], "%03d\tPreset %03d", n, n);   /* No / Name columns; no name in the model (PROPOSAL) */
        mv->preset_items[mv->npresets] = mv->preset_text[mv->npresets];
        mv->preset_no[mv->npresets++] = n;
    }
    if (lv) { lj_w_listview_set_items(lv, mv->preset_items, mv->npresets); if (keep < mv->npresets) ((lj_w_listview *)lv)->index = keep; }
    snprintf(text, sizeof text, "%d Entries", mv->npresets);
    lj_w_set_caption(lj_window_find(mv->presets, "lentries"), text);
}

static int presets_selected(lj_mv *mv)
{
    lj_widget *lv = lj_window_find(mv->presets, "lv");
    int i = lv ? ((lj_w_listview *)lv)->index : -1;
    return i >= 0 && i < mv->npresets ? mv->preset_no[i] : -1;
}

static void presets_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void presets_close(lj_widget *w, void *ud) { (void)w; ((lj_mv *)ud)->presets = NULL; }   /* the platform owns the lj_window after OnClose */
static void presets_assign(lj_widget *w, void *ud) { lj_mv *mv = ud; int n = presets_selected(mv); (void)w; if (n > 0) lj_ctl_movement_preset_apply(mv->win, n); }
static void presets_clear(lj_widget *w, void *ud) { (void)w; lj_ctl_movement_preset_clear(((lj_mv *)ud)->win); }
static struct { lj_mv *mv; int n; } presets_del;                 /* the entry the open "Delete Selected Presets?" box acts on */
static void presets_delete_yes(int yes, void *ud)
{
    (void)ud;
    if (yes && presets_del.n > 0) { lj_ctl_movement_preset_delete(presets_del.n); if (presets_del.mv->presets) presets_fill(presets_del.mv); }
    presets_del.n = 0;
}
static void presets_delete(lj_widget *w, void *ud)
{
    lj_mv *mv = ud;
    (void)w;
    if ((presets_del.n = presets_selected(mv)) <= 0) return;
    presets_del.mv = mv;
    lj_win_confirm("Please Confirm..", "Delete Selected Presets?", NULL, presets_delete_yes, NULL);   /* exe 0x346794 (ME-97) */
}
static void presets_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("presets: Help: no help file is shipped (D2)"); }

static void presets_save(lj_widget *w, void *ud)
{
    lj_mv *mv = ud;
    const lj_show *show = lj_ctx_get()->show;
    int n = presets_selected(mv), filter = lj_w_radiogroup_index(lj_window_find(mv->presets, "rgFixtures"));
    (void)w;
    if (n < 0) for (n = 1; n <= LJ_PRESETS && show && preset_used(show, n); n++) { }   /* no row chosen: the first free number */
    if (n > LJ_PRESETS) { lj_log("presets: all %d presets used", LJ_PRESETS); return; }
    if (lj_ctl_movement_preset_record(mv->win, n, filter < 0 ? 1 : filter) == 0) presets_fill(mv);
}

lj_window *lj_ctl_movement_presets_open(lj_window *win)
{
    lj_mv *mv = mv_of(win);
    if (!mv) return NULL;
    if (!mv->presets) {
        lj_binding b[] = {
            { "FormClose", presets_close, mv }, { "FormCreate", presets_noop, mv }, { "FormDestroy", presets_noop, mv },
            { "FormResize", presets_noop, mv }, { "FormShow", presets_noop, mv }, { "lvChange", presets_noop, mv },
            { "lvDblClick", presets_assign, mv }, { "bSaveClick", presets_save, mv }, { "bAssignClick", presets_assign, mv },
            { "bClearClick", presets_clear, mv }, { "Help1Click", presets_help, mv }, { "LvPopPopup", presets_noop, mv },
            { "P_DeletePalettesClick", presets_delete, mv }, { NULL, NULL, NULL } };
        mv->presets = lj_window_create(&lj_form_TPRESETFORM, b);
        if (!mv->presets) return NULL;
        lj_window_set_caption(mv->presets, "Position Presets");   /* [04 s1.6]; the DFM's design caption is 'PresetForm' */
        lj_win_show(mv->presets, 2u /* fsStayOnTop */);            /* W2 acceptance: it was built and never reached the screen */
    }
    presets_fill(mv);
    return mv->presets;
}

lj_window *lj_ctl_movement_presets_window(lj_window *win) { lj_mv *mv = mv_of(win); return mv ? mv->presets : NULL; }
