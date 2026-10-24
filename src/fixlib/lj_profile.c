/* lj_profile.c - the fixlib API (design 2.4, 6.1): the 201 exe rows as identity, hand
 * profiles (tier 1) over the tier-2 conversions (QLC+, Open Fixture Library, LJ4EVR-authored;
 * built on the heap at first use, W7) over nothing (greyed). lj_profile_window is the W1
 * contract body kept as written. */
#include "fixlib/lj_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixlib/lj_exe_profiles.h"
#include "fixlib/lj_profiles_gen.h"
#include "fixlib/lj_layout.h"
#include "fixlib/hand/lj_mac500.h"
#include "fixlib/hand/lj_rs918.h"
#include "fixlib/hand/lj_generics.h"
#include "fixlib/hand/lj_mac600.h"
#include "fixlib/hand/lj_mac250.h"
#include "fixlib/hand/lj_mac2000.h"
#include "fixlib/hand/lj_roboscan.h"
#include "fixlib/hand/lj_atomic.h"
#include "fixlib/hand/lj_minimac.h"
#include "fixlib/hand/lj_robocolor.h"
#include "fixlib/hand/lj_mac550.h"

/* W2 / W3 tier-1 families 15/02-15/09: one array per hand file. */
static const struct { const lj_profile *const *p; int n; } lj_hand_w2[] = {
    { lj_hand_mac600s, LJ_MAC600_COUNT }, { lj_hand_mac250s, LJ_MAC250_COUNT }, { lj_hand_mac2000s, LJ_MAC2000_COUNT }, { lj_hand_roboscans, LJ_ROBOSCAN_COUNT },
    { lj_hand_atomics, LJ_ATOMIC_COUNT }, { lj_hand_minimacs, LJ_MINIMAC_COUNT }, { lj_hand_robocolors, LJ_ROBOCOLOR_COUNT }, { lj_hand_mac550s, LJ_MAC550_COUNT } };

static const lj_profile *lj_hand_by_id(uint16_t id)
{
    int i, k;
    for (k = 0; k < (int)(sizeof lj_hand_w2 / sizeof lj_hand_w2[0]); k++)
        for (i = 0; i < lj_hand_w2[k].n; i++) if (lj_hand_w2[k].p[i]->id == id) return lj_hand_w2[k].p[i];
    if (id == 12) return &lj_hand_mac500;
    if (id == 31) return &lj_hand_mac500_scrl;
    if (id == 24) return &lj_hand_rs918;
    if (id == 30) return &lj_hand_rs918_scrl;
    for (i = 0; i < LJ_GENERICS_COUNT; i++) if (lj_hand_generics[i]->id == id) return lj_hand_generics[i];
    return NULL;
}

/* lj_qxf_build: one tools/qxf2c.py byte string -> a heap lj_profile (UI thread; the engine only reads
 * profiles the UI built, rule 10). The encoding, all bytes:
 *   F                                   footprint 1..32
 *   F x { class flags dflt pair n c[n] } lj_ch_class, 1 invisible | 2 disabled | 4 ext_gm, default_dmx, pair_lsb, level caption (n <= 5)
 *   D, D x { slot ofs lo hi n c[n] }    a dyn record: slot = lj_qxf_dyn order below, name n <= 8
 *   P pan tilt p0 p1 p2 p3              1 pt16 | 2 pt8 | 4 cmy_on | 8 rgb_on | 16 pt_patch_on | 32/64/128 hsv[0..2]; pt_patch
 *   so v                                strobe_off = v when so = 1
 *   S, S x { which ofs v }              0 blackout 1 lamp_on 2 lamp_off 3 reset: enabled, defined[ofs] = 1, value[ofs] = v
 *   N, N x { idx n, n x { v r g b } }   palette idx (0..5): entries used, mask 0x01, v[0] = v, rgb = 0xRRGGBB
 * use_gm = 1; presets / macros / follow_spot = 1 with pt16 or pt8; notes = "Converted from <src> (<mode>). ..." */
typedef struct { lj_profile p; char notes[200]; } lj_qxf_heap;

