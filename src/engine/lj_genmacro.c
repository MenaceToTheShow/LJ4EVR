/* lj_genmacro.c - rank 6, the 14 sA.3 arithmetic. The per-channel formula (14 sA.3 "derived"):
 * s = shape[(index - delay) mod 8192]; delta = s * Amp+ / 32640 for s >= 0, s * Amp- / 32640
 * for s < 0 (the note writes "-s * Amp-", which would make both halves positive; the
 * -186/186 readout of the editor needs the signed form, LJ_GENMACRO_AMP_SPLIT); ref = base if
 * enabled else the merged value; out = clamp(ref + delta, 0, 255). Shapes: engine/lj_shapes_gen.c
 * (the 66 regenerated tables) or the user's override pinned by LJ_CMD_GM_SHAPES (ME-34). */
#include <string.h>
#include "engine/lj_genmacro.h"
#include "engine/lj_post.h"
#include "engine/lj_shapes_gen.h"

enum { LJ_GM_N = LJ_GENMACRO_SHAPE_N, LJ_GM_MASK = LJ_GM_N - 1 };
#define LJ_GM_IDX_END ((uint32_t)LJ_GM_N << 16)

int16_t lj_genmacro_shape_in(const lj_gm_shapes *sh, int shape, int k)
{
    if (!sh || !sh->n || !sh->s) sh = &lj_gm_shapes_builtin;
    if (shape < 0 || shape >= sh->n) return 0;
    return sh->s[(size_t)shape * LJ_GM_N + (size_t)(k & LJ_GM_MASK)];
}

static int32_t lj_gm_delta_x(const lj_gm_shapes *sh, const lj_gm_param *p, uint32_t idx, int mul)
{
    int delay = p->dir == LJ_DIR_BOUNCE ? 0 : p->delay;        /* CHM 5835 (LJ_GENMACRO_BOUNCE_NO_DELAY) */
    int32_t s = lj_genmacro_shape_in(sh, p->shape, (int)(idx >> 16) - delay);
    int64_t q = (int64_t)s * (s >= 0 ? p->amp_pos : p->amp_neg) * mul;   /* Amp+ on the positive half, Amp- on the negative one; a negative amplitude mirrors its half */
    return (int32_t)((q >= 0 ? q + LJ_GENMACRO_FULL / 2 : q - LJ_GENMACRO_FULL / 2) / LJ_GENMACRO_FULL);   /* nearest, half away from 0 (C division truncates) */
}

int32_t lj_genmacro_delta(const lj_gm_shapes *sh, const lj_gm_param *p, uint32_t idx) { return lj_gm_delta_x(sh, p, idx, 1); }

int32_t lj_genmacro_step(const lj_gm_shapes *sh, const lj_gm_param *p, uint32_t idx, int32_t ref, int wide)
{
    int32_t out;
    if (p->base_on) ref = wide ? p->base * 256 : p->base;
    out = ref + lj_gm_delta_x(sh, p, idx, wide ? LJ_GENMACRO_16BIT_SCALE : 1);   /* ME-128: a declared pair takes round(s * amp * 256 / 32640) from the sample, so the LSB moves (PROPOSAL rig 14 sA.6) */
    if (out < 0) out = 0;
    if (out > (wide ? 65535 : 255)) out = wide ? 65535 : 255;   /* "limited to 0 to 255" (CHM 5493) */
    return out;
}

void lj_genmacro_advance(const lj_genmacro *m, lj_gm_ch st[LJ_CHANS], uint32_t tick_us)
{
    int ch;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        uint64_t cycle_us = (uint64_t)m->ch[ch].cycle_ds * 100000u;
        uint32_t incr, idx;
        if (cycle_us == 0 || !lj_genmacro_ch_used(m, ch)) continue;   /* cycle 0 = static (CHM 5831) */
        incr = (uint32_t)(((uint64_t)LJ_GM_IDX_END * tick_us) / cycle_us);   /* 8192 * tick / cycle in 16.16 */
        if (incr >= LJ_GM_IDX_END) incr = LJ_GM_IDX_END - 1;   /* a cycle shorter than a tick (never with cycle >= 0.4 s) */
        idx = st[ch].idx;
        if (m->ch[ch].dir == LJ_DIR_BOUNCE) {                  /* bounce off either end, delay not considered (CHM 5835) */
            if (st[ch].back) { if (idx < incr) { st[ch].back = 0; idx = incr - idx; } else idx -= incr; }
            else { idx += incr; if (idx >= LJ_GM_IDX_END) { st[ch].back = 1; idx = 2u * LJ_GM_IDX_END - idx - 1; } }
        } else if (m->ch[ch].dir == LJ_DIR_REV) idx -= incr;
        else idx += incr;
        st[ch].idx = idx & (LJ_GM_IDX_END - 1);                /* "looped back into the 0-8191 interval" (CHM 5565) */
    }
}

