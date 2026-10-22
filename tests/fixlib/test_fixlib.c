/* test_fixlib.c - fixlib W1: the 201 exe rows, hand over qxf precedence (id 12), the MAC 500
 * and dimmer window sets, every table's channel offsets below the footprint, palettes
 * within 20 entries, ME-25 classes read back, ME-27 states (Lamp never Fade, SWITCH rows
 * never Fade), and lj_layout_derive on a synthetic .GS2-shaped profile. */
#include <stdio.h>
#include <string.h>
#include "fixlib/lj_profile.h"
#include "fixlib/lj_exe_profiles.h"
#include "fixlib/lj_profiles_gen.h"
#include "fixlib/hand/lj_mac500.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define FADE_BIT (1u << 2)   /* 1 << LJ_FADE */

static void check_tables(const lj_profile *p)
{
    int w, i, k, e;
    for (w = 0; w < p->nwindows; w++) {
        const lj_ctl_window *win = &p->windows[w];
        for (k = 0; k < 2; k++) {
            const lj_ctl_row *rows = k ? win->specials : win->rows;
            int n = k ? win->nspecials : win->nrows;
            for (i = 0; i < n; i++) {
                const lj_ctl_row *r = &rows[i];
                if (r->ch >= p->footprint) { fails++; printf("FAIL id %d window %d row %d: ch %d >= footprint %d\n", p->id, win->kind, i, r->ch, p->footprint); }
                for (e = 0; e < r->nbuttons; e++)
                    if (r->buttons[e].ch >= p->footprint) { fails++; printf("FAIL id %d window %d row %d button %d: ch %d\n", p->id, win->kind, i, e, r->buttons[e].ch); }
                if (win->kind == LJ_CTL_LAMP && (r->states_allowed & FADE_BIT)) { fails++; printf("FAIL id %d: Lamp row %d allows Fade\n", p->id, i); }
                if (r->kind == LJ_W_LEVEL_FADER && p->channel_class[r->ch] == LJ_CH_SWITCH && (r->states_allowed & FADE_BIT)) { fails++; printf("FAIL id %d: SWITCH row %d allows Fade\n", p->id, i); }
                if (r->kind == LJ_W_PALETTE && r->lo >= LJ_PALETTES) { fails++; printf("FAIL id %d: palette row %d index %d\n", p->id, i, r->lo); }
            }
        }
    }
    for (i = 0; i < LJ_PALETTES; i++) {
        int used = 0;
        for (e = 0; e < LJ_PAL_ENTRIES; e++) used += p->palette[i].e[e].used ? 1 : 0;
        CHECK(used <= LJ_PAL_ENTRIES);
    }
    for (i = 0; i < p->footprint; i++) CHECK(p->channel_class[i] < LJ_CH_COUNT);
}

