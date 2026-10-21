/* lj_ctl_colors.h - the Colors window (design 6.4, 6.5, 7.2; [07 s3.3, s9.1]; 15/01 s5)
 * and the row engine the four controls-c windows share (Colors, Gobos, Beam,
 * Effects): a code-built window laid out line by line from the master profile's
 * lj_ctl_window table, every widget bound to a channel of the current scene
 * through lj_prog_* (seam 2, controls/lj_ctl.h). Per-kind row builders live in
 * lj_ctl_gobos.c (mode pair, index bar, shake), lj_ctl_beam.c (bar with end
 * icons), lj_ctl_effects.c (effect button); this file holds the wheel rows
 * (scroll control, rotation, palette tiles), the specials column, the menu bar
 * and the create / refresh / destroy entry points.
 *
 * Geometry (client px) measured from colorcontrol.gif, beam.gif, goboindex.gif,
 * effectcontrol4.gif (1:1 captures, note 07 s14): fade column 38 wide, content
 * column at x = 39, scroll line 34 tall, bar line 21, palette tiles 26x26 at a
 * 27-px pitch, 10 per row. The LJ2 captures carry 3-px frames and a 29-px
 * caption + menu band; the client is what is left (w - 6, h - 35).
 *
 * Table conventions this engine reads (PROPOSAL, for the fixlib hand files):
 *  - LJ_W_SCROLL: buttons[] = the wheel entries {caption, ch, value, lo, hi}
 *    (the exe gel / gobo names); lo..hi of the row = the continuous range the
 *    split buttons may reach; the k-th scroll row of a window shows
 *    profile->palette[base + k] (base 0 Colors, 2 Gobos, 4 Effects) for the
 *    tile look (rgb or a bitmap stem for lj_glyph_get). With buttons == NULL the
 *    palette's used entries (v[0]) are the list.
 *  - LJ_W_PALETTE: ch names the scroll row it belongs to; tiles = that row's entries.
 *  - LJ_W_ROTATION / LJ_W_WHEELSCROLL: buttons[] = {CW lo..hi}, {CCW lo..hi}
 *    [, {stop value}] (LORD_FIXTURES-10); the bar maps speed inside the pressed range.
 *  - LJ_W_MODEPAIR: buttons[] = the modes {caption, ch, type slot 0..4, lo, hi};
 *    while mode j is selected a scroll entry emits palette e[i].v[type] when set.
 *  - LJ_W_INDEXBAR / LJ_W_DRAGBAR: lo..hi -> the bar, reverse flips it; a
 *    DRAGBAR's items[] = {left glyph, right glyph} stems (Beam end icons).
 *  - LJ_W_EFFECT_BTN: buttons[0] = the "in" value and range, row lo = out;
 *    items[0] = the glyph stem on the button.
 *  - LJ_W_SHAKE: buttons[0] = the shake value and range; off re-emits the scroll entry.
 *  - LJ_W_SPECIAL_GROUP (specials[]): caption label + flat black buttons; shown by
 *    Preferences > Show Special (hidden by default, beam.gif / colorcontrol.gif). */
#ifndef LJ_CONTROLS_CTL_COLORS_H
#define LJ_CONTROLS_CTL_COLORS_H

#include <stdint.h>
#include "controls/lj_ctl.h"
#include "gfx/lj_surface.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_widget.h"
#include "ui/lj_window.h"

/* ---- the Colors window ---- */
lj_window *lj_ctl_colors_create(const lj_profile *p);   /* NULL + log when p has no Colors table */
void       lj_ctl_colors_refresh(lj_window *win);       /* repaint every bound widget from the current scene */
void       lj_ctl_colors_destroy(lj_window *win);

/* ---- the shared engine ---- */
enum { LJ_CTL_C_BINDS = 160, LJ_CTL_C_COL = 39, LJ_CTL_C_OSF_W = 38, LJ_CTL_C_SCROLL_LINE = 34, LJ_CTL_C_BAR_LINE = 21,
       LJ_CTL_C_TILE = 26, LJ_CTL_C_TILE_PITCH = 27, LJ_CTL_C_TILES_PER_ROW = 10, LJ_CTL_C_ROT_W = 129, LJ_CTL_C_SPECIAL_GAP = 4 };
