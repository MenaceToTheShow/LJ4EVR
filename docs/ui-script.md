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
code-built windows, the name the builder gave the widget: toolbar buttons
`tb_seqcue_<n>` / `tb_cuelist_<n>` / `tb_fixture_<n>` and rows `tb_row_seqcue` ..,
the name fields `tb_seqname` / `tb_cuename`, the scene counter `sb_seqcounter`, the
icon area `desktop` and its tiles `icon<n>` (their text reads `(n) user name`), the
desktop status bar `sb_status` whose panels are addressed as `main.sb_clock`,
`sb_running`, `sb_memory`, `sb_master`, `sb_seqmod`, `sb_cuemod`, `sb_clmod`,
`sb_trig` (`assert_text` only), control rows `row<ch>_bar` (drag bar or fader),
`row<ch>_fade` (O/S/F), `row<ch>_shutter`, `row<ch>_btn<i>`, `row<ch>_radio<i>`,
`row<ch>_pal<i>`, `row<ch>_scroll` with `<ch>` the 1-based channel offset of the row
(`lj_ctl_elem_find`), the Master Intensity faders `fader_master`, `fader_1`..`fader_8`,
the cue grid `lj_cue_grid` (cells as `lj_cue_grid.<slot>.<col>`, slot 1..12, col 0 No ..
8 Trig), the Movement widgets `MvGrid`, `MvHome`, `MvPanFade`, `MvTiltFade` .. and the
Unassigned Fixtures list `TUnassignedForm.LV_Unassigned` (rows as `LV_Unassigned.<row>.<col>`,
0-based). A `<Form>.<name>` that is no widget but a menu item of the form (`TDMXOutForm.M_EnableOverwrite`)
is fired by `click`. Coordinates are optional and relative to the widget's top-left;
the default is its centre.

| Line | Effect |
|---|---|
| `open <window>` | `lj_win_open` / `lj_ctl_open` / main; fails when it cannot open |
| `close <window>` | `lj_win_close` / `lj_ctl_close` |
| `click <Form>.<widget> [x y]` | left mouse down + up on the widget (synthesised `lj_ui_event`s through `lj_window_event`) |
| `rclick <Form>.<widget> [x y]` | right click (the O/S/F state popup, icon popups) |
| `dblclick <Form>.<widget> [x y]` | two clicks, `clicks = 2` |
| `drag <Form>.<widget> <Form>.<widget> [[sx sy] dx dy]` | mouse down on the first (its centre, or `sx sy` when six words are given), moves, up on the second (`lj_dnd`); `dx dy` offsets the drop point |
| `type "text"` | `LJ_EV_CHAR` per CP1252 byte to the focused widget of the active window |
| `key <name> [count]` | key down + up, `count` times (`key Down 60` walks a tree): `Enter Esc Tab Space Up Down Left Right Home End PgUp PgDn Del BS F1..F12 A..Z 0..9`, with `Shift+` / `Ctrl+` / `Alt+` prefixes (`Ctrl+S`, `Shift+F1`); Windows VK codes as `lj_key_ev.key` |
| `wait <ms>` | lets the loop run (timers, engine, repaints) for at least `ms` |
| `shot <png path>` | every open window captured from its `lj_window_surface` (what `--selftest` proves is on screen, pixel for pixel), one file per window: `<path without extension>-<Class>.png` (`-TSequenceForm.png`, `-TLJMainForm.png` ...); `.bmp` when the path ends in `.bmp`. A window the app has already closed at the platform (AutoClose, the Cue / Sequence windows while Fixture Configuration is up) has no native handle and is skipped; a 0x0 surface fails the verb (ME-68). The OS frame under `[CHROME] Native=1` is not in it: `tools/shot.ps1` (`PrintWindow`) is the capture for that |
| `assert_caption <Form> "text"` | the window caption equals the text (`lj_window_caption()->text`) |
| `assert_text <Form>.<widget> "text"` | the widget's text/caption equals the text (edits, labels, buttons, a TStatusBar's first panel, TStringGrid cells as `Grid.r.c` = Delphi's 0-based `Cells[c, r]`, fixed rows and columns included); a `<Form>.<name>` that is no widget but a menu item (main or popup) compares the item's caption, the TAction's when it has one (W2: `TStatics2Form.ReplacePage2`) |
| `assert_enabled <Form>.<widget> 0\|1` | `widget->enabled` |
| `assert_visible <Form>.<widget> 0\|1` | `widget->visible` and the window open |
| `assert_frame l:a=v [tolerance]` | `lj_engine_frame()->out[link-1][addr-1]` (copied under `lj_lock`) equals `v` within `tolerance` (default 0) |
| `assert_view gm=<v> [blackout=.. freeze=.. offline=..]` | `lj_engine_status` fields of the app's last copy (gm 0..255) |
| `assert_file <path> exists` / `assert_file <path> missing` | the path exists / does not; a relative path is also tried under the user library (`Config.dat`) |
| `assert_client <Form> w h` | the window's client size (`lj_window_client_rect`) |
| `assert_pos <Form> x y` | the window's screen position (the platform's `screen_pos`; what `forms.ini` saves) |
| `assert_pps <link> <min> <max>` | `lj_engine_status.sacn_pps[link-1]` within the range (the acceptance 6/7 rates, after a `wait 2000`) |
| `log "text"` | one line through `lj_log` |
| `quit` | ends the loop; exit code 0 (positions saved as a normal close, no prompt) |

