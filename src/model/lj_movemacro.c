/* lj_movemacro.c - model/lj_movemacro.h: macro type names, .bez shapes and their integer sampling, Fan Out and
 * Auto Delay (17-position_ref.py sections 5 and 6, integer for integer; tests/vectors.h asserts them). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define LJ_BEZ_SEP '\\'
#else
#include <dirent.h>
#define LJ_BEZ_SEP '/'
#endif
#include "model/lj_movemacro.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "core/lj_textrec.h"
#include "model/lj_genmacro.h"
#include "model/lj_kv.h"

const char *const lj_mm_type_names[LJ_MM_TYPE_BEZ0] = {
    "No Macro", "System Ellipse", "System Ellipse 2", "System Cos(2*a),Sin(a)", "System Sin(a)*|Sin(2a)|", "System Sin(a)^3", "System Stepped Ellipse" };

static int64_t lj_floordiv(int64_t a, int64_t b) { int64_t q = a / b; return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q; }   /* Python // */
static int64_t lj_nearest(int64_t a, int64_t b) { return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b); }   /* b > 0; the callers never meet a tie (odd b, or see 17 s5 comment) */
static int32_t lj_clamp16(int64_t v) { return v < -32768 ? -32768 : v > 32767 ? 32767 : (int32_t)v; }

/* ---- Bezier shapes [04 s4.5] ---- */
const char *lj_bez_validate(const lj_bez *in)
{
    if (!in || in->n < 3 || in->n > LJ_BEZ_NODES_MAX) return "bezier shape needs 3..64 nodes";
    if (!memchr(in->name, 0, sizeof in->name) || !in->name[0]) return "bezier shape needs a name";
    return NULL;
}

int lj_bez_load(const char *path, lj_bez *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, bad = 0;
    const char *why = NULL;
    lj_rec *rec;
    memset(out, 0, sizeof *out);
    if ((rec = lj_rec_open_read(path, "bez", &fmt)) == NULL) { lj_kv_err(err, cap, path, 0, "not an LJ4 bez file"); return -1; }
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (!strcmp(k, "name")) { if (lj_kv_unquote(out->name, sizeof out->name, v)) { why = "name longer than 30 characters"; break; } }
        else if (!strcmp(k, "node")) {
            lj_bez_node *nd;
            if (out->n >= LJ_BEZ_NODES_MAX) { why = "more than 64 nodes"; break; }
            nd = &out->node[out->n++];
            nd->x    = (int16_t)lj_kv_range(v, "x", 0, -32768, 32767, &bad);
            nd->y    = (int16_t)lj_kv_range(v, "y", 0, -32768, 32767, &bad);
            nd->in_x = (int16_t)lj_kv_range(v, "ix", nd->x, -32768, 32767, &bad);
            nd->in_y = (int16_t)lj_kv_range(v, "iy", nd->y, -32768, 32767, &bad);
            nd->out_x = (int16_t)lj_kv_range(v, "ox", nd->x, -32768, 32767, &bad);
            nd->out_y = (int16_t)lj_kv_range(v, "oy", nd->y, -32768, 32767, &bad);
            nd->line = (uint8_t)lj_kv_range(v, "line", 0, 0, 1, &bad);
        }                                                    /* unknown keys skipped (design 5.2) */
        if (bad) { why = "value out of range"; break; }
    }
    lj_rec_close(rec);
    if (!why && r < 0) why = "read error";
    if (!why) why = lj_bez_validate(out);
    if (why) { lj_kv_err(err, cap, path, item, why); return -1; }
    return 0;
}

