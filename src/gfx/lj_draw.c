/* lj_draw.c - the Windows-classic drawing primitives (design 2.6, rules 7.3).
 * Bevel geometry is spike A's measured bevel1/button_raised/sunken2; the glyph
 * masks below are our own pixel art of the Windows 95 stock glyphs (not Martin art). */
#include "gfx/lj_draw.h"

#include <stddef.h>

static int lj_clip(const lj_surface *s, int *x, int *y, int *w, int *h)
{
    if (!s || !s->px) return 0;
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > s->w) *w = s->w - *x;
    if (*y + *h > s->h) *h = s->h - *y;
    return *w > 0 && *h > 0;
}

static void lj_pixel(lj_surface *s, int x, int y, uint32_t rgb)
{
    if (x >= 0 && y >= 0 && x < s->w && y < s->h) s->px[(size_t)y * (size_t)s->w + (size_t)x] = rgb;
}

void lj_fill(lj_surface *s, int x, int y, int w, int h, uint32_t rgb)
{
    int i, j;
    if (!lj_clip(s, &x, &y, &w, &h)) return;
    for (j = 0; j < h; j++) {
        uint32_t *p = s->px + (size_t)(y + j) * (size_t)s->w + (size_t)x;
        for (i = 0; i < w; i++) p[i] = rgb;
    }
}

void lj_hline(lj_surface *s, int x, int y, int w, uint32_t rgb) { lj_fill(s, x, y, w, 1, rgb); }
void lj_vline(lj_surface *s, int x, int y, int h, uint32_t rgb) { lj_fill(s, x, y, 1, h, rgb); }

void lj_checker(lj_surface *s, int x, int y, int w, int h, uint32_t a, uint32_t b)
{
    int i, j;
    if (!lj_clip(s, &x, &y, &w, &h)) return;
    for (j = 0; j < h; j++) {
        uint32_t *p = s->px + (size_t)(y + j) * (size_t)s->w + (size_t)x;
        for (i = 0; i < w; i++) p[i] = ((x + i + y + j) & 1) ? b : a;
    }
}

void lj_bevel1(lj_surface *s, int x, int y, int w, int h, uint32_t tl, uint32_t br)
{
    if (w <= 0 || h <= 0) return;
    lj_hline(s, x, y, w - 1, tl); lj_vline(s, x, y, h - 1, tl);
    lj_hline(s, x, y + h - 1, w, br); lj_vline(s, x + w - 1, y, h, br);
}

void lj_button_raised(lj_surface *s, int x, int y, int w, int h)
{
    lj_fill(s, x, y, w, h, LJ_CLR_BTNFACE);
    lj_bevel1(s, x, y, w, h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_3DDKSHADOW);
    lj_hline(s, x + 1, y + h - 2, w - 2, LJ_CLR_BTNSHADOW); lj_vline(s, x + w - 2, y + 1, h - 2, LJ_CLR_BTNSHADOW);
}

