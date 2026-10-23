/* lj_file_dialogs.c - see lj_file_dialogs.h. The FILES family: one table row per id (form, bindings, stay-on-top),
 * one init per id. Every OnXxx of the 20 DFMs is bound (ME-16); the ModalResult buttons (Kind = bkOK / bkCancel, no
 * OnClick in the DFM) get their handler in code, as in lj_user_libraries.c. A handler never destroys its own window:
 * it marks the id and lj_file_dlg_refresh closes it (the 100 ms hook), as the W2 windows do. */
#include "windows/lj_file_dialogs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_restore_data.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "gfx/lj_font.h"
#include "version.h"
#include "windows/lj_system_prefs.h"
#include "windows/lj_warnings.h"

#define LJ_FD_FIRST LJ_WIN_BACKUP_PROGRESS
#define LJ_FD_LAST  LJ_WIN_INIT_FORM
enum { LJ_FD_N = LJ_FD_LAST - LJ_FD_FIRST + 2, LJ_FD_WAIT = LJ_FD_N - 1, LJ_FD_OLD_MAX = 256 };   /* + LJ_WIN_PLEASE_WAIT in the last slot */

/* 12 Q9 / exe strings, verbatim (D13) */
#define LJ_FD_CORRUPT1 "Restore data file seems to be corrupt - do you wish to abort the data restore process ?"   /* exe 0x232564 */
#define LJ_FD_CORRUPT2 "(Aborting will delete the corrupt data file - restore will not be possible)"
#define LJ_FD_PASS_ASK "Are you sure you want to change the password?"                                           /* exe 0x157380 */
#define LJ_FD_MOMMA    "Does your momma kow you are doing this ?"                                                 /* exe 0x1577AC, 12 Q9 (password change) */
#define LJ_FD_BAD_PASS "Incorrect Password"                                                                       /* exe 0x157830 */
#define LJ_FD_DONT_UPD "Do NOT update files - are you absolutely sure ?"                                          /* exe 0x173114 */

static int fd_slot(enum lj_win id) { return id == LJ_WIN_PLEASE_WAIT ? LJ_FD_WAIT : (int)id >= (int)LJ_FD_FIRST && (int)id <= (int)LJ_FD_LAST ? (int)id - (int)LJ_FD_FIRST : -1; }

static struct {
    lj_window *win[LJ_FD_N];
    unsigned char close_pending[LJ_FD_N];
    int ticks[LJ_FD_N];                                      /* 100 ms refresh passes since open (TTimer stand-in) */
    char status[LJ_FILE_STATUS_LINES + 1][160]; const char *rows[LJ_FILE_STATUS_LINES + 1]; int nstatus, keep_open;
    char rel[LJ_FD_OLD_MAX][LJ_REL_CAP]; char kind[LJ_FD_OLD_MAX][16]; int nold;
    char rename_from[LJ_LIBNAME_CAP]; void (*renamed)(int ok, void *ud); void *renamed_ud;
    int started, unlocked;
    char wrap[LJ_FD_N][1024]; const char *wl[LJ_FD_N][24];
} fd;

static lj_window *W(enum lj_win id) { int s = fd_slot(id); return s < 0 ? NULL : fd.win[s]; }
static lj_widget *F(enum lj_win id, const char *n) { return W(id) ? lj_window_find(W(id), n) : NULL; }
static void close_later(enum lj_win id) { int s = fd_slot(id); if (s >= 0) fd.close_pending[s] = 1; }
static void show(lj_widget *w, int on) { if (w) { w->visible = (unsigned)(on != 0); lj_widget_invalidate(w); } }
static void enable(lj_widget *w, int on) { if (w) { w->enabled = (unsigned)(on != 0); lj_widget_invalidate(w); } }
static void set_lines(lj_widget *w, const char *const *lines, int n) { if (w) lj_w_memo_set_lines(w, lines, n); }

/* TRichEdit / TMemo WordWrap (lj_w_memo does not wrap): each line broken at the last blank that fits the text column
 * beside the scrollbar, as lj_fixture_config.c notes_show does; the pieces live in fd.wrap[slot] while the window is open */
