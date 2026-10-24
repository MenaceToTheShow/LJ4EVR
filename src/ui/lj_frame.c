/* lj_frame.c - classic frame painting and hit-testing (design 7.3, 7.5).
 * Metrics (Win98 at 96 dpi): caption 18 px + 1 px separator (SM_CYCAPTION 19),
 * tool-window caption 14 + 1 (SM_CYSMCAPTION 15), sizing border 4 px, dialog /
 * single border 3 px, 16x16 icon slot at x = 2, buttons 16x14 raised. Icons and
 * button glyphs are drawn with lines: no Martin artwork [D2]. */
#include "ui/lj_frame.h"

#include <string.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"

#define LJ_CAPTION_INACTIVE_START 0x808080u
#define LJ_CAPTION_INACTIVE_END   0xB5B5B5u

static const lj_menu_item_desc lj_frame_sysitems[] = {
    { "SysRestore",  "&Restore",  .tag = LJ_SYS_RESTORE },
    { "SysMove",     "&Move",     .tag = LJ_SYS_MOVE },
    { "SysSize",     "&Size",     .tag = LJ_SYS_SIZE },
    { "SysMinimize", "Mi&nimize", .tag = LJ_SYS_MINIMIZE },
    { "SysMaximize", "Ma&ximize", .tag = LJ_SYS_MAXIMIZE },
    { "SysSep",      "-",        .tag = 0 },
    { "SysClose",    "&Close",    .tag = LJ_SYS_CLOSE, .shortcut = 0x8000 | 0x73 /* Alt+F4 */ },
};
const lj_menu_desc lj_frame_sysmenu = { "SysMenu", 0, lj_frame_sysitems, LJ_COUNT_OF(lj_frame_sysitems) };

static int sizeable(const lj_window_caption_state *cs) { return cs->border == LJ_BS_SIZEABLE || cs->border == LJ_BS_SIZETOOLWIN; }
static int toolwin(const lj_window_caption_state *cs)  { return cs->border == LJ_BS_TOOLWINDOW || cs->border == LJ_BS_SIZETOOLWIN; }
static int has_sysmenu(const lj_window_caption_state *cs) { return (cs->icons & LJ_BI_SYSMENU) != 0; }

void lj_frame_metrics(lj_window_caption_state *cs)
{
    if (cs->native_frame || cs->border == LJ_BS_NONE) { cs->frame_w = 0; cs->caption_h = 0; return; }
    cs->frame_w = sizeable(cs) ? 4 : 3;
    cs->caption_h = toolwin(cs) ? 15 : 19;
}

/* Buttons sit inside the caption at the right: close, then (non-tool windows)
 * a 2-px gap, maximize, minimize. 16x14 at y = top + 2. */
int lj_frame_button_rect(const lj_window_caption_state *cs, int sw, int btn, lj_rect *out)
{
    int right = sw - cs->frame_w - 2, top = cs->frame_w + 2;
    if (!has_sysmenu(cs) || cs->caption_h == 0) return -1;
    if (toolwin(cs)) { if (btn != LJ_FBTN_CLOSE) return -1; top = cs->frame_w + 1; }
    else if (btn != LJ_FBTN_CLOSE && !(cs->icons & (LJ_BI_MINIMIZE | LJ_BI_MAXIMIZE))) return -1;
    if (btn == LJ_FBTN_CLOSE) { out->x = right - 16; }
    else if (btn == LJ_FBTN_MAX) { out->x = right - 16 - 2 - 16; }
    else if (btn == LJ_FBTN_MIN) { out->x = right - 16 - 2 - 32; }
    else return -1;
    out->y = top; out->w = 16; out->h = toolwin(cs) ? 12 : 14;
    return 0;
}

