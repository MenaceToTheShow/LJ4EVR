/* lj_dmx_in.c - the DMXIN family (docs/w4-contracts.md s3-s4; design 1 sacrifice list, 12 F8 / I7: "window W4, input W5").
 * The five DMX-in windows open from their entry points with every input control greyed, hint "Not available in LJ4EVR";
 * nothing receives DMX (no socket, no engine path: sACN input is W5). What a window shows is what LJ2 shows with no input:
 * Dmx In lists channels 001..512 at 000 with no function, DMX-In translation lists channels 1..512 with no table and
 * "0 active tranlation(s)" (exe 0x21D0A8, sic), the table editor shows a new 1:1 table. Entry points [05 s3]:
 *   dmx_in             C-L toolbar DMX In (LJ_ACT_DMX_IN), Global Patch DMX-in source (lj_global_patch.c)
 *   dmxin_translation  Dmx In > Value-translations
 *   translation_edit   DMX-In translation > Available tables popup > Create new table / Edit table
 *   value_table_error  raised by a .vtr import; the import is W5, so it opens only by id (fidelity row)
 *   export_table       no menu item in any DFM names it (fidelity row); opens by id
 * Handlers: every OnXxx of the five DFMs (ME-16). */
#include "windows/lj_dmx_in.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"

enum { LJ_DI_N = 5, LJ_DI_CH = 512, LJ_DI_VALUES = 256 };
static lj_window *di_win[LJ_DI_N];
static void (*di_root_orig[LJ_DI_N])(lj_widget *, int, void *);
static char di_rows[LJ_DI_CH][24];
static const char *di_items[LJ_DI_CH];
static char di_cells[LJ_DI_VALUES][2][4];

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
static void grey_menu(lj_menu *m, const lj_menu_item_desc *it, int n, const char *const *keep, int nkeep)   /* leaves only: a parent stays live so its greyed children show */
{
    int i, k, kept;
    for (i = 0; i < n; i++) {
        if (it[i].nchildren) { grey_menu(m, it[i].children, it[i].nchildren, keep, nkeep); continue; }
        if (!it[i].name || (it[i].caption && strcmp(it[i].caption, "-") == 0)) continue;
        for (kept = 0, k = 0; k < nkeep; k++) kept |= strcmp(it[i].name, keep[k]) == 0;
        if (!kept) lj_menu_set_enabled(m, it[i].name, 0);
    }
}

/* ---- handlers ---- */
static void na(lj_widget *w, void *ud) { const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w)); (void)ud; lj_log("dmx in: %s: " LJ_DMX_IN_NA, it && it->name ? it->name : w && w->name ? w->name : "?"); }
static void close_self(lj_widget *w, void *ud) { int i = di_of(w); (void)ud; if (i >= 0) lj_dmx_in_close((enum lj_win)(LJ_WIN_DMX_IN + i)); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }
static void on_translations(lj_widget *w, void *ud) { (void)w; (void)ud; lj_dmx_in_open(LJ_WIN_DMXIN_TRANSLATION); }
static void on_tr_pop(lj_widget *w, void *ud)   /* Pop_Click: OnPopup of both popups and OnClick of their items */
{
    const lj_menu_item_desc *it = lj_window_menu_item(lj_window_of(w));
    if (!it || !it->name) return;
    if (strcmp(it->name, "pop_NewTable") == 0 || strcmp(it->name, "pop_EditRepositoryTable") == 0) lj_dmx_in_open(LJ_WIN_TRANSLATION_EDIT);
    else na(w, ud);
}
static void on_graph(lj_widget *w, void *ud)   /* TTranslationEditForm.PB1Paint: the 1:1 table of a new table; geometry PROPOSAL rig (dmxintranslationedit.gif is reduced) */
{
    lj_window *win = lj_window_of(w);
    lj_surface *s = win ? lj_window_surface(win) : NULL;
    int x, y, v, l = 12, b, gw, gh;
    (void)ud;
    if (!s || w->w < 40 || w->h < 40) return;
    lj_widget_abs(w, &x, &y);
    b = w->h - 12; gw = w->w - l - 6; gh = b - 6;
    lj_fill(s, x, y, w->w, w->h, 0x0000FFu);                          /* Color = clBlue [DFM] */
    lj_vline(s, x + l, y + 4, b - 2, 0x00FFFFu); lj_hline(s, x + 4, y + b, w->w - 8, 0x00FFFFu);
    for (v = 0; v < LJ_DI_VALUES; v++) lj_fill(s, x + l + 1 + v * gw / 255, y + b - 1 - v * gh / 255, 2, 2, 0xFFFFFFu);
    if (w->font) lj_text(s, w->font, x + w->w - 6 - lj_text_width(w->font, "DMX Value"), y + b - 14, "DMX Value", 0xFFFF00u, 0);   /* exe 0x21B194 */
}