static void set_wrapped(enum lj_win id, lj_widget *re, const char *const *lines, int n)
{
    int s = fd_slot(id), k = 0, i, maxw;
    const lj_font *f;
    char *out;
    size_t used = 0;
    if (s < 0 || !re) return;
    f = lj_w_font_of(re);
    maxw = re->w - 2 * 2 - 2 * 3 - LJ_SB_W;                   /* MM_BORDER / MM_PADX of lj_w_memo.c and the scrollbar */
    for (i = 0; i < n && k < LJ_COUNT_OF(fd.wl[0]); i++) {
        char *line = fd.wrap[s] + used, *q, *brk;
        if (lj_str_copy(line, sizeof fd.wrap[0] - used, lines[i]) != 0) break;
        used += strlen(line) + 1;
        for (out = line; k < LJ_COUNT_OF(fd.wl[0]); ) {
            brk = NULL;
            for (q = strchr(out, ' '); q; q = strchr(q + 1, ' ')) { *q = 0; if (lj_text_width(f, out) > maxw) { *q = ' '; break; } *q = ' '; brk = q; }
            fd.wl[s][k++] = out;
            if (!brk || lj_text_width(f, out) <= maxw) break;
            *brk = 0; out = brk + 1;
        }
    }
    lj_w_memo_set_lines(re, fd.wl[s], k);
}
static void help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }
static void on_button(lj_widget *b, void (*fn)(lj_widget *, int, void *), enum lj_win id) { if (b) { b->on_event = fn; b->ud = (void *)(intptr_t)id; } }

/* ---- Startup Status (LJ_WIN_PLEASE_WAIT, TStartupStatusWindow) ---- */

void lj_file_dlg_status_add(const char *line)
{
    if (!line) return;
    if (fd.nstatus == LJ_FILE_STATUS_LINES) { memmove(fd.status[0], fd.status[1], sizeof fd.status[0] * (LJ_FILE_STATUS_LINES - 1)); fd.nstatus--; }
    lj_str_copy(fd.status[fd.nstatus++], sizeof fd.status[0], line);
}

int lj_file_dlg_status_rows(const char *const **rows)
{
    int i;
    for (i = 0; i < fd.nstatus; i++) fd.rows[i] = fd.status[i];
    *rows = fd.rows;
    return fd.nstatus;
}

static void status_paint(void)
{
    const char *const *rows;
    int n = lj_file_dlg_status_rows(&rows), left = LJ_FILE_SELFDESTRUCT_S - fd.ticks[LJ_FD_WAIT] / 10;
    if (!fd.keep_open) {                                     /* the countdown is the last row (PROPOSAL rig: LJ2's place for it is unseen) */
        snprintf(fd.status[LJ_FILE_STATUS_LINES], sizeof fd.status[0], LJ_FILE_SELFDESTRUCT_FMT, left < 0 ? 0 : left);
        fd.rows[n++] = fd.status[LJ_FILE_STATUS_LINES];
    }
    set_lines(F(LJ_WIN_PLEASE_WAIT, "Re1"), rows, n);
}
static void on_status_down(lj_widget *w, void *ud) { (void)w; (void)ud; fd.keep_open = 1; status_paint(); }
static void on_status_close(lj_widget *w, void *ud) { (void)w; (void)ud; close_later(LJ_WIN_PLEASE_WAIT); }

/* ---- progress boxes ---- */

void lj_file_dlg_progress(enum lj_win id, const char *label, const char *text, int pos)
{
    static const char *const bars[] = { "Pg", "PG1", "pg1", "Pg1", "ProgressBar1" };
    lj_widget *b = NULL;
    int i;
    if (pos < 0) { lj_file_dlg_close(id); return; }
    if (!W(id) && !lj_file_dlg_open(id)) return;
    if (label && text) lj_window_set_text(W(id), label, text);
    for (i = 0; !b && i < LJ_COUNT_OF(bars); i++) b = F(id, bars[i]);
    if (b) lj_w_progressbar_set(b, pos);
    lj_window_paint(W(id));
}

/* ---- Restore Data / Creating Auto Restore Data / Enter New Password [06 s4.2] ---- */

static void restore_menu(void)   /* "the option is only available in the menu if a data set has been created" (CHM) */
{
    lj_window *m = lj_desktop_window();
    if (m && lj_window_menus(m)) lj_menu_set_visible(lj_window_menus(m), "M_RestoreProgramData", lj_restore_data_exists(lj_ctx_get()->userdir));
}

