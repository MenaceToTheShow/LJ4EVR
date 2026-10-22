/* lj_cuelist_form.c - the Cuelist window and its dialogs (lj_cuelist_form.h; design 10 W3, 8.5; 03 s6).
 * TCuelist2Form over show->current_cl: the lv1 grid (L # | Adv | Command | Parameter | Cue/Trig | Comment/Exe Parameter),
 * the line states the runner publishes (ctx->cl: last executed gray, next blue [RN v2.6 b7]; load_error red, design 8.5),
 * the transport through LJ_ACT_CUELIST_OP, the ListPop / ActionList commands, the eight recent-command buttons, and the
 * TclEditPanels page of the selected line reparented into pEdit as LJ2 does (the holder form is never shown). The CD / DA /
 * MP rows, menu items and time-code buttons stay verbatim, greyed, hint LJ_CL_NOT_AVAILABLE (design 8.5, ME-11). The list
 * is "locked while active" [03 s6.1]: every edit is refused while the runner is not IDLE. The cue list runs in the app
 * (app/lj_cuelist_run.h); this file never steps it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows/lj_cuelist_form.h"

#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_locale.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "core/lj_types.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "io/lj_actions.h"
#include "io/lj_fn.h"
#include "model/lj_library.h"
#include "model/lj_show.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_windows.h"

#define CL_SEC         "CUELIST2"   /* forms.ini / Main.Ini section [01 s4 table: forms.ini [CUELIST2] 545x350, ViewEditPanel ViewEditCommand] */
#define CL_RECENT      8            /* sbCMD1..8 */
#define CL_LV_BORDER   2            /* = lj_w_listview.c LV_BORDER (the sunken edge) */
#define CL_FORM_RGB    0x6B8DCBu    /* Color 13340011: the form, its panels, and the list while it runs (cuelistactive.gif samples #6A8BC9) */
#define CL_DONE_RGB    0x808080u    /* measured from cuelistactive.gif (reduced): the last executed line, #818181 */
#define CL_NEXT_RGB    0x0000FFu    /* measured from cuelistactive.gif (reduced): the waiting line, #0202FC */
#define CL_ERROR_RGB   0xFF0000u    /* load_error text red, as the cue grid's load error row (lj_cue_form_row_rgb) */
#define CL_FLASH_DFLT  1000u        /* PROPOSAL rig 03 s6.3: a sequence dropped on a line flashes 1.0 s until edited */

/* the TclEditPanels page per command (index = enum lj_cl_cmd); pComment for commands without parameters */
static const char *const cl_panel[LJ_CL_CMD_COUNT] = {
    "pComment", "pCDWaitTime", "pCDTrack", "pCDPlayTime", "pComment",
    "pWaitDATime", "pPlayDA", "pCueDA", "pComment", "pSeekDATime", "pComment", "pDATCFactor",
    "pCueMP", "pComment", "pSeekMPTime", "pComment", "pComment", "pWaitMPTime", "pComment", "pComment", "pComment",
    "pMidiTime", "pComment", "pPCTimeout", "pPCClock", "pComment",
    "pGotoLine", "pExecute", "pComment", "pComment", "pHotkeyCommand",
    "pFlashSeq", "pComment", "pComment" };
/* the ActionList rows that are commands (TAction name -> enum lj_cl_cmd); MPLoadFile1 carries aHotkeyCommand (D13) */
static const struct { const char *action; int cmd; } cl_actions[] = {
    { "aWaitCD", LJ_CL_WAIT_CD }, { "aCDPlayFromTrack", LJ_CL_CD_PLAY_FROM_TRACK }, { "aCDPlayFromTime", LJ_CL_CD_PLAY_FROM_TIME }, { "aCDStop", LJ_CL_CD_STOP },
    { "aWaitDA", LJ_CL_WAIT_DA }, { "aDAPlayFile", LJ_CL_PLAY_DA_FILE }, { "aCueDA", LJ_CL_CUE_DA_FILE }, { "aDAStop", LJ_CL_DA_STOP },
    { "aDASeek", LJ_CL_SEEK_DA }, { "aDAPlay", LJ_CL_DA_PLAY }, { "aDATCFactor", LJ_CL_DA_TIMECODE_FACTOR },
    { "aMediaPlayerCueFile", LJ_CL_MP_LOAD_FILE }, { "aMediaPlayerPlay", LJ_CL_MP_PLAY }, { "aMediaPlayerSeek", LJ_CL_MP_SEEK }, { "aMediaPlayerStop", LJ_CL_MP_STOP },
    { "aMediaPlayerPause", LJ_CL_MP_PAUSE }, { "aMediaPlayerWait", LJ_CL_MP_WAIT }, { "aMediaPlayerCloseClip", LJ_CL_MP_CLOSE_CLIP },
    { "aMediaPlayerLaunch", LJ_CL_MP_START }, { "aMediaPlayerClose", LJ_CL_MP_CLOSE },
    { "aMTCWait", LJ_CL_WAIT_MTC }, { "aMTCSeek", LJ_CL_AUTOSEEK_MTC }, { "aWaitElapsed", LJ_CL_WAIT_ELAPSED }, { "aWaitPCClock", LJ_CL_WAIT_PC_CLOCK },
    { "aWaitDateChange", LJ_CL_WAIT_DATE_CHANGE }, { "aGotoLine", LJ_CL_GOTO_LINE }, { "aExecuteFile", LJ_CL_EXECUTE_FILE },
    { "aSetCurrentCue", LJ_CL_SET_CURRENT_CUE }, { "aHotkeyCommand", LJ_CL_HOTKEY_COMMAND } };
/* menu items greyed for good (design 8.5: the CD / DA / MP families and their tools); hints shown on the buttons only (VCL menu
 * hints go to Application.OnHint, which LJ2 does not display) */
static const char *const cl_inert_items[] = {
    "Pop_CDCommands", "popWaitCD", "popCDPlayTrack", "popCDPlayTime", "popCDStop",
    "pop_DACommands", "P_WaitDA", "P_PlayDAFile", "P_CueDAFile", "P_StopDA", "P_SeekDA", "P_PlayDA", "Pop_DATimecodeFactor",
    "Pop_MPCommands", "CueMediaFile1", "MediaPlayerPlay1", "MPSeek1", "MediaPlayerStop1", "MediaPlayerPause1", "WaitMediaPlayer1", "MPCloseClip1",
    "MediaPlayer1", "MediaPlayerLaunch1", "MPClose1",
    "OffsetDATimecodes", "OffsetMediaplayerTimecodes1", "OffsetCDTimecodes1", "SortDAWaitTimes", "SortMediaCodes1", "SortCDWaitTimes1",
    "DigitalAudioTimecodeScaler1",
    /* ME-120 (inert rule, docs/fidelity.md): Offset / Sort over the MIDI and PC waits (TTIMECODEOFFSETFORM / TSORTTIMECODEFORM,
     * W4) and Import Old Format Cuelist (the LJ1 .cl format, dropped) have no behaviour, so they are greyed, never live and dead */
    "OffsetMIDITimecodes1", "OffsetPCTime1", "OffsetMIDITimecodes2", "M_ImportOldCuelist" };
/* ListPop items that edit the list (greyed while it runs, "the list is locked while active") */
static const char *const cl_edit_items[] = {
    "Pop_MTCCommands", "Pop_PCTimeCommands", "popGotoLine", "Pop_Execute", "Pop_SetCurrentCue", "pop_SetCueTrig", "MPLoadFile1",
    "Pop_Cut", "Pop_Paste", "Pop_ClearCommands", "Pop_SetAdvance", "Pop_SetMark", "Pop_TimeCodeOffsets", "Pop_SortWaitTimes", "PopSetTrackingMode" };
static const char *const cl_tc_btn[LJ_TC_COUNT] = { "sbPCElapsed", "sbPCClock", "sbMIDITime", "sbCDTime", "sbDATime", "sbMPTime" };
/* Main.Ini [CUELIST2] preference keys (ours, PROPOSAL rig: LJ2 keeps them in the registry or not at all); the app's runner reads
 * IgnoreFirstWait / AutoCue / MTCTimeout */
static const char *const cl_pref_items[] = { "M_UseColor", "M_IgnoreFirstWait", "M_LoadCueondoubleclick", "M_StopAudio", "M_AutoCue", "M_AutoCueAudio", "M_Autosave" };
static const char *const cl_pref_keys[]  = { "UseColor", "IgnoreFirstWait", "LoadCueOnDblClick", "StopAudio", "AutoCue", "AutoSeekAudio", "Autosave" };
static const char *const cl_mtc_items[] = { "M_MTCStop", "M_MTCRestart", "MTC_NoTimeout" };          /* enum lj_cl_mtc_timeout order */
static const char *const cl_sync_items[] = { "Pop_MTCSyncframes0", "Pop_MTCSyncFrames1", "Pop_MTCSyncFrames5", "Pop_MTCSyncFrames10" };
static const int cl_sync_frames[] = { 0, 1, 5, 10 };

static struct {
    lj_window *win, *edit;                 /* TCuelist2Form, the TclEditPanels holder (never shown) */
    lj_widget *lv, *pedit, *panel;         /* panel = the page currently parented in pEdit */
    lj_widget_vt lv_vt; const lj_widget_vt *lv_orig;
    char (*row)[200]; const char **rowp; uint32_t rows_cap;
    int recent[CL_RECENT];
    int tc_src;
    lj_cl_line clip; int has_clip;
    int filling;                           /* the edit-panel fields are being written by code: their OnChange is ignored */
    uint32_t shown_line;                   /* the line the edit panel shows */
    uint32_t lock_next;                    /* Lock: the next line last scrolled to */
    uint16_t log_base;                     /* Clear Logs: log entries before this index are spent */
} s;

/* ---------------------------------------------------------------- the list and its lines */
static lj_ctx *ctx(void) { return lj_ctx_get(); }
static int running(void) { return ctx()->cl && ctx()->cl->state != LJ_CL_IDLE; }

lj_cuelist *lj_cuelist_form_list(void)
{
    lj_show *sh = ctx()->show;
    if (!sh) return NULL;
    if (!sh->current_cl) sh->current_cl = lj_cuelist_new_rec();   /* the show owns it (lj_show_free) */
    return sh->current_cl;
}
static uint32_t sel_line(void)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    int i = s.lv ? lj_w_listview_index(s.lv) : -1;
    return cl && i >= 0 && (uint32_t)i < cl->n ? (uint32_t)i + 1 : 0;
}
static lj_cl_line *line_of(uint32_t n) { lj_cuelist *cl = lj_cuelist_form_list(); return cl && n >= 1 && n <= cl->n ? &cl->lines[n - 1] : NULL; }
static lj_cl_line *sel(void) { return running() ? NULL : line_of(sel_line()); }   /* the editable selected line; NULL while the list runs */
static void modified(void) { lj_ctx_set_modified(LJ_MOD_CL, 1); lj_cuelist_form_refresh(); }
static void select_line(uint32_t n) { if (s.lv && n >= 1) { lj_w_listview_select(s.lv, (int)n - 1); lj_widget_invalidate(s.lv); } }
static void op(int o, uint32_t n) { lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(o, n)); }

