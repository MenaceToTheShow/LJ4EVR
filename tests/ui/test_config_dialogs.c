/* test_config_dialogs.c - `lj4evr_tests test_config_dialogs` (W4 dlg-config), headless:
 *  - each of the 25 CONFIG ids opens from its entry point (Fixture Configuration buttons and popup items, the icon popup
 *    actions, Define Fixture Profile, the profile editor's buttons, the profile import / export, lj_win_open for the three
 *    whose entry point no source names) with ME-16 clean: no "lj_window:" binding line and no "has no behaviour file";
 *  - F43 rows: Calculate addresses ignores a global patch, honours the link boxes and leaves Generic Scanner 1 its 8;
 *  - F41: the save-time warning list in LJ2's wording, TConfigWarnForm "&Save  Anyway" (script assert_text) and Cancel;
 *  - the Fixture Configuration prompts of [12 Q9] verbatim (script assert_prompt rows);
 *  - F44: Copy From Fixture copies sequence / preset / generic macro / default DMX data from A onto B through the filter,
 *    Clear Fixture Data empties B and leaves A;
 *  - goldens W4C_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile_editor.h"
#include "gfx/lj_bmp.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_presets.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_config_dialogs.h"
#include "windows/lj_fixture_config.h"
#include "windows/lj_gs2_dialogs.h"
#include "windows/lj_profile_editor_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_config_dialogs_tmp/"

static int fails, complaints;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void sink(void *ud, const char *line) { (void)ud; if (getenv("LJ_TEST_VERBOSE")) printf("  log: %s\n", line); if (strstr(line, "lj_window:") || strstr(line, "has no behaviour file")) { complaints++; printf("  log: %s\n", line); } }
static int act_count[LJ_ACT_COUNT];
static void on_action(enum lj_desktop_action a, int arg, void *ud)
{
    (void)ud;
    act_count[a]++;
    if (a == LJ_ACT_FIXTURE_INFO) { lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_INFO); }   /* what app/lj_app_cuelist_host.c does */
    if (a == LJ_ACT_FIXTURE_NOTES) { lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_NOTES); }
}
static lj_widget *in(enum lj_win id, const char *n) { lj_window *w = lj_win_get(id); return w ? lj_window_find(w, n) : NULL; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static void pick(lj_widget *lv, int row) { if (!lv) { fails++; printf("FAIL list missing\n"); return; } if (lv->kind == LJ_K_LISTBOX) ((lj_w_list *)lv)->index = row; else ((lj_w_listview *)lv)->index = row; lj_w_emit(lv, LJ_WEV_CHANGE); }
static int lv_n(lj_widget *lv) { return !lv ? -1 : lv->kind == LJ_K_LISTBOX ? ((lj_w_list *)lv)->n : ((lj_w_listview *)lv)->n; }
static const char *lv_row(lj_widget *lv, int r) { return lv && r < lv_n(lv) ? (lv->kind == LJ_K_LISTBOX ? ((lj_w_list *)lv)->items[r] : ((lj_w_listview *)lv)->items[r]) : ""; }
static const char *msg_line(const char *label) { lj_window *m = lj_win_message_window(); const char *c = m ? lj_w_caption(lj_window_find(m, label)) : NULL; return c ? c : ""; }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); lj_widget *b = m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL; if (b) lj_w_click(b); else { fails++; printf("FAIL no prompt to answer\n"); } }
static int fire_menu(lj_window *win, const char *item)
{
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, item) : NULL;
    if (!rt || !rt->fn || !rt->enabled) return -1;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}
static int run_text(const char *text)
{
    FILE *f = fopen(TMP "script.lj4s", "wb");
    uint64_t t0;
    int r;
    if (!f) return -1;
    fputs(text, f); fclose(f);
    if ((r = lj_uiscript_load(TMP "script.lj4s")) != 0) return r;
    t0 = lj_clock_us();
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 10000000u) { }
    return r;
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
static void golden_win(lj_window *w, const char *name)
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
static int run_text(const char *text);
static void golden(enum lj_win id, const char *name)
{
    const char *dump = getenv("LJ_CAPTION_DUMP");             /* W4-CAP: the captions of each window as it is captured (tools/caption_diff.py --dump) */
    char line[600];
    golden_win(lj_win_get(id), name);
    if (dump && dump[0] && snprintf(line, sizeof line, "dump_captions %s\n", dump) < (int)sizeof line) run_text(line);
}
static int opened(enum lj_win id) { return lj_win_get(id) != NULL; }

static lj_show *sh;
static int base(int f) { return lj_show_fixture_base(sh, f); }

