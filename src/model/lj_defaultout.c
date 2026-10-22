/* lj_defaultout.c - Setup > Preferences > Default DMX Output (lj_defaultout.h, 01 s6.3).
 * The file layer is lj_defaultout_load/save in lj_show.c (ch lines through lj_kv_ch_parse,
 * which range-checks link, address and value: ME-77). */
#include <string.h>
#include "model/lj_defaultout.h"

int lj_defaultout_scene_blocked(const lj_scene *scene)
{
    int i;
    for (i = 0; i < LJ_FIXTURES; i++) if (scene->preset[i] || scene->macro[i].type) return 1;   /* "presets/pan-tilt macros not allowed -> error" */
    return 0;
}

int lj_defaultout_use_scene(uint8_t out[LJ_CHANS], const lj_scene *scene)
{
    int c;
    if (lj_defaultout_scene_blocked(scene)) return -1;
    for (c = 0; c < LJ_CHANS; c++) if (scene->state[c] != LJ_OFF) out[c] = scene->val[c];   /* merged over the existing defaults; 16-bit pairs copy bytewise (ME-22) */
    return 0;
}

void lj_defaultout_clear(uint8_t out[LJ_CHANS]) { memset(out, 0, LJ_CHANS); }
