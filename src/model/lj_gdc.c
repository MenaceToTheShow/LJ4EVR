/* lj_gdc.c - Generic DMX Control command line, run and groups file (lj_gdc.h; F35, CHM thegenericdmxcontrol.htm). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen probe for the missing-file case (as model/lj_show.c) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_gdc.h"
#include "model/lj_kv.h"
#include "model/lj_show.h"
#include "core/lj_str.h"

/* ---- parse ---- */

static int is_grp(char c) { return c == '#' || c == '.' || c == ','; }        /* CHM: '.' or ',' may stand for '#' */
static int is_at(char c) { return c == '@' || c == '*'; }                    /* CHM: '*' may stand for '@' */
static int is_dig(char c) { return c >= '0' && c <= '9'; }

static int fail(char *err, size_t cap, int col)
{
    if (err && cap) snprintf(err, cap, "input error at column %d", col);   /* PROPOSAL rig F35: LJ2 has no message string for it */
    return -1;
}

/* one number at *i (at most 4 digits); -1 when there is none */
static int number(const char *s, int *i)
{
    int v = 0, n = 0;
    while (is_dig(s[*i])) { if (++n > 4) return -1; v = v * 10 + (s[*i] - '0'); (*i)++; }
    return n ? v : -1;
}

static int add(lj_gdc_prog *out, int op, int lo, int hi, int v)
{
    lj_gdc_cmd *c;
    if (out->n >= LJ_GDC_CMDS_MAX) return -1;
    c = &out->c[out->n++];
    c->op = (uint8_t)op; c->lo = (uint16_t)lo; c->hi = (uint16_t)hi; c->v = (uint16_t)v;
    return 0;
}

int lj_gdc_parse(const char *line, lj_gdc_prog *out, char *err, size_t cap)
{
    int i = 0, len;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    out->n = 0;
    if (!line) line = "";
    len = (int)strlen(line);
    if (len >= LJ_GDC_LINE_MAX) return fail(err, cap, LJ_GDC_LINE_MAX);
    while (line[i] == ' ') i++;
    if (!line[i]) return add(out, LJ_GDC_SEL_NONE, 0, 0, 0);                  /* [Return] on a blank command line */
    while (line[i]) {
        char c = line[i];
        int at = i, sign = 0, grp, lo, hi, v;
        if (c == ' ') { i++; continue; }
        if (c == 'o' || c == 'O' || c == 's' || c == 'S' || c == 'f' || c == 'F') {
            v = (c == 'o' || c == 'O') ? LJ_OFF : (c == 's' || c == 'S') ? LJ_SNAP : LJ_FADE;
            if (add(out, LJ_GDC_STATE, 0, 0, v) != 0) return fail(err, cap, at + 1);
            i++; continue;
        }
        if (is_at(c)) {
            int op = LJ_GDC_LEVEL;
            i++;
            if (is_at(line[i])) { op = LJ_GDC_LEVEL_ALL; i++; }               /* @@v programs all channels */
            else if (line[i] == '+') { op = LJ_GDC_LEVEL_UP; i++; }
            else if (line[i] == '-') { op = LJ_GDC_LEVEL_DOWN; i++; }
            if ((v = number(line, &i)) < 0) return fail(err, cap, i + 1);
            if (add(out, op, 0, 0, v) != 0) return fail(err, cap, at + 1);
            continue;
        }
        if (is_grp(c) && is_grp(line[i + 1])) {                               /* ## selects all channels */
            if (add(out, LJ_GDC_SEL_ALL, 0, 0, 0) != 0) return fail(err, cap, at + 1);
            i += 2; continue;
        }
        if (c == '+' || c == '-') { sign = c; i++; }
        grp = is_grp(line[i]);
        if (grp) i++;
        if ((lo = number(line, &i)) < 0) return fail(err, cap, i + 1);
        hi = lo;
        if (line[i] == '/') {
            i++;
            if (is_grp(line[i])) return fail(err, cap, i + 1);              /* "#1/#5 is considered an input error" */
            if ((hi = number(line, &i)) < 0) return fail(err, cap, i + 1);
        }
        if (hi < lo) { int t = lo; lo = hi; hi = t; }
        if (lo < 1 || hi > (grp ? LJ_GEN_GROUPS : LJ_CHANS)) return fail(err, cap, at + 1);
        if (add(out, (grp ? LJ_GDC_GRP_SET : LJ_GDC_SEL_SET) + (sign == '+' ? 1 : sign == '-' ? 2 : 0), lo, hi, 0) != 0) return fail(err, cap, at + 1);
    }
    return 0;
}

