/* test_model_w2_library.c - `lj4evr_tests test_model_w2_library`: the W2 library dialogs on a temp
 * library with three files (a sequence, a cue, Notes.txt with a CP1252 0xE9): backup -> notes and
 * per-kind counts read back -> restore into a NEW library under the collision rule ("W2Lib (2)"),
 * files byte-equal and DirInfo.Ini Name renamed; a zip with a ".." entry or a drive colon is refused
 * and leaves nothing behind; [NEXT] Bg= high-water after deleting the highest BG cue (ME-14);
 * "copy files"; Select User Library writes Startup.ini and deleting the selected library resets it to
 * Default; Delete Files "Check Used Sequences" marks only the sequences a cue references; lj_show_load reads
 * GlobalPatch.GPA and lj_show_save_config keeps the HTP names.
 * The temp tree is test_model_w2_tmp/ under the cwd. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "miniz.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"
#include "model/lj_cue.h"
#include "model/lj_bgcue.h"
#include "model/lj_notes.h"
#include "model/lj_show.h"
#include "model/lj_patch.h"
#include "model/lj_groups.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define UD "test_model_w2_tmp"

static int lj_same_file(const char *a, const char *b)
{
    static char x[1 << 16], y[1 << 16];
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    size_t na = fa ? fread(x, 1, sizeof x, fa) : 0, nb = fb ? fread(y, 1, sizeof y, fb) : 0;
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return fa && fb && na == nb && na > 0 && memcmp(x, y, na) == 0;
}

static int lj_exists(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return 0; fclose(f); return 1; }

static int lj_bad_zip(const char *zip_path, const char *entry)   /* a backup with one hostile entry beside a good one */
{
    mz_zip_archive z;
    int ok;
    memset(&z, 0, sizeof z);
    if (!mz_zip_writer_init_file(&z, zip_path, 0)) return -1;
    ok = mz_zip_writer_add_mem(&z, "Cues/00000001.cue", "LJ4 cue 1\n", 10, MZ_DEFAULT_LEVEL)
      && mz_zip_writer_add_mem(&z, entry, "x", 1, MZ_DEFAULT_LEVEL)
      && mz_zip_writer_finalize_archive(&z);
    mz_zip_writer_end(&z);
    return ok ? 0 : -1;
}

static void lj_clean(void)
{
    static const char *const names[] = { "W2Lib", "W2Lib (2)", "W2Lib (3)", "W2Copy", "Evil", "Evil (2)" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) lj_library_delete(UD, names[i]);
}

