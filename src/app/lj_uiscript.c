/* lj_uiscript.c - the --script driver of docs/ui-script.md (lj_uiscript.h).
 * The file is read once, tokenised in place and validated line by line at load;
 * lj_uiscript_step then runs one statement per call. Clicks are synthesised
 * lj_ui_events in window-surface coordinates (root origin + widget origin);
 * `shot` writes one PNG per open window from its lj_window_surface, which
 * --selftest proves equals the screen pixel for pixel. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include "app/lj_uiscript.h"
#include "app/lj_app_cuelist_host.h"
#include "app/lj_app_seam.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_common.h"
#include "controls/lj_ctl_movement.h"
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_statusbar.h"
#include "desktop/lj_unassigned.h"
#include "gfx/lj_bmp.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "ui/widgets/lj_w_spinedit.h"
#include "windows/lj_cue_form.h"
#include "windows/lj_fixture_config.h"
#include "windows/lj_user_palettes.h"
#include "windows/lj_warnings.h"
#include "windows/lj_windows.h"
#include "io/lj_midi.h"
#include "io/lj_artnet.h"
#include "io/lj_audio.h"
#include "io/lj_dmxin.h"
#include "io/lj_remote.h"
#include "io/lj_sacn.h"
#include "model/lj_cuelist.h"
#include "model/lj_show.h"
#include "windows/lj_cuelist_form.h"
#include "miniz.h"

enum { LJ_US_ARGS = 6 };
enum lj_us_verb { LJ_US_OPEN, LJ_US_CLOSE, LJ_US_CLICK, LJ_US_RCLICK, LJ_US_DBLCLICK, LJ_US_DRAG, LJ_US_TYPE, LJ_US_KEY, LJ_US_WAIT, LJ_US_SHOT,
                  LJ_US_ASSERT_CAPTION, LJ_US_ASSERT_TEXT, LJ_US_ASSERT_ENABLED, LJ_US_ASSERT_VISIBLE, LJ_US_ASSERT_FRAME, LJ_US_ASSERT_VIEW,
                  LJ_US_ASSERT_FILE, LJ_US_ASSERT_PPS, LJ_US_ASSERT_CLIENT, LJ_US_ASSERT_POS, LJ_US_LOG, LJ_US_QUIT,
                  /* W2 (appended) */ LJ_US_MIDI, LJ_US_HOTKEY, LJ_US_ASSERT_LED, LJ_US_ASSERT_FILE_LINE, LJ_US_SLEEP_UNTIL_TICK, LJ_US_MOVE,
                  /* W3 (appended; assert_pos keeps its W1 row: a fixture number as the first word is the W3 form) */
                  LJ_US_CL_LOAD, LJ_US_CL_GOTO, LJ_US_CL_EXECUTE, LJ_US_CL_ASSERT_LINE, LJ_US_GS2_IMPORT, LJ_US_ASSERT_PROMPT, LJ_US_MM_SET,
                  /* W4 (appended) */ LJ_US_DUMP_CAPTIONS,
                  /* W5 (appended) */ LJ_US_SACN_IN, LJ_US_ARTNET_IN, LJ_US_AUDIO_INJECT, LJ_US_REMOTE,
                  /* W5 fix pass (appended) */ LJ_US_ASSERT_DOWN,
                  /* W6 must-fix (appended) */ LJ_US_ASSERT_CHECKED, LJ_US_VERBS };
static const struct { const char *name; int min_args, max_args; } lj_us_verbs[LJ_US_VERBS] = {
    { "open", 1, 1 }, { "close", 1, 1 }, { "click", 1, 3 }, { "rclick", 1, 3 }, { "dblclick", 1, 3 }, { "drag", 2, 6 }, { "type", 1, 1 }, { "key", 1, 2 },
    { "wait", 1, 1 }, { "shot", 1, 1 }, { "assert_caption", 2, 2 }, { "assert_text", 2, 2 }, { "assert_enabled", 2, 2 }, { "assert_visible", 2, 2 },
    { "assert_frame", 1, 2 }, { "assert_view", 1, 4 }, { "assert_file", 2, 2 }, { "assert_pps", 3, 3 }, { "assert_client", 3, 3 }, { "assert_pos", 3, 3 }, { "log", 1, 1 }, { "quit", 0, 0 },
    /* W2 (appended; docs/ui-script.md "W2 verbs"): parsed and type-checked here, executed by the app track (lj_us_exec_w2) */
    { "midi", 3, 3 }, { "hotkey", 2, 2 }, { "assert_led", 2, 2 }, { "assert_file_line", 2, 2 }, { "sleep_until_tick", 1, 1 }, { "move", 3, 3 },
    /* W3 (appended; docs/ui-script.md "W3 verbs") */
    { "cl_load", 1, 1 }, { "cl_goto", 1, 1 }, { "cl_execute", 0, 0 }, { "cl_assert_line", 2, 2 }, { "gs2_import", 1, 1 }, { "assert_prompt", 1, 1 }, { "mm_set", 4, 4 },
    /* W4 (appended; docs/ui-script.md "W4 verbs") */
    { "dump_captions", 1, 1 },
    /* W5 (appended; docs/ui-script.md "W5 verbs") */
    { "sacn_in", 2, 6 }, { "artnet_in", 2, 6 }, { "audio_inject", 1, 2 }, { "remote", 1, 6 },
    /* W5 fix pass (appended): a TSpeedButton's Down (ME-181) */
    { "assert_down", 2, 2 },
    /* W6 must-fix r1 (appended): a TCheckBox / TRadioButton's Checked (ME-192) */
    { "assert_checked", 2, 2 } };   /* assert_view gains W2 field names inside its W1 row */

typedef struct { int verb, line, nargs; char *arg[LJ_US_ARGS]; } lj_us_stmt;

/* ponytail: one script per process, like the one UI thread that runs it. */
static struct {
    char *buf; lj_us_stmt *st; int n, pc;
    int state;                         /* 0 nothing loaded, 1 running, 2 finished ok, -1 failed */
    int line;                          /* for lj_uiscript_line / lj_uiscript_fail */
    uint64_t wait_until;               /* `wait` in progress when != 0 */
    uint64_t tick_until, tick_deadline;/* `sleep_until_tick` in progress when tick_deadline != 0 */
    lj_window *cur;                    /* the window `type` / `key` go to: the last one opened or clicked */
    lj_uiscript_host host;
} lj_us;

static const struct { const char *name; int vk; } lj_us_keys[] = {
    { "Enter", 0x0D }, { "Esc", 0x1B }, { "Tab", 0x09 }, { "Space", 0x20 }, { "Up", 0x26 }, { "Down", 0x28 }, { "Left", 0x25 }, { "Right", 0x27 },
    { "Home", 0x24 }, { "End", 0x23 }, { "PgUp", 0x21 }, { "PgDn", 0x22 }, { "Del", 0x2E }, { "BS", 0x08 } };
static const char *const lj_us_ctl_names[LJ_CTL_COUNT] = { "intensity", "movement", "colors", "gobos", "beam", "effects", "levels", "lamp", "extended", "generic", "rgbpack" };

/* ---- load ------------------------------------------------------------------ */

static void lj_us_failf(int line, const char *fmt, const char *a)
{
    fprintf(stderr, "SCRIPT FAIL line %d: ", line);
    fprintf(stderr, fmt, a);
    fputc('\n', stderr);
    lj_us.state = -1;
    lj_us.line = line;
}

void lj_uiscript_fail(const char *reason) { lj_us_failf(lj_us.line, "%s", reason ? reason : ""); }

static int lj_us_int(const char *s, int *out)
{
    char *end;
    long v;
    if (!s || !*s) return -1;
    v = strtol(s, &end, 10);
    if (*end) return -1;
    *out = (int)v;
    return 0;
}

static int lj_us_vk(const char *name, int *vk, int *shift, int *ctrl, int *alt)
{
    int i;
    *shift = *ctrl = *alt = 0;
    for (;;) {
        if (strncmp(name, "Shift+", 6) == 0) { *shift = 1; name += 6; }
        else if (strncmp(name, "Ctrl+", 5) == 0) { *ctrl = 1; name += 5; }
        else if (strncmp(name, "Alt+", 4) == 0) { *alt = 1; name += 4; }
        else break;
    }
    for (i = 0; i < LJ_COUNT_OF(lj_us_keys); i++) if (strcmp(lj_us_keys[i].name, name) == 0) { *vk = lj_us_keys[i].vk; return 0; }
    if (name[0] == 'F' && name[1] && lj_us_int(name + 1, &i) == 0 && i >= 1 && i <= 12) { *vk = 0x70 + i - 1; return 0; }
    if (!name[1] && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9'))) { *vk = name[0]; return 0; }
    return -1;
}

/* Tokenises one line in place: words split on blanks, "quoted" with "" for a quote, # starts a comment. Returns the count or -1 (unterminated quote / too many). */
static int lj_us_tokens(char *s, char **arg, int cap)
{
    int n = 0;
    for (;;) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#') return n;
        if (n >= cap) return -1;
        if (*s == '"') {
            char *w = ++s;
            arg[n++] = w;
            for (;;) {
                if (!*s) return -1;
                if (*s == '"') { if (s[1] == '"') { *w++ = '"'; s += 2; continue; } s++; break; }
                *w++ = *s++;
            }
            *w = 0;
            if (*s && *s != ' ' && *s != '\t' && *s != '#') return -1;
        } else {
            arg[n++] = s;
            while (*s && *s != ' ' && *s != '\t') s++;
            if (*s) *s++ = 0;
        }
    }
}

