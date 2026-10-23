/* test_art.c - art_table: the packed art table (tools/artpack.py -> lj_art_gen.c) is sane:
 * every entry has a name, a 1..256 size, a key pixel (bottom-left) that is the classic
 * button face, the W0 toolbar glyphs exist at both sizes, and "missing" is NULL. */
#include <stdio.h>
#include <string.h>
#include "gfx/lj_glyphs.h"

int lj_test_art_table(int argc, char **argv);
int lj_test_art_table(int argc, char **argv)
{
    static const char *need[] = { "add-scene", "insert", "delete", "previous", "next", "first", "last", "play", "stop", "pause",
                                  "record", "up", "down", "left", "right", "check", "cross", "lamp", "reset", "blackout",
                                  "freeze", "online", "offline", "sync" };
    char name24[64];
    int i, bad = 0;
    (void)argc; (void)argv;
    if (lj_art_count < 1) { printf("FAIL empty art table\n"); return 1; }
    for (i = 0; i < lj_art_count; i++) {
        const lj_art_entry *e = &lj_art_table[i];
        if (!e->name[0] || e->img.w < 1 || e->img.w > 256 || e->img.h < 1 || e->img.h > 256 || !e->img.px) {
            printf("FAIL entry %d '%s' %dx%d\n", i, e->name, e->img.w, e->img.h); bad++; continue;
        }
        if (e->img.px[(e->img.h - 1) * e->img.w] != 0xC0C0C0u) { printf("FAIL '%s': bottom-left key is not #C0C0C0\n", e->name); bad++; }
        if (lj_glyph_get(e->name) != &e->img) { printf("FAIL lj_glyph_get('%s') does not find its row\n", e->name); bad++; }
    }
    for (i = 0; i < (int)(sizeof need / sizeof need[0]); i++) {
        const lj_surface *g = lj_glyph_get(need[i]), *g24;
        snprintf(name24, sizeof name24, "%s-24", need[i]);
        g24 = lj_glyph_get(name24);
        if (!g || g->w != 16 || g->h != 16) { printf("FAIL glyph '%s' missing or not 16x16\n", need[i]); bad++; }
        if (!g24 || g24->w != 24 || g24->h != 24) { printf("FAIL glyph '%s' missing or not 24x24\n", name24); bad++; }
    }
    if (lj_glyph_get("missing") || lj_glyph_get("") || lj_glyph_get(NULL)) { printf("FAIL 'missing' must be NULL\n"); bad++; }
    printf("art_table: %d entries, %d failures\n", lj_art_count, bad);
    return bad ? 1 : 0;
}

/* ---- W4 art track (appended): art_census, the W4-ART-1 / W4-ART-2 counts and the 0-unresolved rows ---- */
#include "desktop/lj_fixture_icons.h"
#include "fixlib/lj_profile.h"

