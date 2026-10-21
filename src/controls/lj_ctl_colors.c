/* lj_ctl_colors.c - the Colors window and the controls-c row engine (lj_ctl_colors.h):
 * fade column, scroll control + wheel rotation lines, palette tiles, the specials
 * column, the UserPalette / Preferences / Fades / Help bar, O/S/F <-> scene binding. */
#include "controls/lj_ctl_colors.h"
#include "controls/lj_ctl_gobos.h"
#include "controls/lj_ctl_beam.h"
#include "controls/lj_ctl_effects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_glyphs.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

/* ---- the one button class of these windows: raised or flat, coloured face, glyph / tile or caption ---- */
typedef struct { lj_widget base; const char *caption; const lj_surface *img; uint32_t face, ink; unsigned down:1, pressed:1, flat:1; } lj_ctl_c_btnw;

static void btn_paint(lj_widget *w, lj_surface *s)
{
    lj_ctl_c_btnw *b = (lj_ctl_c_btnw *)w;
    int ax, ay, off = b->down || b->pressed;
    lj_widget_abs(w, &ax, &ay);
    if (b->flat) {
        lj_fill(s, ax, ay, w->w, w->h, b->face);
        if (off) lj_bevel1(s, ax, ay, w->w, w->h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
    } else {
        if (off) lj_button_pressed(s, ax, ay, w->w, w->h); else lj_button_raised(s, ax, ay, w->w, w->h);
        if (b->face != LJ_CLR_BTNFACE) lj_fill(s, ax + 2, ay + 2, w->w - 4, w->h - 4, b->face);
    }
    if (b->img) lj_blit_rgb(s, ax + (w->w - b->img->w) / 2 + off, ay + (w->h - b->img->h) / 2 + off, b->img, 1);
    else if (b->caption) lj_w_text_c(s, w, ax + off, ay + off, w->w, w->h, b->caption, b->ink, 1);
}

static int btn_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_ctl_c_btnw *b = (lj_ctl_c_btnw *)w;
    if (!w->enabled) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button != 1 || !lj_w_inside(w, m->x, m->y)) return 0;
        b->pressed = 1; lj_widget_invalidate(w); return 1;
    case LJ_EV_MOUSE_UP:
        if (!b->pressed) return 0;
        b->pressed = 0; lj_widget_invalidate(w);
        if (lj_w_inside(w, m->x, m->y)) lj_w_emit(w, LJ_WEV_CLICK);
        return 1;
    case LJ_EV_MOUSE_LEAVE:
        if (b->pressed) { b->pressed = 0; lj_widget_invalidate(w); }
        return 1;
    default: return 0;
    }
}

static const lj_widget_vt lj_ctl_c_btn_vt = { btn_paint, btn_mouse, NULL, NULL };

static void on_ev(lj_widget *w, int ev, void *ud);

static lj_widget *place(lj_widget *w, int x, int y, int ww, int h) { w->x = x; w->y = y; w->w = ww; w->h = h; return w; }

lj_widget *lj_ctl_c_btn(lj_ctl_c *c, lj_widget *parent, int x, int y, int w, int h, const char *caption, const char *glyph, uint32_t face, uint32_t ink, int flat)
{
    lj_ctl_c_btnw *b = lj_w_alloc(sizeof *b, parent, NULL, &lj_ctl_c_btn_vt, 0);
    (void)c;
    b->caption = caption; b->img = glyph ? lj_glyph_get(glyph) : NULL; b->face = face; b->ink = ink; b->flat = flat != 0;
    if (glyph && !b->img) b->caption = glyph;          /* art not packed yet: the stem in a box (gfx/lj_glyphs.h rule) */
    return place(&b->base, x, y, w, h);
}

lj_widget *lj_ctl_c_bar(lj_ctl_c *c, lj_widget *parent, int x, int y, int w, int h, const char *caption, int max)
{
    lj_widget *b = lj_w_dragbar_create(parent, NULL);
    (void)c;
    ((lj_w_dragbar *)b)->caption = caption;
    ((lj_w_dragbar *)b)->max = max > 0 ? max : 255;
    return place(b, x, y, w, h);
}

void lj_ctl_c_btn_set_down(lj_widget *w, int down)
{
    lj_ctl_c_btnw *b = (lj_ctl_c_btnw *)w;
    if (b->down == (unsigned)(down != 0)) return;
    b->down = down != 0; lj_widget_invalidate(w);
}
int  lj_ctl_c_btn_down(const lj_widget *w) { return ((const lj_ctl_c_btnw *)w)->down; }
void lj_ctl_c_btn_set_img(lj_widget *w, const lj_surface *img) { ((lj_ctl_c_btnw *)w)->img = img; lj_widget_invalidate(w); }
void lj_ctl_c_btn_set_face(lj_widget *w, uint32_t face, uint32_t ink)
{
    lj_ctl_c_btnw *b = (lj_ctl_c_btnw *)w;
    if (b->face == face && b->ink == ink) return;
    b->face = face; b->ink = ink; lj_widget_invalidate(w);
}

