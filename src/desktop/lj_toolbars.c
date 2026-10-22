/* lj_toolbars.c - the Sequence/cue, Cue list and Fixture toolbars (lj_toolbars.h).
 * Every x/width below is measured from toolandstatusbars.gif (794x140, 1:1, XP),
 * rig item, unless a note says otherwise. Behaviour goes out through lj_fn_run
 * (io) and lj_desktop_run_action, never into windows/ or controls/. */
#include "desktop/lj_toolbars.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_selection.h"
#include "desktop/lj_statusbar.h"
#include "fixlib/lj_profile.h"
#include "io/lj_fn.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_prefs.h"

/* ---- button ids: the toolbar + its F9 / [01 s2.3] number ---- */
enum { B_SEQ_VIEW = 1, B_SEQ_NEW, B_SEQ_SAVE, B_SEQ_LOAD, B_CUE_VIEW, B_CUE_NEW, B_CUE_SAVE, B_CUE_LOAD,
       B_CL_VIEW = 101, B_CL_NEW, B_CL_SAVE, B_CL_LOAD, B_CL_BGCUE, B_CL_STATICS, B_CL_CD, B_CL_MCX, B_CL_2518, B_CL_DMXIN, B_CL_MIDIIN, B_CL_2532,
       B_FX_INTENSITY = 201, B_FX_MOVEMENT, B_FX_COLORS, B_FX_GOBOS, B_FX_BEAM, B_FX_EFFECTS, B_FX_LEVELS, B_FX_LAMP, B_FX_RESET, B_FX_CONTROLS, B_FX_GROUPS,
       B_FX_ALL, B_FX_NONE, B_FX_INCL, B_FX_PREV, B_FX_NEXT, B_FX_SOLO, B_FX_FOLLOW, B_FX_ONLINE, B_FX_FREEZE, B_FX_SMOKE, B_FX_BLACKOUT, B_FX_MASTER };

#define TB_BTN(nm, id, x, w, cap, gl, hnt) { LJ_K_SPEEDBUTTON, nm, x, LJ_TB_BTN_Y, w, LJ_TB_BTN, .tag = id, .caption = cap, .glyph = gl, .hint = hnt, .show_hint = LJ_YES, .num_glyphs = 2, .spacing = 4, .font = LJ_FONT_ARIAL14 }
#define TB_LED(nm, id, x, w, h, cap, hnt)  { LJ_K_UNKNOWN, nm, x, (LJ_TB_ROW_H - (h)) / 2, w, h, .tag = id, .caption = cap, .hint = hnt, .show_hint = LJ_YES, .font = LJ_FONT_ARIAL14 }
#define TB_LABEL(nm, x, cap) { LJ_K_LABEL, nm, x, 6, 36, 18, .caption = cap, .font = LJ_FONT_ARIALBLACK18, .transparent = LJ_YES }   /* bold 12-pt Seq / Cue / C-L (design 7.7) */
#define TB_FIELD(nm, x, w) { LJ_K_EDIT, nm, x, 4, w, 22, .readonly = LJ_YES, .tabstop = LJ_NO, .color = LJ_CLR(LJ_CLR_BTNFACE), .font = LJ_FONT_ARIAL14 }

/* ME-72: every x and width below is the left bevel column / face run of toolandstatusbars.gif (1:1) minus the 9-px frame, rig item:
 * row 1 field 39-216, Seq 222, new 276 / save 301 / load 326, counter 355-413, Cue label 427, field 464-641, Cue 647, 705 / 730 / 755;
 * row 2 field 43-220, CueList 226, 291 / 316 / 341, Background Cue 366-475, Statics 476-565, CD 570, MC-X 595, 620 / 660 / 700 / 740;
 * row 3 seven buttons at 4..154, a 17-px gap, Lamp 196, Reset 221, Controls 248, Groups 300, All 330, None 365, Inclusive 400, Prev 450,
 * Next 474, Solo 498, Follow Spot 528, Smoke 664, Blackout 704, Master 740 (OnLine LED and Freeze: no bevel run, unchanged). */
