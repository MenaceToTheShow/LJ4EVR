/* lj_app_seam.c - see lj_app_seam.h. The W1 record-loading seam moved here from lj_app.c unchanged
 * except that the library is ctx->lib and the view copy is ctx->view (the desktop LEDs, the cue and
 * master windows and the script read that one copy; w1-contracts seam 3). */
#include "app/lj_app_seam.h"
#include "app/lj_app_cuelist_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "fixlib/lj_profile.h"
#include "io/lj_audio.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "io/lj_midi.h"
#include "io/lj_sacn.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_defaultout.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_patch.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "model/lj_statics.h"
#include "windows/lj_bgcue_form.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_global_patch.h"
#include "windows/lj_master_intensity.h"
#include "windows/lj_media_form.h"
#include "windows/lj_prefs.h"
#include "windows/lj_snapshot.h"
#include "windows/lj_striking_lamps.h"
#include "windows/lj_windows.h"

#define LJ_STRIKE_HOLD_MS  5000u      /* PROPOSAL rig: the lamp-on value is held this long when the profile has no lamp_on_ms */

/* ponytail: one app per process (as lj_app.c); the state the seam owns. */
static struct {
    int cue_edit;
    lj_sysprefs prefs;
    lj_midi_map midi;
    int midi_open;                       /* the device in midi.device opened (ME-95: a failed open keeps the operator's DeviceCode) */
    lj_frame snap;                       /* the UI-owned frame LJ_CMD_SNAPSHOT_REQ fills (seam: read only after view.snapshot_tick == snap.tick_no) */
    struct { uint64_t at_us; uint64_t clear_us[LJ_FIXTURES]; } strike;   /* Auto LampOn due time; per fixture, when its lamp-on overwrite is released */
    lj_bgcue *bg_unsaved;                /* the Background Cue window's record before its first save (number 0: no table entry); ponytail: not freed at exit, one record */
} lj_s;

static lj_library *lj_lib(void) { return lj_ctx_get()->lib; }

/* ---- the record-loading seam (design 2.3 rules 2 and 6) ---- */

typedef struct { lj_sequence *seq[LJ_CUE_SLOTS + LJ_BG_SLOTS]; int n; } lj_app_fresh;   /* records loaded from disk this call, inserted under the lock */

static int lj_app_rec_path(enum lj_rec_kind kind, uint32_t number, char *out, size_t cap)
{
    if (lj_lib() && lj_library_record_path(lj_lib(), kind, number, out, cap) == 0) return 0;
    lj_log("app: no path for record kind %d number %u", (int)kind, (unsigned)number);
    return -1;
}

/* The pinned pointer for a sequence number: the table's record, else loaded from disk now; NULL = load error [03 s5.1]. */
static const lj_sequence *lj_app_seq_for(uint32_t number, lj_app_fresh *fresh)
{
    lj_show *show = lj_ctx_get()->show;
    lj_sequence *s;
    char path[LJ_PATH_CAP], err[128];
    int i;
    if (!number) return NULL;
    if ((s = (lj_sequence *)lj_table_get(&show->sequences, number)) != NULL) return s;
    for (i = 0; i < fresh->n; i++) if (fresh->seq[i]->number == number) return fresh->seq[i];
    if (fresh->n >= LJ_COUNT_OF(fresh->seq) || lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0) return NULL;
    s = lj_sequence_new_rec();
    if (lj_sequence_load(path, s, err, sizeof err) != 0) { lj_log("sequence %04u: load error: %s", (unsigned)number, err); lj_sequence_free(s); return NULL; }
    s->number = number;
    fresh->seq[fresh->n++] = s;
    return s;
}

static lj_bgcue *lj_app_bg_for(uint32_t number, lj_cmd *cmd, lj_app_fresh *fresh)
{
    char path[LJ_PATH_CAP], err[128];
    lj_bgcue *bg;
    int i;
    if (!number || lj_app_rec_path(LJ_REC_BG, number, path, sizeof path) != 0) return NULL;
    bg = lj_bgcue_new_rec();
    if (lj_bgcue_load(path, bg, err, sizeof err) != 0) { lj_log("background cue %04u: load error: %s", (unsigned)number, err); lj_bgcue_free(bg); return NULL; }
    bg->number = number;
    cmd->bg = bg;
    for (i = 0; i < LJ_BG_SLOTS; i++) cmd->bgseq[i] = lj_app_seq_for(bg->slot[i].seq, fresh);
    return bg;
}

static lj_genmacro *lj_app_gm_for(uint32_t number)
{
    char path[LJ_PATH_CAP], err[128];
    lj_genmacro *gm;
    if (!number || lj_app_rec_path(LJ_REC_MACRO, number, path, sizeof path) != 0) return NULL;
    gm = lj_genmacro_new_rec();
    if (lj_genmacro_load(path, gm, err, sizeof err) != 0) { lj_log("generic macro %04u: load error: %s", (unsigned)number, err); free(gm); return NULL; }
    gm->number = number;
    return gm;
}

/* ME-102: caller holds lj_lock. 0 = posted (or no engine); -1 = the command array is full, so the engine still holds the
 * record this post was meant to replace: the caller keeps it (a leak, logged once), never frees it under the engine. */
int lj_app_post_locked(const lj_cmd *cmd)
{
    static int warned;
    lj_ctx *c = lj_ctx_get();
    if (!c->engine || lj_engine_cmd(c->engine, cmd) == 0) return 0;
    if (!warned) { warned = 1; lj_log("app: engine command array full: the replaced record is kept, not freed (ME-102; logged once)"); }
    return -1;
}

/* Caller holds lj_lock: the fresh records enter the tables and the command is appended in the same critical section (rule 6). 0 / -1 (ME-102) */
static int lj_app_commit(lj_app_fresh *fresh, lj_cmd *cmd)
{
    lj_ctx *c = lj_ctx_get();
    int i;
    for (i = 0; i < fresh->n; i++) lj_table_put(&c->show->sequences, fresh->seq[i]->number, fresh->seq[i]);
    if (cmd->at_us == 0) cmd->at_us = lj_clock_us();
    return lj_app_post_locked(cmd);
}

