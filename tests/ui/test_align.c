/* test_align.c - ME-43 (W4 track 0): the full align pass reproduces the DFM. Every generated form is built at
 * its design client (the W0 rule: the DFM rects stand), then lj_widget_layout runs the align pass the VCL runs on
 * a resize (alTop by Top, alBottom by bottom edge descending, alLeft by Left, alRight by right edge descending,
 * AdjustClientRect insets per class). An aligned control whose rect moves is "misplaced"; the count must be 0.
 * Forms whose client the generator derived from Width/Height (AutoScroll forms, client_derived) are counted
 * separately and not gated: their DFM was laid out at a client we only estimate.
 * Usage: lj4evr_tests test_align [-v]   (-v lists every misplaced control) */
#include <stdio.h>
#include <string.h>
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"

#define ALIGN_MAX 4096

typedef struct { lj_widget *w; int x, y, w_, h; } snap;

static int collect(lj_widget *p, snap *s, int n)
{
    lj_widget *c;
    for (c = p->first; c; c = c->next) {
        if (c->align != LJ_AL_NONE && n < ALIGN_MAX) { s[n].w = c; s[n].x = c->x; s[n].y = c->y; s[n].w_ = c->w; s[n].h = c->h; n++; }
        n = collect(c, s, n);
    }
    return n;
}

static snap snaps[ALIGN_MAX];

/* Named exclusions: a rect that moves for a reason other than the align pass, with the row that owns it. */
static const struct { const char *form, *ctl, *why; } lj_align_excl[] = {
    { "TIntensityGroupForm", "lvFixtures", "tab rows, not align: LJ2 wraps the 9 MultiLine tabs of PG to 2 rows (DFM 281 - 235 = 46), "
                                           "we fit them in 1; comctl32's minimum tab width is a rig item (W4-FRAME)" },
};

static const char *excluded(const char *form, const char *ctl)
{
    size_t i;
    for (i = 0; i < sizeof lj_align_excl / sizeof lj_align_excl[0]; i++)
        if (strcmp(lj_align_excl[i].form, form) == 0 && ctl && strcmp(lj_align_excl[i].ctl, ctl) == 0) return lj_align_excl[i].why;
    return NULL;
}

int lj_test_align(int argc, char **argv);
int lj_test_align(int argc, char **argv)
{
    const lj_form_desc *const *forms;
    int nforms, i, k, verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
    int aligned = 0, bad = 0, derived_bad = 0, gated_forms = 0, excl = 0;
    forms = lj_forms_all(&nforms);
    for (i = 0; i < nforms; i++) {
        lj_window *win = lj_window_create(forms[i], NULL);
        int n, miss = 0;
        if (!win) { printf("FAIL %s: not built\n", forms[i]->class_name); bad++; continue; }
        n = collect(lj_window_root(win), snaps, 0);
        lj_widget_layout(lj_window_root(win));
        for (k = 0; k < n; k++) {
            const snap *s = &snaps[k];
            const char *why;
            if (s->w->x == s->x && s->w->y == s->y && s->w->w == s->w_ && s->w->h == s->h) continue;
            if (!forms[i]->client_derived && (why = excluded(forms[i]->class_name, s->w->name)) != NULL) {
                printf("  excluded %s.%s: %s\n", forms[i]->class_name, s->w->name, why);
                excl++;
                continue;
            }
            miss++;
            if (verbose) printf("  %s%s.%s align %d: DFM %d,%d %dx%d -> %d,%d %dx%d\n", forms[i]->client_derived ? "(derived) " : "",
                                forms[i]->class_name, s->w->name ? s->w->name : "?",
                                (int)s->w->align, s->x, s->y, s->w_, s->h, s->w->x, s->w->y, s->w->w, s->w->h);
        }
        if (forms[i]->client_derived) derived_bad += miss;
        else { aligned += n; bad += miss; gated_forms++; }
        lj_window_destroy(win);
    }
    printf("align: %d forms at their design client (%d derived-client forms not gated, %d misplaced there), "
           "%d aligned controls, %d misplaced, %d excluded by name\n", gated_forms, nforms - gated_forms, derived_bad, aligned, bad, excl);
    return bad ? 1 : 0;
}
