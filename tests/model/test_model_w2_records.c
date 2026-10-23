/* test_model_w2_records.c - `lj4evr_tests test_model_w2_records`: every W2 record kind round-trips
 * (write, read, rewrite byte-equal): CuePageInfo.Dat through lj_cuepages, Statics.dat with group names
 * and n=64, BG cue, GroupInfo.dat with HTP names, presets, palettes, the .GPA patch, DefaultOut.dat,
 * Notes.txt; a CP1252 0xE9 survives in each name; the loaders reject out-of-range numbers instead of
 * aliasing (ME-77: level=356, cuetime=65636, spt=257, link=257, addr=65537 and the W2 keys); ME-30
 * n=64 loads, n=65 is rejected; the validators reject out-of-range slot / page / scope values; the
 * Statics 2 scopes, link slots, group select and Default output "Use Current Scene". Temp files are
 * test_model_w2_*.tmp in the cwd. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_cuepage.h"
#include "model/lj_statics.h"
#include "core/lj_ini.h"
#include "model/lj_bgcue.h"
#include "model/lj_groups.h"
#include "model/lj_presets.h"
#include "model/lj_palettes.h"
#include "model/lj_patch.h"
#include "model/lj_defaultout.h"
#include "model/lj_notes.h"
#include "model/lj_scene.h"
#include "model/lj_show.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char lj_e9[] = "Caf\xe9 \"page\"";   /* CP1252 0xE9 and a doubled quote */
#define P1 "test_model_w2_a.tmp"
#define P2 "test_model_w2_b.tmp"

