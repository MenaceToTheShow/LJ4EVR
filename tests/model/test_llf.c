/* test_llf.c - `lj4evr_tests test_llf` (W4 lightcorder-debug track, W4-LC, ME-24):
 *  - model: a replay's link-1 frames recorded, written, read back frame-equal (compression on and off); the card cap
 *    (4 MB / 513-byte frames = 8176 frames, the next push refused); lMaxTime in both exe formats;
 *  - the untrusted reader: every prefix of a good file refused (truncation), a trailing byte, a bad header, a bad tag,
 *    a repeat frame first or without compression, a frame count past the data;
 *  - the recorder window headless: DFM items verbatim (Martin's "mamory" kept), the DFM defaults (4 MB, 512 ch, 40 Hz) give
 *    "Max recording time 3:24 (Min:Sec)", Start Recording -> "Cancel Recording", lj_lightcorder_step at 40 Hz from a live
 *    engine fills the card until "Lightcorder card memory full - recording stopped", Save Lightcorder File opens with the
 *    exe system notes, Save File writes LightCorderFiles\LC0001.LLF which reads back; ME-16 clean; goldens W4LC_*.
 * Temp tree test_llf_tmp/. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_replay.h"
#include "gfx/lj_bmp.h"
#include "model/lj_library.h"
#include "model/lj_llf.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_lightcorder.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_llf_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char logged[8192];
static int bind_complaints;
static void sink(void *ud, const char *line)
{
    size_t n = strlen(logged);
    (void)ud;
    if (getenv("LLF_DEBUG")) printf("  log: %s\n", line);
    if (n + strlen(line) + 2 < sizeof logged) { strcat(logged, line); strcat(logged, "\n"); }
    if ((strstr(line, "no binding") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler")) && (strstr(line, "TLightCorderDialog") || strstr(line, "TSaveLightCorderFileDialog"))) {
        bind_complaints++; printf("  log: %s\n", line);
    }
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(lj_window *w, const char *name)
{
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, TMP "%s.bmp", name);
    lj_window_invalidate_all(w);
    lj_window_paint(w);
    if (lj_bmp_write(lj_window_surface(w), upd && upd[0] == '1' ? path : tmp) != 0) { fails++; printf("FAIL golden %s: write\n", name); return; }
    if (upd && upd[0] == '1') { printf("  golden %s written\n", name); return; }
    rc = same_file(tmp, path);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (new render at %s)\n", name, rc == 0 ? "differs" : "missing", tmp); }
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

/* ---- model rows ---- */
enum { REC_FRAME = LJ_CHANS + LJ_LINKS * LJ_LINK_CH + 8 };   /* the dump-frames layout (lj_replay.h) */

