#!/usr/bin/env python3
"""dfm2c.py - Delphi text-DFM -> C tables generator (design 7.6, ME-12).

Reads reference/installer/exe_resources/dfm_text/*.dfm.txt (never committed) and
writes src/ui/forms/lj_forms_gen.c + .h, tools/dfm2c_unknown.txt,
tools/dfm2c_blobs.txt and tests/ui/dfm_census.tsv. Every DFM property is placed
in a slot of ui/forms/lj_form.h except those on tools/dfm2c_drop.txt; a
property with no slot lands in dfm2c_unknown.txt (aim: zero). Art blobs
(Glyph/Picture/Icon/Bitmap) become a name from tools/glyphmap.tsv or "missing";
Items.Data (TListItems stream) is decoded to strings when the bytes allow.

usage: dfm2c.py [--repo D:/lj4evr] [--check]
  --check   regenerate into memory and diff against the committed files; exit 1 on any difference.
Python 3.12, stdlib only. Strings are CP1252 bytes end to end: the .c carries them as octal escapes.
"""
import argparse
import hashlib
import os
import re
import sys

GEN_VERSION = "1.0"
DFM_DIR = os.path.join("reference", "installer", "exe_resources", "dfm_text")
GEN_C = os.path.join("src", "ui", "forms", "lj_forms_gen.c")
GEN_H = os.path.join("src", "ui", "forms", "lj_forms_gen.h")
DROP_TXT = os.path.join("tools", "dfm2c_drop.txt")
UNKNOWN_TXT = os.path.join("tools", "dfm2c_unknown.txt")
BLOBS_TXT = os.path.join("tools", "dfm2c_blobs.txt")
GLYPHMAP_TSV = os.path.join("tools", "glyphmap.tsv")
CENSUS_TSV = os.path.join("tests", "ui", "dfm_census.tsv")
EVENTS_MAX = 12                                  # LJ_EVENTS_MAX in lj_form.h


# ----------------------------------------------------------------------------- parser
class Obj:
    __slots__ = ("kind", "name", "cls", "props", "children")

    def __init__(self, kind, name, cls):
        self.kind, self.name, self.cls = kind, name, cls
        self.props = []        # [(key, value)] in DFM order; value = (tag, payload)
        self.children = []

    def get(self, key, default=None):
        for k, v in self.props:
            if k == key:
                return v
        return default


OBJ_RE = re.compile(r"^(object|inherited|inline)\s+(\w+)\s*:\s*(\w+)\s*(\[\d+\])?$")
PROP_RE = re.compile(r"^([\w.]+)\s*=\s*(.*)$")
CHAR_RE = re.compile(r"#(\$[0-9A-Fa-f]+|\d+)")
SCALAR_RE = re.compile(r"-?\d+(?:\.\d+)?|[A-Za-z_]\w*")
COLL_KEY_RE = re.compile(r"([\w.]+)=")
# A closing quote is the first one followed by: end of text, a #char code, a + join, a list or
# collection delimiter, the next list item, or the next collection key. Needed because the
# dfm_text flattening wrote inner quotes unescaped ('Off' DMX value).
CLOSE_RE = re.compile(r"(?:\s*$|#|\s*[+)>,]|\s+'|\s+\w+=)")


def scan_str(t, i):
    """t[i] == "'"; returns (bytes, index after the closing quote)."""
    j = i + 1
    while True:
        j = t.find("'", j)
        if j < 0:
            raise ValueError("unterminated string: %r" % t[i:i + 60])
        if CLOSE_RE.match(t, j + 1):
            return t[i + 1:j].encode("cp1252"), j + 1
        j += 1


def scan_strexpr(t, i):
    """String pieces 'abc'#13#10'def' (and ' + ' joins) starting at t[i]; returns (bytes, i)."""
    out = b""
    n = len(t)
    while True:
        if i < n and t[i] == "'":
            s, i = scan_str(t, i)
            out += s
        elif i < n and t[i] == "#":
            m = CHAR_RE.match(t, i)
            if not m:
                raise ValueError("bad char code at %r" % t[i:i + 10])
            code = int(m.group(1)[1:], 16) if m.group(1).startswith("$") else int(m.group(1))
            if code > 255:
                raise ValueError("wide char #%d" % code)
            out += bytes([code])
            i = m.end()
        else:
            k = i
            while k < n and t[k] in " \t\r\n":
                k += 1
            if k < n and t[k] == "+":          # '...' + '...' continuation
                k += 1
                while k < n and t[k] in " \t\r\n":
                    k += 1
                if k < n and t[k] in "'#":
                    i = k
                    continue
            return out, i


def scan_scalar(t, i):
    """One value inside a list or a collection item: string expr, number, identifier, (..)."""
    n = len(t)
    c = t[i]
    if c in "'#":
        s, i = scan_strexpr(t, i)
        return ("str", s), i
    if c == "(":
        d, j = 0, i
        while j < n:
            d += (t[j] == "(") - (t[j] == ")")
            j += 1
            if d == 0:
                break
        return ("list", parse_list(t[i + 1:j - 1])), j
    m = SCALAR_RE.match(t, i)
    if not m:
        raise ValueError("unexpected %r" % t[i:i + 20])
    tok = m.group(0)
    if re.fullmatch(r"-?\d+", tok):
        return ("int", int(tok)), m.end()
    if tok[0] in "-0123456789":
        return ("float", tok), m.end()
    return ("ident", tok), m.end()


def parse_list(body):
    items, i, n = [], 0, len(body)
    while True:
        while i < n and body[i] in " \t\r\n":
            i += 1
        if i >= n:
            return items
        v, i = scan_scalar(body, i)
        items.append(v)


