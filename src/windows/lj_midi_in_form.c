/* lj_midi_in_form.c - see lj_midi_in_form.h. The form is built here (no DFM); its rows bind by the
 * handler names given below, the same lj_window_create path as a generated form (ME-16). Sizes:
 * midiin.gif 300x277 and midiinseq.gif 240x222 are reduced CHM captures, so the client 370x240
 * and every rect are PROPOSAL rig items (ME-67: nothing scaled from a 0.8 capture). */
#include "windows/lj_midi_in_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_delete_files.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

enum { LJ_MI_KINDS = 5, LJ_MI_NOTES = 128, LJ_MI_LOG = 64, LJ_MI_DEVS = 16 };

/* ---- the code-built form (PROPOSAL rig: midiin.gif / midiinseq.gif, reduced) ---- */
#define LJ_MI_F .font = LJ_FONT_SANS13, .font_inherited = 1
static const char *const lj_mi_tabs[] = { "Setup", "Sequences", "Cues", "BGCues", "CueLists", "Functions", "MIDI Log" };   /* midiin.gif tab captions */
static const lj_column_desc lj_mi_cols[LJ_MI_KINDS][3] = {
    { { "Note Value", 62, LJ_TA_UNSET, -1, 0, 0 }, { "Seq#", 40, LJ_TA_UNSET, -1, 0, 0 }, { "Seq Name", 0, LJ_TA_UNSET, -1, LJ_YES, 0 } },   /* midiinseq.gif */
    { { "Note Value", 62, LJ_TA_UNSET, -1, 0, 0 }, { "Cue#", 40, LJ_TA_UNSET, -1, 0, 0 }, { "Cue Name", 0, LJ_TA_UNSET, -1, LJ_YES, 0 } },
    { { "Note Value", 62, LJ_TA_UNSET, -1, 0, 0 }, { "BGCue#", 46, LJ_TA_UNSET, -1, 0, 0 }, { "BGCue Name", 0, LJ_TA_UNSET, -1, LJ_YES, 0 } },
    { { "Note Value", 62, LJ_TA_UNSET, -1, 0, 0 }, { "CL#", 40, LJ_TA_UNSET, -1, 0, 0 }, { "Cuelist Name", 0, LJ_TA_UNSET, -1, LJ_YES, 0 } },
    { { "Note Value", 62, LJ_TA_UNSET, -1, 0, 0 }, { "Function", 0, LJ_TA_UNSET, -1, LJ_YES, 0 }, { "", 0, LJ_TA_UNSET, -1, 0, 0 } },
};
#define LJ_MI_LV(n, k, nc) { LJ_K_LISTVIEW, n, 6, 48, 358, 120, .view_style = "vsReport", .row_select = LJ_YES, .hide_selection = LJ_NO, .visible = LJ_NO, \
    .columns = lj_mi_cols[k], .ncolumns = nc, LJ_MI_F, .events = { { "OnChange", "lvMapChange" } } }
