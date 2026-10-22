/* test_gdc.c - the gdc-fades-followspot track's model rows (docs/w3-contracts.md section 7).
 * test_gdc_grammar: the F35 command line (CHM thegenericdmxcontrol.htm examples verbatim): 1+3+5, +1+3+5, -1-3-5, 45/77,
 *   1/10-4/6+12 = 1,2,3,7,8,9,10,12, #1/5, #1/#5 refused, ## and @@, @+10 / @-25 clamped, o/s/f, . , * aliases,
 *   #1/3F@50-#2@+10, the empty line both ways; levels default to Fade unless Snap; % and 8-bit levels; only generic
 *   channels and not hidden ones map; the error column of every invalid line; GenericGroups.dat round trip.
 * test_followspot: record (non follow-spot fixtures ignored, none = -1) / apply (all Snap, unprogrammed channels at the
 *   profile default DMX, the only-selected rule), FollowSpot.dat round trip and validation; sequence fades transforms on
 *   one scene and All Scenes, attribute masks, the Movement Macro rows. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "model/lj_followspot.h"
#include "model/lj_gdc.h"
#include "model/lj_scene.h"
#include "model/lj_seqfades.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static void quiet(void *ud, const char *line) { (void)ud; (void)line; }

/* fixtures: 1 = DMX Dimmer 32 at 1:1 (codes 0..31), 2 = DMX Dimmer 32 at 1:33 (32..63), 3 = DMX Dimmer 24 at 1:65 (64..87),
 * 4 = MAC 500 at 1:101 (not generic), 5 = DMX Switch Pack 8 at 2:1 (512..519) */
static lj_show *make_show(void)
{
    static const struct { uint16_t id; uint8_t link; uint16_t addr; } fx[] = { { 22, 1, 1 }, { 22, 1, 33 }, { 20, 1, 65 }, { 12, 1, 101 }, { 96, 2, 1 } };
    lj_show *s = lj_show_new();
    int i;
    for (i = 0; i < LJ_COUNT_OF(fx); i++) { s->fixture[i].kind = LJ_FX_BUILTIN; s->fixture[i].profile_id = fx[i].id; s->fixture[i].link = fx[i].link; s->fixture[i].address = fx[i].addr; }
    return s;
}

/* the selection as "1,3,5" (channel numbers) */
static const char *sel_text(const lj_gdc_state *st)
{
    static char buf[4096];
    size_t n = 0;
    int ch;
    buf[0] = 0;
    for (ch = 0; ch < LJ_CHANS && n < sizeof buf - 8; ch++) if (st->sel[ch]) n += (size_t)snprintf(buf + n, sizeof buf - n, "%s%d", n ? "," : "", ch + 1);
    return buf;
}

static int run(const char *line, lj_show *sh, lj_gdc_state *st, lj_scene *sc)
{
    lj_gdc_prog p;
    char err[96];
    if (lj_gdc_parse(line, &p, err, sizeof err) != 0) { printf("FAIL parse \"%s\": %s\n", line, err); fails++; return -1; }
    return lj_gdc_run(&p, sh, st, sc);
}

