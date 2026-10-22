/* lj_patch.c - the global patch file (lj_patch.h, 02 s7.1, design 5.2): the .GPA files of
 * the Global Patch dialog and <library>\GlobalPatch.GPA (PROPOSAL rig). Numbers go
 * through lj_kv_range (ME-77). */
#include <stdlib.h>
#include <string.h>
#include "model/lj_patch.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"

enum { LJ_HTP_MASK_MAX = (1L << LJ_HTP_GROUPS) - 1 };   /* 24 HTP groups = bits 0..23 */

static const lj_patch_ch lj_patch_dflt = { LJ_SRC_NONE, 0, 0, LJ_IN_ABSOLUTE, 0, 0, 255, 0 };

void lj_patch_init(lj_patch_ch patch[LJ_CHANS]) { int i; for (i = 0; i < LJ_CHANS; i++) patch[i] = lj_patch_dflt; }

int lj_patch_is_default(const lj_patch_ch *p) { return memcmp(p, &lj_patch_dflt, sizeof *p) == 0; }

int lj_patch_load(const char *path, lj_patch_ch patch[LJ_CHANS], char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "patch", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 patch file"); return -1; }
    lj_patch_init(patch);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "ch") == 0) {
            long link = strtol(v, &e, 10), addr;
            lj_patch_ch *p;
            if (e == v || *e != ':') { bad = "ch needs link:addr"; break; }
            addr = lj_kv_range_lead(e + 1, 0, 1, LJ_LINK_CH, &rbad);
            if (rbad || link < 1 || link > LJ_LINKS) { bad = "link:addr out of range"; break; }
            p = &patch[lj_addr_code((int)link, (int)addr)];
            p->src_kind = (uint8_t)lj_kv_word(v, "src", lj_patch_src_words, LJ_SRC_NONE);
            p->src_link = (uint8_t)lj_kv_range(v, "sl", 0, 0, LJ_LINKS, &rbad);
            p->src_ch = (uint16_t)lj_kv_range(v, "sc", 0, 0, LJ_LINK_CH, &rbad);
            p->in_mode = (uint8_t)lj_kv_word(v, "mode", lj_in_mode_words, LJ_IN_ABSOLUTE);
            p->invert = (uint8_t)lj_kv_range(v, "inv", 0, 0, 1, &rbad);
            p->min = (uint8_t)lj_kv_range(v, "min", 0, 0, 255, &rbad);
            p->max = (uint8_t)lj_kv_range(v, "max", 255, 0, 255, &rbad);
            p->htp_mask = (uint32_t)lj_kv_range(v, "htp", 0, 0, LJ_HTP_MASK_MAX, &rbad);
            if (rbad) { bad = "number out of range"; break; }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_patch_validate(patch);
    if (bad) { lj_kv_err(err, cap, path, item, bad); lj_patch_init(patch); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_patch_save(const char *path, const lj_patch_ch patch[LJ_CHANS])
{
    int i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "patch", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < LJ_CHANS; i++) {
        const lj_patch_ch *p = &patch[i];
        if (lj_patch_is_default(p)) continue;
        ok = lj_rec_write(rec, "ch ", "%d:%d src=%s sl=%d sc=%u mode=%s inv=%d min=%d max=%d htp=%u", i / LJ_LINK_CH + 1, i % LJ_LINK_CH + 1,
                          lj_word(lj_patch_src_words, p->src_kind), p->src_link, p->src_ch, lj_word(lj_in_mode_words, p->in_mode),
                          p->invert, p->min, p->max, (unsigned)p->htp_mask) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_patch_validate(const lj_patch_ch patch[LJ_CHANS])
{
    int i;
    for (i = 0; i < LJ_CHANS; i++) {
        const lj_patch_ch *p = &patch[i];
        if (p->src_kind > LJ_SRC_DMX_IN) return "source kind unknown";
        if (p->in_mode > LJ_IN_LT255) return "DMX-in mode unknown";
        if (p->min > p->max) return "min over max";
        if (p->invert > 1) return "invert flag not 0/1";
        if (p->htp_mask > (uint32_t)LJ_HTP_MASK_MAX) return "HTP group mask over 24 groups";
        if (p->src_kind != LJ_SRC_NONE) {
            if (p->src_link < 1 || p->src_link > LJ_LINKS || p->src_ch < 1 || p->src_ch > LJ_LINK_CH) return "source link:channel out of range";
            if (p->src_kind == LJ_SRC_DMX_OUT && lj_addr_code(p->src_link, p->src_ch) == i) return "channel patched from itself";
        }
    }
    return NULL;
}

int lj_patch_clear(lj_patch_ch patch[LJ_CHANS]) { lj_patch_init(patch); return 0; }

int lj_patch_htp_set(lj_patch_ch patch[LJ_CHANS], int ch, int group, int on)
{
    uint32_t bit;
    if (ch < 0 || ch >= LJ_CHANS || group < 1 || group > LJ_HTP_GROUPS) return -1;
    bit = (uint32_t)1 << (group - 1);
    if (on) patch[ch].htp_mask |= bit; else patch[ch].htp_mask &= ~bit;
    return 0;
}
