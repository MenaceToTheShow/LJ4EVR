/* lj_ctl_common.c - the fixture control windows: open/close/refresh from the master's
 * lj_ctl_window table, the generic row renderers (O/S/F, drag bar, palette, effect
 * buttons, scroll control), the one write path and the lj_prog_* seam to the current
 * scene (design 6.3, 6.4, 7.2, 7.7, note 07 s3, s9, s14; seam 2 of docs/w1-contracts.md).
 * The kind files (lj_ctl_intensity.c, lj_ctl_levels.c, lj_ctl_lamp.c, lj_ctl_extended.c)
 * render their own row kinds; lj_ctl_row_build dispatches to them. */
#include "controls/lj_ctl_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl_colors.h"
#include "controls/lj_ctl_extended.h"
#include "controls/lj_ctl_intensity.h"
#include "controls/lj_ctl_lamp.h"
#include "controls/lj_ctl_levels.h"
#include "controls/lj_ctl_movement.h"
#include "controls/lj_ctl_rgbpack.h"       /* W2 windows-a (LORD_FIDELITY-18) */
#include "controls/lj_ctl_extapp.h"        /* W5 extapp: TSpecialSettings_ExternalApp */
#include "core/lj_lock.h"
#include "core/lj_locale.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "gfx/lj_draw.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "windows/lj_windows.h"

static lj_ctl_win lj_ctls[LJ_CTL_COUNT];
static const lj_win_host *lj_ctl_host;
static int (*lj_ctl_confirm_fn)(const char *, const char *, void *);
static void *lj_ctl_confirm_ud;

/* Window chrome per kind: client sizes from the CHM GIFs (note 07 s9, s14); 0 = the
 * built content's bounding box + 2 px. levelcontrol.gif is 1:1; intensitycontrol,
 * lampcontrol and extended are 0.8 reductions (ME-67) and are read x1.25 with the
 * normal 19-px caption LJ2's control windows carry. The class name is what
 * lj_formsini_section() turns into the Main.Ini/forms.ini section ([INTENSITY], [LEVELS], ... lj_ctl.h). */
static const struct { const char *name, *cls; lj_border_style border; int cw, ch; } lj_ctl_chrome[LJ_CTL_COUNT] = {
    { "IntensityControl", "TIntensity", LJ_BS_SINGLE,     594, 78 },   /* measured from intensitycontrol.gif 480x98, reduced x1.25 = 600x122, rig item: frame 3, caption 19, menu 19; 3 rows x 26 */
    { "MovementControl",  "TMovement",  LJ_BS_TOOLWINDOW, 0, 0 },
    { "ColorControl",     "TColors",    LJ_BS_TOOLWINDOW, 0, 0 },
    { "GoboControl",      "TGobos",     LJ_BS_TOOLWINDOW, 0, 0 },
    { "BeamControl",      "TBeam",      LJ_BS_TOOLWINDOW, 0, 0 },
    { "EffectsControl",   "TEffects",   LJ_BS_TOOLWINDOW, 0, 0 },
    { "LevelControl",     "TLevels",    LJ_BS_DIALOG,     266, 206 },  /* measured from levelcontrol.gif 272x250: frame 3, caption 19, menu 19 */
    { "LampControl",      "TLamp",      LJ_BS_SINGLE,     160, 81 },   /* measured from lampcontrol.gif 133x100, reduced x1.25 = 166x125, rig item */
    { "ExtendedControl",  "TExtended",  LJ_BS_SINGLE,     346, 134 },  /* measured from extended.gif 277x81 (the two groups), reduced x1.25 = 346x101, rig item; + a Fixture Reset row, PROPOSAL (15/01 s10) */
    { "GenericControl",   "TGeneric",   LJ_BS_TOOLWINDOW, 0, 0 },
    { "RgbControl",       "TRgb",       LJ_BS_TOOLWINDOW, 0, 0 },
};

/* ---- hooks ---- */

void lj_ctl_set_host(const lj_win_host *host) { lj_ctl_host = host; }
void lj_ctl_set_confirm(int (*fn)(const char *, const char *, void *), void *ud) { lj_ctl_confirm_fn = fn; lj_ctl_confirm_ud = ud; }

int lj_ctl_menu_run(enum lj_ctl_kind kind, const char *item)
{
    static const enum lj_fn pal[LJ_CTL_COUNT] = { [LJ_CTL_INTENSITY] = LJ_FN_TOGGLE_INTENSITY_PALETTE, [LJ_CTL_COLORS] = LJ_FN_TOGGLE_COLOR_PALETTE,
        [LJ_CTL_GOBOS] = LJ_FN_TOGGLE_GOBO_PALETTE, [LJ_CTL_BEAM] = LJ_FN_TOGGLE_BEAM_PALETTE, [LJ_CTL_EFFECTS] = LJ_FN_TOGGLE_EFFECT_PALETTE,
        [LJ_CTL_LEVELS] = LJ_FN_TOGGLE_LEVELS_PALETTE, [LJ_CTL_EXTENDED] = LJ_FN_TOGGLE_LEVELS_PALETTE };
    if (!item || (unsigned)kind >= LJ_CTL_COUNT) return -1;
    if (!strcmp(item, "UserPalette") || !strcmp(item, "M_UserPalette")) { if (!pal[kind]) return -1; lj_fn_run(pal[kind], 1); return 0; }   /* the User Palette window of this kind */
    if (!strcmp(item, "Fades") || !strcmp(item, "M_Fades")) { lj_win_open(LJ_WIN_SEQ_FADES); return 0; }   /* Sequence Fades [04 s1.4] */
    if (!strcmp(item, "Help") || !strcmp(item, "M_Help")) { lj_desktop_run_action(LJ_ACT_HELP, 0); return 0; }   /* the bundled viewer (design 13 item 12) */
    return -1;
}

