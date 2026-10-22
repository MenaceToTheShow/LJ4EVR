/* lj_hotkeys.c - keyboard hotkeys of lj_hotkeys.h: the setup struct, the .Hot text
 * record (design 5.2 grammar, kind "hot", format 1), the match rules of [01 s7]
 * and the Key Combo text. The focus rule ("only when an LJ4EVR window has OS
 * focus") is the platform's: a key event reaches lj_desktop_key only from a
 * focused window, so nothing here asks about focus. Alt is rejected at match
 * time and VK_MENU/VK_SHIFT/VK_CONTROL are refused as the main key, so an Alt
 * combination can neither be stored nor fire. */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf under /W4 /WX on MSVC (as core/lj_textrec.c) */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "io/lj_hotkeys.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "core/lj_types.h"
#include "core/lj_textrec.h"

enum { LJ_VK_SHIFT = 0x10, LJ_VK_CONTROL = 0x11, LJ_VK_MENU = 0x12, LJ_VK_ESCAPE = 0x1B, LJ_VK_F1 = 0x70, LJ_VK_F12 = 0x7B };   /* Windows virtual-key codes, the numbers LJ2's tables carry */

static int lj_hk_is_fkey(uint16_t key) { return key >= LJ_VK_F1 && key <= LJ_VK_F12; }
static int lj_hk_is_prog(uint16_t fn) { return fn > 0 && fn < LJ_FN_COUNT && lj_fn_table[fn].group == LJ_FNG_PROGRAMS; }

/* index of the entry with this combo, -1 when absent */
static int lj_hk_index(const lj_hotkeys *h, uint16_t key, int shift, int ctrl)
{
    int i;
    for (i = 0; i < h->n; i++)
        if (h->e[i].key == key && h->e[i].shift == (shift != 0) && h->e[i].ctrl == (ctrl != 0)) return i;
    return -1;
}

static const char *lj_hk_check_entry(const lj_hotkey *k)
{
    if (k->key == 0 || k->key == LJ_VK_SHIFT || k->key == LJ_VK_CONTROL || k->key == LJ_VK_MENU) return "key is Alt, Shift, Control or none";
    if (k->shift > 1 || k->ctrl > 1 || k->scroll > 1) return "shift/ctrl/scrl must be 0 or 1";
    if (k->fn == 0 || k->fn >= LJ_FN_COUNT) return "fn out of range";
    if (lj_hk_is_prog(k->fn) ? k->prog_num == 0 : k->prog_num != 0) return "num only (and always) on a program row";
    return NULL;
}

static void lj_hk_err(char *err, size_t cap, const char *fmt, ...)
{
    va_list ap;
    if (!err || cap == 0) return;
    va_start(ap, fmt);
    vsnprintf(err, cap, fmt, ap);
    va_end(ap);
}

/* design 5.2: a string is quoted when it holds spaces, a quote inside is doubled; we always quote on write */
static void lj_hk_quote(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    if (cap == 0) return;
    if (o + 1 < cap) dst[o++] = '"';
    for (; *src; src++) {
        if (*src == '"') { if (o + 2 >= cap) break; dst[o++] = '"'; }
        if (o + 2 >= cap) break;
        dst[o++] = *src;
    }
    if (o + 1 < cap) dst[o++] = '"';
    dst[o] = 0;
}

static void lj_hk_unquote(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    if (cap == 0) return;
    if (*src != '"') { lj_str_copy(dst, cap, src); return; }
    for (src++; *src && o + 1 < cap; src++) {
        if (*src == '"') { if (src[1] != '"') break; src++; }
        dst[o++] = *src;
    }
    dst[o] = 0;
}

void lj_hotkeys_init(lj_hotkeys *h) { memset(h, 0, sizeof *h); h->enabled = 1; h->disable_in_dialogs = 1; }

const char *lj_hotkeys_validate(const lj_hotkeys *in)
{
    int i, j;
    const char *m;
    if (!in) return "no setup";
    if (in->n < 0 || in->n > LJ_HOTKEYS_MAX) return "entry count out of range";
    if (in->enabled > 1 || in->disable_in_dialogs > 1) return "enabled/dialogs must be 0 or 1";
    for (i = 0; i < in->n; i++) {
        if ((m = lj_hk_check_entry(&in->e[i])) != NULL) return m;
        for (j = 0; j < i; j++)   /* ponytail: O(n^2) over at most 512 rows, once per load/save */
            if (in->e[j].key == in->e[i].key && in->e[j].shift == in->e[i].shift && in->e[j].ctrl == in->e[i].ctrl) return "duplicate key combo";
    }
    return NULL;
}

