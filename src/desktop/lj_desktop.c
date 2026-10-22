/* lj_desktop.c - the process context (lj_ctx_*), the desktop window's life cycle
 * (create / destroy / refresh / key), the host hook and the action registry
 * (lj_desktop.h). The pieces live beside it: lj_main_window.c (menu, ME-29),
 * lj_toolbars.c, lj_statusbar.c, lj_desktop_tabs.c (tabs + icon area),
 * lj_fixture_icons.c, lj_selection.c, lj_unassigned.c. */
#include "desktop/lj_desktop.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_statusbar.h"
#include "desktop/lj_toolbars.h"
#include "desktop/lj_unassigned.h"
#include "io/lj_fn.h"
#include "model/lj_defaultout.h"
#include "ui/forms/lj_forms_gen.h"
#include "windows/lj_windows.h"

static lj_ctx lj_the_ctx;
static lj_window *lj_the_desktop;
static lj_desktop_host lj_the_host; static int lj_have_host;
static struct { lj_desktop_action_fn fn; void *ud; unsigned complained:1; } lj_actions[LJ_ACT_COUNT];

lj_ctx *lj_ctx_get(void) { return &lj_the_ctx; }

void lj_ctx_post(const lj_cmd *cmd)
{
    lj_cmd c = *cmd;
    if (c.at_us == 0) c.at_us = lj_clock_us();
    lj_lock();
    if (lj_the_ctx.engine) lj_engine_cmd(lj_the_ctx.engine, &c);
    lj_unlock();
}

void lj_ctx_set_modified(enum lj_modified which, int on)
{
    static const enum lj_status_field field[3] = { LJ_SB_SEQ_MOD, LJ_SB_CUE_MOD, LJ_SB_CL_MOD };
    if ((int)which < 0 || (int)which > 2) return;
    lj_the_ctx.modified[which] = (uint8_t)(on != 0);
    lj_desktop_status_flag(field[which], on);
}

void lj_ctx_set_scene(uint16_t index)
{
    lj_cmd c;
    lj_the_ctx.current_scene = index;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_CURRENT_SEQ; c.a = index; c.b = lj_the_ctx.blind; c.seq[0] = lj_the_ctx.show ? lj_the_ctx.show->current_seq : NULL;
    lj_ctx_post(&c);
    lj_desktop_toolbar_update();
}

/* ---- host + actions ---- */
void lj_desktop_set_host(const lj_desktop_host *host)
{
    if (host) { lj_the_host = *host; lj_have_host = 1; } else lj_have_host = 0;
}

int lj_desktop_host_open(lj_window *w, int stay_on_top)
{
    if (!lj_have_host || !lj_the_host.open) return -1;
    return lj_the_host.open(lj_the_host.ud, w, stay_on_top);
}

void lj_desktop_host_close(lj_window *w)
{
    if (lj_have_host && lj_the_host.close) lj_the_host.close(lj_the_host.ud, w);
}

int lj_desktop_bind_action(enum lj_desktop_action a, lj_desktop_action_fn fn, void *ud)
{
    if ((int)a < 0 || a >= LJ_ACT_COUNT) return -1;
    lj_actions[a].fn = fn; lj_actions[a].ud = ud;
    return 0;
}

/* W2: what an action does when no owner bound it. Window leaves open their window through lj_windows.h (a window whose
 * track has not landed logs "has no behaviour file yet" there); the app binds the action when it needs more (a load, a
 * file, a restart) and its binding wins. Returns 0 when there is no default. */