int lj_ctl_confirm(const char *caption, const char *text)
{
    if (lj_ctl_confirm_fn) return lj_ctl_confirm_fn(caption, text, lj_ctl_confirm_ud) != 0;
    lj_log("lj_ctl: no confirm box wired, %s: %s -> yes", caption, text);
    return 1;
}

/* ---- records ---- */

lj_ctl_win *lj_ctl_rec(enum lj_ctl_kind kind) { return kind >= 0 && kind < LJ_CTL_COUNT ? &lj_ctls[kind] : NULL; }

lj_ctl_win *lj_ctl_of(const lj_window *win)
{
    int k;
    if (!win) return NULL;
    for (k = 0; k < LJ_CTL_COUNT; k++) if (lj_ctls[k].win == win) return &lj_ctls[k];
    return NULL;
}

lj_ctl_elem *lj_ctl_elem_add(lj_ctl_win *c, lj_widget *w, const lj_ctl_row *row, const lj_ctl_button *btn, int role)
{
    lj_ctl_elem *e;
    if (!c || !w) return NULL;
    if (c->n >= LJ_CTL_ELEMS) { lj_log("lj_ctl: %s: more than %d widgets", lj_ctl_chrome[c->kind].name, LJ_CTL_ELEMS); return NULL; }
    e = &c->e[c->n++];
    memset(e, 0, sizeof *e);
    e->w = w; e->row = row; e->btn = btn; e->role = (uint8_t)role;
    if (row) { e->lo = row->lo; e->hi = row->hi; e->reverse = row->reverse; e->dflt_state = lj_ctl_dflt_state(c, row->ch); }
    if (row && role != LJ_CE_NONE) {                        /* code-built widgets carry no DFM name; the --script driver resolves these through lj_ctl_elem_find */
        static const char *const suffix[] = { "", "fade", "bar", "shutter", "btn", "bar", "radio", "pal", "scroll" };
        if (btn && row->buttons && role != LJ_CE_SHUTTER) snprintf(e->name, sizeof e->name, "row%u_%s%u", (unsigned)row->ch + 1u, suffix[role], (unsigned)(btn - row->buttons) + 1u);   /* 3 + 10 + 1 + 7 + 10 + NUL = 32: gcc -Wformat-truncation */
        else snprintf(e->name, sizeof e->name, "row%u_%s", (unsigned)row->ch + 1u, suffix[role]);
    }
    return e;
}

lj_widget *lj_ctl_elem_find(const lj_window *win, const char *name)
{
    lj_ctl_win *c = lj_ctl_of(win);
    int i;
    if (!c || !name) return NULL;
    for (i = 0; i < c->n; i++) if (c->e[i].w && c->e[i].name[0] && strcmp(c->e[i].name, name) == 0) return c->e[i].w;
    return NULL;
}

lj_ctl_desc *lj_ctl_desc_new(lj_ctl_elem *e, enum lj_ctl_row_kind kind, const char *caption, int x, int y, int w, int h)
{
    lj_ctl_desc *d = (lj_ctl_desc *)calloc(1, sizeof *d);
    if (!d) lj_fatal("lj_ctl: out of memory");
    d->kind = kind; d->caption = caption; d->left = x; d->top = y; d->width = w; d->height = h;
    if (e) e->d = d;
    return d;
}

uint8_t lj_ctl_dflt_state(const lj_ctl_win *c, uint8_t offset)
{
    uint8_t s = offset < LJ_PROFILE_CH ? c->dflt[offset] : 0;
    return s ? s : LJ_SNAP;
}

unsigned lj_ctl_states(const lj_ctl_win *c, const lj_ctl_row *row)
{
    if (row->states_allowed) return row->states_allowed & LJ_STATES_ALL;
    if (c->profile && row->ch < LJ_PROFILE_CH && c->profile->channel_class[row->ch] == LJ_CH_SWITCH) return LJ_STATES_OFF_SNAP;   /* ME-25: no Fade on a switch pack */
    return LJ_STATES_ALL;
}

/* ---- the scene seam (lj_ctl.h): current scene under lj_lock ---- */


static int prog_write(uint16_t ch, int value, int state)    /* value < 0 = keep; state LJ_PROG_KEEP = keep; 0 / -1 */
{
    lj_scene *s;
    if (ch >= LJ_CHANS) return -1;
    lj_lock();
    s = lj_ctx_scene();
    if (s) {
        if (value >= 0) s->val[ch] = (uint8_t)value;
        if (state != LJ_PROG_KEEP) s->state[ch] = (uint8_t)state;
        lj_ctx_get()->show->current_seq->modified = 1;
    }
    lj_unlock();
    if (!s) return -1;
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    return 0;
}

