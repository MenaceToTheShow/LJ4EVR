/* lj_audio.c - the Audio Analyzer (io/lj_audio.h). W5 CONTRACT STUB: the audio track replaces every body
 * (docs/w5-contracts.md seam 4). Until then nothing is analysed and no trig is produced. */
#include <string.h>
#include "io/lj_audio.h"


void lj_audio_cfg_default(lj_audio_cfg *c)
{
    int i;
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->noise_range = LJ_AUDIO_NOISE_MEDIUM; c->decay_ms = 200; c->sensitivity_x10 = 30; c->gain_pct = 100;   /* PROPOSAL rig W5-AUD-2 */
    for (i = 0; i < LJ_AUDIO_BANDS; i++) c->eq[i] = 128;
}
void lj_audio_cfg_load(lj_ini *hardware_ini, lj_audio_cfg *c) { (void)hardware_ini; lj_audio_cfg_default(c); }
void lj_audio_cfg_save(lj_ini *hardware_ini, const lj_audio_cfg *c) { (void)hardware_ini; (void)c; }

void lj_audio_reset(lj_audio_state *s) { if (s) memset(s, 0, sizeof *s); }
int  lj_audio_detect(lj_audio_state *s, const lj_audio_cfg *c, uint16_t level, uint64_t now_us) { (void)s; (void)c; (void)level; (void)now_us; return 0; }
int  lj_audio_analyze(lj_audio_state *s, const lj_audio_cfg *c, const int16_t *pcm, size_t n, uint64_t now_us) { (void)s; (void)c; (void)pcm; (void)n; (void)now_us; return 0; }

void     lj_audio_post(const int16_t *pcm, size_t n) { (void)pcm; (void)n; }
size_t   lj_audio_poll(int16_t *pcm, size_t cap) { (void)pcm; (void)cap; return 0; }
uint32_t lj_audio_dropped(void) { return 0; }
