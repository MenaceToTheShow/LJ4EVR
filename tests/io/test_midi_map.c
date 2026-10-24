/* test_midi_map.c - `lj4evr_tests test_midi_map`: io/lj_midi.h without a device: LJ2's shipped
 * defaults (lj_midi_init), assign / unassign / match (exact channel before All), the queue the OS
 * thread posts into (lj_midi_inject = the UI script's `midi` verb) and the UI-thread dispatch:
 * note on 90 3C 7F -> lj_fn_run_prog(LJ_FN_PROG_CUE, 1, 1); velocity 0 = note off (Note0); note
 * off ignored by a latched cue, used for the flash release and (LatchOff) the cue clear, only for
 * the last note on; channel gates; CC 7 -> LJ_FN_MASTER_LEVEL 0..127 -> 0..100 %; Learn binds the
 * next note only; malformed messages dropped, never dispatched; a full queue drops and counts;
 * MIDIIn.Ini (LJ2's keys, unknown keys kept) + MIDIMap.dat round trip and the ME-77 refusals. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as tests/io/test_hotkeys.c) */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "io/lj_fn.h"
#include "io/lj_midi.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_t_calls, lj_t_edge; static enum lj_fn lj_t_fn; static uint32_t lj_t_num;
static void lj_t_handler(enum lj_fn fn, int edge, uint32_t num, void *ud) { (void)ud; lj_t_calls++; lj_t_fn = fn; lj_t_edge = edge; lj_t_num = num; }

static void lj_t_drain(const lj_midi_map *m) { uint8_t msg[3]; while (lj_midi_poll(msg)) lj_midi_dispatch(m, msg); }
static void lj_t_send(const lj_midi_map *m, int b0, int b1, int b2) { uint8_t msg[3]; msg[0] = (uint8_t)b0; msg[1] = (uint8_t)b1; msg[2] = (uint8_t)b2; lj_midi_inject(msg); lj_t_drain(m); }
static void lj_t_write(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }
static lj_midi_map_entry lj_t_entry(int kind, int ch, int note, enum lj_fn fn, uint32_t num)
{ lj_midi_map_entry e; memset(&e, 0, sizeof e); e.kind = (uint8_t)kind; e.ch = (uint8_t)ch; e.note = (uint8_t)note; e.fn = (uint16_t)fn; e.num = num; return e; }

static const char lj_t_shipped_ini[] =   /* reference/.../User/Default/MIDIIn/MIDIIn.Ini, the shipped file's keys and values */
    "[FORM]\r\nTop=574\r\nLeft=1207\r\n[OPTIONS]\r\nOnTop=1\r\nStartMinimized=0\r\nAutoSave=1\r\n[SETUP]\r\nDeviceCode=0\r\nNote0=1\r\nFlashOff=1\r\n"
    "LatchOff=0\r\nListOff=0\r\nBGLatchOff=0\r\nSeqOff=1\r\n[SEQ]\r\nSeqEnable=0\r\nSeqChannel=16\r\n[CUE]\r\nCueEnable=0\r\nCueChannel=16\r\n[BGCUE]\r\n"
    "BGCueEnable=0\r\nBGCueChannel=16\r\n[CUELIST]\r\nCueListChannel=16\r\nCueListEnable=0\r\n[FUNCTIONS]\r\nFunctionsEnable=0\r\nFunctionsChannel=16\r\n";

