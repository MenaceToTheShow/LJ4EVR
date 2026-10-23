/* lj_sdl_media.h - the MediaPlayer's playback through SDL3 (docs/w5-contracts.md seam 7; track media). SDL-free header.
 * lj_sdl_media.c fills an lj_media_host (io/lj_media.h): load = lj_media_check_file + SDL_LoadWAV into one playback
 * stream (SDL_OpenAudioDeviceStream, paused); play = SDL_ResumeAudioStreamDevice / SDL_PauseAudioStreamDevice; seek =
 * clear and re-put the stream from the frame; consumed = frames queued minus SDL_GetAudioStreamQueued, under the
 * stream's own lock. No codec, no new dependency (W5-X-DEP). */
#ifndef LJ_PLATFORM_SDL_SDL_MEDIA_H
#define LJ_PLATFORM_SDL_SDL_MEDIA_H

#include "io/lj_media.h"

const lj_media_host *lj_sdl_media_host(void);   /* the host app/lj_app.c installs with lj_app_io_set_media_host; never NULL */

#endif /* LJ_PLATFORM_SDL_SDL_MEDIA_H */
