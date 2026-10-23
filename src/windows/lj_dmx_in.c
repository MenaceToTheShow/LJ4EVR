/* lj_dmx_in.c - the DMXIN family (docs/w4-contracts.md s3-s4, docs/w5-contracts.md seam 2; design 12 F8 / F48 / I7).
 * W4 opened the five DMX-in windows with every input greyed; W5 (dmxin track) makes them live over io/lj_dmxin.h:
 *   dmx_in             TDmxInForm: the monitor (channels in, update frequency), 512 rows with the incoming (translated)
 *                      value, the table name and the assigned function; Assign / Clear on a WORKING copy, Save writes
 *                      <library>/DmxIn/DmxIn.dat and makes it the live map (05 s3.1: nothing changes before Save); the
 *                      Options and Preferences menus; the popup assigns the current sequence / cue / cue list.
 *                      Entry points: C-L toolbar DMX In (LJ_ACT_DMX_IN), Global Patch DMX In source (double-click).
 *   dmxin_translation  TDMXInValueTranslationForm: channels with their copied table, the Available tables tree (the SYSTEM
 *                      tables regenerated in io/lj_vtr_gen.c, then <userdata>/User/ValueTables/*.vtr), Assign / Remove /
 *                      Update translations, Create / Edit table, Options -> Export (PROPOSAL rig W5-DMXIN-6: the DFM's
 *                      M_Options has an OnClick and no children). Ok copies the translations into the Dmx In working copy.
 *   translation_edit   TTranslationEditForm: the table's grid and graph, Set Linear Range (bSet), ID / Type / Name /
 *                      Purpose; Ok saves <Type>_<Name>_<ID>.vtr under User/ValueTables.
 *   value_table_error  TValueTableImportErrorForm: raised with the reasons when a .vtr in the folder does not parse.
 *   export_table       TExportTableForm: the DFM's drive combo and folder list (ME-17 stubs) replaced by a code-built
 *                      folder list (the lj_backup_restore.c pattern); Export tables writes every user table there.
 * Also: the Hardware Setup "Enable DMX in" row (lj_dmx_in_hs_*) and the LJ_FN_DMXIN_* level rows (lj_dmx_in_bind_fns).
 * Handlers: every OnXxx of the five DFMs (ME-16). */
#include "windows/lj_dmx_in.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "engine/lj_engine.h"
#include "io/lj_audio.h"
#include "io/lj_dmxin.h"
#include "io/lj_fn.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "model/lj_library.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_spinedit.h"
#include "windows/lj_master_intensity.h"
#include "windows/lj_statics2_form.h"

enum { LJ_DI_N = 5, LJ_DI_CH = 512, LJ_DI_VALUES = 256, LJ_DI_TABLES = 128 /* PROPOSAL rig W5-DMXIN-6: tables listed */, LJ_DI_DIRS = 64 };
static lj_window *di_win[LJ_DI_N];
static void (*di_root_orig[LJ_DI_N])(lj_widget *, int, void *);
static char di_rows[LJ_DI_CH][200];
static const char *di_items[LJ_DI_CH];
static char di_tr_rows[LJ_DI_CH][80];
static const char *di_tr_items[LJ_DI_CH];
static char di_cells[LJ_DI_VALUES][2][4];

/* W5: the working copy both windows edit, the repository list, the edited table, the export folder */
static lj_dmxin_map di_work;
static lj_vtr di_tables[LJ_DI_TABLES]; static int di_ntables;
static char di_tree_rows[LJ_DI_TABLES + 16][100]; static const char *di_tree_items[LJ_DI_TABLES + 16]; static int di_tree_table[LJ_DI_TABLES + 16];
static const char *di_fn_items[256];
static lj_vtr di_edit;
static uint64_t di_last_us;
static char di_err_text[16][160]; static const char *di_err_lines[16]; static int di_nerr;
static struct { char dir[LJ_PATH_CAP]; char row[LJ_DI_DIRS][LJ_DIRENT_CAP + 8]; const char *items[LJ_DI_DIRS]; char sub[LJ_DI_DIRS][LJ_DIRENT_CAP]; int nsub; lj_widget *list; } di_ex;

static int di_index(enum lj_win id) { int i = (int)id - (int)LJ_WIN_DMX_IN; return i >= 0 && i < LJ_DI_N ? i : -1; }
static int di_of(lj_widget *w) { lj_window *win = lj_window_of(w); int i; for (i = 0; i < LJ_DI_N; i++) if (win && di_win[i] == win) return i; return -1; }

/* ---- greying (shared with lj_hardware_setup.c) ---- */
static int is_input(int kind)
{
    switch (kind) {
    case LJ_K_EDIT: case LJ_K_BITBTN: case LJ_K_SPEEDBUTTON: case LJ_K_CHECKBOX: case LJ_K_LISTVIEW: case LJ_K_UPDOWN: case LJ_K_BUTTON:
    case LJ_K_RADIOGROUP: case LJ_K_RICHEDIT: case LJ_K_COMBOBOX: case LJ_K_UDBUTTON: case LJ_K_SPINEDIT: case LJ_K_RADIOBUTTON:
    case LJ_K_STRINGGRID: case LJ_K_NUMEDIT: case LJ_K_MEMO: case LJ_K_LISTBOX: case LJ_K_MASKEDIT: case LJ_K_DRAWGRID: case LJ_K_CHECKLISTBOX:
    case LJ_K_TREEVIEW: case LJ_K_TRACKBAR: case LJ_K_DRIVECOMBOBOX: case LJ_K_DIRECTORYLISTBOX: case LJ_K_FILELISTBOX: return 1;
    default: return 0;
    }
}
void lj_dmx_in_grey(lj_widget *root, const char *const *keep, int nkeep)
{
    lj_widget *c;
    int k, kept = 0;
    if (!root) return;
    for (k = 0; k < nkeep && root->name; k++) kept |= strcmp(root->name, keep[k]) == 0;
    if (!kept && is_input(root->kind)) {
        root->enabled = 0;
        if (!root->hint && !(root->desc && root->desc->hint)) root->hint = LJ_DMX_IN_NA;   /* a DFM hint stays, shown or not (caption diff, F55) */
        lj_widget_invalidate(root);
    }
    for (c = root->first; c; c = c->next) lj_dmx_in_grey(c, keep, nkeep);
}

/* ---- the repository: SYSTEM tables, then User/ValueTables/*.vtr (a file that does not parse raises the error form) ---- */
static int di_tables_dir(char *dst, size_t cap) { return snprintf(dst, cap, "%sUser/ValueTables", lj_ctx_get()->userdir) < (int)cap ? 0 : -1; }