static void lj_t_files(void)
{
    static lj_midi_map a, b;
    lj_midi_map_entry e;
    lj_ini *ini;
    char err[256];
    int i;

    lj_t_write("test_midi_MIDIIn.Ini", lj_t_shipped_ini);       /* the shipped file loads to the shipped defaults */
    CHECK(lj_midi_load("test_midi_MIDIIn.Ini", "test_midi_absent.dat", &a, err, sizeof err) == 0 && err[0] == 0);
    CHECK(a.device == 0 && a.note0_is_off == 1 && a.release_on_off[LJ_MIDI_SEQ] == 1 && a.release_on_off[LJ_MIDI_CUE] == 0 && a.on_top == 1 && a.auto_save == 1 && a.n == 0);
    for (i = 0; i < LJ_MIDI_KIND_COUNT; i++) CHECK(a.enabled[i] == 0 && a.channel[i] == 16);
    CHECK(lj_midi_load("test_midi_absent.Ini", "test_midi_absent.dat", &a, err, sizeof err) == 0 && a.device == -1);   /* both missing: defaults */

    a.enabled[LJ_MIDI_CUE] = 1; a.channel[LJ_MIDI_CUE] = 3; a.release_on_off[LJ_MIDI_BGCUE] = 1; a.start_minimized = 1; a.device = 2;
    a.enabled[LJ_MIDI_CC] = 1;
    e = lj_t_entry(LJ_MIDI_CUE, 0, 60, LJ_FN_NONE, 12);              CHECK(lj_midi_assign(&a, &e) == 0);
    e = lj_t_entry(LJ_MIDI_SEQ, 16, 127, LJ_FN_NONE, 2000000000u);   CHECK(lj_midi_assign(&a, &e) == 0);
    e = lj_t_entry(LJ_MIDI_FN, 1, 0, LJ_FN_BLACKOUT_RESTORE, 0);      CHECK(lj_midi_assign(&a, &e) == 0);
    e = lj_t_entry(LJ_MIDI_CC, 0, 7, LJ_FN_MASTER_LEVEL, 0);          CHECK(lj_midi_assign(&a, &e) == 0);
    CHECK(lj_midi_save("test_midi_MIDIIn.Ini", "test_midi_MIDIMap.dat", &a) == 0);
    memset(&b, 0x5A, sizeof b);
    CHECK(lj_midi_load("test_midi_MIDIIn.Ini", "test_midi_MIDIMap.dat", &b, err, sizeof err) == 0 && err[0] == 0);
    CHECK(b.n == a.n && b.device == 2 && b.start_minimized == 1 && b.enabled[LJ_MIDI_CUE] == 1 && b.channel[LJ_MIDI_CUE] == 3 && b.release_on_off[LJ_MIDI_BGCUE] == 1 && b.enabled[LJ_MIDI_CC] == 1);
    CHECK(memcmp(b.enabled, a.enabled, sizeof a.enabled) == 0 && memcmp(b.channel, a.channel, sizeof a.channel) == 0 && memcmp(b.release_on_off, a.release_on_off, sizeof a.release_on_off) == 0);
    for (i = 0; i < a.n && i < b.n; i++)   /* field-wise: the entry has padding */
        if (a.e[i].kind != b.e[i].kind || a.e[i].ch != b.e[i].ch || a.e[i].note != b.e[i].note || a.e[i].fn != b.e[i].fn || a.e[i].num != b.e[i].num) { fails++; printf("FAIL map round trip row %d\n", i); }
    ini = lj_ini_load("test_midi_MIDIIn.Ini");                   /* keys we do not own survive the save; LJ2's spellings */
    CHECK(ini && lj_ini_geti(ini, "FORM", "Left", 0) == 1207 && lj_ini_geti(ini, "SETUP", "SeqOff", 0) == 1 && lj_ini_geti(ini, "CUE", "CueChannel", 0) == 3 && lj_ini_geti(ini, "SETUP", "BGLatchOff", 0) == 1);
    lj_ini_free(ini);

    /* ME-77: a present bad value refuses the file (defaults out), never aliases into range */
    lj_t_write("test_midi_bad.Ini", "[CUE]\r\nCueChannel=17\r\n");
    CHECK(lj_midi_load("test_midi_bad.Ini", NULL, &b, err, sizeof err) == -1 && strstr(err, "CueChannel") != NULL && b.channel[LJ_MIDI_CUE] == 16);
    lj_t_write("test_midi_bad.Ini", "[SETUP]\r\nNote0=x\r\n");
    CHECK(lj_midi_load("test_midi_bad.Ini", NULL, &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=cue ch=0 note=128 num=1\n");
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1 && b.n == 0);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=cue ch=17 note=1 num=1\n");
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=cue ch=0 note=1 num=0\n");
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=fn ch=0 note=1 fn=LJ_FN_NOPE\n");
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=organ ch=0 note=1 num=1\n");
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nmap kind=cue ch=0 num=1\n");   /* note missing */
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 hot 1\n");                 /* wrong kind of file */
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == -1);
    lj_t_write("test_midi_bad.dat", "LJ4 midimap 1\nfuture=1\nmap kind=cl ch=5 note=9 num=3 extra=7\n");   /* unknown keys skipped (design 5.2) */
    CHECK(lj_midi_load(NULL, "test_midi_bad.dat", &b, err, sizeof err) == 0 && b.n == 1 && b.e[0].kind == LJ_MIDI_CL && b.e[0].ch == 5 && b.e[0].num == 3);
    a.channel[LJ_MIDI_FN] = 0;                                    /* an invalid map is never written */
    CHECK(lj_midi_save("test_midi_MIDIIn.Ini", "test_midi_MIDIMap.dat", &a) == -1);
    remove("test_midi_MIDIIn.Ini"); remove("test_midi_MIDIMap.dat"); remove("test_midi_bad.Ini"); remove("test_midi_bad.dat");
}

