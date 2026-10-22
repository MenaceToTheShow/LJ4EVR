/* test_profile_editor_form.c - `lj4evr_tests test_profile_editor_form` (profile-editor track), headless:
 *  - the 11 tab captions of fixlib/lj_profile_editor.h equal the TScanner2DefsForm DFM ("&&" shown as "&");
 *  - the three windows build with every DFM handler bound and no binding left over (ME-16: no "lj_window:" log line);
 *  - NIT-26: the popup caption is "Customize Fixture" for a built-in, "Define Fixture Profile" for USER2 / a dimmer, and
 *    LJ_ACT_CUSTOMIZE_FIXTURE's window follows it;
 *  - Channel Configuration: caption + Disable Grand Master Control land in lj_fixture, LJ_ACT_FIXTURE_APPLY runs, and
 *    the Levels control shows the caption (ME-32);
 *  - Customize Fixture: Clear Entry + Apply write lj_fixture.custom; Copy from Source Fixture copies a palette;
 *  - Generic Fixture Profile: Set / Clear of a static table, level status, the manufacturer refusal and the
 *    "Any unapplied profile changes will be lost ?" prompt verbatim, Save into UserFixtureDef (SKIP while lj_gs2_save
 *    is the W1 stub);
 *  - goldens W3PE_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl_common.h"
#include "core/lj_log.h"
#include "fixlib/lj_profile_editor.h"
#include "gfx/lj_bmp.h"
#include "model/lj_library.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_profile_editor_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_profile_editor_form_tmp/"

static int fails, binding_complaints;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void sink(void *ud, const char *line) { (void)ud; if (strstr(line, "lj_window:")) { binding_complaints++; printf("  log: %s\n", line); } }
static int act_count[LJ_ACT_COUNT], act_arg[LJ_ACT_COUNT];
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)ud; act_count[a]++; act_arg[a] = arg; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void pick(lj_widget *lv, int row) { if (!lv) { fails++; printf("FAIL list missing\n"); return; } ((lj_w_listview *)lv)->index = row; lj_w_emit(lv, LJ_WEV_CHANGE); }
static const char *msg_line(const char *label) { lj_window *m = lj_win_message_window(); const char *c = m ? lj_w_caption(lj_window_find(m, label)) : NULL; return c ? c : ""; }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); lj_widget *b = m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL; if (b) lj_w_click(b); }

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

static void test_tab_captions(void)
{
    const lj_form_desc *d = &lj_form_TSCANNER2DEFSFORM;
    const lj_ctl_desc *pc = NULL;
    int i, n = 0;
    for (i = 0; i < d->n; i++) if (d->ctls[i].name && strcmp(d->ctls[i].name, "PC1") == 0) pc = &d->ctls[i];
    CHECK(pc != NULL);
    for (i = 0; pc && i < pc->nchildren; i++) {
        char want[64], *o = want;
        const char *c = pc->children[i].caption;
        if (pc->children[i].kind != LJ_K_TABSHEET || !c) continue;
        for (; *c && o < want + sizeof want - 1; c++) { *o++ = *c; if (c[0] == '&' && c[1] == '&') c++; }   /* "&&" is a literal "&" */
        *o = 0;
        if (n < LJ_PE_TAB_COUNT && strcmp(want, lj_pe_tab_caption[n]) != 0) { CHECK(0); printf("  tab %d: DFM \"%s\" vs \"%s\"\n", n, want, lj_pe_tab_caption[n]); }
        n++;
    }
    CHECK(n == LJ_PE_TAB_COUNT);
}

static void patch(lj_show *sh, int f, uint16_t id, int addr)
{
    memset(&sh->fixture[f], 0, sizeof sh->fixture[f]);
    sh->fixture[f].kind = LJ_FX_BUILTIN; sh->fixture[f].profile_id = id; sh->fixture[f].link = 1; sh->fixture[f].address = (uint16_t)addr;
    snprintf(sh->fixture[f].user_name, sizeof sh->fixture[f].user_name, "Fx%d", f + 1);
}

