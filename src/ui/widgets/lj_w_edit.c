/* lj_w_edit.c - TEdit (7.3, 7.9): 2-px sunken frame, interior clWindow or the DFM Color
 * (clSilver on read-only fields), text at +3 vertically centred, navy selection with
 * white text, 1-px caret when focused, #808080 text on a face interior when disabled.
 * Keys: chars insert (CP1252 32..255), Back/Delete, arrows/Home/End with Shift
 * extending; mouse sets the caret, drags select, double-click selects the word.
 * TNumEdit and TMaskEdit set `variant` and reuse everything (lj_w_edit_insert). */
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "core/lj_str.h"
#include "gfx/lj_draw.h"

#define LJ_EDIT_PAD 3

static int sel_lo(const lj_w_edit *e) { return e->caret < e->anchor ? e->caret : e->anchor; }
static int sel_hi(const lj_w_edit *e) { return e->caret < e->anchor ? e->anchor : e->caret; }

static void shown(const lj_w_edit *e, char *out)
{
    if (e->password) { memset(out, '*', (size_t)e->len); out[e->len] = 0; }
    else memcpy(out, e->text, (size_t)e->len + 1);
}

static int width_of(const lj_font *f, const char *s, int from, int to)
{
    char buf[256];
    memcpy(buf, s + from, (size_t)(to - from));
    buf[to - from] = 0;
    return lj_text_width(f, buf);
}

static void changed(lj_w_edit *e)
{
    lj_widget_invalidate(&e->w);
    lj_w_emit(&e->w, LJ_WEV_CHANGE);
    if (e->variant == LJ_W_EDIT_NUM) {
        int v = 0, i = 0, neg = e->text[0] == '-';
        for (i = neg; i < e->len && e->text[i] >= '0' && e->text[i] <= '9'; i++)
            if (v < 100000000) v = v * 10 + (e->text[i] - '0');
        if (neg) v = -v;
        if (v < e->min) v = e->min;
        if (v > e->max) v = e->max;
        if (v != e->value) { e->value = v; lj_w_emit(&e->w, LJ_WEV_VALUE_CHANGE); }
    }
}

static void delete_range(lj_w_edit *e, int lo, int hi)
{
    if (e->variant == LJ_W_EDIT_MASK) {                        /* overwrite mode: editable positions go blank */
        int i;
        for (i = lo; i < hi; i++) if (e->mclass[i]) e->text[i] = e->blank;
    } else {
        memmove(e->text + lo, e->text + hi, (size_t)(e->len - hi + 1));
        e->len -= hi - lo;
    }
    e->caret = e->anchor = lo;
}

static int mask_accepts(char cls, int ch)
{
    int digit = ch >= '0' && ch <= '9', alpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch >= 0xC0;
    switch (cls) {
    case 'd': return digit;
    case 'a': return alpha;
    case 'n': return digit || alpha;
    case 'c': return ch >= 32;
    default:  return 0;
    }
}

int lj_w_edit_insert(lj_w_edit *e, int ch)
{
    if (e->readonly || ch < 32 || ch > 255) return 0;
    if (e->caret != e->anchor) delete_range(e, sel_lo(e), sel_hi(e));
    switch (e->variant) {
    case LJ_W_EDIT_NUM:
        if (!((ch >= '0' && ch <= '9') || (ch == '-' && e->min < 0 && e->caret == 0 && e->text[0] != '-'))) return 0;
        break;
    case LJ_W_EDIT_MASK:
        while (e->caret < e->len && !e->mclass[e->caret]) e->caret++;
        if (e->caret >= e->len || !mask_accepts(e->mclass[e->caret], ch)) { e->anchor = e->caret; return 0; }
        e->text[e->caret++] = (char)ch;
        e->anchor = e->caret;
        changed(e);
        return 1;
    default: break;
    }
    if (e->len >= e->max_length) return 0;
    memmove(e->text + e->caret + 1, e->text + e->caret, (size_t)(e->len - e->caret + 1));
    e->text[e->caret++] = (char)ch;
    e->anchor = e->caret;
    e->len++;
    changed(e);
    return 1;
}

