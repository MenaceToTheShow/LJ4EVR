/* test_windows_a.c - the runnable check for src/windows/lj_seq_form.c, lj_save_dialogs.c,
 * lj_lists.c (W1 windows-a): caption table [F19], the s:hh time text [07 fact-check],
 * Add / Insert / Delete / Delete All [F29] through the window, the time fields and
 * check boxes, the W_PROGRESS2 bars advancing (LORD_FIDELITY-14), the z-order sliver
 * after binding, save numbering 0001 / 0002 / delete / 0003 [F28] in a temp library,
 * the List of Sequences rows, sort, load and the drag source. Headless (no host).
 * The scene and save blocks need the model bodies; while src/model/ still carries the
 * contract stubs (lj_sequence_alloc returns -1) they print SKIP and are not counted. */
#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "engine/lj_engine.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_dnd.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_lists.h"
#include "windows/lj_save_dialogs.h"
#include "windows/lj_seq_form.h"
#include "windows/lj_windows.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *panel2(lj_window *win) { const char *s = lj_w_caption(lj_window_find(win, "Panel2")); return s ? s : ""; }

static void fire(lj_widget *w, int ev) { if (w && w->on_event) w->on_event(w, ev, w->ud); }

static int blue_px(lj_window *win, lj_widget *bar)
{
    const lj_surface *s = lj_window_surface(win);
    int ax, ay, x, y, n = 0;
    lj_widget_abs(bar, &ax, &ay);                                   /* surface coordinates: the root's origin is included */
    for (y = ay; y < ay + bar->h; y++) for (x = ax; x < ax + bar->w; x++) n += s->px[y * s->w + x] == 0x0000FFu;
    return n;
}

static int file_exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) fclose(f); return f != NULL; }