/* Caller holds lj_lock: re-pins the current cue from the tables (after a sequence was replaced or deleted); exclude = a number now gone (slot -> load error). */
static int lj_app_repin_cue(uint32_t exclude)
{
    lj_show *show = lj_ctx_get()->show;
    const lj_cue *cue = show->current_cue;
    lj_cmd cmd;
    lj_bgcue *bg;
    int i;
    if (!cue) return 0;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = cue->number; cmd.b = (uint32_t)lj_s.cue_edit; cmd.cue = cue;
    for (i = 0; i < LJ_CUE_SLOTS; i++) {
        uint32_t n = cue->slot[i].seq;
        cmd.seq[i] = (n && n != exclude) ? (const lj_sequence *)lj_table_get(&show->sequences, n) : NULL;
    }
    if (cue->bg_cue && (bg = (lj_bgcue *)lj_table_get(&show->bgcues, cue->bg_cue)) != NULL) {
        cmd.bg = bg;
        for (i = 0; i < LJ_BG_SLOTS; i++) cmd.bgseq[i] = bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&show->sequences, bg->slot[i].seq) : NULL;
    }
    if (cue->gen_macro) cmd.gm = (const lj_genmacro *)lj_table_get(&show->macros, cue->gen_macro);
    cmd.at_us = lj_clock_us();
    return lj_app_post_locked(&cmd);
}

void lj_app_repin_current_cue(void) { lj_lock(); (void)lj_app_repin_cue(0); lj_unlock(); }   /* nothing is freed after it */

/* Caller holds lj_lock: every Statics 2 link slot on `number` gets the table's record now (NULL once deleted) before the old one
 * is freed (rule 6, ME-88). The engine acts only on the slots it holds; the rest ignore it. ponytail: one post per linking slot. */
static int lj_app_repin_statics(uint32_t number)
{
    lj_show *show = lj_ctx_get()->show;
    lj_cmd cmd;
    int p, i, rc = 0;
    if (!lj_ctx_get()->engine) return 0;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_ACTIVATE; cmd.c = LJ_STATIC_REPIN; cmd.at_us = lj_clock_us();
    cmd.seq[0] = (const lj_sequence *)lj_table_get(&show->sequences, number);
    for (p = 0; p < show->npages; p++)
        for (i = 0; i < LJ_STATICS2_SLOTS_MAX; i++)
            if (show->pages[p].slot[i].kind == LJ_STATIC_LINK && show->pages[p].slot[i].link_seq == number) {
                cmd.a = (uint32_t)lj_static_global_index(p + 1, i + 1); cmd.st = &show->pages[p].slot[i];
                rc |= lj_app_post_locked(&cmd);
            }
    return rc;
}

/* LJ_ACT_PIN_SEQ: a Statics 2 link slot's sequence into the table when it is on disk only (the window then pins it under the lock) */
static void lj_app_pin_seq(uint32_t number)
{
    lj_app_fresh fresh;
    memset(&fresh, 0, sizeof fresh);
    if (!lj_ctx_get()->show || !lj_app_seq_for(number, &fresh) || !fresh.n) return;
    lj_lock();
    lj_table_put(&lj_ctx_get()->show->sequences, fresh.seq[0]->number, fresh.seq[0]);
    lj_unlock();
}

int lj_app_load_cue(uint32_t number, int edit)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    lj_cue *cue, *old, *oldcur;
    lj_bgcue *bg = NULL, *oldbg = NULL;
    lj_genmacro *gm = NULL, *oldgm = NULL;
    lj_app_fresh fresh;
    lj_cmd cmd;
    char path[LJ_PATH_CAP], err[128];
    int i, posted;

    if (!show || !number || lj_app_rec_path(LJ_REC_CUE, number, path, sizeof path) != 0) return -1;
    cue = lj_cue_new_rec();
    if (lj_cue_load(path, cue, err, sizeof err) != 0) { lj_log("cue %04u: load error: %s", (unsigned)number, err); lj_cue_free(cue); return -1; }
    cue->number = number;
    memset(&fresh, 0, sizeof fresh);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = number; cmd.b = (uint32_t)(edit != 0); cmd.cue = cue;
    for (i = 0; i < LJ_CUE_SLOTS; i++) cmd.seq[i] = lj_app_seq_for(cue->slot[i].seq, &fresh);
    if (cue->bg_cue) bg = lj_app_bg_for(cue->bg_cue, &cmd, &fresh);
    if (cue->gen_macro) gm = lj_app_gm_for(cue->gen_macro);       /* NULL: the engine asks again through pending_genmacro, which logs once more and runs the cue without it */
    cmd.gm = gm;

    lj_lock();
    old = (lj_cue *)lj_table_get(&show->cues, number);
    lj_table_put(&show->cues, number, cue);
    oldcur = show->current_cue;
    show->current_cue = cue;
    if (bg) { oldbg = (lj_bgcue *)lj_table_get(&show->bgcues, bg->number); lj_table_put(&show->bgcues, bg->number, bg); }
    if (gm) { oldgm = (lj_genmacro *)lj_table_get(&show->macros, gm->number); lj_table_put(&show->macros, gm->number, gm); }
    posted = lj_app_commit(&fresh, &cmd) == 0;
    if (c->forms_ini && lj_ini_geti(c->forms_ini, "CUE", "Flash Auto Release", 0) && c->engine) {   /* "Release Flash on Load" (windows-a persists the box) [03 s5.10] */
        lj_cmd fr;
        memset(&fr, 0, sizeof fr);
        fr.kind = LJ_CMD_FLASH; fr.at_us = cmd.at_us;            /* b 0 c 0: release, latched or not */
        lj_engine_cmd(c->engine, &fr);
    }
    lj_unlock();
    /* freed after the engine was handed the replacement in the same critical section (rule 6); a never-saved current cue is not in the table;
     * not freed at all when the post failed (ME-102: the engine still points at them) */
    if (!posted) { old = oldcur = NULL; oldbg = NULL; oldgm = NULL; }
    if (old && old != cue) lj_cue_free(old);
    if (oldcur && oldcur != cue && oldcur != old && lj_table_get(&show->cues, oldcur->number) != oldcur) lj_cue_free(oldcur);
    if (oldbg && oldbg != bg) lj_bgcue_free(oldbg);
    if (oldgm && oldgm != gm) free(oldgm);
    lj_s.cue_edit = edit != 0;
    c->modified[LJ_MOD_CUE] = 0; c->cue_saved = 1;
    return 0;
}

int lj_app_load_bgcue(uint32_t number)
{
    lj_show *show = lj_ctx_get()->show;
    lj_bgcue *bg = NULL, *old = NULL;
    lj_app_fresh fresh;
    lj_cmd cmd;
    if (!show) return -1;
    memset(&fresh, 0, sizeof fresh);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_BG; cmd.a = number;
    if (number && (bg = lj_app_bg_for(number, &cmd, &fresh)) == NULL) return -1;   /* number 0 = clear; a missing number logged its load error above */
    lj_lock();
    if (bg) { old = (lj_bgcue *)lj_table_get(&show->bgcues, number); lj_table_put(&show->bgcues, number, bg); }
    if (lj_app_commit(&fresh, &cmd) != 0) old = NULL;            /* ME-102 */
    lj_unlock();
    if (old && old != bg) lj_bgcue_free(old);                    /* unpinned: the engine got the new pointer in the critical section above, and drains it before its next read */
    return 0;
}

