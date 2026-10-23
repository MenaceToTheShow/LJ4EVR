/* lj_ctl_rgbpack.c - the RGB pack control window (lj_ctl_rgbpack.h). */
#include "controls/lj_ctl_rgbpack.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "model/lj_show.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

/* Geometry: rgbcolorcontrol.gif (513x180) is not one of 07 s14's 1:1 captures, so it gives the arrangement only
 * (5 + 5 status panels left, the programming panel right); the sizes are ours: PROPOSAL rig 07 s9.9 (a 1:1 capture). */
#define LJ_RP_PW     92    /* status panel width */
#define LJ_RP_PH     76    /* status panel height: caption 15, three 16-px rows, swatch 9 */
#define LJ_RP_GAP     4
#define LJ_RP_CAP_H  15
#define LJ_RP_ROW_Y  17
#define LJ_RP_ROW_P  16
#define LJ_RP_OSF    14    /* the letter O/S/F button */
#define LJ_RP_BAR_X  18
#define LJ_RP_BAR_W  44
#define LJ_RP_PROG_P 34    /* programming fader pitch (LJ_FADER_W 32 + 2) */
#define LJ_RP_FADER_H 96
#define LJ_RP_BTN_H  20

static const uint32_t lj_rp_ink[3] = { 0xFF0000u, 0x00FF00u, 0x0000FFu };
static const char *const lj_rp_letter[3] = { "O", "S", "F" };   /* the panel buttons show the state's letter (rgbcolorcontrol.gif) */

typedef struct { lj_widget base; } lj_rp_status;
static struct {
    lj_ctl_win *c; lj_widget *status, *fader[6];
    int units; uint16_t enabled; int prog[6];
    uint8_t shown[LJ_RGBPACK_UNITS * 3]; uint16_t shown_en; int has_shown;
    const lj_widget_vt *osf_vt; lj_widget_vt letter_vt;
} rp;

void lj_hsv_to_rgb(int h, int s, int v, uint8_t rgb[3])
{
    int region, rem, p, q, t;
    if (s <= 0) { rgb[0] = rgb[1] = rgb[2] = (uint8_t)v; return; }
    region = h / 43; rem = (h - region * 43) * 6;
    p = (v * (255 - s)) >> 8; q = (v * (255 - ((s * rem) >> 8))) >> 8; t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
    switch (region) {
    case 0:  rgb[0] = (uint8_t)v; rgb[1] = (uint8_t)t; rgb[2] = (uint8_t)p; break;
    case 1:  rgb[0] = (uint8_t)q; rgb[1] = (uint8_t)v; rgb[2] = (uint8_t)p; break;
    case 2:  rgb[0] = (uint8_t)p; rgb[1] = (uint8_t)v; rgb[2] = (uint8_t)t; break;
    case 3:  rgb[0] = (uint8_t)p; rgb[1] = (uint8_t)q; rgb[2] = (uint8_t)v; break;
    case 4:  rgb[0] = (uint8_t)t; rgb[1] = (uint8_t)p; rgb[2] = (uint8_t)v; break;
    default: rgb[0] = (uint8_t)v; rgb[1] = (uint8_t)p; rgb[2] = (uint8_t)q; break;
    }
}

static const lj_ctl_row *row_of(int u, int k) { return rp.c && rp.c->table && u * 3 + k < rp.c->table->nrows ? &rp.c->table->rows[u * 3 + k] : NULL; }
static int value_of(int u, int k, uint8_t *v, uint8_t *st)
{
    const lj_ctl_row *r = row_of(u, k);
    int ch = r ? lj_prog_master_ch(r->ch) : -1;
    *v = 0; *st = LJ_OFF;
    return ch < 0 ? -1 : lj_prog_get((uint16_t)ch, v, st);
}
static void write_unit(int u, int k, int v)                 /* lj_ctl_write's rule per channel: an Off channel goes to its default state */
{
    const lj_ctl_row *r = row_of(u, k);
    uint8_t cur, st;
    if (!r || value_of(u, k, &cur, &st) != 0) return;
    lj_prog_set_sel(r->ch, (uint8_t)v, st == LJ_OFF ? lj_ctl_dflt_state(rp.c, r->ch) : LJ_PROG_KEEP);
}

