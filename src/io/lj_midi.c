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
static struct { int learning, learned, ch, note, want_cc, is_cc; int last_ch[LJ_MIDI_KIND_COUNT], last_note[LJ_MIDI_KIND_COUNT]; } lj_mu;

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
    if (msg[0] >= 0xF0 && msg[0] != 0xF1) return;                /* system: clock, active sensing (no MIDI clock, 05 s7.2); 0xF1 = an MTC quarter frame, lj_mtc_feed on the UI thread (W3) */
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

void lj_midi_learn_begin(void)  { lj_mu.learning = 1; lj_mu.learned = 0; lj_mu.want_cc = 0; }
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
    if (lj_mu.learning && (status != 0xB0 || lj_mu.want_cc)) {    /* Learn takes the next note on (or CC: W4 ext Learn), nothing else fires meanwhile */
        if (on || status == 0xB0) { lj_mu.ch = ch; lj_mu.note = note; lj_mu.is_cc = status == 0xB0; lj_mu.learned = 1; lj_mu.learning = 0; }
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

/* ---- W3: MIDI time code [03 s6.4, design 8.3] and the LORD_C-6 byte-stream parser (cuelist-core) ---- */
#define LJ_MTC_TIMEOUT_US 500000u   /* = LJ_MTC_TIMEOUT_MS of engine/lj_engine_rules.h (io/ cannot include engine/) */

void lj_mtc_reset(lj_mtc *t) { memset(t, 0, sizeof *t); }

/* One quarter-frame data byte (0nnn dddd: piece n, nibble d). Pieces 0..7 = frames lo/hi, seconds lo/hi, minutes lo/hi,
 * hours lo, hours hi + rate (0 = 24, 1 = 25, 2 = 30 drop, 3 = 30). A full set ends on piece 7; the time it carries is
 * the one at piece 0, 2 frames behind the source, displayed as received like LJ2 (LJ_MTC_LATE_FRAMES). */
int lj_mtc_feed(lj_mtc *t, uint8_t qf_data, uint64_t now_us)
{
    static const uint8_t rate_fps[4] = { 24, 25, 29, 30 };
    int n = (qf_data >> 4) & 7, f, s, m, h, fps;
    if (qf_data > 0x7F) return 0;
    if (now_us - t->last_us > LJ_MTC_TIMEOUT_US) t->qf_run = 0;   /* SyncFrames: the time code (re)starts, count again */
    if (t->qf_run < 0xFFFFu) t->qf_run++;
    if (n == 0) t->have = 0;                                   /* a set starts at piece 0 */
    t->piece[n] = (uint8_t)(qf_data & 0x0F);
    t->have |= (uint8_t)(1u << n);
    t->last_us = now_us;
    if (n != 7 || t->have != 0xFF) return 0;
    t->have = 0;
    f = t->piece[0] | (t->piece[1] & 1) << 4; s = t->piece[2] | (t->piece[3] & 3) << 4;
    m = t->piece[4] | (t->piece[5] & 3) << 4; h = t->piece[6] | (t->piece[7] & 1) << 4;
    fps = rate_fps[(t->piece[7] >> 1) & 3];
    if (f >= (fps == 29 ? 30 : fps) || s > 59 || m > 59 || h > 23) return 0;   /* a garbled set is dropped, the last good time stays */
    t->fps = (uint8_t)fps;
    t->ms = (uint32_t)(((h * 60 + m) * 60 + s) * 1000) + (uint32_t)((f * 1000 + (fps == 29 ? 30 : fps) - 1) / (fps == 29 ? 30 : fps));   /* lj_cl_hmsf_ms packing (model/lj_cuelist.h) */
    t->running = 1;
    return 1;
}

int lj_mtc_time(const lj_mtc *t, uint64_t now_us, uint32_t *ms)
{
    if (!t->running || now_us - t->last_us > LJ_MTC_TIMEOUT_US) return -1;   /* none yet, or time code stopped: the time-out preference acts on -1 */
    if (t->qf_run < 4u * t->sync_frames) return -1;                         /* SyncFrames: not locked yet */
    *ms = t->ms;
    return 0;
}

/* Raw MIDI bytes -> 3-byte messages: running status kept in run[0] across calls, real-time bytes (F8..FF) skipped where
 * they sit, SysEx and system common other than F1 skipped up to the next status byte; F1 (MTC quarter frame) is
 * emitted as {F1, data, 0} and, being system common, cancels running status. Returns the messages emitted. */
int lj_midi_feed(const uint8_t *bytes, size_t n, uint8_t run[3], int *have, void (*msg)(const uint8_t m[3], void *ud), void *ud)
{
    size_t i;
    int out = 0, need;
    if (*have < 0 || *have > 1) *have = 0;
    for (i = 0; i < n; i++) {
        uint8_t b = bytes[i];
        if (b >= 0xF8) continue;                                       /* real-time: clock, active sensing */
        if (b & 0x80) { run[0] = (uint8_t)(b >= 0xF0 && b != 0xF1 ? 0 : b); *have = 0; continue; }
        if (!run[0]) continue;                                         /* SysEx data, or data with no status */
        need = (run[0] == 0xF1 || (run[0] & 0xE0) == 0xC0) ? 1 : 2;    /* F1, program change, channel pressure: 1 data byte */
        run[1 + (*have)++] = b;
        if (*have < need) continue;
        if (need == 1) run[2] = 0;
        *have = 0;
        if (msg) msg(run, ud);
        out++;
        if (run[0] == 0xF1) run[0] = 0;
    }
    return out;
}

/* ---- W4 da2532-fingers (appended): Learn for the LJ_FNF_EXT rows (lj_midi.h) ---- */

void lj_midi_learn_begin_ext(int want_cc) { lj_midi_learn_begin(); lj_mu.want_cc = want_cc != 0; }

int lj_midi_learn_take_ext(int *ch, int *note, int *is_cc)
{
    int cc = lj_mu.is_cc;
    if (!lj_midi_learn_take(ch, note)) return 0;
    if (is_cc) *is_cc = cc;
    return 1;
}

int lj_midi_learn_bind(lj_midi_map *m, enum lj_fn fn, int ch, int note, int is_cc)
{
    lj_midi_map_entry e;
    int k = is_cc ? LJ_MIDI_CC : LJ_MIDI_FN;
    if (!m || (int)fn <= 0 || fn >= LJ_FN_COUNT || ch < 0 || ch > 16 || note < 0 || note > 127) return -1;
    memset(&e, 0, sizeof e);
    e.kind = (uint8_t)k; e.ch = (uint8_t)ch; e.note = (uint8_t)note; e.fn = (uint16_t)fn;
    if (lj_midi_assign(m, &e) != 0) return -1;
    if (!m->enabled[k]) { m->enabled[k] = 1; lj_log("MIDI-IN: %s enabled by Learn of %s", lj_midi_kind_words[k], lj_fn_table[fn].name); }   /* the binding fires at once */
    if (m->channel[k] != 16 && m->channel[k] != ch) lj_log("MIDI-IN: %s is gated to channel %d; the learnt channel %d does not pass", lj_midi_kind_words[k], m->channel[k], ch);
    return 0;
}
