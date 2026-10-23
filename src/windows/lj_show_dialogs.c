/* lj_show_dialogs.c - W4 family SHOW (lj_show_dialogs.h): the sequence, cue, cue-list, statics and movement dialogs.
 * One table of open windows indexed by enum lj_win; each id builds its DFM form with every OnXxx bound (ME-16). bkOK /
 * bkCancel buttons carry no OnClick in the DFMs, so their on_event is hooked here (the W2 pattern). The cue list
 * operations (Offset / Sort, ME-120) run on the UI thread like the cue list itself (design 2.3 rule 4, ME-2); nothing
 * here touches the engine except through lj_ctx_post / the cue form's repin. */
#include "windows/lj_show_dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_numedit.h"
#include "ui/widgets/lj_w_radiobutton.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "ui/widgets/lj_w_updown.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_cuelist_form.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_lists.h"
#include "windows/lj_movemacro_editor.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_statics2_form.h"
#include "windows/lj_warnings.h"
#include "controls/lj_ctl.h"

#define LJ_SD_ON_TOP   2u          /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */
#define LJ_SD_PICK_MAX 9999        /* a library holds at most 9999 records per kind (lj_table.h) */
#define LJ_SD_CAPTURE_RGB 0xFFA54Au   /* "Capture Activated" panels: measured from cuebuilder.gif, reduced, PROPOSAL rig 03 s5.8 */
#define LJ_SD_STACK_MAX 64         /* PROPOSAL: "no limit to the number of transparent cues" [CHM The cue builder]; 64 kept */

static lj_window *sd_win[LJ_WIN_COUNT];
static uint8_t sd_shown[LJ_WIN_COUNT];                          /* lj_win_show once per open (a second host open would be a second native window) */
static lj_ctx *C(void) { return lj_ctx_get(); }
static lj_widget *in(enum lj_win id, const char *n) { return sd_win[id] ? lj_window_find(sd_win[id], n) : NULL; }
static void enable(lj_widget *w, int on) { if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = on != 0; lj_widget_invalidate(w); } }
static int edit_int(lj_widget *w, int dflt) { const char *t = lj_w_edit_text(w); return t && strspn(t, " ") < strlen(t) ? atoi(t) : dflt; }
static void set_int(lj_widget *w, const char *fmt, int v) { char b[16]; snprintf(b, sizeof b, fmt, v); lj_w_edit_set_text(w, b); }
static void hook(enum lj_win id, const char *name, void (*fn)(lj_widget *w, int ev, void *ud), void *ud) { lj_widget *w = in(id, name); if (w) { w->on_event = fn; w->ud = ud; } }
static void help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_help_viewer_topic(NULL); }   /* every Help item: the bundled viewer's index (W4-HELP) */

static lj_window *mk(enum lj_win id, const lj_form_desc *d, const lj_binding *b)
{
    if (sd_win[id]) return sd_win[id];
    if ((sd_win[id] = lj_window_create(d, b)) == NULL) { lj_log("show dialogs: %s failed to build", d->class_name); return NULL; }
    return sd_win[id];
}
static lj_window *shown(enum lj_win id)
{
    const lj_form_desc *d = sd_win[id] ? lj_window_desc(sd_win[id]) : NULL;
    if (d && !sd_shown[id]) { sd_shown[id] = 1; lj_win_show(sd_win[id], d->style == LJ_FS_STAYONTOP ? LJ_SD_ON_TOP : 0u); }
    return sd_win[id];
}
static void closer(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) lj_show_dlg_close((enum lj_win)(intptr_t)ud); }
static void close_ev(lj_widget *w, void *ud) { lj_window *win = lj_window_of(w); int id; (void)ud; for (id = 0; id < LJ_WIN_COUNT; id++) if (sd_win[id] && sd_win[id] == win) { lj_show_dlg_close((enum lj_win)id); return; } }

