/* lj_desktop.h - the main window (design 2.9, 7.7, 7.8, [07 s4], [01 s2-s3]):
 * menu bar (ME-29 table in lj_main_window.c), the Sequence/cue, Cue list and
 * Fixture toolbars, the status bar, the tab strip and the fixture icon area,
 * selection, the Unassigned Fixtures list and the two desktop popups. Code-built,
 * SDL-free. Bodies: lj_main_window.c lj_toolbars.c lj_statusbar.c
 * lj_fixture_icons.c lj_desktop_tabs.c lj_selection.c lj_unassigned.c lj_ctx.c.
 *
 * lj_ctx is the process context every window, control and the desktop share:
 * desktop/ is the lowest layer that sees ui + model + engine + io, and
 * windows/ and controls/ may include it (they may not include app/). The app
 * fills it at startup before any window opens; there is one per process.
 * Mutations of show / current scene happen under lj_lock (design 3.1).
 * W1 desktop additions (not in the contract text): lj_ctx.view / show_nextprev, LJ_MAIN_CAPTION_FMT, LJ_DND_FIXTURE,
 * lj_desktop_set_library / set_wallpaper / set_host, the lj_desktop_action registry, lj_desktop_icon_rect. */
#ifndef LJ_DESKTOP_DESKTOP_H
#define LJ_DESKTOP_DESKTOP_H

#include <stdint.h>
#include "core/lj_ini.h"
#include "engine/lj_engine.h"
#include "io/lj_hotkeys.h"
#include "io/lj_sacn.h"
#include "model/lj_library.h"
#include "model/lj_model.h"
#include "ui/lj_window.h"

/* ---- the process context ---- */
enum lj_modified { LJ_MOD_SEQ = 0, LJ_MOD_CUE, LJ_MOD_CL };                     /* the status bar flags Seq. Modified / Cue Modified / CueList Modified */
typedef struct {
    lj_show    *show;                  /* owned by the app; current_seq is never NULL once startup is done */
    lj_engine  *engine;
    lj_library *lib;                   /* the open user library */
    lj_ini     *main_ini, *forms_ini, *hardware_ini;   /* Main.Ini / forms.ini (library), Hardware.Ini (installation) */
    lj_sacn_cfg *sacn;                 /* the live sender config; a change is posted as LJ_CMD_SACN_CFG with this pointer (ME-23) */
    lj_hotkeys *hotkeys;
    char        userdir[LJ_PATH_CAP];  /* the pref path (trailing separator) */
    uint16_t    current_scene;         /* 0-based index into show->current_seq->scenes; posted to the engine with LJ_CMD_CURRENT_SEQ on every change */
    uint8_t     blind;                 /* sequence blind mode [03 s4.5] */
    uint8_t     modified[3];           /* enum lj_modified */
    uint8_t     seq_saved, cue_saved;  /* the toolbar number fields turn yellow once saved (acceptance 4/5) */
    uint8_t     allow_freeze, allow_offline, allow_solo;   /* System Preferences gates (W2 flags; LJ2 defaults are all ON [01 s6.1], LORD_FIDELITY-11) */
    uint8_t     show_nextprev;         /* "Enable next/previous buttons" of the Previous/Next dialog [01 s3.3]; LJ2 default on */
    lj_engine_status view;             /* the app's last lj_engine_view copy (w1-contracts seam 3: the app alone calls lj_engine_view); tick_no 0 = none yet */
} lj_ctx;
lj_ctx *lj_ctx_get(void);                                   /* the one context; zeroed until the app fills it */
void    lj_ctx_set_modified(enum lj_modified which, int on); /* also repaints the status bar flag */
void    lj_ctx_set_scene(uint16_t index);                   /* under lj_lock: ctx->current_scene, then LJ_CMD_CURRENT_SEQ; repaints the counter "X:Y" */
void    lj_ctx_post(const lj_cmd *cmd);                     /* lj_lock + lj_engine_cmd + lj_unlock, at_us stamped with lj_clock_us when 0 */

