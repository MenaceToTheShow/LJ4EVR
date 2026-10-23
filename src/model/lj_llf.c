/* lj_llf.c - see lj_llf.h. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include "model/lj_llf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"

static int card_ok(uint32_t b) { return b == 4u << 20 || b == 8u << 20 || b == 16u << 20 || b == 32u << 20 || b == 64u << 20; }

int lj_llf_valid_format(int channels, int hz)
{
    return (channels == 64 || channels == 128 || channels == 256 || channels == 512) && (hz == 10 || hz == 20 || hz == 40);
}

uint32_t lj_llf_max_seconds(uint32_t card_bytes, int channels, int hz)
{
    if (!lj_llf_valid_format(channels, hz)) return 0;
    return card_bytes / ((uint32_t)(channels + 1) * (uint32_t)hz);
}

int lj_llf_rec_begin(lj_llf_rec *r, int link, int channels, int hz, int card_mb, int compress)
{
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    if (link < 1 || link > LJ_LINKS || !lj_llf_valid_format(channels, hz) || !card_ok((uint32_t)card_mb << 20)) return -1;
    r->data = (uint8_t *)malloc((size_t)card_mb << 20);    /* not lj_fatal: LJ2 reports this and cancels (the window shows the exe message) */
    if (!r->data) return -1;
    r->link = (uint8_t)link; r->compress = compress != 0; r->channels = (uint16_t)channels; r->hz = (uint16_t)hz;
    r->card_bytes = (uint32_t)card_mb << 20;
    return 0;
}

int lj_llf_rec_push(lj_llf_rec *r, const uint8_t *slots)
{
    uint32_t need;
    if (!r || !r->data || !slots) return -1;
    if (r->compress && r->frames && memcmp(r->prev, slots, r->channels) == 0) {
        if (r->used + 1 > r->card_bytes) return 1;
        r->data[r->used++] = 1;
    } else {
        need = 1u + r->channels;
        if (r->used + need > r->card_bytes) return 1;
        r->data[r->used] = 0;
        memcpy(r->data + r->used + 1, slots, r->channels);
        r->used += need;
        memcpy(r->prev, slots, r->channels);
    }
    r->frames++;
    return 0;
}

void lj_llf_rec_free(lj_llf_rec *r) { if (!r) return; free(r->data); memset(r, 0, sizeof *r); }

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int lj_llf_write(const char *path, const lj_llf_rec *r, const char *user_notes, const char *sys_notes)
{
    uint8_t h[LJ_LLF_HEADER];
    size_t un = user_notes ? strlen(user_notes) : 0, sn = sys_notes ? strlen(sys_notes) : 0;
    FILE *f;
    int ok;
    if (!path || !r || !r->data || un >= LJ_LLF_NOTES_MAX || sn >= LJ_LLF_NOTES_MAX) { lj_log("llf: nothing to write or notes too long"); return -1; }
    memset(h, 0, sizeof h);
    memcpy(h, LJ_LLF_MAGIC, 6);
    put16(h + 6, LJ_LLF_VERSION);
    h[8] = r->link; h[9] = r->compress ? 1 : 0;
    put16(h + 10, r->channels); put16(h + 12, r->hz);
    put32(h + 16, r->card_bytes); put32(h + 20, r->frames); put32(h + 24, r->used);
    put32(h + 28, (uint32_t)un); put32(h + 32, (uint32_t)sn);
    f = fopen(path, "wb");
    if (!f) { lj_log("llf: cannot create %s", path); return -1; }
    ok = fwrite(h, 1, sizeof h, f) == sizeof h && fwrite(user_notes ? user_notes : "", 1, un, f) == un
      && fwrite(sys_notes ? sys_notes : "", 1, sn, f) == sn && fwrite(r->data, 1, r->used, f) == r->used;
    if (fclose(f) != 0) ok = 0;
    if (!ok) { lj_log("llf: write error on %s", path); remove(path); return -1; }
    return 0;
}

static char *dup_n(const uint8_t *p, size_t n)
{
    char *s = (char *)malloc(n + 1);
    if (!s) lj_fatal("llf: out of memory");
    memcpy(s, p, n); s[n] = 0;
    return s;
}

#define FAIL(...) do { if (err && cap) snprintf(err, cap, __VA_ARGS__); lj_llf_file_free(out); return -1; } while (0)

