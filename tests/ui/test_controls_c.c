/* test_controls_c.c - the runnable check for src/controls/lj_ctl_colors.c, lj_ctl_gobos.c,
 * lj_ctl_beam.c, lj_ctl_effects.c: a MAC 500-shaped profile (15/01 s5-s8, the values
 * of the DMX table) opens the four windows headless; client sizes equal the 1:1
 * captures (colorcontrol.gif width, beam.gif, effectcontrol4.gif; tagged measured),
 * the colour wheel's entries and captions equal note 15/01 s5 (and the fixlib table
 * when it has landed), a palette tile click emits the table's DMX value on the row's
 * channel (selection-wide through lj_prog_set_sel once controls-a and the desktop
 * selection land; the shadow value until then), the selected tile paints the thick
 * black frame, O/S/F cycles both ways, the Index | Rotate mode pair re-targets the
 * rotating gobo values, the effect button toggles prism in / out, Show Palette hides
 * the tiles and shrinks the window. */
#include <stdio.h>
#include <string.h>
#include "controls/lj_ctl_colors.h"
#include "controls/lj_ctl_gobos.h"
#include "controls/lj_ctl_beam.h"
#include "controls/lj_ctl_effects.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_glyphs.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_lj.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- the MAC 500 tables as note 15/01 gives them (DMXTAB-MAC500 values; captions = the exe gel / gobo names) ---- */
static const lj_ctl_button m_col1[] = {   /* ch 3 (offset 2): full positions every 16 [15/01 s5] */
    { "White", 2, 0, 0, 15 }, { "Blue 111", 2, 16, 16, 31 }, { "Red 301", 2, 32, 32, 47 }, { "Magenta 507", 2, 48, 48, 63 }, { "Green 202", 2, 64, 64, 79 },
    { "Yellow 604", 2, 80, 80, 95 }, { "Purple 502", 2, 96, 96, 111 }, { "Blue 101", 2, 112, 112, 127 }, { "Pink 312", 2, 128, 128, 143 }, { "Cyan 401", 2, 144, 144, 148 } };
static const lj_ctl_button m_col2[] = {   /* ch 4 (offset 3) */
    { "White", 3, 0, 0, 15 }, { "CTC 3200-4100", 3, 16, 16, 31 }, { "CTC 3200-5600", 3, 32, 32, 47 }, { "Blue 104", 3, 48, 48, 63 }, { "Blue 108", 3, 64, 64, 79 },
    { "Green 206", 3, 80, 80, 95 }, { "Red 308", 3, 96, 96, 111 }, { "Yellow 603", 3, 112, 112, 127 }, { "CTC 5500-2900", 3, 128, 128, 143 }, { "CTC 5500-4200", 3, 144, 144, 148 } };
static const lj_ctl_button m_rot1[] = { { "CW", 2, 185, 185, 215 }, { "CCW", 2, 216, 216, 245 } };
static const lj_ctl_button m_rot2[] = { { "CW", 3, 216, 216, 230 }, { "CCW", 3, 231, 231, 245 } };
static const lj_ctl_button m_rnd[]  = { { "Rnd Slow", 3, 252, 252, 255 }, { "Rnd Med", 3, 249, 249, 251 }, { "Rnd Fast", 3, 246, 246, 248 } };   /* PROPOSAL: sub-range split (15/01 s2 ch 4) */
static const lj_ctl_row m_colors_rows[] = {
    { LJ_W_FADEBTN, 2, 1, NULL, NULL, 0, 0, 0, LJ_SNAP, NULL, 0, NULL, 0 },
    { LJ_W_SCROLL, 2, 1, "Color Wheel 1", NULL, 0, 148, 0, 0, m_col1, 10, NULL, 0 },
    { LJ_W_ROTATION, 2, 1, "Wheel Scroll Speed", NULL, 0, 0, 0, 0, m_rot1, 2, NULL, 0 },
    { LJ_W_FADEBTN, 3, 1, NULL, NULL, 0, 0, 0, LJ_SNAP, NULL, 0, NULL, 0 },
    { LJ_W_SCROLL, 3, 1, "Color Wheel 2", NULL, 0, 148, 0, 0, m_col2, 10, NULL, 0 },
    { LJ_W_ROTATION, 3, 1, "Wheel Scroll Speed", NULL, 0, 0, 0, 0, m_rot2, 2, NULL, 0 },
    { LJ_W_PALETTE, 2, 1, NULL, NULL, 0, 0, 0, 0, NULL, 0, NULL, 0 },
    { LJ_W_PALETTE, 3, 1, NULL, NULL, 1, 0, 0, 0, NULL, 0, NULL, 0 } };          /* lo = profile palette index (hand-file convention) */