/* assert_view fields: the W1 four + the W2 names of docs/ui-script.md, read from the app's view copy by offset and size */
#define LJ_US_VF(f) { #f, offsetof(lj_engine_status, f), sizeof(((lj_engine_status *)0)->f) }
static const struct { const char *name; size_t off, size; } lj_us_vfields[] = {
    LJ_US_VF(gm), LJ_US_VF(blackout), LJ_US_VF(freeze), LJ_US_VF(offline),
    LJ_US_VF(bg_any), LJ_US_VF(statics_any), LJ_US_VF(flash_latched), LJ_US_VF(flash_seq), LJ_US_VF(transparent_mode), LJ_US_VF(smoke_active),
    LJ_US_VF(haze_level), LJ_US_VF(chase_ms), LJ_US_VF(cue_number), LJ_US_VF(bg_number), LJ_US_VF(pending_link_cue), LJ_US_VF(snapshot_tick),
    /* W3 (appended) */
    LJ_US_VF(gm_engine_on), LJ_US_VF(gm_slot_on), LJ_US_VF(gm_cue_number), LJ_US_VF(gm_editor_number), LJ_US_VF(mm_amp_pct), LJ_US_VF(mm_speed_pct),
    LJ_US_VF(mm_fade), LJ_US_VF(mm_frozen), LJ_US_VF(pending_genmacro) };
static const char *const lj_us_cl_states[4] = { "plain", "done", "next", "error" };   /* enum lj_cl_line_state order */
static int lj_us_cl_state(const char *s) { int i; for (i = 0; i < 4; i++) if (strcmp(lj_us_cl_states[i], s) == 0) return i; return -1; }
static int lj_us_in(const char *s, int lo, int hi) { int v; return lj_us_int(s, &v) == 0 && v >= lo && v <= hi; }
static const char *const lj_us_leds[LJ_LED_COUNT] = { "bgcue", "statics", "smoke" };   /* enum lj_desktop_led order */

static int lj_us_vfield(const char *name, size_t len)
{
    int i;
    for (i = 0; i < LJ_COUNT_OF(lj_us_vfields); i++) if (strlen(lj_us_vfields[i].name) == len && strncmp(lj_us_vfields[i].name, name, len) == 0) return i;
    return -1;
}
static int lj_us_u64(const char *s, unsigned long long *out) { char *end; if (!s || !*s || *s == '-') return -1; *out = strtoull(s, &end, 10); return *end ? -1 : 0; }
static int lj_us_hex(const char *s, int *out) { char *end; long v; if (!s || !*s) return -1; v = strtol(s, &end, 16); if (*end || v < 0 || v > 255) return -1; *out = (int)v; return 0; }
/* W5: a word of hex byte pairs ("FF0080") into out (NULL = count only); the byte count, -1 when malformed or past cap */
static int lj_us_hexrun(const char *s, uint8_t *out, int cap)
{
    int n = 0, hi, lo;
    for (; s && s[0]; s += 2, n++) {
        hi = s[0] >= '0' && s[0] <= '9' ? s[0] - '0' : (s[0] | 32) >= 'a' && (s[0] | 32) <= 'f' ? (s[0] | 32) - 'a' + 10 : -1;
        lo = !s[1] ? -1 : s[1] >= '0' && s[1] <= '9' ? s[1] - '0' : (s[1] | 32) >= 'a' && (s[1] | 32) <= 'f' ? (s[1] | 32) - 'a' + 10 : -1;
        if (hi < 0 || lo < 0) return -1;
        if (out) { if (n >= cap) return -1; out[n] = (uint8_t)(hi * 16 + lo); }
    }
    return n;
}
static int lj_us_led(const char *name) { int i; for (i = 0; i < LJ_LED_COUNT; i++) if (strcmp(lj_us_leds[i], name) == 0) return i; return -1; }
/* assert_view arguments as (field index, value) pairs: name=value words, or one bare "<field> <value>" pair; -1 = malformed */
static int lj_us_vpairs(const lj_us_stmt *st, int *field, unsigned long long *val)
{
    int i, n = 0;
    const char *eq;
    if (st->nargs == 2 && !strchr(st->arg[0], '=') && !strchr(st->arg[1], '=')) {
        if ((field[0] = lj_us_vfield(st->arg[0], strlen(st->arg[0]))) < 0 || lj_us_u64(st->arg[1], &val[0])) return -1;
        return 1;
    }
    for (i = 0; i < st->nargs; i++, n++) {
        if (!(eq = strchr(st->arg[i], '=')) || (field[n] = lj_us_vfield(st->arg[i], (size_t)(eq - st->arg[i]))) < 0 || lj_us_u64(eq + 1, &val[n])) return -1;
    }
    return n;
}

/* Static checks a line can fail without a window: numbers, 0|1, key names, l:a=v, exists|missing, Form.widget spelling. */
static const char *lj_us_check(const lj_us_stmt *st)
{
    int v, a, b, c;
    switch (st->verb) {
    case LJ_US_CLICK: case LJ_US_RCLICK: case LJ_US_DBLCLICK:
        if (!strchr(st->arg[0], '.')) return "expected <Form>.<widget>";
        if (st->nargs == 2) return "expected both x and y";
        if (st->nargs == 3 && (lj_us_int(st->arg[1], &a) || lj_us_int(st->arg[2], &b))) return "x y must be integers";
        break;
    case LJ_US_DRAG:
        if (!strchr(st->arg[0], '.') || !strchr(st->arg[1], '.')) return "expected <Form>.<widget> <Form>.<widget>";
        if (st->nargs == 3 || st->nargs == 5) return "expected [sx sy] dx dy";
        for (a = 2; a < st->nargs; a++) if (lj_us_int(st->arg[a], &b)) return "sx sy dx dy must be integers";
        break;
    case LJ_US_KEY:
        if (lj_us_vk(st->arg[0], &v, &a, &b, &c)) return "unknown key name";
        if (st->nargs == 2 && (lj_us_int(st->arg[1], &v) || v < 1)) return "key count must be a positive integer";
        break;
    case LJ_US_WAIT:
        if (lj_us_int(st->arg[0], &v) || v < 0) return "wait takes milliseconds";
        break;
    case LJ_US_ASSERT_TEXT: case LJ_US_ASSERT_ENABLED: case LJ_US_ASSERT_VISIBLE: case LJ_US_ASSERT_DOWN: case LJ_US_ASSERT_CHECKED:
        if (!strchr(st->arg[0], '.')) return "expected <Form>.<widget>";
        if (st->verb != LJ_US_ASSERT_TEXT && strcmp(st->arg[1], "0") != 0 && strcmp(st->arg[1], "1") != 0) return "expected 0 or 1";
        break;
    case LJ_US_ASSERT_FRAME:
        if (sscanf(st->arg[0], "%d:%d=%d", &a, &b, &c) != 3 || a < 1 || a > LJ_LINKS || b < 1 || b > LJ_LINK_CH || c < 0 || c > 255) return "expected l:a=v (link 1-4, address 1-512, value 0-255)";
        if (st->nargs == 2 && (lj_us_int(st->arg[1], &v) || v < 0)) return "tolerance must be a non-negative integer";
        break;
    case LJ_US_ASSERT_VIEW: {
        int fld[LJ_US_ARGS]; unsigned long long val[LJ_US_ARGS];
        if (lj_us_vpairs(st, fld, val) < 0) return "expected name=value pairs of a known view field (docs/ui-script.md)";
        break; }
    case LJ_US_MIDI:
        for (a = 0; a < 3; a++) if (lj_us_hex(st->arg[a], &v)) return "midi takes three hex bytes 00..FF";
        if (lj_us_hex(st->arg[0], &v) == 0 && v < 0x80) return "the first midi byte must be a status byte (80..FF)";
        break;
    case LJ_US_HOTKEY:
        if (lj_us_int(st->arg[0], &v) || v < 1 || v > 8) return "hotkey setup must be 1..8";
        if (lj_us_vk(st->arg[1], &v, &a, &b, &c)) return "unknown key name";
        break;
    case LJ_US_ASSERT_LED:
        if (lj_us_led(st->arg[0]) < 0) return "unknown led (bgcue statics smoke)";
        if (strcmp(st->arg[1], "0") != 0 && strcmp(st->arg[1], "1") != 0) return "expected 0 or 1";
        break;
    case LJ_US_SLEEP_UNTIL_TICK: {
        unsigned long long t;
        if (lj_us_u64(st->arg[0], &t)) return "sleep_until_tick takes a tick number";
        break; }
    case LJ_US_ASSERT_FILE:
        if (strcmp(st->arg[1], "exists") != 0 && strcmp(st->arg[1], "missing") != 0) return "expected exists or missing";
        break;
    case LJ_US_ASSERT_PPS:
        if (lj_us_int(st->arg[0], &a) || a < 1 || a > LJ_LINKS || lj_us_int(st->arg[1], &b) || lj_us_int(st->arg[2], &c)) return "expected <link 1-4> <min> <max>";
        break;
    case LJ_US_ASSERT_POS:
        if (lj_us_int(st->arg[0], &a) == 0) {                 /* W3: assert_pos <fixture> <pan> <tilt> */
            if (a < 1 || a > LJ_FIXTURES || !lj_us_in(st->arg[1], -32768, 32767) || !lj_us_in(st->arg[2], -32768, 32767)) return "expected <fixture 1-100> <pan> <tilt> (-32768..32767)";
            break;
        }
        /* fall through - the W1 <Form> <x> <y> form */
    case LJ_US_ASSERT_CLIENT: case LJ_US_MOVE:
        if (lj_us_int(st->arg[1], &a) || lj_us_int(st->arg[2], &b)) return "expected <Form> <x> <y>";
        break;
    case LJ_US_CL_GOTO:
        if (!lj_us_in(st->arg[0], 1, 1000000)) return "cl_goto takes a line number (1..)";
        break;
    case LJ_US_CL_ASSERT_LINE:
        if (!lj_us_in(st->arg[0], 1, 1000000)) return "cl_assert_line takes a line number (1..)";
        if (lj_us_cl_state(st->arg[1]) < 0) return "expected plain, done, next or error";
        break;
    case LJ_US_MM_SET:
        if (!lj_us_in(st->arg[0], 1, LJ_FIXTURES)) return "mm_set fixture must be 1..100";
        for (a = 1; a < 4; a++) if (!lj_us_in(st->arg[a], 0, 255)) return "mm_set shape, amp and speed must be 0..255";
        break;
    case LJ_US_SACN_IN: case LJ_US_ARTNET_IN:                  /* W5 */
        if (lj_us_int(st->arg[0], &v) || v < 1 || v > (st->verb == LJ_US_SACN_IN ? 63999 : 32767)) return "expected a universe (sACN 1..63999) / Port-Address (Art-Net 1..32767)";
        for (a = 1; a < st->nargs; a++) if (lj_us_hexrun(st->arg[a], NULL, 0) < 0) return "expected words of hex byte pairs (FF0080)";
        break;
    case LJ_US_AUDIO_INJECT:
        if (lj_us_int(st->arg[0], &v) || v < 0 || v > 32767) return "audio_inject level must be 0..32767";
        if (st->nargs == 2 && (lj_us_int(st->arg[1], &v) || v < 1 || v > 1000)) return "audio_inject blocks must be 1..1000";
        break;
    case LJ_US_REMOTE:
        for (a = 0; a < st->nargs; a++) if (!lj_us_in(st->arg[a], 0, 255)) return "remote takes decimal bytes 0..255";
        break;
    default: break;
    }
    return NULL;
}

