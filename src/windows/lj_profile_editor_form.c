/* lj_profile_editor_form.c - see lj_profile_editor_form.h. Three windows over the generated TFORMs, every OnXxx their
 * DFMs declare bound (ME-16):
 *  - TScanner2DefsForm "Generic Fixture Profile" (note 16 s4, 02 s5.2-5.4): the working copy is fixlib's raw record
 *    (lj_profile_edit); controls are painted from it and written back to it (lj_pe_set_*), Save / Apply commit it
 *    through lj_pe_validate + lj_gs2_save into UserFixtureDef\ and LJ_ACT_GS2_RELOAD (w3-contracts seam 4).
 *  - TFixtureSetupForm "Customize Fixture" (02 s4.3, design 6.5): per-fixture palette overrides, lj_fixture.custom[6],
 *    persisted by LJ_ACT_FIXTURE_APPLY (Setup.<slot>.pal) - with TCopySetupForm "Copy Palettes from".
 *  - TChannelDefForm "Channel Configuration" (ME-32): per-offset caption and Grand Master control.
 * TQuickRGBDefForm "RGB Value" stands in for the Windows colour dialog of both (no platform colour dialog exists).
 * Bitmaps are our own art only (D2, ME-18): Set opens Get Bitmap over our icons, Bitmaps / Gobo/Effects Bitmaps open Select
 * Bitmap over our tiles (W4, lj_config_dialogs.c); nothing of Martin's is read. W4 (dlg-config): Palette Functions opens
 * TPaletteValueTypeForm, Apply to all Selected Fixtures TApplyMultiForm, a format-0/1 record TProfileNameOldStyleForm
 * (ME-126); Define Fixture Profile on Scanner 1 / Color Scroller 1 / RGB pack fixtures opens their definition windows. */
#define _CRT_SECURE_NO_WARNINGS 1
#include "windows/lj_profile_editor_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile_editor.h"
#include "gfx/lj_glyphs.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "desktop/lj_selection.h"
#include "windows/lj_config_dialogs.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"

#define LJ_PE_CAPTION "Generic Fixture Profile"
#define LJ_PE_NA      "Not available in LJ4EVR"

/* ---- shared helpers ---- */
typedef struct { char t[LJ_PROFILE_CH][112]; const char *p[LJ_PROFILE_CH]; int n; } lj_rows;
static void rows_set(lj_widget *w, lj_rows *r)
{
    int i, keep;
    if (!w) return;
    for (i = 0; i < r->n; i++) r->p[i] = r->t[i];
    if (w->kind == LJ_K_LISTBOX) { keep = ((lj_w_list *)w)->index; lj_w_list_set_items(w, r->p, r->n); ((lj_w_list *)w)->index = keep < r->n ? keep : -1; }
    else { keep = lj_w_listview_index(w); lj_w_listview_set_items(w, r->p, r->n); ((lj_w_listview *)w)->index = keep < r->n ? keep : -1; }
    lj_widget_invalidate(w);
}
static int row_of(lj_widget *w) { return !w ? -1 : w->kind == LJ_K_LISTBOX ? ((lj_w_list *)w)->index : lj_w_listview_index(w); }
static void check_quiet(lj_widget *cb, int on)   /* TCheckBox.SetChecked fires OnClick; a repaint from the model must not */
{
    void (*fn)(lj_widget *, int, void *);
    if (!cb) return;
    fn = cb->on_event; cb->on_event = NULL;
    lj_w_checkbox_set_checked(cb, on);
    cb->on_event = fn;
}
static void edit_quiet(lj_widget *e, const char *t)
{
    void (*fn)(lj_widget *, int, void *);
    if (!e) return;
    fn = e->on_event; e->on_event = NULL;
    lj_w_edit_set_text(e, t);
    e->on_event = fn;
}
static int edit_num(lj_widget *e, int lo, int hi, int *out)   /* 1 = a number in range, 0 = blank, -1 = not a number */
{
    const char *t = e ? lj_w_edit_text(e) : NULL;
    char *end;
    long v;
    if (!t || !t[0]) return 0;
    v = strtol(t, &end, 10);
    if (*end || v < lo || v > hi) return -1;
    *out = (int)v;
    return 1;
}
static void close_win(lj_window **win) { if (*win) { lj_win_hide(*win); lj_window_destroy(*win); *win = NULL; } }
static void set_bitbtn(lj_window *win, const char *name, void (*fn)(lj_widget *, int, void *))   /* bkOK / bkCancel rows with no OnClick */
{
    lj_widget *b = win ? lj_window_find(win, name) : NULL;
    if (b) b->on_event = fn;
}
static const char *profile_text(const lj_profile *p, char *buf, size_t cap)
{
    if (!p) { lj_str_copy(buf, cap, "?"); return buf; }
    snprintf(buf, cap, "%s %s%s%s", p->manufacturer, p->name, p->mode[0] ? ", " : "", p->mode);
    return buf;
}

/* ---- TQuickRGBDefForm "RGB Value" (the colour dialog stand-in) ---- */
static struct { lj_window *win; void (*done)(uint32_t rgb); } rgbf;
static void rgb_close(void) { close_win(&rgbf.win); }
static void rgb_ok(lj_widget *w, void *ud)
{
    int r = 0, g = 0, b = 0;
    void (*done)(uint32_t) = rgbf.done;
    (void)w; (void)ud;
    if (edit_num(lj_window_find(rgbf.win, "eRed"), 0, 255, &r) < 0 || edit_num(lj_window_find(rgbf.win, "eGreen"), 0, 255, &g) < 0
        || edit_num(lj_window_find(rgbf.win, "eBlue"), 0, 255, &b) < 0) return;
    rgb_close();
    if (done) done((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
}
static void rgb_change(lj_widget *w, void *ud)
{
    int r = 0, g = 0, b = 0;
    lj_widget *s = rgbf.win ? lj_window_find(rgbf.win, "SRGB") : NULL;
    (void)w; (void)ud;
    if (!s) return;
    edit_num(lj_window_find(rgbf.win, "eRed"), 0, 255, &r); edit_num(lj_window_find(rgbf.win, "eGreen"), 0, 255, &g); edit_num(lj_window_find(rgbf.win, "eBlue"), 0, 255, &b);
    s->color = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    lj_widget_invalidate(s);
}
static const lj_binding lj_rgb_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "eRedChange", rgb_change, NULL },
    { "eEditEnter", lj_handler_noop, NULL }, { "bOkClick", rgb_ok, NULL }, { NULL, NULL, NULL } };
static void rgb_open(uint32_t rgb, void (*done)(uint32_t rgb))
{
    char t[16];
    rgb_close();
    if ((rgbf.win = lj_window_create(&lj_form_TQUICKRGBDEFFORM, lj_rgb_bindings)) == NULL) { lj_log("profile editor: cannot create TQuickRGBDefForm"); return; }
    rgbf.done = done;
    if (rgb == LJ_CUSTOM_NO_RGB) rgb = 0;
    snprintf(t, sizeof t, "%u", (unsigned)(rgb >> 16 & 255)); lj_w_edit_set_text(lj_window_find(rgbf.win, "eRed"), t);
    snprintf(t, sizeof t, "%u", (unsigned)(rgb >> 8 & 255));  lj_w_edit_set_text(lj_window_find(rgbf.win, "eGreen"), t);
    snprintf(t, sizeof t, "%u", (unsigned)(rgb & 255));       lj_w_edit_set_text(lj_window_find(rgbf.win, "eBlue"), t);
    lj_win_show(rgbf.win, 0);
}

/* =====================================================================================================================
 * Generic Fixture Profile (TScanner2DefsForm)
 * ===================================================================================================================== */
