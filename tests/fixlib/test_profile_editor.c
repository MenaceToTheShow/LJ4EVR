/* test_profile_editor.c - `lj4evr_tests test_profile_editor` (owner: profile-editor; the contracts seat wrote the first
 * rows). Rows:
 *  - the 11 tab captions, the field table (every row inside the 20,851-byte record, rows grouped by tab in PC1 order,
 *    no two rows overlapping, every DFM control name non-empty);
 *  - New Profile defaults (version 2609, 0x2E, Blackout enable, 32 enabled levels -> byte 0x05 = 31);
 *  - every field of the table written through lj_pe_set_u8 / lj_pe_set_str reads back through the getters and touches
 *    no byte outside its own span (the raw-record rule); ShortStrings clamp and keep the stale tail;
 *  - the dialog's refusals (lj_pe_validate) and the [12 Q9] / exe prompt texts verbatim;
 *  - working copy -> profile -> .Udf export -> import -> equal, for every field (needs the gs2 track's lj_gs2_parse /
 *    lj_udf_export / lj_udf_parse; prints SKIP while they are the W1 stubs);
 *  - ME-32: Config.dat chcap / chgm round trip, out-of-range lines rejected; Customize Fixture Setup.<slot>.pal round
 *    trip (design 6.5). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixlib/lj_gs2.h"
#include "fixlib/lj_profile_editor.h"
#include "model/lj_show.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int lj_t_size(const lj_pe_field *f)
{
    switch (f->kind) {
    case LJ_PEF_I32: return 4;
    case LJ_PEF_STR: return f->n + 1;
    case LJ_PEF_CHARS: case LJ_PEF_U8S: return f->n;
    case LJ_PEF_DYN: return 17 * f->n;
    case LJ_PEF_STATIC: return 87 * f->n;
    case LJ_PEF_PAL: return 57 * f->n;
    case LJ_PEF_LEVELS: return 16 * f->n;
    default: return 1;
    }
}

static void write_file(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

/* a distinct value per (field, byte) so a misplaced write shows */
static int pat(int field, int k) { return (field * 7 + k * 13 + 1) & 0xFF; }
static int is_text(const lj_pe_field *f) { return f->kind == LJ_PEF_STR || f->kind == LJ_PEF_CHARS; }

static void test_fields(lj_profile_edit *ed, lj_profile_edit *before)
{
    int i, k, b;
    char got[80], want[80];
    for (i = 0; i < lj_pe_nfields; i++) {
        const lj_pe_field *f = &lj_pe_fields[i];
        int sz = lj_t_size(f), bad = 0;
        CHECK(lj_pe_field_size(i) == sz);
        memcpy(before, ed, sizeof *ed);
        if (is_text(f)) {
            snprintf(want, sizeof want, "F%02d%s", i, f->kind == LJ_PEF_CHARS ? "{\\rtf1 notes}" : "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyz");
            if (f->kind == LJ_PEF_STR && (int)strlen(want) > f->n) want[f->n] = 0;   /* clamped */
            CHECK(lj_pe_set_str(ed, i, 0, want) == 0);
            CHECK(lj_pe_get_str(ed, i, 0, got, sizeof got) == 0 && strcmp(got, want) == 0);
        } else {
            for (k = 0; k < sz; k++) CHECK(lj_pe_set_u8(ed, i, k, pat(i, k)) == 0);
            for (k = 0; k < sz; k++) if (lj_pe_get_u8(ed, i, k) != pat(i, k)) bad = 1;
            CHECK(!bad);
            CHECK(lj_pe_set_u8(ed, i, sz, 1) == -1 && lj_pe_set_u8(ed, i, 0, 256) == -1 && lj_pe_get_u8(ed, i, -1) == -1);
            if (f->kind == LJ_PEF_DYN || f->kind == LJ_PEF_STATIC || f->kind == LJ_PEF_PAL || f->kind == LJ_PEF_LEVELS) {   /* the record names */
                CHECK(lj_pe_set_str(ed, i, 0, "Name") == 0 && lj_pe_get_str(ed, i, 0, got, sizeof got) == 0 && strcmp(got, "Name") == 0);
                CHECK(lj_pe_set_str(ed, i, f->n, "x") == -1);
            }
        }
        CHECK(ed->dirty == 1);
        for (b = 0; b < LJ_GS2_REC_SIZE; b++)
            if ((b < f->off || b >= f->off + sz) && ed->raw[b] != before->raw[b]) { CHECK(0); printf("  %s wrote 0x%04X outside its span\n", f->control, b); break; }
    }
    /* ShortString clamp + stale tail: "Generic" then "Red" keeps "Red" + "eric" bytes (16 s3: "Greeen", "Reden") */
    i = lj_pe_field_find("eAuthor");
    CHECK(lj_pe_set_str(ed, i, 0, "Generic") == 0 && lj_pe_set_str(ed, i, 0, "Red") == 0);
    CHECK(ed->raw[0x43] == 3 && memcmp(ed->raw + 0x44, "Rederic", 7) == 0);
    CHECK(lj_pe_get_str(ed, i, 0, got, sizeof got) == 0 && strcmp(got, "Red") == 0);
    ed->raw[0x43] = 200;                                       /* a length byte past the width reads clamped, as the oracle's sstr() */
    CHECK(lj_pe_get_str(ed, i, 0, got, sizeof got) == 0 && strlen(got) == 30);
    i = lj_pe_field_find("Notes");                             /* notes over 4096 bytes are refused, never truncated */
    {
        static char big[LJ_GS2_NOTES + 1];
        memset(big, 'n', LJ_GS2_NOTES); big[LJ_GS2_NOTES] = 0;
        CHECK(lj_pe_set_str(ed, i, 0, big) == -1);
        big[LJ_GS2_NOTES - 1] = 0;
        CHECK(lj_pe_set_str(ed, i, 0, big) == 0);
    }
}

