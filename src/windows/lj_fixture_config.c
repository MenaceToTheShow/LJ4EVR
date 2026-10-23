/* lj_fixture_config.c - see lj_fixture_config.h. Every OnXxx of TLINKCONFIGFORM3 is
 * bound (ME-16). Import Profile, Export All, Create Export File and Reload profile
 * data open lj_gs2_dialogs.c / run the app's reload (ME-31, W3); Print and the
 * Offline Visualizer exports log "Not available in LJ4EVR". W4 (dlg-config): Add Fixture(s),
 * Calculate addresses, Find profile, Copy From Fixture / Clear Fixture Data (F44) and the
 * save-time Warnings (F41) open the CONFIG dialogs of lj_config_dialogs.c; the Fixture
 * Configuration prompts of [12 Q9] are raised here verbatim. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen / localtime of the export (ME-92) */
#include "windows/lj_fixture_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_config_dialogs.h"
#include "windows/lj_warnings.h"

#define LJ_FC_TREE_MAX  640                 /* 201 leaves + at most 201 name folders + manufacturers */
#define LJ_FC_TEXT      112                 /* tabs + manufacturer 31 / name 61 + ", " + mode 31 */
#define LJ_FC_NOTE_LINES 64
#define LJ_FC_NA        "Not available in LJ4EVR"

typedef struct {
    lj_window *win;
    lj_fixture *snapshot;                   /* show->fixture[] at open, restored by "skip changes" */
    int modified, loading, range_check_off;
    /* tree */
    char tree_text[LJ_FC_TREE_MAX][LJ_FC_TEXT]; const char *tree_items[LJ_FC_TREE_MAX]; int16_t tree_id[LJ_FC_TREE_MAX]; int tree_n, tree_leaves;
    int sel_profile;                        /* exe row id selected in the tree, -1 */
    /* configured list */
    char row_text[LJ_FIXTURES][128]; const char *row_items[LJ_FIXTURES];
    int sel;                                /* configured-list selection, -1 */
    char lfix[48], laddr[48], padr[64];
    /* notes */
    char note_buf[4096]; const char *note_lines[LJ_FC_NOTE_LINES]; int note_n;
    uint8_t addr_ok[LJ_FIXTURES];          /* the address-change warning was answered Yes for this row this session */
    uint8_t is_new[LJ_FIXTURES];           /* the save chain's new / modified rows */
} lj_fc_state;
static lj_fc_state fc;

lj_window *lj_fixture_config_window(void) { return fc.win; }
int  lj_fixture_config_modified(void) { return fc.modified; }
int  lj_fixture_config_tree_count(void) { return fc.tree_n; }
int  lj_fixture_config_tree_leaves(void) { return fc.tree_leaves; }
const char *lj_fixture_config_tree_item(int i) { return (i >= 0 && i < fc.tree_n) ? fc.tree_items[i] : NULL; }
int  lj_fixture_config_tree_id(int i) { return (i >= 0 && i < fc.tree_n) ? fc.tree_id[i] : -1; }
int  lj_fixture_config_tree_available(int i) { int id = lj_fixture_config_tree_id(i); return id >= 0 && lj_fixlib_by_id((uint16_t)id) != NULL; }

static lj_show *show(void) { return lj_ctx_get()->show; }
static lj_widget *find(const char *name) { return lj_window_find(fc.win, name); }
static const lj_exe_row *row_by_id(int id) { int i, n = lj_fixlib_count(); for (i = 0; i < n; i++) { const lj_exe_row *r = lj_fixlib_row(i); if (r && r->id == id) return r; } return NULL; }
static int footprint_of(const lj_fixture *f) { const lj_profile *p = lj_fixlib_by_id(f->profile_id); return p && p->footprint ? p->footprint : 1; }
static int used(const lj_fixture *f) { return f->link != 0 || f->profile_id != 0 || f->kind == LJ_FX_GS2; }