static void lj_us_reset(void)
{
    free(lj_us.buf); free(lj_us.st);
    lj_us.buf = NULL; lj_us.st = NULL; lj_us.n = lj_us.pc = 0; lj_us.state = 0; lj_us.line = 0; lj_us.wait_until = 0; lj_us.tick_until = lj_us.tick_deadline = 0; lj_us.cur = NULL;
}

static int lj_us_load_fail(int line, const char *fmt, const char *a)
{
    char msg[256];
    snprintf(msg, sizeof msg, fmt, a);   /* a may point into the buffer the reset frees */
    lj_us_reset();
    lj_us_failf(line, "%s", msg);        /* after the reset so state = -1 and line survive */
    return -1;
}

int lj_uiscript_load(const char *path)
{
    FILE *f;
    long len;
    char *p, *line;
    int lineno = 0, cap = 64;

    lj_us_reset();
    f = path ? fopen(path, "rb") : NULL;
    if (!f) return lj_us_load_fail(0, "cannot read %s", path ? path : "(null)");
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return lj_us_load_fail(0, "cannot read %s", path); }
    lj_us.buf = (char *)malloc((size_t)len + 1);
    lj_us.st = (lj_us_stmt *)calloc((size_t)cap, sizeof *lj_us.st);
    if (!lj_us.buf || !lj_us.st) lj_fatal("uiscript: out of memory");
    if (fread(lj_us.buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); return lj_us_load_fail(0, "cannot read %s", path); }
    fclose(f);
    lj_us.buf[len] = 0;

    for (p = lj_us.buf; *p; ) {
        char *args[LJ_US_ARGS + 1];
        const char *why;
        lj_us_stmt *st;
        int n, v;
        line = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
        if (line[0] && line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = 0;
        lineno++;
        n = lj_us_tokens(line, args, LJ_US_ARGS + 1);
        if (n < 0) return lj_us_load_fail(lineno, "%s", "unterminated quote or too many words");
        if (n == 0) continue;
        for (v = 0; v < LJ_US_VERBS; v++) if (strcmp(args[0], lj_us_verbs[v].name) == 0) break;
        if (v == LJ_US_VERBS) return lj_us_load_fail(lineno, "unknown verb %s", args[0]);
        if (n - 1 < lj_us_verbs[v].min_args || n - 1 > lj_us_verbs[v].max_args) return lj_us_load_fail(lineno, "wrong argument count for %s", args[0]);
        if (lj_us.n == cap) { cap *= 2; lj_us.st = (lj_us_stmt *)realloc(lj_us.st, (size_t)cap * sizeof *lj_us.st); if (!lj_us.st) lj_fatal("uiscript: out of memory"); }
        st = &lj_us.st[lj_us.n];
        memset(st, 0, sizeof *st);
        st->verb = v; st->line = lineno; st->nargs = n - 1;
        memcpy(st->arg, args + 1, (size_t)(n - 1) * sizeof args[0]);
        why = lj_us_check(st);
        if (why) return lj_us_load_fail(lineno, "%s", why);
        lj_us.n++;
    }
    lj_us.state = 1;
    return 0;
}

int  lj_uiscript_active(void) { return lj_us.state == 1; }
int  lj_uiscript_line(void) { return lj_us.line; }
void lj_uiscript_set_host(const lj_uiscript_host *host) { if (host) lj_us.host = *host; else memset(&lj_us.host, 0, sizeof lj_us.host); }

/* ---- resolution ------------------------------------------------------------- */

static int lj_us_ctl_kind(const char *name)
{
    int k;
    for (k = 0; k < LJ_CTL_COUNT; k++) if (strcmp(lj_us_ctl_names[k], name) == 0) return k;
    return -1;
}

static int lj_us_win_id(const char *name)
{
    int id;
    for (id = 0; id < LJ_WIN_COUNT; id++)
        if (strcmp(lj_win_id_name((enum lj_win)id), name) == 0 || strcmp(lj_win_class((enum lj_win)id), name) == 0) return id;
    return -1;
}

lj_window *lj_uiscript_resolve_default(const char *form)
{
    int k = lj_us_ctl_kind(form);
    if (strcmp(form, "main") == 0) return lj_desktop_window();
    if (strcmp(form, "TUnassignedForm") == 0 || strcmp(form, "unassigned") == 0) return lj_unassigned_window();
    if (strcmp(form, "TConfirmForm") == 0 || strcmp(form, "confirm") == 0) return lj_win_message_window();   /* W2: the "Please Confirm.." / message box (Restore, Re-start prompts) */
    if (strcmp(form, "TExportConfigForm") == 0) return lj_fixture_config_export_window();                    /* ME-92: Fixture Configuration's export box */
    if (strcmp(form, "TSavePresetForm2") == 0) return lj_user_palettes_save_window();                        /* W4 LORD_FIDELITY-15: User Palette > Save */
    if (k >= 0) return lj_ctl_window_get((enum lj_ctl_kind)k);
    return lj_win_find(form);
}

/* The window `type` / `key` go to, re-validated: a dialog may have closed itself (AutoClose) since it was clicked. */
static lj_window *lj_us_cur(void)
{
    int id;
    if (!lj_us.cur) return NULL;
    if (lj_us.cur == lj_desktop_window() || lj_us.cur == lj_unassigned_window()) return lj_us.cur;
    for (id = 0; id < LJ_WIN_COUNT; id++) if (lj_win_get((enum lj_win)id) == lj_us.cur) return lj_us.cur;
    for (id = 0; id < LJ_CTL_COUNT; id++) if (lj_ctl_window_get((enum lj_ctl_kind)id) == lj_us.cur) return lj_us.cur;
    if (lj_us.host.resolve && lj_us.host.resolve(lj_us.host.ud, "TSequenceForm") == lj_us.cur) return lj_us.cur;   /* the app's W0 fallback forms */
    if (lj_us.host.resolve && lj_us.host.resolve(lj_us.host.ud, "TCurrentCue") == lj_us.cur) return lj_us.cur;
    if (lj_us.cur == lj_user_palettes_save_window()) return lj_us.cur;   /* ME-201: TSavePresetForm2 has no lj_win id; type / key reach its name field */
    lj_us.cur = NULL;
    return NULL;
}

static lj_window *lj_us_window(const char *form)
{
    lj_window *w = lj_us.host.resolve ? lj_us.host.resolve(lj_us.host.ud, form) : lj_uiscript_resolve_default(form);
    if (!w) lj_us_failf(lj_us.line, "window %s is not open", form);
    return w;
}

