/* lj_profile_editor.c - the editor model of lj_profile_editor.h. The tab captions and the field table (note 16 s4.2-4.12,
 * DFM TScanner2DefsForm control names) and the edit bodies over the raw working record (profile-editor track). */
#include <string.h>
#include "fixlib/lj_profile_editor.h"
#include "core/lj_str.h"
#include "fixlib/lj_gs2.h"

const char *const lj_pe_tab_caption[LJ_PE_TAB_COUNT] = {
    "Fixture Info", "Intensity & Lamp", "Pan & Tilt", "Fixed Color Control", "CMY & RGB Control", "Gobo Control",
    "Beam Control", "Effects Control", "Specials", "Levels", "Offline Visualizer" };

#define F(tab, kind, off, n, ctl, label) { LJ_PE_##tab, LJ_PEF_##kind, off, n, ctl, label }
const lj_pe_field lj_pe_fields[] = {
    /* 16 s4.2 Fixture Info (TabSheet10); cbDipSwitch / eDipOfs have no located field (16 s10) and are not listed */
    F(FIXTURE_INFO, STR,   0x0028, 10,   "eIconCaption",   "Fixture Icon Caption"),
    F(FIXTURE_INFO, STR,   0x0033, 15,   "eSelectionId",   "Fixture ID"),
    F(FIXTURE_INFO, STR,   0x0043, 30,   "eAuthor",        "Author"),
    F(FIXTURE_INFO, CHARS, 0x0062, 4097, "Notes",          "Profile Notes (Max 4096 Chars)"),
    F(FIXTURE_INFO, STR,   0x1063, 36,   "lBitmapName",    "Fixture Icon Bitmap"),
    F(FIXTURE_INFO, U8,    0x10A8, 1,    "cbTransparent",  "Transparent"),
    F(FIXTURE_INFO, STR,   0x3E56, 30,   "cbManufacturer", "Manufacturer"),
    F(FIXTURE_INFO, STR,   0x3E37, 30,   "eFixtureName",   "Fixture Name"),
    F(FIXTURE_INFO, STR,   0x3E75, 30,   "eFixtureMode",   "Fixture Mode"),
    /* 16 s4.3 Intensity & Lamp (TabSheet1) */
    F(INTENSITY_LAMP, DYN,    0x1173, 1, "cbShutter",      "Enable Shutter Function"),
    F(INTENSITY_LAMP, DYN,    0x1184, 1, "cbStrobe",       "Enable Strobe Function"),
    F(INTENSITY_LAMP, U8,     0x1195, 1, "eStrobeOff",     "Strobe Off value"),
    F(INTENSITY_LAMP, DYN,    0x1196, 1, "cbIntensity",    "Enable Intensity Function"),
    F(INTENSITY_LAMP, U8,     0x11A7, 1, "cbInvertInt",    "Invert entire Intensity channel"),
    F(INTENSITY_LAMP, U8,     0x11A8, 1, "cbUseGM",        "Enable Grand Master/Intensity"),
    F(INTENSITY_LAMP, U8,     0x11A9, 1, "cbBOGM",         "Blackout when Grand Master at 0"),
    F(INTENSITY_LAMP, STATIC, 0x11AA, 1, "lvBO",           "Blackout Values"),
    F(INTENSITY_LAMP, STATIC, 0x1201, 1, "cbLampOn",       "Enable Lamp On"),
    F(INTENSITY_LAMP, STATIC, 0x1258, 1, "cbLampOff",      "Enable Lamp Off"),
    F(INTENSITY_LAMP, I32,    0x3D4D, 1, "eLampOnTime",    "Auto LampOn Time"),
    F(INTENSITY_LAMP, U8,     0x3D51, 1, "cbInvertIntVal", "Invert Intensity value range"),
    F(INTENSITY_LAMP, U8,     0x3D52, 1, "cbLimitGM",      "Limit GM to intensity range"),
    /* 16 s4.4 Pan & Tilt (TabSheet2) */
    F(PAN_TILT, U8,  0x1306, 1, "cbPanTilt16",   "Enable Pan/Tilt Control 16 Bit MSB/LSB"),
    F(PAN_TILT, U8,  0x1307, 1, "cbPanTilt8",    "Enable Pan/Tilt Control 8 Bit"),
    F(PAN_TILT, U8,  0x1308, 1, "ePanOfs",       "Pan DMX Offset"),
    F(PAN_TILT, U8,  0x1309, 1, "eTiltOfs",      "Tilt DMX Offset"),
    F(PAN_TILT, U8,  0x130A, 1, "cbPanTiltPatch", "Enable Pan/Tilt Patch"),
    F(PAN_TILT, U8S, 0x130B, 4, "ePanPatchMSB",  "Patch Pan MSB / Pan LSB / Tilt MSB / Tilt LSB to"),
    F(PAN_TILT, U8,  0x130F, 1, "cbPresets",     "Enable Position Presets"),
    F(PAN_TILT, U8,  0x1310, 1, "cbMacros",      "Enable Movement Macros"),
    F(PAN_TILT, DYN, 0x1311, 1, "cbMove1",       "Enable Misc Movement Control"),
    F(PAN_TILT, U8,  0x37EA, 1, "cbFollow",      "Enable Follow Spot Function"),
    /* 16 s4.5 Fixed Color Control (ColorTab) */
    F(FIXED_COLOR, DYN, 0x1322, 1, "cbColor1", "Enable Color 1 Control (Palette)"),
    F(FIXED_COLOR, DYN, 0x1333, 1, "cbColor2", "Enable Color 2 Control (Palette)"),
    F(FIXED_COLOR, DYN, 0x1344, 1, "cbColor3", "Enable Color 3 Control"),
    F(FIXED_COLOR, DYN, 0x1355, 1, "cbColor4", "Enable Color 4 Control"),
    F(FIXED_COLOR, PAL, 0x1377, 20, "lvColor", "Color 1 Palette"),
    F(FIXED_COLOR, PAL, 0x17EB, 20, "lvColor", "Color 2 Palette"),
    F(FIXED_COLOR, U8S, 0x37D1, 5, "lvColor",  "Color 1 palette type codes"),
    F(FIXED_COLOR, U8S, 0x37D6, 5, "lvColor",  "Color 2 palette type codes"),
    /* 16 s4.6 CMY & RGB Control (TabSheet9) */
    F(CMY_RGB, U8,  0x1C5F, 1, "cbCMY",          "Enable CMY Control"),
    F(CMY_RGB, U8,  0x1C60, 1, "cbRGB",          "Enable RGB Control"),
    F(CMY_RGB, U8,  0x1C61, 1, "cbCMYPicker",    "Enable CMY Color Picker"),
    F(CMY_RGB, U8,  0x1C62, 1, "cbInvertPicker", "Invert CMY/RGB Values"),
    F(CMY_RGB, DYN, 0x1C63, 1, "eCyanOfs",       "Cyan/Red"),
    F(CMY_RGB, DYN, 0x1C74, 1, "eMagentaOfs",    "Magenta/Green"),
    F(CMY_RGB, DYN, 0x1C85, 1, "eYellowOfs",     "Yellow/Blue"),
    F(CMY_RGB, U8,  0x3E34, 1, "cbRGBHue",       "Enable RGB hue control"),
    F(CMY_RGB, U8,  0x3E35, 1, "cbRGBSat",       "Enable RGB saturation control"),
    F(CMY_RGB, U8,  0x3E36, 1, "cbRGBVal",       "Enable RGB value control"),
    /* 16 s4.7 Gobo Control (GoboTab) */
    F(GOBO, DYN, 0x1CA7, 1, "cbGobo1", "Enable Gobo 1 Control (Palette)"),
    F(GOBO, DYN, 0x1CB8, 1, "cbGobo2", "Enable Gobo 2 Control (Palette)"),
    F(GOBO, DYN, 0x1CC9, 1, "cbGobo3", "Enable Gobo 3 Control"),
    F(GOBO, DYN, 0x1CDA, 1, "cbGobo4", "Enable Gobo 4 Control"),
    F(GOBO, PAL, 0x1CFC, 20, "lvGobo", "Gobo 1 Palette"),
    F(GOBO, PAL, 0x2170, 20, "lvGobo", "Gobo 2 Palette"),
    F(GOBO, U8S, 0x37DB, 5, "lvGobo",  "Gobo 1 palette type codes"),
    F(GOBO, U8S, 0x37E0, 5, "lvGobo",  "Gobo 2 palette type codes"),
    /* 16 s4.8 Beam Control (TabSheet5): stride 17 from 0x25E4 */
    F(BEAM, DYN, 0x25E4, 1, "cbIris",  "Iris"),
    F(BEAM, DYN, 0x25F5, 1, "cbFocus", "Focus"),
    F(BEAM, DYN, 0x2606, 1, "cbZoom",  "Zoom"),
    F(BEAM, DYN, 0x2617, 1, "cbFrost", "Frost"),
    F(BEAM, DYN, 0x2628, 1, "cbBeam1", "Beam 1"),
    F(BEAM, DYN, 0x2639, 1, "cbBeam2", "Beam 2"),
    /* 16 s4.9 Effects Control (EffectsTab) */
    F(EFFECTS, DYN, 0x264A, 1, "cbEffects1", "Enable Effects 1 Control (Palette)"),
    F(EFFECTS, DYN, 0x265B, 1, "cbEffects2", "Enable Effects 2 Control (Palette)"),
    F(EFFECTS, PAL, 0x267D, 20, "lvEffects", "Effects 1 Palette"),
    F(EFFECTS, U8S, 0x37E5, 5, "lvEffects",  "Effects 1 palette type codes"),
    F(EFFECTS, PAL, 0x37EB, 20, "lvEffects", "Effects 2 Palette"),
    F(EFFECTS, U8S, 0x3C5F, 5, "lvEffects",  "Effects 2 palette type codes"),
    F(EFFECTS, DYN, 0x3C64, 1, "cbEffects3", "Enable Effects 3 Control"),
    F(EFFECTS, DYN, 0x3C75, 1, "cbEffects4", "Enable Effects 4 Control"),
    /* 16 s4.10 Specials (tbSpecials) */
    F(SPECIALS, STATIC, 0x12AF, 1,  "cbReset",      "Enable Reset Function"),
    F(SPECIALS, I32,    0x3D49, 1,  "eResetTime",   "Minimum Reset Time"),
    F(SPECIALS, STATIC, 0x2AF1, 32, "lbFunctions",  "Special Functions 1-32"),
    F(SPECIALS, U8S,    0x3E14, 32, "lvGMControl",  "Extended GM Control"),
    F(SPECIALS, U8S,    0x1088, 32, "lvDefaultDMX", "Default DMX Values"),
    /* 16 s4.11 Levels (TabSheet8) */
    F(LEVELS, LEVELS, 0x35D1, 32, "lvLevels", "Levels"),
    /* 16 s4.12 Offline Visualizer (TabVisualizer): raw-only fields, the struct does not own them */
    F(OFFLINE_VIZ, STR, 0x3C86, 64, "eMan",   "Manufacturer"),
    F(OFFLINE_VIZ, STR, 0x3CC7, 64, "eModel", "Fixture Model"),
    F(OFFLINE_VIZ, STR, 0x3D08, 64, "eMode",  "Fixture Mode"),
};
#undef F
const int lj_pe_nfields = (int)(sizeof lj_pe_fields / sizeof lj_pe_fields[0]);

