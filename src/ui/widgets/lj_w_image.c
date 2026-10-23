/* lj_w_image.c - TImage. desc->picture names our own art (tools/glyphmap.tsv; no Martin art ships, D2), drawn
 * through lj_glyph_get so the Source-Images override applies (design 7.8). Center centres it in the control;
 * Stretch scales it nearest-neighbour in integers to the control; Transparent keys the bottom-left pixel, as the
 * glyphs do (ME-135). No picture paints nothing, as the VCL does. */
#include "ui/widgets/lj_w_image.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_glyphs.h"

static void paint(lj_widget *w, lj_surface *s)
{
    const lj_surface *g = w->desc && w->desc->picture ? lj_glyph_get(w->desc->picture) : NULL;
    int x, y, i, j, key_on;
    uint32_t key;
    if (!g || !g->px || g->w < 1 || g->h < 1 || w->w < 1 || w->h < 1) return;
    lj_widget_abs(w, &x, &y);
    key_on = w->desc->transparent == LJ_YES;
    if (w->desc->stretch == LJ_YES && (g->w != w->w || g->h != w->h)) {
        key = g->px[(g->h - 1) * g->w];
        for (j = 0; j < w->h; j++)
            for (i = 0; i < w->w; i++) {
                uint32_t c = g->px[(j * g->h / w->h) * g->w + i * g->w / w->w];
                if (!key_on || c != key) lj_fill(s, x + i, y + j, 1, 1, c);
            }
        return;
    }
    if (w->desc->center == LJ_YES) { x += (w->w - g->w) / 2; y += (w->h - g->h) / 2; }
    lj_blit_rgb(s, x, y, g, key_on);
}
static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_image_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
