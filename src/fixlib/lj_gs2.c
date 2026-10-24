/* lj_gs2.c - the .GS2 / .Udf / .Udo importer and exporter (lj_gs2.h; note 16; oracle tools/ref/16-gs2_ref.py).
 *
 * One transfer function, lj_gs2_xfer, walks the oracle's LAYOUT for the fields lj_profile owns, in both directions:
 * parse reads the record into the struct, write overwrites the struct-owned bytes of a copy of gs2_raw. Everything the
 * walk does not name (reserved blocks, const04, legacy_name on 2609 records, source_file, the OV strings, rgb_pad,
 * sub-record reserved tails, the 22 stray palette bytes, the 17 bytes at 0x266C) is never touched, so
 * lj_gs2_write(lj_gs2_parse(x)) == x, the oracle's serialize(parse(x)) == x.
 * ShortStrings: read = min(len byte, width) chars cut at the first NUL (C strings); write rewrites the length byte and
 * chars only when the struct text differs from what the raw bytes read as, so stale tails and over-long length bytes
 * survive an unedited round trip.
 * TColor (LORD_FIXTURES-9): bytes +7..+9 are R, G, B ($00BBGGRR little-endian); the struct keeps 0xRRGGBB (gfx order).
 * icon_caption is S[10] at 0x0028 (16 s4.2, oracle LAYOUT 's', 10): char[11], LORD_FIXTURES-14 settled at [11]. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_layout.h"
#include "core/lj_str.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LJ_GS2_FILE_MAX = 16 << 20 };   /* a .Udf/.Udo larger than this is refused unread (the library's largest is ~0.3 MB) */

static void lj_err(char *err, size_t cap, const char *fmt, int a, const char *s)
{
    if (!err || !cap) return;
    if (s) snprintf(err, cap, fmt, s, a); else snprintf(err, cap, fmt, a);
}

/* ---- the transfer walk (w = 0 read rec -> p, w = 1 write p -> rec) ---- */

static void x8(int w, uint8_t *rec, size_t off, uint8_t *v) { if (w) rec[off] = *v; else *v = rec[off]; }
static void xn(int w, uint8_t *rec, size_t off, uint8_t *v, size_t n) { if (w) memcpy(rec + off, v, n); else memcpy(v, rec + off, n); }

static void xs(int w, uint8_t *rec, size_t off, int n, char *s)   /* ShortString[n]; s has room for n + 1 */
{
    char cur[256];
    int ln = rec[off] < n ? rec[off] : n, i;
    for (i = 0; i < ln && rec[off + 1 + i]; i++) cur[i] = (char)rec[off + 1 + i];
    cur[i] = 0;
    if (!w) { memcpy(s, cur, (size_t)i + 1); return; }
    if (strcmp(cur, s) == 0) return;                                  /* unedited: raw bytes stay exactly as read */
    ln = (int)strlen(s);
    if (ln > n) ln = n;
    rec[off] = (uint8_t)ln;
    memcpy(rec + off + 1, s, (size_t)ln);
}

static void x32(int w, uint8_t *rec, size_t off, uint32_t *v)
{
    if (w) { rec[off] = (uint8_t)*v; rec[off + 1] = (uint8_t)(*v >> 8); rec[off + 2] = (uint8_t)(*v >> 16); rec[off + 3] = (uint8_t)(*v >> 24); }
    else *v = (uint32_t)rec[off] | (uint32_t)rec[off + 1] << 8 | (uint32_t)rec[off + 2] << 16 | (uint32_t)rec[off + 3] << 24;
}

static void xdyn(int w, uint8_t *rec, size_t off, lj_dyn *d)          /* 17-byte record, +13..+16 reserved untouched */
{
    x8(w, rec, off, &d->enabled); x8(w, rec, off + 1, &d->offset); xs(w, rec, off + 2, 8, d->name);
    x8(w, rec, off + 11, &d->min); x8(w, rec, off + 12, &d->max);
}

