/* lj_snapshot.c - see lj_snapshot.h. TSNAPSHOTSCENEFORM bound (ME-16). */
#include "windows/lj_snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_locale.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "engine/lj_engine_rules.h"
#include "model/lj_scene.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_warnings.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

static const char *const lj_snap_rates[LJ_SNAPSHOT_RATE_MAX_HZ] = {
    "1 Scenes/sec", "2 Scenes/sec", "3 Scenes/sec", "4 Scenes/sec", "5 Scenes/sec", "6 Scenes/sec", "7 Scenes/sec", "8 Scenes/sec", "9 Scenes/sec", "10 Scenes/sec",
    "11 Scenes/sec", "12 Scenes/sec", "13 Scenes/sec", "14 Scenes/sec", "15 Scenes/sec", "16 Scenes/sec", "17 Scenes/sec", "18 Scenes/sec", "19 Scenes/sec", "20 Scenes/sec" };   /* PROPOSAL rig: cbSampleParam item text (sequencesnapshot.gif is reduced) */

static struct {
    lj_window *win;
    lj_frame frame;                 /* the engine fills it (SNAPSHOT_REQ); read after ctx->view.snapshot_tick echoes its tick_no */
    uint64_t asked_at;              /* ctx->view.snapshot_tick when the request went out */
    int pending;                    /* 1 static, 2 real-time sample */
    lj_scene *rec; int rec_n, rec_k, rate;   /* real-time buffer: rec_k scenes of rec_n, rate Hz; rec NULL = idle */
    uint64_t next_us;
    char total[64];
} sn;

lj_window *lj_snapshot_window(void) { return sn.win; }
int lj_snapshot_recording(void) { return sn.rec ? sn.rec_k : -1; }
static lj_widget *find(const char *n) { return lj_window_find(sn.win, n); }
static int checked(const char *n) { return find(n) ? lj_w_checkbox_checked(find(n)) : 0; }

unsigned lj_snapshot_filters(void)
{
    lj_ctx *ctx = lj_ctx_get();
    if (!sn.win) return ctx->main_ini ? (unsigned)lj_ini_geti(ctx->main_ini, "MISC", "SNAPSHOTSETTINGS", 0) & 15u : 0u;
    return (checked("cbSelectedOnly") ? LJ_SNAPF_SELECTED_ONLY : 0u) | (checked("cbNonDefaultOnly") ? LJ_SNAPF_NON_DEFAULT_ONLY : 0u)
         | (checked("cbDMXinOnly") ? LJ_SNAPF_DMXIN_ONLY : 0u) | (checked("cbAsFade") ? LJ_SNAPF_AS_FADE : 0u);
}

int lj_snapshot_fold(lj_scene *dst, const lj_frame *f, const lj_show *show, unsigned filters)
{
    int c, n = 0;
    if (!dst || !f || !show) return 0;
    for (c = 0; c < LJ_CHANS; c++) {
        int fx = lj_show_fixture_at(show, c);
        uint8_t v = f->out[c / LJ_LINK_CH][c % LJ_LINK_CH];
        if (fx < 0) continue;                                                                  /* PROPOSAL rig: only channels of configured fixtures are recorded */
        if ((filters & LJ_SNAPF_SELECTED_ONLY) && !lj_sel_is_selected(fx)) continue;
        if ((filters & LJ_SNAPF_NON_DEFAULT_ONLY) && v == show->default_out[c]) continue;
        if ((filters & LJ_SNAPF_DMXIN_ONLY) && show->patch[c].src_kind != LJ_SRC_DMX_IN) continue;
        dst->val[c] = v; dst->state[c] = (uint8_t)((filters & LJ_SNAPF_AS_FADE) ? LJ_FADE : LJ_SNAP);
        n++;
    }
    return n;
}

static int request(int kind)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_cmd c;
    if (!ctx->engine || sn.pending) return -1;
    memset(&sn.frame, 0, sizeof sn.frame);
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_SNAPSHOT_REQ; c.a = LJ_SNAPSHOT_OUT; c.snapshot = &sn.frame;
    sn.asked_at = ctx->view.snapshot_tick;
    sn.pending = kind;
    lj_ctx_post(&c);
    return 0;
}