/* ---- window <-> state registry ---- */
static struct { lj_window *win; lj_ctl_c *c; } lj_ctl_c_reg[8];

lj_ctl_c *lj_ctl_c_of(lj_window *win)
{
    int i;
    for (i = 0; i < LJ_COUNT_OF(lj_ctl_c_reg); i++) if (win && lj_ctl_c_reg[i].win == win) return lj_ctl_c_reg[i].c;
    return NULL;
}

/* ---- binds and the scene seam ---- */
lj_ctl_c_bind *lj_ctl_c_bind_add(lj_ctl_c *c, lj_widget *w, const lj_ctl_row *row, int role, int entry, uint8_t ch)
{
    lj_ctl_c_bind *b;
    if (c->nb >= LJ_CTL_C_BINDS) { lj_log("controls: %s: more than %d widgets, row dropped", c->t->caption, LJ_CTL_C_BINDS); w->visible = 0; return NULL; }
    b = &c->b[c->nb++];
    b->c = c; b->row = row; b->btn = NULL; b->w = w; b->role = role; b->entry = entry; b->ch = ch;
    w->on_event = on_ev; w->ud = b;
    return b;
}

int lj_ctl_c_value(lj_ctl_c *c, uint8_t ofs, uint8_t *v, uint8_t *st)
{
    int a = ofs < LJ_PROFILE_CH ? lj_prog_master_ch(ofs) : -1;
    if (a >= 0 && lj_prog_get((uint16_t)a, v, st) == 0) return 0;
    if (ofs < LJ_PROFILE_CH && c->have[ofs]) { *v = c->shadow[ofs]; *st = LJ_SNAP; return 0; }
    return -1;
}

void lj_ctl_c_emit(lj_ctl_c *c, uint8_t ofs, int v)
{
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    lj_prog_set_sel(ofs, (uint8_t)v, LJ_PROG_KEEP);
    if (ofs < LJ_PROFILE_CH) { c->shadow[ofs] = (uint8_t)v; c->have[ofs] = 1; }
    lj_ctl_c_refresh(c->win);
}

int lj_ctl_c_in(const lj_ctl_button *b, int v) { return b && v >= b->lo && v <= b->hi; }

/* ---- wheel entries: the scroll row's buttons, or the profile palette ---- */
static const lj_ctl_row *row_of_kind(const lj_ctl_c *c, enum lj_ctl_elem kind, uint8_t ch)
{
    int i;
    for (i = 0; i < c->t->nrows; i++) if (c->t->rows[i].kind == kind && c->t->rows[i].ch == ch) return &c->t->rows[i];
    return NULL;
}

static int pal_index(const lj_ctl_c *c, uint8_t ch)     /* profile->palette[] slot of the scroll row on ch: the PALETTE row's lo (hand files), else the k-th scroll row of the window */
{
    const lj_ctl_row *pr = row_of_kind(c, LJ_W_PALETTE, ch);
    int i, k = 0, base = c->kind == LJ_CTL_GOBOS ? 2 : c->kind == LJ_CTL_EFFECTS ? 4 : 0;
    if (pr) return pr->lo < LJ_PALETTES ? pr->lo : LJ_PALETTES - 1;
    for (i = 0; i < c->t->nrows; i++) {
        if (c->t->rows[i].kind != LJ_W_SCROLL) continue;
        if (c->t->rows[i].ch == ch) break;
        k++;
    }
    k += base;
    return k < LJ_PALETTES ? k : LJ_PALETTES - 1;
}

/* The Index | Rotate pair governs the nearest scroll row above it (goboindex.gif: the pair sits under the rotating-gobo scroll). */
const lj_ctl_row *lj_ctl_c_scroll_of_mode(const lj_ctl_c *c, const lj_ctl_row *mp)
{
    const lj_ctl_row *sr = NULL;
    int i;
    for (i = 0; i < c->t->nrows; i++) {
        if (c->t->rows[i].kind == LJ_W_SCROLL) sr = &c->t->rows[i];
        if (&c->t->rows[i] == mp) return sr;
    }
    return NULL;
}

static const lj_ctl_row *modepair_of_scroll(const lj_ctl_c *c, const lj_ctl_row *sr)
{
    int i, seen = 0;
    for (i = 0; i < c->t->nrows; i++) {
        const lj_ctl_row *r = &c->t->rows[i];
        if (r == sr) { seen = 1; continue; }
        if (!seen) continue;
        if (r->kind == LJ_W_SCROLL) return NULL;
        if (r->kind == LJ_W_MODEPAIR) return r;
    }
    return NULL;
}

