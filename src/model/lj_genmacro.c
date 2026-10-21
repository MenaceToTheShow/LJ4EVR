/* lj_genmacro.c - CueMacros\NNNNNNNN.CMA (lj_genmacro.h). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_genmacro.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"

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
        if (strcmp(k, "num") == 0) out->number = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "restart") == 0) out->restart_on_load = (uint8_t)(strtol(v, NULL, 10) != 0);
        else if (strcmp(k, "ch") == 0) {
            long link = strtol(v, &e, 10), addr, a = 0, b = 0;
            if (e == v || *e != ':') { bad = "ch needs link:addr"; break; }
            addr = strtol(e + 1, &e, 10);
            if (link < 1 || link > LJ_LINKS || addr < 1 || addr > LJ_LINK_CH) { bad = "link:addr out of range"; break; }
            if (lj_kv_get(v, "amp", q, sizeof q) == 1) { a = strtol(q, &e, 10); if (*e == ',') b = strtol(e + 1, NULL, 10); }
            {
                int c = lj_addr_code((int)link, (int)addr);
                out->ch[c].shape = (uint8_t)lj_kv_long(v, "shape", 0);
                out->ch[c].base_on = (uint8_t)(lj_kv_long(v, "bon", 0) != 0);
                out->ch[c].base = (uint8_t)lj_kv_long(v, "base", 0);
                out->ch[c].amp_pos = (int16_t)a; out->ch[c].amp_neg = (int16_t)b;
                out->ch[c].cycle_ds = (uint16_t)lj_kv_long(v, "cycle", 0);
                out->ch[c].dir = (uint8_t)lj_kv_word(v, "dir", lj_dir_words, LJ_DIR_FWD);
                out->ch[c].delay = (uint16_t)lj_kv_long(v, "delay", 0);
            }
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
        if (!in->ch[i].shape) continue;
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
    for (i = 0; i < LJ_CHANS; i++)
        if (in->ch[i].dir > LJ_DIR_REV) return "direction not fwd/rev";   /* shape is a uint8_t: always inside the 8192-entry table (W2) */
    return NULL;
}

lj_genmacro *lj_genmacro_new_rec(void)
{
    lj_genmacro *m = (lj_genmacro *)calloc(1, sizeof *m);
    if (!m) lj_fatal("lj_genmacro_new_rec: out of memory");
    return m;
}
