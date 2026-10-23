/* lj_user_libraries.c - see lj_user_libraries.h. Every OnXxx of TUSERLIBFORM and TGETUSERLIBNAMEFORM
 * is bound (ME-16); bClose, bOk and BitBtn2 carry no handler in the DFMs (ModalResult buttons), so
 * their widgets get one in code. */
#include "windows/lj_user_libraries.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_file_dialogs.h"
#include "windows/lj_warnings.h"

enum { LJ_UL_MAX = 64 };   /* ponytail: libraries listed; raise when someone keeps more */

static struct {
    lj_window *win, *name_win; int close_pending, name_close_pending;
    int n, row; char names[LJ_UL_MAX][LJ_LIBNAME_CAP]; char line[LJ_UL_MAX][LJ_PATH_CAP + LJ_LIBNAME_CAP + 8]; const char *items[LJ_UL_MAX];
} ul;

static lj_widget *find(const char *n) { return lj_window_find(ul.win, n); }
static const char *current(void) { lj_ctx *c = lj_ctx_get(); return c->lib ? c->lib->name : "Default"; }
static int is_locked(int row) { return row < 0 || row >= ul.n || strcmp(ul.names[row], "Default") == 0 || strcmp(ul.names[row], current()) == 0; }

static void enable(const char *n, int on) { lj_widget *w = find(n); if (w) { w->enabled = (unsigned)(on != 0); lj_widget_invalidate(w); } }
static void gates(void)   /* bSelect bDelete bRename bbackup are Enabled = False in the DFM until a row is picked */
{
    enable("bDelete", !is_locked(ul.row)); enable("bRename", !is_locked(ul.row));
    enable("bSelect", ul.row >= 0 && strcmp(ul.names[ul.row], current()) != 0);
    enable("bbackup", ul.row >= 0);
}
const char *lj_user_libraries_selected(void) { return ul.win && ul.row >= 0 && ul.row < ul.n ? ul.names[ul.row] : NULL; }

static void rebuild(void)
{
    lj_ctx *c = lj_ctx_get();
    char label[LJ_LIBNAME_CAP + 4];
    int i;
    ul.n = lj_library_list(c->userdir, ul.names, LJ_UL_MAX);
    if (ul.n < 0) ul.n = 0;
    for (i = 0; i < ul.n; i++) {
        lj_library lib;
        if (lj_library_open(c->userdir, ul.names[i], &lib) != 0) lib.root[0] = 0;
        snprintf(ul.line[i], sizeof ul.line[i], "%s\t%s\t", ul.names[i], lib.root);   /* ponytail: Date blank, no folder time without an OS call in windows/ */
        ul.items[i] = ul.line[i];
    }
    ul.row = -1;
    if (find("LV1")) lj_w_listview_set_items(find("LV1"), ul.items, ul.n);
    snprintf(label, sizeof label, "\"%s\"", current());
    lj_w_set_caption(find("Label2"), label);
    gates();
}

int lj_user_libraries_count(void) { return ul.n; }
const char *lj_user_libraries_name(int row) { return row >= 0 && row < ul.n ? ul.names[row] : NULL; }
int lj_user_libraries_select(int row)
{
    lj_widget *lv = find("LV1");
    if (row < 0 || row >= ul.n) return -1;
    ul.row = row;
    if (lv) { ((lj_w_listview *)lv)->index = row; lj_widget_invalidate(lv); }
    gates();
    return 0;
}

/* ---- the buttons ---- */

static void on_restart(int yes, void *ud) { (void)ud; if (yes && ul.row >= 0) lj_desktop_run_action(LJ_ACT_SELECT_LIBRARY, ul.row); }
static void on_select(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (ul.row < 0 || strcmp(ul.names[ul.row], current()) == 0) return;
    lj_win_confirm("Please Confirm..", LJ_USERLIB_RESTART1, LJ_USERLIB_RESTART2, on_restart, NULL);
}
static void on_delete_yes(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    if (!yes || is_locked(ul.row)) return;
    lj_file_dlg_progress(LJ_WIN_PROGRESS, NULL, NULL, 0);                                         /* "Deleting Files. Please Wait.." (exe 0x153AAC) */
    if (lj_library_delete(c->userdir, ul.names[ul.row]) != 0) lj_win_message("Warning", "Error erasing file ");   /* exe 0x232684 */
    lj_file_dlg_progress(LJ_WIN_PROGRESS, NULL, NULL, -1);
    rebuild();
}
static void on_delete(lj_widget *w, void *ud)
{
    char l1[LJ_LIBNAME_CAP + 32];
    (void)w; (void)ud;
    if (is_locked(ul.row)) return;
    snprintf(l1, sizeof l1, "Delete User Library \"%s\" ?", ul.names[ul.row]);                       /* exe 0x153A2C + 0x153A4C */
    lj_win_confirm("Please Confirm..", l1, "(This will delete all information contained in the library)", on_delete_yes, NULL);   /* exe 0x153A64 */
}
static void on_create(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_user_lib_name_open()) lj_w_edit_set_text(lj_window_find(ul.name_win, "eName"), ""); }
static void on_renamed(int ok, void *ud) { (void)ud; if (ok) rebuild(); }
static void on_rename(lj_widget *w, void *ud) { (void)w; (void)ud; if (!is_locked(ul.row)) lj_file_dlg_rename(ul.names[ul.row], on_renamed, NULL); }   /* W4: TRenameFileForm "Rename File" (MaxLength 28) */
static void on_backup(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_BACKUP); }
static void on_restore(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_RESTORE); }
static void on_change(lj_widget *w, void *ud) { (void)ud; lj_user_libraries_select(lj_w_listview_index(w)); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; ul.close_pending = 1; }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("user libraries: Help is W4"); }
static void close_event(lj_widget *w, int ev, void *ud) { if (ev == LJ_WEV_CLICK) on_close(w, ud); }

