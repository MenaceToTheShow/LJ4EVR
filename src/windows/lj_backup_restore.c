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
#include "model/lj_ljb.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_file_dialogs.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_user_libraries.h"
#include "windows/lj_warnings.h"

#define LJ_BR_WRITE_ERR   "Error Writing Backup File - Backup file not saved"   /* exe 0x14FE74 */
#define LJ_BR_RESTORE_ERR "Error restoring file :"                              /* exe 0x2326CC */

static struct { lj_window *win; lj_memo_edit ed; int close_pending; char pending[LJ_PATH_CAP]; } bk;
enum { LJ_RS_ROWS = 64 };   /* ponytail: 64 file / folder rows shown; a fuller folder is cut at 64 */
static struct {
    lj_window *win; lj_widget *files, *dirs; int close_pending; char path[LJ_PATH_CAP];
    int n; char file[LJ_RS_ROWS][LJ_PATH_CAP]; char shown[LJ_RS_ROWS][LJ_DIRENT_CAP]; const char *items[LJ_RS_ROWS];
    char dir[LJ_PATH_CAP]; int ndir, nup; char drow[LJ_RS_ROWS][LJ_DIRENT_CAP + 2 * LJ_RS_ROWS]; char dsub[LJ_RS_ROWS][LJ_DIRENT_CAP]; const char *ditems[LJ_RS_ROWS];
    char notes[LJ_NOTES_CAP]; char split[LJ_NOTES_CAP]; const char *lines[256];
} rs;

static int exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) fclose(f); return f != NULL; }

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
    lj_window_set_text(bk.win, "lAction", "Backing up - please wait");                                  /* PROPOSAL rig: lAction's runtime texts */
    if (backup_lib(&lib) == 0) { lj_file_dlg_progress(LJ_WIN_BACKUP_PROGRESS, "Label3", lib.root, 0); lj_file_dlg_progress(LJ_WIN_BACKUP_PROGRESS, "Label4", path, 0); }   /* TBackupForm "Backing Up": Reading / Writing */
    if (backup_lib(&lib) != 0 || lj_library_backup(&lib, path, bk.win ? bk.ed.text : "") != 0) {
        lj_file_dlg_progress(LJ_WIN_BACKUP_PROGRESS, NULL, NULL, -1);
        lj_window_set_text(bk.win, "lAction", "");
        lj_win_message("Warning", LJ_BR_WRITE_ERR);
        return -1;
    }
    lj_file_dlg_progress(LJ_WIN_BACKUP_PROGRESS, NULL, NULL, -1);
    if (pg) lj_w_progressbar_set(pg, 100);
    lj_window_set_text(bk.win, "lAction", path);
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
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bCreateClick", on_create, NULL }, { "bCloseClick", on_bk_close, NULL }, { NULL, NULL, NULL } };

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
    lj_window_set_text(bk.win, "lLibName", have ? lib.name : "");
    lj_window_set_text(bk.win, "lLibPath", have ? lib.root : "");
    lj_window_set_text(bk.win, "lBackupTime", when);
    lj_window_set_text(bk.win, "lAction", "");
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

/* ---- W5 ljb: the Restore window accepts *.LJB beside .lj4b (PROPOSAL entry, W5-LJB-4). The file imports into a NEW
 * library (lj_ljb_import: no existing library is written), the report replaces the notes box, then the User Libraries
 * restart prompt opens the new library through LJ_ACT_SELECT_LIBRARY. ---- */
static struct { int on; lj_library lib; lj_ljb_report rep; char name[LJ_LIBNAME_CAP]; } rb;

static int is_ljb(const char *path)
{
    size_t n = strlen(path);
    return n > 4 && path[n - 4] == '.' && (path[n - 3] | 0x20) == 'l' && (path[n - 2] | 0x20) == 'j' && (path[n - 1] | 0x20) == 'b';
}