/* ---- the Available Profiles tree ---- */
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int stricmp_ascii(const char *a, const char *b) { while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; } return lower((unsigned char)*a) - lower((unsigned char)*b); }   /* [02 s3.1]: case-insensitive since 2.6 */
static const char *manu_of(const lj_exe_row *r) { return r->manufacturer && r->manufacturer[0] ? r->manufacturer : "Other"; }   /* CHM: unlisted manufacturers under "Other" */
static int row_cmp(const void *pa, const void *pb)
{
    const lj_exe_row *a = lj_fixlib_row(*(const int *)pa), *b = lj_fixlib_row(*(const int *)pb);
    int c = stricmp_ascii(manu_of(a), manu_of(b));
    if (c == 0) c = stricmp_ascii(a->name, b->name);
    return c ? c : (int)a->id - (int)b->id;                      /* modes keep the exe order */
}
static int tree_push(int level, const char *text, int id)
{
    char *t;
    if (fc.tree_n >= LJ_FC_TREE_MAX) return -1;
    t = fc.tree_text[fc.tree_n];
    memset(t, '\t', (size_t)level); lj_str_copy(t + level, LJ_FC_TEXT - (size_t)level, text);
    fc.tree_items[fc.tree_n] = t; fc.tree_id[fc.tree_n] = (int16_t)id;
    if (id >= 0) fc.tree_leaves++;
    return fc.tree_n++;
}
static void tree_build(void)
{
    static int order[LJ_FC_TREE_MAX];
    lj_ini *ini = lj_ctx_get()->main_ini;
    int total = lj_fixlib_count(), n = 0, i, j, extras = lj_ini_geti(ini, "FIXTURES", "ShowExtraProfiles", 0);
    char leaf[LJ_FC_TEXT];
    fc.tree_n = 0; fc.tree_leaves = 0;
    if (total > LJ_FC_TREE_MAX) total = LJ_FC_TREE_MAX;
    for (i = 0; i < total; i++) { const lj_exe_row *r = lj_fixlib_row(i); if (r && (r->id <= 200 || extras)) order[n++] = i; }   /* design 6.1: extra ids >= 1000 hidden unless Main.Ini [FIXTURES] ShowExtraProfiles=1 */
    qsort(order, (size_t)n, sizeof order[0], row_cmp);
    for (i = 0; i < n; ) {
        const lj_exe_row *r = lj_fixlib_row(order[i]);
        int modes = 1;
        if (i == 0 || stricmp_ascii(manu_of(r), manu_of(lj_fixlib_row(order[i - 1]))) != 0) tree_push(0, manu_of(r), -1);
        while (i + modes < n && stricmp_ascii(lj_fixlib_row(order[i + modes])->name, r->name) == 0 && stricmp_ascii(manu_of(lj_fixlib_row(order[i + modes])), manu_of(r)) == 0) modes++;
        if (modes == 1) {                                        /* PROPOSAL rig: a single-mode profile is one leaf "name, mode" (CHM example "CP Color 150, CMY Mode"), "name" when the mode is empty */
            if (r->mode && r->mode[0]) snprintf(leaf, sizeof leaf, "%s, %s", r->name, r->mode); else lj_str_copy(leaf, sizeof leaf, r->name);
            tree_push(1, leaf, r->id);
        } else {                                                 /* "listed as separate entries in a separate sub folder" [02 s3.1]: Martin > MAC 500 > Mode 2/4 */
            tree_push(1, r->name, -1);
            for (j = 0; j < modes; j++) { const lj_exe_row *m = lj_fixlib_row(order[i + j]); tree_push(2, m->mode && m->mode[0] ? m->mode : m->name, m->id); }
        }
        i += modes;
    }
}
int lj_fixture_config_tree_find(const char *manufacturer, const char *name, const char *mode)
{
    int i, in_manu = 0, in_name = 0;
    for (i = 0; i < fc.tree_n; i++) {
        const char *t = fc.tree_items[i]; int level = 0;
        while (*t == '\t') { level++; t++; }
        if (level == 0) { in_manu = strcmp(t, manufacturer) == 0; in_name = 0; continue; }
        if (!in_manu) continue;
        if (level == 1) { in_name = fc.tree_id[i] < 0 && strcmp(t, name) == 0; if (fc.tree_id[i] >= 0 && strcmp(t, name) == 0 && !mode) return i; continue; }
        if (level == 2 && in_name && mode && strcmp(t, mode) == 0) return i;
    }
    return -1;
}

/* ---- notes pane + "Profile uses %d DMX addresses" (exe 0x1B9200) ---- */
static void notes_show(int id)
{
    const lj_profile *p = id >= 0 ? lj_fixlib_by_id((uint16_t)id) : NULL;
    lj_widget *re = find("reLJNotes");
    const lj_font *f = re ? lj_w_font_of(re) : NULL;
    int maxw = re ? re->w - 2 * 2 - 2 * 3 - LJ_SB_W : 0;         /* the memo's text column beside its scrollbar (MM_BORDER / MM_PADX of lj_w_memo.c) */
    char *s;
    fc.note_n = 0;
    lj_str_copy(fc.note_buf, sizeof fc.note_buf, p && p->notes ? p->notes : (id >= 0 ? LJ_FC_NA : ""));
    for (s = fc.note_buf; fc.note_n < LJ_FC_NOTE_LINES && *s; ) {
        char *nl = strchr(s, '\n'), *q, *brk = NULL;
        if (nl) *nl = 0;
        if (f) for (q = strchr(s, ' '); q; q = strchr(q + 1, ' ')) { *q = 0; if (lj_text_width(f, s) > maxw) { *q = ' '; break; } *q = ' '; brk = q; }   /* the last blank that still fits */
        if (f && brk && lj_text_width(f, s) > maxw) {               /* TRichEdit WordWrap (lj_w_memo does not wrap): break here and go round on the rest */
            *brk = 0; fc.note_lines[fc.note_n++] = s; s = brk + 1;
            if (nl) *nl = '\n';
            continue;
        }
        fc.note_lines[fc.note_n++] = s;
        if (!nl) break;
        s = nl + 1;
    }
    if (re) lj_w_memo_set_lines(re, fc.note_lines, fc.note_n);
    if (p) snprintf(fc.padr, sizeof fc.padr, "Profile uses %d DMX addresses", p->footprint);
    else lj_str_copy(fc.padr, sizeof fc.padr, id >= 0 ? LJ_FC_NA : "");
    lj_w_set_caption(find("pAdr"), fc.padr);
}

/* ---- the configured list and the labels ---- */
static void list_build(void)
{
    lj_show *sh = show();
    lj_widget *lv = find("lvConfig");
    int i, nfix = 0, naddr = 0;
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_fixture *f = sh ? &sh->fixture[i] : NULL;
        if (f && used(f)) {
            const lj_exe_row *r = row_by_id(f->profile_id);
            int fp = footprint_of(f);
            char addr[24] = "";
            if (f->link) snprintf(addr, sizeof addr, "%d:%03d-%03d", f->link, f->address, f->address + fp - 1);   /* link:first-last as the JPEG shows */
            snprintf(fc.row_text[i], sizeof fc.row_text[i], "%d\t%s %s\t%s\t\t%s", i + 1, r ? r->manufacturer : "", r ? r->name : (f->kind == LJ_FX_GS2 ? f->gs2_file : "?"), addr, f->user_name);
            nfix++; if (f->address) naddr += fp;
        } else snprintf(fc.row_text[i], sizeof fc.row_text[i], "%d", i + 1);
        fc.row_items[i] = fc.row_text[i];
    }
    if (lv) { lj_w_listview_set_items(lv, fc.row_items, LJ_FIXTURES); ((lj_w_listview *)lv)->index = fc.sel; }
    snprintf(fc.lfix, sizeof fc.lfix, "%d Fixtures configured", nfix);       /* exe 0x1B2720 " Fixtures configured" */
    snprintf(fc.laddr, sizeof fc.laddr, "%d DMX Addresses Used", naddr);     /* exe 0x1B19FC " DMX Addresses Used" */
    lj_w_set_caption(find("LFixtures"), fc.lfix);
    lj_w_set_caption(find("lAddresses"), fc.laddr);
}