enum { PE_CB = 0, PE_NUM, PE_STR, PE_MS };
typedef struct { const char *ctl; uint16_t off; uint8_t index, kind; } lj_pe_map;   /* one dialog control -> its record bytes [16 s4] */
static const lj_pe_map lj_pe_ctl[] = {
    { "eIconCaption", 0x0028, 0, PE_STR }, { "eSelectionId", 0x0033, 0, PE_STR }, { "eAuthor", 0x0043, 0, PE_STR },
    { "eFixtureName", 0x3E37, 0, PE_STR }, { "eFixtureMode", 0x3E75, 0, PE_STR }, { "cbTransparent", 0x10A8, 0, PE_CB },
    { "eStrobeOff", 0x1195, 0, PE_NUM }, { "cbInvertInt", 0x11A7, 0, PE_CB }, { "cbUseGM", 0x11A8, 0, PE_CB }, { "cbBOGM", 0x11A9, 0, PE_CB },
    { "cbLampOn", 0x1201, 16, PE_CB }, { "cbLampOff", 0x1258, 16, PE_CB }, { "eLampOnTime", 0x3D4D, 0, PE_MS },
    { "cbInvertIntVal", 0x3D51, 0, PE_CB }, { "cbLimitGM", 0x3D52, 0, PE_CB },
    { "cbPanTilt16", 0x1306, 0, PE_CB }, { "cbPanTilt8", 0x1307, 0, PE_CB }, { "ePanOfs", 0x1308, 0, PE_NUM }, { "eTiltOfs", 0x1309, 0, PE_NUM },
    { "cbPanTiltPatch", 0x130A, 0, PE_CB }, { "ePanPatchMSB", 0x130B, 0, PE_NUM }, { "ePanPatchLSB", 0x130B, 1, PE_NUM },
    { "eTiltPatchMSB", 0x130B, 2, PE_NUM }, { "eTiltPatchLSB", 0x130B, 3, PE_NUM }, { "cbPresets", 0x130F, 0, PE_CB }, { "cbMacros", 0x1310, 0, PE_CB },
    { "cbFollow", 0x37EA, 0, PE_CB },
    { "cbCMY", 0x1C5F, 0, PE_CB }, { "cbRGB", 0x1C60, 0, PE_CB }, { "cbCMYPicker", 0x1C61, 0, PE_CB }, { "cbInvertPicker", 0x1C62, 0, PE_CB },
    { "cbRGBHue", 0x3E34, 0, PE_CB }, { "cbRGBSat", 0x3E35, 0, PE_CB }, { "cbRGBVal", 0x3E36, 0, PE_CB },
    { "cbReset", 0x12AF, 16, PE_CB }, { "eResetTime", 0x3D49, 0, PE_MS },
    { "eMan", 0x3C86, 0, PE_STR }, { "eModel", 0x3CC7, 0, PE_STR }, { "eMode", 0x3D08, 0, PE_STR },
};
/* the dynamic functions [16 s3.1]: enable box + e<P>Ofs / Name / Min / Max (the edits a tab lacks are skipped) */
typedef struct { const char *cb, *prefix; uint16_t off; } lj_pe_dynmap;
static const lj_pe_dynmap lj_pe_dyn[] = {
    { "cbShutter", "Shutter", 0x1173 }, { "cbStrobe", "Strobe", 0x1184 }, { "cbIntensity", "Intensity", 0x1196 }, { "cbMove1", "Move1", 0x1311 },
    { "cbColor1", "Color1", 0x1322 }, { "cbColor2", "Color2", 0x1333 }, { "cbColor3", "Color3", 0x1344 }, { "cbColor4", "Color4", 0x1355 },
    { NULL, "Cyan", 0x1C63 }, { NULL, "Magenta", 0x1C74 }, { NULL, "Yellow", 0x1C85 },
    { "cbGobo1", "Gobo1", 0x1CA7 }, { "cbGobo2", "Gobo2", 0x1CB8 }, { "cbGobo3", "Gobo3", 0x1CC9 }, { "cbGobo4", "Gobo4", 0x1CDA },
    { "cbIris", "Iris", 0x25E4 }, { "cbFocus", "Focus", 0x25F5 }, { "cbZoom", "Zoom", 0x2606 }, { "cbFrost", "Frost", 0x2617 },
    { "cbBeam1", "Beam1", 0x2628 }, { "cbBeam2", "Beam2", 0x2639 },
    { "cbEffects1", "Effects1", 0x264A }, { "cbEffects2", "Effects2", 0x265B }, { "cbEffects3", "Effects3", 0x3C64 }, { "cbEffects4", "Effects4", 0x3C75 },
};
/* the 32-offset tables a Set / Clear button pair edits */
enum { LV_STATIC = 0, LV_DEFAULT, LV_GM };
typedef struct { const char *lv, *set, *clear, *edit; uint16_t off; uint8_t kind; } lj_pe_lvmap;
static const lj_pe_lvmap lj_pe_lv[] = {
    { "lvBO", "bSetBOVal", "bClearBo", "eBoVal", 0x11AA, LV_STATIC }, { "lvLampOn", "bSetLampOnVal", "bClearLampOn", "eLampOnVal", 0x1201, LV_STATIC },
    { "lvLampOff", "bSetLampOffVal", "bClearLampOff", "eLampOffVal", 0x1258, LV_STATIC }, { "lvReset", "bSetResetVal", "bClearReset", "eResetVal", 0x12AF, LV_STATIC },
    { "lvFunctionsVal", "bSetFunctionval", "bClearFunction", "eFunctionsVal", 0x2AF1, LV_STATIC },   /* + 87 * the lbFunctions row */
    { "lvDefaultDMX", "BSetDefault", "bClearDefault", "eDefault", 0x1088, LV_DEFAULT }, { "lvGMControl", "bEnableGMStd", "bDisableGM", NULL, 0x3E14, LV_GM },
};
/* the three palette pages: PC1 page, TTabControl, list, edits in slot order (DFM positions: Val, Rota / CW, CCW / Alt) */
typedef struct { int page; const char *tabs, *lv, *label, *e[5], *set, *bmp, *color, *types; uint16_t pal[2], type[2]; } lj_pe_palmap;
static const lj_pe_palmap lj_pe_pal[] = {
    { LJ_PE_FIXED_COLOR, "ColorPaletteTab", "lvColor", "lColor", { "eColorVal", "eColorRota", "eColorCW", "eColorCCW", "eColorAlt" }, "bSetColorVal", "bGetColorBMP", "bSelectColorColor", "bSelectValTypeColor", { 0x1377, 0x17EB }, { 0x37D1, 0x37D6 } },
    { LJ_PE_GOBO, "GoboPaletteTab", "lvGobo", "lGobo", { "eGoboVal", "EGoboRota", "eGoboCW", "EGoboCCW", "eGoboAlt" }, "bSetGoboVal", "bGetGoboBmp", "bSelectGoboColor", "bSelectValTypeGobo", { 0x1CFC, 0x2170 }, { 0x37DB, 0x37E0 } },
    { LJ_PE_EFFECTS, "effectsPaletteTab", "lvEffects", "lEffects", { "eEffectsVal", "eEffectsRota", "eEffectsCW", "eEffectsCCW", "eEffectsAlt" }, "bSetEffectsVal", "bGetEffectsBmp", "bSelectEffectsColor", "bSelectValTypeEffects", { 0x267D, 0x37EB }, { 0x37E5, 0x3C5F } },
};
static const char *const lj_pe_types[19] = { "Def", "Alt1", "Alt2", "Alt3", "Alt4", "Idx", "Rot", "CW", "CCW", "B/O", "Shk", "Spd", "Fix", "Spl", "RdS", "RdM", "RdF", "Color", "Stop" };   /* 16 s5 */

static struct {
    lj_window *win;
    lj_profile_edit ed;                     /* ~38 KB, static (never a stack local) */
    int fixture, loading;
    lj_rows lv[LJ_COUNT_OF(lj_pe_lv)], pal[3], levels, funcs, manu;
    int legacy;                             /* opened on a format-0/1 record (ME-126) */
    char manu_buf[LJ_PROFILE_CH][31];
    uint8_t clip[LJ_PAL_ENTRIES * 57]; int clip_ok, clip_color;
} pe;

static lj_widget *pfind(const char *n) { return pe.win && n ? lj_window_find(pe.win, n) : NULL; }
static uint8_t *praw(void) { return pe.ed.raw; }
static int pal_index(int i) { lj_widget *t = pfind(lj_pe_pal[i].tabs); return t && ((lj_w_tabs *)t)->active == 1 ? 1 : 0; }
static uint8_t *pal_entry(int i, int e) { return praw() + lj_pe_pal[i].pal[pal_index(i)] + 57 * e; }
static int funcs_row(void) { int r = row_of(pfind("lbFunctions")); return r < 0 ? 0 : r; }
static uint16_t lv_off(int i) { return (uint16_t)(lj_pe_lv[i].off + (strcmp(lj_pe_lv[i].lv, "lvFunctionsVal") == 0 ? 87 * funcs_row() : 0)); }
static void sstr_get(uint16_t off, int width, char *dst, size_t cap)
{
    size_t n = praw()[off] > width ? (size_t)width : praw()[off];
    if (n >= cap) n = cap - 1;
    memcpy(dst, praw() + off + 1, n); dst[n] = 0;
}
static void sstr_put(uint16_t off, int width, const char *s)   /* the stale tail stays (16 s3) */
{
    size_t n = strlen(s);
    if (n > (size_t)width) n = (size_t)width;
    praw()[off] = (uint8_t)n; memcpy(praw() + off + 1, s, n);
    pe.ed.dirty = 1;
}
static int field_at(uint16_t off) { int i; for (i = 0; i < lj_pe_nfields; i++) if (lj_pe_fields[i].off == off) return i; return -1; }

