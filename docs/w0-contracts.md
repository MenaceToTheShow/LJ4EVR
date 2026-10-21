# W0 contracts: the headers every track codes against

One page. The headers are the contract; design.md is the source they were
written from (sections 2.0-2.9, 3.1-3.3, 7.6). Deviations from the design's
prototypes are listed at the bottom.

## Map

| Header | Types | Functions / constants | Implemented by |
|---|---|---|---|
| `core/lj_types.h` | limits enum `LJ_LINKS 4, LJ_LINK_CH 512, LJ_CHANS 2048, LJ_FIXTURES 100, LJ_CUE_SLOTS 12, LJ_BG_SLOTS 5` | `lj_fatal()`, `LJ_PRINTF(f,a)`, `LJ_COUNT_OF(a)` | core (`lj_log.c` holds `lj_fatal`) |
| `core/lj_str.h` | - | `lj_str_copy`, `lj_cp1252_to_utf8`, `lj_utf8_to_cp1252`, `lj_fmt_time` | core |
| `core/lj_ini.h` | `lj_ini` (opaque) | `lj_ini_load/new/save/geti/seti/gets/sets/free` | core |
| `core/lj_textrec.h` | `lj_rec` (opaque) | `lj_rec_open_read`, `lj_rec_next` (1/0/-1), `lj_rec_open_write`, `lj_rec_write`, `lj_rec_close` | core |
| `core/lj_log.h` | `lj_log_sink` | `lj_log`, `lj_log_set_sink` (the one global), `lj_log_overrun` | core |
| `core/lj_clock.h` | - | `lj_clock_us` | core |
| `core/lj_lock.h` | - | `lj_lock`, `lj_unlock` (static init, not recursive) | core |
| `gfx/lj_surface.h` | `lj_surface {w,h,px}` 0x00RRGGBB | `lj_surface_init`, `lj_surface_free` | gfx |
| `gfx/lj_draw.h` | `LJ_CLR_*` palette | `lj_fill, lj_hline, lj_vline, lj_bevel1, lj_button_raised, lj_edge_sunken, lj_focus_rect, lj_blit_1bit, lj_blit_rgb` | gfx |
| `gfx/lj_font.h` | `lj_font {height, ascent, descent, advance[224], offset[224], bits}`; glyph g = standalone 1-bit bitmap advance[g] x height, stride (advance+7)/8, at bits+offset[g] | `lj_font_sans13/sans13b/arial14/arial14b/arialblack16`, `lj_text_width`, `lj_text` | gfx + fontpack |
| `gfx/lj_bmp.h` | - | `lj_bmp_write` (24-bit, no reader) | gfx |
| `ui/lj_widget.h` | `lj_rect`, `lj_align` (alNone..alClient), `lj_anchors` (`LJ_AK_*`), `lj_mouse_ev`, `lj_key_ev`, `lj_ui_event` (`LJ_EV_*`), `enum lj_widget_event` (`LJ_WEV_*` = DFM OnXxx), `lj_widget_vt`, `struct lj_widget` | `lj_wev_names[]`, `lj_widget_add/remove/find/abs/invalidate/collect_dirty/layout/resize/hit/set_focus/focused` | **done** (`ui/lj_widget.c`, test `lj4evr_tests widget`) |
| `ui/lj_window.h` | `lj_window` (opaque), `lj_binding {event, fn, ud}`, `lj_window_caption_state` | `lj_window_create/destroy/find/event/paint/set_caption`, accessors `_surface/_desc/_root/_menu/_client_rect/_dirty/_invalidate_all/_caption/_set_active/_set_native_frame/_resize/_set_native/_native` | ui (`lj_window.c`, `lj_frame.c`, `lj_menu.c`) |
| `ui/forms/lj_form.h` | `enum lj_ctl_row_kind` (`LJ_K_<CLASS>`, every census class + ME-17 four + non-visual), `lj_font_id`, `LJ_NO/LJ_YES`, `LJ_CLR()`, `lj_border_style`, `lj_form_style`, `LJ_BI_*`, `lj_position`, `lj_bevel`, `lj_text_align`, `lj_event_desc`, `lj_column_desc`, `lj_ctl_desc`, `lj_menu_item_desc`, `lj_menu_desc`, `lj_form_desc` | `lj_ctl_class_names[]`, `lj_form_find_ctl`, `lj_form_find_menu`, `lj_form_count_ctls`, `lj_forms_all` | **done** (`lj_form.c`); `lj_forms_all` moves to `lj_forms_gen.c` (dfm2c) and `lj_form_example.c` is then deleted |
| `engine/lj_engine.h` | `lj_engine` (opaque), `lj_frame`, `lj_cmd` (`LJ_CMD_*`), `lj_engine_status` (`LJ_SLOT_*`), `LJ_ENGINE_CMD_CAP 256` | `lj_engine_create/destroy/bind/cmd/tick/frame/view` | engine (W0: deterministic stub) |
| `platform/lj_platform.h` | `lj_platform_cfg`, `LJ_WF_*` | `lj_platform_init/open_window/close_window/show_window/move_window/window_pos/present/run/quit/shutdown/load_bmp/clipboard_get/clipboard_set/pref_path/sleep_us/engine_start/engine_stop` | platform/sdl (`lj_sdl.c`, the only SDL includer) |