int lj_ctl_c_mode_slot(const lj_ctl_c *c, const lj_ctl_row *sr, const lj_ctl_row *mp, int j)   /* palette value slot mode j reads: buttons[j].value, else Def then the j-th typed slot (16 s5 codes) */
{
    const lj_palette *pal = &c->p->palette[pal_index(c, sr->ch)];
    int t, n = 0;
    if (mp->buttons && j < mp->nbuttons) return mp->buttons[j].value < 5 ? mp->buttons[j].value : 0;
    if (j == 0) return 0;
    for (t = 1; t < 5; t++) if (pal->type[t] && ++n == j) return t;
    return 0;
}

static int entries(const lj_ctl_c *c, const lj_ctl_row *r)
{
    int n = 0;
    if (r->buttons) return r->nbuttons;
    while (n < LJ_PAL_ENTRIES && c->p->palette[pal_index(c, r->ch)].e[n].used) n++;
    return n;
}

int lj_ctl_c_entry_value(const lj_ctl_c *c, const lj_ctl_row *r, int i)
{
    const lj_ctl_row *mp = modepair_of_scroll(c, r);
    const lj_pal_entry *e = &c->p->palette[pal_index(c, r->ch)].e[i];
    if (mp && r->ch < LJ_PROFILE_CH) {
        int t = lj_ctl_c_mode_slot(c, r, mp, c->mode[r->ch]);
        if (t > 0 && e->v[t]) return e->v[t];
    }
    return r->buttons ? r->buttons[i].value : e->v[0];
}
#define entry_value lj_ctl_c_entry_value

static int entry_at(const lj_ctl_c *c, const lj_ctl_row *r, int v)   /* the entry a channel value shows: its range, else the nearest value */
{
    int i, n = entries(c, r), best = -1, bestd = 0;
    for (i = 0; i < n; i++) {
        int d = entry_value(c, r, i) - v;
        if (r->buttons && lj_ctl_c_in(&r->buttons[i], v)) return i;
        if (d < 0) d = -d;
        if (best < 0 || d < bestd) { best = i; bestd = d; }
    }
    return best;
}

/* PROPOSAL (LORD_FIXTURES-9): a gobo / effect entry with neither bitmap nor colour shows one of our schematic tiles by slot order. */
static const char *const lj_ctl_c_gobo_stems[] = { "gobo_open", "gobo_cone", "gobo_holes", "gobo_lotus", "gobo_bricks", "gobo_clouds", "gobo_machine", "gobo_breakupbambus",
                                                   "gobo_breakupthreads", "gobo_pling", "gobo_triangle", "gobo_thinbar", "gobo_fan", "gobo_thinbars", "gobo_gridball", "gobo_3facet", "gobo_bells" };

static void entry_look(const lj_ctl_c *c, const lj_ctl_row *r, int i, const lj_surface **tile, uint32_t *rgb)
{
    const lj_pal_entry *e = &c->p->palette[pal_index(c, r->ch)].e[i];
    *tile = e->bitmap[0] ? lj_glyph_get(e->bitmap) : NULL;
    if (!*tile && e->bitmap[0]) {                              /* integrator: the hand files name tiles "Cone", the art stems are gobo_cone (art/glyphs) */
        char stem[40];
        size_t k;
        snprintf(stem, sizeof stem, "gobo_%s", e->bitmap);
        for (k = 5; stem[k]; k++) if (stem[k] >= 'A' && stem[k] <= 'Z') stem[k] = (char)(stem[k] - 'A' + 'a');
        *tile = lj_glyph_get(stem);
    }
    *rgb = e->used ? e->rgb & 0xFFFFFFu : LJ_CLR_BTNHIGHLIGHT;
    if (!*tile && !e->bitmap[0] && e->used && e->rgb == 0 && c->kind != LJ_CTL_COLORS)
        *tile = lj_glyph_get(lj_ctl_c_gobo_stems[i % LJ_COUNT_OF(lj_ctl_c_gobo_stems)]);
}

int lj_ctl_c_scroll_entry(lj_ctl_c *c, const lj_ctl_row *sr)
{
    uint8_t v = 0, st;
    if (!sr || lj_ctl_c_value(c, sr->ch, &v, &st) < 0) v = 0;
    return entry_at(c, sr, v);
}

int lj_ctl_c_scroll_value(lj_ctl_c *c, uint8_t ofs)
{
    const lj_ctl_row *r = row_of_kind(c, LJ_W_SCROLL, ofs);
    uint8_t v = 0, st;
    int i;
    if (!r) return 0;
    if (lj_ctl_c_value(c, ofs, &v, &st) < 0) v = 0;
    i = entry_at(c, r, v);
    return i < 0 ? 0 : entry_value(c, r, i);
}

