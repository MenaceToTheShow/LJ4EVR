/* lj_config_dialogs.c - see lj_config_dialogs.h. The 25 CONFIG windows of docs/w4-contracts.md section 4 (dlg-config track):
 * the Fixture Configuration helpers (Add Fixture(s), Calculate addresses F43, Find profile, Copy From Fixture / Copy
 * Fixture / Clear Fixture Data F44, the save-time Warnings F41, Applying Default DMX), the icon popup boxes (Fixture
 * Info, Fixture Notes, Fixture Special, the Scanner 1 / Color Scroller / RGB pack definitions), the profile editor's
 * pickers (Palette Value Types, Select Bitmap, Get Bitmap, Apply user profile, Oldstyle profile name), the profile
 * import / export boxes (File Exists, File Already Exists, Warning), Patch Sub Masters and the RGB Palette. Every OnXxx a
 * DFM declares is bound (ME-16); a bkOK / bkCancel BitBtn without OnClick is wired through its raw on_event. Captions
 * and prompts are the DFM / exe literals; text the sources do not give is tagged PROPOSAL. Bitmaps are our own art by
 * name (D2, ME-18): nothing of Martin's is read. */
#define _CRT_SECURE_NO_WARNINGS 1
#include "windows/lj_config_dialogs.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_selection.h"
#include "fixlib/lj_gs2.h"
#include "gfx/lj_font.h"
#include "gfx/lj_glyphs.h"
#include "model/lj_genmacro.h"
#include "model/lj_library.h"
#include "model/lj_presets.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_dipswitch.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_shape.h"
#include "ui/widgets/lj_w_udbutton.h"
#include "windows/lj_fixture_config.h"
#include "windows/lj_warnings.h"
#include "windows/lj_help_viewer.h"   /* W4 acceptance: Help items open the bundled viewer (design 13 item 12) */

#define CD_FIRST LJ_WIN_ADD_FIXTURES
#define CD_N     ((int)LJ_WIN_RGB_PALETTE - (int)LJ_WIN_ADD_FIXTURES + 1)
#define CD_NA    "Not available in LJ4EVR"
#define CD_ROWS  256

static lj_window *cd_win[CD_N];
static int cd_target = -1;
static lj_show *show(void) { return lj_ctx_get()->show; }
static int cd_idx(enum lj_win id) { int i = (int)id - (int)CD_FIRST; return i >= 0 && i < CD_N ? i : -1; }
static lj_widget *cw(enum lj_win id, const char *n) { int i = cd_idx(id); return i >= 0 && cd_win[i] ? lj_window_find(cd_win[i], n) : NULL; }
static void close_ev(lj_widget *w, int ev, void *ud) { (void)w; if (ev == LJ_WEV_CLICK) lj_config_dlg_close((enum lj_win)(intptr_t)ud); }
static void btn(enum lj_win id, const char *name, void (*fn)(lj_widget *, int, void *)) { lj_widget *b = cw(id, name); if (b) { b->on_event = fn; b->ud = (void *)(intptr_t)id; } }
static void set_en(lj_widget *w, int on) { if (w) { w->enabled = on != 0; lj_widget_invalidate(w); } }
static void grey(lj_widget *w) { if (w) { w->enabled = 0; w->hint = CD_NA; lj_widget_invalidate(w); } }
static int used(const lj_fixture *f) { return f->link != 0 || f->profile_id != 0 || f->kind == LJ_FX_GS2; }
static int footprint(int f) { const lj_profile *p = lj_show_profile(show(), f); return p && p->footprint ? p->footprint : 1; }
static const char *fx_text(int f, char *buf, size_t cap)   /* "Martin Ego X5" (fixtureinfo.gif): manufacturer, name, ", mode" */
{
    const lj_profile *p = show() ? lj_show_profile(show(), f) : NULL;
    if (!p) { lj_str_copy(buf, cap, "?"); return buf; }
    snprintf(buf, cap, "%s %s%s%s", p->manufacturer, p->name, p->mode[0] ? ", " : "", p->mode);
    return buf;
}
static int ascii_lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int icontains(const char *s, const char *needle)
{
    size_t i, n = strlen(needle);
    for (; *s; s++) { for (i = 0; i < n && s[i] && ascii_lower((unsigned char)s[i]) == ascii_lower((unsigned char)needle[i]); i++) { } if (i == n) return 1; }
    return n == 0;
}
static int fx_ini(const char *key, int dflt) { return lj_ini_geti(lj_ctx_get()->forms_ini, "FIXTURECONF", key, dflt); }

/* =====================================================================================================================
 * F44: programming data keyed by DMX address [02 s1.4, s2.9-2.10]
 * ===================================================================================================================== */
static void seq_apply(lj_sequence *s, int sc, int dc, int src, int dst, int fp)
{
    int k, c;
    for (k = 0; k < s->n; k++) {
        lj_scene *x = &s->scenes[k];
        for (c = 0; c < fp && dc + c < LJ_CHANS; c++) { x->val[dc + c] = src >= 0 ? x->val[sc + c] : 0; x->state[dc + c] = src >= 0 ? x->state[sc + c] : (uint8_t)LJ_OFF; }
        x->preset[dst] = src >= 0 ? x->preset[src] : 0;
        if (src >= 0) x->macro[dst] = x->macro[src]; else memset(&x->macro[dst], 0, sizeof x->macro[dst]);
    }
}
static void gm_apply(lj_genmacro *m, int sc, int dc, int src, int fp)
{
    int c;
    for (c = 0; c < fp && dc + c < LJ_CHANS; c++) { if (src >= 0) m->ch[dc + c] = m->ch[sc + c]; else memset(&m->ch[dc + c], 0, sizeof m->ch[dc + c]); }
}
/* every record of the kind on disk, then the copies in memory under the lock (the engine reads them by pointer, 2.3) */
static int records_apply(enum lj_rec_kind kind, int sc, int dc, int src, int dst, int fp)
{
    static uint32_t nums[4096];
    lj_library *lib = lj_ctx_get()->lib;
    lj_show *sh = show();
    char path[LJ_PATH_CAP], err[160];
    int n = lib ? lj_library_list_records(lib, kind, nums, (int)LJ_COUNT_OF(nums)) : 0, i, rc = 0;
    for (i = 0; i < n; i++) {
        if (lj_library_record_path(lib, kind, nums[i], path, sizeof path) != 0) { rc = -1; continue; }
        if (kind == LJ_REC_SEQ) {
            lj_sequence *s = lj_sequence_new_rec();
            if (lj_sequence_load(path, s, err, sizeof err) == 0) { seq_apply(s, sc, dc, src, dst, fp); if (lj_sequence_save(path, s) != 0) rc = -1; }
            else { lj_log("fixture data: %s", err); rc = -1; }
            lj_sequence_free(s);
        } else {
            lj_genmacro *m = lj_genmacro_new_rec();
            if (lj_genmacro_load(path, m, err, sizeof err) == 0) { gm_apply(m, sc, dc, src, fp); if (lj_genmacro_save(path, m) != 0) rc = -1; }
            else { lj_log("fixture data: %s", err); rc = -1; }
            free(m);
        }
    }
    lj_lock();
    for (i = 0; sh && i < lj_table_count(kind == LJ_REC_SEQ ? &sh->sequences : &sh->macros); i++) {
        void *r = lj_table_at(kind == LJ_REC_SEQ ? &sh->sequences : &sh->macros, i, NULL);
        if (!r) continue;
        if (kind == LJ_REC_SEQ) { if ((lj_sequence *)r != sh->current_seq) seq_apply((lj_sequence *)r, sc, dc, src, dst, fp); }
        else gm_apply((lj_genmacro *)r, sc, dc, src, fp);
    }
    if (sh && kind == LJ_REC_SEQ && sh->current_seq) seq_apply(sh->current_seq, sc, dc, src, dst, fp);
    lj_unlock();
    return rc;
}

int lj_config_dlg_fixture_data(int src, int dst, unsigned what)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *sh = show();
    char rel[48], path[LJ_PATH_CAP];
    int dc, sc = 0, fp, c, rc = 0;
    if (!sh || dst < 0 || dst >= LJ_FIXTURES || src >= LJ_FIXTURES || (dc = lj_show_fixture_base(sh, dst)) < 0) return -1;
    if (src >= 0 && ((sc = lj_show_fixture_base(sh, src)) < 0 || !lj_sel_same_type(src, dst))) return -1;   /* "only if the two fixtures are of an identical type" */
    fp = footprint(dst);
    if (what & LJ_FXD_SEQ) rc |= records_apply(LJ_REC_SEQ, sc, dc, src, dst, fp);
    if (what & LJ_FXD_MACROS) rc |= records_apply(LJ_REC_MACRO, sc, dc, src, dst, fp);
    if (what & LJ_FXD_PRESETS) {
        lj_lock();
        if (src >= 0) memcpy(sh->preset[dst], sh->preset[src], sizeof sh->preset[dst]); else memset(sh->preset[dst], 0, sizeof sh->preset[dst]);
        lj_unlock();
        snprintf(rel, sizeof rel, "Presets\\fixture%03d.pre", dst + 1);   /* lj_show_load's preset file name */
        if (ctx->lib && lj_library_path(ctx->lib, rel, path, sizeof path) == 0 && lj_presets_save(path, sh->preset[dst]) != 0) rc = -1;
    }
    if ((what & LJ_FXD_SETUP) && src >= 0) {                  /* "all setup specific data (except DMX address)" */
        lj_fixture *d = &sh->fixture[dst];
        const lj_fixture *s = &sh->fixture[src];
        lj_lock();
        d->invert_pan = s->invert_pan; d->invert_tilt = s->invert_tilt; d->swap_pt = s->swap_pt; d->ignore_bo = s->ignore_bo; d->ignore_gm = s->ignore_gm;
        d->link_strobe_gm = s->link_strobe_gm; d->intensity_group = s->intensity_group;
        memcpy(d->custom, s->custom, sizeof d->custom); memcpy(d->ch_caption, s->ch_caption, sizeof d->ch_caption); d->ch_gm_off = s->ch_gm_off;
        lj_unlock();
        lj_desktop_run_action(LJ_ACT_FIXTURE_APPLY, dst);     /* Config.dat + Setup.<slot>.pal, rebind */
        lj_fixture_config_mark_saved();
    }
    if (what & LJ_FXD_MISC) {                                  /* default DMX output; ponytail: the statics' recorded scenes are not touched (Statics.dat keeps its group lines through lj_statics_groups_save only) */
        lj_lock();
        for (c = 0; c < fp && dc + c < LJ_CHANS; c++) sh->default_out[dc + c] = src >= 0 ? sh->default_out[sc + c] : 0;
        lj_unlock();
        lj_desktop_run_action(LJ_ACT_DEFAULT_OUT_APPLY, 0);
    }
    return rc ? -1 : 0;
}

