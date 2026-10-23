/* test_dmx_in.c - the runnable check for src/windows/lj_dmx_in.c (W4-DMX, W4-HW-0 entry points; W5-DMXIN-1, -4, -6, -8:
 * the windows live, dmxin track):
 *  1. Dmx In opens from the C-L toolbar DMX In button (LJ_ACT_DMX_IN) and from the Global Patch DMX In source (double-click);
 *     every input live; the monitor shows the incoming channel count and rate; a Fingers-emulated input shows in the rows;
 *     Assign / Clear on the working copy; nothing reaches the live map before Save (05 s3.1); Save writes DmxIn/DmxIn.dat;
 *     Options > Disable DMX-in functions toggles the record; the Global Patch "Set DMX In Sequence" live;
 *  2. DMX-In translation: the SYSTEM tables in the tree, Assign table, "n active tranlation(s)", Ok, Save installs the table
 *     on the input (raw -> out); Create new table -> Edit tranlation table: Set Linear Range, Ok writes the .vtr; a bad .vtr
 *     raises TValueTableImportErrorForm with its reason; Export writes the user tables to the chosen folder;
 *  3. Hardware Setup: Enable DMX in live with the universe beside it; an input universe equal to an output universe refused;
 *     the dropped-hardware tabs stay greyed with the hint;
 *  4. goldens W5DMX_*.bmp (LJ_GOLDEN_UPDATE=1 rewrites them; reviewed in REVIEWED.tsv). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "io/lj_dmxin.h"
#include "io/lj_fn.h"
#include "io/lj_sacn.h"
#include "model/lj_library.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_spinedit.h"
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
static int io_applied;
static void on_io(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)ud; if (arg == LJ_IO_DMXIN) io_applied++; }

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
static int na(lj_window *win, const char *n) { lj_widget *w = win ? lj_window_find(win, n) : NULL; return w && !w->enabled && w->hint && strcmp(w->hint, LJ_DMX_IN_NA) == 0; }
static int live(lj_window *win, const char *n) { lj_widget *w = win ? lj_window_find(win, n) : NULL; return w && w->enabled; }
static int menu_en(lj_window *win, const char *n) { lj_menu_rt *rt = win ? lj_menu_rt_named(lj_window_menus(win), n) : NULL; return rt ? (int)rt->enabled : -1; }
static const char *row(lj_window *win, const char *lv, int r) { lj_widget *w = win ? lj_window_find(win, lv) : NULL; const lj_w_listview *l = (const lj_w_listview *)w; return l && r >= 0 && r < l->n ? l->items[r] : ""; }
static const char *cap(lj_window *win, const char *n) { const char *c = win ? lj_w_caption(lj_window_find(win, n)) : NULL; return c ? c : ""; }
static int fn_row(const char *caption) { int i; for (i = 0; i < lj_dmxin_fn_nrows; i++) if (strcmp(lj_dmxin_fn_rows[i].caption, caption) == 0) return i; return -1; }
static int tree_row(lj_window *win, const char *name)
{
    lj_widget *w = win ? lj_window_find(win, "tv1") : NULL;
    const lj_w_treeview *t = (const lj_w_treeview *)w;
    int i;
    for (i = 0; t && i < t->n; i++) if (t->items[i][0] == '\t' && strcmp(t->items[i] + 1, name) == 0) return i;
    return -1;
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
    static const char *const di_inputs[] = { "LVDmxFunctions", "LVFunctions", "bClear", "bAssign", "bSave", "bcancel" };
    static const char *const di_menu_on[] = { "M_DisableDMXinFunc", "ClearAllAssignments1", "ClearCueLacthassignments1", "ClearCueonCue01",
                                              "Pop_Clear", "Pop_Seq", "Pop_Cue", "Pop_Cuelist", "Pop_BGCue", "M_ValueTranslations", "Help1" };
    static const char *const hw_dropped[] = { "cbFunction1A", "RG2532Enable", "rg2532Comport", "rg2518Enable", "rg2518Comport", "cb2518Version", "cb_MCX" };
    static lj_dmxin_cfg dcfg;
    static lj_dmxin_map saved;
    static lj_sacn_cfg scfg;
    static const char ud[] = "lj_test_dmxin_ud/";
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_library lib;
    lj_window *main_win, *di, *tr, *ed, *w;
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    char path[512];
    size_t i;
    int r;
    (void)argc; (void)argv;

    lj_log_set_sink(sink, NULL);
    lj_fs_mkdir("lj_test_dmxin_ud");
    CHECK(lj_library_create(ud, "T", &lib) == 0);
    {   /* a rerun starts from an empty table folder */
        static char names[64][LJ_DIRENT_CAP];
        int k, n = lj_library_dir_list("lj_test_dmxin_ud/User/ValueTables", ".vtr", names, 64);
        for (k = 0; k < n; k++) { snprintf(path, sizeof path, "lj_test_dmxin_ud/User/ValueTables/%s", names[k]); remove(path); }
    }
    lj_dmxin_cfg_default(&dcfg); lj_dmxin_map_init(&saved); lj_sacn_cfg_default(&scfg, "test");
    lj_dmxin_reset();
    ctx->show = show; ctx->lib = &lib; ctx->main_ini = NULL; ctx->dmxin = &dcfg; ctx->dmxin_map = &saved; ctx->sacn = &scfg;
    lj_str_copy(ctx->userdir, sizeof ctx->userdir, ud);
    lj_win_set_host(NULL);
    lj_desktop_bind_action(LJ_ACT_DMX_IN, on_dmx_in, NULL);
    lj_desktop_bind_action(LJ_ACT_IO_APPLY, on_io, NULL);

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
        lj_widget *lv = lj_window_find(di, "LVDmxFunctions"), *fl = lj_window_find(di, "LVFunctions");
        CHECK(strcmp(lj_window_caption(di)->text, "Dmx In") == 0);
        for (i = 0; i < sizeof di_inputs / sizeof di_inputs[0]; i++) { if (!live(di, di_inputs[i])) printf("  Dmx In %s not live\n", di_inputs[i]); CHECK(live(di, di_inputs[i])); }
        for (i = 0; i < sizeof di_menu_on / sizeof di_menu_on[0]; i++) CHECK(menu_en(di, di_menu_on[i]) == 1);
        CHECK(strcmp(lj_w_edit_text(lj_window_find(di, "eCue")), "N/A") == 0);
        CHECK(strcmp(cap(di, "lChIn"), "DMX Channels In 0") == 0 && strcmp(cap(di, "lFreq"), "Update Freq 0 Hz") == 0);
        CHECK(((const lj_w_listview *)fl)->n == lj_dmxin_fn_nrows && strcmp(row(di, "LVFunctions", 0), "Master Intensity") == 0);
        /* the input through the same seam (Fingers emulation, channels 1..48): the monitor and the rows follow */
        lj_dmxin_emulate(1, 255, 0); lj_dmxin_emulate(2, 70, 0);
        lj_dmx_in_refresh(LJ_WIN_DMX_IN);
        CHECK(strcmp(cap(di, "lChIn"), "DMX Channels In 48") == 0);
        CHECK(strncmp(row(di, "LVDmxFunctions", 0), "001\t255\t", 8) == 0 && strncmp(row(di, "LVDmxFunctions", 1), "002\t070\t", 8) == 0);
        /* Assign on the working copy; the live map changes only on Save (05 s3.1) */
        lj_w_listview_select(lv, 2); lj_w_listview_select(fl, fn_row("HTP Group 3"));
        click(di, "bAssign", 1);
        CHECK(strcmp(row(di, "LVDmxFunctions", 2), "003\t000\t\tHTP Group 3") == 0 && strcmp(cap(di, "lUsed"), "1") == 0);
        lj_w_listview_select(lv, 3); lj_w_listview_select(fl, fn_row("Latch Cue")); click(di, "bAssign", 1);
        lj_w_listview_select(lv, 4); lj_w_listview_select(fl, fn_row("Cuelist Go")); click(di, "bAssign", 1);
        lj_w_listview_select(lv, 4); click(di, "bClear", 1);
        CHECK(strcmp(row(di, "LVDmxFunctions", 4), "005\t000\t\t") == 0);
        CHECK(saved.ch[2].fn == LJ_DIF_NONE);                                       /* not before Save */
        CHECK(fire(di, "ClearCueonCue01") == 0 && lj_dmx_in_working()->cue0_clears == 1);
        golden(di, "W5DMX_DMX_IN");
        click(di, "bSave", 1);
        CHECK(saved.ch[2].fn == LJ_DIF_HTP_GROUP && saved.ch[2].num == 3 && saved.ch[3].fn == LJ_DIF_LATCH_CUE && saved.cue0_clears == 1);
        CHECK(lj_library_path(&lib, "DmxIn/DmxIn.dat", path, sizeof path) == 0 && lj_fs_exists(path));
        CHECK(fire(di, "ClearCueLacthassignments1") == 0 && lj_dmx_in_working()->ch[3].fn == LJ_DIF_NONE && lj_dmx_in_working()->ch[2].fn == LJ_DIF_HTP_GROUP);
        /* Options > Disable DMX-in functions (the three hotkeys reach the same record through app/lj_app_io.c) */
        io_applied = 0;
        CHECK(fire(di, "M_DisableDMXinFunc") == 0 && dcfg.functions_off == 1 && io_applied == 1);
        CHECK(fire(di, "M_DisableDMXinFunc") == 0 && dcfg.functions_off == 0);
        click(di, "bcancel", 1);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) == NULL);
    }

    /* ---- the Global Patch entry point and its DMX-in items ---- */
    w = lj_win_open(LJ_WIN_GLOBAL_PATCH);
    CHECK(w != NULL);
    if (w) {
        CHECK(menu_en(w, "P_InSeq") == 1);
        CHECK(menu_en(w, "P_EnableUse") == 0 && menu_en(w, "P_EnableIgnore") == 0);   /* the per-channel Overwrite Enable exemption: model + engine, not landed */
        click(w, "eSourceIn", 1);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) == NULL);   /* a single click edits the field */
        click(w, "eSourceIn", 2);
        CHECK(lj_win_get(LJ_WIN_DMX_IN) != NULL);
        lj_win_close(LJ_WIN_GLOBAL_PATCH);
    }

    /* ---- 2. Value-translations -> DMX-In translation: assign a SYSTEM table, Ok, Save -> the input translates ---- */
    di = lj_win_get(LJ_WIN_DMX_IN);
    if (di) CHECK(fire(di, "M_ValueTranslations") == 0);
    tr = lj_win_get(LJ_WIN_DMXIN_TRANSLATION);
    CHECK(tr != NULL);
    if (tr) {
        lj_widget *lv = lj_window_find(tr, "lvChannels"), *tv = lj_window_find(tr, "tv1");
        CHECK(strcmp(lj_window_caption(tr)->text, "DMX-In translation") == 0);
        CHECK(strcmp(cap(tr, "lActive"), "0 active tranlation(s)") == 0);
        CHECK(live(tr, "lvChannels") && live(tr, "tv1") && !live(tr, "sbUp") && live(tr, "bOk"));
        CHECK(menu_en(tr, "M_UpdateActiveTranslations") == 1 && menu_en(tr, "M_ClearAll") == 1 && menu_en(tr, "Pop_AssignSelected") == 1 && menu_en(tr, "pop_NewTable") == 1);
        r = tree_row(tr, "Off/On");
        CHECK(r > 0 && ((const lj_w_treeview *)tv)->n == LJ_VTR_STOCK + 1);   /* one SYSTEM node and its 8 tables */
        lj_w_listview_select(lv, 1); ((lj_w_treeview *)tv)->index = r;
        CHECK(fire(tr, "Pop_AssignSelected") == 0);
        CHECK(strcmp(cap(tr, "lActive"), "1 active tranlation(s)") == 0 && strcmp(row(tr, "lvChannels", 1), "2\tOff/On") == 0);
        CHECK(live(tr, "sbUp") && live(tr, "sbDown"));
        golden(tr, "W5DMX_TRANSLATION");
        click(tr, "bOk", 1);
        CHECK(lj_win_get(LJ_WIN_DMXIN_TRANSLATION) == NULL);
    }
    di = lj_win_get(LJ_WIN_DMX_IN);
    if (di) {
        CHECK(strncmp(row(di, "LVDmxFunctions", 1), "002\t070\tOff/On\t", 15) == 0);
        CHECK(f->out[1] == 70);                                                    /* the table is not live before Save */
        click(di, "bSave", 1);
        lj_dmxin_emulate(2, 70, 0); CHECK(f->raw[1] == 70 && f->out[1] == 0);
        lj_dmxin_emulate(2, 200, 0); CHECK(f->out[1] == 255);                      /* SYSTEM Off/On: 50 % threshold */
    }

    /* ---- Create new table -> Edit tranlation table: Set Linear Range, Ok writes the .vtr ---- */
    if (di) CHECK(fire(di, "M_ValueTranslations") == 0);
    tr = lj_win_get(LJ_WIN_DMXIN_TRANSLATION);
    if (tr) CHECK(fire(tr, "pop_NewTable") == 0);
    ed = lj_win_get(LJ_WIN_TRANSLATION_EDIT);
    CHECK(ed != NULL);
    if (ed) {
        CHECK(strcmp(lj_window_caption(ed)->text, "Edit tranlation table") == 0);   /* sic (D13) */
        CHECK(live(ed, "sg1") && live(ed, "eVF") && live(ed, "bSet") && live(ed, "eName") && !live(ed, "eID"));
        lj_w_edit_set_text(lj_window_find(ed, "eVF"), "0"); lj_w_edit_set_text(lj_window_find(ed, "eVT"), "255");
        lj_w_edit_set_text(lj_window_find(ed, "eTF"), "50"); lj_w_edit_set_text(lj_window_find(ed, "eTT"), "72");   /* the help's MX-10 strobe example (05 s3.3) */
        click(ed, "bSet", 1);
        lj_w_edit_set_text(lj_window_find(ed, "eName"), "Strobe range"); lj_w_edit_set_text(lj_window_find(ed, "ePurpose"), "test");
        lj_w_edit_set_text(lj_window_find(ed, "eID"), "{00000000-0000-4000-8000-000000000001}");   /* the generated GUID differs per run: a fixed one for the golden */
        golden(ed, "W5DMX_TRANSLATION_EDIT");
        click(ed, "bOk", 1);
        CHECK(lj_win_get(LJ_WIN_TRANSLATION_EDIT) == NULL);
    }
    if (tr) CHECK(tree_row(tr, "Strobe range") > 0);
    /* a bad file raises TValueTableImportErrorForm from its trigger */
    {
        FILE *fp;
        snprintf(path, sizeof path, "%sUser/ValueTables/USER_Bad_{00000000-0000-4000-8000-00000000000B}.vtr", ud);
        if ((fp = fopen(path, "wb")) != NULL) { fputs("// LightJockey DMX value translation file\r\nID={00000000-0000-4000-8000-00000000000B}\r\nTYPE=USER\r\nNAME=Bad\r\n7=1\r\n7=2\r\n", fp); fclose(fp); }
        lj_win_close(LJ_WIN_DMXIN_TRANSLATION);
        if (di) CHECK(fire(di, "M_ValueTranslations") == 0);
        w = lj_win_get(LJ_WIN_VALUE_TABLE_ERROR);
        CHECK(w != NULL);
        if (w) { CHECK(strcmp(lj_window_caption(w)->text, "Error importing translation tables") == 0 && live(w, "bOk")); golden(w, "W5DMX_VALUE_TABLE_ERROR"); click(w, "bOk", 1); CHECK(lj_win_get(LJ_WIN_VALUE_TABLE_ERROR) == NULL); }
        remove(path);
    }
    /* Options -> Export Value tables (PROPOSAL entry): the user tables into the chosen folder */
    tr = lj_win_get(LJ_WIN_DMXIN_TRANSLATION);
    if (tr) CHECK(fire(tr, "M_Options") == 0);
    w = lj_win_get(LJ_WIN_EXPORT_TABLE);
    CHECK(w != NULL);
    if (w) {
        lj_fs_mkdir("lj_test_dmxin_export");
        CHECK(strcmp(lj_window_caption(w)->text, "Export Value tables") == 0);
        CHECK(live(w, "bExport") && live(w, "cbOverwrite") && live(w, "BitBtn2"));
        CHECK(lj_dmx_in_export_dir("lj_test_dmxin_export") == 0 && strcmp(cap(w, "pTarget"), "lj_test_dmxin_export") == 0);
        golden(w, "W5DMX_EXPORT_TABLE");
        CHECK(lj_dmx_in_export() == 1);
        click(w, "BitBtn2", 1);
        CHECK(lj_win_get(LJ_WIN_EXPORT_TABLE) == NULL);
    }
    lj_win_close(LJ_WIN_DMXIN_TRANSLATION);
    lj_win_close(LJ_WIN_DMX_IN);

    /* ---- 3. Hardware Setup: Enable DMX in live, an output universe refused, the dropped-hardware tabs greyed ---- */
    w = lj_win_open(LJ_WIN_HARDWARE_SETUP);
    CHECK(w != NULL);
    if (w) {
        lj_widget *use = lj_window_find(w, "cbUseDMXin"), *uni = lj_window_find(w, "seDmxInUniverse");
        CHECK(live(w, "cbUseDMXin") && uni && !uni->enabled);
        lj_w_checkbox_set_checked(use, 1);
        CHECK(uni && uni->enabled);
        lj_w_spinedit_set_value(uni, 1);                                           /* = output universe 1 */
        CHECK(lj_dmx_in_hs_apply() != NULL && dcfg.enabled == 0);
        lj_w_spinedit_set_value(uni, 10); io_applied = 0;
        CHECK(lj_dmx_in_hs_apply() == NULL && dcfg.enabled == 1 && dcfg.universe == 10 && io_applied == 1);
        for (i = 0; i < sizeof hw_dropped / sizeof hw_dropped[0]; i++) { if (!na(w, hw_dropped[i])) printf("  Hardware Setup %s not greyed with the hint\n", hw_dropped[i]); CHECK(na(w, hw_dropped[i])); }
        CHECK(live(w, "rgMIDIIn") && live(w, "bOk"));
        lj_win_close(LJ_WIN_HARDWARE_SETUP);
    }

    {   /* W5-AUD-6: the DMX-in Audio Analyzer Gain / Noise Threshold rows use the Fingers jogs' mapping (128 = 0 dB) */
        static lj_audio_cfg ac;
        lj_audio_cfg *was = ctx->audio;
        lj_audio_cfg_default(&ac); ac.gain_pct = 1; ctx->audio = &ac;
        lj_fn_run_prog(LJ_FN_DMXIN_AUDIO, 128u, 1);          CHECK(ac.gain_pct == 100);
        lj_fn_run_prog(LJ_FN_DMXIN_AUDIO, 1u << 8 | 200u, 1); CHECK(ac.noise == 200);
        ctx->audio = was;
    }
    CHECK(me16 == 0);
    lj_win_close_all();
    lj_desktop_destroy(main_win);
    ctx->show = NULL; ctx->lib = NULL; ctx->dmxin = NULL; ctx->dmxin_map = NULL; ctx->sacn = NULL;
    lj_show_free(show);
    lj_dmxin_reset();
    lj_log_set_sink(NULL, NULL);
    printf("test_dmx_in: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
