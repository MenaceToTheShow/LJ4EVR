/* test_restore_data.c - `lj4evr_tests test_restore_data` (W4 dlg-files, tests/model): our restore-data format and the
 * library checks around it, on temp trees under the cwd (test_restore_data_tmp/).
 *  - W4-DLG-2 round trip: create restore data, change a value, delete a file and add one, request the restore,
 *    lj_startup_library erases + extracts it: every file byte-equal, the added file gone, RESTORE=0, Start= the library.
 *  - the password: set, check right / wrong, change, clear; never stored as the text (not a security boundary).
 *  - untrusted archives: a truncated file and a flipped byte (CRC) fail pass 1 and erase nothing; hostile entry names
 *    ("..", a drive colon, a leading backslash, a file entry ending in "\", a name that makes the path >= LJ_PATH_CAP:
 *    W2 LORD_C-5) are refused by lj_library_backup_check and lj_library_replace leaves the library alone; an archive
 *    inside the library itself is refused.
 *  - ME-130: lj_startup_library with U:<new folder> on a fresh temp userdir creates every lj_library_create folder.
 *  - F56: a synthetic "LJ4 cl 0" record is found by lj_library_outdated and rewritten as "LJ4 cl 1", rest byte-equal;
 *    a second scan finds nothing. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_startup.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "miniz.h"
#include "model/lj_library.h"
#include "model/lj_restore_data.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define UD "test_restore_data_tmp/"

static void quiet(void *ud, const char *line) { (void)ud; (void)line; }
static int put(const lj_library *lib, const char *rel, const char *text)
{
    char p[LJ_PATH_CAP];
    FILE *f;
    if (lj_library_path(lib, rel, p, sizeof p) != 0 || (f = fopen(p, "wb")) == NULL) return -1;
    fputs(text, f);
    return fclose(f) == 0 ? 0 : -1;
}
static long get(const lj_library *lib, const char *rel, char *buf, long cap)
{
    char p[LJ_PATH_CAP];
    FILE *f;
    long n;
    if (lj_library_path(lib, rel, p, sizeof p) != 0 || (f = fopen(p, "rb")) == NULL) return -1;
    n = (long)fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    buf[n < 0 ? 0 : n] = 0;
    return n;
}
static int has(const lj_library *lib, const char *rel) { char b[8]; return get(lib, rel, b, sizeof b) >= 0; }
static int is_dir(const lj_library *lib, const char *rel) { char names[4][LJ_DIRENT_CAP], p[LJ_PATH_CAP]; return lj_library_path(lib, rel, p, sizeof p) == 0 && lj_library_dir_list(p, NULL, names, 4) >= 0; }

static int zip_with(const char *path, const char *entry)   /* one good entry and one under test */
{
    mz_zip_archive z;
    int ok;
    memset(&z, 0, sizeof z);
    if (!mz_zip_writer_init_file(&z, path, 0)) return -1;
    ok = mz_zip_writer_add_mem(&z, "Cues/00000001.cue", "LJ4 cue 1\n", 10, MZ_DEFAULT_LEVEL)
      && mz_zip_writer_add_mem(&z, entry, "x", 1, MZ_DEFAULT_LEVEL)
      && mz_zip_writer_finalize_archive(&z);
    mz_zip_writer_end(&z);
    return ok ? 0 : -1;
}

