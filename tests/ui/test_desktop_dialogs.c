/* test_desktop_dialogs.c - `lj4evr_tests test_desktop_dialogs`: the W4 dlg-desktop track's runnable checks, headless with a
 * recording lj_win_host (positions) and a test file-dialog host. Every DESKTOP id opens from its documented entry point
 * (docs/w4-contracts.md section 4): the desktop popup leaves (PD_AutoArrange, PD_Grid, PD_Style, PD_Image, PD_Tabs), the
 * Setup leaf Next/Previous Fixture Selection, that dialog's Set Custom Order, Save Custom Selection on an order (the Fan Out
 * sbSave seam), Auto Launch from Startup.ini at start, the hotkey Notification on Edit Hotkeys opening, the hardware
 * Notification on a config with no universe, the laser pair on a patched profile 118. ME-16: no "no binding" / "unknown
 * event" / "has no behaviour file yet" line. The three 12 Q9 prompts through assert_prompt (the UI script, headless).
 * Goldens W4DD_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_app_w4.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_fixture_icons.h"
#include "desktop/lj_selection.h"
#include "engine/lj_engine.h"
#include "gfx/lj_bmp.h"
#include "io/lj_hotkeys.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_fixicon.h"
#include "ui/widgets/lj_w_updown.h"
#include "windows/lj_desktop_dialogs.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_desktop_dialogs_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char logged[32768];
static void sink(void *ud, const char *line) { (void)ud; if (strlen(logged) + strlen(line) + 2 < sizeof logged) { strcat(logged, line); strcat(logged, "\n"); } }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static const char *cap(lj_window *w, const char *n) { const char *c = lj_w_caption(in(w, n)); return c ? c : ""; }
static const lj_engine_status *view(void *ud) { (void)ud; return &lj_ctx_get()->view; }

static int fire(lj_window *win, const char *item)   /* a menu item's handler, as lj_menu's fire() runs it */
{
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, item) : NULL;
    if (!rt || !rt->fn) { fails++; printf("FAIL fire %s: unbound\n", item); return -1; }
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}
static void key(lj_widget *w, int vk) { lj_key_ev k; memset(&k, 0, sizeof k); k.type = LJ_EV_KEY_DOWN; k.key = vk; if (w && w->vt->key) w->vt->key(w, &k); else fails++; }

/* ---- goldens (the W2D pattern) ---- */
static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(lj_window *w, const char *name)
{
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, TMP "%s.bmp", name);
    lj_window_invalidate_all(w);
    lj_window_paint(w);
    if (lj_bmp_write(lj_window_surface(w), upd && upd[0] == '1' ? path : tmp) != 0) { fails++; printf("FAIL golden %s: write\n", name); return; }
    if (upd && upd[0] == '1') { printf("  golden %s written\n", name); return; }
    rc = same_file(tmp, path);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (new render at %s)\n", name, rc == 0 ? "differs" : "missing", tmp); }
}

/* ---- a recording window host: the main window at (100,50), every move remembered (LORD_FIDELITY-12) ---- */
static struct { const lj_window *w; int x, y; } moved[32]; static int nmoved;
static int  h_open(void *ud, lj_window *w, unsigned f) { (void)ud; (void)w; (void)f; return 0; }
static void h_close(void *ud, lj_window *w) { (void)ud; (void)w; }
static void h_show(void *ud, lj_window *w, int s) { (void)ud; (void)w; (void)s; }
static void h_move(void *ud, lj_window *w, int x, int y) { (void)ud; if (nmoved < 32) { moved[nmoved].w = w; moved[nmoved].x = x; moved[nmoved].y = y; nmoved++; } }
static int  h_pos(void *ud, const lj_window *w, int *x, int *y) { (void)ud; if (w != lj_desktop_window()) return -1; *x = 100; *y = 50; return 0; }
static const lj_win_host fake_host = { NULL, h_open, h_close, h_show, h_move, h_pos };
static int last_move(const lj_window *w, int *x, int *y) { int i; for (i = nmoved - 1; i >= 0; i--) if (moved[i].w == w) { *x = moved[i].x; *y = moved[i].y; return 0; } return -1; }

