/* lj_glyphs.c - name -> art lookup over the generated const tables, the one glyph painter every button
 * uses (W0-glyphs gate, LORD_FIDELITY-15), the palette tile / gel look-up and the Source-Images cache
 * (design 7.8). The cache is the only mutable state; it is touched from the UI thread only. */
#include <stdio.h>
#include <string.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "gfx/lj_glyphs.h"

static const lj_surface *art(const char *name)
{
    int i;
    for (i = 0; i < lj_art_count; i++)          /* ponytail: linear strcmp; sort + bsearch if the table passes a few thousand rows */
        if (strcmp(lj_art_table[i].name, name) == 0) return &lj_art_table[i].img;
    return NULL;
}

/* ---- the cache: Source-Images decodes and built gel tiles; misses are cached too (paint runs every 100 ms) ---- */
enum { LJ_ART_CACHE = 1024, LJ_ART_KEY = 96 };
static struct { char key[LJ_ART_KEY]; int ok; lj_surface s; } lj_cache[LJ_ART_CACHE];
static int lj_ncache;
static lj_srcimg_loader lj_loader;

static int cache_find(const char *key)
{
    int i;
    for (i = 0; i < lj_ncache; i++) if (strcmp(lj_cache[i].key, key) == 0) return i;
    return -1;
}

static int cache_add(const char *key)            /* -1 when full: the caller falls back to our art (ponytail: no eviction, 1024 distinct names is a big show) */
{
    if (lj_ncache >= LJ_ART_CACHE) return -1;
    snprintf(lj_cache[lj_ncache].key, LJ_ART_KEY, "%s", key);
    lj_cache[lj_ncache].ok = 0;
    memset(&lj_cache[lj_ncache].s, 0, sizeof lj_cache[lj_ncache].s);
    return lj_ncache++;
}

void lj_srcimg_set_loader(lj_srcimg_loader fn)
{
    int i;
    for (i = 0; i < lj_ncache; i++) lj_surface_free(&lj_cache[i].s);
    lj_ncache = 0;
    lj_loader = fn;
}

static void lower(char *d, size_t cap, const char *s)
{
    size_t k = 0;
    for (; s && *s && k + 1 < cap; s++) d[k++] = (char)(*s >= 'A' && *s <= 'Z' ? *s - 'A' + 'a' : *s);
    d[k] = 0;
}

const lj_surface *lj_srcimg_get(const char *folder, const char *file, int cell_w, int cell_h)
{
    char key[LJ_ART_KEY], lf[64];
    lj_surface raw = { 0, 0, NULL };
    int i, j;
    if (!lj_loader || !folder || !file || !*file) return NULL;
    lower(lf, sizeof lf, file);
    snprintf(key, sizeof key, "%s/%s/%dx%d", folder, lf, cell_w, cell_h);
    if ((i = cache_find(key)) >= 0) return lj_cache[i].ok ? &lj_cache[i].s : NULL;
    if ((i = cache_add(key)) < 0) return NULL;
    if (lj_loader(folder, file, &raw) != 0 || !raw.px || raw.w < 1 || raw.h < 1) { lj_surface_free(&raw); return NULL; }
    if (cell_w > 0 && cell_h == raw.h && cell_w < raw.w) {            /* LJ2 strips: normal state is the left cell, the key stays bottom-left */
        if (lj_surface_init(&lj_cache[i].s, cell_w, cell_h) != 0) { lj_surface_free(&raw); return NULL; }
        for (j = 0; j < cell_h; j++) memcpy(lj_cache[i].s.px + (size_t)j * (size_t)cell_w, raw.px + (size_t)j * (size_t)raw.w, (size_t)cell_w * sizeof(uint32_t));
        lj_surface_free(&raw);
    } else {
        lj_cache[i].s = raw;
    }
    lj_cache[i].ok = 1;
    return &lj_cache[i].s;
}