static const lj_ctl_desc lj_row1[] = {   /* [07 s4.3] */
    TB_LABEL("tb_seqlabel", 8, "Seq"),
    TB_FIELD("tb_seqname", 39, 178),
    TB_BTN("tb_seqcue_1", B_SEQ_VIEW, 222, 50, "Seq", "view", "View Sequence Control"),
    TB_BTN("tb_seqcue_2", B_SEQ_NEW, 276, LJ_TB_BTN, NULL, "new", "New/Clear Sequence"),
    TB_BTN("tb_seqcue_3", B_SEQ_SAVE, 301, LJ_TB_BTN, NULL, "save", "Save Sequence"),
    TB_BTN("tb_seqcue_4", B_SEQ_LOAD, 326, LJ_TB_BTN, NULL, "load", "List of Sequences"),
    { LJ_K_PANEL, "sb_seqcounter", 355, 4, 59, 22, .caption = "1:1", .bevel_outer = LJ_BV_LOWERED, .font = LJ_FONT_ARIAL14 },   /* "1:1" when the sequence is empty [F7] */
    TB_LABEL("tb_cuelabel", 427, "Cue"),
    TB_FIELD("tb_cuename", 464, 178),
    TB_BTN("tb_seqcue_5", B_CUE_VIEW, 647, 54, "Cue", "view", "View Cue Control"),
    TB_BTN("tb_seqcue_6", B_CUE_NEW, 705, LJ_TB_BTN, NULL, "new", "New/Clear Cue"),
    TB_BTN("tb_seqcue_7", B_CUE_SAVE, 730, LJ_TB_BTN, NULL, "save", "Save Cue"),
    TB_BTN("tb_seqcue_8", B_CUE_LOAD, 755, LJ_TB_BTN, NULL, "load", "List of Cues"),
};
static const lj_ctl_desc lj_row2[] = {   /* [07 s4.4], F8 order */
    TB_LABEL("tb_cllabel", 8, "C-L"),
    TB_FIELD("tb_clname", 43, 178),
    TB_BTN("tb_cuelist_1", B_CL_VIEW, 226, 64, "CueList", "view", "Toggle Cue List control"),
    TB_BTN("tb_cuelist_2", B_CL_NEW, 291, LJ_TB_BTN, NULL, "new", "New/Clear Cue List"),
    TB_BTN("tb_cuelist_3", B_CL_SAVE, 316, LJ_TB_BTN, NULL, "save", "Save Cue List"),
    TB_BTN("tb_cuelist_4", B_CL_LOAD, 341, LJ_TB_BTN, NULL, "load", "List of Cue Lists"),
    TB_LED("tb_cuelist_5", B_CL_BGCUE, 366, 110, 24, "Background Cue", "Background Cue"),   /* backgroundcueicon.gif 109x24 */
    TB_LED("tb_cuelist_6", B_CL_STATICS, 476, 90, 25, "Statics", "Statics"),               /* staticcontrollicon.gif 90x25 */
    TB_BTN("tb_cuelist_7", B_CL_CD, 570, LJ_TB_BTN, NULL, "cd", "CD Control"),
    TB_BTN("tb_cuelist_8", B_CL_MCX, 595, LJ_TB_BTN, NULL, "mcx", "MC-X"),
    TB_BTN("tb_cuelist_9", B_CL_2518, 620, 40, NULL, "a2518", "2518 Controller"),           /* a2518icon.gif 39x24 */
    TB_BTN("tb_cuelist_10", B_CL_DMXIN, 660, 40, NULL, "dmxin", "DMX In"),
    TB_BTN("tb_cuelist_11", B_CL_MIDIIN, 700, 40, NULL, "midiin", "MIDI In"),
    TB_BTN("tb_cuelist_12", B_CL_2532, 740, 40, NULL, "da2532", "2532 Direct Access"),
};
static const lj_ctl_desc lj_row3[] = {   /* [07 s4.6], F9 order 1-21 (+ OnLine/OffLine, Freeze) */
    TB_BTN("tb_fixture_1", B_FX_INTENSITY, 4, LJ_TB_BTN, NULL, "intensity", "Intensity"),
    TB_BTN("tb_fixture_2", B_FX_MOVEMENT, 29, LJ_TB_BTN, NULL, "movement", "Movement"),
    TB_BTN("tb_fixture_3", B_FX_COLORS, 54, LJ_TB_BTN, NULL, "colors", "Colors"),
    TB_BTN("tb_fixture_4", B_FX_GOBOS, 79, LJ_TB_BTN, NULL, "gobos", "Gobos"),
    TB_BTN("tb_fixture_5", B_FX_BEAM, 104, LJ_TB_BTN, NULL, "beam", "Beam"),
    TB_BTN("tb_fixture_6", B_FX_EFFECTS, 129, LJ_TB_BTN, NULL, "effects", "Effects"),
    TB_BTN("tb_fixture_7", B_FX_LEVELS, 154, LJ_TB_BTN, NULL, "levels", "Levels/Extended Controls"),
    TB_BTN("tb_fixture_8", B_FX_LAMP, 196, LJ_TB_BTN, NULL, "lamp", "Lamp Control"),
    TB_BTN("tb_fixture_9", B_FX_RESET, 221, LJ_TB_BTN, NULL, "reset", "Fixture Reset"),
    TB_BTN("tb_fixture_10", B_FX_CONTROLS, 248, 50, NULL, "controls", "Hide/Show Fixture Controls"),   /* fixturecontrols.gif 50x25 */
    TB_BTN("tb_fixture_11", B_FX_GROUPS, 300, 29, NULL, "groups", "Fixture Groups"),               /* fixturegroupsicon.gif 29x24 */
    TB_BTN("tb_fixture_12", B_FX_ALL, 330, 34, "All", NULL, "Select All Fixtures"),
    TB_BTN("tb_fixture_13", B_FX_NONE, 365, 35, "None", NULL, "Deselect All Fixtures"),
    TB_BTN("tb_fixture_14", B_FX_INCL, 400, 50, "Inclusive", NULL, "Inclusive/Exclusive Selection"),   /* 13 exe strings: "Inclusive" / "Exclusive" */
    TB_BTN("tb_fixture_15", B_FX_PREV, 450, 23, NULL, "previous", "Previous Fixture"),             /* nextprevfixturebuttons.gif 47x24 */
    TB_BTN("tb_fixture_16", B_FX_NEXT, 474, 23, NULL, "next", "Next Fixture"),
    { LJ_K_SPEEDBUTTON, "tb_fixture_17", 498, LJ_TB_BTN_Y, 30, LJ_TB_BTN, .tag = B_FX_SOLO, .caption = "Solo", .hint = "Fixture Solo", .show_hint = LJ_YES, .group_index = 1, .allow_all_up = LJ_YES, .font = LJ_FONT_ARIAL14 },   /* latches while solo is on */
    TB_BTN("tb_fixture_18", B_FX_FOLLOW, 528, 29, NULL, "followspot", "Follow Spot"),            /* followspoticon.gif 29x24 */
    TB_LED("tb_fixture_19", B_FX_ONLINE, 570, 64, 25, "OnLine", "DMX OnLine/OffLine"),            /* onlineicon.gif 64x25 */
    TB_BTN("tb_fixture_20", B_FX_FREEZE, 636, 26, NULL, "freeze", "Freeze Output"),               /* blindicon.gif 26x24 */
    { LJ_K_SPEEDBUTTON, "tb_fixture_21", 664, LJ_TB_BTN_Y, 36, LJ_TB_BTN, .tag = B_FX_SMOKE, .glyph = "smoke", .hint = "Smoke Control", .show_hint = LJ_YES, .num_glyphs = 2, .spacing = 4,
      .group_index = 2, .allow_all_up = LJ_YES, .font = LJ_FONT_ARIAL14 },   /* smokecontrolicon.gif 36x24; W2: latches while the smoke timer / button fires (LJ_LED_SMOKE, PROPOSAL rig: smokecontrolicon.gif has two states) */
    TB_BTN("tb_fixture_22", B_FX_BLACKOUT, 704, 36, NULL, "blackout", "Blackout/Restore"),        /* blackouticon.gif 38x25 */
    TB_BTN("tb_fixture_23", B_FX_MASTER, 740, 40, NULL, "master", "Master Intensity"),            /* masterintensityicon.gif 40x25 */
};
static const lj_ctl_desc lj_rows[LJ_TB_COUNT] = {
    { LJ_K_PANEL, "tb_row_seqcue", 0, 0, 791, LJ_TB_ROW_H, .align = LJ_AL_TOP, .bevel_outer = LJ_BV_NONE, .font = LJ_FONT_ARIAL14 },
    { LJ_K_PANEL, "tb_row_cuelist", 0, LJ_TB_ROW_H, 791, LJ_TB_ROW_H, .align = LJ_AL_TOP, .bevel_outer = LJ_BV_NONE, .font = LJ_FONT_ARIAL14 },
    { LJ_K_PANEL, "tb_row_fixture", 0, 2 * LJ_TB_ROW_H, 791, LJ_TB_ROW_H, .align = LJ_AL_TOP, .bevel_outer = LJ_BV_NONE, .font = LJ_FONT_ARIAL14 },
};
static const char *const lj_ini_keys[LJ_TB_COUNT] = { "SeqCue", "CueList", "Fixture" };   /* Main.Ini [TOOLBARS] (ours) */