/* ---- run ---- */

static int generic_profile(const lj_profile *p)    /* the Generic dimmer and switch packs (CHM genericdmxprofiles: "channels allocated to level controls"; PROPOSAL rig F35 includes the switch packs) */
{
    return p && strcmp(p->manufacturer, "Generic") == 0 && (strncmp(p->name, "DMX Dimmer", 10) == 0 || strncmp(p->name, "DMX Switch Pack", 15) == 0);
}

int lj_gdc_is_generic(const lj_show *show, int code)
{
    int f;
    if (!show || code < 0 || code >= LJ_CHANS) return 0;
    f = lj_show_fixture_at(show, code);
    return f >= 0 && generic_profile(lj_show_profile(show, f));
}

static void build_map(const lj_show *show, uint8_t map[LJ_CHANS])     /* mapped = generic and not hidden */
{
    int i, o;
    memset(map, 0, LJ_CHANS);
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_profile *p = lj_show_profile(show, i);
        int base = lj_show_fixture_base(show, i);
        if (base < 0 || !generic_profile(p)) continue;
        for (o = 0; o < p->footprint && base + o < LJ_CHANS; o++) map[base + o] = !show->gdc.hidden[base + o];
    }
}

static int level_raw(const lj_gdc_state *st, int v) { if (st->percent) { if (v > 100) v = 100; return (v * 255 + 50) / 100; } return v > 255 ? 255 : v; }

static int put(lj_scene *sc, int ch, int val, int state)    /* 1 when the scene changed */
{
    int changed;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    if (state < 0) state = sc->state[ch] == LJ_SNAP ? LJ_SNAP : LJ_FADE;   /* "the fade state of each channel will default to fade (unless already programmed as snap)" */
    changed = sc->val[ch] != val || sc->state[ch] != state;
    sc->val[ch] = (uint8_t)val; sc->state[ch] = (uint8_t)state;
    return changed;
}

int lj_gdc_run(const lj_gdc_prog *p, const lj_show *show, lj_gdc_state *st, lj_scene *sc)
{
    static uint8_t map[LJ_CHANS];   /* UI thread only */
    int k, ch, n = 0;
    if (!p || !show || !st || !sc) return -1;
    build_map(show, map);
    for (k = 0; k < p->n; k++) {
        const lj_gdc_cmd *c = &p->c[k];
        int v;
        switch (c->op) {
        case LJ_GDC_SEL_SET: case LJ_GDC_SEL_ADD: case LJ_GDC_SEL_REMOVE:
            if (c->lo < 1 || c->hi > LJ_CHANS || c->lo > c->hi) return -1;
            if (c->op == LJ_GDC_SEL_SET) memset(st->sel, 0, LJ_CHANS);
            for (ch = c->lo - 1; ch < c->hi; ch++) if (map[ch]) st->sel[ch] = c->op != LJ_GDC_SEL_REMOVE;
            break;
        case LJ_GDC_GRP_SET: case LJ_GDC_GRP_ADD: case LJ_GDC_GRP_REMOVE: {
            uint32_t mask = 0;
            int g;
            if (c->lo < 1 || c->hi > LJ_GEN_GROUPS || c->lo > c->hi) return -1;
            for (g = c->lo; g <= c->hi; g++) mask |= 1u << (g - 1);
            if (c->op == LJ_GDC_GRP_SET) memset(st->sel, 0, LJ_CHANS);
            for (ch = 0; ch < LJ_CHANS; ch++) if (map[ch] && (show->gdc.member[ch] & mask)) st->sel[ch] = c->op != LJ_GDC_GRP_REMOVE;
            break;
        }
        case LJ_GDC_SEL_ALL: for (ch = 0; ch < LJ_CHANS; ch++) st->sel[ch] = map[ch]; break;
        case LJ_GDC_SEL_NONE: if (st->empty_clears) memset(st->sel, 0, LJ_CHANS); break;
        case LJ_GDC_LEVEL: case LJ_GDC_LEVEL_UP: case LJ_GDC_LEVEL_DOWN: case LJ_GDC_LEVEL_ALL:
            v = level_raw(st, c->v);
            for (ch = 0; ch < LJ_CHANS; ch++) {
                if (c->op == LJ_GDC_LEVEL_ALL ? !map[ch] : !st->sel[ch]) continue;
                n += put(sc, ch, c->op == LJ_GDC_LEVEL_UP ? sc->val[ch] + v : c->op == LJ_GDC_LEVEL_DOWN ? sc->val[ch] - v : v, -1);
            }
            break;
        case LJ_GDC_STATE:
            if (c->v > LJ_FADE) return -1;
            for (ch = 0; ch < LJ_CHANS; ch++) if (st->sel[ch]) n += put(sc, ch, sc->val[ch], c->v);
            break;
        default: return -1;
        }
    }
    return n;
}

