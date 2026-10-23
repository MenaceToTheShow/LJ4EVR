/* lj_glyphs.h - the named-art registry: toolbar / button glyphs, fixture-desktop icons, 31x30 gobo tiles
 * and gel swatches, all redrawn by us (D2, D13, ME-18) and packed by tools/artpack.py into lj_art_gen.c.
 * A widget asks by name; NULL means "draw the name in a box". lj_glyph_paint draws one glyph with the
 * LORD_FIDELITY-15 disabled form (the VCL single-glyph fallback: #808080 ink over a white copy at +1,+1).
 *
 * Source-Images override (design 7.8, W4-SRCIMG): platform/sdl registers a loader when the owner's
 * <userdata>/Source-Images/ exists; gfx then asks it by LJ2 folder + file name (art/lj2names.tsv for the
 * glyphs, the palette entry name for Colors / Gobos, the profile's bitmap name for Fixtures) and keeps what
 * it returns in a bounded cache. gfx has no decoder; without a loader, or when the loader fails, our art
 * is used. UI thread only. */
#ifndef LJ_GFX_GLYPHS_H
#define LJ_GFX_GLYPHS_H

#include "gfx/lj_surface.h"

typedef struct { const char *name; lj_surface img; } lj_art_entry;   /* img.px = the generated pixel rows (never written); bottom-left pixel is the transparency key */
typedef struct { const char *stem, *folder, *file; } lj_art_lj2;      /* our stem -> LJ2 Source-Images\<folder>\<file> (art/lj2names.tsv) */
typedef struct { const char *name, *text; int n; uint32_t rgb[4]; } lj_gel;   /* art/gels.txt: n colours (1 flat, 2 diagonal halves, 3-4 stripes) or a text tile */

extern const lj_art_entry lj_art_table[];      /* lj_art_gen.c */
extern const int          lj_art_count;
extern const lj_art_lj2   lj_art_lj2_table[];
extern const int          lj_art_lj2_count;
extern const lj_gel       lj_gel_table[];
extern const int          lj_gel_count;

enum { LJ_TILE_W = 31, LJ_TILE_H = 30 };       /* palette tiles (F17, note 07 s3.3) */

const lj_surface *lj_glyph_get(const char *name);   /* Source-Images substitute first, then our art; NULL when unknown */
int lj_glyph_paint(lj_surface *s, int x, int y, const char *name, int enabled);   /* blits the glyph (key = bottom-left pixel); enabled 0 = emboss; -1 when unknown or "missing" */

/* Palette look-up by LJ2 palette name (lj_pal_entry.bitmap): gobo_first = 1 for gobo / effect palettes.
 * Order: Source-Images (Colors\ then Gobos\, or the reverse), our gobo tile gobo_<name>, our gel (a flat
 * swatch goes to *rgb, a split / text gel to a cached tile). Names compare lower-case alphanumerics only.
 * Returns 1 resolved by art or a gel, 0 not known (*tile NULL, *rgb untouched); -1 on NULL arguments. */
int lj_palette_look(const char *name, int gobo_first, const lj_surface **tile, uint32_t *rgb);

typedef int (*lj_srcimg_loader)(const char *folder, const char *file, lj_surface *out);   /* 0 = decoded into out (gfx owns it from then), -1 = absent or malformed */
void lj_srcimg_set_loader(lj_srcimg_loader fn);     /* NULL = off; either way the cache is emptied */
const lj_surface *lj_srcimg_get(const char *folder, const char *file, int cell_w, int cell_h);   /* cached loader result; cell_w/h > 0 crops the left cell when the height matches (2- and 4-state strips); NULL when off, absent, or (with a cell) another height or narrower (LORD_C-11) */

#endif /* LJ_GFX_GLYPHS_H */