const lj_surface *lj_glyph_get(const char *name)
{
    const lj_surface *g;
    int i;
    if (!name) return NULL;
    g = art(name);
    if (lj_loader)
        for (i = 0; i < lj_art_lj2_count; i++)
            if (strcmp(lj_art_lj2_table[i].stem, name) == 0) {
                const lj_surface *o = lj_srcimg_get(lj_art_lj2_table[i].folder, lj_art_lj2_table[i].file, g ? g->w : 0, g ? g->h : 0);
                return o ? o : g;
            }
    return g;
}

/* ---- palette tiles and gels ---- */
static void norm(char *d, size_t cap, const char *s)   /* lower-case alphanumerics: "Blue 101" = "blue101" = "BLUE-101" */
{
    size_t k = 0;
    for (; *s && k + 1 < cap; s++) {
        char ch = *s;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) d[k++] = ch;
    }
    d[k] = 0;
}

static const lj_surface *gel_tile(const lj_gel *g)
{
    char key[LJ_ART_KEY];
    lj_surface *s;
    int i, x, y;
    snprintf(key, sizeof key, "gel/%s", g->name);
    if ((i = cache_find(key)) >= 0) return lj_cache[i].ok ? &lj_cache[i].s : NULL;
    if ((i = cache_add(key)) < 0 || lj_surface_init(&lj_cache[i].s, LJ_TILE_W, LJ_TILE_H) != 0) return NULL;
    s = &lj_cache[i].s;
    if (g->text) {
        lj_fill(s, 0, 0, LJ_TILE_W, LJ_TILE_H, 0xFFFFFFu);
        lj_text(s, &lj_font_sans13, (LJ_TILE_W - lj_text_width(&lj_font_sans13, g->text)) / 2, (LJ_TILE_H - lj_font_sans13.height) / 2, g->text, 0x000000u, 0);
    } else {
        for (y = 0; y < LJ_TILE_H; y++)
            for (x = 0; x < LJ_TILE_W; x++)
                s->px[y * LJ_TILE_W + x] = g->n == 2 ? g->rgb[x * LJ_TILE_H + y * LJ_TILE_W < LJ_TILE_W * LJ_TILE_H ? 0 : 1]   /* diagonal halves, top-left first */
                                         : g->rgb[x * g->n / LJ_TILE_W];
    }
    lj_cache[i].ok = 1;
    return s;
}

static int gel_look(const char *key, const lj_surface **tile, uint32_t *rgb)
{
    char k[64];
    int i;
    for (i = 0; i < lj_gel_count; i++) {
        norm(k, sizeof k, lj_gel_table[i].name);
        if (strcmp(k, key) != 0) continue;
        if (lj_gel_table[i].n == 1 && !lj_gel_table[i].text) { *rgb = lj_gel_table[i].rgb[0]; return 1; }
        *tile = gel_tile(&lj_gel_table[i]);
        return *tile ? 1 : 0;
    }
    return 0;
}

int lj_palette_look(const char *name, int gobo_first, const lj_surface **tile, uint32_t *rgb)
{
    char key[48], stem[56], file[64];
    const char *folders[2];
    int k;
    if (!name || !tile || !rgb) return -1;
    *tile = NULL;
    if (!*name) return 0;
    if (strncmp(name, "gobo_", 5) == 0 && (*tile = art(name)) != NULL) return 1;   /* already one of our stems (the palette editors store them) */
    folders[0] = gobo_first ? "Gobos" : "Colors"; folders[1] = gobo_first ? "Colors" : "Gobos";
    snprintf(file, sizeof file, "%s.bmp", name);
    for (k = 0; k < 2; k++)
        if ((*tile = lj_srcimg_get(folders[k], file, 0, 0)) != NULL) return 1;
    norm(key, sizeof key, name);
    snprintf(stem, sizeof stem, "gobo_%s", key);
    if (gobo_first && (*tile = art(stem)) != NULL) return 1;
    if (gel_look(key, tile, rgb)) return 1;
    if ((*tile = art(stem)) != NULL) return 1;
    return 0;
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
