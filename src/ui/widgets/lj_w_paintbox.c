/* lj_w_paintbox.c - TPaintBox: paints nothing itself, fires OnPaint so the bound
 * behaviour draws into its window's surface (lj_window_surface) at lj_widget_abs. */
#include "ui/widgets/lj_w_paintbox.h"
#include "ui/widgets/lj_widgets_basic.h"

static void paint(lj_widget *w, lj_surface *s) { (void)s; lj_w_emit(w, LJ_WEV_PAINT); }
static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL, NULL };

lj_widget *lj_w_paintbox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
