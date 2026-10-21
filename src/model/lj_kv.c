/* lj_kv.c - field helpers of lj_kv.h. Hand-rolled scanning (strtol, no sscanf) so a
 * 999-scene sequence loads well inside the 200 ms budget. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_kv.h"
#include "model/lj_words.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

/* Next "key=value" token of a field list: 1 produced, 0 end, -1 bad (unterminated quote or overflow). */
static int lj_kv_next(const char **pp, char *k, size_t kcap, char *v, size_t vcap)
{
    const char *p = *pp;
    size_t n = 0;
    while (*p == ' ') p++;
    if (!*p) { *pp = p; return 0; }
    while (*p && *p != ' ' && *p != '=') { if (n + 1 >= kcap) return -1; k[n++] = *p++; }
    k[n] = 0;
    n = 0;
    if (*p == '=') {
        p++;
        if (*p == '"') {
            p++;
            for (;;) {
                if (!*p) return -1;
                if (*p == '"') { if (p[1] == '"') p++; else { p++; break; } }
                if (n + 1 >= vcap) return -1;
                v[n++] = *p++;
            }
        } else {
            while (*p && *p != ' ') { if (n + 1 >= vcap) return -1; v[n++] = *p++; }
        }
    }
    v[n] = 0;
    *pp = p;
    return 1;
}

int lj_kv_get(const char *fields, const char *key, char *out, size_t cap)
{
    char k[32];
    int r;
    while ((r = lj_kv_next(&fields, k, sizeof k, out, cap)) == 1)
        if (strcmp(k, key) == 0) return 1;
    if (cap) out[0] = 0;
    return r;
}

long lj_kv_long(const char *fields, const char *key, long dflt)
{
    char v[32], *e;
    long n;
    if (lj_kv_get(fields, key, v, sizeof v) != 1 || !v[0]) return dflt;
    n = strtol(v, &e, 10);
    return *e ? dflt : n;
}

int lj_kv_word(const char *fields, const char *key, const char *const *table, int dflt)
{
    char v[32];
    int i;
    if (lj_kv_get(fields, key, v, sizeof v) != 1) return dflt;
    i = lj_word_index(table, v);
    return i < 0 ? dflt : i;
}

int lj_kv_quote(char *dst, size_t cap, const char *s)
{
    size_t n = 0;
    if (!strchr(s, ' ') && !strchr(s, '"')) return lj_str_copy(dst, cap, s);
    if (cap < 3) { if (cap) dst[0] = 0; return -1; }
    dst[n++] = '"';
    for (; *s; s++) {
        if (n + 3 >= cap) { dst[n] = 0; return -1; }
        if (*s == '"') dst[n++] = '"';
        dst[n++] = *s;
    }
    dst[n++] = '"'; dst[n] = 0;
    return 0;
}

int lj_kv_unquote(char *dst, size_t cap, const char *s)
{
    size_t n = 0;
    if (*s != '"') return lj_str_copy(dst, cap, s);
    for (s++; *s; s++) {
        if (*s == '"') { if (s[1] == '"') s++; else break; }
        if (n + 1 >= cap) { dst[n] = 0; return -1; }
        dst[n++] = *s;
    }
    dst[n] = 0;
    return 0;
}

int lj_kv_list(const char *s, uint32_t *out, int max)
{
    int n = 0;
    char *e;
    while (*s && *s != ' ') {                     /* the list ends at the next field ("macro=50,100 savemacro=1") */
        unsigned long v = strtoul(s, &e, 10);
        if (e == s || n >= max) return -1;
        out[n++] = (uint32_t)v;
        s = e;
        if (*s == ',') s++; else if (*s && *s != ' ') return -1;
    }
    return n;
}

int lj_kv_ch_write(lj_rec *rec, const uint8_t val[LJ_CHANS], const uint8_t state[LJ_CHANS])
{
    char line[LJ_KV_VAL_CAP];
    size_t n = 0;
    int code, per_line = 0;
    for (code = 0; code < LJ_CHANS; code++) {
        if (state ? state[code] == LJ_OFF : val[code] == 0) continue;
        n += (size_t)snprintf(line + n, sizeof line - n, "%s%d:%d=%d", n ? " " : "", code / LJ_LINK_CH + 1, code % LJ_LINK_CH + 1, val[code]);
        if (state) n += (size_t)snprintf(line + n, sizeof line - n, ":%s", lj_word(lj_fstate_words, state[code]));
        if (++per_line == 256) {                       /* 256 x "4:512=255:F " = 3 KB a line, under the 8 KB line cap */
            if (lj_rec_write(rec, "ch ", "%s", line) != 0) return -1;
            n = 0; per_line = 0;
        }
    }
    return n ? lj_rec_write(rec, "ch ", "%s", line) : 0;
}

