/* test_da2532_fingers.c - `lj4evr_tests test_da2532_fingers`: the W4 da2532-fingers track headless (W4-2532, W4-FINGERS, I8, I9,
 * F8, ME-16, ME-23). A scratch library test_da2532_tmp/ under the cwd.
 *  - every one of the 17 ids opens (Virtual Fingers through "Fingers is disabled - do you wish to activate now ?"), with no
 *    "no binding" / "unknown event" / "matches no DFM handler" line, and closes;
 *  - 2532: latch runs the latch value, flash flashes while held and releases on key-up, user keys, pages / banks through the
 *    EXT rows, Transparent Pages, Record Cue, Customize Text, the setup round trip and five refused files;
 *  - a `midi` script row (lj_uiscript, headless): note 60 = 2532 Latch 1 latches its cue, note 61 = 2532 Flash 1 flashes its
 *    sequence and note-off releases it;
 *  - Learn: Alt+click equivalents arm Learn, the next note / CC binds the EXT row;
 *  - Fingers: a CC on a learnt fader moves sub-master 1, a bump flashes Bump Sub 1, GM through the level convention, F keys,
 *    slot rows, jog GM Relative, Fingers.Ini round trip and a refused file;
 *  - keyboard: a .Hot entry naming an EXT row runs it through lj_desktop_key;
 *  - goldens W4DA_* (tests/ui/golden, REVIEWED.tsv), LJ_GOLDEN_UPDATE=1 rewrites them. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_uiscript.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "gfx/lj_bmp.h"
#include "io/lj_dmxin.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"
#include "io/lj_midi.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_da2532.h"
#include "windows/lj_fingers.h"
#include "windows/lj_master_intensity.h"
#include "windows/lj_prefs.h"
#include "windows/lj_warnings.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif
#define TMP "test_da2532_tmp/"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static char bad_log[512];
static void sink(void *ud, const char *line)
{
    (void)ud;
    if ((strstr(line, "no binding for handler") || strstr(line, "unknown event") || strstr(line, "matches no DFM handler")) && !bad_log[0]) snprintf(bad_log, sizeof bad_log, "%s", line);
}

/* the program rows record what reached them */
static struct { int calls; enum lj_fn fn; uint32_t num; int edge; } hit;
static void record(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)ud; hit.calls++; hit.fn = fn; hit.num = prog; hit.edge = edge; }
static void rebind_programs(void)
{
    static const enum lj_fn f[] = { LJ_FN_PROG_SEQ_FLASH, LJ_FN_PROG_CUE, LJ_FN_PROG_CUELIST, LJ_FN_PROG_BGCUE, LJ_FN_BLACKOUT_RESTORE, LJ_FN_BUMP_SUB_FLASH_1, LJ_FN_TOGGLE_SLOT_ENABLE_3, LJ_FN_TRIG_SEQ_DEFAULT_2 };
    int i;
    for (i = 0; i < LJ_COUNT_OF(f); i++) lj_fn_bind(f[i], record, NULL);
}

