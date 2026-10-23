/* lj_da2532.c - the 2532 Direct Access family (lj_da2532.h; W4 da2532-fingers track, D16, I8, F8, notes 05 s4).
 * TDa2532Form_3 binds FontClick FormActivate FormClose FormCreate FormDestroy FormResize FormShow FunctionsList1Click
 * GridClick GridDragDrop GridDragOver GridDrawCell GridMouseDown GridMouseMove GridMouseUp GridSelectCell Help1Click
 * Load2532Setup1Click M_ClearAllPagesClick M_ClickAutoFlashClick M_ImportOldClick M_NamePageTabsClick M_PrefsToggle
 * M_SetTabSizeClick M_TabColorEnableClick New2532Setup1Click P_GridPasteClick p_PasteFlashClick p_PasteLatchClick
 * pCueDblClick pnRecordClick Pop_GridClick Pop_UserGridClick PopGridPopup PopSetCurrent PopUserPopup Preferences1Click
 * PrintLayout1Click Save2532Setup1Click sbFlashToggleMouseUp Set1Click tbcChange tbcDrawTab Timer1Timer
 * UseColoredTabs1Click UserGridDragDrop UserGridDragOver UserGridDrawCell UserGridMouseDown UserGridMouseUp (ME-16);
 * the six sub-dialogs bind theirs below. Never serial: Import old-style Setup (the undocumented 2532.cfg / .D25) and
 * Print Layout (TPrint2532Form) are greyed with LJ_NA_HINT. */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf under /W4 /WX on MSVC (as io/lj_midi.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows/lj_da2532.h"

#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "io/lj_fn.h"
#include "io/lj_midi.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_updown.h"
#include "windows/lj_warnings.h"

#define LJ_DA_STAY_ON_TOP 2u              /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/); fsStayOnTop */
#define LJ_DA_FILE        "2532Setup.Ini" /* ours (design 5.2 spirit, INI grammar); LJ2's 2532_2.Cfg is binary and undocumented */
#define LJ_DA_SEC         "2532"          /* Main.Ini section of the window preferences (keys = the menu item names) */
#define LJ_DA_DND_FN      0x7532          /* lj_dnd kind private to this family: a function from the 2532 Function List */
#define LJ_DA_NA_HINT     "Not available in LJ4EVR"
#define LJ_DA_SEL_RGB     0x00FFFFu       /* the selected key: cyan in 2532control_1.gif (reduced), PROPOSAL rig 05 s4 */
#define LJ_DA_FIX_W       16              /* the bank-letter column; measured from 2532matrixkeys.gif (reduced), PROPOSAL rig 05 s4 */

static const char *const lj_da_words[LJ_DA_TYPES] = { "", "fn", "seq", "cue", "cl", "bg" };
static const char *const lj_da_prefix[LJ_DA_TYPES] = { "", "", "Seq ", "Cue ", "Cuelist ", "BG Cue " };   /* PROPOSAL rig 05 s4: LJ2 shows the record name */
/* Color Scheme defaults (Text, Backgr.) per type: sampled from 2532colorscheme.gif (reduced, palette colours), PROPOSAL rig 05 s4 */
static const uint32_t lj_da_text_dflt[LJ_DA_TYPES] = { 0x000000u, 0x000000u, 0xFF8040u, 0xFF0000u, 0x008000u, 0x0000FFu };
static const uint32_t lj_da_back_dflt[LJ_DA_TYPES] = { 0xFFFFFFu, 0xEFEFEFu, 0xFFD7C4u, 0xFFA4A4u, 0xB7FFB7u, 0xC4C4FFu };
/* The 16 VGA colours ColorPanelClick steps through (ponytail: TColorDialog is not built, as generic DMX's scheme) */
static const uint32_t lj_da_vga[16] = { 0x000000u, 0x800000u, 0x008000u, 0x808000u, 0x000080u, 0x800080u, 0x008080u, 0xC0C0C0u,
                                        0x808080u, 0xFF0000u, 0x00FF00u, 0xFFFF00u, 0x0000FFu, 0xFF00FFu, 0x00FFFFu, 0xFFFFFFu };
/* the Preferences toggles (M_PrefsToggle) in Main.Ini [2532]; defaults = the DFM's Checked */
enum { P_SHOWCONTROL, P_SHOWLARGE, P_HIDESTATUS, P_MOUSESELECT, P_RELEASEFLASH, P_TRANSPARENT, P_SYNCPAGE, P_AUTORELEASE,
       P_SHOWUSER, P_WRAPUSER, P_AUTORESIZE, P_WRAPGRID, P_ALWAYSSPLIT, P_INFO, P_INFOCURSOR, P_INFOSEL, P_TABCOLORS, P_COUNT };
static const struct { const char *item; int dflt; } lj_da_pref[P_COUNT] = {
    { "M_ShowControl", 0 }, { "M_ShowLarge", 1 }, { "M_HideStatusPanelonLargeControl", 0 }, { "M_EnableMouseSelect", 0 },
    { "M_ReleaseFlashMouse", 0 }, { "M_TransparentPage", 0 }, { "M_SyncPage", 1 }, { "M_Autorelease", 0 }, { "M_ShowSpecial", 1 },
    { "M_WrapUserGrid", 1 }, { "M_AutoResize", 1 }, { "M_WrapGrid", 0 }, { "M_AlwaysSplit", 0 }, { "M_EnableInfo", 0 },
    { "M_ShowInfoCursor", 0 }, { "M_ShowInfoSelected", 0 }, { "M_TabColorEnable", 0 } };

/* ---- the model ---- */
static struct {
    lj_da_val latch[LJ_DA_PAGES][LJ_DA_BANKS][LJ_DA_KEYS], flash[LJ_DA_PAGES][LJ_DA_BANKS][LJ_DA_KEYS], user[LJ_DA_USER];
    char page_name[LJ_DA_PAGES][LJ_DA_TEXT];
    int page, bank;                                   /* 0-based */
    int record;                                       /* enum lj_da2532_type armed by a Record panel, LJ_DA_NONE = off */
    lj_da_val held_latch[LJ_DA_KEYS], held_flash[LJ_DA_KEYS], held_user[LJ_DA_USER];   /* what a press fired: its release reaches the same value */
    char last[48];                                    /* lFunction: the last function run */
    int loaded;
} da;
static lj_da_val lj_da_tmp_l[LJ_DA_PAGES][LJ_DA_BANKS][LJ_DA_KEYS], lj_da_tmp_f[LJ_DA_PAGES][LJ_DA_BANKS][LJ_DA_KEYS];   /* load builds here, commits whole */

static int in(int v, int lo, int hi) { return v >= lo && v <= hi; }
static int pref(int p) { return lj_ini_geti(lj_ctx_get()->main_ini, LJ_DA_SEC, lj_da_pref[p].item, lj_da_pref[p].dflt) != 0; }
static uint32_t ini_rgb(const char *key, uint32_t dflt) { return (uint32_t)lj_ini_geti(lj_ctx_get()->main_ini, LJ_DA_SEC, key, (int)dflt) & 0xFFFFFFu; }
static uint32_t text_rgb(int t) { char k[16]; snprintf(k, sizeof k, "ColorText%d", t); return ini_rgb(k, lj_da_text_dflt[t]); }
static uint32_t back_rgb(int t) { char k[16]; snprintf(k, sizeof k, "ColorBack%d", t); return ini_rgb(k, lj_da_back_dflt[t]); }

const char *lj_da2532_label(const lj_da_val *v, char *dst, size_t cap)
{
    if (!dst || !cap) return "";
    dst[0] = 0;
    if (!v || v->type == LJ_DA_NONE || v->type >= LJ_DA_TYPES) return dst;
    if (v->text[0]) snprintf(dst, cap, "%s", v->text);
    else if (v->type == LJ_DA_FN) lj_fn_caption((enum lj_fn)v->num, dst, cap);
    else snprintf(dst, cap, "%s%u", lj_da_prefix[v->type], (unsigned)v->num);
    return dst;
}

static int val_ok(int type, uint32_t num, int flash)
{
    if (type == LJ_DA_NONE) return 1;
    if (type == LJ_DA_FN) return num > 0 && num < LJ_FN_COUNT && lj_fn_table[num].group != LJ_FNG_PROGRAMS;
    if (num == 0 || num > 99999999u) return 0;                        /* 8-digit record names [F28] */
    return flash ? type == LJ_DA_SEQ : (type == LJ_DA_CUE || type == LJ_DA_CL || type == LJ_DA_BG);   /* 05 s4.2: latch = programs, flash = sequences; functions on both */
}

int lj_da2532_assign(int page, int bank, int key, int flash, int type, uint32_t num)
{
    lj_da_val *v;
    if (!in(page, 1, LJ_DA_PAGES) || !in(bank, 1, LJ_DA_BANKS) || !in(key, 1, LJ_DA_KEYS) || !in(type, 0, LJ_DA_TYPES - 1) || !val_ok(type, num, flash)) return -1;
    v = flash ? &da.flash[page - 1][bank - 1][key - 1] : &da.latch[page - 1][bank - 1][key - 1];
    memset(v, 0, sizeof *v);                                          /* Customize Text "cleared when reassigned" (05 s4.3) */
    v->type = (uint8_t)type; v->num = type ? num : 0;
    return 0;
}
int lj_da2532_assign_user(int key, uint32_t fn)
{
    if (!in(key, 1, LJ_DA_USER) || (fn != LJ_FN_NONE && !val_ok(LJ_DA_FN, fn, 0))) return -1;   /* "functions only (never programs)" (05 s4.2) */
    memset(&da.user[key - 1], 0, sizeof da.user[0]);
    if (fn != LJ_FN_NONE) { da.user[key - 1].type = LJ_DA_FN; da.user[key - 1].num = fn; }
    return 0;
}
const lj_da_val *lj_da2532_value(int page, int bank, int key, int flash)
{
    if (!in(page, 1, LJ_DA_PAGES) || !in(bank, 1, LJ_DA_BANKS) || !in(key, 1, LJ_DA_KEYS)) return NULL;
    return flash ? &da.flash[page - 1][bank - 1][key - 1] : &da.latch[page - 1][bank - 1][key - 1];
}
void lj_da2532_clear_all(void)
{
    memset(da.latch, 0, sizeof da.latch); memset(da.flash, 0, sizeof da.flash); memset(da.user, 0, sizeof da.user);
    memset(da.page_name, 0, sizeof da.page_name);
}

/* ---- the file: [PAGEp] <bank letter><key>L / F = "<word> <number|fn name>", <..>LT / FT = custom text, Name; [USER] Un, UnT ---- */
static int parse_val(const char *s, int flash, lj_da_val *out)
{
    char word[8], arg[64], *end;
    int t;
    unsigned long n;
    memset(out, 0, sizeof *out);
    if (!s || !*s) return 0;
    if (strlen(s) >= sizeof arg || sscanf(s, "%7s %63s", word, arg) != 2) return -1;
    for (t = 1; t < LJ_DA_TYPES && strcmp(word, lj_da_words[t]) != 0; t++) {}
    if (t >= LJ_DA_TYPES) return -1;
    if (t == LJ_DA_FN) n = (unsigned long)lj_fn_by_name(arg);
    else { n = strtoul(arg, &end, 10); if (*end || arg[0] == '-') return -1; }
    if (!val_ok(t, (uint32_t)n, flash) || (flash < 0 && t != LJ_DA_FN)) return -1;   /* flash < 0: a user key */
    out->type = (uint8_t)t; out->num = (uint32_t)n;
    return 0;
}
static void put_val(lj_ini *ini, const char *sec, const char *key, const lj_da_val *v)
{
    char buf[80], k2[16];
    if (v->type == LJ_DA_NONE) return;
    snprintf(buf, sizeof buf, "%s %s", lj_da_words[v->type], v->type == LJ_DA_FN ? lj_fn_table[v->num].name : "");
    if (v->type != LJ_DA_FN) snprintf(buf, sizeof buf, "%s %u", lj_da_words[v->type], (unsigned)v->num);
    lj_ini_sets(ini, sec, key, buf);
    if (v->text[0]) { snprintf(k2, sizeof k2, "%sT", key); lj_ini_sets(ini, sec, k2, v->text); }
}
static void get_text(lj_ini *ini, const char *sec, const char *key, char dst[LJ_DA_TEXT])
{
    char k2[16];
    snprintf(k2, sizeof k2, "%sT", key);
    snprintf(dst, LJ_DA_TEXT, "%s", lj_ini_gets(ini, sec, k2, ""));   /* clamped to Edit1 MaxLength 20 */
}
static int lib_file(char *path, size_t cap) { return lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, LJ_DA_FILE, path, cap) == 0 ? 0 : -1; }

