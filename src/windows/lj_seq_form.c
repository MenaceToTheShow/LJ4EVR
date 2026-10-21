/* lj_seq_form.c - behaviour of TSequenceForm (lj_seq_form.h). The DFM declares
 * handlers only for the check boxes, the up/down pulses, the menu and the form
 * events; the twelve speed buttons had their OnClick assigned in code in LJ2, so
 * they are wired here after lj_window_create (the "runtime-built parts" of 7.6).
 * Seam 3: the progress bars read lj_engine_runner(LJ_RUN_CURRENT) under lj_lock
 * (never lj_engine_view, whose pending_* fields belong to the app). */
#include "windows/lj_seq_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_lists.h"
#include "windows/lj_save_dialogs.h"
#include "windows/lj_warnings.h"

static lj_window *lj_sf_win;
static lj_widget *lj_sf_bar_w;                 /* the code-built W_PROGRESS2 widget inside the Elapsed bevel */
static int lj_sf_shrink = LJ_SEQFORM_SHRINK_TO_PANEL1;
static int lj_sf_loading;                      /* fields are being written from the model: OnClick re-entry is ignored */
static int lj_sf_close_pending;
static uint8_t lj_sf_disable_fade;
static struct { uint32_t se, st, fe, ft; } lj_sf_bar;

/* ---- model access ---- */

static lj_sequence *lj_sf_seq(void) { lj_ctx *c = lj_ctx_get(); return c->show ? c->show->current_seq : NULL; }

static lj_scene *lj_sf_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    if (!s || s->n == 0 || !s->scenes) return NULL;
    if (c->current_scene >= s->n) c->current_scene = (uint16_t)(s->n - 1);
    return &s->scenes[c->current_scene];
}

void lj_seq_form_post_current(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = LJ_CMD_CURRENT_SEQ;
    cmd.a = c->current_scene; cmd.b = c->blind;
    cmd.seq[0] = lj_sf_seq();                  /* the engine header's "seq = the pinned current sequence": slot 0 of the array */
    lj_ctx_post(&cmd);
}

void lj_seq_form_mark_modified(void)
{
    lj_sequence *s = lj_sf_seq();
    if (s) s->modified = 1;
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
}

int lj_seq_time_parse(const char *text, uint32_t *ms)
{
    uint32_t sec = 0, hund = 0;
    int digits = 0, frac = 0, nfrac = 0;
    const char *p;
    if (!text || !ms) return -1;
    for (p = text; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            digits++;
            if (!frac) sec = sec * 10 + (uint32_t)(*p - '0');
            else if (nfrac < 2) { hund = hund * 10 + (uint32_t)(*p - '0'); nfrac++; }
        } else if (*p == ':' || *p == '.' || *p == ',') frac = 1;   /* the field accepts "01234567890,:." [13 exe strings] */
        else if (*p != ' ') return -1;
    }
    if (!digits) return -1;
    if (nfrac == 1) hund *= 10;                /* "2:5" reads as 2.50 */
    *ms = sec * 1000 + hund * 10;
    if (*ms > LJ_TIME_MAX_MS) *ms = LJ_TIME_MAX_MS;
    return 0;
}

/* ---- the window's fields from the model ---- */

static void lj_sf_set_edit(const char *name, const char *text)
{
    lj_widget *w = lj_sf_win ? lj_window_find(lj_sf_win, name) : NULL;
    if (w) lj_w_edit_set_text(w, text);
}

static void lj_sf_set_check(const char *name, int on)
{
    lj_widget *w = lj_sf_win ? lj_window_find(lj_sf_win, name) : NULL;
    if (w) lj_w_checkbox_set_checked(w, on);
}

static void lj_sf_enable(const char *name, int on)
{
    lj_widget *w = lj_sf_win ? lj_window_find(lj_sf_win, name) : NULL;
    if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = on != 0; lj_widget_invalidate(w); }
}