static void msg_answer(int ok) { lj_window *m = lj_win_message_window(); lj_widget *b = m ? lj_window_find(m, ok ? "BOk" : "BCancel") : NULL; if (b) lj_w_click(b); else { fails++; printf("FAIL no message box\n"); } }
static const char *msg_line(void) { lj_window *m = lj_win_message_window(); const char *c = m ? lj_w_caption(lj_window_find(m, "Label1")) : NULL; return c ? c : ""; }
static void pump(void) { uint8_t msg[3]; while (lj_midi_poll(msg)) lj_midi_dispatch(lj_ctx_get()->midi, msg); }
static void inject(uint8_t a, uint8_t b, uint8_t c) { uint8_t m[3]; m[0] = a; m[1] = b; m[2] = c; lj_midi_inject(m); pump(); }

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
static void write_text(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

/* ---- every id opens from lj_win_open (the family table) with every DFM handler bound ---- */
static void test_open_all(void)
{
    int id;
    bad_log[0] = 0;
    CHECK(lj_win_open(LJ_WIN_FINGERS) == NULL);                                   /* Fingers.Ini Mode = Disable: the exe prompt first */
    CHECK(strcmp(msg_line(), "Fingers is disabled - do you wish to activate now ?") == 0);
    msg_answer(1);
    CHECK(lj_win_get(LJ_WIN_FINGERS) != NULL && lj_fingers_mode() == LJ_FG_VIRTUAL);
    for (id = LJ_WIN_DA2532; id <= LJ_WIN_FINGERS_UD_FUNCTIONS; id++) {
        lj_window *w;
        if (id == LJ_WIN_DA2532_TEXT) { lj_da2532_assign(1, 1, 1, 0, LJ_DA_CUE, 12); lj_win_open(LJ_WIN_DA2532); lj_da2532_select(1, 1); CHECK(lj_da2532_edit_text(0, 0) == 0); }
        w = lj_win_open((enum lj_win)id);
        if (!w) { fails++; printf("FAIL open %s\n", lj_win_id_name((enum lj_win)id)); continue; }
        CHECK(strcmp(lj_window_desc(w)->class_name, lj_win_class((enum lj_win)id)) == 0);
    }
    CHECK(bad_log[0] == 0);
    if (bad_log[0]) printf("  log: %s\n", bad_log);
    for (id = LJ_WIN_FINGERS_UD_FUNCTIONS; id >= LJ_WIN_DA2532; id--) { lj_win_close((enum lj_win)id); CHECK(lj_win_get((enum lj_win)id) == NULL); }
}

/* ---- the 2532 model ---- */
static void test_2532(lj_library *lib)
{
    char path[LJ_PATH_CAP];
    const lj_da_val *v;
    lj_da2532_clear_all();
    CHECK(lj_da2532_assign(1, 1, 1, 0, LJ_DA_CUE, 12) == 0 && lj_da2532_assign(1, 1, 1, 1, LJ_DA_SEQ, 3) == 0);
    CHECK(lj_da2532_assign(1, 2, 8, 0, LJ_DA_FN, LJ_FN_BLACKOUT_RESTORE) == 0);
    CHECK(lj_da2532_assign(2, 1, 1, 0, LJ_DA_CL, 4) == 0 && lj_da2532_assign(3, 1, 2, 0, LJ_DA_BG, 5) == 0);
    CHECK(lj_da2532_assign(1, 1, 2, 1, LJ_DA_CUE, 1) == -1);                      /* flash keys take sequences / functions only */
    CHECK(lj_da2532_assign(1, 1, 2, 0, LJ_DA_SEQ, 1) == -1 && lj_da2532_assign(19, 1, 1, 0, LJ_DA_CUE, 1) == -1 && lj_da2532_assign(1, 8, 1, 0, LJ_DA_CUE, 1) == -1);
    CHECK(lj_da2532_assign(1, 1, 3, 0, LJ_DA_FN, LJ_FN_PROG_CUE) == -1);          /* programs travel as their own types */
    CHECK(lj_da2532_assign_user(1, LJ_FN_DA2532_PAGE_2) == 0 && lj_da2532_assign_user(19, LJ_FN_BLACKOUT) == -1);
    lj_da2532_set_page(1); lj_da2532_set_bank(1);
    memset(&hit, 0, sizeof hit);
    CHECK(lj_da2532_latch(1, 1) == 0 && hit.calls == 1 && hit.fn == LJ_FN_PROG_CUE && hit.num == 12 && hit.edge == 1);
    CHECK(lj_da2532_latch(1, 0) == 0 && hit.calls == 1);                           /* a latched cue holds on release */
    CHECK(lj_da2532_flash(1, 1) == 0 && hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.num == 3 && hit.edge == 1);
    lj_da2532_set_bank(2);                                                         /* the release reaches the flashed value even after a bank change */
    CHECK(lj_da2532_flash(1, 0) == 0 && hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.num == 3 && hit.edge == 0);
    memset(&hit, 0, sizeof hit);
    CHECK(lj_da2532_latch(8, 1) == 0 && hit.fn == LJ_FN_BLACKOUT_RESTORE);
    CHECK(lj_da2532_latch(7, 1) == -1);                                            /* an empty key */
    /* EXT rows: page / bank / user keys */
    lj_fn_run(LJ_FN_DA2532_BANK_1, 1); CHECK(lj_da2532_bank() == 1);
    lj_fn_run(LJ_FN_DA2532_USER_1, 1); CHECK(lj_da2532_page() == 2);              /* user key 1 = 2532 Page 2 */
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_DA2532_LATCH_1, 1); CHECK(hit.fn == LJ_FN_PROG_CUELIST && hit.num == 4);
    lj_fn_run(LJ_FN_DA2532_PAGE_3, 1); CHECK(lj_da2532_page() == 3);
    memset(&hit, 0, sizeof hit);
    CHECK(lj_da2532_latch(1, 1) == -1 && hit.calls == 0);                          /* page 3 A1 is empty */
    lj_ini_seti(lj_ctx_get()->main_ini, "2532", "M_TransparentPage", 1);           /* Transparent Pages: the same bank / key on the other pages */
    CHECK(lj_da2532_latch(1, 1) == 0 && hit.fn == LJ_FN_PROG_CUE && hit.num == 12);
    lj_ini_seti(lj_ctx_get()->main_ini, "2532", "M_TransparentPage", 0);
    /* Record Cue: the next latch press stores the current cue */
    lj_ctx_get()->view.cue_number = 77;
    CHECK(lj_da2532_set_record(LJ_DA_CUE) == 0);
    memset(&hit, 0, sizeof hit);
    CHECK(lj_da2532_latch(4, 1) == 0 && hit.calls == 0);
    v = lj_da2532_value(3, 1, 4, 0);
    CHECK(v && v->type == LJ_DA_CUE && v->num == 77);
    lj_da2532_set_record(LJ_DA_NONE);
    lj_ctx_get()->view.cue_number = 0;
    /* Customize Text through Edit 2532 Text */
    lj_da2532_set_page(1);
    CHECK(lj_win_open(LJ_WIN_DA2532) != NULL && lj_da2532_select(1, 1) == 0 && lj_da2532_edit_text(0, 0) == 0);
    {
        lj_window *t = lj_win_get(LJ_WIN_DA2532_TEXT);
        CHECK(t && strcmp(lj_w_edit_text(lj_window_find(t, "Edit1")), "Cue 12") == 0);
        lj_w_edit_set_text(lj_window_find(t, "Edit1"), "Blue Circles");
        lj_w_click(lj_window_find(t, "BitBtn1"));
        CHECK(lj_win_get(LJ_WIN_DA2532_TEXT) == NULL && strcmp(lj_da2532_value(1, 1, 1, 0)->text, "Blue Circles") == 0);
    }
    /* the file: round trip, then refused files leave the setup alone */
    CHECK(lj_da2532_save() == 0);
    lj_da2532_clear_all();
    CHECK(lj_da2532_load() == 0);
    v = lj_da2532_value(1, 1, 1, 0); CHECK(v->type == LJ_DA_CUE && v->num == 12 && strcmp(v->text, "Blue Circles") == 0);
    v = lj_da2532_value(1, 1, 1, 1); CHECK(v->type == LJ_DA_SEQ && v->num == 3);
    v = lj_da2532_value(1, 2, 8, 0); CHECK(v->type == LJ_DA_FN && v->num == LJ_FN_BLACKOUT_RESTORE);
    CHECK(lj_library_path(lib, "2532Setup.Ini", path, sizeof path) == 0);
    {
        static const char *const bad[] = {
            "[PAGE1]\r\nA1L=cue 0\r\n", "[PAGE1]\r\nA1L=fn LJ_FN_NO_SUCH\r\n", "[PAGE1]\r\nA1F=cue 4\r\n",
            "[SETUP]\r\nPage=19\r\n", "[USER]\r\nU1=seq 2\r\n", "[PAGE1]\r\nA1L=cue -3\r\n" };
        int i;
        for (i = 0; i < LJ_COUNT_OF(bad); i++) {
            write_text(path, bad[i]);
            CHECK(lj_da2532_load() == -1);
            v = lj_da2532_value(1, 1, 1, 0); CHECK(v->type == LJ_DA_CUE && v->num == 12);   /* unchanged */
        }
    }
    CHECK(lj_da2532_save() == 0);
}