def parse_coll(text):
    """<<K=v K=v> <..>> (dfm_text flattened form) or the standard multi-line item/end form."""
    t = text.strip()
    if t[0] != "<" or t[-1] != ">":
        raise ValueError("bad collection %r" % text[:40])
    inner = t[1:-1].strip()
    items = []
    if inner.startswith("<"):                      # flattened
        i, n = 0, len(inner)
        while i < n:
            if inner[i] in " \t":
                i += 1
                continue
            if inner[i] != "<":
                raise ValueError("bad collection item at %r" % inner[i:i + 30])
            i += 1
            props = []
            while True:
                while inner[i] in " \t":
                    i += 1
                if inner[i] == ">":
                    i += 1
                    break
                m = COLL_KEY_RE.match(inner, i)
                if not m:
                    raise ValueError("bad collection item at %r" % inner[i:i + 30])
                v, i = scan_scalar(inner, m.end())
                props.append((m.group(1), v))
            items.append(props)
        return items
    props = None
    for line in inner.splitlines():               # standard form
        s = line.strip()
        if s == "item":
            props = []
        elif s == "end":
            items.append(props)
            props = None
        elif s:
            m = PROP_RE.match(s)
            if not m or props is None:
                raise ValueError("bad collection line %r" % s)
            props.append((m.group(1), parse_value(m.group(2))))
    return items


def parse_value(text):
    t = text.strip()
    if t[0] in "'#":
        s, i = scan_strexpr(t, 0)
        if t[i:].strip():
            raise ValueError("trailing text after string: %r" % t)
        return ("str", s)
    if t[0] == "[":
        return ("set", [x.strip() for x in t[1:-1].split(",") if x.strip()])
    if t[0] == "(":
        return ("list", parse_list(t[1:-1]))
    if t[0] == "<":
        return ("coll", parse_coll(t))
    if t[0] == "{":
        trunc = t.endswith("...")
        hexs = re.sub(r"[^0-9A-Fa-f]", "", t[1:t.rindex("}")])
        return ("blob", (bytes.fromhex(hexs), trunc))
    if re.fullmatch(r"-?\d+", t):
        return ("int", int(t))
    if re.fullmatch(r"-?\d+\.\d+", t):
        return ("float", t)
    if re.fullmatch(r"\w+", t):
        return ("ident", t)
    raise ValueError("cannot parse value %r" % t)


def balanced(t, pair):
    return t.count(pair[0]) == t.count(pair[1])


def parse_dfm(text):
    lines = text.splitlines()
    root, stack, i = None, [], 0
    while i < len(lines):
        s = lines[i].strip()
        i += 1
        if not s:
            continue
        m = OBJ_RE.match(s)
        if m:
            o = Obj(m.group(1), m.group(2), m.group(3))
            if stack:
                stack[-1].children.append(o)
            else:
                root = o
            stack.append(o)
            continue
        if s == "end":
            stack.pop()
            continue
        m = PROP_RE.match(s)
        if not m or not stack:
            raise ValueError("line %d: %r" % (i, s))
        key, rest = m.group(1), m.group(2).strip()
        # multi-line values: '...' + continuation, ( ... ), < item ... >, { hex }
        more = rest.endswith("+") or (rest[0] == "(" and not balanced(rest, "()")) \
            or (rest[0] == "<" and not balanced(rest, "<>")) or (rest[0] == "{" and "}" not in rest)
        if more:
            buf = [rest]
            while True:
                nxt = lines[i].strip()
                i += 1
                buf.append(nxt)
                joined = "\n".join(buf)
                if rest[0] in "'#":
                    if not nxt.endswith("+"):
                        break
                elif rest[0] == "(" and balanced(joined, "()"):
                    break
                elif rest[0] == "<" and balanced(joined, "<>"):
                    break
                elif rest[0] == "{" and "}" in nxt:
                    break
            rest = joined
        stack[-1].props.append((key, parse_value(rest)))
    if stack or root is None:
        raise ValueError("unbalanced object nesting")
    return root


# ----------------------------------------------------------------------------- Delphi facts
# Delphi TColor constants as RGB (system colours at the classic Windows values of design 7.3).
COLORS = {
    "clBlack": 0x000000, "clMaroon": 0x800000, "clGreen": 0x008000, "clOlive": 0x808000, "clNavy": 0x000080,
    "clPurple": 0x800080, "clTeal": 0x008080, "clGray": 0x808080, "clSilver": 0xC0C0C0, "clRed": 0xFF0000,
    "clLime": 0x00FF00, "clYellow": 0xFFFF00, "clBlue": 0x0000FF, "clFuchsia": 0xFF00FF, "clAqua": 0x00FFFF,
    "clLtGray": 0xC0C0C0, "clDkGray": 0x808080, "clWhite": 0xFFFFFF, "clMoneyGreen": 0xC0DCC0,
    "clSkyBlue": 0xA6CAF0, "clCream": 0xFFFBF0, "clMedGray": 0xA0A0A4,
    "clBtnFace": 0xC0C0C0, "clBtnHighlight": 0xFFFFFF, "clBtnShadow": 0x808080, "cl3DDkShadow": 0x000000,
    "cl3DLight": 0xC0C0C0, "clBtnText": 0x000000, "clWindow": 0xFFFFFF, "clWindowText": 0x000000,
    "clWindowFrame": 0x000000, "clHighlight": 0x000080, "clHighlightText": 0xFFFFFF, "clGrayText": 0x808080,
    "clMenu": 0xC0C0C0, "clMenuText": 0x000000, "clActiveCaption": 0x000080, "clInactiveCaption": 0x808080,
    "clCaptionText": 0xFFFFFF, "clInactiveCaptionText": 0xC0C0C0, "clActiveBorder": 0xC0C0C0,
    "clInactiveBorder": 0xC0C0C0, "clAppWorkSpace": 0x808080, "clScrollBar": 0xC0C0C0, "clBackground": 0x008080,
    "clInfoBk": 0xFFFFE1, "clInfoText": 0x000000, "clNone": 0xFFFFFF, "clDefault": 0x000000,
}
FONT_STYLES = {"fsBold": "LJ_FS_BOLD", "fsItalic": "LJ_FS_ITALIC", "fsUnderline": "LJ_FS_UNDERLINE", "fsStrikeOut": "LJ_FS_STRIKEOUT"}
ALIGNS = {"alNone": "LJ_AL_NONE", "alTop": "LJ_AL_TOP", "alBottom": "LJ_AL_BOTTOM", "alLeft": "LJ_AL_LEFT", "alRight": "LJ_AL_RIGHT",
          "alClient": "LJ_AL_CLIENT"}
