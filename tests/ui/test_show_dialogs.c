/* test_show_dialogs.c - `lj4evr_tests test_show_dialogs` (W4 dlg-show): every SHOW id of lj_show_dialogs.c headless, each opened from its
 * documented entry point where that entry point lives in a dlg-show file, against a five-scene current sequence, a small library on disk
 * (sequence 0002, cues 0001 plain / 0002 / 0003 transparent, BG cue 01, cue list 0001) and five MAC 500s (profile 12) for Position Options.
 *  - ME-16: every window builds with its DFM handlers bound and nothing logs "has no behaviour file" (the sink counts);
 *  - Delete Scene(s) from the Sequence window's Delete (sb6), Insert Scene(s) from Insert (sb5): copies, current range, another sequence (F29);
 *  - Sequence Slot Labels from the Cue control menu: forms.ini [CUE] SlotName0 and the grid's "(Intensity)";
 *  - Cue Builder from the Cue-Builder menu and the Toggle Cue Builder hotkey: Seq Capture, preset GO into the cue, the stack rule, stack GO;
 *  - Select BG Cue (Cue control sbLoadBGCue), Select Cue (Cuelist Set Current Cue without a saved cue), Select Startup Cuelist, Start Sequence at;
 *  - ME-120: the three Offset / Sort items enabled; Sort MIDI wait times sorts; Offset MIDI / PC with Check Changes' "Offsets Ok" /
 *    "Overflow/UnderFlow (n Lines)" and the clamp; ME-11: the CD / DA / MP offset families greyed;
 *  - Position Options: Mirror Pan / Mirror Tilt / Swap on the selection, Pan 360 greyed; it and Fan Out close with the Movement control (a script row);
 *  - Static Slot Name after Record Current Scene; D11: statics_control opens Statics 2, save_static / statics_replace / save_preset open nothing;
 *  - the 12 Q9 prompts of this track through `assert_prompt` script rows (cue slots, Statics 2), the confirm answered by `click confirm.BOk`;
 *  - goldens W4SD_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_uiscript.h"
#include "controls/lj_ctl.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "model/lj_statics.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_numedit.h"
#include "ui/widgets/lj_w_radiobutton.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_cuelist_form.h"
#include "windows/lj_movemacro_editor.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_show_dialogs.h"
#include "windows/lj_statics2_form.h"
#include "windows/lj_statics2_prefs.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_show_dialogs_tmp/"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static int me16;
static void sink(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "no binding for handler") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler") || strstr(line, "has no behaviour file")) { me16++; printf("ME-16: %s\n", line); }
    if (strstr(line, "SCRIPT FAIL")) printf("%s\n", line);
}
static lj_widget *in(enum lj_win id, const char *n) { return lj_win_get(id) ? lj_window_find(lj_win_get(id), n) : NULL; }
static void menu_fire(lj_window *win, const char *name)
{
    lj_menu *m = win ? lj_window_menus(win) : NULL;
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, name) : NULL;
    CHECK(rt && rt->fn);
    if (!rt || !rt->fn) return;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
}
static int menu_en(lj_window *win, const char *name) { lj_menu_rt *rt = win ? lj_menu_rt_named(lj_window_menus(win), name) : NULL; return rt ? rt->enabled : -1; }
static void click(lj_widget *w) { CHECK(w != NULL); if (w) lj_w_click(w); }
static void set_text(lj_widget *w, const char *t) { CHECK(w != NULL); if (w) lj_w_edit_set_text(w, t); }
static const char *prompt(void) { lj_window *m = lj_win_message_window(); return m ? lj_w_caption(lj_window_find(m, "Label1")) : ""; }
static void prompt_ok(void) { lj_window *m = lj_win_message_window(); if (m) click(lj_window_find(m, "BOk")); }

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(enum lj_win id, const char *name)
{
    lj_window *w = lj_win_get(id);
    char path[1024], tmp[256];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, TMP "%s.bmp", name);
    lj_window_invalidate_all(w);
    lj_window_paint(w);
    if (lj_bmp_write(lj_window_surface(w), upd && upd[0] == '1' ? path : tmp) != 0) { fails++; printf("FAIL golden %s: write\n", name); return; }
    if (upd && upd[0] == '1') { printf("  golden %s written\n", name); return; }
    rc = same_file(tmp, path);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (new render at %s)\n", name, rc == 0 ? "differs" : "missing", tmp); }
}

static int run_script(const char *name, const char *text)   /* a UI script row (docs/ui-script.md), run headless to the end */
{
    char path[256];
    FILE *f;
    uint64_t t0 = lj_clock_us();
    int r;
    snprintf(path, sizeof path, TMP "%s.lj4s", name);
    if ((f = fopen(path, "wb")) == NULL) return -1;
    fputs(text, f); fclose(f);
    if ((r = lj_uiscript_load(path)) != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 10000000u) { }
    return r;
}

