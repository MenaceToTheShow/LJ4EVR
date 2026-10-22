/* test_windows_d.c - `lj4evr_tests test_windows_d`: the runnable checks of the W2 windows-d track,
 * headless (no lj_win_host). A scratch tree test_windows_d_tmp/ under the cwd holds the libraries.
 *  - System Preferences: every bound box flips its lj_sysprefs field and round-trips through Main.Ini
 *    under its LJ2 key; OK runs LJ_ACT_SYSTEM_PREFS_APPLY once, Cancel leaves ctx->prefs alone; the
 *    exclusive pairs; D12 boxes greyed.
 *  - About: Label2 reads "LJ4EVR <VERSION>" and equals the VERSION file.
 *  - Delete Files: Check Used / Mark Unused leave the cue-referenced sequence unselected, the confirm
 *    text, Delete removes only the marked files; the exe prompts of the other kinds; Ctrl/Shift-click.
 *  - User Libraries: create through the name box, delete with the verbatim prompt, Default / current
 *    locked, Select asks to restart and runs LJ_ACT_SELECT_LIBRARY with the row.
 *  - Backup then Restore: every file of the library byte-equal after a modify + delete, notes shown,
 *    the restart prompt runs LJ_ACT_RESTART.
 *  - Notes: typing through the memo, saved on close, read back on open.
 *  - Striking Lamps: Lamp On fixtures only, one per interval, the default writer puts 228 (MAC 500) in
 *    the current scene, the startup run closes itself.
 *  - Edit Hotkeys: capture, assign, the conflict box text, Alt refused, programs, Setup1/Setup8.Hot.
 *  - MIDI-IN: Learn binds a cue through lj_midi_inject, the next note fires it.
 *  - Goldens W2D_* (tests/ui/golden, REVIEWED.tsv): System Preferences, Delete Sequences, Notes,
 *    Striking Lamps, Edit Hotkeys, MIDI-IN Sequences tab; LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <sys/utime.h>
static int lj_t_settime(const char *p, time_t t) { struct _utimbuf u; u.actime = u.modtime = t; return _utime(p, &u); }
#else
#include <utime.h>
static int lj_t_settime(const char *p, time_t t) { struct utimbuf u; u.actime = u.modtime = t; return utime(p, &u); }
#endif
#include "core/lj_log.h"
#include "fixlib/lj_profile.h"
#include "gfx/lj_bmp.h"
#include "io/lj_hotkeys.h"
#include "io/lj_midi.h"
#include "model/lj_cue.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "version.h"
#include "windows/lj_about_form.h"
#include "windows/lj_backup_restore.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_edit_hotkeys.h"
#include "windows/lj_midi_in_form.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_striking_lamps.h"
#include "windows/lj_system_prefs.h"
#include "windows/lj_user_libraries.h"
#include "windows/lj_warnings.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_windows_d_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void quiet(void *ud, const char *line) { (void)ud; (void)line; }
static void click(lj_widget *w) { if (w) lj_w_click(w); else { fails++; printf("FAIL widget missing\n"); } }
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static const char *msg_line(const char *label) { lj_window *m = lj_win_message_window(); const char *c = m ? lj_w_caption(lj_window_find(m, label)) : NULL; return c ? c : ""; }
static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); click(m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL); }

static int act_count[LJ_ACT_COUNT], act_arg[LJ_ACT_COUNT];
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)ud; act_count[a]++; act_arg[a] = arg; }

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
static void golden(lj_window *w, const char *name)
{
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
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

static long read_file(const char *path, char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}
static void write_text(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static void save_seq(lj_library *lib, uint32_t no, const char *name)
{
    lj_sequence *s = lj_sequence_new_rec();
    char path[LJ_PATH_CAP];
    s->number = no; snprintf(s->name, sizeof s->name, "%s", name);
    CHECK(lj_library_record_path(lib, LJ_REC_SEQ, no, path, sizeof path) == 0 && lj_sequence_save(path, s) == 0);
    lj_sequence_free(s);
}

/* ---- System Preferences ---- */
static void test_system_prefs(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_sysprefs dflt, back;
    lj_window *w;
    int i, n = lj_system_prefs_box_count();
    lj_sysprefs_defaults(&dflt);
    CHECK(n == 31);
    for (i = 0; i < n; i++) {
        const char *sec, *key, *box = lj_system_prefs_box(i, &sec, &key);
        lj_ini *ini = lj_ini_new();
        int was, now, applies = act_count[LJ_ACT_SYSTEM_PREFS_APPLY];
        *c->prefs = dflt;
        w = lj_system_prefs_open();
        CHECK(w != NULL);
        if (!w) { lj_ini_free(ini); return; }
        was = lj_w_checkbox_checked(in(w, box));
        lj_w_checkbox_set_checked(in(w, box), !was);       /* programmatic: the greyed D12 boxes too */
        click(in(w, "Bok"));
        lj_system_prefs_refresh();
        CHECK(lj_system_prefs_window() == NULL);
        CHECK(act_count[LJ_ACT_SYSTEM_PREFS_APPLY] == applies + 1);
        lj_sysprefs_save(ini, c->prefs);
        now = lj_ini_geti(ini, sec, key, -1);
        if (now != !was) printf("FAIL box %s -> [%s] %s = %d, expected %d\n", box, sec, key, now, !was);
        CHECK(now == !was);
        lj_sysprefs_load(ini, &back);
        CHECK(memcmp(&back, c->prefs, sizeof back) == 0);
        lj_ini_free(ini);
    }
    /* Cancel discards; the exclusive pairs; D12 greyed */
    *c->prefs = dflt;
    w = lj_system_prefs_open();
    golden(w, "W2D_SYSTEMPREFS");                                               /* the shipped defaults on the System Settings tab */
    CHECK(lj_w_checkbox_checked(in(w, "cbOpenOnWindowsDesktop")) == 1 && lj_w_checkbox_checked(in(w, "cbOpenOnDesktop")) == 0);
    click(in(w, "cbOpenOnDesktop"));
    CHECK(lj_w_checkbox_checked(in(w, "cbOpenOnDesktop")) == 1 && lj_w_checkbox_checked(in(w, "cbOpenOnWindowsDesktop")) == 0);
    click(in(w, "cbPrimary")); click(in(w, "cbSecondary"));
    CHECK(lj_w_checkbox_checked(in(w, "cbSecondary")) == 1 && lj_w_checkbox_checked(in(w, "cbPrimary")) == 0);
    CHECK(!in(w, "cbSandnet")->enabled && !in(w, "cbCapture")->enabled && !in(w, "cbEnableOffline43")->enabled);
    click(in(w, "BCancel"));
    lj_system_prefs_refresh();
    CHECK(lj_system_prefs_window() == NULL && memcmp(c->prefs, &dflt, sizeof dflt) == 0);
}