static void lj_sf_update(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    lj_scene *sc = lj_sf_scene();
    char buf[64];
    uint32_t number = s ? s->number : 0;
    const char *name = s ? s->name : "";
    int scenes = s ? s->n : 0, scene = sc ? c->current_scene + 1 : 1;
    lj_desktop_seqcue_fields(number, name, scene, scenes ? scenes : 1, c->show && c->show->current_cue ? c->show->current_cue->number : 0,
                             c->show && c->show->current_cue ? c->show->current_cue->name : "");
    if (!lj_sf_win) return;
    lj_sf_loading = 1;
    if (lj_win_caption_seq(buf, sizeof buf, number, name) == 0) lj_window_set_caption(lj_sf_win, buf);
    snprintf(buf, sizeof buf, "%04u", (unsigned)number);
    lj_sf_set_edit("ESeqNo", number ? buf : "");
    lj_sf_set_edit("ESeqName", name);
    lj_sf_set_edit("ESeqType", s ? s->type : "");
    snprintf(buf, sizeof buf, "%d:%d", scene, scenes ? scenes : 1);   /* "1:1" for an empty sequence [03 s4.1] */
    lj_w_set_caption(lj_window_find(lj_sf_win, "Panel2"), buf);
    if (lj_fmt_time(buf, sizeof buf, sc ? sc->scene_ms : LJ_SCENE_TIME_DEFAULT_MS) == 0) lj_sf_set_edit("ES", buf);
    if (lj_fmt_time(buf, sizeof buf, sc ? sc->fade_ms : LJ_FADE_TIME_DEFAULT_MS) == 0) lj_sf_set_edit("EF", buf);
    if (sc && sc->scene_ms) snprintf(buf, sizeof buf, "(%u.%02u bmp)", (unsigned)(6000000u / sc->scene_ms / 100), (unsigned)(6000000u / sc->scene_ms % 100));   /* Martin's typo kept [07 s5, D13] */
    else lj_str_copy(buf, sizeof buf, "XXXX.X BPM");
    lj_w_set_caption(lj_window_find(lj_sf_win, "lBPM"), buf);
    lj_sf_set_check("cbSync", s && s->sync_times);
    lj_sf_set_check("cbGlobalScene", s && s->global_scene_time);
    lj_sf_set_check("cbGlobalFade", s && s->global_fade_time);
    lj_sf_enable("sb1", scenes > 1); lj_sf_enable("sb2", scenes > 1);    /* prev/next only with more than one scene */
    lj_sf_enable("sb11", number != 0);                                   /* Add to current cue needs a saved sequence */
    lj_menu_set_checked(lj_window_menus(lj_sf_win), "DisableFade1", lj_sf_disable_fade);   /* ponytail: LJ2 re-captions to "Enable Fade"; lj_menu has no caption setter, a check mark marks the state */
    lj_menu_set_checked(lj_window_menus(lj_sf_win), "M_KeepBlind", c->main_ini ? lj_ini_geti(c->main_ini, "PREFS", "KeepSeqBlind", 1) : 1);
    lj_sf_loading = 0;
}

void lj_seq_form_update(void) { lj_sf_update(); }

/* ---- scene mutations [F29], each under lj_lock then re-pinned ---- */

static int lj_sf_after(int rc)
{
    if (rc == 0) { lj_seq_form_post_current(); lj_seq_form_mark_modified(); }
    lj_sf_update();
    return rc;
}

int lj_seq_form_add_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    int i = -1;
    if (!s) return -1;
    lj_lock();
    i = s->n ? lj_sequence_add(s, c->current_scene) : lj_sequence_alloc(s, 1) == 0 ? 0 : -1;
    if (i >= 0) c->current_scene = (uint16_t)i;
    lj_unlock();
    return lj_sf_after(i >= 0 ? 0 : -1);
}

int lj_seq_form_insert_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    int i = -1;
    if (!s || !s->n) return -1;
    lj_lock();
    i = lj_sequence_insert(s, c->current_scene, 1, 1);
    if (i >= 0) c->current_scene = (uint16_t)i;
    lj_unlock();
    return lj_sf_after(i >= 0 ? 0 : -1);
}