/* =====================================================================================================================
 * F43 / F41: address calculation and the save-time warnings
 * ===================================================================================================================== */
int lj_config_dlg_auto_address(int f, unsigned links, int *link, int *address)
{
    lj_show *sh = show();
    int l, a, j, fp;
    if (!sh || f < 0 || f >= LJ_FIXTURES || !used(&sh->fixture[f])) return -1;
    fp = footprint(f);                                         /* Generic Scanner 1 is 8 (lj_generics.c): the 8 are always taken */
    for (l = 1; l <= LJ_LINKS; l++) {
        if (links && !(links >> (l - 1) & 1u)) continue;
        for (a = 1; a + fp - 1 <= LJ_LINK_CH; a++) {
            for (j = 0; j < LJ_FIXTURES; j++) {                /* other fixtures only: the global patch and a scroller's own address are not looked at [02 s2.5] */
                const lj_fixture *o = &sh->fixture[j];
                int ofp;
                if (j == f || !used(o) || o->link != l || !o->address) continue;
                ofp = footprint(j);
                if (a < o->address + ofp && o->address < a + fp) { a = o->address + ofp - 1; break; }
            }
            if (j == LJ_FIXTURES) { *link = l; *address = a; return 0; }
        }
    }
    return -1;
}

static char cd_warn[LJ_FIXTURES + LJ_CHANS][64];
int lj_config_dlg_warnings(const char **rows, int max)
{
    static uint8_t owners[LJ_CHANS];
    lj_show *sh = show();
    int i, c, n = 0;
#define CD_WARN(...) do { if (n < (int)LJ_COUNT_OF(cd_warn)) { snprintf(cd_warn[n], sizeof cd_warn[0], __VA_ARGS__); if (rows && n < max) rows[n] = cd_warn[n]; n++; } } while (0)
    if (!sh) return 0;
    memset(owners, 0, sizeof owners);
    for (i = 0; i < LJ_FIXTURES; i++) {                        /* configurationwarnings.gif: per fixture in number order, then the shared addresses */
        const lj_fixture *f = &sh->fixture[i];
        int fp;
        if (!used(f)) continue;
        fp = footprint(i);
        if (!f->link || !f->address) { CD_WARN("Fixture %d has no DMX Address", i + 1); continue; }   /* exe 0x1B398C */
        if (f->address + fp - 1 > LJ_LINK_CH) { CD_WARN("Fixture %d has invalid DMX Address", i + 1); continue; }   /* exe 0x1B39A8 */
        for (c = 0; c < fp; c++) if (owners[lj_addr_code(f->link, f->address + c)] < 2) owners[lj_addr_code(f->link, f->address + c)]++;
    }
    for (c = 0; c < LJ_CHANS; c++) if (owners[c] > 1) CD_WARN("DMX address %d:%03d is used by more than one fixture", c / LJ_LINK_CH + 1, c % LJ_LINK_CH + 1);   /* exe 0x1B39E4 */
#undef CD_WARN
    return n;
}

/* =====================================================================================================================
 * the per-id state and behaviour
 * ===================================================================================================================== */
static struct {
    int profile;                                              /* Add Fixture(s) */
    int alink, aaddr, afound;                                 /* Calculate addresses */
    int copy_src;                                             /* Copy From -> Copy Fixture */
    int dflt_list[LJ_FIXTURES], dflt_n;                       /* Applying Default DMX */
    void (*pick_done)(const char *, void *); void *pick_ud; int pick_colors;
    void (*icon_done)(const char *, void *); void *icon_ud;
    unsigned char *vt_codes; void (*vt_done)(void *); void *vt_ud; unsigned char vt_work[5]; char vt_caption[48];
    void (*am_apply)(int, void *); void *am_ud; int am_master, am_id_only;
    void (*os_done)(const char *, const char *, void *); void *os_ud; char os_legacy[64];
    void (*im_done)(int, void *); void *im_ud; char im_file[96];
    void (*fe_done)(int, void *); void *fe_ud; char fe_path[LJ_PATH_CAP];
    void (*ew_done)(int, void *); void *ew_ud;
    /* list storage shared by the list dialogs (one of each open at a time) */
    char rows[CD_ROWS][128]; const char *rowp[CD_ROWS]; int row_id[CD_ROWS], nrows;
    char rows2[32][64]; const char *rowp2[32]; int nrows2;
    char text[4096]; const char *lines[128]; int nlines;
} cd;
static void rows_reset(void) { cd.nrows = 0; }
static void row_add(int id, const char *fmt, ...)
{
    va_list ap;
    if (cd.nrows >= CD_ROWS) return;
    va_start(ap, fmt); vsnprintf(cd.rows[cd.nrows], sizeof cd.rows[0], fmt, ap); va_end(ap);
    cd.rowp[cd.nrows] = cd.rows[cd.nrows]; cd.row_id[cd.nrows++] = id;
}
static void rows_to(lj_widget *lv) { if (!lv) return; if (lv->kind == LJ_K_LISTBOX) lj_w_list_set_items(lv, cd.rowp, cd.nrows); else lj_w_listview_set_items(lv, cd.rowp, cd.nrows); lj_widget_invalidate(lv); }
static int row_sel(lj_widget *lv) { int r = !lv ? -1 : lv->kind == LJ_K_LISTBOX ? ((lj_w_list *)lv)->index : lj_w_listview_index(lv); return r >= 0 && r < cd.nrows ? r : -1; }
/* text -> memo lines, word-wrapped to the memo's text column (lj_w_memo does not wrap; TRichEdit WordWrap) */
static void memo_text(lj_widget *re, const char *text)
{
    const lj_font *f = re ? lj_w_font_of(re) : NULL;
    int maxw = re ? re->w - 4 - 6 - LJ_SB_W : 0;
    char *s, *q, *brk;
    cd.nlines = 0;
    lj_str_copy(cd.text, sizeof cd.text, text);
    for (s = cd.text; *s && cd.nlines < (int)LJ_COUNT_OF(cd.lines); ) {
        char *nl = strchr(s, '\n');
        if (nl) *nl = 0;
        brk = NULL;
        if (f) for (q = strchr(s, ' '); q; q = strchr(q + 1, ' ')) { *q = 0; if (lj_text_width(f, s) > maxw) { *q = ' '; break; } *q = ' '; brk = q; }
        if (f && brk && lj_text_width(f, s) > maxw) { *brk = 0; cd.lines[cd.nlines++] = s; s = brk + 1; if (nl) *nl = '\n'; continue; }
        cd.lines[cd.nlines++] = s;
        if (!nl) break;
        s = nl + 1;
    }
    if (re) { lj_w_memo_set_lines(re, cd.lines, cd.nlines); ((lj_w_memo *)re)->vscroll = (unsigned)(cd.nlines * ((lj_w_memo *)re)->line_h > re->h - 8); lj_widget_invalidate(re); }   /* the bar on demand, as a RichEdit shows it */
}

/* ---- Add Fixture(s) (TAddFixForm2) [02 s2.3] ---- */
#define AF LJ_WIN_ADD_FIXTURES
static int edit_int(lj_widget *e, int dflt) { const char *t = e ? lj_w_edit_text(e) : NULL; char *end = NULL; long v; if (!t || !t[0]) return dflt; v = strtol(t, &end, 10); return !*end && v >= 0 && v <= 99999 ? (int)v : dflt; }
static void edit_int_set(lj_widget *e, int v) { char t[12]; snprintf(t, sizeof t, "%d", v); if (e) lj_w_edit_set_text(e, t); }
static void af_ud(lj_widget *w, void *ud)
{
    lj_widget *e = cw(AF, strcmp(w->name, "UdButton1") == 0 ? "ENum" : "eStart");
    int v = edit_int(e, 1) + lj_w_udbutton_delta(w);
    (void)ud;
    edit_int_set(e, v < 1 ? 1 : v > LJ_FIXTURES ? LJ_FIXTURES : v);
}
static void af_ok(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    int n = edit_int(cw(AF, "ENum"), 1), f = edit_int(cw(AF, "eStart"), 1) - 1, over = lj_w_radiogroup_index(cw(AF, "GP1")) == 0, done = 0;
    (void)w; (void)ud;
    for (; sh && f >= 0 && f < LJ_FIXTURES && done < n; f++)
        if (over || !used(&sh->fixture[f])) { if (lj_fixture_config_put(f, (uint16_t)cd.profile) == 0) done++; }
    lj_config_dlg_close(AF);
    lj_fixture_config_refresh();
}
static const lj_binding af_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "UdButton1Change", af_ud, NULL }, { "UdButton2Change", af_ud, NULL }, { "bOkClick", af_ok, NULL }, { NULL, NULL, NULL } };
static void af_init(lj_window *win)
{
    const lj_profile *p = lj_fixlib_by_id((uint16_t)cd.profile);
    lj_show *sh = show();
    char t[160];
    int f = 0;
    lj_window_set_caption(win, "Add Fixture (s)");             /* addfixturesdialog.gif (the DFM caption is the placeholder "AddFixForm2") */
    snprintf(t, sizeof t, "%s %s%s%s", p ? p->manufacturer : "", p ? p->name : "?", p && p->mode[0] ? ", " : "", p ? p->mode : "");
    lj_window_set_text(win, "lFixtureName", t);
    while (sh && f < LJ_FIXTURES - 1 && used(&sh->fixture[f])) f++;
    edit_int_set(cw(AF, "eStart"), f + 1);
    btn(AF, "bCancel", close_ev);
}
void lj_config_dlg_add_fixtures(int profile_id) { cd.profile = profile_id; lj_win_open(AF); }