static lj_window *lj_tb_win;
static uint64_t lj_tb_led_tick;   /* the view tick the LEDs were last set from */
static lj_widget *lj_tb_row[LJ_TB_COUNT];
static lj_widget *lj_tb_seqname, *lj_tb_cuename, *lj_tb_clname, *lj_tb_counter;
static lj_widget_vt lj_tb_sb_vt;   /* the speed-button vt with a right-click aware mouse (W0-last-event: on_event carries no button) */
static int (*lj_tb_sb_mouse_orig)(lj_widget *, const lj_mouse_ev *);

static uint8_t *bar_flag_ptr(lj_sysprefs *p, int row) { return row == LJ_TB_SEQCUE ? &p->view_seqcue_bar : row == LJ_TB_CUELIST ? &p->view_cuelist_bar : &p->view_fixture_bar; }
static int bar_flag(const lj_sysprefs *p, int row) { return row == LJ_TB_SEQCUE ? p->view_seqcue_bar : row == LJ_TB_CUELIST ? p->view_cuelist_bar : p->view_fixture_bar; }

static lj_widget *btn(int id)
{
    char name[24];
    int row = id > 200 ? 2 : id > 100 ? 1 : 0, k = id - (row == 2 ? 200 : row == 1 ? 100 : 0);
    snprintf(name, sizeof name, "tb_%s_%d", row == 2 ? "fixture" : row == 1 ? "cuelist" : "seqcue", k);
    return lj_tb_row[row] ? lj_widget_find(lj_tb_row[row], name) : NULL;
}

