/* lj_font.h - the in-repo 1-bit fonts (design 2.6, 7.4).
 * Five faces packed by tools/fontpack.py from art/fonts/<face>.txt into lj_fonts_gen.c.
 * Metrics (cell height, ascent, advances) are harvested facts; the glyph pixels
 * are our own. CP1252 32..255 = 224 glyphs.
 *
 * Glyph storage: glyph g (= c - 32) is a standalone 1-bit bitmap of
 * advance[g] columns x height rows, rows packed MSB-first with stride
 * (advance[g] + 7) / 8 bytes, starting at bits + offset[g]. That is exactly the
 * layout lj_blit_1bit() takes, so lj_text() is one blit per character.
 * A control character (< 32) or a byte outside the table is drawn as glyph 0 (space). */
#ifndef LJ_GFX_FONT_H
#define LJ_GFX_FONT_H

#include <stdint.h>
#include "gfx/lj_surface.h"

#define LJ_FONT_FIRST  32
#define LJ_FONT_GLYPHS 224                  /* CP1252 32..255 */

typedef struct lj_font {
    int height, ascent, descent;            /* cell; baseline = y + ascent; 13 = 11 + 2 for sans13 (spike A, measured) */
    int overhang;                           /* tmOverhang: +1 per string on GDI-synthesised bold (sans13b); lj_text_width adds it, lj_text draws nothing extra */
    const uint8_t  *advance;                /* [224] advance width in px per glyph, == ink cell width */
    const uint16_t *offset;                 /* [224] byte offset of the glyph bitmap in bits */
    const uint8_t  *bits;                   /* all glyph bitmaps, each (advance+7)/8 * height bytes */
} lj_font;

extern const lj_font lj_font_sans13, lj_font_sans13b, lj_font_arial14, lj_font_arial14b, lj_font_arialblack18;

int  lj_text_width(const lj_font *f, const char *s);                                                  /* sum of advances; '&' accelerator prefixes count as in the VCL: "&x" is one glyph wide, "&&" one '&' */
void lj_text(lj_surface *s, const lj_font *f, int x, int y, const char *str, uint32_t rgb, int accel_underline);   /* y = top of the cell; accel_underline: "&x" underlines x at ascent+1, "&&" draws '&' */
void lj_text_disabled(lj_surface *s, const lj_font *f, int x, int y, const char *str, int accel_underline);   /* #808080 over a white copy at +1,+1 (classic DrawState) */

#endif /* LJ_GFX_FONT_H */
