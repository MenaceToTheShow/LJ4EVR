# Hotkeys: the function table, the key map and the dispatcher

W1 actions track, 2026-10-21. Sources: design 7.9 (keyboard paragraph), 8.4, note 01
s7 (rules and the function groups), the CHM topics `hotkeys.htm` / `scribble130.htm`,
`TFUNCTIONSFORM` (the Edit Hotkeys DFM), `THOTKEYFUNCTIONDIALOG`,
`TSHOWHOTKEYWARNINGFORM`. Code: `src/io/lj_actions.[ch]`, `src/io/lj_hotkeys.[ch]`,
`src/io/lj_fn.[ch]`; check: `lj4evr_tests test_hotkeys` (`tests/io/test_hotkeys.c`).

## The function table (`io/lj_actions.h`)

`LJ_FN_LIST(X)` is one X-macro row per function of the CHM "Hotkey functions" topic,
grouped as the Edit Hotkeys list shows them (`lj_fn_group_names[]`: System,
Intensity, HTP groups, Fixture controls, Other, Fixture selection, Sequence, Cue, Cue
control, Cue builder, Generic macro, Cue list, Background cue, Statics, Fixture
groups, DMX in, Programs). Numbered families keep one `%d` caption and a per-row `n`
(`lj_fn_caption(LJ_FN_FADE_OUT_SEC_3)` = `Fade Out 3 Sec`, `LJ_FN_STATIC_7` =
`Static 07`). Flags: `LJ_FNF_ENABLE` (acts on press), `LJ_FNF_TOGGLE`, `LJ_FNF_FLASH`
(the `*` rows: active while held, release is an edge). The four `LJ_FN_PROG_*` rows
are the Edit Hotkeys program tabs `Sequence (Flash) | Cue | Cuelist | BG Cue`; the
bound record number travels in `lj_hotkey.prog_num`. 450 functions in 17 groups; the
`.Hot` file spells a function by its enum identifier (`lj_fn_desc.name`), so rows may
be inserted without breaking saved setups. Added by this track against the CHM:
`LJ_FN_TOGGLE_SEQ_CAPTURE` "Toggle Seq Capture Activate" (Cue builder).

## The rules (`lj_hotkeys_match`)

| Rule [01 s7] | Where |
|---|---|
| Only when an LJ4EVR window has OS focus | the platform: a key event reaches `lj_desktop_key` only from a focused window; nothing in io/ asks |
| Setup > Hotkeys > Hotkeys Enabled | `lj_hotkeys.enabled`; off silences everything, F-keys included |
| Shift and Ctrl combinations are distinct hotkeys | the combo key is `(key, shift, ctrl)`; `[Shift] B`, `[Control] B` and `B` are three entries |
| Alt combinations are not recognized | `alt` -> NULL; `VK_MENU`, `VK_SHIFT`, `VK_CONTROL` are refused as the main key by `lj_hotkeys_assign` / `validate` / `load` |
| F1-F12 and their Shift/Ctrl combinations always work | VK 0x70-0x7B bypass the dialog and Scroll Lock gates |
| Disable Hotkeys in Dialogs, default on | `disable_in_dialogs` (LJ2: `Main.Ini [HOTKEYS] DialogDisable=1`); `in_dialog` is the caller's "a dialog needing typing is active" |
| Per-entry Scroll Lock attribute | `lj_hotkey.scroll`: the entry fires only with `scroll_lock` on |
| Keys register as keys, letters as capitals | `key` is the Windows virtual-key code; `lj_hotkeys_combo_text` prints `[Shift] B`, `[Control] F1`, `Esc` (exe literals `[Shift] ` / `[Control] `; key names PROPOSAL, undefined keys as `[<vk>]`) |

The desktop dispatch (`lj_desktop_key`, desktop track) is: `e = lj_hotkeys_match(ctx->hotkeys,
ev->key, ev->shift, ev->ctrl, ev->alt, scroll_lock, in_dialog)`; on key down
`lj_fn_run_prog(e->fn, e->prog_num, 1)`, on key up `lj_fn_run_prog(e->fn, e->prog_num, 0)`.
`lj_fn_run_prog` drops the release for non-flash functions, so callers may send both edges
for every key.

