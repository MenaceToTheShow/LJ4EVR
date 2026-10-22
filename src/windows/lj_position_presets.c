/* lj_position_presets.c - see lj_position_presets.h. */
#include "windows/lj_position_presets.h"

#include "controls/lj_ctl.h"
#include "controls/lj_ctl_movement.h"
#include "io/lj_fn.h"

static lj_window *movement(int open) { lj_window *w = lj_ctl_window_get(LJ_CTL_MOVEMENT); return w || !open ? w : lj_ctl_open(LJ_CTL_MOVEMENT); }

lj_window *lj_position_presets_window(void) { lj_window *m = movement(0); return m ? lj_ctl_movement_presets_window(m) : NULL; }
lj_window *lj_position_presets_open(void) { lj_window *m = movement(1); return m ? lj_ctl_movement_presets_open(m) : NULL; }
void lj_position_presets_refresh(void) { }   /* the Movement control repaints its dialog (lj_ctl_refresh) */

void lj_position_presets_close(void)
{
    lj_window *w = lj_position_presets_window();
    lj_ui_event ev = { 0 };
    if (!w) return;
    ev.type = LJ_EV_CLOSE;
    lj_window_event(w, &ev);   /* FormClose: the Movement control forgets the dialog */
    lj_win_hide(w);
    lj_window_destroy(w);
}

int lj_position_presets_store(int n, int filter) { lj_window *m = movement(1); return m ? lj_ctl_movement_preset_record(m, n, filter) : -1; }
int lj_position_presets_recall(int n) { lj_window *m = movement(1); return m ? lj_ctl_movement_preset_apply(m, n) : -1; }

static void fn_preset(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn == LJ_FN_TOGGLE_POSITION_PRESET) { if (lj_position_presets_window()) lj_position_presets_close(); else lj_position_presets_open(); }
    else lj_position_presets_recall((int)(fn - LJ_FN_ASSIGN_PRESET_1) + 1);   /* "Hotkeys assign presets 1-10 to selected fixtures" [RN 2.1] */
}
void lj_position_presets_bind_fns(void)
{
    int i;
    for (i = 0; i < 10; i++) lj_fn_bind((enum lj_fn)(LJ_FN_ASSIGN_PRESET_1 + i), fn_preset, NULL);
    lj_fn_bind(LJ_FN_TOGGLE_POSITION_PRESET, fn_preset, NULL);
}
