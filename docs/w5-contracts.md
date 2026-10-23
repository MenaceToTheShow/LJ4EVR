ME-165 and ME-166 (the two FORK rows of `docs/w5-gates.md`) have NOT landed: no commit closes them on `main` @ `ab022ad`, and this contracts commit is the first after it. The integrator lands them, or closes each row in writing in `docs/w5-gates.md`, before any W5 track forks; until ME-165 lands no track un-greys a widget (section 5, point 1).

# W5 contracts: the seams seven tracks code against

Written 2026-10-23 by the W5 contracts seat. Design 1 (the sacrifice list), 2.2-2.5, 8 and 10 W5 are the scope;
`docs/w5-gates.md` is the gate list this page serves (committed with it). Every new header compiles alone
(`tools/test_layering.py` with cl: 0 findings, the include graph and the appended OS_ALLOW rows), `LJ_SDL` ON and OFF
build with 0 warnings (Ninja + cl 19.44, Release, `build-w4` and `build-w4-nosdl`), and ctest is green on every existing
row; the five parser fuzz rows are expected-pending (DISABLED, track named). Shared files stay append-only for every
track (the W4 list plus `src/windows/lj_hardware_setup.c` and `src/desktop/lj_main_window.c`, section 3).

## 0. What landed in this commit (beyond stubs)

| Item | Where | Check |
|---|---|---|
| Engine: `LJ_CMD_DMX_IN`, `LJ_CMD_OVERWRITE_ENABLE`, `LJ_TRIG_AUDIO_EVENT` (Audio-Trigs / Trig counted per runner) | `src/engine/lj_engine.h`, `.c` | `test_engine_w5`, `replay_w5_contract` (10 expects) |
| The socket seam: non-blocking IPv4 UDP / TCP, one OS_ALLOW file | `src/io/lj_socket.h`, `.c` | `test_socket` (loopback UDP both ways, truncation, TCP connect / accept / send / recv / peer close) |
| The app seam: W5 records in `lj_ctx`, cfg loads, open-only-when-enabled, the 10 ms pass, shutdown before the engine stops, Emulate Audio Trig and the three DMXin Functions hotkeys bound | `src/app/lj_app_io.h`, `.c`; calls appended in `lj_app.c` | `test_uiscript_w5` (W5-X-OFF on a fresh Hardware.Ini, Emulate Audio Trig advances an audio slot) |
| Engine-thread Art-Net tap: per-link protocol, `lj_artnet_open / set / pump / close` beside sACN | `src/platform/sdl/lj_sdl.c` | stub no-ops; every link stays sACN until the artnet track lands |
| UI script verbs `sacn_in`, `artnet_in`, `audio_inject`, `remote` (load-time checks + execution through the wire entry points) | `src/app/lj_uiscript.c`, `docs/ui-script.md` "W5 verbs" | `test_uiscript_w5` |
| Replay lines `dmx_in`, `overwrite_enable`, `audio_trig` | `tests/engine/lj_replay.c`, `docs/replay-grammar.md` "W5 lines" | `replay_w5_contract`; `replay_dump` unchanged (the scenario sits in `tests/engine/replay/w5/`, outside the glob) |
| Fuzz harness: one seeded mutator, five rows that first prove a synthetic seed parses | `tests/lj_fuzz.h`, `tests/io/test_dmxin.c`, `test_artnet.c`, `test_remote.c`, `tests/model/test_ljb.c` | rows print `PENDING <track>` today; DISABLED in `CMakeLists.txt` |
| Four window ids and four W5 families | `src/windows/lj_windows.h`, `.c`; `lj_{audio,remote,extapp,media}_form.c/.h` | ids log "has no behaviour file yet" until the track lands (ME-131 counts +4) |

## 1. The seams

