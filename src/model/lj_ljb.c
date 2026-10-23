/* lj_ljb.c - the .LJB importer (model/lj_ljb.h; note 18 s1-s13; W5-LJB-1..4). Every byte of the file is hostile: the
 * container is checked whole in lj_ljb_open before any entry is inflated, each record's size is checked before a field
 * is read, and nothing is ever written under a name taken from the file (the converted records get LJ4 names). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include "model/lj_ljb.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "fixlib/lj_gs2.h"
#include "model/lj_cue.h"
#include "model/lj_cuepage.h"
#include "model/lj_genmacro.h"
#include "model/lj_groups.h"
#include "model/lj_notes.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "model/lj_statics.h"
#include "miniz.h"

/* note 18 record sizes (s3 [EXE-IMM]) and offsets */
enum { LJB_CONFIG = 16546, LJB_CONFIG_BASE = 0x15, LJB_CONFIG_REC = 165, LJB_GROUPS = 12972, LJB_GROUP_REC = 131,
       LJB_BLOCK = 2748, LJB_STATICS = 127342, LJB_SCENE = 6312, LJB_SEQ_HDR = 0x18C8, LJB_CMA = 26659, LJB_CMA_HDR = 4131,
       LJB_CMA_REC = 11, LJB_CUE = 1132, LJB_PAGE_REC = 35, LJB_INFLATE_RATIO = 1032,
       LJB_INFO_MIN = 0x204 /* Info.$$$: ShortString[255] name at 0x104 (18 s1.4); the source path at 0x04 is never read */ };

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static int16_t  rds16(const uint8_t *p) { uint16_t v = rd16(p); return v < 0x8000u ? (int16_t)v : (int16_t)(-(int)(0x10000u - v)); }
static int      errs(char *err, size_t cap, const char *msg) { if (err && cap) lj_str_copy(err, cap, msg); return -1; }
static int      erri(char *err, size_t cap, const char *fmt, uint32_t i) { if (err && cap) snprintf(err, cap, fmt, (int)i); return -1; }

/* ShortString[width-1] at p (the caller checked p + width is inside the record): a length byte above the width is
 * clamped (02 s9.1); control bytes become '?' so no imported name can break a line of an LJ4 text record. */
static void sstr(char *dst, size_t cap, const uint8_t *p, size_t width)
{
    size_t n = p[0] < width ? p[0] : width - 1, i;
    if (n >= cap) n = cap - 1;
    for (i = 0; i < n; i++) dst[i] = p[1 + i] < 0x20 ? '?' : (char)p[1 + i];
    dst[n] = 0;
}

/* case-insensitive as the Windows file system folds CP1252: ASCII and the Latin-1 letters 0xE0..0xFE (not 0xF7) */
static int ci(int c) { c &= 0xFF; return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7) ? c - 32 : c; }
static int ci_eq(const char *a, const char *b) { for (; *a && ci(*a) == ci(*b); a++, b++) {} return ci(*a) == ci(*b); }

/* one path component: not empty, ".", "..", no control byte, none of <>:"/\|?*, no trailing dot or space (Windows
 * strips them, so "a." would alias "a"), not a reserved device name with or without an extension */