int lj_seq_form_delete_scene(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    int rc;
    if (!s || !s->n) return -1;
    lj_lock();
    rc = lj_sequence_delete(s, c->current_scene, c->current_scene);
    if (rc == 0 && c->current_scene >= s->n) c->current_scene = (uint16_t)(s->n - 1);
    lj_unlock();
    return lj_sf_after(rc);
}

int lj_seq_form_delete_all(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    int rc;
    if (!s || !s->n) return -1;
    lj_lock();
    rc = lj_sequence_delete(s, 0, (uint16_t)(s->n - 1));
    c->current_scene = 0;
    lj_unlock();
    return lj_sf_after(rc);
}

int lj_seq_form_goto(int index)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    if (!s || !s->n) return -1;
    while (index < 0) index += s->n;
    index %= s->n;
    lj_lock();
    c->current_scene = (uint16_t)index;
    lj_unlock();
    lj_seq_form_post_current();
    lj_sf_update();
    return 0;
}

int lj_seq_form_set_times(uint32_t scene_ms, uint32_t fade_ms)
{
    lj_sequence *s = lj_sf_seq();
    lj_scene *sc = lj_sf_scene();
    int changed = 0;
    uint16_t i;
    if (!s || !sc) return -1;
    if (scene_ms > LJ_TIME_MAX_MS) scene_ms = LJ_TIME_MAX_MS;
    if (fade_ms > LJ_TIME_MAX_MS) fade_ms = LJ_TIME_MAX_MS;
    if (s->sync_times) fade_ms = scene_ms;    /* Synchronize scene and fade time [03 s3.4] */
    lj_lock();
    if (sc->scene_ms != scene_ms || sc->fade_ms != fade_ms) changed = 1;
    sc->scene_ms = scene_ms; sc->fade_ms = fade_ms;
    for (i = 0; i < s->n; i++) {              /* Global Scene / Fade Time: every scene follows */
        if (s->global_scene_time && s->scenes[i].scene_ms != scene_ms) { s->scenes[i].scene_ms = scene_ms; changed = 1; }
        if (s->global_fade_time && s->scenes[i].fade_ms != fade_ms) { s->scenes[i].fade_ms = fade_ms; changed = 1; }
    }
    lj_unlock();
    if (changed) lj_seq_form_mark_modified();
    lj_sf_update();
    return 0;
}

static void lj_sf_commit_times(void)
{
    lj_scene *sc = lj_sf_scene();
    uint32_t sm, fm;
    const char *es = lj_sf_win ? lj_w_edit_text(lj_window_find(lj_sf_win, "ES")) : NULL;
    const char *ef = lj_sf_win ? lj_w_edit_text(lj_window_find(lj_sf_win, "EF")) : NULL;
    if (!sc || !es || !ef) return;
    if (lj_seq_time_parse(es, &sm) != 0) sm = sc->scene_ms;
    if (lj_seq_time_parse(ef, &fm) != 0) fm = sc->fade_ms;
    if (sm != sc->scene_ms || fm != sc->fade_ms) lj_seq_form_set_times(sm, fm);
}

/* ---- New / Load with the [PREFS] WarnSeq prompt ---- */

static int lj_sf_warn(const char *key, int dflt)
{
    lj_ctx *c = lj_ctx_get();
    return c->main_ini ? lj_ini_geti(c->main_ini, "PREFS", key, dflt) != 0 : dflt;
}

static void lj_sf_new_now(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *s = lj_sf_seq();
    (void)ud;
    if (!s || !yes) return;
    lj_lock();
    lj_sequence_new(s);
    if (s->n == 0) lj_sequence_alloc(s, 1);   /* one all-off scene, always [03 s4.3] */
    c->current_scene = 0;
    lj_unlock();
    lj_seq_form_post_current();
    lj_ctx_set_modified(LJ_MOD_SEQ, 0);
    c->seq_saved = 0;
    lj_sf_update();
    lj_save_seq_refresh();
}

