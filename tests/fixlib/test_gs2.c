/* test_gs2.c - the .GS2 / .Udf importer and exporter (gs2 track; lj_gs2.h, note 16, oracle tools/ref/16-gs2_ref.py).
 * Every record here is SYNTHETIC: built by our writer from a struct, then given stale string tails, reserved and
 * 0x266C bytes by hand (no Martin bytes are committed). The 48 shipped .GS2 and the .Udf sample are compared with the
 * oracle by the gs2_check CTest row (tools/gs2_check.py drives `lj4evr_tests gs2_dump <file>`).
 * test_gs2:     round trip byte-exact, field values, TColor byte order, ShortString clamp, ME-33 v0/v1, every truncated
 *               prefix and six malformed variants refused (sizes with the oracle's message), the RTF stripper, lj_gs2_free
 *               of the derived window tables.
 * test_udf_crc: lj_udf_export -> lj_udf_parse round trip, one flipped footer byte -> "footer CRC mismatch" verbatim,
 *               a flipped body byte -> same, every truncated prefix refused, a stream 0 of the wrong size -> the size message.
 * gs2_dump <file>: the flat field list tools/gs2_check.py compares with the oracle, then "roundtrip ok|FAIL". */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixlib/lj_gs2.h"
#include "core/lj_str.h"
#include "miniz.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void lj_t_profile(lj_profile *p)
{
    int i;
    memset(p, 0, sizeof *p);
    p->format_version = LJ_GS2_VERSION; p->footprint = 12;
    strcpy(p->manufacturer, "Synthetic"); strcpy(p->name, "Test Spot"); strcpy(p->mode, "12 ch");
    strcpy(p->fixture_id, "SYNSPOT12"); strcpy(p->author, "LJ4EVR tests"); strcpy(p->icon_caption, "Spot");
    strcpy(p->icon_bitmap, "SPOT.BMP"); p->transparent = 1;
    strcpy((char *)p->notes_rtf, "{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}\\f0 Line one\\par Caf\\'e9 \\{x\\}}");
    p->intensity = (lj_dyn){ 1, 0, "", 0, 255 };
    p->shutter = (lj_dyn){ 1, 1, "", 0, 15 };
    p->strobe = (lj_dyn){ 1, 1, "", 64, 95 }; p->strobe_off = 255;
    p->use_gm = 1; p->limit_gm = 1;
    p->pt16 = 1; p->pan_ofs = 2; p->tilt_ofs = 4; p->presets = 1; p->macros = 1; p->follow_spot = 1;
    p->misc_move = (lj_dyn){ 1, 6, "Speed", 0, 255 };
    p->color[0] = (lj_dyn){ 1, 7, "Color", 0, 192 };
    p->gobo[0] = (lj_dyn){ 1, 8, "Gobo", 0, 190 };
    p->focus = (lj_dyn){ 1, 9, "", 255, 0 };
    p->effects[2] = (lj_dyn){ 1, 10, "Index", 0, 127 };
    p->palette[0].e[0] = (lj_pal_entry){ 1, 3, { 10, 20, 0, 0, 0 }, 0x112233u, "WHITE" };
    p->palette[0].type[0] = 13; p->palette[0].type[1] = 12;
    p->palette[5].e[19] = (lj_pal_entry){ 1, 1, { 255, 0, 0, 0, 0 }, 0xFF00FFu, "FUNC\\ON" };
    p->palette[5].type[0] = 6;
    p->blackout.enabled = 1; p->blackout.defined[0] = 1;
    p->lamp_on.enabled = 1; p->lamp_on.defined[11] = 1; p->lamp_on.value[11] = 42;
    p->reset.enabled = 1; p->reset.defined[11] = 1; p->reset.value[11] = 12;
    strcpy(p->special[0].name, "Display On"); p->special[0].enabled = 1; p->special[0].defined[11] = 1; p->special[0].value[11] = 157;
    p->lamp_on_ms = 3000; p->reset_ms = 5500;
    p->ext_gm[0] = 1; p->default_dmx[2] = 128; p->default_dmx[4] = 128; p->hsv[0] = 1;
    for (i = 0; i < LJ_PROFILE_CH; i++) { p->level_invisible[i] = p->level_disabled[i] = i >= 12; }
    strcpy((char *)p->level_caption[11], "Ctrl"); p->level_invisible[11] = 0;
}