/* ---- GenericGroups.dat: "LJ4 gdcgroups 1" / group n name=".." / ch l:a groups=<mask> / hide l:a ---- */

static int lead_code(const char *v, const char **rest)      /* "l:a" at the start of v -> address code, -1 */
{
    char *e;
    long l = strtol(v, &e, 10), a;
    if (e == v || *e != ':' || l < 1 || l > LJ_LINKS) return -1;
    v = e + 1;
    a = strtol(v, &e, 10);
    if (e == v || (*e && *e != ' ') || a < 1 || a > LJ_LINK_CH) return -1;
    if (rest) *rest = e;
    return lj_addr_code((int)l, (int)a);
}

int lj_gdc_groups_load(const char *path, lj_gdc_groups *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL, *rest;
    lj_rec *rec;
    FILE *f;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!path || (f = fopen(path, "rb")) == NULL) return 0;              /* missing file = no groups */
    fclose(f);
    rec = lj_rec_open_read(path, "gdcgroups", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 gdcgroups file"); return -1; }
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "group") == 0) {
            long g = lj_kv_range_lead(v, 0, 1, LJ_GEN_GROUPS, &rbad);
            if (rbad) { bad = "group not 1..32"; break; }
            q[0] = 0;
            if (lj_kv_get(v, "name", q, sizeof q) < 0) { bad = "bad group name"; break; }
            lj_str_copy(out->name[g - 1], sizeof out->name[g - 1], q);
            out->used[g - 1] = 1;
        } else if (strcmp(k, "ch") == 0) {
            int code = lead_code(v, &rest);
            char *e;
            unsigned long long m;
            if (code < 0 || lj_kv_get(rest, "groups", q, sizeof q) != 1) { bad = "bad ch line"; break; }
            errno = 0;                                   /* ME-171 (c): 64-bit on every OS, so 2^32 is refused on Windows too */
            m = strtoull(q, &e, 10);
            if (errno == ERANGE || e == q || *e || q[0] < '0' || q[0] > '9' || m > 0xFFFFFFFFull) { bad = "bad groups= mask"; break; }
            out->member[code] = (uint32_t)m;
        } else if (strcmp(k, "hide") == 0) {
            int code = lead_code(v, &rest);
            if (code < 0 || *rest) { bad = "bad hide line"; break; }
            out->hidden[code] = 1;
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(out, 0, sizeof *out); return -1; }
    return 0;
}

int lj_gdc_groups_save(const char *path, const lj_gdc_groups *in)
{
    char q[72];
    int i, ok = 1;
    lj_rec *rec;
    if (!path || !in) return -1;
    if ((rec = lj_rec_open_write(path, "gdcgroups", 1)) == NULL) return -1;
    for (i = 0; ok && i < LJ_GEN_GROUPS; i++) {
        if (!in->used[i]) continue;
        if (lj_kv_quote(q, sizeof q, memchr(in->name[i], 0, sizeof in->name[i]) ? in->name[i] : "") != 0) ok = 0;
        else ok = lj_rec_write(rec, "group ", "%d name=%s", i + 1, q[0] ? q : "\"\"") == 0;
    }
    for (i = 0; ok && i < LJ_CHANS; i++)
        if (in->member[i]) ok = lj_rec_write(rec, "ch ", "%d:%d groups=%lu", i / LJ_LINK_CH + 1, i % LJ_LINK_CH + 1, (unsigned long)in->member[i]) == 0;
    for (i = 0; ok && i < LJ_CHANS; i++)
        if (in->hidden[i]) ok = lj_rec_write(rec, "hide ", "%d:%d", i / LJ_LINK_CH + 1, i % LJ_LINK_CH + 1) == 0;
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}
