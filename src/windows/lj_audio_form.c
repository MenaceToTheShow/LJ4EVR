/* lj_audio_form.c - the AUDIO family (windows/lj_audio_form.h): the Audio Analyzer window (LJSound.exe's TLJSoundApp,
 * 05 s9.1) over io/lj_audio.h, and the Hardware Setup tsAudio page.
 *
 * The form table below is dfm2c.py's output for the TPF0 resource TLJSOUNDAPP of LJSound.exe (2.209), pasted here
 * because the side exe is not in the 208-form harvest (W5-X-HARVEST: the integrator may move it into lj_forms_gen.c).
 * Facts only (geometry, captions and hints verbatim incl. "Aggresivness" and "Alway", the menu tree, event names);
 * Icon.Data and ImageList1's bitmap are dropped (no glyphmap row, D2). Edits to dfm2c's text, each because lj_window
 * cannot bind it (ME-46) or LJ2 painted from a timer: OnCanResize (Splitter1/2), OnDrawPanel (StatusBar1) and
 * OnCloseQuery (form) removed; OnPaint added to PaintBox1/2/3 (handler names ours).
 *
 * Behaviour (PROPOSAL rig W5-AUD-4 where the help is silent): the trackbars map to the analyzer settings as
 * TrackBar1 1..25 x 24 ms decay, TrackBar2 0..300 -> factor 10.0..2.0, TrackBar3 1..455 -> noise 0..255, ScrollBar1
 * -127..127 = -20..+20 dB; the Time-line graph spans 0..8192 level units (5.8 s), the Real-time view one block at full
 * scale; the threshold line drags in Time-line view; the equalizer curve drags in PaintBox3. Settings live in
 * ctx->audio and are written to Hardware.Ini [AUDIO] on close when Auto Save Changes is on (the DFM hint "Save
 * Equalizer and volume settings on exit"). One process: Pass Keypresses and Run Minimized are kept and persisted, keys
 * always reach LightJockey and the window opens from &Audio (docs/fidelity.md rows from the audio track). */
#include "windows/lj_audio_form.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_locale.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "io/lj_audio.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_dmx_in.h"
#include "windows/lj_help_viewer.h"