int lj_pe_field_find(const char *control)
{
    int i;
    for (i = 0; control && i < lj_pe_nfields; i++) if (strcmp(lj_pe_fields[i].control, control) == 0) return i;
    return -1;
}

/* ---- the editor (profile-editor track): the working copy is the raw record ---- */
enum { LJ_PE_DYN = 17, LJ_PE_STATIC = 87, LJ_PE_PAL = 57, LJ_PE_LVL = 16, LJ_PE_LEVELS_OFF = 0x35D1, LJ_PE_MANUFACTURER_OFF = 0x3E56 };

int lj_pe_field_size(int field)
{
    const lj_pe_field *f;
    if (field < 0 || field >= lj_pe_nfields) return 0;
    f = &lj_pe_fields[field];
    switch (f->kind) {
    case LJ_PEF_I32: return 4;
    case LJ_PEF_STR: return f->n + 1;
    case LJ_PEF_CHARS: case LJ_PEF_U8S: return f->n;
    case LJ_PEF_DYN: return LJ_PE_DYN * f->n;
    case LJ_PEF_STATIC: return LJ_PE_STATIC * f->n;
    case LJ_PEF_PAL: return LJ_PE_PAL * f->n;
    case LJ_PEF_LEVELS: return LJ_PE_LVL * f->n;
    default: return 1;
    }
}

