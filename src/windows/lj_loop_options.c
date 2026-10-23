/* lj_loop_options.c - TLoopToolsForm behaviour (lj_loop_options.h). */
#include "windows/lj_loop_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_windows.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */

#define F(m) offsetof(lj_cue_slot, m), sizeof(((lj_cue_slot *)0)->m)
/* ranges: the edit MaxLength (3 digits, Loops 2) inside the member's type; Scenes/Trig and Audio-Trigs/Trig are at
 * least 1 (lj_cue_validate); 0 in First/Last/Goto/Link = none [03 s5.5] */
static const lj_loop_field_desc lj_lf[LJ_LF_COUNT] = {
    { "eTrigScenes", LJ_LF_SPT,          F(scenes_per_trig),      1, 255 },
    { "eAudioTrigs", LJ_LF_ATPT,         F(audio_trigs_per_trig), 1, 255 },
    { "cbTrigLoad",  LJ_LF_TRIG_ON_LOAD, F(trig_on_load),         0, 1 },
    { "RDirection",  LJ_LF_DIR,          F(dir),                  LJ_DIR_FWD, LJ_DIR_RANDOM },   /* Items: Forward Reverse Bounce Random = enum lj_dir */
    { "eFirst",      LJ_LF_FIRST,        F(first),                0, 999 },
    { "eLast",       LJ_LF_LAST,         F(last),                 0, 999 },
    { "eLoops",      LJ_LF_LOOPS,        F(loops),                0, 99 },
    { "REndLoop",    LJ_LF_END,          F(end_action),           LJ_END_STOP, LJ_END_LINK },   /* Items: Stop on scene, Sequence out, Goto Scene, Link to Cue = enum lj_end_action */
    { "eGoto",       LJ_LF_GOTO,         F(goto_scene),           0, 999 },
    { "eLink",       LJ_LF_LINK,         F(link_cue),             0, 999 } };
#undef F
/* the TUdButton beside each edit; the DFM Tag pairs them (1 First, 2 Last, 3 Loops, 4 Goto, 5 Scenes/Trig, 6 Audio-Trigs) */
static const struct { const char *ud; enum lj_loop_field f; } lj_lf_ud[] = {
    { "UdFirst", LJ_LF_FIRST }, { "UdLast", LJ_LF_LAST }, { "UdButtons", LJ_LF_LOOPS }, { "UdButton4", LJ_LF_GOTO },
    { "udTrigScenes", LJ_LF_SPT }, { "udAudioTrigs", LJ_LF_ATPT } };

const lj_loop_field_desc *lj_loop_options_fields(int *n) { if (n) *n = LJ_LF_COUNT; return lj_lf; }

static struct { lj_window *win; int filling; uint16_t shown_mask; lj_cmd last; int posts; } lo;

static long get_member(const lj_cue_slot *s, const lj_loop_field_desc *d)
{
    const unsigned char *p = (const unsigned char *)s + d->offset;
    if (d->size == 1) return *p;
    if (d->size == 2) { uint16_t v; memcpy(&v, p, 2); return v; }
    { uint32_t v; memcpy(&v, p, 4); return (long)v; }
}
static void set_member(lj_cue_slot *s, const lj_loop_field_desc *d, long v)
{
    unsigned char *p = (unsigned char *)s + d->offset;
    if (v < d->lo) v = d->lo;
    if (v > d->hi) v = d->hi;
    if (d->size == 1) *p = (unsigned char)v;
    else if (d->size == 2) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
    else { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); }
}

static lj_cue *cue_rec(void) { lj_ctx *c = lj_ctx_get(); return c->show ? c->show->current_cue : NULL; }
static uint16_t target_mask(void)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (lj_cue_form_selected(i)) m = (uint16_t)(m | 1u << i);
    return m ? m : 1u;                                    /* nothing highlighted: slot 1 */
}
int lj_loop_options_target(void) { uint16_t m = target_mask(); int i = 0; while (!(m >> i & 1)) i++; return i; }
int lj_loop_options_last_cmd(lj_cmd *out) { if (out && lo.posts) *out = lo.last; return lo.posts; }
lj_window *lj_loop_options_get(void) { return lo.win; }

static lj_widget *ctl(const char *name) { return lo.win ? lj_window_find(lo.win, name) : NULL; }

