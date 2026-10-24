/* test_audio.c - `lj4evr_tests test_audio`: the Audio Analyzer headless (io/lj_audio.h, W5-AUD-2, W5-AUD-6) on the
 * synthetic PCM of tests/io/audio_vectors.h (tools/gen_vectors.py --audio).
 *  - a 120 BPM click train gives 2 trigs/s; silence 0; noise under the threshold 0;
 *  - decay 24 ms vs 600 ms and sensitivity 2.0 vs 10.0 change the count as stated; gain moves only the threshold side;
 *  - the FFT bands put each bin-exact tone in its band and the equalizer removes a cut band; clip 0 / 1 / 2;
 *  - the capture ring drops and counts past LJ_AUDIO_RING; Hardware.Ini [AUDIO] round trip and hostile values clamped;
 *  - W5-AUD-6: the DMX-in gain / noise levels and the Fingers jogs reach the same two settings;
 *  - W6 LORD_ENGINE-7: trig times follow the sample count, not a jittered wall clock. */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "io/lj_audio.h"
#include "io/audio_vectors.h"
#include "windows/lj_fingers.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { SEC = LJ_AUDIO_RATE, MAXS = 4 * LJ_AUDIO_RATE };
static int16_t pcm[MAXS];

/* clicks every period samples (scale 1/div), over a noise bed when noise != 0; secs seconds */
static size_t make(int secs, int period, int div, int noise)
{
    size_t n = (size_t)secs * SEC, i;
    memset(pcm, 0, sizeof pcm);
    for (i = 0; i < n; i++) {
        int v = noise ? lj_av_noise[i % 512] : 0;
        if (period && (int)(i % (size_t)period) < 256) v += lj_av_click[i % (size_t)period] / div;
        pcm[i] = (int16_t)v;
    }
    return n;
}
/* feeds n samples in uneven chunks, the clock following the samples; returns the trigs */
static int run(lj_audio_state *s, const lj_audio_cfg *c, size_t from, size_t n, uint64_t *t)
{
    size_t i = from, k = 0;
    int trigs = 0;
    while (i < from + n) {
        size_t take = 700 + (k++ % 5) * 300;
        if (take > from + n - i) take = from + n - i;
        *t += (uint64_t)take * 1000000u / LJ_AUDIO_RATE;
        trigs += lj_audio_analyze(s, c, pcm + i, take, *t);
        i += take;
    }
    return trigs;
}
static int count(lj_audio_cfg *c, size_t n)
{
    static lj_audio_state s;
    uint64_t t = 1000000;
    lj_audio_reset(&s);
    return run(&s, c, 0, n, &t);
}

static void test_detector(void)
{
    lj_audio_cfg c;
    size_t n;
    int k;
    lj_audio_cfg_default(&c);
    CHECK(!c.enabled && c.noise_range == LJ_AUDIO_NOISE_MEDIUM && c.gain_pct == 100);

    n = make(4, SEC / 2, 1, 0);                       /* 120 BPM = 2 clicks/s, 4 s */
    k = count(&c, n); CHECK(k == 8); printf("  120 BPM, 4 s: %d trigs\n", k);
    n = make(4, 0, 1, 0);                             /* silence */
    CHECK(count(&c, n) == 0);
    c.noise = 64;                                     /* Medium: threshold 512 > the bed's mean |x| ~300 */
    n = make(4, 0, 1, 1);
    CHECK(count(&c, n) == 0);
    c.noise = 4;                                      /* threshold 32: the bed's onset is one trig, then a steady level never trigs */
    CHECK(count(&c, n) == 1);

    lj_audio_cfg_default(&c);                         /* decay extremes: 10 clicks/s for 2 s */
    n = make(2, SEC / 10, 1, 0);
    c.decay_ms = LJ_AUDIO_DECAY_MIN; k = count(&c, n); CHECK(k == 20); printf("  10 clicks/s, decay 24 ms: %d\n", k);
    c.decay_ms = LJ_AUDIO_DECAY_MAX; k = count(&c, n); CHECK(k == 3);  printf("  10 clicks/s, decay 600 ms: %d (0, 0.7, 1.4 s: the click at 0.6 s ends its block a hair early)\n", k);

    lj_audio_cfg_default(&c);                         /* sensitivity extremes: clicks / 3 over the bed, ratio ~5 */
    c.noise = 0; c.decay_ms = 100;
    n = make(4, SEC / 2, 3, 1);
    c.sensitivity_x10 = LJ_AUDIO_SENS_MIN; k = count(&c, n); CHECK(k == 8); printf("  factor 2.0: %d\n", k);
    c.sensitivity_x10 = LJ_AUDIO_SENS_MAX; k = count(&c, n); CHECK(k <= 1); printf("  factor 10.0: %d (the bed's onset only)\n", k);

    lj_audio_cfg_default(&c);                         /* gain: a level under the threshold at 100 % trigs at 1000 %, ratio unchanged */
    {
        lj_audio_state s;
        lj_audio_reset(&s);
        c.noise = 100;                                /* threshold 800 */
        CHECK(lj_audio_detect(&s, &c, 500, 1000000) == 0 && s.level == 500);
        lj_audio_reset(&s);
        c.gain_pct = 1000;
        CHECK(lj_audio_detect(&s, &c, 500, 1000000) == 1 && s.level == 5000 && (s.timeline[0] & 0x8000u));
    }
}