static void on_restore_ok(lj_widget *w, int ev, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK || !lj_restore_data_exists(c->userdir)) return;
    if (lj_restore_data_request(c->userdir, 1) != 0) { lj_win_message("Warning", "Error restoring file :"); return; }   /* exe 0x2326CC */
    close_later(LJ_WIN_RESTORE_DATA);
    lj_desktop_run_action(LJ_ACT_RESTART, 0);                /* "Restart & Restore": the next start erases and restores (lj_startup_library) */
}
static void on_cancel(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) close_later((enum lj_win)(intptr_t)ud); }   /* the bkCancel buttons: ud = the id */

static int create_restore_data(void)   /* Continue: <userdir>RestoreData.lj4d from the open library */
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    int r;
    if (!c->lib || lj_restore_data_path(c->userdir, path, sizeof path) != 0) return -1;
    lj_file_dlg_progress(LJ_WIN_COMPRESS_RESTORE, NULL, NULL, 0);
    r = lj_restore_data_create(c->lib, path);
    lj_file_dlg_progress(LJ_WIN_COMPRESS_RESTORE, NULL, NULL, 100);
    lj_file_dlg_close(LJ_WIN_COMPRESS_RESTORE);
    if (r != 0) lj_win_message("Warning", "Error Writing Backup File - Backup file not saved");   /* exe 0x14FE74 */
    restore_menu();
    lj_system_prefs_restore_state();
    return r;
}
static void on_create_continue(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) { close_later(LJ_WIN_CREATE_DATA); create_restore_data(); } }

static void on_pass_change(lj_widget *w, void *ud)
{
    const char *a = lj_w_edit_text(F(LJ_WIN_NEW_PASSWORD, "Edit1")), *b = lj_w_edit_text(F(LJ_WIN_NEW_PASSWORD, "Edit2"));
    (void)w; (void)ud;
    enable(F(LJ_WIN_NEW_PASSWORD, "bOk"), a && b && a[0] && strcmp(a, b) == 0);   /* bOk Enabled = False in the DFM: "the exact same password twice" (CHM) */
}
static void on_pass_ok(lj_widget *w, int ev, void *ud)
{
    const char *a = lj_w_edit_text(F(LJ_WIN_NEW_PASSWORD, "Edit1")), *b = lj_w_edit_text(F(LJ_WIN_NEW_PASSWORD, "Edit2"));
    (void)ud;
    if (ev != LJ_WEV_CLICK || !w->enabled || !a || !b || strcmp(a, b) != 0) return;
    if (lj_restore_password_set(lj_ctx_get()->userdir, a) == 0) { fd.unlocked = 1; close_later(LJ_WIN_NEW_PASSWORD); }
    lj_system_prefs_restore_state();
}

static void on_new_password_yes(int yes, void *ud) { (void)ud; if (yes) lj_file_dlg_open(LJ_WIN_NEW_PASSWORD); }
void lj_file_dlg_new_password(void)
{
    if (lj_restore_password_isset(lj_ctx_get()->userdir)) lj_win_confirm("Please Confirm..", LJ_FD_PASS_ASK, LJ_FD_MOMMA, on_new_password_yes, NULL);   /* PROPOSAL rig: the two lines' pairing */
    else lj_file_dlg_open(LJ_WIN_NEW_PASSWORD);
}

int lj_file_dlg_unlock(const char *pw)
{
    if (!lj_restore_password_check(lj_ctx_get()->userdir, pw)) { lj_win_message("Warning", LJ_FD_BAD_PASS); return -1; }
    fd.unlocked = 1;
    return 0;
}
int lj_file_dlg_unlocked(void) { return fd.unlocked; }

/* ---- User Libraries: Rename File, Copying Files [01 s8] ---- */

