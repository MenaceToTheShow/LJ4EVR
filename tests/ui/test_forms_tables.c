/* test_forms_tables.c - the runnable check for tools/dfm2c.py's output (src/ui/forms/lj_forms_gen.c):
 * 208 forms registered, TSEQUENCEFORM facts (design 7.6), TCUELIST2FORM colour (7.3), the menu-item
 * census (7.9: 1081), the forms.ini persistence set (7.10), no LJ_K_UNKNOWN row anywhere. */
#include <stdio.h>
#include <string.h>
#include "ui/forms/lj_form.h"
#include "ui/forms/lj_forms_gen.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int count_menu_items(const lj_menu_item_desc *items, int n)
{
    int i, total = n;
    for (i = 0; i < n; i++) total += count_menu_items(items[i].children, items[i].nchildren);
    return total;
}

static int count_unknown(const lj_ctl_desc *rows, int n)
{
    int i, bad = 0;
    for (i = 0; i < n; i++) {
        if (rows[i].kind == LJ_K_UNKNOWN || rows[i].name == NULL) bad++;
        bad += count_unknown(rows[i].children, rows[i].nchildren);
    }
    return bad;
}

static int index_of(const lj_form_desc *f, const char *name)
{
    int i;
    for (i = 0; i < f->n; i++)
        if (strcmp(f->ctls[i].name, name) == 0) return i;
    return -1;
}

int lj_test_forms_tables(int argc, char **argv);
int lj_test_forms_tables(int argc, char **argv)
{
    const lj_form_desc *const *all;
    const lj_form_desc *f = &lj_form_TSEQUENCEFORM;
    const lj_ctl_desc *panel1;
    int n = 0, i, j, visual = 0, menu_items = 0, persisted = 0, unknown = 0, rows = 0;
    (void)argc; (void)argv;

    all = lj_forms_all(&n);
    CHECK(all != NULL);
    CHECK(n == 208);
    CHECK(n == LJ_FORMS_COUNT);

    /* TSEQUENCEFORM: client 466x163, clGray, 7 visual top-level controls + the TMainMenu row, Panel1 declared after ESeqType */
    CHECK(strcmp(f->name, "SequenceForm") == 0);
    CHECK(strcmp(f->class_name, "TSequenceForm") == 0);
    CHECK(f->client_width == 466 && f->client_height == 163);
    CHECK(lj_desc_rgb(f->color, 0) == 0x808080);
    CHECK(f->border == LJ_BS_SINGLE && f->style == LJ_FS_STAYONTOP);
    CHECK(f->icons == (LJ_BI_GIVEN | LJ_BI_SYSMENU));
    CHECK(f->font == LJ_FONT_SANS13 && f->text_height == 13);
    for (i = 0; i < f->n; i++)
        if (f->ctls[i].kind != LJ_K_MAINMENU) visual++;
    CHECK(f->n == 8 && visual == 7);
    panel1 = lj_form_find_ctl(f, "Panel1");
    CHECK(panel1 != NULL);
    CHECK(panel1 && panel1->left == 0 && panel1->top == 0 && panel1->width == 354 && panel1->height == 106);
    CHECK(panel1 && panel1->bevel_outer == LJ_BV_NONE && panel1->nchildren == 25);
    CHECK(index_of(f, "Panel1") > index_of(f, "ESeqType") && index_of(f, "ESeqType") >= 0);
    CHECK(lj_form_find_ctl(f, "sb7") != NULL && lj_form_find_ctl(f, "sb7")->width == 51);
    CHECK(lj_form_find_ctl(f, "cbSync") && strcmp(lj_form_find_ctl(f, "cbSync")->caption, "S&ynchronize Scene and Fade Times") == 0);
    CHECK(lj_form_find_ctl(f, "ESeqNo") && lj_form_find_ctl(f, "ESeqNo")->readonly == LJ_YES);
    CHECK(lj_form_find_ctl(f, "Label1") && lj_form_find_ctl(f, "Label1")->font_inherited == 1);
    CHECK(f->menu && strcmp(f->menu, "MainMenu1") == 0 && lj_form_find_menu(f, "MainMenu1") != NULL);
    CHECK(lj_form_find_menu(f, "MainMenu1") && lj_form_find_menu(f, "MainMenu1")->n == 4);
    CHECK(f->events[0].event && strcmp(f->events[0].event, "OnActivate") == 0 && strcmp(f->events[0].handler, "FormActivate") == 0);

    /* TCUELIST2FORM: the cue-list blue [F18] */
    CHECK(lj_desc_rgb(lj_form_TCUELIST2FORM.color, 0) == 0x6B8DCB);

    /* whole-corpus counts */
    for (i = 0; i < n; i++) {
        CHECK(all[i]->name != NULL && all[i]->class_name != NULL && all[i]->client_width > 0);
        for (j = 0; j < all[i]->nmenus; j++) menu_items += count_menu_items(all[i]->menus[j].items, all[i]->menus[j].n);
        if (all[i]->position == LJ_PO_DESIGNED || all[i]->position == LJ_PO_DEFAULTPOSONLY) persisted++;
        unknown += count_unknown(all[i]->ctls, all[i]->n);
        rows += lj_form_count_ctls(all[i]);
    }
    CHECK(menu_items == 1081);
    CHECK(persisted == 73);                      /* 70 poDesigned (implicit) + 3 poDefaultPosOnly; 135 centre on their owner/screen */
    CHECK(unknown == 0);
    CHECK(rows == 3632);                         /* every non-menu-item object of the 208 DFMs */
    printf("forms %d, rows %d, menu items %d, forms.ini-persisted %d, unknown rows %d, fails %d\n",
           n, rows, menu_items, persisted, unknown, fails);
    return fails ? 1 : 0;
}