/* ---- Calculate addresses (TAutoAddressForm2) F43 ---- */
#define AA LJ_WIN_AUTO_ADDRESS
static void aa_link(lj_widget *w, void *ud)
{
    static const char *const l[4] = { "cbL1", "cbL2", "cbL3", "cbL4" };
    int i, any = 1;
    (void)ud;
    if (strcmp(w->name, "cbAny") == 0) { if (lj_w_checkbox_checked(w)) for (i = 0; i < 4; i++) { lj_widget *c = cw(AA, l[i]); void (*fn)(lj_widget *, int, void *) = c ? c->on_event : NULL; if (c) { c->on_event = NULL; lj_w_checkbox_set_checked(c, 0); c->on_event = fn; } } return; }
    for (i = 0; i < 4; i++) if (lj_w_checkbox_checked(cw(AA, l[i]))) any = 0;
    { lj_widget *c = cw(AA, "cbAny"); void (*fn)(lj_widget *, int, void *) = c ? c->on_event : NULL; if (c) { c->on_event = NULL; lj_w_checkbox_set_checked(c, any); c->on_event = fn; } }   /* PROPOSAL rig: a link box clears Any */
}
static void aa_find(lj_widget *w, void *ud)
{
    static const char *const l[4] = { "cbL1", "cbL2", "cbL3", "cbL4" };
    unsigned mask = 0;
    int i, f = cd_target;
    char t[96];
    (void)w; (void)ud;
    if (!lj_w_checkbox_checked(cw(AA, "cbAny"))) for (i = 0; i < 4; i++) if (lj_w_checkbox_checked(cw(AA, l[i]))) mask |= 1u << i;
    rows_reset();
    cd.afound = lj_config_dlg_auto_address(f, mask, &cd.alink, &cd.aaddr) == 0;
    if (cd.afound) row_add(f, "%02d\t%s\t%d:%03d-%03d", f + 1, fx_text(f, t, sizeof t), cd.alink, cd.aaddr, cd.aaddr + footprint(f) - 1);   /* autoaddressing.gif "01 | Martin MAC 250 Krypton (1... | 1:001-014" */
    rows_to(cw(AA, "LV1"));
    set_en(cw(AA, "bOk"), cd.afound);
    if (!cd.afound) lj_log("calculate addresses: no free range for fixture %d", f + 1);
}
static void aa_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (cd.afound) lj_fixture_config_set_address(cd_target, cd.alink, cd.aaddr); lj_config_dlg_close(AA); }
static const lj_binding aa_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "cbLinkCkick", aa_link, NULL }, { "bFindClick", aa_find, NULL },
    { "bOkClick", aa_ok, NULL }, { "LV1Click", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static void aa_init(lj_window *win)
{
    (void)win;
    cd_target = lj_fixture_config_selected();
    lj_w_checkbox_set_checked(cw(AA, "cbAny"), 1);            /* autoaddressing.gif: Any checked */
    cd.afound = 0; rows_reset(); rows_to(cw(AA, "LV1"));
    btn(AA, "BitBtn2", close_ev);
}

/* ---- Find profile.. (TFindProfileForm) ---- */
#define FP LJ_WIN_FIND_PROFILE
static void fp_find(lj_widget *w, void *ud)
{
    const char *q = lj_w_edit_text(cw(FP, "eFind"));
    int i, n = lj_fixture_config_tree_count();
    (void)w; (void)ud;
    rows_reset();
    for (i = 0; i < n; i++) {                                  /* the tree's leaves in tree order: by manufacturer, case-insensitive [02 s2.3] */
        int id = lj_fixture_config_tree_id(i), k;
        const lj_exe_row *r = NULL;
        if (id < 0) continue;
        for (k = 0; k < lj_fixlib_count() && !r; k++) if (lj_fixlib_row(k) && lj_fixlib_row(k)->id == id) r = lj_fixlib_row(k);
        if (!r || !(icontains(r->manufacturer ? r->manufacturer : "", q ? q : "") || icontains(r->name, q ? q : ""))) continue;
        row_add(id, "%s\t%s %s%s%s", r->manufacturer && r->manufacturer[0] ? r->manufacturer : "Other", r->manufacturer ? r->manufacturer : "", r->name, r->mode && r->mode[0] ? ", " : "", r->mode ? r->mode : "");   /* findprofiledialog.gif: "Acme | Acme MH-640B The Rover" */
    }
    rows_to(cw(FP, "lvFound"));
}
static void fp_select(lj_widget *w, void *ud) { int r = row_sel(w); (void)ud; if (r >= 0) lj_fixture_config_tree_select(cd.row_id[r]); }
static const lj_binding fp_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "bFindClick", fp_find, NULL },
    { "lvFoundMouseDown", lj_handler_noop, NULL }, { "lvFoundSelectItem", fp_select, NULL }, { NULL, NULL, NULL } };
static void fp_init(lj_window *win) { (void)win; rows_reset(); rows_to(cw(FP, "lvFound")); }

