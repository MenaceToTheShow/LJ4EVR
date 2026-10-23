/* test_window.c - the runnable check for src/ui/lj_window.c + lj_frame.c + lj_focus.c + lj_dnd.c:
 * TSEQUENCEFORM instantiation (client size, frame metrics, z-order), frame
 * hit-test, ME-16 binding both ways, dirty reporting, TabOrder traversal, Enter
 * on Default / Esc on Cancel, drag and drop, native-frame flag, resize. */
#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "ui/lj_window.h"
#include "ui/lj_dnd.h"
#include "ui/lj_focus.h"
#include "ui/lj_frame.h"
#include "ui/forms/lj_form.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

extern const lj_form_desc lj_form_TSEQUENCEFORM;

static int hits[32];
static void count(lj_widget *w, void *ud) { (void)w; (*(int *)ud)++; }

#ifdef NDEBUG   /* LORD_C-12: read only by the release-only ME-16 row below (clang -Wunused-but-set-global) */
static int logged;
static void sink(void *ud, const char *line) { (void)ud; (void)line; logged++; }
#endif

/* a hand-built form: OK (Default) / Cancel buttons, a panel that takes drops, two edits */
static const lj_ctl_desc mini_ctls[] = {
    { LJ_K_EDIT,   "E1",     10, 10, 60, 21, .tab_order = 1 },
    { LJ_K_EDIT,   "E2",     10, 40, 60, 21, .tab_order = 0 },
    { LJ_K_BUTTON, "OK",     10, 70, 60, 25, .tab_order = 2, .caption = "OK", .default_btn = LJ_YES, .events = { { "OnClick", "OKClick" } } },
    { LJ_K_BUTTON, "Cancel", 80, 70, 60, 25, .tab_order = 3, .caption = "Cancel", .cancel = LJ_YES, .events = { { "OnClick", "CancelClick" } } },
    { LJ_K_PANEL,  "Drop",  100, 10, 80, 50, .tab_order = 4, .events = { { "OnDragDrop", "DropDrop" }, { "OnDragOver", "DropOver" } } },
};
static const lj_form_desc mini = {
    .name = "MiniForm", .class_name = "TMiniForm", .caption = "Mini", .client_width = 200, .client_height = 110, .text_height = 13,
    .font = LJ_FONT_SANS13, .border = LJ_BS_DIALOG, .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .position = LJ_PO_OWNERFORMCENTER,
    .ctls = mini_ctls, .n = LJ_COUNT_OF(mini_ctls),
};

static int drop_kind, drop_no, drop_x, drop_y;
static void on_drop(lj_widget *w, void *ud) { (void)ud; lj_dnd_info(lj_window_of(w), &drop_kind, &drop_no, &drop_x, &drop_y); hits[9]++; }