static void test_round_trip(int compress)
{
    char path[256], err[160];
    FILE *dump = tmpfile();
    lj_replay_result rr;
    lj_llf_rec r;
    lj_llf_file f;
    uint8_t *frames = NULL, buf[REC_FRAME];
    int n = 0, i, eq = 1, rc;
    snprintf(path, sizeof path, "%s/tests/engine/replay/fade_linear_40hz.lj4r", LJ_SOURCE_DIR);
    CHECK(dump != NULL);
    if (!dump) return;
    memset(&rr, 0, sizeof rr);
    rc = lj_replay_run(path, dump, &rr);
    CHECK(rc == 0 && rr.ticks > 100);
    rewind(dump);
    CHECK(lj_llf_rec_begin(&r, 1, 512, 40, 4, compress) == 0);
    frames = (uint8_t *)malloc((size_t)rr.ticks * 512);
    while (frames && n < rr.ticks && fread(buf, 1, sizeof buf, dump) == sizeof buf) {
        memcpy(frames + (size_t)n * 512, buf + LJ_CHANS, 512);               /* out[0] = link 1 */
        CHECK(lj_llf_rec_push(&r, buf + LJ_CHANS) == 0);
        n++;
    }
    fclose(dump);
    CHECK(n == rr.ticks && r.frames == (uint32_t)n);
    if (compress) CHECK(r.used < (uint32_t)n * 513u);                         /* the held frames after the fade are 1 byte each */
    else CHECK(r.used == (uint32_t)n * 513u);
    snprintf(path, sizeof path, TMP "round%d" LJ_LLF_EXT, compress);
    CHECK(lj_llf_write(path, &r, "user\r\nnotes \xE9", "LightJockey Generated Lightcorder DMX Stream\r\n") == 0);
    lj_llf_rec_free(&r);
    CHECK(lj_llf_read(path, &f, err, sizeof err) == 0);
    if (err[0]) printf("  read: %s\n", err);
    CHECK(f.link == 1 && f.channels == 512 && f.hz == 40 && f.card_bytes == 4u << 20 && f.compress == compress && f.frames == (uint32_t)n);
    CHECK(f.user_notes && strcmp(f.user_notes, "user\r\nnotes \xE9") == 0);   /* CP1252 bytes travel untouched */
    CHECK(f.sys_notes && strcmp(f.sys_notes, "LightJockey Generated Lightcorder DMX Stream\r\n") == 0);
    for (i = 0; f.frame && frames && i < n; i++) if (memcmp(f.frame + (size_t)i * 512, frames + (size_t)i * 512, 512) != 0) { eq = 0; break; }
    CHECK(eq && f.frame && f.frame[51 * 512] == 128);                        /* the replay's tick 51 expect: 1:1 = 128 */
    lj_llf_file_free(&f);
    free(frames);
}

static void test_card_cap(void)
{
    lj_llf_rec r;
    uint8_t slots[512];
    uint32_t i = 0;
    int rc = 0;
    char t[96];
    memset(slots, 7, sizeof slots);
    CHECK(lj_llf_rec_begin(&r, 2, 512, 40, 4, 0) == 0);
    while ((rc = lj_llf_rec_push(&r, slots)) == 0 && i < 100000) { i++; slots[0]++; }
    CHECK(rc == 1 && i == (4u << 20) / 513u && r.frames == i && r.used == i * 513u);   /* 8176 */
    CHECK(lj_llf_rec_push(&r, slots) == 1 && r.frames == i);                 /* nothing stored past the cap */
    lj_llf_rec_free(&r);
    CHECK(lj_llf_rec_push(&r, slots) == -1);
    CHECK(lj_llf_rec_begin(&r, 0, 512, 40, 4, 0) == -1 && lj_llf_rec_begin(&r, 1, 100, 40, 4, 0) == -1 && lj_llf_rec_begin(&r, 1, 512, 30, 4, 0) == -1 && lj_llf_rec_begin(&r, 1, 512, 40, 5, 0) == -1);
    CHECK(lj_llf_max_seconds(4u << 20, 512, 40) == 204 && lj_llf_max_seconds(64u << 20, 64, 10) == 103244);
    CHECK(lj_lightcorder_max_time_text(t, sizeof t, 4, 512, 40) == 0 && strcmp(t, "Max recording time 3:24 (Min:Sec)") == 0);
    CHECK(lj_lightcorder_max_time_text(t, sizeof t, 64, 64, 10) == 0 && strcmp(t, "Max recording time 28:40:44 (Hour:Min:Sec)") == 0);
}

