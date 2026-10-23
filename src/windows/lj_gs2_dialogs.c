/* lj_gs2_dialogs.c - see lj_gs2_dialogs.h (W3 integrator: the gs2 track left the bodies open). TFixtureImportForm,
 * TExportFixtureForm and TExportAllForm; every DFM handler bound (ME-16). There is no native file dialog in W3, so the
 * folders are fixed inside the user library: <library>\Import\ is what Import lists, <library>\Export\ is where both
 * exports write (PROPOSAL folders, docs/fidelity.md). The sub-dialogs (TProfileImportModifiedFilesForm "File Exists",
 * TFixtureExportWarningDialog, TProfileNameOldStyleForm) are not opened: Import (check modified) keeps an existing
 * profile instead of asking (ponytail: add the File Exists box when a user needs the per-file choice). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include "windows/lj_gs2_dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile.h"
#include "model/lj_library.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_windows.h"

#define GS2_IMPORT_DIR "Import"   /* PROPOSAL folder: LJ2 asks with a TOpenDialog */
#define GS2_EXPORT_DIR "Export"   /* PROPOSAL folder: LJ2 asks with a TSaveDialog / folder box */
#define GS2_ROWS 256

static lj_show *show(void) { return lj_ctx_get()->show; }
static int ends_with(const char *s, const char *ext)   /* ASCII case fold */
{
    size_t a = strlen(s), b = strlen(ext), i;
    if (a <= b) return 0;
    for (i = 0; i < b; i++) if ((s[a - b + i] | 0x20) != (ext[i] | 0x20)) return 0;
    return 1;
}
static void cancel_ev(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) *(int *)ud = 1; }   /* bkCancel has no OnClick */
static void on_cancel(lj_window *win, const char *name, int *flag) { lj_widget *w = win ? lj_window_find(win, name) : NULL; if (w) { w->on_event = cancel_ev; w->ud = flag; } }

/* ---- Import ---- */

static struct { lj_window *win; int close_pending, n; char names[GS2_ROWS][64], rows[GS2_ROWS][160], dir[LJ_PATH_CAP]; const char *rowp[GS2_ROWS]; } im;

static int user_exists(const char *file)   /* UserFixtureDef\<base>.GS2 is on disk */
{
    char rel[96], path[LJ_PATH_CAP], base[64];
    FILE *f;
    const char *dot = strrchr(file, '.');
    snprintf(base, sizeof base, "%.*s", (int)(dot ? (size_t)(dot - file) : strlen(file)), file);
    if (snprintf(rel, sizeof rel, "UserFixtureDef/%s.GS2", base) >= (int)sizeof rel || lj_library_path(lj_ctx_get()->lib, rel, path, sizeof path) != 0 || !(f = fopen(path, "rb"))) return 0;
    fclose(f);
    return 1;
}

static void im_list(void)
{
    static const char *const ext[] = { ".Udf", ".Udo", ".GS2" };
    int i, k;
    im.n = 0;
    for (k = 0; lj_ctx_get()->lib && k < LJ_COUNT_OF(ext); k++) {
        int got = lj_library_list_files(lj_ctx_get()->lib, GS2_IMPORT_DIR, ext[k], im.names + im.n, GS2_ROWS - im.n);
        if (got > 0) im.n += got;
    }
    for (i = 0; i < im.n; i++) {
        snprintf(im.rows[i], sizeof im.rows[i], "%s\t%s\t%s", im.names[i], ends_with(im.names[i], ".GS2") ? "Profile (.GS2)" : "Export file (.Udf)",
                 user_exists(im.names[i]) ? "Exists" : "New");   /* PROPOSAL rig: the Type / Status texts */
        im.rowp[i] = im.rows[i];
    }
    if (im.win) { lj_w_listview_set_items(lj_window_find(im.win, "lv1"), im.rowp, im.n); lj_window_set_text(im.win, "lFileName", im.dir); }
}

int lj_gs2_import_file(const char *path, char *err, size_t cap)
{
    lj_profile p;
    char rel[96], dst[LJ_PATH_CAP];
    int rc;
    if (!lj_ctx_get()->lib) { snprintf(err, cap, "no library"); return -1; }
    memset(&p, 0, sizeof p);
    rc = ends_with(path, ".GS2") ? lj_gs2_load(path, &p, err, cap) : lj_udf_import(path, &p, err, cap);
    if (rc == 0 && (snprintf(rel, sizeof rel, "UserFixtureDef/%s", p.gs2_file) >= (int)sizeof rel || lj_library_path(lj_ctx_get()->lib, rel, dst, sizeof dst) != 0 || lj_gs2_save(dst, &p) != 0)) { snprintf(err, cap, "cannot write %s", rel); rc = -1; }
    lj_gs2_free(&p);
    return rc;
}

