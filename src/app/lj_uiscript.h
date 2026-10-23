/* lj_uiscript.h - the UI script driver (docs/ui-script.md): `lj4evr --script
 * <file>` executes one line per event-loop pass on the UI thread: open/close a
 * window, click/rclick/dblclick/drag a widget by <Form>.<widget>, type text,
 * press a key, wait, screenshot, assert captions/text/enabled/frame values/files,
 * quit. A failure prints "SCRIPT FAIL line n: <reason>" to stderr and the app
 * exits 1; "quit" exits 0. Test affordance, ponytail-sized. SDL-free: it only
 * synthesises lj_ui_events into lj_window_event, so tests/ui/test_uiscript.c
 * runs the same file headless. Window names resolve through lj_win_find() (DFM
 * class or docs/ui-script.md id), `main` and the control kinds; widgets through
 * lj_window_find(). */
#ifndef LJ_APP_UISCRIPT_H
#define LJ_APP_UISCRIPT_H

#include <stddef.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

/* What the host (the app, or a test) supplies: a window resolver that may wrap
 * lj_uiscript_resolve_default, and the app's last lj_engine_view copy for
 * assert_view / assert_pps (only lj_app_service_pending may call lj_engine_view,
 * design 2.3 rule 3). NULL members = default resolver / "no view". */
typedef struct { void *ud; lj_window *(*resolve)(void *ud, const char *form); const lj_engine_status *(*view)(void *ud); } lj_uiscript_host;
void       lj_uiscript_set_host(const lj_uiscript_host *host);   /* NULL = defaults; the struct is copied */
lj_window *lj_uiscript_resolve_default(const char *form);        /* "main" -> lj_desktop_window, a control kind -> lj_ctl_window_get, else lj_win_find */

int  lj_uiscript_load(const char *path);     /* reads and parses the whole file; 0 / -1 + "SCRIPT FAIL line n: <reason>" (n = 0: cannot read) */
int  lj_uiscript_step(void);                 /* called by the app's idle pass: executes the next line (a `wait` spans passes); 1 = more to do, 0 = finished (quit or EOF), -1 = failed (already printed) */
int  lj_uiscript_active(void);               /* a script is loaded and not finished */
int  lj_uiscript_line(void);                 /* the current 1-based line, for log lines */
void lj_uiscript_fail(const char *reason);   /* "SCRIPT FAIL line n: reason" and mark finished with -1; windows may call it from an assert hook */


/* ---- W2 verbs (docs/ui-script.md "W2 verbs"; parsed here, bodies by the app track) ----
 *   midi <hex> <hex> <hex>          lj_midi_inject of the 3 bytes, then one lj_app_midi_dispatch pass ("midi 90 3C 7F")
 *   hotkey <setup> <key>            loads hotkey setup 1..8 (LJ_ACT_HOTKEY_SETUP) and presses <key> (the `key` grammar) through lj_desktop_key
 *   assert_led <name> 0|1           lj_desktop_led_get: bgcue | statics | smoke
 *   assert_view <field>=<value> ... the W1 verb; W2 adds the field names bg_any statics_any flash_latched transparent_mode smoke_active chase_ms cue_number bg_number pending_link_cue snapshot_tick (and accepts `assert_view <field> <value>` as two bare args)
 *   assert_file_line <path> "text"  the file (library-relative like assert_file) contains the line exactly once
 *   sleep_until_tick <n>            lets the loop run until the app's view copy reports tick_no >= n */

/* ---- W3 verbs (docs/ui-script.md "W3 verbs"; parsed here, bodies by the desktop-app track; checked at load like W2) ----
 *   cl_load <path>                  loads a .Cl2 (as given, else library-relative) into show->current_cl through lj_app_load_cuelist's path; opens nothing
 *   cl_goto <n>                     selects line n (1-based) of the Cuelist window's list (a click on the row); while running and waiting = Jump to this
 *   cl_execute                      Go ! (LJ_CL_OP_GO): advances a waiting manual line, or runs from the top when idle
 *   cl_assert_line <n> <state>      lj_cl_line_state of line n: plain | done | next | error (gray / blue / red of RN v2.6 b7 and design 8.5)
 *   assert_pos <fixture> <pan> <tilt>   fixture 1..100: the readout (17 s1.3, -32768..32767) of its pan / tilt in the last frame copy (out[], after S7, 8-bit = MSB)
 *   gs2_import <path>               imports a .GS2 / .Udf / .Udo through lj_udf_parse / lj_gs2_read into UserFixtureDef and runs LJ_ACT_GS2_RELOAD; fails with the importer's message
 *   assert_prompt "text"            the open confirm / message box (lj_win_message_window) shows text verbatim (12 Q9 prompts)
 *   mm_set <fixture> <shape> <amp> <speed>   the current scene's movement macro of fixture 1..100: type = shape (0..6 or a Bezier slot), amp_pan = amp_tilt = amp,
 *                                   cycle_pan = cycle_tilt = speed, every fade state Snap; under lj_lock, then LJ_CMD_CURRENT_SEQ (what the Movement Macro window does) */

/* ---- W4 verbs (docs/ui-script.md "W4 verbs") ----
 *   dump_captions <path>            appends the runtime Caption / Text / Hint of every open window's widgets and menu items to path
 *                                   ("Class<TAB>object<TAB>property<TAB>value"), for tools/caption_diff.py (W4 acceptance 1) */

#endif /* LJ_APP_UISCRIPT_H */
