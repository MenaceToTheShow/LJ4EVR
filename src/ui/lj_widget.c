/* lj_widget.c - tree ops, invalidate, hit-test, Align/Anchors layout as the VCL resolves them (design 2.7, 7.6). */
#include "ui/lj_widget.h"

#include <string.h>

const char *const lj_wev_names[LJ_WEV_COUNT] = {
    "OnClick", "OnDblClick", "OnChange", "OnEnter", "OnExit",
    "OnMouseDown", "OnMouseUp", "OnMouseMove", "OnMouseWheel", "OnMouseWheelDown", "OnMouseWheelUp",
    "OnKeyDown", "OnKeyUp", "OnKeyPress",
    "OnDragOver", "OnDragDrop", "OnStartDrag", "OnEndDrag",
    "OnSelectItem", "OnSelectCell", "OnDrawCell", "OnDrawItem", "OnCustomDrawItem", "OnCustomDrawSubItem",
    "OnColumnClick", "OnCompare", "OnResize", "OnPaint", "OnTimer", "OnPopup", "OnExecute",
    "OnClickCheck", "OnChanging", "OnMoved", "OnValueChange", "OnData", "OnDeletion",
    "OnSectionResize", "OnGetEditMask", "OnDrawTab", "OnApply",
    "OnCreate", "OnDestroy", "OnShow", "OnHide", "OnActivate", "OnDeactivate", "OnClose",
};

int lj_widget_add(lj_widget *parent, lj_widget *child)
{
    lj_widget **p;
    if (!parent || !child || child->parent || child == parent) return -1;
    for (p = &parent->first; *p; p = &(*p)->next) { }
    *p = child;
    child->next = NULL;
    child->parent = parent;
    return 0;
}

int lj_widget_remove(lj_widget *child)
{
    lj_widget **p;
    if (!child || !child->parent) return -1;
    for (p = &child->parent->first; *p && *p != child; p = &(*p)->next) { }
    if (!*p) return -1;
    *p = child->next;
    child->parent = NULL;
    child->next = NULL;
    return 0;
}

lj_widget *lj_widget_find(lj_widget *root, const char *name)
{
    lj_widget *c, *hit;
    if (!root || !name) return NULL;
    if (root->name && strcmp(root->name, name) == 0) return root;
    for (c = root->first; c; c = c->next)
        if ((hit = lj_widget_find(c, name)) != NULL) return hit;
    return NULL;
}

void lj_widget_abs(const lj_widget *w, int *x, int *y)
{
    int ax = 0, ay = 0;
    for (; w; w = w->parent) { ax += w->x; ay += w->y; }
    if (x) *x = ax;
    if (y) *y = ay;
}

void lj_widget_invalidate(lj_widget *w)
{
    if (w) w->dirty = 1;
}

/* A dirty widget's rect covers its children (VCL child controls are clipped to
 * the parent), so a dirty subtree is one rect and its children's flags are cleared too. */
static void clear_dirty(lj_widget *w)
{
    lj_widget *c;
    w->dirty = 0;
    for (c = w->first; c; c = c->next) clear_dirty(c);
}

static int collect(lj_widget *w, int ax, int ay, lj_rect *out, int cap, int n)
{
    lj_widget *c;
    if (!w->visible) { clear_dirty(w); return n; }
    if (w->dirty) {
        if (n < cap) { out[n].x = ax; out[n].y = ay; out[n].w = w->w; out[n].h = w->h; }
        clear_dirty(w);
        return n + 1;
    }
    for (c = w->first; c; c = c->next) n = collect(c, ax + c->x, ay + c->y, out, cap, n);
    return n;
}

int lj_widget_collect_dirty(lj_widget *root, lj_rect *out, int cap)
{
    if (!root) return 0;
    return collect(root, root->x, root->y, out, cap, 0);
}

/* VCL TWinControl.AlignControls / DoAlign InsertBefore (Delphi 5): the order inside one align group. */
static int align_before(const lj_widget *a, const lj_widget *b, lj_align which)
{
    switch (which) {
    case LJ_AL_TOP:    return a->y < b->y;
    case LJ_AL_BOTTOM: return a->y + a->h >= b->y + b->h;
    case LJ_AL_LEFT:   return a->x < b->x;
    case LJ_AL_RIGHT:  return a->x + a->w >= b->x + b->w;
    default:           return 0;                          /* alClient: creation order */
    }
}

#define LJ_ALIGN_GROUP_MAX 64   /* ponytail: aligned siblings of one group past this are not placed; the largest DFM group is 6, raise it if a code-built form needs more */

/* Align pass as TWinControl.AlignControls: the client rect shrinks as each
 * aligned child takes its edge; alTop, alBottom, alLeft, alRight, alClient, each
 * group ordered as DoAlign inserts it, from the children's current rects (design
 * 7.6, ME-43). Invisible children are skipped. */