/* ---- the file dialog host: a 2x2 bitmap for any path, the next pick from pick_next ---- */
static char pick_next[LJ_PATH_CAP]; static int picks;
static int t_load(const char *path, lj_surface *out) { (void)path; out->w = out->h = 2; out->px = (uint32_t *)calloc(4, sizeof *out->px); if (!out->px) return -1; out->px[0] = out->px[3] = 0xFF0000u; return 0; }
static int t_pick(int bmp_only) { (void)bmp_only; picks++; return 0; }
static int t_take(char *dst, size_t cap) { if (!pick_next[0]) return -1; snprintf(dst, cap, "%s", pick_next); pick_next[0] = 0; return 1; }
static const lj_desktop_dlg_host test_dd_host = { t_load, t_pick, t_take, NULL };

static long read_file(const char *path, char *buf, long cap) { FILE *f = fopen(path, "rb"); long n; if (!f) return -1; n = (long)fread(buf, 1, (size_t)cap - 1, f); fclose(f); buf[n > 0 ? n : 0] = 0; return n; }
static int file_has(const char *path, const char *needle) { static char buf[16384]; return read_file(path, buf, sizeof buf) > 0 && strstr(buf, needle) != NULL; }
static int run_text(const char *text) { FILE *f = fopen(TMP "dd.lj4s", "wb"); int r; uint64_t t0 = lj_clock_us(); if (!f) return -1; fputs(text, f); fclose(f);
                                        if ((r = lj_uiscript_load(TMP "dd.lj4s")) != 0) return r;
                                        while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 5000000u) { }
                                        return r; }

static lj_engine *eng; static uint64_t now_us;
static void tick(int n) { while (n-- > 0) { now_us += 25000u; lj_lock(); lj_engine_tick(eng, now_us); lj_unlock(); } }
static int out_at(int code) { const lj_frame *f = lj_engine_frame(eng); return f->out[code / LJ_LINK_CH][code % LJ_LINK_CH]; }