static void di_load_tables(void)
{
    static char names[LJ_DI_TABLES][LJ_DIRENT_CAP];
    char dir[LJ_PATH_CAP], path[LJ_PATH_CAP], err[120];
    int i, n;
    di_ntables = 0; di_nerr = 0;
    for (i = 0; i < LJ_VTR_STOCK && di_ntables < LJ_DI_TABLES; i++) lj_vtr_stock(i, &di_tables[di_ntables++]);
    if (di_tables_dir(dir, sizeof dir) != 0 || (n = lj_library_dir_list(dir, ".vtr", names, LJ_DI_TABLES - di_ntables)) < 0) return;
    for (i = 0; i < n; i++) {
        if (snprintf(path, sizeof path, "%s/%s", dir, names[i]) >= (int)sizeof path) continue;
        if (lj_vtr_load(path, &di_tables[di_ntables], err, sizeof err) == 0) { di_ntables++; continue; }
        if (di_nerr < 16) { snprintf(di_err_text[di_nerr], sizeof di_err_text[di_nerr], "%.80s: %.70s", names[i], err); di_err_lines[di_nerr] = di_err_text[di_nerr]; di_nerr++; }
    }
    if (di_nerr) {                                                         /* TValueTableImportErrorForm from its trigger (W5-DMXIN-6) */
        lj_window *e = lj_dmx_in_open(LJ_WIN_VALUE_TABLE_ERROR);
        lj_widget *re = e ? lj_window_find(e, "re1") : NULL;
        if (re) lj_w_memo_set_lines(re, di_err_lines, di_nerr);
    }
}
static int di_table_by_id(const char *id) { int i; for (i = 0; i < di_ntables; i++) if (strcmp(di_tables[i].id, id) == 0) return i; return -1; }

/* ---- TDmxInForm ---- */
static void di_work_from_saved(void) { lj_ctx *c = lj_ctx_get(); if (c->dmxin_map) di_work = *c->dmxin_map; else lj_dmxin_map_init(&di_work); }

static void di_fill_rows(void)
{
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    lj_widget *w;
    char fn[64];
    int k;
    if (!di_win[0]) return;
    for (k = 0; k < LJ_DI_CH; k++) {
        const lj_dmxin_assign *a = &di_work.ch[k];
        lj_dmxin_fn_caption(a, fn, sizeof fn);
        snprintf(di_rows[k], sizeof di_rows[k], "%03d\t%03d\t%s\t%s", k + 1, f->out[k], a->has_table ? a->table.name : "", fn);   /* dmxindialog.gif: "001" "255" "Off/On" "HTP Group 1" */
        di_items[k] = di_rows[k];
    }
    if ((w = lj_window_find(di_win[0], "LVDmxFunctions")) != NULL) { int keep = lj_w_listview_index(w); lj_w_listview_set_items(w, di_items, LJ_DI_CH); ((lj_w_listview *)w)->index = keep; lj_widget_invalidate(w); }
}

static void di_monitor(void)
{
    static char ch[40], hz[40], cue[16];
    const lj_dmxin_frame *f = lj_dmxin_frame_get();
    uint32_t n;
    lj_widget *w;
    if (!di_win[0]) return;
    snprintf(ch, sizeof ch, "DMX Channels In %u", (unsigned)f->channels);   /* dmxindialog.gif: "DMX Channels In 512" */
    snprintf(hz, sizeof hz, "Update Freq %u Hz", (unsigned)f->hz);          /* "Update Freq 31 Hz" */
    lj_window_set_text(di_win[0], "lChIn", ch);
    lj_window_set_text(di_win[0], "lFreq", hz);
    if (lj_dmxin_remote_cue(&n)) snprintf(cue, sizeof cue, "%u", (unsigned)n); else lj_str_copy(cue, sizeof cue, "N/A");
    if ((w = lj_window_find(di_win[0], "eCue")) != NULL && strcmp(lj_w_edit_text(w), cue) != 0) lj_w_edit_set_text(w, cue);
}

static int di_count_used(void) { int k, n = 0; for (k = 0; k < LJ_DI_CH; k++) n += di_work.ch[k].fn != LJ_DIF_NONE; return n; }
static void di_used(void) { static char t[16]; snprintf(t, sizeof t, "%d", di_count_used()); if (di_win[0]) lj_window_set_text(di_win[0], "lUsed", t); }
static void di_refresh_all(void) { di_fill_rows(); di_used(); di_monitor(); }
static int di_sel_ch(void) { lj_widget *w = di_win[0] ? lj_window_find(di_win[0], "LVDmxFunctions") : NULL; return w ? lj_w_listview_index(w) : -1; }

static void on_assign(lj_widget *w, void *ud)
{
    lj_widget *lv = di_win[0] ? lj_window_find(di_win[0], "LVFunctions") : NULL;
    int ch = di_sel_ch(), f = lv ? lj_w_listview_index(lv) : -1;
    (void)w; (void)ud;
    if (ch < 0 || ch >= LJ_DI_CH || f < 0 || f >= lj_dmxin_fn_nrows) return;
    di_work.ch[ch].fn = lj_dmxin_fn_rows[f].fn; di_work.ch[ch].num = lj_dmxin_fn_rows[f].num;
    di_refresh_all();
}
static void on_clear(lj_widget *w, void *ud) { int ch = di_sel_ch(); (void)w; (void)ud; if (ch >= 0 && ch < LJ_DI_CH) { di_work.ch[ch].fn = LJ_DIF_NONE; di_work.ch[ch].num = 0; di_refresh_all(); } }

int lj_dmx_in_save(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[120];
    if (!c->lib || lj_library_path(c->lib, "DmxIn", path, sizeof path) != 0 || lj_fs_mkdir(path) != 0) { lj_log("dmx in: no library folder for DmxIn"); return -1; }
    if (lj_library_path(c->lib, "DmxIn/DmxIn.dat", path, sizeof path) != 0 || lj_dmxin_map_save(path, &di_work) != 0) return -1;
    if (c->dmxin_map && lj_dmxin_map_load(path, c->dmxin_map, err, sizeof err) != 0) { lj_log("dmx in: %s: %s", path, err); return -1; }   /* the saved map is the live one, tables installed */
    lj_dmxin_functions_reset();
    lj_log("dmx in: saved %s (%d assignments)", path, di_count_used());
    return 0;
}
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_dmx_in_save() != 0) lj_win_message("Dmx In", "The DMX in configuration could not be saved"); }