/* "<Form>.<widget>[.r.c]" -> window + widget (+ TStringGrid cell, Delphi's 0-based Cells[c, r]); the spec is split at its dots in place and restored. */
static lj_widget *lj_us_widget(char *spec, lj_window **win, int *row, int *col)
{
    char *dot = strchr(spec, '.'), *name, *rc;
    lj_widget *w;
    *dot = 0; name = dot + 1;
    *win = lj_us_window(spec);
    *dot = '.';
    if (!*win) return NULL;
    rc = strchr(name, '.');
    if (rc) *rc = 0;
    w = lj_window_find(*win, name);
    if (!w) w = lj_ctl_elem_find(*win, name);          /* control rows: row<ch>_bar, row<ch>_fade ... (lj_ctl_common.h) */
    if (rc) *rc = '.';
    if (!w) { lj_us_failf(lj_us.line, "no widget %s", spec); return NULL; }
    if (row) { *row = *col = -1; if (rc && sscanf(rc + 1, "%d.%d", row, col) != 2) { lj_us_failf(lj_us.line, "expected %s", "<Form>.<grid>.<row>.<col>"); return NULL; } }
    return w;
}

static const char *const lj_us_sb_names[LJ_SB_COUNT] = { "sb_clock", "sb_running", "sb_memory", "sb_master", "sb_seqmod", "sb_cuemod", "sb_clmod", "sb_trig" };   /* docs/ui-script.md: the desktop's panels are not widgets */

static const char *lj_us_text_of(lj_widget *w, int row, int col)
{
    static char buf[96];
    const char *t;
    if (row >= 0) {
        lj_w_grid *g = (lj_w_grid *)w;
        if (w->name && strcmp(w->name, "lj_cue_grid") == 0) return lj_cue_form_cell(row - 1, col, buf, sizeof buf) == 0 ? buf : "";   /* the cue grid: row = slot 1..12, col = 0 No .. 8 Trig */
        if (w->kind == LJ_K_LISTVIEW) {                   /* a TListView row: 0-based row, '	'-separated sub-item col */
            lj_w_listview *v = (lj_w_listview *)w;
            const char *p = row < v->n && v->items ? v->items[row] : "", *e;
            int c;
            for (c = 0; c < col && p; c++) p = (e = strchr(p, '	')) ? e + 1 : NULL;
            if (!p) return "";
            e = strchr(p, '	');
            snprintf(buf, sizeof buf, "%.*s", e ? (int)(e - p) : (int)strlen(p), p);
            return buf;
        }
        if (w->kind != LJ_K_STRINGGRID) return NULL;
        if (row >= g->rows || col >= g->cols || !g->cells) return "";
        t = g->cells[row * g->cols + col];
        return t ? t : "";
    }
    if (w->name && strncmp(w->name, "icon", 4) == 0 && lj_window_of(w) == lj_desktop_window()) {   /* a fixture tile paints "(n) user name" */
        lj_w_fixicon *f = (lj_w_fixicon *)w;
        snprintf(buf, sizeof buf, "(%d) %s", f->number, f->user_name ? f->user_name : "");
        return buf;
    }
    if (w->kind == LJ_K_STATUSBAR && lj_window_of(w) == lj_desktop_window()) return lj_statusbar_panel_text(LJ_SB_CLOCK);   /* the desktop's own status bar struct (sb_* names for the panels) */
    if (w->kind == LJ_K_STATUSBAR) { lj_w_statusbar *s = (lj_w_statusbar *)w; t = s->text[0]; return t ? t : (s->n && s->panels ? s->panels[0].caption : ""); }
    if (w->kind == LJ_K_COMBOBOX) { lj_w_combobox *c = (lj_w_combobox *)w; return c->index >= 0 && c->index < c->n && c->items ? c->items[c->index] : c->text ? c->text : ""; }   /* W5 integrator 2: the shown item */
    if (w->kind == LJ_K_SPINEDIT) { snprintf(buf, sizeof buf, "%d", lj_w_spinedit_value(w)); return buf; }   /* W5 integrator 2 */
    if (w->kind == LJ_K_TRACKBAR) { snprintf(buf, sizeof buf, "%d", ((lj_w_trackbar *)w)->pos); return buf; }   /* W5 integrator 2: Position */
    t = lj_w_edit_text(w);
    if (!t) t = lj_w_caption(w);
    return t;
}

/* ---- events ----------------------------------------------------------------- */

static void lj_us_mouse(lj_window *win, int type, int button, int clicks, int x, int y)
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.x = x; ev.u.mouse.y = y; ev.u.mouse.button = button; ev.u.mouse.clicks = clicks;
    lj_window_event(win, &ev);
}

/* Surface point of a widget: its centre, or arg[first], arg[first + 1] relative to its top-left when given.
 * lj_widget_abs already includes the root's own offset inside the surface (lj_window.c's mouse() subtracts it the same way). */
static void lj_us_point(lj_widget *w, const lj_us_stmt *st, int first, int *sx, int *sy)
{
    int ax, ay, x, y;
    lj_widget_abs(w, &ax, &ay);
    if (st->nargs > first + 1 && lj_us_int(st->arg[first], &x) == 0 && lj_us_int(st->arg[first + 1], &y) == 0) { *sx = ax + x; *sy = ay + y; }
    else { *sx = ax + w->w / 2; *sy = ay + w->h / 2; }
}

static int lj_us_screen(lj_window *win, int *x, int *y)
{
    struct lj_menu *m = lj_window_menus(win);
    *x = *y = 0;
    return (m && m->host && m->host->screen_pos) ? m->host->screen_pos(m->host->ud, win, x, y) : -1;
}

static void lj_us_close_result(int yes, void *ud) { (void)ud; if (yes) lj_desktop_run_action(LJ_ACT_CLOSE, 0); }   /* close main: Yes = the script's quit path */

/* <Form>.<item> names a menu item (TDMXOutForm.M_EnableOverwrite): fired as lj_menu's fire() does, without the bar. */
static int lj_us_menu_click(char *spec)
{
    char *dot = strchr(spec, '.');
    lj_window *win;
    lj_menu *m;
    lj_menu_rt *rt;
    *dot = 0; win = lj_us.host.resolve ? lj_us.host.resolve(lj_us.host.ud, spec) : lj_uiscript_resolve_default(spec); *dot = '.';
    m = win ? lj_window_menus(win) : NULL;
    rt = m ? lj_menu_rt_named(m, dot + 1) : NULL;
    if (!rt || !rt->fn || !rt->enabled) return -1;
    if (lj_desc_bool(rt->item->auto_check, 0)) rt->checked = rt->checked ? 0u : 1u;
    lj_menu_close(m);   /* as fire() does: an open popup (rclick) would otherwise eat the next click on the window (W4 acceptance) */
    m->firing = rt->item;
    rt->fn(lj_window_root(win), rt->ud);
    m->firing = NULL;
    lj_us.cur = win;
    return 0;
}

/* <Form>.<item> names a menu item (main or popup) and no widget: its caption (the TAction's, resolved by dfm2c), for assert_text (W2: the Statics 2 popup's duplicated items). */
static lj_menu_rt *lj_us_menu_rt(char *spec)   /* <Form>.<menu item> when no widget of that name is drawn */
{
    char *dot = strchr(spec, '.');
    lj_window *win;
    lj_menu *m;
    *dot = 0; win = lj_us.host.resolve ? lj_us.host.resolve(lj_us.host.ud, spec) : lj_uiscript_resolve_default(spec); *dot = '.';
    if (!win || strchr(dot + 1, '.')) return NULL;
    m = lj_window_menus(win);
    return m ? lj_menu_rt_named(m, dot + 1) : NULL;
}

static const char *lj_us_menu_caption(char *spec)
{
    char *dot = strchr(spec, '.');
    lj_window *win;
    lj_menu *m;
    lj_menu_rt *rt;
    *dot = 0; win = lj_us.host.resolve ? lj_us.host.resolve(lj_us.host.ud, spec) : lj_uiscript_resolve_default(spec); *dot = '.';
    if (!win || strchr(dot + 1, '.') || lj_window_find(win, dot + 1)) return NULL;
    m = lj_window_menus(win);
    rt = m ? lj_menu_rt_named(m, dot + 1) : NULL;
    return rt && rt->item ? rt->item->caption : NULL;
}

static int lj_us_click(const lj_us_stmt *st, int button, int dbl)
{
    lj_window *win;
    int sx, sy;
    lj_widget *w;
    if (button == 1 && !dbl && st->nargs == 1 && lj_us_menu_click(st->arg[0]) == 0) return 0;
    w = lj_us_widget(st->arg[0], &win, NULL, NULL);
    if (!w) return -1;
    lj_us_point(w, st, 1, &sx, &sy);
    lj_us.cur = win;
    lj_us_mouse(win, LJ_EV_MOUSE_MOVE, 0, 0, sx, sy);
    lj_us_mouse(win, LJ_EV_MOUSE_DOWN, button, 1, sx, sy);
    lj_us_mouse(win, LJ_EV_MOUSE_UP, button, 1, sx, sy);
    if (dbl) { lj_us_mouse(win, LJ_EV_MOUSE_DOWN, button, 2, sx, sy); lj_us_mouse(win, LJ_EV_MOUSE_UP, button, 2, sx, sy); }
    return 0;
}

