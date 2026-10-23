/* lj_desktop_dialogs.c - the DESKTOP window family (lj_desktop_dialogs.h, docs/w4-contracts.md section 4): the desktop popup
 * dialogs of [01 s3.5] (F13), Next/Previous Fixture Selection and its custom order [01 s3.3], Save Custom Selection (Fan Out,
 * 17 s5.2), the Auto Launch box [01 s6.2]; the four warning ids are lj_warnings.c's. Every OnXxx the DFMs declare is bound
 * (ME-16); bkOK / bkCancel buttons without an OnClick get their click here, as lj_warnings.c does. */
#include "windows/lj_desktop_dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_fixture_icons.h"
#include "desktop/lj_selection.h"
#include "fixlib/lj_profile.h"
#include "gfx/lj_draw.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_w_updown.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"

#define LJ_DD_FIRST LJ_WIN_AUTO_ARRANGE
#define LJ_DD_N     (LJ_WIN_AUTO_LAUNCH - LJ_WIN_AUTO_ARRANGE + 1)
static lj_window *dd_win[LJ_DD_N];
static const lj_desktop_dlg_host *dd_host;
static enum lj_pick dd_pending;

static int        mine(enum lj_win id) { return id >= LJ_DD_FIRST && id <= LJ_WIN_AUTO_LAUNCH; }
static lj_window *W(enum lj_win id) { return mine(id) ? dd_win[id - LJ_DD_FIRST] : NULL; }
static lj_widget *F(enum lj_win id, const char *n) { return W(id) ? lj_window_find(W(id), n) : NULL; }
static int        is_warning(enum lj_win id) { return id == LJ_WIN_HOTKEY_WARNING || id == LJ_WIN_HARDWARE_WARNING || id == LJ_WIN_LASER_WARNING || id == LJ_WIN_UNBLOCK_LASERS; }

static void close_id(enum lj_win id)
{
    lj_window *w = W(id);
    if (!w) return;
    dd_win[id - LJ_DD_FIRST] = NULL;
    lj_win_hide(w);
    lj_window_destroy(w);
}
static void on_close(lj_widget *w, void *ud) { (void)w; close_id((enum lj_win)(intptr_t)ud); }
static void on_btn_close(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) close_id((enum lj_win)(intptr_t)ud); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_help_viewer_topic("desktop"); }
static void btn(enum lj_win id, const char *name, void (*fn)(lj_widget *, int, void *))   /* a bk* button with no OnClick in its DFM */
{
    lj_widget *b = F(id, name);
    if (b) { b->on_event = fn; b->ud = (void *)(intptr_t)id; }
}
static lj_window *show(enum lj_win id, const lj_form_desc *d, const lj_binding *b, unsigned flags)
{
    lj_window *w = lj_window_create(d, b);
    if (!w) { lj_log("desktop dialogs: cannot create %s", d->class_name); return NULL; }
    dd_win[id - LJ_DD_FIRST] = w;
    lj_win_show(w, flags);
    return w;
}
#define LJ_DD_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP (platform/ is not includable here), as lj_warnings.c: the fsStayOnTop forms */

/* ---- Auto-Arrange Icons (TAutoArrangeForm 185x93) ---- */
static void aa_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_auto_arrange(lj_w_radiogroup_index(F(LJ_WIN_AUTO_ARRANGE, "RGType")) == 1); close_id(LJ_WIN_AUTO_ARRANGE); }

/* ---- Desktop Grid (TGridForm 152x213, desktopgrid.gif) ---- */
static int dd_filling;
static int size_of(const char *edit, const char *updown)
{
    const char *t = lj_w_edit_text(F(LJ_WIN_DESKTOP_GRID, edit));
    int v = t && *t ? atoi(t) : lj_w_updown_position(F(LJ_WIN_DESKTOP_GRID, updown));
    return v < 8 ? 8 : v > 64 ? 64 : v;   /* UDV / UDH Min 8 Max 64 */
}
static void grid_ok(lj_widget *w, void *ud)
{
    lj_desktop_grid g;
    (void)w; (void)ud;
    g.on = lj_w_checkbox_checked(F(LJ_WIN_DESKTOP_GRID, "DisplayGrid")); g.snap = lj_w_checkbox_checked(F(LJ_WIN_DESKTOP_GRID, "SnapFix"));
    g.vsize = size_of("EVSize", "UDV"); g.hsize = size_of("EHSize", "UDH");
    g.snap_center = lj_w_radiobutton_checked(F(LJ_WIN_DESKTOP_GRID, "SnapCenter"));
    lj_desktop_grid_set(&g);
    close_id(LJ_WIN_DESKTOP_GRID);
}
static void grid_fill(void)
{
    lj_desktop_grid g;
    lj_desktop_grid_get(&g);
    lj_w_checkbox_set_checked(F(LJ_WIN_DESKTOP_GRID, "DisplayGrid"), g.on);
    lj_w_checkbox_set_checked(F(LJ_WIN_DESKTOP_GRID, "SnapFix"), g.snap);
    lj_w_updown_set_position(F(LJ_WIN_DESKTOP_GRID, "UDV"), g.vsize);
    lj_w_updown_set_position(F(LJ_WIN_DESKTOP_GRID, "UDH"), g.hsize);
    lj_w_radiobutton_click(F(LJ_WIN_DESKTOP_GRID, g.snap_center ? "SnapCenter" : "SnapLeft"));
}

