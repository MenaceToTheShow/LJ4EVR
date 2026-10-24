/* lj_draw.h - the Windows-classic drawing primitives (design 2.6, rules in 7.3).
 * Every call clips to the surface. Colours are 0x00RRGGBB. Integer only. */
#ifndef LJ_GFX_DRAW_H
#define LJ_GFX_DRAW_H

#include <stdint.h>
#include "gfx/lj_surface.h"

/* The classic palette (design 7.3), Delphi $00BBGGRR converted. */
#define LJ_CLR_BTNFACE      0xC0C0C0u   /* clBtnFace / clSilver */
#define LJ_CLR_BTNHIGHLIGHT 0xFFFFFFu   /* clBtnHighlight / clWindow */
#define LJ_CLR_BTNSHADOW    0x808080u   /* clBtnShadow / clGray */
#define LJ_CLR_3DDKSHADOW   0x000000u   /* cl3DDkShadow / clWindowText */
#define LJ_CLR_3DLIGHT      0xDFDFDFu   /* cl3DLight: the caption buttons' inner top-left line, measured from movement2.gif (1:1, note 07 s14) */
#define LJ_CLR_HIGHLIGHT    0x000080u   /* clHighlight */
#define LJ_CLR_CAPTION_END  0x1084D0u   /* active caption gradient end */
#define LJ_CLR_RED          0xFF0000u
#define LJ_CLR_YELLOW       0xFFFF00u
#define LJ_CLR_GREEN        0x00FF00u

void lj_fill(lj_surface *s, int x, int y, int w, int h, uint32_t rgb);
void lj_hline(lj_surface *s, int x, int y, int w, uint32_t rgb);
void lj_vline(lj_surface *s, int x, int y, int h, uint32_t rgb);
void lj_bevel1(lj_surface *s, int x, int y, int w, int h, uint32_t tl, uint32_t br);   /* 1-px: top+left in tl, bottom+right in br */
void lj_button_raised(lj_surface *s, int x, int y, int w, int h);                       /* face + white/black outer, #808080 inner BR */
void lj_edge_sunken(lj_surface *s, int x, int y, int w, int h, uint32_t interior);      /* #808080/white outer, black/#C0C0C0 inner, filled interior */
void lj_focus_rect(lj_surface *s, int x, int y, int w, int h);                          /* 1-px dotted black (alternate pixels) */
void lj_blit_1bit(lj_surface *s, int x, int y, const uint8_t *bits, int w, int h, uint32_t rgb);   /* set bits painted rgb; row stride (w+7)/8 bytes, MSB first; clear bits untouched */
void lj_blit_rgb(lj_surface *s, int x, int y, const lj_surface *src, int transparent);           /* transparent: bottom-left pixel of src is the key [07 fact-check 16] */

/* --- the rest of the 7.3 classic rules. Integer, clipped. (Frame, menu, scrollbar and spinner painting live in ui/.) --- */
void lj_checker(lj_surface *s, int x, int y, int w, int h, uint32_t a, uint32_t b);  /* 50 % dither: a where (x+y) even, else b */
void lj_button_pressed(lj_surface *s, int x, int y, int w, int h);                  /* outer #808080 TL / white BR, inner black TL / #C0C0C0 BR, face */
void lj_button_latched(lj_surface *s, int x, int y, int w, int h);                  /* pressed edge, face = white/#C0C0C0 checkerboard (TSpeedButton Down+GroupIndex) */
void lj_bevel_etched(lj_surface *s, int x, int y, int w, int h);                    /* EDGE_ETCHED: #808080 inside white (TGroupBox) */
void lj_check_glyph(lj_surface *s, int x, int y, uint32_t rgb);                     /* the classic 7x7 check mark */
void lj_checkbox(lj_surface *s, int x, int y, int checked);                         /* 13x13 sunken white square + glyph at +3,+3 */
void lj_radio(lj_surface *s, int x, int y, int checked);                            /* 12x12 classic 4-colour ring, 4x4 dot */
void lj_scroll_track(lj_surface *s, int x, int y, int w, int h);                    /* white/#C0C0C0 dither */

#endif /* LJ_GFX_DRAW_H */