/* ---- a `midi` script row: Latch 1 latches, Flash 1 flashes and releases on note-off (W4-2532) ---- */
static lj_window *resolve(void *ud, const char *form) { (void)ud; return lj_uiscript_resolve_default(form); }
static const lj_engine_status *view(void *ud) { (void)ud; return &lj_ctx_get()->view; }
static void test_midi_script(void)
{
    lj_midi_map *m = lj_ctx_get()->midi;
    lj_midi_map_entry e;
    lj_uiscript_host host;
    int r, calls[3] = { 0, 0, 0 };
    lj_midi_init(m);
    memset(&e, 0, sizeof e); e.kind = LJ_MIDI_FN; e.ch = LJ_MIDI_CH_ALL; e.note = 60; e.fn = LJ_FN_DA2532_LATCH_1;
    CHECK(lj_midi_assign(m, &e) == 0);
    e.note = 61; e.fn = LJ_FN_DA2532_FLASH_1;
    CHECK(lj_midi_assign(m, &e) == 0);
    m->enabled[LJ_MIDI_FN] = 1;
    lj_da2532_set_page(1); lj_da2532_set_bank(1);
    host.ud = NULL; host.resolve = resolve; host.view = view;
    lj_uiscript_set_host(&host);
    write_text(TMP "w4_da2532_midi.lj4s", "midi 90 3C 7F\nmidi 90 3D 7F\nmidi 80 3D 00\nquit\n");
    CHECK(lj_uiscript_load(TMP "w4_da2532_midi.lj4s") == 0);
    memset(&hit, 0, sizeof hit);
    for (r = 0; r < 3 && lj_uiscript_step() == 1; r++) {                          /* one script line, then the idle pass that dispatches (lj_app_midi_dispatch) */
        pump();
        calls[r] = hit.calls * 1000 + (int)hit.fn * 10 + hit.edge;
        if (r == 0) CHECK(hit.fn == LJ_FN_PROG_CUE && hit.num == 12 && hit.edge == 1);
        if (r == 1) CHECK(hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.num == 3 && hit.edge == 1);
        if (r == 2) CHECK(hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.num == 3 && hit.edge == 0);   /* note-off releases the flash */
    }
    CHECK(hit.calls == 3 && calls[2] != 0);
    lj_uiscript_set_host(NULL);
#ifdef LJ_SOURCE_DIR
    {   char p[512];
        snprintf(p, sizeof p, "%s/tests/ui/scripts/w4_da2532_fingers.lj4s", LJ_SOURCE_DIR);   /* the on-screen walkthrough parses */
        CHECK(lj_uiscript_load(p) == 0); }
#endif
}

