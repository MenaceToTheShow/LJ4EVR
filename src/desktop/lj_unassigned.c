/* lj_unassigned.c - the Unassigned Fixtures list (lj_unassigned.h). */
#include "desktop/lj_unassigned.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "desktop/lj_desktop_tabs.h"
#include "fixlib/lj_profile.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_dnd.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"

static lj_window   *lj_una;
static lj_widget   *lj_una_lv;
static char         lj_una_row[LJ_FIXTURES][128];
static const char  *lj_una_ptr[LJ_FIXTURES];
static int          lj_una_fixture[LJ_FIXTURES], lj_una_n;
static lj_widget_vt lj_una_vt; static int (*lj_una_mouse_orig)(lj_widget *, const lj_mouse_ev *);

/* h_noop = lj_handler_noop: FormCreate / FormShow / ColumnClick / Compare / EndDrag: the VCL sort hooks; the list is rebuilt by number */
static void h_move_icons(lj_widget *w, void *ud) { (void)ud; lj_desktop_move_mode(!lj_w_checkbox_checked(w)); }   /* OnMouseDown fires before the toggle */
static void h_place_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_auto_place(0); }
static void h_unassign_all(lj_widget *w, void *ud)   /* "Unassign All Fixtures ?" (13 exe strings) is the app's prompt; PROPOSAL: W1 unassigns straight away */
{
    const lj_ctx *ctx = lj_ctx_get();
    int i;
    (void)w; (void)ud;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++)
        if (ctx->show->fixture[i].link && ctx->show->fixture[i].placed) lj_desktop_unassign_icon(i);
}
static void h_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }

static const lj_binding lj_una_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "LV_UnassignedColumnClick", lj_handler_noop, NULL }, { "LV_UnassignedCompare", lj_handler_noop, NULL }, { "LV_UnassignedEndDrag", lj_handler_noop, NULL },
    { "MoveIconsMouseDown", h_move_icons, NULL }, { "M_PlaceAllClick", h_place_all, NULL },
    { "UnassignAll1Click", h_unassign_all, NULL }, { "Help1Click", h_help, NULL },
    { NULL, NULL, NULL } };

static int lv_mouse(lj_widget *w, const lj_mouse_ev *m)   /* dmAutomatic: a left press on a row starts the drag onto the desktop */
{
    int rc = lj_una_mouse_orig ? lj_una_mouse_orig(w, m) : 0;
    if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) {
        int f = lj_unassigned_fixture_at_row(lj_w_listview_row_at(w, m->y));
        if (f >= 0) lj_dnd_begin(w, LJ_DND_FIXTURE, f);
    }
    return rc;
}

int lj_unassigned_fixture_at_row(int row) { return row >= 0 && row < lj_una_n ? lj_una_fixture[row] : -1; }
lj_window *lj_unassigned_window(void) { return lj_una; }

lj_window *lj_unassigned_open(void)
{
    if (lj_una) return lj_una;
    lj_una = lj_window_create(&lj_form_TUNASSIGNEDFORM, lj_una_bindings);
    if (!lj_una) return NULL;
    lj_una_lv = lj_window_find(lj_una, "LV_Unassigned");
    if (lj_una_lv) {
        lj_una_vt = *lj_una_lv->vt; lj_una_mouse_orig = lj_una_lv->vt->mouse; lj_una_vt.mouse = lv_mouse;
        lj_una_lv->vt = &lj_una_vt;
    }
    lj_unassigned_refresh();
    if (lj_desktop_host_open(lj_una, 1) != 0) lj_log("desktop: Unassigned Fixtures list built headless");
    return lj_una;
}

void lj_unassigned_close(void)
{
    if (!lj_una) return;
    lj_desktop_host_close(lj_una);
    lj_window_destroy(lj_una);
    lj_una = NULL; lj_una_lv = NULL; lj_una_n = 0;
}

void lj_unassigned_refresh(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i;
    lj_una_n = 0;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &ctx->show->fixture[i];
        const lj_profile *p;
        if (!f->link || f->placed) continue;
        p = lj_fixlib_by_id(f->profile_id);
        snprintf(lj_una_row[lj_una_n], sizeof lj_una_row[0], "%d\t%s\t%d:%03d\t%s", i + 1, p ? p->name : f->gs2_file, f->link, f->address, f->user_name);   /* Address as link:address, "1:031" [01 s3.7] */
        lj_una_ptr[lj_una_n] = lj_una_row[lj_una_n];
        lj_una_fixture[lj_una_n] = i;
        lj_una_n++;
    }
    if (lj_una_lv) lj_w_listview_set_items(lj_una_lv, lj_una_ptr, lj_una_n);
}

int lj_unassigned_count(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i, n = 0;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++) n += ctx->show->fixture[i].link && !ctx->show->fixture[i].placed;
    return n;
}