int lj_ctl_rgbpack_units(void) { return rp.c && rp.c->win ? rp.units : 0; }
uint16_t lj_ctl_rgbpack_enabled(void) { return rp.enabled; }
void lj_ctl_rgbpack_enable(int unit, int on, int only)
{
    uint16_t all = (uint16_t)((1u << rp.units) - 1u);
    if (unit == 0) rp.enabled = on ? all : 0;
    else if (unit >= 1 && unit <= rp.units) {
        uint16_t bit = (uint16_t)(1u << (unit - 1));
        rp.enabled = (uint16_t)(only ? bit : on ? rp.enabled | bit : rp.enabled & ~bit);
    }
    if (rp.status) lj_widget_invalidate(rp.status);
}

static void show_prog(void)
{
    int k;
    for (k = 0; k < 6; k++) if (rp.fader[k]) lj_w_fader_set_value(rp.fader[k], rp.prog[k]);
}
int lj_ctl_rgbpack_set(int k, int v)
{
    int u, j;
    if (!rp.c || !rp.c->win || k < 0 || k > 5) return -1;
    rp.prog[k] = v < 0 ? 0 : v > 255 ? 255 : v;
    if (k >= 3) {                                           /* HSV sets all three colours (the user-profile rule, 04 s5.3 / colorcontrolsuserdef.htm) */
        uint8_t rgb[3];
        lj_hsv_to_rgb(rp.prog[3], rp.prog[4], rp.prog[5], rgb);
        for (j = 0; j < 3; j++) rp.prog[j] = rgb[j];
    }
    for (u = 0; u < rp.units; u++) if (rp.enabled >> u & 1) {
        if (k < 3) write_unit(u, k, rp.prog[k]);
        else for (j = 0; j < 3; j++) write_unit(u, j, rp.prog[j]);
    }
    show_prog();
    lj_ctl_repaint_all();
    return 0;
}
int lj_ctl_rgbpack_get_color(int unit)
{
    uint8_t v, st;
    int k;
    if (unit < 1 || unit > rp.units) return -1;
    for (k = 0; k < 3; k++) { value_of(unit - 1, k, &v, &st); rp.prog[k] = v; }
    show_prog();
    return 0;
}
void lj_ctl_rgbpack_update(void)
{
    int u, k;
    for (u = 0; u < rp.units; u++) if (rp.enabled >> u & 1) for (k = 0; k < 3; k++) write_unit(u, k, rp.prog[k]);
    lj_ctl_repaint_all();
}
void lj_ctl_rgbpack_snap(int k)
{
    int u;
    if (k < 0 || k > 2) return;
    for (u = 0; u < rp.units; u++) if (rp.enabled >> u & 1) { const lj_ctl_row *r = row_of(u, k); if (r) lj_prog_set_state_sel(r->ch, LJ_SNAP); }
    lj_ctl_repaint_all();
}

