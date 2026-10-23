/* lj_windows.h - the behaviour-heavy windows of W1 (design 2.9, 7.4, 10 W1):
 * one open/close/refresh entry per window, captions per F19 / 13 s5, the red
 * warning boxes and the close prompt. Each window is a generated form
 * (ui/forms/lj_forms_gen.c) plus a behaviour file binding every OnXxx its DFM
 * declares (ME-16): lj_seq_form.c lj_cue_form.c lj_save_dialogs.c lj_lists.c
 * lj_fixture_config.c lj_master_intensity.c lj_hardware_setup.c
 * lj_dmx_monitor.c lj_prefs.c lj_warnings.c (the design's file names carry the
 * lj_ prefix the layering test requires). Positions persist through
 * ui/lj_formsini.h with LJ2's section names (7.10).
 *
 * Seam 3 (windows <-> engine view): a window never touches the engine's state;
 * it copies lj_engine_status / lj_frame under lj_lock in its refresh and
 * paints from the copy (the Cue grid "1:2", the Sequence progress bars, the
 * DMX Output Monitor cells), and acts through lj_ctx_post(). */
#ifndef LJ_WINDOWS_WINDOWS_H
#define LJ_WINDOWS_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include "desktop/lj_desktop.h"
#include "ui/lj_window.h"

