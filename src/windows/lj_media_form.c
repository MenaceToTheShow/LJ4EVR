/* lj_media_form.c - the MEDIA family (windows/lj_media_form.h): the LJ MediaPlayer window (TLJMediaForm of LJMediaPlayer.exe,
 * W5-MED-5; docs/media.md). Code-built: LightJockey.exe carries no TLJMediaForm, so the rows below are the harvested TPF0 of
 * LJMediaPlayer.exe 2.209 (offset 0x77d00, read by a scratch decoder; facts only: geometry, captions, hints, colours, menu
 * tree), every number as the DFM writes it except the three marked PROPOSAL. Audio only (W5-MED-2): the black pPlayer area
 * stays empty, the video and playback-file (.mps) items are greyed with LJ_NA_HINT. The window and the cue list
 * drive the one clip of ctx->media through its host (05 s8.1 "only one instance returns timecode"). */
#include <stdio.h>
#include <string.h>
#include "windows/lj_media_form.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "io/lj_media.h"
#include "model/lj_cuelist.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_help_viewer.h"

#define MF_SEC     "LJMEDIAPLAYER"   /* forms.ini section, ours (LJ2: LJMediaPlayer.Ini [STARTUP] / [RecentMedia]); PROPOSAL key names */
#define MF_CAPTION "LightJockey MediaPlayer"   /* ours: the DFM caption 'LJMediaForm' is never shown (BorderStyle bsNone); the help's name */
#define MF_ON_TOP  2u                /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */
#define MF_CTRL_H  47                /* pControls Height */

/* ---- the rows (TPF0 @0x77d00) ---- */
#define MF_F  .font = LJ_FONT_SANS13, .font_inherited = 1
#define MF_SB(n, x, h_, a_, g_, e_, ...) { LJ_K_SPEEDBUTTON, n, x, 2, 20, 20, .hint = h_, .action = a_, .flat = LJ_YES, .num_glyphs = 2, .glyph = g_, \
    .parent_show_hint = LJ_NO, .show_hint = LJ_YES, .enabled = e_, .font = LJ_FONT_OTHER, .font_name = "Arial", .font_height = -11, __VA_ARGS__ }