static int lj_t_count(const uint8_t *buf, size_t len, int (*fn)(const uint8_t *, size_t, lj_profile *, char *, size_t), const char *want_prefix)
{
    size_t n;
    int bad = 0;
    for (n = 0; n < len; n++) {
        lj_profile q;
        char err[128];
        memset(&q, 0, sizeof q);
        if (fn(buf, n, &q, err, sizeof err) == 0) { bad++; lj_gs2_free(&q); }
        else if (q.gs2_raw || (want_prefix && strncmp(err, want_prefix, strlen(want_prefix)) != 0)) bad++;
    }
    return bad;
}

int lj_test_gs2(int argc, char **argv);
int lj_test_gs2(int argc, char **argv)
{
    lj_profile p, q, r;
    uint8_t *rec = malloc(LJ_GS2_REC_SIZE), *out = malloc(LJ_GS2_REC_SIZE), *bad = malloc(LJ_GS2_REC_SIZE + 1);
    char err[128], txt[256];
    int i;
    (void)argc; (void)argv;
    fails = 0;
    if (!rec || !out || !bad) return 1;

    /* writer from a zeroed record (built-in export path) */
    lj_t_profile(&p);
    CHECK(lj_gs2_write(&p, rec) == 0);
    CHECK(rec[0] == 0x31 && rec[1] == 0x0A && rec[4] == 0x2E && rec[5] == 11);   /* 2609 LE, const04, last offset */
    CHECK(rec[0x1377 + 7] == 0x11 && rec[0x1377 + 8] == 0x22 && rec[0x1377 + 9] == 0x33);   /* TColor bytes R, G, B (LORD_FIXTURES-9) */
    CHECK(rec[0x0028] == 4 && memcmp(rec + 0x0029, "Spot", 4) == 0);
    /* unmapped bytes a real LJ2 file carries: stale tails, OV strings, reserved, the 0x266C bytes, rgb_pad, legacy name */
    memcpy(rec + 0x0028 + 5, "XYZ", 3);                          /* stale tail after "Spot" */
    rec[0x0009] = 5; memcpy(rec + 0x000A, "Older", 5);
    rec[0x3C86] = 3; memcpy(rec + 0x3C87, "Par", 3);
    rec[0x3D53] = 8; memcpy(rec + 0x3D54, "SYN.GS2", 7);
    for (i = 0; i < 17; i++) rec[0x266C + i] = (uint8_t)(0xA0 + i);
    rec[0x10A9] = 0x55; rec[0x5172] = 0x66; rec[0x1377 + 10] = 0x77; rec[0x1377 + 43] = 0x88; rec[0x1173 + 14] = 0x99;
    rec[0x35D1 + 16 * 11 + 9] = 0x44;                            /* level reserved tail */

    memset(&q, 0, sizeof q);
    CHECK(lj_gs2_read(rec, LJ_GS2_REC_SIZE, &q, err, sizeof err) == 0);
    CHECK(q.gs2_raw != NULL && q.nwindows > 0 && q.id == 46 && strcmp(q.tag, "USER2") == 0);
    CHECK(strcmp(q.manufacturer, "Synthetic") == 0 && strcmp(q.name, "Test Spot") == 0 && strcmp(q.mode, "12 ch") == 0);
    CHECK(strcmp(q.icon_caption, "Spot") == 0 && strcmp(q.author, "LJ4EVR tests") == 0 && q.transparent == 1 && q.footprint == 12);
    CHECK(q.palette[0].e[0].rgb == 0x112233u && strcmp(q.palette[0].e[0].bitmap, "WHITE") == 0 && q.palette[0].type[0] == 13);
    CHECK(q.palette[5].e[19].rgb == 0xFF00FFu && q.palette[5].type[0] == 6);
    CHECK(q.lamp_on_ms == 3000 && q.reset_ms == 5500 && q.special[0].value[11] == 157 && strcmp(q.special[0].name, "Display On") == 0);
    CHECK(q.channel_class[0] == LJ_CH_INTENSITY && q.channel_class[1] == LJ_CH_SHUTTER && q.channel_class[2] == LJ_CH_PAN && q.channel_class[3] == LJ_CH_PAN);
    CHECK(q.channel_class[4] == LJ_CH_TILT && q.channel_class[7] == LJ_CH_COLOR && q.channel_class[11] == LJ_CH_LAMP && q.channel_class[12] == LJ_CH_NONE);
    CHECK(lj_gs2_write(&q, out) == 0 && memcmp(out, rec, LJ_GS2_REC_SIZE) == 0);   /* serialize(parse(x)) == x */

    /* an edit rewrites only its own bytes; the stale tail beyond the new length stays */
    r = q; r.gs2_raw = q.gs2_raw;
    strcpy(r.icon_caption, "Sp");
    CHECK(lj_gs2_write(&r, out) == 0 && out[0x0028] == 2 && memcmp(out + 0x0029, "SpotXYZ", 7) == 0);
    out[0x0028] = 4;
    CHECK(memcmp(out, rec, LJ_GS2_REC_SIZE) == 0);
    lj_gs2_free(&q);
    CHECK(q.gs2_raw == NULL && q.windows == NULL);

    /* a length byte above the declared width is clamped (oracle sstr) and survives the round trip */
    memcpy(bad, rec, LJ_GS2_REC_SIZE);
    bad[0x0028] = 200;
    memset(&q, 0, sizeof q);
    CHECK(lj_gs2_read(bad, LJ_GS2_REC_SIZE, &q, err, sizeof err) == 0 && strcmp(q.icon_caption, "SpotXYZ") == 0);   /* 10 chars read, cut at the first NUL */
    CHECK(lj_gs2_write(&q, out) == 0 && memcmp(out, bad, LJ_GS2_REC_SIZE) == 0);
    lj_gs2_free(&q);

    /* ME-33: version 0 and 1 records take the legacy name, "User Definable", empty mode, and still round-trip */
    for (i = 0; i < 2; i++) {
        memcpy(bad, rec, LJ_GS2_REC_SIZE);
        bad[0] = (uint8_t)i; bad[1] = 0;
        memset(&q, 0, sizeof q);
        CHECK(lj_gs2_read(bad, LJ_GS2_REC_SIZE, &q, err, sizeof err) == 0);
        CHECK(strcmp(q.name, "Older") == 0 && strcmp(q.manufacturer, LJ_GS2_LEGACY_MANUFACTURER) == 0 && q.mode[0] == 0 && q.format_version == i);
        CHECK(lj_gs2_write(&q, out) == 0 && memcmp(out, bad, LJ_GS2_REC_SIZE) == 0);
        lj_gs2_free(&q);
    }

    /* every truncated prefix and one oversize buffer: refused with the oracle's message, gs2_raw stays NULL */
    CHECK(lj_t_count(rec, LJ_GS2_REC_SIZE, lj_gs2_read, "not a GS2 record: ") == 0);
    memcpy(bad, rec, LJ_GS2_REC_SIZE); bad[LJ_GS2_REC_SIZE] = 0;
    memset(&q, 0, sizeof q);
    CHECK(lj_gs2_read(bad, LJ_GS2_REC_SIZE + 1, &q, err, sizeof err) == -1 && strcmp(err, "not a GS2 record: 20852 bytes") == 0 && !q.gs2_raw);
    CHECK(lj_gs2_read(rec, 0, &q, err, sizeof err) == -1 && strcmp(err, "not a GS2 record: 0 bytes") == 0);
    /* six malformed variants of a full-size record (offsets the program would index with) */
    {
        static const struct { size_t off; uint8_t v; } mal[6] = {
            { 0x0005, 255 },           /* last offset 255: footprint 256 (would wrap a uint8_t) */
            { 0x1196 + 1, 40 },        /* intensity offset 40, enabled */
            { 0x1322 + 1, 255 },       /* color 1 offset 255, enabled */
            { 0x1308, 31 },            /* 16-bit pan MSB at 31: LSB 32 */
            { 0x3C75, 1 },             /* effects 4 enabled ... with offset 99 below */
            { 0x130A, 1 },             /* pan/tilt patch on ... with a patch target of 60 below */
        };
        for (i = 0; i < 6; i++) {
            memcpy(bad, rec, LJ_GS2_REC_SIZE);
            bad[mal[i].off] = mal[i].v;
            if (i == 4) bad[0x3C75 + 1] = 99;
            if (i == 5) bad[0x130D] = 60;
            memset(&q, 0, sizeof q);
            CHECK(lj_gs2_read(bad, LJ_GS2_REC_SIZE, &q, err, sizeof err) == -1 && !q.gs2_raw && strncmp(err, "GS2 ", 4) == 0);
        }
    }

    /* RTF stripper (LORD_FIDELITY-17): bytes kept in notes_rtf, shown stripped */
    CHECK(lj_str_rtf_strip(txt, sizeof txt, (const char *)p.notes_rtf) == 0 && strcmp(txt, "Line one\nCaf\xe9 {x}") == 0);
    CHECK(lj_str_rtf_strip(txt, sizeof txt, "{\\rtf1{\\*\\generator Riched20;}{\\colortbl ;\\red0\\green0\\blue0;}A\\tab B\\line C}") == 0 && strcmp(txt, "A\tB\nC") == 0);
    CHECK(lj_str_rtf_strip(txt, 4, "{\\rtf1 abcdef}") == -1 && strcmp(txt, "abc") == 0);

    free(rec); free(out); free(bad);
    printf("test_gs2: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}

/* ---- test_udf_crc ---- */

static uint8_t *lj_t_slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b = NULL;
    long n = 0;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 && (b = malloc((size_t)n)) != NULL && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    else if (b) *len = (size_t)n;
    fclose(f);
    return b;
}

int lj_test_udf_crc(int argc, char **argv);
int lj_test_udf_crc(int argc, char **argv)
{
    static const char path[] = "test_udf_crc.Udf";
    lj_profile p, q;
    uint8_t *data, *rec = malloc(LJ_GS2_REC_SIZE), *out = malloc(LJ_GS2_REC_SIZE);
    size_t len = 0;
    char err[128];
    (void)argc; (void)argv;
    fails = 0;
    if (!rec || !out) return 1;
    lj_t_profile(&p);
    strcpy(p.gs2_file, "Synthetic_Spot.GS2");
    CHECK(lj_udf_export(path, &p, err, sizeof err) == 0);
    data = lj_t_slurp(path, &len);
    CHECK(data != NULL && len > LJ_UDF_FOOTER);
    if (!data) { printf("test_udf_crc: FAIL (no export)\n"); return 1; }
    CHECK(data[len - 32] == 1 && data[len - 28] == 1 && data[len - 16] == 1);   /* file count, 1, 1 [16 s2] */

    memset(&q, 0, sizeof q);
    CHECK(lj_udf_parse(data, len, &q, err, sizeof err) == 0 && q.gs2_raw);
    CHECK(lj_gs2_write(&p, rec) == 0 && lj_gs2_write(&q, out) == 0 && memcmp(rec, out, LJ_GS2_REC_SIZE) == 0);
    CHECK(q.gs2_raw && memcmp(q.gs2_raw, rec, LJ_GS2_REC_SIZE) == 0);
    lj_gs2_free(&q);
    memset(&q, 0, sizeof q);
    CHECK(lj_udf_import(path, &q, err, sizeof err) == 0 && strcmp(q.gs2_file, "test_udf_crc.GS2") == 0);
    lj_gs2_free(&q);

    data[len - LJ_UDF_FOOTER] ^= 0x01;                               /* one flipped footer CRC byte */
    memset(&q, 0, sizeof q);
    CHECK(lj_udf_parse(data, len, &q, err, sizeof err) == -1 && strcmp(err, LJ_GS2_ERR_CRC) == 0 && strcmp(err, "footer CRC mismatch") == 0 && !q.gs2_raw);
    data[len - LJ_UDF_FOOTER] ^= 0x01;
    data[10] ^= 0x40;                                                /* one flipped body byte, footer intact */
    CHECK(lj_udf_parse(data, len, &q, err, sizeof err) == -1 && strcmp(err, LJ_GS2_ERR_CRC) == 0);
    data[10] ^= 0x40;
    CHECK(lj_t_count(data, len, lj_udf_parse, NULL) == 0);           /* every truncated prefix refused */
    free(data);

    /* a valid container whose stream 0 is 100 bytes: the oracle's size message */
    {
        uint8_t small[100] = { 0 }, z[256], buf[512];
        mz_ulong zn = sizeof z;
        uint32_t crc;
        CHECK(mz_compress2(z, &zn, small, sizeof small, MZ_DEFAULT_LEVEL) == MZ_OK);
        memset(buf, 0, sizeof buf);
        memcpy(buf, z, zn);
        crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, buf, zn);
        buf[zn] = (uint8_t)crc; buf[zn + 1] = (uint8_t)(crc >> 8); buf[zn + 2] = (uint8_t)(crc >> 16); buf[zn + 3] = (uint8_t)(crc >> 24);
        memset(&q, 0, sizeof q);
        CHECK(lj_udf_parse(buf, zn + LJ_UDF_FOOTER, &q, err, sizeof err) == -1 && strcmp(err, "not a GS2 record: 100 bytes") == 0);
    }
    remove(path);
    free(rec); free(out);
    printf("test_udf_crc: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}

static int lj_ieq_ext(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if ((*a | 0x20) != (*b | 0x20)) return 0;
    return *a == *b;
}

/* ---- gs2_dump <file>: one "key value" line per struct-owned field (tools/gs2_check.py prints the same from the oracle) ---- */

static void lj_d_str(const char *key, const char *s)
{
    printf("%s ", key);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0x20 && c < 0x7F && c != '\\') putchar(c); else printf("\\x%02x", c);
    }
    putchar('\n');
}
static void lj_d_arr(const char *key, const uint8_t *a, int n)
{
    int i;
    printf("%s", key);
    for (i = 0; i < n; i++) printf(" %d", a[i]);
    putchar('\n');
}
static void lj_d_dyn(const char *key, const lj_dyn *d)
{
    char k[64];
    printf("%s %d %d %d %d\n", key, d->enabled, d->offset, d->min, d->max);
    snprintf(k, sizeof k, "%s.name", key);
    lj_d_str(k, d->name);
}
static void lj_d_stat(const char *key, const lj_gs2_sfunc *s)
{
    char k[64];
    snprintf(k, sizeof k, "%s.name", key); lj_d_str(k, s->name);
    printf("%s.enabled %d\n", key, s->enabled);
    snprintf(k, sizeof k, "%s.defined", key); lj_d_arr(k, s->defined, LJ_PROFILE_CH);
    snprintf(k, sizeof k, "%s.values", key); lj_d_arr(k, s->value, LJ_PROFILE_CH);
}

