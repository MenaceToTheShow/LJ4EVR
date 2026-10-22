/* lj_windows.c - W1 contract stub (lj_windows.h); the windows tracks replace the
 * open/close/refresh bodies (one behaviour file per window) and keep the caption
 * helpers, which are complete here (F19, 13 s5 literals). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_windows.h"
#include "core/lj_log.h"
#include "windows/lj_lists.h"
#include "windows/lj_save_dialogs.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_dmx_monitor.h"       /* windows-c */
#include "windows/lj_cue_form.h"          /* windows-b (integrator: rows filled at integration) */
#include "windows/lj_master_intensity.h"
#include "windows/lj_fixture_config.h"
#include "windows/lj_hardware_setup.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"
#include "windows/lj_bgcue_form.h"       /* W2 windows-b */
#include "windows/lj_statics2_form.h"
#include "windows/lj_statics2_prefs.h"
#include "windows/lj_smoke_control.h"
#include "windows/lj_loop_options.h"      /* W2 windows-a */
#include "windows/lj_cue_time.h"
#include "windows/lj_cue_pages.h"
#include "windows/lj_global_patch.h"      /* W2 windows-c */
#include "windows/lj_htp_groups.h"
#include "windows/lj_fixture_groups.h"
#include "windows/lj_intensity_groups.h"
#include "windows/lj_position_presets.h"
#include "windows/lj_user_palettes.h"
#include "windows/lj_snapshot.h"
#include "windows/lj_delete_files.h"     /* W2 windows-d */
#include "windows/lj_user_libraries.h"
#include "windows/lj_backup_restore.h"
#include "windows/lj_notes_form.h"
#include "windows/lj_about_form.h"
#include "windows/lj_striking_lamps.h"
#include "windows/lj_system_prefs.h"
#include "windows/lj_edit_hotkeys.h"
#include "windows/lj_midi_in_form.h"
/* W3 (docs/w3-contracts.md) */
#include "windows/lj_cuelist_form.h"
#include "windows/lj_genmacro_editor.h"
#include "windows/lj_movemacro_editor.h"
#include "windows/lj_gdc_form.h"
#include "windows/lj_profile_editor_form.h"
#include "windows/lj_gs2_dialogs.h"

static const char *const lj_win_classes[LJ_WIN_COUNT] = {
    "TSequenceForm", "TCurrentCue", "TSaveSeqForm", "TSaveCueForm", "TSeqListForm", "TCueListForm", "TLinkConfigForm3" /* windows-c: the DFM class (no TFixtureConfigForm exists) */,
    "TMasterIntensityForm", "THardwareSetupForm2" /* windows-c: the DFM class */, "TDMXOutForm", "TStartupForm", "TStatusWarningForm", "TCloseOrSaveForm" /* windows-c: "Close Lightjockey ?" is this form; TConfirmForm is "Please Confirm.." */, "TStatusForm",
    /* W2 (docs/w2-contracts.md section 4; "code-built" windows carry the class the builder gives their lj_window) */
    "TLoopToolsForm", "TCueTimeForm", "TGetPageInfoForm", "TReorderCuePagesForm", "TBGCueForm", "TSelectBGCueForm2", "TSaveBGForm",
    "TStatics2Form", "TStatics2PrefsForm", "TStaticEntryPropertiesForm", "TSmokeForm", "TPresetForm", "TSavePresetForm",
    "TUserPaletteForm" /* code-built */, "TGroupForm" /* code-built [01 s4] */, "TIntensityGroupForm", "THTPGroupsForm" /* code-built */,
    "TGlobalPatchForm", "" /* LJ_WIN_DEFAULT_OUTPUT has no window; TDefault2Form "Set Fade Status" is LJ_WIN_SEQ_FADES (W3) */, "TSnapshotSceneForm", "TFunctionsForm", "THotkeyFunctionDialog", "TMidiInForm" /* code-built */,
    "TSystemPrefsForm", "TDeleteSequencesForm", "TDeleteCuesForm", "TDeleteCuelistsForm", "TDeleteBGCuesForm", "TDeleteGenericMacrosForm",
    "TUserLibForm", "TGetUserLibNameForm", "TLibraryBackupForm", "TLibraryRestoreForm", "TNoteForm", "TAboutForm2", "TAutoLampForm",
    /* W3 */
    "TCuelist2Form", "TSaveCuelist2Form", "TSelectCuelist2Form", "TClSetCueTrigForm", "TLogTimeCodeForm", "TCueMacroControlForm", "TSaveMacroDialog", "TLoadMacroList", "TMovementMacroForm" /* code-built */, "TDeleteMacrosForm", "TFanOutForm", "TAutoDelayForm", "TCueMacroForm", "TGenericMegaForm", "TFollowSpotForm", "TFollowListForm", "TDefault2Form", "TScanner2DefsForm", "TFixtureSetupForm", "TChannelDefForm", "TFixtureImportForm", "TExportFixtureForm" };
