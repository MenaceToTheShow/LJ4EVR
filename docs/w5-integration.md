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

## Fix pass r1 (W5 code council, `W5_CODE_MASTER_ELDER_VERDICT.md` section 5)

2026-10-23, on `8754390`. One commit, `W5: council fix pass r1`. Toolchain as above (MSVC cl 19.44 + Ninja, SDL from the
`release-3.4.18` source, which is commit `829a65d769d935c4852f8159e964312c0957260a`). Item by item, in the verdict's
order:

| # | ME | What landed | Check |
|---|---|---|---|
| 1 | ME-171 | (a) `slot listed twice` refused (`lj_statics.c`); (b) pass 1 already held the path-length and separator refusals (`lj_zip_check`), rows added; (c) `errno` / `ERANGE` / end / sign checks in `lj_statics.c` and `lj_gdc.c`, GDC through `unsigned long long` (and the W4 LORD_C-10 `strtoul` in `lj_da2532.c`); (d) `LJ_CL_LINES_MAX` 9999 PROPOSAL, at load and in `lj_cuelist_insert`; (e) `GIT_TAG 829a65d7...` (40 hex, `SYSTEM`, no `GIT_SHALLOW`), the CI / release SDL steps fetch that commit, `tools/fitfont.py` deleted (`docs/fonts.md`); (f) a held Wait Elapsed counts the next wait from its due time (PROPOSAL rig 03 s6.3); (g) Run with Automatic Cue update latches from the last non-transparent cue line (host `cue_transparent`, appended); W4 LORD_ENGINE-8 `held_flash` / `held_latch` `[bank][key]` with a key-only release finding its bank | `test_model_w2_records` (duplicate slot, `2^64`, `+5`), `test_gdc_grammar` (`2^32`, `2^64`, signs), `test_model_w2_library` (evil4 / evil5), `test_cuelist` (10,000 lines refused, insert stops at 9999), `test_cuelist_run` (three 1 s waits on a 15.6 ms wake fire at 1014 / 2012 / 3010 ms, was 3042 for the third; Run 6 latches 3, 104, 105, 6), `test_da2532_fingers` (bank A and B key 1 flashed, each released once) |
| 2 | ME-173 | (i) one `LJ_NA_HINT` in `core/lj_types.h`, the nine private defines and the inline literals gone; (ii) `lj_widget_set_enabled(w, on)` (`ui/lj_widget.c`) greys with the hint and clears it on un-grey; every runtime grey / un-grey routes through it (the DMX-in inputs keep a DFM hint, as before); (iii) `lj_w_edit_int(w, lo, hi, dflt)` landed: 3 copies measured (`lj_config_dialogs.c`, `lj_show_dialogs.c`, `lj_statics2_prefs.c`), time-code offsets signed; `lj_window_popup_at` closed in writing: 2 copies (`lj_fingers.c`, `lj_gdc_form.c`); (iv) `lj_app_w4.c` folded and `lj_app_w3.c` renamed: `src/app/lj_app_cuelist_host.c` / `.h` | `git grep -c "Not available in LJ4EVR" -- src ':!src/windows/lj_help_topics_gen.c'` = `src/core/lj_types.h:1`; `test_layering` (rows 11: the text outside its define, a `->hint = LJ_NA_HINT / NULL` write outside the helper, a private `edit_int`, a wave-named seam file, a third `popup_at`): `463 file(s) checked, 221 header(s) compiled alone, ... hint/edit_int/seam copies 0, popup_at 2, 0 finding(s)` |
| 3 | ME-167 | `lj_dmxin_functions_step`: the functions run only while the frame is live (a source or a Fingers emulation), on a change, and once when `functions_off` changes; `lj_dmxin_open` resets the edge state | `dmxin_functions` (no packet: nothing fired; off / on: the same frame closes the gate again; Disable DMX-in functions on a static frame opens it, enabling closes it, an unchanged frame fires nothing); `test_uiscript_w5` (the LORD_ENGINE-1 repro at the app seam: saved `master_intensity`, DMX-in off, five idle passes: 0 Grand Master calls) |
| 4 | ME-168 | `lj_remote_set_setter`: 132 1 x, 135 and 136 post the set form (`lj_smoke_control_timer`, `lj_cue_form_set_slot`, `lj_bgcue_form_set_slot`), never a toggle against the view | `test_remote` (each command's frame twice in one feed: two set calls, no toggle function; `135 1 0 135 1 0` leaves slot 1 off); `test_remote_form` (`remote 132 1 0 132 1 0` through the script verb: the timer off, the toggle never run) |
| 5 | ME-169 | `lj_fs_media_path` (`core/lj_fsinfo`): refuses two leading separators, a `:` other than a drive letter's, a device stem (CON PRN AUX NUL COM0-9 LPT0-9 and the superscript digits, CONIN$ CONOUT$) or a non-`.wav` name before any file call, then resolves a relative name against the library root; used by the loader, the editor's re-check, `lj_media_check_file` and the app's MP Load File | `test_cuelist`: `//203.0.113.7/s/a.wav`, `CON.wav`, `clip.avi` load red with `lj_fs_exists` called 0 times (counter); `tone.wav` beside `test_cl_lib/` loads with the working folder elsewhere (1 call); the helper's 12 direct rows |
| 6 | ME-172 | `.LLF`: `frames > data_n` refused before the malloc; `DmxIn.dat`: a function number outside its own row range (groups 1..24, statics 1..20, slots 1..12, record numbers 1..99999999, 0 for the rest) refuses the file | `test_llf` (41-byte file claiming 4,000,000 x 64: "do not fit"); `dmxin_functions` (`htp_group 16777219`, 25, 0, `static_activate 21`, `seq_onoff 13`, `master_intensity 1`, `flash_seq 0`, `3x` refused; 24 / 0 / 99999999 taken) |
| 7 | ME-176 | ArtPollReply accepted only from the bound interface's subnet (the mask kept from `lj_nic`; none = refused, a loopback alias = 127/8); at most `LJ_ARTNET_ROWS_PER_IP` 4 rows per address (PROPOSAL rig W5-ART-2); the Art-Net interval constants are `LJ_SACN_MIN_INTERVAL_US` / `LJ_SACN_KEEPALIVE_US` | `test_artnet` (8.8.8.8 not subscribed; host .3's fifth BindIndex refused: 2 + 4 rows) |
| 8 | ME-177 | DMX-in drops a datagram from off the `[DMXIN] Nic` subnet (the socket path and the posted Art-Net path in `lj_dmxin_poll`; unknown mask = /24, PROPOSAL); `lj_artnet_feed` drops Art-Net DMX-in from off its own subnet | `dmxin_functions` (203.0.113.9 discarded, the frame not live; 127.0.0.3 taken) |
| 9 | ME-178 | `[ARTNET] In` neither saved nor read; `artnet_in` set only by the DMX-in apply; the extra save in `lj_app_io.c` deleted | `test_artnet` (an old `In=10` loads as 0); `test_uiscript_w5` (`[ARTNET] In=10`, no `[DMXIN]`: `artnet_in` 0, `lj_artnet_open` binds nothing, `bound` 0) |
| 10 | ME-180 | measured (scratch program, Release SDL3, synthetic 16-bit stereo 44.1 kHz WAV in the OS cache): `SDL_LoadWAV` 16 MB 7 ms, 32 MB 11 ms, 64 MB 19 ms, 128 MB 31 ms, 256 MB 59 ms; budget 250 ms (under the ~370 ms capture ring) with a cold read at an assumed 400 MB/s: `LJ_MEDIA_FILE_MAX` 64 MB (PROPOSAL); the "1 = still waiting" contract text deleted; `docs/media.md` durations (64 MB = about 6 min of 44.1 / 16 / stereo, 3.9 min of 48 / 24 / stereo) | `test_media` (64 MB taken, 64 MB + 1 refused with the size message) |
| 11 | ME-181 | `M_Audio` rule 3 (shown while the analyzer is enabled), re-synced by the Audio apply; the `MAINWINDOW` golden is the `v1.0.0` file again (`git diff v1.0.0 -- tests/ui/golden/MAINWINDOW.bmp` empty); `on_tc` no longer pre-sets the clicked button; the `.LJB` Restore labels show the new library and its folder | `w5_entry_points.lj4s` (`assert_visible main.M_Audio 0`; `click sbMPTime` -> down 1; `click sbPCClock` -> MP down 0, PC Clock down 1); `test_audio_form` (hidden, shown when enabled, hidden again); re-shot `ljb-imported-TLibraryRestoreForm.png` |
| 12 | ME-182 | `lj_app_key_passes`: a key with the Analyzer (`PassKeys`) or the MediaPlayer (`[LJMEDIAPLAYER] KeyPass`) focused and its flag 0 is dropped, in `lj_app_key` and the `hotkey` verb | `test_uiscript_w2`: per window, flag 0 = the F1 hotkey does nothing, flag 1 = Cue 1 |
| 13 | ME-183 | glyphmap row `TCUELIST2FORM sbMPTime media` (our `art/glyphs/media.txt`, the ME-48 "row listed, blob absent" path), `dfm2c.py` regenerated; the four other blank buttons get one fidelity row; the source comment corrected | goldens `WIN_CUELIST_FORM` and `TCUELIST2FORM` (164 px each, the glyph only) with REVIEWED rows; `docs/screenshots/w5/media-tc-TCuelist2Form.png` |
| 14 | ME-174 | ME-160 vsIcon view (`lj_w_listview_icon_view`, 75 x 75 PROPOSAL); ME-163 Date column (folder mtime); ME-164 LightCorder tap on `tick_no`; ME-161 seven REVIEWED rows (no detail fixed: the caption-button 3DLight inner line differs from `movement2.gif` and is carried, the X matches; no 1:1 GIF shows a popup, a focus rectangle or a tab control; the two placement rows carry to the rig); ME-162 closed in writing (W4 LORD_FIDELITY's 80-of-80 probe); every Status cell of `docs/w5-gates.md` filled | `test_config_dialogs` (tile view, golden `W4C_BITMAP_PICKER` re-rendered, 11013 px inside LV1); `test_file_dialogs` (Date `dd-mm-yyyy hh:mm`); `test_llf` (100 ticks at 20 Hz, three UI passes per tick at one wall time: 50 samples); re-shot `E/bitmap-picker-TBitMapPickerForm2.png`, `E/user-libraries-TUserLibForm.png` |
| 15 | ME-175 | `tests/ui/scripts/w5_entry_points.lj4s`, run headless by `test_uiscript_w3` (CTest) after `lj_app_io_start` on a fresh Hardware.Ini; `assert_enabled` / `assert_visible` read menu items, `assert_down` reads a speed button | green; a planted `assert_enabled TCuelist2Form.MPSeek1 0` fails it (`SCRIPT FAIL line 22: TCuelist2Form.MPSeek1 is "1", expected "0"`, `test_uiscript_w3: 1 failure(s)`) |
| 16 | ME-179 | `replay_dump` reads `tests/engine/replay/w5/*.lj4r` too; re-recorded once, after items 3 and 4 | `replay-frames.bin` `c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d` (129,664,080 bytes, W4) -> `9da72c3f56a0b728c1e0adfd006d51f68b7fc787e1d00c03045db27f18560adb` (129,914,872 bytes) in Release, Debug and no-SDL; the W4 scenarios alone still give `c46fee4c...` (items 1 / 3 / 4 change no replayed frame); the dump-frames file stays `5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50`. The MP-wait half is `test_media` (the sample-exact wait), as the verdict allows |
| 17 | ME-186 | `lj4evr_tests <fuzz row> --seed <dir>` writes the row's proven seed (`tests/lj_fuzz.h` `lj_fuzz_seed`); the CI fuzz loop passes `corp-<target>`; `test_dmxin.c`'s Art-Net builder check is a `DCHECK`; the PENDING texts and "pending" prose are gone | seeds: `sacn_data` 638 B, `artdmx` + `artpoll` + `artpollreply` 791 B, `frames` 6 B, `table.vtr` 148 B, `show.LJB` 265 B; `grep -c PENDING tests/io/test_dmxin.c` = 0; the CI run's INITED lines are the `corp` > 1 evidence |
| 18 | ME-170 | `SECURITY.md`: in-scope list with the W5 files, packets and the five fuzz targets, the MP path refusal, a "Network listeners" section (5568, 6454, 7232, serial, the External Application client) with the ME-176 / ME-177 filters and the no-authentication sentence; `docs/running.md` "Network listeners (W5)" table and the sentence | `grep -c "no authentication"`: `SECURITY.md` 2, `docs/running.md` 1; "opens no listening socket" 0 |
| 19 | ME-184 | fidelity rows "W5 fix pass r1" (RS-232 / TCP Remote menu item, `&Audio`, vsIcon, Date, the seven frame details, `sbMPTime` and the four blank buttons, the Restore labels, Pass Keypresses); the `.Udf` bullet dropped from README and `docs/fidelity.md` together; the `.Udf` half of W5-LJB-1 closed in `w5-gates.md` | `fidelity_check: 73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)` |
| 20 | ME-185 | help topics `input.txt` (DMX-in, the remote with its command list and the no-authentication sentence, External Application), `audio-media.txt`, `import.txt`, `index.txt`; `lj_help_topics_gen.c` regenerated; `file-formats.md` rows (`[ARTNET]`, `[DMXIN]`, `[AUDIO]`, `[REMOTE]`, `[EXTAPP]`, `Main.Ini [CUELIST2]`, `forms.ini [CUELIST2]` / `[LJMEDIAPLAYER]`, `DmxIn.dat`, `.vtr`, `LJBImport.txt`; `[MIDI] device (W5)` and `[ARTNET] In` removed); three rows each in `provenance.md` and `generated-files.md`; CHANGELOG 1.1.0 fix-pass list and Known gaps (the macOS microphone prompt included); `sacn.md`, `audio.md`, `artnet.md`, `media.md`, `lj_audio.h`, `lj_media.h` texts; `node` / `nodes` and the bind failure from `artnet_last_err`; `build-*.err` in `.gitignore`; `docs/ui-script.md` for the new verb | `help_check: 19 texts against 200060 reference 8-word runs`, `18 topics`; golden `W4HELP_VIEWER` re-rendered (the three topics in the list) with its REVIEWED row; `grep -c "MIDI\] device (W5)" docs/file-formats.md` = 0; `test_artnet` (`cannot bind UDP 6454 (error 10048)`, `not open yet`, `1 node`) |
| 21 | ME-187 | not this pass: design.md is the orchestrator's (vault) | - |
| 22 | ME-188 | not this pass: the release sequence runs after the verifier ticks lines 1-20 | - |

Files changed: docs `SECURITY.md`, `CHANGELOG.md`, `README.md` (the `.Udf` bullet), `docs/{running,file-formats,provenance,generated-files,fidelity,fonts,media,artnet,audio,sacn,ui-script,w5-gates}.md`; help `index.txt` + three new topics.

ME-175, the W5 entry points live after the pass (the before list is the same: no track had greyed them again), asserted by
`w5_entry_points.lj4s`: main `M_RestartSound`, `M_RemoteSetup`; Cuelist `sbMPTime`, `Pop_MPCommands` and its ten items
(`CueMediaFile1`, `MediaPlayerPlay1`, `MPSeek1`, `MediaPlayerStop1`, `MediaPlayerPause1`, `WaitMediaPlayer1`,
`MPCloseClip1`, `MediaPlayerLaunch1`, `MPClose1`, `MediaPlayer1`); Hardware Setup `cbUseDMXin`, `RGAudio`,
`cbAudioDevice`; Dmx In `LVFunctions`, `LVDmxFunctions`; RS232 Settings `rgPort`, `rgBaudRate`; MediaPlayer
`sbLoadMediaFile`. Greyed and asserted greyed: `M_StartDAPlayer`, `sbCDTime`, `sbDATime`, `Pop_CDCommands`,
`popCDPlayTrack`, `pop_DACommands`, `P_PlayDAFile`, Hardware Setup `rgAudioCD`, `rgDigitalAudio`; hidden: `M_Audio` on a
fresh userdata. `PI_DefineAppLink` (an External Application icon's popup) stays covered by `test_extapp_form`.

### Measured on the fix-pass tree

    build-w5i-rel   (Release, SDL)        0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    build-w5i-dbg   (Debug, SDL)          0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    build-w5i-nosdl (Release, LJ_SDL=OFF) 0 warnings, 0 errors   ctest 100% passed, 0 failed out of 151
    build-w5i-fuzz  (Debug, OFF, MSVC /fsanitize=address)        ctest 100% passed, 0 failed out of 151;
                                          fuzz_sacn_rx / fuzz_artnet / fuzz_remote / fuzz_vtr / fuzz_ljb: "20000 iterations, PASS", no ASan report
    test_layering                         see item 2
    dfm2c --check                         OK after the regeneration (glyphmap row)
    help_check / fidelity_check           see items 19 and 20
    goldens changed                       MAINWINDOW (back to v1.0.0), WIN_CUELIST_FORM, TCUELIST2FORM, W4C_BITMAP_PICKER,
                                          W4HELP_VIEWER: each with a 2026-10-23 "W5 fix pass r1" REVIEWED row
    captures (scratch userdata `build-fp1/ud`, `LJ4EVR_USERDATA`): E/bitmap-picker-TBitMapPickerForm2.png,
                                          E/user-libraries-TUserLibForm.png, w5/ljb-imported-TLibraryRestoreForm.png,
                                          w5/media-tc-TCuelist2Form.png (new); viewed: the scratch tree path only

Pre-commit: 135 staged paths, none under `reference/`; sha1 of each against the 13,637 `reference/` hashes: 0 equal; the
ME-148 four-pattern grep of the W4 verdict (LAN prefix, runner login, work-PC name prefix, NIC name) and the user-path grep over the index: 0 and 0.
Not run on this PC: gcc and clang (the CI jobs); macOS.

### Fix pass r1: CI (`79b8d18`)

https://github.com/UnbannableGT/LJ4EVR/actions/runs/40014969020: six of six jobs green on the first run (generate-check,
linux-gcc-asan, linux-clang, windows-msvc, macos-clang, determinism). determinism: `frames-{linux,windows,macos}.bin`
all `5b13f419...63114b50` (unchanged); `replay-frames.bin` all `9da72c3f...560adb` (the ME-179 re-record, equal to the
local trees). ME-186, the fuzz step's INITED lines now start from the seeds: `fuzz_sacn_rx corp: 1/638b`,
`fuzz_artnet corp: 3/791b`, `fuzz_remote corp: 1/6b`, `fuzz_vtr corp: 1/148b`, `fuzz_ljb corp: 1/265b` (each was
`corp: 1/1b`); `fuzz_udf` keeps its `udf-corpus` (`1/222b`). The SDL steps fetched commit `829a65d7...` (ME-171 (e)).

## Release

### v1.1.0 (ME-188)

`Release 1.1.0` (`0f6e97f`: `VERSION` 1.1.0, `CHANGELOG [1.1.0]`, the README status line), CI run 40017600733: six of six
green. `git tag -a v1.1.0 -m "LJ4EVR 1.1.0"` on `0f6e97f`, pushed 2026-10-23. `release.yml` run 40018750013: check,
windows, macos, release all success.

    gh release view v1.1.0      "LJ4EVR 1.1.0", isPrerelease false, isDraft false, published 2026-10-23T20:25:58Z;
                                repos/.../releases/latest = v1.1.0
    assets                      LJ4EVR-1.1.0-win64-setup.exe   1,939,248 bytes
                                LJ4EVR-1.1.0-macos.dmg         5,138,245 bytes
                                SHA256SUMS                       184 bytes
    SHA256SUMS                  fb4caa2088e87853096f1ef5c82f0bb3df87b2ceb660e75be9b8a35e44d74e11  LJ4EVR-1.1.0-win64-setup.exe
                                12be5df5c42315b1cf37c11f817d4e1189ec80112d7c1554feeefd8f1c2aaea4  LJ4EVR-1.1.0-macos.dmg
                                (downloaded, sha256sum -c: both OK)
    notes                       the install / notices / trademark header, then CHANGELOG [1.1.0] up to "Developer detail:"
                                (the seven features, the review fixes, what 1.1.0 is not, the known gaps incl. the
                                macOS microphone-prompt line of ME-189)

Windows installed tree (the downloaded release asset, work PC, silent per-user install into a scratch folder:
`LJ4EVR-1.1.0-win64-setup.exe /S /D=<scratch>\inst`, exit 0):

    lj4evr.exe                12,057,088
    THIRD_PARTY_NOTICES.txt       15,645
    Uninstall.exe                111,892
    help\  audio-media controls cuelists cues desktop files fixtures getting-started hotkeys-midi import index input
           macros not-available notices output sequences statics-bgcues  (.txt, 18 topics)
    HKCU ...\Uninstall\LJ4EVR   DisplayVersion=1.1.0, UninstallString = <scratch>\inst\Uninstall.exe; no HKLM key
    help_check.py --install-tree         install_check: ...\THIRD_PARTY_NOTICES.txt ok (rc 0)
    lj4evr.exe --version                 LJ4EVR 1.1.0 (rc 0)
    w4_entry_points.lj4s against the installed exe (scratch LJ4EVR_USERDATA prepared by w4_entry_prep.sh plus a
      W4Captions row; shots to a scratch folder)   rc 0, 155 PASS lines, 0 SCRIPT FAIL
    w5_acc_media.lj4s against the installed exe (scratch LJ4EVR_USERDATA from `w5_wire.py prep`, U:W5Acc)
                                         rc 0, 0 SCRIPT FAIL; `media: ...\User\W5Acc\tone.wav loaded, 44100 Hz, 1 ch,
                                         176400 frames`; clip.mp3 / clip.avi: `not a local .wav file, kept and skipped`
    Uninstall.exe /S                     exit 0; the folder, lj4evr.exe and the HKCU key gone

Two walkthrough inputs were stale against the 1.1.0 behaviour and are corrected in this record's commit (no product code
changed):

