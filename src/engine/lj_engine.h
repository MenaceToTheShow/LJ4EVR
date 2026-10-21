/* lj_engine.h - the 40 Hz integer tick (design 2.3, 3).
 * W0 ships a deterministic stub body (lj_engine.c): the API, lj_frame and the
 * thread-ownership contract are final; the calculation is W1. No SDL, no float,
 * no <stdatomic.h>; the engine never allocates after lj_engine_create() and
 * never calls lj_table_* (tools/test_layering.py greps for it).
 *
 * Record types the commands carry (lj_cue, lj_sequence, lj_bgcue, lj_genmacro,
 * lj_show) are the model's (model/lj_model.h, design 4); only pointers cross
 * this interface, so they are forward-declared here and this header compiles
 * alone. */
#ifndef LJ_ENGINE_ENGINE_H
#define LJ_ENGINE_ENGINE_H

#include <stdint.h>
#include "core/lj_types.h"

struct lj_cue; struct lj_sequence; struct lj_bgcue; struct lj_genmacro; struct lj_show;

typedef struct lj_engine lj_engine;

typedef struct { uint8_t out[LJ_LINKS][LJ_LINK_CH]; uint8_t merged[LJ_CHANS]; uint8_t layer_of[LJ_CHANS]; uint64_t tick_no; } lj_frame;

enum lj_cmd_kind {
    LJ_CMD_NONE = 0,
    LJ_CMD_LOAD_CUE,      /* carries the pinned pointers below; a = cue number */
    LJ_CMD_GO, LJ_CMD_STOP, LJ_CMD_TRIG, LJ_CMD_ENABLE, LJ_CMD_SET_MASTER, LJ_CMD_BLACKOUT, LJ_CMD_FREEZE,
    LJ_CMD_STATIC, LJ_CMD_FLASH, LJ_CMD_OVERWRITE, LJ_CMD_TIME, LJ_CMD_SACN_CFG,
    LJ_CMD_COUNT
};
typedef struct {
    uint8_t  kind;                       /* enum lj_cmd_kind */
    uint64_t at_us;                      /* UI-thread timestamp (lj_clock_us) so chase taps resolve to 1 ms, design 3.1 */
    uint32_t a, b;                       /* per kind: slot/level/link/universe ... */
    const struct lj_cue *cue; const struct lj_sequence *seq[LJ_CUE_SLOTS]; const struct lj_bgcue *bg; const struct lj_genmacro *gm;   /* LJ_CMD_LOAD_CUE: pinned by the UI thread; seq[i] == NULL = "load error" */
} lj_cmd;

/* Engine -> UI, copied under lj_lock. pending_* are requests (design 2.3 rule 3): 0 = none. */
typedef struct {
    uint8_t  slot_status[LJ_CUE_SLOTS];  /* LJ_SLOT_* */
    uint16_t slot_scene[LJ_CUE_SLOTS], slot_scenes[LJ_CUE_SLOTS];   /* scene X : Y per slot */
    uint8_t  gm;                         /* grand master 0..255 */
    uint32_t tick_us_last;               /* measured tick duration, microseconds (Debug window) */
    uint32_t drops;                      /* commands dropped because the array was full */
    uint32_t pending_link_cue, pending_bg, pending_genmacro;        /* engine -> UI requests, 0 = none */
} lj_engine_status;   /* design 2.3 names this struct lj_engine_view; C puts typedef and function names in one namespace, so the TYPE is lj_engine_status and the FUNCTION keeps the design name */
enum { LJ_SLOT_EMPTY = 0, LJ_SLOT_RUNNING, LJ_SLOT_STOPPED, LJ_SLOT_LOAD_ERROR, LJ_SLOT_DISABLED };

#define LJ_ENGINE_CMD_CAP 256            /* the command array; a full array logs and drops (design 2.3 rule 1) */

lj_engine      *lj_engine_create(void);                              /* fixed-size arenas; never allocates after this; NULL on failure */
void            lj_engine_destroy(lj_engine *e);                     /* after the engine thread has stopped; NULL is a no-op */
void            lj_engine_bind(lj_engine *e, const struct lj_show *show);   /* caller holds lj_lock */
int             lj_engine_cmd(lj_engine *e, const lj_cmd *cmd);      /* caller holds lj_lock; appends to the 256-entry array drained at the next tick; -1 + log when full */
void            lj_engine_tick(lj_engine *e, uint64_t now_us);       /* one calculation pass (design 3.2); pure given its inputs */
const lj_frame *lj_engine_frame(const lj_engine *e);                 /* the live buffer; the UI copies it under lj_lock */
void            lj_engine_view(lj_engine *e, lj_engine_status *out);        /* copied under lj_lock; clears the pending_* fields it hands out (so not const) */

#endif /* LJ_ENGINE_ENGINE_H */