static void fmt_tc(char *dst, size_t cap, uint32_t ms, int fps, int fields)   /* "MM:SS:FF" (3 fields) or "HH:MM:SS:FF" / "HH:MM:SS" */
{
    int h, m, sec, f;
    lj_cl_ms_hmsf(ms, fps, &h, &m, &sec, &f);
    if (fields == 3) snprintf(dst, cap, "%02d:%02d:%02d", h * 60 + m, sec, f);
    else if (fields == 2) snprintf(dst, cap, "%02d:%02d:%02d", h, m, sec);
    else snprintf(dst, cap, "%02d:%02d:%02d:%02d", h, m, sec, f);
}
static int mtc_fps(const lj_cl_line *l) { int f = atoi(l->param); return f == 24 || f == 30 ? f : 25; }

/* the Parameter column (PROPOSAL rig 03 s6.3 for the commands cuelist1.gif does not show; CD = "M:S:F", "Track# n" as shown) */
static void param_text(const lj_cl_line *l, char *dst, size_t cap)
{
    dst[0] = 0;
    switch (l->cmd) {
    case LJ_CL_WAIT_CD: case LJ_CL_CD_PLAY_FROM_TIME: fmt_tc(dst, cap, l->param_ms, 75, 3); break;
    case LJ_CL_CD_PLAY_FROM_TRACK: snprintf(dst, cap, "Track# %u", (unsigned)l->param_ms); break;
    case LJ_CL_WAIT_DA: case LJ_CL_SEEK_DA: case LJ_CL_MP_SEEK: case LJ_CL_MP_WAIT: fmt_tc(dst, cap, l->param_ms, 100, 4); break;
    case LJ_CL_WAIT_MTC: fmt_tc(dst, cap, l->param_ms, mtc_fps(l), 4); break;
    case LJ_CL_WAIT_ELAPSED: fmt_tc(dst, cap, l->param_ms, 100, 3); break;
    case LJ_CL_WAIT_PC_CLOCK: fmt_tc(dst, cap, l->param_ms, 100, 2); break;
    case LJ_CL_GOTO_LINE: snprintf(dst, cap, "%u", (unsigned)l->param_ms); break;
    case LJ_CL_HOTKEY_COMMAND: if (l->param_ms > 0 && l->param_ms < LJ_FN_COUNT) lj_fn_caption((enum lj_fn)l->param_ms, dst, cap); break;
    case LJ_CL_FLASH_SEQUENCE: snprintf(dst, cap, "%04u %u%c%u s", (unsigned)l->ref, (unsigned)(l->param_ms / 1000), lj_locale_decimal(), (unsigned)(l->param_ms % 1000 / 100)); break;   /* D6, ME-119 */
    case LJ_CL_LOAD_BACKGROUND_CUE: case LJ_CL_LINK_CUELIST: snprintf(dst, cap, "%04u", (unsigned)l->ref); break;
    default: lj_str_copy(dst, cap, l->param); break;   /* file names (Execute, DA / MP files), the DA factor */
    }
}

int lj_cuelist_form_row(uint32_t n, char *dst, size_t cap)
{
    lj_show *sh = ctx()->show;
    const lj_cl_line *l = line_of(n);
    char par[96], ct[80];
    int k;
    if (!l || !dst || !cap) return -1;
    param_text(l, par, sizeof par);
    ct[0] = 0;
    if (l->cue) {
        const lj_cue *cue = sh ? (const lj_cue *)lj_table_get(&sh->cues, l->cue) : NULL;
        snprintf(ct, sizeof ct, "%04u %s", (unsigned)l->cue, cue ? cue->name : "");   /* cuelist1.gif: "0003 Intro Color Fade 1" */
    } else if (l->trig_mask) {
        int i, o = snprintf(ct, sizeof ct, "Trig");                                     /* PROPOSAL rig 03 s6.2: the slot list */
        for (i = 0; i < LJ_CUE_SLOTS && o > 0 && (size_t)o < sizeof ct; i++)
            if (l->trig_mask & (1u << i)) o += snprintf(ct + o, sizeof ct - (size_t)o, " %d", i + 1);
    }
    k = snprintf(dst, cap, "%u\t%s\t%s\t%s\t%s\t%s", (unsigned)n, lj_cl_adv_caption[l->manual ? 1 : 0],
                 l->cmd < LJ_CL_CMD_COUNT ? lj_cl_cmd_grid[l->cmd] : "", par, ct, l->comment);
    return k < 0 || (size_t)k >= cap ? -1 : 0;
}

/* ---------------------------------------------------------------- lv1 paint: the line states over the stock report view */
/* Use Color Codes (07 s7, cuelist1.gif): waits green, other commands blue, the cue column blue; "<>" and the rest black.
 * The two hues are PROPOSAL rig 07 s7 (the reduced GIF blends them: #7B7BFF, #73BD73). */
static uint32_t cell_rgb(const lj_cl_line *l, int col)
{
    if (col == 2 && l->cmd != LJ_CL_NONE) return lj_cl_cmd_is_wait(l->cmd) ? 0x008000u : 0x0000FFu;
    return col == 4 ? 0x0000FFu : 0x000000u;
}
static void lv_paint(lj_widget *w, lj_surface *sf)
{
    lj_w_listview *v = (lj_w_listview *)w;
    const lj_cl_status *st = ctx()->cl;
    lj_cuelist *cl = lj_cuelist_form_list();
    int ax, ay, i, c, top, area_w, use_color;
    s.lv_orig->paint(w, sf);
    if (!cl) return;
    use_color = lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, "UseColor", 0);
    lj_widget_abs(w, &ax, &ay);
    top = ay + CL_LV_BORDER + (v->show_headers ? v->header_h : 0);
    area_w = w->w - 2 * CL_LV_BORDER - (v->n * v->row_h > w->h - (top - ay) - CL_LV_BORDER ? LJ_SB_W : 0);
    for (i = 0; v->sb.pos + i < v->n && top + (i + 1) * v->row_h <= ay + w->h - CL_LV_BORDER; i++) {
        int item = v->sb.pos + i, state = lj_cl_line_state(st, cl, (uint32_t)item + 1), x = ax + CL_LV_BORDER, ry = top + i * v->row_h;
        uint32_t bg, fg;
        char buf[200];
        int recolor = use_color && state == LJ_CLL_PLAIN && item != v->index && (uint32_t)item < cl->n;
        if (!recolor && (state == LJ_CLL_PLAIN || (item == v->index && state != LJ_CLL_ERROR))) continue;   /* the selection paints as the stock view does */
        bg = state == LJ_CLL_NEXT ? CL_NEXT_RGB : state == LJ_CLL_DONE ? CL_DONE_RGB : item == v->index ? LJ_CLR_HIGHLIGHT : w->color;
        fg = state == LJ_CLL_ERROR ? CL_ERROR_RGB : state == LJ_CLL_NEXT ? 0xFFFFFFu : 0x000000u;
        lj_fill(sf, x, ry, area_w, v->row_h, bg);
        for (c = 0; c < v->ncols && x < ax + CL_LV_BORDER + area_w; c++) {
            const char *t = v->items[item];
            int k = c, j = 0, cw = ax + CL_LV_BORDER + area_w - x < v->col_w[c] ? ax + CL_LV_BORDER + area_w - x : v->col_w[c];   /* the last column stops at the view's edge */
            while (k > 0 && *t) if (*t++ == '\t') k--;
            while (t[j] && t[j] != '\t' && j < (int)sizeof buf - 1) { buf[j] = t[j]; j++; }
            buf[j] = 0;
            while (j > 0 && lj_text_width(lj_w_font_of(w), buf) + 4 > cw) buf[--j] = 0;   /* ME-111: clipped per glyph at the cell edge, never dropped */
            if (buf[0]) lj_w_text(sf, w, x + 2, ry + (v->row_h - lj_w_font_of(w)->height) / 2, buf, recolor ? cell_rgb(&cl->lines[item], c) : fg);
            if (v->grid_lines && cw == v->col_w[c]) lj_vline(sf, x + cw - 1, ry, v->row_h, LJ_CLR_BTNFACE);
            x += cw;
        }
        if (v->grid_lines) lj_hline(sf, ax + CL_LV_BORDER, ry + v->row_h - 1, area_w, LJ_CLR_BTNFACE);
    }
}

/* ---------------------------------------------------------------- the edit panel (TclEditPanels pages in pEdit) */
static lj_widget *ef(const char *name)   /* a field of the holder, or of the page now parented in pEdit */
{
    lj_widget *w = s.panel ? lj_widget_find(s.panel, name) : NULL;
    return w ? w : s.edit ? lj_window_find(s.edit, name) : NULL;
}
static void set_num(const char *name, int v) { char b[16]; snprintf(b, sizeof b, "%d", v); if (ef(name)) lj_w_edit_set_text(ef(name), b); }
static void set_txt(const char *name, const char *t) { if (ef(name)) lj_w_edit_set_text(ef(name), t); }
static int get_num(const char *name) { const char *t = ef(name) ? lj_w_edit_text(ef(name)) : NULL; return t ? atoi(t) : 0; }
static void enable_tree(lj_widget *w, int on) { lj_widget *c; w->enabled = on != 0; for (c = w->first; c; c = c->next) enable_tree(c, on); }

static void show_panel(const char *name)
{
    lj_widget *p = name ? ef(name) : NULL;
    if (!s.pedit || p == s.panel) return;
    if (s.panel) { lj_widget_remove(s.panel); lj_widget_add(lj_window_root(s.edit), s.panel); s.panel->visible = 0; }
    s.panel = p;
    if (p) { lj_widget_remove(p); lj_widget_add(s.pedit, p); p->x = 0; p->y = 0; p->visible = 1; }   /* LJ2 parents the page into pEdit at its origin */
    lj_widget_invalidate(s.pedit);
}