static const lj_binding di_dmxin_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "FormShow", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "Timer2Timer", lj_handler_noop, NULL }, { "FunctionListPopPopup", lj_handler_noop, NULL },
    { "M_OptionsClick", lj_handler_noop, NULL }, { "LVDmxFunctionsDragOver", lj_handler_noop, NULL }, { "LVDmxFunctionsDragDrop", na, NULL },
    { "bClearClick", na, NULL }, { "bAssignClick", na, NULL }, { "bSaveClick", na, NULL }, { "bcancelClick", close_self, NULL },
    { "M_DisableDMXinClick", na, NULL }, { "ClearAllAssignments1Click", na, NULL }, { "ClearCueLacthassignments1Click", na, NULL }, { "PrefsClick", na, NULL },
    { "Pop_Click", na, NULL }, { "M_ValueTranslationsClick", on_translations, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_tr_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "Panel2Resize", lj_handler_noop, NULL }, { "lvChannelsChange", lj_handler_noop, NULL }, { "lvChannelsMouseMove", lj_handler_noop, NULL }, { "UpdateClick", lj_handler_noop, NULL },
    { "tv1Change", lj_handler_noop, NULL }, { "PB1Paint", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "DoDragOver", lj_handler_noop, NULL },
    { "DoDragDrop", na, NULL }, { "sbUpClick", na, NULL }, { "Menu_Click", na, NULL }, { "Pop_Click", on_tr_pop, NULL },
    { "bOkCancelClick", close_self, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_edit_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL },
    { "sg1Exit", lj_handler_noop, NULL }, { "sg1SelectCell", lj_handler_noop, NULL }, { "eExit", lj_handler_noop, NULL }, { "eKeyPress", lj_handler_noop, NULL },
    { "PB1Paint", on_graph, NULL }, { "bSetClick", na, NULL }, { "bOkClick", close_self, NULL }, { "Help1Click", on_help, NULL },
    { NULL, NULL, NULL } };
static const lj_binding di_none_b[] = { { NULL, NULL, NULL } };
static const lj_binding di_export_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "DirectoryListBoxChange", lj_handler_noop, NULL }, { "bExportClick", na, NULL },
    { NULL, NULL, NULL } };

