/* lj_sacn.h - the sACN (ANSI E1.31-2018) sender, the C port of
 * tools/ref/08-sacn_ref.py Sender (design 8.1). Only the engine thread calls
 * open/set/pump/close (design 3.1, ME-23): the UI changes the config through
 * LJ_CMD_SACN_CFG and the engine thread does close-old (3 x Stream_Terminated
 * per unmapped universe) then open-new. lj_sacn.c is on the OS-header
 * allowlist of tools/test_layering.py (winsock2.h ... sys/socket.h). */
#ifndef LJ_IO_SACN_H
#define LJ_IO_SACN_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_types.h"
#include "core/lj_ini.h"

enum {
    LJ_SACN_PORT = 5568, LJ_SACN_DATA_LEN = 638 /* 512 slots */, LJ_SACN_NAME_CAP = 64, LJ_SACN_UNICAST_MAX = 8,
    LJ_SACN_UNIVERSE_MAX = 63999, LJ_SACN_DISCOVERY_UNIVERSE = 64214, LJ_SACN_PRIORITY_DEFAULT = 100, LJ_SACN_PRIORITY_MAX = 200,
    LJ_SACN_REPEATS = 3 /* the change plus two repeats, E1.31-2018 s6.6.2 */
};
#define LJ_SACN_KEEPALIVE_US     900000u    /* 800-1000 ms window */
#define LJ_SACN_DISCOVERY_US   10000000u
#define LJ_SACN_MIN_INTERVAL_US   22727u    /* ME-24: E1.11 ~44 Hz cap; a universe sent less than this ago is skipped unless unlock_rate (latest frame wins) */
#define LJ_SACN_TTL_DEFAULT          1

typedef struct { uint32_t nic_ipv4; char name[LJ_SACN_NAME_CAP]; uint8_t cid[16]; uint8_t priority, ttl, preview, unlock_rate;
                 uint16_t universe_of_link[LJ_LINKS]; uint32_t unicast[LJ_LINKS][LJ_SACN_UNICAST_MAX]; int nunicast[LJ_LINKS]; } lj_sacn_cfg;   /* name: CP1252, converted to UTF-8 when the packet is built (design 2.1); universe 0 = link not mapped; nic_ipv4 / unicast: host byte order */

typedef struct { uint32_t packets[LJ_LINKS], pps[LJ_LINKS]; uint16_t universe[LJ_LINKS]; uint32_t send_errors; int last_err;
                 uint32_t terminated, discovery; uint32_t nic_ipv4; } lj_sacn_stats;   /* per link index (the universe it feeds); pps = packets in the last whole second */

int  lj_sacn_open(const lj_sacn_cfg *cfg);                            /* one UDP socket, IP_MULTICAST_IF / TTL, no bind; copies cfg; 0 / -1 + log */
void lj_sacn_set(int universe, const uint8_t slots[LJ_LINK_CH]);      /* copies; on any change sets repeats_left = LJ_SACN_REPEATS; never sends; unknown universe ignored */
void lj_sacn_pump(uint64_t now_us);                                   /* at most one packet per universe per call: repeat, else keep-alive when due; discovery when due; rate cap (ME-24) */
void lj_sacn_close(void);                                             /* 3 x Stream_Terminated per universe, then closes the socket; no-op when closed */
void lj_sacn_stats_get(lj_sacn_stats *out);                           /* engine thread only (the UI reads the copy in lj_engine_status) */

const char *lj_sacn_cfg_validate(lj_sacn_cfg *cfg);                   /* clamps priority to 200, ttl to 1..255, keeps the NUL at name[63], nunicast <= 8; returns the first rejection (universe not 0..63999, duplicate universe, universe 64214) or NULL = usable (ME-24) */
void        lj_sacn_cfg_default(lj_sacn_cfg *cfg, const char *host);  /* "LJ4EVR <host>", priority 100, ttl 1, links 1-4 -> universes 1-4, new CID */
void        lj_sacn_cid_new(uint8_t cid[16]);                         /* UUID v4 */
int         lj_sacn_cfg_load(lj_ini *hardware_ini, lj_sacn_cfg *cfg); /* Hardware.Ini [SACN] Nic Name CID Priority TTL Preview UnlockRate U1..U4 Unicast1..4 (ours, installation-wide); missing keys = defaults; 0 / -1 when the CID is absent (caller generates and saves) */
void        lj_sacn_cfg_save(lj_ini *hardware_ini, const lj_sacn_cfg *cfg);

/* Packet builders (pure; tests/sacn/test_packets.c checks them against vectors.h, 08 Appendix B). */
int lj_sacn_pack_data(uint8_t out[LJ_SACN_DATA_LEN], const lj_sacn_cfg *cfg, uint16_t universe, uint8_t seq, uint8_t options, const uint8_t slots[LJ_LINK_CH]);   /* options: 0x40 Stream_Terminated, 0x80 Preview; 0 / -1 */
int lj_sacn_pack_discovery(uint8_t *out, size_t cap, const lj_sacn_cfg *cfg, const uint16_t *universes, int n, uint8_t page, uint8_t last_page);   /* returns the length or -1; universes sorted ascending by the caller */

#endif /* LJ_IO_SACN_H */