/* ---- Fixture Configuration: Add Fixture(s), Find profile, Calculate addresses (F43), Warnings (F41), the Q9 prompts ---- */
static void test_fixture_config(void)
{
    lj_window *fc = lj_win_open(LJ_WIN_FIXTURE_CONFIG);
    int l = 0, a = 0, fp;
    CHECK(fc != NULL);
    if (!fc) return;
    golden_win(fc, "W4C_FIXTURE_CONFIG");                       /* the Martin locate button captioned "Martin" (F42, D2) */
    CHECK(strcmp(lj_w_caption(lj_window_find(fc, "sbMartin")), "Martin") == 0);
    /* Add Fixture(s): two MAC 500 Mode 2/4 at the first unused positions */
    CHECK(lj_fixture_config_tree_select(12) == 0);
    click(lj_window_find(fc, "bAdd"));
    CHECK(opened(LJ_WIN_ADD_FIXTURES));
    CHECK(lj_win_get(LJ_WIN_ADD_FIXTURES) && strcmp(lj_window_caption(lj_win_get(LJ_WIN_ADD_FIXTURES))->text, "Add Fixture (s)") == 0);
    CHECK(strstr(lj_w_caption(in(LJ_WIN_ADD_FIXTURES, "lFixtureName")), "MAC 500") != NULL);
    CHECK(strcmp(lj_w_edit_text(in(LJ_WIN_ADD_FIXTURES, "eStart")), "1") == 0);
    lj_w_edit_set_text(in(LJ_WIN_ADD_FIXTURES, "ENum"), "2");
    golden(LJ_WIN_ADD_FIXTURES, "W4C_ADD_FIXTURES");
    click(in(LJ_WIN_ADD_FIXTURES, "bOk"));
    CHECK(!opened(LJ_WIN_ADD_FIXTURES) && sh->fixture[0].profile_id == 12 && sh->fixture[1].profile_id == 12 && sh->fixture[0].link == 1 && !sh->fixture[0].address);
    /* Find profile.. */
    click(lj_window_find(fc, "sbFind"));
    CHECK(opened(LJ_WIN_FIND_PROFILE));
    lj_w_edit_set_text(in(LJ_WIN_FIND_PROFILE, "eFind"), "mac 5");
    click(in(LJ_WIN_FIND_PROFILE, "bFind"));
    CHECK(lv_n(in(LJ_WIN_FIND_PROFILE, "lvFound")) > 0 && strncmp(lv_row(in(LJ_WIN_FIND_PROFILE, "lvFound"), 0), "Martin\tMartin MAC 5", 19) == 0);
    golden(LJ_WIN_FIND_PROFILE, "W4C_FIND_PROFILE");
    lj_w_listview_select(in(LJ_WIN_FIND_PROFILE, "lvFound"), 0);
    lj_w_emit(in(LJ_WIN_FIND_PROFILE, "lvFound"), LJ_WEV_SELECT_ITEM);
    CHECK(((lj_w_treeview *)lj_window_find(fc, "tvAvailable"))->index >= 0 && lj_fixture_config_tree_id(((lj_w_treeview *)lj_window_find(fc, "tvAvailable"))->index) >= 0);
    lj_win_close(LJ_WIN_FIND_PROFILE);
    /* Calculate addresses: fixture 1 -> 1:001; F43: a global patch on the next free address does not block it */
    fp = lj_fixlib_by_id(12)->footprint;
    lj_fixture_config_select(0);
    click(lj_window_find(fc, "bAutoAddress"));
    CHECK(opened(LJ_WIN_AUTO_ADDRESS));
    click(in(LJ_WIN_AUTO_ADDRESS, "bFind"));
    CHECK(lv_n(in(LJ_WIN_AUTO_ADDRESS, "LV1")) == 1 && strncmp(lv_row(in(LJ_WIN_AUTO_ADDRESS, "LV1"), 0), "01\tMartin MAC 500", 17) == 0 && strstr(lv_row(in(LJ_WIN_AUTO_ADDRESS, "LV1"), 0), "\t1:001-") != NULL);
    golden(LJ_WIN_AUTO_ADDRESS, "W4C_AUTO_ADDRESS");
    click(in(LJ_WIN_AUTO_ADDRESS, "bOk"));
    CHECK(!opened(LJ_WIN_AUTO_ADDRESS) && sh->fixture[0].link == 1 && sh->fixture[0].address == 1);
    sh->patch[lj_addr_code(1, fp + 1)].src_kind = LJ_SRC_DMX_OUT;   /* a global patch on the address the next fixture gets */
    CHECK(lj_config_dlg_auto_address(1, 0, &l, &a) == 0 && l == 1 && a == fp + 1);
    CHECK(lj_config_dlg_auto_address(1, 1u << 1, &l, &a) == 0 && l == 2 && a == 1);   /* Link 2 only */
    sh->patch[lj_addr_code(1, fp + 1)].src_kind = LJ_SRC_NONE;
    CHECK(lj_fixture_config_put(2, 43) == 0 && lj_fixture_config_set_address(2, 3, 1) == 0);   /* Generic Scanner 1 at 3:001 takes 8 */
    CHECK(lj_fixture_config_put(3, 19) == 0);                 /* a one-channel dimmer */
    CHECK(lj_config_dlg_auto_address(3, 1u << 2, &l, &a) == 0 && l == 3 && a == 9);
    /* F41: the warning list */
    lj_fixture_config_set_address(1, 1, 1);                    /* overlaps fixture 1 */
    {
        const char *rows[64];
        int n = lj_config_dlg_warnings(rows, 64);
        CHECK(n == 1 + fp && strcmp(rows[0], "Fixture 4 has no DMX Address") == 0 && strcmp(rows[1], "DMX address 1:001 is used by more than one fixture") == 0);
    }
    lj_fixture_config_set_address(3, 1, 512);                  /* footprint 1 at 512 is valid; make it invalid with the MAC */
    lj_fixture_config_set_address(1, 1, 510);
    {
        const char *rows[8];
        CHECK(lj_config_dlg_warnings(rows, 8) == 1 && strcmp(rows[0], "Fixture 2 has invalid DMX Address") == 0);
    }
    click(lj_window_find(fc, "bSave"));
    CHECK(opened(LJ_WIN_CONFIG_WARN) && lv_n(in(LJ_WIN_CONFIG_WARN, "lbWarnings")) == 1);
    golden(LJ_WIN_CONFIG_WARN, "W4C_CONFIG_WARN");
    CHECK(run_text("assert_caption TConfigWarnForm \"Warnings\"\nassert_text TConfigWarnForm.bSave \"&Save  Anyway\"\nassert_text TConfigWarnForm.bCancel \"&Cancel\"\n") == 0);
    click(in(LJ_WIN_CONFIG_WARN, "bCancel"));
    CHECK(!opened(LJ_WIN_CONFIG_WARN) && !lj_win_message_window());
    lj_fixture_config_set_address(1, 1, fp + 1);
    lj_fixture_config_set_address(3, 1, 2 * fp + 1);
    /* Save: "Clear data for new fixtures ?" (Yes), default DMX applied (AlwaysApplyDefault = 1), Config.dat */
    CHECK(lj_config_dlg_warnings(NULL, 0) == 0);
    CHECK(run_text("click TLinkConfigForm3.bSave\nassert_prompt \"Clear data for new fixtures ?\"\nclick confirm.BOk\n") == 0);
    CHECK(opened(LJ_WIN_APPLY_DEFAULT_DMX) && act_count[LJ_ACT_DEFAULT_OUT_APPLY] >= 1);
    golden(LJ_WIN_APPLY_DEFAULT_DMX, "W4C_APPLY_DEFAULT_DMX");
    lj_win_refresh(LJ_WIN_APPLY_DEFAULT_DMX);
    CHECK(!opened(LJ_WIN_APPLY_DEFAULT_DMX));
    { char p[LJ_PATH_CAP]; CHECK(lj_library_path(lj_ctx_get()->lib, "Config.dat", p, sizeof p) == 0 && lj_fs_exists(p)); }
    CHECK(lj_fixture_config_saved(0) && lj_fixture_config_saved(1));
    /* "Apply Default DMX values to new/modified fixtures ?" with Always apply off */
    lj_ini_seti(lj_ctx_get()->forms_ini, "FIXTURECONF", "AlwaysApplyDefault", 0);
    lj_fixture_config_put(4, 19); lj_fixture_config_set_address(4, 2, 100);
    CHECK(run_text("click TLinkConfigForm3.bSave\nassert_prompt \"Clear data for new fixtures ?\"\nclick confirm.BCancel\nassert_prompt \"Apply Default DMX values to new/modified fixtures ?\"\nclick confirm.BCancel\n") == 0);
    CHECK(!opened(LJ_WIN_APPLY_DEFAULT_DMX) && lj_fixture_config_saved(4));
    lj_ini_seti(lj_ctx_get()->forms_ini, "FIXTURECONF", "AlwaysApplyDefault", 1);
    /* "Do you wish to change the fixtures DMX address ?" on a saved fixture; No puts the address back */
    lj_fixture_config_select(4);
    lj_w_edit_set_text(lj_window_find(fc, "eAdr"), "120");
    lj_w_emit(lj_window_find(fc, "eAdr"), LJ_WEV_EXIT);
    CHECK(strcmp(msg_line("Label2"), "Do you wish to change the fixtures DMX address ?") == 0);
    CHECK(run_text("assert_prompt \"Do you wish to change the fixtures DMX address ?\"\nclick confirm.BCancel\n") == 0);
    CHECK(sh->fixture[4].address == 100);
    /* "Clear Selected Fixtures ?" (the settings panel's Clear) */
    sh->fixture[4].invert_pan = 1;
    CHECK(run_text("click TLinkConfigForm3.bClear\nassert_prompt \"Clear Selected Fixtures ?\"\nclick confirm.BOk\n") == 0);
    CHECK(sh->fixture[4].invert_pan == 0);
    sh->fixture[4].invert_pan = 0;
    lj_fixture_config_save();
    CHECK(!lj_win_message_window() && lj_fixture_config_saved(4));
}

