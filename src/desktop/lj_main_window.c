/* lj_main_window.c - the main window desc (generated TLJMainForm menu + the two
 * desktop popups), its binding table and THE ME-29 table: top-level visibility
 * per F5, inert leaves, the D12 removal. Every handler name the DFM declares is
 * bound (ME-16); leaves whose window lives elsewhere go through lj_fn_run (io)
 * or lj_desktop_run_action (lj_desktop.h), so desktop/ links nothing above it. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include "desktop/lj_main_window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_fixture_icons.h"
#include "desktop/lj_selection.h"
#include "desktop/lj_statusbar.h"
#include "desktop/lj_unassigned.h"
#include "fixlib/lj_profile.h"
#include "io/lj_audio.h"
#include "io/lj_fn.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "windows/lj_prefs.h"
#include "windows/lj_profile_editor_form.h"


/* ---- the ME-29 table ---- */
static const lj_main_top_rule lj_top[] = {   /* F5: what 2.209 shows (toolandstatusbars.gif: System Setup View Fingers LJ-Manager Download Help) */
    { "M_System",    1, "F5" },
    { "M_Setup",     1, "F5" },
    { "M_View",      1, "F5" },
    { "M_OffLine",   0, "D12: no 3D Visualizer" },
    { "M_Capture",   0, "inventory s5: Capture never" },
    { "M_Fingers",   1, "F5: shown without hardware" },
    { "M_Generic",   2, "F5: when a generic fixture exists" },
    { "M_LJManager", 1, "F5: shown without hardware" },
    { "M_Download",  1, "F5: shown without hardware" },
    { "M_Audio",     3, "ME-181: only while the Audio Analyzer is enabled (DFM Visible = False; 01 \"only visible if ... configured\")" },
    { "M_SandNet",   0, "inventory s5: SandNet never" },
    { "M_MHelp",     1, "F5" },
};
static const lj_main_inert_leaf lj_inert[] = {   /* dropped back-ends: greyed, hint LJ_NA_HINT */
    { "M_ManagerLaunch", LJ_NA_HINT }, { "M_ManagerPrefs", LJ_NA_HINT }, { "M_ManagerHelp", LJ_NA_HINT },
    { "M_Download2510", LJ_NA_HINT }, { "M_Download2518", LJ_NA_HINT },
    { "M_CapturePanel", LJ_NA_HINT }, { "M_SandNetStatus", LJ_NA_HINT },
    /* W5 audio: M_RestartSound is live */ { "M_StartDAPlayer", LJ_NA_HINT }, { "M_WinampModule", LJ_NA_HINT },
    { "M_OfflineLaunch", LJ_NA_HINT }, { "M_OfflineEnable", LJ_NA_HINT }, { "M_OfflineDisable", LJ_NA_HINT }, { "OfflinePrefs", LJ_NA_HINT },
    { "M_OfflineAutoLaunch", LJ_NA_HINT }, { "M_Autoloadlastusedscene", LJ_NA_HINT }, { "M_OfflineFixtureSelect", LJ_NA_HINT }, { "M_AutoMinimizeondialogs", LJ_NA_HINT },
    /* W5 extapp: PI_DefineAppLink is live (TExternalAppSetupForm, I11 superseded) */
};
static const char lj_removed[] = "Offlinebar1";   /* View > Toolbars > Offline Toolbar: removed under D12, docs/fidelity.md */

const lj_main_top_rule   *lj_main_menu_top_table(int *n)   { if (n) *n = LJ_COUNT_OF(lj_top); return lj_top; }
const lj_main_inert_leaf *lj_main_menu_inert_table(int *n) { if (n) *n = LJ_COUNT_OF(lj_inert); return lj_inert; }
const char *lj_main_menu_removed_item(void) { return lj_removed; }

