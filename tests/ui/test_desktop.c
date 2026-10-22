/* test_desktop.c - the runnable check for src/desktop (W1 desktop track):
 *  1. toolbar geometry constants vs note 07 s4 (30-px rows, 25x25 buttons, 17-px status bar, three rows + bar on the client);
 *  2. the ME-29 menu table: every top-level item of F5 has a rule, every leaf of the generated TLJMainForm is bound
 *     (lj_window_create logs nothing), the inert leaves are greyed, Offlinebar1 is gone;
 *  3. master selection rules (inclusive / exclusive / shift / ctrl / band / next / prev) on a synthetic 5-fixture desktop;
 *  4. Fixture toolbar enable set for a MAC 500-like profile vs a dimmer-like profile (F9, acceptance 3);
 *  5. ink inside a glyph button (W0-glyphs) and the "Please Wait.." -> library caption (LORD_FIDELITY-20, ME-37);
 *  6. the MAINWINDOW golden (tests/ui/golden/MAINWINDOW.bmp, REVIEWED.tsv) byte for byte, like ui_golden.
 * Usage: lj4evr_tests test_desktop [--write-golden] */
#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_desktop_tabs.h"
#include "desktop/lj_fixture_icons.h"
#include "desktop/lj_main_window.h"
#include "desktop/lj_selection.h"
#include "desktop/lj_statusbar.h"
#include "desktop/lj_toolbars.h"
#include "fixlib/lj_profile.h"
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails, logged;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
static void sink(void *ud, const char *line) { (void)ud; logged++; printf("  log: %s\n", line); }

/* synthetic profiles: a moving head with every control window, a dimmer with Levels only (lj_fixlib_by_id is the fixlib stub in W1: NULL) */
static const lj_ctl_window lj_mh_windows[] = {
    { LJ_CTL_INTENSITY, "Intensity", NULL, NULL, 0, NULL, 0 }, { LJ_CTL_MOVEMENT, "Movement", NULL, NULL, 0, NULL, 0 }, { LJ_CTL_COLORS, "Colors", NULL, NULL, 0, NULL, 0 },
    { LJ_CTL_GOBOS, "Gobos", NULL, NULL, 0, NULL, 0 }, { LJ_CTL_BEAM, "Beam", NULL, NULL, 0, NULL, 0 }, { LJ_CTL_EFFECTS, "Effects", NULL, NULL, 0, NULL, 0 },
    { LJ_CTL_EXTENDED, "Extended", NULL, NULL, 0, NULL, 0 }, { LJ_CTL_LAMP, "Lamp Control", NULL, NULL, 0, NULL, 0 } };
static const lj_ctl_window lj_dim_windows[] = { { LJ_CTL_LEVELS, "Levels", NULL, NULL, 0, NULL, 0 } };

static int count_rgb(const lj_surface *s, int x, int y, int w, int h, uint32_t rgb)
{
    int i, j, n = 0;
    for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) if (i >= 0 && j >= 0 && i < s->w && j < s->h) n += (s->px[j * s->w + i] & 0xFFFFFFu) == rgb;
    return n;
}

static int button_enabled(lj_window *win, const char *name) { lj_widget *w = lj_window_find(win, name); return w ? (int)w->enabled : -1; }
static int button_visible(lj_window *win, const char *name) { lj_widget *w = lj_window_find(win, name); return w ? (int)w->visible : -1; }

static int same_file(const char *a, const char *b, long *at)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    unsigned char ba[4096], bb[4096];
    long off = 0;
    int rc = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    for (;;) {
        size_t na = fread(ba, 1, sizeof ba, fa), nb = fread(bb, 1, sizeof bb, fb), i;
        if (na != nb) { rc = 0; *at = off + (long)(na < nb ? na : nb); break; }
        if (na == 0) break;
        for (i = 0; i < na; i++) if (ba[i] != bb[i]) { rc = 0; *at = off + (long)i; break; }
        if (!rc) break;
        off += (long)na;
    }
    fclose(fa); fclose(fb);
    return rc;
}

