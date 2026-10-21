/* test_ini.c - lj_ini: round trip, unknown keys / comments / order preserved,
 * CRLF and LF input, case-insensitive match, missing file, defaults. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf/sprintf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_read_all(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return -1;
    n = fread(buf, 1, cap - 1, f);
    buf[n] = 0;
    fclose(f);
    return 0;
}

int lj_test_ini(int argc, char **argv);
int lj_test_ini(int argc, char **argv)
{
    static const char seed[] =
        "; Main.Ini\r\n"
        "[OTHER]\r\n"
        "LastRun=LightJockey 2.110\r\n"
        "Unknown=keep me\r\n"
        "\r\n"
        "[chrome]\n"
        "Native=0\n"
        "Caption=T\xE9st \xE6\xF8\xE5\n";
    static const char want[] =
        "; Main.Ini\r\n"
        "[OTHER]\r\n"
        "LastRun=LJ4EVR 0.0.1\r\n"
        "Unknown=keep me\r\n"
        "\r\n"
        "[chrome]\r\n"
        "Native=1\r\n"
        "Caption=T\xE9st \xE6\xF8\xE5\r\n"
        "New=7\r\n"
        "[ENGINE]\r\n"
        "TickHz=40\r\n";
    char buf[1024];
    lj_ini *ini;
    FILE *f;
    (void)argc; (void)argv;

    CHECK(lj_ini_load("test_ini_missing.ini") == NULL);

    f = fopen("test_ini_seed.ini", "wb");
    CHECK(f != NULL);
    if (f) { fwrite(seed, 1, sizeof seed - 1, f); fclose(f); }
    ini = lj_ini_load("test_ini_seed.ini");
    CHECK(ini != NULL);
    if (!ini) return 1;
    CHECK(strcmp(lj_ini_gets(ini, "OTHER", "LastRun", "?"), "LightJockey 2.110") == 0);
    CHECK(strcmp(lj_ini_gets(ini, "other", "lastrun", "?"), "LightJockey 2.110") == 0);      /* case-insensitive */
    CHECK(strcmp(lj_ini_gets(ini, "CHROME", "Caption", "?"), "T\xE9st \xE6\xF8\xE5") == 0);  /* CP1252 untouched */
    CHECK(lj_ini_geti(ini, "chrome", "Native", 9) == 0);
    CHECK(lj_ini_geti(ini, "chrome", "Caption", 9) == 9);                                     /* not a number */
    CHECK(lj_ini_geti(ini, "chrome", "Nope", 9) == 9);
    CHECK(strcmp(lj_ini_gets(ini, "nosuch", "x", "dflt"), "dflt") == 0);
    lj_ini_sets(ini, "OTHER", "LastRun", "LJ4EVR 0.0.1");
    lj_ini_seti(ini, "CHROME", "native", 1);                   /* replaces Native=0, keeps the file's key spelling */
    lj_ini_seti(ini, "chrome", "New", 7);
    lj_ini_seti(ini, "ENGINE", "TickHz", 40);
    CHECK(lj_ini_save(ini, "test_ini_out.ini") == 0);
    lj_ini_free(ini);

    CHECK(lj_read_all("test_ini_out.ini", buf, sizeof buf) == 0);
    if (strcmp(buf, want) != 0) { fails++; printf("FAIL saved ini differs:\n%s---- want:\n%s", buf, want); }

    ini = lj_ini_load("test_ini_out.ini");
    CHECK(ini != NULL);
    if (ini) {
        CHECK(lj_ini_geti(ini, "ENGINE", "TickHz", 0) == 40);
        CHECK(lj_ini_geti(ini, "chrome", "New", 0) == 7);
        CHECK(strcmp(lj_ini_gets(ini, "OTHER", "Unknown", "?"), "keep me") == 0);
        lj_ini_free(ini);
    }
    ini = lj_ini_new();
    CHECK(lj_ini_geti(ini, "a", "b", -5) == -5);
    lj_ini_seti(ini, "a", "b", 3);
    CHECK(lj_ini_geti(ini, "a", "b", -5) == 3);
    lj_ini_free(ini);
    lj_ini_free(NULL);
    remove("test_ini_seed.ini");
    remove("test_ini_out.ini");
    printf("test_ini: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
