/* lj_gs2.h - the .GS2 / .Udf / .Udo importer and exporter (design 2.4, 5.3, 6.2; note 16; the oracle is
 * tools/ref/16-gs2_ref.py). lj_gs2_parse / lj_gs2_write / lj_udf_import keep their W1 prototypes in
 * fixlib/lj_profile.h; W3 adds the buffer forms, the file wrappers, the exporter and the free.
 *
 * Raw-record rule (design 5.3, ME-9): a GS2-kind profile keeps the 20,851 stream-0 bytes as gs2_raw (heap, owned by
 * the profile, released by lj_gs2_free). lj_gs2_write copies gs2_raw and overwrites only the fields the struct owns,
 * which is serialize(parse(x)) of the oracle: reserved blocks, stale ShortString tails, legacy_name, source_file, the
 * three OV strings, rgb_pad, the 22 stray palette bytes and the 17 unlisted bytes at 0x266C (W0-gs2-266C) travel
 * untouched. A built-in exported (Export All) serialises from a zeroed record (gs2_raw NULL).
 *
 * Untrusted input (hard rule 6): every length and offset is checked before use; ShortString length bytes above the
 * declared width are clamped exactly as the oracle's sstr() does; a truncated file is refused, never read past.
 * .Udf / .Udo container [16 s2]: back-to-back zlib streams then a 36-byte footer [crc32 of everything before it]
 * [file count][1][table size][data size][1][12 zero]; the CRC is verified FIRST, then miniz tinfl
 * (TINFL_FLAG_PARSE_ZLIB_HEADER) inflates stream 0 only, refusing any output that is not exactly LJ_GS2_REC_SIZE.
 * Messages are the oracle's assertion texts, verbatim, so the acceptance compare is a string compare.
 * format_version != 2609 (ME-33): name = legacy_name (0x0009), manufacturer = "User Definable", mode = "" (PROPOSAL 16 s10).
 * Bodies: lj_gs2.c (gs2 track). */
#ifndef LJ_FIXLIB_GS2_H
#define LJ_FIXLIB_GS2_H

#include <stddef.h>
#include <stdint.h>
#include "fixlib/lj_profile.h"

#define LJ_GS2_ERR_SIZE "not a GS2 record: %d bytes"    /* 16-gs2_ref.py parse(): assert len(buf) == SIZE */
#define LJ_GS2_ERR_CRC  "footer CRC mismatch"           /* 16-gs2_ref.py udf_streams(): the footer CRC assert */
enum { LJ_UDF_FOOTER = 36, LJ_GS2_VERSION = 2609 /* 0x0000 "2.6 build 9" [16 s4.1] */, LJ_GS2_NOTES = 4097 };
#define LJ_GS2_LEGACY_MANUFACTURER "User Definable"     /* ME-33 (PROPOSAL 16 s10) */

int  lj_gs2_read(const uint8_t *buf, size_t len, lj_profile *out, char *err, size_t cap);   /* size check (LJ_GS2_ERR_SIZE with len), heap copy as gs2_raw, lj_gs2_parse, lj_layout_derive; 0 / -1 (out left with gs2_raw NULL) */
int  lj_gs2_load(const char *path, lj_profile *out, char *err, size_t cap);                 /* a .GS2 file; gs2_file = its base name */
int  lj_gs2_save(const char *path, const lj_profile *in);                                    /* lj_gs2_write to a file (UserFixtureDef\<name>.GS2, Save Profile, Export All) */
int  lj_udf_parse(const uint8_t *data, size_t len, lj_profile *out, char *err, size_t cap);  /* container -> stream 0 -> lj_gs2_read; LJ_GS2_ERR_CRC on a bad footer */
int  lj_udf_export(const char *path, const lj_profile *in, char *err, size_t cap);           /* .Udf: stream 0 = lj_gs2_write, the 266-byte file table, the footer with its CRC; no bitmap stream (D2: the icon bitmap is the user's file, copied by the dialog, never packed by us; PROPOSAL) */
void lj_gs2_free(lj_profile *p);                                                             /* frees gs2_raw (NULL is a no-op); the profile record itself belongs to the caller */

#endif /* LJ_FIXLIB_GS2_H */
