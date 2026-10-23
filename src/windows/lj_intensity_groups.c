/* lj_intensity_groups.c - see lj_intensity_groups.h. TINTENSITYGROUPFORM bound (ME-16). */
#include "windows/lj_intensity_groups.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_warnings.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

static const char *const lj_ig_modes[] = { "Master Proportional", "Master LoTP", "Master HTP", "Master no Effect" };   /* exe 0x23BCBC.., the Master Intensity combo's items = enum lj_master_mode */

static struct {
    lj_window *win;
    uint8_t grp[LJ_FIXTURES]; char name[LJ_SUBMASTERS][6]; uint8_t mode[LJ_SUBMASTERS];   /* the working copy */
    char text[LJ_SUBMASTERS + 1][LJ_FIXTURES][64]; const char *items[LJ_SUBMASTERS + 1][LJ_FIXTURES]; int16_t fx[LJ_SUBMASTERS + 1][LJ_FIXTURES];
    int tab, modified, filling;
} ig;

lj_window *lj_intensity_groups_window(void) { return ig.win; }
static lj_widget *find(const char *n) { return lj_window_find(ig.win, n); }
static lj_widget *findn(const char *stem, int k) { char n[32]; snprintf(n, sizeof n, "%s%d", stem, k); return find(n); }
int lj_intensity_groups_group_of(int f) { return ig.win && f >= 0 && f < LJ_FIXTURES ? ig.grp[f] : -1; }

void lj_intensity_groups_refresh(void)
{
    const lj_show *sh = lj_ctx_get()->show;
    int f, g, n[LJ_SUBMASTERS + 1];
    if (!ig.win || !sh) return;
    memset(n, 0, sizeof n);
    for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        char row[64];
        int k;
        if (!sh->fixture[f].link) continue;
        p = lj_fixlib_by_id(sh->fixture[f].profile_id);
        if (snprintf(row, sizeof row, "%d\t%s\t%d:%03d\t%s", f + 1, p ? p->name : sh->fixture[f].gs2_file, sh->fixture[f].link, sh->fixture[f].address, sh->fixture[f].user_name) < 0) row[0] = 0;
        for (k = 0; k <= LJ_SUBMASTERS; k++) {
            if (k && ig.grp[f] != k) continue;
            if (k == 0) { if (snprintf(ig.text[0][n[0]], sizeof ig.text[0][0], "%s\t%d", row, ig.grp[f]) < 0) ig.text[0][n[0]][0] = 0; }   /* Fixtures tab: + Group */
            else lj_str_copy(ig.text[k][n[k]], sizeof ig.text[0][0], row);
            ig.items[k][n[k]] = ig.text[k][n[k]]; ig.fx[k][n[k]] = (int16_t)f; n[k]++;
        }
    }
    lj_w_listview_set_items(find("lvFixtures"), ig.items[0], n[0]);
    for (g = 1; g <= LJ_SUBMASTERS; g++) lj_w_listview_set_items(findn("ListView", g), ig.items[g], n[g]);
}

static void enable_buttons(void)
{
    static const char *const b[3] = { "BAssignSelected", "bAssignMaster", "bRemove" };
    int i;
    for (i = 0; i < 3; i++) if (find(b[i])) find(b[i])->enabled = ig.tab > 0;
}

int lj_intensity_groups_tab(int tab)
{
    if (!ig.win || tab < 0 || tab > LJ_SUBMASTERS) return -1;
    if (find("PG") && ((lj_w_tabs *)find("PG"))->active != tab) lj_w_tabs_select(find("PG"), tab);   /* fires PGChange */
    ig.tab = tab;
    enable_buttons();
    return 0;
}