static int ljb_set_file(const char *path)
{
    char err[128];
    lj_str_copy(rs.path, sizeof rs.path, path);
    rb.on = 1;
    if (lj_ljb_preview(path, rb.name, sizeof rb.name, rs.notes, sizeof rs.notes, err, sizeof err) != 0) { rb.name[0] = 0; lj_str_copy(rs.notes, sizeof rs.notes, err); }
    lj_window_set_text(rs.win, "lLibName", rb.name);                    /* Info.$$$'s name; its source path is never shown (W5-X-PRIVATE) */
    lj_window_set_text(rs.win, "lRestore", rb.name);                    /* ME-181: the NEW library it imports into, never the current one */
    lj_window_set_text(rs.win, "lLibPath", ""); lj_window_set_text(rs.win, "lBackupTime", "");   /* the TDateTime is not decoded (no double in model/) */
    lj_window_set_text(rs.win, "lPath", "");
    show_notes();
    { lj_widget *b = rs.win ? lj_window_find(rs.win, "bRestore") : NULL; if (b) { b->enabled = 1; lj_widget_invalidate(b); } }
    return 0;
}

static void on_ljb_restart(int yes, void *ud)
{
    char names[64][LJ_LIBNAME_CAP];
    int i, n = lj_library_list(lj_ctx_get()->userdir, names, 64);
    (void)ud;
    for (i = 0; yes && i < n; i++) if (strcmp(names[i], rb.lib.name) == 0) { lj_desktop_run_action(LJ_ACT_SELECT_LIBRARY, i); return; }
}

static void on_ljb_yes(int yes, void *ud)
{
    char err[160];
    size_t at;
    int i;
    (void)ud;
    if (!yes) return;
    if (lj_ljb_import(rs.path, lj_ctx_get()->userdir, NULL, &rb.lib, &rb.rep, err, sizeof err) != 0) { lj_win_message("Warning", err); return; }
    if (snprintf(rs.notes, sizeof rs.notes, "Imported into \"%s\": %d converted, %d not converted (" LJ_LJB_REPORT ")\n", rb.lib.name, rb.rep.converted, rb.rep.skipped) < 0) rs.notes[0] = 0;   /* PROPOSAL text */
    for (i = 0; i < rb.rep.nlines; i++) { at = strlen(rs.notes); if (snprintf(rs.notes + at, sizeof rs.notes - at, "%s\n", rb.rep.lines[i]) < 0) (rs.notes + at)[0] = 0; }
    show_notes();
    lj_window_set_text(rs.win, "lPath", rb.lib.root);                   /* ME-181: Target Library Path is a path (the .lj4b row's rule) */
    lj_win_confirm("Please Confirm..", LJ_USERLIB_RESTART1, LJ_USERLIB_RESTART2, on_ljb_restart, NULL);
}

static int ljb_run(void)
{
    char l1[LJ_LIBNAME_CAP + 48];
    if (snprintf(l1, sizeof l1, "Import \"%s\" as a new user library ?", rb.name[0] ? rb.name : "LJB") < 0) l1[0] = 0;   /* PROPOSAL rig: LJ2 restores .LJB over a library [06 s4] */
    lj_win_confirm("Please Confirm..", l1, "(No existing library is changed)", on_ljb_yes, NULL);
    return 0;
}

