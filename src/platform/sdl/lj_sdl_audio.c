/* lj_sdl_audio.c - SDL3 audio capture for the Audio Analyzer (platform/sdl/lj_sdl_audio.h). One recording stream at
 * LJ_AUDIO_RATE mono S16 (SDL converts from the device format: the one tagged capture format, PROPOSAL rig W5-AUD-1);
 * its callback runs on SDL's audio thread and only copies into the analyzer ring (lj_audio_post). Device names are
 * matched by their CP1252 text; "" or an absent name opens SDL's default recording device. */
#include <SDL3/SDL.h>
#include <string.h>
#include "platform/sdl/lj_sdl_audio.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

static SDL_AudioStream *lj_sa_stream;

static int lj_sa_init(void)
{
    if (SDL_WasInit(SDL_INIT_AUDIO)) return 0;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) { lj_log("audio: SDL audio unavailable: %s", SDL_GetError()); return -1; }
    return 0;
}

static void SDLCALL lj_sa_cb(void *ud, SDL_AudioStream *s, int additional, int total)
{
    int16_t buf[1024];
    int got;
    (void)ud; (void)additional; (void)total;
    while ((got = SDL_GetAudioStreamData(s, buf, (int)sizeof buf)) > 0) lj_audio_post(buf, (size_t)got / sizeof buf[0]);   /* the only act on this thread */
}

int lj_sdl_audio_list(lj_audio_dev *out, int max)
{
    SDL_AudioDeviceID *ids;
    int i, n = 0, count = 0;
    if (lj_sa_init() != 0) return -1;
    if (!out || max <= 0) return 0;
    ids = SDL_GetAudioRecordingDevices(&count);
    if (!ids) return 0;
    for (i = 0; i < count && n < max; i++) {
        const char *name = SDL_GetAudioDeviceName(ids[i]);
        if (!name) continue;
        lj_utf8_to_cp1252(out[n].name, sizeof out[n].name, name);
        n++;
    }
    SDL_free(ids);
    return n;
}

int lj_sdl_audio_open(const char *device)
{
    SDL_AudioSpec spec;
    SDL_AudioDeviceID dev = SDL_AUDIO_DEVICE_DEFAULT_RECORDING, *ids;
    int i, count = 0;
    char name[LJ_AUDIO_DEV_NAME], msg[96];
    lj_sdl_audio_close();
    if (lj_sa_init() != 0) { lj_audio_set_device_status(0, "Audio-in: no audio subsystem"); return -1; }
    if (device && device[0] && (ids = SDL_GetAudioRecordingDevices(&count)) != NULL) {
        for (i = 0; i < count; i++) {
            const char *n = SDL_GetAudioDeviceName(ids[i]);
            if (n && lj_utf8_to_cp1252(name, sizeof name, n) == 0 && strcmp(name, device) == 0) { dev = ids[i]; break; }
        }
        if (dev == SDL_AUDIO_DEVICE_DEFAULT_RECORDING) lj_log("audio: device \"%s\" not present; using the default device", device);
        SDL_free(ids);
    }
    spec.format = SDL_AUDIO_S16; spec.channels = 1; spec.freq = LJ_AUDIO_RATE;
    lj_sa_stream = SDL_OpenAudioDeviceStream(dev, &spec, lj_sa_cb, NULL);
    if (!lj_sa_stream) {   /* a denied microphone (macOS privacy prompt answered No) lands here: logged and shown, never fatal (W5-AUD-5) */
        lj_log("audio: cannot open recording device \"%s\": %s", device && device[0] ? device : "default", SDL_GetError());
        SDL_snprintf(msg, sizeof msg, "Audio-in failed: %s", SDL_GetError());
        lj_audio_set_device_status(0, msg);
        return -1;
    }
    if (!SDL_ResumeAudioStreamDevice(lj_sa_stream)) lj_log("audio: cannot start recording: %s", SDL_GetError());
    lj_log("audio: recording from \"%s\" at %d Hz mono S16", device && device[0] ? device : "default", LJ_AUDIO_RATE);
    lj_audio_set_device_status(1, "");
    return 0;
}

void lj_sdl_audio_close(void)
{
    if (!lj_sa_stream) return;
    SDL_DestroyAudioStream(lj_sa_stream);   /* waits for a running callback; the UI thread must not hold lj_lock here */
    lj_sa_stream = NULL;
    lj_audio_set_device_status(0, "");
    lj_log("audio: recording closed");
}
