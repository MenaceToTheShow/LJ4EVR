/* lj_extapp.h - External Application string output (design 1, 10 W5, 12 I11; note 05 s10.2, note 13 strings;
 * docs/w5-contracts.md seam 6; track extapp).
 *
 * LJ2 drove a plug-in program (LJ_RS232Send) from the "Other / External Application/Device" profile (id 90, tag
 * EXTERNAL): a command string per scene. LJ4EVR replaces the plug-in with built-in transports and executes nothing:
 * the Define Application Link entry (PI_DefineAppLink) opens TExternalAppSetupForm, which picks the transport.
 *
 * Grammar (05 s10.2): ASCII 32..127, at most 32 characters per scene; `#123` = one decimal byte 0..255 (a larger value
 * is refused), `#X7F` = one hex byte, `!` ends the transmitted part (the suffix only makes the string differ, so
 * "TRIG!1" then "TRIG!2" send "TRIG" twice); a string is sent only when it changes (05 s10.2).
 *
 * Data path (W5-X-ENGINE): the string lives in lj_scene.ext (model/lj_model.h, the `ext` word of the scene record); the
 * engine copies the winning one per fixture into lj_frame.ext each tick; the UI loop (app/lj_app_io.c) reads its frame
 * copy and calls lj_extapp_step, which encodes and sends on change. Transports, off until a profile-90 fixture is
 * patched and its link configured (W5-X-OFF), non-blocking, failures logged and counted, never fatal: serial through
 * io/lj_serial.h, or a TCP client (io/lj_socket.h) to host:port with a reconnect every LJ_EXTAPP_RECONNECT_US
 * (PROPOSAL). The 4 DMX-out / 4 DMX-in channel values LJ2 also handed the plug-in are not sent (a fidelity row).
 * Settings: Hardware.Ini [EXTAPP] per fixture slot (ours, additive). */
#ifndef LJ_IO_EXTAPP_H
#define LJ_IO_EXTAPP_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_ini.h"
#include "model/lj_model.h"

enum lj_extapp_transport { LJ_EXTAPP_OFF = 0, LJ_EXTAPP_SERIAL, LJ_EXTAPP_TCP };
enum { LJ_EXTAPP_MAX_BYTES = 32 /* encoded bytes of one string */ };
#define LJ_EXTAPP_RECONNECT_US 2000000u   /* PROPOSAL rig W5-EXT-3 */

typedef struct { uint8_t fixture /* 1..LJ_FIXTURES, 0 = slot unused */; uint8_t transport /*enum lj_extapp_transport*/; char serial_port[64]; uint32_t baud; uint32_t ip; uint16_t port; } lj_extapp_link;
typedef struct { lj_extapp_link link[LJ_SCENE_EXT]; } lj_extapp_cfg;
typedef struct { uint32_t sent, failed, refused; uint8_t connected[LJ_SCENE_EXT]; } lj_extapp_stats;

int  lj_extapp_encode(const char *cmd, uint8_t out[LJ_EXTAPP_MAX_BYTES], size_t *n, char *err, size_t cap);   /* the grammar above: 0 with the bytes to send, -1 refused (length > 32, a byte outside 32..127, #256, a bad #X) with the reason */
const char *lj_extapp_validate(const char *cmd);                           /* NULL = a valid string (the control window's edit check) */

void lj_extapp_cfg_default(lj_extapp_cfg *c);
void lj_extapp_cfg_load(lj_ini *hardware_ini, lj_extapp_cfg *c);
void lj_extapp_cfg_save(lj_ini *hardware_ini, const lj_extapp_cfg *c);
int  lj_extapp_open(const lj_extapp_cfg *c);                               /* close-old then open the configured links (one log line each); 0 / -1 */
void lj_extapp_close(void);
void lj_extapp_step(const uint8_t ext_fixture[LJ_SCENE_EXT], const char *ext, uint64_t now_us);   /* UI thread, the frame copy's fields (ext = &frame.ext[0][0], LJ_SCENE_EXT rows of LJ_EXT_CMD_CAP): send each changed string to its fixture's link; reconnects */
void lj_extapp_stats_get(lj_extapp_stats *out);

#endif /* LJ_IO_EXTAPP_H */
