/* lj_textrec.c - the LJ4 text record reader/writer (design 2.1, grammar 5.2).
 * Line 1 "LJ4 <kind> <format>"; then one item per line. The reader splits a line
 * at its first '=' or ' ', whichever comes first, so "num=12" yields ("num", "12")
 * and the list item "scene st=1000 ft=50" yields ("scene", "st=1000 ft=50"); the
 * caller sscanf's the value. Blank lines and lines starting with '#' are skipped.
 * The writer prints key, '=' unless the key ends with ' ' (a list head: pass
 * "scene " and the format "st=%d ft=%d"), then the formatted value. Lines are
 * LF-terminated, bytes are CP1252 and never converted. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_textrec.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

#define LJ_REC_LINE_MAX 8192   /* ponytail: one "ch" line of ~680 channels; the model splits longer scenes over several "ch" lines */
#define LJ_REC_KIND_MAX 32

struct lj_rec {
    FILE *f;
    int   err, lineno;
    char  path[260];
    char  line[LJ_REC_LINE_MAX + 2];
};

static lj_rec *lj_rec_new(const char *path, FILE *f)
{
    lj_rec *rec = (lj_rec *)calloc(1, sizeof *rec);
    if (!rec) lj_fatal("lj_rec: out of memory");
    rec->f = f;
    lj_str_copy(rec->path, sizeof rec->path, path);
    return rec;
}

/* Next line into rec->line without its terminator: 1, 0 at EOF, -1 when too long or a control byte. */
static int lj_rec_getline(lj_rec *rec)
{
    size_t n, i;
    if (!fgets(rec->line, sizeof rec->line, rec->f)) return 0;
    rec->lineno++;
    n = strlen(rec->line);
    if (n && rec->line[n - 1] != '\n' && !feof(rec->f)) {
        lj_log("rec: %s line %d longer than %d bytes", rec->path, rec->lineno, LJ_REC_LINE_MAX);
        return -1;
    }
    while (n && (rec->line[n - 1] == '\n' || rec->line[n - 1] == '\r')) rec->line[--n] = 0;
    for (i = 0; i < n; i++)
        if ((unsigned char)rec->line[i] < 0x20 && rec->line[i] != '\t') {
            lj_log("rec: %s line %d has control byte 0x%02x", rec->path, rec->lineno, (unsigned char)rec->line[i]);
            return -1;
        }
    return 1;
}

lj_rec *lj_rec_open_read(const char *path, const char *kind, int *format)
{
    char got[LJ_REC_KIND_MAX + 1], extra;
    lj_rec *rec;
    FILE *f = fopen(path, "rb");
    if (!f) { lj_log("rec: cannot open %s", path); return NULL; }
    rec = lj_rec_new(path, f);
    if (lj_rec_getline(rec) != 1 || strncmp(rec->line, "LJ4 ", 4) != 0
        || sscanf(rec->line + 4, "%32s %d %c", got, format, &extra) != 2 || strcmp(got, kind) != 0 || *format < 0) {
        lj_log("rec: %s is not an \"LJ4 %s <format>\" file", path, kind);
        lj_rec_close(rec);
        return NULL;
    }
    return rec;
}

int lj_rec_next(lj_rec *rec, char *key, size_t kcap, char *val, size_t vcap)
{
    int r;
    for (;;) {
        size_t klen;
        const char *l;
        r = lj_rec_getline(rec);
        if (r <= 0) return r;
        l = rec->line;
        if (!*l || *l == '#') continue;
        klen = strcspn(l, "= ");
        if (klen == 0 || !l[klen] || klen >= kcap) {
            lj_log("rec: %s line %d: bad line \"%.40s\"", rec->path, rec->lineno, l);
            return -1;
        }
        memcpy(key, l, klen); key[klen] = 0;
        if (lj_str_copy(val, vcap, l + klen + 1) != 0) {
            lj_log("rec: %s line %d: value of %s longer than %u", rec->path, rec->lineno, key, (unsigned)vcap);
            return -1;
        }
        return 1;
    }
}

lj_rec *lj_rec_open_write(const char *path, const char *kind, int format)
{
    lj_rec *rec;
    FILE *f = fopen(path, "wb");
    if (!f) { lj_log("rec: cannot write %s", path); return NULL; }
    rec = lj_rec_new(path, f);
    if (fprintf(f, "LJ4 %s %d\n", kind, format) < 0) rec->err = 1;
    return rec;
}

int lj_rec_write(lj_rec *rec, const char *key, const char *fmt, ...)
{
    va_list ap;
    size_t kn = strlen(key);
    int ok = fputs(key, rec->f) >= 0 && (kn && key[kn - 1] == ' ' ? 1 : fputc('=', rec->f) != EOF);
    va_start(ap, fmt);
    ok = ok && vfprintf(rec->f, fmt, ap) >= 0 && fputc('\n', rec->f) != EOF;
    va_end(ap);
    if (!ok) rec->err = 1;
    return ok ? 0 : -1;
}

int lj_rec_close(lj_rec *rec)
{
    int r;
    if (!rec) return 0;
    r = fclose(rec->f) == 0 && !rec->err ? 0 : -1;
    if (r) lj_log("rec: write failed %s", rec->path);
    free(rec);
    return r;
}