static void di_menu_state(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_menu *m = di_win[0] ? lj_window_menus(di_win[0]) : NULL;
    if (!m) return;
    lj_menu_set_checked(m, "M_DisableDMXinFunc", c->dmxin && c->dmxin->functions_off);
    lj_menu_set_checked(m, "ClearCueonCue01", di_work.cue0_clears);
}
static void on_options(lj_widget *w, void *ud) { (void)w; (void)ud; di_menu_state(); }
static void on_disable(lj_widget *w, void *ud)   /* Options > Disable DMX-in functions: the Global Patch overrides keep working (05 s3.1) */
{
    lj_ctx *c = lj_ctx_get();
    (void)w; (void)ud;
    if (!c->dmxin) return;
    c->dmxin->functions_off = (uint8_t)!c->dmxin->functions_off;
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_DMXIN);
    di_menu_state();
}
static void on_clear_all(lj_widget *w, void *ud) { int k; (void)w; (void)ud; for (k = 0; k < LJ_DI_CH; k++) { di_work.ch[k].fn = LJ_DIF_NONE; di_work.ch[k].num = 0; } di_refresh_all(); }
static void on_clear_latch(lj_widget *w, void *ud)
{
    int k;
    (void)w; (void)ud;
    for (k = 0; k < LJ_DI_CH; k++) { uint8_t f = di_work.ch[k].fn; if ((f >= LJ_DIF_CUE_BIT_1 && f <= LJ_DIF_CUE_BIT_512) || f == LJ_DIF_LATCH_CUE) di_work.ch[k].fn = LJ_DIF_NONE; }
    di_refresh_all();
}
static void on_prefs(lj_widget *w, void *ud) { (void)w; (void)ud; di_work.cue0_clears = (uint8_t)!di_work.cue0_clears; di_menu_state(); }
static void on_di_pop(lj_widget *w, void *ud)   /* FunctionListPop: Clear Assigned, Assign Current Sequence (Flash) / Cue / Cuelist (Latch) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    const lj_show *s = lj_ctx_get()->show;
    int ch = di_sel_ch();
    uint8_t fn = LJ_DIF_NONE; uint32_t num = 0;
    (void)ud;
    if (!it || !it->name || ch < 0 || ch >= LJ_DI_CH) return;
    if (strcmp(it->name, "Pop_Clear") == 0) { on_clear(w, ud); return; }
    if (strcmp(it->name, "Pop_Seq") == 0 && s && s->current_seq) { fn = LJ_DIF_FLASH_SEQ; num = s->current_seq->number; }
    else if (strcmp(it->name, "Pop_Cue") == 0 && s && s->current_cue) { fn = LJ_DIF_LATCH_CUE_N; num = s->current_cue->number; }
    else if (strcmp(it->name, "Pop_Cuelist") == 0 && s && s->current_cl) { fn = LJ_DIF_LATCH_CUELIST_N; num = s->current_cl->number; }
    if (fn == LJ_DIF_NONE || num == 0) { lj_log("dmx in: %s: nothing current to assign", it->name); return; }   /* Pop_BGCue: no current BG cue in the show record */
    di_work.ch[ch].fn = fn; di_work.ch[ch].num = num;
    di_refresh_all();
}
static void on_di_timer(lj_widget *w, void *ud) { (void)w; (void)ud; di_refresh_all(); }

/* ---- TDMXInValueTranslationForm ---- */
static int di_tr_sel(void) { lj_widget *w = di_win[1] ? lj_window_find(di_win[1], "lvChannels") : NULL; return w ? lj_w_listview_index(w) : -1; }
static int di_tree_sel(void) { lj_widget *w = di_win[1] ? lj_window_find(di_win[1], "tv1") : NULL; int i = w ? ((lj_w_treeview *)w)->index : -1; return i >= 0 && i < LJ_DI_TABLES + 16 ? di_tree_table[i] : -1; }

