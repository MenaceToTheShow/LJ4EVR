/* lj_menu.c - classic menus: bar, drop-downs, submenus, popups, keyboard navigation (design 7.3, 7.9). */
#include "ui/lj_menu.h"

#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"

#define BORDER 3
#define GUTTER 20
#define ARROW_W 18

static const lj_font *menu_font(void) { return &lj_font_sans13; }

static int is_sep(const lj_menu_item_desc *it) { return it->caption && it->caption[0] == '-' && it->caption[1] == 0; }

int lj_menu_accel(const char *caption)
{
    const char *p;
    if (!caption) return 0;
    for (p = caption; *p; p++) {
        if (*p != '&') continue;
        if (p[1] == '&') { p++; continue; }
        if (p[1] >= 'A' && p[1] <= 'Z') return p[1] + 32;
        return (unsigned char)p[1];
    }
    return 0;
}

/* ---- runtime table ---- */

static int count_items(const lj_menu_item_desc *items, int n)
{
    int i, t = n;
    for (i = 0; i < n; i++) if (items[i].children) t += count_items(items[i].children, items[i].nchildren);
    return t;
}

static int fill_rt(lj_menu_rt *rt, int k, const lj_menu_item_desc *items, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        rt[k].item = &items[i];
        rt[k].checked = lj_desc_bool(items[i].checked, 0) ? 1u : 0u;
        rt[k].enabled = lj_desc_bool(items[i].enabled, 1) ? 1u : 0u;
        rt[k].visible = lj_desc_bool(items[i].visible, 1) ? 1u : 0u;
        k++;
        if (items[i].children) k = fill_rt(rt, k, items[i].children, items[i].nchildren);
    }
    return k;
}

int lj_menu_init(lj_menu *m, lj_window *owner, const lj_form_desc *form, const lj_menu_desc *bar, const lj_menu_desc *extra)
{
    int i, total = 0, k = 0;
    memset(m, 0, sizeof *m);
    m->owner = owner; m->bar = bar; m->bar_sel = -1;
    for (i = 0; i < LJ_MENU_LEVELS; i++) m->lv[i].sel = -1;
    for (i = 0; form && i < form->nmenus; i++) total += count_items(form->menus[i].items, form->menus[i].n);
    if (extra) total += count_items(extra->items, extra->n);
    if (total) {
        m->rt = (lj_menu_rt *)calloc((size_t)total, sizeof *m->rt);
        if (!m->rt) lj_fatal("lj_menu: out of memory");
        for (i = 0; form && i < form->nmenus; i++) k = fill_rt(m->rt, k, form->menus[i].items, form->menus[i].n);
        if (extra) k = fill_rt(m->rt, k, extra->items, extra->n);
    }
    m->nrt = total;
    return 0;
}

void lj_menu_free(lj_menu *m)
{
    if (!m) return;
    lj_menu_close(m);
    free(m->rt);
    m->rt = NULL; m->nrt = 0;
}

lj_menu_rt *lj_menu_rt_find(lj_menu *m, const lj_menu_item_desc *item)
{
    int i;
    for (i = 0; i < m->nrt; i++) if (m->rt[i].item == item) return &m->rt[i];
    return NULL;
}

lj_menu_rt *lj_menu_rt_named(lj_menu *m, const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = 0; i < m->nrt; i++) if (m->rt[i].item->name && strcmp(m->rt[i].item->name, name) == 0) return &m->rt[i];
    return NULL;
}

int lj_menu_set_checked(lj_menu *m, const char *name, int v) { lj_menu_rt *r = lj_menu_rt_named(m, name); if (!r) return -1; r->checked = v ? 1u : 0u; m->dirty = 1; return 0; }
int lj_menu_set_enabled(lj_menu *m, const char *name, int v) { lj_menu_rt *r = lj_menu_rt_named(m, name); if (!r) return -1; r->enabled = v ? 1u : 0u; m->dirty = 1; return 0; }
int lj_menu_set_visible(lj_menu *m, const char *name, int v) { lj_menu_rt *r = lj_menu_rt_named(m, name); if (!r) return -1; r->visible = v ? 1u : 0u; m->dirty = 1; return 0; }

