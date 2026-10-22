/* lj_statusbar.c - status bar widget (lj_statusbar.h): panels measured from
 * statusbar.gif 400x17 at 4x, rig item; the memory gauge colours are CHM 1661-1669. */
#include "desktop/lj_statusbar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "io/lj_fn.h"
#include "ui/widgets/lj_widgets_lists.h"

typedef struct { lj_widget base; char text[LJ_SB_COUNT][32]; uint8_t flag[LJ_SB_COUNT]; } lj_sb;

static const struct { int w; const char *caption; } lj_sb_panels[LJ_SB_COUNT] = {   /* PROPOSAL rig (ME-87): widths scaled from statusbar.gif (400 px, a ~0.6 reduction) onto the 791-px client, not measured at 1:1 */
    { 72, "" }, { 84, "Running.." }, { 56, "" }, { 96, "Master 100%" }, { 92, "Seq. Modified" }, { 92, "Cue Modified" }, { 108, "CueList Modified" }, { 0, "Trig" },
};
static const lj_ctl_desc lj_sb_desc = { LJ_K_STATUSBAR, "sb_status", 0, 0, 791, LJ_SB_H, .align = LJ_AL_BOTTOM, .font = LJ_FONT_ARIAL14 };
static lj_sb *lj_the_sb;

static int memory_pct(const lj_sb *b)
{
    const char *t = b->text[LJ_SB_MEMORY];
    int v = atoi(t);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_sb *b = (lj_sb *)w;
    const lj_font *f = lj_w_font_of(w);
    int ax, ay, x, i, ph = w->h - 3, ty;
    lj_widget_abs(w, &ax, &ay);
    lj_fill(s, ax, ay, w->w, w->h, w->color);
    x = ax; ty = ay + 2 + (ph - f->height) / 2;
    for (i = 0; i < LJ_SB_COUNT; i++) {
        int pw = i == LJ_SB_COUNT - 1 ? ax + w->w - x : lj_sb_panels[i].w;
        const char *t = b->text[i][0] ? b->text[i] : lj_sb_panels[i].caption;
        uint32_t ink = LJ_CLR_3DDKSHADOW;
        if (pw <= 0) break;
        lj_bevel1(s, x, ay + 2, pw, ph, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
        if (i == LJ_SB_MEMORY && b->text[i][0]) {                      /* memory gauge: fill proportional, green > 3 MB free .. red < 1 MB (CHM 1661-1669); the app sets "79%" and the band */
            int pct = memory_pct(b), band = b->flag[i];
            uint32_t rgb = band == 3 ? LJ_CLR_RED : band == 2 ? 0xFF8000u : band == 1 ? LJ_CLR_YELLOW : LJ_CLR_GREEN;
            lj_fill(s, x + 1, ay + 3, (pw - 2) * pct / 100, ph - 2, rgb);
        }
        if (i == LJ_SB_TRIG && b->flag[i]) { lj_fill(s, x + 1, ay + 3, pw - 2, ph - 2, 0x0000FFu); ink = LJ_CLR_BTNHIGHLIGHT; }   /* Trig: blue fill, white text */
        if (i >= LJ_SB_SEQ_MOD && i <= LJ_SB_CL_MOD && !b->flag[i]) t = "";   /* the Modified panels are blank until set */
        if (t && *t && lj_text_width(f, t) + 6 <= pw) lj_text(s, f, x + 3, ty, t, ink, 0);
        x += pw + 2;
    }
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int x = 0, i;
    if (m->type != LJ_EV_MOUSE_DOWN || m->button != 1 || m->clicks != 2) return 0;
    for (i = 0; i < LJ_SB_MASTER; i++) x += lj_sb_panels[i].w + 2;
    if (m->x >= x && m->x < x + lj_sb_panels[LJ_SB_MASTER].w) { lj_fn_run(LJ_FN_TOGGLE_MASTER_INTENSITY, 1); return 1; }   /* double-click Master opens Master Intensity (RN 2.5) [F10] */
    (void)w;
    return 0;
}

static const lj_widget_vt vt = { paint, mouse, NULL, NULL };

void lj_statusbar_build(lj_widget *root)
{
    lj_sb *b = lj_w_alloc(sizeof *b, root, &lj_sb_desc, &vt, 0);
    lj_the_sb = b;
    lj_statusbar_refresh();
}

void lj_statusbar_refresh(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char buf[32];
    int gm;
    if (!lj_the_sb) return;
    if (lt) { snprintf(buf, sizeof buf, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec); lj_desktop_status_set(LJ_SB_CLOCK, buf); }
    gm = ctx->view.tick_no ? (ctx->view.gm * 100 + 127) / 255 : 100;   /* 0..255 -> % ; 100 % before the first engine view */
    snprintf(buf, sizeof buf, "Master %d%%", gm);
    lj_desktop_status_set(LJ_SB_MASTER, buf);
    lj_desktop_status_flag(LJ_SB_SEQ_MOD, ctx->modified[LJ_MOD_SEQ]);
    lj_desktop_status_flag(LJ_SB_CUE_MOD, ctx->modified[LJ_MOD_CUE]);
    lj_desktop_status_flag(LJ_SB_CL_MOD, ctx->modified[LJ_MOD_CL]);
}

void lj_desktop_status_set(enum lj_status_field f, const char *text)
{
    if (!lj_the_sb || (int)f < 0 || f >= LJ_SB_COUNT) return;
    if (strcmp(lj_the_sb->text[f], text ? text : "") == 0) return;
    lj_str_copy(lj_the_sb->text[f], sizeof lj_the_sb->text[f], text ? text : "");
    lj_widget_invalidate(&lj_the_sb->base);
}

void lj_desktop_status_flag(enum lj_status_field f, int on)
{
    if (!lj_the_sb || (int)f < 0 || f >= LJ_SB_COUNT) return;
    if (lj_the_sb->flag[f] == (uint8_t)on) return;
    lj_the_sb->flag[f] = (uint8_t)on;   /* LJ_SB_MEMORY: the colour band 0 green 1 yellow 2 orange 3 red */
    lj_widget_invalidate(&lj_the_sb->base);
}

void lj_statusbar_show(int show)
{
    lj_window *win;
    if (!lj_the_sb) return;
    lj_the_sb->base.visible = show != 0;
    win = lj_window_of(&lj_the_sb->base);
    if (win) { lj_widget_layout(lj_window_root(win)); lj_window_invalidate_all(win); }
}

int lj_statusbar_visible(void) { return lj_the_sb ? (int)lj_the_sb->base.visible : 0; }

const char *lj_statusbar_panel_text(enum lj_status_field f)
{
    if (!lj_the_sb || (int)f < 0 || f >= LJ_SB_COUNT) return "";
    if (f >= LJ_SB_SEQ_MOD && f <= LJ_SB_CL_MOD && !lj_the_sb->flag[f]) return "";
    return lj_the_sb->text[f][0] ? lj_the_sb->text[f] : lj_sb_panels[f].caption;
}
