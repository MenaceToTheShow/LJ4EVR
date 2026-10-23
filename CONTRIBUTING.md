# Contributing to LJ4EVR

LJ4EVR is a clone of Martin LightJockey 2 (2.209 feature set, 2.100 help as the
behavioural reference). The goal is fidelity: a user who ran LightJockey should
find every window, caption, menu item and DMX behaviour where they left it.

## Fidelity first

- Geometry, captions, menu trees, tab orders, default values and DMX facts are
  facts and are reproduced verbatim, including Martin's typos and duplicated
  popup items (decisions D2, D13). Do not "fix" a misspelled caption; if a
  deviation is unavoidable it is tagged in the source and listed in `docs/`.
- Behaviour comes from the 2.100 help and the research notes, not from memory.
  A claim in a commit message or a comment names its source.
- Pixel output is deterministic across Windows, macOS and Linux: no floats in
  SDL-free code, integer fade maths, one font pack. The `determinism` CI job
  compares frame dumps byte for byte and goes red on one differing byte.

## No Martin assets

Nothing of Martin's enters the repository: no bitmaps, icons, cursors, RTF
notes, help text, font bitmaps, fixture library files, `Macros.Mac` samples or
installer pieces. The `reference/` tree is gitignored and read as data only.
Chrome and icons are redrawn (ours live in `packaging/icons/` and `art/`, CC0);
fixture definitions come from open sources with provenance headers. A pull
request that adds a binary from `reference/` is closed. `docs/provenance.md`
lists every class of non-code bytes and where it came from; a new class gets a
row there in the same pull request.

Help topics (`help/*.txt`) are our own words. After editing one, run
`python -I tools/help_check.py --write` and commit the regenerated
`src/windows/lj_help_topics_gen.c` / `.h` with it. The `help_check` CTest fails on
a stale generated file, a line over 72 columns, French or German text, and (on a
machine with the reference tree) any run of 8 words shared with Martin's help.

## Code rules (design section 2.0, `docs/conventions.md`)

C11, no C++. Every identifier `lj_`/`LJ_`; files `lj_<name>.c` with one `.h`
each; include guard `LJ_<DIR>_<NAME>_H`; `int` functions return 0/-1, pointer
functions NULL on failure; no extern globals except the log sink (window state is file-static); strings are CP1252
bytes. `<SDL3/...>` only under `src/platform/sdl` and `src/app`. No `float`,
`double`, `<math.h>`, `<stdatomic.h>` or `volatile` outside `platform/`.
`tools/test_layering.py` enforces the mechanical ones and runs under CTest.

Warnings are errors on every compiler (`docs/building.md`). Every non-trivial
piece of logic leaves a runnable check behind: a subcommand in
`tests/lj_tests.c` plus its name in `LJ_TEST_SUBCOMMANDS`.

## Golden images

`tests/ui/golden/*.bmp` are reviewed renders, one per form. CI fails on any
pixel difference. Every changed golden gets a row in `tests/ui/golden/REVIEWED.tsv`
in the same commit: the pixels changed, their bounding box, the cause and what
was viewed (old and new side by side). `ui_golden` fails a form that has no row.
`LJ_GOLDEN_UPDATE=1` rewrites the goldens when you run the tests.

## Commits and pull requests

- `main` is not branch-protected (`gh api repos/UnbannableGT/LJ4EVR/branches/main/protection`
  answers 404; the wave integrators push to it directly). Everyone else works
  on a branch and opens a pull request. Fork PRs run the hosted jobs only
  (Windows, macOS) and need maintainer approval; the Linux jobs run on
  self-hosted runners after merge (`docs/ci.md`).
- Subject line: imperative, under 72 characters, with an area prefix when it
  helps (`engine:`, `ui:`, `ci:`, `golden:`).
- Do not bump `VERSION` in a feature PR; releases are cut separately
  (`docs/releasing.md`).
- Generated files (`lj_*_gen.c`) are committed; regenerate with the tool named
  in the file's banner and commit the tool change and its output together.

## Security

Report vulnerabilities privately, as `SECURITY.md` describes; not in a public
issue.

## Licence

MIT (`LICENSE`; a PROPOSAL until the maintainer's ruling before the public
release). Third-party notices in `NOTICE`, shipped as `THIRD_PARTY_NOTICES.txt`.
By contributing you agree your changes are MIT and your art is CC0. LightJockey
is a trademark of Martin Professional; LJ4EVR is not affiliated with or endorsed
by Martin Professional or HARMAN.