static void di_tr_fill(void)
{
    static char active[48];
    lj_window *win = di_win[1];
    lj_widget *w;
    const char *type = NULL;
    int k, n = 0, rows = 0;
    if (!win) return;
    for (k = 0; k < LJ_DI_CH; k++) {
        snprintf(di_tr_rows[k], sizeof di_tr_rows[k], "%d\t%s", k + 1, di_work.ch[k].has_table ? di_work.ch[k].table.name : "");   /* dmxintranslation.gif */
        di_tr_items[k] = di_tr_rows[k];
        n += di_work.ch[k].has_table;
    }
    if ((w = lj_window_find(win, "lvChannels")) != NULL) { int keep = lj_w_listview_index(w); lj_w_listview_set_items(w, di_tr_items, LJ_DI_CH); ((lj_w_listview *)w)->index = keep; }
    snprintf(active, sizeof active, "%d active tranlation(s)", n);   /* exe 0x21D0A8, sic (D13) */
    lj_window_set_text(win, "lActive", active);
    for (k = 0; k < di_ntables && rows < LJ_DI_TABLES + 15; k++) {   /* a node per TYPE, its tables below */
        if (!type || strcmp(type, di_tables[k].type) != 0) { type = di_tables[k].type; lj_str_copy(di_tree_rows[rows], sizeof di_tree_rows[rows], type); di_tree_table[rows] = -1; di_tree_items[rows] = di_tree_rows[rows]; rows++; }
        snprintf(di_tree_rows[rows], sizeof di_tree_rows[rows], "\t%s", di_tables[k].name); di_tree_table[rows] = k; di_tree_items[rows] = di_tree_rows[rows]; rows++;
    }
    if ((w = lj_window_find(win, "tv1")) != NULL) { int keep = ((lj_w_treeview *)w)->index; lj_w_treeview_set_items(w, di_tree_items, rows); for (k = 0; k < rows; k++) if (di_tree_table[k] < 0) lj_w_treeview_expand(w, k, 1); ((lj_w_treeview *)w)->index = keep < rows ? keep : -1; }
}
static void di_tr_note(void)   /* re1: the PURPOSE of the selected channel's table, or of the selected repository table */
{
    static const char *line[1];
    int ch = di_tr_sel(), t = di_tree_sel();
    lj_widget *re = di_win[1] ? lj_window_find(di_win[1], "re1") : NULL;
    line[0] = ch >= 0 && ch < LJ_DI_CH && di_work.ch[ch].has_table ? di_work.ch[ch].table.purpose : t >= 0 ? di_tables[t].purpose : "";
    if (re) lj_w_memo_set_lines(re, line, 1);
    if (di_win[1]) {
        lj_widget *pb = lj_window_find(di_win[1], "PB1"), *up = lj_window_find(di_win[1], "sbUp"), *dn = lj_window_find(di_win[1], "sbDown");
        if (pb) lj_widget_invalidate(pb);
        if (up) { up->enabled = ch > 0; lj_widget_invalidate(up); }                    /* DFM Enabled False: live once a channel is selected */
        if (dn) { dn->enabled = ch >= 0 && ch < LJ_DI_CH - 1; lj_widget_invalidate(dn); }
    }
}
static void on_tr_change(lj_widget *w, void *ud) { (void)w; (void)ud; di_tr_note(); }
static void di_assign_table(int ch, int t)
{
    if (ch < 0 || ch >= LJ_DI_CH || t < 0 || t >= di_ntables) return;
    di_work.ch[ch].has_table = 1; di_work.ch[ch].table = di_tables[t];   /* copied, not referenced (05 s3.3) */
    di_tr_fill(); di_tr_note();
}
static void on_tr_drop(lj_widget *w, void *ud) { (void)w; (void)ud; di_assign_table(di_tr_sel(), di_tree_sel()); }
static int di_update_translations(int only_ch)   /* "Update translations": re-copy each assigned table from the repository by ID */
{
    int k, n = 0, t;
    for (k = 0; k < LJ_DI_CH; k++) {
        if (!di_work.ch[k].has_table || (only_ch >= 0 && k != only_ch) || (t = di_table_by_id(di_work.ch[k].table.id)) < 0) continue;
        di_work.ch[k].table = di_tables[t]; n++;
    }
    di_tr_fill(); di_tr_note();
    return n;
}
static void di_open_editor(int t)
{
    if (t >= 0 && t < di_ntables) di_edit = di_tables[t];
    else {   /* a new table: 1:1, type USER, a fresh GUID from the sACN UUID generator's bytes */
        uint8_t u[16]; uint64_t c = lj_clock_us();
        int i;
        lj_vtr_identity(&di_edit);
        for (i = 0; i < 16; i++) { c = c * 6364136223846793005ull + 1442695040888963407ull; u[i] = (uint8_t)(c >> 56); }
        u[6] = (uint8_t)(0x40 | (u[6] & 0x0F)); u[8] = (uint8_t)(0x80 | (u[8] & 0x3F));
        snprintf(di_edit.id, sizeof di_edit.id, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
        lj_str_copy(di_edit.type, sizeof di_edit.type, "USER"); lj_str_copy(di_edit.name, sizeof di_edit.name, "New table");
    }
    lj_dmx_in_close(LJ_WIN_TRANSLATION_EDIT);
    lj_dmx_in_open(LJ_WIN_TRANSLATION_EDIT);
}
static void on_tr_menu(lj_widget *w, void *ud)   /* Menu_Click: Update all translations, Remove all translations, Options (-> Export) */
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    int k;
    (void)ud;
    if (!it || !it->name) return;
    if (strcmp(it->name, "M_UpdateActiveTranslations") == 0) di_update_translations(-1);
    else if (strcmp(it->name, "M_ClearAll") == 0) { for (k = 0; k < LJ_DI_CH; k++) di_work.ch[k].has_table = 0; di_tr_fill(); di_tr_note(); }
    else if (strcmp(it->name, "M_Options") == 0) lj_dmx_in_open(LJ_WIN_EXPORT_TABLE);
}
static void on_tr_pop(lj_widget *w, void *ud)   /* Pop_Click: OnPopup of both popups and OnClick of their items */
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    int ch = di_tr_sel(), t = di_tree_sel();
    char dir[LJ_PATH_CAP], name[LJ_DIRENT_CAP], path[LJ_PATH_CAP];
    (void)ud;
    if (!it || !it->name) return;
    if (strcmp(it->name, "pop_NewTable") == 0) di_open_editor(-1);
    else if (strcmp(it->name, "pop_EditRepositoryTable") == 0) di_open_editor(t);
    else if (strcmp(it->name, "pop_CopyRepositoryTable") == 0 && t >= 0) { lj_vtr keep = di_tables[t]; di_open_editor(-1); { char id[40]; lj_str_copy(id, sizeof id, di_edit.id); di_edit = keep; lj_str_copy(di_edit.id, sizeof di_edit.id, id); lj_str_copy(di_edit.type, sizeof di_edit.type, "USER"); } lj_dmx_in_close(LJ_WIN_TRANSLATION_EDIT); lj_dmx_in_open(LJ_WIN_TRANSLATION_EDIT); }
    else if (strcmp(it->name, "pop_CopyToNewTable") == 0 && ch >= 0 && ch < LJ_DI_CH && di_work.ch[ch].has_table) { lj_vtr keep = di_work.ch[ch].table; di_open_editor(-1); { char id[40]; lj_str_copy(id, sizeof id, di_edit.id); di_edit = keep; lj_str_copy(di_edit.id, sizeof di_edit.id, id); lj_str_copy(di_edit.type, sizeof di_edit.type, "USER"); } lj_dmx_in_close(LJ_WIN_TRANSLATION_EDIT); lj_dmx_in_open(LJ_WIN_TRANSLATION_EDIT); }
    else if (strcmp(it->name, "Pop_DeleteRepositoryTable") == 0 && t >= LJ_VTR_STOCK) {   /* the SYSTEM tables are regenerated, never deleted */
        if (di_tables_dir(dir, sizeof dir) == 0 && lj_vtr_file_name(name, sizeof name, &di_tables[t]) == 0 && snprintf(path, sizeof path, "%s/%s", dir, name) < (int)sizeof path) remove(path);
        di_load_tables(); di_tr_fill();
    }
    else if (strcmp(it->name, "Pop_AssignSelected") == 0) di_assign_table(ch, t);
    else if (strcmp(it->name, "Pop_UpdateActive") == 0) di_update_translations(ch);
    else if (strcmp(it->name, "pop_LocateTable") == 0 && ch >= 0 && ch < LJ_DI_CH && di_work.ch[ch].has_table) {
        int r, k = di_table_by_id(di_work.ch[ch].table.id); lj_widget *tv = lj_window_find(di_win[1], "tv1");
        for (r = 0; tv && k >= 0 && r < LJ_DI_TABLES + 16; r++) if (di_tree_table[r] == k) { ((lj_w_treeview *)tv)->index = r; lj_widget_invalidate(tv); break; }
    }
    else if (strcmp(it->name, "Pop_ClearTranslation") == 0 && ch >= 0 && ch < LJ_DI_CH) { di_work.ch[ch].has_table = 0; di_tr_fill(); di_tr_note(); }
    di_refresh_all();
}
static void on_tr_okcancel(lj_widget *w, void *ud)   /* Ok keeps the working translations (the Dmx In Save writes them); Cancel restores the saved ones */
{
    lj_window *win = lj_window_of(w);
    lj_widget *cancel = win ? lj_window_find(win, "bCancel") : NULL;
    lj_ctx *c = lj_ctx_get();
    int k;
    (void)ud;
    if (w == cancel && c->dmxin_map) for (k = 0; k < LJ_DI_CH; k++) { di_work.ch[k].has_table = c->dmxin_map->ch[k].has_table; di_work.ch[k].table = c->dmxin_map->ch[k].table; }
    lj_dmx_in_close(LJ_WIN_DMXIN_TRANSLATION);
    di_refresh_all();
}
static void on_tr_updown(lj_widget *w, void *ud)   /* sbUp / sbDown: move the channel selection */
{
    lj_widget *lv = di_win[1] ? lj_window_find(di_win[1], "lvChannels") : NULL, *up = di_win[1] ? lj_window_find(di_win[1], "sbUp") : NULL;
    int i = lv ? lj_w_listview_index(lv) : -1;
    (void)ud;
    if (!lv) return;
    i += w == up ? -1 : 1;
    if (i >= 0 && i < LJ_DI_CH) lj_w_listview_select(lv, i);
    di_tr_note();
}

