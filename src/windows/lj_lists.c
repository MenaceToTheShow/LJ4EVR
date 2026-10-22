/* lj_lists.c - the List of Sequences (lj_lists.h). */
#include "windows/lj_lists.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_warnings.h"

/* ---- the hand-built form (no DFM: LJ2 created this list at runtime) ---- */

/* column widths measured from listsequences.gif (400x185, reduced), rig */
static const lj_column_desc lj_sl_cols[] = {
    { "Seq No", 38, LJ_TA_UNSET, -1, 0, 0 }, { "Sequence Name", 132, LJ_TA_UNSET, -1, 0, 0 }, { "Types", 85, LJ_TA_UNSET, -1, 0, 0 },
    { "Scenes", 41, LJ_TA_UNSET, -1, 0, 0 }, { "Modified", 99, LJ_TA_UNSET, -1, 0, 0 },
};
static const lj_ctl_desc lj_sl_ctls[] = {
    { LJ_K_LISTVIEW, "LV1", 0, 0, 400, 147, .align = LJ_AL_CLIENT, .view_style = "vsReport", .row_select = LJ_YES, .hide_selection = LJ_NO,
      .popup_menu = "PopupMenu1", .columns = lj_sl_cols, .ncolumns = LJ_COUNT_OF(lj_sl_cols), .font = LJ_FONT_SANS13, .font_inherited = 1,
      .events = { { "OnClick", "LV1Click" }, { "OnDblClick", "LV1DblClick" }, { "OnColumnClick", "LV1ColumnClick" }, { "OnSelectItem", "LV1SelectItem" } } },
    { LJ_K_MAINMENU, "MainMenu1", 0, 0, 0, 0, .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_POPUPMENU, "PopupMenu1", 0, 0, 0, 0, .font = LJ_FONT_SANS13, .font_inherited = 1 },
};
static const lj_menu_item_desc lj_sl_prefs[] = {     /* captions [13 exe strings 0x35D41C, 0x35D430, 0x35D448] */
    { .name = "M_DragMode", .caption = "Drag Mode", .on_click = "M_ModeClick", .checked = LJ_YES, .radio_item = LJ_YES, .image_index = -1 },
    { .name = "M_BrowseEdit", .caption = "Browse to Edit", .on_click = "M_ModeClick", .radio_item = LJ_YES, .image_index = -1 },
    { .name = "M_BrowseCue", .caption = "Browse to Cue", .on_click = "M_ModeClick", .radio_item = LJ_YES, .image_index = -1 },
};
static const lj_menu_item_desc lj_sl_main[] = {
    { .name = "Preferences1", .caption = "&Preferences", .image_index = -1, .children = lj_sl_prefs, .nchildren = LJ_COUNT_OF(lj_sl_prefs) },
    { .name = "Help1", .caption = "&Help", .on_click = "Help1Click", .image_index = -1 },
};
static const lj_menu_item_desc lj_sl_popup[] = {     /* "Edit Sequence", "Add to BG Cue", "Add to 2510 list" [13 exe strings]; the other two per the CHM topic */
    { .name = "P_Edit", .caption = "Edit Sequence", .on_click = "P_EditClick", .image_index = -1 },
    { .name = "P_AddCue", .caption = "Add Sequence To Cue", .on_click = "P_AddCueClick", .image_index = -1 },
    { .name = "P_AddBG", .caption = "Add to BG Cue", .on_click = "P_AddBGClick", .image_index = -1 },
    { .name = "P_Add2510", .caption = "Add to 2510 list", .on_click = "P_Add2510Click", .enabled = LJ_NO, .image_index = -1 },
    { .name = "P_Delete", .caption = "Delete the sequence file", .on_click = "P_DeleteClick", .image_index = -1 },
};
static const lj_menu_desc lj_sl_menus[] = { { "MainMenu1", 1, lj_sl_main, LJ_COUNT_OF(lj_sl_main) }, { "PopupMenu1", 0, lj_sl_popup, LJ_COUNT_OF(lj_sl_popup) } };
static const lj_form_desc lj_sl_form = {
    .name = "SeqListForm", .class_name = "TSeqListForm", .caption = "Select Sequence (Drag To Target)",
    .client_width = 400, .client_height = 147,            /* PROPOSAL rig: listsequences.gif less caption and menu bar */
    .text_height = 13, .color = LJ_CLR(0xC0C0C0), .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11,
    .border = LJ_BS_SIZEABLE, .style = LJ_FS_STAYONTOP, .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .menu = "MainMenu1",
    .menus = lj_sl_menus, .nmenus = LJ_COUNT_OF(lj_sl_menus), .ctls = lj_sl_ctls, .n = LJ_COUNT_OF(lj_sl_ctls),
    .events = { { "OnClose", "FormClose" }, { "OnShow", "FormShow" } },
};
static const char *const lj_sl_captions[] = { "Select Sequence (Drag To Target)", "Select Sequence (Browse to Edit)", "Select Sequence (Browse to Cue)" };   /* [13 exe strings 0x35F244..0x35F29C] */

