/* lj_app_io.c - W5 contract (docs/w5-contracts.md seam 10): the app half of the seven W5 features. Complete as written;
 * the io modules and window families behind it are the tracks' stubs until they land. UI thread only. */
#include "app/lj_app_io.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_main_window.h"
#include "engine/lj_engine.h"
#include "io/lj_artnet.h"
#include "io/lj_dmxin.h"
#include "io/lj_extapp.h"
#include "io/lj_fn.h"
#include "io/lj_remote.h"
#include "model/lj_cuelist.h"
#include "windows/lj_extapp_form.h"
#include "windows/lj_windows.h"

enum { LJ_IO_PCM = 2048, LJ_IO_PCM_PASSES = 8 /* LJ_AUDIO_RING / LJ_IO_PCM: the whole ring at most, so a flood cannot starve the wake */ };

static struct {
    lj_app_io_platform plat;
    lj_dmxin_cfg dmxin; lj_dmxin_map map; uint8_t in[LJ_LINK_CH];   /* in: the engine's DMX_IN source, written under lj_lock only */
    lj_audio_cfg audio; lj_audio_state audio_state; uint8_t audio_open; uint32_t audio_dropped;   /* audio_dropped: lj_audio_dropped() at the last pass (ME-191) */
    lj_remote_cfg remote; lj_extapp_cfg extapp; lj_media media;
    int16_t pcm[LJ_IO_PCM];
} lj_io;

void lj_app_io_set_platform(const lj_app_io_platform *p) { if (p) lj_io.plat = *p; else memset(&lj_io.plat, 0, sizeof lj_io.plat); }

static void lj_io_post(uint8_t kind, uint32_t a, uint32_t b)
{
    lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = kind; c.a = a; c.b = b;
    lj_ctx_post(&c);
}

static void lj_io_save_hw(void)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP];
    if (!c->hardware_ini) return;
    if (snprintf(path, sizeof path, "%sHardware.Ini", c->userdir) < (int)sizeof path && lj_ini_save(c->hardware_ini, path) != 0) lj_log("io: cannot write %s", path);
}

static void lj_io_audio_apply(void)
{
    if (lj_io.audio_open && lj_io.plat.audio_close) lj_io.plat.audio_close();
    lj_io.audio_open = 0;
    lj_audio_reset(&lj_io.audio_state);
    if (lj_io.audio.enabled && lj_io.plat.audio_open) lj_io.audio_open = lj_io.plat.audio_open(lj_io.audio.device) == 0;
}

static void lj_io_dmxin_apply(void)
{
    lj_ctx *c = lj_ctx_get();
    lj_dmxin_open(&lj_io.dmxin, c->sacn ? c->sacn->cid : NULL);
    lj_io_post(LJ_CMD_OVERWRITE_ENABLE, 0, lj_io.dmxin.enabled);   /* 05 s3.2: always on while the function is unassigned; off with DMX-in off (the v1.0.0 behaviour); the dmxin track refines */
    lj_io_post(LJ_CMD_DMX_IN_LIVE, 0, lj_io.dmxin.enabled);        /* ME-190: with DMX-in off S1 leaves every channel alone, an Ignore one included */
    if (c->sacn) {                                                   /* Art-Net input rides the sender's socket: the engine thread filters on artnet_in */
        lj_lock(); c->sacn->artnet_in = (uint16_t)(lj_io.dmxin.enabled && lj_io.dmxin.source == LJ_DMXIN_ARTNET ? lj_io.dmxin.universe : 0); lj_unlock();
        { lj_cmd s; memset(&s, 0, sizeof s); s.kind = LJ_CMD_SACN_CFG; s.sacn = c->sacn; lj_ctx_post(&s); }
    }
}

