/* lj_form.h - the generated-table types tools/dfm2c.py emits (design 7.6).
 *
 * One lj_form_desc per Delphi form, holding a flat array of lj_ctl_desc rows in
 * DFM order (= z-order), each row a designated-initializer-friendly flat struct
 * with a slot for every DFM property the drop-list contract keeps. Menus are
 * lj_menu_item_desc trees. Everything in these tables is a fact harvested from
 * the DFM text (geometry, captions with Martin's typos verbatim, hints, menu
 * trees, tab orders); no artwork: Glyph/Picture/Icon blobs become a glyph NAME
 * from tools/glyphmap.tsv [D2, D13].
 *
 * Conventions for the slots (so a zeroed row means "the DFM did not say"):
 *  - ints: left/top/width/height always written. For the rest the generator
 *    writes the VCL class default when the DFM omits a property whose default is
 *    not 0: max 100 (TUpDown, TProgressBar, TTrackBar, TScrollBar), num_glyphs 1,
 *    step 10, page_size 10 (TScrollBar: 1), drop_down_count 8, spacing 4,
 *    frequency 1, item_height 13 (TListBox 13, TComboBox 13, TCheckListBox 13),
 *    default_row_height 24, default_col_width 64, fixed_rows 1, fixed_cols 1,
 *    row_count 5, col_count 5, bevel_width 1, max_value 100 (TSpinEdit), tag 0,
 *    item_index -1 (TComboBox/TListBox/TRadioGroup: -1 = none), image_index -1.
 *  - bools are tri-state uint8_t: 0 not given (class default), LJ_NO, LJ_YES.
 *    Class defaults the widgets apply: visible YES, enabled YES, tabstop YES for
 *    TWinControls except TPanel/TGroupBox-like containers, autosize YES for
 *    TLabel, parent_font YES, parent_color NO (YES for TLabel/TPanel), show_hint NO.
 *  - colours: LJ_CLR(rgb) sets bit 24 so 0 means "not given"; lj_desc_rgb().
 *  - strings: NULL = not given. Delphi enum values that are class-specific
 *    (Kind = bkOK, Style = csDropDownList, ViewStyle = vsReport, Shape = stCircle,
 *    Layout = blGlyphRight, ScrollBars = ssVertical, State = cbChecked ...) are
 *    kept as the Delphi identifier string verbatim; the widget compares once at
 *    creation. Form-level enums are real enums below.
 *  - fonts: font_name/font_height/font_style are the raw DFM facts when the
 *    control has a Font; font is the resolved face. A row without a Font gets
 *    the parent's resolved face (ultimately the form's, MS Sans Serif -11
 *    [07 fact-check 2]) and font_inherited = 1; LJ_FONT_INHERIT is left for
 *    hand-written rows.
 *  - events: {"OnClick", "sb7Click"} pairs, NULL-terminated, LJ_EVENTS_MAX per row;
 *    the generator fails a row with more.
 *  - Action = acX: resolved at generation time into caption/hint/shortcut/enabled/
 *    checked, and .action = "acX" kept for binding.
 *  - children: nested rows (TPanel, TGroupBox, TTabSheet, TActionList...);
 *    coordinates stay relative to the parent as the DFM gives them. */
#ifndef LJ_UI_FORMS_FORM_H
#define LJ_UI_FORMS_FORM_H

#include <stdint.h>
#include "core/lj_types.h"
#include "ui/lj_widget.h"

