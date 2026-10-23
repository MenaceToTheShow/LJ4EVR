/* test_cuelist_form.c - the runnable check for src/windows/lj_cuelist_form.c (W3 cuelist-window): the caption rules (F19,
 * 13 s5), the transport enable table per runner state with Lock / Browse latched (03 s6.2), the CD / DA / MP time-code
 * buttons, menu items and recent-command buttons greyed with LJ_CL_NOT_AVAILABLE (design 8.5, ME-11), the ListPop commands
 * verbatim in note 03 s6.3 order, the grid row text, a loaded-with-error line painted red and the gray / blue line states,
 * the ops posted through LJ_ACT_CUELIST_OP, the edit panel page per command, the Hotkey Command function picker, and the
 * golden tests/ui/golden/WIN_CUELIST_FORM.bmp (LJ_GOLDEN_UPDATE=1 rewrites it; reviewed in REVIEWED.tsv), and the cue-list
 * toolbar with the Background Cue LED on and the Statics LED off (LORD_FIDELITY-16).
 * `test_cuelist_form --shot <dir>` also writes <dir>/cuelist-form.png and <dir>/cuelist-toolbar-leds.png (docs/screenshots/w3). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as test_desktop_w2.c) */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "io/lj_actions.h"
#include "io/lj_hotkeys.h"
#include "model/lj_cuelist.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/lj_window.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_cuelist_form.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int ops[64], nops;
static int me16;   /* ME-16: lj_window_create complaints about unmatched handler names, either way */
static void sink(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "no binding for handler") || strstr(line, "matches no DFM handler") || strstr(line, "unknown event") || strstr(line, "has no behaviour file")) { me16++; printf("  ME-16: %s\n", line); }
}
static void on_op(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)ud; if (nops < 64) ops[nops++] = arg; }
static int last_op(void) { return nops ? ops[nops - 1] : -1; }

static void mouse(lj_window *win, int type, int x, int y, int button)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.x = x; ev.u.mouse.y = y; ev.u.mouse.button = button; ev.u.mouse.clicks = 1;
    lj_window_event(win, &ev);
}
static void click(lj_window *win, const char *name, int button)
{
    lj_widget *w = lj_window_find(win, name);
    int ax, ay;
    if (!w) { fails++; printf("FAIL no widget %s\n", name); return; }
    lj_widget_abs(w, &ax, &ay);
    mouse(win, LJ_EV_MOUSE_DOWN, ax + w->w / 2, ay + w->h / 2, button);
    mouse(win, LJ_EV_MOUSE_UP, ax + w->w / 2, ay + w->h / 2, button);
}
static int fire(lj_window *win, const char *item)   /* runs a menu item's bound handler as lj_menu's fire() does */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = lj_menu_rt_named(m, item);
    if (!rt || !rt->fn) { printf("  menu item %s: not bound\n", item); return -1; }
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}
static int en(lj_window *win, const char *n) { lj_widget *w = lj_window_find(win, n); return w ? (int)w->enabled : -1; }
static int vis(lj_window *win, const char *n) { lj_widget *w = lj_window_find(win, n); return w ? (int)w->visible : -1; }
static int menu_en(lj_window *win, const char *n) { lj_menu_rt *rt = lj_menu_rt_named(lj_window_menus(win), n); return rt ? (int)rt->enabled : -1; }

/* the leaves of ListPop that are commands, in the order the user reads them (Jump / Run ... and everything below N1 left out) */
static int pop_commands(const lj_menu_item_desc *it, int n, const char **out, int k)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(it[i].name, "N1")) return -k - 1;   /* the separator ends the command block */
        if (!strcmp(it[i].name, "Pop_JumpToThis") || !strcmp(it[i].name, "M_Run")) continue;
        if (it[i].nchildren) { k = pop_commands(it[i].children, it[i].nchildren, out, k); if (k < 0) return k; }
        else if (k < 40) out[k++] = it[i].caption;
    }
    return k;
}