static void xstat(int w, uint8_t *rec, size_t off, lj_gs2_sfunc *s)   /* 87-byte record, +81..+86 reserved untouched */
{
    xs(w, rec, off, 15, s->name); x8(w, rec, off + 16, &s->enabled);
    xn(w, rec, off + 17, s->defined, LJ_PROFILE_CH); xn(w, rec, off + 49, s->value, LJ_PROFILE_CH);
}

static void xpal(int w, uint8_t *rec, size_t off, lj_palette *p)      /* 20 x 57-byte entries; +10 rgb_pad and +42..+56 untouched */
{
    int i;
    for (i = 0; i < LJ_PAL_ENTRIES; i++, off += 57) {
        lj_pal_entry *e = &p->e[i];
        x8(w, rec, off, &e->used); x8(w, rec, off + 1, &e->mask); xn(w, rec, off + 2, e->v, 5);
        if (w) { rec[off + 7] = (uint8_t)(e->rgb >> 16); rec[off + 8] = (uint8_t)(e->rgb >> 8); rec[off + 9] = (uint8_t)e->rgb; }
        else e->rgb = (uint32_t)rec[off + 7] << 16 | (uint32_t)rec[off + 8] << 8 | rec[off + 9];
        xs(w, rec, off + 11, 30, e->bitmap);
    }
}

static void lj_gs2_xfer(int w, lj_profile *p, uint8_t *rec)
{
    int i;
    uint8_t last = p->footprint ? (uint8_t)(p->footprint - 1) : 0;
    if (w) { rec[0] = (uint8_t)p->format_version; rec[1] = (uint8_t)(p->format_version >> 8); }
    else p->format_version = (uint16_t)(rec[0] | rec[1] << 8);
    x8(w, rec, 0x0005, &last);
    if (!w) p->footprint = (uint8_t)(last + 1);
    xs(w, rec, 0x0028, 10, p->icon_caption);
    xs(w, rec, 0x0033, 15, p->fixture_id);
    xs(w, rec, 0x0043, 30, p->author);
    xn(w, rec, 0x0062, p->notes_rtf, LJ_GS2_NOTES);
    xs(w, rec, 0x1063, 36, p->icon_bitmap);
    xn(w, rec, 0x1088, p->default_dmx, LJ_PROFILE_CH);
    x8(w, rec, 0x10A8, &p->transparent);
    xdyn(w, rec, 0x1173, &p->shutter);
    xdyn(w, rec, 0x1184, &p->strobe);
    x8(w, rec, 0x1195, &p->strobe_off);
    xdyn(w, rec, 0x1196, &p->intensity);
    x8(w, rec, 0x11A7, &p->invert_int);
    x8(w, rec, 0x11A8, &p->use_gm);
    x8(w, rec, 0x11A9, &p->bo_at_gm0);
    xstat(w, rec, 0x11AA, &p->blackout);
    xstat(w, rec, 0x1201, &p->lamp_on);
    xstat(w, rec, 0x1258, &p->lamp_off);
    xstat(w, rec, 0x12AF, &p->reset);
    x8(w, rec, 0x1306, &p->pt16); x8(w, rec, 0x1307, &p->pt8);
    x8(w, rec, 0x1308, &p->pan_ofs); x8(w, rec, 0x1309, &p->tilt_ofs);
    x8(w, rec, 0x130A, &p->pt_patch_on); xn(w, rec, 0x130B, p->pt_patch, 4);
    x8(w, rec, 0x130F, &p->presets); x8(w, rec, 0x1310, &p->macros);
    xdyn(w, rec, 0x1311, &p->misc_move);
    for (i = 0; i < 4; i++) xdyn(w, rec, 0x1322 + 17 * (size_t)i, &p->color[i]);
    xpal(w, rec, 0x1377, &p->palette[0]);
    xpal(w, rec, 0x17EB, &p->palette[1]);
    x8(w, rec, 0x1C5F, &p->cmy_on); x8(w, rec, 0x1C60, &p->rgb_on); x8(w, rec, 0x1C61, &p->cmy_picker); x8(w, rec, 0x1C62, &p->invert_cmyrgb);
    for (i = 0; i < 3; i++) xdyn(w, rec, 0x1C63 + 17 * (size_t)i, &p->cmy[i]);
    for (i = 0; i < 4; i++) xdyn(w, rec, 0x1CA7 + 17 * (size_t)i, &p->gobo[i]);
    xpal(w, rec, 0x1CFC, &p->palette[2]);
    xpal(w, rec, 0x2170, &p->palette[3]);
    xdyn(w, rec, 0x25E4, &p->iris); xdyn(w, rec, 0x25F5, &p->focus); xdyn(w, rec, 0x2606, &p->zoom); xdyn(w, rec, 0x2617, &p->frost);
    xdyn(w, rec, 0x2628, &p->beam[0]); xdyn(w, rec, 0x2639, &p->beam[1]);
    xdyn(w, rec, 0x264A, &p->effects[0]); xdyn(w, rec, 0x265B, &p->effects[1]);
    xpal(w, rec, 0x267D, &p->palette[4]);
    for (i = 0; i < 32; i++) xstat(w, rec, 0x2AF1 + 87 * (size_t)i, &p->special[i]);
    for (i = 0; i < LJ_PROFILE_CH; i++) {
        size_t o = 0x35D1 + 16 * (size_t)i;
        xs(w, rec, o, 5, (char *)p->level_caption[i]);
        x8(w, rec, o + 6, &p->level_invisible[i]); x8(w, rec, o + 7, &p->level_disabled[i]);
    }
    for (i = 0; i < 5; i++) xn(w, rec, 0x37D1 + 5 * (size_t)i, p->palette[i].type, 5);
    x8(w, rec, 0x37EA, &p->follow_spot);
    xpal(w, rec, 0x37EB, &p->palette[5]);
    xn(w, rec, 0x3C5F, p->palette[5].type, 5);
    xdyn(w, rec, 0x3C64, &p->effects[2]); xdyn(w, rec, 0x3C75, &p->effects[3]);
    x32(w, rec, 0x3D49, &p->reset_ms);
    x32(w, rec, 0x3D4D, &p->lamp_on_ms);
    x8(w, rec, 0x3D51, &p->invert_int_range); x8(w, rec, 0x3D52, &p->limit_gm);
    xn(w, rec, 0x3E14, p->ext_gm, LJ_PROFILE_CH);
    xn(w, rec, 0x3E34, p->hsv, 3);
    if (p->format_version == LJ_GS2_VERSION) {                       /* 2.6b9 split identity [16 s4.2] */
        xs(w, rec, 0x3E37, 30, p->name); xs(w, rec, 0x3E56, 30, p->manufacturer); xs(w, rec, 0x3E75, 30, p->mode);
    } else if (!w) {                                                 /* ME-33 (PROPOSAL 16 s10): the 0x3E37.. slots are not 2.6b9 data */
        xs(0, rec, 0x0009, 30, p->name);
        lj_str_copy(p->manufacturer, sizeof p->manufacturer, LJ_GS2_LEGACY_MANUFACTURER);
        p->mode[0] = 0;
    }                                                                /* write of a v0/v1 record: legacy_name travels in gs2_raw */
}

