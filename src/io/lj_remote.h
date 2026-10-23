/* lj_remote.h - the RS-232 / TCP remote protocol, LJ_RS232.EXE as a built-in listener (design 8.4, 10 W5; note 05 s10.1;
 * docs/w5-contracts.md seam 5; track remote).
 *
 * Grammar (05 s10.1, H2.100 p230-231): frames of three bytes [cmd >= 128][d1 <= 127][d2 <= 127] arriving within
 * LJ_REMOTE_FRAME_US; value = d1 * 128 + d2; any byte >= 128 starts a new frame (a partial one is dropped); a partial
 * frame older than LJ_REMOTE_FRAME_US is dropped. Commands:
 *   128 load cue list value (0 = stop current)      129 load cue value (0 = clear current)
 *   130 load BG cue value (0 = clear)               131 flash sequence on value (0 = clear)
 *   132 d1 0 smoke off/on (d2 0/1), d1 1 smoke timer off/on, d1 2 blackout (d2 1) / restore (d2 0), d1 3 fade in (d2 0) / out (d2 1)
 *   133 static d1 enable (d2 1) / disable (d2 0)    134 d1 1: current cue list Go (d2 1) / Back (d2 2), manual lists only
 *   135 cue slot d1 on (d2 1) / off (d2 0)          136 BG cue slot d1 on (d2 1) / off (d2 0)
 * Example: cue 130 = 129, 1, 2. Out-of-range numbers are ignored with a rate-limited log line. The parser is pure and
 * hostile-input safe (the remote fuzz row); the actions run on the UI thread through lj_fn_run / lj_fn_run_prog and the
 * existing loaders (lj_remote_exec, called by app/lj_app_io.c), never in the engine (W5-X-ENGINE).
 *
 * Transports, all OFF by default (W5-X-OFF), polled by the UI loop with a byte budget (W5-X-THREAD):
 *   - TCP: a listener (io/lj_socket.h) bound to 127.0.0.1 unless the user picks an interface from lj_net_list_ipv4, on
 *     LJ_REMOTE_TCP_PORT (PROPOSAL); at most LJ_REMOTE_CONNS connections, extra ones accepted and closed at once; each
 *     connection has its own frame state; one silent for LJ_REMOTE_IDLE_US is closed (W6 LORD_C-10). There is no authentication: on a LAN interface anyone on that network can
 *     drive the show (docs/running.md, SECURITY.md).
 *   - serial: one port through io/lj_serial.h at the chosen baud, 8N1.
 * Settings: Hardware.Ini [REMOTE] TcpEnabled TcpIp TcpPort SerialEnabled SerialPort Baud (ours, additive). */
#ifndef LJ_IO_REMOTE_H
#define LJ_IO_REMOTE_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_ini.h"

enum {
    LJ_REMOTE_TCP_PORT = 7232,     /* PROPOSAL rig W5-REM-2 (unassigned by IANA; no LJ2 value exists, LJ2 had no TCP) */
    LJ_REMOTE_CONNS = 4,           /* PROPOSAL rig W5-REM-2 */
    LJ_REMOTE_BUDGET = 1024,       /* bytes per transport per UI pass (W5-X-THREAD), PROPOSAL */
    LJ_REMOTE_BAUD_DEFAULT = 9600  /* PROPOSAL until the harvested LJ_RS232.exe baud list says otherwise (W5-REM-3) */
};
#define LJ_REMOTE_FRAME_US 200000u   /* 05 s10.1: all three bytes within 0.2 s */
#define LJ_REMOTE_IDLE_US 60000000u  /* LORD_C-10: a TCP connection that sends nothing for 60 s is closed and its slot freed. PROPOSAL (LJ2 had no TCP) */

enum lj_remote_cmd { LJ_REM_CUELIST = 128, LJ_REM_CUE, LJ_REM_BGCUE, LJ_REM_FLASH, LJ_REM_MISC, LJ_REM_STATIC, LJ_REM_CL_GOBACK, LJ_REM_SLOT, LJ_REM_BGSLOT, LJ_REM_LAST = LJ_REM_BGSLOT };
typedef struct { uint8_t cmd, d1, d2; uint16_t value; } lj_remote_msg;   /* value = d1 * 128 + d2 */
typedef struct { uint8_t buf[3]; uint8_t have; uint64_t t0_us; uint32_t frames, dropped; } lj_remote_parser;