static void lj_edge_pressed(lj_surface *s, int x, int y, int w, int h)
{
    lj_bevel1(s, x, y, w, h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
    lj_bevel1(s, x + 1, y + 1, w - 2, h - 2, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNFACE);
}

void lj_button_pressed(lj_surface *s, int x, int y, int w, int h)
{
    lj_fill(s, x, y, w, h, LJ_CLR_BTNFACE);
    lj_edge_pressed(s, x, y, w, h);
}

void lj_button_latched(lj_surface *s, int x, int y, int w, int h)
{
    lj_checker(s, x, y, w, h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNFACE);
    lj_edge_pressed(s, x, y, w, h);
}

void lj_edge_sunken(lj_surface *s, int x, int y, int w, int h, uint32_t interior)
{
    lj_fill(s, x, y, w, h, interior);
    lj_edge_pressed(s, x, y, w, h);
}

void lj_bevel_etched(lj_surface *s, int x, int y, int w, int h)
{
    lj_bevel1(s, x, y, w, h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
    lj_bevel1(s, x + 1, y + 1, w - 2, h - 2, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
}

void lj_focus_rect(lj_surface *s, int x, int y, int w, int h)
{
    int i;
    if (!s || !s->px || w <= 0 || h <= 0) return;
    for (i = 0; i < w; i++) {
        if (((x + i + y) & 1) == 0) lj_pixel(s, x + i, y, LJ_CLR_3DDKSHADOW);
        if (((x + i + y + h - 1) & 1) == 0) lj_pixel(s, x + i, y + h - 1, LJ_CLR_3DDKSHADOW);
    }
    for (i = 1; i < h - 1; i++) {
        if (((x + y + i) & 1) == 0) lj_pixel(s, x, y + i, LJ_CLR_3DDKSHADOW);
        if (((x + w - 1 + y + i) & 1) == 0) lj_pixel(s, x + w - 1, y + i, LJ_CLR_3DDKSHADOW);
    }
}

void lj_blit_1bit(lj_surface *s, int x, int y, const uint8_t *bits, int w, int h, uint32_t rgb)
{
    int i, j, stride;
    if (!s || !s->px || !bits || w <= 0 || h <= 0) return;
    stride = (w + 7) / 8;
    for (j = 0; j < h; j++) {
        const uint8_t *row = bits + (size_t)j * (size_t)stride;
        if (y + j < 0 || y + j >= s->h) continue;
        for (i = 0; i < w; i++)
            if (row[i >> 3] & (0x80u >> (i & 7))) lj_pixel(s, x + i, y + j, rgb);
    }
}

void lj_blit_rgb(lj_surface *s, int x, int y, const lj_surface *src, int transparent)
{
    int i, j;
    uint32_t key = 0;
    if (!s || !s->px || !src || !src->px || src->w <= 0 || src->h <= 0) return;
    if (transparent) key = src->px[(size_t)(src->h - 1) * (size_t)src->w];     /* bottom-left pixel [07 fact-check 16] */
    for (j = 0; j < src->h; j++) {
        const uint32_t *row = src->px + (size_t)j * (size_t)src->w;
        if (y + j < 0 || y + j >= s->h) continue;
        for (i = 0; i < src->w; i++)
            if (!transparent || row[i] != key) lj_pixel(s, x + i, y + j, row[i]);
    }
}

void lj_blit_rgb_fit(lj_surface *s, int x, int y, int w, int h, const lj_surface *src, int transparent)
{
    int i, j, dw, dh;
    uint32_t key = 0;
    if (!s || !s->px || !src || !src->px || src->w <= 0 || src->h <= 0 || w <= 0 || h <= 0) return;
    dw = src->w; dh = src->h;
    if (dw > w || dh > h) {                                      /* shrink to the tighter side: dw/dh = src w/h, integer */
        if (src->w * h > src->h * w) { dw = w; dh = src->h * w / src->w; }
        else { dh = h; dw = src->w * h / src->h; }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    }
    x += (w - dw) / 2; y += (h - dh) / 2;
    if (transparent) key = src->px[(size_t)(src->h - 1) * (size_t)src->w];
    for (j = 0; j < dh; j++) {
        const uint32_t *row = src->px + (size_t)(j * src->h / dh) * (size_t)src->w;
        for (i = 0; i < dw; i++) {
            uint32_t c = row[i * src->w / dw];
            if (!transparent || c != key) lj_pixel(s, x + i, y + j, c);
        }
    }
}

/* ---- masks: '#' = ink, drawn at (x, y) ------------------------------------------ */
static void lj_mask(lj_surface *s, int x, int y, const char *const *rows, int n, uint32_t rgb)
{
    int i, j;
    for (j = 0; j < n; j++)
        for (i = 0; rows[j][i]; i++)
            if (rows[j][i] == '#') lj_pixel(s, x + i, y + j, rgb);
}

static const char *const lj_check_mask[7] = {
    "......#", ".....##", "#...###", "##.###.", "#####..", ".###...", "..#...." };

void lj_check_glyph(lj_surface *s, int x, int y, uint32_t rgb) { if (s && s->px) lj_mask(s, x, y, lj_check_mask, 7, rgb); }

void lj_checkbox(lj_surface *s, int x, int y, int checked)
{
    lj_edge_sunken(s, x, y, 13, 13, LJ_CLR_BTNHIGHLIGHT);
    if (checked) lj_check_glyph(s, x + 3, y + 3, LJ_CLR_3DDKSHADOW);
}

/* o = outer ring (#808080 TL / white BR), i = inner ring (black TL / #C0C0C0 BR), w = white interior */
static const char *const lj_radio_mask[12] = {
    "....oooo....", "..ooiiiioo..", ".oiiwwwwiio.", ".oiwwwwwwio.", "oiwwwwwwwwio", "oiwwwwwwwwio",
    "oiwwwwwwwwio", "oiwwwwwwwwio", ".oiwwwwwwio.", ".oiiwwwwiio.", "..ooiiiioo..", "....oooo...." };

void lj_radio(lj_surface *s, int x, int y, int checked)
{
    int i, j;
    if (!s || !s->px) return;
    for (j = 0; j < 12; j++)
        for (i = 0; i < 12; i++) {
            int tl = i + j < 11;
            switch (lj_radio_mask[j][i]) {
            case 'o': lj_pixel(s, x + i, y + j, tl ? LJ_CLR_BTNSHADOW : LJ_CLR_BTNHIGHLIGHT); break;
            case 'i': lj_pixel(s, x + i, y + j, tl ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNFACE); break;
            case 'w': lj_pixel(s, x + i, y + j, LJ_CLR_BTNHIGHLIGHT); break;
            default: break;
            }
        }
    if (checked) lj_fill(s, x + 4, y + 4, 4, 4, LJ_CLR_3DDKSHADOW);
}

void lj_scroll_track(lj_surface *s, int x, int y, int w, int h) { lj_checker(s, x, y, w, h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNFACE); }