/* ---- the Delphi class census of design 7.2 plus ME-17's four; LJ_K_<CLASS> ---- */
enum lj_ctl_row_kind {
    LJ_K_UNKNOWN = 0,          /* a class the generator did not recognise: listed in tools/dfm2c_unknown.txt, painted as a sunken box with the class name */
    /* visual, the census in frequency order */
    LJ_K_LABEL, LJ_K_PANEL, LJ_K_EDIT, LJ_K_BITBTN, LJ_K_SPEEDBUTTON, LJ_K_CHECKBOX, LJ_K_BEVEL,
    LJ_K_LISTVIEW, LJ_K_TABSHEET, LJ_K_UPDOWN, LJ_K_BUTTON, LJ_K_SHAPE, LJ_K_RADIOGROUP, LJ_K_RICHEDIT,
    LJ_K_COMBOBOX, LJ_K_PROGRESSBAR, LJ_K_PAGECONTROL, LJ_K_GROUPBOX, LJ_K_UDBUTTON, LJ_K_STATICTEXT,
    LJ_K_IMAGE, LJ_K_PAINTBOX, LJ_K_SPLITTER, LJ_K_SCROLLBAR, LJ_K_SPINEDIT, LJ_K_RADIOBUTTON,
    LJ_K_STRINGGRID, LJ_K_NUMEDIT, LJ_K_TABCONTROL, LJ_K_ODPANEL2, LJ_K_MEMO, LJ_K_LISTBOX, LJ_K_MASKEDIT,
    LJ_K_DRAWGRID, LJ_K_CHECKLISTBOX, LJ_K_TREEVIEW, LJ_K_DIPSWITCH, LJ_K_TRACKBAR, LJ_K_STATUSBAR,
    LJ_K_HEADERCONTROL,
    LJ_K_DRIVECOMBOBOX, LJ_K_DIRECTORYLISTBOX, LJ_K_FILELISTBOX, LJ_K_ODPANEL,          /* ME-17 */
    /* non-visual components, kept as rows so every DFM object has a home */
    LJ_K_MAINMENU, LJ_K_POPUPMENU,                 /* their items are in lj_form_desc.menus, not in .children */
    LJ_K_TIMER,                                    /* interval, enabled, OnTimer */
    LJ_K_OPENDIALOG, LJ_K_SAVEDIALOG, LJ_K_OPENPICTUREDIALOG, LJ_K_COLORDIALOG, LJ_K_FONTDIALOG,
    LJ_K_PRINTDIALOG, LJ_K_PRINTERSETUPDIALOG,     /* filter/default_ext/title facts; print dialogs are not built (ME-17) */
    LJ_K_IMAGELIST, LJ_K_ACTIONLIST, LJ_K_ACTION,  /* TAction rows are children of their TActionList */
    LJ_K_COUNT
};
extern const char *const lj_ctl_class_names[LJ_K_COUNT];   /* "TLabel", "TPanel", ... Delphi spelling, enum order (lj_form.c) */

/* ---- fonts (design 7.4, ME-18) ---- */
typedef enum { LJ_FONT_INHERIT = 0, LJ_FONT_SANS13, LJ_FONT_SANS13B, LJ_FONT_ARIAL14, LJ_FONT_ARIAL14B, LJ_FONT_ARIALBLACK16,
               LJ_FONT_OTHER /* a face/height the census did not give us a bitmap for; font_name/font_height say which */ } lj_font_id;
#define LJ_FS_BOLD      1u      /* font_style bits: Font.Style = [fsBold, fsItalic, fsUnderline, fsStrikeOut] */
#define LJ_FS_ITALIC    2u
#define LJ_FS_UNDERLINE 4u
#define LJ_FS_STRIKEOUT 8u

/* ---- tri-state bools and tagged colours ---- */
#define LJ_NO  1
#define LJ_YES 2
#define LJ_COLOR_SET 0x01000000u
#define LJ_CLR(rgb) (LJ_COLOR_SET | ((rgb) & 0xFFFFFFu))
static inline int      lj_desc_bool(uint8_t v, int dflt) { return v == 0 ? dflt : v == LJ_YES; }
static inline uint32_t lj_desc_rgb(uint32_t c, uint32_t dflt) { return (c & LJ_COLOR_SET) ? (c & 0xFFFFFFu) : dflt; }

/* ---- form-level enums (facts from BorderStyle, FormStyle, BorderIcons, Position) ---- */
typedef enum { LJ_BS_SIZEABLE = 0 /* VCL default */, LJ_BS_NONE, LJ_BS_SINGLE, LJ_BS_DIALOG, LJ_BS_TOOLWINDOW, LJ_BS_SIZETOOLWIN } lj_border_style;
typedef enum { LJ_FS_NORMAL = 0, LJ_FS_MDICHILD, LJ_FS_MDIFORM, LJ_FS_STAYONTOP } lj_form_style;
#define LJ_BI_SYSMENU  1u       /* icons bits; a DFM "BorderIcons = []" is LJ_BI_GIVEN alone */
#define LJ_BI_MINIMIZE 2u
#define LJ_BI_MAXIMIZE 4u
#define LJ_BI_HELP     8u
#define LJ_BI_GIVEN    16u      /* the DFM wrote BorderIcons; without it the VCL default is [biSystemMenu, biMinimize, biMaximize] */
typedef enum { LJ_PO_DESIGNED = 0, LJ_PO_DEFAULT, LJ_PO_DEFAULTPOSONLY, LJ_PO_DEFAULTSIZEONLY, LJ_PO_SCREENCENTER,
               LJ_PO_DESKTOPCENTER, LJ_PO_MAINFORMCENTER, LJ_PO_OWNERFORMCENTER } lj_position;   /* poDesigned..poOwnerFormCenter, VCL order */