int lj_test_gs2_dump(int argc, char **argv);
int lj_test_gs2_dump(int argc, char **argv)
{
    static const char *const dyn_names[] = { "shutter", "strobe", "intensity", "misc_move", "color1", "color2", "color3", "color4",
        "cyan_red", "magenta_green", "yellow_blue", "gobo1", "gobo2", "gobo3", "gobo4", "iris", "focus", "zoom", "frost", "beam1", "beam2",
        "effects1", "effects2", "effects3", "effects4" };
    lj_profile p;
    const lj_dyn *dyns[25];
    char err[256], k[64], notes[LJ_GS2_NOTES + 1];
    uint8_t *out;
    const char *f, *ext;
    int i, j, r;
    if (argc < 2) { printf("usage: gs2_dump <file.GS2|.Udf|.Udo>\n"); return 2; }
    f = argv[1];
    ext = strrchr(f, '.');
    memset(&p, 0, sizeof p);
    r = ext && (lj_ieq_ext(ext, ".udf") || lj_ieq_ext(ext, ".udo")) ? lj_udf_import(f, &p, err, sizeof err) : lj_gs2_load(f, &p, err, sizeof err);
    if (r != 0) { printf("error %s\n", err); return 1; }
    printf("format_version %d\nfootprint %d\n", p.format_version, p.footprint);
    lj_d_str("manufacturer", p.manufacturer); lj_d_str("name", p.name); lj_d_str("mode", p.mode);
    lj_d_str("fixture_id", p.fixture_id); lj_d_str("author", p.author); lj_d_str("icon_caption", p.icon_caption);
    lj_d_str("icon_bitmap", p.icon_bitmap);
    printf("transparent %d\n", p.transparent);
    memcpy(notes, p.notes_rtf, LJ_GS2_NOTES); notes[LJ_GS2_NOTES] = 0;
    lj_d_str("notes", notes);
    dyns[0] = &p.shutter; dyns[1] = &p.strobe; dyns[2] = &p.intensity; dyns[3] = &p.misc_move;
    for (i = 0; i < 4; i++) { dyns[4 + i] = &p.color[i]; dyns[11 + i] = &p.gobo[i]; dyns[21 + i] = &p.effects[i]; }
    for (i = 0; i < 3; i++) dyns[8 + i] = &p.cmy[i];
    dyns[15] = &p.iris; dyns[16] = &p.focus; dyns[17] = &p.zoom; dyns[18] = &p.frost; dyns[19] = &p.beam[0]; dyns[20] = &p.beam[1];
    for (i = 0; i < 25; i++) lj_d_dyn(dyn_names[i], dyns[i]);
    printf("strobe_off %d\nuse_gm %d\nblackout_on_gm0 %d\ninvert_int_chan %d\ninvert_int_range %d\nlimit_gm %d\n",
           p.strobe_off, p.use_gm, p.bo_at_gm0, p.invert_int, p.invert_int_range, p.limit_gm);
    lj_d_arr("ext_gm", p.ext_gm, LJ_PROFILE_CH); lj_d_arr("default_dmx", p.default_dmx, LJ_PROFILE_CH);
    lj_d_stat("blackout", &p.blackout); lj_d_stat("lamp_on", &p.lamp_on); lj_d_stat("lamp_off", &p.lamp_off); lj_d_stat("reset", &p.reset);
    for (i = 0; i < 32; i++) { snprintf(k, sizeof k, "special%d", i + 1); lj_d_stat(k, &p.special[i]); }
    printf("pt16 %d\npt8 %d\npan_offset %d\ntilt_offset %d\npt_patch %d\n", p.pt16, p.pt8, p.pan_ofs, p.tilt_ofs, p.pt_patch_on);
    lj_d_arr("patch", p.pt_patch, 4);
    printf("presets %d\nmacros %d\nfollow_spot %d\ncmy %d\nrgb %d\ncmy_picker %d\ninvert_cmyrgb %d\n",
           p.presets, p.macros, p.follow_spot, p.cmy_on, p.rgb_on, p.cmy_picker, p.invert_cmyrgb);
    lj_d_arr("hsv", p.hsv, 3);
    for (i = 0; i < LJ_PROFILE_CH; i++) {
        snprintf(k, sizeof k, "level%d", i); lj_d_str(k, (const char *)p.level_caption[i]);
        printf("level%d.flags %d %d\n", i, p.level_invisible[i], p.level_disabled[i]);
    }
    printf("lamp_on_time_ms %d\nreset_time_ms %d\n", (int)(int32_t)p.lamp_on_ms, (int)(int32_t)p.reset_ms);
    for (i = 0; i < LJ_PALETTES; i++) {
        snprintf(k, sizeof k, "pal%d.types", i); lj_d_arr(k, p.palette[i].type, 5);
        for (j = 0; j < LJ_PAL_ENTRIES; j++) {
            const lj_pal_entry *e = &p.palette[i].e[j];
            printf("pal%d.%d %d %d %d %d %d %d %d %06X\n", i, j, e->used, e->mask, e->v[0], e->v[1], e->v[2], e->v[3], e->v[4], (unsigned)e->rgb);
            snprintf(k, sizeof k, "pal%d.%d.bitmap", i, j); lj_d_str(k, e->bitmap);
        }
    }
    out = malloc(LJ_GS2_REC_SIZE);
    printf("roundtrip %s\n", out && lj_gs2_write(&p, out) == 0 && memcmp(out, p.gs2_raw, LJ_GS2_REC_SIZE) == 0 ? "ok" : "FAIL");
    free(out);
    lj_gs2_free(&p);
    return 0;
}
