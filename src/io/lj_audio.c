/* lj_audio.c - the Audio Analyzer (io/lj_audio.h): SDL-free, integer only (hard rule 1). The tables come from
 * io/lj_audio_gen.c (tools/gen_vectors.py --audio); the rules and every PROPOSAL number are in the header. */
#include <stdio.h>
#include <string.h>
#include "io/lj_audio.h"
#include "core/lj_lock.h"
#include "core/lj_str.h"

#define LJ_AU_SEC "AUDIO"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void lj_audio_cfg_default(lj_audio_cfg *c)
{
    int i;
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->noise_range = LJ_AUDIO_NOISE_MEDIUM; c->noise = 16; c->decay_ms = 312; c->sensitivity_x10 = 60; c->gain_pct = 100;   /* the DFM fader defaults: TrackBar1 13 x 24 ms, TrackBar2 150 of 300, ScrollBar1 0 dB; noise PROPOSAL rig W5-AUD-2 */
    for (i = 0; i < LJ_AUDIO_BANDS; i++) c->eq[i] = 128;
    c->view = LJ_AUDIO_VIEW_REALTIME; c->trigger_faders = 1; c->sound_card = 1; c->on_top = 1;   /* the DFM's Tag = 1 items (Real time, Trigger Faders, Sound Card, Stay On Top) */
}

void lj_audio_cfg_load(lj_ini *ini, lj_audio_cfg *c)
{
    char key[24];
    int i;
    if (!c) return;
    lj_audio_cfg_default(c);
    if (!ini) return;   /* Hardware.Ini is user-editable: every value is clamped (W5-X-HOSTILE) */
    c->enabled = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "Enabled", 0) != 0);
    lj_str_copy(c->device, sizeof c->device, lj_ini_gets(ini, LJ_AU_SEC, "Device", ""));
    c->use_eq = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "UseFFT", 0) != 0);
    c->noise_range = (uint8_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "NoiseLevelSwitch", c->noise_range), 0, 2);
    c->noise = (uint8_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "NoiseLevelValue", c->noise), 0, 255);
    c->decay_ms = (uint16_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "TrigDecay", c->decay_ms), LJ_AUDIO_DECAY_MIN, LJ_AUDIO_DECAY_MAX);
    c->sensitivity_x10 = (uint8_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "TrigAggr", c->sensitivity_x10), LJ_AUDIO_SENS_MIN, LJ_AUDIO_SENS_MAX);
    c->gain_pct = (uint16_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "Gain", c->gain_pct), LJ_AUDIO_GAIN_MIN, LJ_AUDIO_GAIN_MAX);
    for (i = 0; i < LJ_AUDIO_BANDS; i++) { { if (snprintf(key, sizeof key, "EqualizerBand%d", i + 1) < 0) key[0] = 0; } c->eq[i] = (uint8_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, key, 128), 0, 255); }
    c->view = (uint8_t)clampi(lj_ini_geti(ini, LJ_AU_SEC, "ShowInput", c->view), 0, 2);
    c->trigger_faders = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "TriggerFaders", c->trigger_faders) != 0);
    c->sound_card = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "SoundCard", c->sound_card) != 0);
    c->on_top = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "StayOnTop", c->on_top) != 0);
    c->run_minimized = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "RunMinimized", 0) != 0);
    c->auto_save = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "AutoSaveChanges", 0) != 0);
    c->pass_keys = (uint8_t)(lj_ini_geti(ini, LJ_AU_SEC, "PassKeys", 0) != 0);
}

void lj_audio_cfg_save(lj_ini *ini, const lj_audio_cfg *c)
{
    char key[24];
    int i;
    if (!ini || !c) return;
    lj_ini_seti(ini, LJ_AU_SEC, "Enabled", c->enabled);
    lj_ini_sets(ini, LJ_AU_SEC, "Device", c->device);
    lj_ini_seti(ini, LJ_AU_SEC, "UseFFT", c->use_eq);
    lj_ini_seti(ini, LJ_AU_SEC, "NoiseLevelSwitch", c->noise_range);
    lj_ini_seti(ini, LJ_AU_SEC, "NoiseLevelValue", c->noise);
    lj_ini_seti(ini, LJ_AU_SEC, "TrigDecay", c->decay_ms);
    lj_ini_seti(ini, LJ_AU_SEC, "TrigAggr", c->sensitivity_x10);
    lj_ini_seti(ini, LJ_AU_SEC, "Gain", c->gain_pct);
    for (i = 0; i < LJ_AUDIO_BANDS; i++) { { if (snprintf(key, sizeof key, "EqualizerBand%d", i + 1) < 0) key[0] = 0; } lj_ini_seti(ini, LJ_AU_SEC, key, c->eq[i]); }
    lj_ini_seti(ini, LJ_AU_SEC, "ShowInput", c->view);
    lj_ini_seti(ini, LJ_AU_SEC, "TriggerFaders", c->trigger_faders);
    lj_ini_seti(ini, LJ_AU_SEC, "SoundCard", c->sound_card);
    lj_ini_seti(ini, LJ_AU_SEC, "StayOnTop", c->on_top);
    lj_ini_seti(ini, LJ_AU_SEC, "RunMinimized", c->run_minimized);
    lj_ini_seti(ini, LJ_AU_SEC, "AutoSaveChanges", c->auto_save);
    lj_ini_seti(ini, LJ_AU_SEC, "PassKeys", c->pass_keys);
}

