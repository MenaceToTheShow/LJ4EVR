/* lj_surface.c - the 32-bit framebuffer (design 2.6). */
#include <stdlib.h>
#include <string.h>
#include "gfx/lj_surface.h"
#include "core/lj_types.h"

int lj_surface_init(lj_surface *s, int w, int h)
{
    if (!s) return -1;
    s->w = s->h = 0; s->px = NULL;
    if (w <= 0 || h <= 0 || w > 32768 || h > 32768) return -1;
    s->px = calloc((size_t)w * (size_t)h, sizeof *s->px);
    if (!s->px) lj_fatal("lj_surface_init: %dx%d out of memory", w, h);
    s->w = w; s->h = h;
    return 0;
}

void lj_surface_free(lj_surface *s)
{
    if (!s) return;
    free(s->px);
    memset(s, 0, sizeof *s);
}