static void fn(enum lj_fn f) { lj_fn_run(f, 1); }

static void right_click(int id)   /* [F9]: open this control, close the others; Save = quick save; Load = quick load; Follow spot = dialog; Prev/Next = dialog */
{
    switch (id) {
    case B_FX_INTENSITY: case B_FX_MOVEMENT: case B_FX_COLORS: case B_FX_GOBOS: case B_FX_BEAM: case B_FX_EFFECTS: case B_FX_LEVELS: case B_FX_LAMP:
        lj_desktop_run_action(LJ_ACT_CTL_OPEN_ONLY, id - B_FX_INTENSITY); break;
    case B_SEQ_SAVE: fn(LJ_FN_QUICK_SAVE_SEQUENCE); break;
    case B_CUE_SAVE: fn(LJ_FN_QUICK_SAVE_CUE); break;
    case B_CL_SAVE:  fn(LJ_FN_QUICK_SAVE_CUELIST); break;
    case B_FX_FOLLOW: lj_desktop_run_action(LJ_ACT_FOLLOWSPOT, 1); break;
    case B_FX_PREV: case B_FX_NEXT: lj_desktop_run_action(LJ_ACT_NEXTPREV_SETTINGS, 0); break;
    default: break;
    }
}

static void click(int id)
{
    lj_cmd c;
    switch (id) {
    case B_SEQ_VIEW: fn(LJ_FN_SEQUENCE_CONTROL); break;
    case B_SEQ_NEW:  fn(LJ_FN_NEW_SEQUENCE); break;
    case B_SEQ_SAVE: lj_desktop_run_action(LJ_ACT_SAVE_SEQUENCE, 0); break;
    case B_SEQ_LOAD: fn(LJ_FN_TOGGLE_LIST_OF_SEQUENCES); break;
    case B_CUE_VIEW: fn(LJ_FN_TOGGLE_CUE_CONTROL); break;
    case B_CUE_NEW:  fn(LJ_FN_NEW_CLEAR_CUE); break;
    case B_CUE_SAVE: lj_desktop_run_action(LJ_ACT_SAVE_CUE, 0); break;
    case B_CUE_LOAD: fn(LJ_FN_TOGGLE_LIST_OF_CUES); break;
    case B_CL_VIEW:  fn(LJ_FN_CUELIST_CONTROL); break;
    case B_CL_NEW:   lj_desktop_run_action(LJ_ACT_NEW_CUELIST, 0); break;
    case B_CL_SAVE:  lj_desktop_run_action(LJ_ACT_SAVE_CUELIST, 0); break;
    case B_CL_LOAD:  fn(LJ_FN_TOGGLE_LIST_OF_CUELISTS); break;
    case B_CL_BGCUE: fn(LJ_FN_TOGGLE_BGCUE_CONTROL); break;
    case B_CL_STATICS: fn(LJ_FN_STATICS_CONTROL); break;
    case B_CL_CD:    fn(LJ_FN_CD_CONTROL); break;
    case B_CL_MIDIIN: lj_desktop_run_action(LJ_ACT_MIDI_IN, 0); break;
    case B_CL_2532:  fn(LJ_FN_2532_CONTROL); break;
    case B_FX_INTENSITY: fn(LJ_FN_CTL_INTENSITY); break;
    case B_FX_MOVEMENT:  fn(LJ_FN_CTL_MOVEMENT); break;
    case B_FX_COLORS:    fn(LJ_FN_CTL_COLOR); break;
    case B_FX_GOBOS:     fn(LJ_FN_CTL_GOBO); break;
    case B_FX_BEAM:      fn(LJ_FN_CTL_BEAM); break;
    case B_FX_EFFECTS:   fn(LJ_FN_CTL_EFFECT); break;
    case B_FX_LEVELS:    fn(LJ_FN_CTL_LEVEL_SPECIAL); break;
    case B_FX_LAMP:      fn(LJ_FN_CTL_LAMP); break;
    case B_FX_RESET:     fn(LJ_FN_HOME_SELECTED_FIXTURES); break;   /* PROPOSAL: no "Fixture Reset" row in the hotkey table; the Lamp control's reset is the W1 path */
    case B_FX_CONTROLS:  fn(LJ_FN_ALL_CONTROLS); break;
    case B_FX_GROUPS:    lj_desktop_run_action(LJ_ACT_FIXTURE_GROUPS, 0); break;
    case B_FX_ALL:       lj_sel_all(); break;
    case B_FX_NONE:      lj_sel_none(); break;
    case B_FX_INCL:      lj_sel_mode_set(lj_sel_mode() == LJ_SEL_INCLUSIVE ? LJ_SEL_EXCLUSIVE : LJ_SEL_INCLUSIVE); break;
    case B_FX_PREV:      lj_sel_prev(); break;
    case B_FX_NEXT:      lj_sel_next(); break;
    case B_FX_SOLO:
        lj_sel_solo_set(!lj_sel_solo());
        memset(&c, 0, sizeof c); c.kind = LJ_CMD_SOLO; c.a = (uint32_t)(lj_sel_master() < 0 ? 0 : lj_sel_master()); c.b = (uint32_t)lj_sel_solo();
        lj_ctx_post(&c);
        break;
    case B_FX_FOLLOW:    lj_desktop_run_action(LJ_ACT_FOLLOWSPOT, 0); break;
    case B_FX_ONLINE:    fn(LJ_FN_DMX_ONLINE_OFFLINE_TOGGLE); break;
    case B_FX_FREEZE:    fn(LJ_FN_FREEZE_OUTPUT_TOGGLE); break;
    case B_FX_SMOKE:     fn(LJ_FN_SMOKE_CONTROL); break;
    case B_FX_BLACKOUT:  fn(LJ_FN_BLACKOUT_RESTORE); break;
    case B_FX_MASTER:    fn(LJ_FN_TOGGLE_MASTER_INTENSITY); break;
    default: break;
    }
    lj_desktop_toolbar_update();
}