static void test_channel_config(lj_show *sh)
{
    lj_window *w, *lv;
    lj_sel_none(); lj_sel_set(0, 1, 1);
    CHECK(strcmp(lj_customize_popup_caption(0), "Define Fixture Profile") == 0);
    CHECK(strcmp(lj_customize_popup_caption(1), "Customize Fixture") == 0);
    CHECK(strcmp(lj_customize_popup_caption(2), "Define Fixture Profile") == 0);
    w = lj_customize_fixture_open();                           /* the dimmer's "Define Fixture Profile" opens Channel Configuration */
    CHECK(w != NULL && w == lj_channel_config_get() && lj_customize_fixture_get() == NULL);
    if (!w) return;
    CHECK(((lj_w_listview *)in(w, "chList"))->n == 4);
    golden(w, "W3PE_CHANNELCONFIG");
    pick(in(w, "chList"), 1);
    lj_w_edit_set_text(in(w, "eChannelName"), "Inte");
    click(in(w, "bDisableGM"));
    CHECK(strstr(((lj_w_listview *)in(w, "chList"))->items[1], "Inte\tNo") != NULL);
    click(in(w, "BitBtn2"));
    CHECK(lj_channel_config_get() == NULL);
    CHECK(strcmp(sh->fixture[0].ch_caption[1], "Inte") == 0 && sh->fixture[0].ch_gm_off == 2u);
    CHECK(act_count[LJ_ACT_FIXTURE_APPLY] == 1 && act_arg[LJ_ACT_FIXTURE_APPLY] == 0);
    /* the Levels control of the master fixture shows the Channel Configuration caption (ME-32) */
    lv = lj_ctl_open(LJ_CTL_LEVELS);
    {
        int found = 0;
        lj_ctl_win *c = lj_ctl_of(lv);
        int i;
        for (i = 0; c && i < c->n; i++) if (strcmp(c->e[i].text, "Inte") == 0) found = 1;
        CHECK(lv != NULL && found);
    }
    lj_ctl_close(LJ_CTL_LEVELS);
    /* Cancel leaves the fixture alone */
    w = lj_channel_config_open();
    pick(in(w, "chList"), 0);
    click(in(w, "bDisableGM"));
    click(in(w, "BitBtn1"));
    CHECK(lj_channel_config_get() == NULL && sh->fixture[0].ch_gm_off == 2u);
}

static void test_customize(lj_show *sh)
{
    lj_window *w;
    const lj_palette *work;
    lj_sel_none(); lj_sel_set(1, 1, 1);
    w = lj_customize_fixture_open();
    CHECK(w != NULL && w == lj_customize_fixture_get());
    if (!w) return;
    golden(w, "W3PE_CUSTOMIZE");
    CHECK(((lj_w_listview *)in(w, "LV1"))->n > 0);
    pick(in(w, "LV1"), 0);
    click(in(w, "bClear"));
    work = lj_customize_fixture_work();
    CHECK(work && work[0].e[0].used == 1 && work[0].e[0].rgb == LJ_CUSTOM_NO_RGB && !work[0].e[0].bitmap[0]);
    click(in(w, "bGetGoboBmp"));                               /* our art only: a gobo_* name */
    CHECK(work && strncmp(work[0].e[0].bitmap, "gobo_", 5) == 0 && work[0].e[0].rgb == LJ_CUSTOM_NO_RGB);
    click(in(w, "bCopy"));                                     /* TCopySetupForm builds with its handlers bound (ME-16); Apply closes it with the form */
    click(in(w, "bApply"));
    CHECK(lj_customize_fixture_get() == NULL);
    CHECK(sh->fixture[1].custom[0].e[0].used == 1 && strncmp(sh->fixture[1].custom[0].e[0].bitmap, "gobo_", 5) == 0);
    CHECK(act_count[LJ_ACT_FIXTURE_APPLY] == 2 && act_arg[LJ_ACT_FIXTURE_APPLY] == 1);
}

