/* lj_form_example.c - HAND-WRITTEN example of the exact shape tools/dfm2c.py must emit.
 *
 * lj_form_TSEQUENCEFORM transcribed from
 *   reference/installer/exe_resources/dfm_text/TSEQUENCEFORM.dfm.txt
 * following the slot rules in lj_form.h (tri-state bools, LJ_CLR colours, VCL
 * defaults for non-zero ints, Delphi enum identifiers verbatim as strings,
 * dropped: OldCreateOrder, PixelsPerInch, Cursor, Font.Charset).
 *
 * The generator overwrites this file's role with lj_forms_gen.c (all 208 forms
 * and the real lj_forms_all()); when that lands, DELETE this file. Geometry,
 * captions and event names are facts from the DFM; nothing here is artwork. */
#include "ui/forms/lj_form.h"

static const lj_ctl_desc tsequenceform_panel1[] = {
    { LJ_K_BEVEL,       "Bevel3",        1,   2, 350,  33 },
    { LJ_K_SPEEDBUTTON, "sb1",         133,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb2",         159,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb3",           5,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb4",          31,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb5",         298,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb6",         324,   6,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb7",         243,   6,  51,  25, .num_glyphs = 1 },
    { LJ_K_BEVEL,       "Elapsed",      59,   6,  73,  25, .parent_show_hint = LJ_NO, .show_hint = LJ_NO },
    { LJ_K_LABEL,       "Label5",        4,  41,  83,  13, .caption = "Scene Time (sec)" },
    { LJ_K_LABEL,       "Label6",        4,  65,  78,  13, .caption = "Fade Time (Sec)" },
    { LJ_K_SPEEDBUTTON, "sb8",         225,  78,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb9",         250,  78,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb10",        275,  78,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "sb11",        300,  78,  25,  25, .num_glyphs = 1 },
    { LJ_K_SPEEDBUTTON, "seqBlind",    326,  78,  25,  25, .num_glyphs = 1 },
    { LJ_K_LABEL,       "lBPM",        163,  41,  64,  13, .caption = "XXXX.X BPM" },
    { LJ_K_EDIT,        "ES",           95,  37,  40,  21, .tab_order = 0, .text = "88:88" },
    { LJ_K_EDIT,        "EF",           95,  61,  40,  21, .tab_order = 1, .text = "88:88" },
    { LJ_K_CHECKBOX,    "cbSync",        4,  86, 195,  17, .tabstop = LJ_NO, .caption = "S&ynchronize Scene and Fade Times", .tab_order = 3,
                                                           .events = { { "OnClick", "cbSyncClick" }, { "OnEnter", "cbSyncEnter" } } },
    { LJ_K_CHECKBOX,    "cbGlobalFade", 239,  58, 113,  17, .tabstop = LJ_NO, .caption = "Global &Fade Time", .tab_order = 2,
                                                           .events = { { "OnClick", "cbGlobalFadeClick" }, { "OnEnter", "cbSyncEnter" } } },
    { LJ_K_CHECKBOX,    "cbGlobalScene", 239, 39, 113,  17, .tabstop = LJ_NO, .caption = "Global &Scene Time", .tab_order = 7,
                                                           .events = { { "OnClick", "cbGlobalSceneClick" }, { "OnEnter", "cbSyncEnter" } } },
    { LJ_K_UDBUTTON,    "UdScene",     137,  37,  20,  21, .enabled = LJ_YES, .events = { { "OnChange", "UdSceneChange" } } },
    { LJ_K_UDBUTTON,    "UdFade",      136,  61,  20,  21, .enabled = LJ_YES, .events = { { "OnChange", "UdFadeChange" } } },
    { LJ_K_PANEL,       "Panel2",      186,   6,  54,  25, .bevel_inner = LJ_BV_LOWERED, .bevel_outer = LJ_BV_LOWERED, .tab_order = 4, .bevel_width = 1 },
};