int lj_da2532_save(void)
{
    char path[LJ_PATH_CAP], sec[16], key[16];
    lj_ini *ini;
    int p, b, k, rc;
    if (lib_file(path, sizeof path) != 0 || (ini = lj_ini_new()) == NULL) return -1;
    lj_ini_seti(ini, "SETUP", "Version", 1); lj_ini_seti(ini, "SETUP", "Page", da.page + 1); lj_ini_seti(ini, "SETUP", "Bank", da.bank + 1);
    for (p = 0; p < LJ_DA_PAGES; p++) {
        snprintf(sec, sizeof sec, "PAGE%d", p + 1);
        if (da.page_name[p][0]) lj_ini_sets(ini, sec, "Name", da.page_name[p]);
        for (b = 0; b < LJ_DA_BANKS; b++)
            for (k = 0; k < LJ_DA_KEYS; k++) {
                snprintf(key, sizeof key, "%c%dL", 'A' + b, k + 1); put_val(ini, sec, key, &da.latch[p][b][k]);
                snprintf(key, sizeof key, "%c%dF", 'A' + b, k + 1); put_val(ini, sec, key, &da.flash[p][b][k]);
            }
    }
    for (k = 0; k < LJ_DA_USER; k++) { snprintf(key, sizeof key, "U%d", k + 1); put_val(ini, "USER", key, &da.user[k]); }
    rc = lj_ini_save(ini, path);
    lj_ini_free(ini);
    if (rc != 0) lj_log("2532: cannot save %s", path);
    return rc;
}

int lj_da2532_load(void)
{
    static lj_da_val user[LJ_DA_USER];
    static char names[LJ_DA_PAGES][LJ_DA_TEXT];
    char path[LJ_PATH_CAP], sec[16], key[16];
    lj_ini *ini;
    int p, b, k, bad = 0, page, bank;
    da.loaded = 1;
    if (lib_file(path, sizeof path) != 0) return 0;
    if ((ini = lj_ini_load(path)) == NULL) return 0;                 /* missing = an empty setup */
    memset(lj_da_tmp_l, 0, sizeof lj_da_tmp_l); memset(lj_da_tmp_f, 0, sizeof lj_da_tmp_f); memset(user, 0, sizeof user); memset(names, 0, sizeof names);
    page = lj_ini_geti(ini, "SETUP", "Page", 1); bank = lj_ini_geti(ini, "SETUP", "Bank", 1);
    if (lj_ini_geti(ini, "SETUP", "Version", 1) != 1 || !in(page, 1, LJ_DA_PAGES) || !in(bank, 1, LJ_DA_BANKS)) bad = 1;
    for (p = 0; p < LJ_DA_PAGES && !bad; p++) {
        snprintf(sec, sizeof sec, "PAGE%d", p + 1);
        snprintf(names[p], LJ_DA_TEXT, "%s", lj_ini_gets(ini, sec, "Name", ""));
        for (b = 0; b < LJ_DA_BANKS && !bad; b++)
            for (k = 0; k < LJ_DA_KEYS && !bad; k++) {
                snprintf(key, sizeof key, "%c%dL", 'A' + b, k + 1);
                if (parse_val(lj_ini_gets(ini, sec, key, ""), 0, &lj_da_tmp_l[p][b][k]) != 0) bad = 1;
                else get_text(ini, sec, key, lj_da_tmp_l[p][b][k].text);
                snprintf(key, sizeof key, "%c%dF", 'A' + b, k + 1);
                if (!bad && parse_val(lj_ini_gets(ini, sec, key, ""), 1, &lj_da_tmp_f[p][b][k]) != 0) bad = 1;
                else if (!bad) get_text(ini, sec, key, lj_da_tmp_f[p][b][k].text);
            }
    }
    for (k = 0; k < LJ_DA_USER && !bad; k++) {
        snprintf(key, sizeof key, "U%d", k + 1);
        if (parse_val(lj_ini_gets(ini, "USER", key, ""), -1, &user[k]) != 0) bad = 1; else get_text(ini, "USER", key, user[k].text);
    }
    lj_ini_free(ini);
    if (bad) { lj_log("2532: %s refused (%s %s)", path, sec, key); return -1; }   /* untrusted file: all or nothing */
    memcpy(da.latch, lj_da_tmp_l, sizeof da.latch); memcpy(da.flash, lj_da_tmp_f, sizeof da.flash);
    memcpy(da.user, user, sizeof da.user); memcpy(da.page_name, names, sizeof da.page_name);
    da.page = page - 1; da.bank = bank - 1;
    return 0;
}
static void ensure_loaded(void) { if (!da.loaded) lj_da2532_load(); }

