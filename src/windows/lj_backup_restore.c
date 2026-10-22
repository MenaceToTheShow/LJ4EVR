/* lj_backup_restore.c - see lj_backup_restore.h. Handlers: TLIBRARYBACKUPFORM FormCreate FormDestroy
 * bCreateClick bCloseClick; TLIBRARYRESTOREFORM FormCreate FormDestroy FileListChange bRestoreClick
 * bCloseClick (ME-16). */
#define _CRT_SECURE_NO_WARNINGS 1   /* localtime: the UI thread is the only caller */
#include "windows/lj_backup_restore.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_user_libraries.h"
#include "windows/lj_warnings.h"

#define LJ_BR_WRITE_ERR   "Error Writing Backup File - Backup file not saved"   /* exe 0x14FE74 */
#define LJ_BR_RESTORE_ERR "Error restoring file :"                              /* exe 0x2326CC */

static struct { lj_window *win; lj_memo_edit ed; int close_pending; char pending[LJ_PATH_CAP]; } bk;
static struct {
    lj_window *win; lj_widget *files; int close_pending; char path[LJ_PATH_CAP];
    int n; char file[64][LJ_PATH_CAP]; char shown[64][LJ_LIBNAME_CAP + 8]; const char *items[64];   /* ponytail: one backup per library (64 libraries) */
    char notes[LJ_NOTES_CAP]; char split[LJ_NOTES_CAP]; const char *lines[256];
} rs;

static int exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) fclose(f); return f != NULL; }
static void label(lj_window *w, const char *name, const char *text) { lj_w_set_caption(w ? lj_window_find(w, name) : NULL, text); }
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

/* the library Backup works on: the one picked in User Libraries while that window is open, else the open one */
static int backup_lib(lj_library *out)
{
    lj_ctx *c = lj_ctx_get();
    const char *sel = lj_user_libraries_selected();
    if (sel) return lj_library_open(c->userdir, sel, out);
    if (!c->lib) return -1;
    *out = *c->lib;
    return 0;
}

/* ---- Backup ---- */

