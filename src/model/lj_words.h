/* lj_words.h - the text-record words behind the model enums (design 5.2:
 * trig=auto dir=fwd end=stop mode=auto|audio|noloop act=toggle rep=add ...).
 * One table per enum, index = enum value, NULL-terminated. Readers and writers
 * of every record kind share these so a word is spelled once. */
#ifndef LJ_MODEL_WORDS_H
#define LJ_MODEL_WORDS_H

extern const char *const lj_trig_words[];        /* auto manual audio */
extern const char *const lj_dir_words[];         /* fwd rev bounce rnd */
extern const char *const lj_end_words[];         /* stop out goto link */
extern const char *const lj_bg_mode_words[];     /* auto audio noloop */
extern const char *const lj_act_words[];         /* toggle flash */
extern const char *const lj_replace_words[];     /* add slot page group all solo_slot solo_page solo_group solo_all */
extern const char *const lj_static_kind_words[]; /* empty recorded link */
extern const char *const lj_fixture_kind_words[];/* builtin gs2 */
extern const char *const lj_patch_src_words[];   /* none out in */
extern const char *const lj_in_mode_words[];     /* abs lotp htp gt0 lt255 */
extern const char *const lj_fstate_words[];      /* O S F (the ch line state letters) */

const char *lj_word(const char *const *table, int v);        /* table[v], or table[0] when v is out of range */
int         lj_word_index(const char *const *table, const char *word);   /* case-insensitive; -1 when unknown (callers skip the key, design 5.2) */

#endif /* LJ_MODEL_WORDS_H */
