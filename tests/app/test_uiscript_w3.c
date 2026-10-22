/* test_uiscript_w3.c - the runnable check for the W3 desktop-app track (src/app/lj_app_w3.c, the W3 verbs of
 * src/app/lj_uiscript.c, the ME-102 paths of src/app/lj_app_seam.c, the W3 rows of src/desktop): every new verb
 * parses and a bad line reports its number; every W3 menu leaf and C-L / follow spot toolbar button reaches a binding
 * (no "is not bound" / "no binding for" line); lj_app_cuelist_step counts its calls (the loop calls it once per idle
 * pass, which `lj4evr --selftest` proves on the real loop); a .Cl2 with an MP Play line loads red and the runner
 * latches the cues around it through the app's host; assert_pos reads a 16-bit pan / tilt; mm_set writes the scene;
 * assert_prompt reads the message box; gs2_import reports the importer's message; ME-102: with the engine's command
 * array full, Clear Slot and a BG cue load keep the record the engine still holds (ASan: no use-after-free). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdint.h>
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
#include "app/lj_app_seam.h"
#include "app/lj_app_w3.h"
#include "app/lj_uiscript.h"
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_movement.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "model/lj_statics.h"
#include "core/lj_fsinfo.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_bgcue_form.h"
#include "windows/lj_genmacro_editor.h"
#include "windows/lj_gs2_dialogs.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char logged[8192];
static void sink(void *ud, const char *line) { (void)ud; if (strlen(logged) + strlen(line) + 2 < sizeof logged) { strcat(logged, line); strcat(logged, "\n"); } }
static const lj_engine_status *view(void *ud) { (void)ud; return &lj_ctx_get()->view; }
static int write_file(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (!f) return -1; fputs(text, f); fclose(f); return 0; }

static int fire_menu(lj_window *win, const char *item)   /* runs a menu item's bound handler as lj_menu's fire() does */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = lj_menu_rt_named(m, item);
    if (!rt || !rt->fn) return -1;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}

static int run_script(const char *path)
{
    int r = lj_uiscript_load(path);
    uint64_t t0 = lj_clock_us();
    if (r != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 15000000u) { }
    return r;
}
static int run_text(const char *text) { write_file("lj_w3_run.lj4s", text); return run_script("lj_w3_run.lj4s"); }

static void bad_line(const char *line)
{
    char text[256];
    snprintf(text, sizeof text, "log \"ok\"\n%s\n", line);
    write_file("lj_w3_bad.lj4s", text);
    if (lj_uiscript_load("lj_w3_bad.lj4s") != -1 || lj_uiscript_line() != 2) { fails++; printf("FAIL: \"%s\" was not rejected on line 2 (line %d)\n", line, lj_uiscript_line()); }
}

static void tick(lj_engine *e, uint64_t *t) { *t += 25000u; lj_lock(); lj_engine_tick(e, *t); lj_unlock(); lj_app_service_pending(); }

