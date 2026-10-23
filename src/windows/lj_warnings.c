/* lj_warnings.c - see lj_warnings.h. One window per state, reused; every OnXxx the
 * DFMs declare is bound (ME-16). Headless (no host) the boxes are still built so the
 * tests click their buttons through lj_window_event. */
#include "windows/lj_warnings.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "fixlib/lj_profile.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_formsini.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_prefs.h"

#define LJ_WIN_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/); fsStayOnTop forms */

/* The LStatus / Label1 text table (LORD_FIDELITY-12), verbatim, one source tag per row. */
static const struct { const char *text, *src; } lj_warn_tab[LJ_WARN_COUNT] = {
    { "BLACKOUT ACTIVE",       "exe 0x18DFE0 (dfm_dup: TSTATUSWARNINGFORM LStatus design caption)" },
    { "Frozen !",              "DFM TFREEZEWARNFORM Label1" },
    { "FADE-OUT ACTIVE",       "exe 0x18DFF8 (written into TSTATUSWARNINGFORM LStatus)" },
    { "FADE-IN ACTIVE",        "exe 0x18E010 (written into TSTATUSWARNINGFORM LStatus)" },
    { "DMX Output is Offline", "DFM TOFFLINEWARNINGFORM Label1" },
};
static const char *const lj_warn_ini_key[LJ_WARN_COUNT] = { "ShowBlackoutWarning", "ShowFreezeWarning", "ShowBlackoutWarning", "ShowBlackoutWarning", "ShowOfflineWarning" };   /* Main.Ini [PREFS], ours (PROPOSAL: the design names the gates "Show*Warning" without the keys) */

static lj_window *lj_warn_win[LJ_WARN_COUNT];      /* BLACKOUT/FADE_OUT/FADE_IN share lj_warn_win[LJ_WARN_BLACKOUT] (one TSTATUSWARNINGFORM) */
static lj_window *lj_msg_win, *lj_close_win;
static struct { void (*fn)(int, void *); void *ud; } lj_close_cb, lj_confirm_cb;

const char *lj_warnings_status_text(enum lj_warning w) { return ((int)w >= 0 && w < LJ_WARN_COUNT) ? lj_warn_tab[w].text : ""; }
const char *lj_warnings_status_source(enum lj_warning w) { return ((int)w >= 0 && w < LJ_WARN_COUNT) ? lj_warn_tab[w].src : ""; }
static int slot_of(enum lj_warning w) { return (w == LJ_WARN_FADE_OUT || w == LJ_WARN_FADE_IN) ? LJ_WARN_BLACKOUT : (int)w; }
lj_window *lj_warnings_window(enum lj_warning w) { return ((int)w >= 0 && w < LJ_WARN_COUNT) ? lj_warn_win[slot_of(w)] : NULL; }
static lj_window *lj_note_win[2], *lj_laser_win, *lj_unblock_win;   /* W4: the two 'Notification !' boxes, the laser pair */
lj_window *lj_win_message_window(void) { return lj_msg_win ? lj_msg_win : lj_unblock_win ? lj_unblock_win : lj_note_win[0] ? lj_note_win[0] : lj_note_win[1] ? lj_note_win[1] : lj_laser_win; }   /* W4: assert_prompt / shot also reach the W4 prompt boxes */
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
    static const lj_binding status_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "BitBtn1Click", on_cancel, (void *)(intptr_t)LJ_WARN_BLACKOUT }, { NULL, NULL, NULL } };
    static const lj_binding freeze_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "bCancelClick", on_cancel, (void *)(intptr_t)LJ_WARN_FROZEN }, { NULL, NULL, NULL } };
    static const lj_binding offline_b[] = { { "Timer1Timer", lj_handler_noop, NULL }, { "bCancelClick", on_cancel, (void *)(intptr_t)LJ_WARN_OFFLINE }, { NULL, NULL, NULL } };
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
    if (slot == LJ_WARN_BLACKOUT) lj_w_set_caption(lj_window_find(lj_warn_win[slot], "LStatus"), lj_warn_tab[w].text);   /* LStatus table, LORD_FIDELITY-12 */
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
    static const lj_binding b[] = { { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
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
    static const lj_binding b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDeactivate", lj_handler_noop, NULL },
                                    { "FormShow", lj_handler_noop, NULL }, { "BOkClick", on_ok, NULL }, { "bSaveClick", on_ok, NULL }, { NULL, NULL, NULL } };
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
    destroy(&lj_note_win[0]); destroy(&lj_note_win[1]); destroy(&lj_laser_win); destroy(&lj_unblock_win);
}