/* Channel classes for the engine and the layout (design 6.2 list). ME-25's SWITCH / SMOKE / HAZE / HAZE_FAN have no
 * .GS2 field to come from (16 s9), so a user profile never carries them. Later rows win: a shared control channel is
 * classed by its most specific function. Lamp / reset / special statics class only otherwise-plain channels. */
static void lj_cls(lj_profile *p, const lj_dyn *d, int c) { if (d->enabled && d->offset < LJ_PROFILE_CH) p->channel_class[d->offset] = (uint8_t)c; }
static void lj_cls_stat(lj_profile *p, const lj_gs2_sfunc *s, int c)
{
    int i;
    if (!s->enabled) return;
    for (i = 0; i < LJ_PROFILE_CH; i++)
        if (s->defined[i] && (p->channel_class[i] == LJ_CH_NONE || p->channel_class[i] == LJ_CH_LEVEL)) p->channel_class[i] = (uint8_t)c;
}
static void lj_gs2_classes(lj_profile *p)
{
    int i;
    memset(p->channel_class, LJ_CH_NONE, sizeof p->channel_class);
    for (i = 0; i < p->footprint && i < LJ_PROFILE_CH; i++) if (!p->level_disabled[i]) p->channel_class[i] = LJ_CH_LEVEL;
    lj_cls(p, &p->misc_move, LJ_CH_SPEED);
    lj_cls(p, &p->iris, LJ_CH_BEAM); lj_cls(p, &p->focus, LJ_CH_BEAM); lj_cls(p, &p->zoom, LJ_CH_BEAM); lj_cls(p, &p->frost, LJ_CH_BEAM);
    for (i = 0; i < 2; i++) lj_cls(p, &p->beam[i], LJ_CH_BEAM);
    for (i = 0; i < 4; i++) lj_cls(p, &p->effects[i], LJ_CH_EFFECT);
    for (i = 0; i < 4; i++) lj_cls(p, &p->gobo[i], LJ_CH_GOBO);
    for (i = 0; i < 4; i++) lj_cls(p, &p->color[i], LJ_CH_COLOR);
    if (p->cmy_on || p->rgb_on) for (i = 0; i < 3; i++) lj_cls(p, &p->cmy[i], LJ_CH_COLOR);
    lj_cls(p, &p->strobe, LJ_CH_STROBE);
    lj_cls(p, &p->shutter, LJ_CH_SHUTTER);
    lj_cls(p, &p->intensity, LJ_CH_INTENSITY);
    if (p->pt16 || p->pt8) {
        if (p->pt_patch_on && p->pt16) {
            p->channel_class[p->pt_patch[0]] = p->channel_class[p->pt_patch[1]] = LJ_CH_PAN;
            p->channel_class[p->pt_patch[2]] = p->channel_class[p->pt_patch[3]] = LJ_CH_TILT;
        } else {
            p->channel_class[p->pan_ofs] = LJ_CH_PAN; p->channel_class[p->tilt_ofs] = LJ_CH_TILT;
            if (p->pt16) { p->channel_class[p->pan_ofs + 1] = LJ_CH_PAN; p->channel_class[p->tilt_ofs + 1] = LJ_CH_TILT; }
        }
    }
    lj_cls_stat(p, &p->lamp_on, LJ_CH_LAMP); lj_cls_stat(p, &p->lamp_off, LJ_CH_LAMP); lj_cls_stat(p, &p->reset, LJ_CH_LAMP);
    for (i = 0; i < 32; i++) lj_cls_stat(p, &p->special[i], LJ_CH_SPECIAL);
}

