/* lj_striking_lamps.h - the Striking Lamps window (TAutoLampForm "Striking Lamps", [01 s6.2]):
 * Lamp On one fixture at a time, LJ_STRIKE_INTERVAL_MS apart, over the selected fixtures (all
 * patched fixtures when none is selected) whose profile has a Lamp On function; the list shows
 * Fixture# / FixtureType / Action, pg1 the progress, Stop (bClose) cancels. Started from the
 * startup Auto LampOn option (the window closes when the last lamp is struck) or from System > Auto
 * Strike Lamps (it stays open until Stop). Steps are driven from the 100 ms refresh (and Timer1);
 * each one calls the strike hook the app installs (lj_app_strike_lamps owns the output path); with
 * no hook the Lamp On values go into the current scene as Snap, as the Lamp control's button does. */
#ifndef LJ_WINDOWS_STRIKING_LAMPS_H
#define LJ_WINDOWS_STRIKING_LAMPS_H

#include <stdint.h>
#include "windows/lj_windows.h"

#define LJ_STRIKE_INTERVAL_MS 1000   /* "one fixture at a time ~1 s apart" [01 s6.2]; PROPOSAL rig: LJ2's exact spacing */

lj_window *lj_striking_lamps_open(void);                      /* LJ_WIN_STRIKING_LAMPS: lj_striking_lamps_start(1, ...) with the default interval */
void       lj_striking_lamps_close(void);
lj_window *lj_striking_lamps_window(void);
void       lj_striking_lamps_refresh(void);                   /* lj_striking_lamps_step(now) + the pending close */

int  lj_striking_lamps_start(int stay_open, uint32_t interval_ms);   /* builds the list, opens the window; returns the fixture count (0 = nothing to strike: no window for the startup run, an empty one for the menu run) */
int  lj_striking_lamps_step(uint32_t now_ms);                 /* strikes the next fixture when the interval has passed; returns how many are struck so far */
int  lj_striking_lamps_count(void);
int  lj_striking_lamps_fixture(int i);                        /* fixture index of row i, -1 */
void lj_striking_lamps_set_strike(void (*fn)(int fixture, void *ud), void *ud);   /* NULL = the current-scene writer */

#endif /* LJ_WINDOWS_STRIKING_LAMPS_H */
