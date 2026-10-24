/* test_draw.c - goldens for src/gfx/lj_draw.c: every primitive into a 64x64 surface,
 * CRC32 against the constants below. A changed constant is a reviewed rule change.
 * `lj4evr_tests draw_goldens print` emits the current table. No text goes through
 * CRCs (the placeholder font is replaced by the fonts track). A few pixel asserts
 * guard the rules the CRCs cannot explain. */
#include <stdio.h>
#include <string.h>
#include "core/lj_types.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t lj_crc32(const lj_surface *s)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t n = (size_t)s->w * (size_t)s->h, i;
    for (i = 0; i < n; i++) {
        uint32_t v = s->px[i];
        int b, k;
        for (b = 0; b < 4; b++) {
            c ^= (v >> (8 * b)) & 0xFF;
            for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        }
    }
    return ~c;
}

static uint32_t px(const lj_surface *s, int x, int y) { return s->px[(size_t)y * (size_t)s->w + (size_t)x]; }

typedef void (*lj_case_fn)(lj_surface *s);
static void c_fill(lj_surface *s)      { lj_fill(s, -5, -5, 20, 20, LJ_CLR_RED); lj_fill(s, 50, 50, 100, 100, LJ_CLR_GREEN); lj_hline(s, 0, 30, 64, LJ_CLR_YELLOW); lj_vline(s, 30, 0, 64, LJ_CLR_HIGHLIGHT); }
static void c_bevel1(lj_surface *s)    { lj_bevel1(s, 2, 2, 40, 20, LJ_CLR_BTNHIGHLIGHT, LJ_CLR_BTNSHADOW); lj_bevel1(s, 10, 30, 30, 30, LJ_CLR_BTNSHADOW, LJ_CLR_BTNHIGHLIGHT); }
static void c_raised(lj_surface *s)    { lj_button_raised(s, 2, 2, 51, 25); }
static void c_pressed(lj_surface *s)   { lj_button_pressed(s, 2, 2, 51, 25); }
static void c_latched(lj_surface *s)   { lj_button_latched(s, 2, 2, 25, 25); }
static void c_sunken(lj_surface *s)    { lj_edge_sunken(s, 3, 3, 33, 21, LJ_CLR_BTNHIGHLIGHT); lj_edge_sunken(s, 3, 30, 33, 21, LJ_CLR_BTNFACE); }
static void c_etched(lj_surface *s)    { lj_bevel_etched(s, 2, 6, 60, 50); }
static void c_focus(lj_surface *s)     { lj_focus_rect(s, 3, 3, 40, 20); lj_focus_rect(s, 4, 30, 9, 9); }
static void c_check(lj_surface *s)     { lj_checkbox(s, 2, 2, 1); lj_checkbox(s, 20, 2, 0); lj_check_glyph(s, 40, 4, LJ_CLR_RED); }
static void c_radio(lj_surface *s)     { lj_radio(s, 2, 2, 1); lj_radio(s, 20, 2, 0); }
static void c_blit(lj_surface *s)
{
    static const uint8_t bits[2] = { 0xA5, 0x5A };          /* 8x2, MSB first */
    lj_surface src;
    lj_surface_init(&src, 4, 3);
    lj_fill(&src, 0, 0, 4, 3, LJ_CLR_RED);
    src.px[2 * 4 + 0] = LJ_CLR_GREEN;                        /* bottom-left = key */
    src.px[0] = LJ_CLR_GREEN;                                /* top-left must vanish too */
    lj_blit_1bit(s, 2, 2, bits, 8, 2, LJ_CLR_YELLOW);
    lj_blit_1bit(s, -3, 62, bits, 8, 2, LJ_CLR_YELLOW);      /* clipped */
    lj_blit_rgb(s, 10, 10, &src, 1);
    lj_blit_rgb(s, 20, 10, &src, 0);
    lj_blit_rgb(s, 62, 62, &src, 0);                         /* clipped */
    lj_surface_free(&src);
}

static const struct { const char *name; lj_case_fn fn; uint32_t crc; } lj_cases[] = {
    { "fill",     c_fill,     0xD17CF5F0u },
    { "bevel1",   c_bevel1,   0x226A0466u },
    { "raised",   c_raised,   0x31BA0EA8u },
    { "pressed",  c_pressed,  0xBD059776u },
    { "latched",  c_latched,  0xDC34EE74u },
    { "sunken",   c_sunken,   0xB5B2D7D6u },
    { "etched",   c_etched,   0x3845C503u },
    { "focus",    c_focus,    0x54935D75u },
    { "check",    c_check,    0xB9DB0325u },
    { "radio",    c_radio,    0x241E7DBDu },
    { "blit",     c_blit,     0xCFA92DBDu },
};