static void fill_edit(void)
{
    uint32_t n = sel_line();
    const lj_cl_line *l = line_of(n);
    int h, m, sec, f;
    const char *pn;
    char cap[96];
    s.shown_line = n;
    if (!s.edit) return;
    if (!l) { show_panel(NULL); return; }
    pn = cl_panel[l->cmd < LJ_CL_CMD_COUNT ? l->cmd : 0];
    s.filling = 1;
    switch (l->cmd) {
    case LJ_CL_WAIT_ELAPSED: lj_cl_ms_hmsf(l->param_ms, 100, &h, &m, &sec, &f); set_num("eElapsedMin", h * 60 + m); set_num("eElapsedSec", sec); set_num("eElapsedSec100", f); set_txt("eComElapsed", l->comment); break;
    case LJ_CL_WAIT_PC_CLOCK: lj_cl_ms_hmsf(l->param_ms, 100, &h, &m, &sec, &f); set_num("ePCHour", h); set_num("ePCMin", m); set_num("ePCSec", sec); set_txt("eComPC", l->comment); break;
    case LJ_CL_WAIT_MTC: {
        int fps = mtc_fps(l);
        lj_cl_ms_hmsf(l->param_ms, fps, &h, &m, &sec, &f); set_num("eMTCHour", h); set_num("eMTCMin", m); set_num("eMTCSec", sec); set_num("eMTCFrame", f);
        lj_w_speedbutton_set_down(ef(fps == 24 ? "sbMIDI1" : fps == 30 ? "sbMIDI3" : "sbMIDI2"), 1);
        set_txt("eComMTC", l->comment); break; }
    case LJ_CL_GOTO_LINE: set_num("eGotoLine", (int)l->param_ms); set_txt("eCOMGotoLine", l->comment); break;
    case LJ_CL_EXECUTE_FILE: set_txt("eExecuteProgram", l->param); set_txt("eCOMExecute", l->comment); break;
    case LJ_CL_HOTKEY_COMMAND:
        cap[0] = 0;
        if (l->param_ms > 0 && l->param_ms < LJ_FN_COUNT) lj_fn_caption((enum lj_fn)l->param_ms, cap, sizeof cap);
        set_txt("eHotkeyFunction", cap); set_txt("eHotkeyComment", l->comment); break;
    case LJ_CL_FLASH_SEQUENCE: set_num("eFlashSeqSec", (int)(l->param_ms / 1000)); set_num("eFlashSeqFrac", (int)(l->param_ms % 1000 / 100)); set_txt("eComFlashSeq", l->comment); break;
    default:
        if (!strcmp(pn, "pComment")) set_txt("eCom", l->comment);
        break;   /* the CD / DA / MP pages show greyed with their DFM defaults: their parameters stay as loaded (design 8.5) */
    }
    s.filling = 0;
    show_panel(pn);
    if (s.panel) enable_tree(s.panel, !lj_cl_cmd_inert(l->cmd) && !running());
}

/* OnChange of the edit-panel fields: write the page back into the selected line */
static void ed_change(lj_widget *w, void *ud)
{
    lj_cl_line *l = sel();
    (void)w; (void)ud;
    if (s.filling || !l || lj_cl_cmd_inert(l->cmd)) return;
    switch (l->cmd) {
    case LJ_CL_WAIT_ELAPSED: l->param_ms = lj_cl_hmsf_ms(0, get_num("eElapsedMin"), get_num("eElapsedSec"), get_num("eElapsedSec100"), 100); break;
    case LJ_CL_WAIT_PC_CLOCK: l->param_ms = lj_cl_hmsf_ms(get_num("ePCHour"), get_num("ePCMin"), get_num("ePCSec"), 0, 100); break;
    case LJ_CL_WAIT_MTC: l->param_ms = lj_cl_hmsf_ms(get_num("eMTCHour"), get_num("eMTCMin"), get_num("eMTCSec"), get_num("eMTCFrame"), mtc_fps(l)); break;
    case LJ_CL_GOTO_LINE: l->param_ms = (uint32_t)(get_num("eGotoLine") > 0 ? get_num("eGotoLine") : 0); break;
    case LJ_CL_EXECUTE_FILE: if (ef("eExecuteProgram")) lj_str_copy(l->param, sizeof l->param, lj_w_edit_text(ef("eExecuteProgram"))); break;
    case LJ_CL_FLASH_SEQUENCE: {
        uint32_t ms = (uint32_t)get_num("eFlashSeqSec") * 1000u + (uint32_t)get_num("eFlashSeqFrac") * 100u;
        l->param_ms = ms > LJ_CL_FLASH_MAX_MS ? LJ_CL_FLASH_MAX_MS : ms; break; }
    default: break;
    }
    modified();
}
static void ed_comment(lj_widget *w, void *ud)
{
    lj_cl_line *l = sel();
    (void)ud;
    if (s.filling || !l || lj_cl_cmd_inert(l->cmd) || !lj_w_edit_text(w)) return;
    lj_str_copy(l->comment, sizeof l->comment, lj_w_edit_text(w));
    modified();
}
static void ed_fps(lj_widget *w, void *ud)   /* sbMIDI1..3: "24 fps" / "25fps" / "30 fps" */
{
    lj_cl_line *l = sel();
    (void)ud;
    if (!l || l->cmd != LJ_CL_WAIT_MTC || !w->name) return;
    {
        int h, m, sec, f;
        lj_cl_ms_hmsf(l->param_ms, mtc_fps(l), &h, &m, &sec, &f);
        lj_str_copy(l->param, sizeof l->param, !strcmp(w->name, "sbMIDI1") ? "24" : !strcmp(w->name, "sbMIDI3") ? "30" : "25");
        l->param_ms = lj_cl_hmsf_ms(h, m, sec, f, mtc_fps(l));
    }
    modified();
}
static void ed_select_cmd(lj_widget *w, void *ud) { (void)w; (void)ud; if (sel()) lj_hotkey_function_open(); }
/* ponytail: no native file dialog in W3 (platform/ owns SDL_ShowOpenFileDialog); the path is typed into the field */
static void ed_browse(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("cuelist: the file browser is not available yet; type the path into the field"); }

