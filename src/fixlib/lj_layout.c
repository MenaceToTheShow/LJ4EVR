/* lj_layout.c - lj_layout_derive(): the control windows a .GS2 or QLC+-derived profile
 * gets, mirroring the help's Generic Fixture 2 derivation (design 6.3, note 02 s5,
 * CHM dynamicfunctions.htm "each enabled function adds a drag bar or palette to its tab's
 * control"). The rule table, one line per rule:
 *
 *   shutter.enabled        -> Intensity: LJ_W_SHUTTER_TOGGLE, buttons {Closed value, Open value}; Snap default
 *   strobe.enabled         -> Intensity: LJ_W_DRAGBAR min..max (strobe_off is the profile field)
 *   intensity.enabled      -> Intensity: LJ_W_DRAGBAR min..max; Fade default
 *   pt16 || pt8            -> Movement: one LJ_W_FADEBTN "Pan/Tilt Fade" on pan_ofs; misc_move.enabled -> LJ_W_DRAGBAR
 *   color[k].enabled       -> Colors: LJ_W_SCROLL min..max, plus LJ_W_PALETTE (lo = k) for k < 2 when palette[k] has an entry
 *   cmy_on / rgb_on        -> Colors: three LJ_W_DRAGBAR (Cyan Magenta Yellow / Red Green Blue); picker and HSV are profile flags
 *   gobo[k].enabled        -> Gobos: k < 2 LJ_W_SCROLL + LJ_W_PALETTE (lo = 2 + k); k >= 2 LJ_W_INDEXBAR ("rotation and indexing", no palette)
 *   iris focus zoom frost beam[k] -> Beam: LJ_W_DRAGBAR each
 *   effects[k].enabled     -> Effects: k < 2 LJ_W_SCROLL + LJ_W_PALETTE (lo = 4 + k); k >= 2 LJ_W_DRAGBAR
 *   lamp_on / lamp_off     -> Lamp Control: LJ_W_LAMP_BTNS (one button per defined offset) + the 5-second LJ_W_NOTE; OFF|SNAP only (ME-27)
 *   enabled visible levels -> Levels: LJ_W_LEVEL_FADER per offset, caption level_caption[] or CHn; SWITCH class rows OFF|SNAP (ME-25)
 *   special[n].enabled     -> Levels specials: LJ_W_SPECIAL_GROUP, one button per defined offset; OFF|SNAP (ME-27)
 *   every dyn with min > max -> reverse = 1, lo/hi ordered (LORD_FIXTURES-10; DesignSpot focus Near=255 Far=0)
 *   consecutive rows on one channel share one LJ_W_FADEBTN whose rows = the span (CHM offsnapfade.htm "linked fade buttons")
 *
 * Storage: rows, buttons and the window array are malloc'ed once per profile and never
 * freed (the profile lives as long as the process; a .GS2 profile reloaded by W3 gets a
 * fresh derive). A failed malloc is lj_fatal, as everywhere outside engine/. */
#include "fixlib/lj_layout.h"
#include "fixlib/hand/lj_hand.h"
#include <stdlib.h>
#include <string.h>

typedef struct { lj_ctl_row *rows; int n, cap; int last_ch; lj_ctl_row *last_fade; } lj_rowbuf;

static void *lj_xalloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (!p) lj_fatal("lj_layout_derive: out of memory (%u bytes)", (unsigned)n);
    return p;
}

static lj_ctl_row *lj_row_new(lj_rowbuf *b, enum lj_ctl_elem kind, int ch, const char *caption, uint8_t states)
{
    lj_ctl_row *r;
    if (b->n == b->cap) {
        lj_ctl_row *grown = lj_xalloc((size_t)(b->cap + 16) * sizeof *grown);
        if (b->n) memcpy(grown, b->rows, (size_t)b->n * sizeof *grown);
        if (b->last_fade) b->last_fade = grown + (b->last_fade - b->rows);
        free(b->rows);
        b->rows = grown;
        b->cap += 16;
    }
    r = &b->rows[b->n++];
    memset(r, 0, sizeof *r);
    r->kind = kind; r->ch = (uint8_t)ch; r->rows = 1; r->caption = caption; r->states_allowed = states;
    return r;
}