Failure: the first failing line prints `SCRIPT FAIL line n: <reason>` to stderr and
the process exits 1 after the current pass (windows closed normally, nothing saved to
forms.ini). Every line is checked at load (verb, argument count, numbers, key names,
`l:a=v`), so a typo fails before the first line runs. EOF without `quit` behaves like
`quit`. A script never prompts: the close prompt and the red warning boxes are answered
through `click`. `type` and `key` go to the window last opened or clicked, re-validated each time (a dialog
that closed itself, such as Save Sequence with AutoClose, is no longer a target). `drag`
across two windows needs the windows on screen (the drop point is mapped through the
platform's screen positions); headless, drags stay inside one window.

Headless: `src/app/lj_uiscript.c` is SDL-free and linked into `lj4evr_tests`; a test
installs an `lj_uiscript_host` (window resolver + engine-view copy) and calls
`lj_uiscript_load` / `lj_uiscript_step` itself (`tests/ui/test_uiscript.c`). In the app
the resolver is `lj_uiscript_resolve_default` (`main`, control kinds, `lj_win_find`) and
the view is the copy `lj_app_service_pending` keeps.

The walkthrough itself: `tests/ui/scripts/w1_acceptance.lj4s` (items 1-5, 7, 8, 10 of design 10 W1)
and `tests/ui/scripts/w1_positions.lj4s` (the restart of item 10); their evidence is in
`docs/w1-integration.md` section 9.

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

## W2 verbs (`lj_uiscript.c`, W2 app track 2026-10-22)

Window names gain the W2 ids of `lj_win_id_name()`: `loop_options cue_time cue_page_name
cue_pages_reorder bgcue_form select_bgcue save_bgcue statics2_form statics2_prefs static_props
smoke_control position_presets save_preset user_palettes fixture_groups intensity_groups
htp_groups global_patch default_output snapshot edit_hotkeys hotkey_function midi_in
system_prefs delete_seq delete_cue delete_cl delete_bg delete_macro user_libraries user_lib_name
backup restore notes about striking_lamps` (DFM classes in `docs/w2-contracts.md`), and `confirm` (or
`TConfirmForm`) = the open "Please Confirm.." / message box (`lj_win_message_window`), and
`TExportConfigForm` = Fixture Configuration's export box (fix pass r1, ME-92; `shot` captures it too).

| Line | Effect |
|---|---|
| `midi <hex> <hex> <hex>` | `lj_midi_inject` of the 3 bytes (`midi 90 3C 7F` = note 60 on, channel 1, velocity 127); the next idle pass dispatches it on the UI thread (`lj_app_midi_dispatch` -> `lj_midi_dispatch` -> `lj_fn_run_prog`), the same path as a device. Checked at load: three hex bytes 00..FF, the first a status byte 80..FF. Follow it with `wait 50` before asserting the effect |
| `hotkey <setup> <key>` | runs `LJ_ACT_HOTKEY_SETUP <setup>` (loads `<library>Hotkeys/Setup<setup>.Hot`, 1..8) and presses `<key>` (the `key` grammar: down + up) through `lj_desktop_key` with no dialog gate, as with the Sequence window focused |
| `assert_led <name> 0\|1` | `lj_desktop_led_get`: `bgcue`, `statics`, `smoke` |
| `assert_view <field>=<value> ...` | the W1 verb; fields `gm blackout freeze offline` and the W2 names `bg_any statics_any flash_latched flash_seq transparent_mode smoke_active haze_level chase_ms cue_number bg_number pending_link_cue snapshot_tick`; also accepted as two bare words `assert_view <field> <value>`. Values are unsigned decimal. `pending_link_cue` is the copy of the pass that handed the request out (the engine clears it at that read), so it is visible to exactly the next script line |
| `assert_file_line <path> "text"` | the file (as given, else library-relative like `assert_file`) contains the line exactly once (CR/LF stripped before the compare) |
| `sleep_until_tick <n>` | lets the loop run until the app's view copy reports `tick_no >= n`; fails after 10 s (a stopped engine) |
| `move <Form> <x> <y>` | puts the window at screen x,y through `lj_win_host.move` (W2 integrator 2): windows open centred on top of each other, and a `drag` needs its drop target visible. Fails headless |

Every W2 line is checked at load like the W1 verbs: unknown view fields, LED names, setups outside
1..8, unknown key names, non-hex MIDI bytes and non-numeric ticks fail with `SCRIPT FAIL line n` before
the first line runs (`tests/app/test_uiscript_w2.c`). The W2 walkthrough is
`tests/ui/scripts/w2_acceptance.lj4s` (design 10 W2 acceptance, shots in `docs/screenshots/w2/`).

## W3 verbs (contracts seat 2026-10-22; bodies by the desktop-app track in `src/app/lj_uiscript.c` + `src/app/lj_app_w3.c`)

Window names gain the W3 ids of `lj_win_id_name()`: `cuelist_form save_cuelist select_cuelist cl_set_cue_trig
log_timecode genmacro_editor save_genmacro select_genmacro movemacro_editor delete_movemacro fan_out auto_delay
cue_macro_control gdc_form followspot_form followspot_list seq_fades profile_editor customize_fixture channel_config
gs2_import gs2_export` (DFM classes in `docs/w3-contracts.md` section 4).

| Line | Effect |
|---|---|
| `cl_load <path>` | loads a `.Cl2` (as given, else library-relative) as the current cue list through `lj_app_load_cuelist_path` (the runner lets go of the old list first); fails when the file is missing or does not load |
| `cl_goto <n>` | while the list waits for Go: Jump to this (`LJ_CL_OP_JUMP`); otherwise selects row n of the Cuelist window's `lv1` (as a click); fails when the window is not open or the list has no line n |
| `cl_execute` | Go ! (`LJ_CL_OP_GO` through `LJ_ACT_CUELIST_OP`): advances a waiting manual line, or runs from the top when idle |
| `cl_assert_line <n> plain\|done\|next\|error` | `lj_cl_line_state` of line n of the current list (gray = done, blue = next, red = load error) |
| `assert_pos <fixture> <pan> <tilt>` | a number first = the W3 form: fixture 1..100's pan / tilt readout (17 s1.3, -32768..32767) from the engine frame's `out[]` (16-bit = the pair through `lj_pos_patch16`, 8-bit = the MSB); fails for a fixture without a Movement window. A window name first = the W1 screen-position form |
| `gs2_import <path>` | imports a `.GS2` / `.Udf` / `.Udo` (`lj_gs2_read` / `lj_udf_parse`) into `UserFixtureDef\<base>.GS2` and runs Reload profile data; fails with the importer's message (`not a GS2 record: n bytes`, `footer CRC mismatch`, ...) or when the profile is not listed after the reload |
| `assert_prompt "text"` | the open confirm / message box (`lj_win_message_window`) shows the text verbatim as `Label1`, `Label2`, both joined by one space, or its caption (the 12 Q9 prompts, acceptance 7); fails when no box is open |
| `mm_set <fixture> <shape> <amp> <speed>` | the current scene's movement macro of fixture 1..100: type = shape (0..6 or a Bezier slot), both amplitudes = amp, both cycles = speed (0 = frozen), every fade state Snap; under `lj_lock`, then `LJ_CMD_CURRENT_SEQ` (what the Movement Macro window does) |

`assert_view` gains the W3 view fields `gm_engine_on gm_slot_on gm_cue_number gm_editor_number mm_amp_pct mm_speed_pct
mm_fade mm_frozen pending_genmacro`.

Every W3 line is checked at load like the W2 verbs (unknown states, fixtures outside 1..100, readouts outside
-32768..32767, shapes / amplitudes / speeds outside 0..255, non-numeric values fail with `SCRIPT FAIL line n` before
the first line runs; `tests/app/test_uiscript_w3.c`). The W3 walkthrough is `tests/ui/scripts/w3_acceptance.lj4s`
(design 10 W3 acceptance, shots in `docs/screenshots/w3/`), with two committed inputs of ours: `w3_mp_play.Cl2`
(cue 0001, an MP Play line, cue 0002 after 0.50 s) and `w3_synthetic.Udf` (written by `lj_udf_export` from the
built-in Generic DMX Dimmer; no Martin bytes). Lines marked RECONCILE name widgets of W3 windows that had not landed
when the walkthrough was written. Generic macro frame values at given ticks are asserted at synthetic time by replay
`genmacro_sine_amp186`; the live walkthrough asserts the editor's Min/Max readout, the view fields and, for movement
macros, positions at two ticks with the macro frozen (speed 0).
