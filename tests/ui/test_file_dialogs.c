/* test_file_dialogs.c - `lj4evr_tests test_file_dialogs` (W4 dlg-files), headless (no lj_win_host). A scratch tree
 * test_file_dialogs_tmp/ under the cwd holds the userdir and the library "Lib".
 *  - ME-131: each of the 19 FILES ids and LJ_WIN_PLEASE_WAIT opens through lj_win_open with no "has no behaviour
 *    file yet", "no binding" or "unknown event" line, and closes.
 *  - W4-WIN please_wait: the startup lines come back with "This window will selfdestruct in 10 seconds (click
 *    anywhere in list to keep open)" as the last row; 10 refresh passes = 9 seconds; a click in the list keeps it
 *    open; untouched it closes itself after 10 s.
 *  - W4-DLG-2 from the entry points: System Preferences Data Restore tab ("(No File)", New Password greyed) ->
 *    Create Restore Data -> TCreateDataForm text -> Continue writes RestoreData.lj4d ("(File Exists)") -> a value
 *    changed -> Restore Data (LJ_ACT_RESTORE_DATA) -> "Restart & Restore" sets RESTORE=1 and runs LJ_ACT_RESTART ->
 *    lj_startup_library brings the value back. New Password: OK only when both boxes match; the change prompt and
 *    "Incorrect Password" verbatim. A corrupt data set at start: the "Error" prompt verbatim, Yes deletes it.
 *  - W4-DLG-4 / F56: one synthetic older record per dialog ("LJ4 cl 0", "LJ4 bgc 0", "LJ4 patch 0", "LJ4 seq 0"):
 *    lj_file_dlg_startup opens the four dialogs with their verbatim captions, painted yellow where the DFM is; each
 *    update rewrites its files with "LJ4 <kind> 1" (TUpdateForm after "Backup Datafiles first"); the next start opens
 *    nothing. "Do NOT update files - are you absolutely sure ?" on Cancel.
 *  - W4-DLG-1: User Libraries Rename through TRenameFileForm, Delete through "Deleting Files. Please Wait..", Restore
 *    User Library's folder tree (ancestors, sub-folders, *.lj4b of the folder picked).
 *  - Goldens FD_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_startup.h"
#include "core/lj_log.h"
#include "gfx/lj_bmp.h"
#include "model/lj_library.h"
#include "model/lj_restore_data.h"
#include "model/lj_show.h"
#include "model/lj_sequence.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_backup_restore.h"
#include "windows/lj_file_dialogs.h"
#include "windows/lj_prefs.h"
#include "windows/lj_system_prefs.h"
#include "windows/lj_user_libraries.h"
#include "windows/lj_warnings.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_file_dialogs_tmp/"

static int fails, bad_lines;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void sink(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "no behaviour file yet") || strstr(line, "no binding") || strstr(line, "unknown event")) { bad_lines++; printf("FAIL log: %s\n", line); }
}
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static lj_widget *in(enum lj_win id, const char *n) { lj_window *w = lj_win_get(id); return w ? lj_window_find(w, n) : NULL; }
static const char *cap(lj_widget *w) { const char *c = w ? lj_w_caption(w) : NULL; return c ? c : ""; }
static const char *msg_line(const char *label) { lj_window *m = lj_win_message_window(); return m ? cap(lj_window_find(m, label)) : ""; }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); click(m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL); }
static const char *memo_line(lj_widget *w, int i) { lj_w_memo *m = (lj_w_memo *)w; return w && i >= 0 && i < m->n ? m->lines[i] : ""; }
static int memo_n(lj_widget *w) { return w ? ((lj_w_memo *)w)->n : -1; }
static void pump(int n) { int i, k; for (k = 0; k < n; k++) for (i = 0; i < LJ_WIN_COUNT; i++) lj_win_refresh((enum lj_win)i); }

static int act_count[LJ_ACT_COUNT];
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)arg; (void)ud; act_count[a]++; if (a == LJ_ACT_RESTORE_DATA) lj_win_open(LJ_WIN_RESTORE_DATA); }

static int put(const lj_library *lib, const char *rel, const char *text)
{
    char p[LJ_PATH_CAP];
    FILE *f;
    if (lj_library_path(lib, rel, p, sizeof p) != 0 || (f = fopen(p, "wb")) == NULL) return -1;
    fputs(text, f);
    return fclose(f) == 0 ? 0 : -1;
}
static int first_line(const lj_library *lib, const char *rel, char *buf, int cap)
{
    char p[LJ_PATH_CAP];
    FILE *f;
    if (lj_library_path(lib, rel, p, sizeof p) != 0 || (f = fopen(p, "rb")) == NULL) return -1;
    if (!fgets(buf, cap, f)) buf[0] = 0;
    fclose(f);
    buf[strcspn(buf, "\r\n")] = 0;
    return 0;
}
static int exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

/* ---- goldens ---- */
static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(enum lj_win id, const char *name)
{
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    lj_window *w = lj_win_get(id);
    int rc;
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, TMP "%s.bmp", name);
    lj_window_invalidate_all(w);
    lj_window_paint(w);
    if (lj_bmp_write(lj_window_surface(w), upd && upd[0] == '1' ? path : tmp) != 0) { fails++; printf("FAIL golden %s: write\n", name); return; }
    if (upd && upd[0] == '1') { printf("  golden %s written\n", name); return; }
    rc = same_file(tmp, path);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (new render at %s)\n", name, rc == 0 ? "differs" : "missing", tmp); }
}

