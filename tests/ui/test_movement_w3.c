/* test_movement_w3.c - `lj4evr_tests test_movement_w3` (W3 movement): the five windows of lj_movemacro_editor.c headless
 * against a five-MAC 500 show (built-in profile 12, 16-bit pan 11/12 tilt 13/14) and a live engine.
 *  - ME-16: every window builds with its DFM handlers bound (lj_window_create logs / asserts on a miss);
 *  - Movement Macro: caption, the Macro Type list ("No Macro" + the six system names verbatim), picking a type writes
 *    every selected fixture's macro with Snap states, the bars write amplitude / cycle / delay, the engine overlays the
 *    master's 16-bit pan exactly as lj_movemacro_pos says (ME-22), Stepped Ellipse shows the Stepsize bar;
 *  - Bezier: New Shape opens the editor ("New Shape (1)"), Save Shape writes <lib>\MacroShapes\<name>.bez;
 *  - Delete Macros: lists the shapes with a non-blank Modified time (LORD_FIDELITY-12), OK deletes the checked file;
 *    a scene macro on a deleted shape is inactive (lj_movemacro_pos -1) and the type list shows "load error";
 *  - Fan Out: Delta Pan 95 / Center Delta on Home on five fixtures = the 17 s5.2 offsets; the Custom order reorders;
 *  - Auto Delay: Linear 256 on five fixtures = 0, 51, 102, 153, 204; Circular / Segments as 17 s6;
 *  - Cue Macro Control: the faders post MACRO_CTL (view mm_amp / mm_speed), Freeze / Fade Out post MACRO_FADE;
 *  - goldens W3MM_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "engine/lj_movemacro.h"
#include "gfx/lj_bmp.h"
#include "model/lj_library.h"
#include "model/lj_movemacro.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_dragbar.h"
#include "ui/widgets/lj_w_fader.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "windows/lj_movemacro_editor.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_movement_w3_tmp/"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static int me16;                                             /* ME-16: lj_window_create complaints about this file's windows */
static void quiet(void *ud, const char *line) { (void)ud; if (strstr(line, "no binding for handler") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler") || strstr(line, "has no behaviour file")) { me16++; printf("ME-16: %s\n", line); } }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void menu_fire(lj_window *win, const char *name)      /* a click on the item: its bound OnClick with the item firing */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, name) : NULL;
    CHECK(rt && rt->fn);
    if (!rt || !rt->fn) return;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
}
static void bar_set(lj_widget *w, int v) { CHECK(w != NULL); if (w) { lj_w_dragbar_set_value(w, v); lj_w_emit(w, LJ_WEV_CHANGE); } }
static void combo_pick(lj_widget *w, int i) { CHECK(w != NULL); if (w) { ((lj_w_combobox *)w)->index = i; lj_w_emit(w, LJ_WEV_CHANGE); } }

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

static int32_t pan16(const lj_frame *f, int fx) { int ch = 16 * fx + 10; return (f->merged[ch] << 8 | f->merged[ch + 1]) - 32768; }
static int32_t scene_pan(const lj_scene *sc, int fx) { int ch = 16 * fx + 10; return (sc->val[ch] << 8 | sc->val[ch + 1]) - 32768; }

static lj_show *show; static lj_engine *eng; static uint64_t now_us;
static void tick(int n) { while (n-- > 0) { now_us += lj_engine_tick_us(eng); lj_engine_tick(eng, now_us); } lj_engine_view(eng, &lj_ctx_get()->view); }

static void test_movemacro_window(lj_scene *sc)
{
    lj_window *w = lj_win_open(LJ_WIN_MOVEMACRO_EDITOR);
    lj_widget *type = in(w, "MmType");
    lj_mm_state st; int32_t p, t; int i;
    CHECK(w && strcmp(lj_window_caption(w)->text, "Movement Macro") == 0);
    CHECK(type && ((lj_w_combobox *)type)->n == LJ_MM_TYPE_BEZ0 && !strcmp(((lj_w_combobox *)type)->items[3], "System Cos(2*a),Sin(a)"));
    combo_pick(type, LJ_MM_TYPE_ELLIPSE);
    for (i = 0; i < 3; i++) CHECK(sc->macro[i].type == LJ_MM_TYPE_ELLIPSE && sc->macro[i].fstate[LJ_MMP_TYPE] == LJ_SNAP);
    CHECK(sc->macro[3].type == 0);                           /* fixture 4 is not selected */
    bar_set(in(w, "MmPanAmp"), 127); bar_set(in(w, "MmTiltAmp"), 100); bar_set(in(w, "MmPanCycle"), 32); bar_set(in(w, "MmDelay"), 64);
    CHECK(sc->macro[1].amp_pan == 127 && sc->macro[1].amp_tilt == 100 && sc->macro[1].cycle_pan == 32 && sc->macro[1].cycle_tilt == 32 && sc->macro[1].delay == 64);   /* the ellipse's one cycle */
    CHECK(!in(w, "MmTiltCycle")->visible && !in(w, "MmStep")->visible);
    tick(17);                                                /* the engine overlays the 16-bit pan (ME-22): exactly lj_movemacro_pos at the view's index */
    st.idx_pan = (uint32_t)lj_ctx_get()->view.mm_idx_pan[0] << 16; st.idx_tilt = (uint32_t)lj_ctx_get()->view.mm_idx_tilt[0] << 16;
    CHECK(lj_ctx_get()->view.mm_idx_pan[0] == 16 * 32);
    CHECK(lj_movemacro_pos(&st, &sc->macro[0], NULL, 100, 0, 0, &p, &t) == 0 && pan16(lj_engine_frame(eng), 0) == p);
    golden(w, "W3MM_MOVEMACRO");
    combo_pick(type, LJ_MM_TYPE_STEPPED_ELLIPSE);
    CHECK(in(w, "MmStep")->visible && !in(w, "MmTweak")->visible);
    bar_set(in(w, "MmStep"), 511);
    CHECK(sc->macro[0].step == 512);
    /* Bezier: New Shape extends the window, Save Shape writes the file */
    menu_fire(w, "M_NewShape");
    CHECK(lj_window_root(w)->w > 441 && in(w, "MmBezier")->visible);
    CHECK(!strcmp(((lj_w_combobox *)type)->items[((lj_w_combobox *)type)->n - 1], "New Shape (1)"));
    golden(w, "W3MM_BEZIER");
    menu_fire(w, "M_SaveShape");
    CHECK(lj_movemacro_editor_name_window() != NULL);       /* an unsaved shape asks for its name (TBezNameForm) */
    if (lj_movemacro_editor_name_window()) { lj_w_edit_set_text(in(lj_movemacro_editor_name_window(), "EName"), "Diamond"); lj_w_click(in(lj_movemacro_editor_name_window(), "bSave")); }
    CHECK(lj_movemacro_editor_name_window() == NULL);
    { FILE *f = fopen(TMP "User/Lib/MacroShapes/Diamond.bez", "rb"); CHECK(f != NULL); if (f) fclose(f); }
    lj_win_close(LJ_WIN_MOVEMACRO_EDITOR);
}

static void test_delete_macros(lj_scene *sc, const lj_library *lib)
{
    static lj_bez_set set;
    lj_window *w;
    lj_widget *lb;
    lj_mm_state st = { 0, 0 }; int32_t p, t;
    char dir[LJ_PATH_CAP];
    lj_library_path(lib, LJ_BEZ_DIR, dir, sizeof dir);
    CHECK(lj_bez_set_load(dir, &set) == 0 && set.n == 1 && !strcmp(set.t[0].name, "Diamond"));
    show->bez = &set;                                        /* what lj_app_bez_reload does */
    sc->macro[0].type = LJ_MM_TYPE_BEZ0; sc->macro[0].fstate[LJ_MMP_TYPE] = LJ_SNAP;
    CHECK(lj_movemacro_pos(&st, &sc->macro[0], show->bez, 100, 0, 0, &p, &t) == 0);
    w = lj_win_open(LJ_WIN_DELETE_MOVEMACRO);
    lb = in(w, "LB1");
    CHECK(w && lb && ((lj_w_list *)lb)->n == 1 && strncmp(((lj_w_list *)lb)->items[0], "Diamond", 7) == 0);
    CHECK(lb && strchr(((lj_w_list *)lb)->items[0] + 8, '-') != NULL);   /* LORD_FIDELITY-12: the Modified time is there */
    if (lb) lj_w_list_set_checked(lb, 0, 1);
    lj_w_click(in(w, "bOk"));
    { FILE *f = fopen(TMP "User/Lib/MacroShapes/Diamond.bez", "rb"); CHECK(f == NULL); if (f) fclose(f); }
    CHECK(lj_win_get(LJ_WIN_DELETE_MOVEMACRO) == NULL);
    lj_bez_set_free(&set);
    CHECK(lj_bez_set_load(dir, &set) == 0 && set.n == 0);   /* the reload the app runs: the user's shape is gone */
    CHECK(lj_movemacro_pos(&st, &sc->macro[0], show->bez, 100, 5, 6, &p, &t) == -1 && p == 5);   /* the scene's macro on it: inactive */
    w = lj_win_open(LJ_WIN_MOVEMACRO_EDITOR);
    CHECK(w && ((lj_w_combobox *)in(w, "MmType"))->index == -1 && !strcmp(((lj_w_combobox *)in(w, "MmType"))->text, "load error"));
    lj_win_close(LJ_WIN_MOVEMACRO_EDITOR);
    show->bez = NULL;
    memset(&sc->macro[0], 0, sizeof sc->macro[0]);
}

static void test_fan_out(lj_scene *sc)
{
    static const int32_t want[5] = { -24320, -12160, 0, 12160, 24320 };   /* 17 s5.2: five fixtures, Delta Pan 95, home */
    lj_window *w = lj_win_open(LJ_WIN_FAN_OUT);
    int i;
    CHECK(w && strcmp(lj_window_caption(w)->text, "Fan Out") == 0);
    for (i = 0; i < 5; i++) lj_sel_set(i, 1, i == 0);
    lj_fan_out_refresh();
    lj_w_checkbox_set_checked(in(w, "cbCenter"), 1);
    bar_set(in(w, "DeltaPan"), 95);
    for (i = 0; i < 5; i++) CHECK(scene_pan(sc, i) == want[i]);
    CHECK(sc->val[16 * 2 + 10] == 128 && sc->val[16 * 2 + 11] == 0 && sc->state[16 * 2 + 11] == LJ_SNAP);   /* one 16-bit value with one state (ME-22) */
    lj_w_radiogroup_set_index(in(w, "rg1"), 3);              /* Custom: the list, first row moved to the end */
    CHECK(lj_fan_out_custom_move(0, 4) == 0);
    CHECK(lj_fan_out_apply() == 0 && scene_pan(sc, 0) == 24320 && scene_pan(sc, 1) == -24320);
    lj_w_click(in(w, "bPan"));                               /* Spread between the first and last in order (1 .. 0): the symmetric fan stays */
    CHECK(scene_pan(sc, 2) == -12160 && scene_pan(sc, 3) == 0 && scene_pan(sc, 4) == 12160);
    golden(w, "W3MM_FANOUT");
    lj_win_close(LJ_WIN_FAN_OUT);
}

static void test_auto_delay(lj_scene *sc)
{
    lj_window *w = lj_win_open(LJ_WIN_AUTO_DELAY);
    int i;
    CHECK(w && strcmp(lj_window_caption(w)->text, "Pan/Tilt Macro Auto Delay") == 0);
    CHECK(!strcmp(lj_w_caption(in(w, "LFixMax")), "5 "));
    lj_auto_delay_set(256, 3, 3);
    CHECK(lj_auto_delay_apply() == 0);
    for (i = 0; i < 5; i++) CHECK(sc->macro[i].delay == 256 * i / 5);   /* 0, 51, 102, 153, 204: never two the same [04 s4.6] */
    lj_w_radiogroup_set_index(in(w, "RGStyle"), 1);          /* Circular, count 3: 0, 85, 170, 0, 85 */
    CHECK(lj_auto_delay_apply() == 0 && sc->macro[3].delay == 0 && sc->macro[4].delay == 85);
    lj_w_radiogroup_set_index(in(w, "RGStyle"), 2);          /* Segments 3 */
    CHECK(lj_auto_delay_apply() == 0 && sc->macro[0].delay == 0 && sc->macro[1].delay == 0 && sc->macro[2].delay == 85 && sc->macro[4].delay == 170);
    lj_w_radiogroup_set_index(in(w, "RGOrder"), 3);          /* Fixture Number - Reversed, Linear */
    lj_w_radiogroup_set_index(in(w, "RGStyle"), 0);
    CHECK(lj_auto_delay_apply() == 0 && sc->macro[4].delay == 0 && sc->macro[0].delay == 204);
    golden(w, "W3MM_AUTODELAY");
    lj_win_close(LJ_WIN_AUTO_DELAY);
}

static void test_cue_macro(void)
{
    lj_window *w = lj_win_open(LJ_WIN_CUE_MACRO_CONTROL);
    CHECK(w && strcmp(lj_window_caption(w)->text, "Cue Macro Control") == 0);
    lj_w_fader_set_value(in(w, "FaderAmp"), 45); lj_w_emit(in(w, "FaderAmp"), LJ_WEV_CHANGE);
    tick(1);
    CHECK(lj_ctx_get()->view.mm_amp_pct == 45 && lj_ctx_get()->view.mm_speed_pct == 100);
    lj_w_click(in(w, "sbFreezeMacro"));
    lj_w_emit(in(w, "sbFreezeMacro"), LJ_WEV_MOUSE_UP);
    tick(1);
    CHECK(lj_ctx_get()->view.mm_frozen == 1 && lj_ctx_get()->view.mm_speed_pct == 0);
    lj_w_click(in(w, "bFadeOut"));
    tick(2);
    CHECK(lj_ctx_get()->view.mm_fade == 1);
    tick(60);
    CHECK(lj_ctx_get()->view.mm_amp_pct == 0 && lj_ctx_get()->view.mm_fade == 0);
    lj_w_click(in(w, "bMacroOn"));
    tick(1);
    CHECK(lj_ctx_get()->view.mm_amp_pct == 45);
    menu_fire(w, "M_Pct_2");                                 /* Fader Range 1-999% */
    lj_cue_macro_control_set(490, 100);
    tick(1);
    CHECK(lj_ctx_get()->view.mm_amp_pct == 490);
    golden(w, "W3MM_CUEMACRO");
    lj_win_close(LJ_WIN_CUE_MACRO_CONTROL);
}

int lj_test_movement_w3(int argc, char **argv);
int lj_test_movement_w3(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    static lj_library lib;
    lj_sequence *seq;
    lj_scene *sc;
    int i;
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    lj_fs_mkdir(TMP); lj_fs_mkdir(TMP "User");
    CHECK(lj_library_create(TMP, "Lib", &lib) == 0);
    show = lj_show_new();
    seq = (lj_sequence *)calloc(1, sizeof *seq);
    if (!show || !seq) return 1;
    seq->scenes = (lj_scene *)calloc(1, sizeof *seq->scenes); seq->n = 1; seq->number = 1;
    lj_scene_init(&seq->scenes[0]);
    sc = &seq->scenes[0];
    show->current_seq = seq;
    for (i = 0; i < 5; i++) {                                /* five MAC 500 Mode 2/4 homed: pan / tilt 32768 Snap */
        show->fixture[i].profile_id = 12; show->fixture[i].link = 1; show->fixture[i].address = (uint16_t)(1 + 16 * i);
        snprintf(show->fixture[i].user_name, sizeof show->fixture[i].user_name, "Spot %d", i + 1);
        sc->val[16 * i + 10] = sc->val[16 * i + 12] = 128; sc->state[16 * i + 10] = sc->state[16 * i + 11] = sc->state[16 * i + 12] = sc->state[16 * i + 13] = LJ_SNAP;
    }
    eng = lj_engine_create();
    if (!eng) return 1;
    lj_engine_bind(eng, show);
    c->show = show; c->engine = eng; c->lib = &lib; c->current_scene = 0;
    lj_ctx_set_scene(0);                                     /* the current sequence runs */
    for (i = 0; i < 3; i++) lj_sel_set(i, 1, i == 0);
    test_movemacro_window(sc);
    test_delete_macros(sc, &lib);
    test_fan_out(sc);
    test_auto_delay(sc);
    test_cue_macro();
    CHECK(me16 == 0);
    c->show = NULL; c->engine = NULL; c->lib = NULL;
    lj_engine_destroy(eng);
    lj_show_free(show);
    lj_log_set_sink(NULL, NULL);
    printf("test_movement_w3: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
