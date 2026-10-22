/* lj_default_output.c - see lj_default_output.h. */
#include "windows/lj_default_output.h"

#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "model/lj_defaultout.h"
#include "model/lj_show.h"

int lj_default_output_run(int use_scene)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *sh = ctx->show;
    lj_cmd c;
    char path[LJ_PATH_CAP];
    int rc = 0;
    if (!sh) return -1;
    lj_lock();
    if (!use_scene) lj_defaultout_clear(sh->default_out);
    else if (!sh->current_seq || ctx->current_scene >= sh->current_seq->n) rc = -1;
    else rc = lj_defaultout_use_scene(sh->default_out, &sh->current_seq->scenes[ctx->current_scene]);
    lj_unlock();
    if (rc != 0) { if (use_scene && sh->current_seq) lj_win_message("Default DMX Output", "Default DMX Output cannot contain Presets"); return -1; }
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_DEFAULT_OUT; c.default_out = sh->default_out;
    lj_ctx_post(&c);
    if (ctx->lib && lj_library_path(ctx->lib, "DefaultOut.dat", path, sizeof path) == 0 && lj_defaultout_save(path, sh->default_out) != 0) lj_log("default output: cannot write %s", path);
    return 0;
}
