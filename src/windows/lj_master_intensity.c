/* lj_master_intensity.c - Master Intensity (lj_master_intensity.h). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_master_intensity.h"

#include "core/lj_locale.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "io/lj_dmxin.h"
#include "io/lj_fn.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_windows.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

#define LJ_MI_COLS      (1 + LJ_SUBMASTERS)
#define LJ_MI_FADER_H   135    /* measured from masterintensity.gif, rig item (~132); 135 keeps the 103-px track's half exact */
#define LJ_MI_FADETIMES 20     /* 0.5 .. 10.0 s in 0.5 s steps (11 G11); PROPOSAL: the list's items are unverified, the gif shows "2.0 Sec" */

/* Column geometry: note 07 s10 reads master ~55 / subs ~40 from the reduced gif; the nine columns must fit the
 * 325 px of Panel1 left of Panel2, so 40 / 35 (PROPOSAL, rig item). Rows measured from masterintensity.gif. */
static const int lj_mi_col_x[LJ_MI_COLS] = { 4, 46, 81, 116, 151, 186, 221, 256, 291 };
static const int lj_mi_col_w[LJ_MI_COLS] = { 40, 33, 33, 33, 33, 33, 33, 33, 33 };
static const char *const lj_mi_fader_names[LJ_MI_COLS] = { "fader_master", "fader_1", "fader_2", "fader_3", "fader_4", "fader_5", "fader_6", "fader_7", "fader_8" };
enum { LJ_MI_Y_SYNC = 2, LJ_MI_Y_NAME = 18, LJ_MI_Y_FADER = 32, LJ_MI_Y_BUMP = 168, LJ_MI_Y_MODE = 192 };

static const char *const lj_mi_mode_items[] = { "Master Proportional", "Master LoTP", "Master HTP", "Master no Effect" };   /* exe strings 0x23BCBC.. */
static const char *const lj_mi_mode_short[] = { "PR", "LoTP", "HTP", "None" };                                            /* the label under the fader: "PR" / "None" in the gif */

static struct {
    lj_window *win;
    lj_widget *fader[LJ_MI_COLS], *mode[LJ_MI_COLS], *name[LJ_MI_COLS], *sync[LJ_MI_COLS], *bump[LJ_MI_COLS];
    int level[LJ_MI_COLS];
    int col[LJ_MI_COLS];                      /* the popup's ud: column index */
    int blackout, gm_fade;
    const lj_engine_status *view;
    lj_cmd last; int has_last;
    char fadetime_text[LJ_MI_FADETIMES][12]; const char *fadetime_items[LJ_MI_FADETIMES];
} s;

static const lj_engine_status lj_zero_view = { 0 };
static lj_ctx *C(void) { return lj_ctx_get(); }
static const lj_engine_status *view(void) { return s.view ? s.view : &lj_zero_view; }
static void main_seti(const char *key, int v) { lj_ctx *c = C(); if (c->main_ini) lj_ini_seti(c->main_ini, "GLOBALINTENSITY", key, v); }
static void host_show(lj_window *w, int open) { const lj_cue_form_host *h = lj_cue_form_get_host(); if (w && h && h->show) h->show(h->ud, w, open); }

static void post3(int kind, uint32_t a, uint32_t b, uint32_t c)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = c;
    s.last = cmd; s.has_last = 1;
    lj_ctx_post(&cmd);
}

static int mode_of(int sub) { lj_ctx *c = C(); return sub >= 1 && sub <= LJ_SUBMASTERS && c->show ? c->show->submaster_mode[sub - 1] : 0; }

lj_window *lj_master_intensity_get(void) { return s.win; }
void lj_master_intensity_set_view(const lj_engine_status *v) { s.view = v; }
lj_widget *lj_master_intensity_fader(int which) { return which >= 0 && which < LJ_MI_COLS ? s.fader[which] : NULL; }
const char *const *lj_master_intensity_mode_items(int *n) { if (n) *n = LJ_COUNT_OF(lj_mi_mode_items); return lj_mi_mode_items; }
int lj_master_intensity_last_cmd(lj_cmd *out) { if (out && s.has_last) *out = s.last; return s.has_last; }
int lj_master_intensity_level(int which) { return which >= 0 && which < LJ_MI_COLS ? s.level[which] : 0; }