/* ---- Select Icon Style (TIconStyleForm 281x86) ---- */
static void style_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_icon_style_set(lj_w_radiogroup_index(F(LJ_WIN_ICON_STYLE, "RG1")) == 1); close_id(LJ_WIN_ICON_STYLE); }

/* ---- Desktop Image (TImageSetupForm 420x279): Image1 previews the picked bitmap stretched to 265x208 (Stretch = True) ---- */
static struct { char file[LJ_PATH_CAP]; lj_surface prev; lj_widget_vt vt; } img;
static void img_paint(lj_widget *w, lj_surface *s)
{
    int ax, ay, x, y;
    if (!img.prev.px || w->w <= 0 || w->h <= 0) return;   /* no picture: TImage paints nothing, Label3 'No Image' shows through */
    lj_widget_abs(w, &ax, &ay);
    for (y = 0; y < w->h; y++)
        for (x = 0; x < w->w; x++) lj_fill(s, ax + x, ay + y, 1, 1, img.prev.px[(y * img.prev.h / w->h) * img.prev.w + x * img.prev.w / w->w]);
}
static void img_show(void)
{
    const char *base = img.file, *p;
    for (p = img.file; *p; p++) if (*p == '\\' || *p == '/') base = p + 1;
    lj_w_set_caption(F(LJ_WIN_DESKTOP_IMAGE, "Label2"), base);   /* right-justified under the preview */
    if (W(LJ_WIN_DESKTOP_IMAGE)) lj_window_invalidate_all(W(LJ_WIN_DESKTOP_IMAGE));
}
static int img_load(const char *file)
{
    lj_surface s = { 0, 0, NULL };
    if (!dd_host || !dd_host->load_bmp || dd_host->load_bmp(file, &s) != 0 || !s.px) { lj_surface_free(&s); lj_log("desktop image: %s does not decode", file); return -1; }
    lj_surface_free(&img.prev);
    img.prev = s;
    lj_str_copy(img.file, sizeof img.file, file);
    img_show();
    return 0;
}
static void img_get(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_dlg_pick(LJ_PICK_IMAGE); }
static void img_clear(lj_widget *w, void *ud) { (void)w; (void)ud; img.file[0] = 0; lj_surface_free(&img.prev); img_show(); }
static void img_ok(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (lj_desktop_image_set(img.file, lj_w_checkbox_checked(F(LJ_WIN_DESKTOP_IMAGE, "cbStretch")), lj_w_checkbox_checked(F(LJ_WIN_DESKTOP_IMAGE, "cbCenter"))) != 0) return;
    close_id(LJ_WIN_DESKTOP_IMAGE);
}
static void img_fill(void)
{
    int st, ce;
    lj_widget *i1 = F(LJ_WIN_DESKTOP_IMAGE, "Image1");
    lj_desktop_image_flags(&st, &ce);
    lj_w_checkbox_set_checked(F(LJ_WIN_DESKTOP_IMAGE, "cbStretch"), st);
    lj_w_checkbox_set_checked(F(LJ_WIN_DESKTOP_IMAGE, "cbCenter"), ce);
    if (i1) { img.vt = *i1->vt; img.vt.paint = img_paint; i1->vt = &img.vt; }
    img.file[0] = 0; lj_surface_free(&img.prev);
    if (lj_desktop_image_file()[0] && img_load(lj_desktop_image_file()) != 0) lj_str_copy(img.file, sizeof img.file, lj_desktop_image_file());
    img_show();
}

/* ---- Desktop tabs (TLayerManagerForm 216x285): LV1 'Tab Name'; Add / Edit name the row in place (LJ2's TListView label edit,
 * no DFM: PROPOSAL rig), Enter keeps, Esc drops ---- */