/* ---- the two desktop popups, captions verbatim [01 s3.5, s3.6] ---- */
#define LJ_MI(nm, cap, h) { .name = nm, .caption = cap, .on_click = h, .image_index = -1 }
#define LJ_MSEP(nm) { .name = nm, .caption = "-", .image_index = -1 }
#define LJ_MSUB(nm, cap, kids) { .name = nm, .caption = cap, .image_index = -1, .children = kids, .nchildren = LJ_COUNT_OF(kids) }
static const lj_menu_item_desc lj_pop_unassign[] = {
    LJ_MI("PD_UnassignAllTab", "All on Current Tab", "DeskPopClick"),   /* PROPOSAL: [01 s3.5] gives "(all on current tab / selected icons)", not the captions */
    LJ_MI("PD_UnassignSelected", "Selected Icons", "DeskPopClick"),
};
static const lj_menu_item_desc lj_pop_icons[] = {
    LJ_MI("PD_Style", "Style..", "DeskPopClick"),
    { .name = "PD_MoveAll", .caption = "Move All", .on_click = "DeskPopClick", .image_index = -1 },
    LJ_MI("PD_AutoArrange", "Auto Arrange..", "DeskPopClick"),
    { .name = "PD_ShowProgrammed", .caption = "Show Programmed Fixtures", .on_click = "DeskPopClick", .image_index = -1 },
    LJ_MSUB("PD_UnassignIcons", "Unassign Icons", lj_pop_unassign),
};
static const lj_menu_item_desc lj_pop_desk[] = {
    LJ_MI("PD_Tabs", "Tabs..", "DeskPopClick"),
    LJ_MI("PD_Grid", "Grid..", "DeskPopClick"),
    LJ_MI("PD_Image", "Image..", "DeskPopClick"),
};
static const lj_menu_item_desc lj_pop_desktop_items[] = {
    LJ_MI("PD_SelectAllProgrammed", "Select All Programmed", "DeskPopClick"),
    LJ_MI("PD_DeselectOdd", "Deselect odd fixtures", "DeskPopClick"),
    LJ_MI("PD_DeselectEven", "Deselect even fixtures", "DeskPopClick"),
    LJ_MSEP("PD_N1"),
    LJ_MI("PD_FixtureGroups", "Fixture Groups", "DeskPopClick"),
    LJ_MSUB("PD_Icons", "Icons", lj_pop_icons),
    LJ_MSUB("PD_Desktop", "Desktop", lj_pop_desk),
    LJ_MSEP("PD_N2"),
    LJ_MI("PD_Help", "Help", "DeskPopClick"),
};
static const lj_menu_item_desc lj_pop_align[] = {
    LJ_MI("PI_AlignTop", "Icon Top", "IconPopClick"), LJ_MI("PI_AlignHCenter", "Icon Horizontal Center", "IconPopClick"), LJ_MI("PI_AlignBottom", "Icon Bottom", "IconPopClick"),
    LJ_MI("PI_AlignLeft", "Icon Left", "IconPopClick"), LJ_MI("PI_AlignVCenter", "Icon Vertical Center", "IconPopClick"), LJ_MI("PI_AlignRight", "Icon Right", "IconPopClick"),
};
static const lj_menu_item_desc lj_pop_fixicon[] = {
    LJ_MI("PI_UnassignIcon", "Unassign Icon", "IconPopClick"),
    { .name = "PI_MoveIcon", .caption = "Move Icon", .on_click = "IconPopClick", .image_index = -1 },
    LJ_MSUB("PI_AlignIcon", "Align Icon", lj_pop_align),
};
static char lj_pi_customize_cap[32] = "Customize Fixture";   /* NIT-26: lj_main_menu_sync sets it per fixture (lj_customize_popup_caption) */
static const lj_menu_item_desc lj_pop_icon_items[] = {
    LJ_MI("PI_SelectProgrammedType", "Select All Programmed of this type", "IconPopClick"),
    LJ_MI("PI_SelectType", "Select All of this type", "IconPopClick"),
    LJ_MI("PI_DeselectType", "Deselect All of this type", "IconPopClick"),
    LJ_MSEP("PI_N1"),
    LJ_MI("PI_ClearThis", "Clear This Fixture", "IconPopClick"),
    LJ_MI("PI_ClearSelected", "Clear Selected Fixtures", "IconPopClick"),
    LJ_MSEP("PI_N2"),
    LJ_MI("PI_Info", "Fixture Info", "IconPopClick"),
    LJ_MI("PI_Notes", "Fixture Notes", "IconPopClick"),
    LJ_MI("PI_DmxOutput", "DMX Output", "IconPopClick"),
    LJ_MSEP("PI_N3"),
    LJ_MI("PI_FixtureConfig", "Fixture Configuration", "IconPopClick"),
    LJ_MI("PI_GlobalPatch", "Global Patch", "IconPopClick"),
    LJ_MI("PI_Customize", lj_pi_customize_cap, "IconPopClick"),
    LJ_MI("PI_DefineAppLink", "Define Application Link", "IconPopClick"),   /* [02 s5.7] CHM caption; shown on an External Application/Device icon only; W5 extapp: opens TExternalAppSetupForm */
    LJ_MSEP("PI_N4"),
    LJ_MSUB("PI_FixtureIcon", "Fixture Icon", lj_pop_fixicon),
    LJ_MSEP("PI_N5"),
    LJ_MI("PI_Help", "Help", "IconPopClick"),
};
const lj_menu_desc lj_main_popup_desktop = { "PopDesktop", 0, lj_pop_desktop_items, LJ_COUNT_OF(lj_pop_desktop_items) };
const lj_menu_desc lj_main_popup_icon    = { "PopIcon",    0, lj_pop_icon_items,    LJ_COUNT_OF(lj_pop_icon_items) };

