/* lj_engine.c - W0 deterministic stub of the 40 Hz tick (design 2.3, 3).
 * The API, lj_frame and the thread-ownership contract are final; the body is a
 * fixed pattern drawn from the committed trig table so the three CI frame dumps
 * (windows/macos/linux) must be byte-identical. W1 replaces the body of
 * lj_engine_tick with design 3.2 and keeps everything else. */
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_trig_gen.h"
#include "core/lj_log.h"

struct lj_engine {
    lj_frame               frame;
    lj_cmd                 cmds[LJ_ENGINE_CMD_CAP];     /* design 2.3 rule 1: a plain array, appended under lj_lock */
    int                    ncmd;
    const struct lj_show  *show;
    lj_engine_status       st;
};

lj_engine *lj_engine_create(void)
{
    return (lj_engine *)calloc(1, sizeof(lj_engine));     /* the one allocation; NULL on failure (no lj_fatal path in engine/) */
}

void lj_engine_destroy(lj_engine *e)
{
    free(e);
}

void lj_engine_bind(lj_engine *e, const struct lj_show *show)
{
    e->show = show;
}

int lj_engine_cmd(lj_engine *e, const lj_cmd *cmd)
{
    if (e->ncmd >= LJ_ENGINE_CMD_CAP) {
        e->st.drops++;
        lj_log("engine: command array full (%d), dropped kind %u", LJ_ENGINE_CMD_CAP, (unsigned)cmd->kind);
        return -1;
    }
    e->cmds[e->ncmd++] = *cmd;
    return 0;
}

void lj_engine_tick(lj_engine *e, uint64_t now_us)
{
    lj_frame *f = &e->frame;
    uint64_t t = f->tick_no;
    int ch, link;
    (void)now_us;
    e->ncmd = 0;                                               /* step 1: drain; the stub acts on nothing */
    for (ch = 0; ch < LJ_CHANS; ch++) {                        /* steps 2-4: a moving pattern, int16 table only */
        f->merged[ch] = (uint8_t)((lj_sin4096[(t * 7 + (uint64_t)ch * 3) & 4095] >> 8) + 128);
        f->layer_of[ch] = 0;
    }
    for (link = 0; link < LJ_LINKS; link++)                    /* step 5: out[] = merged split per link */
        memcpy(f->out[link], f->merged + link * LJ_LINK_CH, LJ_LINK_CH);
    f->tick_no = t + 1;                                        /* step 6: publish */
}

const lj_frame *lj_engine_frame(const lj_engine *e)
{
    return &e->frame;
}

void lj_engine_view(lj_engine *e, lj_engine_status *out)
{
    *out = e->st;
    e->st.pending_link_cue = e->st.pending_bg = e->st.pending_genmacro = 0;   /* handed out once (design 2.3 rule 3) */
}