int lj_kv_ch_parse(const char *fields, uint8_t val[LJ_CHANS], uint8_t state[LJ_CHANS])
{
    const char *p = fields;
    char *e;
    for (;;) {
        long link, addr, v, st = LJ_OFF;
        while (*p == ' ') p++;
        if (!*p) return 0;
        link = strtol(p, &e, 10); if (e == p || *e != ':') return -1; p = e + 1;
        addr = strtol(p, &e, 10); if (e == p || *e != '=') return -1; p = e + 1;
        v = strtol(p, &e, 10);    if (e == p) return -1; p = e;
        if (state) {
            if (*p != ':' || !p[1]) return -1;
            st = (p[1] == 'O') ? LJ_OFF : (p[1] == 'S') ? LJ_SNAP : (p[1] == 'F') ? LJ_FADE : -1;
            if (st < 0) return -1;
            p += 2;
        }
        if (*p && *p != ' ') return -1;
        if (link < 1 || link > LJ_LINKS || addr < 1 || addr > LJ_LINK_CH || v < 0 || v > 255) return -1;
        val[lj_addr_code((int)link, (int)addr)] = (uint8_t)v;
        if (state) state[lj_addr_code((int)link, (int)addr)] = (uint8_t)st;
    }
}

int lj_kv_scene_write(lj_rec *rec, const lj_scene *s)
{
    int i;
    if (lj_rec_write(rec, "scene ", "st=%u ft=%u", (unsigned)s->scene_ms, (unsigned)s->fade_ms) != 0) return -1;
    if (lj_kv_ch_write(rec, s->val, s->state) != 0) return -1;
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_movemacro *m = &s->macro[i];
        char line[160];
        int n;
        if (!s->preset[i] && !m->type) continue;
        n = snprintf(line, sizeof line, "%d", i + 1);
        if (s->preset[i]) n += snprintf(line + n, sizeof line - (size_t)n, " preset=%d", s->preset[i]);
        if (m->type) n += snprintf(line + n, sizeof line - (size_t)n, " macro=%d,%d,%d,%d,%d,%d,%d,%d,%u mst=%d,%d,%d,%d,%d,%d",
                                   m->type, m->amp_pan, m->amp_tilt, m->cycle_pan, m->cycle_tilt, m->dir, m->delay, m->tweak, m->step,
                                   m->fstate[0], m->fstate[1], m->fstate[2], m->fstate[3], m->fstate[4], m->fstate[5]);
        if (lj_rec_write(rec, "fx ", "%s", line) != 0) return -1;
    }
    return 0;
}

int lj_kv_scene_field(lj_scene *s, const char *key, const char *fields)
{
    if (strcmp(key, "scene") == 0) {
        long st = lj_kv_long(fields, "st", -1), ft = lj_kv_long(fields, "ft", -1);
        if (st < 0 || ft < 0) return -1;
        s->scene_ms = (uint32_t)st; s->fade_ms = (uint32_t)ft;
        return 1;
    }
    if (strcmp(key, "ch") == 0) return lj_kv_ch_parse(fields, s->val, s->state) == 0 ? 1 : -1;
    if (strcmp(key, "fx") == 0) {
        char v[96], *e;
        long i = strtol(fields, &e, 10), p;
        uint32_t m[9], st[6];
        if (e == fields || i < 1 || i > LJ_FIXTURES) return -1;
        p = lj_kv_long(fields, "preset", 0);
        if (p < 0 || p > LJ_PRESETS) return -1;
        s->preset[i - 1] = (uint8_t)p;
        if (lj_kv_get(fields, "macro", v, sizeof v) == 1) {
            lj_movemacro *mm = &s->macro[i - 1];
            if (lj_kv_list(v, m, 9) != 9) return -1;
            if (lj_kv_get(fields, "mst", v, sizeof v) != 1 || lj_kv_list(v, st, 6) != 6) return -1;
            if (m[0] > 255 || m[1] > 255 || m[2] > 255 || m[3] > 255 || m[4] > 255 || m[5] > 255 || m[6] > 255 || m[7] > 255 || m[8] > 65535) return -1;
            mm->type = (uint8_t)m[0]; mm->amp_pan = (uint8_t)m[1]; mm->amp_tilt = (uint8_t)m[2]; mm->cycle_pan = (uint8_t)m[3];
            mm->cycle_tilt = (uint8_t)m[4]; mm->dir = (uint8_t)m[5]; mm->delay = (uint8_t)m[6]; mm->tweak = (uint8_t)m[7]; mm->step = (uint16_t)m[8];
            for (p = 0; p < 6; p++) { if (st[p] > LJ_FADE) return -1; mm->fstate[p] = (uint8_t)st[p]; }
        }
        return 1;
    }
    return 0;
}

void lj_kv_err(char *err, size_t cap, const char *path, int item, const char *msg)
{
    char line[512];
    snprintf(line, sizeof line, "%s item %d: %s", path, item, msg);   /* item N = the Nth key/value after the header line (blank and # lines are not counted by core) */
    lj_log("%s", line);
    if (err && cap) lj_str_copy(err, cap, line);
}
