/* lj_lightcorder.c - the LightCorder recorder (W4-LC, F20; note 13 s1.2-1.3): TLightCorderDialog "Record to LightCorder
 * file" (Cuelist > Tools) and TSaveLightCorderFileDialog "Save Lightcorder File". Both features are undocumented (no
 * CHM GIF), so the layout is the DFM's and the flow is PROPOSAL:
 *   Start Recording allocates the card (4..64 MB, model/lj_llf.h) and the button becomes "Cancel Recording" (exe 0x1FFD74,
 *   with the Cancel glyph); lj_lightcorder_step copies lj_engine_frame()->out[link] under lj_lock at 10/20/40 Hz on the UI
 *   thread (ME-2: the engine gains nothing and does no I/O); "Cancel Recording" or a full card ("Lightcorder card memory
 *   full - recording stopped") ends the take and opens Save Lightcorder File with the two notes tabs; Save File writes
 *   <library>\LightCorderFiles\LCnnnn.LLF (no platform TSaveDialog yet, as lj_backup_restore.c); its Cancel asks
 *   "Cancel will loose any recorded data - do you wish to Cancel ?" (Martin's typo kept, D13). Closing the recorder while
 *   it records asks "Closing will cancel the current Lightcorder recording session".
 * The DFM's TLightCorderDialog carries no Visible = True: the form is shown when its menu item opens it, like every
 * LJ2 dialog (W4-LC "you settle visibility": shown on open, never at startup). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen / localtime under /W4 /WX on MSVC (as model/lj_library.c) */
#include "windows/lj_lightcorder.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "core/lj_fsinfo.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "model/lj_llf.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_warnings.h"

#define LJ_LC_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here): FormStyle = fsStayOnTop */
#define LJ_LC_BAR 0x000080u    /* PROPOSAL rig: the gauge colour of pbProgress / pbCompression is unmeasured (no GIF) */
#define LJ_LC_CAPTION_CONFIRM "Confirm"   /* Delphi MessageDlg mtConfirmation caption; PROPOSAL rig */
#define LJ_LC_CAPTION_WARNING "Warning"

static struct {
    lj_window *win, *save;
    lj_llf_rec rec;
    int recording, close_pending, save_close_pending;
    uint64_t t0_tick, k, sec_us; uint32_t sec_frames; int started;   /* ME-164: sample k is engine tick t0_tick + k * TickHz / hz */
    lj_memo_edit user, sys;                       /* the two notes tabs (RichEdits are read-only widgets; lj_memo_edit types into them) */
    char last[LJ_PATH_CAP];
} lc;