ANCHORS = {"akLeft": "LJ_AK_LEFT", "akTop": "LJ_AK_TOP", "akRight": "LJ_AK_RIGHT", "akBottom": "LJ_AK_BOTTOM"}
BEVELS = {"bvNone": "LJ_BV_NONE", "bvLowered": "LJ_BV_LOWERED", "bvRaised": "LJ_BV_RAISED", "bvSpace": "LJ_BV_SPACE"}
TALIGNS = {"taLeftJustify": "LJ_TA_LEFT", "taRightJustify": "LJ_TA_RIGHT", "taCenter": "LJ_TA_CENTER"}
BORDER_STYLES = {"bsSizeable": "LJ_BS_SIZEABLE", "bsNone": "LJ_BS_NONE", "bsSingle": "LJ_BS_SINGLE", "bsDialog": "LJ_BS_DIALOG",
                 "bsToolWindow": "LJ_BS_TOOLWINDOW", "bsSizeToolWin": "LJ_BS_SIZETOOLWIN"}
FORM_STYLES = {"fsNormal": "LJ_FS_NORMAL", "fsMDIChild": "LJ_FS_MDICHILD", "fsMDIForm": "LJ_FS_MDIFORM", "fsStayOnTop": "LJ_FS_STAYONTOP"}
BORDER_ICONS = {"biSystemMenu": "LJ_BI_SYSMENU", "biMinimize": "LJ_BI_MINIMIZE", "biMaximize": "LJ_BI_MAXIMIZE", "biHelp": "LJ_BI_HELP"}
POSITIONS = {"poDesigned": "LJ_PO_DESIGNED", "poDefault": "LJ_PO_DEFAULT", "poDefaultPosOnly": "LJ_PO_DEFAULTPOSONLY",
             "poDefaultSizeOnly": "LJ_PO_DEFAULTSIZEONLY", "poScreenCenter": "LJ_PO_SCREENCENTER",
             "poDesktopCenter": "LJ_PO_DESKTOPCENTER", "poMainFormCenter": "LJ_PO_MAINFORMCENTER",
             "poOwnerFormCenter": "LJ_PO_OWNERFORMCENTER"}
# the census classes in lj_ctl_row_kind order (lj_form.h); TMenuItem is a menu tree, not a row
KINDS = ["TLabel", "TPanel", "TEdit", "TBitBtn", "TSpeedButton", "TCheckBox", "TBevel", "TListView", "TTabSheet", "TUpDown",
         "TButton", "TShape", "TRadioGroup", "TRichEdit", "TComboBox", "TProgressBar", "TPageControl", "TGroupBox", "TUdButton",
         "TStaticText", "TImage", "TPaintBox", "TSplitter", "TScrollBar", "TSpinEdit", "TRadioButton", "TStringGrid", "TNumEdit",
         "TTabControl", "TODPanel2", "TMemo", "TListBox", "TMaskEdit", "TDrawGrid", "TCheckListBox", "TTreeView", "TDipSwitch",
         "TTrackBar", "TStatusBar", "THeaderControl", "TDriveComboBox", "TDirectoryListBox", "TFileListBox", "TODPanel",
         "TMainMenu", "TPopupMenu", "TTimer", "TOpenDialog", "TSaveDialog", "TOpenPictureDialog", "TColorDialog", "TFontDialog",
         "TPrintDialog", "TPrinterSetupDialog", "TImageList", "TActionList", "TAction"]
KIND_ENUM = {c: "LJ_K_" + c[1:].upper() for c in KINDS}
# VCL non-zero int defaults the widgets rely on (lj_form.h header comment)
INT_DEFAULTS = {
    "TUpDown": {"max": 100}, "TProgressBar": {"max": 100, "step": 10}, "TTrackBar": {"max": 10, "page_size": 2, "frequency": 1},
    "TScrollBar": {"max": 100, "page_size": 1}, "TSpinEdit": {"max_value": 100},
    "TBitBtn": {"num_glyphs": 1, "spacing": 4}, "TSpeedButton": {"num_glyphs": 1, "spacing": 4},
    "TComboBox": {"drop_down_count": 8, "item_height": 13, "item_index": -1}, "TListBox": {"item_height": 13, "item_index": -1},
    "TCheckListBox": {"item_height": 13, "item_index": -1}, "TRadioGroup": {"item_index": -1},
    "TStringGrid": {"default_row_height": 24, "default_col_width": 64, "fixed_rows": 1, "fixed_cols": 1, "row_count": 5, "col_count": 5},
    "TDrawGrid": {"default_row_height": 24, "default_col_width": 64, "fixed_rows": 1, "fixed_cols": 1, "row_count": 5, "col_count": 5},
    "TPanel": {"bevel_width": 1}, "TODPanel": {"bevel_width": 1}, "TODPanel2": {"bevel_width": 1},
    "TTreeView": {"indent": 19}, "TImage": {"image_index": -1},
}
# classic Windows frame metrics at 96 dpi (design 7.3): (border px, caption px) per border style, for
# the AutoScroll forms that stored Width/Height instead of ClientWidth/ClientHeight
FRAME = {"LJ_BS_SIZEABLE": (4, 19), "LJ_BS_NONE": (0, 0), "LJ_BS_SINGLE": (3, 19), "LJ_BS_DIALOG": (3, 19),
         "LJ_BS_TOOLWINDOW": (3, 15), "LJ_BS_SIZETOOLWIN": (4, 15)}
MENU_BAR = 19

# control slots: DFM key -> lj_ctl_desc slot
INT = {"Tag": "tag", "TabOrder": "tab_order", "Min": "min", "Max": "max", "Position": "position", "Value": "value",
       "Interval": "interval", "RowCount": "row_count", "ColCount": "col_count", "ItemIndex": "item_index",
       "MaxLength": "max_length", "GroupIndex": "group_index", "NumGlyphs": "num_glyphs", "ImageIndex": "image_index",
       "Step": "step", "PageSize": "page_size", "MinValue": "min_value", "MaxValue": "max_value",
       "DefaultRowHeight": "default_row_height", "DefaultColWidth": "default_col_width", "FixedRows": "fixed_rows",
       "FixedCols": "fixed_cols", "TabIndex": "tab_index", "ItemHeight": "item_height", "Spacing": "spacing",
       "Frequency": "frequency", "DropDownCount": "drop_down_count", "BevelWidth": "bevel_width", "BorderWidth": "border_width",
       "Indent": "indent", "ModalResult": "modal_result", "Constraints.MinWidth": "min_width",
       "Constraints.MinHeight": "min_height", "Constraints.MaxWidth": "max_width", "Constraints.MaxHeight": "max_height",
       "Pen.Width": "pen_width", "SelStart": "sel_start", "SelEnd": "sel_end", "AllocBy": "alloc_by", "FilterIndex": "filter_index",
       "MinSize": "min_size", "MinFontSize": "min_font_size", "MaxFontSize": "max_font_size", "LineWidth": "line_width",
       "ShortCut": "shortcut"}
