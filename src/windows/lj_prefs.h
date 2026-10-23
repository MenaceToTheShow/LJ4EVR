/* lj_prefs.h - Startup Options (TSTARTUPFORM, [01 s6.2]; Main.Ini [STARTUP] AutoLampOn
 * Seq Cue Cl PCUE SeqRun ClRun Minimized ShowNotes, -1 = none [18 s2]) and the four
 * 2.209 preferences check boxes of TSYSTEMPREFSFORM's External Links tab (External
 * Links (ver 0.1), External Links Memory (ver 1.0), Run as Primary / Secondary
 * Instance): present, persisted to Main.Ini, inert (ME-31, LORD_FIDELITY-8). */
#ifndef LJ_WINDOWS_PREFS_H
#define LJ_WINDOWS_PREFS_H

#include <stdint.h>
#include "core/lj_ini.h"
#include "windows/lj_windows.h"

typedef struct lj_sysprefs lj_sysprefs;   /* W2: defined below; forward-declared in desktop/lj_desktop.h for lj_ctx */

typedef struct { int auto_lamp_on /* 0 none, 1..5 minutes */; int32_t seq, cue, cl, pcue /* -1 = none */; uint8_t seq_run, cl_run, minimized, show_notes; } lj_startup_opts;
void lj_startup_opts_load(lj_ini *main_ini, lj_startup_opts *o);     /* missing keys = LJ2's shipped defaults (AutoLampOn=0 Seq=-1 Cue=-1 Cl=-1 PCUE=-1, flags 0) */
void lj_startup_opts_save(lj_ini *main_ini, const lj_startup_opts *o);

lj_window *lj_startup_options_open(void);
void       lj_startup_options_close(void);
lj_window *lj_startup_options_window(void);
int        lj_startup_options_apply(void);                           /* the Ok button: page -> ctx->main_ini; 0 / -1 */
/* W4 dlg-desktop (appended): the Select Startup Cuelist pick-back, the Auto Launch list (Startup.ini [AUTOLAUNCH], ours) */
enum { LJ_LAUNCH_MAX = 16 };
void       lj_startup_options_set_cl(int32_t number);                /* TSelectCuelist2ModalForm's choice -> EClNum (no-op when closed) */
int        lj_startup_options_add_launch(const char *path);          /* Browse result -> lbauto; 0 / -1 */
int        lj_auto_launch_load(char (*out)[LJ_PATH_CAP], int cap);   /* the saved list; returns the count */
void       lj_auto_launch_save(const char *const *paths, int n);



/* ---- W2: System Preferences (TSystemPrefsForm, five tabs [01 s6.1, 07 s12]) as one record ----
 * Main.Ini keys are LJ2's own, read from the shipped User\Default\Main.Ini and the exe's literal pool [13 s6, EXE
 * 0x323138 [PREFS] ..]; the mapping checkbox -> key is ours where the names differ (PROPOSAL rig, marked). The record
 * lives in lj_ctx (ctx->prefs) so the window edits it and the app (lj_app_prefs_load/save/apply) owns the file and the
 * consequences: toolbar gates (ME-29: Solo / OnLine-OffLine / Freeze / blind), warning boxes, close confirm, quick
 * save/load, LJ_CMD_PREFS for the engine-facing keys. The four External Links boxes stay inert (ME-31). */