static const lj_binding ed_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "eNumEditEnter", lj_handler_noop, NULL }, { "CommentChanged", ed_comment, NULL },
    { "eElapsedChange", ed_change, NULL }, { "PCTimeChange", ed_change, NULL }, { "MIDITimeChange", ed_change, NULL }, { "GotoLineChange", ed_change, NULL },
    { "eFlashSeqDurationChange", ed_change, NULL }, { "sbMIDI2Click", ed_fps, NULL }, { "bSelectCommandClick", ed_select_cmd, NULL },
    { "bOpenExeClick", ed_browse, NULL },
    /* the CD / DA / MP pages: greyed (design 8.5), their handlers bound to nothing */
    { "CDTrackNoChange", lj_handler_noop, NULL }, { "CDPlayTimeChange", lj_handler_noop, NULL }, { "CDWaitTimeChange", lj_handler_noop, NULL }, { "eWaitDATimeChange", lj_handler_noop, NULL },
    { "eSeekDAChange", lj_handler_noop, NULL }, { "eWaitMPTimeChange", lj_handler_noop, NULL }, { "eSeekMPFrameChange", lj_handler_noop, NULL }, { "eDATCFactorChange", lj_handler_noop, NULL },
    { "bBrowseDAClick", lj_handler_noop, NULL }, { "bBrowseMPCueClick", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

/* ---------------------------------------------------------------- commands and the recent panel */
static void recent_push(int cmd)
{
    int i, j = CL_RECENT - 1;
    for (i = 0; i < CL_RECENT; i++) if (s.recent[i] == cmd) { j = i; break; }
    for (; j > 0; j--) s.recent[j] = s.recent[j - 1];
    s.recent[0] = cmd;
    if (ctx()->forms_ini) for (i = 0; i < CL_RECENT; i++) { char k[16]; snprintf(k, sizeof k, "Cmd%d", i + 1); lj_ini_sets(ctx()->forms_ini, CL_SEC, k, lj_cl_cmd_words[s.recent[i]]); }
}
static void recent_load(void)
{
    /* default = the three cuelist1.gif shows (measured from cuelist1.gif, reduced): CD Stop, CD Play From Track, Wait CD;
     * forms.ini [CUELIST2] Cmd1..Cmd8 (ours, PROPOSAL key) */
    static const int dflt[CL_RECENT] = { LJ_CL_CD_STOP, LJ_CL_CD_PLAY_FROM_TRACK, LJ_CL_WAIT_CD };
    int i, c;
    for (i = 0; i < CL_RECENT; i++) {
        char k[16];
        const char *v;
        snprintf(k, sizeof k, "Cmd%d", i + 1);
        v = lj_ini_gets(ctx()->forms_ini, CL_SEC, k, NULL);
        s.recent[i] = dflt[i];
        if (v) for (c = 0, s.recent[i] = 0; c < LJ_CL_CMD_COUNT; c++) if (!strcmp(v, lj_cl_cmd_words[c])) s.recent[i] = c;
        if (s.recent[i] == LJ_CL_SET_CUE_TRIG) s.recent[i] = 0;   /* a panel action, never a stored command */
    }
}
int lj_cuelist_form_recent(int i) { return i >= 0 && i < CL_RECENT ? s.recent[i] : 0; }

void lj_cuelist_form_apply_cmd(int cmd)
{
    lj_cl_line *l = sel();
    lj_show *sh = ctx()->show;
    if (cmd <= LJ_CL_NONE || cmd >= LJ_CL_CMD_COUNT || lj_cl_cmd_inert(cmd) || cmd >= LJ_CL_FLASH_SEQUENCE) return;   /* drag-only commands come by drop */
    if (cmd == LJ_CL_SET_CUE_TRIG) { if (l) lj_win_open(LJ_WIN_CL_SET_CUE_TRIG); return; }
    if (!l) return;
    recent_push(cmd);
    if (cmd == LJ_CL_SET_CURRENT_CUE) {   /* "Put the current cue number on the line" [03 s6.3] */
        l->cue = sh && sh->current_cue ? sh->current_cue->number : ctx()->view.cue_number;
        l->trig_mask = 0;
    } else {
        l->cmd = (uint8_t)cmd; l->param_ms = 0; l->param[0] = 0; l->ref = 0;
        if (cmd == LJ_CL_WAIT_MTC) lj_str_copy(l->param, sizeof l->param, "25");   /* PROPOSAL rig 03 s6.4: 25 fps until the fps button says otherwise */
    }
    fill_edit();
    modified();
}

/* ---------------------------------------------------------------- refresh: grid, gates, caption, labels */
static void sel_changed(void);
static void set_btn(const char *name, int enabled, int visible)
{
    lj_widget *w = s.win ? lj_window_find(s.win, name) : NULL;
    if (!w || ((unsigned)(enabled != 0) == w->enabled && (unsigned)(visible != 0) == w->visible)) return;
    w->enabled = enabled != 0; w->visible = visible != 0;
    lj_widget_invalidate(w);
}
static void fill_rows(lj_cuelist *cl)
{
    uint32_t i;
    if (cl->n > s.rows_cap) {
        uint32_t cap = cl->n + 64;
        char (*r)[200] = realloc(s.row, cap * sizeof *s.row);
        const char **p = realloc(s.rowp, cap * sizeof *s.rowp);
        if (r) s.row = r;
        if (p) s.rowp = p;
        if (!r || !p) lj_fatal("cuelist form: out of memory at %u lines", (unsigned)cap);
        s.rows_cap = cap;
    }
    for (i = 0; i < cl->n; i++) { if (lj_cuelist_form_row(i + 1, s.row[i], sizeof s.row[i]) != 0) s.row[i][0] = 0; s.rowp[i] = s.row[i]; }
}

void lj_cuelist_form_refresh(void)
{
    const lj_cl_status *st = ctx()->cl;
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_menu *m;
    char buf[128];
    int run = running(), wait_go = st && st->state == LJ_CL_WAIT_GO, i, keep;
    if (!s.win || !cl) return;
    m = lj_window_menus(s.win);
    if (lj_win_caption_cuelist(buf, sizeof buf, cl->number, cl->name, cl->tracking) == 0) lj_window_set_caption(s.win, buf);
    fill_rows(cl);
    keep = lj_w_listview_index(s.lv);
    lj_w_listview_set_items(s.lv, s.rowp, (int)cl->n);
    ((lj_w_listview *)s.lv)->index = keep < (int)cl->n ? keep : -1;
    s.lv->color = run ? CL_FORM_RGB : (s.lv->desc ? lj_desc_rgb(s.lv->desc->color, LJ_CLR_BTNHIGHLIGHT) : LJ_CLR_BTNHIGHLIGHT);   /* cuelistactive.gif: the list takes the form colour while it runs */
    lj_widget_invalidate(s.lv);
    /* Lock "Keep active line visible": the waiting line selected and scrolled to */
    if (run && st->next && st->next != s.lock_next && lj_w_speedbutton_down(lj_window_find(s.win, "sbLockLine"))) { s.lock_next = st->next; select_line(st->next); }
    if (!run) s.lock_next = 0;
    /* the transport table [03 s6.2]: Top / Run replaced by STOP while active; Go ! only on a manual wait, Back not on line 1 */
    set_btn("sbTop", !run && cl->n > 0, !run);
    set_btn("sbRun", !run && cl->n > 0, !run);
    set_btn("sbStop", run, run);
    set_btn("sbGo", wait_go, 1);
    set_btn("sbBack", wait_go && st->next > 1, 1);
    set_btn("sbAdd", !run, 1); set_btn("sbInsert", !run && sel_line(), 1); set_btn("sbDelete", !run && sel_line(), 1);
    set_btn("sbNew", !run, 1); set_btn("sbLoad", !run, 1); set_btn("sbSave", 1, 1);
    for (i = 0; i < CL_RECENT; i++) {
        char nm[16];
        lj_widget *b;
        snprintf(nm, sizeof nm, "sbCMD%d", i + 1);
        if ((b = lj_window_find(s.win, nm)) == NULL) continue;
        lj_w_set_caption(b, s.recent[i] ? lj_cl_cmd_caption[s.recent[i]] : "");
        b->enabled = s.recent[i] && !lj_cl_cmd_inert(s.recent[i]) && !run;
        b->hint = s.recent[i] && lj_cl_cmd_inert(s.recent[i]) ? LJ_CL_NOT_AVAILABLE : NULL;
        lj_widget_invalidate(b);
    }
    for (i = 0; i < LJ_COUNT_OF(cl_edit_items); i++) lj_menu_set_enabled(m, cl_edit_items[i], !run);
    for (i = 0; i < LJ_COUNT_OF(cl_inert_items); i++) lj_menu_set_enabled(m, cl_inert_items[i], 0);
    lj_menu_set_enabled(m, "Pop_JumpToThis", wait_go);
    lj_menu_set_checked(m, "CuelistTrackingOn1", cl->tracking); lj_menu_set_checked(m, "CuelistTrackingOff1", !cl->tracking);
    snprintf(buf, sizeof buf, "%u Lines", (unsigned)cl->n);                                        /* cuelist1.gif: "133 Lines" */
    lj_w_set_caption(lj_window_find(s.win, "lTotalLines"), buf);
    {
        uint32_t e = st ? st->elapsed_ms / 1000 : 0;
        snprintf(buf, sizeof buf, "Elapsed %04u:%02u:%02u", (unsigned)(e / 3600), (unsigned)(e / 60 % 60), (unsigned)(e % 60));   /* the DFM caption "Elapsed 0000:00:00" */
        lj_w_set_caption(lj_window_find(s.win, "lTotalElapsed"), buf);
    }
    /* lCurrentTime: the runner's time code when it shows our source (no runner op selects the source yet: docs blocker) */
    if (lj_cl_tc_text(buf, sizeof buf, st && st->tc_src == s.tc_src ? st->tc_ms : -1, s.tc_src == LJ_TC_MIDI ? 25 : 100) == 0)
        lj_w_set_caption(lj_window_find(s.win, "lCurrentTime"), buf);
    if (sel_line() != s.shown_line) sel_changed();
    else if (s.panel) enable_tree(s.panel, line_of(s.shown_line) && !lj_cl_cmd_inert(line_of(s.shown_line)->cmd) && !run);
}

/* ---------------------------------------------------------------- events of TCuelist2Form */
static int btn(void) { const lj_mouse_ev *e = s.win ? lj_window_last_event(s.win) : NULL; return e ? e->button : 1; }
static void on_top(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_TOP, 0); }
static void on_run(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_RUN, sel_line() ? sel_line() : 1); }
static void on_go(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_GO, 0); }
static void on_back(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_BACK, 0); }
static void on_stop(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_STOP, 0); }
static void on_mark(lj_widget *w, void *ud)   /* sbMark1..4: click jumps to the mark, right-click runs from it [03 s6.2] */
{
    lj_cuelist *cl = lj_cuelist_form_list();
    int k = w && w->name ? w->name[6] - '0' : 0;
    (void)ud;
    if (k < 1 || k > LJ_MARKS || !cl) return;
    if (btn() == 2) op(LJ_CL_OP_MARK_RUN, (uint32_t)k);
    else { if (!running() && cl->mark[k - 1]) select_line(cl->mark[k - 1]); else op(LJ_CL_OP_MARK_GOTO, (uint32_t)k); }
}
static void on_tc(lj_widget *w, void *ud)   /* sbTimeCodeMouseUp: the shown time-code source; CD / DA / MP greyed */
{
    int i;
    (void)ud;
    for (i = 0; i < LJ_TC_CD; i++) if (w && w->name && !strcmp(w->name, cl_tc_btn[i])) {
        s.tc_src = i;
        if (ctx()->forms_ini) lj_ini_seti(ctx()->forms_ini, CL_SEC, "TCSource", i);   /* ours, PROPOSAL key */
    }
    lj_w_speedbutton_set_down(lj_window_find(s.win, cl_tc_btn[s.tc_src]), 1);
    lj_cuelist_form_refresh();
}
static void on_log(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_LOG_TIMECODE); }
static void on_add(lj_widget *w, void *ud)   /* sbMouseDown of Add / Insert / Delete: right-click on Add / Insert = 10 lines [03 s6.2] */
{
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_cl_line l;
    uint32_t at = sel_line(), k, nlines = btn() == 2 ? 10u : 1u;
    (void)ud;
    if (!cl || running() || !w || !w->name) return;
    if (!strcmp(w->name, "sbDelete")) { if (at) lj_cuelist_delete(cl, at - 1); modified(); return; }
    memset(&l, 0, sizeof l);
    for (k = 0; k < nlines; k++) lj_cuelist_insert(cl, !strcmp(w->name, "sbInsert") && at ? at - 1 : cl->n, &l);
    if (strcmp(w->name, "sbInsert") || !at) select_line(cl->n);
    modified();
}
static void on_new(lj_widget *w, void *ud) { (void)w; (void)ud; if (!running()) lj_cuelist_form_new(); }
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_SAVE_CUELIST); }
static void on_load(lj_widget *w, void *ud) { (void)w; (void)ud; if (!running()) lj_win_open(LJ_WIN_SELECT_CUELIST); }
static void on_cmd_btn(lj_widget *w, void *ud)   /* sbCMD1..8 */
{
    int k = w && w->name ? atoi(w->name + 5) : 0;
    (void)ud;
    if (k >= 1 && k <= CL_RECENT && btn() == 1) lj_cuelist_form_apply_cmd(s.recent[k - 1]);
}
static void load_cue(uint32_t number)
{
    const lj_cue_form_host *h = lj_cue_form_get_host();
    if (!number) return;
    if (!h || !h->load_cue) { lj_log("cuelist: load cue %04u: no loader (headless)", (unsigned)number); return; }
    if (h->load_cue(h->ud, number, 0) != 0) lj_log("cuelist: load cue %04u failed", (unsigned)number);
}
/* the selection moved (the report view emits no OnChange: refresh notices it): the edit panel follows, Browse launches the
 * line's cue "Highlighting a line launches/trigs its cue" [03 s6.2] */
static void sel_changed(void)
{
    const lj_cl_line *l = line_of(sel_line());
    fill_edit();
    if (l && l->cue && !running() && lj_w_speedbutton_down(lj_window_find(s.win, "sbBrowse"))) load_cue(l->cue);
}
static void on_lv_change(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_refresh(); }
static void on_lv_dbl(lj_widget *w, void *ud) { const lj_cl_line *l = line_of(sel_line()); (void)w; (void)ud; if (l && lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, "LoadCueOnDblClick", 0)) load_cue(l->cue); }

/* a record dropped on a line [03 s6.2-6.3]: cue -> Cue/Trig, sequence -> Flash Seq, BG cue -> Load BGCue, cue list ->
 * TCuelistLinkOrImportForm "Cuelist Link or Import ?" (Link = a Link Cuelist line; Import = its lines inserted there) */
static struct { lj_window *win; uint32_t list, row; } lk;
static void lk_close(void) { if (lk.win) { lj_win_hide(lk.win); lj_window_destroy(lk.win); lk.win = NULL; } }
static void lk_click(lj_widget *w, void *ud)   /* BClick: bLink / bImport */
{
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_cl_line *l = line_of(lk.row);
    (void)ud;
    if (cl && l && !running() && w && w->name && !strcmp(w->name, "bLink")) { l->cmd = LJ_CL_LINK_CUELIST; l->ref = lk.list; l->param_ms = 0; l->param[0] = 0; modified(); }
    else if (cl && l && !running() && w && w->name && !strcmp(w->name, "bImport") && ctx()->lib) {
        lj_cuelist in;
        char path[LJ_PATH_CAP], err[160];
        uint32_t i;
        memset(&in, 0, sizeof in);
        if (lj_library_record_path(ctx()->lib, LJ_REC_CL, lk.list, path, sizeof path) == 0 && lj_cuelist_load(path, &in, err, sizeof err) == 0)
            for (i = 0; i < in.n; i++) lj_cuelist_insert(cl, lk.row - 1 + i, &in.lines[i]);
        else lj_log("cuelist: import %04u: %s", (unsigned)lk.list, err);
        free(in.lines);
        modified();
    }
    lk_close();
}
static void lk_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lk_close(); }
static const lj_binding lk_bindings[] = { { "FormCreate", lj_handler_noop, NULL }, { "BClick", lk_click, NULL }, { NULL, NULL, NULL } };