static const char *const lj_win_ids[LJ_WIN_COUNT] = {
    "seq_form", "cue_form", "save_seq", "save_cue", "list_seq", "list_cue", "fixture_config",
    "master_intensity", "hardware_setup", "dmx_monitor", "startup_options", "warnings", "close_prompt", "please_wait",
    /* W2 */
    "loop_options", "cue_time", "cue_page_name", "cue_pages_reorder", "bgcue_form", "select_bgcue", "save_bgcue",
    "statics2_form", "statics2_prefs", "static_props", "smoke_control", "position_presets", "save_preset",
    "user_palettes", "fixture_groups", "intensity_groups", "htp_groups",
    "global_patch", "default_output", "snapshot", "edit_hotkeys", "hotkey_function", "midi_in",
    "system_prefs", "delete_seq", "delete_cue", "delete_cl", "delete_bg", "delete_macro",
    "user_libraries", "user_lib_name", "backup", "restore", "notes", "about", "striking_lamps",
    /* W3 */
    "cuelist_form", "save_cuelist", "select_cuelist", "cl_set_cue_trig", "log_timecode", "genmacro_editor", "save_genmacro", "select_genmacro", "movemacro_editor", "delete_movemacro", "fan_out", "auto_delay", "cue_macro_control", "gdc_form", "followspot_form", "followspot_list", "seq_fades", "profile_editor", "customize_fixture", "channel_config", "gs2_import", "gs2_export" };

/* windows-a: the dispatch, one row per window id naming its behaviour file; windows-b/c add their rows by index
 * (a row left empty = that track has not landed: open logs and returns NULL). */
