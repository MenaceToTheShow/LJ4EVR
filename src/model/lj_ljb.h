/* lj_ljb.h - the .LJB importer: an LJ2 user-library backup into a new LJ4EVR library (design 5.3, 10 W5; note 18 s1-s13;
 * docs/w5-contracts.md seam 8; track ljb).
 *
 * Container (18 s1, the same 36-byte-trailer shape as .Udf / .Udo, fixlib/lj_gs2.h): [entries, each zlib-deflated
 * (flags 0x300) or stored (0x1)][zlib index of n x 532-byte records][trailer: crc32 of every byte before it, n, 1,
 * index size, index offset, 1, 12 reserved]. Every check before use (W5-LJB-1, hostile input): the CRC first; ioff +
 * isz <= len - 36; n <= LJ_LJB_ENTRIES_MAX; the inflated index exactly n * 532; per entry ShortString lengths <= 255,
 * a non-empty name, off + csz <= ioff; 0x300 inflates through miniz tinfl with the output capped at usz, then length
 * and CRC-32 equal; 0x1 requires csz == usz; any other flag refused; the summed usz <= LJ_LJB_TOTAL_MAX before any
 * allocation; duplicate paths (case-insensitive) refused.
 *
 * Paths (W5-LJB-2): folder components "..", ".", empty, absolute, a drive letter, ':' , NUL and the reserved device
 * names are refused; the joined path stays within lj_library_path's cap; nothing is executed; an existing library is
 * never overwritten (lj_ljb_import creates a new folder or fails).
 *
 * Conversion (W5-LJB-3): the patch (Config.dat), groups (GroupInfo.dat), sequences (.seq names, times, scenes),
 * default output (DefaultOut.dat), statics scenes and names (Statics.dat), generic macro parameters (.CMA), cue name
 * and slot 1 (.cue), cue pages, each record size checked before a field is read; strings stay CP1252, times integer ms;
 * TDateTime decoded with integer bit operations or skipped (no double in model/); inferred meanings carry a
 * `PROPOSAL rig 18 sN` tag. Everything else is listed in the report (lj_ljb_report) and in the new library's
 * LJBImport.txt (PROPOSAL name); Info.$$$ gives the library name (sanitised), Notes.$$$ goes through lj_str_rtf_strip
 * into the library notes. Errors use (char *err, size_t cap) with the LJ_LJB_ERR_* texts. */
#ifndef LJ_MODEL_LJB_H
#define LJ_MODEL_LJB_H

#include <stddef.h>
#include <stdint.h>

enum { LJ_LJB_TRAILER = 36, LJ_LJB_INDEX_REC = 532, LJ_LJB_ENTRIES_MAX = 8192 /* PROPOSAL rig W5-LJB-1 (the sample has 127) */,
       LJ_LJB_FLAG_DEFLATE = 0x300, LJ_LJB_FLAG_STORED = 0x1, LJ_LJB_NAME = 256 };
#define LJ_LJB_TOTAL_MAX  (512u * 1024u * 1024u)   /* summed uncompressed size, PROPOSAL rig W5-LJB-1 */
#define LJ_LJB_FILE_MAX   (512u * 1024u * 1024u)   /* the .LJB itself, checked before the read */
#define LJ_LJB_ERR_CRC    "LJB: trailer CRC mismatch"
#define LJ_LJB_ERR_INDEX  "LJB: bad index"
#define LJ_LJB_ERR_ENTRY  "LJB: bad entry %d"
#define LJ_LJB_ERR_PATH   "LJB: refused path in entry %d"
#define LJ_LJB_ERR_SIZE   "LJB: too large"
#define LJ_LJB_ERR_EXISTS "LJB: the library already exists"

typedef struct { char name[LJ_LJB_NAME], folder[LJ_LJB_NAME]; uint32_t usz, csz, off, crc, flags; } lj_ljb_entry;   /* CP1252; folder "" = library root, trailing backslash as stored */
typedef struct { const uint8_t *data; size_t len; uint32_t n; lj_ljb_entry *e; } lj_ljb;   /* data is the caller's; e is owned (lj_ljb_free) */

int  lj_ljb_open(lj_ljb *out, const uint8_t *data, size_t len, char *err, size_t cap);   /* the container checks above, the index decoded into e[]; 0 / -1 */
int  lj_ljb_extract(const lj_ljb *b, uint32_t i, uint8_t **out, size_t *n, char *err, size_t cap);   /* entry i inflated / copied and CRC-checked; *out malloc'd (free()) */
int  lj_ljb_path_ok(const char *folder, const char *name);                 /* 1 when the joined relative path passes the W5-LJB-2 rules */
void lj_ljb_free(lj_ljb *b);

typedef struct { int converted, skipped; char lines[64][96]; int nlines; } lj_ljb_report;   /* one line per file not converted ("Cues\00000003.cue: slots 2-12 not converted") */
int  lj_ljb_import(const char *ljb_path, const char *new_library_dir, lj_ljb_report *rep, char *err, size_t cap);   /* reads (size cap), opens, converts into a NEW folder (fails if it exists); 0 / -1; nothing is left behind on -1 */

#endif /* LJ_MODEL_LJB_H */
