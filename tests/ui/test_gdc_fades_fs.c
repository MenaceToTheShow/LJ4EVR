/* test_gdc_fades_fs.c - `lj4evr_tests test_gdc_fades_fs`: the gdc-fades-followspot windows headless (no lj_win_host).
 *  - ME-16: opening TGenericMegaForm, TDefault2Form, TFollowSpotForm, TFollowListForm and the three Generic DMX sub-forms
 *    logs neither "no binding for handler" nor "matches no DFM handler" (lj_window.c, both directions).
 *  - Generic DMX Control: the command line writes the current scene (values, Fade unless Snap), "N Channels", the
 *    " (N Channels Hidden)" caption, Recall, groups + Hide saved to GenericGroups.dat and read back.
 *  - Sequence fades: Intensity + All Off on the selected MAC only, values kept; Select All toggles the eight boxes.
 *  - Follow spot: Apply Current Scene records the MAC (the dimmer ignored, the box text), button captions, Save writes
 *    FollowSpot.dat; the list shows the used settings and OK runs LJ_ACT_FOLLOWSPOT_APPLY with the setting.
 *  - Goldens W3GDC_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "gfx/lj_bmp.h"
#include "model/lj_followspot.h"
#include "model/lj_gdc.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_gdc_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_gdc_fs_tmp/"


static int fails, unbound;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static void sink(void *ud, const char *line) { (void)ud; if (strstr(line, "no binding for handler") || strstr(line, "matches no DFM handler")) { unbound++; printf("  log: %s\n", line); } }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static const char *cap(lj_window *w, const char *n) { const char *c = lj_w_caption(in(w, n)); return c ? c : ""; }
static void menu(lj_window *w, const char *item) { lj_menu_rt *rt = w ? lj_menu_rt_named(lj_window_menus(w), item) : NULL; if (rt && rt->fn) rt->fn(lj_window_root(w), rt->ud); else { fails++; printf("FAIL menu %s\n", item); } }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); click(m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL); }
static void press_at(lj_window *win, lj_widget *w, int dx, int dy)    /* a left press + release at w-relative dx,dy (dx < 0 = the centre) */
{
    lj_ui_event ev;
    int ax, ay;
    if (!win || !w) { fails++; printf("FAIL press: widget missing\n"); return; }
    lj_widget_abs(w, &ax, &ay);
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.button = 1; ev.u.mouse.clicks = 1;
    ev.u.mouse.x = ax + (dx < 0 ? w->w / 2 : dx); ev.u.mouse.y = ay + (dx < 0 ? w->h / 2 : dy);   /* lj_widget_abs = surface coordinates */
    lj_window_event(win, &ev);
    ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP;
    lj_window_event(win, &ev);
}
static void press(lj_window *win, lj_widget *w) { press_at(win, w, -1, -1); }   /* the TPanel buttons act on OnMouseDown */
static int act_n, act_arg;
static void fs_close_click(lj_window *w) { lj_ui_event ev; memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CLOSE; lj_window_event(w, &ev); }
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)ud; if (a == LJ_ACT_FOLLOWSPOT_APPLY) { act_n++; act_arg = arg; } }

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

/* fixtures: 1 = DMX Dimmer 8 at 1:1, 2 = MAC 500 at 1:21 */
static void make_show(lj_ctx *c)
{
    c->show = lj_show_new();
    c->show->fixture[0].kind = LJ_FX_BUILTIN; c->show->fixture[0].profile_id = 21; c->show->fixture[0].link = 1; c->show->fixture[0].address = 1;
    c->show->fixture[1].kind = LJ_FX_BUILTIN; c->show->fixture[1].profile_id = 12; c->show->fixture[1].link = 1; c->show->fixture[1].address = 21;
    c->show->current_seq = lj_sequence_new_rec(); c->current_scene = 0;
}