/* ---- model -> controls ---- */
static void pe_paint_lists(void)
{
    size_t i;
    int k, e, p;
    char name[32];
    for (i = 0; i < LJ_COUNT_OF(lj_pe_lv); i++) {
        lj_rows *r = &pe.lv[i];
        uint16_t off = lv_off((int)i);
        r->n = LJ_PROFILE_CH;
        for (k = 0; k < LJ_PROFILE_CH; k++) {
            const uint8_t *b = praw() + off;
            if (lj_pe_lv[i].kind == LV_STATIC) { if (b[17 + k]) snprintf(r->t[k], sizeof r->t[k], "%d\t%d", k, b[49 + k]); else snprintf(r->t[k], sizeof r->t[k], "%d\t", k); }
            else if (lj_pe_lv[i].kind == LV_DEFAULT) snprintf(r->t[k], sizeof r->t[k], "%d\t%d", k, b[k]);
            else snprintf(r->t[k], sizeof r->t[k], "%d\t%s", k, b[k] ? "Ext GM" : "");   /* the Type column is derived (16 s4.10) */
        }
        rows_set(pfind(lj_pe_lv[i].lv), r);
    }
    pe.funcs.n = LJ_PROFILE_CH;
    for (k = 0; k < LJ_PROFILE_CH; k++) {
        sstr_get((uint16_t)(0x2AF1 + 87 * k), 15, name, sizeof name);
        snprintf(pe.funcs.t[k], sizeof pe.funcs.t[k], "%d: %s", k + 1, name[0] ? name : "Unused");   /* exe "%d: %s" (0x309E08); "Unused" [02 s5.3] */
    }
    rows_set(pfind("lbFunctions"), &pe.funcs);
    pe.levels.n = LJ_PROFILE_CH;
    for (k = 0; k < LJ_PROFILE_CH; k++) {
        const uint8_t *b = praw() + 0x35D1 + 16 * k;
        sstr_get((uint16_t)(0x35D1 + 16 * k), 5, name, sizeof name);
        snprintf(pe.levels.t[k], sizeof pe.levels.t[k], "%d\t%s\t%s\t%s", k, b[6] ? "No" : "Yes", name, b[7] ? "No" : "Yes");   /* Yes / No PROPOSAL rig (userlevelstab.gif) */
    }
    rows_set(pfind("lvLevels"), &pe.levels);
    { char t[24]; snprintf(t, sizeof t, "%d Channels", praw()[5] + 1); lj_w_set_caption(pfind("lChannels"), t); }   /* the DFM's "32 Channels" */
    for (p = 0; p < 3; p++) {
        int pi = pal_index(p);
        lj_rows *r = &pe.pal[p];
        r->n = LJ_PAL_ENTRIES;
        for (e = 0; e < LJ_PAL_ENTRIES; e++) {
            const uint8_t *b = pal_entry(p, e);
            char v[5][16], bmp[32];
            for (k = 0; k < 5; k++) { if (b[0] && (b[1] >> k & 1)) snprintf(v[k], sizeof v[k], "%d", b[2 + k]); else v[k][0] = 0; }
            sstr_get((uint16_t)(lj_pe_pal[p].pal[pi] + 57 * e + 11), 30, bmp, sizeof bmp);
            if (b[0] && !bmp[0]) snprintf(name, sizeof name, "%02X%02X%02X", b[7], b[8], b[9]); else name[0] = 0;
            snprintf(r->t[e], sizeof r->t[e], "%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s", e, v[0], v[1], v[2], v[3], v[4], name, bmp);
        }
        rows_set(pfind(lj_pe_pal[p].lv), r);
        for (k = 0; k < 5; k++) {
            int code = praw()[lj_pe_pal[p].type[pi] + k];
            snprintf(name, sizeof name, "%s%d", lj_pe_pal[p].label, k + 1);
            lj_w_set_caption(pfind(name), code < 19 ? lj_pe_types[code] : "?");
        }
    }
}

static void pe_paint(void)
{
    size_t i;
    int k;
    char t[256];
    pe.loading = 1;
    for (i = 0; i < LJ_COUNT_OF(lj_pe_ctl); i++) {
        const lj_pe_map *m = &lj_pe_ctl[i];
        const uint8_t *b = praw() + m->off + m->index;
        lj_widget *w = pfind(m->ctl);
        if (m->kind == PE_CB) check_quiet(w, *b != 0);
        else if (m->kind == PE_NUM) { snprintf(t, sizeof t, "%d", *b); edit_quiet(w, t); }
        else if (m->kind == PE_MS) { snprintf(t, sizeof t, "%ld", (long)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24)); edit_quiet(w, t); }
        else { int f = field_at(m->off); lj_pe_get_str(&pe.ed, f, 0, t, sizeof t); edit_quiet(w, t); }
    }
    for (i = 0; i < LJ_COUNT_OF(lj_pe_dyn); i++) {
        const lj_pe_dynmap *d = &lj_pe_dyn[i];
        const uint8_t *b = praw() + d->off;
        char n[32];
        check_quiet(pfind(d->cb), b[0] != 0);
        snprintf(n, sizeof n, "e%sOfs", d->prefix); snprintf(t, sizeof t, "%d", b[1]); edit_quiet(pfind(n), t);
        snprintf(n, sizeof n, "e%sMin", d->prefix); snprintf(t, sizeof t, "%d", b[11]); edit_quiet(pfind(n), t);
        snprintf(n, sizeof n, "e%sMax", d->prefix); snprintf(t, sizeof t, "%d", b[12]); edit_quiet(pfind(n), t);
        snprintf(n, sizeof n, "e%sName", d->prefix); sstr_get((uint16_t)(d->off + 2), 8, t, sizeof t); edit_quiet(pfind(n), t);
    }
    sstr_get(0x1063, 36, t, sizeof t);
    lj_w_set_caption(pfind("lBitmapName"), t);                /* the icon is our art by name (D2), never a decoded Martin bitmap */
    {   /* Manufacturer: the free-text combo shows the record's text; its list = the manufacturers of the exe rows */
        lj_widget *cb = pfind("cbManufacturer");
        sstr_get(0x3E56, 30, pe.manu_buf[0], sizeof pe.manu_buf[0]);
        if (cb) { ((lj_w_combobox *)cb)->text = pe.manu_buf[0]; ((lj_w_combobox *)cb)->index = -1; lj_widget_invalidate(cb); }
    }
    {   /* Profile Notes: the user's RTF shown stripped (design 5.3); kept as raw bytes, not re-encoded */
        static char plain[LJ_GS2_NOTES], *lines[256];
        lj_widget *re = pfind("Notes");
        int n = 0;
        char *s = plain;
        lj_str_rtf_strip(plain, sizeof plain, (const char *)praw() + 0x0062, LJ_GS2_NOTES);
        while (s && *s && n < 256) { char *nl = strchr(s, '\n'); lines[n++] = s; if (nl) { *nl = 0; s = nl + 1; } else s = NULL; }
        if (re) { lj_w_memo_set_lines(re, (const char *const *)lines, n); ((lj_w_memo *)re)->vscroll = (unsigned)(n * ((lj_w_memo *)re)->line_h > re->h - 8); }   /* LORD_FIDELITY-7: a RichEdit shows its bar only when the text overflows */
    }
    for (k = 0; k < 3; k++) {
        size_t j;
        for (j = 0; j < 5; j++) { lj_widget *e = pfind(lj_pe_pal[k].e[j]); if (e) { e->enabled = row_of(pfind(lj_pe_pal[k].lv)) >= 0; lj_widget_invalidate(e); } }
    }
    pe_paint_lists();
    pe.loading = 0;
}

/* ---- controls -> model (Apply / Save read every control back, as the dialog does) ---- */
static void pe_read(void)
{
    size_t i;
    int v;
    for (i = 0; i < LJ_COUNT_OF(lj_pe_ctl); i++) {
        const lj_pe_map *m = &lj_pe_ctl[i];
        lj_widget *w = pfind(m->ctl);
        uint8_t *b = praw() + m->off + m->index;
        if (!w) continue;
        if (m->kind == PE_CB) *b = (uint8_t)lj_w_checkbox_checked(w);
        else if (m->kind == PE_NUM) { if (edit_num(w, 0, 255, &v) == 1) *b = (uint8_t)v; }
        else if (m->kind == PE_MS) { if (edit_num(w, 0, 3600000, &v) == 1) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); } }
        else { char t[80]; sstr_get(m->off, lj_pe_fields[field_at(m->off)].n, t, sizeof t); if (strcmp(t, lj_w_edit_text(w)) != 0) lj_pe_set_str(&pe.ed, field_at(m->off), 0, lj_w_edit_text(w)); }
    }
    for (i = 0; i < LJ_COUNT_OF(lj_pe_dyn); i++) {
        const lj_pe_dynmap *d = &lj_pe_dyn[i];
        uint8_t *b = praw() + d->off;
        char n[32], t[16];
        lj_widget *w;
        if (d->cb && pfind(d->cb)) b[0] = (uint8_t)lj_w_checkbox_checked(pfind(d->cb));
        snprintf(n, sizeof n, "e%sOfs", d->prefix); if (edit_num(pfind(n), 0, 255, &v) == 1) b[1] = (uint8_t)v;
        snprintf(n, sizeof n, "e%sMin", d->prefix); if (edit_num(pfind(n), 0, 255, &v) == 1) b[11] = (uint8_t)v;
        snprintf(n, sizeof n, "e%sMax", d->prefix); if (edit_num(pfind(n), 0, 255, &v) == 1) b[12] = (uint8_t)v;
        snprintf(n, sizeof n, "e%sName", d->prefix);
        if ((w = pfind(n)) != NULL) { sstr_get((uint16_t)(d->off + 2), 8, t, sizeof t); if (strcmp(t, lj_w_edit_text(w)) != 0) sstr_put((uint16_t)(d->off + 2), 8, lj_w_edit_text(w)); }
    }
    pe.ed.dirty = 1;
    lj_pe_refresh(&pe.ed);
}

/* ---- commit: validate, save into UserFixtureDef\, reload, bind ---- */
static int pe_file_name(char *dst, size_t cap)   /* Save Profile suggests the file it came from; else <Manufacturer>_<Name>.GS2 (no platform save dialog yet: PROPOSAL rig) */
{
    char m[32], n[32], *c;
    if (pe.ed.p.gs2_file[0]) return lj_str_copy(dst, cap, pe.ed.p.gs2_file);
    sstr_get(0x3E56, 30, m, sizeof m); sstr_get(0x3E37, 30, n, sizeof n);
    snprintf(dst, cap, "%s_%s", m, n[0] ? n : "Profile");
    for (c = dst; *c; c++) if (strchr("\\/:*?\"<>| ", *c)) *c = '_';
    if (strlen(dst) > 29) return -1;                          /* "Filename cannot exceed 29 characters :" (exe 0x31877C), the name without .GS2 */
    if (strlen(dst) + 4 >= cap) return -1;
    strcat(dst, ".GS2");
    return 0;
}