/* the graph: the edited table in the editor, the selected channel's table in the translation window */
static void di_graph(lj_widget *w, const uint8_t *map)
{
    lj_window *win = lj_window_of(w);
    lj_surface *s = win ? lj_window_surface(win) : NULL;
    int x, y, v, l = 12, b, gw, gh;
    if (!s || w->w < 40 || w->h < 40) return;
    lj_widget_abs(w, &x, &y);
    b = w->h - 12; gw = w->w - l - 6; gh = b - 6;
    lj_fill(s, x, y, w->w, w->h, 0x0000FFu);                          /* Color = clBlue [DFM]; geometry PROPOSAL rig (dmxintranslationedit.gif is reduced) */
    lj_vline(s, x + l, y + 4, b - 2, 0x00FFFFu); lj_hline(s, x + 4, y + b, w->w - 8, 0x00FFFFu);
    for (v = 0; v < LJ_DI_VALUES; v++) lj_fill(s, x + l + 1 + v * gw / 255, y + b - 1 - (map ? map[v] : v) * gh / 255, 2, 2, 0xFFFFFFu);
    if (w->font) lj_text(s, w->font, x + w->w - 6 - lj_text_width(w->font, "DMX Value"), y + b - 14, "DMX Value", 0xFFFF00u, 0);   /* exe 0x21B194 */
}
static void on_graph(lj_widget *w, void *ud) { (void)ud; di_graph(w, di_edit.map); }
static void on_tr_graph(lj_widget *w, void *ud) { int ch = di_tr_sel(); (void)ud; di_graph(w, ch >= 0 && ch < LJ_DI_CH && di_work.ch[ch].has_table ? di_work.ch[ch].table.map : NULL); }

/* ---- TTranslationEditForm ---- */
static void di_ed_fill(void)
{
    lj_window *win = di_win[2];
    lj_widget *w;
    int k;
    if (!win) return;
    if ((w = lj_window_find(win, "sg1")) != NULL) {                    /* missing entries = 1:1 [05 s3.3] */
        lj_w_grid_set_cell(w, 0, 0, "DMX value"); lj_w_grid_set_cell(w, 1, 0, "Translation");   /* exe 0x21AD20, 0x21AD34 */
        for (k = 0; k < LJ_DI_VALUES; k++) {
            snprintf(di_cells[k][0], sizeof di_cells[k][0], "%d", k); snprintf(di_cells[k][1], sizeof di_cells[k][1], "%d", di_edit.map[k]);
            lj_w_grid_set_cell(w, 0, k + 1, di_cells[k][0]); lj_w_grid_set_cell(w, 1, k + 1, di_cells[k][1]);
        }
        lj_widget_invalidate(w);
    }
    if ((w = lj_window_find(win, "eID")) != NULL) lj_w_edit_set_text(w, di_edit.id);
    if ((w = lj_window_find(win, "eType")) != NULL) lj_w_edit_set_text(w, di_edit.type);
    if ((w = lj_window_find(win, "eName")) != NULL) lj_w_edit_set_text(w, di_edit.name);
    if ((w = lj_window_find(win, "ePurpose")) != NULL) lj_w_edit_set_text(w, di_edit.purpose);
    if ((w = lj_window_find(win, "PB1")) != NULL) lj_widget_invalidate(w);
}
static int di_ed_num(const char *name, int *v)
{
    lj_widget *w = di_win[2] ? lj_window_find(di_win[2], name) : NULL;
    const char *t = w ? lj_w_edit_text(w) : NULL;
    int n = 0, k = 0;
    if (!t || !*t) return -1;
    for (; *t; t++, k++) { if (*t < '0' || *t > '9' || k > 3) return -1; n = n * 10 + (*t - '0'); }
    if (n > 255) return -1;
    *v = n;
    return 0;
}
int lj_dmx_in_edit_linear(int vf, int vt, int tf, int tt) { if (vf < 0 || vf > 255 || vt < 0 || vt > 255 || tf < 0 || tf > 255 || tt < 0 || tt > 255) return -1; lj_vtr_linear(&di_edit, vf, vt, tf, tt); di_ed_fill(); return 0; }
static void on_set(lj_widget *w, void *ud)   /* Set Linear Range: Value eVF to eVT, Translation eTF to eTT */
{
    int vf, vt, tf, tt;
    (void)w; (void)ud;
    if (di_ed_num("eVF", &vf) || di_ed_num("eVT", &vt) || di_ed_num("eTF", &tf) || di_ed_num("eTT", &tt)) { lj_win_message("Edit tranlation table", "Values must be 0..255"); return; }
    lj_dmx_in_edit_linear(vf, vt, tf, tt);
}
int lj_dmx_in_edit_save(void)
{
    lj_widget *w;
    char dir[LJ_PATH_CAP], name[LJ_DIRENT_CAP], path[LJ_PATH_CAP];
    if (di_win[2]) {
        if ((w = lj_window_find(di_win[2], "eType")) != NULL) lj_str_copy(di_edit.type, sizeof di_edit.type, lj_w_edit_text(w));
        if ((w = lj_window_find(di_win[2], "eName")) != NULL) lj_str_copy(di_edit.name, sizeof di_edit.name, lj_w_edit_text(w));
        if ((w = lj_window_find(di_win[2], "ePurpose")) != NULL) lj_str_copy(di_edit.purpose, sizeof di_edit.purpose, lj_w_edit_text(w));
    }
    if (strcmp(di_edit.type, "SYSTEM") == 0) { lj_win_message("Edit tranlation table", "SYSTEM tables cannot be changed; copy the table first"); return -1; }
    if (lj_vtr_file_name(name, sizeof name, &di_edit) != 0) { lj_win_message("Edit tranlation table", "Type and Name must be set and hold no : * ? \" < > |"); return -1; }
    if (snprintf(dir, sizeof dir, "%sUser", lj_ctx_get()->userdir) >= (int)sizeof dir || lj_fs_mkdir(dir) != 0 || di_tables_dir(dir, sizeof dir) != 0 || lj_fs_mkdir(dir) != 0) return -1;
    if (snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path || lj_vtr_save(path, &di_edit) != 0) return -1;
    di_load_tables(); di_tr_fill();
    return 0;
}
static void on_ed_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_dmx_in_edit_save() == 0) lj_dmx_in_close(LJ_WIN_TRANSLATION_EDIT); }