/* ---- F44: Copy From Fixture / Copy Fixture / Clear Fixture Data ---- */
static void test_copy_clear(void)
{
    lj_window *fc = lj_win_get(LJ_WIN_FIXTURE_CONFIG);
    lj_library *lib = lj_ctx_get()->lib;
    lj_sequence *s = lj_sequence_new_rec();
    lj_genmacro *m = lj_genmacro_new_rec();
    char sp[LJ_PATH_CAP], mp[LJ_PATH_CAP], err[160];
    int b0 = base(0), b1 = base(1);
    CHECK(fc && b0 >= 0 && b1 >= 0);
    if (!fc || b0 < 0 || b1 < 0) { lj_sequence_free(s); free(m); return; }
    s->number = 1; strcpy(s->name, "copy");
    s->scenes[0].val[b0] = 77; s->scenes[0].state[b0] = LJ_SNAP; s->scenes[0].preset[0] = 5; s->scenes[0].macro[0].type = 3;
    m->number = 1; strcpy(m->name, "gm"); m->ch[b0].shape = 4; m->ch[b0].amp_pos = 10;
    CHECK(lj_library_record_path(lib, LJ_REC_SEQ, 1, sp, sizeof sp) == 0 && lj_sequence_save(sp, s) == 0);
    CHECK(lj_library_record_path(lib, LJ_REC_MACRO, 1, mp, sizeof mp) == 0 && lj_genmacro_save(mp, m) == 0);
    sh->preset[0][0].used = 1; sh->preset[0][0].pan = 1234;
    sh->default_out[b0] = 55;
    sh->fixture[0].invert_tilt = 1;
    lj_fixture_config_select(1);
    CHECK(fire_menu(fc, "P_Clone") == 0 && opened(LJ_WIN_COPY_FROM));
    CHECK(lv_n(in(LJ_WIN_COPY_FROM, "LV1")) == 1 && strncmp(lv_row(in(LJ_WIN_COPY_FROM, "LV1"), 0), "1\t", 2) == 0);
    CHECK(in(LJ_WIN_COPY_FROM, "bOk") && !in(LJ_WIN_COPY_FROM, "bOk")->enabled);
    golden(LJ_WIN_COPY_FROM, "W4C_COPY_FROM");
    pick(in(LJ_WIN_COPY_FROM, "LV1"), 0);
    click(in(LJ_WIN_COPY_FROM, "bOk"));
    CHECK(!opened(LJ_WIN_COPY_FROM) && opened(LJ_WIN_CLONE));
    CHECK(strncmp(lj_w_caption(in(LJ_WIN_CLONE, "lSource")), "Copy From #1 ", 13) == 0);
    golden(LJ_WIN_CLONE, "W4C_CLONE");
    CHECK(run_text("click TCloneForm.bOk\nassert_prompt \"Copy fixture, are you sure?\"\nclick confirm.BOk\n") == 0);
    CHECK(!opened(LJ_WIN_CLONE));
    CHECK(lj_sequence_load(sp, s, err, sizeof err) == 0 && s->scenes[0].val[b1] == 77 && s->scenes[0].state[b1] == LJ_SNAP && s->scenes[0].preset[1] == 5 && s->scenes[0].macro[1].type == 3);
    CHECK(lj_genmacro_load(mp, m, err, sizeof err) == 0 && m->ch[b1].shape == 4 && m->ch[b1].amp_pos == 10);
    CHECK(sh->preset[1][0].used == 1 && sh->preset[1][0].pan == 1234 && sh->default_out[b1] == 55 && sh->fixture[1].invert_tilt == 1);
    CHECK(act_count[LJ_ACT_FIXTURE_APPLY] >= 1);
    CHECK(sh->fixture[1].address != 0 && base(1) == b1);       /* "except DMX address" */
    /* Clear Fixture Data on B */
    CHECK(fire_menu(fc, "P_Clear") == 0 && opened(LJ_WIN_CLEANUP));
    CHECK(strcmp(lj_w_caption(in(LJ_WIN_CLEANUP, "lAction")), "Clearing Fixture #2") == 0);
    golden(LJ_WIN_CLEANUP, "W4C_CLEANUP");
    CHECK(run_text("click TCleanUpForm.bOk\nassert_prompt \"Clear selected fixtures, are you sure?\"\nclick confirm.BOk\n") == 0);
    CHECK(!opened(LJ_WIN_CLEANUP));
    CHECK(lj_sequence_load(sp, s, err, sizeof err) == 0 && s->scenes[0].val[b1] == 0 && s->scenes[0].state[b1] == LJ_OFF && s->scenes[0].preset[1] == 0 && s->scenes[0].macro[1].type == 0);
    CHECK(s->scenes[0].val[b0] == 77 && s->scenes[0].preset[0] == 5);   /* A untouched */
    CHECK(lj_genmacro_load(mp, m, err, sizeof err) == 0 && m->ch[b1].shape == 0 && m->ch[b0].shape == 4);
    CHECK(!sh->preset[1][0].used && sh->default_out[b1] == 0 && sh->preset[0][0].used && sh->default_out[b0] == 55);
    /* a fixture of another type is not offered as a source, and an unsaved row has no Copy From */
    lj_fixture_config_select(2);                               /* Generic Scanner 1: no other of its type */
    CHECK(fire_menu(fc, "P_Clone") == 0 && lv_n(in(LJ_WIN_COPY_FROM, "LV1")) == 0);
    lj_win_close(LJ_WIN_COPY_FROM);
    lj_sequence_free(s); free(m);
}