/* ---- every id opens (ME-131) ---- */
static void test_every_id(void)
{
    static const enum lj_win ids[] = { LJ_WIN_BACKUP_PROGRESS, LJ_WIN_RESTORE_LIB_PROGRESS, LJ_WIN_COPYING_FILES, LJ_WIN_RENAME_FILE, LJ_WIN_PROGRESS,
        LJ_WIN_PRINT_WAIT, LJ_WIN_RESTORE_DATA, LJ_WIN_CREATE_DATA, LJ_WIN_COMPRESS_RESTORE, LJ_WIN_RESTORE_PROGRESS, LJ_WIN_NEW_PASSWORD, LJ_WIN_UPDATE,
        LJ_WIN_UPDATE_FILE, LJ_WIN_UPDATE_PROGRAM, LJ_WIN_UPDATE_BGCUES, LJ_WIN_UPDATE_CL2, LJ_WIN_PATCH_UPDATE, LJ_WIN_REBUILD_SETUP, LJ_WIN_INIT_FORM, LJ_WIN_PLEASE_WAIT };
    static const char *const gold[] = { "FD_BACKUP_PROGRESS", "FD_RESTORE_LIB_PROGRESS", "FD_COPYING_FILES", "FD_RENAME_FILE", "FD_PROGRESS",
        "FD_PRINT_WAIT", "FD_RESTORE_DATA", "FD_CREATE_DATA", "FD_COMPRESS_RESTORE", "FD_RESTORE_PROGRESS", "FD_NEW_PASSWORD", "FD_UPDATE",
        "FD_UPDATE_FILE", "FD_UPDATE_PROGRAM", "FD_UPDATE_BGCUES", "FD_UPDATE_CL2", "FD_PATCH_UPDATE", "FD_REBUILD_SETUP", NULL /* TInitForm1 shows the VERSION: no golden */, "FD_PLEASE_WAIT" };
    size_t i;
    bad_lines = 0;
    lj_log_set_sink(sink, NULL);
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        lj_window *w = lj_win_open(ids[i]);
        if (!w) printf("FAIL %s did not open\n", lj_win_id_name(ids[i]));
        CHECK(w != NULL && lj_win_get(ids[i]) == w && lj_win_open(ids[i]) == w);
        if (w && gold[i]) golden(ids[i], gold[i]);
        lj_win_close(ids[i]);
        CHECK(lj_win_get(ids[i]) == NULL);
    }
    lj_log_set_sink(NULL, NULL);
    CHECK(bad_lines == 0);
    printf("every id: %d opened and closed\n", (int)(sizeof ids / sizeof ids[0]));
}