/* ---- Select source fixture (TCopyFromForm) -> Copy Fixture (TCloneForm); Clear Fixture Data (TCleanUpForm) F44 ---- */
#define CF LJ_WIN_COPY_FROM
#define CL LJ_WIN_CLONE
#define CU LJ_WIN_CLEANUP
static void cfm_change(lj_widget *w, void *ud) { (void)ud; set_en(cw(CF, "bOk"), row_sel(w) >= 0); }
static void cfm_ok(lj_widget *w, void *ud)
{
    int r = row_sel(cw(CF, "LV1"));
    (void)w; (void)ud;
    if (r < 0) return;
    cd.copy_src = cd.row_id[r];
    lj_config_dlg_close(CF);
    lj_win_open(CL);
}
static const lj_binding cfm_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "LV1Change", cfm_change, NULL }, { "bOkClick", cfm_ok, NULL }, { "Heklp1Click", lj_help_viewer_click, "fixtures" } /* ME-141 */, { NULL, NULL, NULL } };
static void cfm_init(lj_window *win)
{
    lj_show *sh = show();
    int f;
    (void)win;
    rows_reset();
    for (f = 0; sh && f < LJ_FIXTURES; f++)                    /* identical type, saved with an address, not the target [02 s2.9] */
        if (f != cd_target && used(&sh->fixture[f]) && sh->fixture[f].address && lj_sel_same_type(f, cd_target)) row_add(f, "%d\t%s", f + 1, sh->fixture[f].user_name);   /* copyfixture.gif "1 | Front 518" */
    rows_to(cw(CF, "LV1"));
    btn(CF, "bcancel", close_ev);
}
static const char *const cl_box[5] = { "cbSeq", "cbPresets", "cbSetup", "cbMacros", "cbMisc" };
static const char *const cl_pg[5] = { "pgSeq", "pgPresets", "pgSetup", "pgMacros", "pgMisc" };
static const unsigned cl_bit[5] = { LJ_FXD_SEQ, LJ_FXD_PRESETS, LJ_FXD_SETUP, LJ_FXD_MACROS, LJ_FXD_MISC };
static void cl_run(int yes, void *ud)
{
    unsigned what = 0;
    int i;
    (void)ud;
    if (!yes || !cd_win[cd_idx(CL)]) return;
    for (i = 0; i < 5; i++) if (lj_w_checkbox_checked(cw(CL, cl_box[i]))) what |= cl_bit[i];
    if (lj_config_dlg_fixture_data(cd.copy_src, cd_target, what) != 0) lj_log("copy fixture: %d -> %d: not every record was written", cd.copy_src + 1, cd_target + 1);
    for (i = 0; i < 5; i++) if (what & cl_bit[i]) lj_w_progressbar_set(cw(CL, cl_pg[i]), 100);
    lj_config_dlg_close(CL);
}
static void cl_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", "Copy fixture, are you sure?", NULL, cl_run, NULL); }   /* exe 0x1A429C */
static void cl_box_click(lj_widget *w, void *ud) { int i, any = 0; (void)w; (void)ud; for (i = 0; i < 5; i++) any |= lj_w_checkbox_checked(cw(CL, cl_box[i])); set_en(cw(CL, "bOk"), any); }
static const lj_binding cl_b[] = { { "FormShow", lj_handler_noop, NULL }, { "bOkClick", cl_ok, NULL }, { "cbSeqClick", cl_box_click, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void cl_init(lj_window *win)
{
    lj_show *sh = show();
    char t[64];
    int i;
    (void)win;
    snprintf(t, sizeof t, "Copy From #%d %s", cd.copy_src + 1, sh && cd.copy_src >= 0 ? sh->fixture[cd.copy_src].user_name : "");   /* exe 0x1A4088 "Copy From #"; copyfixture2.gif "Copy From #7 MAC2000" */
    lj_window_set_text(win, "lSource", t);
    for (i = 0; i < 5; i++) lj_w_checkbox_set_checked(cw(CL, cl_box[i]), 1);   /* copyfixture2.gif: every box checked */
    btn(CL, "bCancel", close_ev);
}
static const char *const cu_box[4] = { "cbClearSeq", "cbClearPreset", "cbClearMacro", "cbClearMisc" };
static const char *const cu_pg[4] = { "PgSeq", "pgPresets", "pgMacro", "PgMisc" };
static const unsigned cu_bit[4] = { LJ_FXD_SEQ, LJ_FXD_PRESETS, LJ_FXD_MACROS, LJ_FXD_MISC };
static void cu_run(int yes, void *ud)
{
    unsigned what = 0;
    int i;
    (void)ud;
    if (!yes || !cd_win[cd_idx(CU)]) return;
    if (cw(CU, "lAction")) cw(CU, "lAction")->visible = 1;
    if (cw(CU, "pgAction")) cw(CU, "pgAction")->visible = 1;
    for (i = 0; i < 4; i++) if (lj_w_checkbox_checked(cw(CU, cu_box[i]))) what |= cu_bit[i];
    if (lj_config_dlg_fixture_data(-1, cd_target, what) != 0) lj_log("clear fixture data: %d: not every record was written", cd_target + 1);
    for (i = 0; i < 4; i++) if (what & cu_bit[i]) lj_w_progressbar_set(cw(CU, cu_pg[i]), 100);
    lj_w_progressbar_set(cw(CU, "pgAction"), 100);   /* the box closes right after: the running line never shows headless */
    lj_config_dlg_close(CU);
}
static void cu_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", "Clear selected fixtures, are you sure?", NULL, cu_run, NULL); }   /* exe 0x1A3C54 */
static void cu_box_click(lj_widget *w, void *ud) { int i, any = 0; (void)w; (void)ud; for (i = 0; i < 4; i++) any |= lj_w_checkbox_checked(cw(CU, cu_box[i])); set_en(cw(CU, "bOk"), any); }
static const lj_binding cu_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "bOkClick", cu_ok, NULL }, { "cbClearSeqClick", cu_box_click, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void cu_init(lj_window *win)
{
    char t[40];
    int i;
    lj_widget *a = cw(CU, "lAction"), *pa = cw(CU, "pgAction");
    snprintf(t, sizeof t, "Clearing Fixture #%d", cd_target + 1);   /* the DFM's "Clearing Fixture #" with the number (PROPOSAL rig) */
    lj_window_set_text(win, "lAction", t);
    if (a) a->visible = 0;                                     /* clearfixturedata.gif: the four boxes and their bars only; the action line is the running state */
    if (pa) pa->visible = 0;
    for (i = 0; i < 4; i++) lj_w_checkbox_set_checked(cw(CU, cu_box[i]), 1);   /* clearfixturedata.gif: every box checked */
    btn(CU, "bCancel", close_ev);
}

/* ---- Warnings (TConfigWarnForm) F41 ---- */
#define CW LJ_WIN_CONFIG_WARN
static const char *cw_rows[LJ_FIXTURES + LJ_CHANS];
static void cw_save(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev != LJ_WEV_CLICK) return; lj_config_dlg_close(CW); lj_fixture_config_save_anyway(); }
static const lj_binding cw_b[] = { { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void cw_init(lj_window *win)
{
    int n = lj_config_dlg_warnings(cw_rows, (int)LJ_COUNT_OF(cw_rows));
    (void)win;
    lj_w_list_set_items(cw(CW, "lbWarnings"), cw_rows, n);
    btn(CW, "bSave", cw_save); btn(CW, "bCancel", close_ev);   /* "&Save  Anyway" (DFM; the 2.1 capture reads "Save Ayway") / "&Cancel" */
}

/* ---- Applying Default DMX - please wait (TApplyDefaultDMXForm) ---- */
#define AD LJ_WIN_APPLY_DEFAULT_DMX
static const lj_binding ad_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
void lj_config_dlg_default_dmx(const int *fixtures, int n)
{
    lj_show *sh = show();
    char t[16];
    int i, c;
    cd.dflt_n = 0;
    for (i = 0; i < n && i < LJ_FIXTURES; i++) cd.dflt_list[cd.dflt_n++] = fixtures[i];
    lj_win_open(AD);
    lj_lock();
    for (i = 0; sh && i < cd.dflt_n; i++) {                    /* the profile's own default values [02 s2.11] */
        int f = cd.dflt_list[i], base = lj_show_fixture_base(sh, f);
        const lj_profile *p = lj_show_profile(sh, f);
        if (base < 0 || !p || !sh->fixture[f].address) continue;
        for (c = 0; c < p->footprint && c < LJ_PROFILE_CH && base + c < LJ_CHANS; c++) sh->default_out[base + c] = p->default_dmx[c];
    }
    lj_unlock();
    if (cd.dflt_n) { snprintf(t, sizeof t, "%d", cd.dflt_list[cd.dflt_n - 1] + 1); lj_window_set_text(cd_win[cd_idx(AD)], "lFixture", t); }
    lj_w_progressbar_set(cw(AD, "pg1"), 100);
    lj_desktop_run_action(LJ_ACT_DEFAULT_OUT_APPLY, 0);        /* posts LJ_CMD_DEFAULT_OUT and saves DefaultOut.dat */
}

/* ---- Fixture Info / Fixture Profile Notes / Fixture Special / Hidden fixture (icon popup, target fixture) ---- */
#define FI LJ_WIN_FIXTURE_INFO
#define FN LJ_WIN_FIXTURE_NOTES
#define FS LJ_WIN_FIXTURE_SPECIAL
static void fi_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(FI); }
static const lj_binding fi_b[] = { { "FormClose", fi_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "sbIdMouseDown", lj_handler_noop, NULL }, { "sbIdMouseUp", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static void fi_init(lj_window *win)
{
    lj_show *sh = show();
    const lj_fixture *f = sh && cd_target >= 0 ? &sh->fixture[cd_target] : NULL;
    char t[112];
    lj_window_set_text(win, "LFixtureType", fx_text(cd_target, t, sizeof t));   /* fixtureinfo.gif: "Martin Ego X5" / "121" / "1" / "Master" */
    snprintf(t, sizeof t, "%d", f ? f->address : 0); lj_window_set_text(win, "LDmxAdr", t);
    snprintf(t, sizeof t, "%d", f ? f->link : 0); lj_window_set_text(win, "LLink", t);
    if (f && f->intensity_group && sh->submaster_name[f->intensity_group - 1][0]) lj_str_copy(t, sizeof t, sh->submaster_name[f->intensity_group - 1]);
    else if (f && f->intensity_group) snprintf(t, sizeof t, "Group %d", f->intensity_group);   /* PROPOSAL rig: a sub-master without a name */
    else lj_str_copy(t, sizeof t, "Master");
    lj_window_set_text(win, "lIntG", t);
    lj_w_dipswitch_set_value(cw(FI, "DipSwitch1"), f ? f->address : 0);
}
static void fn_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(FN); }
static const lj_binding fn_b[] = { { "FormClose", fn_close, NULL }, { NULL, NULL, NULL } };
static void fn_init(lj_window *win)
{
    static char plain[LJ_GS2_NOTES];
    const lj_profile *p = show() ? lj_show_profile(show(), cd_target) : NULL;
    (void)win;
    if (p && p->gs2_raw) lj_str_rtf_strip(plain, sizeof plain, (const char *)p->notes_rtf, sizeof p->notes_rtf);   /* the user's RTF stripped (design 5.3) */
    else lj_str_copy(plain, sizeof plain, p && p->notes ? p->notes : CD_NA);   /* built-ins: our own rewritten notes (D2) */
    memo_text(cw(FN, "Re1"), plain);
}
static void fs_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(FS); }
static void fs_close_on_ok(lj_widget *w, void *ud)
{
    lj_menu *m = cd_win[cd_idx(FS)] ? lj_window_menus(cd_win[cd_idx(FS)]) : NULL;
    lj_menu_rt *rt = m ? lj_menu_rt_find(m, lj_window_menu_item(cd_win[cd_idx(FS)])) : NULL;
    (void)w; (void)ud;
    if (!rt) return;
    rt->checked = !rt->checked;
    lj_ini_seti(lj_ctx_get()->forms_ini, "FIXTURESPECIAL", "CloseOnOK", rt->checked);   /* PROPOSAL key (forms.ini section = class name rule, design 7.10) */
}
static const lj_binding fs_b[] = { { "FormClose", fs_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "LV1Change", lj_handler_noop, NULL },
    { "LV1DblClick", lj_handler_noop, NULL }, { "bOKClick", lj_handler_noop, NULL }, { "M_CloseOnOKClick", fs_close_on_ok, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void fs_init(lj_window *win)
{
    const lj_profile *p = show() ? lj_show_profile(show(), cd_target) : NULL;
    int i;
    rows_reset();
    for (i = 0; p && i < 32; i++) if (p->special[i].enabled && p->special[i].name[0]) row_add(i, "%s", p->special[i].name);
    rows_to(cw(FS, "LV1"));
    grey(cw(FS, "bOK"));                                       /* running a special from here: no engine path (fidelity row) */
    lj_menu_set_checked(lj_window_menus(win), "M_CloseOnOK", lj_ini_geti(lj_ctx_get()->forms_ini, "FIXTURESPECIAL", "CloseOnOK", 1));
}

/* ---- Patch Sub Masters (TInternalIntensityPatchForm) ---- */
#define IP LJ_WIN_INTENSITY_PATCH
static void ip_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(IP); }
static const lj_binding ip_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bkOkClick", ip_ok, NULL }, { NULL, NULL, NULL } };
static void ip_init(lj_window *win)
{
    static const char *const grp[LJ_SUBMASTERS] = { "Group 1", "Group 2", "Group 3", "Group 4", "Group 5", "Group 6", "Group 7", "Group 8" };   /* the grid keeps the pointers */
    lj_widget *g = cw(IP, "SG1");
    int r;
    (void)win;
    lj_w_grid_set_cell(g, 0, 0, "Function"); lj_w_grid_set_cell(g, 1, 0, "Source (DMX Out)");   /* submasterpatchdialog.gif */
    for (r = 1; r <= LJ_SUBMASTERS; r++) lj_w_grid_set_cell(g, 0, r, grp[r - 1]);   /* the Source column stays empty: no sub-master source in the model or the engine (fidelity row) */
    btn(IP, "BitBtn2", close_ev);
}

/* ---- Define Generic Scanner1 / Color Scroller profile, RGBPack configuration (icon popup Define Fixture Profile) ---- */
#define S1 LJ_WIN_SCANNER1_DEFS
#define CS LJ_WIN_COLOR_SCROLLER_DEFS
#define RP LJ_WIN_RGBPACK_DEFS
static void grey_tree(lj_widget *w, const char *keep1, const char *keep2)   /* .SS1 / .SC1: no byte of either format is known (16 s7): every editing control inert */
{
    lj_widget *c;
    for (c = w ? w->first : NULL; c; c = c->next) {
        if ((c->kind == LJ_K_EDIT || c->kind == LJ_K_CHECKBOX || c->kind == LJ_K_BUTTON || c->kind == LJ_K_LISTVIEW || c->kind == LJ_K_BITBTN)
            && c->name && strcmp(c->name, keep1) != 0 && strcmp(c->name, keep2) != 0) grey(c);
        grey_tree(c, keep1, keep2);
    }
}
static void def_ok(lj_widget *w, void *ud) { lj_window *win = lj_window_of(w); int i; (void)ud; for (i = 0; i < CD_N; i++) if (cd_win[i] == win) lj_config_dlg_close((enum lj_win)(CD_FIRST + i)); }
static const lj_binding s1_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "PC1Change", lj_handler_noop, NULL },
    { "cbEnableClick", lj_handler_noop, NULL }, { "lvBOChange", lj_handler_noop, NULL }, { "bClearBoClick", lj_handler_noop, NULL }, { "bSetBOClick", lj_handler_noop, NULL },
    { "lvColor1Change", lj_handler_noop, NULL }, { "lvColor1Click", lj_handler_noop, NULL }, { "bClearColorClick", lj_handler_noop, NULL }, { "bSelectColorClick", lj_handler_noop, NULL },
    { "bGetColorBMPClick", lj_handler_noop, NULL }, { "bSetColorValueClick", lj_handler_noop, NULL }, { "eColorValKeyDown", lj_handler_noop, NULL },
    { "lvGobo1Change", lj_handler_noop, NULL }, { "lvGobo1Click", lj_handler_noop, NULL }, { "bClearGoboClick", lj_handler_noop, NULL }, { "bGetGoboBMPClick", lj_handler_noop, NULL },
    { "bSetGoboValueClick", lj_handler_noop, NULL }, { "eGoboValKeyDown", lj_handler_noop, NULL }, { "lvLevelsClick", lj_handler_noop, NULL }, { "eSelectionIdChange", lj_handler_noop, NULL },
    { "bVisibleClick", lj_handler_noop, NULL }, { "bEnableClick", lj_handler_noop, NULL }, { "bInvisibleClick", lj_handler_noop, NULL }, { "bDisableClick", lj_handler_noop, NULL },
    { "eLevelNameChange", lj_handler_noop, NULL }, { "bOkClick", def_ok, NULL }, { "ImportProfile1Click", lj_handler_noop, NULL }, { "ExportProfile1Click", lj_handler_noop, NULL },
    { "NewProfile1Click", lj_handler_noop, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void def_menu_grey(lj_window *win, const char *const *items, int n) { int i; for (i = 0; i < n; i++) lj_menu_set_enabled(lj_window_menus(win), items[i], 0); }
static void s1_init(lj_window *win)
{
    static const char *const m[] = { "ImportProfile1", "ExportProfile1", "NewProfile1" };
    static const char *const bo[8] = { "0\t-1", "1\t-1", "2\t-1", "3\t-1", "4\t-1", "5\t-1", "6\t-1", "7\t-1" };   /* scanner1intensitydialog.gif: an unset value reads -1 */
    static const char *const lv[8] = { "1\tYes\tCH1\tYes", "2\tYes\tCH2\tYes", "3\tYes\tCH3\tYes", "4\tYes\tCH4\tYes", "5\tYes\tCH5\tYes", "6\tYes\tCH6\tYes", "7\tYes\tCH7\tYes", "8\tYes\tCH8\tYes" };   /* the placeholder's 8 levels (PROPOSAL rig captions) */
    static char pal[20][8]; static const char *palp[20];
    int k;
    for (k = 0; k < 20; k++) { snprintf(pal[k], sizeof pal[k], "%d\t-1", k); palp[k] = pal[k]; }
    lj_w_listview_set_items(cw(S1, "lvBO"), bo, 8); lj_w_listview_set_items(cw(S1, "lvLevels"), lv, 8);
    lj_w_listview_set_items(cw(S1, "lvColor1"), palp, 20); lj_w_listview_set_items(cw(S1, "lvGobo1"), palp, 20);
    lj_w_tabs_select(cw(S1, "PC1"), 0);                        /* Intensity in front (scanner1defdialog.gif; the DFM's ActivePage is Levels & Id) */
    lj_window_resize(win, 418, 236);                           /* PROPOSAL: PC1 + Panel1 (418 x 201 + 35); scanner1defdialog.gif (reduced) shows no area right of the pages */
    grey_tree(lj_window_root(win), "bOk", "bCancel");
    def_menu_grey(win, m, 3);
    btn(S1, "bCancel", close_ev);
}
static const lj_binding cs_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "lvPaletteChange", lj_handler_noop, NULL }, { "eValExit", lj_handler_noop, NULL },
    { "eValKeyDown", lj_handler_noop, NULL }, { "bSetValueClick", lj_handler_noop, NULL }, { "bClearClick", lj_handler_noop, NULL }, { "bGetColorClick", lj_handler_noop, NULL },
    { "bOkClick", def_ok, NULL }, { "ImportSetup1Click", lj_handler_noop, NULL }, { "ExportSetupFile1Click", lj_handler_noop, NULL }, { "ClearProfile1Click", lj_handler_noop, NULL },
    { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void cs_init(lj_window *win)
{
    static const char *const m[] = { "ImportSetup1", "ExportSetupFile1", "ClearProfile1" };
    static char pal[20][8]; static const char *palp[20];
    int k;
    for (k = 0; k < 20; k++) { snprintf(pal[k], sizeof pal[k], "%d\t\t", k); palp[k] = pal[k]; }   /* 20 palette entries 0-19 [16 s7] */
    lj_w_listview_set_items(cw(CS, "lvPalette"), palp, 20);
    grey_tree(lj_window_root(win), "bOk", "bCancel");
    def_menu_grey(win, m, 3);
    btn(CS, "bCancel", close_ev);
}
/* RGBPack configuration: the unit captions ("Unit 1".."Unit 10", rgbcolorcontrol.gif) kept per fixture in ch_caption[3 * unit]
 * (Config.dat chcap, 6 bytes, ME-118; PROPOSAL rig: LJ2's store and length are unknown) */
static char rp_cap[10][7];
static int rp_units(void) { return footprint(cd_target) / 3 > 10 ? 10 : footprint(cd_target) / 3; }
static void rp_paint(void)
{
    static const char *const num[10] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };   /* the grid keeps the pointers */
    lj_widget *g = cw(RP, "sg1");
    int u;
    lj_w_grid_set_cell(g, 0, 0, "Unit"); lj_w_grid_set_cell(g, 1, 0, "Caption");   /* PROPOSAL rig: no capture of this dialog */
    for (u = 0; u < 10; u++) { lj_w_grid_set_cell(g, 0, u + 1, u < rp_units() ? num[u] : ""); lj_w_grid_set_cell(g, 1, u + 1, u < rp_units() ? rp_cap[u] : ""); }
}
static int rp_row(void) { lj_widget *g = cw(RP, "sg1"); return g ? ((lj_w_grid *)g)->row - 1 : -1; }
static void rp_cell(lj_widget *w, void *ud)   /* Edit1 is the in-place editor: moved over the Caption cell of the picked row (PROPOSAL rig) */
{
    lj_widget *e = cw(RP, "Edit1");
    lj_rect r;
    int row = rp_row();
    (void)ud;
    if (!e || row < 0 || row >= rp_units() || lj_w_grid_cell_rect(w, 1, row + 1, &r) != 0) return;
    e->x = w->x + r.x; e->y = w->y + r.y; e->w = r.w; e->h = r.h; e->visible = 1;
    lj_w_edit_set_text(e, rp_cap[row]);
    lj_window_invalidate_all(cd_win[cd_idx(RP)]);
}
static void rp_edit(lj_widget *w, void *ud) { int r = rp_row(); (void)ud; if (w && w->visible && r >= 0 && r < rp_units()) { lj_str_copy(rp_cap[r], sizeof rp_cap[r], lj_w_edit_text(w)); rp_paint(); } }
static void rp_ok(lj_widget *w, int ev, void *ud)
{
    lj_show *sh = show();
    int u;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    rp_edit(cw(RP, "Edit1"), NULL);
    if (sh && cd_target >= 0) { lj_lock(); for (u = 0; u < rp_units(); u++) lj_str_copy(sh->fixture[cd_target].ch_caption[3 * u], sizeof sh->fixture[cd_target].ch_caption[0], rp_cap[u]); lj_unlock(); lj_desktop_run_action(LJ_ACT_FIXTURE_APPLY, cd_target); }
    lj_config_dlg_close(RP);
}
static const lj_binding rp_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "sg1SelectCell", rp_cell, NULL },
    { "Edit1Exit", rp_edit, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void rp_init(lj_window *win)
{
    lj_show *sh = show();
    char t[112], p[96];
    int u;
    snprintf(t, sizeof t, "(%d) %s", cd_target + 1, fx_text(cd_target, p, sizeof p)); lj_window_set_text(win, "lFixType", t);   /* exe "(%d) %s" as Channel Configuration */
    lj_window_set_text(win, "lUsername", sh ? sh->fixture[cd_target].user_name : "");
    snprintf(t, sizeof t, "%d/%d", sh ? sh->fixture[cd_target].link : 0, sh ? sh->fixture[cd_target].address : 0); lj_window_set_text(win, "lDMXadr", t);
    for (u = 0; u < 10; u++) lj_str_copy(rp_cap[u], sizeof rp_cap[u], sh && u < rp_units() ? sh->fixture[cd_target].ch_caption[3 * u] : "");
    if (cw(RP, "Edit1")) { ((lj_w_edit *)cw(RP, "Edit1"))->max_length = 6; cw(RP, "Edit1")->visible = 0; }
    rp_paint();
    btn(RP, "BitBtn1", rp_ok); btn(RP, "BitBtn2", close_ev);
}

/* ---- Palette Value Types (TPaletteValueTypeForm) [16 s5] ---- */
#define VT LJ_WIN_PALETTE_VALUE_TYPE
static const char *const vt_names[19] = { "Default Function", "Alt1 Value", "Alt2 Value", "Alt3 Value", "Alt4 Value", "Indexed", "Rotation", "Rotation CW", "Rotation CCW",
    "B/O (Blink)", "Shake", "Use Speed Parameter", "Fixed", "Split", "Random Slow", "Random Med", "Random Fast", "Color", "Stop" };   /* exe long names (16 s5); Alt3 / Alt4 PROPOSAL (the exe reuses one string) */
static void vt_paint(void)
{
    int k;
    rows_reset();
    for (k = 0; k < 5; k++) row_add(k, "%d\t%s", k + 1, cd.vt_work[k] < 19 ? vt_names[cd.vt_work[k]] : "?");
    rows_to(cw(VT, "lvSelection"));
    cd.nrows2 = 0;
    for (k = 0; k < 19; k++) { lj_str_copy(cd.rows2[k], sizeof cd.rows2[0], vt_names[k]); cd.rowp2[k] = cd.rows2[k]; cd.nrows2++; }
    lj_w_listview_set_items(cw(VT, "lvTypes"), cd.rowp2, cd.nrows2);
}
static void vt_gate(lj_widget *w, void *ud) { (void)w; (void)ud; set_en(cw(VT, "bSet"), lj_w_listview_index(cw(VT, "lvSelection")) >= 0 && lj_w_listview_index(cw(VT, "lvTypes")) >= 0); }
static void vt_set(lj_widget *w, void *ud)
{
    int s = lj_w_listview_index(cw(VT, "lvSelection")), t = lj_w_listview_index(cw(VT, "lvTypes"));
    (void)w; (void)ud;
    if (s < 0 || s >= 5 || t < 0 || t >= 19) return;
    cd.vt_work[s] = (unsigned char)t;
    vt_paint();
    ((lj_w_listview *)cw(VT, "lvSelection"))->index = s;
}
static void vt_ok(lj_widget *w, void *ud) { void (*done)(void *) = cd.vt_done; (void)w; (void)ud; if (cd.vt_codes) memcpy(cd.vt_codes, cd.vt_work, 5); lj_config_dlg_close(VT); if (done) done(cd.vt_ud); }
static const lj_binding vt_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "bOkClick", vt_ok, NULL }, { "lvTypesChange", vt_gate, NULL },
    { "lvClick", vt_gate, NULL }, { "lvTypesDblClick", vt_set, NULL }, { "lvTypesStartDrag", lj_handler_noop, NULL }, { "lvSelectionDragDrop", vt_set, NULL },
    { "lvSelectionDragOver", lj_handler_noop, NULL }, { "bSetClick", vt_set, NULL }, { NULL, NULL, NULL } };