/* W3 (cuelist-core): LORD_C-6 lj_midi_feed rows and one MTC quarter-frame sequence through lj_mtc_feed */
static uint8_t lj_t_fm[8][3]; static int lj_t_fn_count;
static void lj_t_fmsg(const uint8_t m[3], void *ud) { (void)ud; if (lj_t_fn_count < 8) memcpy(lj_t_fm[lj_t_fn_count], m, 3); lj_t_fn_count++; }
static void lj_t_feed_mtc(void)
{
    static const uint8_t running[] = { 0x90, 60, 100, 61, 0 };                 /* running status: two note ons */
    static const uint8_t realtime[] = { 0x90, 0xF8, 62, 0xFE, 90 };            /* clock / active sensing inside a message */
    static const uint8_t sysex[] = { 0xF0, 0x7E, 0x10, 0xF7, 0xC3, 5, 0xD0, 7 };   /* SysEx skipped; program change, pressure 1 byte */
    static const uint8_t qf_then[] = { 0x90, 64, 1, 0xF1, 0x35, 65, 2 };        /* F1 emitted, then cancels running status */
    uint8_t run[3] = { 0, 0, 0 }; int have = 0, k;
    lj_mtc t; uint32_t ms = 0;
    /* 01:02:03:04 at 25 fps, the eight pieces 0..7 (rate 1 = 25 in piece 7 bits 1-2) */
    const uint8_t qf[8] = { 0x04, 0x10, 0x23, 0x30, 0x42, 0x50, 0x61, 0x72 };
    lj_t_fn_count = 0;
    CHECK(lj_midi_feed(running, sizeof running, run, &have, lj_t_fmsg, NULL) == 2 && lj_t_fm[1][0] == 0x90 && lj_t_fm[1][1] == 61 && lj_t_fm[1][2] == 0);
    lj_t_fn_count = 0; memset(run, 0, 3); have = 0;
    CHECK(lj_midi_feed(realtime, sizeof realtime, run, &have, lj_t_fmsg, NULL) == 1 && lj_t_fm[0][1] == 62 && lj_t_fm[0][2] == 90);
    lj_t_fn_count = 0; memset(run, 0, 3); have = 0;
    CHECK(lj_midi_feed(sysex, sizeof sysex, run, &have, lj_t_fmsg, NULL) == 2 && lj_t_fm[0][0] == 0xC3 && lj_t_fm[0][1] == 5 && lj_t_fm[0][2] == 0 && lj_t_fm[1][0] == 0xD0);
    lj_t_fn_count = 0; memset(run, 0, 3); have = 0;
    CHECK(lj_midi_feed(qf_then, sizeof qf_then, run, &have, lj_t_fmsg, NULL) == 2 && lj_t_fm[1][0] == 0xF1 && lj_t_fm[1][1] == 0x35 && lj_t_fm[1][2] == 0);
    lj_t_fn_count = 0; memset(run, 0, 3); have = 0;   /* a message split across two calls keeps its running state */
    CHECK(lj_midi_feed(qf_then, 2, run, &have, lj_t_fmsg, NULL) == 0 && lj_midi_feed(qf_then + 2, 1, run, &have, lj_t_fmsg, NULL) == 1 && lj_t_fm[0][1] == 64);
    lj_mtc_reset(&t);
    CHECK(lj_mtc_time(&t, 0, &ms) == -1);
    for (k = 0; k < 7; k++) CHECK(lj_mtc_feed(&t, qf[k], (uint64_t)k * 10000u) == 0);
    CHECK(lj_mtc_feed(&t, qf[7], 70000u) == 1 && t.fps == 25);
    CHECK(lj_mtc_time(&t, 80000u, &ms) == 0 && ms == 3723160u);              /* (1 h 2 m 3 s) + 4 frames at 25 fps = 3 723 160 ms */
    CHECK(lj_mtc_time(&t, 70000u + 500001u, &ms) == -1);                      /* time code stopped: past the 0.5 s time-out */
    CHECK(lj_mtc_feed(&t, 0x77, 80000u) == 0);                                /* piece 7 without a full set: nothing new */
    for (k = 0; k < 8; k++) lj_mtc_feed(&t, (uint8_t)(k == 2 ? 0x2F : qf[k]), 90000u);   /* seconds lo 15 + hi 0 = 15: a valid set */
    CHECK(lj_mtc_time(&t, 90000u, &ms) == 0 && ms == 3735160u);
    { uint8_t m[3] = { 0xF1, 0x04, 0 }; uint8_t got[3]; while (lj_midi_poll(got)) {} lj_midi_post(m); CHECK(lj_midi_poll(got) == 1 && got[0] == 0xF1 && got[1] == 0x04); }   /* F1 rides the queue to the UI thread */
    /* W6 INT-s7-SYNCFRAMES: [CUELIST2] SyncFrames=5 follows after the fifth frame (20 quarter frames at 25 fps = 10 ms each), not before; a gap past the time-out counts again */
    {   int q, first = -1;
        uint64_t now = 2000000u;
        lj_mtc_reset(&t); t.sync_frames = 5;
        for (q = 0; q < 24; q++) { lj_mtc_feed(&t, qf[q % 8], now += 10000u); if (first < 0 && lj_mtc_time(&t, now, &ms) == 0) first = q + 1; }
        CHECK(first == 20 && ms == 3723160u);                                   /* the set of quarter frames 9..16, shown from the 20th on */
        printf("  MTC SyncFrames=5: follows at quarter frame %d (frame %d)\n", first, first / 4);
        now += 600000u; first = -1;                                            /* the time code stops past 0.5 s and restarts */
        for (q = 0; q < 24; q++) { lj_mtc_feed(&t, qf[q % 8], now += 10000u); if (first < 0 && lj_mtc_time(&t, now, &ms) == 0) first = q + 1; }
        CHECK(first == 20);
        lj_mtc_reset(&t); first = -1;                                          /* Off: the first full set */
        for (q = 0; q < 8; q++) { lj_mtc_feed(&t, qf[q], now += 10000u); if (first < 0 && lj_mtc_time(&t, now, &ms) == 0) first = q + 1; }
        CHECK(first == 8); }
}