void lj_seq_form_new(int force)
{
    lj_sequence *s = lj_sf_seq();
    if (!s) return;
    if (!force && s->modified && lj_sf_warn("WarnSeq", 1))
        lj_win_confirm("New Sequence", "Current Sequence was modified", "This will cancel any modifications since last save", lj_sf_new_now, NULL);   /* [13 exe strings 0x188C80..0x188CD4] */
    else lj_sf_new_now(1, NULL);
}

static uint32_t lj_sf_load_no;

static void lj_sf_load_now(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    lj_sequence *rec, *old;
    char path[LJ_PATH_CAP], err[128];
    (void)ud;
    if (!yes || !c->show || !c->lib) return;
    if (lj_library_record_path(c->lib, LJ_REC_SEQ, lj_sf_load_no, path, sizeof path) != 0) return;
    rec = lj_sequence_new_rec();
    err[0] = 0;
    if (lj_sequence_load(path, rec, err, sizeof err) != 0) {      /* outside the lock (design 3.1) */
        lj_log("seq_form: load %s: %s", path, err);
        lj_win_message("Load Sequence", err[0] ? err : ">>Load Error");
        lj_sequence_free(rec);
        return;
    }
    rec->modified = 0;
    lj_lock();
    old = c->show->current_seq;
    c->show->current_seq = rec;
    c->current_scene = 0;
    lj_unlock();
    lj_seq_form_post_current();               /* the engine reads the new pointer when it drains, before any tick touches old */
    lj_sequence_free(old);
    lj_ctx_set_modified(LJ_MOD_SEQ, 0);
    c->seq_saved = 1;
    lj_sf_update();
    lj_save_seq_refresh();
}

int lj_seq_form_load(uint32_t number, int force)
{
    lj_sequence *s = lj_sf_seq();
    if (!s || number == 0) return -1;
    lj_sf_load_no = number;
    if (!force && s->modified && lj_sf_warn("WarnSeq", 1)) {
        lj_win_confirm("Load Sequence", "Current Sequence was modified", "This will cancel any modifications since last save", lj_sf_load_now, NULL);
        return 0;
    }
    lj_sf_load_now(1, NULL);
    return lj_sf_seq() && lj_sf_seq()->number == number ? 0 : -1;
}

/* ---- blind, playback, disable fade ---- */

static void lj_sf_blind_set(int on)
{
    lj_ctx *c = lj_ctx_get();
    if (on && !lj_sf_warn("AllowBlindSeq", 1)) return;    /* System Preferences gate [03 s4.5] */
    if (c->blind == (uint8_t)(on != 0)) return;
    c->blind = (uint8_t)(on != 0);
    lj_seq_form_post_current();
    lj_desktop_toolbar_update();              /* the toolbar name field turns orange */
}

static void lj_sf_post(int kind, uint32_t a, uint32_t b, uint32_t cc)
{
    lj_cmd cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.kind = (uint8_t)kind; cmd.a = a; cmd.b = b; cmd.c = cc;
    lj_ctx_post(&cmd);
}

static void lj_sf_playback(int dir)
{
    lj_ctx *c = lj_ctx_get();
    const lj_runner *r;
    int running = 0, rdir = 0;
    if (!c->engine) return;
    lj_lock();
    r = lj_engine_runner(c->engine, LJ_RUN_CURRENT);
    if (r) { running = r->running; rdir = r->dir; }
    lj_unlock();
    if (running && rdir == dir) { lj_sf_post(LJ_CMD_STOP, LJ_RUN_CURRENT, 0, 0); return; }   /* the same button again stops */
    lj_sf_post(LJ_CMD_TRIG, LJ_RUN_CURRENT, (uint32_t)dir, 0);
    lj_sf_post(LJ_CMD_GO, LJ_RUN_CURRENT, 0, 0);
}

static void lj_sf_disable_fade_set(int on)
{
    lj_sf_disable_fade = (uint8_t)(on != 0);
    lj_sf_post(LJ_CMD_DISABLE_FADE, 0, lj_sf_disable_fade, 0);   /* snaps the current-sequence runner only [11 F15] */
    lj_sf_update();
}