static int comp_ok(const char *s, size_t n)
{
    static const char dev[6][4] = { "CON", "PRN", "AUX", "NUL", "COM", "LPT" };
    size_t i, stem;
    if (!n || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.') || s[n - 1] == '.' || s[n - 1] == ' ') return 0;
    for (i = 0; i < n; i++) if ((unsigned char)s[i] < 0x20 || strchr("<>:\"/\\|?*", s[i])) return 0;
    for (stem = 0; stem < n && s[stem] != '.'; stem++) {}
    for (i = 0; i < 6; i++) {
        if (stem < 3 || ci(s[0]) != dev[i][0] || ci(s[1]) != dev[i][1] || ci(s[2]) != dev[i][2]) continue;
        if (i < 4 ? stem == 3 : stem == 4 && s[3] >= '1' && s[3] <= '9') return 0;
    }
    return 1;
}

int lj_ljb_path_ok(const char *folder, const char *name)
{
    size_t n, a, i;
    if (!folder || !name || !comp_ok(name, strlen(name))) return 0;
    n = strlen(folder);
    if (!n) return 1;
    if (folder[n - 1] != '\\') return 0;                                  /* LJ2 stores "Sequences\" (18 s1.3) */
    for (a = i = 0; i < n; i++) if (folder[i] == '\\') { if (!comp_ok(folder + a, i - a)) return 0; a = i + 1; }
    return 1;
}

void lj_ljb_free(lj_ljb *b) { if (b) { free(b->e); memset(b, 0, sizeof *b); } }

int lj_ljb_open(lj_ljb *out, const uint8_t *data, size_t len, char *err, size_t cap)
{
    const uint8_t *t;
    uint8_t *idx = NULL;
    uint32_t n, isz, ioff, i, j;
    uint64_t total = 0;
    size_t want;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!data || len < LJ_LJB_TRAILER) return errs(err, cap, LJ_LJB_ERR_CRC);
    if (len > LJ_LJB_FILE_MAX) return errs(err, cap, LJ_LJB_ERR_SIZE);
    t = data + len - LJ_LJB_TRAILER;
    if ((uint32_t)mz_crc32(MZ_CRC32_INIT, data, len - LJ_LJB_TRAILER) != rd32(t)) return errs(err, cap, LJ_LJB_ERR_CRC);   /* first (18 s1.2) */
    n = rd32(t + 4); isz = rd32(t + 12); ioff = rd32(t + 16);
    if (n < 1 || n > LJ_LJB_ENTRIES_MAX || !isz || (uint64_t)ioff + isz > len - LJ_LJB_TRAILER) return errs(err, cap, LJ_LJB_ERR_INDEX);
    want = (size_t)n * LJ_LJB_INDEX_REC;
    if (!(idx = (uint8_t *)malloc(want + 1))) lj_fatal("lj_ljb_open: out of memory");
    if (tinfl_decompress_mem_to_mem(idx, want + 1, data + ioff, isz, TINFL_FLAG_PARSE_ZLIB_HEADER) != want) { free(idx); return errs(err, cap, LJ_LJB_ERR_INDEX); }   /* exactly n x 532 */
    if (!(out->e = (lj_ljb_entry *)calloc(n, sizeof *out->e))) lj_fatal("lj_ljb_open: out of memory");
    for (i = 0; i < n; i++) {
        const uint8_t *r = idx + (size_t)i * LJ_LJB_INDEX_REC;
        lj_ljb_entry *e = &out->e[i];
        if (!r[0] || memchr(r + 1, 0, r[0]) || memchr(r + 257, 0, r[256])) { erri(err, cap, LJ_LJB_ERR_PATH, i); goto bad; }   /* lengths <= 255 by type; NUL inside a name refused */
        memcpy(e->name, r + 1, r[0]); memcpy(e->folder, r + 257, r[256]);
        e->usz = rd32(r + 512); e->csz = rd32(r + 516); e->off = rd32(r + 520); e->crc = rd32(r + 524); e->flags = rd32(r + 528);
        if (e->flags == LJ_LJB_FLAG_STORED ? e->csz != e->usz : e->flags != LJ_LJB_FLAG_DEFLATE || (uint64_t)e->usz > (uint64_t)e->csz * LJB_INFLATE_RATIO + 64)
            { erri(err, cap, LJ_LJB_ERR_ENTRY, i); goto bad; }                 /* any other flag refused */
        if ((uint64_t)e->off + e->csz > ioff) { erri(err, cap, LJ_LJB_ERR_ENTRY, i); goto bad; }   /* inside the data region */
        if ((total += e->usz) > LJ_LJB_TOTAL_MAX) { errs(err, cap, LJ_LJB_ERR_SIZE); goto bad; }   /* before any entry is allocated */
        if (!lj_ljb_path_ok(e->folder, e->name)) { erri(err, cap, LJ_LJB_ERR_PATH, i); goto bad; }
        for (j = 0; j < i; j++)   /* ponytail: O(n^2) over <= 8192 entries; a hash when backups grow */
            if (ci_eq(out->e[j].folder, e->folder) && ci_eq(out->e[j].name, e->name)) { erri(err, cap, LJ_LJB_ERR_PATH, i); goto bad; }
    }
    free(idx);
    out->data = data; out->len = len; out->n = n;
    return 0;
bad:
    free(idx);
    lj_ljb_free(out);
    return -1;
}

int lj_ljb_extract(const lj_ljb *b, uint32_t i, uint8_t **out, size_t *n, char *err, size_t cap)
{
    const lj_ljb_entry *e;
    uint8_t *buf;
    size_t got;
    if (err && cap) err[0] = 0;
    if (!out || !n) return -1;
    *out = NULL; *n = 0;
    if (!b || !b->e || i >= b->n) return erri(err, cap, LJ_LJB_ERR_ENTRY, i);
    e = &b->e[i];
    if (!(buf = (uint8_t *)malloc((size_t)e->usz + 1))) lj_fatal("lj_ljb_extract: out of memory");
    if (e->flags == LJ_LJB_FLAG_STORED) { memcpy(buf, b->data + e->off, e->csz); got = e->csz; }
    else got = tinfl_decompress_mem_to_mem(buf, (size_t)e->usz + 1, b->data + e->off, e->csz, TINFL_FLAG_PARSE_ZLIB_HEADER);   /* capped at usz: one byte more fails the compare */
    if (got != e->usz || (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, got) != e->crc) { free(buf); return erri(err, cap, LJ_LJB_ERR_ENTRY, i); }
    *out = buf; *n = got;
    return 0;
}

