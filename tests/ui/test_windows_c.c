/* test_windows_c.c - the runnable checks of the windows-c track (design 10 W1,
 * docs/w1-gates.md): Fixture Configuration tree (201 exe rows, the MAC 500 path),
 * address 17 -> DIP bits 10001, two fixtures -> Config.dat lines (design 5.2),
 * the DMX Output Monitor colour table and the <= 11-step channel search, the sACN
 * page rejecting priority 250, the warning boxes' LStatus table, the close prompt
 * and the Startup Options keys. Rows that need another track's body (fixlib rows,
 * the model's Config.dat writer, lj_sacn_cfg_validate) print SKIP while the W1 stub
 * is in place and run for real once it lands. Headless: no lj_win_host. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_dmx_monitor.h"
#include "windows/lj_fixture_config.h"
#include "windows/lj_hardware_setup.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"
#include "controls/lj_ctl.h"              /* W2 windows-c rows */
#include "core/lj_lock.h"
#include "io/lj_fn.h"
#include "model/lj_patch.h"
#include "model/lj_sequence.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_default_output.h"
#include "windows/lj_fixture_groups.h"
#include "windows/lj_global_patch.h"
#include "windows/lj_htp_groups.h"
#include "windows/lj_intensity_groups.h"
#include "windows/lj_position_presets.h"
#include "windows/lj_snapshot.h"
#include "windows/lj_user_palettes.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails, skips;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define SKIP(why) do { skips++; printf("SKIP %s:%d %s\n", __FILE__, __LINE__, why); } while (0)

