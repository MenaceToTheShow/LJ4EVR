/* lj_genmacro.c - 14 sA.3 arithmetic. The per-channel formula (14 sA.3 "derived"):
 * s = shape[(index - delay) mod 8192]; delta = s * Amp+ / 32640 for s >= 0, s * Amp- / 32640
 * for s < 0 (the note writes "-s * Amp-", which would make both halves positive; the
 * -186/186 readout of the editor needs the signed form); ref = base if enabled else the
 * merged value; out = clamp(ref + delta, 0, 255). */
#include <string.h>
#include "engine/lj_genmacro.h"
#include "engine/lj_post.h"
#include "engine/lj_trig_gen.h"

enum { LJ_GM_N = LJ_GENMACRO_SHAPE_N, LJ_GM_MASK = LJ_GM_N - 1 };

int lj_genmacro_programmed(const lj_genmacro *m, int ch)
{
    return m->ch[ch].amp_pos != 0 || m->ch[ch].amp_neg != 0;   /* W2: the model's "no shape" encoding */
}

static int16_t lj_gm_sin(uint32_t q /* 4096-table index in 16.16 */)
{
    int32_t a = lj_sin4096[(q >> 16) & 4095], b = lj_sin4096[((q >> 16) + 1) & 4095];
    int32_t v = a + (int32_t)(((int64_t)(b - a) * (int64_t)(q & 0xFFFF)) / 65536);   /* linear between table points (division, not a signed shift: ME-60, C11 6.5.7p5) */
    v = (int32_t)(((int64_t)v * 32640 + (v >= 0 ? 16383 : -16383)) / 32767);         /* 32767 -> 32640 scale [14 sA.6] */
    return (int16_t)v;
}

int16_t lj_genmacro_shape(int shape, int k)
{
    k &= LJ_GM_MASK;
    if (shape == 0) return lj_gm_sin((uint32_t)k << 14);       /* '2*Sine' = sin(pi*k/8192): half a period over the shape, table step k/4 */
    return lj_gm_sin((uint32_t)k << 15);                       /* 'Sine' = sin(2*pi*k/8192), table step k/2; W2: the other 64 shapes */
}

void lj_genmacro_restart(lj_gm_ch st[LJ_CHANS]) { memset(st, 0, sizeof(lj_gm_ch) * LJ_CHANS); }

void lj_genmacro_advance(const lj_genmacro *m, lj_gm_ch st[LJ_CHANS], uint32_t tick_us)
{
    int ch;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        uint64_t cycle_us = (uint64_t)m->ch[ch].cycle_ds * 100000u;
        uint32_t incr, idx;
        if (!lj_genmacro_programmed(m, ch) || cycle_us == 0) continue;   /* cycle 0 = static (CHM 5831) */
        incr = (uint32_t)(((uint64_t)LJ_GM_N << 16) * tick_us / cycle_us);   /* 8192 * tick / cycle in 16.16 */
        idx = st[ch].idx;
        if (m->ch[ch].dir == LJ_DIR_BOUNCE) {                  /* bounce off either end, delay not considered (CHM 5835) */
            if (st[ch].back) { if (idx < incr) { st[ch].back = 0; idx = incr - idx; } else idx -= incr; }
            else { idx += incr; if (idx >= ((uint32_t)LJ_GM_N << 16)) { st[ch].back = 1; idx = 2u * ((uint32_t)LJ_GM_N << 16) - idx - 1; } }
        } else if (m->ch[ch].dir == LJ_DIR_REV) idx -= incr;
        else idx += incr;
        st[ch].idx = idx & (((uint32_t)LJ_GM_N << 16) - 1);   /* "looped back into the 0-8191 interval" (CHM 5565) */
    }
}

void lj_genmacro_apply(const lj_genmacro *m, const lj_gm_ch st[LJ_CHANS], const lj_post_ctx *c, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int32_t s, delta, ref, out, lsb;
        if (!lj_genmacro_programmed(m, ch) || c->fixture_of[ch] < 0 || lj_post_is_lsb(c, ch)) continue;   /* LSB rows are N/A (CHM 5583) */
        s = lj_genmacro_shape(m->ch[ch].shape, (int)(st[ch].idx >> 16) - m->ch[ch].delay);
        delta = s >= 0 ? (int32_t)(((int64_t)s * m->ch[ch].amp_pos + 16320) / 32640)
                       : (int32_t)(((int64_t)s * m->ch[ch].amp_neg - 16320) / 32640);
        lsb = lj_post_pair_lsb(c, ch);
        if (lsb >= 0) {                                        /* 16-bit on the MSB pair: delta scaled by 256 (UNVERIFIED, 14 sA.3) */
            ref = m->ch[ch].base_on ? m->ch[ch].base * 256 : merged[ch] * 256 + merged[lsb];
            out = ref + delta * 256;
            if (out < 0) out = 0;
            if (out > 65535) out = 65535;
            merged[ch] = (uint8_t)(out >> 8); merged[lsb] = (uint8_t)out;
            layer_of[ch] = layer_of[lsb] = LJ_L_GENMACRO;
        } else {
            ref = m->ch[ch].base_on ? m->ch[ch].base : merged[ch];
            out = ref + delta;
            if (out < 0) out = 0;
            if (out > 255) out = 255;                          /* "limited to 0 to 255" (CHM 5493) */
            merged[ch] = (uint8_t)out;
            layer_of[ch] = LJ_L_GENMACRO;
        }
    }
}