/* ---- Learn for the EXT rows: notes for keys, CCs for the level rows ---- */
static void test_learn(void)
{
    lj_midi_map *m = lj_ctx_get()->midi;
    const lj_midi_map_entry *e;
    lj_midi_init(m);
    CHECK(lj_da2532_learn(LJ_FN_DA2532_USER_3) == 0);
    inject(0xB1, 7, 64);                                                           /* a CC does not satisfy a key's Learn */
    lj_da2532_refresh(LJ_WIN_DA2532);
    inject(0x92, 70, 100);                                                         /* note 70 on channel 3 */
    lj_da2532_refresh(LJ_WIN_DA2532);
    e = lj_midi_match(m, 1 << LJ_MIDI_FN, 3, 70);
    CHECK(e && e->fn == LJ_FN_DA2532_USER_3 && m->enabled[LJ_MIDI_FN] == 1);
    CHECK(lj_fingers_learn(LJ_FN_FINGERS_FADER_1) == 0);
    inject(0xB0, 20, 10);
    lj_fingers_refresh(LJ_WIN_FINGERS);
    e = lj_midi_match(m, 1 << LJ_MIDI_CC, 1, 20);
    CHECK(e && e->fn == LJ_FN_FINGERS_FADER_1 && m->enabled[LJ_MIDI_CC] == 1);
}