/* ---- the W_PROGRESS2 bars ---- */

void lj_seq_form_progress(uint32_t scene_elapsed_ms, uint32_t scene_ms, uint32_t fade_elapsed_ms, uint32_t fade_ms)
{
    lj_sf_bar.se = scene_elapsed_ms; lj_sf_bar.st = scene_ms; lj_sf_bar.fe = fade_elapsed_ms; lj_sf_bar.ft = fade_ms;
    if (lj_sf_bar_w) lj_widget_invalidate(lj_sf_bar_w);
}

static int lj_sf_bar_len(int span, uint32_t e, uint32_t t)
{
    if (!t) return 0;
    if (e > t) e = t;
    return (int)((uint64_t)span * e / t);
}

static void lj_sf_bar_paint(lj_widget *w, lj_surface *s)
{
    int x, y, span = w->w - 4;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, LJ_CLR_BTNFACE);
    /* PROPOSAL rig: measured from seqtimeprogression.gif (75x27, reduced): a blue band over a red band, each 5 rows, 2-px margins */
    lj_fill(s, x + 2, y + 5, lj_sf_bar_len(span, lj_sf_bar.se, lj_sf_bar.st), 5, 0x0000FFu);
    lj_fill(s, x + 2, y + 13, lj_sf_bar_len(span, lj_sf_bar.fe, lj_sf_bar.ft), 5, LJ_CLR_RED);
}

static const lj_widget_vt lj_sf_bar_vt = { lj_sf_bar_paint, NULL, NULL, NULL };

static void lj_sf_bar_build(void)
{
    lj_widget *bevel = lj_window_find(lj_sf_win, "Elapsed"), *b;
    if (!bevel || !bevel->parent) return;
    b = lj_w_alloc(sizeof *b, bevel->parent, NULL, &lj_sf_bar_vt, 0);
    b->kind = LJ_K_PAINTBOX; b->name = "ElapsedBar";
    b->x = bevel->x + 1; b->y = bevel->y + 1; b->w = bevel->w - 2; b->h = bevel->h - 2;   /* inside the 1-px bevel frame */
    lj_sf_bar_w = b;
}

/* ---- refresh (100 ms) ---- */

void lj_seq_form_refresh(void)
{
    lj_ctx *c = lj_ctx_get();
    const lj_runner *r;
    lj_sequence *s = lj_sf_seq();
    uint32_t se = 0, st = 0, fe = 0, ft = 0;
    uint64_t now;
    int scene = -1;
    if (lj_sf_close_pending) { lj_seq_form_close(); return; }
    if (!lj_sf_win) return;
    if (c->engine && s && s->n) {
        now = lj_clock_us();
        lj_lock();
        r = lj_engine_runner(c->engine, LJ_RUN_CURRENT);
        if (r && r->running && r->scene < s->n) {
            scene = r->scene;
            st = s->scenes[r->scene].scene_ms;
            if (r->scene_due_us > now) {
                uint64_t left = r->scene_due_us - now;
                se = left >= (uint64_t)st * 1000 ? 0 : (uint32_t)(((uint64_t)st * 1000 - left) / 1000);
            } else se = st;
            ft = (uint32_t)(r->fade_dur_us / 1000);
            fe = now > r->fade_t0_us ? (uint32_t)((now - r->fade_t0_us) / 1000) : 0;
        }
        lj_unlock();
    }
    lj_seq_form_progress(se, st, fe, ft);
    if (scene >= 0 && scene != c->current_scene) { c->current_scene = (uint16_t)scene; lj_sf_update(); }   /* the counter follows playback [acceptance 7] */
}

/* ---- handlers ---- */

static void lj_sf_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void lj_sf_on_update(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sf_update(); }
static void lj_sf_on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sf_close_pending = 1; }

