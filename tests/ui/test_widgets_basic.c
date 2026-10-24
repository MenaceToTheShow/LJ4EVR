/* test_widgets_basic.c - the runnable check for src/ui/widgets (basic group): each
 * class built from a hand-written lj_ctl_desc, painted into a surface, key pixels
 * asserted against the 7.3 rules, then driven by mouse and keys. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"

static int fails, font_inked;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
/* Text-ink assertions: counted once the font track has drawn glyphs (lj_fonts_gen.c
 * ships 0 inked glyphs until then); reported as NOINK meanwhile. */
#define INK(cond) do { if (font_inked) CHECK(cond); else if (!(cond)) printf("NOINK %s:%d %s\n", __FILE__, __LINE__, #cond); } while (0)

static int evcount[LJ_WEV_COUNT];
static void on_ev(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev >= 0 && ev < LJ_WEV_COUNT) evcount[ev]++; }
static int took(int ev) { int n = evcount[ev]; evcount[ev] = 0; return n; }

static lj_widget_ctor tab[LJ_K_COUNT];
static lj_widget root;
static lj_surface surf;

static lj_widget *mk(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_widget *w = tab[d->kind](parent, d);
    if (w) w->on_event = on_ev;
    return w;
}

static void paint_tree(lj_widget *w)
{
    lj_widget *c;
    if (!w->visible) return;
    if (w->vt && w->vt->paint) w->vt->paint(w, &surf);
    for (c = w->first; c; c = c->next) paint_tree(c);
}

static void repaint(void)
{
    lj_fill(&surf, 0, 0, surf.w, surf.h, root.color);
    paint_tree(&root);
}

static uint32_t px(int x, int y) { return surf.px[y * surf.w + x]; }

static int count_rgb(int x, int y, int w, int h, uint32_t rgb)   /* pixels of that colour in the rect */
{
    int i, j, n = 0;
    for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) n += px(i, j) == rgb;
    return n;
}

static int ink(int x, int y, int w, int h, uint32_t face) { return w * h - count_rgb(x, y, w, h, face); }

static void mouse(lj_widget *w, int type, int x, int y, int button, int clicks)
{
    lj_mouse_ev m;
    memset(&m, 0, sizeof m);
    m.type = type; m.x = x; m.y = y; m.button = button; m.clicks = clicks;
    if (w->vt->mouse) w->vt->mouse(w, &m);
}

static void click(lj_widget *w, int x, int y) { mouse(w, LJ_EV_MOUSE_DOWN, x, y, 1, 1); mouse(w, LJ_EV_MOUSE_UP, x, y, 1, 1); }

static int keydown(lj_widget *w, int key, int shift)
{
    lj_key_ev k;
    memset(&k, 0, sizeof k);
    k.type = LJ_EV_KEY_DOWN; k.key = key; k.shift = shift != 0;
    return w->vt->key ? w->vt->key(w, &k) : 0;
}

static void chr(lj_widget *w, int ch)
{
    lj_key_ev k;
    memset(&k, 0, sizeof k);
    k.type = LJ_EV_CHAR; k.ch = ch;
    if (w->vt->key) w->vt->key(w, &k);
}

static void free_children(lj_widget *w)
{
    lj_widget *c, *n;
    for (c = w->first; c; c = n) { n = c->next; free_children(c); free(c); }
    w->first = NULL;
}

static const char *const rg_items[] = { "&Off", "&Snap", "&Fade", "&Auto" };

