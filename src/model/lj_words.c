/* lj_words.c - the record words of lj_words.h. */
#include <string.h>
#include "model/lj_words.h"

const char *const lj_trig_words[]         = { "auto", "manual", "audio", NULL };
const char *const lj_dir_words[]          = { "fwd", "rev", "bounce", "rnd", NULL };
const char *const lj_end_words[]          = { "stop", "out", "goto", "link", NULL };
const char *const lj_bg_mode_words[]      = { "auto", "audio", "noloop", NULL };
const char *const lj_act_words[]          = { "toggle", "flash", NULL };
const char *const lj_replace_words[]      = { "add", "slot", "page", "group", "all", "solo_slot", "solo_page", "solo_group", "solo_all", NULL };
const char *const lj_static_kind_words[]  = { "empty", "recorded", "link", NULL };
const char *const lj_fixture_kind_words[] = { "builtin", "gs2", NULL };
const char *const lj_patch_src_words[]    = { "none", "out", "in", NULL };
const char *const lj_in_mode_words[]      = { "abs", "lotp", "htp", "gt0", "lt255", NULL };
const char *const lj_fstate_words[]       = { "O", "S", "F", NULL };

const char *lj_word(const char *const *table, int v)
{
    int n = 0;
    while (table[n]) n++;
    return (v >= 0 && v < n) ? table[v] : table[0];
}

static int lj_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int lj_word_index(const char *const *table, const char *word)
{
    int i;
    if (!word) return -1;
    for (i = 0; table[i]; i++) {
        const char *a = table[i], *b = word;
        while (*a && *b && lj_lower((unsigned char)*a) == lj_lower((unsigned char)*b)) { a++; b++; }
        if (!*a && !*b) return i;
    }
    return -1;
}