/* The Background Cue window's edited record (a drop, Clear Slot, a save): a fresh copy pinned in the bgcues table (or held
 * here while it has no number) and LJ_CMD_LOAD_BG in the same critical section; the replaced record is freed after the post. */
static int lj_app_bgcue_apply(const lj_bgcue *work)
{
    lj_show *show = lj_ctx_get()->show;
    lj_bgcue *bg, *old = NULL, *old2 = NULL;
    lj_app_fresh fresh;
    lj_cmd cmd;
    int i;
    if (!show || !work || (bg = lj_bgcue_new_rec()) == NULL) return -1;
    *bg = *work;
    memset(&fresh, 0, sizeof fresh);
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_BG; cmd.a = bg->number; cmd.bg = bg;
    for (i = 0; i < LJ_BG_SLOTS; i++) cmd.bgseq[i] = lj_app_seq_for(bg->slot[i].seq, &fresh);
    lj_lock();
    if (bg->number) { old = (lj_bgcue *)lj_table_get(&show->bgcues, bg->number); lj_table_put(&show->bgcues, bg->number, bg); old2 = lj_s.bg_unsaved; lj_s.bg_unsaved = NULL; }
    else { old = lj_s.bg_unsaved; lj_s.bg_unsaved = bg; }
    if (lj_app_commit(&fresh, &cmd) != 0) old = old2 = NULL;     /* ME-102 */
    lj_unlock();
    if (old && old != bg) lj_bgcue_free(old);
    lj_bgcue_free(old2);
    return 0;
}

/* restart: the pending_genmacro service passes the macro's restart_on_load (b = restart, w3-contracts); Add Macro to
 * Cue keeps b = 0 (CHM 5841). */
static int lj_app_post_genmacro(uint32_t number, int restart)
{
    lj_show *show = lj_ctx_get()->show;
    lj_genmacro *gm = lj_app_gm_for(number), *old = NULL;
    lj_cmd cmd;
    if (!show) { free(gm); return -1; }
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_GENMACRO; cmd.a = number; cmd.gm = gm; cmd.c = LJ_GM_SRC_CUE;   /* NULL = engine off for this cue (load error logged); W3: the cue's rank-6 source */
    cmd.b = restart && gm && gm->restart_on_load;
    lj_lock();
    if (gm) { old = (lj_genmacro *)lj_table_get(&show->macros, number); lj_table_put(&show->macros, number, gm); }
    cmd.at_us = lj_clock_us();
    if (lj_app_post_locked(&cmd) != 0) old = NULL;               /* ME-102 */
    lj_unlock();
    if (old && old != gm) free(old);
    return gm ? 0 : -1;
}
int lj_app_load_genmacro(uint32_t number) { return lj_app_post_genmacro(number, 0); }

/* Swaps the current sequence under the lock, posts LJ_CMD_CURRENT_SEQ, returns the old record for the caller to free. */
static lj_sequence *lj_app_set_current_seq(lj_sequence *s)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *old;
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_CURRENT_SEQ; cmd.a = 0; cmd.b = c->blind; cmd.seq[0] = s; cmd.at_us = lj_clock_us();   /* seq[0] = the pinned current sequence (lj_engine.h table) */
    lj_lock();
    old = c->show->current_seq;
    c->show->current_seq = s;
    c->current_scene = 0;
    if (lj_app_post_locked(&cmd) != 0) old = NULL;               /* ME-102: the caller frees what this returns */
    lj_unlock();
    return old;
}

int lj_app_load_sequence(uint32_t number)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s, *old;
    char path[LJ_PATH_CAP], err[128];
    if (!c->show || !number || lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0) return -1;
    s = lj_sequence_new_rec();
    if (lj_sequence_load(path, s, err, sizeof err) != 0) { lj_log("sequence %04u: load error: %s", (unsigned)number, err); lj_sequence_free(s); return -1; }
    s->number = number; s->modified = 0;
    old = lj_app_set_current_seq(s);
    lj_sequence_free(old);                                        /* the current sequence is never a table record (lj_app_save_sequence reloads a copy) */
    c->modified[LJ_MOD_SEQ] = 0; c->seq_saved = 1;
    return 0;
}

int lj_app_save_sequence(int as_new)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *cur = c->show ? c->show->current_seq : NULL, *copy, *old;
    const char *why;
    char path[LJ_PATH_CAP], err[128];
    uint32_t number;
    if (!cur || !lj_lib()) return -1;
    if ((why = lj_sequence_validate(cur)) != NULL) { lj_log("save sequence: %s", why); return -1; }
    number = (as_new || cur->number == 0) ? lj_library_next_number(lj_lib(), LJ_REC_SEQ) : cur->number;   /* assigned on first save, immutable [F28] */
    if (!number) { lj_log("save sequence: no number from DirInfo.Ini"); return -1; }
    lj_lock(); cur->number = number; cur->modified = 0; lj_unlock();
    if (lj_app_rec_path(LJ_REC_SEQ, number, path, sizeof path) != 0 || lj_sequence_save(path, cur) != 0) return -1;
    copy = lj_sequence_new_rec();                                 /* the table holds what is on disk; the current record stays the editing copy */
    if (lj_sequence_load(path, copy, err, sizeof err) != 0) { lj_log("sequence %04u: reload after save: %s", (unsigned)number, err); lj_sequence_free(copy); return -1; }
    copy->number = number;
    lj_lock();
    old = (lj_sequence *)lj_table_get(&c->show->sequences, number);
    lj_table_put(&c->show->sequences, number, copy);
    if ((lj_app_repin_cue(0)                                      /* editing a sequence changes every cue [03 s5.1]: every runner on this number gets the new pointer */
         | lj_app_repin_statics(number)) != 0) old = NULL;        /* and every Statics 2 slot linked to it (ME-88); ME-102 */
    lj_unlock();
    if (old && old != copy) lj_sequence_free(old);
    c->modified[LJ_MOD_SEQ] = 0; c->seq_saved = 1;
    return 0;
}

int lj_app_save_cue(int as_new)
{
    lj_ctx *c = lj_ctx_get();
    lj_cue *cue = c->show ? c->show->current_cue : NULL, *old;
    const char *why;
    char path[LJ_PATH_CAP];
    uint32_t number;
    if (!cue || !lj_lib()) return -1;
    if ((why = lj_cue_validate(cue)) != NULL) { lj_log("save cue: %s", why); return -1; }
    number = (as_new || cue->number == 0) ? lj_library_next_number(lj_lib(), LJ_REC_CUE) : cue->number;
    if (!number) { lj_log("save cue: no number from DirInfo.Ini"); return -1; }
    lj_lock(); cue->number = number; lj_unlock();
    if (lj_app_rec_path(LJ_REC_CUE, number, path, sizeof path) != 0 || lj_cue_save(path, cue) != 0) return -1;
    lj_lock();
    old = (lj_cue *)lj_table_get(&c->show->cues, number);
    if (old != cue) { lj_table_put(&c->show->cues, number, cue); if (lj_app_repin_cue(0) != 0) old = NULL; }   /* the current cue record becomes the table's (and the engine's); ME-102 */
    lj_unlock();
    if (old && old != cue) lj_cue_free(old);
    c->modified[LJ_MOD_CUE] = 0; c->cue_saved = 1;
    return 0;
}

