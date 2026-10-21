/* lj_app.c - lj4evr startup, W0: Main.Ini, stub main window, TSEQUENCEFORM + TCURRENTCUE, engine thread, UI loop, forms.ini. */
#include "app/lj_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_main_window.h"
#include "engine/lj_engine.h"
#include "gfx/lj_bmp.h"
#include "platform/lj_platform.h"
#include "platform/sdl/lj_sdl.h"
#include "ui/lj_formsini.h"
#include "ui/lj_window.h"
#include "version.h"

typedef struct {
    lj_platform_cfg cfg;
    char pref[512];
    lj_ini *forms_ini;
    lj_window *main, *seq, *cue;
    lj_engine *engine;
    int explode, run_ms, selftest, tick_hz;
    const char *save_path, *selftest_ref;
    uint64_t t0;
} lj_app;

static void lj_app_on_main_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_platform_quit(); }

static void lj_app_idle(void *ud)
{
    lj_app *a = ud;
    if (a->run_ms > 0 && (lj_clock_us() - a->t0) / 1000u >= (uint64_t)a->run_ms) lj_platform_quit();
}

static const lj_form_desc *lj_app_find_form(const char *class_name)
{
    int n, i;
    const lj_form_desc *const *all = lj_forms_all(&n);
    for (i = 0; i < n; i++)
        if (all[i]->class_name && strcmp(all[i]->class_name, class_name) == 0) return all[i];
    return NULL;
}

/* --explode (design 7.6): a container declared after the controls it covers moves below them, the client grows. */
static void lj_app_explode(lj_window *win)
{
    lj_widget *root = lj_window_root(win), *c, *p;
    lj_rect cr;
    int bottom, max_bottom = 0;
    if (!root) return;
    for (c = root->first; c; c = c->next) {
        if (c->kind == LJ_K_PANEL || c->kind == LJ_K_GROUPBOX || c->kind == LJ_K_PAGECONTROL) {
            bottom = c->y;
            for (p = root->first; p != c; p = p->next)
                if (p->x < c->x + c->w && c->x < p->x + p->w && p->y < c->y + c->h && c->y < p->y + p->h && p->y + p->h + 4 > bottom)
                    bottom = p->y + p->h + 4;
            c->y = bottom;
        }
        if (c->y + c->h > max_bottom) max_bottom = c->y + c->h;
    }
    lj_window_client_rect(win, &cr);
    if (max_bottom > cr.h) lj_window_resize(win, cr.w, max_bottom);
    lj_window_invalidate_all(win);
}

/* Opens a generated form; position from forms.ini when design 7.10 persists it, else the DFM Left/Top, else centred on the main window. */
static lj_window *lj_app_open_form(lj_app *a, const char *class_name)
{
    const lj_form_desc *d = lj_app_find_form(class_name);
    lj_window *win;
    char sec[64];
    int x, y, mx, my;
    if (!d) { lj_log("app: form %s is not in the tables", class_name); return NULL; }
    win = lj_window_create(d, NULL);
    if (!win) return NULL;
    if (a->explode) lj_app_explode(win);
    if (lj_platform_open_window(win, 0) != 0) { lj_window_destroy(win); return NULL; }
    x = d->left; y = d->top;
    if (lj_formsini_section(d, sec, sizeof sec) < 0) {      /* never persisted: centre on the owner every time */
        const lj_surface *ms = lj_window_surface(a->main), *ws = lj_window_surface(win);
        if (lj_platform_window_pos(a->main, &mx, &my) == 0) lj_formsini_center(mx, my, ms->w, ms->h, ws->w, ws->h, &x, &y);
    } else lj_formsini_restore(a->forms_ini, sec, &x, &y, NULL, NULL);   /* ponytail: W0 keeps the Main.Ini sections in forms.ini too */
    lj_platform_move_window(win, x, y);
    return win;
}

static void lj_app_save_pos(lj_app *a, lj_window *win)
{
    const lj_form_desc *d;
    const lj_surface *s;
    char sec[64];
    int x, y;
    if (!win) return;
    d = lj_window_desc(win); s = lj_window_surface(win);
    if (lj_formsini_section(d, sec, sizeof sec) >= 0 && lj_platform_window_pos(win, &x, &y) == 0)
        lj_formsini_save(a->forms_ini, sec, lj_formsini_sizeable(d), x, y, s->w, s->h);
}

