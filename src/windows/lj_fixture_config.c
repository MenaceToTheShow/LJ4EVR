/* lj_fixture_config.c - see lj_fixture_config.h. Every OnXxx of TLINKCONFIGFORM3 is
 * bound (ME-16); the W2/W3 leaves (Export to text file, Export All, Import Profile,
 * Reload profile data, Copy From Fixture, Clear Fixture Data, Find, the profile
 * export popup) are greyed or log "Not available in LJ4EVR". */
#include "windows/lj_fixture_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
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
static lj_widget *find(const char *name) { return fc.win ? lj_window_find(fc.win, name) : NULL; }
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
    int total = lj_fixlib_count(), n = 0, i, j, extras = ini ? lj_ini_geti(ini, "FIXTURES", "ShowExtraProfiles", 0) : 0;
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

void lj_fixture_config_refresh(void) { if (!fc.win) return; list_build(); panel_fill(); }

int lj_fixture_config_select(int fixture)
{
    if (fixture < -1 || fixture >= LJ_FIXTURES) return -1;
    fc.sel = fixture;
    panel_fill();
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

int lj_fixture_config_save(void)
{
    lj_ctx *ctx = lj_ctx_get();
    const char *msg;
    if (!ctx->show) return -1;
    msg = lj_config_validate(ctx->show);
    if (msg) { lj_win_message("Warnings", msg); return -1; }          /* TCONFIGWARNFORM's list is W2; the first warning in the message box */
    if (ctx->lib && lj_show_save_config(ctx->show, ctx->lib) != 0) { lj_win_message("Fixture Configuration", "Error saving Config.dat"); return -1; }
    if (ctx->engine) { lj_lock(); lj_engine_rebind(ctx->engine); lj_unlock(); }
    if (fc.snapshot) memcpy(fc.snapshot, ctx->show->fixture, sizeof ctx->show->fixture);
    fc.modified = 0;
    lj_desktop_icons_refresh();
    lj_unassigned_open();                                            /* "After Close, new fixtures land in the Unassigned Fixtures list" [02 s2.6] */
    lj_unassigned_refresh();
    return 0;
}

/* ---- handlers (DFM names) ---- */
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void na(lj_widget *w, void *ud) { (void)ud; lj_log("fixture config: %s: " LJ_FC_NA, w && w->name ? w->name : "menu"); }

static void on_tree_change(lj_widget *w, void *ud)
{
    int i = ((lj_w_treeview *)w)->index;
    (void)ud;
    fc.sel_profile = lj_fixture_config_tree_id(i);
    notes_show(fc.sel_profile);
    set_enabled("bAdd", fc.sel_profile >= 0 && lj_fixture_config_tree_available(i));
}
static void on_add(lj_widget *w, void *ud) { (void)w; (void)ud; if (fc.sel_profile >= 0) lj_fixture_config_add((uint16_t)fc.sel_profile, 1, 0); }
static void on_list_change(lj_widget *w, void *ud) { (void)ud; lj_fixture_config_select(((lj_w_listview *)w)->index); }
static void on_list_key(lj_widget *w, void *ud) { (void)w; (void)ud; }

static void remove_result(int yes, void *ud) { (void)ud; if (yes) remove_fixture(fc.sel); }
static void on_remove(lj_widget *w, void *ud)
{
    lj_ctx *ctx = lj_ctx_get();
    (void)w; (void)ud;
    if (fc.sel < 0) return;
    if (ctx->forms_ini && !lj_ini_geti(ctx->forms_ini, "FIXTURECONF", "ConfirmRemove", 1)) { remove_fixture(fc.sel); return; }
    lj_win_confirm("Please Confirm..", "Remove Selected Fixtures ?", NULL, remove_result, NULL);   /* exe 0x1B15F4 */
}
static void on_clear(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    lj_fixture *f;
    const lj_exe_row *r;
    (void)w; (void)ud;
    if (!sh || fc.sel < 0) return;
    f = &sh->fixture[fc.sel]; r = row_by_id(f->profile_id);
    f->invert_pan = f->invert_tilt = f->swap_pt = f->ignore_bo = f->ignore_gm = f->link_strobe_gm = 0;
    lj_str_copy(f->user_name, sizeof f->user_name, r && r->tag ? r->tag : "");
    fc.modified = 1;
    list_build(); panel_fill();
}
static void on_link(lj_widget *w, void *ud)
{
    lj_show *sh = show();
    (void)ud;
    if (fc.loading || !sh || fc.sel < 0 || !w->name || strlen(w->name) < 4) return;
    lj_fixture_config_set_address(fc.sel, w->name[3] - '0', sh->fixture[fc.sel].address);
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
    if (v != sh->fixture[fc.sel].address) lj_fixture_config_set_address(fc.sel, sh->fixture[fc.sel].link ? sh->fixture[fc.sel].link : 1, (int)v);
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
    lj_fixture_config_set_address(fc.sel, sh->fixture[fc.sel].link ? sh->fixture[fc.sel].link : 1, v);
}
static void on_auto(lj_widget *w, void *ud) { (void)w; (void)ud; if (fc.sel >= 0 && lj_fixture_config_calculate(fc.sel) != 0) lj_win_message("Fixture Configuration", "No free DMX addresses"); }
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
static void on_notes(lj_widget *w, void *ud) { lj_show *sh = show(); (void)w; (void)ud; if (sh && fc.sel >= 0) notes_show(sh->fixture[fc.sel].profile_id); }
static void on_print(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("fixture config: Print / Export: " LJ_FC_NA " (Export to text file: W2)"); }   /* W2 */
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
    { "FormActivate", noop, NULL }, { "FormClose", on_form_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormKeyDown", noop, NULL },
    { "lvConfigChange", on_list_change, NULL }, { "lvConfigColumnClick", noop, NULL }, { "lvConfigCompare", noop, NULL }, { "lvConfigDragDrop", noop, NULL },
    { "lvConfigDragOver", noop, NULL }, { "lvConfigKeyDown", on_list_key, NULL },
    { "tvAvailableChange", on_tree_change, NULL }, { "tvAvailableMouseDown", noop, NULL },
    { "sbFindClick", na, NULL }, { "sbLocateClick", on_locate, NULL }, { "sbCollapseClick", on_collapse, NULL }, { "bAddClick", on_add, NULL },
    { "bSaveClick", on_save, NULL }, { "bRemoveClick", on_remove, NULL }, { "bRemoveDragDrop", noop, NULL }, { "bRemoveDragOver", noop, NULL }, { "bClearClick", on_clear, NULL },
    { "LinkMouseDown", on_link, NULL }, { "eAdrExit", on_addr_exit, NULL }, { "eAdrKeyUp", on_addr_key, NULL }, { "UdButton1Change", on_ud, NULL }, { "bAutoAddressClick", on_auto, NULL },
    { "cbInvPanClick", on_flag, NULL }, { "cbIntensitySpecialClick", on_flag, NULL }, { "EUserNameKeyUp", on_username, NULL },
    { "M_PrintClick", on_print, NULL }, { "M_ImportProfileClick", na, NULL }, { "LocalHelpClick", noop, NULL }, { "M_ExportAllClick", na, NULL },
    { "WarningsClick", on_warnings, NULL }, { "PrefsClick", on_warnings, NULL }, { "Help2Click", noop, NULL },
    { "popAvailablePopup", noop, NULL }, { "M_CreateExportFileClick", na, NULL }, { "M_CreateOVExportClick", na, NULL }, { "M_DeleteProfileClick", na, NULL },
    { "ConfiguredPopPopup", noop, NULL }, { "P_FixtureNotesClick", on_notes, NULL }, { "P_CloneClick", na, NULL }, { "P_ClearClick", na, NULL }, { "P_ReloadProfileDataClick", na, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_fixture_config_open(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_widget *tv;
    lj_menu *m;
    static const char *const greyed[] = { "M_Print" /* ME-31: no printing path */, "M_ExportConfig" /* W2 */, "M_ImportProfile", "M_ExportAll", "P_ReloadProfileData" /* W3 */, "P_Clone", "P_Clear" /* W2 */,
                                          "M_CreateExportfileOV", "M_CreateExportFile", "M_CreateOVExport", "M_DeleteProfile" /* W3 */ };
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
    for (i = 0; i < LJ_COUNT_OF(checks); i++) lj_menu_set_checked(m, checks[i][0], ctx->forms_ini ? lj_ini_geti(ctx->forms_ini, "FIXTURECONF", checks[i][1], 1) : 1);
    notes_show(-1);
    lj_w_tabs_select(find("PCNotes"), 0);                             /* LJ Profile Notes in front, as fixtureconfiguration.jpg shows (the DFM's ActivePage is the O/V page) */
    set_enabled("bAdd", 0);
    lj_fixture_config_refresh();
    lj_win_show(fc.win, 0);
    return fc.win;
}

void lj_fixture_config_close(void)
{
    if (!fc.win) return;
    lj_win_hide(fc.win);
    lj_window_destroy(fc.win);
    fc.win = NULL;
    free(fc.snapshot); fc.snapshot = NULL;
    fc.tree_n = 0; fc.tree_leaves = 0; fc.sel = -1; fc.modified = 0;
}
