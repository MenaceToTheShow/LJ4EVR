/* lj_htp_groups.c - see lj_htp_groups.h. */
#include "windows/lj_htp_groups.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_fn.h"
#include "model/lj_groups.h"
#include "model/lj_patch.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_global_patch.h"

/* ---- the code-built form (PROPOSAL rig: no DFM, no GIF; sizes follow the Fixture Groups window) ---- */
static const lj_column_desc lj_hg_cols[] = { { .caption = "No", .width = 30, .image_index = -1 }, { .caption = "Name", .width = 150, .image_index = -1 }, { .caption = "Channels", .width = 60, .image_index = -1 } };
static const lj_ctl_desc lj_hg_panel[] = {
    { LJ_K_EDIT, "eName", 4, 4, 160, 21, .max_length = LJ_NAME_CAP - 1, .tab_order = 0, .font_inherited = 1 },
    { LJ_K_BUTTON, "bRename", 168, 3, 92, 23, .caption = "Rename", .tab_order = 1, .font_inherited = 1, .events = { { "OnClick", "bRenameClick" } } },
    { LJ_K_BUTTON, "bAdd", 4, 30, 128, 23, .caption = "Add Fixtures", .tab_order = 2, .font_inherited = 1, .events = { { "OnClick", "bAddClick" } } },
    { LJ_K_BUTTON, "bRemove", 134, 30, 126, 23, .caption = "Remove Fixtures", .tab_order = 3, .font_inherited = 1, .events = { { "OnClick", "bRemoveClick" } } },
    { LJ_K_BUTTON, "bClear", 4, 56, 128, 23, .caption = "Clear Group", .tab_order = 4, .font_inherited = 1, .events = { { "OnClick", "bClearClick" } } },
    { LJ_K_CHECKBOX, "cbMasters", 4, 84, 256, 17, .caption = "Enable HTP group master intensity control", .tab_order = 5, .font_inherited = 1, .events = { { "OnClick", "cbMastersClick" } } },
};
static const lj_ctl_desc lj_hg_ctls[] = {
    { LJ_K_LISTVIEW, "lvGroups", 0, 0, 264, 196, .align = LJ_AL_CLIENT, .columns = lj_hg_cols, .ncolumns = LJ_COUNT_OF(lj_hg_cols), .grid_lines = LJ_YES, .readonly = LJ_YES, .row_select = LJ_YES, .hide_selection = LJ_NO, .view_style = "vsReport", .tab_order = 0, .font_inherited = 1 },
    { LJ_K_PANEL, "pBottom", 0, 196, 264, 106, .align = LJ_AL_BOTTOM, .tab_order = 1, .font_inherited = 1, .children = lj_hg_panel, .nchildren = LJ_COUNT_OF(lj_hg_panel) },
};
static const lj_menu_item_desc lj_hg_menu[] = { { .name = "Help1", .caption = "&Help", .on_click = "Help1Click", .image_index = -1 } };
static const lj_menu_desc lj_hg_menus[] = { { "MainMenu1", 1, lj_hg_menu, LJ_COUNT_OF(lj_hg_menu) } };
static const lj_form_desc lj_form_HTPGROUPS = {
    .name = "HTPGroupsForm", .class_name = "THTPGroupsForm", .caption = "HTP Groups",
    .client_width = 264, .client_height = 302, .color = LJ_CLR(0xC0C0C0) /* clBtnFace */, .style = LJ_FS_STAYONTOP, .border = LJ_BS_SINGLE,
    .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .position = LJ_PO_DESIGNED, .left = 300, .top = 200, .text_height = 13, .font = LJ_FONT_SANS13,
    .font_name = "MS Sans Serif", .font_height = -11, .menu = "MainMenu1", .menus = lj_hg_menus, .nmenus = LJ_COUNT_OF(lj_hg_menus),
    .ctls = lj_hg_ctls, .n = LJ_COUNT_OF(lj_hg_ctls), .events = { { "OnClose", "FormClose" } }
};

static struct { lj_window *win; char row[LJ_HTP_GROUPS][64]; const char *rows[LJ_HTP_GROUPS]; int sel; } hg;
static uint8_t lj_hg_level[LJ_HTP_GROUPS + 1];   /* the Toggle hotkeys' state */