static int lj_us_drag(const lj_us_stmt *st)
{
    lj_window *src, *dst;
    lj_widget *ws, *wd;
    int sx, sy, dx, dy, ox = 0, oy = 0, sxs, sys, dxs, dys;
    if (!(ws = lj_us_widget(st->arg[0], &src, NULL, NULL)) || !(wd = lj_us_widget(st->arg[1], &dst, NULL, NULL))) return -1;
    lj_us_point(ws, st, st->nargs == 6 ? 2 : LJ_US_ARGS, &sx, &sy);     /* centre, or sx sy when six words; the last two numbers offset the drop */
    lj_us_point(wd, st, LJ_US_ARGS, &dx, &dy);
    if (st->nargs >= 4) { lj_us_int(st->arg[st->nargs - 2], &ox); lj_us_int(st->arg[st->nargs - 1], &oy); }
    dx += ox; dy += oy;
    if (src != dst) {                                   /* the drop point in the source window's surface coordinates; lj_window.c maps it back through host->window_at */
        if (lj_us_screen(src, &sxs, &sys) != 0 || lj_us_screen(dst, &dxs, &dys) != 0) { lj_us_failf(lj_us.line, "%s", "drag across windows needs screen positions (not headless)"); return -1; }
        dx += dxs - sxs; dy += dys - sys;
    }
    lj_us.cur = src;
    lj_us_mouse(src, LJ_EV_MOUSE_MOVE, 0, 0, sx, sy);
    lj_us_mouse(src, LJ_EV_MOUSE_DOWN, 1, 1, sx, sy);
    lj_us_mouse(src, LJ_EV_MOUSE_MOVE, 0, 0, sx + 4, sy + 4);
    lj_us_mouse(src, LJ_EV_MOUSE_MOVE, 0, 0, dx, dy);
    lj_us_mouse(src, LJ_EV_MOUSE_UP, 1, 1, dx, dy);
    return 0;
}

static int lj_us_key(const lj_us_stmt *st)
{
    lj_ui_event ev;
    int vk, s, c, a, n = 1;
    if (!lj_us_cur()) { lj_us_failf(lj_us.line, "%s", "no window to send the key to (open or click one first)"); return -1; }
    lj_us_vk(st->arg[0], &vk, &s, &c, &a);
    if (st->nargs == 2) lj_us_int(st->arg[1], &n);
    memset(&ev, 0, sizeof ev);
    while (n-- > 0 && lj_us_cur()) {
        ev.type = LJ_EV_KEY_DOWN; ev.u.key.type = LJ_EV_KEY_DOWN; ev.u.key.key = vk; ev.u.key.shift = s ? 1u : 0u; ev.u.key.ctrl = c ? 1u : 0u; ev.u.key.alt = a ? 1u : 0u;
        lj_window_event(lj_us.cur, &ev);
        if (!lj_us_cur()) break;   /* the key closed its window (Save on [Enter] with Auto Close): no key-up to a freed window (ASan, CI run 40061418618) */
        ev.type = LJ_EV_KEY_UP; ev.u.key.type = LJ_EV_KEY_UP;
        lj_window_event(lj_us.cur, &ev);
    }
    return 0;
}

static int lj_us_type(const lj_us_stmt *st)
{
    lj_ui_event ev;
    const unsigned char *p;
    if (!lj_us_cur()) { lj_us_failf(lj_us.line, "%s", "no window to type into (open or click one first)"); return -1; }
    memset(&ev, 0, sizeof ev);
    ev.type = LJ_EV_CHAR; ev.u.key.type = LJ_EV_CHAR;
    for (p = (const unsigned char *)st->arg[0]; *p && lj_us_cur(); p++) { ev.u.key.ch = *p; lj_window_event(lj_us.cur, &ev); }   /* a character may close its window too */
    return 0;
}

/* ---- shot ---------------------------------------------------------------------- */

static int lj_us_png(const lj_surface *s, const char *path)
{
    unsigned char *rgb = (unsigned char *)malloc((size_t)s->w * (size_t)s->h * 3);
    void *png;
    size_t len = 0, i, n = (size_t)s->w * (size_t)s->h;
    FILE *f;
    if (!rgb) lj_fatal("uiscript: out of memory");
    for (i = 0; i < n; i++) { rgb[i * 3] = (unsigned char)(s->px[i] >> 16); rgb[i * 3 + 1] = (unsigned char)(s->px[i] >> 8); rgb[i * 3 + 2] = (unsigned char)s->px[i]; }
    png = tdefl_write_image_to_png_file_in_memory_ex(rgb, s->w, s->h, 3, &len, 6, MZ_FALSE);
    free(rgb);
    if (!png) { lj_log("shot: png encode failed for %s", path); return -1; }
    f = fopen(path, "wb");
    if (!f || fwrite(png, 1, len, f) != len) { lj_log("shot: cannot write %s", path); if (f) fclose(f); mz_free(png); return -1; }
    fclose(f);
    mz_free(png);
    return 0;
}

static int lj_us_shot_one(lj_window *win, const char *stem, const char *ext, int *n)
{
    char path[LJ_PATH_CAP];
    const lj_form_desc *d = lj_window_desc(win);
    const lj_surface *s = lj_window_surface(win);
    int rc;
    if (lj_win_host_get() && !lj_window_native(win)) return 0;   /* ME-68: a hosted window with no native handle (closed by AutoClose, not yet unregistered) is not a capture; headless tests have no host */
    if (!s || s->w == 0 || s->h == 0) { lj_log("shot: %s has a %dx%d surface", d && d->class_name ? d->class_name : "window", s ? s->w : 0, s ? s->h : 0); return -1; }
    if (snprintf(path, sizeof path, "%s-%s%s", stem, d && d->class_name ? d->class_name : "window", ext) >= (int)sizeof path) return -1;
    lj_window_paint(win);
    rc = strcmp(ext, ".bmp") == 0 ? lj_bmp_write(s, path) : lj_us_png(s, path);
    if (rc == 0) (*n)++;
    return rc;
}

static int lj_us_shot(const char *arg)
{
    char stem[LJ_PATH_CAP];
    const char *ext = ".png";
    size_t len = strlen(arg);
    lj_window *w;
    int id, n = 0, rc = 0;
    if (len >= sizeof stem) { lj_us_failf(lj_us.line, "%s", "path too long"); return -1; }
    lj_str_copy(stem, sizeof stem, arg);
    if (len > 4 && (strcmp(arg + len - 4, ".png") == 0 || strcmp(arg + len - 4, ".bmp") == 0)) { ext = arg + len - 4; stem[len - 4] = 0; }
    if ((w = lj_desktop_window()) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);
    for (id = 0; id < LJ_WIN_COUNT; id++) if ((w = lj_win_get((enum lj_win)id)) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);
    for (id = 0; id < LJ_CTL_COUNT; id++) if ((w = lj_ctl_window_get((enum lj_ctl_kind)id)) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);
    if ((w = lj_unassigned_window()) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);
    if ((w = lj_fixture_config_export_window()) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);   /* ME-92: no lj_win id of its own */
    if ((w = lj_user_palettes_save_window()) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);      /* W4 LORD_FIDELITY-15: TSavePresetForm2, no lj_win id either */
    if ((w = lj_win_message_window()) != NULL) rc |= lj_us_shot_one(w, stem, ext, &n);   /* the open confirm / message box (assert_prompt's window) */
    if (lj_us_cur() && lj_us.cur != lj_desktop_window() && lj_us.cur != lj_unassigned_window() && lj_us.cur != lj_win_message_window()) {   /* a host-resolved window the lists do not know is still captured once */
        for (id = 0; id < LJ_WIN_COUNT; id++) if (lj_win_get((enum lj_win)id) == lj_us.cur) break;
        if (id == LJ_WIN_COUNT) { for (id = 0; id < LJ_CTL_COUNT; id++) if (lj_ctl_window_get((enum lj_ctl_kind)id) == lj_us.cur) break; if (id == LJ_CTL_COUNT) rc |= lj_us_shot_one(lj_us.cur, stem, ext, &n); }
    }
    lj_log("shot: %d window(s) -> %s-<Class>%s", n, stem, ext);
    if (rc != 0 || n == 0) { lj_us_failf(lj_us.line, "%s", n == 0 ? "no window open to capture" : "could not write every capture"); return -1; }
    return 0;
}

/* ---- asserts -------------------------------------------------------------------- */

static int lj_us_assert_eq(const char *what, const char *got, const char *want)
{
    char msg[600];
    if (got && strcmp(got, want) == 0) return 0;
    snprintf(msg, sizeof msg, "%s is \"%s\", expected \"%s\"", what, got ? got : "(none)", want);
    lj_us_failf(lj_us.line, "%s", msg);
    return -1;
}

static int lj_us_assert_frame(const lj_us_stmt *st)
{
    lj_engine *e = lj_ctx_get()->engine;
    int l, a, v, tol = 0, got, d;
    char msg[96];
    sscanf(st->arg[0], "%d:%d=%d", &l, &a, &v);
    if (st->nargs == 2) lj_us_int(st->arg[1], &tol);
    if (!e) { lj_us_failf(lj_us.line, "%s", "no engine"); return -1; }
    lj_lock(); got = lj_engine_frame(e)->out[l - 1][a - 1]; lj_unlock();
    d = got - v;
    if (d < 0) d = -d;
    if (d <= tol) return 0;
    snprintf(msg, sizeof msg, "%d:%d is %d, expected %d (tolerance %d)", l, a, got, v, tol);
    lj_us_failf(lj_us.line, "%s", msg);
    return -1;
}

static const lj_engine_status *lj_us_view(void)
{
    const lj_engine_status *v = lj_us.host.view ? lj_us.host.view(lj_us.host.ud) : NULL;
    if (!v) lj_us_failf(lj_us.line, "%s", "no engine view (headless)");
    return v;
}