static const lj_ctl_desc lj_mi_ctls[] = {
    { LJ_K_TABCONTROL, "TC1", 0, 0, 370, 204, .items = lj_mi_tabs, .nitems = LJ_COUNT_OF(lj_mi_tabs), LJ_MI_F, .events = { { "OnChange", "TC1Change" } } },
    /* Setup */
    { LJ_K_LABEL, "lDevice", 8, 32, 110, 13, .caption = "Select MIDI in Device", LJ_MI_F },
    { LJ_K_COMBOBOX, "cbDevice", 118, 28, 246, 21, .style = "csDropDownList", LJ_MI_F, .events = { { "OnChange", "cbDeviceChange" } } },
    { LJ_K_CHECKBOX, "cbNote0", 12, 58, 220, 17, .caption = "Treat Velocity = 0 as note off", LJ_MI_F, .events = { { "OnClick", "cbSetupClick" } } },
    { LJ_K_CHECKBOX, "cbSeqOff", 12, 74, 220, 17, .caption = "Release Seq on Note Off", LJ_MI_F, .events = { { "OnClick", "cbSetupClick" } } },
    { LJ_K_CHECKBOX, "cbCueOff", 12, 90, 220, 17, .caption = "Release Cue on Note Off", LJ_MI_F, .events = { { "OnClick", "cbSetupClick" } } },
    { LJ_K_CHECKBOX, "cbBGOff", 12, 106, 220, 17, .caption = "Release BG Cue on Note Off", LJ_MI_F, .events = { { "OnClick", "cbSetupClick" } } },
    { LJ_K_CHECKBOX, "cbClOff", 12, 122, 220, 17, .caption = "Release Cuelist on Note Off", LJ_MI_F, .events = { { "OnClick", "cbSetupClick" } } },
    /* the program tabs */
    { LJ_K_CHECKBOX, "cbEnable", 6, 26, 120, 17, .caption = "Enable Seq Flash", .alignment = LJ_TA_LEFT /* midiinseq.gif: box right of the text; width PROPOSAL rig 05 s7.2 */, .visible = LJ_NO, LJ_MI_F, .events = { { "OnClick", "cbEnableClick" } } },
    { LJ_K_LABEL, "lChannel", 228, 28, 70, 13, .caption = "MIDI Channel#", .visible = LJ_NO, LJ_MI_F },
    { LJ_K_COMBOBOX, "cbChannel", 300, 24, 64, 21, .style = "csDropDownList", .visible = LJ_NO, LJ_MI_F, .events = { { "OnChange", "cbChannelChange" } } },
    LJ_MI_LV("lvSeq", 0, 3), LJ_MI_LV("lvCue", 1, 3), LJ_MI_LV("lvBG", 2, 3), LJ_MI_LV("lvCl", 3, 3), LJ_MI_LV("lvFn", 4, 2),
    { LJ_K_BUTTON, "bLearn", 6, 174, 60, 23, .caption = "Learn", .visible = LJ_NO, LJ_MI_F, .events = { { "OnClick", "bLearnClick" } } },
    { LJ_K_EDIT, "eNum", 228, 175, 50, 21, .visible = LJ_NO, LJ_MI_F },
    { LJ_K_COMBOBOX, "cbFunction", 72, 175, 206, 21, .style = "csDropDownList", .visible = LJ_NO, LJ_MI_F },
    { LJ_K_BUTTON, "bSet", 282, 174, 82, 23, .caption = "<- Set Seq#", .visible = LJ_NO, LJ_MI_F, .events = { { "OnClick", "bSetClick" } } },
    /* MIDI Log */
    { LJ_K_MEMO, "mmLog", 6, 26, 358, 172, .scroll_bars = "ssVertical", .visible = LJ_NO, LJ_MI_F },
    /* bottom */
    { LJ_K_BITBTN, "bSave", 99, 210, 80, 25, .caption = "&Save", LJ_MI_F, .events = { { "OnClick", "bSaveClick" } } },
    { LJ_K_BITBTN, "bClose", 191, 210, 80, 25, .caption = "&Close", LJ_MI_F, .events = { { "OnClick", "bCloseClick" } } },
    { LJ_K_MAINMENU, "MainMenu1", 0, 0, 0, 0, LJ_MI_F },
};
static const lj_menu_item_desc lj_mi_options[] = {   /* [05 s7.2] Options menu */
    { .name = "M_Minimized", .caption = "Run &Minimized", .on_click = "M_OptionClick", .image_index = -1 },
    { .name = "M_OnTop", .caption = "Stay on &Top", .on_click = "M_OptionClick", .image_index = -1 },
    { .name = "M_AutoSave", .caption = "&Auto Save Changes", .on_click = "M_OptionClick", .image_index = -1 },
};
static const lj_menu_item_desc lj_mi_main[] = {
    { .name = "Options1", .caption = "&Options", .image_index = -1, .children = lj_mi_options, .nchildren = LJ_COUNT_OF(lj_mi_options) },
    { .name = "Help1", .caption = "&Help", .on_click = "Help1Click", .image_index = -1 },
};
static const lj_menu_desc lj_mi_menus[] = { { "MainMenu1", 1, lj_mi_main, LJ_COUNT_OF(lj_mi_main) } };
static const lj_form_desc lj_mi_form = {
    .name = "MidiInForm", .class_name = "TMidiInForm", .caption = "LightJockey MIDI-IN",   /* midiin.gif; LJ_MidiIn.exe's own "(V1.2)" suffix dropped */
    .client_width = 370, .client_height = 240,                                            /* PROPOSAL rig: midiin.gif 300x277 is reduced; widened until the seven tabs fit one row in our sans13, as the GIF shows them */
    .text_height = 13, .color = LJ_CLR(0xC0C0C0), .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11,
    .border = LJ_BS_SINGLE, .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU | LJ_BI_MINIMIZE, .menu = "MainMenu1",
    .menus = lj_mi_menus, .nmenus = LJ_COUNT_OF(lj_mi_menus), .ctls = lj_mi_ctls, .n = LJ_COUNT_OF(lj_mi_ctls),
    .events = { { "OnClose", "FormClose" } },
};

