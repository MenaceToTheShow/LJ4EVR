/* test_profiles_w3.c - the W3 hand profiles 15/06-15/09 (design 6.1 item 2, 10 W3 acceptance 8; ME-8, ME-25, ME-27, F47):
 * every id resolves through lj_fixlib_by_id to the hand profile of its exe row with the note's footprint, every window row stays
 * below the footprint, Lamp rows and Off/Snap specials never reach Fade, no pack / smoke / haze class (none of these fixtures has
 * one: ME-25 "where they apply"), the Atomic 3000 intensity / strobe classes, the MAC 550 16-bit pan / tilt pairs, the RoboColor
 * palette counts of the manual, and every window builds headless through the controls renderer. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fixlib/lj_profile.h"
#include "controls/lj_ctl_common.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define FADE_BIT (1u << 2)   /* 1 << LJ_FADE */

static const struct { uint16_t id; uint8_t footprint; } lj_w3_ids[] = {   /* the footprints of tools/check_profiles.py TIERS 6-9 */
    { 80, 4 }, { 114, 6 }, { 115, 2 },
    { 54, 10 }, { 65, 10 }, { 53, 8 }, { 66, 8 }, { 69, 11 },
    { 13, 5 }, { 59, 6 }, { 14, 5 }, { 60, 6 }, { 15, 5 }, { 61, 7 }, { 37, 6 }, { 40, 6 },
    { 110, 21 }, { 155, 19 }, { 116, 23 }, { 140, 16 } };

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
        CHECK(p->channel_class[i] > LJ_CH_NONE && p->channel_class[i] < LJ_CH_SWITCH);   /* ME-25: no SWITCH / SMOKE / HAZE / HAZE_FAN here */
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
                for (e = 0; e < r->nbuttons; e++) {
                    CHECK(r->buttons[e].ch < p->footprint && r->buttons[e].caption);
                    if (r->kind != LJ_W_ROTATION && r->kind != LJ_W_WHEELSCROLL) CHECK(r->buttons[e].lo <= r->buttons[e].value && r->buttons[e].value <= r->buttons[e].hi);   /* F47 */
                }
                if ((win->kind == LJ_CTL_LAMP || r->kind == LJ_W_LAMP_BTNS) && (r->states_allowed & FADE_BIT)) { fails++; printf("FAIL id %d: Lamp row %d allows Fade\n", p->id, i); }   /* ME-27 */
                if (r->kind == LJ_W_SPECIAL_GROUP && r->caption && (strcmp(r->caption, "Fixture Reset") == 0 || strcmp(r->caption, "Fixture Stand Alone") == 0)) CHECK(!(r->states_allowed & FADE_BIT));
                if (r->kind == LJ_W_PALETTE) CHECK(r->lo < LJ_PALETTES && pal_used(&p->palette[r->lo]) > 0);
                if (r->kind == LJ_W_RADIOGROUP_FADER) { CHECK(r->items && r->nbuttons > 0); for (e = 0; e < r->nbuttons; e++) CHECK(r->items[e] != NULL); }
                if (r->kind == LJ_W_ROTATION || r->kind == LJ_W_WHEELSCROLL) CHECK(r->nbuttons == 3);   /* { CW, CCW, Stop } (LORD_FIXTURES-10) */
            }
        }
    }
}

