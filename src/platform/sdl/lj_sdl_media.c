/* lj_sdl_media.c - SDL3 WAV playback for the MediaPlayer (platform/sdl/lj_sdl_media.h). W5 CONTRACT STUB: the media
 * track replaces every body (docs/w5-contracts.md seam 7). Until then every load is refused. */
#include <SDL3/SDL.h>
#include "platform/sdl/lj_sdl_media.h"
#include "core/lj_str.h"

static int lj_sm_load(void *ud, const char *path, uint32_t *rate, uint64_t *frames, char *err, size_t cap)
{ (void)ud; (void)path; if (rate) *rate = 0; if (frames) *frames = 0; if (err && cap) lj_str_copy(err, cap, "pending (media track)"); return -1; }
static int lj_sm_play(void *ud, int on) { (void)ud; (void)on; return -1; }
static int lj_sm_seek(void *ud, uint64_t frame) { (void)ud; (void)frame; return -1; }
static void lj_sm_close(void *ud) { (void)ud; }
static uint64_t lj_sm_consumed(void *ud) { (void)ud; return 0; }

const lj_media_host *lj_sdl_media_host(void)
{
    static const lj_media_host h = { NULL, lj_sm_load, lj_sm_play, lj_sm_seek, lj_sm_close, lj_sm_consumed };
    return &h;
}