/* ---- Fingers: fader CC -> sub-master 1, bump flashes, GM, F keys, slots, jog (W4-FINGERS) ---- */
static void test_fingers(lj_library *lib)
{
    lj_midi_map *m = lj_ctx_get()->midi;
    lj_midi_map_entry e;
    lj_cmd cmd;
    char path[LJ_PATH_CAP];
    CHECK(lj_fingers_set_page(1) == 0);                                            /* page A = Intensity Group (default) */
    inject(0xB0, 20, 127);                                                         /* the fader learnt above */
    CHECK(lj_fingers_fader_value(1) == 100 && lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_SET_MASTER && cmd.a == 1 && cmd.b == 255);
    inject(0xB0, 20, 64);
    CHECK(lj_fingers_fader_value(1) == 50 && lj_master_intensity_last_cmd(&cmd) && cmd.a == 1 && cmd.b == 128);
    CHECK(lj_fingers_set_page_function(1, 11) == 0);                                /* W5: page A = DMX in (13-24): fader 2 = DMX-in channel 14 */
    lj_dmxin_reset();
    lj_fingers_fader(2, 100); CHECK(lj_dmxin_frame_get()->raw[13] == 255 && lj_dmxin_frame_get()->live);
    lj_fingers_fader(2, 0);   CHECK(lj_dmxin_frame_get()->raw[13] == 0);
    lj_dmxin_reset();
    CHECK(lj_fingers_set_page_function(1, 6) == 0);                                 /* back to Intensity Group */
    memset(&e, 0, sizeof e); e.kind = LJ_MIDI_FN; e.ch = LJ_MIDI_CH_ALL; e.note = 40; e.fn = LJ_FN_FINGERS_BUMP_1;
    CHECK(lj_midi_assign(m, &e) == 0);
    memset(&hit, 0, sizeof hit);
    inject(0x90, 40, 100); CHECK(hit.fn == LJ_FN_BUMP_SUB_FLASH_1 && hit.edge == 1);
    inject(0x80, 40, 0);   CHECK(hit.fn == LJ_FN_BUMP_SUB_FLASH_1 && hit.edge == 0);
    /* ME-143: press on page A (Intensity), switch to page B (Statics), release: the flash that fired is released */
    inject(0x90, 40, 100); CHECK(hit.fn == LJ_FN_BUMP_SUB_FLASH_1 && hit.edge == 1);
    CHECK(lj_fingers_set_page(2) == 0);
    inject(0x80, 40, 0);   CHECK(hit.fn == LJ_FN_BUMP_SUB_FLASH_1 && hit.edge == 0);
    CHECK(lj_fingers_set_page(1) == 0);
    e.kind = LJ_MIDI_CC; e.note = 7; e.fn = LJ_FN_FINGERS_GM;
    CHECK(lj_midi_assign(m, &e) == 0);
    inject(0xB0, 7, 0);
    CHECK(lj_fingers_fader_value(0) == 0 && lj_master_intensity_last_cmd(&cmd) && cmd.a == 0 && cmd.b == 0);
    /* F keys: hotkey function and a cue */
    CHECK(lj_fingers_assign_fkey(0, 1, LJ_DA_FN, LJ_FN_BLACKOUT_RESTORE) == 0 && lj_fingers_assign_fkey(0, 2, LJ_DA_CUE, 9) == 0);
    CHECK(lj_fingers_assign_fkey(0, 3, LJ_DA_FN, LJ_FN_PROG_CUE) == -1 && lj_fingers_assign_fkey(4, 1, LJ_DA_CUE, 1) == -1);
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_F_1, 1); CHECK(hit.fn == LJ_FN_BLACKOUT_RESTORE);
    lj_fn_run(LJ_FN_FINGERS_F_2, 1); CHECK(hit.fn == LJ_FN_PROG_CUE && hit.num == 9);
    lj_fingers_set_shift(1);
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_F_2, 1); CHECK(hit.calls == 0);                        /* shift 1 has nothing on F2 */
    lj_fingers_set_shift(0);
    /* slot rows: top = Slot On/Off, middle = Slot Trig Def (defaults) */
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_SLOT_TOP_3, 1); CHECK(hit.fn == LJ_FN_TOGGLE_SLOT_ENABLE_3);
    lj_fn_run(LJ_FN_FINGERS_SLOT_MID_2, 1); CHECK(hit.fn == LJ_FN_TRIG_SEQ_DEFAULT_2 && hit.edge == 1);
    lj_fn_run(LJ_FN_FINGERS_SLOT_MID_2, 0); CHECK(hit.fn == LJ_FN_TRIG_SEQ_DEFAULT_2 && hit.edge == 0);   /* Trig Seq is a flash row */
    /* jog 3 = GM Relative: the level convention, the move is the step */
    lj_fingers_gm(50);
    lj_fn_run_prog(LJ_FN_FINGERS_JOG_3, 40, 1); CHECK(lj_fingers_fader_value(0) == 50);   /* the first value only sets the reference */
    lj_fn_run_prog(LJ_FN_FINGERS_JOG_3, 45, 1); CHECK(lj_fingers_fader_value(0) == 55);
    lj_fn_run_prog(LJ_FN_FINGERS_JOG_3, 35, 1); CHECK(lj_fingers_fader_value(0) == 45);
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_BO, 1); CHECK(hit.fn == LJ_FN_BLACKOUT_RESTORE);
    /* Fingers.Ini round trip and a refused file */
    CHECK(lj_fingers_save() == 0 && lj_fingers_load() == 0);
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_F_2, 1); CHECK(hit.fn == LJ_FN_PROG_CUE && hit.num == 9);
    CHECK(lj_library_path(lib, "Fingers.Ini", path, sizeof path) == 0);
    write_text(path, "[SETUP]\r\nMode=1\r\n[FUNCTIONS]\r\nS0F1=fn LJ_FN_NO_SUCH\r\n");
    CHECK(lj_fingers_load() == -1);
    write_text(path, "[SETUP]\r\nMode=1\r\n[JOGS]\r\nJog1=99\r\n");
    CHECK(lj_fingers_load() == -1);
    memset(&hit, 0, sizeof hit);
    lj_fn_run(LJ_FN_FINGERS_F_2, 1); CHECK(hit.fn == LJ_FN_PROG_CUE && hit.num == 9);   /* kept */
    CHECK(lj_fingers_save() == 0);
    /* Fingers Setup: the serial item and COM port are greyed, OK keeps Virtual Mode */
    {
        lj_window *w = lj_win_open(LJ_WIN_FINGERS_SETUP);
        lj_widget *rg = w ? lj_window_find(w, "rgPort") : NULL, *r;
        int i;
        CHECK(rg && lj_w_radiogroup_index(rg) == LJ_FG_VIRTUAL);
        for (i = 0, r = rg ? rg->first : NULL; r; r = r->next, i++) if (i == 2) CHECK(!r->enabled && r->hint && strcmp(r->hint, "Not available in LJ4EVR") == 0);
        CHECK(w && !lj_window_find(w, "cbPort")->enabled);
        if (w) lj_w_click(lj_window_find(w, "bOk"));
        CHECK(lj_win_get(LJ_WIN_FINGERS_SETUP) == NULL && lj_fingers_mode() == LJ_FG_VIRTUAL);
    }
}

