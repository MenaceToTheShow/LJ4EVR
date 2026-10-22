/* lj_engine.h - the 40 Hz integer tick (design 2.3, 3). No SDL, no float, no
 * <stdatomic.h>; the engine never allocates after lj_engine_create() and never
 * calls lj_table_* (tools/test_layering.py greps for it). Bodies: lj_engine.c
 * (create/bind/cmd/tick/view, the drain and the runner loop), lj_runner.c
 * (scene evaluation, fades, 16-bit pairs, movement macros), lj_merge.c (layer
 * stack), lj_post.c (S1-S9), lj_masters.c (GM/sub maths, GM fade runner).
 *
 * Thread-ownership contract (design 2.3, checked by tests/engine/test_ownership.c):
 *  1. Commands are a plain array of LJ_ENGINE_CMD_CAP lj_cmd appended under
 *     lj_lock() and drained at the start of the tick; a full array logs and drops.
 *  2. The UI thread owns every load; LJ_CMD_LOAD_CUE carries pointers to records
 *     it has already loaded and inserted into the lj_tables; seq[i] == NULL =
 *     "load error".
 *  3. Engine-initiated loads are requests in lj_engine_status.pending_*; the UI
 *     loop services them every pass.
 *  6. Records are freed only under the lock and only after the engine was handed
 *     a replacement or a NULL for every runner that pointed at them.
 *  7. lj_engine_frame() is the live buffer; the UI copies it under the lock.
 * Only the engine thread touches sACN sender state (ME-23): LJ_CMD_SACN_CFG parks
 * the new config in the engine and the engine thread (platform/sdl) collects it
 * with lj_engine_take_sacn_cfg() after the tick, outside the lock. */
#ifndef LJ_ENGINE_ENGINE_H
#define LJ_ENGINE_ENGINE_H

#include <stdint.h>
#include "core/lj_types.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_model.h"

typedef struct lj_engine lj_engine;

typedef struct { uint8_t out[LJ_LINKS][LJ_LINK_CH]; uint8_t merged[LJ_CHANS]; uint8_t layer_of[LJ_CHANS]; uint64_t tick_no; } lj_frame;

/* ---- runner ids: a, b of GO/STOP/TRIG/ENABLE/TIME name one of these ---- */
enum { LJ_RUN_SLOT0 = 0,                               /* cue slots 1-12 = LJ_RUN_SLOT0 + 0..11 */
       LJ_RUN_FLASH = LJ_CUE_SLOTS,                    /* the flash sequence */
       LJ_RUN_CURRENT,                                 /* the current sequence (Sequence control playback) */
       LJ_RUN_BG0,                                     /* background cue slots 1-5 = LJ_RUN_BG0 + 0..4 */
       LJ_RUN_STATIC0 = LJ_RUN_BG0 + LJ_BG_SLOTS,      /* active static slots, LJ_STATIC_RUNNERS of them */
       LJ_RUN_COUNT = LJ_RUN_STATIC0 + LJ_STATIC_RUNNERS,
       LJ_RUN_ALL = 0xFFFF };                          /* a = every cue slot (All Sequences Stop/Go, Trig All) */