static const lj_ctl_desc mf_panel1[] = {
    { LJ_K_PROGRESSBAR, "pg1", 1, 1, 158, 18, .align = LJ_AL_CLIENT, .min = 0, .max = 1000, .smooth = LJ_YES, .parent_show_hint = LJ_NO, .show_hint = LJ_YES, MF_F,
      .events = { { "OnMouseDown", "pg1MouseDown" }, { "OnMouseMove", "pg1MouseMove" }, { "OnMouseUp", "pg1MouseUp" } } },
};
static const lj_ctl_desc mf_controls[] = {
    MF_SB("sbPlay", 2, "Play Clip", "aPlay", "play", LJ_NO, .events = { { "OnClick", "ActionExecute" } }),
    MF_SB("sbPause", 21, "Pause", "aPause", "pause", LJ_NO, .events = { { "OnClick", "ActionExecute" } }),
    MF_SB("sbStop", 40, "Stop", "aStop", "stop", LJ_NO, .events = { { "OnClick", "ActionExecute" } }),
    MF_SB("sbScanRw", 59, "Rewind", NULL, "rewind", LJ_NO, .events = { { "OnMouseDown", "bScanMouseDown" }, { "OnMouseUp", "bScanMouseUp" } }),
    MF_SB("sbScanFw", 78, "Forward", NULL, "fastforward", LJ_NO, .events = { { "OnMouseDown", "bScanMouseDown" }, { "OnMouseUp", "bScanMouseUp" } }),
    MF_SB("sbFrameRW", 97, "Frame-", NULL, "previous", LJ_NO, .events = { { "OnMouseDown", "bScanMouseDown" }, { "OnMouseUp", "bScanMouseUp" } }),
    MF_SB("sbFrameFW", 117, "Frame+", NULL, "next", LJ_NO, .events = { { "OnMouseDown", "bScanMouseDown" }, { "OnMouseUp", "bScanMouseUp" } }),
    { LJ_K_SPEEDBUTTON, "sbLoadMediaFile", 2, 23, 25, 22, .hint = "Load Media File", .action = "aLoadMediaFile", .flat = LJ_YES, .num_glyphs = 2, .glyph = "folder",
      .parent_show_hint = LJ_NO, .show_hint = LJ_YES, MF_F, .events = { { "OnClick", "ActionExecute" } } },
    { LJ_K_EDIT, "eMediaFile", 27, 22, 343, 23, .tabstop = LJ_NO, .anchors = LJ_AK_LEFT | LJ_AK_TOP | LJ_AK_RIGHT, .color = LJ_CLR(0xC0C0C0), .parent_show_hint = LJ_NO,
      .readonly = LJ_YES, .show_hint = LJ_YES, .tab_order = 0, .font = LJ_FONT_OTHER, .font_name = "Arial", .font_height = -12 },
    { LJ_K_PANEL, "pTimeCode", 137, 2, 71, 20, .bevel_outer = LJ_BV_LOWERED, .caption = "00:00:00:00", .color = LJ_CLR(0x000000), .font_color = LJ_CLR(0x00FF00),
      .parent_font = LJ_NO, .parent_show_hint = LJ_NO, .show_hint = LJ_YES, .tab_order = 1, .font = LJ_FONT_OTHER, .font_name = "MS Sans Serif", .font_height = -13 },
    { LJ_K_PANEL, "Panel1", 210, 2, 160, 20, .anchors = LJ_AK_LEFT | LJ_AK_TOP | LJ_AK_RIGHT, .bevel_outer = LJ_BV_LOWERED, .color = LJ_CLR(0xFFFF00), .tab_order = 2, MF_F,
      .children = mf_panel1, .nchildren = LJ_COUNT_OF(mf_panel1) },
};
static const lj_ctl_desc mf_ctls[] = {
    /* PROPOSAL rig W5-MED-5: tbMenu (TToolBar 371x23, Color 16762566, ShowCaptions buttons File / Preferences / Playback File /
     * Help over MainMenu) is drawn as the window's menu bar, so the client loses its 23 top pixels: pPlayer 0..134, pControls 134 */
    { LJ_K_PANEL, "pPlayer", 0, 0, 371, 134, .align = LJ_AL_CLIENT, .bevel_outer = LJ_BV_NONE, .color = LJ_CLR(0x000000), .font_color = LJ_CLR(0xFFFFFF), .parent_font = LJ_NO,
      .tab_order = 1, .font = LJ_FONT_OTHER, .font_name = "Arial", .font_height = -12, .events = { { "OnMouseDown", "pPlayerMouseDown" } } },
    { LJ_K_PANEL, "pControls", 0, 134, 371, MF_CTRL_H, .align = LJ_AL_BOTTOM, .color = LJ_CLR(0x7777FF) /* Color 16742263 = $FF7777 (BGR) */, .tab_order = 0, MF_F,
      .children = mf_controls, .nchildren = LJ_COUNT_OF(mf_controls) },
    { LJ_K_MAINMENU, "MainMenu", 24, 104, 0, 0, MF_F },
    { LJ_K_POPUPMENU, "MainPop", 64, 104, 0, 0, MF_F, .events = { { "OnPopup", "MainPopPopup" } } },
};

/* menus: Action-linked items carry the TAction caption and OnExecute (ActionExecute and the DFM's other Execute handlers) */
#define MI(n, cap, act, h) { .name = n, .caption = cap, .action = act, .on_click = h, .image_index = -1 }
#define MSEP(n)            { .name = n, .caption = "-", .image_index = -1 }
static lj_menu_item_desc mf_recent[LJ_MEDIA_RECENT] = {   /* the five captions point at mf.recent_cap at open */
    MI("M_Recent0", "", NULL, "RecentClick"), MI("M_Recent1", "", NULL, "RecentClick"), MI("M_Recent2", "", NULL, "RecentClick"),
    MI("M_Recent3", "", NULL, "RecentClick"), MI("M_Recent4", "", NULL, "RecentClick") };