static void on_event(lj_widget *w, int ev, void *ud)
{
    (void)ud;
    if (ev == LJ_WEV_CLICK) click(w->desc ? w->desc->tag : 0);
}

static int sb_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int rc = lj_tb_sb_mouse_orig ? lj_tb_sb_mouse_orig(w, m) : 0;
    if (m->type == LJ_EV_MOUSE_UP && m->button == 2 && w->enabled && lj_w_inside(w, m->x, m->y)) right_click(w->desc ? w->desc->tag : 0);
    return rc;
}

static void build_row(lj_widget *root, int row, const lj_ctl_desc *items, int n)
{
    lj_widget *panel = lj_w_panel_create(root, &lj_rows[row]), *w;
    int i;
    lj_tb_row[row] = panel;
    for (i = 0; i < n; i++) {
        const lj_ctl_desc *d = &items[i];
        switch (d->kind) {
        case LJ_K_SPEEDBUTTON:
            w = lj_w_speedbutton_create(panel, d);
            if (!lj_tb_sb_mouse_orig) { lj_tb_sb_vt = *w->vt; lj_tb_sb_mouse_orig = w->vt->mouse; lj_tb_sb_vt.mouse = sb_mouse; }
            w->vt = &lj_tb_sb_vt;
            w->on_event = on_event;
            break;
        case LJ_K_UNKNOWN:   /* the LED buttons */
            w = lj_w_led_create(panel, d);
            w->on_event = on_event;
            break;
        case LJ_K_LABEL: w = lj_w_label_create(panel, d); break;
        case LJ_K_EDIT:  w = lj_w_edit_create(panel, d); break;
        default:         w = lj_w_panel_create(panel, d); break;
        }
        (void)w;
    }
}