int lj_bez_save(const char *path, const lj_bez *in)
{
    char q[80];
    int i, ok;
    lj_rec *rec;
    if (lj_bez_validate(in)) { lj_log("lj_bez_save: %s: %s", path, lj_bez_validate(in)); return -1; }
    if ((rec = lj_rec_open_write(path, "bez", 1)) == NULL) return -1;
    ok = lj_kv_quote(q, sizeof q, in->name) == 0 && lj_rec_write(rec, "name", "%s", q) == 0;
    for (i = 0; ok && i < in->n; i++) {
        const lj_bez_node *nd = &in->node[i];
        ok = lj_rec_write(rec, "node ", "x=%d y=%d ix=%d iy=%d ox=%d oy=%d line=%d", nd->x, nd->y, nd->in_x, nd->in_y, nd->out_x, nd->out_y, nd->line) == 0;
    }
    return (lj_rec_close(rec) == 0 && ok) ? 0 : -1;
}

/* one axis of a cubic segment at j / m: the Bernstein form with exact int64 arithmetic, rounded to nearest (the same
 * curve as de Casteljau; m <= 1366 keeps every term inside int64) */
static int32_t lj_bez_at(int32_t p0, int32_t p1, int32_t p2, int32_t p3, int line, int64_t j, int64_t m)
{
    int64_t u = m - j;
    if (line) return (int32_t)(p0 + lj_nearest((int64_t)(p3 - p0) * j, m));   /* "convert segments to lines" */
    return (int32_t)lj_nearest(u * u * u * p0 + 3 * u * u * j * p1 + 3 * u * j * j * p2 + j * j * j * p3, m * m * m);
}

static void lj_bez_norm(int16_t *a)
{
    int i, lo = 32767, hi = -32768;
    int64_t c2, span;
    for (i = 0; i < LJ_MM_N; i++) { if (a[i] < lo) lo = a[i]; if (a[i] > hi) hi = a[i]; }
    c2 = (int64_t)lo + hi; span = (int64_t)hi - lo;
    for (i = 0; i < LJ_MM_N; i++) a[i] = (int16_t)(span ? lj_nearest((2 * (int64_t)a[i] - c2) * 32767, span) : 0);   /* +-32767 around the centre */
}

int lj_bez_sample(const lj_bez *b, lj_bez_table *out)
{
    int k, j, at = 0, per;
    if (!b || !out || lj_bez_validate(b)) return -1;
    memset(out, 0, sizeof *out);
    lj_str_copy(out->name, sizeof out->name, b->name);
    per = LJ_MM_N / b->n;                                    /* LJ_MOVEMACRO_BEZ_EQUAL_TIME: equal points per segment */
    for (k = 0; k < b->n; k++) {
        const lj_bez_node *a = &b->node[k], *z = &b->node[(k + 1) % b->n];
        int m = k == b->n - 1 ? LJ_MM_N - at : per;          /* the remainder on the last segment (PROPOSAL rig 17 s8 item 6) */
        for (j = 0; j < m; j++, at++) {
            out->pan[at]  = (int16_t)lj_bez_at(a->x, a->out_x, z->in_x, z->x, a->line, j, m);
            out->tilt[at] = (int16_t)lj_bez_at(a->y, a->out_y, z->in_y, z->y, a->line, j, m);
        }
    }
    lj_bez_norm(out->pan);
    lj_bez_norm(out->tilt);
    return 0;
}

/* ---- Fan Out [04 s4.2, 17 s5.2] ---- */
void lj_fan_out_offsets(int n, int delta, int32_t *out)
{
    int i;
    for (i = 0; i < n; i++)                                  /* round(delta*256*(i-m)/m), m = (n-1)/2: never a tie (17 s5: odd n leaves 2^9 in the numerator, even n an odd denominator) */
        out[i] = n < 2 ? 0 : (int32_t)lj_nearest((int64_t)delta * 256 * (2 * i - (n - 1)), n - 1);
}

