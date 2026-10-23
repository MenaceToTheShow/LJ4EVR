/* test_uiscript_w2.c - the runnable check for the W2 app track (src/app/lj_app_seam.c and the W2
 * verbs of src/app/lj_uiscript.c): every new verb parses and a bad line reports its number; the
 * verbs run headless (assert_view W2 fields, assert_led, assert_file_line, sleep_until_tick, hotkey,
 * midi); a MIDI message posted from another thread is dispatched by lj_app_midi_dispatch on this
 * (the UI) thread (thread ids compared); pending_bg round trip: the engine requests a latched BG
 * cue, lj_app_service_pending loads and pins it, the next tick (the engine's ack) runs it, a
 * second pass loads nothing, and a missing number logs a load error without a crash. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <direct.h>
#define LJ_MKDIR(p) _mkdir(p)
static unsigned long long lj_t_tid(void) { return (unsigned long long)GetCurrentThreadId(); }
#else
#include <pthread.h>
#include <sys/stat.h>
#define LJ_MKDIR(p) mkdir(p, 0777)
static unsigned long long lj_t_tid(void) { return (unsigned long long)(uintptr_t)pthread_self(); }
#endif
#define NL "\n"
#include "app/lj_app_io.h"
#include "app/lj_app_seam.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "io/lj_audio.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "io/lj_midi.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_statics2_form.h"
#include "windows/lj_statics2_prefs.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"
#include "model/lj_statics.h"
#include "ui/widgets/lj_widgets_basic.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- the fn handler every MIDI / hotkey path ends in ---- */
static struct { int calls; enum lj_fn fn; uint32_t num; int edge; unsigned long long tid; } hit;
static void record(enum lj_fn fn, int edge, uint32_t prog_num, void *ud) { (void)ud; hit.calls++; hit.fn = fn; hit.num = prog_num; hit.edge = edge; hit.tid = lj_t_tid(); }

/* ---- log capture ---- */
static char logged[4096];
static void sink(void *ud, const char *line) { (void)ud; if (strlen(logged) + strlen(line) + 2 < sizeof logged) { strcat(logged, line); strcat(logged, "\n"); } }

static const lj_engine_status *view(void *ud) { (void)ud; return &lj_ctx_get()->view; }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static lj_window *seq_win;
static lj_window *resolve(void *ud, const char *form) { (void)ud; return (seq_win && strcmp(form, "TSequenceForm") == 0) ? seq_win : lj_uiscript_resolve_default(form); }

static int write_file(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (!f) return -1; fputs(text, f); fclose(f); return 0; }
static int file_has(const char *path, const char *text) { char buf[4096]; size_t n; FILE *f = fopen(path, "rb"); if (!f) return 0; n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0; return strstr(buf, text) != NULL; }

static int run_script(const char *path)
{
    int r = lj_uiscript_load(path);
    uint64_t t0 = lj_clock_us();
    if (r != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 15000000u) { }
    return r;
}

/* a script of "log" + one bad line: the load fails on line 2 */
static void bad_line(const char *line)
{
    char text[256];
    snprintf(text, sizeof text, "log \"ok\"\n%s\n", line);
    write_file("lj_w2_bad.lj4s", text);
    if (lj_uiscript_load("lj_w2_bad.lj4s") != -1 || lj_uiscript_line() != 2) { fails++; printf("FAIL: \"%s\" was not rejected on line 2 (line %d)\n", line, lj_uiscript_line()); }
}

static const uint8_t note_on[3] = { 0x90, 0x3C, 0x7F };
#ifdef _WIN32
static DWORD WINAPI poster(LPVOID p) { *(unsigned long long *)p = lj_t_tid(); lj_midi_post(note_on); return 0; }   /* what the WinMM callback does */
#else
static void *poster(void *p) { *(unsigned long long *)p = lj_t_tid(); lj_midi_post(note_on); return NULL; }
#endif

