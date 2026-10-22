/* lj_palettes.c - Palettes\<Control>.Pre (lj_palettes.h, design 5.2, 04 s9). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_palettes.h"
#include "model/lj_kv.h"
#include "core/lj_str.h"

static int lj_palettes_reserve(lj_palette_list *l, int n)
{
    lj_user_palette *e;
    int cap;
    if (n <= l->cap) return 0;
    cap = l->cap ? l->cap * 2 : 8;
    if (cap < n) cap = n;
    e = (lj_user_palette *)realloc(l->e, (size_t)cap * sizeof *e);
    if (!e) lj_fatal("lj_palettes: out of memory at %d entries", cap);
    l->e = e; l->cap = cap;
    return 0;
}

int lj_palettes_load(const char *path, lj_palette_list *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP];
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "palette", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 palette file"); return -1; }
    lj_palettes_free(out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "entry") == 0) {
            lj_user_palette *p;
            if (lj_kv_range_lead(v, 0, out->n + 1, out->n + 1, NULL) != out->n + 1) { bad = "entry index out of order"; break; }   /* ME-77 helper */
            lj_palettes_reserve(out, out->n + 1);
            p = &out->e[out->n++];
            memset(p, 0, sizeof *p);
            if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(p->name, sizeof p->name, q);
        } else if (strcmp(k, "ch") == 0) {
            if (!out->n) { bad = "channel data before the first entry"; break; }
            if (lj_kv_ch_parse(v, out->e[out->n - 1].val, out->e[out->n - 1].state) != 0) { bad = "bad channel line"; break; }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_palettes_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); lj_palettes_free(out); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_palettes_save(const char *path, const lj_palette_list *in)
{
    char q[72];
    int i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "palette", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < in->n; i++) {
        lj_kv_quote(q, sizeof q, in->e[i].name);
        ok = lj_rec_write(rec, "entry ", "%d name=%s", i + 1, q) == 0 && lj_kv_ch_write(rec, in->e[i].val, in->e[i].state) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_palettes_validate(const lj_palette_list *in)
{
    int i, c;
    if (in->n < 0 || in->n > in->cap || (in->n && !in->e)) return "entry count inconsistent";
    for (i = 0; i < in->n; i++) {
        if (!memchr(in->e[i].name, 0, sizeof in->e[i].name)) return "entry name not NUL-terminated";
        for (c = 0; c < LJ_CHANS; c++) if (in->e[i].state[c] > LJ_FADE) return "fade state not 0..2";
    }
    return NULL;
}

int lj_palettes_add(lj_palette_list *l, const char *name, const uint8_t val[LJ_CHANS], const uint8_t state[LJ_CHANS])
{
    lj_user_palette *p;
    lj_palettes_reserve(l, l->n + 1);
    p = &l->e[l->n];
    memset(p, 0, sizeof *p);
    lj_str_copy(p->name, sizeof p->name, name ? name : "");
    memcpy(p->val, val, LJ_CHANS);
    memcpy(p->state, state, LJ_CHANS);
    return l->n++;
}

int lj_palettes_delete(lj_palette_list *l, int index)
{
    if (index < 0 || index >= l->n) return -1;
    memmove(l->e + index, l->e + index + 1, (size_t)(l->n - index - 1) * sizeof *l->e);
    l->n--;
    return 0;
}

void lj_palettes_free(lj_palette_list *l) { free(l->e); l->e = NULL; l->n = l->cap = 0; }

const char *lj_palettes_file_name(enum lj_ctl_kind kind)
{
    switch (kind) {                                   /* LJ2's own file names, 13 exe strings: Intensity/Movement/Color/Gobo/Beam/Effects/Levels.Pre */
    case LJ_CTL_INTENSITY: return "Intensity.Pre";
    case LJ_CTL_MOVEMENT:  return "Movement.Pre";
    case LJ_CTL_COLORS:    return "Color.Pre";
    case LJ_CTL_GOBOS:     return "Gobo.Pre";
    case LJ_CTL_BEAM:      return "Beam.Pre";
    case LJ_CTL_EFFECTS:   return "Effects.Pre";
    case LJ_CTL_LEVELS:    return "Levels.Pre";
    default:               return NULL;               /* lamp, extended, generic, RGB pack: no palettes */
    }
}