static void test_gdc(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc = &c->show->current_seq->scenes[0];
    lj_gdc_groups back;
    char path[LJ_PATH_CAP], err[160];
    lj_window *w = lj_gdc_form_open();
    CHECK(w != NULL && lj_win_get(LJ_WIN_GDC_FORM) == w);
    if (!w) return;
    CHECK(strcmp(lj_window_caption(w)->text, "Generic DMX Control") == 0);
    CHECK(strcmp(cap(w, "pSelected"), "0 Channels") == 0);
    sc->state[1] = LJ_SNAP;
    CHECK(lj_gdc_form_command("1/4@50") == 0);                      /* % values by default: 50 % = 128 */
    CHECK(sc->val[0] == 128 && sc->state[0] == LJ_FADE && sc->val[1] == 128 && sc->state[1] == LJ_SNAP && sc->state[4] == LJ_OFF);
    CHECK(strcmp(cap(w, "pSelected"), "4 Channels") == 0);
    CHECK(strcmp(lj_w_edit_text(in(w, "eCommand")), "") == 0);      /* cleared after [Enter] */
    CHECK(lj_gdc_form_command("1/#2") == -1);                         /* an input error leaves the scene */
    CHECK(sc->val[0] == 128);
    press(w, in(w, "pRecall"));
    CHECK(strcmp(lj_w_edit_text(in(w, "eCommand")), "1/4@50") == 0);
    CHECK(lj_gdc_form_command("21/40") == 0 && strcmp(cap(w, "pSelected"), "0 Channels") == 0);   /* the MAC is not mapped */
    CHECK(lj_gdc_form_command("2+3s") == 0);
    press(w, in(w, "pFade"));                                        /* the Fade panel: Snap 2,3 -> Fade */
    CHECK(sc->state[1] == LJ_FADE && sc->state[2] == LJ_FADE);
    press(w, in(w, "pb1"));                                          /* the level fader mid-way: both selected channels, one value */
    CHECK(sc->val[1] == sc->val[2] && sc->val[1] > 60 && sc->val[1] < 190 && sc->val[0] == 128);
    press(w, in(w, "sbFaderUp"));
    CHECK(sc->val[1] == sc->val[2] && sc->val[1] > 60);
    {
        lj_rect r;
        CHECK(lj_w_grid_cell_rect(in(w, "DG1"), 4, 0, &r) == 0);    /* cell 5 = channel 5 */
        press_at(w, in(w, "DG1"), r.x + r.w / 2, r.y + r.h / 2);
        CHECK(strcmp(cap(w, "pSelected"), "1 Channels") == 0 && c->gdc == NULL);
        CHECK(lj_gdc_form_command("s@25") == 0 && sc->val[4] == 64 && sc->state[4] == LJ_SNAP);
        CHECK(lj_gdc_form_command("2+3") == 0);
    }
    golden(w, "W3GDC_GENERIC_DMX");
    /* groups and hide, through the model the popup leaves call, then the file */
    c->show->gdc.used[0] = 1; strcpy(c->show->gdc.name[0], "Pair"); c->show->gdc.member[5] = c->show->gdc.member[6] = 1u;
    CHECK(lj_gdc_form_command("#1@100") == 0 && sc->val[5] == 255 && sc->val[6] == 255);
    CHECK(lj_gdc_form_command("7") == 0);
    menu(w, "HideSelectedChannels1");                                /* Options > Hide Selected Channels */
    CHECK(c->show->gdc.hidden[6] == 1);
    menu(w, "GridSize1"); menu(w, "GridColors1"); menu(w, "P_CreateGroup");   /* the three sub-forms build with every handler bound (ME-16) */
    CHECK(lj_gdc_form_command("##") == 0);
    CHECK(strcmp(lj_window_caption(w)->text, "Generic DMX Control (1 Channels Hidden)") == 0);
    CHECK(strcmp(cap(w, "pSelected"), "7 Channels") == 0);
    CHECK(lj_library_path(lib, LJ_GDC_GROUPS_FILE, path, sizeof path) == 0 && lj_gdc_groups_save(path, &c->show->gdc) == 0);
    CHECK(lj_gdc_groups_load(path, &back, err, sizeof err) == 0 && back.used[0] && back.hidden[6] && back.member[5] == 1u && strcmp(back.name[0], "Pair") == 0);
    c->show->gdc.hidden[6] = 0;
    lj_gdc_form_close();
    CHECK(lj_win_get(LJ_WIN_GDC_FORM) == NULL);
}

