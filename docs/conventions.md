# Conventions

Design section 2.0, copied verbatim. Every file under `src/` follows these;
`tools/test_layering.py` checks the mechanical ones (names, guards, include
graph, OS headers, float/atomics/volatile, `lj_table_*` in engine/, each header
compiling alone). The W0 additions that the headers carry are listed at the end.

## 2.0 Conventions

Every file under `src/` follows these; `tools/test_layering.py` checks the mechanical ones (verdict ME-15, WIZARD_STYLES-3/4/5/15/16/17).

- Returns: `int` functions return 0 on success and -1 on failure; pointer-returning functions return NULL on failure. Iterators return 1 (item produced), 0 (end), -1 (error); `lj_rec_next` is the model. Every output buffer is passed with a `size_t cap` and is always NUL-terminated (`lj_str_copy`).
- Errors: `char *err, size_t cap` parameters exist only at import boundaries (`lj_gs2_parse`, `lj_udf_import`, the `lj_<x>_load` readers, the W5 `.LJB` importer); everywhere else -1/NULL plus one line in the log sink is the whole report. A failed `malloc` outside `engine/` calls `lj_fatal(const char *fmt, ...)` (core: logs, then aborts); the engine never allocates after `lj_engine_create()`, so it has no such path.
- Names: every external identifier starts with `lj_` (functions, types, variables) or `LJ_` (macros, enum constants). Files are `lj_<name>.c`; generated files are `lj_<name>_gen.c` and open with a banner naming the tool, its inputs and their hashes, and "do not edit". `app/main.c` and `third_party/` are the only exceptions.
- Headers: one `.h` per `.c` with the same stem; include guard `LJ_<DIR>_<NAME>_H` (`engine/lj_merge.h` -> `LJ_ENGINE_MERGE_H`); every header includes what it uses and compiles alone in its own TU (`test_layering.py` compiles each one once, section 2). The limits enum (`LJ_LINKS`, `LJ_LINK_CH`, `LJ_CHANS`, `LJ_CUE_SLOTS`, ...) lives in `model/lj_model.h` (section 4); `core/lj_types.h` holds only the fixed-width and string types `core` itself needs.
- Globals: none except the log sink (2.1); state lives in the struct a function is handed.
- Renamed before any code because the old names collided or read as something else: `lj_static` -> `lj_gs2_sfunc` (a `.GS2` static-function record, not a storage class), `lj_widget_kind` -> `lj_ctl_row_kind` (a control-window row, not a `ui/` widget), `LJ_SLOTS` -> `LJ_CUE_SLOTS` and `LJ_CH` -> `LJ_LINK_CH` (sACN already owns "slots" and "channels"), `S1_*` -> `LJ_POST_S1_*`. This document uses the new names throughout.

## W0 additions (what the headers fixed, beyond the text above)

- `core/lj_types.h` carries the six engine-facing limits (`LJ_LINKS`, `LJ_LINK_CH`,
  `LJ_CHANS`, `LJ_FIXTURES`, `LJ_CUE_SLOTS`, `LJ_BG_SLOTS`) so `engine/`, `io/` and
  `gfx/` need no `model/lj_model.h`; the model adds the rest of the limits enum
  and must not redefine these six.
- `lj_fatal()` is declared in `core/lj_types.h` and implemented in `core/lj_log.c`.
- Generated form tables: tri-state bools (`0` = not given, `LJ_NO`, `LJ_YES`),
  colours tagged with `LJ_CLR()`, class-specific Delphi enums as verbatim strings;
  see `ui/forms/lj_form.h`.
- Widget classes embed `lj_widget` as their first struct member.
- `tests/lj_tests.c` is the one test driver; a check is a subcommand there plus one
  name in `LJ_TEST_SUBCOMMANDS` in the root `CMakeLists.txt`.