static lj_show *show; static lj_library lib;
static lj_sequence *seq5(void) { return show->current_seq; }
static uint8_t tag(int i) { return seq5()->scenes[i].val[0]; }   /* scene identity: channel 1 holds the original scene number */

static void save_records(void)
{
    char path[LJ_PATH_CAP];
    lj_sequence *s = lj_sequence_new_rec();
    lj_cue *q = lj_cue_new_rec();
    lj_bgcue b;
    lj_cuelist *cl = lj_cuelist_new_rec();
    int i;
    s->number = 2; lj_str_copy(s->name, sizeof s->name, "Chase");
    lj_sequence_alloc(s, 3);
    for (i = 0; i < 3; i++) { s->scenes[i].val[0] = (uint8_t)(100 + i); s->scenes[i].state[0] = LJ_SNAP; }
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 2, path, sizeof path) == 0 && lj_sequence_save(path, s) == 0);
    s->number = 5; lj_str_copy(s->name, sizeof s->name, "Strobes");
    CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 5, path, sizeof path) == 0 && lj_sequence_save(path, s) == 0);
    lj_sequence_free(s);
    for (i = 1; i <= 3; i++) {
        q->number = (uint32_t)i; q->transparent = (uint8_t)(i > 1);
        lj_str_copy(q->name, sizeof q->name, i == 1 ? "Base" : i == 2 ? "Flash" : "Worms");
        CHECK(lj_library_record_path(&lib, LJ_REC_CUE, (uint32_t)i, path, sizeof path) == 0 && lj_cue_save(path, q) == 0);
    }
    lj_cue_free(q);
    memset(&b, 0, sizeof b); b.number = 1; lj_str_copy(b.name, sizeof b.name, "House");
    CHECK(lj_library_record_path(&lib, LJ_REC_BG, 1, path, sizeof path) == 0 && lj_bgcue_save(path, &b) == 0);
    cl->number = 1; lj_str_copy(cl->name, sizeof cl->name, "Show");
    CHECK(lj_library_record_path(&lib, LJ_REC_CL, 1, path, sizeof path) == 0 && lj_cuelist_save(path, cl) == 0);
    lj_cuelist_free(cl);
}

static uint32_t loads[8]; static int nloads;
static int stub_load(void *ud, uint32_t n, int edit) { (void)ud; (void)edit; if (nloads < 8) loads[nloads++] = n; return 0; }
static uint32_t picked; static void pick_cb(uint32_t n, void *ud) { (void)ud; picked = n; }
static int started; static void start_cb(int v, void *ud) { (void)ud; started = v; }

