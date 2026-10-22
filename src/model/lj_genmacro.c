/* lj_genmacro.c - CueMacros\NNNNNNNN.CMA, the Macros.Mac override loader (ME-34) and Auto Delay (lj_genmacro.h). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_genmacro.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"
#include "core/lj_str.h"

int lj_genmacro_load(const char *path, lj_genmacro *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[64], *e;
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "macro", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 macro file"); return -1; }
    memset(out, 0, sizeof *out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) { int rb = 0; out->number = (uint32_t)lj_kv_range_lead(v, 0, 1, LJ_MACROS_MAX, &rb); if (rb) { bad = "num not 1..9999"; break; } }
        else if (strcmp(k, "name") == 0) { if (lj_kv_unquote(out->name, sizeof out->name, v) < 0) { bad = "name longer than 30"; break; } }
        else if (strcmp(k, "restart") == 0) { int rb = 0; out->restart_on_load = (uint8_t)lj_kv_range_lead(v, 0, 0, 1, &rb); if (rb) { bad = "restart not 0/1"; break; } }
        else if (strcmp(k, "ch") == 0) {                       /* every field range-checked: a .CMA from disk is untrusted (rule 6) */
            long link = strtol(v, &e, 10), addr, a, b = 0;
            int c, rb = 0;
            lj_gm_param *pp;
            if (e == v || *e != ':') { bad = "ch needs link:addr"; break; }
            addr = strtol(e + 1, &e, 10);
            if (link < 1 || link > LJ_LINKS || addr < 1 || addr > LJ_LINK_CH) { bad = "link:addr out of range"; break; }
            c = lj_addr_code((int)link, (int)addr); pp = &out->ch[c];
            if (lj_kv_get(v, "amp", q, sizeof q) != 1) { bad = "ch needs amp=<pos>,<neg>"; break; }
            a = strtol(q, &e, 10);
            if (e == q || *e != ',') { bad = "amp not <pos>,<neg>"; break; }
            b = strtol(e + 1, &e, 10);
            if (*e || a < -255 || a > 255 || b < -255 || b > 255) { bad = "amp not -255..255,-255..255"; break; }
            pp->shape = (uint8_t)lj_kv_range(v, "shape", 0, 0, LJ_GM_SHAPES_MAX - 1, &rb);
            pp->base_on = (uint8_t)lj_kv_range(v, "bon", 0, 0, 1, &rb);
            pp->base = (uint8_t)lj_kv_range(v, "base", 0, 0, 255, &rb);
            pp->amp_pos = (int16_t)a; pp->amp_neg = (int16_t)b;
            pp->cycle_ds = (uint16_t)lj_kv_range(v, "cycle", 0, 0, LJ_GM_CYCLE_MAX_DS, &rb);
            pp->dir = (uint8_t)lj_kv_word(v, "dir", lj_dir_words, LJ_DIR_FWD);
            pp->delay = (uint16_t)lj_kv_range(v, "delay", 0, 0, LJ_GM_DELAY_MAX, &rb);
            if (rb) { bad = "ch field out of range"; break; }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_genmacro_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_genmacro_save(const char *path, const lj_genmacro *in)
{
    char q[72];
    int i, ok;
    lj_rec *rec = lj_rec_open_write(path, "macro", 1);
    if (!rec) return -1;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "restart", "%d", in->restart_on_load) == 0;
    for (i = 0; ok && i < LJ_CHANS; i++) {
        if (!lj_genmacro_ch_used(in, i)) continue;              /* shape 0 '2*Sine' is a real shape (W1 skipped it) */
        ok = lj_rec_write(rec, "ch ", "%d:%d shape=%d bon=%d base=%d amp=%d,%d cycle=%u dir=%s delay=%u", i / LJ_LINK_CH + 1, i % LJ_LINK_CH + 1,
                          in->ch[i].shape, in->ch[i].base_on, in->ch[i].base, in->ch[i].amp_pos, in->ch[i].amp_neg, in->ch[i].cycle_ds,
                          lj_word(lj_dir_words, in->ch[i].dir), in->ch[i].delay) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_genmacro_validate(const lj_genmacro *in)
{
    int i;
    if (!memchr(in->name, 0, sizeof in->name)) return "name not NUL-terminated";
    if (in->number > LJ_MACROS_MAX) return "number not 0..9999";
    for (i = 0; i < LJ_CHANS; i++) {
        const lj_gm_param *p = &in->ch[i];
        if (p->dir > LJ_DIR_BOUNCE) return "direction not fwd/rev/bounce";   /* 14 sA.2: forward, reverse, bounce */
        if (p->delay > LJ_GM_DELAY_MAX) return "delay not 0..8191";
        if (p->cycle_ds > LJ_GM_CYCLE_MAX_DS) return "cycle not 0..5950";
        if (p->amp_pos < -255 || p->amp_pos > 255 || p->amp_neg < -255 || p->amp_neg > 255) return "amplitude not -255..255";
        if (p->shape >= LJ_GM_SHAPES_MAX) return "shape not 0..254";
    }
    return NULL;
}

lj_genmacro *lj_genmacro_new_rec(void)
{
    lj_genmacro *m = (lj_genmacro *)calloc(1, sizeof *m);
    if (!m) lj_fatal("lj_genmacro_new_rec: out of memory");
    return m;
}

/* ---- W3 (genmacro track) ---- */
int lj_genmacro_ch_used(const lj_genmacro *m, int ch)
{
    const lj_gm_param *p = &m->ch[ch];
    return p->shape || p->base_on || p->base || p->amp_pos || p->amp_neg || p->cycle_ds || p->dir || p->delay;
}

/* Macros.Mac layout [14 sA.6]: header 8 + 4096 reserved, records of 16415 bytes (ShortString[15], 15 unknown, 8192 x int16 LE). */
enum { LJ_MAC_HDR = 4104, LJ_MAC_REC = 16415, LJ_MAC_SAMPLES_AT = 31 };

static int lj_gm_mac_read(const char *path, lj_gm_shapes *out, char *err, size_t cap)   /* appends to out (s already sized for LJ_GM_SHAPES_MAX) */
{
    unsigned char hdr[8], rec[LJ_MAC_REC];
    long size;
    uint32_t ver, n, i, k;
    FILE *f = fopen(path, "rb");
    if (!f) { lj_kv_err(err, cap, path, 0, "cannot open"); return -1; }
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) || fread(hdr, 1, 8, f) != 8) { fclose(f); lj_kv_err(err, cap, path, 0, "not a Macros.Mac file (short header)"); return -1; }
    ver = (uint32_t)hdr[0] | (uint32_t)hdr[1] << 8 | (uint32_t)hdr[2] << 16 | (uint32_t)hdr[3] << 24;
    n = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16 | (uint32_t)hdr[7] << 24;
    if (ver != 1 || n == 0 || n > (uint32_t)(LJ_GM_SHAPES_MAX - out->n)
        || (unsigned long)size != (unsigned long)LJ_MAC_HDR + (unsigned long)n * LJ_MAC_REC) {   /* the count is checked against the size before any record is read */
        fclose(f); lj_kv_err(err, cap, path, 0, "not a Macros.Mac file (version, count or size)"); return -1;
    }
    if (fseek(f, LJ_MAC_HDR, SEEK_SET)) { fclose(f); lj_kv_err(err, cap, path, 0, "seek failed"); return -1; }
    for (i = 0; i < n; i++) {
        int16_t *dst = (int16_t *)out->s + (size_t)out->n * LJ_GM_SAMPLES;
        if (fread(rec, 1, LJ_MAC_REC, f) != LJ_MAC_REC) { fclose(f); lj_kv_err(err, cap, path, (int)i + 1, "short record"); return -1; }
        if (rec[0] > 15) { fclose(f); lj_kv_err(err, cap, path, (int)i + 1, "name longer than 15"); return -1; }
        memcpy(out->name[out->n], rec + 1, rec[0]); out->name[out->n][rec[0]] = 0;   /* CP1252 bytes as stored; the bytes after the name are garbage [14 sA.6] */
        for (k = 0; k < LJ_GM_SAMPLES; k++) {
            int v = (int)(rec[LJ_MAC_SAMPLES_AT + 2 * k] | rec[LJ_MAC_SAMPLES_AT + 2 * k + 1] << 8);
            dst[k] = (int16_t)(v >= 32768 ? v - 65536 : v);
        }
        out->n++;
    }
    fclose(f);
    return 0;
}