/* ---- row builders of this file ---- */
static void osf_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y, int w)
{
    lj_widget *o = lj_w_osf_create(lj_window_root(c->win), NULL);
    unsigned allowed = r->states_allowed ? r->states_allowed : LJ_STATES_ALL;
    place(o, x, y, w, 32);
    lj_w_osf_set_allowed(o, allowed);
    lj_w_osf_set_state(o, r->dflt_state <= LJ_FADE ? r->dflt_state : LJ_OFF);
    lj_ctl_c_bind_add(c, o, r, LJ_CTL_C_OSF, -1, r->ch);
}

static int scroll_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    lj_widget *s = lj_w_scrollctl_create(lj_window_root(c->win), NULL);
    lj_w_scrollctl *sc = (lj_w_scrollctl *)s;
    int n = entries(c, r);
    place(s, x, y, LJ_SCROLLCTL_W, LJ_SCROLLCTL_H);
    sc->max = r->hi > r->lo ? r->hi : 255;
    sc->big = n >= 2 ? entry_value(c, r, 1) - entry_value(c, r, 0) : 16;   /* one whole position = the entry spacing (16 on the MAC 500 colour wheels) */
    if (sc->big < 0) sc->big = -sc->big;
    if (sc->big == 0) sc->big = 16;
    s->hint = r->caption;
    lj_ctl_c_bind_add(c, s, r, LJ_CTL_C_SCROLL, -1, r->ch);
    return LJ_SCROLLCTL_H;
}

int lj_ctl_c_rotation_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y, int w)
{
    lj_widget *root = lj_window_root(c->win), *panel = lj_w_panel_create(root, NULL), *b;
    lj_ctl_c_bind *bd;
    place(panel, x, y, w, 33);
    /* measured from colourrotate.gif 130x34: CCW at (3,4) 20x25, CW at (23,4), speed bar at (45,7) 80x20 */
    b = lj_ctl_c_btn(c, panel, 3, 4, 20, 25, NULL, "rotccw", LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW, 0);
    bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_ROT_CCW, 1, r->ch); if (bd) bd->btn = r->nbuttons > 1 ? &r->buttons[1] : NULL;
    b = lj_ctl_c_btn(c, panel, 23, 4, 20, 25, NULL, "rotcw", LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW, 0);
    bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_ROT_CW, 0, r->ch); if (bd) bd->btn = r->nbuttons > 0 ? &r->buttons[0] : NULL;
    b = lj_ctl_c_bar(c, panel, 45, 7, w - 49, 20, NULL, 255);
    b->hint = r->caption;
    lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_ROT_BAR, -1, r->ch);
    return 33;
}

static int palette_build(lj_ctl_c *c, const lj_ctl_row *r, int y)
{
    const lj_ctl_row *sr = row_of_kind(c, LJ_W_SCROLL, r->ch);
    int i, n = sr ? entries(c, sr) : entries(c, r), rows = (n + LJ_CTL_C_TILES_PER_ROW - 1) / LJ_CTL_C_TILES_PER_ROW;
    for (i = 0; i < n; i++) {
        lj_widget *t = lj_w_palette_create(lj_window_root(c->win), NULL);
        place(t, 1 + (i % LJ_CTL_C_TILES_PER_ROW) * LJ_CTL_C_TILE_PITCH, y + 1 + (i / LJ_CTL_C_TILES_PER_ROW) * LJ_CTL_C_TILE_PITCH, LJ_CTL_C_TILE, LJ_CTL_C_TILE);
        ((lj_w_palette *)t)->index = i;
        t->visible = c->show_palette;
        t->hint = sr && sr->buttons ? sr->buttons[i].caption : NULL;
        lj_ctl_c_bind_add(c, t, sr ? sr : r, LJ_CTL_C_TILE_ROLE, i, r->ch);
    }
    return rows > 0 ? rows * LJ_CTL_C_TILE_PITCH - 1 : 0;
}

static int note_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y)
{
    lj_widget *l = lj_ctl_c_btn(c, lj_window_root(c->win), x, y, 200, 16, r->caption, NULL, LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW, 1);
    l->enabled = 0;
    return 16;
}

static void specials_build(lj_ctl_c *c, int x0, int *w, int *h)
{
    lj_widget *root = lj_window_root(c->win);
    int j, k, y = 0, right = x0;
    for (j = 0; j < c->t->nspecials; j++) {
        const lj_ctl_row *r = &c->t->specials[j];
        int bx = x0 + 86;
        lj_widget *l = lj_ctl_c_btn(c, root, x0, y + 2, 84, 17, r->caption, NULL, LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW, 1);
        l->enabled = 0; l->visible = c->show_special;
        for (k = 0; k < r->nbuttons; k++) {                                  /* intensitycontrol.gif: flat black buttons, white text, 56x19 */
            lj_widget *b = lj_ctl_c_btn(c, root, bx, y + 1, 56, 19, r->buttons[k].caption, NULL, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNHIGHLIGHT, 0);
            lj_ctl_c_bind *bd = lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_SPECIAL_BTN, k, r->buttons[k].ch);
            if (bd) bd->btn = &r->buttons[k];
            b->visible = c->show_special;
            bx += 58;
        }
        if (r->caption2 && r->hi > r->lo) {                                   /* "<  Pulse Off  >": the speed bar of a pulse special; ponytail: percent caption, LJ2 prints the caption alone at rest */
            lj_widget *b = lj_ctl_c_bar(c, root, bx, y + 1, 90, 19, r->caption2, r->hi - r->lo);
            lj_ctl_c_bind_add(c, b, r, LJ_CTL_C_SPECIAL_BAR, -1, r->ch);
            b->visible = c->show_special;
            bx += 92;
        }
        if (bx > right) right = bx;
        y += LJ_CTL_C_BAR_LINE;
    }
    *w = right; *h = y;
}