1. **Output record and Art-Net (track artnet).** One output record, `lj_sacn_cfg`, gains `proto[LJ_LINKS]`
   (`enum lj_out_proto`: `LJ_OUT_SACN` 0 / `LJ_OUT_ARTNET`), `artnet_addr[LJ_LINKS]` (15-bit Port-Address, default link
   N -> N) and `artnet_in` (the Art-Net input Port-Address, 0 = off), persisted as Hardware.Ini `[ARTNET]`
   (`lj_artnet_cfg_load / save`, called beside `lj_sacn_cfg_*`; `lj_prefs_sacn` already calls the load). The existing
   `LJ_CMD_SACN_CFG` carries both protocols, so only the engine thread touches sender state (ME-23): `lj_sdl.c` calls
   `lj_artnet_open(&cfg)` after `lj_sacn_open`, hands `frame->out[link]` to `lj_artnet_set(link, ...)` for an Art-Net link
   and to `lj_sacn_set` otherwise, then `lj_sacn_pump` and `lj_artnet_pump`, and `lj_artnet_close` on exit. The track
   makes `lj_sacn_open` treat an Art-Net link as unmapped (terminating its sACN universe), owns the Art-Net socket on the
   engine thread (bound to the NIC:6454 with SO_BROADCAST, opened only while a link or `artnet_in` needs it), the
   subscriber table, ArtPoll / ArtPollReply / ArtDmx (`io/lj_artnet.h` header comment: the note 08 s9 rules), and hands
   every ArtDmx for `artnet_in` not sent by us to `lj_dmxin_post(LJ_DMXIN_ARTNET, ...)`. The pump today runs only while
   sACN opened (`have_sacn`): the track decouples that one line in `lj_sdl.c`. Status: `lj_artnet_stats` (packets/s,
   nodes, bound) reaches the UI the way `sacn_pps` does; the track appends the view fields and the setter to
   `engine/lj_engine.h` (`lj_engine_set_artnet_stats`, the `lj_engine_set_sacn_stats` pattern) in its own commit.
2. **DMX-in (track dmxin).** One input seam for both protocols (`io/lj_dmxin.h`): the UI thread owns the sACN input
   socket (0.0.0.0:5568, SO_REUSEADDR, joined to 239.255.hi.lo on the chosen NIC, unicast accepted) and drains it with a
   budget in `lj_dmxin_poll`; the engine thread posts Art-Net packets with `lj_dmxin_post` (a bounded copy under
   `lj_lock`). Every packet goes through `lj_dmxin_rx` on the UI thread: validation (`lj_sacn_rx_parse`, E1.31 5.1-5.5,
   6.2.3, 6.2.6, 6.7, 9.1.1), per-source filing, priority then equal-priority HTP over `LJ_DMXIN_SOURCES` (PROPOSAL),
   time-out 2.5 s or Stream_Terminated, our own CID ignored, the loss rule (`LJ_DMXIN_LOSS_HOLD` / `_ZERO`, PROPOSAL),
   the per-channel `.vtr` tables from `raw[]` to `out[]`. When `lj_dmxin_poll` returns 1 the app copies `out[]` into its
   own buffer and posts **`LJ_CMD_DMX_IN`** (a = input link 1, `dmx_in` = that buffer) in one critical section; the engine
   copies 512 bytes into `post.dmx_in[]` at the drain (W5-X-ENGINE). **`LJ_CMD_OVERWRITE_ENABLE`** (b 0/1) is the S1 gate:
   the app posts `dmxin.enabled` at open (the v1.0.0 behaviour with DMX-in off); the Overwrite Enable function and the
   "1 while unassigned" rule (05 s3.2) are the track's. Then `lj_dmxin_functions(map, out, functions_off)` runs the
   assigned functions on the UI thread through `lj_fn_run` / `lj_fn_run_prog` and the `LJ_FNF_EXT` level rows the track
   appends to `io/lj_actions.h`; the map in `ctx->dmxin_map` is the SAVED one (05 s3.1). Files: Hardware.Ini `[DMXIN]`,
   `<library>/DmxIn/DmxIn.dat` (PROPOSAL name, design 5.2 grammar), `<userdata>/User/ValueTables/*.vtr`
   (`lj_vtr_parse / load / save / file_name / linear`). The function words are one X-macro, `LJ_DMXIN_FN_LIST`, with the
   enum `lj_dmxin_fn` and `lj_dmxin_fn_words` (contract data, kept). Fingers DMX-in channels 1-48 feed
   `lj_dmxin_emulate` as one more source.