Build: `CMakeLists.txt` globs `src/<dir>/**/*.c` into `lj_<dir>` static libraries
(core, fixlib, model, engine, io, gfx, ui, desktop, controls, windows; platform
and app only with `LJ_SDL=ON`). `lj4evr_tests` = `tests/**/*.c` + the SDL-free
libs; `lj4evr_render` = `src/app/lj_render_main.c` + the same; `lj4evr` =
`src/app/main.c` + `src/app/lj_*.c` + `lj_platform`. Each `src/<dir>/lj_stub.c`
exists only so the library has an object: delete it with your first real file.

## Three rules for the tracks

1. **Where you write.** Only inside your `src/<dir>/` (and `tests/<dir>/`,
   `tools/`, `art/` when your brief names them). Replace `src/<dir>/lj_stub.c`;
   do not edit another track's headers. If a contract header is wrong for you,
   say so in your return notes with the exact prototype you need; the integrator
   changes the header. A test is a subcommand row in `tests/lj_tests.c` plus its
   name in `LJ_TEST_SUBCOMMANDS` in the root `CMakeLists.txt` (those two lines
   are the only shared files a track touches).
2. **What you may include.** The graph of design section 2, enforced by
   `tools/test_layering.py`: `core` nothing but itself; `fixlib` <- core;
   `model` <- fixlib, core; `engine` and `io` <- model, fixlib, core; `gfx` <-
   core; `ui` <- gfx, core; `desktop`/`controls`/`windows` <- ui, model, engine,
   fixlib, io (and each other); `platform` <- core, gfx, ui, engine, io, SDL3;
   `app` <- everything. `<SDL3/...>` only under `platform/` and `app/`. OS
   headers only on the allowlist in the script. No `float`, `double`,
   `<math.h>`, `<stdatomic.h>` or `volatile` outside `platform/`. Every
   identifier `lj_`/`LJ_`, files `lj_<name>.c`, one `.h` per `.c`, guard
   `LJ_<DIR>_<NAME>_H` (`ui/forms/lj_form.h` -> `LJ_UI_FORMS_FORM_H`).
3. **How to compile one file standalone** (MSVC, from any shell):

       powershell -c 'cmd /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" >nul && cl /nologo /c /std:c11 /W4 /WX /permissive- /utf-8 /fp:strict /I D:\lj4evr\src /I D:\lj4evr\build-msvc\generated /Fo%TEMP%\x.obj D:\lj4evr\src\<dir>\lj_<name>.c"'

   gcc/clang: `cc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wvla -Werror
   -ffp-contract=off -fsyntax-only -I src -I build-msvc/generated src/<dir>/lj_<name>.c`.
   A header is checked the same way through a one-line TU
   (`#include "ui/lj_window.h"`), which is what `test_layering.py` does for all
   of them. The full build: `cmake --preset dev-msvc && cmake --build --preset
   dev-msvc && ctest --preset dev-msvc`; SDL-free: `cmake -S . -B build-nosdl
   -DLJ_SDL=OFF && cmake --build build-nosdl --config Release && ctest
   --test-dir build-nosdl -C Release`.

## Deviations from design.md prototypes (and why)

- `lj_engine_view`: the design uses the name for both the struct and the
  function; C puts typedef names and function names in one namespace, so the
  **struct is `lj_engine_status`** and the function keeps the design name. The
  function takes a non-const `lj_engine*` because it clears the `pending_*`
  fields it hands out.
- `lj_widget_vt.mouse/key` keep the design signature; the event type
  (down/up/move, key down/up/char) travels inside `lj_mouse_ev.type` /
  `lj_key_ev.type`.
- Limits: `LJ_LINKS .. LJ_BG_SLOTS` live in `core/lj_types.h` (the brief), not
  `model/lj_model.h` (design 2.0); the model's enum adds the rest and includes
  this header.
- `core/lj_ini.h` adds `lj_ini_new`, `lj_ini_sets`, `lj_ini_free`;
  `core/lj_textrec.h` adds `lj_rec_close`; `gfx/lj_surface.h` adds
  `lj_surface_init/free`; `engine/lj_engine.h` adds `lj_engine_destroy`. A
  pointer-returning constructor needs a destructor.
- `lj_rec_next` returns 1/0/-1 (design 2.0's iterator rule) rather than the
  "0 at EOF" of the 2.1 comment, which is the same thing stated from the other side.
- `lj_form_desc` has no `.menu = &menu` pointer as the 7.6 sketch shows; menus are
  an array `menus[]` of named `lj_menu_desc` (a form owns several
  `TPopupMenu`s and controls reference them by name) and `.menu` is the main
  menu's name.
- Invalidation lives in the widget (`dirty` bit + `lj_widget_collect_dirty`)
  rather than in a window-side rect list, so `lj_widget.c` links without
  `lj_window.c`; `lj_window_paint` collects and `lj_window_dirty` reports.
- `CPACK_NSIS_TEMPLATE` does not exist in CMake; the per-user template is picked
  up through `CPACK_MODULE_PATH = packaging/nsis` (CPack's template lookup).
- `cmake_minimum_required(3.25)` not 3.24: `FetchContent_Declare(... SYSTEM)`.