/* the ShortString inside a field: STR itself, a DYN / STATIC / PAL / LEVELS record's name [16 s3.1]; -1 = none */
static int lj_pe_sstr(int field, int index, int *width)
{
    const lj_pe_field *f;
    if (field < 0 || field >= lj_pe_nfields || index < 0) return -1;
    f = &lj_pe_fields[field];
    switch (f->kind) {
    case LJ_PEF_STR:    if (index) return -1; *width = f->n; return f->off;
    case LJ_PEF_DYN:    if (index >= f->n) return -1; *width = 8;  return f->off + LJ_PE_DYN * index + 2;
    case LJ_PEF_STATIC: if (index >= f->n) return -1; *width = 15; return f->off + LJ_PE_STATIC * index;
    case LJ_PEF_PAL:    if (index >= f->n) return -1; *width = 30; return f->off + LJ_PE_PAL * index + 11;
    case LJ_PEF_LEVELS: if (index >= f->n) return -1; *width = 5;  return f->off + LJ_PE_LVL * index;
    default: return -1;
    }
}

/* New Profile / Clear Profile: "By default, a new profile only has generic level control enabled (enabling 8 bit virtual
 * fader control of all 32 channels)" [02 s5.2]: a zeroed record keeps the 32 level records visible and enabled */