int lj_test_profiles_w3(int argc, char **argv);
int lj_test_profiles_w3(int argc, char **argv)
{
    const lj_profile *p;
    const lj_exe_row *row;
    int i, h;
    (void)argc; (void)argv;

    for (i = 0; i < (int)(sizeof lj_w3_ids / sizeof lj_w3_ids[0]); i++) {
        p = lj_fixlib_by_id(lj_w3_ids[i].id);
        row = lj_fixlib_row(lj_w3_ids[i].id);
        if (!p || !row) { fails++; printf("FAIL id %d: no profile\n", lj_w3_ids[i].id); continue; }
        CHECK(p->id == lj_w3_ids[i].id && p->gs2_raw == NULL);
        if (p->footprint != lj_w3_ids[i].footprint) { fails++; printf("FAIL id %d: footprint %d, want %d\n", p->id, p->footprint, lj_w3_ids[i].footprint); }
        CHECK(row->binding == LJ_BIND_HAND);                                       /* ME-8: the hand file wins the id, the qxf row stays mapped */
        CHECK(strcmp(row->name, p->name) == 0 && strcmp(row->mode, p->mode) == 0 && strcmp(row->tag, p->tag) == 0 && strcmp(row->manufacturer, p->manufacturer) == 0);
        check_profile(p);
    }

    /* Atomic 3000: ch 1 intensity (Grand Master), ch 3 the rate = strobe class and dyn, effects on ch 4; no Lamp / Movement window */
    p = lj_fixlib_by_id(80);
    CHECK(p && p->channel_class[0] == LJ_CH_INTENSITY && p->channel_class[2] == LJ_CH_STROBE && p->channel_class[3] == LJ_CH_EFFECT && p->use_gm);
    CHECK(p && p->intensity.enabled && p->intensity.offset == 0 && p->strobe.enabled && p->strobe.offset == 2 && p->strobe_off == 0);
    CHECK(p && !lj_profile_window(p, LJ_CTL_LAMP) && !lj_profile_window(p, LJ_CTL_MOVEMENT) && lj_profile_window(p, LJ_CTL_INTENSITY)->nspecials == 1);
    CHECK(p && lj_profile_window(p, LJ_CTL_INTENSITY)->specials[0].nbuttons == 6);
    p = lj_fixlib_by_id(114);                                                      /* scroller at address + 4: colour ch 5, gel string of 11 frames */
    CHECK(p && p->channel_class[4] == LJ_CH_COLOR && p->color[0].offset == 4 && pal_used(&p->palette[0]) == 11 && lj_profile_window(p, LJ_CTL_EXTENDED));

    /* MAC 550: pan MSB/LSB on ch 16/17, tilt 18/19 (offsets 15-18); the rotating-gobo index pair 6/7 */
    p = lj_fixlib_by_id(110);
    CHECK(p && p->pt16 == 1 && !p->pt8 && p->pan_ofs == 15 && p->tilt_ofs == 17);
    CHECK(p && p->channel_class[15] == LJ_CH_PAN && p->channel_class[16] == LJ_CH_PAN && p->channel_class[17] == LJ_CH_TILT && p->channel_class[18] == LJ_CH_TILT);
    CHECK(p && p->pair_lsb[5] == 1 + 6 && p->nwindows == 8);
    CHECK(p && pal_used(&p->palette[0]) == 9 && pal_used(&p->palette[1]) == 9 && pal_used(&p->palette[2]) == 7 && pal_used(&p->palette[3]) == 10 && p->palette[2].type[1] == 6);
    p = lj_fixlib_by_id(116);
    CHECK(p && p->pt16 && p->pan_ofs == 17 && p->channel_class[18] == LJ_CH_PAN && p->channel_class[20] == LJ_CH_TILT && p->cmy_on && p->pair_lsb[7] == 1 + 8);
    CHECK(p && lj_profile_window(p, LJ_CTL_LAMP) && lj_profile_window(p, LJ_CTL_LAMP)->rows[1].nbuttons == 4);   /* 400W / 700W */
    p = lj_fixlib_by_id(140);
    CHECK(p && p->pan_ofs == 10 && p->channel_class[10] == LJ_CH_PAN && p->channel_class[13] == LJ_CH_TILT && !lj_profile_window(p, LJ_CTL_GOBOS));

    /* RoboColor III: "11 dichroic colors plus white" + blackout = 13 snaps per head, four head rows on palette 0, blackout on the wheels */
    for (i = 0; i < 2; i++) {
        const lj_ctl_window *cw;
        int npal = 0;
        p = lj_fixlib_by_id(i ? 60 : 14);
        cw = p ? lj_profile_window(p, LJ_CTL_COLORS) : NULL;
        CHECK(p && pal_used(&p->palette[0]) == 13 && p->bo_at_gm0 && p->blackout.value[1] == 240 && p->blackout.value[4] == 240);
        CHECK(p && !lj_profile_window(p, LJ_CTL_INTENSITY) && !lj_profile_window(p, LJ_CTL_MOVEMENT));
        for (h = 0; cw && h < cw->nrows; h++) if (cw->rows[h].kind == LJ_W_PALETTE) { npal++; CHECK(cw->rows[h].lo == 0); }
        CHECK(npal == 4);
    }
    /* RoboColor Pro 400: 7 fixed positions on each colour wheel, 5 beam positions; lamp off only in mode 3 */
    p = lj_fixlib_by_id(15);
    CHECK(p && pal_used(&p->palette[0]) == 7 && pal_used(&p->palette[1]) == 7 && pal_used(&p->palette[2]) == 5 && p->lamp_off.enabled);
    CHECK(lj_fixlib_by_id(61) && !lj_fixlib_by_id(61)->lamp_off.enabled && lj_profile_window(lj_fixlib_by_id(61), LJ_CTL_LAMP)->rows[1].nbuttons == 2);

    /* MiniMAC: shutter-only personality vs Intensity Mode (ch 1 0-38 under a range-limited Grand Master) */
    p = lj_fixlib_by_id(54);
    CHECK(p && p->channel_class[0] == LJ_CH_SHUTTER && p->bo_at_gm0 && !p->intensity.enabled && pal_used(&p->palette[0]) == 13 && pal_used(&p->palette[2]) == 8);
    p = lj_fixlib_by_id(65);
    CHECK(p && p->channel_class[0] == LJ_CH_INTENSITY && p->use_gm && p->limit_gm && p->intensity.max == 38);
    CHECK(lj_fixlib_by_id(53) && !lj_profile_window(lj_fixlib_by_id(53), LJ_CTL_GOBOS) && lj_fixlib_by_id(53)->pan_ofs == 2);
    CHECK(lj_fixlib_by_id(69) && lj_profile_window(lj_fixlib_by_id(69), LJ_CTL_BEAM) && lj_fixlib_by_id(69)->pair_lsb[2] == 1 + 3);

    /* every window of every W3 profile builds headless (Movement is the controls-b window, opened from the master) */
    {
        lj_ctx *ctx = lj_ctx_get();
        lj_show *show = (lj_show *)calloc(1, sizeof *show);
        static lj_sequence seq;
        static lj_scene scene;
        int w;
        if (!show) lj_fatal("test_profiles_w3: out of memory");
        memset(&seq, 0, sizeof seq); memset(&scene, 0, sizeof scene);
        seq.scenes = &scene; seq.n = 1;
        show->current_seq = &seq; show->fixture[0].kind = LJ_FX_BUILTIN; show->fixture[0].link = 1; show->fixture[0].address = 1;
        ctx->show = show; ctx->current_scene = 0;
        for (i = 0; i < (int)(sizeof lj_w3_ids / sizeof lj_w3_ids[0]); i++) {
            p = lj_fixlib_by_id(lj_w3_ids[i].id);
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

    printf("profiles_w3: %d ids, %d failure(s)\n", (int)(sizeof lj_w3_ids / sizeof lj_w3_ids[0]), fails);
    return fails ? 1 : 0;
}
