/* lj_user_palettes.c - see lj_user_palettes.h. TPRESETFORM + TSAVEPRESETFORM2 bound (ME-16). */
#include "windows/lj_user_palettes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_fn.h"
#include "model/lj_palettes.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "windows/lj_warnings.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

static const char *lj_up_title(enum lj_ctl_kind k)   /* "User Color Palette" (usercolorpalette.gif); the other five follow it, PROPOSAL rig */
{
    switch (k) {
    case LJ_CTL_INTENSITY: return "User Intensity Palette";
    case LJ_CTL_COLORS:    return "User Color Palette";
    case LJ_CTL_GOBOS:     return "User Gobo Palette";
    case LJ_CTL_BEAM:      return "User Beam Palette";
    case LJ_CTL_EFFECTS:   return "User Effects Palette";
    case LJ_CTL_LEVELS:    return "User Level/Special Palette";
    default:               return NULL;
    }
}
static const char *lj_up_save_title(enum lj_ctl_kind k)   /* exe 0x1CCE50..0x1CCF00 */
{
    switch (k) {
    case LJ_CTL_INTENSITY: return "Save Intensity Palette Entry";
    case LJ_CTL_COLORS:    return "Save Color Palette Entry";
    case LJ_CTL_GOBOS:     return "Save Gobo Palette Entry";
    case LJ_CTL_BEAM:      return "Save Beam Palette Entry";
    case LJ_CTL_EFFECTS:   return "Save Effects Palette Entry";
    default:               return "Save Level/Special Palette Entry";
    }
}

static struct {
    lj_window *win, *save;
    enum lj_ctl_kind kind;
    lj_palette_list list;
    char text[256][40]; const char *items[256]; char entries[24];   /* ponytail: the first 256 entries are listed */
} up = { NULL, NULL, LJ_CTL_COLORS, { NULL, 0, 0 }, { { 0 } }, { NULL }, { 0 } };

lj_window *lj_user_palettes_window(void) { return up.win; }
int lj_user_palettes_count(void) { return up.win ? up.list.n : 0; }
static lj_widget *find(const char *n) { return lj_window_find(up.win, n); }

static int path_of(char *out, size_t cap)
{
    const lj_ctx *ctx = lj_ctx_get();
    char rel[64];
    const char *f = lj_palettes_file_name(up.kind);
    if (!ctx->lib || !f) return -1;
    snprintf(rel, sizeof rel, "Palettes/%s", f);   /* lj_library_path fixes the separator */
    return lj_library_path(ctx->lib, rel, out, cap);
}
static void save_file(void) { char p[LJ_PATH_CAP]; if (path_of(p, sizeof p) == 0 && lj_palettes_save(p, &up.list) != 0) lj_log("user palettes: cannot write %s", p); }

void lj_user_palettes_refresh(void)
{
    lj_widget *lv = find("lv");
    int i, n = up.list.n < 256 ? up.list.n : 256, keep;
    if (!up.win) return;
    for (i = 0; i < n; i++) { snprintf(up.text[i], sizeof up.text[i], "%02d\t%s", i + 1, up.list.e[i].name); up.items[i] = up.text[i]; }   /* "01 All White" (usercolorpalette.gif) */
    keep = lv ? lj_w_listview_index(lv) : -1;
    if (lv) { lj_w_listview_set_items(lv, up.items, n); ((lj_w_listview *)lv)->index = keep < n ? keep : -1; lj_widget_invalidate(lv); }
    snprintf(up.entries, sizeof up.entries, "%d Entries", up.list.n);
    lj_w_set_caption(find("lentries"), up.entries);
}

/* the kind's channel offsets in a profile: every row / special / button channel of its control window, + 16-bit LSBs */
static int kind_offsets(const lj_profile *p, uint8_t on[LJ_PROFILE_CH])
{
    const lj_ctl_window *w = p ? lj_profile_window(p, up.kind) : NULL;
    int i, j, n = 0;
    memset(on, 0, LJ_PROFILE_CH);
    if (!w) return 0;
    for (i = 0; i < w->nrows + w->nspecials; i++) {
        const lj_ctl_row *r = i < w->nrows ? &w->rows[i] : &w->specials[i - w->nrows];
        if (r->ch < LJ_PROFILE_CH) on[r->ch] = 1;
        for (j = 0; j < r->nbuttons; j++) if (r->buttons[j].ch < LJ_PROFILE_CH) on[r->buttons[j].ch] = 1;
    }
    for (i = 0; i < LJ_PROFILE_CH; i++) if (on[i] && p->pair_lsb[i] && p->pair_lsb[i] - 1 < LJ_PROFILE_CH) on[p->pair_lsb[i] - 1] = 1;
    for (i = 0; i < LJ_PROFILE_CH; i++) n += on[i];
    return n;
}
static int takes(int f, int filter) { return filter == 2 ? 1 : filter == 1 ? lj_sel_is_selected(f) : f == lj_sel_master(); }