/* ---- handlers ---- */
static lj_window *win_of(lj_widget *w) { lj_window *win = lj_window_of(w); return win ? win : lj_desktop_window(); }
static const lj_menu_item_desc *item_of(lj_widget *w) { lj_window *win = win_of(w); return win ? lj_window_menu_item(win) : NULL; }
static int item_is(lj_widget *w, const char *name) { const lj_menu_item_desc *it = item_of(w); return it && it->name && strcmp(it->name, name) == 0; }
static int item_tag(lj_widget *w) { const lj_menu_item_desc *it = item_of(w); return it ? it->tag : 0; }
static void fn(enum lj_fn f) { lj_fn_run(f, 1); }

static void h_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_CLOSE, 0); }
static void h_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_refresh(); }   /* ControlUpdateTimer / DelayedUpdateTimer: the 100 ms hook when the platform drives TTimer rows */
static void h_inert(lj_widget *w, void *ud);
static void h_audio(lj_widget *w, void *ud)   /* W5 audio: Start Audio Analyzer opens the built-in analyzer; the Digital Audio Player stays dropped (I5) */
{
    if (item_of(w) && item_of(w)->name && strcmp(item_of(w)->name, "M_RestartSound") == 0) lj_desktop_run_action(LJ_ACT_AUDIO_ANALYZER, 0); else h_inert(w, ud);
}
static void h_inert(lj_widget *w, void *ud) { (void)ud; lj_log("desktop: %s: %s", item_of(w) && item_of(w)->name ? item_of(w)->name : "?", LJ_NA_HINT); }
static void h_sync(lj_widget *w, void *ud) { (void)ud; lj_main_menu_sync(win_of(w)); }
static void h_autostrike(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_STRIKING_LAMPS, 1); }   /* W2: "From System->Auto Strike Lamps it runs until cancelled" [01 s6.2] */
static void h_fixture_config(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_FIXTURE_CONFIG, 0); }
static void h_default_out(lj_widget *w, void *ud) { (void)ud; lj_desktop_run_action(LJ_ACT_DEFAULT_OUT, item_tag(w)); }   /* CurrentScene1 tag 1, Clear1 tag 0 */
static void h_delete_files(lj_widget *w, void *ud)
{
    static const struct { const char *name; int kind; } k[] = { { "M_DeleteSequences", LJ_REC_SEQ }, { "M_DeleteCues", LJ_REC_CUE }, { "M_DeleteCuelists", LJ_REC_CL },
                                                               { "M_DeleteBGCues", LJ_REC_BG }, { "M_DeleteGenericMacros", LJ_REC_MACRO } };
    int i;
    (void)ud;
    for (i = 0; i < LJ_COUNT_OF(k); i++) if (item_is(w, k[i].name)) { lj_desktop_run_action(LJ_ACT_DELETE_FILES, k[i].kind); return; }
}
static void h_global_patch(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_GLOBAL_PATCH, 0); }
static void h_hardware(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HARDWARE_SETUP, 0); }
static void h_remote_setup(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_REMOTE_SETUP, 0); }
static void h_help(lj_widget *w, void *ud) { (void)ud; lj_desktop_run_action(item_is(w, "M_About") ? LJ_ACT_ABOUT : LJ_ACT_HELP, 0); }
static void h_edit_hotkeys(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_EDIT_HOTKEYS, 0); }
static void h_fingers(lj_widget *w, void *ud)
{
    (void)ud;
    if (item_is(w, "M_FingersStatus")) fn(LJ_FN_FINGERS_STATUS_PANEL);
    else if (item_is(w, "M_VirtualFingers")) fn(LJ_FN_VIRTUAL_FINGERS);
    else if (item_is(w, "M_FingersSetup")) lj_desktop_run_action(LJ_ACT_FINGERS_SETUP, 0);   /* W4: keyboard / MIDI transport (D16) */
    else h_inert(w, ud);
}
static void h_followspot(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_FOLLOWSPOT_SETTINGS, 0); }
static void h_generic(lj_widget *w, void *ud) { (void)w; (void)ud; fn(LJ_FN_GENERIC_CONTROLS); }
static int lj_hk_setup = 1, lj_hk_enabled = 1;   /* W2: the current setup 1..8 and Hotkeys Enabled (mirrors ctx->prefs; lj_desktop_hotkey_setup_set) */
static void h_hotkeys_enabled(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    int on = ctx->hotkeys ? !ctx->hotkeys->enabled : !lj_hk_enabled;
    (void)ud; (void)w;
    if (ctx->hotkeys) ctx->hotkeys->enabled = (uint8_t)on;
    if (ctx->prefs) ctx->prefs->hotkeys_enabled = (uint8_t)on;
    lj_desktop_run_action(LJ_ACT_HOTKEYS_ENABLED, on);
    lj_desktop_hotkey_setup_set(lj_hk_setup, on);
}
static void h_hotkey_setup(lj_widget *w, void *ud)   /* Setup > Hotkeys > <setup n> (ours: the items are code-built, LJ2 lists up to 8 saved setups [01 s7]) */
{
    lj_ctx *ctx = lj_ctx_get();
    int n = item_tag(w);
    (void)ud;
    if (n < 1 || n > LJ_HOTKEY_SETUPS_MAX) return;
    if (ctx->prefs) ctx->prefs->hotkey_setup = n;
    lj_desktop_run_action(LJ_ACT_HOTKEY_SETUP, n);
    lj_desktop_hotkey_setup_set(n, ctx->hotkeys ? ctx->hotkeys->enabled : lj_hk_enabled);
}
static void h_restart(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_RESTART, 0); }
static void h_restore(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_RESTORE_DATA, 0); }
static void h_user_libs(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_USER_LIBRARIES, 0); }
static void h_view(lj_widget *w, void *ud)   /* MenuViewClick: View > Toolbars > ... toggles */
{
    static const struct { const char *name; int tb; } t[] = { { "SequenceandCueToolbar1", LJ_TB_SEQCUE }, { "CueListToolbar1", LJ_TB_CUELIST }, { "FixtureToolbar1", LJ_TB_FIXTURE } };
    int i;
    (void)ud;
    for (i = 0; i < LJ_COUNT_OF(t); i++) if (item_is(w, t[i].name)) { lj_desktop_toolbar_show((enum lj_toolbar)t[i].tb, !lj_desktop_toolbar_visible((enum lj_toolbar)t[i].tb)); break; }
    if (item_is(w, "StatusBar1")) lj_statusbar_show(!lj_statusbar_visible());
    lj_main_menu_sync(win_of(w));
}
static void h_nextprev(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_NEXTPREV_SETTINGS, 0); }
static void h_select_helpfile(lj_widget *w, void *ud) { (void)ud; lj_desktop_run_action(LJ_ACT_SELECT_HELPFILE, item_tag(w)); }
static void h_startup(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_STARTUP_OPTIONS, 0); }
static void h_sysprefs(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_SYSTEM_PREFS, 0); }
static void h_view_control(lj_widget *w, void *ud)   /* ViewControlClick: by Tag [DFM] */
{
    (void)ud;
    switch (item_tag(w)) {
    case 1: fn(LJ_FN_SEQUENCE_CONTROL); return;
    case 2: fn(LJ_FN_TOGGLE_CUE_CONTROL); return;
    case 3: fn(LJ_FN_CUELIST_CONTROL); return;
    case 4: if (lj_unassigned_open()) lj_unassigned_refresh(); return;
    case 5: fn(LJ_FN_TOGGLE_NOTES); return;
    case 6: fn(LJ_FN_VIEW_DMX_OUTPUT); return;
    case 7: lj_desktop_run_action(LJ_ACT_WINDOWS_MEMORY, 0); return;
    default: break;
    }
    if (item_is(w, "M_ViewBackgroundCue")) fn(LJ_FN_TOGGLE_BGCUE_CONTROL);
    else if (item_is(w, "M_ViewStatics")) fn(LJ_FN_STATICS_CONTROL);
    else if (item_is(w, "M_ViewDebug")) lj_desktop_run_action(LJ_ACT_DEBUG_WINDOW, 0);   /* hidden in the DFM; the debug track settles the unhide (PROPOSAL) */
    else h_inert(w, ud);
}