static void lj_sf_on_sync(lj_widget *w, void *ud)
{
    lj_sequence *s = lj_sf_seq();
    lj_scene *sc = lj_sf_scene();
    (void)ud;
    if (lj_sf_loading || !s) return;
    s->sync_times = (uint8_t)lj_w_checkbox_checked(w);
    lj_seq_form_mark_modified();
    if (s->sync_times && sc) lj_seq_form_set_times(sc->scene_ms, sc->scene_ms); else lj_sf_update();
}

static void lj_sf_on_global_scene(lj_widget *w, void *ud)
{
    lj_sequence *s = lj_sf_seq();
    lj_scene *sc = lj_sf_scene();
    (void)ud;
    if (lj_sf_loading || !s) return;
    s->global_scene_time = (uint8_t)lj_w_checkbox_checked(w);
    lj_seq_form_mark_modified();
    if (s->global_scene_time && sc) lj_seq_form_set_times(sc->scene_ms, sc->fade_ms); else lj_sf_update();
}

static void lj_sf_on_global_fade(lj_widget *w, void *ud)
{
    lj_sequence *s = lj_sf_seq();
    lj_scene *sc = lj_sf_scene();
    (void)ud;
    if (lj_sf_loading || !s) return;
    s->global_fade_time = (uint8_t)lj_w_checkbox_checked(w);
    lj_seq_form_mark_modified();
    if (s->global_fade_time && sc) lj_seq_form_set_times(sc->scene_ms, sc->fade_ms); else lj_sf_update();
}

static void lj_sf_on_enter(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sf_commit_times(); }   /* cbSyncEnter: focus left the time fields */

static void lj_sf_on_ud(lj_widget *w, void *ud)
{
    lj_scene *sc;
    int d = lj_w_udbutton_delta(w);
    uint32_t sm, fm, step = (uint32_t)LJ_SEQ_TIME_STEP_MS;
    lj_sf_commit_times();
    sc = lj_sf_scene();
    if (!sc) return;
    sm = sc->scene_ms; fm = sc->fade_ms;
    if (ud) { sm = d > 0 ? sm + step : sm > step ? sm - step : 0; }
    else    { fm = d > 0 ? fm + step : fm > step ? fm - step : 0; }
    lj_seq_form_set_times(sm, fm);
}

static void lj_sf_on_disable_fade(lj_widget *w, void *ud) { (void)w; (void)ud; lj_sf_disable_fade_set(!lj_sf_disable_fade); }

static void lj_sf_on_keep_blind(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)w; (void)ud;
    if (c->main_ini) lj_ini_seti(c->main_ini, "PREFS", "KeepSeqBlind", !lj_ini_geti(c->main_ini, "PREFS", "KeepSeqBlind", 1));
    lj_sf_update();
}

static void lj_sf_on_snapshot(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("seq_form: Snapshot is W2 [03 s4.7]"); }   /* stub: the snapshot / real-time recording dialog */

static const lj_binding lj_sf_bindings[] = {
    { "FormActivate", lj_sf_on_update, NULL }, { "FormClose", lj_sf_on_close, NULL }, { "FormCreate", lj_sf_noop, NULL },
    { "FormDestroy", lj_sf_noop, NULL }, { "FormHide", lj_sf_noop, NULL }, { "FormShow", lj_sf_on_update, NULL },
    { "cbSyncClick", lj_sf_on_sync, NULL }, { "cbSyncEnter", lj_sf_on_enter, NULL },
    { "cbGlobalFadeClick", lj_sf_on_global_fade, NULL }, { "cbGlobalSceneClick", lj_sf_on_global_scene, NULL },
    { "UdSceneChange", lj_sf_on_ud, (void *)1 }, { "UdFadeChange", lj_sf_on_ud, NULL },
    { "DisableFade1Click", lj_sf_on_disable_fade, NULL }, { "Preferences1Click", lj_sf_noop, NULL },
    { "M_KeepBlindClick", lj_sf_on_keep_blind, NULL }, { "M_SnapShotClick", lj_sf_on_snapshot, NULL }, { "Help1Click", lj_sf_noop, NULL },
    { NULL, NULL, NULL }
};