static const lj_menu_item_desc mf_file[] = {
    MI("LoadMediaClip1", "Load Media File", "aLoadMediaFile", "ActionExecute"), MSEP("N1"),
    MI("SavePlaybackDefinition2", "Save Playback Definition", "aSavePlayback", "ActionExecute"),
    MI("LoadPlaybackDefinition2", "Load Playback Definition", "aLoadPlayback", "ActionExecute"), MSEP("N2"),
    { .name = "M_RecentMediaFiles", .caption = "Recent Media Files", .auto_hotkeys = LJ_NO, .image_index = -1, .children = mf_recent, .nchildren = LJ_MEDIA_RECENT },
    { .name = "M_RecentPlayBackFiles", .caption = "Recent Playback Files", .auto_hotkeys = LJ_NO, .image_index = -1 }, MSEP("N3"),
    MI("M_Exit", "Exit", "aExit", "ActionExecute") };
static const lj_menu_item_desc mf_monitors[] = {
    { .name = "MStartUpMonitor0", .caption = "Primary Monitor", .radio_item = LJ_YES, .on_click = "MenuToggle", .image_index = -1 },
    { .name = "MStartUpMonitor1", .caption = "Second Monitor", .radio_item = LJ_YES, .on_click = "MenuToggle", .image_index = -1 },
    { .name = "MStartUpMonitor2", .caption = "Third Monitor", .radio_item = LJ_YES, .on_click = "MenuToggle", .image_index = -1 },
    { .name = "MStartUpMonitor3", .caption = "Fourth Monitor", .radio_item = LJ_YES, .on_click = "MenuToggle", .image_index = -1 } };
static const lj_menu_item_desc mf_size[] = {
    { .name = "M_StartUpDisplayCustom", .caption = "Custom", .radio_item = LJ_YES, .on_click = "StartUpMenuClick", .image_index = -1 },
    { .name = "M_StartUpDisplayFull", .caption = "FullScreen", .radio_item = LJ_YES, .on_click = "StartUpMenuClick", .image_index = -1 },
    { .name = "FullScreenMonitor1", .caption = "FullScreen Monitor", .image_index = -1, .children = mf_monitors, .nchildren = LJ_COUNT_OF(mf_monitors) } };
static const lj_menu_item_desc mf_startup[] = {
    { .name = "ScreenSize1", .caption = "Display Size", .image_index = -1, .children = mf_size, .nchildren = LJ_COUNT_OF(mf_size) },
    { .name = "M_StartupShowControls", .caption = "Show Controls", .on_click = "StartUpMenuClick", .image_index = -1 } };
static const lj_menu_item_desc mf_prefs[] = {
    { .name = "Startup1", .caption = "Startup", .image_index = -1, .children = mf_startup, .nchildren = LJ_COUNT_OF(mf_startup) },
    MI("M_Stayontop", "Stay on top", NULL, "PrefsClick"), MI("M_Keypass", "Pass keypresses to LightJockey", NULL, "PrefsClick"), MSEP("N4"),
    MI("MinimumSize1", "Minimum Size (Audio)", "aMinimumSize", "CustomSizeExecute"), MI("DefaultSize1", "Default Size", "aDefaultSize", "CustomSizeExecute"),
    MI("ClipSize1", "Media Clip Size", "aClipSize", "CustomSizeExecute") };
static const lj_menu_item_desc mf_pbmon[] = {
    MI("PrimaryMonitor2", "Primary Monitor", "aPlaybackMonitor0", "APlaybackMonitorSelect"), MI("ThirdMonitor2", "Second Monitor", "aPlaybackMonitor1", "APlaybackMonitorSelect"),
    MI("ThirdMonitor3", "Third Monitor", "aPlaybackMonitor2", "APlaybackMonitorSelect"), MI("FourthMonitor2", "Fourth Monitor", "aPlaybackMonitor3", "APlaybackMonitorSelect") };
