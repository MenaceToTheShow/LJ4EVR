/* lj_delete_files.c - see lj_delete_files.h. Every OnXxx of the five DFMs is bound (ME-16).
 * TListView here has no MultiSelect (ui owner, recorded): the dialogs wrap LV1's vt with a
 * row-select bitset (click = only this row, Ctrl+click toggles, Shift+click extends) and repaint
 * the selected rows navy/white over the base paint, the VCL's MultiSelect look. */
#include "windows/lj_delete_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_genmacro.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_show_dialogs.h"
#include "windows/lj_warnings.h"

enum { LJ_DF_MAX = 9999 };                                     /* the 4-digit record space [F28] */
#define LJ_DF_CONFIRM "Please Confirm.."                       /* TCONFIRMFORM caption */

typedef struct { uint32_t no; char name[31], type[31]; unsigned count; uint8_t sel, used; char line[112]; char mtime[20]; } lj_df_row;   /* mtime: the Modified column (LORD_FIDELITY-12, W3 movement) */
typedef struct { lj_window *win; lj_df_row *rows; const char **items; int n, anchor, sort_col, checked, close_pending; } lj_df_state;
static lj_df_state df[LJ_REC_KIND_COUNT];
static int (*lj_df_deleter)(int kind, uint32_t number);

static const lj_form_desc *const lj_df_forms[LJ_REC_KIND_COUNT] = { &lj_form_TDELETESEQUENCESFORM, &lj_form_TDELETECUESFORM, &lj_form_TDELETECUELISTSFORM, &lj_form_TDELETEBGCUESFORM, &lj_form_TDELETEGENERICMACROSFORM };
static const char *const lj_df_lv[LJ_REC_KIND_COUNT] = { "LV1", "LV1", "lv1", "LV1", "LV1" };   /* TDeleteCuelistsForm spells it lv1 */

static int valid(enum lj_rec_kind k) { return (int)k >= 0 && k < LJ_REC_KIND_COUNT; }
static lj_widget *find(enum lj_rec_kind k, const char *n) { return lj_window_find(df[k].win, n); }
static int kind_of(const lj_widget *w)
{
    lj_window *win = lj_window_of((lj_widget *)w);
    int k;
    for (k = 0; k < LJ_REC_KIND_COUNT; k++) if (win && df[k].win == win) return k;
    return -1;
}

/* ---- rows ---- */

static void row_line(enum lj_rec_kind k, lj_df_row *r)
{
    const char *type = r->type;
    unsigned count = r->count;
    switch (k) {
    case LJ_REC_SEQ: snprintf(r->line, sizeof r->line, "%04u\t%s\t%s\t%u\t%s\t%s", (unsigned)r->no, r->name, type, count, r->mtime, r->used ? "Yes" : ""); break;   /* Modified = the file mtime (LORD_FIDELITY-12, lj_fs_mtime_text) */
    case LJ_REC_CUE: snprintf(r->line, sizeof r->line, "%04u\t%s\t%s\t", (unsigned)r->no, r->name, type); break;
    case LJ_REC_CL:  snprintf(r->line, sizeof r->line, "%04u\t%s\t%u\t", (unsigned)r->no, r->name, count); break;
    default:         snprintf(r->line, sizeof r->line, "%04u\t%s", (unsigned)r->no, r->name); break;
    }
}