/* ---- icon popup windows, Patch Sub Masters, Special, hidden, the definitions ---- */
static void test_popup_windows(void)
{
    lj_desktop_run_action(LJ_ACT_FIXTURE_INFO, 0);
    CHECK(opened(LJ_WIN_FIXTURE_INFO));
    CHECK(strncmp(lj_w_caption(in(LJ_WIN_FIXTURE_INFO, "LFixtureType")), "Martin MAC 500", 14) == 0 && strcmp(lj_w_caption(in(LJ_WIN_FIXTURE_INFO, "LDmxAdr")), "1") == 0
          && strcmp(lj_w_caption(in(LJ_WIN_FIXTURE_INFO, "LLink")), "1") == 0 && strcmp(lj_w_caption(in(LJ_WIN_FIXTURE_INFO, "lIntG")), "Master") == 0);
    golden(LJ_WIN_FIXTURE_INFO, "W4C_FIXTURE_INFO");
    lj_win_close(LJ_WIN_FIXTURE_INFO);
    lj_desktop_run_action(LJ_ACT_FIXTURE_NOTES, 0);
    CHECK(opened(LJ_WIN_FIXTURE_NOTES) && ((lj_w_memo *)in(LJ_WIN_FIXTURE_NOTES, "Re1"))->n > 0);
    golden(LJ_WIN_FIXTURE_NOTES, "W4C_FIXTURE_NOTES");
    lj_win_close(LJ_WIN_FIXTURE_NOTES);
    lj_config_dlg_target(0);
    CHECK(lj_win_open(LJ_WIN_FIXTURE_SPECIAL) != NULL);
    golden(LJ_WIN_FIXTURE_SPECIAL, "W4C_FIXTURE_SPECIAL");
    lj_win_close(LJ_WIN_FIXTURE_SPECIAL);
    CHECK(lj_win_open(LJ_WIN_HIDDEN_FIXTURE) != NULL);
    golden(LJ_WIN_HIDDEN_FIXTURE, "W4C_HIDDEN_FIXTURE");
    lj_win_close(LJ_WIN_HIDDEN_FIXTURE);
    CHECK(lj_win_open(LJ_WIN_INTENSITY_PATCH) != NULL);
    golden(LJ_WIN_INTENSITY_PATCH, "W4C_INTENSITY_PATCH");
    click(in(LJ_WIN_INTENSITY_PATCH, "bkOk"));
    CHECK(!opened(LJ_WIN_INTENSITY_PATCH));
    /* Define Fixture Profile: Generic Scanner 1, Color Scroller 1, RGB pack */
    lj_sel_none(); lj_sel_set(2, 1, 1);
    CHECK(strcmp(lj_customize_popup_caption(2), "Define Fixture Profile") == 0);
    { lj_window *w = lj_win_open(LJ_WIN_CUSTOMIZE_FIXTURE); CHECK(w != NULL && w == lj_win_get(LJ_WIN_SCANNER1_DEFS)); }
    CHECK(in(LJ_WIN_SCANNER1_DEFS, "cbShutter") && !in(LJ_WIN_SCANNER1_DEFS, "cbShutter")->enabled && in(LJ_WIN_SCANNER1_DEFS, "bOk")->enabled);
    golden(LJ_WIN_SCANNER1_DEFS, "W4C_SCANNER1_DEFS");
    click(in(LJ_WIN_SCANNER1_DEFS, "bCancel"));
    CHECK(!opened(LJ_WIN_SCANNER1_DEFS));
    lj_fixture_config_put(5, 42); lj_fixture_config_set_address(5, 4, 1);
    lj_fixture_config_put(6, 75); lj_fixture_config_set_address(6, 4, 10);
    lj_sel_none(); lj_sel_set(5, 1, 1);
    { lj_window *w = lj_win_open(LJ_WIN_CUSTOMIZE_FIXTURE); CHECK(w != NULL && w == lj_win_get(LJ_WIN_COLOR_SCROLLER_DEFS)); }
    golden(LJ_WIN_COLOR_SCROLLER_DEFS, "W4C_COLOR_SCROLLER_DEFS");
    click(in(LJ_WIN_COLOR_SCROLLER_DEFS, "bOk"));
    CHECK(!opened(LJ_WIN_COLOR_SCROLLER_DEFS));
    lj_sel_none(); lj_sel_set(6, 1, 1);
    { lj_window *w = lj_win_open(LJ_WIN_CUSTOMIZE_FIXTURE); CHECK(w != NULL && w == lj_win_get(LJ_WIN_RGBPACK_DEFS)); }
    golden(LJ_WIN_RGBPACK_DEFS, "W4C_RGBPACK_DEFS");
    ((lj_w_grid *)in(LJ_WIN_RGBPACK_DEFS, "sg1"))->row = 1;
    lj_w_emit(in(LJ_WIN_RGBPACK_DEFS, "sg1"), LJ_WEV_SELECT_CELL);
    lj_w_edit_set_text(in(LJ_WIN_RGBPACK_DEFS, "Edit1"), "Left");
    click(in(LJ_WIN_RGBPACK_DEFS, "BitBtn1"));
    CHECK(!opened(LJ_WIN_RGBPACK_DEFS) && strcmp(sh->fixture[6].ch_caption[0], "Left") == 0);
    lj_fixture_config_save();
    if (lj_win_message_window()) msg_answer(0);
    if (lj_win_message_window()) msg_answer(0);
    lj_win_close(LJ_WIN_APPLY_DEFAULT_DMX);
}