static void test_scenes(void)
{
    lj_window *sf = lj_seq_form_open();
    int i;
    CHECK(sf != NULL);
    lj_ctx_get()->current_scene = 1;
    click(lj_window_find(sf, "sb6"));                          /* Sequence window Delete */
    CHECK(lj_win_get(LJ_WIN_DELETE_SCENE) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_DELETE_SCENE))->text, "Delete Scene(s)"));
    CHECK(!strcmp(lj_w_edit_text(in(LJ_WIN_DELETE_SCENE, "eFrom")), "2") && !strcmp(lj_w_edit_text(in(LJ_WIN_DELETE_SCENE, "eTo")), "2"));
    golden(LJ_WIN_DELETE_SCENE, "W4SD_DELETE_SCENE");
    set_text(in(LJ_WIN_DELETE_SCENE, "eTo"), "3");
    click(in(LJ_WIN_DELETE_SCENE, "BOk"));
    CHECK(lj_win_get(LJ_WIN_DELETE_SCENE) == NULL && seq5()->n == 3 && tag(0) == 1 && tag(1) == 4 && tag(2) == 5);
    click(lj_window_find(sf, "sb6"));
    click(in(LJ_WIN_DELETE_SCENE, "bCancel"));                 /* Cancel: nothing deleted */
    CHECK(lj_win_get(LJ_WIN_DELETE_SCENE) == NULL && seq5()->n == 3);

    lj_ctx_get()->current_scene = 1;                           /* scene "4" */
    click(lj_window_find(sf, "sb5"));                          /* Sequence window Insert: two copies of the current scene before it (F29) */
    CHECK(lj_win_get(LJ_WIN_INSERT_SCENES) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_INSERT_SCENES))->text, "Insert Scene(s) before current scene"));
    CHECK(in(LJ_WIN_INSERT_SCENES, "eSceneCount")->enabled && !in(LJ_WIN_INSERT_SCENES, "ESeqFrom")->enabled);
    golden(LJ_WIN_INSERT_SCENES, "W4SD_INSERT_SCENES");
    set_text(in(LJ_WIN_INSERT_SCENES, "eSceneCount"), "  2");
    click(in(LJ_WIN_INSERT_SCENES, "BOk"));
    CHECK(seq5()->n == 5 && tag(0) == 1 && tag(1) == 4 && tag(2) == 4 && tag(3) == 4 && tag(4) == 5 && lj_ctx_get()->current_scene == 1);
    click(lj_window_find(sf, "sb5"));                          /* Insert from current sequence 1..2 before scene index 1 */
    lj_w_radiobutton_click(in(LJ_WIN_INSERT_SCENES, "CopyFromCurrent"));
    CHECK(in(LJ_WIN_INSERT_SCENES, "eCurrentFrom")->enabled && !in(LJ_WIN_INSERT_SCENES, "eSceneCount")->enabled);
    set_text(in(LJ_WIN_INSERT_SCENES, "eCurrentFrom"), "  1"); set_text(in(LJ_WIN_INSERT_SCENES, "eCurrentTo"), "  2");
    click(in(LJ_WIN_INSERT_SCENES, "BOk"));
    CHECK(seq5()->n == 7 && tag(1) == 1 && tag(2) == 4 && tag(3) == 4);
    click(lj_window_find(sf, "sb5"));                          /* Copy from Sequence 0002 scenes 2..3 */
    lj_w_radiobutton_click(in(LJ_WIN_INSERT_SCENES, "CopyFromSeq"));
    CHECK(lj_show_dlg_insert_pick(2) == 0 && !strcmp(lj_w_edit_text(in(LJ_WIN_INSERT_SCENES, "ESeq")), "0002 Chase"));
    set_text(in(LJ_WIN_INSERT_SCENES, "ESeqFrom"), "  2");
    click(in(LJ_WIN_INSERT_SCENES, "BOk"));
    CHECK(seq5()->n == 9 && tag(1) == 101 && tag(2) == 102 && tag(3) == 1);
    for (i = 0; i < 9; i++) seq5()->scenes[i].val[0] = (uint8_t)(i + 1);
    lj_seq_form_close();
}