static void on_lv_drop(lj_widget *w, void *ud)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_cl_line *l;
    lj_widget *b;
    int kind, number, x, y, row;
    (void)ud;
    if (!cl || running() || lj_dnd_info(s.win, &kind, &number, &x, &y) != 0 || number <= 0) return;
    if (kind != LJ_REC_CUE && kind != LJ_REC_SEQ && kind != LJ_REC_BG && kind != LJ_REC_CL) return;
    row = lj_w_listview_row_at(w, y);
    if (row < 0) { lj_cl_line z; memset(&z, 0, sizeof z); lj_cuelist_insert(cl, cl->n, &z); row = (int)cl->n - 1; }   /* past the rows: a new last line */
    l = &cl->lines[row];
    if (kind == LJ_REC_CUE) { l->cue = (uint32_t)number; l->trig_mask = 0; }
    else if (kind == LJ_REC_SEQ) { l->cmd = LJ_CL_FLASH_SEQUENCE; l->ref = (uint32_t)number; l->param_ms = CL_FLASH_DFLT; l->param[0] = 0; }
    else if (kind == LJ_REC_BG) { l->cmd = LJ_CL_LOAD_BACKGROUND_CUE; l->ref = (uint32_t)number; l->param_ms = 0; l->param[0] = 0; }
    else if (!lk.win && (lk.win = lj_window_create(&lj_form_TCUELISTLINKORIMPORTFORM, lk_bindings)) != NULL) {
        lk.list = (uint32_t)number; lk.row = (uint32_t)row + 1;
        if ((b = lj_window_find(lk.win, "BitBtn3")) != NULL) { b->on_event = lk_cancel; b->ud = NULL; }   /* bkCancel carries no OnClick in the DFM */
        lj_win_show(lk.win, 0);
    }
    select_line((uint32_t)row + 1);
    modified();
}

/* ListPop */
static uint32_t popup_line(void) { return sel_line(); }
static void on_jump(lj_widget *w, void *ud) { (void)w; (void)ud; if (popup_line()) op(LJ_CL_OP_JUMP, popup_line()); }
static void on_run_menu(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    (void)w; (void)ud;
    if (!it || !it->name) return;
    if (!strcmp(it->name, "M_RunFromTop")) op(LJ_CL_OP_TOP, 0);
    else if (!strcmp(it->name, "M_RunFromLine")) op(LJ_CL_OP_RUN, popup_line() ? popup_line() : 1);
    else if (!strcmp(it->name, "M_RunToLine") && popup_line()) op(LJ_CL_OP_RUN_TO, popup_line());
}
static void on_set_cue(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_apply_cmd(LJ_CL_SET_CURRENT_CUE); }
static void on_set_trig(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_apply_cmd(LJ_CL_SET_CUE_TRIG); }
static void on_set_mark(lj_widget *w, void *ud)   /* P_SetMark1..4 (Tag 0..3) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    lj_cuelist *cl = lj_cuelist_form_list();
    (void)w; (void)ud;
    if (!it || !cl || running() || !sel_line() || it->tag < 0 || it->tag >= LJ_MARKS) return;
    cl->mark[it->tag] = sel_line();
    modified();
}
static void on_action(lj_widget *w, void *ud)   /* ActionExecute: every TAction of ActionList1 */
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_cl_line *l = sel();
    const char *a = it ? (it->action ? it->action : it->name) : NULL;
    int i;
    (void)w; (void)ud;
    if (!a || !cl) return;
    for (i = 0; i < LJ_COUNT_OF(cl_actions); i++) if (!strcmp(a, cl_actions[i].action)) { lj_cuelist_form_apply_cmd(cl_actions[i].cmd); return; }
    if (!strcmp(a, "aCuelistTrackingOn") || !strcmp(a, "aCuelistTrackingOff")) { if (!running()) { cl->tracking = !strcmp(a, "aCuelistTrackingOn"); modified(); } return; }
    if (!strcmp(a, "aCopy")) { if (line_of(sel_line())) { s.clip = *line_of(sel_line()); s.has_clip = 1; } return; }
    if (!l) return;
    if (!strcmp(a, "aCut")) { s.clip = *l; s.has_clip = 1; lj_cuelist_delete(cl, sel_line() - 1); }
    else if (!strcmp(a, "aPaste")) { if (!s.has_clip) return; lj_cuelist_insert(cl, sel_line() - 1, &s.clip); }   /* "dropped lines insert above the target" [03 s6.2] */
    else if (!strcmp(a, "aClearLine")) { uint32_t n = l->line; memset(l, 0, sizeof *l); l->line = n; }
    else if (!strcmp(a, "aClearCommand")) { l->cmd = 0; l->param_ms = 0; l->param[0] = 0; l->ref = 0; l->load_error = 0; }
    else if (!strcmp(a, "aClearCue")) { l->cue = 0; l->trig_mask = 0; }
    else if (!strcmp(a, "aAdvanceAuto") || !strcmp(a, "aAdvanceManual")) l->manual = (uint8_t)!strcmp(a, "aAdvanceManual");
    else { lj_log("cuelist: %s has no behaviour", a); return; }   /* the Offset / Sort items are greyed (cl_inert_items, ME-120) */
    fill_edit();
    modified();
}

/* main menu */
static void sync_checks(void)
{
    lj_menu *m = lj_window_menus(s.win);
    lj_ini *fi = ctx()->forms_ini;
    int i, mtc = lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, "MTCTimeout", 0), sync = lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, "SyncFrames", 0);
    for (i = 0; i < LJ_COUNT_OF(cl_pref_items); i++) lj_menu_set_checked(m, cl_pref_items[i], lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, cl_pref_keys[i], 0));
    for (i = 0; i < LJ_COUNT_OF(cl_mtc_items); i++) lj_menu_set_checked(m, cl_mtc_items[i], i == mtc);
    for (i = 0; i < LJ_COUNT_OF(cl_sync_items); i++) lj_menu_set_checked(m, cl_sync_items[i], cl_sync_frames[i] == sync);
    lj_menu_set_checked(m, "M_ViewEdit", lj_ini_geti(fi, CL_SEC, "ViewEditPanel", 1));
    lj_menu_set_checked(m, "M_ViewCommand", lj_ini_geti(fi, CL_SEC, "ViewEditCommand", 1));
}
static void apply_view(int relayout)
{
    lj_ini *fi = ctx()->forms_ini;
    int ed = lj_ini_geti(fi, CL_SEC, "ViewEditPanel", 1), cmd = lj_ini_geti(fi, CL_SEC, "ViewEditCommand", 1);
    lj_widget *w;
    if ((w = lj_window_find(s.win, "pEdit")) != NULL) w->visible = ed != 0;
    if ((w = lj_window_find(s.win, "Splitter2")) != NULL) w->visible = ed != 0;
    if ((w = lj_window_find(s.win, "Panel2")) != NULL) w->visible = cmd != 0;
    if ((w = lj_window_find(s.win, "Splitter1")) != NULL) w->visible = cmd != 0;
    if (!ed || !cmd || relayout) lj_widget_layout(lj_window_root(s.win));   /* the DFM rects stand otherwise (W0 rule; the alBottom order needs ME-100) */
    lj_window_invalidate_all(s.win);
    sync_checks();
}
static void on_pref(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    lj_ini *mi = ctx()->main_ini;
    int i;
    (void)w; (void)ud;
    if (!it || !it->name || !mi) return;
    for (i = 0; i < LJ_COUNT_OF(cl_pref_items); i++) if (!strcmp(it->name, cl_pref_items[i])) lj_ini_seti(mi, CL_SEC, cl_pref_keys[i], !lj_ini_geti(lj_ctx_get()->main_ini, CL_SEC, cl_pref_keys[i], 0));
    for (i = 0; i < LJ_COUNT_OF(cl_mtc_items); i++) if (!strcmp(it->name, cl_mtc_items[i])) lj_ini_seti(mi, CL_SEC, "MTCTimeout", i);
    sync_checks();
}
static void on_sync(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    int i;
    (void)w; (void)ud;
    for (i = 0; it && it->name && ctx()->main_ini && i < LJ_COUNT_OF(cl_sync_items); i++)
        if (!strcmp(it->name, cl_sync_items[i])) lj_ini_seti(ctx()->main_ini, CL_SEC, "SyncFrames", cl_sync_frames[i]);
    sync_checks();
}
static void on_view(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(s.win);
    lj_ini *fi = ctx()->forms_ini;
    (void)w; (void)ud;
    if (!it || !it->name || !fi) return;
    if (!strcmp(it->name, "M_ViewEdit")) lj_ini_seti(fi, CL_SEC, "ViewEditPanel", !lj_ini_geti(fi, CL_SEC, "ViewEditPanel", 1));
    else if (!strcmp(it->name, "M_ViewCommand")) lj_ini_seti(fi, CL_SEC, "ViewEditCommand", !lj_ini_geti(fi, CL_SEC, "ViewEditCommand", 1));
    else if (!strcmp(it->name, "M_DefaultColumns")) {
        lj_w_listview *v = (lj_w_listview *)s.lv;
        int c;
        for (c = 0; c < v->ncols; c++) v->col_w[c] = v->cols[c].width;
    }
    apply_view(1);
}
static void on_file_menu(lj_widget *w, void *ud) { (void)w; (void)ud; if (!running()) lj_cuelist_form_new(); }
static void on_autosave(lj_widget *w, void *ud) { (void)w; (void)ud; if (!running()) lj_desktop_run_action(LJ_ACT_LOAD_CUELIST, 0); }
/* ponytail: the LJ2 binary .cl importer is not in W3 (design 5.3 lists the import formats); the item logs */
static void on_import_old(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("cuelist: Import Old Format Cuelist is not available"); }
static void on_popup(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_refresh(); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_refresh(); }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_cuelist_form_close(); }