int lj_profile_editor_commit(int save_as)
{
    lj_ctx *ctx = lj_ctx_get();
    char name[64], path[LJ_PATH_CAP];
    const char *msg;
    pe_read();
    if ((msg = lj_pe_validate(&pe.ed)) != NULL) { lj_win_message(LJ_PE_CAPTION, msg); return -1; }
    if (save_as) pe.ed.p.gs2_file[0] = 0;
    if (pe_file_name(name, sizeof name) != 0) { lj_win_message(LJ_PE_CAPTION, LJ_PE_ERR_FILENAME); return -1; }
    sstr_put(0x3D53, 64, name);                               /* source_file: what Save Profile suggests next time [16 s4.2] */
    if (lj_pe_refresh(&pe.ed) != 0 || !ctx->lib || lj_library_path(ctx->lib, "UserFixtureDef\\", path, sizeof path) != 0
        || strlen(path) + strlen(name) >= sizeof path) { lj_log("profile editor: cannot save %s", name); return -1; }
    strcat(path, name);
    pe.ed.p.gs2_raw = pe.ed.raw;
    if (lj_gs2_save(path, &pe.ed.p) != 0) { lj_log("profile editor: cannot write %s", path); return -1; }
    lj_str_copy(pe.ed.p.gs2_file, sizeof pe.ed.p.gs2_file, name);
    pe.ed.dirty = 0;
    lj_desktop_run_action(LJ_ACT_GS2_RELOAD, 0);
    return 0;
}

static void pe_bind_fixture(int f)   /* "Apply": the fixture now uses this profile (testable while still defining it, 02 s5.2) */
{
    lj_show *sh = lj_ctx_get()->show;
    if (!sh || f < 0 || f >= LJ_FIXTURES || !sh->fixture[f].link) return;
    lj_lock();
    sh->fixture[f].kind = LJ_FX_GS2;
    lj_str_copy(sh->fixture[f].gs2_file, sizeof sh->fixture[f].gs2_file, pe.ed.p.gs2_file);
    lj_unlock();
    lj_desktop_run_action(LJ_ACT_FIXTURE_APPLY, f);
}

