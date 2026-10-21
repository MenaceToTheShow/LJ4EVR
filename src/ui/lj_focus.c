/* lj_focus.c - TabOrder traversal, Default/Cancel lookup, focus moves with OnEnter/OnExit (design 7.9). */
#include "ui/lj_focus.h"

#include "ui/forms/lj_form.h"

static int focusable(const lj_widget *w) { return w->visible && w->enabled && w->tabstop; }

#define LJ_FOCUS_SIBLINGS 512   /* ponytail: the widest DFM container has well under 100 children; raise if lj_focus_order ever truncates */

/* Children in TabOrder (stable: DFM order breaks ties), each followed by its own
 * subtree, as TWinControl.GetTabOrderList walks it. */
static int walk(lj_widget *parent, lj_widget **out, int cap, int n)
{
    lj_widget *kids[LJ_FOCUS_SIBLINGS], *c;
    int k = 0, i, j;
    for (c = parent->first; c && k < LJ_FOCUS_SIBLINGS; c = c->next) {   /* insertion sort by tab_order, stable */
        for (i = k; i > 0 && kids[i - 1]->tab_order > c->tab_order; i--) kids[i] = kids[i - 1];
        kids[i] = c; k++;
    }
    for (j = 0; j < k; j++) {
        if (focusable(kids[j])) { if (n < cap) out[n] = kids[j]; n++; }
        if (kids[j]->visible) n = walk(kids[j], out, cap, n);
    }
    return n;
}

int lj_focus_order(lj_widget *root, lj_widget **out, int cap)
{
    if (!root) return 0;
    return walk(root, out, cap, 0);
}

#define LJ_FOCUS_CAP 512   /* ponytail: the largest DFM has ~300 rows; raise if lj_focus_order ever reports more */

lj_widget *lj_focus_next(lj_widget *root, lj_widget *from, int backward)
{
    lj_widget *chain[LJ_FOCUS_CAP];
    int n = lj_focus_order(root, chain, LJ_FOCUS_CAP), i;
    if (n > LJ_FOCUS_CAP) n = LJ_FOCUS_CAP;
    if (n == 0) return NULL;
    for (i = 0; i < n; i++) if (chain[i] == from) return chain[((i + (backward ? -1 : 1)) % n + n) % n];
    return backward ? chain[n - 1] : chain[0];
}

static lj_widget *find_flag(lj_widget *w, int want_cancel)
{
    lj_widget *c, *hit;
    if (!w || !w->visible) return NULL;
    if (w->desc && w->enabled && lj_desc_bool(want_cancel ? w->desc->cancel : w->desc->default_btn, 0)) return w;
    for (c = w->first; c; c = c->next) if ((hit = find_flag(c, want_cancel)) != NULL) return hit;
    return NULL;
}

lj_widget *lj_focus_default(lj_widget *root) { return find_flag(root, 0); }
lj_widget *lj_focus_cancel(lj_widget *root)  { return find_flag(root, 1); }

lj_widget *lj_focus_set(lj_widget *w)
{
    lj_widget *old;
    if (!w) return NULL;
    old = lj_widget_set_focus(w);
    if (old == w) return old;
    if (old && old->on_event) old->on_event(old, LJ_WEV_EXIT, old->ud);
    if (w->focused && w->on_event) w->on_event(w, LJ_WEV_ENTER, w->ud);
    return old;
}

void lj_focus_click(lj_widget *w)
{
    if (w && w->visible && w->enabled && w->on_event) w->on_event(w, LJ_WEV_CLICK, w->ud);
}