int lj_app_delete_record(int kind, uint32_t number)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    void *rec = NULL;
    lj_cue *blank = NULL;
    lj_cmd cmd;
    if (!show || !number || !lj_lib()) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.at_us = lj_clock_us();
    switch ((enum lj_rec_kind)kind) {
    case LJ_REC_SEQ:
        lj_lock();
        rec = lj_table_remove(&show->sequences, number);
        if (((show->current_cue ? lj_app_repin_cue(number) : 0)          /* its slots show load error from now on */
             | lj_app_repin_statics(number)) != 0) rec = NULL;           /* a linked static outputs nothing (link error, ME-88); ME-102 */
        if (show->current_seq && show->current_seq->number == number) show->current_seq->number = 0;   /* a number is never reused [F28] */
        lj_unlock();
        lj_sequence_free((lj_sequence *)rec);
        break;
    case LJ_REC_CUE:
        blank = lj_cue_new_rec();
        lj_lock();
        rec = lj_table_remove(&show->cues, number);
        if (show->current_cue == rec) {
            show->current_cue = blank; blank = NULL;
            cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 0; cmd.cue = NULL;        /* every runner handed NULL before the record goes (rule 6) */
            if (lj_app_post_locked(&cmd) != 0) rec = NULL;                /* ME-102 */
        }
        lj_unlock();
        lj_cue_free((lj_cue *)rec);
        lj_cue_free(blank);
        break;
    case LJ_REC_BG:
        lj_lock();
        rec = lj_table_remove(&show->bgcues, number);
        if (rec) { cmd.kind = LJ_CMD_LOAD_BG; cmd.a = 0; cmd.bg = NULL; if (lj_app_post_locked(&cmd) != 0) rec = NULL; }   /* ME-102 */
        lj_unlock();
        lj_bgcue_free((lj_bgcue *)rec);
        break;
    case LJ_REC_MACRO:                                                /* generic macros (W2): the running one is switched off first */
        lj_lock();
        rec = lj_table_remove(&show->macros, number);
        if (rec && c->view.gm_cue_number == number) { cmd.kind = LJ_CMD_LOAD_GENMACRO; cmd.a = 0; cmd.gm = NULL; if (lj_app_post_locked(&cmd) != 0) rec = NULL; }   /* ME-102 (the cue source; the editor's record is ctx->gm_edit, never a table record); ME-117: only when it is the running one, another number is freed directly (the table entry was its only pin) */
        lj_unlock();
        free(rec);
        break;
    default:                                                          /* cue lists (W3): file only */
        break;
    }
    return lj_library_delete_record(lj_lib(), (enum lj_rec_kind)kind, number);
}

/* ---- pending loads (design 2.3 rule 3) and MIDI (rule 5): every idle pass ---- */

void lj_app_service_pending(void)
{
    lj_ctx *c = lj_ctx_get();
    uint32_t link, bg, gm;
    if (!c->engine) return;
    lj_lock(); lj_engine_view(c->engine, &c->view); lj_unlock();     /* the one view copy per pass; the pending_* it hands out are cleared in the engine */
    link = c->view.pending_link_cue; bg = c->view.pending_bg; gm = c->view.pending_genmacro;
    if (link && lj_app_load_cue(link, 0) != 0) lj_log("link to cue %04u: not loaded, the slot stays stopped", (unsigned)link);
    if (bg) lj_app_load_bgcue(bg);
    if (gm) lj_app_post_genmacro(gm, 1);
}

void lj_app_midi_dispatch(void)
{
    uint8_t m[3];
    while (lj_midi_poll(m)) {
        if (m[0] == 0xF1) { lj_app_mtc_feed(m[1]); continue; }       /* W3: MTC quarter-frames feed the cue list's MIDI time code (lj_app_cuelist_host.c) */
        if (lj_ctx_get()->midi) lj_midi_dispatch(lj_ctx_get()->midi, m);   /* lj_fn_run on this (the UI) thread */
    }
}

static void lj_app_midi_cb(void *ud, const uint8_t msg[3]) { (void)ud; lj_midi_post(msg); }   /* the OS MIDI thread: post only (ME-23) */

int lj_app_midi_open(int device)
{
    lj_midi_close();
    lj_s.midi_open = 0;
    lj_s.midi.device = device < 0 ? -1 : device;                      /* kept on a failed open: a load-in with the interface unplugged saves the same DeviceCode (ME-95) */
    if (device < 0) return 0;
    if (lj_midi_open(device, lj_app_midi_cb, NULL) != 0) return -1;
    lj_s.midi_open = 1;
    return 0;
}

static int lj_app_midi_paths(char *ini, char *map, size_t cap)
{
    return (lj_lib() && lj_library_path(lj_lib(), "MIDIIn/MIDIIn.Ini", ini, cap) == 0 && lj_library_path(lj_lib(), "MIDIIn/MIDIMap.dat", map, cap) == 0) ? 0 : -1;
}

static void lj_app_midi_save(void)
{
    char ini[LJ_PATH_CAP], map[LJ_PATH_CAP];
    if (lj_app_midi_paths(ini, map, sizeof ini) != 0 || lj_midi_save(ini, map, &lj_s.midi) != 0) lj_log("app: MIDI-IN settings not saved");
}

/* ---- statics and BG cue (seam 1) ---- */

int lj_app_static_clear(int page, int slot)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *show = c->show;
    int g = lj_static_global_index(page, slot);
    char path[LJ_PATH_CAP];
    lj_cmd cmd;
    if (!show || g < 0 || page > show->npages || slot > show->pages[page - 1].n) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_STATIC_DEACTIVATE; cmd.a = (uint32_t)g; cmd.at_us = lj_clock_us();
    lj_lock();
    if (lj_app_post_locked(&cmd) != 0) { lj_unlock(); lj_log("app: Clear Slot %d/%d refused: the engine still holds the slot (ME-102)", page, slot); return -1; }   /* the engine lets go of the slot before its scene is freed (rule 6) */
    lj_static_slot_clear(&show->pages[page - 1].slot[slot - 1]);
    lj_unlock();
    if (lj_lib() && lj_library_path(lj_lib(), "Statics.dat", path, sizeof path) == 0 && lj_statics_groups_save(path, show->pages, show->npages, c->static_groups) == 0) return 0;
    lj_log("app: Statics.dat not saved after Clear Slot %d/%d", page, slot);
    return -1;
}