static const struct {
    const lj_form_desc *form; const lj_binding *b;
    const char *keep[3]; int nkeep;                 /* controls that only close the window stay live */
    const char *menu_keep[3]; int nmenu_keep;       /* menu items that open a window or help */
    const char *close_btn[2];                       /* buttons with no OnClick in the DFM (Kind / ModalResult) that close */
} di_forms[LJ_DI_N] = {
    { &lj_form_TDMXINFORM, di_dmxin_b, { "bcancel" }, 1, { "M_ValueTranslations", "Help1" }, 2, { NULL } },
    { &lj_form_TDMXINVALUETRANSLATIONFORM, di_tr_b, { "bOk", "bCancel", "tv1" }, 3, { "Help1", "pop_NewTable" }, 2, { NULL } },
    { &lj_form_TTRANSLATIONEDITFORM, di_edit_b, { "bOk", "BitBtn4" }, 2, { "Help1" }, 1, { "BitBtn4" } },
    { &lj_form_TVALUETABLEIMPORTERRORFORM, di_none_b, { "bOk" }, 1, { NULL }, 0, { "bOk" } },
    { &lj_form_TEXPORTTABLEFORM, di_export_b, { "BitBtn2" }, 1, { NULL }, 0, { "BitBtn2" } },
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
    if (i == 0 || i == 1) {
        for (k = 0; k < LJ_DI_CH; k++) {
            if (i == 0) snprintf(di_rows[k], sizeof di_rows[k], "%03d\t000\t\t", k + 1);   /* dmxindialog.gif: "001" "255" ... */
            else snprintf(di_rows[k], sizeof di_rows[k], "%d\t", k + 1);                   /* dmxintranslation.gif: "1" ... */
            di_items[k] = di_rows[k];
        }
        if ((w = lj_window_find(win, i == 0 ? "LVDmxFunctions" : "lvChannels")) != NULL) lj_w_listview_set_items(w, di_items, LJ_DI_CH);
    }
    if (i == 0 && (w = lj_window_find(win, "eCue")) != NULL) lj_w_edit_set_text(w, "N/A");   /* dmxindialog.gif: no cue latched */
    if (i == 1) lj_window_set_text(win, "lActive", "0 active tranlation(s)");                  /* exe 0x21D0A8 "%d active tranlation(s)" (sic, D13) */
    if (i == 1 && (w = lj_window_find(win, "pButtons")) != NULL && w->parent) w->color = w->parent->color;   /* DFM clRed (a design-time marker): dmxintranslation.gif shows face, so LJ2 recolours it (PROPOSAL rig) */
    if (i == 2) for (k = 0; k < 2; k++) if ((w = lj_window_find(win, k ? "BitBtn3" : "BitBtn1")) != NULL) w->visible = 0;   /* Save / Load: Top 264 in the 231-px Panel1, clipped away in the VCL (dmxintranslationedit.gif shows neither) */
    if (i == 2 && (w = lj_window_find(win, "sg1")) != NULL) {                                  /* a new table: 1:1 (missing entries = 1:1 [05 s3.3]) */
        lj_w_grid_set_cell(w, 0, 0, "DMX value"); lj_w_grid_set_cell(w, 1, 0, "Translation");   /* exe 0x21AD20, 0x21AD34 */
        for (k = 0; k < LJ_DI_VALUES; k++) {
            snprintf(di_cells[k][0], sizeof di_cells[k][0], "%d", k);
            lj_w_grid_set_cell(w, 0, k + 1, di_cells[k][0]); lj_w_grid_set_cell(w, 1, k + 1, di_cells[k][0]);
        }
    }
    if (i == 4) lj_window_set_text(win, "pTarget", "");   /* LJ2 writes the chosen folder here; the folder pick is W5 */
}

lj_window *lj_dmx_in_open(enum lj_win id)
{
    int i = di_index(id), k;
    lj_widget *root, *w;
    if (i < 0) return NULL;
    if (di_win[i]) { lj_win_show(di_win[i], 0); return di_win[i]; }
    di_win[i] = lj_window_create(di_forms[i].form, di_forms[i].b);
    if (!di_win[i]) { lj_log("dmx in: cannot create %s", di_forms[i].form->class_name); return NULL; }
    root = lj_window_root(di_win[i]);
    di_root_orig[i] = root->on_event; root->on_event = on_root;
    for (k = 0; k < 2 && di_forms[i].close_btn[k]; k++) if ((w = lj_window_find(di_win[i], di_forms[i].close_btn[k])) != NULL) { w->on_event = on_close_btn; w->ud = NULL; }
    fill(i);
    lj_dmx_in_grey(root, di_forms[i].keep, di_forms[i].nkeep);
    for (k = 0; k < di_forms[i].form->nmenus; k++)
        grey_menu(lj_window_menus(di_win[i]), di_forms[i].form->menus[k].items, di_forms[i].form->menus[k].n, di_forms[i].menu_keep, di_forms[i].nmenu_keep);
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
}

lj_window *lj_dmx_in_get(enum lj_win id) { int i = di_index(id); return i >= 0 ? di_win[i] : NULL; }
void       lj_dmx_in_refresh(enum lj_win id) { (void)id; }   /* nothing to show until the W5 input path: no counters move */
void       lj_dmx_in_bind_fns(void) { }                     /* LJ_ACT_DMX_IN is routed by app/lj_app_w4.c; no lj_fn row (the DMX-in hotkeys are W5) */