static const lj_binding cl_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "FormDeactivate", lj_handler_noop, NULL }, { "FormHide", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "sbCDTimeMouseDown", lj_handler_noop, NULL }, { "sbDATimeMouseDown", lj_handler_noop, NULL }, { "sbMPTimeMouseDown", lj_handler_noop, NULL }, { "sbMPTimeMouseUp", lj_handler_noop, NULL },
    { "sbTimeCodeMouseUp", on_tc, NULL }, { "sbLogAudioClick", on_log, NULL }, { "sbLockLineMouseDown", lj_handler_noop, NULL },
    { "sbGoMouseDown", on_go, NULL }, { "sbBackClick", on_back, NULL }, { "sbTopMouseDown", on_top, NULL }, { "sbRunMouseDown", on_run, NULL },
    { "sbMarkMouseUp", on_mark, NULL }, { "sbStopMouseDown", on_stop, NULL },
    { "sbNewMouseUp", on_new, NULL }, { "sbSaveMouseUp", on_save, NULL }, { "sbLoadMouseUp", on_load, NULL }, { "sbMouseDown", on_add, NULL },
    { "sbCMD1MouseDown", on_cmd_btn, NULL },
    { "lv1Change", on_lv_change, NULL }, { "lv1CustomDrawItem", lj_handler_noop, NULL } /* the line colours: lv_paint */, { "lv1DblClick", on_lv_dbl, NULL },
    { "lv1Enter", lj_handler_noop, NULL }, { "lv1Exit", lj_handler_noop, NULL }, { "lv1DragDrop", on_lv_drop, NULL }, { "lv1DragOver", lj_handler_noop, NULL }, { "lv1MouseDown", lj_handler_noop, NULL },
    { "Files1Click", lj_handler_noop, NULL }, { "FileMenuClick", on_file_menu, NULL }, { "M_LoadAutosaveClick", on_autosave, NULL }, { "M_ImportOldCuelistClick", on_import_old, NULL },
    { "ViewClick", on_view, NULL }, { "PrefsClick", on_pref, NULL }, { "Pop_MTCSyncFramesClick", lj_handler_noop, NULL }, { "syncFramesClick", on_sync, NULL },
    { "DigitalAudioTimecodeScaler1Click", lj_handler_noop, NULL } /* DA: greyed */, { "M_RecordtoLightCorderClick", lj_handler_noop, NULL } /* Visible = False in the DFM */,
    { "Help1Click", lj_handler_noop, NULL }, { "ListPopPopup", on_popup, NULL }, { "Pop_JumpToThisClick", on_jump, NULL }, { "M_RunClick", on_run_menu, NULL },
    { "Pop_SetCurrentCueClick", on_set_cue, NULL }, { "pop_SetCueTrigClick", on_set_trig, NULL }, { "P_SetMarkClick", on_set_mark, NULL },
    { "Timer1Timer", on_timer, NULL }, { "ActionExecute", on_action, NULL },
    { NULL, NULL, NULL } };

void lj_cuelist_form_new(void)   /* the app replaces show->current_cl (the runner lets go of the old one first) and asks when it was modified */
{
    if (!ctx()->show) return;
    if (s.lv) ((lj_w_listview *)s.lv)->index = -1;
    lj_desktop_run_action(LJ_ACT_NEW_CUELIST, 0);
    lj_cuelist_form_refresh();
}

static void restore_size(void)
{
    lj_ini *fi = ctx()->forms_ini;
    const lj_window_caption_state *c = lj_window_caption(s.win);
    int w = lj_ini_geti(fi, CL_SEC, "Width", 0), h = lj_ini_geti(fi, CL_SEC, "Height", 0);
    if (w > 0 && h > 0 && c) lj_window_resize(s.win, w - 2 * c->frame_w, h - 2 * c->frame_w - c->caption_h - c->menu_h);   /* Width / Height are outer sizes */
}

lj_window *lj_cuelist_form_open(void)
{
    lj_widget *w;
    int i;
    if (s.win) return s.win;
    if (!lj_cuelist_form_list()) { lj_log("cuelist form: no show"); return NULL; }
    memset(&s, 0, sizeof s);
    if ((s.win = lj_window_create(&lj_form_TCUELIST2FORM, cl_bindings)) == NULL) { lj_log("cuelist form: cannot create TCuelist2Form"); return NULL; }
    if ((s.edit = lj_window_create(&lj_form_TCLEDITPANELS, ed_bindings)) == NULL) { lj_cuelist_form_close(); return NULL; }
    for (w = lj_window_root(s.edit)->first; w; w = w->next) w->visible = 0;   /* the holder's pages show only when parented into pEdit */
    s.lv = lj_window_find(s.win, "lv1");
    s.pedit = lj_window_find(s.win, "pEdit");
    if (!s.lv || !s.pedit) { lj_cuelist_form_close(); return NULL; }
    s.lv_orig = s.lv->vt; s.lv_vt = *s.lv->vt; s.lv_vt.paint = lv_paint; s.lv->vt = &s.lv_vt;
    /* STOP takes Top / Run's place while the list runs (DFM Left 391 overlaps Back; measured from cuelistactive.gif, reduced:
     * STOP starts at Top's Left 421, "Both are replaced by STOP while active" [03 s6.2]) */
    if ((w = lj_window_find(s.win, "sbStop")) != NULL && lj_window_find(s.win, "sbTop")) w->x = lj_window_find(s.win, "sbTop")->x;
    /* ponytail: lj_window binds menu OnClick names only; an Action-linked item (Action = aWaitCD) runs its TAction's
     * OnExecute (ActionExecute) here until lj_window binds TAction rows itself (integrator) */
    {
        lj_menu *m = lj_window_menus(s.win);
        for (i = 0; m && i < m->nrt; i++) if (m->rt[i].item->action && !m->rt[i].fn) { m->rt[i].fn = on_action; m->rt[i].ud = NULL; }
    }
    /* design 8.5: the CD / DA / MP time-code sources greyed with the hint */
    for (i = LJ_TC_CD; i < LJ_TC_COUNT; i++) if ((w = lj_window_find(s.win, cl_tc_btn[i])) != NULL) { w->enabled = 0; w->hint = LJ_CL_NOT_AVAILABLE; }
    s.tc_src = lj_ini_geti(ctx()->forms_ini, CL_SEC, "TCSource", LJ_TC_PC_ELAPSED);
    if (s.tc_src < 0 || s.tc_src >= LJ_TC_CD) s.tc_src = LJ_TC_PC_ELAPSED;
    lj_w_speedbutton_set_down(lj_window_find(s.win, cl_tc_btn[s.tc_src]), 1);
    recent_load();
    restore_size();
    apply_view(0);
    s.shown_line = (uint32_t)-1;
    lj_cuelist_form_refresh();
    lj_win_show(s.win, 0);
    return s.win;
}

void lj_cuelist_form_close(void)
{
    lk_close();
    if (s.panel && s.edit) { lj_widget_remove(s.panel); lj_widget_add(lj_window_root(s.edit), s.panel); }   /* the page back to its holder before either tree is freed */
    if (s.win) { lj_win_hide(s.win); lj_window_destroy(s.win); }
    lj_window_destroy(s.edit);
    free(s.row); free(s.rowp);
    memset(&s, 0, sizeof s);
}
lj_window *lj_cuelist_form_get(void) { return s.win; }
const char *lj_cuelist_form_edit_panel(void) { return s.panel ? s.panel->name : NULL; }
int lj_cuelist_form_tc_src(void) { return s.tc_src; }

/* ---------------------------------------------------------------- Save Cuelist (TSaveCuelist2Form + TSaveAsCuelistForm) [03 s6.2] */
static struct { lj_window *win, *as; } sv;