/* ---- About ---- */
static void test_about(void)
{
    char ver[64] = "", text[256];
    lj_window *w = lj_about_form_open();
    char path[1024];
    long n;
    snprintf(path, sizeof path, "%s/VERSION", LJ_SOURCE_DIR);
    n = read_file(path, ver, sizeof ver - 1);
    if (n > 0) { ver[n] = 0; while (n > 0 && (ver[n - 1] == '\n' || ver[n - 1] == '\r' || ver[n - 1] == ' ')) ver[--n] = 0; }
    CHECK(n > 0 && strcmp(ver, LJ_VERSION_STR) == 0);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "About LightJockey") == 0);
    snprintf(text, sizeof text, "LJ4EVR %s", ver);
    CHECK(w && strcmp(lj_w_caption(in(w, "Label2")), text) == 0);
    CHECK(w && strcmp(lj_w_caption(in(w, "Label4")), LJ_ABOUT_NOTICES) == 0);
    lj_win_about_text(text, sizeof text);
    CHECK(strstr(text, ver) != NULL && strstr(text, LJ_ABOUT_NOTICES) != NULL);
    {   /* ME-89: RichEdit1 (clBlack, Font.Color clWhite) carries white ink, and none in the 3 px pad left of its scrollbar (the long lines are cut) */
        lj_widget *re = in(w, "RichEdit1");
        const lj_surface *sf;
        int ax = 0, ay = 0, x, y, ink = 0, spill = 0;
        if (w) lj_window_paint(w);
        sf = w ? lj_window_surface(w) : NULL;
        if (re) lj_widget_abs(re, &ax, &ay);
        for (y = ay + 2; re && sf && y < ay + re->h - 2 && y < sf->h; y++)
            for (x = ax + 2; x < ax + re->w - 2 - LJ_SB_W && x < sf->w; x++)
                if (sf->px[y * sf->w + x] == 0xFFFFFFu) { if (x >= ax + re->w - 2 - LJ_SB_W - 3) spill++; else ink++; }
        CHECK(re && sf && ink > 200 && spill == 0);
    }
    lj_about_form_close();
    CHECK(lj_about_form_window() == NULL);
}

