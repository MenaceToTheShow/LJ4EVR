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
/* W2 windows-b (lj_test_windows_b2 at the end of the file) */
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "model/lj_bgcue.h"
#include "model/lj_statics.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_bgcue_form.h"
#include "windows/lj_smoke_control.h"
#include "windows/lj_statics2_form.h"

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
    char cap[128], cell[40];
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
    CHECK(lj_cue_form_cell(0, 4, cell, sizeof cell) == 0 && strcmp(cell, "4:00") == 0);   /* ME-75: lj_fmt_time's colon, as cuecontrol.gif reads */
    CHECK(lj_cue_form_cell(0, 5, cell, sizeof cell) == 0 && strcmp(cell, "3:00") == 0);
    CHECK(lj_cue_form_cell(0, 6, cell, sizeof cell) == 0 && cell[0] == 0);
    CHECK(lj_cue_form_cell(0, 7, cell, sizeof cell) == 0 && strcmp(cell, "Fwd") == 0);   /* ME-75: display words capitalised like the GIF */
    CHECK(lj_cue_form_cell(0, 8, cell, sizeof cell) == 0 && strcmp(cell, "Auto") == 0);
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
        CHECK(lj_cue_form_cell(0, 8, cell, sizeof cell) == 0 && strcmp(cell, "Manual") == 0);
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

/* ==== W2 windows-b: Background Cue, Statics 2, Smoke Control (lj_test_windows_b2) ====
 * The Statics 2 slot popup equals note 14 sB.3 verbatim (Martin's duplicated "Replace Page" / "Replace Slot", D13,
 * acceptance 4) and the second pair is bound to rpPage / rpSlot as the DFM binds it; activating slot 3 posts
 * LJ_CMD_STATIC_ACTIVATE with the global index of page 1 / slot 3, its level and scope and the pinned slot; the slot LED and
 * the toolbar Statics LED follow a synthetic engine view; Release Page / All; Static 03 hotkey. Background Cue: the LV1 rows,
 * the slot buttons posting LJ_CMD_BG_ON, the drop-down and New/Clear through LJ_ACT_LOAD_BGCUE. Smoke Control: each fader
 * posts its LJ_CMD_SMOKE (the Cycle fader at the period of its step), Timer, the Smoke button held and in toggle mode.
 * Goldens: tests/ui/golden/WIN_*.bmp, the three windows as opened over this fixture (LJ_GOLDEN_UPDATE=1 rewrites them). */
#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int lj_b2_act_arg = -99;
static void lj_b2_on_action(enum lj_desktop_action a, int arg, void *ud) { (void)a; (void)ud; lj_b2_act_arg = arg; }

static int lj_b2_fire(lj_window *win, const char *item)   /* runs a menu item's bound handler as lj_menu's fire() does */
{
    lj_menu *m = lj_window_menus(win);
    lj_menu_rt *rt = lj_menu_rt_named(m, item);
    if (!rt || !rt->fn) return -1;
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    return 0;
}

static int lj_b2_items_equal(const lj_menu_item_desc *items, int n, const char *const *want, int nwant)
{
    int i;
    if (n != nwant) { printf("  menu: %d items, want %d\n", n, nwant); return 0; }
    for (i = 0; i < n; i++) if (strcmp(items[i].caption, want[i]) != 0) { printf("  menu item %d: \"%s\", want \"%s\"\n", i, items[i].caption, want[i]); return 0; }
    return 1;
}

static int lj_b2_same(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    do { ca = fgetc(fa); cb = fgetc(fb); if (ca != cb) { rc = 0; break; } } while (ca != EOF);
    fclose(fa); fclose(fb);
    return rc;
}

static void lj_b2_golden(lj_window *win, const char *name)
{
    char golden[1024], tmp[1024];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    int rc;
    snprintf(golden, sizeof golden, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name);
    snprintf(tmp, sizeof tmp, "lj_test_windows_b2_%s.bmp", name);
    lj_window_paint(win);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(win), golden) == 0); printf("  %s: written\n", golden); return; }
    CHECK(lj_bmp_write(lj_window_surface(win), tmp) == 0);
    rc = lj_b2_same(tmp, golden);
    if (rc != 1) { fails++; printf("FAIL golden %s: %s (the render is %s)\n", name, rc ? "missing or unreadable" : "differs", tmp); return; }
    printf("  %s: identical\n", name);
    remove(tmp);
}