int lj_test_model_w2_library(int argc, char **argv);
int lj_test_model_w2_library(int argc, char **argv)
{
    lj_library lib, rest, cp, tmp;
    lj_sequence *seq = lj_sequence_new_rec();
    lj_cue *cue = lj_cue_new_rec();
    lj_bgcue *bg = lj_bgcue_new_rec();
    char a[LJ_PATH_CAP], b[LJ_PATH_CAP], notes[256];
    int counts[LJ_REC_KIND_COUNT];
    uint32_t nums[16], bgn;
    uint8_t used[16];
    lj_ini *ini;
    int n;
    (void)argc; (void)argv;
    lj_clean();

    /* a library with three files */
    CHECK(lj_library_create(UD, "W2Lib", &lib) == 0);
    seq->number = 1; strcpy(seq->name, "Caf\xe9");
    lj_library_record_path(&lib, LJ_REC_SEQ, 1, a, sizeof a); CHECK(lj_sequence_save(a, seq) == 0);
    seq->number = 2;
    lj_library_record_path(&lib, LJ_REC_SEQ, 2, a, sizeof a); CHECK(lj_sequence_save(a, seq) == 0);   /* unused by any cue */
    cue->number = 1; lj_cue_add_sequence(cue, 1, 0); cue->bg_cue = 3;
    lj_library_record_path(&lib, LJ_REC_CUE, 1, a, sizeof a); CHECK(lj_cue_save(a, cue) == 0);
    lj_library_path(&lib, LJ_NOTES_FILE, a, sizeof a); CHECK(lj_notes_save(a, "show notes \xe9\r\n") == 0);

    /* backup, its notes and counts */
    CHECK(lj_library_backup(&lib, UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT, "backup \xe9") == 0);
    CHECK(lj_library_backup_notes(UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT, notes, sizeof notes) == 0 && strcmp(notes, "backup \xe9") == 0);
    CHECK(lj_library_backup_lists(UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT, counts) == 0 && counts[LJ_REC_SEQ] == 2 && counts[LJ_REC_CUE] == 1 && counts[LJ_REC_BG] == 0);
    CHECK(lj_library_backup_notes(UD "/missing.lj4b", notes, sizeof notes) == -1 && lj_library_backup_lists(UD "/missing.lj4b", counts) == -1);

    /* restore into a new library: the name is taken, so "W2Lib (2)" */
    CHECK(lj_library_restore_new(UD, "W2Lib", UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT, &rest) == 0 && strcmp(rest.name, "W2Lib (2)") == 0);
    lj_library_record_path(&lib, LJ_REC_CUE, 1, a, sizeof a); lj_library_record_path(&rest, LJ_REC_CUE, 1, b, sizeof b); CHECK(lj_same_file(a, b));
    lj_library_record_path(&lib, LJ_REC_SEQ, 2, a, sizeof a); lj_library_record_path(&rest, LJ_REC_SEQ, 2, b, sizeof b); CHECK(lj_same_file(a, b));
    lj_library_path(&rest, LJ_NOTES_FILE, b, sizeof b); CHECK(lj_notes_load(b, notes, sizeof notes) == 0 && strcmp(notes, "show notes \xe9\r\n") == 0);   /* the Notes window text, not the backup notes */
    lj_library_path(&rest, "DirInfo.Ini", b, sizeof b);
    ini = lj_ini_load(b);
    CHECK(ini && strcmp(lj_ini_gets(ini, "INFO", "Name", ""), "W2Lib (2)") == 0);
    lj_ini_free(ini);
    CHECK(lj_library_restore_new(UD, "W2Lib", UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT, &tmp) == 0 && strcmp(tmp.name, "W2Lib (3)") == 0);
    CHECK(lj_library_restore_new(UD, "Evil", UD "/missing.lj4b", &tmp) == -1 && lj_library_open(UD, "Evil", &tmp) == -1);   /* nothing left behind */

    /* hostile archives: refused before any byte lands, no library left */
    CHECK(lj_bad_zip(UD "/evil1.lj4b", "../evil.txt") == 0);
    CHECK(lj_library_restore_new(UD, "Evil", UD "/evil1.lj4b", &tmp) == -1 && lj_library_open(UD, "Evil", &tmp) == -1 && !lj_exists(UD "/User/evil.txt"));
    CHECK(lj_bad_zip(UD "/evil2.lj4b", "Cues/../../evil.txt") == 0);
    CHECK(lj_library_restore_new(UD, "Evil", UD "/evil2.lj4b", &tmp) == -1 && lj_library_open(UD, "Evil", &tmp) == -1);
    CHECK(lj_bad_zip(UD "/evil3.lj4b", "C:/evil.txt") == 0);
    CHECK(lj_library_restore_new(UD, "Evil", UD "/evil3.lj4b", &tmp) == -1 && lj_library_open(UD, "Evil", &tmp) == -1);
    CHECK(lj_library_restore(&rest, UD "/evil1.lj4b") == -1);
    lj_library_record_path(&lib, LJ_REC_CUE, 1, a, sizeof a); lj_library_record_path(&rest, LJ_REC_CUE, 1, b, sizeof b);
    CHECK(lj_same_file(a, b));                                    /* the archive's good entry was not written over the existing cue either */
    remove(UD "/evil1.lj4b"); remove(UD "/evil2.lj4b"); remove(UD "/evil3.lj4b");

    /* ME-14: [NEXT] Bg= high-water: 1 2 3, save 3, delete 3, the next is 4 */
    CHECK(lj_library_next_number(&lib, LJ_REC_BG) == 1 && lj_library_next_number(&lib, LJ_REC_BG) == 2);
    bgn = lj_library_next_number(&lib, LJ_REC_BG);
    CHECK(bgn == 3);
    bg->number = bgn; lj_bgcue_add_sequence(bg, 1);
    lj_library_record_path(&lib, LJ_REC_BG, bgn, a, sizeof a); CHECK(lj_bgcue_save(a, bg) == 0);
    CHECK(lj_library_list_records(&lib, LJ_REC_BG, nums, 16) == 1 && nums[0] == 3);
    CHECK(lj_library_delete_record(&lib, LJ_REC_BG, 3) == 0 && lj_library_next_number(&lib, LJ_REC_BG) == 4);
    lj_library_path(&lib, "DirInfo.Ini", a, sizeof a);
    ini = lj_ini_load(a);
    CHECK(ini && lj_ini_geti(ini, "NEXT", "Bg", 0) == 5 && lj_ini_geti(ini, "NEXT", "Seq", 0) == 1 && lj_ini_geti(ini, "NEXT", "Macro", 0) == 1);
    lj_ini_free(ini);

    /* the show load reads <library>\GlobalPatch.GPA; a Config.dat save keeps the HTP names in GroupInfo.dat */
    {
        lj_show *sh = lj_show_new();
        lj_htp_names h;
        sh->patch[3].invert = 1;
        lj_library_path(&lib, LJ_PATCH_FILE, a, sizeof a); CHECK(lj_patch_save(a, sh->patch) == 0);
        lj_patch_init(sh->patch);
        lj_library_path(&lib, "GroupInfo.dat", a, sizeof a);
        lj_htp_names_load(a, &h, NULL, 0); strcpy(h.name[0], "House \xe9");
        CHECK(lj_htp_names_save(a, sh->user_group, &h) == 0);
        CHECK(lj_show_load(sh, &lib) == 0 && sh->patch[3].invert == 1);
        CHECK(lj_show_save_config(sh, &lib) == 0 && lj_htp_names_load(a, &h, NULL, 0) == 0 && strcmp(h.name[0], "House \xe9") == 0);
        lj_show_free(sh);
    }

    /* Delete Files: Check Used Sequences / BG cues (03 s4.4): cue 1 uses seq 1 and BG cue 3 */
    n = lj_library_list_records(&lib, LJ_REC_SEQ, nums, 16);
    CHECK(n == 2 && lj_library_used_by_cues(&lib, LJ_REC_SEQ, nums, n, used) == 1 && used[0] == 1 && used[1] == 0);
    nums[0] = 3; nums[1] = 4;
    CHECK(lj_library_used_by_cues(&lib, LJ_REC_BG, nums, 2, used) == 1 && used[0] == 1 && used[1] == 0);
    CHECK(lj_library_used_by_cues(&lib, LJ_REC_CL, nums, 2, used) == -1);

    /* copy files, select, delete resets the start library */
    CHECK(lj_library_copy(UD, "W2Lib", "W2Copy") == 0 && lj_library_open(UD, "W2Copy", &cp) == 0);
    lj_library_record_path(&lib, LJ_REC_SEQ, 1, a, sizeof a); lj_library_record_path(&cp, LJ_REC_SEQ, 1, b, sizeof b); CHECK(lj_same_file(a, b));
    CHECK(lj_library_copy(UD, "W2Lib", "W2Copy") == -1 && lj_library_copy(UD, "Nope", "X") == -1);
    CHECK(lj_library_select(UD, "W2Copy") == 0);
    ini = lj_ini_load(UD "/Startup.ini");
    CHECK(ini && strcmp(lj_ini_gets(ini, "USERDIRECTORY", "Start", ""), "W2Copy\\") == 0);
    lj_ini_free(ini);
    CHECK(lj_library_select(UD, "Nope") == -1);
    CHECK(lj_library_delete(UD, "W2Copy") == 0);
    ini = lj_ini_load(UD "/Startup.ini");
    CHECK(ini && strcmp(lj_ini_gets(ini, "USERDIRECTORY", "Start", ""), "Default\\") == 0);
    lj_ini_free(ini);
    CHECK(lj_library_delete(UD, "Default") == -1);

    lj_clean();
    remove(UD "/W2Lib" LJ_LIBRARY_BACKUP_EXT); remove(UD "/Startup.ini");
    lj_sequence_free(seq); lj_cue_free(cue); lj_bgcue_free(bg);
    printf("test_model_w2_library: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
