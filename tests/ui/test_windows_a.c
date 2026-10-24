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

static void golden(lj_window *win, const char *form, int write);   /* below, with the W2 goldens */
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
        {   /* CARET (fidelity.md:27): a written value leaves the caret at the end; golden W2A_TSEQUENCEFORM_CARET with ES focused */
            lj_widget *es = lj_window_find(win, "ES");
            lj_w_edit_set_text(es, "4:00");
            lj_w_focus(es);
            CHECK(((lj_w_edit *)es)->caret == 4);
            golden(win, "TSEQUENCEFORM_CARET", argc > 1 && strcmp(argv[1], "--write-golden") == 0);
            lj_w_focus(sync);
        }
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

/* ======================= W2 windows-a: Loop Options, Cue Time Control, cue pages, transparent / flash ======================= *
 * lj4evr_tests test_windows_a_w2 [--write-golden]: every control of TLoopToolsForm maps to an lj_cue_slot member
 * (lj_loop_options_fields, table-driven); one change posts exactly one LJ_CMD_LOOP_OPTS per target slot with the
 * pinned slot, no change / open+close posts nothing; the 13 Cue Time faders, "Cue" text, clamps, synchronize and
 * arrows; the cue page dialogs (Ok applies, Cancel drops) and the List of Cues tabs; transparent / flash preferences;
 * captions and menu texts verbatim (DFM / 13 s5); the four opened-window goldens W2A_<FORM>.bmp. Headless, no engine. */
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "ui/forms/lj_forms_gen.h"
#include "model/lj_cue.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_cue_pages.h"
#include "windows/lj_cue_time.h"
#include "windows/lj_loop_options.h"
#include "windows/lj_warnings.h"
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_rgbpack.h"
#include "desktop/lj_selection.h"
#include "ui/widgets/lj_w_osf.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static void fire_click(lj_widget *w) { fire(w, LJ_WEV_CLICK); }
static const char *cap_of(lj_window *win, const char *name) { const char *s = lj_w_caption(lj_window_find(win, name)); return s ? s : ""; }
static const lj_menu_item_desc *menu_item(const lj_menu_item_desc *it, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++) {
        const lj_menu_item_desc *r;
        if (it[i].name && !strcmp(it[i].name, name)) return &it[i];
        if ((r = menu_item(it[i].children, it[i].nchildren, name)) != NULL) return r;
    }
    return NULL;
}
static const char *menu_cap(const lj_form_desc *f, const char *name)
{
    int i;
    for (i = 0; i < f->nmenus; i++) { const lj_menu_item_desc *it = menu_item(f->menus[i].items, f->menus[i].n, name); if (it) return it->caption ? it->caption : ""; }
    return "";
}
static void menu_fire(lj_window *win, const char *name)   /* what a click on the item does: the bound OnClick with the item as the firing one */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = m ? lj_menu_rt_named(m, name) : NULL;
    CHECK(rt && rt->fn);
    if (!rt || !rt->fn) return;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)   /* fopen under /W4 /WX (the golden compare reads two files) */