int lj_test_window(int argc, char **argv);
int lj_test_window(int argc, char **argv)
{
    lj_binding seq_b[] = {
        { "FormActivate", count, &hits[0] }, { "FormClose", count, &hits[1] }, { "FormCreate", count, &hits[2] }, { "FormDestroy", count, &hits[3] },
        { "FormHide", count, &hits[4] }, { "FormShow", count, &hits[5] }, { "cbSyncClick", count, &hits[6] }, { "cbSyncEnter", count, &hits[6] },
        { "cbGlobalFadeClick", count, &hits[6] }, { "cbGlobalSceneClick", count, &hits[6] }, { "UdSceneChange", count, &hits[6] }, { "UdFadeChange", count, &hits[6] },
        { "DisableFade1Click", count, &hits[7] }, { "Preferences1Click", count, &hits[7] }, { "M_KeepBlindClick", count, &hits[7] }, { "M_SnapShotClick", count, &hits[7] },
        { "Help1Click", count, &hits[7] }, { NULL, NULL, NULL } };
    lj_binding mini_b[] = { { "OKClick", count, &hits[10] }, { "CancelClick", count, &hits[11] }, { "DropDrop", on_drop, NULL }, { "DropOver", count, &hits[12] }, { NULL, NULL, NULL } };
    lj_window *win, *mw;
    lj_widget *root, *w, *chain[64];
    lj_rect cr, r[4];
    lj_ui_event ev;
    lj_form_desc d2;
    const lj_window_caption_state *cs;
    int n, i, ax, ay;
    (void)argc; (void)argv;

    /* ---- TSEQUENCEFORM: bsSingle + biSystemMenu + a main menu ---- */
    win = lj_window_create(&lj_form_TSEQUENCEFORM, seq_b);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(hits[2] == 1);                                            /* OnCreate fired once */
    root = lj_window_root(win);
    cs = lj_window_caption(win);
    CHECK(root && root->w == 466 && root->h == 163);
    CHECK(cs->frame_w == 3 && cs->caption_h == 19 && cs->menu_h == 19);
    CHECK(cs->icons == LJ_BI_SYSMENU && cs->border == LJ_BS_SINGLE);
    CHECK(strcmp(cs->text, "Sequence") == 0);
    lj_window_client_rect(win, &cr);
    CHECK(cr.x == 3 && cr.y == 3 + 19 + 19 && cr.w == 466 && cr.h == 163);
    CHECK(lj_window_surface(win)->w == 472 && lj_window_surface(win)->h == 207);
    CHECK(lj_window_menu(win) != NULL && lj_window_menu(win)->n == 4);
    CHECK(lj_window_desc(win) == &lj_form_TSEQUENCEFORM);
    CHECK(lj_window_of(lj_window_find(win, "sb7")) == win);

    /* z-order: Panel1 covers ESeqType except the sliver x 354..359 */
    w = lj_window_find(win, "ESeqType");
    CHECK(w && w->x == 215 && w->w == 145);
    CHECK(lj_widget_hit(root, 356, 20) == w);
    CHECK(lj_widget_hit(root, 295, 20) == lj_window_find(win, "Bevel3"));   /* between sb7 (ends 293) and sb5 (starts 298) */
    CHECK(lj_widget_hit(root, 353, 20) == lj_window_find(win, "Panel1"));   /* Bevel3 ends at x 350 */
    CHECK(lj_widget_hit(root, 220, 36) == lj_window_find(win, "Panel1"));
    CHECK(lj_widget_hit(root, 250, 10) == lj_window_find(win, "sb7"));
    lj_widget_abs(lj_window_find(win, "sb7"), &ax, &ay);
    CHECK(ax == 3 + 243 && ay == 41 + 6);                          /* root origin inside the surface */
    w = lj_window_find(win, "MainMenu1");
    CHECK(w && !w->visible && w->w == 0);                           /* non-visual rows exist, hidden */

    /* frame hit-test in surface coords */
    CHECK(lj_window_hit(win, 10, 5) == LJ_HIT_SYSMENU);             /* the 16x16 icon slot at x=2 of the caption */
    CHECK(lj_window_hit(win, 40, 5) == LJ_HIT_CAPTION);
    CHECK(lj_window_hit(win, 471 - 2 - 8, 10) == LJ_HIT_BUTTON);     /* close button */
    CHECK(lj_window_hit(win, 10, 25) == LJ_HIT_MENU);
    CHECK(lj_window_hit(win, 10, 50) == LJ_HIT_CLIENT);
    CHECK(lj_window_hit(win, 1, 100) == LJ_HIT_FRAME);              /* bsSingle never resizes */
    CHECK(lj_frame_button_at(cs, 472, 471 - 2 - 8, 10) == LJ_FBTN_CLOSE);
    CHECK(lj_frame_button_at(cs, 472, 471 - 2 - 26, 10) == LJ_FBTN_NONE);   /* no min/max on biSystemMenu-only */

    /* paint + dirty: first paint = everything, then nothing, then one widget rect */
    lj_window_paint(win);
    n = lj_window_dirty(win, r, 4);
    CHECK(n == 1 && r[0].w == 472 && r[0].h == 207);
    lj_window_paint(win);
    CHECK(lj_window_dirty(win, r, 4) == 0);
    lj_widget_invalidate(lj_window_find(win, "sb7"));
    lj_window_paint(win);
    n = lj_window_dirty(win, r, 4);
    CHECK(n == 1 && r[0].x == 246 && r[0].y == 47 && r[0].w == 51 && r[0].h == 25);
    lj_window_set_caption(win, "SEQUENCE [0000 No Name]");
    CHECK(strcmp(cs->text, "SEQUENCE [0000 No Name]") == 0);
    lj_window_paint(win);
    CHECK(lj_window_dirty(win, r, 4) == 1);
    CHECK(lj_window_surface(win)->px[5 * 472 + 100] != 0);          /* caption gradient painted */

    /* tab chain: TabOrder within Panel1 (ES 0, EF 1), only TabStop widgets */
    n = lj_focus_order(root, chain, 64);
    CHECK(n >= 2 && n < 64);
    for (i = 0; i + 1 < n; i++) if (chain[i]->parent == chain[i + 1]->parent) CHECK(chain[i]->tab_order <= chain[i + 1]->tab_order);
    for (i = 0; i < n; i++) CHECK(chain[i]->tabstop && chain[i]->visible && chain[i]->enabled);
    w = lj_window_find(win, "ES");
    CHECK(n > 0 && chain[0] == w);
    CHECK(lj_focus_next(root, w, 0) == lj_window_find(win, "EF"));
    CHECK(lj_focus_next(root, chain[n - 1], 0) == chain[0]);         /* wraps */
    CHECK(lj_focus_next(root, chain[0], 1) == chain[n - 1]);
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_KEY_DOWN; ev.u.key.type = LJ_EV_KEY_DOWN; ev.u.key.key = 0x09;
    lj_window_event(win, &ev);
    CHECK(lj_widget_focused(root) == chain[0]);
    lj_window_event(win, &ev);
    CHECK(lj_widget_focused(root) == chain[1 % n]);
    ev.u.key.shift = 1;
    lj_window_event(win, &ev);
    CHECK(lj_widget_focused(root) == chain[0]);

    /* mouse: click on a stub / plain widget fires OnClick through the binding (cbSync has a class, so use the window events) */
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_FOCUS_IN; lj_window_event(win, &ev);
    CHECK(hits[0] == 1 && cs->active);
    ev.type = LJ_EV_CLOSE; lj_window_event(win, &ev);
    CHECK(hits[1] == 1);
    ev.type = LJ_EV_RESIZE; ev.u.size.w = 500; ev.u.size.h = 200; lj_window_event(win, &ev);
    CHECK(root->w == 500 && root->h == 200 && lj_window_surface(win)->w == 506);
    CHECK(lj_window_resize(win, 466, 163) == 0 && root->w == 466);

    /* native frame: no frame, no caption, the bar stays */
    lj_window_set_native_frame(win, 1);
    lj_window_client_rect(win, &cr);
    CHECK(cr.x == 0 && cr.y == 19 && lj_window_surface(win)->w == 466 && lj_window_surface(win)->h == 182);
    CHECK(lj_window_hit(win, 10, 5) == LJ_HIT_MENU && lj_window_hit(win, 10, 30) == LJ_HIT_CLIENT);
    lj_window_destroy(win);
    CHECK(hits[3] == 1);

    /* sizeable copy: resize edges, min/max buttons; icons-less copy: (10,5) drags */
    d2 = lj_form_TSEQUENCEFORM; d2.border = LJ_BS_SIZEABLE; d2.icons = 0;
    win = lj_window_create(&d2, NULL);
    cs = lj_window_caption(win);
    CHECK(cs->frame_w == 4 && cs->icons == (LJ_BI_SYSMENU | LJ_BI_MINIMIZE | LJ_BI_MAXIMIZE));
    CHECK(lj_window_surface(win)->w == 474);
    CHECK(lj_window_hit(win, 0, 0) == LJ_HIT_RESIZE_TL && lj_window_hit(win, 237, 1) == LJ_HIT_RESIZE_T && lj_window_hit(win, 473, 1) == LJ_HIT_RESIZE_TR);
    CHECK(lj_window_hit(win, 472, 100) == LJ_HIT_RESIZE_R && lj_window_hit(win, 473, 208) == LJ_HIT_RESIZE_BR && lj_window_hit(win, 237, 207) == LJ_HIT_RESIZE_B);
    CHECK(lj_window_hit(win, 2, 208) == LJ_HIT_RESIZE_BL && lj_window_hit(win, 1, 100) == LJ_HIT_RESIZE_L);
    CHECK(lj_frame_button_at(cs, 474, 474 - 4 - 2 - 8, 10) == LJ_FBTN_CLOSE);
    CHECK(lj_frame_button_at(cs, 474, 474 - 4 - 2 - 26, 10) == LJ_FBTN_MAX);
    CHECK(lj_frame_button_at(cs, 474, 474 - 4 - 2 - 42, 10) == LJ_FBTN_MIN);
    lj_window_destroy(win);
    d2.border = LJ_BS_SINGLE; d2.icons = LJ_BI_GIVEN;
    win = lj_window_create(&d2, NULL);
    CHECK(lj_window_caption(win)->icons == 0 && lj_window_hit(win, 10, 5) == LJ_HIT_CAPTION);
    lj_window_destroy(win);
    d2.border = LJ_BS_TOOLWINDOW; d2.icons = LJ_BI_GIVEN | LJ_BI_SYSMENU;
    win = lj_window_create(&d2, NULL);
    CHECK(lj_window_caption(win)->caption_h == 15 && lj_window_caption(win)->frame_w == 3 && lj_window_hit(win, 10, 5) == LJ_HIT_CAPTION);
    lj_window_destroy(win);

    /* ---- the mini form: Enter / Esc, drag and drop ---- */
    mw = lj_window_create(&mini, mini_b);
    CHECK(mw != NULL);
    if (!mw) return 1;
    root = lj_window_root(mw);
    CHECK(lj_focus_default(root) == lj_window_find(mw, "OK") && lj_focus_cancel(root) == lj_window_find(mw, "Cancel"));
    n = lj_focus_order(root, chain, 64);
    CHECK(n == 4 && chain[0] == lj_window_find(mw, "E2") && chain[1] == lj_window_find(mw, "E1"));   /* E2 has TabOrder 0 */
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_KEY_DOWN; ev.u.key.type = LJ_EV_KEY_DOWN; ev.u.key.key = 0x1B;
    lj_window_event(mw, &ev);
    CHECK(hits[11] == 1);                                           /* Esc -> Cancel.OnClick */
    lj_focus_set(lj_window_find(mw, "Drop"));
    ev.u.key.key = 0x0D;
    lj_window_event(mw, &ev);
    CHECK(hits[10] == 1);                                           /* Enter -> OK.OnClick */

    w = lj_window_find(mw, "E1");
    CHECK(lj_dnd_begin(w, 7, 42) == 0 && lj_dnd_active(mw));
    CHECK(lj_dnd_begin(w, 1, 1) == -1);
    lj_window_client_rect(mw, &cr);
    lj_dnd_move(mw, cr.x + 120, cr.y + 30);                         /* headless: screen == surface coords */
    CHECK(hits[12] == 1);
    lj_dnd_end(mw, cr.x + 120, cr.y + 30, 1);
    CHECK(hits[9] == 1 && drop_kind == 7 && drop_no == 42 && drop_x == 20 && drop_y == 20);
    CHECK(!lj_dnd_active(mw) && lj_dnd_info(mw, NULL, NULL, NULL, NULL) == -1);
    /* the same through mouse events: down on E1, begin, move, up over Drop */
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.type = LJ_EV_MOUSE_DOWN; ev.u.mouse.button = 1; ev.u.mouse.clicks = 1; ev.u.mouse.x = cr.x + 20; ev.u.mouse.y = cr.y + 15;
    lj_window_event(mw, &ev);
    CHECK(lj_dnd_begin(w, 2, 3) == 0);
    ev.type = LJ_EV_MOUSE_UP; ev.u.mouse.type = LJ_EV_MOUSE_UP; ev.u.mouse.x = cr.x + 150; ev.u.mouse.y = cr.y + 20;
    lj_window_event(mw, &ev);
    CHECK(hits[9] == 2 && drop_kind == 2 && drop_no == 3 && drop_x == 50 && drop_y == 10);
    lj_window_destroy(mw);

    /* ME-16: an incomplete table is logged once per name (and asserts in debug builds, so release only) */
#ifdef NDEBUG
    {
        lj_binding bad[] = { { "OKClick", count, &hits[13] }, { "Nope", count, &hits[13] }, { NULL, NULL, NULL } };
        lj_log_set_sink(sink, NULL);
        mw = lj_window_create(&mini, bad);
        lj_log_set_sink(NULL, NULL);
        CHECK(mw != NULL);
        CHECK(logged == 4);                                         /* CancelClick, DropDrop, DropOver unbound + Nope unmatched */
        lj_window_destroy(mw);
    }
#endif
    CHECK(lj_window_create(NULL, NULL) == NULL);
    lj_window_destroy(NULL);

    printf("window: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
