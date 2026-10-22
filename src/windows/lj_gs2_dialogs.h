/* lj_gs2_dialogs.h - .GS2 / .Udf import and export dialogs (design 5.3, ME-31). W3 contract (docs/w3-contracts.md); owner: the gs2 track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_GS2_DIALOGS_H
#define LJ_WINDOWS_GS2_DIALOGS_H

#include "ui/lj_window.h"

/* LJ_WIN_GS2_IMPORT "gs2_import": TFixtureImportForm "Import Fixture Profile" (+ TProfileImportModifiedFilesForm "File Exists", TProfileNameOldStyleForm "Oldstyle profile name") */
lj_window *lj_gs2_import_open(void);
void       lj_gs2_import_close(void);
lj_window *lj_gs2_import_get(void);
void       lj_gs2_import_refresh(void);

/* LJ_WIN_GS2_EXPORT "gs2_export": TExportFixtureForm "Exporting User profile" 381x396 (+ TFixtureExportWarningDialog "Warning", TExportAllForm: TLINKCONFIGFORM3 Export All, one .GS2 per user profile) */
lj_window *lj_gs2_export_open(void);
void       lj_gs2_export_close(void);
lj_window *lj_gs2_export_get(void);
void       lj_gs2_export_refresh(void);

/* W3 integrator: the testable cores (no file dialog: <library>\Import\ and <library>\Export\, PROPOSAL folders) */
int  lj_gs2_import_file(const char *path, char *err, size_t cap);   /* a .Udf / .Udo / .GS2 parsed and written to UserFixtureDef\<base>.GS2; no reload; 0 / -1 */
int  lj_gs2_import_folder(int keep_existing);                       /* every file of Import\ (keep_existing = "check modified": existing profiles kept), then LJ_ACT_GS2_RELOAD; count imported */
int  lj_gs2_export_one(int index, char *err, size_t cap);           /* show->gs2[index] as Export\<base>.Udf; 0 / -1 */
int  lj_gs2_export_all(void);                                       /* Export All (ME-31): every show->gs2 record as Export\<gs2_file> through lj_gs2_save; count written, -1 no library */
lj_window *lj_gs2_export_all_open(void);                            /* TExportAllForm (Fixture Configuration > Export All); closed by lj_gs2_export_close */

void lj_gs2_dialogs_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

#endif /* LJ_WINDOWS_GS2_DIALOGS_H */
