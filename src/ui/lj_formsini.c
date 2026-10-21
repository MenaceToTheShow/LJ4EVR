/* lj_formsini.c - section naming, Left/Top(/Width/Height) persistence, centre-on-owner (design 7.10). */
#include "ui/lj_formsini.h"

#include <string.h>
#include "core/lj_str.h"

/* Known LJ2 sections, class -> section, from the shipped User\Default\forms.ini
 * and Main.Ini [07 §2, 14 §A.7, 06 §files]. Only classes the DFM set identifies
 * by caption/size are listed; [GROUPS], [MOVEPRE], [SAVEBEZFORM] have no DFM
 * class to pin them to and fall through to the class-name rule. */
static const struct { const char *cls, *section; int file; } lj_known[] = {
    { "TSequenceForm",       "SEQ",            LJ_FORMSINI_FORMS },
    { "TCurrentCue",         "CUE",            LJ_FORMSINI_FORMS },
    { "TCuelist2Form",       "CUELIST2",       LJ_FORMSINI_FORMS },
    { "TAdvancedCueForm",    "ACUESELECT",     LJ_FORMSINI_FORMS },
    { "TLoopToolsForm",      "LOOPFORM",       LJ_FORMSINI_FORMS },
    { "TSavePresetForm",     "SAVEPRESETFORM", LJ_FORMSINI_FORMS },
    { "TCueTimeForm",        "CUETIMEFORM",    LJ_FORMSINI_FORMS },
    { "TSaveCueForm",        "SAVECUEFORM",    LJ_FORMSINI_FORMS },
    { "TFixtureSetupForm",   "FIXTURECONF",    LJ_FORMSINI_FORMS },
    { "TStaticsControlForm", "STATICSFORM",    LJ_FORMSINI_FORMS },
    { "TCueMacroControlForm", "MACRO",         LJ_FORMSINI_MAIN },
    { "TMasterIntensityForm", "GLOBALINTENSITY", LJ_FORMSINI_MAIN },
};

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
    }
    return *a == *b;
}

int lj_formsini_sizeable(const lj_form_desc *d)
{
    return d && (d->border == LJ_BS_SIZEABLE || d->border == LJ_BS_SIZETOOLWIN);
}

int lj_formsini_section(const lj_form_desc *d, char *out, size_t cap)
{
    int i;
    const char *cls;
    if (!d || !out || cap == 0) return -1;
    out[0] = 0;
    cls = d->class_name ? d->class_name : "";
    for (i = 0; i < LJ_COUNT_OF(lj_known); i++)
        if (ieq(cls, lj_known[i].cls)) { lj_str_copy(out, cap, lj_known[i].section); return lj_known[i].file; }
    if (d->position != LJ_PO_DESIGNED && d->position != LJ_PO_DEFAULTPOSONLY) return -1;
    lj_str_copy(out, cap, (cls[0] == 'T' || cls[0] == 't') ? cls + 1 : cls);
    for (i = 0; out[i]; i++) if (out[i] >= 'a' && out[i] <= 'z') out[i] = (char)(out[i] - 32);
    return LJ_FORMSINI_FORMS;
}

int lj_formsini_restore(lj_ini *ini, const char *section, int *x, int *y, int *w, int *h)
{
    int l, t, ww, hh;
    if (!ini || !section) return -1;
    l = lj_ini_geti(ini, section, "Left", -32768);
    t = lj_ini_geti(ini, section, "Top", -32768);
    if (l == -32768 || t == -32768) return -1;
    if (x) *x = l;
    if (y) *y = t;
    ww = lj_ini_geti(ini, section, "Width", 0);
    hh = lj_ini_geti(ini, section, "Height", 0);
    if (w && h && ww > 0 && hh > 0) { *w = ww; *h = hh; }
    return 0;
}

void lj_formsini_save(lj_ini *ini, const char *section, int sizeable, int x, int y, int w, int h)
{
    if (!ini || !section) return;
    lj_ini_seti(ini, section, "Left", x);
    lj_ini_seti(ini, section, "Top", y);
    if (sizeable) { lj_ini_seti(ini, section, "Width", w); lj_ini_seti(ini, section, "Height", h); }
}

void lj_formsini_center(int ox, int oy, int ow, int oh, int w, int h, int *x, int *y)
{
    if (x) *x = ox + (ow - w) / 2;
    if (y) *y = oy + (oh - h) / 2;
}