static const lj_binding lj_ul_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "LV1Change", on_change, NULL },
    { "bSelectClick", on_select, NULL }, { "bCreateClick", on_create, NULL }, { "bDeleteClick", on_delete, NULL }, { "bRenameClick", on_rename, NULL },
    { "bbackupClick", on_backup, NULL }, { "bRestoreClick", on_restore, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };

lj_window *lj_user_libraries_open(void)
{
    lj_widget *b;
    if (ul.win) return ul.win;
    ul.win = lj_window_create(&lj_form_TUSERLIBFORM, lj_ul_bindings);
    if (!ul.win) { lj_log("user libraries: cannot create TUserLibForm"); return NULL; }
    if ((b = find("bClose")) != NULL) { b->on_event = close_event; b->ud = NULL; }
    ul.close_pending = 0;
    rebuild();
    lj_win_show(ul.win, 0);
    return ul.win;
}

void lj_user_libraries_close(void)
{
    lj_user_lib_name_close();
    if (!ul.win) return;
    lj_win_hide(ul.win);
    lj_window_destroy(ul.win);
    ul.win = NULL; ul.close_pending = 0;
}

lj_window *lj_user_libraries_window(void) { return ul.win; }
void lj_user_libraries_refresh(void)
{
    if (ul.name_close_pending) lj_user_lib_name_close();
    if (ul.close_pending) lj_user_libraries_close();
}

/* ---- the name box ---- */

static void name_ok(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_library lib;
    const char *name = lj_w_edit_text(lj_window_find(ul.name_win, "eName"));
    int i, copy = lj_w_checkbox_checked(lj_window_find(ul.name_win, "cbCopyFiles")), rc;
    if (!name || !name[0]) return;
    for (i = 0; i < ul.n; i++) if (strcmp(ul.names[i], name) == 0) { lj_win_message("Warning", "File already exists"); return; }   /* exe 0x14D4F0, PROPOSAL use here */
    if (copy) {
        lj_file_dlg_progress(LJ_WIN_COPYING_FILES, NULL, NULL, 0);                                /* "Copying Files - please wait.." */
        rc = lj_library_copy(c->userdir, current(), name);
        lj_file_dlg_progress(LJ_WIN_COPYING_FILES, NULL, NULL, -1);
    }
    else rc = lj_library_create(c->userdir, name, &lib);
    if (rc != 0) { lj_win_message("Warning", "Unable to create destination path"); return; }   /* exe 0x14CB70 */
    ul.name_close_pending = 1;
    rebuild();
}
static void name_event(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (strcmp(w->name, "bOk") == 0) name_ok(); else ul.name_close_pending = 1;
}
static void on_name_change(lj_widget *w, void *ud)
{
    lj_widget *ok = ul.name_win ? lj_window_find(ul.name_win, "bOk") : NULL;
    const char *t = lj_w_edit_text(w);
    (void)ud;
    if (ok) { ok->enabled = t && t[0]; lj_widget_invalidate(ok); }
}
static const lj_binding lj_ln_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "eNameChange", on_name_change, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_user_lib_name_open(void)
{
    lj_widget *b;
    if (ul.name_win) return ul.name_win;
    ul.name_win = lj_window_create(&lj_form_TGETUSERLIBNAMEFORM, lj_ln_bindings);
    if (!ul.name_win) { lj_log("user libraries: cannot create TGetUserLibNameForm"); return NULL; }
    if ((b = lj_window_find(ul.name_win, "bOk")) != NULL) { b->on_event = name_event; b->ud = NULL; b->enabled = 0; }
    if ((b = lj_window_find(ul.name_win, "BitBtn2")) != NULL) { b->on_event = name_event; b->ud = NULL; }
    ul.name_close_pending = 0;
    lj_win_show(ul.name_win, 0);
    return ul.name_win;
}

void lj_user_lib_name_close(void)
{
    if (!ul.name_win) return;
    lj_win_hide(ul.name_win);
    lj_window_destroy(ul.name_win);
    ul.name_win = NULL; ul.name_close_pending = 0;
}

lj_window *lj_user_lib_name_window(void) { return ul.name_win; }