/* Clear This / Selected Fixtures: every channel of the footprint Off in the current scene [01 s3.6] */
static void clear_fixture(int i)
{
    lj_ctx *ctx = lj_ctx_get();
    const lj_profile *p;
    lj_scene *sc;
    int base, k;
    if (!ctx->show || !ctx->show->current_seq || i < 0 || i >= LJ_FIXTURES) return;
    p = lj_fixlib_by_id(ctx->show->fixture[i].profile_id);
    base = lj_show_fixture_base(ctx->show, i);
    if (!p || base < 0 || ctx->current_scene >= ctx->show->current_seq->n) return;
    lj_lock();
    sc = &ctx->show->current_seq->scenes[ctx->current_scene];
    for (k = 0; k < p->footprint && base + k < LJ_CHANS; k++) { sc->val[base + k] = 0; sc->state[base + k] = LJ_OFF; }
    lj_unlock();
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
}

static void h_desk_pop(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = item_of(w);
    const char *n = it && it->name ? it->name : "";
    lj_window *win = win_of(w);
    (void)ud;
    if (strcmp(n, "PD_SelectAllProgrammed") == 0) fn(LJ_FN_SELECT_ALL_PROGRAMMED);
    else if (strcmp(n, "PD_DeselectOdd") == 0 || strcmp(n, "PD_DeselectEven") == 0) {
        int odd = n[11] == 'O', i;
        lj_ctx *ctx = lj_ctx_get();
        for (i = 0; ctx->show && i < LJ_FIXTURES; i++)
            if (ctx->show->fixture[i].link && ((i + 1) % 2 == 1) == odd) lj_sel_set(i, 0, 0);   /* fixture number = index + 1 */
    }
    else if (strcmp(n, "PD_FixtureGroups") == 0) lj_desktop_run_action(LJ_ACT_FIXTURE_GROUPS, 0);
    else if (strcmp(n, "PD_Style") == 0) lj_desktop_run_action(LJ_ACT_ICON_STYLE, 0);
    else if (strcmp(n, "PD_MoveAll") == 0) { lj_desktop_move_mode(!lj_desktop_move_mode_on()); lj_main_menu_sync(win); }
    else if (strcmp(n, "PD_AutoArrange") == 0) lj_desktop_run_action(LJ_ACT_AUTO_ARRANGE, 0);
    else if (strcmp(n, "PD_ShowProgrammed") == 0) { lj_desktop_show_programmed(!lj_desktop_show_programmed_on()); lj_main_menu_sync(win); }
    else if (strcmp(n, "PD_UnassignAllTab") == 0) lj_desktop_unassign_tab(0);
    else if (strcmp(n, "PD_UnassignSelected") == 0) lj_desktop_unassign_tab(1);
    else if (strcmp(n, "PD_Tabs") == 0) lj_desktop_run_action(LJ_ACT_DESKTOP_TABS, 0);
    else if (strcmp(n, "PD_Grid") == 0) lj_desktop_run_action(LJ_ACT_DESKTOP_GRID, 0);
    else if (strcmp(n, "PD_Image") == 0) lj_desktop_run_action(LJ_ACT_DESKTOP_IMAGE, 0);
    else if (strcmp(n, "PD_Help") == 0) lj_desktop_run_action(LJ_ACT_HELP, 0);
}

