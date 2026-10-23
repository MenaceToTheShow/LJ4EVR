/* lj_selection.c - the desktop click rules of [01 s3.2] (CHM "Selecting fixtures on the desktop"). */
#include "desktop/lj_selection.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_ini.h"
#include "core/lj_str.h"
#include "fixlib/lj_profile.h"
#include "model/lj_show.h"

static uint8_t  lj_selected[LJ_FIXTURES];
static int      lj_master = -1, lj_mode = LJ_SEL_INCLUSIVE, lj_solo;
static uint32_t lj_serial;

static int used(int i)
{
    const lj_ctx *ctx = lj_ctx_get();
    return i >= 0 && i < LJ_FIXTURES && ctx->show && ctx->show->fixture[i].link != 0;
}

static void bump(void) { lj_serial++; lj_desktop_icons_refresh(); lj_desktop_toolbar_update(); }

int lj_sel_master(void) { return lj_master; }
int lj_sel_is_selected(int i) { return i >= 0 && i < LJ_FIXTURES ? lj_selected[i] : 0; }
int lj_sel_mode(void) { return lj_mode; }
void lj_sel_mode_set(enum lj_sel_mode mode) { lj_mode = mode == LJ_SEL_EXCLUSIVE ? LJ_SEL_EXCLUSIVE : LJ_SEL_INCLUSIVE; bump(); }
int lj_sel_solo(void) { return lj_solo; }
void lj_sel_solo_set(int on) { lj_solo = on != 0; bump(); }
uint32_t lj_sel_serial(void) { return lj_serial; }

int lj_sel_count(void)
{
    int i, n = 0;
    for (i = 0; i < LJ_FIXTURES; i++) n += lj_selected[i];
    return n;
}

static void pick_master(void)   /* the master left the selection: the lowest selected fixture takes over, else none */
{
    int i;
    lj_master = -1;
    for (i = 0; i < LJ_FIXTURES; i++) if (lj_selected[i]) { lj_master = i; return; }
}

void lj_sel_set(int i, int selected, int make_master)
{
    if (!used(i)) return;
    lj_selected[i] = (uint8_t)(selected != 0);
    if (selected && (make_master || lj_master < 0)) lj_master = i;   /* "Only one fixture can be master fixture at any time" */
    else if (!selected && lj_master == i) pick_master();
    bump();
}

static void clear_all(void) { memset(lj_selected, 0, sizeof lj_selected); lj_master = -1; }

void lj_sel_click(int i, int shift, int ctrl)
{
    if (!used(i)) return;
    if (ctrl) { clear_all(); lj_selected[i] = 1; lj_master = i; bump(); return; }      /* Ctrl+click: solo fixture select */
    if (shift) { lj_selected[i] = 1; lj_master = i; bump(); return; }                 /* Shift+click: force master */
    if (lj_selected[i]) { lj_selected[i] = 0; if (lj_master == i) pick_master(); bump(); return; }   /* a selected icon de-selects */
    if (lj_mode == LJ_SEL_EXCLUSIVE) clear_all();
    lj_selected[i] = 1; lj_master = i;                                                 /* "This fixture now becomes the 'Master fixture'" */
    bump();
}

void lj_sel_band(int x0, int y0, int x1, int y1, int ctrl)
{
    int i, caught = 0, first = -1;
    lj_rect r;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    if (ctrl || lj_mode == LJ_SEL_EXCLUSIVE) clear_all();
    for (i = 0; i < LJ_FIXTURES; i++) {
        int in;
        if (!used(i) || !lj_ctx_get()->show->fixture[i].placed) continue;
        lj_desktop_icon_rect(i, &r);
        if (r.w <= 0) continue;
        in = ctrl ? r.x >= x0 && r.y >= y0 && r.x + r.w <= x1 && r.y + r.h <= y1                /* Ctrl: "within rubber band frame only" */
                  : r.x < x1 && r.x + r.w > x0 && r.y < y1 && r.y + r.h > y0;
        if (!in) continue;
        lj_selected[i] = 1; caught++;
        if (first < 0) first = i;
    }
    if (!caught) clear_all();                              /* "Rubber-band empty desktop deselects all" */
    else if (lj_master < 0 || !lj_selected[lj_master]) lj_master = first;
    bump();
}

void lj_sel_all(void)
{
    int i;
    for (i = 0; i < LJ_FIXTURES; i++) if (used(i)) lj_selected[i] = 1;
    if (lj_master < 0 || !lj_selected[lj_master]) pick_master();
    bump();
}

void lj_sel_none(void) { clear_all(); bump(); }