/* ---- state ---- */

static const char *const lj_mi_lv[LJ_MI_KINDS] = { "lvSeq", "lvCue", "lvBG", "lvCl", "lvFn" };
static const char *const lj_mi_enable[LJ_MI_KINDS] = { "Enable Seq Flash", "Enable Cue Latch", "Enable BGCue Latch", "Enable Cuelists", "Enable Functions" };   /* the first from midiinseq.gif, the rest PROPOSAL rig */
static const char *const lj_mi_set[LJ_MI_KINDS] = { "<- Set Seq#", "<- Set Cue#", "<- Set BGCue#", "<- Set CL#", "<- Set Function" };                      /* same */
static const enum lj_midi_kind lj_mi_kind[LJ_MI_KINDS] = { LJ_MIDI_SEQ, LJ_MIDI_CUE, LJ_MIDI_BGCUE, LJ_MIDI_CL, LJ_MIDI_FN };
static const enum lj_rec_kind lj_mi_rec[LJ_MI_KINDS - 1] = { LJ_REC_SEQ, LJ_REC_CUE, LJ_REC_BG, LJ_REC_CL };
static const char *const lj_mi_channels[16] = { "Ch 1", "Ch 2", "Ch 3", "Ch 4", "Ch 5", "Ch 6", "Ch 7", "Ch 8", "Ch 9", "Ch 10", "Ch 11", "Ch 12", "Ch 13", "Ch 14", "Ch 15", "All" };   /* 16 = All as shipped (05 s7) */

static struct {
    lj_window *win; int tab, learning, close_pending;
    char line[LJ_MI_KINDS][LJ_MI_NOTES][72]; const char *items[LJ_MI_KINDS][LJ_MI_NOTES];
    lj_midi_dev dev[LJ_MI_DEVS]; const char *dev_items[LJ_MI_DEVS]; int ndev;
    uint16_t fn_of[LJ_FN_COUNT]; char fn_cap[LJ_FN_COUNT][64]; const char *fn_items[LJ_FN_COUNT]; int nfn;
    char log[LJ_MI_LOG][40]; const char *log_items[LJ_MI_LOG]; int nlog;
} mi;

static lj_widget *find(const char *n) { return lj_window_find(mi.win, n); }
static lj_midi_map *map(void) { return lj_ctx_get()->midi; }
static int ktab(void) { return mi.tab >= 1 && mi.tab <= LJ_MI_KINDS ? mi.tab - 1 : -1; }   /* index into the lj_mi_* kind tables */
static int entry_ch(int k) { lj_midi_map *m = map(); int c = m ? m->channel[lj_mi_kind[k]] : 16; return c == 16 ? LJ_MIDI_CH_ALL : c; }   /* the grid edits the entries of the shown channel */
static void show(const char *n, int on) { lj_widget *w = find(n); if (w) { w->visible = (unsigned)(on != 0); lj_widget_invalidate(w); } }
static void combo_index(lj_widget *w, int i) { if (w) { ((lj_w_combobox *)w)->index = i; lj_widget_invalidate(w); } }

static void fill_grid(int k)
{
    lj_midi_map *m = map();
    int n, i, ch = entry_ch(k), keep;
    lj_widget *lv = find(lj_mi_lv[k]);
    keep = lv ? lj_w_listview_index(lv) : -1;
    for (n = 0; n < LJ_MI_NOTES; n++) {
        const lj_midi_map_entry *e = NULL;
        char name[64] = "";
        for (i = 0; m && i < m->n; i++) if (m->e[i].kind == lj_mi_kind[k] && m->e[i].ch == ch && m->e[i].note == n) e = &m->e[i];
        if (!e) snprintf(mi.line[k][n], sizeof mi.line[k][n], "%d\t\t", n);
        else if (lj_mi_kind[k] == LJ_MIDI_FN) { lj_fn_caption((enum lj_fn)e->fn, name, sizeof name); snprintf(mi.line[k][n], sizeof mi.line[k][n], "%d\t%s", n, name); }
        else { lj_delete_files_record_name(lj_mi_rec[k], e->num, name, sizeof name); snprintf(mi.line[k][n], sizeof mi.line[k][n], "%d\t%u\t%s", n, (unsigned)e->num, name); }
        mi.items[k][n] = mi.line[k][n];
    }
    if (lv) { lj_w_listview_set_items(lv, mi.items[k], LJ_MI_NOTES); if (keep >= 0) ((lj_w_listview *)lv)->index = keep; }
}

