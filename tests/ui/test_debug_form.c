/* test_debug_form.c - `lj4evr_tests test_debug_form` (W4 lightcorder-debug track, W4-DBG, ME-28, design 13 item 10), headless:
 *  - View > De&bugWindow stays hidden until Main.Ini [DEBUG] DebugWindow=1 (the unhide PROPOSAL), then shows; the item
 *    reaches LJ_ACT_DEBUG_WINDOW;
 *  - TDebugForm opens with every DFM handler bound (ME-16), its labels verbatim, the hardware tabs' contents disabled;
 *  - the drop-counter row: a full command array plus one lj_ctx_post shows "Command drops: 1" (design 2.3 rule 1);
 *  - layer_of[] (ME-28) in the Misc tab: the current sequence's channel 1:001 at its rank; the perf counters in tenths
 *    (no float), Cue / Macro evaluation n/a;
 *  - TStaticsDebugForm, TMsgDebugForm ("WM_CPOPYDATA DEBUG", sic) and TMemoryViewForm ("System Memory", the DFM's 16 rows)
 *    open; System Memory fills MemoryLoad / Total / Available Physical from lj_debug_form_set_memory;
 *  - goldens W4DBG_* (tests/ui/golden, REVIEWED.tsv); LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_debug_form.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_debug_form_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int bind_complaints;
static void sink(void *ud, const char *line)
{
    (void)ud;
    if (getenv("DBG_DEBUG")) printf("  log: %s\n", line);
    if ((strstr(line, "no binding") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler"))
        && (strstr(line, "TDebugForm") || strstr(line, "TStaticsDebugForm") || strstr(line, "TMsgDebugForm") || strstr(line, "TMemoryViewForm"))) {
        bind_complaints++; printf("  log: %s\n", line);
    }
}
static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}
static void golden(lj_window *w, const char *name)
{
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
static lj_widget *in(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static const char *cap_of(lj_window *w, const char *n) { lj_widget *x = in(w, n); const char *c = x ? lj_w_caption(x) : NULL; return c ? c : ""; }
static int act_debug;
static void on_action(enum lj_desktop_action a, int arg, void *ud) { (void)arg; (void)ud; if (a == LJ_ACT_DEBUG_WINDOW) act_debug++; }
static void view_now(void) { lj_ctx *c = lj_ctx_get(); lj_lock(); lj_engine_view(c->engine, &c->view); lj_unlock(); }

int lj_test_debug_form(int argc, char **argv);
int lj_test_debug_form(int argc, char **argv)
{
    lj_ctx *c = lj_ctx_get();
    lj_window *desk, *w;
    lj_menu_rt *rt;
    lj_cmd cmd;
    lj_scene *sc;
    char want[32];
    int i, ok;
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_fs_mkdir("test_debug_form_tmp");
    c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->show->fixture[0].kind = LJ_FX_BUILTIN; c->show->fixture[0].profile_id = 19; c->show->fixture[0].link = 1; c->show->fixture[0].address = 1;
    c->show->fixture[1].kind = LJ_FX_BUILTIN; c->show->fixture[1].profile_id = 19; c->show->fixture[1].link = 1; c->show->fixture[1].address = 18;
    c->show->current_seq = lj_sequence_new_rec();
    if (!c->show->current_seq->scenes) lj_sequence_alloc(c->show->current_seq, 1);
    sc = &c->show->current_seq->scenes[0];
    sc->val[0] = 255; sc->state[0] = LJ_SNAP; sc->val[17] = 9; sc->state[17] = LJ_SNAP;
    c->engine = lj_engine_create();
    lj_lock(); lj_engine_bind(c->engine, c->show); lj_unlock();

    /* ---- the unhide (PROPOSAL) and the route ---- */
    desk = lj_desktop_create();
    CHECK(desk != NULL);
    rt = desk ? lj_menu_rt_named(lj_window_menus(desk), "M_ViewDebug") : NULL;
    CHECK(rt && !rt->visible);
    lj_debug_form_bind_fns();
    CHECK(rt && !rt->visible);                                               /* no [DEBUG] key: LJ2's hidden item stays hidden */
    lj_ini_seti(c->main_ini, "DEBUG", "DebugWindow", 1);
    lj_debug_form_bind_fns();
    CHECK(rt && rt->visible);
    lj_desktop_bind_action(LJ_ACT_DEBUG_WINDOW, on_action, NULL);
    if (rt && rt->fn) { lj_menu *m = lj_window_menus(desk); m->firing = rt->item; rt->fn(lj_window_root(desk), rt->ud); m->firing = NULL; }   /* ViewControlClick on M_ViewDebug, as lj_menu's fire() dispatches it */
    lj_desktop_bind_action(LJ_ACT_DEBUG_WINDOW, NULL, NULL);

    /* ---- TDebugForm ---- */
    bind_complaints = 0;
    w = lj_win_open(LJ_WIN_DEBUG);
    CHECK(w != NULL && lj_win_get(LJ_WIN_DEBUG) == w && bind_complaints == 0);
    if (!w) goto out;
    CHECK(strcmp(lj_window_caption(w)->text, "DebugForm") == 0);
    CHECK(strcmp(cap_of(w, "Label55"), "DMX engine, % , 1 sec avr.") == 0 && strcmp(cap_of(w, "Label57"), "Cue evaluation, %, 1 sec avr.") == 0
          && strcmp(cap_of(w, "Label56"), "Process time total, ms") == 0 && strcmp(cap_of(w, "Label58"), "Process load, % 5 sec avr.") == 0
          && strcmp(cap_of(w, "Label60"), "Macro evaluation, %, 1 sec avr.") == 0 && strcmp(cap_of(w, "sbEnablePerformanceCounters"), "Enable Performance Counters") == 0);
    CHECK(in(w, "Label1") && !in(w, "Label1")->enabled && in(w, "reISA") && !in(w, "reISA")->enabled && in(w, "lv2532") && !in(w, "lv2532")->enabled
          && in(w, "lUSB1Idle") && !in(w, "lUSB1Idle")->enabled && in(w, "reDetect") && !in(w, "reDetect")->enabled);   /* PCI ISA 2532 USB Hardware detect */
    CHECK(in(w, "Label51")->enabled && in(w, "ReMisc")->enabled);

    /* the drop-counter row: 256 queued + 1 dropped (the array is drained only by a tick) */
    view_now();
    lj_debug_form_refresh(LJ_WIN_DEBUG);
    CHECK(strcmp(lj_debug_form_label("Label53"), "Command drops: 0") == 0);
    memset(&cmd, 0, sizeof cmd); cmd.kind = LJ_CMD_BLACKOUT; cmd.a = 0;
    for (i = 0; i < 257; i++) lj_ctx_post(&cmd);
    view_now();
    lj_debug_form_refresh(LJ_WIN_DEBUG);
    CHECK(strcmp(lj_debug_form_label("Label53"), "Command drops: 1") == 0);
    lj_lock(); lj_engine_tick(c->engine, lj_clock_us()); lj_unlock();
    lj_ctx_set_scene(0);                                                     /* the current sequence: 1:001 = 255, 1:018 = 9 */
    lj_lock(); lj_engine_tick(c->engine, lj_clock_us()); lj_unlock();
    view_now();
    lj_debug_form_refresh(LJ_WIN_DEBUG);
    CHECK(strcmp(lj_debug_form_label("Label53"), "Command drops: 1") == 0);   /* cumulative */
    snprintf(want, sizeof want, "Tick: %llu", (unsigned long long)c->view.tick_no);
    CHECK(c->view.tick_no == 2 && strcmp(lj_debug_form_label("Label51"), want) == 0);

    /* ME-28: layer_of in the Misc tab */
    {   const lj_frame *fr = lj_engine_frame(c->engine);
        int r0 = fr->layer_of[0], r17 = fr->layer_of[17];
        const char *l0 = lj_debug_form_misc_line(0), *l1 = lj_debug_form_misc_line(1);
        if (getenv("DBG_DEBUG")) printf("  out %u r0 %d r17 %d merged %u\n", fr->out[0][0], r0, r17, fr->merged[0]);
        CHECK(fr->out[0][0] == 255 && r0 > 0 && r17 == r0);
        CHECK(lj_debug_form_misc_lines() == 2 && strncmp(l0, "1:001 ", 6) == 0 && l0[6] == '0' + r0 && l0[7] == '0' && strncmp(l1, "1:017 ", 6) == 0 && l1[7] == '0' + r17);
    }

    /* ME-144: a late tick, counted by the engine thread beside lj_log_overrun, reads Overruns: 1 */
    lj_lock(); lj_engine_note_overrun(c->engine); lj_unlock();
    view_now();
    lj_debug_form_refresh(LJ_WIN_DEBUG);
    CHECK(strcmp(lj_debug_form_label("Label54"), "Overruns: 1") == 0);

    /* perf counters: off = blank, on = tenths with the host separator; n/a where the engine has no counter */
    CHECK(strcmp(lj_debug_form_label("lpfCounter1"), "") == 0);
    CHECK(lj_debug_form_load_tenths(1000, 25000) == 40 && lj_debug_form_load_tenths(25000, 25000) == 1000 && lj_debug_form_load_tenths(5, 0) == 0);
    c->view.tick_us_last = 1234; c->view.tick_us_max = 4321; c->view.overruns = 3; c->view.sacn_pps[0] = 40; c->view.sacn_pps[1] = 1;
    lj_w_click(in(w, "sbEnablePerformanceCounters"));
    CHECK(lj_w_speedbutton_down(in(w, "sbEnablePerformanceCounters")));
    for (i = 0; i < 128; i++) lj_debug_form_refresh(LJ_WIN_DEBUG);              /* the sample ring holds only the fixed view: the averages are exact */
    CHECK(strcmp(lj_debug_form_label("lpfCounter1"), "4.9") == 0 || strcmp(lj_debug_form_label("lpfCounter1"), "4,9") == 0);   /* 1234 / 25000 */
    ok = (strcmp(lj_debug_form_label("lpfCounter3"), "1.234") == 0 || strcmp(lj_debug_form_label("lpfCounter3"), "1,234") == 0);
    CHECK(ok && strcmp(lj_debug_form_label("lpfCounter2"), "n/a") == 0 && strcmp(lj_debug_form_label("lpfCounter5"), "n/a") == 0);
    CHECK(strcmp(lj_debug_form_label("Label52"), "Tick time: 1234 us (max 4321 us)") == 0 && strcmp(lj_debug_form_label("Label68"), "Link 1: 40 packets/s") == 0);
    {   lj_engine_status keep = c->view;                                     /* golden: every sample equal, so the averages are fixed */
        lj_widget *pc = in(w, "tsDebug");
        golden(w, "W4DBG_DEBUG");
        lj_w_tabs_select(pc, 1);                                             /* Misc: the layer readout */
        lj_debug_form_refresh(LJ_WIN_DEBUG);
        golden(w, "W4DBG_MISC");
        lj_w_tabs_select(pc, 2);                                             /* PCI Debug, greyed */
        golden(w, "W4DBG_PCI");
        c->view = keep;
    }

    /* ---- the other three ---- */
    w = lj_win_open(LJ_WIN_STATICS_DEBUG);
    CHECK(w && bind_complaints == 0 && strcmp(lj_window_caption(w)->text, "StaticsDebugForm") == 0 && strcmp(cap_of(w, "lUpdateEval"), "Evals: n/a") == 0);
    c->view.static_active[0] = 1; c->view.static_slot_of[0] = 66; c->view.static_level[0] = 100;   /* page 2 slot 3 */
    lj_win_refresh(LJ_WIN_STATICS_DEBUG);
    CHECK(in(w, "lvActive") && ((lj_w_listview *)in(w, "lvActive"))->n == 1 && strcmp(((lj_w_listview *)in(w, "lvActive"))->items[0], "Page 2 slot 3 (100%)") == 0);
    golden(w, "W4DBG_STATICS");
    w = lj_win_open(LJ_WIN_MSG_DEBUG);
    CHECK(w && bind_complaints == 0 && strcmp(lj_window_caption(w)->text, "WM_CPOPYDATA DEBUG") == 0);
    golden(w, "W4DBG_MSG");
    {   lj_ui_event ev; memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CLOSE; lj_window_event(w, &ev); }   /* no OnClose in its DFM: the root's close */
    lj_win_refresh(LJ_WIN_MSG_DEBUG);
    CHECK(lj_win_get(LJ_WIN_MSG_DEBUG) == NULL);
    w = lj_win_open(LJ_WIN_MEMORY_VIEW);
    CHECK(w && bind_complaints == 0 && strcmp(lj_window_caption(w)->text, "System Memory") == 0);
    if (w) {
        lj_w_listview *lv = (lj_w_listview *)in(w, "LV");
        CHECK(lv && lv->n == 16 && strcmp(lv->items[0], "Total Address Space\tn/a\t") == 0 && strcmp(lv->items[2], "Total Comitted\tn/a\t") == 0);   /* Martin's spelling */
        lj_debug_form_set_memory(1ull << 30, 4ull << 30);
        lj_win_refresh(LJ_WIN_MEMORY_VIEW);
        CHECK(lv && strcmp(lv->items[9], "MemoryLoad\t75 %\t") == 0 && strcmp(lv->items[10], "Total Physical\t4194304 KB\t") == 0 && strcmp(lv->items[11], "Available Physical\t1048576 KB\t") == 0);
        golden(w, "W4DBG_MEMORY");
    }
    {   lj_ui_event ev; memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CLOSE; lj_window_event(lj_win_get(LJ_WIN_DEBUG), &ev); }
    CHECK(lj_win_get(LJ_WIN_DEBUG) != NULL);                                 /* FormClose closes at the next refresh, never inside its own event */
    lj_win_refresh(LJ_WIN_DEBUG);
    CHECK(lj_win_get(LJ_WIN_DEBUG) == NULL);

out:
    CHECK(act_debug == 1);
    lj_win_close_all();
    if (desk) lj_desktop_destroy(desk);
    lj_log_set_sink(NULL, NULL);
    lj_engine_destroy(c->engine); c->engine = NULL;
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_lock(); lj_show_free(c->show); c->show = NULL; lj_unlock();
    memset(&c->view, 0, sizeof c->view);
    printf("test_debug_form: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