static void align_pass(lj_widget *parent, lj_align which, lj_rect *r)
{
    lj_widget *list[LJ_ALIGN_GROUP_MAX], *c;
    int n = 0, i, j;
    for (c = parent->first; c; c = c->next) {
        if (!c->visible || c->align != which) continue;
        if (n == LJ_ALIGN_GROUP_MAX) break;   /* the rest of this group stays where it is */
        for (j = 0; j < n && !align_before(c, list[j], which); j++) { }
        for (i = n; i > j; i--) list[i] = list[i - 1];
        list[j] = c;
        n++;
    }
    for (i = 0; i < n; i++) {
        c = list[i];
        switch (which) {
        case LJ_AL_TOP:    c->x = r->x; c->w = r->w; c->y = r->y;              r->y += c->h; r->h -= c->h; break;
        case LJ_AL_BOTTOM: c->x = r->x; c->w = r->w; c->y = r->y + r->h - c->h; r->h -= c->h; break;
        case LJ_AL_LEFT:   c->y = r->y; c->h = r->h; c->x = r->x;              r->x += c->w; r->w -= c->w; break;
        case LJ_AL_RIGHT:  c->y = r->y; c->h = r->h; c->x = r->x + r->w - c->w; r->w -= c->w; break;
        case LJ_AL_CLIENT: c->x = r->x; c->y = r->y; c->w = r->w; c->h = r->h; break;
        default: break;
        }
        if (c->w < 0) c->w = 0;
        if (c->h < 0) c->h = 0;
    }
}

/* At creation the DFM rects are the VCL's own design-time align result, so only the
 * per-class hooks run (tab pages are code-placed); the align pass runs on a client-size
 * change through lj_widget_resize (ME-41; the per-class insets are W1, ME-43). */
void lj_widget_layout_hooks(lj_widget *parent)
{
    lj_widget *c;
    if (!parent) return;
    if (parent->vt && parent->vt->layout) parent->vt->layout(parent);
    for (c = parent->first; c; c = c->next) lj_widget_layout_hooks(c);
}

void lj_widget_layout(lj_widget *parent)
{
    static const lj_align order[5] = { LJ_AL_TOP, LJ_AL_BOTTOM, LJ_AL_LEFT, LJ_AL_RIGHT, LJ_AL_CLIENT };
    lj_rect r;
    lj_widget *c;
    int i;
    if (!parent) return;
    r.x = 0; r.y = 0; r.w = parent->w; r.h = parent->h;
    if (parent->vt && parent->vt->client) parent->vt->client(parent, &r);
    for (i = 0; i < 5; i++) align_pass(parent, order[i], &r);
    if (parent->vt && parent->vt->layout) parent->vt->layout(parent);
    for (c = parent->first; c; c = c->next) lj_widget_layout(c);
}

/* Anchors as TControl.UpdateAnchorRules/ChangeScale resolve them on a parent
 * resize: an anchored edge keeps its distance, both edges stretch, neither
 * keeps the centre in proportion. Aligned children are re-laid out afterwards. */
static int anchored_pos(int pos, int size, unsigned lo, unsigned hi, int old_parent, int new_parent, int *out_size)
{
    int delta = new_parent - old_parent;
    *out_size = size;
    if (lo && hi) { *out_size = size + delta; if (*out_size < 0) *out_size = 0; return pos; }
    if (hi) return pos + delta;
    if (lo) return pos;
    if (old_parent <= 0) return pos;
    return (pos + size / 2) * new_parent / old_parent - size / 2;
}

void lj_widget_resize(lj_widget *w, int new_w, int new_h)
{
    lj_widget *c;
    int old_w, old_h;
    if (!w) return;
    old_w = w->w; old_h = w->h;
    w->w = new_w; w->h = new_h;
    for (c = w->first; c; c = c->next) {
        unsigned a = c->anchors ? c->anchors : LJ_AK_DEFAULT;
        if (c->align != LJ_AL_NONE) continue;
        c->x = anchored_pos(c->x, c->w, a & LJ_AK_LEFT, a & LJ_AK_RIGHT,  old_w, new_w, &c->w);
        c->y = anchored_pos(c->y, c->h, a & LJ_AK_TOP,  a & LJ_AK_BOTTOM, old_h, new_h, &c->h);
    }
    lj_widget_layout(w);
    lj_widget_invalidate(w);
}

lj_widget *lj_widget_hit(lj_widget *root, int x, int y)
{
    lj_widget *c, *hit, *best = NULL;
    if (!root || !root->visible) return NULL;
    if (x < 0 || y < 0 || x >= root->w || y >= root->h) return NULL;   /* x,y relative to root's own origin */
    for (c = root->first; c; c = c->next)                      /* last declared wins: keep scanning */
        if ((hit = lj_widget_hit(c, x - c->x, y - c->y)) != NULL) best = hit;
    return best ? best : root;
}

lj_widget *lj_widget_focused(lj_widget *root)
{
    lj_widget *c, *f;
    if (!root) return NULL;
    if (root->focused) return root;
    for (c = root->first; c; c = c->next)
        if ((f = lj_widget_focused(c)) != NULL) return f;
    return NULL;
}

lj_widget *lj_widget_set_focus(lj_widget *w)
{
    lj_widget *root, *old;
    if (!w) return NULL;
    for (root = w; root->parent; root = root->parent) { }
    old = lj_widget_focused(root);
    if (old == w) return old;
    if (old) { old->focused = 0; lj_widget_invalidate(old); }
    if (w->visible && w->enabled) { w->focused = 1; lj_widget_invalidate(w); }
    return old;
}
