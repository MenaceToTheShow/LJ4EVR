/* lj_bgcue.c - BGCues\NNNNNNNN.bgc (lj_bgcue.h, design 5.2, 03 s7). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_bgcue.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"

int lj_bgcue_load(const char *path, lj_bgcue *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "bgc", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 bgc file"); return -1; }
    memset(out, 0, sizeof *out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) out->number = (uint32_t)strtoul(v, NULL, 10);
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "transparent") == 0) out->transparent = (uint8_t)(strtol(v, NULL, 10) != 0);
        else if (strcmp(k, "slot") == 0) {
            long i = strtol(v, &e, 10);
            if (e == v || i < 1 || i > LJ_BG_SLOTS) { bad = "slot index not 1..5"; break; }
            out->slot[i - 1].seq = (uint32_t)lj_kv_long(v, "seq", 0);
            out->slot[i - 1].mode = (uint8_t)lj_kv_word(v, "mode", lj_bg_mode_words, LJ_BG_AUTO);
            out->slot[i - 1].enabled = (uint8_t)(lj_kv_long(v, "on", 1) != 0);
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_bgcue_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_bgcue_save(const char *path, const lj_bgcue *in)
{
    char q[72];
    int i, ok;
    lj_rec *rec = lj_rec_open_write(path, "bgc", 1);
    if (!rec) return -1;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "transparent", "%d", in->transparent) == 0;
    for (i = 0; ok && i < LJ_BG_SLOTS; i++)
        if (in->slot[i].seq)
            ok = lj_rec_write(rec, "slot ", "%d seq=%u mode=%s on=%d", i + 1, (unsigned)in->slot[i].seq, lj_word(lj_bg_mode_words, in->slot[i].mode), in->slot[i].enabled) == 0;
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_bgcue_validate(const lj_bgcue *in)
{
    int i;
    if (!memchr(in->name, 0, sizeof in->name)) return "name not NUL-terminated";
    for (i = 0; i < LJ_BG_SLOTS; i++) {
        if (!in->slot[i].seq && in->slot[i].enabled) return "slot enabled without a sequence";
        if (in->slot[i].mode > LJ_BG_NOLOOP) return "mode not auto/audio/noloop";
    }
    return NULL;
}

lj_bgcue *lj_bgcue_new_rec(void)
{
    lj_bgcue *bg = (lj_bgcue *)calloc(1, sizeof *bg);
    if (!bg) lj_fatal("lj_bgcue_new_rec: out of memory");
    return bg;
}

void lj_bgcue_free(lj_bgcue *bg) { free(bg); }

int lj_bgcue_add_sequence(lj_bgcue *bg, uint32_t seq)
{
    int i = 0;
    while (i < LJ_BG_SLOTS - 1 && bg->slot[i].seq) i++;    /* first free, else the last slot is overwritten [03 s7] */
    bg->slot[i].seq = seq; bg->slot[i].enabled = 1; bg->slot[i].mode = LJ_BG_AUTO;
    return i + 1;
}