/* visits every channel (address code) of the kind for the filtered fixtures; fn returns 1 when it counted */
static int each_channel(int filter, int (*fn)(int ch, void *ud), void *ud)
{
    const lj_show *sh = lj_ctx_get()->show;
    uint8_t on[LJ_PROFILE_CH];
    int f, o, n = 0;
    for (f = 0; sh && f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        int base = lj_show_fixture_base(sh, f);
        if (base < 0 || !takes(f, filter) || (p = lj_fixlib_by_id(sh->fixture[f].profile_id)) == NULL || !kind_offsets(p, on)) continue;
        for (o = 0; o < p->footprint && base + o < LJ_CHANS; o++) if (on[o]) n += fn(base + o, ud);
    }
    return n;
}
static int rec_ch(int ch, void *ud) { lj_user_palette *e = ud; lj_scene *s = lj_ctx_scene(); e->val[ch] = s->val[ch]; e->state[ch] = s->state[ch]; return 1; }
static int put_ch(int ch, void *ud) { const lj_user_palette *e = ud; lj_scene *s = lj_ctx_scene(); if (e->state[ch] == LJ_OFF) return 0; s->val[ch] = e->val[ch]; s->state[ch] = e->state[ch]; return 1; }   /* only Snap/Fade entries apply [04 s9] */
static int clr_ch(int ch, void *ud) { const lj_user_palette *e = ud; lj_scene *s = lj_ctx_scene(); if (e->state[ch] == LJ_OFF || s->state[ch] == LJ_OFF || s->val[ch] != e->val[ch]) return 0; s->state[ch] = LJ_OFF; return 1; }

int lj_user_palettes_record(int index, const char *name, int filter)
{
    lj_user_palette *e;
    if (!up.win || !lj_ctx_scene() || index >= up.list.n) return -1;
    e = (lj_user_palette *)calloc(1, sizeof *e);
    if (!e) lj_fatal("user palettes: out of memory");
    lj_lock();
    each_channel(filter, rec_ch, e);
    lj_unlock();
    if (index < 0) index = lj_palettes_add(&up.list, name ? name : "", e->val, e->state);
    else { memcpy(up.list.e[index].val, e->val, LJ_CHANS); memcpy(up.list.e[index].state, e->state, LJ_CHANS); if (name && name[0]) lj_str_copy(up.list.e[index].name, sizeof up.list.e[0].name, name); }
    free(e);
    if (index >= 0) { save_file(); lj_user_palettes_refresh(); }
    return index;
}

static int apply(int index, int filter, int (*fn)(int, void *))
{
    int n;
    if (!up.win || !lj_ctx_scene() || index < 0 || index >= up.list.n) return -1;
    lj_lock();
    n = each_channel(filter, fn, &up.list.e[index]);
    lj_unlock();
    if (n) lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    return n;
}
int lj_user_palettes_assign(int index, int filter) { return apply(index, filter, put_ch); }
int lj_user_palettes_clear(int index, int filter) { return apply(index, filter, clr_ch); }
int lj_user_palettes_delete(int index)
{
    if (!up.win || lj_palettes_delete(&up.list, index) != 0) return -1;
    save_file(); lj_user_palettes_refresh();
    return 0;
}

/* ---- TSavePresetForm2 ---- */
static int selected_row(void) { lj_widget *lv = find("lv"); int i = lv ? lj_w_listview_index(lv) : -1; return i < up.list.n ? i : -1; }
static int filter_now(void) { int f = find("rgFixtures") ? lj_w_radiogroup_index(find("rgFixtures")) : 1; return f < 0 ? 1 : f; }
static void save_close(void) { if (up.save) { lj_win_hide(up.save); lj_window_destroy(up.save); up.save = NULL; } }
static void save_click(lj_widget *w, void *ud)
{
    const char *name = up.save ? lj_w_edit_text(lj_window_find(up.save, "ePresetName")) : NULL;
    int as_new = up.save && w == lj_window_find(up.save, "bSaveNew");
    (void)ud;
    if (!up.save) return;
    lj_user_palettes_record(as_new ? -1 : selected_row(), name, filter_now());
    save_close();
}
static void save_form_close(lj_widget *w, void *ud) { (void)w; (void)ud; save_close(); }
static const lj_binding lj_up_save_bindings[] = {
    { "FormClose", save_form_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL },
    { "FormShow", lj_handler_noop, NULL }, { "bSaveClick", save_click, NULL }, { "PrefsClick", lj_handler_noop, NULL }, { "Help1Click", lj_help_viewer_click, "controls" }, { NULL, NULL, NULL } };