/* ---- Startup Status ---- */
static void test_please_wait(void)
{
    char want[160];
    lj_widget *re;
    lj_file_dlg_status_add("library: Default");
    lj_file_dlg_status_add("app: Config.dat loaded");
    CHECK(lj_win_open(LJ_WIN_PLEASE_WAIT) != NULL && strcmp(lj_window_caption(lj_win_get(LJ_WIN_PLEASE_WAIT))->text, "Startup Status") == 0);
    re = in(LJ_WIN_PLEASE_WAIT, "Re1");
    snprintf(want, sizeof want, LJ_FILE_SELFDESTRUCT_FMT, 10);
    CHECK(strcmp(want, "This window will selfdestruct in 10 seconds (click anywhere in list to keep open)") == 0);
    CHECK(memo_n(re) >= 3 && strcmp(memo_line(re, memo_n(re) - 1), want) == 0 && strcmp(memo_line(re, memo_n(re) - 2), "app: Config.dat loaded") == 0);
    pump(10);
    snprintf(want, sizeof want, LJ_FILE_SELFDESTRUCT_FMT, 9);
    CHECK(strcmp(memo_line(re, memo_n(re) - 1), want) == 0);
    re->on_event(re, LJ_WEV_MOUSE_DOWN, re->ud);                       /* Re1MouseDown: keep open */
    CHECK(strcmp(memo_line(re, memo_n(re) - 1), "app: Config.dat loaded") == 0);
    pump(120);
    CHECK(lj_win_get(LJ_WIN_PLEASE_WAIT) != NULL);
    lj_win_close(LJ_WIN_PLEASE_WAIT);
    CHECK(lj_win_open(LJ_WIN_PLEASE_WAIT) != NULL);
    pump(100);
    CHECK(lj_win_get(LJ_WIN_PLEASE_WAIT) == NULL);                     /* selfdestructed */
}