static int parse_fails(const uint8_t *b, size_t n, const char *want)
{
    lj_llf_file f;
    char err[160];
    int rc = lj_llf_parse(b, n, &f, err, sizeof err);
    if (rc == 0) { lj_llf_file_free(&f); return 0; }
    if (want && !strstr(err, want)) { printf("  error \"%s\" lacks \"%s\"\n", err, want); return 0; }
    return f.frame == NULL && f.user_notes == NULL;
}
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void test_hostile(void)
{
    lj_llf_rec r;
    uint8_t slots[64], *b, *x;
    size_t n, k;
    int all = 1;
    lj_llf_file f;
    char err[160];
    memset(slots, 1, sizeof slots);
    CHECK(lj_llf_rec_begin(&r, 3, 64, 10, 4, 1) == 0);
    lj_llf_rec_push(&r, slots); lj_llf_rec_push(&r, slots); slots[5] = 9; lj_llf_rec_push(&r, slots);   /* full, repeat, full */
    CHECK(r.frames == 3 && r.used == 65 + 1 + 65);
    CHECK(lj_llf_write(TMP "small" LJ_LLF_EXT, &r, "u", "s") == 0);
    lj_llf_rec_free(&r);
    b = slurp(TMP "small" LJ_LLF_EXT, &n);
    CHECK(b && n == LJ_LLF_HEADER + 2 + 131);
    if (!b) return;
    CHECK(lj_llf_parse(b, n, &f, err, sizeof err) == 0 && f.frames == 3 && f.frame[64 + 5] == 1 && f.frame[128 + 5] == 9);
    lj_llf_file_free(&f);
    for (k = 0; k < n; k++) if (!parse_fails(b, k, k < LJ_LLF_HEADER ? "truncated" : "truncated")) { all = 0; printf("  prefix %u accepted\n", (unsigned)k); break; }
    CHECK(all);                                                              /* hard rule 6: every truncation refused */
    x = (uint8_t *)malloc(n + 1);
    if (!x) { free(b); return; }
    memcpy(x, b, n); x[n] = 0; CHECK(parse_fails(x, n + 1, "trailing"));
    memcpy(x, b, n); x[10] = 100; CHECK(parse_fails(x, n, "bad header"));     /* channels 100 */
    memcpy(x, b, n); x[0] = 'X'; CHECK(parse_fails(x, n, "not an"));
    memcpy(x, b, n); x[LJ_LLF_HEADER + 2 + 65] = 2; CHECK(parse_fails(x, n, "bad tag"));
    memcpy(x, b, n); x[LJ_LLF_HEADER + 2] = 1; CHECK(parse_fails(x, n, NULL));   /* a repeat as the first frame */
    memcpy(x, b, n); x[9] = 0; CHECK(parse_fails(x, n, "bad tag"));           /* a repeat tag in an uncompressed file */
    memcpy(x, b, n); put32(x + 20, 4); CHECK(parse_fails(x, n, "ends early"));
    memcpy(x, b, n); put32(x + 20, 2); CHECK(parse_fails(x, n, "after the last frame"));
    memcpy(x, b, n); put32(x + 28, 0xFFFFFFF0u); CHECK(parse_fails(x, n, "bad lengths"));
    memcpy(x, b, n); put32(x + 24, 0xFFFFFFF0u); CHECK(parse_fails(x, n, "bad lengths"));
    CHECK(lj_llf_read(TMP "missing" LJ_LLF_EXT, &f, err, sizeof err) == -1);
    /* ME-172 (W4 LORD_C-8): a 41-byte file (the 40-byte header + one data byte) claiming 4,000,000 x 64 slots (256 MB) is
     * refused before the malloc */
    memcpy(x, b, LJ_LLF_HEADER + 1); put32(x + 20, 4000000u); put32(x + 24, 1); put32(x + 28, 0); put32(x + 32, 0);
    CHECK(parse_fails(x, LJ_LLF_HEADER + 1, "do not fit"));
    free(x); free(b);
}

/* ---- the recorder window ---- */
static void lc_tick(uint64_t t) { lj_ctx *c = lj_ctx_get(); lj_lock(); lj_engine_tick(c->engine, t); lj_unlock(); }   /* the engine thread's tick, stepped */
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static const char *cap_of(lj_window *w, const char *n) { lj_widget *x = in(w, n); const char *c = x ? lj_w_caption(x) : NULL; return c ? c : ""; }
static int radio_has(lj_window *w, const char *group, int i, const char *text)
{
    lj_widget *g = in(w, group), *c;
    int k = 0;
    for (c = g ? g->first : NULL; c; c = c->next, k++) if (k == i) return strcmp(lj_w_caption(c) ? lj_w_caption(c) : "", text) == 0;
    return 0;
}