#endif
static void golden(lj_window *win, const char *form, int write)
{
    char path[1024], tmp[128];
    long at = 0;
    FILE *a, *b;
    int same = 0;
    if (!win) { CHECK(win != NULL); return; }
    lj_window_paint(win);
    snprintf(path, sizeof path, "%s/tests/ui/golden/W2A_%s.bmp", LJ_SOURCE_DIR, form);
    if (write) { CHECK(lj_bmp_write(lj_window_surface(win), path) == 0); printf("  wrote %s\n", path); return; }
    snprintf(tmp, sizeof tmp, "lj_test_w2a_%s.bmp", form);
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    a = fopen(tmp, "rb"); b = fopen(path, "rb");
    if (a && b) {
        int ca, cb;
        same = 1;
        do { ca = fgetc(a); cb = fgetc(b); if (ca != cb) { same = 0; break; } at++; } while (ca >= 0);
    }
    if (a) fclose(a);
    if (b) fclose(b);
    if (same) printf("  W2A_%s: identical\n", form);
    else { fails++; printf("FAIL W2A_%s: %s (byte %ld)\n", form, b ? "differs from the golden" : "no golden", at); }
    remove(tmp);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
static long member(const lj_cue_slot *s, const lj_loop_field_desc *d)
{
    const unsigned char *m = (const unsigned char *)s + d->offset;
    if (d->size == 1) return m[0];
    if (d->size == 2) { uint16_t v; memcpy(&v, m, 2); return v; }
    { uint32_t v; memcpy(&v, m, 4); return (long)v; }
}

static int w2a_bind_logs;   /* ME-16: lj_window_create's "no binding for handler" / "matches no DFM handler" / "unknown event" lines while the W2 windows open */
static void w2a_sink(void *ud, const char *line)
{
    (void)ud;
    if (strstr(line, "lj_window:") && (strstr(line, "binding") || strstr(line, "unknown event"))) { w2a_bind_logs++; printf("  %s\n", line); }
}

int lj_test_windows_a_w2(int argc, char **argv);
int lj_test_windows_a_w2(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    int write = argc > 1 && !strcmp(argv[1], "--write-golden");
    const lj_loop_field_desc *fd;
    lj_cue *cue;
    lj_window *win;
    lj_widget *w;
    lj_cuepages *p;
    lj_cmd cmd;
    char buf[32];
    int n, i, before;
    fails = 0;

    c->show = lj_show_new();
    c->show->current_cue = cue = lj_cue_new_rec();
    c->forms_ini = lj_ini_new(); c->main_ini = lj_ini_new();
    c->engine = NULL; c->lib = NULL; c->cue_pages = NULL;
    cue->slot[0].seq = 5; cue->slot[1].seq = 7; cue->slot[2].seq = 9;
    lj_cue_form_select(-1, 0);
    lj_win_set_host(NULL);                                                      /* headless; registers the windows' lj_fn rows */
    lj_log_set_sink(w2a_sink, NULL);

    /* ---- Loop Options: captions verbatim (TLOOPTOOLSFORM, 03 s5.5) ---- */
    win = lj_win_open(LJ_WIN_LOOP_OPTIONS);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(strcmp(lj_window_caption(win)->text, "Loop Options") == 0);
    {
        static const char *const dir[] = { "Forward", "Reverse", "Bounce", "Random" }, *const end[] = { "Stop on scene", "Sequence out", "Goto Scene", "Link to Cue" };
        static const char *const lbl[][2] = { { "Label1", "First Scene" }, { "Label2", "Last Scene" }, { "Label3", "Loops" }, { "Label4", "Scenes pr Trig" },
            { "Label5", "(Audio/Manual Trig Only)" }, { "Label6", "Audio-Trigs/Trig" }, { "Label7", "(Audio Trig Only)" }, { "cbTrigLoad", "Trig on load" },
            { "bDefaultLoop", "&Default Loop Settings" }, { "bSelectCue", "&Select Cue" }, { "RLimits", " Scene Limits " }, { "REndLoop", " End Loop Action " },
            { "RTrigScenes", " Trig Options " }, { "RDirection", "Loop Direction" } };
        const lj_ctl_desc *rd = lj_form_find_ctl(&lj_form_TLOOPTOOLSFORM, "RDirection"), *re = lj_form_find_ctl(&lj_form_TLOOPTOOLSFORM, "REndLoop");
        CHECK(rd && rd->nitems == 4 && re && re->nitems == 4);
        for (i = 0; rd && re && i < 4; i++) { CHECK(strcmp(rd->items[i], dir[i]) == 0); CHECK(strcmp(re->items[i], end[i]) == 0); }
        for (i = 0; i < LJ_COUNT_OF(lbl); i++) { if (strcmp(cap_of(win, lbl[i][0]), lbl[i][1]) != 0) printf("  caption %s = '%s'\n", lbl[i][0], cap_of(win, lbl[i][0])); CHECK(strcmp(cap_of(win, lbl[i][0]), lbl[i][1]) == 0); }
        CHECK(strcmp(menu_cap(&lj_form_TLOOPTOOLSFORM, "Help1"), "&Help") == 0);
    }
    CHECK(lj_loop_options_last_cmd(NULL) == 0);                                  /* opening posts nothing */
    w = lj_window_find(win, "eFirst");
    CHECK(w && strcmp(lj_w_edit_text(w), "") == 0);                               /* 0 = no limit: an empty field */
    CHECK(strcmp(lj_w_edit_text(lj_window_find(win, "eTrigScenes")), "1") == 0);

    /* every control maps to its member: one change = one LOOP_OPTS on slot 1 with the pinned slot; the same value again = nothing */
    fd = lj_loop_options_fields(&n);
    CHECK(n == LJ_LF_COUNT);
    for (i = 0; i < n; i++) {
        long want, got;
        w = lj_window_find(win, fd[i].ctl);
        CHECK(w != NULL);
        if (!w) continue;
        before = lj_loop_options_last_cmd(NULL);
        if (w->kind == LJ_K_RADIOGROUP) { want = 2; lj_w_radiogroup_set_index(w, (int)want); }
        else if (w->kind == LJ_K_CHECKBOX) { want = 1; lj_w_checkbox_set_checked(w, 1); }
        else { want = fd[i].lo + 2; snprintf(buf, sizeof buf, "%ld", want); lj_w_edit_set_text(w, buf); fire(w, LJ_WEV_KEY_UP); }
        got = member(&cue->slot[0], &fd[i]);
        if (got != want) printf("  field %s: %ld, want %ld\n", fd[i].ctl, got, want);
        CHECK(got == want);
        CHECK(lj_loop_options_last_cmd(&cmd) == before + 1);
        CHECK(cmd.kind == LJ_CMD_LOOP_OPTS && cmd.a == LJ_RUN_SLOT0 && cmd.slot == &cue->slot[0]);
        if (w->kind == LJ_K_EDIT) fire(w, LJ_WEV_KEY_UP);                        /* unchanged text: no second command */
        CHECK(lj_loop_options_last_cmd(NULL) == before + 1);
    }
    CHECK(cue->slot[0].dir == LJ_DIR_BOUNCE && cue->slot[0].end_action == LJ_END_GOTO && cue->slot[0].first == 2 && cue->slot[0].loops == 2 && cue->slot[0].link_cue == 2);
    /* the up/down button beside First Scene */
    before = lj_loop_options_last_cmd(NULL);
    w = lj_window_find(win, "UdFirst");
    CHECK(w != NULL);
    if (w) { ((lj_w_udbutton *)w)->delta = 1; fire(w, LJ_WEV_CHANGE); }
    CHECK(cue->slot[0].first == 3 && strcmp(lj_w_edit_text(lj_window_find(win, "eFirst")), "3") == 0 && lj_loop_options_last_cmd(NULL) == before + 1);
    /* ranges: Scenes/Trig at least 1 */
    lj_w_edit_set_text(lj_window_find(win, "eTrigScenes"), "0"); fire(lj_window_find(win, "eTrigScenes"), LJ_WEV_KEY_UP);
    CHECK(cue->slot[0].scenes_per_trig == 1);
    /* Random: no loop count, the field greys [03 s5.5] */
    lj_w_radiogroup_set_index(lj_window_find(win, "RDirection"), LJ_DIR_RANDOM);
    CHECK(cue->slot[0].dir == LJ_DIR_RANDOM && cue->slot[0].loops == 0 && !lj_window_find(win, "eLoops")->enabled);
    CHECK(lj_cue_validate(cue) == NULL);
    /* Default Loop Settings */
    before = lj_loop_options_last_cmd(NULL);
    fire_click(lj_window_find(win, "bDefaultLoop"));
    CHECK(cue->slot[0].dir == LJ_DIR_FWD && cue->slot[0].first == 0 && cue->slot[0].end_action == LJ_END_STOP && cue->slot[0].scenes_per_trig == 1 && cue->slot[0].trig_on_load == 0);
    CHECK(lj_loop_options_last_cmd(NULL) == before + 1);
    fire_click(lj_window_find(win, "bDefaultLoop"));                            /* already default: nothing */
    CHECK(lj_loop_options_last_cmd(NULL) == before + 1);
    /* two highlighted slots: one command each */
    lj_cue_form_select(1, 1); lj_cue_form_select(2, 1);
    lj_win_refresh(LJ_WIN_LOOP_OPTIONS);
    CHECK(lj_loop_options_target() == 1);
    before = lj_loop_options_last_cmd(NULL);
    lj_w_edit_set_text(lj_window_find(win, "eGoto"), "4"); fire(lj_window_find(win, "eGoto"), LJ_WEV_KEY_UP);
    CHECK(cue->slot[1].goto_scene == 4 && cue->slot[2].goto_scene == 4 && cue->slot[0].goto_scene == 0);
    CHECK(lj_loop_options_last_cmd(&cmd) == before + 2 && cmd.a == LJ_RUN_SLOT0 + 2 && cmd.slot == &cue->slot[2]);
    lj_cue_form_select(-1, 0);
    lj_win_refresh(LJ_WIN_LOOP_OPTIONS);
    golden(win, "TLOOPTOOLSFORM", write);
    lj_win_close(LJ_WIN_LOOP_OPTIONS);
    CHECK(lj_win_get(LJ_WIN_LOOP_OPTIONS) == NULL);
    /* open + close without a change ("Cancel"): nothing */
    lj_win_open(LJ_WIN_LOOP_OPTIONS);
    lj_win_close(LJ_WIN_LOOP_OPTIONS);
    CHECK(lj_loop_options_last_cmd(NULL) == 0);
    /* the hotkey toggles the window */
    lj_fn_run(LJ_FN_CUE_LOOP_CONTROL, 1);
    CHECK(lj_win_get(LJ_WIN_LOOP_OPTIONS) != NULL);
    lj_fn_run(LJ_FN_CUE_LOOP_CONTROL, 1);
    CHECK(lj_win_get(LJ_WIN_LOOP_OPTIONS) == NULL);

    /* ---- Cue Time Control: 13 faders [03 s5.6] ---- */
    win = lj_win_open(LJ_WIN_CUE_TIME);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(strcmp(lj_window_caption(win)->text, "[0000 No name]") == 0);         /* cuetime.gif */
    CHECK(lj_cue_time_last_cmd(NULL) == 0);
    CHECK(lj_cue_time_text(0, buf, sizeof buf) == 0 && strcmp(buf, "Cue") == 0);  /* 100 % = as programmed */
    for (i = 1; i < LJ_CUE_TIME_FADERS; i++) CHECK(lj_cue_time_text(i, buf, sizeof buf) == 0 && strcmp(buf, "Cue") == 0);
    CHECK(lj_cue_time_set(0, 240) == 0 && lj_cue_time_last_cmd(&cmd) == 1 && cmd.kind == LJ_CMD_CUE_TIME && cmd.a == 0 && cmd.b == 240 && cue->cue_time_pct == 240);
    CHECK(lj_cue_time_text(0, buf, sizeof buf) == 0 && strcmp(buf, "240%") == 0);
    CHECK(lj_cue_time_set(3, 0) == 0 && lj_cue_time_last_cmd(NULL) == 1);        /* already Cue: nothing */
    CHECK(lj_cue_time_set(3, 147) == 0 && lj_cue_time_last_cmd(&cmd) == 2 && cmd.a == 3 && cmd.b == 147 && cue->slot[2].time_pct == 147);
    CHECK(lj_cue_time_set(0, 0) == 0 && cue->cue_time_pct == LJ_TIME_PCT_MIN);    /* the CUE fader has no "Cue" step */
    CHECK(lj_cue_time_set(5, 5000) == 0 && cue->slot[4].time_pct == LJ_TIME_PCT_MAX);
    CHECK(lj_cue_time_set(13, 50) == -1 && lj_cue_time_value(13) == -1);
    CHECK(lj_cue_time_pos_of(1, 100) == 0 && lj_cue_time_pos_of(100, 100) == 49 && lj_cue_time_pos_of(999, 100) == 99);
    CHECK(lj_cue_time_pct_at(0, 100) == 1 && lj_cue_time_pct_at(49, 100) == 100 && lj_cue_time_pct_at(99, 100) == 999);
    for (i = 8; i <= 999; i += 7) { int q = lj_cue_time_pos_of(i, 100); CHECK(q >= 0 && q < 100 && q >= lj_cue_time_pos_of(i - 7, 100)); }   /* monotonic */
    /* synchronize selected (the default Change mode) + the arrow keys */
    lj_cue_time_select(1, 1); lj_cue_time_select(2, 1);
    before = lj_cue_time_last_cmd(NULL);
    lj_cue_time_set(1, 50);
    CHECK(cue->slot[0].time_pct == 50 && cue->slot[1].time_pct == 50 && lj_cue_time_last_cmd(NULL) == before + 2);
    CHECK(lj_cue_time_key(0x26) == 1 && cue->slot[0].time_pct == 51 && cue->slot[1].time_pct == 51);
    CHECK(lj_cue_time_key(0x22) == 1 && cue->slot[0].time_pct == 41 && cue->slot[1].time_pct == 41);
    CHECK(lj_cue_time_key(0x41) == 0);
    menu_fire(win, "OneSequence1");                                              /* Change > One Sequence Only */
    lj_cue_time_set(1, 60);
    CHECK(cue->slot[0].time_pct == 60 && cue->slot[1].time_pct == 41);
    CHECK(lj_ini_geti(c->forms_ini, "CUETIMEFORM", "Sync", 1) == 0);
    menu_fire(win, "SelectedSequences1");
    menu_fire(win, "OVR_Min2");
    CHECK(lj_ini_geti(c->main_ini, "MISC", "CUETIME_REMOTE_DELTA", -1) == 2);
    menu_fire(win, "Sequences1");                                                /* Default > Sequences: every slot back to Cue */
    for (i = 0; i < LJ_CUE_SLOTS; i++) CHECK(cue->slot[i].time_pct == 0);
    CHECK(cue->cue_time_pct == LJ_TIME_PCT_MIN);
    menu_fire(win, "Cue1");
    CHECK(cue->cue_time_pct == 100);
    menu_fire(win, "CueOnly1");
    CHECK(lj_ini_geti(c->forms_ini, "CUETIMEFORM", "View", 0) == 1);
    menu_fire(win, "CueandSequences1");
    {   /* menu captions verbatim (13 s5 TCUETIMEFORM) */
        static const char *const mc[][2] = { { "CueOnly1", "&Cue Only" }, { "SequencesOnly1", "&Sequences Only" }, { "CueandSequences1", "Cue &and Sequences" },
            { "SequenceTitles1", "Sequence &Titles" }, { "SelectedSequences1", "&Synchronize Selected Sequences" }, { "OneSequence1", "&One Sequence Only" },
            { "OverrideOptions1", "Override Options" }, { "OVR_Always", "Always" }, { "OVR_Min1", "On changes only (Min 1)" }, { "OVR_Min2", "On changes only (Min 2)" },
            { "OVR_Min3", "On changes only (Min 3)" }, { "Default1", "&Default" }, { "Cue1", "&Cue" }, { "Sequences1", "&Sequences" }, { "All1", "&All" }, { "Preferences1", "&Preferences" } };
        for (i = 0; i < LJ_COUNT_OF(mc); i++) CHECK(strcmp(menu_cap(&lj_form_TCUETIMEFORM, mc[i][0]), mc[i][1]) == 0);
    }
    lj_cue_time_select(1, 0); lj_cue_time_select(2, 0);
    lj_cue_time_set(0, 240); lj_cue_time_set(1, 240); lj_cue_time_set(2, 147); lj_cue_time_set(3, 65);   /* the cuetime.gif values */
    lj_win_refresh(LJ_WIN_CUE_TIME);
    golden(win, "TCUETIMEFORM", write);
    lj_win_close(LJ_WIN_CUE_TIME);
    lj_fn_run(LJ_FN_CUE_TIME_CONTROL, 1);
    CHECK(lj_win_get(LJ_WIN_CUE_TIME) != NULL && lj_cue_time_last_cmd(NULL) == 0);
    lj_fn_run(LJ_FN_CUE_TIME_CONTROL, 1);
    CHECK(lj_win_get(LJ_WIN_CUE_TIME) == NULL);

    /* ---- transparent / flash preferences of the Cue control [03 s5.8, s5.10] ---- */
    {
        static const char *const tc[][2] = { { "TransparentCues1", "&Transparent Cues.." }, { "M_UseTransparentCues", "&Use Transparent Cues" },
            { "M_ForceTransparentCues", "&Force Transparent Cues" }, { "M_IgnoreTransparenCues", "&Ignore Transparent Cues" } /* D13: the item name keeps Martin's typo */,
            { "SequenceFlashPreferences1", "Sequence Flash Preferences" }, { "M_StickyFlashSequences", "Latch Flash Sequences" }, { "M_FlashAutoRelease", "Release Flash on Load" } };
        for (i = 0; i < LJ_COUNT_OF(tc); i++) CHECK(strcmp(menu_cap(&lj_form_TCURRENTCUE, tc[i][0]), tc[i][1]) == 0);
    }
    lj_ini_seti(c->forms_ini, "CUE", "Transparent", LJ_TRANSPARENT_FORCE);
    lj_ini_seti(c->forms_ini, "CUE", "StickyFlash", 1);
    lj_cue_form_apply_prefs();
    CHECK(c->transparent_mode == LJ_TRANSPARENT_FORCE && c->flash_latch == 1);
    CHECK(lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_TRANSPARENT && cmd.b == LJ_TRANSPARENT_FORCE);
    win = lj_win_open(LJ_WIN_CUE_FORM);
    CHECK(win != NULL);
    if (win) {
        menu_fire(win, "M_IgnoreTransparenCues");
        CHECK(c->transparent_mode == LJ_TRANSPARENT_IGNORE && lj_ini_geti(c->forms_ini, "CUE", "Transparent", -1) == LJ_TRANSPARENT_IGNORE);
        CHECK(lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_TRANSPARENT && cmd.b == LJ_TRANSPARENT_IGNORE);
        menu_fire(win, "M_StickyFlashSequences");                               /* toggles the latch off */
        CHECK(c->flash_latch == 0 && lj_ini_geti(c->forms_ini, "CUE", "StickyFlash", -1) == 0);
        lj_fn_run(LJ_FN_RELEASE_LATCHED_FLASH, 1);
        CHECK(lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_FLASH && cmd.b == 0 && cmd.c == 0 && cmd.seq[0] == NULL);
        fire(lj_window_find(win, "sbLoop"), LJ_WEV_MOUSE_UP);                    /* the cue control's loop button opens Loop Options */
        CHECK(lj_win_get(LJ_WIN_LOOP_OPTIONS) != NULL);
        fire(lj_window_find(win, "sbCueTime"), LJ_WEV_MOUSE_UP);
        CHECK(lj_win_get(LJ_WIN_CUE_TIME) != NULL);
        lj_win_close(LJ_WIN_LOOP_OPTIONS); lj_win_close(LJ_WIN_CUE_TIME);
        lj_win_close(LJ_WIN_CUE_FORM);
    }

    /* ---- cue pages [03 s5.8] ---- */
    p = lj_cue_pages_model();
    if (p->n == 0) {
        printf("windows_a_w2: SKIP cue pages: model/lj_cuepage.c is still the contract stub\n");
    } else {
        CHECK(c->cue_pages == p && p->n == 1 && strcmp(p->page[0].name, "Page 1") == 0);   /* a library without CuePageInfo.Dat shows "Page 1" */
        CHECK(lj_cue_pages_command("M_AddnewPage") == 0);
        win = lj_win_get(LJ_WIN_CUE_PAGE_NAME);
        CHECK(win != NULL);
        if (win) {
            CHECK(strcmp(lj_window_caption(win)->text, "Add New Cue Page") == 0 && strcmp(cap_of(win, "Label1"), "Cue Page Name") == 0);
            CHECK(strcmp(lj_w_edit_text(lj_window_find(win, "Edit1")), "New Page") == 0);
            golden(win, "TGETPAGEINFOFORM", write);
            lj_w_edit_set_text(lj_window_find(win, "Edit1"), "Intro");
            fire_click(lj_window_find(win, "BitBtn1"));
        }
        CHECK(lj_win_get(LJ_WIN_CUE_PAGE_NAME) == NULL && p->n == 2 && p->current == 1 && strcmp(p->page[1].name, "Intro") == 0);
        lj_cue_pages_command("PopEditPageName");
        win = lj_win_get(LJ_WIN_CUE_PAGE_NAME);
        CHECK(win && strcmp(lj_window_caption(win)->text, "Edit Page Name") == 0 && strcmp(lj_w_edit_text(lj_window_find(win, "Edit1")), "Intro") == 0);
        if (win) { lj_w_edit_set_text(lj_window_find(win, "Edit1"), "Verse"); fire_click(lj_window_find(win, "BitBtn1")); }
        CHECK(strcmp(p->page[1].name, "Verse") == 0);
        lj_cue_pages_command("PopInsertNewPage");                               /* Cancel drops it */
        win = lj_win_get(LJ_WIN_CUE_PAGE_NAME);
        CHECK(win && strcmp(lj_window_caption(win)->text, "Insert New Cue Page") == 0);
        if (win) fire_click(lj_window_find(win, "BitBtn2"));
        CHECK(lj_win_get(LJ_WIN_CUE_PAGE_NAME) == NULL && p->n == 2);
        /* cues on the current page ("Verse"): never twice [03 s5.7], select / step */
        CHECK(lj_cue_pages_add_global(12, 0) == 0 && lj_cue_pages_add_global(5, 0) == 0 && lj_cue_pages_add_global(12, 0) == 0);
        CHECK(p->page[1].n == 2 && p->page[1].cue[0] == 12 && p->page[1].cue[1] == 5);
        p->cursor = -1;
        CHECK(lj_cue_pages_step_cue(1, 0) == 12 && lj_cue_pages_step_cue(1, 0) == 5 && lj_cue_pages_step_cue(1, 0) == 0);
        CHECK(lj_cue_pages_step_page(-1) == 0 && lj_cue_pages_step_page(-1) == -1);
        lj_ini_seti(c->forms_ini, "ACUESELECT", "CircularPageSelect", 1);
        CHECK(lj_cue_pages_step_page(-1) == 1 && lj_ini_geti(c->forms_ini, "ACUESELECT", "Page", -1) == 1);
        /* the List of Cues shows the tabs and the page rows */
        win = lj_win_open(LJ_WIN_LIST_CUE);
        CHECK(win != NULL);
        if (win) {
            lj_w_tabs *t = (lj_w_tabs *)lj_window_find(win, "TC1");
            lj_w_listview *lv = (lj_w_listview *)lj_window_find(win, "lvPages");
            CHECK(t && t->ntabs == 2 && t->active == 1 && strcmp(t->tabs[0], "Page 1") == 0 && strcmp(t->tabs[1], "Verse") == 0);
            CHECK(lv && lv->n == 2 && strncmp(lv->items[0], "0012\t", 5) == 0);
            if (t && lv) { lj_w_tabs_select(&t->base, 0); CHECK(p->current == 0 && lv->n == 0); lj_w_tabs_select(&t->base, 1); CHECK(p->current == 1 && lv->n == 2); }
            menu_fire(win, "M_CircularPageSelection");                         /* toggles the pref back off */
            CHECK(lj_ini_geti(c->forms_ini, "ACUESELECT", "CircularPageSelect", -1) == 0);
            if (lv) lv->index = 0;
            menu_fire(win, "PopClearSelectedCues");
            CHECK(p->page[1].n == 1 && p->page[1].cue[0] == 5 && lv && lv->n == 1);
            menu_fire(win, "PopLockList");                                       /* locked: editing items do nothing */
            CHECK(lj_cue_pages_add_global(9, 0) == -1 && p->page[1].n == 1);
            menu_fire(win, "PopLockList");
            lj_win_close(LJ_WIN_LIST_CUE);
        }
        /* Re-order Cue Pages: Cancel drops the move, Ok applies it */
        lj_cue_pages_command("M_ReorderPages");
        win = lj_win_get(LJ_WIN_CUE_PAGES_REORDER);
        CHECK(win && strcmp(lj_window_caption(win)->text, "Re-order Cue Pages") == 0);
        if (win) {
            golden(win, "TREORDERCUEPAGESFORM", write);
            CHECK(lj_cue_pages_reorder_move(1, 0) == 0);
            fire_click(lj_window_find(win, "bCancel"));
        }
        CHECK(lj_win_get(LJ_WIN_CUE_PAGES_REORDER) == NULL && strcmp(p->page[0].name, "Page 1") == 0);
        lj_cue_pages_command("M_ReorderPages");
        win = lj_win_get(LJ_WIN_CUE_PAGES_REORDER);
        if (win) { CHECK(lj_cue_pages_reorder_move(1, 0) == 0); fire_click(lj_window_find(win, "bOk")); }
        CHECK(strcmp(p->page[0].name, "Verse") == 0 && strcmp(p->page[1].name, "Page 1") == 0 && p->page[0].n == 1);
        /* Clear All Pages ? (exe 0x1E1C68) through the confirm box */
        lj_cue_pages_command("M_ClearAllPages");
        win = lj_win_message_window();
        CHECK(win && strcmp(cap_of(win, "Label1"), "Clear All Pages ?") == 0);
        if (win) fire_click(lj_window_find(win, "BOk"));
        CHECK(p->n == 2 && p->page[0].n == 0 && p->page[1].n == 0);
        lj_cue_pages_command("M_DeleteAllPages");
        win = lj_win_message_window();
        if (win) fire_click(lj_window_find(win, "BOk"));
        CHECK(p->n == 1 && strcmp(p->page[0].name, "Page 1") == 0);
        CHECK(lj_cue_pages_command("NoSuchItem") == -1);
    }

    /* ---- RGB pack control window (LORD_FIDELITY-18): RGB Pack x10 (exe id 75) at 1:001 as the master ---- */
    c->show->fixture[0].kind = LJ_FX_BUILTIN; c->show->fixture[0].profile_id = 75; c->show->fixture[0].link = 1; c->show->fixture[0].address = 1;
    c->show->current_seq = lj_sequence_new_rec();
    if (!c->show->current_seq->scenes) lj_sequence_alloc(c->show->current_seq, 1);
    c->current_scene = 0;
    lj_sel_reset();
    lj_sel_set(0, 1, 1);
    win = c->show->current_seq->scenes ? lj_ctl_open(LJ_CTL_RGBPACK) : NULL;
    if (!c->show->current_seq->scenes) printf("windows_a_w2: SKIP RGB pack: no scene (model stubs)\n");
    else {
        const lj_scene *sc = &c->show->current_seq->scenes[0];
        CHECK(win != NULL);
        if (win) {
            CHECK(strcmp(lj_window_caption(win)->text, "Colors") == 0 && lj_ctl_rgbpack_units() == 10 && lj_ctl_rgbpack_enabled() == 0x3FF);
            w = lj_ctl_elem_find(win, "row1_fade");                             /* the O/S/F row of unit 1 Red */
            CHECK(w != NULL && sc->state[0] == LJ_OFF);
            if (w) { fire_click(w); CHECK(sc->state[0] == LJ_SNAP && lj_w_osf_state(w) == LJ_OSF_SNAP); fire_click(w); CHECK(sc->state[0] == LJ_FADE); fire_click(w); CHECK(sc->state[0] == LJ_OFF); }
            CHECK(lj_ctl_rgbpack_set(0, 200) == 0 && sc->val[0] == 200 && sc->val[27] == 200 && sc->state[0] == LJ_SNAP && sc->val[1] == 0);   /* every enabled unit, Off -> Snap */
            lj_ctl_rgbpack_enable(2, 1, 1);                                       /* Ctrl-click: unit 2 only */
            CHECK(lj_ctl_rgbpack_enabled() == 0x002 && lj_ctl_rgbpack_set(1, 100) == 0 && sc->val[4] == 100 && sc->val[1] == 0);
            lj_ctl_rgbpack_enable(0, 1, 0);                                       /* All */
            CHECK(lj_ctl_rgbpack_set(3, 85) == 0 && sc->val[1] == 255 && sc->val[0] < 10 && sc->val[2] < 10 && sc->val[29] < 10);   /* hue 85 = green */
            lj_ctl_rgbpack_enable(0, 0, 0);                                       /* None: nothing is written */
            CHECK(lj_ctl_rgbpack_set(2, 77) == 0 && sc->val[2] < 10);
            lj_ctl_rgbpack_enable(5, 1, 0);
            lj_ctl_rgbpack_update();
            CHECK(sc->val[14] == 77 && sc->val[13] == 255 && sc->val[2] < 10);
            CHECK(lj_ctl_rgbpack_get_color(5) == 0);
            lj_prog_set(14, 77, LJ_FADE); lj_prog_set(2, 0, LJ_FADE);             /* unit 5 (enabled) and unit 1 (not) Blue in Fade */
            lj_ctl_rgbpack_snap(2);
            CHECK(sc->state[14] == LJ_SNAP && sc->state[2] == LJ_FADE);
            {   uint8_t rgb[3];
                lj_hsv_to_rgb(0, 255, 255, rgb); CHECK(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0);
                lj_hsv_to_rgb(170, 255, 255, rgb); CHECK(rgb[2] == 255 && rgb[0] < 10);
                lj_hsv_to_rgb(10, 0, 128, rgb); CHECK(rgb[0] == 128 && rgb[1] == 128 && rgb[2] == 128); }
            lj_ctl_rgbpack_enable(0, 1, 0);
            lj_ctl_refresh();
            golden(win, "CTL_RGB10_RGBPACK", write);
        }
        lj_ctl_close_all();
    }
    lj_sel_reset();

    lj_win_close_all();
    lj_show_free(c->show); c->show = NULL;
    lj_ini_free(c->forms_ini); lj_ini_free(c->main_ini); c->forms_ini = c->main_ini = NULL;
    c->cue_pages = NULL;
    lj_log_set_sink(NULL, NULL);
    CHECK(w2a_bind_logs == 0);                                                  /* ME-16 both ways for every form opened above */
    printf("windows_a_w2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