static void on_rename_ok(lj_widget *w, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const char *to = lj_w_edit_text(F(LJ_WIN_RENAME_FILE, "eFileName"));
    int ok;
    (void)w; (void)ud;
    if (!to || !to[0]) return;
    ok = strcmp(to, fd.rename_from) == 0 || lj_library_rename(c->userdir, fd.rename_from, to) == 0;
    if (!ok) { lj_win_message("Warning", "Unable to create destination path"); return; }   /* exe 0x14CB70 */
    close_later(LJ_WIN_RENAME_FILE);
    if (fd.renamed) fd.renamed(1, fd.renamed_ud);
}
lj_window *lj_file_dlg_rename(const char *from, void (*done)(int ok, void *ud), void *ud)
{
    lj_window *w = lj_file_dlg_open(LJ_WIN_RENAME_FILE);
    if (!w) return NULL;
    lj_str_copy(fd.rename_from, sizeof fd.rename_from, from ? from : "");
    fd.renamed = done; fd.renamed_ud = ud;
    lj_w_edit_set_text(F(LJ_WIN_RENAME_FILE, "eFileName"), fd.rename_from);
    return w;
}

/* ---- format-update dialogs (F56, W4-DLG-4): an LJ4 record below LJ_REC_FORMAT, irreversible ---- */

static int is_kind(const char *kind, const char *k) { return k ? strcmp(kind, k) == 0 : strcmp(kind, "cl") && strcmp(kind, "bgc") && strcmp(kind, "patch"); }
static int old_count(const char *k) { int i, n = 0; for (i = 0; i < fd.nold; i++) n += is_kind(fd.kind[i], k); return n; }

static int update_kind(const char *k)   /* k NULL = every kind the three special dialogs do not cover */
{
    lj_ctx *c = lj_ctx_get();
    int i, n = 0, total = old_count(k), done = 0;
    if (!c->lib) return -1;
    for (i = 0; i < fd.nold; i++) {
        if (!is_kind(fd.kind[i], k) || !fd.rel[i][0]) continue;
        lj_file_dlg_progress(LJ_WIN_UPDATE_FILE, "Label2", fd.rel[i], 0);   /* "Updating [file]" */
        if (lj_library_update_file(c->lib, fd.rel[i]) == 0) { n++; fd.rel[i][0] = 0; }
        done++;
        if (W(LJ_WIN_UPDATE_BGCUES) && k && strcmp(k, "bgc") == 0) lj_w_progressbar_set(F(LJ_WIN_UPDATE_BGCUES, "PG1"), total ? done * 100 / total : 100);
    }
    lj_file_dlg_close(LJ_WIN_UPDATE_FILE);
    lj_log("update: %d %s file(s) rewritten as format %d", n, k ? k : "LJ4", LJ_REC_FORMAT);
    return n;
}

static void on_update_ok(lj_widget *w, int ev, void *ud)    /* TUpdateForm OK: Backup Datafiles first, then the rewrite */
{
    lj_ctx *c = lj_ctx_get();
    char bak[LJ_PATH_CAP];
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK || !c->lib) return;
    if (lj_w_checkbox_checked(F(LJ_WIN_UPDATE, "cbBackup"))
        && (snprintf(bak, sizeof bak, "%s%s (before update)" LJ_LIBRARY_BACKUP_EXT, c->userdir, c->lib->name) >= (int)sizeof bak || lj_library_backup(c->lib, bak, "Backup Datafiles first") != 0)) {
        lj_win_message("Warning", "Error Writing Backup File - Backup file not saved");   /* nothing rewritten without the backup asked for */
        return;
    }
    update_kind(NULL);
    close_later(LJ_WIN_UPDATE);
}
static void on_dont_update_yes(int yes, void *ud) { (void)ud; if (yes) close_later(LJ_WIN_UPDATE); }
static void on_update_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_win_confirm("Please Confirm..", LJ_FD_DONT_UPD, NULL, on_dont_update_yes, NULL); }

static void on_cl2_update(lj_widget *w, void *ud) { (void)w; (void)ud; update_kind("cl"); close_later(LJ_WIN_UPDATE_CL2); }
static void on_cl2_later(lj_widget *w, void *ud) { (void)w; (void)ud; close_later(LJ_WIN_UPDATE_CL2); }
static void on_cl2_never(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ini_seti(lj_ctx_get()->main_ini, "UPDATE", "NoCl2Reminder", 1); close_later(LJ_WIN_UPDATE_CL2); }   /* ours: Main.Ini [UPDATE] NoCl2Reminder=1 */