/* ---- TExportTableForm: a code-built folder list in place of the two stubs ---- */
static void di_ex_fill(void)
{
    int i, n;
    lj_str_copy(di_ex.row[0], sizeof di_ex.row[0], "..");
    di_ex.items[0] = di_ex.row[0];
    n = lj_library_dir_list(di_ex.dir, NULL, di_ex.sub, LJ_DI_DIRS - 1);
    di_ex.nsub = n < 0 ? 0 : n;
    for (i = 0; i < di_ex.nsub; i++) { snprintf(di_ex.row[i + 1], sizeof di_ex.row[i + 1], "  %s", di_ex.sub[i]); di_ex.items[i + 1] = di_ex.row[i + 1]; }
    if (di_ex.list) lj_w_list_set_items(di_ex.list, di_ex.items, di_ex.nsub + 1);
    if (di_win[4]) lj_window_set_text(di_win[4], "pTarget", di_ex.dir);
}
int lj_dmx_in_export_dir(const char *dir) { if (!dir || !*dir || lj_str_copy(di_ex.dir, sizeof di_ex.dir, dir) != 0) return -1; di_ex_fill(); return 0; }
static void on_ex_pick(lj_widget *w, int ev, void *ud)
{
    int i = ((lj_w_list *)w)->index;
    char d[LJ_PATH_CAP];
    size_t len;
    (void)ud;
    if (ev != LJ_WEV_DBLCLICK && ev != LJ_WEV_CLICK) return;
    if (i == 0) {                                              /* "..": up one folder, a root ("/", "D:/") stays */
        len = strlen(di_ex.dir);
        while (len > 1 && (di_ex.dir[len - 1] == '/' || di_ex.dir[len - 1] == '\\')) len--;
        while (len > 0 && di_ex.dir[len - 1] != '/' && di_ex.dir[len - 1] != '\\') len--;
        if (len == 0) return;
        if (len > 1 && !(len == 3 && di_ex.dir[1] == ':')) len--;
        di_ex.dir[len] = 0; di_ex_fill(); return;
    }
    if (i > 0 && i <= di_ex.nsub && snprintf(d, sizeof d, "%s/%s", di_ex.dir, di_ex.sub[i - 1]) < (int)sizeof d) lj_dmx_in_export_dir(d);
}
int lj_dmx_in_export(void)   /* every user table (not the SYSTEM ones, regenerated anywhere) into the chosen folder */
{
    lj_widget *ow = di_win[4] ? lj_window_find(di_win[4], "cbOverwrite") : NULL;
    char name[LJ_DIRENT_CAP], path[LJ_PATH_CAP];
    int i, n = 0, over = ow ? lj_w_checkbox_checked(ow) : 0;
    if (!di_ex.dir[0]) return -1;
    for (i = LJ_VTR_STOCK; i < di_ntables; i++) {
        if (lj_vtr_file_name(name, sizeof name, &di_tables[i]) != 0 || snprintf(path, sizeof path, "%s/%s", di_ex.dir, name) >= (int)sizeof path) continue;
        if (!over && lj_fs_exists(path)) continue;
        if (lj_vtr_save(path, &di_tables[i]) == 0) n++;
    }
    lj_log("dmx in: exported %d table(s) to %s", n, di_ex.dir);
    return n;
}
static void on_export(lj_widget *w, void *ud) { (void)w; (void)ud; lj_dmx_in_export(); }

/* ---- shared handlers ---- */
static void close_self(lj_widget *w, void *ud) { int i = di_of(w); (void)ud; if (i >= 0) lj_dmx_in_close((enum lj_win)(LJ_WIN_DMX_IN + i)); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }
static void on_translations(lj_widget *w, void *ud) { (void)w; (void)ud; lj_dmx_in_open(LJ_WIN_DMXIN_TRANSLATION); }

static const lj_binding di_dmxin_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "FormShow", lj_handler_noop, NULL }, { "Timer1Timer", on_di_timer, NULL }, { "Timer2Timer", lj_handler_noop, NULL }, { "FunctionListPopPopup", lj_handler_noop, NULL },
    { "M_OptionsClick", on_options, NULL }, { "LVDmxFunctionsDragOver", lj_handler_noop, NULL }, { "LVDmxFunctionsDragDrop", on_assign, NULL },
    { "bClearClick", on_clear, NULL }, { "bAssignClick", on_assign, NULL }, { "bSaveClick", on_save, NULL }, { "bcancelClick", close_self, NULL },
    { "M_DisableDMXinClick", on_disable, NULL }, { "ClearAllAssignments1Click", on_clear_all, NULL }, { "ClearCueLacthassignments1Click", on_clear_latch, NULL }, { "PrefsClick", on_prefs, NULL },
    { "Pop_Click", on_di_pop, NULL }, { "M_ValueTranslationsClick", on_translations, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_tr_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "Panel2Resize", lj_handler_noop, NULL }, { "lvChannelsChange", on_tr_change, NULL }, { "lvChannelsMouseMove", lj_handler_noop, NULL }, { "UpdateClick", on_tr_change, NULL },
    { "tv1Change", on_tr_change, NULL }, { "PB1Paint", on_tr_graph, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "DoDragOver", lj_handler_noop, NULL },
    { "DoDragDrop", on_tr_drop, NULL }, { "sbUpClick", on_tr_updown, NULL }, { "Menu_Click", on_tr_menu, NULL }, { "Pop_Click", on_tr_pop, NULL },
    { "bOkCancelClick", on_tr_okcancel, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_edit_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL },
    { "sg1Exit", lj_handler_noop, NULL }, { "sg1SelectCell", lj_handler_noop, NULL }, { "eExit", lj_handler_noop, NULL }, { "eKeyPress", lj_handler_noop, NULL },
    { "PB1Paint", on_graph, NULL }, { "bSetClick", on_set, NULL }, { "bOkClick", on_ed_ok, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_none_b[] = { { NULL, NULL, NULL } };
static const lj_binding di_export_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "DirectoryListBoxChange", lj_handler_noop, NULL }, { "bExportClick", on_export, NULL },
    { NULL, NULL, NULL } };

static const struct {
    const lj_form_desc *form; const lj_binding *b;
    const char *close_btn[2];                       /* buttons with no OnClick in the DFM (Kind / ModalResult) that close */
} di_forms[LJ_DI_N] = {
    { &lj_form_TDMXINFORM, di_dmxin_b, { NULL } },
    { &lj_form_TDMXINVALUETRANSLATIONFORM, di_tr_b, { NULL } },
    { &lj_form_TTRANSLATIONEDITFORM, di_edit_b, { "BitBtn4" } },
    { &lj_form_TVALUETABLEIMPORTERRORFORM, di_none_b, { "bOk" } },
    { &lj_form_TEXPORTTABLEFORM, di_export_b, { "BitBtn2" } },
};

static void on_close_btn(lj_widget *w, int ev, void *ud) { if (ev == LJ_WEV_CLICK) close_self(w, ud); }
static void on_root(lj_widget *w, int ev, void *ud)   /* the frame's close box closes every one of the five (three have no OnClose) */
{
    int i = di_of(w);
    if (i >= 0 && di_root_orig[i]) di_root_orig[i](w, ev, ud);
    if (ev == LJ_WEV_CLOSE) close_self(w, NULL);
}