static struct { const char *rows[LJ_TABS_MAX]; lj_widget *edit; lj_widget_vt vt; int row; } tb;
static const lj_ctl_desc d_tab_edit = { LJ_K_EDIT, "eTabName", 0, 0, 190, 18, .max_length = 63, .font = LJ_FONT_SANS13 };
static void tabs_list(void)
{
    int i;
    for (i = 0; i < lj_desktop_tab_count(); i++) tb.rows[i] = lj_desktop_tab_name(i);
    lj_w_listview_set_items(F(LJ_WIN_DESKTOP_TABS, "LV1"), tb.rows, lj_desktop_tab_count());
}
static void tab_edit_end(int keep)
{
    if (!tb.edit || !tb.edit->visible) return;
    tb.edit->visible = 0;
    if (keep) { const char *t = lj_w_edit_text(tb.edit); if (t && *t) lj_desktop_tab_rename(tb.row, t); }
    tabs_list();
    if (W(LJ_WIN_DESKTOP_TABS)) lj_window_invalidate_all(W(LJ_WIN_DESKTOP_TABS));
}
static int tab_edit_key(lj_widget *w, const lj_key_ev *k)
{
    if (k->type == LJ_EV_KEY_DOWN && (k->key == LJ_VK_RETURN || k->key == LJ_VK_ESCAPE)) { tab_edit_end(k->key == LJ_VK_RETURN); return 1; }
    return lj_w_edit_vt.key(w, k);
}
static void tab_edit_begin(int row)
{
    lj_widget *lv = F(LJ_WIN_DESKTOP_TABS, "LV1");
    const lj_w_listview *v = (const lj_w_listview *)lv;
    if (!lv || !tb.edit || row < 0 || row >= lj_desktop_tab_count()) return;
    lj_w_listview_select(lv, row);
    tb.row = row;
    tb.edit->x = lv->x + 2; tb.edit->y = lv->y + 2 + (v->show_headers ? v->header_h : 0) + (row - v->sb.pos) * v->row_h - 2;
    tb.edit->w = lv->w - 4 < v->col_w[0] ? lv->w - 4 : v->col_w[0];
    tb.edit->visible = 1;
    lj_w_edit_set_text(tb.edit, lj_desktop_tab_name(row));
    lj_w_edit_select(tb.edit, 0, -1);
    lj_w_focus(tb.edit);
    lj_window_invalidate_all(W(LJ_WIN_DESKTOP_TABS));
}
static void tab_add(lj_widget *w, void *ud)
{
    char name[32];
    (void)w; (void)ud;
    tab_edit_end(1);
    snprintf(name, sizeof name, "Tab%d", lj_desktop_tab_count() + 1);   /* configdesktop.gif: "Default", "Tab2" */
    if (lj_desktop_tab_add(name) != 0) return;
    tabs_list();
    tab_edit_begin(lj_desktop_tab_count() - 1);
}
static void tab_edit(lj_widget *w, void *ud) { (void)w; (void)ud; tab_edit_end(1); tab_edit_begin(lj_w_listview_index(F(LJ_WIN_DESKTOP_TABS, "LV1"))); }
static void tab_delete(lj_widget *w, void *ud) { (void)w; (void)ud; tab_edit_end(0); if (lj_desktop_tab_delete(lj_w_listview_index(F(LJ_WIN_DESKTOP_TABS, "LV1"))) == 0) tabs_list(); }
static void tab_ok(lj_widget *w, void *ud) { (void)w; (void)ud; tab_edit_end(1); close_id(LJ_WIN_DESKTOP_TABS); }
static void tab_click(lj_widget *w, void *ud) { (void)w; (void)ud; tab_edit_end(1); }
static void tabs_fill(void)
{
    lj_widget *root = lj_window_root(W(LJ_WIN_DESKTOP_TABS));
    tabs_list();
    tb.edit = lj_w_edit_create(root, &d_tab_edit);
    if (tb.edit) { tb.vt = *tb.edit->vt; tb.vt.key = tab_edit_key; tb.edit->vt = &tb.vt; tb.edit->visible = 0; }
}