/* ==================================================================================== Delete Scene(s) [03 s4.3] */
#define DS LJ_WIN_DELETE_SCENE
static void ds_ok(lj_widget *w, void *ud)
{
    lj_sequence *s = C()->show ? C()->show->current_seq : NULL;
    int from = edit_int(in(DS, "eFrom"), 0), to = edit_int(in(DS, "eTo"), from);
    (void)w; (void)ud;
    if (s && from >= 1 && to >= 1) lj_seq_form_delete_range(from - 1, to - 1);
    lj_show_dlg_close(DS);
}
static void ds_pref(lj_widget *w, void *ud)   /* Preferences > Ok on [Enter]: forms.ini [DELETESCENEFORM] OkOnEnter (PROPOSAL key) */
{
    lj_ini *fi = C()->forms_ini;
    (void)w; (void)ud;
    if (fi) lj_ini_seti(fi, "DELETESCENEFORM", "OkOnEnter", !lj_ini_geti(fi, "DELETESCENEFORM", "OkOnEnter", 0));
    lj_menu_set_checked(lj_window_menus(sd_win[DS]), "DeleteonEnter", lj_ini_geti(fi, "DELETESCENEFORM", "OkOnEnter", 0));
}
static void ds_open(void)
{
    static const lj_binding b[] = { { "BOkClick", ds_ok, NULL }, { "DeleteonEnterClick", ds_pref, NULL }, { "help1Click", help, NULL },
        { "FormActivate", lj_handler_noop, NULL }, { "FormClose", close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
        { "FormKeyDown", lj_handler_noop, NULL } /* ponytail: lj_window hands no key to form handlers; Enter = the focused button */, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    int cur = C()->current_scene + 1;
    if (!mk(DS, &lj_form_TDELETESCENEFORM, b)) return;
    hook(DS, "bCancel", closer, (void *)(intptr_t)DS);
    set_int(in(DS, "eFrom"), "%d", cur); set_int(in(DS, "eTo"), "%d", cur);   /* deletescenesdialog.gif: the current scene in both */
    lj_menu_set_checked(lj_window_menus(sd_win[DS]), "DeleteonEnter", lj_ini_geti(C()->forms_ini, "DELETESCENEFORM", "OkOnEnter", 0));
}

/* ==================================================================================== Insert Scene(s) [03 s4.3, F29] */
#define IS LJ_WIN_INSERT_SCENES
static lj_sequence *is_src;     /* Copy from Sequence: the picked sequence, read from disk (outside the lock, design 3.1) */
static int is_mode(void) { return lj_w_radiobutton_checked(in(IS, "CopyFromSeq")) ? 2 : lj_w_radiobutton_checked(in(IS, "CopyFromCurrent")) ? 1 : 0; }
static void is_sync(void)
{
    int m = is_mode();
    enable(in(IS, "eSceneCount"), m == 0); enable(in(IS, "eCurrentFrom"), m == 1); enable(in(IS, "eCurrentTo"), m == 1);
    enable(in(IS, "sbselectseq"), m == 2); enable(in(IS, "ESeqFrom"), m == 2); enable(in(IS, "ESeqTo"), m == 2);
}
static void is_radio(lj_widget *w, void *ud) { (void)w; (void)ud; is_sync(); }
static void is_mask(lj_widget *w, void *ud) { (void)ud; lj_w_edit_select(w, 0, -1); }   /* MaskSelect */
static int is_pick(uint32_t number)
{
    lj_ctx *c = C();
    char path[LJ_PATH_CAP], err[128], t[48];
    lj_sequence *rec;
    if (!number || !c->lib || lj_library_record_path(c->lib, LJ_REC_SEQ, number, path, sizeof path) != 0) return -1;
    rec = lj_sequence_new_rec();
    if (lj_sequence_load(path, rec, err, sizeof err) != 0) { lj_log("insert scenes: %s: %s", path, err); lj_sequence_free(rec); return -1; }
    lj_sequence_free(is_src);
    is_src = rec;
    snprintf(t, sizeof t, "%04u %s", (unsigned)number, rec->name);
    lj_w_edit_set_text(in(IS, "ESeq"), t);
    set_int(in(IS, "ESeqFrom"), "%3d", 1); set_int(in(IS, "ESeqTo"), "%3d", rec->n);
    return 0;
}
int lj_show_dlg_insert_pick(uint32_t number) { return sd_win[IS] ? is_pick(number) : -1; }
static void is_select(lj_widget *w, void *ud)   /* Select seq: the sequence highlighted in the List of Sequences (opened when closed) */
{
    int row = lj_seqlist_get() ? lj_seqlist_selected() : -1;
    (void)w; (void)ud;
    if (row < 0) { lj_win_open(LJ_WIN_LIST_SEQ); lj_log("insert scenes: highlight a sequence in the List of Sequences, then Select seq"); return; }
    is_pick(lj_seqlist_number_at(row));
}
static void is_ok(lj_widget *w, void *ud)
{
    int m = is_mode(), rc;
    (void)w; (void)ud;
    if (m == 0) rc = lj_seq_form_insert_copies(edit_int(in(IS, "eSceneCount"), 1));
    else if (m == 1) rc = lj_seq_form_insert_from(NULL, edit_int(in(IS, "eCurrentFrom"), 1) - 1, edit_int(in(IS, "eCurrentTo"), 1) - 1);
    else rc = is_src ? lj_seq_form_insert_from(is_src, edit_int(in(IS, "ESeqFrom"), 1) - 1, edit_int(in(IS, "ESeqTo"), 1) - 1) : -1;
    if (rc != 0) { lj_log("insert scenes: nothing inserted (mode %d)", m); return; }
    lj_show_dlg_close(IS);
}
static void is_open(void)
{
    static const lj_binding b[] = { { "sbSelectSeqClick", is_select, NULL }, { "CopyCurrentClick", is_radio, NULL }, { "CopyFromCurrentClick", is_radio, NULL },
        { "CopyFromSeqClick", is_radio, NULL }, { "MaskSelect", is_mask, NULL }, { "BOkClick", is_ok, NULL }, { "Help1Click", help, NULL },
        { "FormActivate", lj_handler_noop, NULL }, { "FormClose", close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
        { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    lj_sequence *s = C()->show ? C()->show->current_seq : NULL;
    if (!mk(IS, &lj_form_TINSERTSCENESFORM, b)) return;
    hook(IS, "Bcancel", closer, (void *)(intptr_t)IS);
    set_int(in(IS, "eSceneCount"), "%3d", 1);
    set_int(in(IS, "eCurrentFrom"), "%3d", 1); set_int(in(IS, "eCurrentTo"), "%3d", s && s->n ? s->n : 1);
    is_sync();
}

/* ==================================================================================== the three pickers: Select Cue (TModalSelectCueForm),
 * Select Background Cue (TSelectBGCueForm), Select Startup Cuelist (TSelectCuelist2ModalForm). OK (greyed until a row is highlighted,
 * as the DFMs) hands the number to the caller's callback; Cancel / close hands nothing */
typedef struct { enum lj_win id; const lj_form_desc *form; const char *lv; enum lj_rec_kind kind;
                 void (*done)(uint32_t, void *); void *ud; uint32_t *no; char (*row)[48]; const char **rows; int n; } lj_sd_picker;
static lj_sd_picker sd_pk[3] = { { LJ_WIN_MODAL_SELECT_CUE, &lj_form_TMODALSELECTCUEFORM, "lvCues", LJ_REC_CUE, NULL, NULL, NULL, NULL, NULL, 0 },
                                 { LJ_WIN_SELECT_BGCUE_OLD, &lj_form_TSELECTBGCUEFORM, "lv1", LJ_REC_BG, NULL, NULL, NULL, NULL, NULL, 0 },
                                 { LJ_WIN_SELECT_CUELIST_MODAL, &lj_form_TSELECTCUELIST2MODALFORM, "Lv1", LJ_REC_CL, NULL, NULL, NULL, NULL, NULL, 0 } };
static lj_sd_picker *pk_of(enum lj_win id) { int i; for (i = 0; i < 3; i++) if (sd_pk[i].id == id) return &sd_pk[i]; return NULL; }
static void pk_row(lj_sd_picker *p, int i)
{
    lj_ctx *c = C();
    char path[LJ_PATH_CAP], err[128];
    p->row[i][0] = 0;
    if (lj_library_record_path(c->lib, p->kind, p->no[i], path, sizeof path) != 0) return;
    if (p->kind == LJ_REC_CUE) {
        static lj_cue q;
        int ok = lj_cue_load(path, &q, err, sizeof err) == 0;
        snprintf(p->row[i], sizeof p->row[i], "%s%04u\t%s", ok && q.transparent ? "(T)" : "", (unsigned)p->no[i], ok ? q.name : "load error");   /* "(T)" as the List of Cues [03 s5.8] */
    } else if (p->kind == LJ_REC_BG) {
        static lj_bgcue b;
        int ok = lj_bgcue_load(path, &b, err, sizeof err) == 0;
        snprintf(p->row[i], sizeof p->row[i], "%s%02u\t%s", ok && b.transparent ? "(T)" : "", (unsigned)p->no[i], ok ? b.name : "load error");   /* listofbgcues.gif: two digits */
    } else {
        lj_cuelist l;
        memset(&l, 0, sizeof l);
        if (lj_cuelist_load(path, &l, err, sizeof err) == 0) snprintf(p->row[i], sizeof p->row[i], "%04u\t%s\t%u", (unsigned)p->no[i], l.name, (unsigned)l.n);
        else snprintf(p->row[i], sizeof p->row[i], "%04u\tload error\t", (unsigned)p->no[i]);
        free(l.lines);
    }
}
static void pk_ok(lj_widget *w, int ev, void *ud)
{
    lj_sd_picker *p = (lj_sd_picker *)ud;
    int i = lj_w_listview_index(in(p->id, p->lv));
    void (*done)(uint32_t, void *) = p->done; void *dud = p->ud;
    uint32_t n = i >= 0 && i < p->n ? p->no[i] : 0;
    (void)w;
    if (ev != LJ_WEV_CLICK || !n) return;
    p->done = NULL;
    lj_show_dlg_close(p->id);
    if (done) done(n, dud);
}
static void pk_change(lj_widget *w, void *ud) { (void)w; (void)ud; lj_show_dlg_refresh(LJ_WIN_MODAL_SELECT_CUE); lj_show_dlg_refresh(LJ_WIN_SELECT_BGCUE_OLD); lj_show_dlg_refresh(LJ_WIN_SELECT_CUELIST_MODAL); }
static void pk_open(lj_sd_picker *p, void (*done)(uint32_t, void *), void *ud)
{
    static const lj_binding bcue[] = { { "lvCuesChange", pk_change, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL } /* aligned */, { NULL, NULL, NULL } };
    static const lj_binding bbg[] = { { "lv1Change", pk_change, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    static const lj_binding bcl[] = { { "Lv1Change", pk_change, NULL }, { "FormCreate", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    const lj_binding *b = p->kind == LJ_REC_CUE ? bcue : p->kind == LJ_REC_BG ? bbg : bcl;
    lj_ctx *c = C();
    int i, n;
    static lj_form_desc nobar;                                   /* TSelectBGCueForm's MainMenu1 has no items: Windows shows no bar for an empty menu, so the
                                                                  * DFM's 251-px client keeps its OK / Cancel panel. ponytail: a local copy; the root fix is one line in
                                                                  * lj_window.c (menu_h = 0 for a bar without items), integrator */
    const lj_form_desc *form = p->form;
    if (sd_win[p->id]) { p->done = done; p->ud = ud; return; }
    if (form->menu) { const lj_menu_desc *bar = lj_form_find_menu(form, form->menu); if (bar && bar->n == 0) { nobar = *form; nobar.menu = NULL; form = &nobar; } }
    if (!mk(p->id, form, b)) return;
    if (form == &nobar) {                                        /* the outer Width / Height of the DFM less the frame and caption only */
        const lj_window_caption_state *cs = lj_window_caption(sd_win[p->id]);
        lj_window_resize(sd_win[p->id], form->width - 2 * cs->frame_w, form->height - 2 * cs->frame_w - cs->caption_h);
    }
    p->done = done; p->ud = ud;
    p->no = (uint32_t *)calloc(LJ_SD_PICK_MAX, sizeof *p->no);
    n = c->lib && p->no ? lj_library_list_records(c->lib, p->kind, p->no, LJ_SD_PICK_MAX) : 0;
    p->n = n < 0 ? 0 : n;
    p->row = (char (*)[48])calloc((size_t)(p->n ? p->n : 1), sizeof *p->row);
    p->rows = (const char **)calloc((size_t)(p->n ? p->n : 1), sizeof *p->rows);
    if (!p->no || !p->row || !p->rows) lj_fatal("show dialogs: out of memory");
    for (i = 0; i < p->n; i++) { pk_row(p, i); p->rows[i] = p->row[i]; }
    lj_w_listview_set_items(in(p->id, p->lv), p->rows, p->n);
    hook(p->id, "bOk", pk_ok, p);
    hook(p->id, "bCancel", closer, (void *)(intptr_t)p->id);
}
static void pk_free(lj_sd_picker *p) { free(p->no); free(p->row); free(p->rows); p->no = NULL; p->row = NULL; p->rows = NULL; p->n = 0; p->done = NULL; p->ud = NULL; }
void lj_show_dlg_select_cue(void (*done)(uint32_t, void *), void *ud) { pk_open(&sd_pk[0], done, ud); shown(LJ_WIN_MODAL_SELECT_CUE); }
void lj_show_dlg_select_bgcue(void (*done)(uint32_t, void *), void *ud) { pk_open(&sd_pk[1], done, ud); shown(LJ_WIN_SELECT_BGCUE_OLD); }
void lj_show_dlg_select_cuelist(void (*done)(uint32_t, void *), void *ud) { pk_open(&sd_pk[2], done, ud); shown(LJ_WIN_SELECT_CUELIST_MODAL); }

/* ==================================================================================== Start Sequence at (TSetSequenceForm): the Global Patch's
 * sequential patch of a channel range (the form sits in the Global Patch unit of the exe, beside bSet / lPatchedOut [13 exe class strings]) */
#define SS LJ_WIN_SET_SEQUENCE
static struct { void (*done)(int, void *); void *ud; } ss;
static void ss_ok(lj_widget *w, int ev, void *ud)
{
    void (*done)(int, void *) = ss.done; void *dud = ss.ud;
    int v = lj_w_numedit_value(in(SS, "E1"));
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    ss.done = NULL;
    lj_show_dlg_close(SS);
    if (done) done(v, dud);
}
void lj_show_dlg_set_sequence(int channels, void (*done)(int start, void *ud), void *ud)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    char t[16];
    if (!mk(SS, &lj_form_TSETSEQUENCEFORM, b)) return;
    ss.done = done; ss.ud = ud;
    snprintf(t, sizeof t, "%d", channels);
    lj_window_set_text(sd_win[SS], "Label3", t);                 /* "Channels to Patch : " + the count (the DFM's Label3 placeholder) */
    hook(SS, "BitBtn1", ss_ok, NULL);
    hook(SS, "BitBtn2", closer, (void *)(intptr_t)SS);
    shown(SS);
}

/* ==================================================================================== Checking, please wait.. (TSeqUsedForm) */
void lj_show_dlg_seq_used(int cues_done, int cues_total, int bg_done, int bg_total)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    lj_widget *p;
    if (!sd_win[LJ_WIN_SEQ_USED] && (!mk(LJ_WIN_SEQ_USED, &lj_form_TSEQUSEDFORM, b) || !shown(LJ_WIN_SEQ_USED))) return;
    if ((p = in(LJ_WIN_SEQ_USED, "Pg1")) != NULL) { ((lj_w_progressbar *)p)->max = cues_total > 0 ? cues_total : 1; lj_w_progressbar_set(p, cues_done); }
    if ((p = in(LJ_WIN_SEQ_USED, "Pg2")) != NULL) { ((lj_w_progressbar *)p)->max = bg_total > 0 ? bg_total : 1; lj_w_progressbar_set(p, bg_done); }
}

/* ==================================================================================== Sequence Slot Labels (TNameSlotsForm) [03 s5]: forms.ini [CUE]
 * SlotName0..11 (the shipped keys, 06 s3); the label shows in an empty cue slot as "(name)" (seqslotlabel.gif). The grid's goEditing editor is
 * a code-built TEdit over the selected cell (the string grid paints only). */
#define NS LJ_WIN_NAME_SLOTS
static struct { char names[LJ_CUE_SLOTS][64], fixed[LJ_CUE_SLOTS][20]; lj_widget *ed; int row, colw[2]; } ns;
static const lj_ctl_desc ns_ed_desc = { LJ_K_EDIT, "ns_editor", 0, 0, 0, 0, .max_length = 63 };   /* ours: 63 chars, as the Statics name fields; PROPOSAL rig 03 s5 */
static void ns_commit(void)
{
    lj_widget *g = in(NS, "sg1");
    if (!ns.ed || ns.row < 0 || ns.row >= LJ_CUE_SLOTS || !g) return;
    lj_str_copy(ns.names[ns.row], sizeof ns.names[ns.row], lj_w_edit_text(ns.ed));
    lj_w_grid_set_cell(g, 1, ns.row, ns.names[ns.row]);
}
static void ns_place(int row)
{
    lj_widget *g = in(NS, "sg1");
    lj_rect r;
    if (!g || !ns.ed || row < 0 || row >= LJ_CUE_SLOTS || lj_w_grid_cell_rect(g, 1, row, &r) != 0) return;
    ns_commit();
    ns.row = row; ((lj_w_grid *)g)->row = row; ((lj_w_grid *)g)->col = 1;   /* the editor sits on the selected cell */
    ns.ed->x = r.x; ns.ed->y = r.y; ns.ed->w = r.w; ns.ed->h = r.h; ns.ed->visible = 1;
    lj_w_edit_set_text(ns.ed, ns.names[row]);
    lj_w_edit_select(ns.ed, 0, -1);
    lj_widget_set_focus(ns.ed);
    lj_widget_invalidate(g);
}
static void ns_grid(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_SELECT_CELL || ev == LJ_WEV_CLICK) ns_place(((lj_w_grid *)w)->row); }
static void ns_ok(lj_widget *w, void *ud)
{
    char key[16];
    int i;
    (void)w; (void)ud;
    ns_commit();
    for (i = 0; i < LJ_CUE_SLOTS && C()->forms_ini; i++) { snprintf(key, sizeof key, "SlotName%d", i); lj_ini_sets(C()->forms_ini, "CUE", key, ns.names[i]); }
    lj_show_dlg_close(NS);
    lj_cue_form_refresh();
}
static void ns_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_show_dlg_close(NS); }
int lj_show_dlg_name_slot(int slot, const char *name)   /* tests and the script: slot 1..12 typed into the open dialog */
{
    if (!sd_win[NS] || slot < 1 || slot > LJ_CUE_SLOTS) return -1;
    ns_place(slot - 1);
    lj_w_edit_set_text(ns.ed, name ? name : "");
    return 0;
}
static void ns_open(void)
{
    static const lj_binding b[] = { { "bOkClick", ns_ok, NULL }, { "bCancelClick", ns_cancel, NULL }, { "Help1Click", help, NULL },
        { "FormActivate", lj_handler_noop, NULL }, { "FormClose", close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
        { "FormResize", lj_handler_noop, NULL } /* aligned */, { NULL, NULL, NULL } };
    lj_widget *g;
    char key[16];
    int i;
    if (!mk(NS, &lj_form_TNAMESLOTSFORM, b) || (g = in(NS, "sg1")) == NULL) return;
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        snprintf(key, sizeof key, "SlotName%d", i);
        lj_str_copy(ns.names[i], sizeof ns.names[i], lj_ini_gets(C()->forms_ini, "CUE", key, ""));
        snprintf(ns.fixed[i], sizeof ns.fixed[i], "Slot #%d", i + 1);   /* seqslotlabeldialog.gif */
        lj_w_grid_set_cell(g, 0, i, ns.fixed[i]); lj_w_grid_set_cell(g, 1, i, ns.names[i]);
    }
    ns.colw[0] = 50; ns.colw[1] = g->w - 50 - 5;               /* PROPOSAL rig 03 s5: seqslotlabeldialog.gif (reduced) keeps the Slot # column narrow, the label column takes the rest */
    ((lj_w_grid *)g)->col_w = ns.colw;
    g->on_event = ns_grid; g->ud = NULL;
    ns.ed = lj_w_edit_create(g, &ns_ed_desc);
    ns.row = -1;
    ns_place(((lj_w_grid *)g)->row >= 0 ? ((lj_w_grid *)g)->row : 0);
}

/* ==================================================================================== Offset TimeCode (TTimeCodeOffsetForm) and Sort (TSortTimeCodeForm),
 * ME-120: the MIDI and PC families are alive, CD / DA / MP stay greyed with the hint (ME-11, design 8.5). No multi-select in our list view
 * (docs/fidelity.md): Offset covers every line of the family; Sort covers every Wait of the type, which must be one block */
#define TO LJ_WIN_TIMECODE_OFFSET
static int to_family = LJ_CLF_MTC, to_fps = 25;
static int32_t frames_ms(int f, int fps) { return f >= 0 ? (int32_t)lj_cl_hmsf_ms(0, 0, 0, f, fps) : -(int32_t)lj_cl_hmsf_ms(0, 0, 0, -f, fps); }
int32_t lj_show_dlg_offset_ms(int h, int m, int s, int f, int fps) { return h * 3600000 + m * 60000 + s * 1000 + frames_ms(f, fps); }
int lj_show_dlg_offset(lj_cuelist *cl, int family, int32_t delta_ms, int apply, int *nlines)
{
    uint32_t i;
    int over = 0, n = 0;
    if (!cl || (family != LJ_CLF_MTC && family != LJ_CLF_PC)) return -1;
    for (i = 0; i < cl->n; i++) {
        lj_cl_line *l = &cl->lines[i];
        int fps = l->param[0] ? atoi(l->param) : 25;
        int64_t v, hi;
        if (l->cmd != (family == LJ_CLF_MTC ? LJ_CL_WAIT_MTC : LJ_CL_WAIT_PC_CLOCK)) continue;
        if (fps != 24 && fps != 30) fps = 25;
        hi = family == LJ_CLF_MTC ? (int64_t)lj_cl_hmsf_ms(23, 59, 59, fps - 1, fps) : 86399000;   /* H 0-23 [03 s6.3] */
        v = (int64_t)l->param_ms + delta_ms;
        n++;
        if (v < 0 || v > hi) { over++; v = v < 0 ? 0 : hi; }   /* "adjusted to maximum or minimum values" [CHM Offset Time code] */
        if (apply) l->param_ms = (uint32_t)v;
    }
    if (nlines) *nlines = n;
    return over;
}
static int32_t to_delta(void)
{
    if (to_family == LJ_CLF_PC) return lj_show_dlg_offset_ms(edit_int(in(TO, "ePCHours"), 0), edit_int(in(TO, "ePCMinutes"), 0), edit_int(in(TO, "ePCSeconds"), 0), 0, 100);
    return lj_show_dlg_offset_ms(edit_int(in(TO, "eMIDIHours"), 0), edit_int(in(TO, "eMIDIMinutes"), 0), edit_int(in(TO, "eMIDISeconds"), 0), edit_int(in(TO, "eMIDIFrames"), 0), to_fps);
}
static void to_check(lj_widget *w, void *ud)   /* bCheck "Check Changes": exe 0x171224.. "Overflow/UnderFlow (" n " Lines)", "Offsets Ok", "No Lines to Check" */
{
    int n = 0, over = lj_show_dlg_offset(lj_cuelist_form_list(), to_family, to_delta(), 0, &n);
    char t[64];
    (void)w; (void)ud;
    if (!n) lj_win_message("Offset TimeCode", "No Lines to Check");
    else if (over > 0) { snprintf(t, sizeof t, "Overflow/UnderFlow (%d Lines)", over); lj_win_message("Offset TimeCode", t); }
    else lj_win_message("Offset TimeCode", "Offsets Ok");     /* four of the five checks spell "Ok" (exe pool); the box caption is ours, PROPOSAL rig 03 s6.4 */
}
static void to_apply_yes(int yes, void *ud)
{
    (void)ud;
    if (!yes) return;
    lj_show_dlg_offset(lj_cuelist_form_list(), to_family, to_delta(), 1, NULL);
    lj_ctx_set_modified(LJ_MOD_CL, 1);
    lj_cuelist_form_refresh();
    lj_show_dlg_close(TO);
}
static void to_apply(lj_widget *w, void *ud)   /* bOk "Apply Changes": exe 0x1728DC "Nothing to Modify", 0x1728F8 "Overflow/UnderFlow values will be adjusted" */
{
    int n = 0, over = lj_show_dlg_offset(lj_cuelist_form_list(), to_family, to_delta(), 0, &n);
    (void)w; (void)ud;
    if (!n || !to_delta()) { lj_win_message("Offset TimeCode", "Nothing to Modify"); return; }
    if (over > 0) lj_win_confirm("Offset TimeCode", "Overflow/UnderFlow values will be adjusted", NULL, to_apply_yes, NULL);
    else to_apply_yes(1, NULL);
}
static void to_fps_up(lj_widget *w, void *ud)   /* sbMIDI24 / 25 / 30: the frame rate the offset's frames count in */
{
    (void)ud;
    to_fps = w && w->name ? atoi(w->name + 6) : 25;
    lj_w_speedbutton_set_down(in(TO, "sbMIDI24"), to_fps == 24); lj_w_speedbutton_set_down(in(TO, "sbMIDI25"), to_fps == 25); lj_w_speedbutton_set_down(in(TO, "sbMIDI30"), to_fps == 30);
}
static void to_layout(void)
{
    static const char *const panels[] = { "pMidi", "pCD", "pMP", "pDA", "pPC" };
    lj_widget *p1 = in(TO, "Panel1"), *p2 = in(TO, "Panel2");
    int i;
    for (i = 0; i < 5; i++) { lj_widget *p = in(TO, panels[i]); if (p) p->visible = (i == 0 && to_family == LJ_CLF_MTC) || (i == 4 && to_family == LJ_CLF_PC); }
    if (p1) p1->h = 75;                                          /* one 73-px family panel inside Panel1's bevels (cuelistoffsettimecode.gif shows one family) */
    if (p2) p2->y = 75;
    lj_window_resize(sd_win[TO], lj_form_TTIMECODEOFFSETFORM.client_width, 75 + 32);
    lj_widget_layout(lj_window_root(sd_win[TO]));
    lj_menu_set_checked(lj_window_menus(sd_win[TO]), "M_MIDI", to_family == LJ_CLF_MTC);
    lj_menu_set_checked(lj_window_menus(sd_win[TO]), "M_PC", to_family == LJ_CLF_PC);
    lj_window_invalidate_all(sd_win[TO]);
}
static void to_type(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sd_win[TO]);
    (void)w; (void)ud;
    if (it && it->name && !strcmp(it->name, "M_MIDI")) to_family = LJ_CLF_MTC;
    else if (it && it->name && !strcmp(it->name, "M_PC")) to_family = LJ_CLF_PC;
    else return;                                                 /* M_DA / M_MP / M_CD: greyed (ME-11) */
    to_layout();
}
void lj_show_dlg_timecode_offset(int family)
{
    static const lj_binding b[] = { { "bOkClick", to_apply, NULL }, { "bCheckClick", to_check, NULL }, { "sbMIDIMouseUp", to_fps_up, NULL }, { "TimeCodeTypeClick", to_type, NULL },
        { "FormClose", close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    static const char *const greyed[] = { "M_DA", "M_MP", "M_CD" };
    lj_widget *root, *w;
    int i;
    to_family = family == LJ_CLF_PC ? LJ_CLF_PC : LJ_CLF_MTC;
    if (!sd_win[TO]) {
        if (!mk(TO, &lj_form_TTIMECODEOFFSETFORM, b)) return;
        root = lj_window_root(sd_win[TO]);
        for (i = 0; i < 3; i++) lj_menu_set_enabled(lj_window_menus(sd_win[TO]), greyed[i], 0);   /* menus show no hints (W3 LORD_FIDELITY-6) */
        for (w = root; w; ) {                                    /* every TUpDown drives its Associate edit (the DFM's Associate) */
            if (w->kind == LJ_K_UPDOWN && w->desc && w->desc->associate) lj_w_updown_bind(w, lj_window_find(sd_win[TO], w->desc->associate), lj_w_edit_set_text);
            if (w->first) w = w->first; else { while (w && !w->next) w = w->parent; if (w) w = w->next; }
        }
        to_fps_up(in(TO, "sbMIDI25"), NULL);
    }
    to_layout();
    shown(TO);
}

int lj_show_dlg_sort_waits(lj_cuelist *cl, int cmd)   /* exe 0x180E34 "Not enough lines to sort", 0x180E58 "Fragmented Selection, cannot sort" */
{
    uint32_t i, first = 0, n = 0, j;
    lj_widget *pg;
    if (!cl || cmd != LJ_CL_WAIT_MTC) return -1;
    for (i = 0; i < cl->n; i++) if (cl->lines[i].cmd == cmd) { if (!n) first = i; n++; }
    if (n < 2) { lj_win_message("Sort Wait Times", "Not enough lines to sort"); return -1; }
    for (i = first; i < first + n; i++) if (cl->lines[i].cmd != cmd) { lj_win_message("Sort Wait Times", "Fragmented Selection, cannot sort"); return -1; }
    lj_show_dlg_open(LJ_WIN_SORT_TIMECODE);                  /* "Sorting Timecodes" while it runs */
    if ((pg = in(LJ_WIN_SORT_TIMECODE, "pg1")) != NULL) ((lj_w_progressbar *)pg)->max = (int)n;
    for (i = first + 1; i < first + n; i++) {                /* insertion sort, ascending, stable [CHM Sorting wait times] */
        lj_cl_line t = cl->lines[i];
        for (j = i; j > first && cl->lines[j - 1].param_ms > t.param_ms; j--) cl->lines[j] = cl->lines[j - 1];
        cl->lines[j] = t;
        if (pg) lj_w_progressbar_set(pg, (int)(i - first + 1));
    }
    for (i = 0; i < cl->n; i++) cl->lines[i].line = i + 1;
    lj_show_dlg_close(LJ_WIN_SORT_TIMECODE);
    return 0;
}

/* ==================================================================================== Position Options (TMovementOptionsForm) [04 s4, 17 s4.1] */
#define MO LJ_WIN_MOVEMENT_OPTIONS
static int mo_mv_seen;
static void mo_click(lj_widget *w, void *ud) { (void)ud; lj_position_options_apply(!strcmp(w->name, "bMirrorPan") ? 0 : !strcmp(w->name, "bMirrorTilt") ? 1 : 2); }
static void mo_open(void)
{
    static const lj_binding b[] = { { "bMirrorPanClick", mo_click, NULL }, { "bMirrorTiltClick", mo_click, NULL }, { "bSwapClick", mo_click, NULL },
        { "b360Click", lj_handler_noop, NULL } /* greyed below */, { "FormActivate", lj_handler_noop, NULL }, { "FormClose", close_ev, NULL },
        { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    lj_widget *w;
    if (!mk(MO, &lj_form_TMOVEMENTOPTIONSFORM, b)) return;
    if ((w = in(MO, "b360")) != NULL) { w->enabled = 0; w->hint = LJ_CL_NOT_AVAILABLE; }   /* "only enabled if supported by the fixture profile" (H26 p137): no profile carries its pan range in degrees */
    mo_mv_seen = lj_ctl_window_get(LJ_CTL_MOVEMENT) != NULL;
}

/* ==================================================================================== Static Slot Name (TStaticSlotNameForm) [14 sB.3, sB.5] */
#define SN LJ_WIN_STATIC_SLOT_NAME
static int sn_page, sn_slot;
static lj_static_slot *sn_ptr(void)
{
    lj_show *sh = C()->show;
    return sh && sh->pages && sn_page >= 1 && sn_page <= sh->npages && sn_slot >= 1 && sn_slot <= LJ_STATICS2_SLOTS_MAX ? &sh->pages[sn_page - 1].slot[sn_slot - 1] : NULL;
}
static void sn_ok(lj_widget *w, int ev, void *ud)
{
    lj_static_slot *st = sn_ptr();
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (st) { lj_lock(); lj_str_copy(st->name, sizeof st->name, lj_w_edit_text(in(SN, "eStaticName"))); lj_unlock(); lj_statics2_save(); lj_statics2_set_page(lj_statics2_page()); }
    lj_show_dlg_close(SN);
}
void lj_show_dlg_static_slot_name(int page, int slot)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    lj_static_slot *st;
    if (!mk(SN, &lj_form_TSTATICSLOTNAMEFORM, b)) return;
    sn_page = page; sn_slot = slot;
    st = sn_ptr();
    lj_w_edit_set_text(in(SN, "eStaticName"), st ? st->name : "");
    lj_w_edit_select(in(SN, "eStaticName"), 0, -1);
    lj_widget_set_focus(in(SN, "eStaticName"));
    hook(SN, "BitBtn1", sn_ok, NULL);
    hook(SN, "BitBtn2", closer, (void *)(intptr_t)SN);
    shown(SN);
}

/* ==================================================================================== Cue Builder (TCueStackForm) [03 s5.8, CHM The cue builder]:
 * a 12-slot cue preset (on/off, sequence, trig) GO-latched into the current cue, and a cue stack (one non-transparent cue kept on top, distinct
 * transparent cues) GO-loaded top to bottom through the cue form's loader (design 2.3 rule 2: the app loads and pins). Capture redirects the
 * load-sequence (lj_seq_form_load) and load-cue (List of Cues) commands into it while the window is open. */
#define CB LJ_WIN_CUE_STACK
static struct {
    struct { uint32_t seq; uint8_t on, trig; } pre[LJ_CUE_SLOTS];
    struct { uint32_t cue; uint8_t transparent; char name[31]; } st[LJ_SD_STACK_MAX];
    int nst, cap_seq, cap_cue;
    char cells[LJ_CUE_SLOTS][3][40], rowtxt[LJ_SD_STACK_MAX][48]; const char *rows[LJ_SD_STACK_MAX];
    lj_widget_vt sgvt; void (*sg_paint0)(lj_widget *, lj_surface *);
} cb;
static const char *const cb_trig_word[] = { "Auto", "Manual", "Audio" };   /* the cue grid's Trig words (cuebuilder.gif: "Auto") */
static int cb_cue_info(uint32_t number, uint8_t *transparent, char *name, size_t cap)
{
    static lj_cue q;
    char path[LJ_PATH_CAP], err[96];
    lj_ctx *c = C();
    if (!c->lib || lj_library_record_path(c->lib, LJ_REC_CUE, number, path, sizeof path) != 0 || lj_cue_load(path, &q, err, sizeof err) != 0) return -1;
    *transparent = q.transparent; lj_str_copy(name, cap, q.name);
    return 0;
}
static const char *cb_seq_name(uint32_t seq)
{
    static lj_sequence tmp;
    lj_ctx *c = C();
    const lj_sequence *s = c->show ? (const lj_sequence *)lj_table_get(&c->show->sequences, seq) : NULL;
    char path[LJ_PATH_CAP], err[96];
    if (s) return s->name;
    if (c->lib && lj_library_record_path(c->lib, LJ_REC_SEQ, seq, path, sizeof path) == 0) {
        free(tmp.scenes); memset(&tmp, 0, sizeof tmp);
        if (lj_sequence_load(path, &tmp, err, sizeof err) == 0) return tmp.name;
    }
    return "load error";
}
static void cb_fill(void)
{
    lj_widget *g = in(CB, "SG1"), *lv = in(CB, "lvCues");
    int i;
    if (!sd_win[CB]) return;
    for (i = 0; g && i < LJ_CUE_SLOTS; i++) {
        snprintf(cb.cells[i][0], sizeof cb.cells[i][0], "%d", i + 1);
        if (cb.pre[i].seq) snprintf(cb.cells[i][1], sizeof cb.cells[i][1], "%04u", (unsigned)cb.pre[i].seq); else cb.cells[i][1][0] = 0;
        lj_str_copy(cb.cells[i][2], sizeof cb.cells[i][2], cb.pre[i].seq ? cb_seq_name(cb.pre[i].seq) : "");
        lj_w_grid_set_cell(g, 0, i + 1, cb.cells[i][0]); lj_w_grid_set_cell(g, 1, i + 1, cb.cells[i][1]); lj_w_grid_set_cell(g, 2, i + 1, cb.cells[i][2]);
        lj_w_grid_set_cell(g, 3, i + 1, cb.pre[i].seq ? cb_trig_word[cb.pre[i].trig < 3 ? cb.pre[i].trig : 0] : "");
    }
    for (i = 0; i < cb.nst; i++) { snprintf(cb.rowtxt[i], sizeof cb.rowtxt[i], "%s%04u %s", cb.st[i].transparent ? "(T)" : "", (unsigned)cb.st[i].cue, cb.st[i].name); cb.rows[i] = cb.rowtxt[i]; }
    if (lv) { lj_w_listview_set_items(lv, cb.rows, cb.nst); lj_widget_invalidate(lv); }
    enable(in(CB, "bGo"), cb.nst > 0);
    lj_window_set_text(sd_win[CB], "pCaptureSeq", cb.cap_seq ? "Capture Activated (Load Seq Commands)" : "Capture Deactivated");   /* cuebuilder.gif; "Capture Deactivated" = the DFM */
    lj_window_set_text(sd_win[CB], "pCaptureCue", cb.cap_cue ? "Capture Activated (Load Cue Commands)" : "Capture Deactivated");
    if ((g = in(CB, "pCaptureSeq")) != NULL) g->color = cb.cap_seq ? LJ_SD_CAPTURE_RGB : LJ_CLR_BTNSHADOW;
    if ((g = in(CB, "pCaptureCue")) != NULL) g->color = cb.cap_cue ? LJ_SD_CAPTURE_RGB : LJ_CLR_BTNSHADOW;
    lj_window_invalidate_all(sd_win[CB]);
}
static void cb_sg_paint(lj_widget *w, lj_surface *s)   /* the fixed column is the slot's on/off button: green on, red off (cuebuilder.gif, as the cue grid) */
{
    lj_rect r;
    int ax, ay, i;
    cb.sg_paint0(w, s);
    lj_widget_abs(w, &ax, &ay);
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (lj_w_grid_cell_rect(w, 0, i + 1, &r) == 0) {
        lj_fill(s, ax + r.x, ay + r.y, r.w, r.h, cb.pre[i].on ? 0x00FF00u : 0xFF0000u);
        lj_w_text(s, w, ax + r.x + 2, ay + r.y + 1, cb.cells[i][0], 0x000000u);
    }
}
int lj_show_dlg_capture_seq(uint32_t seq)   /* "in as high a priority position as possible ... no free slots: slot 12" [CHM] */
{
    int i;
    if (!sd_win[CB] || !cb.cap_seq || !seq) return 0;
    for (i = LJ_CUE_SLOTS - 1; i >= 0 && cb.pre[i].seq; i--) { }
    if (i < 0) i = LJ_CUE_SLOTS - 1;
    cb.pre[i].seq = seq; cb.pre[i].on = 1; cb.pre[i].trig = LJ_TRIG_AUTO;
    cb_fill();
    return 1;
}
int lj_show_dlg_cue_stack_add(uint32_t cue)   /* one non-transparent cue, always on top; a transparent cue once (a second add removes it) */
{
    uint8_t tr = 0;
    char name[31] = "";
    int i;
    if (!cue) return -1;
    if (cb_cue_info(cue, &tr, name, sizeof name) != 0) lj_str_copy(name, sizeof name, "load error");
    for (i = 0; i < cb.nst; i++) if (cb.st[i].cue == cue) { memmove(&cb.st[i], &cb.st[i + 1], (size_t)(cb.nst - i - 1) * sizeof cb.st[0]); cb.nst--; cb_fill(); return 0; }
    if (!tr && cb.nst && !cb.st[0].transparent) i = 0;          /* replaces the non-transparent one */
    else {
        if (cb.nst >= LJ_SD_STACK_MAX) return -1;
        i = tr ? cb.nst : 0;
        memmove(&cb.st[i + 1], &cb.st[i], (size_t)(cb.nst - i) * sizeof cb.st[0]);
        cb.nst++;
    }
    cb.st[i].cue = cue; cb.st[i].transparent = tr; lj_str_copy(cb.st[i].name, sizeof cb.st[i].name, name);
    cb_fill();
    return 0;
}
int lj_show_dlg_capture_cue(uint32_t cue) { if (!sd_win[CB] || !cb.cap_cue || !cue) return 0; lj_show_dlg_cue_stack_add(cue); return 1; }
static int cb_pref(const char *key) { return lj_ini_geti(C()->forms_ini, "CUESTACK", key, 0); }   /* forms.ini [CUESTACK] (ours, PROPOSAL keys) */
int lj_show_dlg_cue_builder_go(int stack)
{
    lj_ctx *c = C();
    const lj_cue_form_host *h = lj_cue_form_get_host();
    lj_cue *cue = c->show ? c->show->current_cue : NULL;
    int i;
    if (!cue) return -1;
    if (lj_w_speedbutton_down(in(CB, stack ? "sbClearCue" : "sbClearCueSeq"))) lj_cue_form_new_cue();
    cue = c->show->current_cue;
    if (stack) {
        for (i = 0; i < cb.nst; i++) if (!h || !h->load_cue || h->load_cue(h->ud, cb.st[i].cue, 0) != 0) lj_log("cue builder: cue %04u not loaded", (unsigned)cb.st[i].cue);
        if (cb_pref("AutoClearStack")) cb.nst = 0;
    } else {
        lj_lock();
        for (i = 0; i < LJ_CUE_SLOTS; i++) {
            if (!cb.pre[i].seq) continue;
            if (cue->slot[i].seq == cb.pre[i].seq && !cb_pref("ForceReload")) { cue->slot[i].enabled = cb.pre[i].on; continue; }   /* Force Sequence Reload off: an occupied same-sequence slot is kept */
            lj_cue_add_sequence(cue, cb.pre[i].seq, i + 1);
            cue->slot[i].enabled = cb.pre[i].on; cue->slot[i].trig = cb.pre[i].trig;
        }
        lj_unlock();
        lj_ctx_set_modified(LJ_MOD_CUE, 1);
        lj_cue_form_repin();
        if (cb_pref("AutoClearSeq")) memset(cb.pre, 0, sizeof cb.pre);
    }
    lj_cue_form_refresh();
    cb_fill();
    return 0;
}
static int cb_row(void) { lj_widget *g = in(CB, "SG1"); int r = g ? ((lj_w_grid *)g)->row - 1 : -1; return r >= 0 && r < LJ_CUE_SLOTS ? r : -1; }
static void cb_get_cue(lj_widget *w, void *ud)   /* Get Cue: the current cue's sequences into the preset */
{
    const lj_cue *cue = C()->show ? C()->show->current_cue : NULL;
    int i;
    (void)w; (void)ud;
    for (i = 0; cue && i < LJ_CUE_SLOTS; i++) { cb.pre[i].seq = cue->slot[i].seq; cb.pre[i].on = cue->slot[i].enabled; cb.pre[i].trig = cue->slot[i].trig; }
    cb_fill();
}
static void cb_trig(lj_widget *w, void *ud)   /* sbAuto / sbManual / sbMusic: the highlighted slot; right-click = every slot [CHM] */
{
    const lj_mouse_ev *m = lj_window_last_event(sd_win[CB]);
    int t = !strcmp(w->name, "sbManual") ? LJ_TRIG_MANUAL : !strcmp(w->name, "sbMusic") ? LJ_TRIG_AUDIO : LJ_TRIG_AUTO, i, r = cb_row();
    (void)ud;
    for (i = 0; i < LJ_CUE_SLOTS; i++) if ((m && m->button == 2) || i == r) cb.pre[i].trig = (uint8_t)t;
    cb_fill();
}
static void cb_clear_seq(lj_widget *w, void *ud)   /* Clear Seq: the highlighted slot; right-click = every slot [CHM] */
{
    const lj_mouse_ev *m = lj_window_last_event(sd_win[CB]);
    int r = cb_row();
    (void)w; (void)ud;
    if (m && m->button == 2) memset(cb.pre, 0, sizeof cb.pre); else if (r >= 0) memset(&cb.pre[r], 0, sizeof cb.pre[r]);
    cb_fill();
}
static void cb_clear_stack(lj_widget *w, void *ud) { (void)w; (void)ud; cb.nst = 0; cb_fill(); }
static void cb_go(lj_widget *w, void *ud) { (void)w; lj_show_dlg_cue_builder_go(ud != NULL); }
static void cb_capture(lj_widget *w, void *ud) { (void)ud; if (!strcmp(w->name, "pCaptureSeq")) cb.cap_seq = !cb.cap_seq; else cb.cap_cue = !cb.cap_cue; cb_fill(); }
static void cb_sg_down(lj_widget *w, void *ud)   /* the on/off column toggles the slot (CHM "Sequence On/Off") */
{
    const lj_mouse_ev *m = lj_window_last_event(sd_win[CB]);
    int ax, ay, col, row;
    (void)ud;
    if (!m) return;
    lj_widget_abs(w, &ax, &ay);
    if (lj_w_grid_cell_at(w, m->x - ax - lj_window_root(sd_win[CB])->x, m->y - ay - lj_window_root(sd_win[CB])->y, &col, &row) == 0 && col == 0 && row >= 1 && cb.pre[row - 1].seq) { cb.pre[row - 1].on = !cb.pre[row - 1].on; cb_fill(); }
}
static void cb_sg_drop(lj_widget *w, void *ud)   /* a sequence dragged from the List of Sequences onto a slot row */
{
    int kind, number, x, y, col, row;
    (void)ud;
    if (lj_dnd_info(sd_win[CB], &kind, &number, &x, &y) != 0 || kind != LJ_REC_SEQ || number <= 0) return;
    if (lj_w_grid_cell_at(w, x, y, &col, &row) != 0 || row < 1) return;
    cb.pre[row - 1].seq = (uint32_t)number; cb.pre[row - 1].on = 1; cb.pre[row - 1].trig = LJ_TRIG_AUTO;
    cb_fill();
}
static void cb_lv_drop(lj_widget *w, void *ud) { int kind, number, x, y; (void)w; (void)ud; if (lj_dnd_info(sd_win[CB], &kind, &number, &x, &y) == 0 && kind == LJ_REC_CUE && number > 0) lj_show_dlg_cue_stack_add((uint32_t)number); }
static void cb_view(lj_widget *w, void *ud)   /* Preferences > View: Default / Cue Preset / Cue Stack */
{
    const lj_menu_item_desc *it = lj_window_menu_item(sd_win[CB]);
    lj_widget *pre = in(CB, "pCuePreset"), *stk = in(CB, "pCueStack");
    (void)w; (void)ud;
    if (!it || !it->name || !pre || !stk) return;
    if (!strcmp(it->name, "M_ViewCuePreset")) pre->visible = !pre->visible;
    else if (!strcmp(it->name, "m_ViewCueStack")) stk->visible = !stk->visible;
    else { pre->visible = stk->visible = 1; lj_window_resize(sd_win[CB], lj_form_TCUESTACKFORM.client_width, lj_form_TCUESTACKFORM.client_height); }
    lj_menu_set_checked(lj_window_menus(sd_win[CB]), "M_ViewCuePreset", pre->visible);
    lj_menu_set_checked(lj_window_menus(sd_win[CB]), "m_ViewCueStack", stk->visible);
    lj_widget_layout(lj_window_root(sd_win[CB]));
    lj_window_invalidate_all(sd_win[CB]);
}
static const struct { const char *item, *key; } cb_prefs[] = {
    { "M_EnableSeqCapture", "SeqCapture" }, { "M_ReloadSequence", "ForceReload" }, { "M_AutoClearSequencesonGo", "AutoClearSeq" },
    { "M_EnableCueCapture", "CueCapture" }, { "M_AutoClearStackonGo", "AutoClearStack" }, { "M_AddCueStacktoCuePreset", "StackInPreset" }, { "M_ColorTransparent", "ColorTransparent" } };
static void cb_checks(void) { int i; for (i = 0; i < (int)LJ_COUNT_OF(cb_prefs); i++) lj_menu_set_checked(lj_window_menus(sd_win[CB]), cb_prefs[i].item, cb_pref(cb_prefs[i].key)); }
static void cb_pref_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sd_win[CB]);
    int i;
    (void)w; (void)ud;
    for (i = 0; it && it->name && C()->forms_ini && i < (int)LJ_COUNT_OF(cb_prefs); i++)
        if (!strcmp(it->name, cb_prefs[i].item)) lj_ini_seti(C()->forms_ini, "CUESTACK", cb_prefs[i].key, !cb_pref(cb_prefs[i].key));
    cb_checks();
}
static const char *const cb_pop_trig[3] = { "Pop_Auto", "Pop_Manual", "Pop_Audio" };   /* enum lj_trig order */
static void cb_pop(lj_widget *w, void *ud)   /* PresetPop: Clear Sequence / Clear all Sequences / Auto / Manual / Audio Trig on the highlighted slot */
{
    const lj_menu_item_desc *it = lj_window_menu_item(sd_win[CB]);
    int r = cb_row(), i;
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "Pop_ClearAllSeq")) memset(cb.pre, 0, sizeof cb.pre);
    else if (r < 0) return;
    else if (!strcmp(it->name, "Pop_ClearSequence")) memset(&cb.pre[r], 0, sizeof cb.pre[r]);
    else for (i = 0; i < 3; i++) if (!strcmp(it->name, cb_pop_trig[i])) cb.pre[r].trig = (uint8_t)i;
    cb_fill();
}
static void cb_stack_pop(lj_widget *w, void *ud)   /* CueStackPop > Clear Cues: the highlighted cue */
{
    int i = lj_w_listview_index(in(CB, "lvCues"));
    (void)w; (void)ud;
    if (i >= 0 && i < cb.nst) { memmove(&cb.st[i], &cb.st[i + 1], (size_t)(cb.nst - i - 1) * sizeof cb.st[0]); cb.nst--; cb_fill(); }
}
static void cb_open(void)
{
    static const lj_binding b[] = {
        { "pCaptureClick", cb_capture, NULL }, { "lvCuesChange", lj_handler_noop, NULL }, { "lvCuesEndDrag", lj_handler_noop, NULL }, { "lvCuesDragDrop", cb_lv_drop, NULL },
        { "lvCuesDragOver", lj_handler_noop, NULL }, { "lvCuesStartDrag", lj_handler_noop, NULL } /* ponytail: the stack is reordered by its rule, not by drag */,
        { "sbClearStackClick", cb_clear_stack, NULL }, { "bGoClick", cb_go, (void *)1 }, { "sbSeqGoClick", cb_go, NULL },
        { "SG1DragDrop", cb_sg_drop, NULL }, { "SG1DragOver", lj_handler_noop, NULL }, { "SG1DrawCell", lj_handler_noop, NULL } /* cb_sg_paint */, { "SG1EndDrag", lj_handler_noop, NULL },
        { "SG1MouseDown", cb_sg_down, NULL }, { "sbClearSeqMouseDown", cb_clear_seq, NULL }, { "sbTrigMouseDown", cb_trig, NULL }, { "sbGetCueClick", cb_get_cue, NULL },
        { "default1Click", cb_view, NULL }, { "ViewClick", cb_view, NULL }, { "PrefClicked", cb_pref_click, NULL }, { "Help1Click", help, NULL },
        { "CueStackPopPopup", lj_handler_noop, NULL }, { "PopItemClicked", cb_stack_pop, NULL }, { "PresetPopClick", lj_handler_noop, NULL }, { "PresetPopItemClick", cb_pop, NULL },
        { "FormActivate", lj_handler_noop, NULL }, { "FormClose", close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
        { "FormResize", lj_handler_noop, NULL } /* aligned */, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    static const int colw[4] = { 18, 34, 136, 42 };              /* PROPOSAL rig 03 s5.8: cuebuilder.gif is reduced; No / Sequence / Trig as the cue grid */
    lj_widget *g;
    if (!mk(CB, &lj_form_TCUESTACKFORM, b)) return;
    if ((g = in(CB, "SG1")) != NULL) {
        static const char *const hdr[4] = { "", "No", "Sequence", "Trig" };
        int i;
        ((lj_w_grid *)g)->col_w = colw;
        for (i = 0; i < 4; i++) lj_w_grid_set_cell(g, i, 0, hdr[i]);
        cb.sgvt = *g->vt; cb.sg_paint0 = g->vt->paint; cb.sgvt.paint = cb_sg_paint; g->vt = &cb.sgvt;
    }
    if ((g = in(CB, "lvCues")) != NULL) ((lj_w_listview *)g)->col_w[0] = g->w - 4;   /* the one "Cue Stack" column spans the list (DFM Width 10 is resized at runtime: cuebuilder.gif) */
    cb.cap_seq = cb_pref("SeqCapture"); cb.cap_cue = cb_pref("CueCapture");
    lj_menu_set_checked(lj_window_menus(sd_win[CB]), "M_ViewCuePreset", 1);
    lj_menu_set_checked(lj_window_menus(sd_win[CB]), "m_ViewCueStack", 1);
    cb_checks();
    cb_fill();
}
static void fn_cb(enum lj_fn fn, int edge, uint32_t prog, void *ud)   /* Cue builder hotkeys [03 s5.8]; all but Toggle only while the window is open [CHM] */
{
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn == LJ_FN_TOGGLE_CUE_BUILDER) { lj_win_toggle(CB); return; }
    if (!sd_win[CB]) return;
    switch (fn) {
    case LJ_FN_TOGGLE_CUE_CAPTURE: cb.cap_cue = !cb.cap_cue; break;
    case LJ_FN_TOGGLE_SEQ_CAPTURE: cb.cap_seq = !cb.cap_seq; break;
    case LJ_FN_CUE_STACK_GO:       lj_show_dlg_cue_builder_go(1); return;
    case LJ_FN_CUE_PRESET_GO:      lj_show_dlg_cue_builder_go(0); return;
    case LJ_FN_GET_CUE:            cb_get_cue(NULL, NULL); return;
    case LJ_FN_CLEAR_CUE_STACK:    cb.nst = 0; break;
    case LJ_FN_CLEAR_ALL_SEQUENCES: memset(cb.pre, 0, sizeof cb.pre); break;
    default: return;
    }
    cb_fill();
}

/* ==================================================================================== the family entry points */
static void dropped(enum lj_win id, const char *why) { lj_log("lj_win_open: %s: %s (docs/fidelity.md)", lj_win_id_name(id), why); }
lj_window *lj_show_dlg_open(enum lj_win id)
{
    if ((int)id < 0 || id >= LJ_WIN_COUNT) return NULL;
    if (sd_win[id]) return shown(id);
    switch (id) {
    case LJ_WIN_CUE_STACK:            cb_open(); break;
    case LJ_WIN_MODAL_SELECT_CUE:     pk_open(&sd_pk[0], NULL, NULL); break;
    case LJ_WIN_DELETE_SCENE:         ds_open(); break;
    case LJ_WIN_INSERT_SCENES:        is_open(); break;
    case LJ_WIN_SET_SEQUENCE:         lj_show_dlg_set_sequence(1, NULL, NULL); break;
    case LJ_WIN_SEQ_USED:             lj_show_dlg_seq_used(0, 1, 0, 1); break;
    case LJ_WIN_SELECT_BGCUE_OLD:     pk_open(&sd_pk[1], NULL, NULL); break;
    case LJ_WIN_SELECT_CUELIST_MODAL: pk_open(&sd_pk[2], NULL, NULL); break;
    case LJ_WIN_NAME_SLOTS:           ns_open(); break;
    case LJ_WIN_TIMECODE_OFFSET:      lj_show_dlg_timecode_offset(LJ_CLF_MTC); break;
    case LJ_WIN_MOVEMENT_OPTIONS:     mo_open(); break;
    case LJ_WIN_STATIC_SLOT_NAME:     lj_show_dlg_static_slot_name(lj_statics2_page(), lj_statics2_popup_slot() ? lj_statics2_popup_slot() : 1); break;
    case LJ_WIN_SORT_TIMECODE: {
        static const lj_binding b[] = { { NULL, NULL, NULL } };
        mk(id, &lj_form_TSORTTIMECODEFORM, b);
        break; }
    case LJ_WIN_STATICS_CONTROL:      return lj_win_open(LJ_WIN_STATICS2_FORM);   /* D11: View > Statics Control opens Statics 2; the 20-entry form is not built */
    case LJ_WIN_SAVE_STATIC:          dropped(id, "the 20-entry Statics Control's Save is not built (D11); Statics 2 names a slot with Static Slot Name"); return NULL;
    case LJ_WIN_STATICS_REPLACE:      dropped(id, "the 20-entry Statics Control's replace matrix is not built (D11); Statics 2 replaces by scope"); return NULL;
    default: lj_log("lj_win_open: %s is not a SHOW id", lj_win_id_name(id)); return NULL;
    }
    return shown(id);
}

void lj_show_dlg_close(enum lj_win id)
{
    lj_window *w;
    lj_sd_picker *p;
    if ((int)id < 0 || id >= LJ_WIN_COUNT || !(w = sd_win[id])) return;
    sd_win[id] = NULL; sd_shown[id] = 0;                         /* FormClose re-entry guard */
    if ((p = pk_of(id)) != NULL) pk_free(p);
    if (id == IS) { lj_sequence_free(is_src); is_src = NULL; }
    if (id == NS) ns.ed = NULL;                                  /* a child of sg1: freed with the tree */
    if (id == CB) cb.cap_seq = cb.cap_cue = 0;                   /* "When the control is closed, capturing is disabled" [CHM] */
    lj_win_hide(w);
    lj_window_destroy(w);
}

lj_window *lj_show_dlg_get(enum lj_win id) { return (int)id >= 0 && id < LJ_WIN_COUNT ? sd_win[id] : NULL; }

void lj_show_dlg_refresh(enum lj_win id)
{
    lj_sd_picker *p = pk_of(id);
    if ((int)id < 0 || id >= LJ_WIN_COUNT || !sd_win[id]) return;
    if (p) enable(in(id, "bOk"), lj_w_listview_index(in(id, p->lv)) >= 0);   /* OK greyed until a row is highlighted (the DFMs' Enabled = False) */
    if (id == MO) {                                              /* closes with the position control, as Fan Out [04 s4, RN 2.5] */
        lj_window *mv = lj_ctl_window_get(LJ_CTL_MOVEMENT);
        if (mo_mv_seen && !mv) lj_show_dlg_close(MO); else if (mv) mo_mv_seen = 1;
    }
}

void lj_show_dlg_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_TOGGLE_CUE_BUILDER, LJ_FN_TOGGLE_CUE_CAPTURE, LJ_FN_TOGGLE_SEQ_CAPTURE, LJ_FN_CUE_STACK_GO, LJ_FN_CUE_PRESET_GO,
                                       LJ_FN_GET_CUE, LJ_FN_CLEAR_CUE_STACK, LJ_FN_CLEAR_ALL_SEQUENCES };
    int i;
    for (i = 0; i < (int)LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_cb, NULL);
}
