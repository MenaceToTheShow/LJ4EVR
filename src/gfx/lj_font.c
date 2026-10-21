/* lj_font.c - 1-bit text (design 2.6, 7.4). One lj_blit_1bit per character. */
#include "gfx/lj_font.h"
#include "gfx/lj_draw.h"

/* Walks the string as the VCL does when accel is set: "&x" -> x underlined, "&&" -> '&',
 * a trailing '&' is dropped. Returns the glyph index (0..223), -1 at the end; *ul = underline this one. */
static int lj_next_glyph(const char **p, int accel, int *ul)
{
    unsigned char c = (unsigned char)**p;
    *ul = 0;
    if (!c) return -1;
    (*p)++;
    if (c == '&' && accel) {
        unsigned char n = (unsigned char)**p;
        if (!n) return -1;
        (*p)++;
        if (n != '&') *ul = 1;
        c = n;
    }
    return c < LJ_FONT_FIRST ? 0 : (int)c - LJ_FONT_FIRST;
}

int lj_text_width(const lj_font *f, const char *s)
{
    int w = 0, g, ul;
    if (!f || !s) return 0;
    while ((g = lj_next_glyph(&s, 1, &ul)) >= 0) w += f->advance[g];
    return w;
}

void lj_text(lj_surface *s, const lj_font *f, int x, int y, const char *str, uint32_t rgb, int accel_underline)
{
    int g, ul;
    if (!s || !f || !str) return;
    while ((g = lj_next_glyph(&str, accel_underline, &ul)) >= 0) {
        int adv = f->advance[g];
        lj_blit_1bit(s, x, y, f->bits + f->offset[g], adv, f->height, rgb);
        if (ul) lj_hline(s, x, y + f->ascent + 1, adv, rgb);
        x += adv;
    }
}

void lj_text_disabled(lj_surface *s, const lj_font *f, int x, int y, const char *str, int accel_underline)
{
    lj_text(s, f, x + 1, y + 1, str, LJ_CLR_BTNHIGHLIGHT, accel_underline);
    lj_text(s, f, x, y, str, LJ_CLR_BTNSHADOW, accel_underline);
}
