/* lj_form.c - helpers over the generated form tables (design 7.6). */
#include "ui/forms/lj_form.h"

#include <string.h>

const char *const lj_ctl_class_names[LJ_K_COUNT] = {
    "?",
    "TLabel", "TPanel", "TEdit", "TBitBtn", "TSpeedButton", "TCheckBox", "TBevel",
    "TListView", "TTabSheet", "TUpDown", "TButton", "TShape", "TRadioGroup", "TRichEdit",
    "TComboBox", "TProgressBar", "TPageControl", "TGroupBox", "TUdButton", "TStaticText",
    "TImage", "TPaintBox", "TSplitter", "TScrollBar", "TSpinEdit", "TRadioButton",
    "TStringGrid", "TNumEdit", "TTabControl", "TODPanel2", "TMemo", "TListBox", "TMaskEdit",
    "TDrawGrid", "TCheckListBox", "TTreeView", "TDipSwitch", "TTrackBar", "TStatusBar",
    "THeaderControl",
    "TDriveComboBox", "TDirectoryListBox", "TFileListBox", "TODPanel",
    "TMainMenu", "TPopupMenu",
    "TTimer",
    "TOpenDialog", "TSaveDialog", "TOpenPictureDialog", "TColorDialog", "TFontDialog",
    "TPrintDialog", "TPrinterSetupDialog",
    "TImageList", "TActionList", "TAction",
};

static const lj_ctl_desc *find_in(const lj_ctl_desc *rows, int n, const char *name)
{
    int i;
    const lj_ctl_desc *hit;
    for (i = 0; i < n; i++) {
        if (rows[i].name && strcmp(rows[i].name, name) == 0) return &rows[i];
        if (rows[i].children && (hit = find_in(rows[i].children, rows[i].nchildren, name)) != NULL) return hit;
    }
    return NULL;
}

const lj_ctl_desc *lj_form_find_ctl(const lj_form_desc *f, const char *name)
{
    if (!f || !name) return NULL;
    return find_in(f->ctls, f->n, name);
}

const lj_menu_desc *lj_form_find_menu(const lj_form_desc *f, const char *name)
{
    int i;
    if (!f || !name) return NULL;
    for (i = 0; i < f->nmenus; i++)
        if (f->menus[i].name && strcmp(f->menus[i].name, name) == 0) return &f->menus[i];
    return NULL;
}

static int count_in(const lj_ctl_desc *rows, int n)
{
    int i, total = n;
    for (i = 0; i < n; i++)
        if (rows[i].children) total += count_in(rows[i].children, rows[i].nchildren);
    return total;
}

int lj_form_count_ctls(const lj_form_desc *f)
{
    return f ? count_in(f->ctls, f->n) : 0;
}