/* ---- commands (design 3.2 step 1) ----
 * kind                 a                         b                          c            pointers
 * LOAD_CUE             cue number                edit (1 = Edit Cue: links do not fire)  -          cue, seq[12], bg, bgseq[5], gm; NULL seq[i] = load error [03 s5.1]
 * LOAD_BG              bg cue number             -                          -            bg, bgseq[5] (Background cue window / Statics link; bg NULL = clear)
 * LOAD_GENMACRO        macro number              restart (1)                -            gm (NULL = engine off for this cue)
 * CURRENT_SEQ          scene index (0-based)     blind (0/1)                -            seq = the pinned current sequence (NULL = none); posted after every scene change / New / Load
 * GO, STOP             runner id or LJ_RUN_ALL   -                          -            -        (STOP holds the fade, LJ_STOP_HOLDS_FADE)
 * TRIG                 runner id or LJ_RUN_ALL   enum lj_dir, or 0xFF = programmed direction   scenes (0 = slot's scenes_per_trig)   at_us = the UI timestamp (chase taps, 1 ms)
 * ENABLE               slot runner id            0/1                        -            -        (sequence on/off in the cue, Edit/Out flags via c: 0 normal, 1 edit, 2 out)
 * TIME                 0 = cue fader, 1..12 slot pct 0 (= CUE) .. 999       -            -        (applied at the next trig, LJ_TIME_SCALE_AT_TRIG_ONLY)
 * CHASE                slot runner id            0/1 chase manual           fade 0/1 (DisableChaseFade inverse)   period from the last two TRIG at_us
 * SET_MASTER           0 = GM, 1..8 = sub        level 0..255 (LJ_GM_DOMAIN) enum lj_master_mode (sub only) -
 * GM_FADE              0 = out, 1 = in           ms 500..10000              -            -        (GM Fade Out/In runner, 11 G11)
 * BLACKOUT             -                         0/1                        -            -
 * FREEZE               -                         0/1                        -            -        (LJ_FREEZE_PAUSED_CLOCK)
 * OFFLINE              -                         0/1                        -            -        (S9: frame computed, lj_sacn_set skipped by the engine thread)
 * SOLO                 fixture index             0/1 (solo = blackout of the unselected, S4)  -   -
 * STATIC               static runner slot id     0/1 active                 -            st = the pinned lj_static_slot (its scene, level, fade), NULL = release
 * FLASH                -                         0/1 held / latched         -            seq = the pinned flash sequence (NULL = release)
 * OVERWRITE            address code 0..2047      value 0..255, 256 = clear  -            -        (S8; the DMX Output Monitor; 0xFFFF a = clear all)
 * HTP_GROUP            group 1..24               level 0..255               -            -        (S5 g_k)
 * SEQ_INT              slot 1..12 (0 = mode)     level 0..255 (a = 0: enum lj_seq_int_mode)  -   -        (03 s5.9)
 * MACRO_CTL            amp pct 0..100            speed pct 0..100           -            -        (cue macro control, cue-slot runners only [03 s5.11])
 * DISABLE_FADE         -                         0/1                        -            -        (sequence-control Disable Fade: current-sequence runner snaps [11 F15])
 * SACN_CFG             -                         -                          -            sacn = the new lj_sacn_cfg (opaque here; io/lj_sacn.h), owned by the UI, applied by the engine thread (ME-23) */
enum lj_cmd_kind {
    LJ_CMD_NONE = 0,
    LJ_CMD_LOAD_CUE, LJ_CMD_LOAD_BG, LJ_CMD_LOAD_GENMACRO, LJ_CMD_CURRENT_SEQ,
    LJ_CMD_GO, LJ_CMD_STOP, LJ_CMD_TRIG, LJ_CMD_ENABLE, LJ_CMD_TIME, LJ_CMD_CHASE,
    LJ_CMD_SET_MASTER, LJ_CMD_GM_FADE, LJ_CMD_BLACKOUT, LJ_CMD_FREEZE, LJ_CMD_OFFLINE, LJ_CMD_SOLO,
    LJ_CMD_STATIC, LJ_CMD_FLASH, LJ_CMD_OVERWRITE, LJ_CMD_HTP_GROUP, LJ_CMD_SEQ_INT, LJ_CMD_MACRO_CTL, LJ_CMD_DISABLE_FADE,
    LJ_CMD_SACN_CFG,
    LJ_CMD_COUNT
};
typedef struct {
    uint8_t  kind;                       /* enum lj_cmd_kind */
    uint64_t at_us;                      /* UI-thread timestamp (lj_clock_us) so chase taps resolve to 1 ms, design 3.1; 0 = "now" */
    uint32_t a, b, c;                    /* per kind, table above */
    const lj_cue *cue; const lj_sequence *seq[LJ_CUE_SLOTS];               /* LOAD_CUE: pinned by the UI thread; seq[i] == NULL = "load error" */
    const lj_bgcue *bg; const lj_sequence *bgseq[LJ_BG_SLOTS];             /* LOAD_CUE / LOAD_BG */
    const lj_genmacro *gm;                                                 /* LOAD_CUE / LOAD_GENMACRO */
    const lj_static_slot *st;                                              /* STATIC */
    const void *sacn;                                                      /* SACN_CFG: const lj_sacn_cfg* (io/, not includable here) */
} lj_cmd;

