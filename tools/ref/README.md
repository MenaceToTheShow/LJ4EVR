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

`w5_wire.py` is ours, not a vault copy: the loopback peers of the W5 acceptance walkthrough (an Art-Net node and sACN
listener on 127.0.0.2, sACN / Art-Net senders, a TCP remote client, a TCP receiver for External Application output)
and `prep`, which writes the scratch library the `tests/ui/scripts/w5_acc_*.lj4s` scripts run in. It imports
`08-sacn_ref.py` for the E1.31 packets. See `docs/w5-integration.md`, "Acceptance".

`pe_tpf0.py` and `golden_anchors.py` are ours too (W6 fidelity track). `pe_tpf0.py <exe> <out dir> [NAME ...]` reads the
named RT_RCDATA TPF0 form streams of a Delphi exe (bounds-checked, never run) and writes them as DFM text in the
dfm_text form; it reproduces the 208 LightJockey.exe texts of `reference/installer/exe_resources/dfm_text/` and wrote
the three side-exe forms there (TLJSOUNDAPP, TLJMEDIAFORM, TRS232SETTINGSFORM; gitignored, read by
`caption_diff.py --dfm`). `golden_anchors.py --ref <reference dir>` writes the `tests/ui/golden/REVIEWED.tsv` rows of
a golden re-render: the pixel diff against a git revision, and for the DFM-only template rows the DFM anchors probed
in the golden and the CHM capture with its scale (ME-162). Both read `reference/` as data only; `pe_tpf0.py --selftest`
needs no reference tree (a CTest row), and `golden_anchors.py` only prints its rows unless `--write` is given; see
`docs/w6-track-fidelity.md`.