lj_window *lj_htp_groups_window(void) { return hg.win; }
static lj_widget *find(const char *n) { return lj_window_find(hg.win, n); }
static const char *name_of(int g)
{
    static char dflt[24];
    const lj_ctx *ctx = lj_ctx_get();
    if (ctx->htp_names && ctx->htp_names->name[g - 1][0]) return ctx->htp_names->name[g - 1];
    snprintf(dflt, sizeof dflt, "HTP group %d", g);   /* PROPOSAL default (model/lj_groups.h) */
    return dflt;
}

int lj_htp_groups_count(int group)
{
    const lj_show *sh = lj_ctx_get()->show;
    int c, n = 0;
    if (!sh || group < 1 || group > LJ_HTP_GROUPS) return 0;
    for (c = 0; c < LJ_CHANS; c++) n += (sh->patch[c].htp_mask >> (group - 1)) & 1u;
    return n;
}

static void post_swap(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_cmd c;
    char path[LJ_PATH_CAP];
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_HTP_GROUPS; c.patch = ctx->show->patch; c.b = (uint32_t)lj_global_patch_htp_masters();
    lj_ctx_post(&c);
    if (ctx->lib && lj_library_path(ctx->lib, LJ_PATCH_FILE, path, sizeof path) == 0 && lj_patch_save(path, ctx->show->patch) != 0) lj_log("htp groups: cannot write %s", path);
}

int lj_htp_groups_assign(int group, int on)
{
    lj_show *sh = lj_ctx_get()->show;
    int f, o, n = 0;
    if (!sh || group < 1 || group > LJ_HTP_GROUPS) return -1;
    lj_lock();
    for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        int base = lj_show_fixture_base(sh, f);
        if (base < 0 || !lj_sel_is_selected(f) || (p = lj_fixlib_by_id(sh->fixture[f].profile_id)) == NULL) continue;
        for (o = 0; o < p->footprint && base + o < LJ_CHANS; o++)
            if ((p->channel_class[o] == LJ_CH_INTENSITY || p->ext_gm[o]) && lj_patch_htp_set(sh->patch, base + o, group, on) == 0) n++;
    }
    lj_unlock();
    post_swap();
    lj_htp_groups_refresh();
    return n;
}

int lj_htp_groups_clear(int group)
{
    lj_show *sh = lj_ctx_get()->show;
    int c;
    if (!sh || group < 1 || group > LJ_HTP_GROUPS) return -1;
    lj_lock();
    for (c = 0; c < LJ_CHANS; c++) lj_patch_htp_set(sh->patch, c, group, 0);
    lj_unlock();
    post_swap();
    lj_htp_groups_refresh();
    return 0;
}

int lj_htp_groups_rename(int group, const char *name)
{
    lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!ctx->htp_names || !ctx->show || group < 1 || group > LJ_HTP_GROUPS || !name) return -1;
    lj_str_copy(ctx->htp_names->name[group - 1], LJ_NAME_CAP, name);
    if (ctx->lib && lj_library_path(ctx->lib, "GroupInfo.dat", path, sizeof path) == 0 && lj_htp_names_save(path, ctx->show->user_group, ctx->htp_names) != 0) lj_log("htp groups: cannot write %s", path);
    lj_htp_groups_refresh();
    return 0;
}

void lj_htp_groups_refresh(void)
{
    lj_widget *lv = find("lvGroups");
    int g;
    if (!hg.win) return;
    if (lv && lj_w_listview_index(lv) != hg.sel) {   /* no OnChange on a code-built list we fill ourselves: the 100 ms hook notices the row */
        hg.sel = lj_w_listview_index(lv);
        lj_w_edit_set_text(find("eName"), hg.sel >= 0 ? name_of(hg.sel + 1) : "");
    }
    for (g = 1; g <= LJ_HTP_GROUPS; g++) {
        if (snprintf(hg.row[g - 1], sizeof hg.row[0], "%d\t%s\t%d", g, name_of(g), lj_htp_groups_count(g)) < 0) hg.row[g - 1][0] = 0;
        hg.rows[g - 1] = hg.row[g - 1];
    }
    if (lv) { int keep = hg.sel; lj_w_listview_set_items(lv, hg.rows, LJ_HTP_GROUPS); ((lj_w_listview *)lv)->index = keep; lj_widget_invalidate(lv); }
}