int lj_test_gdc_grammar(int argc, char **argv);
int lj_test_gdc_grammar(int argc, char **argv)
{
    static const struct { const char *line, *sel; } sel_rows[] = {
        { "1+3+5", "1,3,5" }, { "+7+9", "1,3,5,7,9" }, { "-1-3-5", "7,9" }, { "45/77", "45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77" },
        { "1/10-4/6+12", "1,2,3,7,8,9,10,12" }, { "10/1", "1,2,3,4,5,6,7,8,9,10" },
        { "100/102", "" } /* the MAC 500 is not a generic channel */, { "88/89", "88" }, { "513+520+521", "513,520" } /* switch pack mapped, 521 unpatched */,
        { "#1", "1,2" }, { "#1/3", "1,2,3,4,5" }, { ".2", "3,4" }, { ",3+#1", "1,2,5" }, { "#1/3-#2", "1,2,5" }, { "#1/3-.2/3", "1,2" },
        { "##", NULL }, { "", "" } };
    static const struct { const char *line; int col; } bad_rows[] = {
        { "#1/#5", 4 }, { "1/", 3 }, { "abc", 1 }, { "@", 2 }, { "@+", 3 }, { "#33", 1 }, { "2049", 1 }, { "0", 1 }, { "12345", 5 },
        { "+", 2 }, { "-#", 3 }, { "1/.3", 3 }, { "1@50x", 5 }, { "@@", 3 }, { "1 / 2", 3 } };
    lj_show *sh;
    lj_gdc_state st;
    lj_gdc_prog p;
    lj_scene *sc;
    lj_gdc_groups g2;
    char err[96], line[200];
    int i, ch, n;
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    sh = make_show();
    sc = (lj_scene *)calloc(1, sizeof *sc);
    if (!sc) return 1;
    lj_scene_init(sc);
    memset(&st, 0, sizeof st);
    st.empty_clears = 1;
    /* groups: 1 = ch 1,2; 2 = ch 3,4; 3 = ch 5; ch 6 hidden; MAC channel 101 in group 1 must stay unmapped */
    sh->gdc.used[0] = sh->gdc.used[1] = sh->gdc.used[2] = 1; strcpy(sh->gdc.name[0], "Front wash");
    sh->gdc.member[0] = sh->gdc.member[1] = 1u; sh->gdc.member[2] = sh->gdc.member[3] = 2u; sh->gdc.member[4] = 4u; sh->gdc.member[100] = 1u;

    CHECK(lj_gdc_is_generic(sh, 0) && lj_gdc_is_generic(sh, 87) && !lj_gdc_is_generic(sh, 88) && !lj_gdc_is_generic(sh, 100) && lj_gdc_is_generic(sh, 519) && !lj_gdc_is_generic(sh, -1));
    for (i = 0; i < LJ_COUNT_OF(sel_rows); i++) {
        CHECK(run(sel_rows[i].line, sh, &st, sc) == 0);
        if (sel_rows[i].sel && strcmp(sel_text(&st), sel_rows[i].sel) != 0) { fails++; printf("FAIL \"%s\" selected %s, want %s\n", sel_rows[i].line, sel_text(&st), sel_rows[i].sel); }
    }
    for (n = 0, ch = 0; ch < LJ_CHANS; ch++) n += st.sel[ch];
    CHECK(n == 0);                                                   /* "" cleared the ## selection */
    run("##", sh, &st, sc);
    for (n = 0, ch = 0; ch < LJ_CHANS; ch++) n += st.sel[ch];
    CHECK(n == 88 + 8);                                              /* every generic channel: 32 + 32 + 24 + 8 */
    st.empty_clears = 0;
    run("", sh, &st, sc);
    for (n = 0, ch = 0; ch < LJ_CHANS; ch++) n += st.sel[ch];
    CHECK(n == 96);                                                  /* Empty Command Clears Selection off */
    st.empty_clears = 1;
    sh->gdc.hidden[5] = 1;
    run("1/8", sh, &st, sc);
    CHECK(strcmp(sel_text(&st), "1,2,3,4,5,7,8") == 0);             /* Hide Selected Channels removes 6 from the map */
    sh->gdc.hidden[5] = 0;

    /* levels: 8-bit, default Fade unless Snap, relative clamps, @@ all channels, states */
    sc->state[1] = LJ_SNAP;
    CHECK(run("1/3@100", sh, &st, sc) == 3);
    CHECK(sc->val[0] == 100 && sc->state[0] == LJ_FADE && sc->val[1] == 100 && sc->state[1] == LJ_SNAP && sc->val[2] == 100 && sc->state[3] == LJ_OFF);
    CHECK(run("@+200", sh, &st, sc) == 3 && sc->val[0] == 255 && sc->val[2] == 255);   /* capped at 255 */
    CHECK(run("@-25", sh, &st, sc) == 3 && sc->val[0] == 230);
    CHECK(run("*-250", sh, &st, sc) == 3 && sc->val[0] == 0);       /* '*' for '@', capped at 0 */
    CHECK(run("s", sh, &st, sc) == 2 && sc->state[0] == LJ_SNAP && sc->state[2] == LJ_SNAP);
    CHECK(run("O", sh, &st, sc) == 3 && sc->state[0] == LJ_OFF && sc->state[1] == LJ_OFF);
    CHECK(run("f", sh, &st, sc) == 3 && sc->state[1] == LJ_FADE);
    CHECK(run("@@7", sh, &st, sc) == 96 && sc->val[87] == 7 && sc->state[87] == LJ_FADE && sc->val[100] == 0 && sc->state[100] == LJ_OFF && sc->val[519] == 7);
    CHECK(strcmp(sel_text(&st), "1,2,3") == 0);                      /* @@ leaves the selection */
    /* the CHM's combined example: groups 1..3, state, level 50, deselect group 2, +10 on the rest */
    st.percent = 0;
    CHECK(run("#1/3F@50-#2@+10", sh, &st, sc) > 0);
    CHECK(sc->val[0] == 60 && sc->val[1] == 60 && sc->val[2] == 50 && sc->val[3] == 50 && sc->val[4] == 60 && sc->val[5] == 7);
    CHECK(strcmp(sel_text(&st), "1,2,5") == 0);
    /* % values: 50 % = 128 (nearest), relative +10 % = +26, 150 % clamps to 100 % */
    st.percent = 1;
    CHECK(run("1@50", sh, &st, sc) == 1 && sc->val[0] == 128);
    CHECK(run("@+10", sh, &st, sc) == 1 && sc->val[0] == 154);
    CHECK(run("@150", sh, &st, sc) == 1 && sc->val[0] == 255);
    CHECK(run("1@50 3@25", sh, &st, sc) == 2 && sc->val[0] == 128 && sc->val[2] == 64);   /* spaces separate, each plain number replaces the selection */
    st.percent = 0;

    /* invalid lines: the column of the first byte that does not fit (1-based) */
    for (i = 0; i < LJ_COUNT_OF(bad_rows); i++) {
        char want[64];
        snprintf(want, sizeof want, "input error at column %d", bad_rows[i].col);
        CHECK(lj_gdc_parse(bad_rows[i].line, &p, err, sizeof err) == -1);
        if (strcmp(err, want) != 0) { fails++; printf("FAIL \"%s\": \"%s\", want \"%s\"\n", bad_rows[i].line, err, want); }
    }
    memset(line, '1', sizeof line); line[LJ_GDC_LINE_MAX] = 0;
    CHECK(lj_gdc_parse(line, &p, err, sizeof err) == -1);
    for (i = 0, line[0] = 0; i < LJ_GDC_CMDS_MAX + 1; i++) strcat(line, "o");
    line[LJ_GDC_CMDS_MAX + 1] = 0;
    CHECK(lj_gdc_parse(line, &p, err, sizeof err) == -1);          /* 65 commands > LJ_GDC_CMDS_MAX */
    CHECK(lj_gdc_parse("1", NULL, err, sizeof err) == -1);
    CHECK(lj_gdc_run(NULL, sh, &st, sc) == -1);
    p.n = 1; p.c[0].op = LJ_GDC_SEL_SET; p.c[0].lo = 0; p.c[0].hi = 3;
    CHECK(lj_gdc_run(&p, sh, &st, sc) == -1);                        /* channel 0: a hand-built program is checked too */
    p.c[0].op = LJ_GDC_GRP_ADD; p.c[0].lo = 1; p.c[0].hi = 33;
    CHECK(lj_gdc_run(&p, sh, &st, sc) == -1);

    /* GenericGroups.dat */
    sh->gdc.hidden[7] = 1; sh->gdc.member[2047] = 0x80000001u; sh->gdc.used[31] = 1;
    CHECK(lj_gdc_groups_save("test_gdc_groups.dat", &sh->gdc) == 0);
    CHECK(lj_gdc_groups_load("test_gdc_groups.dat", &g2, err, sizeof err) == 0);
    CHECK(memcmp(&g2, &sh->gdc, sizeof g2) == 0);
    CHECK(lj_gdc_groups_load("test_gdc_groups_missing.dat", &g2, err, sizeof err) == 0 && g2.used[0] == 0);
    {
        static const char *const bad[] = { "group 33 name=x\n", "ch 1:513 groups=1\n", "ch 1:1 groups=x\n", "hide 5:1\n", "ch 1:1\n" };
        for (i = 0; i < LJ_COUNT_OF(bad); i++) {
            FILE *f = fopen("test_gdc_groups.dat", "wb");
            if (f) { fputs("LJ4 gdcgroups 1\n", f); fputs(bad[i], f); fclose(f); }
            CHECK(lj_gdc_groups_load("test_gdc_groups.dat", &g2, err, sizeof err) == -1);
        }
    }
    remove("test_gdc_groups.dat");
    free(sc);
    lj_show_free(sh);
    printf("test_gdc_grammar: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

/* ---- follow spot + sequence fades ---- */

static int find_class(const lj_profile *p, int cls) { int o; for (o = 0; o < p->footprint; o++) if (p->channel_class[o] == cls) return o; return -1; }

int lj_test_followspot(int argc, char **argv);
int lj_test_followspot(int argc, char **argv)
{
    lj_show *sh;
    lj_scene *sc;
    lj_followspot *fs, *back;
    lj_sequence *seq;
    const lj_profile *mac;
    uint8_t sel[LJ_FIXTURES], out[LJ_FIXTURES], only[LJ_FIXTURES];
    char err[256];
    int base, o, pan, inten, ch;
    (void)argc; (void)argv;
    lj_log_set_sink(quiet, NULL);
    sh = make_show();
    mac = lj_show_profile(sh, 3);
    base = lj_show_fixture_base(sh, 3);
    sc = (lj_scene *)calloc(1, sizeof *sc);
    fs = (lj_followspot *)calloc(LJ_FOLLOWSPOT, sizeof *fs);
    back = (lj_followspot *)calloc(LJ_FOLLOWSPOT, sizeof *back);
    if (!sc || !fs || !back || !mac) { printf("test_followspot: setup failed\n"); return 1; }
    pan = find_class(mac, LJ_CH_PAN); inten = find_class(mac, LJ_CH_INTENSITY);
    if (inten < 0) inten = find_class(mac, LJ_CH_SHUTTER);
    CHECK(base == 100 && pan >= 0 && inten >= 0);
    lj_scene_init(sc);

    /* record: the MAC's programmed channels + its selection; the dimmer is not a follow spot and is ignored */
    sc->val[base + pan] = 77; sc->state[base + pan] = LJ_FADE;
    sc->val[base + inten] = 200; sc->state[base + inten] = LJ_SNAP;
    sc->val[0] = 99; sc->state[0] = LJ_FADE;
    memset(sel, 0, sizeof sel);
    CHECK(lj_followspot_record(&fs[0], sh, sc, sel) == -1);         /* nothing selected */
    sel[0] = 1;
    CHECK(lj_followspot_record(&fs[0], sh, sc, sel) == -1 && !fs[0].used);   /* "No Followspot Fixtures Selected" */
    sel[3] = 1;
    strcpy(fs[0].name, "Guitar Solo 1"); fs[0].static_page = 1; fs[0].static_slot = 3; fs[0].static_auto_clear = 1;
    CHECK(lj_followspot_record(&fs[0], sh, sc, sel) == 0);
    CHECK(fs[0].used == 1 && fs[0].selected[3] == 1 && fs[0].selected[0] == 0 && strcmp(fs[0].name, "Guitar Solo 1") == 0);
    CHECK(fs[0].val[base + pan] == 77 && fs[0].state[base + pan] == LJ_FADE && fs[0].state[0] == LJ_OFF && fs[0].val[0] == 0);

    /* apply: programmed values, the rest at default DMX, every channel of the fixture Snap; other fixtures untouched */
    lj_scene_init(sc);
    sc->val[base + inten] = 1; sc->state[base + inten] = LJ_FADE; sc->val[3] = 5; sc->state[3] = LJ_FADE;
    CHECK(lj_followspot_apply(&fs[1], sh, sc, NULL, out) == -1);    /* unused setting */
    CHECK(lj_followspot_apply(&fs[0], sh, sc, NULL, out) == 1);
    CHECK(out[3] == 1 && out[0] == 0);
    for (o = 0; o < mac->footprint; o++) {
        ch = base + o;
        CHECK(sc->state[ch] == LJ_SNAP);
        CHECK(sc->val[ch] == (o == pan ? 77 : o == inten ? 200 : mac->default_dmx[o]));
    }
    CHECK(sc->val[3] == 5 && sc->state[3] == LJ_FADE && sc->state[base + mac->footprint] == LJ_OFF);
    /* "Use only selected fixtures as Follow spots": only the recorded fixtures already selected */
    memset(only, 0, sizeof only); only[0] = 1;
    lj_scene_init(sc);
    CHECK(lj_followspot_apply(&fs[0], sh, sc, only, out) == 0 && out[3] == 0 && sc->state[base + pan] == LJ_OFF);
    only[3] = 1;
    CHECK(lj_followspot_apply(&fs[0], sh, sc, only, out) == 1 && out[3] == 1 && sc->val[base + pan] == 77);

    /* FollowSpot.dat round trip + validation */
    strcpy(fs[19].name, "Spare \"B\""); fs[19].static_page = 64; fs[19].static_slot = 64;
    CHECK(lj_followspot_validate(fs) == NULL);
    CHECK(lj_followspot_save("test_followspot.dat", fs) == 0);
    CHECK(lj_followspot_load("test_followspot.dat", back, err, sizeof err) == 0);
    CHECK(memcmp(back, fs, sizeof(lj_followspot) * LJ_FOLLOWSPOT) == 0);
    CHECK(lj_followspot_load("test_followspot_missing.dat", back, err, sizeof err) == 0 && back[0].used == 0);
    fs[5].state[9] = 3; CHECK(lj_followspot_validate(fs) != NULL); fs[5].state[9] = 0;
    fs[5].static_page = 65; fs[5].static_slot = 1; CHECK(lj_followspot_validate(fs) != NULL); fs[5].static_page = 0; fs[5].static_slot = 0;
    memset(fs[5].name, 'x', sizeof fs[5].name); CHECK(lj_followspot_validate(fs) != NULL); fs[5].name[0] = 0;
    CHECK(lj_followspot_save("test_followspot.dat", NULL) == -1);
    {
        static const char *const bad[] = { "spot 21 used=1\n", "ch 1:1=5:S\n", "spot 1 used=2\n", "spot 1 sel=101\n", "spot 1 static=1\n", "spot 1 static=65.1\n",
                                           "spot 1 used=1\nch 1:1=256:S\n", "spot 1 used=1\nch 1:1=5:X\n" };
        int i;
        for (i = 0; i < LJ_COUNT_OF(bad); i++) {
            FILE *f = fopen("test_followspot.dat", "wb");
            if (f) { fputs("LJ4 follow 1\n", f); fputs(bad[i], f); fclose(f); }
            CHECK(lj_followspot_load("test_followspot.dat", back, err, sizeof err) == -1 && back[0].used == 0);
        }
    }
    remove("test_followspot.dat");

    /* sequence fades: attributes by class; one scene vs All Scenes; Movement Macro rows; values never change */
    {
        lj_seqfades f;
        int dim = 0;
        seq = lj_sequence_new_rec();
        CHECK(lj_sequence_alloc(seq, 3) == 0);
        CHECK(lj_ch_attr(mac, pan) == LJ_ATTR_MOVEMENT && lj_ch_attr(mac, inten) == LJ_ATTR_INTENSITY && lj_ch_attr(mac, 99) == -1 && lj_ch_attr(NULL, 0) == -1);
        CHECK(lj_ch_attr(lj_show_profile(sh, 0), 0) == LJ_ATTR_INTENSITY || lj_ch_attr(lj_show_profile(sh, 0), 0) == LJ_ATTR_LEVEL);
        {   /* ME-22: a pair's LSB carries its MSB's attribute (one fade state per 16-bit value) */
            lj_profile pp = *mac;
            pp.channel_class[2] = LJ_CH_COLOR; pp.channel_class[3] = LJ_CH_GOBO; pp.pair_lsb[2] = 1 + 3;
            CHECK(lj_ch_attr(&pp, 3) == LJ_ATTR_COLOR && lj_ch_attr(&pp, 2) == LJ_ATTR_COLOR);
            pp.channel_class[pan + 1] = LJ_CH_NONE;
            CHECK(lj_ch_attr(&pp, pan + 1) == LJ_ATTR_MOVEMENT);   /* pt16: pan_ofs + 1 */
        }
        for (o = 0; o < 3; o++) {
            seq->scenes[o].val[base + pan] = 40; seq->scenes[o].state[base + pan] = LJ_FADE;
            seq->scenes[o].val[base + inten] = 50; seq->scenes[o].state[base + inten] = LJ_FADE;
            seq->scenes[o].state[dim] = LJ_FADE;
            seq->scenes[o].macro[3].fstate[0] = LJ_FADE; seq->scenes[o].macro[3].fstate[1] = LJ_SNAP;
        }
        memset(sel, 0, sizeof sel); sel[3] = 1;
        f.attr_mask = 1u << LJ_ATTR_MOVEMENT; f.all_scenes = 0; f.sfx = LJ_SFX_FADE_SNAP;
        CHECK(lj_seqfades_apply(&f, sh, seq, 1, sel) >= 1);
        CHECK(seq->scenes[1].state[base + pan] == LJ_SNAP && seq->scenes[1].val[base + pan] == 40);
        CHECK(seq->scenes[0].state[base + pan] == LJ_FADE && seq->scenes[2].state[base + pan] == LJ_FADE);
        CHECK(seq->scenes[1].state[base + inten] == LJ_FADE && seq->scenes[1].state[dim] == LJ_FADE);   /* other attribute / unselected fixture */
        CHECK(lj_seqfades_apply(&f, sh, seq, 1, sel) == 0);              /* nothing left to change */
        f.attr_mask = 1u << LJ_ATTR_INTENSITY; f.all_scenes = 1; f.sfx = LJ_SFX_ALL_OFF;
        CHECK(lj_seqfades_apply(&f, sh, seq, 99, sel) >= 3);
        for (o = 0; o < 3; o++) CHECK(seq->scenes[o].state[base + inten] == LJ_OFF && seq->scenes[o].val[base + inten] == 50 && seq->scenes[o].state[base + pan] == (o == 1 ? LJ_SNAP : LJ_FADE));
        f.sfx = LJ_SFX_OFF_FADE;
        CHECK(lj_seqfades_apply(&f, sh, seq, 0, sel) >= 3 && seq->scenes[0].state[base + inten] == LJ_FADE);   /* re-applying makes the values active again */
        f.attr_mask = 1u << LJ_ATTR_MOVEMACRO; f.all_scenes = 0; f.sfx = LJ_SFX_SNAP_OFF;
        CHECK(lj_seqfades_apply(&f, sh, seq, 2, sel) == 1 && seq->scenes[2].macro[3].fstate[1] == LJ_OFF && seq->scenes[2].macro[3].fstate[0] == LJ_FADE);
        CHECK(lj_seqfades_apply(&f, sh, seq, 3, sel) == -1);             /* scene out of range */
        f.sfx = LJ_SFX_COUNT;
        CHECK(lj_seqfades_apply(&f, sh, seq, 0, sel) == -1);
        lj_sequence_free(seq);
    }
    free(sc); free(fs); free(back);
    lj_show_free(sh);
    printf("test_followspot: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
