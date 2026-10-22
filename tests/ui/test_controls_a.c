/* test_controls_a.c - the runnable check for src/controls/lj_ctl_common.c, lj_ctl_intensity.c,
 * lj_ctl_levels.c, lj_ctl_lamp.c, lj_ctl_extended.c (W1 controls-a):
 *  - lj_prog_cycle_state honours D10 (left/right order) and the ME-27 states mask;
 *  - the Intensity window built from the design 6.4 MAC 500 table measures 480x98 like
 *    intensitycontrol.gif, with its rows where the GIF has them;
 *  - the drag-bar caption reads "Intensity 100,0%" under a comma locale and "100.0%"
 *    under a point locale (D6, ME-52);
 *  - O/S/F left-click cycles Off -> Snap -> Fade, right-click the reverse, and the
 *    handler can read the button through lj_window_last_event (W0-last-event);
 *  - the Lamp window's O/S/F never reaches Fade (ME-27), nor does a SWITCH level's (ME-25);
 *  - with a real desktop selection: a selection-wide write lands on 3 fixtures, two
 *    O/S/F buttons bound to one channel repaint from one scene state, the shutter
 *    toggle and the Lamp Off confirm write the scene. The W1 desktop stub makes that
 *    block print SKIP instead of failing. */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl_common.h"
#include "controls/lj_ctl_intensity.h"
#include "controls/lj_ctl_lamp.h"
#include "core/lj_locale.h"
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- the tables: TIntensityControl_Mac500 as design 6.4 writes it, Lamp, Levels, Extended per note 15/01 s9-s10 ---- */
static const lj_ctl_button mac500_pulse[]  = { {"Pulse Open", 0, 80, 80, 99}, {"Pulse Close", 0, 100, 100, 119} };
static const lj_ctl_button mac500_rpulse[] = { {"Open Fast", 0, 191, 191, 193}, {"Open Slow", 0, 194, 194, 196}, {"Close Fast", 0, 197, 197, 199}, {"Close Slow", 0, 200, 200, 202} };
static const lj_ctl_button mac500_rstrobe[]= { {"Strobe Slow", 0, 168, 168, 187}, {"Strobe Med", 0, 148, 148, 167}, {"Strobe Fast", 0, 128, 128, 147} };
static const lj_ctl_row mac500_intensity_rows[] = {
    { LJ_W_FADEBTN,        0, 2, .dflt_state = LJ_SNAP },
    { LJ_W_SHUTTER_TOGGLE, 0, 1, "Shutter Closed", "Shutter Open", .lo = 0, .hi = 49 },
    { LJ_W_DRAGBAR,        0, 1, "Strobe", .lo = 50, .hi = 72, .reverse = 1 },
    { LJ_W_FADEBTN,        1, 1, .dflt_state = LJ_FADE },
    { LJ_W_DRAGBAR,        1, 1, "Intensity", .lo = 0, .hi = 255 } };
static const lj_ctl_row mac500_intensity_specials[] = {
    { LJ_W_SPECIAL_GROUP, 0, 1, "Intensity Pulse", .buttons = mac500_pulse, .nbuttons = 2, .caption2 = "Pulse Off", .lo = 80, .hi = 119 },
    { LJ_W_SPECIAL_GROUP, 0, 1, "Random Pulse",    .buttons = mac500_rpulse, .nbuttons = 4 },
    { LJ_W_SPECIAL_GROUP, 0, 1, "Random Strobe",   .buttons = mac500_rstrobe, .nbuttons = 3 } };
static const char *const menu_std[] = { "UserPalette", "Preferences", "Fades", "Help", NULL };
static const char *const menu_help[] = { "Help", NULL };

static const lj_ctl_button lamp_btns[] = { {"Lamp On", 0, 232, 228, 237}, {"Lamp Off", 0, 251, 248, 255} };   /* 15/01 s2 ch 1: 228-237 lamp on, 248-255 lamp off */
static const lj_ctl_row lamp_rows[] = {
    { LJ_W_FADEBTN,   0, 1, .dflt_state = LJ_SNAP, .states_allowed = LJ_STATES_OFF_SNAP },
    { LJ_W_LAMP_BTNS, 0, 1, .buttons = lamp_btns, .nbuttons = 2 },
    { LJ_W_NOTE,      0, 1, "NOTE !\nLamp Off must be held for at least 5 seconds", .lo = 0 } };   /* exe 0x1313EC, the "NOTE !" line above it (lampcontrol.gif) */