static void test_validate(lj_profile_edit *ed)
{
    int man = lj_pe_field_find("cbManufacturer");
    lj_pe_begin(ed, NULL);
    CHECK(strcmp(lj_pe_validate(ed), LJ_PE_ERR_MANUFACTURER) == 0);
    CHECK(lj_pe_set_str(ed, man, 0, "Generic") == 0 && lj_pe_validate(ed) == NULL);
    lj_pe_set_u8(ed, lj_pe_field_find("cbStrobe"), 0, 1); lj_pe_set_u8(ed, lj_pe_field_find("cbStrobe"), 1, 32);
    CHECK(strcmp(lj_pe_validate(ed), LJ_PE_ERR_OFFSET) == 0);
    lj_pe_set_u8(ed, lj_pe_field_find("cbStrobe"), 1, 31);
    CHECK(lj_pe_validate(ed) == NULL);
    lj_pe_set_u8(ed, lj_pe_field_find("cbPanTilt16"), 0, 1); lj_pe_set_u8(ed, lj_pe_field_find("ePanOfs"), 0, 4); lj_pe_set_u8(ed, lj_pe_field_find("eTiltOfs"), 0, 4);
    CHECK(strcmp(lj_pe_validate(ed), LJ_PE_ERR_IDENTICAL) == 0);
    lj_pe_set_u8(ed, lj_pe_field_find("eTiltOfs"), 0, 31);      /* 16 bit: MSB 31 + LSB 32 does not fit */
    CHECK(strcmp(lj_pe_validate(ed), LJ_PE_ERR_OFFSET) == 0);
    lj_pe_set_u8(ed, lj_pe_field_find("eTiltOfs"), 0, 6);
    CHECK(lj_pe_validate(ed) == NULL);
    memset(ed->raw + 0x62, 'x', LJ_GS2_NOTES);                 /* no NUL inside the notes block */
    CHECK(strcmp(lj_pe_validate(ed), LJ_PE_ERR_NOTES) == 0);
    /* the prompt / refusal table: verbatim exe literals [12 Q9, 13 exe strings] */
    CHECK(strcmp(LJ_PE_ASK_UNAPPLIED, "Any unapplied profile changes will be lost ?") == 0);
    CHECK(strcmp(LJ_PE_ERR_NOTES, "Notes section too large - cannot apply profile") == 0);
    CHECK(strcmp(LJ_PE_ERR_IDENTICAL, "Values cannot be identical") == 0);
    CHECK(strcmp(LJ_PE_ERR_MANUFACTURER, "Please specify manufacturer") == 0);
    CHECK(strcmp(LJ_PE_ERR_FILENAME, "Filename cannot exceed 29 characters :") == 0);
}