static const lj_ctl_row m_colors_specials[] = { { LJ_W_SPECIAL_GROUP, 3, 1, "Random", NULL, 0, 0, 0, 0, m_rnd, 3, NULL, 0 } };

static const lj_ctl_button m_gstat[] = {   /* ch 7 (offset 6), FIX: stepped 10 [15/01 s2]; slot names PROPOSAL (15/01 s6) */
    { "Open", 6, 0, 0, 9 }, { "Cone", 6, 10, 10, 19 }, { "Holes", 6, 20, 20, 29 }, { "Lotus", 6, 30, 30, 39 }, { "Bricks", 6, 40, 40, 49 },
    { "Clouds", 6, 50, 50, 59 }, { "Machine", 6, 60, 60, 69 }, { "BreakupBambus", 6, 70, 70, 79 }, { "BreakupThreads", 6, 80, 80, 89 }, { "Pling", 6, 90, 90, 102 } };
static const lj_ctl_button m_grot[] = {    /* ch 5 (offset 4): index values; rotate values come from palette[3].e[i].v[1] */
    { "Open", 4, 0, 0, 55 }, { "Triangle", 4, 56, 56, 75 }, { "Thinbar", 4, 76, 76, 95 }, { "Fan", 4, 96, 96, 115 }, { "ThinBars", 4, 116, 116, 135 }, { "Gridball", 4, 136, 136, 155 } };
static const lj_ctl_button m_gmodes[] = { { "Index", 4, 3, 0, 155 }, { "Rotate", 4, 1, 156, 255 } };   /* value = palette type slot (02 s5.4: Def CW CCW Idx Shk) */
static const lj_ctl_button m_grotation[] = { { "CW", 5, 3, 3, 127 }, { "CCW", 5, 128, 128, 251 }, { "Stop", 5, 0, 0, 2 } };
static const lj_ctl_row m_gobos_rows[] = {
    { LJ_W_FADEBTN, 6, 1, NULL, NULL, 0, 0, 0, LJ_SNAP, NULL, 0, NULL, 0 },
    { LJ_W_SCROLL, 6, 1, "Static Gobo", NULL, 0, 102, 0, 0, m_gstat, 10, NULL, 0 },
    { LJ_W_PALETTE, 6, 1, NULL, NULL, 2, 0, 0, 0, NULL, 0, NULL, 0 },
    { LJ_W_FADEBTN, 4, 2, NULL, NULL, 0, 0, 0, LJ_SNAP, NULL, 0, NULL, 0 },
    { LJ_W_SCROLL, 4, 1, "Rotating Gobo", NULL, 0, 255, 0, 0, m_grot, 6, NULL, 0 },
    { LJ_W_MODEPAIR, 4, 1, "Index Gobo", "Rotate Gobo", 0, 0, 0, 0, m_gmodes, 2, NULL, 0 },
    { LJ_W_FADEBTN, 5, 2, NULL, NULL, 0, 0, 0, LJ_FADE, NULL, 0, NULL, 0 },
    { LJ_W_INDEXBAR, 5, 1, "Gobo Index", NULL, 0, 255, 0, 0, NULL, 0, NULL, 0 },
    { LJ_W_ROTATION, 5, 1, "Gobo Rotation", NULL, 0, 0, 0, 0, m_grotation, 3, NULL, 0 },
    { LJ_W_PALETTE, 4, 1, NULL, NULL, 3, 0, 0, 0, NULL, 0, NULL, 0 } };