static lj_profile *lj_qxf_build(const lj_qxf_desc *d)
{
    lj_qxf_heap *h = calloc(1, sizeof *h);
    lj_profile *p;
    const uint8_t *b = lj_qxf_parts[d->part] + d->at;
    int i, n, k;
    if (!h) lj_fatal("lj_fixlib_by_id: out of memory (%u bytes)", (unsigned)sizeof *h);
    p = &h->p;
    p->id = d->id;
    snprintf(p->tag, sizeof p->tag, "%s", d->tag); snprintf(p->manufacturer, sizeof p->manufacturer, "%s", d->manufacturer);
    snprintf(p->name, sizeof p->name, "%s", d->name); snprintf(p->mode, sizeof p->mode, "%s", d->mode);
    snprintf(h->notes, sizeof h->notes, "Converted from %s (%s). Tier 2: control windows derived by the Generic Fixture 2 rules.", d->src, d->src_mode);
    p->notes = h->notes;
    p->footprint = *b++;
    if (p->footprint > LJ_PROFILE_CH) p->footprint = LJ_PROFILE_CH;     /* qxf2c never writes more (the map refuses > 32) */
    for (i = 0; i < p->footprint; i++) {
        p->channel_class[i] = b[0]; p->level_invisible[i] = b[1] & 1; p->level_disabled[i] = (b[1] >> 1) & 1; p->ext_gm[i] = (b[1] >> 2) & 1; p->default_dmx[i] = b[2]; p->pair_lsb[i] = b[3];
        n = b[4] < 5 ? b[4] : 5;
        memcpy(p->level_caption[i], b + 5, (size_t)n);
        b += 5 + b[4];
    }
    for (n = *b++; n > 0; n--) {
        lj_dyn *const slot[] = { &p->shutter, &p->strobe, &p->intensity, &p->color[0], &p->color[1], &p->color[2], &p->color[3],
                                 &p->cmy[0], &p->cmy[1], &p->cmy[2], &p->gobo[0], &p->gobo[1], &p->gobo[2], &p->gobo[3],
                                 &p->iris, &p->focus, &p->zoom, &p->frost, &p->beam[0], &p->beam[1],
                                 &p->effects[0], &p->effects[1], &p->effects[2], &p->effects[3] };
        if ((size_t)b[0] < sizeof slot / sizeof slot[0]) {
            lj_dyn *y = slot[b[0]];
            y->enabled = 1; y->offset = b[1]; y->min = b[2]; y->max = b[3];
            memcpy(y->name, b + 5, (size_t)(b[4] < 8 ? b[4] : 8));
        }
        b += 5 + b[4];
    }
    p->pt16 = b[0] & 1; p->pt8 = (b[0] >> 1) & 1; p->cmy_on = (b[0] >> 2) & 1; p->rgb_on = (b[0] >> 3) & 1; p->pt_patch_on = (b[0] >> 4) & 1;
    for (i = 0; i < 3; i++) p->hsv[i] = (b[0] >> (5 + i)) & 1;
    p->pan_ofs = b[1]; p->tilt_ofs = b[2];
    memcpy(p->pt_patch, b + 3, 4);
    p->use_gm = 1;
    if (p->pt16 || p->pt8) p->presets = p->macros = p->follow_spot = 1;
    b += 7;
    if (b[0]) p->strobe_off = b[1];
    b += 2;
    for (n = *b++; n > 0; n--, b += 3) {
        lj_gs2_sfunc *const sf[] = { &p->blackout, &p->lamp_on, &p->lamp_off, &p->reset };
        if (b[0] < 4 && b[1] < LJ_PROFILE_CH) { sf[b[0]]->enabled = 1; sf[b[0]]->defined[b[1]] = 1; sf[b[0]]->value[b[1]] = b[2]; }
    }
    for (n = *b++; n > 0; n--) {
        int idx = b[0], ne = b[1];
        b += 2;
        for (k = 0; k < ne; k++, b += 4) {
            if (idx >= LJ_PALETTES || k >= LJ_PAL_ENTRIES) continue;
            p->palette[idx].e[k].used = 1; p->palette[idx].e[k].mask = 0x01; p->palette[idx].e[k].v[0] = b[0];
            p->palette[idx].e[k].rgb = ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
        }
    }
    return p;
}

const lj_profile *lj_fixlib_qxf(int i)
{
    lj_profile *p;
    if (i < 0 || i >= lj_qxf_count) return NULL;
    if (!lj_qxf_built[i]) lj_qxf_built[i] = lj_qxf_build(&lj_qxf_descs[i]);
    p = lj_qxf_built[i];
    if (!p->windows && lj_layout_derive(p) != 0) return NULL;   /* tier 2: windows derived once, at first use (6.3) */
    return p;
}

void lj_fixlib_release(void)
{
    int i;
    for (i = 0; i < lj_qxf_count; i++) {
        if (!lj_qxf_built[i]) continue;
        lj_layout_free(lj_qxf_built[i]);
        free(lj_qxf_built[i]);
        lj_qxf_built[i] = NULL;
    }
}

const lj_profile *lj_fixlib_by_id(uint16_t profile_id)
{
    const lj_profile *h = lj_hand_by_id(profile_id);     /* a hand file supersedes the conversion bound to the same id (6.1 item 1) */
    int lo = 0, hi = lj_qxf_count - 1;
    if (h) return h;
    while (lo <= hi) {                                   /* lj_qxf_descs is sorted by id */
        int mid = (lo + hi) / 2;
        if (lj_qxf_descs[mid].id == profile_id) return lj_fixlib_qxf(mid);
        if (lj_qxf_descs[mid].id < profile_id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

int lj_fixlib_count(void) { return LJ_EXE_ROWS + lj_qxf_extra_count; }

const lj_exe_row *lj_fixlib_row(int i)
{
    if (i < 0) return NULL;
    if (i < LJ_EXE_ROWS) return &lj_exe_rows[i];
    i -= LJ_EXE_ROWS;
    return i < lj_qxf_extra_count ? &lj_qxf_extra_rows[i] : NULL;
}

const lj_exe_row *lj_fixlib_row_by_id(int id)
{
    int lo = 0, hi = lj_qxf_extra_count - 1;
    if (id >= 0 && id < LJ_EXE_ROWS) return &lj_exe_rows[id];
    while (lo <= hi) {                                   /* lj_qxf_extra_rows is sorted by id */
        int mid = (lo + hi) / 2;
        if (lj_qxf_extra_rows[mid].id == id) return &lj_qxf_extra_rows[mid];
        if (lj_qxf_extra_rows[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const lj_ctl_window *lj_profile_window(const lj_profile *p, enum lj_ctl_kind kind)
{
    int i;
    if (!p) return NULL;
    for (i = 0; i < p->nwindows; i++) if (p->windows[i].kind == kind) return &p->windows[i];
    return NULL;
}
