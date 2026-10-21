/* lj_profile.h - the fixture profile (design 6.2 verbatim plus the W1 additions
 * marked below), the control-window layout tables (design 6.4), the exe row
 * table and the fixlib API (design 2.4). fixlib is data plus the .GS2 parser:
 * no SDL, no I/O at runtime, below model/ (model embeds lj_palette).
 *
 * Bodies: lj_profile.c (lj_fixlib_*, lj_profile_window), lj_gs2.c (W3; W1
 * stubs return -1), lj_layout.c (lj_layout_derive), lj_exe_profiles.c (the 201
 * rows of reference/installer/exe_profile_table.txt, verbatim), hand/ (tier 1),
 * lj_profiles_gen.c (tools/qxf2c.py). */
#ifndef LJ_FIXLIB_PROFILE_H
#define LJ_FIXLIB_PROFILE_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_types.h"

enum { LJ_PROFILE_CH = 32, LJ_GS2_REC_SIZE = 20851, LJ_PAL_ENTRIES = 20, LJ_PALETTES = 6 };

/* ---- channel classes (design 6.2 list + ME-25: SWITCH SMOKE HAZE HAZE_FAN) ---- */
enum lj_ch_class {
    LJ_CH_NONE = 0, LJ_CH_INTENSITY, LJ_CH_SHUTTER, LJ_CH_STROBE, LJ_CH_PAN, LJ_CH_TILT, LJ_CH_COLOR, LJ_CH_GOBO,
    LJ_CH_BEAM, LJ_CH_EFFECT, LJ_CH_LEVEL, LJ_CH_SPEED, LJ_CH_LAMP, LJ_CH_SPECIAL,
    LJ_CH_SWITCH,     /* ME-25: on/off pack channel; S3 leaves it unscaled unless gm == 0 || sub == 0 (LJ_SWITCH_UNSCALED) */
    LJ_CH_SMOKE, LJ_CH_HAZE, LJ_CH_HAZE_FAN,   /* ME-25: layer 7 (smoke/haze) channels [11 C12] */
    LJ_CH_COUNT
};

/* ---- the .GS2 sub-records (16 s3.1) ---- */
typedef struct { uint8_t enabled, offset; char name[9]; uint8_t min, max; } lj_dyn;                 /* 17-byte record semantics */
typedef struct { char name[16]; uint8_t enabled; uint8_t defined[LJ_PROFILE_CH], value[LJ_PROFILE_CH]; } lj_gs2_sfunc;   /* 87-byte static-function record */
typedef struct { uint8_t used, mask, v[5]; uint32_t rgb; char bitmap[31]; } lj_pal_entry;
typedef struct { lj_pal_entry e[LJ_PAL_ENTRIES]; uint8_t type[5]; } lj_palette;                      /* 16 s5 codes 0-18 */

/* ---- control-window layout tables (design 6.4) ----
 * The design names the row enum lj_ctl_row_kind; W0 took that name for the DFM
 * class census in ui/forms/lj_form.h (LJ_K_*), and controls/ includes both
 * headers, so the layout-row kind is lj_ctl_elem with LJ_W_* constants. */
enum lj_ctl_kind { LJ_CTL_INTENSITY = 0, LJ_CTL_MOVEMENT, LJ_CTL_COLORS, LJ_CTL_GOBOS, LJ_CTL_BEAM, LJ_CTL_EFFECTS,
                   LJ_CTL_LEVELS, LJ_CTL_LAMP, LJ_CTL_EXTENDED, LJ_CTL_GENERIC, LJ_CTL_RGBPACK, LJ_CTL_COUNT };
enum lj_ctl_elem { LJ_W_FADEBTN = 0, LJ_W_SHUTTER_TOGGLE, LJ_W_DRAGBAR, LJ_W_SCROLL, LJ_W_ROTATION, LJ_W_INDEXBAR, LJ_W_MODEPAIR, LJ_W_SHAKE,
                   LJ_W_WHEELSCROLL, LJ_W_PALETTE, LJ_W_EFFECT_BTN, LJ_W_SPECIAL_GROUP, LJ_W_RADIOGROUP_FADER, LJ_W_LEVEL_FADER, LJ_W_LAMP_BTNS, LJ_W_NOTE,
                   LJ_W_COUNT };
typedef struct { const char *caption; uint8_t ch, value, lo, hi; } lj_ctl_button;   /* value = what the click emits; lo..hi = the Martin range (F47: value is the one number the rig edits) */
typedef struct {
    enum lj_ctl_elem kind; uint8_t ch; uint8_t rows /* fade button span */; const char *caption, *caption2;
    uint8_t lo, hi, reverse, dflt_state; const lj_ctl_button *buttons; int nbuttons; const char *const *items;
    uint8_t states_allowed;      /* ME-27: bitmask of (1 << lj_fstate) the O/S/F button may reach; 0 = all three (LJ_STATES_ALL); Lamp and Off/Snap-only specials = LJ_STATES_OFF_SNAP */
} lj_ctl_row;
#define LJ_STATES_ALL      0x07u   /* (1<<LJ_OFF) | (1<<LJ_SNAP) | (1<<LJ_FADE) */
#define LJ_STATES_OFF_SNAP 0x03u
typedef struct { enum lj_ctl_kind kind; const char *caption; const char *const *menu; const lj_ctl_row *rows; int nrows;
                 const lj_ctl_row *specials; int nspecials; } lj_ctl_window;