/* ---- the Fixture Settings panel ---- */
int lj_fixture_config_dip_text(int address, char *out, size_t cap)
{
    int i;
    if (!out || cap < 11) return -1;
    for (i = 0; i < 10; i++) out[i] = (address >> i) & 1 ? '1' : '0';          /* lj_w_dipswitch: switch i = bit i-1, set = thrown down */
    out[10] = 0;
    return 0;
}
static void set_enabled(const char *name, int on) { lj_widget *w = find(name); if (w) { w->enabled = on != 0; lj_widget_invalidate(w); } }
static void set_visible(const char *name, int on) { lj_widget *w = find(name); if (w) { w->visible = on != 0; lj_widget_invalidate(w); } }
/* the 100 ms refresh refills the panel: an edit the user is typing into keeps its text and caret (W2 acceptance: "17" read back "" / "71") */
static void set_edit(const char *name, const char *t) { lj_widget *w = find(name); if (w && !w->focused) lj_w_edit_set_text(w, t); }
static int has_pt(const lj_profile *p) { int i; if (!p) return 0; for (i = 0; i < p->footprint && i < LJ_PROFILE_CH; i++) if (p->channel_class[i] == LJ_CH_PAN || p->channel_class[i] == LJ_CH_TILT) return 1; return 0; }

static void panel_fill(void)
{
    lj_show *sh = show();
    const lj_fixture *f = (sh && fc.sel >= 0 && fc.sel < LJ_FIXTURES && used(&sh->fixture[fc.sel])) ? &sh->fixture[fc.sel] : NULL;
    static const char *const links[4] = { "sbL1", "sbl2", "sbl3", "sbl4" };
    char num[8];
    int i;
    fc.loading = 1;
    set_visible("pFixtureSettings", f != NULL);
    set_enabled("bRemove", f != NULL);
    set_enabled("bClear", f != NULL);
    if (f) {
        const lj_profile *p = lj_fixlib_by_id(f->profile_id);
        for (i = 0; i < 4; i++) lj_w_speedbutton_set_down(find(links[i]), f->link == i + 1);
        snprintf(num, sizeof num, "%d", f->address);
        set_edit("eAdr", f->address ? num : "");
        lj_w_dipswitch_set_value(find("DipSwitch1"), f->address);
        set_visible("lDip", 1); set_visible("DipSwitch1", 1);                 /* W1: every profile shows the DIP pattern (LJ2 gates it on the profile's DIP flag, undecoded) */
        set_visible("pPTOptions", has_pt(p));
        lj_w_checkbox_set_checked(find("cbInvPan"), f->invert_pan);
        lj_w_checkbox_set_checked(find("cbInvTilt"), f->invert_tilt);
        lj_w_checkbox_set_checked(find("cbSwapPT"), f->swap_pt);
        lj_w_checkbox_set_checked(find("cbIgnoreBo"), f->ignore_bo);
        lj_w_checkbox_set_checked(find("cbIgnoreGM"), f->ignore_gm);
        lj_w_checkbox_set_checked(find("cbLinkSM"), f->link_strobe_gm);
        set_edit("EUserName", f->user_name);
    }
    fc.loading = 0;
}

static void popup_gate(void);
void lj_fixture_config_refresh(void) { if (!fc.win) return; list_build(); panel_fill(); popup_gate(); }

int lj_fixture_config_select(int fixture)
{
    if (fixture < -1 || fixture >= LJ_FIXTURES) return -1;
    fc.sel = fixture;
    panel_fill();
    popup_gate();
    return 0;
}

static int overlaps(const lj_show *sh, int self, int link, int addr, int fp)
{
    int j;
    for (j = 0; j < LJ_FIXTURES; j++) {
        const lj_fixture *o = &sh->fixture[j];
        if (j == self || !used(o) || o->link != link || o->address == 0) continue;
        if (addr < o->address + footprint_of(o) && o->address < addr + fp) return 1;
    }
    return 0;
}

int lj_fixture_config_set_address(int fixture, int link, int address)
{
    lj_show *sh = show();
    lj_fixture *f;
    if (!sh || fixture < 0 || fixture >= LJ_FIXTURES || link < 1 || link > LJ_LINKS || address < 0 || address > LJ_LINK_CH) return -1;
    f = &sh->fixture[fixture];
    if (!used(f)) return -1;
    f->link = (uint8_t)link; f->address = (uint16_t)address;
    fc.modified = 1;
    if (fixture == fc.sel) panel_fill();
    list_build();
    return 0;
}

int lj_fixture_config_calculate(int fixture)
{
    lj_show *sh = show();
    int link, addr, fp;
    if (!sh || fixture < 0 || fixture >= LJ_FIXTURES || !used(&sh->fixture[fixture])) return -1;
    fp = footprint_of(&sh->fixture[fixture]);
    for (link = 1; link <= LJ_LINKS; link++)                        /* first fit over links 1-4 [02 s2.5]; "calculation and suggestion only" */
        for (addr = 1; addr + fp - 1 <= LJ_LINK_CH; addr++)
            if (!overlaps(sh, fixture, link, addr, fp)) return lj_fixture_config_set_address(fixture, link, addr);
    return -1;
}

int lj_fixture_config_add(uint16_t profile_id, int link, int address)
{
    lj_show *sh = show();
    const lj_exe_row *r = row_by_id(profile_id);
    lj_fixture fx;
    int i;
    if (!sh || !r || !lj_fixlib_by_id(profile_id) || link < 1 || link > LJ_LINKS || address < 0 || address > LJ_LINK_CH) return -1;
    memset(&fx, 0, sizeof fx);
    fx.kind = LJ_FX_BUILTIN; fx.profile_id = profile_id; fx.link = (uint8_t)link; fx.address = (uint16_t)address;
    lj_str_copy(fx.user_name, sizeof fx.user_name, r->tag ? r->tag : "");    /* "user name (defaults to the profile tag)" [02 s9.2] */
    for (i = 0; i < LJ_FIXTURES && used(&sh->fixture[i]); i++) { }      /* first free row (lj_show_add_fixture does the same; kept local so the dialog works against the W1 model stub) */
    if (i >= LJ_FIXTURES) { lj_log("fixture config: no free fixture slot (%d)", LJ_FIXTURES); return -1; }
    sh->fixture[i] = fx;
    fc.modified = 1;
    fc.sel = i;
    list_build(); panel_fill();
    return i;
}