/* ---- Next/Previous Fixture Selection (TNextPrevSelectionForm 289x170, nextprevfixturedialog.gif) ---- */
static void np_fill(void)
{
    lj_nextprev_opts o;
    lj_sel_nextprev_get(&o);
    dd_filling = 1;
    lj_w_checkbox_set_checked(F(LJ_WIN_NEXTPREV_SELECTION, "cbEnabled"), o.enabled);
    lj_w_checkbox_set_checked(F(LJ_WIN_NEXTPREV_SELECTION, "cbSelect"), o.same_type);
    lj_w_radiogroup_set_index(F(LJ_WIN_NEXTPREV_SELECTION, "rgTabs"), o.tabs);
    lj_w_radiogroup_set_index(F(LJ_WIN_NEXTPREV_SELECTION, "rgOrder"), o.custom);
    dd_filling = 0;
}
static void np_change(lj_widget *w, void *ud)   /* changeOptions: every box applies at once (the toolbar buttons follow Enable) */
{
    lj_nextprev_opts o, was;
    (void)w; (void)ud;
    if (dd_filling) return;
    lj_sel_nextprev_get(&was);
    o.enabled = lj_w_checkbox_checked(F(LJ_WIN_NEXTPREV_SELECTION, "cbEnabled"));
    o.same_type = lj_w_checkbox_checked(F(LJ_WIN_NEXTPREV_SELECTION, "cbSelect"));
    o.tabs = lj_w_radiogroup_index(F(LJ_WIN_NEXTPREV_SELECTION, "rgTabs")); if (o.tabs < 0) o.tabs = 0;
    o.custom = lj_w_radiogroup_index(F(LJ_WIN_NEXTPREV_SELECTION, "rgOrder")) == 1;
    lj_sel_nextprev_set(&o);
    if (o.custom && !was.custom) lj_win_open(LJ_WIN_CUSTOM_ORDER);   /* "Custom Order ..." */
}
static void np_set_custom(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_CUSTOM_ORDER); }