static const lj_menu_item_desc mf_playback[] = {
    MI("GetCurrentSettings1", "Use Current Settings", "aGetPlaybackSettings", "aGetPlaybackSettingsExecute"), MSEP("N10"),
    MI("ShowControls1", "Show Controls", "aPlaybackShowControls", "PlaybackMenuExecute"), MI("LoopPlayback1", "Loop Playback", "aLoopPlayback", "PlaybackMenuExecute"),
    MI("aPlaybackCustom1", "Custom Display Size", "aPlaybackCustom", "PlaybackMenuExecute"), MI("FullScreen1", "Full Screen Display", "aPlaybackFullScreen", "PlaybackMenuExecute"),
    { .name = "FullScreenMonitor2", .caption = "FullScreen Monitor", .image_index = -1, .children = mf_pbmon, .nchildren = LJ_COUNT_OF(mf_pbmon) }, MSEP("N11"),
    MI("SavePlaybackDefinition1", "Save Playback Definition", "aSavePlayback", "ActionExecute"), MI("LoadMediaFile2", "Load Playback Definition", "aLoadPlayback", "ActionExecute") };
static const lj_menu_item_desc mf_help[] = { MI("M_Help", "Help", NULL, "M_HelpClick"), MSEP("N9"), MI("About1", "About", NULL, "About1Click") };
static const lj_menu_item_desc mf_main[] = {
    { .name = "MM_File", .caption = "File", .on_click = "MM_FileClick", .image_index = -1, .children = mf_file, .nchildren = LJ_COUNT_OF(mf_file) },
    { .name = "MM_Preferences", .caption = "Preferences", .image_index = -1, .children = mf_prefs, .nchildren = LJ_COUNT_OF(mf_prefs) },
    { .name = "MM_Playback", .caption = "Playback File", .image_index = -1, .children = mf_playback, .nchildren = LJ_COUNT_OF(mf_playback) },
    { .name = "MM_Help", .caption = "Help", .on_click = "MM_HelpClick", .image_index = -1, .children = mf_help, .nchildren = LJ_COUNT_OF(mf_help) } };
static const lj_menu_item_desc mf_poppb_mon[] = {
    MI("PrimaryMonitor1", "Primary Monitor", "aPlaybackMonitor0", "APlaybackMonitorSelect"), MI("SecondaryMonitor1", "Second Monitor", "aPlaybackMonitor1", "APlaybackMonitorSelect"),
    MI("ThirdMonitor1", "Third Monitor", "aPlaybackMonitor2", "APlaybackMonitorSelect"), MI("FourthMonitor1", "Fourth Monitor", "aPlaybackMonitor3", "APlaybackMonitorSelect") };
static const lj_menu_item_desc mf_poppb[] = {
    MI("UseCurrentSettings1", "Use Current Settings", "aGetPlaybackSettings", "aGetPlaybackSettingsExecute"), MI("Exit2", "-", NULL, "ActionExecute"),
    MI("ShowControls4", "Show Controls", "aPlaybackShowControls", "PlaybackMenuExecute"), MI("LoopPlayback2", "Loop Playback", "aLoopPlayback", "PlaybackMenuExecute"),
    MI("CustomSize1", "Custom Display Size", "aPlaybackCustom", "PlaybackMenuExecute"), MI("FullScreen4", "Full Screen Display", "aPlaybackFullScreen", "PlaybackMenuExecute"),
    { .name = "FullScreenMonitor3", .caption = "Full Screen Monitor", .image_index = -1, .children = mf_poppb_mon, .nchildren = LJ_COUNT_OF(mf_poppb_mon) },
    MI("Exit1", "-", NULL, "ActionExecute"),
    MI("SavePlaybackDefinition4", "Save Playback Definition", "aSavePlayback", "ActionExecute"), MI("LoadPlaybackDefinition1", "Load Playback Definition", "aLoadPlayback", "ActionExecute") };
