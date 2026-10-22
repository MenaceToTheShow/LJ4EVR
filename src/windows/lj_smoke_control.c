/* lj_smoke_control.c - Smoke Control (lj_smoke_control.h). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_smoke_control.h"

#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_w_fader.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_windows.h"

#define LJ_SMOKE_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */
#define LJ_SMOKE_DURATION_S  (LJ_SMOKE_DURATION_MAX_MS / 1000)   /* the Duration fader in whole seconds ("10s" in smokecontrol.gif); PROPOSAL rig 04 s12 */

/* 32 steps 5 s .. 60 min, linear in log time: round(5 * 720^(i/31)) s (PROPOSAL rig, LJ_SMOKE_CYCLE_STEPS; smokecontrol.gif reads "35s") */
static const int lj_smoke_cycle_tab[LJ_SMOKE_CYCLE_STEPS] = {
    5000, 6000, 8000, 9000, 12000, 14000, 18000, 22000, 27000, 34000, 42000, 52000, 64000, 79000, 98000, 121000,
    149000, 184000, 228000, 282000, 349000, 431000, 533000, 659000, 815000, 1008000, 1246000, 1540000, 1905000, 2355000, 2912000, 3600000 };
static const char *const lj_smoke_shapes[LJ_SMOKE_F_COUNT] = { "Shape1", "Shape2", "Shape3", "Shape5", "Shape6" };
static const char *const lj_smoke_keys[LJ_SMOKE_F_COUNT]   = { "Level", "Duration", "Cycle", "HazerLevel", "HazerFan" };   /* Main.Ini [SMOKE], shipped */
static const int lj_smoke_max[LJ_SMOKE_F_COUNT]            = { 255, LJ_SMOKE_DURATION_S, LJ_SMOKE_CYCLE_STEPS - 1, 255, 255 };
static const char *const lj_smoke_haze_ctls[] = { "Bevel3", "Label5", "Label6", "Label7" };   /* hidden with the Hazer faders by View Haze Control */

static struct {
    lj_window *win;
    lj_widget *fader[LJ_SMOKE_F_COUNT];
    int idx[LJ_SMOKE_F_COUNT];
    int button_down, timer_on;
    lj_cmd last; int has_last;
} s;

int lj_smoke_cycle_ms(int step) { return lj_smoke_cycle_tab[step < 0 ? 0 : step >= LJ_SMOKE_CYCLE_STEPS ? LJ_SMOKE_CYCLE_STEPS - 1 : step]; }
lj_window *lj_smoke_control_get(void) { return s.win; }
lj_widget *lj_smoke_control_fader(enum lj_smoke_fader f) { return (int)f >= 0 && f < LJ_SMOKE_F_COUNT ? s.fader[f] : NULL; }
int lj_smoke_control_last_cmd(lj_cmd *out) { if (out && s.has_last) *out = s.last; return s.has_last; }

