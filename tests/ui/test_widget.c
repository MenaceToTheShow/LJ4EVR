/* test_widget.c - the runnable check for src/ui/lj_widget.c: tree ops, z-order
 * hit-test on the TSEQUENCEFORM shape (Panel1 covers ESeqType except its 6-px
 * sliver, design 7.6), Align resolution, Anchors on resize, dirty collection, focus. */
#include <stdio.h>
#include <string.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void mk(lj_widget *w, const char *name, int x, int y, int ww, int hh)
{
    memset(w, 0, sizeof *w);
    w->name = name; w->x = x; w->y = y; w->w = ww; w->h = hh;
    w->visible = 1; w->enabled = 1; w->tabstop = 1;
}

int lj_test_widget(int argc, char **argv);
int lj_test_widget(int argc, char **argv)
{
    lj_widget root, l1, eno, etype, panel, bevel, sb7, top, bottom, left, client, anch;
    lj_rect r[8];
    int n, ax, ay;
    (void)argc; (void)argv;

    /* TSEQUENCEFORM top rows: ESeqType at 215,15 145x21 then Panel1 at 0,0 354x106 declared after it */
    mk(&root, "SequenceForm", 0, 0, 466, 163);
    mk(&l1, "Label1", 7, 2, 26, 13);
    mk(&eno, "ESeqNo", 7, 15, 33, 21);
    mk(&etype, "ESeqType", 215, 15, 145, 21);
    mk(&panel, "Panel1", 0, 0, 354, 106);
    mk(&bevel, "Bevel3", 1, 2, 350, 33);
    mk(&sb7, "sb7", 243, 6, 51, 25);
    CHECK(lj_widget_add(&root, &l1) == 0);
    CHECK(lj_widget_add(&root, &eno) == 0);
    CHECK(lj_widget_add(&root, &etype) == 0);
    CHECK(lj_widget_add(&root, &panel) == 0);
    CHECK(lj_widget_add(&panel, &bevel) == 0);
    CHECK(lj_widget_add(&panel, &sb7) == 0);
    CHECK(lj_widget_add(&root, &panel) == -1);                 /* already parented */
    CHECK(lj_widget_find(&root, "sb7") == &sb7);
    CHECK(lj_widget_find(&root, "nope") == NULL);
    lj_widget_abs(&sb7, &ax, &ay);
    CHECK(ax == 243 && ay == 6);

    /* z-order: Panel1 (declared last) covers ESeqType; the sliver x 354..359 is still ESeqType */
    CHECK(lj_widget_hit(&root, 220, 20) == &bevel);           /* Bevel3 inside Panel1 covers the edit */
    CHECK(lj_widget_hit(&root, 220, 36) == &panel);           /* Panel1 itself, below Bevel3 */
    CHECK(lj_widget_hit(&root, 356, 20) == &etype);
    CHECK(lj_widget_hit(&root, 250, 10) == &sb7);            /* sb7 over Bevel3 over Panel1 */
    CHECK(lj_widget_hit(&root, 100, 10) == &bevel);
    CHECK(lj_widget_hit(&root, 400, 150) == &root);
    CHECK(lj_widget_hit(&root, 466, 0) == NULL);
    panel.visible = 0;
    CHECK(lj_widget_hit(&root, 220, 20) == &etype);
    panel.visible = 1;

    /* dirty: a dirty parent is one rect and swallows its children's flags */
    lj_widget_invalidate(&sb7);
    lj_widget_invalidate(&panel);
    n = lj_widget_collect_dirty(&root, r, 8);
    CHECK(n == 1 && r[0].x == 0 && r[0].y == 0 && r[0].w == 354 && r[0].h == 106);
    CHECK(!sb7.dirty && !panel.dirty);
    CHECK(lj_widget_collect_dirty(&root, r, 8) == 0);
    lj_widget_invalidate(&sb7);
    n = lj_widget_collect_dirty(&root, r, 0);                 /* count may exceed cap */
    CHECK(n == 1);

    /* Align: alTop, alBottom, alLeft, alClient in creation order */
    lj_widget_remove(&panel);
    CHECK(lj_widget_remove(&panel) == -1);
    mk(&top, "top", 5, 5, 10, 20);       top.align = LJ_AL_TOP;
    mk(&bottom, "bottom", 5, 5, 10, 30); bottom.align = LJ_AL_BOTTOM;
    mk(&left, "left", 5, 5, 40, 10);     left.align = LJ_AL_LEFT;
    mk(&client, "client", 5, 5, 1, 1);   client.align = LJ_AL_CLIENT;
    mk(&anch, "anch", 400, 100, 50, 20); anch.anchors = LJ_AK_TOP | LJ_AK_RIGHT;
    lj_widget_add(&root, &client);                            /* declared first, still resolved last */
    lj_widget_add(&root, &top);
    lj_widget_add(&root, &bottom);
    lj_widget_add(&root, &left);
    lj_widget_add(&root, &anch);
    lj_widget_layout(&root);
    CHECK(top.x == 0 && top.y == 0 && top.w == 466 && top.h == 20);
    CHECK(bottom.x == 0 && bottom.y == 133 && bottom.w == 466 && bottom.h == 30);
    CHECK(left.x == 0 && left.y == 20 && left.w == 40 && left.h == 113);
    CHECK(client.x == 40 && client.y == 20 && client.w == 426 && client.h == 113);
    CHECK(anch.x == 400 && anch.y == 100);                    /* alNone untouched by layout */

    /* Anchors on resize: +100 wide, +50 tall; akRight moves, akTop holds, akLeft+akTop default holds */
    lj_widget_resize(&root, 566, 213);
    CHECK(anch.x == 500 && anch.y == 100 && anch.w == 50);
    CHECK(l1.x == 7 && l1.y == 2);
    CHECK(client.w == 526 && client.h == 163);
    anch.anchors = LJ_AK_LEFT | LJ_AK_RIGHT | LJ_AK_BOTTOM;
    lj_widget_resize(&root, 466, 163);
    CHECK(anch.x == 500 && anch.w == 0 && anch.y == 50);      /* width clamps at 0: 50 - 100; akBottom moves y by -50 */
    anch.anchors = 0;                                         /* 0 in a desc = [akLeft, akTop] */
    lj_widget_resize(&root, 500, 200);
    CHECK(anch.x == 500 && anch.y == 50);

    /* focus */
    CHECK(lj_widget_focused(&root) == NULL);
    CHECK(lj_widget_set_focus(&eno) == NULL && eno.focused);
    CHECK(lj_widget_set_focus(&etype) == &eno && !eno.focused && etype.focused);
    CHECK(lj_widget_focused(&root) == &etype);
    eno.enabled = 0;
    CHECK(lj_widget_set_focus(&eno) == &etype && !eno.focused && lj_widget_focused(&root) == NULL);

    /* the example table has the shape the widget layer builds from */
    {
        extern const lj_form_desc lj_form_TSEQUENCEFORM;
        const lj_ctl_desc *p1 = lj_form_find_ctl(&lj_form_TSEQUENCEFORM, "Panel1");
        const lj_ctl_desc *sb = lj_form_find_ctl(&lj_form_TSEQUENCEFORM, "sb7");
        CHECK(p1 && p1->width == 354 && p1->bevel_outer == LJ_BV_NONE);
        CHECK(sb && sb->left == 243 && sb->kind == LJ_K_SPEEDBUTTON);
        CHECK(lj_form_count_ctls(&lj_form_TSEQUENCEFORM) == 8 + 25);
        CHECK(lj_form_find_menu(&lj_form_TSEQUENCEFORM, "MainMenu1") != NULL);
        CHECK(strcmp(lj_ctl_class_names[LJ_K_ODPANEL], "TODPanel") == 0 && strcmp(lj_ctl_class_names[LJ_K_ACTION], "TAction") == 0);
        CHECK(strcmp(lj_wev_names[LJ_WEV_CLOSE], "OnClose") == 0);
    }

    printf("widget: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