/* --selftest: what the OS window surface holds must equal what lj4evr_render paints for the same form. */
static int lj_app_selftest(lj_app *a)
{
    lj_surface got, ref_bmp;
    const lj_surface *ref;
    lj_window *ref_win = NULL;
    int bad = 0, i, n;
    memset(&got, 0, sizeof got); memset(&ref_bmp, 0, sizeof ref_bmp);
    if (!a->seq) { printf("selftest FAIL: TSEQUENCEFORM did not open\n"); return 1; }
    lj_window_set_active(a->seq, 0);                        /* the reference is painted inactive */
    lj_platform_present(a->seq);
    if (lj_platform_readback(a->seq, &got) != 0) { printf("selftest FAIL: read-back\n"); return 1; }
    if (a->selftest_ref) {
        if (lj_platform_load_bmp(a->selftest_ref, &ref_bmp) != 0) { printf("selftest FAIL: cannot load %s\n", a->selftest_ref); lj_surface_free(&got); return 1; }
        ref = &ref_bmp;
    } else {
        ref_win = lj_window_create(lj_window_desc(a->seq), NULL);
        if (!ref_win) { printf("selftest FAIL: reference window\n"); lj_surface_free(&got); return 1; }
        lj_window_paint(ref_win);
        ref = lj_window_surface(ref_win);
    }
    if (got.w != ref->w || got.h != ref->h) bad = -1;
    else for (i = 0, n = got.w * got.h; i < n; i++) bad += (got.px[i] & 0xFFFFFFu) != (ref->px[i] & 0xFFFFFFu);
    printf("selftest TSEQUENCEFORM: screen %dx%d vs render %dx%d, %d mismatching pixel(s): %s\n",
           got.w, got.h, ref->w, ref->h, bad, bad == 0 ? "OK" : "FAIL");
    lj_surface_free(&got);
    lj_surface_free(&ref_bmp);
    lj_window_destroy(ref_win);
    return bad == 0 ? 0 : 1;
}

int lj_app_main(int argc, char **argv)
{
    lj_app a;
    lj_ini *main_ini;
    char path[600];
    int i, rc = 0;
    const lj_binding main_bindings[] = { { "FormClose", lj_app_on_main_close, NULL }, { NULL, NULL, NULL } };

    memset(&a, 0, sizeof a);
    a.cfg.tick_ms = 100; a.cfg.zoom = 1; a.tick_hz = 40;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) a.save_path = argv[++i];
        else if (strcmp(argv[i], "--selftest") == 0) { a.selftest = 1; if (i + 1 < argc && argv[i + 1][0] != '-') a.selftest_ref = argv[++i]; }
        else if (strcmp(argv[i], "--explode") == 0) a.explode = 1;
        else if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc) a.run_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--native") == 0) a.cfg.native_frame = 1;
        else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) a.cfg.zoom = atoi(argv[++i]);
        else { fprintf(stderr, "lj4evr %s: unknown argument %s\n", LJ_VERSION_STR, argv[i]); return 2; }
    }

    if (lj_platform_pref_path(a.pref, sizeof a.pref) != 0) a.pref[0] = 0;
    snprintf(path, sizeof path, "%sMain.Ini", a.pref);
    main_ini = lj_ini_load(path);
    if (main_ini) {
        a.cfg.zoom = lj_ini_geti(main_ini, "CHROME", "Zoom", a.cfg.zoom);
        a.cfg.native_frame = lj_ini_geti(main_ini, "CHROME", "Native", a.cfg.native_frame);
        a.tick_hz = lj_ini_geti(main_ini, "ENGINE", "TickHz", a.tick_hz);
        lj_ini_free(main_ini);
    }
    if (a.tick_hz < 1 || a.tick_hz > 1000) a.tick_hz = 40;
    if (a.selftest) a.cfg.zoom = 1;
    if (lj_platform_init(&a.cfg) != 0) return 1;

    snprintf(path, sizeof path, "%sforms.ini", a.pref);
    a.forms_ini = lj_ini_load(path);
    if (!a.forms_ini) a.forms_ini = lj_ini_new();
    a.engine = lj_engine_create();

    a.main = lj_main_window_create(main_bindings);
    if (!a.main || lj_platform_open_window(a.main, LJ_WF_MAIN) != 0) { lj_log("app: no main window"); rc = 1; goto out; }
    a.seq = lj_app_open_form(&a, "TSequenceForm");
    a.cue = lj_app_open_form(&a, "TCurrentCue");

    if (a.save_path && lj_bmp_write(lj_window_surface(a.main), a.save_path) == 0) printf("saved %s\n", a.save_path);
    if (a.selftest) { rc = lj_app_selftest(&a); goto out; }

    if (a.engine && lj_platform_engine_start(a.engine, 1000000u / (uint32_t)a.tick_hz) != 0) lj_log("app: engine thread did not start");
    a.t0 = lj_clock_us();
    lj_platform_run(lj_app_idle, &a);
    lj_platform_engine_stop();

    lj_app_save_pos(&a, a.seq);
    lj_app_save_pos(&a, a.cue);
    if (lj_ini_save(a.forms_ini, path) != 0) lj_log("app: cannot save %s", path);
out:
    lj_platform_close_window(a.cue); lj_window_destroy(a.cue);
    lj_platform_close_window(a.seq); lj_window_destroy(a.seq);
    lj_platform_close_window(a.main); lj_main_window_destroy(a.main);
    lj_engine_destroy(a.engine);
    lj_ini_free(a.forms_ini);
    lj_platform_shutdown();
    return rc;
}
