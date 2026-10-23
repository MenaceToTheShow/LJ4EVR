# W6 track net

Branch `w6/net` from `origin/main` @ `2668de8` ("W6: gates"). Rows of `docs/w6-gates.md` owned by `net`, plus the
External Application serial-output check of the brief. Measured 2026-10-23 on the Windows dev PC, MSVC 2022, Ninja,
three trees inside the worktree: `build-w6net-rel` (Release, SDL ON), `build-w6net-dbg` (Debug, SDL ON),
`build-w6net-nosdl` (Release, SDL OFF). Every row is LAND; none is closed in writing.

## Rows

| Id | Status | What changed | Verify (run, output quoted) |
|---|---|---|---|
| W5/LORD_FIDELITY-11 | LANDED | `windows/lj_hardware_setup.c`: `artnet_grey()` runs on every `cbProto<n>` OnChange and after the page is filled. Per link, `seArtAddr<n>` is live only for Art-Net and `seU<n>` only for sACN. It writes `enabled` alone (plus invalidate), so the Port-Address keeps its own hint and no `LJ_NA_HINT` is set. | 17 rows appended to `tests/ui/scripts/w5_entry_points.lj4s` (run by `test_uiscript_w3`): sACN gives `seArtAddr1 0` / `seU1 1`; `click cbProto1`, `key Down`, `key Enter` gives `Art-Net`, `seArtAddr1 1`, `seU1 0`, link 2 untouched; back to sACN restores it. `lj4evr_tests test_uiscript_w3`: `test_uiscript_w3: 0 failure(s)`. Mutant (the `seU1 0` row flipped to `1`): `SCRIPT FAIL line 58: THardwareSetupForm2.seU1 is "0", expected "1"`, `1 failure(s)`, rc 1 (file restored) |
| W5/LORD_ENGINE-10 | LANDED | `io/lj_artnet.c` `lj_artnet_open`: when the socket is open and the new record has the same NIC (the port is always 6454), the node table and the poll clock are kept across the close and re-bind. Another NIC, Art-Net off or an explicit `lj_artnet_close` still forget them. | `test_artnet` row `art_reopen`: a node subscribed, `lj_artnet_open` with the same NIC (priority and link 3 changed), then one pump: `reopen: nodes=1 after re-open, 1 ArtDmx, 0 polls since`; a re-open on 127.0.0.2 gives 0 nodes. `test_artnet: PASS` |
| W5/LORD_C-10 | LANDED | `io/lj_remote.c`: each TCP slot records its accept time and its last bytes; a slot silent for `LJ_REMOTE_IDLE_US` (60 s, tagged PROPOSAL in `io/lj_remote.h`) is closed with a log line and freed. Documented in `docs/remote.md`, `docs/running.md` (listener table) and `SECURITY.md`. | `test_remote` row `rem_idle` on the injected clock: two clients accepted at t; one sends a frame at t + 30 s; at t + 60 s - 1 us both open; at t + 60 s the silent one is closed (`remote: TCP connection 2 closed: silent for 60 s`); a new client takes the freed slot with no refusal; at t + 90 s the other closes, the new one stays. `idle: a connection silent 60 s closed, its slot reused, a live one kept`, `test_remote: PASS` |
| W5/WIZARD_STYLES-9 | LANDED | `io/lj_net.c` gains `lj_net_ip_parse` (strict: 1-3 decimal digits per group, 0..255, three dots, nothing else; `*ip` written only on success) and `lj_net_ip_fmt`. Deleted: `lj_di_ip_parse`, `lj_extapp_ip_parse` (and its `lj_extapp.h` line), `lj_remote_ip_parse` (and its `lj_remote.h` line), `lj_rem_ip_fmt`, `lj_ip_parse` / `lj_ip_fmt` (`lj_sacn.c`), `ip_parse` / `ip_text` (`lj_hardware_setup.c`), and the inline `snprintf("%u.%u.%u.%u")` copies in `lj_dmxin.c` (2), `lj_extapp.c` and `lj_remote_form.c`. Log lines that print an address through `lj_log` stay. `tools/test_layering.py` check 12 counts any copy. New `test_net` CTest row. | `test_layering`: `... popup_at 2, ip-parse copies 0, 0 finding(s)` (rc 0). The same tool over unmodified `main` `src/`: `ip-parse copies 21`. `test_net`: `ip: 17 hostile forms refused ("+127.0.0.1" first), fmt round trip`, `test_net: PASS` |
| W5/WIZARD_STYLES-10 | LANDED | (a) `io/lj_sacn.c` sends through `lj_socket_udp(0, 0, 0)` (0.0.0.0 and an ephemeral port, which is what the old unbound `sendto` picked) and `lj_socket_sendto`. The new seam call `lj_socket_mcast_out` (`io/lj_socket.h`) sets IP_MULTICAST_IF and IP_MULTICAST_TTL, with the Windows DWORD / POSIX u_char split. `lj_sacn.c` includes no OS header. (b) The `io/lj_sacn.c` row is gone from `OS_ALLOW`. | `grep -c '"io/lj_sacn.c"' tools/test_layering.py`: `0`; `test_layering` `0 finding(s)`; `test_sacn_packets`, `test_sacn_rate`, `test_sacn_cfg`, `test_sacn_loopback` Passed in all three trees; determinism `lj4evr_tests dump-frames` in rel, dbg and nosdl: `5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50`, 1,641,600 bytes each (unchanged) |
| extapp serial output on `lj_serial` (brief) | NOT NEEDED | At `2668de8` the serial path is not a stub. `io/lj_extapp.c:230` opens with `lj_serial_open` and `:280` writes with `lj_serial_write`. Both backends (`io/lj_serial_win.c`, `io/lj_serial_posix.c`) implement list / open / read / write / close. No change. | `grep -n -i "stub\|not implemented" src/io/lj_serial*.c src/io/lj_serial.h`: no output |

