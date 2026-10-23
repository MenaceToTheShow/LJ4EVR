/* lj_audio.h - the Audio Analyzer and audio trig (design 1, 10 W5 SDL_AUDIO; note 05 s9.1; docs/w5-contracts.md seam 4;
 * track audio). LJ2's LJSound.exe becomes a built-in module: SDL-free and integer only (hard rule 1).
 *
 * Threads (the MIDI posting pattern, design 2.3 rule 5 / 8.3): the capture glue (platform/sdl/lj_sdl_audio.c, an SDL3
 * recording stream at LJ_AUDIO_RATE mono S16) only calls lj_audio_post from SDL's audio thread, a bounded copy into a
 * fixed ring under lj_lock (full = dropped and counted). The UI loop (app/lj_app_io.c) drains it with lj_audio_poll and
 * runs lj_audio_analyze; every trig it returns becomes LJ_CMD_TRIG a = LJ_RUN_ALL, b = LJ_TRIG_AUDIO_EVENT posted by
 * the app, the same command Emulate Audio Trig posts (engine/lj_engine.h; the engine counts Audio-Trigs / Trig). The
 * analyzer never touches the engine or a window; the Analyzer window reads ctx->audio_state on its 100 ms refresh and
 * the DMX-in Gain / Noise Threshold functions and the Fingers jogs write ctx->audio (W5-AUD-6).
 *
 * Detector (05 s9.1): a level-ratio onset detector, independent of absolute level: a trig when the current block level
 * exceeds the average of earlier levels times the sensitivity factor (2.0 .. 10.0) and the noise threshold, and the
 * last trig is older than the decay (~0.05 .. 0.6 s). Input gain scales the graph and the threshold comparison only.
 * The 16-band equalizer (Use Equalizer) weights an integer FFT whose twiddle and window tables come from
 * tools/gen_vectors.py (no <math.h>). Every constant the notes leave open is a PROPOSAL tag.
 *
 * Settings: Hardware.Ini [AUDIO] Enabled Device (the Hardware Setup tsAudio page; 0 on a fresh userdata, W5-X-OFF) and
 * the analyzer's own keys (LJSound.Ini key names if the harvested exe has them, else ours, tagged). */
#ifndef LJ_IO_AUDIO_H
#define LJ_IO_AUDIO_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_ini.h"

enum {
    LJ_AUDIO_RATE = 44100,         /* capture rate, PROPOSAL rig W5-AUD-1 */
    LJ_AUDIO_BLOCK = 512,          /* samples per analysis block (~11.6 ms), PROPOSAL rig W5-AUD-2 */
    LJ_AUDIO_BANDS = 16,           /* the Equalizer (05 s9.1) */
    LJ_AUDIO_TIMELINE = 500,       /* Time-line view: last 5.8 s = 500 blocks at 44100 / 512 */
    LJ_AUDIO_RING = 16384,         /* samples parked between two UI passes (~370 ms) */
    LJ_AUDIO_DEV_NAME = 64
};
enum lj_audio_range { LJ_AUDIO_NOISE_LOW = 0, LJ_AUDIO_NOISE_MEDIUM, LJ_AUDIO_NOISE_HIGH };   /* Trigger menu */
enum lj_audio_view  { LJ_AUDIO_VIEW_NONE = 0, LJ_AUDIO_VIEW_REALTIME, LJ_AUDIO_VIEW_TIMELINE };

typedef struct { char name[LJ_AUDIO_DEV_NAME]; } lj_audio_dev;   /* CP1252 (converted from SDL's UTF-8 in platform/) */
typedef struct {
    uint8_t  enabled;              /* Hardware Setup Audio-in */
    char     device[LJ_AUDIO_DEV_NAME];   /* "" = the default device (listed first) */
    uint8_t  noise_range;          /* enum lj_audio_range */
    uint8_t  noise;                /* threshold 0..255 inside the range */
    uint16_t decay_ms;             /* 50..600 */
    uint8_t  sensitivity_x10;      /* 20..100 = factor 2.0 .. 10.0 */
    uint16_t gain_pct;             /* input gain, 100 = unity, PROPOSAL range 10..1000 */
    uint8_t  use_eq; uint8_t eq[LJ_AUDIO_BANDS];   /* 0..255 per band, 128 = flat (Set default) */
    uint8_t  view;                 /* enum lj_audio_view */
    uint8_t  auto_save, pass_keys, run_minimized, on_top, show_input, trigger_faders;   /* the Analyzer / View menus */
} lj_audio_cfg;
typedef struct {
    uint16_t level;                /* last block, 0..32767 after gain */
    uint16_t band[LJ_AUDIO_BANDS];
    uint8_t  clip;                 /* 0 none, 1 near clip (yellow), 2 clipped (red) */
    uint16_t avg;                  /* the detector's running average of earlier levels */
    uint64_t last_trig_us;
    uint32_t trigs;                /* since reset (the status-bar Trig indicator flashes on a change) */
    uint16_t timeline[LJ_AUDIO_TIMELINE]; int tl_pos;   /* ring of block levels */
} lj_audio_state;

void lj_audio_cfg_default(lj_audio_cfg *c);                                /* off, default device, Medium, decay 200 ms, factor 3.0, gain 100, EQ flat (PROPOSAL rig W5-AUD-2) */
void lj_audio_cfg_load(lj_ini *hardware_ini, lj_audio_cfg *c);
void lj_audio_cfg_save(lj_ini *hardware_ini, const lj_audio_cfg *c);

void lj_audio_reset(lj_audio_state *s);
int  lj_audio_detect(lj_audio_state *s, const lj_audio_cfg *c, uint16_t level, uint64_t now_us);   /* the onset rule on one block level: 1 = trig; the UI script `audio_inject` feeds this */
int  lj_audio_analyze(lj_audio_state *s, const lj_audio_cfg *c, const int16_t *pcm, size_t n, uint64_t now_us);   /* whole blocks of n samples (a remainder is kept for the next call): level, clip, bands, detect; returns the trigs */

void     lj_audio_post(const int16_t *pcm, size_t n);                      /* the capture thread's only act: copy under lj_lock into the ring */
size_t   lj_audio_poll(int16_t *pcm, size_t cap);                          /* UI thread: samples taken from the ring */
uint32_t lj_audio_dropped(void);

#endif /* LJ_IO_AUDIO_H */
