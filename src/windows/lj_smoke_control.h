/* lj_smoke_control.h - Smoke Control (TSmokeForm "Smoke Control" 310x213, [04 s12], [01 s4], design 3.4 rank 7).
 * Timer / Reset / Smoke buttons and five faders built over the DFM's Shape1..3 (Level, Duration, Cycle) and
 * Shape5..6 (Hazer Level, Fan); every change posts LJ_CMD_SMOKE (enum lj_smoke_ctl). The engine runs the cycle
 * (LJ_SMOKE_TIMER b 1: every Cycle, smoke for Duration at Level, view smoke_cycle_elapsed_ms); the window only sends
 * the settings. Main.Ini [SMOKE] keeps LJ2's shipped keys (UseLevel TimerOnStart Level Duration Cycle HazerLevel
 * HazerFan Reset) plus two of ours, ViewHaze and Toggle (PROPOSAL: LJ2's keys for those menu items are unknown). */
#ifndef LJ_WINDOWS_SMOKE_CONTROL_H
#define LJ_WINDOWS_SMOKE_CONTROL_H

#include "engine/lj_engine.h"
#include "ui/lj_window.h"

enum lj_smoke_fader { LJ_SMOKE_F_LEVEL = 0, LJ_SMOKE_F_DURATION, LJ_SMOKE_F_CYCLE, LJ_SMOKE_F_HAZE, LJ_SMOKE_F_FAN, LJ_SMOKE_F_COUNT };

lj_window *lj_smoke_control_open(void);
void       lj_smoke_control_close(void);
lj_window *lj_smoke_control_get(void);
void       lj_smoke_control_refresh(void);                  /* Timer latch and the toolbar Smoke LED from ctx->view */
void       lj_smoke_control_bind_fns(void);                 /* Smoke Control, Smoke Button (flash), Toggle smoke timer */
lj_widget *lj_smoke_control_fader(enum lj_smoke_fader f);   /* NULL when closed */
void       lj_smoke_control_button(int down);               /* the Smoke button: press / release (the toggle preference decides c) */
void       lj_smoke_control_timer(int on);                  /* the Timer button */
int        lj_smoke_cycle_ms(int step);                     /* 0..31 -> 5 s .. 60 min (LJ_SMOKE_CYCLE_STEPS) */
int        lj_smoke_control_last_cmd(lj_cmd *out);          /* the last command posted (tests); 0 = none */

#endif /* LJ_WINDOWS_SMOKE_CONTROL_H */