int lj_backup_path(char *out, size_t cap)
{
    lj_ctx *c = lj_ctx_get();
    lj_library lib;
    int n;
    if (!out || !cap || backup_lib(&lib) != 0) return -1;
    n = snprintf(out, cap, "%s%s" LJ_LIBRARY_BACKUP_EXT, c->userdir, lib.name);   /* design 5.2: <name>.lj4b; SaveDialog1 (*.LJB) has no platform dialog yet */
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

int lj_backup_set_notes(const char *text) { return bk.win ? lj_memo_edit_set(&bk.ed, text ? text : "") : -1; }

int lj_backup_create(const char *path)
{
    lj_library lib;
    char def[LJ_PATH_CAP];
    lj_widget *pg = bk.win ? lj_window_find(bk.win, "pg1") : NULL;
    if (!path) { if (lj_backup_path(def, sizeof def) != 0) return -1; path = def; }
    label(bk.win, "lAction", "Backing up - please wait");                                  /* PROPOSAL rig: lAction's runtime texts */
    if (backup_lib(&lib) != 0 || lj_library_backup(&lib, path, bk.win ? bk.ed.text : "") != 0) {
        label(bk.win, "lAction", "");
        lj_win_message("Warning", LJ_BR_WRITE_ERR);
        return -1;
    }
    if (pg) lj_w_progressbar_set(pg, 100);
    label(bk.win, "lAction", path);
    return 0;
}

static void on_overwrite(int yes, void *ud) { (void)ud; if (yes) lj_backup_create(bk.pending); }
static void on_create(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (lj_backup_path(bk.pending, sizeof bk.pending) != 0) return;
    if (exists(bk.pending)) lj_win_confirm("Please Confirm..", "File already exists", "Overwrite the backup file ?", on_overwrite, NULL);   /* exe 0x14D4F0 + PROPOSAL second line */
    else lj_backup_create(bk.pending);
}
static void on_bk_close(lj_widget *w, void *ud) { (void)w; (void)ud; bk.close_pending = 1; }
static const lj_binding lj_bk_bindings[] = {
    { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "bCreateClick", on_create, NULL }, { "bCloseClick", on_bk_close, NULL }, { NULL, NULL, NULL } };

lj_window *lj_backup_open(void)
{
    lj_library lib;
    int have = backup_lib(&lib) == 0;
    char when[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (bk.win) return bk.win;
    bk.win = lj_window_create(&lj_form_TLIBRARYBACKUPFORM, lj_bk_bindings);
    if (!bk.win) { lj_log("backup: cannot create TLibraryBackupForm"); return NULL; }
    bk.close_pending = 0; bk.ed.text[0] = 0; bk.ed.changed = 0;
    if (!tm || strftime(when, sizeof when, "%Y-%m-%d %H:%M", tm) == 0) when[0] = 0;
    label(bk.win, "lLibName", have ? lib.name : "");
    label(bk.win, "lLibPath", have ? lib.root : "");
    label(bk.win, "lBackupTime", when);
    label(bk.win, "lAction", "");
    lj_memo_edit_attach(&bk.ed, lj_window_find(bk.win, "Re1"));
    lj_win_show(bk.win, 0);
    return bk.win;
}

void lj_backup_close(void)
{
    if (!bk.win) return;
    lj_memo_edit_detach(&bk.ed);
    lj_win_hide(bk.win);
    lj_window_destroy(bk.win);
    bk.win = NULL; bk.close_pending = 0;
}
lj_window *lj_backup_window(void) { return bk.win; }
void lj_backup_refresh(void) { if (bk.close_pending) lj_backup_close(); }

/* ---- Restore ---- */

static void show_notes(void)
{
    int n = 0;
    char *p = rs.split;
    lj_str_copy(rs.split, sizeof rs.split, rs.notes);
    rs.lines[n++] = p;
    for (; *p && n < LJ_COUNT_OF(rs.lines); p++) if (*p == '\n') { *p = 0; if (p > rs.split && p[-1] == '\r') p[-1] = 0; rs.lines[n++] = p + 1; }
    lj_w_memo_set_lines(rs.win ? lj_window_find(rs.win, "Re1") : NULL, rs.lines, n);
}

int lj_restore_set_file(const char *path)
{
    const char *base, *s;
    char name[LJ_LIBNAME_CAP];
    size_t len;
    if (!path || !exists(path)) return -1;
    lj_str_copy(rs.path, sizeof rs.path, path);
    for (base = s = path; *s; s++) if (*s == '/' || *s == '\\') base = s + 1;
    len = strlen(base) > strlen(LJ_LIBRARY_BACKUP_EXT) ? strlen(base) - strlen(LJ_LIBRARY_BACKUP_EXT) : strlen(base);
    if (len >= sizeof name) len = sizeof name - 1;
    memcpy(name, base, len); name[len] = 0;
    if (lj_library_backup_notes(path, rs.notes, sizeof rs.notes) != 0) rs.notes[0] = 0;
    label(rs.win, "lLibName", name);                                   /* the archive carries no original name / path / time (model, recorded): the file name stands in */
    label(rs.win, "lLibPath", ""); label(rs.win, "lBackupTime", "");
    label(rs.win, "lPath", lj_ctx_get()->lib ? lj_ctx_get()->lib->root : "");   /* Target Library Path = the library the files go to (userlibrestore.gif), not the archive */
    show_notes();
    { lj_widget *b = rs.win ? lj_window_find(rs.win, "bRestore") : NULL; if (b) { b->enabled = 1; lj_widget_invalidate(b); } }   /* Enabled = False in the DFM until a file is picked */
    return 0;
}

static void on_restart(int yes, void *ud) { (void)ud; if (yes) lj_desktop_run_action(LJ_ACT_RESTART, 0); }
static void on_restore_yes(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    if (!yes || !c->lib) return;
    if (lj_library_restore(c->lib, rs.path) != 0) { lj_win_message("Warning", LJ_BR_RESTORE_ERR); return; }
    lj_win_confirm("Please Confirm..", "Re-start LightJockey ?", "(There will be no prompt for unsaved data)", on_restart, NULL);   /* exe 0x2308BC */
}

int lj_restore_run(void)
{
    lj_ctx *c = lj_ctx_get();
    char l1[LJ_LIBNAME_CAP + 40];
    if (!rs.path[0] || !c->lib) return -1;
    snprintf(l1, sizeof l1, "Restore Backup files to \"%s\" ?", c->lib->name);   /* PROPOSAL rig: Label7's words; LJ2's restore prompt is not in the exe pool */
    lj_win_confirm("Please Confirm..", l1, "(Files with the same names in the library are replaced)", on_restore_yes, NULL);
    return 0;
}

static void on_pick(lj_widget *w, int ev, void *ud)
{
    int i = ((lj_w_list *)w)->index;
    (void)ud;
    if ((ev == LJ_WEV_CLICK || ev == LJ_WEV_CHANGE) && i >= 0 && i < rs.n) lj_restore_set_file(rs.file[i]);
}

static void fill_files(void)
{
    lj_ctx *c = lj_ctx_get();
    char names[64][LJ_LIBNAME_CAP];
    int i, n = lj_library_list(c->userdir, names, 64);
    rs.n = 0;
    for (i = 0; i < n; i++) {
        if (snprintf(rs.file[rs.n], sizeof rs.file[rs.n], "%s%s" LJ_LIBRARY_BACKUP_EXT, c->userdir, names[i]) >= (int)sizeof rs.file[rs.n] || !exists(rs.file[rs.n])) continue;
        snprintf(rs.shown[rs.n], sizeof rs.shown[rs.n], "%s" LJ_LIBRARY_BACKUP_EXT, names[i]);
        rs.items[rs.n] = rs.shown[rs.n];
        rs.n++;
    }
    if (rs.files) lj_w_list_set_items(rs.files, rs.items, rs.n);
}

static void on_file_change(lj_widget *w, void *ud) { (void)w; (void)ud; }   /* the stub FileList never fires; the code-built list does (on_pick) */
static void on_rs_restore(lj_widget *w, void *ud) { (void)w; (void)ud; lj_restore_run(); }
static void on_rs_close(lj_widget *w, void *ud) { (void)w; (void)ud; rs.close_pending = 1; }
static const lj_binding lj_rs_bindings[] = {
    { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FileListChange", on_file_change, NULL }, { "bRestoreClick", on_rs_restore, NULL },
    { "bCloseClick", on_rs_close, NULL }, { NULL, NULL, NULL } };

lj_window *lj_restore_open(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *fl, *w;
    if (rs.win) return rs.win;
    rs.win = lj_window_create(&lj_form_TLIBRARYRESTOREFORM, lj_rs_bindings);
    if (!rs.win) { lj_log("restore: cannot create TLibraryRestoreForm"); return NULL; }
    rs.close_pending = 0; rs.path[0] = 0; rs.notes[0] = 0; rs.files = NULL;
    if ((w = lj_window_find(rs.win, "DriveList")) != NULL) w->visible = 0;          /* ME-17 stubs, see the header */
    if ((w = lj_window_find(rs.win, "DirList")) != NULL) w->visible = 0;
    if ((fl = lj_window_find(rs.win, "FileList")) != NULL && fl->parent) {
        rs.files = lj_w_listbox_create(fl->parent, NULL);
        rs.files->x = fl->x; rs.files->y = fl->y; rs.files->w = fl->w; rs.files->h = fl->h;
        rs.files->on_event = on_pick; rs.files->ud = NULL;
        fl->visible = 0;
    }
    label(rs.win, "lLibName", ""); label(rs.win, "lLibPath", ""); label(rs.win, "lBackupTime", ""); label(rs.win, "lPath", "");
    label(rs.win, "lRestore", c->lib ? c->lib->name : "");
    fill_files();
    show_notes();
    lj_win_show(rs.win, 0);
    return rs.win;
}

void lj_restore_close(void)
{
    if (!rs.win) return;
    lj_win_hide(rs.win);
    lj_window_destroy(rs.win);
    rs.win = NULL; rs.files = NULL; rs.close_pending = 0;
}
lj_window *lj_restore_window(void) { return rs.win; }
void lj_restore_refresh(void) { if (rs.close_pending) lj_restore_close(); }