int lj_prog_get(uint16_t ch, uint8_t *value, uint8_t *state)
{
    lj_scene *s;
    if (ch >= LJ_CHANS) return -1;
    lj_lock();
    s = lj_ctx_scene();
    if (s) { if (value) *value = s->val[ch]; if (state) *state = s->state[ch]; }
    lj_unlock();
    return s ? 0 : -1;
}

void lj_prog_set(uint16_t ch, uint8_t value, uint8_t state) { prog_write(ch, value, state); }

void lj_prog_set16(uint16_t ch_msb, uint16_t ch_lsb, uint16_t value, uint8_t state)
{
    prog_write(ch_msb, value >> 8, state);
    prog_write(ch_lsb, value & 0xFF, state);
}

/* every selected fixture of the master's profile: address code of base + offset, or -1 */
static int sel_ch(int i, int master, uint8_t offset)
{
    const lj_show *show = lj_ctx_get()->show;
    const lj_fixture *f, *m;
    int ch;
    if (!show || master < 0 || master >= LJ_FIXTURES || i < 0 || i >= LJ_FIXTURES) return -1;
    f = &show->fixture[i]; m = &show->fixture[master];
    if (i != master && !lj_sel_is_selected(i)) return -1;
    if (f->kind != m->kind || f->profile_id != m->profile_id) return -1;
    if (f->link < 1 || f->link > LJ_LINKS || f->address < 1 || f->address > LJ_LINK_CH) return -1;
    ch = lj_addr_code(f->link, f->address) + offset;
    return ch < LJ_CHANS ? ch : -1;
}

int lj_prog_master_ch(uint8_t offset) { return sel_ch(lj_sel_master(), lj_sel_master(), offset); }

static void set_sel(uint8_t offset, int value, int state)
{
    int i, m = lj_sel_master(), ch;
    for (i = 0; i < LJ_FIXTURES; i++) if ((ch = sel_ch(i, m, offset)) >= 0) prog_write((uint16_t)ch, value, state);
}

void lj_prog_set_sel(uint8_t offset, uint8_t value, uint8_t state) { set_sel(offset, value, state); }
void lj_prog_set_state_sel(uint8_t offset, uint8_t state) { set_sel(offset, -1, state); }

void lj_prog_set_sel16(uint8_t msb_ofs, uint8_t lsb_ofs, uint16_t value, uint8_t state)
{
    set_sel(msb_ofs, value >> 8, state);
    set_sel(lsb_ofs, value & 0xFF, state);
}

int lj_prog_cycle_state(uint16_t ch, int dir, uint8_t states_allowed)
{
    lj_scene *s;
    int st = LJ_OFF, i, n;
    unsigned mask = (states_allowed & LJ_STATES_ALL) ? (states_allowed & LJ_STATES_ALL) : LJ_STATES_ALL;
    if (ch >= LJ_CHANS) return LJ_OFF;
    lj_lock();
    s = lj_ctx_scene();
    if (s) {
        st = s->state[ch];
        for (i = 1; i <= 3; i++) {                       /* D10: +1 Off -> Snap -> Fade -> Off, -1 the reverse; ME-27: skip what the mask forbids */
            n = (st + dir * i + 6) % 3;
            if (mask & (1u << n)) { st = n; break; }
        }
        s->state[ch] = (uint8_t)st;
        lj_ctx_get()->show->current_seq->modified = 1;
    }
    lj_unlock();
    if (s) lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    return st;
}

/* ---- the write path and the repaint ---- */

void lj_ctl_write(lj_ctl_elem *e, uint8_t value)
{
    uint8_t v = 0, st = LJ_OFF;
    int ch = lj_prog_master_ch(e->row->ch);
    if (ch < 0) return;
    if (lj_prog_get((uint16_t)ch, &v, &st) != 0) return;
    lj_prog_set_sel(e->row->ch, value, st == LJ_OFF ? e->dflt_state : LJ_PROG_KEEP);   /* PROPOSAL rig: touching a control on an Off channel arms its default fade state */
    lj_ctl_repaint_all();
}

static int bar_pos(const lj_ctl_elem *e, uint8_t v)     /* channel value -> bar position 0..hi-lo */
{
    int p = e->reverse ? (int)e->hi - v : v - (int)e->lo, max = e->hi - e->lo;
    return p < 0 ? 0 : p > max ? max : p;
}

static uint8_t bar_value(const lj_ctl_elem *e, int pos) { return (uint8_t)(e->reverse ? e->hi - pos : e->lo + pos); }

static int in_range(const lj_ctl_button *b, uint8_t v) { return b && v >= b->lo && v <= b->hi; }

