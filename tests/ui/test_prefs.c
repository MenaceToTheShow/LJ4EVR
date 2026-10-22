/* test_prefs.c - `lj4evr_tests test_prefs`: the System Preferences record (windows/lj_prefs.h
 * lj_sysprefs): LJ2's shipped defaults, every key round-trips through a Main.Ini document with the
 * exe's spelling ([PREFS] AllowBlind = Allow freeze output, [SOLO] Enabled, [DESKTOP] Restore,
 * [HOTKEYS] File=SetupN.Hot), a missing key falls back to the shipped value. */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "windows/lj_prefs.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_prefs(int argc, char **argv);
int lj_test_prefs(int argc, char **argv)
{
    lj_sysprefs p, q; lj_ini *ini = lj_ini_new();
    (void)argc; (void)argv;
    lj_sysprefs_defaults(&p);
    CHECK(p.allow_freeze == 0 && p.allow_seq_blind == 1 && p.allow_offline == 0 && p.allow_solo == 1);   /* the shipped Main.Ini [13 s6] */
    CHECK(p.keep_seq_blind == 1 && p.open_on_windows_desktop == 1 && p.confirm_close == 1 && p.warn_blackout == 1 && p.quick_seq == 0);
    CHECK(p.view_seqcue_bar == 1 && p.hotkeys_enabled == 1 && p.hotkeys_disable_in_dialogs == 1 && p.hotkey_setup == 1);
    lj_sysprefs_load(NULL, &q);
    CHECK(memcmp(&p, &q, sizeof p) == 0);

    p.allow_freeze = 1; p.allow_solo = 0; p.warn_freeze = 0; p.quick_cue = 1; p.restore_desktop = 1; p.hotkey_setup = 3; p.keep_seq_blind = 0;
    lj_sysprefs_save(ini, &p);
    CHECK(lj_ini_geti(ini, "PREFS", "AllowBlind", -1) == 1);
    CHECK(lj_ini_geti(ini, "SOLO", "Enabled", -1) == 0);
    CHECK(lj_ini_geti(ini, "PREFS", "WarnBlind", -1) == 0);
    CHECK(lj_ini_geti(ini, "PREFS", "QuickCue", -1) == 1);
    CHECK(lj_ini_geti(ini, "DESKTOP", "Restore", -1) == 1);
    CHECK(lj_ini_geti(ini, "PREFS", "KeepSeqBlind", -1) == 0);
    CHECK(strcmp(lj_ini_gets(ini, "HOTKEYS", "File", ""), "Setup3.Hot") == 0);
    lj_sysprefs_load(ini, &q);
    CHECK(memcmp(&p, &q, sizeof p) == 0);

    lj_ini_free(ini); ini = lj_ini_new();
    lj_ini_seti(ini, "PREFS", "AllowOffline", 1);               /* one key present, the rest shipped */
    lj_sysprefs_load(ini, &q);
    CHECK(q.allow_offline == 1 && q.allow_freeze == 0 && q.hotkey_setup == 1);
    lj_ini_free(ini);
    printf("test_prefs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
