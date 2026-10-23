/* lj_sdl_audio.c - SDL3 audio capture for the Audio Analyzer (platform/sdl/lj_sdl_audio.h). W5 CONTRACT STUB: the audio
 * track replaces every body (docs/w5-contracts.md seam 4). Until then no device is listed or opened. */
#include <SDL3/SDL.h>
#include "platform/sdl/lj_sdl_audio.h"

int  lj_sdl_audio_list(lj_audio_dev *out, int max) { (void)out; (void)max; return 0; }
int  lj_sdl_audio_open(const char *device) { (void)device; return -1; }
void lj_sdl_audio_close(void) {}
