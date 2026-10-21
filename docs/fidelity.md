# Fidelity deviations

Every place where LJ4EVR knowingly differs from LightJockey 2.209, with the decision that
allows it. "Exactly like" is the rule (design 1, F1-F60); a row here is the exception.
Append rows; never delete one.

| Where | LJ2 2.209 | LJ4EVR | Decision | Evidence |
|---|---|---|---|---|
| Main window, View > Toolbars | Fifth item `Offline Toolbar` (checked), the Offline Visualizer `O/V` toolbar row | Item removed from the live menu (`lj_menu_set_visible("Offlinebar1", 0)` in `src/desktop/lj_main_window.c`, `lj_main_menu_removed_item()`); no `O/V` row is built. The generated `TLJMainForm` table keeps the DFM item verbatim. | D12 (no Offline / 3D Visualizer), ME-29 | `lj4evr_tests test_desktop` asserts the item invisible; `TLJMAINFORM.dfm.txt` `Offlinebar1` |
| Main window, menu bar | `&3D Visualizer`, `Ca&pture`, `&Audio`, `SandNet` shown when their back-end is configured | Never shown (`&Audio` returns in W5); `LJ-Manager`, `&Download`, `&Fingers` shown with every leaf greyed (hint `Not available in LJ4EVR`) | D12, ME-29, LORD_FIDELITY-5, F5 | `lj_main_menu_top_table()` / `lj_main_menu_inert_table()`, `test_desktop` |
| Desktop popup, Icons > Unassign Icons | two sub-items whose captions the notes give only as "(all on current tab / selected icons)" | `All on Current Tab`, `Selected Icons` | PROPOSAL (captions unverified) | `src/desktop/lj_main_window.c` `lj_pop_unassign` |
| Fixture toolbar, Reset (button 9) | Fixture reset of the master's profile | Routed to the hotkey function `Home Selected Fixtures`; the profile's reset static function is applied by the Lamp control | PROPOSAL (no Fixture Reset row in the hotkey table) | `src/desktop/lj_toolbars.c` `B_FX_RESET` |