/* Offsets the program indexes with (untrusted input, hard rule 6): every enabled function's channel is < 32. */
static int lj_gs2_check(const lj_profile *p, char *err, size_t cap)
{
    const lj_dyn *d[] = { &p->shutter, &p->strobe, &p->intensity, &p->misc_move, &p->color[0], &p->color[1], &p->color[2], &p->color[3],
                          &p->cmy[0], &p->cmy[1], &p->cmy[2], &p->gobo[0], &p->gobo[1], &p->gobo[2], &p->gobo[3], &p->iris, &p->focus,
                          &p->zoom, &p->frost, &p->beam[0], &p->beam[1], &p->effects[0], &p->effects[1], &p->effects[2], &p->effects[3] };
    size_t i;
    for (i = 0; i < sizeof d / sizeof d[0]; i++)
        if (d[i]->enabled && d[i]->offset >= LJ_PROFILE_CH) { lj_err(err, cap, "GS2 function %d offset out of range", (int)i, NULL); return -1; }
    if (p->pt16 || p->pt8) {
        int hi = p->pt16 ? LJ_PROFILE_CH - 1 : LJ_PROFILE_CH;
        if (p->pt_patch_on && p->pt16) {
            for (i = 0; i < 4; i++) if (p->pt_patch[i] >= LJ_PROFILE_CH) { lj_err(err, cap, "GS2 pan/tilt patch %d out of range", p->pt_patch[i], NULL); return -1; }
        } else if (p->pan_ofs >= hi || p->tilt_ofs >= hi) { lj_err(err, cap, "GS2 pan/tilt offset %d out of range", p->pan_ofs > p->tilt_ofs ? p->pan_ofs : p->tilt_ofs, NULL); return -1; }
    }
    return 0;
}