void lj_ctl_win_repaint(lj_ctl_win *c)
{
    int i, ch;
    uint8_t v, st;
    if (!c || !c->win) return;
    for (i = 0; i < c->n; i++) {
        lj_ctl_elem *e = &c->e[i];
        if (e->role == LJ_CE_NONE || !e->row) continue;
        ch = lj_prog_master_ch(e->row->ch);
        if (ch < 0 || lj_prog_get((uint16_t)ch, &v, &st) != 0) continue;
        switch (e->role) {
        case LJ_CE_OSF:     lj_w_osf_set_state(e->w, st); break;
        case LJ_CE_DRAGBAR: lj_w_dragbar_set_value(e->w, bar_pos(e, v)); break;
        case LJ_CE_FADER:   lj_w_fader_set_value(e->w, bar_pos(e, v)); break;
        case LJ_CE_SCROLL:  lj_w_scrollctl_set_value(e->w, v); break;
        case LJ_CE_SHUTTER: lj_w_set_caption(e->w, v == e->hi ? e->row->caption2 : e->row->caption); break;   /* open reads only at the exact open value [02 s5.3] */
        case LJ_CE_BUTTON:  lj_w_speedbutton_set_down(e->w, in_range(e->btn, v)); break;
        case LJ_CE_RADIO:   if (((lj_w_cap *)e->w)->checked != (unsigned)in_range(e->btn, v)) { ((lj_w_cap *)e->w)->checked = in_range(e->btn, v) != 0; lj_widget_invalidate(e->w); } break;
        case LJ_CE_PALETTE: lj_w_palette_select(e->w, in_range(e->btn, v)); break;
        default: break;
        }
    }
    if (c->kind == LJ_CTL_RGBPACK) lj_ctl_rgbpack_repaint(c);   /* W2 windows-a */
    if (c->kind == LJ_CTL_EXTENDED && c->profile && c->profile->id == 90) lj_ctl_extapp_repaint(c);   /* W5 extapp */
}

void lj_ctl_repaint_all(void)
{
    int k;
    for (k = 0; k < LJ_CTL_COUNT; k++) lj_ctl_win_repaint(&lj_ctls[k]);
}

/* ---- generic handlers (w->ud = the elem) ---- */

static void osf_click(lj_widget *w, void *ud)
{
    lj_ctl_elem *e = (lj_ctl_elem *)ud;
    const lj_mouse_ev *m = lj_window_last_event(lj_window_of(w));
    int ch = lj_prog_master_ch(e->row->ch), st;
    if (ch < 0) return;
    st = lj_prog_cycle_state((uint16_t)ch, m && m->button == 2 ? -1 : 1, (uint8_t)lj_ctl_states(lj_ctl_of(lj_window_of(w)), e->row));   /* D10, ME-27: the state lives on the scene channel */
    lj_prog_set_state_sel(e->row->ch, (uint8_t)st);
    lj_ctl_repaint_all();
}

static void bar_change(lj_widget *w, void *ud) { lj_ctl_elem *e = (lj_ctl_elem *)ud; lj_ctl_write(e, bar_value(e, lj_w_dragbar_value(w))); }
static void fader_change(lj_widget *w, void *ud) { lj_ctl_elem *e = (lj_ctl_elem *)ud; lj_ctl_write(e, bar_value(e, lj_w_fader_value(w))); }
static void scroll_change(lj_widget *w, void *ud) { lj_ctl_write((lj_ctl_elem *)ud, (uint8_t)lj_w_scrollctl_value(w)); }
static void btn_click(lj_widget *w, void *ud) { lj_ctl_elem *e = (lj_ctl_elem *)ud; (void)w; if (e->btn) lj_ctl_write(e, e->btn->value); }

void lj_ctl_on_event(lj_widget *w, int ev, void *ud)
{
    lj_ctl_elem *e = (lj_ctl_elem *)ud;
    switch (e->role) {
    case LJ_CE_OSF:     if (ev == LJ_WEV_CLICK) osf_click(w, ud); break;
    case LJ_CE_DRAGBAR: if (ev == LJ_WEV_CHANGE) bar_change(w, ud); break;
    case LJ_CE_FADER:   if (ev == LJ_WEV_CHANGE) fader_change(w, ud); break;
    case LJ_CE_SCROLL:  if (ev == LJ_WEV_CHANGE) scroll_change(w, ud); break;
    case LJ_CE_BUTTON: case LJ_CE_PALETTE: if (ev == LJ_WEV_CLICK) btn_click(w, ud); break;
    default: break;
    }
}

/* ---- widgets the kind files build with ---- */

lj_widget *lj_ctl_osf_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w, int h)
{
    lj_widget *b = lj_w_osf_create(parent, NULL);
    lj_ctl_elem *e;
    b->x = x; b->y = y; b->w = w; b->h = h;
    lj_w_osf_set_allowed(b, lj_ctl_states(c, row));
    e = lj_ctl_elem_add(c, b, row, NULL, LJ_CE_OSF);
    if (e) { b->on_event = lj_ctl_on_event; b->ud = e; }
    return b;
}

static int row_group(const lj_ctl_win *c, const lj_ctl_row *row)
{
    const lj_ctl_window *t = c->table;
    if (t && row >= t->rows && row < t->rows + t->nrows) return 1 + (int)(row - t->rows);
    if (t && row >= t->specials && row < t->specials + t->nspecials) return 100 + (int)(row - t->specials);
    return 200;
}