/* ---- Delete Files ---- */
static int row_of(enum lj_rec_kind k, uint32_t no) { int i; for (i = 0; i < lj_delete_files_count(k); i++) if (lj_delete_files_number(k, i) == no) return i; return -1; }
static int file_there(lj_library *lib, enum lj_rec_kind k, uint32_t no) { char p[LJ_PATH_CAP]; FILE *f; lj_library_record_path(lib, k, no, p, sizeof p); f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

static void test_delete_files(lj_library *lib)
{
    char l1[96], l2[16], path[LJ_PATH_CAP];
    lj_cue *q = lj_cue_new_rec();
    lj_window *w;
    lj_widget *lv;
    lj_mouse_ev m;
    save_seq(lib, 1, "Seq One"); save_seq(lib, 2, "Used By Cue"); save_seq(lib, 3, "Seq Three");
    q->number = 1; snprintf(q->name, sizeof q->name, "Cue One"); q->slot[0].seq = 2; q->slot[0].enabled = 1;
    CHECK(lj_library_record_path(lib, LJ_REC_CUE, 1, path, sizeof path) == 0 && lj_cue_save(path, q) == 0);
    lj_cue_free(q);

    {   /* W3 movement, LORD_FIDELITY-12: the Modified column is the file mtime; pin it to 22-10-2026 12:00 local so the golden is stable in any zone */
        struct tm tm; time_t t; uint32_t n;
        memset(&tm, 0, sizeof tm); tm.tm_year = 126; tm.tm_mon = 9; tm.tm_mday = 22; tm.tm_hour = 12; tm.tm_isdst = -1;
        t = mktime(&tm);
        for (n = 1; n <= 3; n++) { CHECK(lj_library_record_path(lib, LJ_REC_SEQ, n, path, sizeof path) == 0 && lj_t_settime(path, t) == 0); }
    }
    w = lj_win_open(LJ_WIN_DELETE_SEQ);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Delete Sequences") == 0 && lj_delete_files_count(LJ_REC_SEQ) == 3);
    click(in(w, "BCheck"));
    CHECK(lj_delete_files_in_use(row_of(LJ_REC_SEQ, 2)) == 1 && lj_delete_files_in_use(row_of(LJ_REC_SEQ, 1)) == 0);
    click(in(w, "bMark"));
    CHECK(lj_delete_files_selected(LJ_REC_SEQ, row_of(LJ_REC_SEQ, 1)) && lj_delete_files_selected(LJ_REC_SEQ, row_of(LJ_REC_SEQ, 3)));
    CHECK(!lj_delete_files_selected(LJ_REC_SEQ, row_of(LJ_REC_SEQ, 2)));       /* the unused filter keeps the cue-referenced sequence out */
    CHECK(strcmp(lj_w_caption(in(w, "BDelete")), "&Delete (2) Sequences") == 0);
    golden(w, "W2D_DELETESEQ");
    click(in(w, "BDelete"));
    CHECK(strcmp(msg_line("Label1"), "Delete (2) Sequences ?") == 0);
    msg_answer(1);
    CHECK(!file_there(lib, LJ_REC_SEQ, 1) && file_there(lib, LJ_REC_SEQ, 2) && !file_there(lib, LJ_REC_SEQ, 3));
    CHECK(lj_delete_files_count(LJ_REC_SEQ) == 1);

    /* the list's own multi-select: click, Ctrl+click, Shift+click */
    save_seq(lib, 4, "Four"); save_seq(lib, 5, "Five");
    lj_win_close(LJ_WIN_DELETE_SEQ);
    w = lj_win_open(LJ_WIN_DELETE_SEQ);
    lv = in(w, "LV1");
    memset(&m, 0, sizeof m); m.type = LJ_EV_MOUSE_DOWN; m.button = 1; m.x = 20;
    m.y = 2 + 17 + 0 * 14 + 5; lv->vt->mouse(lv, &m);
    m.y = 2 + 17 + 2 * 14 + 5; m.ctrl = 1; lv->vt->mouse(lv, &m);
    CHECK(lj_delete_files_selected(LJ_REC_SEQ, 0) && !lj_delete_files_selected(LJ_REC_SEQ, 1) && lj_delete_files_selected(LJ_REC_SEQ, 2));
    m.ctrl = 0; m.shift = 1; m.y = 2 + 17 + 1 * 14 + 5; lv->vt->mouse(lv, &m);
    CHECK(!lj_delete_files_selected(LJ_REC_SEQ, 0) && lj_delete_files_selected(LJ_REC_SEQ, 1) && lj_delete_files_selected(LJ_REC_SEQ, 2));   /* anchor = row 2 */
    click(in(w, "BClearAll"));
    CHECK(!lj_delete_files_selected(LJ_REC_SEQ, 1));
    click(in(w, "BDelete"));
    CHECK(lj_win_message_window() == NULL);                                       /* nothing selected: no prompt */
    lj_win_close(LJ_WIN_DELETE_SEQ);

    /* the other four: captions, the exe prompts */
    CHECK(lj_delete_files_prompt(LJ_REC_CUE, 1, l1, sizeof l1, l2, sizeof l2) == 0 && strcmp(l1, "Delete Cue file ?") == 0);
    CHECK(lj_delete_files_prompt(LJ_REC_CUE, 3, l1, sizeof l1, l2, sizeof l2) == 0 && strcmp(l1, "Delete 3 Cues ?") == 0);
    CHECK(lj_delete_files_prompt(LJ_REC_CL, 2, l1, sizeof l1, l2, sizeof l2) == 0 && strcmp(l1, "Delete Cuelist File?") == 0);
    CHECK(lj_delete_files_prompt(LJ_REC_BG, 2, l1, sizeof l1, l2, sizeof l2) == 0 && strcmp(l1, "Delete Background Cue files - are you sure ?") == 0);
    w = lj_win_open(LJ_WIN_DELETE_CUE);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Delete Cues") == 0 && lj_delete_files_count(LJ_REC_CUE) == 1);
    click(in(w, "BSelectAll"));
    click(in(w, "BDelete"));
    CHECK(strcmp(msg_line("Label1"), "Delete Cue file ?") == 0);
    msg_answer(0);
    CHECK(file_there(lib, LJ_REC_CUE, 1));                                        /* Cancel deletes nothing */
    lj_win_close(LJ_WIN_DELETE_CUE);
    CHECK(lj_win_open(LJ_WIN_DELETE_CL) && strcmp(lj_window_desc(lj_win_get(LJ_WIN_DELETE_CL))->caption, "Delete Cuelists") == 0);
    CHECK(lj_win_open(LJ_WIN_DELETE_BG) && lj_win_open(LJ_WIN_DELETE_MACRO));
    lj_win_close(LJ_WIN_DELETE_CL); lj_win_close(LJ_WIN_DELETE_BG); lj_win_close(LJ_WIN_DELETE_MACRO);
}

/* ---- User Libraries ---- */
static int lib_row(const char *name) { int i; for (i = 0; i < lj_user_libraries_count(); i++) if (strcmp(lj_user_libraries_name(i), name) == 0) return i; return -1; }

static void test_user_libraries(void)
{
    lj_library tmp;
    lj_window *w, *nw;
    CHECK(lj_library_open_default(TMP, &tmp) == 0);
    w = lj_win_open(LJ_WIN_USER_LIBRARIES);
    CHECK(w && lj_user_libraries_count() == 2 && lib_row("Default") >= 0 && lib_row("Lib") >= 0);
    CHECK(strcmp(lj_w_caption(in(w, "Label2")), "\"Lib\"") == 0);
    lj_user_libraries_select(lib_row("Lib"));
    CHECK(!in(w, "bDelete")->enabled && !in(w, "bRename")->enabled);            /* the open library is locked */
    lj_user_libraries_select(lib_row("Default"));
    CHECK(!in(w, "bDelete")->enabled);
    click(in(w, "bCreate"));
    nw = lj_user_lib_name_window();
    CHECK(nw && strcmp(lj_window_desc(nw)->caption, "Enter User Library Name") == 0 && !in(nw, "bOk")->enabled);
    lj_w_edit_set_text(in(nw, "eName"), "Show2");
    click(in(nw, "bOk"));
    lj_user_libraries_refresh();
    CHECK(lj_user_lib_name_window() == NULL && lj_library_open(TMP, "Show2", &tmp) == 0 && lib_row("Show2") >= 0);
    /* no golden: the Path column and the folder order are host-specific */
    lj_user_libraries_select(lib_row("Show2"));
    CHECK(in(w, "bDelete")->enabled);
    click(in(w, "bDelete"));
    CHECK(strcmp(msg_line("Label1"), "Delete User Library \"Show2\" ?") == 0);
    CHECK(strcmp(msg_line("Label2"), "(This will delete all information contained in the library)") == 0);
    msg_answer(1);
    CHECK(lib_row("Show2") < 0 && lj_library_open(TMP, "Show2", &tmp) != 0);
    lj_user_libraries_select(lib_row("Default"));
    click(in(w, "bSelect"));
    CHECK(strcmp(msg_line("Label1"), LJ_USERLIB_RESTART1) == 0);
    msg_answer(1);
    CHECK(act_count[LJ_ACT_SELECT_LIBRARY] == 1 && act_arg[LJ_ACT_SELECT_LIBRARY] == lib_row("Default"));
    click(in(w, "bClose"));
    lj_user_libraries_refresh();
    CHECK(lj_user_libraries_window() == NULL);
}

/* ---- Backup / Restore ---- */
static void test_backup_restore(lj_library *lib)
{
    static const char *const rel[] = { "Sequences/00000002.seq", "Cues/00000001.cue", "Notes.txt", "DirInfo.Ini" };
    static char before[4][8192], after[8192];
    long len[4];
    char path[LJ_PATH_CAP], bak[LJ_PATH_CAP];
    lj_window *w;
    int i;
    lj_library_path(lib, "Notes.txt", path, sizeof path);
    write_text(path, "rig notes\r\nline 2\r\n");
    for (i = 0; i < 4; i++) { lj_library_path(lib, rel[i], path, sizeof path); len[i] = read_file(path, before[i], sizeof before[i]); CHECK(len[i] >= 0); }
    w = lj_win_open(LJ_WIN_BACKUP);
    CHECK(w && strcmp(lj_w_caption(in(w, "lLibName")), "Lib") == 0);
    CHECK(lj_backup_set_notes("before the tour") == 0);
    CHECK(lj_backup_path(bak, sizeof bak) == 0 && strcmp(bak, TMP "Lib.lj4b") == 0);
    remove(bak);
    click(in(w, "bCreate"));
    CHECK(lj_win_message_window() == NULL && strcmp(lj_w_caption(in(w, "lAction")), bak) == 0);
    click(in(w, "bCreate"));                                                    /* the file exists now: the overwrite prompt */
    CHECK(strcmp(msg_line("Label1"), "File already exists") == 0);
    msg_answer(0);
    click(in(w, "bClose")); lj_backup_refresh();
    CHECK(lj_backup_window() == NULL);

    lj_library_path(lib, "Notes.txt", path, sizeof path); write_text(path, "changed");
    lj_library_path(lib, rel[0], path, sizeof path); remove(path);

    w = lj_win_open(LJ_WIN_RESTORE);
    CHECK(w && strcmp(lj_w_caption(in(w, "lRestore")), "Lib") == 0);
    CHECK(lj_restore_set_file(bak) == 0 && strcmp(lj_w_caption(in(w, "lLibName")), "Lib") == 0);
    CHECK(((lj_w_memo *)in(w, "Re1"))->n >= 1 && strcmp(((lj_w_memo *)in(w, "Re1"))->lines[0], "before the tour") == 0);
    click(in(w, "bRestore"));
    CHECK(strcmp(msg_line("Label1"), "Restore Backup files to \"Lib\" ?") == 0);
    msg_answer(1);
    CHECK(strcmp(msg_line("Label1"), "Re-start LightJockey ?") == 0);
    msg_answer(1);
    CHECK(act_count[LJ_ACT_RESTART] == 1);
    for (i = 0; i < 4; i++) {
        long n;
        lj_library_path(lib, rel[i], path, sizeof path);
        n = read_file(path, after, sizeof after);
        if (n != len[i] || memcmp(after, before[i], (size_t)n) != 0) printf("FAIL restore: %s differs\n", rel[i]);
        CHECK(n == len[i] && memcmp(after, before[i], (size_t)n) == 0);
    }
    lj_library_path(lib, "notes.txt", path, sizeof path);
    { long n = read_file(path, after, sizeof after); CHECK(n < 0 || n != 15 || memcmp(after, "before the tour", 15) != 0); }   /* the backup's own notes.txt is never extracted (it is Notes.txt on Windows) */
    lj_win_close(LJ_WIN_RESTORE);
}

/* ---- Notes ---- */
static void key(lj_widget *w, int type, int k, int ch) { lj_key_ev e; memset(&e, 0, sizeof e); e.type = type; e.key = k; e.ch = ch; w->vt->key(w, &e); }

static void test_notes(lj_library *lib)
{
    char path[LJ_PATH_CAP], buf[64];
    lj_window *w;
    lj_widget *m;
    lj_library_path(lib, LJ_NOTES_FILE, path, sizeof path); remove(path);
    w = lj_win_open(LJ_WIN_NOTES);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Notes") == 0 && strcmp(lj_notes_form_text(), "") == 0);
    m = in(w, "Memo1");
    key(m, LJ_EV_CHAR, 0, 'H'); key(m, LJ_EV_CHAR, 0, 'i'); key(m, LJ_EV_KEY_DOWN, 0x0D, 0); key(m, LJ_EV_CHAR, 0, 0xE9); key(m, LJ_EV_CHAR, 0, 'X'); key(m, LJ_EV_KEY_DOWN, LJ_VK_BACK, 0);
    CHECK(strcmp(lj_notes_form_text(), "Hi\r\n\xE9") == 0);                     /* CP1252 0xE9 kept as a byte */
    CHECK(((lj_w_memo *)m)->n == 2 && strcmp(((lj_w_memo *)m)->lines[1], "\xE9") == 0);
    golden(w, "W2D_NOTES");
    lj_win_close(LJ_WIN_NOTES);
    CHECK(read_file(path, buf, sizeof buf) == 5 && memcmp(buf, "Hi\r\n\xE9", 5) == 0);
    w = lj_win_open(LJ_WIN_NOTES);
    CHECK(strcmp(lj_notes_form_text(), "Hi\r\n\xE9") == 0);
    lj_win_close(LJ_WIN_NOTES);
}

/* ---- Striking Lamps ---- */
static int struck[8], nstruck;
static void rec_strike(int fx, void *ud) { (void)ud; if (nstruck < 8) struck[nstruck++] = fx; }

static void test_striking_lamps(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_show *sh = c->show;
    lj_fixture *f;
    if (!lj_fixlib_by_id(12) || !lj_fixlib_by_id(19)) { printf("SKIP striking lamps: fixlib rows 12/19 absent\n"); return; }
    f = &sh->fixture[0]; memset(f, 0, sizeof *f); f->kind = LJ_FX_BUILTIN; f->profile_id = 12; f->link = 1; f->address = 1;
    f = &sh->fixture[1]; memset(f, 0, sizeof *f); f->kind = LJ_FX_BUILTIN; f->profile_id = 19; f->link = 1; f->address = 17;   /* dimmer: no Lamp On */
    f = &sh->fixture[2]; memset(f, 0, sizeof *f); f->kind = LJ_FX_BUILTIN; f->profile_id = 12; f->link = 1; f->address = 21;
    lj_sel_none();
    lj_striking_lamps_set_strike(rec_strike, NULL);
    CHECK(lj_striking_lamps_start(0, 1000) == 2 && lj_striking_lamps_fixture(0) == 0 && lj_striking_lamps_fixture(1) == 2);
    CHECK(lj_striking_lamps_window() && strcmp(lj_window_desc(lj_striking_lamps_window())->caption, "Striking Lamps") == 0);
    CHECK(lj_striking_lamps_step(5000) == 1 && lj_striking_lamps_step(5500) == 1 && lj_striking_lamps_step(6000) == 2);
    CHECK(nstruck == 2 && struck[0] == 0 && struck[1] == 2);
    golden(lj_striking_lamps_window(), "W2D_STRIKINGLAMPS");
    lj_striking_lamps_refresh();
    CHECK(lj_striking_lamps_window() == NULL);                                    /* the startup run closes when done */
    /* the selection limits it; the default writer, the System menu run stays open until Stop */
    lj_sel_set(2, 1, 1);
    lj_striking_lamps_set_strike(NULL, NULL);
    CHECK(lj_striking_lamps_start(1, 1000) == 1 && lj_striking_lamps_step(0) == 1);
    CHECK(sh->current_seq->scenes[0].val[20] == 228 && sh->current_seq->scenes[0].state[20] == LJ_SNAP && sh->current_seq->scenes[0].state[0] == LJ_OFF);
    lj_striking_lamps_refresh();
    CHECK(lj_striking_lamps_window() != NULL);
    click(in(lj_striking_lamps_window(), "bClose"));
    lj_striking_lamps_refresh();
    CHECK(lj_striking_lamps_window() == NULL);
    lj_sel_none();
}

/* ---- Edit Hotkeys ---- */
static void test_edit_hotkeys(lj_library *lib)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *w;
    lj_hotkeys back;
    char path[LJ_PATH_CAP], err[64];
    lj_widget *ek;
    const lj_hotkey *k;
    lj_hotkeys_init(c->hotkeys);
    c->prefs->hotkey_setup = 1;
    w = lj_win_open(LJ_WIN_EDIT_HOTKEYS);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "Edit Hotkeys") == 0);
    CHECK(strcmp(lj_w_caption(in(w, "bAssignProg")), "Assign &Sequence") == 0);
    ek = in(w, "eKey");
    key(ek, LJ_EV_KEY_DOWN, 'B', 0); { lj_key_ev e; memset(&e, 0, sizeof e); e.type = LJ_EV_KEY_DOWN; e.key = 'B'; e.shift = 1; ek->vt->key(ek, &e); }
    CHECK(strcmp(lj_w_edit_text(ek), "[Shift] B") == 0);
    CHECK(lj_edit_hotkeys_select_fn(LJ_FN_BLACKOUT_RESTORE) == 0 && lj_edit_hotkeys_assign_fn() == 0);
    k = lj_hotkeys_find_fn(lj_edit_hotkeys_work(), LJ_FN_BLACKOUT_RESTORE, 0);
    CHECK(k && k->key == 'B' && k->shift == 1 && k->ctrl == 0);
    CHECK(lj_edit_hotkeys_select_fn(LJ_FN_CLOSE_WINDOW) == 0 && lj_edit_hotkeys_assign_fn() == -1);   /* the conflict */
    CHECK(strcmp(msg_line("Label1"), "Key Combo [Shift] B is already assigned to Blackout/Restore") == 0);
    msg_answer(1);
    CHECK(lj_hotkeys_find_fn(lj_edit_hotkeys_work(), LJ_FN_CLOSE_WINDOW, 0) == NULL);
    CHECK(lj_edit_hotkeys_capture('C', 0, 0, 1) == -1 && lj_edit_hotkeys_capture(0x10, 1, 0, 0) == -1);   /* Alt, bare Shift */
    CHECK(strcmp(lj_w_edit_text(ek), "[Shift] B") == 0);
    CHECK(lj_edit_hotkeys_capture(0x74, 0, 1, 0) == 0 && strcmp(lj_w_edit_text(ek), "[Control] F5") == 0);
    CHECK(lj_edit_hotkeys_select_prog(1, 1) == 0 && strcmp(lj_w_caption(in(w, "bAssignProg")), "Assign &Cue") == 0);
    CHECK(lj_edit_hotkeys_assign_prog() == 0);
    k = lj_hotkeys_find_fn(lj_edit_hotkeys_work(), LJ_FN_PROG_CUE, 1);
    CHECK(k && k->key == 0x74 && k->ctrl == 1);
    CHECK(lj_edit_hotkeys_select_fn(LJ_FN_BLACKOUT) == 0 && lj_edit_hotkeys_assign_fn() == -1);
    CHECK(strcmp(msg_line("Label1"), "Key Combo [Control] F5 is already assigned to Cue 0001") == 0);
    msg_answer(1);
    golden(w, "W2D_EDITHOTKEYS");
    click(in(w, "bCancel"));                                                        /* modified: the exe's prompt */
    CHECK(strcmp(msg_line("Label1"), LJ_HOTKEY_CHANGED) == 0);
    msg_answer(0);
    lj_edit_hotkeys_refresh();
    CHECK(lj_edit_hotkeys_window() != NULL);
    click(in(w, "bOk"));
    lj_edit_hotkeys_refresh();
    CHECK(lj_edit_hotkeys_window() == NULL && c->hotkeys->n == 2);
    CHECK(lj_hotkeys_setup_path(lib->root, 1, path, sizeof path) == 0 && lj_hotkeys_load(path, &back, err, sizeof err) == 0 && back.n == 2);
    /* setup 8 */
    c->prefs->hotkey_setup = 8;
    w = lj_win_open(LJ_WIN_EDIT_HOTKEYS);
    CHECK(lj_edit_hotkeys_setup(8) == 0 && lj_edit_hotkeys_work()->n == 0);         /* a missing Setup8.Hot reads as an empty setup */
    CHECK(lj_edit_hotkeys_capture('Q', 0, 0, 0) == 0 && lj_edit_hotkeys_select_fn(LJ_FN_BLACKOUT) == 0 && lj_edit_hotkeys_assign_fn() == 0);
    CHECK(lj_edit_hotkeys_accept() == 0);
    lj_win_close(LJ_WIN_EDIT_HOTKEYS);
    CHECK(lj_hotkeys_setup_path(lib->root, 8, path, sizeof path) == 0 && lj_hotkeys_load(path, &back, err, sizeof err) == 0 && back.n == 1 && back.e[0].key == 'Q');
    CHECK(lj_hotkeys_setup_path(lib->root, 1, path, sizeof path) == 0 && lj_hotkeys_load(path, &back, err, sizeof err) == 0 && back.n == 2);   /* setup 1 untouched */
    c->prefs->hotkey_setup = 1;
}