static void lj_put(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static int lj_same_bytes(const char *a, const char *b)
{
    static char x[1 << 16], y[1 << 16];
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    size_t na = fa ? fread(x, 1, sizeof x, fa) : 0, nb = fb ? fread(y, 1, sizeof y, fb) : 0;
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return fa && fb && na == nb && na > 0 && memcmp(x, y, na) == 0;
}

static void lj_test_cuepages(void)
{
    static lj_cuepages a, b;   /* 270 KB each: off the stack */
    char err[256];
    lj_cuepages_init(&a);
    CHECK(lj_cuepages_add(&a, "Page 1") == 0 && lj_cuepages_add(&a, lj_e9) == 1 && lj_cuepages_insert(&a, 0, "First") == 0);
    CHECK(a.n == 3 && strcmp(a.page[1].name, "Page 1") == 0);
    CHECK(lj_cuepages_add_cue(&a, 1, 5, -1) == 1 && lj_cuepages_add_cue(&a, 1, 9, -1) == 1 && lj_cuepages_add_cue(&a, 1, 1, 0) == 1);
    CHECK(lj_cuepages_add_cue(&a, 1, 5, -1) == 0);                                   /* never adds twice [03 s5.7] */
    CHECK(lj_cuepages_add_cue(&a, 1, 0, -1) == -1 && lj_cuepages_add_cue(&a, 9, 3, -1) == -1);
    CHECK(a.page[1].n == 3 && a.page[1].cue[0] == 1 && a.page[1].cue[1] == 5 && a.page[1].cue[2] == 9);
    CHECK(lj_cuepages_add_cue(&a, 2, 77, -1) == 1);
    CHECK(lj_cuepages_save(P1, &a) == 0 && lj_cuepages_load(P1, &b, err, sizeof err) == 0);
    CHECK(b.n == 3 && strcmp(b.page[2].name, lj_e9) == 0 && b.page[1].n == 3 && b.page[2].cue[0] == 77 && b.current == 0 && b.cursor == -1);
    CHECK(lj_cuepages_save(P2, &b) == 0 && lj_same_bytes(P1, P2));

    /* tabs, cursor, steps */
    a.current = 1;
    CHECK(lj_cuepages_step_cue(&a, 1) == 1 && lj_cuepages_step_cue(&a, 1) == 5 && lj_cuepages_step_cue(&a, 1) == 9 && lj_cuepages_step_cue(&a, 1) == 0 && a.cursor == 2);
    CHECK(lj_cuepages_step_cue(&a, -1) == 5);
    CHECK(lj_cuepages_step(&a, 1, 0) == 2 && lj_cuepages_step(&a, 1, 0) == -1 && a.current == 2 && lj_cuepages_step(&a, 1, 1) == 0 && lj_cuepages_step(&a, -1, 1) == 2);
    CHECK(lj_cuepages_move(&a, 2, 0) == 0 && a.current == 0 && strcmp(a.page[0].name, lj_e9) == 0 && strcmp(a.page[1].name, "First") == 0);
    CHECK(lj_cuepages_rename(&a, 1, "Renamed") == 0 && strcmp(a.page[1].name, "Renamed") == 0);
    CHECK(lj_cuepages_clear(&a, 2) == 0 && a.page[2].n == 0);
    a.current = 2;
    CHECK(lj_cuepages_delete(&a, 2) == 0 && a.n == 2 && a.current == 1);              /* current clamps */
    CHECK(lj_cuepages_remove_cue(&a, 0, 0) == 0 && a.page[0].n == 0 && lj_cuepages_remove_cue(&a, 0, 0) == -1);
    CHECK(lj_cuepages_validate(&a) == NULL);

    /* validator and loader rejections (ME-77: no alias of an over-long number) */
    a.current = 7;                         CHECK(lj_cuepages_validate(&a) != NULL); a.current = 0;
    a.page[1].n = 1; a.page[1].cue[0] = 0; CHECK(lj_cuepages_validate(&a) != NULL); a.page[1].cue[0] = 3;
    a.n = LJ_CUE_PAGES + 1;                CHECK(lj_cuepages_validate(&a) != NULL); a.n = 2;
    lj_put(P1, "LJ4 cuepages 1\npage 1 name=x cues=1,100000000\n");
    CHECK(lj_cuepages_load(P1, &b, err, sizeof err) == -1 && b.n == 0);
    lj_put(P1, "LJ4 cuepages 1\npage 1 name=x cues=1,0\n");
    CHECK(lj_cuepages_load(P1, &b, err, sizeof err) == -1);
    remove(P1);
    CHECK(lj_cuepages_load(P1, &b, err, sizeof err) == 0 && b.n == 0);               /* missing file = no pages */
    while (lj_cuepages_add(&a, "p") >= 0) { }
    CHECK(a.n == LJ_CUE_PAGES && lj_cuepages_insert(&a, 0, "x") == -1);
}

static void lj_test_statics2(void)
{
    lj_static_page *pages = NULL, *back = NULL;
    lj_static_groups g, gb;
    lj_scene sc;
    char err[256];
    int n = 0, nb = 0, i;
    for (i = 0; i < LJ_STATIC_GROUPS; i++) snprintf(g.name[i], sizeof g.name[i], "Group %d", i + 1);
    strcpy(g.name[0], "Strobes"); strcpy(g.name[63], lj_e9);
    CHECK(lj_statics_add_page(&pages, &n, "Page 1") == 0 && lj_statics_add_page(&pages, &n, lj_e9) == 1);
    pages[1].n = LJ_STATICS2_SLOTS_MAX;                                               /* ME-30: Visible Slots 64 */
    lj_scene_init(&sc); sc.val[0] = 200; sc.state[0] = LJ_SNAP; sc.val[1] = 7; sc.state[1] = LJ_FADE;   /* a 16-bit pair stored bytewise */
    CHECK(lj_static_slot_record(&pages[0].slot[0], &sc) == 0);
    strcpy(pages[0].slot[0].name, lj_e9); pages[0].slot[0].replace = LJ_REP_GROUP; pages[0].slot[0].groups = 1u | ((uint64_t)1 << 63);
    pages[0].slot[0].level_pct = 100; pages[0].slot[0].fade_ms = 60000;
    CHECK(lj_static_slot_link(&pages[0].slot[1], 12, 998) == 0 && pages[0].slot[1].kind == LJ_STATIC_LINK);
    CHECK(lj_static_slot_link(&pages[0].slot[2], 0, 1) == -1 && lj_static_slot_link(&pages[0].slot[2], 1, 999) == -1);
    pages[0].slot[1].groups = 1; pages[0].slot[1].replace = LJ_REP_SLOT;
    pages[1].slot[1].kind = LJ_STATIC_LINK; pages[1].slot[1].link_seq = 3; pages[1].slot[1].replace = LJ_REP_SOLO_PAGE;
    CHECK(lj_statics_validate(pages, n) == NULL);
    CHECK(lj_statics_groups_save(P1, pages, n, &g) == 0);
    CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == 0 && nb == 2 && lj_statics_groups_load(P1, &gb, err, sizeof err) == 0);
    CHECK(nb == 2 && back[1].n == 64 && strcmp(back[1].name, lj_e9) == 0 && strcmp(back[0].slot[0].name, lj_e9) == 0);
    CHECK(nb == 2 && back[0].slot[0].scene && back[0].slot[0].scene->val[0] == 200 && back[0].slot[0].scene->state[1] == LJ_FADE && back[0].slot[0].groups == (1u | ((uint64_t)1 << 63)));
    CHECK(nb == 2 && back[0].slot[1].link_seq == 12 && back[0].slot[1].link_scene == 998 && !back[0].slot[1].scene);
    CHECK(strcmp(gb.name[0], "Strobes") == 0 && strcmp(gb.name[63], lj_e9) == 0 && strcmp(gb.name[5], "Group 6") == 0);
    CHECK(lj_statics_groups_save(P2, back, nb, &gb) == 0 && lj_same_bytes(P1, P2));

    /* scopes (PROPOSAL rig 14 sB.8 item 3): 0 = page1 slot1 (group 1|64), 1 = page1 slot2 (slot), 65 = page2 slot2 (solo page); stride 64 (ME-30) */
    CHECK(lj_static_scope_hits(back, nb, 0, 1) == 1);                                 /* shared group bit */
    CHECK(lj_static_scope_hits(back, nb, 0, 65) == 0);                                /* page2 slot2 has no group */
    CHECK(lj_static_scope_hits(back, nb, 1, 65) == 1 && lj_static_scope_hits(back, nb, 1, 0) == 0);   /* same slot row on another page */
    CHECK(lj_static_scope_hits(back, nb, 65, 64) == 1 && lj_static_scope_hits(back, nb, 65, 1) == 0); /* solo page = same page */
    CHECK(lj_static_scope_hits(back, nb, 2, 0) == 0);                                 /* Add releases nothing */
    CHECK(lj_static_scope_hits(back, nb, 0, 0) == 0 && lj_static_scope_hits(back, nb, 0, 128) == 0 && lj_static_scope_hits(back, nb, -1, 0) == 0);
    back[0].slot[3].replace = LJ_REP_ALL; CHECK(lj_static_scope_hits(back, nb, 3, 127) == 1);

    /* validator: out-of-range slot / page / scope */
    back[0].slot[4].replace = 99;      CHECK(lj_statics_validate(back, nb) != NULL); back[0].slot[4].replace = 0;
    back[0].n = 0;                     CHECK(lj_statics_validate(back, nb) != NULL); back[0].n = 65; CHECK(lj_statics_validate(back, nb) != NULL); back[0].n = 20;
    back[0].slot[4].level_pct = 101;   CHECK(lj_statics_validate(back, nb) != NULL); back[0].slot[4].level_pct = 0;
    CHECK(lj_statics_validate(back, 65) != NULL);

    /* loader: ME-77 and ME-30 rows */
    lj_statics_free(back, nb); back = NULL;
    lj_put(P1, "LJ4 statics 1\npage 1 name=a n=64\nslot 1 name=s level=100\n");      CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == 0 && nb == 1 && back[0].n == 64);
    lj_statics_free(back, nb); back = NULL;
    lj_put(P1, "LJ4 statics 1\npage 1 name=a n=65\n");                                CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1 && nb == 0 && !back);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 name=s level=356\n");            CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1 && strstr(err, "out of range"));
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 kind=link link=4294967297,0\n"); CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 kind=link link=4,999\n");        CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 groups=-1\n");                   CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1);
    /* ME-171 (a), (c): a slot listed twice is refused (it leaked the first scene); groups=2^64 is ERANGE, never all 64 groups */
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 3 kind=recorded\nslot 3 kind=recorded\n"); CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1 && strstr(err, "twice") && !back);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 3 name=x\npage 2 name=b\nslot 3 name=y\n");  CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == 0 && nb == 2); lj_statics_free(back, nb);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 groups=18446744073709551616\n");       CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1 && strstr(err, "out of range"));
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 groups=18446744073709551615\n");       CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == 0 && nb == 1 && back[0].slot[0].groups == UINT64_MAX); lj_statics_free(back, nb);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 1 groups=+5\n");                        CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 statics 1\npage 1 name=a\nslot 65 name=s\n");                     CHECK(lj_statics_load(P1, &back, &nb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 statics 1\ngroup 65 name=x\n");                                   CHECK(lj_statics_groups_load(P1, &gb, err, sizeof err) == -1);
    lj_statics_free(pages, n);

    /* ME-91 / ME-30: Main.Ini [STATICS2] SlotsVisible 64 is taken, 65 clamps to 64 (recorded), absent = 20 */
    {   lj_ini *ini = lj_ini_new();
        lj_static_page *pg = NULL, *at0;
        lj_static_slot *pinned;
        int np = 0, k;
        CHECK(lj_statics_ini_visible(ini) == 20 && lj_statics_ini_visible(NULL) == 20);
        lj_ini_seti(ini, "STATICS2", "SlotsVisible", 64); CHECK(lj_statics_ini_visible(ini) == 64);
        lj_ini_seti(ini, "STATICS2", "SlotsVisible", 65); CHECK(lj_statics_ini_visible(ini) == 64);
        lj_ini_seti(ini, "STATICS2", "SlotsVisible", 0);  CHECK(lj_statics_ini_visible(ini) == 1);
        lj_ini_free(ini);
        /* the pages array never moves after its first page (a slot the engine holds stays valid); insert / delete / swap move pages inside it */
        CHECK(lj_statics_add_page(&pg, &np, "A") == 0);
        at0 = pg; pinned = &pg[0].slot[3];
        for (k = 1; k < LJ_STATIC_PAGES_MAX; k++) CHECK(lj_statics_add_page(&pg, &np, "x") == k);
        CHECK(pg == at0 && pinned == &pg[0].slot[3] && np == LJ_STATIC_PAGES_MAX && lj_statics_add_page(&pg, &np, "y") == -1);
        CHECK(lj_statics_delete_page(pg, &np, 1) == 0 && np == LJ_STATIC_PAGES_MAX - 1);
        CHECK(lj_statics_insert_page(&pg, &np, 0, "B") == 0 && !strcmp(pg[0].name, "B") && !strcmp(pg[1].name, "A") && pg[0].n == 20);
        CHECK(lj_statics_swap_pages(pg, np, 0, 1) == 0 && !strcmp(pg[0].name, "A") && lj_statics_swap_pages(pg, np, 0, np) == -1);
        CHECK(lj_statics_insert_page(&pg, &np, np + 1, "z") == -1 && lj_statics_delete_page(pg, &np, np) == -1);
        lj_statics_free(pg, np);
    }
}