static void seti(const char *key, int v) { lj_ctx *c = lj_ctx_get(); if (c->main_ini) lj_ini_seti(c->main_ini, "SMOKE", key, v); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int value(enum lj_smoke_fader f) { return clampi(lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", lj_smoke_keys[f], 0), 0, lj_smoke_max[f]); }
static void text_hex(int v, char *buf, size_t cap) { snprintf(buf, cap, "%02X", (unsigned)v & 0xFFu); }
static void text_secs(int v, char *buf, size_t cap) { snprintf(buf, cap, "%ds", v); }
static void text_cycle(int v, char *buf, size_t cap)           /* PROPOSAL rig 04 s12: minutes past 59 s (only "35s" is in the capture) */
{
    int ms = lj_smoke_cycle_ms(v);
    if (ms < 60000) snprintf(buf, cap, "%ds", ms / 1000); else snprintf(buf, cap, "%dm", ms / 60000);
}

static void post(int ctl, uint32_t b, uint32_t c)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_SMOKE; cmd.a = (uint32_t)ctl; cmd.b = b; cmd.c = c;
    s.last = cmd; s.has_last = 1;
    lj_ctx_post(&cmd);
}

static void post_fader(enum lj_smoke_fader f)
{
    int v = value(f);
    switch (f) {
    case LJ_SMOKE_F_LEVEL:    post(LJ_SMOKE_LEVEL, (uint32_t)v, 0); break;
    case LJ_SMOKE_F_DURATION: post(LJ_SMOKE_DURATION_MS, (uint32_t)v * 1000u, 0); break;
    case LJ_SMOKE_F_CYCLE:    post(LJ_SMOKE_CYCLE_MS, (uint32_t)lj_smoke_cycle_ms(v), 0); break;
    case LJ_SMOKE_F_HAZE:     post(LJ_SMOKE_HAZE_LEVEL, (uint32_t)v, 0); break;
    default:                  post(LJ_SMOKE_FAN_LEVEL, (uint32_t)v, 0); break;
    }
}

/* Main.Ini [SMOKE] Toggle (ours): 0 = smoke while held, 2/4/6/8 = M_Toggle2..8 (the low second of "2-3 sec", PROPOSAL rig 04 s12), -1 = the Duration fader */
static uint32_t toggle_ms(void)
{
    int t = lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Toggle", 0);
    if (t < 0) return (uint32_t)value(LJ_SMOKE_F_DURATION) * 1000u;
    return (uint32_t)clampi(t, 0, 9) * 1000u;
}

void lj_smoke_control_button(int down)
{
    uint32_t t = toggle_ms();
    if (down && lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Reset", 1)) post(LJ_SMOKE_RESET, 0, 0);       /* "Reset cycle on smoke" (shipped Reset=1) */
    if (t && !down) return;                                          /* toggle mode: the engine ends the burst (mouse only, 04 s12) */
    s.button_down = down != 0;
    post(LJ_SMOKE_BUTTON, (uint32_t)s.button_down, t);
    lj_desktop_led_set(LJ_LED_SMOKE, s.button_down || t);          /* lit now; the next view copy (smoke_active) takes over */
}

void lj_smoke_control_timer(int on)
{
    lj_widget *b = s.win ? lj_window_find(s.win, "dSmokeTimer") : NULL;
    s.timer_on = on != 0;
    if (b && lj_w_speedbutton_down(b) != s.timer_on) lj_w_speedbutton_set_down(b, s.timer_on);
    post(LJ_SMOKE_TIMER, (uint32_t)s.timer_on, 0);
}

/* ---- widget events ---- */
static void fader_event(lj_widget *w, int ev, void *ud)
{
    int f = *(int *)ud;
    if (ev != LJ_WEV_CHANGE) return;
    seti(lj_smoke_keys[f], lj_w_fader_value(w));
    post_fader((enum lj_smoke_fader)f);
}
static void unlatch(lj_widget *w) { ((lj_w_cap *)w)->down = 0; lj_widget_invalidate(w); }   /* GroupIndex without AllowAllUp would stick down (TSpeedButton.SetDown) */
static void smoke_event(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev == LJ_WEV_MOUSE_DOWN && lj_window_last_event(s.win)->button == 1) lj_smoke_control_button(1);
    else if (ev == LJ_WEV_MOUSE_UP && s.button_down) lj_smoke_control_button(0);
    else if (ev == LJ_WEV_CLICK) unlatch(w);
}
static void timer_event(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_CLICK) lj_smoke_control_timer(lj_w_speedbutton_down(w)); }
static void reset_event(lj_widget *w, int ev, void *ud) { (void)ud; if (ev == LJ_WEV_CLICK) { unlatch(w); post(LJ_SMOKE_RESET, 0, 0); } }

static void view_haze(void)
{
    int on = lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "ViewHaze", 1), i;
    lj_widget *w;
    for (i = 0; i < (int)LJ_COUNT_OF(lj_smoke_haze_ctls); i++) if ((w = lj_window_find(s.win, lj_smoke_haze_ctls[i])) != NULL) w->visible = on != 0;
    for (i = LJ_SMOKE_F_HAZE; i < LJ_SMOKE_F_COUNT; i++) if (s.fader[i]) s.fader[i]->visible = on != 0;
    lj_window_invalidate_all(s.win);
}
static void sync_checks(void)
{
    lj_menu *m = lj_window_menus(s.win);
    int t = lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Toggle", 0);
    lj_menu_set_checked(m, "M_ViewHaze", lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "ViewHaze", 1));
    lj_menu_set_checked(m, "M_EnableOnStart", lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "TimerOnStart", 0));
    lj_menu_set_checked(m, "M_ResetOnSmoke", lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Reset", 1));
    lj_menu_set_checked(m, "M_UseLevel", lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "UseLevel", 0));
    lj_menu_set_checked(m, "M_Toggle2", t == 2); lj_menu_set_checked(m, "M_Toggle4", t == 4);
    lj_menu_set_checked(m, "M_Toggle6", t == 6); lj_menu_set_checked(m, "M_Toggle8", t == 8);
    lj_menu_set_checked(m, "M_ToggleDur", t < 0);
}
/* UseLevel and TimerOnStart are persisted only: the engine's LJ_SMOKE_BUTTON_USES_LEVEL and the app's startup read them
 * (ponytail: no LJ_CMD_SMOKE kind carries UseLevel; add one when the rig confirms the button level differs from the timer's) */