int lj_gs2_parse(const uint8_t rec[LJ_GS2_REC_SIZE], lj_profile *out, char *err, size_t cap)
{
    const uint8_t *raw;
    char file[sizeof out->gs2_file];
    if (err && cap) err[0] = 0;
    if (!rec || !out) return -1;
    if (rec[5] >= LJ_PROFILE_CH) { lj_err(err, cap, "GS2 last offset %d out of range", rec[5], NULL); return -1; }
    raw = out->gs2_raw;                                              /* the record's owner and name are not record fields */
    memcpy(file, out->gs2_file, sizeof file);
    memset(out, 0, sizeof *out);
    out->gs2_raw = raw;
    memcpy(out->gs2_file, file, sizeof file);
    lj_gs2_xfer(0, out, (uint8_t *)rec);                             /* w = 0 only reads rec */
    if (lj_gs2_check(out, err, cap) != 0) return -1;
    out->id = 46;                                                    /* exe row USER2 "Generic Fixture 2" (6.1) */
    lj_str_copy(out->tag, sizeof out->tag, "USER2");
    lj_gs2_classes(out);
    return 0;
}

int lj_gs2_write(const lj_profile *in, uint8_t rec[LJ_GS2_REC_SIZE])
{
    lj_profile tmp;
    int i;
    if (!in || !rec) return -1;
    tmp = *in;                                                      /* xfer takes a mutable profile; w = 1 only reads it */
    if (in->gs2_raw) memcpy(rec, in->gs2_raw, LJ_GS2_REC_SIZE);
    else {                                                           /* built-in / new profile: a zeroed 2.6b9 record */
        memset(rec, 0, LJ_GS2_REC_SIZE);
        rec[4] = 0x2E;                                               /* const04 [16 s4.1] */
        tmp.format_version = LJ_GS2_VERSION;
        for (i = tmp.footprint ? tmp.footprint : LJ_PROFILE_CH; i < LJ_PROFILE_CH; i++) tmp.level_invisible[i] = tmp.level_disabled[i] = 1;   /* LORD_FIXTURES-3: past the footprint = the library's (1,1) "unused" pattern [16 s3.1], so byte 5 keeps footprint - 1 */
    }
    lj_gs2_xfer(1, &tmp, rec);
    return 0;
}

int lj_gs2_read(const uint8_t *buf, size_t len, lj_profile *out, char *err, size_t cap)
{
    uint8_t *raw;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    if (!buf || len != LJ_GS2_REC_SIZE) { lj_err(err, cap, LJ_GS2_ERR_SIZE, buf ? (len > 0x7FFFFFFF ? 0x7FFFFFFF : (int)len) : 0, NULL); out->gs2_raw = NULL; return -1; }
    raw = malloc(LJ_GS2_REC_SIZE);
    if (!raw) { lj_err(err, cap, "out of memory", 0, NULL); out->gs2_raw = NULL; return -1; }
    memcpy(raw, buf, LJ_GS2_REC_SIZE);
    out->gs2_raw = NULL;
    if (lj_gs2_parse(raw, out, err, cap) != 0) { free(raw); out->gs2_raw = NULL; return -1; }
    out->gs2_raw = raw;
    lj_layout_derive(out);
    return 0;
}

/* ---- files ---- */

static uint8_t *lj_slurp(const char *path, size_t *len, char *err, size_t cap)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) { lj_err(err, cap, "cannot open %s", 0, path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); lj_err(err, cap, "cannot read %s", 0, path); return NULL; }
    if (n > LJ_GS2_FILE_MAX) { fclose(f); lj_err(err, cap, "%s: file too large (%d bytes)", (int)(n > 0x7FFFFFFF ? 0x7FFFFFFF : n), path); return NULL; }
    b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); lj_err(err, cap, "cannot read %s", 0, path); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

static void lj_base_gs2(lj_profile *p, const char *path)            /* gs2_file = base name, extension forced to .GS2 */
{
    const char *b = path, *s;
    char *dot;
    for (s = path; *s; s++) if (*s == '/' || *s == '\\') b = s + 1;
    lj_str_copy(p->gs2_file, sizeof p->gs2_file - 4, b);
    if ((dot = strrchr(p->gs2_file, '.')) != NULL) *dot = 0;
    strcat(p->gs2_file, ".GS2");
}

