/* lj_render_main.c - lj4evr_render, the headless BMP renderer (design 2.9). SDL-free.
 *   lj4evr_render <dir>                      instantiates every generated form with no bindings, paints, writes <dir>/<CLASS>.bmp
 *                                            (class name upper-cased = the DFM file stem) plus MAINWINDOW.bmp, the code-built stub
 *   lj4evr_render --font-proof <face> <bmp>  draws every glyph of sans13|sans13b|arial14|arial14b|arialblack16 in a 16-column grid */
#include <stdio.h>
#include <string.h>
#include "desktop/lj_main_window.h"
#include "gfx/lj_bmp.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"
#include "version.h"

static int lj_render_window(lj_window *win, const char *dir, const char *stem)
{
    char path[1024];
    size_t i;
    if (!win) { printf("  %s: lj_window_create failed\n", stem); return -1; }
    snprintf(path, sizeof path, "%s/%s.bmp", dir, stem);
    for (i = strlen(dir) + 1; path[i] != '.'; i++) if (path[i] >= 'a' && path[i] <= 'z') path[i] = (char)(path[i] - 'a' + 'A');   /* the stem only */
    lj_window_paint(win);
    return lj_bmp_write(lj_window_surface(win), path);
}

static int lj_render_font_proof(const char *face, const char *out)
{
    static const struct { const char *name; const lj_font *f; } faces[] = {
        { "sans13", &lj_font_sans13 }, { "sans13b", &lj_font_sans13b }, { "arial14", &lj_font_arial14 },
        { "arial14b", &lj_font_arial14b }, { "arialblack16", &lj_font_arialblack16 } };
    const lj_font *f = NULL;
    lj_surface s;
    char str[2] = { 0, 0 };
    int i, cw = 0, ch, rc;
    for (i = 0; i < LJ_COUNT_OF(faces); i++) if (strcmp(faces[i].name, face) == 0) f = faces[i].f;
    if (!f) { fprintf(stderr, "unknown face %s\n", face); return 2; }
    for (i = 0; i < LJ_FONT_GLYPHS; i++) if (f->advance[i] > cw) cw = f->advance[i];
    cw += 2; ch = f->height + 2;
    if (lj_surface_init(&s, 16 * cw, (LJ_FONT_GLYPHS / 16) * ch) != 0) return 1;
    lj_fill(&s, 0, 0, s.w, s.h, LJ_CLR_BTNHIGHLIGHT);
    for (i = 0; i < LJ_FONT_GLYPHS; i++) {
        str[0] = (char)(LJ_FONT_FIRST + i);
        lj_text(&s, f, (i % 16) * cw + 1, (i / 16) * ch + 1, str, LJ_CLR_3DDKSHADOW, 0);
    }
    rc = lj_bmp_write(&s, out);
    lj_surface_free(&s);
    printf("font proof %s: %d glyphs, %dx%d -> %s\n", face, LJ_FONT_GLYPHS, s.w, s.h, out);
    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const lj_form_desc *const *forms;
    const char *dir;
    lj_window *win;
    int n, i, ok = 0, bad = 0;

    if (argc == 4 && strcmp(argv[1], "--font-proof") == 0) return lj_render_font_proof(argv[2], argv[3]);
    if (argc != 2) { fprintf(stderr, "usage: lj4evr_render <out dir> | --font-proof <face> <out.bmp>\n"); return 2; }
    dir = argv[1];
    forms = lj_forms_all(&n);
    printf("lj4evr_render %s: %d form(s)\n", LJ_VERSION_STR, n);
    for (i = 0; i < n; i++) {
        fprintf(stderr, "render %s\n", forms[i]->class_name);
        win = lj_window_create(forms[i], NULL);
        if (lj_render_window(win, dir, forms[i]->class_name) == 0) ok++; else bad++;
        lj_window_destroy(win);
    }
    win = lj_main_window_create(NULL);
    if (lj_render_window(win, dir, "MAINWINDOW") == 0) ok++; else bad++;
    lj_main_window_destroy(win);
    printf("lj4evr_render: %d written, %d failed -> %s\n", ok, bad, dir);
    return bad ? 1 : 0;
}