static void paint(lj_widget *w, lj_surface *s)
{
    lj_w_edit *e = (lj_w_edit *)w;
    const lj_font *f = lj_w_font_of(w);
    uint32_t interior = w->enabled ? w->color : LJ_CLR_BTNFACE, ink = w->enabled ? lj_w_fg(w) : LJ_CLR_BTNSHADOW;
    char buf[256];
    int x, y, tx, ty, lo = sel_lo(e), hi = sel_hi(e);
    lj_widget_abs(w, &x, &y);
    lj_edge_sunken(s, x, y, w->w, w->h, interior);
    tx = x + LJ_EDIT_PAD; ty = y + (w->h - f->height) / 2;
    shown(e, buf);
    lj_text(s, f, tx, ty, buf, ink, 0);
    if (lo != hi && (w->focused || !e->hide_sel)) {
        int sx = tx + width_of(f, buf, 0, lo), sw = width_of(f, buf, lo, hi);
        char sel[256];
        memcpy(sel, buf + lo, (size_t)(hi - lo)); sel[hi - lo] = 0;
        lj_fill(s, sx, ty, sw, f->height, LJ_CLR_HIGHLIGHT);
        lj_text(s, f, sx, ty, sel, LJ_CLR_BTNHIGHLIGHT, 0);
    }
    if (w->focused) lj_vline(s, tx + width_of(f, buf, 0, e->caret), ty, f->height, LJ_CLR_3DDKSHADOW);
}

static int pos_at(const lj_w_edit *e, int mx)
{
    const lj_font *f = lj_w_font_of(&e->w);
    char buf[256];
    int i, acc = LJ_EDIT_PAD;
    shown(e, buf);
    for (i = 0; i < e->len; i++) {
        int cw = width_of(f, buf, i, i + 1);
        if (mx < acc + cw / 2) return i;
        acc += cw;
    }
    return e->len;
}

static int mouse(lj_widget *w, const lj_mouse_ev *m)
{
    lj_w_edit *e = (lj_w_edit *)w;
    if (!w->enabled) return 0;
    lj_w_plain_mouse(w, m);
    if (m->type == LJ_EV_MOUSE_DOWN && m->button == 1) {
        lj_w_focus(w);
        e->caret = e->anchor = pos_at(e, m->x);
        e->pressed = 1;
        if (m->clicks == 2) {                                  /* select the word under the caret */
            int lo = e->caret, hi = e->caret;
            while (lo > 0 && e->text[lo - 1] != ' ') lo--;
            while (hi < e->len && e->text[hi] != ' ') hi++;
            e->anchor = lo; e->caret = hi;
        }
        lj_widget_invalidate(w);
    } else if (m->type == LJ_EV_MOUSE_MOVE && e->pressed) {
        e->caret = pos_at(e, m->x);
        lj_widget_invalidate(w);
    } else if (m->type == LJ_EV_MOUSE_UP) {
        e->pressed = 0;
    }
    return 1;
}

