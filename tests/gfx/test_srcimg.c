/* test_srcimg.c - W4-SRCIMG. In lj4evr_tests (no SDL): gfx's substitution with an in-memory loader (synthetic
 * strips, our pixels), the crop to the left cell, the bottom-left key, a failing loader and no loader = our art, and
 * (LORD_C-11) a BMP of another height or narrower than its cell = our art.
 * Built a second time with LJ_SRCIMG_SDL_TEST as lj4evr_srcimg_test (CMake, LJ_SDL): real BMP files written by
 * lj_bmp_write into a temp Source-Images tree, a truncated BMP, a '..' name and a user folder without Source-Images. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "gfx/lj_glyphs.h"

static int fake_calls;
static int fake_loader(const char *folder, const char *file, lj_surface *out)
{
    int x, y;
    fake_calls++;
    if (strcmp(folder, "Glyphs") == 0 && strcmp(file, "AddScene.Bmp") == 0) {      /* a 2-state 32x16 strip: left cell red, right cell blue, key #00FF00 */
        if (lj_surface_init(out, 32, 16) != 0) return -1;
        for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) out->px[y * 32 + x] = x < 16 ? 0xFF0000u : 0x0000FFu;
        out->px[15 * 32] = 0x00FF00u;
        return 0;
    }
    if (strcmp(folder, "Colors") == 0 && strcmp(file, "Blue 101.bmp") == 0) {
        if (lj_surface_init(out, 31, 30) != 0) return -1;
        for (x = 0; x < 31 * 30; x++) out->px[x] = 0x123456u;
        return 0;
    }
    if (strcmp(folder, "Glyphs") == 0 && (strcmp(file, "Cancel.bmp") == 0 || strcmp(file, "DeleteScene.bmp") == 0)) {   /* LORD_C-11: 18x20 for an 18x18 cell, 8x16 for a 16x16 one */
        int w = file[0] == 'C' ? 18 : 8, h = file[0] == 'C' ? 20 : 16;
        if (lj_surface_init(out, w, h) != 0) return -1;
        for (x = 0; x < w * h; x++) out->px[x] = 0xABCDEFu;
        return 0;
    }
    return -1;                                                                   /* absent / malformed */
}

int lj_test_srcimg(int argc, char **argv);
int lj_test_srcimg(int argc, char **argv)
{
    const lj_surface *ours = lj_glyph_get("add-scene"), *ins = lj_glyph_get("insert"), *can = lj_glyph_get("cancel"), *del = lj_glyph_get("delete"), *g, *t = NULL;
    uint32_t rgb = 0;
    int bad = 0, calls;
    (void)argc; (void)argv;
    lj_srcimg_set_loader(NULL);
    if (!ours || !ins || !can || !del) { printf("FAIL no loader: our art\n"); return 1; }
    lj_srcimg_set_loader(fake_loader);
    g = lj_glyph_get("add-scene");
    if (!g || g == ours || g->w != ours->w || g->h != 16 || g->px[0] != 0xFF0000u || g->px[15 * g->w] != 0x00FF00u) { printf("FAIL substitute: left 16x16 cell with the bottom-left key\n"); bad++; }
    calls = fake_calls;
    if (lj_glyph_get("add-scene") != g || fake_calls != calls) { printf("FAIL cache: a second paint must not reload\n"); bad++; }
    if (lj_glyph_get("insert") != ins) { printf("FAIL loader miss: our art\n"); bad++; }
    if (lj_glyph_get("missing")) { printf("FAIL 'missing' must stay NULL\n"); bad++; }
    if (lj_glyph_get("cancel") != can || lj_srcimg_get("Glyphs", "Cancel.bmp", 18, 18) != NULL) { printf("FAIL LORD_C-11: a BMP taller than its cell must give our art\n"); bad++; }
    if (lj_glyph_get("delete") != del || lj_srcimg_get("Glyphs", "DeleteScene.bmp", 16, 16) != NULL) { printf("FAIL LORD_C-11: a BMP narrower than its cell must give our art\n"); bad++; }
    if (lj_palette_look("Blue 101", 0, &t, &rgb) != 1 || !t || t->px[0] != 0x123456u) { printf("FAIL Colors\\Blue 101 substitute\n"); bad++; }
    if (lj_palette_look("Cone", 1, &t, &rgb) != 1 || t != lj_glyph_get("gobo_cone")) { printf("FAIL Gobos miss: our tile\n"); bad++; }
    lj_srcimg_set_loader(NULL);
    if (lj_glyph_get("add-scene") != ours) { printf("FAIL loader off: our art again\n"); bad++; }
    printf("srcimg: in-memory loader, %d loader calls, %d failures\n", fake_calls, bad);
    return bad ? 1 : 0;
}

