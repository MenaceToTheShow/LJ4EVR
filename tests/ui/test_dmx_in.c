/* test_dmx_in.c - the runnable check for src/windows/lj_dmx_in.c and the W4 hw-dmxin sweep (W4-DMX, W4-HW-0, F8, I7, I13):
 *  1. Dmx In opens from the C-L toolbar DMX In button (LJ_ACT_DMX_IN) with its DFM caption, and from the Global Patch DMX In
 *     source (double-click on eSourceIn, PROPOSAL); every input control disabled with the hint "Not available in LJ4EVR",
 *     Close and Value-translations live, every assignment menu item greyed; ME-16 clean;
 *  2. Value-translations opens DMX-In translation; its popup Create new table opens Edit tranlation table (sic) with a 1:1 grid,
 *     every field greyed; Cancel closes it; the import-error and export windows open by id with their inputs greyed;
 *  3. Hardware Setup: the dropped-hardware tabs (USB interfaces, Audio, Peripheral devices) greyed with the hint, a DFM hint
 *     kept, the MIDI tab live;
 *  4. goldens W4DMX_*.bmp (LJ_GOLDEN_UPDATE=1 rewrites them; reviewed in REVIEWED.tsv). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_dmx_in.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails, me16;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void sink(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "no binding for handler") || strstr(line, "matches no DFM handler") || strstr(line, "unknown event") || strstr(line, "has no behaviour file")) { me16++; printf("  ME-16: %s\n", line); }
}
static void on_dmx_in(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)arg; (void)ud; lj_win_open(LJ_WIN_DMX_IN); }   /* = app/lj_app_w4.c */

static void mouse(lj_window *win, int type, int x, int y, int clicks)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.x = x; ev.u.mouse.y = y; ev.u.mouse.button = 1; ev.u.mouse.clicks = clicks;
    lj_window_event(win, &ev);
}
static void click(lj_window *win, const char *name, int clicks)
{
    lj_widget *w = win ? lj_window_find(win, name) : NULL;
    int ax, ay;
    if (!w) { fails++; printf("FAIL no widget %s\n", name); return; }
    lj_widget_abs(w, &ax, &ay);
    mouse(win, LJ_EV_MOUSE_DOWN, ax + w->w / 2, ay + w->h / 2, clicks);
    mouse(win, LJ_EV_MOUSE_UP, ax + w->w / 2, ay + w->h / 2, clicks);
}
static int fire(lj_window *win, const char *item)   /* runs a menu item's bound handler as lj_menu's fire() does */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, item) : NULL;
    if (!rt || !rt->fn) { printf("  menu item %s: not bound\n", item); return -1; }
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}
static int greyed(lj_window *win, const char *n)   /* disabled with the hint (or the DFM's own hint) */
{
    lj_widget *w = win ? lj_window_find(win, n) : NULL;
    if (!w) { printf("  no widget %s\n", n); return 0; }
    return !w->enabled && w->hint && w->hint[0];
}
static int na(lj_window *win, const char *n) { lj_widget *w = win ? lj_window_find(win, n) : NULL; return w && !w->enabled && w->hint && strcmp(w->hint, LJ_DMX_IN_NA) == 0; }
static int live(lj_window *win, const char *n) { lj_widget *w = win ? lj_window_find(win, n) : NULL; return w && w->enabled; }
static int menu_en(lj_window *win, const char *n) { lj_menu_rt *rt = win ? lj_menu_rt_named(lj_window_menus(win), n) : NULL; return rt ? (int)rt->enabled : -1; }

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(lj_window *win, const char *name)
{
    char g[1024], tmp[1024];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (!win) { fails++; printf("FAIL golden %s: window not open\n", name); return; }
    snprintf(g, sizeof g, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, "lj_test_dmx_in_%s.bmp", name);
    lj_window_paint(win);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(win), g) == 0); printf("  %s: written\n", g); return; }
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    rc = same_file(tmp, g);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (the render is %s)\n", name, rc ? "missing or unreadable" : "differs", tmp); return; }
    printf("  %s: identical\n", name);
    remove(tmp);
}