static void test_cue_dialogs(void)
{
    lj_window *cf = lj_cue_form_open();
    lj_cue *cue = show->current_cue;
    char cell[64];
    int i;
    CHECK(cf != NULL);
    menu_fire(cf, "SlotsNames1");                              /* Preferences > Sequence Slot Labels */
    CHECK(lj_win_get(LJ_WIN_NAME_SLOTS) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_NAME_SLOTS))->text, "Sequence Slot Labels"));
    CHECK(lj_show_dlg_name_slot(1, "Intensity") == 0 && lj_show_dlg_name_slot(2, "Color") == 0);
    golden(LJ_WIN_NAME_SLOTS, "W4SD_NAME_SLOTS");
    click(in(LJ_WIN_NAME_SLOTS, "bOk"));
    CHECK(lj_win_get(LJ_WIN_NAME_SLOTS) == NULL && !strcmp(lj_ini_gets(lj_ctx_get()->forms_ini, "CUE", "SlotName0", ""), "Intensity"));
    lj_cue_form_refresh();
    CHECK(lj_cue_form_cell(0, 1, cell, sizeof cell) == 0 && !strcmp(cell, "(Intensity)") && lj_cue_form_cell(1, 1, cell, sizeof cell) == 0 && !strcmp(cell, "(Color)"));

    /* Select BG Cue: the Cue control's sbLoadBGCue */
    lj_w_emit(lj_window_find(cf, "sbLoadBGCue"), LJ_WEV_MOUSE_UP);
    CHECK(lj_win_get(LJ_WIN_SELECT_BGCUE_OLD) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_SELECT_BGCUE_OLD))->text, "Select Background Cue"));
    CHECK(!in(LJ_WIN_SELECT_BGCUE_OLD, "bOk")->enabled);      /* greyed until a row is highlighted */
    lj_w_listview_select(in(LJ_WIN_SELECT_BGCUE_OLD, "lv1"), 0);
    lj_win_refresh(LJ_WIN_SELECT_BGCUE_OLD);
    golden(LJ_WIN_SELECT_BGCUE_OLD, "W4SD_SELECT_BGCUE_OLD");
    click(in(LJ_WIN_SELECT_BGCUE_OLD, "bOk"));
    CHECK(lj_win_get(LJ_WIN_SELECT_BGCUE_OLD) == NULL && cue->bg_cue == 1);

    /* Cue Builder: the Cue-Builder menu; Seq Capture takes a load-sequence command, preset GO latches it, the stack rule, stack GO */
    menu_fire(cf, "CueBuilder1");
    CHECK(lj_win_get(LJ_WIN_CUE_STACK) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_CUE_STACK))->text, "Cue Builder"));
    lj_w_emit(in(LJ_WIN_CUE_STACK, "pCaptureSeq"), LJ_WEV_CLICK);
    CHECK(!strcmp(lj_w_caption(in(LJ_WIN_CUE_STACK, "pCaptureSeq")), "Capture Activated (Load Seq Commands)"));
    CHECK(lj_seq_form_load(5, 0) == 0 && seq5()->number == 0);   /* captured, not loaded */
    lj_w_emit(in(LJ_WIN_CUE_STACK, "pCaptureCue"), LJ_WEV_CLICK);
    CHECK(lj_show_dlg_capture_cue(2) == 1 && lj_show_dlg_cue_stack_add(1) == 0 && lj_show_dlg_cue_stack_add(3) == 0);
    CHECK(lj_show_dlg_cue_stack_add(2) == 0);                  /* the same transparent cue again: removed */
    CHECK(lj_win_get(LJ_WIN_CUE_STACK) && in(LJ_WIN_CUE_STACK, "bGo")->enabled);
    golden(LJ_WIN_CUE_STACK, "W4SD_CUE_STACK");
    click(in(LJ_WIN_CUE_STACK, "sbSeqGo"));
    CHECK(cue->slot[11].seq == 5 && cue->slot[11].enabled == 1);   /* "as high a priority position as possible": slot 12 */
    nloads = 0;
    click(in(LJ_WIN_CUE_STACK, "bGo"));
    CHECK(nloads == 2 && loads[0] == 1 && loads[1] == 3);     /* the non-transparent cue first, then the transparent ones top to bottom */
    lj_fn_run(LJ_FN_TOGGLE_CUE_BUILDER, 1);                    /* the hotkey closes it; capture ends with the window */
    CHECK(lj_win_get(LJ_WIN_CUE_STACK) == NULL && lj_show_dlg_capture_seq(2) == 0);
    lj_fn_run(LJ_FN_TOGGLE_CUE_BUILDER, 1);
    CHECK(lj_win_get(LJ_WIN_CUE_STACK) != NULL);
    lj_win_close(LJ_WIN_CUE_STACK);

    /* 12 Q9: a sequence asked into a taken slot, then into a full cue (assert_prompt script rows; the confirm answered by click) */
    cue->slot[0].seq = 2;
    CHECK(lj_cue_form_add_sequence_at(5, 1) == 0);
    CHECK(run_script("q9_nonprio", "assert_prompt \"No priority slots available - add sequence as non-priority sequence ?\"\nclick confirm.BOk\n") == 0);
    CHECK(cue->slot[1].seq == 5);                              /* Yes: the first free slot */
    for (i = 0; i < LJ_CUE_SLOTS; i++) cue->slot[i].seq = 2;
    CHECK(lj_cue_form_add_sequence_at(5, 0) == -1);
    CHECK(run_script("q9_full", "assert_prompt \"No free sequence slots in cue - sequence not added to cue\"\nclick confirm.BOk\n") == 0);
    CHECK(lj_win_message_window() == NULL);
    memset(cue->slot, 0, sizeof cue->slot);
    lj_cue_form_close();
}