int lj_test_desktop_dialogs(int argc, char **argv);
int lj_test_desktop_dialogs(int argc, char **argv)
{
    static lj_library lib;
    static lj_sysprefs prefs;
    static lj_hotkeys hk;
    static lj_sacn_cfg no_universe, mapped;
    lj_ctx *c = lj_ctx_get();
    lj_window *main, *w;
    lj_widget *e;
    lj_show *show;
    char path[LJ_PATH_CAP];
    int i, x, y, order[LJ_FIXTURES * 2], n;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_library_delete(TMP, "Lib");
    remove(TMP "Startup.ini");                               /* the Auto Launch list of a previous run */
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    snprintf(c->userdir, sizeof c->userdir, TMP);
    show = lj_show_new();
    for (i = 0; i < 5; i++) { show->fixture[i].kind = LJ_FX_BUILTIN; show->fixture[i].profile_id = (uint16_t)(i < 3 ? 13 : 12); show->fixture[i].link = 1; show->fixture[i].address = (uint16_t)(1 + 20 * i); snprintf(show->fixture[i].user_name, sizeof show->fixture[i].user_name, "Fix %d", i + 1); }
    show->fixture[5].kind = LJ_FX_BUILTIN; show->fixture[5].profile_id = 118; show->fixture[5].link = 1; show->fixture[5].address = 201;   /* Mac 250 Laser, 18 channels from 201 */
    show->current_seq = lj_sequence_new_rec();
    c->lib = &lib; c->show = show; c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new(); c->current_scene = 0;
    lj_sysprefs_defaults(&prefs); c->prefs = &prefs;
    lj_hotkeys_init(&hk); c->hotkeys = &hk;
    c->engine = eng = lj_engine_create();
    lj_lock(); lj_engine_bind(eng, show); lj_unlock();
    main = lj_desktop_create();
    CHECK(main != NULL);
    if (!main) return 1;
    lj_desktop_set_library("Lib");
    lj_win_set_host(&fake_host);
    lj_desktop_dlg_set_host(&test_dd_host);
    lj_app_w4_start();
    { lj_uiscript_host h; h.ud = NULL; h.resolve = NULL; h.view = view; lj_uiscript_set_host(&h); }
    for (i = 0; i < 4; i++) CHECK(lj_desktop_place_icon(i, 300 - 60 * i, 17 * i) == 0);

    /* ---- 1. Auto-Arrange Icons from the desktop popup ---- */
    fire(main, "PD_AutoArrange");
    w = lj_win_get(LJ_WIN_AUTO_ARRANGE);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Auto-Arrange Icons") == 0 && lj_w_radiogroup_index(in(w, "RGType")) == 0);
    golden(w, "W4DD_TAUTOARRANGEFORM");
    click(in(w, "bOk"));
    CHECK(lj_win_get(LJ_WIN_AUTO_ARRANGE) == NULL);
    for (i = 0; i < 4; i++) CHECK(show->fixture[i].icon_x == 4 + i * LJ_ICON_PITCH && show->fixture[i].icon_y == 4);   /* by fixture number */
    fire(main, "PD_AutoArrange");
    click(in(lj_win_get(LJ_WIN_AUTO_ARRANGE), "bCancel"));
    CHECK(lj_win_get(LJ_WIN_AUTO_ARRANGE) == NULL);

    /* ---- 2. Desktop Grid: sizes, snap, Tabs.Ini [GRID] ---- */
    fire(main, "PD_Grid");
    w = lj_win_get(LJ_WIN_DESKTOP_GRID);
    CHECK(w && lj_w_checkbox_checked(in(w, "DisplayGrid")) == 0 && lj_w_radiobutton_checked(in(w, "SnapCenter")) == 1 && strcmp(lj_w_edit_text(in(w, "EVSize")), "8") == 0);
    golden(w, "W4DD_TGRIDFORM");
    click(in(w, "DisplayGrid")); click(in(w, "SnapFix"));
    lj_w_edit_set_text(in(w, "EVSize"), "20"); lj_w_edit_set_text(in(w, "EHSize"), "16");   /* desktopgrid.gif values */
    click(in(w, "BOk"));
    {
        lj_desktop_grid g;
        lj_desktop_grid_get(&g);
        CHECK(lj_win_get(LJ_WIN_DESKTOP_GRID) == NULL && g.on == 1 && g.snap == 1 && g.vsize == 20 && g.hsize == 16 && g.snap_center == 1);
    }
    for (i = 0; i < 4; i++) CHECK((show->fixture[i].icon_x + LJ_FIXICON_W / 2) % 16 == 0 && (show->fixture[i].icon_y + LJ_FIXICON_H / 2) % 20 == 0);
    CHECK(lj_desktop_grid_snap(0, 8, 0, 0) == 0 && lj_desktop_grid_snap(5, 8, 0, 0) == 8 && lj_desktop_grid_snap(3, 8, 0, 0) == 0 && lj_desktop_grid_snap(10, 16, 63, 1) == 17);
    CHECK(lj_library_path(&lib, "Tabs.Ini", path, sizeof path) == 0 && file_has(path, "VSize=20") && file_has(path, "HSize=16"));
    { lj_desktop_grid g = { 0, 8, 8, 0, 1 }; lj_desktop_grid_set(&g); }

    /* ---- 3. Select Icon Style ---- */
    fire(main, "PD_Style");
    w = lj_win_get(LJ_WIN_ICON_STYLE);
    CHECK(w && lj_w_radiogroup_index(in(w, "RG1")) == 0);
    golden(w, "W4DD_TICONSTYLEFORM");
    lj_w_radiogroup_set_index(in(w, "RG1"), 1);
    click(in(w, "BOk"));
    CHECK(lj_win_get(LJ_WIN_ICON_STYLE) == NULL && lj_desktop_icon_style() == 1);
    fire(main, "PD_Style");
    lj_w_radiogroup_set_index(in(lj_win_get(LJ_WIN_ICON_STYLE), "RG1"), 0);
    click(in(lj_win_get(LJ_WIN_ICON_STYLE), "BCancel"));
    CHECK(lj_win_get(LJ_WIN_ICON_STYLE) == NULL && lj_desktop_icon_style() == 1);
    lj_desktop_icon_style_set(0);

    /* ---- 4. Desktop Image: Get Image through the picker, OK writes [IMAGE]; Clear Image ---- */
    fire(main, "PD_Image");
    w = lj_win_get(LJ_WIN_DESKTOP_IMAGE);
    CHECK(w && strcmp(cap(w, "Label3"), "No Image") == 0 && lj_w_checkbox_checked(in(w, "cbStretch")) == 1);
    golden(w, "W4DD_TIMAGESETUPFORM");
    snprintf(pick_next, sizeof pick_next, "C:\\Pictures\\stage.bmp");
    click(in(w, "BGet"));
    CHECK(picks == 1);
    lj_win_refresh(LJ_WIN_DESKTOP_IMAGE);
    CHECK(strcmp(cap(w, "Label2"), "stage.bmp") == 0);
    click(in(w, "Bok"));
    CHECK(lj_win_get(LJ_WIN_DESKTOP_IMAGE) == NULL && strcmp(lj_desktop_image_file(), "C:\\Pictures\\stage.bmp") == 0 && lj_desktop_image() != NULL);
    CHECK(file_has(path, "File=C:\\Pictures\\stage.bmp"));
    fire(main, "PD_Image");
    w = lj_win_get(LJ_WIN_DESKTOP_IMAGE);
    CHECK(w && strcmp(cap(w, "Label2"), "stage.bmp") == 0);
    click(in(w, "BClear")); click(in(w, "Bok"));
    CHECK(lj_desktop_image_file()[0] == 0 && lj_desktop_image() == NULL);

    /* ---- 5. Desktop tabs: Add names the row in place, Delete unassigns that tab's icons ---- */
    fire(main, "PD_Tabs");
    w = lj_win_get(LJ_WIN_DESKTOP_TABS);
    CHECK(w && ((lj_w_listview *)in(w, "LV1"))->n == 1 && strcmp(((lj_w_listview *)in(w, "LV1"))->items[0], "Default") == 0);
    golden(w, "W4DD_TLAYERMANAGERFORM");
    click(in(w, "BAdd"));
    e = in(w, "eTabName");
    CHECK(e && e->visible && strcmp(lj_w_edit_text(e), "Tab2") == 0);
    lj_w_edit_set_text(e, "Floor");
    key(e, LJ_VK_RETURN);
    CHECK(lj_desktop_tab_count() == 2 && strcmp(lj_desktop_tab_name(1), "Floor") == 0 && !e->visible);
    CHECK(file_has(path, "TAB1=Floor") && file_has(path, "Count=2"));
    lj_desktop_tab_set(1);
    CHECK(lj_desktop_place_icon(4, 10, 10) == 0 && show->fixture[4].tab == 1);
    lj_desktop_tab_set(0);
    lj_w_listview_select(in(w, "LV1"), 1);
    click(in(w, "Bdelete"));
    CHECK(lj_desktop_tab_count() == 1 && show->fixture[4].placed == 0 && show->fixture[0].placed == 1);
    click(in(w, "BOk"));
    CHECK(lj_win_get(LJ_WIN_DESKTOP_TABS) == NULL);
    CHECK(lj_desktop_place_icon(4, 400, 4) == 0);

    /* ---- 6. Next/Previous Fixture Selection (Setup > Preferences) and 7. the custom order ---- */
    fire(main, "NextPreviousFixtureSelection1");
    w = lj_win_get(LJ_WIN_NEXTPREV_SELECTION);
    CHECK(w && lj_w_checkbox_checked(in(w, "cbEnabled")) == 1 && lj_w_radiogroup_index(in(w, "rgTabs")) == 0 && lj_w_radiogroup_index(in(w, "rgOrder")) == 0);
    golden(w, "W4DD_TNEXTPREVSELECTIONFORM");
    click(in(w, "cbEnabled"));
    CHECK(c->show_nextprev == 0 && lj_ini_geti(c->main_ini, "NEXTPREV", "Enabled", 1) == 0);
    click(in(w, "cbEnabled"));
    CHECK(c->show_nextprev == 1);
    click(in(w, "cbSelect"));                                  /* only the master's type: fixtures 1-3 are RoboColor II (13), 4-5 MAC 500 (12) */
    lj_sel_click(1, 0, 1);
    lj_sel_next(); CHECK(lj_sel_master() == 2);
    lj_sel_next(); CHECK(lj_sel_master() == 0);                /* wraps inside the type */
    click(in(w, "cbSelect"));
    fire(w, "SetCustomOrder1");
    w = lj_win_get(LJ_WIN_CUSTOM_ORDER);
    CHECK(w != NULL && ((lj_w_listview *)in(w, "LV1"))->n == 0);
    fire(w, "AddRemainingFixtures1");                          /* 1..6 */
    CHECK(lj_sel_custom_order(order, LJ_FIXTURES * 2) == 6 && order[0] == 0 && order[5] == 5);
    lj_w_listview_select(in(w, "LV1"), 2);
    fire(w, "P_InsertBreak");                                  /* 1 2 | 3 4 5 6 */
    CHECK(lj_desktop_dlg_custom_move(0, 1) == 0);              /* 2 1 | 3 4 5 6 */
    n = lj_sel_custom_order(order, LJ_FIXTURES * 2);
    CHECK(n == 7 && order[0] == 1 && order[1] == 0 && order[2] == LJ_SEL_BREAK && order[3] == 2);
    CHECK(strcmp(((lj_w_listview *)in(w, "LV1"))->items[2], "\t--- Selection Break --") == 0);
    CHECK(strstr(((lj_w_listview *)in(w, "LV1"))->items[0], "2\t") == ((lj_w_listview *)in(w, "LV1"))->items[0] && strstr(((lj_w_listview *)in(w, "LV1"))->items[0], "\tFix 2") != NULL);
    golden(w, "W4DD_TCUSTOMORDERFORM");
    lj_w_radiogroup_set_index(in(lj_win_get(LJ_WIN_NEXTPREV_SELECTION), "rgOrder"), 1);
    lj_sel_click(0, 0, 1);                                     /* master 1: the block "2 1" */
    lj_sel_next(); CHECK(lj_sel_master() == 1);
    lj_sel_next(); CHECK(lj_sel_master() == 0);
    lj_sel_click(3, 0, 1);                                     /* master 4: the block 3 4 5 6 */
    lj_sel_prev(); CHECK(lj_sel_master() == 2);
    lj_sel_prev(); CHECK(lj_sel_master() == 5);
    lj_w_listview_select(in(w, "LV1"), 2);
    fire(w, "P_RemoveBreak");
    CHECK(lj_sel_custom_order(order, LJ_FIXTURES * 2) == 6);
    fire(w, "ClearAll1");
    CHECK(lj_sel_custom_order(order, LJ_FIXTURES * 2) == 0);
    click(in(w, "sbAddMaster"));                               /* Auto-add Master Fixture */
    lj_sel_click(4, 0, 1);
    lj_win_refresh(LJ_WIN_CUSTOM_ORDER);
    CHECK(lj_sel_custom_order(order, LJ_FIXTURES * 2) == 1 && order[0] == 4);
    lj_win_close(LJ_WIN_CUSTOM_ORDER);
    lj_w_radiogroup_set_index(in(lj_win_get(LJ_WIN_NEXTPREV_SELECTION), "rgOrder"), 0);
    lj_win_close(LJ_WIN_NEXTPREV_SELECTION);
    CHECK(lj_ini_geti(c->main_ini, "NEXTPREV", "Order", -1) == 0 && strcmp(lj_ini_gets(c->main_ini, "NEXTPREV", "Custom", ""), "5") == 0);

    /* ---- 8. Save Custom Selection on an order (Fan Out sbSave) ---- */
    {
        static const int fan[3] = { 3, 1, 2 };
        w = lj_desktop_dlg_save_selection(fan, 3, 0);
        CHECK(w && strcmp(lj_window_desc(w)->caption, "Save Custom Selection") == 0 && in(w, "Bsave")->enabled == 0);
        lj_w_edit_set_text(in(w, "eName"), "Left 518s");
        click(in(w, "BSaveNew"));
        CHECK(lj_desktop_dlg_saved_selection() == 1 && lj_win_get(LJ_WIN_SAVE_SELECTION) == NULL);   /* Auto Close */
        CHECK(lj_custom_sel_count() == 1 && strcmp(lj_custom_sel_name(1), "Left 518s") == 0 && lj_custom_sel_get(1, order, LJ_FIXTURES) == 3 && order[0] == 3 && order[2] == 2);
        w = lj_desktop_dlg_save_selection(fan, 2, 1);
        CHECK(w && strcmp(lj_w_edit_text(in(w, "ENo")), "1") == 0 && in(w, "Bsave")->enabled == 1 && strcmp(lj_w_edit_text(in(w, "eName")), "Left 518s") == 0);
        golden(w, "W4DD_TSAVESELECTIONFORM");                  /* saveselectiondialog.gif: No empty there (a new one), name "Left 518s" */
        fire(w, "M_AutoClose");                                /* Auto Close off */
        click(in(w, "Bsave"));
        CHECK(lj_win_get(LJ_WIN_SAVE_SELECTION) != NULL && lj_custom_sel_count() == 1 && lj_custom_sel_get(1, order, LJ_FIXTURES) == 2);
        lj_ini_seti(c->forms_ini, "SAVESELECTION", "SaveOnEnter", 1);
        lj_w_edit_set_text(in(w, "eName"), "Right");
        key(in(w, "eName"), LJ_VK_RETURN);
        CHECK(strcmp(lj_custom_sel_name(1), "Right") == 0);
        lj_win_close(LJ_WIN_SAVE_SELECTION);
        lj_custom_sel_clear_all();
        CHECK(lj_custom_sel_count() == 0);
    }

    /* ---- 9. Startup Options: Browse adds to the Auto Launch list, Select Startup Cuelist, numbers read back; 10. Launching.. ---- */
    w = lj_win_open(LJ_WIN_STARTUP_OPTIONS);
    CHECK(w != NULL);
    snprintf(pick_next, sizeof pick_next, "C:\\Tools\\showclock.exe");
    click(in(w, "bBrowse"));
    lj_win_refresh(LJ_WIN_DESKTOP_IMAGE);                      /* the picker's answer arrives on the refresh */
    CHECK(((lj_w_list *)in(w, "lbauto"))->n == 1);
    logged[0] = 0;
    click(in(w, "ClSelect"));
    CHECK(lj_win_get(LJ_WIN_SELECT_CUELIST_MODAL) != NULL || strstr(logged, "select_cuelist_modal") != NULL);
    lj_win_close(LJ_WIN_SELECT_CUELIST_MODAL);
    lj_startup_options_set_cl(12);
    lj_w_edit_set_text(in(w, "ESeqNum"), "7");
    click(in(w, "BOk"));
    CHECK(lj_ini_geti(c->main_ini, "STARTUP", "Cl", -1) == 12 && lj_ini_geti(c->main_ini, "STARTUP", "Seq", -1) == 7);
    CHECK(file_has(TMP "Startup.ini", "File1=C:\\Tools\\showclock.exe"));
    lj_win_refresh(LJ_WIN_AUTO_LAUNCH);                        /* the start-up check: opens with the list */
    w = lj_win_get(LJ_WIN_AUTO_LAUNCH);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Launching..") == 0);
    lj_win_refresh(LJ_WIN_AUTO_LAUNCH);
    CHECK(w && strcmp(((lj_w_listview *)in(w, "lv1"))->items[0], "C:\\Tools\\showclock.exe\tNot launched") == 0);
    golden(w, "W4DD_TAUTOLAUNCHFORM");
    click(in(w, "bCancel"));
    CHECK(lj_win_get(LJ_WIN_AUTO_LAUNCH) == NULL);

    /* ---- 11. 'Notification !' on Edit Hotkeys, "Don't show this warning again" ---- */
    lj_win_open(LJ_WIN_EDIT_HOTKEYS);
    lj_win_refresh(LJ_WIN_HOTKEY_WARNING);
    w = lj_win_get(LJ_WIN_HOTKEY_WARNING);
    CHECK(w && lj_win_message_window() == w && strcmp(lj_window_desc(w)->caption, "Notification !") == 0 && strcmp(cap(w, "Label2"), "suspended when the Hotkey Dialog") == 0);
    golden(w, "W4DD_TSHOWHOTKEYWARNINGFORM");
    click(in(w, "cbShow")); click(in(w, "BitBtn1"));
    CHECK(lj_win_get(LJ_WIN_HOTKEY_WARNING) == NULL && prefs.show_hotkey_warning == 0 && lj_ini_geti(c->main_ini, "MISC", "ShowHotkeyWarning", 1) == 0);
    lj_win_close(LJ_WIN_EDIT_HOTKEYS); lj_win_refresh(LJ_WIN_HOTKEY_WARNING);
    lj_win_open(LJ_WIN_EDIT_HOTKEYS); lj_win_refresh(LJ_WIN_HOTKEY_WARNING);
    CHECK(lj_win_get(LJ_WIN_HOTKEY_WARNING) == NULL);
    lj_win_close(LJ_WIN_EDIT_HOTKEYS);

    /* ---- 12. 'Notification !' with no sACN universe mapped; none once one is ---- */
    c->sacn = &no_universe;
    lj_win_refresh(LJ_WIN_HARDWARE_WARNING);
    w = lj_win_get(LJ_WIN_HARDWARE_WARNING);
    CHECK(w && strcmp(cap(w, "Label1"), "No Transmitterhardware configured.") == 0);
    CHECK(last_move(w, &x, &y) == 0 && x == 100 + (lj_window_surface(main)->w - lj_window_surface(w)->w) / 2 && y == 50 + (lj_window_surface(main)->h - lj_window_surface(w)->h) / 2);   /* LORD_FIDELITY-12 */
    golden(w, "W4DD_TSHOWHARDWAREWARNINGFORM");
    CHECK(run_text("assert_prompt \"Notification !\"\nassert_prompt \"No Transmitterhardware configured.\"\nclick hardware_warning.BitBtn1\nquit\n") == 0);
    CHECK(lj_win_get(LJ_WIN_HARDWARE_WARNING) == NULL);
    mapped.universe_of_link[0] = 1; c->sacn = &mapped;
    lj_win_refresh(LJ_WIN_HARDWARE_WARNING);
    CHECK(lj_win_get(LJ_WIN_HARDWARE_WARNING) == NULL);
    c->sacn = NULL;

    /* ---- 13. lasers: blocked at start, 'Enable laser output ?' on demand, 'Warning!' while emitting, click blocks ---- */
    CHECK(lj_laser_fixture(5) == 1 && lj_laser_fixture(0) == 0 && lj_show_profile(show, 5)->footprint == 18);   /* profile 118 keeps its PROPOSAL footprint */
    lj_lock(); show->current_seq->scenes[0].val[200] = 200; show->current_seq->scenes[0].state[200] = LJ_SNAP; lj_unlock();
    lj_ctx_set_scene(0);
    tick(4);
    lj_win_refresh(LJ_WIN_LASER_WARNING);                      /* the first poll posts the block and asks */
    tick(2);
    w = lj_win_get(LJ_WIN_UNBLOCK_LASERS);
    CHECK(w && lj_laser_enabled() == 0 && out_at(200) == 0 && lj_engine_frame(eng)->merged[200] == 200);
    CHECK(w && strcmp(cap(w, "Label1"), "Enable laser output ?") == 0 && strcmp(lj_window_desc(w)->caption, "Please confirm") == 0);
    golden(w, "W4DD_TUNBLOCKLASERSFORM");
    CHECK(run_text("assert_prompt \"Enable laser output ?\"\nclick unblock_lasers.BitBtn2\nquit\n") == 0);   /* No: stays blocked, not asked again */
    lj_win_refresh(LJ_WIN_LASER_WARNING); tick(2);
    CHECK(lj_win_get(LJ_WIN_UNBLOCK_LASERS) == NULL && out_at(200) == 0);
    lj_lock(); show->current_seq->scenes[0].val[200] = 0; lj_unlock(); lj_ctx_set_scene(0); tick(3);
    lj_win_refresh(LJ_WIN_LASER_WARNING);                      /* the demand dropped: re-armed */
    lj_lock(); show->current_seq->scenes[0].val[200] = 200; lj_unlock(); lj_ctx_set_scene(0); tick(3);
    lj_win_refresh(LJ_WIN_LASER_WARNING);
    CHECK(lj_win_get(LJ_WIN_UNBLOCK_LASERS) != NULL);
    click(in(lj_win_get(LJ_WIN_UNBLOCK_LASERS), "BitBtn1")); /* Yes */
    tick(2);
    lj_win_refresh(LJ_WIN_LASER_WARNING);
    w = lj_win_get(LJ_WIN_LASER_WARNING);
    CHECK(lj_laser_enabled() == 1 && out_at(200) == 200 && w && strcmp(lj_window_desc(w)->caption, "Warning!") == 0);
    CHECK(w && strcmp(cap(w, "Label1"), "MAC250L emitting!") == 0 && strcmp(cap(w, "Label2"), "(click to disable)") == 0);
    CHECK(last_move(w, &x, &y) == 0 && x == 100 + (lj_window_surface(main)->w - lj_window_surface(w)->w) / 2);
    golden(w, "W4DD_TLASERWARNINGFORM");
    CHECK(run_text("assert_prompt \"Warning!\"\nclick laser_warning.Label1\nquit\n") == 0);   /* "(click to disable)" */
    tick(2);
    lj_win_refresh(LJ_WIN_LASER_WARNING);                      /* the box goes on the next poll (it was clicked on mouse down) */
    CHECK(lj_win_get(LJ_WIN_LASER_WARNING) == NULL && lj_laser_enabled() == 0 && out_at(200) == 0);

    /* ---- 14. (W4-WIN: the default_output id is deleted, fidelity row); LORD_FIDELITY-12 source tags; ME-16 ---- */
    logged[0] = 0;
    for (i = 0; i < LJ_WIN_COUNT; i++) CHECK(strcmp(lj_win_id_name((enum lj_win)i), "default_output") != 0 && strcmp(lj_win_id_name((enum lj_win)i), "save_preset") != 0);
    for (i = 0; i < LJ_WARN_COUNT; i++) CHECK(lj_warnings_status_text((enum lj_warning)i)[0] && lj_warnings_status_source((enum lj_warning)i)[0]);
    for (i = LJ_WIN_AUTO_ARRANGE; i <= LJ_WIN_UNBLOCK_LASERS; i++) {   /* every id of the family opens by id too */
        CHECK(lj_win_open((enum lj_win)i) != NULL);
        lj_win_close((enum lj_win)i);
        CHECK(lj_win_get((enum lj_win)i) == NULL);
    }
    CHECK(strstr(logged, "no binding") == NULL && strstr(logged, "unknown event") == NULL && strstr(logged, "has no behaviour file yet") == NULL);
    if (strstr(logged, "no binding") || strstr(logged, "unknown event")) printf("%s", logged);

    lj_win_close_all();
    lj_warnings_close_all();
    lj_uiscript_set_host(NULL);
    lj_desktop_dlg_set_host(NULL);
    lj_win_set_host(NULL);
    lj_desktop_destroy(main);
    lj_lock(); lj_engine_bind(eng, NULL); c->engine = NULL; lj_unlock();
    lj_engine_destroy(eng);
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(show); c->show = NULL; c->lib = NULL; c->prefs = NULL; c->hotkeys = NULL; c->userdir[0] = 0;
    lj_log_set_sink(NULL, NULL);
    printf("desktop_dialogs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
