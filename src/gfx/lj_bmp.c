/* lj_bmp.c - 24-bit bottom-up BMP writer (design 2.6). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC; core/lj_ini.c needs the same (CMake-wide define would be cleaner) */
#include <stdio.h>
#include "gfx/lj_bmp.h"
#include "core/lj_log.h"

static void lj_put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void lj_put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

int lj_bmp_write(const lj_surface *s, const char *path)
{
    uint8_t hdr[54] = { 'B', 'M' };
    uint8_t row[4 * 3 + 3];                      /* written in 4-pixel chunks + pad */
    uint32_t stride, size;
    FILE *fp;
    int y, x;

    if (!s || !s->px || s->w <= 0 || s->h <= 0 || !path) { lj_log("lj_bmp_write: bad surface"); return -1; }
    stride = ((uint32_t)s->w * 3 + 3) & ~3u;
    size = 54 + stride * (uint32_t)s->h;
    lj_put32(hdr + 2, size);
    lj_put32(hdr + 10, 54);
    lj_put32(hdr + 14, 40);                      /* BITMAPINFOHEADER */
    lj_put32(hdr + 18, (uint32_t)s->w);
    lj_put32(hdr + 22, (uint32_t)s->h);          /* positive = bottom-up */
    lj_put16(hdr + 26, 1);
    lj_put16(hdr + 28, 24);
    lj_put32(hdr + 34, stride * (uint32_t)s->h);
    lj_put32(hdr + 38, 2835); lj_put32(hdr + 42, 2835);   /* 72 dpi */

    fp = fopen(path, "wb");
    if (!fp) { lj_log("lj_bmp_write: cannot open %s", path); return -1; }
    if (fwrite(hdr, 1, 54, fp) != 54) goto fail;
    for (y = s->h - 1; y >= 0; y--) {
        const uint32_t *p = s->px + (size_t)y * (size_t)s->w;
        for (x = 0; x < s->w; x++) {
            uint8_t bgr[3] = { (uint8_t)p[x], (uint8_t)(p[x] >> 8), (uint8_t)(p[x] >> 16) };
            if (fwrite(bgr, 1, 3, fp) != 3) goto fail;
        }
        row[0] = row[1] = row[2] = 0;
        if (fwrite(row, 1, stride - (uint32_t)s->w * 3, fp) != stride - (uint32_t)s->w * 3) goto fail;
    }
    if (fclose(fp) != 0) { lj_log("lj_bmp_write: close failed on %s", path); return -1; }
    return 0;
fail:
    fclose(fp);
    lj_log("lj_bmp_write: write failed on %s", path);
    return -1;
}