static uint32_t lj_b2_px(lj_window *win, lj_widget *w, int rx, int ry)
{
    const lj_surface *sf = lj_window_surface(win);
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    return sf->px[(ay + ry) * sf->w + ax + rx];
}

int lj_test_windows_b2(int argc, char **argv);
int lj_test_windows_b2(int argc, char **argv)
{
    static const char *const top[] = { "Record Current Scene", "Link to current Sequence/Scene", "Clear Slot", "-", "Edit Scene",
                                       "Convert to current scene", "-", "Activation Mode", "Replace Mode", "-", "Properties.." };   /* 14 sB.3 items 1-11 */
    static const char *const act[] = { "Toggle", "Flash" };
    static const char *const rep[] = { "Add", "-", "Replace Group", "Replace Page", "Replace Slot", "Replace All", "-",
                                       "Solo Group", "Replace Page", "Replace Slot", "Solo All" };   /* D13: the DFM's ReplacePage2 / ReplaceSlot2 */
    lj_ctx *ctx = lj_ctx_get();
    lj_static_slot *st;
    lj_sequence *seq;
    lj_bgcue *bg;
    lj_window *win;
    lj_widget *w;
    lj_menu *m;
    lj_cmd cmd;
    char row[96];
    int i;
    (void)argc; (void)argv;
    fails = 0;

    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->show = lj_show_new();
    ctx->engine = lj_engine_create();
    ctx->main_ini = lj_ini_new();
    ctx->forms_ini = lj_ini_new();
    ctx->static_visible = 0;
    CHECK(ctx->show && ctx->engine);
    if (!ctx->show || !ctx->engine) return 1;

    /* ---- Statics 2: page 1, slot 3 "Front Strobes" recorded, 80 %, Replace Page ---- */
    CHECK(lj_statics_add_page(&ctx->show->pages, &ctx->show->npages, "Page 1") == 0);
    if (ctx->show->npages < 1) return 1;
    for (i = 0; i < LJ_STATICS2_SLOTS_PER_PAGE; i++) ctx->show->pages[0].slot[i].level_pct = 100;   /* Default Levels at 100 % */
    st = &ctx->show->pages[0].slot[2];
    lj_str_copy(st->name, sizeof st->name, "Front Strobes");
    st->kind = LJ_STATIC_RECORDED; st->level_pct = 80; st->replace = LJ_REP_PAGE; st->activation = LJ_ACT_TOGGLE;
    st->scene = (lj_scene *)calloc(1, sizeof *st->scene);
    CHECK(st->scene != NULL);
    win = lj_statics2_form_open();
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_statics2_visible() == 20);
    CHECK(lj_window_root(win)->w == 502 && lj_window_root(win)->h == 58 + 20 * LJ_S2_ROW_H + 30);
    CHECK(strcmp(lj_window_caption(win)->text, "Statics") == 0);
    CHECK(lj_statics2_slot_button(20) != NULL && lj_statics2_slot_button(21) == NULL);

    /* the slot popup, verbatim (acceptance 4) */
    m = lj_window_menus(win);
    click_at(win, lj_statics2_slot_button(3), 5, 5, 2);                                     /* right click on the slot */
    CHECK(lj_statics2_popup_slot() == 3);
    CHECK(m->depth == 1 && m->lv[0].items == lj_form_find_menu(&lj_form_TSTATICS2FORM, "popStaticEntry")->items);
    CHECK(!lj_statics2_last_cmd(&cmd));                                                     /* a right click activates nothing */
    if (m->depth == 1) {
        const lj_menu_item_desc *items = m->lv[0].items;
        CHECK(lj_b2_items_equal(items, m->lv[0].n, top, (int)LJ_COUNT_OF(top)));
        if (m->lv[0].n == (int)LJ_COUNT_OF(top)) {
            CHECK(lj_b2_items_equal(items[7].children, items[7].nchildren, act, (int)LJ_COUNT_OF(act)));
            CHECK(lj_b2_items_equal(items[8].children, items[8].nchildren, rep, (int)LJ_COUNT_OF(rep)));
            CHECK(items[4].enabled == LJ_NO);                                            /* Edit Scene greyed in the DFM */
        }
    }
    lj_menu_close(m);
    /* the duplicated items act as the DFM binds them: the second "Replace Slot" sets Replace Slot, not Solo Slot */
    CHECK(lj_b2_fire(win, "ReplaceSlot2") == 0 && st->replace == LJ_REP_SLOT);
    CHECK(lj_b2_fire(win, "ReplacePage2") == 0 && st->replace == LJ_REP_PAGE);
    CHECK(lj_b2_fire(win, "ReplaceSoloGroup1") == 0 && st->replace == LJ_REP_SOLO_GROUP);
    CHECK(lj_b2_fire(win, "Flash1") == 0 && st->activation == LJ_ACT_FLASH);
    CHECK(lj_b2_fire(win, "Toggle1") == 0 && st->activation == LJ_ACT_TOGGLE);
    CHECK(lj_b2_fire(win, "ReplacePage1") == 0 && st->replace == LJ_REP_PAGE);
    lj_statics2_popup(3, 40, 40);
    CHECK(lj_menu_rt_named(m, "ReplacePage1")->checked && lj_menu_rt_named(m, "ReplacePage2")->checked && !lj_menu_rt_named(m, "ReplaceSlot2")->checked);
    lj_menu_close(m);

    /* activating slot 3: one click on its button */
    w = lj_statics2_slot_button(3);
    CHECK(w != NULL);
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_ACTIVATE);
    CHECK(cmd.a == (uint32_t)lj_static_global_index(1, 3) && cmd.a == 2 && cmd.b == 80 && cmd.c == LJ_REP_PAGE && cmd.st == st);
    CHECK(lj_statics2_press(4, 1) == -1);                                                   /* an empty slot posts nothing */
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_ACTIVATE);

    /* the engine view reports runner 5 holding page 1 / slot 3: the slot LED and the toolbar LED go green */
    ctx->view.tick_no = 1;
    ctx->view.static_slot_of[5] = 2; ctx->view.static_active[5] = 1; ctx->view.static_level[5] = 80; ctx->view.statics_any = 1;
    lj_statics2_form_refresh();
    CHECK(lj_statics2_slot_active(3) && !lj_statics2_slot_active(2));
    CHECK(lj_desktop_led_get(LJ_LED_STATICS) == 1);
    lj_window_paint(win);
    if (w) {
        CHECK(lj_b2_px(win, w, w->w - 6, w->h / 2) == 0x00FF00u);
        CHECK(lj_b2_px(win, lj_statics2_slot_button(2), lj_statics2_slot_button(2)->w - 6, w->h / 2) == 0xC0C0C0u);
    }
    lj_b2_golden(win, "WIN_STATICS2_FORM");
    /* a level change on the active slot re-posts it without scope releases */
    CHECK(lj_statics2_set_level(3, 50) == 0 && st->level_pct == 50);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_ACTIVATE && cmd.b == 50 && cmd.c == LJ_REP_ADD);
    /* a second click releases */
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_DEACTIVATE && cmd.a == 2);
    ctx->view.static_active[5] = 0; ctx->view.statics_any = 0; ctx->view.tick_no = 2;
    lj_statics2_form_refresh();
    CHECK(!lj_statics2_slot_active(3) && lj_desktop_led_get(LJ_LED_STATICS) == 0 && w && w->color == 0xC0C0C0u);
    /* Release Page / Release All panels */
    w = lj_window_find(win, "pReleasePage");
    if (w) click_at(win, w, 10, 10, 1);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_DEACTIVATE && cmd.a == LJ_RUN_ALL && cmd.c == 1);
    w = lj_window_find(win, "pReleaseAll");
    if (w) click_at(win, w, 10, 10, 1);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_DEACTIVATE && cmd.a == LJ_RUN_ALL && cmd.c == 0);
    /* Flash: active while held */
    st->activation = LJ_ACT_FLASH;
    CHECK(lj_statics2_press(3, 1) == 0 && lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_ACTIVATE);
    CHECK(lj_statics2_press(3, 0) == 0 && lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_DEACTIVATE);
    st->activation = LJ_ACT_TOGGLE;
    /* the Static 03 hotkey = slot 3 of the current page */
    lj_statics2_form_bind_fns();
    lj_fn_run(LJ_FN_STATIC_3, 1);
    CHECK(lj_statics2_last_cmd(&cmd) && cmd.kind == LJ_CMD_STATIC_ACTIVATE && cmd.a == 2);
    lj_statics2_form_close();
    CHECK(lj_statics2_form_get() == NULL);

    /* ---- Background Cue: BG 7 "Wild Bar Lights", slot 1 = seq 4 "Lite on" running 1:1, slot 2 = seq 2 Audio, off ---- */
    seq = lj_sequence_new_rec();
    seq->number = 4; lj_str_copy(seq->name, sizeof seq->name, "Lite on");
    lj_table_put(&ctx->show->sequences, 4, seq);
    bg = lj_bgcue_new_rec();
    CHECK(bg != NULL);
    if (!bg) return 1;
    bg->number = 7; lj_str_copy(bg->name, sizeof bg->name, "Wild Bar Lights");
    bg->slot[0].seq = 4; bg->slot[0].enabled = 1;
    bg->slot[1].seq = 2; bg->slot[1].mode = LJ_BG_AUDIO;
    lj_table_put(&ctx->show->bgcues, 7, bg);
    ctx->view.bg_number = 7; ctx->view.bg_any = 1; ctx->view.tick_no = 3;
    ctx->view.bg_status[0] = LJ_SLOT_RUNNING; ctx->view.bg_scene[0] = 1; ctx->view.bg_scenes[0] = 1; ctx->view.bg_enabled[0] = 1;
    lj_desktop_bind_action(LJ_ACT_LOAD_BGCUE, lj_b2_on_action, NULL);
    {
        static lj_library lib;
        char dir[LJ_PATH_CAP], path[LJ_PATH_CAP];
        const char *tmp = getenv("TEMP");
        snprintf(dir, sizeof dir, "%s/lj4evr-test-windows-b2/", tmp ? tmp : ".");
        lj_library_delete(dir, "B2");
        if (lj_library_create(dir, "B2", &lib) == 0 && lj_library_record_path(&lib, LJ_REC_BG, 7, path, sizeof path) == 0 && lj_bgcue_save(path, bg) == 0) ctx->lib = &lib;
        else printf("test_windows_b2: SKIP the BG drop-down rows (library or BG file not written)\n");
        win = lj_bgcue_form_open();
        CHECK(win != NULL);
        if (!win) return 1;
        CHECK(lj_window_root(win)->w == 384 && lj_window_root(win)->h == 124);
        CHECK(lj_bgcue_form_row(1, row, sizeof row) == 0 && strcmp(row, "0004\tLite on\t1:1\t") == 0);
        CHECK(lj_bgcue_form_row(2, row, sizeof row) == 0 && strcmp(row, "0002\t\t\tA") == 0);   /* seq 2 not loaded: no name; idle: no scene */
        CHECK(lj_bgcue_form_row(3, row, sizeof row) == 0 && row[0] == 0);
        CHECK(lj_bgcue_form_row(6, row, sizeof row) == -1);
        CHECK(lj_desktop_led_get(LJ_LED_BGCUE) == 1);
        CHECK(lj_bgcue_form_slot_button(1)->color == 0x00FF00u && lj_bgcue_form_slot_button(2)->color == 0xFF0000u);
        lj_b2_golden(win, "WIN_BGCUE_FORM");
        click_at(win, lj_bgcue_form_slot_button(1), 5, 5, 1);
        CHECK(lj_bgcue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_BG_ON && cmd.a == LJ_RUN_BG0 && cmd.b == 0);
        click_at(win, lj_bgcue_form_slot_button(2), 5, 5, 1);
        CHECK(lj_bgcue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_BG_ON && cmd.a == LJ_RUN_BG0 + 1 && cmd.b == 1);
        w = lj_window_find(win, "sbNew");
        if (w) click_at(win, w, 5, 5, 1);
        CHECK(lj_b2_act_arg == 0);                                                          /* New/Clear = LJ_ACT_LOAD_BGCUE 0 */
        w = lj_window_find(win, "cb1");
        if (w && ctx->lib) {
            lj_w_combobox *cb = (lj_w_combobox *)w;
            CHECK(cb->n == 1 && strcmp(cb->items[0], "07 Wild Bar Lights") == 0 && cb->index == 0);
            lj_w_emit(w, LJ_WEV_CHANGE);
            CHECK(lj_b2_act_arg == 7);                                                      /* the app pins and posts LOAD_BG */
        }
        lj_bgcue_form_bind_fns();
        lj_fn_run(LJ_FN_BGCUE_ALL_SLOTS_OFF, 1);
        CHECK(lj_bgcue_form_last_cmd(&cmd) && cmd.kind == LJ_CMD_BG_ON && cmd.a == LJ_RUN_ALL && cmd.b == 0);
        lj_bgcue_form_close();
        ctx->lib = NULL;
        lj_library_delete(dir, "B2");
    }

    /* ---- Smoke Control ---- */
    CHECK(lj_smoke_cycle_ms(0) == LJ_SMOKE_CYCLE_MIN_MS && lj_smoke_cycle_ms(LJ_SMOKE_CYCLE_STEPS - 1) == LJ_SMOKE_CYCLE_MAX_MS);
    for (i = 1; i < LJ_SMOKE_CYCLE_STEPS; i++) CHECK(lj_smoke_cycle_ms(i) > lj_smoke_cycle_ms(i - 1));
    lj_ini_seti(ctx->main_ini, "SMOKE", "Level", 255);
    lj_ini_seti(ctx->main_ini, "SMOKE", "Duration", 10);
    lj_ini_seti(ctx->main_ini, "SMOKE", "HazerLevel", 41);
    lj_ini_seti(ctx->main_ini, "SMOKE", "HazerFan", 242);
    win = lj_smoke_control_open();
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(lj_window_root(win)->w == 310 && lj_window_root(win)->h == 213);
    CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.kind == LJ_CMD_SMOKE && cmd.a == LJ_SMOKE_FAN_LEVEL && cmd.b == 242);   /* open sends the saved settings */
    w = lj_smoke_control_fader(LJ_SMOKE_F_CYCLE);
    CHECK(w != NULL && w->x == 153 && w->y == 33 && w->h == 160);                            /* over Shape3 */
    if (w) {
        lj_w_fader_set_value(w, 9);
        lj_w_emit(w, LJ_WEV_CHANGE);
        CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.kind == LJ_CMD_SMOKE && cmd.a == LJ_SMOKE_CYCLE_MS && cmd.b == 34000u);   /* step 9 = 34 s */
        CHECK(lj_ini_geti(ctx->main_ini, "SMOKE", "Cycle", -1) == 9);
    }
    w = lj_smoke_control_fader(LJ_SMOKE_F_DURATION);
    if (w) { lj_w_fader_set_value(w, 12); lj_w_emit(w, LJ_WEV_CHANGE); }
    CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_DURATION_MS && cmd.b == 12000u);
    w = lj_smoke_control_fader(LJ_SMOKE_F_HAZE);
    if (w) { lj_w_fader_set_value(w, 100); lj_w_emit(w, LJ_WEV_CHANGE); }
    CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_HAZE_LEVEL && cmd.b == 100);
    w = lj_window_find(win, "dSmokeTimer");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.kind == LJ_CMD_SMOKE && cmd.a == LJ_SMOKE_TIMER && cmd.b == 1 && w && lj_w_speedbutton_down(w));
    lj_b2_golden(win, "WIN_SMOKE_CONTROL");
    if (w) click_at(win, w, 5, 5, 1);
    CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_TIMER && cmd.b == 0);
    w = lj_window_find(win, "dSmoke");
    if (w) {
        lj_window *sw = win;
        int ax, ay;
        lj_widget_abs(w, &ax, &ay);
        mouse(sw, LJ_EV_MOUSE_DOWN, ax + 5, ay + 5, 1);
        CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_BUTTON && cmd.b == 1 && cmd.c == 0 && lj_desktop_led_get(LJ_LED_SMOKE) == 1);
        mouse(sw, LJ_EV_MOUSE_UP, ax + 5, ay + 5, 1);
        CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_BUTTON && cmd.b == 0);
        lj_ini_seti(ctx->main_ini, "SMOKE", "Toggle", 4);                                     /* "Smoke button toggle 4-5 sec" */
        click_at(win, w, 5, 5, 1);
        CHECK(lj_smoke_control_last_cmd(&cmd) && cmd.a == LJ_SMOKE_BUTTON && cmd.b == 1 && cmd.c == 4000u);   /* release posts nothing */
    }
    lj_smoke_control_close();
    CHECK(lj_smoke_control_get() == NULL);

    lj_engine_destroy(ctx->engine); ctx->engine = NULL;
    lj_show_free(ctx->show); ctx->show = NULL;
    lj_ini_free(ctx->forms_ini); lj_ini_free(ctx->main_ini); ctx->forms_ini = ctx->main_ini = NULL;
    memset(&ctx->view, 0, sizeof ctx->view);
    printf("test_windows_b2: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
