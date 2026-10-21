/* lj_glyphs.c - name -> art lookup over the generated const table (no mutable state, design 2.0). */
#include <string.h>
#include "gfx/lj_glyphs.h"

const lj_surface *lj_glyph_get(const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = 0; i < lj_art_count; i++)          /* ponytail: linear strcmp; sort + bsearch if the table passes a few hundred rows */
        if (strcmp(lj_art_table[i].name, name) == 0) return &lj_art_table[i].img;
    return NULL;
}