void lj_master_intensity_set(int which, int level)
{
    if (which < 0 || which >= LJ_MI_COLS) return;
    if (level < 0) level = 0;
    if (level > LJ_GM_DOMAIN) level = LJ_GM_DOMAIN;
    s.level[which] = level;
    if (s.fader[which]) lj_w_fader_set_value(s.fader[which], level);
    post3(LJ_CMD_SET_MASTER, (uint32_t)which, (uint32_t)level, (uint32_t)mode_of(which));
}

void lj_master_intensity_set_mode(int sub, int mode)
{
    lj_ctx *c = C();
    if (sub < 1 || sub > LJ_SUBMASTERS || mode < 0 || mode > LJ_MASTER_NO_EFFECT) return;
    if (c->show) { lj_lock(); c->show->submaster_mode[sub - 1] = (uint8_t)mode; lj_unlock(); }
    if (s.mode[sub]) lj_w_set_caption(s.mode[sub], lj_mi_mode_short[mode]);
    post3(LJ_CMD_SET_MASTER, (uint32_t)sub, (uint32_t)s.level[sub], (uint32_t)mode);
}

/* ---- widget events ---- */
static void fader_change(lj_widget *w, int ev, void *ud)
{
    int i = *(int *)ud;
    if (ev != LJ_WEV_CHANGE) return;
    s.level[i] = lj_w_fader_value(w);
    post3(LJ_CMD_SET_MASTER, (uint32_t)i, (uint32_t)s.level[i], (uint32_t)mode_of(i));
}
static void mode_pick(int item, void *ud) { lj_master_intensity_set_mode(*(int *)ud, item); }
static void mode_click(lj_widget *w, int ev, void *ud)
{
    int i = *(int *)ud, ax, ay, rx, ry;
    if (ev != LJ_WEV_MOUSE_DOWN || i == 0) return;              /* the Grand Master has no mode */
    lj_widget_abs(w, &ax, &ay);
    lj_widget_abs(lj_window_root(s.win), &rx, &ry);                /* the popup takes root-relative coordinates */
    lj_cue_popup_open(s.win, lj_mi_mode_items, LJ_COUNT_OF(lj_mi_mode_items), ax - rx, ay - ry + w->h, mode_pick, ud);
}
static void w2_event(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_CLICK) lj_log("lj_master_intensity: %s: Sync / Bump are W2", lj_w_caption(w) ? lj_w_caption(w) : "?"); }

static int fadetime_ms(void)
{
    lj_widget *cb = s.win ? lj_window_find(s.win, "cbFadetime") : NULL;
    int i = cb ? ((lj_w_combobox *)cb)->index : lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "Fadetime", 3);
    if (i < 0) i = 0;
    if (i >= LJ_MI_FADETIMES) i = LJ_MI_FADETIMES - 1;
    main_seti("Fadetime", i);
    return (i + 1) * 500;                                        /* 500 .. 10000, LJ_GM_FADE_MIN/MAX_MS */
}

static void all(int level) { int i; for (i = 0; i < LJ_MI_COLS; i++) lj_master_intensity_set(i, level); }
static void b_all100(lj_widget *w, void *ud) { (void)w; (void)ud; all(LJ_GM_DOMAIN); }
static void b_all0(lj_widget *w, void *ud) { (void)w; (void)ud; all(0); }
static void b_fade_out(lj_widget *w, void *ud) { (void)w; (void)ud; post3(LJ_CMD_GM_FADE, 0, (uint32_t)fadetime_ms(), 0); }
static void b_fade_in(lj_widget *w, void *ud) { (void)w; (void)ud; post3(LJ_CMD_GM_FADE, 1, (uint32_t)fadetime_ms(), 0); }
static void b_blackout(lj_widget *w, void *ud) { (void)ud; s.blackout = lj_w_speedbutton_down(w); post3(LJ_CMD_BLACKOUT, 0, (uint32_t)s.blackout, 0); }
static void b_restore(lj_widget *w, void *ud)
{
    lj_widget *bo = lj_window_find(s.win, "bBlackOut");
    (void)w; (void)ud;
    s.blackout = 0;
    if (bo) lj_w_speedbutton_set_down(bo, 0);
    post3(LJ_CMD_BLACKOUT, 0, 0, 0);
}