/* ---- the profile editor's pickers, Apply user profile, Oldstyle profile name (ME-126) ---- */
static lj_profile legacy;
static uint8_t legacy_raw[LJ_GS2_REC_SIZE];
static void test_profile_pickers(void)
{
    lj_profile_edit *ed;
    char err[160];
    int k;
    lj_fixture_config_put(7, 46); lj_fixture_config_set_address(7, 2, 200);
    lj_fixture_config_put(8, 46); lj_fixture_config_set_address(8, 2, 240);
    lj_sel_none(); lj_sel_set(8, 1, 0); lj_sel_set(7, 1, 1);
    { lj_window *w = lj_win_open(LJ_WIN_CUSTOMIZE_FIXTURE); CHECK(w != NULL && w == lj_win_get(LJ_WIN_PROFILE_EDITOR)); }
    ed = lj_profile_editor_work();
    CHECK(ed != NULL && !opened(LJ_WIN_PROFILE_NAME_OLDSTYLE));
    if (!ed) return;
    /* Palette Functions on the gobo page */
    click(in(LJ_WIN_PROFILE_EDITOR, "bSelectValTypeGobo"));
    CHECK(opened(LJ_WIN_PALETTE_VALUE_TYPE) && strcmp(lj_window_caption(lj_win_get(LJ_WIN_PALETTE_VALUE_TYPE))->text, "Select Gobo 1 Palette Types") == 0);
    CHECK(lv_n(in(LJ_WIN_PALETTE_VALUE_TYPE, "lvSelection")) == 5 && lv_n(in(LJ_WIN_PALETTE_VALUE_TYPE, "lvTypes")) == 19);
    golden(LJ_WIN_PALETTE_VALUE_TYPE, "W4C_PALETTE_VALUE_TYPE");
    ((lj_w_listview *)in(LJ_WIN_PALETTE_VALUE_TYPE, "lvSelection"))->index = 1; lj_w_emit(in(LJ_WIN_PALETTE_VALUE_TYPE, "lvSelection"), LJ_WEV_CLICK);
    pick(in(LJ_WIN_PALETTE_VALUE_TYPE, "lvTypes"), 10);
    CHECK(in(LJ_WIN_PALETTE_VALUE_TYPE, "bSet")->enabled);
    click(in(LJ_WIN_PALETTE_VALUE_TYPE, "bSet"));
    click(in(LJ_WIN_PALETTE_VALUE_TYPE, "bOk"));
    CHECK(!opened(LJ_WIN_PALETTE_VALUE_TYPE) && ed->raw[0x37DB + 1] == 10);   /* Shake in slot 2 [16 s5] */
    /* Bitmaps on a gobo entry: Select Bitmap over our tiles */
    pick(in(LJ_WIN_PROFILE_EDITOR, "lvGobo"), 0);
    click(in(LJ_WIN_PROFILE_EDITOR, "bGetGoboBmp"));
    CHECK(opened(LJ_WIN_BITMAP_PICKER) && strcmp(lj_window_caption(lj_win_get(LJ_WIN_BITMAP_PICKER))->text, "Get Gobo/Effects Bitmap") == 0);
    CHECK(lv_n(in(LJ_WIN_BITMAP_PICKER, "LV1")) > 0);
    CHECK(((const lj_w_listview *)in(LJ_WIN_BITMAP_PICKER, "LV1"))->tile_w == 75);   /* ME-160: the vsIcon view (tiles, captions under them) */
    golden(LJ_WIN_BITMAP_PICKER, "W4C_BITMAP_PICKER");
    pick(in(LJ_WIN_BITMAP_PICKER, "LV1"), 0);
    lj_w_emit(in(LJ_WIN_BITMAP_PICKER, "LV1"), LJ_WEV_DBLCLICK);
    CHECK(!opened(LJ_WIN_BITMAP_PICKER) && ed->raw[0x1CFC] == 1 && ed->raw[0x1CFC + 11] > 0 && memcmp(ed->raw + 0x1CFC + 12, "gobo_", 5) == 0);
    /* Set (icon): Get Bitmap over our icons */
    click(in(LJ_WIN_PROFILE_EDITOR, "bSetIconBMP"));
    CHECK(opened(LJ_WIN_GET_BITMAP) && lv_n(in(LJ_WIN_GET_BITMAP, "LB1")) >= 2);
    golden(LJ_WIN_GET_BITMAP, "W4C_GET_BITMAP");
    ((lj_w_list *)in(LJ_WIN_GET_BITMAP, "LB1"))->index = 0; lj_w_emit(in(LJ_WIN_GET_BITMAP, "LB1"), LJ_WEV_CLICK);
    click(in(LJ_WIN_GET_BITMAP, "bOk"));
    CHECK(!opened(LJ_WIN_GET_BITMAP) && ed->raw[0x1063] > 0);
    /* Apply to all Selected Fixtures: Apply user profile */
    lj_pe_set_str(ed, lj_pe_field_find("cbManufacturer"), 0, "Generic");
    lj_w_edit_set_text(in(LJ_WIN_PROFILE_EDITOR, "eFixtureName"), "W4");
    click(in(LJ_WIN_PROFILE_EDITOR, "bApplyAll"));
    CHECK(opened(LJ_WIN_APPLY_MULTI) && lv_n(in(LJ_WIN_APPLY_MULTI, "LV1")) == 2);
    golden(LJ_WIN_APPLY_MULTI, "W4C_APPLY_MULTI");
    click(in(LJ_WIN_APPLY_MULTI, "bIdOnly"));
    CHECK(lv_n(in(LJ_WIN_APPLY_MULTI, "LV1")) == 2);         /* both carry the same (empty) Fixture Id */
    click(in(LJ_WIN_APPLY_MULTI, "bOk"));
    CHECK(!opened(LJ_WIN_APPLY_MULTI) && sh->fixture[7].kind == LJ_FX_GS2 && sh->fixture[8].kind == LJ_FX_GS2 && strcmp(sh->fixture[8].gs2_file, "Generic_W4.GS2") == 0);
    lj_win_close(LJ_WIN_PROFILE_EDITOR);
    /* a format-1 record: the editor opens TProfileNameOldStyleForm; Suggest Name translates, OK splits into the 2.6b9 slots */
    lj_pe_begin(ed, NULL);
    memcpy(legacy_raw, ed->raw, sizeof legacy_raw);
    legacy_raw[0] = 1; legacy_raw[1] = 0;
    legacy_raw[0x0009] = 16; memcpy(legacy_raw + 0x000A, "HES Studio Color", 16);
    memset(&legacy, 0, sizeof legacy);
    CHECK(lj_gs2_read(legacy_raw, sizeof legacy_raw, &legacy, err, sizeof err) == 0 && legacy.format_version == 1);
    lj_str_copy(legacy.gs2_file, sizeof legacy.gs2_file, "HES_Studio.GS2");
    sh->gs2 = &legacy; sh->ngs2 = 1;
    sh->fixture[9] = sh->fixture[7]; sh->fixture[9].address = 300; lj_str_copy(sh->fixture[9].gs2_file, sizeof sh->fixture[9].gs2_file, "HES_Studio.GS2");
    lj_sel_none(); lj_sel_set(9, 1, 1);
    { lj_window *w = lj_win_open(LJ_WIN_CUSTOMIZE_FIXTURE); CHECK(w != NULL && w == lj_win_get(LJ_WIN_PROFILE_EDITOR)); } CHECK(opened(LJ_WIN_PROFILE_NAME_OLDSTYLE));
    CHECK(strcmp(lj_w_edit_text(in(LJ_WIN_PROFILE_NAME_OLDSTYLE, "eName")), "HES Studio Color") == 0);
    golden(LJ_WIN_PROFILE_NAME_OLDSTYLE, "W4C_PROFILE_NAME_OLDSTYLE");
    click(in(LJ_WIN_PROFILE_NAME_OLDSTYLE, "Button1"));
    CHECK(strcmp(lj_w_edit_text(in(LJ_WIN_PROFILE_NAME_OLDSTYLE, "eName")), "High End Systems Studio Color") == 0);
    click(in(LJ_WIN_PROFILE_NAME_OLDSTYLE, "bOk"));
    ed = lj_profile_editor_work();
    CHECK(!opened(LJ_WIN_PROFILE_NAME_OLDSTYLE) && ed != NULL);
    if (ed) {
        char t[40];
        lj_pe_get_str(ed, lj_pe_field_find("cbManufacturer"), 0, t, sizeof t); CHECK(strcmp(t, "High End Systems") == 0);
        lj_pe_get_str(ed, lj_pe_field_find("eFixtureName"), 0, t, sizeof t); CHECK(strcmp(t, "Studio Color") == 0);
        CHECK(ed->raw[0] == 0x31 && ed->raw[1] == 0x0A && lj_pe_validate(ed) == NULL);
    }
    lj_win_close(LJ_WIN_PROFILE_EDITOR);
    if (lj_win_message_window()) msg_answer(1);
    for (k = 7; k <= 9; k++) memset(&sh->fixture[k], 0, sizeof sh->fixture[k]);
}