static void quiet(void *ud, const char *line) { (void)ud; (void)line; }
static int answer_yes, answer_called;
static void on_answer(int yes, void *ud) { (void)ud; answer_yes = yes; answer_called++; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL %s:%d widget missing\n", __FILE__, __LINE__); } }

static void test_warnings(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *w;
    CHECK(strcmp(lj_warnings_status_text(LJ_WARN_BLACKOUT), "BLACKOUT ACTIVE") == 0);
    CHECK(strcmp(lj_warnings_status_text(LJ_WARN_FADE_OUT), "FADE-OUT ACTIVE") == 0);
    CHECK(strcmp(lj_warnings_status_text(LJ_WARN_FADE_IN), "FADE-IN ACTIVE") == 0);
    CHECK(strcmp(lj_warnings_status_text(LJ_WARN_FROZEN), "Frozen !") == 0);
    CHECK(strcmp(lj_warnings_status_text(LJ_WARN_OFFLINE), "DMX Output is Offline") == 0);
    lj_win_warning(LJ_WARN_BLACKOUT, 1);
    w = lj_warnings_window(LJ_WARN_BLACKOUT);
    CHECK(w && strcmp(lj_window_desc(w)->class_name, "TStatusWarningForm") == 0);
    CHECK(w && strcmp(lj_w_caption(lj_window_find(w, "LStatus")), "BLACKOUT ACTIVE") == 0);
    lj_win_warning(LJ_WARN_FADE_IN, 1);                                   /* the same box, LStatus rewritten (LORD_FIDELITY-12) */
    CHECK(lj_warnings_window(LJ_WARN_FADE_IN) == w && strcmp(lj_w_caption(lj_window_find(w, "LStatus")), "FADE-IN ACTIVE") == 0);
    lj_win_warning(LJ_WARN_FROZEN, 1);
    CHECK(lj_warnings_window(LJ_WARN_FROZEN) && lj_window_desc(lj_warnings_window(LJ_WARN_FROZEN))->color == LJ_CLR(0xFF0000));   /* red boxes [07 s2] */
    click(lj_window_find(lj_warnings_window(LJ_WARN_FROZEN), "bCancel"));
    CHECK(lj_warnings_window(LJ_WARN_FROZEN) == NULL);
    lj_win_warning(LJ_WARN_BLACKOUT, 0);
    CHECK(lj_warnings_window(LJ_WARN_BLACKOUT) == NULL);

    /* close prompt: the applicable "was modified" lines, Cancel = 0, OK = 1 */
    ctx->modified[LJ_MOD_SEQ] = 1; ctx->modified[LJ_MOD_CUE] = 0; ctx->modified[LJ_MOD_CL] = 0;
    answer_called = 0;
    lj_win_close_prompt(on_answer, NULL);
    w = lj_win_close_prompt_window();
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Close Lightjockey ?") == 0 && answer_called == 0);
    CHECK(w && lj_window_find(w, "LSeq")->visible && !lj_window_find(w, "LCue")->visible && !lj_window_find(w, "LCl")->visible);
    CHECK(w && lj_window_find(w, "Panel1")->color == 0xFF8000u);            /* the orange panel (DFM Color = 33023 = $00BBGGRR -> #ff8000 [07 s2]) */
    click(lj_window_find(w, "BitBtn2"));
    CHECK(answer_called == 1 && answer_yes == 0 && lj_win_close_prompt_window() == NULL);
    lj_win_close_prompt(on_answer, NULL);
    click(lj_window_find(lj_win_close_prompt_window(), "BitBtn1"));
    CHECK(answer_called == 2 && answer_yes == 1);
    ctx->modified[LJ_MOD_SEQ] = 0;
    lj_ini_seti(ctx->main_ini, "PREFS", "CloseConfirm", 0);
    lj_win_close_prompt(on_answer, NULL);
    CHECK(answer_called == 3 && answer_yes == 1 && lj_win_close_prompt_window() == NULL);   /* CloseConfirm=0 + nothing modified: no prompt */
    lj_ini_seti(ctx->main_ini, "PREFS", "CloseConfirm", 1);

    /* confirm + message boxes (TCONFIRMFORM) */
    answer_called = 0;
    CHECK(lj_win_confirm("Please Confirm..", "Remove Selected Fixtures ?", NULL, on_answer, NULL) != NULL);
    CHECK(strcmp(lj_w_caption(lj_window_find(lj_win_message_window(), "Label1")), "Remove Selected Fixtures ?") == 0);
    click(lj_window_find(lj_win_message_window(), "BOk"));
    CHECK(answer_called == 1 && answer_yes == 1 && lj_win_message_window() == NULL);
    lj_win_message("Warnings", "Fixture 1 has no DMX Address");
    CHECK(lj_win_message_window() && !lj_window_find(lj_win_message_window(), "BCancel")->visible);
    click(lj_window_find(lj_win_message_window(), "BOk"));
    CHECK(lj_win_message_window() == NULL);
}

static void test_fixture_config(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *sh = ctx->show;
    lj_window *w;
    char dip[16], tmp[1024], line[256];
    int i, n = lj_fixlib_count();
    w = lj_fixture_config_open();
    CHECK(w && lj_window_desc(w)->client_width == 782 && lj_window_desc(w)->client_height == 544);   /* 790x590 outer (acceptance 10) */
    if (n == 0) SKIP("fixlib stub: lj_fixlib_count() == 0, tree rows and the MAC 500 path not checked");
    else {
        int leaf = lj_fixture_config_tree_find("Martin", "MAC 500", "Mode 2/4");
        int dim = lj_fixture_config_tree_find("Generic", "DMX Dimmer", " 1 Channel");
        CHECK(lj_fixture_config_tree_leaves() == 201);                                 /* the exe rows; extras (ids >= 1000) hidden by default (design 6.1) */
        for (i = 0; i < lj_fixture_config_tree_count(); i++) CHECK(lj_fixture_config_tree_id(i) <= 200);
        CHECK(leaf >= 0 && lj_fixture_config_tree_id(leaf) == 12);
        CHECK(leaf >= 0 && strcmp(lj_fixture_config_tree_item(leaf), "\t\tMode 2/4") == 0);
        CHECK(dim >= 0 && lj_fixture_config_tree_id(dim) == 19);
        for (i = 0; i < lj_fixture_config_tree_count(); i++) if (lj_fixture_config_tree_id(i) >= 0 && !lj_fixture_config_tree_available(i)) break;
        printf("  tree: %d items, %d leaves, first greyed leaf %d\n", lj_fixture_config_tree_count(), lj_fixture_config_tree_leaves(), i < lj_fixture_config_tree_count() ? i : -1);
    }
    /* address 17 -> DIP bits 10001 (switch i = bit i-1, '1' = down) */
    CHECK(lj_fixture_config_dip_text(17, dip, sizeof dip) == 0 && strncmp(dip, "10001", 5) == 0 && strcmp(dip, "1000100000") == 0);
    CHECK(lj_fixture_config_dip_text(123, dip, sizeof dip) == 0 && strcmp(dip, "1101111000") == 0);   /* fixtureconfigurationsettings.gif at 123 */
    /* two fixtures: MAC 500 at 1:001, DMX Dimmer 1 at 1:017 (acceptance 2) */
    if (lj_fixlib_by_id(12) && lj_fixlib_by_id(19)) {
        CHECK(lj_fixture_config_add(12, 1, 1) == 0 && lj_fixture_config_add(19, 1, 17) == 1);
        CHECK(strcmp(sh->fixture[0].user_name, "MAC500") == 0 && strcmp(sh->fixture[1].user_name, "DMX1") == 0);
    } else {
        SKIP("fixlib stub: lj_fixlib_by_id(12/19) == NULL, rows planted directly");
        memset(&sh->fixture[0], 0, sizeof sh->fixture[0]); sh->fixture[0].kind = LJ_FX_BUILTIN; sh->fixture[0].profile_id = 12; sh->fixture[0].link = 1; sh->fixture[0].address = 1; strcpy(sh->fixture[0].user_name, "MAC500");
        memset(&sh->fixture[1], 0, sizeof sh->fixture[1]); sh->fixture[1].kind = LJ_FX_BUILTIN; sh->fixture[1].profile_id = 19; sh->fixture[1].link = 1; sh->fixture[1].address = 17; strcpy(sh->fixture[1].user_name, "DMX1");
        lj_fixture_config_refresh();
    }
    CHECK(lj_fixture_config_select(1) == 0 && lj_w_dipswitch_value(lj_window_find(w, "DipSwitch1")) == 17);
    CHECK(strcmp(lj_w_edit_text(lj_window_find(w, "eAdr")), "17") == 0 && lj_w_speedbutton_down(lj_window_find(w, "sbL1")));
    CHECK(lj_window_find(w, "pFixtureSettings")->visible);
    CHECK(lj_fixture_config_set_address(1, 2, 123) == 0 && lj_w_dipswitch_value(lj_window_find(w, "DipSwitch1")) == 123 && lj_w_speedbutton_down(lj_window_find(w, "sbl2")));
    CHECK(lj_fixture_config_set_address(1, 1, 17) == 0 && lj_fixture_config_modified());
    CHECK(strstr(lj_w_caption(lj_window_find(w, "LFixtures")), "2 Fixtures configured") != NULL);
    /* flags through the check boxes */
    lj_w_checkbox_set_checked(lj_window_find(w, "cbIgnoreGM"), 1);
    CHECK(sh->fixture[1].ignore_gm == 1);
    lj_w_checkbox_set_checked(lj_window_find(w, "cbIgnoreGM"), 0);
    CHECK(sh->fixture[1].ignore_gm == 0);
    /* Config.dat lines per design 5.2 */
    snprintf(tmp, sizeof tmp, "%s", "lj_test_windows_c_config.dat");
    if (lj_config_save(tmp, sh) != 0) SKIP("model stub: lj_config_save returned -1, Config.dat lines not checked");
    else {
        FILE *f = fopen(tmp, "rb");
        int got1 = 0, got2 = 0, header = 0;
        CHECK(f != NULL);
        while (f && fgets(line, sizeof line, f)) {
            if (strncmp(line, "LJ4 config 1", 12) == 0) header = 1;
            if (strncmp(line, "fx 1 ", 5) == 0 && strstr(line, "kind=builtin") && strstr(line, " id=12 ") && strstr(line, " link=1 ") && strstr(line, " addr=1 ") && (strstr(line, "user=MAC500") || strstr(line, "user=\"MAC500\""))) got1 = 1;
            if (strncmp(line, "fx 2 ", 5) == 0 && strstr(line, "kind=builtin") && strstr(line, " id=19 ") && strstr(line, " link=1 ") && strstr(line, " addr=17 ") && (strstr(line, "user=DMX1") || strstr(line, "user=\"DMX1\""))) got2 = 1;
        }
        if (f) fclose(f);
        remove(tmp);
        CHECK(header && got1 && got2);
    }
    /* remove with the verbatim prompt: Cancel keeps, OK removes */
    lj_fixture_config_select(1);
    click(lj_window_find(w, "bRemove"));
    CHECK(lj_win_message_window() && strcmp(lj_w_caption(lj_window_find(lj_win_message_window(), "Label1")), "Remove Selected Fixtures ?") == 0);
    click(lj_window_find(lj_win_message_window(), "BCancel"));
    CHECK(sh->fixture[1].link == 1);
    lj_fixture_config_select(1);
    click(lj_window_find(w, "bRemove"));
    click(lj_window_find(lj_win_message_window(), "BOk"));
    CHECK(sh->fixture[1].link == 0 && sh->fixture[1].profile_id == 0);
    /* Print Configuration greyed (ME-31) */
    CHECK(lj_menu_rt_named(lj_window_menus(w), "M_Print") && !lj_menu_rt_named(lj_window_menus(w), "M_Print")->enabled);
    CHECK(lj_menu_rt_named(lj_window_menus(w), "M_ExportConfig") && !lj_menu_rt_named(lj_window_menus(w), "M_ExportConfig")->enabled);
    lj_fixture_config_close();
    CHECK(lj_fixture_config_window() == NULL);
}

static void test_monitor(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *w;
    uint32_t bg, fg;
    unsigned all = LJ_DM_PREF_CHANGES | LJ_DM_PREF_USED | LJ_DM_PREF_MASTER;
    lj_dmx_search s;
    uint8_t inc[LJ_LINKS] = { 1, 1, 1, 1 };
    int target = lj_addr_code(2, 300), found = -1, first, last, steps = 0;
    /* the colour table [01 s5]: grey/white/green cells, black/red/green text */
    lj_dmx_monitor_cell_colour(0, 0, 0, 0, all, &bg, &fg); CHECK(bg == LJ_CLR_BTNHIGHLIGHT && fg == LJ_CLR_3DDKSHADOW);
    lj_dmx_monitor_cell_colour(1, 0, 0, 0, all, &bg, &fg); CHECK(bg == LJ_CLR_BTNFACE && fg == LJ_CLR_3DDKSHADOW);
    lj_dmx_monitor_cell_colour(1, 1, 0, 0, all, &bg, &fg); CHECK(bg == LJ_CLR_GREEN);
    lj_dmx_monitor_cell_colour(1, 0, 1, 0, all, &bg, &fg); CHECK(bg == LJ_CLR_BTNFACE && fg == LJ_CLR_RED);
    lj_dmx_monitor_cell_colour(1, 0, 1, 1, all, &bg, &fg); CHECK(fg == 0x008000u);
    lj_dmx_monitor_cell_colour(1, 1, 1, 0, 0, &bg, &fg); CHECK(bg == LJ_CLR_BTNHIGHLIGHT && fg == LJ_CLR_3DDKSHADOW);   /* every highlight off */
    /* the window: 356x264 outer, menu Preferences / Tools / Help, grid 1 + 205 rows x 1 + 10 columns */
    w = lj_dmx_monitor_open();
    CHECK(w && strcmp(lj_window_desc(w)->caption, "DMX Output Monitor") == 0);
    CHECK(w && ((lj_w_grid *)lj_window_find(w, "sg1"))->rows == 206 && ((lj_w_grid *)lj_window_find(w, "sg1"))->cols == 11);
    CHECK(lj_dmx_monitor_select(lj_addr_code(1, 1)) == 0 && lj_dmx_monitor_selected() == 0 && strncmp(lj_dmx_monitor_status(), "1:001", 5) == 0);
    CHECK(strstr(lj_dmx_monitor_status(), "(1) MAC500") != NULL);                      /* the fixture at 1:001 (planted by test_fixture_config): LJ2's control-function text (ME-28) */
    CHECK(strstr(lj_dmx_monitor_status(), "layer") == NULL);                            /* layer_of only behind [DEBUG] (ME-28) */
    CHECK(lj_dmx_monitor_select(lj_addr_code(1, 17)) == 0 && lj_dmx_monitor_selected() == 16 && strncmp(lj_dmx_monitor_status(), "1:017", 5) == 0);
    CHECK(strstr(lj_dmx_monitor_status(), "not configured") != NULL);                  /* the dimmer was removed by test_fixture_config */
    lj_window_paint(w);                                                                 /* sg1DrawCell over every visible cell */
    CHECK(lj_dmx_monitor_overwrite(16, 200) == -1);                                     /* overwrite disabled by default */
    lj_dmx_monitor_set_overwrite_enabled(1);
    CHECK(lj_dmx_monitor_overwrite(16, 200) == 0 && lj_dmx_monitor_overwrite(16, 257) == -1);
    lj_dmx_monitor_refresh();
    /* Find Channel: 2:300 in <= 11 steps with an oracle answering "is it on now ?" */
    CHECK(lj_dmx_search_begin(&s, inc, 0, 255, 0) == 0 && s.n == LJ_CHANS);
    while (lj_dmx_search_half(&s, &first, &last) == 0 && steps < 20) {
        int yes = target + 1 >= first && target + 1 <= last;                              /* first/last are 1-based channel numbers */
        steps++;
        found = lj_dmx_search_answer(&s, yes);
        if (found >= 0) break;
    }
    CHECK(found == target && steps <= 11 && s.steps == steps);
    printf("  search: 2:300 = code %d found in %d steps\n", found, steps);
    inc[1] = 0;
    CHECK(lj_dmx_search_begin(&s, inc, 1, 255, 0) == 0 && s.n == LJ_CHANS - LJ_LINK_CH - (lj_fixlib_by_id(12) ? lj_fixlib_by_id(12)->footprint : 1));   /* link 2 excluded, the MAC 500's footprint excluded */
    /* the Find Channel dialog opens from the Tools menu handler path */
    {
        lj_menu *m = lj_window_menus(w);
        CHECK(lj_menu_rt_named(m, "FindChannel1") != NULL && lj_menu_rt_named(m, "M_EnableOverwrite")->checked);
    }
    lj_dmx_monitor_close();
    CHECK(lj_dmx_monitor_window() == NULL);
    (void)ctx;
}

static void test_hardware_setup(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_sacn_cfg cfg;
    lj_window *w;
    const char *msg;
    memset(&cfg, 0, sizeof cfg);
    cfg.priority = LJ_SACN_PRIORITY_DEFAULT; cfg.ttl = 1; cfg.universe_of_link[0] = 1; cfg.universe_of_link[1] = 2; cfg.universe_of_link[2] = 3; cfg.universe_of_link[3] = 4;
    strcpy(cfg.name, "LJ4EVR test");
    ctx->sacn = &cfg;
    w = lj_hardware_setup_open();
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Hardware Setup") == 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(w, "lType")), "sACN (E1.31)") == 0);         /* where Martin's cards were */
    CHECK(strcmp(lj_w_caption(lj_window_find(w, "lLicenseStatus")), "") == 0);           /* licence lines gone (D3) */
    CHECK(!lj_window_find(w, "pUSB")->visible && !lj_window_find(w, "bdetectHardware")->enabled);
    CHECK(lj_w_spinedit_value(lj_window_find(w, "sePriority")) == 100 && lj_w_spinedit_value(lj_window_find(w, "seU1")) == 1);
    CHECK(strcmp(lj_w_edit_text(lj_window_find(w, "eName")), "LJ4EVR test") == 0);
    CHECK(lj_hardware_setup_set_priority(250) == 0);
    msg = lj_hardware_setup_apply();
    CHECK(msg != NULL && strstr(msg, "priority") != NULL && cfg.priority == 100);       /* rejected, nothing applied */
    printf("  apply with priority 250: \"%s\"\n", msg ? msg : "(accepted)");
    if (lj_win_message_window()) click(lj_window_find(lj_win_message_window(), "BOk"));
    lj_hardware_setup_set_priority(150);
    lj_w_edit_set_text(lj_window_find(w, "eUnicast1"), "192.168.1.300");
    msg = lj_hardware_setup_apply();
    CHECK(msg != NULL && strstr(msg, "unicast") != NULL);
    if (lj_win_message_window()) click(lj_window_find(lj_win_message_window(), "BOk"));
    lj_w_edit_set_text(lj_window_find(w, "eUnicast1"), "192.168.1.20, 10.0.0.5");
    msg = lj_hardware_setup_apply();
    if (msg && strcmp(msg, "not implemented") == 0) SKIP("sacn stub: lj_sacn_cfg_validate not implemented, the accepted path not checked");
    else {
        CHECK(msg == NULL && cfg.priority == 150 && cfg.nunicast[0] == 2 && cfg.unicast[0][1] == 0x0A000005u);
    }
    if (lj_win_message_window()) click(lj_window_find(lj_win_message_window(), "BOk"));
    lj_hardware_setup_close();
    CHECK(lj_hardware_setup_window() == NULL);
    ctx->sacn = NULL;
}

static void test_prefs(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_startup_opts o, o2;
    lj_window *w, *sp;
    lj_startup_opts_load(ctx->main_ini, &o);
    CHECK(o.seq == -1 && o.cue == -1 && o.cl == -1 && o.pcue == -1 && o.auto_lamp_on == 0 && !o.seq_run);
    o.seq = 1; o.cue = 12; o.seq_run = 1; o.auto_lamp_on = 3; o.show_notes = 1;
    lj_startup_opts_save(ctx->main_ini, &o);
    lj_startup_opts_load(ctx->main_ini, &o2);
    CHECK(memcmp(&o, &o2, sizeof o) == 0 && lj_ini_geti(ctx->main_ini, "STARTUP", "PCUE", 0) == -1);
    w = lj_startup_options_open();
    CHECK(w && strcmp(lj_w_edit_text(lj_window_find(w, "ESeqNum")), "0001") == 0 && strcmp(lj_w_edit_text(lj_window_find(w, "ECueNum")), "0012") == 0);
    CHECK(strcmp(lj_w_edit_text(lj_window_find(w, "EClNum")), "") == 0 && lj_w_checkbox_checked(lj_window_find(w, "SeqRun")));
    CHECK(lj_w_radiogroup_index(lj_window_find(w, "RGAutolamp")) == 3);
    click(lj_window_find(w, "CueClear"));
    lj_w_checkbox_set_checked(lj_window_find(w, "cbRunMinimized"), 1);
    click(lj_window_find(w, "BOk"));                                     /* Ok = save + close */
    CHECK(lj_startup_options_window() == NULL && lj_ini_geti(ctx->main_ini, "STARTUP", "Cue", 0) == -1 && lj_ini_geti(ctx->main_ini, "STARTUP", "Minimized", 0) == 1);
    /* the four inert 2.209 boxes (ME-31): present, persisted, nothing else */
    sp = lj_window_create(&lj_form_TSYSTEMPREFSFORM, NULL);
    CHECK(sp && lj_window_find(sp, "cbExternal01") && lj_window_find(sp, "cbSecondary"));
    lj_w_checkbox_set_checked(lj_window_find(sp, "cbPrimary"), 1);
    lj_prefs_inert_save(sp, ctx->main_ini);
    lj_w_checkbox_set_checked(lj_window_find(sp, "cbPrimary"), 0);
    lj_prefs_inert_load(sp, ctx->main_ini);
    CHECK(lj_w_checkbox_checked(lj_window_find(sp, "cbPrimary")) == 1 && lj_w_checkbox_checked(lj_window_find(sp, "cbSecondary")) == 0);
    lj_window_destroy(sp);
}

/* ---- W2 windows-c (docs/w2-gates.md, the brief's five rows): groups round trip, Global Patch Apply = one swap with the
 * table, default output = the engine frame, snapshot scene = the frame, preset recall = the selection's pan/tilt; plus
 * Intensity Groups, HTP Groups and user palettes. A synthetic desktop: MAC 500 x2 + a dimmer, a real engine, headless. */
static int lj_t_posts; static lj_cmd lj_t_last;
static void rec_post(const lj_cmd *c) { lj_t_posts++; lj_t_last = *c; }
static void ok_box(void) { if (lj_win_message_window()) click(lj_window_find(lj_win_message_window(), "BOk")); }
static void engine_run(uint64_t *t, int ticks)   /* the engine thread's loop, then the app's view copy */
{
    lj_ctx *ctx = lj_ctx_get();
    int i;
    for (i = 0; i < ticks; i++) { *t += 25000; lj_lock(); lj_engine_tick(ctx->engine, *t); lj_unlock(); }
    lj_lock(); lj_engine_view(ctx->engine, &ctx->view); lj_unlock();
}
static void wc_golden(lj_window *win, const char *name)   /* the windows-b pattern: LJ_GOLDEN_UPDATE=1 writes, else byte compare */
{
    char golden[1024], tmp[1024];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    FILE *fa, *fb;
    int ca, cb, same = 1;
    if (!win) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(golden, sizeof golden, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, "lj_test_windows_c_%s.bmp", name);
    lj_window_paint(win);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(win), golden) == 0); printf("  %s: written\n", golden); return; }
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    fa = fopen(tmp, "rb"); fb = fopen(golden, "rb");
    if (!fa || !fb) same = -1;
    else do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { same = 0; break; } } while (ca != EOF);
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    if (same != 1) { fails++; printf("FAIL golden %s: %s (the render is %s)\n", name, same ? "missing or unreadable" : "differs", tmp); return; }
    printf("  %s: identical\n", name);
    remove(tmp);
}
static void plant(lj_show *sh, int i, int id, int addr)
{
    memset(&sh->fixture[i], 0, sizeof sh->fixture[i]);
    sh->fixture[i].kind = LJ_FX_BUILTIN; sh->fixture[i].profile_id = (uint16_t)id; sh->fixture[i].link = 1; sh->fixture[i].address = (uint16_t)addr;
    snprintf(sh->fixture[i].user_name, sizeof sh->fixture[i].user_name, "FX%d", i + 1);
}

