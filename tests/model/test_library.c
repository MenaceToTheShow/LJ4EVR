/* test_library.c - `lj4evr_tests test_model_library`: the library folder tree with
 * LJ2's names, Default cannot be renamed or deleted and is recreated, [NEXT]
 * never reuses a deleted highest number, max-existing+1 recovery when the key is
 * missing, record listing, backup/restore .lj4b, ME-24 TickHz clamping, and the
 * 4-digit display helper. The temp tree is test_model_tmp/ under the cwd. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_read_text(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, cap - 1, f) : 0;
    if (f) fclose(f);
    buf[n] = 0;
    return f ? (int)n : -1;
}

static int lj_is_dir(const lj_library *lib, const char *rel)   /* a folder is one we can create a file in */
{
    char probe[LJ_PATH_CAP];
    FILE *f;
    lj_library_path(lib, rel, probe, sizeof probe);
    strcat(probe, "/probe.tmp");
    f = fopen(probe, "wb");
    if (!f) return 0;
    fclose(f);
    remove(probe);
    return 1;
}

int lj_test_model_library(int argc, char **argv);
int lj_test_model_library(int argc, char **argv)
{
    static const char *const folders[] = { "Sequences", "Cues", "CueLists", "CueMacros", "BGCues", "UserFixtureDef", "MIDIIn", "Presets", "Palettes" };
    lj_library lib, rest;
    char names[8][LJ_LIBNAME_CAP], path[LJ_PATH_CAP], path2[LJ_PATH_CAP], buf[4096], buf2[4096], num[16];
    uint32_t nums[16];
    lj_sequence *seq = lj_sequence_new_rec();
    lj_ini *ini;
    int i, n;
    (void)argc; (void)argv;

    lj_library_delete("test_model_tmp", "Lib");
    lj_library_delete("test_model_tmp", "Renamed");
    lj_library_delete("test_model_tmp", "Rest");
    lj_library_delete("test_model_tmp", "Gone");
    CHECK(lj_library_open("test_model_tmp", "Lib", &lib) == -1);
    CHECK(lj_library_create("test_model_tmp", "Lib", &lib) == 0);
    CHECK(lj_library_open("test_model_tmp", "Lib", &lib) == 0 && strcmp(lib.name, "Lib") == 0 && !lj_library_is_default(&lib));
    for (i = 0; i < 9; i++) CHECK(lj_is_dir(&lib, folders[i]));
    lj_library_path(&lib, "DirInfo.Ini", path, sizeof path);
    ini = lj_ini_load(path);
    CHECK(ini && strcmp(lj_ini_gets(ini, "INFO", "Name", ""), "Lib") == 0 && lj_ini_geti(ini, "NEXT", "Seq", 0) == 1);
    lj_ini_free(ini);
    CHECK(lj_library_create("test_model_tmp", "bad/name", &rest) == -1 && lj_library_create("test_model_tmp", "..", &rest) == -1);

    /* Default: created when missing, cannot be renamed or deleted, recreated after the folder vanished */
    CHECK(lj_library_open_default("test_model_tmp", &rest) == 0 && lj_library_is_default(&rest));
    CHECK(lj_library_delete("test_model_tmp", "Default") == -1 && lj_library_delete("test_model_tmp", "default") == -1);
    CHECK(lj_library_rename("test_model_tmp", "Default", "X") == -1 && lj_library_rename("test_model_tmp", "Lib", "Default") == -1);
    strcpy(path, rest.root); path[strlen(path) - 1] = 0;
    strcpy(path2, path); strcpy(path2 + strlen(path2) - 7, "Gone");
    CHECK(rename(path, path2) == 0);                       /* the folder vanishes behind our back */
    CHECK(lj_library_open("test_model_tmp", "Default", &rest) == -1);
    CHECK(lj_library_open_default("test_model_tmp", &rest) == 0 && lj_is_dir(&rest, "Sequences"));
    CHECK(lj_library_delete("test_model_tmp", "Gone") == 0);
    n = lj_library_list("test_model_tmp", names, 8);
    CHECK(n >= 2);                                          /* Default and Lib (other rows leave their own libraries in the tree) */
    for (i = 0; i < n && strcmp(names[i], "Default") != 0; i++) { }
    CHECK(i < n);
    for (i = 0; i < n && strcmp(names[i], "Lib") != 0; i++) { }
    CHECK(i < n);
    strcpy(path, rest.root); strcpy(path + strlen(path) - 8, "ValueTables");   /* W5: <userdata>/User/ValueTables is not a library */
    CHECK(lj_fs_mkdir(path) == 0);
    n = lj_library_list("test_model_tmp", names, 8);
    for (i = 0; i < n && strcmp(names[i], "ValueTables") != 0; i++) { }
    CHECK(i == n);

    /* [NEXT]: 1 2 3, delete 3, next is 4; missing key recovers as max existing + 1 */
    CHECK(lj_library_next_number(&lib, LJ_REC_SEQ) == 1 && lj_library_next_number(&lib, LJ_REC_SEQ) == 2 && lj_library_next_number(&lib, LJ_REC_SEQ) == 3);
    CHECK(lj_library_next_number(&lib, LJ_REC_CUE) == 1);
    for (i = 1; i <= 3; i++) {
        seq->number = (uint32_t)i;
        lj_library_record_path(&lib, LJ_REC_SEQ, (uint32_t)i, path, sizeof path);
        CHECK(lj_sequence_save(path, seq) == 0);
    }
    CHECK(lj_library_list_records(&lib, LJ_REC_SEQ, nums, 16) == 3 && nums[0] == 1 && nums[2] == 3);
    CHECK(lj_library_delete_record(&lib, LJ_REC_SEQ, 3) == 0 && lj_library_delete_record(&lib, LJ_REC_SEQ, 3) == -1);
    CHECK(lj_library_list_records(&lib, LJ_REC_SEQ, nums, 16) == 2);
    CHECK(lj_library_next_number(&lib, LJ_REC_SEQ) == 4);  /* never reused */
    lj_library_path(&lib, "DirInfo.Ini", path, sizeof path);
    ini = lj_ini_new();
    lj_ini_sets(ini, "INFO", "Name", "Lib");
    CHECK(lj_ini_save(ini, path) == 0);                    /* [NEXT] gone */
    lj_ini_free(ini);
    lj_library_record_path(&lib, LJ_REC_SEQ, 7, path, sizeof path);
    CHECK(lj_sequence_save(path, seq) == 0);
    CHECK(lj_library_next_number(&lib, LJ_REC_SEQ) == 8 && lj_library_next_number(&lib, LJ_REC_SEQ) == 9);
    CHECK(lj_library_next_number(&lib, LJ_REC_BG) == 1);  /* empty folder, missing key */
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 0, path, sizeof path) == -1);
    CHECK(lj_library_record_path(&lib, LJ_REC_CL, 12, path, sizeof path) == 0 && strstr(path, "00000012.Cl2"));
    CHECK(lj_number_text(12, num, sizeof num) == 0 && strcmp(num, "0012") == 0);
    CHECK(lj_number_text(12345, num, sizeof num) == 0 && strcmp(num, "12345") == 0);

    /* backup / restore */
    CHECK(lj_library_backup(&lib, "test_model_tmp/Lib.lj4b", "my notes") == 0);
    CHECK(lj_library_create("test_model_tmp", "Rest", &rest) == 0);
    CHECK(lj_library_restore(&rest, "test_model_tmp/Lib.lj4b") == 0);
    lj_library_record_path(&lib, LJ_REC_SEQ, 7, path, sizeof path);
    lj_library_record_path(&rest, LJ_REC_SEQ, 7, path2, sizeof path2);
    CHECK(lj_read_text(path, buf, sizeof buf) > 0 && lj_read_text(path2, buf2, sizeof buf2) > 0 && strcmp(buf, buf2) == 0);
    CHECK(lj_library_backup_notes("test_model_tmp/Lib.lj4b", buf, sizeof buf) == 0 && strcmp(buf, "my notes") == 0);   /* W2: the backup notes stay in the archive (never extracted over Notes.txt) */
    lj_library_path(&rest, "notes.txt", path, sizeof path);
    CHECK(lj_read_text(path, buf, sizeof buf) == -1);
    CHECK(lj_library_restore(&rest, "test_model_tmp/missing.lj4b") == -1);

    /* rename, delete */
    CHECK(lj_library_rename("test_model_tmp", "Lib", "Renamed") == 0 && lj_library_open("test_model_tmp", "Renamed", &lib) == 0);
    CHECK(lj_library_open("test_model_tmp", "Lib", &lib) == -1);
    CHECK(lj_library_delete("test_model_tmp", "Renamed") == 0 && lj_library_delete("test_model_tmp", "Rest") == 0);
    CHECK(lj_library_open("test_model_tmp", "Renamed", &lib) == -1);

    /* ME-24: TickHz clamped to 10..200 at load */
    ini = lj_ini_new();
    CHECK(lj_library_tick_hz(ini) == 40);
    lj_ini_seti(ini, "ENGINE", "TickHz", 5);    CHECK(lj_library_tick_hz(ini) == 10);
    lj_ini_seti(ini, "ENGINE", "TickHz", 1000); CHECK(lj_library_tick_hz(ini) == 200);
    lj_ini_seti(ini, "ENGINE", "TickHz", 50);   CHECK(lj_library_tick_hz(ini) == 50);
    lj_ini_free(ini);

    lj_sequence_free(seq);
    printf("test_model_library: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