static void test_spectrum(void)
{
    static lj_audio_state s;
    lj_audio_cfg c;
    int b, best;
    uint16_t lvl_flat;
    lj_audio_cfg_default(&c);
    c.use_eq = 1;
    lj_audio_reset(&s);
    lj_audio_analyze(&s, &c, lj_av_low, 512, 1000000);
    for (best = 0, b = 1; b < LJ_AUDIO_BANDS; b++) if (s.band[b] > s.band[best]) best = b;
    CHECK(best == 1); printf("  172 Hz tone: band %d = %u\n", best, s.band[best]);
    lvl_flat = s.level;
    lj_audio_reset(&s);
    lj_audio_analyze(&s, &c, lj_av_high, 512, 1000000);
    for (best = 0, b = 1; b < LJ_AUDIO_BANDS; b++) if (s.band[b] > s.band[best]) best = b;
    CHECK(best == 12); printf("  5.5 kHz tone: band %d = %u\n", best, s.band[best]);
    c.eq[1] = 0; c.eq[0] = 0; c.eq[2] = 0;            /* cut the low bands: the low tone all but vanishes */
    lj_audio_reset(&s);
    lj_audio_analyze(&s, &c, lj_av_low, 512, 1000000);
    CHECK(s.level < lvl_flat / 8); printf("  low tone level flat %u, cut %u\n", lvl_flat, s.level);
    c.use_eq = 0;
    lj_audio_reset(&s);
    lj_audio_analyze(&s, &c, lj_av_low, 512, 1000000);
    CHECK(s.band[1] == 0 && s.level > 9000 && s.level < 11000 && s.clip == 0);   /* mean |16000 sin| = 10186 */
    {
        static int16_t sq[512];
        int i;
        for (i = 0; i < 512; i++) sq[i] = (int16_t)(i & 32 ? 30000 : -30000);
        lj_audio_reset(&s); lj_audio_analyze(&s, &c, sq, 512, 1000000); CHECK(s.clip == 1);
        for (i = 0; i < 512; i++) sq[i] = (int16_t)(i & 32 ? 32767 : -32768);
        lj_audio_reset(&s); lj_audio_analyze(&s, &c, sq, 512, 1000000); CHECK(s.clip == 2);
        lj_audio_reset(&s); lj_audio_analyze(&s, &c, sq, 300, 1000000); CHECK(s.nrem == 300 && s.clip == 0);   /* a part block waits */
        lj_audio_analyze(&s, &c, sq, 212, 1000000); CHECK(s.nrem == 0 && s.clip == 2);
    }
}

static void test_ring(void)
{
    int16_t out[2048];
    size_t got = 0, n;
    uint32_t d0 = lj_audio_dropped();
    memset(pcm, 0, sizeof pcm);
    pcm[0] = 7; pcm[LJ_AUDIO_RING - 1] = 9;
    lj_audio_post(pcm, 20000);
    CHECK(lj_audio_dropped() - d0 == 20000 - LJ_AUDIO_RING);
    while ((n = lj_audio_poll(out, 2048)) > 0) { if (!got) CHECK(out[0] == 7); got += n; if (got == LJ_AUDIO_RING) CHECK(out[n - 1] == 9); }
    CHECK(got == LJ_AUDIO_RING);
    lj_audio_post(NULL, 5); CHECK(lj_audio_poll(NULL, 5) == 0 && lj_audio_poll(out, 5) == 0);
}