/* ---- control-level enums shared by many classes ---- */
typedef enum { LJ_BV_UNSET = 0, LJ_BV_NONE, LJ_BV_LOWERED, LJ_BV_RAISED, LJ_BV_SPACE } lj_bevel;   /* BevelInner/BevelOuter; unset = class default (TPanel: inner bvNone, outer bvRaised) */
typedef enum { LJ_TA_UNSET = 0, LJ_TA_LEFT, LJ_TA_RIGHT, LJ_TA_CENTER } lj_text_align;              /* Alignment = taLeftJustify / taRightJustify / taCenter */

#define LJ_EVENTS_MAX 12
typedef struct { const char *event, *handler; } lj_event_desc;        /* {"OnClick", "sb7Click"}; {NULL, NULL} ends the list */
typedef struct { const char *caption; int width; lj_text_align alignment;
                 int image_index; uint8_t autosize, allow_click; } lj_column_desc;   /* TListView Columns, THeaderControl Sections, TStatusBar Panels */

typedef struct lj_ctl_desc lj_ctl_desc;
struct lj_ctl_desc {
    enum lj_ctl_row_kind kind;
    const char *name;                                  /* DFM object name, the lj_window_find() key */
    int left, top, width, height;                      /* always written; relative to the parent */

    /* ints (see the defaults rule in the header comment) */
    int tag, tab_order, min, max, position, value, interval, row_count, col_count, item_index,
        max_length, group_index, num_glyphs, image_index, step, page_size, min_value, max_value,
        default_row_height, default_col_width, fixed_rows, fixed_cols, tab_index, item_height,
        spacing, frequency, drop_down_count, bevel_width, border_width, indent, modal_result,
        min_width, min_height, max_width, max_height,  /* Constraints.* */
        pen_width, sel_start, sel_end, alloc_by, filter_index, min_size, min_font_size, max_font_size,
        line_width, columns_count /* TRadioGroup/TListBox Columns = n */, shortcut /* TShortCut word, menus and actions */;

    /* tri-state bools */
    uint8_t visible, enabled, tabstop, readonly, checked, autosize, transparent, wordwrap, default_btn, cancel,
            flat, down, multiselect, allow_all_up, wrap, show_hint, parent_show_hint, parent_font, parent_color,
            row_select, grid_lines, hide_selection, column_click, key_preview, stretch, smooth, multiline,
            full_repaint, want_returns, thousands, owner_draw, owner_data, show_column_headers, sorted,
            auto_select, auto_snap, beveled, checkboxes, show_root, show_lines, right_click_select, simple_panel,
            default_drawing, auto_hotkeys, plain_text, tool_tips, full_drag, drag_reorder, auto_line_reduction,
            auto_arrange, radio_item, break_item /* TMenuItem Break */, center /* TImage Center */,
            parent_bidi_mode, font_inherited /* 1 = no Font in the DFM; font is the parent's resolved face */;

    /* strings (CP1252, verbatim) */
    const char *caption, *hint, *text, *action, *popup_menu, *filter, *default_ext, *title, *edit_mask,
               *password_char, *kind_str /* Kind = bkOK, sbVertical, TDip10 */, *style /* Style = csDropDownList, bsRaised ... */,
               *view_style, *shape, *layout, *border_style /* bsNone/bsSingle on controls */, *scroll_bars, *drag_mode,
               *state /* cbChecked */, *tick_style, *tick_marks, *orientation, *char_case, *text_case, *sort_type,
               *resize_style, *options /* the Delphi set text, e.g. "[goFixedVertLine, goRowSelect]" */, *category,
               *mask, *associate, *focus_control, *active_page, *active_control, *images, *file_list, *dir_list,
               *glyph /* art/glyphs/<name> from tools/glyphmap.tsv, replaces Glyph.Data [D2] */,
               *picture /* same, for Picture.Data / Icon.Data / TImageList Bitmap */, *font_name,
               *bidi_mode /* bdLeftToRight ... */;
    int font_height;                                   /* Font.Height as written, e.g. -11 */
    unsigned font_style;                               /* LJ_FS_* bits */
    lj_font_id font;                                   /* resolved face; LJ_FONT_INHERIT when the row has no Font */