/* ---- the profile (design 6.2 verbatim; W1 additions marked) ---- */
typedef struct {
    uint16_t id; char tag[16]; char manufacturer[31], name[61], mode[31]; char fixture_id[16];   /* id = exe row or extra (6.1); 15/00 s1, 16 s4.2 */
    uint16_t format_version;                                       /* .GS2 0x0000: 2609, 0 or 1 [16 s4.1] */
    char     author[31], icon_caption[11], icon_bitmap[37]; uint8_t transparent;   /* .GS2 0x0043, 0x0028, 0x1063, 0x10A8; the W3 profile editor's Fixture Info tab [16 s4.2] */
    uint8_t  notes_rtf[4097];                                      /* GS2 kind: the user's own RTF, raw bytes for export, stripped for display (5.3) */
    const uint8_t *gs2_raw;                                        /* GS2 kind: the 20,851 stream-0 bytes; NULL for built-ins. lj_gs2_write starts from it */
    uint8_t  footprint; uint8_t channel_class[LJ_PROFILE_CH];     /* enum lj_ch_class per offset */
    lj_dyn   shutter, strobe, intensity, misc_move, color[4], cmy[3], gobo[4], iris, focus, zoom, frost, beam[2], effects[4];
    uint8_t  strobe_off;
    lj_gs2_sfunc blackout, lamp_on, lamp_off, reset, special[32];
    lj_palette palette[LJ_PALETTES];                               /* color1 color2 gobo1 gobo2 fx1 fx2 */
    uint8_t  use_gm, bo_at_gm0, invert_int, invert_int_range, limit_gm, ext_gm[LJ_PROFILE_CH], default_dmx[LJ_PROFILE_CH];
    uint8_t  pt16, pt8, pan_ofs, tilt_ofs, pt_patch_on, pt_patch[4], presets, macros, follow_spot;   /* 16 s4.4: pt16 pair = (ofs, ofs+1) MSB,LSB unless pt_patch_on, then pt_patch = pan MSB, pan LSB, tilt MSB, tilt LSB offsets */
    uint8_t  cmy_on, rgb_on, cmy_picker, invert_cmyrgb, hsv[3];
    uint8_t  level_caption[LJ_PROFILE_CH][6], level_invisible[LJ_PROFILE_CH], level_disabled[LJ_PROFILE_CH];
    uint32_t lamp_on_ms, reset_ms;
    const lj_ctl_window *windows;  int nwindows;                    /* section 6.4 */
    const char *notes;                                             /* built-ins: our rewritten facts, never Martin's RTF [D2] */
    /* W1 additions (not in design 6.2): */
    uint8_t  pair_lsb[LJ_PROFILE_CH];                              /* non-pan/tilt 16-bit pairs (LJ_PAIR16_NON_PT_AS_ONE, rig M11): 0 = none, else 1 + offset of this MSB's LSB; hand files / qxf2c fill it */
} lj_profile;

/* ---- the exe row table (design 6.1): 201 rows, ids 0-200, LJ2's own Config.dat type code ---- */
enum lj_profile_binding { LJ_BIND_NONE = 0 /* greyed, "Not available in LJ4EVR" */, LJ_BIND_HAND /* tier 1 */, LJ_BIND_QXF /* tier 2 */ };
typedef struct { uint16_t id; const char *tag, *manufacturer, *name, *mode, *notes_file; uint8_t binding; } lj_exe_row;

/* ---- fixlib API (design 2.4) ---- */
const lj_profile *lj_fixlib_by_id(uint16_t profile_id);                       /* exe row id 0-200, extras >= 1000; NULL = greyed row */
int               lj_fixlib_count(void);                                      /* 201 (+ extras when present; extras sort after id 200) */
const lj_exe_row *lj_fixlib_row(int i);                                       /* the exe triples verbatim: the Fixture Configuration tree; NULL out of range */
const lj_ctl_window *lj_profile_window(const lj_profile *p, enum lj_ctl_kind kind);   /* NULL = toolbar button disabled [F9] */
int               lj_layout_derive(lj_profile *p);                            /* .GS2 / QLC+-derived profiles: builds windows/nwindows by the Generic Fixture 2 rules (6.3) and sets states_allowed (ME-27); 0 / -1 */
int  lj_gs2_parse(const uint8_t rec[LJ_GS2_REC_SIZE], lj_profile *out, char *err, size_t cap);   /* W3 (W1 stub: -1). The 17 unlisted bytes at offset 0x266C travel in gs2_raw only (W0-gs2-266C): nothing interprets them */
int  lj_gs2_write(const lj_profile *in, uint8_t rec[LJ_GS2_REC_SIZE]);                           /* copy in->gs2_raw, then overwrite the struct-owned fields: serialize(parse(x)) of 16-gs2_ref.py. W1 stub: -1 */
int  lj_udf_import(const char *path, lj_profile *out, char *err, size_t cap);                     /* footer CRC verified, miniz inflate, stream 0 kept as gs2_raw [16 s2]. W1 stub: -1 */

#endif /* LJ_FIXLIB_PROFILE_H */