static void lj_pe_defaults(uint8_t *raw)
{
    memset(raw, 0, LJ_GS2_REC_SIZE);
    raw[0] = LJ_GS2_VERSION & 0xFF; raw[1] = LJ_GS2_VERSION >> 8; raw[4] = 0x2E;   /* 16 s4.1 */
    raw[0x11AA + 16] = 1;   /* the Blackout record's enable byte "is 1 in every file" [16 s3.1] */
    raw[5] = LJ_PROFILE_CH - 1;   /* 32 enabled levels */
}

/* byte 0x05 = the last enabled level offset [16 s4.1]; recomputed only after a Levels edit (LORD_FIXTURES-4) */
static void lj_pe_last_level(uint8_t *raw)
{
    int k, last = 0;
    for (k = 0; k < LJ_PROFILE_CH; k++) if (!raw[LJ_PE_LEVELS_OFF + LJ_PE_LVL * k + 7]) last = k;
    raw[5] = (uint8_t)last;
}

/* ME-126 / LORD_FIXTURES-1: a format-0/1 record opens as a 2.6b9 one, its three identity slots holding the ME-33 values
 * (name = legacy_name, "User Definable", empty mode), so a rename takes effect and an unedited validate passes; the
 * disk file changes only when the user applies. Byte 5 and every other byte stay as read. */