/* ---- handlers ---- */
static void pe_cancel_result(int yes, void *ud) { (void)ud; if (yes) lj_profile_editor_close(); }
static void pe_on_cancel(lj_widget *w, void *ud)
{
    static uint8_t before[LJ_GS2_REC_SIZE];   /* a typed field is a change too: the TEdits have no OnChange (DFM), so compare the record */
    uint8_t was = pe.ed.dirty;
    (void)w; (void)ud;
    lj_pe_refresh(&pe.ed);
    memcpy(before, pe.ed.raw, sizeof before);
    pe_read();
    pe.ed.dirty = (uint8_t)(was || memcmp(before, pe.ed.raw, sizeof before) != 0);
    if (pe.ed.dirty) lj_win_confirm("Please Confirm..", LJ_PE_ASK_UNAPPLIED, NULL, pe_cancel_result, NULL);
    else lj_profile_editor_close();
}
static void pe_on_apply(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_profile_editor_commit(0) == 0) pe_bind_fixture(pe.fixture); }
static void pe_apply_one(int f, void *ud) { (void)ud; pe_bind_fixture(f); }
static void pe_on_apply_all(lj_widget *w, void *ud)   /* TApplyMultiForm "Apply user profile": the selected user-definable fixtures, Identical Id Only (CHM scribble7334) */
{
    (void)w; (void)ud;
    if (lj_profile_editor_commit(0) != 0) return;
    lj_config_dlg_apply_multi(pe.fixture, pe_apply_one, NULL);
}
static void pe_on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_profile_editor_commit(0); }
static void pe_on_save_as(lj_widget *w, void *ud) { (void)w; (void)ud; lj_profile_editor_commit(1); }
static void pe_load(int yes, void *ud)   /* Load Profile: the master fixture's file again (no platform open dialog yet) */
{
    const lj_profile *p = lj_show_profile(lj_ctx_get()->show, pe.fixture);
    (void)ud;
    if (!yes) return;
    lj_pe_begin(&pe.ed, p && p->gs2_raw ? p : NULL);
    pe_paint();
}
static void pe_clear(int yes, void *ud) { char f[64]; (void)ud; if (!yes) return; lj_str_copy(f, sizeof f, pe.ed.p.gs2_file); lj_pe_begin(&pe.ed, NULL); lj_str_copy(pe.ed.p.gs2_file, sizeof pe.ed.p.gs2_file, f); pe.ed.dirty = 1; pe_paint(); }
static void pe_on_load(lj_widget *w, void *ud) { (void)w; (void)ud; if (pe.ed.dirty) lj_win_confirm("Please Confirm..", LJ_PE_ASK_UNAPPLIED, NULL, pe_load, NULL); else pe_load(1, NULL); }
static void pe_on_clear_profile(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Please Confirm..", LJ_PE_ASK_UNAPPLIED, NULL, pe_clear, NULL); }
static void pe_on_files(lj_widget *w, void *ud) { (void)w; (void)ud; if (pe.win) lj_menu_set_enabled(lj_window_menus(pe.win), "M_Save", pe.ed.p.gs2_file[0] != 0); }   /* "only when loaded from a file" [02 s5.2] */
static void pe_on_dirty(lj_widget *w, void *ud) { (void)w; (void)ud; if (!pe.loading) pe.ed.dirty = 1; }
static void pe_on_close(lj_widget *w, void *ud) { pe_on_cancel(w, ud); }
static void pe_icon_done(const char *name, void *ud) { (void)ud; if (!pe.win) return; lj_pe_set_str(&pe.ed, field_at(0x1063), 0, name); pe_paint(); }
static void pe_on_set_icon(lj_widget *w, void *ud) { (void)w; (void)ud; lj_config_dlg_get_icon(pe_icon_done, NULL); }   /* TGetBitmapForm over our icons (art/icons, by name) */
static void pe_on_clear_icon(lj_widget *w, void *ud) { (void)w; (void)ud; lj_pe_set_str(&pe.ed, field_at(0x1063), 0, ""); pe_paint(); }

static int lv_by_ctl(const char *name)
{
    size_t i;
    for (i = 0; name && i < LJ_COUNT_OF(lj_pe_lv); i++)
        if (strcmp(name, lj_pe_lv[i].lv) == 0 || strcmp(name, lj_pe_lv[i].set) == 0 || strcmp(name, lj_pe_lv[i].clear) == 0 || (lj_pe_lv[i].edit && strcmp(name, lj_pe_lv[i].edit) == 0)) return (int)i;
    return -1;
}
static int pal_by_ctl(const char *name)
{
    int i, k;
    for (i = 0; name && i < 3; i++) {
        if (strcmp(name, lj_pe_pal[i].lv) == 0 || strcmp(name, lj_pe_pal[i].set) == 0 || strcmp(name, lj_pe_pal[i].bmp) == 0 || strcmp(name, lj_pe_pal[i].color) == 0
            || strcmp(name, lj_pe_pal[i].types) == 0 || strcmp(name, lj_pe_pal[i].tabs) == 0) return i;
        for (k = 0; k < 5; k++) if (strcmp(name, lj_pe_pal[i].e[k]) == 0) return i;
    }
    return -1;
}
static int pal_of_page(void) { lj_widget *pc = pfind("PC1"); int i, a = pc ? ((lj_w_tabs *)pc)->active : -1; for (i = 0; i < 3; i++) if (lj_pe_pal[i].page == a) return i; return 0; }

static void pe_on_lv(lj_widget *w, void *ud)   /* lvChange / lvClick: the row's value into the edit(s) */
{
    int i = lv_by_ctl(w->name), r = row_of(w), k;
    char t[12];   /* any int (gcc -Wformat-truncation) */
    (void)ud;
    if (pe.loading || r < 0) return;
    if (i >= 0) {
        const uint8_t *b = praw() + lv_off(i);
        int v = lj_pe_lv[i].kind == LV_STATIC ? (b[17 + r] ? b[49 + r] : -1) : b[r];
        lj_widget *set = pfind(lj_pe_lv[i].set), *clr = pfind(lj_pe_lv[i].clear);
        if (v >= 0) snprintf(t, sizeof t, "%d", v); else t[0] = 0;
        if (lj_pe_lv[i].edit) edit_quiet(pfind(lj_pe_lv[i].edit), t);
        if (set) { set->enabled = 1; lj_widget_invalidate(set); }   /* the DFM has Set / Clear disabled until a row is picked */
        if (clr) { clr->enabled = 1; lj_widget_invalidate(clr); }
    } else if ((i = pal_by_ctl(w->name)) >= 0) {
        const uint8_t *b = pal_entry(i, r);
        for (k = 0; k < 5; k++) {
            lj_widget *e = pfind(lj_pe_pal[i].e[k]);
            if (b[0] && (b[1] >> k & 1)) snprintf(t, sizeof t, "%d", b[2 + k]); else t[0] = 0;
            if (e) { e->enabled = 1; edit_quiet(e, t); }
        }
    }
}
static void pe_on_set_val(lj_widget *w, void *ud)   /* bSetValClick: every "Set" of the dialog */
{
    int i = lv_by_ctl(w->name), r, v, k;
    (void)ud;
    if (i >= 0) {
        uint8_t *b = praw() + lv_off(i);
        if ((r = row_of(pfind(lj_pe_lv[i].lv))) < 0) return;
        if (lj_pe_lv[i].kind == LV_GM) b[r] = 1;
        else if (edit_num(pfind(lj_pe_lv[i].edit), 0, 255, &v) == 1) { if (lj_pe_lv[i].kind == LV_STATIC) { b[17 + r] = 1; b[49 + r] = (uint8_t)v; } else b[r] = (uint8_t)v; }
        else return;
    } else if ((i = pal_by_ctl(w->name)) >= 0) {   /* Set Palette Values: a blank slot clears its bit ("just leave the field blank", 16 s3.1) */
        uint8_t *b;
        if ((r = row_of(pfind(lj_pe_pal[i].lv))) < 0) return;
        b = pal_entry(i, r);
        for (k = 0; k < 5; k++) {
            int got = edit_num(pfind(lj_pe_pal[i].e[k]), 0, 255, &v);
            if (got == 1) { b[1] |= (uint8_t)(1u << k); b[2 + k] = (uint8_t)v; } else if (got == 0) b[1] &= (uint8_t)~(1u << k);
        }
        b[0] = 1;
    } else return;
    pe.ed.dirty = 1;
    lj_pe_refresh(&pe.ed);
    pe_paint_lists();
}
static void pe_on_clear_val(lj_widget *w, void *ud)   /* bClearClick: Clear / Disable of a 32-offset table */
{
    int i = lv_by_ctl(w->name), r;
    uint8_t *b;
    (void)ud;
    if (i < 0 || (r = row_of(pfind(lj_pe_lv[i].lv))) < 0) return;
    b = praw() + lv_off(i);
    if (lj_pe_lv[i].kind == LV_STATIC) b[17 + r] = b[49 + r] = 0; else b[r] = 0;
    pe.ed.dirty = 1;
    pe_paint_lists();
}
static void pe_on_functions(lj_widget *w, void *ud) { char t[20]; (void)w; (void)ud; sstr_get((uint16_t)(0x2AF1 + 87 * funcs_row()), 15, t, sizeof t); pe.loading = 1; edit_quiet(pfind("eFunctionName"), t); pe.loading = 0; pe_paint_lists(); }
static void pe_on_function_name(lj_widget *w, void *ud) { (void)ud; if (!pe.loading && row_of(pfind("lbFunctions")) >= 0) { sstr_put((uint16_t)(0x2AF1 + 87 * funcs_row()), 15, lj_w_edit_text(w)); pe_paint_lists(); } }
static void pe_on_delete_function(lj_widget *w, void *ud)   /* Clear Function: name, enable and the table of that special */
{
    uint8_t *b = praw() + 0x2AF1 + 87 * funcs_row();
    (void)w; (void)ud;
    if (row_of(pfind("lbFunctions")) < 0) return;
    b[0] = 0; memset(b + 16, 0, 65);
    pe.ed.dirty = 1;
    pe_on_functions(NULL, NULL);
}
static void pe_on_level_status(lj_widget *w, void *ud)   /* Visible / Invisible / Disabled / Enabled / Disable All */
{
    int r = row_of(pfind("lvLevels")), k, lv = lj_pe_field_find("lvLevels");
    (void)ud;
    if (strcmp(w->name, "bDisableAll") == 0) { for (k = 0; k < LJ_PROFILE_CH; k++) lj_pe_set_u8(&pe.ed, lv, 16 * k + 7, 1); }
    else if (r >= 0) {                                        /* through lj_pe_set_u8: a Levels edit recomputes byte 5 (LORD_FIXTURES-4) */
        if (strcmp(w->name, "bVisible") == 0) lj_pe_set_u8(&pe.ed, lv, 16 * r + 6, 0); else if (strcmp(w->name, "bInvisible") == 0) lj_pe_set_u8(&pe.ed, lv, 16 * r + 6, 1);
        else if (strcmp(w->name, "bDisable") == 0) lj_pe_set_u8(&pe.ed, lv, 16 * r + 7, 1); else if (strcmp(w->name, "bEnable") == 0) lj_pe_set_u8(&pe.ed, lv, 16 * r + 7, 0);
    }
    pe.ed.dirty = 1;
    lj_pe_refresh(&pe.ed);
    pe_paint_lists();
}
static void pe_on_levels_click(lj_widget *w, void *ud) { char t[8]; int r = row_of(w); (void)ud; if (r < 0) return; sstr_get((uint16_t)(0x35D1 + 16 * r), 5, t, sizeof t); pe.loading = 1; edit_quiet(pfind("eLevelName"), t); pe.loading = 0; }
static void pe_on_level_name(lj_widget *w, void *ud) { int r = row_of(pfind("lvLevels")); (void)ud; if (pe.loading || r < 0) return; sstr_put((uint16_t)(0x35D1 + 16 * r), 5, lj_w_edit_text(w)); pe_paint_lists(); }
static void pe_on_pal_tab(lj_widget *w, void *ud) { (void)w; (void)ud; pe_paint_lists(); }
static void pe_on_page(lj_widget *w, void *ud) { (void)w; (void)ud; pe_paint_lists(); }
static uint16_t pe_bmp_off;   /* the palette entry Select Bitmap answers for */
static void pe_bmp_done(const char *name, void *ud)
{
    (void)ud;
    if (!pe.win) return;
    sstr_put((uint16_t)(pe_bmp_off + 11), 30, name);
    praw()[pe_bmp_off] = 1;
    pe_paint_lists();
}
static void pe_on_get_bmp(lj_widget *w, void *ud)   /* Bitmaps: TBitMapPickerForm2 over our tiles (D2) */
{
    int i = pal_by_ctl(w->name), r = i >= 0 ? row_of(pfind(lj_pe_pal[i].lv)) : -1;
    (void)ud;
    if (r < 0) return;
    pe_bmp_off = (uint16_t)(lj_pe_pal[i].pal[pal_index(i)] + 57 * r);
    lj_config_dlg_pick_bitmap(lj_pe_pal[i].page == LJ_PE_FIXED_COLOR, pe_bmp_done, NULL);
}
static int pe_rgb_pal = -1;
static void pe_rgb_done(uint32_t rgb)
{
    int r;
    uint8_t *b;
    if (pe_rgb_pal < 0 || !pe.win || (r = row_of(pfind(lj_pe_pal[pe_rgb_pal].lv))) < 0) return;
    b = pal_entry(pe_rgb_pal, r);
    b[0] = 1; b[7] = (uint8_t)(rgb >> 16); b[8] = (uint8_t)(rgb >> 8); b[9] = (uint8_t)rgb;   /* R, G, B, 0 (16 s3.1) */
    sstr_put((uint16_t)(b + 11 - praw()), 30, "");               /* "a colour or a bitmap (not both)" [02 s5.4] */
    pe.ed.dirty = 1;
    pe_paint_lists();
}
static void pe_on_colors(lj_widget *w, void *ud)
{
    int i = pal_by_ctl(w->name), r = i >= 0 ? row_of(pfind(lj_pe_pal[i].lv)) : -1;
    const uint8_t *b;
    (void)ud;
    if (r < 0) return;
    b = pal_entry(i, r);
    pe_rgb_pal = i;
    rgb_open((uint32_t)b[7] << 16 | (uint32_t)b[8] << 8 | b[9], pe_rgb_done);
}
static void pe_val_done(void *ud) { (void)ud; pe.ed.dirty = 1; pe_paint_lists(); }
static void pe_on_val_type(lj_widget *w, void *ud)   /* Palette Functions: TPaletteValueTypeForm edits the five codes in the record [16 s5] */
{
    static const char *const pal_name[3][2] = { { "Color 1", "Color 2" }, { "Gobo 1", "Gobo 2" }, { "Effects 1", "Effects 2" } };
    int i = pal_by_ctl(w->name);
    (void)ud;
    if (i < 0) return;
    lj_config_dlg_value_types(pal_name[i][pal_index(i)], praw() + lj_pe_pal[i].type[pal_index(i)], pe_val_done, NULL);
}
static void pe_on_lv_popup(lj_widget *w, void *ud)   /* P_lvClick: Insert / Delete / Clear / Clear all / Compact / Copy / Paste on the page's palette */
{
    const lj_menu_item_desc *it = pe.win ? lj_window_menu_item(pe.win) : NULL;
    int p = pal_of_page(), r = row_of(pfind(lj_pe_pal[p].lv)), e;
    uint8_t *base = praw() + lj_pe_pal[p].pal[pal_index(p)];
    const char *n = it && it->name ? it->name : "";
    (void)w; (void)ud;
    if (strcmp(n, "P_lvInsert") == 0 && r >= 0) { memmove(base + 57 * (r + 1), base + 57 * r, (size_t)57 * (LJ_PAL_ENTRIES - 1 - r)); memset(base + 57 * r, 0, 57); }
    else if (strcmp(n, "P_lvDelete") == 0 && r >= 0) { memmove(base + 57 * r, base + 57 * (r + 1), (size_t)57 * (LJ_PAL_ENTRIES - 1 - r)); memset(base + 57 * (LJ_PAL_ENTRIES - 1), 0, 57); }
    else if (strcmp(n, "P_lvClear") == 0 && r >= 0) memset(base + 57 * r, 0, 57);
    else if (strcmp(n, "P_ClearAll") == 0) memset(base, 0, (size_t)57 * LJ_PAL_ENTRIES);
    else if (strcmp(n, "P_lvCompact") == 0) {
        int to = 0;
        for (e = 0; e < LJ_PAL_ENTRIES; e++) if (base[57 * e]) { if (to != e) memmove(base + 57 * to, base + 57 * e, 57); to++; }
        if (to < LJ_PAL_ENTRIES) memset(base + 57 * to, 0, (size_t)57 * (LJ_PAL_ENTRIES - to));
    } else if (strcmp(n, "P_CopyColorPalette") == 0 || strcmp(n, "P_CopyGoboPalette") == 0) { memcpy(pe.clip, base, sizeof pe.clip); pe.clip_ok = 1; pe.clip_color = n[6] == 'C'; return; }
    else if (strcmp(n, "p_PastePalette") == 0) { if (!pe.clip_ok || pe.clip_color != (p == 0)) return; memcpy(base, pe.clip, sizeof pe.clip); }   /* colour only from colour; gobo / effects interchangeable [02 s5.4] */
    else return;
    pe.ed.dirty = 1;
    pe_paint_lists();
}
static void pe_on_ov_clear(lj_widget *w, void *ud) { (void)w; (void)ud; edit_quiet(pfind("eMan"), ""); edit_quiet(pfind("eModel"), ""); edit_quiet(pfind("eMode"), ""); pe.ed.dirty = 1; }   /* the OV link strings; the OV list itself is inert (D12) */

static const lj_binding lj_pe_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormClose", pe_on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "bCancelClick", pe_on_cancel, NULL }, { "bApplyClick", pe_on_apply, NULL }, { "bApplyAllClick", pe_on_apply_all, NULL },
    { "PC1Change", pe_on_page, NULL }, { "PaletteTabChange", pe_on_pal_tab, NULL },
    { "cbEnableClick", pe_on_dirty, NULL }, { "cbTransparentClick", pe_on_dirty, NULL }, { "NotesChange", lj_handler_noop, NULL },
    { "bSetIconBMPClick", pe_on_set_icon, NULL }, { "bClearIconBMPClick", pe_on_clear_icon, NULL },
    { "lvChange", pe_on_lv, NULL }, { "lvClick", pe_on_lv, NULL }, { "lvBitmapDragDrop", lj_handler_noop, NULL }, { "lvBitmapDragOver", lj_handler_noop, NULL },
    { "bSetValClick", pe_on_set_val, NULL }, { "bClearClick", pe_on_clear_val, NULL }, { "eValKeyDown", lj_handler_noop, NULL },
    { "eColorValKeyDown", lj_handler_noop, NULL }, { "eGoboValKeyDown", lj_handler_noop, NULL }, { "eEffectsValKeyDown", lj_handler_noop, NULL },
    { "bGetBMPClick", pe_on_get_bmp, NULL }, { "bSelectColorColorClick", pe_on_colors, NULL }, { "bSelectValTypeClick", pe_on_val_type, NULL },
    { "lbFunctionsClick", pe_on_functions, NULL }, { "lbFunctionsDrawItem", lj_handler_noop, NULL }, { "eFunctionNameChange", pe_on_function_name, NULL },
    { "bDeleteFunctionClick", pe_on_delete_function, NULL },
    { "lvLevelsClick", pe_on_levels_click, NULL }, { "bLevelStatusClick", pe_on_level_status, NULL }, { "eLevelNameChange", pe_on_level_name, NULL }, { "eLevelNameKeyDown", lj_handler_noop, NULL },
    { "lvDefsChange", lj_handler_noop, NULL }, { "lvDefsResize", lj_handler_noop, NULL }, { "lvDefsSelectItem", lj_handler_noop, NULL }, { "Button1Click", pe_on_ov_clear, NULL },
    { "M_FilesClick", pe_on_files, NULL }, { "M_LoadClick", pe_on_load, NULL }, { "M_SaveClick", pe_on_save, NULL }, { "M_SaveAsClick", pe_on_save_as, NULL },
    { "ClearProfile1Click", pe_on_clear_profile, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "lv_PopUp", lj_handler_noop, NULL }, { "P_lvClick", pe_on_lv_popup, NULL },
    { NULL, NULL, NULL } };