/* ---- Data Restore ---- */
static void test_data_restore(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *sp;
    lj_library got;
    char rd[LJ_PATH_CAP], line[128];
    lj_restore_data_path(TMP, rd, sizeof rd);
    remove(rd);
    lj_restore_password_set(TMP, NULL);
    CHECK(put(lib, "Sequences/00000001.seq", "LJ4 seq 1\nnum=1\nname=\"Opener\"\n") == 0);

    sp = lj_system_prefs_open();
    CHECK(sp != NULL);
    CHECK(strcmp(cap(lj_window_find(sp, "bCreate")), "Create Restore Data (No File)") == 0);
    CHECK(!lj_window_find(sp, "bNewPassword")->enabled && !lj_window_find(sp, "cbEnablePassword")->enabled && !lj_window_find(sp, "pPassword")->visible);
    lj_w_tabs_select(lj_window_find(sp, "PC1"), 4);
    golden(LJ_WIN_SYSTEM_PREFS, "FD_PREFS_DATA_RESTORE_NOFILE");
    click(lj_window_find(sp, "bCreate"));
    CHECK(lj_win_get(LJ_WIN_CREATE_DATA) != NULL && strcmp(lj_window_caption(lj_win_get(LJ_WIN_CREATE_DATA))->text, "Creating Auto Restore Data") == 0);
    CHECK(strcmp(memo_line(in(LJ_WIN_CREATE_DATA, "Re1"), 0), "Creating Restore Data - Warning") == 0);
    CHECK(strncmp(memo_line(in(LJ_WIN_CREATE_DATA, "Re1"), 2), "Creating auto restore data will completely", 42) == 0);
    golden(LJ_WIN_CREATE_DATA, "FD_CREATE_DATA");
    click(in(LJ_WIN_CREATE_DATA, "BitBtn1"));                          /* Continue */
    pump(1);
    CHECK(lj_win_get(LJ_WIN_CREATE_DATA) == NULL && lj_win_get(LJ_WIN_COMPRESS_RESTORE) == NULL && exists(rd));
    CHECK(strcmp(cap(lj_window_find(sp, "bCreate")), "Create Restore Data (File Exists)") == 0 && lj_window_find(sp, "bNewPassword")->enabled);

    /* New Password */
    click(lj_window_find(sp, "bNewPassword"));
    CHECK(lj_win_get(LJ_WIN_NEW_PASSWORD) != NULL && !in(LJ_WIN_NEW_PASSWORD, "bOk")->enabled);
    lj_w_edit_set_text(in(LJ_WIN_NEW_PASSWORD, "Edit1"), "club");
    lj_w_edit_set_text(in(LJ_WIN_NEW_PASSWORD, "Edit2"), "clu");
    CHECK(!in(LJ_WIN_NEW_PASSWORD, "bOk")->enabled);
    lj_w_edit_set_text(in(LJ_WIN_NEW_PASSWORD, "Edit2"), "club");
    CHECK(in(LJ_WIN_NEW_PASSWORD, "bOk")->enabled);
    golden(LJ_WIN_NEW_PASSWORD, "FD_NEW_PASSWORD_TYPED");
    click(in(LJ_WIN_NEW_PASSWORD, "bOk"));
    pump(1);
    CHECK(lj_win_get(LJ_WIN_NEW_PASSWORD) == NULL && lj_restore_password_isset(TMP) && lj_restore_password_check(TMP, "club"));
    click(lj_window_find(sp, "bNewPassword"));                         /* a password is set: the change prompt */
    CHECK(strcmp(msg_line("Label1"), "Are you sure you want to change the password?") == 0 && strcmp(msg_line("Label2"), "Does your momma kow you are doing this ?") == 0);
    msg_answer(0);
    CHECK(lj_win_get(LJ_WIN_NEW_PASSWORD) == NULL);
    CHECK(lj_file_dlg_unlock("wrong") == -1 && strcmp(msg_line("Label1"), "Incorrect Password") == 0);
    msg_answer(1);
    CHECK(lj_file_dlg_unlock("club") == 0);
    lj_w_tabs_select(lj_window_find(sp, "PC1"), 4);
    golden(LJ_WIN_SYSTEM_PREFS, "FD_PREFS_DATA_RESTORE_FILE");
    lj_system_prefs_close();

    /* change a value, restore it from the System menu entry */
    CHECK(put(lib, "Sequences/00000001.seq", "LJ4 seq 1\nnum=1\nname=\"Changed\"\n") == 0);
    lj_desktop_run_action(LJ_ACT_RESTORE_DATA, 0);
    CHECK(lj_win_get(LJ_WIN_RESTORE_DATA) != NULL && strcmp(memo_line(in(LJ_WIN_RESTORE_DATA, "Re1"), 0), "Restore Program Data - Warning") == 0);
    CHECK(strcmp(cap(in(LJ_WIN_RESTORE_DATA, "BitBtn1")), "Restart && Restore") == 0 && in(LJ_WIN_RESTORE_DATA, "BitBtn1")->enabled);
    golden(LJ_WIN_RESTORE_DATA, "FD_RESTORE_DATA_TEXT");
    act_count[LJ_ACT_RESTART] = 0;
    click(in(LJ_WIN_RESTORE_DATA, "BitBtn1"));
    pump(1);
    CHECK(act_count[LJ_ACT_RESTART] == 1 && lj_restore_data_requested(TMP) && lj_win_get(LJ_WIN_RESTORE_DATA) == NULL);
    CHECK(lj_startup_library(TMP, NULL, &got) == 0 && strcmp(got.name, "Lib") == 0 && !lj_restore_data_requested(TMP));   /* the "restart" */
    CHECK(first_line(&got, "Sequences/00000001.seq", line, sizeof line) == 0);
    { char p[LJ_PATH_CAP], b[256]; FILE *f; size_t n = 0; lj_library_path(&got, "Sequences/00000001.seq", p, sizeof p); if ((f = fopen(p, "rb")) != NULL) { n = fread(b, 1, sizeof b - 1, f); fclose(f); } b[n] = 0; CHECK(strstr(b, "\"Opener\"") != NULL); }
    printf("data restore: create, change, restore -> the value is back\n");

    /* a corrupt data set at the next start: the Error prompt, Yes = abort and delete */
    { FILE *f = fopen(rd, "wb"); if (f) { fputs("PK not a zip", f); fclose(f); } }
    lj_restore_data_request(TMP, 1);
    CHECK(lj_startup_library(TMP, NULL, &got) == 0 && lj_restore_data_requested(TMP));
    CHECK(lj_file_dlg_startup() >= 1);
    CHECK(strcmp(lj_window_caption(lj_win_message_window())->text, "Error") == 0);
    CHECK(strcmp(msg_line("Label1"), "Restore data file seems to be corrupt - do you wish to abort the data restore process ?") == 0);
    CHECK(strcmp(msg_line("Label2"), "(Aborting will delete the corrupt data file - restore will not be possible)") == 0);
    msg_answer(1);
    CHECK(!exists(rd) && !lj_restore_data_requested(TMP));
    (void)c;
    lj_restore_password_set(TMP, NULL);
}