/* ---- TLJSoundApp: dfm2c.py output (see the header comment) ---- */
static const lj_ctl_desc tljsoundapp_panel1[] = {
    { LJ_K_PAINTBOX, "PaintBox1", 1, 1, 127, 201, .hint = "Level in|Use this window to verify sound level", .align = LJ_AL_CLIENT, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnMouseDown", "PaintBox1MouseDown" }, { "OnMouseMove", "PaintBox1MouseMove" }, { "OnMouseUp", "PaintBox1MouseUp" }, { "OnPaint", "PaintBox1Paint" } } },
    { LJ_K_LABEL, "Label1", 1, 202, 127, 13, .align = LJ_AL_BOTTOM, .caption = "0.00 BPM", .color = LJ_CLR(0x000000), .font_color = LJ_CLR(0xFF0000), .parent_color = LJ_NO, .parent_font = LJ_NO, .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11, .font_style = 0 },
    { LJ_K_LABEL, "Label2", 16, 24, 39, 13, .caption = "Label2", .font_color = LJ_CLR(0xFFFFFF), .parent_font = LJ_NO, .font = LJ_FONT_SANS13B, .font_name = "MS Sans Serif", .font_height = -11, .font_style = LJ_FS_BOLD },
    { LJ_K_LABEL, "Label3", 16, 40, 39, 13, .caption = "Label3", .font_color = LJ_CLR(0xFFFFFF), .parent_font = LJ_NO, .font = LJ_FONT_SANS13B, .font_name = "MS Sans Serif", .font_height = -11, .font_style = LJ_FS_BOLD },
};
static const lj_ctl_desc tljsoundapp_fftpanel_panel2[] = {
    { LJ_K_PAINTBOX, "PaintBox3", 0, 0, 149, 38, .align = LJ_AL_CLIENT, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnMouseDown", "PaintBox2MouseDown" }, { "OnMouseMove", "PaintBox3MouseMove" }, { "OnMouseUp", "PaintBox3MouseUp" }, { "OnPaint", "PaintBox3Paint" } } },
};
static const lj_ctl_desc tljsoundapp_fftpanel[] = {
    { LJ_K_PAINTBOX, "PaintBox2", 1, 1, 149, 174, .hint = "Spectrum analyzer|Spectrum analyzer", .align = LJ_AL_CLIENT, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnMouseMove", "PaintBox2MouseMove" }, { "OnPaint", "PaintBox2Paint" } } },
    { LJ_K_SPLITTER, "Splitter2", 1, 175, 149, 2, .align = LJ_AL_BOTTOM, .color = LJ_CLR(0x808080), .min_size = 1, .parent_color = LJ_NO, .resize_style = "rsLine", .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_PANEL, "Panel2", 1, 177, 149, 38, .align = LJ_AL_BOTTOM, .bevel_outer = LJ_BV_NONE, .color = LJ_CLR(0x000000), .tab_order = 0, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = tljsoundapp_fftpanel_panel2, .nchildren = LJ_COUNT_OF(tljsoundapp_fftpanel_panel2) },
};
static const lj_column_desc tljsoundapp_statusbar1_cols[] = {
    { .width = 30, .image_index = -1 },
    { .width = 30, .image_index = -1 },
    { .width = 200, .image_index = -1 },
};
static const lj_ctl_desc tljsoundapp_soundcardpanel[] = {
    { LJ_K_COMBOBOX, "ComboBox1", 0, 0, 301, 21, .style = "csDropDownList", .anchors = LJ_AK_LEFT | LJ_AK_RIGHT | LJ_AK_BOTTOM, .color = LJ_CLR(0x000000), .font_color = LJ_CLR(0x00FF00), .item_height = 13, .parent_font = LJ_NO, .tab_order = 0, .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11, .font_style = 0, .drop_down_count = 8, .item_index = -1, .events = { { "OnChange", "ComboBox1Change" } } },
};
static const lj_ctl_desc tljsoundapp_faderpanel[] = {
    { LJ_K_TRACKBAR, "TrackBar1", 0, 0, 81, 28, .hint = "Decay|Minimum time between trigs", .align = LJ_AL_LEFT, .max = 25, .min = 1, .orientation = "trHorizontal", .frequency = 2, .position = 13, .sel_end = 0, .sel_start = 0, .tab_order = 0, .tick_marks = "tmBottomRight", .tick_style = "tsAuto", .font = LJ_FONT_SANS13, .font_inherited = 1, .page_size = 2, .events = { { "OnChange", "TrackBar1Change" } } },
    { LJ_K_TRACKBAR, "TrackBar2", 138, 0, 159, 28, .hint = "Aggresivness|Trigger sensitivity", .align = LJ_AL_CLIENT, .max = 300, .orientation = "trHorizontal", .frequency = 10, .position = 150, .sel_end = 0, .sel_start = 0, .tab_order = 2, .tick_marks = "tmBottomRight", .tick_style = "tsAuto", .font = LJ_FONT_SANS13, .font_inherited = 1, .page_size = 2, .events = { { "OnChange", "TrackBar2Change" } } },
    { LJ_K_TRACKBAR, "TrackBar3", 81, 0, 57, 28, .hint = "Noise cut-off level|Minimum sound trig level:", .align = LJ_AL_LEFT, .max = 455, .min = 1, .orientation = "trHorizontal", .frequency = 10, .position = 25, .sel_end = 0, .sel_start = 0, .tab_order = 1, .tick_marks = "tmBottomRight", .tick_style = "tsAuto", .font = LJ_FONT_SANS13, .font_inherited = 1, .page_size = 2, .events = { { "OnChange", "TrackBar3Change" } } },
};
static const lj_ctl_desc tljsoundapp_ctls[] = {
    { LJ_K_SPLITTER, "Splitter1", 145, 0, 1, 216, .align = LJ_AL_RIGHT, .color = LJ_CLR(0xC0C0C0), .min_size = 1, .parent_color = LJ_NO, .resize_style = "rsLine", .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_PANEL, "Panel1", 16, 0, 129, 216, .align = LJ_AL_CLIENT, .bevel_outer = LJ_BV_LOWERED, .color = LJ_CLR(0x000000), .tab_order = 0, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = tljsoundapp_panel1, .nchildren = LJ_COUNT_OF(tljsoundapp_panel1), .events = { { "OnResize", "Panel1Resize" } } },
    { LJ_K_PANEL, "FFTPanel", 146, 0, 151, 216, .align = LJ_AL_RIGHT, .bevel_outer = LJ_BV_LOWERED, .color = LJ_CLR(0x000000), .tab_order = 1, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = tljsoundapp_fftpanel, .nchildren = LJ_COUNT_OF(tljsoundapp_fftpanel), .events = { { "OnResize", "FFTPanelResize" } } },
    { LJ_K_STATUSBAR, "StatusBar1", 0, 266, 297, 21, .font_color = LJ_CLR(0x000000), .columns = tljsoundapp_statusbar1_cols, .ncolumns = LJ_COUNT_OF(tljsoundapp_statusbar1_cols), .simple_panel = LJ_NO, .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11, .font_style = 0, .events = { { "OnMouseMove", "StatusBar1MouseMove" } } },
    { LJ_K_PANEL, "SoundCardPanel", 0, 244, 297, 22, .align = LJ_AL_BOTTOM, .bevel_outer = LJ_BV_NONE, .tab_order = 3, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = tljsoundapp_soundcardpanel, .nchildren = LJ_COUNT_OF(tljsoundapp_soundcardpanel) },
    { LJ_K_PANEL, "FaderPanel", 0, 216, 297, 28, .align = LJ_AL_BOTTOM, .bevel_outer = LJ_BV_NONE, .tab_order = 4, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = tljsoundapp_faderpanel, .nchildren = LJ_COUNT_OF(tljsoundapp_faderpanel) },
    { LJ_K_SCROLLBAR, "ScrollBar1", 0, 0, 16, 216, .hint = "Gain: 0,0dB|Global volume setting for the sound input: 0,0dB", .align = LJ_AL_LEFT, .kind_str = "sbVertical", .max = 127, .min = -127, .page_size = 0, .parent_show_hint = LJ_NO, .show_hint = LJ_YES, .tab_order = 5, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnChange", "ScrollBar1Change" } } },
    { LJ_K_MAINMENU, "MainMenu1", 216, 24, 0, 0, .images = "ImageList1", .owner_draw = LJ_YES, .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_TIMER, "VisualTimer", 216, 56, 0, 0, .interval = 100, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnTimer", "VisualTimerTimer" } } },
    { LJ_K_TIMER, "SharedMapTimer", 182, 56, 0, 0, .interval = 100, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnTimer", "SharedMapTimerTimer" } } },
    { LJ_K_IMAGELIST, "ImageList1", 182, 24, 0, 0, .picture = "missing", .font = LJ_FONT_SANS13, .font_inherited = 1 },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_analyzer1[] = {
    { .name = "AutoSaveChanges1", .tag = 1, .caption = "&Auto Save Changes", .hint = "|Save Equalizer and volume settings on exit", .image_index = 1, .on_click = "AutoSaveChanges1Click" },
    { .name = "PassKeypressestoLightJockey", .caption = "Pass Keypresses to LightJockey", .hint = "Pass Keypresses to LightJockey", .on_click = "PassKeypressestoLightJockeyClick", .image_index = -1 },
    { .name = "N1", .caption = "-", .image_index = -1 },
    { .name = "Exit1", .caption = "E&xit", .hint = "|Close Audio Analyzer", .image_index = 9, .on_click = "Exit1Click" },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_trigger1_setdefault1[] = {
    { .name = "Volume1", .caption = "&Gain", .hint = "|Set the gain fader to the middle (default) position", .image_index = 12, .on_click = "Volume1Click" },
    { .name = "Equalizer1", .caption = "&Equalizer", .hint = "|Reset all equalizer faders to the middle (default) value", .image_index = 11, .on_click = "Equalizer1Click" },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_trigger1_noiselevel1[] = {
    { .name = "Low1", .tag = 1, .caption = "&Low", .group_index = 2, .image_index = 18, .on_click = "Low1Click" },
    { .name = "Medium1", .caption = "&Medium", .group_index = 2, .image_index = 17, .on_click = "Medium1Click" },
    { .name = "High1", .caption = "&High", .group_index = 2, .image_index = 16, .on_click = "High1Click" },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_trigger1[] = {
    { .name = "UseFFT1", .caption = "Use &Equalizer", .hint = "Trig on custom frequencies|Enables equalizer and spectrum analyzer", .on_click = "UseFFT1Click", .image_index = -1 },
    { .name = "SetDefault1", .caption = "Set &Default", .hint = "|Reset to the default value", .image_index = -1, .children = tljsoundapp_mainmenu1_trigger1_setdefault1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_trigger1_setdefault1) },
    { .name = "Noiselevel1", .caption = "&Noise level", .image_index = -1, .children = tljsoundapp_mainmenu1_trigger1_noiselevel1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_trigger1_noiselevel1) },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_view1_showinput1[] = {
    { .name = "None1", .caption = "&None", .group_index = 1, .on_click = "None1Click", .image_index = -1 },
    { .name = "Realtime1", .tag = 1, .caption = "&Real time", .group_index = 1, .on_click = "Realtime1Click", .image_index = -1 },
    { .name = "Timeline1", .caption = "&Time-line", .group_index = 1, .on_click = "Timeline1Click", .image_index = -1 },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_view1[] = {
    { .name = "Runminimized1", .tag = 1, .caption = "&Run Minimized", .hint = "|Don't show Audio Analyzer on startup", .on_click = "Runminimized1Click", .image_index = -1 },
    { .name = "StayOnTop1", .tag = 1, .caption = "Stay On &Top", .hint = "|Alway put Audio Analyzer in front", .on_click = "StayOnTop1Click", .image_index = -1 },
    { .name = "Showinput1", .caption = "Show &input", .image_index = -1, .children = tljsoundapp_mainmenu1_view1_showinput1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_view1_showinput1) },
    { .name = "Triggerfaders1", .tag = 1, .caption = "Trigger &Faders", .on_click = "Triggerfaders1Click", .image_index = -1 },
    { .name = "SoundCard1", .tag = 1, .caption = "&Sound Card", .on_click = "SoundCard1Click", .image_index = -1 },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1_help1[] = {
    { .name = "Contents1", .caption = "&Help", .image_index = 5, .on_click = "Contents1Click" },
    { .name = "N2", .caption = "-", .image_index = -1 },
    { .name = "About1", .caption = "&About", .on_click = "About1Click", .image_index = -1 },
};
static const lj_menu_item_desc tljsoundapp_mainmenu1[] = {
    { .name = "Analyzer1", .caption = "&Analyzer", .on_click = "Analyzer1Click", .image_index = -1, .children = tljsoundapp_mainmenu1_analyzer1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_analyzer1) },
    { .name = "Trigger1", .caption = "&Trigger", .on_click = "Trigger1Click", .image_index = -1, .children = tljsoundapp_mainmenu1_trigger1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_trigger1) },
    { .name = "View1", .caption = "&View", .on_click = "View1Click", .image_index = -1, .children = tljsoundapp_mainmenu1_view1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_view1) },
    { .name = "Help1", .caption = "&Help", .hint = "Help topics", .on_click = "Help1Click", .image_index = -1, .children = tljsoundapp_mainmenu1_help1, .nchildren = LJ_COUNT_OF(tljsoundapp_mainmenu1_help1) },
};
static const lj_menu_desc tljsoundapp_menus[] = {
    { "MainMenu1", 1, tljsoundapp_mainmenu1, 4 },
};
const lj_form_desc lj_form_TLJSOUNDAPP = {
    .name = "LJSoundApp", .class_name = "TLJSoundApp",
    .left = 450,
    .top = 185,
    .width = 305,
    .height = 333,
    .horz_scrollbar_visible = LJ_NO,
    .vert_scrollbar_visible = LJ_NO,
    .caption = "Martin Audio Analyzer",
    .color = LJ_CLR(0x000000),
    .font_color = LJ_CLR(0x000000),
    .style = LJ_FS_STAYONTOP,
    .icon = "missing",
    .menu = "MainMenu1",
    .show_hint = LJ_YES,
    .text_height = 13,
    .font = LJ_FONT_SANS13,
    .font_name = "MS Sans Serif",
    .font_height = -11,
    .font_style = 0,
    .client_width = 297,
    .client_height = 287,
    .client_derived = 1,
    .ctls = tljsoundapp_ctls,
    .n = LJ_COUNT_OF(tljsoundapp_ctls),
    .menus = tljsoundapp_menus,
    .nmenus = LJ_COUNT_OF(tljsoundapp_menus),
    .events = { { "OnClose", "FormClose" }, { "OnCreate", "FormCreate" } }
};

/* ---- state ---- */
enum { AU_DEVS = 16, AU_TL_FULL = 8192 /* level units at the top of the Time-line graph, PROPOSAL rig W5-AUD-4 */, AU_LIT_US = 150000 };
static struct {
    lj_window *win;
    lj_widget *pb1, *pb2, *pb3, *bpm, *combo, *tb1, *tb2, *tb3, *gain, *sb;
    lj_audio_dev devs[AU_DEVS]; const char *items[AU_DEVS + 1]; int ndev;
    char bpm_text[40], status[112], gain_hint[128];
    uint32_t seen_trigs; uint64_t lit_us, last_us, period_us;
    int drag;                                          /* 1 threshold line, 2 equalizer */
    const lj_widget_vt *sb_vt, *cb_vt; lj_widget_vt sb_own, cb_own;
    const lj_ctl_desc *bpm_desc; lj_ctl_desc bpm_lime;  /* Label1: clRed in the DFM, green once a tempo is measured (audiotimeline.gif, audiofft.gif, audiorealtime1.gif) */
    lj_widget *hs_rg, *hs_combo; lj_audio_dev hs_devs[AU_DEVS]; const char *hs_items[AU_DEVS + 1]; int hs_ndev;
} au;
static lj_audio_cfg au_local;                          /* used while ctx->audio is NULL (tests before the app seam) */
static lj_audio_state au_local_state;
static const char au_default_dev[] = "Default device";   /* PROPOSAL rig W5-AUD-1: LJ2 listed "Microsoft Sound Mapper" (05 s9.1) */

static lj_audio_cfg *cfg(void) { lj_ctx *c = lj_ctx_get(); return c->audio ? c->audio : &au_local; }
static lj_audio_state *st(void) { lj_ctx *c = lj_ctx_get(); return c->audio_state ? c->audio_state : &au_local_state; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int lj_audio_form_trig_lit(uint64_t now_us)
{
    const lj_audio_state *s = st();
    if (s->trigs != au.seen_trigs) {
        if (au.last_us && now_us > au.last_us) {        /* the BPM readout: a smoothed trig period between 0.15 and 3 s */
            uint64_t p = now_us - au.last_us;
            if (p >= 150000u && p <= 3000000u) au.period_us = au.period_us ? (au.period_us * 3u + p) / 4u : p;
        }
        au.seen_trigs = s->trigs; au.lit_us = au.last_us = now_us;
    }
    return au.lit_us && now_us - au.lit_us < AU_LIT_US;
}

static int devices(lj_audio_dev *devs, const char **items)
{
    lj_ctx *c = lj_ctx_get();
    int n = c->audio_list ? c->audio_list(devs, AU_DEVS) : 0, i;
    if (n < 0) n = 0;
    items[0] = au_default_dev;
    for (i = 0; i < n; i++) items[i + 1] = devs[i].name;
    return n;
}
static int device_index(const lj_audio_dev *devs, int n)
{
    int i;
    for (i = 0; cfg()->device[0] && i < n; i++) if (strcmp(devs[i].name, cfg()->device) == 0) return i + 1;
    return 0;
}

/* ---- the view: menus, faders, panels from cfg() ---- */
static void gain_hint(void)
{
    int pos = lj_audio_gain_pos(cfg()), t = pos * 200 / 127, a = t < 0 ? -t : t;   /* tenths of a dB */
    char v[32];
    snprintf(v, sizeof v, "%s%d%c%ddB", t < 0 ? "-" : "", a / 10, lj_locale_decimal(), a % 10);
    snprintf(au.gain_hint, sizeof au.gain_hint, "Gain: %s|Global volume setting for the sound input: %s", v, v);   /* the DFM hint's format, "Gain: 0,0dB|..." */
    if (au.gain) au.gain->hint = au.gain_hint;
}
static void sync(void)
{
    lj_menu *m = au.win ? lj_window_menus(au.win) : NULL;
    const lj_audio_cfg *c = cfg();
    lj_widget *w;
    if (!au.win) return;
    if (m) {
        lj_menu_set_checked(m, "AutoSaveChanges1", c->auto_save);
        lj_menu_set_checked(m, "PassKeypressestoLightJockey", c->pass_keys);
        lj_menu_set_checked(m, "UseFFT1", c->use_eq);
        lj_menu_set_checked(m, "Low1", c->noise_range == LJ_AUDIO_NOISE_LOW);
        lj_menu_set_checked(m, "Medium1", c->noise_range == LJ_AUDIO_NOISE_MEDIUM);
        lj_menu_set_checked(m, "High1", c->noise_range == LJ_AUDIO_NOISE_HIGH);
        lj_menu_set_checked(m, "Runminimized1", c->run_minimized);
        lj_menu_set_checked(m, "StayOnTop1", c->on_top);
        lj_menu_set_checked(m, "None1", c->view == LJ_AUDIO_VIEW_NONE);
        lj_menu_set_checked(m, "Realtime1", c->view == LJ_AUDIO_VIEW_REALTIME);
        lj_menu_set_checked(m, "Timeline1", c->view == LJ_AUDIO_VIEW_TIMELINE);
        lj_menu_set_checked(m, "Triggerfaders1", c->trigger_faders);
        lj_menu_set_checked(m, "SoundCard1", c->sound_card);
    }
    if ((w = lj_window_find(au.win, "FFTPanel")) != NULL) w->visible = c->use_eq;   /* "Enables equalizer and spectrum analyzer" */
    if ((w = lj_window_find(au.win, "Splitter1")) != NULL) w->visible = c->use_eq;
    if ((w = lj_window_find(au.win, "FaderPanel")) != NULL) w->visible = c->trigger_faders;
    if ((w = lj_window_find(au.win, "SoundCardPanel")) != NULL) w->visible = c->sound_card;
    lj_w_trackbar_set(au.tb1, clampi(c->decay_ms / 24, 1, 25));
    lj_w_trackbar_set(au.tb2, clampi((LJ_AUDIO_SENS_MAX - c->sensitivity_x10) * 300 / (LJ_AUDIO_SENS_MAX - LJ_AUDIO_SENS_MIN), 0, 300));
    lj_w_trackbar_set(au.tb3, clampi(c->noise * 455 / 255, 1, 455));
    lj_w_scrollbar_set(au.gain, lj_audio_gain_pos(c));
    gain_hint();
    lj_widget_layout(lj_window_root(au.win));
    lj_window_invalidate_all(au.win);
}

/* ---- painting ---- */
static void paint_level(lj_widget *w, void *ud)
{
    lj_surface *s = au.win ? lj_window_surface(au.win) : NULL;
    const lj_audio_state *a = st();
    const lj_audio_cfg *c = cfg();
    int x, y, i, h = w->h, n = w->w;
    (void)ud;
    if (!s || n < 2 || h < 2) return;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, n, h, 0x000000u);
    if (c->view == LJ_AUDIO_VIEW_REALTIME) {          /* one block, a vertical line per column; yellow near clip, red clipped (05 s9.1) */
        for (i = 0; i < n; i++) {
            int v = a->wave[i * LJ_AUDIO_BLOCK / n], av = v < 0 ? -v : v, len = av * (h / 2) / 32768;
            uint32_t rgb = av >= 32767 ? 0xFF0000u : av >= 29491 ? 0xFFFF00u : 0x00FF00u;
            if (v >= 0) lj_vline(s, x + i, y + h / 2 - len, len + 1, rgb); else lj_vline(s, x + i, y + h / 2, len + 1, rgb);
        }
    } else if (c->view == LJ_AUDIO_VIEW_TIMELINE) {   /* the last 5.8 s: level line green, trigs yellow, threshold blue (audiotimeline.gif) */
        int prev = -1, ty = h - 1 - clampi(lj_audio_threshold(c), 0, AU_TL_FULL) * (h - 1) / AU_TL_FULL;
        lj_hline(s, x, y + ty, n, 0x0000FFu);
        for (i = 0; i < n; i++) {                       /* each column: the loudest of its blocks, a trig when any of them trigged */
            int k = i * LJ_AUDIO_TIMELINE / n, k1 = (i + 1) * LJ_AUDIO_TIMELINE / n, lv = 0, tr = 0, py;
            do {
                uint16_t e = a->timeline[(a->tl_pos + k) % LJ_AUDIO_TIMELINE];
                if ((e & 0x7FFF) > lv) lv = e & 0x7FFF;
                tr |= (e & 0x8000u) != 0;
            } while (++k < k1);
            py = h - 1 - clampi(lv, 0, AU_TL_FULL) * (h - 1) / AU_TL_FULL;
            if (prev >= 0) lj_vline(s, x + i, y + (prev < py ? prev : py), (prev < py ? py - prev : prev - py) + 1, 0x00FF00u);
            else lj_fill(s, x + i, y + py, 1, 1, 0x00FF00u);
            if (tr) lj_vline(s, x + i, y + py, h - py, 0xFFFF00u);   /* "Trigs are shown on the graph as yellow lines" (CHM usingaudioin) */
            prev = py;
        }
    }
}
static void paint_spectrum(lj_widget *w, void *ud)   /* audiofft.gif: green bars, one per band */
{
    lj_surface *s = au.win ? lj_window_surface(au.win) : NULL;
    const lj_audio_state *a = st();
    int x, y, b;
    (void)ud;
    if (!s || w->w < LJ_AUDIO_BANDS || w->h < 2) return;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, 0x000000u);
    for (b = 0; b < LJ_AUDIO_BANDS; b++) {
        int bx = b * w->w / LJ_AUDIO_BANDS, bw = (b + 1) * w->w / LJ_AUDIO_BANDS - bx - 1, bh = clampi(a->band[b], 0, AU_TL_FULL) * w->h / AU_TL_FULL;
        if (bh) lj_fill(s, x + bx, y + w->h - bh, bw > 0 ? bw : 1, bh, 0x00C000u);
    }
}
static void paint_eq(lj_widget *w, void *ud)         /* audiofft.gif: the yellow equalizer curve */
{
    lj_surface *s = au.win ? lj_window_surface(au.win) : NULL;
    const lj_audio_cfg *c = cfg();
    int x, y, b, prev = -1;
    (void)ud;
    if (!s || w->w < LJ_AUDIO_BANDS || w->h < 2) return;
    lj_widget_abs(w, &x, &y);
    lj_fill(s, x, y, w->w, w->h, 0x000000u);
    for (b = 0; b < LJ_AUDIO_BANDS; b++) {
        int bx = b * w->w / LJ_AUDIO_BANDS, bw = (b + 1) * w->w / LJ_AUDIO_BANDS - bx, py = (w->h - 2) - c->eq[b] * (w->h - 2) / 255;
        lj_fill(s, x + bx, y + py, bw, 2, 0xFFFF00u);
        if (prev >= 0 && prev != py) lj_vline(s, x + bx, y + (prev < py ? prev : py), (prev < py ? py - prev : prev - py) + 2, 0xFFFF00u);
        prev = py;
    }
}
static void sb_paint(lj_widget *w, lj_surface *s)   /* StatusBar1: panels 1 and 2 are psOwnerDraw: clip and Audio Trig (audiotimeline.gif) */
{
    const lj_audio_state *a = st();
    int ax, ay, ph = w->h - 3;
    au.sb_vt->paint(w, s);
    lj_widget_abs(w, &ax, &ay);
    if (a->clip) lj_fill(s, ax + 1, ay + 3, 28, ph - 1, a->clip == 2 ? 0xFF0000u : 0xFFFF00u);
    if (lj_audio_form_trig_lit(lj_clock_us())) lj_fill(s, ax + 33, ay + 3, 28, ph - 1, 0x0000FFu);
}

static void cb_paint(lj_widget *w, lj_surface *s)   /* ComboBox1: Font.Color clLime on clBlack (DFM; audiofft.gif "Microsoft Sound Mapper"); the widget inks black */
{
    const lj_w_combobox *c = (const lj_w_combobox *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay;
    au.cb_vt->paint(w, s);
    if (c->index < 0 || c->index >= c->n || !f || w->w < 24) return;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax + 2, ay + 2, w->w - 21, w->h - 4, 0x000000u);
    lj_w_text(s, w, ax + 4, ay + (w->h - f->height) / 2, c->items[c->index], 0x00FF00u);
}

/* ---- mouse: the threshold line and the equalizer curve ---- */
static int rel(lj_widget *w, int *rx, int *ry)
{
    const lj_mouse_ev *m = au.win ? lj_window_last_event(au.win) : NULL;
    int x, y;
    if (!m) return -1;
    lj_widget_abs(w, &x, &y);
    *rx = m->x - x; *ry = m->y - y;
    return 0;
}
static void set_noise_y(lj_widget *w, int ry)
{
    static const int mul[3] = { 2, 8, 32 };           /* io/lj_audio.c lj_audio_threshold */
    lj_audio_cfg *c = cfg();
    int level = (w->h - 1 - clampi(ry, 0, w->h - 1)) * AU_TL_FULL / (w->h > 1 ? w->h - 1 : 1);
    lj_audio_set_noise(c, level / mul[c->noise_range < 3 ? c->noise_range : 1]);
    lj_w_trackbar_set(au.tb3, clampi(c->noise * 455 / 255, 1, 455));
    lj_widget_invalidate(w);
}
static void set_eq_xy(lj_widget *w, int rx, int ry)
{
    int b = clampi(rx * LJ_AUDIO_BANDS / (w->w > 0 ? w->w : 1), 0, LJ_AUDIO_BANDS - 1);
    cfg()->eq[b] = (uint8_t)clampi((w->h - 2 - ry) * 255 / (w->h > 2 ? w->h - 2 : 1), 0, 255);
    lj_widget_invalidate(w);
}
static void pb1_down(lj_widget *w, void *ud) { int rx, ry; (void)ud; if (cfg()->view == LJ_AUDIO_VIEW_TIMELINE && rel(w, &rx, &ry) == 0) { au.drag = 1; set_noise_y(w, ry); } }
static void pb1_move(lj_widget *w, void *ud) { int rx, ry; (void)ud; if (au.drag == 1 && rel(w, &rx, &ry) == 0) set_noise_y(w, ry); }
static void pb_up(lj_widget *w, void *ud) { (void)w; (void)ud; au.drag = 0; }
static void pb3_down(lj_widget *w, void *ud) { int rx, ry; (void)ud; if (rel(w, &rx, &ry) == 0) { au.drag = 2; set_eq_xy(w, rx, ry); } }
static void pb3_move(lj_widget *w, void *ud) { int rx, ry; (void)ud; if (au.drag == 2 && rel(w, &rx, &ry) == 0) set_eq_xy(w, rx, ry); }

/* ---- controls ---- */
static void on_decay(lj_widget *w, void *ud) { (void)ud; cfg()->decay_ms = (uint16_t)clampi(((lj_w_trackbar *)w)->pos * 24, LJ_AUDIO_DECAY_MIN, LJ_AUDIO_DECAY_MAX); }
static void on_aggr(lj_widget *w, void *ud) { (void)ud; cfg()->sensitivity_x10 = (uint8_t)(LJ_AUDIO_SENS_MAX - clampi(((lj_w_trackbar *)w)->pos, 0, 300) * (LJ_AUDIO_SENS_MAX - LJ_AUDIO_SENS_MIN) / 300); }
static void on_noise(lj_widget *w, void *ud) { (void)ud; lj_audio_set_noise(cfg(), ((lj_w_trackbar *)w)->pos * 255 / 455); if (au.pb1) lj_widget_invalidate(au.pb1); }
static void on_gain(lj_widget *w, void *ud) { (void)ud; lj_audio_set_gain_pos(cfg(), ((lj_w_scrollbar *)w)->sb.pos); gain_hint(); }
static void on_device(lj_widget *w, void *ud)
{
    int i = ((lj_w_combobox *)w)->index;
    (void)ud;
    if (i < 0 || i > au.ndev) return;
    lj_str_copy(cfg()->device, sizeof cfg()->device, i ? au.devs[i - 1].name : "");
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_AUDIO);   /* saves Hardware.Ini and re-opens the capture on the new device */
}
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_audio_form_refresh(LJ_WIN_AUDIO_ANALYZER); }

static void save_settings(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!c->hardware_ini) return;
    lj_audio_cfg_save(c->hardware_ini, cfg());
    if (snprintf(path, sizeof path, "%sHardware.Ini", c->userdir) < (int)sizeof path && lj_ini_save(c->hardware_ini, path) != 0) lj_log("audio: cannot write %s", path);
}
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_audio_form_close(LJ_WIN_AUDIO_ANALYZER); }
static void on_sync(lj_widget *w, void *ud) { (void)w; (void)ud; sync(); }
static void on_menu(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = au.win ? lj_window_menu_item(au.win) : NULL;
    lj_audio_cfg *c = cfg();
    const char *n = it && it->name ? it->name : "";
    int i;
    (void)w; (void)ud;
    if (!strcmp(n, "AutoSaveChanges1")) c->auto_save = (uint8_t)!c->auto_save;
    else if (!strcmp(n, "PassKeypressestoLightJockey")) c->pass_keys = (uint8_t)!c->pass_keys;
    else if (!strcmp(n, "UseFFT1")) c->use_eq = (uint8_t)!c->use_eq;
    else if (!strcmp(n, "Volume1")) c->gain_pct = 100;                                   /* Set Default > Gain: the middle position */
    else if (!strcmp(n, "Equalizer1")) for (i = 0; i < LJ_AUDIO_BANDS; i++) c->eq[i] = 128;
    else if (!strcmp(n, "Low1")) c->noise_range = LJ_AUDIO_NOISE_LOW;
    else if (!strcmp(n, "Medium1")) c->noise_range = LJ_AUDIO_NOISE_MEDIUM;
    else if (!strcmp(n, "High1")) c->noise_range = LJ_AUDIO_NOISE_HIGH;
    else if (!strcmp(n, "Runminimized1")) c->run_minimized = (uint8_t)!c->run_minimized;
    else if (!strcmp(n, "StayOnTop1")) c->on_top = (uint8_t)!c->on_top;                  /* applies from the next open */
    else if (!strcmp(n, "None1")) c->view = LJ_AUDIO_VIEW_NONE;
    else if (!strcmp(n, "Realtime1")) c->view = LJ_AUDIO_VIEW_REALTIME;
    else if (!strcmp(n, "Timeline1")) c->view = LJ_AUDIO_VIEW_TIMELINE;
    else if (!strcmp(n, "Triggerfaders1")) c->trigger_faders = (uint8_t)!c->trigger_faders;
    else if (!strcmp(n, "SoundCard1")) c->sound_card = (uint8_t)!c->sound_card;
    else if (!strcmp(n, "About1")) { lj_win_open(LJ_WIN_ABOUT); return; }               /* LJSound.exe's own TAboutForm: ours shows the one About */
    else return;
    sync();
}

static const lj_binding lj_au_b[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "Panel1Resize", lj_handler_noop, NULL }, { "FFTPanelResize", lj_handler_noop, NULL },
    { "PaintBox1MouseDown", pb1_down, NULL }, { "PaintBox1MouseMove", pb1_move, NULL }, { "PaintBox1MouseUp", pb_up, NULL }, { "PaintBox1Paint", paint_level, NULL },
    { "PaintBox2MouseMove", lj_handler_noop, NULL }, { "PaintBox2Paint", paint_spectrum, NULL },
    { "PaintBox2MouseDown", pb3_down, NULL }, { "PaintBox3MouseMove", pb3_move, NULL }, { "PaintBox3MouseUp", pb_up, NULL }, { "PaintBox3Paint", paint_eq, NULL },
    { "StatusBar1MouseMove", lj_handler_noop, NULL }, { "ComboBox1Change", on_device, NULL },
    { "TrackBar1Change", on_decay, NULL }, { "TrackBar2Change", on_aggr, NULL }, { "TrackBar3Change", on_noise, NULL }, { "ScrollBar1Change", on_gain, NULL },
    { "VisualTimerTimer", on_timer, NULL }, { "SharedMapTimerTimer", lj_handler_noop, NULL },   /* LJ2's shared memory with LightJockey: one process here */
    { "Analyzer1Click", on_sync, NULL }, { "Trigger1Click", on_sync, NULL }, { "View1Click", on_sync, NULL }, { "Help1Click", on_sync, NULL },
    { "AutoSaveChanges1Click", on_menu, NULL }, { "PassKeypressestoLightJockeyClick", on_menu, NULL }, { "Exit1Click", on_close, NULL },
    { "UseFFT1Click", on_menu, NULL }, { "Volume1Click", on_menu, NULL }, { "Equalizer1Click", on_menu, NULL },
    { "Low1Click", on_menu, NULL }, { "Medium1Click", on_menu, NULL }, { "High1Click", on_menu, NULL },
    { "Runminimized1Click", on_menu, NULL }, { "StayOnTop1Click", on_menu, NULL }, { "None1Click", on_menu, NULL }, { "Realtime1Click", on_menu, NULL },
    { "Timeline1Click", on_menu, NULL }, { "Triggerfaders1Click", on_menu, NULL }, { "SoundCard1Click", on_menu, NULL },
    { "Contents1Click", lj_help_viewer_click, NULL }, { "About1Click", on_menu, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_audio_form_open(enum lj_win id)
{
    lj_widget *w;
    if (id != LJ_WIN_AUDIO_ANALYZER) return NULL;
    if (au.win) return au.win;
    if (!lj_ctx_get()->audio && !au_local.decay_ms) lj_audio_cfg_default(&au_local);
    au.win = lj_window_create(&lj_form_TLJSOUNDAPP, lj_au_b);
    if (!au.win) { lj_log("audio: cannot create TLJSoundApp"); return NULL; }
    au.pb1 = lj_window_find(au.win, "PaintBox1"); au.pb2 = lj_window_find(au.win, "PaintBox2"); au.pb3 = lj_window_find(au.win, "PaintBox3");
    au.bpm = lj_window_find(au.win, "Label1"); au.combo = lj_window_find(au.win, "ComboBox1");
    au.tb1 = lj_window_find(au.win, "TrackBar1"); au.tb2 = lj_window_find(au.win, "TrackBar2"); au.tb3 = lj_window_find(au.win, "TrackBar3");
    au.gain = lj_window_find(au.win, "ScrollBar1"); au.sb = lj_window_find(au.win, "StatusBar1");
    if ((w = lj_window_find(au.win, "Label2")) != NULL) w->visible = 0;   /* design-time "Label2" / "Label3": no screenshot shows them */
    if ((w = lj_window_find(au.win, "Label3")) != NULL) w->visible = 0;
    if (au.sb && au.sb->vt) { au.sb_vt = au.sb->vt; au.sb_own = *au.sb_vt; au.sb_own.paint = sb_paint; au.sb->vt = &au.sb_own; }
    if (au.combo && au.combo->vt) { au.cb_vt = au.combo->vt; au.cb_own = *au.cb_vt; au.cb_own.paint = cb_paint; au.combo->vt = &au.cb_own; }
    if (au.bpm && au.bpm->desc) { au.bpm_desc = au.bpm->desc; au.bpm_lime = *au.bpm_desc; au.bpm_lime.font_color = LJ_CLR(0x00FF00); }
    au.ndev = devices(au.devs, au.items);
    lj_w_combobox_set_items(au.combo, au.items, au.ndev + 1);
    if (au.combo) ((lj_w_combobox *)au.combo)->index = device_index(au.devs, au.ndev);
    au.seen_trigs = st()->trigs; au.lit_us = 0;
    sync();
    lj_audio_form_refresh(id);
    lj_win_show(au.win, cfg()->on_top ? 2u /* LJ_WF_STAY_ON_TOP: fsStayOnTop in the DFM */ : 0u);
    return au.win;
}

void lj_audio_form_close(enum lj_win id)
{
    if (id != LJ_WIN_AUDIO_ANALYZER || !au.win) return;
    if (cfg()->auto_save) save_settings();
    lj_win_hide(au.win);
    lj_window_destroy(au.win);
    au.win = NULL; au.pb1 = au.pb2 = au.pb3 = au.bpm = au.combo = au.tb1 = au.tb2 = au.tb3 = au.gain = au.sb = NULL; au.drag = 0;
}

lj_window *lj_audio_form_get(enum lj_win id) { return id == LJ_WIN_AUDIO_ANALYZER ? au.win : NULL; }

void lj_audio_form_refresh(enum lj_win id)
{
    int ok = 0;
    const char *why;
    uint32_t bpm;
    if (id != LJ_WIN_AUDIO_ANALYZER || !au.win) return;
    lj_audio_form_trig_lit(lj_clock_us());
    bpm = au.period_us ? (uint32_t)(6000000000ull / au.period_us) : 0;   /* hundredths of a beat per minute */
    snprintf(au.bpm_text, sizeof au.bpm_text, "%u%c%02u BPM", (unsigned)(bpm / 100), lj_locale_decimal(), (unsigned)(bpm % 100));
    lj_w_set_caption(au.bpm, au.bpm_text);
    if (au.bpm && au.bpm_desc) au.bpm->desc = bpm ? &au.bpm_lime : au.bpm_desc;   /* PROPOSAL rig W5-AUD-4: red until two trigs give a tempo */
    why = lj_audio_device_status(&ok);
    if (!cfg()->enabled) snprintf(au.status, sizeof au.status, "Audio-in disabled (Setup > Hardware Setup > Audio)");
    else if (!ok && why[0]) lj_str_copy(au.status, sizeof au.status, why);
    else snprintf(au.status, sizeof au.status, "Input: %s", cfg()->device[0] ? cfg()->device : au_default_dev);   /* "Current soundcard|Input: " (exe) */
    lj_w_statusbar_set_text(au.sb, 2, au.status);
    if (au.pb1) lj_widget_invalidate(au.pb1);
    if (au.pb2 && cfg()->use_eq) lj_widget_invalidate(au.pb2);
    if (au.sb) lj_widget_invalidate(au.sb);
}

void lj_audio_form_bind_fns(void) {}

/* ---- Hardware Setup, tsAudio ---- */
void lj_audio_form_hs_build(lj_window *hs)
{
    static lj_ctl_desc d;
    lj_widget *page = hs ? lj_window_find(hs, "tsAudio") : NULL, *w;
    if (!page) return;
    au.hs_rg = lj_window_find(hs, "RGAudio");
    lj_w_radiogroup_set_index(au.hs_rg, cfg()->enabled ? 1 : 0);
    memset(&d, 0, sizeof d);   /* code-built under the Winamp row: PROPOSAL rig W5-AUD-1 (LJ2 chose the device in the analyzer) */
    d.kind = LJ_K_COMBOBOX; d.name = "cbAudioDevice"; d.left = 4; d.top = 176; d.width = 395; d.height = 21; d.style = "csDropDownList";
    d.item_index = -1; d.drop_down_count = 8; d.item_height = 13; d.font_inherited = 1; d.hint = "Audio Analyzer input device";
    au.hs_combo = lj_w_combobox_create(page, &d);
    au.hs_ndev = devices(au.hs_devs, au.hs_items);
    lj_w_combobox_set_items(au.hs_combo, au.hs_items, au.hs_ndev + 1);
    if (au.hs_combo) ((lj_w_combobox *)au.hs_combo)->index = device_index(au.hs_devs, au.hs_ndev);
    for (w = page->first; w; w = w->next)                /* CD-Audio and the Winamp player stay dropped (I5, ME-11) */
        if (w->name && (!strcmp(w->name, "rgAudioCD") || !strcmp(w->name, "rgDigitalAudio") || !strcmp(w->name, "eWinamp") || !strcmp(w->name, "sbWinamp")))
            lj_dmx_in_grey(w, NULL, 0);                  /* the one greying helper and its hint (W4 I13) */
}

void lj_audio_form_hs_apply(void)
{
    lj_audio_cfg *c = cfg();
    int en, i;
    const char *dev;
    if (!au.hs_rg) return;
    en = lj_w_radiogroup_index(au.hs_rg) == 1; i = au.hs_combo ? ((lj_w_combobox *)au.hs_combo)->index : 0;
    dev = i > 0 && i <= au.hs_ndev ? au.hs_devs[i - 1].name : "";
    if (en == c->enabled && strcmp(dev, c->device) == 0) return;
    c->enabled = (uint8_t)en;
    lj_str_copy(c->device, sizeof c->device, dev);
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_AUDIO);   /* saves Hardware.Ini [AUDIO], opens / closes the capture */
    if (au.win) lj_audio_form_refresh(LJ_WIN_AUDIO_ANALYZER);
}

void lj_audio_form_hs_closed(void) { au.hs_rg = au.hs_combo = NULL; }
