/* lj_config_dialogs.h - W4 window family CONFIG (docs/w4-contracts.md section 3), owned by the dlg-config track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_config_dialogs.c and adds what it needs below the contract block.
 *   LJ_WIN_ADD_FIXTURES  TAddFixForm2: Fixture Configuration bAdd / Add Fixture(s) [02 s2.3]
 *   LJ_WIN_APPLY_DEFAULT_DMX  TApplyDefaultDMXForm: progress box while Default DMX output is applied (Setup > Default Output) [02 s7]
 *   LJ_WIN_APPLY_MULTI  TApplyMultiForm: Fixture Configuration: apply a user profile to the selected fixtures [02 s2]
 *   LJ_WIN_AUTO_ADDRESS  TAutoAddressForm2: Fixture Configuration Calculate addresses (Auto) [02 s2.6, F43]
 *   LJ_WIN_CLEANUP  TCleanUpForm: Fixture Configuration Clear Fixture Data [02 s2.10, F44]
 *   LJ_WIN_CLONE  TCloneForm: Fixture Configuration Copy Fixture [02 s2.9, F44]
 *   LJ_WIN_COPY_FROM  TCopyFromForm: Fixture Configuration Copy From Fixture (F44 copy filter) [02 s2.9]
 *   LJ_WIN_CONFIG_WARN  TConfigWarnForm: Fixture Configuration Save with conflicts: Warnings / 'Save Ayway' [F41, 02 s2.8]
 *   LJ_WIN_EXPORT_WARNING  TFixtureExportWarningDialog: Fixture Configuration Create Export File / Export All warning [02 s5]
 *   LJ_WIN_FILE_EXISTS  TFileExistsForm: Export when the target file exists [02 s5]
 *   LJ_WIN_FIND_PROFILE  TFindProfileForm: Fixture Configuration profile tree Find profile.. [02 s2]
 *   LJ_WIN_FIXTURE_INFO  TFixtureInfoForm: icon popup Fixture Info (LJ_ACT_FIXTURE_INFO, arg = fixture) [02 s2.13]
 *   LJ_WIN_FIXTURE_NOTES  TFixtureNotesForm: icon popup Fixture Notes (LJ_ACT_FIXTURE_NOTES, arg = fixture) [02 s2.13]
 *   LJ_WIN_FIXTURE_SPECIAL  TFixtureSpecialForm: Fixture Configuration Special [02 s2]
 *   LJ_WIN_HIDDEN_FIXTURE  THiddenFixtureForm: Fixture Configuration hidden-fixture flag [02 s2]
 *   LJ_WIN_INTENSITY_PATCH  TInternalIntensityPatchForm: Patch Sub Masters (Fixture Configuration / Master Intensity) [04 s11]
 *   LJ_WIN_SCANNER1_DEFS  TScanner1DefsForm: icon popup Define Generic Scanner1 profile [02 s5.5]
 *   LJ_WIN_COLOR_SCROLLER_DEFS  TColorScrolDefForm: icon popup Define Color Scroller profile [02 s5.6]
 *   LJ_WIN_RGBPACK_DEFS  TRGBPackDefForm: icon popup RGB pack configuration [02 s5.7]
 *   LJ_WIN_PALETTE_VALUE_TYPE  TPaletteValueTypeForm: Customize Fixture palette value type [02 s4.3]
 *   LJ_WIN_PROFILE_NAME_OLDSTYLE  TProfileNameOldStyleForm: Generic Fixture Profile on a format-0/1 .GS2 (ME-126) [16 s2]
 *   LJ_WIN_PROFILE_IMPORT_MODIFIED  TProfileImportModifiedFilesForm: Import Profile when a modified file exists [02 s5]
 *   LJ_WIN_BITMAP_PICKER  TBitMapPickerForm2: palette editors Select Bitmap (31x30 library) [07 s12]
 *   LJ_WIN_GET_BITMAP  TGetBitmapForm: Generic Fixture Profile Icon Bitmap Set [16 s4]
 *   LJ_WIN_RGB_PALETTE  TRGBPaletteForm: RGB pack Colors > RGB Palette [04 s5.3]
 */
#ifndef LJ_WINDOWS_CONFIG_DIALOGS_H
#define LJ_WINDOWS_CONFIG_DIALOGS_H

#include "windows/lj_windows.h"

lj_window *lj_config_dlg_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_config_dlg_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_config_dlg_get(enum lj_win id);       /* NULL when closed */
void       lj_config_dlg_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_config_dlg_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */
void       lj_config_dlg_target(int fixture);       /* the fixture (index 0..LJ_FIXTURES-1) Fixture Info / Notes / Special act on; set by the app before lj_win_open */

#endif /* LJ_WINDOWS_CONFIG_DIALOGS_H */