int lj_cuelist_form_save(int mode, uint32_t number)
{
    lj_ctx *c = ctx();
    lj_cuelist *cl = lj_cuelist_form_list();
    char path[LJ_PATH_CAP];
    const char *bad = NULL;
    uint32_t keep;
    if (!cl || !c->lib) return -1;
    keep = cl->number;
    if (mode == 1 || (mode == 0 && !cl->number)) cl->number = lj_library_next_number(c->lib, LJ_REC_CL);
    else if (mode == 2) cl->number = number;
    if (!cl->number || (bad = lj_cuelist_validate(cl)) != NULL) { lj_log("save cuelist: %s", cl->number ? bad : "no number"); cl->number = keep; return -1; }
    if (lj_library_record_path(c->lib, LJ_REC_CL, cl->number, path, sizeof path) != 0 || lj_cuelist_save(path, cl) != 0) {
        lj_log("save cuelist: %04u not written", (unsigned)cl->number); cl->number = keep; return -1;
    }
    lj_ctx_set_modified(LJ_MOD_CL, 0);
    lj_cuelist_form_refresh();
    return 0;
}
static void sv_fill(void)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    char no[12];
    lj_widget *w;
    if (!sv.win || !cl) return;
    if (!cl->number || lj_number_text(cl->number, no, sizeof no) != 0) no[0] = 0;
    if ((w = lj_window_find(sv.win, "eCuelistNo")) != NULL) lj_w_edit_set_text(w, no);
    if ((w = lj_window_find(sv.win, "eCuelistName")) != NULL) lj_w_edit_set_text(w, cl->name);
    if ((w = lj_window_find(sv.win, "bSave")) != NULL) { w->enabled = cl->number != 0; lj_widget_invalidate(w); }
    lj_menu_set_checked(lj_window_menus(sv.win), "M_AutoClose", lj_ini_geti(lj_ctx_get()->forms_ini, "SAVECUELIST2FORM", "AutoClose", 1));
    lj_menu_set_checked(lj_window_menus(sv.win), "M_EnterSave", lj_ini_geti(lj_ctx_get()->forms_ini, "SAVECUELIST2FORM", "SaveOnEnter", 0));
}
static void sv_take_name(lj_window *from, const char *field)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_widget *e = from ? lj_window_find(from, field) : NULL;
    if (cl && e && lj_w_edit_text(e)) lj_str_copy(cl->name, sizeof cl->name, lj_w_edit_text(e));
}
static void sv_done(int rc) { if (rc == 0 && lj_ini_geti(lj_ctx_get()->forms_ini, "SAVECUELIST2FORM", "AutoClose", 1)) lj_save_cuelist_close(); else sv_fill(); }
static void sv_save(lj_widget *w, void *ud)
{
    (void)ud;
    sv_take_name(sv.win, "eCuelistName");
    sv_done(lj_cuelist_form_save(w && w->name && !strcmp(w->name, "bSaveNew") ? 1 : 0, 0));
}
static void as_close(void) { if (sv.as) { lj_win_hide(sv.as); lj_window_destroy(sv.as); sv.as = NULL; } }
static void as_no_change(lj_widget *w, void *ud)
{
    lj_widget *b = sv.as ? lj_window_find(sv.as, "bSave") : NULL;
    const char *t = lj_w_edit_text(w);
    (void)ud;
    if (b) { b->enabled = t && atoi(t) > 0 && atoi(t) <= 9999; lj_widget_invalidate(b); }
}
static void as_button(lj_widget *w, int ev, void *ud)
{
    lj_widget *no = sv.as ? lj_window_find(sv.as, "eCuelistNo") : NULL;
    int rc;
    if (ev != LJ_WEV_CLICK) return;
    if (ud && no && lj_w_edit_text(no)) {   /* bSave: ModalResult = 1 */
        sv_take_name(sv.as, "ECueListName");
        rc = lj_cuelist_form_save(2, (uint32_t)atoi(lj_w_edit_text(no)));
        as_close();
        sv_done(rc);
        return;
    }
    (void)w;
    as_close();
}
static const lj_binding as_bindings[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "eCuelistNoChange", as_no_change, NULL }, { NULL, NULL, NULL } };
static void sv_save_as(lj_widget *w, void *ud)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    lj_widget *b;
    (void)w; (void)ud;
    if (sv.as || !cl) return;
    if ((sv.as = lj_window_create(&lj_form_TSAVEASCUELISTFORM, as_bindings)) == NULL) return;
    if ((b = lj_window_find(sv.as, "ECueListName")) != NULL) lj_w_edit_set_text(b, cl->name);
    if ((b = lj_window_find(sv.as, "bSave")) != NULL) { b->on_event = as_button; b->ud = (void *)1; }   /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((b = lj_window_find(sv.as, "BitBtn2")) != NULL) { b->on_event = as_button; b->ud = NULL; }
    lj_win_show(sv.as, 0);
}
static void sv_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(sv.win);
    lj_ini *fi = ctx()->forms_ini;
    (void)w; (void)ud;
    if (!it || !it->name || !fi) return;
    if (!strcmp(it->name, "M_AutoClose")) lj_ini_seti(fi, "SAVECUELIST2FORM", "AutoClose", !lj_ini_geti(lj_ctx_get()->forms_ini, "SAVECUELIST2FORM", "AutoClose", 1));
    else if (!strcmp(it->name, "M_EnterSave")) lj_ini_seti(fi, "SAVECUELIST2FORM", "SaveOnEnter", !lj_ini_geti(lj_ctx_get()->forms_ini, "SAVECUELIST2FORM", "SaveOnEnter", 0));
    sv_fill();
}
/* sv_key = lj_handler_noop: ponytail: Save on [Enter] needs the key of FormKeyDown, which lj_window does not hand to form handlers */
static void sv_on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_save_cuelist_close(); }
static const lj_binding sv_bindings[] = {
    { "bSaveClick", sv_save, NULL }, { "bSaveAsClick", sv_save_as, NULL }, { "M_EnterSaveClick", sv_prefs, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "FormClose", sv_on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };
lj_window *lj_save_cuelist_open(void)
{
    if (sv.win) return sv.win;
    if ((sv.win = lj_window_create(&lj_form_TSAVECUELIST2FORM, sv_bindings)) == NULL) { lj_log("cuelist form: cannot create TSaveCuelist2Form"); return NULL; }
    sv_fill();
    lj_win_show(sv.win, 0);
    return sv.win;
}
void lj_save_cuelist_close(void) { as_close(); if (sv.win) { lj_win_hide(sv.win); lj_window_destroy(sv.win); sv.win = NULL; } }
lj_window *lj_save_cuelist_get(void) { return sv.win; }
void lj_save_cuelist_refresh(void) { }

/* ---------------------------------------------------------------- Select Cuelist (TSelectCuelist2Form) [03 s6.2] */
#define SL_MAX 1024   /* ponytail: the first 1024 lists of the library are listed */
static struct { lj_window *win; uint32_t no[SL_MAX]; int n; char row[SL_MAX][80]; const char *rowp[SL_MAX]; } sl;
static uint32_t sl_sel(void) { lj_widget *lv = sl.win ? lj_window_find(sl.win, "Lv1") : NULL; int i = lv ? lj_w_listview_index(lv) : -1; return i >= 0 && i < sl.n ? sl.no[i] : 0; }
static void sl_fill(void)
{
    lj_ctx *c = ctx();
    lj_cuelist tmp;
    char path[LJ_PATH_CAP], err[160];
    int i;
    lj_widget *lv = lj_window_find(sl.win, "Lv1");
    sl.n = c->lib ? lj_library_list_records(c->lib, LJ_REC_CL, sl.no, SL_MAX) : 0;
    if (sl.n < 0) sl.n = 0;
    for (i = 0; i < sl.n; i++) {
        memset(&tmp, 0, sizeof tmp);
        if (lj_library_record_path(c->lib, LJ_REC_CL, sl.no[i], path, sizeof path) == 0 && lj_cuelist_load(path, &tmp, err, sizeof err) == 0)
            snprintf(sl.row[i], sizeof sl.row[i], "%04u\t%s\t%u\t", (unsigned)sl.no[i], tmp.name, (unsigned)tmp.n);   /* Modified: blank until a core mtime helper exists (LORD_FIDELITY-12) */
        else snprintf(sl.row[i], sizeof sl.row[i], "%04u\tload error\t\t", (unsigned)sl.no[i]);
        free(tmp.lines);
        sl.rowp[i] = sl.row[i];
    }
    if (lv) lj_w_listview_set_items(lv, sl.rowp, sl.n);
}
static void sl_load(lj_widget *w, void *ud) { (void)w; (void)ud; if (sl_sel() && !running()) lj_desktop_run_action(LJ_ACT_LOAD_CUELIST, (int)sl_sel()); }
static void sl_delete(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DELETE_CL); }
static void sl_close_ev(lj_widget *w, void *ud) { (void)w; (void)ud; lj_select_cuelist_close(); }
static void sl_activate(lj_widget *w, void *ud) { (void)w; (void)ud; sl_fill(); }
static const lj_binding sl_bindings[] = {
    { "FormActivate", sl_activate, NULL }, { "FormClose", sl_close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "Lv1ColumnClick", lj_handler_noop, NULL } /* ponytail: sorting by column, W4 */, { "Lv1Compare", lj_handler_noop, NULL }, { "Lv1DblClick", sl_load, NULL },
    { "Help1Click", lj_handler_noop, NULL }, { "Cl2PpopupPopup", lj_handler_noop, NULL }, { "pop_LoadCuelistClick", sl_load, NULL }, { "pop_DeleteCuelistClick", sl_delete, NULL },
    { NULL, NULL, NULL } };
lj_window *lj_select_cuelist_open(void)
{
    if (sl.win) return sl.win;
    if ((sl.win = lj_window_create(&lj_form_TSELECTCUELIST2FORM, sl_bindings)) == NULL) { lj_log("cuelist form: cannot create TSelectCuelist2Form"); return NULL; }
    sl_fill();
    lj_win_show(sl.win, 0);
    return sl.win;
}
void lj_select_cuelist_close(void) { if (sl.win) { lj_win_hide(sl.win); lj_window_destroy(sl.win); sl.win = NULL; } }
lj_window *lj_select_cuelist_get(void) { return sl.win; }
void lj_select_cuelist_refresh(void) { }

/* ---------------------------------------------------------------- Set Cue Trig (TClSetCueTrigForm): slots 1-12 of the line [03 s6.2] */
static lj_window *tr_win;
static void tr_sync(void)
{
    const lj_cl_line *l = line_of(sel_line());
    int i;
    for (i = 0; tr_win && i < LJ_CUE_SLOTS; i++) {
        char nm[24];
        lj_w_cap *b;
        snprintf(nm, sizeof nm, "SpeedButton%d", i + 1);
        if ((b = (lj_w_cap *)lj_window_find(tr_win, nm)) == NULL) continue;
        b->down = l && (l->trig_mask >> i & 1u);   /* shown latched; the DFM gives these no GroupIndex */
        lj_widget_invalidate(&b->w);
    }
}
static void tr_click(lj_widget *w, void *ud)
{
    lj_cl_line *l = sel();
    int k = w && w->name ? atoi(w->name + 11) : 0;
    (void)ud;
    if (!l || k < 1 || k > LJ_CUE_SLOTS) return;
    l->trig_mask ^= (uint16_t)(1u << (k - 1));
    l->cue = 0;   /* a cue or trigs, never both [03 s6.1] */
    tr_sync();
    modified();
}
static void tr_clear(lj_widget *w, void *ud) { lj_cl_line *l = sel(); (void)w; (void)ud; if (l) { l->trig_mask = 0; tr_sync(); modified(); } }
static const lj_binding tr_bindings[] = { { "CueTrigMouseUp", tr_click, NULL }, { "sbClearAllMouseUp", tr_clear, NULL }, { NULL, NULL, NULL } };
lj_window *lj_cl_set_cue_trig_open(void)
{
    if (tr_win) return tr_win;
    if ((tr_win = lj_window_create(&lj_form_TCLSETCUETRIGFORM, tr_bindings)) == NULL) return NULL;
    tr_sync();
    lj_win_show(tr_win, 0);
    return tr_win;
}
void lj_cl_set_cue_trig_close(void) { if (tr_win) { lj_win_hide(tr_win); lj_window_destroy(tr_win); tr_win = NULL; } }
lj_window *lj_cl_set_cue_trig_get(void) { return tr_win; }
void lj_cl_set_cue_trig_refresh(void) { }

/* ---------------------------------------------------------------- Log TimeCode (TLogTimeCodeForm) [03 s6.4] */
static struct { lj_window *win; uint64_t t0; } lg;
static int lg_count(void) { const lj_cl_status *st = ctx()->cl; return st && st->nlog > s.log_base ? st->nlog - s.log_base : 0; }
/* the logs as wait lines of the shown source: PC elapsed = the time since the previous log, PC clock and MIDI = absolute */
static void lg_line(lj_cl_line *l, int i)
{
    const lj_cl_status *st = ctx()->cl;
    uint32_t ms = st->log_ms[s.log_base + i];
    memset(l, 0, sizeof *l);
    l->cmd = s.tc_src == LJ_TC_MIDI ? LJ_CL_WAIT_MTC : s.tc_src == LJ_TC_PC_CLOCK ? LJ_CL_WAIT_PC_CLOCK : LJ_CL_WAIT_ELAPSED;
    l->param_ms = l->cmd == LJ_CL_WAIT_ELAPSED && i > 0 ? ms - st->log_ms[s.log_base + i - 1] : ms;
    if (l->cmd == LJ_CL_WAIT_MTC) lj_str_copy(l->param, sizeof l->param, "25");
}
static void lg_apply(lj_widget *w, void *ud)   /* bAddLogs (ud 0) / bInsertLogs (1) / bApplyLogs (2) */
{
    lj_cuelist *cl = lj_cuelist_form_list();
    uint32_t at = sel_line();
    int i, n = lg_count(), mode = (int)(intptr_t)ud;
    lj_cl_line l;
    (void)w;
    if (!cl || running() || !n) return;
    for (i = 0; i < n; i++) {
        lg_line(&l, i);
        if (mode == 2) { lj_cl_line *t = line_of(at + (uint32_t)i); if (!t) break; t->cmd = l.cmd; t->param_ms = l.param_ms; lj_str_copy(t->param, sizeof t->param, l.param); }
        else lj_cuelist_insert(cl, mode == 1 && at ? at - 1 + (uint32_t)i : cl->n, &l);
    }
    if (lj_ini_geti(lj_ctx_get()->forms_ini, "LOGTIMECODE", "AutoClear", 0)) s.log_base = ctx()->cl->nlog;
    modified();
    lj_log_timecode_refresh();
}
static void lg_log(lj_widget *w, void *ud) { (void)w; (void)ud; op(LJ_CL_OP_LOG, 0); }
static void lg_clear(lj_widget *w, void *ud) { (void)w; (void)ud; if (ctx()->cl) s.log_base = ctx()->cl->nlog; lj_log_timecode_refresh(); }
static void lg_start(lj_widget *w, void *ud) { (void)w; (void)ud; lg.t0 = lj_clock_us(); }
static void lg_prefs(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(lg.win);
    (void)w; (void)ud;
    if (!it || !it->name || !ctx()->forms_ini) return;
    if (!strcmp(it->name, "M_AutoInsert")) lj_ini_seti(ctx()->forms_ini, "LOGTIMECODE", "AutoInsert", !lj_ini_geti(lj_ctx_get()->forms_ini, "LOGTIMECODE", "AutoInsert", 0));   /* ponytail: kept, not acted on (03 s6.4 gives no rule) */
    if (!strcmp(it->name, "M_AutoClear")) lj_ini_seti(ctx()->forms_ini, "LOGTIMECODE", "AutoClear", !lj_ini_geti(lj_ctx_get()->forms_ini, "LOGTIMECODE", "AutoClear", 0));
    lj_log_timecode_refresh();
}
static void lg_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log_timecode_refresh(); }
static void lg_close_ev(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log_timecode_close(); }
static const lj_binding lg_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lg_close_ev, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "FormDeactivate", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL } /* [CTRL] logs: the hotkey Log Timecode does the same */,
    { "bLogMouseDown", lg_log, NULL }, { "bStartElapsedClick", lg_start, NULL }, { "bAddLogsClick", lg_apply, (void *)0 },
    { "bClearLogsClick", lg_clear, NULL }, { "bInsertLogsClick", lg_apply, (void *)1 }, { "bApplyLogsClick", lg_apply, (void *)2 },
    { "M_AutoClearClick", lg_prefs, NULL }, { "Help1Click", lj_handler_noop, NULL }, { "Timer1Timer", lg_timer, NULL },
    { NULL, NULL, NULL } };
