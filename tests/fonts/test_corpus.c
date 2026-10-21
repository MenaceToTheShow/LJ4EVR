/* test_corpus.c - the font width gate (design 7.4 step 5).
 * font_corpus: for every row of tests/fonts/widths.tsv whose font is packed,
 *              lj_text_width(font, caption) == the DFM Width (Delphi's DrawText extent). Expect 0 mismatches.
 *              Rows for faces that have no sheet yet are counted as skipped, not failed.
 * font_cells:  the harvested cell metrics of the five packed faces. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/lj_font.h"

extern const lj_font lj_font_arialblack18;      /* harvested cell is 18, not 16 (ME-18): lj_font.h still says lj_font_arialblack16 */

static const struct { const char *face; int height, style; const lj_font *font; const char *name; int cell, ascent, descent; } lj_faces[] = {
    { "MS Sans Serif", -11, 0, &lj_font_sans13,       "sans13",       13, 11, 2 },
    { "MS Sans Serif", -11, 1, &lj_font_sans13b,      "sans13b",      13, 11, 2 },
    { "Arial",         -11, 0, &lj_font_arial14,      "arial14",      14, 11, 3 },
    { "Arial",         -11, 1, &lj_font_arial14b,     "arial14b",     14, 11, 3 },
    { "Arial Black",   -13, 0, &lj_font_arialblack18, "arialblack18", 18, 14, 4 },
};
#define LJ_NFACES ((int)(sizeof lj_faces / sizeof lj_faces[0]))

int lj_test_font_corpus(int argc, char **argv);
int lj_test_font_corpus(int argc, char **argv)
{
    char line[512], *col[7];
    int rows = 0, skipped = 0, bad = 0, per_face[LJ_NFACES] = { 0 };
    FILE *f = fopen(LJ_SOURCE_DIR "/tests/fonts/widths.tsv", "rb");
    (void)argc; (void)argv;
    if (!f) { printf("FAIL cannot open widths.tsv\n"); return 1; }
    if (!fgets(line, sizeof line, f)) { fclose(f); return 1; }
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        int i, w;
        line[strcspn(line, "\r\n")] = 0;
        for (i = 0; i < 7; i++) {                        /* form object face height style caption width */
            col[i] = p;
            p = strchr(p, '\t');
            if (!p) break;
            *p++ = 0;
        }
        if (i != 6) { printf("FAIL malformed row: %s\n", line); bad++; continue; }
        rows++;
        for (i = 0; i < LJ_NFACES; i++)
            if (!strcmp(col[2], lj_faces[i].face) && atoi(col[3]) == lj_faces[i].height && (atoi(col[4]) & 3) == lj_faces[i].style) break;
        if (i == LJ_NFACES) { skipped++; continue; }
        w = lj_text_width(lj_faces[i].font, col[5][0] ? col[5] : " ");   /* Delphi measures an empty caption as one space */
        if (w != atoi(col[6])) {
            if (bad < 20) printf("MISMATCH %s.%s %s '%s' got %d want %s\n", col[0], col[1], lj_faces[i].name, col[5], w, col[6]);
            bad++; per_face[i]++;
        }
    }
    fclose(f);
    printf("font_corpus: %d rows, %d skipped (face not packed), %d mismatches\n", rows, skipped, bad);
    for (int i = 0; i < LJ_NFACES; i++) if (per_face[i]) printf("  %s: %d mismatches\n", lj_faces[i].name, per_face[i]);
    return bad ? 1 : 0;
}

int lj_test_font_cells(int argc, char **argv);
int lj_test_font_cells(int argc, char **argv)
{
    int i, bad = 0;
    (void)argc; (void)argv;
    for (i = 0; i < LJ_NFACES; i++) {
        const lj_font *f = lj_faces[i].font;
        if (f->height != lj_faces[i].cell || f->ascent != lj_faces[i].ascent || f->descent != lj_faces[i].descent) {
            printf("FAIL %s cell %d/%d/%d want %d/%d/%d\n", lj_faces[i].name, f->height, f->ascent, f->descent, lj_faces[i].cell, lj_faces[i].ascent, lj_faces[i].descent);
            bad++;
        }
        if (f->ascent + f->descent != f->height) { printf("FAIL %s ascent + descent != height\n", lj_faces[i].name); bad++; }
    }
    printf("font_cells: %d faces, %d failures\n", LJ_NFACES, bad);
    return bad ? 1 : 0;
}