int lj_test_uiscript_w2(int argc, char **argv);
int lj_test_uiscript_w2(int argc, char **argv)
{
    static const char every_w2_verb[] =
        "midi 90 3C 7F\n"
        "midi 80 3c 00\n"
        "hotkey 2 F1\n"
        "hotkey 8 Shift+B\n"
        "assert_led bgcue 0\n"
        "assert_led statics 0\n"
        "assert_led smoke 0\n"
        "assert_view bg_any=0 statics_any=0 flash_latched=0 flash_seq=0\n"
        "assert_view transparent_mode=0 smoke_active=0 haze_level=0 chase_ms=0\n"
        "assert_view cue_number=0 bg_number=0 pending_link_cue=0 snapshot_tick=0\n"
        "assert_view gm 255\n"
        "assert_file_line w2.txt \"key=1\"\n"
        "sleep_until_tick 5\n";
    lj_ctx *c = lj_ctx_get();
    lj_library lib;
    lj_show *show;
    lj_engine *e;
    lj_hotkeys hk;
    lj_hotkey k;
    lj_sequence *s;
    lj_bgcue *bg;
    lj_cue *cue, *cue2;
    lj_cmd cmd;
    lj_uiscript_host host;
    const lj_runner *r;
    const void *bg_rec;
    char path[LJ_PATH_CAP];
    unsigned long long poster_tid = 0, ui_tid = lj_t_tid();
    lj_midi_map *map;
    lj_midi_map_entry me;
    (void)argc; (void)argv;

    /* ---- 1. parsing: every W2 verb loads; a bad line reports its number ---- */
    write_file("lj_w2_verbs.lj4s", every_w2_verb);
    CHECK(lj_uiscript_load("lj_w2_verbs.lj4s") == 0);
    bad_line("midi 90 3C");                 /* argument count */
    bad_line("midi 90 3C 1FF");             /* not a byte */
    bad_line("midi 3C 3C 7F");              /* no status byte */
    bad_line("midi 90 zz 7F");
    bad_line("hotkey 9 F1");                /* setup 1..8 */
    bad_line("hotkey 1 NoSuchKey");
    bad_line("assert_led sequence 1");
    bad_line("assert_led bgcue 2");
    bad_line("assert_view nosuchfield=1");
    bad_line("assert_view bg_any=x");
    bad_line("assert_view bg_any");
    bad_line("assert_file_line w2.txt");
    bad_line("sleep_until_tick -3");
    bad_line("sleep_until_tick soon");
    bad_line("move TSeqListForm 600");      /* move <Form> <x> <y> */
    bad_line("move TSeqListForm x 380");
#ifdef LJ_SOURCE_DIR
    {   char p[512];                          /* the W2 walkthrough parses (every line checked at load) */
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w2_acceptance.lj4s", LJ_SOURCE_DIR);
        CHECK(lj_uiscript_load(p) == 0);
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w2_positions_1.lj4s", LJ_SOURCE_DIR);   /* the restart persistence pair (integrator 2) */
        CHECK(lj_uiscript_load(p) == 0);
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w2_positions_2.lj4s", LJ_SOURCE_DIR);
        CHECK(lj_uiscript_load(p) == 0); }
#endif

    /* ---- 2. the app context headless: a library, a show, an engine without its thread ---- */
    LJ_MKDIR("lj_w2_app");
    memset(&lib, 0, sizeof lib);
    if (lj_library_open("lj_w2_app/", "W2App", &lib) != 0 && lj_library_create("lj_w2_app/", "W2App", &lib) != 0) { printf("FAIL: no library under lj_w2_app/\n"); return 1; }
    show = lj_show_new();
    show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].profile_id = 19; show->fixture[0].link = 1; show->fixture[0].address = 1;   /* one DMX dimmer at 1:001: section 8 reads its frame */
    e = lj_engine_create();
    lj_hotkeys_init(&hk);
    memset(c, 0, sizeof *c);
    c->show = show; c->engine = e; c->lib = &lib; c->hotkeys = &hk; c->main_ini = lj_ini_new();
    lj_lock(); lj_engine_bind(e, show); lj_unlock();
    {   static const lj_binding seq_b[] = {   /* the TSEQUENCEFORM handler names (as tests/ui/test_uiscript.c) */
            { "FormActivate", noop, NULL }, { "FormClose", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL },
            { "FormHide", noop, NULL }, { "FormShow", noop, NULL }, { "cbSyncClick", noop, NULL }, { "cbSyncEnter", noop, NULL },
            { "cbGlobalFadeClick", noop, NULL }, { "cbGlobalSceneClick", noop, NULL }, { "UdSceneChange", noop, NULL }, { "UdFadeChange", noop, NULL },
            { "DisableFade1Click", noop, NULL }, { "Preferences1Click", noop, NULL }, { "M_KeepBlindClick", noop, NULL }, { "M_SnapShotClick", noop, NULL },
            { "Help1Click", noop, NULL }, { NULL, NULL, NULL } };
        seq_win = lj_window_create(&lj_form_TSEQUENCEFORM, seq_b); }
    host.ud = NULL; host.resolve = resolve; host.view = view;
    lj_uiscript_set_host(&host);

    /* hotkey setup 2: F1 = Cue 1 (the .Hot grammar of io/lj_hotkeys.h) */
    memset(&k, 0, sizeof k); k.key = 0x70; k.fn = LJ_FN_PROG_CUE; k.prog_num = 1;
    { lj_hotkeys h2; lj_hotkeys_init(&h2); lj_hotkeys_assign(&h2, &k); CHECK(lj_hotkeys_setup_path(lib.root, 2, path, sizeof path) == 0 && lj_hotkeys_save(path, &h2) == 0); }
    CHECK(lj_library_path(&lib, "w2.txt", path, sizeof path) == 0 && write_file(path, "[X]\r\nkey=1\r\nkey=2\r\n") == 0);
    /* ME-95: the operator's DeviceCode on an interface that is not plugged in (no backend has an input 200) */
    CHECK(lj_library_path(&lib, "MIDIIn/MIDIIn.Ini", path, sizeof path) == 0 && write_file(path, "[SETUP]\r\nDeviceCode=200\r\n") == 0);
    lj_ini_seti(c->main_ini, "GLOBALPATCH", "HTPMasters", 1);   /* ME-94: the saved 11 P8 preference */

    logged[0] = 0;
    lj_log_set_sink(sink, NULL);
    lj_app_seam_start();
    lj_log_set_sink(NULL, NULL);
    CHECK(c->prefs != NULL && c->midi != NULL);
    CHECK(c->midi && c->midi->device == 200 && strstr(logged, "MIDI-IN: device 200 did not open") != NULL);   /* ME-95: kept, and the log names the requested index */
    lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0);
    CHECK(lj_library_path(&lib, "MIDIIn/MIDIIn.Ini", path, sizeof path) == 0 && file_has(path, "DeviceCode=200"));
    lj_lock(); lj_engine_tick(e, lj_clock_us()); lj_unlock();
    CHECK(lj_engine_post_ctx(e)->htp_masters == 1);          /* ME-94: posted at start, before any HTP edit */
    CHECK(lj_engine_post_ctx(e)->patch == show->patch);
    CHECK(c->allow_freeze == 1 && c->allow_offline == 1);   /* ME-29: a Main.Ini without the keys keeps LJ2's gates ON */
    lj_fn_bind(LJ_FN_PROG_CUE, record, NULL);                /* after the seam bound its own: every path below ends here */

    /* MIDI map: Cue tab enabled on All channels, note 60 -> cue 1 */
    map = c->midi;
    map->enabled[LJ_MIDI_CUE] = 1; map->channel[LJ_MIDI_CUE] = 16;
    memset(&me, 0, sizeof me); me.kind = LJ_MIDI_CUE; me.ch = LJ_MIDI_CH_ALL; me.note = 60; me.num = 1;
    CHECK(lj_midi_assign(map, &me) == 0);

    /* ---- 3. the W2 verbs headless ---- */
    c->view.gm = 255; c->view.tick_no = 7;                   /* the app's copy; sleep_until_tick 5 is already satisfied */
    memset(&hit, 0, sizeof hit);
    write_file("lj_w2_run.lj4s",
               "hotkey 2 F1\n"
               "assert_led bgcue 0\n"
               "assert_view bg_any=0 statics_any=0 gm=255\n"
               "assert_view cue_number 0\n"
               "assert_file_line w2.txt \"key=1\"\n"
               "sleep_until_tick 5\n"
               "midi 90 3C 7F\n"
               "assert_text TSequenceForm.M_KeepBlind \"Keep Contents When Entering Blind Mode\"\n"   /* a menu item: its caption */
               "quit\n");
    CHECK(run_script("lj_w2_run.lj4s") == 0);
    CHECK(hit.calls == 1 && hit.fn == LJ_FN_PROG_CUE && hit.num == 1 && hit.edge == 1);   /* the hotkey: F1 of setup 2 */
    memset(&hit, 0, sizeof hit);
    lj_app_midi_dispatch();                                  /* the idle pass after the `midi` line */
    CHECK(hit.calls == 1 && hit.num == 1 && hit.edge == 1 && hit.tid == ui_tid);
    /* ME-182: Pass Keypresses. The Analyzer (flag off by default) and the MediaPlayer keep a hotkey while their flag is 0;
     * with it 1 the hotkey acts (F1 of setup 2 = Cue 1) */
    lj_app_io_set_platform(NULL);
    lj_app_io_start();
    c->forms_ini = lj_ini_new();
    {   static const char *const opens[2] = { "open audio_analyzer", "open media_player" };
        static const char *const closes[2] = { "close audio_analyzer", "close media_player" };
        char text[160];
        int w2, pass;
        for (w2 = 0; w2 < 2; w2++)
            for (pass = 0; pass <= 1; pass++) {
                if (w2 == 0) c->audio->pass_keys = (uint8_t)pass; else lj_ini_seti(c->forms_ini, "LJMEDIAPLAYER", "KeyPass", pass);
                snprintf(text, sizeof text, "%s%shotkey 2 F1%s%s%squit%s", opens[w2], NL, NL, closes[w2], NL, NL);
                write_file("lj_w2_pass.lj4s", text);
                memset(&hit, 0, sizeof hit);
                CHECK(run_script("lj_w2_pass.lj4s") == 0 && hit.calls == pass);
            }
    }
    lj_ini_free(c->forms_ini); c->forms_ini = NULL;
    lj_app_io_stop();
    write_file("lj_w2_fail.lj4s", "log \"a\"\nassert_view gm=10\n");
    CHECK(run_script("lj_w2_fail.lj4s") == -1 && lj_uiscript_line() == 2);
    write_file("lj_w2_fail.lj4s", "assert_file_line w2.txt \"[Y]\"\n");
    CHECK(run_script("lj_w2_fail.lj4s") == -1 && lj_uiscript_line() == 1);
    write_file("lj_w2_fail.lj4s", "assert_text TSequenceForm.DisableFade1 \"Enable Fade\"\n");
    CHECK(run_script("lj_w2_fail.lj4s") == -1 && lj_uiscript_line() == 1);
    write_file("lj_w2_fail.lj4s", "log \"a\"\nlog \"b\"\nassert_led smoke 1\n");
    CHECK(run_script("lj_w2_fail.lj4s") == -1 && lj_uiscript_line() == 3);

    /* ---- 4. MIDI from another thread is run on this one (design 2.3 rule 5, ME-23) ---- */
    memset(&hit, 0, sizeof hit);