## Seam change (docs/w5-contracts.md seam 3)

`io/lj_socket.h` gains one call:
`int lj_socket_mcast_out(lj_socket s, uint32_t nic_ip, uint8_t ttl)`. It returns 0, or -1 with a log line for each
failed option. Its only caller is the sACN sender. The sender socket is now non-blocking like every other seam handle.
A full send buffer therefore counts as a send error (logged once per distinct error), where the old socket would have
blocked the engine tick. `io/lj_extapp.h` loses `lj_extapp_ip_parse` and `io/lj_remote.h` loses `lj_remote_ip_parse`.
Both callers now use `lj_net_ip_parse`.

## Gates of the brief

| Check | Result |
|---|---|
| Release SDL ON build (`/WX` on every TU) | rc 0, 0 warnings |
| Debug SDL ON build | rc 0, 0 warnings |
| Release SDL OFF build | rc 0, 0 warnings |
| ctest Release SDL ON | `100% tests passed, 0 tests failed out of 154` |
| ctest Debug SDL ON | `100% tests passed, 0 tests failed out of 154` |
| ctest Release SDL OFF | `100% tests passed, 0 tests failed out of 152` |
| test_layering | `ip-parse copies 0, 0 finding(s)` |
| fidelity_check | `73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)` |
| D4 | `git diff origin/main -- src/engine/lj_engine_rules.h` empty; no `PROPOSAL rig` tag removed |
| Listeners | none added; the remote stays off by default, and the sACN sender opens only when the engine applies a record, as before |

Note on the loopback rows. The first full Debug and SDL-OFF runs each failed `test_sacn_loopback` (and once
`dmxin_loopback`) on received-packet counts, with `0 send errors` on the sender side. Other W6 tracks' `ctest` runs
were live on the same PC at the time, and their tests also bind and send to 127.0.0.1:5568. The `RESOURCE_LOCK` only
serializes within one ctest. The Debug binary run alone while those runs were live failed 3 of 7 times, again on
received counts with `0 send errors`. It then passed 5 of 5, and the next full run of each tree was 100%. The
unmodified `main` binary passed 5 of 5 in the same later window. The cause is not proven. The evidence points to the
shared port: the sender counts were right in every failing run. The integrator's single-tree run on a quiet PC is the
clean measure, and that row should be watched there.

Not run here: gcc / clang (no Linux toolchain on this PC); the branch CI is the first gcc / clang compile of these
files.