- `w4_entry_points.lj4s` line 728 asserted the Dmx In window's `bAssign` greyed (the 1.0.0 state); W5 made the DMX-in
  inputs live, so the row now asserts `1`. The first run against the installed copy failed there (122 PASS, `SCRIPT FAIL
  line 728: TDmxInForm.bAssign is "1", expected "0"`). The script is not a CTest row; `w5_entry_points.lj4s` (CTest)
  already asserts the live DMX-in entries.
- `tools/ref/w5_wire.py prep` wrote `tone.wav` to the userdir root; since ME-169 a relative MP Load File name resolves
  against the library folder, so `w5_acc_media.lj4s` saw `tone.wav: file not found`. prep now writes it into
  `User\W5Acc`.

macOS (from the `release.yml` macos job log; no hand run, ME-189):

    lipo        Architectures in the fat file: ... lj4evr are: x86_64 arm64
    codesign    Format=app bundle with Mach-O universal (x86_64 arm64), Signature=adhoc; --verify --strict:
                valid on disk, satisfies its Designated Requirement
    Info.plist  CFBundleShortVersionString 1.1.0, LSMinimumSystemVersion 12.0; minos 12.0
    .dmg        5,138,245 bytes, UDIF trailer at size - 512 = `koly` (6b 6f 6c 79); hdiutil verify: every partition
                verified; mounted: Applications -> /Applications, LJ4EVR.app; install_check:
                .../LJ4EVR.app/Contents/Resources/THIRD_PARTY_NOTICES.txt ok

Windows release job: `dumpbin /dependents` with no vcruntime / msvcp / ucrtbase; installer VERSIONINFO ProductName=LJ4EVR
ProductVersion=1.1.0 FileVersion=1.1.0; install_check on the silent CI install ok.

ME-188 ticked by this record. Open after the tag: ME-187 (design.md v1.1.0 paragraph, orchestrator) and the section 6
carries (the reference rig, the Mac hand run, the OemCode).