/* ---- runner state (design 3.2) ---- */
enum { LJ_SLOT_EMPTY = 0, LJ_SLOT_RUNNING, LJ_SLOT_STOPPED, LJ_SLOT_LOAD_ERROR, LJ_SLOT_DISABLED, LJ_SLOT_EDIT, LJ_SLOT_OUT };
typedef struct {
    uint32_t seq_no;                     /* identity [03 s5.3]; 0 = empty */
    const lj_sequence *seq;              /* pinned by the last LOAD; NULL with seq_no != 0 = load error */
    uint16_t scene;                      /* current scene index, 0-based */
    uint16_t first, last;                /* limits (1-based in the slot, 0 = none; resolved here 0-based inclusive) */
    uint64_t scene_due_us, fade_t0_us, fade_dur_us;   /* absolute, drift-free [11 E10]; fade of the current scene (dur 0 = snap) */
    uint8_t  from[LJ_CHANS];             /* captured at trig from last tick's merged[], so an interrupted fade restarts from where it got to [11 F3, F6] */
    uint8_t  val[LJ_CHANS], st[LJ_CHANS];/* this tick's sparse contribution {value, lj_fstate}; LJ_OFF = transparent */
    uint8_t  dir, trig, end_action;      /* enum lj_dir / lj_trig / lj_end_action */
    uint8_t  bounce_back;                /* bounce direction flag */
    uint16_t loops_left, loop_count;     /* finite loops (0 = unlimited) and loops done */
    uint8_t  scenes_per_trig, spt_left, audio_per_trig, audio_left;
    uint16_t time_pct;                   /* 0 = CUE (slots), 1..999 */
    uint32_t chase_us; uint64_t tap_us[2]; uint8_t chase_on, chase_fade;
    uint32_t link_cue; uint16_t goto_scene;
    uint8_t  seq_int;                    /* sequence intensity 0..255 (255 = 100 %) [03 s5.9] */
    uint8_t  level_pct;                  /* statics: per-slot level */
    uint64_t activated_us;               /* statics: LTP order */
    unsigned enabled:1, stopped:1, edit:1, out:1, link:1, blind:1, running:1, flash:1, disable_fade:1;
} lj_runner;

/* ---- engine -> UI view, copied under lj_lock ---- */
typedef struct {
    uint8_t  slot_status[LJ_CUE_SLOTS];  /* LJ_SLOT_* */
    uint16_t slot_scene[LJ_CUE_SLOTS], slot_scenes[LJ_CUE_SLOTS];   /* scene X : Y per slot (1-based X) */
    uint16_t slot_time_pct[LJ_CUE_SLOTS], slot_loops[LJ_CUE_SLOTS];  /* time fader and loop counter per slot */
    uint8_t  slot_chase[LJ_CUE_SLOTS];   /* chase manual on */
    uint32_t chase_ms;                   /* the global chase period (carries across cue loads [03 s5.5]) */
    uint16_t cur_scene, cur_scenes;      /* current-sequence runner: scene X:Y */
    uint32_t cur_scene_ms, cur_scene_elapsed_ms, cur_fade_ms, cur_fade_elapsed_ms;   /* W_PROGRESS2 of the Sequence window (LORD_FIDELITY-14): blue = scene, red = fade */
    uint8_t  cur_running, cur_dir, cur_blind;
    uint8_t  flash_status;               /* LJ_SLOT_* */
    uint8_t  bg_status[LJ_BG_SLOTS]; uint16_t bg_scene[LJ_BG_SLOTS], bg_scenes[LJ_BG_SLOTS];
    uint8_t  static_active[LJ_STATIC_RUNNERS];   /* statics LEDs (toolbar LED green when any) */
    uint8_t  statics_any, bg_any;
    uint8_t  gm, sub[LJ_SUBMASTERS];     /* masters 0..255 (LJ_GM_DOMAIN) */
    uint8_t  gm_fade;                    /* 0 none, 1 FADE-OUT ACTIVE, 2 FADE-IN ACTIVE */
    uint8_t  blackout, freeze, offline, solo;
    uint8_t  cue_edit;                   /* the running cue was loaded with edit = 1 ("EDIT CUE " caption prefix, F19) */
    uint32_t cue_number;                 /* running cue, 0 = none */
    uint32_t tick_us_last, tick_us_max;  /* measured tick duration, microseconds (Debug window / Hardware Setup status line) */
    uint32_t drops;                      /* commands dropped because the array was full */
    uint32_t overruns;                   /* lj_log_overrun count (ticks that ran late) */
    uint64_t tick_no;
    uint32_t pending_link_cue, pending_bg, pending_genmacro;        /* engine -> UI requests, 0 = none; cleared once handed out */
    uint32_t sacn_pps[LJ_LINKS];         /* packets/s per link's universe (filled by the engine thread through lj_engine_set_sacn_stats) */
    int      sacn_last_err;              /* last sendto errno / WSAGetLastError, 0 = none */
} lj_engine_status;   /* design 2.3 names this struct lj_engine_view; C puts typedef and function names in one namespace, so the TYPE is lj_engine_status and the FUNCTION keeps the design name */

