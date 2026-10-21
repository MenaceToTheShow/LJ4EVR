/* test_menu.c - the runnable check for src/ui/lj_menu.c on TSEQUENCEFORM's main
 * menu and the system menu: bar geometry (19 px, 6-px padding, text at +6,+2), drop-down
 * surfaces (17-px items, 3-px border), mouse and keyboard navigation, Alt and
 * Alt+letter, firing through the binding, checked/enabled state, shortcut text. */
#include <stdio.h>
#include <string.h>
#include "gfx/lj_font.h"
#include "ui/lj_window.h"
#include "ui/lj_menu.h"
#include "ui/lj_frame.h"
#include "ui/forms/lj_form.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

extern const lj_form_desc lj_form_TSEQUENCEFORM;

static int fired[8];
static const lj_menu_item_desc *fired_item;
static void on_item(lj_widget *w, void *ud) { fired_item = lj_window_menu_item(lj_window_of(w)); (*(int *)ud)++; }
static void nop(lj_widget *w, void *ud) { (void)w; (void)ud; }

static int sys_cmd_seen = -1;
static void sys_cmd(void *ud, lj_window *w, int cmd) { (void)ud; (void)w; sys_cmd_seen = cmd; }
static int screen_pos(void *ud, const lj_window *w, int *x, int *y) { (void)ud; (void)w; *x = 1000; *y = 500; return 0; }
static int shown;
static void *popup_show(void *ud, lj_window *o, const lj_surface *s, int sx, int sy) { (void)ud; (void)o; (void)sx; (void)sy; shown++; return (void *)s; }
static void popup_hide(void *ud, void *h) { (void)ud; (void)h; shown--; }

static void key(lj_window *win, int vk, int alt, int type)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.key.type = type; ev.u.key.key = vk; ev.u.key.alt = alt ? 1u : 0u;
    lj_window_event(win, &ev);
}

static void mouse(lj_window *win, int type, int x, int y)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.button = 1; ev.u.mouse.clicks = 1; ev.u.mouse.x = x; ev.u.mouse.y = y;
    lj_window_event(win, &ev);
}