int lj_app_bgcue_clear(void)
{
    lj_cmd cmd;
    if (!lj_ctx_get()->engine) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_LOAD_BG; cmd.a = 0; cmd.bg = NULL;              /* the record stays in the table */
    lj_ctx_post(&cmd);
    return 0;
}

/* ---- snapshot [03 s4.7]: the app entries of lj_app.h; the window (windows/lj_snapshot.c) does its own requests and the recording ---- */

int lj_app_snapshot_request(void)
{
    lj_cmd cmd;
    if (!lj_ctx_get()->engine) return -1;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_SNAPSHOT_REQ; cmd.a = LJ_SNAPSHOT_OUT; cmd.snapshot = &lj_s.snap;
    lj_lock(); lj_s.snap.tick_no = 0; lj_unlock();
    lj_ctx_post(&cmd);
    return 0;
}

/* 1 = the frame of the last request is in lj_s.snap (copied under the lock that the engine fills it under). */
static int lj_app_snap_ready(void)
{
    uint64_t t;
    lj_lock(); t = lj_s.snap.tick_no; lj_unlock();
    return t != 0 && lj_ctx_get()->view.snapshot_tick == t;
}

int lj_app_snapshot_take(const uint8_t *filters, int add_scene)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = c->show ? c->show->current_seq : NULL;
    int idx;
    if (!s || !lj_app_snap_ready()) return -1;
    lj_lock();
    idx = add_scene ? lj_sequence_add(s, c->current_scene) : c->current_scene;   /* "Auto add scene": a copy of the current one, then overwritten */
    if (idx >= 0) { lj_snapshot_fold(&s->scenes[idx], &lj_s.snap, c->show, filters ? *filters : 0u); s->modified = 1; }   /* the Snapshot window's fold (LJ_SNAPF_* == LJ_SNAP_*) */
    lj_unlock();
    if (idx < 0) { lj_log("snapshot: the sequence is full (%d scenes)", (int)s->n); return -1; }
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    lj_ctx_set_scene((uint16_t)idx);                                  /* posts CURRENT_SEQ: the engine re-reads the scene */
    return 0;
}

/* ---- Striking Lamps [01 s6.2]: the window (windows/lj_striking_lamps.c) walks the fixtures; this is its strike hook ---- */

static void lj_app_strike_set(int f, int on)
{
    const lj_show *show = lj_ctx_get()->show;
    const lj_profile *p = lj_fixlib_by_id(show->fixture[f].profile_id);   /* ponytail: built-in profiles; a .GS2 fixture's profile is the engine's (W3 when Fixture Configuration exposes it) */
    int base = lj_show_fixture_base(show, f), o;
    lj_cmd cmd;
    if (!p || !p->lamp_on.enabled || base < 0 || show->fixture[f].kind != LJ_FX_BUILTIN) return;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_OVERWRITE;                                      /* S8: the lamp-on value outranks the scenes while it is held */
    for (o = 0; o < p->footprint && o < LJ_PROFILE_CH; o++) {
        if (!p->lamp_on.defined[o] || base + o >= LJ_CHANS) continue;
        cmd.a = (uint32_t)(base + o); cmd.b = on ? p->lamp_on.value[o] : 256u;
        lj_ctx_post(&cmd);
    }
    lj_s.strike.clear_us[f] = on ? lj_clock_us() + (uint64_t)(p->lamp_on_ms ? p->lamp_on_ms : LJ_STRIKE_HOLD_MS) * 1000u : 0;
}

static void lj_app_strike_hook(int fixture, void *ud) { (void)ud; if (fixture >= 0 && fixture < LJ_FIXTURES) lj_app_strike_set(fixture, 1); }

int lj_app_strike_lamps(int from_startup)
{
    if (!lj_ctx_get()->show) return -1;
    lj_striking_lamps_set_strike(lj_app_strike_hook, NULL);          /* the window steps the list (1 s apart), the app owns the output path */
    return lj_striking_lamps_start(!from_startup, LJ_STRIKE_INTERVAL_MS) > 0 ? 0 : -1;   /* the startup run closes itself after the last lamp */
}

static void lj_app_strike_idle(uint64_t now)
{
    int f;
    if (!lj_ctx_get()->show) return;
    for (f = 0; f < LJ_FIXTURES; f++) if (lj_s.strike.clear_us[f] && now >= lj_s.strike.clear_us[f]) lj_app_strike_set(f, 0);   /* the hold is over: the overwrite goes */
    if (lj_s.strike.at_us && now >= lj_s.strike.at_us) { lj_s.strike.at_us = 0; lj_app_strike_lamps(1); }   /* [STARTUP] AutoLampOn after n minutes */
}

/* ---- System Preferences (seam 3) ---- */

static void lj_app_post_pref(enum lj_engine_pref k, int v)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_PREFS; cmd.a = (uint32_t)k; cmd.b = (uint32_t)(v != 0);
    lj_ctx_post(&cmd);
}

int lj_app_prefs_load(void)
{
    lj_ctx *c = lj_ctx_get();
    c->prefs = &lj_s.prefs;
    lj_sysprefs_load(c->main_ini, &lj_s.prefs);
    /* ME-29 / LORD_FIDELITY-11: LJ2 defaults the Freeze / OnLine-OffLine gates ON [01 s6.1]; the shipped Main.Ini's AllowBlind=0
     * AllowOffline=0 is a PROPOSAL mapping (docs/w2-contracts.md s6), so a library that never saved System Preferences keeps them ON. */
    if (lj_ini_geti(c->main_ini, "PREFS", "AllowBlind", -1) < 0) lj_s.prefs.allow_freeze = 1;
    if (lj_ini_geti(c->main_ini, "PREFS", "AllowOffline", -1) < 0) lj_s.prefs.allow_offline = 1;
    c->static_visible = lj_statics_ini_visible(c->main_ini);       /* ME-30: [STATICS2] SlotsVisible 1..64 */
    lj_app_prefs_apply();
    return c->main_ini ? 0 : -1;
}

int lj_app_prefs_save(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!c->main_ini || !c->prefs) return -1;
    lj_sysprefs_save(c->main_ini, c->prefs);
    lj_ini_seti(c->main_ini, "STATICS2", "SlotsVisible", c->static_visible);
    if (!lj_lib() || lj_library_path(lj_lib(), "Main.Ini", path, sizeof path) != 0 || lj_ini_save(c->main_ini, path) != 0) { lj_log("app: Main.Ini not saved"); return -1; }
    return 0;
}