static void on_bg_continue(lj_widget *w, int ev, void *ud)
{
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    show(F(LJ_WIN_UPDATE_BGCUES, "Panel1"), 1);              /* Updating BGCues + PG1, Visible = False in the DFM */
    update_kind("bgc");
    close_later(LJ_WIN_UPDATE_BGCUES);
}
static void on_patch_ok(lj_widget *w, void *ud) { (void)w; (void)ud; update_kind("patch"); close_later(LJ_WIN_PATCH_UPDATE); }

static void on_corrupt(int yes, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    (void)ud;
    if (!yes) return;                                        /* No: RESTORE=1 stays, the next start tries again */
    if (lj_restore_data_path(c->userdir, path, sizeof path) == 0) remove(path);   /* "aborting the operation will delete the restore data file" (CHM) */
    lj_restore_data_request(c->userdir, 0);
    restore_menu();
}

int lj_file_dlg_startup(void)
{
    lj_ctx *c = lj_ctx_get();
    int n = 0;
    restore_menu();
    if (lj_restore_data_requested(c->userdir) && lj_restore_data_exists(c->userdir)) {   /* lj_startup_library kept the flag: the archive failed its check */
        lj_win_confirm("Error", LJ_FD_CORRUPT1, LJ_FD_CORRUPT2, on_corrupt, NULL);
        n++;
    }
    if (!c->lib) return n;
    fd.nold = lj_library_outdated(c->lib, fd.rel, fd.kind, LJ_FD_OLD_MAX);
    if (fd.nold < 0) fd.nold = 0;
    if (old_count("cl") && !lj_ini_geti(c->main_ini, "UPDATE", "NoCl2Reminder", 0) && lj_file_dlg_open(LJ_WIN_UPDATE_CL2)) n++;
    if (old_count("bgc") && lj_file_dlg_open(LJ_WIN_UPDATE_BGCUES)) n++;
    if (old_count("patch") && lj_file_dlg_open(LJ_WIN_PATCH_UPDATE)) n++;
    if (old_count(NULL) && lj_file_dlg_open(LJ_WIN_UPDATE)) n++;
    return n;
}

/* ---- the table ---- */

