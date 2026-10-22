/* test_desktop_w2.c - the runnable check for the W2 desktop track (docs/w2-contracts.md, docs/w2-gates.md):
 *  1. the Cue list toolbar order equals F8 [01 s2.3.2, 07 s4.4]: names, left-to-right x, hints, the two LED captions;
 *  2. the Background Cue / Statics LEDs and the Smoke latch follow a synthetic engine view (one tick each), and a
 *     direct lj_desktop_led_set holds until the next view tick;
 *  3. ME-29: the Fixture toolbar gates and the three bars follow lj_sysprefs read from a Main.Ini document (Solo hidden,
 *     OnLine/OffLine and Freeze greyed), and the record round-trips through the same document;
 *  4. every W2 menu leaf is bound, enabled and not inert, and firing it reaches its window through lj_windows.h (the
 *     window opens, or lj_win_open logs that its track has not landed); the toolbar buttons BG / Statics / Smoke /
 *     MIDI In / Groups the same; Default DMX Output Clear zeroes the table; Setup > Hotkeys lists the saved setups,
 *     loads the one picked and the status bar shows its number;
 *  5. the memory gauge text and colour band (ME-87);
 *  6. --shot <png>: the C-L toolbar row with both LEDs lit (docs/screenshots/w2/).
 * Usage: lj4evr_tests test_desktop_w2 [--shot <png>] */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define LJ_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define LJ_MKDIR(p) mkdir(p, 0777)
#endif
#include "miniz.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_statusbar.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_prefs.h"
#include "windows/lj_windows.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char lj_t_log[8192]; static size_t lj_t_log_n;
static void sink(void *ud, const char *line)
{
    size_t n = strlen(line);
    (void)ud;
    printf("  log: %s\n", line);
    if (lj_t_log_n + n + 2 < sizeof lj_t_log) { memcpy(lj_t_log + lj_t_log_n, line, n); lj_t_log_n += n; lj_t_log[lj_t_log_n++] = '\n'; lj_t_log[lj_t_log_n] = 0; }
}
static void log_reset(void) { lj_t_log_n = 0; lj_t_log[0] = 0; }

/* the leaf reached its window: open now, or lj_win_open said "<id> has no behaviour file yet" */
static int reached(enum lj_win id)
{
    char needle[96];
    snprintf(needle, sizeof needle, "lj_win_open: %s ", lj_win_id_name(id));
    return lj_win_get(id) != NULL || strstr(lj_t_log, needle) != NULL;
}

static void fire(lj_window *win, const char *name)   /* what lj_menu's fire() does for a click on the item */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = lj_menu_rt_named(m, name);
    if (!rt || !rt->fn) { fails++; printf("FAIL fire %s: unbound\n", name); return; }
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
}

static void click(lj_window *win, const char *name)
{
    lj_widget *w = lj_window_find(win, name);
    if (!w || !w->on_event) { fails++; printf("FAIL click %s: no widget\n", name); return; }
    w->on_event(w, LJ_WEV_CLICK, w->ud);
}

static int led(lj_window *win, const char *name) { lj_widget *w = lj_window_find(win, name); return w ? lj_w_led_on(w) : -1; }
static int visible(lj_window *win, const char *name) { lj_widget *w = lj_window_find(win, name); return w ? (int)w->visible : -1; }
static int enabled(lj_window *win, const char *name) { lj_widget *w = lj_window_find(win, name); return w ? (int)w->enabled : -1; }

static int write_png(const lj_surface *s, int x, int y, int w, int h, const char *path)
{
    unsigned char *rgb = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    size_t len = 0;
    void *png;
    FILE *f;
    int i, j, rc = -1;
    if (!rgb) return -1;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        uint32_t p = s->px[(y + j) * s->w + x + i];
        unsigned char *o = rgb + ((size_t)j * (size_t)w + (size_t)i) * 3;
        o[0] = (unsigned char)(p >> 16); o[1] = (unsigned char)(p >> 8); o[2] = (unsigned char)p;
    }
    png = tdefl_write_image_to_png_file_in_memory_ex(rgb, w, h, 3, &len, 6, MZ_FALSE);
    free(rgb);
    if (!png) return -1;
    if ((f = fopen(path, "wb")) != NULL) { rc = fwrite(png, 1, len, f) == len ? 0 : -1; fclose(f); }
    mz_free(png);
    return rc;
}

