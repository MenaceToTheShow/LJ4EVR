/* test_genmacro_editor.c - `lj4evr_tests test_genmacro_editor` (W3 genmacro track), headless (no lj_win_host, no engine):
 *  - TCueMacroControlForm opens with every DFM handler bound both ways (ME-16: no "no binding" / "unknown event" /
 *    "matches no DFM handler" log line), runtime caption "Generic Macro Editor []" (F19);
 *  - the Built-in shapes list is the 66 regenerated shapes in file order with the duplicate 2*Sine^2 (F39), User-shapes empty;
 *  - a shape click programs the highlighted offset on every selected fixture of the master's type with the panel
 *    (amplitude from the bar, base from eBase, direction), runs LJ_ACT_GENMACRO_APPLY; a 16-bit LSB row shows N/A and
 *    is never programmed (CHM 5583); Clear Offset(s) empties the channel;
 *  - Save Macro > Save New Macro writes CueMacros\00000001.CMA (read back field-exact) and the caption becomes
 *    "Generic Macro Editor [0001 Waves]"; Select Macro lists "0001	Waves";
 *  - the Generic Macro hotkeys: Toggle Editor closes it, Toggle List opens the list;
 *  - golden W3GM_EDITOR (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites it. Temp tree test_genmacro_editor_tmp/. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_shapes_gen.h"
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_genmacro_editor.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_genmacro_editor_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int bind_complaints;
static void sink(void *ud, const char *line)
{
    (void)ud;
    if (getenv("GM_DEBUG")) printf("  log: %s\n", line);
    if ((strstr(line, "no binding") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler"))
        && (strstr(line, "TCueMacroControlForm") || strstr(line, "TSaveMacroDialog") || strstr(line, "TSaveAsMacroDialog") || strstr(line, "TLoadMacroList"))) {
        bind_complaints++; printf("  log: %s\n", line);
    }
}
static int act_count[LJ_ACT_COUNT];
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)ud; (void)arg; act_count[a]++; }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static void press_at(lj_window *win, lj_widget *w, int dx)      /* left press + release at w-relative x (dx < 0 = centre) */
{
    lj_ui_event ev;
    int ax, ay;
    if (!win || !w) { fails++; printf("FAIL press: widget missing\n"); return; }
    lj_widget_abs(w, &ax, &ay);
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.button = 1; ev.u.mouse.clicks = 1;
    ev.u.mouse.x = ax + (dx < 0 ? w->w / 2 : dx); ev.u.mouse.y = ay + w->h / 2;
    lj_window_event(win, &ev);
    ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP;
    lj_window_event(win, &ev);
}
static int row_has(lj_widget *lv, int row, const char *text)
{
    const lj_w_listview *l = (const lj_w_listview *)lv;
    return l && row >= 0 && row < l->n && l->items[row] && strstr(l->items[row], text) != NULL;
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

static void patch(lj_show *sh, int f, uint16_t profile, int addr)
{
    sh->fixture[f].kind = LJ_FX_BUILTIN; sh->fixture[f].profile_id = profile; sh->fixture[f].link = 1; sh->fixture[f].address = (uint16_t)addr;
}

static void test_editor(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_genmacro *rd;
    lj_window *w, *sv, *sl;
    lj_widget *lv;
    char t[96], path[LJ_PATH_CAP], err[128];
    int c0 = lj_addr_code(1, 1), c1 = lj_addr_code(1, 101), applies;

    bind_complaints = 0;
    w = lj_win_open(LJ_WIN_GENMACRO_EDITOR);
    CHECK(w != NULL && w == lj_genmacro_editor_get());
    if (!w) return;
    CHECK(bind_complaints == 0);
    CHECK(lj_genmacro_editor_caption(t, sizeof t) == 0 && !strcmp(t, "Generic Macro Editor []"));

    lv = in(w, "lvBuiltIn");
    CHECK(lv && ((lj_w_listview *)lv)->n == LJ_GM_SHAPES && row_has(lv, 0, "2*Sine") && row_has(lv, 12, "2*Sine^2") && row_has(lv, 65, "Static 2"));
    CHECK(in(w, "lvUser") && ((lj_w_listview *)in(w, "lvUser"))->n == 0);
    lv = in(w, "lvChannels");
    CHECK(lv && ((lj_w_listview *)lv)->n == 16);            /* MAC 500 Mode 2/4: 16 offsets */
    CHECK(row_has(lv, 11, "N/A") && row_has(lv, 10, "(16 bit MSB)"));   /* pan LSB / MSB (CHM 5583) */

    lj_w_listview_select(lv, 0);                             /* highlight offset 0 */
    press_at(w, in(w, "pbAmpPos"), in(w, "pbAmpPos") ? in(w, "pbAmpPos")->w - 1 : 0);   /* the bar's right end: +255 */
    lj_w_numedit_set_value(in(w, "eBase"), 100);
    applies = act_count[LJ_ACT_GENMACRO_APPLY];
    lj_w_listview_select(in(w, "lvBuiltIn"), 7);             /* Sine */
    CHECK(act_count[LJ_ACT_GENMACRO_APPLY] == applies + 1);
    CHECK(c->gm_edit->ch[c0].shape == 7 && c->gm_edit->ch[c0].amp_pos == 255 && c->gm_edit->ch[c0].base_on && c->gm_edit->ch[c0].base == 100);
    CHECK(c->gm_edit->ch[c1].shape == 7 && c->gm_edit->ch[c1].amp_pos == 255);   /* the slave of the same type too */
    press_at(w, in(w, "sbDirectionBounce"), -1);
    CHECK(c->gm_edit->ch[c0].dir == LJ_DIR_BOUNCE && c->gm_edit->ch[c1].dir == LJ_DIR_BOUNCE);
    lj_w_numedit_set_value(in(w, "eBase"), 40);
    CHECK(c->gm_edit->ch[c0].base == 40);
    lj_genmacro_editor_refresh();
    CHECK(row_has(lv, 0, "Sine") && row_has(lv, 0, "<>"));

    lj_w_listview_select(lv, 11);                            /* the LSB row: N/A, never programmed */
    lj_w_listview_select(in(w, "lvBuiltIn"), 42);
    CHECK(!lj_genmacro_ch_used(c->gm_edit, c0 + 11));
    lj_w_listview_select(lv, 3);
    lj_w_listview_select(in(w, "lvBuiltIn"), 43);
    lj_w_listview_select(in(w, "lvBuiltIn"), 42);            /* 2/3 Square then Square on offset 3 (a click on the highlighted row is no new selection) ... */
    CHECK(c->gm_edit->ch[c0 + 3].shape == 42);
    click(in(w, "sbClearCh"));                               /* ... and Clear Offset(s) */
    CHECK(!lj_genmacro_ch_used(c->gm_edit, c0 + 3) && !lj_genmacro_ch_used(c->gm_edit, c1 + 3) && c->gm_edit->ch[c0].shape == 7);

    /* Save Macro > Save New Macro */
    press_at(w, in(w, "sbSaveMacro"), -1);
    sv = lj_save_genmacro_get();
    CHECK(sv != NULL);
    if (sv) {
        CHECK(in(sv, "Bsave") && !in(sv, "Bsave")->enabled);   /* disabled until the macro has a number */
        lj_w_edit_set_text(in(sv, "eMacroName"), "Waves");
        click(in(sv, "BSaveNew"));
    }
    CHECK(c->gm_edit->number == 1 && !strcmp(c->gm_edit->name, "Waves"));
    CHECK(lj_library_record_path(lib, LJ_REC_MACRO, 1, path, sizeof path) == 0 && strstr(path, "00000001.CMA"));
    rd = lj_genmacro_new_rec();
    CHECK(lj_genmacro_load(path, rd, err, sizeof err) == 0 && rd->ch[c0].shape == 7 && rd->ch[c1].dir == LJ_DIR_BOUNCE && !strcmp(rd->name, "Waves"));
    free(rd);
    CHECK(lj_genmacro_editor_caption(t, sizeof t) == 0 && !strcmp(t, "Generic Macro Editor [0001 Waves]"));
    lj_genmacro_editor_refresh();
    golden(w, "W3GM_EDITOR");

    /* Select Macro and the hotkeys */
    lj_genmacro_editor_bind_fns();
    lj_fn_run(LJ_FN_GENMACRO_TOGGLE_LIST, 1);
    sl = lj_select_genmacro_get();
    CHECK(sl != NULL && in(sl, "lvMacros") && ((lj_w_listview *)in(sl, "lvMacros"))->n == 1 && row_has(in(sl, "lvMacros"), 0, "0001\tWaves"));
    lj_fn_run(LJ_FN_GENMACRO_TOGGLE_EDITOR, 1);
    CHECK(lj_genmacro_editor_get() == NULL);
    lj_fn_run(LJ_FN_GENMACRO_CLEAR_EDITOR, 1);
    CHECK(c->gm_edit->number == 0 && !lj_genmacro_ch_used(c->gm_edit, c0));
}

int lj_test_genmacro_editor(int argc, char **argv);
int lj_test_genmacro_editor(int argc, char **argv)
{
    static lj_library lib;
    lj_ctx *c = lj_ctx_get();
    int a;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->gm_edit = lj_genmacro_new_rec(); c->gm_shapes = &lj_gm_shapes_builtin;
    patch(c->show, 0, 12, 1); patch(c->show, 1, 12, 101); patch(c->show, 2, 19, 201);   /* two MAC 500 Mode 2/4 + a dimmer of another type */
    lj_sel_set(0, 1, 1); lj_sel_set(1, 1, 0); lj_sel_set(2, 1, 0);
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, on_action, NULL);

    test_editor(&lib);

    lj_win_close_all();
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, NULL, NULL);
    lj_sel_none();
    free(c->gm_edit); c->gm_edit = NULL; c->gm_shapes = NULL;
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(c->show); c->show = NULL; c->lib = NULL;
    lj_log_set_sink(NULL, NULL);
    printf("test_genmacro_editor: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