lj_widget *lj_ctl_button_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, const lj_ctl_button *btn, int role,
                             const char *caption, int x, int y, int w, int h, int black)
{
    lj_ctl_elem *e = lj_ctl_elem_add(c, parent, row, btn, role);   /* registered before creation so the desc can be owned; w patched below */
    lj_ctl_desc *d;
    lj_widget *b;
    if (!e) return NULL;
    d = lj_ctl_desc_new(e, LJ_K_SPEEDBUTTON, caption, x, y, w, h);
    d->allow_all_up = LJ_YES; d->group_index = row_group(c, row);   /* one exclusive group per row */
    if (black) { d->color = LJ_CLR(0x000000); d->font_color = LJ_CLR(0xFFFFFF); d->flat = LJ_YES; }   /* the flat black buttons of the specials and the shutter bar */
    b = lj_w_speedbutton_create(parent, d);
    e->w = b; b->on_event = lj_ctl_on_event; b->ud = e;
    return b;
}

lj_widget *lj_ctl_label_new(lj_ctl_win *c, lj_widget *parent, const char *caption, int x, int y, int w, int h, int center)
{
    lj_ctl_elem *e = lj_ctl_elem_add(c, parent, NULL, NULL, LJ_CE_NONE);
    lj_ctl_desc *d;
    lj_widget *l;
    if (!e) return NULL;
    d = lj_ctl_desc_new(e, LJ_K_LABEL, caption, x, y, w, h);
    d->autosize = LJ_NO; d->layout = "tlCenter"; if (center) d->alignment = LJ_TA_CENTER;
    l = lj_w_label_create(parent, d);
    e->w = l;
    return l;
}

/* ---- row renderers (lj_ctl.h) ---- */

lj_widget *lj_ctl_dragbar_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, const char *caption, int x, int y, int w)
{
    lj_widget *b = lj_w_dragbar_create(parent, NULL);
    lj_ctl_elem *e;
    b->x = x; b->y = y; b->w = w; b->h = LJ_CTL_ROW_H;
    ((lj_w_dragbar *)b)->caption = caption;
    ((lj_w_dragbar *)b)->max = row->hi > row->lo ? row->hi - row->lo : 1;
    e = lj_ctl_elem_add(c, b, row, NULL, LJ_CE_DRAGBAR);
    if (e) { b->on_event = lj_ctl_on_event; b->ud = e; }
    return b;
}

static int build_palette(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y)
{
    /* row->lo = index into profile->palette[] (0 color1 .. 5 fx2); 2 rows x 10 tiles 31x30 (note 07 s3.3, s9.1) */
    const lj_palette *pal = c->profile && row->lo < LJ_PALETTES ? &c->profile->palette[row->lo] : NULL;
    int i, n = 0;
    if (!pal) return 0;
    for (i = 0; i < LJ_PAL_ENTRIES; i++) {
        lj_widget *t;
        lj_ctl_elem *e;
        struct { lj_ctl_desc d; lj_ctl_button b; } *blk;                /* the tile's emit value rides behind an owned desc (one free() per elem) */
        if (!pal->e[i].used) continue;
        t = lj_w_palette_create(parent, NULL);
        t->x = x + (n % 10) * (LJ_PALETTE_W + 1); t->y = y + (n / 10) * (LJ_PALETTE_H + 1); t->w = LJ_PALETTE_W; t->h = LJ_PALETTE_H;
        lj_w_palette_set_tile(t, NULL, pal->e[i].rgb);
        e = lj_ctl_elem_add(c, t, row, NULL, LJ_CE_PALETTE);
        if (!e) break;
        blk = calloc(1, sizeof *blk);
        if (!blk) lj_fatal("lj_ctl: out of memory");
        blk->b.value = blk->b.lo = blk->b.hi = pal->e[i].v[0]; blk->b.ch = row->ch;
        e->btn = &blk->b; e->d = &blk->d;
        t->on_event = lj_ctl_on_event; t->ud = e;
        n++;
    }
    return n ? ((n + 9) / 10) * (LJ_PALETTE_H + 1) : 0;
}

static int build_buttons(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w, int black)
{
    int i, n = row->nbuttons, bw;
    if (n <= 0) return 0;
    bw = (w - (n - 1) * LJ_CTL_GAP) / n;
    for (i = 0; i < n; i++)
        lj_ctl_button_new(c, parent, row, &row->buttons[i], LJ_CE_BUTTON, row->buttons[i].caption, x + i * (bw + LJ_CTL_GAP), y, bw, LJ_CTL_ROW_H, black);
    return LJ_CTL_ROW_H;
}

static int build_scroll(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y)
{
    lj_widget *s = lj_w_scrollctl_create(parent, NULL);
    lj_ctl_elem *e;
    s->x = x; s->y = y; s->w = LJ_SCROLLCTL_W; s->h = LJ_SCROLLCTL_H;
    e = lj_ctl_elem_add(c, s, row, NULL, LJ_CE_SCROLL);
    if (e) { e->lo = 0; e->hi = 255; s->on_event = lj_ctl_on_event; s->ud = e; }
    return LJ_SCROLLCTL_H;
}

