/* lj_sdl_audio.h - Audio Analyzer capture through SDL3 (docs/w5-contracts.md seam 4; track audio). SDL-free header.
 * lj_sdl_audio.c opens one SDL3 recording stream (SDL_OpenAudioDeviceStream, LJ_AUDIO_RATE, mono, S16) whose callback
 * only calls lj_audio_post (io/lj_audio.h): no engine, window or analyzer state is touched on SDL's audio thread. A
 * denied microphone permission (macOS NSMicrophoneUsageDescription, Windows privacy settings) fails the open or gives
 * silence; either is logged and shown in the status line, never a crash (W5-AUD-5). CI uses SDL's dummy driver. */
#ifndef LJ_PLATFORM_SDL_SDL_AUDIO_H
#define LJ_PLATFORM_SDL_SDL_AUDIO_H

#include "io/lj_audio.h"

int  lj_sdl_audio_list(lj_audio_dev *out, int max);   /* recording devices, CP1252 names; count (0 = none), -1 when the audio subsystem is unavailable */
int  lj_sdl_audio_open(const char *device);           /* "" = the default device; 0 / -1 + log naming the device */
void lj_sdl_audio_close(void);                        /* no-op when closed; called before the engine stops */

#endif /* LJ_PLATFORM_SDL_SDL_AUDIO_H */