static long copy_cut(const char *from, const char *to, long keep, long flip)   /* keep < 0 = all; flip >= 0 = xor that byte */
{
    static char buf[1 << 20];
    FILE *f = fopen(from, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (keep >= 0 && keep < n) n = keep;
    if (flip >= 0 && flip < n) buf[flip] ^= 0x5A;
    if ((f = fopen(to, "wb")) == NULL) return -1;
    fwrite(buf, 1, (size_t)n, f);
    fclose(f);
    return n;
}

static void test_round_trip(void)
{
    lj_library lib, got;
    char rd[LJ_PATH_CAP], b[256], start[LJ_PATH_CAP];
    lj_ini *ini = NULL;
    CHECK(lj_library_create(UD, "Club", &lib) == 0);
    CHECK(put(&lib, "Sequences/00000001.seq", "LJ4 seq 1\nnum=1\nname=\"Opener\"\n") == 0);
    CHECK(put(&lib, "Main.Ini", "[PREFS]\r\nWarnSeq=1\r\n") == 0);
    CHECK(put(&lib, "Cues/00000002.cue", "LJ4 cue 1\nnum=2\n") == 0);
    CHECK(lj_restore_data_path(UD, rd, sizeof rd) == 0 && strcmp(rd, UD LJ_RESTORE_DATA_FILE) == 0);
    remove(rd);
    CHECK(!lj_restore_data_exists(UD));
    CHECK(lj_restore_data_create(&lib, rd) == 0 && lj_restore_data_exists(UD));
    CHECK(lj_restore_data_info(rd, b, sizeof b) == 0 && strcmp(b, "Club") == 0);

    CHECK(put(&lib, "Sequences/00000001.seq", "LJ4 seq 1\nnum=1\nname=\"Changed\"\n") == 0);   /* the club's inexperienced user */
    CHECK(put(&lib, "Sequences/00000009.seq", "LJ4 seq 1\nnum=9\n") == 0);
    { char p[LJ_PATH_CAP]; lj_library_path(&lib, "Cues/00000002.cue", p, sizeof p); remove(p); }

    CHECK(lj_restore_data_request(UD, 1) == 0 && lj_restore_data_requested(UD));
    CHECK(lj_startup_library(UD, NULL, &got) == 0 && strcmp(got.name, "Club") == 0);
    CHECK(get(&got, "Sequences/00000001.seq", b, sizeof b) > 0 && strstr(b, "\"Opener\"") != NULL);   /* the value is back */
    CHECK(has(&got, "Cues/00000002.cue") && !has(&got, "Sequences/00000009.seq"));                   /* erased first, then restored */
    CHECK(get(&got, "Main.Ini", b, sizeof b) > 0 && strcmp(b, "[PREFS]\r\nWarnSeq=1\r\n") == 0);
    CHECK(!has(&got, "notes.txt"));                                                                    /* the manifest stays in the archive */
    CHECK(!lj_restore_data_requested(UD) && lj_restore_data_exists(UD));                               /* the data set survives for the next time */
    CHECK(lj_library_startup_ini(UD, start, sizeof start) == 0 && (ini = lj_ini_load(start)) != NULL);
    if (ini) { CHECK(strcmp(lj_ini_gets(ini, "USERDIRECTORY", "Start", ""), "Club\\") == 0); lj_ini_free(ini); }
    printf("round trip: create, change, restore -> \"Opener\" back, Cues/00000002.cue back, Sequences/00000009.seq gone\n");
}

static void test_password(void)
{
    char start[LJ_PATH_CAP];
    lj_ini *ini = NULL;
    CHECK(lj_restore_password_set(UD, "") == 0 && !lj_restore_password_isset(UD) && lj_restore_password_check(UD, "anything"));
    CHECK(lj_restore_password_set(UD, "s3cret") == 0 && lj_restore_password_isset(UD));
    CHECK(lj_restore_password_check(UD, "s3cret") && !lj_restore_password_check(UD, "S3cret") && !lj_restore_password_check(UD, ""));
    CHECK(lj_restore_password_set(UD, "12345678901234567") == -1);                                     /* MaxLength 16 */
    CHECK(lj_restore_password_set(UD, "newer") == 0 && lj_restore_password_check(UD, "newer") && !lj_restore_password_check(UD, "s3cret"));
    CHECK(lj_library_startup_ini(UD, start, sizeof start) == 0 && (ini = lj_ini_load(start)) != NULL);
    if (ini) { const char *h = lj_ini_gets(ini, "RESTORELIB", "Pass", ""); CHECK(strlen(h) == 8 && !strstr(h, "newer")); lj_ini_free(ini); }
    CHECK(lj_restore_password_set(UD, NULL) == 0 && !lj_restore_password_isset(UD));
}

static void test_untrusted(void)
{
    static const char *const hostile[] = { "../evil.txt", "Cues/../../evil.txt", "C:evil.txt", "\\evil.txt", "Cues\\", "Sequences/stream:x" };
    lj_library lib;
    char rd[LJ_PATH_CAP], bad[LJ_PATH_CAP], b[256], longname[LJ_PATH_CAP + 8];
    long n;
    size_t i;
    CHECK(lj_library_open(UD, "Club", &lib) == 0);
    lj_restore_data_path(UD, rd, sizeof rd);
    snprintf(bad, sizeof bad, "%s", UD "bad.lj4r");
    n = copy_cut(rd, bad, -1, -1);
    CHECK(n > 100);
    CHECK(copy_cut(rd, bad, n / 2, -1) > 0);                                                           /* truncated */
    CHECK(lj_library_backup_check(&lib, bad) == -1 && lj_library_replace(&lib, bad) == -1);
    CHECK(get(&lib, "Sequences/00000001.seq", b, sizeof b) > 0 && strstr(b, "Opener"));                /* nothing erased */
    CHECK(copy_cut(rd, bad, -1, 60) > 0);                                                              /* one byte of the first entry's data: CRC */
    CHECK(lj_library_backup_check(&lib, bad) == -1 && has(&lib, "Main.Ini"));
    {   /* the same through Data Restore: the request stays for the error prompt (windows/lj_file_dialogs.c) */
        lj_library got;
        CHECK(copy_cut(bad, rd, -1, -1) > 0 && lj_restore_data_request(UD, 1) == 0);
        lj_startup_library(UD, NULL, &got);
        CHECK(lj_restore_data_requested(UD) && has(&lib, "Main.Ini"));
        lj_restore_data_request(UD, 0);
    }
    for (i = 0; i < sizeof hostile / sizeof hostile[0]; i++) {
        CHECK(zip_with(bad, hostile[i]) == 0);
        if (lj_library_backup_check(&lib, bad) != -1) printf("FAIL hostile entry \"%s\" accepted\n", hostile[i]);
        CHECK(lj_library_backup_check(&lib, bad) == -1 && lj_library_replace(&lib, bad) == -1 && has(&lib, "Main.Ini"));
    }
    memset(longname, 'a', sizeof longname); longname[600] = 0;                                         /* miniz's stat would cut it at 511: refused, not truncated */
    CHECK(zip_with(bad, longname) == 0 && lj_library_backup_check(&lib, bad) == -1);
    longname[400] = 0;                                                                                 /* fits root + name < LJ_PATH_CAP (checked, never extracted here) */
    CHECK(zip_with(bad, longname) == 0 && lj_library_backup_check(&lib, bad) == 0);
    {   /* the path-length rule itself: a library root long enough that a 500-byte name crosses LJ_PATH_CAP */
        lj_library deep = lib;
        memset(deep.root, 'd', 600); deep.root[600] = '/'; deep.root[601] = 0;
        longname[400] = 'a'; longname[500] = 0;
        CHECK(zip_with(bad, longname) == 0 && lj_library_backup_check(&deep, bad) == -1);
    }
    CHECK(zip_with(bad, "Cues/00000003.cue") == 0 && lj_library_backup_check(&lib, bad) == 0);         /* the control: a plain archive passes */
    {   /* an archive inside the library it would replace */
        char inside[LJ_PATH_CAP];
        lj_library_path(&lib, "self.lj4b", inside, sizeof inside);
        CHECK(lj_library_backup(&lib, bad, "") == 0 && copy_cut(bad, inside, -1, -1) > 0);
        CHECK(lj_library_replace(&lib, inside) == -1 && has(&lib, "Main.Ini"));
        remove(inside);
    }
    remove(bad);
}

static void test_me130(void)
{
    static const char *const folders[] = { "Sequences", "Cues", "CueLists", "CueMacros", "BGCues", "UserFixtureDef", "MIDIIn", "Presets", "Palettes", "Hotkeys" };
    lj_library lib, again;
    size_t i;
    lj_library_delete(UD "fresh/", "NewShow");
    CHECK(lj_startup_library(UD "fresh/", "NewShow\\", &lib) == 0 && strcmp(lib.name, "NewShow") == 0);
    for (i = 0; i < sizeof folders / sizeof folders[0]; i++) { if (!is_dir(&lib, folders[i])) printf("FAIL ME-130: %s missing\n", folders[i]); CHECK(is_dir(&lib, folders[i])); }
    CHECK(has(&lib, "DirInfo.Ini"));
    CHECK(put(&lib, "MIDIIn/MIDIIn.Ini", "[SETUP]\r\nDeviceCode=0\r\n") == 0);                         /* MIDI-IN settings survive the next start */
    CHECK(lj_startup_library(UD "fresh/", "NewShow", &again) == 0 && has(&again, "MIDIIn/MIDIIn.Ini"));
    CHECK(lj_startup_library(UD "fresh/", "bad:name", &again) == 0 && strcmp(again.name, "Default") == 0);
    printf("ME-130: U:NewShow on a fresh userdir created %d folders\n", (int)(sizeof folders / sizeof folders[0]));
}

static void test_f56(void)
{
    char rel[8][LJ_REL_CAP], kind[8][16], b[256];
    lj_library lib;
    int n;
    CHECK(lj_library_open(UD, "Club", &lib) == 0);
    CHECK(put(&lib, "CueLists/00000004.Cl2", "LJ4 cl 0\r\nnum=4\r\nname=\"Old list\"\r\n") == 0);
    n = lj_library_outdated(&lib, rel, kind, 8);
    CHECK(n == 1 && strcmp(rel[0], "CueLists/00000004.Cl2") == 0 && strcmp(kind[0], "cl") == 0);
    CHECK(lj_library_update_file(&lib, "CueLists/00000004.Cl2") == 0);
    CHECK(get(&lib, "CueLists/00000004.Cl2", b, sizeof b) > 0 && strcmp(b, "LJ4 cl 1\r\nnum=4\r\nname=\"Old list\"\r\n") == 0);
    CHECK(lj_library_outdated(&lib, rel, kind, 8) == 0 && lj_library_update_file(&lib, "CueLists/00000004.Cl2") == -1);   /* irreversible, and once */
}

static void test_dir_list(void)
{
    char names[8][LJ_DIRENT_CAP];
    lj_library lib;
    char p[LJ_PATH_CAP];
    CHECK(lj_library_open(UD, "Club", &lib) == 0);
    put(&lib, "b.lj4b", "x"); put(&lib, "A.LJ4B", "x"); put(&lib, "c.txt", "x");
    lj_library_path(&lib, "", p, sizeof p);
    CHECK(lj_library_dir_list(p, ".lj4b", names, 8) == 2 && strcmp(names[0], "A.LJ4B") == 0 && strcmp(names[1], "b.lj4b") == 0);
    CHECK(lj_library_dir_list(p, NULL, names, 8) == 8 && strcmp(names[0], "BGCues") == 0);              /* sorted, cut at max */
}

int lj_test_restore_data(int argc, char **argv);
int lj_test_restore_data(int argc, char **argv)
{
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    lj_library_delete(UD, "Club");
    test_round_trip();
    test_password();
    test_untrusted();
    test_me130();
    test_f56();
    test_dir_list();
    lj_log_set_sink(NULL, NULL);
    printf("restore_data: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