typedef struct { lj_window *(*open)(void); void (*close)(void); lj_window *(*get)(void); void (*refresh)(void); } lj_win_ops;
static const lj_win_ops lj_win_table[LJ_WIN_COUNT] = {
    [LJ_WIN_SEQ_FORM] = { lj_seq_form_open, lj_seq_form_close, lj_seq_form_get, lj_seq_form_refresh },
    [LJ_WIN_SAVE_SEQ] = { lj_save_seq_open, lj_save_seq_close, lj_save_seq_get, lj_save_seq_refresh },
    [LJ_WIN_LIST_SEQ] = { lj_seqlist_open, lj_seqlist_close, lj_seqlist_get, lj_seqlist_refresh },
    /* windows-b */
    [LJ_WIN_CUE_FORM]         = { lj_cue_form_open, lj_cue_form_close, lj_cue_form_get, lj_cue_form_refresh },
    [LJ_WIN_SAVE_CUE]         = { lj_save_cue_open, lj_save_cue_close, lj_save_cue_get, NULL },
    [LJ_WIN_LIST_CUE]         = { lj_list_cue_open, lj_list_cue_close, lj_list_cue_get, lj_list_cue_refresh },
    [LJ_WIN_MASTER_INTENSITY] = { lj_master_intensity_open, lj_master_intensity_close, lj_master_intensity_get, lj_master_intensity_refresh },
    /* windows-c */
    [LJ_WIN_FIXTURE_CONFIG]  = { lj_fixture_config_open, lj_fixture_config_close, lj_fixture_config_window, lj_fixture_config_refresh },
    [LJ_WIN_HARDWARE_SETUP]  = { lj_hardware_setup_open, lj_hardware_setup_close, lj_hardware_setup_window, lj_hardware_setup_refresh },
    [LJ_WIN_DMX_MONITOR]     = { lj_dmx_monitor_open, lj_dmx_monitor_close, lj_dmx_monitor_window, lj_dmx_monitor_refresh },
    [LJ_WIN_STARTUP_OPTIONS] = { lj_startup_options_open, lj_startup_options_close, lj_startup_options_window, NULL },
    [LJ_WIN_WARNINGS]        = { NULL /* lj_win_warning(state, 1) */, lj_warnings_close_all, lj_win_warnings_any, NULL },
    [LJ_WIN_CLOSE_PROMPT]    = { NULL /* lj_win_close_prompt(cb) */, lj_warnings_close_all, lj_win_close_prompt_window, NULL },
    /* W2 windows-b */
    [LJ_WIN_BGCUE_FORM]      = { lj_bgcue_form_open, lj_bgcue_form_close, lj_bgcue_form_get, lj_bgcue_form_refresh },
    [LJ_WIN_SAVE_BGCUE]      = { lj_save_bg_open, lj_save_bg_close, lj_save_bg_get, NULL },
    [LJ_WIN_STATICS2_FORM]   = { lj_statics2_form_open, lj_statics2_form_close, lj_statics2_form_get, lj_statics2_form_refresh },
    [LJ_WIN_STATICS2_PREFS]  = { lj_statics2_prefs_open, lj_statics2_prefs_close, lj_statics2_prefs_window, lj_statics2_prefs_refresh },   /* ME-91 */
    [LJ_WIN_STATIC_PROPS]    = { lj_static_props_open, lj_static_props_close, lj_static_props_window, lj_static_props_refresh },
    [LJ_WIN_SMOKE_CONTROL]   = { lj_smoke_control_open, lj_smoke_control_close, lj_smoke_control_get, lj_smoke_control_refresh },
    /* W2 windows-a */
    [LJ_WIN_LOOP_OPTIONS]      = { lj_loop_options_open, lj_loop_options_close, lj_loop_options_get, lj_loop_options_refresh },
    [LJ_WIN_CUE_TIME]          = { lj_cue_time_open, lj_cue_time_close, lj_cue_time_get, lj_cue_time_refresh },
    [LJ_WIN_CUE_PAGE_NAME]     = { lj_cue_page_name_open, lj_cue_page_name_close, lj_cue_page_name_get, NULL },
    [LJ_WIN_CUE_PAGES_REORDER] = { lj_cue_pages_reorder_open, lj_cue_pages_reorder_close, lj_cue_pages_reorder_get, NULL },
    /* W2 windows-c (LJ_WIN_DEFAULT_OUTPUT has no window: windows/lj_default_output.h; LJ_WIN_SAVE_PRESET unused: lj_position_presets.h) */
    [LJ_WIN_POSITION_PRESETS]  = { lj_position_presets_open, lj_position_presets_close, lj_position_presets_window, lj_position_presets_refresh },
    [LJ_WIN_USER_PALETTES]     = { lj_user_palettes_open, lj_user_palettes_close, lj_user_palettes_window, lj_user_palettes_refresh },
    [LJ_WIN_FIXTURE_GROUPS]    = { lj_fixture_groups_open, lj_fixture_groups_close, lj_fixture_groups_window, lj_fixture_groups_refresh },
    [LJ_WIN_INTENSITY_GROUPS]  = { lj_intensity_groups_open, lj_intensity_groups_close, lj_intensity_groups_window, lj_intensity_groups_refresh },
    [LJ_WIN_HTP_GROUPS]        = { lj_htp_groups_open, lj_htp_groups_close, lj_htp_groups_window, lj_htp_groups_refresh },
    [LJ_WIN_GLOBAL_PATCH]      = { lj_global_patch_open, lj_global_patch_close, lj_global_patch_window, lj_global_patch_refresh },
    [LJ_WIN_SNAPSHOT]          = { lj_snapshot_open, lj_snapshot_close, lj_snapshot_window, lj_snapshot_refresh },
    /* W2 windows-d */
    [LJ_WIN_DELETE_SEQ]      = { lj_delete_seq_open, lj_delete_seq_close, lj_delete_seq_get, lj_delete_seq_refresh },
    [LJ_WIN_DELETE_CUE]      = { lj_delete_cue_open, lj_delete_cue_close, lj_delete_cue_get, lj_delete_cue_refresh },
    [LJ_WIN_DELETE_CL]       = { lj_delete_cl_open, lj_delete_cl_close, lj_delete_cl_get, lj_delete_cl_refresh },
    [LJ_WIN_DELETE_BG]       = { lj_delete_bg_open, lj_delete_bg_close, lj_delete_bg_get, lj_delete_bg_refresh },
    [LJ_WIN_DELETE_MACRO]    = { lj_delete_macro_open, lj_delete_macro_close, lj_delete_macro_get, lj_delete_macro_refresh },
    [LJ_WIN_USER_LIBRARIES]  = { lj_user_libraries_open, lj_user_libraries_close, lj_user_libraries_window, lj_user_libraries_refresh },
    [LJ_WIN_USER_LIB_NAME]   = { lj_user_lib_name_open, lj_user_lib_name_close, lj_user_lib_name_window, NULL /* closed through lj_user_libraries_refresh */ },
    [LJ_WIN_BACKUP]          = { lj_backup_open, lj_backup_close, lj_backup_window, lj_backup_refresh },
    [LJ_WIN_RESTORE]         = { lj_restore_open, lj_restore_close, lj_restore_window, lj_restore_refresh },
    [LJ_WIN_NOTES]           = { lj_notes_form_open, lj_notes_form_close, lj_notes_form_window, lj_notes_form_refresh },
    [LJ_WIN_ABOUT]           = { lj_about_form_open, lj_about_form_close, lj_about_form_window, lj_about_form_refresh },
    [LJ_WIN_STRIKING_LAMPS]  = { lj_striking_lamps_open, lj_striking_lamps_close, lj_striking_lamps_window, lj_striking_lamps_refresh },
    [LJ_WIN_SYSTEM_PREFS]    = { lj_system_prefs_open, lj_system_prefs_close, lj_system_prefs_window, lj_system_prefs_refresh },
    [LJ_WIN_EDIT_HOTKEYS]    = { lj_edit_hotkeys_open, lj_edit_hotkeys_close, lj_edit_hotkeys_window, lj_edit_hotkeys_refresh },
    [LJ_WIN_MIDI_IN]         = { lj_midi_in_form_open, lj_midi_in_form_close, lj_midi_in_form_window, lj_midi_in_form_refresh },
    /* W3 (docs/w3-contracts.md): every row filled by the contracts; the tracks replace the stub bodies, never these rows */
    [LJ_WIN_CUELIST_FORM] = { lj_cuelist_form_open, lj_cuelist_form_close, lj_cuelist_form_get, lj_cuelist_form_refresh },
    [LJ_WIN_SAVE_CUELIST] = { lj_save_cuelist_open, lj_save_cuelist_close, lj_save_cuelist_get, lj_save_cuelist_refresh },
    [LJ_WIN_SELECT_CUELIST] = { lj_select_cuelist_open, lj_select_cuelist_close, lj_select_cuelist_get, lj_select_cuelist_refresh },
    [LJ_WIN_CL_SET_CUE_TRIG] = { lj_cl_set_cue_trig_open, lj_cl_set_cue_trig_close, lj_cl_set_cue_trig_get, lj_cl_set_cue_trig_refresh },
    [LJ_WIN_LOG_TIMECODE] = { lj_log_timecode_open, lj_log_timecode_close, lj_log_timecode_get, lj_log_timecode_refresh },
    [LJ_WIN_GENMACRO_EDITOR] = { lj_genmacro_editor_open, lj_genmacro_editor_close, lj_genmacro_editor_get, lj_genmacro_editor_refresh },
    [LJ_WIN_SAVE_GENMACRO] = { lj_save_genmacro_open, lj_save_genmacro_close, lj_save_genmacro_get, lj_save_genmacro_refresh },
    [LJ_WIN_SELECT_GENMACRO] = { lj_select_genmacro_open, lj_select_genmacro_close, lj_select_genmacro_get, lj_select_genmacro_refresh },
    [LJ_WIN_MOVEMACRO_EDITOR] = { lj_movemacro_editor_open, lj_movemacro_editor_close, lj_movemacro_editor_get, lj_movemacro_editor_refresh },
    [LJ_WIN_DELETE_MOVEMACRO] = { lj_delete_movemacro_open, lj_delete_movemacro_close, lj_delete_movemacro_get, lj_delete_movemacro_refresh },
    [LJ_WIN_FAN_OUT] = { lj_fan_out_open, lj_fan_out_close, lj_fan_out_get, lj_fan_out_refresh },
    [LJ_WIN_AUTO_DELAY] = { lj_auto_delay_open, lj_auto_delay_close, lj_auto_delay_get, lj_auto_delay_refresh },
    [LJ_WIN_CUE_MACRO_CONTROL] = { lj_cue_macro_control_open, lj_cue_macro_control_close, lj_cue_macro_control_get, lj_cue_macro_control_refresh },
    [LJ_WIN_GDC_FORM] = { lj_gdc_form_open, lj_gdc_form_close, lj_gdc_form_get, lj_gdc_form_refresh },
    [LJ_WIN_FOLLOWSPOT_FORM] = { lj_followspot_form_open, lj_followspot_form_close, lj_followspot_form_get, lj_followspot_form_refresh },
    [LJ_WIN_FOLLOWSPOT_LIST] = { lj_followspot_list_open, lj_followspot_list_close, lj_followspot_list_get, lj_followspot_list_refresh },
    [LJ_WIN_SEQ_FADES] = { lj_seq_fades_open, lj_seq_fades_close, lj_seq_fades_get, lj_seq_fades_refresh },
    [LJ_WIN_PROFILE_EDITOR] = { lj_profile_editor_open, lj_profile_editor_close, lj_profile_editor_get, lj_profile_editor_refresh },
    [LJ_WIN_CUSTOMIZE_FIXTURE] = { lj_customize_fixture_open, lj_customize_fixture_close, lj_customize_fixture_get, lj_customize_fixture_refresh },
    [LJ_WIN_CHANNEL_CONFIG] = { lj_channel_config_open, lj_channel_config_close, lj_channel_config_get, lj_channel_config_refresh },
    [LJ_WIN_GS2_IMPORT] = { lj_gs2_import_open, lj_gs2_import_close, lj_gs2_import_get, lj_gs2_import_refresh },
    [LJ_WIN_GS2_EXPORT] = { lj_gs2_export_open, lj_gs2_export_close, lj_gs2_export_get, lj_gs2_export_refresh },
    [LJ_WIN_HOTKEY_FUNCTION] = { lj_hotkey_function_open, lj_hotkey_function_close, lj_hotkey_function_get, lj_hotkey_function_refresh },   /* W3: bodies in lj_cuelist_form.c */
    [LJ_WIN_SELECT_BGCUE] = { lj_select_bgcue_open, lj_select_bgcue_close, lj_select_bgcue_get, lj_select_bgcue_refresh },   /* ME-112: bodies in lj_bgcue_form.c */
};
static const lj_win_ops *lj_win_ops_of(enum lj_win id) { return (int)id >= 0 && id < LJ_WIN_COUNT ? &lj_win_table[id] : NULL; }