int lj_gs2_load(const char *path, lj_profile *out, char *err, size_t cap)
{
    size_t len = 0;
    uint8_t *b;
    int r;
    if (!path || !out) return -1;
    if (!(b = lj_slurp(path, &len, err, cap))) { out->gs2_raw = NULL; return -1; }
    r = lj_gs2_read(b, len, out, err, cap);
    free(b);
    if (r == 0) lj_base_gs2(out, path);
    return r;
}

int lj_gs2_save(const char *path, const lj_profile *in)
{
    uint8_t *rec = malloc(LJ_GS2_REC_SIZE);
    FILE *f;
    int r = -1;
    if (!rec || !path || lj_gs2_write(in, rec) != 0 || !(f = fopen(path, "wb"))) { free(rec); return -1; }
    if (fwrite(rec, 1, LJ_GS2_REC_SIZE, f) == LJ_GS2_REC_SIZE) r = 0;
    if (fclose(f) != 0) r = -1;
    free(rec);
    return r;
}

/* ---- .Udf / .Udo container [16 s2] ---- */

typedef struct { uint8_t *out; size_t n; } lj_inflate_sink;

static int lj_inflate_put(const void *buf, int len, void *ud)        /* keeps the first SIZE + 1 bytes, counts the rest; aborts past 16 MB */
{
    lj_inflate_sink *s = ud;
    size_t room = s->n < LJ_GS2_REC_SIZE + 1 ? LJ_GS2_REC_SIZE + 1 - s->n : 0, k = (size_t)len < room ? (size_t)len : room;
    if (k) memcpy(s->out + s->n, buf, k);
    s->n += (size_t)len;
    return s->n <= LJ_GS2_FILE_MAX;
}

int lj_udf_parse(const uint8_t *data, size_t len, lj_profile *out, char *err, size_t cap)
{
    const uint8_t *foot;
    size_t body;
    uint32_t crc;
    lj_inflate_sink s;
    uint8_t *rec;
    int r;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    out->gs2_raw = NULL;
    if (!data || len < LJ_UDF_FOOTER) { lj_err(err, cap, LJ_GS2_ERR_CRC, 0, NULL); return -1; }   /* the oracle: data[:-36] empty, CRC 0 != footer */
    body = len - LJ_UDF_FOOTER;
    foot = data + body;
    crc = (uint32_t)foot[0] | (uint32_t)foot[1] << 8 | (uint32_t)foot[2] << 16 | (uint32_t)foot[3] << 24;
    if ((uint32_t)mz_crc32(MZ_CRC32_INIT, data, body) != crc) { lj_err(err, cap, LJ_GS2_ERR_CRC, 0, NULL); return -1; }
    if (!(rec = malloc(LJ_GS2_REC_SIZE + 1))) { lj_err(err, cap, "out of memory", 0, NULL); return -1; }
    s.out = rec; s.n = 0;
    if (!tinfl_decompress_mem_to_callback(data, &body, lj_inflate_put, &s, TINFL_FLAG_PARSE_ZLIB_HEADER)) {
        free(rec);
        lj_err(err, cap, s.n > LJ_GS2_FILE_MAX ? "stream 0 inflates past %d bytes" : "stream 0: inflate failed (%d bytes out)",
               s.n > LJ_GS2_FILE_MAX ? LJ_GS2_FILE_MAX : (int)s.n, NULL);
        return -1;
    }
    r = lj_gs2_read(rec, s.n, out, err, cap);                        /* a wrong size gets the oracle's "not a GS2 record: %d bytes" */
    free(rec);
    return r;
}

int lj_udf_import(const char *path, lj_profile *out, char *err, size_t cap)
{
    size_t len = 0;
    uint8_t *b;
    int r;
    if (!path || !out) return -1;
    if (!(b = lj_slurp(path, &len, err, cap))) { out->gs2_raw = NULL; return -1; }
    r = lj_udf_parse(b, len, out, err, cap);
    free(b);
    if (r == 0) lj_base_gs2(out, path);
    return r;
}

