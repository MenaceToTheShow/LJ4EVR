/* lj_midi.c - the OS-independent half of lj_midi.h: the queue the OS thread posts
 * into, the map, Learn, the UI-thread dispatch and the two files (MIDIIn.Ini in
 * LJ2's keys, MIDIMap.dat in our grammar). ponytail: one process-wide queue, like
 * the one UI thread that drains it. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as io/lj_hotkeys.c) */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "io/lj_midi.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_textrec.h"
#include "io/lj_fn.h"
#include "model/lj_kv.h"

const char *const lj_midi_kind_words[] = { "seq", "cue", "bgcue", "cl", "fn", "cc", NULL };

/* the queue (OS thread -> UI thread, under lj_lock) and the UI-thread state (Learn, the last note per kind) */
static struct { uint8_t msg[LJ_MIDI_QUEUE][3]; int head, n; uint32_t dropped; } lj_mq;
static struct { int learning, learned, ch, note; int last_ch[LJ_MIDI_KIND_COUNT], last_note[LJ_MIDI_KIND_COUNT]; } lj_mu;

void lj_midi_init(lj_midi_map *m)
{
    int k;
    memset(m, 0, sizeof *m);
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) m->channel[k] = 16;   /* shipped MIDIIn.Ini: every channel 16 (= All), 05 s7 */
    m->note0_is_off = 1; m->release_on_off[LJ_MIDI_SEQ] = 1;       /* Note0=1 FlashOff=1 SeqOff=1 */
    m->on_top = 1; m->auto_save = 1; m->device = -1;
}

static int lj_midi_entry_bad(const lj_midi_map_entry *e)
{
    if (e->kind >= LJ_MIDI_KIND_COUNT || e->ch > 16 || e->note > 127) return 1;
    if (e->kind == LJ_MIDI_FN || e->kind == LJ_MIDI_CC) return e->fn == LJ_FN_NONE || e->fn >= LJ_FN_COUNT;
    return e->num == 0;
}

const char *lj_midi_validate(const lj_midi_map *in)
{
    int i, k;
    if (!in || in->n < 0 || in->n > LJ_MIDI_MAP_MAX) return "map count";
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) {
        if (in->channel[k] < 1 || in->channel[k] > 16) return "channel 1..16";
        if (in->enabled[k] > 1 || in->release_on_off[k] > 1) return "enable / release flags 0 or 1";
    }
    for (i = 0; i < in->n; i++) if (lj_midi_entry_bad(&in->e[i])) return "entry: kind / ch 0..16 / note 0..127 / fn / num";
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
    if (!m || !e || lj_midi_entry_bad(e)) return -1;
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

/* ---- the queue ---- */