/* ---- W4 dlg-desktop: TShowHotKeyWarningForm / TShowHardwareWarningForm ('Notification !', 12 Q9) and the laser pair
 * TLaserWarningForm ('Warning!') / TUnblockLasersForm ('Please confirm' / 'Enable laser output ?'), all centred on the main
 * window (LORD_FIDELITY-12). The 100 ms refresh (lj_warnings_w4_refresh) raises them: the Edit Hotkeys / Select Function
 * window opening, no sACN universe mapped once the desktop is up, a laser fixture asking for output. ---- */
static const char *const lj_note_key[2] = { "ShowHotkeyWarning", "ShowNoHardwareWarning" };   /* Main.Ini [MISC], exe 0x323724 / 0x323750 */
static int note_slot(enum lj_win id) { return id == LJ_WIN_HOTKEY_WARNING ? 0 : id == LJ_WIN_HARDWARE_WARNING ? 1 : -1; }

static void note_ok(int k)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *w = lj_note_win[k];
    if (!w) return;
    if (lj_w_checkbox_checked(lj_window_find(w, k ? "cbNoShow" : "cbShow"))) {   /* "Don't show this warning again" */
        if (ctx->prefs) { if (k) ctx->prefs->show_no_hardware_warning = 0; else ctx->prefs->show_hotkey_warning = 0; }
        lj_ini_seti(ctx->main_ini, "MISC", lj_note_key[k], 0);
    }
    destroy(&lj_note_win[k]);
}
static void on_note_btn(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) note_ok((int)(intptr_t)ud); }
static void on_note_click(lj_widget *w, void *ud) { (void)w; note_ok((int)(intptr_t)ud); }