BOOL = {"Visible": "visible", "Enabled": "enabled", "TabStop": "tabstop", "ReadOnly": "readonly", "Checked": "checked",
        "AutoSize": "autosize", "Transparent": "transparent", "WordWrap": "wordwrap", "Default": "default_btn", "Cancel": "cancel",
        "Flat": "flat", "Down": "down", "MultiSelect": "multiselect", "AllowAllUp": "allow_all_up", "Wrap": "wrap",
        "ShowHint": "show_hint", "ParentShowHint": "parent_show_hint", "ParentFont": "parent_font", "ParentColor": "parent_color",
        "RowSelect": "row_select", "GridLines": "grid_lines", "HideSelection": "hide_selection", "ColumnClick": "column_click",
        "KeyPreview": "key_preview", "Stretch": "stretch", "Smooth": "smooth", "MultiLine": "multiline",
        "FullRepaint": "full_repaint", "WantReturns": "want_returns", "Thousands": "thousands", "OwnerDraw": "owner_draw",
        "OwnerData": "owner_data", "ShowColumnHeaders": "show_column_headers", "Sorted": "sorted", "AutoSelect": "auto_select",
        "AutoSnap": "auto_snap", "Beveled": "beveled", "Checkboxes": "checkboxes", "ShowRoot": "show_root",
        "ShowLines": "show_lines", "RightClickSelect": "right_click_select", "SimplePanel": "simple_panel",
        "DefaultDrawing": "default_drawing", "AutoHotkeys": "auto_hotkeys", "PlainText": "plain_text", "ToolTips": "tool_tips",
        "FullDrag": "full_drag", "DragReorder": "drag_reorder", "AutoLineReduction": "auto_line_reduction",
        "IconOptions.AutoArrange": "auto_arrange", "RadioItem": "radio_item", "Break": "break_item", "Center": "center",
        "ParentBiDiMode": "parent_bidi_mode", "AutoCheck": "auto_check"}
STR = {"Caption": "caption", "Hint": "hint", "Text": "text", "Action": "action", "PopupMenu": "popup_menu", "Filter": "filter",
       "DefaultExt": "default_ext", "Title": "title", "EditMask": "edit_mask", "PasswordChar": "password_char", "Kind": "kind_str",
       "Style": "style", "ViewStyle": "view_style", "Shape": "shape", "Layout": "layout", "BorderStyle": "border_style",
       "ScrollBars": "scroll_bars", "DragMode": "drag_mode", "State": "state", "TickStyle": "tick_style", "TickMarks": "tick_marks",
       "Orientation": "orientation", "CharCase": "char_case", "TextCase": "text_case", "SortType": "sort_type",
       "ResizeStyle": "resize_style", "Options": "options", "Category": "category", "Mask": "mask", "Associate": "associate",
       "FocusControl": "focus_control", "ActivePage": "active_page", "ActiveControl": "active_control", "Images": "images",
       "FileList": "file_list", "DirList": "dir_list", "BiDiMode": "bidi_mode"}
COLOR = {"Color": "color", "Font.Color": "font_color", "Brush.Color": "brush_color", "Pen.Color": "pen_color",
         "BaseColor": "base_color", "DipColor": "dip_color", "TextColor": "text_color"}
ART = {"Glyph.Data": "glyph", "Picture.Data": "picture", "Icon.Data": "picture", "Bitmap": "picture"}
ITEMS = {"Items.Strings", "Lines.Strings", "Tabs.Strings"}
COLLS = {"Columns", "Sections", "Panels"}
# Action -> control/menu-item slots the action's facts fill when the control did not say
ACTION_SLOTS = (("Caption", "caption"), ("Hint", "hint"), ("ShortCut", "shortcut"), ("Enabled", "enabled"),
                ("Checked", "checked"), ("ImageIndex", "image_index"), ("GroupIndex", "group_index"))
# form-level slots beyond the shared ones
FORM_INT = {"Left": "left", "Top": "top", "Width": "width", "Height": "height", "ClientWidth": "client_width",
            "ClientHeight": "client_height", "TextHeight": "text_height", "Constraints.MinWidth": "min_width",
            "Constraints.MinHeight": "min_height", "Constraints.MaxWidth": "max_width", "Constraints.MaxHeight": "max_height",
            "Tag": "tag", "BorderWidth": "border_width"}
FORM_BOOL = {"KeyPreview": "key_preview", "Visible": "visible", "Enabled": "enabled", "ShowHint": "show_hint",
             "AutoScroll": "auto_scroll", "HorzScrollBar.Visible": "horz_scrollbar_visible",
             "VertScrollBar.Visible": "vert_scrollbar_visible", "AutoSize": "autosize", "ParentFont": "parent_font"}
FORM_STR = {"Caption": "caption", "Menu": "menu", "PopupMenu": "popup_menu", "ActiveControl": "active_control", "Hint": "hint",
            "PrintScale": "print_scale"}
FORM_COLOR = {"Color": "color", "Font.Color": "font_color"}
FONT_ID = {("MS Sans Serif", -11, 0): "LJ_FONT_SANS13", ("MS Sans Serif", -11, 1): "LJ_FONT_SANS13B",
           ("Arial", -11, 0): "LJ_FONT_ARIAL14", ("Arial", -11, 1): "LJ_FONT_ARIAL14B", ("Arial Black", -13, 0): "LJ_FONT_ARIALBLACK18"}
MA_BOOL = {"AutoHotkeys", "AutoLineReduction"}          # maAutomatic / maManual -> LJ_YES / LJ_NO
MENU_INT = {"ShortCut": "shortcut", "Tag": "tag", "GroupIndex": "group_index", "ImageIndex": "image_index"}
MENU_BOOL = {"Checked": "checked", "RadioItem": "radio_item", "Enabled": "enabled", "Visible": "visible", "Default": "default_item",
             "Break": "break_item", "AutoCheck": "auto_check"}


