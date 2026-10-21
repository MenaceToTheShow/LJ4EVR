/* test_windows_b.c - the runnable check for src/windows/lj_cue_form.c and lj_master_intensity.c
 * (W1 windows-b): column widths from forms.ini [CUE] defaults, the row colour table, Add Sequence
 * to Current Cue picking slot 1 then 2, the caption rules (F19, LORD_FIDELITY-13), the grid row
 * text "0005 | name | | 1:2 | 4.00 | 3.00 | | fwd | auto", Go / enable button clicks posting
 * LJ_CMD_GO / LJ_CMD_ENABLE, the slot popup, the Grand Master fader at 50 % posting
 * LJ_CMD_SET_MASTER 128 (LJ_GM_DOMAIN), the sub-master mode popup items verbatim, and the
 * Save Cue flow when a library can be created (skipped while the model bodies are stubs). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_locale.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "engine/lj_engine.h"
#include "model/lj_cue.h"
#include "model/lj_library.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/lj_window.h"
#include "ui/widgets/lj_w_fader.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_master_intensity.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void mouse(lj_window *win, int type, int x, int y, int button)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.x = x; ev.u.mouse.y = y; ev.u.mouse.button = button; ev.u.mouse.clicks = 1;
    lj_window_event(win, &ev);
}
/* click a widget at a widget-relative point: lj_widget_abs already includes the client origin (the root's x,y) */
static void click_at(lj_window *win, lj_widget *w, int rx, int ry, int button)
{
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    mouse(win, LJ_EV_MOUSE_DOWN, ax + rx, ay + ry, button);
    mouse(win, LJ_EV_MOUSE_UP, ax + rx, ay + ry, button);
}
static void click_root(lj_window *win, int x, int y, int button) { click_at(win, lj_window_root(win), x, y, button); }