int lj_test_menu(int argc, char **argv);
int lj_test_menu(int argc, char **argv)
{
    lj_binding b[] = {
        { "FormActivate", nop, NULL }, { "FormClose", nop, NULL }, { "FormCreate", nop, NULL }, { "FormDestroy", nop, NULL }, { "FormHide", nop, NULL }, { "FormShow", nop, NULL },
        { "cbSyncClick", nop, NULL }, { "cbSyncEnter", nop, NULL }, { "cbGlobalFadeClick", nop, NULL }, { "cbGlobalSceneClick", nop, NULL }, { "UdSceneChange", nop, NULL }, { "UdFadeChange", nop, NULL },
        { "DisableFade1Click", on_item, &fired[0] }, { "Preferences1Click", on_item, &fired[1] }, { "M_KeepBlindClick", on_item, &fired[2] }, { "M_SnapShotClick", on_item, &fired[3] },
        { "Help1Click", on_item, &fired[4] }, { NULL, NULL, NULL } };
    lj_window_host host = { NULL, screen_pos, popup_show, NULL, popup_hide, NULL, sys_cmd };
    lj_window *win = lj_window_create(&lj_form_TSEQUENCEFORM, b);
    lj_menu *m;
    lj_rect r0, r1, r3;
    char sc[32];
    int bar_y;
    (void)argc; (void)argv;
    CHECK(win != NULL);
    if (!win) return 1;
    m = lj_window_menus(win);
    bar_y = 3 + 19;

    /* bar geometry: items at x = frame_w, width = text + 2 * 6, 19 px tall */
    CHECK(m->bar && m->bar->n == 4 && m->bar_x == 3 && m->bar_y == bar_y && m->bar_w == 466);
    CHECK(lj_menu_bar_item_rect(m, 0, &r0) == 0 && r0.x == 3 && r0.y == bar_y && r0.h == 19);
    CHECK(r0.w == lj_text_width(&lj_font_sans13, "Disable Fade") + 12);
    CHECK(lj_menu_bar_item_rect(m, 1, &r1) == 0 && r1.x == r0.x + r0.w);
    CHECK(lj_menu_bar_item_rect(m, 3, &r3) == 0);
    CHECK(lj_menu_bar_item_at(m, r1.x + 2, bar_y + 5) == 1 && lj_menu_bar_item_at(m, r1.x + 2, bar_y + 19) == -1);
    CHECK(lj_menu_bar_item_rect(m, 4, &r0) == -1);
    CHECK(lj_menu_accel("&Preferences") == 'p' && lj_menu_accel("Mi&nimize") == 'n' && lj_menu_accel("A && B") == 0 && lj_menu_accel("Snapshot") == 0);
    /* painted bar vs the 1:1 LJ2 captures (ME-39: saveselectiondialog.gif, text rows 26..34, underline row 36, first ink
     * column bar_x + 6): the 'P' stem of "&Preferences" inks rows 26..34 at x = r1.x + 6, its accelerator underline is
     * row 36 (= bar_y + 2 + ascent 11 + 1), rows 35 and 37 are face, column r1.x + 5 is never inked */
    {
        const lj_surface *s;
        int px = r1.x + 6;
        lj_window_paint(win);
        s = lj_window_surface(win);
        CHECK(s->px[26 * s->w + px] == 0x000000u && s->px[34 * s->w + px] == 0x000000u && s->px[25 * s->w + px] == 0xC0C0C0u);
        CHECK(s->px[36 * s->w + px] == 0x000000u && s->px[35 * s->w + px] == 0xC0C0C0u && s->px[37 * s->w + px] == 0xC0C0C0u);
        CHECK(s->px[36 * s->w + px - 1] == 0xC0C0C0u && s->px[30 * s->w + px - 1] == 0xC0C0C0u);
    }

    /* mouse: click Preferences -> one level with its single child, 6 + 17 px tall */
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    CHECK(m->bar_open && m->bar_sel == 1 && m->depth == 1 && lj_menu_is_open(m));
    CHECK(m->lv[0].n == 1 && strcmp(m->lv[0].items[0].name, "M_KeepBlind") == 0);
    CHECK(m->lv[0].s.h == 6 + 17 && m->lv[0].s.w >= 3 + 20 + lj_text_width(&lj_font_sans13, "Keep Contents When Entering Blind Mode") + 18 + 3);
    CHECK(m->lv[0].sel == -1 && m->lv[0].handle == NULL);              /* headless: built, not shown */
    key(win, 0x28, 0, LJ_EV_KEY_DOWN);                                   /* Down selects the first item */
    CHECK(m->lv[0].sel == 0);
    key(win, 0x0D, 0, LJ_EV_KEY_DOWN);                                   /* Enter fires it and closes everything */
    CHECK(fired[2] == 1 && fired_item && strcmp(fired_item->name, "M_KeepBlind") == 0);
    CHECK(!lj_menu_is_open(m) && m->depth == 0 && m->bar_sel == -1 && lj_window_menu_item(win) == NULL);
    /* click toggles: open, click the same item again closes */
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    CHECK(m->depth == 1);
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    CHECK(m->depth == 0 && !m->bar_open);
    /* a childless bar item fires on the click */
    mouse(win, LJ_EV_MOUSE_DOWN, r3.x + 3, bar_y + 4);
    CHECK(fired[4] == 1 && !lj_menu_is_open(m));
    /* mouse on an open level: move selects, up fires */
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    lj_window_set_host(win, &host);                                      /* now levels get handles */
    lj_menu_close(m);
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    CHECK(m->depth == 1 && m->lv[0].handle != NULL && shown == 1);
    CHECK(m->lv[0].x == 1000 + r1.x && m->lv[0].y == 500 + bar_y + 19);  /* below the bar item, in screen coords */
    {
        lj_ui_event ev;
        memset(&ev, 0, sizeof ev);
        ev.type = LJ_EV_MOUSE_MOVE; ev.u.mouse.type = LJ_EV_MOUSE_MOVE; ev.u.mouse.x = 30; ev.u.mouse.y = 3 + 5;
        lj_window_popup_event(win, m->lv[0].handle, &ev);
        CHECK(m->lv[0].sel == 0);
        ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP; ev.u.mouse.button = 1;
        lj_window_popup_event(win, m->lv[0].handle, &ev);
        CHECK(fired[2] == 2 && m->depth == 0 && shown == 0);
    }
    /* a click outside while open just closes */
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    mouse(win, LJ_EV_MOUSE_DOWN, 200, 150);
    CHECK(m->depth == 0 && fired[0] == 0);

    /* keyboard: Alt press+release = bar active on item 0; Right/Left move; Esc leaves */
    key(win, 0x12, 1, LJ_EV_KEY_DOWN);
    key(win, 0x12, 0, LJ_EV_KEY_UP);
    CHECK(m->bar_active && m->bar_sel == 0 && m->depth == 0);
    key(win, 0x27, 0, LJ_EV_KEY_DOWN);
    CHECK(m->bar_sel == 1 && m->depth == 1 && m->lv[0].sel == 0);        /* Right opens the next item's drop-down */
    key(win, 0x27, 0, LJ_EV_KEY_DOWN);
    CHECK(m->bar_sel == 2 && m->depth == 0);                             /* Snapshot has no children */
    key(win, 0x25, 0, LJ_EV_KEY_DOWN);
    CHECK(m->bar_sel == 1 && m->depth == 1);
    key(win, 0x1B, 0, LJ_EV_KEY_DOWN);
    CHECK(m->depth == 0 && m->bar_active);
    key(win, 0x1B, 0, LJ_EV_KEY_DOWN);
    CHECK(!lj_menu_is_open(m));
    /* F10 toggles; Alt+H fires Help (childless) directly; Alt+P opens Preferences */
    key(win, 0x79, 0, LJ_EV_KEY_DOWN);
    CHECK(m->bar_active);
    key(win, 0x79, 0, LJ_EV_KEY_DOWN);
    CHECK(!m->bar_active);
    key(win, 'H', 1, LJ_EV_KEY_DOWN);
    CHECK(fired[4] == 2 && !lj_menu_is_open(m));
    key(win, 'P', 1, LJ_EV_KEY_DOWN);
    CHECK(m->depth == 1 && m->lv[0].sel == 0);
    key(win, 0x1B, 0, LJ_EV_KEY_DOWN);
    key(win, 0x1B, 0, LJ_EV_KEY_DOWN);
    CHECK(!lj_menu_is_open(m));
    /* an unknown Alt+letter is swallowed by nothing */
    key(win, 'Q', 1, LJ_EV_KEY_DOWN);
    CHECK(!lj_menu_is_open(m));

    /* item state by name; disabled items are skipped by keyboard navigation */
    CHECK(lj_menu_set_checked(m, "M_KeepBlind", 1) == 0 && lj_menu_rt_named(m, "M_KeepBlind")->checked);
    CHECK(lj_menu_set_enabled(m, "Nope", 1) == -1);
    CHECK(lj_menu_set_enabled(m, "Preferences1", 0) == 0);
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    CHECK(m->depth == 0 && m->bar_sel == 1);                             /* disabled bar item highlights but does not drop */
    lj_menu_close(m);
    lj_menu_set_enabled(m, "Preferences1", 1);
    lj_menu_set_visible(m, "DisableFade1", 0);
    CHECK(lj_menu_bar_item_rect(m, 1, &r0) == 0 && r0.x == 3);           /* hidden item takes no room */
    lj_menu_set_visible(m, "DisableFade1", 1);

    /* shortcut text + dispatch */
    CHECK(lj_menu_shortcut_text(0x4000 | 'S', sc, sizeof sc) == 0 && strcmp(sc, "Ctrl+S") == 0);
    CHECK(lj_menu_shortcut_text(0x2000 | 0x4000 | 0x73, sc, sizeof sc) == 0 && strcmp(sc, "Shift+Ctrl+F4") == 0);
    CHECK(lj_menu_shortcut_text(0x8000 | 0x7B, sc, sizeof sc) == 0 && strcmp(sc, "Alt+F12") == 0);
    CHECK(lj_menu_shortcut_text(0x2E, sc, sizeof sc) == 0 && strcmp(sc, "Del") == 0);
    CHECK(lj_menu_shortcut_text(0, sc, sizeof sc) == -1);
    {
        lj_key_ev k; memset(&k, 0, sizeof k); k.type = LJ_EV_KEY_DOWN; k.key = 0x73; k.alt = 1;
        CHECK(lj_menu_shortcut(m, &k) == 0);                             /* Alt+F4 lives on the system menu, not the bar */
    }

    /* the system menu: icon click opens it; items carry the lj_sys_cmd tag; Close reaches the host */
    mouse(win, LJ_EV_MOUSE_DOWN, 10, 5);
    CHECK(m->depth == 1 && m->lv[0].items == lj_frame_sysmenu.items && m->lv[0].n == 7);
    CHECK(m->lv[0].x == 1000 + 3 && m->lv[0].y == 500 + 3 + 19);
    CHECK(m->lv[0].s.h == 6 + 6 * 17 + 9);
    CHECK(!lj_menu_rt_named(m, "SysRestore")->enabled && !lj_menu_rt_named(m, "SysSize")->enabled && !lj_menu_rt_named(m, "SysMinimize")->enabled);
    CHECK(lj_menu_rt_named(m, "SysMove")->enabled && lj_menu_rt_named(m, "SysClose")->enabled);
    key(win, 0x26, 0, LJ_EV_KEY_DOWN);                                   /* Up from none wraps to the last = Close */
    CHECK(m->lv[0].sel == 6);
    key(win, 0x0D, 0, LJ_EV_KEY_DOWN);
    CHECK(sys_cmd_seen == LJ_SYS_CLOSE && m->depth == 0);
    /* caption close button: press + release inside */
    mouse(win, LJ_EV_MOUSE_DOWN, 472 - 3 - 2 - 8, 10);
    sys_cmd_seen = -1;
    mouse(win, LJ_EV_MOUSE_UP, 472 - 3 - 2 - 8, 10);
    CHECK(sys_cmd_seen == LJ_SYS_CLOSE);
    mouse(win, LJ_EV_MOUSE_DOWN, 472 - 3 - 2 - 8, 10);
    sys_cmd_seen = -1;
    mouse(win, LJ_EV_MOUSE_UP, 200, 100);                                /* released elsewhere: nothing */
    CHECK(sys_cmd_seen == -1);

    /* a level paints: highlighted row is #000080 inside the 3-px border */
    mouse(win, LJ_EV_MOUSE_DOWN, r1.x + 3, bar_y + 4);
    key(win, 0x28, 0, LJ_EV_KEY_DOWN);
    CHECK(m->lv[0].s.px[(3 + 8) * m->lv[0].s.w + 5] == 0x000080u);
    CHECK(m->lv[0].s.px[1 * m->lv[0].s.w + 1] == 0xFFFFFFu && m->lv[0].s.px[0] == 0xC0C0C0u);
    lj_menu_close(m);
    CHECK(shown == 0);

    lj_window_destroy(win);
    printf("menu: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
