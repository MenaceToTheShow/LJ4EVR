/* lj_kv.h - the field helpers every record kind shares (design 5.2): "key=value"
 * fields inside one list line ("slot 1 seq=12 on=1 ..."), quoted strings (quoted
 * when they hold a space or a quote, an inner quote doubled), "ch l:a=v:S" channel
 * lines (non-Off only), the per-scene lines ("scene st= ft=", "ch", "fx n preset=
 * macro=a,.. mst=.."), number lists ("1,5,9") and the loader error line. */
#ifndef LJ_MODEL_KV_H
#define LJ_MODEL_KV_H

#include <stddef.h>
#include <stdint.h>
#include "core/lj_textrec.h"
#include "model/lj_model.h"

enum { LJ_KV_VAL_CAP = 8192 };   /* core/lj_textrec.c line cap: the one value buffer of a loader */

int  lj_kv_get(const char *fields, const char *key, char *out, size_t cap);              /* 1 found (out = the value, unquoted) / 0 absent / -1 truncated or unterminated quote */
long lj_kv_long(const char *fields, const char *key, long dflt);                         /* integer field; dflt when absent or not a number */
int  lj_kv_word(const char *fields, const char *key, const char *const *table, int dflt);   /* enum word (lj_word_index); dflt when absent or unknown (the key is skipped, design 5.2) */
int  lj_kv_quote(char *dst, size_t cap, const char *s);                                   /* a b -> "a b", " doubled; plain words copied; -1 truncated */
int  lj_kv_unquote(char *dst, size_t cap, const char *s);                                 /* the inverse for whole-line values (name="..."); -1 truncated */
int  lj_kv_list(const char *s, uint32_t *out, int max);                                   /* "1,5,9" -> numbers (the list ends at a space); count, -1 on a bad token or more than max */

int  lj_kv_ch_write(lj_rec *rec, const uint8_t val[LJ_CHANS], const uint8_t state[LJ_CHANS]);   /* "ch 1:1=255:F ..." lines, non-Off only; state NULL = "l:a=v" for every non-zero val (DefaultOut) */
int  lj_kv_ch_parse(const char *fields, uint8_t val[LJ_CHANS], uint8_t state[LJ_CHANS]);        /* one ch line into the arrays (state NULL = no ":S"); -1 on a bad token */
int  lj_kv_scene_write(lj_rec *rec, const lj_scene *s);                                          /* "scene st= ft=" + ch + fx lines */
int  lj_kv_scene_field(lj_scene *s, const char *key, const char *fields);                       /* scene / ch / fx lines into s; 1 handled, 0 not a scene key, -1 bad */

void lj_kv_err(char *err, size_t cap, const char *path, int item, const char *msg);     /* "<path> item N: msg" into err (NULL ok) and the log; item = the Nth key/value after the header */

#endif /* LJ_MODEL_KV_H */