static void test_window(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *w, *sv, *box;
    uint64_t t = 1000000;
    lj_llf_file f;
    char err[160], sys[4096];
    int i;
    bind_complaints = 0;
    w = lj_win_open(LJ_WIN_LIGHTCORDER);
    CHECK(w != NULL && lj_win_get(LJ_WIN_LIGHTCORDER) == w && bind_complaints == 0);
    if (!w) return;
    CHECK(strcmp(lj_window_caption(w)->text, "Record to LightCorder file") == 0);
    CHECK(radio_has(w, "rgTarget", 2, "16 MB mamory card") && radio_has(w, "rgLink", 3, "Link 4") && radio_has(w, "rgFrequency", 0, "10 Hz") && radio_has(w, "rgCompression", 1, "Enable"));
    CHECK(strcmp(cap_of(w, "lMaxTime"), "Max recording time 3:24 (Min:Sec)") == 0 && strcmp(cap_of(w, "bRecord"), "Start Recording") == 0);
    lj_w_radiogroup_set_index(in(w, "rgFrequency"), 0);                     /* 10 Hz: 817 s */
    CHECK(strcmp(cap_of(w, "lMaxTime"), "Max recording time 13:37 (Min:Sec)") == 0);
    lj_w_radiogroup_set_index(in(w, "rgFrequency"), 2);
    golden(w, "W4LC_LIGHTCORDER");

    lj_w_radiogroup_set_index(in(w, "rgCompression"), 0);                   /* raw frames: the cap is exactly 8176 samples */
    lj_w_click(in(w, "bRecord"));
    CHECK(lj_lightcorder_recording() && strcmp(cap_of(w, "bRecord"), "Cancel Recording") == 0 && !in(w, "rgTarget")->enabled);
    lj_lightcorder_step(t);                                                  /* first sample now */
    lj_lightcorder_step(t + 1000);                                           /* inside the 25 ms period: nothing */
    CHECK(lj_lightcorder_frames() == 1);
    logged[0] = 0;
    for (i = 1; i <= 8176 && lj_lightcorder_recording(); i++) { lc_tick(t + (uint64_t)i * 25000u); lj_lightcorder_step(t + (uint64_t)i * 25000u); }
    CHECK(!lj_lightcorder_recording() && lj_lightcorder_frames() == 8176);
    CHECK(strstr(logged, "Lightcorder card memory full - recording stopped") != NULL);
    CHECK(strstr(logged, "lightcorder: 40 frames/s") != NULL);              /* ME-24: the per-second count */
    box = lj_win_message_window();
    CHECK(box != NULL && strcmp(cap_of(box, "Label1"), "Lightcorder card memory full - recording stopped") == 0);
    if (box) lj_w_click(in(box, "BOk"));
    sv = lj_win_get(LJ_WIN_SAVE_LIGHTCORDER);
    CHECK(sv != NULL && bind_complaints == 0);
    if (!sv) return;
    CHECK(lj_lightcorder_sys_notes(sys, sizeof sys) == 0);
    CHECK(strstr(sys, "LightJockey Generated Lightcorder DMX Stream\r\nFile Created ") == sys && strstr(sys, "Recording Time : 3:24\r\n")
          && strstr(sys, "Recorded number of channels : 512\r\nRecording Frequency : 40 Hz\r\n") && strstr(sys, "LightJockey Link Configuration\r\n")
          && strstr(sys, " 1. Spot A (") && strstr(sys, ") at DMX address 1\r\n") && !strstr(sys, "Dimmer B"));   /* only link-1 fixtures inside the recorded channels */
    {   lj_ui_event ev;                                                      /* type into the File (User) Notes tab */
        const char *s = "Opening look";
        lj_w_tabs_select(in(sv, "PageControl1"), 0);
        lj_widget_set_focus(in(sv, "reUserNotes"));
        for (; *s; s++) { memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CHAR; ev.u.key.type = LJ_EV_CHAR; ev.u.key.ch = (unsigned char)*s; lj_window_event(sv, &ev); }
    }
    golden(sv, "W4LC_SAVE");
    lj_w_click(in(sv, "bSave"));
    lj_lightcorder_step(t + 9000u * 25000u);                                 /* the pending close */
    CHECK(lj_win_get(LJ_WIN_SAVE_LIGHTCORDER) == NULL && strstr(lj_lightcorder_last_file(), LJ_LLF_DIR) && strstr(lj_lightcorder_last_file(), "LC0001" LJ_LLF_EXT));
    CHECK(lj_llf_read(lj_lightcorder_last_file(), &f, err, sizeof err) == 0 && f.frames == 8176 && f.link == 1 && f.channels == 512 && !f.compress);
    CHECK(f.user_notes && strcmp(f.user_notes, "Opening look") == 0 && f.sys_notes && strstr(f.sys_notes, "Recording Frequency : 40 Hz"));
    CHECK(f.frame && f.frame[0] == 200 && f.frame[8175u * 512u] == 200);    /* the live engine frame: 1:001 = 200 */
    lj_llf_file_free(&f);

    /* a second take cancelled from Save: the discard prompt, nothing written */
    lj_w_click(in(w, "bRecord"));
    lj_lightcorder_step(t); lc_tick(t + 25000); lj_lightcorder_step(t + 25000);
    lj_w_click(in(w, "bRecord"));                                            /* Cancel Recording ends the take and opens Save */
    sv = lj_win_get(LJ_WIN_SAVE_LIGHTCORDER);
    CHECK(sv != NULL && lj_lightcorder_frames() == 2);
    if (sv) lj_w_click(in(sv, "bCancel"));
    box = lj_win_message_window();
    CHECK(box && strcmp(cap_of(box, "Label1"), "Cancel will loose any recorded data - do you wish to Cancel ?") == 0);
    if (box) lj_w_click(in(box, "BOk"));
    lj_lightcorder_step(t + 50000);
    CHECK(lj_win_get(LJ_WIN_SAVE_LIGHTCORDER) == NULL && lj_lightcorder_frames() == 0 && strstr(lj_lightcorder_last_file(), "LC0001"));

    /* ME-164: the tap is keyed on tick_no, not the wall clock: 100 engine ticks at 20 Hz recording (every second tick of
     * 40), three UI passes per tick, all at one wall time: exactly 50 samples (the wall-clock tap took 1, or 150) */
    lj_w_radiogroup_set_index(in(w, "rgFrequency"), 1);
    lj_w_click(in(w, "bRecord"));
    CHECK(lj_lightcorder_recording());
    for (i = 0; i < 100; i++) { lc_tick(t + (uint64_t)(i + 1) * 25000u); lj_lightcorder_step(t); lj_lightcorder_step(t); lj_lightcorder_step(t); }
    CHECK(lj_lightcorder_frames() == 50);
    if (lj_lightcorder_frames() != 50) printf("  %u samples from 100 ticks at 20 Hz\n", (unsigned)lj_lightcorder_frames());
    lj_w_click(in(w, "bRecord"));
    if ((sv = lj_win_get(LJ_WIN_SAVE_LIGHTCORDER)) != NULL) lj_w_click(in(sv, "bCancel"));
    if ((box = lj_win_message_window()) != NULL) lj_w_click(in(box, "BOk"));
    lj_lightcorder_step(t);
    lj_w_radiogroup_set_index(in(w, "rgFrequency"), 2);

    /* closing while recording asks first */
    lj_w_click(in(w, "bRecord"));
    {   lj_ui_event ev; memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CLOSE; lj_window_event(w, &ev); }
    box = lj_win_message_window();
    CHECK(box && strcmp(cap_of(box, "Label1"), "Closing will cancel the current Lightcorder recording session") == 0 && lj_win_get(LJ_WIN_LIGHTCORDER) == w);
    if (box) lj_w_click(in(box, "BOk"));
    lj_lightcorder_step(t + 75000);
    CHECK(lj_win_get(LJ_WIN_LIGHTCORDER) == NULL && !lj_lightcorder_recording());
    (void)lib; (void)c;
}