/* name / type / count of one record from its file (a file that does not load shows its number only) */
static void read_row(enum lj_rec_kind k, lj_df_row *r)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[64], *type = r->type;
    unsigned count = 0;
    r->name[0] = r->type[0] = 0;
    if (lj_library_record_path(c->lib, k, r->no, path, sizeof path) == 0) {
        lj_fs_mtime_text(path, r->mtime, sizeof r->mtime);   /* W3 movement: LORD_FIDELITY-12 */
        if (k == LJ_REC_SEQ) {
            lj_sequence s; memset(&s, 0, sizeof s);
            if (lj_sequence_load(path, &s, err, sizeof err) == 0) { lj_str_copy(r->name, sizeof r->name, s.name); lj_str_copy(type, sizeof r->type, s.type); count = s.n; }
            lj_sequence_free_scenes(&s);
        } else if (k == LJ_REC_CUE) {
            lj_cue *q = lj_cue_new_rec();
            if (lj_cue_load(path, q, err, sizeof err) == 0) { lj_str_copy(r->name, sizeof r->name, q->name); lj_str_copy(type, sizeof r->type, q->type); }
            lj_cue_free(q);
        } else if (k == LJ_REC_CL) {
            lj_cuelist cl; memset(&cl, 0, sizeof cl);
            if (lj_cuelist_load(path, &cl, err, sizeof err) == 0) { lj_str_copy(r->name, sizeof r->name, cl.name); count = cl.n; }
            free(cl.lines);
        } else if (k == LJ_REC_BG) {
            lj_bgcue *b = lj_bgcue_new_rec();
            if (lj_bgcue_load(path, b, err, sizeof err) == 0) lj_str_copy(r->name, sizeof r->name, b->name);
            lj_bgcue_free(b);
        } else {
            lj_genmacro *g = lj_genmacro_new_rec();
            if (lj_genmacro_load(path, g, err, sizeof err) == 0) lj_str_copy(r->name, sizeof r->name, g->name);
            free(g);
        }
    }
    r->count = count;
    row_line(k, r);
}

int lj_delete_files_record_name(enum lj_rec_kind k, uint32_t number, char *name, size_t cap)
{
    lj_df_row r;
    if (!valid(k) || !name || !cap || !lj_ctx_get()->lib) return -1;
    memset(&r, 0, sizeof r); r.no = number;
    read_row(k, &r);
    return lj_str_copy(name, cap, r.name);
}

static int row_cmp_col;
static int row_cmp(const void *pa, const void *pb)
{
    const lj_df_row *a = pa, *b = pb;
    int r = row_cmp_col == 1 ? strcmp(a->name, b->name) : 0;   /* ponytail: Name sorts byte-wise, every other column by number */
    return r ? r : a->no < b->no ? -1 : a->no > b->no;
}

static void enable(lj_widget *w, int on) { if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = (unsigned)(on != 0); lj_widget_invalidate(w); } }

static void show_rows(enum lj_rec_kind k)
{
    lj_widget *lv = find(k, lj_df_lv[k]), *bd = find(k, "BDelete");
    char cap[48];
    int i, n = 0;
    for (i = 0; i < df[k].n; i++) { df[k].items[i] = df[k].rows[i].line; n += df[k].rows[i].sel; }
    if (lv) { lj_w_listview_set_items(lv, df[k].items, df[k].n); lj_widget_invalidate(lv); }
    if (k == LJ_REC_SEQ && bd) { snprintf(cap, sizeof cap, "&Delete (%d) Sequences", n); lj_w_set_caption(bd, cap); }   /* exe 0x1865C8 + 0x1865DC */
    if (k != LJ_REC_CUE) enable(bd, n > 0);                                    /* Enabled = False in the four DFMs until a row is selected (TDeleteCuesForm's is always on) */
    if (k == LJ_REC_SEQ) enable(find(k, "bMark"), df[k].checked);              /* Mark Unused after Check Used Sequences */
}

static void rebuild(enum lj_rec_kind k)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t *nums = calloc(LJ_DF_MAX, sizeof *nums);
    int n, i;
    if (!nums) lj_fatal("delete files: out of memory");
    free(df[k].rows); free(df[k].items); df[k].rows = NULL; df[k].items = NULL; df[k].n = 0; df[k].checked = 0;
    n = c->lib ? lj_library_list_records(c->lib, k, nums, LJ_DF_MAX) : 0;
    if (n > 0) {
        df[k].rows = calloc((size_t)n, sizeof *df[k].rows); df[k].items = calloc((size_t)n, sizeof *df[k].items);
        if (!df[k].rows || !df[k].items) lj_fatal("delete files: out of memory");
        for (i = 0; i < n; i++) { df[k].rows[i].no = nums[i]; read_row(k, &df[k].rows[i]); }
        df[k].n = n;
    }
    free(nums);
    row_cmp_col = df[k].sort_col;
    if (df[k].n > 1) qsort(df[k].rows, (size_t)df[k].n, sizeof *df[k].rows, row_cmp);
    show_rows(k);
}