/* the speed buttons: no OnClick in the DFM, wired here [07 s5 row 1 / row 4] */
enum { LJ_SB_PREV = 1, LJ_SB_NEXT, LJ_SB_REV, LJ_SB_FWD, LJ_SB_INSERT, LJ_SB_DELETE, LJ_SB_ADD, LJ_SB_NEW, LJ_SB_SAVE, LJ_SB_LOAD, LJ_SB_ADDCUE, LJ_SB_BLIND };
static const struct { const char *name; int id; } lj_sf_sbs[] = {
    { "sb1", LJ_SB_PREV }, { "sb2", LJ_SB_NEXT }, { "sb3", LJ_SB_REV }, { "sb4", LJ_SB_FWD }, { "sb5", LJ_SB_INSERT }, { "sb6", LJ_SB_DELETE },
    { "sb7", LJ_SB_ADD }, { "sb8", LJ_SB_NEW }, { "sb9", LJ_SB_SAVE }, { "sb10", LJ_SB_LOAD }, { "sb11", LJ_SB_ADDCUE }, { "seqBlind", LJ_SB_BLIND },
};

static void lj_sf_add_to_cue(void)
{
    lj_sequence *s = lj_sf_seq();
    if (!s || !s->number) return;
    if (lj_cue_form_add_sequence(s->number) < 0) lj_win_message("Add Sequence to Cue", "No free sequence slots available");   /* [13 exe strings 0x191BE8] */
}

static void lj_sf_sb_event(lj_widget *w, int ev, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)w;
    if (ev != LJ_WEV_CLICK) return;
    switch ((int)(intptr_t)ud) {
    case LJ_SB_PREV:   lj_sf_commit_times(); lj_seq_form_goto(c->current_scene - 1); break;
    case LJ_SB_NEXT:   lj_sf_commit_times(); lj_seq_form_goto(c->current_scene + 1); break;
    case LJ_SB_REV:    lj_sf_playback(LJ_DIR_REV); break;
    case LJ_SB_FWD:    lj_sf_playback(LJ_DIR_FWD); break;
    case LJ_SB_INSERT: lj_sf_commit_times(); lj_seq_form_insert_scene(); break;    /* ponytail: the Insert dialog's default; TINSERTSCENESFORM is deferred */
    case LJ_SB_DELETE: lj_sf_commit_times(); lj_seq_form_delete_scene(); break;    /* ponytail: the Delete dialog's default; TDELETESCENESFORM is deferred */
    case LJ_SB_ADD:    lj_sf_commit_times(); lj_seq_form_add_scene(); break;
    case LJ_SB_NEW:    lj_seq_form_new(0); break;
    case LJ_SB_SAVE:   lj_sf_commit_times(); lj_win_open(LJ_WIN_SAVE_SEQ); break;
    case LJ_SB_LOAD:   lj_win_toggle(LJ_WIN_LIST_SEQ); break;
    case LJ_SB_ADDCUE: lj_sf_add_to_cue(); break;
    case LJ_SB_BLIND:  lj_sf_blind_set(!c->blind); break;
    default: break;
    }
}

/* ---- lj_fn rows of the Sequence group ---- */

