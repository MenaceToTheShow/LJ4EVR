/* lj_notes.c - the Notes window text (lj_notes.h): <library>\Notes.txt, CP1252 bytes as
 * typed (binary I/O, no line-end or encoding conversion, D6). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <string.h>
#include "model/lj_notes.h"
#include "core/lj_log.h"

int lj_notes_load(const char *path, char *buf, size_t cap)
{
    FILE *f;
    size_t n;
    int more;
    if (!buf || !cap) return -1;
    buf[0] = 0;
    f = fopen(path, "rb");
    if (!f) return 0;                                   /* missing file = empty text */
    n = fread(buf, 1, cap - 1, f);
    buf[n] = 0;
    more = fgetc(f) != EOF;
    if (ferror(f)) { fclose(f); buf[0] = 0; lj_log("notes: cannot read %s", path); return -1; }
    fclose(f);
    if (more) { lj_log("notes: %s is longer than %u bytes, truncated", path, (unsigned)(cap - 1)); return -1; }
    return 0;
}

int lj_notes_save(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    size_t n = text ? strlen(text) : 0;
    int ok;
    if (!f) { lj_log("notes: cannot write %s", path); return -1; }
    ok = fwrite(text ? text : "", 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (!ok) lj_log("notes: write to %s failed", path);
    return ok ? 0 : -1;
}