/* working copy -> profile -> .Udf export -> import -> equal (every field: the bytes and the parsed struct) */
static void test_roundtrip(lj_profile_edit *ed, lj_profile_edit *back)
{
    static lj_profile in;
    static uint8_t buf[1 << 17];
    char err[160];
    const char *path = "test_profile_editor.Udf";
    FILE *f;
    size_t n;
    int i, k;
    lj_pe_begin(ed, NULL);
    if (lj_pe_refresh(ed) != 0) { printf("SKIP round trip: lj_gs2_parse is the W1 stub (gs2 track)\n"); return; }
    for (i = 0; i < lj_pe_nfields; i++) {                      /* a plausible value in every field (offsets 0-31 so the parse keeps them) */
        const lj_pe_field *fl = &lj_pe_fields[i];
        if (fl->kind == LJ_PEF_STR) lj_pe_set_str(ed, i, 0, "Abc");
        else if (fl->kind == LJ_PEF_CHARS) lj_pe_set_str(ed, i, 0, "{\\rtf1 user notes}");
        else for (k = 0; k < lj_t_size(fl); k++) lj_pe_set_u8(ed, i, k, (i + k) % 31);
    }
    CHECK(lj_pe_refresh(ed) == 0);
    remove(path);
    if (lj_udf_export(path, &ed->p, err, sizeof err) != 0) { printf("SKIP round trip: lj_udf_export: %s (gs2 track)\n", err); return; }
    f = fopen(path, "rb");
    n = f ? fread(buf, 1, sizeof buf, f) : 0;
    if (f) fclose(f);
    memset(&in, 0, sizeof in);
    CHECK(n > 0 && lj_udf_parse(buf, n, &in, err, sizeof err) == 0);
    if (!in.gs2_raw) { CHECK(0); return; }
    CHECK(memcmp(in.gs2_raw, ed->raw, LJ_GS2_REC_SIZE) == 0);  /* byte-exact through gs2_raw */
    CHECK(lj_pe_begin(back, &in) == 0);
    for (i = 0; i < lj_pe_nfields; i++) {                      /* every field equal, through the getters */
        char a[80], b[80];
        if (is_text(&lj_pe_fields[i])) { lj_pe_get_str(ed, i, 0, a, sizeof a); lj_pe_get_str(back, i, 0, b, sizeof b); CHECK(strcmp(a, b) == 0); }
        else for (k = 0; k < lj_t_size(&lj_pe_fields[i]); k++) if (lj_pe_get_u8(ed, i, k) != lj_pe_get_u8(back, i, k)) { CHECK(0); printf("  %s byte %d\n", lj_pe_fields[i].control, k); break; }
    }
    CHECK(strcmp(in.author, ed->p.author) == 0 && in.transparent == ed->p.transparent && strcmp(in.icon_bitmap, ed->p.icon_bitmap) == 0);
    lj_gs2_free(&in);
    remove(path);
}

