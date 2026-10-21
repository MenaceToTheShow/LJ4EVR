/* lj_focus.h - Delphi focus rules over the widget tree (design 7.9): the TabOrder
 * chain, Default / Cancel buttons, OnEnter / OnExit on focus moves. Window-free;
 * lj_window.c calls these from its key dispatch. The focus rectangle itself is
 * painted by each widget class from widget->focused. */
#ifndef LJ_UI_FOCUS_H
#define LJ_UI_FOCUS_H

#include "ui/lj_widget.h"

int        lj_focus_order(lj_widget *root, lj_widget **out, int cap);   /* the tab chain: visible+enabled+TabStop widgets, TabOrder within each container, containers recursed in place; returns the count, which may exceed cap */
lj_widget *lj_focus_next(lj_widget *root, lj_widget *from, int backward);   /* the chain neighbour of from (wraps); the first/last when from is not in it; NULL when the chain is empty */
lj_widget *lj_focus_default(lj_widget *root);                            /* the visible+enabled widget whose row says Default = True, or NULL */
lj_widget *lj_focus_cancel(lj_widget *root);                             /* ... Cancel = True */
lj_widget *lj_focus_set(lj_widget *w);                                   /* lj_widget_set_focus + OnExit on the old widget, OnEnter on w; returns the old one */
void       lj_focus_click(lj_widget *w);                                 /* fires OnClick on a visible+enabled widget (Enter on Default, Esc on Cancel) */

#endif /* LJ_UI_FOCUS_H */
