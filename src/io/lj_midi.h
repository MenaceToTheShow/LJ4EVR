/* lj_midi.h - MIDI in (design 2.5, 8.3; 05 s7; W2). Note on/off -> flash sequence /
 * latch cue / latch BG cue / cue list / hotkey function, per MIDI channel 1-16 or All,
 * with Learn; velocity 0 = note off (Note0); Release-on-Note-Off options per kind;
 * CC / program change / clock are marked extensions, off by default (05 s7 clone
 * verdict). MTC quarter-frame decode is W3 (cue list) and lands in this module.
 *
 * Threads (design 2.3 rule 5, ME-23): the OS callback (WinMM / CoreMIDI thread) only
 * appends the 3-byte message to a fixed queue under lj_lock(); the UI loop drains it
 * every idle pass (lj_midi_poll) and runs the mapped function through lj_fn_run /
 * lj_fn_run_prog on the UI thread. No engine or sender call happens on the MIDI
 * thread (grep: no lj_engine_* / lj_sacn_* in io/lj_midi_*.c). The same queue serves
 * every backend (the design names SDL_PushEvent for the OS backends; the 10 ms idle
 * wake bounds the latency, so one plain queue is enough: ponytail, add a platform
 * wake if a measured latency says so).
 *
 * Backends: lj_midi_winmm.c (midiInOpen / midiInStart, Windows), lj_midi_coremidi.c
 * (MIDIClientCreate / MIDIInputPortCreate, macOS), lj_midi_null.c (Linux and tests:
 * no devices). CMake compiles exactly one; each defines lj_midi_list / open / close.
 * The map, the queue, Learn, inject and the files are lj_midi.c (every OS).
 *
 * Files (per user library, 05 s7): MIDIIn\MIDIIn.Ini keeps LJ2's shipped keys
 * verbatim ([SETUP] DeviceCode Note0 FlashOff LatchOff ListOff BGLatchOff SeqOff;
 * [SEQ] SeqEnable SeqChannel; [CUE] CueEnable CueChannel; [BGCUE] BGCueEnable
 * BGCueChannel; [CUELIST] CueListEnable CueListChannel; [FUNCTIONS] FunctionsEnable
 * FunctionsChannel; [OPTIONS] OnTop StartMinimized AutoSave; channel 16 = All as
 * shipped); the note tables are ours, MIDIIn\MIDIMap.dat in the design 5.2 grammar:
 *   LJ4 midimap 1
 *   map kind=seq ch=0 note=60 num=12
 *   map kind=fn ch=1 note=61 fn=LJ_FN_BLACKOUT_RESTORE
 * kind seq|cue|bgcue|cl|fn, ch 0 = All or 1..16, note 0..127 (lj_kv_range, ME-77),
 * num = the record number (seq/cue/bgcue/cl), fn = the lj_fn_table name.
 *
 * W2 midi track additions (docs/midi.md): kind cc is the marked extension (05 s7 clone
 * verdict, off by default: [CC] CCEnable=0 CCChannel=16 in MIDIIn.Ini are ours); its
 * `note` is the controller number and its fn gets the value scaled 0..127 -> 0..100 as
 * prog_num (map kind=cc ch=0 note=7 fn=LJ_FN_MASTER_LEVEL = CC 7 drives the Grand
 * Master). A release (note off) fires only for the note that last fired that kind (05 s7:
 * "a note off command corresponding to the last ... note on"); Release Cue / BGCue /
 * Cuelist run LJ_FN_NEW_CLEAR_CUE / LJ_FN_CLEAR_BGCUE / LJ_FN_CUELIST_STOP_AND_CLEAR,
 * Release Seq is the flash release (edge 0). lj_midi_post drops a malformed message
 * (status < 0x80 or a data byte > 0x7F, counted in lj_midi_dropped) and ignores system
 * messages (status >= 0xF0: clock, active sensing; MTC quarter-frame is W3). */
#ifndef LJ_IO_MIDI_H
#define LJ_IO_MIDI_H

#include <stddef.h>
#include <stdint.h>
#include "io/lj_actions.h"