/* ---- running a key ---- */
static void note_last(const lj_da_val *v) { lj_da2532_label(v, da.last, sizeof da.last); }
static int run(const lj_da_val *v, int down)
{
    switch (v->type) {
    case LJ_DA_FN:  if (down) note_last(v); lj_fn_run((enum lj_fn)v->num, down); return 0;   /* the release reaches flash functions only (lj_fn_run) */
    case LJ_DA_SEQ: lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, v->num, down); return 0;            /* flashed while held, released on key-up [05 s4.2] */
    case LJ_DA_CUE: if (down) lj_fn_run_prog(LJ_FN_PROG_CUE, v->num, 1); return 0;
    case LJ_DA_CL:  if (down) lj_fn_run_prog(LJ_FN_PROG_CUELIST, v->num, 1); return 0;
    case LJ_DA_BG:  if (down) lj_fn_run_prog(LJ_FN_PROG_BGCUE, v->num, 1); return 0;
    default: return -1;
    }
}
static uint32_t current_of(int type)
{
    lj_ctx *c = lj_ctx_get();
    switch (type) {
    case LJ_DA_CUE: return c->view.cue_number;
    case LJ_DA_SEQ: return c->show && c->show->current_seq ? c->show->current_seq->number : 0;
    case LJ_DA_CL:  return c->cl ? c->cl->number : 0;
    case LJ_DA_BG:  return c->view.bg_number;
    default: return 0;
    }
}
static void refresh_main(void);
static int press(int bank, int key, int flash, int down)   /* bank / key 0-based */
{
    lj_da_val *v = flash ? &da.flash[da.page][bank][key] : &da.latch[da.page][bank][key], *held = flash ? &da.held_flash[key] : &da.held_latch[key];
    int p;
    if (!down) { lj_da_val h = *held; memset(held, 0, sizeof *held); return h.type ? run(&h, 0) : -1; }
    if (da.record && (da.record == LJ_DA_SEQ) == (flash != 0)) {     /* "the next key press on the 2532 will 'record' the active program" (05 s4.3) */
        uint32_t n = current_of(da.record);
        if (!n) { lj_log("2532: nothing to record (no current %s)", lj_da_words[da.record]); return -1; }
        lj_da2532_assign(da.page + 1, bank + 1, key + 1, flash, da.record, n);
        if (pref(P_AUTORELEASE)) da.record = LJ_DA_NONE;
        lj_da2532_save();
        refresh_main();
        return 0;
    }
    if (!flash) da.bank = bank;                                       /* a latch press makes the bank active, a flash press does not (05 s4.2) */
    for (p = 0; v->type == LJ_DA_NONE && pref(P_TRANSPARENT) && p < LJ_DA_PAGES; p++)   /* Transparent Pages: the same bank / key on the other pages (05 s4.3) */
        v = flash ? &da.flash[p][bank][key] : &da.latch[p][bank][key];
    if (v->type == LJ_DA_NONE) return -1;
    *held = *v;
    run(v, 1);
    refresh_main();
    return 0;
}
int lj_da2532_latch(int key, int down) { ensure_loaded(); return in(key, 1, LJ_DA_KEYS) ? press(da.bank, key - 1, 0, down) : -1; }
int lj_da2532_flash(int key, int down) { ensure_loaded(); return in(key, 1, LJ_DA_KEYS) ? press(da.bank, key - 1, 1, down) : -1; }
int lj_da2532_user(int key, int down)
{
    lj_da_val *held;
    ensure_loaded();
    if (!in(key, 1, LJ_DA_USER)) return -1;
    held = &da.held_user[key - 1];
    if (!down) { lj_da_val h = *held; memset(held, 0, sizeof *held); return h.type ? run(&h, 0) : -1; }
    if (da.user[key - 1].type == LJ_DA_NONE) return -1;
    *held = da.user[key - 1];
    return run(held, 1);
}
int lj_da2532_set_page(int page) { ensure_loaded(); if (!in(page, 1, LJ_DA_PAGES)) return -1; da.page = page - 1; refresh_main(); return 0; }
int lj_da2532_page(void) { return da.page + 1; }
int lj_da2532_set_bank(int bank) { ensure_loaded(); if (!in(bank, 1, LJ_DA_BANKS)) return -1; da.bank = bank - 1; refresh_main(); return 0; }
int lj_da2532_bank(void) { return da.bank + 1; }
int lj_da2532_set_record(int type) { if (type != LJ_DA_NONE && type != LJ_DA_CUE && type != LJ_DA_SEQ && type != LJ_DA_CL && type != LJ_DA_BG) return -1; da.record = type; refresh_main(); return 0; }

/* ---- MIDI Learn for the EXT rows (Alt+click on a key, tab, bank letter or user key) ---- */
static enum lj_fn lj_da_learn_fn;
int lj_da2532_learn(enum lj_fn fn)
{
    if ((int)fn <= 0 || fn >= LJ_FN_COUNT) return -1;
    lj_da_learn_fn = fn;
    lj_midi_learn_begin_ext(0);
    lj_log("2532: MIDI Learn armed for %s", lj_fn_table[fn].name);
    return 0;
}
static void learn_poll(void)
{
    int ch, note, cc;
    if (lj_da_learn_fn == LJ_FN_NONE || !lj_midi_learn_take_ext(&ch, &note, &cc)) return;
    if (lj_midi_learn_bind(lj_ctx_get()->midi, lj_da_learn_fn, ch, note, cc) == 0) {
        lj_log("2532: %s learnt (channel %d, %s %d)", lj_fn_table[lj_da_learn_fn].name, ch, cc ? "CC" : "note", note);
        lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0);
    }
    lj_da_learn_fn = LJ_FN_NONE;
}

/* ================= the window TDa2532Form_3 ================= */
static struct {
    lj_window *win;
    lj_widget *grid, *ugrid, *tbc;
    lj_widget_vt grid_vt, ugrid_vt, led_vt;
    int (*grid_mouse0)(lj_widget *, const lj_mouse_ev *), (*ugrid_mouse0)(lj_widget *, const lj_mouse_ev *);
    int col_w[1 + LJ_DA_KEYS];
    char tabtxt[LJ_DA_PAGES][LJ_DA_TEXT]; const char *tabs[LJ_DA_PAGES];
    int sel_bank, sel_key, sel_user;                  /* 0-based selection (the popup acts on it) */
    int mouse_flash;                                  /* sbFlashToggle: 0 Latch, 1 Flash */
    int down_flash, down_bank, down_key, down_user;   /* the key a mouse press fired: -1 none */
    lj_da_val clip_l, clip_f; int has_clip;
} ui;
static lj_window *lj_da_sub[LJ_WIN_DA2532_PAGE_NAMES - LJ_WIN_DA2532 + 1];   /* index id - LJ_WIN_DA2532; [0] = ui.win */

static int led_in(const lj_widget *w, int x, int y)   /* pixel centre inside the ellipse inscribed in the widget (integer test) */
{
    long dx = 2L * x + 1 - w->w, dy = 2L * y + 1 - w->h, a = w->w, b = w->h;
    return x >= 0 && y >= 0 && x < w->w && y < w->h && dx * dx * b * b + dy * dy * a * a <= a * a * b * b;
}
static void led_paint(lj_widget *w, lj_surface *s)   /* the three TShape stCircle LEDs with a runtime brush (w->color), black pen */
{
    int ax, ay, x, y;
    lj_widget_abs(w, &ax, &ay);
    for (y = 0; y < w->h; y++)
        for (x = 0; x < w->w; x++)
            if (led_in(w, x, y))
                lj_fill(s, ax + x, ay + y, 1, 1, led_in(w, x - 1, y) && led_in(w, x + 1, y) && led_in(w, x, y - 1) && led_in(w, x, y + 1) ? w->color : 0x000000u);
}

static void set_label(const char *name, const char *text) { if (ui.win) lj_window_set_text(ui.win, name, text); }
static void refresh_main(void)
{
    lj_ctx *c = lj_ctx_get();
    char t[48];
    lj_widget *w;
    if (!ui.win) return;
    snprintf(t, sizeof t, "%d", da.page + 1); set_label("lPage", t);
    snprintf(t, sizeof t, "%u", (unsigned)c->view.cue_number); set_label("lCue", c->view.cue_number ? t : "");
    snprintf(t, sizeof t, "%u", (unsigned)c->view.flash_seq); set_label("lSeq", c->view.flash_seq ? t : "");
    snprintf(t, sizeof t, "%u", (unsigned)(c->cl ? c->cl->number : 0)); set_label("lCuelist", c->cl && c->cl->number ? t : "");
    snprintf(t, sizeof t, "%u", (unsigned)c->view.bg_number); set_label("lBGCue", c->view.bg_number ? t : "");
    set_label("lFunction", da.last);
    if ((w = lj_window_find(ui.win, "sOnline")) != NULL) { w->color = c->view.offline ? LJ_CLR_RED : LJ_CLR_GREEN; lj_widget_invalidate(w); }   /* PROPOSAL rig 05 s4: green = DMX online */
    if ((w = lj_window_find(ui.win, "sBo")) != NULL) { w->color = c->view.blackout ? LJ_CLR_RED : 0x800000u; lj_widget_invalidate(w); }
    if ((w = lj_window_find(ui.win, "sRecord")) != NULL) { w->color = da.record ? LJ_CLR_RED : 0x800000u; lj_widget_invalidate(w); }
    {
        static const char *const pn[4] = { "pCue", "pSeq", "pCl", "pBg" };
        static const int pt[4] = { LJ_DA_CUE, LJ_DA_SEQ, LJ_DA_CL, LJ_DA_BG };
        int i;
        for (i = 0; i < 4; i++) if ((w = lj_window_find(ui.win, pn[i])) != NULL) { w->color = da.record == pt[i] ? LJ_CLR_RED : LJ_CLR_BTNFACE; lj_widget_invalidate(w); }
    }
    if (ui.tbc && ((lj_w_tabs *)ui.tbc)->active != da.page) { ((lj_w_tabs *)ui.tbc)->active = da.page; lj_widget_invalidate(ui.tbc); }
    if (ui.grid) lj_widget_invalidate(ui.grid);
    if (ui.ugrid) lj_widget_invalidate(ui.ugrid);
    {
        static char sb0[48], sb1[80];                                     /* the status bar keeps the pointers */
        const lj_da_val *l = &da.latch[da.page][ui.sel_bank][ui.sel_key], *f = &da.flash[da.page][ui.sel_bank][ui.sel_key];
        char a[32], b[32];
        lj_widget *sp = lj_window_find(ui.win, "Sp1");
        snprintf(sb0, sizeof sb0, "Page %d  Bank %c  Key %d", da.page + 1, 'A' + ui.sel_bank, ui.sel_key + 1);   /* PROPOSAL rig 05 s4 (Matrix button info) */
        snprintf(sb1, sizeof sb1, "Latch: %s  Flash: %s", lj_da2532_label(l, a, sizeof a), lj_da2532_label(f, b, sizeof b));
        if (sp) { lj_w_statusbar_set_text(sp, 0, sb0); lj_w_statusbar_set_text(sp, 1, sb1); }
    }
}

