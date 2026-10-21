/* test_hittest.c - data-driven z-order hit-test over an instantiated form:
 * tests/ui/hittest/<form>.txt lists "x y name" (client coordinates) pairs; the
 * widget under each point must be the named one (design 7.6: DFM order is z-order). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf under /W4 /WX on MSVC (as core/lj_ini.c) */
#include <stdio.h>
#include <string.h>
#include "ui/lj_window.h"
#include "ui/forms/lj_form.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

extern const lj_form_desc lj_form_TSEQUENCEFORM;

static int run_file(const lj_form_desc *d, const char *path)
{
    FILE *f = fopen(path, "r");
    lj_window *win;
    lj_widget *root, *hit;
    char line[256], name[128];
    int x, y, n = 0;
    if (!f) { printf("hittest: cannot open %s\n", path); return -1; }
    win = lj_window_create(d, NULL);
    if (!win) { fclose(f); printf("hittest: lj_window_create(%s) failed\n", d->class_name); return -1; }
    root = lj_window_root(win);
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d %d %127s", &x, &y, name) != 3) { printf("hittest: bad line: %s", line); fails++; continue; }
        hit = lj_widget_hit(root, x, y);
        if (!hit || !hit->name || strcmp(hit->name, name) != 0) {
            printf("FAIL %s (%d,%d): expected %s, got %s\n", d->class_name, x, y, name, hit && hit->name ? hit->name : "(none)");
            fails++;
        }
        n++;
    }
    fclose(f);
    lj_window_destroy(win);
    printf("hittest: %s: %d point(s)\n", d->class_name, n);
    return n;
}

int lj_test_hittest(int argc, char **argv);
int lj_test_hittest(int argc, char **argv)
{
    int n;
    (void)argc; (void)argv;
    n = run_file(&lj_form_TSEQUENCEFORM, LJ_SOURCE_DIR "/tests/ui/hittest/tsequenceform.txt");
    CHECK(n >= 12);
    printf("hittest: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