static void fill_page(void)
{
    lj_midi_map *m = map();
    int k = ktab(), i;
    static const char *const setup[] = { "lDevice", "cbDevice", "cbNote0", "cbSeqOff", "cbCueOff", "cbBGOff", "cbClOff" };
    static const char *const prog[] = { "cbEnable", "lChannel", "cbChannel", "bLearn", "bSet" };
    for (i = 0; i < LJ_COUNT_OF(setup); i++) show(setup[i], mi.tab == 0);
    for (i = 0; i < LJ_COUNT_OF(prog); i++) show(prog[i], k >= 0);
    for (i = 0; i < LJ_MI_KINDS; i++) show(lj_mi_lv[i], i == k);
    show("eNum", k >= 0 && lj_mi_kind[k] != LJ_MIDI_FN); show("cbFunction", k >= 0 && lj_mi_kind[k] == LJ_MIDI_FN);
    show("mmLog", mi.tab == 6);
    if (!m) return;
    lj_w_checkbox_set_checked(find("cbNote0"), m->note0_is_off);
    lj_w_checkbox_set_checked(find("cbSeqOff"), m->release_on_off[LJ_MIDI_SEQ]); lj_w_checkbox_set_checked(find("cbCueOff"), m->release_on_off[LJ_MIDI_CUE]);
    lj_w_checkbox_set_checked(find("cbBGOff"), m->release_on_off[LJ_MIDI_BGCUE]); lj_w_checkbox_set_checked(find("cbClOff"), m->release_on_off[LJ_MIDI_CL]);
    for (i = 0; i < mi.ndev; i++) if (mi.dev[i].idx == m->device) combo_index(find("cbDevice"), i);
    if (k < 0) return;
    lj_w_set_caption(find("cbEnable"), lj_mi_enable[k]); lj_w_set_caption(find("bSet"), lj_mi_set[k]);
    lj_w_checkbox_set_checked(find("cbEnable"), m->enabled[lj_mi_kind[k]]);
    combo_index(find("cbChannel"), m->channel[lj_mi_kind[k]] >= 1 && m->channel[lj_mi_kind[k]] <= 16 ? m->channel[lj_mi_kind[k]] - 1 : 15);
    fill_grid(k);
}

static void changed(void) { lj_midi_map *m = map(); if (m && m->auto_save) lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0); }

int lj_midi_in_form_tab(int tab)
{
    if (!mi.win || tab < 0 || tab > 6) return -1;
    if (mi.learning) { lj_midi_learn_cancel(); mi.learning = 0; }
    mi.tab = tab;
    { lj_widget *tc = find("TC1"); if (tc && ((lj_w_tabs *)tc)->active != tab) lj_w_tabs_select(tc, tab); }
    fill_page();
    return 0;
}

int lj_midi_in_form_set_number(uint32_t number) { char t[16]; snprintf(t, sizeof t, "%u", (unsigned)number); return lj_w_edit_set_text(find("eNum"), t); }
int lj_midi_in_form_set_function(enum lj_fn fn)
{
    int i;
    for (i = 0; i < mi.nfn; i++) if (mi.fn_of[i] == fn) { combo_index(find("cbFunction"), i); return 0; }
    return -1;
}
int lj_midi_in_form_set_channel(int channel)
{
    lj_midi_map *m = map();
    int k = ktab();
    if (!m || k < 0 || channel < 1 || channel > 16) return -1;
    m->channel[lj_mi_kind[k]] = (uint8_t)channel;
    fill_page(); changed();
    return 0;
}

int lj_midi_in_form_set_note(int note)
{
    lj_midi_map *m = map();
    lj_midi_map_entry e;
    lj_widget *lv;
    const char *t;
    int k = ktab(), fi;
    if (!m || k < 0) return -1;
    lv = find(lj_mi_lv[k]);
    if (note < 0 && lv) note = lj_w_listview_index(lv);
    if (note < 0 || note >= LJ_MI_NOTES) return -1;
    memset(&e, 0, sizeof e);
    e.kind = (uint8_t)lj_mi_kind[k]; e.ch = (uint8_t)entry_ch(k); e.note = (uint8_t)note;
    if (lj_mi_kind[k] == LJ_MIDI_FN) {
        fi = find("cbFunction") ? ((lj_w_combobox *)find("cbFunction"))->index : -1;
        if (fi < 0 || fi >= mi.nfn) return -1;
        e.fn = mi.fn_of[fi];
    } else {
        t = lj_w_edit_text(find("eNum"));
        if (!t || !*t) { lj_midi_unassign(m, lj_mi_kind[k], e.ch, note); fill_grid(k); changed(); return 0; }   /* an empty number clears the note */
        e.num = (uint32_t)strtoul(t, NULL, 10);
        if (e.num == 0 || e.num > 9999) return -1;
    }
    if (lj_midi_assign(m, &e) != 0) return -1;
    if (lv) ((lj_w_listview *)lv)->index = note;
    fill_grid(k); changed();
    return 0;
}