static void test_profile_editor(lj_show *sh)
{
    lj_window *w;
    lj_profile_edit *ed;
    int saves = act_count[LJ_ACT_GS2_RELOAD];
    lj_sel_none(); lj_sel_set(2, 1, 1);
    w = lj_customize_fixture_open();                           /* USER2: "Define Fixture Profile" = the editor on a New Profile */
    CHECK(w != NULL && w == lj_profile_editor_get());
    if (!w) return;
    ed = lj_profile_editor_work();
    CHECK(ed && ed->raw[0] == 0x31 && ed->raw[4] == 0x2E);
    if (!ed) return;
    golden(w, "W3PE_PROFILE_EDITOR");
    pick(in(w, "lvBO"), 3);
    lj_w_edit_set_text(in(w, "eBoVal"), "77");
    click(in(w, "bSetBOVal"));
    CHECK(ed->raw[0x11AA + 17 + 3] == 1 && ed->raw[0x11AA + 49 + 3] == 77);
    click(in(w, "bClearBo"));
    CHECK(ed->raw[0x11AA + 17 + 3] == 0);
    pick(in(w, "lvLevels"), 2);
    click(in(w, "bDisable"));
    CHECK(ed->raw[0x35D1 + 16 * 2 + 7] == 1);
    click(in(w, "bEnable"));
    pick(in(w, "lvDefaultDMX"), 0);
    lj_w_edit_set_text(in(w, "eDefault"), "128");
    click(in(w, "BSetDefault"));
    CHECK(ed->raw[0x1088] == 128);
    lj_w_edit_set_text(in(w, "eAuthor"), "LJ4EVR test");
    lj_w_edit_set_text(in(w, "eShutterOfs"), "5");
    click(in(w, "cbShutter"));
    CHECK(lj_profile_editor_commit(0) == -1);                  /* no manufacturer yet */
    CHECK(strcmp(msg_line("Label1"), "Please specify manufacturer") == 0);
    msg_answer(1);
    CHECK(ed->raw[0x43] == 11 && memcmp(ed->raw + 0x44, "LJ4EVR test", 11) == 0 && ed->raw[0x1173] == 1 && ed->raw[0x1174] == 5);   /* read back from the controls */
    lj_pe_set_str(ed, lj_pe_field_find("cbManufacturer"), 0, "Generic");
    lj_w_edit_set_text(in(w, "eFixtureName"), "Test");          /* the controls are read back on commit */
    if (lj_profile_editor_commit(0) == 0) {
        char path[LJ_PATH_CAP];
        FILE *f = lj_library_path(lj_ctx_get()->lib, "UserFixtureDef\\Generic_Test.GS2", path, sizeof path) == 0 ? fopen(path, "rb") : NULL;
        CHECK(f != NULL && act_count[LJ_ACT_GS2_RELOAD] == saves + 1);
        if (f) fclose(f);
    } else printf("SKIP save: lj_gs2_save is the W1 stub (gs2 track)\n");
    lj_pe_set_u8(ed, lj_pe_field_find("cbUseGM"), 0, 1);   /* dirty: Cancel asks first */
    click(in(w, "bCancel"));
    CHECK(strcmp(msg_line("Label1"), "Any unapplied profile changes will be lost ?") == 0);
    msg_answer(0);
    CHECK(lj_profile_editor_get() != NULL);
    click(in(w, "bCancel"));
    msg_answer(1);
    CHECK(lj_profile_editor_get() == NULL);
    (void)sh;
}

int lj_test_profile_editor_form(int argc, char **argv);
int lj_test_profile_editor_form(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_library lib;
    lj_show *keep = ctx->show;
    lj_library *keep_lib = ctx->lib;
    int a;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, on_action, NULL);
    test_tab_captions();
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "Lib\n"); return 1; }
    ctx->lib = &lib;
    ctx->show = lj_show_new();
    patch(ctx->show, 0, 56, 1);                                /* Generic DMX Dimmer 4 Channel */
    patch(ctx->show, 1, 12, 10);                               /* Martin MAC 500 Mode 2/4 */
    patch(ctx->show, 2, 46, 40);                               /* User Definable Generic Fixture 2 */
    patch(ctx->show, 3, 12, 80);
    test_channel_config(ctx->show);
    test_customize(ctx->show);
    test_profile_editor(ctx->show);
    {   /* F42: the blank Fixture Configuration locate button reads "Martin" (text, no logo; D2) */
        lj_window *fc = lj_win_open(LJ_WIN_FIXTURE_CONFIG);
        const char *cap = fc ? lj_w_caption(lj_window_find(fc, "sbMartin")) : NULL;
        CHECK(cap && strcmp(cap, "Martin") == 0);
        lj_win_close(LJ_WIN_FIXTURE_CONFIG);
    }
    CHECK(binding_complaints == 0);                            /* ME-16 both ways */
    lj_warnings_close_all();
    lj_show_free(ctx->show);
    ctx->show = keep; ctx->lib = keep_lib;
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, NULL, NULL);
    lj_log_set_sink(NULL, NULL);
    printf("test_profile_editor_form: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