def cstr(b):
    """CP1252 bytes -> C string literal, ASCII only (octal escapes cannot run into the next char; ? dodges trigraphs)."""
    out = ['"']
    for ch in b:
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append("\\\\")
        elif ch == 0x3F:
            out.append("\\?")
        elif 0x20 <= ch < 0x7F:
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    out.append('"')
    return "".join(out)


def cbool(v):
    return "LJ_YES" if v == ("ident", "True") else "LJ_NO"


def ctext(v):
    """Any scalar as a verbatim string fact (identifiers, sets, numbers)."""
    tag, p = v
    if tag == "str":
        return cstr(p)
    if tag == "set":
        return cstr(("[" + ", ".join(p) + "]").encode("cp1252"))
    if tag in ("ident", "float"):
        return cstr(p.encode("cp1252"))
    if tag == "int":
        return cstr(str(p).encode())
    raise ValueError("not a scalar: %r" % (v,))


def font_id(font):
    bold = 1 if "fsBold" in font["style"] else 0
    return FONT_ID.get((font["name"], font["height"], bold), "LJ_FONT_OTHER")


def events_text(form, name, events):
    if len(events) > EVENTS_MAX:
        raise SystemExit("%s.%s: %d events > LJ_EVENTS_MAX %d" % (form, name, len(events), EVENTS_MAX))
    return "{ %s }" % ", ".join('{ "%s", "%s" }' % e for e in events)


def walk(o):
    yield o
    for k in o.children:
        yield from walk(k)


def decode_listitems(data):
    """Delphi TListItems stream (TItemHeader + TItemInfo records, length-byte strings) -> [b'caption\\tsub\\tsub', ...].
    Raises ValueError when the bytes stop short (the dfm_text extraction kept 20 bytes of each blob)."""
    if len(data) < 8:
        raise ValueError("stream header short (%d bytes)" % len(data))
    size, count = int.from_bytes(data[0:4], "little"), int.from_bytes(data[4:8], "little")
    if len(data) < size:
        raise ValueError("truncated stream: %d of %d bytes, %d items" % (len(data), size, count))
    pos, out = 8, []
    for _ in range(count):
        # TItemInfo: ImageIndex, StateIndex, OverlayIndex, SubItemCount: Integer; Data: Pointer; then the strings
        nsub = int.from_bytes(data[pos + 12:pos + 16], "little", signed=True)
        pos += 20
        parts = []
        for _ in range(nsub + 1):
            ln = data[pos]
            parts.append(data[pos + 1:pos + 1 + ln])
            pos += 1 + ln
        out.append(b"\t".join(parts))
    return out


def read_lines(path):
    if not os.path.exists(path):
        return []
    return [l.strip() for l in open(path, encoding="utf-8").read().splitlines() if l.strip() and not l.startswith("#")]