static void pe_oldstyle_done(const char *manufacturer, const char *name, void *ud)   /* ME-126: the split identity into the 2.6b9 slots; takes effect on Apply / Save */
{
    (void)ud;
    if (!pe.win) return;
    lj_pe_set_str(&pe.ed, lj_pe_field_find("cbManufacturer"), 0, manufacturer);
    lj_pe_set_str(&pe.ed, field_at(0x3E37), 0, name);
    pe_paint();
}
lj_window *lj_profile_editor_open(void)
{
    lj_ctx *ctx = lj_ctx_get();
    const lj_profile *p;
    lj_widget *w;
    int i, n = 0, k;
    if (pe.win) return pe.win;
    pe.fixture = lj_sel_master();
    p = lj_show_profile(ctx->show, pe.fixture);
    if (lj_pe_begin(&pe.ed, p && p->gs2_raw ? p : NULL) != 0) lj_pe_begin(&pe.ed, NULL);   /* the USER2 placeholder: New Profile */
    pe.legacy = p && p->gs2_raw && p->format_version != LJ_GS2_VERSION;   /* ME-126: the editor holds it as 2609 already (lj_pe_begin); the name box offers the split */
    if ((pe.win = lj_window_create(&lj_form_TSCANNER2DEFSFORM, lj_pe_bindings)) == NULL) { lj_log("profile editor: cannot create TScanner2DefsForm"); return NULL; }
    if ((w = pfind("cbDipSwitch")) != NULL) w->enabled = 0;   /* not located in the record (16 s10): shown, inert */
    if ((w = pfind("eDipOfs")) != NULL) w->enabled = 0;
    if ((w = pfind("DipSwitch1")) != NULL) w->visible = lj_w_checkbox_checked(pfind("cbDipSwitch"));   /* LORD_FIDELITY-7 / LORD_FIXTURES-5: the switch art only with Show Adr DIPSwitch (userdeffixtureinfotab.gif; PROPOSAL rig: whether LJ2 hides it at FormShow or on the click) */
    for (i = 0; i < lj_fixlib_count() && n < LJ_PROFILE_CH; i++) {   /* the combo's list: manufacturers of the exe rows, first-seen order */
        const lj_exe_row *r = lj_fixlib_row(i);
        if (!r || !r->manufacturer || !r->manufacturer[0]) continue;
        for (k = 1; k <= n && strcmp(pe.manu_buf[k], r->manufacturer) != 0; k++) { }
        if (k <= n || n + 1 >= LJ_PROFILE_CH) continue;
        lj_str_copy(pe.manu_buf[++n], sizeof pe.manu_buf[0], r->manufacturer);
        pe.manu.p[n - 1] = pe.manu_buf[n];
    }
    if ((w = pfind("cbManufacturer")) != NULL) lj_w_combobox_set_items(w, pe.manu.p, n);
    pe_paint();
    lj_win_show(pe.win, 0);
    if (pe.legacy) { char legacy[32]; sstr_get(0x3E37, 30, legacy, sizeof legacy); lj_config_dlg_oldstyle_name(legacy, pe_oldstyle_done, NULL); }   /* TProfileNameOldStyleForm "Oldstyle profile name" */
    return pe.win;
}
void lj_profile_editor_close(void) { rgb_close(); close_win(&pe.win); }
lj_window *lj_profile_editor_get(void) { return pe.win; }
void lj_profile_editor_refresh(void)
{
    lj_widget *cb = pfind("cbManufacturer");
    if (cb && ((lj_w_combobox *)cb)->index >= 0 && ((lj_w_combobox *)cb)->index < ((lj_w_combobox *)cb)->n) {   /* a pick from the list sets the Manufacturer text */
        sstr_put(0x3E56, 30, ((lj_w_combobox *)cb)->items[((lj_w_combobox *)cb)->index]);
        ((lj_w_combobox *)cb)->index = -1;
        sstr_get(0x3E56, 30, pe.manu_buf[0], sizeof pe.manu_buf[0]);
        ((lj_w_combobox *)cb)->text = pe.manu_buf[0];
        lj_widget_invalidate(cb);
    }
}
lj_profile_edit *lj_profile_editor_work(void) { return pe.win ? &pe.ed : NULL; }

/* =====================================================================================================================
 * Customize Fixture (TFixtureSetupForm) + Copy Palettes from (TCopySetupForm)
 * ===================================================================================================================== */
static const char *const lj_cf_pal_names[LJ_PALETTES] = { "Color 1", "Color 2", "Gobo 1", "Gobo 2", "Effects 1", "Effects 2" };   /* PROPOSAL rig: the CHM names them "color (wheel) 1" */
static struct {
    lj_window *win, *copy;
    int fixture, ntabs, tab_pal[LJ_PALETTES];
    lj_palette work[LJ_PALETTES];
    const char *tabs[LJ_PALETTES];
    char rows[LJ_PAL_ENTRIES][80]; const char *rowp[LJ_PAL_ENTRIES]; int row_e[LJ_PAL_ENTRIES], nrows;
    char crow[LJ_FIXTURES][96]; const char *crowp[LJ_FIXTURES]; int crow_f[LJ_FIXTURES], ncrows;
} cf;

static lj_widget *cfind(const char *n) { return cf.win ? lj_window_find(cf.win, n) : NULL; }
static const lj_profile *cf_profile(void) { return lj_show_profile(lj_ctx_get()->show, cf.fixture); }
static int cf_pal(void) { lj_widget *t = cfind("tc1"); int a = t ? ((lj_w_tabs *)t)->active : 0; return a >= 0 && a < cf.ntabs ? cf.tab_pal[a] : -1; }
static int cf_entry(void) { int r = row_of(cfind("LV1")); return r >= 0 && r < cf.nrows ? cf.row_e[r] : -1; }