int lj_restore_set_file(const char *path)
{
    const char *base, *s;
    char name[LJ_LIBNAME_CAP];
    size_t len;
    if (!path || !exists(path)) return -1;
    rb.on = 0;
    if (is_ljb(path)) return ljb_set_file(path);
    lj_str_copy(rs.path, sizeof rs.path, path);
    for (base = s = path; *s; s++) if (*s == '/' || *s == '\\') base = s + 1;
    len = strlen(base) > strlen(LJ_LIBRARY_BACKUP_EXT) ? strlen(base) - strlen(LJ_LIBRARY_BACKUP_EXT) : strlen(base);
    if (len >= sizeof name) len = sizeof name - 1;
    memcpy(name, base, len); name[len] = 0;
    if (lj_library_backup_notes(path, rs.notes, sizeof rs.notes) != 0) rs.notes[0] = 0;
    lj_window_set_text(rs.win, "lLibName", name);                                   /* the archive carries no original name / path / time (model, recorded): the file name stands in */
    lj_window_set_text(rs.win, "lLibPath", ""); lj_window_set_text(rs.win, "lBackupTime", "");
    lj_window_set_text(rs.win, "lPath", lj_ctx_get()->lib ? lj_ctx_get()->lib->root : "");   /* Target Library Path = the library the files go to (userlibrestore.gif), not the archive */
    lj_window_set_text(rs.win, "lRestore", lj_ctx_get()->lib ? lj_ctx_get()->lib->name : "");   /* an .LJB picked before named its new library here */
    show_notes();
    { lj_widget *b = rs.win ? lj_window_find(rs.win, "bRestore") : NULL; if (b) { b->enabled = 1; lj_widget_invalidate(b); } }   /* Enabled = False in the DFM until a file is picked */
    return 0;
}

static void on_restart(int yes, void *ud) { (void)ud; if (yes) lj_desktop_run_action(LJ_ACT_RESTART, 0); }
static void on_restore_yes(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    int rc;
    (void)ud;
    if (!yes || !c->lib) return;
    lj_file_dlg_progress(LJ_WIN_RESTORE_LIB_PROGRESS, NULL, NULL, 0);   /* "Deleting current files from directory" PG1, "Copying Backup Files into directory" PG2 */
    rc = lj_library_replace(c->lib, rs.path);                           /* restoring "will completely replace the existing contents of the library" [06 s4.1] */
    lj_w_progressbar_set(lj_window_find(lj_win_get(LJ_WIN_RESTORE_LIB_PROGRESS), "PG1"), 2);
    lj_w_progressbar_set(lj_window_find(lj_win_get(LJ_WIN_RESTORE_LIB_PROGRESS), "PG2"), 2);
    lj_file_dlg_progress(LJ_WIN_RESTORE_LIB_PROGRESS, NULL, NULL, -1);
    if (rc != 0) { lj_win_message("Warning", LJ_BR_RESTORE_ERR); return; }
    lj_win_confirm("Please Confirm..", "Re-start LightJockey ?", "(There will be no prompt for unsaved data)", on_restart, NULL);   /* exe 0x2308BC */
}

int lj_restore_run(void)
{
    lj_ctx *c = lj_ctx_get();
    char l1[LJ_LIBNAME_CAP + 40];
    if (rs.path[0] && rb.on) return ljb_run();
    if (!rs.path[0] || !c->lib) return -1;
    if (snprintf(l1, sizeof l1, "Restore Backup files to \"%s\" ?", c->lib->name) < 0) l1[0] = 0;   /* PROPOSAL rig: Label7's words; LJ2's restore prompt is not in the exe pool */
    lj_win_confirm("Please Confirm..", l1, "(Files with the same names in the library are replaced)", on_restore_yes, NULL);
    return 0;
}

static void on_pick(lj_widget *w, int ev, void *ud)
{
    int i = ((lj_w_list *)w)->index;
    (void)ud;
    if ((ev == LJ_WEV_CLICK || ev == LJ_WEV_CHANGE) && i >= 0 && i < rs.n) lj_restore_set_file(rs.file[i]);
}

/* The Restore folder tree (W4-DLG-1): TDirectoryListBox rows = the folders of rs.dir from the root, indented two
 * spaces per level, then its sub-folders one level deeper; TFileListBox = the *.lj4b files of rs.dir (Mask "*.LJB" in
 * the DFM: the extension is ours, F51). Text rows without folder bitmaps (D2); the drive combo stays hidden. */