static void vt_init(lj_window *win) { lj_window_set_caption(win, cd.vt_caption); vt_paint(); btn(VT, "bCancel", close_ev); }
void lj_config_dlg_value_types(const char *palette, unsigned char codes[5], void (*done)(void *ud), void *ud)
{
    lj_config_dlg_close(VT);
    cd.vt_codes = codes; cd.vt_done = done; cd.vt_ud = ud;
    memcpy(cd.vt_work, codes, 5);
    snprintf(cd.vt_caption, sizeof cd.vt_caption, "Select %s Palette Types", palette ? palette : "");   /* selectingpalettetypes.gif "Select Gobo 1 Palette Types" */
    lj_win_open(VT);
}

/* ---- Select Bitmap (TBitMapPickerForm2) and Get Bitmap (TGetBitmapForm): our art by name (D2) ---- */
#define BP LJ_WIN_BITMAP_PICKER
#define GB LJ_WIN_GET_BITMAP
static int art_is_tile(const char *n, int colors) { return colors < 0 ? (!strncmp(n, "gobo_", 5) || !strncmp(n, "gel_", 4) || !strncmp(n, "color_", 6)) : colors ? (!strncmp(n, "gel_", 4) || !strncmp(n, "color_", 6)) : !strncmp(n, "gobo_", 5); }   /* PROPOSAL names: gobo_* tiles, gel_* / color_* swatches (art track) */
static void bp_fill(int colors)
{
    char t[32];
    int i;
    rows_reset();
    for (i = 0; i < lj_art_count; i++) if (art_is_tile(lj_art_table[i].name, colors)) row_add(i, "%s", lj_art_table[i].name);
    rows_to(cw(BP, "LV1"));
    snprintf(t, sizeof t, "%d Bitmaps", cd.nrows);             /* selectbitmapdialog2.gif "174 Bitmaps" */
    if (cd_win[cd_idx(BP)]) lj_window_set_text(cd_win[cd_idx(BP)], "lBitmaps", t);
}
static void bp_pick(lj_widget *w, void *ud)
{
    int r = row_sel(cw(BP, "LV1"));
    void (*done)(const char *, void *) = cd.pick_done;
    void *dud = cd.pick_ud;
    (void)w; (void)ud;
    if (r < 0) return;
    lj_config_dlg_close(BP);
    if (done) done(lj_art_table[cd.row_id[r]].name, dud);
}
static void bp_filter(lj_widget *w, void *ud) { (void)ud; bp_fill(!strcmp(w->name, "bColors") ? 1 : !strcmp(w->name, "bGobos") ? 0 : -1); }
static void bp_menu(lj_widget *w, void *ud) { (void)w; (void)ud; bp_fill(cd.pick_colors); }   /* Refresh [02 s9.4] */
static const lj_binding bp_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "sbAllClick", bp_filter, NULL },
    { "bGobosClick", bp_filter, NULL }, { "bColorsClick", bp_filter, NULL }, { "LV2Click", lj_handler_noop, NULL }, { "LV1DblClick", bp_pick, NULL }, { "LV1Deletion", lj_handler_noop, NULL },
    { "LV1MouseMove", lj_handler_noop, NULL }, { "MenuClick", bp_menu, NULL }, { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void bp_init(lj_window *win)
{
    static const char *const libs[1] = { "SYSTEM BITMAPS" };    /* selectbitmapdialog2.gif: the library list; ours is one */
    lj_window_set_caption(win, cd.pick_colors ? "Get Color Bitmap" : "Get Gobo/Effects Bitmap");   /* "Get Gobo/Effects Bitmap" (07 s12); the colour caption PROPOSAL */
    static const lj_column_desc col = { .caption = "", .width = 270, .image_index = -1 };   /* LV1 is a vsIcon view; ours lists the tile names in one column (fidelity row) */
    lj_w_listview *lv = (lj_w_listview *)cw(BP, "LV1");
    if (lv && !lv->ncols) { lv->cols = &col; lv->ncols = 1; lv->col_w[0] = 270; lv->show_headers = 0; }
    lj_w_listview_set_items(cw(BP, "LV2"), libs, 1);
    bp_fill(cd.pick_colors);
}
void lj_config_dlg_pick_bitmap(int colors, void (*done)(const char *name, void *ud), void *ud) { lj_config_dlg_close(BP); cd.pick_colors = colors; cd.pick_done = done; cd.pick_ud = ud; lj_win_open(BP); }

static void gb_click(lj_widget *w, void *ud) { (void)ud; set_en(cw(GB, "bOk"), row_sel(w) >= 0); }
static void gb_ok(lj_widget *w, void *ud)
{
    int r = row_sel(cw(GB, "LB1"));
    void (*done)(const char *, void *) = cd.icon_done;
    void *dud = cd.icon_ud;
    (void)w; (void)ud;
    if (r < 0) return;
    lj_config_dlg_close(GB);
    if (done) done(lj_art_table[cd.row_id[r]].name, dud);
}
static void gb_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(GB); }
static const lj_binding gb_b[] = { { "FormResize", lj_handler_noop, NULL }, { "LB1Click", gb_click, NULL }, { "LB1DrawItem", lj_handler_noop, NULL }, { "LB1KeyDown", lj_handler_noop, NULL },
    { "bOkClick", gb_ok, NULL }, { "bCancelClick", gb_cancel, NULL }, { NULL, NULL, NULL } };