static lj_widget *find(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static int rg(const char *n) { int i = lj_w_radiogroup_index(find(lc.win, n)); return i < 0 ? 0 : i; }
static int sel_link(void) { return 1 + rg("rgLink"); }
static int sel_channels(void) { return 64 << rg("rgChannels"); }
static int sel_hz(void) { return 10 << rg("rgFrequency"); }
static int sel_card_mb(void) { return 4 << rg("rgTarget"); }

const char *lj_lightcorder_last_file(void) { return lc.last; }
int lj_lightcorder_recording(void) { return lc.recording; }
uint32_t lj_lightcorder_frames(void) { return lc.rec.frames; }

/* ---- lMaxTime: the exe formats 0x1FF0A8 / 0x1FF0DC (the DFM's 'Maximum recording time : MM:SS' is the design-time text) ---- */
int lj_lightcorder_max_time_text(char *dst, size_t cap, int card_mb, int channels, int hz)
{
    uint32_t s = lj_llf_max_seconds((uint32_t)card_mb << 20, channels, hz);
    char ss[8];
    int n;
    snprintf(ss, sizeof ss, "%02u", (unsigned)(s % 60));
    if (s >= 3600) n = snprintf(dst, cap, "Max recording time %d:%d:%s (Hour:Min:Sec)", (int)(s / 3600), (int)(s / 60 % 60), ss);
    else n = snprintf(dst, cap, "Max recording time %d:%s (Min:Sec)", (int)(s / 60), ss);
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

static void set_label(lj_widget *w, const char *t)   /* AutoSize labels: the width follows the text, the parent repaints the old extent */
{
    if (!w) return;
    lj_w_set_caption(w, t);
    w->w = lj_text_width(lj_w_font_of(w), t);
    lj_widget_invalidate(w->parent ? w->parent : w);
}

static void update_max_time(void)
{
    char t[96];
    if (lc.win && lj_lightcorder_max_time_text(t, sizeof t, sel_card_mb(), sel_channels(), sel_hz()) == 0) set_label(find(lc.win, "lMaxTime"), t);
}
static void on_option(lj_widget *w, void *ud) { (void)w; (void)ud; update_max_time(); }

static void set_recording_ui(int on)
{
    static const char *const groups[] = { "rgLink", "rgChannels", "rgFrequency", "rgTarget", "rgCompression" };
    lj_widget *b = find(lc.win, "bRecord"), *c;
    size_t i;
    if (!lc.win) return;
    lj_w_set_caption(b, on ? "Cancel Recording" : "Start Recording");   /* exe 0x1FFD74 / 0x1FEADC */
    for (i = 0; i < sizeof groups / sizeof groups[0]; i++) {
        lj_widget *g = find(lc.win, groups[i]);
        if (!g) continue;
        g->enabled = !on;
        for (c = g->first; c; c = c->next) c->enabled = !on;   /* the radio buttons paint greyed with their group */
        lj_widget_invalidate(g);
    }
}

/* ---- the save dialog ---- */
static void fixture_list(char *dst, size_t cap)
{
    lj_ctx *ctx = lj_ctx_get();
    size_t len = strlen(dst);
    int i;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &ctx->show->fixture[i];
        const lj_profile *p = f->link ? lj_fixlib_by_id(f->profile_id) : NULL;
        int n;
        if (f->link != lc.rec.link || !f->address || f->address > lc.rec.channels) continue;
        n = snprintf(dst + len, cap - len, "%2d. %s (%s) at DMX address %d\r\n", i + 1, f->user_name, p ? p->name : "", f->address);   /* exe 0x1FFAA8 */
        if (n < 0 || (size_t)n >= cap - len) { dst[len] = 0; return; }
        len += (size_t)n;
    }
}

int lj_lightcorder_sys_notes(char *dst, size_t cap)
{
    time_t now = time(NULL);
    const struct tm *tm = localtime(&now);   /* UI thread only */
    char d[32] = "", t[32] = "";
    uint32_t s = lc.rec.hz ? lc.rec.frames / lc.rec.hz : 0;
    int n;
    if (!dst || !cap) return -1;
    if (tm) { strftime(d, sizeof d, "%d-%m-%Y", tm); strftime(t, sizeof t, "%H:%M:%S", tm); }   /* PROPOSAL: LJ2 formats with the host short date */
    n = snprintf(dst, cap, "LightJockey Generated Lightcorder DMX Stream\r\nFile Created %s at %s\r\nRecording Time : %u:%02u\r\n"
                 "Recorded number of channels : %d\r\nRecording Frequency : %d Hz\r\n\r\nLightJockey Link Configuration\r\n",
                 d, t, (unsigned)(s / 60), (unsigned)(s % 60), lc.rec.channels, lc.rec.hz);   /* exe 0x1FF9AC, 0x1FF9E8, 0x1FFA00, 0x1FFA64, 0x1FFA14, 0x1FFA40, 0x1FFA80 */
    if (n < 0 || (size_t)n >= cap) return -1;
    fixture_list(dst, cap);
    return 0;
}

static int next_path(char *out, size_t cap)
{
    lj_ctx *ctx = lj_ctx_get();
    char dir[LJ_PATH_CAP], rel[64];
    int i;
    if (!ctx->lib || lj_library_path(ctx->lib, LJ_LLF_DIR, dir, sizeof dir) != 0 || lj_fs_mkdir(dir) != 0) return -1;
    for (i = 1; i <= 9999; i++) {
        FILE *f;
        snprintf(rel, sizeof rel, LJ_LLF_DIR "/LC%04d" LJ_LLF_EXT, i);   /* PROPOSAL name: LJ2 asks with a TSaveDialog (*.LLF) */
        if (lj_library_path(ctx->lib, rel, out, cap) != 0) return -1;
        if ((f = fopen(out, "rb")) == NULL) return 0;
        fclose(f);
    }
    return -1;
}

static void save_destroy(void)
{
    if (!lc.save) return;
    lj_memo_edit_detach(&lc.user); lj_memo_edit_detach(&lc.sys);
    lj_win_hide(lc.save);
    lj_window_destroy(lc.save);
    lc.save = NULL; lc.save_close_pending = 0;
}
static void on_save_file(lj_widget *w, void *ud)
{
    char path[LJ_PATH_CAP];
    (void)w; (void)ud;
    if (!lc.rec.data) return;
    if (next_path(path, sizeof path) != 0 || lj_llf_write(path, &lc.rec, lc.user.text, lc.sys.text) != 0) {
        lj_win_message(LJ_LC_CAPTION_WARNING, "Error saving Lightcorder File");   /* exe 0x1FFAF4 (its trailing newline dropped: one-line box) */
        return;
    }
    lj_str_copy(lc.last, sizeof lc.last, path);
    lj_log("lightcorder: %u frames saved to %s", (unsigned)lc.rec.frames, path);
    lj_llf_rec_free(&lc.rec);
    lc.save_close_pending = 1;
}
static void discard_answer(int yes, void *ud) { (void)ud; if (yes) { lj_llf_rec_free(&lc.rec); lc.save_close_pending = 1; } }
static void on_save_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm(LJ_LC_CAPTION_CONFIRM, "Cancel will loose any recorded data - do you wish to Cancel ?", NULL, discard_answer, NULL); }   /* exe 0x1FE5A4 */