static int sel_group(void) { lj_widget *lv = find("lvGroups"); return lv && lj_w_listview_index(lv) >= 0 ? lj_w_listview_index(lv) + 1 : -1; }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_htp_groups_close(); }
static void on_add(lj_widget *w, void *ud) { (void)w; (void)ud; lj_htp_groups_assign(sel_group(), 1); }
static void on_remove(lj_widget *w, void *ud) { (void)w; (void)ud; lj_htp_groups_assign(sel_group(), 0); }
static void on_clear(lj_widget *w, void *ud) { (void)w; (void)ud; lj_htp_groups_clear(sel_group()); }
static void on_rename(lj_widget *w, void *ud) { (void)w; (void)ud; lj_htp_groups_rename(sel_group(), lj_w_edit_text(find("eName"))); }
static void on_masters(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    (void)ud;
    if (ctx->main_ini) lj_ini_seti(ctx->main_ini, "GLOBALPATCH", "HTPMasters", lj_w_checkbox_checked(w));
    if (ctx->show) post_swap();
}
static const lj_binding lj_hg_bindings[] = {
    { "FormClose", on_close, NULL }, { "bRenameClick", on_rename, NULL }, { "bAddClick", on_add, NULL }, { "bRemoveClick", on_remove, NULL },
    { "bClearClick", on_clear, NULL }, { "cbMastersClick", on_masters, NULL }, { "Help1Click", lj_handler_noop, NULL }, { NULL, NULL, NULL } };

lj_window *lj_htp_groups_open(void)
{
    if (hg.win) return hg.win;
    hg.win = lj_window_create(&lj_form_HTPGROUPS, lj_hg_bindings);
    if (!hg.win) { lj_log("htp groups: cannot create the window"); return NULL; }
    hg.sel = -2;   /* forces the name field to follow the first refresh */
    {   /* set without OnClick: the box mirrors the stored preference */
        lj_widget *cb = find("cbMasters");
        void (*fn)(lj_widget *, int, void *) = cb ? cb->on_event : NULL;
        if (cb) { cb->on_event = NULL; lj_w_checkbox_set_checked(cb, lj_global_patch_htp_masters()); cb->on_event = fn; }
    }
    lj_htp_groups_refresh();
    lj_win_show(hg.win, 0);
    return hg.win;
}

void lj_htp_groups_close(void)
{
    if (!hg.win) return;
    lj_win_hide(hg.win);
    lj_window_destroy(hg.win);
    hg.win = NULL; hg.sel = -1;
}

/* ---- hotkeys [01 s7, 05 s11.2]: Bump = level 255 while held, Toggle = 255 / 0 ---- */
static void fn_htp(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    lj_cmd c;
    int bump = fn >= LJ_FN_HTP_BUMP_FLASH_1 && fn <= LJ_FN_HTP_BUMP_FLASH_12;
    int g = bump ? (int)(fn - LJ_FN_HTP_BUMP_FLASH_1) + 1 : (int)(fn - LJ_FN_HTP_TOGGLE_1) + 1;
    (void)prog; (void)ud;
    if (!bump && !edge) return;
    lj_hg_level[g] = (uint8_t)(bump ? (edge ? 255 : 0) : (lj_hg_level[g] ? 0 : 255));
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_HTP_GROUP; c.a = (uint32_t)g; c.b = lj_hg_level[g];
    lj_ctx_post(&c);
}

void lj_htp_groups_bind_fns(void)
{
    int i;
    for (i = 0; i < 12; i++) { lj_fn_bind((enum lj_fn)(LJ_FN_HTP_BUMP_FLASH_1 + i), fn_htp, NULL); lj_fn_bind((enum lj_fn)(LJ_FN_HTP_TOGGLE_1 + i), fn_htp, NULL); }
}
