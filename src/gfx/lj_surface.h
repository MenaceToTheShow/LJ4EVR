/* lj_surface.h - the 32-bit framebuffer every window paints into (design 2.6).
 * 0x00RRGGBB, row-major, w*h pixels, owned by the surface. No file decoding in
 * gfx: BMPs arrive from platform/ already decoded into an lj_surface. */
#ifndef LJ_GFX_SURFACE_H
#define LJ_GFX_SURFACE_H

#include <stdint.h>

typedef struct { int w, h; uint32_t *px; } lj_surface;      /* 0x00RRGGBB, row-major, owned */

int  lj_surface_init(lj_surface *s, int w, int h);          /* allocates w*h pixels, zero-filled; -1 on bad size; lj_fatal on malloc failure */
void lj_surface_free(lj_surface *s);                        /* frees px, zeroes the struct; safe on a zeroed struct */

#endif /* LJ_GFX_SURFACE_H */