/* ---- Custom Fixture Selection Order (TCustomOrderForm 358x317, nextprevfixturecustom.gif) ---- */
static struct { int e[LJ_FIXTURES * 2], n, last_master; char text[LJ_FIXTURES * 2][128]; const char *rows[LJ_FIXTURES * 2]; } co;
static int co_find(int f) { int i; for (i = 0; i < co.n; i++) if (co.e[i] == f) return i; return -1; }
static void co_list(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i;
    for (i = 0; i < co.n; i++) {
        const lj_profile *p = co.e[i] >= 0 && ctx->show ? lj_show_profile(ctx->show, co.e[i]) : NULL;
        if (co.e[i] == LJ_SEL_BREAK) snprintf(co.text[i], sizeof co.text[i], "\t--- Selection Break --");   /* nextprevfixturecustom.gif, verbatim */
        else snprintf(co.text[i], sizeof co.text[i], "%d\t%s %s (%s)\t%s", co.e[i] + 1, p ? p->manufacturer : "", p ? p->name : "", p ? p->mode : "",
                      ctx->show ? ctx->show->fixture[co.e[i]].user_name : "");   /* "Martin MAC 250 Krypton (16 Bit)" | "MAC 250KR" */
        co.rows[i] = co.text[i];
    }
    lj_w_listview_set_items(F(LJ_WIN_CUSTOM_ORDER, "LV1"), co.rows, co.n);
}
static void co_commit(int select)
{
    lj_sel_custom_order_set(co.e, co.n);
    co.n = lj_sel_custom_order(co.e, LJ_FIXTURES * 2);
    co_list();
    if (select >= 0 && select < co.n) lj_w_listview_select(F(LJ_WIN_CUSTOM_ORDER, "LV1"), select);
    if (W(LJ_WIN_CUSTOM_ORDER)) lj_window_invalidate_all(W(LJ_WIN_CUSTOM_ORDER));
}
static void co_insert(int at, int v)
{
    if (co.n >= LJ_FIXTURES * 2 || (v != LJ_SEL_BREAK && (v < 0 || co_find(v) >= 0))) return;
    if (at < 0 || at > co.n) at = co.n;
    memmove(&co.e[at + 1], &co.e[at], sizeof co.e[0] * (size_t)(co.n - at));
    co.e[at] = v; co.n++;
    co_commit(at);
}
static void co_remove(int at) { if (at < 0 || at >= co.n) return; memmove(&co.e[at], &co.e[at + 1], sizeof co.e[0] * (size_t)(co.n - at - 1)); co.n--; co_commit(at < co.n ? at : co.n - 1); }
int lj_desktop_dlg_custom_move(int from, int to)
{
    int v;
    if (!W(LJ_WIN_CUSTOM_ORDER) || from < 0 || from >= co.n || to < 0 || to >= co.n) return -1;
    v = co.e[from];
    if (to > from) memmove(&co.e[from], &co.e[from + 1], sizeof co.e[0] * (size_t)(to - from));
    else memmove(&co.e[to + 1], &co.e[to], sizeof co.e[0] * (size_t)(from - to));
    co.e[to] = v;
    co_commit(to);
    return 0;
}
static void co_pop(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(W(LJ_WIN_CUSTOM_ORDER));
    int sel = lj_w_listview_index(F(LJ_WIN_CUSTOM_ORDER, "LV1")), m = lj_sel_master();
    const char *n = it && it->name ? it->name : "";
    (void)w; (void)ud;
    if (!strcmp(n, "P_InsertMaster")) co_insert(sel < 0 ? 0 : sel, m);
    else if (!strcmp(n, "P_AddMaster")) co_insert(co.n, m);
    else if (!strcmp(n, "P_InsertBreak")) co_insert(sel < 0 ? 0 : sel, LJ_SEL_BREAK);
    else if (!strcmp(n, "P_AddBreak")) co_insert(co.n, LJ_SEL_BREAK);
    else if (!strcmp(n, "P_RemoveFixture")) { if (sel >= 0 && sel < co.n && co.e[sel] != LJ_SEL_BREAK) co_remove(sel); }
    else if (!strcmp(n, "P_RemoveBreak")) { if (sel >= 0 && sel < co.n && co.e[sel] == LJ_SEL_BREAK) co_remove(sel); }
    else if (!strcmp(n, "P_MovetotopofList")) lj_desktop_dlg_custom_move(sel, 0);
    else if (!strcmp(n, "P_MovetoendofList")) lj_desktop_dlg_custom_move(sel, co.n - 1);
    else if (!strcmp(n, "P_RemoveLines")) co_remove(sel);   /* ponytail: our TListView selects one row (MultiSelect is not modelled), so one line goes */
}
static void co_popup(lj_widget *w, void *ud)   /* PopupMenu1Popup: only what applies to the clicked row and the master */
{
    lj_menu *m = lj_window_menus(W(LJ_WIN_CUSTOM_ORDER));
    int sel = lj_w_listview_index(F(LJ_WIN_CUSTOM_ORDER, "LV1")), ms = lj_sel_master(), brk = sel >= 0 && sel < co.n && co.e[sel] == LJ_SEL_BREAK;
    (void)w; (void)ud;
    if (!m) return;
    lj_menu_set_enabled(m, "P_InsertMaster", ms >= 0 && co_find(ms) < 0); lj_menu_set_enabled(m, "P_AddMaster", ms >= 0 && co_find(ms) < 0);
    lj_menu_set_enabled(m, "P_RemoveFixture", sel >= 0 && !brk); lj_menu_set_enabled(m, "P_RemoveBreak", brk);
    lj_menu_set_enabled(m, "P_MovetotopofList", sel > 0); lj_menu_set_enabled(m, "P_MovetoendofList", sel >= 0 && sel < co.n - 1);
    lj_menu_set_enabled(m, "P_RemoveLines", sel >= 0);
}
static void co_clear(lj_widget *w, void *ud) { (void)w; (void)ud; co.n = 0; co_commit(-1); }
static void co_add_rest(lj_widget *w, void *ud)   /* Options > Add Remaining Fixtures: every patched fixture not listed, by number */
{
    const lj_ctx *ctx = lj_ctx_get();
    int f;
    (void)w; (void)ud;
    for (f = 0; ctx->show && f < LJ_FIXTURES && co.n < LJ_FIXTURES * 2; f++) if (ctx->show->fixture[f].link && co_find(f) < 0) co.e[co.n++] = f;
    co_commit(-1);
}
static void co_drop(lj_widget *w, void *ud)   /* LV1DragDrop: the dragged (selected) row onto the row under the cursor */
{
    lj_window *win = lj_window_of(w);
    const lj_mouse_ev *m = win ? lj_window_last_event(win) : NULL;
    int ax, ay, to;
    (void)ud;
    if (!m) return;
    lj_widget_abs(w, &ax, &ay);
    to = lj_w_listview_row_at(w, m->y - ay);
    lj_desktop_dlg_custom_move(lj_w_listview_index(w), to < 0 ? co.n - 1 : to);
}
static void co_refresh(void)   /* sbAddMaster down: a master picked on the desktop is appended (Auto-add Master Fixture) */
{
    int m = lj_sel_master();
    if (m == co.last_master) return;
    co.last_master = m;
    if (m >= 0 && lj_w_speedbutton_down(F(LJ_WIN_CUSTOM_ORDER, "sbAddMaster")) && co_find(m) < 0) co_insert(co.n, m);
}

