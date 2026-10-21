/* test_encoding.c - design 2.1 / 9.3: every byte 128-255 round-trips CP1252 ->
 * UTF-8 -> CP1252, unmappable code points become '?', malformed UTF-8 is -1,
 * lj_str_copy / lj_fmt_time, and a directory named "Tést æøå" is created, written
 * into, reopened and deleted through the UTF-8 CRT (the manifest on Windows). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define LJ_MKDIR(p) _mkdir(p)
#define LJ_RMDIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define LJ_MKDIR(p) mkdir(p, 0777)
#define LJ_RMDIR(p) rmdir(p)
#endif
#include "core/lj_str.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_encoding(int argc, char **argv);
int lj_test_encoding(int argc, char **argv)
{
    char cp[256], u8[4 * 256], back[256], dir[64], path[96];
    int b, n = 0;
    FILE *f;
    (void)argc; (void)argv;

    for (b = 128; b < 256; b++) {                       /* one byte at a time */
        cp[0] = (char)b; cp[1] = 0;
        CHECK(lj_cp1252_to_utf8(u8, sizeof u8, cp) == 0);
        CHECK(strlen(u8) == ((unsigned char)u8[0] >= 0xE0 ? 3u : 2u));
        CHECK(lj_utf8_to_cp1252(back, sizeof back, u8) == 0);
        if (back[0] != (char)b || back[1]) { fails++; printf("FAIL byte 0x%02x -> \"%s\" -> 0x%02x\n", b, u8, (unsigned char)back[0]); }
    }
    for (b = 1; b < 256; b++) cp[n++] = (char)b;        /* all at once, 1..255 */
    cp[n] = 0;
    CHECK(lj_cp1252_to_utf8(u8, sizeof u8, cp) == 0);
    CHECK(lj_utf8_to_cp1252(back, sizeof back, u8) == 0);
    CHECK(strcmp(back, cp) == 0);

    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xE2\x82\xAC") == 0 && strcmp(back, "\x80") == 0);        /* euro */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xC3\xBC") == 0 && strcmp(back, "\xFC") == 0);            /* u-umlaut */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "a\xC7\x85z") == 0 && strcmp(back, "a?z") == 0);           /* U+01C5 */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xE6\x97\xA5\xE6\x9C\xAC") == 0 && strcmp(back, "??") == 0); /* CJK */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xF0\x9F\x98\x80!") == 0 && strcmp(back, "?!") == 0);     /* U+1F600 */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "ab\xC3") == -1 && strcmp(back, "ab") == 0);              /* truncated */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xC0\x80") == -1);                                       /* overlong */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xED\xA0\x80") == -1);                                   /* surrogate */
    CHECK(lj_utf8_to_cp1252(back, sizeof back, "\xFF") == -1);
    CHECK(lj_utf8_to_cp1252(back, 3, "abc") == -1 && strcmp(back, "ab") == 0);                            /* cap */
    CHECK(lj_cp1252_to_utf8(u8, 3, "\x80") == -1 && u8[0] == 0);                                          /* needs 4 */
    CHECK(lj_cp1252_to_utf8(u8, 4, "\x80") == 0 && strcmp(u8, "\xE2\x82\xAC") == 0);

    CHECK(lj_str_copy(back, 4, "abcdef") == -1 && strcmp(back, "abc") == 0);
    CHECK(lj_str_copy(back, 4, "abc") == 0 && strcmp(back, "abc") == 0);
    CHECK(lj_str_copy(back, 0, "x") == -1);
    CHECK(lj_fmt_time(back, sizeof back, 460) == 0 && strcmp(back, "0:46") == 0);     /* 07 fact-check: 0:46 */
    CHECK(lj_fmt_time(back, sizeof back, 50) == 0 && strcmp(back, "0:05") == 0);
    CHECK(lj_fmt_time(back, sizeof back, 1000) == 0 && strcmp(back, "1:00") == 0);
    CHECK(lj_fmt_time(back, sizeof back, 61239) == 0 && strcmp(back, "61:23") == 0);
    CHECK(lj_fmt_time(back, 4, 1000) == -1);

    /* the UTF-8 path: "Tést æøå" built from its CP1252 bytes, under the cwd (the build dir under ctest) */
    CHECK(lj_cp1252_to_utf8(dir, sizeof dir, "T\xE9st \xE6\xF8\xE5") == 0);
    CHECK(strcmp(dir, "T\xC3\xA9st \xC3\xA6\xC3\xB8\xC3\xA5") == 0);
    LJ_RMDIR(dir);                                      /* leftovers from a failed run */
    CHECK(LJ_MKDIR(dir) == 0);
    snprintf(path, sizeof path, "%s/%s.seq", dir, dir);
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) { CHECK(fputs(cp, f) >= 0); fclose(f); }
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f) {
        memset(back, 0, sizeof back);
        CHECK(fread(back, 1, sizeof back - 1, f) == (size_t)n);
        CHECK(strcmp(back, cp) == 0);
        fclose(f);
    }
    CHECK(remove(path) == 0);
    CHECK(LJ_RMDIR(dir) == 0);

    printf("test_encoding: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
