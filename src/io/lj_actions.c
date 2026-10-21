/* lj_actions.c - the function table of lj_actions.h, expanded from LJ_FN_LIST. */
#include <stdio.h>
#include <string.h>
#include "io/lj_actions.h"

const char *const lj_fn_group_names[LJ_FNG_COUNT] = {
    "System", "Intensity", "HTP groups", "Fixture controls", "Other", "Fixture selection", "Sequence", "Cue",
    "Cue control", "Cue builder", "Generic macro", "Cue list", "Background cue", "Statics", "Fixture groups", "DMX in", "Programs" };

#define LJ_FN_ROW(id, g, c, n, f) { #id, (uint8_t)(g), c, (uint8_t)(n), (uint8_t)(f) },
const lj_fn_desc lj_fn_table[LJ_FN_COUNT] = {
    { "LJ_FN_NONE", LJ_FNG_SYSTEM, "", 0, 0 },
    LJ_FN_LIST(LJ_FN_ROW)
};
#undef LJ_FN_ROW

int lj_fn_caption(enum lj_fn fn, char *dst, size_t cap)
{
    const lj_fn_desc *d;
    int n;
    if (!dst || cap == 0) return -1;
    dst[0] = 0;
    if ((int)fn <= 0 || fn >= LJ_FN_COUNT) return -1;
    d = &lj_fn_table[fn];
    if (strstr(d->caption, "%")) {
        /* the caption is a trusted literal from this file with exactly one %d / %02d */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4774)
#endif
        n = snprintf(dst, cap, d->caption, (int)d->n);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    } else {
        n = snprintf(dst, cap, "%s", d->caption);
    }
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

enum lj_fn lj_fn_by_name(const char *name)
{
    int i;
    if (!name) return LJ_FN_NONE;
    for (i = 1; i < LJ_FN_COUNT; i++) if (strcmp(lj_fn_table[i].name, name) == 0) return (enum lj_fn)i;
    return LJ_FN_NONE;
}

int lj_fn_is_flash(enum lj_fn fn)
{
    return ((int)fn > 0 && fn < LJ_FN_COUNT) ? (lj_fn_table[fn].flags & LJ_FNF_FLASH) != 0 : 0;
}
