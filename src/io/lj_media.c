/* lj_media.c - the LJ MediaPlayer state machine and clip clock (io/lj_media.h; design 8.5; docs/media.md). SDL-free and
 * integer only: the clock is sample frames consumed * 100 / rate, read from the host every UI pass, never the wall clock. */
#include <string.h>
#include "io/lj_media.h"
#include "core/lj_str.h"
#include "model/lj_cuelist.h"

static int lj_media_err(char *err, size_t cap, const char *msg) { if (err && cap) lj_str_copy(err, cap, msg); return -1; }

void lj_media_init(lj_media *m) { if (m) memset(m, 0, sizeof *m); }

static uint32_t lj_media_cs_of(const lj_media *m, uint64_t frame) { return m->rate ? (uint32_t)(frame * 100u / m->rate) : 0; }

int lj_media_recent_push(lj_media *m, const char *path)
{
    int i, j = LJ_MEDIA_RECENT - 1;
    if (!m || !path || !*path || strlen(path) >= sizeof m->recent[0]) return -1;
    for (i = 0; i < LJ_MEDIA_RECENT; i++) if (!strcmp(m->recent[i], path)) { j = i; break; }
    for (; j > 0; j--) memcpy(m->recent[j], m->recent[j - 1], sizeof m->recent[j]);
    lj_str_copy(m->recent[0], sizeof m->recent[0], path);
    return 0;
}

static void lj_media_drop(lj_media *m, const lj_media_host *h)
{
    if (m->state != LJ_MEDIA_CLOSED && h && h->close) h->close(h->ud);
    m->state = LJ_MEDIA_CLOSED; m->path[0] = 0; m->rate = 0; m->frames = 0; m->cs = 0;
}

int lj_media_cmd(lj_media *m, const lj_media_host *h, int cl_cmd, const char *param, uint32_t param_ms, char *err, size_t cap)
{
    uint64_t f;
    if (!m) return lj_media_err(err, cap, LJ_MEDIA_ERR_NOCLIP);
    if (h) m->host = h;
    if (err && cap) err[0] = 0;
    switch (cl_cmd) {
    case LJ_CL_MP_WAIT: case LJ_CL_MP_START: return 0;
    case LJ_CL_MP_CLOSE_CLIP: case LJ_CL_MP_CLOSE: lj_media_drop(m, h); return 0;
    case LJ_CL_MP_LOAD_FILE: {
        uint32_t rate = 0;
        uint64_t frames = 0;
        if (lj_media_check_file(param, 0, err, cap) != 0) return -1;              /* the extension; the host checks the size */
        lj_media_drop(m, h);
        if (!h || !h->load) return lj_media_err(err, cap, "No audio output");    /* ours: headless */
        if (h->load(h->ud, param, &rate, &frames, err, cap) != 0) return -1;
        if (rate == 0 || frames == 0) { h->close(h->ud); return lj_media_err(err, cap, LJ_MEDIA_ERR_FORMAT); }   /* an empty clip cannot be timed */
        m->state = LJ_MEDIA_LOADED; m->rate = rate; m->frames = frames; m->cs = 0;
        lj_str_copy(m->path, sizeof m->path, param);
        lj_media_recent_push(m, param);
        return 0; }
    default: break;
    }
    if (cl_cmd < LJ_CL_MP_LOAD_FILE || cl_cmd > LJ_CL_MP_CLOSE) return lj_media_err(err, cap, "not a MediaPlayer command");
    if (m->state == LJ_MEDIA_CLOSED || !h) return lj_media_err(err, cap, LJ_MEDIA_ERR_NOCLIP);
    switch (cl_cmd) {
    case LJ_CL_MP_PLAY:
        if (m->state == LJ_MEDIA_PLAYING) return 0;
        if (m->state == LJ_MEDIA_ENDED) { if (h->seek(h->ud, 0) != 0) return lj_media_err(err, cap, "seek failed"); m->cs = 0; }
        if (h->play(h->ud, 1) != 0) return lj_media_err(err, cap, "play failed");
        m->state = LJ_MEDIA_PLAYING;
        return 0;
    case LJ_CL_MP_PAUSE:
        if (m->state != LJ_MEDIA_PLAYING) return 0;
        h->play(h->ud, 0);
        m->state = LJ_MEDIA_PAUSED;
        return 0;
    case LJ_CL_MP_STOP:
        h->play(h->ud, 0);
        h->seek(h->ud, 0);
        m->state = LJ_MEDIA_STOPPED; m->cs = 0;
        return 0;
    case LJ_CL_MP_SEEK:
        f = (uint64_t)param_ms * m->rate / 1000u;
        if (f > m->frames) f = m->frames;
        if (h->seek(h->ud, f) != 0) return lj_media_err(err, cap, "seek failed");
        m->cs = lj_media_cs_of(m, f);
        if (m->state == LJ_MEDIA_ENDED && f < m->frames) m->state = LJ_MEDIA_PAUSED;   /* the device was paused at the end */
        return 0;
    default: return lj_media_err(err, cap, "not a MediaPlayer command");
    }
}

void lj_media_step(lj_media *m, const lj_media_host *h)
{
    uint64_t c;
    if (!m) return;
    if (h) m->host = h;
    if (m->state == LJ_MEDIA_CLOSED || !h || !h->consumed) return;
    c = h->consumed(h->ud);
    if (c > m->frames) c = m->frames;
    m->cs = lj_media_cs_of(m, c);
    if (m->state == LJ_MEDIA_PLAYING && c >= m->frames) { h->play(h->ud, 0); m->state = LJ_MEDIA_ENDED; }
}

uint32_t lj_media_time_cs(const lj_media *m) { return m ? m->cs : 0; }

int lj_media_format_tc(char *dst, size_t cap, uint32_t cs)
{
    uint32_t h = cs / 360000u, mi = cs / 6000u % 60u, s = cs / 100u % 60u, f = cs % 100u;
    char b[16];
    int i = 0;
    if (!dst || cap < 12 || h > 99) { if (dst && cap) dst[0] = 0; return -1; }
    b[i++] = (char)('0' + h / 10); b[i++] = (char)('0' + h % 10); b[i++] = ':';
    b[i++] = (char)('0' + mi / 10); b[i++] = (char)('0' + mi % 10); b[i++] = ':';
    b[i++] = (char)('0' + s / 10); b[i++] = (char)('0' + s % 10); b[i++] = ':';
    b[i++] = (char)('0' + f / 10); b[i++] = (char)('0' + f % 10); b[i] = 0;
    memcpy(dst, b, 12);
    return 0;
}

int lj_media_check_file(const char *path, uint64_t size, char *err, size_t cap)
{
    size_t n = path ? strlen(path) : 0;
    const char *e;
    if (!n) return lj_media_err(err, cap, "No file name");                         /* ours */
    if (n >= sizeof ((lj_media *)0)->path) return lj_media_err(err, cap, "The file name is too long");   /* ours */
    e = path + n;
    while (e > path && e[-1] != '.' && e[-1] != '/' && e[-1] != '\\') e--;
    if (e == path || e[-1] != '.' || strlen(e) != 3 || (e[0] | 0x20) != 'w' || (e[1] | 0x20) != 'a' || (e[2] | 0x20) != 'v')
        return lj_media_err(err, cap, LJ_MEDIA_ERR_FORMAT);
    if (size > LJ_MEDIA_FILE_MAX) return lj_media_err(err, cap, LJ_MEDIA_ERR_SIZE);
    if (err && cap) err[0] = 0;
    return 0;
}
