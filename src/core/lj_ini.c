/* lj_ini.c - Windows/Delphi-style INI (design 2.1): [section] and key=value lines,
 * CP1252 bytes untouched, case-insensitive section/key match (TIniFile), CRLF or
 * LF in, CRLF out. The document is the file's lines in order; comments, blank
 * lines and keys nobody asked for survive a load/save round trip. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_types.h"

#define LJ_INI_LINE_MAX 1024

struct lj_ini {
    char **line;        /* each malloc'd, no line terminator */
    int    n, cap;
};

static int lj_ini_ieq(const char *a, const char *b, size_t n)      /* a[0..n) == b, ASCII case-folded */
{
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y || !y) return 0;
    }
    return b[n] == 0;
}

static char *lj_ini_dup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) lj_fatal("lj_ini: out of memory");
    memcpy(p, s, n);
    return p;
}

static void lj_ini_insert(lj_ini *ini, int at, char *text)       /* takes ownership of text */
{
    if (ini->n == ini->cap) {
        ini->cap = ini->cap ? ini->cap * 2 : 32;
        ini->line = (char **)realloc(ini->line, (size_t)ini->cap * sizeof *ini->line);
        if (!ini->line) lj_fatal("lj_ini: out of memory");
    }
    memmove(ini->line + at + 1, ini->line + at, (size_t)(ini->n - at) * sizeof *ini->line);
    ini->line[at] = text;
    ini->n++;
}

static int lj_ini_is_section(const char *l, const char *sec)      /* "[sec]" */
{
    size_t n = strlen(sec);
    return l[0] == '[' && lj_ini_ieq(l + 1, sec, n) && l[1 + n] == ']';
}

/* Index of "key=" inside [sec], or -1; *sec_end = index of the first line after the section (ini->n when none). */
static int lj_ini_find(lj_ini *ini, const char *sec, const char *key, int *sec_end)
{
    int i, in = 0, found = -1;
    size_t kn = strlen(key);
    *sec_end = -1;
    for (i = 0; i < ini->n; i++) {
        const char *l = ini->line[i];
        if (l[0] == '[') {
            if (in) { *sec_end = i; break; }
            in = lj_ini_is_section(l, sec);
        } else if (in && found < 0 && lj_ini_ieq(l, key, kn) && l[kn] == '=')
            found = i;
    }
    if (in && *sec_end < 0) *sec_end = ini->n;
    return found;
}

lj_ini *lj_ini_new(void)
{
    lj_ini *ini = (lj_ini *)calloc(1, sizeof *ini);
    if (!ini) lj_fatal("lj_ini: out of memory");
    return ini;
}

lj_ini *lj_ini_load(const char *path)
{
    char buf[LJ_INI_LINE_MAX + 2];
    lj_ini *ini;
    FILE *f = fopen(path, "rb");
    if (!f) { lj_log("ini: cannot open %s", path); return NULL; }
    ini = lj_ini_new();
    while (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        if (n && buf[n - 1] != '\n' && !feof(f)) {
            lj_log("ini: %s line %d longer than %d bytes", path, ini->n + 1, LJ_INI_LINE_MAX);
            lj_ini_free(ini); fclose(f); return NULL;
        }
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        lj_ini_insert(ini, ini->n, lj_ini_dup(buf));
    }
    fclose(f);
    return ini;
}

int lj_ini_save(lj_ini *ini, const char *path)
{
    int i, ok = 1;
    FILE *f = fopen(path, "wb");
    if (!f) { lj_log("ini: cannot write %s", path); return -1; }
    for (i = 0; i < ini->n && ok; i++)
        ok = fputs(ini->line[i], f) >= 0 && fputs("\r\n", f) >= 0;
    if (fclose(f) != 0 || !ok) { lj_log("ini: write failed %s", path); return -1; }
    return 0;
}

const char *lj_ini_gets(lj_ini *ini, const char *sec, const char *key, const char *dflt)
{
    int end, i = lj_ini_find(ini, sec, key, &end);
    return i < 0 ? dflt : ini->line[i] + strlen(key) + 1;
}

int lj_ini_geti(lj_ini *ini, const char *sec, const char *key, int dflt)
{
    const char *v = lj_ini_gets(ini, sec, key, NULL);
    char *endp;
    long n;
    if (!v || !*v) return dflt;
    n = strtol(v, &endp, 10);
    return *endp ? dflt : (int)n;
}

void lj_ini_sets(lj_ini *ini, const char *sec, const char *key, const char *v)
{
    int end, i = lj_ini_find(ini, sec, key, &end);
    size_t kn = strlen(key), vn = strlen(v);
    char *text = (char *)malloc(kn + 1 + vn + 1);
    if (!text) lj_fatal("lj_ini: out of memory");
    memcpy(text, i >= 0 ? ini->line[i] : key, kn);           /* an existing key keeps the file's spelling */
    text[kn] = '='; memcpy(text + kn + 1, v, vn + 1);
    if (i >= 0) { free(ini->line[i]); ini->line[i] = text; return; }
    if (end < 0) {                                              /* no such section: "[sec]" at the end */
        char *h = (char *)malloc(strlen(sec) + 3);
        if (!h) lj_fatal("lj_ini: out of memory");
        sprintf(h, "[%s]", sec);
        lj_ini_insert(ini, ini->n, h);
        end = ini->n;
    }
    lj_ini_insert(ini, end, text);                              /* last line of the section */
}

void lj_ini_seti(lj_ini *ini, const char *sec, const char *key, int v)
{
    char buf[16];
    sprintf(buf, "%d", v);
    lj_ini_sets(ini, sec, key, buf);
}

void lj_ini_free(lj_ini *ini)
{
    int i;
    if (!ini) return;
    for (i = 0; i < ini->n; i++) free(ini->line[i]);
    free(ini->line);
    free(ini);
}
