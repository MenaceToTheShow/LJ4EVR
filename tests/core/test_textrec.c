/* test_textrec.c - lj_rec: write then read 1000 records in the 5.2 grammar
 * (key=value, list items, CP1252 bytes), header check, bad lines rejected. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "core/lj_textrec.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void lj_write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

/* Writes the file, opens it as "seq", returns what the first lj_rec_next says (2 = open failed). */
static int lj_bad(const char *text)
{
    char k[32], v[64];
    int fmt, r;
    lj_rec *rec;
    lj_write_text("test_rec_bad.seq", text);
    rec = lj_rec_open_read("test_rec_bad.seq", "seq", &fmt);
    if (!rec) return 2;
    r = lj_rec_next(rec, k, sizeof k, v, sizeof v);
    lj_rec_close(rec);
    return r;
}

int lj_test_textrec(int argc, char **argv);
int lj_test_textrec(int argc, char **argv)
{
    char k[32], v[256], big[9000];
    int i, fmt = -1, n = 0, st, ft, a, b, c;
    lj_rec *rec;
    (void)argc; (void)argv;

    rec = lj_rec_open_write("test_rec.seq", "seq", 1);
    CHECK(rec != NULL);
    if (!rec) return 1;
    CHECK(lj_rec_write(rec, "name", "\"MX-4 Color Chase 1\"") == 0);
    CHECK(lj_rec_write(rec, "type", "%s", "") == 0);
    CHECK(lj_rec_write(rec, "sync", "%d gst=%d gft=%d", 0, 0, 0) == 0);
    CHECK(lj_rec_write(rec, "note", "T\xE9st \xE6\xF8\xE5") == 0);
    for (i = 0; i < 1000; i++) {
        CHECK(lj_rec_write(rec, "scene ", "st=%d ft=%d", i * 10, 50) == 0);
        CHECK(lj_rec_write(rec, "ch ", "1:%d=%d:F 3:11=0:S", i % 512 + 1, i % 256) == 0);
    }
    CHECK(lj_rec_close(rec) == 0);
    CHECK(lj_rec_close(NULL) == 0);

    rec = lj_rec_open_read("test_rec.seq", "seq", &fmt);
    CHECK(rec != NULL && fmt == 1);
    if (!rec) return 1;
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 1 && strcmp(k, "name") == 0 && strcmp(v, "\"MX-4 Color Chase 1\"") == 0);
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 1 && strcmp(k, "type") == 0 && v[0] == 0);
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 1 && strcmp(k, "sync") == 0 && strcmp(v, "0 gst=0 gft=0") == 0);
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 1 && strcmp(k, "note") == 0 && strcmp(v, "T\xE9st \xE6\xF8\xE5") == 0);
    for (i = 0; i < 1000; i++) {
        if (lj_rec_next(rec, k, sizeof k, v, sizeof v) != 1 || strcmp(k, "scene") != 0 || sscanf(v, "st=%d ft=%d", &st, &ft) != 2 || st != i * 10 || ft != 50) break;
        if (lj_rec_next(rec, k, sizeof k, v, sizeof v) != 1 || strcmp(k, "ch") != 0 || sscanf(v, "1:%d=%d:F 3:%d=0:S", &a, &b, &c) != 3 || a != i % 512 + 1 || b != i % 256 || c != 11) break;
        n++;
    }
    CHECK(n == 1000);
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 0);      /* EOF */
    CHECK(lj_rec_next(rec, k, sizeof k, v, sizeof v) == 0);      /* stays at EOF */
    CHECK(lj_rec_close(rec) == 0);

    CHECK(lj_rec_open_read("test_rec.seq", "cue", &fmt) == NULL);          /* kind mismatch */
    CHECK(lj_rec_open_read("test_rec_missing.seq", "seq", &fmt) == NULL);
    CHECK(lj_bad("LJ4 seq x\nnum=1\n") == 2);                               /* format not an int */
    CHECK(lj_bad("LJ4seq 1\n") == 2);
    CHECK(lj_bad("LJ4 seq 1 x\n") == 2);                                    /* trailing junk on the header */
    CHECK(lj_bad("LJ4 seq 1  \nx=1\n") == 1);                               /* trailing blanks are fine */
    CHECK(lj_bad("") == 2);
    CHECK(lj_bad("LJ4 seq 1\r\n# comment\r\n\r\nnum=12\r\n") == 1);        /* CRLF, comment, blank */
    CHECK(lj_bad("LJ4 seq 1\ngarbage\n") == -1);                            /* no '=' or ' ' */
    CHECK(lj_bad("LJ4 seq 1\n=12\n") == -1);                                /* empty key */
    CHECK(lj_bad("LJ4 seq 1\nnum=1\x01\n") == -1);                          /* control byte */
    CHECK(lj_bad("LJ4 seq 1\nthis_key_is_longer_than_thirty_one_characters=1\n") == -1);
    memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0;
    memcpy(big, "LJ4 seq 1\nv=", 12);                                       /* 8988-byte line */
    CHECK(lj_bad(big) == -1);
    big[300] = 0;                                                           /* 288-byte value > vcap 64 */
    CHECK(lj_bad(big) == -1);
    CHECK(lj_bad("LJ4 seq 1\nx=1\n") == 1);
    remove("test_rec.seq");
    remove("test_rec_bad.seq");
    printf("test_textrec: %d record pair(s), %d failure(s)\n", n, fails);
    return fails ? 1 : 0;
}