static void cf_paint(void)
{
    const lj_profile *p = cf_profile();
    int pi = cf_pal(), e;
    cf.nrows = 0;
    for (e = 0; p && pi >= 0 && e < LJ_PAL_ENTRIES; e++) {
        const lj_pal_entry *x = &p->palette[pi].e[e], *c = &cf.work[pi].e[e];
        uint32_t rgb = c->used ? c->rgb : (x->bitmap[0] ? LJ_CUSTOM_NO_RGB : x->rgb);
        const char *bmp = c->used ? c->bitmap : x->bitmap;
        char hex[16];
        if (!x->used) continue;
        if (rgb == LJ_CUSTOM_NO_RGB) hex[0] = 0; else snprintf(hex, sizeof hex, "%06X", (unsigned)(rgb & 0xFFFFFF));
        snprintf(cf.rows[cf.nrows], sizeof cf.rows[0], "%d\t%s\t%s", e, hex, bmp);
        cf.rowp[cf.nrows] = cf.rows[cf.nrows]; cf.row_e[cf.nrows++] = e;
    }
    if (cfind("LV1")) { int keep = row_of(cfind("LV1")); lj_w_listview_set_items(cfind("LV1"), cf.rowp, cf.nrows); ((lj_w_listview *)cfind("LV1"))->index = keep < cf.nrows ? keep : -1; lj_widget_invalidate(cfind("LV1")); }
}
static void cf_set(int e, uint32_t rgb, const char *bmp)   /* one override: "either an RGB colour or a bitmap, but not both" [02 s4.3] */
{
    int pi = cf_pal();
    if (pi < 0 || e < 0) return;
    cf.work[pi].e[e].used = 1; cf.work[pi].e[e].rgb = rgb;
    lj_str_copy(cf.work[pi].e[e].bitmap, sizeof cf.work[pi].e[e].bitmap, bmp);
    cf_paint();
}
static void cf_rgb_done(uint32_t rgb) { if (cf.win) cf_set(cf_entry(), rgb, ""); }
static void cf_on_rgb(lj_widget *w, void *ud) { int pi = cf_pal(), e = cf_entry(); (void)w; (void)ud; if (pi >= 0 && e >= 0) rgb_open(cf.work[pi].e[e].used ? cf.work[pi].e[e].rgb : cf_profile()->palette[pi].e[e].rgb, cf_rgb_done); }
static int cf_bmp_entry = -1;
static void cf_bmp_done(const char *name, void *ud) { (void)ud; if (cf.win && cf_bmp_entry >= 0) cf_set(cf_bmp_entry, LJ_CUSTOM_NO_RGB, name); }
static void cf_on_bmp(lj_widget *w, void *ud)   /* Gobo/Effects Bitmaps / Color Bitmaps: TBitMapPickerForm2 over our tiles (D2) */
{
    (void)ud;
    if (cf_pal() < 0 || (cf_bmp_entry = cf_entry()) < 0) return;
    lj_config_dlg_pick_bitmap(strcmp(w->name, "bGetColorBMP") == 0, cf_bmp_done, NULL);
}
static void cf_on_clear(lj_widget *w, void *ud) { (void)w; (void)ud; cf_set(cf_entry(), LJ_CUSTOM_NO_RGB, ""); }
static void cf_on_menu(lj_widget *w, void *ud)
{
    const lj_menu_item_desc *it = cf.win ? lj_window_menu_item(cf.win) : NULL;
    const lj_profile *p = cf_profile();
    const char *n = it && it->name ? it->name : "";
    int pi = cf_pal(), k, e;
    (void)w; (void)ud;
    if (strcmp(n, "M_DefaultPalette") == 0 && pi >= 0) memset(&cf.work[pi], 0, sizeof cf.work[pi]);
    else if (strcmp(n, "M_DefaultAllPalettes") == 0) memset(cf.work, 0, sizeof cf.work);
    else for (k = 0; p && k < LJ_PALETTES; k++) {
        if (!(strcmp(n, "M_ClearAll") == 0 || (strcmp(n, "M_ClearPalette") == 0 && k == pi))) continue;
        for (e = 0; e < LJ_PAL_ENTRIES; e++) if (p->palette[k].e[e].used) { cf.work[k].e[e].used = 1; cf.work[k].e[e].rgb = LJ_CUSTOM_NO_RGB; cf.work[k].e[e].bitmap[0] = 0; }
    }
    cf_paint();
}
static void cf_on_apply(lj_widget *w, void *ud)
{
    lj_show *sh = lj_ctx_get()->show;
    int f = cf.fixture;
    (void)w; (void)ud;
    if (sh && f >= 0 && f < LJ_FIXTURES) { lj_lock(); memcpy(sh->fixture[f].custom, cf.work, sizeof cf.work); lj_unlock(); lj_desktop_run_action(LJ_ACT_FIXTURE_APPLY, f); }
    lj_customize_fixture_close();
}
static void cf_on_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; lj_customize_fixture_close(); }
static void cf_on_tab(lj_widget *w, void *ud) { (void)w; (void)ud; if (cfind("LV1")) ((lj_w_listview *)cfind("LV1"))->index = -1; cf_paint(); }

/* Copy Palettes from: the source list (same type only by default), six palette boxes, OK enabled by a pick */
static lj_widget *cpfind(const char *n) { return cf.copy ? lj_window_find(cf.copy, n) : NULL; }
static void cp_fill(void)
{
    const lj_show *sh = lj_ctx_get()->show;
    int f, same = lj_w_checkbox_checked(cpfind("cb1"));
    char t[64];
    cf.ncrows = 0;
    for (f = 0; sh && f < LJ_FIXTURES; f++) {
        if (f == cf.fixture || !sh->fixture[f].link || (same && !lj_sel_same_type(f, cf.fixture))) continue;
        snprintf(cf.crow[cf.ncrows], sizeof cf.crow[0], "%03d\t%s", f + 1, profile_text(lj_show_profile(sh, f), t, sizeof t));
        cf.crowp[cf.ncrows] = cf.crow[cf.ncrows]; cf.crow_f[cf.ncrows++] = f;
    }
    if (cpfind("lv1")) { lj_w_listview_set_items(cpfind("lv1"), cf.crowp, cf.ncrows); ((lj_w_listview *)cpfind("lv1"))->index = -1; }
    if (cpfind("bOk")) { cpfind("bOk")->enabled = 0; lj_widget_invalidate(cpfind("bOk")); }
}
static void cp_close(void) { close_win(&cf.copy); }
static void cp_ok(lj_widget *w, int ev, void *ud)
{
    static const char *const box[LJ_PALETTES] = { "cbColor1", "cbColor2", "cbGobo1", "cbGobo2", "cbEffects1", "cbEffects2" };
    const lj_show *sh = lj_ctx_get()->show;
    int r = row_of(cpfind("lv1")), k;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (sh && r >= 0 && r < cf.ncrows)
        for (k = 0; k < LJ_PALETTES; k++) if (lj_w_checkbox_checked(cpfind(box[k]))) cf.work[k] = sh->fixture[cf.crow_f[r]].custom[k];
    cp_close();
    cf_paint();
}
static void cp_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) cp_close(); }
static void cp_select(lj_widget *w, void *ud)
{
    static const char *const box[] = { "cbColor1", "cbColor2", "cbGobo1", "cbGobo2", "cbEffects1", "cbEffects2" };
    size_t k;
    (void)ud;
    for (k = 0; k < LJ_COUNT_OF(box); k++) check_quiet(cpfind(box[k]), strcmp(w->name, "bSelectAll") == 0);
}
static void cp_change(lj_widget *w, void *ud) { (void)ud; if (cpfind("bOk")) { cpfind("bOk")->enabled = row_of(w) >= 0; lj_widget_invalidate(cpfind("bOk")); } }
static void cp_same(lj_widget *w, void *ud) { (void)w; (void)ud; cp_fill(); }
static const lj_binding lj_cp_bindings[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "bSelectAllClick", cp_select, NULL }, { "bSelectNoneClick", cp_select, NULL }, { "cb1Click", cp_same, NULL }, { "cbGobo1Click", lj_handler_noop, NULL },
    { "lv1Change", cp_change, NULL }, { NULL, NULL, NULL } };
static void cf_on_copy(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    cp_close();
    if ((cf.copy = lj_window_create(&lj_form_TCOPYSETUPFORM, lj_cp_bindings)) == NULL) { lj_log("customize fixture: cannot create TCopySetupForm"); return; }
    set_bitbtn(cf.copy, "bOk", cp_ok); set_bitbtn(cf.copy, "bCancel", cp_cancel);
    cp_fill();
    lj_win_show(cf.copy, 0);
}

static const lj_binding lj_cf_bindings[] = {
    { "FormClose", cf_on_cancel, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "pc1Change", lj_handler_noop, NULL }, { "tc1Change", cf_on_tab, NULL }, { "LV1Change", lj_handler_noop, NULL }, { "LV1DragDrop", lj_handler_noop, NULL }, { "LV1DragOver", lj_handler_noop, NULL },
    { "lvDefsChange", lj_handler_noop, NULL }, { "lvDefsSelectItem", lj_handler_noop, NULL }, { "bClearOVClick", lj_handler_noop, NULL }, { "DefaultOVLink1Click", lj_handler_noop, NULL },   /* the O/V tab: present, inert (D12) */
    { "bApplyClick", cf_on_apply, NULL }, { "bCancelClick", cf_on_cancel, NULL },
    { "bGetGoboBmpClick", cf_on_bmp, NULL }, { "bGetColorBMPClick", cf_on_bmp, NULL }, { "bGetRGBClick", cf_on_rgb, NULL },
    { "bClearClick", cf_on_clear, NULL }, { "B_CopyClick", cf_on_copy, NULL },
    { "M_DefaultPaletteClick", cf_on_menu, NULL }, { "M_DefaultAllPalettesClick", cf_on_menu, NULL }, { "M_ClearPaletteClick", cf_on_menu, NULL }, { "M_ClearAllClick", cf_on_menu, NULL },
    { NULL, NULL, NULL } };

/* the fixture icon popup item [01 s3.6, LORD_FIDELITY-26]: built-ins say "Customize Fixture", user-definable and generic
 * dimmer / switch profiles "Define Fixture Profile" (the latter opening Channel Configuration, CHM genericdmxprofiles) */