int lj_delete_files_count(enum lj_rec_kind k) { return valid(k) ? df[k].n : 0; }
uint32_t lj_delete_files_number(enum lj_rec_kind k, int row) { return valid(k) && row >= 0 && row < df[k].n ? df[k].rows[row].no : 0; }
int lj_delete_files_selected(enum lj_rec_kind k, int row) { return valid(k) && row >= 0 && row < df[k].n && df[k].rows[row].sel; }
int lj_delete_files_in_use(int row) { return row >= 0 && row < df[LJ_REC_SEQ].n && df[LJ_REC_SEQ].rows[row].used; }
void lj_delete_files_select(enum lj_rec_kind k, int row, int on) { if (valid(k) && row >= 0 && row < df[k].n) { df[k].rows[row].sel = (uint8_t)(on != 0); show_rows(k); } }
void lj_delete_files_set_deleter(int (*fn)(int kind, uint32_t number)) { lj_df_deleter = fn; }
int lj_delete_files_record(enum lj_rec_kind kind, uint32_t number)
{
    lj_ctx *c = lj_ctx_get();
    if (lj_df_deleter) return lj_df_deleter((int)kind, number);
    return c->lib ? lj_library_delete_record(c->lib, kind, number) : -1;
}

/* ---- Check Used Sequences / Mark Unused Sequences: every cue slot and BG cue slot on disk (TSeqUsedForm "Checking Cues" / "Checking HouseLight Cues"; the second
 * pass is read as the BG cues, PROPOSAL rig: LJ2's "HouseLight" cue kind is undocumented) ---- */
static void mark_used(uint32_t seq)
{
    int i;
    for (i = 0; i < df[LJ_REC_SEQ].n; i++) if (df[LJ_REC_SEQ].rows[i].no == seq) df[LJ_REC_SEQ].rows[i].used = 1;
}

static void check_used(void)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t *nums = calloc(LJ_DF_MAX, sizeof *nums);
    char path[LJ_PATH_CAP], err[64];
    int n, i, s, ncue;
    if (!nums) lj_fatal("delete files: out of memory");
    for (i = 0; i < df[LJ_REC_SEQ].n; i++) df[LJ_REC_SEQ].rows[i].used = 0;
    n = ncue = c->lib ? lj_library_list_records(c->lib, LJ_REC_CUE, nums, LJ_DF_MAX) : 0;
    for (i = 0; i < n; i++) {
        lj_cue *q = lj_cue_new_rec();
        lj_show_dlg_seq_used(i, ncue, 0, 0);                   /* TSeqUsedForm "Checking, please wait.." (dlg-show) */
        if (lj_library_record_path(c->lib, LJ_REC_CUE, nums[i], path, sizeof path) == 0 && lj_cue_load(path, q, err, sizeof err) == 0)
            for (s = 0; s < LJ_CUE_SLOTS; s++) if (q->slot[s].seq) mark_used(q->slot[s].seq);
        lj_cue_free(q);
    }
    n = c->lib ? lj_library_list_records(c->lib, LJ_REC_BG, nums, LJ_DF_MAX) : 0;
    for (i = 0; i < n; i++) {
        lj_bgcue *b = lj_bgcue_new_rec();
        lj_show_dlg_seq_used(ncue, ncue, i, n);
        if (lj_library_record_path(c->lib, LJ_REC_BG, nums[i], path, sizeof path) == 0 && lj_bgcue_load(path, b, err, sizeof err) == 0)
            for (s = 0; s < LJ_BG_SLOTS; s++) if (b->slot[s].seq) mark_used(b->slot[s].seq);
        lj_bgcue_free(b);
    }
    free(nums);
    lj_win_close(LJ_WIN_SEQ_USED);
    for (i = 0; i < df[LJ_REC_SEQ].n; i++) row_line(LJ_REC_SEQ, &df[LJ_REC_SEQ].rows[i]);
    df[LJ_REC_SEQ].checked = 1;
    show_rows(LJ_REC_SEQ);
}