/* A widget row on channel ch: adds the O/S/F button when the channel changes, else widens the last one. */
static lj_ctl_row *lj_widget(lj_rowbuf *b, enum lj_ctl_elem kind, int ch, const char *caption, uint8_t dflt, uint8_t states)
{
    lj_ctl_row *r;
    if (b->last_fade && b->last_ch == ch) {
        b->last_fade->rows++;
        if (states != LJ_STATES_ALL) b->last_fade->states_allowed &= states;
    } else {
        r = lj_row_new(b, LJ_W_FADEBTN, ch, NULL, states);
        r->dflt_state = dflt;
        b->last_fade = r; b->last_ch = ch;
    }
    r = lj_row_new(b, kind, ch, caption, states);
    r->dflt_state = dflt;
    return r;
}

static void lj_range(lj_ctl_row *r, const lj_dyn *d)
{
    if (d->min > d->max) { r->lo = d->max; r->hi = d->min; r->reverse = 1; }   /* LORD_FIXTURES-10 */
    else { r->lo = d->min; r->hi = d->max; }
}

static void lj_bar(lj_rowbuf *b, enum lj_ctl_elem kind, const lj_dyn *d, uint8_t dflt)
{
    if (!d->enabled || d->offset >= LJ_PROFILE_CH) return;
    lj_range(lj_widget(b, kind, d->offset, d->name, dflt, LJ_STATES_ALL), d);
}

static int lj_palette_used(const lj_palette *p)
{
    int i;
    for (i = 0; i < LJ_PAL_ENTRIES; i++) if (p->e[i].used) return 1;
    return 0;
}

static void lj_wheel(lj_rowbuf *b, const lj_dyn *d, const lj_profile *p, int pal)
{
    lj_ctl_row *r;
    if (!d->enabled || d->offset >= LJ_PROFILE_CH) return;
    lj_bar(b, LJ_W_SCROLL, d, LJ_HAND_SNAP);
    if (pal >= 0 && lj_palette_used(&p->palette[pal])) {
        r = lj_widget(b, LJ_W_PALETTE, d->offset, d->name, LJ_HAND_SNAP, LJ_STATES_ALL);
        r->lo = (uint8_t)pal;
    }
}

/* Static-function buttons: one per defined offset, caption = the function name. */
static int lj_sfunc_buttons(const lj_gs2_sfunc *s, const char *caption, lj_ctl_button **out)
{
    int i, n = 0;
    lj_ctl_button *bt;
    for (i = 0; i < LJ_PROFILE_CH; i++) if (s->defined[i]) n++;
    if (!n) { *out = NULL; return 0; }
    bt = lj_xalloc((size_t)n * sizeof *bt);
    n = 0;
    for (i = 0; i < LJ_PROFILE_CH; i++)
        if (s->defined[i]) { bt[n].caption = caption; bt[n].ch = (uint8_t)i; bt[n].value = bt[n].lo = bt[n].hi = s->value[i]; n++; }
    *out = bt;
    return n;
}

static int lj_sfunc_first(const lj_gs2_sfunc *s)
{
    int i;
    for (i = 0; i < LJ_PROFILE_CH; i++) if (s->defined[i]) return i;
    return -1;
}

static void lj_window_add(lj_ctl_window *w, int *nw, enum lj_ctl_kind kind, const char *caption, const char *const *menu, lj_rowbuf *rows, lj_rowbuf *specials)
{
    int i;
    if (!rows->n) {                                   /* no window: its special groups' buttons go too (LSan, W3 integration) */
        for (i = 0; specials && i < specials->n; i++) free((void *)specials->rows[i].buttons);
        free(rows->rows); free(specials ? specials->rows : NULL);
        return;
    }
    w[*nw].kind = kind; w[*nw].caption = caption; w[*nw].menu = menu;
    w[*nw].rows = rows->rows; w[*nw].nrows = rows->n;
    w[*nw].specials = specials ? specials->rows : NULL; w[*nw].nspecials = specials ? specials->n : 0;
    (*nw)++;
}