enum lj_win {
    LJ_WIN_SEQ_FORM = 0,     /* TSequenceForm  "SEQUENCE [0000 No Name]"  [SEQ] */
    LJ_WIN_CUE_FORM,         /* TCurrentCue    "CUE [0001 name]"          [CUE] */
    LJ_WIN_SAVE_SEQ,         /* TSaveSeqForm   */
    LJ_WIN_SAVE_CUE,         /* TSaveCueForm   [SAVECUEFORM] AutoClose SaveOnEnter SaveMacro */
    LJ_WIN_LIST_SEQ,         /* TSeqListForm   */
    LJ_WIN_LIST_CUE,         /* TCueListForm (global list) [ACUESELECT] */
    LJ_WIN_FIXTURE_CONFIG,   /* TFixtureConfigForm 790x590 */
    LJ_WIN_MASTER_INTENSITY, /* TMasterIntensityForm 664x297 [GLOBALINTENSITY] */
    LJ_WIN_HARDWARE_SETUP,   /* THardwareForm: the sACN page where Martin's cards were [08 s7] */
    LJ_WIN_DMX_MONITOR,      /* TDMXOUTFORM 356x264 + the code-built grid [F49, ME-28] */
    LJ_WIN_STARTUP_OPTIONS,  /* TStartupForm [01 s6.2] */
    LJ_WIN_WARNINGS,         /* TSTATUSWARNINGFORM / TFREEZEWARNFORM: the red boxes */
    LJ_WIN_CLOSE_PROMPT,     /* TCONFIRMFORM "Close Lightjockey ?" */
    LJ_WIN_PLEASE_WAIT,      /* the startup status window "Please Wait.." (LORD_FIDELITY-20) */
    /* ---- W2 (appended; the DFM class each binds, or "code-built" with the GIF it is sized from, is docs/w2-contracts.md section 4) ---- */
    LJ_WIN_LOOP_OPTIONS,     /* TLoopToolsForm      "Loop Options" 353x247 [03 s5.5]                 [LOOPFORM] Top */
    LJ_WIN_CUE_TIME,         /* TCueTimeForm        "Time Control": the 13 faders [03 s5.6]            [CUETIMEFORM] Top Expanded */
    LJ_WIN_CUE_PAGE_NAME,    /* TGetPageInfoForm    "Add New Cue Page" 254x77 (also Insert / Edit Current Page Name) */
    LJ_WIN_CUE_PAGES_REORDER,/* TReorderCuePagesForm "Re-order Cue Pages" */
    LJ_WIN_BGCUE_FORM,       /* TBGCueForm          "Background Cue" 384x124 [03 s7, 07 s12] */
    LJ_WIN_SELECT_BGCUE,     /* TSelectBGCueForm2   "Select Background Cue" (the list with the (T) column) */
    LJ_WIN_SAVE_BGCUE,       /* TSaveBGForm         "Save Background Cue" 198x92 */
    LJ_WIN_STATICS2_FORM,    /* TStatics2Form       "Statics" 510x356 design, pages / slots / Release Page / Release All [14 sB.3] */
    LJ_WIN_STATICS2_PREFS,   /* TStatics2PrefsForm  "Statics Preferences" 481x299 [14 sB.4]; Main.Ini [STATICS2] (ME-30) */
    LJ_WIN_STATIC_PROPS,     /* TStaticEntryPropertiesForm "Static entry properties" 540x300 [14 sB.5] (+ TGetStaticsPageNameForm, TStaticSlotNameForm) */
    LJ_WIN_SMOKE_CONTROL,    /* TSmokeForm          "Smoke Control" 310x213 [04 s12]; Main.Ini [SMOKE] */
    LJ_WIN_POSITION_PRESETS, /* TPresetForm         DFM caption "PresetForm" (runtime caption UNVERIFIED) [04 s4.3, F36]; forms.ini [MOVEPRE] */
    LJ_WIN_USER_PALETTES,    /* code-built: usercolorpalette.gif 169x208 is reduced (not in 07 s14's 1:1 list) -> every size PROPOSAL rig [07 s9.10] */
    LJ_WIN_FIXTURE_GROUPS,   /* code-built tree + TAddGroupForm "Add Fixture Group": fixturegroupwindow.gif 225x205 reduced, INI 220x250 -> PROPOSAL rig [01 s3.4, 07 s12]; forms.ini [GROUPS] */
    LJ_WIN_INTENSITY_GROUPS, /* TIntensityGroupForm "Intensity Groups" 325x348, 9 tabs [01 s4] */
    LJ_WIN_HTP_GROUPS,       /* code-built: no DFM; the Global Patch "HTP Groups" column edited per channel (02 s7.1) plus the 24 names of lj_htp_names; no GIF */
    LJ_WIN_GLOBAL_PATCH,     /* TGlobalPatchForm    "Global Patch" [02 s7.1]; File > New / Load / Save Global Patch (.GPA) */
    LJ_WIN_SNAPSHOT,         /* TSnapshotSceneForm  "Snapshot from DMX Out" 375x281 [03 s4.7]; Main.Ini [MISC] SNAPSHOTSETTINGS RTSNAPSHOTPARAM RTSNAPSHOTSCENES SNAPSHOTINSETTINGSIN */
    LJ_WIN_EDIT_HOTKEYS,     /* TFunctionsForm      "Edit Hotkeys" 758x469 [01 s7]; Options > Save / Load / Default Hotkeys, Disable Hotkeys in Dialogs */
    LJ_WIN_HOTKEY_FUNCTION,  /* THotkeyFunctionDialog "Select Function" (+ TShowHotKeyWarningForm "Notification !" 271x122, Main.Ini [MISC] ShowHotkeyWarning) */
    LJ_WIN_MIDI_IN,          /* code-built: LJ_MidiIn.exe's forms are not in LightJockey.exe (05 s7.1); midiin.gif is reduced -> PROPOSAL rig; tabs Setup | Sequence | Cue | Background cue | Cue list | Functions | MIDI Monitor [05 s7.2] */
    LJ_WIN_SYSTEM_PREFS,     /* TSystemPrefsForm    "System Preferences" 380x327, five tabs [01 s6.1]; Main.Ini keys in windows/lj_prefs.h */
    LJ_WIN_DELETE_SEQ,       /* TDeleteSequencesForm "Delete Sequences" (+ Check Used Sequences / Mark Unused: TSeqUsedForm "Checking, please wait..") */
    LJ_WIN_DELETE_CUE,       /* TDeleteCuesForm     "Delete Cues" */
    LJ_WIN_DELETE_CL,        /* TDeleteCuelistsForm "Delete Cuelists" */
    LJ_WIN_DELETE_BG,        /* TDeleteBGCuesForm   "Delete Background Cues" */
    LJ_WIN_DELETE_MACRO,     /* TDeleteGenericMacrosForm "Delete Generic Macros" (TDeleteMacrosForm "Delete Macros" = movement macros, W3) */
    LJ_WIN_USER_LIBRARIES,   /* TUserLibForm        "User Libraries" [01 s8]: Select / Create New / Delete / Rename / Close */
    LJ_WIN_USER_LIB_NAME,    /* TGetUserLibNameForm "Enter User Library Name" 345x89 */
    LJ_WIN_BACKUP,           /* TLibraryBackupForm  "Backup User Library" 569x257 (+ TBackupForm "Backing Up" progress) -> .lj4b */
    LJ_WIN_RESTORE,          /* TLibraryRestoreForm "Restore User Library" 715x299 (+ TRestoreLibProgressForm) */
    LJ_WIN_NOTES,            /* TNoteForm           "Notes" (poScreenCenter, stay-on-top) [01 s4] */
    LJ_WIN_ABOUT,            /* TAboutForm2         "About LightJockey" 360x346: our product string, VERSION, THIRD_PARTY_NOTICES.txt; no logo (D2, LORD_FIDELITY-16) */
    LJ_WIN_STRIKING_LAMPS,   /* TAutoLampForm       "Striking Lamps" progress with Stop [01 s6.2] */
    /* ---- W3 (appended; the TFORM class each binds, its file and owner: docs/w3-contracts.md section 4) ---- */
    LJ_WIN_CUELIST_FORM,      /* TCuelist2Form */
    LJ_WIN_SAVE_CUELIST,      /* TSaveCuelist2Form */
    LJ_WIN_SELECT_CUELIST,    /* TSelectCuelist2Form */
    LJ_WIN_CL_SET_CUE_TRIG,   /* TClSetCueTrigForm */
    LJ_WIN_LOG_TIMECODE,      /* TLogTimeCodeForm */
    LJ_WIN_GENMACRO_EDITOR,   /* TCueMacroControlForm */
    LJ_WIN_SAVE_GENMACRO,     /* TSaveMacroDialog */
    LJ_WIN_SELECT_GENMACRO,   /* TLoadMacroList */
    LJ_WIN_MOVEMACRO_EDITOR,  /* TMovementMacroForm (code-built) */
    LJ_WIN_DELETE_MOVEMACRO,  /* TDeleteMacrosForm */
    LJ_WIN_FAN_OUT,           /* TFanOutForm */
    LJ_WIN_AUTO_DELAY,        /* TAutoDelayForm */
    LJ_WIN_CUE_MACRO_CONTROL, /* TCueMacroForm */
    LJ_WIN_GDC_FORM,          /* TGenericMegaForm */
    LJ_WIN_FOLLOWSPOT_FORM,   /* TFollowSpotForm */
    LJ_WIN_FOLLOWSPOT_LIST,   /* TFollowListForm */
    LJ_WIN_SEQ_FADES,         /* TDefault2Form */
    LJ_WIN_PROFILE_EDITOR,    /* TScanner2DefsForm */
    LJ_WIN_CUSTOMIZE_FIXTURE, /* TFixtureSetupForm */
    LJ_WIN_CHANNEL_CONFIG,    /* TChannelDefForm */
    LJ_WIN_GS2_IMPORT,        /* TFixtureImportForm */
    LJ_WIN_GS2_EXPORT,        /* TExportFixtureForm */
    /* ---- W4 (appended; family, behaviour file and track: docs/w4-contracts.md section 3; the rows below dispatch through
     *      the family table of lj_windows.c, so no track edits lj_windows.c) ---- */
    LJ_WIN_ADD_FIXTURES,           /* TAddFixForm2                     Fixture Configuration bAdd / Add Fixture(s) [02 s2.3] */
    LJ_WIN_APPLY_DEFAULT_DMX,      /* TApplyDefaultDMXForm             progress box while Default DMX output is applied (Setup > Default Output) [02 s7] */
    LJ_WIN_APPLY_MULTI,            /* TApplyMultiForm                  Fixture Configuration: apply a user profile to the selected fixtures [02 s2] */
    LJ_WIN_AUTO_ADDRESS,           /* TAutoAddressForm2                Fixture Configuration Calculate addresses (Auto) [02 s2.6, F43] */
    LJ_WIN_CLEANUP,                /* TCleanUpForm                     Fixture Configuration Clear Fixture Data [02 s2.10, F44] */
    LJ_WIN_CLONE,                  /* TCloneForm                       Fixture Configuration Copy Fixture [02 s2.9, F44] */
    LJ_WIN_COPY_FROM,              /* TCopyFromForm                    Fixture Configuration Copy From Fixture (F44 copy filter) [02 s2.9] */
    LJ_WIN_CONFIG_WARN,            /* TConfigWarnForm                  Fixture Configuration Save with conflicts: Warnings / 'Save Ayway' [F41, 02 s2.8] */
    LJ_WIN_EXPORT_WARNING,         /* TFixtureExportWarningDialog      Fixture Configuration Create Export File / Export All warning [02 s5] */
    LJ_WIN_FILE_EXISTS,            /* TFileExistsForm                  Export when the target file exists [02 s5] */
    LJ_WIN_FIND_PROFILE,           /* TFindProfileForm                 Fixture Configuration profile tree Find profile.. [02 s2] */
    LJ_WIN_FIXTURE_INFO,           /* TFixtureInfoForm                 icon popup Fixture Info (LJ_ACT_FIXTURE_INFO, arg = fixture) [02 s2.13] */
    LJ_WIN_FIXTURE_NOTES,          /* TFixtureNotesForm                icon popup Fixture Notes (LJ_ACT_FIXTURE_NOTES, arg = fixture) [02 s2.13] */
    LJ_WIN_FIXTURE_SPECIAL,        /* TFixtureSpecialForm              Fixture Configuration Special [02 s2] */
    LJ_WIN_HIDDEN_FIXTURE,         /* THiddenFixtureForm               Fixture Configuration hidden-fixture flag [02 s2] */
    LJ_WIN_INTENSITY_PATCH,        /* TInternalIntensityPatchForm      Patch Sub Masters (Fixture Configuration / Master Intensity) [04 s11] */
    LJ_WIN_SCANNER1_DEFS,          /* TScanner1DefsForm                icon popup Define Generic Scanner1 profile [02 s5.5] */
    LJ_WIN_COLOR_SCROLLER_DEFS,    /* TColorScrolDefForm               icon popup Define Color Scroller profile [02 s5.6] */
    LJ_WIN_RGBPACK_DEFS,           /* TRGBPackDefForm                  icon popup RGB pack configuration [02 s5.7] */
    LJ_WIN_PALETTE_VALUE_TYPE,     /* TPaletteValueTypeForm            Customize Fixture palette value type [02 s4.3] */
    LJ_WIN_PROFILE_NAME_OLDSTYLE,  /* TProfileNameOldStyleForm         Generic Fixture Profile on a format-0/1 .GS2 (ME-126) [16 s2] */
    LJ_WIN_PROFILE_IMPORT_MODIFIED,/* TProfileImportModifiedFilesForm  Import Profile when a modified file exists [02 s5] */
    LJ_WIN_BITMAP_PICKER,          /* TBitMapPickerForm2               palette editors Select Bitmap (31x30 library) [07 s12] */
    LJ_WIN_GET_BITMAP,             /* TGetBitmapForm                   Generic Fixture Profile Icon Bitmap Set [16 s4] */
    LJ_WIN_RGB_PALETTE,            /* TRGBPaletteForm                  RGB pack Colors > RGB Palette [04 s5.3] */
    LJ_WIN_CUE_STACK,              /* TCueStackForm                    Cue control menu Cue Builder, hotkey Cue Builder [03 s5.8] */
    LJ_WIN_MODAL_SELECT_CUE,       /* TModalSelectCueForm              Cuelist edit panel Set Current Cue / cue pick [03 s6] */
    LJ_WIN_DELETE_SCENE,           /* TDeleteSceneForm                 Sequence window Delete (LJ_SB_DELETE) [03 s4.3] */
    LJ_WIN_INSERT_SCENES,          /* TInsertScenesForm                Sequence window Insert [03 s4.3] */
    LJ_WIN_SET_SEQUENCE,           /* TSetSequenceForm                 Sequence window Start Sequence at : [03 s4] */
    LJ_WIN_SEQ_USED,               /* TSeqUsedForm                     Delete Sequences Check Used Sequences / Mark Unused [01 s8] */
    LJ_WIN_SELECT_BGCUE_OLD,       /* TSelectBGCueForm                 BG slot Edit Sequence / cue-list Load BG Cue picker [03 s7] */
    LJ_WIN_SAVE_STATIC,            /* TSaveStaticForm                  Statics Control Save [03 s8, 14 sB.2] */
    LJ_WIN_STATICS_CONTROL,        /* TStaticsControlForm              View > Controls Statics Control [03 s8, 14 sB.2] */
    LJ_WIN_STATIC_SLOT_NAME,       /* TStaticSlotNameForm              Statics 2 slot popup Name [14 sB.3] */
    LJ_WIN_STATICS_REPLACE,        /* TStaticsReplaceForm              Statics 2 Replace matrix [14 sB.3] */
    LJ_WIN_NAME_SLOTS,             /* TNameSlotsForm                   Cue control Sequence Slot Labels [03 s5] */
    LJ_WIN_SELECT_CUELIST_MODAL,   /* TSelectCuelist2ModalForm         Startup Options Select Startup Cuelist [01 s6.2] */
    LJ_WIN_SORT_TIMECODE,          /* TSortTimeCodeForm                Cuelist Tools Sort Waits (ME-120) [03 s6] */
    LJ_WIN_TIMECODE_OFFSET,        /* TTimeCodeOffsetForm              Cuelist Tools Offset TimeCode (ME-120) [03 s6] */
    LJ_WIN_MOVEMENT_OPTIONS,       /* TMovementOptionsForm             Movement window Options > Position Options [04 s4] */
    LJ_WIN_BACKUP_PROGRESS,        /* TBackupForm                      Backup User Library progress 'Backing Up' [01 s8] */
    LJ_WIN_RESTORE_LIB_PROGRESS,   /* TRestoreLibProgressForm          Restore User Library progress [01 s8] */
    LJ_WIN_COPYING_FILES,          /* TCopyingFilesForm                User Libraries copy progress [01 s8] */
    LJ_WIN_RENAME_FILE,            /* TRenameFileForm                  User Libraries Rename [01 s8] */
    LJ_WIN_PROGRESS,               /* TProgressForm                    generic file progress [01 s8] */
    LJ_WIN_PRINT_WAIT,             /* TPrintWaitForm                   print spool wait (Print stays greyed: fidelity row) [01 s8] */
    LJ_WIN_RESTORE_DATA,           /* TRestoreDataForm                 System > Restore Program Data (LJ_ACT_RESTORE_DATA), System Preferences Data Restore tab [06 s4.2] */
    LJ_WIN_CREATE_DATA,            /* TCreateDataForm                  Creating Auto Restore Data (System Preferences Data Restore) [06 s4.2] */
    LJ_WIN_COMPRESS_RESTORE,       /* TCompressRestoreDataForm         Compressing Restore Data progress [06 s4.2] */
    LJ_WIN_RESTORE_PROGRESS,       /* TRestoreProgressForm             Restoring Files progress [06 s4.2] */
    LJ_WIN_NEW_PASSWORD,           /* TNewPasswordForm                 System Preferences Data Restore Enter New Password [06 s4.2] */
    LJ_WIN_UPDATE,                 /* TUpdateForm                      first start on an older record format 'Dataformat has Changed' [F56, 12 Q9] */
    LJ_WIN_UPDATE_FILE,            /* TUpdateFileForm                  format update progress 'Updating Files' [F56] */
    LJ_WIN_UPDATE_PROGRAM,         /* TUpdateProgramForm               installation update progress [F56] */
    LJ_WIN_UPDATE_BGCUES,          /* TUpdateBGCuesForm                older BG cue files 'File Format Update Needed' [F56] */
    LJ_WIN_UPDATE_CL2,             /* TUpdateCl2Form                   older .Cl2 'Datafile format update required' [F56, 12 Q9] */
    LJ_WIN_PATCH_UPDATE,           /* TPatchUpdateForm                 older patch 'Update to Patch Configuration' [F56, 12 Q9] */
    LJ_WIN_REBUILD_SETUP,          /* TRebuildSetupForm                setup rebuild 'Please Wait...' [F56] */
    LJ_WIN_INIT_FORM,              /* TInitForm1                       first-start splash TInitForm1 [F56] */
    LJ_WIN_AUTO_ARRANGE,           /* TAutoArrangeForm                 desktop popup Auto-Arrange Icons (LJ_ACT_AUTO_ARRANGE) [01 s3.5] */
    LJ_WIN_DESKTOP_GRID,           /* TGridForm                        desktop popup Desktop Grid (LJ_ACT_DESKTOP_GRID) [01 s3.5, F13] */
    LJ_WIN_ICON_STYLE,             /* TIconStyleForm                   desktop popup Select Icon Style (LJ_ACT_ICON_STYLE) [01 s3.5] */
    LJ_WIN_DESKTOP_IMAGE,          /* TImageSetupForm                  desktop popup Desktop Image (LJ_ACT_DESKTOP_IMAGE) [01 s3.5, F13] */
    LJ_WIN_DESKTOP_TABS,           /* TLayerManagerForm                desktop popup Desktop tabs (LJ_ACT_DESKTOP_TABS) [01 s3.5] */
    LJ_WIN_NEXTPREV_SELECTION,     /* TNextPrevSelectionForm           right-click prev/next (LJ_ACT_NEXTPREV_SETTINGS) [01 s3.3] */
    LJ_WIN_CUSTOM_ORDER,           /* TCustomOrderForm                 Next/Previous Fixture Selection Custom order [01 s3.3] */
    LJ_WIN_SAVE_SELECTION,         /* TSaveSelectionForm               Fan Out named orders / Custom order Save [01 s3.3, 17 s5.2] */
    LJ_WIN_AUTO_LAUNCH,            /* TAutoLaunchForm                  Startup Options auto-launch programs, behind the Execute File rule (LORD_C-8) [01 s6.2] */
    LJ_WIN_HOTKEY_WARNING,         /* TShowHotKeyWarningForm           Edit Hotkeys / Select Function 'Notification !' (Main.Ini [MISC] ShowHotkeyWarning) [01 s7] */
    LJ_WIN_HARDWARE_WARNING,       /* TShowHardwareWarningForm         startup without transmitter hardware 'Notification !' [12 Q9] */
    LJ_WIN_LASER_WARNING,          /* TLaserWarningForm                laser output active 'Warning!' Laser(s) emitting! [12 Q9] */
    LJ_WIN_UNBLOCK_LASERS,         /* TUnblockLasersForm               'Enable laser output ?' before laser output opens [12 Q9] */
    LJ_WIN_DMX_IN,                 /* TDmxInForm                       C-L toolbar DMX In (LJ_ACT_DMX_IN), Global Patch DMX-in source [04 s15, 05 s3] */
    LJ_WIN_DMXIN_TRANSLATION,      /* TDMXInValueTranslationForm       Dmx In translation tables [05 s3] */
    LJ_WIN_TRANSLATION_EDIT,       /* TTranslationEditForm             DMX-In translation Edit [05 s3] */
    LJ_WIN_VALUE_TABLE_ERROR,      /* TValueTableImportErrorForm       DMX-In translation import error [05 s3] */
    LJ_WIN_EXPORT_TABLE,           /* TExportTableForm                 DMX-In translation Export Value tables (.vtr) [05 s3] */
    LJ_WIN_DA2532,                 /* TDa2532Form_3                    C-L toolbar 2532 (LJ_FN_2532_CONTROL) [05 s4, D16] */
    LJ_WIN_DA2532_FUNCTIONS,       /* TButtonFunctions2532Form         2532 Direct Access key function list [05 s4] */
    LJ_WIN_DA2532_COLORS,          /* TColorScheme2532Form             2532 Direct Access Color Scheme [05 s4] */
    LJ_WIN_DA2532_TAB_COLORS,      /* TDA2532_TabColorsForm            2532 Direct Access Tab Colors [05 s4] */
    LJ_WIN_DA2532_TAB_HEIGHT,      /* TDa2532TabHeight                 2532 Direct Access Set Tab Size [05 s4] */
    LJ_WIN_DA2532_TEXT,            /* TEdit2532TextForm                2532 Direct Access Edit 2532 Text [05 s4] */
    LJ_WIN_DA2532_PAGE_NAMES,      /* TName2532PagesForm               2532 Direct Access 2532 Page Names [05 s4] */
    LJ_WIN_FINGERS,                /* TLJFingersMainForm               &Fingers > Virtual Fingers (LJ_FN_VIRTUAL_FINGERS) [04 s17, 05 s5, D16] */
    LJ_WIN_FINGERS_SETUP,          /* TFingersSetupDialog              &Fingers > Fingers Setup (LJ_ACT_FINGERS_SETUP) [05 s5] */
    LJ_WIN_FINGERS_STATUS,         /* TFingersStatus                   &Fingers > Fingers status panel (LJ_FN_FINGERS_STATUS_PANEL) [05 s5] */
    LJ_WIN_FINGERS_ERROR,          /* TFingersErrorForm                Fingers error box [05 s5] */
    LJ_WIN_FINGERS_SKIN,           /* TFingersSkinSelectForm           Virtual Fingers Select Skin [05 s5] */
    LJ_WIN_FINGERS_FUNCTIONS,      /* TFingersFunctionListForm         Fingers Setup F1-F10 function list [05 s5] */
    LJ_WIN_FINGERS_JOG_FUNCTIONS,  /* TFingersJogFunctionListForm      Fingers Setup Datawheel function list [05 s5] */
    LJ_WIN_FINGERS_PAGE_FUNCTIONS, /* TFingersPageFunctionListForm     Fingers Setup fader page function list [05 s5] */
    LJ_WIN_FINGERS_SLOT_FUNCTIONS, /* TFingersSlotFunctionListForm     Fingers Setup Cue Slot functions [05 s5] */
    LJ_WIN_FINGERS_UD_FUNCTIONS,   /* TFingersUDFunctionListForm       Fingers Setup Scroll button function list [05 s5] */
    LJ_WIN_LIGHTCORDER,            /* TLightCorderDialog               Cuelist > Tools Record to LightCorder file (M_RecordtoLightCorderClick) [13 s1.2] */
    LJ_WIN_SAVE_LIGHTCORDER,       /* TSaveLightCorderFileDialog       Record to LightCorder file Save [13 s1.2] */
    LJ_WIN_DEBUG,                  /* TDebugForm                       View > De&bugWindow, hidden (LJ_ACT_DEBUG_WINDOW; unhide method PROPOSAL) [13 s1.2, design 13 item 10] */
    LJ_WIN_STATICS_DEBUG,          /* TStaticsDebugForm                Debug window Statics debug [13 s1.2] */
    LJ_WIN_MSG_DEBUG,              /* TMsgDebugForm                    Debug window WM_CPOPYDATA DEBUG [13 s1.2] */
    LJ_WIN_MEMORY_VIEW,            /* TMemoryViewForm                  View > System Memory (ViewControlClick Tag 7, LJ_ACT_WINDOWS_MEMORY) [01 s4] */
    LJ_WIN_HELP,                   /* THelpViewer                      every Help leaf (LJ_ACT_HELP, F1): the bundled text viewer, code-built (design 13 item 12) */
    LJ_WIN_COUNT
};