static const lj_menu_rt *rt_of(const lj_menu *m, const lj_menu_item_desc *it)
{
    int i;
    for (i = 0; i < m->nrt; i++) if (m->rt[i].item == it) return &m->rt[i];
    return NULL;
}
static int item_visible(const lj_menu *m, const lj_menu_item_desc *it) { const lj_menu_rt *r = rt_of(m, it); return r ? (int)r->visible : 1; }
static int item_enabled(const lj_menu *m, const lj_menu_item_desc *it) { const lj_menu_rt *r = rt_of(m, it); return r ? (int)r->enabled : 1; }

/* ---- shortcut text ---- */

int lj_menu_shortcut_text(int shortcut, char *out, size_t cap)
{
    static const struct { int vk; const char *name; } names[] = {
        { 0x08, "BkSp" }, { 0x09, "Tab" }, { 0x0D, "Enter" }, { 0x1B, "Esc" }, { 0x20, "Space" },
        { 0x21, "PgUp" }, { 0x22, "PgDn" }, { 0x23, "End" }, { 0x24, "Home" }, { 0x25, "Left" }, { 0x26, "Up" },
        { 0x27, "Right" }, { 0x28, "Down" }, { 0x2D, "Ins" }, { 0x2E, "Del" },
    };
    int vk = shortcut & 0xFF, i;
    char key[8];
    const char *kn = NULL;
    if (!out || cap == 0) return -1;
    out[0] = 0;
    if (vk == 0) return -1;
    for (i = 0; i < LJ_COUNT_OF(names); i++) if (names[i].vk == vk) kn = names[i].name;
    if (!kn) {
        if (vk >= 0x70 && vk <= 0x87) { int f = vk - 0x6F; key[0] = 'F'; key[1] = (char)(f >= 10 ? '0' + f / 10 : '0' + f); key[2] = (char)(f >= 10 ? '0' + f % 10 : 0); key[3] = 0; }   /* F1..F24 (ME-64: F20..F24 were "F1:".."F1>") */
        else if (vk >= 0x60 && vk <= 0x69) { lj_str_copy(key, sizeof key, "Num "); key[4] = (char)('0' + vk - 0x60); key[5] = 0; }
        else { key[0] = (char)vk; key[1] = 0; }
        kn = key;
    }
    if (shortcut & 0x2000) lj_str_copy(out, cap, "Shift+");
    if (shortcut & 0x4000) { size_t n = strlen(out); lj_str_copy(out + n, cap - n, "Ctrl+"); }
    if (shortcut & 0x8000) { size_t n = strlen(out); lj_str_copy(out + n, cap - n, "Alt+"); }
    { size_t n = strlen(out); lj_str_copy(out + n, cap - n, kn); }
    return 0;
}

/* ---- geometry ---- */

static int owner_screen(lj_menu *m, int *x, int *y)
{
    *x = 0; *y = 0;
    if (m->host && m->host->screen_pos) return m->host->screen_pos(m->host->ud, m->owner, x, y);
    return -1;
}

int lj_menu_bar_item_rect(const lj_menu *m, int i, lj_rect *r)
{
    int k, x = m->bar_x;
    if (!m->bar) return -1;
    for (k = 0; k < m->bar->n; k++) {
        const lj_menu_item_desc *it = &m->bar->items[k];
        int w;
        if (!item_visible(m, it)) { if (k == i) return -1; continue; }
        w = lj_text_width(menu_font(), it->caption ? it->caption : "") + 12;   /* 6-px padding each side (ME-39, 1:1 captures) */
        if (k == i) { r->x = x; r->y = m->bar_y; r->w = w; r->h = LJ_MENU_BAR_H; return 0; }
        x += w;
    }
    return -1;
}

int lj_menu_bar_item_at(const lj_menu *m, int x, int y)
{
    int i;
    lj_rect r;
    if (!m->bar || y < m->bar_y || y >= m->bar_y + LJ_MENU_BAR_H) return -1;
    for (i = 0; i < m->bar->n; i++)
        if (lj_menu_bar_item_rect(m, i, &r) == 0 && x >= r.x && x < r.x + r.w) return i;
    return -1;
}