int lj_llf_parse(const uint8_t *buf, size_t len, lj_llf_file *out, char *err, size_t cap)
{
    uint32_t frames, data_n, un, sn, i, at;
    unsigned ch, hz;
    const uint8_t *d;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (err && cap) err[0] = 0;
    if (!buf || len < LJ_LLF_HEADER) FAIL("truncated header (%u bytes)", (unsigned)len);
    if (memcmp(buf, LJ_LLF_MAGIC, 6) != 0) FAIL("not an LJ4EVR .LLF file");
    if (get16(buf + 6) != LJ_LLF_VERSION) FAIL("unknown .LLF version %u", get16(buf + 6));
    ch = get16(buf + 10); hz = get16(buf + 12);
    if (buf[8] < 1 || buf[8] > LJ_LINKS || buf[9] > 1 || !lj_llf_valid_format((int)ch, (int)hz) || !card_ok(get32(buf + 16)))
        FAIL("bad header (link %u, channels %u, %u Hz, card %u)", buf[8], ch, hz, (unsigned)get32(buf + 16));
    frames = get32(buf + 20); data_n = get32(buf + 24); un = get32(buf + 28); sn = get32(buf + 32);
    if (un >= LJ_LLF_NOTES_MAX || sn >= LJ_LLF_NOTES_MAX || data_n > get32(buf + 16)) FAIL("bad lengths");
    if ((uint64_t)LJ_LLF_HEADER + un + sn + data_n != len) FAIL("%s: %u bytes, header says %u", (uint64_t)LJ_LLF_HEADER + un + sn + data_n > len ? "truncated" : "trailing bytes",
                                                             (unsigned)len, (unsigned)(LJ_LLF_HEADER + un + sn + data_n));
    if ((uint64_t)frames * ch > LJ_LLF_DECODE_MAX) FAIL("%u frames are more than this reader decodes", (unsigned)frames);
    out->link = buf[8]; out->compress = buf[9]; out->channels = (uint16_t)ch; out->hz = (uint16_t)hz;
    out->card_bytes = get32(buf + 16); out->frames = frames;
    out->user_notes = dup_n(buf + LJ_LLF_HEADER, un);
    out->sys_notes = dup_n(buf + LJ_LLF_HEADER + un, sn);
    out->frame = (uint8_t *)malloc(frames ? (size_t)frames * ch : 1);
    if (!out->frame) lj_fatal("llf: out of memory");
    d = buf + LJ_LLF_HEADER + un + sn;
    for (i = 0, at = 0; i < frames; i++) {
        if (at >= data_n) FAIL("frame %u: data ends early", (unsigned)i);
        if (d[at] == 0) {
            if (data_n - at - 1 < ch) FAIL("frame %u: truncated", (unsigned)i);
            memcpy(out->frame + (size_t)i * ch, d + at + 1, ch); at += 1 + ch;
        } else if (d[at] == 1 && out->compress && i > 0) {
            memcpy(out->frame + (size_t)i * ch, out->frame + (size_t)(i - 1) * ch, ch); at++;
        } else FAIL("frame %u: bad tag %u", (unsigned)i, d[at]);
    }
    if (at != data_n) FAIL("%u bytes after the last frame", (unsigned)(data_n - at));
    return 0;
}

int lj_llf_read(const char *path, lj_llf_file *out, char *err, size_t cap)
{
    FILE *f;
    long n;
    uint8_t *buf;
    int rc;
    if (out) memset(out, 0, sizeof *out);
    if (!path || !out || (f = fopen(path, "rb")) == NULL) { if (err && cap) snprintf(err, cap, "cannot open %s", path ? path : "(null)"); return -1; }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || n > LJ_LLF_HEADER + 2L * LJ_LLF_NOTES_MAX + (64L << 20) || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); if (err && cap) snprintf(err, cap, "%s: not a readable .LLF size", path); return -1;
    }
    buf = (uint8_t *)malloc(n ? (size_t)n : 1);
    if (!buf) lj_fatal("llf: out of memory");
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); if (err && cap) snprintf(err, cap, "%s: read error", path); return -1; }
    fclose(f);
    rc = lj_llf_parse(buf, (size_t)n, out, err, cap);
    free(buf);
    return rc;
}

void lj_llf_file_free(lj_llf_file *f)
{
    if (!f) return;
    free(f->user_notes); free(f->sys_notes); free(f->frame);
    memset(f, 0, sizeof *f);
}