/* ---- rows ---- */

typedef struct { uint32_t no; char name[31], type[31]; uint16_t scenes; char line[96]; } lj_sl_row;
static lj_sl_row *lj_sl_rows; static const char **lj_sl_items; static int lj_sl_n, lj_sl_col;
static lj_window *lj_sl_win;
static int lj_sl_mode = LJ_SEQLIST_DRAG, lj_sl_close_pending;

static int lj_sl_icmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb || !ca) return ca - cb;
    }
}

static int lj_sl_cmp(const void *pa, const void *pb)
{
    const lj_sl_row *a = pa, *b = pb;
    int r = 0;
    switch (lj_sl_col) {
    case 1: r = lj_sl_icmp(a->name, b->name); break;
    case 2: r = lj_sl_icmp(a->type, b->type); break;
    case 3: r = (int)a->scenes - (int)b->scenes; break;
    default: break;                           /* 0 and 4 (Modified is blank: no timestamp in the record) fall back to the number */
    }
    return r ? r : a->no < b->no ? -1 : a->no > b->no;
}

static void lj_sl_show(void)
{
    lj_widget *lv = lj_sl_win ? lj_window_find(lj_sl_win, "LV1") : NULL;
    int i;
    for (i = 0; i < lj_sl_n; i++) lj_sl_items[i] = lj_sl_rows[i].line;
    if (lv) lj_w_listview_set_items(lv, lj_sl_items, lj_sl_n);
}

void lj_seqlist_sort(int col)
{
    lj_sl_col = col;
    if (lj_sl_n > 1) qsort(lj_sl_rows, (size_t)lj_sl_n, sizeof *lj_sl_rows, lj_sl_cmp);
    lj_sl_show();
}

void lj_seqlist_rebuild(void)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t *nums;
    int n, i;
    if (!lj_sl_win) return;
    free(lj_sl_rows); free(lj_sl_items); lj_sl_rows = NULL; lj_sl_items = NULL; lj_sl_n = 0;
    nums = calloc(LJ_MACROS_MAX, sizeof *nums);  /* 9999: the library's per-kind ceiling */
    if (!nums) lj_fatal("seqlist: out of memory");
    n = c->lib ? lj_library_list_records(c->lib, LJ_REC_SEQ, nums, LJ_MACROS_MAX) : 0;
    if (n > 0) {
        lj_sl_rows = calloc((size_t)n, sizeof *lj_sl_rows); lj_sl_items = calloc((size_t)n, sizeof *lj_sl_items);
        if (!lj_sl_rows || !lj_sl_items) lj_fatal("seqlist: out of memory");
        for (i = 0; i < n; i++) {
            lj_sl_row *r = &lj_sl_rows[i];
            lj_sequence tmp;
            char path[LJ_PATH_CAP], err[64];
            memset(&tmp, 0, sizeof tmp);
            r->no = nums[i];
            if (lj_library_record_path(c->lib, LJ_REC_SEQ, nums[i], path, sizeof path) == 0 && lj_sequence_load(path, &tmp, err, sizeof err) == 0) {
                lj_str_copy(r->name, sizeof r->name, tmp.name); lj_str_copy(r->type, sizeof r->type, tmp.type); r->scenes = tmp.n;
            }
            lj_sequence_free_scenes(&tmp);
            snprintf(r->line, sizeof r->line, "%04u\t%s\t%s\t%u\t", (unsigned)r->no, r->name, r->type, (unsigned)r->scenes);   /* ponytail: Modified blank, the record carries no timestamp */
        }
        lj_sl_n = n;
    }
    free(nums);
    lj_seqlist_sort(lj_sl_col);
}