enum { CF_CUSTOMIZE = 0, CF_DEFINE_PROFILE, CF_DEFINE_CHANNELS, CF_NONE, CF_DEFINE_SCANNER1, CF_DEFINE_SCROLLER, CF_DEFINE_RGBPACK };
static int cf_kind(int f)
{
    const lj_show *sh = lj_ctx_get()->show;
    const lj_profile *p;
    if (!sh || f < 0 || f >= LJ_FIXTURES || !sh->fixture[f].link) return CF_NONE;
    if (sh->fixture[f].kind == LJ_FX_GS2 || sh->fixture[f].profile_id == 46) return CF_DEFINE_PROFILE;
    if (sh->fixture[f].profile_id == 43) return CF_DEFINE_SCANNER1;   /* User Definable Generic Scanner 1 (02 s5.5) */
    if (sh->fixture[f].profile_id == 42) return CF_DEFINE_SCROLLER;   /* User Definable Color Scroller 1 (02 s5.6) */
    p = lj_fixlib_by_id(sh->fixture[f].profile_id);
    if (p && strcmp(p->manufacturer, "Generic") == 0 && strcmp(p->name, "RGB Pack") == 0) return CF_DEFINE_RGBPACK;   /* "for RGB packs the RGB pack configuration dialog" (CHM genericrgbpack.htm) */
    if (p && strcmp(p->manufacturer, "Generic") == 0 && (strcmp(p->name, "DMX Dimmer") == 0 || strcmp(p->name, "DMX Switch Pack") == 0)) return CF_DEFINE_CHANNELS;
    return CF_CUSTOMIZE;
}
const char *lj_customize_popup_caption(int fixture) { return cf_kind(fixture) == CF_CUSTOMIZE || cf_kind(fixture) == CF_NONE ? "Customize Fixture" : "Define Fixture Profile"; }

lj_window *lj_customize_fixture_open(void)
{
    const lj_profile *p;
    lj_widget *t;
    char buf[96];
    int k, e, f = lj_sel_master();
    switch (cf_kind(f)) {                                     /* LJ_ACT_CUSTOMIZE_FIXTURE lands here for every profile kind */
    case CF_DEFINE_PROFILE:  return lj_win_open(LJ_WIN_PROFILE_EDITOR);
    case CF_DEFINE_CHANNELS: return lj_win_open(LJ_WIN_CHANNEL_CONFIG);
    case CF_DEFINE_SCANNER1: lj_config_dlg_target(f); return lj_win_open(LJ_WIN_SCANNER1_DEFS);
    case CF_DEFINE_SCROLLER: lj_config_dlg_target(f); return lj_win_open(LJ_WIN_COLOR_SCROLLER_DEFS);
    case CF_DEFINE_RGBPACK:  lj_config_dlg_target(f); return lj_win_open(LJ_WIN_RGBPACK_DEFS);
    case CF_NONE: return NULL;
    default: break;
    }
    if (cf.win) return cf.win;
    cf.fixture = f;
    p = cf_profile();
    memcpy(cf.work, lj_ctx_get()->show->fixture[f].custom, sizeof cf.work);
    if ((cf.win = lj_window_create(&lj_form_TFIXTURESETUPFORM, lj_cf_bindings)) == NULL) { lj_log("customize fixture: cannot create TFixtureSetupForm"); return NULL; }
    snprintf(buf, sizeof buf, "%03d", f + 1); lj_w_set_caption(cfind("lFixtureNo"), buf);
    lj_w_set_caption(cfind("lFixtureType"), profile_text(p, buf, sizeof buf));
    cf.ntabs = 0;
    for (k = 0; p && k < LJ_PALETTES; k++) {
        for (e = 0; e < LJ_PAL_ENTRIES && !p->palette[k].e[e].used; e++) { }
        if (e < LJ_PAL_ENTRIES) { cf.tabs[cf.ntabs] = lj_cf_pal_names[k]; cf.tab_pal[cf.ntabs++] = k; }
    }
    lj_w_tabs_select(cfind("pc1"), 0);                        /* Images in front (the DFM's ActivePage is the O/V page, inert under D12) */
    if ((t = cfind("tc1")) != NULL && cf.ntabs) { ((lj_w_tabs *)t)->tabs = cf.tabs; ((lj_w_tabs *)t)->ntabs = cf.ntabs; ((lj_w_tabs *)t)->active = 0; lj_widget_invalidate(t); }
    cf_paint();
    lj_win_show(cf.win, 0);
    return cf.win;
}
void lj_customize_fixture_close(void) { cp_close(); close_win(&cf.win); }
lj_window *lj_customize_fixture_get(void) { return cf.win; }
void lj_customize_fixture_refresh(void) { }
const lj_palette *lj_customize_fixture_work(void) { return cf.win ? cf.work : NULL; }

/* =====================================================================================================================
 * Channel Configuration (TChannelDefForm, ME-32)
 * ===================================================================================================================== */
static struct { lj_window *win; int fixture, n; char cap[LJ_PROFILE_CH][7]; uint32_t gm_off; lj_rows rows; } cc;

static lj_widget *ccfind(const char *n) { return cc.win ? lj_window_find(cc.win, n) : NULL; }
static void cc_paint(void)
{
    const lj_profile *p = lj_show_profile(lj_ctx_get()->show, cc.fixture);
    int k;
    cc.rows.n = cc.n;
    for (k = 0; k < cc.n; k++) {
        const char *c = cc.cap[k][0] ? cc.cap[k] : p && p->level_caption[k][0] ? (const char *)p->level_caption[k] : "";
        char dflt[16];   /* "CH" + the full %d range (gcc -Wformat-truncation) */
        if (!c[0]) { snprintf(dflt, sizeof dflt, "CH%d", k + 1); c = dflt; }   /* the Levels window's default caption */
        snprintf(cc.rows.t[k], sizeof cc.rows.t[k], "%d\t%s\t%s", k + 1, c, cc.gm_off >> k & 1u ? "No" : "Yes");   /* channelconfiguration.gif: "Yes"; "No" its pair (PROPOSAL rig) */
    }
    rows_set(ccfind("chList"), &cc.rows);
}
static void cc_on_list(lj_widget *w, void *ud)
{
    int r = row_of(w);
    void (*fn)(lj_widget *, int, void *);
    lj_widget *e = ccfind("eChannelName");
    (void)ud;
    if (r < 0 || !e) return;
    fn = e->on_event; e->on_event = NULL; lj_w_edit_set_text(e, cc.cap[r]); e->on_event = fn;
}
static void cc_on_name(lj_widget *w, void *ud) { int r = row_of(ccfind("chList")); (void)ud; if (r < 0) return; lj_str_copy(cc.cap[r], sizeof cc.cap[r], lj_w_edit_text(w)); cc_paint(); }   /* 6 bytes: TCHANNELDEFFORM eChannelName MaxLength = 6, ch_caption[7] (ME-118) */
static void cc_on_gm(lj_widget *w, void *ud)
{
    int r = row_of(ccfind("chList"));
    (void)ud;
    if (r < 0) return;
    if (strcmp(w->name, "bDisableGM") == 0) cc.gm_off |= 1u << r; else cc.gm_off &= ~(1u << r);
    cc_paint();
}
static void cc_ok(lj_widget *w, int ev, void *ud)
{
    lj_show *sh = lj_ctx_get()->show;
    (void)w; (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (sh && cc.fixture >= 0) {
        lj_lock();
        memcpy(sh->fixture[cc.fixture].ch_caption, cc.cap, sizeof cc.cap);
        sh->fixture[cc.fixture].ch_gm_off = cc.gm_off;
        lj_unlock();
        lj_desktop_run_action(LJ_ACT_FIXTURE_APPLY, cc.fixture);
    }
    lj_channel_config_close();
}
static void cc_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) lj_channel_config_close(); }
static const lj_binding lj_cc_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { "Help1Click", lj_handler_noop, NULL },
    { "chListChange", cc_on_list, NULL }, { "eChannelNameChange", cc_on_name, NULL }, { "bEnableGMClick", cc_on_gm, NULL }, { "bDisableGMClick", cc_on_gm, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_channel_config_open(void)
{
    lj_show *sh = lj_ctx_get()->show;
    const lj_profile *p;
    char buf[96];
    int f = lj_sel_master();
    if (cc.win) return cc.win;
    if (!sh || f < 0 || !sh->fixture[f].link) return NULL;
    cc.fixture = f;
    p = lj_show_profile(sh, f);
    cc.n = p && p->footprint ? p->footprint : 1;
    memcpy(cc.cap, sh->fixture[f].ch_caption, sizeof cc.cap);
    cc.gm_off = sh->fixture[f].ch_gm_off;
    if ((cc.win = lj_window_create(&lj_form_TCHANNELDEFFORM, lj_cc_bindings)) == NULL) { lj_log("channel configuration: cannot create TChannelDefForm"); return NULL; }
    set_bitbtn(cc.win, "BitBtn2", cc_ok); set_bitbtn(cc.win, "BitBtn1", cc_cancel);
    {   char t[112]; snprintf(t, sizeof t, "(%d) %s", f + 1, profile_text(p, buf, sizeof buf)); lj_w_set_caption(ccfind("lFixType"), t); }   /* "(2) Generic.." (channelconfiguration.gif; exe "(%d) %s") */
    lj_w_set_caption(ccfind("lUsername"), sh->fixture[f].user_name);
    snprintf(buf, sizeof buf, "%d/%d", sh->fixture[f].link, sh->fixture[f].address); lj_w_set_caption(ccfind("lDmxAdr"), buf);   /* "1/38" (channelconfiguration.gif) */
    if (ccfind("eChannelName")) { ((lj_w_edit *)ccfind("eChannelName"))->max_length = 6; edit_quiet(ccfind("eChannelName"), ""); }
    cc_paint();
    lj_win_show(cc.win, 0);
    return cc.win;
}
void lj_channel_config_close(void) { close_win(&cc.win); }
lj_window *lj_channel_config_get(void) { return cc.win; }
void lj_channel_config_refresh(void) { }

void lj_profile_editor_form_bind_fns(void) { }   /* no hotkey of 01 s7 opens these three */