static void test_w2(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *keep = ctx->show, *sh;
    lj_sequence *seq;
    lj_scene *sc;
    const lj_profile *mac = lj_fixlib_by_id(12), *dim = lj_fixlib_by_id(19);
    lj_frame fr;
    uint64_t t = 0;
    int c, ok, n;

    if (!mac || !dim || !mac->pt16) { SKIP("fixlib: MAC 500 (12) / DMX Dimmer (19) missing, W2 windows-c rows not run"); return; }
    sh = ctx->show = lj_show_new();
    plant(sh, 0, 12, 1); plant(sh, 1, 12, 33); plant(sh, 2, 19, 100);
    seq = lj_sequence_new_rec(); seq->number = 1;
    sh->current_seq = seq; ctx->current_scene = 0;
    ctx->engine = lj_engine_create();
    CHECK(ctx->engine != NULL);
    if (!ctx->engine) { lj_show_free(sh); ctx->show = keep; return; }
    lj_lock(); lj_engine_bind(ctx->engine, sh); lj_unlock();
    lj_win_set_host(NULL);   /* registers the windows' hotkey rows */
    lj_sel_none();

    /* Fixture Groups: Add Group, assign the selection, select through the window (Exclusive), filter, hotkey, popup delete */
    CHECK(lj_fixture_groups_open() != NULL && strcmp(lj_window_desc(lj_fixture_groups_window())->caption, "Fixture Groups") == 0);
    CHECK(lj_fixture_groups_add("Floor") == 1 && strcmp(sh->user_group[0].name, "Floor") == 0);
    lj_sel_set(0, 1, 1); lj_sel_set(2, 1, 0);
    CHECK(lj_fixture_groups_assign(1, 1) == 2 && sh->user_group[0].member[0] && !sh->user_group[0].member[1] && sh->user_group[0].member[2]);
    lj_sel_none(); lj_sel_set(1, 1, 1);
    lj_sel_mode_set(LJ_SEL_EXCLUSIVE);
    CHECK(lj_fixture_groups_select(1) == 2 && lj_sel_is_selected(0) && !lj_sel_is_selected(1) && lj_sel_is_selected(2) && lj_sel_master() == 0);
    lj_fixture_groups_set_filter(2);                                                    /* Filter:Even: fixture numbers 1 and 3 are odd */
    CHECK(lj_fixture_groups_select(1) == 0 && lj_sel_count() == 0);
    lj_fixture_groups_set_filter(0);
    lj_sel_none();
    lj_fn_run(LJ_FN_FIXTURE_GROUP_1, 1);                                               /* hotkey "Fixture group 1" */
    CHECK(lj_sel_count() == 2 && lj_sel_is_selected(2));
    lj_sel_none(); lj_sel_set(2, 1, 1);
    CHECK(lj_fixture_groups_assign(1, 0) == 1 && !sh->user_group[0].member[2]);       /* round trip: the dimmer leaves again */
    CHECK(lj_fixture_groups_add("Back") == 2);
    wc_golden(lj_fixture_groups_window(), "WIN_FIXTURE_GROUPS");
    CHECK(lj_fixture_groups_popup("P_Delete", 2) == 0 && lj_win_message_window() != NULL);
    ok_box();
    CHECK(sh->user_group[1].name[0] == 0 && sh->user_group[0].name[0] != 0);
    lj_fixture_groups_close();
    lj_sel_mode_set(LJ_SEL_INCLUSIVE);

    /* Intensity Groups: tab 2 takes the selection, Save writes the show */
    lj_sel_none(); lj_sel_set(0, 1, 1);
    CHECK(lj_intensity_groups_open() != NULL);
    CHECK(lj_intensity_groups_tab(2) == 0 && lj_intensity_groups_assign(0) == 1 && lj_intensity_groups_group_of(0) == 2);
    CHECK(sh->fixture[0].intensity_group == 0);                                         /* a working copy until Save */
    CHECK(lj_intensity_groups_save() == 0 && sh->fixture[0].intensity_group == 2);
    wc_golden(lj_intensity_groups_window(), "WIN_INTENSITY_GROUPS");                   /* W2 integration: tab 2 with the MAC 500 assigned */
    lj_intensity_groups_close();

    /* Global Patch: edit channel 6 (code 5) on the working copy, Apply = ONE GLOBAL_PATCH with show->patch */
    CHECK(lj_global_patch_open() != NULL && lj_global_patch_select(5) == 0);
    lj_w_edit_set_text(lj_window_find(lj_global_patch_window(), "eMin"), "300");
    CHECK(lj_global_patch_set_values() != NULL);                                         /* 300 rejected */
    ok_box();
    lj_w_edit_set_text(lj_window_find(lj_global_patch_window(), "eMin"), "10");
    lj_w_edit_set_text(lj_window_find(lj_global_patch_window(), "eMax"), "200");
    lj_w_checkbox_set_checked(lj_window_find(lj_global_patch_window(), "cbInverted"), 1);
    CHECK(lj_global_patch_set_values() == NULL);
    CHECK(lj_global_patch_work()[5].min == 10 && lj_global_patch_work()[5].max == 200 && lj_global_patch_work()[5].invert == 1);
    CHECK(sh->patch[5].invert == 0);                                                    /* the live table waits for Apply */
    wc_golden(lj_global_patch_window(), "WIN_GLOBAL_PATCH");
    lj_t_posts = 0;
    lj_global_patch_set_post(rec_post);
    CHECK(lj_global_patch_apply() == 0);
    CHECK(lj_t_posts == 1 && lj_t_last.kind == LJ_CMD_GLOBAL_PATCH && lj_t_last.patch == sh->patch);
    CHECK(sh->patch[5].min == 10 && sh->patch[5].max == 200 && sh->patch[5].invert == 1);
    CHECK(strcmp(lj_w_caption(lj_window_find(lj_global_patch_window(), "lInverted")), "Inverted:1") == 0);
    CHECK(lj_global_patch_menu("ClearAll1") == 0 && lj_win_message_window() != NULL);   /* "Clear All Patch Information ?" */
    ok_box();
    CHECK(lj_patch_is_default(&lj_global_patch_work()[5]));
    lj_global_patch_set_post(NULL);
    lj_global_patch_close();
    lj_patch_init(sh->patch);

    /* HTP Groups: the dimmer's intensity channel (1:100 = code 99) joins group 3, then the group is cleared */
    lj_sel_none(); lj_sel_set(2, 1, 1);
    CHECK(lj_htp_groups_open() != NULL);
    CHECK(lj_htp_groups_assign(3, 1) == 1 && (sh->patch[99].htp_mask & 4u) && lj_htp_groups_count(3) == 1);
    wc_golden(lj_htp_groups_window(), "WIN_HTP_GROUPS");
    CHECK(lj_htp_groups_clear(3) == 0 && lj_htp_groups_count(3) == 0);
    lj_htp_groups_close();

    /* Default output: Use Current Scene equals the frame the engine made from that scene */
    sc = &seq->scenes[0];
    sc->val[0] = 77; sc->state[0] = LJ_SNAP; sc->val[99] = 200; sc->state[99] = LJ_SNAP;
    lj_ctx_set_scene(0);
    engine_run(&t, 8);
    lj_lock(); fr = *lj_engine_frame(ctx->engine); lj_unlock();
    printf("  engine frame: merged[0]=%u merged[99]=%u out[0]=%u\n", fr.merged[0], fr.merged[99], fr.out[0][0]);
    CHECK(fr.merged[0] == 77 && fr.merged[99] == 200);
    CHECK(lj_default_output_run(1) == 0);
    for (ok = 1, c = 0; c < LJ_CHANS; c++) if (sc->state[c] != LJ_OFF && sh->default_out[c] != fr.merged[c]) ok = 0;
    CHECK(ok && sh->default_out[0] == 77 && sh->default_out[99] == 200);
    sc->preset[0] = 1;
    CHECK(lj_default_output_run(1) == -1 && lj_win_message_window() != NULL);          /* "Default DMX Output cannot contain Presets" */
    CHECK(lj_win_message_window() && strcmp(lj_w_caption(lj_window_find(lj_win_message_window(), "Label1")), "Default DMX Output cannot contain Presets") == 0);
    ok_box();
    sc->preset[0] = 0;
    CHECK(lj_default_output_run(0) == 0 && sh->default_out[0] == 0);

    /* Snapshot: Static Snapshot with "Auto add scene" adds scene 2 holding the served frame's output */
    CHECK(lj_snapshot_open() != NULL);
    wc_golden(lj_snapshot_window(), "WIN_SNAPSHOT");
    lj_w_checkbox_set_checked(lj_window_find(lj_snapshot_window(), "cbAddScene"), 1);
    CHECK(lj_snapshot_static() == 0);
    engine_run(&t, 1);
    lj_snapshot_tick(0);
    lj_lock(); fr = *lj_engine_frame(ctx->engine); lj_unlock();
    CHECK(seq->n == 2 && ctx->current_scene == 1);
    for (ok = 1, n = 0, c = 0; seq->n == 2 && c < LJ_CHANS; c++) {
        if (lj_show_fixture_at(sh, c) < 0) { ok &= seq->scenes[1].state[c] == LJ_OFF; continue; }
        ok &= seq->scenes[1].val[c] == fr.out[c / LJ_LINK_CH][c % LJ_LINK_CH] && seq->scenes[1].state[c] == LJ_SNAP;
        n += fr.out[c / LJ_LINK_CH][c % LJ_LINK_CH] != 0;
    }
    CHECK(ok && n > 0);
    printf("  snapshot: scene 2 = frame tick %llu, %d non-zero fixture channels\n", (unsigned long long)ctx->view.snapshot_tick, n);
    /* real-time: 1 scene refused with the exe's text, then 2 scenes recorded and converted */
    lj_w_edit_set_text(lj_window_find(lj_snapshot_window(), "eScenes"), "1");
    CHECK(lj_snapshot_realtime() == -1 && lj_win_message_window() != NULL);
    ok_box();
    lj_w_edit_set_text(lj_window_find(lj_snapshot_window(), "eScenes"), "2");
    CHECK(lj_snapshot_realtime() == 0 && lj_snapshot_recording() == 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(lj_snapshot_window(), "pRecordRealTime")), "Recording (Cancel)") == 0);
    for (c = 0; c < 20 && lj_snapshot_recording() >= 0 && !lj_win_message_window(); c++) { lj_snapshot_tick(t); engine_run(&t, 1); lj_snapshot_tick(t); }
    CHECK(lj_win_message_window() != NULL && lj_snapshot_recording() == 2);             /* "Recording finished / Convert to sequence ?" */
    ok_box();
    CHECK(seq->n == 2 && lj_snapshot_recording() == -1 && seq->scenes[0].scene_ms == 100);   /* 10 Scenes/sec default = 100 ms per scene */
    lj_snapshot_close();

    /* Position presets: store 1 from the selection, move pan away, recall puts pan/tilt and the reference back */
    lj_sel_none(); lj_sel_set(0, 1, 1); lj_sel_set(1, 1, 0);
    lj_ctx_set_scene(0);
    sc = &seq->scenes[0];
    memset(sc->state, LJ_OFF, sizeof sc->state);
    sc->val[mac->pan_ofs] = 0x40; sc->state[mac->pan_ofs] = LJ_SNAP; sc->val[mac->tilt_ofs] = 0x90; sc->state[mac->tilt_ofs] = LJ_SNAP;
    sc->val[32 + mac->pan_ofs] = 0x40; sc->state[32 + mac->pan_ofs] = LJ_SNAP; sc->val[32 + mac->tilt_ofs] = 0x90; sc->state[32 + mac->tilt_ofs] = LJ_SNAP;
    if (lj_position_presets_open() == NULL) SKIP("controls: no Movement control for the MAC 500 master, preset rows not run");
    else {
        CHECK(lj_position_presets_store(1, 1) == 0 && sh->preset[0][0].used && sh->preset[1][0].used);
        sc->val[mac->pan_ofs] = 0x10; sc->val[32 + mac->tilt_ofs] = 0x20;
        CHECK(lj_position_presets_recall(1) == 0);
        CHECK(sc->val[mac->pan_ofs] == 0x40 && sc->val[mac->tilt_ofs] == 0x90 && sc->val[32 + mac->tilt_ofs] == 0x90 && sc->preset[0] == 1 && sc->preset[1] == 1);
        lj_fn_run(LJ_FN_TOGGLE_POSITION_PRESET, 1);
        CHECK(lj_position_presets_window() == NULL);
        lj_ctl_close_all();
    }

    /* User palettes: Color entry from the master, overwrite the scene, Assign brings it back, Clear turns it Off */
    {
        const lj_ctl_window *cw = lj_profile_window(mac, LJ_CTL_COLORS);
        int ch = cw && cw->nrows ? cw->rows[0].ch : -1;
        if (ch < 0) SKIP("fixlib: the MAC 500 has no Colors rows, palette rows not run");
        else {
            CHECK(lj_user_palettes_open_kind(LJ_CTL_COLORS) != NULL && strcmp(lj_window_desc(lj_user_palettes_window())->class_name, "TPresetForm") == 0);
            sc->val[ch] = 123; sc->state[ch] = LJ_SNAP;
            CHECK(lj_user_palettes_record(-1, "Red", 0) == 0 && lj_user_palettes_count() == 1);
            CHECK(strcmp(lj_w_caption(lj_window_find(lj_user_palettes_window(), "lentries")), "1 Entries") == 0);
            wc_golden(lj_user_palettes_window(), "WIN_USER_PALETTE");                  /* W2 integration: "User Color Palette" with entry 1 Red */
            sc->val[ch] = 5;
            CHECK(lj_user_palettes_assign(0, 0) >= 1 && sc->val[ch] == 123);
            CHECK(lj_user_palettes_clear(0, 0) >= 1 && sc->state[ch] == LJ_OFF);
            CHECK(lj_user_palettes_delete(0) == 0 && lj_user_palettes_count() == 0);
            lj_user_palettes_close();
        }
    }

    lj_sel_none();
    lj_lock(); lj_engine_bind(ctx->engine, NULL); lj_unlock();
    lj_engine_destroy(ctx->engine); ctx->engine = NULL;
    memset(&ctx->view, 0, sizeof ctx->view);
    lj_show_free(sh);
    ctx->show = keep;
}

int lj_test_windows_c(int argc, char **argv);
int lj_test_windows_c(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    ctx->show = lj_show_new();
    ctx->main_ini = lj_ini_new(); ctx->forms_ini = lj_ini_new();
    test_warnings();
    test_fixture_config();
    test_monitor();
    test_hardware_setup();
    test_prefs();
    test_w2();
    lj_warnings_close_all();
    lj_ini_free(ctx->main_ini); lj_ini_free(ctx->forms_ini); ctx->main_ini = ctx->forms_ini = NULL;
    lj_show_free(ctx->show); ctx->show = NULL;
    lj_log_set_sink(NULL, NULL);
    printf("windows_c: %d failure(s), %d skipped\n", fails, skips);
    return fails ? 1 : 0;
}