static int key(lj_widget *w, const lj_key_ev *k)
{
    lj_w_edit *e = (lj_w_edit *)w;
    int lo = sel_lo(e), hi = sel_hi(e);
    if (k->type == LJ_EV_KEY_UP) { lj_w_emit(w, LJ_WEV_KEY_UP); return 0; }
    if (k->type == LJ_EV_CHAR) {
        lj_w_emit(w, LJ_WEV_KEY_PRESS);
        if (k->ch < 32) return 0;
        lj_w_edit_insert(e, k->ch);
        return 1;
    }
    lj_w_emit(w, LJ_WEV_KEY_DOWN);
    switch (k->key) {
    case LJ_VK_LEFT:  if (e->caret > 0) e->caret--; break;
    case LJ_VK_RIGHT: if (e->caret < e->len) e->caret++; break;
    case LJ_VK_HOME:  e->caret = 0; break;
    case LJ_VK_END:   e->caret = e->len; break;
    case LJ_VK_BACK:
        if (e->readonly) return 1;
        if (lo != hi) delete_range(e, lo, hi);
        else if (e->caret > 0) {
            if (e->variant == LJ_W_EDIT_MASK) { do e->caret--; while (e->caret > 0 && !e->mclass[e->caret]); }
            else e->caret--;
            delete_range(e, e->caret, e->caret + 1);
        } else return 1;
        changed(e);
        return 1;
    case LJ_VK_DELETE:
        if (e->readonly) return 1;
        if (lo != hi) delete_range(e, lo, hi);
        else if (e->caret < e->len) delete_range(e, e->caret, e->caret + 1);
        else return 1;
        changed(e);
        return 1;
    default:
        return 0;
    }
    if (!k->shift) e->anchor = e->caret;
    lj_widget_invalidate(w);
    return 1;
}

const lj_widget_vt lj_w_edit_vt = { paint, mouse, key, NULL, NULL };

const char *lj_w_edit_text(const lj_widget *w)
{
    return w && (w->kind == LJ_K_EDIT || w->kind == LJ_K_NUMEDIT || w->kind == LJ_K_MASKEDIT) ? ((const lj_w_edit *)w)->text : NULL;
}

int lj_w_edit_int(const lj_widget *w, int lo, int hi, int dflt)
{
    const char *t = lj_w_edit_text(w);
    char *end;
    long v;
    if (!t) return dflt;
    errno = 0;
    v = strtol(t, &end, 10);
    while (*end == ' ') end++;
    return end != t && !*end && errno != ERANGE && v >= lo && v <= hi ? (int)v : dflt;
}

int lj_w_edit_set_text(lj_widget *w, const char *s)
{
    lj_w_edit *e = (lj_w_edit *)w;
    int i;
    if (!lj_w_edit_text(w)) return -1;
    if (e->variant == LJ_W_EDIT_MASK) {                        /* fixed length: pad with blanks, literals stay */
        for (i = 0; i < e->len; i++)
            if (e->mclass[i]) e->text[i] = s && (int)strlen(s) > i && mask_accepts(e->mclass[i], (unsigned char)s[i]) ? s[i] : e->blank;
    } else {
        lj_str_copy(e->text, (size_t)e->max_length + 1, s ? s : "");
        e->len = (int)strlen(e->text);
    }
    e->caret = e->anchor = 0;
    changed(e);
    return 0;
}

void lj_w_edit_select(lj_widget *w, int start, int len)
{
    lj_w_edit *e = (lj_w_edit *)w;
    if (!lj_w_edit_text(w)) return;
    if (start < 0) start = 0;
    if (start > e->len) start = e->len;
    e->anchor = start;
    e->caret = len < 0 || start + len > e->len ? e->len : start + len;
    lj_widget_invalidate(w);
}

lj_widget *lj_w_edit_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_edit *e = lj_w_alloc(sizeof(lj_w_edit), parent, d, &lj_w_edit_vt, 1);
    e->max_length = 255;
    e->hide_sel = 1;
    if (d) {
        e->w.color = lj_desc_rgb(d->color, LJ_CLR_BTNHIGHLIGHT);
        if (d->max_length > 0 && d->max_length < 255) e->max_length = d->max_length;
        e->readonly = lj_desc_bool(d->readonly, 0) != 0;
        e->hide_sel = lj_desc_bool(d->hide_selection, 1) != 0;
        e->password = d->password_char && d->password_char[0];
        if (d->text) { lj_str_copy(e->text, (size_t)e->max_length + 1, d->text); e->len = (int)strlen(e->text); }
    } else {
        e->w.color = LJ_CLR_BTNHIGHLIGHT;
    }
    return &e->w;
}