/* ---- post-processing context (design 3.5): what S1-S9 read, rebuilt at bind and by commands ---- */
typedef struct {
    const lj_show *show;                          /* patch[], default_out[], fixture flags, submaster modes */
    const lj_profile *profile_of[LJ_FIXTURES];    /* resolved at bind: lj_fixlib_by_id, or the UI-supplied GS2 profile (lj_engine_bind_profile) */
    int16_t  fixture_of[LJ_CHANS];                /* -1 unpatched, else the fixture index; offset = ch - base */
    uint8_t  gm, sub[LJ_SUBMASTERS];
    uint8_t  blackout, freeze, offline, solo; int solo_fixture;
    uint8_t  ovr_on[LJ_CHANS], ovr_val[LJ_CHANS]; /* S8 overwrites */
    uint8_t  dmx_in[LJ_CHANS]; uint8_t overwrite_enable;   /* S1 DMX-in source (W later: always 0 / enabled) */
    uint8_t  htp_level[LJ_HTP_GROUPS + 1]; uint8_t htp_masters;   /* S5 g_k (1-based) and "Enable HTP group master intensity control" [11 P8] */
    uint8_t  frozen_s0[LJ_CHANS];                 /* S9: the S0 input held while frozen */
} lj_post_ctx;

/* ---- the function set (design 2.3) ---- */
lj_engine      *lj_engine_create(void);                              /* fixed-size arenas; never allocates after this; NULL on failure */
void            lj_engine_destroy(lj_engine *e);                     /* after the engine thread has stopped; NULL is a no-op */
void            lj_engine_bind(lj_engine *e, const lj_show *show);   /* caller holds lj_lock; rebuilds the post ctx (fixture_of, profile_of, default_out); NULL unbinds */
void            lj_engine_bind_profile(lj_engine *e, int fixture, const lj_profile *p);   /* caller holds lj_lock; a .GS2 fixture's profile the UI loaded (built-ins resolve themselves in bind) */
void            lj_engine_rebind(lj_engine *e);                      /* caller holds lj_lock; after Config.dat changed (addresses, flags): same as bind with the current show */
int             lj_engine_cmd(lj_engine *e, const lj_cmd *cmd);      /* caller holds lj_lock; appends to the array drained at the next tick; -1 + log when full */
void            lj_engine_tick(lj_engine *e, uint64_t now_us);       /* one calculation pass (design 3.2); pure given its inputs; caller holds lj_lock */
const lj_frame *lj_engine_frame(const lj_engine *e);                 /* the live buffer; the UI copies it under lj_lock */
void            lj_engine_view(lj_engine *e, lj_engine_status *out);        /* copied under lj_lock; clears the pending_* fields it hands out (so not const) */
const lj_runner *lj_engine_runner(const lj_engine *e, int id);       /* read-only runner state for tests and the Debug window; NULL when id >= LJ_RUN_COUNT */
const lj_post_ctx *lj_engine_post_ctx(const lj_engine *e);
uint32_t        lj_engine_tick_us(const lj_engine *e);               /* the tick period the engine was told (lj_engine_set_tick_us; default LJ_TICK_US_DEFAULT) */
void            lj_engine_set_tick_us(lj_engine *e, uint32_t tick_us);   /* clamped to LJ_TICK_HZ_MIN..MAX (ME-24) */

/* Engine-thread hooks (platform/sdl, design 3.1 loop; tests call them directly). */
int             lj_engine_is_offline(const lj_engine *e);            /* read after the tick under the lock: 1 = skip lj_sacn_set this tick (S9) */
const void     *lj_engine_take_sacn_cfg(lj_engine *e);               /* the lj_sacn_cfg parked by LJ_CMD_SACN_CFG, handed out once (NULL = none); caller holds lj_lock and copies the record before lj_unlock() (ME-69: the UI rewrites it under the lock); the thread then does close-old / open-new on its copy outside it (ME-23) */
void            lj_engine_set_sacn_stats(lj_engine *e, const uint32_t pps[LJ_LINKS], int last_err);   /* caller holds lj_lock; copied into the view */

/* Fade maths (design 3.3), shared with the replay harness and tests. */
static inline uint32_t lj_fade_value(uint32_t from, uint32_t to, uint64_t now, uint64_t t0, uint64_t dur)
{
    uint64_t e, m; int64_t d; uint32_t step;
    if (now <= t0) return from;                      /* a trig timestamped after this tick, or t0 moved by unfreeze: no wrap */
    if (dur == 0 || now >= t0 + dur) return to;
    e = now - t0;
    d = (int64_t)to - (int64_t)from;
    m = (uint64_t)(d < 0 ? -d : d);                  /* m <= 65535, e < dur <= 60 000 000 us: product < 2^52 */
#if LJ_FADE_ROUND_NEAREST
    step = (uint32_t)((m * e + dur / 2) / dur);      /* round the magnitude, so up- and down-fades mirror */
#else
    step = (uint32_t)((m * e) / dur);
#endif
    return d < 0 ? from - step : from + step;
}

#endif /* LJ_ENGINE_ENGINE_H */