void lj_midi_post(const uint8_t msg[3])
{
    if (msg[0] >= 0xF0) return;                                   /* system: clock, active sensing (no MIDI clock, 05 s7.2); MTC 0xF1 is W3 */
    lj_lock();
    if (msg[0] < 0x80 || msg[1] > 0x7F || msg[2] > 0x7F || lj_mq.n >= LJ_MIDI_QUEUE) lj_mq.dropped++;   /* malformed or full: dropped, never dispatched */
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

/* ---- Learn: the next note on (after begin) is reported once; the notes after it dispatch normally ---- */

void lj_midi_learn_begin(void)  { lj_mu.learning = 1; lj_mu.learned = 0; }
void lj_midi_learn_cancel(void) { lj_mu.learning = 0; lj_mu.learned = 0; }
int  lj_midi_learn_take(int *ch, int *note)
{
    if (!lj_mu.learned) return 0;
    if (ch) *ch = lj_mu.ch;
    if (note) *note = lj_mu.note;
    lj_mu.learned = 0;
    return 1;
}

/* ---- dispatch (UI thread) ---- */

const lj_midi_map_entry *lj_midi_dispatch(const lj_midi_map *m, const uint8_t msg[3])
{
    static const enum lj_fn release_fn[LJ_MIDI_KIND_COUNT] = { LJ_FN_PROG_SEQ_FLASH, LJ_FN_NEW_CLEAR_CUE, LJ_FN_CLEAR_BGCUE, LJ_FN_CUELIST_STOP_AND_CLEAR, LJ_FN_NONE, LJ_FN_NONE };
    static const enum lj_fn prog_fn[4] = { LJ_FN_PROG_SEQ_FLASH, LJ_FN_PROG_CUE, LJ_FN_PROG_BGCUE, LJ_FN_PROG_CUELIST };
    int status, ch, note, on, kind_mask = 0, k;
    const lj_midi_map_entry *e;
    if (!m || !msg || msg[0] < 0x80 || msg[0] >= 0xF0 || msg[1] > 0x7F || msg[2] > 0x7F) return NULL;   /* the post rule again: dispatch is public */
    status = msg[0] & 0xF0; ch = (msg[0] & 0x0F) + 1; note = msg[1];
    if (status != 0x90 && status != 0x80 && status != 0xB0) return NULL;   /* notes (05 s7) and the CC extension; program change / pressure / bend ignored */
    on = status == 0x90 && !(m->note0_is_off && msg[2] == 0);
    if (status != 0xB0 && lj_mu.learning) {                       /* Learn takes the next note on, nothing else fires meanwhile */
        if (on) { lj_mu.ch = ch; lj_mu.note = note; lj_mu.learned = 1; lj_mu.learning = 0; }
        return NULL;
    }
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++)
        if (m->enabled[k] && (m->channel[k] == 16 || m->channel[k] == ch) && ((k == LJ_MIDI_CC) == (status == 0xB0))) kind_mask |= 1 << k;   /* channel 16 = All as LJ2 ships it */
    if (!kind_mask) return NULL;
    e = lj_midi_match(m, kind_mask, ch, note);
    if (!e) return NULL;
    if (e->kind == LJ_MIDI_CC) { lj_fn_run_prog((enum lj_fn)e->fn, (uint32_t)((msg[2] * 100 + 63) / 127), 1); return e; }   /* 0..127 -> 0..100 %, rounded */
    if (e->kind == LJ_MIDI_FN) { lj_fn_run((enum lj_fn)e->fn, on); return e; }   /* functions: press / release edges (flash functions) */
    if (on) {
        lj_mu.last_ch[e->kind] = ch; lj_mu.last_note[e->kind] = note + 1;   /* +1: 0 = nothing latched */
        lj_fn_run_prog(prog_fn[e->kind], e->num, 1);
        return e;
    }
    if (!m->release_on_off[e->kind] || lj_mu.last_ch[e->kind] != ch || lj_mu.last_note[e->kind] != note + 1) return e;   /* latched, or not the last note on: ignored */
    lj_mu.last_note[e->kind] = 0;
    if (e->kind == LJ_MIDI_SEQ) lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, e->num, 0);   /* Release Seq on Note Off: the flash ends */
    else lj_fn_run(release_fn[e->kind], 1);                                        /* Release Cue / BGCue / Cuelist: clear it (05 s7, CHM midiin.htm) */
    return e;
}

/* ---- files ---- */

static int lj_midi_exists(const char *path) { FILE *f = fopen(path, "rb"); if (!f) return 0; fclose(f); return 1; }

static const struct { const char *sec, *en, *ch; } lj_midi_keys[LJ_MIDI_KIND_COUNT] = {
    { "SEQ", "SeqEnable", "SeqChannel" }, { "CUE", "CueEnable", "CueChannel" }, { "BGCUE", "BGCueEnable", "BGCueChannel" },
    { "CUELIST", "CueListEnable", "CueListChannel" }, { "FUNCTIONS", "FunctionsEnable", "FunctionsChannel" },
    { "CC", "CCEnable", "CCChannel" } };   /* [CC]: ours, the extension */
