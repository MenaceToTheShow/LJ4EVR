/* test_hotkeys.c - `lj4evr_tests test_hotkeys`: the function table (every row named,
 * captioned, grouped, round-trips by name), the default setup loads, F-keys always
 * fire, Alt never matches, Scroll Lock scope and Disable-in-dialogs gate, Shift and
 * Ctrl distinct, program rows carry a number, .Hot save/load round trip, bad files
 * refused, and lj_fn_run edge semantics (flash press+release, enable press only,
 * unbound = one logged no-op). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (as tests/core/test_textrec.c) */
#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "io/lj_fn.h"
#include "io/lj_hotkeys.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { LJ_T_VK_ESC = 0x1B, LJ_T_VK_MENU = 0x12, LJ_T_VK_F1 = 0x70, LJ_T_VK_F12 = 0x7B };

static char lj_t_log[256];
static int  lj_t_log_n;
static void lj_t_sink(void *ud, const char *line) { (void)ud; lj_t_log_n++; strncpy(lj_t_log, line, sizeof lj_t_log - 1); lj_t_log[sizeof lj_t_log - 1] = 0; }

static int lj_t_calls, lj_t_last_edge;
static uint32_t lj_t_last_num;
static void lj_t_handler(enum lj_fn fn, int edge, uint32_t prog_num, void *ud) { (void)fn; (void)ud; lj_t_calls++; lj_t_last_edge = edge; lj_t_last_num = prog_num; }