static int item_h(const lj_menu_item_desc *it) { return is_sep(it) ? LJ_MENU_SEP_H : LJ_MENU_ITEM_H; }

/* y of item i inside the level surface, or the level height when i == n */
static int level_item_y(lj_menu *m, const lj_menu_level *lv, int i)
{
    int k, y = BORDER;
    for (k = 0; k < i && k < lv->n; k++) if (item_visible(m, &lv->items[k])) y += item_h(&lv->items[k]);
    return y;
}

static int level_item_at(lj_menu *m, const lj_menu_level *lv, int y)
{
    int k, top = BORDER;
    for (k = 0; k < lv->n; k++) {
        if (!item_visible(m, &lv->items[k])) continue;
        if (y >= top && y < top + item_h(&lv->items[k])) return k;
        top += item_h(&lv->items[k]);
    }
    return -1;
}

static void level_paint(lj_menu *m, lj_menu_level *lv)
{
    lj_surface *s = &lv->s;
    const lj_font *f = menu_font();
    int k, y = BORDER, i;
    char sc[32];
    lj_fill(s, 0, 0, s->w, s->h, LJ_CLR_BTNFACE);
    lj_bevel1(s, 0, 0, s->w, s->h, LJ_CLR_BTNFACE, LJ_CLR_3DDKSHADOW);
    lj_bevel1(s, 1, 1, s->w - 2, s->h - 2, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW);
    for (k = 0; k < lv->n; k++) {
        const lj_menu_item_desc *it = &lv->items[k];
        lj_menu_rt *rt = lj_menu_rt_find(m, it);
        int sel = k == lv->sel, en = item_enabled(m, it), h = item_h(it);
        uint32_t tc;
        if (!item_visible(m, it)) continue;
        if (is_sep(it)) {
            lj_hline(s, BORDER + 1, y + 4, s->w - 2 * BORDER - 2, LJ_CLR_BTNSHADOW);
            lj_hline(s, BORDER + 1, y + 5, s->w - 2 * BORDER - 2, LJ_CLR_BTNHIGHLIGHT);
            y += h;
            continue;
        }
        if (sel) lj_fill(s, BORDER, y, s->w - 2 * BORDER, h, LJ_CLR_HIGHLIGHT);
        tc = sel ? LJ_CLR_BTNHIGHLIGHT : en ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW;
        if (!en && !sel) lj_text(s, f, BORDER + GUTTER + 1, y + 3, it->caption ? it->caption : "", LJ_CLR_BTNHIGHLIGHT, 1);
        lj_text(s, f, BORDER + GUTTER, y + 2, it->caption ? it->caption : "", tc, 1);
        if (it->shortcut && lj_menu_shortcut_text(it->shortcut, sc, sizeof sc) == 0) {
            int sw = lj_text_width(f, sc), sx = s->w - BORDER - ARROW_W - sw;
            if (!en && !sel) lj_text(s, f, sx + 1, y + 3, sc, LJ_CLR_BTNHIGHLIGHT, 0);
            lj_text(s, f, sx, y + 2, sc, tc, 0);
        }
        if (rt && rt->checked) {
            int cx = BORDER + 6, cy = y + 5;
            if (lj_desc_bool(it->radio_item, 0)) {   /* 6-px dot */
                lj_hline(s, cx + 2, cy, 2, tc); lj_hline(s, cx + 1, cy + 1, 4, tc); lj_fill(s, cx, cy + 2, 6, 2, tc);
                lj_hline(s, cx + 1, cy + 4, 4, tc); lj_hline(s, cx + 2, cy + 5, 2, tc);
            } else {                                  /* 7x7 check as the classic TCheckBox glyph */
                for (i = 0; i < 3; i++) lj_vline(s, cx + i, cy + 2 + i, 3, tc);
                for (i = 0; i < 4; i++) lj_vline(s, cx + 3 + i, cy + 4 - i, 3, tc);
            }
        }
        if (it->children && it->nchildren > 0) {       /* submenu arrow: 4 columns 7,5,3,1 */
            int ax = s->w - BORDER - 10, ay = y + 5;
            for (i = 0; i < 4; i++) lj_vline(s, ax + i, ay + i, 7 - 2 * i, tc);
        }
        y += h;
    }
}