/* ---- conversion (W5-LJB-3) ---- */

static uint8_t *slurp(const char *path, size_t *len, char *err, size_t cap)
{
    FILE *f = path ? fopen(path, "rb") : NULL;
    long sz;
    uint8_t *b = NULL;
    if (!f) { errs(err, cap, LJ_LJB_ERR_READ); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); errs(err, cap, LJ_LJB_ERR_READ); return NULL; }
    if ((unsigned long)sz > LJ_LJB_FILE_MAX) { fclose(f); errs(err, cap, LJ_LJB_ERR_SIZE); return NULL; }   /* before the read */
    if (!(b = (uint8_t *)malloc((size_t)sz + 1))) lj_fatal("lj_ljb: out of memory");
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); b = NULL; errs(err, cap, LJ_LJB_ERR_READ); }
    fclose(f);
    *len = (size_t)sz;
    return b;
}

/* a library folder name from Info.$$$ (or the file name): the lj_library name rules, CP1252 kept */
static void lib_name(char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    for (; *src && n + 1 < cap; src++) if ((unsigned char)*src >= 0x20 && !strchr("\\/:*?\"<>|", *src)) dst[n++] = *src;
    while (n && (dst[n - 1] == ' ' || dst[n - 1] == '.')) n--;
    dst[n] = 0;
    while (dst[0] == ' ') memmove(dst, dst + 1, strlen(dst));
    if (!comp_ok(dst, strlen(dst))) lj_str_copy(dst, cap, "LJB Import");   /* PROPOSAL fallback name */
}

static int find(const lj_ljb *b, const char *folder, const char *name)
{
    uint32_t i;
    for (i = 0; i < b->n; i++) if (ci_eq(b->e[i].folder, folder) && ci_eq(b->e[i].name, name)) return (int)i;
    return -1;
}

/* Info.$$$ name and Notes.$$$ text (18 s1.4); either may be absent */
static void info_notes(const lj_ljb *b, char *name, size_t ncap, char *notes, size_t tcap)
{
    uint8_t *x = NULL;
    size_t n = 0;
    int i;
    if (name && ncap) name[0] = 0;
    if (notes && tcap) notes[0] = 0;
    if (name && ncap && (i = find(b, "", "Info.$$$")) >= 0 && lj_ljb_extract(b, (uint32_t)i, &x, &n, NULL, 0) == 0 && n >= LJB_INFO_MIN) {
        char raw[256];
        sstr(raw, sizeof raw, x + 0x104, 256);
        if (raw[0]) lib_name(name, ncap, raw);
    }
    free(x); x = NULL;
    if (notes && tcap && (i = find(b, "", "Notes.$$$")) >= 0 && lj_ljb_extract(b, (uint32_t)i, &x, &n, NULL, 0) == 0)
        lj_str_rtf_strip(notes, tcap, (const char *)x, n);                  /* truncated text is kept (-1 ignored) */
    free(x);
}

int lj_ljb_preview(const char *ljb_path, char *name, size_t ncap, char *notes, size_t tcap, char *err, size_t cap)
{
    size_t len = 0;
    uint8_t *d = slurp(ljb_path, &len, err, cap);
    lj_ljb b;
    if (!d) return -1;
    if (lj_ljb_open(&b, d, len, err, cap) != 0) { free(d); return -1; }
    info_notes(&b, name, ncap, notes, tcap);
    lj_ljb_free(&b);
    free(d);
    return 0;
}

typedef struct { lj_ljb_report *rep; FILE *f; } lj_ljb_out;

static void note(lj_ljb_out *o, const lj_ljb_entry *e, const char *fmt, ...) LJ_PRINTF(3, 4);
static void note(lj_ljb_out *o, const lj_ljb_entry *e, const char *fmt, ...)
{
    char why[160], line[sizeof o->rep->lines[0]];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, sizeof why, fmt, ap);
    va_end(ap);
    if (o->f) fprintf(o->f, "%s%s: %s\r\n", e ? e->folder : "", e ? e->name : "", why);
    if (o->rep->nlines < (int)(sizeof o->rep->lines / sizeof o->rep->lines[0])) {
        snprintf(line, sizeof line, "%.40s%.40s: %s", e ? e->folder : "", e ? e->name : "", why);
        lj_str_copy(o->rep->lines[o->rep->nlines++], sizeof line, line);
    }
}

