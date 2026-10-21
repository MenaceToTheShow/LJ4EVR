/* lj_w_image.c - TImage. No Martin art ships [D2]: without a picture the VCL paints
 * nothing, and so does this. desc->picture (a glyph name from tools/glyphmap.tsv),
 * Stretch and Center are the facts a later art pass draws from. */
#include "ui/widgets/lj_w_image.h"
#include "ui/widgets/lj_widgets_basic.h"

static void paint(lj_widget *w, lj_surface *s) { (void)w; (void)s; }
static int mouse(lj_widget *w, const lj_mouse_ev *m) { return lj_w_plain_mouse(w, m); }

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

lj_widget *lj_w_image_create(lj_widget *parent, const lj_ctl_desc *d)
{
    return lj_w_alloc(sizeof(lj_widget), parent, d, &vt, 0);
}
