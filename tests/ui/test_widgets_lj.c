/* test_widgets_lj.c - the runnable check for src/ui/widgets/lj_w_*.c of the LJ group:
 * each class built from a hand-written lj_ctl_desc, painted into a surface, key
 * pixels asserted (bevel colours at the corners, face, text present by non-face
 * count), then driven by mouse/key events and its state asserted (updown steps
 * and writes its associate, O/S/F cycles both ways and honours states_allowed,
 * drag bar arrows/min-max/drag, fader drag, dip switch bits, LED, palette). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_locale.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_lj.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int ev_count[LJ_WEV_COUNT];
static void on_ev(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev >= 0 && ev < LJ_WEV_COUNT) ev_count[ev]++; }
static int took(int ev) { int n = ev_count[ev]; ev_count[ev] = 0; return n; }

static uint32_t px(const lj_surface *s, int x, int y) { return s->px[y * s->w + x]; }
static int non(const lj_surface *s, int x, int y, int w, int h, uint32_t rgb)
{
    int i, j, n = 0;
    for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) if (px(s, i, j) != rgb) n++;
    return n;
}

static void mouse(lj_widget *w, int type, int x, int y, int button, int wheel)
{
    lj_mouse_ev m;
    memset(&m, 0, sizeof m);
    m.type = type; m.x = x; m.y = y; m.button = button; m.clicks = 1; m.wheel = wheel;
    if (w->vt->mouse) w->vt->mouse(w, &m);
}
static void click(lj_widget *w, int x, int y, int button) { mouse(w, LJ_EV_MOUSE_DOWN, x, y, button, 0); mouse(w, LJ_EV_MOUSE_UP, x, y, button, 0); }
static void key(lj_widget *w, int type, int vk, int ch)
{
    lj_key_ev k;
    memset(&k, 0, sizeof k);
    k.type = type; k.key = vk; k.ch = ch;
    if (w->vt->key) w->vt->key(w, &k);
}
static void paint(lj_widget *w, lj_surface *s) { lj_fill(s, 0, 0, s->w, s->h, LJ_CLR_BTNFACE); w->vt->paint(w, s); }

static char assoc_text[16];
static int assoc_set(lj_widget *w, const char *text) { (void)w; return lj_str_copy(assoc_text, sizeof assoc_text, text); }

static lj_widget *mk(lj_widget *parent, lj_widget *(*ctor)(lj_widget *, const lj_ctl_desc *), const lj_ctl_desc *d)
{
    lj_widget *w = ctor(parent, d);
    CHECK(w != NULL);
    if (w) w->on_event = on_ev;
    return w;
}

int lj_test_widgets_lj(int argc, char **argv);
int lj_test_widgets_lj(int argc, char **argv)
{
    lj_widget root, assoc;
    lj_surface s;
    lj_widget *w;
    lj_widget_ctor tab[LJ_K_COUNT];
    char text[64], want[64];
    (void)argc; (void)argv;

    memset(&root, 0, sizeof root); root.w = 200; root.h = 200; root.visible = 1; root.enabled = 1; root.color = LJ_CLR_BTNFACE;
    memset(&assoc, 0, sizeof assoc);
    CHECK(lj_surface_init(&s, 200, 200) == 0);

    /* registration: only the four DFM classes of this group */
    memset(tab, 0, sizeof tab);
    CHECK(lj_widgets_register_lj(tab, LJ_K_COUNT) == 0 && lj_widgets_register_lj(tab, 3) == -1);
    CHECK(tab[LJ_K_UPDOWN] == lj_w_updown_create && tab[LJ_K_UDBUTTON] == lj_w_udbutton_create);
    CHECK(tab[LJ_K_SPINEDIT] == lj_w_spinedit_create && tab[LJ_K_DIPSWITCH] == lj_w_dipswitch_create && tab[LJ_K_LABEL] == NULL);

    /* TUpDown udCDTrack (TCUELISTEDITFORM-style row): Min 1 Max 99 Position 1 Wrap False */
    {
        static const lj_ctl_desc d = { LJ_K_UPDOWN, "udCDTrack", 96, 19, 15, 21, .min = 1, .max = 99, .position = 1, .wrap = LJ_NO, .associate = "eCDTrackNo" };
        w = mk(&root, tab[LJ_K_UPDOWN], &d);
        CHECK(w->x == 96 && w->w == 15 && w->tabstop && lj_w_updown_position(w) == 1);
        paint(w, &s);
        CHECK(px(&s, 96, 19) == LJ_CLR_BTNHIGHLIGHT && px(&s, 110, 28) == LJ_CLR_3DDKSHADOW);       /* top half 15x10: white TL, black BR */
        CHECK(px(&s, 96, 29) == LJ_CLR_BTNHIGHLIGHT && px(&s, 110, 39) == LJ_CLR_3DDKSHADOW);       /* bottom half 15x11 */
        CHECK(px(&s, 103, 23) == LJ_CLR_3DDKSHADOW && px(&s, 103, 34) == LJ_CLR_3DDKSHADOW);        /* the two arrow apexes (5x3 glyphs) */
        lj_w_updown_bind(w, &assoc, assoc_set);
        CHECK(strcmp(assoc_text, "1") == 0);
        click(w, 7, 3, 1);
        CHECK(lj_w_updown_position(w) == 2 && lj_w_updown_dir(w) == 1 && took(LJ_WEV_CLICK) == 1 && strcmp(assoc_text, "2") == 0);
        click(w, 7, 15, 1);
        click(w, 7, 15, 1);
        CHECK(lj_w_updown_position(w) == 1 && took(LJ_WEV_CLICK) == 1 && strcmp(assoc_text, "1") == 0);   /* no wrap at Min */
        ((lj_w_updown *)w)->wrap = 1;
        click(w, 7, 15, 1);
        CHECK(lj_w_updown_position(w) == 99 && strcmp(assoc_text, "99") == 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_UP, 0);
        CHECK(lj_w_updown_position(w) == 1);
        mouse(w, LJ_EV_MOUSE_WHEEL, 7, 7, 0, 1);
        CHECK(lj_w_updown_position(w) == 2);
        mouse(w, LJ_EV_MOUSE_DOWN, 7, 3, 1, 0);
        paint(w, &s);
        CHECK(px(&s, 96, 19) == LJ_CLR_BTNSHADOW && ((lj_w_updown *)w)->pressed == 1);              /* pressed half: sunken edge */
        mouse(w, LJ_EV_MOUSE_UP, 7, 3, 1, 0);
        CHECK(lj_w_updown_position(w) == 3 && took(LJ_WEV_CLICK) == 4);                              /* the wrap click, the key, the wheel notch and the press */
        CHECK(lj_w_updown_set_position(w, 500) == 0 && lj_w_updown_position(w) == 99 && took(LJ_WEV_CLICK) == 0);
        w->enabled = 0;
        click(w, 7, 3, 1);
        CHECK(lj_w_updown_position(w) == 99 && took(LJ_WEV_CLICK) == 0);
        paint(w, &s);
        CHECK(px(&s, 104, 26) == LJ_CLR_BTNHIGHLIGHT && px(&s, 103, 23) == LJ_CLR_BTNSHADOW);         /* disabled glyph: #808080 over its white +1,+1 shadow */
        took(LJ_WEV_MOUSE_DOWN); took(LJ_WEV_MOUSE_UP);
        lj_widget_remove(w); free(w);
    }

    /* TUdButton UdScene 20x21: a pulse with a direction */
    {
        static const lj_ctl_desc d = { LJ_K_UDBUTTON, "UdScene", 137, 37, 20, 21, .enabled = LJ_YES };
        w = mk(&root, tab[LJ_K_UDBUTTON], &d);
        CHECK(!w->tabstop);
        paint(w, &s);
        CHECK(px(&s, 137, 37) == LJ_CLR_BTNHIGHLIGHT && px(&s, 156, 57) == LJ_CLR_3DDKSHADOW && px(&s, 155, 56) == LJ_CLR_BTNSHADOW);
        click(w, 10, 3, 1);
        CHECK(lj_w_udbutton_delta(w) == 1 && took(LJ_WEV_CHANGE) == 1);
        click(w, 10, 15, 1);
        CHECK(lj_w_udbutton_delta(w) == -1 && took(LJ_WEV_CHANGE) == 1);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_UP, 0);
        CHECK(lj_w_udbutton_delta(w) == 1 && took(LJ_WEV_CHANGE) == 1);
        lj_widget_remove(w); free(w);
    }

    /* TSpinEdit sevalue 49x22: 0..255, MaxLength 3 */
    {
        static const lj_ctl_desc d = { LJ_K_SPINEDIT, "sevalue", 3, 153, 49, 22, .max_length = 3, .max_value = 255, .min_value = 0, .value = 0 };
        w = mk(&root, tab[LJ_K_SPINEDIT], &d);
        paint(w, &s);
        CHECK(px(&s, 3, 153) == LJ_CLR_BTNSHADOW && px(&s, 4, 154) == LJ_CLR_3DDKSHADOW && px(&s, 51, 174) == LJ_CLR_BTNHIGHLIGHT);   /* sunken edit */
        CHECK(px(&s, 25, 163) == LJ_CLR_BTNHIGHLIGHT && non(&s, 6, 156, 8, 16, LJ_CLR_BTNHIGHLIGHT) > 0);                           /* white interior, "0" drawn */
        CHECK(px(&s, 35, 155) == LJ_CLR_BTNHIGHLIGHT && px(&s, 49, 164) == LJ_CLR_3DDKSHADOW);                                     /* spin pair at x 35..49, y 155..172 */
        click(w, 37, 5, 1);                                          /* widget-relative: the upper spin button */
        CHECK(lj_w_spinedit_value(w) == 1 && took(LJ_WEV_CHANGE) == 1 && w->focused);
        click(w, 37, 15, 1);
        CHECK(lj_w_spinedit_value(w) == 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_UP, 0);
        CHECK(lj_w_spinedit_value(w) == 1);
        key(w, LJ_EV_CHAR, 0, '5');
        key(w, LJ_EV_CHAR, 0, '9');
        CHECK(lj_w_spinedit_value(w) == 159);
        key(w, LJ_EV_CHAR, 0, '1');
        CHECK(lj_w_spinedit_value(w) == 159);                        /* MaxLength 3 */
        key(w, LJ_EV_KEY_DOWN, LJ_VK_BACK, 0);
        CHECK(lj_w_spinedit_value(w) == 15);
        mouse(w, LJ_EV_MOUSE_WHEEL, 10, 10, 0, 1);
        CHECK(lj_w_spinedit_value(w) == 16);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_END, 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_UP, 0);
        CHECK(lj_w_spinedit_value(w) == 255);
        key(w, LJ_EV_CHAR, 0, '7');
        CHECK(lj_w_spinedit_value(w) == 255);                        /* clamped at MaxValue */
        took(LJ_WEV_CHANGE);
        lj_widget_remove(w); free(w);
    }

    /* TDipSwitch 62x24 at address 123 = 0001111011: set bits throw the switch down */
    {
        static const lj_ctl_desc d = { LJ_K_DIPSWITCH, "DipSwitch1", 10, 10, 62, 24, .value = 123, .kind_str = "TDip10",
                                       .base_color = LJ_CLR(0x0000FF), .dip_color = LJ_CLR(0xFFFFFF), .text_color = LJ_CLR(0xFFFFFF) };
        w = mk(&root, tab[LJ_K_DIPSWITCH], &d);
        paint(w, &s);
        CHECK(px(&s, 10, 10) == LJ_CLR_3DDKSHADOW && px(&s, 71, 33) == LJ_CLR_3DDKSHADOW && px(&s, 11, 11) == 0x0000FFu);
        CHECK(px(&s, 18, 14) == LJ_CLR_3DDKSHADOW && px(&s, 18, 23) == LJ_CLR_BTNHIGHLIGHT);     /* switch 1 (bit 0 set): knob at the bottom */
        CHECK(px(&s, 28, 14) == LJ_CLR_BTNHIGHLIGHT && px(&s, 28, 23) == LJ_CLR_3DDKSHADOW);     /* switch 3 (bit 2 clear): knob at the top */
        CHECK(px(&s, 14, 20) == LJ_CLR_BTNHIGHLIGHT && non(&s, 16, 26, 50, 5, 0x0000FFu) > 40);   /* arrow and digits */
        click(w, 19, 8, 1);                                                                      /* switch 3 */
        CHECK(lj_w_dipswitch_value(w) == 127 && took(LJ_WEV_CHANGE) == 1);
        click(w, 9, 8, 1);
        CHECK(lj_w_dipswitch_value(w) == 126 && took(LJ_WEV_CHANGE) == 1);
        click(w, 3, 8, 1);                                                                       /* the arrow, not a switch */
        CHECK(lj_w_dipswitch_value(w) == 126 && took(LJ_WEV_CHANGE) == 0);
        CHECK(lj_w_dipswitch_set_value(w, 0x7FFF) == 0 && lj_w_dipswitch_value(w) == 1023);
        lj_widget_remove(w); free(w);
    }

    /* O/S/F 48x24: red Off, left click forward, right click back (D10), states_allowed (ME-27) */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "osf", 0, 0, 48, 24 };
        w = mk(&root, lj_w_osf_create, &d);
        CHECK(lj_w_osf_state(w) == LJ_OSF_OFF);
        paint(w, &s);
        CHECK(px(&s, 0, 0) == LJ_CLR_BTNHIGHLIGHT && px(&s, 47, 23) == LJ_CLR_3DDKSHADOW && px(&s, 46, 22) == LJ_CLR_BTNSHADOW);
        CHECK(px(&s, 24, 3) == LJ_CLR_RED && non(&s, 14, 6, 20, 12, LJ_CLR_RED) > 10);              /* face and the "Off" caption */
        click(w, 10, 10, 1);
        CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP && took(LJ_WEV_CLICK) == 1);
        paint(w, &s);
        CHECK(px(&s, 24, 3) == LJ_CLR_YELLOW);
        click(w, 10, 10, 1);
        CHECK(lj_w_osf_state(w) == LJ_OSF_FADE);
        paint(w, &s);
        CHECK(px(&s, 24, 3) == LJ_CLR_GREEN);
        click(w, 10, 10, 1);
        CHECK(lj_w_osf_state(w) == LJ_OSF_OFF);
        click(w, 10, 10, 2);
        CHECK(lj_w_osf_state(w) == LJ_OSF_FADE);
        click(w, 10, 10, 2);
        CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP && took(LJ_WEV_CLICK) == 4);
        mouse(w, LJ_EV_MOUSE_DOWN, 10, 10, 1, 0);
        paint(w, &s);
        CHECK(px(&s, 0, 0) == LJ_CLR_BTNSHADOW && px(&s, 1, 1) == LJ_CLR_3DDKSHADOW);               /* pressed: sunken edge */
        mouse(w, LJ_EV_MOUSE_UP, 60, 10, 1, 0);                                                      /* released outside: no cycle */
        CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP && took(LJ_WEV_CLICK) == 0);
        lj_w_osf_set_allowed(w, LJ_OSF_ALLOW_OFF | LJ_OSF_ALLOW_SNAP);
        click(w, 10, 10, 1); CHECK(lj_w_osf_state(w) == LJ_OSF_OFF);
        click(w, 10, 10, 1); CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP);
        click(w, 10, 10, 1); CHECK(lj_w_osf_state(w) == LJ_OSF_OFF);                                 /* a Lamp window never reaches Fade */
        click(w, 10, 10, 2); CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP);
        CHECK(lj_w_osf_set_state(w, LJ_OSF_FADE) == -1 && lj_w_osf_set_state(w, LJ_OSF_OFF) == 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_SPACE, 0);
        CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP);
        w->enabled = 0;
        paint(w, &s);
        CHECK(non(&s, 14, 6, 20, 12, LJ_CLR_YELLOW) > 10 && px(&s, 24, 3) == LJ_CLR_YELLOW);
        click(w, 10, 10, 1);
        CHECK(lj_w_osf_state(w) == LJ_OSF_SNAP);
        took(LJ_WEV_CLICK); took(LJ_WEV_MOUSE_DOWN); took(LJ_WEV_MOUSE_UP);
        lj_widget_remove(w); free(w);
    }

    /* drag bar 169x23 "Intensity", 128/255 -> "Intensity 50,2%" in the host locale (D6) */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "bar", 0, 30, 169, 23, .caption = "Intensity", .position = 128, .max = 255 };
        w = mk(&root, lj_w_dragbar_create, &d);
        snprintf(want, sizeof want, "Intensity 50%c2%%", lj_locale_decimal());
        CHECK(lj_w_dragbar_text(w, text, sizeof text) == 0 && strcmp(text, want) == 0);
        paint(w, &s);
        CHECK(px(&s, 0, 30) == LJ_CLR_BTNHIGHLIGHT && px(&s, 168, 52) == LJ_CLR_BTNSHADOW);         /* raised panel */
        CHECK(px(&s, 26, 41) == LJ_CLR_BTNSHADOW && px(&s, 27, 41) == LJ_CLR_3DDKSHADOW && px(&s, 142, 41) == LJ_CLR_BTNHIGHLIGHT);   /* sunken bar */
        CHECK(px(&s, 30, 33) == LJ_CLR_BTNHIGHLIGHT && px(&s, 128, 33) == LJ_CLR_BTNSHADOW);       /* fill 58 of 115 px, rest #808080 */
        CHECK(non(&s, 60, 36, 50, 12, LJ_CLR_BTNHIGHLIGHT) > 20);                                   /* centred caption */
        CHECK(px(&s, 10, 41) == LJ_CLR_3DDKSHADOW && px(&s, 158, 41) == LJ_CLR_3DDKSHADOW);          /* arrow apexes */
        click(w, 160, 11, 1);
        CHECK(lj_w_dragbar_value(w) == 129 && took(LJ_WEV_CHANGE) == 1);
        click(w, 160, 11, 2);
        CHECK(lj_w_dragbar_value(w) == 255);
        click(w, 8, 11, 2);
        CHECK(lj_w_dragbar_value(w) == 0);
        click(w, 8, 11, 1);
        CHECK(lj_w_dragbar_value(w) == 0 && took(LJ_WEV_CHANGE) == 2);
        mouse(w, LJ_EV_MOUSE_DOWN, 28 + 57, 11, 1, 0);
        CHECK(lj_w_dragbar_value(w) == 128);
        mouse(w, LJ_EV_MOUSE_MOVE, 28 + 114, 11, 0, 0);
        CHECK(lj_w_dragbar_value(w) == 255);
        mouse(w, LJ_EV_MOUSE_MOVE, -40, 11, 0, 0);
        CHECK(lj_w_dragbar_value(w) == 0);
        mouse(w, LJ_EV_MOUSE_UP, 28, 11, 1, 0);
        mouse(w, LJ_EV_MOUSE_MOVE, 100, 11, 0, 0);
        CHECK(lj_w_dragbar_value(w) == 0);                                                           /* no drag after release */
        key(w, LJ_EV_KEY_DOWN, LJ_VK_RIGHT, 0);
        CHECK(lj_w_dragbar_value(w) == 1);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_END, 0);
        CHECK(lj_w_dragbar_value(w) == 255);
        mouse(w, LJ_EV_MOUSE_WHEEL, 80, 11, 0, -1);
        CHECK(lj_w_dragbar_value(w) == 254);
        snprintf(want, sizeof want, "99%c6%%", lj_locale_decimal());
        ((lj_w_dragbar *)w)->caption = NULL;
        CHECK(lj_w_dragbar_text(w, text, sizeof text) == 0 && strcmp(text, want) == 0);
        lj_w_dragbar_set_fill(w, 0x00FFFF);
        paint(w, &s);
        CHECK(px(&s, 30, 33) == 0x00FFFFu);
        took(LJ_WEV_CHANGE);
        lj_widget_remove(w); free(w);
    }

    /* scroll control 132x33 */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "scroll", 0, 60, 132, 33, .position = 100, .max = 255, .step = 16 };
        lj_surface tile;
        w = mk(&root, lj_w_scrollctl_create, &d);
        paint(w, &s);
        CHECK(px(&s, 0, 60) == LJ_CLR_BTNHIGHLIGHT && px(&s, 131, 92) == LJ_CLR_3DDKSHADOW);
        CHECK(px(&s, 2, 65) == LJ_CLR_BTNHIGHLIGHT && px(&s, 24, 88) == LJ_CLR_3DDKSHADOW);          /* button 1 at (2,5) 23x24 */
        CHECK(px(&s, 49, 63) == LJ_CLR_3DDKSHADOW && px(&s, 66, 77) == LJ_CLR_BTNHIGHLIGHT);         /* well frame, white swatch */
        CHECK(non(&s, 4, 70, 19, 14, LJ_CLR_BTNFACE) > 5 && non(&s, 27, 70, 19, 14, LJ_CLR_BTNFACE) > 5);   /* glyphs */
        click(w, 112, 10, 1);
        CHECK(lj_w_scrollctl_value(w) == 116 && lj_w_scrollctl_last(w) == 2 && took(LJ_WEV_CHANGE) == 1);
        click(w, 30, 10, 1);
        CHECK(lj_w_scrollctl_value(w) == 115 && lj_w_scrollctl_last(w) == -1);
        click(w, 10, 10, 1);
        CHECK(lj_w_scrollctl_value(w) == 99 && lj_w_scrollctl_last(w) == -2);
        click(w, 90, 10, 1);
        CHECK(lj_w_scrollctl_value(w) == 100 && lj_w_scrollctl_last(w) == 1);
        mouse(w, LJ_EV_MOUSE_DOWN, 112, 10, 1, 0);
        mouse(w, LJ_EV_MOUSE_UP, 60, 10, 1, 0);                                                      /* released off the button */
        CHECK(lj_w_scrollctl_value(w) == 100);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_RIGHT, 0);
        CHECK(lj_w_scrollctl_value(w) == 101);
        CHECK(lj_surface_init(&tile, 31, 30) == 0);
        lj_fill(&tile, 0, 0, 31, 30, 0xFF00FF);
        lj_w_scrollctl_set_tile(w, &tile, 0);
        paint(w, &s);
        CHECK(px(&s, 55, 67) == 0xFF00FFu && px(&s, 49, 63) == LJ_CLR_3DDKSHADOW);
        lj_surface_free(&tile);
        took(LJ_WEV_CHANGE);
        lj_widget_remove(w); free(w);
    }

    /* palette tile 31x30, cyan swatch */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "pal", 0, 100, 31, 30, .tag = 4, .color = LJ_CLR(0x00FFFF) };
        w = mk(&root, lj_w_palette_create, &d);
        CHECK(((lj_w_palette *)w)->index == 4);
        paint(w, &s);
        CHECK(px(&s, 0, 100) == LJ_CLR_BTNSHADOW && px(&s, 30, 129) == LJ_CLR_BTNSHADOW && px(&s, 1, 101) == 0x00FFFFu);
        click(w, 10, 10, 1);
        CHECK(lj_w_palette_selected(w) && took(LJ_WEV_CLICK) == 1);
        paint(w, &s);
        CHECK(px(&s, 0, 100) == LJ_CLR_3DDKSHADOW && px(&s, 1, 101) == LJ_CLR_3DDKSHADOW && px(&s, 2, 102) == 0x00FFFFu && px(&s, 29, 128) == LJ_CLR_3DDKSHADOW);
        lj_w_palette_select(w, 0);
        CHECK(!lj_w_palette_selected(w));
        lj_widget_remove(w); free(w);
    }

    /* vertical fader 32x160 (128-px track): blue covers from the top down to the value */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "fader", 40, 0, 32, 160, .position = 0, .max = 255 };
        w = mk(&root, lj_w_fader_create, &d);
        paint(w, &s);
        CHECK(px(&s, 40, 0) == LJ_CLR_BTNHIGHLIGHT && px(&s, 71, 159) == LJ_CLR_3DDKSHADOW);
        CHECK(px(&s, 48, 16) == 0x0000FFu && px(&s, 48, 143) == 0x0000FFu);                         /* 0 %: all blue */
        CHECK(px(&s, 55, 6) == LJ_CLR_3DDKSHADOW && px(&s, 55, 153) == LJ_CLR_3DDKSHADOW);           /* arrow glyphs */
        CHECK(non(&s, 44, 127, 24, 13, 0x0000FFu) > 10);                                            /* "0%" in white at the foot */
        CHECK(lj_w_fader_set_value(w, 255) == 0);
        paint(w, &s);
        CHECK(px(&s, 48, 16) == LJ_CLR_BTNHIGHLIGHT && px(&s, 44, 143) == LJ_CLR_3DDKSHADOW && px(&s, 44, 80) == 0x7F7F7Fu);   /* white-to-black gradient */
        lj_w_fader_set_value(w, 128);
        paint(w, &s);
        CHECK(px(&s, 48, 16 + 63) == 0x0000FFu && px(&s, 48, 16 + 64) != 0x0000FFu);              /* covered = 128 - 64 rows */
        click(w, 15, 5, 1);
        CHECK(lj_w_fader_value(w) == 129 && took(LJ_WEV_CHANGE) == 1);
        click(w, 15, 150, 1);
        CHECK(lj_w_fader_value(w) == 128);
        mouse(w, LJ_EV_MOUSE_DOWN, 15, 16 + 127, 1, 0);
        CHECK(lj_w_fader_value(w) == 0);
        mouse(w, LJ_EV_MOUSE_MOVE, 15, 16, 0, 0);
        CHECK(lj_w_fader_value(w) == 255);
        mouse(w, LJ_EV_MOUSE_MOVE, 15, 16 + 63, 0, 0);
        CHECK(lj_w_fader_value(w) == 129);
        mouse(w, LJ_EV_MOUSE_UP, 15, 16 + 63, 1, 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_PRIOR, 0);
        CHECK(lj_w_fader_value(w) == 145);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_END, 0);
        CHECK(lj_w_fader_value(w) == 0);
        mouse(w, LJ_EV_MOUSE_WHEEL, 15, 50, 0, 1);
        CHECK(lj_w_fader_value(w) == 1);
        took(LJ_WEV_CHANGE);
        lj_widget_remove(w); free(w);
    }

    /* LED button 109x24 "&Background Cue": green on, red #A0160A off */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "led", 0, 130, 109, 24, .caption = "&Background Cue", .checked = LJ_YES };
        w = mk(&root, lj_w_led_create, &d);
        CHECK(lj_w_led_on(w));
        paint(w, &s);
        CHECK(px(&s, 0, 130) == LJ_CLR_BTNHIGHLIGHT && px(&s, 108, 153) == LJ_CLR_3DDKSHADOW);
        CHECK(px(&s, 13, 142) == LJ_LED_ON_RGB && px(&s, 7, 142) == LJ_CLR_3DDKSHADOW && px(&s, 19, 142) == LJ_CLR_3DDKSHADOW);
        CHECK(px(&s, 13, 136) == LJ_CLR_3DDKSHADOW && px(&s, 13, 148) == LJ_CLR_3DDKSHADOW && px(&s, 6, 142) == LJ_CLR_BTNFACE);
        CHECK(non(&s, 25, 134, 70, 14, LJ_CLR_BTNFACE) > 50);                                       /* caption */
        lj_w_led_set(w, 0);
        paint(w, &s);
        CHECK(px(&s, 13, 142) == LJ_LED_OFF_RGB);
        click(w, 50, 10, 1);
        CHECK(took(LJ_WEV_CLICK) == 1 && took(LJ_WEV_MOUSE_DOWN) == 1 && took(LJ_WEV_MOUSE_UP) == 1);
        mouse(w, LJ_EV_MOUSE_DOWN, 50, 10, 1, 0);
        paint(w, &s);
        CHECK(px(&s, 0, 130) == LJ_CLR_BTNSHADOW && px(&s, 14, 143) == LJ_LED_OFF_RGB);              /* pressed: sunken, dot +1,+1 */
        mouse(w, LJ_EV_MOUSE_UP, 200, 10, 1, 0);
        CHECK(took(LJ_WEV_CLICK) == 0);
        key(w, LJ_EV_KEY_DOWN, LJ_VK_SPACE, 0);
        CHECK(took(LJ_WEV_CLICK) == 1);
        took(LJ_WEV_MOUSE_DOWN); took(LJ_WEV_MOUSE_UP);
        lj_widget_remove(w); free(w);
    }

    /* fixture icon tile 63x53 "(5) MAC 250KR" */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "icon", 70, 100, 63, 53, .tag = 5, .caption = "MAC 250KR" };
        lj_surface bmp;
        w = mk(&root, lj_w_fixicon_create, &d);
        paint(w, &s);
        CHECK(px(&s, 70, 100) == LJ_CLR_BTNHIGHLIGHT && px(&s, 71, 100) == 0x0000FFu && px(&s, 130, 114) == 0x0000FFu && px(&s, 132, 152) == LJ_CLR_3DDKSHADOW);
        CHECK(px(&s, 100, 130) == LJ_CLR_BTNFACE && non(&s, 72, 101, 50, 13, 0x0000FFu) > 30);      /* body, caption text */
        CHECK(lj_surface_init(&bmp, 20, 20) == 0);
        lj_fill(&bmp, 0, 0, 20, 20, 0x123456);
        lj_fill(&bmp, 0, 19, 1, 1, 0xC0C0C0);                                                    /* bottom-left pixel = the transparent key */
        lj_w_fixicon_set(w, LJ_FIX_MASTER, LJ_FIX_PROG_SCENE, &bmp);
        paint(w, &s);
        CHECK(px(&s, 71, 100) == LJ_CLR_RED && px(&s, 100, 130) == 0x123456u && px(&s, 74, 119) == LJ_CLR_GREEN);
        CHECK(px(&s, 91, 142) == LJ_CLR_BTNFACE && px(&s, 92, 142) == 0x123456u);                 /* the key pixel is not painted */
        lj_w_fixicon_set(w, LJ_FIX_SLAVE, LJ_FIX_PROG_SEQ, NULL);
        paint(w, &s);
        CHECK(px(&s, 71, 100) == 0x800000u && px(&s, 74, 119) == 0x00FFFFu && px(&s, 100, 130) == LJ_CLR_BTNFACE);
        lj_surface_free(&bmp);
        lj_widget_remove(w); free(w);
    }

    /* cue grid row 160x16: No | Sequence at C0/C1 widths, navy when selected */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "row", 0, 160, 160, 16 };
        static const char *const cells[2] = { "0002", "Color Chase 1" };
        static const int widths[2] = { 30, 160 };
        w = mk(&root, lj_w_cuerow_create, &d);
        lj_w_cuerow_set(w, cells, widths, 2, 0x800000, 0);
        paint(w, &s);
        CHECK(px(&s, 0, 160) == LJ_CLR_BTNHIGHLIGHT && non(&s, 2, 161, 26, 13, LJ_CLR_BTNHIGHLIGHT) > 10 && non(&s, 32, 161, 60, 13, LJ_CLR_BTNHIGHLIGHT) > 10);
        CHECK(px(&s, 26, 170) == LJ_CLR_BTNHIGHLIGHT || px(&s, 26, 170) == 0x800000u);
        lj_w_cuerow_set(w, cells, widths, 2, 0x800000, 1);
        paint(w, &s);
        CHECK(px(&s, 1, 160) == LJ_CLR_HIGHLIGHT && non(&s, 2, 161, 26, 13, LJ_CLR_HIGHLIGHT) > 10 && px(&s, 159, 175) == LJ_CLR_HIGHLIGHT);
        lj_widget_remove(w); free(w);
    }

    /* statics slot button 100x18 */
    {
        static const lj_ctl_desc d = { LJ_K_UNKNOWN, "slot", 100, 180, 100, 18, .caption = "01 Center Strobes" };
        w = mk(&root, lj_w_slotbtn_create, &d);
        paint(w, &s);
        CHECK(px(&s, 100, 180) == LJ_CLR_BTNHIGHLIGHT && px(&s, 199, 197) == LJ_CLR_3DDKSHADOW && non(&s, 104, 182, 60, 13, LJ_CLR_BTNFACE) > 30);
        lj_w_slotbtn_set(w, "02 Back strobes", 1);
        paint(w, &s);
        CHECK(px(&s, 100, 180) == LJ_CLR_BTNSHADOW && px(&s, 101, 181) == LJ_CLR_3DDKSHADOW);
        lj_widget_remove(w); free(w);
    }

    /* code-built defaults: NULL desc gives the LJ2 sizes */
    w = lj_w_osf_create(NULL, NULL);     CHECK(w && w->w == 48 && w->h == 24); free(w);
    w = lj_w_scrollctl_create(NULL, NULL); CHECK(w && w->w == 132 && w->h == 33); free(w);
    w = lj_w_palette_create(NULL, NULL); CHECK(w && w->w == 31 && w->h == 30); free(w);
    w = lj_w_fader_create(NULL, NULL);   CHECK(w && w->w == 32 && w->h == 160); free(w);
    w = lj_w_fixicon_create(NULL, NULL); CHECK(w && w->w == 63 && w->h == 53); free(w);
    w = lj_w_dragbar_create(NULL, NULL); CHECK(w && w->h == 23); free(w);
    w = lj_w_cuerow_create(NULL, NULL);  CHECK(w && w->h == 16); free(w);

    lj_surface_free(&s);
    printf("widgets_lj: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