lj_window *lj_win_open(enum lj_win id);                    /* creates (bindings, forms.ini position, platform open through the app hook) or raises; NULL + log */
void       lj_win_close(enum lj_win id);                   /* saves the position, destroys */
void       lj_win_toggle(enum lj_win id);
lj_window *lj_win_get(enum lj_win id);                     /* NULL when closed */
void       lj_win_refresh(enum lj_win id);                 /* repaint from the engine view / model (seam 3) */
void       lj_win_refresh_all(void);                       /* the 100 ms hook */
void       lj_win_close_all(void);                         /* shutdown and "Close All Windows" */
lj_window *lj_win_find(const char *form);                  /* by DFM class name ("TSequenceForm") or window id name ("seq_form"): the UI script driver's resolver */
const char *lj_win_class(enum lj_win id);                  /* "TSequenceForm" ... */
const char *lj_win_id_name(enum lj_win id);                /* "seq_form" ... the docs/ui-script.md window names */

/* The app supplies how a created lj_window reaches the screen (platform/ is not includable here). */
typedef struct { void *ud; int (*open)(void *ud, lj_window *w, unsigned flags); void (*close)(void *ud, lj_window *w); void (*show)(void *ud, lj_window *w, int show);
                 void (*move)(void *ud, lj_window *w, int x, int y); int (*pos)(void *ud, const lj_window *w, int *x, int *y); } lj_win_host;   /* move/pos appended by windows-c (LORD_FIDELITY-12: the warning boxes centre on the main window); NULL = not available */