# ----------------------------------------------------------------------------- generation
class Gen:
    def __init__(self, repo):
        self.repo = repo
        self.drop = set(read_lines(os.path.join(repo, DROP_TXT)))
        self.glyphmap = {}                       # (form, object) -> name
        for line in read_lines(os.path.join(repo, GLYPHMAP_TSV)):
            parts = line.split("\t")
            if len(parts) >= 3 and parts[0] != "form":
                self.glyphmap[(parts[0], parts[1])] = parts[2]
        self.unknown = {}                        # "TClass.Prop" -> count
        self.blobs = []                          # (form, object, class, prop, disposition)
        self.census = {}                         # class -> count
        self.c = []                              # output chunks
        self.forms = []                          # (FILESTEM, name, class)
        self.inputs = []                         # (file, sha256)

    def warn_unknown(self, cls, key):
        k = cls + "." + key
        self.unknown[k] = self.unknown.get(k, 0) + 1

    def color(self, v, cls, key):
        tag, p = v
        if tag == "ident" and p in COLORS:
            return "LJ_CLR(0x%06X)" % COLORS[p]
        if tag == "int":                        # $00BBGGRR
            p &= 0xFFFFFF
            return "LJ_CLR(0x%06X)" % (((p & 0xFF) << 16) | (p & 0xFF00) | (p >> 16))
        self.warn_unknown(cls, key + ("=" + p if tag == "ident" else ""))
        return None

    def art(self, form, obj, key, v):
        data, trunc = v[1]
        name = self.glyphmap.get((form, obj.name))
        if name:
            self.blobs.append((form, obj.name, obj.cls, key, "glyph:" + name))
            return cstr(name.encode())
        self.blobs.append((form, obj.name, obj.cls, key,
                           "dropped:no-glyphmap-row (%d bytes%s)" % (len(data), ", truncated" if trunc else "")))
        return '"missing"'

    def resolve_action(self, cls, s, actions, ref):
        act = actions.get(ref)
        if act is None:
            self.warn_unknown(cls, "Action=" + ref + " (no TAction on the form)")
            return
        for k, slot in ACTION_SLOTS:
            av = act.get(k)
            if av is not None and slot not in s:
                s[slot] = ctext(av) if slot in ("caption", "hint") else (cbool(av) if slot in ("enabled", "checked") else str(av[1]))

    def columns(self, form, obj, key, items, prefix):
        rows = []
        for props in items:
            d = {}
            for k, v in props:
                if k in ("Caption", "Text"):
                    d["caption"] = ctext(v)
                elif k == "Width":
                    d["width"] = str(v[1])
                elif k == "WidthType":              # TListColumn stores WidthType for Width = -1/-2: the same fact
                    d["width"] = str(v[1][0][1])
                elif k == "Alignment" and v[1] in TALIGNS:
                    d["alignment"] = TALIGNS[v[1]]
                elif k == "AutoSize":
                    d["autosize"] = cbool(v)
                elif k == "AllowClick":
                    d["allow_click"] = cbool(v)
                elif k == "ImageIndex":
                    d["image_index"] = str(v[1])
                else:
                    self.warn_unknown(obj.cls, key + "." + k)
            d.setdefault("image_index", "-1")
            rows.append("    { " + ", ".join(".%s = %s" % kv for kv in d.items()) + " },")
        if not rows:
            return None
        self.c.append("static const lj_column_desc %s_cols[] = {\n%s\n};" % (prefix, "\n".join(rows)))
        return "%s_cols, .ncolumns = LJ_COUNT_OF(%s_cols)" % (prefix, prefix)

    def row(self, form, obj, parent_font, actions, prefix):
        """Emit helper arrays for obj (children first) and return the row initializer text."""
        cls = obj.cls
        kind = KIND_ENUM.get(cls)
        if kind is None:
            kind = "LJ_K_UNKNOWN"
            self.warn_unknown(cls, "(class)")
        self.census[cls] = self.census.get(cls, 0) + 1
        s = {}                                   # slot -> C text, insertion ordered
        items, events = [], []
        geom = {"Left": 0, "Top": 0, "Width": 0, "Height": 0}
        font = dict(parent_font)
        own_font = False
        for key, v in obj.props:
            tag, p = v
            if key in self.drop:
                continue
            if key in geom and tag == "int":
                geom[key] = p
            elif key.startswith("On") and tag == "ident":
                events.append((key, p))
            elif key in INT and tag == "int":
                s[INT[key]] = str(p)
            elif key in BOOL and tag == "ident" and p in ("True", "False"):
                s[BOOL[key]] = cbool(v)
            elif key in MA_BOOL and tag == "ident" and p in ("maAutomatic", "maManual"):
                s[BOOL[key]] = "LJ_YES" if p == "maAutomatic" else "LJ_NO"
            elif key == "Columns" and tag == "int":          # TRadioGroup/TListBox Columns = n
                s["columns_count"] = str(p)
            elif key in COLOR:
                c = self.color(v, cls, key)
                if c:
                    s[COLOR[key]] = c
            elif key in STR and tag in ("str", "ident", "set", "int", "float"):
                s[STR[key]] = ctext(v)
            elif key == "Font.Name" and tag == "str":
                font["name"] = p.decode("cp1252")
                own_font = True
            elif key == "Font.Height" and tag == "int":
                font["height"] = p
                own_font = True
            elif key == "Font.Style" and tag == "set":
                font["style"] = p
                own_font = True
            elif key == "Align" and tag == "ident" and p in ALIGNS:
                s["align"] = ALIGNS[p]
            elif key == "Anchors" and tag == "set" and all(a in ANCHORS for a in p):
                s["anchors"] = " | ".join(ANCHORS[a] for a in p) if p else "0"
            elif key in ("BevelInner", "BevelOuter") and tag == "ident" and p in BEVELS:
                s["bevel_inner" if key == "BevelInner" else "bevel_outer"] = BEVELS[p]
            elif key == "Alignment" and tag == "ident" and p in TALIGNS:
                s["alignment"] = TALIGNS[p]
            elif key in ITEMS and tag == "list" and all(x[0] == "str" for x in p):
                items.extend(x[1] for x in p)
            elif key == "Items.Data" and tag == "blob":
                try:
                    items.extend(decode_listitems(p[0]))
                    self.blobs.append((form, obj.name, cls, key, "decoded"))
                except (ValueError, IndexError) as e:
                    self.blobs.append((form, obj.name, cls, key, "dropped:%s" % e))
            elif key in ART and tag == "blob":
                s[ART[key]] = self.art(form, obj, key, v)
            elif key in COLLS and tag == "coll":
                cols = self.columns(form, obj, key, p, prefix)
                if cols:
                    s["columns"] = cols
            elif key == "RowHeights" and tag == "list" and all(x[0] == "int" for x in p):
                self.c.append("static const int %s_rowh[] = { %s };" % (prefix, ", ".join(str(x[1]) for x in p)))
                s["row_heights"] = "%s_rowh" % prefix
                s["nrow_heights"] = str(len(p))
            else:
                self.warn_unknown(cls, key)
        if "action" in s:
            self.resolve_action(cls, s, actions, obj.get("Action")[1])
        s["font"] = font_id(font)               # own Font.* or the parent's, MS Sans Serif -11 at the root (design 7.4)
        if own_font:
            s["font_name"] = cstr(font["name"].encode("cp1252"))
            s["font_height"] = str(font["height"])
            s["font_style"] = " | ".join(FONT_STYLES[x] for x in font["style"]) if font["style"] else "0"
        else:
            s["font_inherited"] = "1"
        for slot, dflt in INT_DEFAULTS.get(cls, {}).items():
            s.setdefault(slot, str(dflt))
        if items:
            self.c.append("static const char *const %s_items[] = {\n    %s,\n};" % (prefix, ",\n    ".join(cstr(x) for x in items)))
            s["items"] = "%s_items" % prefix
            s["nitems"] = "LJ_COUNT_OF(%s_items)" % prefix
        kids = [k for k in obj.children if k.cls != "TMenuItem"]
        if kids:
            rows = [self.row(form, k, font, actions, prefix + "_" + k.name.lower()) for k in kids]
            self.c.append("static const lj_ctl_desc %s[] = {\n%s\n};" % (prefix, "\n".join(rows)))
            s["children"] = prefix
            s["nchildren"] = "LJ_COUNT_OF(%s)" % prefix
        if events:
            s["events"] = events_text(form, obj.name, events)
        head = "    { %s, %s, %d, %d, %d, %d" % (kind, cstr(obj.name.encode()), geom["Left"], geom["Top"], geom["Width"], geom["Height"])
        return head + "".join(", .%s = %s" % kv for kv in s.items()) + " },"

    def menu_items(self, form, obj, actions, prefix):
        rows = []
        for it in obj.children:
            self.census["TMenuItem"] = self.census.get("TMenuItem", 0) + 1
            d, events = {}, []
            for key, v in it.props:
                tag, p = v
                if key in self.drop:
                    continue
                if key == "Caption" and tag == "str":
                    d["caption"] = cstr(p)
                elif key == "OnClick" and tag == "ident":
                    d["on_click"] = cstr(p.encode())
                elif key in ("Hint", "Action") and tag in ("str", "ident"):
                    d[key.lower()] = ctext(v)
                elif key in MENU_INT and tag == "int":
                    d[MENU_INT[key]] = str(p)
                elif key in MENU_BOOL and tag == "ident" and p in ("True", "False"):
                    d[MENU_BOOL[key]] = cbool(v)
                elif key in MA_BOOL and tag == "ident" and p in ("maAutomatic", "maManual"):
                    d[BOOL[key]] = "LJ_YES" if p == "maAutomatic" else "LJ_NO"
                elif key.startswith("On") and tag == "ident":
                    events.append((key, p))
                else:
                    self.warn_unknown("TMenuItem", key)
            if "action" in d:
                self.resolve_action("TMenuItem", d, actions, it.get("Action")[1])
            d.setdefault("image_index", "-1")
            if it.children:
                sub = prefix + "_" + it.name.lower()
                self.menu_items(form, it, actions, sub)
                d["children"] = sub
                d["nchildren"] = "LJ_COUNT_OF(%s)" % sub
            if events:
                d["events"] = events_text(form, it.name, events)
            rows.append("    { .name = %s%s }," % (cstr(it.name.encode()), "".join(", .%s = %s" % kv for kv in d.items())))
        self.c.append("static const lj_menu_item_desc %s[] = {\n%s\n};" % (prefix, "\n".join(rows) if rows else "    { 0 },"))
        return len(rows)

    def form(self, stem, root):
        form = stem
        self.census["(form)"] = self.census.get("(form)", 0) + 1
        actions = {a.name: a for a in walk(root) if a.cls == "TAction"}
        s, events, raw = {}, [], {}
        font = {"name": "MS Sans Serif", "height": -11, "style": []}
        icons = None
        for key, v in root.props:
            tag, p = v
            if key in self.drop:
                continue
            if key.startswith("On") and tag == "ident":
                events.append((key, p))
            elif key in FORM_INT and tag == "int":
                s[FORM_INT[key]] = str(p)
                raw[key] = p
            elif key in FORM_BOOL and tag == "ident":
                s[FORM_BOOL[key]] = cbool(v)
            elif key in FORM_STR and tag in ("str", "ident"):
                s[FORM_STR[key]] = ctext(v)
            elif key in FORM_COLOR:
                c = self.color(v, root.cls, key)
                if c:
                    s[FORM_COLOR[key]] = c
            elif key == "Font.Name" and tag == "str":
                font["name"] = p.decode("cp1252")
            elif key == "Font.Height" and tag == "int":
                font["height"] = p
            elif key == "Font.Style" and tag == "set":
                font["style"] = p
            elif key == "BorderIcons" and tag == "set" and all(x in BORDER_ICONS for x in p):
                icons = " | ".join(["LJ_BI_GIVEN"] + [BORDER_ICONS[x] for x in p])
            elif key == "BorderStyle" and tag == "ident" and p in BORDER_STYLES:
                s["border"] = BORDER_STYLES[p]
            elif key == "FormStyle" and tag == "ident" and p in FORM_STYLES:
                s["style"] = FORM_STYLES[p]
            elif key == "Position" and tag == "ident" and p in POSITIONS:
                s["position"] = POSITIONS[p]
            elif key == "Icon.Data" and tag == "blob":
                s["icon"] = self.art(form, root, key, v)
            else:
                self.warn_unknown(root.cls, key)
        if icons:
            s["icons"] = icons
        s["font"] = font_id(font)
        s["font_name"] = cstr(font["name"].encode("cp1252"))
        s["font_height"] = str(font["height"])
        s["font_style"] = " | ".join(FONT_STYLES[x] for x in font["style"]) if font["style"] else "0"
        if "ClientWidth" not in raw and "Width" in raw:
            # AutoScroll forms store the window size: derive the client from the classic frame metrics
            fx, cap = FRAME[s.get("border", "LJ_BS_SIZEABLE")]
            s["client_width"] = str(raw["Width"] - 2 * fx)
            s["client_height"] = str(raw["Height"] - 2 * fx - cap - (MENU_BAR if "menu" in s else 0))
            s["client_derived"] = "1"
        pfx = stem.lower()
        rows = [self.row(form, k, font, actions, pfx + "_" + k.name.lower()) for k in root.children if k.cls != "TMenuItem"]
        if rows:
            self.c.append("static const lj_ctl_desc %s_ctls[] = {\n%s\n};" % (pfx, "\n".join(rows)))
            s["ctls"] = "%s_ctls" % pfx
            s["n"] = "LJ_COUNT_OF(%s_ctls)" % pfx
        menus = []
        for m in walk(root):
            if m.cls in ("TMainMenu", "TPopupMenu"):
                n = self.menu_items(form, m, actions, pfx + "_" + m.name.lower())
                menus.append('    { "%s", %d, %s_%s, %d },' % (m.name, 1 if m.cls == "TMainMenu" else 0, pfx, m.name.lower(), n))
        if menus:
            self.c.append("static const lj_menu_desc %s_menus[] = {\n%s\n};" % (pfx, "\n".join(menus)))
            s["menus"] = "%s_menus" % pfx
            s["nmenus"] = "LJ_COUNT_OF(%s_menus)" % pfx
        if events:
            s["events"] = events_text(form, root.name, events)
        self.c.append("const lj_form_desc lj_form_%s = {\n    .name = %s, .class_name = %s,\n%s\n};" % (
            stem, cstr(root.name.encode()), cstr(root.cls.encode()), ",\n".join("    .%s = %s" % kv for kv in s.items())))
        self.forms.append((stem, root.name, root.cls))

    def run(self):
        dfm_dir = os.path.join(self.repo, DFM_DIR)
        for fn in sorted(f for f in os.listdir(dfm_dir) if f.endswith(".dfm.txt")):
            data = open(os.path.join(dfm_dir, fn), "rb").read()
            self.inputs.append((fn, hashlib.sha256(data).hexdigest()))
            stem = fn[:-len(".dfm.txt")]
            self.c.append("\n/* ---- %s ---- */" % fn)
            try:
                self.form(stem, parse_dfm(data.decode("utf-8")))     # the dfm_text extraction is UTF-8; strings go out as CP1252
            except ValueError as e:
                raise SystemExit("%s: %s" % (fn, e))
        n = len(self.forms)
        banner = ["/* lj_forms_gen.c - GENERATED by tools/dfm2c.py %s; do not edit." % GEN_VERSION,
                  " * Facts harvested from the Delphi text DFMs (geometry, captions verbatim with Martin's typos,",
                  " * menu trees, event names); no artwork [D2, D13]. Inputs (sha256):"]
        banner += [" *   %s %s" % (h, f) for f, h in self.inputs]
        banner += [" */", '#include "ui/forms/lj_forms_gen.h"', ""]
        table = ["", "static const lj_form_desc *const lj_forms_table[LJ_FORMS_COUNT] = {"]
        table += ["    &lj_form_%s," % f[0] for f in self.forms]
        table += ["};", "", "const lj_form_desc *const *lj_forms_all(int *count)", "{",
                  "    if (count) *count = LJ_FORMS_COUNT;", "    return lj_forms_table;", "}", ""]
        out_c = "\n".join(banner + self.c + table)
        out_h = "\n".join(
            ["/* lj_forms_gen.h - GENERATED by tools/dfm2c.py %s; do not edit. One lj_form_desc per DFM form;" % GEN_VERSION,
             " * lj_forms_all() (lj_form.h) returns them in file-name order. */",
             "#ifndef LJ_UI_FORMS_FORMS_GEN_H", "#define LJ_UI_FORMS_FORMS_GEN_H", "",
             '#include "ui/forms/lj_form.h"', "", "#define LJ_FORMS_COUNT %d" % n, ""]
            + ["extern const lj_form_desc lj_form_%s;%s/* %s: %s */" % (f[0], " " * max(1, 44 - len(f[0])), f[1], f[2]) for f in self.forms]
            + ["", "#endif /* LJ_UI_FORMS_FORMS_GEN_H */", ""])
        out_unknown = "\n".join(["# dfm2c.py: DFM properties with no slot in lj_form.h, as class.property count. Must stay empty."]
                                + ["%s %d" % kv for kv in sorted(self.unknown.items())]) + "\n"
        out_blobs = "\n".join(["# dfm2c.py: every binary blob in the DFMs and what became of it.",
                               "# form\tobject\tclass\tproperty\tdisposition (glyph:<name> | decoded | dropped:<reason>)"]
                              + ["\t".join(b) for b in self.blobs]) + "\n"
        out_census = "\n".join(["class\tcount"] + ["%s\t%d" % kv for kv in sorted(self.census.items(), key=lambda kv: (-kv[1], kv[0]))]) + "\n"
        return {GEN_C: out_c, GEN_H: out_h, UNKNOWN_TXT: out_unknown, BLOBS_TXT: out_blobs, CENSUS_TSV: out_census}