static const lj_ctl_row levels_rows[] = {
    { LJ_W_LEVEL_FADER, 0, 1, "Yellow", .lo = 0, .hi = 255 },
    { LJ_W_LEVEL_FADER, 1, 1, "Red",    .lo = 0, .hi = 255 },
    { LJ_W_LEVEL_FADER, 2, 1, NULL,     .lo = 0, .hi = 255 } };   /* ch 2 is a SWITCH class channel in the profile below (ME-25) */

static const lj_ctl_button pt_radios[] = { {"Tracking", 14, 0, 0, 2}, {"Speed", 14, 3, 3, 245}, {"Normal", 14, 247, 246, 248}, {"Fast", 14, 250, 249, 251}, {"B/O while moving", 14, 253, 252, 255} };
static const lj_ctl_button fx_radios[] = { {"Tracking", 15, 0, 0, 2}, {"Speed", 15, 3, 3, 239}, {"Shortcuts Disabled", 15, 250, 249, 251}, {"Shortcuts Enabled", 15, 247, 246, 248}, {"B/O while moving", 15, 253, 252, 255} };
static const lj_ctl_button reset_btn[] = { {"Fixture Reset", 0, 212, 208, 217} };
static const lj_ctl_row extended_rows[] = {
    { LJ_W_RADIOGROUP_FADER, 14, 1, "Pan/Tilt Settings", .lo = 3, .hi = 245, .buttons = pt_radios, .nbuttons = 5 },
    { LJ_W_RADIOGROUP_FADER, 15, 1, "Effects Settings",  .lo = 3, .hi = 239, .buttons = fx_radios, .nbuttons = 5 },
    { LJ_W_LAMP_BTNS,        0,  1, .buttons = reset_btn, .nbuttons = 1 } };

static const lj_ctl_window windows[] = {
    { LJ_CTL_INTENSITY, "Intensity",    menu_std,  mac500_intensity_rows, 5, mac500_intensity_specials, 3 },
    { LJ_CTL_LAMP,      "Lamp Control", menu_help, lamp_rows, 3, NULL, 0 },
    { LJ_CTL_LEVELS,    "Levels",       menu_std,  levels_rows, 3, NULL, 0 },
    { LJ_CTL_EXTENDED,  "Extended",     menu_help, extended_rows, 3, NULL, 0 } };
static lj_profile prof;

/* ---- helpers ---- */

static lj_ctl_elem *elem(lj_ctl_win *c, int role, int nth)   /* the nth elem of that role */
{
    int i;
    for (i = 0; i < c->n; i++) if (c->e[i].role == role && nth-- == 0) return &c->e[i];
    return NULL;
}

static void click(lj_window *win, lj_widget *w, int button)   /* a press + release at the widget's centre, in surface coordinates */
{
    lj_ui_event ev;
    int ax, ay;
    lj_widget_abs(w, &ax, &ay);                                   /* already includes the root's origin inside the surface */
    memset(&ev, 0, sizeof ev);
    ev.u.mouse.x = ax + w->w / 2; ev.u.mouse.y = ay + w->h / 2;
    ev.u.mouse.button = button; ev.u.mouse.clicks = 1;
    ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; lj_window_event(win, &ev);
    ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP; lj_window_event(win, &ev);
}

static int confirm_answer, confirm_calls;
static char confirm_last[160];
static int confirm(const char *caption, const char *text, void *ud) { (void)ud; confirm_calls++; snprintf(confirm_last, sizeof confirm_last, "%s|%s", caption, text); return confirm_answer; }

static int never_fade(lj_window *win, lj_widget *osf)
{
    int i, ok = 1;
    for (i = 0; i < 6; i++) { click(win, osf, 1); if (lj_w_osf_state(osf) == LJ_OSF_FADE) ok = 0; }
    for (i = 0; i < 6; i++) { click(win, osf, 2); if (lj_w_osf_state(osf) == LJ_OSF_FADE) ok = 0; }
    return ok;
}

