/* lj_merge.c - the layer stack, ranks 0-5 and 7 (design 3.4, 11 s3.4; ranks 2-4,
 * 6-7 PROPOSAL rig M12-M15). Rank 6 (generic macro) is applied by the engine
 * after this call (lj_genmacro_apply) because it needs the macro record; rank 7
 * (smoke/haze control, 11 C12) by the engine after that (lj_engine.c, it reads the
 * smoke state, not a runner). A layer writes only where its state is Snap or Fade
 * [11 s3.4]. */
#include <string.h>
#include "engine/lj_merge.h"
#include "engine/lj_runner.h"
#include "engine/lj_post.h"

const uint8_t LJ_LAYER_ORDER[LJ_L_COUNT] = { LJ_L_DEFAULT, LJ_L_CUE, LJ_L_FLASH, LJ_L_CURRENT, LJ_L_STATICS, LJ_L_BGCUE, LJ_L_GENMACRO, LJ_L_SMOKE };

static void lj_merge_runner(const lj_runner *r, int rank, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch;
    for (ch = 0; ch < LJ_CHANS; ch++)
        if (r->st[ch] != LJ_OFF) { merged[ch] = r->val[ch]; layer_of[ch] = (uint8_t)rank; }
}

static void lj_merge_cue(const lj_runner *slots, const lj_post_ctx *ctx, int mode, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch, k;
    uint8_t act[LJ_CUE_SLOTS], on[LJ_CUE_SLOTS], prog[LJ_CUE_SLOTS], faded[LJ_CUE_SLOTS], si[LJ_CUE_SLOTS];
    int any = 0;
    for (k = 0; k < LJ_CUE_SLOTS; k++) { act[k] = (uint8_t)lj_runner_active(&slots[k]); any |= act[k]; si[k] = slots[k].seq_int; }
    if (!any) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int win, intensity, n = 0;
        uint8_t v;
        for (k = 0; k < LJ_CUE_SLOTS; k++) {                   /* the slot contributes this channel (Snap/Fade) [03 s5.3, F23] */
            const lj_runner *r = &slots[k];
            on[k] = (uint8_t)(act[k] && r->st[ch] != LJ_OFF);
            if (!on[k]) continue;
            n++; faded[k] = r->val[ch];
            prog[k] = r->seq && r->scene < r->seq->n ? r->seq->scenes[r->scene].val[ch] : r->val[ch];   /* F18: programmed value selects */
        }
        if (!n) continue;
        intensity = lj_post_class(ctx, ch) == LJ_CH_INTENSITY;  /* 03 s5.9: SI and HTP / LoTP on intensity-class channels only */
        win = lj_seq_intensity_rule(intensity ? mode : LJ_SEQINT_DEFAULT, on, prog, faded, si, &v);
        merged[ch] = intensity ? v : faded[win];               /* output = the winner's faded value x SI (LORD_ENGINE-17) */
        layer_of[ch] = LJ_L_CUE;
    }
}

int lj_merge_statics_order(const lj_runner *st, int order[LJ_STATIC_RUNNERS])
{
    int n = 0, i, j;
    for (i = 0; i < LJ_STATIC_RUNNERS; i++) {                  /* insertion sort by activation time: LTP, the latest activation wins per channel [03 s8, C5] */
        if (!lj_runner_active(&st[i])) continue;
        for (j = n; j > 0 && st[order[j - 1]].activated_us > st[i].activated_us; j--) order[j] = order[j - 1];
        order[j] = i; n++;
    }
    return n;
}

static void lj_merge_statics(const lj_runner *st, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int order[LJ_STATIC_RUNNERS], n = lj_merge_statics_order(st, order), i;
    for (i = 0; i < n; i++) lj_merge_runner(&st[order[i]], LJ_L_STATICS, merged, layer_of);
}

void lj_merge(const uint8_t default_out[LJ_CHANS], const lj_runner *runners, const lj_post_ctx *ctx,
              int seq_int_mode, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int i, k;
    memcpy(merged, default_out, LJ_CHANS);                     /* rank 0: DefaultOut, always present */
    memset(layer_of, LJ_L_DEFAULT, LJ_CHANS);
    for (i = 1; i < LJ_L_COUNT; i++) {
        switch (LJ_LAYER_ORDER[i]) {
        case LJ_L_CUE:     lj_merge_cue(&runners[LJ_RUN_SLOT0], ctx, seq_int_mode, merged, layer_of); break;
        case LJ_L_FLASH:   if (lj_runner_active(&runners[LJ_RUN_FLASH])) lj_merge_runner(&runners[LJ_RUN_FLASH], LJ_L_FLASH, merged, layer_of); break;   /* C2, rig M12 */
        case LJ_L_CURRENT: if (lj_runner_active(&runners[LJ_RUN_CURRENT])) lj_merge_runner(&runners[LJ_RUN_CURRENT], LJ_L_CURRENT, merged, layer_of); break;   /* C1; blind: val[]/st[] hold the buffer copied at blind entry (F21, LORD_ENGINE-13) */
        case LJ_L_STATICS: lj_merge_statics(&runners[LJ_RUN_STATIC0], merged, layer_of); break;
        case LJ_L_BGCUE:                                       /* C3: slot 5 down to 1, first Snap/Fade wins [03 s7] */
            for (k = 0; k < LJ_BG_SLOTS; k++) if (lj_runner_active(&runners[LJ_RUN_BG0 + k])) lj_merge_runner(&runners[LJ_RUN_BG0 + k], LJ_L_BGCUE, merged, layer_of);
            break;
        default: break;                                        /* LJ_L_GENMACRO: lj_genmacro_apply in the engine; LJ_L_SMOKE: lj_engine.c after rank 6 (C12) */
        }
    }
}