void lj_app_prefs_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    const lj_sysprefs *p = c->prefs;
    if (!p) return;
    c->allow_freeze = p->allow_freeze; c->allow_offline = p->allow_offline; c->allow_solo = p->allow_solo;
    if (c->hotkeys) { c->hotkeys->enabled = p->hotkeys_enabled; c->hotkeys->disable_in_dialogs = p->hotkeys_disable_in_dialogs; }
    lj_desktop_toolbar_gates(p);
    lj_desktop_hotkey_setup_set(p->hotkey_setup, p->hotkeys_enabled);
    lj_app_post_pref(LJ_PREF_KEEP_SEQ_BLIND, p->keep_seq_blind);                                          /* LORD_ENGINE-13 */
    lj_app_post_pref(LJ_PREF_USE_GLOBAL_CHASE_RATE, lj_ini_geti(c->main_ini, "CUE", "UseGlobalChaseRate", LJ_CHASE_GLOBAL_RATE));   /* exe inikey 0x34E59C; the one store the cue form's menu writes (ME-93) */
    lj_app_post_pref(LJ_PREF_DISABLE_CHASE_FADE, lj_ini_geti(c->main_ini, "CUE", "DisableChaseFade", 0));
    lj_app_post_pref(LJ_PREF_STATICS_USE_FADETIME, lj_ini_geti(c->main_ini, "STATICS", "Use Fadetime", 0));
    lj_app_post_pref(LJ_PREF_HOTKEY_TRIG_CHANGES_DIR, lj_ini_geti(c->main_ini, "CUE", "HotkeyChangeSeqDir", 0));
}

/* ---- hotkeys (w1-integration s7: one place, <library>Hotkeys/SetupN.Hot) ---- */

int lj_app_hotkey_setup(int setup)
{
    lj_ctx *c = lj_ctx_get();
    char err[128];
    if (!c->hotkeys || !lj_lib() || setup < 1 || setup > LJ_HOTKEY_SETUPS_MAX) return -1;
    if (lj_hotkeys_setup_load(lj_lib()->root, setup, c->hotkeys, err, sizeof err) != 0) lj_log("hotkeys: Setup%d.Hot: %s", setup, err);   /* a missing file is silent (the old "rec: cannot open" line is gone) */
    if (c->prefs) { lj_s.prefs.hotkey_setup = setup; c->hotkeys->enabled = c->prefs->hotkeys_enabled; c->hotkeys->disable_in_dialogs = c->prefs->hotkeys_disable_in_dialogs; lj_desktop_hotkey_setup_set(setup, c->prefs->hotkeys_enabled); }
    return 0;
}

int lj_app_key_passes(const lj_window *win)
{
    const lj_ctx *c = lj_ctx_get();
    if (!win) return 1;
    if (win == lj_win_get(LJ_WIN_AUDIO_ANALYZER)) return c->audio && c->audio->pass_keys;   /* Analyzer > Pass Keypresses to LightJockey */
    if (win == lj_win_get(LJ_WIN_MEDIA_PLAYER)) return lj_media_form_keypass();             /* Options > Pass Keypresses */
    return 1;
}

void lj_app_key(const lj_key_ev *ev, lj_window *win)
{
    int id, in_dialog = 0;
    if (!lj_app_key_passes(win)) return;                                  /* ME-182 (ME-17: no stub widget) */
    for (id = 0; win && id < LJ_WIN_COUNT; id++) if (lj_win_get((enum lj_win)id) == win) { in_dialog = lj_win_is_dialog((enum lj_win)id); break; }
    lj_desktop_key(ev, in_dialog);
}

/* ---- lj_fn rows that need the disk: the programs and the quick saves ---- */

static void lj_app_fn(enum lj_fn fn, int edge, uint32_t prog_num, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    lj_cmd cmd;
    lj_app_fresh fresh;
    (void)ud;
    switch (fn) {
    case LJ_FN_PROG_CUE:   if (edge) lj_app_load_cue(prog_num, 0); break;
    case LJ_FN_PROG_BGCUE: if (edge) lj_app_load_bgcue(prog_num); break;
    case LJ_FN_PROG_SEQ_FLASH:                                        /* held: the pinned sequence flashes; released: NULL (latched: c = 1 keeps it, 03 s5.10) */
        memset(&cmd, 0, sizeof cmd); memset(&fresh, 0, sizeof fresh);
        cmd.kind = LJ_CMD_FLASH; cmd.b = (uint32_t)(edge != 0); cmd.c = c->flash_latch;
        if (edge) cmd.seq[0] = lj_app_seq_for(prog_num, &fresh);
        lj_lock(); lj_app_commit(&fresh, &cmd); lj_unlock();
        break;
    case LJ_FN_QUICK_SAVE_SEQUENCE: if (edge) lj_app_save_sequence(0); break;
    case LJ_FN_QUICK_SAVE_CUE:      if (edge) lj_app_save_cue(0); break;
    case LJ_FN_PROG_CUELIST:        if (edge) lj_app_cuelist_prog(prog_num); break;   /* W3: load + Top (lj_app.h) */
    /* integrator: the toolbar / hotkey rows no window file owns */
    case LJ_FN_CTL_INTENSITY: case LJ_FN_CTL_MOVEMENT: case LJ_FN_CTL_COLOR: case LJ_FN_CTL_GOBO: case LJ_FN_CTL_BEAM: case LJ_FN_CTL_EFFECT: case LJ_FN_CTL_LAMP: {
        enum lj_ctl_kind k = fn == LJ_FN_CTL_LAMP ? LJ_CTL_LAMP : (enum lj_ctl_kind)(LJ_CTL_INTENSITY + (fn - LJ_FN_CTL_INTENSITY));
        const lj_profile *p = lj_sel_master_profile();
        if (k == LJ_CTL_COLORS && p && !lj_profile_window(p, LJ_CTL_COLORS) && lj_profile_window(p, LJ_CTL_RGBPACK)) k = LJ_CTL_RGBPACK;   /* RGB packs: Colors is the RGB pack window [04 s5.3] (W4 acceptance) */
        if (edge) lj_ctl_toggle(k);
        break;
    }
    case LJ_FN_CTL_LEVEL_SPECIAL: {                                   /* button 7: Levels, or Extended for a profile without a Levels grid (MAC 500/918, 15/01 s3) */
        const lj_profile *p = lj_sel_master_profile();
        if (edge && p) lj_ctl_toggle(lj_profile_window(p, LJ_CTL_LEVELS) ? LJ_CTL_LEVELS : LJ_CTL_EXTENDED);
        break;
    }
    case LJ_FN_VIEW_DMX_OUTPUT:        if (edge) lj_win_toggle(LJ_WIN_DMX_MONITOR); break;
    case LJ_FN_TOGGLE_LIST_OF_CUES:    if (edge) lj_win_toggle(LJ_WIN_LIST_CUE); break;
    case LJ_FN_FREEZE_OUTPUT_TOGGLE:   if (edge && c->allow_freeze) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_FREEZE; cmd.b = !c->view.freeze; lj_ctx_post(&cmd); } break;   /* ME-29 gate */
    case LJ_FN_DMX_ONLINE_OFFLINE_TOGGLE: if (edge && c->allow_offline) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_OFFLINE; cmd.b = !c->view.offline; lj_ctx_post(&cmd); } break;
    default: break;
    }
}