static void lj_pe_upgrade_legacy(uint8_t *raw)
{
    static const struct { uint16_t off; int w; } slot[3] = { { 0x3E37, 30 }, { LJ_PE_MANUFACTURER_OFF, 30 }, { 0x3E75, 30 } };
    char name[31];
    size_t n = raw[0x0009] > 30 ? 30 : raw[0x0009];
    const char *v[3];
    int i;
    if ((raw[0] | raw[1] << 8) == LJ_GS2_VERSION) return;
    memcpy(name, raw + 0x000A, n); name[n] = 0;
    v[0] = name; v[1] = LJ_GS2_LEGACY_MANUFACTURER; v[2] = "";
    for (i = 0; i < 3; i++) { size_t k = strlen(v[i]); if (k > (size_t)slot[i].w) k = (size_t)slot[i].w; raw[slot[i].off] = (uint8_t)k; memcpy(raw + slot[i].off + 1, v[i], k); }
    raw[0] = LJ_GS2_VERSION & 0xFF; raw[1] = LJ_GS2_VERSION >> 8;
}

int lj_pe_begin(lj_profile_edit *ed, const lj_profile *src)
{
    if (!ed) return -1;
    memset(ed, 0, sizeof *ed);
    if (!src) lj_pe_defaults(ed->raw);
    else if (src->gs2_raw) memcpy(ed->raw, src->gs2_raw, LJ_GS2_REC_SIZE);
    else if (lj_gs2_write(src, ed->raw) != 0) return -1;      /* a built-in: serialised from a zeroed record (lj_gs2.h) */
    if (src && src->gs2_raw) lj_pe_upgrade_legacy(ed->raw);
    if (src) lj_str_copy(ed->p.gs2_file, sizeof ed->p.gs2_file, src->gs2_file);
    lj_pe_refresh(ed);   /* -1 only while lj_gs2_parse is unavailable; the raw record is the working copy either way */
    ed->dirty = 0;
    return 0;
}

int lj_pe_refresh(lj_profile_edit *ed)
{
    char file[sizeof ed->p.gs2_file], err[128];
    if (!ed) return -1;
    if (ed->tab == LJ_PE_LEVELS) { lj_pe_last_level(ed->raw); ed->tab = LJ_PE_FIXTURE_INFO; }   /* byte 5 only after a Levels edit (LORD_FIXTURES-4) */
    lj_str_copy(file, sizeof file, ed->p.gs2_file);
    /* ponytail: no lj_layout_derive here (its tables are never freed, W1); the app derives on GS2_RELOAD after a save */
    if (lj_gs2_parse(ed->raw, &ed->p, err, sizeof err) != 0) { lj_str_copy(ed->p.gs2_file, sizeof ed->p.gs2_file, file); return -1; }
    ed->p.gs2_raw = ed->raw;                                  /* lj_gs2_write / lj_udf_export of ed->p = the working bytes */
    lj_str_copy(ed->p.gs2_file, sizeof ed->p.gs2_file, file);
    return 0;
}

int lj_pe_get_u8(const lj_profile_edit *ed, int field, int index)
{
    int sz = lj_pe_field_size(field);
    if (!ed || !sz || index < 0 || index >= sz || lj_pe_fields[field].kind == LJ_PEF_STR || lj_pe_fields[field].kind == LJ_PEF_CHARS) return -1;
    return ed->raw[lj_pe_fields[field].off + index];
}

/* index = the byte inside the field: 0 for U8, 0..3 for I32 (little-endian), the array index for U8S, the byte offset
 * inside the record(s) for DYN (+0 enabled, +1 offset, +11 min, +12 max), STATIC (rec * 87 + 16 enabled, + 17 + k
 * defined, + 49 + k value), PAL (entry * 57 + 0 used, +1 mask, +2..+6 values, +7..+9 R G B) and LEVELS (k * 16 + 6
 * invisible, + 7 disabled). Text goes through lj_pe_set_str. */
int lj_pe_set_u8(lj_profile_edit *ed, int field, int index, int v)
{
    if (v < 0 || v > 255 || lj_pe_get_u8(ed, field, index) < 0) return -1;
    ed->raw[lj_pe_fields[field].off + index] = (uint8_t)v;
    if (lj_pe_fields[field].kind == LJ_PEF_LEVELS) ed->tab = LJ_PE_LEVELS;   /* a Levels edit: the next refresh moves byte 5 (LORD_FIXTURES-4) */
    ed->dirty = 1;
    return 0;
}

