/* test_golden.c - the W0 golden-image gate (design 9.3, KING_DAVE-20): the five
 * reviewed forms painted through lj4evr_render's path must equal
 * tests/ui/golden/<FORM>.bmp byte for byte. A golden update is a reviewed commit
 * (tests/ui/golden/REVIEWED.tsv). Usage: lj4evr_tests ui_golden [tmp.bmp] */
#include <stdio.h>
#include <string.h>
#include "gfx/lj_bmp.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static const char *const lj_golden_forms[] = { "TSEQUENCEFORM", "TSAVESEQFORM", "TCURRENTCUE", "TSYSTEMPREFSFORM", "TLINKCONFIGFORM3" };

static const lj_form_desc *find_form(const char *upper)
{
    const lj_form_desc *const *forms;
    int n, i;
    size_t k;
    forms = lj_forms_all(&n);
    for (i = 0; i < n; i++) {
        const char *c = forms[i]->class_name;
        for (k = 0; c[k] && upper[k]; k++) {
            char ch = c[k] >= 'a' && c[k] <= 'z' ? (char)(c[k] - 'a' + 'A') : c[k];
            if (ch != upper[k]) break;
        }
        if (!c[k] && !upper[k]) return forms[i];
    }
    return NULL;
}

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

int lj_test_ui_golden(int argc, char **argv);
int lj_test_ui_golden(int argc, char **argv)
{
    const char *tmp = argc > 1 ? argv[1] : "lj_test_golden_tmp.bmp";
    int i, fails = 0;
    for (i = 0; i < LJ_COUNT_OF(lj_golden_forms); i++) {
        const lj_form_desc *d = find_form(lj_golden_forms[i]);
        lj_window *win = d ? lj_window_create(d, NULL) : NULL;
        char golden[1024];
        long at = 0;
        int rc = -1;
        snprintf(golden, sizeof golden, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, lj_golden_forms[i]);
        if (win) {
            lj_window_paint(win);
            if (lj_bmp_write(lj_window_surface(win), tmp) == 0) rc = same_file(tmp, golden, &at);
            lj_window_destroy(win);
        }
        if (rc == 1) printf("  %s: identical\n", lj_golden_forms[i]);
        else if (rc == 0) { fails++; printf("FAIL %s: differs from %s at byte %ld (pixel %ld of the bottom-up rows)\n", lj_golden_forms[i], golden, at, (at - 54) / 3); }
        else { fails++; printf("FAIL %s: %s\n", lj_golden_forms[i], win ? "golden or temp file unreadable" : "form not found / not created"); }
    }
    remove(tmp);
    printf("ui_golden: %d form(s), %d failure(s)\n", (int)LJ_COUNT_OF(lj_golden_forms), fails);
    return fails ? 1 : 0;
}