/* W4 da2532-fingers (appended): the LJ_FNF_EXT rows of the 2532 and Fingers surfaces through the map (fn = note, cc = level rows),
 * their flags and captions, and the ext Learn (a CC only when asked for, the bind enables the kind). */
static void lj_t_ext_rows(void)
{
    static const struct { enum lj_fn fn; int note; int flash; const char *cap; } rows[] = {
        { LJ_FN_DA2532_LATCH_1, 10, 1, "2532 Latch 1" }, { LJ_FN_DA2532_FLASH_8, 11, 1, "2532 Flash 8" }, { LJ_FN_DA2532_USER_18, 12, 1, "2532 User Key 18" },
        { LJ_FN_DA2532_PAGE_18, 13, 0, "2532 Page 18" }, { LJ_FN_DA2532_BANK_7, 14, 0, "2532 Bank 7" }, { LJ_FN_FINGERS_F_10, 15, 1, "Fingers F10" },
        { LJ_FN_FINGERS_SLOT_TOP_1, 16, 1, "Fingers top slot 1" }, { LJ_FN_FINGERS_SLOT_BOT_12, 17, 1, "Fingers bottom slot 12" },
        { LJ_FN_FINGERS_BUMP_12, 18, 1, "Fingers bump 12" }, { LJ_FN_FINGERS_BO, 19, 0, "Fingers B/O" } };
    static const struct { enum lj_fn fn; int cc; const char *cap; } levels[] = {
        { LJ_FN_FINGERS_FADER_1, 20, "Fingers fader 1" }, { LJ_FN_FINGERS_FADER_12, 21, "Fingers fader 12" }, { LJ_FN_FINGERS_GM, 22, "Fingers GM" }, { LJ_FN_FINGERS_JOG_5, 23, "Fingers jog wheel 5" } };
    static lj_midi_map m;
    lj_midi_map_entry e;
    char cap[64];
    int i, f, ch, note, cc;
    for (f = LJ_FN_DA2532_LATCH_1; f <= LJ_FN_FINGERS_JOG_5; f++) {          /* every appended row is an extension, named in the table */
        CHECK((lj_fn_table[f].flags & LJ_FNF_EXT) != 0);
        CHECK(lj_fn_by_name(lj_fn_table[f].name) == (enum lj_fn)f);
        lj_fn_bind((enum lj_fn)f, lj_t_handler, NULL);
    }
    f = LJ_FN_DA2532_BANK_7 - LJ_FN_DA2532_LATCH_1 + 1; i = LJ_FN_FINGERS_JOG_5 - LJ_FN_FINGERS_F_1 + 1;
    CHECK(f == 8 + 8 + 18 + 18 + 7 && i == 10 + 36 + 12 + 12 + 1 + 1 + 5);
    lj_midi_init(&m);
    m.enabled[LJ_MIDI_FN] = 1; m.enabled[LJ_MIDI_CC] = 1;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        CHECK(lj_fn_caption(rows[i].fn, cap, sizeof cap) == 0 && strcmp(cap, rows[i].cap) == 0);
        e = lj_t_entry(LJ_MIDI_FN, 0, rows[i].note, rows[i].fn, 0); CHECK(lj_midi_assign(&m, &e) == 0);
        lj_t_calls = 0;
        lj_t_send(&m, 0x90, rows[i].note, 0x40);
        CHECK(lj_t_calls == 1 && lj_t_fn == rows[i].fn && lj_t_edge == 1);
        lj_t_send(&m, 0x80, rows[i].note, 0);
        CHECK(lj_t_calls == 1 + rows[i].flash && (!rows[i].flash || lj_t_edge == 0));   /* a key-up reaches the flash rows only */
    }
    for (i = 0; i < (int)(sizeof levels / sizeof levels[0]); i++) {
        CHECK(lj_fn_caption(levels[i].fn, cap, sizeof cap) == 0 && strcmp(cap, levels[i].cap) == 0);
        e = lj_t_entry(LJ_MIDI_CC, 0, levels[i].cc, levels[i].fn, 0); CHECK(lj_midi_assign(&m, &e) == 0);
        lj_t_calls = 0;
        lj_t_send(&m, 0xB0, levels[i].cc, 127); CHECK(lj_t_calls == 1 && lj_t_fn == levels[i].fn && lj_t_num == 100);
        lj_t_send(&m, 0xB5, levels[i].cc, 0);   CHECK(lj_t_calls == 2 && lj_t_num == 0);
    }
    /* ext Learn: a CC only when asked for; lj_midi_learn_bind writes the entry and enables the kind */
    lj_midi_init(&m);
    lj_midi_learn_begin();                                                     /* the MIDI-IN window's Learn: notes only */
    lj_t_send(&m, 0xB0, 30, 5); CHECK(lj_midi_learn_take_ext(&ch, &note, &cc) == 0);
    lj_midi_learn_cancel();
    lj_midi_learn_begin_ext(0);
    lj_t_send(&m, 0xB0, 30, 5); CHECK(lj_midi_learn_take_ext(&ch, &note, &cc) == 0);
    lj_t_send(&m, 0x93, 31, 5); CHECK(lj_midi_learn_take_ext(&ch, &note, &cc) == 1 && ch == 4 && note == 31 && cc == 0);
    CHECK(lj_midi_learn_bind(&m, LJ_FN_DA2532_LATCH_2, ch, note, cc) == 0 && m.enabled[LJ_MIDI_FN] == 1 && m.n == 1);
    lj_midi_learn_begin_ext(1);
    lj_t_send(&m, 0xB1, 32, 99); CHECK(lj_midi_learn_take_ext(&ch, &note, &cc) == 1 && ch == 2 && note == 32 && cc == 1);
    CHECK(lj_midi_learn_bind(&m, LJ_FN_FINGERS_FADER_2, ch, note, cc) == 0 && m.enabled[LJ_MIDI_CC] == 1 && m.n == 2);
    CHECK(lj_midi_learn_bind(&m, LJ_FN_NONE, 0, 1, 0) == -1 && lj_midi_learn_bind(&m, LJ_FN_FINGERS_GM, 17, 1, 1) == -1 && lj_midi_learn_bind(&m, LJ_FN_FINGERS_GM, 0, 128, 1) == -1);
    lj_t_calls = 0;
    lj_t_send(&m, 0x93, 31, 5); CHECK(lj_t_calls == 1 && lj_t_fn == LJ_FN_DA2532_LATCH_2);
    lj_t_send(&m, 0xB1, 32, 64); CHECK(lj_t_calls == 2 && lj_t_fn == LJ_FN_FINGERS_FADER_2 && lj_t_num == 50);
    lj_fn_unbind_all();
}