static lj_window *note_open(int k)
{
    static const lj_binding hot_b[] = { { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    static const lj_binding hw_b[] = { { "BitBtn1Click", on_note_click, (void *)(intptr_t)1 }, { NULL, NULL, NULL } };
    lj_widget *b;
    if (lj_note_win[k]) return lj_note_win[k];
    lj_note_win[k] = lj_window_create(k ? &lj_form_TSHOWHARDWAREWARNINGFORM : &lj_form_TSHOWHOTKEYWARNINGFORM, k ? hw_b : hot_b);
    if (!lj_note_win[k]) { lj_log("warnings: cannot create the Notification box"); return NULL; }
    if (!k && (b = lj_window_find(lj_note_win[k], "BitBtn1")) != NULL) { b->on_event = on_note_btn; b->ud = (void *)(intptr_t)0; }   /* bkOK without OnClick in the DFM */
    show_centered(lj_note_win[k], LJ_WIN_STAY_ON_TOP);
    return lj_note_win[k];
}

/* ---- lasers ---- */
static int lj_laser_on;           /* "Enable laser output ?" answered Yes; LJ4EVR starts with laser output blocked (PROPOSAL rig, 12 Q9) */
static int lj_laser_asked;        /* the prompt was shown for this demand; re-armed when the demand drops to 0 */
static int lj_laser_held;         /* the block (S8 overwrite 0 on every laser channel) was posted */

int lj_laser_fixture(int f)       /* profile 118 "Mac 250 Laser, prototype rev.d" (tag MAC250L, PROPOSAL footprint kept) and any "Laser" profile (the RGB Laser 1.6 rows are unbound today) */
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_profile *p;
    if (!ctx->show || f < 0 || f >= LJ_FIXTURES || !ctx->show->fixture[f].link) return 0;
    p = lj_show_profile(ctx->show, f);
    return p && (strcmp(p->tag, "MAC250L") == 0 || strstr(p->name, "Laser") != NULL);
}
int lj_laser_enabled(void) { return lj_laser_on; }

static void laser_hold(int on)    /* S8 overwrite (11 P17): after every patch, so a blocked laser's channels leave at 0 */
{
    const lj_ctx *ctx = lj_ctx_get();
    int f, i;
    for (f = 0; ctx->show && f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        int base;
        if (!lj_laser_fixture(f) || !(p = lj_show_profile(ctx->show, f))) continue;
        base = lj_addr_code(ctx->show->fixture[f].link, ctx->show->fixture[f].address);   /* ponytail: the address code, i.e. an identity Global Patch for the laser's channels */
        for (i = 0; i < p->footprint && base + i < LJ_CHANS; i++) { lj_cmd c; memset(&c, 0, sizeof c); c.kind = LJ_CMD_OVERWRITE; c.a = (uint32_t)(base + i); c.b = on ? 0u : 256u; lj_ctx_post(&c); }
    }
    lj_laser_held = on;
}

static void laser_set(int on)
{
    lj_laser_on = on;
    laser_hold(!on);
    if (!on) lj_laser_asked = 1;   /* blocked from the warning: no prompt until the demand drops and comes back; the box goes on the next poll (DoMouseDown runs on mouse down, the up still targets it) */
}
static void on_laser_click(lj_widget *w, void *ud) { (void)w; (void)ud; laser_set(0); }   /* "(click to disable)" */
static void on_unblock(lj_widget *w, int ev, void *ud) { (void)w; if (ev != LJ_WEV_CLICK) return; destroy(&lj_unblock_win); if (ud) laser_set(1); }

static lj_window *laser_open(const char *name)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "DoMouseDown", on_laser_click, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    char text[64];
    if (!lj_laser_win) {
        if (!(lj_laser_win = lj_window_create(&lj_form_TLASERWARNINGFORM, b))) { lj_log("warnings: cannot create TLaserWarningForm"); return NULL; }
        show_centered(lj_laser_win, LJ_WIN_STAY_ON_TOP);
    }
    if (name) { snprintf(text, sizeof text, "%s emitting!", name); lj_w_set_caption(lj_window_find(lj_laser_win, "Label1"), text); }   /* exe 0x220708 '%s emitting!'; more than one: the DFM 'Laser(s) emitting!' */
    return lj_laser_win;
}
static lj_window *unblock_open(void)
{
    lj_widget *b;
    if (lj_unblock_win) return lj_unblock_win;
    if (!(lj_unblock_win = lj_window_create(&lj_form_TUNBLOCKLASERSFORM, NULL))) { lj_log("warnings: cannot create TUnblockLasersForm"); return NULL; }   /* no OnXxx in its DFM */
    if ((b = lj_window_find(lj_unblock_win, "BitBtn1")) != NULL) { b->on_event = on_unblock; b->ud = (void *)(intptr_t)1; }   /* bkYes */
    if ((b = lj_window_find(lj_unblock_win, "BitBtn2")) != NULL) { b->on_event = on_unblock; b->ud = NULL; }                 /* bkNo */
    lj_laser_asked = 1;
    show_centered(lj_unblock_win, LJ_WIN_STAY_ON_TOP);
    return lj_unblock_win;
}