static const lj_ctl_desc d_button   = { LJ_K_BUTTON, "Button1", 10, 10, 75, 25, .caption = "&OK" };
static const lj_ctl_desc d_default  = { LJ_K_BUTTON, "bDef", 100, 10, 75, 25, .caption = "Default", .default_btn = LJ_YES };
static const lj_ctl_desc d_bitbtn   = { LJ_K_BITBTN, "bOK", 200, 10, 75, 25, .kind_str = "bkOK", .num_glyphs = 1 };
static const lj_ctl_desc d_cb       = { LJ_K_CHECKBOX, "cbSync", 10, 50, 195, 17, .caption = "S&ynchronize Scene and Fade Times" };
static const lj_ctl_desc d_sbA      = { LJ_K_SPEEDBUTTON, "sbA", 10, 80, 25, 25, .caption = "A", .group_index = 1, .allow_all_up = LJ_YES };
static const lj_ctl_desc d_sbB      = { LJ_K_SPEEDBUTTON, "sbB", 40, 80, 25, 25, .caption = "B", .group_index = 1, .allow_all_up = LJ_YES };
static const lj_ctl_desc d_sbC      = { LJ_K_SPEEDBUTTON, "sbC", 70, 80, 25, 25, .caption = "C", .group_index = 2, .down = LJ_YES };
static const lj_ctl_desc d_sbFlat   = { LJ_K_SPEEDBUTTON, "sbF", 100, 80, 25, 25, .caption = "F", .flat = LJ_YES };
static const lj_ctl_desc d_rg       = { LJ_K_RADIOGROUP, "rg", 10, 120, 185, 70, .caption = "Mode", .items = rg_items, .nitems = 4, .columns_count = 2, .item_index = 0 };
static const lj_ctl_desc d_edit     = { LJ_K_EDIT, "ES", 10, 200, 40, 21, .text = "88:88", .max_length = 5 };
static const lj_ctl_desc d_ro       = { LJ_K_EDIT, "ESeqNo", 60, 200, 40, 21, .color = LJ_CLR(0xC0C0C0), .readonly = LJ_YES, .tabstop = LJ_NO, .text = "RO" };
static const lj_ctl_desc d_num      = { LJ_K_NUMEDIT, "eBase", 110, 200, 34, 21, .max_length = 3, .text = "0", .min = 0, .max = 255, .value = 0 };
static const lj_ctl_desc d_mask     = { LJ_K_MASKEDIT, "eSceneCount", 150, 200, 32, 21, .edit_mask = "!999;1; ", .max_length = 3, .text = "   " };
static const lj_ctl_desc d_dis      = { LJ_K_EDIT, "EDis", 190, 200, 40, 21, .text = "x", .enabled = LJ_NO };
static const lj_ctl_desc d_panel2   = { LJ_K_PANEL, "Panel2", 200, 120, 54, 25, .bevel_inner = LJ_BV_LOWERED, .bevel_outer = LJ_BV_LOWERED, .bevel_width = 1 };
static const lj_ctl_desc d_panelR   = { LJ_K_PANEL, "PanelR", 260, 120, 54, 25, .bevel_width = 1, .caption = "P" };
static const lj_ctl_desc d_panelS   = { LJ_K_PANEL, "PanelS", 320, 120, 54, 25, .bevel_width = 1, .border_style = "bsSingle", .parent_color = LJ_YES };
static const lj_ctl_desc d_bevTop   = { LJ_K_BEVEL, "BevT", 260, 160, 100, 20, .shape = "bsTopLine" };
static const lj_ctl_desc d_bevBox   = { LJ_K_BEVEL, "BevB", 260, 185, 100, 10, .tag = 0 };
static const lj_ctl_desc d_circle   = { LJ_K_SHAPE, "Shape1", 300, 10, 30, 30, .shape = "stCircle", .brush_color = LJ_CLR(0xFF0000), .pen_width = 1 };
static const lj_ctl_desc d_rect     = { LJ_K_SHAPE, "Shape2", 340, 10, 30, 30, .pen_width = 1 };
static const lj_ctl_desc d_label    = { LJ_K_LABEL, "Label1", 10, 240, 26, 13, .caption = "Seq#", .transparent = LJ_YES, .enabled = LJ_NO };
static const lj_ctl_desc d_labelY   = { LJ_K_LABEL, "LabelY", 50, 240, 60, 13, .caption = "Seq#", .color = LJ_CLR(0xFFFF00), .alignment = LJ_TA_RIGHT, .focus_control = "ES" };
static const lj_ctl_desc d_static   = { LJ_K_STATICTEXT, "st", 120, 240, 60, 17, .caption = "Static" };
static const lj_ctl_desc d_od2      = { LJ_K_ODPANEL2, "ODPanel21", 200, 230, 50, 30, .tag = 0 };
static const lj_ctl_desc d_od       = { LJ_K_ODPANEL, "ODPanel1", 260, 230, 60, 40, .caption = "ODPanel1" };
static const lj_ctl_desc d_image    = { LJ_K_IMAGE, "Image1", 330, 230, 20, 20, .stretch = LJ_YES };
static const lj_ctl_desc d_paintbox = { LJ_K_PAINTBOX, "pb", 360, 230, 20, 20, .hint = NULL };
static const lj_ctl_desc d_group    = { LJ_K_GROUPBOX, "GroupBox1", 210, 50, 80, 60, .caption = "Grp" };
/* splitter rig, its own root */
static const lj_ctl_desc d_left     = { LJ_K_PANEL, "PL", 0, 0, 100, 100, .align = LJ_AL_LEFT, .bevel_width = 1 };
static const lj_ctl_desc d_split    = { LJ_K_SPLITTER, "Sp", 100, 0, 3, 100, .align = LJ_AL_LEFT };
static const lj_ctl_desc d_client   = { LJ_K_PANEL, "PC", 103, 0, 197, 100, .align = LJ_AL_CLIENT, .bevel_width = 1 };

