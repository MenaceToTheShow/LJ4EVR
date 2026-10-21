/* test_bmp.c - lj_bmp_write: header fields, bottom-up BGR rows, 4-byte padding. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int lj_test_bmp(int argc, char **argv);
int lj_test_bmp(int argc, char **argv)
{
    static const char *path = "lj_test_bmp_out.bmp";
    uint8_t buf[128];
    lj_surface s;
    FILE *fp;
    size_t n = 0;
    (void)argc; (void)argv;

    CHECK(lj_surface_init(&s, 3, 2) == 0);
    s.px[0] = 0x112233; s.px[1] = 0x445566; s.px[2] = 0x778899;      /* top row */
    s.px[3] = 0xAABBCC; s.px[4] = 0xDDEEFF; s.px[5] = 0x010203;      /* bottom row */
    CHECK(lj_bmp_write(&s, path) == 0);
    CHECK(lj_bmp_write(NULL, path) == -1);
    CHECK(lj_bmp_write(&s, "no_such_dir_lj/x.bmp") == -1);
    fp = fopen(path, "rb");
    CHECK(fp != NULL);
    if (fp) { n = fread(buf, 1, sizeof buf, fp); fclose(fp); remove(path); }
    CHECK(n == 54 + 12 * 2);                                          /* stride 9 -> 12 */
    if (n == 78) {
        CHECK(buf[0] == 'B' && buf[1] == 'M');
        CHECK(rd32(buf + 2) == 78 && rd32(buf + 10) == 54 && rd32(buf + 14) == 40);
        CHECK(rd32(buf + 18) == 3 && rd32(buf + 22) == 2);
        CHECK(buf[26] == 1 && buf[28] == 24 && rd32(buf + 34) == 24);
        CHECK(buf[54] == 0xCC && buf[55] == 0xBB && buf[56] == 0xAA);      /* first file row = bottom row, BGR */
        CHECK(buf[63] == 0 && buf[64] == 0 && buf[65] == 0);                /* pad */
        CHECK(buf[66] == 0x33 && buf[67] == 0x22 && buf[68] == 0x11);
    }
    lj_surface_free(&s);
    printf("bmp: %d failures\n", fails);
    return fails ? 1 : 0;
}
