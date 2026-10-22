/* lj_presets.c - Presets\fixtureNNN.pre (lj_presets.h, design 5.2, [F36]). pan/tilt are the
 * one 16-bit value of 17 s1 each (ME-22: the pair is stored as one number, never two bytes). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_presets.h"
#include "model/lj_kv.h"

int lj_presets_load(const char *path, lj_preset presets[LJ_PRESETS], char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "presets", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 presets file"); return -1; }
    memset(presets, 0, (size_t)LJ_PRESETS * sizeof *presets);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "preset") == 0) {
            long i = lj_kv_range_lead(v, 0, 1, LJ_PRESETS, &rbad);
            long pan = lj_kv_range(v, "pan", 0, -32768, 32767, &rbad), tilt = lj_kv_range(v, "tilt", 0, -32768, 32767, &rbad);   /* ME-77 */
            if (rbad) { bad = "preset index not 1..99 or pan/tilt outside int16"; break; }
            presets[i - 1].used = 1;
            presets[i - 1].pan = (int16_t)pan;
            presets[i - 1].tilt = (int16_t)tilt;
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(presets, 0, (size_t)LJ_PRESETS * sizeof *presets); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_presets_save(const char *path, const lj_preset presets[LJ_PRESETS])
{
    int i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "presets", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < LJ_PRESETS; i++)
        if (presets[i].used) ok = lj_rec_write(rec, "preset ", "%d pan=%d tilt=%d", i + 1, presets[i].pan, presets[i].tilt) == 0;
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_presets_validate(const lj_preset presets[LJ_PRESETS])
{
    int i;
    for (i = 0; i < LJ_PRESETS; i++) if (presets[i].used > 1) return "used flag not 0/1";
    return NULL;
}