/* ---- slot -> controls ---- */
static void fill(void)
{
    const lj_cue *cue = cue_rec();
    lj_cue_slot sl;
    char buf[16];
    int i;
    lj_widget *w;
    if (!lo.win) return;
    memset(&sl, 0, sizeof sl);
    sl.scenes_per_trig = sl.audio_trigs_per_trig = 1;
    if (cue) sl = cue->slot[lj_loop_options_target()];
    lo.filling = 1;
    for (i = 0; i < LJ_LF_COUNT; i++) {
        long v = get_member(&sl, &lj_lf[i]);
        if ((w = ctl(lj_lf[i].ctl)) == NULL) continue;
        if (w->kind == LJ_K_RADIOGROUP) lj_w_radiogroup_set_index(w, (int)v);
        else if (w->kind == LJ_K_CHECKBOX) lj_w_checkbox_set_checked(w, (int)v);
        else { if (v || i == LJ_LF_GOTO || i == LJ_LF_LINK) snprintf(buf, sizeof buf, "%ld", v); else buf[0] = 0; lj_w_edit_set_text(w, buf); }   /* loopcontrol.gif: limits / loops empty at 0, Goto and Link show "0" */
    }
    if ((w = ctl("eLoops")) != NULL) { w->enabled = sl.dir != LJ_DIR_RANDOM; lj_widget_invalidate(w); }   /* no loop count in random [03 s5.5] */
    if ((w = ctl("UdButtons")) != NULL) { w->enabled = sl.dir != LJ_DIR_RANDOM; lj_widget_invalidate(w); }
    lo.filling = 0;
    lo.shown_mask = target_mask();
}

/* ---- controls -> slot: one LJ_CMD_LOOP_OPTS per target slot when anything changed ---- */
static void post_loop(int i)
{
    lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_LOOP_OPTS; c.a = (uint32_t)(LJ_RUN_SLOT0 + i); c.slot = &cue_rec()->slot[i];
    lo.last = c; lo.posts++;
    lj_ctx_post(&c);
}
static void commit_value(enum lj_loop_field f, long v)
{
    lj_cue *cue = cue_rec();
    uint16_t m = target_mask(), changed = 0;
    int i, pushed = 0;
    if (!cue || lo.filling) return;
    lj_lock();
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        lj_cue_slot old, *s = &cue->slot[i];
        if (!(m >> i & 1)) continue;
        old = *s;
        set_member(s, &lj_lf[f], v);
        if (s->dir == LJ_DIR_RANDOM) s->loops = 0;                     /* LJ_LOOP_RANDOM_NO_COUNT */
        if (s->first && s->last && s->first > s->last) { if (f == LJ_LF_LAST) s->first = s->last; else s->last = s->first; pushed = 1; }   /* first <= last (lj_cue_validate): the other limit follows */
        if (memcmp(&old, s, sizeof old) != 0) changed = (uint16_t)(changed | 1u << i);
    }
    lj_unlock();
    if (!changed) return;
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (changed >> i & 1) post_loop(i);
    lj_ctx_set_modified(LJ_MOD_CUE, 1);
    lj_cue_form_refresh();
    if (f == LJ_LF_DIR || pushed) fill();                              /* eLoops greys with Random; a limit pushed the other one */
}

static long edit_value(lj_widget *w) { const char *t = lj_w_edit_text(w); return t && t[0] ? strtol(t, NULL, 10) : 0; }
static int field_of(const lj_widget *w)
{
    int i;
    for (i = 0; i < LJ_LF_COUNT; i++) if (w->name && !strcmp(w->name, lj_lf[i].ctl)) return i;
    return -1;
}