static void test_cuelist(void)
{
    static const uint32_t mtc[4] = { 30000, 10000, 20000, 5000 };
    lj_cuelist *cl;
    lj_window *w;
    lj_cl_line l;
    int i;
    w = lj_cuelist_form_open();
    cl = lj_cuelist_form_list();
    CHECK(w && cl);
    if (!w || !cl) return;
    memset(&l, 0, sizeof l);
    lj_cuelist_insert(cl, 0, &l);                              /* line 1: plain */
    for (i = 0; i < 4; i++) { l.cmd = LJ_CL_WAIT_MTC; l.param_ms = mtc[i]; lj_str_copy(l.param, sizeof l.param, "25"); lj_cuelist_insert(cl, cl->n, &l); }
    l.cmd = LJ_CL_WAIT_PC_CLOCK; l.param_ms = 3600000; l.param[0] = 0; lj_cuelist_insert(cl, cl->n, &l);
    lj_cuelist_form_refresh();
    CHECK(menu_en(w, "OffsetMIDITimecodes1") == 1 && menu_en(w, "OffsetPCTime1") == 1 && menu_en(w, "OffsetMIDITimecodes2") == 1);   /* ME-120 alive */
    CHECK(menu_en(w, "OffsetCDTimecodes1") == 0 && menu_en(w, "SortDAWaitTimes") == 0 && menu_en(w, "M_ImportOldCuelist") == 0);   /* ME-11 */
    menu_fire(w, "OffsetMIDITimecodes2");                      /* Sort MIDI wait times */
    CHECK(cl->lines[1].param_ms == 5000 && cl->lines[2].param_ms == 10000 && cl->lines[4].param_ms == 30000 && cl->lines[4].line == 5);
    CHECK(lj_win_get(LJ_WIN_SORT_TIMECODE) == NULL);
    cl->lines[2].cmd = LJ_CL_NONE;                             /* a hole in the block */
    CHECK(lj_show_dlg_sort_waits(cl, LJ_CL_WAIT_MTC) == -1 && !strcmp(prompt(), "Fragmented Selection, cannot sort"));
    prompt_ok();
    cl->lines[2].cmd = LJ_CL_WAIT_MTC;

    menu_fire(w, "OffsetMIDITimecodes1");                      /* Offset MIDI Timecodes */
    CHECK(lj_win_get(LJ_WIN_TIMECODE_OFFSET) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_TIMECODE_OFFSET))->text, "Offset TimeCode"));
    CHECK(in(LJ_WIN_TIMECODE_OFFSET, "pMidi")->visible && !in(LJ_WIN_TIMECODE_OFFSET, "pCD")->visible);
    CHECK(menu_en(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "M_CD") == 0 && menu_en(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "M_DA") == 0 && menu_en(lj_win_get(LJ_WIN_TIMECODE_OFFSET), "M_MP") == 1 /* W5 media */);
    golden(LJ_WIN_TIMECODE_OFFSET, "W4SD_TIMECODE_OFFSET");
    set_text(in(LJ_WIN_TIMECODE_OFFSET, "eMIDISeconds"), "10");
    click(in(LJ_WIN_TIMECODE_OFFSET, "bCheck"));
    CHECK(!strcmp(prompt(), "Offsets Ok"));
    prompt_ok();
    set_text(in(LJ_WIN_TIMECODE_OFFSET, "eMIDISeconds"), "-6");
    click(in(LJ_WIN_TIMECODE_OFFSET, "bCheck"));
    CHECK(!strcmp(prompt(), "Overflow/UnderFlow (1 Lines)"));   /* 5 s - 6 s */
    prompt_ok();
    click(in(LJ_WIN_TIMECODE_OFFSET, "bOk"));
    CHECK(!strcmp(prompt(), "Overflow/UnderFlow values will be adjusted"));
    prompt_ok();
    CHECK(lj_win_get(LJ_WIN_TIMECODE_OFFSET) == NULL && cl->lines[1].param_ms == 0 && cl->lines[2].param_ms == 4000 && cl->lines[4].param_ms == 24000);
    menu_fire(w, "OffsetPCTime1");                             /* Offset PC Time: the PC panel */
    CHECK(in(LJ_WIN_TIMECODE_OFFSET, "pPC")->visible && !in(LJ_WIN_TIMECODE_OFFSET, "pMidi")->visible);
    set_text(in(LJ_WIN_TIMECODE_OFFSET, "ePCHours"), "2");
    click(in(LJ_WIN_TIMECODE_OFFSET, "bOk"));
    CHECK(cl->lines[5].param_ms == 3 * 3600000u && cl->lines[1].param_ms == 0);

    /* Set Current Cue without a saved current cue: Select Cue (TModalSelectCueForm) */
    lj_w_listview_select(lj_window_find(w, "lv1"), 0);
    lj_cuelist_form_apply_cmd(LJ_CL_SET_CURRENT_CUE);
    CHECK(lj_win_get(LJ_WIN_MODAL_SELECT_CUE) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_MODAL_SELECT_CUE))->text, "Select Cue"));
    lj_w_listview_select(in(LJ_WIN_MODAL_SELECT_CUE, "lvCues"), 2);
    lj_win_refresh(LJ_WIN_MODAL_SELECT_CUE);
    golden(LJ_WIN_MODAL_SELECT_CUE, "W4SD_MODAL_SELECT_CUE");
    click(in(LJ_WIN_MODAL_SELECT_CUE, "bOk"));
    CHECK(cl->lines[0].cue == 3);
    lj_cuelist_form_close();
    lj_win_open(LJ_WIN_SORT_TIMECODE);
    golden(LJ_WIN_SORT_TIMECODE, "W4SD_SORT_TIMECODE");
    lj_win_close(LJ_WIN_SORT_TIMECODE);
}