static int lj_us_assert_view(const lj_us_stmt *st)
{
    const lj_engine_status *v = lj_us_view();
    int fld[LJ_US_ARGS], i, n;
    unsigned long long want[LJ_US_ARGS], got;
    char msg[128];
    if (!v) return -1;
    n = lj_us_vpairs(st, fld, want);
    for (i = 0; i < n; i++) {
        uint8_t b1; uint16_t b2; uint32_t b4; uint64_t b8;
        const char *p = (const char *)v + lj_us_vfields[fld[i]].off;
        switch (lj_us_vfields[fld[i]].size) {
        case 1: memcpy(&b1, p, 1); got = b1; break;
        case 2: memcpy(&b2, p, 2); got = b2; break;
        case 4: memcpy(&b4, p, 4); got = b4; break;
        default: memcpy(&b8, p, 8); got = b8; break;
        }
        if (got != want[i]) { snprintf(msg, sizeof msg, "%s is %llu, expected %llu", lj_us_vfields[fld[i]].name, got, want[i]); lj_us_failf(lj_us.line, "%s", msg); return -1; }
    }
    return 0;
}

static int lj_us_assert_pps(const lj_us_stmt *st)
{
    const lj_engine_status *v = lj_us_view();
    char msg[96];
    int link, lo, hi, got;
    if (!v) return -1;
    lj_us_int(st->arg[0], &link); lj_us_int(st->arg[1], &lo); lj_us_int(st->arg[2], &hi);
    got = (int)v->sacn_pps[link - 1];
    if (got >= lo && got <= hi) return 0;
    snprintf(msg, sizeof msg, "link %d sends %d packets/s, expected %d..%d", link, got, lo, hi);
    lj_us_failf(lj_us.line, "%s", msg);
    return -1;
}

/* ---- W2 verbs (lj_uiscript.h, docs/ui-script.md "W2 verbs") ---- */

static FILE *lj_us_open_lib(const char *rel, char *path, size_t cap)   /* the path as given, else under the user library (assert_file's rule) */
{
    FILE *f = fopen(rel, "rb");
    if (!f && lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, rel, path, cap) == 0) f = fopen(path, "rb");
    return f;
}

static int lj_us_assert_file_line(const lj_us_stmt *st)
{
    char path[LJ_PATH_CAP], line[1024], msg[160];
    FILE *f = lj_us_open_lib(st->arg[0], path, sizeof path);
    int hits = 0;
    size_t n;
    if (!f) { lj_us_failf(lj_us.line, "%s is missing", st->arg[0]); return -1; }
    while (fgets(line, sizeof line, f)) {
        n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (strcmp(line, st->arg[1]) == 0) hits++;
    }
    fclose(f);
    if (hits == 1) return 0;
    snprintf(msg, sizeof msg, "%s holds the line %d time(s), expected once", st->arg[0], hits);
    lj_us_failf(lj_us.line, "%s", msg);
    return -1;
}

static int lj_us_exec_w2(lj_us_stmt *st)
{
    int i, v, s, c, a, setup;
    unsigned long long t;
    uint8_t m[3];
    lj_key_ev ev;
    switch (st->verb) {
    case LJ_US_MIDI:                                          /* the same post path as a device; the next idle pass dispatches it on the UI thread */
        for (i = 0; i < 3; i++) { lj_us_hex(st->arg[i], &v); m[i] = (uint8_t)v; }
        lj_midi_inject(m);
        return 0;
    case LJ_US_HOTKEY:
        lj_us_int(st->arg[0], &setup);
        lj_desktop_run_action(LJ_ACT_HOTKEY_SETUP, setup);
        lj_us_vk(st->arg[1], &v, &s, &c, &a);
        memset(&ev, 0, sizeof ev);
        ev.key = v; ev.shift = s ? 1u : 0u; ev.ctrl = c ? 1u : 0u; ev.alt = a ? 1u : 0u;
        if (!lj_app_key_passes(lj_us_cur())) return 0;           /* ME-182: the Analyzer / MediaPlayer with its pass flag off keeps the key */
        ev.type = LJ_EV_KEY_DOWN; lj_desktop_key(&ev, 0);    /* as with the Sequence window focused: no dialog gate */
        ev.type = LJ_EV_KEY_UP;   lj_desktop_key(&ev, 0);
        return 0;
    case LJ_US_ASSERT_LED:
        return lj_us_assert_eq(st->arg[0], lj_desktop_led_get((enum lj_desktop_led)lj_us_led(st->arg[0])) ? "1" : "0", st->arg[1]);
    case LJ_US_ASSERT_FILE_LINE:
        return lj_us_assert_file_line(st);
    case LJ_US_SLEEP_UNTIL_TICK:
        lj_us_u64(st->arg[0], &t);
        if (!lj_us_view()) return -1;
        lj_us.tick_until = t; lj_us.tick_deadline = lj_clock_us() + 10000000u;   /* 10 s: a stopped engine fails the line instead of hanging the run */
        return 0;
    default:
        return -1;
    }
}

/* ---- W3 verbs (lj_uiscript.h, docs/ui-script.md "W3 verbs") ---- */

static int lj_us_path(const char *rel, char *path, size_t cap)   /* as given, else under the user library (assert_file's rule) */
{
    FILE *f = fopen(rel, "rb");
    if (f) { fclose(f); if (lj_str_copy(path, cap, rel) == 0) return 0; }
    else if (lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, rel, path, cap) == 0 && (f = fopen(path, "rb")) != NULL) { fclose(f); return 0; }
    lj_us_failf(lj_us.line, "%s is missing", rel);
    return -1;
}

/* assert_pos <fixture> <pan> <tilt>: the readout (17 s1.3) of the fixture's pan / tilt bytes in the engine's frame (out[], after S7) */
static int lj_us_assert_fixture_pos(const lj_us_stmt *st)
{
    lj_ctx *c = lj_ctx_get();
    const lj_profile *p;
    uint8_t ofs[4], b[4];
    int f, want_p, want_t, base, i, ch;
    int32_t pan, tilt;
    char msg[128];
    lj_us_int(st->arg[0], &f); lj_us_int(st->arg[1], &want_p); lj_us_int(st->arg[2], &want_t);
    if (!c->show || !c->engine) { lj_us_failf(lj_us.line, "%s", "no show or engine"); return -1; }
    p = lj_show_profile(c->show, f - 1);
    base = lj_show_fixture_base(c->show, f - 1);
    if (!p || base < 0 || !lj_profile_window(p, LJ_CTL_MOVEMENT)) { snprintf(msg, sizeof msg, "fixture %d has no pan / tilt", f); lj_us_failf(lj_us.line, "%s", msg); return -1; }
    if (p->pt16) lj_pos_patch16(p, ofs);
    else { ofs[0] = ofs[1] = p->pan_ofs; ofs[2] = ofs[3] = p->tilt_ofs; }
    lj_lock();
    for (i = 0; i < 4; i++) { ch = base + ofs[i]; b[i] = ch < LJ_CHANS ? lj_engine_frame(c->engine)->out[ch / LJ_LINK_CH][ch % LJ_LINK_CH] : 0; }
    lj_unlock();
    pan = p->pt16 ? lj_pos_from_dmx16((uint16_t)((b[0] << 8) | b[1])) : lj_pos_from_dmx8(b[0]);
    tilt = p->pt16 ? lj_pos_from_dmx16((uint16_t)((b[2] << 8) | b[3])) : lj_pos_from_dmx8(b[2]);
    if (pan == want_p && tilt == want_t) return 0;
    snprintf(msg, sizeof msg, "fixture %d is at %ld : %ld, expected %d : %d", f, (long)pan, (long)tilt, want_p, want_t);
    lj_us_failf(lj_us.line, "%s", msg);
    return -1;
}