static const char *const m_iris_icons[] = { "irisopen", "irisclosed" }, *const m_focus_icons[] = { "far", "near" };
static const lj_ctl_row m_beam_rows[] = {
    { LJ_W_FADEBTN, 8, 1, NULL, NULL, 0, 0, 0, LJ_OFF, NULL, 0, NULL, 0 },
    { LJ_W_DRAGBAR, 8, 1, "Iris", NULL, 0, 199, 0, 0, NULL, 0, m_iris_icons, 0 },
    { LJ_W_FADEBTN, 7, 1, NULL, NULL, 0, 0, 0, LJ_OFF, NULL, 0, NULL, 0 },
    { LJ_W_DRAGBAR, 7, 1, "Focus", NULL, 0, 255, 0, 0, NULL, 0, m_focus_icons, 0 } };

static const lj_ctl_button m_prism[] = { { "Prism", 9, 80, 80, 89 } };
static const lj_ctl_button m_prot[] = { { "CW", 9, 20, 20, 79 }, { "CCW", 9, 90, 90, 149 } };
static const char *const m_prism_tile[] = { "gobo_3facet" };
static const lj_ctl_row m_fx_rows[] = {
    { LJ_W_FADEBTN, 9, 1, NULL, NULL, 0, 0, 0, LJ_SNAP, NULL, 0, NULL, 0 },
    { LJ_W_EFFECT_BTN, 9, 1, "Prism", NULL, 0, 19, 0, 0, m_prism, 1, m_prism_tile, 0 },
    { LJ_W_ROTATION, 9, 1, "Prism Rotation", NULL, 0, 0, 0, 0, m_prot, 2, NULL, 0 } };

static const char *const m_menu_std[] = { "UserPalette", "Preferences", "Fades", "Help", NULL };
static const char *const m_menu_fx[] = { "UserPalette", "Fades", "Help", NULL };
static const lj_ctl_window m_windows[] = {
    { LJ_CTL_COLORS, "Colors", m_menu_std, m_colors_rows, LJ_COUNT_OF(m_colors_rows), m_colors_specials, 1 },
    { LJ_CTL_GOBOS, "Gobos", m_menu_std, m_gobos_rows, LJ_COUNT_OF(m_gobos_rows), NULL, 0 },
    { LJ_CTL_BEAM, "Beam", m_menu_std, m_beam_rows, LJ_COUNT_OF(m_beam_rows), NULL, 0 },
    { LJ_CTL_EFFECTS, "Effects", m_menu_fx, m_fx_rows, LJ_COUNT_OF(m_fx_rows), NULL, 0 } };
static lj_profile m_mac500;   /* ~5 KB: static */

static const char *const note_col1[] = { "White", "Blue 111", "Red 301", "Magenta 507", "Green 202", "Yellow 604", "Purple 502", "Blue 101", "Pink 312", "Cyan 401" };   /* 15/01 s5 */
static const uint32_t m_col1_rgb[] = { 0xFFFFFF, 0x2040C0, 0xE01010, 0xE020A0, 0x20B040, 0xF0E020, 0x8030C0, 0x3070E0, 0xF080B0, 0x20C0E0 };   /* ours (LORD_FIXTURES-9) */
static const char *const m_gobo_tiles[] = { "gobo_open", "gobo_cone", "gobo_holes", "gobo_lotus", "gobo_bricks", "gobo_clouds", "gobo_machine", "gobo_breakupbambus", "gobo_breakupthreads", "gobo_pling" };
static const char *const m_rot_tiles[] = { "gobo_open", "gobo_triangle", "gobo_thinbar", "gobo_fan", "gobo_thinbars", "gobo_gridball" };

