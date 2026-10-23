/* lj_dmxin.h - DMX-in over sACN (and Art-Net) input, the .vtr value tables and the DMX-in functions (design 1, 10 W5,
 * 12 F8 / F48 / I7; note 05 s3; note 08 s5; docs/w5-contracts.md seam 2; track dmxin).
 *
 * One input seam for both protocols: every packet, from the UI thread's own sACN socket or posted by the engine thread
 * (Art-Net arrives on the sender's 6454 socket, io/lj_artnet.h), goes through lj_dmxin_rx on the UI thread, which
 * validates it (E1.31 5.1-5.5, 6.2.3, 6.2.6, 6.7, 9.1.1 / Art-Net spec p.63), files it under its source and merges the
 * sources into ONE 512-slot input frame the UI thread owns (lj_dmxin_frame). Nothing here touches the engine: the app
 * (app/lj_app_io.c) posts LJ_CMD_DMX_IN with a pointer to frame.out (translated) when lj_dmxin_poll reports a change,
 * which the engine copies into post.dmx_in[] at the drain under lj_lock (W5-X-ENGINE), and runs the assigned
 * functions with lj_dmxin_functions (lj_fn_run on the UI thread, never in the engine).
 *
 * Receiver rules (note 08 s5; declared in docs/sacn.md, the 6.2.3.4-5 documentation duty):
 *   - discard: wrong preamble / post-amble / ACN id / root vector (0x4 only; 0x8 extended ignored) / framing vector 0x2 /
 *     DMP vector 0x02 / address-data type 0xA1 / first address 0 / increment 1; property count 1..513; START code 0x00
 *     only (0xDD and others ignored); Preview_Data ignored; the universe comes from the packet, never the group address;
 *   - sequence per source: discard when the signed 8-bit (B - A) is in (-20, 0];
 *   - a source is lost after LJ_DMXIN_TIMEOUT_US or one Stream_Terminated;
 *   - our own CID (lj_sacn_cfg.cid) is ignored; an input universe equal to one of our output universes is refused by
 *     lj_dmxin_cfg_validate;
 *   - merge: the highest priority present wins; equal-priority sources HTP per slot over at most LJ_DMXIN_SOURCES
 *     (PROPOSAL rig W5-DMXIN-2); a further source is ignored and counted ("sources exceeded");
 *   - fewer than 512 slots: the rest are 0; on the loss of every source the frame follows cfg.loss (PROPOSAL rig
 *     W5-DMXIN-3). Art-Net sources merge by the same rule at priority 100 (PROPOSAL; the spec's node merge is LTP/HTP
 *     between two sources only).
 *
 * Sockets: the sACN input is a UDP socket (io/lj_socket.h) bound to 0.0.0.0:5568 with SO_REUSEADDR (sACNView and others
 * may share the port), joined to 239.255.hi.lo on cfg.nic_ipv4, and also takes unicast; opened only while cfg.enabled
 * and cfg.source == LJ_DMXIN_SACN (W5-X-OFF), non-blocking, drained LJ_DMXIN_RX_BUDGET datagrams per pass.
 *
 * Files: the configuration is Hardware.Ini [DMXIN] (installation-wide, additive); the channel assignments are the user
 * library's DmxIn\DmxIn.dat (PROPOSAL name, design 5.2 grammar, `LJ4 dmxin 1`, one `ch` line per assigned channel);
 * value tables are <userdata>\User\ValueTables\<Type>_<Name>_<ID>.vtr (05 s3.3: installation-wide, not in backups).
 * Every file is untrusted (W5-X-HOSTILE): sizes, line lengths, integer ranges with ERANGE, names. */
#ifndef LJ_IO_DMXIN_H
#define LJ_IO_DMXIN_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_types.h"
#include "core/lj_ini.h"