enum { LJ_MIDI_NAME_CAP = 64, LJ_MIDI_MAP_MAX = 512, LJ_MIDI_CH_ALL = 0,
       LJ_MIDI_QUEUE = 256 /* 3-byte events parked by the OS thread until the UI pass drains them (design 2.3 rule 5); a full queue drops and counts */ };

typedef struct { int idx; char name[LJ_MIDI_NAME_CAP]; } lj_midi_dev;   /* idx = the backend's device index */

enum lj_midi_kind { LJ_MIDI_SEQ = 0 /* flash sequence while the note is held (SeqOff: release on note off) */, LJ_MIDI_CUE /* latch cue (LatchOff) */,
                    LJ_MIDI_BGCUE /* latch BG cue (BGLatchOff) */, LJ_MIDI_CL /* load/start cue list (ListOff), W3 */, LJ_MIDI_FN /* hotkey function: note on = edge 1, note off = edge 0 */,
                    LJ_MIDI_CC /* W2 extension, off by default: control change -> fn with the value in percent as prog_num */,
                    LJ_MIDI_KIND_COUNT };
typedef struct { uint8_t kind /*enum lj_midi_kind*/, ch /*0 = All, 1..16*/, note /*0..127*/; uint16_t fn /*enum lj_fn, LJ_MIDI_FN*/; uint32_t num /*record number*/; } lj_midi_map_entry;
typedef struct {
    uint8_t  enabled[LJ_MIDI_KIND_COUNT];   /* [SEQ] SeqEnable ... per kind */
    uint8_t  channel[LJ_MIDI_KIND_COUNT];   /* per kind, 16 = All as LJ2 ships it; 1..16 */
    uint8_t  note0_is_off;                  /* [SETUP] Note0 (default 1) */
    uint8_t  release_on_off[LJ_MIDI_KIND_COUNT];   /* FlashOff / LatchOff / BGLatchOff / ListOff / (functions always edge) */
    uint8_t  on_top, start_minimized, auto_save;   /* [OPTIONS] */
    int      device;                        /* [SETUP] DeviceCode: the opened device index, -1 none */
    int      n; lj_midi_map_entry e[LJ_MIDI_MAP_MAX];
} lj_midi_map;

/* ---- backend (one of the three .c files; the callback runs on the OS MIDI thread and only posts) ---- */
int  lj_midi_list(lj_midi_dev *out, int max);                                       /* device count (0 when none), -1 on error */
int  lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud);  /* 0 / -1 + log; one device at a time */
void lj_midi_close(void);

/* ---- the common part (lj_midi.c): queue, map, learn, dispatch, files ---- */
void lj_midi_init(lj_midi_map *m);                                                  /* LJ2's shipped defaults: everything disabled, channels 16, Note0 1, FlashOff 1, SeqOff 1, OnTop 1, AutoSave 1 */
int  lj_midi_load(const char *ini_path, const char *map_path, lj_midi_map *out, char *err, size_t cap);   /* MIDIIn.Ini (missing = defaults) + MIDIMap.dat (missing = empty); -1 only when a present file is bad */
int  lj_midi_save(const char *ini_path, const char *map_path, const lj_midi_map *in);
const char *lj_midi_validate(const lj_midi_map *in);                                /* NULL = valid: kinds / channels / notes in range, fn in the table for LJ_MIDI_FN / LJ_MIDI_CC, num != 0 otherwise */
int  lj_midi_assign(lj_midi_map *m, const lj_midi_map_entry *e);                   /* replaces an entry with the same kind/ch/note, else appends; -1 when full or invalid */
int  lj_midi_unassign(lj_midi_map *m, enum lj_midi_kind kind, int ch, int note);
const lj_midi_map_entry *lj_midi_match(const lj_midi_map *m, int kind_mask, int ch, int note);   /* the entry that fires: exact channel first, then All; kind_mask = bits (1 << kind), 0 = every kind; NULL when none */

void lj_midi_post(const uint8_t msg[3]);                                            /* the backend callback's only act: append under lj_lock (full = dropped + counted) */
int  lj_midi_poll(uint8_t msg[3]);                                                  /* UI thread: 1 = one message produced (under lj_lock), 0 = empty */
uint32_t lj_midi_dropped(void);                                                     /* queue drops since start (the MIDI Monitor tab / Debug window) */
void lj_midi_inject(const uint8_t msg[3]);                                          /* test seam: the UI script verb `midi 90 3C 7F` and tests/io/test_midi_map.c run the same post path as a device */

