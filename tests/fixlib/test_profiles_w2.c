/* test_profiles_w2.c - the W2 hand profiles 15/02-15/05 (design 6.1 item 2, 10 W2 acceptance 5; ME-25, ME-26, ME-27):
 * every id resolves through lj_fixlib_by_id to a hand profile whose identity is the exe row and whose footprint is the
 * note's, every window row stays below the footprint, Lamp rows and Fixture Reset never reach Fade, the MAC 2000 Profile
 * 16-bit pan pair is tagged, the Roboscan 812 gobo palette has the manual's 13 positions. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fixlib/lj_profile.h"
#include "controls/lj_ctl_common.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define FADE_BIT (1u << 2)   /* 1 << LJ_FADE */

static const struct { uint16_t id; uint8_t footprint; } lj_w2_ids[] = {   /* the footprints of tools/check_profiles.py TIERS 2-5 */
    { 11, 14 }, { 67, 15 },
    { 28, 13 }, { 47, 13 }, { 39, 13 }, { 105, 14 }, { 112, 15 }, { 133, 13 }, { 48, 13 }, { 118, 18 },
    { 68, 24 }, { 84, 31 }, { 87, 21 }, { 88, 21 }, { 117, 21 }, { 161, 21 }, { 162, 21 }, { 188, 15 }, { 189, 15 }, { 190, 15 }, { 191, 15 },
    { 3, 9 }, { 58, 9 }, { 1, 5 }, { 52, 7 }, { 2, 8 }, { 57, 8 }, { 41, 5 }, { 36, 5 }, { 33, 14 } };

static int pal_used(const lj_palette *p)
{
    int i, n = 0;
    for (i = 0; i < LJ_PAL_ENTRIES; i++) n += p->e[i].used ? 1 : 0;
    return n;
}

static void check_profile(const lj_profile *p)
{
    int w, k, i, e;
    for (i = 0; i < p->footprint; i++) {
        CHECK(p->channel_class[i] > LJ_CH_NONE && p->channel_class[i] < LJ_CH_COUNT);
        CHECK(p->channel_class[i] != LJ_CH_SWITCH && p->channel_class[i] != LJ_CH_SMOKE && p->channel_class[i] != LJ_CH_HAZE && p->channel_class[i] != LJ_CH_HAZE_FAN);   /* ME-25: no pack / smoke channel in these fixtures */
        if (p->pair_lsb[i]) CHECK(p->pair_lsb[i] - 1 < p->footprint && p->pair_lsb[i] - 1 != i);
    }
    for (i = p->footprint; i < LJ_PROFILE_CH; i++) CHECK(p->channel_class[i] == LJ_CH_NONE);
    if (p->pt16 || p->pt8) {
        CHECK(p->channel_class[p->pan_ofs] == LJ_CH_PAN && p->channel_class[p->tilt_ofs] == LJ_CH_TILT);
        if (p->pt16) CHECK(p->channel_class[p->pan_ofs + 1] == LJ_CH_PAN && p->channel_class[p->tilt_ofs + 1] == LJ_CH_TILT);
        CHECK(lj_profile_window(p, LJ_CTL_MOVEMENT) != NULL);
    }
    CHECK(p->nwindows > 0 && p->windows && p->notes && p->notes[0]);
    for (w = 0; w < p->nwindows; w++) {
        const lj_ctl_window *win = &p->windows[w];
        CHECK(win->caption && win->nrows > 0);
        for (k = 0; k < 2; k++) {
            const lj_ctl_row *rows = k ? win->specials : win->rows;
            int n = k ? win->nspecials : win->nrows;
            for (i = 0; i < n; i++) {
                const lj_ctl_row *r = &rows[i];
                if (r->ch >= p->footprint) { fails++; printf("FAIL id %d window %d row %d: ch %d >= footprint %d\n", p->id, win->kind, i, r->ch, p->footprint); }
                CHECK(r->states_allowed != 0);
                for (e = 0; e < r->nbuttons; e++) CHECK(r->buttons[e].ch < p->footprint && r->buttons[e].caption);
                if (win->kind == LJ_CTL_LAMP && (r->states_allowed & FADE_BIT)) { fails++; printf("FAIL id %d: Lamp row %d allows Fade\n", p->id, i); }   /* ME-27 */
                if (r->kind == LJ_W_SPECIAL_GROUP && r->caption && strcmp(r->caption, "Fixture Reset") == 0) CHECK(!(r->states_allowed & FADE_BIT));
                if (r->kind == LJ_W_PALETTE) CHECK(r->lo < LJ_PALETTES && pal_used(&p->palette[r->lo]) > 0);
                if (r->kind == LJ_W_RADIOGROUP_FADER) { CHECK(r->items && r->nbuttons > 0); for (e = 0; e < r->nbuttons; e++) CHECK(r->items[e] != NULL); }
                if (r->kind == LJ_W_ROTATION || r->kind == LJ_W_WHEELSCROLL) CHECK(r->nbuttons == 3);   /* {CW}, {CCW}, {Stop} (LORD_FIXTURES-10) */
            }
        }
    }
}