enum lj_dmxin_src  { LJ_DMXIN_SACN = 0, LJ_DMXIN_ARTNET };
enum lj_dmxin_loss { LJ_DMXIN_LOSS_HOLD = 0 /* hold the last frame */, LJ_DMXIN_LOSS_ZERO /* zeros: the E1.31 10.2.2 "output stops" mode */ };   /* PROPOSAL rig W5-DMXIN-3 */
enum {
    LJ_DMXIN_SOURCES = 4,          /* equal-priority HTP over at most 4 sources, PROPOSAL rig W5-DMXIN-2 */
    LJ_DMXIN_PKT_MAX = 638,        /* the largest packet either protocol sends (sACN 638 > ArtDmx 530); a receive buffer is one byte more */
    LJ_DMXIN_QUEUE = 8,            /* packets the engine thread may park between two UI passes (latest kept per source by the merge anyway) */
    LJ_DMXIN_RX_BUDGET = 64        /* datagrams per UI pass (W5-X-THREAD), PROPOSAL */
};
#define LJ_DMXIN_TIMEOUT_US 2500000u   /* E1.31 6.7.1 */

typedef struct {
    uint8_t  enabled;              /* Hardware Setup cbUseDMXin; 0 on a fresh userdata (W5-X-OFF) */
    uint8_t  source;               /* enum lj_dmxin_src */
    uint16_t universe;             /* sACN 1..63999, or the Art-Net Port-Address 1..32767 (the app copies it to lj_sacn_cfg.artnet_in) */
    uint32_t nic_ipv4;             /* the interface to join on (host order; 0 = default) */
    uint8_t  loss;                 /* enum lj_dmxin_loss */
    uint8_t  functions_off;        /* Options > Disable DMX-in functions / the three DMXin Functions hotkeys (the Global Patch overrides keep working) */
} lj_dmxin_cfg;

typedef struct { uint8_t cid[16]; uint32_t ip; uint8_t kind /*enum lj_dmxin_src*/, priority, seq, terminated; uint64_t last_us; uint16_t n; uint8_t slots[LJ_LINK_CH]; char name[64]; } lj_dmxin_source;   /* name: CP1252 after lj_utf8_to_cp1252 of the bounded 64 bytes (W5-X-TEXT) */
typedef struct {
    uint8_t  raw[LJ_LINK_CH];      /* merged, before translation (the DMX-in window's left list shows out[]) */
    uint8_t  out[LJ_LINK_CH];      /* after the per-channel .vtr tables: what LJ_CMD_DMX_IN carries */
    uint16_t channels;             /* incoming channel count (the window's upper part) */
    uint16_t hz;                   /* refresh rate over the last whole second */
    uint8_t  live;                 /* at least one source not lost */
    int      nsources; lj_dmxin_source src[LJ_DMXIN_SOURCES];
    uint32_t packets, discarded, exceeded;   /* counters for the window / Debug */
} lj_dmxin_frame;

/* ---- receive (UI thread, except lj_dmxin_post) ---- */
int  lj_dmxin_open(const lj_dmxin_cfg *cfg, const uint8_t own_cid[16]);   /* close-old then open-new; disabled or Art-Net = no socket, returns 0; -1 + log on a bind failure */
void lj_dmxin_close(void);
int  lj_dmxin_poll(uint64_t now_us);                                       /* drains the sACN socket (budget) and the posted queue through lj_dmxin_rx, applies time-outs and the loss rule, merges, translates; 1 when out[] changed */
int  lj_dmxin_rx(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us);   /* one packet (enum lj_dmxin_src): 0 filed, -1 discarded (counted); the sacn_rx / artnet fuzz rows call it */
void lj_dmxin_post(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us);  /* any thread: a bounded copy into the queue under lj_lock; full / oversize = dropped and counted */
const lj_dmxin_frame *lj_dmxin_frame_get(void);                            /* UI thread */
void lj_dmxin_emulate(int ch, int level, uint64_t now_us);                 /* Fingers "DMX in (1-12)" .. "(37-48)" (05 s3.3): channel 1..48 as one more source at priority 100, tables applied; level < 0 releases it */
void lj_dmxin_set_table(int ch, const uint8_t *map256);                    /* 1-based channel: the assigned .vtr copy (NULL = 1:1), applied from raw to out */