enum lj_ctl_c_role { LJ_CTL_C_OSF = 0, LJ_CTL_C_SCROLL, LJ_CTL_C_TILE_ROLE, LJ_CTL_C_ROT_CCW, LJ_CTL_C_ROT_CW, LJ_CTL_C_ROT_BAR,
                     LJ_CTL_C_SPECIAL_BTN, LJ_CTL_C_SPECIAL_BAR, LJ_CTL_C_MODE, LJ_CTL_C_INDEX, LJ_CTL_C_SHAKE_BTN, LJ_CTL_C_SHAKE_BAR,
                     LJ_CTL_C_FXBTN, LJ_CTL_C_BEAM_BAR, LJ_CTL_C_PLAIN };

typedef struct lj_ctl_c lj_ctl_c;
typedef struct { lj_ctl_c *c; const lj_ctl_row *row; const lj_ctl_button *btn; lj_widget *w; int role, entry; uint8_t ch; } lj_ctl_c_bind;

struct lj_ctl_c {
    enum lj_ctl_kind kind; const lj_profile *p; const lj_ctl_window *t; lj_window *win;
    lj_form_desc desc; lj_menu_desc menu; lj_menu_item_desc items[4], prefs[2]; lj_binding binds[8];
    lj_ctl_c_bind b[LJ_CTL_C_BINDS]; int nb;
    uint8_t mode[LJ_PROFILE_CH];           /* LJ_W_MODEPAIR selection per channel offset */
    uint8_t shadow[LJ_PROFILE_CH], have[LJ_PROFILE_CH];   /* last emitted value per offset: what the widgets show while lj_prog_get has no current sequence */
    unsigned show_palette:1, show_special:1;
    int main_w, main_h;                     /* client size without the specials column */
};

lj_window *lj_ctl_c_create(enum lj_ctl_kind kind, const lj_profile *p);
void       lj_ctl_c_refresh(lj_window *win);
void       lj_ctl_c_destroy(lj_window *win);
lj_ctl_c  *lj_ctl_c_of(lj_window *win);                 /* the state behind a window this engine created, NULL otherwise */

/* for the per-kind row builders */
lj_ctl_c_bind *lj_ctl_c_bind_add(lj_ctl_c *c, lj_widget *w, const lj_ctl_row *row, int role, int entry, uint8_t ch);
lj_widget *lj_ctl_c_btn(lj_ctl_c *c, lj_widget *parent, int x, int y, int w, int h, const char *caption, const char *glyph, uint32_t face, uint32_t ink, int flat);
lj_widget *lj_ctl_c_bar(lj_ctl_c *c, lj_widget *parent, int x, int y, int w, int h, const char *caption, int max);   /* lj_w_dragbar, no event bound yet */
void       lj_ctl_c_btn_set_down(lj_widget *w, int down);
int        lj_ctl_c_btn_down(const lj_widget *w);
void       lj_ctl_c_btn_set_img(lj_widget *w, const lj_surface *img);
void       lj_ctl_c_btn_set_face(lj_widget *w, uint32_t face, uint32_t ink);
int        lj_ctl_c_value(lj_ctl_c *c, uint8_t ofs, uint8_t *v, uint8_t *st);   /* current scene value of the master's channel at ofs; shadow when there is no current sequence; -1 none */
void       lj_ctl_c_emit(lj_ctl_c *c, uint8_t ofs, int v);                      /* lj_prog_set_sel + shadow + refresh */
int        lj_ctl_c_in(const lj_ctl_button *b, int v);                          /* v inside b->lo..hi */
int        lj_ctl_c_scroll_value(lj_ctl_c *c, uint8_t ofs);                     /* the value of the current entry of the scroll row on ofs, 0 without one */
int        lj_ctl_c_scroll_entry(lj_ctl_c *c, const lj_ctl_row *sr);              /* the entry index the scroll row's channel shows (-1 without entries) */
int        lj_ctl_c_entry_value(const lj_ctl_c *c, const lj_ctl_row *sr, int i);  /* the DMX value entry i emits in the row's current mode */
const lj_ctl_row *lj_ctl_c_scroll_of_mode(const lj_ctl_c *c, const lj_ctl_row *mp);   /* the scroll row a MODEPAIR governs (the nearest above it) */
int        lj_ctl_c_mode_slot(const lj_ctl_c *c, const lj_ctl_row *sr, const lj_ctl_row *mp, int j);
int        lj_ctl_c_rotation_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y, int w);   /* CCW CW + speed bar in a raised panel (colourrotate.gif 130x34); returns 33 */

#endif /* LJ_CONTROLS_CTL_COLORS_H */