/* ---- keyboard: a .Hot entry naming an EXT row (Edit Hotkeys does not list them) ---- */
static void test_keyboard(void)
{
    lj_hotkeys *h = lj_ctx_get()->hotkeys;
    lj_hotkey k;
    lj_key_ev ev;
    lj_hotkeys_init(h);
    memset(&k, 0, sizeof k); k.key = 'Q'; k.fn = LJ_FN_DA2532_FLASH_1;
    CHECK(lj_hotkeys_assign(h, &k) == 0 && lj_hotkeys_validate(h) == NULL);
    lj_da2532_set_page(1); lj_da2532_set_bank(1);
    memset(&hit, 0, sizeof hit);
    memset(&ev, 0, sizeof ev); ev.type = LJ_EV_KEY_DOWN; ev.key = 'Q';
    lj_desktop_key(&ev, 0);
    CHECK(hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.num == 3 && hit.edge == 1);
    ev.type = LJ_EV_KEY_UP;
    lj_desktop_key(&ev, 0);
    CHECK(hit.fn == LJ_FN_PROG_SEQ_FLASH && hit.edge == 0);
}

/* ---- goldens, one per window ---- */
static void test_goldens(void)
{
    static const char *const name[] = { "W4DA_DA2532", "W4DA_FUNCTIONS", "W4DA_COLORS", "W4DA_TAB_COLORS", "W4DA_TAB_HEIGHT", "W4DA_TEXT", "W4DA_PAGE_NAMES",
        "W4DA_FINGERS", "W4DA_FINGERS_SETUP", "W4DA_FINGERS_STATUS", "W4DA_FINGERS_ERROR", "W4DA_FINGERS_SKIN", "W4DA_FINGERS_FUNCTIONS",
        "W4DA_FINGERS_JOG", "W4DA_FINGERS_PAGE", "W4DA_FINGERS_SLOT", "W4DA_FINGERS_UD" };
    int id;
    lj_da2532_set_page(1); lj_da2532_set_bank(1);
    lj_da2532_assign(1, 2, 1, 0, LJ_DA_CL, 2); lj_da2532_assign(1, 3, 2, 1, LJ_DA_SEQ, 6); lj_da2532_assign(1, 4, 3, 0, LJ_DA_BG, 1);
    lj_da2532_assign_user(1, LJ_FN_DA2532_PAGE_2);
    lj_fingers_fkey(1, 0); lj_fingers_fkey(2, 0); lj_fn_run(LJ_FN_FINGERS_SLOT_TOP_3, 0);   /* nothing held in the shots */
    lj_fingers_set_page(1); lj_fingers_fader(3, 70); lj_fingers_gm(80);
    for (id = LJ_WIN_DA2532; id <= LJ_WIN_FINGERS_UD_FUNCTIONS; id++) {
        if (id == LJ_WIN_DA2532_TEXT) { lj_win_open(LJ_WIN_DA2532); lj_da2532_select(1, 1); lj_da2532_edit_text(0, 0); }
        if (id == LJ_WIN_DA2532_TAB_HEIGHT || id == LJ_WIN_DA2532_TEXT || id == LJ_WIN_DA2532_PAGE_NAMES) lj_da2532_select(1, 1);
        golden(lj_win_open((enum lj_win)id), name[id - LJ_WIN_DA2532]);
        if (id != LJ_WIN_DA2532) lj_win_close((enum lj_win)id);
    }
    lj_win_close(LJ_WIN_DA2532);
}