int lj_intensity_groups_assign(int master_only)
{
    const lj_show *sh = lj_ctx_get()->show;
    int f, n = 0;
    if (!ig.win || !sh || ig.tab < 1) return -1;
    for (f = 0; f < LJ_FIXTURES; f++) {
        if (sh->fixture[f].link && (master_only ? f == lj_sel_master() : lj_sel_is_selected(f))) { ig.grp[f] = (uint8_t)ig.tab; ig.modified = 1; }
        n += ig.grp[f] == ig.tab;
    }
    lj_intensity_groups_refresh();
    return n;
}

int lj_intensity_groups_remove(int fixture)
{
    if (!ig.win || fixture < 0 || fixture >= LJ_FIXTURES) return -1;
    ig.grp[fixture] = 0; ig.modified = 1;
    lj_intensity_groups_refresh();
    return 0;
}

int lj_intensity_groups_save(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *sh = ctx->show;
    int f, g;
    if (!ig.win || !sh) return -1;
    for (g = 1; g <= LJ_SUBMASTERS; g++) {
        lj_widget *cb = findn("ComboBox", g);
        if (cb && ((lj_w_combobox *)cb)->index >= 0) ig.mode[g - 1] = (uint8_t)((lj_w_combobox *)cb)->index;
    }
    lj_lock();
    for (f = 0; f < LJ_FIXTURES; f++) sh->fixture[f].intensity_group = ig.grp[f];
    memcpy(sh->submaster_name, ig.name, sizeof ig.name);
    memcpy(sh->submaster_mode, ig.mode, sizeof ig.mode);
    if (ctx->engine) lj_engine_rebind(ctx->engine);   /* S3 reads the sub-master of each fixture from the post ctx */
    lj_unlock();
    if (ctx->lib && lj_show_save_config(sh, ctx->lib) != 0) lj_log("intensity groups: cannot write Config.dat");
    ig.modified = 0;
    return 0;
}

static void load_working(void)
{
    const lj_show *sh = lj_ctx_get()->show;
    int f, g;
    lj_widget *w;
    memset(ig.grp, 0, sizeof ig.grp); memset(ig.mode, 0, sizeof ig.mode);
    for (g = 0; g < LJ_SUBMASTERS; g++) snprintf(ig.name[g], sizeof ig.name[g], "%d", g + 1);   /* PROPOSAL rig: the unnamed group shows its number, as the Master Intensity captions "4..8" */
    if (sh) {
        for (f = 0; f < LJ_FIXTURES; f++) ig.grp[f] = sh->fixture[f].link && sh->fixture[f].intensity_group <= LJ_SUBMASTERS ? sh->fixture[f].intensity_group : 0;
        for (g = 0; g < LJ_SUBMASTERS; g++) { if (sh->submaster_name[g][0]) lj_str_copy(ig.name[g], sizeof ig.name[g], sh->submaster_name[g]); ig.mode[g] = sh->submaster_mode[g] < 4 ? sh->submaster_mode[g] : 0; }
    }
    ig.filling = 1;
    for (g = 1; g <= LJ_SUBMASTERS; g++) {
        if ((w = findn("Edit", g)) != NULL) lj_w_edit_set_text(w, ig.name[g - 1]);
        if ((w = findn("ComboBox", g)) != NULL) { lj_w_combobox_set_items(w, lj_ig_modes, LJ_COUNT_OF(lj_ig_modes)); ((lj_w_combobox *)w)->index = ig.mode[g - 1]; }
    }
    ig.filling = 0;
}