static void gb_init(lj_window *win)
{
    int i;
    (void)win;
    rows_reset();
    for (i = 0; i < lj_art_count; i++) if (lj_art_table[i].img.h >= 32) row_add(i, "%s", lj_art_table[i].name);   /* the fixture icons (50-px silhouettes), not the 16-px glyphs */
    rows_to(cw(GB, "LB1"));
}
void lj_config_dlg_get_icon(void (*done)(const char *name, void *ud), void *ud) { lj_config_dlg_close(GB); cd.icon_done = done; cd.icon_ud = ud; lj_win_open(GB); }

/* ---- Apply user profile (TApplyMultiForm) ---- */
#define AM LJ_WIN_APPLY_MULTI
static void am_fill(void)
{
    lj_show *sh = show();
    const lj_profile *mp = sh ? lj_show_profile(sh, cd.am_master) : NULL;
    int f;
    rows_reset();
    for (f = 0; sh && f < LJ_FIXTURES; f++) {
        const lj_profile *p;
        if (!sh->fixture[f].link || !lj_sel_is_selected(f) || !(sh->fixture[f].kind == LJ_FX_GS2 || sh->fixture[f].profile_id == 46)) continue;
        p = lj_show_profile(sh, f);
        if (cd.am_id_only && (!p || !mp || strcmp(p->fixture_id, mp->fixture_id) != 0)) continue;   /* "Identical Id Only unchecks fixtures whose Fixture ID differs" (CHM scribble7334) */
        row_add(f, "%d\t%s\t%s", f + 1, p ? p->fixture_id : "", sh->fixture[f].user_name);
    }
    rows_to(cw(AM, "LV1"));
}
static void am_filter(lj_widget *w, void *ud) { (void)ud; cd.am_id_only = strcmp(w->name, "bIdOnly") == 0; am_fill(); }
static void am_ok(lj_widget *w, int ev, void *ud)
{
    int i, list[CD_ROWS], n = cd.nrows;
    void (*apply)(int, void *) = cd.am_apply;
    void *aud = cd.am_ud;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    for (i = 0; i < n; i++) list[i] = cd.row_id[i];
    lj_config_dlg_close(AM);
    for (i = 0; apply && i < n; i++) apply(list[i], aud);
}
static const lj_binding am_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "LV1Click", lj_handler_noop, NULL }, { "bIdOnlyClick", am_filter, NULL }, { "Button2Click", am_filter, NULL },
    { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void am_init(lj_window *win)
{
    const lj_profile *p = show() ? lj_show_profile(show(), cd.am_master) : NULL;
    char t[48];
    snprintf(t, sizeof t, "Master: %d", cd.am_master + 1); lj_window_set_text(win, "lMasterNo", t);   /* PROPOSAL rig: the label formats */
    snprintf(t, sizeof t, "Master Id: %s", p ? p->fixture_id : ""); lj_window_set_text(win, "lMasterId", t);
    cd.am_id_only = 0;
    am_fill();
    btn(AM, "bOk", am_ok); btn(AM, "bCancel", close_ev);
}
void lj_config_dlg_apply_multi(int master, void (*apply)(int fixture, void *ud), void *ud) { lj_config_dlg_close(AM); cd.am_master = master; cd.am_apply = apply; cd.am_ud = ud; lj_win_open(AM); }

/* ---- Oldstyle profile name (TProfileNameOldStyleForm, ME-126) ---- */
#define ON LJ_WIN_PROFILE_NAME_OLDSTYLE
static const char *const on_tr[][2] = {                         /* the DFM memo's table, verbatim pairs */
    { "HES", "High End Systems" }, { "FAL", "F.A..L." }, { "PR", "Pearl River" }, { "VEI", "Visual Effects, Inc." }, { "Visual Effects", "Visual Effects, Inc." },
    { "ADJ", "American DJ" }, { "ColorKinetics", "Color Kinetics" }, { "Eureka", "R&D International" }, { "Coemar/TAS", "Coemar" }, { "JB", "JB-Lighting" },
    { "IMG", "IMG Stage Line" }, { "CP", "ClayPaky" }, { "5Star", "5 Star Systems" } };
static size_t prefix_word(const char *s, const char *p) { size_t n = strlen(p); return strncmp(s, p, n) == 0 && (s[n] == ' ' || !s[n]) ? n : 0; }
static void on_suggest(lj_widget *w, void *ud)
{
    char t[96];
    const char *s = lj_w_edit_text(cw(ON, "eName"));
    size_t i, n;
    (void)w; (void)ud;
    if (!s) return;
    for (i = 0; i < LJ_COUNT_OF(on_tr); i++) if ((n = prefix_word(s, on_tr[i][0])) != 0) { snprintf(t, sizeof t, "%s%s", on_tr[i][1], s + n); lj_w_edit_set_text(cw(ON, "eName"), t); return; }
}
static void on_ok(lj_widget *w, void *ud)   /* the manufacturer = the longest known one the name starts with (the table, then the exe rows); PROPOSAL rig */
{
    char name[96], manu[64] = "";
    const char *s = lj_w_edit_text(cw(ON, "eName"));
    void (*done)(const char *, const char *, void *) = cd.os_done;
    void *dud = cd.os_ud;
    size_t i, n, best = 0;
    int k;
    (void)w; (void)ud;
    lj_str_copy(name, sizeof name, s ? s : "");
    for (i = 0; i < LJ_COUNT_OF(on_tr); i++) if ((n = prefix_word(name, on_tr[i][1])) > best) { best = n; lj_str_copy(manu, sizeof manu, on_tr[i][1]); }
    for (k = 0; k < lj_fixlib_count(); k++) { const lj_exe_row *r = lj_fixlib_row(k); if (r && r->manufacturer && r->manufacturer[0] && (n = prefix_word(name, r->manufacturer)) > best) { best = n; lj_str_copy(manu, sizeof manu, r->manufacturer); } }
    lj_config_dlg_close(ON);
    if (done) done(best ? manu : LJ_GS2_LEGACY_MANUFACTURER, best ? name + best + (name[best] == ' ') : name, dud);
}
static const lj_binding on_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "bOkClick", on_ok, NULL }, { "Button1Click", on_suggest, NULL }, { NULL, NULL, NULL } };
static void on_init(lj_window *win) { (void)win; lj_w_edit_set_text(cw(ON, "eName"), cd.os_legacy); btn(ON, "BitBtn2", close_ev); }
void lj_config_dlg_oldstyle_name(const char *legacy, void (*done)(const char *manufacturer, const char *name, void *ud), void *ud)
{ lj_config_dlg_close(ON); lj_str_copy(cd.os_legacy, sizeof cd.os_legacy, legacy ? legacy : ""); cd.os_done = done; cd.os_ud = ud; lj_win_open(ON); }