lj_window *lj_win_open(enum lj_win id)
{
    const lj_win_ops *o = lj_win_ops_of(id);
    if (!o || !o->open) { lj_log("lj_win_open: %s has no behaviour file yet", lj_win_id_name(id)); return NULL; }
    return o->open();
}
void lj_win_close(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); if (o && o->close) o->close(); }
void lj_win_toggle(enum lj_win id) { if (lj_win_get(id)) lj_win_close(id); else lj_win_open(id); }
lj_window *lj_win_get(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); return o && o->get ? o->get() : NULL; }
void lj_win_refresh(enum lj_win id) { const lj_win_ops *o = lj_win_ops_of(id); if (o && o->refresh) o->refresh(); }
void lj_win_refresh_all(void) { int i; for (i = 0; i < LJ_WIN_COUNT; i++) lj_win_refresh((enum lj_win)i); }
void lj_win_close_all(void) { int i; for (i = 0; i < LJ_WIN_COUNT; i++) lj_win_close((enum lj_win)i); }
lj_window *lj_win_find(const char *form)
{
    int i;
    if (!form) return NULL;
    for (i = 0; i < LJ_WIN_COUNT; i++)
        if (strcmp(form, lj_win_classes[i]) == 0 || strcmp(form, lj_win_ids[i]) == 0) return lj_win_get((enum lj_win)i);
    return NULL;
}
const char *lj_win_class(enum lj_win id) { return ((int)id >= 0 && id < LJ_WIN_COUNT) ? lj_win_classes[id] : ""; }
static const lj_win_host *lj_win_the_host;   /* windows-c: stored so the behaviour files can reach the screen */
void lj_win_set_host(const lj_win_host *host) { lj_win_the_host = host; lj_seq_form_bind_fns(); lj_cue_form_bind_fns(); lj_master_intensity_bind_fns(); lj_bgcue_form_bind_fns(); lj_statics2_form_bind_fns(); lj_smoke_control_bind_fns() /* W2 windows-b */;
    lj_loop_options_bind_fns(); lj_cue_time_bind_fns(); lj_cue_pages_bind_fns(); /* W2 windows-a */
    lj_htp_groups_bind_fns(); lj_fixture_groups_bind_fns(); lj_position_presets_bind_fns(); lj_user_palettes_bind_fns(); /* W2 windows-c */
    lj_cuelist_form_bind_fns(); lj_genmacro_editor_bind_fns(); lj_movemacro_editor_bind_fns(); lj_gdc_form_bind_fns(); lj_profile_editor_form_bind_fns(); lj_gs2_dialogs_bind_fns(); /* W3 */ }

