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
#include "app/lj_app_seam.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
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
#ifdef LJ_SOURCE_DIR
    {   char p[512];                          /* the W2 walkthrough parses (every line checked at load) */
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w2_acceptance.lj4s", LJ_SOURCE_DIR);
        CHECK(lj_uiscript_load(p) == 0); }
#endif

    /* ---- 2. the app context headless: a library, a show, an engine without its thread ---- */
    LJ_MKDIR("lj_w2_app");
    memset(&lib, 0, sizeof lib);
    if (lj_library_open("lj_w2_app/", "W2App", &lib) != 0 && lj_library_create("lj_w2_app/", "W2App", &lib) != 0) { printf("FAIL: no library under lj_w2_app/\n"); return 1; }
    show = lj_show_new();
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

    lj_app_seam_start();
    CHECK(c->prefs != NULL && c->midi != NULL);
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
