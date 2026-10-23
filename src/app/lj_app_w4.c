/* lj_app_w4.c - W4 contract (docs/w4-contracts.md section 4): the desktop actions that open W4 windows, and the idle hook.
 * Complete as written; the families behind it are the tracks' stubs until they land. */
#include "app/lj_app_w4.h"

#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "windows/lj_config_dialogs.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_lightcorder.h"
#include "windows/lj_windows.h"

static void act(enum lj_desktop_action a, int arg, void *ud)
{
    (void)ud;
    switch (a) {
    case LJ_ACT_FIXTURE_INFO:      lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_INFO); break;
    case LJ_ACT_FIXTURE_NOTES:     lj_config_dlg_target(arg); lj_win_open(LJ_WIN_FIXTURE_NOTES); break;
    case LJ_ACT_AUTO_ARRANGE:      lj_win_open(LJ_WIN_AUTO_ARRANGE); break;
    case LJ_ACT_DESKTOP_GRID:      lj_win_open(LJ_WIN_DESKTOP_GRID); break;
    case LJ_ACT_DESKTOP_IMAGE:     lj_win_open(LJ_WIN_DESKTOP_IMAGE); break;
    case LJ_ACT_DESKTOP_TABS:      lj_win_open(LJ_WIN_DESKTOP_TABS); break;
    case LJ_ACT_ICON_STYLE:        lj_win_open(LJ_WIN_ICON_STYLE); break;
    case LJ_ACT_NEXTPREV_SETTINGS: lj_win_open(LJ_WIN_NEXTPREV_SELECTION); break;
    case LJ_ACT_RESTORE_DATA:      lj_win_open(LJ_WIN_RESTORE_DATA); break;
    case LJ_ACT_HELP:              lj_help_viewer_topic(NULL); break;
    case LJ_ACT_SELECT_HELPFILE:   if (arg != 1) lj_log("help: only the English topics ship (design 13 item 12)"); lj_help_viewer_topic(NULL); break;
    case LJ_ACT_WINDOWS_MEMORY:    lj_win_open(LJ_WIN_MEMORY_VIEW); break;
    case LJ_ACT_DEBUG_WINDOW:      lj_win_open(LJ_WIN_DEBUG); break;
    case LJ_ACT_FINGERS_SETUP:     lj_win_open(LJ_WIN_FINGERS_SETUP); break;
    case LJ_ACT_DMX_IN:            lj_win_open(LJ_WIN_DMX_IN); break;
    default: break;
    }
}

void lj_app_w4_start(void)
{
    static const enum lj_desktop_action acts[] = { LJ_ACT_FIXTURE_INFO, LJ_ACT_FIXTURE_NOTES, LJ_ACT_AUTO_ARRANGE, LJ_ACT_DESKTOP_GRID,
        LJ_ACT_DESKTOP_IMAGE, LJ_ACT_DESKTOP_TABS, LJ_ACT_ICON_STYLE, LJ_ACT_NEXTPREV_SETTINGS, LJ_ACT_RESTORE_DATA, LJ_ACT_HELP,
        LJ_ACT_SELECT_HELPFILE, LJ_ACT_WINDOWS_MEMORY, LJ_ACT_DEBUG_WINDOW, LJ_ACT_FINGERS_SETUP, LJ_ACT_DMX_IN };
    size_t i;
    for (i = 0; i < sizeof acts / sizeof acts[0]; i++) lj_desktop_bind_action(acts[i], act, NULL);
}

void lj_app_w4_idle(uint64_t now_us) { lj_lightcorder_step(now_us); }