/* ---- delete ---- */

int lj_delete_files_prompt(enum lj_rec_kind k, int n, char *l1, size_t c1, char *l2, size_t c2)
{
    int r;
    if (!valid(k) || !l1 || !c1 || !l2 || !c2) return -1;
    l2[0] = 0;
    switch (k) {
    case LJ_REC_SEQ: r = snprintf(l1, c1, "Delete (%d) Sequences ?", n); break;              /* PROPOSAL rig: no sequence prompt in the exe pool; the BDelete caption's words */
    case LJ_REC_CUE: r = n == 1 ? snprintf(l1, c1, "Delete Cue file ?") : snprintf(l1, c1, "Delete %d Cues ?", n); break;   /* exe 0x16401C; 0x163FF8 "Delete " + n + 0x164008 " Cues ?" */
    case LJ_REC_CL:  r = snprintf(l1, c1, "Delete Cuelist File?"); break;                  /* exe 0x1826C4 */
    case LJ_REC_BG:  r = snprintf(l1, c1, "Delete Background Cue files - are you sure ?"); break;   /* exe 0x165574 */
    default:         r = snprintf(l1, c1, "Delete Generic Macro files - are you sure ?"); break;    /* PROPOSAL rig: modelled on the BG prompt, none in the exe pool */
    }
    return r < 0 || (size_t)r >= c1 ? -1 : 0;
}

static void on_confirm(int yes, void *ud)
{
    enum lj_rec_kind k = (enum lj_rec_kind)(intptr_t)ud;
    lj_ctx *c = lj_ctx_get();
    int i, bad = 0;
    if (!yes || !valid(k) || !c->lib) return;
    for (i = 0; i < df[k].n; i++) {
        if (!df[k].rows[i].sel) continue;
        if (lj_delete_files_record(k, df[k].rows[i].no) != 0) bad++;
    }
    if (bad) lj_log("delete files: %d record(s) not deleted", bad);
    if (df[k].win) rebuild(k);
}

static void do_delete(enum lj_rec_kind k)
{
    char l1[96], l2[8];
    int i, n = 0;
    for (i = 0; i < df[k].n; i++) n += df[k].rows[i].sel;
    if (n == 0 || lj_delete_files_prompt(k, n, l1, sizeof l1, l2, sizeof l2) != 0) return;
    lj_win_confirm(LJ_DF_CONFIRM, l1, l2[0] ? l2 : NULL, on_confirm, (void *)(intptr_t)k);
}

/* ---- the multi-select listview wrapper ---- */

static lj_widget_vt lj_df_vt;
static const lj_widget_vt *lj_df_base;

static void ms_paint(lj_widget *w, lj_surface *s)
{
    lj_w_listview *v = (lj_w_listview *)w;
    int k = kind_of(w), keep = v->index, ax, ay, i, x, y, top, area;
    v->index = -1;                                             /* the base paint draws no selection; the bitset is painted below */
    lj_df_base->paint(w, s);
    v->index = keep;
    if (k < 0) return;
    lj_widget_abs(w, &ax, &ay);
    top = 2 + (v->show_headers ? v->header_h : 0);
    area = w->w - 4 - (v->n * v->row_h > w->h - top - 2 ? LJ_SB_W : 0);
    for (i = v->sb.pos; i < v->n && i < df[k].n; i++) {
        int ry = ay + top + (i - v->sb.pos) * v->row_h;
        if (ry + v->row_h > ay + w->h - 2) break;
        if (!df[k].rows[i].sel) continue;
        for (y = ry; y < ry + v->row_h; y++)
            for (x = ax + 2; x < ax + 2 + area; x++) {
                uint32_t *p;
                if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
                p = &s->px[y * s->w + x];
                *p = *p == w->color ? LJ_CLR_HIGHLIGHT : *p == LJ_CLR_3DDKSHADOW ? LJ_CLR_BTNHIGHLIGHT : *p;   /* clWindow -> clHighlight, clWindowText -> clHighlightText */
            }
    }
}