/* ---- MIDI-IN ---- */
static void pump(void) { uint8_t msg[3]; while (lj_midi_poll(msg)) { lj_midi_dispatch(lj_ctx_get()->midi, msg); lj_midi_in_form_log(msg); } }

static void test_midi_in(void)
{
    lj_ctx *c = lj_ctx_get();
    static const uint8_t on[3] = { 0x90, 0x3C, 0x7F }, off[3] = { 0x80, 0x3C, 0x00 };
    uint8_t msg[3];
    const lj_midi_map_entry *e;
    lj_window *w;
    int i, found = 0;
    lj_midi_init(c->midi);
    w = lj_win_open(LJ_WIN_MIDI_IN);
    CHECK(w && strcmp(lj_window_desc(w)->caption, "LightJockey MIDI-IN") == 0);
    CHECK(lj_w_checkbox_checked(in(w, "cbNote0")) == 1 && lj_w_checkbox_checked(in(w, "cbSeqOff")) == 1);   /* the shipped MIDIIn.Ini */
    CHECK(lj_midi_in_form_tab(2) == 0 && strcmp(lj_w_caption(in(w, "bSet")), "<- Set Cue#") == 0);
    click(in(w, "cbEnable"));
    CHECK(c->midi->enabled[LJ_MIDI_CUE] == 1);
    CHECK(lj_midi_in_form_set_number(12) == 0 && lj_midi_in_form_learn() == 0);
    lj_midi_inject(on);
    pump();
    lj_midi_in_form_refresh();
    for (i = 0; i < c->midi->n; i++) if (c->midi->e[i].kind == LJ_MIDI_CUE && c->midi->e[i].note == 60 && c->midi->e[i].num == 12 && c->midi->e[i].ch == LJ_MIDI_CH_ALL) found = 1;
    CHECK(found);
    CHECK(strcmp(((lj_w_listview *)in(w, "lvCue"))->items[60], "60\t12\t") == 0);
    lj_midi_inject(off);
    pump();
    lj_midi_inject(on);
    CHECK(lj_midi_poll(msg) == 1);
    e = lj_midi_dispatch(c->midi, msg);                                            /* the next note latches cue 12 */
    CHECK(e && e->kind == LJ_MIDI_CUE && e->num == 12);
    lj_midi_in_form_log(msg);
    lj_midi_in_form_tab(6);
    CHECK(((lj_w_memo *)in(w, "mmLog"))->n >= 1);
    lj_midi_in_form_tab(1);
    golden(w, "W2D_MIDIIN_SEQ");
    act_count[LJ_ACT_MIDI_SAVE] = 0;
    click(in(w, "bClose"));
    lj_midi_in_form_refresh();
    CHECK(lj_midi_in_form_window() == NULL && act_count[LJ_ACT_MIDI_SAVE] >= 1);   /* Auto Save (shipped 1) */
}