static const struct { enum lj_desktop_action a; enum lj_win w; } lj_act_win[] = {
    { LJ_ACT_USER_LIBRARIES, LJ_WIN_USER_LIBRARIES }, { LJ_ACT_GLOBAL_PATCH, LJ_WIN_GLOBAL_PATCH }, { LJ_ACT_SYSTEM_PREFS, LJ_WIN_SYSTEM_PREFS },
    { LJ_ACT_EDIT_HOTKEYS, LJ_WIN_EDIT_HOTKEYS }, { LJ_ACT_ABOUT, LJ_WIN_ABOUT }, { LJ_ACT_FIXTURE_GROUPS, LJ_WIN_FIXTURE_GROUPS },
    { LJ_ACT_MIDI_IN, LJ_WIN_MIDI_IN }, { LJ_ACT_BGCUE_FORM, LJ_WIN_BGCUE_FORM }, { LJ_ACT_STATICS2_FORM, LJ_WIN_STATICS2_FORM },
    { LJ_ACT_SMOKE_CONTROL, LJ_WIN_SMOKE_CONTROL }, { LJ_ACT_NOTES, LJ_WIN_NOTES }, { LJ_ACT_STRIKING_LAMPS, LJ_WIN_STRIKING_LAMPS },
    { LJ_ACT_BACKUP, LJ_WIN_BACKUP }, { LJ_ACT_RESTORE_LIBRARY, LJ_WIN_RESTORE }, { LJ_ACT_SNAPSHOT, LJ_WIN_SNAPSHOT },
};
static int default_action(enum lj_desktop_action a, int arg)
{
    lj_ctx *ctx = &lj_the_ctx;
    int i, rc;
    for (i = 0; i < LJ_COUNT_OF(lj_act_win); i++) if (lj_act_win[i].a == a) { lj_win_open(lj_act_win[i].w); return 1; }
    switch (a) {
    case LJ_ACT_DELETE_FILES:   /* arg = enum lj_rec_kind, the five dialogs in the same order */
        if (arg >= LJ_REC_SEQ && arg <= LJ_REC_MACRO) lj_win_open((enum lj_win)(LJ_WIN_DELETE_SEQ + arg));
        return 1;
    case LJ_ACT_DEFAULT_OUT:    /* Use Current Scene (1) / Clear Default Output (0) [01 s6.3]; the app's DEFAULT_OUT_APPLY saves DefaultOut.dat */
        if (!ctx->show) return 1;
        lj_lock();
        if (arg) {
            const lj_sequence *q = ctx->show->current_seq;
            rc = q && ctx->current_scene < q->n ? lj_defaultout_use_scene(ctx->show->default_out, &q->scenes[ctx->current_scene]) : -1;
        } else { lj_defaultout_clear(ctx->show->default_out); rc = 0; }
        lj_unlock();
        if (rc == 0) lj_desktop_run_action(LJ_ACT_DEFAULT_OUT_APPLY, arg);
        else lj_log("desktop: Default DMX Output: the current scene holds a preset or a pan/tilt macro (or is empty); nothing recorded");   /* PROPOSAL: LJ2's error box text is not captured */
        return 1;
    case LJ_ACT_DEFAULT_OUT_APPLY: {   /* without the app: the engine re-reads the table; the file is the app's */
        lj_cmd c;
        memset(&c, 0, sizeof c); c.kind = LJ_CMD_DEFAULT_OUT; c.default_out = ctx->show ? ctx->show->default_out : NULL;
        if (c.default_out) lj_ctx_post(&c);
        return 1;
    }
    case LJ_ACT_HOTKEY_SETUP: {   /* without the app: load <userdir>Hotkeys/SetupN.Hot into the live table */
        char path[LJ_PATH_CAP], err[128];
        if (ctx->hotkeys && lj_hotkeys_setup_path(ctx->userdir, arg, path, sizeof path) == 0 && lj_hotkeys_load(path, ctx->hotkeys, err, sizeof err) != 0)
            lj_log("desktop: hotkey setup %d: %s", arg, err);
        return 1;
    }
    case LJ_ACT_HOTKEYS_ENABLED: return 1;   /* the desktop already flipped ctx->hotkeys; the app saves [HOTKEYS] Enabled */
    default: return 0;
    }
}

void lj_desktop_run_action(enum lj_desktop_action a, int arg)
{
    if ((int)a < 0 || a >= LJ_ACT_COUNT) return;
    if (lj_actions[a].fn) { lj_actions[a].fn(a, arg, lj_actions[a].ud); return; }
    if (default_action(a, arg)) return;
    if (!lj_actions[a].complained) { lj_actions[a].complained = 1; lj_log("desktop: action %d is not bound", (int)a); }
}