int lj_snapshot_static(void) { return request(1); }

static void total_label(void)
{
    char d = lj_locale_decimal();
    int ms = sn.rate ? (sn.rec ? sn.rec_n : 0) * 1000 / sn.rate : 0, pct;
    if (!sn.rec) {
        int n = find("eScenes") ? atoi(lj_w_edit_text(find("eScenes"))) : 0, r = find("cbSampleParam") ? ((lj_w_combobox *)find("cbSampleParam"))->index + 1 : 1;
        ms = r > 0 ? n * 1000 / r : 0;
        snprintf(sn.total, sizeof sn.total, "Total recording time: %d%c%02d sec", ms / 1000, d, ms % 1000 / 10);   /* exe 0x1F877C, D6 separator */
    } else {
        pct = sn.rec_n ? sn.rec_k * 10000 / sn.rec_n : 0;
        snprintf(sn.total, sizeof sn.total, "Total recording time: %d%c%02d sec - (%d%c%02d%%)", ms / 1000, d, ms % 1000 / 10, pct / 100, d, pct % 100);   /* exe 0x1F8AD8 */
    }
    lj_w_set_caption(find("ltotalTime"), sn.total);
}

static void stop_recording(void)
{
    free(sn.rec); sn.rec = NULL; sn.rec_k = sn.rec_n = 0;
    lj_w_set_caption(find("pRecordRealTime"), "Record Realtime");                      /* exe 0x1F8B0C */
    if (find("pgRealtime")) lj_w_progressbar_set(find("pgRealtime"), 0);
    total_label();
}

static void convert(int yes, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_sequence *seq = ctx->show ? ctx->show->current_seq : NULL;
    int i, ms = sn.rate ? 1000 / sn.rate : 1000;
    (void)ud;
    if (yes && seq && sn.rec) {
        lj_lock();
        if (lj_sequence_alloc(seq, (uint16_t)sn.rec_n) == 0)
            for (i = 0; i < sn.rec_n; i++) { seq->scenes[i] = sn.rec[i]; seq->scenes[i].scene_ms = (uint32_t)ms; seq->scenes[i].fade_ms = (uint32_t)ms; }   /* PROPOSAL rig: one sample period per scene, fade over it */
        lj_unlock();
        lj_ctx_set_scene(0);
        lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    }
    stop_recording();
}

int lj_snapshot_realtime(void)
{
    lj_ctx *ctx = lj_ctx_get();
    int n, r, i;
    if (!sn.win) return -1;
    if (sn.rec) { stop_recording(); return 0; }                                         /* "Recording (Cancel)" */
    n = atoi(lj_w_edit_text(find("eScenes")) ? lj_w_edit_text(find("eScenes")) : "0");
    r = find("cbSampleParam") ? ((lj_w_combobox *)find("cbSampleParam"))->index + 1 : 0;
    if (n < 2) { lj_win_message("Snapshot from DMX Out", "Not enough scenes specified for realtime snapshot recording (need at least 2 scenes)"); return -1; }   /* exe 0x1F8CD0 */
    if (n > LJ_SNAPSHOT_SCENES_MAX || r < LJ_SNAPSHOT_RATE_MIN_HZ || r > LJ_SNAPSHOT_RATE_MAX_HZ) return -1;
    sn.rec = (lj_scene *)malloc((size_t)n * sizeof *sn.rec);
    if (!sn.rec) lj_fatal("snapshot: out of memory");
    for (i = 0; i < n; i++) lj_scene_init(&sn.rec[i]);
    sn.rec_n = n; sn.rec_k = 0; sn.rate = r; sn.next_us = 0;
    lj_w_set_caption(find("pRecordRealTime"), "Recording (Cancel)");                    /* exe 0x1F8ABC */
    if (checked("cbAutostartPlayback") && ctx->engine) { lj_cmd c; memset(&c, 0, sizeof c); c.kind = LJ_CMD_GO; c.a = LJ_RUN_CURRENT; lj_ctx_post(&c); }   /* Sync start with Sequence playback */
    total_label();
    return 0;
}