int lj_test_dmx_in(int argc, char **argv);
int lj_test_dmx_in(int argc, char **argv)
{
    static const char *const di_inputs[] = { "LVDmxFunctions", "LVFunctions", "bClear", "bAssign", "bSave", "eCue", "eCuename" };
    static const char *const di_menu_off[] = { "M_DisableDMXinFunc", "ClearAllAssignments1", "ClearCueLacthassignments1", "ClearCueonCue01",
                                               "Pop_Clear", "Pop_Seq", "Pop_Cue", "Pop_Cuelist", "Pop_BGCue" };
    static const char *const tr_inputs[] = { "lvChannels", "sbUp", "sbDown", "re1" };
    static const char *const ed_inputs[] = { "sg1", "eVF", "eVT", "eTF", "eTT", "bSet", "eID", "eName", "ePurpose", "eType", "BitBtn1", "BitBtn3" };
    static const char *const hw_dropped[] = { "cbFunction1A", "RGAudio", "rgAudioCD", "rgDigitalAudio", "eWinamp", "sbWinamp",
                                              "RG2532Enable", "rg2532Comport", "rg2518Enable", "rg2518Comport", "cb2518Version", "cb_MCX" };
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_window *main_win, *di, *tr, *ed, *w;
    size_t i;
    (void)argc; (void)argv;

    lj_log_set_sink(sink, NULL);
    ctx->show = show; ctx->lib = NULL; ctx->main_ini = NULL;
    lj_win_set_host(NULL);
    lj_desktop_bind_action(LJ_ACT_DMX_IN, on_dmx_in, NULL);

    /* ---- 1. the C-L toolbar entry point (F8: DMX In opens; CD / MC-X / 2518 greyed with the hint) ---- */
    main_win = lj_desktop_create();
    CHECK(main_win != NULL);
    lj_desktop_toolbar_update();
    CHECK(live(main_win, "tb_cuelist_10"));
    CHECK(na(main_win, "tb_cuelist_7") && na(main_win, "tb_cuelist_8") && na(main_win, "tb_cuelist_9"));
    click(main_win, "tb_cuelist_10", 1);
    di = lj_win_get(LJ_WIN_DMX_IN);
    CHECK(di != NULL);
    if (di) {
        CHECK(strcmp(lj_window_caption(di)->text, "Dmx In") == 0);
        for (i = 0; i < sizeof di_inputs / sizeof di_inputs[0]; i++) { if (!na(di, di_inputs[i])) printf("  Dmx In %s not greyed with the hint\n", di_inputs[i]); CHECK(na(di, di_inputs[i])); }
        CHECK(live(di, "bcancel"));
        for (i = 0; i < sizeof di_menu_off / sizeof di_menu_off[0]; i++) CHECK(menu_en(di, di_menu_off[i]) == 0);
        CHECK(menu_en(di, "M_ValueTranslations") == 1 && menu_en(di, "Help1") == 1);
        CHECK(strcmp(lj_w_edit_text(lj_window_find(di, "eCue")), "N/A") == 0);
        golden(di, "W4DMX_DMX_IN");
        click(di, "bcancel", 1);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) == NULL);
    }

    /* ---- the Global Patch entry point: double-click on the DMX In source field ---- */
    w = lj_win_open(LJ_WIN_GLOBAL_PATCH);
    CHECK(w != NULL);
    if (w) {
        CHECK(menu_en(w, "P_InSeq") == 0 && menu_en(w, "P_EnableUse") == 0 && menu_en(w, "P_EnableIgnore") == 0);
        click(w, "eSourceIn", 1);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) == NULL);   /* a single click edits the field */
        click(w, "eSourceIn", 2);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) != NULL);
        lj_win_close(LJ_WIN_GLOBAL_PATCH);
    }

    /* ---- 2. Value-translations -> DMX-In translation -> Create new table -> Edit tranlation table ---- */
    di = lj_win_get(LJ_WIN_DMX_IN);
    if (di) CHECK(fire(di, "M_ValueTranslations") == 0);
    tr = lj_win_get(LJ_WIN_DMXIN_TRANSLATION);
    CHECK(tr != NULL);
    if (tr) {
        CHECK(strcmp(lj_window_caption(tr)->text, "DMX-In translation") == 0);
        CHECK(strcmp(lj_w_caption(lj_window_find(tr, "lActive")), "0 active tranlation(s)") == 0);
        for (i = 0; i < sizeof tr_inputs / sizeof tr_inputs[0]; i++) CHECK(greyed(tr, tr_inputs[i]));
        CHECK(live(tr, "bOk") && live(tr, "bCancel") && live(tr, "tv1"));
        CHECK(menu_en(tr, "M_UpdateActiveTranslations") == 0 && menu_en(tr, "M_ClearAll") == 0 && menu_en(tr, "Pop_ClearTranslation") == 0 && menu_en(tr, "pop_NewTable") == 1);
        golden(tr, "W4DMX_TRANSLATION");
        CHECK(fire(tr, "pop_NewTable") == 0);
    }
    ed = lj_win_get(LJ_WIN_TRANSLATION_EDIT);
    CHECK(ed != NULL);
    if (ed) {
        CHECK(strcmp(lj_window_caption(ed)->text, "Edit tranlation table") == 0);   /* sic (D13) */
        for (i = 0; i < sizeof ed_inputs / sizeof ed_inputs[0]; i++) { if (!na(ed, ed_inputs[i])) printf("  Edit table %s not greyed with the hint\n", ed_inputs[i]); CHECK(na(ed, ed_inputs[i])); }
        CHECK(live(ed, "bOk") && live(ed, "BitBtn4"));
        golden(ed, "W4DMX_TRANSLATION_EDIT");
        click(ed, "BitBtn4", 1);   /* Cancel: ModalResult only in the DFM */
        CHECK(lj_win_get(LJ_WIN_TRANSLATION_EDIT) == NULL);
    }
    if (tr) { click(tr, "bCancel", 1); CHECK(lj_win_get(LJ_WIN_DMXIN_TRANSLATION) == NULL); }
    lj_win_close(LJ_WIN_DMX_IN);

    w = lj_win_open(LJ_WIN_VALUE_TABLE_ERROR);   /* raised by a .vtr import, which is W5: by id only */
    CHECK(w != NULL);
    if (w) { CHECK(strcmp(lj_window_caption(w)->text, "Error importing translation tables") == 0); CHECK(greyed(w, "re1") && live(w, "bOk")); golden(w, "W4DMX_VALUE_TABLE_ERROR"); click(w, "bOk", 1); CHECK(lj_win_get(LJ_WIN_VALUE_TABLE_ERROR) == NULL); }
    w = lj_win_open(LJ_WIN_EXPORT_TABLE);
    CHECK(w != NULL);
    if (w) {
        CHECK(strcmp(lj_window_caption(w)->text, "Export Value tables") == 0);
        CHECK(na(w, "bExport") && na(w, "cbOverwrite") && na(w, "DirectoryListBox") && na(w, "DriveComboBox") && live(w, "BitBtn2"));
        golden(w, "W4DMX_EXPORT_TABLE");
        click(w, "BitBtn2", 1);
        CHECK(lj_win_get(LJ_WIN_EXPORT_TABLE) == NULL);
    }

    /* ---- 3. Hardware Setup: the dropped-hardware tabs (W4-HW-0, I13) ---- */
    w = lj_win_open(LJ_WIN_HARDWARE_SETUP);
    CHECK(w != NULL);
    if (w) {
        for (i = 0; i < sizeof hw_dropped / sizeof hw_dropped[0]; i++) { if (!na(w, hw_dropped[i])) printf("  Hardware Setup %s not greyed with the hint\n", hw_dropped[i]); CHECK(na(w, hw_dropped[i])); }
        CHECK(!live(w, "sbID1") && !na(w, "sbID1"));   /* a DFM hint ("Hold to ID USB interface", ShowHint off) is not replaced (F55) */
        CHECK(live(w, "rgMIDIIn") && live(w, "bOk"));
        lj_win_close(LJ_WIN_HARDWARE_SETUP);
    }

    CHECK(me16 == 0);
    lj_win_close_all();
    lj_desktop_destroy(main_win);
    ctx->show = NULL;
    lj_show_free(show);
    lj_log_set_sink(NULL, NULL);
    printf("test_dmx_in: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