/* ---- main window ---- */
#define LJ_MAIN_CAPTION_FMT "Martin LightJockey (User lib: %s)"   /* ME-37: THE caption literal [01 s2.1, F1]; "Please Wait.." (the DFM caption) shows until the library is open (LORD_FIDELITY-20) */
#define LJ_DND_FIXTURE 100                                        /* lj_dnd kind: Unassigned list -> desktop, number = fixture index */
lj_window *lj_desktop_create(void);                         /* menu (TLJMainForm tables, ME-29 visibility table), three toolbars, status bar, tabs, icon area; caption "Martin LightJockey (User lib: <name>)"; NULL + log */
void       lj_desktop_destroy(lj_window *win);
lj_window *lj_desktop_window(void);                         /* NULL before create */
void       lj_desktop_refresh(void);                        /* the 100 ms hook: clock, Master %, modified flags, toolbar enables (F9), icon colours, Trig LED */
void       lj_desktop_key(const lj_key_ev *ev, int in_dialog);   /* hotkey dispatch: lj_hotkeys_match -> lj_fn_run / lj_fn_run_prog */
void       lj_desktop_set_library(const char *name);             /* caption LJ_MAIN_CAPTION_FMT with the open library's name; NULL = "Please Wait.." */
void       lj_desktop_set_wallpaper(const lj_surface *img, int stretch, int center);   /* Tabs.Ini [IMAGE]: the decoded bitmap (platform/sdl), owned by the caller; NULL = none [F13] */

/* How a desktop-made window (the Unassigned list) reaches the screen; platform/ is not includable here (the app sets it, as windows/ does with lj_win_host). NULL = headless. */
typedef struct { void *ud; int (*open)(void *ud, lj_window *w, int stay_on_top); void (*close)(void *ud, lj_window *w); } lj_desktop_host;
void lj_desktop_set_host(const lj_desktop_host *host);
int  lj_desktop_host_open(lj_window *w, int stay_on_top);         /* through the host; -1 when headless */
void lj_desktop_host_close(lj_window *w);

/* Menu / popup leaves whose window lives in windows/ or app/ (desktop may not link them): the owner binds a handler at startup;
 * an unbound action logs once and does nothing, as lj_fn_run does. arg: DELETE_FILES = enum lj_rec_kind, DEFAULT_OUT 1 = Use Current Scene / 0 = Clear,
 * SELECT_HELPFILE = 1 English 2 Deutsch 3 Francais, FIXTURE_INFO/NOTES/CUSTOMIZE = fixture index, HELP = 0. */
enum lj_desktop_action {
    LJ_ACT_CLOSE = 0 /* FormClose: the "Close Lightjockey ?" prompt, then lj_app_quit */, LJ_ACT_USER_LIBRARIES, LJ_ACT_DELETE_FILES, LJ_ACT_RESTORE_DATA, LJ_ACT_RESTART,
    LJ_ACT_FIXTURE_CONFIG, LJ_ACT_HARDWARE_SETUP, LJ_ACT_GLOBAL_PATCH, LJ_ACT_SYSTEM_PREFS, LJ_ACT_STARTUP_OPTIONS, LJ_ACT_DEFAULT_OUT,
    LJ_ACT_FOLLOWSPOT_SETTINGS, LJ_ACT_NEXTPREV_SETTINGS, LJ_ACT_EDIT_HOTKEYS, LJ_ACT_HELP, LJ_ACT_ABOUT, LJ_ACT_SELECT_HELPFILE, LJ_ACT_WINDOWS_MEMORY,
    LJ_ACT_FIXTURE_GROUPS, LJ_ACT_FIXTURE_INFO, LJ_ACT_FIXTURE_NOTES, LJ_ACT_CUSTOMIZE_FIXTURE,
    LJ_ACT_ICON_STYLE, LJ_ACT_AUTO_ARRANGE, LJ_ACT_DESKTOP_TABS, LJ_ACT_DESKTOP_GRID, LJ_ACT_DESKTOP_IMAGE,   /* W2 dialogs [F13] */
    LJ_ACT_SAVE_SEQUENCE, LJ_ACT_SAVE_CUE, LJ_ACT_SAVE_CUELIST, LJ_ACT_NEW_CUELIST,   /* toolbar Save = the save dialogs (windows/), New cue list (W3) */
    LJ_ACT_MIDI_IN, LJ_ACT_FOLLOWSPOT /* arg 0 = re-activate last setting (left click), 1 = selection dialog (right click) */,
    LJ_ACT_CTL_OPEN_ONLY /* arg = enum lj_ctl_kind: right-click on a fixture-control button [F9] */,
    LJ_ACT_COUNT
};
typedef void (*lj_desktop_action_fn)(enum lj_desktop_action a, int arg, void *ud);
int  lj_desktop_bind_action(enum lj_desktop_action a, lj_desktop_action_fn fn, void *ud);   /* -1 out of range */
void lj_desktop_run_action(enum lj_desktop_action a, int arg);