int lj_test_da2532_fingers(int argc, char **argv);
int lj_test_da2532_fingers(int argc, char **argv)
{
    static lj_library lib;
    static lj_sysprefs prefs;
    static lj_hotkeys hk;
    static lj_midi_map midi;
    lj_ctx *c = lj_ctx_get();
    (void)argc; (void)argv;
    lj_log_set_sink(sink, NULL);
    lj_library_delete(TMP, "Lib");
    if (lj_library_create(TMP, "Lib", &lib) != 0) { printf("FAIL cannot create " TMP "User/Lib\n"); return 1; }
    snprintf(c->userdir, sizeof c->userdir, TMP);
    c->lib = &lib; c->show = lj_show_new(); c->main_ini = lj_ini_new(); c->forms_ini = lj_ini_new();
    c->show->current_seq = lj_sequence_new_rec(); c->current_scene = 0;
    lj_sysprefs_defaults(&prefs); c->prefs = &prefs;
    lj_hotkeys_init(&hk); c->hotkeys = &hk;
    lj_midi_init(&midi); c->midi = &midi;
    lj_win_set_host(NULL);                                                         /* binds every family's lj_fn rows, ours included */
    rebind_programs();

    test_open_all();
    test_2532(&lib);
    test_midi_script();
    test_learn();
    test_fingers(&lib);
    test_keyboard();
    test_goldens();

    lj_win_close_all();
    lj_warnings_close_all();
    lj_fn_unbind_all();
    lj_ini_free(c->main_ini); lj_ini_free(c->forms_ini); c->main_ini = c->forms_ini = NULL;
    lj_show_free(c->show); c->show = NULL;
    c->lib = NULL; c->prefs = NULL; c->hotkeys = NULL; c->midi = NULL;
    lj_log_set_sink(NULL, NULL);
    printf("da2532_fingers: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