int lj_gs2_import_folder(int keep_existing)
{
    char rel[96], path[LJ_PATH_CAP], err[160];
    int i, done = 0;
    im_list();
    for (i = 0; i < im.n; i++) {
        if (keep_existing && user_exists(im.names[i])) continue;
        if (snprintf(rel, sizeof rel, "%s/%s", GS2_IMPORT_DIR, im.names[i]) >= (int)sizeof rel || lj_library_path(lj_ctx_get()->lib, rel, path, sizeof path) != 0) continue;
        if (lj_gs2_import_file(path, err, sizeof err) != 0) { lj_log("import profile: %s: %s", im.names[i], err); continue; }
        done++;
    }
    if (done) lj_desktop_run_action(LJ_ACT_GS2_RELOAD, 0);
    im_list();
    return done;
}

static void im_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_gs2_import_folder(0); im.close_pending = 1; }
static void im_sel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_gs2_import_folder(1); im.close_pending = 1; }
static void im_ov(lj_widget *w, void *ud) { (void)ud; if (w) { w->enabled = 0; w->hint = "Not available in LJ4EVR"; } }   /* Offline Visualizer files: design 8.5 */
static const lj_binding im_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bImportAllClick", im_all, NULL },
    { "bImportSelClick", im_sel, NULL }, { "cbImportOVFilesClick", im_ov, NULL }, { "Help1Click", lj_handler_noop, NULL }, { NULL, NULL, NULL } };

lj_window *lj_gs2_import_open(void)
{
    lj_widget *w;
    if (im.win) return im.win;
    if (!(im.win = lj_window_create(&lj_form_TFIXTUREIMPORTFORM, im_b))) { lj_log("import profile: cannot create TFixtureImportForm"); return NULL; }
    im.close_pending = 0;
    if (!lj_ctx_get()->lib || lj_library_path(lj_ctx_get()->lib, GS2_IMPORT_DIR, im.dir, sizeof im.dir) != 0) im.dir[0] = 0;
    else lj_fs_mkdir(im.dir);
    if ((w = lj_window_find(im.win, "cbImportOVFiles")) != NULL) { w->enabled = 0; w->hint = "Not available in LJ4EVR"; }
    on_cancel(im.win, "bCancel", &im.close_pending);
    im_list();
    lj_win_show(im.win, 0);
    return im.win;
}
void lj_gs2_import_close(void) { if (!im.win) return; lj_win_hide(im.win); lj_window_destroy(im.win); im.win = NULL; im.close_pending = 0; }
lj_window *lj_gs2_import_get(void) { return im.win; }
void lj_gs2_import_refresh(void) { if (im.close_pending) lj_gs2_import_close(); }

/* ---- Export (one .Udf) and Export All (one .GS2 per user profile) ---- */

static struct { lj_window *win, *all; int close_pending, all_close, n; const char *items[GS2_ROWS]; char names[GS2_ROWS][64], log[GS2_ROWS][96]; const char *logp[GS2_ROWS]; } ex;

static int export_dir(char *out, size_t cap) { if (!lj_ctx_get()->lib || lj_library_path(lj_ctx_get()->lib, GS2_EXPORT_DIR, out, cap) != 0) return -1; lj_fs_mkdir(out); return 0; }

int lj_gs2_export_one(int index, char *err, size_t cap)
{
    lj_show *s = show();
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP], base[64];
    const char *dot;
    if (!s || index < 0 || index >= s->ngs2 || export_dir(dir, sizeof dir) != 0) { snprintf(err, cap, "no user profile selected"); return -1; }
    dot = strrchr(s->gs2[index].gs2_file, '.');
    snprintf(base, sizeof base, "%.*s", (int)(dot ? (size_t)(dot - s->gs2[index].gs2_file) : strlen(s->gs2[index].gs2_file)), s->gs2[index].gs2_file);
    if (snprintf(path, sizeof path, "%s/%s.Udf", dir, base) >= (int)sizeof path) { snprintf(err, cap, "path too long"); return -1; }
    return lj_udf_export(path, &s->gs2[index], err, cap);
}