static void lj_test_bgcue(void)
{
    lj_bgcue *a = lj_bgcue_new_rec(), b;
    char err[256];
    a->number = 3; strcpy(a->name, lj_e9); a->transparent = 1;
    CHECK(lj_bgcue_add_sequence(a, 12) == 1 && lj_bgcue_add_sequence(a, 99999999) == 2);
    a->slot[1].mode = LJ_BG_NOLOOP; a->slot[1].enabled = 0;
    CHECK(lj_bgcue_save(P1, a) == 0 && lj_bgcue_load(P1, &b, err, sizeof err) == 0 && memcmp(a, &b, sizeof b) == 0);
    CHECK(lj_bgcue_save(P2, &b) == 0 && lj_same_bytes(P1, P2));
    lj_put(P1, "LJ4 bgc 1\nnum=1\nslot 1 seq=5 on=2\n");          CHECK(lj_bgcue_load(P1, &b, err, sizeof err) == -1);
    lj_put(P1, "LJ4 bgc 1\nnum=1\nslot 1 seq=100000000\n");       CHECK(lj_bgcue_load(P1, &b, err, sizeof err) == -1);
    lj_put(P1, "LJ4 bgc 1\nnum=4294967297\n");                    CHECK(lj_bgcue_load(P1, &b, err, sizeof err) == -1);
    lj_put(P1, "LJ4 bgc 1\nnum=1\nslot 6 seq=1\n");               CHECK(lj_bgcue_load(P1, &b, err, sizeof err) == -1);
    lj_bgcue_free(a);
}