static void test_pickers(void)
{
    picked = 0;
    lj_show_dlg_select_cuelist(pick_cb, NULL);                 /* Startup Options' Select Startup Cuelist (dlg-desktop's button) */
    CHECK(lj_win_get(LJ_WIN_SELECT_CUELIST_MODAL) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_SELECT_CUELIST_MODAL))->text, "Select Startup Cuelist"));
    lj_w_listview_select(in(LJ_WIN_SELECT_CUELIST_MODAL, "Lv1"), 0);
    lj_win_refresh(LJ_WIN_SELECT_CUELIST_MODAL);
    golden(LJ_WIN_SELECT_CUELIST_MODAL, "W4SD_SELECT_CUELIST_MODAL");
    click(in(LJ_WIN_SELECT_CUELIST_MODAL, "bOk"));
    CHECK(picked == 1 && lj_win_get(LJ_WIN_SELECT_CUELIST_MODAL) == NULL);
    started = 0;
    lj_show_dlg_set_sequence(8, start_cb, NULL);               /* the Global Patch's sequential patch (hw-dmxin's caller) */
    CHECK(!strcmp(lj_w_caption(in(LJ_WIN_SET_SEQUENCE, "Label3")), "8"));
    lj_w_numedit_set_value(in(LJ_WIN_SET_SEQUENCE, "E1"), 17);
    golden(LJ_WIN_SET_SEQUENCE, "W4SD_SET_SEQUENCE");
    click(in(LJ_WIN_SET_SEQUENCE, "BitBtn1"));
    CHECK(started == 17 && lj_win_get(LJ_WIN_SET_SEQUENCE) == NULL);
    lj_show_dlg_seq_used(3, 4, 1, 2);                          /* Delete Sequences > Check Used's progress */
    CHECK(lj_win_get(LJ_WIN_SEQ_USED) && ((lj_w_progressbar *)in(LJ_WIN_SEQ_USED, "Pg1"))->pos == 3);
    golden(LJ_WIN_SEQ_USED, "W4SD_SEQ_USED");
    lj_win_close(LJ_WIN_SEQ_USED);
}

static int32_t pan_of(int fx) { const lj_scene *sc = lj_ctx_scene(); int ch = 16 * fx + 10; return (sc->val[ch] << 8 | sc->val[ch + 1]) - 32768; }
static int32_t tilt_of(int fx) { const lj_scene *sc = lj_ctx_scene(); int ch = 16 * fx + 12; return (sc->val[ch] << 8 | sc->val[ch + 1]) - 32768; }
static void test_movement(void)
{
    lj_scene *sc = lj_ctx_scene();
    int i;
    for (i = 0; i < 2; i++) {                                  /* pan -1234, tilt 1234 (17 s4.1 worked example) */
        uint16_t p = (uint16_t)(32768 - 1234), t = (uint16_t)(32768 + 1234);
        sc->val[16 * i + 10] = (uint8_t)(p >> 8); sc->val[16 * i + 11] = (uint8_t)p; sc->val[16 * i + 12] = (uint8_t)(t >> 8); sc->val[16 * i + 13] = (uint8_t)t;
        lj_sel_set(i, 1, i == 0);
    }
    lj_win_open(LJ_WIN_MOVEMENT_OPTIONS);
    CHECK(lj_win_get(LJ_WIN_MOVEMENT_OPTIONS) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_MOVEMENT_OPTIONS))->text, "Position Options"));
    CHECK(!in(LJ_WIN_MOVEMENT_OPTIONS, "b360")->enabled);
    golden(LJ_WIN_MOVEMENT_OPTIONS, "W4SD_MOVEMENT_OPTIONS");
    click(in(LJ_WIN_MOVEMENT_OPTIONS, "bMirrorPan"));
    CHECK(pan_of(0) == 1234 && tilt_of(0) == 1234 && pan_of(1) == 1234);
    click(in(LJ_WIN_MOVEMENT_OPTIONS, "bMirrorTilt"));
    CHECK(tilt_of(1) == -1234);
    click(in(LJ_WIN_MOVEMENT_OPTIONS, "bSwap"));
    CHECK(pan_of(0) == -1234 && tilt_of(0) == 1234);
    /* LORD_FIDELITY-11: Fan Out and Position Options close with the Movement control [04 s4, RN 2.5] (a script row, then the 100 ms refresh) */
    CHECK(run_script("autoclose", "open movement\nopen fan_out\nopen movement_options\n") == 0);
    CHECK(lj_ctl_window_get(LJ_CTL_MOVEMENT) != NULL && lj_win_get(LJ_WIN_FAN_OUT) != NULL);
    lj_fan_out_refresh(); lj_win_refresh(LJ_WIN_MOVEMENT_OPTIONS);
    CHECK(lj_win_get(LJ_WIN_FAN_OUT) != NULL && lj_win_get(LJ_WIN_MOVEMENT_OPTIONS) != NULL);   /* open while the control is */
    CHECK(run_script("autoclose2", "close movement\n") == 0);
    lj_fan_out_refresh(); lj_win_refresh(LJ_WIN_MOVEMENT_OPTIONS);
    CHECK(lj_win_get(LJ_WIN_FAN_OUT) == NULL && lj_win_get(LJ_WIN_MOVEMENT_OPTIONS) == NULL);
}