/* ---- the layout pass: every row of the table becomes widgets of the root ---- */
static void free_children(lj_widget *parent)
{
    lj_widget *w = parent->first, *n;
    while (w) { n = w->next; free_children(w); lj_widget_remove(w); free(w); w = n; }
}

static void build(lj_ctl_c *c)
{
    lj_widget *root = lj_window_root(c->win), *w;
    const lj_ctl_window *t = c->t;
    int i, y = 0, line_y = 0, cur_x = 0, col = LJ_CTL_C_COL, prev = -1;
    int grp_x = 0, grp_y0 = 0, grp_y_end = 0, right = 0, osf_i = -1, osf_lines = 0, max_w = 0, sw = 0, sh = 0, cw, ch;
    lj_ui_event leave;
    memset(&leave, 0, sizeof leave); leave.type = LJ_EV_MOUSE_LEAVE; leave.u.mouse.type = LJ_EV_MOUSE_LEAVE;
    lj_window_event(c->win, &leave);                     /* the window forgets its hover widget before the tree goes */
    free_children(root);
    c->nb = 0;
    for (i = 0; i < t->nrows; i++) {
        const lj_ctl_row *r = &t->rows[i];
        int h = 0, attach = 0, x = col;
        if (r->kind == LJ_W_FADEBTN) {
            int osf_w = LJ_CTL_C_OSF_W;
            if (right) { if (grp_y_end > y) y = grp_y_end; right = 0; }
            if (i > 0 && i + 1 < t->nrows && t->rows[i + 1].kind == LJ_W_INDEXBAR) {   /* goboindex.gif: the Fade / index / rotation group sits right of the Snap / scroll / mode group */
                int gr = 0;
                for (w = root->first; w; w = w->next) if (w->y >= grp_y0 && w->x + w->w > gr) gr = w->x + w->w;
                grp_y_end = y; y = grp_y0; grp_x = gr + 4; osf_w = 24; right = 1;
            } else { grp_x = 0; grp_y0 = y; }
            col = grp_x + osf_w + 1;
            osf_build(c, r, grp_x, y + 1, osf_w);
            osf_i = c->nb - 1; osf_lines = r->rows ? r->rows : 1;
            prev = r->kind;
            continue;
        }
        switch (r->kind) {
        case LJ_W_SCROLL:      h = scroll_build(c, r, x, y); cur_x = x + LJ_SCROLLCTL_W + 1; break;
        case LJ_W_ROTATION: case LJ_W_WHEELSCROLL:
            if (prev == LJ_W_SCROLL || prev == LJ_W_EFFECT_BTN) { attach = 1; x = cur_x; cur_x = x + LJ_CTL_C_ROT_W + 1; }
            h = lj_ctl_c_rotation_build(c, r, x, attach ? line_y : y, LJ_CTL_C_ROT_W); break;
        case LJ_W_SHAKE:       if (prev == LJ_W_SCROLL || prev == LJ_W_WHEELSCROLL) { attach = 1; x = cur_x; cur_x = x + 122; } h = lj_ctl_c_shake_build(c, r, x, attach ? line_y : y); break;
        case LJ_W_PALETTE:     h = palette_build(c, r, y); break;
        case LJ_W_MODEPAIR:    h = lj_ctl_c_modepair_build(c, r, x, y); break;
        case LJ_W_INDEXBAR:    h = lj_ctl_c_indexbar_build(c, r, x, y); break;
        case LJ_W_DRAGBAR:     h = lj_ctl_c_dragbar_build(c, r, x, y); break;
        case LJ_W_EFFECT_BTN:  h = lj_ctl_c_effectbtn_build(c, r, x, y, &cur_x); break;
        case LJ_W_NOTE:        h = note_build(c, r, x, y); break;
        default: break;        /* shutter toggle, faders, lamp buttons: the controls-a/b windows */
        }
        for (w = root->first; w; w = w->next) if (w->x + w->w > max_w) max_w = w->x + w->w;
        if (attach) { prev = r->kind; continue; }
        line_y = y;
        if (h > 0) y += h + 1;
        if (osf_i >= 0 && --osf_lines <= 0) { lj_widget *o = c->b[osf_i].w; o->h = y - o->y - 1; osf_i = -1; }
        prev = r->kind;
    }
    if (right && grp_y_end > y) y = grp_y_end;
    if (osf_i >= 0) { lj_widget *o = c->b[osf_i].w; o->h = y - o->y - 1; }
    for (w = root->first; w; w = w->next) if (w->visible && w->x + w->w > max_w) max_w = w->x + w->w;
    c->main_w = max_w; c->main_h = y;
    cw = max_w; ch = y;
    if (t->nspecials > 0) {
        specials_build(c, max_w + LJ_CTL_C_SPECIAL_GAP, &sw, &sh);
        if (c->show_special) { if (sw > cw) cw = sw; if (sh > ch) ch = sh; }
    }
    if (!c->show_palette) {                                 /* hidden tiles leave their lines empty: shrink to what is visible */
        int bottom = 0;
        for (w = root->first; w; w = w->next) if (w->visible && w->y + w->h > bottom) bottom = w->y + w->h;
        if (bottom + 1 < ch) ch = bottom + 1;
    }
    lj_window_resize(c->win, cw, ch);
    lj_window_invalidate_all(c->win);
}