static void lj_sf_fn(enum lj_fn fn, int edge, uint32_t prog_num, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)prog_num; (void)ud;
    switch (fn) {
    case LJ_FN_SEQUENCE_CONTROL:         if (edge) lj_win_toggle(LJ_WIN_SEQ_FORM); break;
    case LJ_FN_TOGGLE_LIST_OF_SEQUENCES: if (edge) lj_win_toggle(LJ_WIN_LIST_SEQ); break;
    case LJ_FN_NEXT_SCENE:               if (edge) lj_seq_form_goto(c->current_scene + 1); break;
    case LJ_FN_PREVIOUS_SCENE:           if (edge) lj_seq_form_goto(c->current_scene - 1); break;
    case LJ_FN_ADD_SCENE:                if (edge) lj_seq_form_add_scene(); break;
    case LJ_FN_FIRST_SCENE:              if (edge) lj_seq_form_goto(0); break;
    case LJ_FN_LAST_SCENE:               if (edge) lj_seq_form_goto(-1); break;
    case LJ_FN_QUICK_SAVE_SEQUENCE:      if (edge) lj_save_seq_quick(); break;
    case LJ_FN_NEW_SEQUENCE:             if (edge) lj_seq_form_new(0); break;
    case LJ_FN_SET_SEQUENCE_BLIND:       if (edge) lj_sf_blind_set(1); break;
    case LJ_FN_SEQUENCE_BLIND_FLASH:     lj_sf_blind_set(edge); break;
    case LJ_FN_CLEAR_SEQUENCE_BLIND:     if (edge) lj_sf_blind_set(0); break;
    case LJ_FN_SEQUENCE_BLIND_TOGGLE:    if (edge) lj_sf_blind_set(!c->blind); break;
    case LJ_FN_PLAYBACK_FWD:             if (edge) lj_sf_playback(LJ_DIR_FWD); break;
    case LJ_FN_PLAYBACK_REV:             if (edge) lj_sf_playback(LJ_DIR_REV); break;
    case LJ_FN_PLAYBACK_STOP:            if (edge) lj_sf_post(LJ_CMD_STOP, LJ_RUN_CURRENT, 0, 0); break;
    default: break;
    }
}

void lj_seq_form_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_SEQUENCE_CONTROL, LJ_FN_TOGGLE_LIST_OF_SEQUENCES, LJ_FN_NEXT_SCENE, LJ_FN_PREVIOUS_SCENE, LJ_FN_ADD_SCENE,
        LJ_FN_FIRST_SCENE, LJ_FN_LAST_SCENE, LJ_FN_QUICK_SAVE_SEQUENCE, LJ_FN_NEW_SEQUENCE, LJ_FN_SET_SEQUENCE_BLIND, LJ_FN_SEQUENCE_BLIND_FLASH,
        LJ_FN_CLEAR_SEQUENCE_BLIND, LJ_FN_SEQUENCE_BLIND_TOGGLE, LJ_FN_PLAYBACK_FWD, LJ_FN_PLAYBACK_REV, LJ_FN_PLAYBACK_STOP };
    int i;
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], lj_sf_fn, NULL);
}

/* ---- open / close ---- */

void lj_seq_form_shrink(int on) { lj_sf_shrink = on != 0; }

lj_window *lj_seq_form_open(void)
{
    const lj_win_host *h = lj_win_host_get();
    int i;
    if (lj_sf_win) { if (h && h->show) h->show(h->ud, lj_sf_win, 1); return lj_sf_win; }
    lj_sf_win = lj_window_create(&lj_form_TSEQUENCEFORM, lj_sf_bindings);
    if (!lj_sf_win) return NULL;
    for (i = 0; i < LJ_COUNT_OF(lj_sf_sbs); i++) {
        lj_widget *w = lj_window_find(lj_sf_win, lj_sf_sbs[i].name);
        if (w) { w->on_event = lj_sf_sb_event; w->ud = (void *)(intptr_t)lj_sf_sbs[i].id; }
    }
    lj_sf_bar_build();
    if (lj_sf_shrink) lj_window_resize(lj_sf_win, 354, 106);   /* LJ_SEQFORM_SHRINK_TO_PANEL1 */
    lj_sf_close_pending = 0;
    lj_sf_update();
    if (h && h->open && h->open(h->ud, lj_sf_win, 0) != 0) { lj_window_destroy(lj_sf_win); lj_sf_win = NULL; lj_sf_bar_w = NULL; return NULL; }
    return lj_sf_win;
}

void lj_seq_form_close(void)
{
    const lj_win_host *h = lj_win_host_get();
    lj_window *w = lj_sf_win;
    lj_sf_win = NULL; lj_sf_bar_w = NULL; lj_sf_close_pending = 0;
    if (!w) return;
    if (h && h->close) h->close(h->ud, w);
    lj_window_destroy(w);
}

lj_window *lj_seq_form_get(void) { return lj_sf_win; }