/* ---- Save Custom Selection (TSaveSelectionForm 197x74, saveselectiondialog.gif) ---- */
static struct { int fix[LJ_FIXTURES], n, saved; lj_widget_vt vt; } ss;
static int ss_pref(const char *key) { return lj_ini_geti(lj_ctx_get()->forms_ini, "SAVESELECTION", key, key[0] == 'A'); }   /* forms.ini [SAVESELECTION] AutoClose (1) SaveOnEnter (0): ours, as [SAVECUEFORM] */
static void ss_save(int as_new)
{
    const char *no = lj_w_edit_text(F(LJ_WIN_SAVE_SELECTION, "ENo")), *name = lj_w_edit_text(F(LJ_WIN_SAVE_SELECTION, "eName"));
    char t[8];
    int r = lj_custom_sel_save(as_new || !no ? 0 : atoi(no), name ? name : "", ss.fix, ss.n);
    if (r < 0) { lj_log("save custom selection: refused (empty name or %d selections)", LJ_CUSTOM_SEL_MAX); return; }
    ss.saved = r;
    snprintf(t, sizeof t, "%d", r);
    lj_w_edit_set_text(F(LJ_WIN_SAVE_SELECTION, "ENo"), t);
    if (ss_pref("AutoClose")) close_id(LJ_WIN_SAVE_SELECTION);
}
static void ss_save_btn(lj_widget *w, void *ud) { (void)ud; ss_save(w && w->name && strcmp(w->name, "BSaveNew") == 0); }
static void ss_no_change(lj_widget *w, void *ud) { const char *t = lj_w_edit_text(w); lj_widget *b = F(LJ_WIN_SAVE_SELECTION, "Bsave"); (void)ud; if (b) { b->enabled = t && *t; lj_widget_invalidate(b); } }
static void ss_prefs(lj_widget *w, void *ud)   /* PrefsClick: M_AutoClose / M_SaveOnEnter toggle */
{
    const lj_menu_item_desc *it = lj_window_menu_item(W(LJ_WIN_SAVE_SELECTION));
    const char *key = it && it->name && !strcmp(it->name, "M_SaveOnEnter") ? "SaveOnEnter" : "AutoClose";
    int v = !ss_pref(key);
    (void)w; (void)ud;
    lj_ini_seti(lj_ctx_get()->forms_ini, "SAVESELECTION", key, v);
    lj_menu_set_checked(lj_window_menus(W(LJ_WIN_SAVE_SELECTION)), key[0] == 'A' ? "M_AutoClose" : "M_SaveOnEnter", v);
}
static int ss_name_key(lj_widget *w, const lj_key_ev *k)   /* KeyPreview + FormKeyDown: Enter saves when Save On Enter is checked */
{
    if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_RETURN && ss_pref("SaveOnEnter")) { const char *no = lj_w_edit_text(F(LJ_WIN_SAVE_SELECTION, "ENo")); ss_save(!no || !*no); return 1; }
    return lj_w_edit_vt.key(w, k);
}
lj_window *lj_desktop_dlg_save_selection(const int *fix, int n, int existing_no)
{
    lj_widget *e;
    char t[8] = "";
    int i;
    if (!lj_win_open(LJ_WIN_SAVE_SELECTION)) return NULL;
    for (ss.n = 0, i = 0; fix && i < n && ss.n < LJ_FIXTURES; i++) if (fix[i] >= 0 && fix[i] < LJ_FIXTURES) ss.fix[ss.n++] = fix[i];
    if (existing_no > 0 && existing_no <= lj_custom_sel_count()) snprintf(t, sizeof t, "%d", existing_no);
    lj_w_edit_set_text(F(LJ_WIN_SAVE_SELECTION, "ENo"), t);   /* fires ENoChange: Save is enabled with a number */
    lj_w_edit_set_text(F(LJ_WIN_SAVE_SELECTION, "eName"), t[0] ? lj_custom_sel_name(existing_no) : "");
    if ((e = F(LJ_WIN_SAVE_SELECTION, "eName")) != NULL) { ss.vt = *e->vt; ss.vt.key = ss_name_key; e->vt = &ss.vt; lj_w_focus(e); }
    return W(LJ_WIN_SAVE_SELECTION);
}
int lj_desktop_dlg_saved_selection(void) { return ss.saved; }

/* ---- Launching.. (TAutoLaunchForm 395x205): the Startup Options list, one program per Timer1 tick (100 ms) ---- */
static struct { char path[LJ_LAUNCH_MAX][LJ_PATH_CAP]; char text[LJ_LAUNCH_MAX][LJ_PATH_CAP + 48]; const char *rows[LJ_LAUNCH_MAX]; int n, next; } al;
static void al_list(void) { int i; for (i = 0; i < al.n; i++) al.rows[i] = al.text[i]; lj_w_listview_set_items(F(LJ_WIN_AUTO_LAUNCH, "lv1"), al.rows, al.n); }
static void al_step(void)
{
    const char *st;
    if (!W(LJ_WIN_AUTO_LAUNCH) || al.next >= al.n) return;
    st = !dd_host || !dd_host->exec ? "Not launched"   /* LORD_C-8: no launch path is wired to the auto-launch list (the Execute File rule); the Status column is ~127 px */
       : dd_host->exec(al.path[al.next]) == 0 ? "Launched" : "Failed";
    snprintf(al.text[al.next], sizeof al.text[0], "%s\t%s", al.path[al.next], st);
    al.next++;
    al_list();
    lj_window_invalidate_all(W(LJ_WIN_AUTO_LAUNCH));
}
static void al_fill(void)
{
    int i;
    al.n = lj_auto_launch_load(al.path, LJ_LAUNCH_MAX); al.next = 0;
    for (i = 0; i < al.n; i++) snprintf(al.text[i], sizeof al.text[0], "%s\t", al.path[i]);
    al_list();
}