static lj_nextprev_opts lj_np = { 1, 0, LJ_NP_TABS_ANY, 0 };
static int lj_np_order[LJ_FIXTURES * 2], lj_np_n;   /* fixture index or LJ_SEL_BREAK */

static int np_match(int k)   /* the dialog's filters, relative to the master */
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_fixture *f;
    if (!used(k)) return 0;
    f = &ctx->show->fixture[k];
    if (lj_np.same_type && lj_master >= 0 && !lj_sel_same_type(k, lj_master)) return 0;
    if (lj_np.tabs == LJ_NP_TABS_CURRENT && (!f->placed || f->tab != lj_desktop_tab())) return 0;
    if (lj_np.tabs == LJ_NP_TABS_SAME && lj_master >= 0 && (!f->placed || f->tab != ctx->show->fixture[lj_master].tab)) return 0;
    return 1;
}

static void step(int dir)   /* [01 s3.3]: by fixture number from the master (nothing selected: lowest for Next, highest for Previous), or
                             * through the custom order, where Selection Breaks make Next / Previous loop inside the master's block */
{
    int ring[LJ_FIXTURES * 2], n = 0, i, at = -1, pick = -1;
    if (lj_np.custom && lj_np_n > 0) {
        int lo = 0, hi = lj_np_n, m = -1;
        for (i = 0; i < lj_np_n; i++) if (lj_np_order[i] == lj_master) { m = i; break; }
        if (m >= 0) {
            for (lo = m; lo > 0 && lj_np_order[lo - 1] != LJ_SEL_BREAK; lo--) { }
            for (hi = m; hi < lj_np_n && lj_np_order[hi] != LJ_SEL_BREAK; hi++) { }
        }
        for (i = lo; i < hi; i++) if (lj_np_order[i] != LJ_SEL_BREAK) ring[n++] = lj_np_order[i];
    } else for (i = 0; i < LJ_FIXTURES; i++) ring[n++] = i;
    for (i = 0; i < n; i++) if (ring[i] == lj_master) { at = i; break; }
    for (i = 1; i <= n; i++) {
        int k = at < 0 ? ring[dir > 0 ? i - 1 : n - i] : ring[((at + dir * i) % n + n) % n];
        if (np_match(k)) { pick = k; break; }
    }
    if (pick < 0) return;
    clear_all();
    lj_selected[pick] = 1; lj_master = pick;
    bump();
}
void lj_sel_next(void) { step(1); }
void lj_sel_prev(void) { step(-1); }

const lj_profile *lj_sel_master_profile(void)
{
    const lj_ctx *ctx = lj_ctx_get();
    if (!used(lj_master)) return NULL;
    return lj_show_profile(ctx->show, lj_master);                      /* built-in or GS2 (ME-110) */
}

int lj_sel_same_type(int a, int b)
{
    const lj_ctx *ctx = lj_ctx_get();
    const lj_fixture *fa, *fb;
    if (!used(a) || !used(b)) return 0;
    fa = &ctx->show->fixture[a]; fb = &ctx->show->fixture[b];
    return fa->kind == fb->kind && fa->profile_id == fb->profile_id && strcmp(fa->gs2_file, fb->gs2_file) == 0;
}

void lj_sel_reset(void) { clear_all(); lj_mode = LJ_SEL_INCLUSIVE; lj_solo = 0; bump(); }

/* ---- W4 dlg-desktop: Next/Previous options, custom order, named custom selections (lj_selection.h) ---- */
static int list_parse(const char *t, int *out, int cap, int breaks)   /* "3,7,0,12" fixture numbers (1-based) -> indices; 0 = break; untrusted text */
{
    int n = 0;
    while (t && *t && n < cap) {
        int v = 0, digits = 0;
        while (*t >= '0' && *t <= '9' && digits < 4) { v = v * 10 + (*t++ - '0'); digits++; }
        if (digits && v == 0 && breaks) out[n++] = LJ_SEL_BREAK;
        else if (digits && v >= 1 && v <= LJ_FIXTURES) out[n++] = v - 1;
        while (*t && *t != ',') t++;
        if (*t == ',') t++;
    }
    return n;
}
static void list_text(char *dst, size_t cap, const int *e, int n)
{
    size_t len = 0;
    int i;
    dst[0] = 0;
    for (i = 0; i < n && len + 6 < cap; i++) len += (size_t)snprintf(dst + len, cap - len, "%s%d", i ? "," : "", e[i] == LJ_SEL_BREAK ? 0 : e[i] + 1);
}