#ifdef _WIN32
    { HANDLE t = CreateThread(NULL, 0, poster, &poster_tid, 0, NULL); CHECK(t != NULL); if (t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); } }
#else
    { pthread_t t; CHECK(pthread_create(&t, NULL, poster, &poster_tid) == 0); pthread_join(t, NULL); }
#endif
    CHECK(hit.calls == 0);                                    /* the post alone runs nothing */
    lj_app_midi_dispatch();
    CHECK(hit.calls == 1 && hit.num == 1 && hit.tid == ui_tid && hit.tid != poster_tid && poster_tid != 0);

    /* ---- 5. pending_bg round trip ---- */
    s = lj_sequence_new_rec(); s->number = 3; s->scenes[0].val[0] = 200; s->scenes[0].state[0] = LJ_SNAP;
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 3, path, sizeof path) == 0 && lj_sequence_save(path, s) == 0);
    lj_sequence_free(s);
    bg = lj_bgcue_new_rec(); bg->number = 5; bg->slot[0].seq = 3; bg->slot[0].enabled = 1;
    CHECK(lj_library_record_path(&lib, LJ_REC_BG, 5, path, sizeof path) == 0 && lj_bgcue_save(path, bg) == 0);
    lj_bgcue_free(bg);
    cue = lj_cue_new_rec(); cue->number = 7; cue->bg_cue = 5;
    cue2 = lj_cue_new_rec(); cue2->number = 8; cue2->bg_cue = 99;   /* 99 is on no disk */
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 7; cmd.cue = cue; cmd.at_us = lj_clock_us();   /* the synthetic request: a cue latched with its BG cue not pinned */
    lj_lock();
    lj_table_put(&show->cues, 7, cue); lj_table_put(&show->cues, 8, cue2);
    lj_engine_cmd(e, &cmd);
    lj_engine_tick(e, lj_clock_us());
    lj_unlock();
    lj_app_service_pending();
    CHECK(c->view.pending_bg == 5);
    CHECK((bg_rec = lj_table_get(&show->bgcues, 5)) != NULL && lj_table_get(&show->sequences, 3) != NULL);   /* loaded and pinned by the UI thread */
    lj_lock(); lj_engine_tick(e, lj_clock_us() + 25000u); r = lj_engine_runner(e, LJ_RUN_BG0); lj_unlock();   /* the engine's ack: the LOAD_BG drained */
    CHECK(r && r->seq_no == 3 && r->seq == (const lj_sequence *)lj_table_get(&show->sequences, 3));
    lj_app_service_pending();
    CHECK(c->view.pending_bg == 0 && lj_table_get(&show->bgcues, 5) == bg_rec);   /* handed out once: nothing loaded twice */
    logged[0] = 0;
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_LOAD_CUE; cmd.a = 8; cmd.cue = cue2; cmd.at_us = lj_clock_us();
    lj_lock(); lj_engine_cmd(e, &cmd); lj_engine_tick(e, lj_clock_us() + 50000u); lj_unlock();
    lj_log_set_sink(sink, NULL);
    lj_app_service_pending();
    lj_log_set_sink(NULL, NULL);
    CHECK(c->view.pending_bg == 99 && strstr(logged, "background cue 0099: load error") != NULL && lj_table_get(&show->bgcues, 99) == NULL);
    lj_lock(); lj_engine_tick(e, lj_clock_us() + 75000u); lj_unlock();   /* no crash on the next tick */

    /* ---- 6. W2 integration: forms.ini [CUE] "Flash Auto Release" = Release Flash on Load [03 s5.10] ---- */
    CHECK(lj_library_record_path(&lib, LJ_REC_CUE, 7, path, sizeof path) == 0 && lj_cue_save(path, cue) == 0);
    c->forms_ini = lj_ini_new();
    {   int pass;
        for (pass = 0; pass < 2; pass++) {                     /* pass 0: box clear, the latched flash survives the load; pass 1: box set, released */
            lj_ini_seti(c->forms_ini, "CUE", "Flash Auto Release", pass);
            memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_FLASH; cmd.b = 1; cmd.c = 1; cmd.seq[0] = lj_table_get(&show->sequences, 3); cmd.at_us = lj_clock_us();
            lj_lock(); lj_engine_cmd(e, &cmd); lj_engine_tick(e, lj_clock_us() + 100000u); r = lj_engine_runner(e, LJ_RUN_FLASH); lj_unlock();
            CHECK(r && r->seq_no == 3);
            CHECK(lj_app_load_cue(7, 0) == 0);
            lj_lock(); lj_engine_tick(e, lj_clock_us() + 125000u); r = lj_engine_runner(e, LJ_RUN_FLASH); lj_unlock();
            CHECK(r && (pass ? r->seq == NULL : r->seq_no == 3));
            memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_FLASH; cmd.at_us = lj_clock_us();
            lj_lock(); lj_engine_cmd(e, &cmd); lj_engine_tick(e, lj_clock_us() + 150000u); lj_unlock();
        }
    }
    lj_ini_free(c->forms_ini); c->forms_ini = NULL;

    /* ---- 7. W2 acceptance: New Cue after a load keeps the table's record (the walkthrough's Link to Cue load, New Cue,
     *         MIDI load of another cue freed cue 0001's table record and the exit freed it again) ---- */
    {   lj_cue *loaded = show->current_cue, *nine = lj_cue_new_rec();
        nine->number = 9;
        CHECK(lj_library_record_path(&lib, LJ_REC_CUE, 9, path, sizeof path) == 0 && lj_cue_save(path, nine) == 0);
        lj_cue_free(nine);
        CHECK(loaded && lj_table_get(&show->cues, 7) == loaded);
        lj_cue_form_new_cue();
        CHECK(show->current_cue && show->current_cue != loaded && show->current_cue->number == 0);
        CHECK(lj_table_get(&show->cues, 7) == loaded && loaded->number == 7);
        CHECK(lj_app_load_cue(9, 0) == 0);                      /* frees the unsaved working record, not cue 0007's */
        CHECK(lj_table_get(&show->cues, 7) == loaded && loaded->number == 7 && show->current_cue == lj_table_get(&show->cues, 9));
        lj_lock(); lj_engine_tick(e, lj_clock_us() + 175000u); lj_unlock();
    }

    /* ---- 8. W2 fix pass r1 (ME-88, ME-90, ME-93) ---- */
    {   lj_cue *ten = lj_cue_new_rec();
        lj_sequence *cur = NULL;
        const lj_frame *f;
        lj_engine_status v;
        lj_window *w;
        int row, i;
        uint64_t t = lj_clock_us() + 200000u;
        ten->number = 10; ten->slot[0].seq = 3; ten->slot[0].enabled = 1;
        CHECK(lj_library_record_path(&lib, LJ_REC_CUE, 10, path, sizeof path) == 0 && lj_cue_save(path, ten) == 0);
        lj_cue_free(ten);
        CHECK(lj_app_load_cue(10, 0) == 0);
        lj_desktop_run_action(LJ_ACT_LOAD_BGCUE, 0);             /* section 5's latched BG cue (rank 5) would cover the static */
        lj_lock(); lj_engine_tick(e, t); r = lj_engine_runner(e, LJ_RUN_SLOT0); lj_unlock();
        CHECK(r && r->seq_no == 3 && r->seq == lj_table_get(&show->sequences, 3));

        /* ME-88: a link slot (Link to current Sequence/Scene) outputs its sequence's scene from the pinned record */
        CHECK(lj_statics2_form_open() != NULL && show->npages >= 1);
        lj_lock(); CHECK(lj_static_slot_link(&show->pages[0].slot[0], 3, 0) == 0); show->pages[0].slot[0].level_pct = 100; lj_unlock();
        CHECK(lj_statics2_press(1, 1) == 0);
        lj_lock(); lj_engine_tick(e, t += 25000u); f = lj_engine_frame(e); CHECK(f->merged[0] == 200 && f->layer_of[0] == 4); lj_unlock();
        /* re-saved (Quick Save of the loaded sequence): the table record is replaced and freed; the static reads the new one */
        CHECK(lj_app_load_sequence(3) == 0 && (cur = show->current_seq) != NULL);
        if (cur) { lj_lock(); cur->scenes[0].val[0] = 150; lj_unlock(); }
        CHECK(lj_app_save_sequence(0) == 0);
        lj_lock(); lj_engine_tick(e, t += 25000u); f = lj_engine_frame(e); CHECK(f->merged[0] == 150 && f->layer_of[0] == 4); lj_unlock();

        /* ME-90: Delete Files through the dialog reaches the app's deleter: the slot shows load error, the static goes dark */
        w = lj_win_open(LJ_WIN_DELETE_SEQ);
        for (row = -1, i = 0; i < lj_delete_files_count(LJ_REC_SEQ); i++) if (lj_delete_files_number(LJ_REC_SEQ, i) == 3) row = i;
        CHECK(w && row >= 0);
        if (w && row >= 0) {
            lj_delete_files_select(LJ_REC_SEQ, row, 1);
            lj_w_click(lj_window_find(w, "BDelete"));
            if (lj_win_message_window()) lj_w_click(lj_window_find(lj_win_message_window(), "BOk"));
        }
        lj_win_close(LJ_WIN_DELETE_SEQ);
        CHECK(lj_table_get(&show->sequences, 3) == NULL);
        CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 3, path, sizeof path) == 0 && !file_has(path, ""));
        lj_lock(); lj_engine_tick(e, t += 25000u); f = lj_engine_frame(e); CHECK(f->layer_of[0] == 3);   /* the static adds nothing: the current sequence (rank 3, the editing copy) is what is left */ lj_engine_view(e, &v); lj_unlock();
        CHECK(v.slot_status[0] == LJ_SLOT_LOAD_ERROR && v.statics_any == 1);
        lj_statics2_release(0);
        lj_statics2_form_close();

        /* ME-93: Use Global Chase Rate is one Main.Ini key, on by default, posted when the menu toggles; a tap on slot 1 sets slot 2 */
        lj_lock(); lj_engine_tick(e, t += 25000u); lj_engine_view(e, &v); lj_unlock();
        CHECK(v.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] == 1);
        CHECK(lj_cue_form_open() != NULL);
        write_file("lj_w2_chase.lj4s", "click TCurrentCue.M_UseGlobalChaseRate\nquit\n");
        CHECK(run_script("lj_w2_chase.lj4s") == 0 && lj_ini_geti(c->main_ini, "CUE", "UseGlobalChaseRate", -1) == 0);
        lj_lock(); lj_engine_tick(e, t += 25000u); lj_engine_view(e, &v); lj_unlock();
        CHECK(v.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] == 0);
        CHECK(run_script("lj_w2_chase.lj4s") == 0 && lj_ini_geti(c->main_ini, "CUE", "UseGlobalChaseRate", -1) == 1);
        lj_cue_form_close();
        for (i = 0; i < 2; i++) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_CHASE; cmd.a = (uint32_t)i; cmd.b = 1; cmd.at_us = t; lj_ctx_post(&cmd); }
        for (i = 0; i < 2; i++) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_CHASE_TAP; cmd.a = 0; cmd.at_us = t + 1000u + (uint64_t)i * 300000u; lj_ctx_post(&cmd); }
        lj_lock(); lj_engine_tick(e, t += 325000u); lj_engine_view(e, &v); r = lj_engine_runner(e, LJ_RUN_SLOT0 + 1); lj_unlock();
        CHECK(v.prefs[LJ_PREF_USE_GLOBAL_CHASE_RATE] == 1 && v.chase_tap_us == 300000u && r && r->chase_us == 300000u);
        for (i = 0; i < 2; i++) { memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_TRIG; cmd.a = LJ_RUN_CURRENT; cmd.b = 0xFF; cmd.at_us = t + (uint64_t)i * 100000u; lj_ctx_post(&cmd); }
        lj_lock(); lj_engine_tick(e, t += 125000u); lj_engine_view(e, &v); lj_unlock();
        CHECK(v.chase_tap_us == 300000u);                       /* the Sequence window's Trig is no chase tap (11 F10) */
    }

    /* ---- 9. ME-91: Statics Preferences adds page 2 and shows 30 slots while a static on page 1 runs; Static entry properties ---- */
    {   static const char *const keys[9] = { "Width", "Height", "LvlVisible", "ModeVisible", "SlotsVisible", "TabWidth", "TabHeight", "Multiline", "PRWidth" };
        lj_scene sc2;
        lj_window *pw, *nw, *sw;
        const lj_frame *f;
        lj_static_page *before;
        int i;
        uint64_t t = lj_clock_us() + 2000000u;
        CHECK((sw = lj_statics2_form_open()) != NULL);
        lj_scene_init(&sc2); sc2.val[0] = 77; sc2.state[0] = LJ_SNAP;
        lj_lock(); CHECK(lj_static_slot_record(&show->pages[0].slot[1], &sc2) == 0); show->pages[0].slot[1].level_pct = 100; lj_unlock();
        CHECK(lj_statics2_press(2, 1) == 0);
        before = show->pages;
        CHECK((pw = lj_win_open(LJ_WIN_STATICS2_PREFS)) != NULL);
        lj_w_click(lj_window_find(pw, "sbAddPage"));
        CHECK((nw = lj_statics2_page_name_window()) != NULL && strcmp(lj_window_caption(nw)->text, "Add Page") == 0);
        if (nw) { lj_w_edit_set_text(lj_window_find(nw, "eName"), "Second"); lj_w_click(lj_window_find(nw, "BitBtn1")); }
        CHECK(lj_statics2_page_name_window() == NULL && show->npages == 2 && strcmp(show->pages[1].name, "Second") == 0 && show->pages == before);
        lj_lock(); lj_engine_tick(e, t); f = lj_engine_frame(e); CHECK(f->merged[0] == 77 && f->layer_of[0] == 4); lj_unlock();   /* the held slot is still the engine's (ASan: no realloc under it) */
        lj_w_edit_set_text(lj_window_find(pw, "eRows"), "30");
        lj_w_click(lj_window_find(pw, "bOk"));
        lj_statics2_prefs_refresh();
        CHECK(lj_win_get(LJ_WIN_STATICS2_PREFS) == NULL && c->static_visible == 30 && show->pages[0].n == 30 && show->pages[1].n == 30);
        CHECK((sw = lj_statics2_form_get()) != NULL && lj_window_root(sw)->h == 58 + 30 * LJ_S2_ROW_H + 30);
        CHECK(sw && ((lj_w_tabs *)lj_window_find(sw, "tbPages"))->ntabs == 2);
        for (i = 0; i < 9; i++) if (!lj_ini_gets(c->main_ini, "STATICS2", keys[i], NULL)) { fails++; printf("FAIL Main.Ini [STATICS2] %s missing\n", keys[i]); }
        CHECK(lj_ini_geti(c->main_ini, "STATICS2", "SlotsVisible", 0) == 30 && lj_ini_geti(c->main_ini, "STATICS2", "PRWidth", 0) == 202);
        /* Static entry properties of slot 2 (the slot of the last popup; Add Page made page 2 current) */
        CHECK(lj_statics2_page() == 2 && lj_statics2_set_page(1) == 0);
        lj_statics2_popup(2, 10, 10);
        CHECK((pw = lj_win_open(LJ_WIN_STATIC_PROPS)) != NULL);
        if (pw) {
            CHECK(strcmp(lj_w_caption(lj_window_find(pw, "lSlot")), "Entry: ") == 0 && ((lj_w_combobox *)lj_window_find(pw, "cbReplaceMode"))->n == 9);
            lj_w_edit_set_text(lj_window_find(pw, "eName"), "Haze");
            ((lj_w_combobox *)lj_window_find(pw, "cbReplaceMode"))->index = LJ_REP_PAGE;
            lj_w_click(lj_window_find(pw, "sbSetGrpAll"));
            CHECK(strcmp(lj_w_caption(lj_window_find(pw, "lGroupCount")), "64 Groups") == 0);
            lj_w_click(lj_window_find(pw, "bOk"));
            lj_static_props_refresh();
            CHECK(lj_win_get(LJ_WIN_STATIC_PROPS) == NULL && strcmp(show->pages[0].slot[1].name, "Haze") == 0 && show->pages[0].slot[1].replace == LJ_REP_PAGE && show->pages[0].slot[1].groups == UINT64_MAX);
        }
        lj_statics2_release(0);
        lj_lock(); lj_engine_tick(e, t + 25000u); lj_unlock();
        lj_statics2_form_close();
    }

    /* ---- teardown ---- */
    lj_app_seam_stop();
    lj_fn_unbind_all();
    lj_lock(); lj_engine_bind(e, NULL); c->engine = NULL; lj_unlock();
    lj_engine_destroy(e);
    lj_lock(); lj_show_free(show); c->show = NULL; lj_unlock();
    lj_ini_free(c->main_ini);
    memset(c, 0, sizeof *c);
    lj_uiscript_set_host(NULL);
    lj_window_destroy(seq_win); seq_win = NULL;
    printf("test_uiscript_w2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
