/* lj_dnd.h - internal drag and drop (design 7.9): a list widget calls
 * lj_dnd_begin on mouse-down; lj_window.c feeds the cursor while the button is
 * held and ends the drag on mouse-up; the widget under the cursor (in any of our
 * windows, found through the host's window_at) receives OnDragOver / OnDragDrop,
 * the source OnStartDrag / OnEndDrag. Payload = (kind, number): sequence,
 * cue, macro ... numbers, the LJ2 drag sources of [03]. */
#ifndef LJ_UI_DND_H
#define LJ_UI_DND_H

#include "ui/lj_window.h"

typedef struct lj_dnd { int active; int kind, number; lj_widget *source, *target; int x, y; } lj_dnd;   /* x,y relative to target during dispatch */

int  lj_dnd_begin(lj_widget *source, int kind, int number);   /* fires OnStartDrag; -1 when source is detached or a drag is already active in its window */
void lj_dnd_move(lj_window *src, int sx, int sy);             /* cursor at screen sx,sy: OnDragOver to the widget under it */
void lj_dnd_end(lj_window *src, int sx, int sy, int drop);    /* drop: OnDragDrop to the target; then OnEndDrag to the source */
int  lj_dnd_info(lj_window *w, int *kind, int *number, int *x, int *y);   /* what is being dragged / dropped at w, for handlers; -1 when nothing */
int  lj_dnd_active(lj_window *w);

#endif /* LJ_UI_DND_H */