int lj_test_widgets_basic(int argc, char **argv);
int lj_test_widgets_basic(int argc, char **argv)
{
    lj_widget *b, *bdef, *bok, *cb, *sa, *sb, *sc, *sf, *rg, *es, *ro, *num, *mask, *dis, *p2, *pr, *ps, *bt, *bb, *ci, *re, *la, *ly, *st, *od2, *od, *im, *pb, *gb, *c;
    const lj_font *f = &lj_font_sans13;
    int i, tw;
    (void)argc; (void)argv;

    /* registration */
    CHECK(lj_widgets_register_basic(tab, LJ_K_COUNT) == 0);
    CHECK(lj_widgets_register_basic(tab, 3) == -1);
    CHECK(tab[LJ_K_LABEL] == lj_w_label_create && tab[LJ_K_ODPANEL] == lj_w_odpanel_create && tab[LJ_K_LISTVIEW] == NULL);

    memset(&root, 0, sizeof root);
    root.name = "Form"; root.w = 400; root.h = 300; root.visible = 1; root.enabled = 1; root.color = LJ_CLR_BTNFACE; root.font = f;
    CHECK(lj_surface_init(&surf, 400, 300) == 0);
    lj_fill(&surf, 0, 0, 40, 13, LJ_CLR_BTNFACE);                 /* the surface starts black: black text on it is invisible to the probe */
    lj_text(&surf, f, 0, 0, "Mode", LJ_CLR_3DDKSHADOW, 0);
    font_inked = ink(0, 0, 40, 13, LJ_CLR_BTNFACE) > 0;
    if (!font_inked) printf("widgets_basic: sans13 has no inked glyphs yet; text-ink checks reported as NOINK, not counted\n");

    b = mk(&root, &d_button); bdef = mk(&root, &d_default); bok = mk(&root, &d_bitbtn); cb = mk(&root, &d_cb);
    sa = mk(&root, &d_sbA); sb = mk(&root, &d_sbB); sc = mk(&root, &d_sbC); sf = mk(&root, &d_sbFlat);
    rg = mk(&root, &d_rg); es = mk(&root, &d_edit); ro = mk(&root, &d_ro); num = mk(&root, &d_num); mask = mk(&root, &d_mask); dis = mk(&root, &d_dis);
    p2 = mk(&root, &d_panel2); pr = mk(&root, &d_panelR); ps = mk(&root, &d_panelS); bt = mk(&root, &d_bevTop); bb = mk(&root, &d_bevBox);
    ci = mk(&root, &d_circle); re = mk(&root, &d_rect); la = mk(&root, &d_label); ly = mk(&root, &d_labelY); st = mk(&root, &d_static);
    od2 = mk(&root, &d_od2); od = mk(&root, &d_od); im = mk(&root, &d_image); pb = mk(&root, &d_paintbox); gb = mk(&root, &d_group);
    CHECK(b && bdef && bok && cb && sa && sb && sc && sf && rg && es && ro && num && mask && dis && p2 && pr && ps && bt && bb && ci && re && la && ly && st && od2 && od && im && pb && gb);
    CHECK(lj_widget_find(&root, "eSceneCount") == mask && mask->kind == LJ_K_MASKEDIT && num->kind == LJ_K_NUMEDIT);
    CHECK(b->tabstop && es->tabstop && !ro->tabstop && !sa->tabstop && !la->tabstop && !pr->tabstop);
    repaint();
    CHECK(took(LJ_WEV_PAINT) == 1);                                   /* the paint box fired OnPaint */

    /* TButton: raised bevel colours at the corners, face inside, caption ink, default frame */
    CHECK(px(10, 10) == LJ_CLR_BTNHIGHLIGHT && px(84, 34) == LJ_CLR_3DDKSHADOW && px(83, 33) == LJ_CLR_BTNSHADOW && px(12, 12) == LJ_CLR_BTNFACE);
    tw = lj_text_width(f, "&OK");
    INK(ink(10 + (75 - tw) / 2, 10 + 6, tw, 13, LJ_CLR_BTNFACE) > 0);
    CHECK(px(100, 10) == LJ_CLR_3DDKSHADOW && px(101, 11) == LJ_CLR_BTNHIGHLIGHT);   /* bDef: black frame, bevel inside it */
    CHECK(strcmp(lj_w_caption(bok), "OK") == 0 && lj_w_button_is_default(bok) && !lj_w_button_is_default(b));
    mouse(b, LJ_EV_MOUSE_DOWN, 5, 5, 1, 1);
    CHECK(b->focused && took(LJ_WEV_ENTER) == 1 && took(LJ_WEV_MOUSE_DOWN) == 1);
    repaint();
    CHECK(px(11, 11) == LJ_CLR_BTNSHADOW && px(100, 10) == LJ_CLR_BTNHIGHLIGHT);     /* pressed: #808080 TL inside the focus frame; bDef lost its frame */
    mouse(b, LJ_EV_MOUSE_MOVE, -5, 5, 1, 1);
    repaint();
    CHECK(px(11, 11) == LJ_CLR_BTNHIGHLIGHT);                                        /* held outside: raised again */
    mouse(b, LJ_EV_MOUSE_UP, -5, 5, 1, 1);
    CHECK(took(LJ_WEV_CLICK) == 0 && took(LJ_WEV_MOUSE_UP) == 1);
    click(b, 5, 5);
    CHECK(took(LJ_WEV_CLICK) == 1);
    CHECK(keydown(b, LJ_VK_SPACE, 0) == 1 && took(LJ_WEV_CLICK) == 1);
    repaint();
    CHECK(px(10, 10) == LJ_CLR_3DDKSHADOW && count_rgb(14, 14, 63, 1, LJ_CLR_3DDKSHADOW) > 0);   /* focused: frame + dotted focus rect row at bevel +3 */
    b->enabled = 0;
    click(b, 5, 5);
    CHECK(took(LJ_WEV_CLICK) == 0);
    repaint();
    CHECK(count_rgb(10, 10, 75, 25, LJ_CLR_BTNHIGHLIGHT) > count_rgb(100, 10, 75, 25, LJ_CLR_BTNHIGHLIGHT));   /* disabled caption's white shadow */
    b->enabled = 1;

    /* TCheckBox: 13x13 sunken box, glyph, click/Space/Alt+y toggle, set_checked fires OnClick only on change */
    repaint();
    CHECK(px(10, 52) == LJ_CLR_BTNSHADOW && px(11, 53) == LJ_CLR_3DDKSHADOW && px(22, 64) == LJ_CLR_BTNHIGHLIGHT);
    CHECK(count_rgb(12, 54, 9, 9, LJ_CLR_BTNHIGHLIGHT) == 81);
    INK(ink(27, 52, lj_text_width(f, d_cb.caption), 13, LJ_CLR_BTNFACE) > 0);
    click(cb, 100, 8);
    CHECK(lj_w_checkbox_checked(cb) == 1 && took(LJ_WEV_CLICK) == 1 && cb->focused);
    repaint();
    CHECK(count_rgb(12, 54, 9, 9, LJ_CLR_3DDKSHADOW) > 0);
    CHECK(keydown(cb, LJ_VK_SPACE, 0) == 1 && lj_w_checkbox_checked(cb) == 0 && took(LJ_WEV_CLICK) == 1);
    CHECK(lj_w_dialog_char(&root, 'Y') == 1 && lj_w_checkbox_checked(cb) == 1 && took(LJ_WEV_CLICK) == 1);
    CHECK(lj_w_dialog_char(&root, 'z') == 0);
    lj_w_checkbox_set_checked(cb, 1);
    CHECK(took(LJ_WEV_CLICK) == 0);
    lj_w_checkbox_set_checked(cb, 0);
    CHECK(took(LJ_WEV_CLICK) == 1 && !lj_w_checkbox_checked(cb));
    CHECK(lj_w_dialog_char(&root, 'o') == 1 && took(LJ_WEV_CLICK) == 1 && b->focused);   /* &OK button */

    /* TSpeedButton: GroupIndex/AllowAllUp, latched checkerboard, flat hover edge */
    CHECK(!lj_w_speedbutton_down(sa) && lj_w_speedbutton_down(sc));
    click(sa, 3, 3);
    CHECK(lj_w_speedbutton_down(sa) && took(LJ_WEV_CLICK) == 1 && !sa->focused);
    click(sb, 3, 3);
    CHECK(lj_w_speedbutton_down(sb) && !lj_w_speedbutton_down(sa));
    click(sb, 3, 3);
    CHECK(!lj_w_speedbutton_down(sb) && !lj_w_speedbutton_down(sa));                 /* AllowAllUp */
    click(sc, 3, 3);
    CHECK(lj_w_speedbutton_down(sc) && took(LJ_WEV_CLICK) == 3);                     /* not AllowAllUp: stays down, OnClick still fires */
    lj_w_speedbutton_set_down(sa, 1);
    repaint();
    CHECK(px(10, 80) == LJ_CLR_BTNSHADOW && (px(15, 85) ^ px(16, 85)) == (LJ_CLR_BTNHIGHLIGHT ^ LJ_CLR_BTNFACE));   /* latched: pressed edge, checkerboard face */
    CHECK(px(40, 80) == LJ_CLR_BTNHIGHLIGHT && px(100, 80) == LJ_CLR_BTNFACE);       /* sbB raised; flat sbF edgeless */
    mouse(sf, LJ_EV_MOUSE_MOVE, 3, 3, 0, 0);
    repaint();
    CHECK(px(100, 80) == LJ_CLR_BTNHIGHLIGHT && px(124, 104) == LJ_CLR_BTNSHADOW);   /* hover: 1-px raised */
    mouse(sf, LJ_EV_MOUSE_LEAVE, -1, -1, 0, 0);
    repaint();
    CHECK(px(100, 80) == LJ_CLR_BTNFACE);

    /* TRadioGroup: VCL ArrangeButtons geometry, etched frame broken by the caption, click/arrows/set_index */
    for (i = 0, c = rg->first; c; c = c->next, i++) { }
    CHECK(i == 4);
    c = rg->first;
    CHECK(c->x == 8 && c->y == 14 && c->w == 87 && c->h == 26 && c->kind == LJ_K_RADIOBUTTON && lj_w_radiobutton_checked(c) && c->tabstop);
    c = c->next;        CHECK(c->x == 8 && c->y == 40 && !c->tabstop);
    c = c->next;        CHECK(c->x == 95 && c->y == 14);
    c = c->next;        CHECK(c->x == 95 && c->y == 40 && strcmp(lj_w_caption(c), "&Auto") == 0);
    repaint();
    CHECK(px(10, 125) == LJ_CLR_BTNSHADOW && px(11, 126) == LJ_CLR_BTNHIGHLIGHT && px(194, 189) == LJ_CLR_BTNHIGHLIGHT && px(193, 188) == LJ_CLR_BTNSHADOW);
    CHECK(count_rgb(18, 125, lj_text_width(f, "Mode"), 1, LJ_CLR_BTNSHADOW) == 0);  /* caption breaks the top line */
    INK(ink(18, 120, lj_text_width(f, "Mode"), 13, LJ_CLR_BTNFACE) > 0);
    CHECK(ink(18, 141, 12, 12, LJ_CLR_BTNFACE) > 0);                                 /* item 0's 12x12 ring at (8, 14+7) inside the group */
    CHECK(lj_w_radiogroup_index(rg) == 0);
    c = rg->first->next->next;
    click(c, 3, 3);
    CHECK(lj_w_radiogroup_index(rg) == 2 && took(LJ_WEV_CLICK) == 1 && c->focused && c->tabstop && !rg->first->tabstop);
    CHECK(keydown(c, LJ_VK_DOWN, 0) == 1 && lj_w_radiogroup_index(rg) == 3 && took(LJ_WEV_CLICK) == 1);
    c = lj_widget_focused(&root);
    CHECK(keydown(c, LJ_VK_RIGHT, 0) == 1 && lj_w_radiogroup_index(rg) == 0);          /* wraps */
    c = lj_widget_focused(&root);
    CHECK(keydown(c, LJ_VK_UP, 0) == 1 && lj_w_radiogroup_index(rg) == 3);             /* wraps backwards */
    took(LJ_WEV_CLICK);
    CHECK(lj_w_radiogroup_set_index(rg, 1) == 0 && lj_w_radiogroup_index(rg) == 1 && took(LJ_WEV_CLICK) == 1);
    CHECK(lj_w_radiogroup_set_index(rg, 9) == -1);
    CHECK(lj_w_dialog_char(&root, 'f') == 1 && lj_w_radiogroup_index(rg) == 2 && took(LJ_WEV_CLICK) == 1);

    /* TEdit: text, caret, selection, MaxLength, read-only silver, disabled face */
    CHECK(strcmp(lj_w_edit_text(es), "88:88") == 0 && lj_w_edit_text(la) == NULL);
    lj_w_focus(cb); took(LJ_WEV_ENTER); took(LJ_WEV_EXIT);
    lj_w_focus(es);
    CHECK(es->focused && !cb->focused && took(LJ_WEV_ENTER) == 1 && took(LJ_WEV_EXIT) == 1);
    keydown(es, LJ_VK_END, 0);
    chr(es, '9');
    CHECK(strcmp(lj_w_edit_text(es), "88:88") == 0 && took(LJ_WEV_CHANGE) == 0);     /* MaxLength 5 */
    keydown(es, LJ_VK_BACK, 0);
    chr(es, 'x');
    CHECK(strcmp(lj_w_edit_text(es), "88:8x") == 0 && took(LJ_WEV_CHANGE) == 2 && took(LJ_WEV_KEY_PRESS) == 2);
    keydown(es, LJ_VK_HOME, 0);
    keydown(es, LJ_VK_END, 1);
    chr(es, 0xE9);                                                                   /* CP1252 e-acute replaces the selection */
    CHECK(strcmp(lj_w_edit_text(es), "\xE9") == 0);
    repaint();
    CHECK(px(12, 202) == LJ_CLR_BTNHIGHLIGHT && px(10, 200) == LJ_CLR_BTNSHADOW && px(11, 201) == LJ_CLR_3DDKSHADOW);
    CHECK(count_rgb(13 + lj_text_width(f, "\xE9"), 204, 1, 13, LJ_CLR_3DDKSHADOW) == 13);   /* caret after the glyph */
    mouse(es, LJ_EV_MOUSE_DOWN, 3, 10, 1, 1);
    CHECK(((lj_w_edit *)es)->caret == 0);
    mouse(es, LJ_EV_MOUSE_MOVE, 38, 10, 1, 1);
    CHECK(((lj_w_edit *)es)->caret == 1 && ((lj_w_edit *)es)->anchor == 0);
    mouse(es, LJ_EV_MOUSE_UP, 38, 10, 1, 1);
    CHECK(took(LJ_WEV_CLICK) == 1);                                                  /* release inside = OnClick */
    repaint();
    CHECK(count_rgb(13, 204, lj_text_width(f, "\xE9"), 13, LJ_CLR_HIGHLIGHT) > 0);  /* selection navy */
    CHECK(lj_w_edit_set_text(es, "ab cdef") == 0 && strcmp(lj_w_edit_text(es), "ab cd") == 0 && took(LJ_WEV_CHANGE) >= 1);   /* MaxLength clamps */
    mouse(es, LJ_EV_MOUSE_DOWN, 3 + lj_text_width(f, "ab c"), 10, 1, 2);            /* double-click selects the word */
    mouse(es, LJ_EV_MOUSE_UP, 3 + lj_text_width(f, "ab c"), 10, 1, 2);
    CHECK(((lj_w_edit *)es)->anchor == 3 && ((lj_w_edit *)es)->caret == 5 && took(LJ_WEV_DBLCLICK) == 1 && took(LJ_WEV_CLICK) == 1);
    chr(ro, 'a');
    CHECK(strcmp(lj_w_edit_text(ro), "RO") == 0 && px(62, 202) == LJ_CLR_BTNFACE);  /* read-only: text kept, silver interior */
    CHECK(px(192, 202) == LJ_CLR_BTNFACE);                                           /* disabled: face interior */
    INK(count_rgb(193, 204, 10, 13, LJ_CLR_BTNSHADOW) > 0);                          /* disabled: grey text */
    CHECK(lj_w_edit_set_text(es, "4:00") == 0 && ((lj_w_edit *)es)->caret == 0 && ((lj_w_edit *)es)->anchor == 0);   /* ME-206: caret 0 after Text := (Win32 WM_SETTEXT); the Sequence time fields move it (test_windows_a) */
    took(LJ_WEV_CHANGE);

    /* TNumEdit: digits only, Value clamped, OnValueChange */
    CHECK(lj_w_numedit_value(num) == 0 && lj_w_numedit_value(es) == 0);
    lj_w_edit_set_text(num, "");
    chr(num, '1'); chr(num, 'a'); chr(num, '2');
    CHECK(strcmp(lj_w_edit_text(num), "12") == 0 && lj_w_numedit_value(num) == 12 && took(LJ_WEV_VALUE_CHANGE) == 2);
    chr(num, '9');
    CHECK(lj_w_numedit_value(num) == 129);
    lj_w_edit_set_text(num, "999");
    CHECK(lj_w_numedit_value(num) == 255);
    CHECK(lj_w_numedit_set_value(num, 300) == 0 && strcmp(lj_w_edit_text(num), "255") == 0 && lj_w_numedit_set_value(es, 1) == -1);
    took(LJ_WEV_VALUE_CHANGE); took(LJ_WEV_CHANGE);

    /* TMaskEdit "!999;1; ": fixed 3 positions, overwrite, digits only, Back blanks */
    CHECK(strcmp(lj_w_edit_text(mask), "   ") == 0);
    chr(mask, '4');
    CHECK(strcmp(lj_w_edit_text(mask), "4  ") == 0 && ((lj_w_edit *)mask)->caret == 1 && took(LJ_WEV_CHANGE) == 1);
    chr(mask, 'a');
    CHECK(strcmp(lj_w_edit_text(mask), "4  ") == 0);
    chr(mask, '5'); chr(mask, '6'); chr(mask, '7');
    CHECK(strcmp(lj_w_edit_text(mask), "456") == 0 && ((lj_w_edit *)mask)->caret == 3);
    keydown(mask, LJ_VK_BACK, 0);
    CHECK(strcmp(lj_w_edit_text(mask), "45 ") == 0 && ((lj_w_edit *)mask)->caret == 2);
    lj_w_edit_set_text(mask, "7");
    CHECK(strcmp(lj_w_edit_text(mask), "7  ") == 0);
    took(LJ_WEV_CHANGE);

    /* TPanel: lowered/lowered, default raised, bsSingle black frame, ParentColor */
    repaint();
    CHECK(px(200, 120) == LJ_CLR_BTNSHADOW && px(253, 144) == LJ_CLR_BTNHIGHLIGHT && px(201, 121) == LJ_CLR_BTNSHADOW && px(252, 143) == LJ_CLR_BTNHIGHLIGHT && px(210, 130) == LJ_CLR_BTNFACE);
    CHECK(px(260, 120) == LJ_CLR_BTNHIGHLIGHT && px(313, 144) == LJ_CLR_BTNSHADOW && px(261, 121) == LJ_CLR_BTNFACE);
    INK(ink(261, 121, 52, 23, LJ_CLR_BTNFACE) > 0);                                 /* caption "P" */
    CHECK(px(320, 120) == LJ_CLR_3DDKSHADOW && px(321, 121) == LJ_CLR_BTNHIGHLIGHT);
    CHECK(ps->color == LJ_CLR_BTNFACE && pr->color == LJ_CLR_BTNFACE);
    click(pr, 5, 5);
    CHECK(took(LJ_WEV_CLICK) == 1);
    CHECK(lj_w_set_caption(pr, "Q") == 0 && strcmp(lj_w_caption(pr), "Q") == 0 && lj_w_set_caption(bb, "x") == -1 && lj_w_caption(bb) == NULL);

    /* TBevel: bsTopLine pair, bsBox lowered */
    CHECK(px(300, 160) == LJ_CLR_BTNSHADOW && px(300, 161) == LJ_CLR_BTNHIGHLIGHT && px(300, 162) == LJ_CLR_BTNFACE);
    CHECK(px(260, 185) == LJ_CLR_BTNSHADOW && px(359, 194) == LJ_CLR_BTNHIGHLIGHT && px(300, 190) == LJ_CLR_BTNFACE);

    /* TShape: circle (brush red, black pen, corners untouched), rectangle (white, black outline) */
    CHECK(px(315, 25) == LJ_CLR_RED && px(300, 10) == LJ_CLR_BTNFACE && px(329, 39) == LJ_CLR_BTNFACE && px(315, 10) == LJ_CLR_3DDKSHADOW && px(300, 25) == LJ_CLR_3DDKSHADOW);
    CHECK(px(340, 10) == LJ_CLR_3DDKSHADOW && px(369, 39) == LJ_CLR_3DDKSHADOW && px(355, 25) == LJ_CLR_BTNHIGHLIGHT);

    /* TLabel: disabled transparent = grey ink + white shadow, no black; coloured right-aligned label; FocusControl */
    CHECK(count_rgb(10, 240, 27, 14, LJ_CLR_3DDKSHADOW) == 0);
    INK(count_rgb(10, 240, 27, 14, LJ_CLR_BTNSHADOW) > 0 && count_rgb(10, 240, 27, 14, LJ_CLR_BTNHIGHLIGHT) > 0);
    CHECK(px(50, 240) == LJ_CLR_YELLOW && ink(50, 240, 30, 13, LJ_CLR_YELLOW) == 0);
    INK(ink(84, 240, 26, 13, LJ_CLR_YELLOW) > 0);
    CHECK(la->transparent && !ly->transparent && la->color == LJ_CLR_BTNFACE);
    lj_w_focus(cb);
    CHECK(lj_w_dialog_char(&root, 'q') == 0);
    ((lj_w_cap *)ly)->caption[0] = '&';                                              /* "&eq#": Alt+e focuses ES through FocusControl */
    CHECK(lj_w_dialog_char(&root, 'e') == 1 && es->focused);

    /* TStaticText, TGroupBox, TODPanel2, TODPanel, TImage */
    CHECK(px(120, 240) == LJ_CLR_BTNFACE);
    INK(ink(120, 240, lj_text_width(f, "Static"), 13, LJ_CLR_BTNFACE) > 0);
    CHECK(px(210, 55) == LJ_CLR_BTNSHADOW && px(211, 56) == LJ_CLR_BTNHIGHLIGHT);
    INK(ink(218, 50, lj_text_width(f, "Grp"), 13, LJ_CLR_BTNFACE) > 0);
    CHECK(px(200, 230) == LJ_CLR_BTNSHADOW && px(249, 259) == LJ_CLR_BTNHIGHLIGHT && px(225, 245) == LJ_CLR_BTNFACE);
    CHECK(px(260, 230) == LJ_CLR_BTNSHADOW && px(261, 246) == LJ_CLR_BTNSHADOW && px(261, 247) == LJ_CLR_BTNHIGHLIGHT);
    INK(ink(262, 232, 56, 13, LJ_CLR_BTNFACE) > 0);
    CHECK(ink(330, 230, 20, 20, LJ_CLR_BTNFACE) == 0 && ink(360, 230, 20, 20, LJ_CLR_BTNFACE) == 0);
    click(im, 2, 2);
    CHECK(took(LJ_WEV_CLICK) == 1);

    /* TSplitter: drags the alLeft sibling live, clamps at MinSize both sides, OnMoved on release */
    {
        lj_widget r2, *pl, *sp, *pc;
        memset(&r2, 0, sizeof r2);
        r2.w = 300; r2.h = 100; r2.visible = 1; r2.enabled = 1; r2.color = LJ_CLR_BTNFACE; r2.font = f;
        pl = mk(&r2, &d_left); sp = mk(&r2, &d_split); pc = mk(&r2, &d_client);
        lj_widget_layout(&r2);
        CHECK(sp->x == 100 && pc->x == 103 && pc->w == 197);
        mouse(sp, LJ_EV_MOUSE_DOWN, 1, 50, 1, 1);
        mouse(sp, LJ_EV_MOUSE_MOVE, 21, 50, 1, 1);
        CHECK(pl->w == 120 && sp->x == 120 && pc->x == 123 && pc->w == 177 && r2.dirty);
        mouse(sp, LJ_EV_MOUSE_MOVE, -300, 50, 1, 1);
        CHECK(pl->w == 30 && sp->x == 30);
        mouse(sp, LJ_EV_MOUSE_MOVE, 500, 50, 1, 1);
        CHECK(pl->w == 300 - 3 - 30);
        mouse(sp, LJ_EV_MOUSE_UP, 0, 50, 1, 1);
        CHECK(took(LJ_WEV_MOVED) == 1);
        free_children(&r2);
    }

    free_children(&root);
    lj_surface_free(&surf);
    printf("widgets_basic: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