int lj_gs2_export_all(void)
{
    lj_show *s = show();
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
    int i, n = 0;
    ex.n = 0;
    if (!s || export_dir(dir, sizeof dir) != 0) return -1;
    for (i = 0; i < s->ngs2 && ex.n < GS2_ROWS; i++) {
        int ok = snprintf(path, sizeof path, "%s/%s", dir, s->gs2[i].gs2_file) < (int)sizeof path && lj_gs2_save(path, &s->gs2[i]) == 0;   /* lj_gs2_write: the raw-record rule */
        snprintf(ex.log[ex.n], sizeof ex.log[0], "%s %s", s->gs2[i].gs2_file, ok ? "exported" : "not written");   /* PROPOSAL rig: Re1's log text */
        ex.logp[ex.n] = ex.log[ex.n]; ex.n++;
        n += ok;
    }
    if (ex.all) { lj_w_memo_set_lines(lj_window_find(ex.all, "Re1"), ex.logp, ex.n); lj_widget_invalidate(lj_window_find(ex.all, "Re1")); }
    return n;
}

static void ex_save(lj_widget *w, void *ud)
{
    char err[160];
    lj_widget *lb = ex.win ? lj_window_find(ex.win, "lb1") : NULL;
    (void)w; (void)ud;
    if (lj_gs2_export_one(lb ? ((lj_w_list *)lb)->index : -1, err, sizeof err) != 0) { lj_log("export profile: %s", err); return; }
    ex.close_pending = 1;
}
static const lj_binding ex_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "bSaveClick", ex_save, NULL }, { "BitBtn1Click", lj_handler_noop, NULL } /* bkHelp */, { NULL, NULL, NULL } };

lj_window *lj_gs2_export_open(void)
{
    lj_show *s = show();
    char dir[LJ_PATH_CAP];
    int i;
    if (ex.win) return ex.win;
    if (!(ex.win = lj_window_create(&lj_form_TEXPORTFIXTUREFORM, ex_b))) { lj_log("export profile: cannot create TExportFixtureForm"); return NULL; }
    ex.close_pending = 0;
    for (i = 0; s && i < s->ngs2 && i < GS2_ROWS; i++) { lj_str_copy(ex.names[i], sizeof ex.names[i], s->gs2[i].gs2_file); ex.items[i] = ex.names[i]; }   /* copies: a reload frees show->gs2 */
    lj_w_list_set_items(lj_window_find(ex.win, "lb1"), ex.items, i);
    lj_window_set_text(ex.win, "lExportFile", export_dir(dir, sizeof dir) == 0 ? dir : "");
    on_cancel(ex.win, "bCancel", &ex.close_pending);
    lj_win_show(ex.win, 0);
    return ex.win;
}

static void all_export(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("export all: %d profile(s) written", lj_gs2_export_all()); }
static void all_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; ex.all_close = 1; }   /* BitBtn1 bkCancel */
static const lj_binding all_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bExportClick", all_export, NULL },
    { "BitBtn1Click", all_cancel, NULL }, { NULL, NULL, NULL } };

lj_window *lj_gs2_export_all_open(void)
{
    if (ex.all) return ex.all;
    if (!(ex.all = lj_window_create(&lj_form_TEXPORTALLFORM, all_b))) { lj_log("export all: cannot create TExportAllForm"); return NULL; }
    ex.all_close = 0;
    lj_win_show(ex.all, 0);
    return ex.all;
}

void lj_gs2_export_close(void)
{
    if (ex.win) { lj_win_hide(ex.win); lj_window_destroy(ex.win); ex.win = NULL; }
    if (ex.all) { lj_win_hide(ex.all); lj_window_destroy(ex.all); ex.all = NULL; }
    ex.close_pending = ex.all_close = 0;
}
lj_window *lj_gs2_export_get(void) { return ex.win ? ex.win : ex.all; }
void lj_gs2_export_refresh(void)
{
    if (ex.close_pending && ex.win) { lj_win_hide(ex.win); lj_window_destroy(ex.win); ex.win = NULL; ex.close_pending = 0; }
    if (ex.all_close && ex.all) { lj_win_hide(ex.all); lj_window_destroy(ex.all); ex.all = NULL; ex.all_close = 0; }
}

void lj_gs2_dialogs_bind_fns(void) { }