static void remove_fixture(int i)
{
    lj_show *sh = show();
    if (!sh || i < 0 || i >= LJ_FIXTURES) return;
    lj_show_remove_fixture(sh, i);
    fc.modified = 1;
    fc.sel = -1;
    list_build(); panel_fill();
}

/* ---- Save (F41, [02 s2.6], 12 Q9): the warning list, "Clear data for new fixtures ?", the default DMX values, then Config.dat ---- */
static int fc_ini(const char *key) { return lj_ini_geti(lj_ctx_get()->forms_ini, "FIXTURECONF", key, 1); }   /* LJ2's keys incl. the "Promt" typo */
static int same_profile(const lj_fixture *a, const lj_fixture *b) { return a->kind == b->kind && a->profile_id == b->profile_id && strcmp(a->gs2_file, b->gs2_file) == 0; }
static int fc_mark_new(void)   /* rows whose profile is new since the last save: their data space is re-used [02 s2.6] */
{
    lj_show *sh = show();
    int i, n = 0;
    for (i = 0; i < LJ_FIXTURES; i++) {
        fc.is_new[i] = (uint8_t)(sh && used(&sh->fixture[i]) && (!fc.snapshot || !used(&fc.snapshot[i]) || !same_profile(&sh->fixture[i], &fc.snapshot[i])));
        n += fc.is_new[i];
    }
    return n;
}
static int fc_write(void);
static void fc_apply_default(int yes, void *ud)
{
    int list[LJ_FIXTURES], i, n = 0;
    (void)ud;
    for (i = 0; yes && i < LJ_FIXTURES; i++) if (fc.is_new[i]) list[n++] = i;
    if (n) lj_config_dlg_default_dmx(list, n);
    fc_write();
}
static void fc_clear_new(int yes, void *ud)
{
    int i;
    (void)ud;
    for (i = 0; yes && i < LJ_FIXTURES; i++) if (fc.is_new[i]) lj_config_dlg_fixture_data(-1, i, LJ_FXD_ALL);
    if (fc_ini("AlwaysApplyDefault")) fc_apply_default(1, NULL);
    else lj_win_confirm("Please Confirm..", "Apply Default DMX values to new/modified fixtures ?", NULL, fc_apply_default, NULL);   /* exe 0x1B3EDC */
}
int lj_fixture_config_save_anyway(void)
{
    if (!show()) return -1;
    if (!fc_mark_new()) return fc_write();
    if (fc_ini("PromtClearOnSave")) { lj_win_confirm("Please Confirm..", "Clear data for new fixtures ?", NULL, fc_clear_new, NULL); return 1; }   /* exe 0x1B3EB4 */
    fc_clear_new(0, NULL);
    return lj_win_message_window() ? 1 : 0;
}
int lj_fixture_config_save(void)
{
    if (!show()) return -1;
    if (lj_config_dlg_warnings(NULL, 0) > 0) { lj_win_open(LJ_WIN_CONFIG_WARN); return 1; }   /* TCONFIGWARNFORM: Save  Anyway / Cancel */
    return lj_fixture_config_save_anyway();
}
static int fc_write(void)
{
    lj_ctx *ctx = lj_ctx_get();
    if (!ctx->show) return -1;
    if (ctx->lib && lj_show_save_config(ctx->show, ctx->lib) != 0) { lj_win_message("Fixture Configuration", "Error saving Config.dat"); return -1; }
    if (ctx->engine) { lj_lock(); lj_engine_rebind(ctx->engine); lj_unlock(); }
    lj_fixture_config_mark_saved();
    lj_desktop_icons_refresh();
    lj_unassigned_open();                                            /* "After Close, new fixtures land in the Unassigned Fixtures list" [02 s2.6] */
    lj_unassigned_refresh();
    return 0;
}

/* ---- handlers (DFM names) ---- */
static void na(lj_widget *w, void *ud) { (void)ud; lj_log("fixture config: %s: " LJ_FC_NA, w && w->name ? w->name : "menu"); }

static void on_tree_change(lj_widget *w, void *ud)
{
    int i = ((lj_w_treeview *)w)->index;
    (void)ud;
    fc.sel_profile = lj_fixture_config_tree_id(i);
    notes_show(fc.sel_profile);
    set_enabled("bAdd", fc.sel_profile >= 0 && lj_fixture_config_tree_available(i));
}
static void on_add(lj_widget *w, void *ud) { (void)w; (void)ud; if (fc.sel_profile >= 0) lj_config_dlg_add_fixtures(fc.sel_profile); }   /* TAddFixForm2 [02 s2.3] */
static void on_list_change(lj_widget *w, void *ud) { (void)ud; lj_fixture_config_select(lj_w_listview_index(w)); }

static void remove_result(int yes, void *ud) { (void)ud; if (yes) remove_fixture(fc.sel); }
static void on_remove(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    (void)w; (void)ud;
    if (fc.sel < 0) return;
    if (ctx->forms_ini && !lj_ini_geti(ctx->forms_ini, "FIXTURECONF", "ConfirmRemove", 1)) { remove_fixture(fc.sel); return; }
    lj_win_confirm("Please Confirm..", "Remove Selected Fixtures ?", NULL, remove_result, NULL);   /* exe 0x1B15F4 */
}
static void clear_settings(int yes, void *ud)
{
    lj_show *sh = show();
    lj_fixture *f;
    const lj_exe_row *r;
    (void)ud;
    if (!yes || !sh || fc.sel < 0) return;
    f = &sh->fixture[fc.sel]; r = row_by_id(f->profile_id);
    f->invert_pan = f->invert_tilt = f->swap_pt = f->ignore_bo = f->ignore_gm = f->link_strobe_gm = 0;
    lj_str_copy(f->user_name, sizeof f->user_name, r && r->tag ? r->tag : "");
    fc.modified = 1;
    list_build(); panel_fill();
}
static void on_clear(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (fc.sel < 0) return;
    if (fc_ini("ConfirmClear")) lj_win_confirm("Please Confirm..", "Clear Selected Fixtures ?", NULL, clear_settings, NULL);   /* exe 0x1B179C */
    else clear_settings(1, NULL);
}
/* changing the address of a saved fixture invalidates its programs [02 s1.4]: exe 0x1B2FDC, once per row and session;
 * "No" puts the saved address back. Caption = its first line, the two labels its last two (PROPOSAL rig: TConfirmForm has
 * two labels; the copy-from-fixture sentence is not shown). */