/* ---- File Exists (TProfileImportModifiedFilesForm), File Already Exists (TFileExistsForm), Warning (TFixtureExportWarningDialog) ---- */
#define IM LJ_WIN_PROFILE_IMPORT_MODIFIED
#define FE LJ_WIN_FILE_EXISTS
#define EW LJ_WIN_EXPORT_WARNING
static void im_answer(int v) { void (*done)(int, void *) = cd.im_done; void *dud = cd.im_ud; cd.im_done = NULL; lj_config_dlg_close(IM); if (done) done(v, dud); }
static void im_import(lj_widget *w, void *ud) { (void)w; (void)ud; im_answer(1); }
static void im_keep(lj_widget *w, void *ud) { (void)w; (void)ud; im_answer(0); }
static const lj_binding im_b[] = { { "bUseImportClick", im_import, NULL }, { "bUseExistingClick", im_keep, NULL }, { "BitBtn4Click", lj_help_viewer_click, "fixtures" } /* bkHelp, ME-141 */, { NULL, NULL, NULL } };
static void im_init(lj_window *win)
{
    char t[160];
    (void)win;
    snprintf(t, sizeof t, "%s\nAlready exists with different contents", cd.im_file);   /* exe 0x198FC4 */
    memo_text(cw(IM, "re"), t);
}
void lj_config_dlg_import_modified(const char *file, void (*done)(int use_import, void *ud), void *ud)
{ lj_config_dlg_close(IM); lj_str_copy(cd.im_file, sizeof cd.im_file, file ? file : ""); cd.im_done = done; cd.im_ud = ud; lj_win_open(IM); }

static void fe_answer(int v) { void (*done)(int, void *) = cd.fe_done; void *dud = cd.fe_ud; cd.fe_done = NULL; lj_config_dlg_close(FE); if (done) done(v, dud); }
static void fe_rename(lj_widget *w, void *ud) { (void)w; (void)ud; fe_answer(0); }
static void fe_over(lj_widget *w, void *ud) { (void)w; (void)ud; fe_answer(1); }
static void fe_skip(lj_widget *w, void *ud) { (void)w; (void)ud; fe_answer(2); }
static const lj_binding fe_b[] = { { "bRenameClick", fe_rename, NULL }, { "bOverwriteClick", fe_over, NULL }, { "bSkipClick", fe_skip, NULL }, { "BitBtn1Click", lj_help_viewer_click, "files" } /* bkHelp, ME-141 */, { NULL, NULL, NULL } };
static void fe_init(lj_window *win)
{
    char t[LJ_PATH_CAP + 32];
    const char *f = cd.fe_path, *p;
    (void)win;
    for (p = cd.fe_path; *p; p++) if (*p == '/' || *p == '\\') f = p + 1;
    snprintf(t, sizeof t, "File already exists\n%s", f);   /* exe "File already exists" (0x14D4F0); the file name only, PROPOSAL rig (the same text on every OS) */
    memo_text(cw(FE, "Memo1"), t);
}
void lj_config_dlg_file_exists(const char *path, void (*done)(int choice, void *ud), void *ud)
{ lj_config_dlg_close(FE); lj_str_copy(cd.fe_path, sizeof cd.fe_path, path ? path : ""); cd.fe_done = done; cd.fe_ud = ud; lj_win_open(FE); }

static void ew_answer(lj_widget *w, int ev, void *ud)
{
    void (*done)(int, void *) = cd.ew_done;
    void *dud = cd.ew_ud;
    int yes = w && w->name && strcmp(w->name, "bYes") == 0;
    (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (lj_w_checkbox_checked(cw(EW, "cbDontShow"))) lj_ini_seti(lj_ctx_get()->forms_ini, "FIXTURECONF", "ExportWarning", 0);   /* PROPOSAL key */
    cd.ew_done = NULL;
    lj_config_dlg_close(EW);
    if (done) done(yes, dud);
}
static const lj_binding ew_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static void ew_init(lj_window *win)
{
    (void)win;
    memo_text(cw(EW, "re1"), "The export file holds the profile only. The icon and palette bitmaps the profile names are not packed: LJ4EVR ships no bitmaps, "
                             "so the receiving installation shows its own art or the names.\n\nCreate the export file ?");   /* PROPOSAL rig: ours; LJ2's RTF is Martin's text (D2) */
    btn(EW, "bYes", ew_answer); btn(EW, "bNo", ew_answer);   /* BitBtn1 is bkHelp */
}
void lj_config_dlg_export_warning(void (*done)(int yes, void *ud), void *ud)
{
    if (!fx_ini("ExportWarning", 1)) { if (done) done(1, ud); return; }
    lj_config_dlg_close(EW); cd.ew_done = done; cd.ew_ud = ud; lj_win_open(EW);
}

/* ---- RGB Palette (TRGBPaletteForm) [04 s5.3]: <library>\RGBPalette.Ini, [Type] Name=RRGGBB (PROPOSAL file) ---- */
#define RG LJ_WIN_RGB_PALETTE
#define RG_MAX 192
static struct { char type[RG_MAX][32], name[RG_MAX][32]; uint32_t rgb[RG_MAX]; int n; char text[RG_MAX + 32][72]; const char *items[RG_MAX + 32]; int item_e[RG_MAX + 32], nitems; } rg;
static const struct { const char *name; uint32_t rgb; } rg_seed[] = {   /* rgbpalette.gif GENERIC folder, in its order */
    { "Black", 0x000000 }, { "Blue", 0x0000FF }, { "Cyan", 0x00FFFF }, { "Dark Blue", 0x000080 }, { "Dark Green", 0x008000 }, { "Dark Yellow", 0x808000 },
    { "Green", 0x00FF00 }, { "Light Blue", 0x0080FF }, { "Light Green", 0x80FF80 }, { "Light Yellow", 0xFFFF80 }, { "Magenta", 0xFF00FF }, { "Orange", 0xFF8000 },
    { "Red", 0xFF0000 }, { "White", 0xFFFFFF }, { "Yellow", 0xFFFF00 } };
static int rg_path(char *out, size_t cap) { return lj_ctx_get()->lib ? lj_library_path(lj_ctx_get()->lib, "RGBPalette.Ini", out, cap) : -1; }
static void rg_build(void)
{
    int i, j;
    rg.nitems = 0;
    for (i = 0; i < rg.n; i++) {
        for (j = 0; j < i && strcmp(rg.type[j], rg.type[i]) != 0; j++) { }
        if (j < i) continue;                                     /* one folder per type, entries in file order */
        snprintf(rg.text[rg.nitems], sizeof rg.text[0], "%s", rg.type[i]); rg.item_e[rg.nitems] = -1; rg.items[rg.nitems] = rg.text[rg.nitems]; rg.nitems++;
        for (j = i; j < rg.n; j++) if (strcmp(rg.type[j], rg.type[i]) == 0 && rg.nitems < (int)LJ_COUNT_OF(rg.items)) {
            snprintf(rg.text[rg.nitems], sizeof rg.text[0], "\t%s", rg.name[j]); rg.item_e[rg.nitems] = j; rg.items[rg.nitems] = rg.text[rg.nitems]; rg.nitems++;
        }
    }
    lj_w_treeview_set_items(cw(RG, "tv1"), rg.items, rg.nitems);
    for (i = 0; i < rg.nitems; i++) if (rg.item_e[i] < 0) lj_w_treeview_expand(cw(RG, "tv1"), i, 1);
}
static void rg_load(void)
{
    char path[LJ_PATH_CAP], key[24];
    lj_ini *ini = rg_path(path, sizeof path) == 0 ? lj_ini_load(path) : NULL;
    int i, k;
    rg.n = 0;
    for (i = 0; ini && i < RG_MAX; i++) {                       /* Entry<i>=Type|Name|RRGGBB: the file is ours, every field checked */
        const char *v, *a, *b;
        snprintf(key, sizeof key, "Entry%d", i);
        if (!(v = lj_ini_gets(ini, "RGBPALETTE", key, NULL))) break;
        if (!(a = strchr(v, '|')) || !(b = strchr(a + 1, '|')) || (size_t)(a - v) >= sizeof rg.type[0] || (size_t)(b - a - 1) >= sizeof rg.name[0] || strlen(b + 1) != 6) continue;
        snprintf(rg.type[rg.n], sizeof rg.type[0], "%.*s", (int)(a - v), v); snprintf(rg.name[rg.n], sizeof rg.name[0], "%.*s", (int)(b - a - 1), a + 1);
        rg.rgb[rg.n++] = (uint32_t)strtoul(b + 1, NULL, 16) & 0xFFFFFFu;
    }
    lj_ini_free(ini);
    for (k = 0; !rg.n && k < (int)LJ_COUNT_OF(rg_seed); k++) { lj_str_copy(rg.type[k], sizeof rg.type[0], "GENERIC"); lj_str_copy(rg.name[k], sizeof rg.name[0], rg_seed[k].name); rg.rgb[k] = rg_seed[k].rgb; if (k == (int)LJ_COUNT_OF(rg_seed) - 1) rg.n = k + 1; }
}
static void rg_save(void)
{
    char path[LJ_PATH_CAP], key[24], v[128];
    lj_ini *ini = lj_ini_new();
    int i;
    for (i = 0; i < rg.n; i++) { snprintf(key, sizeof key, "Entry%d", i); snprintf(v, sizeof v, "%s|%s|%06X", rg.type[i], rg.name[i], (unsigned)rg.rgb[i]); lj_ini_sets(ini, "RGBPALETTE", key, v); }
    if (rg_path(path, sizeof path) != 0 || lj_ini_save(ini, path) != 0) lj_log("rgb palette: not saved");
    lj_ini_free(ini);
}
static void rg_pop(lj_widget *w, void *ud)
{
    lj_window *win = cd_win[cd_idx(RG)];
    const lj_menu_item_desc *it = win ? lj_window_menu_item(win) : NULL;
    lj_widget *tv = cw(RG, "tv1");
    const char *n = it && it->name ? it->name : "";
    int i = tv ? ((lj_w_treeview *)tv)->index : -1, k;
    (void)w; (void)ud;
    if (!tv) return;
    if (!strcmp(n, "Pop_Expand") || !strcmp(n, "pop_Collapse")) { if (i >= 0) lj_w_treeview_expand(tv, i, n[0] == 'P'); }
    else if (!strcmp(n, "pop_CollapseAll") || !strcmp(n, "pop_ExpandAll")) { for (k = 0; k < rg.nitems; k++) if (rg.item_e[k] < 0) lj_w_treeview_expand(tv, k, n[4] == 'E'); }
    else if (!strcmp(n, "Pop_DeleteEntry") && i >= 0 && i < rg.nitems && rg.item_e[i] >= 0) {
        int e = rg.item_e[i];
        memmove(rg.type[e], rg.type[e + 1], sizeof rg.type[0] * (size_t)(rg.n - e - 1)); memmove(rg.name[e], rg.name[e + 1], sizeof rg.name[0] * (size_t)(rg.n - e - 1));
        memmove(&rg.rgb[e], &rg.rgb[e + 1], sizeof rg.rgb[0] * (size_t)(rg.n - e - 1)); rg.n--;
        rg_save(); rg_build();
    }
    lj_widget_invalidate(tv);
}
static void rg_add(lj_widget *w, void *ud)   /* bOk adds, BitBtn2 drops the add panel */
{
    lj_widget *p = cw(RG, "pAdd");
    const char *t = ((lj_w_combobox *)cw(RG, "cbType"))->text, *nm = lj_w_edit_text(cw(RG, "eName"));
    (void)ud;
    if (!strcmp(w->name, "bOk") && nm && nm[0] && rg.n < RG_MAX) {
        lj_str_copy(rg.type[rg.n], sizeof rg.type[0], t && t[0] ? t : "GENERIC"); lj_str_copy(rg.name[rg.n], sizeof rg.name[0], nm);
        rg.rgb[rg.n++] = cw(RG, "sColor") ? cw(RG, "sColor")->color : 0;
        rg_save(); rg_build();
    }
    if (p) { p->visible = 0; lj_widget_layout(lj_window_root(cd_win[cd_idx(RG)])); lj_window_invalidate_all(cd_win[cd_idx(RG)]); }
}
static void rg_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_close(RG); }
static const lj_binding rg_b[] = { { "FormClose", rg_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "tv1Change", lj_handler_noop, NULL },
    { "tv1CustomDrawItem", lj_handler_noop, NULL }, { "tv1DblClick", lj_handler_noop, NULL }, { "AddBtnClick", rg_add, NULL }, { "PopClick", rg_pop, NULL }, { "MenuClick", lj_handler_noop, NULL },
    { "Help1Click", lj_help_viewer_click, "fixtures" }, { NULL, NULL, NULL } };