void lj_toolbars_build(lj_window *win, lj_widget *root)
{
    lj_ctx *ctx = lj_ctx_get();
    int i;
    lj_tb_win = win;
    build_row(root, LJ_TB_SEQCUE, lj_row1, LJ_COUNT_OF(lj_row1));
    build_row(root, LJ_TB_CUELIST, lj_row2, LJ_COUNT_OF(lj_row2));
    build_row(root, LJ_TB_FIXTURE, lj_row3, LJ_COUNT_OF(lj_row3));
    lj_tb_seqname = lj_widget_find(lj_tb_row[0], "tb_seqname");
    lj_tb_cuename = lj_widget_find(lj_tb_row[0], "tb_cuename");
    lj_tb_counter = lj_widget_find(lj_tb_row[0], "sb_seqcounter");
    lj_tb_clname  = lj_widget_find(lj_tb_row[1], "tb_clname");
    for (i = 0; i < LJ_TB_COUNT; i++)   /* W2: Main.Ini [PREFS] ViewSCBar ViewClBar ViewFixBar through ctx->prefs; the W1 [TOOLBARS] keys only without a prefs record */
        lj_tb_row[i]->visible = ctx->prefs ? bar_flag(ctx->prefs, i) != 0 : ctx->main_ini ? lj_ini_geti(ctx->main_ini, "TOOLBARS", lj_ini_keys[i], 1) != 0 : 1;
    lj_desktop_seqcue_fields(0, NULL, 1, 1, 0, NULL);
}