int lj_test_draw_goldens(int argc, char **argv);
int lj_test_draw_goldens(int argc, char **argv)
{
    lj_surface s;
    int i, print = argc > 1 && strcmp(argv[1], "print") == 0;

    for (i = 0; i < LJ_COUNT_OF(lj_cases); i++) {
        uint32_t crc;
        CHECK(lj_surface_init(&s, 64, 64) == 0);
        lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
        lj_cases[i].fn(&s);
        crc = lj_crc32(&s);
        if (print) printf("    { \"%s\", c_%s, 0x%08Xu },\n", lj_cases[i].name, lj_cases[i].name, crc);
        else if (crc != lj_cases[i].crc) { fails++; printf("FAIL golden %s: got 0x%08X want 0x%08X\n", lj_cases[i].name, crc, lj_cases[i].crc); }
        lj_surface_free(&s);
    }

    /* rule asserts (design 7.3) */
    CHECK(lj_surface_init(&s, 64, 64) == 0);
    lj_button_raised(&s, 0, 0, 10, 10);
    CHECK(px(&s, 0, 0) == LJ_CLR_BTNHIGHLIGHT && px(&s, 9, 9) == LJ_CLR_3DDKSHADOW && px(&s, 8, 8) == LJ_CLR_BTNSHADOW && px(&s, 1, 1) == LJ_CLR_BTNFACE && px(&s, 9, 0) == LJ_CLR_3DDKSHADOW);
    lj_edge_sunken(&s, 20, 0, 10, 10, LJ_CLR_BTNHIGHLIGHT);
    CHECK(px(&s, 20, 0) == LJ_CLR_BTNSHADOW && px(&s, 21, 1) == LJ_CLR_3DDKSHADOW && px(&s, 29, 9) == LJ_CLR_BTNHIGHLIGHT && px(&s, 28, 8) == LJ_CLR_BTNFACE && px(&s, 25, 5) == LJ_CLR_BTNHIGHLIGHT);
    lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
    lj_focus_rect(&s, 0, 0, 8, 8);
    CHECK(px(&s, 0, 0) == LJ_CLR_3DDKSHADOW && px(&s, 1, 0) == LJ_CLR_BTNFACE && px(&s, 2, 0) == LJ_CLR_3DDKSHADOW && px(&s, 0, 1) == LJ_CLR_BTNFACE && px(&s, 3, 3) == LJ_CLR_BTNFACE);
    lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
    c_blit(&s);
    CHECK(px(&s, 10, 10) == LJ_CLR_BTNFACE && px(&s, 10, 12) == LJ_CLR_BTNFACE && px(&s, 11, 10) == LJ_CLR_RED);    /* keyed pixels skipped */
    CHECK(px(&s, 20, 10) == LJ_CLR_GREEN && px(&s, 20, 12) == LJ_CLR_GREEN);                                          /* opaque copy keeps them */
    CHECK(px(&s, 2, 2) == LJ_CLR_YELLOW && px(&s, 3, 2) == LJ_CLR_BTNFACE && px(&s, 9, 2) == LJ_CLR_YELLOW && px(&s, 2, 3) == LJ_CLR_BTNFACE && px(&s, 3, 3) == LJ_CLR_YELLOW);
    CHECK(px(&s, 63, 63) == LJ_CLR_RED && px(&s, 0, 62) == LJ_CLR_BTNFACE && px(&s, 0, 63) == LJ_CLR_YELLOW);        /* 0x5A row 1 bit 7 clear, bit 6 set at x=-3+... */
    lj_surface_free(&s);

    /* lj_blit_rgb_fit: a 50x50 icon into the 60x36 Fixed Size tile body shrinks to 36x36, centred, nothing outside the box;
     * a 20x20 one is copied 1:1 (never enlarged); src pixel (x, y) is coloured (x << 8) | y + 1 so the sample is readable */
    {
        lj_surface src = { 0 };
        int x, y, out = 0, in = 0;
        CHECK(lj_surface_init(&s, 64, 64) == 0 && lj_surface_init(&src, 50, 50) == 0);
        for (y = 0; y < 50; y++) for (x = 0; x < 50; x++) src.px[y * 50 + x] = ((uint32_t)x << 8) | (uint32_t)(y + 1);
        lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
        lj_blit_rgb_fit(&s, 2, 20, 60, 36, &src, 0);
        for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
            int inbox = x >= 14 && x < 50 && y >= 20 && y < 56;
            if (px(&s, x, y) != LJ_CLR_BTNFACE) { if (inbox) in++; else out++; }
        }
        CHECK(out == 0 && in == 36 * 36);
        CHECK(px(&s, 14, 20) == 1u && px(&s, 49, 55) == ((48u << 8) | 49u) && px(&s, 15, 21) == ((1u << 8) | 2u) && px(&s, 32, 38) == ((25u << 8) | 26u));
        src.px[48 * 50] = 50u;                                                          /* (0, 48), the last row sampled, takes the key colour of (0, 49) */
        lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
        lj_blit_rgb_fit(&s, 2, 20, 60, 36, &src, 1);
        CHECK(px(&s, 14, 55) == LJ_CLR_BTNFACE && px(&s, 15, 55) == ((1u << 8) | 49u));
        lj_surface_free(&src);
        CHECK(lj_surface_init(&src, 20, 10) == 0);
        lj_fill(&src, 0, 0, 20, 10, LJ_CLR_RED);
        lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
        lj_blit_rgb_fit(&s, 2, 20, 60, 36, &src, 0);
        CHECK(px(&s, 22, 33) == LJ_CLR_RED && px(&s, 41, 42) == LJ_CLR_RED && px(&s, 21, 33) == LJ_CLR_BTNFACE && px(&s, 42, 42) == LJ_CLR_BTNFACE && px(&s, 22, 43) == LJ_CLR_BTNFACE);
        lj_fill(&s, 0, 0, 64, 64, LJ_CLR_BTNFACE);
        lj_blit_rgb_fit(&s, 0, 0, 10, 60, &src, 0);                                       /* wide: 10x5 at y 27 */
        CHECK(px(&s, 0, 27) == LJ_CLR_RED && px(&s, 9, 31) == LJ_CLR_RED && px(&s, 10, 27) == LJ_CLR_BTNFACE && px(&s, 0, 26) == LJ_CLR_BTNFACE && px(&s, 0, 32) == LJ_CLR_BTNFACE);
        lj_surface_free(&src);
        lj_surface_free(&s);
    }
    CHECK(lj_surface_init(&s, 0, 5) == -1 && s.px == NULL);
    lj_surface_free(&s);

    printf("draw_goldens: %d cases, %d failures\n", LJ_COUNT_OF(lj_cases), fails);
    return fails ? 1 : 0;
}
