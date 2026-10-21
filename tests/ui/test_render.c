/* test_render.c - the runnable check for lj4evr_render's path: every generated
 * form and the stub main window instantiate with no bindings, paint to a surface
 * at least as large as their client, and lj_bmp_write produces a file of exactly
 * 54 + h * ((w * 3 + 3) & ~3) bytes. Usage: lj4evr_tests render [tmp.bmp] */
#include <stdio.h>
#include <string.h>
#include "desktop/lj_main_window.h"
#include "gfx/lj_bmp.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static long lj_file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n = -1;
    if (f && fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (f) fclose(f);
    return n;
}

static void lj_check_window(lj_window *win, const char *what, int cw, int ch, const char *tmp)
{
    const lj_surface *s;
    long want;
    lj_rect cr;
    CHECK(win != NULL);
    if (!win) { printf("  %s: not created\n", what); return; }
    lj_window_paint(win);
    s = lj_window_surface(win);
    lj_window_client_rect(win, &cr);
    CHECK(s->w >= cw && s->h >= ch);
    CHECK(cr.w == cw && cr.h == ch);
    CHECK(lj_bmp_write(s, tmp) == 0);
    want = 54 + (long)s->h * ((s->w * 3 + 3) & ~3);
    CHECK(lj_file_size(tmp) == want);
    remove(tmp);
}

int lj_test_render(int argc, char **argv);
int lj_test_render(int argc, char **argv)
{
    const char *tmp = argc > 1 ? argv[1] : "lj_test_render_tmp.bmp";
    const lj_form_desc *const *forms;
    lj_window *win;
    int n, i;

    forms = lj_forms_all(&n);
    CHECK(n >= 1);
    for (i = 0; i < n; i++) {
        win = lj_window_create(forms[i], NULL);
        lj_check_window(win, forms[i]->class_name, forms[i]->client_width, forms[i]->client_height, tmp);
        lj_window_destroy(win);
    }
    win = lj_main_window_create(NULL);
    lj_check_window(win, "main window", 799, 606, tmp);
    CHECK(win && lj_window_menu(win) != NULL);
    lj_main_window_destroy(win);
    printf("render: %d form(s) + main window, %d failure(s)\n", n, fails);
    return fails ? 1 : 0;
}