/* The four window-toggle hotkey functions the toolbar LED buttons and View > Controls run: bound here unless a window
 * track bound them first (a later lj_fn_bind replaces this one). */
static void fn_toggle_win(enum lj_fn f, int edge, uint32_t prog, void *ud)
{
    (void)f; (void)prog;
    if (edge) lj_win_toggle((enum lj_win)(intptr_t)ud);
}
static void bind_window_fns(void)
{
    static const struct { enum lj_fn f; enum lj_win w; } t[] = {
        { LJ_FN_TOGGLE_BGCUE_CONTROL, LJ_WIN_BGCUE_FORM }, { LJ_FN_STATICS_CONTROL, LJ_WIN_STATICS2_FORM },
        { LJ_FN_SMOKE_CONTROL, LJ_WIN_SMOKE_CONTROL }, { LJ_FN_TOGGLE_NOTES, LJ_WIN_NOTES } };
    int i;
    for (i = 0; i < LJ_COUNT_OF(t); i++) if (!lj_fn_bound(t[i].f)) lj_fn_bind(t[i].f, fn_toggle_win, (void *)(intptr_t)t[i].w);
}

/* ---- the window ---- */
lj_window *lj_desktop_create(void)
{
    lj_window *win;
    lj_widget *root;
    if (lj_the_desktop) return lj_the_desktop;
    win = lj_main_window_build(NULL);
    if (!win) return NULL;
    bind_window_fns();
    lj_the_desktop = win;
    root = lj_window_root(win);
    lj_toolbars_build(win, root);           /* alTop rows in creation order: Seq/Cue, Cue list, Fixture */
    lj_statusbar_build(root);               /* alBottom */
    lj_desktop_tabs_build(root);            /* alClient: tab strip + icon area */
    lj_widget_layout(root);
    lj_main_menu_apply(win);
    lj_desktop_set_library(lj_the_ctx.lib ? lj_the_ctx.lib->name : NULL);
    lj_desktop_toolbar_update();
    lj_desktop_icons_refresh();
    lj_window_invalidate_all(win);
    return win;
}

void lj_desktop_destroy(lj_window *win)
{
    if (!win) return;
    lj_unassigned_close();
    if (win == lj_the_desktop) lj_the_desktop = NULL;
    lj_main_window_free(win);
}

lj_window *lj_desktop_window(void) { return lj_the_desktop; }

void lj_desktop_set_library(const char *name)
{
    char cap[128];
    if (!lj_the_desktop) return;
    if (!name) lj_str_copy(cap, sizeof cap, lj_form_TLJMAINFORM.caption);   /* "Please Wait.." verbatim (13 exe strings; LORD_FIDELITY-20) */
    else snprintf(cap, sizeof cap, LJ_MAIN_CAPTION_FMT, name);
    lj_window_set_caption(lj_the_desktop, cap);
}

void lj_desktop_refresh(void)
{
    if (!lj_the_desktop) return;
    lj_statusbar_refresh();
    lj_desktop_toolbar_update();
    lj_desktop_icons_refresh();
}

void lj_desktop_key(const lj_key_ev *ev, int in_dialog)
{
    const lj_hotkey *k;
    int edge;
    if (!ev || !lj_the_ctx.hotkeys || !lj_the_ctx.hotkeys->enabled) return;
    if (ev->type == LJ_EV_KEY_DOWN) edge = 1; else if (ev->type == LJ_EV_KEY_UP) edge = 0; else return;
    k = lj_hotkeys_match(lj_the_ctx.hotkeys, (uint16_t)ev->key, ev->shift, ev->ctrl, ev->alt, 0, in_dialog);   /* ponytail: Scroll Lock state not read; the platform can pass it later */
    if (!k) return;
    if (k->fn >= LJ_FN_PROG_SEQ_FLASH) lj_fn_run_prog((enum lj_fn)k->fn, k->prog_num, edge);
    else lj_fn_run((enum lj_fn)k->fn, edge);
}