struct lj_sysprefs {
    /* System Settings tab                         Main.Ini key */
    uint8_t bitmap_cache;                       /* [PREFS] BitmapCache (inert: no bitmap cache here) */
    uint8_t allow_freeze;                       /* [PREFS] AllowBlind      (PROPOSAL: the exe's pre-rename key; cbAllowFreeze) */
    uint8_t allow_seq_blind;                    /* [PREFS] AllowBlindSeq   (cbAllowSeqBlind) */
    uint8_t allow_offline;                      /* [PREFS] AllowOffline */
    uint8_t allow_solo;                         /* [SOLO] Enabled          (cbAllowSolo; RN 2.6 moved it here) */
    uint8_t open_on_lj_desktop;                 /* [PREFS] ConfineToDesktop */
    uint8_t open_on_windows_desktop;            /* [PREFS] ConfineToWindowsDesktop (shipped 1) */
    uint8_t start_on_secondary;                 /* [PREFS] SecondaryMonitor */
    uint8_t restore_desktop;                    /* [DESKTOP] Restore (with Maximized Left Top Width Height) */
    uint8_t keep_seq_blind;                     /* [PREFS] KeepSeqBlind: LORD_ENGINE-13 (the gate's "[SEQ] KeepContentsBlind" is this exe key) -> LJ_PREF_KEEP_SEQ_BLIND */
    /* Warnings tab */
    uint8_t warn_seq_modified, warn_cue_modified, warn_cl_modified;   /* [PREFS] WarnSeq WarnCue WarnCueList */
    uint8_t warn_overwrite_seq, warn_overwrite_cue, warn_overwrite_cl;/* [PREFS] WarnOWSeq WarnOWCue WarnOWCueList */
    uint8_t warn_blackout, warn_freeze, warn_offline;                 /* [PREFS] WarnBO WarnBlind (PROPOSAL: = Show Freeze Warning) WarnOffline -> lj_win_warning gates */
    uint8_t confirm_close;                      /* [PREFS] CloseConfirm -> the "Close Lightjockey ?" prompt */
    /* Quick Save/Load tab */
    uint8_t quick_seq, quick_cue, quick_cl, quick_macro;   /* [PREFS] QuickSeq QuickCue QuickCl QuickMacro */
    /* External Links tab (inert, ME-31) */
    uint8_t ext_links_01, ext_links_mem_10, primary_instance, secondary_instance;   /* [PREFS] ExtMem ExtMem10 Primary Secondary */
    uint8_t enable_offline_visualizer, enable_sandnet, enable_capture;                /* [OFFLINE] Enable43, [MISC] SANDNET, [CAPTURE] Enable (greyed: D12) */
    /* Data Restore tab (the restore data itself is W4) */
    uint8_t restore_password, restore_at_startup, restore_pass_prompt;   /* [RESTORE] Password AtStartup PassOnRestore (ours, PROPOSAL rig) */
    /* not on the dialog but read by the same owner: toolbar visibility and the misc flags the W2 windows need */
    uint8_t view_seqcue_bar, view_cuelist_bar, view_fixture_bar, view_status_bar;   /* [PREFS] ViewSCBar ViewClBar ViewFixBar ViewStatusBar (the W1 [TOOLBARS] keys retire to these, ME-29) */
    uint8_t mark_active_fixtures;               /* [PREFS] MarkActiveFixtures (the green/cyan programmed square, F11) */
    uint8_t move_id;                            /* [PREFS] MoveId */
    uint8_t show_hotkey_warning, show_no_hardware_warning;   /* [MISC] ShowHotkeyWarning ShowNoHardwareWarning */
    uint8_t hotkeys_enabled, hotkeys_disable_in_dialogs; int hotkey_setup;   /* [HOTKEYS] Enabled DialogDisable (shipped: Disable=1 DialogDisable=1) + File -> the current setup 1..8 (ours: File= holds "SetupN.Hot") */
};
void lj_sysprefs_defaults(lj_sysprefs *p);                       /* LJ2's shipped Main.Ini values [13 s6]: warnings on, AllowBlind 0 (!), AllowBlindSeq 1, AllowOffline 0, [SOLO] Enabled 1, KeepSeqBlind 1, ConfineToWindowsDesktop 1, bars 1 (LORD_FIDELITY-11 note: the W1 app forced the three gates ON; W2 reads the file, so a fresh library follows the shipped file) */
void lj_sysprefs_load(lj_ini *main_ini, lj_sysprefs *p);        /* missing keys = lj_sysprefs_defaults */
void lj_sysprefs_save(lj_ini *main_ini, const lj_sysprefs *p);  /* every key above, LJ2's spelling */

lj_window *lj_system_prefs_open(void);                           /* LJ_WIN_SYSTEM_PREFS: TSystemPrefsForm bound; page <- ctx->prefs */
void       lj_system_prefs_close(void);
lj_window *lj_system_prefs_window(void);
int        lj_system_prefs_apply(void);                          /* the OK button: page -> ctx->prefs, then LJ_ACT_SYSTEM_PREFS_APPLY (the app saves and re-gates); 0 / -1 */

#endif /* LJ_WINDOWS_PREFS_H */