int lj_test_windows_d(int argc, char **argv);
int lj_test_windows_d(int argc, char **argv)
{
    static lj_library lib;
    static lj_sysprefs prefs;
    static lj_hotkeys hk;
    static lj_midi_map midi;
    lj_ctx *c = lj_ctx_get();
    int a;
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    lj_library_delete(TMP, "Lib"); lj_library_delete(TMP, "Show2");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->show->current_seq = lj_sequence_new_rec(); c->current_scene = 0;
    lj_sysprefs_defaults(&prefs); c->prefs = &prefs;
    lj_hotkeys_init(&hk); c->hotkeys = &hk;
    lj_midi_init(&midi); c->midi = &midi;
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, on_action, NULL);

    test_system_prefs();
    test_about();
    test_delete_files(&lib);
    test_user_libraries();
    test_backup_restore(&lib);
    test_notes(&lib);
    test_striking_lamps();
    test_edit_hotkeys(&lib);
    test_midi_in();

    lj_win_close_all();
    lj_warnings_close_all();
    for (a = 0; a < LJ_ACT_COUNT; a++) lj_desktop_bind_action((enum lj_desktop_action)a, NULL, NULL);
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(c->show); c->show = NULL;
    c->lib = NULL; c->prefs = NULL; c->hotkeys = NULL; c->midi = NULL;
    lj_log_set_sink(NULL, NULL);
    printf("windows_d: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
