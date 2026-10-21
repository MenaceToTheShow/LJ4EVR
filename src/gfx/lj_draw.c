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

void lj_arrow(lj_surface *s, int cx, int cy, enum lj_dir d, int n, uint32_t rgb)
{
    int i;
    for (i = 0; i < n; i++) {
        int len = 2 * i + 1;
        switch (d) {
        case LJ_DIR_UP:    lj_hline(s, cx - i, cy + i, len, rgb); break;
        case LJ_DIR_DOWN:  lj_hline(s, cx - i, cy - i, len, rgb); break;
        case LJ_DIR_LEFT:  lj_vline(s, cx + i, cy - i, len, rgb); break;
        case LJ_DIR_RIGHT: lj_vline(s, cx - i, cy - i, len, rgb); break;
        }
    }
}

void lj_caption_bar(lj_surface *s, int x, int y, int w, int h, int active)
{
    uint32_t a = active ? LJ_CLR_HIGHLIGHT : LJ_CLR_CAPTION_INACTIVE;
    uint32_t b = active ? LJ_CLR_CAPTION_END : LJ_CLR_CAPTION_INACTIVE_END;
    int i, den = w > 1 ? w - 1 : 1;
    for (i = 0; i < w; i++) {
        uint32_t c = 0;
        int sh;
        for (sh = 0; sh <= 16; sh += 8) {
            int ca = (int)((a >> sh) & 0xFF), cb = (int)((b >> sh) & 0xFF);
            c |= (uint32_t)(ca + (cb - ca) * i / den) << sh;
        }
        lj_vline(s, x + i, y, h, c);
    }
}