static void fill_tabs(void)
{
    int i;
    for (i = 0; i < LJ_DA_PAGES; i++) {
        if (da.page_name[i][0]) snprintf(ui.tabtxt[i], sizeof ui.tabtxt[i], "%s", da.page_name[i]);
        else snprintf(ui.tabtxt[i], sizeof ui.tabtxt[i], "P%d", i + 1);   /* the DFM Tabs.Strings */
        ui.tabs[i] = ui.tabtxt[i];
    }
    if (!ui.tbc) return;
    ((lj_w_tabs *)ui.tbc)->tabs = ui.tabs; ((lj_w_tabs *)ui.tbc)->ntabs = LJ_DA_PAGES; ((lj_w_tabs *)ui.tbc)->active = da.page;
    lj_w_tabs_layout(ui.tbc);
    lj_widget_invalidate(ui.tbc);
}

static void size_grid(void)   /* Auto Resize Matrix: the 8 key columns share the grid width; the 8 rows its height (PROPOSAL rig 05 s4) */
{
    lj_w_grid *g = (lj_w_grid *)ui.grid;
    int i, cw, rh;
    if (!g) return;
    cw = (ui.grid->w - 4 - LJ_DA_FIX_W - LJ_DA_KEYS - 1) / LJ_DA_KEYS;
    rh = (ui.grid->h - 4 - 8) / 8;
    if (!pref(P_AUTORESIZE)) { cw = 64; rh = 24; }                     /* the DFM DefaultColWidth / RowHeights */
    ui.col_w[0] = LJ_DA_FIX_W;
    for (i = 1; i <= LJ_DA_KEYS; i++) ui.col_w[i] = cw < 20 ? 20 : cw;
    g->col_w = ui.col_w; g->def_rh = rh < 13 ? 13 : rh;
    if (ui.ugrid) { lj_w_grid *u = (lj_w_grid *)ui.ugrid; int h = (ui.ugrid->h - 4 - 9) / 9; u->def_rh = h < 13 ? 13 : h; }
    lj_widget_invalidate(ui.grid);
}

static void apply_view(void)   /* Show Large Control / Hide Status Panel / Show User Keys */
{
    lj_widget *ps = lj_window_find(ui.win, "pStatus"), *p4 = lj_window_find(ui.win, "Panel4"), *pu = lj_window_find(ui.win, "pUserKeys");
    int large = pref(P_SHOWLARGE);
    if (ps) ps->visible = !(large && pref(P_HIDESTATUS));
    if (p4) p4->visible = large != 0;
    if (pu) pu->visible = pref(P_SHOWUSER);
    if (!large) lj_window_resize(ui.win, lj_window_root(ui.win)->w, 62);   /* the small control: the status panel only (a2532small.gif) */
    lj_widget_layout(lj_window_root(ui.win));
    size_grid();
}

/* the cells */
static void draw_val(lj_surface *s, const lj_font *f, int x, int y, int w, int h, const lj_da_val *v)
{
    char t[48];
    if (v->type == LJ_DA_NONE) return;
    lj_fill(s, x, y, w, h, back_rgb(v->type));
    lj_da2532_label(v, t, sizeof t);
    lj_text(s, f, x + 2, y + 1, t, text_rgb(v->type), 0);
}
static void on_grid_draw(lj_widget *w, void *ud)
{
    lj_w_grid *g = (lj_w_grid *)w;
    const lj_font *f = lj_w_font_of(w);
    lj_surface *s = g->draw_surf;
    int c = g->draw_col, r = g->draw_row, x = g->draw_rect.x, y = g->draw_rect.y, cw = g->draw_rect.w, ch = g->draw_rect.h;
    char t[12];
    (void)ud;
    if (!s) return;
    if (r == 0 && c > 0) { snprintf(t, sizeof t, "%d", c); lj_text(s, f, x + (cw - lj_text_width(f, t)) / 2, y + (ch - f->height) / 2, t, 0, 0); return; }
    if (c == 0 && r > 0) { t[0] = (char)('A' + r - 1); t[1] = 0; lj_text(s, f, x + (cw - lj_text_width(f, t)) / 2, y + (ch - f->height) / 2, t, r - 1 == da.bank ? LJ_CLR_RED : 0, 0); return; }   /* the active bank (its LED on the 2532): red letter, PROPOSAL rig 05 s4 */
    if (r == 0 || c == 0) return;
    {
        const lj_da_val *l = &da.latch[da.page][r - 1][c - 1], *fl = &da.flash[da.page][r - 1][c - 1];
        int sel = r - 1 == ui.sel_bank && c - 1 == ui.sel_key;
        lj_fill(s, x, y, cw, ch, sel ? LJ_DA_SEL_RGB : 0xFFFFFFu);
        if (l->type && fl->type) { draw_val(s, f, x, y, cw, ch / 2, l); draw_val(s, f, x, y + ch / 2, cw, ch - ch / 2, fl); }   /* split: latch above, flash below */
        else if (l->type) draw_val(s, f, x, y, cw, pref(P_ALWAYSSPLIT) ? ch / 2 : ch, l);
        else if (fl->type) draw_val(s, f, x, pref(P_ALWAYSSPLIT) ? y + ch / 2 : y, cw, pref(P_ALWAYSSPLIT) ? ch - ch / 2 : ch, fl);
        if (sel) lj_bevel1(s, x, y, cw, ch, LJ_DA_SEL_RGB, LJ_DA_SEL_RGB);
    }
}
static void on_ugrid_draw(lj_widget *w, void *ud)
{
    lj_w_grid *g = (lj_w_grid *)w;
    lj_surface *s = g->draw_surf;
    int k = g->draw_row * 2 + g->draw_col;                              /* user keys row by row, two per row (PROPOSAL rig 05 s4, 2532userkeys.gif) */
    (void)ud;
    if (!s || k >= LJ_DA_USER) return;
    lj_fill(s, g->draw_rect.x, g->draw_rect.y, g->draw_rect.w, g->draw_rect.h, k == ui.sel_user ? LJ_DA_SEL_RGB : 0xFFFFFFu);
    draw_val(s, lj_w_font_of(w), g->draw_rect.x, g->draw_rect.y, g->draw_rect.w, g->draw_rect.h, &da.user[k]);
}

/* the grid mouse: our TDrawGrid emits OnDrawCell / OnSelectCell / OnClick only, so the mouse entry decides (as generic DMX) */
static int grid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int col, row, flash;
    if (m->type == LJ_EV_MOUSE_UP && ui.down_key >= 0) {                 /* release what the press fired */
        if (ui.down_flash ? pref(P_RELEASEFLASH) : 1) press(ui.down_bank, ui.down_key, ui.down_flash, 0);
        ui.down_key = -1;
    }
    if (m->type == LJ_EV_MOUSE_DOWN && lj_w_grid_cell_at(w, m->x, m->y, &col, &row) == 0) {
        if (m->alt) {                                                    /* Learn: a key = its column's latch (flash with Ctrl) row, a bank letter = the bank row */
            if (row >= 1 && col == 0) lj_da2532_learn((enum lj_fn)(LJ_FN_DA2532_BANK_1 + row - 1));
            else if (row >= 1 && col >= 1) lj_da2532_learn((enum lj_fn)((m->ctrl ? LJ_FN_DA2532_FLASH_1 : LJ_FN_DA2532_LATCH_1) + col - 1));
            return 1;
        }
        if (row >= 1 && col >= 1) {
            ui.sel_bank = row - 1; ui.sel_key = col - 1;
            flash = m->button == 3 || (m->button == 1 && (m->ctrl || ui.mouse_flash));   /* Ctrl+click or the middle button = flash (05 s4.4) */
            if ((m->button == 1 || m->button == 3) && pref(P_MOUSESELECT)) {
                if (flash && !pref(P_RELEASEFLASH) && da.held_flash[col - 1].type) press(row - 1, col - 1, 1, 0);   /* no Release on Mouse-Up: a second click releases */
                else if (press(row - 1, col - 1, flash, 1) == 0) { ui.down_flash = flash; ui.down_bank = row - 1; ui.down_key = col - 1; }
            }                                                            /* else select only: Emulate with Mouse is off by default (DFM) */
            refresh_main();
        }
    }
    return ui.grid_mouse0 ? ui.grid_mouse0(w, m) : 1;
}
static int ugrid_mouse(lj_widget *w, const lj_mouse_ev *m)
{
    int col, row, k;
    if (m->type == LJ_EV_MOUSE_UP && ui.down_user >= 0) { lj_da2532_user(ui.down_user + 1, 0); ui.down_user = -1; }
    if (m->type == LJ_EV_MOUSE_DOWN && lj_w_grid_cell_at(w, m->x, m->y, &col, &row) == 0 && (k = row * 2 + col) < LJ_DA_USER) {
        ui.sel_user = k;
        if (m->alt) { lj_da2532_learn((enum lj_fn)(LJ_FN_DA2532_USER_1 + k)); return 1; }
        if (m->button == 1 && pref(P_MOUSESELECT) && lj_da2532_user(k + 1, 1) == 0) ui.down_user = k;
        refresh_main();
    }
    return ui.ugrid_mouse0 ? ui.ugrid_mouse0(w, m) : 1;
}
static int tbc_mouse0_alt(lj_widget *w, const lj_mouse_ev *m);
static int (*lj_da_tbc_mouse0)(lj_widget *, const lj_mouse_ev *);
static lj_widget_vt lj_da_tbc_vt;
static int tbc_mouse0_alt(lj_widget *w, const lj_mouse_ev *m)   /* Alt+click on a page tab arms Learn for that page row */
{
    int t;
    if (m->type == LJ_EV_MOUSE_DOWN && m->alt && (t = lj_w_tabs_at(w, m->x, m->y)) >= 0) { lj_da2532_learn((enum lj_fn)(LJ_FN_DA2532_PAGE_1 + t)); return 1; }
    return lj_da_tbc_mouse0 ? lj_da_tbc_mouse0(w, m) : 1;
}

