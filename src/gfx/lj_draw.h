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

/* --- W0 gfx track additions: the rest of the 7.3 classic rules. Integer, clipped, no text unless a font is passed. --- */
#include "gfx/lj_font.h"

#define LJ_CLR_CAPTION_INACTIVE      0x808080u   /* inactive caption gradient start */
#define LJ_CLR_CAPTION_INACTIVE_END  0xB5B5B5u

enum lj_dir { LJ_DIR_LEFT, LJ_DIR_RIGHT, LJ_DIR_UP, LJ_DIR_DOWN };
enum lj_cap_glyph { LJ_CAP_MINIMIZE, LJ_CAP_MAXIMIZE, LJ_CAP_RESTORE, LJ_CAP_CLOSE, LJ_CAP_HELP };

#define LJ_MENU_BAR_H     19    /* SM_CYMENU */
#define LJ_MENU_ITEM_H    17
#define LJ_MENU_SEP_H      9
#define LJ_MENU_GUTTER    20    /* check-mark column; text starts at x + gutter */
#define LJ_MENU_PAD        7    /* menu-bar item padding each side */
#define LJ_CAPTION_H      18    /* + 1-px border = SM_CYCAPTION 19 */
#define LJ_SMCAPTION_H    14    /* tool window */
#define LJ_CAP_BTN_W      16
#define LJ_CAP_BTN_H      14

void lj_checker(lj_surface *s, int x, int y, int w, int h, uint32_t a, uint32_t b);  /* 50 % dither: a where (x+y) even, else b */
void lj_button_pressed(lj_surface *s, int x, int y, int w, int h);                  /* outer #808080 TL / white BR, inner black TL / #C0C0C0 BR, face */
void lj_button_latched(lj_surface *s, int x, int y, int w, int h);                  /* pressed edge, face = white/#C0C0C0 checkerboard (TSpeedButton Down+GroupIndex) */
void lj_bevel_etched(lj_surface *s, int x, int y, int w, int h);                    /* EDGE_ETCHED: #808080 inside white (TGroupBox) */
void lj_check_glyph(lj_surface *s, int x, int y, uint32_t rgb);                     /* the classic 7x7 check mark */
void lj_checkbox(lj_surface *s, int x, int y, int checked);                         /* 13x13 sunken white square + glyph at +3,+3 */
void lj_radio(lj_surface *s, int x, int y, int checked);                            /* 12x12 classic 4-colour ring, 4x4 dot */
void lj_arrow(lj_surface *s, int cx, int cy, enum lj_dir d, int n, uint32_t rgb);   /* solid triangle, n rows of 1,3,..,2n-1 px; (cx,cy) = the tip */
void lj_caption_bar(lj_surface *s, int x, int y, int w, int h, int active);         /* horizontal gradient #000080->#1084D0, inactive #808080->#B5B5B5 */
void lj_caption_button(lj_surface *s, int x, int y, enum lj_cap_glyph g, int pressed);  /* 16x14 raised/pressed with the classic glyph */
void lj_menu_bar(lj_surface *s, int x, int y, int w);                               /* 19-px face row */
int  lj_menu_bar_item(lj_surface *s, const lj_font *f, int x, int y, const char *caption, int highlighted);   /* returns item width = text + 2*pad; text at +7,+3 */
void lj_popup_frame(lj_surface *s, int x, int y, int w, int h);                     /* face fill + 3-px raised border (EDGE_RAISED + 1 px face) */
void lj_menu_separator(lj_surface *s, int x, int y, int w);                         /* 9-px row, #808080 over white at +3/+4 */
void lj_menu_item(lj_surface *s, const lj_font *f, int x, int y, int w, const char *caption, const char *shortcut, int highlighted, int checked, int disabled);  /* 17-px popup row */
void lj_scroll_arrow(lj_surface *s, int x, int y, int w, int h, enum lj_dir d, int pressed);   /* raised button + 4-row black triangle */
void lj_scroll_track(lj_surface *s, int x, int y, int w, int h);                    /* white/#C0C0C0 dither */
void lj_spinner(lj_surface *s, int x, int y, int w, int h, int up_pressed, int down_pressed);  /* TUdButton: two stacked buttons, 3-row triangles */

#endif /* LJ_GFX_DRAW_H */
