/* lj_media.h - the LJ MediaPlayer (design 2 io/lj_media.c, 8.5, 10 W5; note 03 s6.3, note 05 s8; docs/w5-contracts.md
 * seam 7; track media).
 *
 * SDL-free (W5-MED-1): the state machine and the clip clock only. The clock is computed from the sample frames the audio
 * device has consumed (lj_media_host.consumed), never the wall clock, in 1/100 s (time code HH:MM:SS:hh, 05 s8.1), so a
 * deterministic test feeds a frame count and a Wait MP fires at the sample-exact point. Decoding and playback are
 * platform/sdl/lj_sdl_media.c through the host below: SDL_LoadWAV only (W5-MED-2: SDL3 decodes nothing else and no new
 * dependency is allowed), so the player is audio-only and plays WAV; any other file is refused with
 * LJ_MEDIA_ERR_FORMAT, a file over LJ_MEDIA_FILE_MAX before the load with LJ_MEDIA_ERR_SIZE. Video is never played;
 * the video options of the LJ2 player stay greyed and are explained in docs/fidelity.md, the help and the user guide.
 *
 * Cue list (design 8.5, W5-MED-3): the nine MP commands of model/lj_cuelist.h (LJ_CL_MP_LOAD_FILE .. LJ_CL_MP_CLOSE)
 * leave lj_cl_cmd_inert and reach lj_media_cmd through lj_cl_host.media on the UI thread (the runner's 10 ms wake);
 * MP Load File waits at most LJ_MEDIA_LOAD_WAIT_MS and MP Seek at most LJ_MEDIA_SEEK_WAIT_MS (05 s8.2); MP Wait and the
 * sbMPTime time-code source read lj_media_time_cs through lj_cl_host.timecode (LJ_TC_MP, ms = cs * 10). One player
 * returns time code (05 s8.1); `.mps` playback files started through Execute File are not reproduced (a fidelity row).
 * The CD and DA rows stay greyed with the hint for good (ME-11). */
#ifndef LJ_IO_MEDIA_H
#define LJ_IO_MEDIA_H

#include <stddef.h>
#include <stdint.h>

enum lj_media_state { LJ_MEDIA_CLOSED = 0, LJ_MEDIA_LOADED, LJ_MEDIA_PLAYING, LJ_MEDIA_PAUSED, LJ_MEDIA_STOPPED, LJ_MEDIA_ENDED };
#define LJ_MEDIA_FILE_MAX     (256u * 1024u * 1024u)   /* refused before the load, PROPOSAL rig W5-MED-2 */
#define LJ_MEDIA_LOAD_WAIT_MS 5000u                    /* 05 s8.2 "MP load file (waits max 5 s)" */
#define LJ_MEDIA_SEEK_WAIT_MS 5000u                    /* 05 s8.2 "MP Seek (max 5 s)" */
#define LJ_MEDIA_ERR_FORMAT "Only WAV files can be played (LJ4EVR plays audio through SDL3, without codecs)"   /* ours, PROPOSAL rig W5-MED-2 */
#define LJ_MEDIA_ERR_SIZE   "The file is too large to load"                                                  /* ours, PROPOSAL rig W5-MED-2 */

/* What platform/sdl supplies (installed by app/lj_app.c; tests install a fake). Every member runs on the UI thread. */
typedef struct {
    void *ud;
    int      (*load)(void *ud, const char *path, uint32_t *rate, uint64_t *frames, char *err, size_t cap);   /* decode + open the stream, paused; 0 / -1 */
    int      (*play)(void *ud, int on);                  /* resume (1) / pause (0) the device */
    int      (*seek)(void *ud, uint64_t frame);          /* restart the stream at that sample frame */
    void     (*close)(void *ud);                         /* stop and free the clip */
    uint64_t (*consumed)(void *ud);                      /* sample frames the device has taken since the last load / seek, plus the seek point */
} lj_media_host;

typedef struct {
    uint8_t  state;                /* enum lj_media_state */
    char     path[260];            /* CP1252, the clip loaded */
    uint32_t rate;                 /* sample frames per second */
    uint64_t frames;               /* clip length in sample frames */
    uint32_t cs;                   /* clip clock, 1/100 s, from consumed frames: frames_consumed * 100 / rate */
    char     recent[5][260];       /* the player window's recent-files list of five (W5-MED-5) */
} lj_media;

void     lj_media_init(lj_media *m);
int      lj_media_cmd(lj_media *m, const lj_media_host *h, int cl_cmd, const char *param, uint32_t param_ms, char *err, size_t cap);   /* cl_cmd = LJ_CL_MP_* (model/lj_cuelist.h); param = the file of MP Load File, param_ms = MP Seek's time; 0 / -1 with the message */
void     lj_media_step(lj_media *m, const lj_media_host *h);   /* every UI pass: cs from consumed(); PLAYING -> ENDED at the clip's end */
uint32_t lj_media_time_cs(const lj_media *m);
int      lj_media_format_tc(char *dst, size_t cap, uint32_t cs);   /* "HH:MM:SS:hh"; 0 / -1 */
int      lj_media_check_file(const char *path, uint64_t size, char *err, size_t cap);   /* the extension and the size cap before a load: 0 / -1 with LJ_MEDIA_ERR_FORMAT / _SIZE */

#endif /* LJ_IO_MEDIA_H */
