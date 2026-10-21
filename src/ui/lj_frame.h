/* lj_frame.h - the self-drawn Windows-classic window frame (design 7.3, 7.5).
 * Pure functions over lj_window_caption_state and a surface; lj_window.c owns
 * the state and calls these. Under native_frame the metrics are 0 and nothing
 * is painted. */
#ifndef LJ_UI_FRAME_H
#define LJ_UI_FRAME_H

#include "ui/lj_window.h"

/* Caption buttons, right to left as Windows lays them out. */
enum lj_frame_btn { LJ_FBTN_NONE = -1, LJ_FBTN_CLOSE = 0, LJ_FBTN_MAX, LJ_FBTN_MIN, LJ_FBTN_COUNT };

void   lj_frame_metrics(lj_window_caption_state *cs);                                 /* frame_w / caption_h from border + native_frame; menu_h untouched */
void   lj_frame_paint(lj_surface *s, const lj_window_caption_state *cs, int pressed);  /* frame + caption; pressed = enum lj_frame_btn drawn sunken */
lj_hit lj_frame_hit(const lj_window_caption_state *cs, int sw, int sh, int x, int y);  /* CLIENT for anything inside the client + menu band */
int    lj_frame_button_at(const lj_window_caption_state *cs, int sw, int x, int y);   /* enum lj_frame_btn under a surface point */
int    lj_frame_button_rect(const lj_window_caption_state *cs, int sw, int btn, lj_rect *out);   /* -1 when that button is not shown */

/* The classic system menu (Restore/Move/Size/Minimize/Maximize/Close); the
 * items' tag is the enum lj_sys_cmd. lj_window.c enables them per state. */
extern const lj_menu_desc lj_frame_sysmenu;

#endif /* LJ_UI_FRAME_H */