#ifdef LJ_SRCIMG_SDL_TEST
#include <SDL3/SDL.h>
#include "gfx/lj_bmp.h"
#include "platform/sdl/lj_sdl_srcimg.h"

static void mkdirs(const char *root)
{
    static const char *const sub[] = { "", "/Source-Images", "/Source-Images/Glyphs", "/Source-Images/Gobos", "/empty" };
    char p[1100];
    int i;
    for (i = 0; i < 5; i++) { snprintf(p, sizeof p, "%s%s", root, sub[i]); SDL_CreateDirectory(p); }
}

static void truncate_to(const char *path, size_t keep)
{
    unsigned char head[64];
    size_t n = 0;
    FILE *f = fopen(path, "rb");
    if (f) { n = fread(head, 1, keep < sizeof head ? keep : sizeof head, f); fclose(f); }
    if ((f = fopen(path, "wb")) != NULL) { fwrite(head, 1, n, f); fclose(f); }
}

int main(int argc, char **argv)
{
    char path[1100];
    lj_surface strip;
    const lj_surface *ours, *g, *t = NULL;
    uint32_t rgb = 0;
    int bad = 0, x, y;
    if (argc < 2) { fprintf(stderr, "usage: lj4evr_srcimg_test <temp dir>\n"); return 2; }
    bad += lj_test_srcimg(0, NULL);
    mkdirs(argv[1]);
    ours = lj_glyph_get("add-scene");
    if (lj_surface_init(&strip, 32, 16) != 0) return 1;                          /* our synthetic pixels: a two-cell strip, key bottom-left */
    for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) strip.px[y * 32 + x] = x < 16 ? 0x112233u : 0x445566u;
    strip.px[15 * 32] = 0x00FF00u;
    snprintf(path, sizeof path, "%s/Source-Images/Glyphs/AddScene.Bmp", argv[1]); lj_bmp_write(&strip, path);
    snprintf(path, sizeof path, "%s/Source-Images/Glyphs/Ok.Bmp", argv[1]); lj_bmp_write(&strip, path); truncate_to(path, 40);
    snprintf(path, sizeof path, "%s/Source-Images/Gobos/..evil.bmp", argv[1]); lj_bmp_write(&strip, path);
    if (lj_sdl_srcimg_init(argv[1]) != 1) { printf("FAIL Source-Images folder not found\n"); bad++; }
    g = lj_glyph_get("add-scene");
    if (!g || g == ours || g->w != 16 || g->h != 16 || g->px[0] != 0x112233u || g->px[15 * 16] != 0x00FF00u) { printf("FAIL BMP substitute (left cell, key)\n"); bad++; }
    g = lj_glyph_get("ok");
    if (!g || g->w != 18 || g != lj_glyph_get("ok")) { printf("FAIL truncated BMP must fall back to our art\n"); bad++; }
    if (lj_palette_look("..evil", 1, &t, &rgb) != 0) { printf("FAIL a '..' name must not load\n"); bad++; }
    snprintf(path, sizeof path, "%s/empty", argv[1]);
    if (lj_sdl_srcimg_init(path) != 0 || lj_glyph_get("add-scene") != ours) { printf("FAIL no folder: our art\n"); bad++; }
    lj_surface_free(&strip);
    printf("srcimg_sdl: real BMPs, truncated BMP, '..' name, no folder: %d failures\n", bad);
    return bad ? 1 : 0;
}
#endif