/* drops: sequences / cues / cue lists / BG cues from their lists, functions from the 2532 Function List */
static void on_grid_drop(lj_widget *w, void *ud)
{
    int kind, number, x, y, col, row, flash;
    static const int type_of[LJ_REC_KIND_COUNT] = { LJ_DA_SEQ, LJ_DA_CUE, LJ_DA_CL, LJ_DA_BG, LJ_DA_NONE };
    (void)ud;
    if (!ui.win || lj_dnd_info(ui.win, &kind, &number, &x, &y) != 0 || number <= 0 || lj_w_grid_cell_at(w, x, y, &col, &row) != 0 || row < 1 || col < 1) return;
    if (kind == LJ_DA_DND_FN) flash = ui.mouse_flash && lj_fn_is_flash((enum lj_fn)number);
    else if (kind >= 0 && kind < LJ_REC_KIND_COUNT && type_of[kind]) flash = type_of[kind] == LJ_DA_SEQ;
    else return;
    if (lj_da2532_assign(da.page + 1, row, col, flash, kind == LJ_DA_DND_FN ? LJ_DA_FN : type_of[kind], (uint32_t)number) == 0) { lj_da2532_save(); refresh_main(); }
}
static void on_ugrid_drop(lj_widget *w, void *ud)
{
    int kind, number, x, y, col, row;
    (void)ud;
    if (!ui.win || lj_dnd_info(ui.win, &kind, &number, &x, &y) != 0 || kind != LJ_DA_DND_FN || lj_w_grid_cell_at(w, x, y, &col, &row) != 0) return;
    if (lj_da2532_assign_user(row * 2 + col + 1, (uint32_t)number) == 0) { lj_da2532_save(); refresh_main(); }
}

/* the menus */
static const char *item_name(void) { const lj_menu_item_desc *it = ui.win ? lj_window_menu_item(ui.win) : NULL; return it && it->name ? it->name : ""; }
static void sync_menu(void)
{
    lj_menu *m = ui.win ? lj_window_menus(ui.win) : NULL;
    int i;
    if (!m) return;
    for (i = 0; i < P_COUNT; i++) lj_menu_set_checked(m, lj_da_pref[i].item, pref(i));
    lj_menu_set_checked(m, "M_ClickAutoLatch", lj_ini_geti(lj_ctx_get()->main_ini, LJ_DA_SEC, "M_ClickAutoFlash", 0) == 0);
    lj_menu_set_checked(m, "M_ClickAutoFlash", lj_ini_geti(lj_ctx_get()->main_ini, LJ_DA_SEC, "M_ClickAutoFlash", 0) != 0);
    lj_menu_set_enabled(m, "PrintLayout1", 0);                         /* TPrint2532Form: no printing (fidelity row) */
    lj_menu_set_enabled(m, "M_ImportOld", 0);                          /* 2532.cfg / .D25: binary, undocumented, never imported */
    lj_menu_set_enabled(m, "M_SelectFont", 0);                         /* no TFontDialog: the matrix keeps MS Sans Serif 8 */
}
static void on_prefs_toggle(lj_widget *w, void *ud)
{
    const char *n = item_name();
    int i;
    (void)w; (void)ud;
    for (i = 0; i < P_COUNT; i++)
        if (!strcmp(n, lj_da_pref[i].item)) { lj_ini_seti(lj_ctx_get()->main_ini, LJ_DA_SEC, n, !pref(i)); break; }
    sync_menu();
    if (ui.win) apply_view();
    refresh_main();
}
static void on_click_auto(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ini_seti(lj_ctx_get()->main_ini, LJ_DA_SEC, "M_ClickAutoFlash", !strcmp(item_name(), "M_ClickAutoFlash")); sync_menu(); }
static void on_tab_color_enable(lj_widget *w, void *ud) { (void)w; (void)ud; lj_ini_seti(lj_ctx_get()->main_ini, LJ_DA_SEC, "M_TabColorEnable", !pref(P_TABCOLORS)); sync_menu(); if (ui.tbc) lj_widget_invalidate(ui.tbc); }
static void new_answer(int yes, void *ud) { (void)ud; if (yes) { lj_da2532_clear_all(); lj_da2532_save(); fill_tabs(); refresh_main(); } }
static void on_new(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Confirm", "Clear Current 2532 Setup ?", "", new_answer, NULL); }   /* exe prompt 0x1E1BC4 verbatim; the box caption PROPOSAL */
static void on_load(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_da2532_load() != 0) lj_win_message("Error", "2532 setup file refused (see the log)"); fill_tabs(); refresh_main(); }   /* one setup per library: no file dialog (fidelity row) */
static void on_save(lj_widget *w, void *ud) { (void)w; (void)ud; lj_da2532_save(); }
static void clear_answer(int yes, void *ud) { (void)ud; if (yes) { memset(da.latch, 0, sizeof da.latch); memset(da.flash, 0, sizeof da.flash); lj_da2532_save(); refresh_main(); } }
static void on_clear_all(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_confirm("Confirm", "Clear all Pages ?", "", clear_answer, NULL); }   /* PROPOSAL: no exe string for this prompt */
static void on_na(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("2532: %s - " LJ_DA_NA_HINT, item_name()); }
static void on_font(lj_widget *w, void *ud) { (void)w; (void)ud; if (!strcmp(item_name(), "M_ColorScheme")) lj_win_open(LJ_WIN_DA2532_COLORS); else on_na(w, ud); }
static void on_name_tabs(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DA2532_PAGE_NAMES); }
static void on_tab_size(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DA2532_TAB_HEIGHT); }
static void on_tab_colors(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DA2532_TAB_COLORS); }
static void on_functions(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_DA2532_FUNCTIONS); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }
static void on_tbc_change(lj_widget *w, void *ud) { (void)ud; lj_da2532_set_page(((lj_w_tabs *)w)->active + 1); }
static void on_record(lj_widget *w, void *ud)
{
    int t = !strcmp(w->name, "pCue") ? LJ_DA_CUE : !strcmp(w->name, "pSeq") ? LJ_DA_SEQ : !strcmp(w->name, "pCl") ? LJ_DA_CL : LJ_DA_BG;
    (void)ud;
    lj_da2532_set_record(da.record == t ? LJ_DA_NONE : t);             /* mutually exclusive, a second click disarms (05 s4.3) */
}
static void on_flash_toggle(lj_widget *w, void *ud)
{
    (void)ud;
    ui.mouse_flash = !ui.mouse_flash;
    lj_w_set_caption(w, ui.mouse_flash ? "Flash" : "Latch");          /* the button reads the mouse-click function (Hint 'Mouse Click Function') */
}