/* test_llf <file.LLF>: the playback half of the W4 acceptance (docs/w4-integration.md): decode a take the app recorded and
 * print, per frame, the slots that are not zero (address:value), so the frames can be read back against what was played. */
static int llf_play(const char *path)
{
    lj_llf_file f;
    char err[128];
    uint32_t i, nz = 0;
    if (lj_llf_read(path, &f, err, sizeof err) != 0) { printf("FAIL %s: %s\n", path, err); return 1; }
    printf("llf %s: link %u, %u channels, %u Hz, %u frame(s), card %u bytes, compress %u\n", path, (unsigned)f.link, (unsigned)f.channels, (unsigned)f.hz, (unsigned)f.frames, (unsigned)f.card_bytes, (unsigned)f.compress);
    for (i = 0; i < f.frames; i++) {
        int k, n = 0;
        printf("frame %u:", (unsigned)i);
        for (k = 0; k < f.channels; k++) if (f.frame[(size_t)i * f.channels + (size_t)k]) { if (n++ < 12) printf(" %d:%u", k + 1, (unsigned)f.frame[(size_t)i * f.channels + (size_t)k]); }
        if (n > 12) printf(" (+%d more)", n - 12);
        printf("\n");
        nz += n != 0;
    }
    printf("%u of %u frame(s) carry a non-zero slot\n", nz, f.frames);
    i = f.frames;
    lj_llf_file_free(&f);
    return i && nz ? 0 : 1;
}