static void profile_init(void)
{
    int i;
    memset(&m_mac500, 0, sizeof m_mac500);
    m_mac500.id = 12; lj_str_copy(m_mac500.tag, sizeof m_mac500.tag, "MAC500"); lj_str_copy(m_mac500.manufacturer, sizeof m_mac500.manufacturer, "Martin"); lj_str_copy(m_mac500.name, sizeof m_mac500.name, "MAC 500"); lj_str_copy(m_mac500.mode, sizeof m_mac500.mode, "Mode 2/4");
    m_mac500.footprint = 16;
    m_mac500.windows = m_windows; m_mac500.nwindows = LJ_COUNT_OF(m_windows);
    for (i = 0; i < 10; i++) {
        m_mac500.palette[0].e[i].used = 1; m_mac500.palette[0].e[i].v[0] = m_col1[i].value; m_mac500.palette[0].e[i].rgb = m_col1_rgb[i];
        m_mac500.palette[1].e[i].used = 1; m_mac500.palette[1].e[i].v[0] = m_col2[i].value; m_mac500.palette[1].e[i].rgb = i == 0 ? 0xFFFFFFu : 0xE0E0C0u;
        m_mac500.palette[2].e[i].used = 1; m_mac500.palette[2].e[i].v[0] = m_gstat[i].value; lj_str_copy(m_mac500.palette[2].e[i].bitmap, sizeof m_mac500.palette[2].e[i].bitmap, m_gobo_tiles[i]);
    }
    for (i = 0; i < 6; i++) {
        lj_pal_entry *e = &m_mac500.palette[3].e[i];
        e->used = 1; e->v[0] = m_grot[i].value; e->v[3] = m_grot[i].value; e->v[1] = i == 0 ? 0 : (uint8_t)(236 - 20 * (i - 1));   /* rotate: reversed order, 156-255 [15/01 s2 ch 5] */
        lj_str_copy(e->bitmap, sizeof e->bitmap, m_rot_tiles[i]);
    }
}

/* ---- helpers ---- */
static lj_ctl_c_bind *bind_of(lj_ctl_c *c, int role, uint8_t ch, int entry)
{
    int i;
    for (i = 0; i < c->nb; i++) if (c->b[i].role == role && c->b[i].ch == ch && (entry < 0 || c->b[i].entry == entry)) return &c->b[i];
    return NULL;
}
static void click_at(lj_window *win, lj_widget *w, int dx, int dy, int button)   /* through lj_window_event, surface coordinates */
{
    lj_ui_event ev;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.x = ax + dx; ev.u.mouse.y = ay + dy; ev.u.mouse.button = button; ev.u.mouse.clicks = 1;
    lj_window_event(win, &ev);
    ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP;
    lj_window_event(win, &ev);
}
static int value_of(lj_ctl_c *c, uint8_t ofs) { uint8_t v = 0, st = 0; return lj_ctl_c_value(c, ofs, &v, &st) == 0 ? v : -1; }
static uint32_t px_at(lj_window *win, lj_widget *w, int dx, int dy)
{
    const lj_surface *s = lj_window_surface(win);
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);
    return s->px[(ay + dy) * s->w + ax + dx];
}