static void h_icon_pop(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = item_of(w);
    const char *n = it && it->name ? it->name : "";
    lj_ctx *ctx = lj_ctx_get();
    int f = lj_desktop_popup_fixture(), i;
    (void)ud;
    if (f < 0 || !ctx->show) return;
    if (strcmp(n, "PI_SelectType") == 0 || strcmp(n, "PI_DeselectType") == 0 || strcmp(n, "PI_SelectProgrammedType") == 0) {
        int on = n[3] == 'S';
        for (i = 0; i < LJ_FIXTURES; i++)
            if (ctx->show->fixture[i].link && lj_sel_same_type(i, f) && (n[9] != 'P' || lj_desktop_icon_programmed(i))) lj_sel_set(i, on, 0);
        lj_sel_set(f, 1, 1);   /* "The 'clicked' fixture automatically becomes master fixture" */
    }
    else if (strcmp(n, "PI_ClearThis") == 0) clear_fixture(f);
    else if (strcmp(n, "PI_ClearSelected") == 0) { for (i = 0; i < LJ_FIXTURES; i++) if (lj_sel_is_selected(i)) clear_fixture(i); }
    else if (strcmp(n, "PI_Info") == 0) lj_desktop_run_action(LJ_ACT_FIXTURE_INFO, f);
    else if (strcmp(n, "PI_Notes") == 0) lj_desktop_run_action(LJ_ACT_FIXTURE_NOTES, f);
    else if (strcmp(n, "PI_DmxOutput") == 0) fn(LJ_FN_VIEW_DMX_OUTPUT);
    else if (strcmp(n, "PI_FixtureConfig") == 0) lj_desktop_run_action(LJ_ACT_FIXTURE_CONFIG, f);
    else if (strcmp(n, "PI_GlobalPatch") == 0) lj_desktop_run_action(LJ_ACT_GLOBAL_PATCH, f);
    else if (strcmp(n, "PI_Customize") == 0) lj_desktop_run_action(LJ_ACT_CUSTOMIZE_FIXTURE, f);
    else if (strcmp(n, "PI_DefineAppLink") == 0) lj_desktop_run_action(LJ_ACT_EXTAPP_SETUP, f);   /* W5 extapp: f = the icon's fixture, 0-based */
    else if (strcmp(n, "PI_UnassignIcon") == 0) lj_desktop_unassign_icon(f);
    else if (strcmp(n, "PI_MoveIcon") == 0) lj_desktop_icon_move(f, !lj_desktop_icon_move_on(f));
    else if (strncmp(n, "PI_Align", 8) == 0) lj_desktop_align_icons(f, n + 8);
    else if (strcmp(n, "PI_Help") == 0) lj_desktop_run_action(LJ_ACT_HELP, 0);
}

