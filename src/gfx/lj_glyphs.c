/* lj_glyphs.c - name -> art lookup over the generated const table (no mutable state, design 2.0),
 * and the one glyph painter every button uses (W0-glyphs gate, LORD_FIDELITY-15). */
#include <string.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_glyphs.h"

const lj_surface *lj_glyph_get(const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = 0; i < lj_art_count; i++)          /* ponytail: linear strcmp; sort + bsearch if the table passes a few hundred rows */
        if (strcmp(lj_art_table[i].name, name) == 0) return &lj_art_table[i].img;
    return NULL;
}

/* Disabled form (DrawState DSS_DISABLED as the VCL paints a single-cell glyph): every
 * non-key pixel becomes #808080, with a white copy one pixel down-right underneath. */
static void emboss(lj_surface *s, int x, int y, const lj_surface *g, uint32_t rgb)
{
    uint32_t key = g->px[(g->h - 1) * g->w];
    int i, j;
    for (j = 0; j < g->h; j++)
        for (i = 0; i < g->w; i++)
            if (g->px[j * g->w + i] != key) lj_fill(s, x + i, y + j, 1, 1, rgb);
}

int lj_glyph_paint(lj_surface *s, int x, int y, const char *name, int enabled)
{
    const lj_surface *g = lj_glyph_get(name);
    if (!g || !g->px) return -1;
    if (enabled) { lj_blit_rgb(s, x, y, g, 1); return 0; }
    emboss(s, x + 1, y + 1, g, LJ_CLR_BTNHIGHLIGHT);
    emboss(s, x, y, g, LJ_CLR_BTNSHADOW);
    return 0;
}