static void save_dialog(void)
{
    int i = selected_row();
    char no[12];
    save_close();
    up.save = lj_window_create(&lj_form_TSAVEPRESETFORM2, lj_up_save_bindings);
    if (!up.save) return;
    lj_window_set_caption(up.save, lj_up_save_title(up.kind));
    snprintf(no, sizeof no, "%02d", i >= 0 ? i + 1 : up.list.n + 1);
    lj_w_edit_set_text(lj_window_find(up.save, "ePresetNo"), no);
    lj_w_edit_set_text(lj_window_find(up.save, "ePresetName"), i >= 0 ? up.list.e[i].name : "");
    lj_win_show(up.save, 0);
}

/* ---- TPresetForm ---- */
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; save_dialog(); }
static void on_assign(lj_widget *w, void *ud) { (void)w; (void)ud; lj_user_palettes_assign(selected_row(), filter_now()); }
static void on_clear(lj_widget *w, void *ud) { (void)w; (void)ud; lj_user_palettes_clear(selected_row(), filter_now()); }
static void on_delete_yes(int yes, void *ud) { if (yes) lj_user_palettes_delete((int)(intptr_t)ud); }
static void on_delete(lj_widget *w, void *ud) { (void)w; (void)ud; if (selected_row() >= 0) lj_win_confirm("Please Confirm..", "Delete Selected Presets?", NULL, on_delete_yes, (void *)(intptr_t)selected_row()); }   /* exe 0x346794 (ME-97); ponytail: the highlighted entry (single-select list) */
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_user_palettes_close(); }
static void on_help(lj_widget *w, void *ud) { (void)ud; lj_help_viewer_click(w, (void *)"controls"); }   /* W4 acceptance: the bundled viewer */
static const lj_binding lj_up_bindings[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "lvChange", lj_handler_noop, NULL }, { "lvDblClick", on_assign, NULL }, { "bSaveClick", on_save, NULL }, { "bAssignClick", on_assign, NULL },
    { "bClearClick", on_clear, NULL }, { "Help1Click", on_help, NULL }, { "LvPopPopup", lj_handler_noop, NULL }, { "P_DeletePalettesClick", on_delete, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_user_palettes_open_kind(enum lj_ctl_kind kind)
{
    char p[LJ_PATH_CAP], err[160];
    if (!lj_up_title(kind)) return NULL;
    if (up.win && up.kind == kind) return up.win;
    lj_user_palettes_close();
    up.kind = kind;
    up.win = lj_window_create(&lj_form_TPRESETFORM, lj_up_bindings);
    if (!up.win) { lj_log("user palettes: cannot create TPresetForm"); return NULL; }
    lj_window_set_caption(up.win, lj_up_title(kind));
    lj_w_set_caption(find("bAssign"), "Assign");
    if (path_of(p, sizeof p) != 0 || lj_palettes_load(p, &up.list, err, sizeof err) != 0) up.list.n = 0;   /* a missing file = no entries yet */
    lj_user_palettes_refresh();
    lj_win_show(up.win, 0);
    return up.win;
}
lj_window *lj_user_palettes_open(void) { return lj_user_palettes_open_kind(up.kind); }

void lj_user_palettes_close(void)
{
    save_close();
    if (!up.win) return;
    lj_win_hide(up.win);
    lj_window_destroy(up.win);
    up.win = NULL;
    lj_palettes_free(&up.list);
}

static void fn_palette(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    enum lj_ctl_kind k = fn == LJ_FN_TOGGLE_INTENSITY_PALETTE ? LJ_CTL_INTENSITY : fn == LJ_FN_TOGGLE_COLOR_PALETTE ? LJ_CTL_COLORS : fn == LJ_FN_TOGGLE_GOBO_PALETTE ? LJ_CTL_GOBOS
                       : fn == LJ_FN_TOGGLE_BEAM_PALETTE ? LJ_CTL_BEAM : fn == LJ_FN_TOGGLE_EFFECT_PALETTE ? LJ_CTL_EFFECTS : LJ_CTL_LEVELS;
    (void)prog; (void)ud;
    if (!edge) return;
    if (up.win && up.kind == k) lj_user_palettes_close(); else lj_user_palettes_open_kind(k);
}
void lj_user_palettes_bind_fns(void)
{
    static const enum lj_fn rows[] = { LJ_FN_TOGGLE_INTENSITY_PALETTE, LJ_FN_TOGGLE_COLOR_PALETTE, LJ_FN_TOGGLE_GOBO_PALETTE, LJ_FN_TOGGLE_BEAM_PALETTE, LJ_FN_TOGGLE_EFFECT_PALETTE, LJ_FN_TOGGLE_LEVELS_PALETTE };
    int i;
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_fn_bind(rows[i], fn_palette, NULL);
}