/* ---- format-update dialogs (F56) ---- */
static void test_updates(lj_library *lib)
{
    char line[64], bak[LJ_PATH_CAP];
    lj_window *w;
    CHECK(put(lib, "CueLists/00000004.Cl2", "LJ4 cl 0\nnum=4\nname=\"Old list\"\n") == 0);
    CHECK(put(lib, "BGCues/00000002.bgc", "LJ4 bgc 0\nnum=2\n") == 0);
    CHECK(put(lib, "GlobalPatch.GPA", "LJ4 patch 0\n") == 0);
    CHECK(put(lib, "Sequences/00000007.seq", "LJ4 seq 0\nnum=7\n") == 0);
    CHECK(lj_file_dlg_startup() == 4);
    CHECK((w = lj_win_get(LJ_WIN_UPDATE_CL2)) && strcmp(lj_window_caption(w)->text, "Datafile format update required") == 0 && (lj_window_root(w)->color & 0xFFFFFF) == 0xFFFF00);
    CHECK((w = lj_win_get(LJ_WIN_UPDATE_BGCUES)) && strcmp(lj_window_caption(w)->text, "File Format Update Needed") == 0 && (lj_window_root(w)->color & 0xFFFFFF) == 0xFFFF00);
    CHECK((w = lj_win_get(LJ_WIN_PATCH_UPDATE)) && strcmp(lj_window_caption(w)->text, "Update to Patch Configuration") == 0);
    CHECK((w = lj_win_get(LJ_WIN_UPDATE)) && strcmp(lj_window_caption(w)->text, "Dataformat has Changed") == 0 && (lj_window_root(w)->color & 0xFFFFFF) == 0xFFFF00);
    CHECK(strcmp(cap(in(LJ_WIN_UPDATE, "cbBackup")), "Backup Datafiles first ") == 0 && lj_w_checkbox_checked(in(LJ_WIN_UPDATE, "cbBackup")));
    CHECK(strcmp(memo_line(in(LJ_WIN_PATCH_UPDATE, "Memo1"), memo_n(in(LJ_WIN_PATCH_UPDATE, "Memo1")) - 1), "This update is irreversible.") == 0);
    CHECK(strncmp(memo_line(in(LJ_WIN_UPDATE, "Memo1"), 0), "Updating the file format will make the files", 45) == 0);   /* wrapped at the memo width */
    golden(LJ_WIN_UPDATE_CL2, "FD_UPDATE_CL2"); golden(LJ_WIN_UPDATE_BGCUES, "FD_UPDATE_BGCUES");
    golden(LJ_WIN_PATCH_UPDATE, "FD_PATCH_UPDATE"); golden(LJ_WIN_UPDATE, "FD_UPDATE");

    click(in(LJ_WIN_UPDATE, "BitBtn3"));                               /* Cancel */
    CHECK(strcmp(msg_line("Label1"), "Do NOT update files - are you absolutely sure ?") == 0);
    msg_answer(0);
    CHECK(lj_win_get(LJ_WIN_UPDATE) != NULL);

    click(in(LJ_WIN_UPDATE_CL2, "bUpdate"));
    CHECK(first_line(lib, "CueLists/00000004.Cl2", line, sizeof line) == 0 && strcmp(line, "LJ4 cl 1") == 0);
    CHECK(first_line(lib, "Sequences/00000007.seq", line, sizeof line) == 0 && strcmp(line, "LJ4 seq 0") == 0);   /* only its own kind */
    click(in(LJ_WIN_UPDATE_BGCUES, "BitBtn1"));
    CHECK(first_line(lib, "BGCues/00000002.bgc", line, sizeof line) == 0 && strcmp(line, "LJ4 bgc 1") == 0);
    click(in(LJ_WIN_PATCH_UPDATE, "bOk"));
    CHECK(first_line(lib, "GlobalPatch.GPA", line, sizeof line) == 0 && strcmp(line, "LJ4 patch 1") == 0);
    snprintf(bak, sizeof bak, TMP "Lib (before update)" LJ_LIBRARY_BACKUP_EXT);
    remove(bak);
    click(in(LJ_WIN_UPDATE, "BitBtn1"));                               /* OK with Backup Datafiles first */
    CHECK(exists(bak) && first_line(lib, "Sequences/00000007.seq", line, sizeof line) == 0 && strcmp(line, "LJ4 seq 1") == 0);
    pump(1);
    CHECK(!lj_win_get(LJ_WIN_UPDATE) && !lj_win_get(LJ_WIN_UPDATE_CL2) && !lj_win_get(LJ_WIN_UPDATE_BGCUES) && !lj_win_get(LJ_WIN_PATCH_UPDATE) && !lj_win_get(LJ_WIN_UPDATE_FILE));
    CHECK(lj_file_dlg_startup() == 0);                                 /* the second start shows nothing */

    CHECK(put(lib, "CueLists/00000005.Cl2", "LJ4 cl 0\nnum=5\n") == 0);   /* Don't update - don't remind me */
    CHECK(lj_file_dlg_startup() == 1);
    click(in(LJ_WIN_UPDATE_CL2, "bCancel"));
    pump(1);
    CHECK(lj_file_dlg_startup() == 0 && first_line(lib, "CueLists/00000005.Cl2", line, sizeof line) == 0 && strcmp(line, "LJ4 cl 0") == 0);
    lj_ini_seti(lj_ctx_get()->main_ini, "UPDATE", "NoCl2Reminder", 0);
    CHECK(lj_file_dlg_startup() == 1);
    click(in(LJ_WIN_UPDATE_CL2, "bRemindMe"));                         /* remind me later: still there next start */
    pump(1);
    CHECK(lj_file_dlg_startup() == 1);
    click(in(LJ_WIN_UPDATE_CL2, "bUpdate"));
    pump(1);
    remove(bak);
    printf("F56: four synthetic older records raised their four dialogs; rewritten; the next start opened 0\n");
}