/* ---- W2 desktop actions (lj_desktop.h enum lj_desktop_action, the W2 rows) ---- */

static void lj_app_default_out_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_DEFAULT_OUT; cmd.default_out = c->show->default_out;
    lj_ctx_post(&cmd);
    if (!lj_lib() || lj_library_path(lj_lib(), "DefaultOut.dat", path, sizeof path) != 0 || lj_defaultout_save(path, c->show->default_out) != 0) lj_log("app: DefaultOut.dat not saved");
}

static struct { int pending; char lib[LJ_LIBNAME_CAP]; } lj_restart;
void lj_app_restart_request(const char *lib)
{
    if (lj_restart.pending || !lib) return;
    lj_str_copy(lj_restart.lib, sizeof lj_restart.lib, lib);
    lj_restart.pending = 1;
}
int lj_app_restart_spawn(const char *self, int (*spawn)(const char *const *argv))
{
    char u[LJ_LIBNAME_CAP + 2];
    const char *argv[3];
    if (!lj_restart.pending || !self || !spawn) return 0;
    lj_restart.pending = 0;
    lj_sacn_close();                                                       /* the old instance's universes terminate before the new one opens them */
    snprintf(u, sizeof u, "U:%s", lj_restart.lib);                         /* lib < LJ_LIBNAME_CAP: never truncates */
    argv[0] = self; argv[1] = u; argv[2] = NULL;
    if (spawn(argv) != 0) { lj_log("app: restart into %s failed; start LJ4EVR again", lj_restart.lib); return -1; }
    lj_log("app: restarting into user library %s", lj_restart.lib);
    return 1;
}

static void lj_app_select_library(int index)
{
    lj_ctx *c = lj_ctx_get();
    char names[64][LJ_LIBNAME_CAP];
    int n = lj_library_list(c->userdir, names, 64);
    if (index < 0 || index >= n || lj_library_select(c->userdir, names[index]) != 0) { lj_log("app: user library %d not selected", index); return; }
    lj_app_restart_request(names[index]);                                   /* "Re-start LightJockey in new user library ? (There will be no prompt for unsaved data)" */
    lj_desktop_run_action(LJ_ACT_RESTART, 0);
}

static void lj_app_w2_action(enum lj_desktop_action a, int arg, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    switch (a) {
    case LJ_ACT_USER_LIBRARIES:  lj_win_open(LJ_WIN_USER_LIBRARIES); break;
    case LJ_ACT_DELETE_FILES:    if (arg >= LJ_REC_SEQ && arg <= LJ_REC_MACRO) lj_win_open((enum lj_win)(LJ_WIN_DELETE_SEQ + arg)); break;   /* lj_rec_kind order == the five Delete ids */
    case LJ_ACT_GLOBAL_PATCH:    lj_win_open(LJ_WIN_GLOBAL_PATCH); break;
    case LJ_ACT_SYSTEM_PREFS:    lj_win_open(LJ_WIN_SYSTEM_PREFS); break;
    case LJ_ACT_EDIT_HOTKEYS:    lj_win_open(LJ_WIN_EDIT_HOTKEYS); break;
    case LJ_ACT_ABOUT:           lj_win_open(LJ_WIN_ABOUT); break;
    case LJ_ACT_FIXTURE_GROUPS:  lj_win_open(LJ_WIN_FIXTURE_GROUPS); break;
    case LJ_ACT_MIDI_IN:         lj_win_open(LJ_WIN_MIDI_IN); break;
    case LJ_ACT_BGCUE_FORM:      lj_win_open(LJ_WIN_BGCUE_FORM); break;
    case LJ_ACT_STATICS2_FORM:   lj_win_open(LJ_WIN_STATICS2_FORM); break;
    case LJ_ACT_SMOKE_CONTROL:   lj_win_open(LJ_WIN_SMOKE_CONTROL); break;
    case LJ_ACT_NOTES:           lj_win_open(LJ_WIN_NOTES); break;
    case LJ_ACT_BACKUP:          lj_win_open(LJ_WIN_BACKUP); break;
    case LJ_ACT_RESTORE_LIBRARY: lj_win_open(LJ_WIN_RESTORE); break;
    case LJ_ACT_STRIKING_LAMPS:  lj_app_strike_lamps(arg == 0); break;
    case LJ_ACT_SELECT_LIBRARY:  lj_app_select_library(arg); break;
    case LJ_ACT_SYSTEM_PREFS_APPLY: lj_app_prefs_save(); lj_app_prefs_apply(); break;
    case LJ_ACT_HOTKEY_SETUP:    lj_app_hotkey_setup(arg); break;
    case LJ_ACT_HOTKEYS_ENABLED:
        if (c->prefs) lj_s.prefs.hotkeys_enabled = (uint8_t)(arg != 0);
        if (c->hotkeys) c->hotkeys->enabled = (uint8_t)(arg != 0);
        lj_desktop_hotkey_setup_set(lj_s.prefs.hotkey_setup, arg != 0);
        break;
    case LJ_ACT_SNAPSHOT:                                             /* the Snapshot window owns the filters and the recording (windows/lj_snapshot.h) */
        if (arg == 0) lj_snapshot_static();
        else if ((arg == 1) == (lj_snapshot_recording() < 0)) lj_snapshot_realtime();   /* 1 starts, 2 stops: Record Realtime toggles */
        break;
    case LJ_ACT_DEFAULT_OUT:                                          /* arg 1 = Use Current Scene, 0 = Clear Default Output */
        if (!c->show) break;
        lj_lock();
        if (!arg) lj_defaultout_clear(c->show->default_out);
        else if (c->show->current_seq && lj_defaultout_use_scene(c->show->default_out, &c->show->current_seq->scenes[c->current_scene]) != 0) arg = -1;
        lj_unlock();
        if (arg < 0) { lj_log("default output: the scene holds a preset or a movement macro, not used"); break; }
        lj_app_default_out_apply();
        break;
    case LJ_ACT_DEFAULT_OUT_APPLY: if (c->show) lj_app_default_out_apply(); break;
    case LJ_ACT_MIDI_SAVE:       lj_app_midi_save(); break;
    case LJ_ACT_MIDI_OPEN_DEVICE: if (lj_app_midi_open(arg) != 0) lj_log("MIDI-IN: device %d did not open", arg); break;
    case LJ_ACT_LOAD_BGCUE:      if (arg > 0) lj_app_load_bgcue((uint32_t)arg); else lj_app_bgcue_clear(); break;   /* 0 = New / Clear BGCue */
    case LJ_ACT_BGCUE_APPLY:     lj_app_bgcue_apply(lj_bgcue_form_work()); break;
    case LJ_ACT_PIN_SEQ:         if (arg > 0) lj_app_pin_seq((uint32_t)arg); break;
    case LJ_ACT_STATIC_CLEAR:    if (arg >= 0) lj_app_static_clear(arg / LJ_STATICS2_SLOTS_MAX + 1, arg % LJ_STATICS2_SLOTS_MAX + 1); break;
    default: break;
    }
}