3. **Sockets (contracts, complete).** `io/lj_socket.h`: `lj_socket_udp / join / recvfrom / sendto / listen / accept /
   connect / connected / recv / send / port / close / error`, every handle non-blocking, every bind logged with its
   address and port. `recvfrom` returns `cap` for a longer datagram (truncated): a receiver sizes its buffer one byte past
   the largest valid packet and refuses a full one. `LJ_IP_LOOPBACK` is the default bind of every listener. A handle
   belongs to one thread (Art-Net: the engine thread; sACN input, remote, External Application: the UI thread).
   `lj_sacn.c` keeps its own sender socket.
4. **Audio (track audio).** Capture is `platform/sdl/lj_sdl_audio.c` (`lj_sdl_audio_list / open / close`: one SDL3
   recording stream, `LJ_AUDIO_RATE` mono S16) whose callback only calls `lj_audio_post` (a bounded copy into a ring
   under `lj_lock`). The analyzer (`io/lj_audio.h`) is SDL-free and integer only: `lj_audio_analyze` on whole
   `LJ_AUDIO_BLOCK`s (level, clip, 16 bands from an integer FFT whose tables `tools/gen_vectors.py` generates) then
   `lj_audio_detect` (the level-ratio onset rule). The app drains with `lj_audio_poll` in at most 8 passes per wake and
   posts **`LJ_CMD_TRIG a = LJ_RUN_ALL, b = LJ_TRIG_AUDIO_EVENT`** per trig, the command Emulate Audio Trig
   (`LJ_FN_EMULATE_AUDIO_TRIG`, bound in `lj_app_io.c`) and `LJ_ACT_AUDIO_TRIG` already post. The engine (committed)
   counts `audio_left` down from `audio_per_trig` on every cue-slot and BG runner whose trig mode is `LJ_TRIG_AUDIO` and
   trigs it in its programmed direction at 0. Settings in `ctx->audio` (Hardware.Ini `[AUDIO]` + the analyzer keys),
   state in `ctx->audio_state`; the DMX-in Gain / Noise Threshold functions and the Fingers jogs write `ctx->audio`
   (W5-AUD-6). The capture device list reaches the Hardware Setup Audio page through `ctx->audio_list`.
5. **Remote and serial (track remote).** `io/lj_remote.h`: the pure 3-byte parser (`lj_remote_feed`, 0.2 s frame window,
   commands 128-136 of 05 s10.1) and `lj_remote_exec` (UI thread, `lj_fn_run` and the loaders); transports opened by
   `lj_remote_open(ctx->remote)` and polled by `lj_remote_poll` with a byte budget: TCP on 127.0.0.1:`LJ_REMOTE_TCP_PORT`
   (7232, PROPOSAL) unless the user picks an interface, at most `LJ_REMOTE_CONNS` connections; serial through
   `io/lj_serial.h` (8N1, non-blocking; `lj_serial_win.c` / `lj_serial_posix.c`, one compiled per OS, both on OS_ALLOW;
   the Linux CI row drives a pty). `lj_remote_inject` is the test and script seam. Hardware.Ini `[REMOTE]`. The serial
   files are shared with External Application output (the extapp track calls them, never edits them).
6. **External Application (track extapp).** The per-scene string is `lj_scene.ext[LJ_SCENE_EXT]` (`fixture` 1..100 +
   `cmd[33]`, `model/lj_model.h`, appended; the `ext` word of the scene record, written by the track in
   `model/lj_sequence.c`, absent = none, so v1.0.0 files load unchanged). The engine copies, at the end of each tick, the
   topmost running scene's string per profile-90 fixture into `lj_frame.ext_fixture[] / ext[][]` (appended, zero
   today); the track writes that copy in `engine/lj_engine.c` (plain copy: no I/O, no allocation, ME-2). The UI loop
   copies the two fields under the lock and calls `lj_extapp_step(fx, &ext[0][0], now)`, which encodes
   (`lj_extapp_encode`: ASCII 32..127, `#123`, `#X7F`, `!`, 32 characters) and sends on change over serial or a TCP
   client (`io/lj_socket.h`), failures counted, never fatal. Settings Hardware.Ini `[EXTAPP]`, `ctx->extapp`. Entry:
   `PI_DefineAppLink` runs `LJ_ACT_EXTAPP_SETUP` (arg = fixture) -> `lj_extapp_form_target` + `LJ_WIN_EXTAPP_SETUP`.
   The profile-90 control window is the track's (a new `controls/` file and the profile's window entry).