/* ---- the profile import / export boxes ---- */
static void test_import_export(void)
{
    lj_library *lib = lj_ctx_get()->lib;
    char p[LJ_PATH_CAP], q[LJ_PATH_CAP];
    FILE *f;
    /* Create Export File: Warning, then File Already Exists on the second export, Rename New File */
    CHECK(lj_win_open(LJ_WIN_GS2_EXPORT) != NULL);
    ((lj_w_list *)lj_window_find(lj_win_get(LJ_WIN_GS2_EXPORT), "lb1"))->index = 0;
    click(lj_window_find(lj_win_get(LJ_WIN_GS2_EXPORT), "bSave"));
    CHECK(opened(LJ_WIN_EXPORT_WARNING));
    golden(LJ_WIN_EXPORT_WARNING, "W4C_EXPORT_WARNING");
    click(in(LJ_WIN_EXPORT_WARNING, "bYes"));
    CHECK(!opened(LJ_WIN_EXPORT_WARNING) && lj_library_path(lib, "Export/HES_Studio.Udf", p, sizeof p) == 0 && lj_fs_exists(p));
    lj_win_refresh(LJ_WIN_GS2_EXPORT);
    CHECK(lj_win_open(LJ_WIN_GS2_EXPORT) != NULL);
    ((lj_w_list *)lj_window_find(lj_win_get(LJ_WIN_GS2_EXPORT), "lb1"))->index = 0;
    click(lj_window_find(lj_win_get(LJ_WIN_GS2_EXPORT), "bSave"));
    click(in(LJ_WIN_EXPORT_WARNING, "bYes"));
    CHECK(opened(LJ_WIN_FILE_EXISTS));
    golden(LJ_WIN_FILE_EXISTS, "W4C_FILE_EXISTS");
    click(in(LJ_WIN_FILE_EXISTS, "bRename"));
    CHECK(!opened(LJ_WIN_FILE_EXISTS) && lj_library_path(lib, "Export/HES_Studio_1.Udf", q, sizeof q) == 0 && lj_fs_exists(q));
    lj_win_close(LJ_WIN_GS2_EXPORT);
    /* Import (check modified): an identical file is skipped, a different one asks File Exists */
    CHECK(lj_library_path(lib, "UserFixtureDef/HES_Studio.GS2", p, sizeof p) == 0 && lj_gs2_save(p, &legacy) == 0);
    CHECK(lj_library_path(lib, "Import", q, sizeof q) == 0 && lj_fs_mkdir(q) == 0);
    CHECK(lj_library_path(lib, "Import/HES_Studio.GS2", q, sizeof q) == 0 && lj_gs2_save(q, &legacy) == 0);
    CHECK(lj_gs2_import_folder(1) == 0 && !opened(LJ_WIN_PROFILE_IMPORT_MODIFIED));
    legacy_raw[0x0043] = 3; memcpy(legacy_raw + 0x0044, "Joe", 3);   /* Author differs */
    f = fopen(q, "wb"); if (f) { fwrite(legacy_raw, 1, sizeof legacy_raw, f); fclose(f); }
    CHECK(lj_gs2_import_folder(1) == 0 && opened(LJ_WIN_PROFILE_IMPORT_MODIFIED));
    golden(LJ_WIN_PROFILE_IMPORT_MODIFIED, "W4C_PROFILE_IMPORT_MODIFIED");
    click(in(LJ_WIN_PROFILE_IMPORT_MODIFIED, "bUseImport"));
    CHECK(!opened(LJ_WIN_PROFILE_IMPORT_MODIFIED) && act_count[LJ_ACT_GS2_RELOAD] >= 1);
    {   lj_profile back; char err[160]; memset(&back, 0, sizeof back);
        CHECK(lj_gs2_load(p, &back, err, sizeof err) == 0 && strcmp(back.author, "Joe") == 0); lj_gs2_free(&back); }
    sh->gs2 = NULL; sh->ngs2 = 0;
}