int lj_fan_out(int32_t *pan, int32_t *tilt, const int *order, int n, int dpan, int dtilt, int center_home)
{
    int32_t op[LJ_FIXTURES], ot[LJ_FIXTURES];
    int64_t cp = 0, ct = 0;
    int i;
    if (n < 2 || n > LJ_FIXTURES || !order) return -1;
    if (!center_home) {
        int a = order[(n - 1) / 2], b = order[n / 2];        /* the middle fixture, or the mean of the two middle ones (floor, as 17 s5) */
        cp = lj_floordiv((int64_t)pan[a] + pan[b], 2);
        ct = lj_floordiv((int64_t)tilt[a] + tilt[b], 2);
    }
    lj_fan_out_offsets(n, dpan, op);
    lj_fan_out_offsets(n, dtilt, ot);
    for (i = 0; i < n; i++) { pan[order[i]] = lj_clamp16(cp + op[i]); tilt[order[i]] = lj_clamp16(ct + ot[i]); }
    return 0;
}

int lj_fan_spread(int32_t *v, const int *order, int n)
{
    int64_t first, last;
    int i;
    if (n < 2 || !order) return -1;
    first = v[order[0]]; last = v[order[n - 1]];
    for (i = 0; i < n; i++) v[order[i]] = lj_clamp16(first + lj_floordiv((last - first) * i, n - 1));   /* Spread [CHM 3135, 17 s5] */
    return 0;
}

/* ---- Auto Delay [04 s4.6, 17 s6.2] ---- */
int lj_mm_auto_delay(int n, int style, int max_delay, int count, int segments, uint32_t *seed, uint8_t *out)
{
    int k;
    uint32_t x = seed && *seed ? *seed : 0x9E3779B9u;
    if (n < 1 || max_delay < 1 || max_delay > 256 || !out) return -1;
    if ((style == LJ_MMD_CIRCULAR && count < 1) || (style == LJ_MMD_SEGMENTS && segments < 1) || style < 0 || style > LJ_MMD_RANDOM) return -1;
    for (k = 0; k < n; k++) {
        int d;
        switch (style) {
        case LJ_MMD_LINEAR:   d = max_delay * k / n; break;
        case LJ_MMD_CIRCULAR: d = max_delay * (k % count) / count; break;
        case LJ_MMD_SEGMENTS: d = max_delay * (k * segments / n) / segments; break;
        default: x ^= x << 13; x ^= x >> 17; x ^= x << 5; d = (int)(x % (uint32_t)max_delay); break;   /* Random: xorshift32 (deterministic for tests) */
        }
        out[k] = (uint8_t)(d % 256);                        /* LJ_MOVEMACRO_DELAY_UNIT: 256 == 0 % */
    }
    if (seed) *seed = x;
    return 0;
}

int lj_delay_order(const lj_show *show, const int *sel, int n, int mode, int *order)
{
    int i, j, rev = mode == LJ_DO_FIXTURE_REV || mode == LJ_DO_ICON_H_REV || mode == LJ_DO_ICON_V_REV;
    int axis = mode == LJ_DO_ICON_H || mode == LJ_DO_ICON_H_REV ? 1 : mode == LJ_DO_ICON_V || mode == LJ_DO_ICON_V_REV ? 2 : 0;
    if (!show || !sel || !order || n < 0 || n > LJ_FIXTURES || mode < LJ_DO_FIXTURE || mode > LJ_DO_ICON_V_REV) return -1;
    for (i = 0; i < n; i++) {
        int f = sel[i], key, kj;
        if (f < 0 || f >= LJ_FIXTURES) return -1;
        key = axis == 1 ? show->fixture[f].icon_x : axis == 2 ? show->fixture[f].icon_y : 0;
        for (j = i; j > 0; j--) {                            /* insertion sort by (icon key, fixture number), 17 s6.2 ASSUMED ties */
            int g = order[j - 1];
            kj = axis == 1 ? show->fixture[g].icon_x : axis == 2 ? show->fixture[g].icon_y : 0;
            if (kj < key || (kj == key && g < f)) break;
            order[j] = g;
        }
        order[j] = f;
    }
    for (i = 0; rev && i < n / 2; i++) { int t = order[i]; order[i] = order[n - 1 - i]; order[n - 1 - i] = t; }
    return 0;
}