static const lj_menu_item_desc mf_pop[] = {
    MI("Play1", "Play", "aPlay", "ActionExecute"), MI("Pause1", "Pause", "aPause", "ActionExecute"), MI("LoadMediaFile1", "Load Media File", "aLoadMediaFile", "ActionExecute"), MSEP("N6"),
    MI("ShowControls2", "Toggle Controls", "aToggleControls", "ActionExecute"), MSEP("N8"),
    MI("FullScreen3", "Full Screen", "aSwitchToFull", "ActionExecute"), MI("CustomSize2", "Custom Display Size", "aSwitchToCustom", "ActionExecute"),
    MI("MinimumSizeAudio1", "Minimum Size (Audio)", "aMinimumSize", "CustomSizeExecute"), MI("DefaultSize2", "Default Size", "aDefaultSize", "CustomSizeExecute"),
    MI("aClipSize1", "Media Clip Size", "aClipSize", "CustomSizeExecute"), MSEP("N7"),
    { .name = "PlaybackSettings1", .caption = "Playback File", .image_index = -1, .children = mf_poppb, .nchildren = LJ_COUNT_OF(mf_poppb) }, MSEP("N5"),
    MI("Pop_Exit", "Exit", "aExit", "ActionExecute") };
static const lj_menu_desc mf_menus[] = { { "MainMenu", 1, mf_main, LJ_COUNT_OF(mf_main) }, { "MainPop", 0, mf_pop, LJ_COUNT_OF(mf_pop) } };
static const lj_form_desc mf_form = {
    .name = "LJMediaForm", .class_name = "TLJMediaForm", .caption = MF_CAPTION, .left = 538, .top = 340,
    .client_width = 371, .client_height = 204 - 23,   /* ClientHeight 204 less tbMenu's 23 (PROPOSAL rig W5-MED-5, see mf_ctls) */
    .text_height = 15, .color = LJ_CLR(0xC0C0C0), .font = LJ_FONT_OTHER, .font_name = "Arial", .font_height = -12,
    .border = LJ_BS_SINGLE /* PROPOSAL rig W5-MED-5: BorderStyle bsNone moved by dragging the video area in LJ2; framed here */, .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU,
    .menu = "MainMenu", .popup_menu = "MainPop", .menus = mf_menus, .nmenus = LJ_COUNT_OF(mf_menus), .ctls = mf_ctls, .n = LJ_COUNT_OF(mf_ctls),
    .events = { { "OnActivate", "FormActivate" }, { "OnClose", "FormClose" }, { "OnCreate", "FormCreate" }, { "OnDestroy", "FormDestroy" } },
};

/* the items with no audio meaning (video, full screen, monitors, the .mps playback files: docs/media.md, fidelity rows) */
static const char *const mf_greyed[] = {
    "SavePlaybackDefinition2", "LoadPlaybackDefinition2", "M_RecentPlayBackFiles", "ScreenSize1", "ClipSize1", "MM_Playback",
    "FullScreen3", "CustomSize2", "aClipSize1", "PlaybackSettings1" };

static struct { lj_window *win; int minimum, pending_close, seeking; char recent_cap[LJ_MEDIA_RECENT][272]; } mf;

static lj_media *med(void) { return lj_ctx_get()->media; }
static lj_widget *find(const char *n) { return mf.win ? lj_window_find(mf.win, n) : NULL; }
static lj_ini *fini(void) { return lj_ctx_get()->forms_ini; }
int lj_media_form_keypass(void) { return lj_ini_geti(fini(), MF_SEC, "KeyPass", 1) != 0; }

static void run(int cmd, const char *param, uint32_t ms)
{
    lj_media *m = med();
    char err[160];
    if (!m) return;
    if (lj_media_cmd(m, m->host, cmd, param, ms, err, sizeof err) != 0) lj_win_message(MF_CAPTION, err);
    lj_media_form_refresh(LJ_WIN_MEDIA_PLAYER);
}

static void set_enabled(const char *n, int on) { lj_widget *w = find(n); if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = on != 0; lj_widget_invalidate(w); } }