int lj_test_desktop_w2(int argc, char **argv);
int lj_test_desktop_w2(int argc, char **argv)
{
    static const char *const f8_hints[12] = { "Toggle Cue List control", "New/Clear Cue List", "Save Cue List", "List of Cue Lists", "Background Cue", "Statics",
                                              "CD Control", "MC-X", "2518 Controller", "DMX In", "MIDI In", "2532 Direct Access" };   /* F8 / [01 s2.3.2] 1..13 without the C-L field */
    static const struct { const char *leaf; enum lj_win win; } leaves[] = {   /* the W2 entry points of the main menu and the desktop popup [01 s2.2, s3.5] */
        { "M_UserLibraries", LJ_WIN_USER_LIBRARIES }, { "M_DeleteSequences", LJ_WIN_DELETE_SEQ }, { "M_DeleteCues", LJ_WIN_DELETE_CUE },
        { "M_DeleteCuelists", LJ_WIN_DELETE_CL }, { "M_DeleteBGCues", LJ_WIN_DELETE_BG }, { "M_DeleteGenericMacros", LJ_WIN_DELETE_MACRO },
        { "AutoStrikeLamps1", LJ_WIN_STRIKING_LAMPS }, { "GlobalPatch1", LJ_WIN_GLOBAL_PATCH }, { "SystemPrefs", LJ_WIN_SYSTEM_PREFS },
        { "M_EditHotkeys", LJ_WIN_EDIT_HOTKEYS }, { "M_ViewBackgroundCue", LJ_WIN_BGCUE_FORM }, { "M_ViewStatics", LJ_WIN_STATICS2_FORM },
        { "M_ViewNotes", LJ_WIN_NOTES }, { "M_About", LJ_WIN_ABOUT }, { "PD_FixtureGroups", LJ_WIN_FIXTURE_GROUPS },
    };
    static const char *const other_leaves[] = { "CurrentScene1", "Clear1", "M_HotkeysEnabled", "PI_GlobalPatch", "M_HotkeySetup1" };
    static const struct { const char *button; enum lj_win win; } buttons[] = {
        { "tb_cuelist_5", LJ_WIN_BGCUE_FORM }, { "tb_cuelist_6", LJ_WIN_STATICS2_FORM }, { "tb_cuelist_11", LJ_WIN_MIDI_IN },
        { "tb_fixture_11", LJ_WIN_FIXTURE_GROUPS }, { "tb_fixture_21", LJ_WIN_SMOKE_CONTROL },
    };
    static lj_hotkeys hk, saved;
    const char *shot = NULL;
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_sysprefs prefs, back;
    lj_ini *ini;
    lj_window *win;
    lj_menu *m;
    lj_widget *w;
    char dir[512], path[600];
    int i, ninert, prev_x = -1;
    const lj_main_inert_leaf *inert = lj_main_menu_inert_table(&ninert);

    for (i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--shot") == 0) shot = argv[i + 1];
    lj_log_set_sink(sink, NULL);
    lj_hotkeys_init(&hk);
    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->show = show; ctx->lib = NULL; ctx->main_ini = NULL; ctx->prefs = NULL; ctx->hotkeys = &hk; ctx->userdir[0] = 0; ctx->engine = NULL;
    ctx->allow_freeze = ctx->allow_offline = ctx->allow_solo = ctx->show_nextprev = 1;
    win = lj_desktop_create();
    CHECK(win != NULL);
    if (!win) { lj_show_free(show); return 1; }
    lj_desktop_set_library("Default");
    m = lj_window_menus(win);

    /* ---- 1. F8 order ---- */
    for (i = 0; i < 12; i++) {
        char nm[24];
        snprintf(nm, sizeof nm, "tb_cuelist_%d", i + 1);
        w = lj_window_find(win, nm);
        CHECK(w != NULL);
        if (!w) continue;
        CHECK(w->x > prev_x); prev_x = w->x;
        CHECK(w->desc && w->desc->hint && strcmp(w->desc->hint, f8_hints[i]) == 0);
    }
    w = lj_window_find(win, "tb_cuelist_5"); CHECK(w && strcmp(((lj_w_led *)w)->caption, "Background Cue") == 0 && w->w == 110);   /* backgroundcueicon.gif 109x24 */
    w = lj_window_find(win, "tb_cuelist_6"); CHECK(w && strcmp(((lj_w_led *)w)->caption, "Statics") == 0 && w->w == 90);           /* staticcontrollicon.gif 90x25 */
    w = lj_window_find(win, "tb_cllabel"); CHECK(w && w->x < lj_window_find(win, "tb_clname")->x && lj_window_find(win, "tb_clname")->x < lj_window_find(win, "tb_cuelist_1")->x);   /* C-L, name field, then the buttons */

    /* ---- 2. LEDs follow the view ---- */
    lj_desktop_toolbar_update();
    CHECK(led(win, "tb_cuelist_5") == 0 && led(win, "tb_cuelist_6") == 0);   /* no view yet: red */
    ctx->view.tick_no = 1; ctx->view.bg_any = 1; ctx->view.statics_any = 0; ctx->view.smoke_active = 1;
    lj_desktop_toolbar_update();
    CHECK(led(win, "tb_cuelist_5") == 1 && led(win, "tb_cuelist_6") == 0);
    CHECK(lj_desktop_led_get(LJ_LED_BGCUE) == 1 && lj_desktop_led_get(LJ_LED_STATICS) == 0 && lj_desktop_led_get(LJ_LED_SMOKE) == 1);
    CHECK(lj_w_speedbutton_down(lj_window_find(win, "tb_fixture_21")) == 1);   /* the Smoke button latches */
    lj_desktop_led_set(LJ_LED_STATICS, 1);                                    /* a window lights it after posting ... */
    lj_desktop_toolbar_update();
    CHECK(led(win, "tb_cuelist_6") == 1);                                     /* ... and the same view tick does not undo it */
    ctx->view.tick_no = 2; ctx->view.bg_any = 0; ctx->view.statics_any = 1; ctx->view.smoke_active = 0;
    lj_desktop_toolbar_update();
    CHECK(led(win, "tb_cuelist_5") == 0 && led(win, "tb_cuelist_6") == 1 && lj_w_speedbutton_down(lj_window_find(win, "tb_fixture_21")) == 0);
    ctx->view.tick_no = 3; ctx->view.statics_any = 0;
    lj_desktop_toolbar_update();
    CHECK(led(win, "tb_cuelist_6") == 0);

    /* ---- 3. ME-29 gates from Main.Ini ---- */
    ini = lj_ini_new();
    lj_sysprefs_load(ini, &prefs);                                          /* an empty document = the shipped file: AllowBlind 0, AllowOffline 0, [SOLO] Enabled 1 */
    ctx->prefs = &prefs;
    lj_desktop_toolbar_gates(&prefs);
    CHECK(visible(win, "tb_fixture_17") == 1 && enabled(win, "tb_fixture_19") == 0 && enabled(win, "tb_fixture_20") == 0);
    lj_ini_seti(ini, "PREFS", "AllowBlind", 1); lj_ini_seti(ini, "PREFS", "AllowOffline", 1); lj_ini_seti(ini, "SOLO", "Enabled", 0);
    lj_ini_seti(ini, "PREFS", "ViewClBar", 0); lj_ini_seti(ini, "PREFS", "ViewStatusBar", 0);
    lj_sysprefs_load(ini, &prefs);
    lj_desktop_toolbar_gates(&prefs);
    CHECK(visible(win, "tb_fixture_17") == 0 && enabled(win, "tb_fixture_19") == 1 && enabled(win, "tb_fixture_20") == 1);   /* Solo hidden, not greyed */
    CHECK(lj_desktop_toolbar_visible(LJ_TB_CUELIST) == 0 && lj_desktop_toolbar_visible(LJ_TB_SEQCUE) == 1 && lj_statusbar_visible() == 0);
    CHECK(lj_menu_rt_named(m, "CueListToolbar1") && !lj_menu_rt_named(m, "CueListToolbar1")->checked);
    lj_desktop_toolbar_show(LJ_TB_CUELIST, 1); lj_statusbar_show(1);         /* View > Toolbars writes the record the app saves */
    CHECK(prefs.view_cuelist_bar == 1 && prefs.view_status_bar == 1);
    lj_sysprefs_save(ini, &prefs);
    lj_sysprefs_load(ini, &back);
    CHECK(memcmp(&prefs, &back, sizeof prefs) == 0);
    CHECK(lj_ini_geti(ini, "PREFS", "AllowBlind", -1) == 1 && lj_ini_geti(ini, "SOLO", "Enabled", -1) == 0 && lj_ini_geti(ini, "PREFS", "ViewClBar", -1) == 1);
    prefs.allow_solo = 1; lj_desktop_toolbar_gates(&prefs);
    CHECK(visible(win, "tb_fixture_17") == 1);
    lj_ini_free(ini);

    /* ---- 4. W2 leaves ---- */
    for (i = 0; i < LJ_COUNT_OF(leaves); i++) {
        lj_menu_rt *rt = lj_menu_rt_named(m, leaves[i].leaf);
        int k, is_inert = 0;
        CHECK(rt && rt->fn && rt->enabled);
        for (k = 0; k < ninert; k++) is_inert |= strcmp(inert[k].name, leaves[i].leaf) == 0;
        CHECK(!is_inert);   /* the "Not available in LJ4EVR" hint stays on the dropped / W5 leaves */
        log_reset();
        fire(win, leaves[i].leaf);
        if (!reached(leaves[i].win)) { fails++; printf("FAIL leaf %s did not reach %s\n", leaves[i].leaf, lj_win_id_name(leaves[i].win)); }
        lj_win_close(leaves[i].win);
    }
    for (i = 0; i < LJ_COUNT_OF(other_leaves); i++) { lj_menu_rt *rt = lj_menu_rt_named(m, other_leaves[i]); CHECK(rt && rt->fn && rt->enabled); }
    for (i = 0; i < LJ_COUNT_OF(buttons); i++) {
        log_reset();
        click(win, buttons[i].button);
        if (!reached(buttons[i].win)) { fails++; printf("FAIL button %s did not reach %s\n", buttons[i].button, lj_win_id_name(buttons[i].win)); }
        lj_win_close(buttons[i].win);
    }
    show->default_out[5] = 7;
    fire(win, "Clear1");
    CHECK(show->default_out[5] == 0);                                         /* Clear Default Output [01 s6.3] */
    fire(win, "M_HotkeysEnabled");
    CHECK(hk.enabled == 0 && prefs.hotkeys_enabled == 0 && strcmp(lj_statusbar_hotkeys_text(), "Hotkeys Off") == 0);
    CHECK(!lj_menu_rt_named(m, "M_HotkeysEnabled")->checked);
    fire(win, "M_HotkeysEnabled");
    CHECK(hk.enabled == 1 && lj_menu_rt_named(m, "M_HotkeysEnabled")->checked);

    snprintf(dir, sizeof dir, "lj_test_desktop_w2_lib");                      /* <userdir>Hotkeys/Setup2.Hot, name "Playback Keys" (hotkeys2.gif) */
    LJ_MKDIR(dir);
    snprintf(path, sizeof path, "%s/Hotkeys", dir); LJ_MKDIR(path);
    snprintf(ctx->userdir, sizeof ctx->userdir, "%s/", dir);
    lj_hotkeys_init(&saved); lj_str_copy(saved.name, sizeof saved.name, "Playback Keys");
    { lj_hotkey k; memset(&k, 0, sizeof k); k.key = 66; k.shift = 1; k.fn = LJ_FN_BLACKOUT_RESTORE; CHECK(lj_hotkeys_assign(&saved, &k) == 0); }
    CHECK(lj_hotkeys_setup_path(ctx->userdir, 2, path, sizeof path) == 0 && lj_hotkeys_save(path, &saved) == 0);
    log_reset();
    lj_main_menu_sync(win);
    CHECK(strstr(lj_t_log, "cannot open") == NULL);                            /* missing setups are probed, not loaded (w1-int s7: no "cannot open .../SetupN.Hot" line) */
    CHECK(lj_menu_rt_named(m, "M_HotkeySetup1")->visible && lj_menu_rt_named(m, "M_HotkeySetup1")->checked);   /* the current setup is always listed */
    CHECK(lj_menu_rt_named(m, "M_HotkeySetup2")->visible && strcmp(lj_menu_rt_named(m, "M_HotkeySetup2")->item->caption, "Playback Keys") == 0);
    CHECK(!lj_menu_rt_named(m, "M_HotkeySetup3")->visible);
    fire(win, "M_HotkeySetup2");
    CHECK(strcmp(hk.name, "Playback Keys") == 0 && hk.n == 1 && prefs.hotkey_setup == 2);
    CHECK(lj_menu_rt_named(m, "M_HotkeySetup2")->checked && !lj_menu_rt_named(m, "M_HotkeySetup1")->checked);
    CHECK(strcmp(lj_statusbar_hotkeys_text(), "Hotkeys 2") == 0);
    remove(path);
    snprintf(path, sizeof path, "%s/Hotkeys", dir); remove(path);
#ifdef _WIN32
    _rmdir(path); _rmdir(dir);
#else
    remove(dir);
#endif
    ctx->userdir[0] = 0;

    /* ---- 5. memory gauge (ME-87) ---- */
    lj_desktop_memory_set((uint64_t)3 << 30, (uint64_t)8 << 30);
    CHECK(strcmp(lj_statusbar_panel_text(LJ_SB_MEMORY), "37%") == 0);
    lj_desktop_memory_set((uint64_t)3 << 19, (uint64_t)8 << 30);              /* 1.5 MB free: orange */
    CHECK(strcmp(lj_statusbar_panel_text(LJ_SB_MEMORY), "0%") == 0);

    /* ---- 6. the C-L toolbar with both LEDs lit ---- */
    ctx->view.tick_no = 4; ctx->view.bg_any = 1; ctx->view.statics_any = 1; ctx->view.smoke_active = 0;
    lj_desktop_toolbar_update();
    lj_window_paint(win);
    {
        const lj_surface *s = lj_window_surface(win);
        int ax, ay, n = 0, j, k;
        w = lj_window_find(win, "tb_row_cuelist");
        lj_widget_abs(w, &ax, &ay);
        for (j = ay; j < ay + w->h; j++) for (k = ax; k < ax + w->w; k++) n += (s->px[j * s->w + k] & 0xFFFFFFu) == LJ_LED_ON_RGB;
        CHECK(n >= 2 * 60);   /* two lit dots (69 green px each, lj_w_led.c) */
        if (shot) { CHECK(write_png(s, ax, ay, w->w, w->h, shot) == 0); printf("  wrote %s (%dx%d)\n", shot, w->w, w->h); }
    }

    lj_desktop_destroy(win);
    ctx->show = NULL; ctx->prefs = NULL; ctx->hotkeys = NULL;
    memset(&ctx->view, 0, sizeof ctx->view);
    lj_show_free(show);
    lj_log_set_sink(NULL, NULL);
    printf("test_desktop_w2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
