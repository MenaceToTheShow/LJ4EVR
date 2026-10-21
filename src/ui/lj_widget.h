/* lj_widget.h - the retained widget tree (design 2.7, 7.6).
 * One tree per window, built from a generated lj_form_desc in DFM order, which is
 * Delphi z-order (last declared paints on top and wins the hit-test). Knows
 * nothing about SDL or the engine. Coordinates are relative to the parent, as
 * the DFM gives them. Integer only.
 *
 * Widget classes (ui/widgets/lj_w_<class>.c) embed lj_widget as the FIRST member
 * of their own struct and fill vt; lj_widget.c only knows the base struct. */
#ifndef LJ_UI_WIDGET_H
#define LJ_UI_WIDGET_H

#include <stdint.h>
#include "gfx/lj_surface.h"
#include "gfx/lj_font.h"

typedef struct { int x, y, w, h; } lj_rect;

/* Align, the VCL values in resolution order: alTop, alBottom, alLeft, alRight, alClient. */
typedef enum { LJ_AL_NONE = 0, LJ_AL_TOP, LJ_AL_BOTTOM, LJ_AL_LEFT, LJ_AL_RIGHT, LJ_AL_CLIENT } lj_align;

/* Anchors, a set. 0 in a desc means "not given" = the VCL default [akLeft, akTop]. */
typedef unsigned lj_anchors;
#define LJ_AK_LEFT    1u
#define LJ_AK_TOP     2u
#define LJ_AK_RIGHT   4u
#define LJ_AK_BOTTOM  8u
#define LJ_AK_DEFAULT (LJ_AK_LEFT | LJ_AK_TOP)

/* Input events as the window hands them to a widget: mouse x,y are relative to
 * the receiving widget's origin. key is a Windows virtual-key code (the numbers
 * LJ2's .Hot files and hotkey tables carry: VK_F1 = 0x70, VK_RETURN = 0x0D ...);
 * ch is the CP1252 character for a text event, else 0. */
typedef struct { int type; /* LJ_EV_MOUSE_* */ int x, y; int button; /* 1 left, 2 right, 3 middle */ int clicks; /* 1 or 2 */ int wheel; /* +-notches, LJ_EV_MOUSE_WHEEL only */
                 unsigned shift:1, ctrl:1, alt:1; } lj_mouse_ev;
typedef struct { int type; /* LJ_EV_KEY_DOWN, LJ_EV_KEY_UP, LJ_EV_CHAR */ int key; int ch; unsigned shift:1, ctrl:1, alt:1; } lj_key_ev;

enum lj_ui_event_type {
    LJ_EV_MOUSE_DOWN, LJ_EV_MOUSE_UP, LJ_EV_MOUSE_MOVE, LJ_EV_MOUSE_WHEEL, LJ_EV_MOUSE_LEAVE,
    LJ_EV_KEY_DOWN, LJ_EV_KEY_UP, LJ_EV_CHAR,
    LJ_EV_TIMER,                     /* u.timer.id: 0 = the 100 ms repaint tick, else a TTimer row's tag */
    LJ_EV_FOCUS_IN, LJ_EV_FOCUS_OUT, /* window activation */
    LJ_EV_EXPOSE, LJ_EV_RESIZE,      /* u.size = new client size */
    LJ_EV_CLOSE
};
/* The event lj_window_event() takes: mouse x,y are window-client coordinates. */
typedef struct { int type; union { lj_mouse_ev mouse; lj_key_ev key; struct { int w, h; } size; struct { int id; } timer; } u; } lj_ui_event;

/* Widget-level events, the DFM "OnXxx" names (lj_wev_names[]); lj_window maps a
 * bound handler to (widget, event) and calls widget->on_event(w, ev, ud). */