static void level_layout(lj_menu *m, lj_menu_level *lv)
{
    const lj_font *f = menu_font();
    int k, tmax = 0, smax = 0, h = 2 * BORDER, w;
    char sc[32];
    for (k = 0; k < lv->n; k++) {
        const lj_menu_item_desc *it = &lv->items[k];
        int tw;
        if (!item_visible(m, it)) continue;
        h += item_h(it);
        tw = lj_text_width(f, it->caption ? it->caption : "");
        if (tw > tmax) tmax = tw;
        if (it->shortcut && lj_menu_shortcut_text(it->shortcut, sc, sizeof sc) == 0) { tw = lj_text_width(f, sc); if (tw > smax) smax = tw; }
    }
    w = 2 * BORDER + GUTTER + tmax + (smax ? 20 + smax : 0) + ARROW_W;
    if (w < 40) w = 40;
    lv->short_x = w - BORDER - ARROW_W;
    lj_surface_free(&lv->s);
    lj_surface_init(&lv->s, w, h);
}

static void level_close(lj_menu *m, int d)
{
    lj_menu_level *lv = &m->lv[d];
    if (lv->handle && m->host && m->host->popup_hide) m->host->popup_hide(m->host->ud, lv->handle);
    lv->handle = NULL;
    lj_surface_free(&lv->s);
    lv->items = NULL; lv->n = 0; lv->sel = -1;
}

static void close_above(lj_menu *m, int keep)
{
    while (m->depth > keep) level_close(m, --m->depth);
}

static void level_show(lj_menu *m, lj_menu_level *lv)
{
    level_paint(m, lv);
    if (!m->host) return;
    if (lv->handle) { if (m->host->popup_update) m->host->popup_update(m->host->ud, lv->handle, &lv->s); }
    else if (m->host->popup_show) lv->handle = m->host->popup_show(m->host->ud, m->owner, &lv->s, lv->x, lv->y);
}

static int level_open(lj_menu *m, const lj_menu_item_desc *items, int n, int sx, int sy)
{
    lj_menu_level *lv;
    if (m->depth >= LJ_MENU_LEVELS) return -1;
    lv = &m->lv[m->depth];
    memset(lv, 0, sizeof *lv);
    lv->items = items; lv->n = n; lv->sel = -1; lv->x = sx; lv->y = sy;
    level_layout(m, lv);
    m->depth++;
    level_show(m, lv);
    return 0;
}

static void level_repaint(lj_menu *m, int d)
{
    if (d < 0 || d >= m->depth) return;
    level_show(m, &m->lv[d]);
}

void lj_menu_close(lj_menu *m)
{
    close_above(m, 0);
    if (m->bar_active || m->bar_open || m->bar_sel >= 0) m->dirty = 1;
    m->bar_active = 0; m->bar_open = 0; m->bar_sel = -1;
}

int lj_menu_is_open(const lj_menu *m) { return m->depth > 0 || m->bar_active; }

static int first_visible_bar(lj_menu *m, int from, int step)
{
    int i, n = m->bar ? m->bar->n : 0;
    if (n == 0) return -1;
    for (i = 0; i < n; i++) {
        int k = ((from + step * (i + 1)) % n + n) % n;
        if (item_visible(m, &m->bar->items[k])) return k;
    }
    return -1;
}

void lj_menu_open_bar(lj_menu *m, int i)
{
    lj_rect r;
    int ox, oy;
    const lj_menu_item_desc *it;
    if (!m->bar || i < 0 || i >= m->bar->n || lj_menu_bar_item_rect(m, i, &r) != 0) return;
    close_above(m, 0);
    m->bar_sel = i; m->bar_active = 1; m->dirty = 1;
    it = &m->bar->items[i];
    if (!item_enabled(m, it)) { m->bar_open = 0; return; }
    owner_screen(m, &ox, &oy);
    m->bar_open = 1;
    if (it->children && it->nchildren > 0) {
        level_open(m, it->children, it->nchildren, ox + r.x, oy + r.y + LJ_MENU_BAR_H);
    } else {
        /* a bar item without children fires like a button (TMainMenu top-level OnClick) */
        m->bar_open = 0;
    }
}