static uint32_t px(lj_window *win, lj_widget *w, int rx, int ry)
{
    const lj_surface *sf = lj_window_surface(win);
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    return sf->px[(ay + ry) * sf->w + ax + rx] & 0xFFFFFFu;
}
static int row_has(lj_window *win, lj_widget *lv, int row, uint32_t rgb)   /* any pixel of the row (text or fill) in rgb */
{
    const lj_w_listview *v = (const lj_w_listview *)lv;
    int x, y, top = 2 + v->header_h + row * v->row_h;
    for (y = top + 1; y < top + v->row_h - 1; y++) for (x = 4; x < lv->w - 20; x++) if (px(win, lv, x, y) == rgb) return 1;
    return 0;
}
static int cell_ink(lj_window *win, lj_widget *lv, int row, int col, uint32_t rgb)   /* ME-111: a pixel of one cell in rgb */
{
    const lj_w_listview *v = (const lj_w_listview *)lv;
    int x, y, x0 = 2, top = 2 + v->header_h + row * v->row_h;
    for (x = 0; x < col; x++) x0 += v->col_w[x];
    for (y = top + 1; y < top + v->row_h - 1; y++) for (x = x0; x < x0 + v->col_w[col] - 1; x++) if (px(win, lv, x, y) == rgb) return 1;
    return 0;
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
    snprintf(g, sizeof g, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, "lj_test_cuelist_form_%s.bmp", name);
    lj_window_paint(win);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(win), g) == 0); printf("  %s: written\n", g); return; }
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    rc = same_file(tmp, g);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (the render is %s)\n", name, rc ? "missing or unreadable" : "differs", tmp); return; }
    printf("  %s: identical\n", name);
    remove(tmp);
}

static int write_png(const lj_surface *sf, int x, int y, int w, int h, const char *path)
{
    unsigned char *rgb = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    size_t len = 0;
    void *png;
    FILE *f;
    int i, j, rc = -1;
    if (!rgb) return -1;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        uint32_t p = sf->px[(y + j) * sf->w + x + i];
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
static void shot(lj_window *win, const char *dir, const char *name)
{
    char path[1024];
    const lj_surface *sf = lj_window_surface(win);
    if (!dir) return;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    CHECK(write_png(sf, 0, 0, sf->w, sf->h, path) == 0);
    printf("  wrote %s\n", path);
}

/* LORD_FIDELITY-16: the C-L toolbar row with one LED on (Background Cue) and one off (Statics) */
static void toolbar_leds(const char *dir)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_hotkeys hk;
    lj_window *win;
    lj_widget *w, *bg, *stc;
    int ax, ay, j, k, lit = 0;
    lj_hotkeys_init(&hk);
    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->hotkeys = &hk; ctx->allow_freeze = ctx->allow_offline = ctx->allow_solo = ctx->show_nextprev = 1;
    if ((win = lj_desktop_create()) == NULL) { CHECK(!"lj_desktop_create"); return; }
    ctx->view.tick_no = 1; ctx->view.bg_any = 1; ctx->view.statics_any = 0;
    lj_desktop_toolbar_update();
    lj_window_paint(win);
    bg = lj_window_find(win, "tb_cuelist_5"); stc = lj_window_find(win, "tb_cuelist_6"); w = lj_window_find(win, "tb_row_cuelist");
    CHECK(bg && stc && w && lj_w_led_on(bg) == 1 && lj_w_led_on(stc) == 0);
    if (w) {
        const lj_surface *sf = lj_window_surface(win);
        lj_widget_abs(w, &ax, &ay);
        for (j = ay; j < ay + w->h; j++) for (k = ax; k < ax + w->w; k++) lit += (sf->px[j * sf->w + k] & 0xFFFFFFu) == LJ_LED_ON_RGB;
        CHECK(lit >= 60 && lit < 2 * 60);   /* one lit dot (69 green px, lj_w_led.c), not two */
        if (dir) { char path[1024]; snprintf(path, sizeof path, "%s/cuelist-toolbar-leds.png", dir); CHECK(write_png(sf, ax, ay, w->w, w->h, path) == 0); printf("  wrote %s\n", path); }
    }
    lj_desktop_destroy(win);
    ctx->hotkeys = NULL;
    memset(&ctx->view, 0, sizeof ctx->view);
}

