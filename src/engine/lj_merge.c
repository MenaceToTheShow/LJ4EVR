/* lj_merge.c - the layer stack, ranks 0-5 and 7 (design 3.4, 11 s3.4; ranks 2-4,
 * 6-7 PROPOSAL rig M12-M15). Rank 6 (generic macro) is applied by the engine
 * after this call (lj_genmacro_apply) because it needs the macro record; rank 7
 * (smoke/haze control) has no W1 source. A layer writes only where its state
 * is Snap or Fade [11 s3.4]. */
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

static int lj_merge_si(int v, int si) { return (v * si + 127) / 255; }   /* sequence intensity 0..255 [03 s5.9] */

static void lj_merge_cue(const lj_runner *slots, const lj_post_ctx *ctx, int mode, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int ch, k;
    uint8_t act[LJ_CUE_SLOTS];
    int any = 0;
    for (k = 0; k < LJ_CUE_SLOTS; k++) { act[k] = (uint8_t)lj_runner_active(&slots[k]); any |= act[k]; }
    if (!any) return;
    for (ch = 0; ch < LJ_CHANS; ch++) {
        int win = -1, intensity;
        for (k = LJ_CUE_SLOTS - 1; k >= 0; k--)                /* slot 12 down to 1, first Snap/Fade wins [03 s5.3, F23] */
            if (act[k] && slots[k].st[ch] != LJ_OFF) { win = k; break; }
        if (win < 0) continue;
        intensity = lj_post_class(ctx, ch) == LJ_CH_INTENSITY;
        if (intensity && mode != LJ_SEQINT_DEFAULT) {          /* HTP / LoTP: selection by programmed x SI (F18, LORD_ENGINE-17) */
            int best = -1, bestkey = 0;
            for (k = LJ_CUE_SLOTS - 1; k >= 0; k--) {
                const lj_runner *r = &slots[k];
                int key;
                if (!act[k] || r->st[ch] == LJ_OFF) continue;
                key = lj_merge_si(r->seq->scenes[r->scene].val[ch], r->seq_int);
                if (best < 0 || (mode == LJ_SEQINT_HTP ? key > bestkey : key < bestkey)) { best = k; bestkey = key; }
            }
            win = best;
        }
        merged[ch] = intensity ? (uint8_t)lj_merge_si(slots[win].val[ch], slots[win].seq_int) : slots[win].val[ch];   /* output = the winner's faded value x SI */
        layer_of[ch] = LJ_L_CUE;
    }
}

static void lj_merge_statics(const lj_runner *st, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS])
{
    int order[LJ_STATIC_RUNNERS], n = 0, i, j;
    for (i = 0; i < LJ_STATIC_RUNNERS; i++) {                  /* insertion sort by activation time: LTP, the latest activation wins per channel [03 s8, C5] */
        if (!lj_runner_active(&st[i])) continue;
        for (j = n; j > 0 && st[order[j - 1]].activated_us > st[i].activated_us; j--) order[j] = order[j - 1];
        order[j] = i; n++;
    }
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
        default: break;                                        /* LJ_L_GENMACRO: lj_genmacro_apply in the engine; LJ_L_SMOKE: W2 (smoke control, C12) */
        }
    }
}
