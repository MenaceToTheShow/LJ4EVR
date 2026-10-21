/* test_formsini.c - the runnable check for src/ui/lj_formsini.c (design 7.10):
 * section naming (known LJ2 sections, class-name rule, the never-persisted
 * centred forms), Left/Top(/Width/Height) round trip, centre-on-owner. */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "ui/lj_formsini.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

extern const lj_form_desc lj_form_TSEQUENCEFORM;

int lj_test_formsini(int argc, char **argv);
int lj_test_formsini(int argc, char **argv)
{
    char sec[64];
    lj_form_desc d;
    lj_ini *ini;
    int x = -1, y = -1, w = 10, h = 20;
    (void)argc; (void)argv;

    /* sections */
    CHECK(lj_formsini_section(&lj_form_TSEQUENCEFORM, sec, sizeof sec) == LJ_FORMSINI_FORMS && strcmp(sec, "SEQ") == 0);
    memset(&d, 0, sizeof d);
    d.class_name = "TCuelist2Form"; d.position = LJ_PO_OWNERFORMCENTER;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == LJ_FORMSINI_FORMS && strcmp(sec, "CUELIST2") == 0);   /* known section wins over Position */
    d.class_name = "TCueMacroControlForm";
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == LJ_FORMSINI_MAIN && strcmp(sec, "MACRO") == 0);
    d.class_name = "TSetupForm"; d.position = LJ_PO_OWNERFORMCENTER;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == -1 && sec[0] == 0);
    d.position = LJ_PO_MAINFORMCENTER;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == -1);
    d.position = LJ_PO_SCREENCENTER;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == -1);
    d.position = LJ_PO_DESIGNED;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == LJ_FORMSINI_FORMS && strcmp(sec, "SETUPFORM") == 0);
    d.position = LJ_PO_DEFAULTPOSONLY;
    CHECK(lj_formsini_section(&d, sec, sizeof sec) == LJ_FORMSINI_FORMS && strcmp(sec, "SETUPFORM") == 0);
    CHECK(lj_formsini_section(NULL, sec, sizeof sec) == -1);
    CHECK(lj_formsini_section(&d, sec, 4) == LJ_FORMSINI_FORMS && strcmp(sec, "SET") == 0);   /* truncates, NUL-terminated */
    d.border = LJ_BS_SIZEABLE;
    CHECK(lj_formsini_sizeable(&d) && !lj_formsini_sizeable(&lj_form_TSEQUENCEFORM));

    /* restore / save round trip */
    ini = lj_ini_new();
    CHECK(ini != NULL);
    CHECK(lj_formsini_restore(ini, "SEQ", &x, &y, &w, &h) == -1 && x == -1 && w == 10);
    lj_formsini_save(ini, "SEQ", 0, 100, 120, 466, 163);
    CHECK(lj_formsini_restore(ini, "SEQ", &x, &y, &w, &h) == 0 && x == 100 && y == 120 && w == 10 && h == 20);   /* not sizeable: no Width/Height */
    CHECK(lj_ini_geti(ini, "SEQ", "Width", -1) == -1);
    lj_formsini_save(ini, "CUE", 1, 100, 100, 620, 300);
    CHECK(lj_formsini_restore(ini, "CUE", &x, &y, &w, &h) == 0 && x == 100 && w == 620 && h == 300);
    CHECK(lj_formsini_restore(ini, "CUE", &x, &y, NULL, NULL) == 0);
    lj_ini_seti(ini, "HALF", "Left", 5);
    CHECK(lj_formsini_restore(ini, "HALF", &x, &y, &w, &h) == -1);     /* Top missing */
    CHECK(lj_formsini_restore(NULL, "SEQ", &x, &y, &w, &h) == -1);
    lj_ini_free(ini);

    /* centre on the owner */
    lj_formsini_center(100, 100, 400, 300, 200, 100, &x, &y);
    CHECK(x == 200 && y == 200);
    lj_formsini_center(0, 0, 100, 100, 300, 50, &x, &y);
    CHECK(x == -100 && y == 25);

    printf("formsini: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