/* toolbars [01 s2.3, F9]: visibility persists in Main.Ini [TOOLBARS] (ours); the Offline toolbar is dropped (D12, ME-29) */
enum lj_toolbar { LJ_TB_SEQCUE = 0, LJ_TB_CUELIST, LJ_TB_FIXTURE, LJ_TB_COUNT };
void lj_desktop_toolbar_show(enum lj_toolbar tb, int show);
int  lj_desktop_toolbar_visible(enum lj_toolbar tb);
void lj_desktop_toolbar_update(void);                       /* enables from the master's profile windows + the engine view (Blackout/Freeze/OnLine LEDs); called by lj_desktop_refresh */
void lj_desktop_seqcue_fields(uint32_t seq_no, const char *seq_name, int scene, int scenes, uint32_t cue_no, const char *cue_name);   /* the toolbar number/name fields and the "X:Y" counter; yellow when ctx->seq_saved / cue_saved */

/* status bar [01 s2.4]: clock | Running.. | memory | Master 100% | Seq. Modified | Cue Modified | CueList Modified | Trig */
enum lj_status_field { LJ_SB_CLOCK = 0, LJ_SB_STATUS, LJ_SB_MEMORY, LJ_SB_MASTER, LJ_SB_SEQ_MOD, LJ_SB_CUE_MOD, LJ_SB_CL_MOD, LJ_SB_TRIG, LJ_SB_COUNT };
void lj_desktop_status_set(enum lj_status_field f, const char *text);
void lj_desktop_status_flag(enum lj_status_field f, int on);   /* the three Modified panels and Trig: text shown / blank */

/* tabs and icons [07 s4, 7.8]: Tabs.Ini [TABNAMES] TAB0.. ; icon positions in Config.dat (placed x y tab) */
int  lj_desktop_tab(void);
void lj_desktop_tab_set(int tab);
int  lj_desktop_tab_count(void);
void lj_desktop_icons_refresh(void);                        /* after Config.dat / placement / selection changes: rebuilds the icon widgets of the current tab */
int  lj_desktop_icon_at(int x, int y);                      /* fixture index under an icon-area point, -1 */
void lj_desktop_icon_rect(int fixture, lj_rect *out);       /* the tile rect in icon-area coordinates (placed fixtures) */
int  lj_desktop_place_icon(int fixture, int x, int y);      /* drop from the Unassigned list: placed = 1, tab = current; 0 / -1 */
void lj_desktop_unassign_icon(int fixture);

/* selection [F12, 01 s3.2]; the desktop owns the state (Selection.Dat is W2) */
enum lj_sel_mode { LJ_SEL_INCLUSIVE = 0, LJ_SEL_EXCLUSIVE };
int  lj_sel_master(void);                                   /* fixture index or -1 */
int  lj_sel_is_selected(int i);
void lj_sel_set(int i, int selected, int make_master);      /* one fixture; make_master promotes it (the previous master becomes a slave) */
void lj_sel_click(int i, int shift, int ctrl);              /* the desktop click rule: plain = master (+ add in Inclusive, replace in Exclusive), Shift = force master, Ctrl = solo select */
void lj_sel_band(int x0, int y0, int x1, int y1, int ctrl); /* rubber band; ctrl = inside only */
void lj_sel_all(void);
void lj_sel_none(void);
int  lj_sel_count(void);
int  lj_sel_mode(void);
void lj_sel_mode_set(enum lj_sel_mode mode);
void lj_sel_next(void);                                     /* Previous/Next with the custom order [01 s3.3] */
void lj_sel_prev(void);
int  lj_sel_solo(void);                                     /* Fixture solo state (LJ_CMD_SOLO mirrors it) */
void lj_sel_solo_set(int on);
uint32_t lj_sel_serial(void);                               /* increments on every change; controls compare it on refresh (seam 2) */
const lj_profile *lj_sel_master_profile(void);              /* the master's profile or NULL (toolbar enables, control windows) */

/* Unassigned Fixtures list [01 s3.7]: grey, stay-on-top, No / FixtureType / Address / Username, Unassign All / Help, Move Icons, Auto Place All; opens after Fixture Configuration adds fixtures */
lj_window *lj_unassigned_open(void);
void       lj_unassigned_close(void);
void       lj_unassigned_refresh(void);
int        lj_unassigned_count(void);

/* popups [01 s3.5 desktop, s3.6 icon]: verbatim trees from the generated TLJMainForm popup menus */
enum lj_desktop_popup { LJ_POP_DESKTOP = 0, LJ_POP_ICON };
void lj_desktop_popup(enum lj_desktop_popup which, int x, int y, int fixture);

#endif /* LJ_DESKTOP_DESKTOP_H */