static void show_controls(int on)
{
    lj_widget *c = find("pControls"), *p = find("pPlayer");
    if (!c || !p) return;
    c->visible = on != 0;
    lj_widget_layout(lj_window_root(mf.win));
    if (mf.minimum && on) lj_window_resize(mf.win, mf_form.client_width, MF_CTRL_H);
    lj_window_invalidate_all(mf.win);
    (void)p;
}

static void size_to(int minimum)   /* aMinimumSize: "resize to minimum hiding the video area"; aDefaultSize: the DFM size (the help's 320x200 is the video area) */
{
    lj_widget *p = find("pPlayer");
    mf.minimum = minimum;
    if (p) p->visible = !minimum;
    lj_window_resize(mf.win, mf_form.client_width, minimum ? MF_CTRL_H : mf_form.client_height);
    lj_widget_layout(lj_window_root(mf.win));
    lj_window_invalidate_all(mf.win);
}

static void pick(void)
{
    lj_media *m = med();
    if (!m || !m->host || !m->host->pick || m->host->pick(m->host->ud) != 0) { lj_log("media: no file dialog (headless)"); return; }
    m->pick_owner = 2;
}

static void on_action(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(mf.win);
    const char *a = it ? it->action : (w && w->desc ? w->desc->action : NULL);
    (void)ud;
    if (!a) return;
    if (!strcmp(a, "aPlay")) run(LJ_CL_MP_PLAY, NULL, 0);
    else if (!strcmp(a, "aPause")) run(LJ_CL_MP_PAUSE, NULL, 0);
    else if (!strcmp(a, "aStop")) run(LJ_CL_MP_STOP, NULL, 0);
    else if (!strcmp(a, "aLoadMediaFile")) pick();
    else if (!strcmp(a, "aToggleControls")) { lj_widget *c = find("pControls"); show_controls(!(c && c->visible)); }
    else if (!strcmp(a, "aExit")) mf.pending_close = 1;
    else lj_log("media: %s: %s", a, LJ_NA_HINT);   /* the greyed video / playback-file items */
}
static void on_recent(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(mf.win);
    lj_media *m = med();
    char p[260];
    (void)w; (void)ud;
    if (!it || !it->name || !m) return;
    lj_str_copy(p, sizeof p, m->recent[(it->name[8] - '0') % LJ_MEDIA_RECENT]);
    if (p[0]) run(LJ_CL_MP_LOAD_FILE, p, 0);
}
static void on_size(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(mf.win);
    (void)w; (void)ud;
    if (it && it->action && !strcmp(it->action, "aMinimumSize")) size_to(1);
    else if (it && it->action && !strcmp(it->action, "aDefaultSize")) size_to(0);
}
static void on_pref(lj_widget *w, void *ud)   /* PrefsClick / StartUpMenuClick: Stay on top, Pass keypresses, Show Controls (forms.ini [LJMEDIAPLAYER]) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(mf.win);
    const char *key = NULL;
    (void)w; (void)ud;
    if (!it || !it->name || !fini()) return;
    if (!strcmp(it->name, "M_Stayontop")) key = "OnTop";               /* "requires a restart of the Mediaplayer": applied at the next open */
    else if (!strcmp(it->name, "M_Keypass")) key = "KeyPass";
    else if (!strcmp(it->name, "M_StartupShowControls")) key = "ShowControls";
    if (!key) return;
    lj_ini_seti(fini(), MF_SEC, key, !lj_ini_geti(fini(), MF_SEC, key, 1));
    lj_media_form_refresh(LJ_WIN_MEDIA_PLAYER);
}
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; mf.pending_close = 1; }
static void seek_at(lj_widget *w)   /* pg1: "drag the bar to the desired location" */
{
    const lj_mouse_ev *e = lj_window_last_event(mf.win);
    lj_media *m = med();
    int ax, ay;
    uint64_t len_ms;
    if (!e || !m || m->state == LJ_MEDIA_CLOSED || !m->rate || w->w <= 0) return;
    lj_widget_abs(w, &ax, &ay);
    len_ms = m->frames * 1000u / m->rate;
    run(LJ_CL_MP_SEEK, NULL, (uint32_t)(len_ms * (uint64_t)(e->x < ax ? 0 : e->x - ax > w->w ? w->w : e->x - ax) / (uint64_t)w->w));
}
static void on_pg_down(lj_widget *w, void *ud) { (void)ud; mf.seeking = 1; seek_at(w); }
static void on_pg_move(lj_widget *w, void *ud) { (void)ud; if (mf.seeking) seek_at(w); }
static void on_pg_up(lj_widget *w, void *ud) { (void)ud; if (mf.seeking) seek_at(w); mf.seeking = 0; }
static void on_menu_drop(lj_widget *w, void *ud) { (void)w; (void)ud; lj_media_form_refresh(LJ_WIN_MEDIA_PLAYER); }   /* MM_FileClick / MM_HelpClick: the drop-down opens on fresh state (recent list) */
static void on_about(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_ABOUT); }