void lj_menu_popup(lj_menu *m, const lj_menu_desc *d, int sx, int sy)
{
    if (!d) return;
    lj_menu_close(m);
    level_open(m, d->items, d->n, sx, sy);
}

void lj_menu_activate_bar(lj_menu *m)
{
    if (!m->bar) return;
    if (lj_menu_is_open(m)) { lj_menu_close(m); return; }
    m->bar_active = 1; m->bar_open = 0; m->bar_sel = first_visible_bar(m, -1, 1); m->dirty = 1;
}

/* ---- firing ---- */

static void fire(lj_menu *m, const lj_menu_item_desc *it, const lj_menu_item_desc *siblings, int nsib)
{
    lj_menu_rt *rt = lj_menu_rt_find(m, it);
    if (rt && lj_desc_bool(it->auto_check, 0)) {
        if (lj_desc_bool(it->radio_item, 0)) {
            int k;
            for (k = 0; k < nsib; k++) {
                lj_menu_rt *s = lj_menu_rt_find(m, &siblings[k]);
                if (s && siblings[k].group_index == it->group_index && lj_desc_bool(siblings[k].radio_item, 0)) s->checked = 0;
            }
            rt->checked = 1;
        } else rt->checked = rt->checked ? 0u : 1u;
    }
    lj_menu_close(m);
    if (rt && rt->fn) {
        m->firing = it;
        rt->fn(lj_window_root(m->owner), rt->ud);
        m->firing = NULL;
    }
}

/* Enter / click on the selected item of the top level: fire it or open its submenu. */
static int select_current(lj_menu *m)
{
    lj_menu_level *lv;
    const lj_menu_item_desc *it;
    if (m->depth == 0) return 0;
    lv = &m->lv[m->depth - 1];
    if (lv->sel < 0) return 0;
    it = &lv->items[lv->sel];
    if (is_sep(it) || !item_enabled(m, it)) return 0;
    if (it->children && it->nchildren > 0) {
        if (m->depth < LJ_MENU_LEVELS && level_open(m, it->children, it->nchildren, lv->x + lv->s.w - BORDER, lv->y + level_item_y(m, lv, lv->sel) - BORDER) == 0) {
            lj_menu_level *sub = &m->lv[m->depth - 1];
            int k;
            for (k = 0; k < sub->n; k++) if (item_visible(m, &sub->items[k]) && !is_sep(&sub->items[k])) { sub->sel = k; break; }
            level_repaint(m, m->depth - 1);
        }
        return 1;
    }
    fire(m, it, lv->items, lv->n);
    return 1;
}

static void move_sel(lj_menu *m, int step)
{
    lj_menu_level *lv = &m->lv[m->depth - 1];
    int i, n = lv->n, from = lv->sel >= 0 ? lv->sel : step > 0 ? -1 : n;
    for (i = 0; i < n; i++) {
        int k = ((from + step * (i + 1)) % n + n) % n;
        if (item_visible(m, &lv->items[k]) && !is_sep(&lv->items[k])) { lv->sel = k; break; }
    }
    level_repaint(m, m->depth - 1);
}

/* ---- input ---- */

int lj_menu_bar_mouse(lj_menu *m, const lj_mouse_ev *ev)
{
    int i = lj_menu_bar_item_at(m, ev->x, ev->y);
    if (!m->bar) return 0;
    if (ev->type == LJ_EV_MOUSE_DOWN && ev->button == 1) {
        if (i < 0) { lj_menu_close(m); return 1; }
        if (m->bar_open && m->bar_sel == i) { lj_menu_close(m); return 1; }
        lj_menu_open_bar(m, i);
        if (!m->bar_open && item_enabled(m, &m->bar->items[i])) {   /* childless bar item: fires on the click */
            m->bar_active = 0; m->bar_sel = -1;
            fire(m, &m->bar->items[i], m->bar->items, m->bar->n);
        }
        return 1;
    }
    if (ev->type == LJ_EV_MOUSE_MOVE && m->bar_open && i >= 0 && i != m->bar_sel) { lj_menu_open_bar(m, i); return 1; }
    return 0;
}

