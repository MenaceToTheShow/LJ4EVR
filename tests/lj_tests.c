/* lj_tests.c - the CTest driver: lj4evr_tests <subcommand> [args]. No SDL.
 * A track adds a row here and the same name to LJ_TEST_SUBCOMMANDS in CMakeLists.txt.
 * A subcommand returns 0 on pass, non-zero on fail, and prints its own evidence. */
#include <stdio.h>
#include <string.h>

int lj_test_widget(int argc, char **argv);
int lj_test_widgets_lists(int argc, char **argv);
int lj_test_render(int argc, char **argv);
int lj_test_draw_goldens(int argc, char **argv);
int lj_test_bmp(int argc, char **argv);
int lj_test_text(int argc, char **argv);
int lj_test_encoding(int argc, char **argv);
int lj_test_ini(int argc, char **argv);
int lj_test_textrec(int argc, char **argv);
int lj_test_lock(int argc, char **argv);
int lj_test_clock_monotonic(int argc, char **argv);
int lj_test_trig(int argc, char **argv);
int lj_test_engine_stub(int argc, char **argv);
int lj_test_dump_frames(int argc, char **argv);
int lj_test_font_corpus(int argc, char **argv);
int lj_test_font_cells(int argc, char **argv);
int lj_test_art_table(int argc, char **argv);
int lj_test_forms_tables(int argc, char **argv);
int lj_test_window(int argc, char **argv);
int lj_test_menu(int argc, char **argv);
int lj_test_formsini(int argc, char **argv);
int lj_test_hittest(int argc, char **argv);
int lj_test_widgets_lj(int argc, char **argv);
int lj_test_widgets_basic(int argc, char **argv);

static const struct { const char *name; int (*fn)(int, char **); } lj_subcommands[] = {
    { "font_corpus", lj_test_font_corpus },
    { "font_cells", lj_test_font_cells },
    { "art_table", lj_test_art_table },
    { "widget", lj_test_widget },
    { "widgets_lists", lj_test_widgets_lists },
    { "forms_tables", lj_test_forms_tables },
    { "window", lj_test_window },
    { "menu", lj_test_menu },
    { "formsini", lj_test_formsini },
    { "hittest", lj_test_hittest },
    { "render", lj_test_render },
    { "draw_goldens", lj_test_draw_goldens },
    { "bmp", lj_test_bmp },
    { "text", lj_test_text },
    { "encoding", lj_test_encoding },
    { "ini", lj_test_ini },
    { "textrec", lj_test_textrec },
    { "lock", lj_test_lock },
    { "clock_monotonic", lj_test_clock_monotonic },
    { "trig", lj_test_trig },
    { "engine_stub", lj_test_engine_stub },
    { "dump-frames", lj_test_dump_frames },      /* dump-frames [path], default frames.bin (CI determinism job) */
    { "widgets_lj", lj_test_widgets_lj },
    { "widgets_basic", lj_test_widgets_basic },
};

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: lj4evr_tests <subcommand>\n");
        for (i = 0; i < (int)(sizeof lj_subcommands / sizeof lj_subcommands[0]); i++) fprintf(stderr, "  %s\n", lj_subcommands[i].name);
        return 2;
    }
    for (i = 0; i < (int)(sizeof lj_subcommands / sizeof lj_subcommands[0]); i++)
        if (strcmp(argv[1], lj_subcommands[i].name) == 0) return lj_subcommands[i].fn(argc - 1, argv + 1);
    fprintf(stderr, "unknown subcommand %s\n", argv[1]);
    return 2;
}