int lj_test_fixlib_profiles(int argc, char **argv);
int lj_test_fixlib_profiles(int argc, char **argv)
{
    const lj_profile *p, *q;
    const lj_exe_row *row;
    static lj_profile synth;   /* static: lj_layout_derive stores process-lifetime tables in it (LSan sees them as reachable) */
    int i, nhand = 0, nqxf = 0;
    (void)argc; (void)argv;

    /* the 201 rows, verbatim, ids in order */
    CHECK(lj_fixlib_count() >= LJ_EXE_ROWS);
    for (i = 0; i < lj_fixlib_count(); i++) { row = lj_fixlib_row(i); CHECK(row != NULL); if (row && i < LJ_EXE_ROWS) CHECK(row->id == i); if (row && i >= LJ_EXE_ROWS) CHECK(row->id >= LJ_QXF_EXTRA_FIRST); }
    CHECK(lj_fixlib_row(-1) == NULL && lj_fixlib_row(lj_fixlib_count()) == NULL);
    row = lj_fixlib_row(12);
    CHECK(row && strcmp(row->manufacturer, "Martin") == 0 && strcmp(row->name, "MAC 500") == 0 && strcmp(row->mode, "Mode 2/4") == 0 && strcmp(row->tag, "MAC500") == 0 && row->binding == LJ_BIND_HAND);
    row = lj_fixlib_row(19);
    CHECK(row && strcmp(row->manufacturer, "Generic") == 0 && strcmp(row->name, "DMX Dimmer") == 0 && strcmp(row->mode, " 1 Channel") == 0 && row->binding == LJ_BIND_HAND);
    row = lj_fixlib_row(0);
    CHECK(row && strcmp(row->name, "None") == 0);
    row = lj_fixlib_row(32);
    CHECK(row && row->tag[0] == 0 && strcmp(row->notes_file, "xxx") == 0 && row->binding == LJ_BIND_NONE);
    for (i = 0; i < LJ_EXE_ROWS; i++) {
        row = lj_fixlib_row(i);
        p = lj_fixlib_by_id((uint16_t)i);
        CHECK((p != NULL) == (row->binding != LJ_BIND_NONE));            /* greyed rows have no profile */
        if (p) { CHECK(p->id == i); CHECK(p->footprint >= 1 && p->footprint <= LJ_PROFILE_CH); check_tables(p); }
        if (row->binding == LJ_BIND_HAND) nhand++;
        if (row->binding == LJ_BIND_QXF) nqxf++;
    }
    printf("rows: %d, hand %d, qxf %d, extras %d\n", lj_fixlib_count(), nhand, nqxf, lj_qxf_extra_count);
    CHECK(nhand == 86 && nqxf >= 15);   /* W1 36 + the 30 W2 ids of 15/02-15/05 + the 20 W3 ids of 15/06-15/09 (ME-8 / ME-26: the hand count rises; their qxf rows stay mapped, hand wins) */

    /* id 12 = Martin / MAC 500 / Mode 2/4, footprint 16, hand wins over the QLC+ conversion */
    p = lj_fixlib_by_id(12);
    CHECK(p == &lj_hand_mac500);
    CHECK(p && p->footprint == 16 && strcmp(p->name, "MAC 500") == 0 && strcmp(p->mode, "Mode 2/4") == 0);
    CHECK(p && p->channel_class[1] == LJ_CH_INTENSITY && p->intensity.offset == 1 && p->color[0].offset == 2);
    q = NULL;
    for (i = 0; i < lj_qxf_count; i++) if (lj_qxf_profiles[i]->id == 12) q = lj_qxf_profiles[i];
    CHECK(q != NULL && q != p);                                              /* the conversion exists, so the precedence is real */
    for (i = 0; p && i < LJ_CTL_COUNT; i++) {
        int has = lj_profile_window(p, (enum lj_ctl_kind)i) != NULL;
        int want = i != LJ_CTL_LEVELS && i != LJ_CTL_GENERIC && i != LJ_CTL_RGBPACK;   /* 8 windows: no Levels grid [15/01 s3] */
        if (has != want) { fails++; printf("FAIL MAC 500 window kind %d: %s\n", i, has ? "present" : "missing"); }
    }
    CHECK(p && lj_profile_window(p, LJ_CTL_INTENSITY)->nrows == 5 && lj_profile_window(p, LJ_CTL_INTENSITY)->nspecials == 3);
    CHECK(p && strcmp(lj_profile_window(p, LJ_CTL_INTENSITY)->specials[0].buttons[0].caption, "Pulse Open") == 0 && lj_profile_window(p, LJ_CTL_INTENSITY)->specials[0].buttons[0].value == 80);
    CHECK(p && strcmp(lj_profile_window(p, LJ_CTL_EXTENDED)->rows[1].items[0], "Tracking") == 0 && lj_profile_window(p, LJ_CTL_EXTENDED)->rows[1].ch == 14);
    CHECK(p && lj_profile_window(p, LJ_CTL_LAMP)->rows[0].states_allowed == LJ_STATES_OFF_SNAP);
    CHECK(p && p->pt16 && p->pan_ofs == 10 && p->tilt_ofs == 12);
    CHECK(lj_fixlib_by_id(31) && lj_fixlib_by_id(31)->footprint == 16 && lj_profile_window(lj_fixlib_by_id(31), LJ_CTL_GOBOS)->nrows == 12);
    CHECK(lj_fixlib_by_id(24) && strcmp(lj_fixlib_by_id(24)->name, "Roboscan Pro 918") == 0 && lj_fixlib_by_id(24)->windows == lj_mac500_windows_fix);
    CHECK(lj_fixlib_by_id(30) && lj_fixlib_by_id(30)->windows == lj_mac500_windows_scrl);

    /* the dimmer: only Levels; ME-25 classes read back from the hand tables */
    p = lj_fixlib_by_id(19);
    CHECK(p && p->footprint == 1 && p->nwindows == 1 && lj_profile_window(p, LJ_CTL_LEVELS) && !lj_profile_window(p, LJ_CTL_INTENSITY));
    CHECK(p && p->channel_class[0] == LJ_CH_LEVEL && strcmp(lj_profile_window(p, LJ_CTL_LEVELS)->rows[0].caption, "CH1") == 0);
    CHECK(lj_fixlib_by_id(22) && lj_fixlib_by_id(22)->footprint == 32 && lj_profile_window(lj_fixlib_by_id(22), LJ_CTL_LEVELS)->nrows == 32);
    p = lj_fixlib_by_id(92);
    CHECK(p && p->channel_class[0] == LJ_CH_SWITCH && lj_profile_window(p, LJ_CTL_LEVELS)->rows[0].states_allowed == LJ_STATES_OFF_SNAP);
    CHECK(lj_fixlib_by_id(101) && lj_fixlib_by_id(101)->channel_class[31] == LJ_CH_SWITCH);
    CHECK(lj_fixlib_by_id(18) && lj_fixlib_by_id(18)->channel_class[0] == LJ_CH_SMOKE);
    CHECK(lj_fixlib_by_id(45) && lj_fixlib_by_id(45)->channel_class[0] == LJ_CH_HAZE && lj_fixlib_by_id(45)->channel_class[1] == LJ_CH_HAZE_FAN);
    CHECK(lj_fixlib_by_id(27) && lj_fixlib_by_id(27)->channel_class[5] == LJ_CH_SWITCH);
    CHECK(lj_fixlib_by_id(78) && lj_fixlib_by_id(78)->footprint == 12 && lj_profile_window(lj_fixlib_by_id(78), LJ_CTL_RGBPACK));
    CHECK(lj_fixlib_by_id(90) && lj_fixlib_by_id(90)->footprint == 32 && lj_fixlib_by_id(90)->nwindows == 0);
    CHECK(lj_fixlib_by_id(999) == NULL && lj_fixlib_by_id(32) == NULL);

    /* tier 2: a QLC+-bound id gets derived windows at first use; extras resolve too */
    p = lj_fixlib_by_id(156);                                                /* smartMAC from its qxf (54 MiniMAC Profile is a W3 hand file, 11 a W2 one) */
    CHECK(p && p->windows && p->nwindows > 0 && lj_profile_window(p, LJ_CTL_INTENSITY) && lj_profile_window(p, LJ_CTL_MOVEMENT) && lj_profile_window(p, LJ_CTL_COLORS));
    CHECK(p && p == lj_fixlib_by_id(156) && p->windows != lj_fixlib_by_id(54)->windows);   /* derived once; 54 is a hand table now */
    CHECK(lj_fixlib_by_id(1000) && lj_fixlib_by_id(1000)->footprint == 32);
    for (i = 0; i < lj_qxf_count; i++) { q = lj_fixlib_by_id(lj_qxf_profiles[i]->id); if (q) check_tables(q); }

    /* lj_layout_derive on a synthetic .GS2-shaped profile */
    memset(&synth, 0, sizeof synth);
    synth.footprint = 8;
    synth.shutter = (lj_dyn){ 1, 0, "Shutter", 0, 20 }; synth.strobe = (lj_dyn){ 1, 0, "Strobe", 50, 72 }; synth.intensity = (lj_dyn){ 1, 1, "Dim", 0, 255 };
    synth.color[0] = (lj_dyn){ 1, 2, "Color", 0, 200 }; synth.palette[0].e[0].used = 1;
    synth.focus = (lj_dyn){ 1, 3, "Focus", 255, 0 };                      /* Near = 255, Far = 0: reverse */
    synth.lamp_on.enabled = 1; synth.lamp_on.defined[0] = 1; synth.lamp_on.value[0] = 230;
    synth.lamp_off.enabled = 1; synth.lamp_off.defined[0] = 1; synth.lamp_off.value[0] = 250;
    synth.special[0].enabled = 1; memcpy(synth.special[0].name, "Fan Full", 9); synth.special[0].defined[4] = 1; synth.special[0].value[4] = 255;
    synth.channel_class[0] = LJ_CH_SHUTTER; synth.channel_class[1] = LJ_CH_INTENSITY; synth.channel_class[2] = LJ_CH_COLOR; synth.channel_class[3] = LJ_CH_BEAM;
    synth.channel_class[4] = LJ_CH_LEVEL; synth.channel_class[5] = LJ_CH_SWITCH; synth.channel_class[6] = LJ_CH_LEVEL; synth.channel_class[7] = LJ_CH_NONE;
    for (i = 0; i < 4; i++) synth.level_invisible[i] = 1;
    synth.level_invisible[7] = 1; synth.level_disabled[7] = 1;
    memcpy(synth.level_caption[6], "Fan", 4);
    CHECK(lj_layout_derive(NULL) == -1);
    CHECK(lj_layout_derive(&synth) == 0 && synth.nwindows == 5);
    check_tables(&synth);
    {
        const lj_ctl_window *w = lj_profile_window(&synth, LJ_CTL_INTENSITY);
        CHECK(w && w->nrows == 5 && w->rows[0].kind == LJ_W_FADEBTN && w->rows[0].rows == 2 && w->rows[1].kind == LJ_W_SHUTTER_TOGGLE && w->rows[1].nbuttons == 2 && w->rows[1].buttons[1].value == 20);
        CHECK(w && w->rows[2].kind == LJ_W_DRAGBAR && w->rows[2].lo == 50 && w->rows[3].kind == LJ_W_FADEBTN && w->rows[4].ch == 1);
        w = lj_profile_window(&synth, LJ_CTL_COLORS);
        CHECK(w && w->nrows == 3 && w->rows[1].kind == LJ_W_SCROLL && w->rows[1].hi == 200 && w->rows[2].kind == LJ_W_PALETTE && w->rows[2].lo == 0 && w->rows[0].rows == 2);
        w = lj_profile_window(&synth, LJ_CTL_BEAM);
        CHECK(w && w->nrows == 2 && w->rows[1].reverse == 1 && w->rows[1].lo == 0 && w->rows[1].hi == 255 && strcmp(w->rows[1].caption, "Focus") == 0);
        w = lj_profile_window(&synth, LJ_CTL_LAMP);
        CHECK(w && w->nrows == 3 && w->rows[1].kind == LJ_W_LAMP_BTNS && w->rows[1].nbuttons == 2 && w->rows[1].buttons[1].value == 250 && w->rows[0].states_allowed == LJ_STATES_OFF_SNAP);
        w = lj_profile_window(&synth, LJ_CTL_LEVELS);
        CHECK(w && w->nrows == 3 && w->rows[0].ch == 4 && strcmp(w->rows[0].caption, "CH5") == 0 && w->rows[1].ch == 5 && w->rows[1].states_allowed == LJ_STATES_OFF_SNAP && strcmp(w->rows[2].caption, "Fan") == 0);
        CHECK(w && w->nspecials == 1 && w->specials[0].kind == LJ_W_SPECIAL_GROUP && w->specials[0].states_allowed == LJ_STATES_OFF_SNAP && w->specials[0].nbuttons == 1 && w->specials[0].buttons[0].ch == 4 && w->specials[0].buttons[0].value == 255);
        CHECK(!lj_profile_window(&synth, LJ_CTL_MOVEMENT) && !lj_profile_window(&synth, LJ_CTL_GOBOS) && !lj_profile_window(&synth, LJ_CTL_EFFECTS));
    }
    printf("test_fixlib_profiles: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
