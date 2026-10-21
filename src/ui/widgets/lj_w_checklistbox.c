/* lj_w_checklistbox.c - TCheckListBox: a TListBox whose rows carry the 13x13 sunken
 * check square (design 7.3); click on the square or Space toggles, OnClickCheck. */
#include "ui/widgets/lj_widgets_lists.h"

lj_widget *lj_w_checklistbox_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_widget *w = lj_w_listbox_create(parent, d);
    ((lj_w_list *)w)->checks_on = 1;
    return w;
}