static const char *const lj_midi_release_keys[LJ_MIDI_KIND_COUNT] = { "FlashOff", "LatchOff", "BGLatchOff", "ListOff", NULL, NULL };

/* an integer key inside lo..hi: absent = dflt; present and out of range or not a number = *bad (ME-77) */
static int lj_midi_ini_int(lj_ini *ini, const char *sec, const char *key, int dflt, int lo, int hi, const char **bad)
{
    int v;
    if (!lj_ini_gets(ini, sec, key, NULL)) return dflt;
    v = lj_ini_geti(ini, sec, key, INT_MIN);
    if (v < lo || v > hi) { if (!*bad) *bad = key; return dflt; }
    return v;
}

static int lj_midi_load_map(const char *path, lj_midi_map *out, char *err, size_t cap)
{
    static char val[LJ_KV_VAL_CAP];   /* UI thread only (the file loaders' pattern) */
    char key[32], fname[64];
    int fmt, r, item = 0, bad;
    lj_rec *rec = lj_rec_open_read(path, "midimap", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 midimap file"); return -1; }
    while ((r = lj_rec_next(rec, key, sizeof key, val, sizeof val)) == 1) {
        lj_midi_map_entry e;
        item++;
        if (strcmp(key, "map") != 0) continue;                   /* unknown keys skipped (design 5.2) */
        memset(&e, 0, sizeof e); bad = 0;
        r = lj_kv_word(val, "kind", lj_midi_kind_words, -1);
        e.kind = (uint8_t)(r < 0 ? LJ_MIDI_KIND_COUNT : r);
        e.ch = (uint8_t)lj_kv_range(val, "ch", 0, 0, 16, &bad);
        e.note = (uint8_t)lj_kv_range(val, "note", 255, 0, 127, &bad);   /* absent = 255 = refused below */
        e.num = (uint32_t)lj_kv_range(val, "num", 0, 0, 0x7FFFFFFFL, &bad);
        if (lj_kv_get(val, "fn", fname, sizeof fname) == 1) e.fn = (uint16_t)lj_fn_by_name(fname);
        if (bad || lj_midi_entry_bad(&e)) { r = -1; break; }
        if (lj_midi_assign(out, &e) != 0) { r = -1; break; }     /* full (a duplicate kind/ch/note replaces, the last line wins) */
    }
    lj_rec_close(rec);
    if (r < 0) { lj_kv_err(err, cap, path, item, "bad map line (kind seq|cue|bgcue|cl|fn|cc, ch 0..16, note 0..127, num / fn)"); return -1; }
    return 0;
}

int lj_midi_load(const char *ini_path, const char *map_path, lj_midi_map *out, char *err, size_t cap)
{
    const char *bad = NULL;
    lj_ini *ini;
    int k;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    lj_midi_init(out);
    if (ini_path && lj_midi_exists(ini_path) && (ini = lj_ini_load(ini_path)) != NULL) {   /* missing = LJ2's shipped defaults, silently */
        out->device = lj_midi_ini_int(ini, "SETUP", "DeviceCode", out->device, -1, 255, &bad);
        out->note0_is_off = (uint8_t)lj_midi_ini_int(ini, "SETUP", "Note0", out->note0_is_off, 0, 1, &bad);
        for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) {
            out->enabled[k] = (uint8_t)lj_midi_ini_int(ini, lj_midi_keys[k].sec, lj_midi_keys[k].en, 0, 0, 1, &bad);
            out->channel[k] = (uint8_t)lj_midi_ini_int(ini, lj_midi_keys[k].sec, lj_midi_keys[k].ch, 16, 1, 16, &bad);
            if (lj_midi_release_keys[k]) out->release_on_off[k] = (uint8_t)lj_midi_ini_int(ini, "SETUP", lj_midi_release_keys[k], out->release_on_off[k], 0, 1, &bad);
        }
        (void)lj_midi_ini_int(ini, "SETUP", "SeqOff", 1, 0, 1, &bad);   /* PROPOSAL rig M-midi-1: SeqOff has no box of its own in the help (FlashOff = Release Seq); validated, kept in the file */
        out->on_top = (uint8_t)lj_midi_ini_int(ini, "OPTIONS", "OnTop", out->on_top, 0, 1, &bad);
        out->start_minimized = (uint8_t)lj_midi_ini_int(ini, "OPTIONS", "StartMinimized", 0, 0, 1, &bad);
        out->auto_save = (uint8_t)lj_midi_ini_int(ini, "OPTIONS", "AutoSave", out->auto_save, 0, 1, &bad);
        lj_ini_free(ini);
        if (bad) { lj_kv_err(err, cap, ini_path, 0, bad); lj_midi_init(out); return -1; }
    }
    if (map_path && lj_midi_exists(map_path) && lj_midi_load_map(map_path, out, err, cap) != 0) { lj_midi_init(out); return -1; }   /* missing = empty */
    return 0;
}

