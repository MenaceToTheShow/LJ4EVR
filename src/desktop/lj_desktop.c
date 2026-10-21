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
#include "ui/forms/lj_forms_gen.h"

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

void lj_desktop_run_action(enum lj_desktop_action a, int arg)
{
    if ((int)a < 0 || a >= LJ_ACT_COUNT) return;
    if (lj_actions[a].fn) { lj_actions[a].fn(a, arg, lj_actions[a].ud); return; }
    if (!lj_actions[a].complained) { lj_actions[a].complained = 1; lj_log("desktop: action %d is not bound", (int)a); }
}

/* ---- the window ---- */
lj_window *lj_desktop_create(void)
{
    lj_window *win;
    lj_widget *root;
    if (lj_the_desktop) return lj_the_desktop;
    win = lj_main_window_build(NULL);
    if (!win) return NULL;
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
