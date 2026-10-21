/* lj_widget_events.h - widget event ids the lists group needs that ui/lj_widget.h
 * does not define (brief: add here and NOTE it). Continues enum lj_widget_event. */
#ifndef LJ_UI_WIDGETS_WIDGET_EVENTS_H
#define LJ_UI_WIDGETS_WIDGET_EVENTS_H

#include "ui/lj_widget.h"

enum lj_widget_event_lists {
    LJ_WEV_SCROLL = LJ_WEV_COUNT,     /* TScrollBar.OnScroll */
    LJ_WEV_SECTION_CLICK,             /* THeaderControl.OnSectionClick */
    LJ_WEV_LISTS_COUNT
};
extern const char *const lj_wev_lists_names[LJ_WEV_LISTS_COUNT - LJ_WEV_COUNT];   /* "OnScroll", "OnSectionClick" (lj_widgets_lists.c) */

#endif /* LJ_UI_WIDGETS_WIDGET_EVENTS_H */