static void lj_test_groups(void)
{
    static lj_group g[LJ_USER_GROUPS], gb[LJ_USER_GROUPS];
    lj_htp_names h, hb;
    uint8_t sel[LJ_FIXTURES];
    char err[256];
    int i;
    memset(g, 0, sizeof g);
    strcpy(g[0].name, lj_e9); g[0].member[0] = g[0].member[4] = 1;
    strcpy(g[98].name, "Back"); g[98].member[99] = 1;
    for (i = 0; i < LJ_HTP_GROUPS; i++) snprintf(h.name[i], sizeof h.name[i], "HTP group %d", i + 1);
    strcpy(h.name[2], "Salle \xe9t\xe9");
    CHECK(lj_htp_names_save(P1, g, &h) == 0);
    CHECK(lj_groups_load(P1, gb, err, sizeof err) == 0 && memcmp(g, gb, sizeof g) == 0);       /* the W1 reader skips the htp lines */
    CHECK(lj_htp_names_load(P1, &hb, err, sizeof err) == 0 && strcmp(hb.name[2], "Salle \xe9t\xe9") == 0 && strcmp(hb.name[23], "HTP group 24") == 0);
    CHECK(lj_htp_names_save(P2, gb, &hb) == 0 && lj_same_bytes(P1, P2));
    lj_put(P1, "LJ4 groups 1\nhtp 25 name=x\n"); CHECK(lj_htp_names_load(P1, &hb, err, sizeof err) == -1 && strcmp(hb.name[2], "HTP group 3") == 0);
    memset(sel, 0, sizeof sel); sel[50] = 1;
    CHECK(lj_group_select(&g[0], sel, 1) == 2 && sel[0] && sel[4] && sel[50]);         /* inclusive adds */
    CHECK(lj_group_select(&g[0], sel, 0) == 2 && sel[0] && sel[4] && !sel[50]);        /* exclusive replaces */
}