/* ---- handlers ---- */
static void on_pg(lj_widget *w, void *ud) { (void)ud; ig.tab = ((lj_w_tabs *)w)->active; enable_buttons(); }
static void on_name(lj_widget *w, void *ud)
{
    int g;
    (void)ud;
    if (ig.filling) return;
    for (g = 1; g <= LJ_SUBMASTERS; g++) if (findn("Edit", g) == w) { lj_str_copy(ig.name[g - 1], sizeof ig.name[0], lj_w_edit_text(w)); ig.modified = 1; }
}
static void on_assign_sel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_intensity_groups_assign(0); }
static void on_assign_master(lj_widget *w, void *ud) { (void)w; (void)ud; lj_intensity_groups_assign(1); }
static void on_remove(lj_widget *w, void *ud)
{
    lj_widget *lv = ig.tab > 0 ? findn("ListView", ig.tab) : NULL;
    int i = lv ? lj_w_listview_index(lv) : -1;
    (void)w; (void)ud;
    if (i >= 0 && i < ((lj_w_listview *)lv)->n) lj_intensity_groups_remove(ig.fx[ig.tab][i]);   /* ponytail: the list's one selected row; MultiSelect rows need a multi-select listview */
}
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_intensity_groups_save(); }
static void clear_answer(int yes, void *ud) { (void)ud; if (yes && ig.win) { memset(ig.grp, 0, sizeof ig.grp); ig.modified = 1; lj_intensity_groups_refresh(); } }
static void on_clear_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", "Clear All Groups, are you sure ?", NULL, clear_answer, NULL); }     /* exe 0x1BC024 */
static void default_answer(int yes, void *ud)   /* PROPOSAL rig: "Default Setup" = no fixture in a group, names 1..8, every mode Proportional (LJ2's defaults are not documented) */
{
    (void)ud;
    if (!yes || !ig.win) return;
    memset(ig.grp, 0, sizeof ig.grp); memset(ig.mode, 0, sizeof ig.mode);
    {
        int g;
        ig.filling = 1;
        for (g = 0; g < LJ_SUBMASTERS; g++) {
            snprintf(ig.name[g], sizeof ig.name[g], "%d", g + 1);
            lj_w_edit_set_text(findn("Edit", g + 1), ig.name[g]);
            if (findn("ComboBox", g + 1)) ((lj_w_combobox *)findn("ComboBox", g + 1))->index = 0;
        }
        ig.filling = 0;
    }
    ig.modified = 1;
    lj_intensity_groups_refresh();
}
static void on_default(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", "Default Setup, are you sure ?", NULL, default_answer, NULL); }  /* exe 0x1BC07C */
static void on_help(lj_widget *w, void *ud) { (void)ud; lj_help_viewer_click(w, (void *)"controls"); }   /* W4 acceptance: the bundled viewer */
static void close_answer(int yes, void *ud) { (void)ud; if (yes) lj_intensity_groups_save(); ig.modified = 0; lj_intensity_groups_close(); }
static void on_close(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (ig.modified) lj_win_confirm("Please Confirm..", "Changes were made to Intensity Groups", "Do you wish to save changes ?", close_answer, NULL);   /* exe 0x1BB24C */
    else lj_intensity_groups_close();
}

static const lj_binding lj_ig_bindings[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "PGChange", on_pg, NULL }, { "ListViewChange", lj_handler_noop, NULL }, { "NameChange", on_name, NULL },
    { "BAssignSelectedClick", on_assign_sel, NULL }, { "bAssignMasterClick", on_assign_master, NULL }, { "bRemoveClick", on_remove, NULL },
    { "bSaveClick", on_save, NULL }, { "DefaultSetup1Click", on_default, NULL }, { "ClearAll1Click", on_clear_all, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_intensity_groups_open(void)
{
    if (ig.win) return ig.win;
    ig.win = lj_window_create(&lj_form_TINTENSITYGROUPFORM, lj_ig_bindings);
    if (!ig.win) { lj_log("intensity groups: cannot create TIntensityGroupForm"); return NULL; }
    load_working();
    ig.modified = 0;
    ig.tab = find("PG") ? ((lj_w_tabs *)find("PG"))->active : 0;   /* the DFM's ActivePage = TabSheet4 */
    enable_buttons();
    lj_intensity_groups_refresh();
    lj_win_show(ig.win, 0);
    return ig.win;
}

void lj_intensity_groups_close(void)
{
    if (!ig.win) return;
    lj_win_hide(ig.win);
    lj_window_destroy(ig.win);
    ig.win = NULL; ig.modified = 0;
}