static void addr_answer(int yes, void *ud)
{
    int f = (int)(intptr_t)ud;
    if (yes) { fc.addr_ok[f] = 1; return; }
    if (fc.snapshot) lj_fixture_config_set_address(f, fc.snapshot[f].link, fc.snapshot[f].address);
}
static void ui_set_address(int f, int link, int address)
{
    lj_show *sh = show();
    const lj_fixture *s = fc.snapshot ? &fc.snapshot[f] : NULL;
    if (lj_fixture_config_set_address(f, link, address) != 0 || !sh) return;
    if (!s || fc.addr_ok[f] || !used(s) || !s->address || !same_profile(s, &sh->fixture[f]) || (s->link == sh->fixture[f].link && s->address == sh->fixture[f].address)) return;
    if (lj_win_message_window()) return;
    lj_win_confirm("IMPORTANT WARNING", "Changing the DMX address for an alreay programmed fixture will invalidate existing programs.",
                   "Do you wish to change the fixtures DMX address ?", addr_answer, (void *)(intptr_t)f);
}
static void on_link(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    (void)ud;
    if (fc.loading || !sh || fc.sel < 0 || !w->name || strlen(w->name) < 4) return;
    ui_set_address(fc.sel, w->name[3] - '0', sh->fixture[fc.sel].address);
    lj_w_speedbutton_set_down(w, 1);
}
static void addr_from_edit(void)
{
    lj_show *sh = show();
    const char *t = lj_w_edit_text(find("eAdr"));
    long v;
    if (fc.loading || !sh || fc.sel < 0 || !t) return;
    v = strtol(t, NULL, 10);
    if (v < 0) v = 0;
    if (v > LJ_LINK_CH) v = LJ_LINK_CH;
    if (v != sh->fixture[fc.sel].address) ui_set_address(fc.sel, sh->fixture[fc.sel].link ? sh->fixture[fc.sel].link : 1, (int)v);
    else lj_w_dipswitch_set_value(find("DipSwitch1"), (int)v);
}
static void on_addr_exit(lj_widget *w, void *ud) { (void)w; (void)ud; addr_from_edit(); }
static void on_addr_key(lj_widget *w, void *ud) { (void)w; (void)ud; addr_from_edit(); }
static void on_ud(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    int v;
    (void)ud;
    if (fc.loading || !sh || fc.sel < 0) return;
    v = sh->fixture[fc.sel].address + lj_w_udbutton_delta(w);
    if (v < 0) v = 0;
    if (v > LJ_LINK_CH) v = LJ_LINK_CH;
    ui_set_address(fc.sel, sh->fixture[fc.sel].link ? sh->fixture[fc.sel].link : 1, v);
}
static void on_auto(lj_widget *w, void *ud) { (void)w; (void)ud; if (fc.sel >= 0) lj_win_open(LJ_WIN_AUTO_ADDRESS); }   /* TAutoAddressForm2 "Calculate addresses" (F43) */
static void on_flag(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    lj_fixture *f;
    int on = lj_w_checkbox_checked(w);
    (void)ud;
    if (fc.loading || !sh || fc.sel < 0 || !w->name) return;
    f = &sh->fixture[fc.sel];
    if (strcmp(w->name, "cbInvPan") == 0) f->invert_pan = (uint8_t)on;
    else if (strcmp(w->name, "cbInvTilt") == 0) f->invert_tilt = (uint8_t)on;
    else if (strcmp(w->name, "cbSwapPT") == 0) f->swap_pt = (uint8_t)on;
    else if (strcmp(w->name, "cbIgnoreBo") == 0) f->ignore_bo = (uint8_t)on;
    else if (strcmp(w->name, "cbIgnoreGM") == 0) f->ignore_gm = (uint8_t)on;
    else if (strcmp(w->name, "cbLinkSM") == 0) f->link_strobe_gm = (uint8_t)on;
    fc.modified = 1;
}
static void on_username(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    const char *t = lj_w_edit_text(w);
    (void)ud;
    if (fc.loading || !sh || fc.sel < 0 || !t) return;
    lj_str_copy(sh->fixture[fc.sel].user_name, sizeof sh->fixture[fc.sel].user_name, t);
    fc.modified = 1;
    list_build();
}
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_fixture_config_save(); }
static void on_collapse(lj_widget *w, void *ud) { lj_widget *tv = find("tvAvailable"); int i; (void)w; (void)ud; for (i = 0; i < fc.tree_n; i++) lj_w_treeview_expand(tv, i, 0); }
static void on_locate(lj_widget *w, void *ud)
{
    lj_widget *tv = find("tvAvailable");
    const char *want = !w->name ? "" : strcmp(w->name, "sbMartin") == 0 ? "Martin" : strcmp(w->name, "sbGeneric") == 0 ? "Generic" : "User Definable";
    int i, r;
    (void)ud;
    if (!tv) return;
    for (i = 0; i < fc.tree_n; i++) if (fc.tree_items[i][0] != '\t' && strcmp(fc.tree_items[i], want) == 0) break;
    if (i >= fc.tree_n) return;
    lj_w_treeview_expand(tv, i, 1);
    ((lj_w_treeview *)tv)->index = i;
    for (r = 0; lj_w_treeview_visible_item(tv, r) >= 0; r++) if (lj_w_treeview_visible_item(tv, r) == i) { ((lj_w_treeview *)tv)->sb.pos = r; break; }
    on_tree_change(tv, NULL);
}
static void on_find(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_FIND_PROFILE); }   /* TFindProfileForm "Find profile.." [02 s2.3] */
/* Copy From Fixture / Clear Fixture Data: "only after the configuration is saved, only for fixtures with a valid address" [02 s2.9-2.10] */
static void popup_gate(void)
{
    lj_menu *m = fc.win ? lj_window_menus(fc.win) : NULL;
    int ok = fc.sel >= 0 && lj_fixture_config_saved(fc.sel);
    if (!m) return;
    lj_menu_set_enabled(m, "P_Clone", ok); lj_menu_set_enabled(m, "P_Clear", ok);
}
static void on_popup(lj_widget *w, void *ud) { (void)w; (void)ud; popup_gate(); }
static void on_clone(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(fc.win);
    (void)w; (void)ud;
    if (fc.sel < 0 || !lj_fixture_config_saved(fc.sel)) return;
    lj_config_dlg_target(fc.sel);
    lj_win_open(it && it->name && !strcmp(it->name, "P_Clear") ? LJ_WIN_CLEANUP : LJ_WIN_COPY_FROM);
}
static void on_notes(lj_widget *w, void *ud) { lj_show *sh = show(); (void)w; (void)ud; if (sh && fc.sel >= 0) notes_show(sh->fixture[fc.sel].profile_id); }
/* ---- Export to text file (ME-31, ME-92): TEXPORTCONFIGFORM "Configuration Export" shows the text in re1, "Save to text file"
 * writes it. Every literal is the exe pool block 0x1AFA18-0x1AFB64 (title, Date, User Library, the column header, the row
 * format "%s %s %s %s (%s)", N/A, "- End of list", Configuration.txt). ---- */