static void test_seq_fades(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc = &c->show->current_seq->scenes[0];
    lj_window *w = lj_seq_fades_open();
    CHECK(w != NULL);
    if (!w) return;
    CHECK(strcmp(lj_window_caption(w)->text, "Set Fade Status") == 0);
    golden(w, "W3GDC_SET_FADE_STATUS");
    click(in(w, "sbToggle"));
    CHECK(lj_w_checkbox_checked(in(w, "cbIntensity")) && lj_w_checkbox_checked(in(w, "cbMovementmacro")));
    click(in(w, "sbToggle"));
    CHECK(!lj_w_checkbox_checked(in(w, "cbIntensity")) && !lj_w_checkbox_checked(in(w, "cbLevel")));
    lj_w_checkbox_set_checked(in(w, "cbIntensity"), 1);
    lj_w_radiogroup_set_index(in(w, "RG1"), 0);                      /* All Off */
    sc->val[21] = 200; sc->state[21] = LJ_FADE; sc->val[30] = 99; sc->state[30] = LJ_FADE;   /* MAC intensity 1:22, pan 1:31 */
    sc->state[0] = LJ_FADE;                                          /* dimmer, unselected */
    lj_sel_none(); lj_sel_set(1, 1, 1);
    click(in(w, "bOk"));
    CHECK(sc->state[21] == LJ_OFF && sc->val[21] == 200 && sc->state[30] == LJ_FADE && sc->state[0] == LJ_FADE);
    CHECK(lj_seq_fades_get() == NULL);                               /* OK closes */
}

static void test_followspot(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc = &c->show->current_seq->scenes[0];
    lj_followspot back[LJ_FOLLOWSPOT];
    char path[LJ_PATH_CAP], err[160];
    lj_window *w = lj_followspot_form_open(), *l, *m;
    CHECK(w != NULL);
    if (!w) return;
    CHECK(strcmp(lj_window_caption(w)->text, "Edit FollowSpot Settings") == 0);
    CHECK(strcmp(cap(w, "bApplyScene"), "&Apply Current Scene to 1") == 0 && strcmp(cap(w, "bClear"), "&Clear Settings in 1") == 0);
    lj_sel_none(); lj_sel_set(0, 1, 1);
    CHECK(lj_followspot_form_apply_scene() == -1);                    /* only the dimmer */
    m = lj_win_message_window();
    CHECK(m != NULL && strcmp(lj_w_caption(lj_window_find(m, "Label1")) ? lj_w_caption(lj_window_find(m, "Label1")) : "", "No Followspot Fixtures Selected") == 0);
    msg_answer(1);
    sc->val[21] = 180; sc->state[21] = LJ_SNAP;
    lj_sel_set(1, 1, 0);
    CHECK(lj_followspot_form_apply_scene() == 0);
    CHECK(c->show->followspot[0].used && c->show->followspot[0].selected[1] && !c->show->followspot[0].selected[0] && c->show->followspot[0].val[21] == 180);
    strcpy(c->show->followspot[0].name, "Guitar Solo 1");
    fs_close_click(w);                                                 /* modified: "Followspot settings changed but not saved - save now?" */
    CHECK(lj_followspot_form_get() == w && lj_win_message_window() != NULL);
    msg_answer(0);                                                     /* no: closes without saving */
    CHECK(lj_followspot_form_get() == NULL);
    w = lj_followspot_form_open();
    golden(w, "W3GDC_EDIT_FOLLOWSPOT");
    click(in(w, "bSave"));
    CHECK(lj_library_path(lib, LJ_FOLLOWSPOT_FILE, path, sizeof path) == 0 && lj_followspot_load(path, back, err, sizeof err) == 0 && back[0].used && strcmp(back[0].name, "Guitar Solo 1") == 0);
    lj_followspot_form_close();
    l = lj_followspot_list_open();
    CHECK(l != NULL);
    if (!l) return;
    CHECK(strcmp(lj_window_caption(l)->text, "Select Followspot Setting") == 0);
    CHECK(!in(l, "bOk")->enabled);
    lj_w_listview_select(in(l, "lv1"), 0);
    CHECK(in(l, "bOk")->enabled);
    golden(l, "W3GDC_SELECT_FOLLOWSPOT");
    click(in(l, "bOk"));
    CHECK(act_n == 1 && act_arg == 1);
    CHECK(lj_followspot_list_get() == NULL);
    l = lj_followspot_list_open();
    click(in(l, "bClear"));
    CHECK(act_n == 2 && act_arg == 0);
    lj_followspot_list_close();
}

int lj_test_gdc_fades_fs(int argc, char **argv);
int lj_test_gdc_fades_fs(int argc, char **argv)
{
    static lj_library lib;
    lj_ctx *c = lj_ctx_get();
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    c->lib = &lib; c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    make_show(c);
    lj_desktop_bind_action(LJ_ACT_FOLLOWSPOT_APPLY, on_action, NULL);
    test_gdc(&lib);
    test_seq_fades();
    test_followspot(&lib);
    CHECK(unbound == 0);                                               /* ME-16 */
    lj_log_set_sink(NULL, NULL);
    printf("test_gdc_fades_fs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