int lj_hotkeys_load(const char *path, lj_hotkeys *out, char *err, size_t cap)
{
    char key[32], val[256], fname[64];
    int fmt, r;
    lj_rec *rec;
    const char *m;
    if (err && cap) err[0] = 0;
    if (!path || !out) return -1;
    rec = lj_rec_open_read(path, "hot", &fmt);
    if (!rec) { lj_hk_err(err, cap, "cannot read %s", path); return -1; }
    lj_hotkeys_init(out);
    while ((r = lj_rec_next(rec, key, sizeof key, val, sizeof val)) == 1) {
        if (strcmp(key, "name") == 0) {
            lj_hk_unquote(out->name, sizeof out->name, val);
        } else if (strcmp(key, "enabled") == 0) {
            int en = 1, dl = 1;
            if (sscanf(val, "%d dialogs=%d", &en, &dl) < 1) { lj_hk_err(err, cap, "bad enabled line"); r = -1; break; }
            out->enabled = en != 0; out->disable_in_dialogs = dl != 0;
        } else if (strcmp(key, "key") == 0) {
            unsigned vk, sh, ct, sc, num = 0;
            lj_hotkey k;
            if (sscanf(val, "vk=%u shift=%u ctrl=%u scrl=%u fn=%63s num=%u", &vk, &sh, &ct, &sc, fname, &num) < 5 || vk > 0xFFFF || sh > 1 || ct > 1 || sc > 1) {
                lj_hk_err(err, cap, "bad key line: %s", val); r = -1; break;
            }
            if (out->n >= LJ_HOTKEYS_MAX) { lj_hk_err(err, cap, "more than %d hotkeys", LJ_HOTKEYS_MAX); r = -1; break; }
            memset(&k, 0, sizeof k);
            k.key = (uint16_t)vk; k.shift = (uint8_t)sh; k.ctrl = (uint8_t)ct; k.scroll = (uint8_t)sc; k.prog_num = num;
            k.fn = (uint16_t)lj_fn_by_name(fname);
            if (k.fn == LJ_FN_NONE) { lj_hk_err(err, cap, "unknown function %s", fname); r = -1; break; }
            out->e[out->n++] = k;
        }
        /* any other key is skipped so an old build reads a new file (design 5.2) */
    }
    lj_rec_close(rec);
    if (r < 0) { if (err && cap && !err[0]) lj_hk_err(err, cap, "read error in %s", path); lj_hotkeys_init(out); return -1; }
    if ((m = lj_hotkeys_validate(out)) != NULL) { lj_hk_err(err, cap, "%s", m); lj_hotkeys_init(out); return -1; }
    return 0;
}

int lj_hotkeys_save(const char *path, const lj_hotkeys *in)
{
    char q[2 * sizeof in->name + 2];
    lj_rec *rec;
    const char *m;
    int i, ok = 0;
    if (!path || !in) return -1;
    if ((m = lj_hotkeys_validate(in)) != NULL) { lj_log("hotkeys: %s not saved: %s", path, m); return -1; }
    rec = lj_rec_open_write(path, "hot", 1);
    if (!rec) return -1;
    lj_hk_quote(q, sizeof q, in->name);
    ok |= lj_rec_write(rec, "name", "%s", q);
    ok |= lj_rec_write(rec, "enabled", "%d dialogs=%d", in->enabled, in->disable_in_dialogs);
    for (i = 0; i < in->n; i++) {
        const lj_hotkey *k = &in->e[i];
        if (k->prog_num)
            ok |= lj_rec_write(rec, "key ", "vk=%u shift=%u ctrl=%u scrl=%u fn=%s num=%u", k->key, k->shift, k->ctrl, k->scroll, lj_fn_table[k->fn].name, k->prog_num);
        else
            ok |= lj_rec_write(rec, "key ", "vk=%u shift=%u ctrl=%u scrl=%u fn=%s", k->key, k->shift, k->ctrl, k->scroll, lj_fn_table[k->fn].name);
    }
    ok |= lj_rec_close(rec);
    return ok ? -1 : 0;
}

int lj_hotkeys_setup_path(const char *userdir, int setup, char *out, size_t cap)
{
    int n;
    if (!out || cap == 0) return -1;
    out[0] = 0;
    if (!userdir || setup < 1 || setup > LJ_HOTKEY_SETUPS_MAX) return -1;
    n = snprintf(out, cap, "%sHotkeys/Setup%d.Hot", userdir, setup);   /* "Hotkeys\" is the exe's folder literal (0x22F7FC); '/' opens on every host */
    if (n < 0 || (size_t)n >= cap) { out[0] = 0; return -1; }         /* a half path must never be opened */
    return 0;
}

