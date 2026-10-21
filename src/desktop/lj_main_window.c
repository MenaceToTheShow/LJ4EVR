/* lj_main_window.c - W0 stub main window: 799x606 clBtnFace, sizeable, min 300x100 (design 7.1, 7.7).
 * The menu bar comes from the generated TLJMainForm when lj_forms_all() has it
 * (looked up at runtime so this file builds with or without lj_forms_gen.c). */
#include "desktop/lj_main_window.h"

#include <stdlib.h>
#include <string.h>

static const lj_menu_desc lj_main_empty_menu[] = { { "MainMenu1", 1, NULL, 0 } };

lj_window *lj_main_window_create(const lj_binding *bindings)
{
    lj_form_desc *d = calloc(1, sizeof *d);
    const lj_form_desc *const *all;
    const lj_form_desc *gen = NULL;
    lj_window *win;
    int n, i;

    if (!d) lj_fatal("main window: out of memory");
    all = lj_forms_all(&n);
    for (i = 0; i < n; i++)
        if (all[i]->class_name && strcmp(all[i]->class_name, "TLJMainForm") == 0) gen = all[i];

    d->name = "LJMainForm"; d->class_name = "TLJMainForm"; d->caption = "LJ4EVR";   /* runtime caption is W1 behaviour */
    d->client_width = 799; d->client_height = 606; d->text_height = 13;              /* [07 §4] design geometry */
    d->color = LJ_CLR(0xC0C0C0); d->font = LJ_FONT_SANS13; d->font_name = "MS Sans Serif"; d->font_height = -11;
    d->border = LJ_BS_SIZEABLE; d->position = LJ_PO_DEFAULT;
    d->min_width = 300; d->min_height = 100;                                          /* TLJMAINFORM Constraints */
    if (gen && gen->menu) { d->menus = gen->menus; d->nmenus = gen->nmenus; d->menu = gen->menu; }
    else { d->menus = lj_main_empty_menu; d->nmenus = 1; d->menu = "MainMenu1"; }
    d->events[0].event = "OnClose"; d->events[0].handler = "FormClose";

    win = lj_window_create(d, bindings);
    if (!win) free(d);
    return win;
}

void lj_main_window_destroy(lj_window *win)
{
    const lj_form_desc *d;
    if (!win) return;
    d = lj_window_desc(win);
    lj_window_destroy(win);
    free((void *)d);
}