    /* colours, LJ_CLR()-tagged */
    uint32_t color, font_color, brush_color, pen_color, base_color, dip_color, text_color;

    /* layout */
    lj_align align; lj_anchors anchors; lj_bevel bevel_inner, bevel_outer; lj_text_align alignment;

    /* lists */
    const char *const *items; int nitems;              /* Items.Strings / Lines.Strings / Tabs.Strings / decoded Items.Data */
    const lj_column_desc *columns; int ncolumns;
    const int *row_heights; int nrow_heights;          /* TStringGrid/TDrawGrid RowHeights = (16 16 ...) */
    const lj_ctl_desc *children; int nchildren;
    lj_event_desc events[LJ_EVENTS_MAX];
};

typedef struct lj_menu_item_desc lj_menu_item_desc;
struct lj_menu_item_desc {
    const char *name, *caption;                        /* "-" caption is a separator, as in the VCL */
    const char *hint, *action, *on_click;              /* on_click = the OnClick handler name */
    int shortcut;                                      /* TShortCut word: low byte VK, 0x2000 Shift, 0x4000 Ctrl, 0x8000 Alt */
    int tag, group_index, image_index;
    uint8_t checked, radio_item, enabled, visible, default_item, break_item, auto_check,   /* tri-state */
            auto_hotkeys, auto_line_reduction;         /* maAutomatic = LJ_YES, maManual = LJ_NO */
    const lj_menu_item_desc *children; int nchildren;
};
typedef struct { const char *name; int is_main; const lj_menu_item_desc *items; int n; } lj_menu_desc;   /* one TMainMenu or TPopupMenu */

typedef struct {
    const char *name;                                  /* DFM object name, "SequenceForm" */
    const char *class_name;                            /* "TSequenceForm" */
    const char *caption;
    int client_width, client_height, left, top;        /* design geometry; Left/Top used only for poDesigned (7.10) */
    int width, height;                                 /* window size as the DFM wrote it (AutoScroll forms store this instead of Client*) */
    uint8_t client_derived;                            /* 1 = client_* computed from width/height with the classic frame metrics (7.3) */
    int text_height;                                   /* TextHeight fact: 13 for MS Sans Serif -11 */
    int border_width, tag;
    const char *icon;                                  /* Icon.Data -> glyphmap name or "missing" */
    const char *print_scale;                           /* PrintScale = poNone, verbatim */
    uint32_t color, font_color;                        /* LJ_CLR()-tagged; form default clBtnFace */
    lj_font_id font; const char *font_name; int font_height; unsigned font_style;
    lj_border_style border; lj_form_style style; unsigned icons; lj_position position;
    int min_width, min_height, max_width, max_height;  /* Constraints */
    uint8_t key_preview, visible, enabled, show_hint, auto_scroll, scaled,   /* tri-state */
            autosize, parent_font, horz_scrollbar_visible, vert_scrollbar_visible;
    const char *menu;                                  /* Menu = MainMenu1 (a name into menus[]), NULL = none */
    const char *popup_menu, *active_control, *hint;
    const lj_menu_desc *menus; int nmenus;             /* every TMainMenu/TPopupMenu on the form, by name */
    const lj_ctl_desc *ctls; int n;                    /* top-level rows in DFM order */
    lj_event_desc events[LJ_EVENTS_MAX];               /* OnCreate, OnShow, OnClose ... */
} lj_form_desc;

/* lj_form.c helpers over the tables (SDL-free, used by ui, windows and lj4evr_render). */
const lj_ctl_desc       *lj_form_find_ctl(const lj_form_desc *f, const char *name);   /* depth-first over ctls and children; NULL when absent */
const lj_menu_desc      *lj_form_find_menu(const lj_form_desc *f, const char *name);
int                      lj_form_count_ctls(const lj_form_desc *f);                   /* rows including nested children */
const lj_form_desc *const *lj_forms_all(int *count);                                  /* the 208 generated forms (lj_forms_gen.c), for lj4evr_render */

#endif /* LJ_UI_FORMS_FORM_H */