int lj_gm_shapes_load(const char *macros_mac, const char *user_mac, lj_gm_shapes *out, char *err, size_t cap)
{
    FILE *u;
    memset(out, 0, sizeof *out);
    if (!macros_mac) { lj_kv_err(err, cap, "Macros.Mac", 0, "no file given"); return -1; }   /* ponytail: UserMacros.Mac rides only beside an override Macros.Mac; user shapes over the regenerated built-ins need a copy of lj_gm_shapes_builtin here (engine) */
    out->s = (const int16_t *)calloc((size_t)LJ_GM_SHAPES_MAX * LJ_GM_SAMPLES, sizeof(int16_t));
    if (!out->s) { lj_kv_err(err, cap, macros_mac, 0, "out of memory"); return -1; }
    if (lj_gm_mac_read(macros_mac, out, err, cap)) { lj_gm_shapes_free(out); return -1; }
    out->nb = out->n;
    if (user_mac && (u = fopen(user_mac, "rb")) != NULL) {     /* missing = no user shapes */
        fclose(u);
        if (lj_gm_mac_read(user_mac, out, err, cap)) { lj_gm_shapes_free(out); return -1; }
    }
    if (err && cap) err[0] = 0;
    return 0;
}

void lj_gm_shapes_free(lj_gm_shapes *sh) { if (sh) { free((void *)sh->s); sh->s = NULL; sh->n = sh->nb = 0; } }