/* ---- the status panels: one widget paints all ten, the O/S/F letter buttons sit on top as their own elems ---- */
static void panel_xy(int u, int *x, int *y) { *x = (u % 5) * (LJ_RP_PW + LJ_RP_GAP) + 2; *y = (u / 5) * (LJ_RP_PH + LJ_RP_GAP) + 2; }
static void status_paint(lj_widget *w, lj_surface *s)
{
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, u, k, mf = lj_sel_master();
    const lj_show *sh = lj_ctx_get()->show;
    lj_widget_abs(w, &ax, &ay);
    for (u = 0; u < rp.units; u++) {
        int px, py, tw;
        uint8_t v[3], st;
        char cap[24], num[12];
        panel_xy(u, &px, &py);
        px += ax; py += ay;
        lj_edge_sunken(s, px, py, LJ_RP_PW, LJ_RP_PH, LJ_CLR_BTNFACE);
        if (mf >= 0 && sh && sh->fixture[mf].ch_caption[3 * u][0]) lj_str_copy(cap, sizeof cap, sh->fixture[mf].ch_caption[3 * u]);   /* RGBPack configuration (TRGBPackDefForm) */
        else snprintf(cap, sizeof cap, "Unit %d", u + 1);  /* rgbcolorcontrol.gif */
        lj_fill(s, px + 2, py + 2, LJ_RP_PW - 4, LJ_RP_CAP_H - 2, (rp.enabled >> u & 1) ? 0x0000FFu : LJ_CLR_BTNSHADOW);
        tw = lj_text_width(f, cap);
        lj_text(s, f, px + (LJ_RP_PW - tw) / 2, py + 2, cap, 0xFFFFFFu, 0);
        for (k = 0; k < 3; k++) {
            int ry = py + LJ_RP_ROW_Y + k * LJ_RP_ROW_P, fill;
            value_of(u, k, &v[k], &st);
            lj_edge_sunken(s, px + LJ_RP_BAR_X, ry + 2, LJ_RP_BAR_W, 10, 0xFFFFFFu);
            fill = (LJ_RP_BAR_W - 4) * v[k] / 255;
            if (fill > 0) lj_fill(s, px + LJ_RP_BAR_X + 2, ry + 4, fill, 6, lj_rp_ink[k]);
            snprintf(num, sizeof num, "%d", v[k]);
            lj_text(s, f, px + LJ_RP_PW - 4 - lj_text_width(f, num), ry, num, 0x000000u, 0);
        }
        lj_fill(s, px + 2, py + LJ_RP_PH - 11, LJ_RP_PW - 4, 9, (uint32_t)v[0] << 16 | (uint32_t)v[1] << 8 | v[2]);   /* the resulting colour */
    }
}
static int status_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int u, px, py;
    (void)w;
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1) return m->type == LJ_EV_MOUSE_UP;
    for (u = 0; u < rp.units; u++) {
        panel_xy(u, &px, &py);
        if (m->x < px || m->x >= px + LJ_RP_PW || m->y < py || m->y >= py + LJ_RP_PH) continue;
        if (m->clicks == 2) lj_ctl_rgbpack_get_color(u + 1);
        else lj_ctl_rgbpack_enable(u + 1, !(rp.enabled >> u & 1) || m->ctrl, m->ctrl);
        return 1;
    }
    return 1;
}
static const lj_widget_vt lj_rp_status_vt = { status_paint, status_mouse, NULL, NULL, NULL };

static void letter_paint(lj_widget *w, lj_surface *s)            /* the panel's O/S/F: same widget, the state's letter on the 14-px face */
{
    int ax, ay, st = lj_w_osf_state(w), p = ((lj_w_osf *)w)->pressed;
    static const uint32_t face[3] = { LJ_CLR_RED, LJ_CLR_YELLOW, LJ_CLR_GREEN };
    lj_widget_abs(w, &ax, &ay);
    lj_w_button(s, ax, ay, w->w, w->h, face[st], p);
    lj_w_text_c(s, w, ax + p, ay + p, w->w, w->h, lj_rp_letter[st], LJ_CLR_3DDKSHADOW, 0);
}

/* ---- the programming panel ---- */
static void on_fader(lj_widget *w, int ev, void *ud) { if (ev == LJ_WEV_CHANGE) lj_ctl_rgbpack_set((int)(intptr_t)ud, lj_w_fader_value(w)); }
static void on_button(lj_widget *w, int ev, void *ud)
{
    int id = (int)(intptr_t)ud;
    (void)w;
    if (ev != LJ_WEV_CLICK) return;
    if (id < 3) lj_ctl_rgbpack_snap(id);
    else if (id == 3) lj_ctl_rgbpack_enable(0, 1, 0);
    else if (id == 4) lj_ctl_rgbpack_enable(0, 0, 0);
    else if (id == 5) lj_ctl_rgbpack_update();
}
static lj_widget *owned(lj_widget *parent, enum lj_ctl_row_kind kind, const char *caption, int x, int y, int w, int h)
{
    lj_ctl_elem *e = lj_ctl_elem_add(rp.c, parent, NULL, NULL, LJ_CE_NONE);   /* owns the desc; w patched after creation */
    lj_ctl_desc *d;
    lj_widget *out;
    if (!e) return NULL;
    d = lj_ctl_desc_new(e, kind, caption, x, y, w, h);
    if (kind == LJ_K_TRACKBAR) { d->max = 255; out = lj_w_fader_create(parent, d); }
    else out = lj_w_button_create(parent, d);
    e->w = out;
    return out;
}