int lj_seqlist_count(void) { return lj_sl_n; }
uint32_t lj_seqlist_number_at(int row) { return row >= 0 && row < lj_sl_n ? lj_sl_rows[row].no : 0; }

int lj_seqlist_selected(void)
{
    lj_widget *lv = lj_sl_win ? lj_window_find(lj_sl_win, "LV1") : NULL;
    return lv ? ((lj_w_listview *)lv)->index : -1;
}

static uint32_t lj_sl_sel_no(void) { return lj_seqlist_number_at(lj_seqlist_selected()); }

void lj_seqlist_mode_set(enum lj_seqlist_mode mode)
{
    lj_menu *m;
    lj_sl_mode = (int)mode;
    if (!lj_sl_win) return;
    m = lj_window_menus(lj_sl_win);
    lj_menu_set_checked(m, "M_DragMode", mode == LJ_SEQLIST_DRAG);
    lj_menu_set_checked(m, "M_BrowseEdit", mode == LJ_SEQLIST_BROWSE_EDIT);
    lj_menu_set_checked(m, "M_BrowseCue", mode == LJ_SEQLIST_BROWSE_CUE);
    lj_window_set_caption(lj_sl_win, lj_sl_captions[mode < 3 ? mode : 0]);
}

/* ---- actions ---- */

static void lj_sl_add_to_cue(uint32_t no)
{
    if (no && lj_cue_form_add_sequence(no) < 0) lj_win_message("Add Sequence to Cue", "No free sequence slots available");   /* [13 exe strings 0x191BE8] */
}

static int lj_sl_in_current_cue(uint32_t no)
{
    lj_ctx *c = lj_ctx_get();
    int i;
    if (!c->show || !c->show->current_cue) return 0;
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (c->show->current_cue->slot[i].seq == no) return 1;
    return 0;
}

static void lj_sl_delete_yes(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t no = (uint32_t)(uintptr_t)ud;
    lj_sequence *rec;
    if (!yes || !c->lib || !no) return;
    if (lj_delete_files_record(LJ_REC_SEQ, no) != 0) { lj_log("seqlist: delete %u failed", (unsigned)no); return; }   /* the app's deleter: runners and link statics handed NULL first (ME-90) */
    lj_lock();
    rec = (lj_sequence *)lj_table_remove(&c->show->sequences, no);   /* only without the app (headless): the deleter already took it */
    lj_unlock();
    if (rec && !lj_sl_in_current_cue(no)) lj_sequence_free(rec);   /* ponytail: a record the current cue still pins is leaked, not freed (2.3 rule 6); lj_app_delete_record hands the engine NULL first and is the app's */
    lj_seqlist_rebuild();
}

static void lj_sl_delete(uint32_t no)
{
    char line[64];
    if (!no) return;
    snprintf(line, sizeof line, "Delete sequence file %04u ?", (unsigned)no);   /* our wording: LJ2's prompt text for this item is not in the string catalogue */
    lj_win_confirm("Delete Sequence", line, NULL, lj_sl_delete_yes, (void *)(uintptr_t)no);
}

/* ---- handlers ---- */

static void lj_sl_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void lj_sl_on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sl_close_pending = 1; }
static void lj_sl_on_show(lj_widget *w, void *ud) { (void)w; (void)ud; lj_seqlist_rebuild(); }

static void lj_sl_on_click(lj_widget *w, void *ud)      /* the press that selected a row: in Drag Mode it is the drag source */
{
    uint32_t no = lj_sl_sel_no();
    (void)ud;
    if (lj_sl_mode == LJ_SEQLIST_DRAG && no) lj_dnd_begin(w, LJ_DND_SEQUENCE, (int)no);
}

