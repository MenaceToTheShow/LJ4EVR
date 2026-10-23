/* lj_app_io.h - the W5 I/O seam of the app (docs/w5-contracts.md seam 10). It owns the W5 records lj_ctx points at
 * (DMX-in, audio, remote, External Application, MediaPlayer), opens what Hardware.Ini enables at start (nothing on a
 * fresh userdata, W5-X-OFF), polls every part on each UI loop pass after lj_app_cuelist_host_idle, posts the engine commands
 * (LJ_CMD_DMX_IN, LJ_CMD_OVERWRITE_ENABLE, LJ_CMD_TRIG / LJ_TRIG_AUDIO_EVENT) under lj_lock, and closes every listener
 * before the engine thread stops (W5-X-THREAD). SDL-free: the platform pieces (audio capture, WAV playback) arrive
 * through lj_app_io_set_platform, so tests drive it headless. The tracks never edit this file's logic; a track that
 * needs another call here asks the integrator (the W4 lj_app_cuelist_host.c rule). */
#ifndef LJ_APP_APP_IO_H
#define LJ_APP_APP_IO_H

#include <stdint.h>
#include "io/lj_audio.h"
#include "io/lj_media.h"
#include "model/lj_model.h"

typedef struct {
    int  (*audio_list)(lj_audio_dev *out, int max);   /* platform/sdl/lj_sdl_audio.h; NULL = no audio (headless) */
    int  (*audio_open)(const char *device);
    void (*audio_close)(void);
    const lj_media_host *media;                        /* platform/sdl/lj_sdl_media.h; NULL = no playback */
} lj_app_io_platform;

void lj_app_io_set_platform(const lj_app_io_platform *p);   /* lj_app.c, before lj_app_io_start; copied */
void lj_app_io_start(void);                                  /* after lj_app_cuelist_host_windows_start: ctx records, cfg loads, opens what is enabled, binds the W5 actions and lj_fn rows */
void lj_app_io_idle(uint64_t now_us);                        /* every UI loop pass: DMX-in, audio, remote, External Application, MediaPlayer */
void lj_app_io_stop(void);                                   /* before lj_platform_engine_stop: closes every W5 listener and device */
int  lj_app_io_cl_media(void *ud, const lj_cl_line *l, uint64_t now_us);   /* lj_cl_host.media: the MP lines through lj_media_cmd */
int  lj_app_io_mp_time(uint32_t *ms);                        /* lj_cl_host.timecode for LJ_TC_MP: 0 with the clip clock in ms, -1 when no clip is loaded */

#endif /* LJ_APP_APP_IO_H */