static int ms_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_listview *v = (lj_w_listview *)w;
    int k = kind_of(w), row, i, a, b;
    if (k < 0 || m->type != LJ_EV_MOUSE_DOWN || m->button != 1 || m->x >= w->w - 2 - LJ_SB_W * (v->n * v->row_h > w->h - 21)) return lj_df_base->mouse(w, m);
    row = lj_w_listview_row_at(w, m->y);
    if (row < 0 || row >= df[k].n) return lj_df_base->mouse(w, m);
    lj_widget_set_focus(w);
    if (m->shift && df[k].anchor >= 0) {
        a = df[k].anchor < row ? df[k].anchor : row; b = df[k].anchor < row ? row : df[k].anchor;
        for (i = 0; i < df[k].n; i++) df[k].rows[i].sel = (uint8_t)(i >= a && i <= b);
    } else if (m->ctrl) { df[k].rows[row].sel ^= 1; df[k].anchor = row; }
    else { for (i = 0; i < df[k].n; i++) df[k].rows[i].sel = (uint8_t)(i == row); df[k].anchor = row; }
    v->index = row;
    show_rows((enum lj_rec_kind)k);
    lj_w_emit(w, LJ_WEV_CHANGE); lj_w_emit(w, LJ_WEV_SELECT_ITEM); lj_w_emit(w, LJ_WEV_CLICK);
    return 1;
}

static void wrap_lv(enum lj_rec_kind k)
{
    lj_widget *lv = find(k, lj_df_lv[k]);
    if (!lv) return;
    if (!lj_df_base) { lj_df_base = lv->vt; lj_df_vt = *lv->vt; lj_df_vt.paint = ms_paint; lj_df_vt.mouse = ms_mouse; }
    lv->vt = &lj_df_vt;
}

/* ---- handlers ---- */

static void on_close(lj_widget *w, void *ud) { int k = kind_of(w); (void)ud; if (k >= 0) df[k].close_pending = 1; }
static void on_select_all(lj_widget *w, void *ud) { int k = kind_of(w), i; (void)ud; if (k < 0) return; for (i = 0; i < df[k].n; i++) df[k].rows[i].sel = 1; show_rows((enum lj_rec_kind)k); }
static void on_clear_all(lj_widget *w, void *ud) { int k = kind_of(w), i; (void)ud; if (k < 0) return; for (i = 0; i < df[k].n; i++) df[k].rows[i].sel = 0; show_rows((enum lj_rec_kind)k); }
static void on_delete(lj_widget *w, void *ud) { int k = kind_of(w); (void)ud; if (k >= 0) do_delete((enum lj_rec_kind)k); }
static void on_check(lj_widget *w, void *ud) { (void)w; (void)ud; check_used(); }
static void on_mark(lj_widget *w, void *ud)
{
    int i;
    (void)w; (void)ud;
    if (!df[LJ_REC_SEQ].checked) check_used();                 /* Mark Unused runs the check first when it has not run */
    for (i = 0; i < df[LJ_REC_SEQ].n; i++) df[LJ_REC_SEQ].rows[i].sel = (uint8_t)!df[LJ_REC_SEQ].rows[i].used;
    show_rows(LJ_REC_SEQ);
}
static void on_column(lj_widget *w, void *ud)
{
    int k = kind_of(w);
    (void)ud;
    if (k < 0) return;
    df[k].sort_col = ((lj_w_listview *)w)->clicked_col;
    row_cmp_col = df[k].sort_col;
    if (df[k].n > 1) qsort(df[k].rows, (size_t)df[k].n, sizeof *df[k].rows, row_cmp);
    show_rows((enum lj_rec_kind)k);
}
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("delete files: Help is W4"); }