static void lj_test_presets_palettes(void)
{
    static lj_preset p[LJ_PRESETS], pb[LJ_PRESETS];
    static uint8_t val[LJ_CHANS], st[LJ_CHANS];
    lj_palette_list l = { 0 }, lb = { 0 };
    char err[256];
    memset(p, 0, sizeof p);
    p[0].used = 1; p[0].pan = -32768; p[0].tilt = 32767;                              /* ME-22: one 16-bit value each */
    p[98].used = 1; p[98].pan = 1234; p[98].tilt = -5;
    CHECK(lj_presets_save(P1, p) == 0 && lj_presets_load(P1, pb, err, sizeof err) == 0 && memcmp(p, pb, sizeof p) == 0);
    CHECK(lj_presets_save(P2, pb) == 0 && lj_same_bytes(P1, P2));
    lj_put(P1, "LJ4 presets 1\npreset 1 pan=40000 tilt=0\n"); CHECK(lj_presets_load(P1, pb, err, sizeof err) == -1 && !pb[0].used);
    lj_put(P1, "LJ4 presets 1\npreset 100 pan=1 tilt=0\n");   CHECK(lj_presets_load(P1, pb, err, sizeof err) == -1);

    val[0] = 255; st[0] = LJ_FADE; val[2047] = 1; st[2047] = LJ_SNAP;
    CHECK(lj_palettes_add(&l, lj_e9, val, st) == 0 && lj_palettes_add(&l, "two", val, st) == 1);
    CHECK(lj_palettes_save(P1, &l) == 0 && lj_palettes_load(P1, &lb, err, sizeof err) == 0 && lb.n == 2 && strcmp(lb.e[0].name, lj_e9) == 0 && lb.e[1].val[2047] == 1);
    CHECK(lj_palettes_save(P2, &lb) == 0 && lj_same_bytes(P1, P2));
    lj_put(P1, "LJ4 palette 1\nentry 2 name=x\n"); CHECK(lj_palettes_load(P1, &lb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 palette 1\nentry 1 name=x\nch 1:1=256:S\n"); CHECK(lj_palettes_load(P1, &lb, err, sizeof err) == -1);
    lj_palettes_free(&l); lj_palettes_free(&lb);
}

static void lj_test_patch_default_notes(void)
{
    static lj_patch_ch p[LJ_CHANS], pb[LJ_CHANS];
    static uint8_t out[LJ_CHANS], ob[LJ_CHANS];
    static char big[LJ_NOTES_CAP + 16], nb[LJ_NOTES_CAP];
    lj_scene sc;
    char err[256];
    lj_patch_init(p);
    p[5].src_kind = LJ_SRC_DMX_OUT; p[5].src_link = 1; p[5].src_ch = 1; p[5].invert = 1; p[5].min = 10; p[5].max = 200;
    CHECK(lj_patch_htp_set(p, 2047, 24, 1) == 0 && lj_patch_htp_set(p, 2047, 1, 1) == 0 && p[2047].htp_mask == 0x800001u);
    CHECK(lj_patch_htp_set(p, 2047, 1, 0) == 0 && p[2047].htp_mask == 0x800000u);
    CHECK(lj_patch_htp_set(p, 2048, 1, 1) == -1 && lj_patch_htp_set(p, 0, 25, 1) == -1 && lj_patch_htp_set(p, 0, 0, 1) == -1);
    CHECK(lj_patch_save("test_model_w2" LJ_PATCH_EXT, p) == 0 && lj_patch_load("test_model_w2" LJ_PATCH_EXT, pb, err, sizeof err) == 0 && memcmp(p, pb, sizeof p) == 0);
    CHECK(lj_patch_save(P2, pb) == 0 && lj_same_bytes("test_model_w2" LJ_PATCH_EXT, P2));
    remove("test_model_w2" LJ_PATCH_EXT);
    lj_put(P1, "LJ4 patch 1\nch 1:1 htp=16777216\n");   CHECK(lj_patch_load(P1, pb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 patch 1\nch 1:1 src=out sl=5 sc=1\n"); CHECK(lj_patch_load(P1, pb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 patch 1\nch 1:513 inv=1\n");        CHECK(lj_patch_load(P1, pb, err, sizeof err) == -1);
    lj_put(P1, "LJ4 patch 1\nch 1:1 max=256\n");        CHECK(lj_patch_load(P1, pb, err, sizeof err) == -1);
    CHECK(lj_patch_clear(pb) == 0 && lj_patch_is_default(&pb[0]));

    /* Default DMX output: Use Current Scene merges Snap/Fade channels over the old defaults; presets/macros refuse */
    memset(out, 0, sizeof out); out[100] = 9; out[0] = 1;
    lj_scene_init(&sc); sc.val[0] = 255; sc.state[0] = LJ_SNAP; sc.val[1] = 128; sc.state[1] = LJ_FADE; sc.val[100] = 0; sc.state[100] = LJ_OFF;
    CHECK(lj_defaultout_scene_blocked(&sc) == 0 && lj_defaultout_use_scene(out, &sc) == 0 && out[0] == 255 && out[1] == 128 && out[100] == 9);
    sc.preset[3] = 1; CHECK(lj_defaultout_scene_blocked(&sc) == 1 && lj_defaultout_use_scene(out, &sc) == -1 && out[0] == 255);
    sc.preset[3] = 0; sc.macro[7].type = 2; CHECK(lj_defaultout_use_scene(out, &sc) == -1);
    CHECK(lj_defaultout_save(P1, out) == 0 && lj_defaultout_load(P1, ob, err, sizeof err) == 0 && memcmp(out, ob, sizeof out) == 0);
    CHECK(lj_defaultout_save(P2, ob) == 0 && lj_same_bytes(P1, P2));
    lj_put(P1, "LJ4 defaultout 1\nch 1:1=999\n");      CHECK(lj_defaultout_load(P1, ob, err, sizeof err) == -1);
    lj_defaultout_clear(out); CHECK(out[0] == 0 && out[1] == 0 && out[100] == 0);

    /* Notes: bytes as typed (CR LF kept, 0xE9 kept), missing = empty, over the cap = -1 */
    CHECK(lj_notes_save(P1, "Caf\xe9\r\nline 2\n") == 0 && lj_notes_load(P1, nb, sizeof nb) == 0 && strcmp(nb, "Caf\xe9\r\nline 2\n") == 0);
    CHECK(lj_notes_save(P2, nb) == 0 && lj_same_bytes(P1, P2));
    remove(P1);
    CHECK(lj_notes_load(P1, nb, sizeof nb) == 0 && nb[0] == 0);
    memset(big, 'x', LJ_NOTES_CAP); big[LJ_NOTES_CAP] = 0;
    CHECK(lj_notes_save(P1, big) == 0 && lj_notes_load(P1, nb, sizeof nb) == -1 && strlen(nb) == LJ_NOTES_CAP - 1);
}

static void lj_test_w1_sites(void)   /* ME-77: the five W1 sites fail the line instead of aliasing */
{
    lj_cue c;
    lj_show *show = lj_show_new();
    char err[256];
    lj_put(P1, "LJ4 cue 1\nnum=1\ncuetime=65636\n");                CHECK(lj_cue_load(P1, &c, err, sizeof err) == -1 && strstr(err, "out of range"));
    lj_put(P1, "LJ4 cue 1\nnum=1\nslot 1 seq=1 spt=257\n");         CHECK(lj_cue_load(P1, &c, err, sizeof err) == -1);
    lj_put(P1, "LJ4 cue 1\nnum=1\nslot 1 seq=1 chase=60001\n");     CHECK(lj_cue_load(P1, &c, err, sizeof err) == -1);
    lj_put(P1, "LJ4 cue 1\nnum=1\nslot 1 seq=1 on=1 trig=auto dir=fwd spt=0 atpt=0 tol=0\n");   /* a v0.1.0 cue (Add Sequence to Current Cue wrote 0): loads as 1 */
    CHECK(lj_cue_load(P1, &c, err, sizeof err) == 0 && c.slot[0].scenes_per_trig == 1 && c.slot[0].audio_trigs_per_trig == 1);
    lj_put(P1, "LJ4 cue 1\nnum=1\ncuetime=250\nslot 1 seq=1 spt=255 link=7\n"); CHECK(lj_cue_load(P1, &c, err, sizeof err) == 0 && c.cue_time_pct == 250 && c.slot[0].scenes_per_trig == 255 && c.slot[0].link_cue == 7);
    lj_put(P1, "LJ4 config 1\nfx 1 kind=builtin id=0 link=257 addr=1\n");   CHECK(lj_config_load(P1, show, err, sizeof err) == -1);
    lj_put(P1, "LJ4 config 1\nfx 1 kind=builtin id=0 link=1 addr=65537\n"); CHECK(lj_config_load(P1, show, err, sizeof err) == -1);
    lj_show_free(show);
}

int lj_test_model_w2_records(int argc, char **argv);
int lj_test_model_w2_records(int argc, char **argv)
{
    (void)argc; (void)argv;
    lj_test_cuepages();
    lj_test_statics2();
    lj_test_bgcue();
    lj_test_groups();
    lj_test_presets_palettes();
    lj_test_patch_default_notes();
    lj_test_w1_sites();
    remove(P1); remove(P2);
    printf("test_model_w2_records: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
