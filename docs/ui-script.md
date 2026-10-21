# UI script driver (`lj4evr --script <file>`)

A test affordance, ponytail-sized: the app executes one line per UI-thread event-loop
pass (between `SDL_WaitEventTimeout` wakes), so every line runs on the UI thread with
real windows on screen. Header: `src/app/lj_uiscript.h`; implementation: the app track
(`src/app/lj_uiscript.c`). Not a user feature; it drives the acceptance screenshots of
design 10 W1 and the `tests/ui` scripts.

Rules: CP1252 text, one statement per line, `#` comments, blank lines ignored. Strings
are double-quoted; a quote inside is doubled. `<Form>` is a DFM class name
(`TSequenceForm`) or a window id of `docs/ui-script.md` / `lj_win_id_name()`
(`seq_form`, `cue_form`, `save_seq`, `save_cue`, `list_seq`, `list_cue`,
`fixture_config`, `master_intensity`, `hardware_setup`, `dmx_monitor`,
`startup_options`, `warnings`, `close_prompt`, `main` = the main window, or a control
kind `intensity movement colors gobos beam effects levels lamp extended`).
`<widget>` is the DFM object name (`lj_window_find`: `ESeqNo`, `sb7`) or, for
code-built windows, the name the builder gave the widget (toolbar buttons
`tb_seqcue_1`.., status panels `sb_master`, control rows `row<k>_fade`, `row<k>_bar`,
icon tiles `icon<n>`). Coordinates are optional and relative to the widget's top-left;
the default is its centre.

| Line | Effect |
|---|---|
| `open <window>` | `lj_win_open` / `lj_ctl_open` / main; fails when it cannot open |
| `close <window>` | `lj_win_close` / `lj_ctl_close` |
| `click <Form>.<widget> [x y]` | left mouse down + up on the widget (synthesised `lj_ui_event`s through `lj_window_event`) |
| `rclick <Form>.<widget> [x y]` | right click (the O/S/F state popup, icon popups) |
| `dblclick <Form>.<widget> [x y]` | two clicks, `clicks = 2` |
| `drag <Form>.<widget> <Form>.<widget> [dx dy]` | mouse down on the first, moves, up on the second (`lj_dnd`); `dx dy` offsets the drop point |
| `type "text"` | `LJ_EV_CHAR` per CP1252 byte to the focused widget of the active window |
| `key <name>` | key down + up: `Enter Esc Tab Space Up Down Left Right Home End PgUp PgDn Del BS F1..F12 A..Z 0..9`, with `Shift+` / `Ctrl+` / `Alt+` prefixes (`Ctrl+S`, `Shift+F1`); Windows VK codes as `lj_key_ev.key` |
| `wait <ms>` | lets the loop run (timers, engine, repaints) for at least `ms` |
| `shot <png path>` | every open window captured from its `lj_window_surface` (what `--selftest` proves is on screen, pixel for pixel), one file per window: `<path without extension>-<Class>.png` (`-TSequenceForm.png`, `-TLJMainForm.png` ...); `.bmp` when the path ends in `.bmp`. The OS frame under `[CHROME] Native=1` is not in it: `tools/shot.ps1` (`PrintWindow`) is the capture for that |
| `assert_caption <Form> "text"` | the window caption equals the text (`lj_window_caption()->text`) |
| `assert_text <Form>.<widget> "text"` | the widget's text/caption equals the text (edits, labels, buttons, a TStatusBar's first panel, TStringGrid cells as `Grid.r.c` = Delphi's 0-based `Cells[c, r]`, fixed rows and columns included) |
| `assert_enabled <Form>.<widget> 0\|1` | `widget->enabled` |
| `assert_visible <Form>.<widget> 0\|1` | `widget->visible` and the window open |
| `assert_frame l:a=v [tolerance]` | `lj_engine_frame()->out[link-1][addr-1]` (copied under `lj_lock`) equals `v` within `tolerance` (default 0) |
| `assert_view gm=<v> [blackout=.. freeze=.. offline=..]` | `lj_engine_status` fields of the app's last copy (gm 0..255) |
| `assert_file <path> exists` / `assert_file <path> missing` | the path exists / does not |
| `assert_pps <link> <min> <max>` | `lj_engine_status.sacn_pps[link-1]` within the range (the acceptance 6/7 rates, after a `wait 2000`) |
| `log "text"` | one line through `lj_log` |
| `quit` | ends the loop; exit code 0 (positions saved as a normal close, no prompt) |

Failure: the first failing line prints `SCRIPT FAIL line n: <reason>` to stderr and
the process exits 1 after the current pass (windows closed normally, nothing saved to
forms.ini). Every line is checked at load (verb, argument count, numbers, key names,
`l:a=v`), so a typo fails before the first line runs. EOF without `quit` behaves like
`quit`. A script never prompts: the close prompt and the red warning boxes are answered
through `click`. `type` and `key` go to the window last opened or clicked. `drag`
across two windows needs the windows on screen (the drop point is mapped through the
platform's screen positions); headless, drags stay inside one window.

Headless: `src/app/lj_uiscript.c` is SDL-free and linked into `lj4evr_tests`; a test
installs an `lj_uiscript_host` (window resolver + engine-view copy) and calls
`lj_uiscript_load` / `lj_uiscript_step` itself (`tests/ui/test_uiscript.c`). In the app
the resolver is `lj_uiscript_resolve_default` (`main`, control kinds, `lj_win_find`) and
the view is the copy `lj_app_service_pending` keeps.

Example (acceptance 4 and 5, abbreviated):

```
open fixture_config
click TFixtureConfigForm.tvProfiles          # tree: Martin / MAC 500 / Mode 2/4 selected by later clicks
...
open seq_form
click TSequenceForm.sbAddScene
assert_text main.sb_seqcounter "2:2"
click TSequenceForm.sbSave
type "first"
key Enter
assert_caption TSequenceForm "SEQUENCE [0001 first]"
open cue_form
click TSequenceForm.sbAddToCue
assert_text TCurrentCue.Grid.1.2 "0001"
click TCurrentCue.sbGo
wait 1500
assert_frame 1:2=255
shot docs/screenshots/w1/cue-running.png
quit
```