int lj_test_controls_c(int argc, char **argv);
int lj_test_controls_c(int argc, char **argv)
{
    lj_window *colors, *gobos, *beam, *fx;
    lj_ctl_c *c;
    lj_ctl_c_bind *b, *t0, *t3;
    lj_widget *root;
    const lj_profile *fixlib;
    const lj_ctl_window *t;
    int i, seam = 0;
    (void)argc; (void)argv;
    profile_init();

    /* ---- the fixlib MAC 500 table against note 15/01 s5 (skips until src/fixlib/hand/mac500.c lands) ---- */
    fixlib = lj_fixlib_by_id(12);
    t = fixlib ? lj_profile_window(fixlib, LJ_CTL_COLORS) : NULL;
    if (t) {                                                               /* the hand file keeps the wheel in palette[0]: slots 1-6 RGB blocks, 7-10 the exe bitmap names (configurefixturepalette.gif) */
        const lj_palette *pal = &fixlib->palette[0];
        lj_window *fw;
        int n = 0;
        while (n < LJ_PAL_ENTRIES && pal->e[n].used) n++;
        CHECK(n == 10);
        for (i = 0; i < 10 && i < n; i++) CHECK(pal->e[i].v[0] == 16 * i);
        for (i = 6; i < 10 && i < n; i++) CHECK(strcmp(pal->e[i].bitmap, note_col1[i]) == 0);
        printf("controls_c: fixlib MAC 500 Colors wheel 1: %d entries checked against 15/01\n", n);
        fw = lj_ctl_colors_create(fixlib);
        CHECK(fw && lj_window_root(fw)->w == 376 && lj_window_root(fw)->h == 146);
        if (fw) {
            lj_ctl_c *fc = lj_ctl_c_of(fw);
            lj_ctl_c_bind *ft = bind_of(fc, LJ_CTL_C_TILE_ROLE, 2, 9);
            CHECK(ft && ((lj_w_palette *)ft->w)->swatch_rgb == pal->e[9].rgb);
            click_at(fw, ft->w, 10, 10, 1);
            CHECK(value_of(fc, 2) == 144);
            lj_ctl_colors_destroy(fw);
        }
        fw = lj_ctl_gobos_create(fixlib);
        if (fw) { printf("controls_c: fixlib MAC 500 Gobos client %dx%d\n", lj_window_root(fw)->w, lj_window_root(fw)->h); lj_window_paint(fw); lj_ctl_gobos_destroy(fw); }
        fw = lj_ctl_beam_create(fixlib);
        CHECK(fw && lj_window_root(fw)->w == 251 && lj_window_root(fw)->h == 52);
        if (fw) lj_ctl_beam_destroy(fw);
        fw = lj_ctl_effects_create(fixlib);
        if (fw) { lj_window_paint(fw); lj_ctl_effects_destroy(fw); }
    } else printf("controls_c: SKIP fixlib MAC 500 table (lj_fixlib_by_id(12) has no Colors window yet); the test profile carries the 15/01 values\n");
    CHECK(m_colors_rows[1].nbuttons == 10);
    for (i = 0; i < 10; i++) CHECK(strcmp(m_col1[i].caption, note_col1[i]) == 0);

    /* ---- Colors: colorcontrol.gif 307x227 is a 0.8 reduction (ME-67): 376 wide (O/S/F 48, scroll control 165, rotation 161), 2 wheel lines (42) + 2 palette rows (31) = 146 ---- */
    colors = lj_ctl_colors_create(&m_mac500);
    CHECK(colors != NULL);
    if (!colors) return 1;
    c = lj_ctl_c_of(colors);
    root = lj_window_root(colors);
    CHECK(c != NULL && c->kind == LJ_CTL_COLORS);
    CHECK(root->w == 376 && root->h == 146);
    CHECK(strcmp(lj_window_caption(colors)->text, "Colors") == 0);
    CHECK(lj_window_caption(colors)->icons == LJ_BI_SYSMENU && lj_window_caption(colors)->border == LJ_BS_SINGLE);
    CHECK(lj_window_menu(colors) && lj_window_menu(colors)->n == 4 && strcmp(lj_window_menu(colors)->items[0].caption, "&UserPalette") == 0);
    CHECK(lj_menu_rt_named(lj_window_menus(colors), "M_UserPalette") && lj_menu_rt_named(lj_window_menus(colors), "M_UserPalette")->enabled == 0);
    CHECK(lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette") && lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->checked == 1);
    CHECK(lj_menu_rt_named(lj_window_menus(colors), "M_Fades") && strcmp(lj_menu_rt_named(lj_window_menus(colors), "M_Fades")->item->caption, "&Fades") == 0);
    printf("controls_c: Colors client %dx%d, %d bound widgets\n", root->w, root->h, c->nb);

    /* scroll control at the content column, rotation panel right of it, 20 tiles (31x30) at the 32-px pitch */
    b = bind_of(c, LJ_CTL_C_SCROLL, 2, -1);
    CHECK(b && b->w->x == LJ_CTL_C_COL && b->w->y == 0 && b->w->w == LJ_SCROLLCTL_W && b->w->h == LJ_SCROLLCTL_H);
    CHECK(((lj_w_scrollctl *)b->w)->big == 16);
    b = bind_of(c, LJ_CTL_C_ROT_BAR, 2, -1);
    CHECK(b && b->w->parent && b->w->parent->x == LJ_CTL_C_COL + LJ_SCROLLCTL_W + 1 && b->w->parent->x + b->w->parent->w == 376);
    t0 = bind_of(c, LJ_CTL_C_TILE_ROLE, 2, 0); t3 = bind_of(c, LJ_CTL_C_TILE_ROLE, 2, 3);
    CHECK(t0 && t3 && t0->w->w == 31 && t0->w->h == 30 && t3->w->x - t0->w->x == 3 * 32 && t0->w->y == 85);
    b = bind_of(c, LJ_CTL_C_TILE_ROLE, 3, 9);
    CHECK(b && b->w->y == 116 && b->w->x == 1 + 9 * 32);

    /* ---- a palette tile click emits the table's value on the row's channel ---- */
    lj_window_paint(colors);
    CHECK(t0 && lj_w_palette_selected(t0->w));                           /* nothing programmed reads as 0 = White */
    click_at(colors, t3->w, 10, 10, 1);
    CHECK(value_of(c, 2) == 48);                                          /* Magenta 507 = 48 on ch 3 (offset 2) */
    CHECK(lj_w_palette_selected(t3->w) && !lj_w_palette_selected(t0->w));
    b = bind_of(c, LJ_CTL_C_SCROLL, 2, -1);
    CHECK(b && lj_w_scrollctl_value(b->w) == 48 && ((lj_w_scrollctl *)b->w)->swatch_rgb == m_col1_rgb[3]);
    lj_window_paint(colors);
    CHECK(px_at(colors, t3->w, 0, 0) == LJ_CLR_3DDKSHADOW && px_at(colors, t3->w, 1, 1) == LJ_CLR_3DDKSHADOW && px_at(colors, t3->w, 2, 2) == m_col1_rgb[3]);   /* thick black frame */
    CHECK(px_at(colors, t0->w, 0, 0) == LJ_CLR_BTNSHADOW && px_at(colors, t0->w, 1, 1) == 0xFFFFFFu);                                                            /* 1-px grey frame, white gel */
    if (lj_prog_master_ch(2) >= 0) {                                      /* selection-wide: every selected fixture of the master's profile (controls-a + desktop) */
        uint8_t v = 0, st = 0;
        seam = 1;
        CHECK(lj_prog_get((uint16_t)lj_prog_master_ch(2), &v, &st) == 0 && v == 48);
    }
    /* the scroll control's split step: one DMX step, the whole step: one entry */
    b = bind_of(c, LJ_CTL_C_SCROLL, 2, -1);
    click_at(colors, b->w, 105 + 10, 6 + 10, 1);                          /* ►| : +1 (lj_w_scrollctl button 3) */
    CHECK(value_of(c, 2) == 49);
    click_at(colors, b->w, 134 + 10, 6 + 10, 1);                          /* ► : +16 (one whole position) */
    CHECK(value_of(c, 2) == 65 && lj_w_palette_selected(bind_of(c, LJ_CTL_C_TILE_ROLE, 2, 4)->w));   /* nearest entry: Green 202 (64) */

    /* ---- O/S/F: left click cycles Snap -> Fade, right click back (D10); the channel state follows through lj_prog_set_state_sel ---- */
    b = bind_of(c, LJ_CTL_C_OSF, 2, -1);
    CHECK(b && b->w->x == 0 && b->w->y == 1 && b->w->w == 48 && b->w->h == 40 && lj_w_osf_state(b->w) == LJ_SNAP);
    click_at(colors, b->w, 10, 10, 1);
    CHECK(lj_w_osf_state(b->w) == LJ_FADE);
    click_at(colors, b->w, 10, 10, 2);
    CHECK(lj_w_osf_state(b->w) == LJ_SNAP);
    if (seam) { uint8_t v = 0, st = 0; CHECK(lj_prog_get((uint16_t)lj_prog_master_ch(2), &v, &st) == 0 && st == LJ_SNAP); }

    /* ---- wheel rotation: CW writes 185 + speed inside 185..215; Rnd specials hidden until Show Special ---- */
    b = bind_of(c, LJ_CTL_C_ROT_CW, 2, -1);
    CHECK(b && !lj_ctl_c_btn_down(b->w));
    click_at(colors, b->w, 5, 5, 1);
    CHECK(value_of(c, 2) == 185 && lj_ctl_c_btn_down(b->w));
    b = bind_of(c, LJ_CTL_C_SPECIAL_BTN, 3, 0);
    CHECK(b && !b->w->visible && strcmp(b->btn->caption, "Rnd Slow") == 0);
    lj_menu_rt_named(lj_window_menus(colors), "M_ShowSpecial")->fn(NULL, lj_menu_rt_named(lj_window_menus(colors), "M_ShowSpecial")->ud);
    c = lj_ctl_c_of(colors);
    b = bind_of(c, LJ_CTL_C_SPECIAL_BTN, 3, 1);
    CHECK(b && b->w->visible && root->w > 376 && lj_menu_rt_named(lj_window_menus(colors), "M_ShowSpecial")->checked == 1);
    click_at(colors, b->w, 5, 5, 1);
    CHECK(value_of(c, 3) == 249 && lj_ctl_c_btn_down(b->w));             /* Rnd Med */
    /* Show Palette off: the tiles go, the window shrinks to the two wheel lines */
    lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->fn(NULL, lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->ud);
    CHECK(root->h == 84 && lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->checked == 0);
    CHECK(bind_of(c, LJ_CTL_C_TILE_ROLE, 2, 0) && !bind_of(c, LJ_CTL_C_TILE_ROLE, 2, 0)->w->visible);
    lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->fn(NULL, lj_menu_rt_named(lj_window_menus(colors), "M_ShowPalette")->ud);
    CHECK(root->h == 146);

    /* ---- Gobos: static row + palette, rotating group with the Index | Rotate pair and the Fade / index / rotation group to its right (goboindex.gif) ---- */
    gobos = lj_ctl_gobos_create(&m_mac500);
    CHECK(gobos != NULL);
    if (gobos) {
        lj_ctl_c *g = lj_ctl_c_of(gobos);
        lj_widget *gr = lj_window_root(gobos);
        lj_ctl_c_bind *mode1, *tile1, *idx, *scroll;
        printf("controls_c: Gobos client %dx%d\n", gr->w, gr->h);
        CHECK(gr->h == 42 + 31 + 42 + 26 + 31);                             /* static line, 10 tiles, rotating line, mode pair, 6 tiles (ME-67 pitches) */
        scroll = bind_of(g, LJ_CTL_C_SCROLL, 6, -1);
        CHECK(scroll && ((lj_w_scrollctl *)scroll->w)->tile == lj_glyph_get("gobo_open") && ((lj_w_scrollctl *)scroll->w)->big == 10);
        tile1 = bind_of(g, LJ_CTL_C_TILE_ROLE, 6, 1);
        CHECK(tile1 && ((lj_w_palette *)tile1->w)->tile == lj_glyph_get("gobo_cone") && tile1->w->y == 43);
        click_at(gobos, tile1->w, 12, 12, 1);
        CHECK(value_of(g, 6) == 10);
        b = bind_of(g, LJ_CTL_C_OSF, 4, -1);
        CHECK(b && b->w->y == 74 && b->w->h == 42 + 26 - 2);                /* Snap spans the scroll + mode lines */
        idx = bind_of(g, LJ_CTL_C_INDEX, 5, -1);
        b = bind_of(g, LJ_CTL_C_OSF, 5, -1);
        CHECK(idx && b && b->w->x == LJ_CTL_C_COL + LJ_SCROLLCTL_W + 5 && b->w->y == 74 && idx->w->y == 73 && idx->w->x == b->w->x + 31 && b->w->w == 30);
        b = bind_of(g, LJ_CTL_C_ROT_BAR, 5, -1);
        CHECK(b && b->w->parent->y == 99 && b->w->parent->x == idx->w->x);
        mode1 = bind_of(g, LJ_CTL_C_MODE, 4, 1);
        CHECK(mode1 && strcmp(mode1->btn->caption, "Rotate") == 0 && !lj_ctl_c_btn_down(mode1->w) && mode1->w->y == 116);
        tile1 = bind_of(g, LJ_CTL_C_TILE_ROLE, 4, 1);
        CHECK(tile1 && tile1->w->y == 142);
        click_at(gobos, tile1->w, 12, 12, 1);
        CHECK(value_of(g, 4) == 56);                                        /* Triangle, Index mode */
        click_at(gobos, mode1->w, 10, 10, 1);
        CHECK(g->mode[4] == 1 && lj_ctl_c_btn_down(mode1->w) && value_of(g, 4) == 236);   /* re-sent in the Rotate range */
        click_at(gobos, bind_of(g, LJ_CTL_C_TILE_ROLE, 4, 2)->w, 12, 12, 1);
        CHECK(value_of(g, 4) == 216 && lj_w_palette_selected(bind_of(g, LJ_CTL_C_TILE_ROLE, 4, 2)->w));
        click_at(gobos, bind_of(g, LJ_CTL_C_MODE, 4, 0)->w, 10, 10, 1);
        CHECK(g->mode[4] == 0 && value_of(g, 4) == 76);
        click_at(gobos, idx->w, idx->w->w - 5, 10, 1);                       /* index bar ► : +1 */
        CHECK(value_of(g, 5) == 1);
        b = bind_of(g, LJ_CTL_C_ROT_CCW, 5, -1);
        click_at(gobos, b->w, 5, 5, 1);
        CHECK(value_of(g, 5) == 128 && lj_ctl_c_btn_down(b->w));
        click_at(gobos, b->w, 5, 5, 1);                                      /* the active direction again: Stop (RN 2.5) */
        CHECK(value_of(g, 5) == 0 && !lj_ctl_c_btn_down(b->w));
        lj_window_paint(gobos);
        lj_ctl_gobos_destroy(gobos);
    }

    /* ---- Beam: 251x52 (beam.gif 206x77 is a 0.8 reduction: 257x96 less frame and caption/menu band, ME-67) ---- */
    beam = lj_ctl_beam_create(&m_mac500);
    CHECK(beam != NULL);
    if (beam) {
        lj_ctl_c *bm = lj_ctl_c_of(beam);
        lj_widget *br = lj_window_root(beam);
        printf("controls_c: Beam client %dx%d\n", br->w, br->h);
        CHECK(br->w == 251 && br->h == 52);
        b = bind_of(bm, LJ_CTL_C_BEAM_BAR, 8, -1);
        CHECK(b && b->w->x == LJ_CTL_C_COL + 28 && b->w->w == 146 && b->w->y == 0);
        click_at(beam, b->w, b->w->w - 5, 10, 1);
        CHECK(value_of(bm, 8) == 1);
        click_at(beam, b->w, b->w->w - 5, 10, 2);                            /* right-click ► = max of the range */
        CHECK(value_of(bm, 8) == 199);
        b = bind_of(bm, LJ_CTL_C_OSF, 7, -1);
        CHECK(b && b->w->y == 27 && b->w->h == 24 && lj_w_osf_state(b->w) == LJ_OFF);
        lj_window_paint(beam);
        lj_ctl_beam_destroy(beam);
    }

    /* ---- Effects: one line, prism button + rotation; menu UserPalette Fades Help (15/01 s8) ---- */
    fx = lj_ctl_effects_create(&m_mac500);
    CHECK(fx != NULL);
    if (fx) {
        lj_ctl_c *f = lj_ctl_c_of(fx);
        lj_widget *fr = lj_window_root(fx);
        printf("controls_c: Effects client %dx%d\n", fr->w, fr->h);
        CHECK(fr->h == 42 && fr->w == LJ_CTL_C_COL + 5 + 34 + 2 + LJ_CTL_C_ROT_W);
        CHECK(lj_window_menu(fx)->n == 3 && strcmp(lj_window_menu(fx)->items[1].caption, "&Fades") == 0);
        b = bind_of(f, LJ_CTL_C_FXBTN, 9, -1);
        CHECK(b && b->w->x == LJ_CTL_C_COL + 5 && b->w->w == 34 && !lj_ctl_c_btn_down(b->w));
        click_at(fx, b->w, 10, 10, 1);
        CHECK(value_of(f, 9) == 80 && lj_ctl_c_btn_down(b->w));
        click_at(fx, bind_of(f, LJ_CTL_C_ROT_CW, 9, -1)->w, 5, 5, 1);
        CHECK(value_of(f, 9) == 20);
        click_at(fx, b->w, 10, 10, 1);                                       /* prism button while rotating: in (80) */
        CHECK(value_of(f, 9) == 80);
        click_at(fx, b->w, 10, 10, 1);                                       /* out */
        CHECK(value_of(f, 9) == 0 && !lj_ctl_c_btn_down(b->w));
        lj_window_paint(fx);
        lj_ctl_effects_destroy(fx);
    }

    lj_ctl_colors_destroy(colors);
    CHECK(lj_ctl_c_of(colors) == NULL);
    CHECK(lj_ctl_colors_create(NULL) == NULL);
    printf("controls_c: %d failures%s\n", fails, seam ? "" : " (lj_prog_* / lj_sel_* stubs: selection-wide checks skipped)");
    return fails ? 1 : 0;
}