int lj_test_uiscript_w3(int argc, char **argv);
int lj_test_uiscript_w3(int argc, char **argv)
{
    static const char every_w3_verb[] =
        "cl_load CueLists/00000001.Cl2\n"
        "cl_goto 3\n"
        "cl_execute\n"
        "cl_assert_line 2 error\n"
        "cl_assert_line 1 plain\n"
        "cl_assert_line 4 next\n"
        "cl_assert_line 5 done\n"
        "assert_pos 1 0 -16384\n"
        "assert_pos TCurrentCue 10 20\n"           /* the W1 form keeps working */
        "gs2_import fixtures/x.Udf\n"
        "assert_prompt \"Please Confirm..\"\n"
        "mm_set 1 1 100 50\n"
        "mm_set 100 255 0 255\n"
        "assert_view gm_engine_on=1 gm_slot_on=1 gm_cue_number=0 gm_editor_number=0\n"
        "assert_view mm_amp_pct=100 mm_speed_pct=100 mm_fade=0 mm_frozen=0\n"
        "assert_view pending_genmacro 0\n";
    lj_ctx *c = lj_ctx_get();
    lj_library lib;
    lj_show *show;
    lj_engine *e;
    lj_hotkeys hk;
    lj_uiscript_host host;
    lj_window *desk;
    lj_cuelist *cl;
    lj_cl_line ln;
    lj_cue *q;
    char path[LJ_PATH_CAP];
    uint64_t t = lj_clock_us();
    uint32_t s0;
    int i;
    (void)argc; (void)argv;

    /* ---- 1. parsing: every W3 verb loads; a bad line reports its number ---- */
    write_file("lj_w3_verbs.lj4s", every_w3_verb);
    CHECK(lj_uiscript_load("lj_w3_verbs.lj4s") == 0);
    bad_line("cl_load");                       /* argument count */
    bad_line("cl_goto 0");
    bad_line("cl_goto three");
    bad_line("cl_execute now");
    bad_line("cl_assert_line 1 grey");
    bad_line("cl_assert_line x done");
    bad_line("assert_pos 0 0 0");              /* fixture 1..100 */
    bad_line("assert_pos 101 0 0");
    bad_line("assert_pos 5 40000 0");          /* readout -32768..32767 */
    bad_line("assert_pos 5 0 x");
    bad_line("gs2_import");
    bad_line("assert_prompt");
    bad_line("mm_set 0 1 1 1");
    bad_line("mm_set 1 256 0 0");
    bad_line("mm_set 1 1 -1 0");
    bad_line("mm_set 1 1 1");
    bad_line("assert_view mm_amp_pct=x");
#ifdef LJ_SOURCE_DIR
    {   char p[512];                          /* the W3 walkthrough parses (every line checked at load) */
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w3_acceptance.lj4s", LJ_SOURCE_DIR);
        CHECK(lj_uiscript_load(p) == 0); }
#endif

    /* ---- 2. the app context headless: library, show (MAC 500 at 1:001, a dimmer at 1:100), engine, desktop, seam + W3 start ---- */
    LJ_MKDIR("lj_w3_app");
    memset(&lib, 0, sizeof lib);
    if (lj_library_open("lj_w3_app/", "W3App", &lib) != 0 && lj_library_create("lj_w3_app/", "W3App", &lib) != 0) { printf("FAIL: no library under lj_w3_app/\n"); return 1; }
    show = lj_show_new();
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].profile_id = 12; show->fixture[0].link = 1; show->fixture[0].address = 1;
    show->fixture[1].kind = LJ_FX_BUILTIN; show->fixture[1].profile_id = 19; show->fixture[1].link = 1; show->fixture[1].address = 100;
    show->current_seq = lj_sequence_new_rec();
    if (!show->current_seq->scenes) lj_sequence_alloc(show->current_seq, 1);
    show->current_cue = lj_cue_new_rec();
    e = lj_engine_create();
    lj_hotkeys_init(&hk);
    memset(c, 0, sizeof *c);
    c->show = show; c->engine = e; c->lib = &lib; c->hotkeys = &hk; c->main_ini = lj_ini_new();
    lj_lock(); lj_engine_bind(e, show); lj_unlock();
    host.ud = NULL; host.resolve = NULL; host.view = view;
    lj_uiscript_set_host(&host);
    desk = lj_desktop_create();
    CHECK(desk != NULL);
    lj_app_seam_start();
    lj_app_w3_start();
    CHECK(c->cl != NULL && c->gm_edit != NULL && c->gm_shapes != NULL && c->gdc != NULL && show->current_cl != NULL);

    /* ---- 3. every W3 menu leaf / C-L and follow spot toolbar button has a binding ---- */
    {   static const enum lj_fn bound[] = { LJ_FN_CUELIST_CONTROL, LJ_FN_TOGGLE_LIST_OF_CUELISTS, LJ_FN_QUICK_SAVE_CUELIST, LJ_FN_CUELIST_START_TOP, LJ_FN_CUELIST_GO,
            LJ_FN_CUELIST_BACK, LJ_FN_CUELIST_STOP, LJ_FN_CUELIST_STOP_AND_CLEAR, LJ_FN_CUELIST_GOTO_MARK_1, LJ_FN_CUELIST_RUN_FROM_MARK_4, LJ_FN_LOG_TIMECODE,
            LJ_FN_PROG_CUELIST, LJ_FN_GENERIC_CONTROLS, LJ_FN_GENMACRO_TOGGLE_EDITOR, LJ_FN_GENMACRO_TOGGLE_LIST, LJ_FN_CUE_MACRO_CONTROL_TOGGLE,
            LJ_FN_FANOUT_CONTROL, LJ_FN_PT_MACRO, LJ_FN_TOGGLE_BG_CUE_LIST, LJ_FN_FOLLOW_SPOT_1, LJ_FN_FOLLOW_SPOT_10, LJ_FN_CLEAR_FOLLOWSPOT };
        static const enum lj_desktop_action acts[] = { LJ_ACT_FOLLOWSPOT_SETTINGS, LJ_ACT_SAVE_CUELIST, LJ_ACT_GS2_EXPORT_ALL, LJ_ACT_GS2_RELOAD, LJ_ACT_GM_SHAPES_RELOAD,
            LJ_ACT_GENMACRO_APPLY, LJ_ACT_FIXTURE_APPLY, LJ_ACT_FOLLOWSPOT_APPLY, LJ_ACT_LOAD_GENMACRO, LJ_ACT_GENMACRO_TO_CUE, LJ_ACT_BEZ_RELOAD };
        for (i = 0; i < LJ_COUNT_OF(bound); i++) if (!lj_fn_bound(bound[i])) { fails++; printf("FAIL: lj_fn %d has no binding\n", (int)bound[i]); }
        logged[0] = 0;
        lj_log_set_sink(sink, NULL);
        CHECK(run_text("click main.M_CuelistControl\n"          /* View > Controls > Cuelist Control */
                       "click main.M_FollowSpot\n"              /* Setup > Preferences > Edit Followspot settings */
                       "click main.M_Generic\n"                 /* Generics */
                       "click main.M_DeleteCuelists\n"          /* System > Delete Files > Cuelists */
                       "click main.tb_cuelist_1\n" "click main.tb_cuelist_2\n" "click main.tb_cuelist_3\n" "click main.tb_cuelist_4\n"
                       "rclick main.tb_cuelist_3\n"             /* quick save */
                       "click main.tb_fixture_18\n" "rclick main.tb_fixture_18\n"
                       "quit\n") == 0);
        lj_desktop_run_action(LJ_ACT_CUSTOMIZE_FIXTURE, 0);    /* the icon popup's item (the popup needs a fixture under the mouse) */
        for (i = 0; i < LJ_COUNT_OF(acts); i++) lj_desktop_run_action(acts[i], acts[i] == LJ_ACT_FIXTURE_APPLY ? 0 : -1);
        lj_log_set_sink(NULL, NULL);
        CHECK(strstr(logged, "is not bound") == NULL && strstr(logged, "no binding for") == NULL);
        if (strstr(logged, "is not bound") || strstr(logged, "no binding for")) printf("%s", logged);
        lj_win_close_all();
    }

    /* ---- 4. the runner step counts its calls and publishes ctx->cl ---- */
    s0 = lj_app_cuelist_steps();
    for (i = 0; i < 3; i++) lj_app_cuelist_step(lj_clock_us());
    CHECK(lj_app_cuelist_steps() == s0 + 3 && c->cl && c->cl->state == LJ_CL_IDLE);

    /* ---- 5. a .Cl2 with an MP Play line: red, skipped; the cues around it latch through the app's host ---- */
    for (i = 7; i <= 8; i++) {
        q = lj_cue_new_rec(); q->number = (uint32_t)i;
        CHECK(lj_library_record_path(&lib, LJ_REC_CUE, (uint32_t)i, path, sizeof path) == 0 && lj_cue_save(path, q) == 0);
        lj_cue_free(q);
    }
    cl = lj_cuelist_new_rec(); cl->number = 1; strcpy(cl->name, "W3 test");
    memset(&ln, 0, sizeof ln); ln.manual = LJ_CL_AUTO; ln.cue = 7; CHECK(lj_cuelist_insert(cl, 1, &ln) == 0);
    memset(&ln, 0, sizeof ln); ln.manual = LJ_CL_AUTO; ln.cmd = LJ_CL_MP_PLAY; CHECK(lj_cuelist_insert(cl, 2, &ln) == 0);
    memset(&ln, 0, sizeof ln); ln.manual = LJ_CL_MANUAL; ln.cue = 8; CHECK(lj_cuelist_insert(cl, 3, &ln) == 0);
    CHECK(lj_library_record_path(&lib, LJ_REC_CL, 1, path, sizeof path) == 0 && lj_cuelist_save(path, cl) == 0);
    lj_cuelist_free(cl);
    CHECK(run_text("cl_load CueLists/00000001.Cl2\ncl_assert_line 2 error\ncl_assert_line 1 plain\nquit\n") == 0);
    CHECK(show->current_cl && show->current_cl->n == 3 && show->current_cl->lines[1].load_error == 1);
    CHECK(run_text("log \"x\"\ncl_assert_line 2 plain\n") == -1 && lj_uiscript_line() == 2);
    CHECK(run_text("cl_execute\nquit\n") == 0);              /* Go on an idle list = run from the top */
    lj_app_cuelist_step(lj_clock_us());
    CHECK(show->current_cue && show->current_cue->number == 7);
    CHECK(c->cl->state == LJ_CL_WAIT_GO && c->cl->next == 3);
    CHECK(run_text("cl_assert_line 3 next\ncl_assert_line 2 error\nquit\n") == 0);
    CHECK(run_text("cl_execute\nquit\n") == 0);
    lj_app_cuelist_step(lj_clock_us());
    CHECK(show->current_cue && show->current_cue->number == 8 && c->cl->state == LJ_CL_IDLE);
    tick(e, &t);
    CHECK(c->view.cue_number == 8);
    {   const lj_cuelist *before = show->current_cl;           /* a re-load unpins the runner from the old list before it is freed */
        lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(LJ_CL_OP_TOP, 0));
        lj_app_cuelist_step(lj_clock_us());
        CHECK(lj_app_load_cuelist(1) == 0 && show->current_cl != before && c->cl->state == LJ_CL_IDLE);
        lj_app_cuelist_step(lj_clock_us()); }

    /* ---- 6. assert_pos (16-bit pan / tilt of the MAC 500) and mm_set ---- */
    {   const lj_profile *p = lj_show_profile(show, 0);
        uint8_t ofs[4];
        lj_scene *sc = &show->current_seq->scenes[0];
        CHECK(p && p->pt16);
        if (p && p->pt16) {
            lj_pos_patch16(p, ofs);
            lj_lock();
            sc->val[ofs[0]] = 0xC0; sc->val[ofs[1]] = 0x00; sc->val[ofs[2]] = 0x40; sc->val[ofs[3]] = 0x00;
            for (i = 0; i < 4; i++) sc->state[ofs[i]] = LJ_SNAP;
            lj_unlock();
            lj_ctx_set_scene(0);
            tick(e, &t); tick(e, &t);
            CHECK(run_text("assert_pos 1 16384 -16384\nquit\n") == 0);
            CHECK(run_text("log \"x\"\nassert_pos 1 0 0\n") == -1 && lj_uiscript_line() == 2);
            CHECK(run_text("assert_pos 2 0 0\n") == -1);       /* a dimmer has no pan / tilt */
        }
        CHECK(run_text("mm_set 1 1 100 50\nquit\n") == 0);
        CHECK(sc->macro[0].type == 1 && sc->macro[0].amp_pan == 100 && sc->macro[0].amp_tilt == 100 && sc->macro[0].cycle_pan == 50 && sc->macro[0].fstate[0] == LJ_SNAP && c->modified[LJ_MOD_SEQ] == 1);
        lj_lock(); sc->macro[0].type = 0; lj_unlock(); }

    /* ---- 7. assert_prompt reads the open message box verbatim ---- */
    lj_win_message("Please Confirm..", "Delete the selected profile ?");
    CHECK(run_text("assert_prompt \"Delete the selected profile ?\"\nquit\n") == 0);
    CHECK(run_text("assert_prompt \"Delete the profile ?\"\n") == -1 && lj_uiscript_line() == 1);
    lj_warnings_close_all();
    CHECK(run_text("assert_prompt \"x\"\n") == -1);           /* none open */

    /* ---- 8. gs2_import: the importer's message; a missing file fails the line ---- */
    {   static uint8_t ten[10];
        FILE *f = fopen("lj_w3_bad.GS2", "wb");
        if (f) { fwrite(ten, 1, sizeof ten, f); fclose(f); }
        logged[0] = 0;
        lj_log_set_sink(sink, NULL);
        CHECK(run_text("gs2_import lj_w3_bad.GS2\n") == -1 && lj_uiscript_line() == 1);
        CHECK(run_text("gs2_import lj_w3_no_such_file.Udf\n") == -1);
        lj_log_set_sink(NULL, NULL);
        CHECK(lj_library_path(&lib, "UserFixtureDef/lj_w3_bad.GS2", path, sizeof path) == 0 && (f = fopen(path, "rb")) == NULL);   /* nothing written */
        if (f) fclose(f);
        {   const lj_profile *dim = lj_fixlib_by_id(19);       /* a .Udf written by our exporter from the built-in dimmer (no Martin bytes) imports and is listed */
            char err[160];
            CHECK(dim && lj_udf_export("lj_w3_synthetic.Udf", dim, err, sizeof err) == 0);
            CHECK(run_text("gs2_import lj_w3_synthetic.Udf\nquit\n") == 0);
            CHECK(lj_library_path(&lib, "UserFixtureDef/lj_w3_synthetic.GS2", path, sizeof path) == 0 && (f = fopen(path, "rb")) != NULL);
            if (f) fclose(f); } }

    /* ---- 8b. ME-31 remainder: Import (check modified) from Import\, Export All byte-equal, Reload after an on-disk edit ---- */
    {   static uint8_t a1[32768], b1[32768];
        char src[LJ_PATH_CAP], dst[LJ_PATH_CAP], err[160];
        size_t na = 0, nb = 0;
        int k, hit = 0;
        enum lj_ctl_kind kk;
        lj_profile p;
        FILE *f;
        if (lj_library_path(&lib, "UserFixtureDef/lj_w3_two.GS2", dst, sizeof dst) == 0) remove(dst);   /* the library outlives a run */
        CHECK(lj_library_path(&lib, "Import", dst, sizeof dst) == 0 && lj_fs_mkdir(dst) == 0);
        CHECK(lj_library_path(&lib, "UserFixtureDef/lj_w3_synthetic.GS2", src, sizeof src) == 0 && lj_library_path(&lib, "Import/lj_w3_two.GS2", dst, sizeof dst) == 0);
        if ((f = fopen(src, "rb")) != NULL) { na = fread(a1, 1, sizeof a1, f); fclose(f); }
        if ((f = fopen(dst, "wb")) != NULL) { fwrite(a1, 1, na, f); fclose(f); }
        CHECK(na > 0 && lj_gs2_import_folder(1) == 1 && show->ngs2 == 2);       /* lj_w3_two is new; nothing else in Import\ */
        CHECK(lj_gs2_import_folder(1) == 0);                                     /* check modified: the existing one is kept */
        CHECK(lj_gs2_export_all() == 2);
        for (k = 0; k < show->ngs2; k++) {
            char rel[96];
            snprintf(rel, sizeof rel, "UserFixtureDef/%s", show->gs2[k].gs2_file); lj_library_path(&lib, rel, src, sizeof src);
            snprintf(rel, sizeof rel, "Export/%s", show->gs2[k].gs2_file); lj_library_path(&lib, rel, dst, sizeof dst);
            na = nb = 0;
            if ((f = fopen(src, "rb")) != NULL) { na = fread(a1, 1, sizeof a1, f); fclose(f); }
            if ((f = fopen(dst, "rb")) != NULL) { nb = fread(b1, 1, sizeof b1, f); fclose(f); }
            CHECK(na > 0 && na == nb && memcmp(a1, b1, na) == 0);               /* Export All writes the source bytes back (raw-record rule) */
        }
        CHECK(lj_library_path(&lib, "UserFixtureDef/lj_w3_two.GS2", src, sizeof src) == 0);
        memset(&p, 0, sizeof p);
        CHECK(lj_gs2_load(src, &p, err, sizeof err) == 0);
        strcpy(p.name, "EDITED");
        CHECK(lj_gs2_save(src, &p) == 0);
        lj_gs2_free(&p);
        lj_desktop_run_action(LJ_ACT_GS2_RELOAD, 0);                             /* Reload profile data */
        for (k = 0; k < show->ngs2; k++) if (!strcmp(show->gs2[k].gs2_file, "lj_w3_two.GS2")) hit = !strcmp(show->gs2[k].name, "EDITED");
        CHECK(hit);
        /* ME-110: a GS2 fixture's controls come from its GS2 profile, and Reload profile data with Intensity and the
         * Generic Macro Editor open re-resolves both before the old array is freed (linux-gcc-asan: no use-after-free) */
        show->fixture[2].kind = LJ_FX_GS2; strcpy(show->fixture[2].gs2_file, "lj_w3_two.GS2"); show->fixture[2].link = 1; show->fixture[2].address = 200;
        lj_sel_none(); lj_sel_set(2, 1, 1);
        CHECK(lj_sel_master_profile() == lj_show_profile(show, 2) && lj_sel_master_profile() >= show->gs2 && lj_sel_master_profile() < show->gs2 + show->ngs2);
        kk = lj_sel_master_profile() && lj_sel_master_profile()->nwindows ? lj_sel_master_profile()->windows[0].kind : LJ_CTL_INTENSITY;   /* the dimmer-derived GS2: its Levels window */
        CHECK(lj_ctl_open(kk) != NULL);
        CHECK(lj_genmacro_editor_open() != NULL);
        lj_desktop_run_action(LJ_ACT_GS2_RELOAD, 0);
        CHECK(lj_ctl_window_get(kk) != NULL && lj_sel_master_profile() >= show->gs2 && lj_sel_master_profile() < show->gs2 + show->ngs2);
        lj_ctl_refresh(); lj_genmacro_editor_refresh();                          /* both read the profile they hold */
        lj_ctl_close(kk); lj_genmacro_editor_close();
        lj_sel_none(); memset(&show->fixture[2], 0, sizeof show->fixture[2]); }

    /* ---- 9. ME-102: a full command array; Clear Slot and a BG cue load keep what the engine still holds ---- */
    {   lj_scene sc2;
        lj_cmd cmd;
        lj_bgcue *bg;
        const void *held;
        int rc;
        CHECK(lj_statics_add_page(&show->pages, &show->npages, "P1") == 0);
        show->pages[0].n = 2;
        lj_scene_init(&sc2); sc2.val[99] = 77; sc2.state[99] = LJ_SNAP;
        lj_lock(); CHECK(lj_static_slot_record(&show->pages[0].slot[0], &sc2) == 0); show->pages[0].slot[0].level_pct = 100; lj_unlock();
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_STATIC_ACTIVATE; cmd.a = (uint32_t)lj_static_global_index(1, 1); cmd.b = 100; cmd.c = 0xFF; cmd.st = &show->pages[0].slot[0]; cmd.at_us = t;
        lj_ctx_post(&cmd);
        tick(e, &t);
        lj_lock(); CHECK(lj_engine_frame(e)->merged[99] == 77); lj_unlock();
        bg = lj_bgcue_new_rec(); bg->number = 4;
        CHECK(lj_library_record_path(&lib, LJ_REC_BG, 4, path, sizeof path) == 0 && lj_bgcue_save(path, bg) == 0);
        lj_bgcue_free(bg);
        CHECK(lj_app_load_bgcue(4) == 0);
        tick(e, &t);
        held = lj_table_get(&show->bgcues, 4);
        memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_PREFS; cmd.a = LJ_PREF_KEEP_SEQ_BLIND; cmd.at_us = t;   /* harmless filler */
        lj_log_set_sink(sink, NULL);
        lj_lock(); for (i = 0; i < LJ_ENGINE_CMD_CAP + 4; i++) lj_engine_cmd(e, &cmd); lj_unlock();   /* the array is full now */
        logged[0] = 0;
        rc = lj_app_static_clear(1, 1);
        CHECK(rc == -1 && show->pages[0].slot[0].kind == LJ_STATIC_RECORDED);
        CHECK(lj_app_load_bgcue(4) == 0);                        /* the table gets the new record; the engine's is kept, not freed */
        lj_log_set_sink(NULL, NULL);
        CHECK(strstr(logged, "ME-102") != NULL);
        tick(e, &t);                                             /* drains the array; the engine still reads its pinned slot and BG cue */
        lj_lock(); CHECK(lj_engine_frame(e)->merged[99] == 77); lj_unlock();
        CHECK(lj_table_get(&show->bgcues, 4) != held);
        CHECK(lj_app_load_bgcue(4) == 0);                        /* a successful post hands the engine the newest record ... */
        tick(e, &t);
        lj_bgcue_free((lj_bgcue *)held);                         /* ... so the record ME-102 kept is ours to free now (LSan) */
        CHECK(lj_app_static_clear(1, 1) == 0 && show->pages[0].slot[0].kind == LJ_STATIC_EMPTY);   /* room again: cleared */
        tick(e, &t);
        lj_lock(); CHECK(lj_engine_frame(e)->merged[99] != 77); lj_unlock();
    }

    /* ---- 9a. ME-104 / ME-112: sbLoad opens Select Background Cue latched; a double-click loads a BG cue; "(T)"; the Toggle
     * BG Cue List hotkey; the slot popup's Sequence Mode.. writes the slot mode ---- */
    {   lj_window *lw, *bw, *mw = NULL;
        lj_widget *lv;
        lj_bgcue *bg = lj_bgcue_new_rec();
        int k, row = -1;
        bg->number = 7; bg->transparent = 1; bg->slot[0].seq = 1; bg->slot[0].enabled = 1; bg->slot[0].mode = LJ_BG_AUTO;
        CHECK(lj_library_record_path(&lib, LJ_REC_BG, 7, path, sizeof path) == 0 && lj_bgcue_save(path, bg) == 0);
        lj_bgcue_free(bg);
        logged[0] = 0;
        lj_log_set_sink(sink, NULL);
        CHECK(run_text("open bgcue_form\nclick TBGCueForm.sbLoad\nquit\n") == 0);
        lw = lj_win_get(LJ_WIN_SELECT_BGCUE); bw = lj_win_get(LJ_WIN_BGCUE_FORM);
        CHECK(lw != NULL && bw != NULL && lj_w_speedbutton_down(lj_window_find(bw, "sbLoad")) == 1);
        lv = lj_window_find(lw, "lv1");
        for (k = 0; lv && k < ((lj_w_listview *)lv)->n; k++) if (strcmp(((lj_w_listview *)lv)->items[k], "(T)07\t") == 0) row = k;
        CHECK(row >= 0);                                         /* transparent: "(T)" before the number; no name */
        if (row >= 0) { ((lj_w_listview *)lv)->index = row; lj_w_emit(lv, LJ_WEV_DBLCLICK); }
        tick(e, &t); tick(e, &t);
        lj_win_refresh_all();
        CHECK(c->view.bg_number == 7 && lj_bgcue_form_work()->slot[0].seq == 1);
        CHECK(run_text("click TBGCueForm.sbLoad\nquit\n") == 0);
        CHECK(lj_win_get(LJ_WIN_SELECT_BGCUE) == NULL && lj_w_speedbutton_down(lj_window_find(bw, "sbLoad")) == 0);
        lj_fn_run(LJ_FN_TOGGLE_BG_CUE_LIST, 1); CHECK(lj_win_get(LJ_WIN_SELECT_BGCUE) != NULL);
        lj_fn_run(LJ_FN_TOGGLE_BG_CUE_LIST, 1); CHECK(lj_win_get(LJ_WIN_SELECT_BGCUE) == NULL);
        lj_w_listview_select(lj_window_find(bw, "LV1"), 0);
        CHECK(fire_menu(bw, "P_SequenceMode") == 0 && (mw = lj_bgcue_seqmode_get()) != NULL);
        if (mw) {
            CHECK(strncmp(lj_w_edit_text(lj_window_find(mw, "eSeqName")), "0001 ", 5) == 0);   /* "Seq 0001 <name>" (bgcue-sequencemode.gif) */
            lj_w_checkbox_click(lj_window_find(mw, "cb1Shot"));
            lj_w_click(lj_window_find(mw, "bOk"));
            CHECK(lj_bgcue_seqmode_get() == NULL && lj_bgcue_form_work()->slot[0].mode == LJ_BG_NOLOOP);
        }
        lj_log_set_sink(NULL, NULL);
        CHECK(strstr(logged, "has no behaviour file yet") == NULL);
        lj_win_close(LJ_WIN_BGCUE_FORM); }

    /* ---- 9b. ME-117 / ME-121: Delete Generic Macros stops only the running macro; delete, re-save, load, tick (ASan) ---- */
    {   lj_genmacro *gm = lj_genmacro_new_rec();
        uint32_t k;
        for (k = 3; gm && k <= 7; k += 4) {
            gm->number = k; gm->ch[0].shape = 1; gm->ch[0].amp_pos = 40; gm->ch[0].cycle_ds = 10;
            CHECK(lj_library_record_path(&lib, LJ_REC_MACRO, k, path, sizeof path) == 0 && lj_genmacro_save(path, gm) == 0);
        }
        free(gm);
        CHECK(lj_app_load_genmacro(7) == 0 && lj_app_load_genmacro(3) == 0);   /* 7 stays in show->macros, 3 runs */
        tick(e, &t); tick(e, &t);
        CHECK(c->view.gm_cue_number == 3 && lj_table_get(&show->macros, 7) != NULL);
        CHECK(lj_app_delete_record(LJ_REC_MACRO, 7) == 0);
        tick(e, &t); tick(e, &t);
        CHECK(c->view.gm_cue_number == 3 && lj_table_get(&show->macros, 7) == NULL);   /* ME-117: macro 3 keeps running */
        CHECK(lj_app_delete_record(LJ_REC_MACRO, 3) == 0);
        tick(e, &t); tick(e, &t);
        CHECK(c->view.gm_cue_number == 0);                       /* the running one stops; freed after the engine let go */
        gm = lj_genmacro_new_rec();                              /* re-save 3 and load it again */
        if (gm) { gm->number = 3; gm->ch[0].shape = 2; gm->ch[0].amp_pos = 20; gm->ch[0].cycle_ds = 5;
                  CHECK(lj_library_record_path(&lib, LJ_REC_MACRO, 3, path, sizeof path) == 0 && lj_genmacro_save(path, gm) == 0); free(gm); }
        CHECK(lj_app_load_genmacro(3) == 0);
        tick(e, &t); tick(e, &t);
        CHECK(c->view.gm_cue_number == 3);
        CHECK(lj_app_delete_record(LJ_REC_MACRO, 3) == 0);
        tick(e, &t); tick(e, &t);
        CHECK(c->view.gm_cue_number == 0); }

    /* ---- teardown ---- */
    lj_win_close_all();
    lj_app_seam_stop();
    lj_fn_unbind_all();
    lj_lock(); lj_engine_bind(e, NULL); c->engine = NULL; lj_unlock();
    lj_engine_destroy(e);
    lj_app_w3_stop();
    lj_desktop_destroy(desk);
    lj_lock(); lj_show_free(show); c->show = NULL; lj_unlock();
    lj_ini_free(c->main_ini); c->main_ini = NULL;
    lj_uiscript_set_host(NULL);
    printf("test_uiscript_w3: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
