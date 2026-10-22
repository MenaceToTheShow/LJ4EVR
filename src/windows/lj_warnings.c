/* lj_warnings.c - see lj_warnings.h. One window per state, reused; every OnXxx the
 * DFMs declare is bound (ME-16). Headless (no host) the boxes are still built so the
 * tests click their buttons through lj_window_event. */
#include "windows/lj_warnings.h"

#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_formsini.h"
#include "ui/widgets/lj_widgets_basic.h"

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/); fsStayOnTop forms */

/* 13-exe-strings-catalogue.tsv 0x18DFE0 / 0x18DFF8 / 0x18E010 and the DFM captions, verbatim */
static const char *const lj_warn_text[LJ_WARN_COUNT] = { "BLACKOUT ACTIVE", "Frozen !", "FADE-OUT ACTIVE", "FADE-IN ACTIVE", "DMX Output is Offline" };
static const char *const lj_warn_ini_key[LJ_WARN_COUNT] = { "ShowBlackoutWarning", "ShowFreezeWarning", "ShowBlackoutWarning", "ShowBlackoutWarning", "ShowOfflineWarning" };   /* Main.Ini [PREFS], ours (PROPOSAL: the design names the gates "Show*Warning" without the keys) */

static lj_window *lj_warn_win[LJ_WARN_COUNT];      /* BLACKOUT/FADE_OUT/FADE_IN share lj_warn_win[LJ_WARN_BLACKOUT] (one TSTATUSWARNINGFORM) */
static lj_window *lj_msg_win, *lj_close_win;
static struct { void (*fn)(int, void *); void *ud; } lj_close_cb, lj_confirm_cb;

const char *lj_warnings_status_text(enum lj_warning w) { return ((int)w >= 0 && w < LJ_WARN_COUNT) ? lj_warn_text[w] : ""; }
static int slot_of(enum lj_warning w) { return (w == LJ_WARN_FADE_OUT || w == LJ_WARN_FADE_IN) ? LJ_WARN_BLACKOUT : (int)w; }
lj_window *lj_warnings_window(enum lj_warning w) { return ((int)w >= 0 && w < LJ_WARN_COUNT) ? lj_warn_win[slot_of(w)] : NULL; }
lj_window *lj_win_message_window(void) { return lj_msg_win; }
lj_window *lj_win_warnings_any(void) { int i; for (i = 0; i < LJ_WARN_COUNT; i++) if (lj_warn_win[i]) return lj_warn_win[i]; return NULL; }
lj_window *lj_win_close_prompt_window(void) { return lj_close_win; }

void lj_warnings_center(const lj_window *box, int *x, int *y)
{
    const lj_window *main = lj_desktop_window();
    const lj_win_host *h = lj_win_host_get();
    const lj_form_desc *d = lj_window_desc(box);
    int mx = 0, my = 0;
    *x = d ? d->left : 0; *y = d ? d->top : 0;
    if (!main) return;
    if (h && h->pos && h->pos(h->ud, main, &mx, &my) != 0) return;
    lj_formsini_center(mx, my, lj_window_surface((lj_window *)main)->w, lj_window_surface((lj_window *)main)->h,
                       lj_window_surface((lj_window *)box)->w, lj_window_surface((lj_window *)box)->h, x, y);
}

static void show_centered(lj_window *w, unsigned flags)
{
    const lj_win_host *h = lj_win_host_get();
    int x, y;
    lj_win_show(w, flags);
    lj_warnings_center(w, &x, &y);                                   /* LORD_FIDELITY-12: every box centred on the main window on show */
    if (h && h->move) h->move(h->ud, w, x, y);
}

static void destroy(lj_window **slot) { if (*slot) { lj_win_hide(*slot); lj_window_destroy(*slot); *slot = NULL; } }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

/* ---- the three red boxes ---- */
static void post_b(enum lj_cmd_kind kind, uint32_t b) { lj_cmd c; memset(&c, 0, sizeof c); c.kind = (uint8_t)kind; c.b = b; lj_ctx_post(&c); }
static void on_cancel(lj_widget *w, void *ud)
{
    enum lj_warning which = (enum lj_warning)(intptr_t)ud;
    (void)w;
    switch (which) {
    case LJ_WARN_BLACKOUT: post_b(LJ_CMD_BLACKOUT, 0); break;   /* the GM fade boxes share this form: Cancel ends the blackout state only (fade cancel has no command, 11 G11) */
    case LJ_WARN_FROZEN:   post_b(LJ_CMD_FREEZE, 0); break;
    case LJ_WARN_OFFLINE:  post_b(LJ_CMD_OFFLINE, 0); break;
    default: break;
    }
    lj_win_warning(which, 0);
}