/* the matrix popup (PopGrid) and the user-key popup (PopUser) act on the selected key */
static lj_da_val *sel_val(int flash) { return flash ? &da.flash[da.page][ui.sel_bank][ui.sel_key] : &da.latch[da.page][ui.sel_bank][ui.sel_key]; }
static struct { lj_da_val *target; } lj_da_text;
int lj_da2532_select(int bank, int key) { if (!in(bank, 1, LJ_DA_BANKS) || !in(key, 1, LJ_DA_KEYS)) return -1; ui.sel_bank = bank - 1; ui.sel_key = key - 1; refresh_main(); return 0; }
int lj_da2532_edit_text(int user, int flash)
{
    lj_da_text.target = user ? &da.user[ui.sel_user] : sel_val(flash);
    if (!lj_da_text.target->type) return -1;
    return lj_win_open(LJ_WIN_DA2532_TEXT) ? 0 : -1;
}
static void on_grid_pop(lj_widget *w, void *ud)
{
    const char *n = item_name();
    (void)w; (void)ud;
    if (!strcmp(n, "Pop_GridClearLatch") || !strcmp(n, "Pop_ClearBoth")) memset(sel_val(0), 0, sizeof(lj_da_val));
    if (!strcmp(n, "Pop_GridClearFlash") || !strcmp(n, "Pop_ClearBoth")) memset(sel_val(1), 0, sizeof(lj_da_val));
    if (!strcmp(n, "Pop_GridClearLatchText") || !strcmp(n, "Pop_GridClearBothText")) sel_val(0)->text[0] = 0;
    if (!strcmp(n, "Pop_GridClearFlashText") || !strcmp(n, "Pop_GridClearBothText")) sel_val(1)->text[0] = 0;
    if (!strcmp(n, "Pop_EditLatchText") || !strcmp(n, "Pop_EditFlashText")) { lj_da2532_edit_text(0, !strcmp(n, "Pop_EditFlashText")); return; }
    if (!strcmp(n, "P_GridCut") || !strcmp(n, "P_GridCopy")) { ui.clip_l = *sel_val(0); ui.clip_f = *sel_val(1); ui.has_clip = 1; }
    if (!strcmp(n, "P_GridCut")) { memset(sel_val(0), 0, sizeof(lj_da_val)); memset(sel_val(1), 0, sizeof(lj_da_val)); }
    lj_da2532_save();
    refresh_main();
}
static void paste(int l, int f) { if (!ui.has_clip) return; if (l) *sel_val(0) = ui.clip_l; if (f) *sel_val(1) = ui.clip_f; lj_da2532_save(); refresh_main(); }
static void on_paste(lj_widget *w, void *ud) { (void)w; (void)ud; paste(1, 1); }
static void on_paste_latch(lj_widget *w, void *ud) { (void)w; (void)ud; paste(1, 0); }
static void on_paste_flash(lj_widget *w, void *ud) { (void)w; (void)ud; paste(0, 1); }
static void on_set_current(lj_widget *w, void *ud)
{
    const char *n = item_name();
    int t = !strcmp(n, "M_SetCurrentCue") ? LJ_DA_CUE : !strcmp(n, "M_SetCurrentCueList") ? LJ_DA_CL : !strcmp(n, "M_SetCurrentBGCue") ? LJ_DA_BG : LJ_DA_SEQ;
    uint32_t num = current_of(t);
    (void)w; (void)ud;
    if (!num) { lj_log("2532: no current %s", lj_da_words[t]); return; }
    lj_da2532_assign(da.page + 1, ui.sel_bank + 1, ui.sel_key + 1, t == LJ_DA_SEQ, t, num);
    lj_da2532_save();
    refresh_main();
}
static void on_grid_popup(lj_widget *w, void *ud)
{
    lj_menu *m = ui.win ? lj_window_menus(ui.win) : NULL;
    (void)w; (void)ud;
    if (!m) return;
    lj_menu_set_enabled(m, "P_GridPaste", ui.has_clip); lj_menu_set_enabled(m, "p_PasteLatch", ui.has_clip); lj_menu_set_enabled(m, "p_PasteFlash", ui.has_clip);
    lj_menu_set_enabled(m, "Pop_EditLatchText", sel_val(0)->type != LJ_DA_NONE); lj_menu_set_enabled(m, "Pop_EditFlashText", sel_val(1)->type != LJ_DA_NONE);
}
static void on_user_pop(lj_widget *w, void *ud)
{
    const char *n = item_name();
    lj_da_val *v = &da.user[ui.sel_user];
    (void)w; (void)ud;
    if (!strcmp(n, "Pop_ClearUser")) memset(v, 0, sizeof *v);
    else if (!strcmp(n, "Pop_UserCustomTextClear")) v->text[0] = 0;
    else if (!strcmp(n, "Pop_UserGridEdit")) { lj_da2532_edit_text(1, 0); return; }
    lj_da2532_save();
    refresh_main();
}
static void on_main_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_da2532_close(LJ_WIN_DA2532); }
static void on_resize(lj_widget *w, void *ud) { (void)w; (void)ud; size_grid(); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_da2532_refresh(LJ_WIN_DA2532); }
static void on_prefs_menu(lj_widget *w, void *ud) { (void)w; (void)ud; sync_menu(); }