static int lj_us_exec_w3(lj_us_stmt *st)
{
    lj_ctx *c = lj_ctx_get();
    char path[LJ_PATH_CAP], err[160], msg[600];
    lj_window *win;
    lj_widget *w;
    int n, got, f, shape, amp, speed, k;
    switch (st->verb) {
    case LJ_US_CL_LOAD:
        if (lj_us_path(st->arg[0], path, sizeof path) != 0) return -1;
        if (lj_app_load_cuelist_path(path, 0) != 0) { lj_us_failf(lj_us.line, "cue list %s did not load (see the log)", st->arg[0]); return -1; }
        return 0;
    case LJ_US_CL_GOTO:                                    /* waiting for Go: Jump to this; otherwise a click on the row of the Cuelist window */
        lj_us_int(st->arg[0], &n);
        if (c->cl && c->cl->state == LJ_CL_WAIT_GO) { lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(LJ_CL_OP_JUMP, n)); return 0; }
        if (!c->show || !c->show->current_cl || (uint32_t)n > c->show->current_cl->n) { lj_us_failf(lj_us.line, "the cue list has no line %s", st->arg[0]); return -1; }
        if (!(win = lj_win_get(LJ_WIN_CUELIST_FORM)) || !(w = lj_window_find(win, "lv1"))) { lj_us_failf(lj_us.line, "%s", "the Cuelist window is not open"); return -1; }
        lj_w_listview_select(w, n - 1);
        lj_us.cur = win;
        return 0;
    case LJ_US_CL_EXECUTE:
        lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(LJ_CL_OP_GO, 0));
        return 0;
    case LJ_US_CL_ASSERT_LINE:
        lj_us_int(st->arg[0], &n);
        if (!c->cl || !c->show || !c->show->current_cl) { lj_us_failf(lj_us.line, "%s", "no cue list"); return -1; }
        got = lj_cl_line_state(c->cl, c->show->current_cl, (uint32_t)n);
        snprintf(msg, sizeof msg, "line %d state", n);
        return lj_us_assert_eq(msg, got >= 0 && got < 4 ? lj_us_cl_states[got] : "?", st->arg[1]);
    case LJ_US_GS2_IMPORT:
        if (lj_us_path(st->arg[0], path, sizeof path) != 0) return -1;
        if (lj_app_gs2_import(path, err, sizeof err) != 0) { lj_us_failf(lj_us.line, "%s", err); return -1; }
        return 0;
    case LJ_US_ASSERT_PROMPT: {
        const char *l1, *l2, *cap;
        if (!(win = lj_win_message_window())) { lj_us_failf(lj_us.line, "%s", "no prompt is open"); return -1; }
        l1 = lj_w_caption(lj_window_find(win, "Label1")); l2 = lj_w_caption(lj_window_find(win, "Label2")); cap = lj_window_caption(win)->text;
        l1 = l1 ? l1 : ""; l2 = l2 ? l2 : "";
        snprintf(msg, sizeof msg, "%s %s", l1, l2);
        if (strcmp(st->arg[0], l1) == 0 || strcmp(st->arg[0], l2) == 0 || strcmp(st->arg[0], msg) == 0 || (cap && strcmp(st->arg[0], cap) == 0)) return 0;
        snprintf(msg, sizeof msg, "the prompt shows \"%s\" / \"%s\", expected \"%s\"", l1, l2, st->arg[0]);
        lj_us_failf(lj_us.line, "%s", msg);
        return -1; }
    case LJ_US_MM_SET:                                     /* what the Movement Macro window does: the current scene, under the lock, then CURRENT_SEQ */
        lj_us_int(st->arg[0], &f); lj_us_int(st->arg[1], &shape); lj_us_int(st->arg[2], &amp); lj_us_int(st->arg[3], &speed);
        if (!c->show || !c->show->current_seq || c->current_scene >= c->show->current_seq->n) { lj_us_failf(lj_us.line, "%s", "no current scene"); return -1; }
        lj_lock();
        {   lj_movemacro *m = &c->show->current_seq->scenes[c->current_scene].macro[f - 1];
            m->type = (uint8_t)shape; m->amp_pan = m->amp_tilt = (uint8_t)amp; m->cycle_pan = m->cycle_tilt = (uint8_t)speed;
            for (k = 0; k < 6; k++) m->fstate[k] = LJ_SNAP; }
        lj_unlock();
        lj_ctx_set_modified(LJ_MOD_SEQ, 1);
        lj_ctx_set_scene(c->current_scene);
        return 0;
    default:
        return -1;
    }
}

/* ---- W4: dump_captions <path> (W4-CAP): the runtime Caption / Text / Hint of every widget and the Caption / Hint of
 * every menu item of each open window, one "Class<TAB>object<TAB>property<TAB>value" line each (CP1252), appended to
 * path; tools/caption_diff.py compares them with 13-dfm-captions.tsv. ---- */
static void lj_us_dump_widget(FILE *f, const char *form, const lj_widget *w)
{
    const lj_widget *c;
    const char *t;
    if (w->desc && w->name) {
        if ((t = lj_w_caption(w)) != NULL) fprintf(f, "%s\t%s\tCaption\t%s\n", form, w->name, t);
        if ((t = lj_w_edit_text(w)) != NULL) fprintf(f, "%s\t%s\tText\t%s\n", form, w->name, t);
        if (w->hint) fprintf(f, "%s\t%s\tHint\t%s\n", form, w->name, w->hint);
    }
    for (c = w->first; c; c = c->next) lj_us_dump_widget(f, form, c);
}

static int lj_us_dump_captions(const char *path)
{
    FILE *f = fopen(path, "ab");
    int id, i;
    if (!f) { lj_us_failf(lj_us.line, "dump_captions: cannot write %s", path); return -1; }
    for (id = 0; id < LJ_WIN_COUNT; id++) {
        lj_window *win = lj_win_get((enum lj_win)id);
        const lj_form_desc *d = win ? lj_window_desc(win) : NULL;
        lj_menu *m;
        if (!d || !d->class_name) continue;
        fprintf(f, "%s\t%s\tCaption\t%s\n", d->class_name, d->name ? d->name : "", lj_window_caption(win)->text);
        lj_us_dump_widget(f, d->class_name, lj_window_root(win));
        if ((m = lj_window_menus(win)) != NULL)
            for (i = 0; i < m->nrt; i++) {
                const lj_menu_item_desc *it = m->rt[i].item;
                if (!it || !it->name) continue;
                if (it->caption) fprintf(f, "%s\t%s\tCaption\t%s\n", d->class_name, it->name, it->caption);
                if (it->hint) fprintf(f, "%s\t%s\tHint\t%s\n", d->class_name, it->name, it->hint);
            }
    }
    fclose(f);
    return 0;
}

/* ---- step ------------------------------------------------------------------------- */

/* ---- W5 verbs (docs/ui-script.md "W5 verbs"): synthetic input through the same entry points as the wire ---- */
static int lj_us_exec_w5(const lj_us_stmt *st)
{
    static const uint8_t cid[16] = { 0x5C, 0x21, 0x9E, 0x10, 0x4A, 0x77, 0x4B, 0x3D, 0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };   /* the script's own source, never ours (our CID is ignored by the receiver) */
    static uint8_t slots[LJ_LINK_CH], pkt[LJ_SACN_DATA_LEN];
    lj_ctx *c = lj_ctx_get();
    int i, n = 0, k, v, blocks = 1, len;
    uint8_t bytes[LJ_US_ARGS];
    memset(slots, 0, sizeof slots);
    switch (st->verb) {
    case LJ_US_SACN_IN: case LJ_US_ARTNET_IN:
        lj_us_int(st->arg[0], &v);
        for (i = 1; i < st->nargs; i++) { k = lj_us_hexrun(st->arg[i], slots + n, LJ_LINK_CH - n); if (k < 0) { lj_us_failf(lj_us.line, "%s", "more than 512 slots"); return -1; } n += k; }
        if (st->verb == LJ_US_SACN_IN) {
            lj_sacn_cfg cfg;
            lj_sacn_cfg_default(&cfg, "script");
            memcpy(cfg.cid, cid, sizeof cid);
            len = lj_sacn_pack_data(pkt, &cfg, (uint16_t)v, (uint8_t)lj_us.line, 0, slots) == 0 ? LJ_SACN_DATA_LEN : -1;
        } else len = lj_artnet_pack_dmx(pkt, (uint8_t)(lj_us.line % 255 + 1), 0, (uint16_t)v, slots);
        if (len < 0) { lj_us_failf(lj_us.line, "%s: cannot build the packet", st->verb == LJ_US_SACN_IN ? "sacn_in" : "artnet_in"); return -1; }
        if (lj_dmxin_rx(st->verb == LJ_US_SACN_IN ? LJ_DMXIN_SACN : LJ_DMXIN_ARTNET, pkt, (size_t)len, 0x7F000002u, lj_clock_us()) != 0) { lj_us_failf(lj_us.line, "%s", "packet discarded by the DMX-in receiver (DMX-in off or another universe)"); return -1; }
        return 0;   /* the next idle pass's lj_dmxin_poll merges it and posts LJ_CMD_DMX_IN */
    case LJ_US_AUDIO_INJECT:
        lj_us_int(st->arg[0], &v);
        if (st->nargs == 2) lj_us_int(st->arg[1], &blocks);
        if (!c->audio || !c->audio_state) { lj_us_failf(lj_us.line, "%s", "audio_inject: no analyzer (app/lj_app_io.c not started)"); return -1; }
        for (i = 0; i < blocks; i++)   /* one analysis block per level, 1 ms apart; a trig runs what an analyzer trig runs */
            if (lj_audio_detect(c->audio_state, c->audio, (uint16_t)v, lj_clock_us() + (uint64_t)i * 1000u)) lj_desktop_run_action(LJ_ACT_AUDIO_TRIG, 0);
        return 0;
    case LJ_US_REMOTE:
        for (i = 0; i < st->nargs; i++) { lj_us_int(st->arg[i], &v); bytes[i] = (uint8_t)v; }
        if (lj_remote_inject(bytes, (size_t)st->nargs, lj_clock_us()) < 0) { lj_us_failf(lj_us.line, "%s", "remote: not executed"); return -1; }
        return 0;
    default:
        return -1;
    }
}

