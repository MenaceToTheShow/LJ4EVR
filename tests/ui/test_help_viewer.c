/* test_help_viewer.c - the runnable check of the W4 docs-release track (docs/w4-contracts.md, W4-HELP, ME-36/37):
 *  - every Help leaf of the main menu (each drop-down's Help, &Help, the three Select Helpfile languages) and of the
 *    desktop and icon popups is bound, enabled and visible, and opens LJ_WIN_HELP through the real app seam
 *    (app/lj_app_w4.c) at the index; SearchHelp1 stays hidden as its DFM has it
 *  - lj_help_viewer_topic: a named topic, an unknown one (index, -1), the topic list click, lj_help_viewer_click
 *  - the generated topics: index first, every topic titled, no line over the 72-column memo
 *  - the About text names THIRD_PARTY_NOTICES.txt and carries the provenance trademark line
 *  - golden W4HELP_VIEWER (the viewer at the index); --shot <png> writes it for review */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"
#include "app/lj_app_w4.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_help_topics_gen.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char logbuf[4096]; static size_t logn;
static void sink(void *ud, const char *line)
{
    size_t n = strlen(line);
    (void)ud;
    if (logn + n + 2 < sizeof logbuf) { memcpy(logbuf + logn, line, n); logn += n; logbuf[logn++] = '\n'; logbuf[logn] = 0; }
}

static void fire(lj_menu *m, lj_window *win, const char *name)
{
    lj_menu_rt *rt = lj_menu_rt_named(m, name);
    if (!rt || !rt->fn || !rt->enabled || !rt->visible) { fails++; printf("FAIL help leaf %s: %s\n", name, !rt ? "absent" : !rt->fn ? "unbound" : "greyed or hidden"); return; }
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    if (!lj_win_get(LJ_WIN_HELP) || lj_help_viewer_current() != 0) { fails++; printf("FAIL help leaf %s did not open the viewer at the index\n", name); }
    lj_win_close(LJ_WIN_HELP);
    CHECK(lj_win_get(LJ_WIN_HELP) == NULL);
}

static int write_png(lj_surface *s, const char *path)
{
    unsigned char *rgb = malloc((size_t)s->w * (size_t)s->h * 3);
    size_t len = 0, i;
    void *png;
    FILE *f;
    int rc = -1;
    if (!rgb) return -1;
    for (i = 0; i < (size_t)s->w * (size_t)s->h; i++) { uint32_t p = s->px[i]; rgb[i * 3] = (unsigned char)(p >> 16); rgb[i * 3 + 1] = (unsigned char)(p >> 8); rgb[i * 3 + 2] = (unsigned char)p; }
    png = tdefl_write_image_to_png_file_in_memory_ex(rgb, s->w, s->h, 3, &len, 6, MZ_FALSE);
    free(rgb);
    if (!png) return -1;
    if ((f = fopen(path, "wb")) != NULL) { rc = fwrite(png, 1, len, f) == len ? 0 : -1; fclose(f); }
    mz_free(png);
    return rc;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}

