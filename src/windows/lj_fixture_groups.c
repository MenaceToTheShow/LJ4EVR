/* lj_fixture_groups.c - see lj_fixture_groups.h. Code-built (design 7.7); captions are the exe's strings
 * (13 catalogue 0x357174..0x359B4C); every size is PROPOSAL rig (fixturegroupwindow.gif is reduced, 07 s14). */
#include "windows/lj_fixture_groups.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_fn.h"
#include "model/lj_groups.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_warnings.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

/* ---- the code-built form: 220x250 outer (forms.ini [GROUPS] default, 01 s3.4) = 212x204 client under a sizeable frame + menu ---- */
static const lj_ctl_desc lj_fg_panel[] = {
    { LJ_K_BUTTON, "bDeselectAll", 2, 2, 103, 20, .caption = "De-select All", .tab_order = 0, .font_inherited = 1, .events = { { "OnClick", "bDeselectAllClick" } } },
    { LJ_K_BUTTON, "bSelectAll", 107, 2, 103, 20, .caption = "Select All", .tab_order = 1, .font_inherited = 1, .events = { { "OnClick", "bSelectAllClick" } } },
    { LJ_K_BUTTON, "bMode", 2, 23, 103, 20, .caption = "Mode:Inclusive", .hint = "Selection Mode", .show_hint = LJ_YES, .tab_order = 2, .font_inherited = 1, .events = { { "OnClick", "bModeClick" } } },
    { LJ_K_BUTTON, "bFilter", 107, 23, 103, 20, .caption = "Filter:All", .hint = "Group Selection Filter", .show_hint = LJ_YES, .tab_order = 3, .font_inherited = 1, .events = { { "OnClick", "bFilterClick" } } },
};
static const lj_ctl_desc lj_fg_ctls[] = {
    { LJ_K_TREEVIEW, "tvGroups", 0, 0, 212, 159, .align = LJ_AL_CLIENT, .show_root = LJ_YES, .show_lines = LJ_NO, .hide_selection = LJ_NO, .readonly = LJ_YES, .tab_order = 0, .font_inherited = 1, .events = { { "OnChange", "tvGroupsChange" } } },
    { LJ_K_PANEL, "pButtons", 0, 159, 212, 45, .align = LJ_AL_BOTTOM, .bevel_outer = LJ_BV_NONE, .tab_order = 1, .font_inherited = 1, .children = lj_fg_panel, .nchildren = LJ_COUNT_OF(lj_fg_panel) },
};
static const lj_menu_item_desc lj_fg_prefs[] = {
    { .name = "M_ShowSystem", .caption = "&Show System Groups", .on_click = "PrefsClick", .image_index = -1 },
    { .name = "M_ShowUser", .caption = "Show &User Names", .on_click = "PrefsClick", .image_index = -1 },
};
static const lj_menu_item_desc lj_fg_main[] = {
    { .name = "M_Preferences", .caption = "&Preferences", .image_index = -1, .children = lj_fg_prefs, .nchildren = LJ_COUNT_OF(lj_fg_prefs) },
    { .name = "M_AddGroup", .caption = "&Add Group", .on_click = "AddGroupClick", .image_index = -1 },
    { .name = "Help1", .caption = "&Help", .on_click = "Help1Click", .image_index = -1 },
};
static const lj_menu_item_desc lj_fg_pop[] = {
    { .name = "P_Add", .caption = "Add &Selected Fixtures to this Group", .on_click = "PopClick", .image_index = -1 },
    { .name = "P_Remove", .caption = "&Remove Selected Fixtures from this Group", .on_click = "PopClick", .image_index = -1 },
    { .name = "N1", .caption = "-", .image_index = -1 },
    { .name = "P_RemoveAll", .caption = "Remove All Fixtures from this Group", .on_click = "PopClick", .image_index = -1 },
    { .name = "P_RemoveFromAll", .caption = "Remove Selected Fixtures from all Groups", .on_click = "PopClick", .image_index = -1 },
    { .name = "N2", .caption = "-", .image_index = -1 },
    { .name = "P_Delete", .caption = "Delete this group", .on_click = "PopClick", .image_index = -1 },
    { .name = "P_Rename", .caption = "Rename this group", .on_click = "PopClick", .image_index = -1 },
};
static const lj_menu_desc lj_fg_menus[] = { { "MainMenu1", 1, lj_fg_main, LJ_COUNT_OF(lj_fg_main) }, { "GroupPop", 0, lj_fg_pop, LJ_COUNT_OF(lj_fg_pop) } };
static const lj_form_desc lj_form_GROUPS = {
    .name = "GroupForm", .class_name = "TGroupForm", .caption = "Fixture Groups",
    .client_width = 212, .client_height = 204, .color = LJ_CLR(0xC0C0C0) /* clBtnFace */, .style = LJ_FS_STAYONTOP, .border = LJ_BS_SIZEABLE,
    .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .position = LJ_PO_DESIGNED, .text_height = 13, .font = LJ_FONT_SANS13,
    .font_name = "MS Sans Serif", .font_height = -11, .min_width = 220, .min_height = 150, .menu = "MainMenu1", .menus = lj_fg_menus, .nmenus = LJ_COUNT_OF(lj_fg_menus),
    .ctls = lj_fg_ctls, .n = LJ_COUNT_OF(lj_fg_ctls), .events = { { "OnClose", "FormClose" } }
};

