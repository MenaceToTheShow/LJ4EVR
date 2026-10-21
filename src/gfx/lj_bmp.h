/* lj_bmp.h - 24-bit BMP writer for lj4evr_render, --save and the golden tests.
 * gfx has no reader: BMPs are decoded by platform/ (SDL_LoadBMP). */
#ifndef LJ_GFX_BMP_H
#define LJ_GFX_BMP_H

#include "gfx/lj_surface.h"

int lj_bmp_write(const lj_surface *s, const char *path);   /* BITMAPINFOHEADER, 24 bpp, bottom-up, rows padded to 4; 0 / -1 + log */

#endif /* LJ_GFX_BMP_H */
