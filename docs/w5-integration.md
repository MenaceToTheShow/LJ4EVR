# W5 integration log

Started 2026-10-23. Base: `52b999a` ("W5: contracts"); the seven tracks' returns committed as `f57fd58` ("W5: tracks
landed (pre-integration)"): 114 paths, 0 under `reference/`, 0 build trees, sha1 of every new or changed file against
13,637 `reference/` files: 0 equal; the private-detail grep (user name, host name, both LAN prefixes of this PC, the
Dropbox root, `Users\`) over the 114 paths: 0 hits beyond the retired-host word `atlas-ci2` in a `release.yml` comment
(kept, ME-148).

## Integrator 1: build and tests

Toolchain: MSVC cl 19.44 + Ninja (vcvars64), SDL from the `release-3.4.18` source (`FETCHCONTENT_SOURCE_DIR_SDL3`).
Trees: `build-w5i-rel` (Release, `LJ_SDL` ON), `build-w5i-dbg` (Debug, ON; the `debug-win` settings),
`build-w5i-nosdl` (Release, OFF), `build-w5i-fuzz` (Debug, OFF, MSVC `/fsanitize=address`, `LJ_FUZZ_N=200000`).

At `f57fd58` the integrated tree already built: the breakages the tracks reported (other tracks' half-edited files)
were gone once every track had returned. Measured there: 0 warnings / 0 errors in all three trees; ctest 152 / 150 /
152 of 152 / 150 / 152; `test_layering` 0 findings.

### Fixes in the integration commit

| Blocker (track) | Fix | Check |
|---|---|---|
| `lj_library_list` shows `<userdata>/User/ValueTables` as a library (dmxin) | the folder is skipped (`src/model/lj_library.c`) | `test_model_library`: a `ValueTables` folder beside the libraries is not listed |
| DMX-in Audio Gain mapped 127 = 0 dB, the Fingers jogs 128 = 0 dB (audio ask 3) | `windows/lj_dmx_in.c` calls `lj_audio_dmx_gain` / `lj_audio_dmx_noise`: one mapping | `test_dmx_in`: `LJ_FN_DMXIN_AUDIO` 128 -> gain 100 %, noise 200 |
| Fingers "DMX in (1-12)" .. "(37-48)" pages not wired (dmxin) | `lj_fingers_fader` on those pages calls `lj_dmxin_emulate((page - 10) * 12 + fader, level)` | `test_da2532_fingers`: page "DMX in (13-24)", fader 2 at 100 % -> DMX-in raw channel 14 = 255, `live`; 0 % -> 0 |
| `LJ_WIN_REMOTE_SETUP` reachable by id only (remote) | Setup > `&RS-232 / TCP Remote ...` after Global Patch (code-built, PROPOSAL rig W5-REM-4), `LJ_ACT_REMOTE_SETUP` appended, bound in `app/lj_app_io.c` | `test_desktop`: item present, enabled, bound (`menu_leaf_bound`) |
| Run Minimized persisted with no effect (audio ask 1) | `lj_app_io_start` opens the Analyzer when `[AUDIO] Enabled` and not Run Minimized (ponytail: no minimized state) | W5-X-OFF unchanged: `test_uiscript_w5` (fresh Hardware.Ini, audio off) |
| The W4DMX_* goldens referenced by no test (dmxin) | deleted (their REVIEWED rows stay: the file is append-only) | `ui_golden`, `test_dmx_in` green |
| `w3_acceptance.lj4s` asserts an `mp_play` line red (media) | `w3_mp_play.Cl2` line 2 is `da_play` (still inert); the stale `cuelist_run.lj4r` NOTE reworded (comment only) | `replay_all`, `replay_dump` unchanged |
| caption_diff 2 differences: TDMXINFORM `lChIn` / `lFreq` now carry the live count and rate | two F55 runtime-fill rows in `tools/caption_diff_excl.tsv` | caption_diff 0 differences |
| ci.yml fuzz step had only `fuzz_gs2` / `fuzz_udf` (all W5 parser tracks) | the five W5 libFuzzer targets, 60 s each, in the linux-clang step | the CI run below |
| fuzz rows fixed at 20000 iterations | `tests/lj_fuzz.h`: `LJ_FUZZ_N` overrides the count for a local long run | the 200k run below |

Not done here (integrator 2 or later, as the tracks returned them): ME-165 (`lj_widget_set_enabled`, one `LJ_NA_HINT`;
the tracks un-greyed with direct `enabled` writes and added no literal: `git grep -c "Not available in LJ4EVR" -- src`
= 24, as at the contracts commit), the Global Patch per-channel Use / Ignore DMX In Enable (needs a model field and an
engine S1 rule, W5-X-ENGINE), masters showing the DMX-in remote state, Statics Latch through a pinned path, Pass
Keypresses (an `lj_app_key` check), the `.Udf` half of W5-LJB-1, and every docs / fidelity / file-formats / provenance
row the tracks returned as text.

### Measured on the integration commit

    build-w5i-rel   (Release, SDL)        0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    build-w5i-nosdl (Release, LJ_SDL=OFF) 0 warnings, 0 errors   ctest 100% passed, 0 failed out of 151
    build-w5i-dbg   (Debug, SDL)          0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    test_layering                         465 file(s) checked, 222 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                          scene getters 0, set-text/lib/ctx copies 0, noop Help handlers 0, 0 finding(s)
                                          (with the W5 OS_ALLOW / PAIR_EXEMPT rows: io/lj_socket.c, io/lj_serial_win.c,
                                          io/lj_serial_posix.c + dirent.h, io/lj_vtr_gen.c, io/lj_audio_gen.c)
    fuzz rows (ctest, 20000 each)         fuzz_sacn_rx, fuzz_vtr, fuzz_artnet, fuzz_remote, fuzz_ljb: Passed (none DISABLED)
    fuzz rows, MSVC ASan, 200000 each     fuzz_sacn_rx / fuzz_vtr / fuzz_artnet / fuzz_remote / fuzz_ljb:
                                          "200000 iterations, PASS", rc 0, no ASan report
    replay_all, replay_w5_*               Passed (5 dmxin S1 scenarios, audio_trig, the contract scenario)
    font_corpus, ui_golden, ui_golden_ctl Passed
    caption_diff (w4_captions.lj4s + the four W5 window ids appended, scratch LJ4EVR_USERDATA)
                                          rc 0; 146 implemented form(s) of 207, 0 difference(s), 140 excluded by a fidelity row,
                                          84 absent at runtime (TExternalAppSetupForm is the new implemented form)
    goldens W5 changed                    12: CTL_EXTERNAL_EXTENDED, MAINWINDOW, W5AU_ANALYZER, W5AU_ANALYZER_REALTIME,
                                          W5DMX_{DMX_IN, EXPORT_TABLE, TRANSLATION, TRANSLATION_EDIT, VALUE_TABLE_ERROR},
                                          W5MED_MEDIA_PLAYER, W5X_EXTAPP_SETUP, WIN_CUELIST_FORM: each has a 2026-10-23 REVIEWED row
    determinism (dump-frames)             Release twice, Debug, no-SDL: equal, 1,641,600 bytes,
                                          sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50 (= W4)
    replay-frames.bin                     129,664,080 bytes, sha256 c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d
                                          in all three trees (= W4: the W5 scenarios sit outside the replay_dump glob)
    build-w5i-dbg lj4evr --selftest       rc 0, six OK lines; 0 "no binding" / "unknown event" lines
    W5-X-ENGINE                           grep "recv|send(|fopen" src/engine/*.c = 0
    W5-X-DEP                              FetchContent_Declare 1; git diff v1.0.0 -- third_party empty
    W5-X-RIG                              git diff v1.0.0 -- src/engine/lj_engine_rules.h empty; "PROPOSAL rig" 358 -> 464
    ME-159                                grep self-hosted|pull_request_target|secrets. in workflows, beyond GITHUB_TOKEN = 0

Not run on this PC: gcc (no Linux toolchain here); the linux-gcc-asan job is the first gcc compile of the W5 files, and
`test_serial_pty` runs only on Linux / macOS.

### CI round 1 (`f57fd58`, `4c22a41`) and the fixes

windows-msvc and generate-check green; linux-gcc-asan, linux-clang and macos-clang red:

- clang `-Wcomment`: `windows/lj_dmx_in.c:9` and `:89` wrote `ValueTables/*.vtr` inside a block comment; now
  `ValueTables/<name>.vtr`. A scan of every `.c` / `.h` under `src`, `tests` and `tools/fuzz` for `/*` inside a comment:
  0 left.
- gcc `-Wformat-truncation`: `model/lj_ljb.c:221`. The return value of that call and of 31 more W5 `snprintf` calls
  that write a fixed-size array into a smaller buffer is now checked, the repo idiom
  `if (snprintf(d, ...) < 0) d[0] = 0;` (ljb, serial_posix, audio_form, backup_restore, dmx_in, test_dmxin,
  test_audio_form, test_dmx_in).
- `tools/fuzz/fuzz_vtr.c:14` held `'\'` (a backslash lost to a shell heredoc): `'\\'`. It only builds with
  `LJ_FUZZ=ON` (clang), so no Windows tree saw it.

Local clang check, a stand-in for the two clang jobs: zig cc 0.16.0 (clang) in the session scratchpad, the CI flags
(`-std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wvla -Werror -O2`), every SDL-free `.c` for `x86_64-linux-gnu`: 339
files, 0 failures beyond the three that need their platform (`lj_midi_coremidi.c`, `lj_midi_winmm.c`) or a CMake
define (`test_corpus.c`); `src/io`, `src/model`, `tests/io`, `src/app` and `src/platform/sdl` (SDL3 headers) for
`aarch64-macos`: 0 failures. MSVC trees and ctest unchanged after the fixes (153 / 151 / 153 passed).

### CI round 2 (`2feb44b`) and the fixes

windows-msvc, generate-check and linux-clang green: `test_serial_pty` passed on Linux, and the fuzz step printed an
`INITED` / `DONE` pair for each of the seven targets (`fuzz_gs2`, `fuzz_udf` and the five W5 parsers, 60 s each).
Red:

- linux-gcc-asan `-Wmisleading-indentation`: the round-1 idiom left `if (...) d[0] = 0; next;` on one line in
  `windows/lj_dmx_in.c:243` and nine rows of `tests/io/test_dmxin.c`; each is now braced. A scan of every `.c` for an
  unbraced `if` / `for` / `else` body followed by more code on the same line: only the `else do { } while` of
  `tests/ui/test_windows_c.c:378`, which gcc has always accepted.
- macos-clang `test_remote`: the listener's backlog was `LJ_REMOTE_CONNS` (4), so macOS dropped one of five
  simultaneous SYNs. The fifth client was then never accepted and refused, and the dropped one was `cl[0]`, whose close
  the test waits for. The backlog is now `4 * LJ_REMOTE_CONNS`; the cap of 4 open connections is unchanged.
- macos-clang `test_extapp`: the test sent `GO` microseconds after closing the peer, before the loopback FIN arrived,
  so the bytes went into the dead socket (normal TCP). The test now waits 100 ms after the close, as a real peer loss
  does. The extapp code is unchanged.

MSVC trees after these fixes: ctest 153 / 151 / 153 passed (including `test_layering`).

### CI round 3 (`7b0172b`) and the fix

windows-msvc, macos-clang, linux-clang and generate-check green. linux-gcc-asan red on `-Wformat-truncation` at
`windows/lj_hardware_setup.c:55-56` (`"cbProto%d"` into `char[16]`: gcc counts an unknown `int` as up to 11 bytes even
at level 1). Every remaining `snprintf` added in W5 whose return value was unused (67 lines, listed by a diff scan from
`52b999a`) now checks it with the same idiom, braced where more code follows on the line. The five left unchecked write
into 96- or 292-byte rows with a bounded format and advance an index in the destination expression
(`ea.rows[n++]`, `rep->lines[rep->nlines++]`). MSVC trees: ctest 153 / 151 / 153 passed.

### CI round 4 (`fbaa9c3`) and the fix

Every job built; windows-msvc, macos-clang, linux-clang, generate-check green. linux-gcc-asan: 151 of 152, LeakSanitizer
in `test_ljb` (23 bytes from `lj_ljb_extract`): the test reused `x` for the out-of-range call, which sets it to NULL
before the earlier buffer was freed. The test now frees it first; `lj_ljb_extract` is unchanged.

### CI green: run 39993461343 (`80705f3`)

https://github.com/UnbannableGT/LJ4EVR/actions/runs/39993461343: six of six jobs green (generate-check,
linux-gcc-asan 152 of 152 under ASan + UBSan, linux-clang with the seven 60 s fuzz targets, windows-msvc Release + Debug
ctest + selftests + installer, macos-clang, determinism). determinism: `frames-{linux,windows,macos}.bin` all
`5b13f419...63114b50`; `replay-frames.bin` all `c46fee4c...5039d` (both = W4).

## Acceptance (W5 integrator 2, acceptance walkthrough)

Session of 2026-10-23 on the work PC (Windows 11, MSVC cl 19.44 + Ninja), `build-w5i-rel\lj4evr.exe` (Release, SDL ON).
Every row of `docs/w5-gates.md` section 2 walked from a fresh scratch userdata (`LJ4EVR_USERDATA=<repo>\build-w5acc\udN\`,
gitignored, so no account path reaches a capture), library `W5Acc` written by `python -I tools/ref/w5_wire.py prep`
(our own text: three fixtures, seven sequences, seven cues, six cue lists, `GlobalPatch.GPA`, `DirInfo.Ini`, a hotkey
setup, a 4 s WAV, an External Application definition file). The app runs one `tests/ui/scripts/w5_acc_*.lj4s` per step
with the userdir as the working directory; the wire peers are `tools/ref/w5_wire.py` (an Art-Net node and sACN listener
on 127.0.0.2, sACN / Art-Net senders from 127.0.0.2, a TCP remote client, a TCP receiver), all on loopback: no LAN, no
multicast join off-host. `Hardware.Ini [SACN]` of the scratch userdata: `Nic=127.0.0.1`, `Name=LJ4EVR`,
`Unicast1=127.0.0.2`. The quoted results are from the final pass (every step in one run, after the fixes below).
Captures: `docs/screenshots/w5/` (36, chosen from that pass; our windows only, viewed for private detail: none).

| # | Feature | Result |
|---|---|---|
| 1 | Art-Net output | tick |
| 2 | Audio Analyzer and audio trig | tick (synthetic, and real audio through a capture device) |
| 3 | `.LJB` importer | tick |
| 4 | DMX-in over sACN and Art-Net input, `.vtr` tables | tick (the masters' remote state was missing: built here) |
| 5 | RS-232 / TCP remote | tick (TCP here; serial = CI's `test_serial_pty` on Linux) |
| 6 | External Application string output | tick |
| 7 | LJ MediaPlayer | tick |

### Row 1: Art-Net (`w5_acc_artnet_1.lj4s`, `w5_acc_artnet_2.lj4s`, `w5_wire.py node 40 --answer 20`)

Hardware Setup: `cbProto2` sACN -> Art-Net (click, Down, Enter), `seArtAddr2` = 2, OK; `Hardware.Ini` gains
`Proto2=1`, `Addr2=2`. Cue list 3 (cue 1: DIM1 on 1:001 and DIM2 on 2:001 fade 0 -> 255 over 3 s) runs; the node
answers ArtPoll for its first 20 s, then stays silent and keeps counting. Node capture (its own clock):

    artpoll  count 13, intervals 2.5 2.5 2.5 2.53 2.52 2.53 2.5 2.53 2.5 2.5 2.5 2.5 s, replies sent 8 (answering until 20 s)
    artdmx   count 147, Port-Address [2], length [530], sequence 0 seen 0, first 0.547 s, last 27.219 s
             slot 1 every 20th packet [0, 43, 85, 128, 170, 213, 255, 255]
             packets per second 18 40 40 23 1 1 1 1 1 2 1 1 1 1 1 1 1 1 1 2 1 1 1 1 1 1 1 1 0 0
             gaps over 100 ms (first 20) 0.922 0.906 0.922 0.891 0.937 0.891 0.906 0.922 0.906 0.891 0.906 0.891 0.937 0.922 0.922 0.906 0.891 0.906 0.922 0.906
             after 28 s (answer end + 8 s) 0
    sacn     count 158, universes [1], slot 1 last 255
             packets per second 20 40 40 25 1 1 1 1 1 1 2 1 1 1 1 1 1 1 1 1 2 1 1 1 1 1 1 1 1 1 2 3 0

- ArtPoll every 2.5 s (22 bytes) to the loopback directed broadcast 127.255.255.255 (mask 255.0.0.0), received on
  127.0.0.2; the node's ArtPollReply (SwOut Port-Address 2) subscribes it.
- ArtDmx only for Port-Address 2, unicast to 127.0.0.2, 530 bytes, sequence never 0, 40/s during the fade (slot 1
  rising to 255), then one keep-alive every 0.89-0.94 s. The node's last reply answers the poll at 18.0 s; the last
  ArtDmx goes at 27.2 s: the node is dropped at the fourth unanswered poll, 7.5 to 10 s after its last reply
  (`lj_artnet.h` said 7.5-9 s and `docs/artnet.md` 7.5 s: both corrected), and from then on nothing is sent.
- Link 1 stays sACN: universe 1 unicast to 127.0.0.2 at 40/s during the fade, then keep-alives. Universes 3 and 4
  multicast and fail on loopback (`sacn: sendto 239.255.0.3 failed: 10051`: the loopback interface has no multicast
  route), shown as `last send error 10051`.
- Status line: `U1 40 pkt/s | U3 1 pkt/s | U4 1 pkt/s | ... | Art-Net 40 pkt/s, 1 nodes` while fading
  (`artnet-fading-THardwareSetupForm2.png`), `Art-Net 1 pkt/s, 1 nodes` idle, `Art-Net 0 pkt/s, 0 nodes` after the
  node is dropped (`artnet-nonode-THardwareSetupForm2.png`).
- Restart (`w5_acc_artnet_2.lj4s`): `cbProto1` "sACN", `cbProto2` "Art-Net", `seArtAddr2` 2, `Proto2=1` in the file.

Found and fixed: the sACN `U<n> pkt/s` part of the Hardware Setup status line read 0 since W1: nothing ever called
`lj_hardware_setup_status`, the setter it read. The line now reads the app's engine-status copy (`ctx->view`), as its
Art-Net part does; the dead setter is gone.

### Row 2: Audio (`w5_acc_audio_1.lj4s`, `w5_acc_audio_2.lj4s`, `w5_acc_audio_real.lj4s`)

Hardware Setup > Audio: `RGAudio` Disabled -> Enabled, device "Default device", OK (`audio: recording from "default" at
44100 Hz mono S16`). &Audio > Start Audio Analyzer (`M_RestartSound`) opens TLJSoundApp. Cue 4 (sequence 3: DIM1 at 40 /
80 / 120 / 160) is in Audio trig mode with 2 Audio-Trigs / Trig. A click = `audio_inject 30 20` then
`audio_inject 1500 1`, 450 ms apart (the default decay is 312 ms):

- frame 1:1 reads 40 after click 1, 80 after click 2, 80 after click 3, 120 after click 4: one scene per two trigs;
- 120 ms after a click the main status bar's Trig panel is blue (pixel (0,0,255) in `audio-trig-TLJMainForm.png`), and
  so is the Analyzer's own Trig panel (`audio-trig-TLJSoundApp.png`);
- Emulate Audio Trig (key E of `Hotkeys/Setup1.Hot`, our file, through the `hotkey` verb) twice: 120 -> 160;
- View > Show input > Time-line, two more clicks: 160 -> 40 (the sequence wraps), trig lines and BPM drawn
  (`audio-timeline-TLJSoundApp.png`);
- Trigger > Noise level > High and the noise fader to its end (`TrackBar3` 455), two more clicks: the frame stays 40;
- Analyzer > Auto Save Changes, close: `AutoSaveChanges=1`, `NoiseLevelSwitch=2`, `ShowInput=2` in `Hardware.Ini`; after
  a restart the Audio page reads Enabled and `TrackBar3` is 455 (`w5_acc_audio_2.lj4s`).

Real audio: this PC has capture devices, among them a virtual loopback audio cable (what another program plays into its
input arrives on its capture side). With `[AUDIO] Device` set to that capture side and cue 4 running, three clicks
0.5 s apart played into it by a separate program move frame 1:1 from 40 to 80 (three trigs, one scene) and draw three
trig lines at 123 BPM (`w5_acc_audio_real.lj4s` asserts both frames). That capture is not committed: its device combo
names the driver.

### Row 3: `.LJB` (`w5_acc_ljb_1.lj4s`, `w5_acc_ljb_2.lj4s`)

`lj4evr_tests test_ljb --write <userdir>` (new: writes the test's synthetic containers; no LJ2 byte) gives `Show.LJB`,
`Bad.LJB` (one flipped byte) and `Evil.LJB` (an entry `..\..\evil.txt`). User Libraries > Restore opens at the userdir
and lists the three (`ljb-restore-list-TLibraryRestoreForm.png`):

- Bad.LJB: Restore, OK -> `Warning: LJB: trailer CRC mismatch` (`ljb-bad-refused-TConfirmForm.png`);
- Evil.LJB: Restore, OK -> `Warning: LJB: refused path in entry 17`; no `evil.txt` in the userdir or in `User`, no
  `MyShow` folder;
- Show.LJB: `lLibName` "MyShow"; Restore asks `Import "MyShow" as a new user library ?`; OK: the window shows the report
  (`ljb-imported-TLibraryRestoreForm.png`) and `User\MyShow\LJBImport.txt` holds it, from `CueLists\AutoSave.Cl2: cue
  list not converted (line records undecoded, note 18 s11)` to `9 converted, 7 not converted`; the restart prompt's OK
  selects MyShow (`app: user library MyShow opens at the next start`) and closes LJ4EVR;
- the next start (no `U:` argument) opens `LJ4EVR (User lib: MyShow)` with fixture `(1) MAC2000` placed and sequence
  0003 "Chase" (2 scenes) in Select Sequence (`ljb-opened-*.png`).

The oversize-entry refusal is a `test_ljb` row (W5-LJB-1), not walked here.

### Row 4: DMX-in (`w5_acc_dmxin_1.lj4s` .. `_3.lj4s`)

Part 1, configured in the windows and driven by `sacn_in` (a real E1.31 packet through `lj_dmxin_rx`): Hardware Setup
`cbUseDMXin` on, universe 10, sACN, OK (`dmx in: sACN universe 10 on 0.0.0.0:5568, joined on 127.0.0.1`). The C-L DMX In
button opens the window; channels 11 / 12 / 13 / 14 get Binary Cue Value 1 / Binary Cue Value 4 / Latch Cue / Master
Intensity (keyboard selection in both lists, Assign Function); Value-translations puts the SYSTEM Off/On table on
channel 3 (`dmxin-translation-TDMXInValueTranslationForm.png`). `GlobalPatch.GPA` patches 1:1 (DIM1) from DMX-in 2 in
HTP mode and 1:3 from DMX-in 3 Absolute.

- Before Save: binary 5 latched and the master channel at 128 -> cue 0, GM 255, no `DmxIn.dat`; the patch already
  follows (1:1 = 200 from DMX-in 2, 1:3 = 200 untranslated) (`dmxin-before-save-TDmxInForm.png`).
- Save (`dmx in: saved ...\DmxIn\DmxIn.dat (4 assignments)`): ch 14 at 128 -> GM 128, at 255 -> GM 255; Latch rising
  with binary 5 -> cue 5 (1:1 = 128, above DMX-in 2 = 50 in the HTP), `eCue` "5", `eCuename` "Cue5"; ch 3 at 100 ->
  1:3 = 0, at 200 -> 255 (the Off/On threshold); the Grand Master's mode text red and its fader refusing the mouse
  (`dmxin-after-save-*.png`).
- Options > Disable DMX-in functions: binary 1 latched does not load cue 1, the master channel at 64 leaves GM at 255,
  1:1 and 1:3 keep following DMX-in, and the Grand Master's fader takes the mouse again.

Part 2, live from `w5_wire.py send sacn 10 0:2=200,3=100,14=255 6000:3=200 12000:stop` (40 packets/s):
`DMX Channels In 512`, `Update Freq 40 Hz` (`dmxin-live-TDmxInForm.png`); 1:1 = 200, 1:3 = 0, then 1:3 = 255 at the
table threshold; the sender stops at 12 s, and 4.5 s later the monitor reads `Update Freq 0 Hz` while the frame holds
1:1 = 200 and 1:3 = 255 (`Loss=0`, hold; `dmxin-lost-TDmxInForm.png`).

Part 3, Art-Net input: Hardware Setup `cbDmxInSource` -> Art-Net, OK (`[ARTNET] In=10`, `socket: udp bound to
127.0.0.1:6454`); `w5_wire.py send artnet 10 0:2=150,14=255 9000:stop` from 127.0.0.2: `DMX Channels In 512`, 40 Hz,
1:1 = 150 (`dmxin-artnet-TDmxInForm.png`).

Found and fixed:
- the gate's "masters show the remote state" was not built: a master driven by a saved Master Intensity or Intensity
  Gr.n function now shows its mode text red and its fader refuses the mouse while DMX-in is on and its functions are
  enabled, whether or not input arrives (05 s3.2); Disable DMX-in functions releases it (`lj_master_intensity.c`;
  `assert_enabled` rows in part 1);
- `eCuename` beside Cue # was never filled: it shows the latched cue's name (`lj_dmx_in.c`);
- `[ARTNET] In` on disk stayed 0 after the DMX-in source changed to Art-Net (the input worked: the app derives it from
  `[DMXIN]`); the DMX-in apply now writes `[ARTNET]` too (`lj_app_io.c`).

### Row 5: TCP remote (`w5_acc_remote.lj4s`, `w5_wire.py tcp-send 7232 ...`)

The client is refused until the listener exists (a fresh userdata has no `[REMOTE]` and nothing listens on 7232).
Setup > RS-232 / TCP Remote: `cbInterface` "127.0.0.1 (local)", `eTcpPort` "7232", TCP remote enabled, OK
(`TcpEnabled=1`, `TcpIp=127.0.0.1`; `remote: TCP listening on 127.0.0.1:7232 (no authentication, this computer only)`).
Client:

    2.219 connected 127.0.0.1 -> 127.0.0.1 7232 after 1 refused attempt(s)
    2.219 sent [129, 1, 2]
    3.219 sent [128, 0, 1]
    4.219 sent [134, 1, 1]
    5.219 sent [132, 2, 1]
    6.328 flood 200000 bytes
    7.219 sent [200, 1]
    7.313 sent [5, 5, 5]
    8.219 closed

- `129 1 2` -> cue 130 (1:1 = 30); `128 0 1` loads cue list 1 (its line 1 is manual) and `134 1 1` goes: cue 2
  (1:1 = 200); `132 2 1` -> blackout (BLACKOUT ACTIVE, `remote-after-TStatusWarningForm.png`; the Generic DMX Dimmer has
  no shutter or blackout value, so 1:1 stays 200, 11 P15-P16);
- a 200000-byte flood with no byte 128..136, a half frame `200 1` and data bytes `5 5 5`: cue 2 and the blackout stay,
  the log has two rate-limited lines (`remote: ignored [...] (unknown command); <n> more ignored since the last line`),
  and the script's asserts ran on time through the flood (1024 bytes per connection per pass).

### Row 6: External Application (`w5_acc_extapp.lj4s`, `w5_wire.py tcp-recv 7000 32 --close 12`)

Fixture 3 (External Application/Device) as master, Extended window (TSpecialSettings_ExternalApp): `TRIG!1` Set Command on
scene 1, Add scene, `TRIG!2` Set Command, Get Command reads it back (`extapp-control-TExtended.png`); saved as sequence
0008, whose file holds `ext 3 cmd=TRIG!1` and `ext 3 cmd=TRIG!2`. Icon popup > Define Application Link, `extapp.ini`
(TRANSPORT=TCP, 127.0.0.1:7000), OK (`Transport1=2`, `Port1=7000`). Cue 6 (sequence 6: `TRIG!1`, `TRIG!2`, `V#X7F`,
`V#X7F`, 1 s each) runs. Receiver:

    1.594 accepted from 127.0.0.1
    1.703 recv 54 52 49 47 b'TRIG'
    2.610 recv 54 52 49 47 b'TRIG'
    3.610 recv 56 7f b'V\x7f'
    5.625 recv 54 52 49 47 b'TRIG'
    6.641 recv 54 52 49 47 b'TRIG'
    7.641 recv 56 7f b'V\x7f'
    9.656 recv 54 52 49 47 b'TRIG'
    10.672 recv 54 52 49 47 b'TRIG'
    11.672 recv 56 7f b'V\x7f'
    12.078 receiver closed (dead peer from here)

`TRIG` on each change, `V` + 0x7F for `V#X7F`, nothing for scene 4 (the same string). The receiver dies at 12 s: the
log gets `extapp: fixture 3: TCP peer closed (retry every 2000 ms)` once, the Extended window's link line goes from
`Link: Active` to `Link:-` (`extapp-running-` / `extapp-dead-TExtended.png`), and the show keeps running (1:1 still in
10..40 at the end).

### Row 7: MediaPlayer (`w5_acc_media.lj4s`)

Cue list 2 (`LJ4 cl 1`, the v1.0.0 record: MP Load File `tone.wav`, MP Play, MP Wait 00:00:02:00, cue 3, MP Wait
00:00:03:80) loads with no red line; `sbMPTime` on (the player opens), `sbCDTime` / `sbDATime` greyed; Preferences > Stop
Audio/Media Playback when cuelist stops on; Go: `media: tone.wav loaded, 44100 Hz, 1 ch, 176400 frames`, the WAV plays
through SDL3; at 1.5 s no cue, at 2.5 s cue 3 (1:1 = 99); both time displays count in hundredths (`00:00:02:53` in the
cue list, `00:00:02:54` in the player, `media-waited-*.png`); Stop while the list waits on its last line: the player reads
`00:00:00:00` (`media-stopped-TLJMediaForm.png`). Cue list 6 (MP Load File `clip.mp3` and `clip.avi`, our 10 / 12 bytes)
logs `cue list: Only WAV files can be played (LJ4EVR plays audio through SDL3, without codecs)` and `MP Load File failed`
for each. The sample-exact wait (frame 88200, not 88199) is `test_media`.

### Captures against the reference GIFs (viewed side by side; the GIFs stay in `reference/`)

| Window | Ours | LJ2 GIF | Differences seen |
|---|---|---|---|
| Audio Analyzer (TLJSoundApp) | `audio-timeline-TLJSoundApp.png` | `audiotimeline.gif`, `audiorealtime1.gif` | same layout, menus, gain bar and three faders; the device combo reads "Default device" (LJ2: Sound Mapper) |
| Hardware Setup, Audio page | `audio-hs-THardwareSetupForm2.png` | `setupaudio.gif` | the device combo under the Winamp row is ours (PROPOSAL rig W5-AUD-1); CD-Audio and Digital Audio Player greyed |
| Dmx In (TDmxInForm) | `dmxin-after-save-TDmxInForm.png` | `dmxindialog.gif` | same columns, captions and buttons (the GIF's arrows and labels are annotations) |
| DMX-In translation | `dmxin-translation-TDMXInValueTranslationForm.png` | `dmxintranslation.gif` | SYSTEM tables only (the MX-10 examples are not shipped); notes and graph panes as the GIF |
| RS232 Settings | `remote-settings-TRS232SettingsForm.png` | `rs232.gif` | a different window: the GIF is LJ_RS232.exe's status window, not reproduced (I10 row); ours is its settings form plus our Remote group |
| External Application Link | `extapp-link-TExternalAppSetupForm.png` | `ljexternalconfigsetup.gif` | same rows and controls; the DMX channel boxes and Auto-launch / Auto-close greyed (I11 row) |
| LJ MediaPlayer | `media-waited-TLJMediaForm.png` | `mediaplayer.gif` | ours has a caption bar and an empty black video area; Playback File greyed (docs/media.md) |
| Restore User Library | `ljb-imported-TLibraryRestoreForm.png` | `userlibrestore.gif` | as W4; `*.LJB` listed and the notes box carries the import report |

### Other changes of this pass

- `lj_uiscript.c`: `assert_text` reads a TComboBox, TSpinEdit and TTrackBar (`docs/ui-script.md`).
- `tests/model/test_ljb.c`: `test_ljb --write <dir>` writes the synthetic Show / Bad / Evil containers.
- Docs: README status, the "What v1.1.0 is not" paragraph (README and `docs/fidelity.md`, byte-equal; the v1.0.0
  paragraph is kept as history) and `tools/fidelity_check.py` comparing both paragraphs; `docs/fidelity.md` W5 rows
  (I4, I6, I7, I10, I11, F5 / `&Audio`, F8, F31, F48, F51, the by-id forms, TExternalAppSetupForm, Art-Net, the input
  merge, the master remote state, the status line); CHANGELOG `[Unreleased]` 1.1.0; `docs/user-guide.md`, help topics
  `not-available` (only what is still greyed) and `output` (Art-Net with the credit line), NOTICE and README with the
  Art-Net trade mark; `docs/dmx-in.md`, `docs/remote.md`, `docs/artnet.md` corrected; `tools/ref/README.md`.
- Not done here: `VERSION` 1.1.0, the tag and the release rows (W5-X-VERSION, ME-158 Mac run), design.md (vault, design
  owner), the `docs/file-formats.md` rows for `[DMXIN]` / `[AUDIO]` / `[REMOTE]` / `DmxIn.dat` / `LJBImport.txt` (each
  feature doc carries its keys), new help topics per feature, and the other "Not done here" items of integrator 1 (the
  Global Patch Use / Ignore DMX In Enable, Statics Latch through a pinned path, Pass Keypresses, the `.Udf` half of
  W5-LJB-1, ME-165 / ME-166).

### Builds and tests after the walkthrough fixes

    build-w5i-rel   (Release, SDL)        0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    build-w5i-nosdl (Release, LJ_SDL=OFF) 0 warnings, 0 errors   ctest 100% passed, 0 failed out of 151
    build-w5i-dbg   (Debug, SDL)          0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    fidelity_check                        73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)
    help_check                            16 texts against the reference 8-word runs, 15 topics (regenerated)
    the final walk (every step in one run) 13 of 13 app runs rc 0, 0 SCRIPT FAIL

One walkthrough run broke `test_remote_form` on the way: `assert_text` had first been taught to read a TRadioGroup's
ItemIndex, and that test asserts a radio group's caption; that change was dropped (the Audio page's state is asserted
through `Hardware.Ini` instead). Pre-commit: 74 paths, none under `reference/`, sha1 of each against the 13,637
distinct `reference/` hashes: 0 equal; the private-detail grep (user and host names, the Dropbox root, `Users\`, LAN
prefixes, audio device brand names) over the diff and the new files: 0 hits; the 36 captures viewed: loopback
addresses, the source name `LJ4EVR` and the `build-w5acc` scratch path only.