static void test_config_channels(void)
{
    lj_show *a = lj_show_new(), *b = lj_show_new();
    char err[256];
    const char *path = "test_profile_editor_config.dat";
    a->fixture[4].kind = LJ_FX_BUILTIN; a->fixture[4].profile_id = 56; a->fixture[4].link = 1; a->fixture[4].address = 10;
    strcpy(a->fixture[4].ch_caption[0], "Inte"); strcpy(a->fixture[4].ch_caption[3], "A b");
    a->fixture[4].ch_gm_off = (1u << 1) | (1u << 31);
    CHECK(lj_config_save(path, a) == 0);
    CHECK(lj_config_load(path, b, err, sizeof err) == 0);
    CHECK(memcmp(b->fixture[4].ch_caption, a->fixture[4].ch_caption, sizeof a->fixture[4].ch_caption) == 0);
    CHECK(b->fixture[4].ch_gm_off == a->fixture[4].ch_gm_off);
    write_file(path, "LJ4 config 1\nfx 1 kind=builtin id=19 link=1 addr=1\nchcap 33 text=X\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == -1);      /* offset 33 rejected (lj_kv_range_lead) */
    write_file(path, "LJ4 config 1\nfx 1 kind=builtin id=19 link=1 addr=1\nchcap 1 text=TooLong\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == -1);      /* ME-118: seven bytes do not fit ch_caption[7] (DFM MaxLength = 6) */
    write_file(path, "LJ4 config 1\nfx 1 kind=builtin id=19 link=1 addr=1\nchcap 1 text=Yellow\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == 0 && strcmp(b->fixture[0].ch_caption[0], "Yellow") == 0);   /* ME-118: six accepted */
    write_file(path, "LJ4 config 1\nfx 1 kind=builtin id=19 link=1 addr=1\nchgm=0,2\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == -1);
    write_file(path, "LJ4 config 1\nchgm=2\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == -1);      /* before any fx */
    write_file(path, "LJ4 config 1\nfx 1 kind=builtin id=19 link=1 addr=1\nchcap 1 text=Dim\nchgm=1\nfx 2 kind=builtin id=19 link=1 addr=2\n");
    CHECK(lj_config_load(path, b, err, sizeof err) == 0 && strcmp(b->fixture[0].ch_caption[0], "Dim") == 0 && b->fixture[0].ch_gm_off == 1u && b->fixture[1].ch_gm_off == 0);
    remove(path);
    lj_show_free(a); lj_show_free(b);
}

static void test_custom_palettes(void)
{
    static lj_palette a[LJ_PALETTES], b[LJ_PALETTES];
    char err[256];
    const char *path = "test_profile_editor_setup.pal";
    memset(a, 0, sizeof a);
    a[0].e[3].used = 1; a[0].e[3].rgb = 0xFF8000;
    a[2].e[0].used = 1; a[2].e[0].rgb = LJ_CUSTOM_NO_RGB; strcpy(a[2].e[0].bitmap, "gobo_bells");
    a[5].e[19].used = 1; a[5].e[19].rgb = LJ_CUSTOM_NO_RGB;
    CHECK(lj_fixture_custom_save(path, a) == 0);
    CHECK(lj_fixture_custom_load(path, b, err, sizeof err) == 0);
    CHECK(memcmp(a, b, sizeof a) == 0);
    write_file(path, "LJ4 custompal 1\npal 7 entry=1 rgb=0 bmp=\n");
    CHECK(lj_fixture_custom_load(path, b, err, sizeof err) == -1);
    write_file(path, "LJ4 custompal 1\npal 1 entry=21 rgb=0 bmp=\n");
    CHECK(lj_fixture_custom_load(path, b, err, sizeof err) == -1);
    remove(path);
}

int lj_test_profile_editor(int argc, char **argv);
int lj_test_profile_editor(int argc, char **argv)
{
    static uint8_t used[LJ_GS2_REC_SIZE];
    static lj_profile_edit ed, before;
    int i, b, k;
    (void)argc; (void)argv;
    CHECK(strcmp(lj_pe_tab_caption[LJ_PE_INTENSITY_LAMP], "Intensity & Lamp") == 0 && strcmp(lj_pe_tab_caption[LJ_PE_OFFLINE_VIZ], "Offline Visualizer") == 0);
    CHECK(lj_pe_nfields > 60);
    memset(used, 0, sizeof used);
    for (i = 0; i < lj_pe_nfields; i++) {
        const lj_pe_field *f = &lj_pe_fields[i];
        int sz = lj_t_size(f);
        CHECK(f->control && f->control[0] && f->label && f->label[0]);
        CHECK(f->tab < LJ_PE_TAB_COUNT && (i == 0 || f->tab >= lj_pe_fields[i - 1].tab));
        if (f->off + sz > LJ_GS2_REC_SIZE) { CHECK(0); printf("  %s 0x%04X + %d past the record\n", f->control, f->off, sz); continue; }
        for (b = f->off; b < f->off + sz; b++) {
            if (used[b]) { CHECK(0); printf("  %s overlaps at 0x%04X\n", f->control, b); break; }
            used[b] = 1;
        }
    }
    CHECK(lj_pe_field_find("cbLimitGM") >= 0 && lj_pe_fields[lj_pe_field_find("cbLimitGM")].off == 0x3D52);
    CHECK(lj_pe_field_find("nope") == -1);

    /* New Profile */
    CHECK(lj_pe_begin(&ed, NULL) == 0);
    CHECK(ed.raw[0] == 0x31 && ed.raw[1] == 0x0A && ed.raw[4] == 0x2E && ed.raw[0x11AA + 16] == 1 && ed.dirty == 0);
    lj_pe_refresh(&ed);
    CHECK(ed.raw[5] == 31);
    for (k = 5; k < 32; k++) lj_pe_set_u8(&ed, lj_pe_field_find("lvLevels"), 16 * k + 7, 1);   /* disable 5..31: footprint 5 */
    lj_pe_refresh(&ed);
    CHECK(ed.raw[5] == 4);

    test_fields(&ed, &before);
    test_validate(&ed);
    test_roundtrip(&ed, &before);
    test_config_channels();
    test_custom_palettes();
    printf("test_profile_editor: %d fields, %d failure(s)\n", lj_pe_nfields, fails);
    return fails ? 1 : 0;
}