#define LJ_FC_EXPORT_CAP (LJ_FIXTURES * 160 + 1024)
static struct { lj_window *win; char text[LJ_FC_EXPORT_CAP], lines_buf[LJ_FC_EXPORT_CAP]; const char *lines[LJ_FIXTURES + 8]; } fx_ex;
static void pad(char *dst, size_t cap, const char *s, int width) { snprintf(dst, cap, "%-*.*s", width, width, s ? s : ""); }
int lj_fixture_config_export_text(char *out, size_t cap)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *sh = show();
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char date[64] = "", num[12], no[8], type[48], user[24], addr[24], fp[16], full[112];
    size_t n = 0;
    int i, k;
    if (!out || !cap) return -1;
    if (tm) snprintf(date, sizeof date, "%04d-%02d-%02d %02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);   /* PROPOSAL rig: LJ2's date format (host short date) */
#define LJ_FC_PUT(...) do { k = snprintf(out + n, cap - n, __VA_ARGS__); if (k < 0 || (size_t)k >= cap - n) return -1; n += (size_t)k; } while (0)
    LJ_FC_PUT("LightJockey fixture configuration export\r\n");
    LJ_FC_PUT("Date        : %s\r\n", date);
    LJ_FC_PUT("User Library: %s     (Location: %s)\r\n", ctx->lib ? ctx->lib->name : "", ctx->lib ? ctx->lib->root : "");
    LJ_FC_PUT("\r\n");                                                    /* PROPOSAL rig: the blank line before the header */
    LJ_FC_PUT("No   Fixture Type                             User Name            DMX Address\r\n");
    for (i = 0; sh && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &sh->fixture[i];
        const lj_exe_row *r;
        int foot;
        if (!used(f)) continue;
        r = row_by_id(f->profile_id); foot = footprint_of(f);
        snprintf(num, sizeof num, "%d", i + 1);
        if (r && r->mode && r->mode[0]) snprintf(full, sizeof full, "%s %s, %s", r->manufacturer, r->name, r->mode);
        else snprintf(full, sizeof full, "%s %s", r ? r->manufacturer : "", r ? r->name : (f->kind == LJ_FX_GS2 ? f->gs2_file : "?"));
        if (f->link) snprintf(addr, sizeof addr, "%d:%03d-%03d", f->link, f->address, f->address + foot - 1); else lj_str_copy(addr, sizeof addr, "N/A");
        snprintf(fp, sizeof fp, "%d ch", foot);                           /* PROPOSAL rig: what the exe's "(%s)" holds */
        pad(no, sizeof no, num, 4);                                       /* padded to the header's column starts: PROPOSAL rig, column padding from the header line */
        pad(type, sizeof type, full, 40); pad(user, sizeof user, f->user_name, 20);
        LJ_FC_PUT("%s %s %s %s (%s)\r\n", no, type, user, addr, fp);
    }
    LJ_FC_PUT("- End of list\r\n");
#undef LJ_FC_PUT
    return 0;
}
int lj_fixture_config_export_save(const char *path)
{
    char text[LJ_FC_EXPORT_CAP];
    FILE *f;
    if (!path || lj_fixture_config_export_text(text, sizeof text) != 0 || (f = fopen(path, "wb")) == NULL) return -1;
    fputs(text, f);
    return fclose(f) == 0 ? 0 : -1;
}
lj_window *lj_fixture_config_export_window(void) { return fx_ex.win; }
static void export_close(void) { if (fx_ex.win) { lj_win_hide(fx_ex.win); lj_window_destroy(fx_ex.win); fx_ex.win = NULL; } }
static void export_close_ev(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) export_close(); }   /* BitBtn2 bkClose has no OnClick */
static void export_save(lj_widget *w, void *ud)                       /* "Save to text file": Configuration.txt in the user library (no TSaveDialog yet: PROPOSAL rig, the file name is the exe default) */
{
    lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP];
    (void)w; (void)ud;
    if (!ctx->lib) lj_str_copy(path, sizeof path, "Configuration.txt");
    else if (lj_library_path(ctx->lib, "Configuration.txt", path, sizeof path) != 0) return;
    if (lj_fixture_config_export_save(path) != 0) lj_log("fixture config: %s not written", path); else lj_log("fixture config: %s written", path);
}
static void export_open(void)
{
    static const lj_binding b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "BitBtn1Click", export_save, NULL }, { NULL, NULL, NULL } };
    lj_widget *bt;
    char *p;
    int nl = 0;
    export_close();
    if (lj_fixture_config_export_text(fx_ex.text, sizeof fx_ex.text) != 0) { lj_log("fixture config: export text too long"); return; }
    if ((fx_ex.win = lj_window_create(&lj_form_TEXPORTCONFIGFORM, b)) == NULL) { lj_log("fixture config: cannot create TExportConfigForm"); return; }
    memcpy(fx_ex.lines_buf, fx_ex.text, sizeof fx_ex.text);
    for (p = fx_ex.lines_buf; *p && nl < (int)LJ_COUNT_OF(fx_ex.lines); ) {   /* re1: one memo line per CRLF line */
        char *e = strstr(p, "\r\n");
        fx_ex.lines[nl++] = p;
        if (!e) break;
        *e = 0; p = e + 2;
    }
    lj_w_memo_set_lines(lj_window_find(fx_ex.win, "re1"), fx_ex.lines, nl);
    if ((bt = lj_window_find(fx_ex.win, "BitBtn2")) != NULL) { bt->on_event = export_close_ev; bt->ud = NULL; }
    lj_win_show(fx_ex.win, 0);
}
static void on_print(lj_widget *w, void *ud)                          /* M_Print and M_ExportConfig share M_PrintClick (DFM) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(fc.win);
    (void)w; (void)ud;
    if (it && it->name && !strcmp(it->name, "M_ExportConfig")) export_open();
    else lj_log("fixture config: Print Configuration: " LJ_FC_NA);   /* ME-31: no printing path (docs/fidelity.md) */
}
static void on_gs2(lj_widget *w, void *ud)                            /* ME-31: Import Profile, Export All, Create Export File, Reload profile data */
{
    const lj_menu_item_desc *it = lj_window_menu_item(fc.win);
    const char *n = it && it->name ? it->name : "";
    (void)w; (void)ud;
    if (!strcmp(n, "M_ImportProfile")) lj_win_open(LJ_WIN_GS2_IMPORT);
    else if (!strcmp(n, "M_ExportAll")) lj_desktop_run_action(LJ_ACT_GS2_EXPORT_ALL, 0);
    else if (!strcmp(n, "M_CreateExportFile")) lj_win_open(LJ_WIN_GS2_EXPORT);
    else if (!strcmp(n, "P_ReloadProfileData")) { lj_desktop_run_action(LJ_ACT_GS2_RELOAD, 0); tree_build(); if (find("tvAvailable")) lj_w_treeview_set_items(find("tvAvailable"), fc.tree_items, fc.tree_n); }
}
static void on_warnings(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = lj_window_menu_item(fc.win);
    lj_menu *m = lj_window_menus(fc.win);
    lj_menu_rt *rt = it ? lj_menu_rt_find(m, it) : NULL;
    const char *key;
    (void)w; (void)ud;
    if (!it || !rt) return;
    rt->checked = !rt->checked;
    key = strcmp(it->name, "M_RemoveFixtures") == 0 ? "ConfirmRemove" : strcmp(it->name, "M_OverwriteFixtures") == 0 ? "ConfirmOverwrite" : strcmp(it->name, "M_ClearFixtures") == 0 ? "ConfirmClear"
        : strcmp(it->name, "M_PrompClear") == 0 ? "PromtClearOnSave" : strcmp(it->name, "M_ApplyDefault") == 0 ? "AlwaysApplyDefault" : NULL;   /* forms.ini [FIXTURECONF], LJ2's keys incl. the "Promt" typo */
    if (key && lj_ctx_get()->forms_ini) lj_ini_seti(lj_ctx_get()->forms_ini, "FIXTURECONF", key, rt->checked);
    if (!key) fc.range_check_off = rt->checked;                         /* Disable Hardware address range check: every link is allowed in LJ4EVR anyway */
}
static void skip_result(int yes, void *ud)
{
    lj_show *sh = show();
    (void)ud;
    if (!yes) return;
    if (sh && fc.snapshot) memcpy(sh->fixture, fc.snapshot, sizeof sh->fixture);
    fc.modified = 0;
    lj_fixture_config_close();
}
static void on_form_close(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (fc.modified) lj_win_confirm("Please Confirm..", "Configuration was changed, skip changes ?", NULL, skip_result, NULL);   /* exe 0x1B3F64 */
    else lj_fixture_config_close();
}