int lj_frame_button_at(const lj_window_caption_state *cs, int sw, int x, int y)
{
    int b;
    lj_rect r;
    for (b = 0; b < LJ_FBTN_COUNT; b++)
        if (lj_frame_button_rect(cs, sw, b, &r) == 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return b;
    return LJ_FBTN_NONE;
}

static int in_icon(const lj_window_caption_state *cs, int x, int y)
{
    int ix = cs->frame_w + 2, iy = cs->frame_w + 1;
    if (!has_sysmenu(cs) || toolwin(cs)) return 0;
    return x >= ix && x < ix + 16 && y >= iy && y < iy + 16;
}

lj_hit lj_frame_hit(const lj_window_caption_state *cs, int sw, int sh, int x, int y)
{
    int f = cs->frame_w, cap = cs->caption_h;
    if (x < 0 || y < 0 || x >= sw || y >= sh) return LJ_HIT_CLIENT;
    if (x < f || y < f || x >= sw - f || y >= sh - f) {
        int left = x < f, right = x >= sw - f, top = y < f, bottom = y >= sh - f;
        if (!sizeable(cs)) return LJ_HIT_FRAME;
        /* corners are the 16-px ends of each edge, as Windows does it */
        if (top || bottom) { if (x < f + 16) left = 1; if (x >= sw - f - 16) right = 1; }
        if (left || right)  { if (y < f + 16) top = 1;  if (y >= sh - f - 16) bottom = 1; }
        if (top)    return left ? LJ_HIT_RESIZE_TL : right ? LJ_HIT_RESIZE_TR : LJ_HIT_RESIZE_T;
        if (bottom) return left ? LJ_HIT_RESIZE_BL : right ? LJ_HIT_RESIZE_BR : LJ_HIT_RESIZE_B;
        return left ? LJ_HIT_RESIZE_L : LJ_HIT_RESIZE_R;
    }
    if (y < f + cap) {
        if (in_icon(cs, x, y)) return LJ_HIT_SYSMENU;
        if (lj_frame_button_at(cs, sw, x, y) != LJ_FBTN_NONE) return LJ_HIT_BUTTON;
        return LJ_HIT_CAPTION;
    }
    if (y < f + cap + cs->menu_h) return LJ_HIT_MENU;
    return LJ_HIT_CLIENT;
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, int i, int n)
{
    uint32_t out = 0;
    int sh;
    if (n <= 1) return a;
    for (sh = 0; sh <= 16; sh += 8) {
        int ca = (int)((a >> sh) & 0xFF), cb = (int)((b >> sh) & 0xFF);
        out |= (uint32_t)(ca + (cb - ca) * i / (n - 1)) << sh;
    }
    return out;
}

static void glyph(lj_surface *s, int btn, int x, int y, int maximized, uint32_t rgb)
{
    /* 16x14 button; glyphs as the classic ones: 8-wide X, 6x2 bar, 9x9 box with a 2-px lid */
    int i;
    switch (btn) {
    case LJ_FBTN_CLOSE:
        for (i = 0; i < 7; i++) { lj_hline(s, x + 4 + i, y + 3 + i, 2, rgb); lj_hline(s, x + 10 - i, y + 3 + i, 2, rgb); }
        break;
    case LJ_FBTN_MIN:
        lj_fill(s, x + 4, y + 9, 6, 2, rgb);
        break;
    case LJ_FBTN_MAX:
        if (!maximized) { lj_fill(s, x + 3, y + 2, 9, 2, rgb); lj_hline(s, x + 3, y + 10, 9, rgb); lj_vline(s, x + 3, y + 2, 9, rgb); lj_vline(s, x + 11, y + 2, 9, rgb); }
        else {           /* restore: two overlapping boxes */
            lj_fill(s, x + 5, y + 2, 6, 2, rgb); lj_vline(s, x + 10, y + 2, 5, rgb); lj_hline(s, x + 8, y + 6, 3, rgb);
            lj_fill(s, x + 3, y + 5, 6, 2, rgb); lj_hline(s, x + 3, y + 10, 6, rgb); lj_vline(s, x + 3, y + 5, 6, rgb); lj_vline(s, x + 8, y + 5, 6, rgb);
        }
        break;
    default: break;
    }
}

void lj_frame_paint(lj_surface *s, const lj_window_caption_state *cs, int pressed)
{
    int f = cs->frame_w, cap = cs->caption_h, sw = s->w, sh = s->h, i, b, tx;
    uint32_t c0, c1;
    lj_rect r;
    if (f == 0 && cap == 0) return;
    /* border: white/black outer bevel, face inside (the 2 extra px of a sizing border included) */
    for (i = 0; i < f; i++) {
        if (i == 0) lj_bevel1(s, 0, 0, sw, sh, LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW);
        else if (i == 1) lj_bevel1(s, 1, 1, sw - 2, sh - 2, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
        else lj_bevel1(s, i, i, sw - 2 * i, sh - 2 * i, LJ_CLR_BTNFACE, LJ_CLR_BTNFACE);
    }
    if (cap == 0) return;
    /* caption gradient, then the 1-px face separator below it */
    c0 = cs->active ? LJ_CLR_HIGHLIGHT : LJ_CAPTION_INACTIVE_START;
    c1 = cs->active ? LJ_CLR_CAPTION_END : LJ_CAPTION_INACTIVE_END;
    for (i = 0; i < sw - 2 * f; i++) lj_vline(s, f + i, f, cap - 1, lerp_rgb(c0, c1, i, sw - 2 * f));
    lj_hline(s, f, f + cap - 1, sw - 2 * f, LJ_CLR_BTNFACE);
    tx = f + 2;
    if (has_sysmenu(cs) && !toolwin(cs)) {   /* generic 16x16 icon: a white sheet with a blue title bar, ours */
        lj_fill(s, f + 2, f + 1, 16, 16, LJ_CLR_BTNFACE);
        lj_fill(s, f + 3, f + 2, 14, 14, LJ_CLR_BTNHIGHLIGHT);
        lj_bevel1(s, f + 3, f + 2, 14, 14, LJ_CLR_3DDKSHADOW, LJ_CLR_3DDKSHADOW);
        lj_fill(s, f + 4, f + 3, 12, 3, LJ_CLR_HIGHLIGHT);
        tx = f + 2 + 16 + 2;
    }
    lj_text(s, &lj_font_sans13b, tx, f + (cap - 1 - 13) / 2, cs->text, LJ_CLR_BTNHIGHLIGHT, 0);
    for (b = 0; b < LJ_FBTN_COUNT; b++) {
        int enabled = 1;
        if (lj_frame_button_rect(cs, sw, b, &r) != 0) continue;
        if (b == LJ_FBTN_MIN) enabled = (cs->icons & LJ_BI_MINIMIZE) != 0;
        if (b == LJ_FBTN_MAX) enabled = (cs->icons & LJ_BI_MAXIMIZE) != 0;
        if (pressed == b) {
            lj_fill(s, r.x, r.y, r.w, r.h, LJ_CLR_BTNFACE);
            lj_bevel1(s, r.x, r.y, r.w, r.h, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT);
            lj_bevel1(s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, LJ_CLR_3DDKSHADOW, LJ_CLR_BTNFACE);
            glyph(s, b, r.x + 1, r.y + 1, cs->maximized, LJ_CLR_3DDKSHADOW);
        } else {
            lj_button_raised(s, r.x, r.y, r.w, r.h);
            lj_hline(s, r.x + 1, r.y + 1, r.w - 3, LJ_CLR_3DLIGHT); lj_vline(s, r.x + 1, r.y + 1, r.h - 3, LJ_CLR_3DLIGHT);   /* W5-INT ME-161-3DLIGHT: movement2.gif close button */
            if (enabled) glyph(s, b, r.x, r.y, cs->maximized, LJ_CLR_3DDKSHADOW);
            else { glyph(s, b, r.x + 1, r.y + 1, cs->maximized, LJ_CLR_BTNHIGHLIGHT); glyph(s, b, r.x, r.y, cs->maximized, LJ_CLR_BTNSHADOW); }
        }
    }
}