/* ---- events ---- */
static void rotation_emit(lj_ctl_c_bind *b, const lj_ctl_button *dir)
{
    lj_ctl_c *c = b->c;
    lj_ctl_c_bind *bar = NULL;
    int i, speed, span;
    for (i = 0; i < c->nb; i++) if (c->b[i].row == b->row && c->b[i].role == LJ_CTL_C_ROT_BAR) bar = &c->b[i];
    speed = bar ? lj_w_dragbar_value(bar->w) : 0;
    span = dir->hi - dir->lo;
    lj_ctl_c_emit(c, b->ch, dir->lo + (span > 0 ? (speed * span + 127) / 255 : 0));
}

static void on_ev(lj_widget *w, int ev, void *ud)
{
    lj_ctl_c_bind *b = (lj_ctl_c_bind *)ud;
    lj_ctl_c *c = b->c;
    uint8_t v = 0, st = LJ_OFF;
    switch (b->role) {
    case LJ_CTL_C_OSF:
        if (ev != LJ_WEV_CLICK) return;
        lj_prog_set_state_sel(b->ch, (uint8_t)lj_w_osf_state(w));
        return;
    case LJ_CTL_C_SCROLL:
        if (ev != LJ_WEV_CHANGE) return;
        lj_ctl_c_emit(c, b->ch, lj_w_scrollctl_value(w));
        return;
    case LJ_CTL_C_TILE_ROLE:
        if (ev != LJ_WEV_CLICK) return;
        lj_ctl_c_emit(c, b->ch, entry_value(c, b->row, b->entry));
        return;
    case LJ_CTL_C_ROT_CW: case LJ_CTL_C_ROT_CCW: {
        const lj_ctl_row *r = b->row;
        if (ev != LJ_WEV_CLICK || !b->btn) return;
        if (lj_ctl_c_value(c, b->ch, &v, &st) == 0 && lj_ctl_c_in(b->btn, v)) {   /* the active direction again: stop (RN 2.5), when the table has a stop value */
            if (r->nbuttons > 2) lj_ctl_c_emit(c, b->ch, r->buttons[2].value);
            return;
        }
        rotation_emit(b, b->btn);
        return; }
    case LJ_CTL_C_ROT_BAR: {
        const lj_ctl_row *r = b->row;
        if (ev != LJ_WEV_CHANGE || lj_ctl_c_value(c, b->ch, &v, &st) < 0) return;
        if (r->nbuttons > 0 && lj_ctl_c_in(&r->buttons[0], v)) rotation_emit(b, &r->buttons[0]);
        else if (r->nbuttons > 1 && lj_ctl_c_in(&r->buttons[1], v)) rotation_emit(b, &r->buttons[1]);
        return; }
    case LJ_CTL_C_SPECIAL_BTN:
        if (ev == LJ_WEV_CLICK && b->btn) lj_ctl_c_emit(c, b->btn->ch, b->btn->value);
        return;
    case LJ_CTL_C_SPECIAL_BAR:
        if (ev == LJ_WEV_CHANGE) lj_ctl_c_emit(c, b->ch, b->row->lo + lj_w_dragbar_value(w));
        return;
    default:
        if (lj_ctl_c_gobos_event(b, ev) || lj_ctl_c_beam_event(b, ev) || lj_ctl_c_effects_event(b, ev)) return;
        return;
    }
}