static void lj_t_write(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static lj_hotkey lj_t_key(uint16_t key, int shift, int ctrl, int scroll, enum lj_fn fn, uint32_t num)
{ lj_hotkey k; memset(&k, 0, sizeof k); k.key = key; k.shift = (uint8_t)shift; k.ctrl = (uint8_t)ctrl; k.scroll = (uint8_t)scroll; k.fn = (uint16_t)fn; k.prog_num = num; return k; }

int lj_test_hotkeys(int argc, char **argv);
int lj_test_hotkeys(int argc, char **argv)
{
    static lj_hotkeys a, b;   /* 512 entries each: keep them off the stack */
    lj_hotkey k;
    const lj_hotkey *e;
    char text[64], err[128], path[300];
    int i;
    (void)argc; (void)argv;

    /* ---- the function table ---- */
    i = LJ_FN_COUNT;
    CHECK(i > 300 && i < 1000);
    for (i = 1; i < LJ_FN_COUNT; i++) {
        const lj_fn_desc *d = &lj_fn_table[i];
        if (strncmp(d->name, "LJ_FN_", 6) != 0 || !d->caption[0] || d->group >= LJ_FNG_COUNT || lj_fn_by_name(d->name) != (enum lj_fn)i || lj_fn_caption((enum lj_fn)i, text, sizeof text) != 0) {
            fails++; printf("FAIL table row %d (%s)\n", i, d->name);
        }
    }
    CHECK(lj_fn_caption(LJ_FN_FADE_OUT_SEC_3, text, sizeof text) == 0 && strcmp(text, "Fade Out 3 Sec") == 0);
    CHECK(lj_fn_caption(LJ_FN_STATIC_7, text, sizeof text) == 0 && strcmp(text, "Static 07") == 0);
    CHECK(lj_fn_caption(LJ_FN_HTP_BUMP_FLASH_12, text, sizeof text) == 0 && strcmp(text, "HTP group 12 - Bump") == 0);
    CHECK(lj_fn_caption(LJ_FN_NONE, text, sizeof text) == -1 && lj_fn_caption(LJ_FN_COUNT, text, sizeof text) == -1);
    CHECK(lj_fn_by_name("LJ_FN_NOPE") == LJ_FN_NONE && lj_fn_by_name(NULL) == LJ_FN_NONE);
    CHECK(lj_fn_is_flash(LJ_FN_FREEZE_OUTPUT_FLASH) && lj_fn_is_flash(LJ_FN_PROG_SEQ_FLASH) && !lj_fn_is_flash(LJ_FN_CLOSE_WINDOW) && !lj_fn_is_flash(LJ_FN_PROG_CUE));
    CHECK(strcmp(lj_fn_group_names[LJ_FNG_HTP], "HTP groups") == 0 && strcmp(lj_fn_group_names[LJ_FNG_PROGRAMS], "Programs") == 0);
    CHECK(lj_fn_table[LJ_FN_PROG_BGCUE].group == LJ_FNG_PROGRAMS && lj_fn_table[LJ_FN_TOGGLE_SEQ_CAPTURE].group == LJ_FNG_CUE_BUILDER);

    /* ---- the default setup loads ---- */
    lj_hotkeys_default(&a);
    CHECK(lj_hotkeys_validate(&a) == NULL && a.n == 4 && a.enabled == 1 && a.disable_in_dialogs == 1 && strcmp(a.name, "Default") == 0);
    e = lj_hotkeys_find_fn(&a, LJ_FN_SEQUENCE_BLIND_FLASH, 0);
    CHECK(e && e->key == 'B' && e->shift == 1 && e->ctrl == 0);
    CHECK(lj_hotkeys_combo_text(e, text, sizeof text) == 0 && strcmp(text, "[Shift] B") == 0);
    CHECK(lj_hotkeys_match(&a, 'B', 1, 0, 0, 0, 0) == e);
    CHECK(lj_hotkeys_match(&a, 'B', 0, 0, 0, 0, 0) != e && lj_hotkeys_match(&a, 'B', 0, 0, 0, 0, 0)->fn == LJ_FN_SEQUENCE_BLIND_TOGGLE);   /* Shift distinct */
    CHECK(lj_hotkeys_match(&a, 'B', 0, 1, 0, 0, 0) == NULL);                                                                     /* Ctrl distinct */
    CHECK(lj_hotkeys_match(&a, 'B', 1, 0, 1, 0, 0) == NULL);                                                                     /* Alt never */
    CHECK(lj_hotkeys_match(&a, LJ_T_VK_ESC, 1, 0, 0, 0, 0) && lj_hotkeys_match(&a, LJ_T_VK_ESC, 1, 0, 0, 0, 0)->fn == LJ_FN_CLOSE_ALL_WINDOWS);
    CHECK(lj_hotkeys_match(&a, 'B', 1, 0, 0, 0, 1) == NULL);                                                                     /* Disable Hotkeys in Dialogs, default on */
    a.disable_in_dialogs = 0;
    CHECK(lj_hotkeys_match(&a, 'B', 1, 0, 0, 0, 1) == e);
    a.disable_in_dialogs = 1;

    /* ---- F-keys always fire, Scroll Lock scope toggles, Hotkeys Enabled ---- */
    k = lj_t_key(LJ_T_VK_F1, 0, 0, 1, LJ_FN_BLACKOUT_RESTORE, 0);  CHECK(lj_hotkeys_assign(&a, &k) == 0);
    k = lj_t_key(LJ_T_VK_F12, 1, 1, 1, LJ_FN_RESTORE, 0);          CHECK(lj_hotkeys_assign(&a, &k) == 0);
    k = lj_t_key('X', 0, 0, 1, LJ_FN_BLACKOUT, 0);                 CHECK(lj_hotkeys_assign(&a, &k) == 0);
    CHECK(lj_hotkeys_match(&a, LJ_T_VK_F1, 0, 0, 0, 0, 1) != NULL);          /* in a dialog, Scroll Lock off, scroll-scoped: still fires */
    CHECK(lj_hotkeys_match(&a, LJ_T_VK_F12, 1, 1, 0, 0, 1) != NULL);         /* Shift+Ctrl+F12 too */
    CHECK(lj_hotkeys_match(&a, LJ_T_VK_F1, 0, 0, 1, 1, 0) == NULL);          /* but never with Alt */
    CHECK(lj_hotkeys_match(&a, 'X', 0, 0, 0, 0, 0) == NULL);                 /* Scroll Lock off: scoped key silent */
    CHECK(lj_hotkeys_match(&a, 'X', 0, 0, 0, 1, 0) != NULL);                 /* Scroll Lock on: fires */
    CHECK(lj_hotkeys_match(&a, 'X', 0, 0, 0, 1, 1) == NULL);                 /* ... unless in a dialog */
    a.enabled = 0;
    CHECK(lj_hotkeys_match(&a, LJ_T_VK_F1, 0, 0, 0, 1, 0) == NULL && lj_hotkeys_match(&a, 'B', 1, 0, 0, 0, 0) == NULL);   /* Hotkeys Enabled off silences everything */
    a.enabled = 1;
    CHECK(lj_hotkeys_match(NULL, 'B', 1, 0, 0, 0, 0) == NULL);

    /* ---- assign rules, programs, clear, find ---- */
    k = lj_t_key(LJ_T_VK_MENU, 0, 0, 0, LJ_FN_RESTORE, 0);        CHECK(lj_hotkeys_assign(&a, &k) == -1);   /* Alt itself is not a key */
    k = lj_t_key(0, 0, 0, 0, LJ_FN_RESTORE, 0);                   CHECK(lj_hotkeys_assign(&a, &k) == -1);
    k = lj_t_key('Y', 0, 0, 0, LJ_FN_NONE, 0);                    CHECK(lj_hotkeys_assign(&a, &k) == -1);
    k = lj_t_key('Y', 0, 0, 0, LJ_FN_COUNT, 0);                   CHECK(lj_hotkeys_assign(&a, &k) == -1);
    k = lj_t_key('Y', 0, 0, 0, LJ_FN_RESTORE, 5);                 CHECK(lj_hotkeys_assign(&a, &k) == -1);   /* num only on program rows */
    k = lj_t_key('1', 0, 0, 0, LJ_FN_PROG_CUE, 0);                CHECK(lj_hotkeys_assign(&a, &k) == -1);   /* ... and always there */
    k = lj_t_key('1', 0, 0, 0, LJ_FN_PROG_CUE, 12);               CHECK(lj_hotkeys_assign(&a, &k) == 0);
    k = lj_t_key('1', 1, 0, 0, LJ_FN_PROG_SEQ_FLASH, 7);          CHECK(lj_hotkeys_assign(&a, &k) == 0);
    k = lj_t_key('1', 0, 1, 0, LJ_FN_PROG_CUELIST, 2);            CHECK(lj_hotkeys_assign(&a, &k) == 0);
    k = lj_t_key('2', 0, 1, 0, LJ_FN_PROG_BGCUE, 3);              CHECK(lj_hotkeys_assign(&a, &k) == 0);
    CHECK(a.n == 11);
    k = lj_t_key('1', 0, 0, 0, LJ_FN_PROG_CUE, 13);               CHECK(lj_hotkeys_assign(&a, &k) == 0 && a.n == 11);   /* same combo: replaced, not appended */
    CHECK(lj_hotkeys_find_fn(&a, LJ_FN_PROG_CUE, 13) != NULL && lj_hotkeys_find_fn(&a, LJ_FN_PROG_CUE, 12) == NULL);
    CHECK(lj_hotkeys_find_fn(&a, LJ_FN_PROG_SEQ_FLASH, 7)->shift == 1 && lj_hotkeys_find_fn(&a, LJ_FN_PROG_CUELIST, 2)->ctrl == 1);
    CHECK(lj_hotkeys_find_fn(&a, LJ_FN_FADE_IN, 0) == NULL);
    CHECK(lj_hotkeys_combo_text(lj_hotkeys_find_fn(&a, LJ_FN_RESTORE, 0), text, sizeof text) == 0 && strcmp(text, "[Shift] [Control] F12") == 0);
    CHECK(lj_hotkeys_combo_text(lj_hotkeys_find_fn(&a, LJ_FN_CLOSE_WINDOW, 0), text, sizeof text) == 0 && strcmp(text, "Esc") == 0);
    CHECK(lj_hotkeys_combo_text(NULL, text, sizeof text) == -1 && text[0] == 0);
    CHECK(lj_hotkeys_combo_text(e, text, 4) == -1);
    CHECK(lj_hotkeys_validate(&a) == NULL);

    /* ---- .Hot round trip through the setup path ---- */
    CHECK(lj_hotkeys_setup_path("lib/", 1, path, sizeof path) == 0 && strcmp(path, "lib/Hotkeys/Setup1.Hot") == 0);
    CHECK(lj_hotkeys_setup_path("lib/", 8, path, sizeof path) == 0 && strcmp(path, "lib/Hotkeys/Setup8.Hot") == 0);
    CHECK(lj_hotkeys_setup_path("lib/", 9, path, sizeof path) == -1 && lj_hotkeys_setup_path("lib/", 0, path, sizeof path) == -1);
    CHECK(lj_hotkeys_setup_path("lib/", 1, path, 8) == -1 && path[0] == 0);
    strcpy(a.name, "Say \"go\" twice");   /* a quote inside is doubled on disk */
    a.disable_in_dialogs = 0;
    CHECK(lj_hotkeys_save("test_hotkeys_Setup1.Hot", &a) == 0);
    memset(&b, 0x5A, sizeof b);
    CHECK(lj_hotkeys_load("test_hotkeys_Setup1.Hot", &b, err, sizeof err) == 0 && err[0] == 0);
    CHECK(b.n == a.n && b.enabled == 1 && b.disable_in_dialogs == 0 && strcmp(b.name, a.name) == 0);
    for (i = 0; i < a.n && i < b.n; i++)   /* field-wise: lj_hotkey has padding */
        if (a.e[i].key != b.e[i].key || a.e[i].shift != b.e[i].shift || a.e[i].ctrl != b.e[i].ctrl || a.e[i].scroll != b.e[i].scroll || a.e[i].fn != b.e[i].fn || a.e[i].prog_num != b.e[i].prog_num) {
            fails++; printf("FAIL round trip row %d\n", i);
        }
    a.disable_in_dialogs = 1;

    /* ---- refused files ---- */
    CHECK(lj_hotkeys_load("test_hotkeys_missing.Hot", &b, err, sizeof err) == -1 && err[0] != 0);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 hot 1\nname=\"x\"\nkey vk=66 shift=0 ctrl=0 scrl=0 fn=LJ_FN_NOT_A_FUNCTION\n");
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == -1 && strstr(err, "LJ_FN_NOT_A_FUNCTION") != NULL && b.n == 0);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 hot 1\nkey vk=66 shift=0 ctrl=0 scrl=0 fn=LJ_FN_RESTORE\nkey vk=66 shift=0 ctrl=0 scrl=0 fn=LJ_FN_BLACKOUT\n");
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == -1 && strstr(err, "duplicate") != NULL);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 hot 1\nkey vk=66 shift=0 ctrl=0 scrl=0 fn=LJ_FN_RESTORE num=4\n");
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == -1);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 hot 1\nkey vk=18 shift=0 ctrl=0 scrl=0 fn=LJ_FN_RESTORE\n");   /* VK_MENU as the key */
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == -1);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 seq 1\nnum=1\n");
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == -1);
    lj_t_write("test_hotkeys_bad.Hot", "LJ4 hot 1\nfuture=1\nkey vk=112 shift=0 ctrl=0 scrl=1 fn=LJ_FN_PROG_BGCUE num=9 extra=1\n");   /* unknown key and trailing field skipped */
    CHECK(lj_hotkeys_load("test_hotkeys_bad.Hot", &b, err, sizeof err) == 0 && b.n == 1 && b.e[0].fn == LJ_FN_PROG_BGCUE && b.e[0].prog_num == 9 && b.e[0].scroll == 1);
    b.e[0].fn = 0;
    CHECK(lj_hotkeys_validate(&b) != NULL && lj_hotkeys_save("test_hotkeys_bad.Hot", &b) == -1);   /* an invalid setup never reaches disk */

    /* ---- clear ---- */
    CHECK(lj_hotkeys_clear_key(&a, 'B', 1, 0) == 0 && lj_hotkeys_find_fn(&a, LJ_FN_SEQUENCE_BLIND_FLASH, 0) == NULL && a.n == 10);
    CHECK(lj_hotkeys_clear_key(&a, 'B', 1, 0) == -1);
    CHECK(lj_hotkeys_clear_fn(&a, LJ_FN_PROG_CUE, 12) == -1 && lj_hotkeys_clear_fn(&a, LJ_FN_PROG_CUE, 13) == 0 && a.n == 9);
    CHECK(lj_hotkeys_clear_fn(&a, LJ_FN_BLACKOUT, 0) == 0 && lj_hotkeys_match(&a, 'X', 0, 0, 0, 1, 0) == NULL && a.n == 8);
    CHECK(lj_hotkeys_validate(&a) == NULL);
    for (i = a.n; i < LJ_HOTKEYS_MAX; i++) { k = lj_t_key((uint16_t)(0x100 + i), 0, 0, 0, LJ_FN_FADE_IN, 0); CHECK(lj_hotkeys_assign(&a, &k) == 0); }
    k = lj_t_key(0x7FF, 0, 0, 0, LJ_FN_FADE_IN, 0);
    CHECK(a.n == LJ_HOTKEYS_MAX && lj_hotkeys_assign(&a, &k) == -1);   /* full */
    CHECK(lj_hotkeys_clear_fn(&a, LJ_FN_FADE_IN, 0) == 0 && a.n == 8);

    /* ---- dispatch edges: flash press+release, enable press only, unbound logs once ---- */
    lj_fn_unbind_all();
    CHECK(lj_fn_bind(LJ_FN_NONE, lj_t_handler, NULL) == -1 && lj_fn_bind(LJ_FN_COUNT, lj_t_handler, NULL) == -1);
    CHECK(lj_fn_bind(LJ_FN_SEQUENCE_BLIND_FLASH, lj_t_handler, NULL) == 0 && lj_fn_bound(LJ_FN_SEQUENCE_BLIND_FLASH) == 1);
    CHECK(lj_fn_bind(LJ_FN_CLOSE_WINDOW, lj_t_handler, NULL) == 0 && lj_fn_bind(LJ_FN_PROG_CUE, lj_t_handler, NULL) == 0);
    lj_t_calls = 0;
    lj_fn_run(LJ_FN_SEQUENCE_BLIND_FLASH, 1);  CHECK(lj_t_calls == 1 && lj_t_last_edge == 1);
    lj_fn_run(LJ_FN_SEQUENCE_BLIND_FLASH, 0);  CHECK(lj_t_calls == 2 && lj_t_last_edge == 0);           /* flash: release is an edge */
    lj_fn_run(LJ_FN_CLOSE_WINDOW, 1);          CHECK(lj_t_calls == 3 && lj_t_last_edge == 1);
    lj_fn_run(LJ_FN_CLOSE_WINDOW, 0);          CHECK(lj_t_calls == 3);                                   /* enable: release ignored */
    lj_fn_run_prog(LJ_FN_PROG_CUE, 13, 1);     CHECK(lj_t_calls == 4 && lj_t_last_num == 13);
    lj_fn_run_prog(LJ_FN_PROG_CUE, 13, 0);     CHECK(lj_t_calls == 4);
    lj_log_set_sink(lj_t_sink, NULL);
    lj_t_log_n = 0; lj_t_log[0] = 0;
    lj_fn_run(LJ_FN_RESTORE, 1);
    CHECK(lj_t_log_n == 1 && strstr(lj_t_log, "no binding for LJ_FN_RESTORE") != NULL);
    lj_fn_run(LJ_FN_RESTORE, 1);
    CHECK(lj_t_log_n == 1);                                                                              /* logged once */
    lj_fn_run(LJ_FN_NONE, 1); lj_fn_run(LJ_FN_COUNT, 1);
    CHECK(lj_t_log_n == 1 && lj_t_calls == 4);
    lj_log_set_sink(NULL, NULL);
    /* the keyboard path end to end: Shift+Esc down then up */
    e = lj_hotkeys_match(&a, LJ_T_VK_ESC, 1, 0, 0, 0, 0);
    CHECK(e && lj_fn_bind((enum lj_fn)e->fn, lj_t_handler, NULL) == 0);
    lj_fn_run_prog((enum lj_fn)e->fn, e->prog_num, 1);  CHECK(lj_t_calls == 5);
    lj_fn_run_prog((enum lj_fn)e->fn, e->prog_num, 0);  CHECK(lj_t_calls == 5);
    lj_fn_unbind_all();
    CHECK(lj_fn_bound(LJ_FN_SEQUENCE_BLIND_FLASH) == 0);

    remove("test_hotkeys_Setup1.Hot");
    remove("test_hotkeys_bad.Hot");
    printf("test_hotkeys: %d failure(s), %d functions, %d groups\n", fails, LJ_FN_COUNT - 1, LJ_FNG_COUNT);
    return fails ? 1 : 0;
}
