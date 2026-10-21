/* test_perf.c - the design 3.7 performance guard: 100 fixtures (20 channels
 * each, 2000 patched channels), 12 cue slots + 5 background slots each running
 * a two-scene fading sequence over every channel, 20 active statics, a generic
 * macro on all 2048 channels; 100 ticks; the longest lj_engine_tick is logged
 * and gated at 10x the 2 ms budget in Release only (LORD_C-12: a logged
 * measurement everywhere, an assertion only without sanitizers / NDEBUG). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_model.h"
#include "core/lj_clock.h"
#include "core/lj_log.h"

enum { LJ_PERF_FOOTPRINT = 20, LJ_PERF_FIXTURES = 100, LJ_PERF_SEQS = LJ_CUE_SLOTS + LJ_BG_SLOTS, LJ_PERF_STATICS = 20, LJ_PERF_TICKS = 100, LJ_PERF_BUDGET_US = 2000 };

static lj_profile s_prof;

static lj_sequence *lj_seq_make(uint32_t number, int phase)
{
    lj_sequence *s = (lj_sequence *)calloc(1, sizeof *s);
    int i, ch;
    s->number = number; s->n = 2; s->scenes = (lj_scene *)calloc(2, sizeof(lj_scene));
    for (i = 0; i < 2; i++) {
        s->scenes[i].scene_ms = 1000 + 100 * phase; s->scenes[i].fade_ms = 900;
        for (ch = 0; ch < LJ_CHANS; ch++) { s->scenes[i].val[ch] = (uint8_t)(i ? 255 - ch - phase : ch + phase); s->scenes[i].state[ch] = LJ_FADE; }
    }
    return s;
}

int lj_test_engine_perf(int argc, char **argv);
int lj_test_engine_perf(int argc, char **argv)
{
    lj_show *show = (lj_show *)calloc(1, sizeof *show);   /* test code: no model bodies needed */
    lj_engine *e = lj_engine_create();
    lj_sequence *seqs[LJ_PERF_SEQS];
    lj_cue *cue = (lj_cue *)calloc(1, sizeof *cue);
    lj_bgcue *bg = (lj_bgcue *)calloc(1, sizeof *bg);
    lj_genmacro *gm = (lj_genmacro *)calloc(1, sizeof *gm);
    lj_static_slot *stat = (lj_static_slot *)calloc(LJ_PERF_STATICS, sizeof *stat);
    lj_engine_status st;
    lj_cmd cmd;
    uint32_t max_us = 0;
    int i, ch, k, fail = 0;
    (void)argc; (void)argv;
    memset(&s_prof, 0, sizeof s_prof);
    s_prof.footprint = LJ_PERF_FOOTPRINT; s_prof.use_gm = 1; s_prof.intensity.enabled = 1; s_prof.intensity.max = 255;
    for (ch = 0; ch < LJ_PERF_FOOTPRINT; ch++) s_prof.channel_class[ch] = (ch % 4 == 0) ? LJ_CH_INTENSITY : LJ_CH_COLOR;
    for (i = 0; i < LJ_PERF_FIXTURES; i++) {                   /* 100 x 20 = 2000 channels over 4 links */
        show->fixture[i].kind = LJ_FX_BUILTIN; show->fixture[i].link = (uint8_t)(1 + (i * LJ_PERF_FOOTPRINT) / LJ_LINK_CH);
        show->fixture[i].address = (uint16_t)(1 + (i * LJ_PERF_FOOTPRINT) % LJ_LINK_CH);
        show->fixture[i].intensity_group = (uint8_t)(1 + i % LJ_SUBMASTERS);
    }
    for (ch = 0; ch < LJ_CHANS; ch++) show->patch[ch].htp_mask = 1u << (ch % LJ_HTP_GROUPS);
    for (i = 0; i < LJ_PERF_SEQS; i++) seqs[i] = lj_seq_make((uint32_t)i + 1, i);
    cue->number = 1; cue->cue_time_pct = 100; cue->gen_macro = 1; cue->bg_cue = 1;
    for (k = 0; k < LJ_CUE_SLOTS; k++) { cue->slot[k].seq = (uint32_t)k + 1; cue->slot[k].enabled = 1; }
    bg->number = 1;
    for (k = 0; k < LJ_BG_SLOTS; k++) { bg->slot[k].seq = (uint32_t)LJ_CUE_SLOTS + k + 1; bg->slot[k].enabled = 1; }
    gm->number = 1;
    for (ch = 0; ch < LJ_CHANS; ch++) { gm->ch[ch].shape = 7; gm->ch[ch].amp_pos = 40; gm->ch[ch].amp_neg = 40; gm->ch[ch].cycle_ds = (uint16_t)(10 + ch % 50); gm->ch[ch].delay = (uint16_t)(ch * 3); }
    for (i = 0; i < LJ_PERF_STATICS; i++) {
        stat[i].kind = LJ_STATIC_RECORDED; stat[i].level_pct = 100; stat[i].scene = (lj_scene *)calloc(1, sizeof(lj_scene));
        for (ch = i * 100; ch < i * 100 + 100 && ch < LJ_CHANS; ch++) { stat[i].scene->val[ch] = 128; stat[i].scene->state[ch] = LJ_SNAP; }
    }
    lj_engine_bind(e, show);
    for (i = 0; i < LJ_PERF_FIXTURES; i++) lj_engine_bind_profile(e, i, &s_prof);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 1; cmd.cue = cue; cmd.bg = bg; cmd.gm = gm;
    for (k = 0; k < LJ_CUE_SLOTS; k++) cmd.seq[k] = seqs[k];
    for (k = 0; k < LJ_BG_SLOTS; k++) cmd.bgseq[k] = seqs[LJ_CUE_SLOTS + k];
    lj_engine_cmd(e, &cmd);
    for (i = 0; i < LJ_PERF_STATICS; i++) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_STATIC; cmd.a = (uint32_t)i; cmd.b = 1; cmd.st = &stat[i]; lj_engine_cmd(e, &cmd); }
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_SET_MASTER; cmd.a = 0; cmd.b = 200; lj_engine_cmd(e, &cmd);
    for (i = 0; i < LJ_PERF_TICKS; i++) {
        uint64_t t0 = lj_clock_us(), t1;
        lj_engine_tick(e, (uint64_t)i * LJ_TICK_US_DEFAULT);
        t1 = lj_clock_us();
        if (i > 0 && t1 - t0 > max_us) max_us = (uint32_t)(t1 - t0);   /* the load tick (first) is not the steady state */
    }
    lj_engine_view(e, &st);
    if (st.slot_status[0] != LJ_SLOT_RUNNING || !st.bg_any || !st.statics_any) { printf("FAIL load: slot %u bg %u statics %u\n", st.slot_status[0], st.bg_any, st.statics_any); fail = 1; }
    lj_log("perf: 100 fixtures, 12+5 slots, 20 statics, genmacro 2048 ch: max tick %u us (budget %u us, engine view max %u us)", (unsigned)max_us, (unsigned)LJ_PERF_BUDGET_US, (unsigned)st.tick_us_max);
    printf("test_engine_perf: max tick %u us over %d ticks (budget %d us)\n", (unsigned)max_us, LJ_PERF_TICKS, LJ_PERF_BUDGET_US);
#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(LJ_SANITIZE)
    if (max_us > 10u * LJ_PERF_BUDGET_US) { printf("FAIL perf: %u us > 10 x %d us (LORD_C-12)\n", (unsigned)max_us, LJ_PERF_BUDGET_US); fail = 1; }
#endif
    lj_engine_bind(e, NULL);
    lj_engine_destroy(e);
    for (i = 0; i < LJ_PERF_SEQS; i++) { free(seqs[i]->scenes); free(seqs[i]); }
    for (i = 0; i < LJ_PERF_STATICS; i++) free(stat[i].scene);
    free(stat); free(gm); free(bg); free(cue); free(show);
    return fail;
}