static void test_cfg(void)
{
    lj_ini *ini = lj_ini_new();
    lj_audio_cfg a, b;
    lj_audio_cfg_default(&a);
    a.enabled = 1; lj_str_copy(a.device, sizeof a.device, "Line In (Synthetic)"); a.use_eq = 1; a.noise_range = 2; a.noise = 77;
    a.decay_ms = 120; a.sensitivity_x10 = 45; a.gain_pct = 251; a.eq[3] = 9; a.view = LJ_AUDIO_VIEW_TIMELINE; a.auto_save = 1; a.pass_keys = 1;
    a.sound_card = 0; a.on_top = 0; a.run_minimized = 1; a.trigger_faders = 0;
    lj_audio_cfg_save(ini, &a);
    lj_audio_cfg_load(ini, &b);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    CHECK(lj_ini_geti(ini, "AUDIO", "TrigAggr", 0) == 45 && lj_ini_geti(ini, "AUDIO", "EqualizerBand4", 0) == 9);
    lj_ini_seti(ini, "AUDIO", "NoiseLevelSwitch", 9); lj_ini_seti(ini, "AUDIO", "TrigDecay", -5); lj_ini_seti(ini, "AUDIO", "TrigAggr", 100000);
    lj_ini_seti(ini, "AUDIO", "Gain", 0); lj_ini_seti(ini, "AUDIO", "EqualizerBand16", 999); lj_ini_seti(ini, "AUDIO", "ShowInput", -1);
    lj_audio_cfg_load(ini, &b);                       /* a hand-edited Hardware.Ini: every value clamped */
    CHECK(b.noise_range == 2 && b.decay_ms == LJ_AUDIO_DECAY_MIN && b.sensitivity_x10 == LJ_AUDIO_SENS_MAX && b.gain_pct == LJ_AUDIO_GAIN_MIN && b.eq[15] == 255 && b.view == 0);
    lj_audio_cfg_load(NULL, &b);
    CHECK(!b.enabled);                                /* W5-X-OFF */
    lj_ini_free(ini);
}

static void test_remote_inputs(void)   /* W5-AUD-6: one row per input */
{
    static lj_audio_cfg a;
    lj_ctx *c = lj_ctx_get();
    lj_audio_cfg_default(&a);
    lj_audio_dmx_gain(&a, 255); CHECK(a.gain_pct == 1000);   /* DMX-in "Audio Analyzer Gain" */
    lj_audio_dmx_gain(&a, 0);   CHECK(a.gain_pct == 10);
    lj_audio_dmx_gain(&a, 128); CHECK(a.gain_pct == 100 && lj_audio_gain_pos(&a) == 0);
    lj_audio_dmx_noise(&a, 200); CHECK(a.noise == 200);      /* DMX-in "Audio Analyzer Noise Threshold" */
    c->audio = &a;
    CHECK(lj_fingers_set_jog_function(1, 5) == 0);           /* "Audio Gain" */
    CHECK(lj_fingers_jog(1, 127) == 0 && a.gain_pct == 1000);
    CHECK(lj_fingers_jog(1, -127) == 0 && a.gain_pct == 100);
    CHECK(lj_fingers_set_jog_function(1, 6) == 0);           /* "Min Trglvl" */
    CHECK(lj_fingers_jog(1, -50) == 0 && a.noise == 150);
    CHECK(lj_fingers_jog(1, 500) == 0 && a.noise == 255);
    c->audio = NULL;
}

/* W6 LORD_ENGINE-7: the trig times come from the sample count. The same clicks fed with a wall clock that jitters by up to
 * +-20 ms per poll give the same trig times as an exact clock, each one on a 512-sample block boundary after t0. */
