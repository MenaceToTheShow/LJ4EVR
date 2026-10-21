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