/* one 6,312-byte scene record (18 s4.2): times in ms, fade state per channel at +0x10, value at +0xADC; the 700- and
 * 800-byte tails are undecoded and ignored. Off / Snap / Fade = 0 / 1 / 2 is inferred: PROPOSAL rig 18 s4. */
static int scene_rd(const uint8_t *p, lj_scene *s)
{
    int i;
    lj_scene_init(s);
    s->scene_ms = rd32(p); s->fade_ms = rd32(p + 4);
    memcpy(s->state, p + 0x10, LJ_CHANS);
    memcpy(s->val, p + 0xADC, LJ_CHANS);
    if (s->scene_ms > LJ_TIME_MAX_MS || s->fade_ms > LJ_TIME_MAX_MS) return -1;
    for (i = 0; i < LJ_CHANS; i++) if (s->state[i] > LJ_FADE) return -1;
    return 0;
}

static int num8(const char *name, const char *ext, uint32_t *num)   /* "00000012" + ext -> 12 */
{
    uint32_t v = 0;
    int i;
    if (strlen(name) != 8 + strlen(ext) || !ci_eq(name + 8, ext)) return 0;
    for (i = 0; i < 8; i++) { if (name[i] < '0' || name[i] > '9') return 0; v = v * 10 + (uint32_t)(name[i] - '0'); }
    *num = v;
    return v >= 1 && v <= LJ_NUMBER_MAX;
}

enum { K_SKIP, K_INFO, K_NOTES, K_CONFIG, K_GROUPS, K_DEFOUT, K_STATICS, K_PAGES, K_SEQ, K_CUE, K_CMA, K_GS2, K_LJM };

static int kind_of(const lj_ljb_entry *e, uint32_t *num, const char **why)
{
    const char *f = e->folder, *n = e->name;
    *why = "not converted";
    if (!f[0]) {
        if (ci_eq(n, "Info.$$$")) return K_INFO;
        if (ci_eq(n, "Notes.$$$")) return K_NOTES;
        if (ci_eq(n, "Config.dat")) return K_CONFIG;
        if (ci_eq(n, "GroupInfo.dat")) return K_GROUPS;
        if (ci_eq(n, "DefaultOut.dat")) return K_DEFOUT;
        if (ci_eq(n, "Statics.dat")) return K_STATICS;
        if (ci_eq(n, "Special.dat") || ci_eq(n, "Scanner1.dat") || ci_eq(n, "Patch2.Cfg")) *why = "not converted (contents undecoded, note 18 s11)";
        return K_SKIP;
    }
    if (ci_eq(f, "Cues\\") && ci_eq(n, "CuePageInfo.Dat")) return K_PAGES;
    if (ci_eq(f, "Cues\\") && num8(n, ".cue", num)) return K_CUE;
    if (ci_eq(f, "Sequences\\") && num8(n, ".seq", num)) return K_SEQ;
    if (ci_eq(f, "CueMacros\\") && num8(n, ".CMA", num)) return K_CMA;
    if (ci_eq(f, "UserFixtureDef\\") && strlen(n) >= 12 && strlen(n) <= 13) {
        char head[12];
        memcpy(head, n, 11); head[11] = 0;
        if (ci_eq(head, "FixtureDef.") && n[11] >= '0' && n[11] <= '9' && (!n[12] || (n[12] >= '0' && n[12] <= '9'))) {
            *num = (uint32_t)(n[11] - '0'); if (n[12]) *num = *num * 10 + (uint32_t)(n[12] - '0');   /* FixtureDef.<k>, k = slot - 1 (18 s2) */
            return K_GS2;
        }
    }
    if (strlen(f) >= 8 && strncmp(f, "LJMData\\", 8) == 0) return K_LJM;
    if (ci_eq(f, "CueLists\\")) *why = "cue list not converted (line records undecoded, note 18 s11)";
    else if (ci_eq(f, "BGCues\\")) *why = "background cue not converted (format undecoded, note 18 s2)";
    else if (ci_eq(f, "UserFixtureDef\\")) *why = "custom palettes not converted (note 18 s10)";
    else if (ci_eq(f, "Presets\\") || ci_eq(f, "Palettes\\") || (strlen(n) > 4 && ci_eq(n + strlen(n) - 4, ".Pre"))) *why = "presets not converted";
    return K_SKIP;
}