def selftest():
    """The parser corners the corpus does not exercise uniformly: unescaped inner quotes, #nn codes,
    ' + ' joins, the standard multi-line list/collection/blob forms, and the TListItems decoder."""
    r = parse_dfm("object F: TF\n  Caption = ''Off' DMX value'\n  Hint = 'a'#39'b'#13#10 +\n    'c'\n"
                  "  Items.Strings = (\n    'x'\n    'keep 'em coming.'\n    '')\n"
                  "  Columns = <\n    item\n      Caption = 'L #'\n      Width = 32\n    end>\n"
                  "  Glyph.Data = {\n    0001\n    0203}\n  Anchors = [akLeft, akRight]\n  Font.Height = -11\n"
                  "  object C: TLabel\n    Columns = <<Caption='a b' Width=-2 WidthType=(-2)> <Text='q'>>\n  end\nend\n")
    assert r.get("Caption") == ("str", b"'Off' DMX value"), r.get("Caption")
    assert r.get("Hint") == ("str", b"a'b\r\nc"), r.get("Hint")
    assert [x[1] for x in r.get("Items.Strings")[1]] == [b"x", b"keep 'em coming.", b""], r.get("Items.Strings")
    assert r.get("Columns") == ("coll", [[("Caption", ("str", b"L #")), ("Width", ("int", 32))]]), r.get("Columns")
    assert r.get("Glyph.Data") == ("blob", (b"\x00\x01\x02\x03", False))
    assert r.get("Anchors") == ("set", ["akLeft", "akRight"]) and r.get("Font.Height") == ("int", -11)
    assert r.children[0].get("Columns")[1] == [[("Caption", ("str", b"a b")), ("Width", ("int", -2)), ("WidthType", ("list", [("int", -2)]))],
                                               [("Text", ("str", b"q"))]], r.children[0].get("Columns")
    stream = (34).to_bytes(4, "little") + (1).to_bytes(4, "little") + b"\0\0\0\0" + b"\xff\xff\xff\xff" * 2 \
        + (1).to_bytes(4, "little") + b"\0\0\0\0" + b"\x03abc" + b"\x02de"
    assert decode_listitems(stream) == [b"abc\tde"], decode_listitems(stream)
    try:
        decode_listitems(stream[:20])
        raise AssertionError("truncated stream not rejected")
    except ValueError:
        pass
    assert cstr(b'a"\\?\xe7\x01') == '"a\\"\\\\\\?\\347\\001"', cstr(b'a"\\?\xe7\x01')
    print("dfm2c --selftest: OK")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--selftest", action="store_true", help="parser and decoder unit checks, no reference tree needed")
    a = ap.parse_args()
    if a.selftest:
        selftest()
        return 0
    if not os.path.isdir(os.path.join(a.repo, DFM_DIR)):
        print("dfm2c: no reference tree at %s" % os.path.join(a.repo, DFM_DIR))
        return 2
    outputs = Gen(a.repo).run()
    bad = 0
    for rel, text in outputs.items():
        path = os.path.join(a.repo, rel)
        if a.check:
            old = open(path, "rb").read().decode("utf-8") if os.path.exists(path) else None
            if old != text:
                bad += 1
                print("dfm2c --check: %s differs from the committed copy" % rel)
                if old is not None:
                    o, n = old.splitlines(), text.splitlines()
                    for i, (x, y) in enumerate(zip(o, n)):
                        if x != y:
                            print("  first difference at line %d:\n  - %s\n  + %s" % (i + 1, x[:200], y[:200]))
                            break
                    else:
                        print("  line count %d -> %d" % (len(o), len(n)))
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(text.encode("utf-8"))
            print("wrote %s" % rel)
    if a.check:
        print("dfm2c --check: %s" % ("OK" if not bad else "%d file(s) differ" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