static void lj_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* One-file container: stream 0 = the record, then the file table, then the footer. The table entry is 532 bytes as
 * measured on the udf200 sample (note 16 s2 says 266, which is one of its two ShortString[255] halves):
 * +0 S[255] file name, +256 S[255] destination folder, +512 u32 size, +516 u32 compressed size, +520 u32 offset of the
 * stream, +524 u32 crc32 of the uncompressed file, +528 u32 0x200 (constant in the sample). */
int lj_udf_export(const char *path, const lj_profile *in, char *err, size_t cap)
{
    static const char folder[] = "User\\UserFixtures\\";
    uint8_t *rec = NULL, *z0 = NULL, tbl[532], ztbl[700], foot[LJ_UDF_FOOTER];
    mz_ulong n0 = 0, nt = sizeof ztbl;
    char name[sizeof in->gs2_file];
    size_t ln;
    uint32_t crc;
    FILE *f = NULL;
    int r = -1;
    if (err && cap) err[0] = 0;
    if (!path || !in) return -1;
    rec = malloc(LJ_GS2_REC_SIZE);
    n0 = mz_compressBound(LJ_GS2_REC_SIZE);
    z0 = malloc(n0);
    if (!rec || !z0 || lj_gs2_write(in, rec) != 0 || mz_compress2(z0, &n0, rec, LJ_GS2_REC_SIZE, MZ_DEFAULT_LEVEL) != MZ_OK) { lj_err(err, cap, "cannot build stream 0", 0, NULL); goto done; }
    memset(tbl, 0, sizeof tbl);
    lj_str_copy(name, sizeof name, in->gs2_file[0] ? in->gs2_file : "Profile.GS2");
    ln = strlen(name);
    tbl[0] = (uint8_t)ln; memcpy(tbl + 1, name, ln);
    tbl[256] = (uint8_t)(sizeof folder - 1); memcpy(tbl + 257, folder, sizeof folder - 1);
    lj_le32(tbl + 512, LJ_GS2_REC_SIZE); lj_le32(tbl + 516, (uint32_t)n0); lj_le32(tbl + 520, 0);
    lj_le32(tbl + 524, (uint32_t)mz_crc32(MZ_CRC32_INIT, rec, LJ_GS2_REC_SIZE)); lj_le32(tbl + 528, 0x200);
    if (mz_compress2(ztbl, &nt, tbl, sizeof tbl, MZ_DEFAULT_LEVEL) != MZ_OK) { lj_err(err, cap, "cannot build the file table", 0, NULL); goto done; }
    crc = (uint32_t)mz_crc32(mz_crc32(MZ_CRC32_INIT, z0, n0), ztbl, nt);
    memset(foot, 0, sizeof foot);
    lj_le32(foot, crc); lj_le32(foot + 4, 1); lj_le32(foot + 8, 1); lj_le32(foot + 12, (uint32_t)nt); lj_le32(foot + 16, (uint32_t)n0); lj_le32(foot + 20, 1);
    if (!(f = fopen(path, "wb"))) { lj_err(err, cap, "cannot create %s", 0, path); goto done; }
    if (fwrite(z0, 1, n0, f) == n0 && fwrite(ztbl, 1, nt, f) == nt && fwrite(foot, 1, sizeof foot, f) == sizeof foot) r = 0;
    if (fclose(f) != 0) r = -1;
    if (r) lj_err(err, cap, "cannot write %s", 0, path);
done:
    free(rec); free(z0);
    return r;
}

/* gs2_raw and the lj_layout_derive tables (w3-contracts "Found while reading"): derive mallocs the window array, each
 * window's rows and specials, and every row's buttons; a profile with gs2_raw got its windows from derive (lj_gs2_read),
 * built-ins have gs2_raw NULL and static tables, so they are never touched. */
void lj_gs2_free(lj_profile *p)
{
    if (!p || !p->gs2_raw) return;
    lj_layout_free(p);
    free((void *)p->gs2_raw);
    p->gs2_raw = NULL;
}