void lj_snapshot_tick(uint64_t now_us)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_sequence *seq;
    if (sn.pending && ctx->view.snapshot_tick > sn.asked_at && sn.frame.tick_no == ctx->view.snapshot_tick) {   /* served */
        unsigned filt = lj_snapshot_filters();
        int kind = sn.pending;
        sn.pending = 0;
        if (kind == 2 && sn.rec && sn.rec_k < sn.rec_n) {
            lj_snapshot_fold(&sn.rec[sn.rec_k++], &sn.frame, ctx->show, filt);
            if (find("pgRealtime")) lj_w_progressbar_set(find("pgRealtime"), sn.rec_k * 100 / sn.rec_n);
            total_label();
            if (sn.rec_k == sn.rec_n) lj_win_confirm("Please Confirm..", "Recording finished", "Convert to sequence ?", convert, NULL);   /* exe 0x226264 */
        } else if (kind == 1 && ctx->show && (seq = ctx->show->current_seq) != NULL && ctx->current_scene < seq->n) {
            int at = ctx->current_scene;
            lj_lock();
            if (checked("cbAddScene")) { at = lj_sequence_add(seq, ctx->current_scene); if (at >= 0) { lj_scene_init(&seq->scenes[at]); seq->scenes[at].scene_ms = seq->scenes[ctx->current_scene].scene_ms; seq->scenes[at].fade_ms = seq->scenes[ctx->current_scene].fade_ms; } }   /* PROPOSAL rig: the added scene holds the snapshot only, with the current scene's times */
            if (at >= 0) lj_snapshot_fold(&seq->scenes[at], &sn.frame, ctx->show, filt);
            lj_unlock();
            if (at >= 0) { lj_ctx_set_scene((uint16_t)at); lj_ctx_set_modified(LJ_MOD_SEQ, 1); }
        }
    }
    if (sn.rec && !sn.pending && sn.rec_k < sn.rec_n && now_us >= sn.next_us) {
        uint64_t period = 1000000u / (uint64_t)sn.rate;
        sn.next_us = sn.next_us ? sn.next_us + period : now_us + period;
        request(2);
    }
}

void lj_snapshot_refresh(void) { if (sn.win) lj_snapshot_tick(lj_clock_us()); }

/* ---- handlers ---- */
static void save_settings(void)
{
    lj_ctx *ctx = lj_ctx_get();
    if (!ctx->main_ini || !sn.win) return;
    lj_ini_seti(ctx->main_ini, "MISC", "SNAPSHOTSETTINGS", (int)(lj_snapshot_filters() | (checked("cbAddScene") ? 16u : 0u) | (checked("cbAutostartPlayback") ? 32u : 0u)));   /* PROPOSAL rig: LJ2's bit layout is not decoded */
    lj_ini_seti(ctx->main_ini, "MISC", "RTSNAPSHOTPARAM", find("cbSampleParam") ? ((lj_w_combobox *)find("cbSampleParam"))->index : 0);
    lj_ini_seti(ctx->main_ini, "MISC", "RTSNAPSHOTSCENES", atoi(lj_w_edit_text(find("eScenes")) ? lj_w_edit_text(find("eScenes")) : "10"));
}
static void on_options(lj_widget *w, void *ud)
{
    (void)ud;
    if (w == find("cbAsSnap") && lj_w_checkbox_checked(w) && checked("cbAsFade")) lj_w_checkbox_set_checked(find("cbAsFade"), 0);   /* Snap / Fade exclude each other */
    else if (w == find("cbAsFade") && lj_w_checkbox_checked(w) && checked("cbAsSnap")) lj_w_checkbox_set_checked(find("cbAsSnap"), 0);
    save_settings();
}
static void on_static(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    (void)ud;
    if (w == find("bClearScene")) {   /* "Clear current scene" */
        if (ctx->show && ctx->show->current_seq && ctx->current_scene < ctx->show->current_seq->n) {
            lj_scene *s = &ctx->show->current_seq->scenes[ctx->current_scene];
            lj_lock(); memset(s->state, LJ_OFF, sizeof s->state); memset(s->val, 0, sizeof s->val); lj_unlock();
            lj_ctx_set_modified(LJ_MOD_SEQ, 1);
        }
    } else lj_snapshot_static();
}
static void on_realtime(lj_widget *w, void *ud) { (void)ud; if (w == find("pRecordRealTime")) lj_snapshot_realtime(); }
static void on_params(lj_widget *w, void *ud) { (void)w; (void)ud; if (!sn.rec) total_label(); save_settings(); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_snapshot_close(); }
static void on_help(lj_widget *w, void *ud) { (void)ud; lj_help_viewer_click(w, (void *)"sequences"); }   /* W4 acceptance: the bundled viewer */

