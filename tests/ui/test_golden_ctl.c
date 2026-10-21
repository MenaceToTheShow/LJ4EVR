/* test_golden_ctl.c - the W1 goldens of the code-built fixture-control windows
 * (design 10 W1 acceptance 3, KING_DAVE-20): a show with one MAC 500 Mode 2/4
 * (built-in 12) at 1:001 and one Generic DMX Dimmer 1 (built-in 19) at 1:017, the
 * MAC 500 as master, every window its profile table offers opened headless through
 * lj_ctl_open and painted; then the dimmer as master and its Levels window. Each
 * surface must equal tests/ui/golden/CTL_<FIXTURE>_<KIND>.bmp byte for byte; a
 * golden update is a reviewed commit (tests/ui/golden/REVIEWED.tsv).
 * Usage: lj4evr_tests ui_golden_ctl [--write-golden] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl.h"
#include "desktop/lj_selection.h"
#include "gfx/lj_bmp.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *const lj_kind_names[LJ_CTL_COUNT] = { "INTENSITY", "MOVEMENT", "COLORS", "GOBOS", "BEAM", "EFFECTS", "LEVELS", "LAMP", "EXTENDED", "GENERIC", "RGBPACK" };

static int same_file(const char *a, const char *b, long *at)   /* 1 identical, 0 differ, -1 unreadable */
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

static void one(const char *fixture, enum lj_ctl_kind kind, int write_golden)
{
    char golden[1024], tmp[256];
    const lj_surface *s;
    lj_window *win = lj_ctl_open(kind);
    long at = 0;
    int rc = -1;
    snprintf(golden, sizeof golden, "%s/tests/ui/golden/CTL_%s_%s.bmp", LJ_SOURCE_DIR, fixture, lj_kind_names[kind]);
    snprintf(tmp, sizeof tmp, "lj_test_golden_ctl_%s_%s.bmp", fixture, lj_kind_names[kind]);
    CHECK(win != NULL);
    if (!win) return;
    lj_window_paint(win);
    s = lj_window_surface(win);
    if (write_golden) { CHECK(lj_bmp_write(s, golden) == 0); printf("  wrote %s (%dx%d)\n", golden, s->w, s->h); }
    else if (lj_bmp_write(s, tmp) == 0) rc = same_file(tmp, golden, &at);
    if (!write_golden) {
        if (rc == 1) printf("  CTL_%s_%s: identical (%dx%d)\n", fixture, lj_kind_names[kind], s->w, s->h);
        else { fails++; printf("FAIL CTL_%s_%s: %s (byte %ld)\n", fixture, lj_kind_names[kind], rc == 0 ? "differs from the golden" : "golden or temp file unreadable", at); }
    }
    remove(tmp);
    lj_ctl_close(kind);
}

int lj_test_ui_golden_ctl(int argc, char **argv);
int lj_test_ui_golden_ctl(int argc, char **argv)
{
    int write_golden = argc > 1 && strcmp(argv[1], "--write-golden") == 0;
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    const lj_profile *p;
    int k;
    if (!show) { printf("FAIL out of memory\n"); return 1; }
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].profile_id = 12; show->fixture[0].link = 1; show->fixture[0].address = 1;    /* MAC 500 Mode 2/4 */
    show->fixture[1].kind = LJ_FX_BUILTIN; show->fixture[1].profile_id = 19; show->fixture[1].link = 1; show->fixture[1].address = 17;   /* Generic DMX Dimmer 1 */
    show->current_seq = lj_sequence_new_rec();
    if (!show->current_seq->scenes) lj_sequence_alloc(show->current_seq, 1);
    ctx->show = show; ctx->current_scene = 0;
    lj_sel_reset();

    lj_sel_set(0, 1, 1);
    p = lj_sel_master_profile();
    CHECK(p && p->id == 12);
    for (k = 0; p && k < LJ_CTL_COUNT; k++) if (lj_profile_window(p, (enum lj_ctl_kind)k)) one("MAC500", (enum lj_ctl_kind)k, write_golden);

    lj_sel_none();
    lj_sel_set(1, 1, 1);
    p = lj_sel_master_profile();
    CHECK(p && p->id == 19);
    for (k = 0; p && k < LJ_CTL_COUNT; k++) if (lj_profile_window(p, (enum lj_ctl_kind)k)) one("DMX1", (enum lj_ctl_kind)k, write_golden);

    lj_ctl_close_all();
    lj_sel_reset();
    ctx->show = NULL;
    lj_show_free(show);
    printf("ui_golden_ctl: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