static void fill(int i)
{
    lj_window *win = di_win[i];
    lj_widget *w;
    int k;
    if (i == 0) {
        for (k = 0; k < lj_dmxin_fn_nrows && k < 256; k++) di_fn_items[k] = lj_dmxin_fn_rows[k].caption;
        if ((w = lj_window_find(win, "LVFunctions")) != NULL) lj_w_listview_set_items(w, di_fn_items, k);
        if ((w = lj_window_find(win, "Timer1")) != NULL) w->enabled = 1;   /* the monitor's 100 ms refresh (DFM Interval 100, enabled at run time) */
        di_refresh_all(); di_menu_state();
    }
    if (i == 1) {
        if ((w = lj_window_find(win, "pButtons")) != NULL && w->parent) w->color = w->parent->color;   /* DFM clRed (a design-time marker): dmxintranslation.gif shows face, so LJ2 recolours it (PROPOSAL rig) */
        lj_menu_set_enabled(lj_window_menus(win), "M_UpdateActiveTranslations", 1); lj_menu_set_enabled(lj_window_menus(win), "M_ClearAll", 1);   /* DFM Enabled False: LJ2 enables them at run time */
        { static const char *const pops[] = { "Pop_LocateActivetranslationsFromGUID", "pop_EditRepositoryTable", "Pop_UpdateActive", "pop_CopyRepositoryTable", "Pop_DeleteRepositoryTable", "Pop_AssignSelected", "pop_LocateTable", "pop_CopyToNewTable" };
          for (k = 0; k < (int)(sizeof pops / sizeof pops[0]); k++) lj_menu_set_enabled(lj_window_menus(win), pops[k], 1); }
        di_tr_fill(); di_tr_note();
    }
    if (i == 2) {
        for (k = 0; k < 2; k++) if ((w = lj_window_find(win, k ? "BitBtn3" : "BitBtn1")) != NULL) w->visible = 0;   /* Save / Load: Top 264 in the 231-px Panel1, clipped away in the VCL (dmxintranslationedit.gif shows neither) */
        if ((w = lj_window_find(win, "eID")) != NULL) w->enabled = 0;                                             /* the GUID is generated, never typed */
        di_ed_fill();
    }
    if (i == 4) {
        lj_widget *dl = lj_window_find(win, "DirectoryListBox"), *dc = lj_window_find(win, "DriveComboBox");
        if (dc) dc->visible = 0;                                   /* ME-17: the stub drive combo hidden (paths carry the drive) */
        if (dl) {
            dl->visible = 0;
            di_ex.list = lj_w_listbox_create(dl->parent, NULL);   /* the folder list in its place (the lj_backup_restore.c pattern) */
            if (di_ex.list) { di_ex.list->x = dl->x; di_ex.list->y = dl->y; di_ex.list->w = dl->w; di_ex.list->h = dl->h; di_ex.list->on_event = on_ex_pick; di_ex.list->ud = NULL; }
        }
        if (!di_ex.dir[0]) snprintf(di_ex.dir, sizeof di_ex.dir, "%sUser", lj_ctx_get()->userdir);
        di_ex_fill();
    }
}

lj_window *lj_dmx_in_open(enum lj_win id)
{
    int i = di_index(id), k;
    lj_widget *root, *w;
    if (i < 0) return NULL;
    if (di_win[i]) { lj_win_show(di_win[i], 0); return di_win[i]; }
    if ((i == 0 || i == 1) && !di_win[0] && !di_win[1]) di_work_from_saved();   /* a fresh working copy when neither window holds one */
    if (i == 1 || i == 4) di_load_tables();
    di_win[i] = lj_window_create(di_forms[i].form, di_forms[i].b);
    if (!di_win[i]) { lj_log("dmx in: cannot create %s", di_forms[i].form->class_name); return NULL; }
    root = lj_window_root(di_win[i]);
    di_root_orig[i] = root->on_event; root->on_event = on_root;
    for (k = 0; k < 2 && di_forms[i].close_btn[k]; k++) if ((w = lj_window_find(di_win[i], di_forms[i].close_btn[k])) != NULL) { w->on_event = on_close_btn; w->ud = NULL; }
    fill(i);
    lj_win_show(di_win[i], 0);
    return di_win[i];
}

void lj_dmx_in_close(enum lj_win id)
{
    int i = di_index(id);
    if (i < 0 || !di_win[i]) return;
    lj_win_hide(di_win[i]);
    lj_window_destroy(di_win[i]);
    di_win[i] = NULL; di_root_orig[i] = NULL;
    if (i == 4) di_ex.list = NULL;
}

lj_window *lj_dmx_in_get(enum lj_win id) { int i = di_index(id); return i >= 0 ? di_win[i] : NULL; }
void lj_dmx_in_refresh(enum lj_win id)   /* the monitor follows the input at 10 Hz (the 100 ms of Timer1) */
{
    uint64_t now = lj_clock_us();
    if (id != LJ_WIN_DMX_IN || !di_win[0] || now - di_last_us < 100000u) return;
    di_last_us = now;
    di_refresh_all();
}
const lj_dmxin_map *lj_dmx_in_working(void) { return &di_work; }
lj_dmxin_map *lj_dmx_in_working_mut(void) { return &di_work; }

/* ---- the Hardware Setup row (W5-DMXIN-1): Enable DMX in + the input universe and protocol, code-built beside it ---- */
static struct { lj_window *win; lj_widget *use, *uni, *src; } di_hs;
static const char *const di_src_items[] = { "sACN", "Art-Net" };
void lj_dmx_in_hs_build(lj_window *hs)
{
    static lj_ctl_desc d[2];
    lj_ctx *c = lj_ctx_get();
    lj_widget *use = hs ? lj_window_find(hs, "cbUseDMXin") : NULL;
    memset(&di_hs, 0, sizeof di_hs);
    if (!use || !use->parent || !c->dmxin) return;
    di_hs.win = hs; di_hs.use = use;
    use->enabled = 1; use->hint = NULL;
    lj_w_checkbox_set_checked(use, c->dmxin->enabled);
    memset(d, 0, sizeof d);   /* right of the checkbox, PROPOSAL rig W5-DMXIN-1 (LJ2 picked the input card's port here) */
    d[0].kind = LJ_K_SPINEDIT; d[0].name = "seDmxInUniverse"; d[0].left = use->x + use->w + 4; d[0].top = use->y - 2; d[0].width = 64; d[0].height = 22;
    d[0].min_value = 1; d[0].max_value = LJ_SACN_UNIVERSE_MAX; d[0].max_length = 5; d[0].font_inherited = 1; d[0].hint = "DMX-in universe (sACN 1..63999, Art-Net Port-Address 1..32767)";
    di_hs.uni = lj_w_spinedit_create(use->parent, &d[0]);
    lj_w_spinedit_set_value(di_hs.uni, c->dmxin->universe);
    d[1].kind = LJ_K_COMBOBOX; d[1].name = "cbDmxInSource"; d[1].left = d[0].left + 68; d[1].top = use->y - 2; d[1].width = 72; d[1].height = 21; d[1].style = "csDropDownList";
    d[1].item_index = -1; d[1].drop_down_count = 2; d[1].item_height = 13; d[1].font_inherited = 1; d[1].hint = "DMX-in protocol";
    di_hs.src = lj_w_combobox_create(use->parent, &d[1]);
    lj_w_combobox_set_items(di_hs.src, di_src_items, 2);
    if (di_hs.src) ((lj_w_combobox *)di_hs.src)->index = c->dmxin->source == LJ_DMXIN_ARTNET ? 1 : 0;
    lj_dmx_in_hs_use_click(use, NULL);
}
const char *lj_dmx_in_hs_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_dmxin_cfg n;
    uint16_t outs[LJ_LINKS];
    const char *msg;
    int i;
    if (!di_hs.use || !c->dmxin) return NULL;
    n = *c->dmxin;
    n.enabled = (uint8_t)lj_w_checkbox_checked(di_hs.use);
    n.universe = (uint16_t)lj_w_spinedit_value(di_hs.uni);
    n.source = (uint8_t)(di_hs.src && ((lj_w_combobox *)di_hs.src)->index == 1 ? LJ_DMXIN_ARTNET : LJ_DMXIN_SACN);
    if (c->sacn) n.nic_ipv4 = c->sacn->nic_ipv4;                       /* the chosen NIC of the output page */
    for (i = 0; i < LJ_LINKS; i++) outs[i] = c->sacn && c->sacn->proto[i] == LJ_OUT_SACN ? c->sacn->universe_of_link[i] : 0;
    if (n.enabled && (msg = lj_dmxin_cfg_validate(&n, outs)) != NULL) { lj_win_message("Hardware Setup", msg); return msg; }
    if (memcmp(&n, c->dmxin, sizeof n) == 0) return NULL;
    *c->dmxin = n;
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_DMXIN);              /* saves [DMXIN], opens / closes the input */
    return NULL;
}
void lj_dmx_in_hs_closed(void) { memset(&di_hs, 0, sizeof di_hs); }
void lj_dmx_in_hs_use_click(lj_widget *w, void *ud)   /* cbUseDMXinClick: the universe and protocol follow the box; the state is applied on Ok */
{
    int on = w && lj_w_checkbox_checked(w);
    (void)ud;
    if (di_hs.uni) { di_hs.uni->enabled = (unsigned)on; lj_widget_invalidate(di_hs.uni); }
    if (di_hs.src) { di_hs.src->enabled = (unsigned)on; lj_widget_invalidate(di_hs.src); }
}

