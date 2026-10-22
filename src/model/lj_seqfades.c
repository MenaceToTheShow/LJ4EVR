/* lj_seqfades.c - Sequence fades (lj_seqfades.h; 04 s1.4, CHM sequencefades.htm): fade states only, never values. */
#include <string.h>
#include "model/lj_seqfades.h"
#include "model/lj_show.h"

const char *const lj_attr_caption[LJ_ATTR_COUNT] = { "Intensity", "Movement", "Color", "Gobo", "Beam", "Effects", "Level/Extended", "Movement Macro" };
const char *const lj_sfx_caption[LJ_SFX_COUNT] = { "All Off", "All Snap", "All Fade", "Off -> Snap", "Off -> Fade", "Snap -> Off", "Snap -> Fade", "Fade -> Off", "Fade -> Snap" };

/* built-ins: the control the channel class has its main function on; shutter / strobe / reset sit with intensity (CHM
 * "shutter commands are typical intensity commands"); lamp, speed, special and the pack classes are Level/Extended (PROPOSAL rig 04 s1.4) */
static const int8_t lj_class_attr[LJ_CH_COUNT] = {
    [LJ_CH_NONE] = -1, [LJ_CH_INTENSITY] = LJ_ATTR_INTENSITY, [LJ_CH_SHUTTER] = LJ_ATTR_INTENSITY, [LJ_CH_STROBE] = LJ_ATTR_INTENSITY,
    [LJ_CH_PAN] = LJ_ATTR_MOVEMENT, [LJ_CH_TILT] = LJ_ATTR_MOVEMENT, [LJ_CH_COLOR] = LJ_ATTR_COLOR, [LJ_CH_GOBO] = LJ_ATTR_GOBO,
    [LJ_CH_BEAM] = LJ_ATTR_BEAM, [LJ_CH_EFFECT] = LJ_ATTR_EFFECTS, [LJ_CH_LEVEL] = LJ_ATTR_LEVEL, [LJ_CH_SPEED] = LJ_ATTR_LEVEL,
    [LJ_CH_LAMP] = LJ_ATTR_LEVEL, [LJ_CH_SPECIAL] = LJ_ATTR_LEVEL, [LJ_CH_SWITCH] = LJ_ATTR_LEVEL, [LJ_CH_SMOKE] = LJ_ATTR_LEVEL,
    [LJ_CH_HAZE] = LJ_ATTR_LEVEL, [LJ_CH_HAZE_FAN] = LJ_ATTR_LEVEL };

static void mark(int8_t attr[LJ_PROFILE_CH], const lj_dyn *d, int n, int a) { int i; for (i = 0; i < n; i++) if (d[i].enabled && d[i].offset < LJ_PROFILE_CH) attr[d[i].offset] = (int8_t)a; }

/* ME-22: a 16-bit pair carries one state, so its LSB takes the MSB's attribute (pan / tilt pairs, pair_lsb[]) */
static int pair_msb(const lj_profile *p, int o)
{
    int k;
    for (k = 0; k < LJ_PROFILE_CH; k++) if (p->pair_lsb[k] && p->pair_lsb[k] - 1 == o) return k;
    if (p->pt_patch_on) return o == p->pt_patch[1] ? p->pt_patch[0] : o == p->pt_patch[3] ? p->pt_patch[2] : -1;
    if (p->pt16) return o == p->pan_ofs + 1 ? p->pan_ofs : o == p->tilt_ofs + 1 ? p->tilt_ofs : -1;
    return -1;
}