int lj_test_help_viewer(int argc, char **argv);
int lj_test_help_viewer(int argc, char **argv)
{
    static const char *const leaves[] = { "M_Help", "M_SystemHelp", "M_HelpDefaultDMX", "M_SetupHelp", "M_MainViewHelp",
                                          "M_English", "M_German", "M_French", "PD_Help", "PI_Help" };
    const char *shot = NULL, *upd = getenv("LJ_GOLDEN_UPDATE");
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_window *desk, *hv;
    lj_menu *m;
    char text[512], path[1024];
    int i, nleaf = 0, rc;

    for (i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--shot") == 0) shot = argv[i + 1];
    lj_log_set_sink(sink, NULL);
    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->show = show; ctx->lib = NULL; ctx->main_ini = NULL; ctx->forms_ini = NULL; ctx->prefs = NULL; ctx->engine = NULL; ctx->userdir[0] = 0;
    desk = lj_desktop_create();
    CHECK(desk != NULL);
    if (!desk) { lj_show_free(show); return 1; }
    lj_app_w4_start();
    m = lj_window_menus(desk);

    /* 1. every Help leaf, by name and by handler: each opens the viewer */
    for (i = 0; i < LJ_COUNT_OF(leaves); i++) {
        if (strcmp(leaves[i], "PI_Help") == 0) lj_desktop_popup(LJ_POP_ICON, 10, 10, 0);   /* the icon popup acts on the fixture it was raised on */
        fire(m, desk, leaves[i]);
        lj_menu_close(m);
    }
    for (i = 0; i < m->nrt; i++) {   /* any other item routed to the main form's HelpClick (the per-menu Help items) */
        const lj_menu_item_desc *it = m->rt[i].item;
        int k, named = 0;
        if (!it->on_click || strcmp(it->on_click, "HelpClick") != 0 || strcmp(it->name, "M_About") == 0) continue;
        for (k = 0; k < LJ_COUNT_OF(leaves); k++) named |= strcmp(leaves[k], it->name) == 0;
        nleaf++;
        if (!named) fire(m, desk, it->name);
    }
    CHECK(nleaf >= 5);
    CHECK(lj_menu_rt_named(m, "SearchHelp1") && !lj_menu_rt_named(m, "SearchHelp1")->visible);   /* hidden in the DFM, kept hidden */
    CHECK(strstr(logbuf, "only the English topics ship") != NULL);                                /* Deutsch / Francais open the English index */

    /* 2. topics */
    CHECK(lj_help_ntopics >= 10 && strcmp(lj_help_topics[0].name, "index") == 0);
    for (i = 0; i < lj_help_ntopics; i++) {
        int k;
        CHECK(lj_help_topics[i].title[0] && lj_help_topics[i].n > 0);
        for (k = 0; k < lj_help_topics[i].n; k++) CHECK(strlen(lj_help_topics[i].lines[k]) <= 72);
    }
    CHECK(lj_help_viewer_find(NULL) == 0 && lj_help_viewer_find("") == 0 && lj_help_viewer_find("nope") == -1);
    CHECK(lj_help_viewer_topic("output") == 0 && lj_help_viewer_current() == lj_help_viewer_find("output"));
    CHECK(lj_help_viewer_topic("nope") == -1 && lj_help_viewer_current() == 0);
    hv = lj_win_get(LJ_WIN_HELP);
    CHECK(hv && strcmp(lj_window_desc(hv)->caption, "LJ4EVR Help") == 0);
    if (hv) {
        lj_w_list *l = (lj_w_list *)lj_window_find(hv, "lbTopics");
        lj_w_memo *mm = (lj_w_memo *)lj_window_find(hv, "mText");
        CHECK(l && l->n == lj_help_ntopics);
        CHECK(mm && mm->n == lj_help_topics[0].n + 2 && strcmp(mm->lines[0], lj_help_topics[0].title) == 0);
        if (l && l->base.on_event) { l->index = 3; l->base.on_event(&l->base, LJ_WEV_CLICK, l->base.ud); CHECK(lj_help_viewer_current() == 3); }
        lj_help_viewer_click(NULL, (void *)"cuelists");
        CHECK(lj_help_viewer_current() == lj_help_viewer_find("cuelists"));
        CHECK(lj_help_viewer_find("notices") > 0);
    }
    CHECK(strstr(logbuf, "no binding") == NULL);   /* ME-16 */

    /* 3. golden: the viewer at the index */
    lj_help_viewer_topic(NULL);
    hv = lj_win_get(LJ_WIN_HELP);
    if (hv) {
        lj_window_invalidate_all(hv);
        lj_window_paint(hv);
        snprintf(path, sizeof path, "%s/tests/ui/golden/W4HELP_VIEWER.bmp", LJ_SOURCE_DIR);
        if (upd && upd[0] == '1') CHECK(lj_bmp_write(lj_window_surface(hv), path) == 0);
        else {
            CHECK(lj_bmp_write(lj_window_surface(hv), "W4HELP_VIEWER.new.bmp") == 0);
            rc = same_file("W4HELP_VIEWER.new.bmp", path);
            if (rc != 1) { fails++; printf("FAIL golden W4HELP_VIEWER: %s (new render at W4HELP_VIEWER.new.bmp)\n", rc == 0 ? "differs" : "missing"); }
        }
        if (shot) CHECK(write_png(lj_window_surface(hv), shot) == 0);
    }
    lj_win_close(LJ_WIN_HELP);

    /* 4. About (ME-36 / ME-37) */
    lj_win_about_text(text, sizeof text);
    CHECK(strstr(text, "THIRD_PARTY_NOTICES.txt") != NULL);
    CHECK(strstr(text, "LightJockey is a trademark of Martin Professional; LJ4EVR is not affiliated with or endorsed by Martin Professional or HARMAN.") != NULL);

    lj_desktop_destroy(desk);
    lj_show_free(show);
    ctx->show = NULL;
    lj_log_set_sink(NULL, NULL);
    printf("test_help_viewer: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