static const char *conv_config(lj_ljb_out *o, const lj_ljb_entry *e, const uint8_t *b, size_t n, lj_show *show, const char gs2[LJ_FIXTURES][64])
{
    int i, got = 0;
    if (n != LJB_CONFIG) return "wrong size, not converted";
    for (i = 0; i < LJ_FIXTURES; i++) {
        const uint8_t *r = b + LJB_CONFIG_BASE + i * LJB_CONFIG_REC;
        lj_fixture *f = &show->fixture[i];
        unsigned type = rd16(r), code = rd16(r + 2);
        int32_t x = (int32_t)rd32(r + 0x21), y = (int32_t)rd32(r + 0x25);
        if (!type && !code) continue;                                       /* an empty slot is all zero (02 s9.2) */
        if (code >= LJ_CHANS) { note(o, e, "fixture %d: address code %u not 0..2047, not converted", i + 1, code); continue; }
        if (!gs2[i][0] && !lj_fixlib_by_id((uint16_t)type)) { note(o, e, "fixture %d: type %u has no LJ4EVR profile, not converted", i + 1, type); continue; }   /* ME-8: never renumbered */
        memset(f, 0, sizeof *f);
        f->kind = gs2[i][0] ? LJ_FX_GS2 : LJ_FX_BUILTIN;
        f->profile_id = (uint16_t)type;                                     /* ME-8: the exe row id as stored */
        lj_str_copy(f->gs2_file, sizeof f->gs2_file, gs2[i]);
        f->link = (uint8_t)(code / LJ_LINK_CH + 1); f->address = (uint16_t)(code % LJ_LINK_CH + 1);   /* code = (link-1)*512 + address-1, PROPOSAL rig 18 s3 */
        sstr(f->user_name, sizeof f->user_name, r + 0x0B, 21);
        f->placed = r[0x20] != 0;                                           /* PROPOSAL rig 18 s8 (02 s9.2 unverified) */
        f->icon_x = (int16_t)(x < 0 ? 0 : x > 32767 ? 32767 : x);
        f->icon_y = (int16_t)(y < 0 ? 0 : y > 32767 ? 32767 : y);
        f->tab = (uint8_t)(r[0x29] ? r[0x29] - 1 : 0);                      /* LJ2 1..4 = Tabs.Ini TAB0..TAB3, PROPOSAL rig 18 s8 */
        got++;
    }
    if (got == 0) note(o, e, "no fixture converted");
    return NULL;
}

static const char *conv_groups(const uint8_t *b, size_t n, lj_group g[LJ_USER_GROUPS])
{
    int k, j;
    if (n != LJB_GROUPS) return "wrong size, not converted";
    for (k = 0; k < LJ_USER_GROUPS; k++) {
        const uint8_t *r = b + 2 + k * LJB_GROUP_REC;
        sstr(g[k].name, sizeof g[k].name, r, 31);
        for (j = 0; j < LJ_FIXTURES; j++) g[k].member[j] = r[31 + j] != 0;   /* one flag per slot, PROPOSAL rig 18 s8 */
    }
    return lj_groups_validate(g);
}

static const char *conv_statics(lj_ljb_out *o, const lj_ljb_entry *e, const uint8_t *b, size_t n, lj_show *show)
{
    static lj_scene sc;   /* UI thread */
    const uint8_t *tail = b + 2 + 20 * LJB_SCENE;
    int k, pg;
    if (n != LJB_STATICS) return "wrong size, not converted";
    if ((pg = lj_statics_add_page(&show->pages, &show->npages, "Page 1")) < 0) return "no page free";   /* LJ2's 20 statics = one page, PROPOSAL rig 18 s6 */
    for (k = 0; k < 20; k++) {
        lj_static_slot *s = &show->pages[pg].slot[k];
        sstr(s->name, sizeof s->name, tail + 40 + k * 31, 31);
        s->level_pct = 100; s->fade_ms = LJ_FADE_TIME_DEFAULT_MS;
        if (scene_rd(b + 2 + k * LJB_SCENE, &sc) != 0) { note(o, e, "static %d: times or fade states out of range, not converted", k + 1); continue; }
        s->fade_ms = sc.fade_ms;                                            /* the entry's recorded fade (18 s6) */
        if (lj_scene_is_programmed(&sc)) lj_static_slot_record(s, &sc);
    }
    note(o, e, "static modes, levels and replace rules not converted (tail undecoded, note 18 s6)");
    return lj_statics_validate(show->pages, show->npages);
}