static void sync_checks(void)
{
    lj_menu *m = lj_window_menus(s.win);
    lj_menu_set_checked(m, "M_MasterOnly", lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "MasterOnly", 0));
    lj_menu_set_checked(m, "M_Pct", lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "Show Pct", 1));
    lj_menu_set_checked(m, "M_SynchronizeBump", lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "SyncBump", 0));
}
static void apply_master_only(void)
{
    int only = lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "MasterOnly", 0), i;
    for (i = 1; i < LJ_MI_COLS; i++) {
        lj_widget *ws[5] = { s.fader[i], s.mode[i], s.name[i], s.sync[i], s.bump[i] };
        int k;
        for (k = 0; k < 5; k++) if (ws[k]) ws[k]->visible = !only;
    }
    lj_window_invalidate_all(s.win);
}
static void prefs_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "M_MasterOnly")) { main_seti("MasterOnly", !lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "MasterOnly", 0)); apply_master_only(); }
    else if (!strcmp(it->name, "M_Pct")) main_seti("Show Pct", !lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "Show Pct", 1));   /* ponytail: lj_w_fader prints % only; the absolute readout ("FF") waits for the widget */
    else if (!strcmp(it->name, "M_SynchronizeBump")) main_seti("SyncBump", !lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "SyncBump", 0));
    sync_checks();
}
static void internal_patch(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_INTENSITY_PATCH); }   /* Patch Sub Masters [04 s11] */
static void int_groups(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_INTENSITY_GROUPS); }   /* W4 acceptance: Preferences > Groups [04 s11] */