/* ---- the family entry points ---- */
lj_window *lj_desktop_dlg_open(enum lj_win id)
{
    static const lj_binding aa_b[] = { { "bOkClick", aa_ok, NULL }, { NULL, NULL, NULL } };
    static const lj_binding grid_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, (void *)(intptr_t)LJ_WIN_DESKTOP_GRID }, { "FormCreate", lj_handler_noop, NULL },
                                         { "FormDestroy", lj_handler_noop, NULL }, { "DisplayGridClick", lj_handler_noop, NULL } /* applied on Ok */, { "BOkClick", grid_ok, NULL }, { NULL, NULL, NULL } };
    static const lj_binding style_b[] = { { "FormShow", lj_handler_noop, NULL }, { "BOkClick", style_ok, NULL }, { "BCancelClick", on_close, (void *)(intptr_t)LJ_WIN_ICON_STYLE }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding img_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
                                        { "BGetClick", img_get, NULL }, { "BClearClick", img_clear, NULL }, { "BokClick", img_ok, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding tabs_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, (void *)(intptr_t)LJ_WIN_DESKTOP_TABS }, { "FormCreate", lj_handler_noop, NULL },
                                         { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "LV1Click", tab_click, NULL }, { "BAddClick", tab_add, NULL },
                                         { "BdeleteClick", tab_delete, NULL }, { "BEditClick", tab_edit, NULL }, { "BOkClick", tab_ok, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding np_b[] = { { "FormClose", on_close, (void *)(intptr_t)LJ_WIN_NEXTPREV_SELECTION }, { "FormShow", lj_handler_noop, NULL }, { "changeOptions", np_change, NULL },
                                       { "SetCustomOrder1Click", np_set_custom, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding co_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, (void *)(intptr_t)LJ_WIN_CUSTOM_ORDER }, { "FormShow", lj_handler_noop, NULL },
                                       { "LV1DragDrop", co_drop, NULL }, { "LV1DragOver", lj_handler_noop, NULL }, { "PopupMenu1Popup", co_popup, NULL }, { "PopClick", co_pop, NULL },
                                       { "ClearAll1Click", co_clear, NULL }, { "AddRemainingFixtures1Click", co_add_rest, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding ss_b[] = { { "FormClose", on_close, (void *)(intptr_t)LJ_WIN_SAVE_SELECTION }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
                                       { "FormKeyDown", lj_handler_noop, NULL } /* Enter is eName's (ss_name_key) */, { "FormShow", lj_handler_noop, NULL }, { "ENoChange", ss_no_change, NULL },
                                       { "BsaveClick", ss_save_btn, NULL }, { "BSaveNewClick", ss_save_btn, NULL }, { "PrefsClick", ss_prefs, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
    static const lj_binding al_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "bCancelClick", on_close, (void *)(intptr_t)LJ_WIN_AUTO_LAUNCH }, { "Timer1Timer", lj_handler_noop, NULL } /* stepped from the 100 ms refresh */, { NULL, NULL, NULL } };
    lj_window *w;
    if (is_warning(id)) return lj_warnings_w4_open(id);
    if (!mine(id)) return NULL;
    if ((w = W(id)) != NULL) { lj_win_show(w, 0); return w; }
    switch (id) {
    case LJ_WIN_AUTO_ARRANGE:
        if (!show(id, &lj_form_TAUTOARRANGEFORM, aa_b, 0)) return NULL;
        btn(id, "bCancel", on_btn_close);
        break;
    case LJ_WIN_DESKTOP_GRID:
        if (!show(id, &lj_form_TGRIDFORM, grid_b, 0)) return NULL;
        grid_fill();
        break;
    case LJ_WIN_ICON_STYLE:
        if (!show(id, &lj_form_TICONSTYLEFORM, style_b, 0)) return NULL;
        lj_w_radiogroup_set_index(F(id, "RG1"), lj_desktop_icon_style());
        break;
    case LJ_WIN_DESKTOP_IMAGE:
        if (!show(id, &lj_form_TIMAGESETUPFORM, img_b, 0)) return NULL;
        img_fill();
        break;
    case LJ_WIN_DESKTOP_TABS:
        if (!show(id, &lj_form_TLAYERMANAGERFORM, tabs_b, 0)) return NULL;
        tabs_fill();
        break;
    case LJ_WIN_NEXTPREV_SELECTION:
        if (!show(id, &lj_form_TNEXTPREVSELECTIONFORM, np_b, LJ_DD_STAY_ON_TOP)) return NULL;
        np_fill();
        break;
    case LJ_WIN_CUSTOM_ORDER:
        if (!show(id, &lj_form_TCUSTOMORDERFORM, co_b, LJ_DD_STAY_ON_TOP)) return NULL;
        co.n = lj_sel_custom_order(co.e, LJ_FIXTURES * 2); co.last_master = lj_sel_master();
        co_list();
        break;
    case LJ_WIN_SAVE_SELECTION:
        if (!show(id, &lj_form_TSAVESELECTIONFORM, ss_b, LJ_DD_STAY_ON_TOP)) return NULL;
        ss.n = 0;
        lj_menu_set_checked(lj_window_menus(W(id)), "M_AutoClose", ss_pref("AutoClose"));
        lj_menu_set_checked(lj_window_menus(W(id)), "M_SaveOnEnter", ss_pref("SaveOnEnter"));
        ss_no_change(F(id, "ENo"), NULL);
        break;
    case LJ_WIN_AUTO_LAUNCH:
        if (!show(id, &lj_form_TAUTOLAUNCHFORM, al_b, LJ_DD_STAY_ON_TOP)) return NULL;
        al_fill();
        break;
    default: return NULL;
    }
    return W(id);
}

void lj_desktop_dlg_close(enum lj_win id)
{
    if (is_warning(id)) { lj_warnings_w4_close(id); return; }
    if (id == LJ_WIN_DESKTOP_IMAGE) lj_surface_free(&img.prev);
    if (id == LJ_WIN_DESKTOP_TABS) tb.edit = NULL;   /* freed with the window's tree */
    close_id(id);
}
lj_window *lj_desktop_dlg_get(enum lj_win id) { return is_warning(id) ? lj_warnings_w4_get(id) : W(id); }

static void pick_poll(void)
{
    char path[LJ_PATH_CAP];
    int r;
    if (dd_pending == LJ_PICK_NONE || !dd_host || !dd_host->pick_take) return;
    if ((r = dd_host->pick_take(path, sizeof path)) == 0) return;
    if (r == 1) lj_desktop_dlg_picked(dd_pending, path);
    dd_pending = LJ_PICK_NONE;
}

void lj_desktop_dlg_refresh(enum lj_win id)
{
    static int al_checked;
    if (is_warning(id)) { lj_warnings_w4_refresh(id); return; }
    switch (id) {
    case LJ_WIN_DESKTOP_IMAGE: pick_poll(); break;      /* the picker answers Desktop Image and Startup Options alike */
    case LJ_WIN_CUSTOM_ORDER: if (W(id)) co_refresh(); break;
    case LJ_WIN_AUTO_LAUNCH:
        if (!al_checked && lj_desktop_window() && lj_ctx_get()->userdir[0]) {   /* once, when the desktop is up: the Startup Options programs [01 s6.2] */
            char tmp[1][LJ_PATH_CAP];
            al_checked = 1;
            if (lj_auto_launch_load(tmp, 1) > 0) lj_win_open(LJ_WIN_AUTO_LAUNCH);
        }
        al_step();
        break;
    default: break;
    }
}

void lj_desktop_dlg_bind_fns(void) { lj_sel_nextprev_load(); }   /* Main.Ini is open when the app installs the window host */

void lj_desktop_dlg_set_host(const lj_desktop_dlg_host *h) { dd_host = h; lj_desktop_tabs_set_bmp_loader(h ? h->load_bmp : NULL); }
int lj_desktop_dlg_pick(enum lj_pick who)
{
    if (!dd_host || !dd_host->pick_file) { lj_log("desktop dialogs: no file dialog (headless)"); return -1; }
    if (dd_host->pick_file(who == LJ_PICK_IMAGE) != 0) return -1;
    dd_pending = who;
    return 0;
}
void lj_desktop_dlg_picked(enum lj_pick who, const char *path)
{
    if (!path || !path[0]) return;
    if (who == LJ_PICK_IMAGE && W(LJ_WIN_DESKTOP_IMAGE)) img_load(path);
    else if (who == LJ_PICK_LAUNCH) lj_startup_options_add_launch(path);
}