static void test_statics(void)
{
    lj_show *sh = show;
    lj_window *w = lj_win_open(LJ_WIN_STATICS_CONTROL);         /* D11: View > Statics Control = Statics 2 */
    lj_window *pw;
    CHECK(w != NULL && w == lj_win_get(LJ_WIN_STATICS2_FORM));
    CHECK(lj_win_open(LJ_WIN_SAVE_STATIC) == NULL && lj_win_open(LJ_WIN_STATICS_REPLACE) == NULL);   /* W4-WIN: save_preset deleted */
    seq5()->number = 7;                                        /* Record Current Scene on slot 1, then its name (TStaticSlotNameForm) */
    lj_statics2_popup(1, 10, 10);
    menu_fire(w, "M_RecordCurrentScene");
    CHECK(lj_win_get(LJ_WIN_STATIC_SLOT_NAME) && !strcmp(lj_window_caption(lj_win_get(LJ_WIN_STATIC_SLOT_NAME))->text, "Static Slot Name"));
    set_text(in(LJ_WIN_STATIC_SLOT_NAME, "eStaticName"), "Strobes");
    golden(LJ_WIN_STATIC_SLOT_NAME, "W4SD_STATIC_SLOT_NAME");
    click(in(LJ_WIN_STATIC_SLOT_NAME, "BitBtn1"));
    CHECK(lj_win_get(LJ_WIN_STATIC_SLOT_NAME) == NULL && sh->npages >= 1 && !strcmp(sh->pages[0].slot[0].name, "Strobes"));
    {   /* LORD_FIDELITY-10: the slot popup with its Replace Mode submenu, both "Replace Page" rows (D13) -> TMP statics2-popup.bmp
         * (docs/screenshots/w4/statics2-popup.png is converted from it) */
        lj_menu *m = lj_window_menus(w);
        lj_key_ev k;
        lj_surface out;
        int n = 0, i;
        lj_statics2_popup(1, 10, 10);
        CHECK(m && m->depth == 1);
        if (m && m->depth == 1) {
            m->lv[0].sel = 8;                                  /* "Replace Mode" */
            memset(&k, 0, sizeof k); k.type = LJ_EV_KEY_DOWN; k.key = 0x27;   /* VK_RIGHT opens the submenu */
            lj_menu_key(m, &k);
            CHECK(m->depth == 2 && m->lv[1].n == 11);
            for (i = 0; m->depth == 2 && i < m->lv[1].n; i++) n += !strcmp(m->lv[1].items[i].caption, "Replace Page");
            CHECK(n == 2);
            if (m->depth == 2 && lj_surface_init(&out, m->lv[0].s.w + m->lv[1].s.w, m->lv[1].y - m->lv[0].y + m->lv[1].s.h > m->lv[0].s.h ? m->lv[1].y - m->lv[0].y + m->lv[1].s.h : m->lv[0].s.h) == 0) {
                lj_fill(&out, 0, 0, out.w, out.h, 0xFFFFFFu);
                lj_blit_rgb(&out, 0, 0, &m->lv[0].s, 0);
                lj_blit_rgb(&out, m->lv[1].x - m->lv[0].x, m->lv[1].y - m->lv[0].y, &m->lv[1].s, 0);
                CHECK(lj_bmp_write(&out, TMP "statics2-popup.bmp") == 0);
                lj_surface_free(&out);
            }
            lj_menu_close(m);
        }
    }
    seq5()->number = 0;                                        /* a link needs a saved sequence: "Error!" */
    lj_statics2_popup(2, 10, 10);
    menu_fire(w, "M_LinkToCurrentScene");
    CHECK(run_script("q9_error", "assert_prompt \"Error!\"\nclick confirm.BOk\n") == 0);
    pw = lj_win_open(LJ_WIN_STATICS2_PREFS);                   /* Statics Preferences > Pages with no page highlighted */
    CHECK(pw != NULL);
    if (pw) { lj_w_tabs_select(lj_window_find(pw, "PageControl1"), 1); ((lj_w_listview *)lj_window_find(pw, "lvPages"))->index = -1; }   /* the Pages tab, no page highlighted */
    CHECK(run_script("q9_page", "click TStatics2PrefsForm.sbEditPage\nassert_prompt \"Get Page Error !\"\nclick confirm.BOk\n"
                                "click TStatics2PrefsForm.sbDeletePage\nassert_prompt \"Page Error (Idx = -1)\"\nclick confirm.BOk\n") == 0);
    lj_win_close(LJ_WIN_STATICS2_PREFS);
    lj_win_close(LJ_WIN_STATICS2_FORM);
}