/* ---- W5 dmxin: the DMX-in level / state rows (io/lj_actions.h LJ_FN_DMXIN_*; prog = index << 8 | value), UI thread ---- */
static void di_post(uint8_t kind, uint32_t a, uint32_t b, uint32_t c) { lj_cmd m; memset(&m, 0, sizeof m); m.kind = kind; m.a = a; m.b = b; m.c = c; lj_ctx_post(&m); }
static struct { uint8_t amp, speed; } di_macro = { 100, 100 };

static void di_fn(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    uint32_t idx = prog >> 8, v = prog & 255u;
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    if (!edge) return;
    switch (fn) {
    case LJ_FN_DMXIN_HTP:          if (idx >= 1 && idx <= 24) di_post(LJ_CMD_HTP_GROUP, idx, v, 0); break;
    case LJ_FN_DMXIN_STATIC:       if (idx >= 1 && idx <= 20) lj_statics2_press((int)idx, (int)v); break;   /* ponytail: press / release on the current page; a Toggle slot flips on each rise (the exe's "flash and merge modes disregarded" needs the statics track's pinned path) */
    case LJ_FN_DMXIN_STATIC_LEVEL: if (idx >= 1 && idx <= 20) lj_statics2_set_level((int)idx, (int)(v * 100u + 127u) / 255); break;
    case LJ_FN_DMXIN_SEQ_ONOFF:    if (idx >= 1 && idx <= LJ_CUE_SLOTS) di_post(LJ_CMD_ENABLE, LJ_RUN_SLOT0 + idx - 1, v != 0, 0); break;
    case LJ_FN_DMXIN_SEQ_INT:      if (idx >= 1 && idx <= LJ_CUE_SLOTS) di_post(LJ_CMD_SEQ_INT, idx, v, 0); break;
    case LJ_FN_DMXIN_MASTER:       if (idx <= 8) lj_master_intensity_set((int)idx, (int)v); break;
    case LJ_FN_DMXIN_BLACKOUT:     di_post(LJ_CMD_BLACKOUT, 0, v != 0, 0); break;
    case LJ_FN_DMXIN_OVERWRITE:    di_post(LJ_CMD_OVERWRITE_ENABLE, 0, v != 0, 0); break;
    case LJ_FN_DMXIN_SMOKE:
        switch (idx) {
        case LJ_DMXIN_SMOKE_BUTTON:   di_post(LJ_CMD_SMOKE, LJ_SMOKE_BUTTON, v != 0, 0); break;
        case LJ_DMXIN_SMOKE_LEVEL:    di_post(LJ_CMD_SMOKE, LJ_SMOKE_LEVEL, v, 0); break;
        case LJ_DMXIN_SMOKE_DURATION: di_post(LJ_CMD_SMOKE, LJ_SMOKE_DURATION_MS, v * 100u, 0); break;   /* 0..25.5 s, PROPOSAL rig W5-DMXIN-5 */
        case LJ_DMXIN_SMOKE_CYCLE:    di_post(LJ_CMD_SMOKE, LJ_SMOKE_CYCLE_MS, 5000u + v * ((3600000u - 5000u) / 255u), 0); break;   /* 5 s .. 60 min (04 s12), linear PROPOSAL rig W5-DMXIN-5 */
        case LJ_DMXIN_SMOKE_HAZE:     di_post(LJ_CMD_SMOKE, LJ_SMOKE_HAZE_LEVEL, v, 0); break;
        case LJ_DMXIN_SMOKE_FAN:      di_post(LJ_CMD_SMOKE, LJ_SMOKE_FAN_LEVEL, v, 0); break;
        default: break;
        }
        break;
    case LJ_FN_DMXIN_MACRO:                                   /* level 0..100 % of the programmed value: "cannot exceed programmed values" (05 s3.2) */
        if (idx == 0) di_macro.amp = (uint8_t)((v * 100u + 127u) / 255u); else di_macro.speed = (uint8_t)((v * 100u + 127u) / 255u);
        di_post(LJ_CMD_MACRO_CTL, di_macro.amp, di_macro.speed, 0);
        break;
    case LJ_FN_DMXIN_TIME:         if (idx <= LJ_CUE_SLOTS) di_post(LJ_CMD_TIME, idx, (v * 200u + 127u) / 255u, 0); break;   /* 0..200 %, PROPOSAL rig W5-DMXIN-5 */
    case LJ_FN_DMXIN_AUDIO:                                    /* W5-AUD-6: the analyzer's Gain fader position / Noise Threshold */
        if (c->audio) { if (idx == 0) c->audio->gain_pct = lj_audio_gain_pct[v > 254u ? 254u : v]; else c->audio->noise = (uint8_t)v; }
        break;
    default: break;
    }
}

void lj_dmx_in_bind_fns(void)
{
    enum lj_fn f;
    for (f = LJ_FN_DMXIN_HTP; f <= LJ_FN_DMXIN_AUDIO; f = (enum lj_fn)(f + 1)) lj_fn_bind(f, di_fn, NULL);
}