/* Learn: the next note on the UI thread sees (after lj_midi_learn_begin) is reported once and consumed. */
void lj_midi_learn_begin(void);
void lj_midi_learn_cancel(void);
int  lj_midi_learn_take(int *ch, int *note);                                        /* 1 when a note arrived since begin (learn ends), 0 while waiting / not learning */

/* Dispatch on the UI thread (lj_app_midi_dispatch calls it per polled message): decodes note on/off (Note0), applies the
 * enable / channel gates of the map, runs lj_fn_run_prog(LJ_FN_PROG_*, num, edge) for seq / cue / bgcue / cl and
 * lj_fn_run(fn, edge) for LJ_MIDI_FN; while learning, the first note on feeds Learn instead. Returns the entry fired or NULL. */
const lj_midi_map_entry *lj_midi_dispatch(const lj_midi_map *m, const uint8_t msg[3]);

extern const char *const lj_midi_kind_words[];   /* seq cue bgcue cl fn cc (the map= file words and the MIDI-IN window tabs; cc is the extension, no LJ2 tab), NULL-terminated */

/* ---- W3 (appended, docs/w3-contracts.md; cuelist-core track): MIDI time code for the cue list [03 s6.4, design 8.3] ----
 * Quarter-frame messages (F1 0n..7n) only, not SysEx full frames; 24 / 25 / 30df / 30 fps from piece 7. The MIDI path
 * hands every F1 data byte to lj_mtc_feed on the UI thread (lj_app_midi_dispatch, ME-23: never in the OS callback); the
 * cue list runner reads lj_mtc_time each 10 ms step. LORD_C-6: the CoreMIDI byte-stream parser moves to lj_midi_feed so
 * quarter-frames ride the same parser on both OSes. */
typedef struct { uint8_t piece[8], have; uint8_t fps /* 24 25 29 (=30df) 30, 0 = unknown */; uint32_t ms; uint64_t last_us; uint8_t running;
                 /* W6 INT-s7-SYNCFRAMES (appended): Main.Ini [CUELIST2] SyncFrames (0 Off, 1, 5, 10; PROPOSAL rig W6-SYNC): lj_mtc_time
                  * follows only after that many frames (4 quarter frames each) since the time code (re)started, a gap past the
                  * time-out restarting the count */
                 uint8_t sync_frames; uint16_t qf_run; } lj_mtc;
void lj_mtc_reset(lj_mtc *t);
int  lj_mtc_feed(lj_mtc *t, uint8_t qf_data, uint64_t now_us);    /* one quarter-frame data byte; 1 when a full time was assembled (t->ms valid), else 0 */
int  lj_mtc_time(const lj_mtc *t, uint64_t now_us, uint32_t *ms); /* 0 with the last time, -1 when none or older than LJ_MTC_TIMEOUT_MS (the time-out preference acts on -1) */
int  lj_midi_feed(const uint8_t *bytes, size_t n, uint8_t run[3], int *have, void (*msg)(const uint8_t m[3], void *ud), void *ud);   /* LORD_C-6 byte-stream parser (running status, F1 carried as {F1, data, 0}); messages emitted */

/* ---- W4 da2532-fingers (appended, D16): Learn for the LJ_FNF_EXT rows (the 2532 keys, the Fingers surface). The MIDI-IN
 * window's Learn is unchanged (notes only); this variant may also take a control change (the fader / GM / jog level rows),
 * reports which, and lj_midi_learn_bind writes the `fn` (note) or `cc` entry at the learnt channel and note, enabling that
 * kind when it was off (logged) so the binding fires at once. UI thread only, like Learn. */
void lj_midi_learn_begin_ext(int want_cc);
int  lj_midi_learn_take_ext(int *ch, int *note, int *is_cc);                        /* 1 once after a capture (is_cc = 1 for a CC), else 0 */
int  lj_midi_learn_bind(lj_midi_map *m, enum lj_fn fn, int ch, int note, int is_cc); /* 0 / -1 (bad fn / range, map full) */

#endif /* LJ_IO_MIDI_H */