/* ---- User Libraries and the Restore folder tree ---- */
static int lib_row(const char *name) { int i; for (i = 0; i < lj_user_libraries_count(); i++) if (strcmp(lj_user_libraries_name(i), name) == 0) return i; return -1; }
static void test_libraries(lj_library *lib)
{
    lj_window *w;
    lj_library tmp;
    lj_w_list *dirs, *files;
    int i, sub = -1;
    lj_library_delete(TMP, "Gig"); lj_library_delete(TMP, "Gig2");
    CHECK(lj_library_create(TMP, "Gig", &tmp) == 0);
    w = lj_win_open(LJ_WIN_USER_LIBRARIES);
    CHECK(w && lj_user_libraries_select(lib_row("Gig")) == 0);
    {   /* ME-163: the Date column carries the folder's modification time, "dd-mm-yyyy hh:mm" (PROPOSAL format, lj_fs_mtime_text) */
        const lj_w_listview *lv = w ? (const lj_w_listview *)lj_window_find(w, "LV1") : NULL;
        const char *it = lv && lib_row("Gig") >= 0 && lv->items ? lv->items[lib_row("Gig")] : "", *t2 = strrchr(it, '\t');
        CHECK(t2 && strlen(t2 + 1) == 16 && t2[3] == '-' && t2[6] == '-' && t2[14] == ':');
    }
    click(lj_window_find(w, "bRename"));
    CHECK(lj_win_get(LJ_WIN_RENAME_FILE) && strcmp(lj_window_caption(lj_win_get(LJ_WIN_RENAME_FILE))->text, "Rename File") == 0);
    CHECK(strcmp(lj_w_edit_text(in(LJ_WIN_RENAME_FILE, "eFileName")), "Gig") == 0);
    lj_w_edit_set_text(in(LJ_WIN_RENAME_FILE, "eFileName"), "Gig2");
    click(in(LJ_WIN_RENAME_FILE, "bOk"));
    pump(1);
    CHECK(lj_win_get(LJ_WIN_RENAME_FILE) == NULL && lj_library_open(TMP, "Gig2", &tmp) == 0 && lib_row("Gig2") >= 0);
    CHECK(lj_user_libraries_select(lib_row("Gig2")) == 0);
    click(lj_window_find(w, "bDelete"));
    msg_answer(1);
    CHECK(lj_library_open(TMP, "Gig2", &tmp) != 0 && lj_win_get(LJ_WIN_PROGRESS) == NULL);
    lj_win_close(LJ_WIN_USER_LIBRARIES);

    CHECK(lj_library_backup(lib, TMP "Lib.lj4b", "tree") == 0);
    w = lj_win_open(LJ_WIN_RESTORE);
    CHECK(w != NULL);
    dirs = (lj_w_list *)lj_window_find(w, "DirList");                 /* the stub stays hidden; the tree is the code-built list over its rect */
    CHECK(dirs && !dirs->base.visible);
    CHECK(lj_restore_set_dir(TMP) == 0);
    dirs = NULL;
    {   /* find the code-built lists by their rows */
        lj_widget *p = lj_window_find(w, "DirList")->parent, *k;
        for (k = p ? p->first : NULL; k; k = k->next) if (!k->name && k->visible) dirs = (lj_w_list *)k;
        p = lj_window_find(w, "FileList")->parent;
        for (k = p ? p->first : NULL, files = NULL; k; k = k->next) if (!k->name && k->visible) files = (lj_w_list *)k;
    }
    CHECK(dirs && files);
    if (!dirs || !files) return;
    CHECK(dirs->n >= 2 && strcmp(dirs->items[0], "test_file_dialogs_tmp/") == 0 && dirs->index == 0);
    for (i = 1; i < dirs->n; i++) if (strcmp(dirs->items[i], "  User") == 0) sub = i;
    CHECK(sub > 0);
    for (i = 0; i < files->n; i++) if (strcmp(files->items[i], "Lib.lj4b") == 0) break;
    CHECK(i < files->n);
    golden(LJ_WIN_RESTORE, "FD_RESTORE_TREE");
    CHECK(lj_restore_dir_row(sub) == 0 && dirs->n >= 2 && strcmp(dirs->items[1], "  User") == 0 && dirs->index == 1);
    for (i = 2; i < dirs->n; i++) if (strcmp(dirs->items[i], "    Lib") == 0) break;
    CHECK(i < dirs->n && files->n == 0);
    CHECK(lj_restore_dir_row(0) == 0 && dirs->index == 0 && files->n >= 1);
    lj_win_close(LJ_WIN_RESTORE);
    remove(TMP "Lib.lj4b");
}

int lj_test_file_dialogs(int argc, char **argv);
int lj_test_file_dialogs(int argc, char **argv)
{
    static lj_library lib;
    static lj_sysprefs prefs;
    lj_ctx *c = lj_ctx_get();
    int a;
    (void)argc; (void)argv;
    lj_log_set_sink(NULL, NULL);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    lj_library_select(TMP, "Lib");
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->show->current_seq = lj_sequence_new_rec(); c->current_scene = 0;
    lj_sysprefs_defaults(&prefs); c->prefs = &prefs;
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, on_action, NULL);

    test_every_id();
    test_please_wait();
    test_data_restore(&lib);
    test_updates(&lib);
    test_libraries(&lib);

    lj_win_close_all();
    lj_warnings_close_all();
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, NULL, NULL);
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(c->show); c->show = NULL;
    c->lib = NULL; c->prefs = NULL;
    printf("file_dialogs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