7. **MediaPlayer (track media).** `io/lj_media.h` is the SDL-free state machine and clip clock (1/100 s from sample
   frames consumed, never the wall clock); `platform/sdl/lj_sdl_media.c` fills `lj_media_host` (`SDL_LoadWAV`, one
   playback stream; WAV only, video never, `LJ_MEDIA_ERR_FORMAT` / `_SIZE`). `lj_cl_host` gains **`media(ud, line,
   now_us)`** (appended; the app's is `lj_app_io_cl_media`); the track takes the MP family out of `lj_cl_cmd_inert` and
   `lj_cuelist_load`'s red marking (`model/lj_cuelist.c`), sends MP lines to `h->media` in `app/lj_cuelist_run.c`, and
   makes the TCUELIST2FORM MP rows, `ListPop` items and `sbMPTime` live (`windows/lj_cuelist_form.c`). The MP time code
   reaches the runner through `lj_cl_host.timecode` (`LJ_TC_MP` -> `lj_app_io_mp_time`, committed). CD and DA stay
   inert (ME-11).
8. **.LJB (track ljb).** `model/lj_ljb.h`: `lj_ljb_open` (the container checks of W5-LJB-1, CRC first), `lj_ljb_extract`
   (tinfl capped at `usz`, CRC-checked), `lj_ljb_path_ok` (W5-LJB-2), `lj_ljb_import` (a NEW library folder, the
   W5-LJB-3 conversions, `lj_ljb_report`), error texts `LJ_LJB_ERR_*`. Entry point: the Restore window accepts `*.LJB`
   beside `.lj4b` (PROPOSAL, `windows/lj_backup_restore.c`, the ljb track's block). `tests/model/test_ljb.c` holds the
   synthetic container writer (`ljb_build`) every ljb row reuses.
9. **Window ids and families.** `enum lj_win` gains `LJ_WIN_AUDIO_ANALYZER` (TLJSoundApp), `LJ_WIN_REMOTE_SETUP`
   (TRS232SettingsForm), `LJ_WIN_EXTAPP_SETUP` (TExternalAppSetupForm) and `LJ_WIN_MEDIA_PLAYER` (TLJMediaForm); ids
   `audio_analyzer`, `remote_setup`, `extapp_setup`, `media_player`. The side-exe classes are the TPF0 form names probed in
   `reference/installer/extracted_2.209/app/` (LJSound.exe: TLJSoundApp, TAboutForm; LJMediaPlayer.exe: TLJMediaForm,
   TAboutForm; LJ_RS232.exe: TLJRS232Main, TRS232SettingsForm): class names only, the W5-X-HARVEST row generates or
   code-builds the forms. The DMX-in windows keep their W4 ids (family DMXIN, now the dmxin track's). One behaviour file
   per family (`windows/lj_audio_form.c`, `lj_remote_form.c`, `lj_extapp_form.c`, `lj_media_form.c`), dispatched by
   `lj_windows.c` exactly as the W4 families; a track replaces its stub bodies and adds below its header's contract block.
10. **The app seam (`app/lj_app_io.c`, contracts, complete).** Owns the W5 records `lj_ctx` points at (`dmxin`,
    `dmxin_map`, `audio`, `audio_state`, `remote`, `extapp`, `media`, `audio_list`), loads them in `lj_app_io_start`
    (after `lj_app_w4_start`), opens only what Hardware.Ini enables, runs `lj_app_io_idle` on every pass after
    `lj_app_w4_idle` (DMX-in -> `LJ_CMD_DMX_IN` + functions, audio -> trigs, remote, External Application from the frame
    copy, MediaPlayer step) and `lj_app_io_stop` before `lj_platform_engine_stop` (W5-X-THREAD: listeners close before the
    engine; the sACN termination order is unchanged). Desktop actions (appended to `lj_desktop.h`, bound here):
    `LJ_ACT_IO_APPLY` (arg `enum lj_io_part`: a settings window changed its ctx record; the app saves Hardware.Ini and
    re-opens that part; `LJ_IO_OUTPUT` re-posts `LJ_CMD_SACN_CFG`), `LJ_ACT_AUDIO_ANALYZER`, `LJ_ACT_AUDIO_OPTIONS`,
    `LJ_ACT_EXTAPP_SETUP`, `LJ_ACT_MEDIA_PLAYER`, `LJ_ACT_AUDIO_TRIG`. Windows never include `app/`: they edit the ctx
    record and run the action. Platform pieces arrive through `lj_app_io_set_platform` (`lj_app.c` installs the SDL ones;
    the tests run headless with NULL).
11. **Tests.** `test_engine_w5`, `test_socket`, `test_uiscript_w5`, `replay_w5_contract` are live. The five
    expected-pending rows each prove a synthetic seed first (our own sACN packet from `lj_sacn_pack_data`; ArtDmx /
    ArtPoll from the track's builders plus a hand-built 239-byte ArtPollReply; a synthetic `.vtr` text; the 05 s10.1
    example bytes; a three-entry `.LJB` built with miniz), then run `LJ_FUZZ_ITERS` (20000) seeded mutations through the
    real entry points: `fuzz_sacn_rx` and `fuzz_vtr` (dmxin), `fuzz_artnet` (artnet), `fuzz_remote` (remote), `fuzz_ljb`
    (ljb). A track deletes its row names from the `DISABLED` lines in the commit that lands the parser (the W2
    `profiles_ids` precedent) and appends the matching libFuzzer target to `tools/fuzz/` and the `LJ_FUZZ` list; the
    `ci.yml` fuzz step is the integrator's.
12. **Script verbs and replay lines.** `docs/ui-script.md` "W5 verbs" and `docs/replay-grammar.md` "W5 lines". The
    verbs build packets with the real builders and enter through `lj_dmxin_rx` / `lj_audio_detect` / `lj_remote_inject`,
    failing with `pending: <track>` until the module lands.

### Listeners and devices (W5-X-OFF)

| Part | Opens when | Bound to | Thread |
|---|---|---|---|
| Art-Net UDP 6454 | a link has `proto` Art-Net, or `artnet_in` != 0 | the chosen NIC address | engine |
| sACN input UDP 5568 | `[DMXIN] Enabled` and source sACN | 0.0.0.0, joined on the chosen NIC | UI |
| Remote TCP | `[REMOTE] TcpEnabled` | 127.0.0.1 unless the user picks an interface | UI |
| Remote serial | `[REMOTE] SerialEnabled` | the chosen port | UI |
| External Application | a profile-90 fixture's link is configured | serial port / TCP client | UI |
| Audio capture | `[AUDIO] Enabled` | the chosen device | SDL audio thread posts, UI analyses |
| MediaPlayer playback | an MP Load File line or the player window loads a WAV | the default output device | UI (SDL's stream lock) |

## 2. Tracks (disjoint write paths)

"Writes" is exclusive unless marked shared (section 3). Every track: reads design 1, 2.0, 2.3, 8, 10 W5, 12 and its
notes in full; meets every X row of `docs/w5-gates.md`; binds every OnXxx its forms declare (ME-16); returns its
fidelity rows and docs paragraphs as text for the integrator; keeps CP1252, integer ms, C11, no float in SDL-free dirs.

| Track | Writes | Gates |
|---|---|---|
| artnet | `src/io/lj_artnet.c` (+ `.h` below the contract), `src/io/lj_sacn.c` (Art-Net links unmapped for sACN, `[ARTNET]` keys), the `have_sacn` line of `src/platform/sdl/lj_sdl.c`, the Art-Net stats fields appended to `engine/lj_engine.h` + setter in `lj_engine.c`, `tests/io/test_artnet.c`, `tools/fuzz/fuzz_artnet.c` (new), `tools/gen_vectors.py` (append ArtDmx vectors) + `tests/vectors.h` | W5-ART-1..7, W5-ART-IN (with dmxin), ME-23, ME-24 |
| dmxin | `src/io/lj_dmxin.c` (+ `.h` below the contract), `src/windows/lj_dmx_in.c` (+ `.h`), `src/windows/lj_global_patch.c`, `src/desktop/lj_toolbars.c` (C-L DMX In live), `tests/io/test_dmxin.c`, `tests/ui/test_dmx_in.c` (append), `tests/engine/replay/w5/*.lj4r` (append), `tools/fuzz/fuzz_sacn_rx.c`, `fuzz_vtr.c` (new), `tools/gen_vectors.py` (append the Sine tables), the `LJ_FNF_EXT` DMX-in level rows in `io/lj_actions.h` (append) | W5-DMXIN-1..8, W5-ART-IN, F48, I7, F8 |
| audio | `src/io/lj_audio.c` (+ `.h` below the contract), `src/platform/sdl/lj_sdl_audio.c`, `src/windows/lj_audio_form.c` (+ `.h`), `src/desktop/lj_statusbar.c` (the Trig indicator), `tests/io/test_audio.c` (new), `tools/gen_vectors.py` (append FFT / window tables, the synthetic PCM), `packaging/macos/Info.plist.in` (`NSMicrophoneUsageDescription`) | W5-AUD-1..6 |
| ljb | `src/model/lj_ljb.c` (+ `.h` below the contract), `tests/model/test_ljb.c`, `tools/fuzz/fuzz_ljb.c` (new), the `.LJB` block of `src/windows/lj_backup_restore.c`, the LORD_C-9 `notes.txt` cap in `src/model/lj_restore_data.c` | W5-LJB-1..5, ME-8, LORD_C-9 |
| remote | `src/io/lj_remote.c` (+ `.h` below the contract), `src/io/lj_serial_win.c`, `lj_serial_posix.c` (+ `lj_serial.h`), `src/windows/lj_remote_form.c` (+ `.h`), `tests/io/test_remote.c`, `tools/fuzz/fuzz_remote.c` (new) | W5-REM-1..4, I10 |
| extapp | `src/io/lj_extapp.c` (+ `.h` below the contract), `src/windows/lj_extapp_form.c` (+ `.h`), a new `src/controls/lj_ctl_extapp.c` (+ `.h`) and profile 90's window entry in `src/fixlib/hand/lj_generics.c`, the `ext` word in `src/model/lj_sequence.c`, the `lj_frame.ext` copy in `src/engine/lj_engine.c` (one static function called at the end of the tick), `tests/io/test_extapp.c` (new) | W5-EXT-1..4, I11 |
| media | `src/io/lj_media.c` (+ `.h` below the contract), `src/platform/sdl/lj_sdl_media.c`, `src/windows/lj_media_form.c` (+ `.h`), `src/model/lj_cuelist.c` (MP out of `lj_cl_cmd_inert`), `src/app/lj_cuelist_run.c` (MP lines to `h->media`), `src/windows/lj_cuelist_form.c` (MP rows, `sbMPTime`), `tests/io/test_media.c` (new), `tests/app` / `tests/ui` cue-list rows (append) | W5-MED-1..5, F31, I4, ME-11 |
| integrator | ME-165, ME-166 and every other integrator row of `docs/w5-gates.md`; `docs/fidelity.md`, `docs/file-formats.md` rows from the tracks' text, `docs/sacn.md`, `running.md`, `SECURITY.md`, user guide, help, `ci.yml` fuzz step, design.md edits (design owner) | the X, ENTRY and RELEASE rows |

## 3. Shared files (append-only, one marked block per track)

The W4 list (`CMakeLists.txt` tests section, `tests/lj_tests.c`, `tools/glyphmap.tsv`, `src/io/lj_actions.h`,
`src/app/lj_app.c` bindings, `tools/gen_vectors.py`, `tools/test_layering.py` allowlists, `docs/file-formats.md`,
`tests/ui/golden/REVIEWED.tsv`) plus:

- `src/windows/lj_hardware_setup.c`: artnet (the output page's per-link protocol and Port-Address), dmxin
  (`cbUseDMXin`, the input universe), audio (`tsAudio` back in the page set) and remote (`TSDA_RS232`) each add their
  controls' handler rows and one call into their own family file, inside a `/* W5 <track> */` block; the page logic
  lives in the track's own file. Nobody reorders the existing `greyed[]` / `dropped[]` arrays: a track removes only its
  own name from them.
- `src/desktop/lj_main_window.c`: audio (`M_Audio` shown, `M_AudioClick` -> `LJ_ACT_AUDIO_ANALYZER`,
  `M_AudioOptionsClick` -> `LJ_ACT_AUDIO_OPTIONS`) and extapp (`PI_DefineAppLink` out of `lj_inert`, its handler running
  `LJ_ACT_EXTAPP_SETUP` with the icon's fixture): each edits only its own rows.
- `src/engine/lj_engine.h` / `.c`: artnet (stats) and extapp (the frame copy) append only; no other engine edit in W5
  (W5-X-RIG: `lj_engine_rules.h` untouched).
- `src/app/lj_app_io.c` and the engine W5 kinds are the contracts seat's; a track that needs another call asks the
  integrator.

## 4. Measured (Ninja + cl 19.44, Release, this commit)

- `build-w4-nosdl` (`LJ_SDL` OFF) and `build-w4` (`LJ_SDL` ON): 0 warnings, 0 errors.
- `ctest` in both trees: every existing row and the four new live rows pass; the five fuzz rows are `Not Run
  (Disabled)` and each prints `PENDING <track>` when run by hand.
- `tools/test_layering.py` (the `test_layering` CTest row, cl): 0 findings with the three appended OS_ALLOW rows
  (`io/lj_socket.c`, `io/lj_serial_win.c`, `io/lj_serial_posix.c`), the two PAIR_EXEMPT rows and `termios.h`,
  `fcntl.h`, `sys/select.h` appended to OS_HEADERS.
- `replay_dump`: the scenario set is unchanged (the W5 scenario sits outside the glob).

## 5. Deviations and open points of this seat

1. **ME-165 / ME-166 not landed** (first line). The tracks' un-grey step depends on ME-165's `lj_widget_set_enabled`
   and the single `LJ_NA_HINT`; until it lands a track writes its parser, module and tests, not its chrome.
2. **W5-LJB-1 "one container reader shared with .Udf".** The reader lives in `model/` as the brief places it;
   `fixlib/lj_gs2.c` cannot call up into `model/`, so `.Udf` keeps its W3 reader (fuzzed by `fuzz_udf`). Either the ljb
   track moves `lj_ljb_open / extract` into `fixlib/` and points `lj_udf_parse` at it, or the integrator closes that half
   of the row in writing.
3. **Art-Net lives in the sACN record.** One output record and one command keep ME-23 with no second config path; the
   Art-Net fields are appended to `lj_sacn_cfg` and its validation is `lj_artnet_cfg_validate`.
4. **The sACN receiver parser is `lj_sacn_rx_parse` in `io/lj_dmxin.h`,** not `io/lj_sacn.c`: the sender file stays
   engine-thread only and the dmxin track owns every byte of the receiver.
5. **PROPOSAL numbers introduced here** (each tagged where defined): `LJ_DMXIN_SOURCES` 4, `LJ_DMXIN_RX_BUDGET` 64, the
   loss rule default hold, `LJ_ARTNET_NODES` 32, `LJ_ARTNET_EXPIRE_POLLS` 3, `LJ_ARTNET_REPLY_MIN` 207, `LJ_ARTNET_OEM`
   0x00FF, `LJ_REMOTE_TCP_PORT` 7232, `LJ_REMOTE_CONNS` 4, `LJ_REMOTE_BAUD_DEFAULT` 9600, `LJ_AUDIO_RATE` 44100,
   `LJ_AUDIO_BLOCK` 512, the analyzer defaults (decay 200 ms, factor 3.0, gain 100 %), `LJ_SCENE_EXT` 4,
   `LJ_EXTAPP_RECONNECT_US` 2 s, `LJ_MEDIA_FILE_MAX` 256 MB, `LJ_LJB_ENTRIES_MAX` 8192, `LJ_LJB_TOTAL_MAX` 512 MB, the
   file names `DmxIn/DmxIn.dat` and `LJBImport.txt`. Each track may change its own with the reason in its log.
6. **Hardware.Ini sections** `[ARTNET]`, `[DMXIN]`, `[AUDIO]`, `[REMOTE]`, `[EXTAPP]` are ours and additive (W5-X-FORMAT);
   each track writes its `docs/file-formats.md` rows. The `[MIDI] device (W5)` row is the integrator's.
7. **design.md** stays the vault's: the 5.3 sentence "Not imported, ever ... `.vtr` tables" and the W5 paragraph of
   design 1 / 10 are design-owner edits (W5-DMXIN-6, W5-X-SACRIFICE).