static int lj_caption(char *dst, size_t cap, const char *prefix, uint32_t number, const char *name, const char *suffix)
{
    int n;
    if (!dst || cap == 0) return -1;
    n = snprintf(dst, cap, "%s[%04u %s]%s", prefix, (unsigned)number, (name && name[0]) ? name : "No Name", suffix);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}
int lj_win_caption_seq(char *dst, size_t cap, uint32_t number, const char *name) { return lj_caption(dst, cap, "SEQUENCE ", number, name, ""); }
int lj_win_caption_cue(char *dst, size_t cap, uint32_t number, const char *name, int edit) { return lj_caption(dst, cap, edit ? "EDIT CUE " : "CUE ", number, name, ""); }
int lj_win_caption_cuelist(char *dst, size_t cap, uint32_t number, const char *name, int tracking) { return lj_caption(dst, cap, "Cuelist ", number, name, tracking ? " Tracking on" : ""); }
int lj_win_caption_main(char *dst, size_t cap, const char *library)
{
    int n;
    if (!dst || cap == 0) return -1;
    n = snprintf(dst, cap, "Martin LightJockey (User lib: %s)", library ? library : "Default");
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* lj_win_warning / lj_win_close_prompt / lj_win_message: windows-c bodies in lj_warnings.c */
const char *lj_win_id_name(enum lj_win id) { return ((int)id >= 0 && id < LJ_WIN_COUNT) ? lj_win_ids[id] : ""; }

/* ---- windows-c additions (appended): the host accessor and the two show/hide helpers every behaviour file uses ---- */
const lj_win_host *lj_win_host_get(void) { return lj_win_the_host; }
int  lj_win_show(lj_window *w, unsigned flags) { return (w && lj_win_the_host && lj_win_the_host->open) ? lj_win_the_host->open(lj_win_the_host->ud, w, flags) : 0; }
void lj_win_hide(lj_window *w) { if (w && lj_win_the_host && lj_win_the_host->close) lj_win_the_host->close(lj_win_the_host->ud, w); }

/* ---- W2 contract stubs (lj_windows.h additions); the windows tracks replace them and fill their lj_win_table rows ---- */
int  lj_win_is_dialog(enum lj_win id)
{
    switch (id) {
    case LJ_WIN_SAVE_SEQ: case LJ_WIN_SAVE_CUE: case LJ_WIN_CUE_PAGE_NAME: case LJ_WIN_SAVE_BGCUE: case LJ_WIN_STATIC_PROPS: case LJ_WIN_SAVE_PRESET:
    case LJ_WIN_SYSTEM_PREFS: case LJ_WIN_USER_LIB_NAME: case LJ_WIN_BACKUP: case LJ_WIN_RESTORE: case LJ_WIN_NOTES: case LJ_WIN_EDIT_HOTKEYS:
    case LJ_WIN_STATICS2_PREFS: case LJ_WIN_SNAPSHOT: case LJ_WIN_MIDI_IN:
    /* W3 typing dialogs (GDC_FORM is not one: its command line yields to assigned hotkeys, CHM thegenericdmxcontrol) */
    case LJ_WIN_SAVE_CUELIST: case LJ_WIN_CL_SET_CUE_TRIG: case LJ_WIN_SAVE_GENMACRO: case LJ_WIN_FOLLOWSPOT_FORM:
    case LJ_WIN_PROFILE_EDITOR: case LJ_WIN_CUSTOMIZE_FIXTURE: case LJ_WIN_CHANNEL_CONFIG: case LJ_WIN_GS2_IMPORT: case LJ_WIN_GS2_EXPORT:
        return 1;
    default: return 0;
    }
}
/* lj_win_about_text: windows-d body in lj_about_form.c */