int lj_pe_set_str(lj_profile_edit *ed, int field, int index, const char *s)
{
    int w, off;
    size_t n;
    if (!ed || !s) return -1;
    if (field >= 0 && field < lj_pe_nfields && lj_pe_fields[field].kind == LJ_PEF_CHARS) {   /* the RTF notes: NUL-terminated, refused when too long */
        n = strlen(s);
        if (index || n >= lj_pe_fields[field].n) return -1;
        memcpy(ed->raw + lj_pe_fields[field].off, s, n + 1);  /* bytes past the NUL stay as they were (stale, like LJ2) */
        ed->dirty = 1;
        return 0;
    }
    if ((off = lj_pe_sstr(field, index, &w)) < 0) return -1;
    n = strlen(s);
    if (n > (size_t)w) n = (size_t)w;                         /* clamps to the width; the stale tail past n is kept [16 s3] */
    ed->raw[off] = (uint8_t)n;
    memcpy(ed->raw + off + 1, s, n);
    ed->dirty = 1;
    return 0;
}

int lj_pe_get_str(const lj_profile_edit *ed, int field, int index, char *dst, size_t cap)
{
    int w, off;
    size_t n;
    if (!ed || !dst || !cap) return -1;
    if (field >= 0 && field < lj_pe_nfields && lj_pe_fields[field].kind == LJ_PEF_CHARS) {
        const uint8_t *b = ed->raw + lj_pe_fields[field].off;
        for (n = 0; n < lj_pe_fields[field].n && b[n]; n++) { }
        if (index || n == lj_pe_fields[field].n) { dst[0] = 0; return -1; }   /* no NUL inside the block: LJ_PE_ERR_NOTES */
        if (n >= cap) n = cap - 1;
        memcpy(dst, b, n); dst[n] = 0;
        return 0;
    }
    if ((off = lj_pe_sstr(field, index, &w)) < 0) { dst[0] = 0; return -1; }
    n = ed->raw[off] > w ? (size_t)w : ed->raw[off];          /* the oracle's sstr(): min(length byte, width) */
    if (n >= cap) n = cap - 1;
    memcpy(dst, ed->raw + off + 1, n); dst[n] = 0;
    return 0;
}

const char *lj_pe_validate(const lj_profile_edit *ed)
{
    const uint8_t *r;
    int i, k, ofs;
    if (!ed) return LJ_PE_ERR_OFFSET;
    r = ed->raw;
    for (k = 0; k < LJ_GS2_NOTES && r[0x0062 + k]; k++) { }
    if (k == LJ_GS2_NOTES) return LJ_PE_ERR_NOTES;
    if (r[LJ_PE_MANUFACTURER_OFF] == 0) return LJ_PE_ERR_MANUFACTURER;
    for (i = 0; i < lj_pe_nfields; i++)                       /* every enabled dynamic function addresses an offset 0-31 */
        if (lj_pe_fields[i].kind == LJ_PEF_DYN)
            for (k = 0; k < lj_pe_fields[i].n; k++)
                if (r[lj_pe_fields[i].off + LJ_PE_DYN * k] && r[lj_pe_fields[i].off + LJ_PE_DYN * k + 1] >= LJ_PROFILE_CH) return LJ_PE_ERR_OFFSET;
    if (r[0x1306] || r[0x1307]) {                             /* Pan & Tilt [16 s4.4]: 16 bit uses ofs and ofs + 1 */
        int span = r[0x1306] ? 2 : 1;
        if (r[0x1308] + span > LJ_PROFILE_CH || r[0x1309] + span > LJ_PROFILE_CH) return LJ_PE_ERR_OFFSET;
        if (r[0x1308] == r[0x1309]) return LJ_PE_ERR_IDENTICAL;   /* PROPOSAL: where LJ2 raises the literal is unverified */
        if (r[0x1306] && r[0x130A])
            for (ofs = 0x130B; ofs <= 0x130E; ofs++) if (r[ofs] >= LJ_PROFILE_CH) return LJ_PE_ERR_OFFSET;
    }
    return NULL;
}