int lj_ctl_row_build(lj_window *win, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    lj_ctl_win *c = lj_ctl_of(win);
    static unsigned logged;
    if (!c || !parent || !row) return 0;
    switch (row->kind) {
    case LJ_W_FADEBTN:
        c->dflt[row->ch < LJ_PROFILE_CH ? row->ch : 0] = row->dflt_state;
        lj_ctl_osf_new(c, parent, row, x, y, LJ_CTL_OSF_W, (row->rows > 1 ? row->rows : 1) * LJ_CTL_PITCH - 2);
        return 0;                                                   /* spans the following rows; the caller does not advance */
    case LJ_W_DRAGBAR:         lj_ctl_dragbar_new(c, parent, row, row->caption, x, y, w); return LJ_CTL_ROW_H;
    case LJ_W_PALETTE:         return build_palette(c, parent, row, x, y);
    case LJ_W_EFFECT_BTN:      return build_buttons(c, parent, row, x, y, w, 1);
    case LJ_W_SCROLL:          return build_scroll(c, parent, row, x, y);
    case LJ_W_SHUTTER_TOGGLE:
    case LJ_W_SPECIAL_GROUP:   return lj_ctl_intensity_row(c, parent, row, x, y, w);
    case LJ_W_LEVEL_FADER:     return lj_ctl_levels_row(c, parent, row, x, y);
    case LJ_W_LAMP_BTNS:
    case LJ_W_NOTE:            return lj_ctl_lamp_row(c, parent, row, x, y, w);
    case LJ_W_RADIOGROUP_FADER:return lj_ctl_extended_row(c, parent, row, x, y);
    default:
        if (row->kind >= 0 && row->kind < 32 && !(logged & (1u << row->kind))) { logged |= 1u << row->kind; lj_log("lj_ctl: row kind %d not built in W1 controls-a (controls-b)", (int)row->kind); }
        return 0;
    }
}

void lj_ctl_row_refresh(lj_widget *row_root) { lj_ctl_win_repaint(lj_ctl_of(lj_window_of(row_root))); }

int lj_ctl_value_text(char *dst, size_t cap, const lj_ctl_row *row, uint8_t v)
{
    int lo = row->lo, hi = row->hi, pos, pct10, n;
    if (!dst || !cap) return -1;
    switch (row->kind) {
    case LJ_W_DRAGBAR: case LJ_W_SPECIAL_GROUP: case LJ_W_ROTATION: case LJ_W_INDEXBAR:
        pos = row->reverse ? hi - v : v - lo;
        if (hi <= lo) hi = lo + 1;
        if (pos < 0) pos = 0;
        if (pos > hi - lo) pos = hi - lo;
        pct10 = (pos * 1000 + (hi - lo) / 2) / (hi - lo);
        n = row->caption ? snprintf(dst, cap, "%s %d%c%d%%", row->caption, pct10 / 10, lj_locale_decimal(), pct10 % 10)   /* D6, ME-52: the host separator only in displayed text */
                         : snprintf(dst, cap, "%d%c%d%%", pct10 / 10, lj_locale_decimal(), pct10 % 10);
        break;
    case LJ_W_LEVEL_FADER: case LJ_W_RADIOGROUP_FADER:
        n = snprintf(dst, cap, "%d%%", (v * 100 + 127) / 255);
        break;
    default:
        n = snprintf(dst, cap, "%d", v);
        break;
    }
    return n >= 0 && n < (int)cap ? 0 : -1;
}

/* ---- the window ---- */

static void menu_click(lj_widget *root, void *ud)
{
    lj_ctl_win *c = (lj_ctl_win *)ud;
    const lj_menu_item_desc *it = lj_window_menu_item(c->win);
    const char *name = it && it->name ? it->name : "?";
    (void)root;
    if (strcmp(name, "Preferences") == 0 && c->kind == LJ_CTL_INTENSITY) { lj_ctl_intensity_prefs(c); return; }
    if (strcmp(name, "Palette") == 0 && c->kind == LJ_CTL_RGBPACK) { lj_win_open(LJ_WIN_RGB_PALETTE); return; }   /* W4: RGB Palette [04 s5.3] */
    if (lj_ctl_menu_run(c->kind, name) == 0) return;   /* W4 acceptance: UserPalette / Fades / Help */
    lj_log("lj_ctl: %s menu %s: no action", lj_ctl_chrome[c->kind].name, name);   /* PROPOSAL: a childless bar item fires like a button */
}

static void form_close(lj_widget *root, void *ud) { (void)root; lj_ctl_close(((lj_ctl_win *)ud)->kind); }

