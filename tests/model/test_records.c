/* test_records.c - `lj4evr_tests test_model_records`: every record kind of design
 * 5.2 round-trips through a temp library (write, read, field-exact compare), the
 * validators reject the brief's cases (scene_ms > 60000, slot seq 0 with enabled,
 * link 5, loops in random), a bad line is rejected with its item number, CP1252
 * bytes 128-255 survive in names, quotes are doubled, and a 999-scene sequence
 * loads in under 200 ms. The temp tree is test_model_tmp/ under the cwd. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_clock.h"
#include "model/lj_library.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_cue.h"
#include "model/lj_cuepage.h"
#include "model/lj_cuelist.h"
#include "model/lj_bgcue.h"
#include "model/lj_statics.h"
#include "model/lj_patch.h"
#include "model/lj_groups.h"
#include "model/lj_presets.h"
#include "model/lj_palettes.h"
#include "model/lj_genmacro.h"
#include "model/lj_show.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char lj_name8[] = "Caf\xe9 \"Bl\xe5\" \xc6\xd8\xff";   /* CP1252 bytes 128-255 and a quoted word */

static void lj_write_text(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static void lj_fill_scene(lj_scene *s, int seed)
{
    int c;
    lj_scene_init(s);
    s->scene_ms = 1000 + (uint32_t)seed; s->fade_ms = 50 + (uint32_t)seed;
    for (c = 0; c < 60; c++) {                       /* 60 programmed channels across the four links */
        int code = (c * 37 + seed) % LJ_CHANS;
        s->val[code] = (uint8_t)(c * 7 + seed);
        s->state[code] = (uint8_t)(c % 2 ? LJ_SNAP : LJ_FADE);
    }
    s->preset[4] = 3;
    s->macro[6].type = 1; s->macro[6].amp_pan = 127; s->macro[6].amp_tilt = 127; s->macro[6].cycle_pan = 40; s->macro[6].cycle_tilt = 40;
    s->macro[6].step = 512; s->macro[6].fstate[0] = 2; s->macro[6].fstate[1] = 1;
}

static int lj_seq_equal(const lj_sequence *a, const lj_sequence *b)
{
    return a->number == b->number && strcmp(a->name, b->name) == 0 && strcmp(a->type, b->type) == 0 && a->n == b->n
        && a->sync_times == b->sync_times && a->global_scene_time == b->global_scene_time && a->global_fade_time == b->global_fade_time
        && memcmp(a->scenes, b->scenes, (size_t)a->n * sizeof *a->scenes) == 0;
}

static void lj_test_sequence(const lj_library *lib)
{
    lj_sequence *a = lj_sequence_new_rec(), b = { 0 };
    char path[LJ_PATH_CAP], err[256];
    uint64_t t0;
    int i;
    a->number = 12; strcpy(a->name, lj_name8); strcpy(a->type, "chase"); a->sync_times = 1;
    lj_sequence_alloc(a, 3);
    for (i = 0; i < 3; i++) lj_fill_scene(&a->scenes[i], i);
    CHECK(lj_library_record_path(lib, LJ_REC_SEQ, 12, path, sizeof path) == 0 && strstr(path, "00000012.seq"));
    CHECK(lj_sequence_save(path, a) == 0);
    CHECK(lj_sequence_load(path, &b, err, sizeof err) == 0 && err[0] == 0);
    CHECK(lj_seq_equal(a, &b));
    CHECK(strcmp(b.name, lj_name8) == 0);
    {                                               /* the quote is doubled on disk */
        FILE *f = fopen(path, "rb"); char buf[4096]; size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0; if (f) fclose(f); buf[n] = 0;
        CHECK(strstr(buf, "name=\"Caf\xe9 \"\"Bl\xe5\"\" \xc6\xd8\xff\"") != NULL);
    }
    a->scenes[1].scene_ms = 60001;
    CHECK(lj_sequence_validate(a) != NULL);
    a->scenes[1].scene_ms = 60000;
    CHECK(lj_sequence_validate(a) == NULL);
    lj_sequence_free_scenes(&b);

    lj_sequence_alloc(a, LJ_SCENES_MAX);              /* 999 scenes, 60 channels each: load < 200 ms */
    for (i = 0; i < LJ_SCENES_MAX; i++) lj_fill_scene(&a->scenes[i], i);
    CHECK(lj_sequence_save(path, a) == 0);
    t0 = lj_clock_us();
    CHECK(lj_sequence_load(path, &b, err, sizeof err) == 0);
    t0 = lj_clock_us() - t0;
    printf("test_model_records: 999-scene sequence loaded in %u us\n", (unsigned)t0);
    CHECK(t0 < 200000);
    CHECK(lj_seq_equal(a, &b));
    lj_sequence_free_scenes(&b);

    lj_write_text(path, "LJ4 seq 1\nnum=1\nscenes=1\nscene st=abc ft=50\n");   /* a bad line: rejected with its item number */
    CHECK(lj_sequence_load(path, &b, err, sizeof err) == -1 && strstr(err, "item 3") != NULL);
    lj_write_text(path, "LJ4 seq 1\nnum=1\nscenes=2\nscene st=1000 ft=50\n");
    CHECK(lj_sequence_load(path, &b, err, sizeof err) == -1 && strstr(err, "scene count") != NULL);
    lj_write_text(path, "LJ4 seq 1\nnum=1\nfuture=1\nscenes=1\nscene st=1000 ft=50 newkey=7\nch 1:1=10:S\n");   /* unknown keys skipped */
    CHECK(lj_sequence_load(path, &b, err, sizeof err) == 0 && b.n == 1 && b.scenes[0].val[0] == 10 && b.scenes[0].state[0] == LJ_SNAP);
    lj_sequence_free_scenes(&b);
    lj_sequence_free(a);
}

static void lj_test_cue(const lj_library *lib)
{
    lj_cue *a = lj_cue_new_rec(), b;
    char path[LJ_PATH_CAP], err[256];
    a->number = 1; strcpy(a->name, "Very Busy Cue"); a->cue_time_pct = 250; a->macro_amp_pct = 50; a->save_macro_values = 1; a->transparent = 1; a->bg_cue = 3; a->gen_macro = 4;
    CHECK(lj_cue_add_sequence(a, 12, 0) == 1 && lj_cue_add_sequence(a, 7, 2) == 2 && lj_cue_find_free(a) == 3);
    a->slot[1].trig = LJ_TRIG_MANUAL; a->slot[1].dir = LJ_DIR_BOUNCE; a->slot[1].first = 2; a->slot[1].last = 5; a->slot[1].loops = 3;
    a->slot[1].end_action = LJ_END_OUT; a->slot[1].time_pct = 250; a->slot[1].chase_ms = 120; a->slot[1].scenes_per_trig = 2; a->slot[1].trig_on_load = 1;
    a->slot[11].seq = 99; a->slot[11].enabled = 0; a->slot[11].end_action = LJ_END_LINK; a->slot[11].link_cue = 5; a->slot[11].scenes_per_trig = a->slot[11].audio_trigs_per_trig = 1;
    CHECK(lj_cue_validate(a) == NULL);
    lj_library_record_path(lib, LJ_REC_CUE, 1, path, sizeof path);
    CHECK(lj_cue_save(path, a) == 0);
    CHECK(lj_cue_load(path, &b, err, sizeof err) == 0);
    CHECK(memcmp(a, &b, sizeof *a) == 0);
    b.slot[3].enabled = 1;                             /* seq 0 with enabled */
    CHECK(lj_cue_validate(&b) != NULL);
    b.slot[3].enabled = 0; b.slot[1].dir = LJ_DIR_RANDOM;   /* loop count in random */
    CHECK(lj_cue_validate(&b) != NULL);
    lj_cue_free(a);
}

static void lj_test_cuelist(const lj_library *lib)
{
    lj_cuelist *a = lj_cuelist_new_rec(), b = { 0 };
    lj_cl_line l = { 0 };
    char path[LJ_PATH_CAP], err[256];
    a->number = 2; strcpy(a->name, "Show 1"); a->tracking = 1; a->mark[0] = 2;
    l.manual = LJ_CL_MANUAL; l.cmd = 23; l.param_ms = 5000; strcpy(l.param, "a \"b\""); l.cue = 3; strcpy(l.comment, "first, go"); lj_cuelist_insert(a, 0, &l);
    memset(&l, 0, sizeof l); l.trig_mask = 0x803; lj_cuelist_insert(a, 99, &l);
    memset(&l, 0, sizeof l); l.cmd = 26; l.param_ms = 1; lj_cuelist_insert(a, 1, &l);
    CHECK(a->n == 3 && a->lines[0].line == 1 && a->lines[1].cmd == 26 && a->lines[2].line == 3);
    CHECK(lj_cuelist_validate(a) == NULL);
    lj_library_record_path(lib, LJ_REC_CL, 2, path, sizeof path);
    CHECK(lj_cuelist_save(path, a) == 0);
    CHECK(lj_cuelist_load(path, &b, err, sizeof err) == 0);
    CHECK(b.n == 3 && b.number == 2 && strcmp(b.name, "Show 1") == 0 && b.tracking == 1 && b.mark[0] == 2);
    CHECK(memcmp(a->lines, b.lines, 3 * sizeof *a->lines) == 0);
    CHECK(lj_cuelist_delete(&b, 1) == 0 && b.n == 2 && b.lines[1].line == 2 && b.lines[1].trig_mask == 0x803);
    CHECK(lj_cuelist_delete(&b, 5) == -1);
    free(b.lines);
    lj_cuelist_free(a);
}

static void lj_test_bgcue(const lj_library *lib)
{
    lj_bgcue *a = lj_bgcue_new_rec(), b;
    char path[LJ_PATH_CAP], err[256];
    int i;
    a->number = 5; strcpy(a->name, "BG"); a->transparent = 1;
    for (i = 0; i < 7; i++) CHECK(lj_bgcue_add_sequence(a, (uint32_t)(10 + i)) == (i < 5 ? i + 1 : 5));   /* the 6th and 7th overwrite slot 5 */
    CHECK(a->slot[4].seq == 16);
    a->slot[2].mode = LJ_BG_NOLOOP; a->slot[3].enabled = 0;
    lj_library_record_path(lib, LJ_REC_BG, 5, path, sizeof path);
    CHECK(lj_bgcue_save(path, a) == 0 && lj_bgcue_load(path, &b, err, sizeof err) == 0 && memcmp(a, &b, sizeof *a) == 0);
    b.slot[1].seq = 0;
    CHECK(lj_bgcue_validate(&b) != NULL);
    lj_bgcue_free(a);
}

static void lj_test_statics(const lj_library *lib)
{
    lj_static_page *pages = NULL, *back = NULL;
    lj_scene scene;
    char path[LJ_PATH_CAP], err[256];
    int n = 0, nb = 0;
    CHECK(lj_statics_add_page(&pages, &n, "Page 1") == 0 && lj_statics_add_page(&pages, &n, lj_name8) == 1);
    lj_fill_scene(&scene, 3);
    strcpy(pages[0].slot[0].name, "Strobe A"); pages[0].slot[0].activation = LJ_ACT_FLASH; pages[0].slot[0].replace = LJ_REP_SOLO_GROUP;
    pages[0].slot[0].fade_ms = 300; pages[0].slot[0].level_pct = 80; pages[0].slot[0].groups = 0x8000000000000005ull;
    CHECK(lj_static_slot_record(&pages[0].slot[0], &scene) == 0 && pages[0].slot[0].kind == LJ_STATIC_RECORDED);
    strcpy(pages[1].slot[19].name, "Link"); pages[1].slot[19].kind = LJ_STATIC_LINK; pages[1].slot[19].link_seq = 12; pages[1].slot[19].link_scene = 2;
    CHECK(lj_statics_validate(pages, n) == NULL);
    lj_library_path(lib, "Statics.dat", path, sizeof path);
    CHECK(lj_statics_save(path, pages, n) == 0);
    CHECK(lj_statics_load(path, &back, &nb, err, sizeof err) == 0 && nb == 2);
    CHECK(strcmp(back[1].name, lj_name8) == 0 && back[1].slot[19].kind == LJ_STATIC_LINK && back[1].slot[19].link_seq == 12 && back[1].slot[19].link_scene == 2);
    CHECK(back[0].slot[0].kind == LJ_STATIC_RECORDED && back[0].slot[0].scene && memcmp(back[0].slot[0].scene, &scene, sizeof scene) == 0);
    CHECK(back[0].slot[0].groups == 0x8000000000000005ull && back[0].slot[0].replace == LJ_REP_SOLO_GROUP && back[0].slot[0].fade_ms == 300 && back[0].slot[0].level_pct == 80);
    lj_static_slot_clear(&back[0].slot[0]);
    CHECK(back[0].slot[0].scene == NULL && back[0].slot[0].kind == LJ_STATIC_EMPTY && strcmp(back[0].slot[0].name, "Strobe A") == 0);
    lj_statics_free(back, nb);
    lj_statics_free(pages, n);
}

static void lj_test_config(const lj_library *lib)
{
    lj_show *a = lj_show_new(), *b = lj_show_new();
    lj_fixture fx = { 0 };
    char path[LJ_PATH_CAP], err[256];
    fx.kind = LJ_FX_GS2; strcpy(fx.gs2_file, "My Dimmer.GS2"); fx.link = 2; fx.address = 10; fx.placed = 1; fx.icon_x = -5; fx.icon_y = 80; fx.tab = 1; fx.intensity_group = 3;
    strcpy(fx.user_name, "Dim \"one\""); fx.invert_pan = 1; fx.link_strobe_gm = 1;
    CHECK(lj_show_add_fixture(a, &fx) == 0);
    fx.address = 11; memset(fx.user_name, 0, sizeof fx.user_name);
    CHECK(lj_show_add_fixture(a, &fx) == 1);
    strcpy(a->submaster_name[2], "Wash"); a->submaster_mode[2] = 2;
    a->default_out[lj_addr_code(2, 10)] = 255; a->default_out[LJ_CHANS - 1] = 1;
    strcpy(a->user_group[0].name, "Front"); a->user_group[0].member[0] = 1; a->user_group[0].member[1] = 1;
    CHECK(lj_config_validate(a) == NULL);
    CHECK(lj_show_save_config(a, lib) == 0);
    CHECK(lj_show_load(b, lib) == 0);
    CHECK(memcmp(a->fixture, b->fixture, sizeof a->fixture) == 0);
    CHECK(memcmp(a->default_out, b->default_out, sizeof a->default_out) == 0);
    CHECK(memcmp(a->user_group, b->user_group, sizeof a->user_group) == 0);
    CHECK(memcmp(a->submaster_name, b->submaster_name, sizeof a->submaster_name) == 0 && b->submaster_mode[2] == 2);
    CHECK(lj_show_fixture_count(b) == 2 && lj_show_fixture_base(b, 1) == lj_addr_code(2, 11) && lj_show_fixture_at(b, lj_addr_code(2, 10)) == 0 && lj_show_fixture_at(b, 0) == -1);
    b->fixture[0].link = 5;
    CHECK(lj_config_validate(b) != NULL);
    b->fixture[0].link = 2; b->fixture[0].address = 11;     /* overlap */
    CHECK(lj_config_validate(b) != NULL);
    b->fixture[0].address = 10; b->fixture[0].gs2_file[0] = 0;   /* profile unknown */
    CHECK(lj_config_validate(b) != NULL);
    lj_show_remove_fixture(b, 0);
    CHECK(lj_show_fixture_count(b) == 1 && lj_config_validate(b) == NULL);
    /* W4 integration (F41): a Config.dat saved with Save Anyway (no address, a shared channel) loads and keeps every fixture */
    a->fixture[1].address = 0;
    CHECK(lj_show_save_config(a, lib) == 0 && lj_show_load(b, lib) == 0 && lj_show_fixture_count(b) == 2);
    CHECK(b->fixture[1].address == 0 && lj_show_fixture_base(b, 1) == -1 && lj_config_validate(b) != NULL);
    a->fixture[1].address = 10;                                  /* shares channel 2:10 with fixture 0 */
    CHECK(lj_show_save_config(a, lib) == 0 && lj_show_load(b, lib) == 0 && lj_show_fixture_count(b) == 2 && lj_config_validate(b) != NULL);
    b->fixture[1].link = 5;                                      /* a hard error still refuses the file */
    CHECK(lj_show_save_config(b, lib) == 0 && lj_show_load(b, lib) == -1);
    a->fixture[1].address = 11;
    CHECK(lj_show_save_config(a, lib) == 0 && lj_show_load(b, lib) == 0);
    lj_show_remove_fixture(b, 0);
    /* ME-107: a 63-char gs2= (the gs2_file cap) no longer drops ipan / swap / placed after it */
    memset(b->fixture[1].gs2_file, 'G', sizeof b->fixture[1].gs2_file - 1); b->fixture[1].gs2_file[sizeof b->fixture[1].gs2_file - 1] = 0;
    b->fixture[1].invert_pan = 1; b->fixture[1].swap_pt = 1; b->fixture[1].placed = 1;
    CHECK(lj_show_save_config(b, lib) == 0);
    CHECK(lj_show_load(a, lib) == 0);
    CHECK(strlen(a->fixture[1].gs2_file) == 63 && a->fixture[1].invert_pan == 1 && a->fixture[1].swap_pt == 1 && a->fixture[1].placed == 1);
    lj_library_path(lib, "Config.dat", path, sizeof path);
    lj_write_text(path, "LJ4 config 1\nfx 1 kind=gs2 gs2=x.GS2 link=9 addr=1\n");
    CHECK(lj_show_load(b, lib) == -1);                    /* Config.dat exists and is invalid */
    CHECK(lj_config_load(path, b, err, sizeof err) == -1 && strstr(err, "out of range"));   /* W2 ME-77: link=9 fails the line at load (it no longer reaches the validator) */
    lj_library_path(lib, "GroupInfo.dat", path, sizeof path);
    lj_write_text(path, "LJ4 groups 1\ngroup 1 name=X members=1,200\n");
    CHECK(lj_groups_load(path, b->user_group, err, sizeof err) == -1 && strstr(err, "item 1"));
    lj_show_free(a);
    lj_show_free(b);
}

static void lj_test_presets_palettes_patch_macro(const lj_library *lib)
{
    lj_preset p[LJ_PRESETS], q[LJ_PRESETS];
    lj_palette_list pl = { 0 }, pb = { 0 };
    lj_patch_ch patch[LJ_CHANS], patch2[LJ_CHANS];
    lj_genmacro *m = lj_genmacro_new_rec(), *m2 = lj_genmacro_new_rec();
    lj_cue_page pages[3], pages2[3];
    uint8_t val[LJ_CHANS] = { 0 }, state[LJ_CHANS] = { 0 };
    char path[LJ_PATH_CAP], err[256];
    int c;

    memset(p, 0, sizeof p);
    p[0].used = 1; p[0].pan = -32768; p[0].tilt = 32767; p[98].used = 1; p[98].pan = 5;
    lj_library_path(lib, "Presets\\fixture001.pre", path, sizeof path);
    CHECK(lj_presets_save(path, p) == 0 && lj_presets_load(path, q, err, sizeof err) == 0 && memcmp(p, q, sizeof p) == 0);

    val[0] = 255; state[0] = LJ_FADE; val[2047] = 1; state[2047] = LJ_SNAP;
    CHECK(lj_palettes_add(&pl, lj_name8, val, state) == 0 && lj_palettes_add(&pl, "", val, state) == 1 && lj_palettes_add(&pl, "two", val, state) == 2);
    CHECK(lj_palettes_delete(&pl, 1) == 0 && pl.n == 2 && strcmp(pl.e[1].name, "two") == 0);
    CHECK(strcmp(lj_palettes_file_name(LJ_CTL_COLORS), "Color.Pre") == 0 && lj_palettes_file_name(LJ_CTL_LAMP) == NULL);
    lj_library_path(lib, "Palettes\\Color.Pre", path, sizeof path);
    CHECK(lj_palettes_save(path, &pl) == 0 && lj_palettes_load(path, &pb, err, sizeof err) == 0 && pb.n == 2);
    CHECK(memcmp(pl.e, pb.e, 2 * sizeof *pl.e) == 0);
    lj_palettes_free(&pl); lj_palettes_free(&pb);

    lj_patch_init(patch);
    CHECK(lj_patch_is_default(&patch[7]) && lj_patch_validate(patch) == NULL);
    patch[16].src_kind = LJ_SRC_DMX_IN; patch[16].src_link = 1; patch[16].src_ch = 5; patch[16].in_mode = LJ_IN_HTP; patch[16].invert = 1; patch[16].min = 10; patch[16].max = 200; patch[16].htp_mask = 0x800001;
    patch[2047].src_kind = LJ_SRC_DMX_OUT; patch[2047].src_link = 4; patch[2047].src_ch = 511;
    lj_library_path(lib, "patch.lj4p", path, sizeof path);
    CHECK(lj_patch_save(path, patch) == 0 && lj_patch_load(path, patch2, err, sizeof err) == 0 && memcmp(patch, patch2, sizeof patch) == 0);
    patch2[2047].src_ch = 512;                              /* self-patch */
    CHECK(lj_patch_validate(patch2) != NULL);

    m->number = 7; strcpy(m->name, "Sine"); m->restart_on_load = 1;
    c = lj_addr_code(1, 17);
    m->ch[c].shape = 7; m->ch[c].base_on = 1; m->ch[c].base = 100; m->ch[c].amp_pos = 45; m->ch[c].amp_neg = -45; m->ch[c].cycle_ds = 168; m->ch[c].dir = LJ_DIR_REV; m->ch[c].delay = 3;
    lj_library_record_path(lib, LJ_REC_MACRO, 7, path, sizeof path);
    CHECK(strstr(path, "CueMacros") && strstr(path, "00000007.CMA"));
    CHECK(lj_genmacro_save(path, m) == 0 && lj_genmacro_load(path, m2, err, sizeof err) == 0 && memcmp(m, m2, sizeof *m) == 0);
    free(m); free(m2);

    memset(pages, 0, sizeof pages);
    strcpy(pages[0].name, "Page 1"); pages[0].n = 4; pages[0].cue[0] = 1; pages[0].cue[1] = 5; pages[0].cue[2] = 5; pages[0].cue[3] = 9;
    strcpy(pages[1].name, lj_name8);
    lj_library_path(lib, "Cues\\CuePageInfo.Dat", path, sizeof path);
    CHECK(lj_cue_pages_save(path, pages, 2) == 0 && lj_cue_pages_load(path, pages2, 3) == 2 && memcmp(pages, pages2, 2 * sizeof *pages) == 0);
}

int lj_test_model_records(int argc, char **argv);
int lj_test_model_records(int argc, char **argv)
{
    lj_library lib;
    (void)argc; (void)argv;
    lj_library_delete("test_model_tmp", "Records");
    CHECK(lj_library_create("test_model_tmp", "Records", &lib) == 0);
    lj_test_sequence(&lib);
    lj_test_cue(&lib);
    lj_test_cuelist(&lib);
    lj_test_bgcue(&lib);
    lj_test_statics(&lib);
    lj_test_config(&lib);
    lj_test_presets_palettes_patch_macro(&lib);
    printf("test_model_records: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