static const lj_binding lj_sn_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "cbOptionsClick", on_options, NULL }, { "bStaticMouseDown", on_static, NULL }, { "RealtimeParamsChange", on_params, NULL },
    { "Panel3MouseDown", on_realtime, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };

lj_window *lj_snapshot_open(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_widget *cb;
    unsigned bits;
    char t[8];
    if (sn.win) return sn.win;
    sn.win = lj_window_create(&lj_form_TSNAPSHOTSCENEFORM, lj_sn_bindings);
    if (!sn.win) { lj_log("snapshot: cannot create TSnapshotSceneForm"); return NULL; }
    bits = ctx->main_ini ? (unsigned)lj_ini_geti(ctx->main_ini, "MISC", "SNAPSHOTSETTINGS", 0) : 0u;
    if ((cb = find("cbSampleParam")) != NULL) {
        int i = lj_ini_geti(ctx->main_ini, "MISC", "RTSNAPSHOTPARAM", 9);
        lj_w_combobox_set_items(cb, lj_snap_rates, LJ_SNAPSHOT_RATE_MAX_HZ);
        ((lj_w_combobox *)cb)->index = i >= 0 && i < LJ_SNAPSHOT_RATE_MAX_HZ ? i : 9;
    }
    snprintf(t, sizeof t, "%d", lj_ini_geti(ctx->main_ini, "MISC", "RTSNAPSHOTSCENES", 10));
    {   /* restore the boxes without firing cbOptionsClick (it would write the half-restored state back) */
        static const char *const names[6] = { "cbSelectedOnly", "cbNonDefaultOnly", "cbDMXinOnly", "cbAsFade", "cbAddScene", "cbAutostartPlayback" };
        static const unsigned bit[6] = { LJ_SNAPF_SELECTED_ONLY, LJ_SNAPF_NON_DEFAULT_ONLY, LJ_SNAPF_DMXIN_ONLY, LJ_SNAPF_AS_FADE, 16u, 32u };
        int i;
        for (i = 0; i < 6; i++) if ((cb = find(names[i])) != NULL) { void (*fn)(lj_widget *, int, void *) = cb->on_event; cb->on_event = NULL; lj_w_checkbox_set_checked(cb, (bits & bit[i]) != 0); cb->on_event = fn; }
        if ((cb = find("cbAsSnap")) != NULL) { void (*fn)(lj_widget *, int, void *) = cb->on_event; cb->on_event = NULL; lj_w_checkbox_set_checked(cb, !(bits & LJ_SNAPF_AS_FADE)); cb->on_event = fn; }
        if ((cb = find("eScenes")) != NULL) { void (*fn)(lj_widget *, int, void *) = cb->on_event; cb->on_event = NULL; lj_w_edit_set_text(cb, t); cb->on_event = fn; }
    }
    if ((cb = find("bClearScene")) != NULL) cb->enabled = 1;   /* PROPOSAL rig: the DFM ships it disabled; LJ2's enabling rule is not documented */
    stop_recording();
    lj_win_show(sn.win, 0);
    return sn.win;
}

void lj_snapshot_close(void)
{
    if (!sn.win) return;
    lj_win_hide(sn.win);
    lj_window_destroy(sn.win);
    free(sn.rec);
    memset(&sn, 0, sizeof sn);   /* a pending request's frame is dropped with it: the engine writes only sn.frame, which stays valid (static) */
}