static const char *conv_seq(const uint8_t *b, size_t n, uint32_t num, lj_sequence *q)
{
    unsigned cnt, k;
    if (n < LJB_SEQ_HDR + LJB_SCENE) return "wrong size, not converted";
    cnt = b[0x5B];                                                          /* scene count, PROPOSAL rig 18 s4.1 */
    if (!cnt || n != LJB_SEQ_HDR + (size_t)cnt * LJB_SCENE) return "size does not match the scene count, not converted";
    memset(q, 0, sizeof *q);
    q->number = num;
    sstr(q->name, sizeof q->name, b + 0x10, 36);                            /* string[35]; the two TDateTimes are not imported (no double in model/) */
    if (lj_sequence_alloc(q, (uint16_t)cnt) != 0) return "cannot allocate";
    for (k = 0; k < cnt; k++) if (scene_rd(b + LJB_SEQ_HDR + (size_t)k * LJB_SCENE, &q->scenes[k]) != 0) return "times or fade states out of range, not converted";
    return lj_sequence_validate(q);
}

static const char *conv_cma(const uint8_t *b, size_t n, uint32_t num, lj_genmacro *m)
{
    int i;
    if (n != LJB_CMA) return "wrong size, not converted";
    memset(m, 0, sizeof *m);
    m->number = num;
    sstr(m->name, sizeof m->name, b + 4, 31);
    for (i = 0; i < LJ_CHANS; i++) {   /* 11-byte channel records at 0x1023, roles by plausibility: PROPOSAL rig 18 s9 */
        const uint8_t *p = b + LJB_CMA_HDR + i * LJB_CMA_REC;
        lj_gm_param *c = &m->ch[i];
        uint16_t cyc = rd16(p + 9);
        c->shape = p[0]; c->base_on = p[1] != 0; c->base = p[2];
        c->amp_pos = rds16(p + 3); c->amp_neg = rds16(p + 5); c->delay = rd16(p + 7);
        c->cycle_ds = cyc > LJ_GM_CYCLE_MAX_DS ? LJ_GM_CYCLE_MAX_DS : cyc;   /* LJ2's cycle index copied as deciseconds, PROPOSAL rig 18 s9 */
        c->dir = LJ_DIR_FWD;                                                /* where direction lives is undecoded */
    }
    return lj_genmacro_validate(m);
}

static const char *conv_cue(const uint8_t *b, size_t n, uint32_t num, lj_cue *c)
{
    uint32_t seq;
    if (n != LJB_CUE) return "wrong size, not converted";
    c->number = num;
    sstr(c->name, sizeof c->name, b, 31);
    seq = rd32(b + 0x90);
    if (seq >= 1 && seq <= LJ_NUMBER_MAX && lj_cue_add_sequence(c, seq, 1) == 1) {
        c->slot[0].enabled = b[0x118] != 0;                                 /* PROPOSAL rig 18 s7 */
        c->slot[0].trig = b[0x94] <= LJ_TRIG_AUDIO ? b[0x94] : LJ_TRIG_AUTO; /* the trig byte, PROPOSAL rig 18 s7 */
    }
    return lj_cue_validate(c);
}

static const char *conv_pages(const uint8_t *b, size_t n, lj_cuepages *p)
{
    uint32_t cnt, k;
    if (n < 6) return "wrong size, not converted";
    cnt = rd32(b + 2);
    if (cnt > LJ_CUE_PAGES || n != 6 + (size_t)cnt * LJB_PAGE_REC) return "size does not match the page count, not converted";   /* 35 B per page, PROPOSAL rig 18 s8 */
    lj_cuepages_init(p);
    for (k = 0; k < cnt; k++) sstr(p->page[k].name, sizeof p->page[k].name, b + 6 + k * LJB_PAGE_REC, LJB_PAGE_REC);
    p->n = (int)cnt;
    return lj_cuepages_validate(p);
}

static int lib_file(const lj_library *lib, const char *rel, char *path) { return lj_library_path(lib, rel, path, LJ_PATH_CAP); }