/* sACN data-packet reader (pure; the receiver half of design 8.1's packet table) */
typedef struct { uint8_t cid[16]; char name[64]; uint8_t priority, seq, options, start; uint16_t universe, n; const uint8_t *slots; } lj_sacn_rx;   /* name bounded and NUL-terminated (UTF-8 as sent); slots points into the packet */
int  lj_sacn_rx_parse(const uint8_t *pkt, size_t n, lj_sacn_rx *out);      /* 0 = a data packet that passed every check above, -1 = discard */

/* ---- configuration (Hardware.Ini [DMXIN] Enabled Source Universe Nic Loss FunctionsOff; ours, additive) ---- */
void        lj_dmxin_cfg_default(lj_dmxin_cfg *cfg);                       /* off, sACN, universe 1, hold */
void        lj_dmxin_cfg_load(lj_ini *hardware_ini, lj_dmxin_cfg *cfg);
void        lj_dmxin_cfg_save(lj_ini *hardware_ini, const lj_dmxin_cfg *cfg);
const char *lj_dmxin_cfg_validate(const lj_dmxin_cfg *cfg, const uint16_t out_universes[LJ_LINKS]);   /* NULL = usable; the universe range, an sACN input universe equal to an output universe */

/* ---- .vtr value tables (05 s3.3; ASCII CRLF: a header line, ID= GUID, TYPE=, NAME=, PURPOSE=, n=m lines 0..255; missing = 1:1) ---- */
enum { LJ_VTR_FILE_MAX = 16384, LJ_VTR_LINE_MAX = 256 };   /* PROPOSAL rig W5-DMXIN-6: a full table is ~2.5 KB */
typedef struct { char id[40]; char type[32]; char name[64]; char purpose[128]; uint8_t map[256]; } lj_vtr;   /* CP1252 */
void lj_vtr_identity(lj_vtr *t);                                           /* map[n] = n, empty strings */
void lj_vtr_linear(lj_vtr *t, int in_lo, int in_hi, int out_lo, int out_hi);   /* the editor's "set linear range" helper, integer rounding (no float) */
int  lj_vtr_parse(const char *text, size_t n, lj_vtr *out, char *err, size_t cap);   /* 0 / -1 with the reason TValueTableImportErrorForm shows; duplicate n PROPOSAL rule */
int  lj_vtr_load(const char *path, lj_vtr *out, char *err, size_t cap);    /* size checked before the read */
int  lj_vtr_save(const char *path, const lj_vtr *in);                      /* CRLF; every entry written */
int  lj_vtr_file_name(char *dst, size_t cap, const lj_vtr *t);             /* "<Type>_<Name>_<ID>.vtr"; -1 when a part holds a character a file name cannot */