static void rg_init(lj_window *win)
{
    lj_widget *p = cw(RG, "pAdd");
    rg_load(); rg_build();
    if (p) { p->visible = 0; lj_widget_layout(lj_window_root(win)); }   /* rgbpalette.gif: the add panel shows only when a colour is added (rgbpaletteentry.gif) */
}

/* ---- Hidden fixture (THiddenFixtureForm): a 104x29 form with no controls ---- */
static const lj_binding hf_b[] = { { NULL, NULL, NULL } };

/* =====================================================================================================================
 * the family table
 * ===================================================================================================================== */
typedef struct { const lj_form_desc *form; const lj_binding *b; void (*init)(lj_window *win); } cd_row;
static const cd_row cd_rows[CD_N] = {
    [LJ_WIN_ADD_FIXTURES - CD_FIRST] = { &lj_form_TADDFIXFORM2, af_b, af_init },
    [LJ_WIN_APPLY_DEFAULT_DMX - CD_FIRST] = { &lj_form_TAPPLYDEFAULTDMXFORM, ad_b, NULL },
    [LJ_WIN_APPLY_MULTI - CD_FIRST] = { &lj_form_TAPPLYMULTIFORM, am_b, am_init },
    [LJ_WIN_AUTO_ADDRESS - CD_FIRST] = { &lj_form_TAUTOADDRESSFORM2, aa_b, aa_init },
    [LJ_WIN_CLEANUP - CD_FIRST] = { &lj_form_TCLEANUPFORM, cu_b, cu_init },
    [LJ_WIN_CLONE - CD_FIRST] = { &lj_form_TCLONEFORM, cl_b, cl_init },
    [LJ_WIN_COPY_FROM - CD_FIRST] = { &lj_form_TCOPYFROMFORM, cfm_b, cfm_init },
    [LJ_WIN_CONFIG_WARN - CD_FIRST] = { &lj_form_TCONFIGWARNFORM, cw_b, cw_init },
    [LJ_WIN_EXPORT_WARNING - CD_FIRST] = { &lj_form_TFIXTUREEXPORTWARNINGDIALOG, ew_b, ew_init },
    [LJ_WIN_FILE_EXISTS - CD_FIRST] = { &lj_form_TFILEEXISTSFORM, fe_b, fe_init },
    [LJ_WIN_FIND_PROFILE - CD_FIRST] = { &lj_form_TFINDPROFILEFORM, fp_b, fp_init },
    [LJ_WIN_FIXTURE_INFO - CD_FIRST] = { &lj_form_TFIXTUREINFOFORM, fi_b, fi_init },
    [LJ_WIN_FIXTURE_NOTES - CD_FIRST] = { &lj_form_TFIXTURENOTESFORM, fn_b, fn_init },
    [LJ_WIN_FIXTURE_SPECIAL - CD_FIRST] = { &lj_form_TFIXTURESPECIALFORM, fs_b, fs_init },
    [LJ_WIN_HIDDEN_FIXTURE - CD_FIRST] = { &lj_form_THIDDENFIXTUREFORM, hf_b, NULL },
    [LJ_WIN_INTENSITY_PATCH - CD_FIRST] = { &lj_form_TINTERNALINTENSITYPATCHFORM, ip_b, ip_init },
    [LJ_WIN_SCANNER1_DEFS - CD_FIRST] = { &lj_form_TSCANNER1DEFSFORM, s1_b, s1_init },
    [LJ_WIN_COLOR_SCROLLER_DEFS - CD_FIRST] = { &lj_form_TCOLORSCROLDEFFORM, cs_b, cs_init },
    [LJ_WIN_RGBPACK_DEFS - CD_FIRST] = { &lj_form_TRGBPACKDEFFORM, rp_b, rp_init },
    [LJ_WIN_PALETTE_VALUE_TYPE - CD_FIRST] = { &lj_form_TPALETTEVALUETYPEFORM, vt_b, vt_init },
    [LJ_WIN_PROFILE_NAME_OLDSTYLE - CD_FIRST] = { &lj_form_TPROFILENAMEOLDSTYLEFORM, on_b, on_init },
    [LJ_WIN_PROFILE_IMPORT_MODIFIED - CD_FIRST] = { &lj_form_TPROFILEIMPORTMODIFIEDFILESFORM, im_b, im_init },
    [LJ_WIN_BITMAP_PICKER - CD_FIRST] = { &lj_form_TBITMAPPICKERFORM2, bp_b, bp_init },
    [LJ_WIN_GET_BITMAP - CD_FIRST] = { &lj_form_TGETBITMAPFORM, gb_b, gb_init },
    [LJ_WIN_RGB_PALETTE - CD_FIRST] = { &lj_form_TRGBPALETTEFORM, rg_b, rg_init },
};

lj_window *lj_config_dlg_open(enum lj_win id)
{
    int i = cd_idx(id);
    if (i < 0 || !cd_rows[i].form) { lj_log("lj_win_open: %s is not a CONFIG id", lj_win_id_name(id)); return NULL; }
    if (cd_win[i]) return cd_win[i];
    if ((id == LJ_WIN_FIXTURE_INFO || id == LJ_WIN_FIXTURE_NOTES || id == LJ_WIN_FIXTURE_SPECIAL || id == LJ_WIN_SCANNER1_DEFS || id == LJ_WIN_COLOR_SCROLLER_DEFS || id == LJ_WIN_RGBPACK_DEFS
         || id == LJ_WIN_CLEANUP || id == LJ_WIN_CLONE || id == LJ_WIN_COPY_FROM) && (!show() || cd_target < 0 || cd_target >= LJ_FIXTURES || !show()->fixture[cd_target].link)) {
        lj_log("%s: no target fixture", lj_win_id_name(id));
        return NULL;
    }
    if (id == LJ_WIN_AUTO_ADDRESS && lj_fixture_config_selected() < 0) { lj_log("%s: no fixture selected", lj_win_id_name(id)); return NULL; }
    {   /* ponytail: the list dialogs share one row store (cd.rows), so one of them is open at a time */
        static const enum lj_win lists[] = { LJ_WIN_AUTO_ADDRESS, LJ_WIN_FIND_PROFILE, LJ_WIN_COPY_FROM, LJ_WIN_FIXTURE_SPECIAL, LJ_WIN_PALETTE_VALUE_TYPE, LJ_WIN_BITMAP_PICKER, LJ_WIN_GET_BITMAP, LJ_WIN_APPLY_MULTI };
        static const enum lj_win memos[] = { LJ_WIN_FIXTURE_NOTES, LJ_WIN_PROFILE_IMPORT_MODIFIED, LJ_WIN_FILE_EXISTS, LJ_WIN_EXPORT_WARNING };   /* and one text store (cd.text) */
        size_t k, mine = 0, memo = 0;
        for (k = 0; k < LJ_COUNT_OF(lists); k++) mine |= lists[k] == id;
        for (k = 0; mine && k < LJ_COUNT_OF(lists); k++) if (lists[k] != id) lj_config_dlg_close(lists[k]);
        for (k = 0; k < LJ_COUNT_OF(memos); k++) memo |= memos[k] == id;
        for (k = 0; memo && k < LJ_COUNT_OF(memos); k++) if (memos[k] != id) lj_config_dlg_close(memos[k]);
    }
    if ((cd_win[i] = lj_window_create(cd_rows[i].form, cd_rows[i].b)) == NULL) { lj_log("%s: cannot create %s", lj_win_id_name(id), cd_rows[i].form->class_name); return NULL; }
    if (cd_rows[i].init) cd_rows[i].init(cd_win[i]);
    lj_win_show(cd_win[i], 0);
    return cd_win[i];
}
void lj_config_dlg_close(enum lj_win id)
{
    int i = cd_idx(id);
    lj_window *w;
    if (i < 0 || !(w = cd_win[i])) return;
    cd_win[i] = NULL;
    lj_win_hide(w);
    lj_window_destroy(w);
}
lj_window *lj_config_dlg_get(enum lj_win id) { int i = cd_idx(id); return i >= 0 ? cd_win[i] : NULL; }
void lj_config_dlg_refresh(enum lj_win id) { if (id == LJ_WIN_APPLY_DEFAULT_DMX) lj_config_dlg_close(id); }   /* the progress box's Timer1: gone after the next pass */
void lj_config_dlg_bind_fns(void) { }   /* no hotkey of 01 s7 opens a CONFIG window */
void lj_config_dlg_target(int fixture) { cd_target = fixture; }