static void np_save(void)
{
    lj_ini *ini = lj_ctx_get()->main_ini;
    char t[LJ_FIXTURES * 2 * 5];
    lj_ini_seti(ini, "NEXTPREV", "Enabled", lj_np.enabled); lj_ini_seti(ini, "NEXTPREV", "SameType", lj_np.same_type);
    lj_ini_seti(ini, "NEXTPREV", "Tabs", lj_np.tabs); lj_ini_seti(ini, "NEXTPREV", "Order", lj_np.custom);
    list_text(t, sizeof t, lj_np_order, lj_np_n);
    lj_ini_sets(ini, "NEXTPREV", "Custom", t);
}
void lj_sel_nextprev_load(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_np.enabled = lj_ini_geti(ctx->main_ini, "NEXTPREV", "Enabled", 1) != 0; lj_np.same_type = lj_ini_geti(ctx->main_ini, "NEXTPREV", "SameType", 0) != 0;
    lj_np.tabs = lj_ini_geti(ctx->main_ini, "NEXTPREV", "Tabs", 0); lj_np.custom = lj_ini_geti(ctx->main_ini, "NEXTPREV", "Order", 0) != 0;
    if (lj_np.tabs < LJ_NP_TABS_ANY || lj_np.tabs > LJ_NP_TABS_CURRENT) lj_np.tabs = LJ_NP_TABS_ANY;
    lj_np_n = list_parse(lj_ini_gets(ctx->main_ini, "NEXTPREV", "Custom", ""), lj_np_order, LJ_FIXTURES * 2, 1);
    ctx->show_nextprev = (uint8_t)lj_np.enabled;
}
void lj_sel_nextprev_get(lj_nextprev_opts *o) { *o = lj_np; }
void lj_sel_nextprev_set(const lj_nextprev_opts *o)
{
    lj_np = *o;
    lj_ctx_get()->show_nextprev = (uint8_t)(o->enabled != 0);
    np_save();
    lj_desktop_toolbar_update();
}
int lj_sel_custom_order(int *out, int cap) { int n = lj_np_n < cap ? lj_np_n : cap; memcpy(out, lj_np_order, sizeof *out * (size_t)(n > 0 ? n : 0)); return n; }
void lj_sel_custom_order_set(const int *e, int n)
{
    int i;
    lj_np_n = 0;
    for (i = 0; i < n && lj_np_n < LJ_FIXTURES * 2; i++) if (e[i] == LJ_SEL_BREAK || (e[i] >= 0 && e[i] < LJ_FIXTURES)) lj_np_order[lj_np_n++] = e[i];
    np_save();
}

int lj_custom_sel_count(void) { int n = lj_ini_geti(lj_ctx_get()->main_ini, "CUSTOMSEL", "Count", 0); return n < 0 ? 0 : n > LJ_CUSTOM_SEL_MAX ? LJ_CUSTOM_SEL_MAX : n; }
const char *lj_custom_sel_name(int no)
{
    char key[16];
    if (no < 1 || no > lj_custom_sel_count()) return "";
    snprintf(key, sizeof key, "Name%d", no);
    return lj_ini_gets(lj_ctx_get()->main_ini, "CUSTOMSEL", key, "");
}
int lj_custom_sel_get(int no, int *out, int cap)
{
    char key[16];
    if (no < 1 || no > lj_custom_sel_count()) return -1;
    snprintf(key, sizeof key, "Fix%d", no);
    return list_parse(lj_ini_gets(lj_ctx_get()->main_ini, "CUSTOMSEL", key, ""), out, cap, 0);
}
int lj_custom_sel_save(int no, const char *name, const int *fix, int n)
{
    lj_ini *ini = lj_ctx_get()->main_ini;
    char key[16], t[LJ_FIXTURES * 5], nm[LJ_CUSTOM_SEL_NAME];
    int count = lj_custom_sel_count();
    if (!ini || !name || !name[0] || n < 0 || n > LJ_FIXTURES) return -1;
    if (no < 1 || no > count) { if (count >= LJ_CUSTOM_SEL_MAX) return -1; no = ++count; lj_ini_seti(ini, "CUSTOMSEL", "Count", count); }
    lj_str_copy(nm, sizeof nm, name);
    snprintf(key, sizeof key, "Name%d", no); lj_ini_sets(ini, "CUSTOMSEL", key, nm);
    list_text(t, sizeof t, fix, n);
    snprintf(key, sizeof key, "Fix%d", no); lj_ini_sets(ini, "CUSTOMSEL", key, t);
    return no;
}
void lj_custom_sel_clear_all(void) { lj_ini_seti(lj_ctx_get()->main_ini, "CUSTOMSEL", "Count", 0); }
