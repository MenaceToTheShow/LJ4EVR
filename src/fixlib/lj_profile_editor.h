/* lj_profile_editor.h - the Generic Fixture Profile editor model (TScanner2DefsForm "Generic Fixture Profile",
 * 575x391, PC1 with 11 tabs; note 16 s4.2-4.12, design 10 W3 "Author / Icon Caption / Icon Bitmap / Transparent").
 *
 * The working copy IS the raw record: every field edit writes ed->raw at its .GS2 offset (the field table below,
 * one row per dialog control with a located field), then lj_pe_refresh re-parses raw into ed->p (lj_gs2_parse +
 * lj_layout_derive), so the struct and the bytes never disagree and the raw-record rule holds by construction
 * (fields the struct does not own, such as the Offline Visualizer strings, are edited in raw directly). A built-in
 * opened in the editor ("Define" from Fixture Configuration) starts from lj_gs2_write of a zeroed record. Commit =
 * lj_pe_validate + the bytes; the window saves them (lj_gs2_save) and asks the app to reload (ME-31 seam).
 * Customize Fixture and Channel Configuration are per-FIXTURE (lj_fixture.custom / ch_caption / ch_gm_off) and live in
 * model/lj_show.h and the windows; this header is the per-PROFILE editor only. Bodies: lj_profile_editor.c
 * (profile-editor track; the field table is complete here). */
#ifndef LJ_FIXLIB_PROFILE_EDITOR_H
#define LJ_FIXLIB_PROFILE_EDITOR_H

#include <stddef.h>
#include <stdint.h>
#include "fixlib/lj_profile.h"

enum lj_pe_tab { LJ_PE_FIXTURE_INFO = 0, LJ_PE_INTENSITY_LAMP, LJ_PE_PAN_TILT, LJ_PE_FIXED_COLOR, LJ_PE_CMY_RGB, LJ_PE_GOBO,
                 LJ_PE_BEAM, LJ_PE_EFFECTS, LJ_PE_SPECIALS, LJ_PE_LEVELS, LJ_PE_OFFLINE_VIZ, LJ_PE_TAB_COUNT };   /* PC1 page order */
extern const char *const lj_pe_tab_caption[LJ_PE_TAB_COUNT];   /* DFM captions as shown: "Fixture Info", "Intensity & Lamp" ("&&" in the DFM), ... "Offline Visualizer" */

enum lj_pe_kind { LJ_PEF_U8 = 0 /* number or check box */, LJ_PEF_I32 /* ms fields */, LJ_PEF_STR /* ShortString[n] */, LJ_PEF_CHARS /* NUL-terminated block (RTF notes) */,
                  LJ_PEF_U8S /* n bytes (patch targets, type codes, ext GM, default DMX) */, LJ_PEF_DYN /* 17-byte dynamic record */,
                  LJ_PEF_STATIC /* 87-byte static record(s), n of them */, LJ_PEF_PAL /* 20 x 57-byte palette */, LJ_PEF_LEVELS /* 32 x 16-byte level records */ };
typedef struct { uint8_t tab, kind; uint16_t off /* .GS2 offset [16 s4] */; uint16_t n /* width / count */; const char *control /* DFM control name */; const char *label /* the dialog label [16 s4] */; } lj_pe_field;
extern const lj_pe_field lj_pe_fields[];
extern const int lj_pe_nfields;

typedef struct { lj_profile p; uint8_t raw[LJ_GS2_REC_SIZE]; uint8_t dirty; uint8_t tab /* enum lj_pe_tab: LJ_PE_LEVELS after a Levels edit until the next lj_pe_refresh */; } lj_profile_edit;   /* ~38 KB: heap or static, never a stack local */
int         lj_pe_begin(lj_profile_edit *ed, const lj_profile *src);   /* src NULL = New Profile (zeroed record, format_version 2609, byte 0x04 = 0x2E); a format-0/1 src opens as 2609 with the ME-33 identity in its slots (ME-126); 0 / -1 */
int         lj_pe_refresh(lj_profile_edit *ed);                        /* raw -> p after an edit; after a Levels edit (tab = LJ_PE_LEVELS, set by lj_pe_set_u8) also byte 0x05 = last enabled level offset [16 s4.1], else byte 5 stays as read (LORD_FIXTURES-4); 0 / -1 */
int         lj_pe_set_u8(lj_profile_edit *ed, int field, int index, int v);         /* one byte of field (index into U8S / DYN +off / STATIC defined-or-value); dirty = 1; 0 / -1 */
int         lj_pe_set_str(lj_profile_edit *ed, int field, int index, const char *s);   /* ShortString / name of a DYN or STATIC entry or palette bitmap; clamps to the width, keeps the stale tail bytes; 0 / -1 */
const char *lj_pe_validate(const lj_profile_edit *ed);                  /* the dialog's own refusals: offsets < 32, footprint <= 32, ... NULL = OK */
int         lj_pe_field_find(const char *control);                       /* row index by DFM control name, -1 */
/* profile-editor track additions: the window paints from the raw record, so it never depends on the parse, and
 * the dialog's refusals / prompts are the exe literals verbatim [12 Q9, 13 exe strings 0x30FA10-0x319674]. */
int         lj_pe_field_size(int field);                                  /* bytes the field occupies in the record; 0 for a bad row */
int         lj_pe_get_u8(const lj_profile_edit *ed, int field, int index);  /* the byte set_u8 writes; -1 out of range */
int         lj_pe_get_str(const lj_profile_edit *ed, int field, int index, char *dst, size_t cap);   /* the text set_str writes (ShortString length clamped as the oracle's sstr); 0 / -1 */
#define LJ_PE_ASK_UNAPPLIED     "Any unapplied profile changes will be lost ?"     /* exe 0x319674 (Cancel, Load, Clear Profile) */
#define LJ_PE_ERR_NOTES         "Notes section too large - cannot apply profile"   /* exe 0x30FA10 */
#define LJ_PE_ERR_IDENTICAL     "Values cannot be identical"                       /* exe 0x31490C */
#define LJ_PE_ERR_MANUFACTURER  "Please specify manufacturer"                      /* exe 0x316A5C */
#define LJ_PE_ERR_FILENAME      "Filename cannot exceed 29 characters :"           /* exe 0x31877C */
#define LJ_PE_ERR_OFFSET        "DMX offset must be 0-31"                          /* ours: LJ2 has no literal for it (PROPOSAL) */

#endif /* LJ_FIXLIB_PROFILE_EDITOR_H */
