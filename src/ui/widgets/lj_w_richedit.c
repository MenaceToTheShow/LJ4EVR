/* lj_w_richedit.c - TRichEdit as a read-only plain-text memo (design 7.2, D2: no
 * RTF ships; imported notes arrive through the stripper of 5.3). Same widget as
 * TMemo; PlainText is implied. */
#include "ui/widgets/lj_widgets_lists.h"

lj_widget *lj_w_richedit_create(lj_widget *parent, const lj_ctl_desc *d) { return lj_w_memo_create(parent, d); }