int lj_audio_threshold(const lj_audio_cfg *c)
{
    static const int mul[3] = { 2, 8, 32 };   /* Low / Medium / High: up to 510 / 2040 / 8160 of 32767, PROPOSAL rig W5-AUD-2 */
    return c->noise * mul[c->noise_range < 3 ? c->noise_range : 1];
}
int lj_audio_gain_pos(const lj_audio_cfg *c)
{
    int p, best = 0, bd = 1 << 30;
    for (p = 0; p < 255; p++) { int d = lj_audio_gain_pct[p] - c->gain_pct; if (d < 0) d = -d; if (d < bd) { bd = d; best = p; } }
    return best - 127;
}
void lj_audio_set_gain_pos(lj_audio_cfg *c, int pos) { if (c) c->gain_pct = lj_audio_gain_pct[clampi(pos, -127, 127) + 127]; }
void lj_audio_set_noise(lj_audio_cfg *c, int noise) { if (c) c->noise = (uint8_t)clampi(noise, 0, 255); }

void lj_audio_reset(lj_audio_state *s) { if (s) memset(s, 0, sizeof *s); }

int lj_audio_detect(lj_audio_state *s, const lj_audio_cfg *c, uint16_t level, uint64_t now_us)
{
    uint32_t gl, decay_us;
    int trig;
    if (!s || !c) return 0;
    if (level > 32767) level = 32767;
    gl = (uint32_t)level * c->gain_pct / 100u;           /* gain: the graph and the threshold comparison only (05 s9.1) */
    if (gl > 32767) gl = 32767;
    decay_us = (uint32_t)clampi(c->decay_ms, LJ_AUDIO_DECAY_MIN, LJ_AUDIO_DECAY_MAX) * 1000u;
    trig = (int)gl > lj_audio_threshold(c)
        && (uint32_t)level * 10u > (uint32_t)s->avg * (uint32_t)clampi(c->sensitivity_x10, LJ_AUDIO_SENS_MIN, LJ_AUDIO_SENS_MAX)
        && (!s->trigs || now_us < s->last_trig_us || now_us - s->last_trig_us >= decay_us);
    s->avg = (uint16_t)(((uint32_t)s->avg * 7u + level) / 8u);
    s->level = (uint16_t)gl;
    if (trig) { s->trigs++; s->last_trig_us = now_us; }
    if (s->tl_pos < 0 || s->tl_pos >= LJ_AUDIO_TIMELINE) s->tl_pos = 0;
    s->timeline[s->tl_pos] = (uint16_t)(gl | (trig ? 0x8000u : 0u));
    s->tl_pos = (s->tl_pos + 1) % LJ_AUDIO_TIMELINE;
    return trig;
}

/* Hann window, then an in-place radix-2 decimation-in-time FFT with a 1/2 scale per stage (9 stages = 1/512, so int32
 * never overflows: |x| <= 32767 throughout); magnitude ~ max + min/2 (within 12 %). */
void lj_audio_fft_bands(const int16_t pcm[LJ_AUDIO_BLOCK], uint16_t band[LJ_AUDIO_BANDS])
{
    static int32_t re[LJ_AUDIO_BLOCK], im[LJ_AUDIO_BLOCK];   /* UI thread only */
    int i, j, b, h;
    for (i = 0; i < LJ_AUDIO_BLOCK; i++) {
        int r = 0, k = i;
        for (j = 0; j < 9; j++) { r = (r << 1) | (k & 1); k >>= 1; }
        re[r] = (int32_t)(((int64_t)pcm[i] * lj_audio_hann[i]) >> 15); im[r] = 0;
    }
    for (h = 1; h < LJ_AUDIO_BLOCK; h <<= 1) {
        int step = LJ_AUDIO_BLOCK / (2 * h);
        for (i = 0; i < LJ_AUDIO_BLOCK; i += 2 * h)
            for (j = 0; j < h; j++) {
                int a = i + j, z = a + h;
                int64_t wc = lj_audio_cos[j * step], ws = lj_audio_sin[j * step];   /* w = e^(-i 2 pi k / N) = cos - i sin */
                int32_t tr = (int32_t)((re[z] * wc + im[z] * ws) >> 15), ti = (int32_t)((im[z] * wc - re[z] * ws) >> 15);
                re[z] = (re[a] - tr) / 2; im[z] = (im[a] - ti) / 2;
                re[a] = (re[a] + tr) / 2; im[a] = (im[a] + ti) / 2;
            }
    }
    for (b = 0; b < LJ_AUDIO_BANDS; b++) {
        uint32_t sum = 0;
        for (i = lj_audio_band_edge[b]; i < lj_audio_band_edge[b + 1]; i++) {
            uint32_t x = (uint32_t)(re[i] < 0 ? -re[i] : re[i]), y = (uint32_t)(im[i] < 0 ? -im[i] : im[i]);
            sum += x > y ? x + y / 2 : y + x / 2;
        }
        band[b] = (uint16_t)(sum > 32767 ? 32767 : sum);
    }
}