void lj_ctl_rgbpack_build(lj_ctl_win *c)
{
    static const char *const snap[3] = { "Snap", "Snap", "Snap" };
    static const char *const btn[3] = { "All", "None", "Update" };
    lj_widget *root = lj_window_root(c->win), *b;
    int u, k, x0, cw, ch;
    memset(&rp, 0, sizeof rp);
    rp.c = c;
    rp.units = c->table ? c->table->nrows / 3 : 0;
    if (rp.units > LJ_RGBPACK_UNITS) rp.units = LJ_RGBPACK_UNITS;
    rp.enabled = (uint16_t)((1u << rp.units) - 1u);          /* every unit enabled at open (PROPOSAL rig 04 s5.3) */
    rp.status = lj_w_alloc(sizeof(lj_rp_status), root, NULL, &lj_rp_status_vt, 0);
    rp.status->name = "rgb_status";
    rp.status->x = 0; rp.status->y = 0; rp.status->w = 5 * (LJ_RP_PW + LJ_RP_GAP); rp.status->h = 2 * (LJ_RP_PH + LJ_RP_GAP);
    for (u = 0; u < rp.units; u++) for (k = 0; k < 3; k++) {    /* the O/S/F row of every channel (elem name row<ch+1>_fade, docs/ui-script.md) */
        int px, py;
        lj_widget *o;
        panel_xy(u, &px, &py);
        o = lj_ctl_osf_new(c, root, &c->table->rows[u * 3 + k], px + 2, py + LJ_RP_ROW_Y + k * LJ_RP_ROW_P, LJ_RP_OSF, LJ_RP_OSF);
        if (!rp.osf_vt) { rp.osf_vt = o->vt; rp.letter_vt = *o->vt; rp.letter_vt.paint = letter_paint; }
        o->vt = &rp.letter_vt;
    }
    x0 = rp.status->w + 2;
    for (k = 0; k < 6; k++) {                                   /* R G B, then H S V */
        rp.fader[k] = owned(root, LJ_K_TRACKBAR, NULL, x0 + k * LJ_RP_PROG_P, 2, LJ_FADER_W, LJ_RP_FADER_H);
        if (rp.fader[k]) { rp.fader[k]->on_event = on_fader; rp.fader[k]->ud = (void *)(intptr_t)k; }
    }
    for (k = 0; k < 3; k++) if ((b = owned(root, LJ_K_BUTTON, snap[k], x0 + k * LJ_RP_PROG_P, LJ_RP_FADER_H + 4, LJ_FADER_W, LJ_RP_BTN_H)) != NULL) { b->on_event = on_button; b->ud = (void *)(intptr_t)k; }
    if ((b = owned(root, LJ_K_BUTTON, "Color Dialog", x0, LJ_RP_FADER_H + 6 + LJ_RP_BTN_H, 6 * LJ_RP_PROG_P - 2, LJ_RP_BTN_H)) != NULL) {
        lj_widget_set_enabled(b, 0);                            /* no OS colour picker behind the platform layer: greyed, docs/fidelity.md */
    }
    for (k = 0; k < 3; k++) if ((b = owned(root, LJ_K_BUTTON, btn[k], x0 + k * 2 * LJ_RP_PROG_P, LJ_RP_FADER_H + 8 + 2 * LJ_RP_BTN_H, 2 * LJ_RP_PROG_P - 2, LJ_RP_BTN_H)) != NULL) { b->on_event = on_button; b->ud = (void *)(intptr_t)(3 + k); }
    cw = x0 + 6 * LJ_RP_PROG_P;
    ch = rp.status->h > LJ_RP_FADER_H + 8 + 3 * LJ_RP_BTN_H ? rp.status->h : LJ_RP_FADER_H + 8 + 3 * LJ_RP_BTN_H;
    lj_window_resize(c->win, cw, ch + 2);
    rp.prog[4] = rp.prog[5] = 255;                              /* full saturation and value: the hue fader alone sweeps the colours */
    show_prog();
}

void lj_ctl_rgbpack_repaint(lj_ctl_win *c)
{
    uint8_t now[LJ_RGBPACK_UNITS * 3], st;
    int u, k;
    if (!c || c != rp.c || !rp.status) return;
    memset(now, 0, sizeof now);
    for (u = 0; u < rp.units; u++) for (k = 0; k < 3; k++) value_of(u, k, &now[u * 3 + k], &st);
    if (!rp.has_shown || memcmp(now, rp.shown, sizeof now) != 0 || rp.shown_en != rp.enabled) {
        memcpy(rp.shown, now, sizeof now); rp.shown_en = rp.enabled; rp.has_shown = 1;
        lj_widget_invalidate(rp.status);
    }
}
