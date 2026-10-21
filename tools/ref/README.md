# tools/ref

Reference scripts copied unchanged from the design vault,
`Autonomy/projects/_staging/lj4evr/notes/research/` (2026-10-21). They are the
executable half of the research notes with the same number and are the source of
`tests/vectors.h` and `src/engine/lj_trig_gen.c` through `tools/gen_vectors.py`.
Python 3 stdlib only; run with `python -I`. Do not edit here: fix the vault copy,
then re-copy.

| File | Note | What gen_vectors.py takes from it |
|---|---|---|
| `08-sacn_ref.py` | 08-sacn-and-output.md | E1.31 packet builders (Appendix B vectors); the socket self-test is not run |
| `14-macros_mac_decode.py` | 14-gap-generic-macros-and-statics2.md | the sine-family formula (`round(32640 * sin)`); the Macros.Mac decode needs Martin's file and is skipped |
| `16-gs2_ref.py` | 16-gap-gs2-format-decode.md | the `.GS2` field layout table and record size |
| `17-position_ref.py` | 17-gap-position-maths.md | pan/tilt, fan-out, auto-delay, macro-index and cue-scaling vectors; its `demo()` self-check is run |