static const lj_binding lj_df_seq_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "LV1Change", lj_handler_noop, NULL }, { "LV1ColumnClick", on_column, NULL }, { "LV1Compare", lj_handler_noop, NULL },
    { "BCheckClick", on_check, NULL }, { "bMarkClick", on_mark, NULL }, { "BSelectAllClick", on_select_all, NULL }, { "BClearAllClick", on_clear_all, NULL },
    { "BDeleteClick", on_delete, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_df_cue_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "LV1Click", lj_handler_noop, NULL }, { "LV1ColumnClick", on_column, NULL }, { "LV1Compare", lj_handler_noop, NULL },
    { "BSelectAllClick", on_select_all, NULL }, { "BClearAllClick", on_clear_all, NULL }, { "BDeleteClick", on_delete, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_df_cl_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "lv1Change", lj_handler_noop, NULL },
    { "BSelectAllClick", on_select_all, NULL }, { "BClearAllClick", on_clear_all, NULL }, { "BDeleteClick", on_delete, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_df_bg_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "LV1Change", lj_handler_noop, NULL },
    { "BSelectAllClick", on_select_all, NULL }, { "BClearAllClick", on_clear_all, NULL }, { "BDeleteClick", on_delete, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_df_macro_b[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "LV1SelectItem", lj_handler_noop, NULL },
    { "BSelectAllClick", on_select_all, NULL }, { "BClearAllClick", on_clear_all, NULL }, { "BDeleteClick", on_delete, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding *const lj_df_bindings[LJ_REC_KIND_COUNT] = { lj_df_seq_b, lj_df_cue_b, lj_df_cl_b, lj_df_bg_b, lj_df_macro_b };

/* ---- open / close ---- */

lj_window *lj_delete_files_open(enum lj_rec_kind k)
{
    if (!valid(k)) return NULL;
    if (df[k].win) return df[k].win;
    df[k].win = lj_window_create(lj_df_forms[k], lj_df_bindings[k]);
    if (!df[k].win) { lj_log("delete files: cannot create %s", lj_df_forms[k]->class_name); return NULL; }
    df[k].anchor = -1; df[k].close_pending = 0;
    wrap_lv(k);
    rebuild(k);
    lj_win_show(df[k].win, 0);
    return df[k].win;
}

void lj_delete_files_close(enum lj_rec_kind k)
{
    if (!valid(k) || !df[k].win) return;
    lj_win_hide(df[k].win);
    lj_window_destroy(df[k].win);
    free(df[k].rows); free(df[k].items);
    memset(&df[k], 0, sizeof df[k]);
}

lj_window *lj_delete_files_window(enum lj_rec_kind k) { return valid(k) ? df[k].win : NULL; }
void lj_delete_files_refresh(enum lj_rec_kind k) { if (valid(k) && df[k].close_pending) lj_delete_files_close(k); }

#define LJ_DF_ROW(fn, K) \
    lj_window *lj_delete_##fn##_open(void) { return lj_delete_files_open(K); } void lj_delete_##fn##_close(void) { lj_delete_files_close(K); } \
    lj_window *lj_delete_##fn##_get(void) { return lj_delete_files_window(K); } void lj_delete_##fn##_refresh(void) { lj_delete_files_refresh(K); }
LJ_DF_ROW(seq, LJ_REC_SEQ)
LJ_DF_ROW(cue, LJ_REC_CUE)
LJ_DF_ROW(cl, LJ_REC_CL)
LJ_DF_ROW(bg, LJ_REC_BG)
LJ_DF_ROW(macro, LJ_REC_MACRO)