int lj_test_controls_a(int argc, char **argv);
int lj_test_controls_a(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    static lj_sequence seq;
    static lj_scene scene;
    lj_window *win, *lamp;
    lj_ctl_win *c, *cl;
    lj_ctl_elem *osf, *bar, *shutter, *e;
    const lj_surface *s;
    lj_rect cr;
    char text[64];
    int i;
    uint8_t v, st, st2;   /* ME-70: lj_prog_get writes a uint8_t; an int written through a cast fails under /RTC1 */
    (void)argc; (void)argv;
    if (!show) lj_fatal("test_controls_a: out of memory");

    /* the process context: three MAC-500-shaped fixtures at 1:001, 1:017, 1:033 and one all-off scene */
    for (i = 0; i < 3; i++) { show->fixture[i].kind = LJ_FX_BUILTIN; show->fixture[i].profile_id = 7; show->fixture[i].link = 1; show->fixture[i].address = (uint16_t)(1 + 16 * i); }
    memset(&seq, 0, sizeof seq); memset(&scene, 0, sizeof scene);
    seq.scenes = &scene; seq.n = 1;
    show->current_seq = &seq;
    ctx->show = show; ctx->current_scene = 0;
    memset(&prof, 0, sizeof prof);
    prof.id = 7; prof.footprint = 16; prof.windows = windows; prof.nwindows = LJ_COUNT_OF(windows);
    prof.channel_class[0] = LJ_CH_SHUTTER; prof.channel_class[1] = LJ_CH_INTENSITY; prof.channel_class[2] = LJ_CH_SWITCH;
    memcpy(prof.level_caption[2], "SW", 3);

    /* ---- lj_prog_*: the scene seam without a window ---- */
    CHECK(lj_prog_get(5, &v, &st) == 0 && v == 0 && st == LJ_OFF);
    lj_prog_set(5, 77, LJ_FADE);
    CHECK(lj_prog_get(5, &v, &st) == 0 && v == 77 && st == LJ_FADE && seq.modified);
    lj_prog_set(5, 78, LJ_PROG_KEEP);
    CHECK(scene.val[5] == 78 && scene.state[5] == LJ_FADE);
    lj_prog_set16(6, 7, 0x1234, LJ_SNAP);
    CHECK(scene.val[6] == 0x12 && scene.val[7] == 0x34 && scene.state[7] == LJ_SNAP);
    CHECK(lj_prog_get(LJ_CHANS, &v, &st) == -1);
    /* D10 + ME-27: left Off -> Snap -> Fade -> Off, right the reverse, mask skips */
    CHECK(lj_prog_cycle_state(9, 1, LJ_STATES_ALL) == LJ_SNAP && lj_prog_cycle_state(9, 1, LJ_STATES_ALL) == LJ_FADE && lj_prog_cycle_state(9, 1, LJ_STATES_ALL) == LJ_OFF);
    CHECK(lj_prog_cycle_state(9, -1, 0) == LJ_FADE && lj_prog_cycle_state(9, -1, 0) == LJ_SNAP && lj_prog_cycle_state(9, -1, 0) == LJ_OFF);
    for (i = 0, st = 0; i < 6; i++) { st2 = (uint8_t)lj_prog_cycle_state(9, i < 3 ? 1 : -1, LJ_STATES_OFF_SNAP); if (st2 == LJ_FADE) st = 1; }
    CHECK(st == 0 && scene.state[9] != LJ_FADE);
    CHECK(lj_prog_cycle_state(9, 1, 1u << LJ_FADE) == LJ_FADE);   /* the only allowed state wins */
    scene.state[9] = LJ_OFF;

    /* ---- the Intensity window: intensitycontrol.gif 480x98 is a 0.8 reduction (ME-67): 600x122 with the normal caption, rows at the 26-px pitch ---- */
    win = lj_ctl_open_table(LJ_CTL_INTENSITY, &windows[0], &prof);
    CHECK(win != NULL);
    if (!win) return 1;
    s = lj_window_surface(win);
    lj_window_client_rect(win, &cr);
    CHECK(s->w == 600 && s->h == 122);                             /* measured from intensitycontrol.gif, reduced x1.25 */
    CHECK(cr.w == 594 && cr.h == 78);
    CHECK(lj_window_menu(win) && lj_window_menu(win)->n == 4 && strcmp(lj_window_menu(win)->items[2].caption, "Fades") == 0);
    CHECK(strcmp(lj_window_caption(win)->text, "Intensity") == 0 && lj_window_caption(win)->caption_h == 19 && lj_window_desc(win)->style == LJ_FS_STAYONTOP);   /* normal caption (ME-67) */
    c = lj_ctl_of(win);
    CHECK(c && c->kind == LJ_CTL_INTENSITY && lj_ctl_window_get(LJ_CTL_INTENSITY) == win);
    osf = elem(c, LJ_CE_OSF, 0); shutter = elem(c, LJ_CE_SHUTTER, 0); bar = elem(c, LJ_CE_DRAGBAR, 1);
    CHECK(osf && osf->w->x == 0 && osf->w->y == 0 && osf->w->w == 48 && osf->w->h == 50);              /* Snap spans rows 1-2 (GIF y 33-71 x1.25; off.gif 48 wide) */
    CHECK(elem(c, LJ_CE_OSF, 1) && elem(c, LJ_CE_OSF, 1)->w->y == 52 && elem(c, LJ_CE_OSF, 1)->w->h == 24);   /* Fade on row 3 (GIF y 75-92 x1.25) */
    CHECK(shutter && shutter->w->x == 51 && shutter->w->y == 0 && shutter->w->w == 169 && shutter->lo == 0 && shutter->hi == 20);
    CHECK(elem(c, LJ_CE_DRAGBAR, 0) && elem(c, LJ_CE_DRAGBAR, 0)->w->y == 26 && elem(c, LJ_CE_DRAGBAR, 0)->reverse);   /* Strobe, reverse */
    CHECK(bar && bar->w->y == 52 && bar->w->x == 51 && bar->w->w == 169 && bar->row == &mac500_intensity_rows[4]);
    CHECK(elem(c, LJ_CE_NONE, 0) && elem(c, LJ_CE_NONE, 0)->w->x == 226 && elem(c, LJ_CE_NONE, 0)->special);   /* "Intensity Pulse" label at GIF x 183 x1.25 */
    CHECK(elem(c, LJ_CE_BUTTON, 0) && elem(c, LJ_CE_BUTTON, 0)->w->x == 306 && elem(c, LJ_CE_BUTTON, 8) && elem(c, LJ_CE_BUTTON, 8)->btn == &mac500_rstrobe[2]);
    CHECK(elem(c, LJ_CE_DRAGBAR, 2) && elem(c, LJ_CE_DRAGBAR, 2)->special && elem(c, LJ_CE_DRAGBAR, 2)->row == &mac500_intensity_specials[0] && elem(c, LJ_CE_DRAGBAR, 2)->lo == 80);   /* the Pulse speed bar */
    CHECK(osf->dflt_state == LJ_SNAP && bar->dflt_state == LJ_FADE && shutter->dflt_state == LJ_SNAP);
    lj_window_paint(win);
    CHECK(s->px[(lj_window_root(win)->y + 5) * s->w + lj_window_root(win)->x + 5] == LJ_CLR_RED);          /* the Off button face */
    CHECK(s->px[(lj_window_root(win)->y + 5) * s->w + lj_window_root(win)->x + 60] == 0x000000u);          /* the black shutter bar */

    /* ---- D6 / ME-52: the caption follows the host locale ---- */
    lj_w_dragbar_set_value(bar->w, 255);
    if (setlocale(LC_NUMERIC, "da-DK") || setlocale(LC_NUMERIC, "Danish") || setlocale(LC_NUMERIC, "da_DK.UTF-8") || setlocale(LC_NUMERIC, "de_DE.UTF-8")) {
        CHECK(lj_locale_decimal() == ',');
        CHECK(lj_w_dragbar_text(bar->w, text, sizeof text) == 0 && strcmp(text, "Intensity 100,0%") == 0);
        CHECK(lj_ctl_value_text(text, sizeof text, &mac500_intensity_rows[4], 255) == 0 && strcmp(text, "Intensity 100,0%") == 0);
        CHECK(lj_ctl_value_text(text, sizeof text, &mac500_intensity_rows[2], 50) == 0 && strcmp(text, "Strobe 100,0%") == 0);   /* reverse: 50 = fast = 100 % */
        printf("locale comma: %s\n", text);
        lj_w_dragbar_set_value(bar->w, 128);                        /* "Intensity 50,2%" (255 has no exact 50,0 %); the ME-52 screenshot */
        lj_window_paint(win);
        CHECK(lj_bmp_write(s, "controls_a_intensity_comma.bmp") == 0);
        printf("screenshot: controls_a_intensity_comma.bmp (600x122, comma locale) in the working directory\n");
        lj_w_dragbar_set_value(bar->w, 255);
    } else printf("SKIP: no comma locale on this host (setlocale failed), ME-52 comma evidence not produced here\n");
    setlocale(LC_NUMERIC, "C");
    CHECK(lj_locale_decimal() == '.');
    CHECK(lj_w_dragbar_text(bar->w, text, sizeof text) == 0 && strcmp(text, "Intensity 100.0%") == 0);
    CHECK(lj_ctl_value_text(text, sizeof text, &mac500_intensity_rows[2], 72) == 0 && strcmp(text, "Strobe 0.0%") == 0);
    CHECK(lj_ctl_value_text(text, sizeof text, &levels_rows[0], 128) == 0 && strcmp(text, "50%") == 0);
    CHECK(lj_ctl_value_text(text, 4, &mac500_intensity_rows[4], 255) == -1);

    /* ---- O/S/F: left click cycles Off -> Snap -> Fade -> Off, right click the reverse; the handler sees the button (W0-last-event) ---- */
    CHECK(lj_w_osf_state(osf->w) == LJ_OSF_OFF);
    click(win, osf->w, 1); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_SNAP && lj_window_last_event(win)->button == 1);
    click(win, osf->w, 1); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_FADE);
    click(win, osf->w, 1); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_OFF);
    click(win, osf->w, 2); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_FADE && lj_window_last_event(win)->button == 2 && lj_window_last_event(win)->type == LJ_EV_MOUSE_UP);
    click(win, osf->w, 2); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_SNAP);
    click(win, osf->w, 2); CHECK(lj_w_osf_state(osf->w) == LJ_OSF_OFF);

    /* ---- Preferences hides the specials and narrows the window (intensitycontroluser.gif 176x98) ---- */
    lj_ctl_intensity_prefs(c);
    lj_window_client_rect(win, &cr);
    CHECK(cr.w == 222 && !elem(c, LJ_CE_BUTTON, 0)->w->visible && osf->w->visible);
    lj_ctl_intensity_prefs(c);
    lj_window_client_rect(win, &cr);
    CHECK(cr.w == 594 && elem(c, LJ_CE_BUTTON, 0)->w->visible);

    /* ---- Lamp: Off/Snap only (ME-27), lampcontrol.gif 133x100 reduced x1.25 = 166x125 (ME-67), the note box ---- */
    lamp = lj_ctl_open_table(LJ_CTL_LAMP, &windows[1], &prof);
    CHECK(lamp != NULL);
    cl = lj_ctl_of(lamp);
    CHECK(cl && lj_window_surface(lamp)->w == 166 && lj_window_surface(lamp)->h == 125 && lj_window_menu(lamp)->n == 1);
    e = elem(cl, LJ_CE_OSF, 0);
    CHECK(e && never_fade(lamp, e->w));
    CHECK(elem(cl, LJ_CE_BUTTON, 0) && elem(cl, LJ_CE_BUTTON, 1) && elem(cl, LJ_CE_BUTTON, 1)->w->x > elem(cl, LJ_CE_BUTTON, 0)->w->x && elem(cl, LJ_CE_BUTTON, 1)->w->x + elem(cl, LJ_CE_BUTTON, 1)->w->w <= 160 && elem(cl, LJ_CE_BUTTON, 0)->w->w == 49 && elem(cl, LJ_CE_BUTTON, 0)->w->h == 25);   /* lampon.gif 49x25 */
    CHECK(elem(cl, LJ_CE_NONE, 0) && elem(cl, LJ_CE_NONE, 0)->w->color == 0xFFFFFFu && elem(cl, LJ_CE_NONE, 0)->w->y == 26);   /* the white note panel */
    CHECK(elem(cl, LJ_CE_NONE, 2) && strcmp(lj_w_caption(elem(cl, LJ_CE_NONE, 1)->w), "NOTE !") == 0 && strncmp(lj_w_caption(elem(cl, LJ_CE_NONE, 2)->w), "Lamp Off must be held", 21) == 0);   /* wrapped into lines */
    for (i = 1; elem(cl, LJ_CE_NONE, i); i++) printf("note line %d: %s\n", i, lj_w_caption(elem(cl, LJ_CE_NONE, i)->w));

    /* ---- Levels: the SWITCH column hides Fade (ME-25), a normal one reaches it; captions from the row or the profile ---- */
    {
        lj_window *lv = lj_ctl_open_table(LJ_CTL_LEVELS, &windows[2], &prof);
        lj_ctl_win *clv = lj_ctl_of(lv);
        CHECK(lv && clv && lj_window_surface(lv)->w == 272 && lj_window_surface(lv)->h == 250);   /* measured from levelcontrol.gif */
        CHECK(elem(clv, LJ_CE_FADER, 2) && elem(clv, LJ_CE_FADER, 2)->w->x == 64 && elem(clv, LJ_CE_OSF, 2) && elem(clv, LJ_CE_OSF, 2)->w->x == 64);
        CHECK(elem(clv, LJ_CE_OSF, 2) && never_fade(lv, elem(clv, LJ_CE_OSF, 2)->w));
        click(lv, elem(clv, LJ_CE_OSF, 0)->w, 2);
        CHECK(lj_w_osf_state(elem(clv, LJ_CE_OSF, 0)->w) == LJ_OSF_FADE);
        CHECK(elem(clv, LJ_CE_NONE, 2) && strcmp(lj_w_caption(elem(clv, LJ_CE_NONE, 2)->w), "SW") == 0 && strcmp(lj_w_caption(elem(clv, LJ_CE_NONE, 0)->w), "Yellow") == 0);
    }

    /* ---- Extended: two radio groups with their speed faders and the Fixture Reset button ---- */
    {
        lj_window *ex = lj_ctl_open_table(LJ_CTL_EXTENDED, &windows[3], &prof);
        lj_ctl_win *cx = lj_ctl_of(ex);
        CHECK(ex && cx && lj_window_surface(ex)->w == 352);                       /* extended.gif 277 reduced x1.25 = 346 client + 2 x 3 frame */
        CHECK(elem(cx, LJ_CE_RADIO, 4) && elem(cx, LJ_CE_RADIO, 5) && elem(cx, LJ_CE_RADIO, 9) && !elem(cx, LJ_CE_RADIO, 10));
        CHECK(elem(cx, LJ_CE_RADIO, 5)->w->parent != elem(cx, LJ_CE_RADIO, 4)->w->parent && elem(cx, LJ_CE_RADIO, 5)->w->parent->x == 174);
        CHECK(elem(cx, LJ_CE_FADER, 1) && elem(cx, LJ_CE_FADER, 1)->row == &extended_rows[1] && ((lj_w_fader *)elem(cx, LJ_CE_FADER, 1)->w)->max == 236);
        CHECK(elem(cx, LJ_CE_BUTTON, 0) && elem(cx, LJ_CE_BUTTON, 0)->w->y == 101 && strcmp(lj_w_caption(elem(cx, LJ_CE_BUTTON, 0)->w), "Fixture Reset") == 0);
    }

    /* ---- selection-wide writes: need the desktop's selection (a W1 track); the stub leaves no master ---- */
    lj_sel_set(0, 1, 1); lj_sel_set(1, 1, 0); lj_sel_set(2, 1, 0);
    if (lj_sel_master() == 0) {
        lj_ctl_set_confirm(confirm, NULL);
        memset(&scene, 0, sizeof scene);
        lj_prog_set_sel(1, 200, LJ_SNAP);
        CHECK(scene.val[1] == 200 && scene.val[17] == 200 && scene.val[33] == 200 && scene.state[33] == LJ_SNAP && scene.val[49] == 0);   /* 3 selected fixtures = 3 channels */
        lj_prog_set_sel16(11, 12, 0xABCD, LJ_FADE);
        CHECK(scene.val[11] == 0xAB && scene.val[28] == 0xCD && scene.state[44] == LJ_FADE);
        CHECK(lj_prog_master_ch(1) == 1);
        lj_ctl_repaint_all();
        CHECK(lj_w_dragbar_value(bar->w) == 200 && lj_w_osf_state(elem(c, LJ_CE_OSF, 1)->w) == LJ_OSF_SNAP);
        /* ME-27: the state lives on the scene channel; the Intensity and Lamp O/S/F of ch 1 repaint from it */
        click(win, osf->w, 1);
        CHECK(scene.state[0] == LJ_SNAP && scene.state[16] == LJ_SNAP && scene.state[32] == LJ_SNAP);
        CHECK(lj_w_osf_state(elem(cl, LJ_CE_OSF, 0)->w) == LJ_OSF_SNAP);
        click(win, osf->w, 1);
        CHECK(scene.state[0] == LJ_FADE && lj_w_osf_state(osf->w) == LJ_OSF_FADE);
        click(lamp, elem(cl, LJ_CE_OSF, 0)->w, 1);                   /* Lamp: Fade -> Off (Fade is masked there, so the next is Off) */
        CHECK(scene.state[0] == LJ_OFF);
        lj_ctl_refresh();
        CHECK(lj_w_osf_state(osf->w) == LJ_OSF_OFF && lj_w_osf_state(elem(cl, LJ_CE_OSF, 0)->w) == LJ_OSF_OFF);
        /* the shutter toggle: closed 0 <-> open 20, an Off channel is armed to the row's default state */
        click(win, shutter->w, 1);
        CHECK(scene.val[0] == 20 && scene.state[0] == LJ_SNAP && scene.val[16] == 20 && strcmp(lj_w_caption(shutter->w), "Shutter Open") == 0);
        click(win, shutter->w, 1);
        CHECK(scene.val[0] == 0 && strcmp(lj_w_caption(shutter->w), "Shutter Closed") == 0);
        /* Lamp Off asks first; "no" leaves the scene alone, "yes" writes 251 */
        confirm_answer = 0; confirm_calls = 0;
        click(lamp, elem(cl, LJ_CE_BUTTON, 1)->w, 1);
        CHECK(confirm_calls == 1 && scene.val[0] == 0 && strcmp(confirm_last, "Confirm|" LJ_CTL_LAMP_OFF_TEXT) == 0);
        confirm_answer = 1;
        click(lamp, elem(cl, LJ_CE_BUTTON, 1)->w, 1);
        CHECK(confirm_calls == 2 && scene.val[0] == 251 && scene.val[32] == 251);
        click(lamp, elem(cl, LJ_CE_BUTTON, 0)->w, 1);
        CHECK(confirm_calls == 2 && scene.val[0] == 232);
        /* a special: Strobe Fast = 128 on ch 1, every selected fixture */
        click(win, elem(c, LJ_CE_BUTTON, 8)->w, 1);
        CHECK(scene.val[0] == 128 && scene.val[16] == 128 && scene.val[32] == 128);
        lj_ctl_set_confirm(NULL, NULL);
        printf("selection-wide writes: exercised on 3 fixtures\n");
    } else printf("SKIP: desktop selection is the W1 stub (lj_sel_master() < 0): selection-wide writes not exercised here\n");

    lj_ctl_close_all();
    CHECK(lj_ctl_window_get(LJ_CTL_INTENSITY) == NULL && lj_ctl_window_get(LJ_CTL_LAMP) == NULL && lj_ctl_of(win) == NULL);
    ctx->show = NULL;
    free(show);
    printf("test_controls_a: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
