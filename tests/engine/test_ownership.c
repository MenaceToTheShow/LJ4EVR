/* test_ownership.c - the thread-ownership contract of design 2.3: a cue is
 * loaded with pinned records; the UI side deletes and re-saves the sequence
 * while a runner uses it (a LOAD_CUE with seq[i] = NULL in the same critical
 * section, the old record freed only after the tick swapped it out, then a
 * new record pinned); 300 commands between two ticks -> 44 logged drops
 * (rule 1); pending_* fields are handed out once (rule 3). Under ASan the
 * free-after-swap order is what proves rule 6. ME-121 (ME-2, W3): the same order for a generic macro on the cue
 * source: LOAD_GENMACRO gm NULL under the lock, a tick, then the free; a re-saved record pinned by the next load.
 * The app path (lj_app_delete_record(LJ_REC_MACRO)) is test_uiscript_w3 section 9b. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_model.h"
#include "core/lj_log.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int s_drop_lines;
static void lj_count_drops(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "command array full")) s_drop_lines++;
}

static lj_profile s_dimmer;

static lj_sequence *lj_seq_make(uint32_t number, uint8_t level)
{
    lj_sequence *s = (lj_sequence *)calloc(1, sizeof *s);
    s->number = number; s->n = 2; s->scenes = (lj_scene *)calloc(2, sizeof(lj_scene));
    s->scenes[0].scene_ms = s->scenes[1].scene_ms = 1000; s->scenes[0].fade_ms = s->scenes[1].fade_ms = 50;
    s->scenes[0].val[0] = level; s->scenes[0].state[0] = LJ_SNAP;
    s->scenes[1].val[0] = (uint8_t)(level / 2); s->scenes[1].state[0] = LJ_SNAP;
    return s;
}

static void lj_seq_free(lj_sequence *s) { free(s->scenes); free(s); }

int lj_test_engine_ownership(int argc, char **argv);
int lj_test_engine_ownership(int argc, char **argv)
{
    lj_show *show = (lj_show *)calloc(1, sizeof *show);   /* test code: no model bodies needed */
    lj_engine *e = lj_engine_create();
    lj_sequence *seq = lj_seq_make(1, 200), *seq2;
    lj_cue *cue = (lj_cue *)calloc(1, sizeof *cue);
    lj_engine_status st;
    lj_cmd cmd;
    const lj_frame *f;
    int i, ok = 0;
    uint64_t now = 0;
    (void)argc; (void)argv;
    memset(&s_dimmer, 0, sizeof s_dimmer);
    s_dimmer.footprint = 1; s_dimmer.channel_class[0] = LJ_CH_INTENSITY; s_dimmer.use_gm = 1; s_dimmer.intensity.enabled = 1; s_dimmer.intensity.max = 255;
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].link = 1; show->fixture[0].address = 1;
    cue->number = 7; cue->cue_time_pct = 100; cue->slot[0].seq = 1; cue->slot[0].enabled = 1; cue->bg_cue = 5;   /* bg cue 5 names a record the UI has not pinned */
    lj_engine_bind(e, show);
    lj_engine_bind_profile(e, 0, &s_dimmer);
    f = lj_engine_frame(e);

    /* rule 2: the UI pins the records in the command */
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 7; cmd.cue = cue; cmd.seq[0] = seq;
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    lj_engine_tick(e, now);
    lj_engine_view(e, &st);
    CHECK(st.slot_status[0] == LJ_SLOT_RUNNING && st.slot_scene[0] == 1 && st.slot_scenes[0] == 2);
    CHECK(f->out[0][0] == 200 && f->layer_of[0] == LJ_L_CUE);
    CHECK(lj_engine_runner(e, LJ_RUN_SLOT0)->seq == seq && lj_engine_runner(e, LJ_RUN_SLOT0)->seq_no == 1);
    /* rule 3: the latched bg cue the UI did not pin is a request, handed out once */
    CHECK(st.pending_bg == 5 && st.cue_number == 7);
    lj_engine_view(e, &st);
    CHECK(st.pending_bg == 0);

    /* rule 6: delete the sequence while the runner uses it: the UI hands the engine NULL
     * (load error) under the lock, ticks, and only then frees the record */
    cmd.seq[0] = NULL;
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    now += LJ_TICK_US_DEFAULT;
    lj_engine_tick(e, now);                                    /* the swap: the runner no longer points at seq */
    CHECK(lj_engine_runner(e, LJ_RUN_SLOT0)->seq == NULL && lj_engine_runner(e, LJ_RUN_SLOT0)->seq_no == 1);
    lj_seq_free(seq);                                          /* freed after the swap (ASan would report a use otherwise) */
    seq = NULL;
    lj_engine_view(e, &st);
    CHECK(st.slot_status[0] == LJ_SLOT_LOAD_ERROR);
    now += LJ_TICK_US_DEFAULT;
    lj_engine_tick(e, now);
    CHECK(f->out[0][0] == 0 && f->layer_of[0] == LJ_L_DEFAULT);   /* a load-error slot contributes nothing */

    /* re-save: a new record under the same number is pinned by the next load */
    seq2 = lj_seq_make(1, 100);
    cmd.seq[0] = seq2;
    CHECK(lj_engine_cmd(e, &cmd) == 0);
    now += LJ_TICK_US_DEFAULT;
    lj_engine_tick(e, now);
    lj_engine_view(e, &st);
    CHECK(st.slot_status[0] == LJ_SLOT_RUNNING && f->out[0][0] == 100);
    CHECK(lj_engine_runner(e, LJ_RUN_SLOT0)->seq == seq2);

    /* ME-121: generic macro 3 on the cue source; delete (gm NULL, tick, free), re-save, tick (ASan: no use after free) */
    {
        lj_genmacro *gm = (lj_genmacro *)calloc(1, sizeof *gm), *gm2 = (lj_genmacro *)calloc(1, sizeof *gm2);
        if (!gm || !gm2) { CHECK(0); free(gm); free(gm2); }
        else {
            gm->number = gm2->number = 3;
            gm->ch[0].shape = gm2->ch[0].shape = 1; gm->ch[0].amp_pos = gm2->ch[0].amp_pos = 50; gm->ch[0].cycle_ds = gm2->ch[0].cycle_ds = 10;
            memset(&cmd, 0, sizeof cmd);
            cmd.kind = LJ_CMD_LOAD_GENMACRO; cmd.c = LJ_GM_SRC_CUE; cmd.gm = gm;
            CHECK(lj_engine_cmd(e, &cmd) == 0);
            now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now); lj_engine_view(e, &st);
            CHECK(st.gm_cue_number == 3);
            cmd.gm = NULL;                                     /* the delete: the engine lets go first */
            CHECK(lj_engine_cmd(e, &cmd) == 0);
            now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now); lj_engine_view(e, &st);
            CHECK(st.gm_cue_number == 0);
            free(gm);                                          /* after the swap */
            now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now);
            cmd.gm = gm2;                                      /* the re-save under the same number */
            CHECK(lj_engine_cmd(e, &cmd) == 0);
            now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now); now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now); lj_engine_view(e, &st);
            CHECK(st.gm_cue_number == 3);
            cmd.gm = NULL; CHECK(lj_engine_cmd(e, &cmd) == 0);
            now += LJ_TICK_US_DEFAULT; lj_engine_tick(e, now);
            free(gm2);
        }
    }

    /* rule 1: 300 commands between two ticks -> 256 accepted, 44 logged drops */
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_GO; cmd.a = LJ_RUN_SLOT0;
    s_drop_lines = 0;
    lj_log_set_sink(lj_count_drops, NULL);
    for (i = 0; i < 300; i++) ok += lj_engine_cmd(e, &cmd) == 0;
    lj_log_set_sink(NULL, NULL);
    CHECK(ok == LJ_ENGINE_CMD_CAP && s_drop_lines == 300 - LJ_ENGINE_CMD_CAP);
    lj_engine_view(e, &st);
    CHECK(st.drops == 300 - LJ_ENGINE_CMD_CAP);
    now += LJ_TICK_US_DEFAULT;
    lj_engine_tick(e, now);                                    /* drains */
    CHECK(lj_engine_cmd(e, &cmd) == 0);

    /* unbind before the records go (rule 6 for the show itself) */
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.cue = NULL;
    lj_engine_cmd(e, &cmd);
    now += LJ_TICK_US_DEFAULT;
    lj_engine_tick(e, now);
    lj_engine_view(e, &st);
    CHECK(st.slot_status[0] == LJ_SLOT_EMPTY && st.cue_number == 0);
    lj_engine_bind(e, NULL);
    lj_engine_destroy(e);
    lj_seq_free(seq2); free(cue); free(show);
    printf("test_engine_ownership: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