static const lj_binding lj_da_bindings[] = {
    { "FontClick", on_font, NULL }, { "FormActivate", lj_handler_noop, NULL }, { "FormClose", on_main_close, NULL }, { "FormCreate", lj_handler_noop, NULL },
    { "FormDestroy", lj_handler_noop, NULL }, { "FormResize", on_resize, NULL }, { "FormShow", lj_handler_noop, NULL }, { "FunctionsList1Click", on_functions, NULL },
    { "GridClick", lj_handler_noop, NULL } /* grid_mouse */, { "GridDragDrop", on_grid_drop, NULL }, { "GridDragOver", lj_handler_noop, NULL },
    { "GridDrawCell", on_grid_draw, NULL }, { "GridMouseDown", lj_handler_noop, NULL }, { "GridMouseMove", lj_handler_noop, NULL }, { "GridMouseUp", lj_handler_noop, NULL },
    { "GridSelectCell", lj_handler_noop, NULL }, { "Help1Click", on_help, NULL }, { "Load2532Setup1Click", on_load, NULL },
    { "M_ClearAllPagesClick", on_clear_all, NULL }, { "M_ClickAutoFlashClick", on_click_auto, NULL }, { "M_ImportOldClick", on_na, NULL },
    { "M_NamePageTabsClick", on_name_tabs, NULL }, { "M_PrefsToggle", on_prefs_toggle, NULL }, { "M_SetTabSizeClick", on_tab_size, NULL },
    { "M_TabColorEnableClick", on_tab_color_enable, NULL }, { "New2532Setup1Click", on_new, NULL }, { "P_GridPasteClick", on_paste, NULL },
    { "p_PasteFlashClick", on_paste_flash, NULL }, { "p_PasteLatchClick", on_paste_latch, NULL }, { "pCueDblClick", lj_handler_noop, NULL },
    { "pnRecordClick", on_record, NULL }, { "Pop_GridClick", on_grid_pop, NULL }, { "Pop_UserGridClick", on_user_pop, NULL },
    { "PopGridPopup", on_grid_popup, NULL }, { "PopSetCurrent", on_set_current, NULL }, { "PopUserPopup", lj_handler_noop, NULL },
    { "Preferences1Click", on_prefs_menu, NULL }, { "PrintLayout1Click", on_na, NULL }, { "Save2532Setup1Click", on_save, NULL },
    { "sbFlashToggleMouseUp", on_flash_toggle, NULL }, { "Set1Click", on_tab_colors, NULL }, { "tbcChange", on_tbc_change, NULL },
    { "tbcDrawTab", lj_handler_noop, NULL }, { "Timer1Timer", on_timer, NULL }, { "UseColoredTabs1Click", lj_handler_noop, NULL },
    { "UserGridDragDrop", on_ugrid_drop, NULL }, { "UserGridDragOver", lj_handler_noop, NULL }, { "UserGridDrawCell", on_ugrid_draw, NULL },
    { "UserGridMouseDown", lj_handler_noop, NULL }, { "UserGridMouseUp", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

static lj_window *open_main(void)
{
    static const char *const leds[3] = { "sOnline", "sBo", "sRecord" };
    lj_widget *w;
    int i;
    if (ui.win) return ui.win;
    ensure_loaded();
    memset(&ui, 0, sizeof ui);
    ui.down_key = ui.down_user = -1;
    if ((ui.win = lj_window_create(&lj_form_TDA2532FORM_3, lj_da_bindings)) == NULL) { lj_log("2532: cannot create TDa2532Form_3"); return NULL; }
    ui.grid = lj_window_find(ui.win, "Grid"); ui.ugrid = lj_window_find(ui.win, "UserGrid"); ui.tbc = lj_window_find(ui.win, "tbc");
    if (ui.grid) { ui.grid_vt = *ui.grid->vt; ui.grid_mouse0 = ui.grid_vt.mouse; ui.grid_vt.mouse = grid_mouse; ui.grid->vt = &ui.grid_vt; }
    if (ui.ugrid) { ui.ugrid_vt = *ui.ugrid->vt; ui.ugrid_mouse0 = ui.ugrid_vt.mouse; ui.ugrid_vt.mouse = ugrid_mouse; ui.ugrid->vt = &ui.ugrid_vt; }
    if (ui.tbc) { lj_da_tbc_vt = *ui.tbc->vt; lj_da_tbc_mouse0 = lj_da_tbc_vt.mouse; lj_da_tbc_vt.mouse = tbc_mouse0_alt; ui.tbc->vt = &lj_da_tbc_vt; }
    for (i = 0; i < 3; i++) if ((w = lj_window_find(ui.win, leds[i])) != NULL) { if (!ui.led_vt.paint) { ui.led_vt = *w->vt; ui.led_vt.paint = led_paint; } w->vt = &ui.led_vt; }
    fill_tabs();
    sync_menu();
    apply_view();
    refresh_main();
    lj_win_show(ui.win, LJ_DA_STAY_ON_TOP);
    return ui.win;
}

/* ================= the sub-dialogs ================= */
static lj_window **sub(enum lj_win id) { return &lj_da_sub[id - LJ_WIN_DA2532]; }
static void close_sub(enum lj_win id) { lj_window **w = sub(id); if (!*w) return; lj_win_hide(*w); lj_window_destroy(*w); *w = NULL; }
static void close_from(lj_widget *w)   /* the sub-dialog owning w closes */
{
    lj_window *win = lj_window_of(w);
    int id;
    for (id = LJ_WIN_DA2532_FUNCTIONS; id <= LJ_WIN_DA2532_PAGE_NAMES; id++) if (*sub((enum lj_win)id) == win) { close_sub((enum lj_win)id); return; }
}
static void on_sub_close(lj_widget *w, void *ud) { (void)ud; close_from(w); }

/* 2532 Function List: the functions a key can take (lvFunctions, a drag source). User-key-only functions first (05 s4.2): the page
 * changes; then LJ2's hotkey functions (programs excluded: they are dragged from their own lists). */
static struct { uint16_t fn[LJ_FN_COUNT]; char cap[LJ_FN_COUNT][64]; const char *items[LJ_FN_COUNT]; int n; } lj_da_fl;
static void fl_event(lj_widget *w, int ev, void *ud)
{
    int i = lj_w_listview_index(w);
    (void)ud;
    if (ev == LJ_WEV_CLICK && i >= 0 && i < lj_da_fl.n) lj_dnd_begin(w, LJ_DA_DND_FN, lj_da_fl.fn[i]);   /* DragMode = dmAutomatic */
}
static void fill_functions(lj_window *win)
{
    lj_widget *lv = lj_window_find(win, "lvFunctions");
    int f;
    lj_da_fl.n = 0;
    for (f = LJ_FN_DA2532_PAGE_1; f <= LJ_FN_DA2532_PAGE_18; f++) { lj_da_fl.fn[lj_da_fl.n] = (uint16_t)f; lj_fn_caption((enum lj_fn)f, lj_da_fl.cap[lj_da_fl.n], 64); lj_da_fl.n++; }
    for (f = 1; f < LJ_FN_COUNT; f++) {
        if (lj_fn_table[f].group == LJ_FNG_PROGRAMS || (lj_fn_table[f].flags & LJ_FNF_EXT)) continue;
        lj_da_fl.fn[lj_da_fl.n] = (uint16_t)f; lj_fn_caption((enum lj_fn)f, lj_da_fl.cap[lj_da_fl.n], 64); lj_da_fl.n++;
    }
    for (f = 0; f < lj_da_fl.n; f++) lj_da_fl.items[f] = lj_da_fl.cap[f];
    if (lv) { lj_w_listview_set_items(lv, lj_da_fl.items, lj_da_fl.n); lv->on_event = fl_event; }
}
int lj_da2532_function_at(int row) { return row >= 0 && row < lj_da_fl.n ? lj_da_fl.fn[row] : LJ_FN_NONE; }

/* Color Scheme / Tab Colors: each panel shows its colour; a click steps it through the 16 VGA colours */
static const char *const lj_da_cs_text[5] = { "pFunctions", "pSequences", "pCues", "pCuelists", "pBGCues" };
static const char *const lj_da_cs_back[5] = { "pbgrFunctions", "pbgrSequences", "pBgrCues", "pbgrCuelists", "pbgrBgCues" };
static const char *const lj_da_tc[4] = { "pActiveText", "pInactiveText", "pActiveBG", "pInactiveBG" };
static const uint32_t lj_da_tc_dflt[4] = { 0x000000u, 0x000000u, 0xC0C0C0u, 0xC0C0C0u };   /* the classic tab colours, PROPOSAL rig 05 s4 */
static int panel_key(const char *name, char *key, size_t cap, uint32_t *dflt)
{
    int i;
    for (i = 0; i < 5; i++) {
        if (!strcmp(name, lj_da_cs_text[i])) { snprintf(key, cap, "ColorText%d", i + 1); *dflt = lj_da_text_dflt[i + 1]; return 0; }
        if (!strcmp(name, lj_da_cs_back[i])) { snprintf(key, cap, "ColorBack%d", i + 1); *dflt = lj_da_back_dflt[i + 1]; return 0; }
    }
    for (i = 0; i < 4; i++) if (!strcmp(name, lj_da_tc[i])) { snprintf(key, cap, "Tab%s", name + 1); *dflt = lj_da_tc_dflt[i]; return 0; }
    return -1;
}
static void paint_panels(lj_window *win)
{
    static const char *const *const lists[3] = { lj_da_cs_text, lj_da_cs_back, lj_da_tc };
    static const int counts[3] = { 5, 5, 4 };
    int l, i;
    for (l = 0; l < 3; l++)
        for (i = 0; i < counts[l]; i++) {
            lj_widget *p = lj_window_find(win, lists[l][i]);
            char key[24]; uint32_t d;
            if (p && panel_key(p->name, key, sizeof key, &d) == 0) { p->color = ini_rgb(key, d); lj_widget_invalidate(p); }
        }
}
static void on_color_panel(lj_widget *w, void *ud)
{
    char key[24];
    uint32_t d, cur;
    int i;
    (void)ud;
    if (!w->name || panel_key(w->name, key, sizeof key, &d) != 0) return;
    cur = ini_rgb(key, d);
    for (i = 0; i < 16 && lj_da_vga[i] != cur; i++) {}
    lj_ini_seti(lj_ctx_get()->main_ini, LJ_DA_SEC, key, (int)lj_da_vga[(i + 1) % 16]);   /* ponytail: no TColorDialog; add one when a colour outside the 16 is asked for */
    paint_panels(lj_window_of(w));
    refresh_main();
}

/* Set Tab Size: Main.Ini [2532] TabWidth TabHeight AutoWidth AutoHeight (the udWidth / udHeight ranges of the DFM) */
static void tab_size_ok(lj_widget *w, void *ud)
{
    lj_window *win = lj_window_of(w);
    lj_ini *ini = lj_ctx_get()->main_ini;
    (void)ud;
    lj_ini_seti(ini, LJ_DA_SEC, "TabWidth", lj_w_updown_position(lj_window_find(win, "udWidth")));
    lj_ini_seti(ini, LJ_DA_SEC, "TabHeight", lj_w_updown_position(lj_window_find(win, "udHeight")));
    lj_ini_seti(ini, LJ_DA_SEC, "AutoWidth", lj_w_checkbox_checked(lj_window_find(win, "cbAutoWidth")));
    lj_ini_seti(ini, LJ_DA_SEC, "AutoHeight", lj_w_checkbox_checked(lj_window_find(win, "cbAutoHeight")));
    close_sub(LJ_WIN_DA2532_TAB_HEIGHT);
}
static void tab_size_auto(lj_widget *w, void *ud)
{
    lj_window *win = lj_window_of(w);
    lj_widget *a;
    (void)ud;
    if ((a = lj_window_find(win, "eWidth")) != NULL) a->enabled = !lj_w_checkbox_checked(lj_window_find(win, "cbAutoWidth"));
    if ((a = lj_window_find(win, "udWidth")) != NULL) a->enabled = !lj_w_checkbox_checked(lj_window_find(win, "cbAutoWidth"));
    if ((a = lj_window_find(win, "eHeight")) != NULL) a->enabled = !lj_w_checkbox_checked(lj_window_find(win, "cbAutoHeight"));
    if ((a = lj_window_find(win, "udHeight")) != NULL) a->enabled = !lj_w_checkbox_checked(lj_window_find(win, "cbAutoHeight"));
    lj_window_invalidate_all(win);
}

/* Edit 2532 Text: Edit1 (MaxLength 20); OK writes the Customize Text of the key the popup acted on */
static void text_button(lj_widget *w, int ev, void *ud)
{
    lj_window *win = lj_window_of(w);
    (void)ud;
    if (ev != LJ_WEV_CLICK) return;
    if (ud && lj_da_text.target) {
        const char *t = lj_w_edit_text(lj_window_find(win, "Edit1"));
        snprintf(lj_da_text.target->text, LJ_DA_TEXT, "%s", t ? t : "");
        lj_da2532_save();
        refresh_main();
    }
    close_sub(LJ_WIN_DA2532_TEXT);
}

/* 2532 Page Names: lv1 (Page# | Page Name), eName edits the selected row; OK keeps, Cancel drops */
static struct { char name[LJ_DA_PAGES][LJ_DA_TEXT]; char row[LJ_DA_PAGES][40]; const char *rows[LJ_DA_PAGES]; } lj_da_pn;
static void pn_fill(lj_window *win)
{
    int i;
    lj_widget *lv = lj_window_find(win, "lv1");
    for (i = 0; i < LJ_DA_PAGES; i++) { snprintf(lj_da_pn.row[i], sizeof lj_da_pn.row[i], "%d\t%s\t", i + 1, lj_da_pn.name[i]); lj_da_pn.rows[i] = lj_da_pn.row[i]; }
    if (lv) { int sel = lj_w_listview_index(lv); lj_w_listview_set_items(lv, lj_da_pn.rows, LJ_DA_PAGES); if (sel >= 0) ((lj_w_listview *)lv)->index = sel; lj_widget_invalidate(lv); }
}
static void pn_select(lj_widget *w, void *ud)
{
    int i = lj_w_listview_index(w);
    (void)ud;
    if (i >= 0 && i < LJ_DA_PAGES) lj_w_edit_set_text(lj_window_find(lj_window_of(w), "eName"), lj_da_pn.name[i]);
}
static void pn_name(lj_widget *w, void *ud)
{
    lj_window *win = lj_window_of(w);
    int i = lj_w_listview_index(lj_window_find(win, "lv1"));
    const char *t = lj_w_edit_text(w);
    (void)ud;
    if (i < 0 || i >= LJ_DA_PAGES || !t || !strcmp(t, lj_da_pn.name[i])) return;
    snprintf(lj_da_pn.name[i], LJ_DA_TEXT, "%s", t);
    pn_fill(win);
}
static void pn_default(lj_widget *w, void *ud) { (void)ud; memset(lj_da_pn.name, 0, sizeof lj_da_pn.name); pn_fill(lj_window_of(w)); }
static void pn_button(lj_widget *w, int ev, void *ud)   /* w unused: the dialog is a singleton */
{
    (void)w;
    if (ev != LJ_WEV_CLICK) return;
    if (ud) { memcpy(da.page_name, lj_da_pn.name, sizeof da.page_name); lj_da2532_save(); fill_tabs(); }
    close_sub(LJ_WIN_DA2532_PAGE_NAMES);
}

static const lj_binding lj_da_fl_b[] = { { "FormClose", on_sub_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormResize", lj_handler_noop, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_da_cs_b[] = { { "ColorPanelClick", on_color_panel, NULL }, { "FormClose", on_sub_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_da_th_b[] = { { "bOkClick", tab_size_ok, NULL }, { "cbAutoClick", tab_size_auto, NULL }, { "eHeightChange", lj_handler_noop, NULL }, { "eWidthChange", lj_handler_noop, NULL },
                                         { "FormClose", on_sub_close, NULL }, { "udChanging", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_da_tx_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_da_pn_b[] = { { "DefaultAll1Click", pn_default, NULL }, { "eNameChange", pn_name, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
                                         { "Help1Click", on_help, NULL }, { "lv1Change", pn_select, NULL }, { NULL, NULL, NULL } };

static void hook_button(lj_window *win, const char *name, void (*fn)(lj_widget *, int, void *), void *ud) { lj_widget *b = lj_window_find(win, name); if (b) { b->on_event = fn; b->ud = ud; } }

static lj_window *open_sub(enum lj_win id)
{
    static int yes = 1;
    lj_window **w = sub(id);
    lj_ini *ini = lj_ctx_get()->main_ini;
    if (*w) return *w;
    switch (id) {
    case LJ_WIN_DA2532_FUNCTIONS:  *w = lj_window_create(&lj_form_TBUTTONFUNCTIONS2532FORM, lj_da_fl_b); if (*w) fill_functions(*w); break;
    case LJ_WIN_DA2532_COLORS:     *w = lj_window_create(&lj_form_TCOLORSCHEME2532FORM, lj_da_cs_b); if (*w) paint_panels(*w); break;
    case LJ_WIN_DA2532_TAB_COLORS: *w = lj_window_create(&lj_form_TDA2532_TABCOLORSFORM, lj_da_cs_b); if (*w) paint_panels(*w); break;
    case LJ_WIN_DA2532_TAB_HEIGHT:
        if ((*w = lj_window_create(&lj_form_TDA2532TABHEIGHT, lj_da_th_b)) != NULL) {
            lj_w_updown_set_position(lj_window_find(*w, "udWidth"), lj_ini_geti(ini, LJ_DA_SEC, "TabWidth", 32));
            lj_w_updown_set_position(lj_window_find(*w, "udHeight"), lj_ini_geti(ini, LJ_DA_SEC, "TabHeight", 11));
            lj_w_checkbox_set_checked(lj_window_find(*w, "cbAutoWidth"), lj_ini_geti(ini, LJ_DA_SEC, "AutoWidth", 0) != 0);
            lj_w_checkbox_set_checked(lj_window_find(*w, "cbAutoHeight"), lj_ini_geti(ini, LJ_DA_SEC, "AutoHeight", 0) != 0);
        }
        break;
    case LJ_WIN_DA2532_TEXT:
        if (!lj_da_text.target) { lj_log("2532: Edit 2532 Text opens from a key's Customize Text"); return NULL; }
        if ((*w = lj_window_create(&lj_form_TEDIT2532TEXTFORM, lj_da_tx_b)) != NULL) {
            char t[48];
            lj_w_edit_set_text(lj_window_find(*w, "Edit1"), lj_da2532_label(lj_da_text.target, t, sizeof t));
            hook_button(*w, "BitBtn1", text_button, &yes); hook_button(*w, "BitBtn2", text_button, NULL);   /* bkOK / bkCancel: no OnClick in the DFM */
        }
        break;
    case LJ_WIN_DA2532_PAGE_NAMES:
        if ((*w = lj_window_create(&lj_form_TNAME2532PAGESFORM, lj_da_pn_b)) != NULL) {
            memcpy(lj_da_pn.name, da.page_name, sizeof lj_da_pn.name);
            pn_fill(*w);
            hook_button(*w, "bOk", pn_button, &yes); hook_button(*w, "BitBtn2", pn_button, NULL);
        }
        break;
    default: return NULL;
    }
    if (!*w) { lj_log("2532: cannot create %s", lj_win_class(id)); return NULL; }
    lj_win_show(*w, (id == LJ_WIN_DA2532_FUNCTIONS || id == LJ_WIN_DA2532_TAB_HEIGHT) ? LJ_DA_STAY_ON_TOP : 0u);   /* fsStayOnTop in those two DFMs */
    return *w;
}

/* ================= the family entry points ================= */
lj_window *lj_da2532_open(enum lj_win id) { return id == LJ_WIN_DA2532 ? open_main() : (id > LJ_WIN_DA2532 && id <= LJ_WIN_DA2532_PAGE_NAMES) ? open_sub(id) : NULL; }
void lj_da2532_close(enum lj_win id)
{
    int i;
    if (id != LJ_WIN_DA2532) { if (id > LJ_WIN_DA2532 && id <= LJ_WIN_DA2532_PAGE_NAMES) close_sub(id); return; }
    if (!ui.win) return;
    for (i = LJ_WIN_DA2532_FUNCTIONS; i <= LJ_WIN_DA2532_PAGE_NAMES; i++) close_sub((enum lj_win)i);
    lj_win_hide(ui.win);
    lj_window_destroy(ui.win);
    ui.win = NULL; ui.grid = ui.ugrid = ui.tbc = NULL;
}
lj_window *lj_da2532_get(enum lj_win id) { return id == LJ_WIN_DA2532 ? ui.win : (id > LJ_WIN_DA2532 && id <= LJ_WIN_DA2532_PAGE_NAMES) ? *sub(id) : NULL; }
void lj_da2532_refresh(enum lj_win id)
{
    learn_poll();
    if (id == LJ_WIN_DA2532) refresh_main();
}

/* ---- the hotkey rows: 2532 Control and the EXT rows (keyboard .Hot lines, MIDIMap.dat fn / cc lines) ---- */
static void fn_2532(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    (void)prog; (void)ud;
    if (fn == LJ_FN_2532_CONTROL) { if (edge) { if (ui.win) lj_da2532_close(LJ_WIN_DA2532); else open_main(); } return; }
    if (fn >= LJ_FN_DA2532_LATCH_1 && fn <= LJ_FN_DA2532_LATCH_8) lj_da2532_latch((int)(fn - LJ_FN_DA2532_LATCH_1) + 1, edge);
    else if (fn >= LJ_FN_DA2532_FLASH_1 && fn <= LJ_FN_DA2532_FLASH_8) lj_da2532_flash((int)(fn - LJ_FN_DA2532_FLASH_1) + 1, edge);
    else if (fn >= LJ_FN_DA2532_USER_1 && fn <= LJ_FN_DA2532_USER_18) lj_da2532_user((int)(fn - LJ_FN_DA2532_USER_1) + 1, edge);
    else if (!edge) return;
    else if (fn >= LJ_FN_DA2532_PAGE_1 && fn <= LJ_FN_DA2532_PAGE_18) lj_da2532_set_page((int)(fn - LJ_FN_DA2532_PAGE_1) + 1);
    else if (fn >= LJ_FN_DA2532_BANK_1 && fn <= LJ_FN_DA2532_BANK_7) lj_da2532_set_bank((int)(fn - LJ_FN_DA2532_BANK_1) + 1);
}

void lj_da2532_bind_fns(void)
{
    int f;
    lj_fn_bind(LJ_FN_2532_CONTROL, fn_2532, NULL);
    for (f = LJ_FN_DA2532_LATCH_1; f <= LJ_FN_DA2532_BANK_7; f++) lj_fn_bind((enum lj_fn)f, fn_2532, NULL);
}