lj_window *lj_log_timecode_open(void)
{
    if (lg.win) return lg.win;
    if ((lg.win = lj_window_create(&lj_form_TLOGTIMECODEFORM, lg_bindings)) == NULL) return NULL;
    lg.t0 = lj_clock_us();
    lj_log_timecode_refresh();
    lj_win_show(lg.win, 0);
    return lg.win;
}
void lj_log_timecode_close(void) { if (lg.win) { lj_win_hide(lg.win); lj_window_destroy(lg.win); lg.win = NULL; } }
lj_window *lj_log_timecode_get(void) { return lg.win; }
void lj_log_timecode_refresh(void)
{
    char b[32];
    uint64_t e;
    if (!lg.win) return;
    snprintf(b, sizeof b, "%03d Logs Total", lg_count());   /* the DFM caption "000 Logs Total" */
    lj_w_set_caption(lj_window_find(lg.win, "lLogs"), b);
    e = (lj_clock_us() - lg.t0) / 1000000u;
    snprintf(b, sizeof b, "%02u:%02u:%02u", (unsigned)(e / 3600 % 100), (unsigned)(e / 60 % 60), (unsigned)(e % 60));
    lj_w_set_caption(lj_window_find(lg.win, "lElapsed"), b);
    lj_menu_set_checked(lj_window_menus(lg.win), "M_AutoInsert", lj_ini_geti(lj_ctx_get()->forms_ini, "LOGTIMECODE", "AutoInsert", 0));
    lj_menu_set_checked(lj_window_menus(lg.win), "M_AutoClear", lj_ini_geti(lj_ctx_get()->forms_ini, "LOGTIMECODE", "AutoClear", 0));
}

/* ---------------------------------------------------------------- Select Function (THotkeyFunctionDialog) */
static struct { lj_window *win; int fn[LJ_FN_COUNT]; char row[LJ_FN_COUNT][64]; const char *rowp[LJ_FN_COUNT]; int n; } hf;
static void hf_ok(lj_widget *w, int ev, void *ud)
{
    lj_widget *lv = hf.win ? lj_window_find(hf.win, "lvFunctions") : NULL;
    int i = lv ? lj_w_listview_index(lv) : -1;
    lj_cl_line *l = sel();
    (void)w;
    if (ev != LJ_WEV_CLICK && ev != LJ_WEV_DBLCLICK) return;
    if (ud && l && l->cmd == LJ_CL_HOTKEY_COMMAND && i >= 0 && i < hf.n) { l->param_ms = (uint32_t)hf.fn[i]; fill_edit(); modified(); }
    lj_hotkey_function_close();
}
static void hf_dbl(lj_widget *w, void *ud) { (void)ud; hf_ok(w, LJ_WEV_DBLCLICK, (void *)1); }
static const lj_binding hf_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "lvFunctionsChange", lj_handler_noop, NULL }, { "lvFunctionsDblClick", hf_dbl, NULL },
    { "lvFunctionsDrawItem", lj_handler_noop, NULL } /* OwnerDraw: the stock report row */, { NULL, NULL, NULL } };
lj_window *lj_hotkey_function_open(void)
{
    lj_widget *b;
    int f;
    if (hf.win) return hf.win;
    if ((hf.win = lj_window_create(&lj_form_THOTKEYFUNCTIONDIALOG, hf_bindings)) == NULL) return NULL;
    for (hf.n = 0, f = 1; f < LJ_FN_COUNT; f++) {
        if (lj_fn_table[f].flags & LJ_FNF_EXT) continue;   /* not one of LJ2's functions */
        if (lj_fn_caption((enum lj_fn)f, hf.row[hf.n], sizeof hf.row[hf.n]) != 0) continue;
        hf.fn[hf.n] = f; hf.rowp[hf.n] = hf.row[hf.n]; hf.n++;
    }
    if ((b = lj_window_find(hf.win, "lvFunctions")) != NULL) lj_w_listview_set_items(b, hf.rowp, hf.n);
    if ((b = lj_window_find(hf.win, "bOk")) != NULL) { b->on_event = hf_ok; b->ud = (void *)1; }      /* bkOK / bkCancel carry no OnClick in the DFM */
    if ((b = lj_window_find(hf.win, "BitBtn1")) != NULL) { b->on_event = hf_ok; b->ud = NULL; }
    lj_win_show(hf.win, 0);
    return hf.win;
}
void lj_hotkey_function_close(void) { if (hf.win) { lj_win_hide(hf.win); lj_window_destroy(hf.win); hf.win = NULL; } }
lj_window *lj_hotkey_function_get(void) { return hf.win; }
void lj_hotkey_function_refresh(void) { }

/* ---------------------------------------------------------------- hotkeys [03 s6.5] */
static void fn_cl(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    lj_cuelist *cl = lj_cuelist_form_list();
    (void)prog; (void)ud;
    if (!edge) return;
    if (fn >= LJ_FN_CUELIST_GOTO_MARK_1 && fn <= LJ_FN_CUELIST_GOTO_MARK_4) { op(LJ_CL_OP_MARK_GOTO, (uint32_t)(fn - LJ_FN_CUELIST_GOTO_MARK_1) + 1); return; }
    if (fn >= LJ_FN_CUELIST_RUN_FROM_MARK_1 && fn <= LJ_FN_CUELIST_RUN_FROM_MARK_4) { op(LJ_CL_OP_MARK_RUN, (uint32_t)(fn - LJ_FN_CUELIST_RUN_FROM_MARK_1) + 1); return; }
    switch (fn) {
    case LJ_FN_CUELIST_CONTROL:          lj_win_toggle(LJ_WIN_CUELIST_FORM); break;
    case LJ_FN_TOGGLE_LIST_OF_CUELISTS:  lj_win_toggle(LJ_WIN_SELECT_CUELIST); break;
    case LJ_FN_QUICK_SAVE_CUELIST:       if (cl && cl->number) lj_cuelist_form_save(0, 0); else lj_win_open(LJ_WIN_SAVE_CUELIST); break;
    case LJ_FN_CUELIST_START_TOP:        op(LJ_CL_OP_TOP, 0); break;
    case LJ_FN_CUELIST_START_SELECTED:   op(LJ_CL_OP_RUN, sel_line() ? sel_line() : 1); break;
    case LJ_FN_CUELIST_GO:               op(LJ_CL_OP_GO, 0); break;   /* "if not running, acts as Run from top": the runner's GO on IDLE */
    case LJ_FN_CUELIST_BACK:             op(LJ_CL_OP_BACK, 0); break;
    case LJ_FN_CUELIST_STOP:             op(LJ_CL_OP_STOP, 0); break;
    case LJ_FN_CUELIST_STOP_AND_CLEAR:   op(LJ_CL_OP_STOP_CLEAR, 0); break;
    case LJ_FN_LOG_TIMECODE:             op(LJ_CL_OP_LOG, 0); break;
    case LJ_FN_CUELIST_SELECT_PREVIOUS: case LJ_FN_CUELIST_SELECT_NEXT: {   /* PROPOSAL rig 03 s6.5: the neighbouring file of the library is loaded */
        lj_ctx *c = ctx();
        int i, n = c->lib ? lj_library_list_records(c->lib, LJ_REC_CL, sl.no, SL_MAX) : 0;
        for (i = 0; i < n; i++) if (cl && sl.no[i] == cl->number) break;
        i = fn == LJ_FN_CUELIST_SELECT_NEXT ? (i < n ? i + 1 : 0) : (i < n ? i - 1 : n - 1);
        if (!running() && i >= 0 && i < n) lj_desktop_run_action(LJ_ACT_LOAD_CUELIST, (int)sl.no[i]);
        break; }
    case LJ_FN_CUELIST_SELECT_START:     op(LJ_CL_OP_TOP, 0); break;
    default: break;
    }
}

void lj_cuelist_form_bind_fns(void)
{
    static const enum lj_fn rows[] = {
        LJ_FN_CUELIST_CONTROL, LJ_FN_TOGGLE_LIST_OF_CUELISTS, LJ_FN_QUICK_SAVE_CUELIST, LJ_FN_CUELIST_START_TOP, LJ_FN_CUELIST_START_SELECTED,
        LJ_FN_CUELIST_GO, LJ_FN_CUELIST_BACK, LJ_FN_CUELIST_STOP, LJ_FN_CUELIST_STOP_AND_CLEAR,
        LJ_FN_CUELIST_GOTO_MARK_1, LJ_FN_CUELIST_GOTO_MARK_2, LJ_FN_CUELIST_GOTO_MARK_3, LJ_FN_CUELIST_GOTO_MARK_4,
        LJ_FN_CUELIST_RUN_FROM_MARK_1, LJ_FN_CUELIST_RUN_FROM_MARK_2, LJ_FN_CUELIST_RUN_FROM_MARK_3, LJ_FN_CUELIST_RUN_FROM_MARK_4,
        LJ_FN_LOG_TIMECODE, LJ_FN_CUELIST_SELECT_PREVIOUS, LJ_FN_CUELIST_SELECT_NEXT, LJ_FN_CUELIST_SELECT_START };
    int i;
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_cl, NULL);
}