int lj_midi_in_form_learn(void)
{
    if (ktab() < 0) return -1;
    lj_midi_learn_begin();
    mi.learning = 1;
    lj_w_set_caption(find("bLearn"), "Waiting..");   /* PROPOSAL rig: LJ_MidiIn's learning feedback */
    return 0;
}

void lj_midi_in_form_log(const uint8_t msg[3])
{
    lj_widget *mm = find("mmLog");
    int i, st = msg[0] & 0xF0, on = st == 0x90 && msg[2] != 0;
    if (st != 0x80 && st != 0x90) return;                                 /* notes only, as LJ2 logs them [05 s7.2] */
    if (mi.nlog == LJ_MI_LOG) { memmove(mi.log[0], mi.log[1], sizeof mi.log[0] * (LJ_MI_LOG - 1)); mi.nlog--; }
    snprintf(mi.log[mi.nlog++], sizeof mi.log[0], "%s  Ch %d  Note %d  Vel %d", on ? "Note On " : "Note Off", (msg[0] & 0x0F) + 1, msg[1], msg[2]);   /* PROPOSAL rig: the log's line format */
    for (i = 0; i < mi.nlog; i++) mi.log_items[i] = mi.log[i];
    if (mm) { lj_w_memo_set_lines(mm, mi.log_items, mi.nlog); lj_widget_invalidate(mm); }
}

/* ---- handlers ---- */

static void on_tab(lj_widget *w, void *ud) { (void)ud; if (((lj_w_tabs *)w)->active != mi.tab) lj_midi_in_form_tab(((lj_w_tabs *)w)->active); }
static void on_device(lj_widget *w, void *ud)
{
    lj_midi_map *m = map();
    int i = ((lj_w_combobox *)w)->index;
    (void)ud;
    if (!m || i < 0 || i >= mi.ndev) return;
    m->device = mi.dev[i].idx;
    lj_desktop_run_action(LJ_ACT_MIDI_OPEN_DEVICE, m->device);
    changed();
}
static void on_setup_box(lj_widget *w, void *ud)
{
    lj_midi_map *m = map();
    uint8_t v = (uint8_t)lj_w_checkbox_checked(w);
    (void)ud;
    if (!m || !w->name) return;
    if (strcmp(w->name, "cbNote0") == 0) m->note0_is_off = v;
    else if (strcmp(w->name, "cbSeqOff") == 0) m->release_on_off[LJ_MIDI_SEQ] = v;
    else if (strcmp(w->name, "cbCueOff") == 0) m->release_on_off[LJ_MIDI_CUE] = v;
    else if (strcmp(w->name, "cbBGOff") == 0) m->release_on_off[LJ_MIDI_BGCUE] = v;
    else if (strcmp(w->name, "cbClOff") == 0) m->release_on_off[LJ_MIDI_CL] = v;
    changed();
}
static void on_enable(lj_widget *w, void *ud) { lj_midi_map *m = map(); (void)ud; if (m && ktab() >= 0) { m->enabled[lj_mi_kind[ktab()]] = (uint8_t)lj_w_checkbox_checked(w); changed(); } }
static void on_channel(lj_widget *w, void *ud) { (void)ud; lj_midi_in_form_set_channel(((lj_w_combobox *)w)->index + 1); }
static void on_learn(lj_widget *w, void *ud) { (void)w; (void)ud; lj_midi_in_form_learn(); }
static void on_set(lj_widget *w, void *ud) { (void)w; (void)ud; lj_midi_in_form_set_note(-1); }
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; mi.close_pending = 1; }
static void on_option(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(mi.win);
    lj_midi_map *m = map();
    uint8_t *f;
    (void)w; (void)ud;
    if (!it || !it->name || !m) return;
    f = strcmp(it->name, "M_Minimized") == 0 ? &m->start_minimized : strcmp(it->name, "M_OnTop") == 0 ? &m->on_top : &m->auto_save;
    *f = (uint8_t)!*f;
    lj_menu_set_checked(lj_window_menus(mi.win), it->name, *f);
    changed();
}
static void on_help(lj_widget *w, void *ud) { (void)ud; lj_help_viewer_click(w, (void *)"hotkeys-midi"); }   /* W4 acceptance: the bundled viewer */