void lj_desktop_toolbar_show(enum lj_toolbar tb, int show)
{
    lj_ctx *ctx = lj_ctx_get();
    if ((int)tb < 0 || tb >= LJ_TB_COUNT || !lj_tb_row[tb]) return;
    lj_tb_row[tb]->visible = show != 0;
    if (ctx->prefs) *bar_flag_ptr(ctx->prefs, tb) = (uint8_t)(show != 0);   /* saved with Main.Ini by the app */
    else if (ctx->main_ini) lj_ini_seti(ctx->main_ini, "TOOLBARS", lj_ini_keys[tb], show != 0);
    if (lj_tb_win) { lj_widget_layout(lj_window_root(lj_tb_win)); lj_window_invalidate_all(lj_tb_win); }
}

int lj_desktop_toolbar_visible(enum lj_toolbar tb)
{
    return (int)tb >= 0 && tb < LJ_TB_COUNT && lj_tb_row[tb] ? (int)lj_tb_row[tb]->visible : 0;
}

static void set_enabled(int id, int on)
{
    lj_widget *w = btn(id);
    if (w && w->enabled != (unsigned)(on != 0)) { w->enabled = on != 0; lj_widget_invalidate(w); }
}
static void set_visible(int id, int on)
{
    lj_widget *w = btn(id);
    if (w && w->visible != (unsigned)(on != 0)) { w->visible = on != 0; if (lj_tb_row[2]) lj_widget_invalidate(lj_tb_row[2]); }
}

/* F9 / [01 s2.3.4] / acceptance 3: buttons 1-9 follow the master's control windows; LORD_FIDELITY-11 gates. */
void lj_desktop_toolbar_update(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_profile *p = lj_sel_master_profile();
    lj_widget *w;
    int master = lj_sel_master() >= 0, viewed = ctx->view.tick_no != 0;
    if (!lj_tb_row[2]) return;
    set_enabled(B_FX_INTENSITY, p && lj_profile_window(p, LJ_CTL_INTENSITY));
    set_enabled(B_FX_MOVEMENT,  p && lj_profile_window(p, LJ_CTL_MOVEMENT));
    set_enabled(B_FX_COLORS,    p && lj_profile_window(p, LJ_CTL_COLORS));
    set_enabled(B_FX_GOBOS,     p && lj_profile_window(p, LJ_CTL_GOBOS));
    set_enabled(B_FX_BEAM,      p && lj_profile_window(p, LJ_CTL_BEAM));
    set_enabled(B_FX_EFFECTS,   p && lj_profile_window(p, LJ_CTL_EFFECTS));
    set_enabled(B_FX_LEVELS,    p && (lj_profile_window(p, LJ_CTL_LEVELS) || lj_profile_window(p, LJ_CTL_EXTENDED) || lj_profile_window(p, LJ_CTL_GENERIC)));   /* button 7 [15/01 s3] */
    set_enabled(B_FX_LAMP,      p && lj_profile_window(p, LJ_CTL_LAMP));
    set_enabled(B_FX_RESET,     p && p->reset.enabled);
    set_enabled(B_FX_PREV, master || ctx->show != NULL);
    set_enabled(B_FX_NEXT, master || ctx->show != NULL);
    set_visible(B_FX_SOLO, ctx->allow_solo);                 /* hidden, not greyed [01 s2.3.4] */
    set_visible(B_FX_PREV, ctx->show_nextprev);
    set_visible(B_FX_NEXT, ctx->show_nextprev);
    set_enabled(B_FX_ONLINE, ctx->allow_offline);
    set_enabled(B_FX_FREEZE, ctx->allow_freeze);
    set_enabled(B_CL_CD, 0); set_enabled(B_CL_MCX, 0); set_enabled(B_CL_2518, 0); set_enabled(B_CL_DMXIN, 0);   /* F8: greyed until their waves (W4 / W5) */
    if ((w = btn(B_FX_INCL)) != NULL) lj_w_set_caption(w, lj_sel_mode() == LJ_SEL_INCLUSIVE ? "Inclusive" : "Exclusive");
    if ((w = btn(B_FX_SOLO)) != NULL) lj_w_speedbutton_set_down(w, lj_sel_solo());
    if (viewed && ctx->view.tick_no != lj_tb_led_tick) {   /* W2: the three LEDs follow each new view copy; a window's direct lj_desktop_led_set holds until the next one */
        lj_tb_led_tick = ctx->view.tick_no;
        lj_desktop_led_set(LJ_LED_BGCUE, ctx->view.bg_any);
        lj_desktop_led_set(LJ_LED_STATICS, ctx->view.statics_any);
        lj_desktop_led_set(LJ_LED_SMOKE, ctx->view.smoke_active);
    }
    if ((w = btn(B_FX_ONLINE)) != NULL) {
        int offline = viewed && ctx->view.offline;
        lj_w_led_set(w, !offline);
        ((lj_w_led *)w)->caption = offline ? "OffLine" : "OnLine";   /* "the button mimics the current offline state" */
    }
}