static const lj_binding lj_fc_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_form_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyDown", lj_handler_noop, NULL },
    { "lvConfigChange", on_list_change, NULL }, { "lvConfigColumnClick", lj_handler_noop, NULL }, { "lvConfigCompare", lj_handler_noop, NULL }, { "lvConfigDragDrop", lj_handler_noop, NULL },
    { "lvConfigDragOver", lj_handler_noop, NULL }, { "lvConfigKeyDown", lj_handler_noop, NULL },
    { "tvAvailableChange", on_tree_change, NULL }, { "tvAvailableMouseDown", lj_handler_noop, NULL },
    { "sbFindClick", on_find, NULL }, { "sbLocateClick", on_locate, NULL }, { "sbCollapseClick", on_collapse, NULL }, { "bAddClick", on_add, NULL },
    { "bSaveClick", on_save, NULL }, { "bRemoveClick", on_remove, NULL }, { "bRemoveDragDrop", lj_handler_noop, NULL }, { "bRemoveDragOver", lj_handler_noop, NULL }, { "bClearClick", on_clear, NULL },
    { "LinkMouseDown", on_link, NULL }, { "eAdrExit", on_addr_exit, NULL }, { "eAdrKeyUp", on_addr_key, NULL }, { "UdButton1Change", on_ud, NULL }, { "bAutoAddressClick", on_auto, NULL },
    { "cbInvPanClick", on_flag, NULL }, { "cbIntensitySpecialClick", on_flag, NULL }, { "EUserNameKeyUp", on_username, NULL },
    { "M_PrintClick", on_print, NULL }, { "M_ImportProfileClick", on_gs2, NULL }, { "LocalHelpClick", lj_handler_noop, NULL }, { "M_ExportAllClick", on_gs2, NULL },
    { "WarningsClick", on_warnings, NULL }, { "PrefsClick", on_warnings, NULL }, { "Help2Click", lj_handler_noop, NULL },
    { "popAvailablePopup", lj_handler_noop, NULL }, { "M_CreateExportFileClick", on_gs2, NULL }, { "M_CreateOVExportClick", na, NULL }, { "M_DeleteProfileClick", na, NULL },
    { "ConfiguredPopPopup", on_popup, NULL }, { "P_FixtureNotesClick", on_notes, NULL }, { "P_CloneClick", on_clone, NULL }, { "P_ClearClick", on_clone, NULL }, { "P_ReloadProfileDataClick", on_gs2, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_fixture_config_open(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_widget *tv;
    lj_menu *m;
    static const char *const greyed[] = { "M_Print" /* ME-31: no printing path */,
                                          "M_CreateExportfileOV", "M_CreateOVExport" /* Offline Visualizer: design 8.5 */, "M_DeleteProfile" /* W4 */ };
    static const char *const checks[][2] = { { "M_RemoveFixtures", "ConfirmRemove" }, { "M_OverwriteFixtures", "ConfirmOverwrite" }, { "M_ClearFixtures", "ConfirmClear" }, { "M_PrompClear", "PromtClearOnSave" }, { "M_ApplyDefault", "AlwaysApplyDefault" } };
    size_t i;
    if (fc.win) return fc.win;
    fc.win = lj_window_create(&lj_form_TLINKCONFIGFORM3, lj_fc_bindings);
    if (!fc.win) { lj_log("fixture config: cannot create TLinkConfigForm3"); return NULL; }
    fc.sel = -1; fc.sel_profile = -1; fc.modified = 0;
    if (ctx->show) { fc.snapshot = (lj_fixture *)malloc(sizeof ctx->show->fixture); if (fc.snapshot) memcpy(fc.snapshot, ctx->show->fixture, sizeof ctx->show->fixture); }
    tree_build();
    if ((tv = find("tvAvailable")) != NULL) lj_w_treeview_set_items(tv, fc.tree_items, fc.tree_n);
    m = lj_window_menus(fc.win);
    for (i = 0; i < LJ_COUNT_OF(greyed); i++) lj_menu_set_enabled(m, greyed[i], 0);
    for (i = 0; i < LJ_COUNT_OF(checks); i++) lj_menu_set_checked(m, checks[i][0], lj_ini_geti(ctx->forms_ini, "FIXTURECONF", checks[i][1], 1));
    notes_show(-1);
    lj_w_tabs_select(find("PCNotes"), 0);                             /* LJ Profile Notes in front, as fixtureconfiguration.jpg shows (the DFM's ActivePage is the O/V page) */
    set_enabled("bAdd", 0);
    lj_w_set_caption(find("sbMartin"), "Martin");                  /* F42: the blank locate button captioned with text, no logo (D2; fidelity row) */
    memset(fc.addr_ok, 0, sizeof fc.addr_ok);
    lj_fixture_config_refresh();
    lj_win_show(fc.win, 0);
    return fc.win;
}

void lj_fixture_config_close(void)
{
    export_close();
    if (!fc.win) return;
    lj_win_hide(fc.win);
    lj_window_destroy(fc.win);
    fc.win = NULL;
    free(fc.snapshot); fc.snapshot = NULL;
    fc.tree_n = 0; fc.tree_leaves = 0; fc.sel = -1; fc.modified = 0;
}

/* ---- dlg-config track (W4): the seams the CONFIG dialogs use ---- */
int lj_fixture_config_selected(void) { return fc.win ? fc.sel : -1; }
int lj_fixture_config_saved(int f)
{
    lj_show *sh = show();
    return sh && fc.snapshot && !fc.modified && f >= 0 && f < LJ_FIXTURES && used(&sh->fixture[f]) && sh->fixture[f].address
        && memcmp(&sh->fixture[f], &fc.snapshot[f], sizeof sh->fixture[f]) == 0;
}
void lj_fixture_config_mark_saved(void)
{
    lj_show *sh = show();
    if (sh && fc.snapshot) memcpy(fc.snapshot, sh->fixture, sizeof sh->fixture);
    fc.modified = 0;
    memset(fc.addr_ok, 0, sizeof fc.addr_ok);
}
int lj_fixture_config_put(int f, uint16_t profile_id)
{
    lj_show *sh = show();
    const lj_exe_row *r = row_by_id(profile_id);
    if (!sh || !r || !lj_fixlib_by_id(profile_id) || f < 0 || f >= LJ_FIXTURES) return -1;
    memset(&sh->fixture[f], 0, sizeof sh->fixture[f]);
    sh->fixture[f].kind = LJ_FX_BUILTIN; sh->fixture[f].profile_id = profile_id; sh->fixture[f].link = 1;   /* address 0: "has no DMX Address" until set */
    lj_str_copy(sh->fixture[f].user_name, sizeof sh->fixture[f].user_name, r->tag ? r->tag : "");
    fc.modified = 1;
    fc.sel = f;
    if (fc.win) { list_build(); panel_fill(); }
    return 0;
}
int lj_fixture_config_tree_select(int id)
{
    lj_widget *tv = fc.win ? find("tvAvailable") : NULL;
    int i, k, lvl, r;
    for (i = 0; i < fc.tree_n && fc.tree_id[i] != id; i++) { }
    if (!tv || i >= fc.tree_n) return -1;
    for (k = i - 1, lvl = lj_w_treeview_level(tv, i); k >= 0 && lvl > 0; k--)   /* expand every ancestor folder */
        if (lj_w_treeview_level(tv, k) < lvl) { lj_w_treeview_expand(tv, k, 1); lvl = lj_w_treeview_level(tv, k); }
    ((lj_w_treeview *)tv)->index = i;
    for (r = 0; lj_w_treeview_visible_item(tv, r) >= 0; r++) if (lj_w_treeview_visible_item(tv, r) == i) { ((lj_w_treeview *)tv)->sb.pos = r; break; }
    on_tree_change(tv, NULL);
    lj_widget_invalidate(tv);
    return 0;
}