static int clocked(lj_audio_state *s, const lj_audio_cfg *c, size_t n, int jitter, uint64_t *times, int cap)
{
    size_t i = 0, k = 0;
    uint64_t t = 1000000;
    int m = 0;
    lj_audio_reset(s);
    while (i < n) {
        size_t take = 700 + (k % 5) * 300;
        int64_t j = jitter && k ? (int64_t)((k * 7919u) % 41u) * 1000 - 20000 : 0;   /* the first poll is on time: it anchors t0 */
        if (take > n - i) take = n - i;
        t += (uint64_t)take * 1000000u / LJ_AUDIO_RATE;
        if (lj_audio_analyze(s, c, pcm + i, take, (uint64_t)((int64_t)t + j)) > 0 && m < cap) times[m++] = s->last_trig_us;
        i += take; k++;
    }
    return m;
}
static void test_sample_clock(void)
{
    static lj_audio_state s;
    lj_audio_cfg c;
    uint64_t a[16], b[16], t0;
    int na, nb, i, ok = 1;
    lj_audio_cfg_default(&c);
    make(4, SEC / 2, 1, 0);
    na = clocked(&s, &c, 4u * SEC, 0, a, 16);
    nb = clocked(&s, &c, 4u * SEC, 1, b, 16);
    t0 = s.t0_us;
    CHECK(na == 8 && nb == 8 && t0 == 1000000u && s.samples == 4u * SEC);
    for (i = 0; i < na && i < nb; i++) {
        uint64_t blk = ((b[i] - t0) * LJ_AUDIO_RATE + 256000000u) / 512000000u;   /* the nearest block count */
        if (a[i] != b[i] || b[i] != t0 + blk * 512u * 1000000u / LJ_AUDIO_RATE) ok = 0;
    }
    CHECK(ok);
    printf("  sample clock: %d trigs under a +-20 ms jittered wall clock, times equal to the exact clock's (first %llu us, last %llu us)\n",
           nb, (unsigned long long)(nb ? b[0] : 0), (unsigned long long)(nb ? b[nb - 1] : 0));
}

int lj_test_audio(int argc, char **argv);
int lj_test_audio(int argc, char **argv)
{
    (void)argc; (void)argv;
    test_detector();
    test_spectrum();
    test_ring();
    test_cfg();
    test_remote_inputs();
    test_sample_clock();
    printf("test_audio: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}

#ifdef LJ_AUDIO_SDL_TEST
/* lj4evr_audio_sdl_test (CMake, LJ_SDL; CTest runs it with SDL_AUDIO_DRIVER=dummy): the capture glue end to end. The device
 * list has the default recording device, the stream opens at LJ_AUDIO_RATE mono S16, the callback fills the ring (dummy =
 * silence) which the analyzer drains with 0 trigs, and a close stops the posts. */
#include "core/lj_clock.h"
#include "platform/sdl/lj_sdl_audio.h"
int main(void)
{
    static int16_t buf[4096];
    lj_audio_dev devs[8];
    static lj_audio_state s;
    lj_audio_cfg c;
    int n = lj_sdl_audio_list(devs, 8), ok = 0, trigs = 0;
    size_t got = 0, k;
    uint64_t t0;
    printf("devices: %d%s%s\n", n, n > 0 ? ", first " : "", n > 0 ? devs[0].name : "");
    CHECK(n >= 0);
    CHECK(lj_sdl_audio_open("") == 0);
    lj_audio_device_status(&ok); CHECK(ok == 1);
    lj_audio_cfg_default(&c); lj_audio_reset(&s);
    for (t0 = lj_clock_us(); lj_clock_us() - t0 < 2000000u && got < LJ_AUDIO_RATE / 4;)
        if ((k = lj_audio_poll(buf, 4096)) > 0) { got += k; trigs += lj_audio_analyze(&s, &c, buf, k, lj_clock_us()); }
    printf("samples in 2 s window: %u, trigs %d\n", (unsigned)got, trigs);
    CHECK(got >= LJ_AUDIO_RATE / 4 && trigs == 0);
    CHECK(lj_sdl_audio_open("No Such Device (Synthetic)") == 0);   /* an absent name falls back to the default device, logged */
    lj_sdl_audio_close();
    while (lj_audio_poll(buf, 4096) > 0) {}
    lj_sdl_audio_close();                                          /* closed: a no-op */
    printf("audio_sdl: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
#endif