static const lj_binding lj_lc_save_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bSaveClick", on_save_file, NULL }, { "bCancelClick", on_save_cancel, NULL },
    { NULL, NULL, NULL } };

static lj_window *save_open(void)
{
    char *notes = lc.sys.text;
    if (lc.save) return lc.save;
    if (!lc.rec.data) { lj_log("lightcorder: Save Lightcorder File has no recording to save"); return NULL; }
    lc.save = lj_window_create(&lj_form_TSAVELIGHTCORDERFILEDIALOG, lj_lc_save_bindings);
    if (!lc.save) { lj_log("lightcorder: cannot create TSaveLightCorderFileDialog"); return NULL; }
    lc.user.text[0] = 0; lc.user.changed = 0;
    if (lj_lightcorder_sys_notes(notes, sizeof lc.sys.text) != 0) notes[0] = 0;
    lj_memo_edit_attach(&lc.user, find(lc.save, "reUserNotes"));
    lj_memo_edit_attach(&lc.sys, find(lc.save, "reSysNotes"));
    lj_win_show(lc.save, 0);
    return lc.save;
}

/* ---- recording ---- */
static void stop_recording(void)
{
    lc.recording = 0;
    set_recording_ui(0);
    lj_log("lightcorder: stopped after %u frames (%u of %u card bytes)", (unsigned)lc.rec.frames, (unsigned)lc.rec.used, (unsigned)lc.rec.card_bytes);
    if (lc.rec.frames) save_open(); else lj_llf_rec_free(&lc.rec);
}

static void on_record(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (lc.recording) { stop_recording(); return; }
    if (lc.save) return;                                                     /* the last take is still waiting for Save / Cancel */
    if (lj_llf_rec_begin(&lc.rec, sel_link(), sel_channels(), sel_hz(), sel_card_mb(), rg("rgCompression") == 1) != 0) {
        lj_win_message(LJ_LC_CAPTION_WARNING, "Error pre-allocating allocating sufficient memory for lightcorder file- Recording canceled");   /* exe 0x1FFD94 */
        return;
    }
    lc.recording = 1; lc.started = 0; lc.sec_frames = 0;
    set_recording_ui(1);
    lj_log("lightcorder: recording link %d, %d channels, %d Hz, %d MB card, compression %s", lc.rec.link, lc.rec.channels, lc.rec.hz, sel_card_mb(), lc.rec.compress ? "on" : "off");
}

void lj_lightcorder_step(uint64_t now_us)
{
    lj_ctx *ctx = lj_ctx_get();
    uint8_t slots[LJ_LINK_CH];
    uint64_t tick = 0, den, due;
    const lj_frame *fr;
    int rc;
    if (lc.save_close_pending) save_destroy();
    if (lc.close_pending) lj_lightcorder_close(LJ_WIN_LIGHTCORDER);
    if (!lc.recording || !ctx->engine) return;
    den = (uint64_t)lc.rec.hz * lj_engine_tick_us(ctx->engine);             /* ME-164: keyed on the engine tick, never the wall clock */
    if (!den) return;
    lj_lock();                                                               /* seam 3: copy under the lock, write outside it */
    fr = lj_engine_frame(ctx->engine);
    if (fr) tick = fr->tick_no;
    if (!lc.started) { lc.started = 1; lc.t0_tick = tick; lc.k = 0; lc.sec_us = now_us; }
    due = lc.t0_tick + lc.k * 1000000u / den;
    if (fr && tick >= due) memcpy(slots, fr->out[lc.rec.link - 1], lc.rec.channels);
    lj_unlock();
    if (!fr || tick < due) return;
    memset(slots + lc.rec.channels, 0, sizeof slots - lc.rec.channels);
    rc = lj_llf_rec_push(&lc.rec, slots);
    do lc.k++; while (lc.t0_tick + lc.k * 1000000u / den <= tick);          /* each sample tick at most once; one the UI missed is skipped (no burst) */
    if (rc == 0) lc.sec_frames++;
    if (now_us - lc.sec_us >= 1000000u) {                                     /* ME-24: per-second counts while recording */
        lj_log("lightcorder: %u frames/s, tick %u us, sACN %u %u %u %u packets/s", (unsigned)lc.sec_frames, (unsigned)(ctx->engine ? lj_engine_tick_us(ctx->engine) : 0),
               (unsigned)ctx->view.sacn_pps[0], (unsigned)ctx->view.sacn_pps[1], (unsigned)ctx->view.sacn_pps[2], (unsigned)ctx->view.sacn_pps[3]);
        lc.sec_us = now_us; lc.sec_frames = 0;
    }
    if (lc.win) { lj_widget_invalidate(find(lc.win, "pbProgress")); lj_widget_invalidate(find(lc.win, "pbCompression")); }
    if (rc == 1) { lj_win_message(LJ_LC_CAPTION_WARNING, "Lightcorder card memory full - recording stopped"); stop_recording(); }   /* exe 0x1FF970 */
}