int lj_test_llf(int argc, char **argv);
int lj_test_llf(int argc, char **argv)
{
    static lj_library lib;
    lj_ctx *c = lj_ctx_get();
    lj_scene *sc;
    if (argc > 1) return llf_play(argv[1]);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    test_round_trip(1);
    test_round_trip(0);
    test_card_cap();
    test_hostile();

    lj_log_set_sink(sink, NULL);
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->show->fixture[0].kind = LJ_FX_BUILTIN; c->show->fixture[0].profile_id = 19; c->show->fixture[0].link = 1; c->show->fixture[0].address = 1; strcpy(c->show->fixture[0].user_name, "Spot A");
    c->show->fixture[1].kind = LJ_FX_BUILTIN; c->show->fixture[1].profile_id = 19; c->show->fixture[1].link = 2; c->show->fixture[1].address = 1; strcpy(c->show->fixture[1].user_name, "Dimmer B");
    c->show->current_seq = lj_sequence_new_rec();
    if (!c->show->current_seq->scenes) lj_sequence_alloc(c->show->current_seq, 1);
    sc = &c->show->current_seq->scenes[0];
    sc->val[0] = 200; sc->state[0] = LJ_SNAP;
    c->engine = lj_engine_create();
    lj_lock(); lj_engine_bind(c->engine, c->show); lj_unlock();
    lj_ctx_set_scene(0);
    lj_lock(); lj_engine_tick(c->engine, lj_clock_us()); lj_engine_view(c->engine, &c->view); lj_unlock();
    CHECK(lj_engine_frame(c->engine)->out[0][0] == 200);

    test_window(&lib);

    lj_win_close_all();
    lj_log_set_sink(NULL, NULL);
    lj_engine_destroy(c->engine); c->engine = NULL;
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(c->show); c->show = NULL; c->lib = NULL;
    printf("test_llf: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