static const lj_binding mf_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "ActionExecute", on_action, NULL }, { "RecentClick", on_recent, NULL }, { "CustomSizeExecute", on_size, NULL },
    { "PrefsClick", on_pref, NULL }, { "StartUpMenuClick", on_pref, NULL }, { "MenuToggle", lj_handler_noop, NULL } /* greyed: monitors */,
    { "PlaybackMenuExecute", lj_handler_noop, NULL }, { "APlaybackMonitorSelect", lj_handler_noop, NULL }, { "aGetPlaybackSettingsExecute", lj_handler_noop, NULL },   /* greyed: .mps */
    { "bScanMouseDown", lj_handler_noop, NULL }, { "bScanMouseUp", lj_handler_noop, NULL },   /* Enabled = False in the DFM (WMP enabled them per format) */
    { "pg1MouseDown", on_pg_down, NULL }, { "pg1MouseMove", on_pg_move, NULL }, { "pg1MouseUp", on_pg_up, NULL },
    { "pPlayerMouseDown", lj_handler_noop, NULL } /* LJ2: drags the captionless window; framed here */, { "MainPopPopup", lj_handler_noop, NULL },
    { "MM_FileClick", on_menu_drop, NULL }, { "MM_HelpClick", on_menu_drop, NULL }, { "M_HelpClick", lj_help_viewer_click, "cuelists" }, { "About1Click", on_about, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_media_form_open(enum lj_win id)
{
    lj_media *m = med();
    lj_menu *mm;
    int i;
    if (id != LJ_WIN_MEDIA_PLAYER) return NULL;
    if (mf.win) { lj_win_show(mf.win, 0); return mf.win; }
    if (m && fini()) for (i = 0; i < LJ_MEDIA_RECENT; i++) {   /* the five recent files persist in forms.ini ("File0".."File4", LJMediaPlayer.Ini [RecentMedia] names) */
        char k[8];
        const char *v;
        if (snprintf(k, sizeof k, "File%d", i) < 0) k[0] = 0;
        if (!m->recent[i][0] && (v = lj_ini_gets(fini(), MF_SEC, k, NULL)) != NULL) lj_str_copy(m->recent[i], sizeof m->recent[i], v);
    }
    for (i = 0; i < LJ_MEDIA_RECENT; i++) mf_recent[i].caption = mf.recent_cap[i];
    if ((mf.win = lj_window_create(&mf_form, mf_b)) == NULL) { lj_log("media: cannot create TLJMediaForm"); return NULL; }
    mf.minimum = 0; mf.pending_close = 0; mf.seeking = 0;
    {   /* Flat speed buttons show their parent's colour in the VCL (mediaplayer.gif: blue on the blue pControls) */
        lj_widget *pc = find("pControls"), *c;
        for (c = pc ? pc->first : NULL; c; c = c->next) if (c->kind == LJ_K_SPEEDBUTTON) c->color = pc->color;
    }
    mm = lj_window_menus(mf.win);
    for (i = 0; i < LJ_COUNT_OF(mf_greyed); i++) lj_menu_set_enabled(mm, mf_greyed[i], 0);
    show_controls(lj_ini_geti(fini(), MF_SEC, "ShowControls", 1));
    lj_media_form_refresh(id);
    lj_win_show(mf.win, lj_ini_geti(fini(), MF_SEC, "OnTop", 1) ? MF_ON_TOP : 0u);   /* LJMediaPlayer.Ini as shipped: OnTop=1, ShowControls=1, KeyPass=1 */
    return mf.win;
}

void lj_media_form_close(enum lj_win id)
{
    if (id != LJ_WIN_MEDIA_PLAYER || !mf.win) return;
    lj_win_hide(mf.win);
    lj_window_destroy(mf.win);
    mf.win = NULL; mf.pending_close = 0;
}

lj_window *lj_media_form_get(enum lj_win id) { return id == LJ_WIN_MEDIA_PLAYER ? mf.win : NULL; }

void lj_media_form_refresh(enum lj_win id)
{
    lj_media *m = med();
    lj_menu *mm;
    lj_widget *w;
    char tc[16], p[260];
    int i, st, r;
    if (id != LJ_WIN_MEDIA_PLAYER || !mf.win) return;
    if (mf.pending_close) { lj_media_form_close(id); return; }      /* Exit / the frame's close: never from inside a handler */
    st = m ? m->state : LJ_MEDIA_CLOSED;
    if (m && m->pick_owner == 2 && m->host && m->host->pick_take && (r = m->host->pick_take(m->host->ud, p, sizeof p)) != 0) {
        m->pick_owner = 0;
        if (r == 1) { run(LJ_CL_MP_LOAD_FILE, p, 0); return; }
    }
    set_enabled("sbPlay", st != LJ_MEDIA_CLOSED && st != LJ_MEDIA_PLAYING);
    set_enabled("sbPause", st == LJ_MEDIA_PLAYING);
    set_enabled("sbStop", st != LJ_MEDIA_CLOSED);
    if (lj_media_format_tc(tc, sizeof tc, m ? lj_media_time_cs(m) : 0) == 0 && (w = find("pTimeCode")) != NULL) { lj_w_set_caption(w, tc); lj_widget_invalidate(w); }
    if ((w = find("eMediaFile")) != NULL) lj_w_edit_set_text(w, m && st != LJ_MEDIA_CLOSED ? m->path : "");
    if ((w = find("pg1")) != NULL) {
        uint64_t len = m && m->rate ? m->frames * 100u / m->rate : 0;
        lj_w_progressbar_set(w, len ? (int)((uint64_t)lj_media_time_cs(m) * 1000u / len) : 0);
    }
    mm = lj_window_menus(mf.win);
    lj_menu_set_enabled(mm, "Play1", st != LJ_MEDIA_CLOSED && st != LJ_MEDIA_PLAYING);
    lj_menu_set_enabled(mm, "Pause1", st == LJ_MEDIA_PLAYING);
    lj_menu_set_checked(mm, "M_Stayontop", lj_ini_geti(fini(), MF_SEC, "OnTop", 1));
    lj_menu_set_checked(mm, "M_Keypass", lj_ini_geti(fini(), MF_SEC, "KeyPass", 1));
    lj_menu_set_checked(mm, "M_StartupShowControls", lj_ini_geti(fini(), MF_SEC, "ShowControls", 1));
    for (i = 0; i < LJ_MEDIA_RECENT; i++) {
        char k[8];
        const char *path = m ? m->recent[i] : "";
        if (snprintf(mf.recent_cap[i], sizeof mf.recent_cap[i], "%d %s", i + 1, path) < 0) mf.recent_cap[i][0] = 0;   /* PROPOSAL rig W5-MED-5: LJ2's recent-item text is not captured */
        lj_menu_set_visible(mm, mf_recent[i].name, path[0] != 0);
        if (snprintf(k, sizeof k, "File%d", i) < 0) k[0] = 0;
        if (fini() && path[0]) lj_ini_sets(fini(), MF_SEC, k, path);
    }
}

void lj_media_form_bind_fns(void) {}   /* LJ_ACT_MEDIA_PLAYER is bound in app/lj_app_io.c */