int lj_menu_popup_mouse(lj_menu *m, void *handle, const lj_mouse_ev *ev)
{
    int d, k;
    lj_menu_level *lv = NULL;
    for (d = 0; d < m->depth; d++) if (m->lv[d].handle == handle) lv = &m->lv[d];
    if (!lv) return 0;
    d = (int)(lv - m->lv);
    k = level_item_at(m, lv, ev->y);
    if (ev->x < 0 || ev->x >= lv->s.w) k = -1;
    if (ev->type == LJ_EV_MOUSE_MOVE) {
        if (k >= 0 && k != lv->sel && !is_sep(&lv->items[k])) {
            lv->sel = k;
            close_above(m, d + 1);
            level_repaint(m, d);
            if (lv->items[k].children && lv->items[k].nchildren > 0 && item_enabled(m, &lv->items[k])) {
                level_open(m, lv->items[k].children, lv->items[k].nchildren, lv->x + lv->s.w - BORDER, lv->y + level_item_y(m, lv, k) - BORDER);
            }
        }
        return 1;
    }
    if (ev->type == LJ_EV_MOUSE_UP && ev->button == 1) {
        if (k < 0) return 1;
        lv->sel = k;
        close_above(m, d + 1);
        return select_current(m);
    }
    return ev->type == LJ_EV_MOUSE_DOWN;
}

static int find_accel(lj_menu *m, const lj_menu_item_desc *items, int n, int ch)
{
    int k;
    for (k = 0; k < n; k++) if (item_visible(m, &items[k]) && !is_sep(&items[k]) && lj_menu_accel(items[k].caption) == ch) return k;
    return -1;
}

int lj_menu_alt_letter(lj_menu *m, int ch)
{
    int i;
    if (!m->bar) return 0;
    if (ch >= 'A' && ch <= 'Z') ch += 32;
    i = find_accel(m, m->bar->items, m->bar->n, ch);
    if (i < 0) return 0;
    lj_menu_open_bar(m, i);
    if (m->bar_open && m->depth > 0) move_sel(m, 1);
    else if (!m->bar_open && item_enabled(m, &m->bar->items[i])) { m->bar_active = 0; m->bar_sel = -1; fire(m, &m->bar->items[i], m->bar->items, m->bar->n); }
    return 1;
}