/* ---- refresh: every bound widget from the current scene ---- */
static void refresh_bind(lj_ctl_c_bind *b)
{
    lj_ctl_c *c = b->c;
    uint8_t v = 0, st = LJ_OFF;
    int have = lj_ctl_c_value(c, b->ch, &v, &st) == 0;
    switch (b->role) {
    case LJ_CTL_C_OSF: {                                      /* the scene channel's own state (ME-27); without a current sequence the button keeps what was clicked */
        int a = lj_prog_master_ch(b->ch);
        if (a >= 0 && lj_prog_get((uint16_t)a, &v, &st) == 0) lj_w_osf_set_state(b->w, st);
        break; }
    case LJ_CTL_C_SCROLL: {
        const lj_surface *tile = NULL; uint32_t rgb = LJ_CLR_BTNHIGHLIGHT;
        int e = entry_at(c, b->row, have ? v : 0);
        lj_w_scrollctl_set_value(b->w, have ? v : 0);
        if (e >= 0) entry_look(c, b->row, e, &tile, &rgb);
        lj_w_scrollctl_set_tile(b->w, tile, rgb);
        break; }
    case LJ_CTL_C_TILE_ROLE: {
        const lj_surface *tile = NULL; uint32_t rgb = LJ_CLR_BTNHIGHLIGHT;
        entry_look(c, b->row, b->entry, &tile, &rgb);
        lj_w_palette_set_tile(b->w, tile, rgb);
        lj_w_palette_select(b->w, entry_at(c, b->row, have ? v : 0) == b->entry);
        break; }
    case LJ_CTL_C_ROT_CW: case LJ_CTL_C_ROT_CCW:
        lj_ctl_c_btn_set_down(b->w, have && lj_ctl_c_in(b->btn, v));
        break;
    case LJ_CTL_C_ROT_BAR: {
        const lj_ctl_row *r = b->row;
        const lj_ctl_button *dir = NULL;
        if (have && r->nbuttons > 0 && lj_ctl_c_in(&r->buttons[0], v)) dir = &r->buttons[0];
        else if (have && r->nbuttons > 1 && lj_ctl_c_in(&r->buttons[1], v)) dir = &r->buttons[1];
        if (dir && dir->hi > dir->lo) lj_w_dragbar_set_value(b->w, ((v - dir->lo) * 255 + (dir->hi - dir->lo) / 2) / (dir->hi - dir->lo));
        break; }
    case LJ_CTL_C_SPECIAL_BTN: {
        uint8_t sv = 0, sst;
        lj_ctl_c_btn_set_down(b->w, b->btn && lj_ctl_c_value(c, b->btn->ch, &sv, &sst) == 0 && lj_ctl_c_in(b->btn, sv));
        break; }
    case LJ_CTL_C_SPECIAL_BAR:
        if (have && v >= b->row->lo && v <= b->row->hi) lj_w_dragbar_set_value(b->w, v - b->row->lo);
        break;
    default:
        if (lj_ctl_c_gobos_refresh(b) || lj_ctl_c_beam_refresh(b) || lj_ctl_c_effects_refresh(b)) return;
        break;
    }
}

void lj_ctl_c_refresh(lj_window *win)
{
    lj_ctl_c *c = lj_ctl_c_of(win);
    int i;
    if (!c) return;
    for (i = 0; i < c->nb; i++) refresh_bind(&c->b[i]);
}

/* ---- menu bar: UserPalette Preferences Fades Help, from the table's list (15/00 s2) ---- */
static void menu_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }   /* UserPalette / Fades / Help: W2 behaviour, the items are greyed */

static void toggle(lj_ctl_c *c, const char *item, int which)
{
    if (which) c->show_special ^= 1u; else c->show_palette ^= 1u;
    lj_menu_set_checked(lj_window_menus(c->win), item, which ? (int)c->show_special : (int)c->show_palette);
    build(c);
    lj_ctl_c_refresh(c->win);
}
static void show_palette_click(lj_widget *w, void *ud) { (void)w; toggle((lj_ctl_c *)ud, "M_ShowPalette", 0); }
static void show_special_click(lj_widget *w, void *ud) { (void)w; toggle((lj_ctl_c *)ud, "M_ShowSpecial", 1); }

