/* test_golden.c - the W0 golden-image gate (design 9.3, KING_DAVE-20): the six
 * reviewed forms (W4: all 208) painted through lj4evr_render's path must equal
 * tests/ui/golden/<FORM>.bmp byte for byte. A golden update is a reviewed commit
 * (tests/ui/golden/REVIEWED.tsv). Usage: lj4evr_tests ui_golden [tmp.bmp] */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as test_cuelist_form.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/lj_bmp.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

/* 1 identical, 0 differ, -1 unreadable; *at = first differing byte offset, *pixel = (offset-54)/3 of the first diff */
static int same_file(const char *a, const char *b, long *at)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    unsigned char ba[4096], bb[4096];
    long off = 0;
    int rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    for (;;) {
        size_t na = fread(ba, 1, sizeof ba, fa), nb = fread(bb, 1, sizeof bb, fb), i;
        if (na != nb) { rc = 0; *at = off + (long)(na < nb ? na : nb); break; }
        if (na == 0) break;
        for (i = 0; i < na; i++) if (ba[i] != bb[i]) { rc = 0; *at = off + (long)i; break; }
        if (!rc) break;
        off += (long)na;
    }
    fclose(fa); fclose(fb);
    return rc;
}

/* ME-17 census for the REVIEWED rows: visual rows whose class has no widget constructor (painted by the stub) */
static int stubs(const lj_ctl_desc *rows, int n, const lj_widget_ctor *ctor, char *names, size_t cap)
{
    int i, k = 0;
    for (i = 0; i < n; i++) {
        const lj_ctl_desc *d = &rows[i];
        if (d->kind < LJ_K_MAINMENU && (d->kind <= 0 || !ctor[d->kind])) {
            size_t l = strlen(names);
            k++;
            if (l + 40 < cap) snprintf(names + l, cap - l, " %s", d->kind > 0 && d->kind < LJ_K_COUNT ? lj_ctl_class_names[d->kind] : "?");
        }
        if (d->children) k += stubs(d->children, d->nchildren, ctor, names, cap);
    }
    return k;
}

/* W4-GOLD (design 10 W4 acceptance 3): every one of the 208 generated forms through lj4evr_render's path (DFM only, no
 * behaviour file) against tests/ui/golden/<FORM>.bmp; LJ_GOLDEN_UPDATE=1 rewrites them (each one a REVIEWED.tsv row) */
int lj_test_ui_golden(int argc, char **argv);
int lj_test_ui_golden(int argc, char **argv)
{
    int census = argc > 1 && strcmp(argv[1], "--census") == 0;
    const char *tmp = argc > 1 && !census ? argv[1] : "lj_test_golden_tmp.bmp", *upd = getenv("LJ_GOLDEN_UPDATE");
    static lj_widget_ctor ctor[LJ_K_COUNT];
    const lj_form_desc *const *forms;
    int i, n, fails = 0;
    forms = lj_forms_all(&n);
    lj_widgets_register_basic(ctor, LJ_K_COUNT); lj_widgets_register_lists(ctor, LJ_K_COUNT); lj_widgets_register_lj(ctor, LJ_K_COUNT);
    for (i = 0; i < n; i++) {
        lj_window *win = lj_window_create(forms[i], NULL);
        char golden[1024], name[64];
        size_t k;
        long at = 0;
        int rc = -1;
        for (k = 0; forms[i]->class_name[k] && k + 1 < sizeof name; k++) { char c = forms[i]->class_name[k]; name[k] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
        name[k] = 0;
        snprintf(golden, sizeof golden, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
        if (win && census) {
            char names[512] = "";
            const lj_surface *sf = lj_window_surface(win);
            int ns = stubs(forms[i]->ctls, forms[i]->n, ctor, names, sizeof names);
            printf("%s%ssurface %dx%d%sDFM %dx%d client %dx%d derived %d border %d menu %d%sstubs %d%s%s", name, "|", sf->w, sf->h, "|", forms[i]->width, forms[i]->height, forms[i]->client_width, forms[i]->client_height, forms[i]->client_derived, (int)forms[i]->border, forms[i]->menu != NULL, "|", ns, names, "\n");
        }
        if (win) {
            lj_window_paint(win);
            if (upd && upd[0] == '1') rc = lj_bmp_write(lj_window_surface(win), golden) == 0 ? 1 : -1;
            else if (lj_bmp_write(lj_window_surface(win), tmp) == 0) rc = same_file(tmp, golden, &at);
            lj_window_destroy(win);
        }
        if (rc == 0) { fails++; printf("FAIL %s: differs from %s at byte %ld (pixel %ld of the bottom-up rows)\n", name, golden, at, (at - 54) / 3); }
        else if (rc < 0) { fails++; printf("FAIL %s: %s\n", name, win ? "golden or temp file unreadable" : "form not created"); }
    }
    remove(tmp);
    {   /* KING_DAVE-20: "reviewed" = a REVIEWED.tsv row named by the form */
        char path[1024], line[4096];
        FILE *f;
        int have = 0;
        snprintf(path, sizeof path, "%s/tests/ui/golden/REVIEWED.tsv", LJ_SOURCE_DIR);
        if ((f = fopen(path, "rb")) == NULL) { fails++; printf("FAIL REVIEWED.tsv unreadable\n"); }
        for (i = 0; f && i < n; i++) {
            char name[64];
            size_t k;
            int found = 0;
            for (k = 0; forms[i]->class_name[k] && k + 1 < sizeof name; k++) { char c = forms[i]->class_name[k]; name[k] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
            name[k] = 0;
            rewind(f);
            while (!found && fgets(line, sizeof line, f)) found = strncmp(line, name, k) == 0 && line[k] == '\t';
            if (found) have++; else { fails++; printf("FAIL %s: no REVIEWED.tsv row\n", name); }
        }
        if (f) fclose(f);
        printf("ui_golden: %d of %d form(s) have a REVIEWED.tsv row\n", have, n);
    }
    printf("ui_golden: %d form(s), %d failure(s)\n", n, fails);
    return fails ? 1 : 0;
}
