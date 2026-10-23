/* lj_llf.h - the LightCorder recording, our own .LLF format (PROPOSAL: no Martin LightCorder file is known; the name,
 * the folder LightCorderFiles and the dialog items are LJ2's [13 s1.2], the bytes are ours; docs/file-formats.md).
 * A recording is an in-memory "card" of 4..64 MB allocated at start (LJ2 pre-allocates and says
 * "Error pre-allocating ..." when it cannot), filled frame by frame on the UI thread and written whole on Save.
 *
 * File, little-endian:
 *   0  "LJ4LLF"   6 bytes        16 u32 card_bytes        28 u32 user notes bytes
 *   6  u16 version = 1           20 u32 frames            32 u32 system notes bytes
 *   8  u8 link 1..4, u8 flags    24 u32 data bytes        36 u32 0
 *  10  u16 channels 64/128/256/512, 12 u16 hz 10/20/40, 14 u16 0
 *  40  user notes, system notes (CP1252, no NUL), then the frames:
 *      0x00 + channels bytes (a full frame) | 0x01 (same as the previous frame; only when flags bit 0, compression)
 * The reader trusts nothing: every length is checked against the file size before use (hard rule 6). */
#ifndef LJ_MODEL_LLF_H
#define LJ_MODEL_LLF_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_types.h"

#define LJ_LLF_MAGIC "LJ4LLF"
#define LJ_LLF_DIR   "LightCorderFiles"                     /* exe 0x1FE558, under the user library */
#define LJ_LLF_EXT   ".LLF"
enum { LJ_LLF_VERSION = 1, LJ_LLF_HEADER = 40, LJ_LLF_NOTES_MAX = 65536,
       LJ_LLF_DECODE_MAX = 256 * 1024 * 1024 };              /* ponytail: a whole-file decode cap (frames x channels); a streaming reader when a player needs more */

typedef struct {
    uint8_t  link, compress;                                /* link 1..4; compress = "Target File Compression" Enable */
    uint16_t channels, hz;
    uint32_t card_bytes, frames, used;                      /* used = encoded bytes in data */
    uint8_t *data;                                          /* card_bytes, malloc'd at begin */
    uint8_t  prev[LJ_LINK_CH];
} lj_llf_rec;

int      lj_llf_valid_format(int channels, int hz);          /* 1 when channels is 64/128/256/512 and hz 10/20/40 */
uint32_t lj_llf_max_seconds(uint32_t card_bytes, int channels, int hz);   /* uncompressed worst case: card / ((channels + 1) * hz); 0 on bad args */
int      lj_llf_rec_begin(lj_llf_rec *r, int link, int channels, int hz, int card_mb, int compress);   /* allocates the card; -1 bad args or no memory (nothing held) */
int      lj_llf_rec_push(lj_llf_rec *r, const uint8_t *slots);   /* slots[channels]: 0 stored, 1 card full (nothing stored), -1 not recording */
void     lj_llf_rec_free(lj_llf_rec *r);                    /* zeroes r; NULL / idle is a no-op */
int      lj_llf_write(const char *path, const lj_llf_rec *r, const char *user_notes, const char *sys_notes);   /* 0 / -1 + log (notes > LJ_LLF_NOTES_MAX - 1 refused) */

typedef struct {
    uint8_t  link, compress;
    uint16_t channels, hz;
    uint32_t card_bytes, frames;
    char    *user_notes, *sys_notes;                        /* NUL-terminated copies */
    uint8_t *frame;                                         /* frames x channels, decoded */
} lj_llf_file;

int  lj_llf_parse(const uint8_t *buf, size_t len, lj_llf_file *out, char *err, size_t cap);   /* 0 / -1 with the reason in err; out zeroed on failure */
int  lj_llf_read(const char *path, lj_llf_file *out, char *err, size_t cap);                 /* whole file (<= header + 2 notes + 64 MB card) then parse */
void lj_llf_file_free(lj_llf_file *f);

#endif /* LJ_MODEL_LLF_H */
