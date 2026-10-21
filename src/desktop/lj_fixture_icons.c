/* lj_fixture_icons.c - fixture icon tiles (lj_fixture_icons.h): paint through
 * ui/widgets/lj_w_fixicon.c, mouse through a desktop-owned vt (selection clicks with
 * Shift/Ctrl, move-mode drags, the icon popup), state from lj_selection + the scene. */
#include "desktop/lj_fixture_icons.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_selection.h"
#include "gfx/lj_glyphs.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_lj.h"

static lj_widget   *lj_area;
static lj_widget   *lj_tile[LJ_FIXTURES];
static lj_ctl_desc  lj_tile_desc[LJ_FIXTURES];          /* mutable: name / tag / caption per fixture */
static char         lj_tile_name[LJ_FIXTURES][8];
static lj_widget_vt lj_tile_vt; static void (*lj_tile_paint_orig)(lj_widget *, lj_surface *);
static int          lj_image_size;                      /* 0 Fixed Size (63x53), 1 Image Size */
static struct { int fixture, dx, dy, moved; } lj_drag = { -1, 0, 0, 0 };

const lj_surface *lj_fixture_icon_bitmap(const lj_profile *p)
{
    const lj_surface *s = lj_glyph_get(p && lj_profile_window(p, LJ_CTL_MOVEMENT) ? "movinghead" : "dimmer");
    return s;
}

int lj_desktop_icon_style(void) { return lj_image_size; }
void lj_desktop_icon_style_set(int image_size) { lj_image_size = image_size != 0; lj_desktop_icons_refresh(); }

/* Green = programmed in the current scene, cyan = in another scene of the sequence [01 s3.1]; footprint channels not LJ_OFF. */
int lj_desktop_icon_programmed(int fixture)
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_profile *p;
    int base, k, s, rc = LJ_FIX_PROG_NONE;
    if (!ctx->show || !ctx->show->current_seq || fixture < 0 || fixture >= LJ_FIXTURES) return rc;
    p = lj_fixlib_by_id(ctx->show->fixture[fixture].profile_id);
    base = lj_show_fixture_base(ctx->show, fixture);
    if (!p || base < 0) return rc;
    for (s = 0; s < ctx->show->current_seq->n; s++) {
        const lj_scene *sc = &ctx->show->current_seq->scenes[s];
        for (k = 0; k < p->footprint && base + k < LJ_CHANS; k++)
            if (sc->state[base + k] != LJ_OFF) {
                if (s == ctx->current_scene) return LJ_FIX_PROG_SCENE;
                rc = LJ_FIX_PROG_SEQ;
                break;
            }
    }
    return rc;
}

static int fixture_of(const lj_widget *w) { return w->desc ? w->desc->tag - 1 : -1; }

static int tile_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int i = fixture_of(w), ax, ay;
    lj_ctx *ctx = lj_ctx_get();
    if (i < 0 || !ctx->show) return 0;
    switch (m->type) {
    case LJ_EV_MOUSE_DOWN:
        if (m->button == 1) {
            if (lj_desktop_move_mode_on() || lj_desktop_icon_move_on(i)) { lj_drag.fixture = i; lj_drag.dx = m->x; lj_drag.dy = m->y; lj_drag.moved = 0; }   /* move mode: icons cannot be used for selection */
            else lj_sel_click(i, m->shift, m->ctrl);
        }
        return 1;
    case LJ_EV_MOUSE_MOVE:
        if (lj_drag.fixture == i) {
            lj_widget_abs(w, &ax, &ay);
            w->x += m->x - lj_drag.dx; w->y += m->y - lj_drag.dy;
            if (w->x < 0) w->x = 0;
            if (w->y < 0) w->y = 0;
            lj_drag.moved = 1;
            if (lj_area) lj_widget_invalidate(lj_area);
        }
        return 1;
    case LJ_EV_MOUSE_UP:
        if (m->button == 1 && lj_drag.fixture == i) {
            if (lj_drag.moved) { ctx->show->fixture[i].icon_x = (int16_t)w->x; ctx->show->fixture[i].icon_y = (int16_t)w->y; lj_desktop_config_save(); }
            lj_drag.fixture = -1;
        } else if (m->button == 2 && lj_w_inside(w, m->x, m->y)) {
            lj_widget_abs(w, &ax, &ay);
            lj_desktop_popup(LJ_POP_ICON, ax + m->x, ay + m->y, i);
        }
        return 1;
    default:
        return 0;
    }
}