static void test_scripts(void)   /* entry points as the UI script drives them (docs/ui-script.md) */
{
    CHECK(run_script("entry", "open seq_form\nclick TSequenceForm.sb6\nassert_caption TDeleteSceneForm \"Delete Scene(s)\"\nclick TDeleteSceneForm.bCancel\n"
                              "click TSequenceForm.sb5\nassert_caption TInsertScenesForm \"Insert Scene(s) before current scene\"\nassert_enabled TInsertScenesForm.ESeqFrom 0\n"
                              "click TInsertScenesForm.Bcancel\nclose seq_form\n") == 0);
}

int lj_test_show_dialogs(int argc, char **argv);
int lj_test_show_dialogs(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    lj_cue_form_host host = { NULL, stub_load, NULL, NULL };
    lj_uiscript_host uh = { NULL, NULL, NULL };
    lj_sequence *seq;
    int i;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_fs_mkdir(TMP); lj_fs_mkdir(TMP "User");
    CHECK(lj_library_create(TMP, "Lib", &lib) == 0);
    show = lj_show_new();
    if (!show) return 1;
    seq = lj_sequence_new_rec();
    lj_sequence_alloc(seq, 5);
    for (i = 0; i < 5; i++) { seq->scenes[i].val[0] = (uint8_t)(i + 1); seq->scenes[i].state[0] = LJ_SNAP; }
    show->current_seq = seq;
    show->current_cue = lj_cue_new_rec();
    for (i = 0; i < 5; i++) {                                  /* five MAC 500 Mode 2/4 (16-bit pan 11/12, tilt 13/14) */
        show->fixture[i].profile_id = 12; show->fixture[i].link = 1; show->fixture[i].address = (uint16_t)(1 + 16 * i);
        snprintf(show->fixture[i].user_name, sizeof show->fixture[i].user_name, "Spot %d", i + 1);
    }
    c->show = show; c->lib = &lib; c->current_scene = 0;
    c->forms_ini = lj_ini_new(); c->main_ini = lj_ini_new();
    lj_win_set_host(NULL);                                     /* headless; registers every family's lj_fn rows */
    lj_cue_form_set_host(&host);
    lj_uiscript_set_host(&uh);
    save_records();
    test_scenes();
    test_cue_dialogs();
    test_cuelist();
    test_pickers();
    test_movement();
    test_statics();
    test_scripts();
    CHECK(me16 == 0);
    lj_win_close_all();
    lj_warnings_close_all();
    lj_cue_form_set_host(NULL);
    lj_uiscript_set_host(NULL);
    c->show = NULL; c->lib = NULL;
    lj_ini_free(c->forms_ini); lj_ini_free(c->main_ini); c->forms_ini = c->main_ini = NULL;
    lj_show_free(show);
    lj_log_set_sink(NULL, NULL);
    printf("test_show_dialogs: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