int lj_layout_derive(lj_profile *p)
{
    lj_ctl_window *w;
    lj_rowbuf b, sp;
    lj_ctl_row *r;
    lj_ctl_button *bt;
    int nw = 0, i, n, ch;
    if (!p) return -1;
    w = lj_xalloc(LJ_CTL_COUNT * sizeof *w);

    /* Intensity */
    memset(&b, 0, sizeof b);
    if (p->shutter.enabled && p->shutter.offset < LJ_PROFILE_CH) {
        r = lj_widget(&b, LJ_W_SHUTTER_TOGGLE, p->shutter.offset, "Shutter Closed", LJ_HAND_SNAP, LJ_STATES_ALL);
        r->caption2 = "Shutter Open";
        bt = lj_xalloc(2 * sizeof *bt);
        bt[0].caption = "Shutter Closed"; bt[0].ch = p->shutter.offset; bt[0].value = bt[0].lo = bt[0].hi = p->shutter.min;
        bt[1].caption = "Shutter Open";   bt[1].ch = p->shutter.offset; bt[1].value = bt[1].lo = bt[1].hi = p->shutter.max;
        r->buttons = bt; r->nbuttons = 2;
        r->lo = p->shutter.min < p->shutter.max ? p->shutter.min : p->shutter.max;
        r->hi = p->shutter.min < p->shutter.max ? p->shutter.max : p->shutter.min;
    }
    lj_bar(&b, LJ_W_DRAGBAR, &p->strobe, LJ_HAND_SNAP);
    lj_bar(&b, LJ_W_DRAGBAR, &p->intensity, LJ_HAND_FADE);
    lj_window_add(w, &nw, LJ_CTL_INTENSITY, "Intensity", lj_ctl_menu_std, &b, NULL);

    /* Movement */
    memset(&b, 0, sizeof b);
    if ((p->pt16 || p->pt8) && p->pan_ofs < LJ_PROFILE_CH) {
        r = lj_row_new(&b, LJ_W_FADEBTN, p->pan_ofs, "Pan/Tilt Fade", LJ_STATES_ALL);
        r->dflt_state = LJ_HAND_SNAP;
        b.last_fade = NULL;                                   /* misc_move gets its own button */
        lj_bar(&b, LJ_W_DRAGBAR, &p->misc_move, LJ_HAND_FADE);
    }
    lj_window_add(w, &nw, LJ_CTL_MOVEMENT, p->pt16 ? "Movement 16 Bit" : "Movement 8 Bit", lj_ctl_menu_move, &b, NULL);   /* 8-bit caption: PROPOSAL */

    /* Colors */
    memset(&b, 0, sizeof b);
    for (i = 0; i < 4; i++) lj_wheel(&b, &p->color[i], p, i < 2 ? i : -1);
    if (p->cmy_on || p->rgb_on) {
        static const char *const cmy_names[3] = { "Cyan", "Magenta", "Yellow" }, *const rgb_names[3] = { "Red", "Green", "Blue" };
        for (i = 0; i < 3; i++) {
            const lj_dyn *d = &p->cmy[i];
            if (!d->enabled || d->offset >= LJ_PROFILE_CH) continue;
            r = lj_widget(&b, LJ_W_DRAGBAR, d->offset, p->rgb_on ? rgb_names[i] : cmy_names[i], LJ_HAND_FADE, LJ_STATES_ALL);
            lj_range(r, d);
        }
    }
    lj_window_add(w, &nw, LJ_CTL_COLORS, "Colors", lj_ctl_menu_std, &b, NULL);

    /* Gobos */
    memset(&b, 0, sizeof b);
    for (i = 0; i < 4; i++) {
        if (i < 2) lj_wheel(&b, &p->gobo[i], p, 2 + i);
        else lj_bar(&b, LJ_W_INDEXBAR, &p->gobo[i], LJ_HAND_FADE);
    }
    lj_window_add(w, &nw, LJ_CTL_GOBOS, "Gobos", lj_ctl_menu_std, &b, NULL);

    /* Beam */
    memset(&b, 0, sizeof b);
    lj_bar(&b, LJ_W_DRAGBAR, &p->iris, LJ_HAND_FADE);
    lj_bar(&b, LJ_W_DRAGBAR, &p->focus, LJ_HAND_FADE);
    lj_bar(&b, LJ_W_DRAGBAR, &p->zoom, LJ_HAND_FADE);
    lj_bar(&b, LJ_W_DRAGBAR, &p->frost, LJ_HAND_FADE);
    for (i = 0; i < 2; i++) lj_bar(&b, LJ_W_DRAGBAR, &p->beam[i], LJ_HAND_FADE);
    lj_window_add(w, &nw, LJ_CTL_BEAM, "Beam", lj_ctl_menu_std, &b, NULL);

    /* Effects */
    memset(&b, 0, sizeof b);
    for (i = 0; i < 4; i++) {
        if (i < 2) lj_wheel(&b, &p->effects[i], p, 4 + i);
        else lj_bar(&b, LJ_W_DRAGBAR, &p->effects[i], LJ_HAND_FADE);
    }
    lj_window_add(w, &nw, LJ_CTL_EFFECTS, "Effects", lj_ctl_menu_std, &b, NULL);

    /* Levels: enabled, visible offsets below the footprint; specials -> the Special menu */
    memset(&b, 0, sizeof b);
    memset(&sp, 0, sizeof sp);
    for (ch = 0; ch < p->footprint && ch < LJ_PROFILE_CH; ch++) {
        int sw;
        if (p->level_disabled[ch] || p->level_invisible[ch]) continue;
        sw = p->channel_class[ch] == LJ_CH_SWITCH;
        r = lj_row_new(&b, LJ_W_LEVEL_FADER, ch, p->level_caption[ch][0] ? (const char *)p->level_caption[ch] : lj_hand_level_rows[ch].caption,
                       sw ? LJ_STATES_OFF_SNAP : LJ_STATES_ALL);
        r->hi = 255; r->dflt_state = sw ? LJ_HAND_SNAP : LJ_HAND_FADE;
    }
    for (i = 0; i < 32; i++) {
        if (!p->special[i].enabled || (ch = lj_sfunc_first(&p->special[i])) < 0) continue;
        r = lj_row_new(&sp, LJ_W_SPECIAL_GROUP, ch, p->special[i].name, LJ_STATES_OFF_SNAP);   /* snapped by default, never Fade (ME-27) */
        r->dflt_state = LJ_HAND_SNAP;
        r->nbuttons = lj_sfunc_buttons(&p->special[i], p->special[i].name, &bt);
        r->buttons = bt;
    }
    lj_window_add(w, &nw, LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, &b, &sp);

    /* Lamp Control: Off/Snap only, one button per defined offset of lamp on then lamp off */
    memset(&b, 0, sizeof b);
    if ((p->lamp_on.enabled && lj_sfunc_first(&p->lamp_on) >= 0) || (p->lamp_off.enabled && lj_sfunc_first(&p->lamp_off) >= 0)) {
        int non = 0, noff = 0;
        lj_ctl_button *bon = NULL, *boff = NULL;
        if (p->lamp_on.enabled) non = lj_sfunc_buttons(&p->lamp_on, "Power On", &bon);
        if (p->lamp_off.enabled) noff = lj_sfunc_buttons(&p->lamp_off, "Power Off", &boff);
        ch = non ? bon[0].ch : boff[0].ch;
        r = lj_widget(&b, LJ_W_LAMP_BTNS, ch, "Power On", LJ_HAND_OFF, LJ_STATES_OFF_SNAP);
        r->caption2 = "Power Off";
        n = non + noff;
        bt = lj_xalloc((size_t)n * sizeof *bt);
        if (non) memcpy(bt, bon, (size_t)non * sizeof *bt);
        if (noff) memcpy(bt + non, boff, (size_t)noff * sizeof *bt);
        free(bon); free(boff);
        r->buttons = bt; r->nbuttons = n;
        r = lj_row_new(&b, LJ_W_NOTE, ch, "NOTE ! Lamp Off must be held for at least 5 seconds", LJ_STATES_OFF_SNAP);
        (void)r;
    }
    lj_window_add(w, &nw, LJ_CTL_LAMP, "Lamp Control", lj_ctl_menu_help, &b, NULL);

    p->windows = w;
    p->nwindows = nw;
    return 0;
}
