/* test_uiscript.c - the runnable check for src/app/lj_uiscript.c and lj_startup.c
 * (app track, W1): every verb parses, a bad line is rejected with its number, a
 * headless script clicks a TSEQUENCEFORM check box and asserts its state, an
 * assert failure stops with its line, a missing Default library is recreated
 * (model) and LastRun is written, lj_locale_decimal() reads ',' under a comma
 * locale (ME-52), and a non-CP1252 pref path round-trips through forms.ini (ME-65). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <locale.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define LJ_MKDIR(p) _mkdir(p)
#define LJ_RMDIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define LJ_MKDIR(p) mkdir(p, 0777)
#define LJ_RMDIR(p) rmdir(p)
#endif
#include "app/lj_startup.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_locale.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "version.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int hits[8];
static void count(lj_widget *w, void *ud) { (void)w; (*(int *)ud)++; }

static lj_window *seq_win;
static lj_window *resolve(void *ud, const char *form)
{
    (void)ud;
    return (strcmp(form, "TSequenceForm") == 0 || strcmp(form, "seq_form") == 0) ? seq_win : NULL;
}

static int write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static int run_script(const char *path)
{
    int r = lj_uiscript_load(path);
    uint64_t t0 = lj_clock_us();
    if (r != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 10000000u) { }   /* a `wait` spins here; 10 s cap */
    return r;
}

static int file_exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) fclose(f); return f != NULL; }