static const lj_binding lj_bindings[] = {
    { "FormClose", h_close, NULL },
    { "ControlUpdateTimerTimer", h_timer, NULL },
    { "DelayedUpdateTimerTimer", h_timer, NULL },
    { "AutoStrikeLamps1Click", h_autostrike, NULL },
    { "Config21Click", h_fixture_config, NULL },
    { "DefaultCurrentScene1Click", h_default_out, NULL },
    { "DeleteFilesClick", h_delete_files, NULL },
    { "Fingers1Click", h_sync, NULL },
    { "GlobalPatch1Click", h_global_patch, NULL },
    { "Hardware2Click", h_hardware, NULL },
    { "M_RemoteSetupClick", h_remote_setup, NULL },   /* W5: the code-built Setup item below (not a DFM handler) */
    { "HelpClick", h_help, NULL },
    { "M_AudioClick", lj_handler_noop, NULL },        /* W5 audio: the &Audio header (LJ2 refreshed its items here) */
    { "M_AudioOptionsClick", h_audio, NULL },         /* W5 audio: one handler for the three items (DFM) */
    { "M_CaptureClick", h_inert, NULL },
    { "M_Downloadclick", h_inert, NULL },
    { "M_EditHotkeysClick", h_edit_hotkeys, NULL },
    { "M_FingersClick", h_fingers, NULL },
    { "M_FollowSpotClick", h_followspot, NULL },
    { "M_GenericClick", h_generic, NULL },
    { "M_HotkeysClick", h_sync, NULL },
    { "M_HotkeysEnabledClick", h_hotkeys_enabled, NULL },
    { "M_HotkeySetupClick", h_hotkey_setup, NULL },   /* W2: the code-built setup items (not a DFM handler; the patched menu below names it) */
    { "M_ManagerClick", h_inert, NULL },
    { "M_OffLineClick", h_inert, NULL },
    { "M_OfflineDisableClick", h_inert, NULL },
    { "M_OfflineLaunchClick", h_inert, NULL },
    { "M_OfflinePrefs", h_inert, NULL },
    { "M_RestartLightJockeyClick", h_restart, NULL },
    { "M_RestoreProgramDataClick", h_restore, NULL },
    { "M_SandNetStatusClick", h_inert, NULL },
    { "M_SystemClick", h_sync, NULL },
    { "M_UserLibrariesClick", h_user_libs, NULL },
    { "MenuViewClick", h_view, NULL },
    { "NextPreviousFixtureSelection1Click", h_nextprev, NULL },
    { "SearchHelp1Click", h_help, NULL },
    { "SelectHelpFile", h_select_helpfile, NULL },
    { "SelectHelpfile1Click", h_sync, NULL },
    { "StartUp1Click", h_startup, NULL },
    { "SystemPrefsClick", h_sysprefs, NULL },
    { "Toolbars1Click", h_sync, NULL },
    { "ViewControlClick", h_view_control, NULL },
    { "DeskPopClick", h_desk_pop, NULL },
    { "IconPopClick", h_icon_pop, NULL },
    { NULL, NULL, NULL } };

const lj_binding *lj_main_window_bindings(void) { return lj_bindings; }

/* ---- the desc and the window ---- */
static lj_menu_desc lj_main_menus[3];

/* W2: MainMenu1 with Setup > Hotkeys extended by the 8 setup items after M_HotkeySep [01 s2.2, s7]: "up to 8 previously
 * saved setups will appear below the Edit Hotkeys item". Caption = the .Hot file's name= (fallback "Setup n"), shown
 * only when <userdir>Hotkeys/SetupN.Hot exists or n is the current setup (lj_main_menu_sync); radio group 1. */
enum { LJ_MM_MAX = 16, LJ_MM_SETUP_MAX = 16, LJ_MM_HK_MAX = 16 };
static lj_menu_item_desc lj_mm_bar[LJ_MM_MAX], lj_mm_setup[LJ_MM_SETUP_MAX], lj_mm_hk[LJ_MM_HK_MAX];
static char lj_mm_hk_caption[LJ_HOTKEY_SETUPS_MAX][64];
static const char *const lj_mm_hk_names[LJ_HOTKEY_SETUPS_MAX] = { "M_HotkeySetup1", "M_HotkeySetup2", "M_HotkeySetup3", "M_HotkeySetup4", "M_HotkeySetup5", "M_HotkeySetup6", "M_HotkeySetup7", "M_HotkeySetup8" };

static const lj_menu_item_desc lj_mm_remote = { .name = "M_RemoteSetup", .caption = "&RS-232 / TCP Remote ...", .on_click = "M_RemoteSetupClick", .image_index = -1 };   /* ours, PROPOSAL rig W5-REM-4 */
static int find_item(const lj_menu_item_desc *items, int n, const char *name) { int i; for (i = 0; i < n; i++) if (items[i].name && strcmp(items[i].name, name) == 0) return i; return -1; }