int lj_ljb_import(const char *ljb_path, const char *userdir, const char *name, lj_library *out, lj_ljb_report *rep, char *err, size_t cap)
{
    static char gs2[LJ_FIXTURES][64];
    static lj_sequence q;
    char want[LJ_LIBNAME_CAP], try_name[LJ_LIBNAME_CAP], path[LJ_PATH_CAP], rel[2 * LJ_LJB_NAME], *notes = NULL;
    uint32_t next[LJ_REC_KIND_COUNT] = { 0 }, num, i;
    uint8_t *d = NULL, *x = NULL;
    size_t len = 0, xn;
    int k, created = 0, ok = 0, ljm = 0, config = -1, kind;
    lj_ljb b;
    lj_ljb_out o;
    lj_show *show = NULL;
    lj_cuepages *pages = NULL;
    lj_genmacro *m = NULL;
    lj_cue *c = NULL;
    lj_ini *ini;
    const char *why;
    memset(&b, 0, sizeof b);
    o.rep = rep; o.f = NULL;
    if (err && cap) err[0] = 0;
    if (!rep || !out || !userdir) return -1;
    memset(rep, 0, sizeof *rep);
    if (!(d = slurp(ljb_path, &len, err, cap)) || lj_ljb_open(&b, d, len, err, cap) != 0) goto done;

    /* the name: given, Info.$$$, or the file name; "name (2)" .. when taken (never an existing library, never Default) */
    want[0] = 0;
    if (name) lib_name(want, sizeof want, name);
    else info_notes(&b, want, sizeof want, NULL, 0);
    if (!want[0]) {
        const char *base = ljb_path, *s, *dot;
        for (s = ljb_path; *s; s++) if (*s == '/' || *s == '\\') base = s + 1;
        dot = strrchr(base, '.');
        snprintf(try_name, sizeof try_name, "%.*s", (int)(dot ? (size_t)(dot - base) : strlen(base)), base);
        lib_name(want, sizeof want, try_name);
    }
    lj_str_copy(try_name, sizeof try_name, want);
    for (k = 2; ci_eq(try_name, "Default") || lj_library_open(userdir, try_name, out) == 0; k++)
        if (k > 99 || snprintf(try_name, sizeof try_name, "%.50s (%d)", want, k) >= (int)sizeof try_name) { errs(err, cap, LJ_LJB_ERR_EXISTS); goto done; }
    if (lj_library_create(userdir, try_name, out) != 0) { errs(err, cap, LJ_LJB_ERR_EXISTS); goto done; }
    created = 1;
    for (i = 0; i < b.n; i++) {   /* W5-LJB-2: every name the file carries would fit inside the library (nothing is written under it) */
        snprintf(rel, sizeof rel, "%s%s", b.e[i].folder, b.e[i].name);
        if (lib_file(out, rel, path) != 0) { erri(err, cap, LJ_LJB_ERR_PATH, i); goto done; }
    }
    if (lib_file(out, LJ_LJB_REPORT, path) != 0 || (o.f = fopen(path, "wb")) == NULL) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    fprintf(o.f, "LJB import report\r\nFiles not converted, and what a converted file lost:\r\n");

    show = lj_show_new();
    if (!(pages = (lj_cuepages *)malloc(sizeof *pages)) || !(notes = (char *)malloc(LJ_NOTES_CAP))) lj_fatal("lj_ljb_import: out of memory");
    lj_cuepages_init(pages);
    memset(gs2, 0, sizeof gs2);
    for (i = 0; i < b.n; i++) {
        const lj_ljb_entry *e = &b.e[i];
        num = 0;
        if (lj_ljb_extract(&b, i, &x, &xn, err, cap) != 0) goto done;       /* every entry size + CRC: a bad one refuses the whole file */
        kind = kind_of(e, &num, &why);
        if (kind == K_SKIP || kind == K_LJM || kind == K_INFO || kind == K_NOTES || kind == K_CONFIG) {
            free(x); x = NULL;
            if (kind == K_SKIP) { note(&o, e, "%s", why); rep->skipped++; }
            if (kind == K_LJM) { ljm++; rep->skipped++; }
            if (kind == K_CONFIG) config = (int)i;                          /* after the FixtureDef entries */
            continue;                                                       /* Info.$$$ / Notes.$$$: the name and the notes, below */
        }
        why = NULL;
        switch (kind) {
        case K_GROUPS: why = conv_groups(x, xn, show->user_group); break;
        case K_DEFOUT: if (xn == LJB_BLOCK) memcpy(show->default_out, x, LJ_CHANS); else why = "wrong size, not converted"; break;   /* 18 s5 */
        case K_STATICS: why = conv_statics(&o, e, x, xn, show); break;
        case K_PAGES:
            if ((why = conv_pages(x, xn, pages)) == NULL) note(&o, e, "page names converted; the cues on each page are undecoded");
            break;
        case K_SEQ:
            if ((why = conv_seq(x, xn, num, &q)) == NULL && (lj_library_record_path(out, LJ_REC_SEQ, num, path, sizeof path) != 0 || lj_sequence_save(path, &q) != 0)) why = "cannot write";
            lj_sequence_free_scenes(&q);
            if (!why && num >= next[LJ_REC_SEQ]) next[LJ_REC_SEQ] = num + 1;
            break;
        case K_CUE:
            c = lj_cue_new_rec();
            if ((why = conv_cue(x, xn, num, c)) == NULL && (lj_library_record_path(out, LJ_REC_CUE, num, path, sizeof path) != 0 || lj_cue_save(path, c) != 0)) why = "cannot write";
            lj_cue_free(c); c = NULL;
            if (!why) { note(&o, e, "slots 2-12 and the cue options not converted (note 18 s7)"); if (num >= next[LJ_REC_CUE]) next[LJ_REC_CUE] = num + 1; }
            break;
        case K_CMA:
            m = lj_genmacro_new_rec();
            if ((why = conv_cma(x, xn, num, m)) == NULL && (lj_library_record_path(out, LJ_REC_MACRO, num, path, sizeof path) != 0 || lj_genmacro_save(path, m) != 0)) why = "cannot write";
            free(m); m = NULL;
            if (!why && num >= next[LJ_REC_MACRO]) next[LJ_REC_MACRO] = num + 1;
            break;
        case K_GS2: {
            lj_profile p;
            FILE *f;
            memset(&p, 0, sizeof p);
            if (num >= LJ_FIXTURES || lj_gs2_read(x, xn, &p, NULL, 0) != 0) { why = "not a GS2 record, not converted"; break; }
            lj_gs2_free(&p);
            snprintf(gs2[num], sizeof gs2[num], "LJBSlot%03u.GS2", (unsigned)num + 1);   /* our name, never the file's */
            snprintf(rel, sizeof rel, "UserFixtureDef\\%s", gs2[num]);
            if (lib_file(out, rel, path) != 0 || (f = fopen(path, "wb")) == NULL) { gs2[num][0] = 0; why = "cannot write"; break; }
            if (fwrite(x, 1, xn, f) != xn) why = "cannot write";
            if (fclose(f) != 0) why = "cannot write";
            if (why) gs2[num][0] = 0;
            break; }
        default: break;
        }
        free(x); x = NULL;
        if (why) { note(&o, e, "%s", why); rep->skipped++; } else rep->converted++;
    }
    if (config >= 0) {
        if (lj_ljb_extract(&b, (uint32_t)config, &x, &xn, err, cap) != 0) goto done;
        why = conv_config(&o, &b.e[config], x, xn, show, (const char (*)[64])gs2);
        free(x); x = NULL;
        if (why) { note(&o, &b.e[config], "%s", why); rep->skipped++; } else rep->converted++;
    }
    if (ljm) { fprintf(o.f, "LJMData\\: %d files not converted (LJ Manager data, out of scope)\r\n", ljm);
               if (rep->nlines < 64) snprintf(rep->lines[rep->nlines++], sizeof rep->lines[0], "LJMData\\: %d files not converted (LJ Manager data)", ljm); }

    info_notes(&b, NULL, 0, notes, LJ_NOTES_CAP);
    if (lj_show_save_config(show, out) != 0) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    if (show->npages && (lib_file(out, "Statics.dat", path) != 0 || lj_statics_save(path, show->pages, show->npages) != 0)) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    if (pages->n && (lib_file(out, "Cues\\CuePageInfo.Dat", path) != 0 || lj_cuepages_save(path, pages) != 0)) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    if (notes[0] && (lib_file(out, LJ_NOTES_FILE, path) != 0 || lj_notes_save(path, notes) != 0)) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    if (lib_file(out, "DirInfo.Ini", path) == 0 && (ini = lj_ini_load(path)) != NULL) {   /* the numbering high-water marks past every imported record (design 5.2) */
        static const char *const key[LJ_REC_KIND_COUNT] = { "Seq", "Cue", "Cl", "Bg", "Macro" };
        for (k = 0; k < LJ_REC_KIND_COUNT; k++) if (next[k]) lj_ini_seti(ini, "NEXT", key[k], (int)next[k]);
        k = lj_ini_save(ini, path);
        lj_ini_free(ini);
        if (k != 0) { errs(err, cap, LJ_LJB_ERR_READ); goto done; }
    }
    fprintf(o.f, "%d converted, %d not converted\r\n", rep->converted, rep->skipped);
    ok = 1;
done:
    if (o.f && fclose(o.f) != 0) ok = 0;
    if (!ok && created) lj_library_delete(userdir, out->name);              /* nothing left behind */
    lj_show_free(show);                                                     /* private: the engine never saw it */
    free(x); free(pages); free(notes); free(d);
    lj_ljb_free(&b);
    return ok ? 0 : -1;
}