void lj_remote_parser_init(lj_remote_parser *p);
int  lj_remote_feed(lj_remote_parser *p, const uint8_t *bytes, size_t n, uint64_t now_us, lj_remote_msg *out, int max);   /* complete frames written to out (at most max; the rest of the input is still consumed and counted) */
int  lj_remote_exec(const lj_remote_msg *m);                              /* UI thread: runs the action; 0 done, -1 ignored (unknown command, out of range, Go / Back on an automatic list) */

typedef struct { uint8_t tcp_on; uint32_t tcp_ip /* host order; LJ_IP_LOOPBACK default */; uint16_t tcp_port; uint8_t serial_on; char serial_port[64]; uint32_t baud; } lj_remote_cfg;
void lj_remote_cfg_default(lj_remote_cfg *c);                             /* both off, 127.0.0.1:LJ_REMOTE_TCP_PORT, COM1 / first port, 9600 */
void lj_remote_cfg_load(lj_ini *hardware_ini, lj_remote_cfg *c);
void lj_remote_cfg_save(lj_ini *hardware_ini, const lj_remote_cfg *c);
int  lj_remote_open(const lj_remote_cfg *c);                              /* close-old then open what c enables (one log line per bind); 0 / -1 (the part that failed stays closed) */
void lj_remote_close(void);                                               /* before the engine stops (W5-X-THREAD shutdown order) */
int  lj_remote_poll(uint64_t now_us);                                     /* UI thread: accept, read every transport within the budget, feed, exec; returns the frames executed */
int  lj_remote_inject(const uint8_t *bytes, size_t n, uint64_t now_us);   /* the UI script `remote <line>` and the tests: the same feed + exec as a connection; frames executed, -1 bad arguments */

/* ---- remote track (below the contract block) ---- */
/* 132 d1 1, 133, 135, 136 and 134 set a state, but the hotkey functions they run are toggles: the windows layer answers
 * what is on now (windows/lj_remote_form.c reads ctx->view, the Statics 2 page and the cue-list status). Without a query
 * (headless) those commands are ignored and logged. */
enum lj_remote_q { LJ_REMQ_SMOKE_TIMER, LJ_REMQ_STATIC /* n 1..20, current page: active */, LJ_REMQ_STATIC_FLASH /* n: activation Flash */,
                   LJ_REMQ_SLOT /* n 1..12 enabled */, LJ_REMQ_BGSLOT /* n 1..5 enabled */, LJ_REMQ_CL_MANUAL /* the current list waits on a manual line */ };
typedef int (*lj_remote_query_fn)(enum lj_remote_q q, int n);             /* 1 / 0, -1 unknown */
void lj_remote_set_query(lj_remote_query_fn fn);
/* ME-168: the set-style commands (132 1 x smoke timer, 135 slot, 136 BG slot) post the set form through this hook (the
 * cue record / BG / smoke setter), never a toggle against the engine view, which lags one tick: two equal frames in one
 * tick set twice, they never toggle twice. 0 done, -1 = state unknown (ignored). */
typedef int (*lj_remote_setter_fn)(enum lj_remote_q q, int n, int on);
void lj_remote_set_setter(lj_remote_setter_fn fn);
enum { LJ_REMOTE_BAUDS = 7 };
extern const uint32_t lj_remote_bauds[LJ_REMOTE_BAUDS];                   /* rgBaudRate of TRS232SettingsForm (LJ_RS232.exe): 2400 .. 57600 */
typedef struct { uint8_t tcp_open, serial_open; uint16_t tcp_port; int conns; uint32_t bytes, frames, ignored, refused; } lj_remote_stats;
void lj_remote_get_stats(lj_remote_stats *out);                           /* the settings window status line and the tests */

#endif /* LJ_IO_REMOTE_H */