static const lj_binding lj_mi_bindings[] = {
    { "bAll100Click", b_all100, NULL }, { "bAll0Click", b_all0, NULL }, { "bFadeOutClick", b_fade_out, NULL }, { "bFadeInClick", b_fade_in, NULL },
    { "bBlackOutClick", b_blackout, NULL }, { "bRestoreClick", b_restore, NULL }, { "InternalPatch1Click", internal_patch, NULL }, { "PrefsClick", prefs_click, NULL },
    { "M_IntGroupsClick", int_groups, NULL }, { "Help1Click", lj_help_viewer_click, "controls" },
    { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormHide", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

static lj_widget *mk_button(lj_widget *parent, int x, int y, int w, int h, const char *caption, void *ud)
{
    lj_widget *b = lj_w_speedbutton_create(parent, NULL);
    b->kind = LJ_K_SPEEDBUTTON;                                   /* a NULL desc leaves kind 0: the caption and latch helpers key on it */
    b->x = x; b->y = y; b->w = w; b->h = h;
    lj_w_set_caption(b, caption);
    b->on_event = w2_event; b->ud = ud;
    return b;
}
static const lj_ctl_desc lj_mi_label_desc = { LJ_K_LABEL, "lj_mi_label", 0, 0, 0, 0, .tag = 0 };   /* lj_w_label's paint reads desc->alignment / layout: a code-built label needs a row */
static lj_widget *mk_label(lj_widget *parent, int x, int y, int w, int h, const char *caption)
{
    lj_widget *l = lj_w_label_create(parent, NULL);
    l->kind = LJ_K_LABEL; l->desc = &lj_mi_label_desc;
    l->x = x; l->y = y; l->w = w; l->h = h;
    lj_w_set_caption(l, caption);
    return l;
}

lj_window *lj_master_intensity_open(void)
{
    lj_ctx *c = C();
    lj_widget *panel, *cb;
    int i;
    if (s.win) return s.win;
    s.win = lj_window_create(&lj_form_TMASTERINTENSITYFORM, lj_mi_bindings);
    if (!s.win) { lj_log("lj_master_intensity: TMasterIntensityForm failed to build"); return NULL; }
    panel = lj_window_find(s.win, "Panel1");
    if (!panel) { lj_window_destroy(s.win); s.win = NULL; return NULL; }
    for (i = 1; i <= 19; i++) {                                   /* the DFM's placeholder labels sit under the runtime-built columns */
        char n[12];
        lj_widget *l;
        snprintf(n, sizeof n, "Label%d", i);
        if ((l = lj_window_find(s.win, n)) != NULL && l->parent == panel) l->visible = 0;
    }
    for (i = 0; i < LJ_MI_COLS; i++) {
        char nm[8];
        const char *name = "Master";
        int x = lj_mi_col_x[i], w = lj_mi_col_w[i];
        s.col[i] = i;
        if (i > 0) {
            if (c->show && c->show->submaster_name[i - 1][0]) name = c->show->submaster_name[i - 1];
            else { snprintf(nm, sizeof nm, "%d", i); name = nm; }
        }
        s.sync[i] = mk_button(panel, x, LJ_MI_Y_SYNC, w, 14, i == 0 ? "Clr Sync" : "Sync", &s.col[i]);   /* exe strings 0x23CE90 / 0x23CEA8 */
        s.name[i] = mk_label(panel, x, LJ_MI_Y_NAME, w, 13, name);
        s.fader[i] = lj_w_fader_create(panel, NULL);
        s.fader[i]->name = lj_mi_fader_names[i];                 /* docs/ui-script.md: fader_master, fader_1 .. fader_8 */
        s.fader[i]->x = x + (w - LJ_FADER_W) / 2; s.fader[i]->y = LJ_MI_Y_FADER; s.fader[i]->h = LJ_MI_FADER_H;
        s.level[i] = i == 0 ? view()->gm : view()->sub[i - 1];
        if (!s.view) s.level[i] = LJ_GM_DOMAIN;                   /* before the first view copy every master is full (lj_engine_create) */
        lj_w_fader_set_value(s.fader[i], s.level[i]);
        s.fader[i]->on_event = fader_change; s.fader[i]->ud = &s.col[i];
        s.bump[i] = mk_button(panel, x, LJ_MI_Y_BUMP, w, 17, "Bump", &s.col[i]);                        /* exe string 0x23CE80 */
        s.mode[i] = mk_label(panel, x, LJ_MI_Y_MODE, w, 13, i == 0 ? "None" : lj_mi_mode_short[mode_of(i)]);
        s.mode[i]->on_event = mode_click; s.mode[i]->ud = &s.col[i];
    }
    for (i = 0; i < LJ_MI_FADETIMES; i++) {
        snprintf(s.fadetime_text[i], sizeof s.fadetime_text[i], "%d%c%d Sec", (i + 1) / 2, lj_locale_decimal(), (i + 1) % 2 ? 5 : 0);   /* D6 */
        s.fadetime_items[i] = s.fadetime_text[i];
    }
    if ((cb = lj_window_find(s.win, "cbFadetime")) != NULL) {
        int idx = lj_ini_geti(lj_ctx_get()->main_ini, "GLOBALINTENSITY", "Fadetime", 3);
        lj_w_combobox_set_items(cb, s.fadetime_items, LJ_MI_FADETIMES);
        ((lj_w_combobox *)cb)->index = idx < 0 ? 0 : idx >= LJ_MI_FADETIMES ? LJ_MI_FADETIMES - 1 : idx;
    }
    apply_master_only();
    sync_checks();
    lj_master_intensity_refresh();
    host_show(s.win, 1);
    return s.win;
}

void lj_master_intensity_close(void)
{
    if (!s.win) return;
    host_show(s.win, 0);
    lj_window_destroy(s.win);
    s.win = NULL;
    memset(s.fader, 0, sizeof s.fader); memset(s.mode, 0, sizeof s.mode); memset(s.name, 0, sizeof s.name);
    memset(s.sync, 0, sizeof s.sync); memset(s.bump, 0, sizeof s.bump);
}

/* W5 integrator 2 (05 s3.2): a master driven by a saved DMX-in function (Master Intensity, Intensity Gr.n) shows its mode
 * text red and refuses the mouse while assigned, whether or not input arrives; Disable DMX-in functions releases it */
static const lj_ctl_desc lj_mi_label_remote = { LJ_K_LABEL, "lj_mi_label", 0, 0, 0, 0, .font_color = LJ_CLR(0xFF0000) };
static int remote_of(int col)
{
    const lj_ctx *c = C();
    int k;
    if (!c->dmxin || !c->dmxin->enabled || c->dmxin->functions_off || !c->dmxin_map) return 0;
    for (k = 0; k < LJ_LINK_CH; k++) {
        const lj_dmxin_assign *a = &c->dmxin_map->ch[k];
        if (col == 0 ? a->fn == LJ_DIF_MASTER_INTENSITY : (a->fn == LJ_DIF_INTENSITY_GROUP && a->num == (uint32_t)col)) return 1;
    }
    return 0;
}

void lj_master_intensity_refresh(void)
{
    const lj_engine_status *v = view();
    lj_widget *w;
    int i;
    if (!s.win || !s.view) return;
    for (i = 0; i < LJ_MI_COLS; i++) {
        int lv = i == 0 ? v->gm : v->sub[i - 1], rem = remote_of(i);
        lj_w_fader *f = (lj_w_fader *)s.fader[i];
        if (s.mode[i] && (s.mode[i]->desc == &lj_mi_label_remote) != rem) { s.mode[i]->desc = rem ? &lj_mi_label_remote : &lj_mi_label_desc; lj_widget_invalidate(s.mode[i]); }
        if (s.fader[i] && (int)s.fader[i]->enabled == rem) { s.fader[i]->enabled = !rem; lj_widget_invalidate(s.fader[i]); }
        if (f && !f->dragging && !f->pressed && lj_w_fader_value(s.fader[i]) != lv) { lj_w_fader_set_value(s.fader[i], lv); s.level[i] = lv; }
    }
    s.blackout = v->blackout; s.gm_fade = v->gm_fade;
    if ((w = lj_window_find(s.win, "bBlackOut")) != NULL) lj_w_speedbutton_set_down(w, v->blackout);
    if ((w = lj_window_find(s.win, "bFadeOut")) != NULL) lj_w_speedbutton_set_down(w, v->gm_fade == 1);
    if ((w = lj_window_find(s.win, "bFadeIn")) != NULL) {
        int en = v->gm_fade == 1 || v->blackout;              /* CHM 7213: Fade In only during a fade-out or blackout */
        lj_w_speedbutton_set_down(w, v->gm_fade == 2);
        if ((int)w->enabled != en) { w->enabled = en != 0; lj_widget_invalidate(w); }
    }
}

/* ---- hotkeys (io/lj_actions.h intensity rows) ---- */
static void fn_master(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)ud;
    if (fn == LJ_FN_BLACKOUT_FLASH) { post3(LJ_CMD_BLACKOUT, 0, (uint32_t)edge, 0); return; }
    if (!edge) return;
    switch (fn) {
    case LJ_FN_TOGGLE_MASTER_INTENSITY: lj_win_toggle(LJ_WIN_MASTER_INTENSITY); break;
    case LJ_FN_BLACKOUT:         post3(LJ_CMD_BLACKOUT, 0, 1, 0); break;
    case LJ_FN_BLACKOUT_RESTORE: s.blackout = !s.blackout; post3(LJ_CMD_BLACKOUT, 0, (uint32_t)s.blackout, 0); break;
    case LJ_FN_MASTER_OFF:       lj_master_intensity_set(0, 0); break;
    case LJ_FN_MASTER_FULL:      lj_master_intensity_set(0, LJ_GM_DOMAIN); break;
    case LJ_FN_MASTER_PLUS1:     lj_master_intensity_set(0, s.level[0] + 1); break;
    case LJ_FN_MASTER_PLUS10:    lj_master_intensity_set(0, s.level[0] + 10); break;
    case LJ_FN_MASTER_MINUS1:    lj_master_intensity_set(0, s.level[0] - 1); break;
    case LJ_FN_MASTER_MINUS10:   lj_master_intensity_set(0, s.level[0] - 10); break;
    case LJ_FN_MASTER_LEVEL:     lj_master_intensity_set(0, (int)((prog > 100u ? 100u : prog) * LJ_GM_DOMAIN + 50u) / 100); break;   /* W2 midi CC extension: prog = 0..100 % */
    default: break;
    }
}

void lj_master_intensity_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_TOGGLE_MASTER_INTENSITY, LJ_FN_BLACKOUT, LJ_FN_BLACKOUT_RESTORE, LJ_FN_BLACKOUT_FLASH,
                                       LJ_FN_MASTER_OFF, LJ_FN_MASTER_FULL, LJ_FN_MASTER_PLUS1, LJ_FN_MASTER_PLUS10, LJ_FN_MASTER_MINUS1, LJ_FN_MASTER_MINUS10, LJ_FN_MASTER_LEVEL };
    int i;
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_master, NULL);
    s.level[0] = LJ_GM_DOMAIN;
}