static void lj_sl_on_select(lj_widget *w, void *ud)
{
    uint32_t no = lj_sl_sel_no();
    (void)w; (void)ud;
    if (lj_sl_mode == LJ_SEQLIST_BROWSE_EDIT) lj_seq_form_load(no, 0);
    else if (lj_sl_mode == LJ_SEQLIST_BROWSE_CUE) lj_sl_add_to_cue(no);
}

static void lj_sl_on_dblclick(lj_widget *w, void *ud) { (void)w; (void)ud; lj_seq_form_load(lj_sl_sel_no(), 0); }

static void lj_sl_on_column(lj_widget *w, void *ud) { (void)ud; lj_seqlist_sort(((lj_w_listview *)w)->clicked_col); }

static void lj_sl_on_mode(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_sl_win);
    (void)w; (void)ud;
    if (!it) return;
    lj_seqlist_mode_set(strcmp(it->name, "M_BrowseEdit") == 0 ? LJ_SEQLIST_BROWSE_EDIT : strcmp(it->name, "M_BrowseCue") == 0 ? LJ_SEQLIST_BROWSE_CUE : LJ_SEQLIST_DRAG);
}

static void lj_sl_on_edit(lj_widget *w, void *ud) { (void)w; (void)ud; lj_seq_form_load(lj_sl_sel_no(), 0); }
static void lj_sl_on_addcue(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sl_add_to_cue(lj_sl_sel_no()); }
static void lj_sl_on_addbg(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("seqlist: Add to BG Cue needs the background cue window (W2)"); }
static void lj_sl_on_delete(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sl_delete(lj_sl_sel_no()); }

static const lj_binding lj_sl_bindings[] = {
    { "FormClose", lj_sl_on_close, NULL }, { "FormShow", lj_sl_on_show, NULL },
    { "LV1Click", lj_sl_on_click, NULL }, { "LV1DblClick", lj_sl_on_dblclick, NULL }, { "LV1ColumnClick", lj_sl_on_column, NULL }, { "LV1SelectItem", lj_sl_on_select, NULL },
    { "M_ModeClick", lj_sl_on_mode, NULL }, { "Help1Click", lj_sl_noop, NULL },
    { "P_EditClick", lj_sl_on_edit, NULL }, { "P_AddCueClick", lj_sl_on_addcue, NULL }, { "P_AddBGClick", lj_sl_on_addbg, NULL },
    { "P_Add2510Click", lj_sl_noop, NULL }, { "P_DeleteClick", lj_sl_on_delete, NULL },
    { NULL, NULL, NULL }
};

/* ---- open / close / refresh ---- */

lj_window *lj_seqlist_open(void)
{
    const lj_win_host *h = lj_win_host_get();
    if (lj_sl_win) { if (h && h->show) h->show(h->ud, lj_sl_win, 1); lj_seqlist_rebuild(); return lj_sl_win; }
    lj_sl_win = lj_window_create(&lj_sl_form, lj_sl_bindings);
    if (!lj_sl_win) return NULL;
    lj_sl_close_pending = 0;
    lj_seqlist_mode_set((enum lj_seqlist_mode)lj_sl_mode);
    lj_seqlist_rebuild();
    if (h && h->open && h->open(h->ud, lj_sl_win, 0) != 0) { lj_window_destroy(lj_sl_win); lj_sl_win = NULL; return NULL; }
    return lj_sl_win;
}

void lj_seqlist_close(void)
{
    const lj_win_host *h = lj_win_host_get();
    lj_window *w = lj_sl_win;
    lj_sl_win = NULL; lj_sl_close_pending = 0;
    if (w) {
        if (h && h->close) h->close(h->ud, w);
        lj_window_destroy(w);
    }
    free(lj_sl_rows); free(lj_sl_items); lj_sl_rows = NULL; lj_sl_items = NULL; lj_sl_n = 0;
}

lj_window *lj_seqlist_get(void) { return lj_sl_win; }

void lj_seqlist_refresh(void) { if (lj_sl_close_pending) lj_seqlist_close(); }