/* One source over merged[]: every programmed channel of a patched fixture except LSB members (CHM 5583: N/A);
 * a declared 16-bit pair is one value with one state (ME-22): ref = MSB * 256 + LSB, split back bytewise. */
static void lj_gm_apply(const lj_genmacro *m, const lj_gm_ch st[LJ_CHANS], const lj_gm_shapes *sh, const lj_post_ctx *c,
                        uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int lsb;
        if (!lj_genmacro_ch_used(m, ch) || c->fixture_of[ch] < 0 || lj_post_is_lsb(c, ch)) continue;
        lsb = lj_post_pair_lsb(c, ch);
        if (lsb >= 0) {
            int32_t v = lj_genmacro_step(sh, &m->ch[ch], st[ch].idx, merged[ch] * 256 + merged[lsb], 1);
            merged[ch] = (uint8_t)(v >> 8); merged[lsb] = (uint8_t)(v & 0xFF);
            layer_of[ch] = layer_of[lsb] = LJ_L_GENMACRO;
        } else {
            merged[ch] = (uint8_t)lj_genmacro_step(sh, &m->ch[ch], st[ch].idx, merged[ch], 0);
            layer_of[ch] = LJ_L_GENMACRO;
        }
    }
}

static void lj_gm_load(lj_gm_rank6 *g, int src, const lj_genmacro *m, int restart)
{
    if (restart) memset(g->st[src], 0, sizeof g->st[src]);    /* indexes reset only on restart: channels "may ... become unsynchronized when selecting new macros" [CHM 5567] */
    g->m[src] = m;
}

void lj_gm_rank6_load_cue(lj_gm_rank6 *g, const lj_genmacro *m)
{
    lj_gm_load(g, LJ_GM_SRC_CUE, m, m && m->restart_on_load);   /* 14 sA.3 (7): restart on cue load if flagged */
}

void lj_gm_rank6_cmd(lj_gm_rank6 *g, const lj_cmd *c)
{
    int src = c->c == LJ_GM_SRC_EDITOR ? LJ_GM_SRC_EDITOR : LJ_GM_SRC_CUE;
    switch (c->kind) {
    case LJ_CMD_LOAD_GENMACRO: lj_gm_load(g, src, c->gm, c->b != 0); break;   /* b = restart: the UI passes the record's flag when a cue loaded it (LJ_GENMACRO_RESTART_CUE_ONLY) */
    case LJ_CMD_GENMACRO_ON:
        if (c->a == LJ_GM_SRC_ENGINE) g->engine_off = (uint8_t)(c->b == 0);
        else g->slot_off = (uint8_t)(c->b == 0);              /* Generic Macro Slot On/Off: the cue's macro only */
        break;
    case LJ_CMD_GENMACRO_RESTART: memset(g->st[c->a == LJ_GM_SRC_EDITOR ? LJ_GM_SRC_EDITOR : LJ_GM_SRC_CUE], 0, sizeof g->st[0]); break;
    case LJ_CMD_GM_SHAPES: g->shapes = c->gm_shapes; break;    /* pinned by the UI until the next GM_SHAPES (ME-34) */
    default: break;
    }
}

void lj_gm_rank6_tick(lj_gm_rank6 *g, const lj_post_ctx *c, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS], uint32_t tick_us)
{
    int src;
    if (g->engine_off) return;                                 /* "disable ALL macros running" [14 sA.3 (8)]; indexes hold (PROPOSAL rig 14 sA.7) */
    for (src = LJ_GM_SRC_CUE; src <= LJ_GM_SRC_EDITOR; src++) {   /* the editor's after the cue's (LJ_GENMACRO_EDITOR_AFTER_CUE) */
        if (!g->m[src] || (src == LJ_GM_SRC_CUE && g->slot_off)) continue;
        if (c->show) lj_gm_apply(g->m[src], g->st[src], g->shapes, c, merged, layer_of);
        lj_genmacro_advance(g->m[src], g->st[src], tick_us);   /* index for the next tick: a (re)started macro shows index 0 first [14 sA.3 (7)] */
    }
}

void lj_gm_rank6_view(const lj_gm_rank6 *g, lj_engine_status *s)
{
    s->gm_engine_on = (uint8_t)!g->engine_off; s->gm_slot_on = (uint8_t)!g->slot_off;
    s->gm_cue_number = g->m[LJ_GM_SRC_CUE] ? g->m[LJ_GM_SRC_CUE]->number : 0;
    s->gm_editor_number = g->m[LJ_GM_SRC_EDITOR] ? g->m[LJ_GM_SRC_EDITOR]->number : 0;
}