void lj_win_set_host(const lj_win_host *host);             /* once at startup; NULL = headless (tests, lj4evr_render); also registers the windows' lj_fn rows */

/* captions (F19, 13 s5): 4-digit numbers, "No Name" for an unsaved record, verbatim literals */
int lj_win_caption_seq(char *dst, size_t cap, uint32_t number, const char *name);            /* "SEQUENCE [0000 No Name]" */
int lj_win_caption_cue(char *dst, size_t cap, uint32_t number, const char *name, int edit);  /* "CUE [0009 name]", "EDIT CUE [0009 name]" (prefix condition UNVERIFIED) */
int lj_win_caption_cuelist(char *dst, size_t cap, uint32_t number, const char *name, int tracking);   /* "Cuelist [0001 name]" + " Tracking on" */
int lj_win_caption_main(char *dst, size_t cap, const char *library);                         /* "LJ4EVR (User lib: Default)" */

/* warnings [03 s9, 13 s2.2]: the red boxes, stay-on-top, one per state */
enum lj_warning { LJ_WARN_BLACKOUT = 0 /* "BLACKOUT ACTIVE" */, LJ_WARN_FROZEN /* "Frozen !" */, LJ_WARN_FADE_OUT /* "FADE-OUT ACTIVE" */, LJ_WARN_FADE_IN /* "FADE-IN ACTIVE" */, LJ_WARN_OFFLINE, LJ_WARN_COUNT };
void lj_win_warning(enum lj_warning w, int show);          /* driven from lj_desktop_refresh by the engine view; Main.Ini [PREFS] Show*Warning gates */

