/* lj_midi.c - the OS-independent half of lj_midi.h: the queue the OS thread posts
 * into, the map, Learn and the UI-thread dispatch. Load/save are W2 midi-track
 * bodies (stubs here). ponytail: one process-wide queue, like the one UI thread
 * that drains it. */
#include <string.h>
#include "io/lj_midi.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "io/lj_fn.h"

const char *const lj_midi_kind_words[] = { "seq", "cue", "bgcue", "cl", "fn", NULL };

static struct { uint8_t msg[LJ_MIDI_QUEUE][3]; int head, n; uint32_t dropped; int learning, learned, learn_ch, learn_note; } lj_mq;

void lj_midi_init(lj_midi_map *m)
{
    int k;
    memset(m, 0, sizeof *m);
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) m->channel[k] = 16;   /* shipped MIDIIn.Ini: every channel 16 (= All), 05 s7 */
    m->note0_is_off = 1; m->release_on_off[LJ_MIDI_SEQ] = 1;       /* Note0=1 FlashOff=1 SeqOff=1 */
    m->on_top = 1; m->auto_save = 1; m->device = -1;
}

int  lj_midi_load(const char *ini_path, const char *map_path, lj_midi_map *out, char *err, size_t cap) { (void)ini_path; (void)map_path; (void)err; (void)cap; lj_midi_init(out); return -1; }   /* W2 midi track */
int  lj_midi_save(const char *ini_path, const char *map_path, const lj_midi_map *in) { (void)ini_path; (void)map_path; (void)in; return -1; }   /* W2 midi track */

const char *lj_midi_validate(const lj_midi_map *in)
{
    int i, k;
    if (!in || in->n < 0 || in->n > LJ_MIDI_MAP_MAX) return "map count";
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) if (in->channel[k] < 1 || in->channel[k] > 16) return "channel 1..16";
    for (i = 0; i < in->n; i++) {
        const lj_midi_map_entry *e = &in->e[i];
        if (e->kind >= LJ_MIDI_KIND_COUNT) return "kind";
        if (e->ch > 16 || e->note > 127) return "ch 0..16 / note 0..127";
        if (e->kind == LJ_MIDI_FN ? (e->fn == LJ_FN_NONE || e->fn >= LJ_FN_COUNT) : e->num == 0) return "fn / num";
    }
    return NULL;
}

static int lj_midi_find(const lj_midi_map *m, int kind, int ch, int note)
{
    int i;
    for (i = 0; i < m->n; i++) if (m->e[i].kind == kind && m->e[i].ch == ch && m->e[i].note == note) return i;
    return -1;
}

int lj_midi_assign(lj_midi_map *m, const lj_midi_map_entry *e)
{
    int i;
    if (!m || !e || e->kind >= LJ_MIDI_KIND_COUNT || e->ch > 16 || e->note > 127) return -1;
    if (e->kind == LJ_MIDI_FN ? (e->fn == LJ_FN_NONE || e->fn >= LJ_FN_COUNT) : e->num == 0) return -1;
    i = lj_midi_find(m, e->kind, e->ch, e->note);
    if (i < 0) { if (m->n >= LJ_MIDI_MAP_MAX) return -1; i = m->n++; }
    m->e[i] = *e;
    return 0;
}

int lj_midi_unassign(lj_midi_map *m, enum lj_midi_kind kind, int ch, int note)
{
    int i = m ? lj_midi_find(m, (int)kind, ch, note) : -1;
    if (i < 0) return -1;
    memmove(&m->e[i], &m->e[i + 1], (size_t)(m->n - i - 1) * sizeof m->e[0]);
    m->n--;
    return 0;
}

const lj_midi_map_entry *lj_midi_match(const lj_midi_map *m, int kind_mask, int ch, int note)
{
    int i, pass;
    if (!m) return NULL;
    for (pass = 0; pass < 2; pass++)                       /* exact channel first, then All */
        for (i = 0; i < m->n; i++) {
            const lj_midi_map_entry *e = &m->e[i];
            if (kind_mask && !(kind_mask & (1 << e->kind))) continue;
            if (e->note == note && e->ch == (pass == 0 ? ch : LJ_MIDI_CH_ALL)) return e;
        }
    return NULL;
}

void lj_midi_post(const uint8_t msg[3])
{
    lj_lock();
    if (lj_mq.n >= LJ_MIDI_QUEUE) lj_mq.dropped++;
    else { memcpy(lj_mq.msg[(lj_mq.head + lj_mq.n) % LJ_MIDI_QUEUE], msg, 3); lj_mq.n++; }
    lj_unlock();
}

int lj_midi_poll(uint8_t msg[3])
{
    int got = 0;
    lj_lock();
    if (lj_mq.n > 0) { memcpy(msg, lj_mq.msg[lj_mq.head], 3); lj_mq.head = (lj_mq.head + 1) % LJ_MIDI_QUEUE; lj_mq.n--; got = 1; }
    lj_unlock();
    return got;
}

uint32_t lj_midi_dropped(void) { uint32_t d; lj_lock(); d = lj_mq.dropped; lj_unlock(); return d; }
void lj_midi_inject(const uint8_t msg[3]) { lj_midi_post(msg); }

void lj_midi_learn_begin(void)  { lj_mq.learning = 1; lj_mq.learned = 0; }
void lj_midi_learn_cancel(void) { lj_mq.learning = 0; lj_mq.learned = 0; }
int  lj_midi_learn_take(int *ch, int *note)
{
    if (!lj_mq.learned) return 0;
    if (ch) *ch = lj_mq.learn_ch;
    if (note) *note = lj_mq.learn_note;
    lj_mq.learned = 0; lj_mq.learning = 0;
    return 1;
}

const lj_midi_map_entry *lj_midi_dispatch(const lj_midi_map *m, const uint8_t msg[3])
{
    int status = msg[0] & 0xF0, ch = (msg[0] & 0x0F) + 1, note = msg[1] & 0x7F, on, kind_mask = 0, k;
    const lj_midi_map_entry *e;
    if (status != 0x90 && status != 0x80) return NULL;           /* notes only (05 s7); CC / program change are the marked extensions, off */
    on = status == 0x90 && !(m->note0_is_off && msg[2] == 0);
    if (lj_mq.learning) { if (on) { lj_mq.learn_ch = ch; lj_mq.learn_note = note; lj_mq.learned = 1; } return NULL; }
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++)
        if (m->enabled[k] && (m->channel[k] == 16 || m->channel[k] == ch)) kind_mask |= 1 << k;   /* channel 16 = All as LJ2 ships it */
    if (!kind_mask) return NULL;
    e = lj_midi_match(m, kind_mask, ch, note);
    if (!e) return NULL;
    if (!on && e->kind != LJ_MIDI_FN && !m->release_on_off[e->kind]) return e;   /* latched: note off ignored */
    switch (e->kind) {
    case LJ_MIDI_SEQ:   lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, e->num, on); break;
    case LJ_MIDI_CUE:   lj_fn_run_prog(LJ_FN_PROG_CUE, e->num, on); break;
    case LJ_MIDI_BGCUE: lj_fn_run_prog(LJ_FN_PROG_BGCUE, e->num, on); break;
    case LJ_MIDI_CL:    lj_fn_run_prog(LJ_FN_PROG_CUELIST, e->num, on); break;
    default:            lj_fn_run((enum lj_fn)e->fn, on); break;
    }
    return e;
}