/* Auto Delay [14 sA.5]: m positions spread over the range, index j. Full cycle: from + 8192 * j / m (Martin's
 * "10 fixtures: 0 %, 10 %, ... 90 %"); From..To: from + span * j / (m - 1) with span = To - From, wrapped through
 * 8191 -> 0 when To < From (CHM 5873). Circular repeats the spread every ceil(n / groups) fixtures (12 fixtures,
 * 3 groups: 0/25/50/75 %); Segments gives each of the groups one value (12 fixtures, 4 groups: 0/25/50/75 %);
 * Random draws inside the range from *seed (xorshift32). The integer division truncates (PROPOSAL rig 14 sA.5). */
static uint16_t lj_gm_spread(int from, int span, int full, int j, int m)
{
    long v = full ? from + 8192L * j / m : m > 1 ? from + (long)span * j / (m - 1) : from;
    return (uint16_t)(v & LJ_GM_DELAY_MAX);
}

int lj_genmacro_auto_delay(int n, int style, int from, int to, int full_cycle, int groups, uint32_t *seed, uint16_t *out)
{
    int i, span = ((to - from) % 8192 + 8192) % 8192, per;
    if (n < 1 || from < 0 || from > LJ_GM_DELAY_MAX || to < 0 || to > LJ_GM_DELAY_MAX || !out) return -1;
    if ((style == LJ_GMD_CIRCULAR || style == LJ_GMD_SEGMENTS) && (groups < 2 || groups > 100)) return -1;   /* DFM min 2, max 100 */
    switch (style) {
    case LJ_GMD_LINEAR: for (i = 0; i < n; i++) out[i] = lj_gm_spread(from, span, full_cycle, i, n); break;
    case LJ_GMD_CIRCULAR:
        per = (n + groups - 1) / groups;
        for (i = 0; i < n; i++) out[i] = lj_gm_spread(from, span, full_cycle, i % per, per);
        break;
    case LJ_GMD_SEGMENTS: for (i = 0; i < n; i++) out[i] = lj_gm_spread(from, span, full_cycle, i * groups / n, groups); break;
    case LJ_GMD_RANDOM:
        if (!seed) return -1;
        for (i = 0; i < n; i++) {
            uint32_t x = *seed ? *seed : 0x9E3779B9u;
            x ^= x << 13; x ^= x >> 17; x ^= x << 5; *seed = x;
            out[i] = (uint16_t)((from + (long)(x % (uint32_t)((full_cycle ? 8191 : span) + 1))) & LJ_GM_DELAY_MAX);
        }
        break;
    default: return -1;
    }
    return 0;
}