static void build_rows(lj_ctl_win *c)
{
    const lj_ctl_window *t = c->table;
    lj_widget *root = lj_window_root(c->win);
    int i, x = LJ_CTL_X, y = 0, col = 0, h, sy = 0, start;
    for (i = 0; i < t->nrows; i++) {
        const lj_ctl_row *r = &t->rows[i];
        if (r->kind == LJ_W_LEVEL_FADER || r->kind == LJ_W_RADIOGROUP_FADER) {   /* column kinds run left to right */
            h = lj_ctl_row_build(c->win, root, r, col, 0, 0);
            col += r->kind == LJ_W_LEVEL_FADER ? LJ_CTL_LEVELS_COL_W : LJ_CTL_EXT_COL_W;
            if (h > y) y = h;
            continue;
        }
        if (r->kind == LJ_W_FADEBTN) { lj_ctl_row_build(c->win, root, r, 0, y, 0); continue; }
        h = lj_ctl_row_build(c->win, root, r, x, y, LJ_CTL_BAR_W);
        if (h > 0) y += h > LJ_CTL_PITCH ? h + 2 : LJ_CTL_PITCH;
    }
    start = c->n;
    for (i = 0; i < t->nspecials; i++) {
        h = lj_ctl_row_build(c->win, root, &t->specials[i], LJ_CTL_SPEC_X, sy, (c->cw_full ? c->cw_full : 594) - LJ_CTL_SPEC_X);
        if (h > 0) sy += LJ_CTL_PITCH;
    }
    for (i = start; i < c->n; i++) c->e[i].special = 1;
    if (!lj_ctl_chrome[c->kind].cw) {                                 /* size from the content */
        int cw = 0, ch = 0;
        for (i = 0; i < c->n; i++) { lj_widget *w = c->e[i].w; if (w->x + w->w > cw) cw = w->x + w->w; if (w->y + w->h > ch) ch = w->y + w->h; }
        lj_window_resize(c->win, cw + 2, ch + 2);
    }
}

lj_window *lj_ctl_open_table(enum lj_ctl_kind kind, const lj_ctl_window *table, const lj_profile *p)
{
    lj_ctl_win *c = lj_ctl_rec(kind);
    int i, n = 0;
    if (!c || !table) return NULL;
    if (c->win) { if (c->table == table) { if (lj_ctl_host && lj_ctl_host->show) lj_ctl_host->show(lj_ctl_host->ud, c->win, 1); return c->win; } lj_ctl_close(kind); }
    memset(c, 0, sizeof *c);
    c->kind = kind; c->table = table; c->profile = p; c->specials_shown = 1;
    c->cw_full = lj_ctl_chrome[kind].cw; c->cw_plain = LJ_CTL_X + LJ_CTL_BAR_W + 2;   /* intensitycontroluser.gif 176x98 (reduced): the three left rows alone */
    c->desc.name = lj_ctl_chrome[kind].name; c->desc.class_name = lj_ctl_chrome[kind].cls; c->desc.caption = table->caption;
    c->desc.client_width = c->cw_full ? c->cw_full : 100; c->desc.client_height = lj_ctl_chrome[kind].ch ? lj_ctl_chrome[kind].ch : 100;
    c->desc.text_height = 13; c->desc.color = LJ_CLR(LJ_CLR_BTNFACE); c->desc.font = LJ_FONT_SANS13; c->desc.font_name = "MS Sans Serif"; c->desc.font_height = -11;
    c->desc.border = lj_ctl_chrome[kind].border; c->desc.icons = LJ_BI_GIVEN | LJ_BI_SYSMENU;   /* system-menu-only windows (note 07 s9) */
    c->desc.style = LJ_FS_STAYONTOP;                                                            /* PROPOSAL rig LORD_FIDELITY-10: stay-on-top like Sequence/Cue */
    c->desc.position = LJ_PO_DESIGNED;                                                          /* persisted by section (lj_formsini_section -> the class name) */
    for (i = 0; table->menu && table->menu[i] && n < LJ_CTL_MENU_MAX; i++, n++) {
        snprintf(c->hname[n], sizeof c->hname[n], "%sClick", table->menu[i]);
        c->items[n].name = table->menu[i]; c->items[n].caption = table->menu[i]; c->items[n].on_click = c->hname[n];
        c->bind[n].event = c->hname[n]; c->bind[n].fn = menu_click; c->bind[n].ud = c;
    }
    c->menu.name = "MainMenu1"; c->menu.is_main = 1; c->menu.items = c->items; c->menu.n = n;
    c->desc.menus = &c->menu; c->desc.nmenus = 1; c->desc.menu = n ? "MainMenu1" : NULL;
    c->desc.events[0].event = "OnClose"; c->desc.events[0].handler = "FormClose";
    c->bind[n].event = "FormClose"; c->bind[n].fn = form_close; c->bind[n].ud = c; n++;
    c->bind[n].event = NULL;
    c->win = lj_window_create(&c->desc, c->bind);
    if (!c->win) { lj_log("lj_ctl: %s: window create failed", c->desc.name); return NULL; }
    if (kind == LJ_CTL_RGBPACK) lj_ctl_rgbpack_build(c);       /* W2 windows-a: status panels + programming panel, not the row renderer */
    else if (kind == LJ_CTL_EXTENDED && p && p->id == 90) lj_ctl_extapp_build(c);   /* W5 extapp: the command-string window, not the row renderer */
    else build_rows(c);
    lj_ctl_win_repaint(c);
    if (lj_ctl_host && lj_ctl_host->open && lj_ctl_host->open(lj_ctl_host->ud, c->win, 2u /* LJ_WF_STAY_ON_TOP */) != 0) lj_log("lj_ctl: %s: host open failed", c->desc.name);
    return c->win;
}