static void lj_io_action(enum lj_desktop_action a, int arg, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    (void)ud;
    switch (a) {
    case LJ_ACT_IO_APPLY:
        switch (arg) {
        case LJ_IO_OUTPUT: if (c->sacn) { lj_cmd s; memset(&s, 0, sizeof s); s.kind = LJ_CMD_SACN_CFG; s.sacn = c->sacn; lj_ctx_post(&s); lj_artnet_cfg_save(c->hardware_ini, c->sacn); } break;
        case LJ_IO_DMXIN:  lj_dmxin_cfg_save(c->hardware_ini, &lj_io.dmxin); lj_io_dmxin_apply(); break;   /* ME-178: [ARTNET] is not written here (artnet_in is runtime only) */
        case LJ_IO_AUDIO:  lj_audio_cfg_save(c->hardware_ini, &lj_io.audio); lj_io_audio_apply(); lj_main_menu_sync(lj_desktop_window()); break;   /* ME-181: &Audio shows with the analyzer */
        case LJ_IO_REMOTE: lj_remote_cfg_save(c->hardware_ini, &lj_io.remote); lj_remote_open(&lj_io.remote); break;
        case LJ_IO_EXTAPP: lj_extapp_cfg_save(c->hardware_ini, &lj_io.extapp); lj_extapp_open(&lj_io.extapp); break;
        default: return;
        }
        lj_io_save_hw();
        break;
    case LJ_ACT_AUDIO_ANALYZER: lj_win_open(LJ_WIN_AUDIO_ANALYZER); break;
    case LJ_ACT_AUDIO_OPTIONS:  lj_win_open(LJ_WIN_HARDWARE_SETUP); break;   /* the audio track selects the tsAudio page */
    case LJ_ACT_EXTAPP_SETUP:   lj_extapp_form_target(arg); lj_win_open(LJ_WIN_EXTAPP_SETUP); break;
    case LJ_ACT_MEDIA_PLAYER:   lj_win_open(LJ_WIN_MEDIA_PLAYER); break;
    case LJ_ACT_REMOTE_SETUP:   lj_win_open(LJ_WIN_REMOTE_SETUP); break;
    case LJ_ACT_AUDIO_TRIG:     lj_io_post(LJ_CMD_TRIG, LJ_RUN_ALL, LJ_TRIG_AUDIO_EVENT); break;
    default: break;
    }
}

static void lj_io_fn(enum lj_fn fn, int edge, uint32_t prog_num, void *ud)
{
    (void)prog_num; (void)ud;
    if (!edge) return;
    switch (fn) {
    case LJ_FN_EMULATE_AUDIO_TRIG:       lj_io_post(LJ_CMD_TRIG, LJ_RUN_ALL, LJ_TRIG_AUDIO_EVENT); break;   /* 05 s9.1: the same command as an analyzer trig */
    case LJ_FN_DMXIN_FUNCTIONS_DISABLE:  lj_io.dmxin.functions_off = 1; break;
    case LJ_FN_DMXIN_FUNCTIONS_ENABLE:   lj_io.dmxin.functions_off = 0; break;
    case LJ_FN_DMXIN_FUNCTIONS_TOGGLE:   lj_io.dmxin.functions_off = (uint8_t)!lj_io.dmxin.functions_off; break;
    default: break;
    }
}

void lj_app_io_start(void)
{
    static const enum lj_desktop_action acts[] = { LJ_ACT_IO_APPLY, LJ_ACT_AUDIO_ANALYZER, LJ_ACT_AUDIO_OPTIONS, LJ_ACT_EXTAPP_SETUP, LJ_ACT_MEDIA_PLAYER, LJ_ACT_AUDIO_TRIG, LJ_ACT_REMOTE_SETUP };
    static const enum lj_fn fns[] = { LJ_FN_EMULATE_AUDIO_TRIG, LJ_FN_DMXIN_FUNCTIONS_DISABLE, LJ_FN_DMXIN_FUNCTIONS_ENABLE, LJ_FN_DMXIN_FUNCTIONS_TOGGLE };
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[160];
    size_t i;
    lj_dmxin_cfg_load(c->hardware_ini, &lj_io.dmxin);
    lj_dmxin_map_init(&lj_io.map);
    if (c->lib && lj_library_path(c->lib, "DmxIn/DmxIn.dat", path, sizeof path) == 0 && lj_dmxin_map_load(path, &lj_io.map, err, sizeof err) != 0) lj_log("io: %s: %s", path, err);
    lj_audio_cfg_load(c->hardware_ini, &lj_io.audio);
    lj_remote_cfg_load(c->hardware_ini, &lj_io.remote);
    lj_extapp_cfg_load(c->hardware_ini, &lj_io.extapp);
    lj_media_init(&lj_io.media);
    c->dmxin = &lj_io.dmxin; c->dmxin_map = &lj_io.map; c->audio = &lj_io.audio; c->audio_state = &lj_io.audio_state;
    c->remote = &lj_io.remote; c->extapp = &lj_io.extapp; c->media = &lj_io.media; c->audio_list = lj_io.plat.audio_list;
    for (i = 0; i < sizeof acts / sizeof acts[0]; i++) lj_desktop_bind_action(acts[i], lj_io_action, NULL);
    for (i = 0; i < sizeof fns / sizeof fns[0]; i++) lj_fn_bind(fns[i], lj_io_fn, NULL);
    if (lj_io.dmxin.enabled) lj_io_dmxin_apply();                    /* W5-X-OFF: every part below opens only when its Hardware.Ini key enables it */
    lj_io_audio_apply();
    if (lj_io.audio.enabled && !lj_io.audio.run_minimized) lj_win_open(LJ_WIN_AUDIO_ANALYZER);   /* LJ2 started LJSound with the show (ponytail: Run minimized = not opened; no minimized state) */
    lj_remote_open(&lj_io.remote);
    lj_extapp_open(&lj_io.extapp);
}

