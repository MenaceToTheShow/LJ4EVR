/* lj_w_drawgrid.c - TDrawGrid: the grid of lj_w_stringgrid.c without cell storage;
 * every visible cell fires OnDrawCell with lj_w_grid.draw_* set (design 7.2). */
#include "ui/widgets/lj_widgets_lists.h"

lj_widget *lj_w_drawgrid_create(lj_widget *parent, const lj_ctl_desc *d) { return lj_w_grid_create(parent, d, 0); }