int lj_ch_attr(const lj_profile *p, int offset)
{
    int8_t attr[LJ_PROFILE_CH];
    int o;
    if (!p || offset < 0 || offset >= LJ_PROFILE_CH || offset >= p->footprint) return -1;
    if ((o = pair_msb(p, offset)) >= 0 && o != offset && pair_msb(p, o) < 0) return lj_ch_attr(p, o);
    if (!p->gs2_raw) return p->channel_class[offset] < LJ_CH_COUNT ? lj_class_attr[p->channel_class[offset]] : -1;
    /* user profiles: "Movement/position, beam, effects, level (extended), intensity, gobo, color" - last has priority */
    memset(attr, -1, sizeof attr);
    for (o = 0; o < LJ_PROFILE_CH; o++) if (p->channel_class[o] == LJ_CH_PAN || p->channel_class[o] == LJ_CH_TILT) attr[o] = LJ_ATTR_MOVEMENT;
    if (p->pt_patch_on) { for (o = 0; o < 4; o++) if (p->pt_patch[o] < LJ_PROFILE_CH) attr[p->pt_patch[o]] = LJ_ATTR_MOVEMENT; }
    else if (p->pt16 || p->pt8) {
        if (p->pan_ofs < LJ_PROFILE_CH) attr[p->pan_ofs] = LJ_ATTR_MOVEMENT;
        if (p->tilt_ofs < LJ_PROFILE_CH) attr[p->tilt_ofs] = LJ_ATTR_MOVEMENT;
        if (p->pt16 && p->pan_ofs + 1 < LJ_PROFILE_CH) attr[p->pan_ofs + 1] = LJ_ATTR_MOVEMENT;     /* 16 s4.4: the pair is (ofs, ofs+1) */
        if (p->pt16 && p->tilt_ofs + 1 < LJ_PROFILE_CH) attr[p->tilt_ofs + 1] = LJ_ATTR_MOVEMENT;
    }
    mark(attr, &p->misc_move, 1, LJ_ATTR_MOVEMENT);
    mark(attr, &p->iris, 1, LJ_ATTR_BEAM); mark(attr, &p->focus, 1, LJ_ATTR_BEAM); mark(attr, &p->zoom, 1, LJ_ATTR_BEAM);
    mark(attr, &p->frost, 1, LJ_ATTR_BEAM); mark(attr, p->beam, 2, LJ_ATTR_BEAM);
    mark(attr, p->effects, 4, LJ_ATTR_EFFECTS);
    for (o = 0; o < p->footprint && o < LJ_PROFILE_CH; o++)          /* the Levels control's visible faders; a named function's channel is hidden there */
        if (attr[o] < 0 && !p->level_invisible[o]) attr[o] = LJ_ATTR_LEVEL;   /* by default (04 s8.3), so level never takes a channel an earlier rule claimed */
    mark(attr, &p->shutter, 1, LJ_ATTR_INTENSITY); mark(attr, &p->strobe, 1, LJ_ATTR_INTENSITY); mark(attr, &p->intensity, 1, LJ_ATTR_INTENSITY);
    mark(attr, p->gobo, 4, LJ_ATTR_GOBO);
    mark(attr, p->color, 4, LJ_ATTR_COLOR); mark(attr, p->cmy, 3, LJ_ATTR_COLOR);
    return attr[offset];
}

static int sfx_state(int sfx, int st)    /* the new state, or st when the transform leaves it */
{
    switch (sfx) {
    case LJ_SFX_ALL_OFF: return LJ_OFF;
    case LJ_SFX_ALL_SNAP: return LJ_SNAP;
    case LJ_SFX_ALL_FADE: return LJ_FADE;
    case LJ_SFX_OFF_SNAP: return st == LJ_OFF ? LJ_SNAP : st;
    case LJ_SFX_OFF_FADE: return st == LJ_OFF ? LJ_FADE : st;
    case LJ_SFX_SNAP_OFF: return st == LJ_SNAP ? LJ_OFF : st;
    case LJ_SFX_SNAP_FADE: return st == LJ_SNAP ? LJ_FADE : st;
    case LJ_SFX_FADE_OFF: return st == LJ_FADE ? LJ_OFF : st;
    case LJ_SFX_FADE_SNAP: return st == LJ_FADE ? LJ_SNAP : st;
    default: return st;
    }
}

static int xform(uint8_t *st, int sfx) { int n = sfx_state(sfx, *st); if (n == *st) return 0; *st = (uint8_t)n; return 1; }

int lj_seqfades_apply(const lj_seqfades *f, const lj_show *show, lj_sequence *seq, int scene, const uint8_t selected[LJ_FIXTURES])
{
    int i, o, s, first, last, n = 0;
    if (!f || !show || !seq || !seq->scenes || !selected || f->sfx >= LJ_SFX_COUNT) return -1;
    if (!f->all_scenes && (scene < 0 || scene >= seq->n)) return -1;
    first = f->all_scenes ? 0 : scene; last = f->all_scenes ? seq->n - 1 : scene;
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_profile *p;
        int8_t attr[LJ_PROFILE_CH];
        int base;
        if (!selected[i] || (base = lj_show_fixture_base(show, i)) < 0 || (p = lj_show_profile(show, i)) == NULL) continue;
        for (o = 0; o < LJ_PROFILE_CH; o++) attr[o] = (int8_t)lj_ch_attr(p, o);
        for (s = first; s <= last; s++) {
            lj_scene *sc = &seq->scenes[s];
            for (o = 0; o < p->footprint && o < LJ_PROFILE_CH && base + o < LJ_CHANS; o++)
                if (attr[o] >= 0 && (f->attr_mask >> attr[o] & 1)) n += xform(&sc->state[base + o], f->sfx);
            if (f->attr_mask >> LJ_ATTR_MOVEMACRO & 1)                          /* Movement Macro: the macro's own O/S/F rows */
                for (o = 0; o < 6; o++) n += xform(&sc->macro[i].fstate[o], f->sfx);
        }
    }
    return n;
}