int lj_test_midi_map(int argc, char **argv);
int lj_test_midi_map(int argc, char **argv)
{
    static lj_midi_map m;
    lj_midi_map_entry e; int i, ch, note; uint8_t msg[3]; uint32_t d0;
    (void)argc; (void)argv;
    lj_midi_init(&m);
    CHECK(m.note0_is_off == 1 && m.release_on_off[LJ_MIDI_SEQ] == 1 && m.release_on_off[LJ_MIDI_CUE] == 0 && m.on_top == 1 && m.auto_save == 1 && m.device == -1);
    for (i = 0; i < LJ_MIDI_KIND_COUNT; i++) CHECK(m.enabled[i] == 0 && m.channel[i] == 16);
    CHECK(lj_midi_validate(&m) == NULL);
    CHECK(lj_midi_list(NULL, 0) >= 0);                        /* any backend: a count, no error */
    CHECK(strcmp(lj_midi_kind_words[LJ_MIDI_CC], "cc") == 0 && lj_midi_kind_words[LJ_MIDI_KIND_COUNT] == NULL);
    CHECK(lj_fn_table[LJ_FN_MASTER_LEVEL].flags == LJ_FNF_EXT);

    memset(&e, 0, sizeof e); e.kind = LJ_MIDI_CUE; e.ch = LJ_MIDI_CH_ALL; e.note = 60; e.num = 12;
    CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 1);
    e.ch = 1; e.num = 13; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 2);
    e.num = 14; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 2 && m.e[1].num == 14);   /* same kind/ch/note replaces */
    e.kind = LJ_MIDI_FN; e.fn = LJ_FN_BLACKOUT_RESTORE; e.ch = 2; e.note = 61; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 3);
    e.fn = LJ_FN_NONE; CHECK(lj_midi_assign(&m, &e) == -1);
    e.kind = LJ_MIDI_SEQ; e.fn = 0; e.num = 0; CHECK(lj_midi_assign(&m, &e) == -1);
    e.kind = LJ_MIDI_CUE; e.num = 1; e.note = 128; CHECK(lj_midi_assign(&m, &e) == -1);
    CHECK(lj_midi_match(&m, 0, 1, 60)->num == 14);            /* exact channel first */
    CHECK(lj_midi_match(&m, 0, 5, 60)->num == 12);            /* then All */
    CHECK(lj_midi_match(&m, 1 << LJ_MIDI_FN, 1, 60) == NULL); /* kind mask */
    CHECK(lj_midi_match(&m, 0, 1, 62) == NULL);

    lj_fn_bind(LJ_FN_PROG_CUE, lj_t_handler, NULL); lj_fn_bind(LJ_FN_BLACKOUT_RESTORE, lj_t_handler, NULL); lj_fn_bind(LJ_FN_PROG_SEQ_FLASH, lj_t_handler, NULL);
    lj_fn_bind(LJ_FN_NEW_CLEAR_CUE, lj_t_handler, NULL); lj_fn_bind(LJ_FN_MASTER_LEVEL, lj_t_handler, NULL);
    lj_t_calls = 0;
    lj_t_send(&m, 0x90, 60, 100);                             /* every kind disabled: nothing fires */
    CHECK(lj_t_calls == 0);
    m.enabled[LJ_MIDI_CUE] = 1;
    lj_t_send(&m, 0x90, 60, 100);                             /* channel 1 note 60 on -> cue 14 */
    CHECK(lj_t_calls == 1 && lj_t_fn == LJ_FN_PROG_CUE && lj_t_num == 14 && lj_t_edge == 1);
    lj_t_send(&m, 0x90, 60, 0);                               /* velocity 0 = note off; LatchOff 0: a latched cue ignores it */
    CHECK(lj_t_calls == 1);
    lj_t_send(&m, 0x80, 60, 64);                              /* a real note off: ignored too */
    CHECK(lj_t_calls == 1);
    m.release_on_off[LJ_MIDI_CUE] = 1;
    lj_t_send(&m, 0x80, 60, 0);                               /* LatchOff: the note off of the last cue note on clears the cue (New/Clear Cue) */
    CHECK(lj_t_calls == 2 && lj_t_fn == LJ_FN_NEW_CLEAR_CUE && lj_t_edge == 1);
    lj_t_send(&m, 0x80, 60, 0);                               /* once: nothing latched any more */
    CHECK(lj_t_calls == 2);
    m.release_on_off[LJ_MIDI_CUE] = 0;
    m.channel[LJ_MIDI_CUE] = 3;                               /* channel gate: only MIDI channel 3 */
    lj_t_send(&m, 0x90, 60, 100);
    CHECK(lj_t_calls == 2);
    lj_t_send(&m, 0x92, 60, 100);                             /* channel 3 note 60 -> the All entry (cue 12) */
    CHECK(lj_t_calls == 3 && lj_t_num == 12);
    m.enabled[LJ_MIDI_FN] = 1;
    lj_t_send(&m, 0x91, 61, 1);                               /* channel 2 note 61 -> the function, press */
    CHECK(lj_t_calls == 4 && lj_t_fn == LJ_FN_BLACKOUT_RESTORE && lj_t_edge == 1);
    lj_t_send(&m, 0xB1, 61, 1);                               /* CC with the extension off: ignored */
    CHECK(lj_t_calls == 4);

    /* the brief's row: note on 90 3C 7F through inject -> latch cue 1 */
    m.channel[LJ_MIDI_CUE] = 16;
    e = lj_t_entry(LJ_MIDI_CUE, LJ_MIDI_CH_ALL, 0x3C, LJ_FN_NONE, 1); CHECK(lj_midi_assign(&m, &e) == 0);
    CHECK(lj_midi_unassign(&m, LJ_MIDI_CUE, 1, 60) == 0);    /* the channel-1 entry would win otherwise */
    lj_t_send(&m, 0x90, 0x3C, 0x7F);
    CHECK(lj_t_calls == 5 && lj_t_fn == LJ_FN_PROG_CUE && lj_t_num == 1 && lj_t_edge == 1);
    lj_t_send(&m, 0x80, 0x3C, 0x40);                          /* note off: the latch holds */
    CHECK(lj_t_calls == 5);

    /* flash: the note off releases (Release Seq on Note Off, FlashOff=1 shipped), only for the last flash note */
    m.enabled[LJ_MIDI_SEQ] = 1;
    e = lj_t_entry(LJ_MIDI_SEQ, LJ_MIDI_CH_ALL, 40, LJ_FN_NONE, 7); CHECK(lj_midi_assign(&m, &e) == 0);
    e = lj_t_entry(LJ_MIDI_SEQ, LJ_MIDI_CH_ALL, 41, LJ_FN_NONE, 8); CHECK(lj_midi_assign(&m, &e) == 0);
    lj_t_send(&m, 0x90, 40, 90);
    CHECK(lj_t_calls == 6 && lj_t_fn == LJ_FN_PROG_SEQ_FLASH && lj_t_num == 7 && lj_t_edge == 1);
    lj_t_send(&m, 0x90, 40, 0);                               /* velocity 0 = note off -> release */
    CHECK(lj_t_calls == 7 && lj_t_fn == LJ_FN_PROG_SEQ_FLASH && lj_t_num == 7 && lj_t_edge == 0);
    lj_t_send(&m, 0x90, 40, 90); lj_t_send(&m, 0x90, 41, 90); /* 41 is now the flashed one */
    lj_t_send(&m, 0x80, 40, 0);                               /* not the last note on: ignored */
    CHECK(lj_t_calls == 9 && lj_t_num == 8 && lj_t_edge == 1);
    lj_t_send(&m, 0x80, 41, 0);
    CHECK(lj_t_calls == 10 && lj_t_num == 8 && lj_t_edge == 0);
    m.note0_is_off = 0;                                       /* Note0 off: velocity 0 is a note on */
    lj_t_send(&m, 0x90, 40, 0);
    CHECK(lj_t_calls == 11 && lj_t_edge == 1);
    m.note0_is_off = 1; lj_t_send(&m, 0x80, 40, 0);
    CHECK(lj_t_calls == 12 && lj_t_edge == 0);

    /* CC 7 -> the Grand Master, 0..127 -> 0..100 % (the marked extension) */
    e = lj_t_entry(LJ_MIDI_CC, LJ_MIDI_CH_ALL, 7, LJ_FN_MASTER_LEVEL, 0); CHECK(lj_midi_assign(&m, &e) == 0);
    lj_t_send(&m, 0xB0, 7, 127);
    CHECK(lj_t_calls == 12);                                  /* kind cc disabled */
    m.enabled[LJ_MIDI_CC] = 1;
    lj_t_send(&m, 0xB0, 7, 127); CHECK(lj_t_calls == 13 && lj_t_fn == LJ_FN_MASTER_LEVEL && lj_t_num == 100 && lj_t_edge == 1);
    lj_t_send(&m, 0xB4, 7, 0);   CHECK(lj_t_calls == 14 && lj_t_num == 0);
    lj_t_send(&m, 0xB0, 7, 64);  CHECK(lj_t_calls == 15 && lj_t_num == 50);
    lj_t_send(&m, 0xB0, 7, 1);   CHECK(lj_t_calls == 16 && lj_t_num == 1);
    lj_t_send(&m, 0xB0, 8, 64);  CHECK(lj_t_calls == 16);    /* another controller */
    lj_t_send(&m, 0x90, 7, 64);  CHECK(lj_t_calls == 16);    /* note 7 is not CC 7 */

    /* Learn binds the next note on only */
    lj_midi_learn_begin();
    CHECK(lj_midi_learn_take(&ch, &note) == 0);
    lj_t_send(&m, 0x80, 10, 0);                               /* a note off does not learn */
    CHECK(lj_midi_learn_take(&ch, &note) == 0);
    lj_t_send(&m, 0x95, 70, 90);                              /* the next note on: learned, not dispatched */
    lj_t_send(&m, 0x90, 0x3C, 0x7F);                          /* the one after it dispatches and does not overwrite */
    CHECK(lj_t_calls == 17 && lj_t_num == 1);
    CHECK(lj_midi_learn_take(&ch, &note) == 1 && ch == 6 && note == 70);
    CHECK(lj_midi_learn_take(&ch, &note) == 0);               /* consumed once */
    lj_midi_learn_begin(); lj_midi_learn_cancel();
    lj_t_send(&m, 0x90, 0x3C, 0x7F);
    CHECK(lj_t_calls == 18 && lj_midi_learn_take(&ch, &note) == 0);

    /* malformed: status < 0x80 or a data byte > 0x7F is dropped and counted, never dispatched */
    d0 = lj_midi_dropped();
    lj_t_send(&m, 0x3C, 0x3C, 0x7F); lj_t_send(&m, 0x90, 0xBC, 0x7F); lj_t_send(&m, 0x90, 0x3C, 0xFF);
    CHECK(lj_t_calls == 18 && lj_midi_dropped() == d0 + 3);
    msg[0] = 0x90; msg[1] = 0x3C; msg[2] = 0x80;
    CHECK(lj_midi_dispatch(&m, msg) == NULL && lj_t_calls == 18);   /* dispatch refuses it too (the public entry) */
    lj_t_send(&m, 0xF8, 0, 0); lj_t_send(&m, 0xFE, 0, 0);    /* clock / active sensing: ignored, not counted */
    CHECK(lj_t_calls == 18 && lj_midi_dropped() == d0 + 3);

    msg[0] = 0x90; msg[1] = 1; msg[2] = 1;
    for (i = 0; i < LJ_MIDI_QUEUE + 5; i++) lj_midi_inject(msg);   /* a full queue drops and counts */
    CHECK(lj_midi_dropped() == d0 + 8);
    for (i = 0; lj_midi_poll(msg); i++) { }
    CHECK(i == LJ_MIDI_QUEUE);

    CHECK(lj_midi_unassign(&m, LJ_MIDI_CUE, 0, 60) == 0 && lj_midi_unassign(&m, LJ_MIDI_CUE, 0, 60) == -1);
    lj_fn_unbind_all();
    lj_t_files();
    lj_t_feed_mtc();
    lj_t_ext_rows();   /* W4 da2532-fingers */
    printf("test_midi_map: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