static const lj_binding lj_mi_bindings[] = {
    { "FormClose", on_close, NULL }, { "TC1Change", on_tab, NULL }, { "cbDeviceChange", on_device, NULL }, { "cbSetupClick", on_setup_box, NULL },
    { "cbEnableClick", on_enable, NULL }, { "cbChannelChange", on_channel, NULL }, { "lvMapChange", lj_handler_noop, NULL }, { "bLearnClick", on_learn, NULL },
    { "bSetClick", on_set, NULL }, { "bSaveClick", on_save, NULL }, { "bCloseClick", on_close, NULL }, { "M_OptionClick", on_option, NULL },
    { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };

lj_window *lj_midi_in_form_open(void)
{
    lj_midi_map *m = map();
    int i;
    if (mi.win) return mi.win;
    mi.win = lj_window_create(&lj_mi_form, lj_mi_bindings);
    if (!mi.win) { lj_log("midi in: cannot create the MIDI-IN window"); return NULL; }
    mi.close_pending = 0; mi.learning = 0; mi.tab = 0;
    mi.ndev = lj_midi_list(mi.dev, LJ_MI_DEVS);
    if (mi.ndev < 0) mi.ndev = 0;
    for (i = 0; i < mi.ndev; i++) mi.dev_items[i] = mi.dev[i].name;
    lj_w_combobox_set_items(find("cbDevice"), mi.dev_items, mi.ndev);
    lj_w_combobox_set_items(find("cbChannel"), lj_mi_channels, 16);
    for (mi.nfn = 0, i = 1; i < LJ_FN_COUNT; i++) {                       /* the Functions list: LJ2's hotkey functions, programs and extensions excluded */
        if (lj_fn_table[i].group == LJ_FNG_PROGRAMS || (lj_fn_table[i].flags & LJ_FNF_EXT)) continue;
        lj_fn_caption((enum lj_fn)i, mi.fn_cap[mi.nfn], sizeof mi.fn_cap[mi.nfn]);
        mi.fn_of[mi.nfn] = (uint16_t)i; mi.fn_items[mi.nfn] = mi.fn_cap[mi.nfn]; mi.nfn++;
    }
    lj_w_combobox_set_items(find("cbFunction"), mi.fn_items, mi.nfn);
    if (m) {
        lj_menu_set_checked(lj_window_menus(mi.win), "M_Minimized", m->start_minimized);
        lj_menu_set_checked(lj_window_menus(mi.win), "M_OnTop", m->on_top);
        lj_menu_set_checked(lj_window_menus(mi.win), "M_AutoSave", m->auto_save);
    }
    lj_midi_in_form_tab(0);
    lj_win_show(mi.win, 0);                                              /* ponytail: Stay on Top persists; the platform flag waits for the app host to expose it */
    return mi.win;
}

void lj_midi_in_form_close(void)
{
    lj_midi_map *m = map();
    if (!mi.win) return;
    if (mi.learning) lj_midi_learn_cancel();
    if (m && m->auto_save) lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0);
    lj_win_hide(mi.win);
    lj_window_destroy(mi.win);
    mi.win = NULL; mi.close_pending = 0; mi.learning = 0;
}

lj_window *lj_midi_in_form_window(void) { return mi.win; }
void lj_midi_in_form_refresh(void)
{
    int ch, note;
    if (mi.win && mi.learning && lj_midi_learn_take(&ch, &note)) {
        lj_widget *lv = ktab() >= 0 ? find(lj_mi_lv[ktab()]) : NULL;
        mi.learning = 0;
        lj_w_set_caption(find("bLearn"), "Learn");
        if (lv) lj_w_listview_select(lv, note);                          /* the learnt note scrolls into view (midiinseq.gif shows the grid around the bound notes) */
        if (ktab() >= 0) {                                                /* bind at once when there is something to bind; an empty number only selects the row */
            const char *t = lj_w_edit_text(find("eNum"));
            if (lj_mi_kind[ktab()] == LJ_MIDI_FN || (t && *t)) lj_midi_in_form_set_note(note);
        }
        (void)ch;                                                         /* the grid edits the tab's own channel (MIDI Channel#), not the channel played */
    }
    if (mi.close_pending) lj_midi_in_form_close();
}