static lj_menu_desc patched_main_menu(const lj_menu_desc *gen)
{
    lj_menu_desc d = *gen;
    const lj_menu_item_desc *setup, *hk;
    int s, h, k, g, n;
    if (gen->n > LJ_MM_MAX || (s = find_item(gen->items, gen->n, "M_Setup")) < 0) return d;
    setup = &gen->items[s];
    if (setup->nchildren + 1 > LJ_MM_SETUP_MAX || (h = find_item(setup->children, setup->nchildren, "M_Hotkeys")) < 0) return d;
    hk = &setup->children[h];
    if (hk->nchildren + LJ_HOTKEY_SETUPS_MAX > LJ_MM_HK_MAX) return d;
    memcpy(lj_mm_bar, gen->items, sizeof *lj_mm_bar * (size_t)gen->n);
    memcpy(lj_mm_setup, setup->children, sizeof *lj_mm_setup * (size_t)setup->nchildren);
    memcpy(lj_mm_hk, hk->children, sizeof *lj_mm_hk * (size_t)hk->nchildren);
    for (k = 0; k < LJ_HOTKEY_SETUPS_MAX; k++) {
        lj_menu_item_desc *it = &lj_mm_hk[hk->nchildren + k];
        memset(it, 0, sizeof *it);
        snprintf(lj_mm_hk_caption[k], sizeof lj_mm_hk_caption[k], "Setup %d", k + 1);
        it->name = lj_mm_hk_names[k]; it->caption = lj_mm_hk_caption[k]; it->on_click = "M_HotkeySetupClick";
        it->tag = k + 1; it->group_index = 1; it->radio_item = LJ_YES; it->image_index = -1;
    }
    lj_mm_setup[h].children = lj_mm_hk; lj_mm_setup[h].nchildren = hk->nchildren + LJ_HOTKEY_SETUPS_MAX;
    n = setup->nchildren;
    if ((g = find_item(lj_mm_setup, n, "GlobalPatch1")) >= 0) {   /* W5: the remote's settings after Global Patch (LJ2 ran LJ_RS232.exe on its own) */
        memmove(lj_mm_setup + g + 2, lj_mm_setup + g + 1, sizeof *lj_mm_setup * (size_t)(n - g - 1));
        lj_mm_setup[g + 1] = lj_mm_remote; n++;
    }
    lj_mm_bar[s].children = lj_mm_setup; lj_mm_bar[s].nchildren = n;
    d.items = lj_mm_bar;
    return d;
}

lj_window *lj_main_window_build(const lj_binding *extra)
{
    lj_form_desc *d = calloc(1, sizeof *d);
    lj_binding merged[LJ_COUNT_OF(lj_bindings) + 1];
    const lj_binding *b = lj_bindings;
    lj_window *win;
    if (!d) lj_fatal("main window: out of memory");
    *d = lj_form_TLJMAINFORM;                                      /* 799x606 outer / 791x560 client, Arial 8 pt, Constraints 300x100, "Please Wait.." [F2, 07 s4.1] */
    lj_main_menus[0] = patched_main_menu(lj_form_find_menu(&lj_form_TLJMAINFORM, "MainMenu1"));
    lj_main_menus[1] = lj_main_popup_desktop;
    lj_main_menus[2] = lj_main_popup_icon;
    d->menus = lj_main_menus; d->nmenus = 3;
    memset(d->events, 0, sizeof d->events);
    d->events[0].event = "OnClose"; d->events[0].handler = "FormClose";
    for (; extra && extra->event; extra++)
        if (strcmp(extra->event, "FormClose") == 0) { merged[0] = *extra; memcpy(merged + 1, lj_bindings, sizeof lj_bindings); b = merged; break; }
    win = lj_window_create(d, b);
    if (!win) free(d);
    return win;
}

void lj_main_window_free(lj_window *win)
{
    const lj_form_desc *d;
    if (!win) return;
    d = lj_window_desc(win);
    lj_window_destroy(win);
    free((void *)d);
}

static lj_binding lj_w0_close;   /* the W0 lj_app FormClose row, run through LJ_ACT_CLOSE until the app binds the close prompt */
static void w0_close(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)arg; (void)ud; if (lj_w0_close.fn) lj_w0_close.fn(lj_window_root(lj_desktop_window()), lj_w0_close.ud); }

lj_window *lj_main_window_create(const lj_binding *bindings)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *win;
    if (!ctx->show) ctx->allow_freeze = ctx->allow_offline = ctx->allow_solo = ctx->show_nextprev = 1;   /* no app context: LJ2's System Preferences defaults [01 s6.1] */
    for (; bindings && bindings->event; bindings++)   /* the other W0 noop rows are superseded by lj_bindings (ME-29) */
        if (strcmp(bindings->event, "FormClose") == 0) { lj_w0_close = *bindings; lj_desktop_bind_action(LJ_ACT_CLOSE, w0_close, NULL); break; }
    win = lj_desktop_create();
    if (win && !ctx->lib) lj_desktop_set_library("Default");   /* W0-era startup has no library: the app track replaces this with lj_desktop_set_library after lj_library_open */
    return win;
}

void lj_main_window_destroy(lj_window *win) { lj_desktop_destroy(win); }

/* ---- applying the table ---- */
static int audio_enabled(void) { const lj_ctx *c = lj_ctx_get(); return c->audio && c->audio->enabled; }   /* ME-181: the M_Audio rule */
static int generic_fixture_exists(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    int i;
    for (i = 0; ctx->show && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &ctx->show->fixture[i];
        if (f->link && lj_profile_window(lj_fixlib_by_id(f->profile_id), LJ_CTL_GENERIC)) return 1;
    }
    return 0;
}