int lj_hotkeys_assign(lj_hotkeys *h, const lj_hotkey *k)
{
    int i;
    if (!h || !k || lj_hk_check_entry(k)) return -1;
    i = lj_hk_index(h, k->key, k->shift, k->ctrl);
    if (i < 0) { if (h->n >= LJ_HOTKEYS_MAX) return -1; i = h->n++; }
    h->e[i] = *k;
    return 0;
}

static void lj_hk_remove(lj_hotkeys *h, int i)
{
    memmove(&h->e[i], &h->e[i + 1], (size_t)(h->n - i - 1) * sizeof h->e[0]);
    h->n--;
}

int lj_hotkeys_clear_key(lj_hotkeys *h, uint16_t key, int shift, int ctrl)
{
    int i;
    if (!h) return -1;
    i = lj_hk_index(h, key, shift, ctrl);
    if (i < 0) return -1;
    lj_hk_remove(h, i);
    return 0;
}

/* a program row matches on (fn, prog_num); a function row on fn alone */
static int lj_hk_fn_match(const lj_hotkey *e, enum lj_fn fn, uint32_t prog_num)
{
    return e->fn == (uint16_t)fn && (!lj_hk_is_prog(e->fn) || e->prog_num == prog_num);
}

int lj_hotkeys_clear_fn(lj_hotkeys *h, enum lj_fn fn, uint32_t prog_num)
{
    int i, removed = 0;
    if (!h) return -1;
    for (i = h->n - 1; i >= 0; i--)
        if (lj_hk_fn_match(&h->e[i], fn, prog_num)) { lj_hk_remove(h, i); removed = 1; }
    return removed ? 0 : -1;
}

const lj_hotkey *lj_hotkeys_find_fn(const lj_hotkeys *h, enum lj_fn fn, uint32_t prog_num)
{
    int i;
    if (!h) return NULL;
    for (i = 0; i < h->n; i++) if (lj_hk_fn_match(&h->e[i], fn, prog_num)) return &h->e[i];
    return NULL;
}

const lj_hotkey *lj_hotkeys_match(const lj_hotkeys *h, uint16_t key, int shift, int ctrl, int alt, int scroll_lock, int in_dialog)
{
    int i;
    const lj_hotkey *e;
    if (!h || !h->enabled || alt) return NULL;                   /* Setup > Hotkeys > Hotkeys Enabled; [Alt] combinations are not recognized [01 s7] */
    i = lj_hk_index(h, key, shift, ctrl);
    if (i < 0) return NULL;
    e = &h->e[i];
    if (lj_hk_is_fkey(key)) return e;                            /* F1-F12 and their Shift/Control combinations always work */
    if (in_dialog && h->disable_in_dialogs) return NULL;         /* Disable Hotkeys in Dialogs */
    if (e->scroll && !scroll_lock) return NULL;                  /* Scroll Lock scope */
    return e;
}

/* PROPOSAL: LJ2 names the key itself ("Esc", "B", "F1"); undefined keys "list their value as
 * [XXX]" (CHM hotkeys.htm), rendered here as the decimal virtual-key code in brackets. */
static const char *lj_hk_key_name(uint16_t key, char *buf, size_t cap)
{
    static const struct { uint16_t vk; const char *name; } names[] = {
        { 0x08, "BkSp" }, { 0x09, "Tab" }, { 0x0D, "Enter" }, { 0x13, "Pause" }, { 0x14, "CapsLock" }, { LJ_VK_ESCAPE, "Esc" },
        { 0x20, "Space" }, { 0x21, "PgUp" }, { 0x22, "PgDn" }, { 0x23, "End" }, { 0x24, "Home" }, { 0x25, "Left" }, { 0x26, "Up" },
        { 0x27, "Right" }, { 0x28, "Down" }, { 0x2D, "Ins" }, { 0x2E, "Del" }, { 0x6A, "Num *" }, { 0x6B, "Num +" }, { 0x6D, "Num -" },
        { 0x6E, "Num ." }, { 0x6F, "Num /" }, { 0x91, "ScrollLock" } };
    int i;
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) { buf[0] = (char)key; buf[1] = 0; return buf; }   /* letters register as capitals [01 s7] */
    if (lj_hk_is_fkey(key)) { snprintf(buf, cap, "F%d", key - LJ_VK_F1 + 1); return buf; }
    if (key >= 0x60 && key <= 0x69) { snprintf(buf, cap, "Num %d", key - 0x60); return buf; }
    for (i = 0; i < LJ_COUNT_OF(names); i++) if (names[i].vk == key) return names[i].name;
    snprintf(buf, cap, "[%u]", key);
    return buf;
}