int lj_test_windows_a(int argc, char **argv);
int lj_test_windows_a(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    lj_library lib;
    lj_window *win, *lw;
    lj_widget *root, *w, *bar;
    lj_sequence *s;
    lj_ui_event ev;
    lj_rect cr;
    char buf[64], path[LJ_PATH_CAP];
    uint32_t ms;
    int stub, n0, n1, kind, no;
    (void)argc; (void)argv;

    /* ---- captions [F19, 07 s5] ---- */
    CHECK(lj_win_caption_seq(buf, sizeof buf, 0, "") == 0 && strcmp(buf, "SEQUENCE [0000 No Name]") == 0);
    CHECK(lj_win_caption_seq(buf, sizeof buf, 3, "MX-4 Color Chase 1") == 0 && strcmp(buf, "SEQUENCE [0003 MX-4 Color Chase 1]") == 0);
    CHECK(lj_win_caption_seq(buf, sizeof buf, 12, NULL) == 0 && strcmp(buf, "SEQUENCE [0012 No Name]") == 0);
    CHECK(lj_win_caption_seq(buf, 8, 1, "x") == -1);

    /* ---- the time fields: seconds:hundredths, no leading zero on the seconds (the reference field reads "0:46" / "0:05") ---- */
    CHECK(lj_fmt_time(buf, sizeof buf, 2550) == 0 && strcmp(buf, "2:55") == 0);
    CHECK(lj_fmt_time(buf, sizeof buf, 50) == 0 && strcmp(buf, "0:05") == 0);
    CHECK(lj_seq_time_parse("2:55", &ms) == 0 && ms == 2550);
    CHECK(lj_seq_time_parse("0:05", &ms) == 0 && ms == 50);
    CHECK(lj_seq_time_parse("2,5", &ms) == 0 && ms == 2500);
    CHECK(lj_seq_time_parse("3", &ms) == 0 && ms == 3000);
    CHECK(lj_seq_time_parse("99:00", &ms) == 0 && ms == 60000);
    CHECK(lj_seq_time_parse("", &ms) == -1 && lj_seq_time_parse("ab", &ms) == -1);

    /* ---- context: a show with one current sequence, the engine, an empty Main.Ini ---- */
    c->show = lj_show_new();
    c->show->current_seq = lj_sequence_new_rec();
    c->engine = lj_engine_create();
    c->main_ini = lj_ini_new();
    s = c->show->current_seq;
    stub = lj_sequence_alloc(s, 1) != 0;
    if (stub) printf("windows_a: SKIP scene and save blocks: src/model carries the contract stubs (lj_sequence_alloc = -1)\n");

    /* ---- the window, unshrunk: binding, z-order sliver, caption, counter, progress widget ---- */
    lj_seq_form_shrink(0);
    win = lj_win_open(LJ_WIN_SEQ_FORM);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_win_get(LJ_WIN_SEQ_FORM) == win && lj_win_find("TSequenceForm") == win && lj_win_find("seq_form") == win);
    root = lj_window_root(win);
    w = lj_window_find(win, "ESeqType");
    CHECK(w && w->x + w->w == 360 && lj_window_find(win, "Panel1")->w == 354);                    /* the 6-px sliver x 354..359 */
    CHECK(lj_widget_hit(root, 356, 20) == w && lj_widget_hit(root, 353, 20) == lj_window_find(win, "Panel1"));
    CHECK(strcmp(lj_window_caption(win)->text, "SEQUENCE [0000 No Name]") == 0);
    CHECK(strcmp(panel2(win), "1:1") == 0);
    CHECK(!lj_window_find(win, "sb1")->enabled && !lj_window_find(win, "sb11")->enabled);          /* one scene, unsaved */
    bar = lj_window_find(win, "ElapsedBar");
    CHECK(bar && bar->w == 71 && bar->h == 23 && bar->parent == lj_window_find(win, "Panel1"));

    /* ---- W_PROGRESS2: the blue scene bar advances between two feeds ---- */
    lj_seq_form_progress(1000, 4000, 0, 3000);
    lj_window_paint(win);
    n0 = blue_px(win, bar);
    lj_seq_form_progress(2000, 4000, 1500, 3000);
    lj_window_paint(win);
    n1 = blue_px(win, bar);
    CHECK(n0 > 0 && n1 > n0 && n1 >= 2 * n0 - 5);
    lj_seq_form_progress(0, 0, 0, 0);
    lj_window_paint(win);
    CHECK(blue_px(win, bar) == 0);

    /* ---- scenes [F29] ---- */
    if (!stub) {
        lj_widget *sync = lj_window_find(win, "cbSync"), *gfade = lj_window_find(win, "cbGlobalFade"), *ud = lj_window_find(win, "UdScene");
        s->scenes[0].val[5] = 200; s->scenes[0].state[5] = LJ_SNAP; s->scenes[0].scene_ms = 4000;
        CHECK(lj_seq_form_add_scene() == 0 && s->n == 2 && c->current_scene == 1);
        CHECK(s->scenes[1].val[5] == 200 && s->scenes[1].state[5] == LJ_SNAP && s->scenes[1].scene_ms == 4000);   /* Add copies the current scene */
        CHECK(strcmp(panel2(win), "2:2") == 0 && c->modified[LJ_MOD_SEQ] && s->modified);
        CHECK(lj_window_find(win, "sb1")->enabled);
        s->scenes[1].val[5] = 10;
        CHECK(lj_seq_form_insert_scene() == 0 && s->n == 3 && c->current_scene == 1);
        CHECK(s->scenes[1].val[5] == 10 && s->scenes[2].val[5] == 10 && s->scenes[0].val[5] == 200);   /* a copy, before the current scene */
        CHECK(lj_seq_form_goto(5) == 0 && c->current_scene == 2);                                      /* wraps */
        CHECK(lj_seq_form_goto(-1) == 0 && c->current_scene == 2);
        CHECK(strcmp(panel2(win), "3:3") == 0);
        CHECK(lj_seq_form_delete_scene() == 0 && s->n == 2 && c->current_scene == 1);
        CHECK(lj_seq_form_delete_all() == 0 && s->n == 1 && c->current_scene == 0 && !lj_scene_is_programmed(&s->scenes[0]));
        CHECK(s->scenes[0].scene_ms == LJ_SCENE_TIME_DEFAULT_MS && s->scenes[0].fade_ms == LJ_FADE_TIME_DEFAULT_MS);
        CHECK(strcmp(panel2(win), "1:1") == 0 && strcmp(lj_w_edit_text(lj_window_find(win, "ES")), "1:00") == 0);
        /* typed times, committed when the focus reaches a check box (cbSyncEnter) */
        lj_w_edit_set_text(lj_window_find(win, "ES"), "4:00"); lj_w_edit_set_text(lj_window_find(win, "EF"), "3:00");
        fire(sync, LJ_WEV_ENTER);
        CHECK(s->scenes[0].scene_ms == 4000 && s->scenes[0].fade_ms == 3000);
        CHECK(strcmp(lj_w_edit_text(lj_window_find(win, "ES")), "4:00") == 0 && strcmp(lj_w_edit_text(lj_window_find(win, "EF")), "3:00") == 0);
        /* Synchronize: the fade follows the scene time */
        lj_w_checkbox_set_checked(sync, 1);
        CHECK(s->sync_times == 1 && s->scenes[0].fade_ms == 4000);
        /* one up pulse on the scene time */
        ((lj_w_udbutton *)ud)->delta = 1;
        fire(ud, LJ_WEV_CHANGE);
        CHECK(s->scenes[0].scene_ms == 4000 + LJ_SEQ_TIME_STEP_MS && s->scenes[0].fade_ms == 4000 + LJ_SEQ_TIME_STEP_MS);
        lj_w_checkbox_set_checked(sync, 0);
        CHECK(s->sync_times == 0);
        /* Global Fade Time: every scene follows */
        CHECK(lj_seq_form_add_scene() == 0 && s->n == 2);
        lj_w_checkbox_set_checked(gfade, 1);
        CHECK(s->global_fade_time == 1);
        CHECK(lj_seq_form_set_times(4050, 1000) == 0 && s->scenes[0].fade_ms == 1000 && s->scenes[1].fade_ms == 1000);
        lj_w_checkbox_set_checked(gfade, 0);
        /* New (forced): one all-off scene, number 0, flag clear */
        lj_seq_form_new(1);
        CHECK(s->n == 1 && s->number == 0 && !c->modified[LJ_MOD_SEQ] && strcmp(panel2(win), "1:1") == 0);
    }

    /* ---- save numbering [F28] in a temp library under the working directory ---- */
    lj_library_delete("./", "WindowsATest");                    /* a previous run's folder would hand out [NEXT] 4 */
    if (!stub && lj_library_create("./", "WindowsATest", &lib) == 0) {
        c->lib = &lib;
        lj_ini_seti(c->main_ini, "PREFS", "WarnOWSeq", 0);
        lj_ini_seti(c->main_ini, "PREFS", "WarnSeq", 0);
        CHECK(lj_seq_form_add_scene() == 0 && s->n == 2);
        CHECK(lj_win_open(LJ_WIN_SAVE_SEQ) != NULL);
        CHECK(strcmp(lj_w_edit_text(lj_window_find(lj_win_get(LJ_WIN_SAVE_SEQ), "SeqNum")), "") == 0);           /* blank until saved */
        CHECK(!lj_window_find(lj_win_get(LJ_WIN_SAVE_SEQ), "Bsave")->enabled);
        lj_w_edit_set_text(lj_window_find(lj_win_get(LJ_WIN_SAVE_SEQ), "ESeqName"), "first");
        CHECK(lj_save_seq_do(0) == 0 && s->number == 1 && strcmp(s->name, "first") == 0 && !s->modified && !c->modified[LJ_MOD_SEQ] && c->seq_saved);
        CHECK(strcmp(lj_window_caption(win)->text, "SEQUENCE [0001 first]") == 0);
        CHECK(lj_window_find(win, "sb11")->enabled);
        CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 1, path, sizeof path) == 0 && file_exists(path));
        lj_win_refresh(LJ_WIN_SAVE_SEQ);                                                                  /* AutoClose (default on) closes it */
        CHECK(lj_win_get(LJ_WIN_SAVE_SEQ) == NULL);
        CHECK(lj_save_seq_do(0) == 0 && s->number == 1);                                                  /* Save Sequence overwrites */
        CHECK(lj_save_seq_do(1) == 0 && s->number == 2);                                                  /* Save as New Sequence */
        CHECK(lj_library_record_path(&lib, LJ_REC_SEQ, 1, path, sizeof path) == 0 && file_exists(path)); /* the original is left intact */
        CHECK(lj_library_delete_record(&lib, LJ_REC_SEQ, 2) == 0);
        CHECK(lj_save_seq_do(1) == 0 && s->number == 3);                                                  /* a number is never reused */
        /* the list: 0001 and 0003, sort, load, the drag source */
        lw = lj_win_open(LJ_WIN_LIST_SEQ);
        CHECK(lw != NULL && lj_win_find("list_seq") == lw);
        CHECK(lj_seqlist_count() == 2 && lj_seqlist_number_at(0) == 1 && lj_seqlist_number_at(1) == 3);
        CHECK(strcmp(lj_window_caption(lw)->text, "Select Sequence (Drag To Target)") == 0);
        lj_seqlist_sort(1);                                                                               /* by name: "first" (1) < "first" (3) ties on the number */
        CHECK(lj_seqlist_number_at(0) == 1);
        lj_seqlist_mode_set(LJ_SEQLIST_BROWSE_CUE);
        CHECK(strcmp(lj_window_caption(lw)->text, "Select Sequence (Browse to Cue)") == 0);
        lj_seqlist_mode_set(LJ_SEQLIST_DRAG);
        if (lw) {
            lj_window_client_rect(lw, &cr);
            memset(&ev, 0, sizeof ev);
            ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.button = 1; ev.u.mouse.clicks = 1;
            ev.u.mouse.x = cr.x + 20; ev.u.mouse.y = cr.y + 17 + 2 + 7;                                    /* row 0 under the 17-px header */
            lj_window_event(lw, &ev);
            CHECK(lj_seqlist_selected() == 0);
            CHECK(lj_dnd_active(lw) && lj_dnd_info(lw, &kind, &no, NULL, NULL) == 0 && kind == LJ_DND_SEQUENCE && no == 1);
            ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP;
            lj_window_event(lw, &ev);
            CHECK(!lj_dnd_active(lw));
        }
        CHECK(lj_seq_form_load(1, 1) == 0 && c->show->current_seq->number == 1 && strcmp(lj_window_caption(win)->text, "SEQUENCE [0001 first]") == 0);
        s = c->show->current_seq;
        CHECK(s->n == 2 && c->current_scene == 0 && strcmp(panel2(win), "1:2") == 0);
        lj_win_close(LJ_WIN_LIST_SEQ);
        CHECK(lj_win_get(LJ_WIN_LIST_SEQ) == NULL && lj_seqlist_count() == 0);
        lj_library_delete_record(&lib, LJ_REC_SEQ, 1);
        lj_library_delete_record(&lib, LJ_REC_SEQ, 3);
        c->lib = NULL;
    } else if (!stub) printf("windows_a: SKIP save block: lj_library_create failed\n");

    /* ---- shrink PROPOSAL: reopened with the default, the client is Panel1's 354x106 ---- */
    lj_win_close(LJ_WIN_SEQ_FORM);
    CHECK(lj_win_get(LJ_WIN_SEQ_FORM) == NULL);
    lj_seq_form_shrink(LJ_SEQFORM_SHRINK_TO_PANEL1);
    win = lj_win_open(LJ_WIN_SEQ_FORM);
    CHECK(win && lj_window_root(win)->w == (LJ_SEQFORM_SHRINK_TO_PANEL1 ? 354 : 466));
    lj_win_toggle(LJ_WIN_SEQ_FORM);
    CHECK(lj_win_get(LJ_WIN_SEQ_FORM) == NULL);
    lj_win_close_all();

    lj_engine_destroy(c->engine); c->engine = NULL;
    lj_show_free(c->show); c->show = NULL;
    lj_ini_free(c->main_ini); c->main_ini = NULL;
    printf("windows_a: %d failure(s)%s\n", fails, stub ? " (model stubs: scene/save blocks skipped)" : "");
    return fails ? 1 : 0;
}