static const lj_ctl_desc tsequenceform_ctls[] = {
    { LJ_K_LABEL,    "Label1",    7,  2,  26,  13, .caption = "Seq#" },
    { LJ_K_LABEL,    "Label2",   47,  2,  80,  13, .caption = "Sequence Name" },
    { LJ_K_LABEL,    "Label3",  215,  2,  76,  13, .caption = "Sequence Type" },
    { LJ_K_EDIT,     "ESeqNo",    7, 15,  33,  21, .tabstop = LJ_NO, .color = LJ_CLR(0xC0C0C0), .readonly = LJ_YES, .tab_order = 0 },
    { LJ_K_EDIT,     "ESeqName", 47, 15, 160,  21, .tabstop = LJ_NO, .color = LJ_CLR(0xC0C0C0), .readonly = LJ_YES, .tab_order = 1 },
    { LJ_K_EDIT,     "ESeqType", 215, 15, 145, 21, .tabstop = LJ_NO, .color = LJ_CLR(0xC0C0C0), .readonly = LJ_YES, .tab_order = 2 },
    { LJ_K_PANEL,    "Panel1",    0,  0, 354, 106, .bevel_outer = LJ_BV_NONE, .tab_order = 3, .bevel_width = 1,
                                                   .children = tsequenceform_panel1, .nchildren = LJ_COUNT_OF(tsequenceform_panel1) },
    { LJ_K_MAINMENU, "MainMenu1", 392, 24, 0, 0 },
};

static const lj_menu_item_desc tsequenceform_preferences1[] = {
    { "M_KeepBlind", "Keep Contents When Entering Blind Mode", .on_click = "M_KeepBlindClick" },
};
static const lj_menu_item_desc tsequenceform_mainmenu1[] = {
    { "DisableFade1", "Disable Fade", .on_click = "DisableFade1Click" },
    { "Preferences1", "&Preferences", .on_click = "Preferences1Click",
      .children = tsequenceform_preferences1, .nchildren = LJ_COUNT_OF(tsequenceform_preferences1) },
    { "M_Snapshot",   "Snapshot",     .on_click = "M_SnapShotClick" },
    { "Help1",        "&Help",        .on_click = "Help1Click" },
};
static const lj_menu_desc tsequenceform_menus[] = {
    { "MainMenu1", 1, tsequenceform_mainmenu1, LJ_COUNT_OF(tsequenceform_mainmenu1) },
};

const lj_form_desc lj_form_TSEQUENCEFORM = {
    .name = "SequenceForm", .class_name = "TSequenceForm", .caption = "Sequence",
    .client_width = 466, .client_height = 163, .left = 589, .top = 412, .text_height = 13,
    .color = LJ_CLR(0x808080) /* clGray */, .font_color = LJ_CLR(0x000000) /* clWindowText */,
    .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11, .font_style = 0,
    .border = LJ_BS_SINGLE, .style = LJ_FS_STAYONTOP, .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .position = LJ_PO_DESIGNED,
    .menu = "MainMenu1",
    .menus = tsequenceform_menus, .nmenus = LJ_COUNT_OF(tsequenceform_menus),
    .ctls = tsequenceform_ctls, .n = LJ_COUNT_OF(tsequenceform_ctls),
    .events = { { "OnActivate", "FormActivate" }, { "OnClose", "FormClose" }, { "OnCreate", "FormCreate" },
                { "OnDestroy", "FormDestroy" }, { "OnHide", "FormHide" }, { "OnShow", "FormShow" } },
};

/* Replaced by lj_forms_gen.c's table of all 208 forms. */
static const lj_form_desc *const lj_forms_example_all[] = { &lj_form_TSEQUENCEFORM };

const lj_form_desc *const *lj_forms_all(int *count)
{
    if (count) *count = LJ_COUNT_OF(lj_forms_example_all);
    return lj_forms_example_all;
}