int lj_test_windows_b(int argc, char **argv);
int lj_test_windows_b(int argc, char **argv)
{
    static const int dflt[9] = { 30, 160, 115, 45, 38, 38, 45, 45, 40 };
    lj_ctx *ctx = lj_ctx_get();
    lj_engine_status view;
    lj_sequence *seq;
    lj_cue *cue;
    lj_window *win;
    lj_widget *w;
    lj_cmd cmd;
    const int *cw;
    const char *const *items;
    char cap[128], cell[40], want[40];
    int i, n, ax, ay;
    (void)argc; (void)argv;

    memset(&view, 0, sizeof view);
    ctx->show = lj_show_new();
    ctx->show->current_cue = lj_cue_new_rec();
    ctx->engine = lj_engine_create();
    ctx->forms_ini = lj_ini_new();
    ctx->main_ini = lj_ini_new();
    cue = ctx->show->current_cue;
    seq = lj_sequence_new_rec();
    seq->number = 5; lj_str_copy(seq->name, sizeof seq->name, "name");
    free(seq->scenes); seq->scenes = (lj_scene *)calloc(2, sizeof *seq->scenes); seq->n = 2;
    CHECK(seq->scenes != NULL);
    if (!seq->scenes) return 1;
    seq->scenes[0].scene_ms = 4000; seq->scenes[0].fade_ms = 3000;
    lj_table_put(&ctx->show->sequences, 5, seq);
    lj_cue_form_set_view(&view);
    lj_master_intensity_set_view(&view);

    /* column widths: forms.ini [CUE] C0..C8 defaults, then an override */
    cw = lj_cue_form_col_widths();
    CHECK(memcmp(cw, dflt, sizeof dflt) == 0);
    lj_ini_seti(ctx->forms_ini, "CUE", "C1", 200);
    CHECK(lj_cue_form_col_widths()[1] == 200);
    lj_ini_seti(ctx->forms_ini, "CUE", "C1", 160);

    /* row colours per state [07 s3.5] */
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_RUNNING, 2, 0, 0, 1) == 0x000000u);
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_RUNNING, 1, 0, 0, 1) == 0x800000u);      /* static = maroon */
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_STOPPED, 2, 0, 0, 1) == 0x800000u);
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_RUNNING, 2, 1, 0, 1) == 0xFF0000u);      /* manual in chase = red */
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_LOAD_ERROR, 2, 0, 0, 1) == 0xFF0000u);
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_DISABLED, 2, 0, 0, 1) == 0x808080u);
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_EDIT, 2, 0, 0, 1) == 0x0000FFu);
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_EDIT, 2, 0, 1, 1) == 0x008000u);         /* link-cue in edit = green */
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_STOPPED, 2, 0, 0, 0) == 0x000000u);      /* Use Status Colors off */
    CHECK(lj_cue_form_row_rgb(LJ_SLOT_EMPTY, 0, 0, 0, 1) == 0x808080u);

    /* Add Sequence to Current Cue: slot 1, then 2; full = -1 */
    ctx->modified[LJ_MOD_CUE] = 0;
    CHECK(lj_cue_form_add_sequence(5) == 1);
    CHECK(cue->slot[0].seq == 5 && cue->slot[0].enabled == 1 && cue->slot[0].trig == LJ_TRIG_AUTO && cue->slot[0].dir == LJ_DIR_FWD && cue->slot[0].time_pct == 0);
    CHECK(ctx->modified[LJ_MOD_CUE] == 1);
    CHECK(lj_cue_form_add_sequence(7) == 2);
    for (i = 3; i <= 12; i++) CHECK(lj_cue_form_add_sequence(7) == i);
    CHECK(lj_cue_form_add_sequence(7) == -1);
    for (i = 1; i < LJ_CUE_SLOTS; i++) memset(&cue->slot[i], 0, sizeof cue->slot[i]);

    /* caption rules */
    CHECK(lj_cue_form_caption(cap, sizeof cap) == 0 && strcmp(cap, "CUE [0000 No Name]") == 0);
    cue->number = 1; lj_str_copy(cue->name, sizeof cue->name, "name");
    CHECK(lj_cue_form_caption(cap, sizeof cap) == 0 && strcmp(cap, "CUE [0001 name]") == 0);
    view.cue_edit = 1;
    CHECK(lj_cue_form_caption(cap, sizeof cap) == 0 && strcmp(cap, "EDIT CUE [0001 name]") == 0);
    view.cue_edit = 0;
    view.slot_status[3] = LJ_SLOT_EDIT;
    CHECK(lj_cue_form_caption(cap, sizeof cap) == 0 && (strcmp(cap, "EDIT CUE [0001 name]") == 0) == LJ_CUE_EDIT_PREFIX_RULE);
    view.slot_status[3] = LJ_SLOT_EMPTY;

    /* the Cue control: client 604x254 (acceptance 10), the grid row of acceptance 5 */
    view.slot_status[0] = LJ_SLOT_RUNNING; view.slot_scene[0] = 1; view.slot_scenes[0] = 2;
    win = lj_cue_form_open();
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_window_root(win)->w == 604 && lj_window_root(win)->h == 254);
    CHECK(strcmp(lj_window_caption(win)->text, "CUE [0001 name]") == 0);
    lj_cue_form_refresh();
    CHECK(lj_cue_form_cell(0, 0, cell, sizeof cell) == 0 && strcmp(cell, "0005") == 0);
    CHECK(lj_cue_form_cell(0, 1, cell, sizeof cell) == 0 && strcmp(cell, "name") == 0);
    CHECK(lj_cue_form_cell(0, 2, cell, sizeof cell) == 0 && cell[0] == 0);
    CHECK(lj_cue_form_cell(0, 3, cell, sizeof cell) == 0 && strcmp(cell, "1:2") == 0);
    snprintf(want, sizeof want, "4%c00", lj_locale_decimal());
    CHECK(lj_cue_form_cell(0, 4, cell, sizeof cell) == 0 && strcmp(cell, want) == 0);
    snprintf(want, sizeof want, "3%c00", lj_locale_decimal());
    CHECK(lj_cue_form_cell(0, 5, cell, sizeof cell) == 0 && strcmp(cell, want) == 0);
    CHECK(lj_cue_form_cell(0, 6, cell, sizeof cell) == 0 && cell[0] == 0);
    CHECK(lj_cue_form_cell(0, 7, cell, sizeof cell) == 0 && strcmp(cell, "fwd") == 0);
    CHECK(lj_cue_form_cell(0, 8, cell, sizeof cell) == 0 && strcmp(cell, "auto") == 0);
    CHECK(lj_cue_form_cell(1, 0, cell, sizeof cell) == 0 && cell[0] == 0);     /* empty slot */
    CHECK(lj_cue_form_cell(12, 0, cell, sizeof cell) == -1);
    lj_window_paint(win);
    {
        lj_rect cr;
        const lj_surface *sf = lj_window_surface(win);
        lj_window_client_rect(win, &cr);
        CHECK(sf->px[(cr.y + 17 + 4) * sf->w + cr.x + 4] == 0x00FF00u);        /* slot 1 enable button: green raised */
        CHECK(sf->px[(cr.y + 8) * sf->w + cr.x + 182] == 0xC0C0C0u);           /* header face, inside the Sequence section */
    }

    /* Go with nothing selected = every slot */
    w = lj_window_find(win, "sbGo");
    CHECK(w != NULL);
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_GO && cmd.a == LJ_RUN_ALL);
    w = lj_window_find(win, "sbStop");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_STOP && cmd.a == LJ_RUN_ALL);

    /* the numbered enable button: red depressed = disabled, posts LJ_CMD_ENABLE */
    ctx->modified[LJ_MOD_CUE] = 0;
    click_root(win, 10, 17 + 4, 1);
    CHECK(cue->slot[0].enabled == 0 && lj_cue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_ENABLE && cmd.a == LJ_RUN_SLOT0 && cmd.b == 0);
    CHECK(ctx->modified[LJ_MOD_CUE] == 1);
    lj_window_paint(win);
    {
        lj_rect cr;
        const lj_surface *sf = lj_window_surface(win);
        lj_window_client_rect(win, &cr);
        CHECK(sf->px[(cr.y + 17 + 4) * sf->w + cr.x + 4] == 0xFF0000u);
    }
    click_root(win, 10, 17 + 4, 1);
    CHECK(cue->slot[0].enabled == 1 && cmd.kind == LJ_CMD_ENABLE);

    /* right click on row 1: selected (navy) + the slot popup; "Manual Trig" is item 1 */
    mouse(win, LJ_EV_MOUSE_DOWN, 0, 0, 0);   /* nothing: keeps the capture state sane */
    {
        lj_rect cr;
        lj_window_client_rect(win, &cr);
        mouse(win, LJ_EV_MOUSE_DOWN, cr.x + 100, cr.y + 17 + 4, 2);
        mouse(win, LJ_EV_MOUSE_UP, cr.x + 100, cr.y + 17 + 4, 2);
        CHECK(lj_cue_form_selected(0) && !lj_cue_form_selected(1));
        CHECK(lj_cue_popup_is_open(win));
        mouse(win, LJ_EV_MOUSE_DOWN, cr.x + 105, cr.y + 2 + 17 + 8, 1);      /* the popup is clamped to y = 0: item 1 */
        mouse(win, LJ_EV_MOUSE_UP, cr.x + 105, cr.y + 2 + 17 + 8, 1);
        CHECK(!lj_cue_popup_is_open(win));
        CHECK(cue->slot[0].trig == LJ_TRIG_MANUAL);
        CHECK(lj_cue_form_cell(0, 8, cell, sizeof cell) == 0 && strcmp(cell, "manual") == 0);
    }
    /* Select All: with a slot highlighted it de-selects all, then selects all [03 s5.4] */
    w = lj_window_find(win, "sbSelect");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(!lj_cue_form_selected(0) && w && strcmp(lj_w_caption(w), "Select All") == 0);
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_cue_form_selected(0) && lj_cue_form_selected(11) && w && strcmp(lj_w_caption(w), "Deselect All") == 0);
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(!lj_cue_form_selected(0));

    /* Save Cue: the dialog shows the number only once assigned; the file flow needs the model bodies */
    {
        lj_window *sw = lj_save_cue_open();
        CHECK(sw != NULL);
        if (sw) {
            w = lj_window_find(sw, "CueNo");
            CHECK(w && strcmp(lj_w_edit_text(w), "0001") == 0);
            w = lj_window_find(sw, "Bsave");
            CHECK(w && w->enabled);
            lj_save_cue_close();
        }
        cue->number = 0;
        sw = lj_save_cue_open();
        w = sw ? lj_window_find(sw, "CueNo") : NULL;
        CHECK(w && lj_w_edit_text(w)[0] == 0);
        w = sw ? lj_window_find(sw, "Bsave") : NULL;
        CHECK(w && !w->enabled);
        lj_save_cue_close();
        CHECK(lj_cue_form_save(1, 0) == -1);                                  /* no library: refused, nothing cleared */
        {
            static lj_library lib;
            char dir[LJ_PATH_CAP];
            const char *tmp = getenv("TEMP");
            snprintf(dir, sizeof dir, "%s/lj4evr-test-windows-b/", tmp ? tmp : ".");
            lj_library_delete(dir, "T");                                          /* a previous run's files would shift the numbering */
            if (lj_library_create(dir, "T", &lib) == 0) {
                ctx->lib = &lib;
                ctx->modified[LJ_MOD_CUE] = 1;
                CHECK(lj_cue_form_save(1, 0) == 0);
                CHECK(cue->number == 1 && ctx->modified[LJ_MOD_CUE] == 0 && ctx->cue_saved == 1);
                CHECK(lj_table_get(&ctx->show->cues, 1) != NULL && lj_table_get(&ctx->show->cues, 1) != cue);
                CHECK(lj_cue_form_caption(cap, sizeof cap) == 0 && strncmp(cap, "CUE [0001 ", 10) == 0);
                CHECK(lj_cue_form_save(2, 7) == 0 && cue->number == 7);          /* Save as Cue # */
                ctx->lib = NULL;
                lj_library_delete(dir, "T");
            } else printf("test_windows_b: SKIP save flow (lj_library_create failed: model bodies not landed)\n");
        }
    }
    lj_cue_form_close();
    CHECK(lj_cue_form_get() == NULL);

    /* Master Intensity: client 664x297; the GM fader at 50 % posts SET_MASTER 128 */
    view.gm = 255; memset(view.sub, 255, sizeof view.sub);
    win = lj_master_intensity_open();
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_window_root(win)->w == 664 && lj_window_root(win)->h == 297);
    w = lj_master_intensity_fader(0);
    CHECK(w != NULL && lj_w_fader_value(w) == 255);
    if (w) {
        int th = w->h - LJ_FADER_CHROME;                                       /* 103: the half-way click lands on 128 exactly */
        click_at(win, w, LJ_FADER_W / 2, LJ_FADER_TRACK_Y + (th - 1) / 2, 1);
        CHECK(lj_w_fader_value(w) == 128);
    }
    CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_SET_MASTER && cmd.a == 0 && cmd.b == 128);
    CHECK(lj_master_intensity_level(0) == 128);
    lj_master_intensity_set(3, 64);
    CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_SET_MASTER && cmd.a == 3 && cmd.b == 64 && cmd.c == LJ_MASTER_PROPORTIONAL);

    /* the sub-master mode popup, verbatim exe strings; picking "Master LoTP" on sub 1 */
    items = lj_master_intensity_mode_items(&n);
    CHECK(n == 4 && strcmp(items[0], "Master Proportional") == 0 && strcmp(items[1], "Master LoTP") == 0 && strcmp(items[2], "Master HTP") == 0 && strcmp(items[3], "Master no Effect") == 0);
    w = lj_window_find(win, "Panel1");
    CHECK(w != NULL);
    {
        lj_widget *m = NULL, *c;
        for (c = w ? w->first : NULL; c; c = c->next) if (c->y == 192 && c->x == 46) m = c;   /* sub 1's mode label */
        CHECK(m != NULL);
        if (m) {
            lj_rect cr;
            lj_window_client_rect(win, &cr);
            lj_widget_abs(m, &ax, &ay);
            click_at(win, m, 3, 3, 1);
            CHECK(lj_cue_popup_is_open(win));
            mouse(win, LJ_EV_MOUSE_DOWN, ax + 5, ay + m->h + 2 + 17 + 8, 1);
            mouse(win, LJ_EV_MOUSE_UP, ax + 5, ay + m->h + 2 + 17 + 8, 1);
            CHECK(!lj_cue_popup_is_open(win));
            CHECK(ctx->show->submaster_mode[0] == LJ_MASTER_LOTP);
            CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_SET_MASTER && cmd.a == 1 && cmd.c == LJ_MASTER_LOTP);
            CHECK(strcmp(lj_w_caption(m), "LoTP") == 0);
        }
    }
    /* BlackOut latches and posts; Restore releases */
    w = lj_window_find(win, "bBlackOut");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_BLACKOUT && cmd.b == 1);
    w = lj_window_find(win, "bRestore");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_BLACKOUT && cmd.b == 0);
    w = lj_window_find(win, "bFadeOut");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_master_intensity_last_cmd(&cmd) && cmd.kind == LJ_CMD_GM_FADE && cmd.a == 0 && cmd.b == 2000);   /* Main.Ini Fadetime=3 -> "2.0 Sec" */
    lj_window_paint(win);
    lj_master_intensity_close();
    CHECK(lj_master_intensity_get() == NULL);

    lj_engine_destroy(ctx->engine); ctx->engine = NULL;
    lj_show_free(ctx->show); ctx->show = NULL;
    lj_ini_free(ctx->forms_ini); lj_ini_free(ctx->main_ini); ctx->forms_ini = ctx->main_ini = NULL;
    printf("test_windows_b: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