static void test_rgb_palette(void)
{
    char p[LJ_PATH_CAP];
    CHECK(lj_win_open(LJ_WIN_RGB_PALETTE) != NULL);
    CHECK(((lj_w_treeview *)in(LJ_WIN_RGB_PALETTE, "tv1"))->n == 16 && !in(LJ_WIN_RGB_PALETTE, "pAdd")->visible);
    golden(LJ_WIN_RGB_PALETTE, "W4C_RGB_PALETTE");
    ((lj_w_treeview *)in(LJ_WIN_RGB_PALETTE, "tv1"))->index = 1;
    CHECK(fire_menu(lj_win_get(LJ_WIN_RGB_PALETTE), "Pop_DeleteEntry") == 0 && ((lj_w_treeview *)in(LJ_WIN_RGB_PALETTE, "tv1"))->n == 15);
    CHECK(lj_library_path(lj_ctx_get()->lib, "RGBPalette.Ini", p, sizeof p) == 0 && lj_fs_exists(p));
    lj_win_close(LJ_WIN_RGB_PALETTE);
    CHECK(lj_win_open(LJ_WIN_RGB_PALETTE) != NULL && ((lj_w_treeview *)in(LJ_WIN_RGB_PALETTE, "tv1"))->n == 15);   /* read back from the file */
    lj_win_close(LJ_WIN_RGB_PALETTE);
}

