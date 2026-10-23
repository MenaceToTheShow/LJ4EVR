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
 * LJ2's MP Load File / MP Seek waited up to 5 s (05 s8.2, LJ_MEDIA_*_WAIT_MS kept as the facts); here both are synchronous
 * and the list never waits on them (ME-180: the file cap bounds the inline decode); MP Wait and the
 * sbMPTime time-code source read lj_media_time_cs through lj_cl_host.timecode (LJ_TC_MP, ms = cs * 10). One player
 * returns time code (05 s8.1); `.mps` playback files started through Execute File are not reproduced (a fidelity row).
 * The CD and DA rows stay greyed with the hint for good (ME-11). */
#ifndef LJ_IO_MEDIA_H
#define LJ_IO_MEDIA_H

#include <stddef.h>
#include <stdint.h>

enum lj_media_state { LJ_MEDIA_CLOSED = 0, LJ_MEDIA_LOADED, LJ_MEDIA_PLAYING, LJ_MEDIA_PAUSED, LJ_MEDIA_STOPPED, LJ_MEDIA_ENDED };
/* ME-180: refused before the load. SDL_LoadWAV decodes inline on the UI thread (no loader thread, ponytail), so the cap
 * is what a stated stall budget allows: 250 ms, under the audio capture ring (16384 samples, ~370 ms at 44.1 kHz).
 * Measured on the work PC (Release SDL3, file in the OS cache): 64 MB in 19 ms, 128 MB in 31 ms, 256 MB in 59 ms; a cold
 * read adds the disk, ~160 ms for 64 MB at 400 MB/s. PROPOSAL rig W5-MED-2: about 6 min of 44.1 kHz 16-bit stereo. */
#define LJ_MEDIA_FILE_MAX     (64u * 1024u * 1024u)
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
    /* ---- media track (appended): the OS open dialog for a WAV (TOpenDialog OpenMediaFileDialog, bBrowseMPCue); NULL = none (headless) */
    int      (*pick)(void *ud);                          /* starts the dialog; 0 / -1 */
    int      (*pick_take)(void *ud, char *dst, size_t cap);   /* 1 = a CP1252 path arrived, 0 = still open, -1 = cancelled / idle */
} lj_media_host;

typedef struct {
    uint8_t  state;                /* enum lj_media_state */
    char     path[260];            /* CP1252, the clip loaded */
    uint32_t rate;                 /* sample frames per second */
    uint64_t frames;               /* clip length in sample frames */
    uint32_t cs;                   /* clip clock, 1/100 s, from consumed frames: frames_consumed * 100 / rate */
    char     recent[5][260];       /* the player window's recent-files list of five (W5-MED-5) */
    /* ---- media track (appended) ---- */
    const lj_media_host *host;     /* the host lj_media_step last ran with: the player window drives the same clip through it */
    uint8_t  pick_owner;           /* who started host->pick: 0 none, 1 the Cuelist window's MP Load File line, 2 the player window */
} lj_media;

void     lj_media_init(lj_media *m);
int      lj_media_cmd(lj_media *m, const lj_media_host *h, int cl_cmd, const char *param, uint32_t param_ms, char *err, size_t cap);   /* cl_cmd = LJ_CL_MP_* (model/lj_cuelist.h); param = the file of MP Load File, param_ms = MP Seek's time; 0 / -1 with the message */
void     lj_media_step(lj_media *m, const lj_media_host *h);   /* every UI pass: cs from consumed(); PLAYING -> ENDED at the clip's end */
uint32_t lj_media_time_cs(const lj_media *m);
int      lj_media_format_tc(char *dst, size_t cap, uint32_t cs);   /* "HH:MM:SS:hh"; 0 / -1 */
int      lj_media_check_file(const char *path, uint64_t size, char *err, size_t cap);   /* the extension and the size cap before a load: 0 / -1 with LJ_MEDIA_ERR_FORMAT / _SIZE */

/* ---- media track (below the contract) ----
 * Command semantics (H2.100 "Mediaplayer Commands": the quoted words; the rest PROPOSAL rig 05 s8.2):
 *   MP Load File  closes the clip and loads `param`, paused at 0 ("waits max 5 s": SDL_LoadWAV is synchronous, never waits)
 *   MP Play       "Play the loaded media file from its current position"; an ENDED clip restarts at 0 (PROPOSAL)
 *   MP Seek       param_ms, clamped to the clip's end; play / pause kept ("max 5 s": synchronous, never waits)
 *   MP Stop       "Stop playback and set position to beginning of file"
 *   MP Pause      "Pause playback."
 *   MP Wait       not a command: the runner's condition on the clip clock (0 here)
 *   MP Close Clip "Stop and clear the current media clip."
 *   MP Start      opens the player window (the app does it; 0 here); MP Close closes the window and the clip
 * MP Wait's frame field is 0-99 (TClEditPanels UpDown28 Max = 99, the clock's 1/100 s); the help's "Frames (0-74)" is the CD
 * row's text repeated (docs/media.md). */
#define LJ_MEDIA_ERR_NOCLIP "No media clip loaded"   /* ours, PROPOSAL rig W5-MED-3 */
#define LJ_MEDIA_RECENT 5                            /* H2.100: "pick from the 5 most recently loaded media files" */
int lj_media_recent_push(lj_media *m, const char *path);   /* most recent first, no duplicates; 0 / -1 */

#endif /* LJ_IO_MEDIA_H */