int lj_hotkeys_combo_text(const lj_hotkey *k, char *dst, size_t cap)
{
    char kb[16];
    int n;
    if (!dst || cap == 0) return -1;
    dst[0] = 0;
    if (!k || k->key == 0) return -1;
    n = snprintf(dst, cap, "%s%s%s", k->shift ? "[Shift] " : "", k->ctrl ? "[Control] " : "", lj_hk_key_name(k->key, kb, sizeof kb));   /* "[Shift] " / "[Control] " = exe literals 0x1C220C / 0x1C2220 */
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

void lj_hotkeys_default(lj_hotkeys *h)
{
    /* PROPOSAL: the installer ships no .Hot file and LJ2's compiled-in "Default Hotkeys" table
     * is undecoded [10 line 171]; these are the function rows legible in hotkeys1.gif (the CHM's
     * Edit Hotkeys screenshot, an older function list whose "*Blind" / "Blind Toggle" are today's
     * "Sequence Blind" / "Sequence Blind Toggle"). The cue rows in that picture (1-5, [Shift] 1-5,
     * [Control] 1-3 -> cues 1-13) are the help author's own show and are not shipped. */
    static const lj_hotkey rows[] = {
        { LJ_VK_ESCAPE, 0, 0, 0, (uint16_t)LJ_FN_CLOSE_WINDOW,          0 },   /* measured from hotkeys1.gif, row 1 "C... Esc" (caption half-hidden by the callout) */
        { LJ_VK_ESCAPE, 1, 0, 0, (uint16_t)LJ_FN_CLOSE_ALL_WINDOWS,     0 },   /* measured from hotkeys1.gif, "Close All Windows  [Shift] Esc" */
        { 'B',          1, 0, 0, (uint16_t)LJ_FN_SEQUENCE_BLIND_FLASH,  0 },   /* measured from hotkeys1.gif, "*Blind  [Shift] B" */
        { 'B',          0, 0, 0, (uint16_t)LJ_FN_SEQUENCE_BLIND_TOGGLE, 0 },   /* measured from hotkeys1.gif, "Blind Toggle  B" */
    };
    int i;
    lj_hotkeys_init(h);
    lj_str_copy(h->name, sizeof h->name, "Default");   /* the first saved setup in hotkeys2.gif */
    for (i = 0; i < LJ_COUNT_OF(rows); i++) lj_hotkeys_assign(h, &rows[i]);
}

static void lj_hk_setup_fallback(int setup, lj_hotkeys *out)
{
    if (setup == 1) { lj_hotkeys_default(out); return; }
    lj_hotkeys_init(out);
    snprintf(out->name, sizeof out->name, "Setup %d", setup);   /* PROPOSAL: LJ2's name for an unsaved setup is unknown */
}

int lj_hotkeys_setup_load(const char *dir, int setup, lj_hotkeys *out, char *err, size_t cap)
{
    char path[512];
    FILE *f;
    if (err && cap) err[0] = 0;
    if (!out) return -1;
    if (lj_hotkeys_setup_path(dir, setup, path, sizeof path) != 0) { lj_hk_err(err, cap, "hotkey setup %d: bad folder or number", setup); lj_hk_setup_fallback(1, out); return -1; }
    f = fopen(path, "rb");                                        /* missing = the fallback, silently (no "cannot open .../Setup1.Hot" start-up line) */
    if (!f) { lj_hk_setup_fallback(setup, out); return 0; }
    fclose(f);
    if (lj_hotkeys_load(path, out, err, cap) == 0) return 0;
    lj_hk_setup_fallback(setup, out);
    return -1;
}

int lj_hotkeys_setup_of_file(const char *file)
{
    const char *b, *p;
    if (!file) return -1;
    for (b = p = file; *p; p++) if (*p == '/' || *p == '\\' || *p == ':') b = p + 1;
    if ((b[0] | 0x20) != 's' || (b[1] | 0x20) != 'e' || (b[2] | 0x20) != 't' || (b[3] | 0x20) != 'u' || (b[4] | 0x20) != 'p') return -1;
    if (b[5] < '1' || b[5] > '0' + LJ_HOTKEY_SETUPS_MAX || b[6] != '.') return -1;
    if ((b[7] | 0x20) != 'h' || (b[8] | 0x20) != 'o' || (b[9] | 0x20) != 't' || b[10]) return -1;
    return b[5] - '0';
}
