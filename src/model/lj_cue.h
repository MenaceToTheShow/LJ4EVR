/* lj_cue.h - Cues\NNNNNNNN.cue (design 5.2: "LJ4 cue 1", num name type
 * cuetime, "macro=amp,speed savemacro=", transparent, "bgcue= genmacro=", then
 * "slot k seq= on= trig= dir= spt= atpt= tol= first= last= loops= end= goto=
 * link= time= chase=" per used slot) and Cues\CuePageInfo.Dat ("page n name=
 * cues=1,5,5,9"). */
#ifndef LJ_MODEL_CUE_H
#define LJ_MODEL_CUE_H

#include <stddef.h>
#include "model/lj_model.h"

int         lj_cue_load(const char *path, lj_cue *out, char *err, size_t cap);   /* 0 / -1 with err = first problem */
int         lj_cue_save(const char *path, const lj_cue *in);                      /* 0 / -1 + log */
const char *lj_cue_validate(const lj_cue *in);                                   /* NULL = valid: slot enums in range, time_pct 0..999, first <= last, loops rule (no loop count in random) */
lj_cue     *lj_cue_new_rec(void);                                                 /* heap record, every slot empty (seq 0), cue_time_pct 100, macro 100/100 */
void        lj_cue_free(lj_cue *cue);                                             /* NULL is a no-op */
int         lj_cue_add_sequence(lj_cue *cue, uint32_t seq, int slot /* 0 = first free, else 1..12 */);   /* "Add Sequence to Current Cue": returns the slot used (1..12) or -1 when full */
int         lj_cue_find_free(const lj_cue *cue);                                  /* 1..12 or -1 */

typedef struct { char name[31]; uint32_t cue[LJ_CUE_PAGES]; int n; } lj_cue_page;
int lj_cue_pages_load(const char *path, lj_cue_page *out, int max);              /* CuePageInfo.Dat: returns the count, -1 on error (W2 consumer) */
int lj_cue_pages_save(const char *path, const lj_cue_page *in, int n);

#endif /* LJ_MODEL_CUE_H */