void lj_win_warning(enum lj_warning w, int show)
{
    static const lj_binding status_b[] = { { "FormActivate", noop, NULL }, { "FormDestroy", noop, NULL }, { "Timer1Timer", noop, NULL }, { "BitBtn1Click", on_cancel, (void *)(intptr_t)LJ_WARN_BLACKOUT }, { NULL, NULL, NULL } };
    static const lj_binding freeze_b[] = { { "FormActivate", noop, NULL }, { "FormDestroy", noop, NULL }, { "Timer1Timer", noop, NULL }, { "bCancelClick", on_cancel, (void *)(intptr_t)LJ_WARN_FROZEN }, { NULL, NULL, NULL } };
    static const lj_binding offline_b[] = { { "Timer1Timer", noop, NULL }, { "bCancelClick", on_cancel, (void *)(intptr_t)LJ_WARN_OFFLINE }, { NULL, NULL, NULL } };
    lj_ctx *ctx = lj_ctx_get();
    int slot;
    if ((int)w < 0 || w >= LJ_WARN_COUNT) return;
    slot = slot_of(w);
    if (!show) { destroy(&lj_warn_win[slot]); return; }
    if (ctx->main_ini && !lj_ini_geti(ctx->main_ini, "PREFS", lj_warn_ini_key[w], 1)) return;
    if (!lj_warn_win[slot]) {
        const lj_form_desc *d = slot == LJ_WARN_FROZEN ? &lj_form_TFREEZEWARNFORM : slot == LJ_WARN_OFFLINE ? &lj_form_TOFFLINEWARNINGFORM : &lj_form_TSTATUSWARNINGFORM;
        lj_window *win = lj_window_create(d, slot == LJ_WARN_FROZEN ? freeze_b : slot == LJ_WARN_OFFLINE ? offline_b : status_b);
        if (!win) { lj_log("warnings: cannot create %s", d->class_name); return; }
        lj_warn_win[slot] = win;
        show_centered(win, LJ_WIN_STAY_ON_TOP);
    }
    if (slot == LJ_WARN_BLACKOUT) lj_w_set_caption(lj_window_find(lj_warn_win[slot], "LStatus"), lj_warn_text[w]);   /* LStatus table, LORD_FIDELITY-12 */
}

/* ---- "Close Lightjockey ?" (TCLOSEORSAVEFORM: the three "was modified" lines as applicable, Close LightJockey Now ? on the orange panel) ---- */
static void close_answer(int yes)
{
    void (*fn)(int, void *) = lj_close_cb.fn; void *ud = lj_close_cb.ud;
    lj_close_cb.fn = NULL;
    destroy(&lj_close_win);
    if (fn) fn(yes, ud);
}
static void on_close_ok(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) close_answer(1); }
static void on_close_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) close_answer(0); }

void lj_win_close_prompt(void (*on_result)(int yes, void *ud), void *ud)
{
    static const lj_binding b[] = { { "FormShow", noop, NULL }, { NULL, NULL, NULL } };
    lj_ctx *ctx = lj_ctx_get();
    int any = ctx->modified[LJ_MOD_SEQ] || ctx->modified[LJ_MOD_CUE] || ctx->modified[LJ_MOD_CL];
    lj_widget *w;
    if (!any && ctx->main_ini && !lj_ini_geti(ctx->main_ini, "PREFS", "CloseConfirm", 1)) { if (on_result) on_result(1, ud); return; }   /* [PREFS] CloseConfirm=0: no prompt unless something is unsaved */
    if (lj_close_win) destroy(&lj_close_win);
    lj_close_win = lj_window_create(&lj_form_TCLOSEORSAVEFORM, b);
    if (!lj_close_win) { if (on_result) on_result(1, ud); return; }
    lj_close_cb.fn = on_result; lj_close_cb.ud = ud;
    if ((w = lj_window_find(lj_close_win, "LSeq")) != NULL) w->visible = ctx->modified[LJ_MOD_SEQ] != 0;
    if ((w = lj_window_find(lj_close_win, "LCue")) != NULL) w->visible = ctx->modified[LJ_MOD_CUE] != 0;
    if ((w = lj_window_find(lj_close_win, "LCl")) != NULL) w->visible = ctx->modified[LJ_MOD_CL] != 0;
    if ((w = lj_window_find(lj_close_win, "BitBtn1")) != NULL) { w->on_event = on_close_ok; w->ud = NULL; }     /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((w = lj_window_find(lj_close_win, "BitBtn2")) != NULL) { w->on_event = on_close_cancel; w->ud = NULL; }
    show_centered(lj_close_win, 0);
}