/* close prompt: "Close Lightjockey ?" (TCONFIRMFORM, Yes/No); result through the callback on the UI thread */
void lj_win_close_prompt(void (*on_result)(int yes, void *ud), void *ud);

/* load-error / confirm boxes the W1 windows raise (caption, text verbatim from 13 s2.2; Ok only) */
void lj_win_message(const char *caption, const char *text);

/* windows-c additions (appended, lj_windows.c): the behaviour files reach the screen through the host the app installed */
const lj_win_host *lj_win_host_get(void);                   /* NULL = headless */
int  lj_win_show(lj_window *w, unsigned flags);             /* host->open, 0 when headless */
void lj_win_hide(lj_window *w);                             /* host->close, no-op when headless */


/* ---- W2 (appended) ----
 * Every id above LJ_WIN_PLEASE_WAIT dispatches through the same lj_win_open / close / get / refresh table
 * (lj_windows.c, one row per id filled by its windows track; an empty row logs "has no behaviour file yet").
 * Windows that need the app (loads, library switches, restarts) act through lj_desktop_run_action /
 * lj_ctx_post, never through app/. The three W2 seams (docs/w2-contracts.md):
 *  - statics / BG cue pinned pointers: the Statics 2 and Background Cue windows post LJ_CMD_STATIC_ACTIVATE /
 *    LJ_CMD_LOAD_BG with `st` / `bg` pointing at show->pages[..].slot[..] / a table record; a slot is cleared or a
 *    record freed only through lj_app_static_clear / lj_app_delete_record (the engine is handed NULL first).
 *  - MIDI: the MIDI-IN window edits the lj_midi_map it gets from lj_ctx (ctx->midi) and asks the app to save it;
 *    Learn = lj_midi_learn_begin + polling lj_midi_learn_take on its 100 ms refresh.
 *  - prefs: System Preferences edits ctx->prefs (windows/lj_prefs.h lj_sysprefs) and runs
 *    LJ_ACT_SYSTEM_PREFS_APPLY so the app saves Main.Ini, re-gates the toolbar and posts LJ_CMD_PREFS. */
int  lj_win_is_dialog(enum lj_win id);                     /* 1 for the typing dialogs (save / name / delete / prefs boxes): the "Disable Hotkeys in Dialogs" gate of lj_hotkeys_match */
void lj_win_about_text(char *dst, size_t cap);             /* "LJ4EVR <VERSION>" + the notices line the About form shows (ME-37 product string, LORD_FIDELITY-16) */

#endif /* LJ_WINDOWS_WINDOWS_H */