static int lj_us_exec(lj_us_stmt *st)
{
    lj_window *win;
    lj_widget *w;
    int row, col, id, k, ms;
    char path[LJ_PATH_CAP];
    FILE *f;
    switch (st->verb) {
    case LJ_US_OPEN:
        if (strcmp(st->arg[0], "main") == 0) win = lj_desktop_window();
        else if ((k = lj_us_ctl_kind(st->arg[0])) >= 0) win = lj_ctl_open((enum lj_ctl_kind)k);
        else if ((id = lj_us_win_id(st->arg[0])) >= 0) win = lj_win_open((enum lj_win)id);
        else win = NULL;
        if (!win) { lj_us_failf(lj_us.line, "cannot open %s", st->arg[0]); return -1; }
        lj_us.cur = win;
        return 0;
    case LJ_US_CLOSE:
        if (strcmp(st->arg[0], "main") == 0) lj_win_close_prompt(lj_us_close_result, NULL);   /* W4: "Close Lightjockey ?" as a user's close raises it (LJ_ACT_CLOSE skips it under a script); Yes quits */
        else if ((k = lj_us_ctl_kind(st->arg[0])) >= 0) lj_ctl_close((enum lj_ctl_kind)k);
        else if ((id = lj_us_win_id(st->arg[0])) >= 0) lj_win_close((enum lj_win)id);
        else { lj_us_failf(lj_us.line, "unknown window %s", st->arg[0]); return -1; }
        lj_us.cur = NULL;
        return 0;
    case LJ_US_CLICK:    return lj_us_click(st, 1, 0);
    case LJ_US_RCLICK:   return lj_us_click(st, 2, 0);
    case LJ_US_DBLCLICK: return lj_us_click(st, 1, 1);
    case LJ_US_DRAG:     return lj_us_drag(st);
    case LJ_US_TYPE:     return lj_us_type(st);
    case LJ_US_KEY:      return lj_us_key(st);
    case LJ_US_WAIT:
        lj_us_int(st->arg[0], &ms);
        lj_us.wait_until = lj_clock_us() + (uint64_t)ms * 1000u;
        return 0;
    case LJ_US_SHOT:     return lj_us_shot(st->arg[0]);
    case LJ_US_ASSERT_CAPTION:
        if (!(win = lj_us_window(st->arg[0]))) return -1;
        return lj_us_assert_eq("caption", lj_window_caption(win)->text, st->arg[1]);
    case LJ_US_ASSERT_TEXT:
        if (strncmp(st->arg[0], "main.sb_", 8) == 0) {
            for (k = 0; k < LJ_SB_COUNT; k++) if (strcmp(st->arg[0] + 5, lj_us_sb_names[k]) == 0) return lj_us_assert_eq(st->arg[0], lj_statusbar_panel_text((enum lj_status_field)k), st->arg[1]);
        }
        { const char *mc = lj_us_menu_caption(st->arg[0]); if (mc) return lj_us_assert_eq(st->arg[0], mc, st->arg[1]); }
        if (!(w = lj_us_widget(st->arg[0], &win, &row, &col))) return -1;
        return lj_us_assert_eq(st->arg[0], lj_us_text_of(w, row, col), st->arg[1]);
    case LJ_US_ASSERT_ENABLED:
    case LJ_US_ASSERT_VISIBLE:
        {   lj_menu_rt *rt = lj_us_menu_rt(st->arg[0]);                          /* a menu item: its runtime state (the item widget is non-visual) */
            if (rt) return lj_us_assert_eq(st->arg[0], (st->verb == LJ_US_ASSERT_ENABLED ? rt->enabled : rt->visible) ? "1" : "0", st->arg[1]); }
        if (!(w = lj_us_widget(st->arg[0], &win, NULL, NULL))) return -1;
        k = st->verb == LJ_US_ASSERT_ENABLED ? (int)w->enabled : (int)w->visible;
        return lj_us_assert_eq(st->arg[0], k ? "1" : "0", st->arg[1]);
    case LJ_US_ASSERT_DOWN:
        if (!(w = lj_us_widget(st->arg[0], &win, NULL, NULL))) return -1;
        if (w->kind != LJ_K_SPEEDBUTTON) { lj_us_failf(lj_us.line, "%s is not a TSpeedButton", st->arg[0]); return -1; }
        return lj_us_assert_eq(st->arg[0], lj_w_speedbutton_down(w) ? "1" : "0", st->arg[1]);
    case LJ_US_ASSERT_CHECKED:
        if (!(w = lj_us_widget(st->arg[0], &win, NULL, NULL))) return -1;
        if (w->kind != LJ_K_CHECKBOX && w->kind != LJ_K_RADIOBUTTON) { lj_us_failf(lj_us.line, "%s is not a TCheckBox or TRadioButton", st->arg[0]); return -1; }
        return lj_us_assert_eq(st->arg[0], (w->kind == LJ_K_CHECKBOX ? lj_w_checkbox_checked(w) : lj_w_radiobutton_checked(w)) ? "1" : "0", st->arg[1]);
    case LJ_US_ASSERT_FRAME: return lj_us_assert_frame(st);
    case LJ_US_ASSERT_VIEW:  return lj_us_assert_view(st);
    case LJ_US_ASSERT_FILE:
        f = fopen(st->arg[0], "rb");
        if (!f && lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, st->arg[0], path, sizeof path) == 0) f = fopen(path, "rb");   /* a relative path is also tried under the user library */
        if (f) fclose(f);
        if ((f != NULL) == (strcmp(st->arg[1], "exists") == 0)) return 0;
        lj_us_failf(lj_us.line, f ? "%s exists" : "%s is missing", st->arg[0]);
        return -1;
    case LJ_US_ASSERT_PPS:   return lj_us_assert_pps(st);
    case LJ_US_ASSERT_CLIENT: case LJ_US_ASSERT_POS: {
        lj_rect r;
        if (st->verb == LJ_US_ASSERT_POS && lj_us_int(st->arg[0], &k) == 0) return lj_us_assert_fixture_pos(st);   /* W3 */
        int x, y, want_x, want_y;
        char msg[96];
        if (!(win = lj_us_window(st->arg[0]))) return -1;
        lj_us_int(st->arg[1], &want_x); lj_us_int(st->arg[2], &want_y);
        if (st->verb == LJ_US_ASSERT_CLIENT) { lj_window_client_rect(win, &r); x = r.w; y = r.h; }
        else if (lj_us_screen(win, &x, &y) != 0) { lj_us_failf(lj_us.line, "%s", "no screen position (headless)"); return -1; }
        if (x == want_x && y == want_y) return 0;
        snprintf(msg, sizeof msg, "%s %s is %dx%d, expected %dx%d", st->arg[0], st->verb == LJ_US_ASSERT_CLIENT ? "client" : "position", x, y, want_x, want_y);
        lj_us_failf(lj_us.line, "%s", msg);
        return -1;
    }
    case LJ_US_MOVE: {                                  /* W2 integrator 2: windows that open centred overlap; a drag needs its target visible */
        const lj_win_host *h = lj_win_host_get();
        int x, y;
        if (!(win = lj_us_window(st->arg[0]))) return -1;
        if (!h || !h->move) { lj_us_failf(lj_us.line, "%s", "move needs the platform (headless)"); return -1; }
        lj_us_int(st->arg[1], &x); lj_us_int(st->arg[2], &y);
        h->move(h->ud, win, x, y);
        return 0;
    }
    case LJ_US_LOG:
        lj_log("script: %s", st->arg[0]);
        return 0;
    case LJ_US_QUIT:
        lj_us.state = 2;
        return 0;
    case LJ_US_MIDI: case LJ_US_HOTKEY: case LJ_US_ASSERT_LED: case LJ_US_ASSERT_FILE_LINE: case LJ_US_SLEEP_UNTIL_TICK:
        return lj_us_exec_w2(st);
    case LJ_US_CL_LOAD: case LJ_US_CL_GOTO: case LJ_US_CL_EXECUTE: case LJ_US_CL_ASSERT_LINE: case LJ_US_GS2_IMPORT: case LJ_US_ASSERT_PROMPT: case LJ_US_MM_SET:
        return lj_us_exec_w3(st);
    case LJ_US_DUMP_CAPTIONS:
        return lj_us_dump_captions(st->arg[0]);
    case LJ_US_SACN_IN: case LJ_US_ARTNET_IN: case LJ_US_AUDIO_INJECT: case LJ_US_REMOTE:
        return lj_us_exec_w5(st);
    default:
        return -1;
    }
}

int lj_uiscript_step(void)
{
    lj_us_stmt *st;
    if (lj_us.state == -1) return -1;
    if (lj_us.state != 1) return 0;
    if (lj_us.wait_until) {
        if (lj_clock_us() < lj_us.wait_until) return 1;
        lj_us.wait_until = 0;
    }
    if (lj_us.tick_deadline) {
        const lj_engine_status *v = lj_us.host.view ? lj_us.host.view(lj_us.host.ud) : NULL;
        if (v && v->tick_no >= lj_us.tick_until) lj_us.tick_deadline = 0;
        else if (lj_clock_us() < lj_us.tick_deadline) return 1;
        else { char msg[96]; snprintf(msg, sizeof msg, "sleep_until_tick: tick_no %llu after 10 s, expected %llu", v ? (unsigned long long)v->tick_no : 0ull, (unsigned long long)lj_us.tick_until); lj_us_failf(lj_us.line, "%s", msg); return -1; }
    }
    if (lj_us.pc >= lj_us.n) { lj_us.state = 2; return 0; }     /* EOF behaves like quit */
    st = &lj_us.st[lj_us.pc++];
    lj_us.line = st->line;
    if (lj_us_exec(st) != 0) { if (lj_us.state != -1) lj_us_failf(st->line, "%s", "failed"); return -1; }
    if (lj_us.state == 2) return 0;
    return 1;
}