static int menu_leaf_bound(const lj_menu *m, const lj_menu_item_desc *items, int n, int *leaves)
{
    int i, bad = 0;
    for (i = 0; i < n; i++) {
        const lj_menu_item_desc *it = &items[i];
        if (it->children && it->nchildren) { bad += menu_leaf_bound(m, it->children, it->nchildren, leaves); continue; }
        if (!it->caption || strcmp(it->caption, "-") == 0) continue;
        (*leaves)++;
        if (it->on_click) {
            lj_menu_rt *rt = lj_menu_rt_find((lj_menu *)m, it);
            if (!rt || !rt->fn) { bad++; printf("  unbound leaf %s (%s)\n", it->name, it->on_click); }
        }
    }
    return bad;
}

int lj_test_desktop(int argc, char **argv);
int lj_test_desktop(int argc, char **argv)
{
    int write_golden = argc > 1 && strcmp(argv[1], "--write-golden") == 0;
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_profile mh, dim;
    lj_window *win;
    lj_widget *w;
    lj_menu *m;
    const lj_main_top_rule *top; const lj_main_inert_leaf *inert;
    const lj_menu_desc *bar;
    lj_rect cr;
    int ntop, ninert, i, leaves = 0, n;
    char golden[1024], tmp[1024];
    long at = 0;

    lj_log_set_sink(sink, NULL);
    memset(&mh, 0, sizeof mh); memset(&dim, 0, sizeof dim);
    mh.footprint = 16; mh.windows = lj_mh_windows; mh.nwindows = LJ_COUNT_OF(lj_mh_windows); mh.reset.enabled = 1;
    dim.footprint = 1; dim.windows = lj_dim_windows; dim.nwindows = LJ_COUNT_OF(lj_dim_windows);
    for (i = 0; i < 5; i++) {   /* (1)..(5): three heads at 1:001/017/033, two dimmers at 1:100/101 */
        lj_fixture *f = &show->fixture[i];
        f->kind = 1; f->profile_id = (uint16_t)(i < 3 ? 1 : 2); f->link = 1; f->address = (uint16_t)(i < 3 ? 1 + 16 * i : 100 + i - 3);
        snprintf(f->user_name, sizeof f->user_name, i < 3 ? "MAC500" : "DMX1");
        f->placed = 1; f->tab = 0; f->icon_x = (int16_t)(4 + i * LJ_ICON_PITCH); f->icon_y = 4;
    }
    show->current_seq = NULL;
    ctx->show = show; ctx->lib = NULL; ctx->main_ini = NULL;
    ctx->allow_freeze = ctx->allow_offline = ctx->allow_solo = ctx->show_nextprev = 1;

    /* ---- 1. geometry [07 s4] ---- */
    {   /* [07 s4]: 30-px rows, 25x25 TSpeedButtons, 17-px status bar, 63x53 tile (unselectedfixture.gif) */
        int row_h = LJ_TB_ROW_H, btn = LJ_TB_BTN, sb_h = LJ_SB_H, tw = LJ_FIXICON_W, th = LJ_FIXICON_H;
        CHECK(row_h == 30); CHECK(btn == 25); CHECK(sb_h == 17); CHECK(tw == 63 && th == 53);
    }

    /* ---- 2. the window + ME-29 ---- */
    logged = 0;
    win = lj_desktop_create();
    CHECK(win != NULL);
    if (!win) { lj_show_free(show); return 1; }
    CHECK(logged == 0);   /* no "no binding for handler" / "matches no DFM handler" line (ME-16) */
    lj_window_client_rect(win, &cr);
    CHECK(cr.w == 791 && cr.h == 560);                                     /* TLJMAINFORM 799x606 outer, client derived */
    w = lj_window_find(win, "tb_row_seqcue");  CHECK(w && w->y == 0 && w->h == LJ_TB_ROW_H && w->w == cr.w);
    w = lj_window_find(win, "tb_row_cuelist"); CHECK(w && w->y == LJ_TB_ROW_H);
    w = lj_window_find(win, "tb_row_fixture"); CHECK(w && w->y == 2 * LJ_TB_ROW_H);
    w = lj_window_find(win, "sb_status");      CHECK(w && w->y == cr.h - LJ_SB_H && w->h == LJ_SB_H);
    w = lj_window_find(win, "desktop_tabs");   CHECK(w && w->y == 3 * LJ_TB_ROW_H && w->h == cr.h - 3 * LJ_TB_ROW_H - LJ_SB_H);
    w = lj_window_find(win, "tb_seqcue_2");    CHECK(w && w->w == 25 && w->h == 25 && w->y == 2);
    w = lj_window_find(win, "tb_fixture_8");   CHECK(w && w->x == 196 && w->w == 25);              /* ME-72: Lamp after the 17-px gap, toolandstatusbars.gif bevel 205 - frame 9 */
    w = lj_window_find(win, "tb_fixture_10");  CHECK(w && w->x == 248 && w->w == 50);              /* Controls 257 - 9 */
    w = lj_window_find(win, "sb_seqcounter");  CHECK(w && strcmp(lj_w_caption(w), "1:1") == 0);   /* [F7] empty sequence */
    CHECK(strcmp(lj_window_caption(win)->text, "Please Wait..") == 0);     /* LORD_FIDELITY-20: the DFM caption until the library is open */
    lj_desktop_set_library("Default");
    CHECK(strcmp(lj_window_caption(win)->text, "Martin LightJockey (User lib: Default)") == 0);   /* ME-37, F1 */

    m = lj_window_menus(win);
    top = lj_main_menu_top_table(&ntop); inert = lj_main_menu_inert_table(&ninert);
    bar = lj_form_find_menu(&lj_form_TLJMAINFORM, "MainMenu1");
    CHECK(bar && bar->n == 12 && ntop == 12);
    for (i = 0; bar && i < bar->n; i++) {   /* every top-level item of F5 has a rule, applied */
        int k, found = 0;
        for (k = 0; k < ntop; k++) if (strcmp(top[k].name, bar->items[i].name) == 0) { found = 1; CHECK((int)lj_menu_rt_named(m, top[k].name)->visible == (top[k].visible == 1)); }
        CHECK(found);
    }
    CHECK(lj_menu_rt_named(m, "M_OffLine") && !lj_menu_rt_named(m, "M_OffLine")->visible);   /* D12 */
    CHECK(lj_menu_rt_named(m, "M_System")->visible && lj_menu_rt_named(m, "M_MHelp")->visible && lj_menu_rt_named(m, "M_LJManager")->visible);
    for (i = 0; i < ninert; i++) { lj_menu_rt *rt = lj_menu_rt_named(m, inert[i].name); CHECK(rt && !rt->enabled); CHECK(strcmp(inert[i].hint, "Not available in LJ4EVR") == 0); }
    CHECK(lj_menu_rt_named(m, lj_main_menu_removed_item()) && !lj_menu_rt_named(m, lj_main_menu_removed_item())->visible);
    n = menu_leaf_bound(m, bar->items, bar->n, &leaves);
    CHECK(n == 0); CHECK(leaves > 60);
    n = menu_leaf_bound(m, lj_main_popup_desktop.items, lj_main_popup_desktop.n, &leaves); CHECK(n == 0);
    n = menu_leaf_bound(m, lj_main_popup_icon.items, lj_main_popup_icon.n, &leaves); CHECK(n == 0);
    printf("  menu: %d leaves, every one bound; %d inert; %s removed\n", leaves, ninert, lj_main_menu_removed_item());

    /* ---- 3. selection rules [01 s3.2] on the five icons ---- */
    lj_sel_reset();
    CHECK(lj_sel_master() == -1 && lj_sel_count() == 0);
    lj_sel_click(0, 0, 0); CHECK(lj_sel_master() == 0 && lj_sel_count() == 1);          /* click = master */
    lj_sel_click(1, 0, 0); CHECK(lj_sel_master() == 1 && lj_sel_count() == 2);          /* Inclusive adds, the new one is master */
    lj_sel_click(2, 1, 0); CHECK(lj_sel_master() == 2 && lj_sel_count() == 3);          /* Shift = force master */
    lj_sel_click(0, 1, 0); CHECK(lj_sel_master() == 0 && lj_sel_count() == 3);          /* Shift on a selected slave: master, nothing dropped */
    lj_sel_click(1, 0, 0); CHECK(!lj_sel_is_selected(1) && lj_sel_count() == 2);        /* a selected icon de-selects */
    lj_sel_click(3, 0, 1); CHECK(lj_sel_master() == 3 && lj_sel_count() == 1);          /* Ctrl = solo select */
    lj_sel_mode_set(LJ_SEL_EXCLUSIVE);
    lj_sel_click(4, 0, 0); CHECK(lj_sel_master() == 4 && lj_sel_count() == 1 && !lj_sel_is_selected(3));   /* Exclusive replaces */
    lj_sel_mode_set(LJ_SEL_INCLUSIVE);
    lj_sel_all(); CHECK(lj_sel_count() == 5 && lj_sel_master() == 4);
    lj_sel_none(); CHECK(lj_sel_count() == 0 && lj_sel_master() == -1);
    lj_sel_next(); CHECK(lj_sel_master() == 0);                                        /* nothing selected: Next = lowest */
    lj_sel_next(); CHECK(lj_sel_master() == 1 && lj_sel_count() == 1);
    lj_sel_prev(); lj_sel_prev(); CHECK(lj_sel_master() == 4);                        /* wraps */
    lj_sel_none(); lj_sel_prev(); CHECK(lj_sel_master() == 4);                        /* nothing selected: Previous = highest */
    lj_sel_none(); lj_desktop_icons_refresh();
    lj_sel_band(0, 0, 4 + 2 * LJ_ICON_PITCH + 10, 60, 0); CHECK(lj_sel_count() == 3 && lj_sel_is_selected(2) && !lj_sel_is_selected(3));   /* band touches (1)(2)(3) */
    lj_sel_band(4 + LJ_ICON_PITCH - 2, 0, 4 + 2 * LJ_ICON_PITCH + LJ_FIXICON_W + 2, 60, 1); CHECK(lj_sel_count() == 2 && lj_sel_master() == 1);   /* Ctrl: inside only (2)(3) */
    lj_sel_band(700, 300, 720, 320, 0); CHECK(lj_sel_count() == 0);                   /* empty band deselects all */
    CHECK(lj_desktop_icon_at(10, 10) == 0 && lj_desktop_icon_at(4 + LJ_ICON_PITCH + 5, 10) == 1 && lj_desktop_icon_at(700, 300) == -1);

    /* ---- 4. F9 enables: the fixlib stub has no profiles, so a head and a dimmer are fed through a hook-free path: the toolbar asks lj_sel_master_profile() ---- */
    lj_sel_none(); lj_desktop_toolbar_update();
    for (i = 1; i <= 9; i++) { char nm[24]; snprintf(nm, sizeof nm, "tb_fixture_%d", i); CHECK(button_enabled(win, nm) == 0); }   /* nothing selected: all nine disabled */
    CHECK(button_enabled(win, "tb_fixture_22") == 1);                                 /* Blackout always */
    CHECK(button_visible(win, "tb_fixture_17") == 1 && button_visible(win, "tb_fixture_15") == 1);   /* Solo / Prev shown with the LJ2 defaults */
    ctx->allow_solo = 0; ctx->show_nextprev = 0; ctx->allow_offline = 0; ctx->allow_freeze = 0;
    lj_desktop_toolbar_update();
    CHECK(button_visible(win, "tb_fixture_17") == 0 && button_visible(win, "tb_fixture_15") == 0 && button_visible(win, "tb_fixture_16") == 0);   /* LORD_FIDELITY-11: hidden, not greyed */
    CHECK(button_enabled(win, "tb_fixture_19") == 0 && button_enabled(win, "tb_fixture_20") == 0);
    ctx->allow_solo = ctx->show_nextprev = ctx->allow_offline = ctx->allow_freeze = 1;
    lj_desktop_toolbar_update();
    CHECK(button_enabled(win, "tb_cuelist_7") == 0 && button_enabled(win, "tb_cuelist_10") == 0);   /* CD / DMX-in greyed (F8) */
    {   /* a MAC 500-like master through the table-driven rule: lj_profile_window on the two synthetic profiles */
        const lj_profile *p = &mh;
        CHECK(lj_profile_window(p, LJ_CTL_INTENSITY) && lj_profile_window(p, LJ_CTL_EXTENDED) && !lj_profile_window(p, LJ_CTL_LEVELS));
        CHECK(lj_profile_window(&dim, LJ_CTL_LEVELS) && !lj_profile_window(&dim, LJ_CTL_INTENSITY) && !lj_profile_window(&dim, LJ_CTL_MOVEMENT));
        CHECK(lj_fixture_icon_bitmap(&mh) != NULL && lj_fixture_icon_bitmap(&dim) != NULL && lj_fixture_icon_bitmap(&mh) != lj_fixture_icon_bitmap(&dim));
    }

    /* ---- 5. glyph ink + golden ---- */
    lj_sel_none();
    lj_desktop_status_set(LJ_SB_CLOCK, "12:59:17");   /* the clock of statusbar.gif, so the golden is deterministic */
    lj_desktop_status_set(LJ_SB_MEMORY, "79%");       /* statusbar.gif reads 79% green (07 fact-check 19) */
    lj_window_paint(win);
    {
        const lj_surface *s = lj_window_surface(win);
        int ax, ay, face;
        w = lj_window_find(win, "tb_seqcue_2");                                   /* New: the white page glyph */
        lj_widget_abs(w, &ax, &ay);                                             /* lj_widget_abs includes the root's frame offset */
        CHECK(count_rgb(s, ax + 4, ay + 4, 17, 17, LJ_CLR_BTNHIGHLIGHT) > 40);
        w = lj_window_find(win, "tb_fixture_1");                                  /* Intensity, disabled: the emboss (#808080 + white), no black */
        lj_widget_abs(w, &ax, &ay);
        face = count_rgb(s, ax + 4, ay + 4, 17, 17, LJ_CLR_BTNSHADOW);
        CHECK(face > 20 && count_rgb(s, ax + 4, ay + 4, 17, 17, LJ_CLR_3DDKSHADOW) == 0);
        w = lj_window_find(win, "icon1");                                         /* (1) MAC500 tile: blue caption bar */
        CHECK(w != NULL);
        if (w) { lj_widget_abs(w, &ax, &ay); CHECK(count_rgb(s, ax + 2, ay + 1, 20, 10, 0x0000FFu) > 50); }
    }
    snprintf(golden, sizeof golden, "%s/tests/ui/golden/MAINWINDOW.bmp", LJ_SOURCE_DIR);
    snprintf(tmp, sizeof tmp, "lj_test_desktop_tmp.bmp");
    if (write_golden) { CHECK(lj_bmp_write(lj_window_surface(win), golden) == 0); printf("  wrote %s\n", golden); }
    else if (lj_bmp_write(lj_window_surface(win), tmp) == 0) {
        int rc = same_file(tmp, golden, &at);
        if (rc == 1) printf("  MAINWINDOW: identical to the golden\n");
        else { fails++; printf("FAIL MAINWINDOW: %s (byte %ld)\n", rc == 0 ? "differs from the golden" : "golden unreadable", at); }
        remove(tmp);
    }

    lj_desktop_destroy(win);
    ctx->show = NULL;
    lj_show_free(show);
    lj_log_set_sink(NULL, NULL);
    printf("test_desktop: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