static const char *rs_join(const char *dir)   /* the separator to append before a name ("" when dir ends in one) */
{
    size_t len = strlen(dir);
    if (len && (dir[len - 1] == '/' || dir[len - 1] == '\\')) return "";
    return strchr(dir, '\\') ? "\\" : "/";
}

static void fill_files(void)
{
    char names[LJ_RS_ROWS][LJ_DIRENT_CAP];
    int i, n = lj_library_dir_list(rs.dir, LJ_LIBRARY_BACKUP_EXT, names, LJ_RS_ROWS);
    rs.n = 0;
    for (i = 0; i < n; i++) {
        if (snprintf(rs.file[rs.n], sizeof rs.file[rs.n], "%s%s%s", rs.dir, rs_join(rs.dir), names[i]) >= (int)sizeof rs.file[rs.n]) continue;
        lj_str_copy(rs.shown[rs.n], sizeof rs.shown[rs.n], names[i]);
        rs.items[rs.n] = rs.shown[rs.n];
        rs.n++;
    }
    n = lj_library_dir_list(rs.dir, ".LJB", names, LJ_RS_ROWS - rs.n);   /* W5 ljb: LJ2 backups listed after ours (any case) */
    for (i = 0; i < n; i++) {
        if (snprintf(rs.file[rs.n], sizeof rs.file[rs.n], "%s%s%s", rs.dir, rs_join(rs.dir), names[i]) >= (int)sizeof rs.file[rs.n]) continue;
        lj_str_copy(rs.shown[rs.n], sizeof rs.shown[rs.n], names[i]);
        rs.items[rs.n] = rs.shown[rs.n];
        rs.n++;
    }
    if (rs.files) lj_w_list_set_items(rs.files, rs.items, rs.n);
}

/* component k of rs.dir ends at end[k] (exclusive, the separator kept for the root: "D:\" or "/") */
static int rs_split(size_t end[LJ_RS_ROWS])
{
    size_t i, len = strlen(rs.dir);
    int n = 0, in = 0;
    for (i = 0; i <= len && n < LJ_RS_ROWS; i++) {
        int sep = i == len || rs.dir[i] == '/' || rs.dir[i] == '\\';
        if (!sep) { in = 1; continue; }
        if (in) { end[n] = n == 0 ? i + (i < len) : i; n++; }          /* "D:" keeps its "\" */
        else if (n == 0 && i == 0 && i < len) end[n++] = 1;     /* POSIX root "/" */
        in = 0;
    }
    return n;
}

static void fill_dirs(void)
{
    size_t end[LJ_RS_ROWS], start = 0;
    int n = rs_split(end), i;
    for (i = 0; i < n; i++) {
        while (i > 0 && start < end[i] && (rs.dir[start] == '/' || rs.dir[start] == '\\')) start++;
        snprintf(rs.drow[i], sizeof rs.drow[i], "%*s%.*s", 2 * i, "", (int)(end[i] - start), rs.dir + start);
        start = end[i];
    }
    rs.nup = n;
    rs.ndir = lj_library_dir_list(rs.dir, NULL, rs.dsub, LJ_RS_ROWS - n);
    if (rs.ndir < 0) rs.ndir = 0;
    for (i = 0; i < rs.ndir; i++) snprintf(rs.drow[n + i], sizeof rs.drow[n + i], "%*s%s", 2 * n, "", rs.dsub[i]);
    for (i = 0; i < n + rs.ndir; i++) rs.ditems[i] = rs.drow[i];
    if (rs.dirs) { lj_w_list_set_items(rs.dirs, rs.ditems, n + rs.ndir); ((lj_w_list *)rs.dirs)->index = n - 1; lj_widget_invalidate(rs.dirs); }
}

int lj_restore_set_dir(const char *dir)
{
    if (!dir || !dir[0] || lj_str_copy(rs.dir, sizeof rs.dir, dir) != 0) return -1;
    fill_dirs();
    fill_files();
    return 0;
}