/* controls-b/c windows build themselves (lj_ctl_movement_create, lj_ctl_c_create); the registry row keeps them reachable through the lj_ctl_* API. */
static lj_window *lj_ctl_open_foreign(enum lj_ctl_kind kind, const lj_ctl_window *t, const lj_profile *p)
{
    lj_ctl_win *c = lj_ctl_rec(kind);
    lj_window *w;
    if (c->win) { if (c->table == t) { if (lj_ctl_host && lj_ctl_host->show) lj_ctl_host->show(lj_ctl_host->ud, c->win, 1); return c->win; } lj_ctl_close(kind); }
    w = kind == LJ_CTL_MOVEMENT ? lj_ctl_movement_create(p) : lj_ctl_c_create(kind, p);
    if (!w) return NULL;
    memset(c, 0, sizeof *c);
    c->kind = kind; c->win = w; c->table = t; c->profile = p; c->foreign = 1;
    if (kind == LJ_CTL_MOVEMENT) lj_ctl_movement_select_from_desktop(w);
    if (lj_ctl_host && lj_ctl_host->open && lj_ctl_host->open(lj_ctl_host->ud, w, 2u /* LJ_WF_STAY_ON_TOP */) != 0) lj_log("lj_ctl: %s: host open failed", lj_ctl_chrome[kind].name);
    return w;
}

static void lj_ctl_refresh_foreign(lj_ctl_win *c)
{
    if (c->kind == LJ_CTL_MOVEMENT) { lj_ctl_movement_select_from_desktop(c->win); lj_ctl_movement_refresh(c->win); }
    else lj_ctl_c_refresh(c->win);
}

lj_window *lj_ctl_open(enum lj_ctl_kind kind)
{
    const lj_profile *p = lj_sel_master_profile();
    const lj_ctl_window *t = p ? lj_profile_window(p, kind) : NULL;
    if (!t) { lj_log("lj_ctl: no %s window for the master fixture", kind >= 0 && kind < LJ_CTL_COUNT ? lj_ctl_chrome[kind].name : "?"); return NULL; }
    switch (kind) {
    case LJ_CTL_MOVEMENT: case LJ_CTL_COLORS: case LJ_CTL_GOBOS: case LJ_CTL_BEAM: case LJ_CTL_EFFECTS: return lj_ctl_open_foreign(kind, t, p);
    default: return lj_ctl_open_table(kind, t, p);
    }
}

void lj_ctl_close(enum lj_ctl_kind kind)
{
    lj_ctl_win *c = lj_ctl_rec(kind);
    lj_window *w;
    int i;
    if (!c || !c->win) return;
    w = c->win; c->win = NULL;                                       /* FormClose re-entry guard */
    if (lj_ctl_host && lj_ctl_host->close) lj_ctl_host->close(lj_ctl_host->ud, w);
    if (c->foreign) { if (c->kind == LJ_CTL_MOVEMENT) lj_ctl_movement_destroy(w); else lj_ctl_c_destroy(w); }
    else lj_window_destroy(w);
    for (i = 0; i < c->n; i++) free(c->e[i].d);
    c->n = 0; c->table = NULL; c->profile = NULL; c->foreign = 0;
}

void lj_ctl_close_all(void) { int k; for (k = 0; k < LJ_CTL_COUNT; k++) lj_ctl_close(k); }
unsigned lj_ctl_drop_profiles(const lj_profile *lo, int n)
{
    unsigned mask = 0;
    int k;
    for (k = 0; lo && k < LJ_CTL_COUNT; k++)
        if (lj_ctls[k].win && lj_ctls[k].profile >= lo && lj_ctls[k].profile < lo + n) { lj_ctl_close(k); mask |= 1u << k; }
    return mask;
}
void lj_ctl_toggle(enum lj_ctl_kind kind) { if (lj_ctl_window_get(kind)) lj_ctl_close(kind); else lj_ctl_open(kind); }
void lj_ctl_open_only(enum lj_ctl_kind kind) { int k; for (k = 0; k < LJ_CTL_COUNT; k++) if (k != (int)kind) lj_ctl_close(k); lj_ctl_open(kind); }
lj_window *lj_ctl_window_get(enum lj_ctl_kind kind) { lj_ctl_win *c = lj_ctl_rec(kind); return c ? c->win : NULL; }

const char *lj_ctl_caption(enum lj_ctl_kind kind, const lj_profile *p)
{
    const lj_ctl_window *w = lj_profile_window(p, kind);
    return w && w->caption ? w->caption : "";
}

void lj_ctl_refresh(void)
{
    static uint32_t last = 0xFFFFFFFFu;
    uint32_t serial = lj_sel_serial();
    int k;
    if (serial != last) {
        const lj_profile *p = lj_sel_master_profile();
        last = serial;
        for (k = 0; p && k < LJ_CTL_COUNT; k++) {
            const lj_ctl_window *t;
            if (!lj_ctls[k].win) continue;
            t = lj_profile_window(p, k);
            if (!t) lj_ctl_close(k);                                 /* PROPOSAL: the new master has no such control (F9 greys the button) */
            else if (t != lj_ctls[k].table) { if (lj_ctls[k].foreign) lj_ctl_open_foreign(k, t, p); else lj_ctl_open_table(k, t, p); }
        }
    }
    lj_ctl_repaint_all();
    for (k = 0; k < LJ_CTL_COUNT; k++) if (lj_ctls[k].win && lj_ctls[k].foreign) lj_ctl_refresh_foreign(&lj_ctls[k]);
}