int lj_test_profiles_w2(int argc, char **argv);
int lj_test_profiles_w2(int argc, char **argv)
{
    const lj_profile *p;
    const lj_exe_row *row;
    int i;
    (void)argc; (void)argv;

    for (i = 0; i < (int)(sizeof lj_w2_ids / sizeof lj_w2_ids[0]); i++) {
        p = lj_fixlib_by_id(lj_w2_ids[i].id);
        row = lj_fixlib_row(lj_w2_ids[i].id);
        if (!p || !row) { fails++; printf("FAIL id %d: no profile\n", lj_w2_ids[i].id); continue; }
        CHECK(p->id == lj_w2_ids[i].id && p->gs2_raw == NULL);
        if (p->footprint != lj_w2_ids[i].footprint) { fails++; printf("FAIL id %d: footprint %d, want %d\n", p->id, p->footprint, lj_w2_ids[i].footprint); }
        CHECK(row->binding == LJ_BIND_HAND);                                       /* ME-26 / ME-76: the hand file wins the id */
        CHECK(strcmp(row->name, p->name) == 0 && strcmp(row->mode, p->mode) == 0 && strcmp(row->tag, p->tag) == 0 && strcmp(row->manufacturer, p->manufacturer) == 0);
        check_profile(p);
    }

    /* MAC 2000 Profile: 16-bit pan pair on ch 19/20, tilt 21/22; gobo index pairs 9/10 and 12/13 */
    p = lj_fixlib_by_id(68);
    CHECK(p && p->pt16 == 1 && p->pan_ofs == 18 && p->tilt_ofs == 20);
    CHECK(p && p->channel_class[18] == LJ_CH_PAN && p->channel_class[19] == LJ_CH_PAN && p->channel_class[20] == LJ_CH_TILT && p->channel_class[21] == LJ_CH_TILT);
    CHECK(p && p->pair_lsb[8] == 1 + 9 && p->pair_lsb[11] == 1 + 12);
    CHECK(p && p->nwindows == 8 && lj_profile_window(p, LJ_CTL_GOBOS) && lj_profile_window(p, LJ_CTL_EFFECTS) && !lj_profile_window(p, LJ_CTL_LEVELS));
    CHECK(p && pal_used(&p->palette[2]) == 6 && pal_used(&p->palette[3]) == 6 && p->palette[2].type[1] == 6);

    /* Roboscan 812: "11 gobos on the gobo wheel plus open and blackout" (UM-812 Rev C) = 13 slots; 13 colour positions; no intensity, gobo blackout */
    for (i = 0; i < 2; i++) {
        p = lj_fixlib_by_id(i ? 52 : 1);
        CHECK(p && pal_used(&p->palette[2]) == 13 && pal_used(&p->palette[0]) == 13);
        CHECK(p && !lj_profile_window(p, LJ_CTL_INTENSITY) && p->pt8 && p->blackout.defined[2] && p->blackout.value[2] == 255 && p->bo_at_gm0);
        CHECK(p && lj_profile_window(p, LJ_CTL_LAMP) && lj_profile_window(p, LJ_CTL_LAMP)->rows[1].nbuttons == 1);   /* TLampControl_DO: Strike Lamp only */
    }

    /* MAC 600: no Gobos / Effects window; CMY on ch 3-5 */
    p = lj_fixlib_by_id(11);
    CHECK(p && !lj_profile_window(p, LJ_CTL_GOBOS) && !lj_profile_window(p, LJ_CTL_EFFECTS) && p->cmy_on && p->cmy[0].offset == 2 && p->channel_class[0] == LJ_CH_SHUTTER);
    /* MAC 2000 Wash Studio: the secondary intensity defaults to 255 (NOTES); Krypton 16-bit gobo index pair */
    CHECK(lj_fixlib_by_id(117) && lj_fixlib_by_id(117)->default_dmx[1] == 255 && lj_fixlib_by_id(117)->channel_class[7] == LJ_CH_INTENSITY);
    CHECK(lj_fixlib_by_id(105) && lj_fixlib_by_id(105)->pair_lsb[4] == 1 + 5);
    /* Roboscan 518: strobe and lamp share ch 1 (one state); DOOF lamp window has Fan Lo */
    p = lj_fixlib_by_id(3);
    CHECK(p && p->channel_class[0] == LJ_CH_STROBE && lj_profile_window(p, LJ_CTL_LAMP) && lj_profile_window(p, LJ_CTL_LAMP)->rows[1].nbuttons == 3);
    CHECK(p && pal_used(&p->palette[0]) == 18 && pal_used(&p->palette[4]) == 4);

    /* every window of every W2 profile builds headless through the controls renderer (Movement is the controls-b window, opened from the master) */
    {
        lj_ctx *ctx = lj_ctx_get();
        lj_show *show = (lj_show *)calloc(1, sizeof *show);
        static lj_sequence seq;
        static lj_scene scene;
        int w;
        if (!show) lj_fatal("test_profiles_w2: out of memory");
        memset(&seq, 0, sizeof seq); memset(&scene, 0, sizeof scene);
        seq.scenes = &scene; seq.n = 1;
        show->current_seq = &seq; show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].link = 1; show->fixture[0].address = 1;
        ctx->show = show; ctx->current_scene = 0;
        for (i = 0; i < (int)(sizeof lj_w2_ids / sizeof lj_w2_ids[0]); i++) {
            p = lj_fixlib_by_id(lj_w2_ids[i].id);
            if (!p) continue;
            show->fixture[0].profile_id = p->id;
            for (w = 0; w < p->nwindows; w++) {
                if (p->windows[w].kind == LJ_CTL_MOVEMENT) continue;
                if (!lj_ctl_open_table(p->windows[w].kind, &p->windows[w], p)) { fails++; printf("FAIL id %d: window %s does not build\n", p->id, p->windows[w].caption); }
                lj_ctl_close_all();
            }
        }
        ctx->show = NULL;
        free(show);
    }

    printf("profiles_w2: %d ids, %d failure(s)\n", (int)(sizeof lj_w2_ids / sizeof lj_w2_ids[0]), fails);
    return fails ? 1 : 0;
}