void lj_main_menu_apply(lj_window *win)
{
    lj_menu *m = lj_window_menus(win);
    int i;
    if (!m) return;
    for (i = 0; i < LJ_COUNT_OF(lj_top); i++)
        lj_menu_set_visible(m, lj_top[i].name, lj_top[i].visible == 2 ? generic_fixture_exists() : lj_top[i].visible == 3 ? audio_enabled() : lj_top[i].visible);
    for (i = 0; i < LJ_COUNT_OF(lj_inert); i++) lj_menu_set_enabled(m, lj_inert[i].name, 0);
    lj_menu_set_visible(m, lj_removed, 0);
    lj_menu_set_enabled(m, "M_UserLibraries", 1);   /* W2: the DFM ships it disabled; User Libraries is live [01 s8] */
    if (lj_ctx_get()->prefs) { lj_hk_setup = lj_ctx_get()->prefs->hotkey_setup; lj_hk_enabled = lj_ctx_get()->prefs->hotkeys_enabled; }
    lj_main_menu_sync(win);
}

static void hotkey_items_sync(lj_menu *m)
{
    static lj_hotkeys scratch;   /* ponytail: each setup file is parsed whole for its name= line; 8 small files per menu sync */
    const lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP], err[64];
    int k;
    lj_menu_set_checked(m, "M_HotkeysEnabled", ctx->hotkeys ? ctx->hotkeys->enabled : lj_hk_enabled);
    for (k = 0; k < LJ_HOTKEY_SETUPS_MAX; k++) {
        FILE *f = ctx->userdir[0] && lj_hotkeys_setup_path(ctx->userdir, k + 1, path, sizeof path) == 0 ? fopen(path, "rb") : NULL;   /* probe first: the loader logs a missing file */
        int have = f != NULL;
        if (f) fclose(f);
        have = have && lj_hotkeys_load(path, &scratch, err, sizeof err) == 0;   /* a setup that does not load is not listed */
        if (have && scratch.name[0]) lj_str_copy(lj_mm_hk_caption[k], sizeof lj_mm_hk_caption[k], scratch.name);
        else snprintf(lj_mm_hk_caption[k], sizeof lj_mm_hk_caption[k], "Setup %d", k + 1);
        lj_menu_set_visible(m, lj_mm_hk_names[k], have || k + 1 == lj_hk_setup);
        lj_menu_set_checked(m, lj_mm_hk_names[k], k + 1 == lj_hk_setup);
    }
}

void lj_main_menu_sync(lj_window *win)
{
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    if (!m) return;
    lj_menu_set_checked(m, "SequenceandCueToolbar1", lj_desktop_toolbar_visible(LJ_TB_SEQCUE));
    lj_menu_set_checked(m, "CueListToolbar1", lj_desktop_toolbar_visible(LJ_TB_CUELIST));
    lj_menu_set_checked(m, "FixtureToolbar1", lj_desktop_toolbar_visible(LJ_TB_FIXTURE));
    lj_menu_set_checked(m, "StatusBar1", lj_statusbar_visible());
    lj_menu_set_checked(m, "PD_MoveAll", lj_desktop_move_mode_on());
    lj_menu_set_checked(m, "PD_ShowProgrammed", lj_desktop_show_programmed_on());
    if (lj_desktop_popup_fixture() >= 0) lj_str_copy(lj_pi_customize_cap, sizeof lj_pi_customize_cap, lj_customize_popup_caption(lj_desktop_popup_fixture()));
    lj_menu_set_checked(m, "PI_MoveIcon", lj_desktop_popup_fixture() >= 0 && lj_desktop_icon_move_on(lj_desktop_popup_fixture()));
    lj_menu_set_visible(m, "M_Generic", generic_fixture_exists());
    lj_menu_set_visible(m, "M_Audio", audio_enabled());                 /* ME-181: re-synced by the Hardware Setup Audio apply */
    {   /* I13 sweep: the External Application link item follows the icon's profile (tag EXTERNAL, id 90) */
        const lj_ctx *ctx = lj_ctx_get();
        int f = lj_desktop_popup_fixture();
        const lj_profile *p = f >= 0 && ctx->show ? lj_fixlib_by_id(ctx->show->fixture[f].profile_id) : NULL;
        lj_menu_set_visible(m, "PI_DefineAppLink", p && strcmp(p->tag, "EXTERNAL") == 0);
    }
    hotkey_items_sync(m);
}

void lj_desktop_hotkey_setup_set(int setup, int enabled)
{
    char buf[32];
    lj_window *win = lj_desktop_window();
    if (setup >= 1 && setup <= LJ_HOTKEY_SETUPS_MAX) lj_hk_setup = setup;
    lj_hk_enabled = enabled != 0;
    if (enabled) snprintf(buf, sizeof buf, "Hotkeys %d", lj_hk_setup); else snprintf(buf, sizeof buf, "Hotkeys Off");   /* PROPOSAL rig: LJ2 has no such panel */
    lj_statusbar_hotkeys(buf);
    if (win) lj_main_menu_sync(win);
}
