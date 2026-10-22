/* test_midi_map.c - `lj4evr_tests test_midi_map`: io/lj_midi.h without a device: LJ2's shipped
 * defaults (lj_midi_init), assign / unassign / match (exact channel before All), the queue the OS
 * thread posts into (lj_midi_inject = the UI script's `midi` verb) and the UI-thread dispatch:
 * note on -> lj_fn_run_prog(LJ_FN_PROG_CUE, num, 1), velocity 0 = note off (Note0), release on
 * note off per kind, channel gates, Learn, drops counted when the queue is full. Load/save of
 * MIDIIn.Ini / MIDIMap.dat are the midi track's rows (stubs return -1 today). */
#include <stdio.h>
#include <string.h>
#include "io/lj_fn.h"
#include "io/lj_midi.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_t_calls, lj_t_edge; static enum lj_fn lj_t_fn; static uint32_t lj_t_num;
static void lj_t_handler(enum lj_fn fn, int edge, uint32_t num, void *ud) { (void)ud; lj_t_calls++; lj_t_fn = fn; lj_t_edge = edge; lj_t_num = num; }

static void lj_t_drain(const lj_midi_map *m) { uint8_t msg[3]; while (lj_midi_poll(msg)) lj_midi_dispatch(m, msg); }
static void lj_t_send(const lj_midi_map *m, int b0, int b1, int b2) { uint8_t msg[3]; msg[0] = (uint8_t)b0; msg[1] = (uint8_t)b1; msg[2] = (uint8_t)b2; lj_midi_inject(msg); lj_t_drain(m); }

int lj_test_midi_map(int argc, char **argv);
int lj_test_midi_map(int argc, char **argv)
{
    lj_midi_map m; lj_midi_map_entry e; int i, ch, note; uint8_t msg[3];
    (void)argc; (void)argv;
    lj_midi_init(&m);
    CHECK(m.note0_is_off == 1 && m.release_on_off[LJ_MIDI_SEQ] == 1 && m.release_on_off[LJ_MIDI_CUE] == 0 && m.on_top == 1 && m.auto_save == 1 && m.device == -1);
    for (i = 0; i < LJ_MIDI_KIND_COUNT; i++) CHECK(m.enabled[i] == 0 && m.channel[i] == 16);
    CHECK(lj_midi_validate(&m) == NULL);
    CHECK(lj_midi_list(NULL, 0) >= 0);                        /* the null backend: no devices, no error */

    memset(&e, 0, sizeof e); e.kind = LJ_MIDI_CUE; e.ch = LJ_MIDI_CH_ALL; e.note = 60; e.num = 12;
    CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 1);
    e.ch = 1; e.num = 13; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 2);
    e.num = 14; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 2 && m.e[1].num == 14);   /* same kind/ch/note replaces */
    e.kind = LJ_MIDI_FN; e.fn = LJ_FN_BLACKOUT_RESTORE; e.ch = 2; e.note = 61; CHECK(lj_midi_assign(&m, &e) == 0 && m.n == 3);
    e.fn = LJ_FN_NONE; CHECK(lj_midi_assign(&m, &e) == -1);
    e.kind = LJ_MIDI_SEQ; e.fn = 0; e.num = 0; CHECK(lj_midi_assign(&m, &e) == -1);
    CHECK(lj_midi_match(&m, 0, 1, 60)->num == 14);            /* exact channel first */
    CHECK(lj_midi_match(&m, 0, 5, 60)->num == 12);            /* then All */
    CHECK(lj_midi_match(&m, 1 << LJ_MIDI_FN, 1, 60) == NULL); /* kind mask */
    CHECK(lj_midi_match(&m, 0, 1, 62) == NULL);

    lj_fn_bind(LJ_FN_PROG_CUE, lj_t_handler, NULL); lj_fn_bind(LJ_FN_BLACKOUT_RESTORE, lj_t_handler, NULL); lj_fn_bind(LJ_FN_PROG_SEQ_FLASH, lj_t_handler, NULL);
    lj_t_calls = 0;
    lj_t_send(&m, 0x90, 60, 100);                             /* every kind disabled: nothing fires */
    CHECK(lj_t_calls == 0);
    m.enabled[LJ_MIDI_CUE] = 1;
    lj_t_send(&m, 0x90, 60, 100);                             /* channel 1 note 60 on -> cue 14 */
    CHECK(lj_t_calls == 1 && lj_t_fn == LJ_FN_PROG_CUE && lj_t_num == 14 && lj_t_edge == 1);
    lj_t_send(&m, 0x90, 60, 0);                               /* velocity 0 = note off; LatchOff 0: a latched cue ignores it */
    CHECK(lj_t_calls == 1);
    m.release_on_off[LJ_MIDI_CUE] = 1;
    lj_t_send(&m, 0x80, 60, 0);                               /* note off with LatchOff: dispatch hands edge 0 on, but lj_fn_run_prog drops a
                                                                 non-flash release (the W1 test_hotkeys row); the LatchOff release path is the
                                                                 W2 midi + app tracks' (pending, contracts finish 2026-10-22) */
    CHECK(lj_t_calls == 1);
    m.channel[LJ_MIDI_CUE] = 3;                               /* channel gate: only MIDI channel 3 */
    lj_t_send(&m, 0x90, 60, 100);
    CHECK(lj_t_calls == 1);
    lj_t_send(&m, 0x92, 60, 100);                             /* channel 3 note 60 -> the All entry (cue 12) */
    CHECK(lj_t_calls == 2 && lj_t_num == 12);
    m.enabled[LJ_MIDI_FN] = 1;
    lj_t_send(&m, 0x91, 61, 1);                               /* channel 2 note 61 -> the function, press */
    CHECK(lj_t_calls == 3 && lj_t_fn == LJ_FN_BLACKOUT_RESTORE && lj_t_edge == 1);
    lj_t_send(&m, 0xB1, 61, 1);                               /* CC: ignored (extension off) */
    CHECK(lj_t_calls == 3);

    lj_midi_learn_begin();                                    /* Learn eats the next note on */
    CHECK(lj_midi_learn_take(&ch, &note) == 0);
    lj_t_send(&m, 0x80, 10, 0);                               /* a note off does not learn */
    CHECK(lj_midi_learn_take(&ch, &note) == 0);
    lj_t_send(&m, 0x95, 70, 90);
    CHECK(lj_midi_learn_take(&ch, &note) == 1 && ch == 6 && note == 70 && lj_t_calls == 3);
    CHECK(lj_midi_learn_take(&ch, &note) == 0);               /* consumed once */

    msg[0] = 0x90; msg[1] = 1; msg[2] = 1;
    for (i = 0; i < LJ_MIDI_QUEUE + 5; i++) lj_midi_inject(msg);   /* a full queue drops and counts */
    CHECK(lj_midi_dropped() == 5);
    for (i = 0; lj_midi_poll(msg); i++) { }
    CHECK(i == LJ_MIDI_QUEUE);

    CHECK(lj_midi_unassign(&m, LJ_MIDI_CUE, 1, 60) == 0 && m.n == 2 && lj_midi_unassign(&m, LJ_MIDI_CUE, 1, 60) == -1);
    lj_fn_unbind_all();
    printf("test_midi_map: %d failure(s) (MIDIIn.Ini / MIDIMap.dat round trip: W2 midi track)\n", fails);
    return fails ? 1 : 0;
}