int lj_test_uiscript(int argc, char **argv);
int lj_test_uiscript(int argc, char **argv)
{
    static const lj_binding seq_b[] = {
        { "FormActivate", count, &hits[0] }, { "FormClose", count, &hits[0] }, { "FormCreate", count, &hits[0] }, { "FormDestroy", count, &hits[0] },
        { "FormHide", count, &hits[0] }, { "FormShow", count, &hits[0] }, { "cbSyncClick", count, &hits[1] }, { "cbSyncEnter", count, &hits[0] },
        { "cbGlobalFadeClick", count, &hits[0] }, { "cbGlobalSceneClick", count, &hits[0] }, { "UdSceneChange", count, &hits[0] }, { "UdFadeChange", count, &hits[0] },
        { "DisableFade1Click", count, &hits[0] }, { "Preferences1Click", count, &hits[0] }, { "M_KeepBlindClick", count, &hits[0] }, { "M_SnapShotClick", count, &hits[0] },
        { "Help1Click", count, &hits[0] }, { NULL, NULL, NULL } };
    static const char every_verb[] =
        "# every verb of docs/ui-script.md\n"
        "open seq_form\n"
        "close seq_form\n"
        "click TSequenceForm.cbSync\n"
        "click TSequenceForm.cbSync 3 4\n"
        "rclick TSequenceForm.sb7\n"
        "dblclick TSequenceForm.sb7\n"
        "drag TSequenceForm.sb1 TSequenceForm.sb2 2 2\n"
        "type \"ab\"\"c\"   # a doubled quote\n"
        "key Ctrl+Shift+F1\n"
        "wait 10\n"
        "shot x.png\n"
        "assert_caption TSequenceForm \"Sequence\"\n"
        "assert_text TSequenceForm.ES \"88:88\"\n"
        "assert_enabled TSequenceForm.cbSync 1\n"
        "assert_visible TSequenceForm.cbSync 0\n"
        "assert_frame 1:2=255 3\n"
        "assert_view gm=255 blackout=0\n"
        "assert_file x.png missing\n"
        "assert_pps 1 1 80\n"
        "log \"hello\"\n"
        "quit\n";
    static const char headless[] =
        "assert_caption TSequenceForm \"Sequence\"\n"
        "assert_enabled TSequenceForm.cbSync 1\n"
        "assert_visible TSequenceForm.cbSync 1\n"
        "click TSequenceForm.cbSync\n"
        "click TSequenceForm.ES\n"
        "key End\n"
        "type \"x\"\n"
        "wait 5\n"
        "shot lj_test_uiscript_shot.png\n"
        "assert_file lj_test_uiscript_shot-TSequenceForm.png exists\n"
        "log \"headless ok\"\n"
        "quit\n";
    lj_uiscript_host host;
    lj_widget *cb, *es;
    lj_library lib;
    lj_ini *ini;
    int r, x = -1, y = -1;
    const char *loc;
    char path[LJ_PATH_CAP];
    (void)argc; (void)argv;

    /* 1. every verb parses */
    CHECK(write_file("lj_test_uiscript_all.lj4s", every_verb) == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_all.lj4s") == 0);
    CHECK(lj_uiscript_active());

    /* 2. a bad line is rejected with its number */
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "# c\nopen seq_form\nbogus 1\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 3 && !lj_uiscript_active());
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "wait abc\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 1);
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "\n\nclick NoDot\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 3);
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "log \"unterminated\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 1);
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "key Meta+X\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 1);
    CHECK(write_file("lj_test_uiscript_bad.lj4s", "assert_frame 5:1=0\n") == 0);
    CHECK(lj_uiscript_load("lj_test_uiscript_bad.lj4s") == -1 && lj_uiscript_line() == 1);
    CHECK(lj_uiscript_load("lj_test_uiscript_missing.lj4s") == -1 && lj_uiscript_line() == 0);

    /* 3. headless: click the check box, type into an edit, shot, assert */
    seq_win = lj_window_create(&lj_form_TSEQUENCEFORM, seq_b);
    CHECK(seq_win != NULL);
    if (!seq_win) return 1;
    cb = lj_window_find(seq_win, "cbSync"); es = lj_window_find(seq_win, "ES");
    CHECK(cb && es && lj_w_checkbox_checked(cb) == 0);
    host.ud = NULL; host.resolve = resolve; host.view = NULL;
    lj_uiscript_set_host(&host);
    CHECK(write_file("lj_test_uiscript_headless.lj4s", headless) == 0);
    r = run_script("lj_test_uiscript_headless.lj4s");
    CHECK(r == 0);
    CHECK(!lj_uiscript_active());
    CHECK(lj_w_checkbox_checked(cb) == 1 && hits[1] == 1);
    CHECK(strcmp(lj_w_edit_text(es), "88:88x") == 0);
    CHECK(file_exists("lj_test_uiscript_shot-TSequenceForm.png"));

    /* 4. an assert failure stops with its line; a view assert without a host view fails */
    CHECK(write_file("lj_test_uiscript_fail.lj4s", "log \"first\"\nassert_enabled TSequenceForm.cbSync 0\nquit\n") == 0);
    CHECK(run_script("lj_test_uiscript_fail.lj4s") == -1 && lj_uiscript_line() == 2);
    CHECK(write_file("lj_test_uiscript_fail.lj4s", "assert_view gm=255\n") == 0);
    CHECK(run_script("lj_test_uiscript_fail.lj4s") == -1 && lj_uiscript_line() == 1);
    CHECK(write_file("lj_test_uiscript_fail.lj4s", "open warnings\n") == 0);   /* a window id with no open row (lj_win_warning drives it); cue_form opens since windows-b landed */
    CHECK(run_script("lj_test_uiscript_fail.lj4s") == -1 && lj_uiscript_line() == 1);
    lj_uiscript_set_host(NULL);
    lj_window_destroy(seq_win); seq_win = NULL;

    /* 5. startup: a missing Default library is recreated (model track) and LastRun is written */
    LJ_MKDIR("lj_test_uiscript_userdir");
    r = lj_startup_library("lj_test_uiscript_userdir/", NULL, &lib);
    if (r == 0) {
        CHECK(strcmp(lib.name, "Default") == 0 && strstr(lib.root, "Default") != NULL);
        CHECK(lj_library_path(&lib, "DirInfo.Ini", path, sizeof path) == 0 && file_exists(path));
    } else printf("SKIP: lj_library_open_default returned -1 (model stub); the fallback root is %s\n", lib.root);
    CHECK(strcmp(lib.name, "Default") == 0);
    r = lj_startup_library("lj_test_uiscript_userdir/", "NoSuchLib", &lib);   /* an unknown U:<folder> falls back to Default */
    CHECK(strcmp(lib.name, "Default") == 0);
    ini = lj_ini_new();
    lj_startup_last_run(ini);
    CHECK(strcmp(lj_ini_gets(ini, "OTHER", "LastRun", ""), "LJ4EVR " LJ_VERSION_STR) == 0);
    CHECK(lj_ini_geti(ini, "OTHER", "FirstRun", 1) == 0);
    lj_ini_free(ini);

    /* 6. ME-52: a comma locale reads ',' (skipped where the locale is not installed) */
    CHECK(lj_locale_decimal() == '.');
    loc = setlocale(LC_NUMERIC, "da-DK");
    if (!loc) loc = setlocale(LC_NUMERIC, "da_DK.UTF-8");
    if (!loc) loc = setlocale(LC_NUMERIC, "de_DE.UTF-8");
    if (loc) { CHECK(lj_locale_decimal() == ','); printf("locale %s: decimal '%c'\n", loc, lj_locale_decimal()); }
    else printf("SKIP: no comma locale installed (da-DK / da_DK.UTF-8 / de_DE.UTF-8)\n");
    setlocale(LC_NUMERIC, "C");
    CHECK(lj_locale_decimal() == '.');

    /* 7. ME-65: a pref path with a non-CP1252 code point (U+0411, UTF-8 D0 91) round-trips through forms.ini */
    LJ_MKDIR("lj_test_uiscript_\xD0\x91");
    ini = lj_ini_new();
    lj_ini_seti(ini, "SEQ", "Left", 123); lj_ini_seti(ini, "SEQ", "Top", 45);
    CHECK(lj_ini_save(ini, "lj_test_uiscript_\xD0\x91/forms.ini") == 0);
    lj_ini_free(ini);
    ini = lj_ini_load("lj_test_uiscript_\xD0\x91/forms.ini");
    CHECK(ini != NULL);
    if (ini) { x = lj_ini_geti(ini, "SEQ", "Left", -1); y = lj_ini_geti(ini, "SEQ", "Top", -1); lj_ini_free(ini); }
    CHECK(x == 123 && y == 45);
    remove("lj_test_uiscript_\xD0\x91/forms.ini");
    LJ_RMDIR("lj_test_uiscript_\xD0\x91");            /* the userdir keeps the library the model created: a non-empty tree, left in the build dir */

    remove("lj_test_uiscript_all.lj4s"); remove("lj_test_uiscript_bad.lj4s"); remove("lj_test_uiscript_headless.lj4s"); remove("lj_test_uiscript_fail.lj4s");
    remove("lj_test_uiscript_shot-TSequenceForm.png");
    printf("uiscript: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