static const lj_binding b_none[] = { { NULL, NULL, NULL } };
static const lj_binding b_backup[] = { { "FormCreate", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_create[] = { { "Help1Click", help, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_init[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL },
    { "Image1Click", lj_handler_noop, NULL }, { "Panel1Click", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "Timer2Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_pass[] = { { "EditChange", on_pass_change, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_rename[] = { { "bHelpClick", help, NULL }, { "bOkClick", on_rename_ok, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_restore[] = { { "FormCreate", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_status[] = { { "FormClose", on_status_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "Re1MouseDown", on_status_down, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_cl2[] = { { "bCancelClick", on_cl2_never, NULL }, { "bHelpClick", help, NULL }, { "bRemindMeClick", on_cl2_later, NULL }, { "bUpdateClick", on_cl2_update, NULL }, { "FormCreate", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_upfile[] = { { "FormCreate", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_update[] = { { "bHelpClick", help, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_upprog[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding b_patch[] = { { "BitBtn3Click", help, NULL }, { "bOkClick", on_patch_ok, NULL }, { NULL, NULL, NULL } };

typedef struct { enum lj_win id; const lj_form_desc *form; const lj_binding *b; unsigned on_top; } lj_fd_row;
static const lj_fd_row lj_fd_rows[] = {   /* on_top = FormStyle fsStayOnTop in the DFM (2u = LJ_WF_STAY_ON_TOP of platform/lj_platform.h) */
    { LJ_WIN_BACKUP_PROGRESS, &lj_form_TBACKUPFORM, b_backup, 2u },            { LJ_WIN_RESTORE_LIB_PROGRESS, &lj_form_TRESTORELIBPROGRESSFORM, b_none, 2u },
    { LJ_WIN_COPYING_FILES, &lj_form_TCOPYINGFILESFORM, b_none, 2u },          { LJ_WIN_RENAME_FILE, &lj_form_TRENAMEFILEFORM, b_rename, 0 },
    { LJ_WIN_PROGRESS, &lj_form_TPROGRESSFORM, b_none, 2u },                   { LJ_WIN_PRINT_WAIT, &lj_form_TPRINTWAITFORM, b_none, 2u },
    { LJ_WIN_RESTORE_DATA, &lj_form_TRESTOREDATAFORM, b_restore, 0 },          { LJ_WIN_CREATE_DATA, &lj_form_TCREATEDATAFORM, b_create, 0 },
    { LJ_WIN_COMPRESS_RESTORE, &lj_form_TCOMPRESSRESTOREDATAFORM, b_none, 2u }, { LJ_WIN_RESTORE_PROGRESS, &lj_form_TRESTOREPROGRESSFORM, b_none, 2u },
    { LJ_WIN_NEW_PASSWORD, &lj_form_TNEWPASSWORDFORM, b_pass, 0 },             { LJ_WIN_UPDATE, &lj_form_TUPDATEFORM, b_update, 0 },
    { LJ_WIN_UPDATE_FILE, &lj_form_TUPDATEFILEFORM, b_upfile, 0 },             { LJ_WIN_UPDATE_PROGRAM, &lj_form_TUPDATEPROGRAMFORM, b_upprog, 2u },
    { LJ_WIN_UPDATE_BGCUES, &lj_form_TUPDATEBGCUESFORM, b_none, 2u },          { LJ_WIN_UPDATE_CL2, &lj_form_TUPDATECL2FORM, b_cl2, 0 },
    { LJ_WIN_PATCH_UPDATE, &lj_form_TPATCHUPDATEFORM, b_patch, 0 },            { LJ_WIN_REBUILD_SETUP, &lj_form_TREBUILDSETUPFORM, b_none, 0 },
    { LJ_WIN_INIT_FORM, &lj_form_TINITFORM1, b_init, 2u },                     { LJ_WIN_PLEASE_WAIT, &lj_form_TSTARTUPSTATUSWINDOW, b_status, 2u },
};

static int count_records(enum lj_rec_kind k)   /* ponytail: counts up to 65536 records per kind; a bigger library shows 65536 */
{
    static uint32_t nums[65536];
    lj_ctx *c = lj_ctx_get();
    int n = c->lib ? lj_library_list_records(c->lib, k, nums, LJ_COUNT_OF(nums)) : 0;
    return n < 0 ? 0 : n;
}

static void init_window(enum lj_win id)
{
    lj_ctx *c = lj_ctx_get();
    char t[64];
    switch (id) {
    case LJ_WIN_RESTORE_DATA: {
        static const char *const l[] = { "Restore Program Data - Warning", "",   /* exe 0x1EE328 .. 0x1EE3F4 */
            "Restoring data will completely restore the users library from where the data was originally compiled - overwriting any existing contents of the library",
            "Restoring data requires a restart to initialize with the restored data." };
        set_wrapped(id, F(id, "Re1"), l, LJ_COUNT_OF(l));
        on_button(F(id, "BitBtn1"), on_restore_ok, id); on_button(F(id, "BitBtn2"), on_cancel, id);
        enable(F(id, "BitBtn1"), lj_restore_data_exists(c->userdir));
        break; }
    case LJ_WIN_CREATE_DATA: {
        static const char *const l[] = { "Creating Restore Data - Warning", "",   /* exe 0x156BC0 .. 0x156C9C */
            "Creating auto restore data will completely overwrite any previously generated restore data.",
            "The restore data will be generated from the current users library",
            "When restored, the data will be extracted to the same library, completely overwriting any contents of that library" };
        set_wrapped(id, F(id, "Re1"), l, LJ_COUNT_OF(l));
        on_button(F(id, "BitBtn1"), on_create_continue, id); on_button(F(id, "BitBtn2"), on_cancel, id);
        break; }
    case LJ_WIN_NEW_PASSWORD: on_button(F(id, "bOk"), on_pass_ok, id); on_button(F(id, "BitBtn2"), on_cancel, id); break;
    case LJ_WIN_RENAME_FILE: on_button(F(id, "BitBtn2"), on_cancel, id); break;
    case LJ_WIN_UPDATE: {
        static const char *const l[] = { "Updating the file format will make the files incompatible with previous versions of LightJockey !" };   /* exe 0x3607A8 */
        set_wrapped(id, F(id, "Memo1"), l, 1);
        on_button(F(id, "BitBtn1"), on_update_ok, id); on_button(F(id, "BitBtn3"), on_update_cancel, id);
        break; }
    case LJ_WIN_UPDATE_BGCUES: {
        static const char *const l[] = { "Background cue files format changed.", "Existing Background Cue files must be updated before LightJockey can continue." };   /* exe 0x360720, 0x360750 */
        set_wrapped(id, F(id, "Memo1"), l, 2);
        on_button(F(id, "BitBtn1"), on_bg_continue, id); on_button(F(id, "BitBtn2"), on_cancel, id);
        break; }
    case LJ_WIN_PATCH_UPDATE: {                                /* the DFM's nine Lines, wrapped as the TMemo shows them */
        lj_w_memo *m = (lj_w_memo *)F(id, "Memo1");
        if (m) set_wrapped(id, &m->base, m->lines, m->n);
        on_button(F(id, "BitBtn2"), on_cancel, id);           /* bOk has its bOkClick */
        break; }
    case LJ_WIN_PROGRESS: lj_window_set_caption(W(id), "Deleting Files. Please Wait.."); break;   /* exe 0x153AAC: the one ProgressForm use we have (Delete Library) */
    /* the progress boxes keep their DFM placeholders (Label3, '[]', lAction, lFile) until lj_file_dlg_progress fills them (W4-CAP) */
    case LJ_WIN_INIT_FORM:
        snprintf(t, sizeof t, "Version %s", LJ_VERSION_STR);   /* the lVersion pair's "Version 2.0 -Build 1" slot (design 9.4) */
        lj_window_set_text(W(id), "lVersion1", t); lj_window_set_text(W(id), "lVersion2", t);
        snprintf(t, sizeof t, "%d", count_records(LJ_REC_SEQ));   lj_window_set_text(W(id), "LSeq", t);
        snprintf(t, sizeof t, "%d", count_records(LJ_REC_CUE));   lj_window_set_text(W(id), "LCues", t);
        snprintf(t, sizeof t, "%d", count_records(LJ_REC_MACRO)); lj_window_set_text(W(id), "LMacro", t);
        lj_window_set_text(W(id), "LPreset", "....");
        break;
    case LJ_WIN_PLEASE_WAIT: fd.keep_open = 0; status_paint(); break;
    default: break;
    }
}

lj_window *lj_file_dlg_open(enum lj_win id)
{
    int s = fd_slot(id), i;
    const lj_fd_row *r = NULL;
    for (i = 0; i < LJ_COUNT_OF(lj_fd_rows); i++) if (lj_fd_rows[i].id == id) r = &lj_fd_rows[i];
    if (s < 0 || !r) return NULL;
    if (fd.win[s]) return fd.win[s];
    fd.win[s] = lj_window_create(r->form, r->b);
    if (!fd.win[s]) { lj_log("files: cannot create %s", r->form->class_name); return NULL; }
    fd.close_pending[s] = 0; fd.ticks[s] = 0;
    init_window(id);
    lj_win_show(fd.win[s], r->on_top);
    return fd.win[s];
}

void lj_file_dlg_close(enum lj_win id)
{
    int s = fd_slot(id);
    if (s < 0 || !fd.win[s]) return;
    lj_win_hide(fd.win[s]);
    lj_window_destroy(fd.win[s]);
    fd.win[s] = NULL; fd.close_pending[s] = 0;
}

lj_window *lj_file_dlg_get(enum lj_win id) { return W(id); }

void lj_file_dlg_refresh(enum lj_win id)
{
    int s = fd_slot(id);
    if (s < 0 || !fd.win[s]) return;
    if (fd.close_pending[s]) { lj_file_dlg_close(id); return; }
    fd.ticks[s]++;
    if (id == LJ_WIN_PLEASE_WAIT && !fd.keep_open) {         /* Timer1 (1 s): the countdown, then the window closes itself */
        if (fd.ticks[s] % 10 == 0) status_paint();
        if (fd.ticks[s] >= LJ_FILE_SELFDESTRUCT_S * 10) lj_file_dlg_close(id);
    }
    if (id == LJ_WIN_INIT_FORM && fd.ticks[s] >= 20) lj_file_dlg_close(id);   /* Timer1 Interval = 2000 */
}

void lj_file_dlg_bind_fns(void)
{
    if (fd.started || !lj_win_host_get()) return;            /* headless (tests) never runs the start checks here */
    fd.started = 1;
    lj_file_dlg_startup();
}