/* ---- start / stop / idle ---- */

void lj_app_seam_start(void)
{
    static const enum lj_desktop_action acts[] = {
        LJ_ACT_USER_LIBRARIES, LJ_ACT_DELETE_FILES, LJ_ACT_GLOBAL_PATCH, LJ_ACT_SYSTEM_PREFS, LJ_ACT_EDIT_HOTKEYS, LJ_ACT_ABOUT, LJ_ACT_FIXTURE_GROUPS,
        LJ_ACT_MIDI_IN, LJ_ACT_BGCUE_FORM, LJ_ACT_STATICS2_FORM, LJ_ACT_SMOKE_CONTROL, LJ_ACT_NOTES, LJ_ACT_BACKUP, LJ_ACT_RESTORE_LIBRARY,
        LJ_ACT_STRIKING_LAMPS, LJ_ACT_SELECT_LIBRARY, LJ_ACT_SYSTEM_PREFS_APPLY, LJ_ACT_HOTKEY_SETUP, LJ_ACT_HOTKEYS_ENABLED, LJ_ACT_SNAPSHOT,
        LJ_ACT_DEFAULT_OUT, LJ_ACT_DEFAULT_OUT_APPLY, LJ_ACT_MIDI_SAVE, LJ_ACT_MIDI_OPEN_DEVICE, LJ_ACT_LOAD_BGCUE, LJ_ACT_STATIC_CLEAR, LJ_ACT_BGCUE_APPLY, LJ_ACT_PIN_SEQ };
    lj_ctx *c = lj_ctx_get();
    char ini[LJ_PATH_CAP], map[LJ_PATH_CAP], err[128];
    int i, n;

    lj_app_prefs_load();
    lj_app_hotkey_setup(lj_s.prefs.hotkey_setup);
    lj_midi_init(&lj_s.midi);
    if (lj_app_midi_paths(ini, map, sizeof ini) == 0 && lj_midi_load(ini, map, &lj_s.midi, err, sizeof err) != 0) { lj_log("MIDI-IN: %s, defaults used", err); lj_midi_init(&lj_s.midi); }
    c->midi = &lj_s.midi;
    if (lj_s.midi.device >= 0 && lj_app_midi_open(lj_s.midi.device) != 0) lj_log("MIDI-IN: device %d did not open", lj_s.midi.device);
    lj_fn_bind(LJ_FN_PROG_CUE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_PROG_BGCUE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_PROG_SEQ_FLASH, lj_app_fn, NULL);
    lj_fn_bind(LJ_FN_PROG_CUELIST, lj_app_fn, NULL);                  /* quick saves: the windows' handlers (lj_save_seq_quick, lj_cue_form_save) bound by lj_win_set_host */
    for (i = LJ_FN_CTL_INTENSITY; i <= LJ_FN_CTL_LAMP; i++) lj_fn_bind((enum lj_fn)i, lj_app_fn, NULL);
    lj_fn_bind(LJ_FN_VIEW_DMX_OUTPUT, lj_app_fn, NULL); lj_fn_bind(LJ_FN_TOGGLE_LIST_OF_CUES, lj_app_fn, NULL);
    lj_fn_bind(LJ_FN_FREEZE_OUTPUT_TOGGLE, lj_app_fn, NULL); lj_fn_bind(LJ_FN_DMX_ONLINE_OFFLINE_TOGGLE, lj_app_fn, NULL);
    for (i = 0; i < LJ_COUNT_OF(acts); i++) lj_desktop_bind_action(acts[i], lj_app_w2_action, NULL);
    lj_delete_files_set_deleter(lj_app_delete_record);                /* ME-90: Delete Files hands the engine NULL and drops the table record (rule 6) */
    if (c->show) {                                                    /* ME-94: the saved 11 P8 preference is in force from the start, not from the first HTP edit */
        lj_cmd hc;
        memset(&hc, 0, sizeof hc);
        hc.kind = LJ_CMD_HTP_GROUPS; hc.patch = c->show->patch; hc.b = (uint32_t)lj_global_patch_htp_masters();
        lj_ctx_post(&hc);
    }
    lj_cue_form_set_view(&c->view);                                   /* seam 3: the windows paint from the app's one copy */
    lj_master_intensity_set_view(&c->view);
    if ((n = lj_ini_geti(c->main_ini, "STARTUP", "AutoLampOn", 0)) > 0 && n <= 5) lj_s.strike.at_us = lj_clock_us() + (uint64_t)n * 60000000u;   /* [01 s6.2] after n minutes */
}

void lj_app_seam_stop(void)
{
    if (lj_s.midi_open) lj_midi_close();                              /* ME-23: the MIDI thread is gone before the engine thread stops */
    lj_s.midi_open = 0;
    if (lj_ctx_get()->midi && lj_s.midi.auto_save) lj_app_midi_save();
    lj_ctx_get()->midi = NULL;
    if (lj_ctx_get()->main_ini && lj_ctx_get()->prefs) {               /* Setup > Hotkeys choices outlive the run (Main.Ini is written after this by lj_app.c) */
        char file[16];
        snprintf(file, sizeof file, "Setup%d.Hot", lj_s.prefs.hotkey_setup);
        lj_ini_sets(lj_ctx_get()->main_ini, "HOTKEYS", "File", file);
        lj_ini_seti(lj_ctx_get()->main_ini, "HOTKEYS", "Enabled", lj_s.prefs.hotkeys_enabled);
    }
}

void lj_app_seam_idle(uint64_t now_us)
{
    lj_snapshot_tick(now_us);                                        /* real-time recording samples at up to 20/s from here, not the 100 ms refresh */
    lj_app_strike_idle(now_us);
}

/* W3: the lj_app.h W3 block (cue-list runner, reloads, follow spot) lives in lj_app_cuelist_host.c */