void lj_app_io_idle(uint64_t now_us)
{
    lj_ctx *c = lj_ctx_get();
    int k, trigs = 0;
    size_t n;
    if ((k = lj_dmxin_poll(now_us)) != 0) {                          /* seam 2: the UI-owned frame into the S1 source */
        const lj_dmxin_frame *f = lj_dmxin_frame_get();
        lj_cmd cmd;
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = LJ_CMD_DMX_IN; cmd.a = 1; cmd.dmx_in = lj_io.in; cmd.at_us = lj_clock_us();
        lj_lock();
        memcpy(lj_io.in, f->out, sizeof lj_io.in);                   /* the engine reads lj_io.in at the drain, under the same lock */
        if (c->engine) lj_engine_cmd(c->engine, &cmd);
        lj_unlock();
    }
    lj_dmxin_functions_step(&lj_io.map, k, lj_io.dmxin.functions_off);   /* ME-167: a live frame only; once more on a functions_off change */
    for (k = 0; k < LJ_IO_PCM_PASSES && (n = lj_audio_poll(lj_io.pcm, LJ_IO_PCM)) > 0; k++)   /* seam 4 */
        trigs += lj_audio_analyze(&lj_io.audio_state, &lj_io.audio, lj_io.pcm, n, now_us);
    { uint32_t d = lj_audio_dropped(); lj_io.audio_state.samples += d - lj_io.audio_dropped; lj_io.audio_dropped = d; }   /* ME-191: the ring's drops advance the sample clock too (unsigned difference: wrap-safe) */
    for (k = 0; k < trigs; k++) lj_io_post(LJ_CMD_TRIG, LJ_RUN_ALL, LJ_TRIG_AUDIO_EVENT);
    lj_remote_poll(now_us);                                          /* seam 5 */
    if (c->engine) {                                                 /* seam 6: the strings leave from a frame copy, outside engine/ */
        uint8_t fx[LJ_SCENE_EXT]; char ext[LJ_SCENE_EXT][LJ_EXT_CMD_CAP];
        const lj_frame *fr;
        lj_lock(); fr = lj_engine_frame(c->engine); memcpy(fx, fr->ext_fixture, sizeof fx); memcpy(ext, fr->ext, sizeof ext); lj_unlock();
        lj_extapp_step(fx, &ext[0][0], now_us);
    }
    lj_media_step(&lj_io.media, lj_io.plat.media);                   /* seam 7 */
}

void lj_app_io_stop(void)
{
    if (lj_io.audio_open && lj_io.plat.audio_close) lj_io.plat.audio_close();
    lj_io.audio_open = 0;
    lj_remote_close();
    lj_dmxin_close();
    lj_extapp_close();
    if (lj_io.plat.media && lj_io.media.state != LJ_MEDIA_CLOSED) lj_io.plat.media->close(lj_io.plat.media->ud);
}

int lj_app_io_cl_media(void *ud, const lj_cl_line *l, uint64_t now_us)
{
    char err[160], full[LJ_PATH_CAP];
    const lj_ctx *c = lj_ctx_get();
    (void)ud; (void)now_us;
    if (!l) return -1;
    if (l->cmd == LJ_CL_MP_LOAD_FILE && lj_fs_media_path(c->lib ? c->lib->root : NULL, l->param, full, sizeof full) != 0) { lj_log("cue list: %s", LJ_MEDIA_ERR_FORMAT); return -1; }   /* ME-169: relative = the library root */
    if (lj_media_cmd(&lj_io.media, lj_io.plat.media, l->cmd, l->cmd == LJ_CL_MP_LOAD_FILE ? full : l->param, l->param_ms, err, sizeof err) != 0) { lj_log("cue list: %s", err); return -1; }
    if (l->cmd == LJ_CL_MP_START) lj_win_open(LJ_WIN_MEDIA_PLAYER);          /* media track: "Start the LightJockey Mediaplayer" */
    else if (l->cmd == LJ_CL_MP_CLOSE) lj_win_close(LJ_WIN_MEDIA_PLAYER);   /* "Close the LightJockey Mediaplayer" */
    return 0;
}

int lj_app_io_mp_time(uint32_t *ms)
{
    if (lj_io.media.state == LJ_MEDIA_CLOSED) return -1;
    if (ms) *ms = lj_media_time_cs(&lj_io.media) * 10u;
    return 0;
}