void lj_fixture_icons_build(lj_widget *area)
{
    int i;
    lj_area = area;
    for (i = 0; i < LJ_FIXTURES; i++) {
        snprintf(lj_tile_name[i], sizeof lj_tile_name[i], "icon%d", i + 1);
        memset(&lj_tile_desc[i], 0, sizeof lj_tile_desc[i]);
        lj_tile_desc[i].kind = LJ_K_UNKNOWN; lj_tile_desc[i].name = lj_tile_name[i]; lj_tile_desc[i].tag = i + 1;
        lj_tile_desc[i].width = LJ_FIXICON_W; lj_tile_desc[i].height = LJ_FIXICON_H; lj_tile_desc[i].font = LJ_FONT_ARIAL14;
        lj_tile[i] = NULL;
    }
}

lj_widget *lj_fixture_icon_widget(int fixture) { return fixture >= 0 && fixture < LJ_FIXTURES ? lj_tile[fixture] : NULL; }

void lj_desktop_icons_refresh(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i, tab = lj_desktop_tab(), changed = 0;
    if (!lj_area) return;
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_fixture *f = ctx->show ? &ctx->show->fixture[i] : NULL;
        lj_widget *w = lj_tile[i];
        int show = f && f->link && f->placed && f->tab == tab, state, prog;
        const lj_surface *bmp;
        if (!show) { if (w && w->visible) { w->visible = 0; changed = 1; } continue; }
        if (!w) {
            lj_tile_desc[i].caption = f->user_name;
            w = lj_tile[i] = lj_w_fixicon_create(lj_area, &lj_tile_desc[i]);
            if (!lj_tile_paint_orig) { lj_tile_vt = *w->vt; lj_tile_paint_orig = w->vt->paint; lj_tile_vt.mouse = tile_mouse; }
            w->vt = &lj_tile_vt;
            changed = 1;
        }
        ((lj_w_fixicon *)w)->user_name = f->user_name;
        bmp = lj_fixture_icon_bitmap(lj_fixlib_by_id(f->profile_id));
        if (lj_drag.fixture != i) { w->x = f->icon_x; w->y = f->icon_y; }
        if (lj_image_size && bmp) { w->w = bmp->w + 6 > 40 ? bmp->w + 6 : 40; w->h = LJ_FIXICON_CAP_H + bmp->h + 5; }
        else { w->w = LJ_FIXICON_W; w->h = LJ_FIXICON_H; }
        state = lj_desktop_move_mode_on() || lj_desktop_icon_move_on(i) ? LJ_FIX_MOVE : lj_sel_master() == i ? LJ_FIX_MASTER : lj_sel_is_selected(i) ? LJ_FIX_SLAVE : LJ_FIX_UNSELECTED;
        prog = lj_desktop_show_programmed_on() ? lj_desktop_icon_programmed(i) : LJ_FIX_PROG_NONE;
        if (!w->visible) { w->visible = 1; changed = 1; }
        lj_w_fixicon_set(w, state, prog, bmp);
    }
    if (changed) lj_widget_invalidate(lj_area);
}

int lj_desktop_icon_at(int x, int y)
{
    int i;
    for (i = LJ_FIXTURES - 1; i >= 0; i--) {   /* last created paints on top */
        const lj_widget *w = lj_tile[i];
        if (w && w->visible && x >= w->x && y >= w->y && x < w->x + w->w && y < w->y + w->h) return i;
    }
    return -1;
}

void lj_desktop_icon_rect(int fixture, lj_rect *out)
{
    const lj_widget *w = fixture >= 0 && fixture < LJ_FIXTURES ? lj_tile[fixture] : NULL;
    memset(out, 0, sizeof *out);
    if (!w || !w->visible) return;
    out->x = w->x; out->y = w->y; out->w = w->w; out->h = w->h;
}
