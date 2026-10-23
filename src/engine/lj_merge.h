/* lj_merge.h - the layer stack (design 3.4): merge every runner's sparse
 * contribution in LJ_LAYER_ORDER rank order into merged[] and layer_of[];
 * a layer writes only where its state is Snap or Fade [11 s3.4]. */
#ifndef LJ_ENGINE_MERGE_H
#define LJ_ENGINE_MERGE_H

#include <stdint.h>
#include "engine/lj_engine.h"

/* rank 0: default_out always present; then the runner ranks. transparent_cue:
 * a transparent cue's empty slots do not overwrite [03 s11.3]. seq_int_mode:
 * enum lj_seq_int_mode for the cue slots' intensity-class channels [03 s5.9]. */
void lj_merge(const uint8_t default_out[LJ_CHANS], const lj_runner *runners /* LJ_RUN_COUNT */, const lj_post_ctx *ctx,
              int seq_int_mode, uint8_t merged[LJ_CHANS], uint8_t layer_of[LJ_CHANS]);
/* W6 LORD_ENGINE-8: the active static runners (indices from 0) in activation order, oldest first; returns the count.
 * lj_merge and the engine's External Application string copy share it, so both layer the statics the same way. */
int lj_merge_statics_order(const lj_runner *st /* LJ_STATIC_RUNNERS */, int order[LJ_STATIC_RUNNERS]);

#endif /* LJ_ENGINE_MERGE_H */