enum lj_widget_event {
    LJ_WEV_CLICK, LJ_WEV_DBLCLICK, LJ_WEV_CHANGE, LJ_WEV_ENTER, LJ_WEV_EXIT,
    LJ_WEV_MOUSE_DOWN, LJ_WEV_MOUSE_UP, LJ_WEV_MOUSE_MOVE, LJ_WEV_MOUSE_WHEEL,
    LJ_WEV_KEY_DOWN, LJ_WEV_KEY_UP, LJ_WEV_KEY_PRESS,
    LJ_WEV_DRAG_OVER, LJ_WEV_DRAG_DROP, LJ_WEV_START_DRAG, LJ_WEV_END_DRAG,
    LJ_WEV_SELECT_ITEM, LJ_WEV_SELECT_CELL, LJ_WEV_DRAW_CELL, LJ_WEV_DRAW_ITEM, LJ_WEV_CUSTOM_DRAW_ITEM, LJ_WEV_CUSTOM_DRAW_SUBITEM,
    LJ_WEV_COLUMN_CLICK, LJ_WEV_COMPARE, LJ_WEV_RESIZE, LJ_WEV_PAINT, LJ_WEV_TIMER, LJ_WEV_POPUP, LJ_WEV_EXECUTE,
    LJ_WEV_CLICK_CHECK, LJ_WEV_CHANGING, LJ_WEV_MOVED, LJ_WEV_VALUE_CHANGE, LJ_WEV_DATA, LJ_WEV_DELETION,
    LJ_WEV_SECTION_RESIZE, LJ_WEV_GET_EDIT_MASK, LJ_WEV_DRAW_TAB, LJ_WEV_APPLY,
    /* form-level */
    LJ_WEV_CREATE, LJ_WEV_DESTROY, LJ_WEV_SHOW, LJ_WEV_HIDE, LJ_WEV_ACTIVATE, LJ_WEV_DEACTIVATE, LJ_WEV_CLOSE,
    LJ_WEV_COUNT
};
extern const char *const lj_wev_names[LJ_WEV_COUNT];   /* "OnClick", "OnDblClick", ... in enum order (lj_widget.c) */

typedef struct lj_widget lj_widget;
typedef struct {
    void (*paint)(lj_widget *w, lj_surface *s);         /* s is the window surface; paint at the widget's absolute origin (lj_widget_abs) */
    int  (*mouse)(lj_widget *w, const lj_mouse_ev *m);           /* m->type says down/up/move/wheel/leave; 1 = consumed */
    int  (*key)(lj_widget *w, const lj_key_ev *k);               /* k->type says key down/up/char; 1 = consumed */
    void (*layout)(lj_widget *w);                        /* after lj_widget_layout placed the children; NULL = nothing extra */
} lj_widget_vt;

struct lj_widget {
    const lj_widget_vt *vt;
    lj_widget *parent, *first, *next;                    /* children: singly linked in creation order (= z-order, last on top) */
    int x, y, w, h;                                      /* relative to parent */
    uint32_t color;                                      /* 0x00RRGGBB face colour */
    unsigned visible:1, enabled:1, tabstop:1, transparent:1, focused:1, dirty:1;
    int tab_order;
    int kind;                                            /* enum lj_ctl_row_kind (ui/forms/lj_form.h) */
    const char *name, *hint;                             /* point into the generated tables */
    lj_align align; lj_anchors anchors; const lj_font *font;
    const struct lj_ctl_desc *desc;                      /* the generated row this widget was built from, NULL for code-built widgets */
    void (*on_event)(lj_widget *w, int ev, void *ud);   /* ev = enum lj_widget_event; set by lj_window_create from the bindings */
    void *ud;
};

/* Tree ops. Every function tolerates NULL where it makes sense and never allocates. */
int        lj_widget_add(lj_widget *parent, lj_widget *child);          /* appends; -1 if child already has a parent or parent == child */
int        lj_widget_remove(lj_widget *child);                           /* unlinks from its parent; -1 if it has none */
lj_widget *lj_widget_find(lj_widget *root, const char *name);            /* depth-first by DFM name; NULL when absent */
void       lj_widget_abs(const lj_widget *w, int *x, int *y);            /* origin relative to the root */
void       lj_widget_invalidate(lj_widget *w);                           /* marks w (and so its covered children) for repaint */
int        lj_widget_collect_dirty(lj_widget *root, lj_rect *out, int cap);   /* absolute rects of dirty widgets, flags cleared; returns the count, which may exceed cap (then repaint everything) */
void       lj_widget_layout(lj_widget *parent);                          /* Align pass over the children as the VCL resolves it (alTop, alBottom, alLeft, alRight, alClient, creation order), then vt->layout, recursively */
void       lj_widget_resize(lj_widget *w, int new_w, int new_h);         /* applies Anchors to the children for the delta, then lj_widget_layout(w) */
lj_widget *lj_widget_hit(lj_widget *root, int x, int y);                 /* deepest visible widget under root-relative x,y; the last child declared wins; NULL outside root */
lj_widget *lj_widget_set_focus(lj_widget *w);                            /* clears the focused flag elsewhere in the tree, sets it on w when visible+enabled; returns the previously focused widget or NULL */
lj_widget *lj_widget_focused(lj_widget *root);                           /* the widget with focused set, or NULL */

#endif /* LJ_UI_WIDGET_H */