/* ---- TCONFIRMFORM: lj_win_message (OK only) and lj_win_confirm (OK / Cancel) ---- */
static void confirm_answer(int yes)
{
    void (*fn)(int, void *) = lj_confirm_cb.fn; void *ud = lj_confirm_cb.ud;
    lj_confirm_cb.fn = NULL;
    destroy(&lj_msg_win);
    if (fn) fn(yes, ud);
}
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; confirm_answer(1); }
static void on_bcancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) confirm_answer(0); }

static lj_window *confirm_form(const char *caption, const char *line1, const char *line2, int with_cancel)
{
    static const lj_binding b[] = { { "FormActivate", noop, NULL }, { "FormClose", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDeactivate", noop, NULL },
                                    { "FormShow", noop, NULL }, { "BOkClick", on_ok, NULL }, { "bSaveClick", on_ok, NULL }, { NULL, NULL, NULL } };
    lj_widget *w;
    if (lj_msg_win) confirm_answer(0);
    lj_msg_win = lj_window_create(&lj_form_TCONFIRMFORM, b);
    if (!lj_msg_win) return NULL;
    if (caption) lj_window_set_caption(lj_msg_win, caption);
    lj_w_set_caption(lj_window_find(lj_msg_win, "Label1"), line1 ? line1 : "");
    lj_w_set_caption(lj_window_find(lj_msg_win, "Label2"), line2 ? line2 : "");
    if ((w = lj_window_find(lj_msg_win, "bSave")) != NULL) w->visible = 0;      /* the Save variant is the W2 "was modified" prompt */
    if ((w = lj_window_find(lj_msg_win, "BCancel")) != NULL) { w->visible = with_cancel != 0; w->on_event = on_bcancel; w->ud = NULL; }
    {   /* a line wider than the 267-px DFM client widens the box, and Bevel1 (153 px in the DFM) grows to the text: PROPOSAL rig 12 Q9, the
         * longer prompts of 12 Q9 do not fit the DFM as designed */
        lj_widget *l1 = lj_window_find(lj_msg_win, "Label1"), *l2 = lj_window_find(lj_msg_win, "Label2"), *bv = lj_window_find(lj_msg_win, "Bevel1");
        lj_rect r;
        int need = 0, t;
        if (l1 && line1) need = l1->x + lj_text_width(lj_w_font_of(l1), line1) + 8;
        if (l2 && line2 && (t = l2->x + lj_text_width(lj_w_font_of(l2), line2) + 8) > need) need = t;
        lj_window_client_rect(lj_msg_win, &r);
        if (need > r.w) lj_window_resize(lj_msg_win, need, r.h);
        lj_window_client_rect(lj_msg_win, &r);
        if (bv && need > bv->w) bv->w = need < r.w ? need : r.w;   /* lines that fit the DFM's 153-px bevel keep it */
    }
    show_centered(lj_msg_win, LJ_WIN_STAY_ON_TOP);
    return lj_msg_win;
}

void lj_win_message(const char *caption, const char *text)
{
    lj_confirm_cb.fn = NULL;
    lj_log("%s: %s", caption ? caption : "Message", text ? text : "");
    confirm_form(caption, text, NULL, 0);
}

lj_window *lj_win_confirm(const char *caption, const char *line1, const char *line2, void (*on_result)(int yes, void *ud), void *ud)
{
    lj_window *w = confirm_form(caption, line1, line2, 1);
    if (!w) { if (on_result) on_result(0, ud); return NULL; }
    lj_confirm_cb.fn = on_result; lj_confirm_cb.ud = ud;
    return w;
}

void lj_warnings_close_all(void)
{
    int i;
    for (i = 0; i < LJ_WARN_COUNT; i++) destroy(&lj_warn_win[i]);
    lj_close_cb.fn = NULL; destroy(&lj_close_win);
    lj_confirm_cb.fn = NULL; destroy(&lj_msg_win);
}