int lj_test_config_dialogs(int argc, char **argv);
int lj_test_config_dialogs(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_library lib;
    lj_show *keep = ctx->show;
    lj_library *keep_lib = ctx->lib;
    lj_ini *keep_forms = ctx->forms_ini;
    int a, id;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, on_action, NULL);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "Lib\n"); return 1; }
    ctx->lib = &lib;
    ctx->show = sh = lj_show_new();
    ctx->forms_ini = lj_ini_new();
    lj_uiscript_set_host(NULL);
    test_fixture_config();
    test_copy_clear();
    test_popup_windows();
    test_profile_pickers();
    test_import_export();
    test_rgb_palette();
    lj_win_close(LJ_WIN_FIXTURE_CONFIG);
    if (lj_win_message_window()) msg_answer(1);
    for (id = LJ_WIN_ADD_FIXTURES; id <= LJ_WIN_RGB_PALETTE; id++) CHECK(!opened((enum lj_win)id));
    CHECK(complaints == 0);                                    /* ME-16 both ways, and every CONFIG id has its body */
    lj_warnings_close_all();
    lj_gs2_free(&legacy);
    lj_show_free(ctx->show);
    lj_ini_free(ctx->forms_ini);
    ctx->show = keep; ctx->lib = keep_lib; ctx->forms_ini = keep_forms;
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, NULL, NULL);
    lj_log_set_sink(NULL, NULL);
    printf("test_config_dialogs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