int lj_midi_save(const char *ini_path, const char *map_path, const lj_midi_map *in)
{
    lj_ini *ini;
    lj_rec *rec;
    const char *m;
    int i, k, ok = 0;
    if (!ini_path || !map_path || !in) return -1;
    if ((m = lj_midi_validate(in)) != NULL) { lj_log("midi: %s not saved: %s", map_path, m); return -1; }
    ini = lj_midi_exists(ini_path) ? lj_ini_load(ini_path) : NULL;   /* update in place: [FORM] Top/Left and any key we do not own survive */
    if (!ini) ini = lj_ini_new();
    lj_ini_seti(ini, "OPTIONS", "OnTop", in->on_top); lj_ini_seti(ini, "OPTIONS", "StartMinimized", in->start_minimized); lj_ini_seti(ini, "OPTIONS", "AutoSave", in->auto_save);
    lj_ini_seti(ini, "SETUP", "DeviceCode", in->device); lj_ini_seti(ini, "SETUP", "Note0", in->note0_is_off);
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) if (lj_midi_release_keys[k]) lj_ini_seti(ini, "SETUP", lj_midi_release_keys[k], in->release_on_off[k]);
    if (!lj_ini_gets(ini, "SETUP", "SeqOff", NULL)) lj_ini_seti(ini, "SETUP", "SeqOff", 1);   /* the shipped value; see load */
    for (k = 0; k < LJ_MIDI_KIND_COUNT; k++) {
        if (k == LJ_MIDI_CC && !in->enabled[k] && in->channel[k] == 16 && !lj_ini_gets(ini, "CC", "CCEnable", NULL)) continue;   /* the extension stays out of an LJ2-shaped file until used */
        lj_ini_seti(ini, lj_midi_keys[k].sec, lj_midi_keys[k].en, in->enabled[k]);
        lj_ini_seti(ini, lj_midi_keys[k].sec, lj_midi_keys[k].ch, in->channel[k]);
    }
    ok |= lj_ini_save(ini, ini_path);
    lj_ini_free(ini);
    rec = lj_rec_open_write(map_path, "midimap", 1);
    if (!rec) return -1;
    for (i = 0; i < in->n; i++) {
        const lj_midi_map_entry *e = &in->e[i];
        if (e->kind == LJ_MIDI_FN || e->kind == LJ_MIDI_CC)
            ok |= lj_rec_write(rec, "map ", "kind=%s ch=%u note=%u fn=%s", lj_midi_kind_words[e->kind], e->ch, e->note, lj_fn_table[e->fn].name);
        else
            ok |= lj_rec_write(rec, "map ", "kind=%s ch=%u note=%u num=%lu", lj_midi_kind_words[e->kind], e->ch, e->note, (unsigned long)e->num);
    }
    ok |= lj_rec_close(rec);
    return ok ? -1 : 0;
}