static void field(lj_widget *e, const char *text, int active)
{
    if (!e) return;
    lj_w_edit_set_text(e, text ? text : "");
    e->color = active ? LJ_CLR_YELLOW : LJ_CLR_BTNFACE;   /* yellow = contains data / saved [F7, acceptance 4-5]; grey when empty */
    lj_widget_invalidate(e);
}

void lj_desktop_seqcue_fields(uint32_t seq_no, const char *seq_name, int scene, int scenes, uint32_t cue_no, const char *cue_name)
{
    const lj_ctx *ctx = lj_ctx_get();
    char counter[24];   /* gcc -Wformat-truncation: "%d:%d" of any two ints */
    field(lj_tb_seqname, seq_name, ctx->seq_saved || seq_no != 0);
    field(lj_tb_cuename, cue_name, ctx->cue_saved || cue_no != 0);
    if (lj_tb_clname && ctx->show && ctx->show->current_cl) field(lj_tb_clname, ctx->show->current_cl->name, 0);
    if (scenes < 1) scenes = 1;
    if (scene < 1) scene = 1;
    snprintf(counter, sizeof counter, "%d:%d", scene, scenes);   /* X:Y = current : last [01 s2.3.1] */
    if (lj_tb_counter) lj_w_set_caption(lj_tb_counter, counter);
}

/* ---- W2: LED buttons [07 s4.4, 4.6] and the System Preferences gates (ME-29) ---- */
static uint8_t lj_tb_led_state[LJ_LED_COUNT];
void lj_desktop_led_set(enum lj_desktop_led led, int on)
{
    lj_widget *w;
    if ((int)led < 0 || led >= LJ_LED_COUNT) return;
    lj_tb_led_state[led] = (uint8_t)(on != 0);
    w = btn(led == LJ_LED_BGCUE ? B_CL_BGCUE : led == LJ_LED_STATICS ? B_CL_STATICS : B_FX_SMOKE);
    if (!w) return;
    if (led == LJ_LED_SMOKE) { if (lj_w_speedbutton_down(w) != (on != 0)) lj_w_speedbutton_set_down(w, on); }   /* the Smoke TSpeedButton latches */
    else if (lj_w_led_on(w) != (on != 0)) lj_w_led_set(w, on);   /* green = active, red = none [03 s7, s8] */
}
int  lj_desktop_led_get(enum lj_desktop_led led) { return ((int)led >= 0 && led < LJ_LED_COUNT) ? lj_tb_led_state[led] : 0; }

void lj_desktop_toolbar_gates(const lj_sysprefs *prefs)
{
    lj_ctx *ctx = lj_ctx_get();
    int i;
    if (!prefs) return;
    ctx->allow_freeze = prefs->allow_freeze; ctx->allow_offline = prefs->allow_offline; ctx->allow_solo = prefs->allow_solo;   /* [PREFS] AllowBlind AllowOffline, [SOLO] Enabled */
    for (i = 0; i < LJ_TB_COUNT; i++) if (lj_tb_row[i]) lj_tb_row[i]->visible = bar_flag(prefs, i) != 0;
    lj_statusbar_show(prefs->view_status_bar);   /* relayouts and invalidates the window */
    lj_desktop_toolbar_update();
    if (lj_tb_win) lj_main_menu_sync(lj_tb_win);
}