static void on_edit_keyup(lj_widget *w, void *ud) { int f = field_of(w); (void)ud; if (f >= 0) commit_value((enum lj_loop_field)f, edit_value(w)); }
static void on_ud(lj_widget *w, void *ud)
{
    int i;
    (void)ud;
    for (i = 0; i < LJ_COUNT_OF(lj_lf_ud); i++) if (w->name && !strcmp(w->name, lj_lf_ud[i].ud)) {
        const lj_loop_field_desc *d = &lj_lf[lj_lf_ud[i].f];
        lj_widget *e = ctl(d->ctl);
        long v = (e ? edit_value(e) : 0) + lj_w_udbutton_delta(w);
        char buf[16];
        if (v < d->lo) v = d->lo;
        if (v > d->hi) v = d->hi;
        if (e) { lo.filling = 1; snprintf(buf, sizeof buf, "%ld", v); lj_w_edit_set_text(e, v || d->field == LJ_LF_GOTO ? buf : ""); lo.filling = 0; }
        commit_value(d->field, v);
    }
}
static void on_dir(lj_widget *w, void *ud) { (void)ud; commit_value(LJ_LF_DIR, lj_w_radiogroup_index(w)); }
static void on_end(lj_widget *w, void *ud) { (void)ud; commit_value(LJ_LF_END, lj_w_radiogroup_index(w)); }
static void on_trig_load(lj_widget *w, void *ud) { (void)ud; commit_value(LJ_LF_TRIG_ON_LOAD, lj_w_checkbox_checked(w)); }
static void on_default(lj_widget *w, void *ud)
{
    lj_cue *cue = cue_rec();
    uint16_t m = target_mask(), changed = 0;
    int i;
    (void)w; (void)ud;
    if (!cue) return;
    lj_lock();
    for (i = 0; i < LJ_CUE_SLOTS; i++) {                              /* fwd, no limits, unlimited, stop on scene, 1 / 1, no trig on load [03 s5.5 defaults] */
        lj_cue_slot *s = &cue->slot[i], old;
        if (!(m >> i & 1)) continue;
        old = *s;
        s->dir = LJ_DIR_FWD; s->first = s->last = s->loops = 0; s->end_action = LJ_END_STOP; s->goto_scene = 0; s->link_cue = 0;
        s->scenes_per_trig = s->audio_trigs_per_trig = 1; s->trig_on_load = 0;
        if (memcmp(&old, s, sizeof old) != 0) changed = (uint16_t)(changed | 1u << i);
    }
    lj_unlock();
    for (i = 0; i < LJ_CUE_SLOTS; i++) if (changed >> i & 1) post_loop(i);
    if (changed) { lj_ctx_set_modified(LJ_MOD_CUE, 1); lj_cue_form_refresh(); }
    fill();
}
static void on_select_cue(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_LIST_CUE); }   /* ponytail: opens the List of Cues; the number is typed into eLink (no pick-back seam yet) */
static void on_show(lj_widget *w, void *ud) { (void)w; (void)ud; fill(); }

static const lj_binding lj_lo_bindings[] = {
    { "bDefaultLoopClick", on_default, NULL }, { "REndLoopClick", on_end, NULL }, { "RDirectionClick", on_dir, NULL },
    { "UdButtonsChange", on_ud, NULL }, { "EValidateKeyPress", lj_handler_noop, NULL } /* digits only: the edits are LJ_W_EDIT_NUM */, { "EKeyUp", on_edit_keyup, NULL },
    { "bSelectCueClick", on_select_cue, NULL }, { "cbTrigLoadClick", on_trig_load, NULL }, { "Help1Click", lj_help_viewer_click, "cues" },
    { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormHide", lj_handler_noop, NULL }, { "FormShow", on_show, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_loop_options_open(void)
{
    int i;
    if (lo.win) return lo.win;
    lo.win = lj_window_create(&lj_form_TLOOPTOOLSFORM, lj_lo_bindings);
    if (!lo.win) { lj_log("lj_loop_options: TLoopToolsForm failed to build"); return NULL; }
    lo.posts = 0;
    for (i = 0; i < LJ_LF_COUNT; i++) {                                /* EValidateKeyPress: digits only */
        lj_widget *w = ctl(lj_lf[i].ctl);
        if (w && w->kind == LJ_K_EDIT) { lj_w_edit *e = (lj_w_edit *)w; e->variant = LJ_W_EDIT_NUM; e->min = 0; e->max = (int)lj_lf[i].hi; }
    }
    fill();
    lj_win_show(lo.win, LJ_WIN_STAY_ON_TOP);                         /* FormStyle = fsStayOnTop */
    return lo.win;
}
void lj_loop_options_close(void)
{
    if (!lo.win) return;
    lj_win_hide(lo.win);
    lj_window_destroy(lo.win);
    lo.win = NULL;
}
void lj_loop_options_refresh(void) { if (lo.win && target_mask() != lo.shown_mask) fill(); }

static void fn_loop(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)fn; (void)prog; (void)ud; if (edge) lj_win_toggle(LJ_WIN_LOOP_OPTIONS); }
void lj_loop_options_bind_fns(void) { lj_fn_bind(LJ_FN_CUE_LOOP_CONTROL, fn_loop, NULL); }