#define LJ_FG_ROWS (2 * LJ_FIXTURES + LJ_FIXTURES + LJ_USER_GROUPS + LJ_USER_GROUPS * 4)   /* ponytail: a fixture in many user groups beyond this is not listed */
static struct {
    lj_window *win, *add;
    char text[LJ_FG_ROWS][80]; const char *items[LJ_FG_ROWS]; int16_t grp[LJ_FG_ROWS], fx[LJ_FG_ROWS]; int n;   /* grp: user group 1..99, -(profile id + 1) for a system group, 0 none; fx: fixture or -1 */
    int filter, rename;     /* 0 All 1 Odd 2 Even; the user group TAddGroupForm renames (0 = Add) */
} fg;

lj_window *lj_fixture_groups_window(void) { return fg.win; }
static lj_widget *find(const char *n) { return lj_window_find(fg.win, n); }

static void save_groups(void)
{
    lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!ctx->lib || !ctx->show || lj_library_path(ctx->lib, "GroupInfo.dat", path, sizeof path) != 0) return;
    if ((ctx->htp_names ? lj_htp_names_save(path, ctx->show->user_group, ctx->htp_names) : lj_groups_save(path, ctx->show->user_group)) != 0) lj_log("fixture groups: cannot write %s", path);
}

static const char *fixture_label(const lj_show *sh, int f, char *buf, size_t cap)
{
    const lj_profile *p = lj_fixlib_by_id(sh->fixture[f].profile_id);
    snprintf(buf, cap, "\t(%d) %s", f + 1, lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowUserNames", 0) || !p ? sh->fixture[f].user_name : p->name);
    return buf;
}

static void add_row(const char *text, int grp, int fx)
{
    if (fg.n >= LJ_FG_ROWS) return;
    lj_str_copy(fg.text[fg.n], sizeof fg.text[0], text);
    fg.items[fg.n] = fg.text[fg.n]; fg.grp[fg.n] = (int16_t)grp; fg.fx[fg.n] = (int16_t)fx; fg.n++;
}

void lj_fixture_groups_refresh(void)
{
    const lj_show *sh = lj_ctx_get()->show;
    lj_widget *tv = find("tvGroups");
    char buf[80];
    int f, g, i;
    if (!fg.win || !sh) return;
    fg.n = 0;
    if (lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowSystem", 1)) for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        if (!sh->fixture[f].link) continue;
        for (i = 0; i < f; i++) if (sh->fixture[i].link && sh->fixture[i].profile_id == sh->fixture[f].profile_id) break;
        if (i < f) continue;                                                        /* one system group per fixture type */
        p = lj_fixlib_by_id(sh->fixture[f].profile_id);
        if (snprintf(buf, sizeof buf, "(System) %s%s%s", p ? p->manufacturer : "", p ? " " : "", p ? p->name : sh->fixture[f].gs2_file) < 0) buf[0] = 0;   /* exe "(System) " 0x357174 */
        add_row(buf, -(int)sh->fixture[f].profile_id - 1, -1);
        for (i = f; i < LJ_FIXTURES; i++) if (sh->fixture[i].link && sh->fixture[i].profile_id == sh->fixture[f].profile_id) add_row(fixture_label(sh, i, buf, sizeof buf), -(int)sh->fixture[f].profile_id - 1, i);
    }
    for (g = 0; g < LJ_USER_GROUPS; g++) {
        if (!sh->user_group[g].name[0]) continue;
        snprintf(buf, sizeof buf, "(%d) %s", g + 1, sh->user_group[g].name);
        add_row(buf, g + 1, -1);
        for (f = 0; f < LJ_FIXTURES; f++) if (sh->user_group[g].member[f] && sh->fixture[f].link) add_row(fixture_label(sh, f, buf, sizeof buf), g + 1, f);
    }
    if (tv) lj_w_treeview_set_items(tv, fg.items, fg.n);
    lj_w_set_caption(find("bMode"), lj_sel_mode() == LJ_SEL_EXCLUSIVE ? "Mode:Exclusive" : "Mode:Inclusive");
    lj_w_set_caption(find("bFilter"), fg.filter == 1 ? "Filter:Odd" : fg.filter == 2 ? "Filter:Even" : "Filter:All");
    lj_menu_set_checked(lj_window_menus(fg.win), "M_ShowSystem", lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowSystem", 1));
    lj_menu_set_checked(lj_window_menus(fg.win), "M_ShowUser", lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowUserNames", 0));
}

static int passes(int f) { return fg.filter == 0 || (fg.filter == 1) == ((f + 1) % 2 == 1); }   /* odd/even by fixture number [RN 2.2] */

static int select_members(const uint8_t *member, int profile_id)   /* member[] or every fixture of profile_id (system group) */
{
    const lj_show *sh = lj_ctx_get()->show;
    int f, n = 0, first = 1;
    if (!sh) return -1;
    if (lj_sel_mode() == LJ_SEL_EXCLUSIVE) lj_sel_none();
    for (f = 0; f < LJ_FIXTURES; f++) {
        int in = member ? member[f] != 0 : sh->fixture[f].profile_id == profile_id;
        if (!sh->fixture[f].link || !in || !passes(f)) continue;
        lj_sel_set(f, 1, first && lj_sel_mode() == LJ_SEL_EXCLUSIVE);
        first = 0; n++;
    }
    lj_desktop_icons_refresh();
    return n;
}

int lj_fixture_groups_select(int group)
{
    const lj_show *sh = lj_ctx_get()->show;
    if (!sh || group < 1 || group > LJ_USER_GROUPS || !sh->user_group[group - 1].name[0]) return -1;
    return select_members(sh->user_group[group - 1].member, -1);
}

int lj_fixture_groups_add(const char *name)
{
    lj_show *sh = lj_ctx_get()->show;
    int g;
    if (!sh || !name || !name[0]) return -1;
    for (g = 0; g < LJ_USER_GROUPS && sh->user_group[g].name[0]; g++) { }
    if (g == LJ_USER_GROUPS) { lj_log("fixture groups: all %d user groups used", LJ_USER_GROUPS); return -1; }   /* max 99 [04 s2.3] */
    lj_lock();
    memset(&sh->user_group[g], 0, sizeof sh->user_group[g]);
    lj_str_copy(sh->user_group[g].name, sizeof sh->user_group[g].name, name);
    lj_unlock();
    save_groups();
    lj_fixture_groups_refresh();
    return g + 1;
}

int lj_fixture_groups_assign(int group, int on)
{
    lj_show *sh = lj_ctx_get()->show;
    int f, n = 0;
    if (!sh || group < 1 || group > LJ_USER_GROUPS || !sh->user_group[group - 1].name[0]) return -1;
    lj_lock();
    for (f = 0; f < LJ_FIXTURES; f++) {
        if (sh->fixture[f].link && lj_sel_is_selected(f)) sh->user_group[group - 1].member[f] = (uint8_t)(on != 0);
        n += sh->user_group[group - 1].member[f] != 0;
    }
    lj_unlock();
    save_groups();
    lj_fixture_groups_refresh();
    return n;
}

static int lj_fg_pending;
static void delete_answer(int yes, void *ud) { lj_show *sh = lj_ctx_get()->show; (void)ud; if (yes && sh) { lj_lock(); memset(&sh->user_group[lj_fg_pending - 1], 0, sizeof sh->user_group[0]); lj_unlock(); save_groups(); lj_fixture_groups_refresh(); } }
static void clear_answer(int yes, void *ud) { lj_show *sh = lj_ctx_get()->show; (void)ud; if (yes && sh) { lj_lock(); memset(sh->user_group[lj_fg_pending - 1].member, 0, LJ_FIXTURES); lj_unlock(); save_groups(); lj_fixture_groups_refresh(); } }

static void add_dialog(int rename);
int lj_fixture_groups_popup(const char *item, int group)
{
    lj_show *sh = lj_ctx_get()->show;
    int g, f;
    if (!sh || !item || group < 1 || group > LJ_USER_GROUPS || !sh->user_group[group - 1].name[0]) return -1;
    lj_fg_pending = group;
    if (strcmp(item, "P_Add") == 0) lj_fixture_groups_assign(group, 1);
    else if (strcmp(item, "P_Remove") == 0) lj_fixture_groups_assign(group, 0);
    else if (strcmp(item, "P_RemoveAll") == 0) lj_win_confirm("Please Confirm..", "Clear Group ?", NULL, clear_answer, NULL);          /* exe 0x1E7ED0 */
    else if (strcmp(item, "P_RemoveFromAll") == 0) {
        lj_lock();
        for (g = 0; g < LJ_USER_GROUPS; g++) for (f = 0; f < LJ_FIXTURES; f++) if (lj_sel_is_selected(f)) sh->user_group[g].member[f] = 0;
        lj_unlock();
        save_groups(); lj_fixture_groups_refresh();
    }
    else if (strcmp(item, "P_Delete") == 0) lj_win_confirm("Please Confirm..", "Delete Selected Group ?", NULL, delete_answer, NULL);  /* exe 0x1E7E58 */
    else if (strcmp(item, "P_Rename") == 0) add_dialog(group);
    else return -1;
    return 0;
}

void lj_fixture_groups_set_filter(int filter) { fg.filter = filter >= 0 && filter <= 2 ? filter : 0; if (fg.win) lj_fixture_groups_refresh(); }

/* ---- TAddGroupForm: "Add Fixture Group" / "Rename Fixture Group" (exe 0x359588) ---- */
static void add_close(void) { if (fg.add) { lj_win_hide(fg.add); lj_window_destroy(fg.add); fg.add = NULL; } }
static void add_ok(lj_widget *w, int ev, void *ud)
{
    lj_show *sh = lj_ctx_get()->show;
    const char *name;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK || !fg.add) return;
    name = lj_w_edit_text(lj_window_find(fg.add, "eName"));
    if (name && name[0]) {
        if (!fg.rename) lj_fixture_groups_add(name);
        else if (sh) { lj_lock(); lj_str_copy(sh->user_group[fg.rename - 1].name, sizeof sh->user_group[0].name, name); lj_unlock(); save_groups(); lj_fixture_groups_refresh(); }
    }
    add_close();
}
static void add_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) add_close(); }
static const lj_binding lj_fg_add_bindings[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static void add_dialog(int rename)
{
    const lj_show *sh = lj_ctx_get()->show;
    lj_widget *w;
    add_close();
    fg.add = lj_window_create(&lj_form_TADDGROUPFORM, lj_fg_add_bindings);
    if (!fg.add) return;
    fg.rename = rename;
    if (rename) { lj_window_set_caption(fg.add, "Rename Fixture Group"); if (sh) lj_w_edit_set_text(lj_window_find(fg.add, "eName"), sh->user_group[rename - 1].name); }
    if ((w = lj_window_find(fg.add, "BitBtn1")) != NULL) { w->on_event = add_ok; w->ud = NULL; }       /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((w = lj_window_find(fg.add, "BitBtn2")) != NULL) { w->on_event = add_cancel; w->ud = NULL; }
    lj_win_show(fg.add, 0);
}

/* ---- the group window's handlers ---- */
static int cur_index(void) { lj_widget *tv = find("tvGroups"); return tv ? ((lj_w_treeview *)tv)->index : -1; }
static void on_change(lj_widget *w, void *ud)
{
    const lj_show *sh = lj_ctx_get()->show;
    int i = cur_index();
    (void)w; (void)ud;
    if (i < 0 || i >= fg.n || !sh) return;
    if (fg.fx[i] >= 0) lj_sel_click(fg.fx[i], 0, 0);                                    /* a fixture line: the desktop click rule */
    else if (fg.grp[i] > 0) lj_fixture_groups_select(fg.grp[i]);
    else if (fg.grp[i] < 0) select_members(NULL, -fg.grp[i] - 1);
    lj_desktop_icons_refresh();
}
static void on_deselect(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sel_none(); lj_desktop_icons_refresh(); }
static void on_select_all(lj_widget *w, void *ud)
{
    const lj_show *sh = lj_ctx_get()->show;
    int f;
    (void)w; (void)ud;
    for (f = 0; sh && f < LJ_FIXTURES; f++) if (sh->fixture[f].link && passes(f)) lj_sel_set(f, 1, 0);   /* the filter applies to select and deselect through the dialog [RN 2.2] */
    lj_desktop_icons_refresh();
}
static void on_mode(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sel_mode_set(lj_sel_mode() == LJ_SEL_EXCLUSIVE ? LJ_SEL_INCLUSIVE : LJ_SEL_EXCLUSIVE); lj_fixture_groups_refresh(); }
static void on_filter(lj_widget *w, void *ud) { (void)w; (void)ud; lj_fixture_groups_set_filter((fg.filter + 1) % 3); }
static void on_prefs(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    const lj_menu_item_desc *it = fg.win ? lj_window_menu_item(fg.win) : NULL;
    (void)w; (void)ud;
    if (!it || !ctx->forms_ini) return;
    if (strcmp(it->name, "M_ShowSystem") == 0) lj_ini_seti(ctx->forms_ini, "GROUPS", "ShowSystem", !lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowSystem", 1));
    else lj_ini_seti(ctx->forms_ini, "GROUPS", "ShowUserNames", !lj_ini_geti(lj_ctx_get()->forms_ini, "GROUPS", "ShowUserNames", 0));
    lj_fixture_groups_refresh();
}
static void on_add_group(lj_widget *w, void *ud) { (void)w; (void)ud; add_dialog(0); }
static void on_pop(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = fg.win ? lj_window_menu_item(fg.win) : NULL;
    int i = cur_index();
    (void)w; (void)ud;
    if (it && i >= 0 && i < fg.n && fg.grp[i] > 0) lj_fixture_groups_popup(it->name, fg.grp[i]);
}
static void on_help(lj_widget *w, void *ud) { (void)ud; lj_help_viewer_click(w, (void *)"controls"); }   /* W4 acceptance: the bundled viewer */
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_fixture_groups_close(); }
static const lj_binding lj_fg_bindings[] = {
    { "FormClose", on_close, NULL }, { "tvGroupsChange", on_change, NULL }, { "bDeselectAllClick", on_deselect, NULL }, { "bSelectAllClick", on_select_all, NULL },
    { "bModeClick", on_mode, NULL }, { "bFilterClick", on_filter, NULL }, { "PrefsClick", on_prefs, NULL }, { "AddGroupClick", on_add_group, NULL },
    { "PopClick", on_pop, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };

/* right-click on a user-group line: select it, then the fixturegrouppopup.gif menu */
static lj_widget_vt lj_fg_tv_vt;
static int (*lj_fg_tv_mouse)(lj_widget *, const lj_mouse_ev *);
static int tv_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    if (m->type == LJ_EV_MOUSE_DOWN && m->button == 3) {
        lj_w_treeview *t = (lj_w_treeview *)w;
        int item = lj_w_treeview_visible_item(w, t->sb.pos + (m->y - 2) / (t->row_h ? t->row_h : 16));   /* 2 = the tree's border */
        lj_menu *mn = lj_window_menus(fg.win);
        const lj_menu_desc *d = lj_form_find_menu(lj_window_desc(fg.win), "GroupPop");
        if (item >= 0 && item < fg.n && fg.grp[item] > 0 && mn && d) {
            int ax, ay, sx = 0, sy = 0;
            lj_rect cr;
            t->index = item;
            lj_widget_abs(w, &ax, &ay);
            lj_window_client_rect(fg.win, &cr);
            if (mn->host && mn->host->screen_pos) mn->host->screen_pos(mn->host->ud, fg.win, &sx, &sy);
            lj_menu_popup(mn, d, sx + cr.x + ax + m->x, sy + cr.y + ay + m->y);
        }
        return 1;
    }
    return lj_fg_tv_mouse ? lj_fg_tv_mouse(w, m) : 0;
}

lj_window *lj_fixture_groups_open(void)
{
    lj_widget *tv;
    if (fg.win) return fg.win;
    fg.win = lj_window_create(&lj_form_GROUPS, lj_fg_bindings);
    if (!fg.win) { lj_log("fixture groups: cannot create the window"); return NULL; }
    if ((tv = find("tvGroups")) != NULL) { lj_fg_tv_vt = *tv->vt; lj_fg_tv_mouse = tv->vt->mouse; lj_fg_tv_vt.mouse = tv_mouse; tv->vt = &lj_fg_tv_vt; }
    lj_fixture_groups_refresh();
    lj_win_show(fg.win, 0);
    return fg.win;
}

void lj_fixture_groups_close(void)
{
    add_close();
    if (!fg.win) return;
    lj_win_hide(fg.win);
    lj_window_destroy(fg.win);
    fg.win = NULL;
}

/* ---- hotkeys [01 s3.4: user groups 1-20 since 2.6] ---- */
static void fn_group(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn == LJ_FN_TOGGLE_GROUP_VIEW) lj_win_toggle(LJ_WIN_FIXTURE_GROUPS);
    else lj_fixture_groups_select((int)(fn - LJ_FN_FIXTURE_GROUP_1) + 1);
}
void lj_fixture_groups_bind_fns(void)
{
    int i;
    for (i = 0; i < 20; i++) lj_fn_bind((enum lj_fn)(LJ_FN_FIXTURE_GROUP_1 + i), fn_group, NULL);
    lj_fn_bind(LJ_FN_TOGGLE_GROUP_VIEW, fn_group, NULL);
}