/* Classic 16x14 caption button glyphs; the up-state origin is (x+4, y+3), pressed shifts +1,+1. */
static const char *const lj_cap_min[8]  = { "", "", "", "", "", "", "", "######" };
static const char *const lj_cap_max[8]  = { "#########", "#########", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", "#########" };
static const char *const lj_cap_rest[9] = { "..######.", "..######.", "..#....#.", "######.#.", "######.#.", "#....###.", "#....#...", "#....#...", "######..." };
static const char *const lj_cap_close[7] = { "##....##", ".##..##.", "..####..", "...##...", "..####..", ".##..##.", "##....##" };
static const char *const lj_cap_help[9]  = { ".####.", "##..##", "##..##", "....##", "...##.", "..##..", "..##..", "......", "..##.." };

void lj_caption_button(lj_surface *s, int x, int y, enum lj_cap_glyph g, int pressed)
{
    int o = pressed ? 1 : 0;
    if (!s || !s->px) return;
    if (pressed) lj_button_pressed(s, x, y, LJ_CAP_BTN_W, LJ_CAP_BTN_H);
    else lj_button_raised(s, x, y, LJ_CAP_BTN_W, LJ_CAP_BTN_H);
    x += 4 + o; y += 3 + o;
    switch (g) {
    case LJ_CAP_MINIMIZE: lj_mask(s, x, y, lj_cap_min, 8, LJ_CLR_3DDKSHADOW); break;
    case LJ_CAP_MAXIMIZE: lj_mask(s, x - 1, y - 1, lj_cap_max, 8, LJ_CLR_3DDKSHADOW); break;
    case LJ_CAP_RESTORE:  lj_mask(s, x - 1, y - 1, lj_cap_rest, 9, LJ_CLR_3DDKSHADOW); break;
    case LJ_CAP_CLOSE:    lj_mask(s, x, y, lj_cap_close, 7, LJ_CLR_3DDKSHADOW); break;
    case LJ_CAP_HELP:     lj_mask(s, x + 1, y - 1, lj_cap_help, 9, LJ_CLR_3DDKSHADOW); break;
    }
}

void lj_menu_bar(lj_surface *s, int x, int y, int w) { lj_fill(s, x, y, w, LJ_MENU_BAR_H, LJ_CLR_BTNFACE); }

int lj_menu_bar_item(lj_surface *s, const lj_font *f, int x, int y, const char *caption, int highlighted)
{
    int w = lj_text_width(f, caption) + 2 * LJ_MENU_PAD;
    if (highlighted) lj_fill(s, x, y + 1, w, LJ_MENU_BAR_H - 2, LJ_CLR_HIGHLIGHT);
    lj_text(s, f, x + LJ_MENU_PAD, y + 3, caption, highlighted ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW, 1);
    return w;
}

void lj_popup_frame(lj_surface *s, int x, int y, int w, int h)
{
    lj_fill(s, x, y, w, h, LJ_CLR_BTNFACE);
    lj_bevel1(s, x, y, w, h, LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW);
    lj_bevel1(s, x + 1, y + 1, w - 2, h - 2, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
}

void lj_menu_separator(lj_surface *s, int x, int y, int w)
{
    lj_hline(s, x, y + 3, w, LJ_CLR_BTNSHADOW);
    lj_hline(s, x, y + 4, w, LJ_CLR_BTNHIGHLIGHT);
}

void lj_menu_item(lj_surface *s, const lj_font *f, int x, int y, int w, const char *caption, const char *shortcut, int highlighted, int checked, int disabled)
{
    int ty;
    uint32_t ink = highlighted ? LJ_CLR_BTNHIGHLIGHT : LJ_CLR_3DDKSHADOW;
    if (!f) return;
    ty = y + (LJ_MENU_ITEM_H - f->height) / 2;
    if (highlighted) lj_fill(s, x, y, w, LJ_MENU_ITEM_H, LJ_CLR_HIGHLIGHT);
    if (disabled && highlighted) ink = LJ_CLR_BTNSHADOW;
    if (disabled && !highlighted) {
        lj_text_disabled(s, f, x + LJ_MENU_GUTTER, ty, caption, 1);
        if (shortcut) lj_text_disabled(s, f, x + w - LJ_MENU_GUTTER - lj_text_width(f, shortcut), ty, shortcut, 0);
    } else {
        lj_text(s, f, x + LJ_MENU_GUTTER, ty, caption, ink, 1);
        if (shortcut) lj_text(s, f, x + w - LJ_MENU_GUTTER - lj_text_width(f, shortcut), ty, shortcut, ink, 0);
    }
    if (checked) lj_check_glyph(s, x + 6, y + 5, ink);
}

void lj_scroll_arrow(lj_surface *s, int x, int y, int w, int h, enum lj_dir d, int pressed)
{
    int o = pressed ? 1 : 0, cx = x + w / 2 + o, cy = y + h / 2 + o;
    if (pressed) lj_button_pressed(s, x, y, w, h); else lj_button_raised(s, x, y, w, h);
    switch (d) {
    case LJ_DIR_UP:    cy -= 2; break;
    case LJ_DIR_DOWN:  cy += 1; break;
    case LJ_DIR_LEFT:  cx -= 2; break;
    case LJ_DIR_RIGHT: cx += 1; break;
    }
    lj_arrow(s, cx, cy, d, 4, LJ_CLR_3DDKSHADOW);
}

void lj_scroll_track(lj_surface *s, int x, int y, int w, int h) { lj_checker(s, x, y, w, h, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNFACE); }

void lj_spinner(lj_surface *s, int x, int y, int w, int h, int up_pressed, int down_pressed)
{
    int h1 = h / 2, uo = up_pressed ? 1 : 0, dn = down_pressed ? 1 : 0;   /* spike A updown(): two stacked halves */
    if (up_pressed) lj_button_pressed(s, x, y, w, h1); else lj_button_raised(s, x, y, w, h1);
    if (down_pressed) lj_button_pressed(s, x, y + h1, w, h - h1); else lj_button_raised(s, x, y + h1, w, h - h1);
    lj_arrow(s, x + w / 2 + uo, y + h1 / 2 - 1 + uo, LJ_DIR_UP, 3, LJ_CLR_3DDKSHADOW);
    lj_arrow(s, x + w / 2 + dn, y + h1 + (h - h1) / 2 + 1 + dn, LJ_DIR_DOWN, 3, LJ_CLR_3DDKSHADOW);
}