static void prefs_click(lj_widget *w, void *ud)
{
    static const char *const names[] = { "M_ViewHaze", "M_EnableOnStart", "M_ResetOnSmoke", "M_UseLevel" };
    static const char *const keys[] = { "ViewHaze", "TimerOnStart", "Reset", "UseLevel" };
    static const int dflt[] = { 1, 0, 1, 0 };
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    int i;
    (void)w; (void)ud;
    for (i = 0; it && it->name && i < (int)LJ_COUNT_OF(names); i++)
        if (!strcmp(it->name, names[i])) seti(keys[i], !lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", keys[i], dflt[i]));
    view_haze();
    sync_checks();
}
static void toggle_click(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    (void)w; (void)ud;
    if (!it || !it->name) return;
    seti("Toggle", !strcmp(it->name, "M_ToggleDur") ? -1 : (lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Toggle", 0) == it->tag ? 0 : it->tag));   /* the checked radio clicked again = hold mode */
    sync_checks();
}
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_smoke_control_close(); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_smoke_control_refresh(); }

static const lj_binding lj_smoke_bindings[] = {
    { "SmokePrefsClick", prefs_click, NULL }, { "M_Toggle2Click", toggle_click, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "Timer1Timer", on_timer, NULL }, { "Timer2Timer", on_timer, NULL },
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_smoke_control_open(void)
{
    lj_widget *w;
    int i;
    if (s.win) return s.win;
    memset(&s, 0, sizeof s);
    s.win = lj_window_create(&lj_form_TSMOKEFORM, lj_smoke_bindings);
    if (!s.win) { lj_log("smoke control: cannot create TSmokeForm"); return NULL; }
    for (i = 0; i < LJ_SMOKE_F_COUNT; i++) {
        lj_widget *shape = lj_window_find(s.win, lj_smoke_shapes[i]);
        if (!shape) { lj_smoke_control_close(); return NULL; }
        shape->visible = 0;                                          /* the 32x160 placeholders = LJ_FADER_W; the faders take their rects */
        s.idx[i] = i;
        s.fader[i] = lj_w_fader_create(shape->parent, NULL);
        s.fader[i]->x = shape->x; s.fader[i]->y = shape->y; s.fader[i]->w = shape->w; s.fader[i]->h = shape->h;
        ((lj_w_fader *)s.fader[i])->max = lj_smoke_max[i];
        ((lj_w_fader *)s.fader[i])->text = i == LJ_SMOKE_F_DURATION ? text_secs : i == LJ_SMOKE_F_CYCLE ? text_cycle : text_hex;   /* smokecontrol.gif: "FF" "10s" "35s" "29" "95" */
        lj_w_fader_set_value(s.fader[i], value((enum lj_smoke_fader)i));
        s.fader[i]->on_event = fader_event; s.fader[i]->ud = &s.idx[i];
    }
    if ((w = lj_window_find(s.win, "dSmoke")) != NULL) { w->on_event = smoke_event; w->ud = NULL; }        /* no DFM events on the three buttons: LJ2 wires them in code */
    if ((w = lj_window_find(s.win, "dSmokeTimer")) != NULL) { w->on_event = timer_event; w->ud = NULL; }
    if ((w = lj_window_find(s.win, "dReset")) != NULL) { w->on_event = reset_event; w->ud = NULL; }
    view_haze();
    sync_checks();
    for (i = 0; i < LJ_SMOKE_F_COUNT; i++) post_fader((enum lj_smoke_fader)i);   /* the engine starts from the saved settings */
    lj_smoke_control_refresh();
    lj_win_show(s.win, LJ_SMOKE_STAY_ON_TOP);
    return s.win;
}

void lj_smoke_control_close(void)
{
    if (!s.win) return;
    lj_win_hide(s.win);
    lj_window_destroy(s.win);
    memset(&s, 0, sizeof s);
}

void lj_smoke_control_refresh(void)
{
    const lj_engine_status *v = &lj_ctx_get()->view;
    lj_widget *b;
    if (!v->tick_no) return;                                         /* no view copy yet: keep what this window posted */
    lj_desktop_led_set(LJ_LED_SMOKE, v->smoke_active);              /* the toolbar Smoke button follows the firing, as lj_desktop_refresh does */
    s.timer_on = v->smoke_timer != 0;
    if (s.win && (b = lj_window_find(s.win, "dSmokeTimer")) != NULL && lj_w_speedbutton_down(b) != s.timer_on) lj_w_speedbutton_set_down(b, s.timer_on);
}

/* ---- hotkeys (io/lj_actions.h Other rows) ---- */
static void fn_smoke(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (fn == LJ_FN_SMOKE_BUTTON_FLASH) {                            /* hotkeys smoke while held: the toggle preference is mouse only (04 s12) */
        if (edge && lj_ini_geti(lj_ctx_get()->main_ini, "SMOKE", "Reset", 1)) post(LJ_SMOKE_RESET, 0, 0);
        s.button_down = edge != 0;
        post(LJ_SMOKE_BUTTON, (uint32_t)s.button_down, 0);
        return;
    }
    if (!edge) return;
    if (fn == LJ_FN_SMOKE_CONTROL) lj_win_toggle(LJ_WIN_SMOKE_CONTROL);
    else if (fn == LJ_FN_TOGGLE_SMOKE_TIMER) lj_smoke_control_timer(!s.timer_on);
}

void lj_smoke_control_bind_fns(void)
{
    lj_fn_bind(LJ_FN_SMOKE_CONTROL, fn_smoke, NULL);
    lj_fn_bind(LJ_FN_SMOKE_BUTTON_FLASH, fn_smoke, NULL);
    lj_fn_bind(LJ_FN_TOGGLE_SMOKE_TIMER, fn_smoke, NULL);
}