static void laser_poll(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    static lj_frame fr;
    int f, i, n = 0, want = 0, emit = 0, one = -1;
    if (!ctx->show || !ctx->engine) return;
    lj_lock(); fr = *lj_engine_frame(ctx->engine); lj_unlock();
    for (f = 0; f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        int base, w = 0, e = 0;
        if (!lj_laser_fixture(f) || !(p = lj_show_profile(ctx->show, f))) continue;
        n++;
        base = lj_addr_code(ctx->show->fixture[f].link, ctx->show->fixture[f].address);
        for (i = 0; i < p->footprint && base + i < LJ_CHANS; i++) { w |= fr.merged[base + i] != 0; e |= fr.out[(base + i) / LJ_LINK_CH][(base + i) % LJ_LINK_CH] != 0; }
        want |= w;
        if (e) { emit = 1; one = one < 0 ? f : LJ_FIXTURES; }
    }
    if (!n) { destroy(&lj_laser_win); return; }
    if (!lj_laser_on) {
        destroy(&lj_laser_win);
        if (!lj_laser_held || emit) laser_hold(1);     /* also re-asserts after the DMX Output Monitor's Clear All overwrites */
        if (want && !lj_laser_asked) unblock_open();
        if (!want) lj_laser_asked = 0;
        return;
    }
    if (emit) {
        const lj_fixture *fx = one >= 0 && one < LJ_FIXTURES ? &ctx->show->fixture[one] : NULL;
        const lj_profile *p = fx ? lj_show_profile(ctx->show, one) : NULL;
        laser_open(fx ? (fx->user_name[0] ? fx->user_name : p ? p->tag : NULL) : NULL);   /* the icon caption: user name, else the short tag (02 s4.1) */
    } else destroy(&lj_laser_win);
}

lj_window *lj_warnings_w4_open(enum lj_win id)
{
    int k = note_slot(id);
    if (k >= 0) return note_open(k);
    if (id == LJ_WIN_LASER_WARNING) return laser_open(NULL);
    if (id == LJ_WIN_UNBLOCK_LASERS) return unblock_open();
    return NULL;
}
lj_window *lj_warnings_w4_get(enum lj_win id)
{
    int k = note_slot(id);
    return k >= 0 ? lj_note_win[k] : id == LJ_WIN_LASER_WARNING ? lj_laser_win : id == LJ_WIN_UNBLOCK_LASERS ? lj_unblock_win : NULL;
}
void lj_warnings_w4_close(enum lj_win id)
{
    int k = note_slot(id);
    if (k >= 0) destroy(&lj_note_win[k]);
    else if (id == LJ_WIN_LASER_WARNING) destroy(&lj_laser_win);
    else if (id == LJ_WIN_UNBLOCK_LASERS) destroy(&lj_unblock_win);
}

static int no_transmitter(const lj_sacn_cfg *c) { int i; for (i = 0; i < LJ_LINKS; i++) if (c->universe_of_link[i]) return 0; return 1; }

void lj_warnings_w4_refresh(enum lj_win id)
{
    static int hot_was;
    static const void *hw_seen;
    lj_ctx *ctx = lj_ctx_get();
    if (id == LJ_WIN_HOTKEY_WARNING) {         /* "Normal keyboard operation is suspended when the Hotkey Dialog Is Open." */
        int now = lj_win_get(LJ_WIN_EDIT_HOTKEYS) != NULL || lj_win_get(LJ_WIN_HOTKEY_FUNCTION) != NULL;
        if (now && !hot_was && ctx->prefs && ctx->prefs->show_hotkey_warning) note_open(0);
        hot_was = now;
    } else if (id == LJ_WIN_HARDWARE_WARNING) { /* our transmitter is the sACN sender: "configured" = at least one link mapped to a universe */
        if (ctx->sacn && (const void *)ctx->sacn != hw_seen && lj_desktop_window()) {
            hw_seen = ctx->sacn;
            if (no_transmitter(ctx->sacn) && (!ctx->prefs || ctx->prefs->show_no_hardware_warning)) note_open(1);
        }
    } else if (id == LJ_WIN_LASER_WARNING) laser_poll();
}
