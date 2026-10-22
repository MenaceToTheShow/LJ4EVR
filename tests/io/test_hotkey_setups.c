/* test_hotkey_setups.c - `lj4evr_tests test_hotkey_setups`: the 8 hotkey setups of io/lj_hotkeys.h
 * under a library folder (<dir>Hotkeys/SetupN.Hot): Setup 1..8 written through the Edit Hotkeys model
 * calls, a setup switch loads the right .Hot, a missing file falls back silently (setup 1 = Default
 * Hotkeys, others empty), a bad file reports and falls back, Main.Ini [HOTKEYS] File=SetupN.Hot parses. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
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
#include "core/lj_log.h"
#include "io/lj_hotkeys.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_t_log_n;
static void lj_t_sink(void *ud, const char *line) { (void)ud; (void)line; lj_t_log_n++; }

int lj_test_hotkey_setups(int argc, char **argv);
int lj_test_hotkey_setups(int argc, char **argv)
{
    static lj_hotkeys h;
    static const char dir[] = "test_hk_lib/";
    char path[300], err[200];
    lj_hotkey k;
    int s;
    (void)argc; (void)argv;
    LJ_MKDIR("test_hk_lib"); LJ_MKDIR("test_hk_lib/Hotkeys");

    for (s = 1; s <= LJ_HOTKEY_SETUPS_MAX; s++) {                 /* Edit Hotkeys: setup s binds F<s> to cue s, then Save */
        lj_hotkeys_init(&h);
        snprintf(h.name, sizeof h.name, "Show %d", s);
        memset(&k, 0, sizeof k); k.key = (uint16_t)(0x70 + s - 1); k.fn = (uint16_t)LJ_FN_PROG_CUE; k.prog_num = (uint32_t)(100 + s);
        CHECK(lj_hotkeys_assign(&h, &k) == 0);
        if (s == 3) { k.key = 'Q'; k.fn = (uint16_t)LJ_FN_BLACKOUT; k.prog_num = 0; CHECK(lj_hotkeys_assign(&h, &k) == 0 && lj_hotkeys_clear_fn(&h, LJ_FN_BLACKOUT, 0) == 0); }
        CHECK(lj_hotkeys_setup_path(dir, s, path, sizeof path) == 0 && lj_hotkeys_save(path, &h) == 0);
    }
    for (s = LJ_HOTKEY_SETUPS_MAX; s >= 1; s--) {                 /* Setup > Hotkeys > Setup s: the right file comes back */
        const lj_hotkey *e;
        char name[24];
        memset(&h, 0x5A, sizeof h);
        CHECK(lj_hotkeys_setup_load(dir, s, &h, err, sizeof err) == 0 && err[0] == 0);
        snprintf(name, sizeof name, "Show %d", s);
        e = lj_hotkeys_match(&h, (uint16_t)(0x70 + s - 1), 0, 0, 0, 0, 0);
        CHECK(strcmp(h.name, name) == 0 && h.n == 1 && e && e->fn == LJ_FN_PROG_CUE && e->prog_num == (uint32_t)(100 + s));
        CHECK(lj_hotkeys_match(&h, (uint16_t)(0x70 + s), 0, 0, 0, 0, 0) == NULL);   /* the neighbour's key is not here */
    }

    lj_t_log_n = 0; lj_log_set_sink(lj_t_sink, NULL);             /* missing files: silent fallback */
    CHECK(lj_hotkeys_setup_path(dir, 1, path, sizeof path) == 0 && remove(path) == 0);
    CHECK(lj_hotkeys_setup_load(dir, 1, &h, err, sizeof err) == 0 && strcmp(h.name, "Default") == 0 && h.n == 4);
    CHECK(lj_hotkeys_setup_load("test_hk_nowhere/", 5, &h, err, sizeof err) == 0 && strcmp(h.name, "Setup 5") == 0 && h.n == 0 && h.enabled == 1);
    CHECK(lj_t_log_n == 0);
    lj_log_set_sink(NULL, NULL);

    CHECK(lj_hotkeys_setup_path(dir, 2, path, sizeof path) == 0);   /* a bad file: -1, the message, the fallback */
    { FILE *f = fopen(path, "wb"); if (f) { fputs("LJ4 hot 1\nkey vk=66 shift=0 ctrl=0 scrl=0 fn=LJ_FN_NOPE\n", f); fclose(f); } }
    CHECK(lj_hotkeys_setup_load(dir, 2, &h, err, sizeof err) == -1 && strstr(err, "LJ_FN_NOPE") != NULL && strcmp(h.name, "Setup 2") == 0 && h.n == 0);
    CHECK(lj_hotkeys_setup_load(dir, 0, &h, err, sizeof err) == -1 && lj_hotkeys_setup_load(dir, 9, &h, err, sizeof err) == -1);

    CHECK(lj_hotkeys_setup_of_file("Setup1.Hot") == 1 && lj_hotkeys_setup_of_file("SETUP8.HOT") == 8);
    CHECK(lj_hotkeys_setup_of_file("C:\\LJ\\User\\Default\\Hotkeys\\Setup4.Hot") == 4 && lj_hotkeys_setup_of_file("lib/Hotkeys/setup2.hot") == 2);
    CHECK(lj_hotkeys_setup_of_file("Setup9.Hot") == -1 && lj_hotkeys_setup_of_file("Setup0.Hot") == -1 && lj_hotkeys_setup_of_file("Setup1.Hotx") == -1);
    CHECK(lj_hotkeys_setup_of_file("Setup12.Hot") == -1 && lj_hotkeys_setup_of_file("") == -1 && lj_hotkeys_setup_of_file(NULL) == -1);

    for (s = 1; s <= LJ_HOTKEY_SETUPS_MAX; s++) if (lj_hotkeys_setup_path(dir, s, path, sizeof path) == 0) remove(path);
    LJ_RMDIR("test_hk_lib/Hotkeys"); LJ_RMDIR("test_hk_lib");
    printf("test_hotkey_setups: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
