/* lj_sdl_media.c - SDL3 WAV playback for the MediaPlayer (platform/sdl/lj_sdl_media.h; docs/media.md). One clip, one
 * playback stream on the default device, opened paused. The stream's get-callback feeds the decoded WAV from `pos` on the
 * audio thread under the stream lock (SDL holds it around the callback); seek and consumed take the same lock, so the
 * position is never read half-written. consumed = bytes fed - bytes still queued: the sample frames the device has taken,
 * which the SDL-free clock in io/lj_media.c turns into 1/100 s. The device buffer (one SDL period, ~10-20 ms) counts as
 * consumed before it is heard: the clip clock leads the speaker by that much (docs/media.md). UI thread only, except cb. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include "platform/sdl/lj_sdl_media.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

enum { LJ_SM_CHUNK = 64 * 1024 };   /* bytes handed per callback round */

static struct { SDL_AudioStream *st; Uint8 *buf; Uint32 len, pos; int fsz; } sm;

static void SDLCALL lj_sm_feed(void *ud, SDL_AudioStream *st, int additional, int total)
{
    (void)ud; (void)total;
    while (additional > 0 && sm.pos < sm.len) {
        Uint32 n = sm.len - sm.pos;
        if (n > (Uint32)additional) n = (Uint32)additional;
        if (n > LJ_SM_CHUNK) n = LJ_SM_CHUNK;
        n -= n % (Uint32)sm.fsz;                                           /* whole frames only */
        if (!n) n = (Uint32)sm.fsz <= sm.len - sm.pos ? (Uint32)sm.fsz : sm.len - sm.pos;
        if (!SDL_PutAudioStreamData(st, sm.buf + sm.pos, (int)n)) break;
        sm.pos += n; additional -= (int)n;
    }
}

static void lj_sm_close(void *ud)
{
    (void)ud;
    if (sm.st) SDL_DestroyAudioStream(sm.st);   /* also closes the device the stream opened */
    SDL_free(sm.buf);
    SDL_memset(&sm, 0, sizeof sm);
}

static int lj_sm_load(void *ud, const char *path, uint32_t *rate, uint64_t *frames, char *err, size_t cap)
{
    char u8[3 * 260];
    SDL_PathInfo info;
    SDL_AudioSpec spec;
    lj_sm_close(ud);
    if (rate) *rate = 0;
    if (frames) *frames = 0;
    if (lj_cp1252_to_utf8(u8, sizeof u8, path) != 0) { lj_str_copy(err, cap, "The file name is too long"); return -1; }
    if (!SDL_GetPathInfo(u8, &info) || info.type != SDL_PATHTYPE_FILE) { lj_str_copy(err, cap, "File not found"); return -1; }   /* ours */
    if (lj_media_check_file(path, info.size, err, cap) != 0) return -1;                                 /* the size cap BEFORE the decode */
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) { lj_str_copy(err, cap, "No audio output"); lj_log("media: SDL audio: %s", SDL_GetError()); return -1; }
    if (!SDL_LoadWAV(u8, &spec, &sm.buf, &sm.len)) { lj_log("media: %s: %s", path, SDL_GetError()); lj_str_copy(err, cap, "Not a playable WAV file"); return -1; }   /* ours */
    sm.fsz = SDL_AUDIO_FRAMESIZE(spec);
    if (sm.fsz <= 0 || spec.freq <= 0 || sm.len < (Uint32)sm.fsz) { lj_sm_close(ud); lj_str_copy(err, cap, "Not a playable WAV file"); return -1; }
    sm.len -= sm.len % (Uint32)sm.fsz;
    sm.st = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, lj_sm_feed, NULL);   /* opened paused */
    if (!sm.st) { lj_log("media: SDL_OpenAudioDeviceStream: %s", SDL_GetError()); lj_sm_close(ud); lj_str_copy(err, cap, "No audio output"); return -1; }
    if (rate) *rate = (uint32_t)spec.freq;
    if (frames) *frames = sm.len / (Uint32)sm.fsz;
    lj_log("media: %s loaded, %d Hz, %d ch, %u frames", path, spec.freq, spec.channels, (unsigned)(sm.len / (Uint32)sm.fsz));
    return 0;
}

static int lj_sm_play(void *ud, int on)
{
    (void)ud;
    if (!sm.st) return -1;
    return (on ? SDL_ResumeAudioStreamDevice(sm.st) : SDL_PauseAudioStreamDevice(sm.st)) ? 0 : -1;
}

static int lj_sm_seek(void *ud, uint64_t frame)
{
    uint64_t b = frame * (uint64_t)sm.fsz;
    (void)ud;
    if (!sm.st) return -1;
    if (b > sm.len) b = sm.len;
    SDL_LockAudioStream(sm.st);
    SDL_ClearAudioStream(sm.st);
    sm.pos = (Uint32)b;
    SDL_UnlockAudioStream(sm.st);
    return 0;
}

static uint64_t lj_sm_consumed(void *ud)
{
    int q;
    Uint32 pos;
    (void)ud;
    if (!sm.st) return 0;
    SDL_LockAudioStream(sm.st);
    pos = sm.pos;
    q = SDL_GetAudioStreamQueued(sm.st);
    SDL_UnlockAudioStream(sm.st);
    if (q < 0 || (Uint32)q > pos) q = 0;
    return (uint64_t)(pos - (Uint32)q) / (uint64_t)sm.fsz;
}

/* the OS open dialog: SDL may answer on another thread, so the path waits under lj_lock until a window's refresh takes it */
static struct { int state; char path[260]; } sm_pick;   /* 0 idle, 1 open, 2 path, 3 cancelled */
static void SDLCALL lj_sm_pick_cb(void *ud, const char *const *files, int filter)
{
    (void)ud; (void)filter;
    lj_lock();
    sm_pick.state = files && files[0] && lj_utf8_to_cp1252(sm_pick.path, sizeof sm_pick.path, files[0]) == 0 ? 2 : 3;
    lj_unlock();
}
static int lj_sm_pick(void *ud)
{
    static const SDL_DialogFileFilter wav[] = { { "WAV audio (*.wav)", "wav" } };   /* ours: the one format SDL3 decodes */
    (void)ud;
    lj_lock(); sm_pick.state = 1; lj_unlock();
    SDL_ShowOpenFileDialog(lj_sm_pick_cb, NULL, NULL, wav, 1, NULL, false);
    return 0;
}
static int lj_sm_pick_take(void *ud, char *dst, size_t cap)
{
    int r;
    (void)ud;
    lj_lock();
    r = sm_pick.state == 2 ? 1 : sm_pick.state == 1 ? 0 : -1;
    if (r == 1) lj_str_copy(dst, cap, sm_pick.path);
    if (r != 0) sm_pick.state = 0;
    lj_unlock();
    return r;
}

const lj_media_host *lj_sdl_media_host(void)
{
    static const lj_media_host h = { NULL, lj_sm_load, lj_sm_play, lj_sm_seek, lj_sm_close, lj_sm_consumed, lj_sm_pick, lj_sm_pick_take };
    return &h;
}
