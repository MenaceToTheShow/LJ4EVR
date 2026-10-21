/* lj_glyphs.h - the named-art registry: toolbar / cue-grid / fixture-icon sheets
 * redrawn by us (D2, D13) and packed by the fonts track into lj_art_gen.c as
 * lj_art_table[]. A widget asks by name; NULL means "draw the name in a box".
 * lj_glyph_paint draws one glyph with the LORD_FIDELITY-15 disabled form (the
 * VCL single-glyph fallback: #808080 ink over a white copy at +1,+1). */
#ifndef LJ_GFX_GLYPHS_H
#define LJ_GFX_GLYPHS_H

#include "gfx/lj_surface.h"

typedef struct { const char *name; lj_surface img; } lj_art_entry;   /* img.px = the generated pixel rows (never written); bottom-left pixel is the transparency key */

extern const lj_art_entry lj_art_table[];      /* lj_art_gen.c */
extern const int          lj_art_count;

const lj_surface *lj_glyph_get(const char *name);   /* NULL when unknown */
int lj_glyph_paint(lj_surface *s, int x, int y, const char *name, int enabled);   /* blits the glyph (key = bottom-left pixel); enabled 0 = emboss; -1 when unknown or "missing" */

#endif /* LJ_GFX_GLYPHS_H */