static lj_cl_line mk(int manual, int cmd, uint32_t param_ms, const char *pstr, uint32_t cue, const char *comment, int load_error)
{
    lj_cl_line l;
    memset(&l, 0, sizeof l);
    l.manual = (uint8_t)manual; l.cmd = (uint8_t)cmd; l.param_ms = param_ms; l.cue = cue; l.load_error = (uint8_t)load_error;
    if (pstr) lj_str_copy(l.param, sizeof l.param, pstr);
    if (comment) lj_str_copy(l.comment, sizeof l.comment, comment);
    return l;
}

int lj_test_cuelist_form(int argc, char **argv);
int lj_test_cuelist_form(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_cl_status st;
    lj_cuelist *cl;
    lj_window *win;
    lj_widget *lv, *w;
    lj_cl_line l;
    const char *cmds[40];
    char row[200];
    const char *dir = NULL;
    int i, n;
    for (i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--shot") == 0) dir = argv[i + 1];

    ctx->show = lj_show_new();
    ctx->forms_ini = lj_ini_new();
    ctx->main_ini = lj_ini_new();
    memset(&st, 0, sizeof st);
    st.tc_src = LJ_TC_PC_ELAPSED; st.tc_ms = -1;
    ctx->cl = &st;
    lj_desktop_bind_action(LJ_ACT_CUELIST_OP, on_op, NULL);
    lj_log_set_sink(sink, NULL);

    /* the fixture: wait elapsed 1:02.50 + cue 3 | manual Go + cue 4 | a loaded DA Play line (load error; W5: MP is live, so the
     * inert family of this fixture is DA, ME-11) | Hotkey Command | Goto Line 1 */
    cl = lj_cuelist_form_list();
    CHECK(cl != NULL && cl == ctx->show->current_cl);
    if (!cl) return 1;
    cl->number = 1; lj_str_copy(cl->name, sizeof cl->name, "ThunderStrike");
    l = mk(0, LJ_CL_WAIT_ELAPSED, lj_cl_hmsf_ms(0, 1, 2, 50, 100), NULL, 3, "Intro", 0); lj_cuelist_insert(cl, cl->n, &l);
    l = mk(1, LJ_CL_NONE, 0, NULL, 4, NULL, 0);                                         lj_cuelist_insert(cl, cl->n, &l);
    l = mk(0, LJ_CL_DA_PLAY, 0, NULL, 0, "clip", 1);                                    lj_cuelist_insert(cl, cl->n, &l);
    l = mk(0, LJ_CL_HOTKEY_COMMAND, LJ_FN_CUELIST_STOP, NULL, 0, NULL, 0);              lj_cuelist_insert(cl, cl->n, &l);
    l = mk(0, LJ_CL_GOTO_LINE, 1, NULL, 0, "again", 0);                                 lj_cuelist_insert(cl, cl->n, &l);

    win = lj_cuelist_form_open();
    CHECK(win != NULL);
    if (!win) return 1;
    lv = lj_window_find(win, "lv1");

    /* caption (F19, 13 s5: "Cuelist [" ... " Tracking on") */
    CHECK(strcmp(lj_window_caption(win)->text, "Cuelist [0001 ThunderStrike]") == 0);
    cl->tracking = 1; lj_cuelist_form_refresh();
    CHECK(strcmp(lj_window_caption(win)->text, "Cuelist [0001 ThunderStrike] Tracking on") == 0);
    cl->tracking = 0; lj_cuelist_form_refresh();

    /* grid rows */
    CHECK(lj_cuelist_form_row(1, row, sizeof row) == 0 && strcmp(row, "1\tAuto\tWait Elapsed\t01:02:50\t0003 \tIntro") == 0);
    CHECK(lj_cuelist_form_row(2, row, sizeof row) == 0 && strcmp(row, "2\tMan\t<>\t\t0004 \t") == 0);   /* "<>": no command (exe 0x179F14, cuelist1.gif) */
    CHECK(lj_cuelist_form_row(3, row, sizeof row) == 0 && strcmp(row, "3\tAuto\tDA Play\t\t\tclip") == 0);
    CHECK(lj_cuelist_form_row(4, row, sizeof row) == 0 && strcmp(row, "4\tAuto\tHotkey Command\tCuelist Stop\t\t") == 0);
    CHECK(lj_cuelist_form_row(5, row, sizeof row) == 0 && strcmp(row, "5\tAuto\tGoto Line\t1\t\tagain") == 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "lTotalLines")), "5 Lines") == 0);
    /* ME-119 (D6): the Flash Sequence time uses the host decimal separator */
    l = mk(0, LJ_CL_FLASH_SEQUENCE, 1500, NULL, 0, NULL, 0); l.ref = 1; lj_cuelist_insert(cl, cl->n, &l);
    CHECK(lj_cuelist_form_row(6, row, sizeof row) == 0 && strstr(row, "\t0001 1.5 s\t") != NULL);
    if (setlocale(LC_NUMERIC, "de-DE") || setlocale(LC_NUMERIC, "German") || setlocale(LC_NUMERIC, "de_DE.UTF-8") || setlocale(LC_NUMERIC, "da_DK.UTF-8")) {
        CHECK(lj_cuelist_form_row(6, row, sizeof row) == 0 && strstr(row, "\t0001 1,5 s\t") != NULL);
    } else printf("SKIP: no comma locale on this host, the ME-119 comma row not run here\n");
    setlocale(LC_NUMERIC, "C");
    lj_cuelist_delete(cl, cl->n - 1);

    /* ME-11 / design 8.5: time-code sources, recent commands and ListPop families greyed with the hint */
    CHECK(en(win, "sbCDTime") == 0 && en(win, "sbDATime") == 0 && en(win, "sbMPTime") == 1);   /* W5 media: sbMPTime live (W5-MED-4) */
    CHECK(en(win, "sbMIDITime") == 1 && en(win, "sbPCElapsed") == 1 && en(win, "sbPCClock") == 1);
    CHECK(strcmp(lj_window_find(win, "sbCDTime")->hint, LJ_CL_NOT_AVAILABLE) == 0 && strcmp(lj_window_find(win, "sbMPTime")->hint, "Show Media Timecode") == 0);   /* the DFM hint back */
    CHECK(strcmp(lj_window_find(win, "sbDATime")->hint, "Not available in LJ4EVR") == 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "sbCMD1")), "CD Stop") == 0 && en(win, "sbCMD1") == 0 && strcmp(lj_window_find(win, "sbCMD1")->hint, LJ_CL_NOT_AVAILABLE) == 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "sbCMD3")), "Wait CD") == 0 && en(win, "sbCMD3") == 0);
    CHECK(menu_en(win, "Pop_CDCommands") == 0 && menu_en(win, "pop_DACommands") == 0 && menu_en(win, "Pop_MPCommands") == 1);   /* W5 media (W5-MED-3) */
    CHECK(menu_en(win, "MediaPlayer1") == 1 && menu_en(win, "CueMediaFile1") == 1 && menu_en(win, "popWaitCD") == 0);
    /* ME-120 (W4): the three alive, Import greyed */
    CHECK(menu_en(win, "OffsetMIDITimecodes1") == 1 && menu_en(win, "OffsetPCTime1") == 1 && menu_en(win, "OffsetMIDITimecodes2") == 1 && menu_en(win, "M_ImportOldCuelist") == 0);
    CHECK(menu_en(win, "MPLoadFile1") == 1 && menu_en(win, "Pop_MTCCommands") == 1);   /* MPLoadFile1 carries aHotkeyCommand (D13) */

    /* the command list in note 03 s6.3 order, verbatim (the ListPop commands = lj_cl_cmd_caption 1..Hotkey Command) */
    {
        const lj_menu_desc *pm = lj_form_find_menu(&lj_form_TCUELIST2FORM, "ListPop");
        n = pm ? pop_commands(pm->items, pm->n, cmds, 0) : 0;
        if (n < 0) n = -n - 1;
        CHECK(n == LJ_CL_HOTKEY_COMMAND);
        for (i = 0; i < n && i < LJ_CL_HOTKEY_COMMAND; i++)
            if (strcmp(cmds[i], lj_cl_cmd_caption[i + 1]) != 0) { fails++; printf("FAIL ListPop command %d: \"%s\", want \"%s\"\n", i, cmds[i], lj_cl_cmd_caption[i + 1]); }
        CHECK(n > 29 && strcmp(cmds[0], "Wait CD") == 0 && strcmp(cmds[11], "MP Load File") == 0 && strcmp(cmds[25], "Goto Line#") == 0 && strcmp(cmds[29], "Hotkey Command") == 0);
    }

    /* enable table, stopped (IDLE) */
    CHECK(vis(win, "sbTop") == 1 && en(win, "sbTop") == 1 && vis(win, "sbRun") == 1 && en(win, "sbRun") == 1);
    CHECK(vis(win, "sbStop") == 0 && en(win, "sbGo") == 0 && en(win, "sbBack") == 0);
    CHECK(en(win, "sbAdd") == 1 && en(win, "sbNew") == 1 && en(win, "sbLoad") == 1 && en(win, "sbSave") == 1);
    CHECK(en(win, "sbInsert") == 0 && en(win, "sbDelete") == 0);   /* nothing selected */
    lj_w_listview_select(lv, 1); lj_cuelist_form_refresh();
    CHECK(en(win, "sbInsert") == 1 && en(win, "sbDelete") == 1);
    CHECK(strcmp(lj_cuelist_form_edit_panel(), "pComment") == 0);
    nops = 0; click(win, "sbTop", 1); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_TOP, 0));
    click(win, "sbRun", 1); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_RUN, 2));
    cl->mark[0] = 4;
    click(win, "sbMark1", 2); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_MARK_RUN, 1));
    click(win, "sbMark1", 1); CHECK(((lj_w_listview *)lv)->index == 3);   /* idle: a click jumps the selection to the mark */
    CHECK(fire(win, "M_RunToLine") == 0 && last_op() == LJ_CL_OP_ARG(LJ_CL_OP_RUN_TO, 4));

    /* running (an auto line waits): Top / Run replaced by STOP, edits locked, Go ! / Back greyed */
    st.state = LJ_CL_RUNNING; st.last = 1; st.next = 2; st.serial++;
    lj_cuelist_form_refresh();
    CHECK(vis(win, "sbTop") == 0 && vis(win, "sbRun") == 0 && vis(win, "sbStop") == 1 && en(win, "sbStop") == 1);
    CHECK(en(win, "sbGo") == 0 && en(win, "sbBack") == 0 && en(win, "sbAdd") == 0 && en(win, "sbNew") == 0 && en(win, "sbLoad") == 0 && en(win, "sbDelete") == 0);
    CHECK(menu_en(win, "MPLoadFile1") == 0 && menu_en(win, "Pop_JumpToThis") == 0);
    nops = 0; click(win, "sbStop", 1); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_STOP, 0));
    n = cl->lines[3].cmd; lj_cuelist_form_apply_cmd(LJ_CL_WAIT_PC_CLOCK); CHECK(cl->lines[3].cmd == n);   /* "the list is locked while active" */
    /* a manual line waits for Go !: Go enabled, Back not on line 1 */
    st.state = LJ_CL_WAIT_GO; st.next = 1; lj_cuelist_form_refresh();
    CHECK(en(win, "sbGo") == 1 && en(win, "sbBack") == 0 && menu_en(win, "Pop_JumpToThis") == 1);
    st.next = 2; lj_cuelist_form_refresh();
    CHECK(en(win, "sbGo") == 1 && en(win, "sbBack") == 1);
    click(win, "sbGo", 1); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_GO, 0));
    click(win, "sbBack", 1); CHECK(last_op() == LJ_CL_OP_ARG(LJ_CL_OP_BACK, 0));
    /* Lock and Browse latched: the table is unchanged; Lock keeps the waiting line selected */
    click(win, "sbLockLine", 1); click(win, "sbBrowse", 1);
    CHECK(lj_w_speedbutton_down(lj_window_find(win, "sbLockLine")) == 1 && lj_w_speedbutton_down(lj_window_find(win, "sbBrowse")) == 1);
    st.next = 5; lj_cuelist_form_refresh();
    CHECK(((lj_w_listview *)lv)->index == 4);
    CHECK(en(win, "sbGo") == 1 && en(win, "sbBack") == 1 && vis(win, "sbStop") == 1 && vis(win, "sbTop") == 0 && en(win, "sbAdd") == 0);
    st.state = LJ_CL_IDLE; lj_cuelist_form_refresh();
    CHECK(vis(win, "sbTop") == 1 && en(win, "sbTop") == 1 && vis(win, "sbStop") == 0 && en(win, "sbGo") == 0 && en(win, "sbAdd") == 1);
    click(win, "sbLockLine", 1); click(win, "sbBrowse", 1);
    CHECK(lj_w_speedbutton_down(lj_window_find(win, "sbLockLine")) == 0 && lj_w_speedbutton_down(lj_window_find(win, "sbBrowse")) == 0);

    /* the line states: last executed gray, next blue, the load-error line red (design 8.5) */
    st.state = LJ_CL_RUNNING; st.last = 1; st.next = 2; lj_w_listview_select(lv, 4); lj_cuelist_form_refresh();
    lj_window_paint(win);
    CHECK(row_has(win, lv, 0, 0x808080u));
    CHECK(row_has(win, lv, 1, 0x0000FFu));
    CHECK(row_has(win, lv, 2, 0xFF0000u));
    CHECK(!row_has(win, lv, 3, 0xFF0000u) && !row_has(win, lv, 3, 0x0000FFu));
    CHECK(cell_ink(win, lv, 3, 2, 0x000000u));   /* ME-111: "Hotkey Command" is wider than its column: clipped at the cell edge, not blank */
    golden(win, "WIN_CUELIST_FORM");
    shot(win, dir, "cuelist-form.png");
    st.state = LJ_CL_IDLE; st.last = st.next = 0; lj_cuelist_form_refresh();
    lj_window_paint(win);
    CHECK(row_has(win, lv, 2, 0xFF0000u));   /* red whatever the runner does */

    /* commands onto the selected line, the recent panel, the edit page per command */
    lj_w_listview_select(lv, 4); lj_cuelist_form_refresh();
    CHECK(strcmp(lj_cuelist_form_edit_panel(), "pGotoLine") == 0);
    lj_cuelist_form_apply_cmd(LJ_CL_CD_STOP); CHECK(cl->lines[4].cmd == LJ_CL_GOTO_LINE);   /* inert: refused (ME-11; MP is live from W5) */
    lj_cuelist_form_apply_cmd(LJ_CL_WAIT_PC_CLOCK);
    CHECK(cl->lines[4].cmd == LJ_CL_WAIT_PC_CLOCK && lj_cuelist_form_recent(0) == LJ_CL_WAIT_PC_CLOCK && lj_cuelist_form_recent(1) == LJ_CL_CD_STOP);
    CHECK(strcmp(lj_cuelist_form_edit_panel(), "pPCClock") == 0);
    lj_w_edit_set_text(lj_window_find(win, "ePCHour"), "21"); lj_w_edit_set_text(lj_window_find(win, "ePCMin"), "30");
    CHECK(cl->lines[4].param_ms == lj_cl_hmsf_ms(21, 30, 0, 0, 100));
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "sbCMD1")), "Wait PC-Clock") == 0 && en(win, "sbCMD1") == 1);
    CHECK(fire(win, "popWaitTimeOut") == 0 && cl->lines[4].cmd == LJ_CL_WAIT_ELAPSED);   /* an Action-linked ListPop item runs ActionExecute */
    lj_w_edit_set_text(lj_window_find(win, "eElapsedSec"), "30");
    CHECK(cl->lines[4].param_ms == 30000u);
    CHECK(fire(win, "Pop_SetCurrentCue") == 0 && cl->lines[4].cmd == LJ_CL_WAIT_ELAPSED);   /* a panel action, never a stored command */
    CHECK(fire(win, "AdvanceManual1") == 0 && cl->lines[4].manual == 1);
    lj_w_listview_select(lv, 2); lj_cuelist_form_refresh();
    CHECK(strcmp(lj_cuelist_form_edit_panel(), "pComment") == 0 && lj_window_find(win, "eCom")->enabled == 0);   /* the DA line's page greyed */

    /* Set Cue Trig and the Hotkey Command picker (THotkeyFunctionDialog) */
    lj_w_listview_select(lv, 0); lj_cuelist_form_refresh();
    CHECK(fire(win, "pop_SetCueTrig") == 0 && lj_win_get(LJ_WIN_CL_SET_CUE_TRIG) != NULL);
    if (lj_win_get(LJ_WIN_CL_SET_CUE_TRIG)) { click(lj_win_get(LJ_WIN_CL_SET_CUE_TRIG), "SpeedButton3", 1); CHECK(cl->lines[0].trig_mask == 4u && cl->lines[0].cue == 0); lj_win_close(LJ_WIN_CL_SET_CUE_TRIG); }
    lj_w_listview_select(lv, 3); lj_cuelist_form_refresh();
    CHECK(strcmp(lj_cuelist_form_edit_panel(), "pHotkeyCommand") == 0 && strcmp(lj_w_edit_text(lj_window_find(win, "eHotkeyFunction")), "Cuelist Stop") == 0);
    click(win, "bSelectCommand", 1);
    if ((w = lj_hotkey_function_get() ? lj_window_find(lj_hotkey_function_get(), "lvFunctions") : NULL) != NULL) {
        lj_w_listview_select(w, 0);
        click(lj_hotkey_function_get(), "bOk", 1);
        CHECK(cl->lines[3].param_ms == 1u && lj_hotkey_function_get() == NULL);
    } else CHECK(!"Select ..: no Select Function dialog");

    /* Add (right-click = 10 lines), Delete, the line total */
    lj_w_listview_select(lv, -1);
    click(win, "sbAdd", 2); CHECK(cl->n == 15);
    lj_w_listview_select(lv, 14); lj_cuelist_form_refresh(); click(win, "sbDelete", 1); CHECK(cl->n == 14);
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "lTotalLines")), "14 Lines") == 0);

    /* forms.ini: View > Edit Panel off persists, the recent list persists */
    CHECK(fire(win, "M_ViewEdit") == 0 && lj_ini_geti(ctx->forms_ini, "CUELIST2", "ViewEditPanel", 1) == 0 && vis(win, "pEdit") == 0);
    CHECK(strcmp(lj_ini_gets(ctx->forms_ini, "CUELIST2", "Cmd1", ""), "set_current_cue") == 0);

    /* ME-16 over every form this file builds: the dialogs open, the window closes them with nothing unmatched */
    CHECK(lj_win_open(LJ_WIN_SAVE_CUELIST) != NULL && lj_win_open(LJ_WIN_SELECT_CUELIST) != NULL && lj_win_open(LJ_WIN_LOG_TIMECODE) != NULL);
    CHECK(lj_win_open(LJ_WIN_CL_SET_CUE_TRIG) != NULL && lj_hotkey_function_open() != NULL);
    if (lj_win_get(LJ_WIN_SAVE_CUELIST)) click(lj_win_get(LJ_WIN_SAVE_CUELIST), "bSaveAs", 1);
    lj_win_close(LJ_WIN_SAVE_CUELIST); lj_win_close(LJ_WIN_SELECT_CUELIST); lj_win_close(LJ_WIN_LOG_TIMECODE); lj_win_close(LJ_WIN_CL_SET_CUE_TRIG); lj_hotkey_function_close();
    CHECK(me16 == 0);
    lj_cuelist_form_close();
    CHECK(lj_cuelist_form_get() == NULL);
    toolbar_leds(dir);
    lj_desktop_bind_action(LJ_ACT_CUELIST_OP, NULL, NULL);
    lj_log_set_sink(NULL, NULL);
    ctx->cl = NULL;
    lj_show_free(ctx->show); ctx->show = NULL;
    lj_ini_free(ctx->forms_ini); lj_ini_free(ctx->main_ini); ctx->forms_ini = ctx->main_ini = NULL;
    printf("test_cuelist_form: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