static int lj_au_block(lj_audio_state *s, const lj_audio_cfg *c, const int16_t *b, uint64_t t)
{
    uint32_t sum = 0, peak = 0, level;
    int i;
    for (i = 0; i < LJ_AUDIO_BLOCK; i++) {
        uint32_t a = (uint32_t)(b[i] < 0 ? -(int32_t)b[i] : b[i]);
        int32_t g = (int32_t)((int64_t)b[i] * c->gain_pct / 100);
        sum += a; if (a > peak) peak = a;
        s->wave[i] = (int16_t)(g > 32767 ? 32767 : g < -32768 ? -32768 : g);
    }
    s->peak = (uint16_t)(peak > 32767 ? 32767 : peak);
    s->clip = (uint8_t)(peak >= 32767 ? 2 : peak >= 29491 ? 1 : 0);   /* red clipped, yellow within 10 % (05 s9.1), PROPOSAL rig W5-AUD-2 */
    level = sum / LJ_AUDIO_BLOCK;
    if (c->use_eq) {
        uint32_t w = 0;
        lj_audio_fft_bands(b, s->band);
        for (i = 0; i < LJ_AUDIO_BANDS; i++) w += (uint32_t)s->band[i] * c->eq[i];
        level = w / 128u;
    } else memset(s->band, 0, sizeof s->band);   /* the spectrum runs only with Use Equalizer ("Enables equalizer and spectrum analyzer") */
    return lj_audio_detect(s, c, (uint16_t)(level > 32767 ? 32767 : level), t);
}

int lj_audio_analyze(lj_audio_state *s, const lj_audio_cfg *c, const int16_t *pcm, size_t n, uint64_t now_us)
{
    int trigs = 0;
    if (!s || !c || !pcm) return 0;
    if (s->nrem >= LJ_AUDIO_BLOCK) s->nrem = 0;
    while (n) {
        size_t take = LJ_AUDIO_BLOCK - s->nrem;
        if (take > n) take = n;
        memcpy(s->rem + s->nrem, pcm, take * sizeof *pcm);
        s->nrem = (uint16_t)(s->nrem + take); pcm += take; n -= take;
        if (s->nrem == LJ_AUDIO_BLOCK) {               /* the block ended n samples before now_us */
            uint64_t back = (uint64_t)n * 1000000u / LJ_AUDIO_RATE;
            trigs += lj_au_block(s, c, s->rem, now_us > back ? now_us - back : 0);
            s->nrem = 0;
        }
    }
    return trigs;
}

/* ---- the capture ring: SDL's audio thread posts, the UI thread polls (both under lj_lock, bounded copies) ---- */
static struct { int16_t buf[LJ_AUDIO_RING]; size_t head, count; uint32_t dropped; int ok; char status[96]; } lj_au_ring;

void lj_audio_post(const int16_t *pcm, size_t n)
{
    size_t i;
    if (!pcm) return;
    lj_lock();
    for (i = 0; i < n; i++) {
        if (lj_au_ring.count == LJ_AUDIO_RING) { lj_au_ring.dropped += (uint32_t)(n - i); break; }   /* full: the rest is dropped and counted */
        lj_au_ring.buf[(lj_au_ring.head + lj_au_ring.count++) % LJ_AUDIO_RING] = pcm[i];
    }
    lj_unlock();
}
size_t lj_audio_poll(int16_t *pcm, size_t cap)
{
    size_t i, n;
    if (!pcm) return 0;
    lj_lock();
    n = lj_au_ring.count < cap ? lj_au_ring.count : cap;
    for (i = 0; i < n; i++) pcm[i] = lj_au_ring.buf[(lj_au_ring.head + i) % LJ_AUDIO_RING];
    lj_au_ring.head = (lj_au_ring.head + n) % LJ_AUDIO_RING; lj_au_ring.count -= n;
    lj_unlock();
    return n;
}
uint32_t lj_audio_dropped(void) { uint32_t d; lj_lock(); d = lj_au_ring.dropped; lj_unlock(); return d; }

void lj_audio_set_device_status(int ok, const char *text) { lj_lock(); lj_au_ring.ok = ok; lj_str_copy(lj_au_ring.status, sizeof lj_au_ring.status, text ? text : ""); lj_unlock(); }
const char *lj_audio_device_status(int *ok) { if (ok) *ok = lj_au_ring.ok; return lj_au_ring.status; }   /* UI thread; written only around open / close */