/* ---- the DMX-in functions (05 s3.2). The words are this file's own (DmxIn.dat); captions come from the harvested DFM list. ---- */
#define LJ_DMXIN_FN_LIST(X) \
    X(NONE, "none") \
    X(CUE_BIT_1, "cue_bit_1") X(CUE_BIT_2, "cue_bit_2") X(CUE_BIT_4, "cue_bit_4") X(CUE_BIT_8, "cue_bit_8") \
    X(CUE_BIT_16, "cue_bit_16") X(CUE_BIT_32, "cue_bit_32") X(CUE_BIT_64, "cue_bit_64") X(CUE_BIT_128, "cue_bit_128") X(CUE_BIT_512, "cue_bit_512") \
    X(LATCH_CUE, "latch_cue") X(LATCH_PAGE_CUE, "latch_page_cue") X(PREV_CUE_PAGE, "prev_cue_page") X(NEXT_CUE_PAGE, "next_cue_page") \
    X(LATCH_PREV_PAGE_CUE, "latch_prev_page_cue") X(LATCH_NEXT_PAGE_CUE, "latch_next_page_cue") X(SELECT_PREV_PAGE_CUE, "select_prev_page_cue") X(SELECT_NEXT_PAGE_CUE, "select_next_page_cue") \
    X(FLASH_SEQ, "flash_seq") X(LATCH_CUE_N, "latch_cue_n") X(LATCH_CUELIST_N, "latch_cuelist_n") X(LATCH_BGCUE_N, "latch_bgcue_n") \
    X(HTP_GROUP, "htp_group") X(STATIC_ACTIVATE, "static_activate") X(STATIC_LEVEL, "static_level") X(SEQ_ONOFF, "seq_onoff") X(SEQ_INTENSITY, "seq_intensity") \
    X(MASTER_INTENSITY, "master_intensity") X(INTENSITY_GROUP, "intensity_group") X(BLACKOUT, "blackout") X(OVERWRITE_ENABLE, "overwrite_enable") \
    X(CL_GO, "cl_go") X(CL_BACK, "cl_back") X(CL_STOP, "cl_stop") X(CL_STOP_CLEAR, "cl_stop_clear") X(CL_GO_ALT, "cl_go_alt") \
    X(SMOKE_BUTTON, "smoke_button") X(SMOKE_LEVEL, "smoke_level") X(SMOKE_DURATION, "smoke_duration") X(SMOKE_CYCLE, "smoke_cycle") X(HAZE_LEVEL, "haze_level") X(HAZE_FAN, "haze_fan") \
    X(MACRO_AMP, "macro_amp") X(MACRO_SPEED, "macro_speed") X(MACRO_AMP_INV, "macro_amp_inv") X(MACRO_SPEED_INV, "macro_speed_inv") \
    X(CUE_TIME, "cue_time") X(SEQ_TIME, "seq_time") X(AUDIO_GAIN, "audio_gain") X(AUDIO_NOISE, "audio_noise") \
    X(TRIG_ALL_MANUAL_DEFAULT, "trig_all_manual_default") X(EMULATE_AUDIO_TRIG, "emulate_audio_trig")
#define LJ_DMXIN_FN_ENUM(id, word) LJ_DIF_##id,
enum lj_dmxin_fn { LJ_DMXIN_FN_LIST(LJ_DMXIN_FN_ENUM) LJ_DIF_COUNT };
#undef LJ_DMXIN_FN_ENUM
extern const char *const lj_dmxin_fn_words[LJ_DIF_COUNT];

enum lj_dmxin_time_mode { LJ_DMXIN_TIME_ALWAYS = 0, LJ_DMXIN_TIME_CHANGE1, LJ_DMXIN_TIME_CHANGE2, LJ_DMXIN_TIME_CHANGE3 };   /* Cue Time Control preference: always, or on a change of 1 / 2 / 3 steps */
typedef struct { uint8_t fn /*enum lj_dmxin_fn*/; uint32_t num /* record number (FLASH_SEQ, LATCH_*_N) or index (group, static, slot) */; uint8_t has_table; lj_vtr table; } lj_dmxin_assign;
typedef struct { lj_dmxin_assign ch[LJ_LINK_CH]; uint8_t time_mode /*enum lj_dmxin_time_mode*/; } lj_dmxin_map;   /* ~270 KB: the app keeps one, static */

void lj_dmxin_map_init(lj_dmxin_map *m);
int  lj_dmxin_map_load(const char *path, lj_dmxin_map *out, char *err, size_t cap);   /* missing file = empty map, 0 */
int  lj_dmxin_map_save(const char *path, const lj_dmxin_map *in);
/* UI thread, once per changed frame: threshold and edge rules of 05 s3.2 (level > 127 on; rising edges on 127 -> 128;
 * Cuelist Go (Alt) on its three transitions; statics on >= 50 %; binary cue bits + Latch Cue gate remote cue loads;
 * Overwrite Enable posts LJ_CMD_OVERWRITE_ENABLE, 1 while unassigned) run through lj_fn_run / lj_fn_run_prog and the
 * LJ_FNF_EXT level rows the track appends to io/lj_actions.h. map = the SAVED assignments only (new ones take effect
 * after Save, 05 s3.1); functions_off skips everything but the Global Patch path. Returns the functions fired. */
int  lj_dmxin_functions(const lj_dmxin_map *m, const uint8_t in[LJ_LINK_CH], int functions_off);
int  lj_dmxin_remote_cue(uint32_t *cue);                                   /* 1 while Latch Cue holds a valid binary cue (manual cue loads are overwritten by it) */

#endif /* LJ_IO_DMXIN_H */