## Setups and the `.Hot` file

Up to 8 setups (`LJ_HOTKEY_SETUPS_MAX`) at `<library root>Hotkeys/SetupN.Hot`
(`lj_hotkeys_setup_path`). Per the W1 brief they live inside the user library beside
`MIDIIn\` (the exe's `Hotkeys\` literal is at 0x22F7FC; LJ2's folder placement is not
documented, PROPOSAL). The header comment still says `<userdir>`; the desktop passes
`ctx->lib->root`. `lj_library_create` does not yet create `Hotkeys/` (model track; asked
for in the return notes).

The grammar is ours (design 5.2; LJ2's binary `.Hot` is undecoded), kind `hot`, format 1:

```
LJ4 hot 1
name="Default"
enabled=1 dialogs=1
key vk=27 shift=0 ctrl=0 scrl=0 fn=LJ_FN_CLOSE_WINDOW
key vk=49 shift=0 ctrl=0 scrl=0 fn=LJ_FN_PROG_CUE num=12
```

`vk` is the Windows virtual-key code, `fn` the enum identifier, `num` only on the four
program rows. Unknown keys and trailing fields are skipped. A file is refused (-1 plus
`err`) for an unknown function, a duplicate combo, a `num` on a function row, a missing
`num` on a program row, Alt/Shift/Control as the key, or more than 512 rows; a setup that
fails `lj_hotkeys_validate` never reaches disk.

## Setup 1, the shipped default (`lj_hotkeys_default`)

PROPOSAL. The 2.209 installer ships no `.Hot` file and the compiled-in set behind Edit
Hotkeys > Options > `&Default Hotkeys` is undecoded (note 10, line 171). The shipped
Setup 1 therefore carries only the function rows legible in the CHM's Edit Hotkeys
screenshot `hotkeys1.gif` (an older function list; its `*Blind` / `Blind Toggle` are
today's `Sequence Blind` / `Sequence Blind Toggle`):

| Key | Function | Evidence |
|---|---|---|
| `Esc` | Close Window | hotkeys1.gif row 1 (caption half-hidden by the callout) |
| `[Shift] Esc` | Close All Windows | hotkeys1.gif |
| `[Shift] B` | Sequence Blind (flash) | hotkeys1.gif, the highlighted row |
| `B` | Sequence Blind Toggle | hotkeys1.gif |

The cue rows in the same picture (`1`-`5`, `[Shift] 1`-`5`, `[Control] 1`-`3` -> cues
1-13) are the help author's own show and are not shipped. The setup is named `Default`
after the first entry of `hotkeys2.gif`. When LJ2's table is decoded, replace the rows in
`lj_hotkeys_default` and this table.

## The dispatcher (`io/lj_fn.h`)

One handler slot per function, UI thread only. Windows and the desktop call
`lj_fn_bind(fn, handler, ud)` at startup; `lj_fn_run(fn, edge)` / `lj_fn_run_prog(fn,
num, edge)` call it with the edge (1 press, 0 release; release reaches flash functions
only). An unbound function logs `fn: no binding for LJ_FN_...` once and does nothing;
Debug `--selftest` lists the unbound rows through `lj_fn_bound`. 2532, Fingers, the cue
list `Hotkey Command` and (W2) MIDI reach the same entry point.

## Check

`lj4evr_tests test_hotkeys`: every table row named, captioned, grouped and found by
name; the default setup validates and loads; F1 / Shift+Ctrl+F12 fire in a dialog with
Scroll Lock off; Alt never matches (and cannot be stored); Shift and Ctrl distinct; the
Scroll Lock scope and Disable-in-dialogs gates; program rows require a number, function
rows refuse one; `.Hot` save/load round trip with a quoted name; six refused files and one
with unknown keys accepted; clear by key and by function; the 512-row ceiling; flash
press+release reaches the handler, enable press only, unbound logs once.