int lj_restore_dir_row(int row)
{
    char d[LJ_PATH_CAP];
    size_t end[LJ_RS_ROWS];
    if (row < 0 || row >= rs.nup + rs.ndir) return -1;
    if (row >= rs.nup) {
        if (snprintf(d, sizeof d, "%s%s%s", rs.dir, rs_join(rs.dir), rs.dsub[row - rs.nup]) >= (int)sizeof d) return -1;
        return lj_restore_set_dir(d);
    }
    rs_split(end);
    snprintf(d, sizeof d, "%.*s", (int)end[row], rs.dir);
    return lj_restore_set_dir(d);
}

static void on_dir(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev == LJ_WEV_CLICK || ev == LJ_WEV_DBLCLICK) lj_restore_dir_row(((lj_w_list *)w)->index);
}

/* on_file_change = lj_handler_noop: the stub FileList never fires; the code-built list does (on_pick) */
static void on_rs_restore(lj_widget *w, void *ud) { (void)w; (void)ud; lj_restore_run(); }
static void on_rs_close(lj_widget *w, void *ud) { (void)w; (void)ud; rs.close_pending = 1; }
static const lj_binding lj_rs_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FileListChange", lj_handler_noop, NULL }, { "bRestoreClick", on_rs_restore, NULL },
    { "bCloseClick", on_rs_close, NULL }, { NULL, NULL, NULL } };

lj_window *lj_restore_open(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_widget *fl, *w;
    if (rs.win) return rs.win;
    rs.win = lj_window_create(&lj_form_TLIBRARYRESTOREFORM, lj_rs_bindings);
    if (!rs.win) { lj_log("restore: cannot create TLibraryRestoreForm"); return NULL; }
    rs.close_pending = 0; rs.path[0] = 0; rs.notes[0] = 0; rs.files = NULL; rs.dirs = NULL; rb.on = 0;
    if ((w = lj_window_find(rs.win, "DriveList")) != NULL) w->visible = 0;          /* ME-17 stubs, see the header */
    if ((w = lj_window_find(rs.win, "DirList")) != NULL && w->parent) {      /* W4-DLG-1: the folder tree over DirList's rect */
        rs.dirs = lj_w_listbox_create(w->parent, NULL);
        rs.dirs->x = w->x; rs.dirs->y = w->y; rs.dirs->w = w->w; rs.dirs->h = w->h;
        rs.dirs->on_event = on_dir; rs.dirs->ud = NULL;
        w->visible = 0;
    }
    if ((fl = lj_window_find(rs.win, "FileList")) != NULL && fl->parent) {
        rs.files = lj_w_listbox_create(fl->parent, NULL);
        rs.files->x = fl->x; rs.files->y = fl->y; rs.files->w = fl->w; rs.files->h = fl->h;
        rs.files->on_event = on_pick; rs.files->ud = NULL;
        fl->visible = 0;
    }
    lj_window_set_text(rs.win, "lLibName", ""); lj_window_set_text(rs.win, "lLibPath", ""); lj_window_set_text(rs.win, "lBackupTime", ""); lj_window_set_text(rs.win, "lPath", "");
    lj_window_set_text(rs.win, "lRestore", c->lib ? c->lib->name : "");
    lj_restore_set_dir(c->userdir[0] ? c->userdir : ".");                     /* opens where Backup writes: <userdir> */
    show_notes();
    lj_win_show(rs.win, 0);
    return rs.win;
}

void lj_restore_close(void)
{
    if (!rs.win) return;
    lj_win_hide(rs.win);
    lj_window_destroy(rs.win);
    rs.win = NULL; rs.files = NULL; rs.dirs = NULL; rs.close_pending = 0;
}
lj_window *lj_restore_window(void) { return rs.win; }
void lj_restore_refresh(void) { if (rs.close_pending) lj_restore_close(); }