int lj_test_art_census(int argc, char **argv);
int lj_test_art_census(int argc, char **argv)
{
    int i, j, k, bad = 0, glyph_rows = 0, icons = 0, tiles = 0, rows = 0, entries = 0, unresolved = 0, by_gel = 0, by_tile = 0;
    const lj_surface *t = NULL;
    uint32_t rgb = 0;
    (void)argc; (void)argv;
    for (i = 0; i < lj_art_lj2_count; i++) {                                   /* W4-ART-1: the 210 LJ2 Glyphs names, each drawn by us */
        if (strcmp(lj_art_lj2_table[i].folder, "Glyphs") != 0) continue;
        glyph_rows++;
        if (!lj_glyph_get(lj_art_lj2_table[i].stem)) { printf("FAIL LJ2 %s: stem '%s' not packed\n", lj_art_lj2_table[i].file, lj_art_lj2_table[i].stem); bad++; }
    }
    if (glyph_rows != 210) { printf("FAIL %d LJ2 Glyphs rows, want 210\n", glyph_rows); bad++; }
    for (i = 0; i < lj_art_count; i++) {
        if (lj_art_table[i].img.h == 50) icons++;                                /* art/icons: 50 px tall (design 7.8) */
        if (strncmp(lj_art_table[i].name, "gobo_", 5) == 0) {
            tiles++;
            if (lj_art_table[i].img.w != LJ_TILE_W || lj_art_table[i].img.h != LJ_TILE_H) { printf("FAIL tile %s is %dx%d\n", lj_art_table[i].name, lj_art_table[i].img.w, lj_art_table[i].img.h); bad++; }
        }
    }
    if (icons != 58) { printf("FAIL %d fixture icons, want 58\n", icons); bad++; }
    for (i = 0; i < lj_fixlib_count(); i++) {                                  /* W4-ART-2: every exe row (greyed ones too) and extra -> one of our icons */
        const lj_exe_row *r = lj_fixlib_row(i);
        const lj_profile *p = r ? lj_fixlib_by_id(r->id) : NULL;
        const lj_surface *s;
        if (!r) continue;
        s = lj_glyph_get(lj_fixture_icon_stem(r->name, p));
        rows++;
        if (!s || s->h != 50) { printf("FAIL profile %u '%s': icon '%s' unresolved\n", r->id, r->name ? r->name : "", lj_fixture_icon_stem(r->name, p)); bad++; unresolved++; }
        if (p && lj_fixture_icon_bitmap(p) != lj_glyph_get(lj_fixture_icon_stem(p->name, p))) { printf("FAIL profile %u: lj_fixture_icon_bitmap disagrees with the census\n", r->id); bad++; }
        if (!p) continue;
        for (j = 0; j < LJ_PALETTES; j++)                                      /* every named palette entry -> a tile or a gel */
            for (k = 0; k < LJ_PAL_ENTRIES; k++) {
                const lj_pal_entry *e = &p->palette[j].e[k];
                if (!e->used || !e->bitmap[0]) continue;
                entries++; t = NULL; rgb = 0x123456u;
                if (lj_palette_look(e->bitmap, j >= 2, &t, &rgb) != 1) { printf("FAIL profile %u palette %d '%s' unresolved\n", r->id, j, e->bitmap); bad++; unresolved++; }
                else if (t) by_tile++; else by_gel++;
            }
    }
    /* gel looks: a flat swatch, a split (top-left / bottom-right halves), a text tile; an unknown name stays unresolved */
    t = NULL; rgb = 0;
    if (lj_palette_look("Red 301", 0, &t, &rgb) != 1 || t || rgb == 0) { printf("FAIL flat gel 'Red 301'\n"); bad++; }
    if (lj_palette_look("red-green", 0, &t, &rgb) != 1 || !t || t->w != LJ_TILE_W || t->px[0] != 0xFF0000u || t->px[LJ_TILE_W * LJ_TILE_H - 1] != 0x00FF00u) { printf("FAIL split gel 'Red-Green'\n"); bad++; }
    if (lj_palette_look("RANDOM 3S", 0, &t, &rgb) != 1 || !t || t->px[0] != 0xFFFFFFu) { printf("FAIL text gel 'RANDOM 3S'\n"); bad++; }
    if (lj_palette_look("Cone", 1, &t, &rgb) != 1 || t != lj_glyph_get("gobo_cone")) { printf("FAIL gobo tile 'Cone'\n"); bad++; }
    if (lj_palette_look("no such gel", 0, &t, &rgb) != 0 || t) { printf("FAIL unknown name resolved\n"); bad++; }
    printf("art_census: %d LJ2 Glyphs names (210), %d icons (58), %d gobo tiles, %d gel rows; %d profile rows -> icons, %d named palette entries (%d tile, %d gel), %d unresolved, %d failures\n",
           glyph_rows, icons, tiles, lj_gel_count, rows, entries, by_tile, by_gel, unresolved, bad);
    return bad ? 1 : 0;
}