int lj_menu_key(lj_menu *m, const lj_key_ev *k)
{
    int ch;
    if (!lj_menu_is_open(m)) return 0;
    if (k->type == LJ_EV_CHAR) {
        ch = k->ch;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (m->depth > 0) {
            lj_menu_level *lv = &m->lv[m->depth - 1];
            int i = find_accel(m, lv->items, lv->n, ch);
            if (i >= 0) { lv->sel = i; select_current(m); }
        } else if (m->bar_active) lj_menu_alt_letter(m, ch);
        return 1;
    }
    if (k->type != LJ_EV_KEY_DOWN) return 1;
    if ((k->key >= 'A' && k->key <= 'Z') || (k->key >= '0' && k->key <= '9')) {   /* accelerators arrive as key downs too (no text input while a menu is up) */
        lj_key_ev c = *k;
        c.type = LJ_EV_CHAR; c.ch = k->key >= 'A' ? k->key + 32 : k->key;
        if (k->alt) { lj_menu_alt_letter(m, c.ch); return 1; }
        return lj_menu_key(m, &c);
    }
    switch (k->key) {
    case 0x79: lj_menu_close(m); return 1;   /* F10 toggles the bar off */
    case 0x1B:   /* Esc: one level, then the bar highlight */
        if (m->depth > 0) { close_above(m, m->depth - 1); if (m->depth == 0) { m->bar_open = 0; if (!m->bar) lj_menu_close(m); } }
        else lj_menu_close(m);
        m->dirty = 1;
        return 1;
    case 0x0D: if (m->depth > 0) select_current(m); else if (m->bar_active) lj_menu_open_bar(m, m->bar_sel); if (m->depth > 0 && m->lv[m->depth - 1].sel < 0) move_sel(m, 1); return 1;
    case 0x26: if (m->depth > 0) move_sel(m, -1); return 1;
    case 0x28: if (m->depth > 0) move_sel(m, 1); else if (m->bar_active) { lj_menu_open_bar(m, m->bar_sel); if (m->depth > 0) move_sel(m, 1); } return 1;
    case 0x27: {
        lj_menu_level *lv = m->depth > 0 ? &m->lv[m->depth - 1] : NULL;
        if (lv && lv->sel >= 0 && lv->items[lv->sel].children && lv->items[lv->sel].nchildren > 0 && item_enabled(m, &lv->items[lv->sel])) { select_current(m); return 1; }
        if (m->bar && (m->bar_open || m->bar_active)) { lj_menu_open_bar(m, first_visible_bar(m, m->bar_sel, 1)); if (m->depth > 0) move_sel(m, 1); }
        return 1; }
    case 0x25:
        if (m->depth > 1) { close_above(m, m->depth - 1); return 1; }
        if (m->bar && (m->bar_open || m->bar_active)) { lj_menu_open_bar(m, first_visible_bar(m, m->bar_sel, -1)); if (m->depth > 0) move_sel(m, 1); }
        return 1;
    default: return 1;   /* a menu swallows every other key while open, as Windows does */
    }
}

static const lj_menu_item_desc *find_shortcut(lj_menu *m, const lj_menu_item_desc *items, int n, int word, const lj_menu_item_desc **sib, int *nsib)
{
    int k;
    const lj_menu_item_desc *hit;
    for (k = 0; k < n; k++) {
        if (!item_visible(m, &items[k])) continue;
        if (items[k].shortcut == word && item_enabled(m, &items[k])) { *sib = items; *nsib = n; return &items[k]; }
        if (items[k].children && (hit = find_shortcut(m, items[k].children, items[k].nchildren, word, sib, nsib)) != NULL) return hit;
    }
    return NULL;
}

int lj_menu_shortcut(lj_menu *m, const lj_key_ev *k)
{
    int word;
    const lj_menu_item_desc *it, *sib = NULL;
    int nsib = 0;
    if (!m->bar || k->type != LJ_EV_KEY_DOWN || k->key == 0) return 0;
    word = (k->key & 0xFF) | (k->shift ? 0x2000 : 0) | (k->ctrl ? 0x4000 : 0) | (k->alt ? 0x8000 : 0);
    it = find_shortcut(m, m->bar->items, m->bar->n, word, &sib, &nsib);
    if (!it) return 0;
    fire(m, it, sib, nsib);
    return 1;
}

/* ---- bar painting ---- */

void lj_menu_bar_paint(lj_menu *m, lj_surface *s)
{
    int i;
    lj_rect r;
    m->dirty = 0;
    if (!m->bar) return;
    lj_fill(s, m->bar_x, m->bar_y, m->bar_w, LJ_MENU_BAR_H, LJ_CLR_BTNFACE);
    for (i = 0; i < m->bar->n; i++) {
        const lj_menu_item_desc *it = &m->bar->items[i];
        int hi = i == m->bar_sel && (m->bar_active || m->bar_open), en = item_enabled(m, it);
        if (lj_menu_bar_item_rect(m, i, &r) != 0) continue;
        if (hi) lj_fill(s, r.x, r.y, r.w, r.h, LJ_CLR_HIGHLIGHT);
        if (!en && !hi) lj_text(s, menu_font(), r.x + 7, r.y + 3, it->caption ? it->caption : "", LJ_CLR_BTNHIGHLIGHT, 1);
        lj_text(s, menu_font(), r.x + 6, r.y + 2, it->caption ? it->caption : "", hi ? LJ_CLR_BTNHIGHLIGHT : en ? LJ_CLR_3DDKSHADOW : LJ_CLR_BTNSHADOW, 1);
    }
}