static void menu_build(lj_ctl_c *c)
{
    static const char *const menu_std[] = { "UserPalette", "Preferences", "Fades", "Help", NULL };
    const char *const *names = c->t->menu ? c->t->menu : menu_std;
    int n = 0, nb = 0;
    for (; names[n] && n < LJ_COUNT_OF(c->items); n++) {
        lj_menu_item_desc *it = &c->items[n];
        memset(it, 0, sizeof *it);
        if (strcmp(names[n], "UserPalette") == 0) {
            it->name = "M_UserPalette"; it->caption = "&UserPalette"; it->on_click = "UserPaletteClick"; it->enabled = LJ_NO;
            c->binds[nb].event = "UserPaletteClick"; c->binds[nb].fn = menu_noop; c->binds[nb++].ud = c;
        } else if (strcmp(names[n], "Preferences") == 0) {
            it->name = "M_Preferences"; it->caption = "&Preferences"; it->children = c->prefs; it->nchildren = 2;
            c->prefs[0].name = "M_ShowPalette"; c->prefs[0].caption = "Show &Palette"; c->prefs[0].on_click = "ShowPaletteClick"; c->prefs[0].checked = LJ_YES;
            c->prefs[1].name = "M_ShowSpecial"; c->prefs[1].caption = "Show &Special"; c->prefs[1].on_click = "ShowSpecialClick";
            c->binds[nb].event = "ShowPaletteClick"; c->binds[nb].fn = show_palette_click; c->binds[nb++].ud = c;
            c->binds[nb].event = "ShowSpecialClick"; c->binds[nb].fn = show_special_click; c->binds[nb++].ud = c;
        } else if (strcmp(names[n], "Fades") == 0) {
            it->name = "M_Fades"; it->caption = "&Fades"; it->on_click = "FadesClick"; it->enabled = LJ_NO;
            c->binds[nb].event = "FadesClick"; c->binds[nb].fn = menu_noop; c->binds[nb++].ud = c;
        } else if (strcmp(names[n], "Help") == 0) {
            it->name = "M_Help"; it->caption = "&Help"; it->on_click = "HelpClick"; it->enabled = LJ_NO;
            c->binds[nb].event = "HelpClick"; c->binds[nb].fn = menu_noop; c->binds[nb++].ud = c;
        } else { it->name = names[n]; it->caption = names[n]; it->enabled = LJ_NO; }
    }
    c->menu.name = "MainMenu1"; c->menu.is_main = 1; c->menu.items = c->items; c->menu.n = n;
}

/* ---- create / destroy ---- */
lj_window *lj_ctl_c_create(enum lj_ctl_kind kind, const lj_profile *p)
{
    static const char *const names[LJ_CTL_COUNT] = { "IntensityControl", "MovementControl", "ColorControl", "GoboControl", "BeamControl", "EffectControl",
                                                     "LevelControl", "LampControl", "SpecialSettings", "GenericControl", "RGBControl" };
    static const char *const classes[LJ_CTL_COUNT] = { "TIntensityControl", "TMovementControl", "TColorControl", "TGoboControl", "TBeamControl", "TEffectControl",
                                                       "TLevelControl", "TLampControl", "TSpecialSettings", "TGenericControl", "TRGBControl" };
    const lj_ctl_window *t = lj_profile_window(p, kind);
    lj_ctl_c *c;
    int i;
    if (!t || kind >= LJ_CTL_COUNT) { lj_log("controls: no window of kind %d for %s", (int)kind, p ? p->name : "(no master)"); return NULL; }
    c = calloc(1, sizeof *c);
    if (!c) lj_fatal("controls: out of memory");
    c->kind = kind; c->p = p; c->t = t; c->show_palette = 1;
    menu_build(c);
    c->desc.name = names[kind]; c->desc.class_name = classes[kind]; c->desc.caption = t->caption;
    c->desc.client_width = 1; c->desc.client_height = 1; c->desc.text_height = 13;
    c->desc.color = LJ_CLR(LJ_CLR_BTNFACE); c->desc.font = LJ_FONT_SANS13; c->desc.font_name = "MS Sans Serif"; c->desc.font_height = -11;
    c->desc.border = LJ_BS_SINGLE; c->desc.icons = LJ_BI_GIVEN | LJ_BI_SYSMENU; c->desc.position = LJ_PO_DESIGNED;   /* system-menu-only small window (07 s9) */
    c->desc.menu = "MainMenu1"; c->desc.menus = &c->menu; c->desc.nmenus = 1;
    c->win = lj_window_create(&c->desc, c->binds);
    if (!c->win) { free(c); return NULL; }
    for (i = 0; i < LJ_COUNT_OF(lj_ctl_c_reg); i++) if (!lj_ctl_c_reg[i].win) { lj_ctl_c_reg[i].win = c->win; lj_ctl_c_reg[i].c = c; break; }
    if (i == LJ_COUNT_OF(lj_ctl_c_reg)) { lj_log("controls: window registry full"); lj_window_destroy(c->win); free(c); return NULL; }
    build(c);
    lj_ctl_c_refresh(c->win);
    return c->win;
}

void lj_ctl_c_destroy(lj_window *win)
{
    lj_ctl_c *c = lj_ctl_c_of(win);
    int i;
    if (!c) { lj_window_destroy(win); return; }
    for (i = 0; i < LJ_COUNT_OF(lj_ctl_c_reg); i++) if (lj_ctl_c_reg[i].win == win) { lj_ctl_c_reg[i].win = NULL; lj_ctl_c_reg[i].c = NULL; }
    lj_window_destroy(win);
    free(c);
}

lj_window *lj_ctl_colors_create(const lj_profile *p) { return lj_ctl_c_create(LJ_CTL_COLORS, p); }
void lj_ctl_colors_refresh(lj_window *win) { lj_ctl_c_refresh(win); }
void lj_ctl_colors_destroy(lj_window *win) { lj_ctl_c_destroy(win); }