/* ---- the MacroShapes folder ---- */
int lj_bez_path(const char *dir, const char *name, char *out, size_t cap)
{
    if (!name || !name[0] || strpbrk(name, "\\/:*?\"<>|")) return -1;
    return snprintf(out, cap, "%s%c%s.bez", dir, LJ_BEZ_SEP, name) < (int)cap ? 0 : -1;
}

static int lj_bez_cmp(const void *a, const void *b)
{
    const unsigned char *x = a, *y = b;
    for (;; x++, y++) {
        int cx = (*x >= 'A' && *x <= 'Z') ? *x + 32 : *x, cy = (*y >= 'A' && *y <= 'Z') ? *y + 32 : *y;
        if (cx != cy || !cx) return cx - cy;
    }
}

static int lj_bez_take(const char *file, char names[][32], int *n, int max)   /* "<name>.bez" (any case) -> names[n++] */
{
    size_t len = strlen(file);
    if (len < 5 || len - 4 >= 32 || *n >= max) return 0;
    if (lj_bez_cmp(file + len - 4, ".bez") != 0) return 0;
    memcpy(names[*n], file, len - 4); names[*n][len - 4] = 0;
    (*n)++;
    return 1;
}

int lj_bez_list(const char *dir, char names[][32], int max)
{
    int n = 0;
#ifdef _WIN32
    char pat[1100]; struct _finddata_t fd; intptr_t h;
    if (snprintf(pat, sizeof pat, "%s%c*.bez", dir, LJ_BEZ_SEP) >= (int)sizeof pat) return -1;
    if ((h = _findfirst(pat, &fd)) == -1) return 0;           /* no folder or no shape: an empty list */
    do { if (!(fd.attrib & _A_SUBDIR)) lj_bez_take(fd.name, names, &n, max); } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR *d = opendir(dir); struct dirent *e;
    if (!d) return 0;
    while ((e = readdir(d)) != NULL) lj_bez_take(e->d_name, names, &n, max);
    closedir(d);
#endif
    qsort(names, (size_t)n, sizeof names[0], lj_bez_cmp);
    return n;
}

int lj_bez_set_load(const char *dir, struct lj_bez_set *out)
{
    static char names[LJ_BEZ_SHAPES_MAX][32];                /* UI thread only (the app's reload) */
    char path[1100], err[200];
    lj_bez b;
    int i, n = lj_bez_list(dir, names, LJ_BEZ_SHAPES_MAX);
    out->n = 0; out->t = NULL;
    if (n <= 0) return n < 0 ? -1 : 0;
    out->t = (lj_bez_table *)calloc((size_t)n, sizeof *out->t);
    if (!out->t) lj_fatal("lj_bez_set_load: out of memory");
    for (i = 0; i < n; i++) {
        if (lj_bez_path(dir, names[i], path, sizeof path) || lj_bez_load(path, &b, err, sizeof err) || lj_bez_sample(&b, &out->t[out->n])) {
            lj_log("lj_bez_set_load: %s skipped", names[i]);  /* lj_bez_load logged the reason */
            continue;
        }
        lj_str_copy(out->t[out->n].name, sizeof out->t[0].name, names[i]);   /* the drop-down shows the file's name */
        out->n++;
    }
    return 0;
}

void lj_bez_set_free(struct lj_bez_set *s)
{
    if (!s) return;
    free(s->t);
    s->t = NULL; s->n = 0;
}

int lj_bez_eval(const lj_bez *b, int seg, int j, int m, int32_t *x, int32_t *y)
{
    const lj_bez_node *a, *z;
    if (!b || b->n < 2 || seg < 0 || seg >= b->n || m < 1 || j < 0 || j > m) return -1;
    a = &b->node[seg]; z = &b->node[(seg + 1) % b->n];
    *x = lj_bez_at(a->x, a->out_x, z->in_x, z->x, a->line, j, m);
    *y = lj_bez_at(a->y, a->out_y, z->in_y, z->y, a->line, j, m);
    return 0;
}