/* ---- the recorder window ---- */
static void on_gauge(lj_widget *w, void *ud)   /* pbProgressPaint, both boxes: card used / encoded vs raw size */
{
    lj_surface *s = lc.win ? lj_window_surface(lc.win) : NULL;
    uint64_t num = 0, den = 1;
    int ax, ay, fill;
    (void)ud;
    if (!s) return;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, LJ_CLR_BTNHIGHLIGHT);
    if (strcmp(w->name, "pbProgress") == 0) { num = lc.rec.used; den = lc.rec.card_bytes ? lc.rec.card_bytes : 1; }
    else if (lc.rec.frames) { num = lc.rec.used; den = (uint64_t)lc.rec.frames * (lc.rec.channels + 1u); }
    fill = (int)(num * (uint64_t)w->w / den);
    lj_fill(s, ax, ay, fill > w->w ? w->w : fill, w->h, LJ_LC_BAR);
}

static void close_answer(int yes, void *ud) { (void)ud; if (yes) { lc.recording = 0; lj_llf_rec_free(&lc.rec); lc.close_pending = 1; } }
static void on_form_close(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (lc.recording) lj_win_confirm(LJ_LC_CAPTION_CONFIRM, "Closing will cancel the current Lightcorder recording session", NULL, close_answer, NULL);   /* exe 0x1FFE68 */
    else lc.close_pending = 1;
}
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_lightcorder_refresh(LJ_WIN_LIGHTCORDER); }

static const lj_binding lj_lc_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_form_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "rgTargetClick", on_option, NULL }, { "rgChannelsClick", on_option, NULL }, { "rgFrequencyClick", on_option, NULL },
    { "pbProgressPaint", on_gauge, NULL }, { "bRecordClick", on_record, NULL }, { "Timer1Timer", on_timer, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_lightcorder_open(enum lj_win id)
{
    if (id == LJ_WIN_SAVE_LIGHTCORDER) return save_open();
    if (id != LJ_WIN_LIGHTCORDER) return NULL;
    if (lc.win) return lc.win;
    lc.win = lj_window_create(&lj_form_TLIGHTCORDERDIALOG, lj_lc_bindings);
    if (!lc.win) { lj_log("lightcorder: cannot create TLightCorderDialog"); return NULL; }
    lc.close_pending = 0;
    update_max_time();
    set_recording_ui(lc.recording);
    lj_win_show(lc.win, LJ_LC_STAY_ON_TOP);
    return lc.win;
}

void lj_lightcorder_close(enum lj_win id)
{
    if (id == LJ_WIN_SAVE_LIGHTCORDER) { save_destroy(); return; }
    if (id != LJ_WIN_LIGHTCORDER || !lc.win) return;
    if (lc.recording) { lc.recording = 0; lj_llf_rec_free(&lc.rec); lj_log("lightcorder: recording discarded (window closed)"); }
    lj_win_hide(lc.win);
    lj_window_destroy(lc.win);
    lc.win = NULL; lc.close_pending = 0;
}

lj_window *lj_lightcorder_get(enum lj_win id) { return id == LJ_WIN_LIGHTCORDER ? lc.win : id == LJ_WIN_SAVE_LIGHTCORDER ? lc.save : NULL; }

void lj_lightcorder_refresh(enum lj_win id)
{
    if (id == LJ_WIN_SAVE_LIGHTCORDER) { if (lc.save_close_pending) save_destroy(); return; }
    if (id != LJ_WIN_LIGHTCORDER) return;
    if (lc.close_pending) { lj_lightcorder_close(id); return; }
    if (lc.win && lc.recording) { lj_widget_invalidate(find(lc.win, "pbProgress")); lj_widget_invalidate(find(lc.win, "pbCompression")); }
}

void lj_lightcorder_bind_fns(void) { }   /* no hotkey function opens the recorder; Cuelist > Tools does (lj_cuelist_form.c) */
