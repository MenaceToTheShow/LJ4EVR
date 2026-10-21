/* test_text.c - lj_text_width / lj_text / lj_text_disabled / lj_glyph_get. Widths and the
 * underline row come from the linked font table itself (placeholder or packed, inked or
 * not); the ink checks use a synthetic 4-px block font so they never depend on glyph art. */
#include <stdio.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "gfx/lj_glyphs.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t px(const lj_surface *s, int x, int y) { return s->px[(size_t)y * (size_t)s->w + (size_t)x]; }
static int adv(const lj_font *f, int c) { return f->advance[c - LJ_FONT_FIRST]; }
static int row_has(const lj_surface *s, int y, int x0, int x1, uint32_t c) { int x; for (x = x0; x < x1; x++) if (px(s, x, y) == c) return 1; return 0; }

static uint8_t  blk_adv[LJ_FONT_GLYPHS];
static uint16_t blk_off[LJ_FONT_GLYPHS];                           /* all zero: every glyph is the same 4x9 block */
static const uint8_t blk_bits[13] = { 0, 0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0, 0 };

int lj_test_text(int argc, char **argv);
int lj_test_text(int argc, char **argv)
{
    const lj_font *f = &lj_font_sans13;
    lj_font blk = { 13, 11, 2, 0, blk_adv, blk_off, blk_bits };
    lj_surface s;
    int ws, ul, i;
    (void)argc; (void)argv;
    for (i = 0; i < LJ_FONT_GLYPHS; i++) blk_adv[i] = 4;

    ws = adv(f, 'S') + adv(f, 'e') + adv(f, 'q') + adv(f, '#');
    printf("text: \"Seq#\" width %d (cell %d = %d + %d)\n", lj_text_width(f, "Seq#"), f->height, f->ascent, f->descent);
    CHECK(lj_text_width(f, "Seq#") == ws);
    CHECK(lj_text_width(f, "&File") == lj_text_width(f, "File"));
    CHECK(lj_text_width(f, "&&") == adv(f, '&'));
    CHECK(lj_text_width(f, "A&") == adv(f, 'A'));
    CHECK(lj_text_width(f, "") == 0 && lj_text_width(f, NULL) == 0);
    CHECK(lj_text_width(f, "\t") == adv(f, ' '));                     /* control byte -> space */
    CHECK(f->height == f->ascent + f->descent);

    CHECK(lj_surface_init(&s, 64, 32) == 0);
    lj_fill(&s, 0, 0, 64, 32, LJ_CLR_BTNFACE);
    lj_text(&s, f, 2, 1, "&S", LJ_CLR_RED, 1);
    ul = 1 + f->ascent + 1;                                          /* underline row for y = 1 */
    CHECK(row_has(&s, ul, 2, 2 + adv(f, 'S'), LJ_CLR_RED));
    CHECK(px(&s, 2, ul) == LJ_CLR_RED && px(&s, 2 + adv(f, 'S') - 1, ul) == LJ_CLR_RED);
    CHECK(!row_has(&s, ul, 2 + adv(f, 'S'), 64, LJ_CLR_RED));         /* '&' consumed: nothing past S */
    CHECK(!row_has(&s, 1 + f->height, 0, 64, LJ_CLR_RED));            /* nothing below the cell */
    lj_fill(&s, 0, 0, 64, 32, LJ_CLR_BTNFACE);
    lj_text(&s, &blk, 2, 1, "&S", LJ_CLR_RED, 0);                     /* literal: two 4-px blocks at x 2..9, no underline */
    CHECK(!row_has(&s, ul, 0, 64, LJ_CLR_RED));
    CHECK(px(&s, 2, 3) == LJ_CLR_RED && px(&s, 9, 11) == LJ_CLR_RED && px(&s, 10, 3) == LJ_CLR_BTNFACE && px(&s, 2, 2) == LJ_CLR_BTNFACE && px(&s, 2, 12) == LJ_CLR_BTNFACE);
    lj_fill(&s, 0, 0, 64, 32, LJ_CLR_BTNFACE);
    lj_text(&s, &blk, 2, 1, "&S", LJ_CLR_RED, 1);                     /* one block, underline across its 4 px at row 13 */
    CHECK(px(&s, 6, 3) == LJ_CLR_BTNFACE && px(&s, 2, 13) == LJ_CLR_RED && px(&s, 5, 13) == LJ_CLR_RED && px(&s, 6, 13) == LJ_CLR_BTNFACE);
    lj_fill(&s, 0, 0, 64, 32, LJ_CLR_BTNFACE);
    lj_text_disabled(&s, &blk, 2, 1, "S", 0);                          /* grey block, white shadow peeking at +1,+1 */
    CHECK(px(&s, 2, 3) == LJ_CLR_BTNSHADOW && px(&s, 6, 12) == LJ_CLR_BTNHIGHLIGHT && px(&s, 6, 4) == LJ_CLR_BTNHIGHLIGHT && px(&s, 3, 12) == LJ_CLR_BTNHIGHLIGHT);
    lj_text(&s, f, -100, -100, "clip", LJ_CLR_RED, 1);                /* off-surface: must not crash */
    lj_text(&s, f, 60, 20, "clip", LJ_CLR_RED, 1);
    lj_surface_free(&s);

    CHECK(lj_glyph_get("no_such_glyph") == NULL && lj_glyph_get(NULL) == NULL);
    printf("text: %d failures\n", fails);
    return fails ? 1 : 0;
}
