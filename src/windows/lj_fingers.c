/* lj_fingers.c - the Fingers family (lj_fingers.h; W4 da2532-fingers track, D16, I9, notes 04 s17, 05 s5).
 * TLJFingersMainForm binds FormActivate FormCreate FormDestroy FormMouseDown FormMouseMove FormMouseUp FormMouseWheel
 * FormPaint FormShow; the surface itself is one code-built paint box over the 800 x 589 client (the DFM has no controls:
 * LJ2 painted a skin bitmap, which is Martin art and never ships; ours is drawn from rectangles and Arial Black text).
 * TFingersStatus is code-built the same way over its 451 x 567 client. The other eight forms are generated; their OnXxx
 * are bound below (ME-16). Never serial: Fingers Setup's "Serial connection (RS-232)" and COM Port are greyed. */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf under /W4 /WX on MSVC (as io/lj_midi.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows/lj_fingers.h"

#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "io/lj_dmxin.h"
#include "io/lj_fn.h"
#include "io/lj_midi.h"
#include "ui/lj_dnd.h"
#include "ui/lj_menu.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_da2532.h"
#include "windows/lj_master_intensity.h"
#include "windows/lj_statics2_form.h"
#include "windows/lj_warnings.h"

#define LJ_FG_STAY_ON_TOP 2u              /* = LJ_WF_STAY_ON_TOP of platform/lj_platform.h (not includable from windows/) */
#define LJ_FG_FILE        "Fingers.Ini"   /* LJ2's name (exe 0x20274C); the keys are ours, PROPOSAL rig 05 s5 */
#define LJ_FG_NA_HINT     "Not available in LJ4EVR"

/* ---- the catalogues, verbatim from the help (05 s5.3); the lj_fn each one reaches, LJ_FN_NONE = listed, not in LJ4EVR yet ---- */
const char *const lj_fingers_slot_functions[] = { "Slot On/Off", "Slot on flash", "Slot On Solo", "Slot OnSolo Flash", "Slot Clear", "Slot Stop",
    "Slot Start", "Slot Start/Stop", "Slot Trig Def", "Slot Trig Fwd", "Slot Trig Rev", "Slot Trig Rnd", "Slot Trig Bounce", "Slot Auto Trig",
    "Slot Audio Trig", "Slot Manual Trig", "Slot Trig Type", "Slot On/Off (Sync)", "Slot Start/Stop (Sync)", "Slot Trig Type (Sync)", NULL };
static const enum lj_fn lj_fg_slot_fn[] = { LJ_FN_TOGGLE_SLOT_ENABLE_1, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_CLEAR_SEQUENCE_1, LJ_FN_SEQ_STOP_1,
    LJ_FN_SEQ_GO_1, LJ_FN_SEQ_STOP_GO_TOGGLE_1, LJ_FN_TRIG_SEQ_DEFAULT_1, LJ_FN_TRIG_SEQ_FWD_1, LJ_FN_TRIG_SEQ_REV_1, LJ_FN_TRIG_SEQ_RND_1, LJ_FN_TRIG_SEQ_BOUNCE_1,
    LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE, LJ_FN_NONE };   /* the base row: slot n = base + n - 1 */
const char *const lj_fingers_page_functions[] = { "Sequence Intensity", "Seq. Intensity/On-Off", "Sequence Time", "Sequence Time (Inv)", "Statics 1-12 Level",
    "Statics 13-20 Level", "Intensity Group", "HTP Group 1-12", "HTP Group 13-24", "Sequence DMX Values", "DMX in (1-12)", "DMX in (13-24)", "DMX in (25-36)",
    "DMX in (37-48)", NULL };
enum { PF_STATICS1 = 4, PF_STATICS13 = 5, PF_INTENSITY = 6, PF_HTP1 = 7, PF_HTP13 = 8, PF_DMXIN1 = 10, PF_DMXIN37 = 13 };
const char *const lj_fingers_jog_functions[] = { "Cue Time", "P/T Macro Ampl.", "P/T Macro Ampl. (Inv)", "P/T Macro Speed", "P/T Macro Speed (Inv)", "Audio Gain",
    "Min Trglvl", "Smoke Ctrl Level", "Smoke Ctrl Duration", "Smoke Ctrl Cycle", "Hazer Ctrl Level", "Hazer Ctrl Fan", "Pan MSB", "Pan LSB", "Tilt MSB",
    "Tilt LSB", "GM Relative", "Fader Relative", NULL };
enum { JF_GM_REL = 16, JF_FADER_REL = 17, JF_AUDIO_GAIN = 5, JF_MIN_TRGLVL = 6 /* W5 audio */ };
const char *const lj_fingers_ud_functions[] = { "Select Cue (Global)", "Select Cue (Page)", "Select Sequence", "Select Cuelist", "Select BGCue", "Select Macro",
    "Active Select", "Cuelist (Manual)", "Cuelist (Edit)", NULL };
enum { UF_CL_MANUAL = 7 };
static int count_of(const char *const *l) { int n = 0; while (l[n]) n++; return n; }
/* the status panel colour code of an F key (05 s5.2): hotkey white, sequence orange, cue yellow, BG cue magenta, cue list cyan */
static const uint32_t lj_fg_type_rgb[LJ_DA_TYPES] = { 0x000000u, 0xFFFFFFu, 0xFF8040u, 0xFFFF00u, 0x00FFFFu, 0xFF00FFu };

/* ---- the model ---- */
static struct {
    lj_da_val fkey[LJ_FG_SHIFTS][LJ_FG_FKEYS];
    int row_fn[LJ_FG_ROWS], page_fn[LJ_FG_PAGES], jog_fn[LJ_FG_JOGS], ud_fn;
    int shift, page;                                  /* 0-based */
    int fader[LJ_FG_FADERS + 1];                      /* [0] = GM; percent */
    int jog_last[LJ_FG_JOGS];                         /* the last absolute value a level row brought (-1 none) */
    lj_da_val held_f[LJ_FG_FKEYS];
    enum lj_fn held_slot[LJ_FG_ROWS][LJ_FG_SLOTS];
    enum lj_fn held_bump[LJ_FG_FADERS];                        /* ME-143: the function a bump fired, released by its note-off whatever the page is then */
    int loaded;
} fg = { .row_fn = { 0, 8, 9 }, .page_fn = { PF_INTENSITY, PF_STATICS1, PF_STATICS13, PF_HTP1 }, .jog_fn = { 5, 1, 16, 0, 17 }, .ud_fn = UF_CL_MANUAL,
         .jog_last = { -1, -1, -1, -1, -1 }, .fader = { 100 } };   /* defaults from fingersstatuslarge.gif (reduced) where it shows them, PROPOSAL rig 05 s5 */

static int in(int v, int lo, int hi) { return v >= lo && v <= hi; }
static int lib_file(char *path, size_t cap) { return lj_ctx_get()->lib && lj_library_path(lj_ctx_get()->lib, LJ_FG_FILE, path, cap) == 0 ? 0 : -1; }
static lj_ini *fg_ini;                                /* Fingers.Ini as loaded (the Setup dialog's keys live here too) */
static int seti(const char *sec, const char *key, int dflt) { return lj_ini_geti(fg_ini, sec, key, dflt); }

int lj_fingers_save(void)
{
    char path[LJ_PATH_CAP], key[16], val[80];
    int s, k, rc;
    if (lib_file(path, sizeof path) != 0) return -1;
    if (!fg_ini && (fg_ini = lj_ini_new()) == NULL) return -1;
    for (s = 0; s < LJ_FG_SHIFTS; s++)
        for (k = 0; k < LJ_FG_FKEYS; k++) {
            const lj_da_val *v = &fg.fkey[s][k];
            static const char *const w[LJ_DA_TYPES] = { "", "fn", "seq", "cue", "cl", "bg" };
            snprintf(key, sizeof key, "S%dF%d", s, k + 1);
            if (v->type == LJ_DA_FN) snprintf(val, sizeof val, "fn %s", lj_fn_table[v->num].name);
            else if (v->type) snprintf(val, sizeof val, "%s %u", w[v->type], (unsigned)v->num);
            else val[0] = 0;
            lj_ini_sets(fg_ini, "FUNCTIONS", key, val);
        }
    for (k = 0; k < LJ_FG_ROWS; k++) { snprintf(key, sizeof key, "Row%d", k + 1); lj_ini_seti(fg_ini, "SLOTS", key, fg.row_fn[k]); }
    for (k = 0; k < LJ_FG_PAGES; k++) { snprintf(key, sizeof key, "Page%c", 'A' + k); lj_ini_seti(fg_ini, "FADERS", key, fg.page_fn[k]); }
    for (k = 0; k < LJ_FG_JOGS; k++) { snprintf(key, sizeof key, "Jog%d", k + 1); lj_ini_seti(fg_ini, "JOGS", key, fg.jog_fn[k]); }
    lj_ini_seti(fg_ini, "SCROLL", "Set", fg.ud_fn);
    rc = lj_ini_save(fg_ini, path);
    if (rc != 0) lj_log("fingers: cannot save %s", path);
    return rc;
}

int lj_fingers_load(void)
{
    static const char *const w[LJ_DA_TYPES] = { "", "fn", "seq", "cue", "cl", "bg" };
    char path[LJ_PATH_CAP], key[16], word[8], arg[64], *end;
    lj_da_val fk[LJ_FG_SHIFTS][LJ_FG_FKEYS];
    int row[LJ_FG_ROWS], page[LJ_FG_PAGES], jog[LJ_FG_JOGS], ud, s, k, t, bad = 0;
    lj_ini *ini;
    fg.loaded = 1;
    if (lib_file(path, sizeof path) != 0 || (ini = lj_ini_load(path)) == NULL) { if (!fg_ini) fg_ini = lj_ini_new(); return 0; }
    memset(fk, 0, sizeof fk);
    for (s = 0; s < LJ_FG_SHIFTS && !bad; s++)
        for (k = 0; k < LJ_FG_FKEYS && !bad; k++) {
            const char *v;
            unsigned long n;
            snprintf(key, sizeof key, "S%dF%d", s, k + 1);
            v = lj_ini_gets(ini, "FUNCTIONS", key, "");
            if (!*v) continue;
            if (strlen(v) >= sizeof arg || sscanf(v, "%7s %63s", word, arg) != 2) { bad = 1; break; }
            for (t = 1; t < LJ_DA_TYPES && strcmp(word, w[t]) != 0; t++) {}
            if (t >= LJ_DA_TYPES) { bad = 1; break; }
            if (t == LJ_DA_FN) { n = (unsigned long)lj_fn_by_name(arg); if (!n || lj_fn_table[n].group == LJ_FNG_PROGRAMS) { bad = 1; break; } }
            else { n = strtoul(arg, &end, 10); if (*end || arg[0] == '-' || n == 0 || n > 99999999ul) { bad = 1; break; } }
            fk[s][k].type = (uint8_t)t; fk[s][k].num = (uint32_t)n;
        }
    for (k = 0; k < LJ_FG_ROWS; k++) { snprintf(key, sizeof key, "Row%d", k + 1); row[k] = lj_ini_geti(ini, "SLOTS", key, fg.row_fn[k]); if (!in(row[k], 0, count_of(lj_fingers_slot_functions) - 1)) bad = 1; }
    for (k = 0; k < LJ_FG_PAGES; k++) { snprintf(key, sizeof key, "Page%c", 'A' + k); page[k] = lj_ini_geti(ini, "FADERS", key, fg.page_fn[k]); if (!in(page[k], 0, count_of(lj_fingers_page_functions) - 1)) bad = 1; }
    for (k = 0; k < LJ_FG_JOGS; k++) { snprintf(key, sizeof key, "Jog%d", k + 1); jog[k] = lj_ini_geti(ini, "JOGS", key, fg.jog_fn[k]); if (!in(jog[k], 0, count_of(lj_fingers_jog_functions) - 1)) bad = 1; }
    ud = lj_ini_geti(ini, "SCROLL", "Set", fg.ud_fn);
    if (!in(ud, 0, count_of(lj_fingers_ud_functions) - 1) || !in(lj_ini_geti(ini, "SETUP", "Mode", 0), 0, 1) || !in(lj_ini_geti(ini, "SETUP", "FlickerThreshold", 4), 2, 4)) bad = 1;
    if (bad) { lj_ini_free(ini); lj_log("fingers: %s refused", path); if (!fg_ini) fg_ini = lj_ini_new(); return -1; }   /* untrusted file: all or nothing */
    memcpy(fg.fkey, fk, sizeof fg.fkey); memcpy(fg.row_fn, row, sizeof row); memcpy(fg.page_fn, page, sizeof page); memcpy(fg.jog_fn, jog, sizeof jog); fg.ud_fn = ud;
    lj_ini_free(fg_ini); fg_ini = ini;
    return 0;
}
static void ensure_loaded(void) { if (!fg.loaded) lj_fingers_load(); }
int lj_fingers_mode(void) { ensure_loaded(); return seti("SETUP", "Mode", LJ_FG_DISABLED); }

/* ---- the controls ---- */
static void repaint(void);
static int run_val(const lj_da_val *v, int down)
{
    switch (v->type) {
    case LJ_DA_FN:  lj_fn_run((enum lj_fn)v->num, down); return 0;
    case LJ_DA_SEQ: lj_fn_run_prog(LJ_FN_PROG_SEQ_FLASH, v->num, down); return 0;
    case LJ_DA_CUE: if (down) lj_fn_run_prog(LJ_FN_PROG_CUE, v->num, 1); return 0;
    case LJ_DA_CL:  if (down) lj_fn_run_prog(LJ_FN_PROG_CUELIST, v->num, 1); return 0;
    case LJ_DA_BG:  if (down) lj_fn_run_prog(LJ_FN_PROG_BGCUE, v->num, 1); return 0;
    default: return -1;
    }
}
int lj_fingers_fkey(int key, int down)
{
    lj_da_val *h;
    ensure_loaded();
    if (!in(key, 1, LJ_FG_FKEYS)) return -1;
    h = &fg.held_f[key - 1];
    if (!down) { lj_da_val v = *h; memset(h, 0, sizeof *h); return v.type ? run_val(&v, 0) : -1; }
    if (fg.fkey[fg.shift][key - 1].type == LJ_DA_NONE) return -1;
    *h = fg.fkey[fg.shift][key - 1];
    return run_val(h, 1);
}
int lj_fingers_slot(int row, int slot, int down)
{
    enum lj_fn f;
    ensure_loaded();
    if (!in(row, 1, LJ_FG_ROWS) || !in(slot, 1, LJ_FG_SLOTS)) return -1;
    if (!down) { f = fg.held_slot[row - 1][slot - 1]; fg.held_slot[row - 1][slot - 1] = LJ_FN_NONE; if (f) lj_fn_run(f, 0); return f ? 0 : -1; }
    if (lj_fg_slot_fn[fg.row_fn[row - 1]] == LJ_FN_NONE) { lj_log("fingers: %s - " LJ_FG_NA_HINT, lj_fingers_slot_functions[fg.row_fn[row - 1]]); return -1; }
    f = (enum lj_fn)(lj_fg_slot_fn[fg.row_fn[row - 1]] + slot - 1);   /* the 1-12 families are contiguous in lj_actions.h */
    fg.held_slot[row - 1][slot - 1] = f;
    lj_fn_run(f, 1);
    return 0;
}
static int static_fn(int slot) { return slot <= 10 ? LJ_FN_STATIC_1 + slot - 1 : LJ_FN_STATIC_11 + slot - 11; }
int lj_fingers_fader(int fader, int pct)
{
    int f;
    ensure_loaded();
    if (!in(fader, 1, LJ_FG_FADERS)) return -1;
    pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    fg.fader[fader] = pct;
    f = fg.page_fn[fg.page];
    repaint();
    if (f == PF_INTENSITY && fader <= 8) { lj_master_intensity_set(fader, (pct * 255 + 50) / 100); return 0; }   /* faders 1-8 = sub-masters (05 s5.3) */
    if (f == PF_STATICS1) return lj_statics2_set_level(fader, pct);
    if (f == PF_STATICS13 && fader <= 8) return lj_statics2_set_level(12 + fader, pct);
    if (f >= PF_DMXIN1 && f <= PF_DMXIN37) { lj_dmxin_emulate((f - PF_DMXIN1) * 12 + fader, (pct * 255 + 50) / 100, lj_clock_us()); return 0; }   /* W5: DMX-in channels 1-48, one more source (05 s3.3) */
    lj_log("fingers: %s - " LJ_FG_NA_HINT, lj_fingers_page_functions[f]);
    return -1;
}
int lj_fingers_bump(int fader, int down)
{
    int f;
    enum lj_fn fn;
    ensure_loaded();
    if (!in(fader, 1, LJ_FG_FADERS)) return -1;
    if (!down) { fn = fg.held_bump[fader - 1]; fg.held_bump[fader - 1] = LJ_FN_NONE; if (fn) lj_fn_run(fn, 0); return fn ? 0 : -1; }   /* ME-143, as held_slot */
    f = fg.page_fn[fg.page];
    if (f == PF_INTENSITY && fader <= 8) fn = (enum lj_fn)(LJ_FN_BUMP_SUB_FLASH_1 + fader - 1);   /* "follows fader function" (05 s5.2) */
    else if (f == PF_HTP1) fn = (enum lj_fn)(LJ_FN_HTP_BUMP_FLASH_1 + fader - 1);
    else if (f == PF_STATICS1) fn = (enum lj_fn)static_fn(fader);
    else if (f == PF_STATICS13 && fader <= 8) fn = (enum lj_fn)static_fn(12 + fader);
    else return -1;
    fg.held_bump[fader - 1] = fn;
    lj_fn_run(fn, 1);
    return 0;
}
int lj_fingers_gm(int pct)
{
    pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    fg.fader[0] = pct;
    lj_fn_run_prog(LJ_FN_MASTER_LEVEL, (uint32_t)pct, 1);           /* the Master Intensity row's level convention */
    repaint();
    return 0;
}
int lj_fingers_bo(void) { lj_fn_run(LJ_FN_BLACKOUT_RESTORE, 1); return 0; }
int lj_fingers_jog(int jog, int delta)
{
    int f;
    ensure_loaded();
    if (!in(jog, 1, LJ_FG_JOGS)) return -1;
    f = fg.jog_fn[jog - 1];
    if (f == JF_GM_REL) return lj_fingers_gm(fg.fader[0] + delta);
    if (f == JF_FADER_REL) { int i; for (i = 1; i <= LJ_FG_FADERS; i++) lj_fingers_fader(i, fg.fader[i] + delta); return 0; }   /* PROPOSAL rig 05 s5: every fader of the page moves */
    if ((f == JF_AUDIO_GAIN || f == JF_MIN_TRGLVL) && lj_ctx_get()->audio) {   /* W5-AUD-6: the analyzer's Gain fader / noise threshold, one step per detent */
        lj_audio_cfg *a = lj_ctx_get()->audio;
        if (f == JF_AUDIO_GAIN) lj_audio_set_gain_pos(a, lj_audio_gain_pos(a) + delta); else lj_audio_set_noise(a, a->noise + delta);
        return 0;
    }
    lj_log("fingers: %s - " LJ_FG_NA_HINT, lj_fingers_jog_functions[f]);
    return -1;
}
int lj_fingers_fader_value(int fader) { return in(fader, 0, LJ_FG_FADERS) ? fg.fader[fader] : -1; }
int lj_fingers_set_shift(int shift) { if (!in(shift, 0, LJ_FG_SHIFTS - 1)) return -1; fg.shift = shift; repaint(); return 0; }
int lj_fingers_set_page(int page) { if (!in(page, 1, LJ_FG_PAGES)) return -1; fg.page = page - 1; repaint(); return 0; }
int lj_fingers_assign_fkey(int shift, int key, int type, uint32_t num)
{
    lj_da_val *v;
    if (!in(shift, 0, LJ_FG_SHIFTS - 1) || !in(key, 1, LJ_FG_FKEYS) || !in(type, 0, LJ_DA_TYPES - 1)) return -1;
    if (type == LJ_DA_FN && (num == 0 || num >= LJ_FN_COUNT || lj_fn_table[num].group == LJ_FNG_PROGRAMS)) return -1;
    if (type > LJ_DA_FN && (num == 0 || num > 99999999u)) return -1;
    v = &fg.fkey[shift][key - 1];
    memset(v, 0, sizeof *v);
    v->type = (uint8_t)type; v->num = type ? num : 0;
    repaint();
    return 0;
}
int lj_fingers_set_row_function(int row, int index) { if (!in(row, 1, LJ_FG_ROWS) || !in(index, 0, count_of(lj_fingers_slot_functions) - 1)) return -1; fg.row_fn[row - 1] = index; repaint(); return 0; }
int lj_fingers_set_page_function(int page, int index) { if (!in(page, 1, LJ_FG_PAGES) || !in(index, 0, count_of(lj_fingers_page_functions) - 1)) return -1; fg.page_fn[page - 1] = index; repaint(); return 0; }
int lj_fingers_set_jog_function(int jog, int index) { if (!in(jog, 1, LJ_FG_JOGS) || !in(index, 0, count_of(lj_fingers_jog_functions) - 1)) return -1; fg.jog_fn[jog - 1] = index; repaint(); return 0; }
static void scroll(int which /* 0 up, 1 down, 2 R/S */)
{
    if (fg.ud_fn == UF_CL_MANUAL) { static const enum lj_fn f[3] = { LJ_FN_CUELIST_BACK, LJ_FN_CUELIST_GO, LJ_FN_CUELIST_STOP }; lj_fn_run(f[which], 1); return; }   /* [up = back, down = go, R/S = start/stop] (05 s5.3) */
    lj_log("fingers: %s - " LJ_FG_NA_HINT, lj_fingers_ud_functions[fg.ud_fn]);
}

/* ---- MIDI Learn (Alt+click on a control) ---- */
static enum lj_fn lj_fg_learn_fn;
int lj_fingers_learn(enum lj_fn fn)
{
    int level;
    if ((int)fn <= 0 || fn >= LJ_FN_COUNT) return -1;
    level = (fn >= LJ_FN_FINGERS_FADER_1 && fn <= LJ_FN_FINGERS_FADER_12) || fn == LJ_FN_FINGERS_GM || (fn >= LJ_FN_FINGERS_JOG_1 && fn <= LJ_FN_FINGERS_JOG_5);
    lj_fg_learn_fn = fn;
    lj_midi_learn_begin_ext(level);
    lj_log("fingers: MIDI Learn armed for %s", lj_fn_table[fn].name);
    return 0;
}
static void learn_poll(void)
{
    int ch, note, cc;
    if (lj_fg_learn_fn == LJ_FN_NONE || !lj_midi_learn_take_ext(&ch, &note, &cc)) return;
    if (lj_midi_learn_bind(lj_ctx_get()->midi, lj_fg_learn_fn, ch, note, cc) == 0) {
        lj_log("fingers: %s learnt (channel %d, %s %d)", lj_fn_table[lj_fg_learn_fn].name, ch, cc ? "CC" : "note", note);
        lj_desktop_run_action(LJ_ACT_MIDI_SAVE, 0);
    }
    lj_fg_learn_fn = LJ_FN_NONE;
}

/* ================= Virtual Fingers (TLJFingersMainForm, code-drawn) ================= */
/* Layout: our own, proportional to fingers.gif (300 x 221, reduced from the 800 x 589 client): PROPOSAL rig 05 s5, nothing traced. */
enum fg_kind { K_F, K_SHIFT, K_MODE, K_SLOT, K_SQUARE, K_SCROLL, K_BO, K_FADER, K_BUMP, K_GM, K_PAGE, K_JOG, K_COUNT };
typedef struct { uint8_t kind, idx, row; short x, y, w, h; } fg_ctl;
static fg_ctl lj_fg_ctl[LJ_FG_FKEYS + 3 + 3 + 36 + 1 + 3 + 1 + 12 + 12 + 1 + 4 + 5];
static int lj_fg_nctl;
#define FG_TRACK_Y 300
#define FG_TRACK_H 210
#define FG_KNOB_H  18
static void add(int kind, int idx, int row, int x, int y, int w, int h)
{
    fg_ctl *c = &lj_fg_ctl[lj_fg_nctl++];
    c->kind = (uint8_t)kind; c->idx = (uint8_t)idx; c->row = (uint8_t)row; c->x = (short)x; c->y = (short)y; c->w = (short)w; c->h = (short)h;
}
static void build_layout(void)
{
    int i, r;
    if (lj_fg_nctl) return;
    for (i = 0; i < LJ_FG_FKEYS; i++) add(K_F, i, 0, 26 + i * 46, 84, 42, 30);
    for (i = 0; i < 3; i++) { add(K_SHIFT, i, 0, 498, 52 + i * 24, 36, 18); add(K_MODE, i, 0, 600, 52 + i * 24, 24, 18); }
    for (r = 0; r < LJ_FG_ROWS; r++) for (i = 0; i < LJ_FG_SLOTS; i++) add(K_SLOT, i, r, 26 + i * 38, 150 + r * 40, 34, 34);
    add(K_SQUARE, 0, 0, 500, 150, 44, 34);
    for (i = 0; i < 3; i++) add(K_SCROLL, i, 0, 566, 150 + i * 40, 40, 34);
    add(K_BO, 0, 0, 500, 230, 44, 34);
    for (i = 0; i < LJ_FG_FADERS; i++) { add(K_FADER, i, 0, 26 + i * 38, FG_TRACK_Y, 34, FG_TRACK_H); add(K_BUMP, i, 0, 26 + i * 38, 530, 34, 22); }
    add(K_GM, 0, 0, 504, FG_TRACK_Y, 34, FG_TRACK_H);
    for (i = 0; i < LJ_FG_PAGES; i++) add(K_PAGE, i, 0, 566, 330 + i * 46, 40, 38);
    for (i = 0; i < LJ_FG_JOGS; i++) add(K_JOG, i, 0, 690, 20 + i * 114, 90, 90);
}
static struct { lj_window *win; lj_widget *pb; int drag; int drag_y; int drag_v; } vf;

static void face(lj_surface *s, int x, int y, int w, int h, int down) { if (down) lj_button_pressed(s, x, y, w, h); else lj_button_raised(s, x, y, w, h); }
static void ctext(lj_surface *s, const lj_font *f, int x, int y, int w, int h, const char *t, uint32_t rgb) { lj_text(s, f, x + (w - lj_text_width(f, t)) / 2, y + (h - f->height) / 2, t, rgb, 0); }
static int knob_y(int v) { return FG_TRACK_Y + (100 - v) * (FG_TRACK_H - FG_KNOB_H) / 100; }
static void draw_fader(lj_surface *s, const lj_font *sm, const fg_ctl *c, int v)
{
    char t[8];
    int ky = knob_y(v), y;
    lj_fill(s, c->x + c->w / 2 - 2, c->y, 4, c->h, 0x000000u);                       /* the slot */
    lj_edge_sunken(s, c->x + c->w / 2 - 3, c->y - 1, 6, c->h + 2, 0x000000u);
    face(s, c->x + 2, ky, c->w - 4, FG_KNOB_H, 0);
    for (y = ky + 4; y < ky + FG_KNOB_H - 3; y += 3) lj_hline(s, c->x + 5, y, c->w - 10, 0x808080u);
    snprintf(t, sizeof t, "%d", v);
    ctext(s, sm, c->x, c->y + c->h + 2, c->w, sm->height, t, 0x000000u);
}
static void draw_jog(lj_surface *s, const fg_ctl *c, int lit)
{
    int x, y, r = c->w / 2, cx = c->x + r, cy = c->y + r;
    for (y = -r; y < r; y++)
        for (x = -r; x < r; x++) {
            int d = x * x + y * y;
            if (d < r * r) lj_fill(s, cx + x, cy + y, 1, 1, d > (r - 3) * (r - 3) ? 0x404040u : d < (r / 2) * (r / 2) ? 0x202020u : (lit ? 0x707070u : 0x585858u));
        }
}
static void vf_paint(lj_widget *w, lj_surface *s)
{
    const lj_font *ab = &lj_font_arialblack18, *sm = &lj_font_sans13;
    char t[24];
    int ax, ay, i;
    (void)w;
    lj_widget_abs(vf.pb, &ax, &ay);
    lj_fill(s, ax, ay, vf.pb->w, vf.pb->h, 0xD8D8D8u);                                    /* our panel grey, no skin */
    lj_bevel1(s, ax, ay, vf.pb->w, vf.pb->h, 0xFFFFFFu, 0x606060u);
    lj_text(s, ab, ax + 26, ay + 26, "VIRTUAL FINGERS", 0x000000u, 0);
    for (i = 0; i < lj_fg_nctl; i++) {
        const fg_ctl *c = &lj_fg_ctl[i];
        int x = ax + c->x, y = ay + c->y;
        fg_ctl cc = *c; cc.x = (short)x; cc.y = (short)y;
        switch (c->kind) {
        case K_F:      face(s, x, y, c->w, c->h, fg.held_f[c->idx].type != 0); snprintf(t, sizeof t, "F%d", c->idx + 1); ctext(s, ab, x, y, c->w, c->h, t, 0x000000u); break;
        case K_SHIFT:  face(s, x, y, c->w, c->h, fg.shift == c->idx + 1); snprintf(t, sizeof t, "SHIFT %d", c->idx + 1); lj_text(s, sm, x + c->w + 4, y + 3, t, 0x000000u, 0); break;
        case K_MODE:   lj_fill(s, x - 10, y + 6, 6, 6, c->idx == 0 ? 0xFF0000u : 0x600000u); face(s, x, y, c->w, c->h, 0); snprintf(t, sizeof t, "MODE %d", c->idx + 1); lj_text(s, sm, x + c->w + 4, y + 3, t, 0x000000u, 0); break;   /* only mode 1 works (05 s5.2) */
        case K_SLOT:   face(s, x, y, c->w, c->h, fg.held_slot[c->row][c->idx] != LJ_FN_NONE); ctext(s, ab, x, y, c->w, c->h, c->row == 0 ? ">>" : c->row == 1 ? "<" : ">", 0x000000u);
                       if (c->row == 0) { snprintf(t, sizeof t, "%d", c->idx + 1); ctext(s, sm, x, y - sm->height - 1, c->w, sm->height, t, 0x000000u); } break;
        case K_SQUARE: face(s, x, y, c->w, c->h, lj_fingers_get(LJ_WIN_FINGERS_STATUS) != NULL); lj_fill(s, x + c->w / 2 - 5, y + c->h / 2 - 5, 10, 10, 0x000000u); break;
        case K_SCROLL: face(s, x, y, c->w, c->h, 0); ctext(s, ab, x, y, c->w, c->h, c->idx == 0 ? "^" : c->idx == 1 ? "v" : "R/S", 0x000000u); break;
        case K_BO:     face(s, x, y, c->w, c->h, lj_ctx_get()->view.blackout); ctext(s, ab, x, y, c->w, c->h, "B/O", lj_ctx_get()->view.blackout ? 0xFF0000u : 0x000000u); break;
        case K_FADER:  draw_fader(s, sm, &cc, fg.fader[c->idx + 1]); break;
        case K_BUMP:   face(s, x, y, c->w, c->h, 0); snprintf(t, sizeof t, "%d", c->idx + 1); ctext(s, sm, x, y + c->h + 14, c->w, sm->height, t, 0x000000u); break;
        case K_GM:     draw_fader(s, sm, &cc, fg.fader[0]); ctext(s, ab, x, y - ab->height - 2, c->w, ab->height, "GM", 0x000000u); break;
        case K_PAGE:   face(s, x, y, c->w, c->h, fg.page == c->idx); t[0] = (char)('A' + c->idx); t[1] = 0; ctext(s, ab, x, y, c->w, c->h, t, 0x000000u);
                       if (fg.page == c->idx) lj_fill(s, x + c->w / 2 - 3, y + 4, 6, 4, 0x00C000u);   /* the green page LED (fingers.gif) */
                       break;
        case K_JOG:    draw_jog(s, &cc, vf.drag == i + 1); snprintf(t, sizeof t, "JOG %d", c->idx + 1); lj_text(s, sm, x - 2, y + c->h + 2, t, 0x000000u, 0); break;
        default: break;
        }
    }
}
static int vf_hit(int x, int y) { int i; for (i = 0; i < lj_fg_nctl; i++) { const fg_ctl *c = &lj_fg_ctl[i]; if (x >= c->x && y >= c->y && x < c->x + c->w && y < c->y + c->h) return i; } return -1; }
static enum lj_fn ext_of(const fg_ctl *c)
{
    static const enum lj_fn row_base[LJ_FG_ROWS] = { LJ_FN_FINGERS_SLOT_TOP_1, LJ_FN_FINGERS_SLOT_MID_1, LJ_FN_FINGERS_SLOT_BOT_1 };
    switch (c->kind) {
    case K_F: return (enum lj_fn)(LJ_FN_FINGERS_F_1 + c->idx);
    case K_SLOT: return (enum lj_fn)(row_base[c->row] + c->idx);
    case K_FADER: return (enum lj_fn)(LJ_FN_FINGERS_FADER_1 + c->idx);
    case K_BUMP: return (enum lj_fn)(LJ_FN_FINGERS_BUMP_1 + c->idx);
    case K_GM: return LJ_FN_FINGERS_GM;
    case K_BO: return LJ_FN_FINGERS_BO;
    case K_JOG: return (enum lj_fn)(LJ_FN_FINGERS_JOG_1 + c->idx);
    default: return LJ_FN_NONE;
    }
}
static int fader_pct_at(int y) { int v = 100 - ((y - FG_TRACK_Y - FG_KNOB_H / 2) * 100 + (FG_TRACK_H - FG_KNOB_H) / 2) / (FG_TRACK_H - FG_KNOB_H); return v < 0 ? 0 : v > 100 ? 100 : v; }
static void vf_event(lj_widget *w, int ev, void *ud)
{
    const lj_mouse_ev *m = lj_window_last_event(vf.win);
    int ax, ay, x, y, i;
    (void)ud;
    if (!m) return;
    lj_widget_abs(w, &ax, &ay);
    x = m->x - ax; y = m->y - ay;
    if (ev == LJ_WEV_MOUSE_DOWN) {
        const fg_ctl *c;
        if ((i = vf_hit(x, y)) < 0) return;
        c = &lj_fg_ctl[i];
        if (m->alt) { if (ext_of(c)) lj_fingers_learn(ext_of(c)); return; }
        if (m->button == 2) {                                         /* right-click: the square = hold (deactivate), pages = multi-select (05 s5.2): neither in LJ4EVR */
            if (c->kind == K_SQUARE || c->kind == K_PAGE) lj_log("fingers: right-click %s - " LJ_FG_NA_HINT, c->kind == K_SQUARE ? "square" : "page");
            return;
        }
        if (m->button != 1) return;
        vf.drag = i + 1; vf.drag_y = y; vf.drag_v = 0;
        switch (c->kind) {
        case K_F: lj_fingers_fkey(c->idx + 1, 1); break;
        case K_SHIFT: lj_fingers_set_shift(fg.shift == c->idx + 1 ? 0 : c->idx + 1); break;   /* shift latches on Virtual Fingers (05 s5.2) */
        case K_SLOT: lj_fingers_slot(c->row + 1, c->idx + 1, 1); break;
        case K_SQUARE: lj_fn_run(LJ_FN_FINGERS_STATUS_PANEL, 1); break;
        case K_SCROLL: scroll(c->idx); break;
        case K_BO: lj_fingers_bo(); break;
        case K_FADER: lj_fingers_fader(c->idx + 1, fader_pct_at(y)); break;
        case K_GM: lj_fingers_gm(fader_pct_at(y)); break;
        case K_BUMP: lj_fingers_bump(c->idx + 1, 1); break;
        case K_PAGE: lj_fingers_set_page(c->idx + 1); break;
        default: break;
        }
    } else if (ev == LJ_WEV_MOUSE_MOVE && vf.drag) {
        const fg_ctl *c = &lj_fg_ctl[vf.drag - 1];
        if (c->kind == K_FADER) lj_fingers_fader(c->idx + 1, fader_pct_at(y));
        else if (c->kind == K_GM) lj_fingers_gm(fader_pct_at(y));
        else if (c->kind == K_JOG) { int d = (vf.drag_y - y) / 4 - vf.drag_v; if (d) { vf.drag_v += d; lj_fingers_jog(c->idx + 1, d); } }   /* click-hold and move up / down (04 s17) */
    } else if (ev == LJ_WEV_MOUSE_UP && vf.drag) {
        const fg_ctl *c = &lj_fg_ctl[vf.drag - 1];
        if (c->kind == K_F) lj_fingers_fkey(c->idx + 1, 0);
        else if (c->kind == K_SLOT) lj_fingers_slot(c->row + 1, c->idx + 1, 0);
        else if (c->kind == K_BUMP) lj_fingers_bump(c->idx + 1, 0);
        vf.drag = 0;
    } else if ((ev == LJ_WEV_MOUSE_WHEEL) && seti("VIRTUAL", "MouseWheel", 0) && (i = vf_hit(x, y)) >= 0) {   /* Enable mouse wheel (Fingers Setup) */
        const fg_ctl *c = &lj_fg_ctl[i];
        if (c->kind == K_FADER) lj_fingers_fader(c->idx + 1, fg.fader[c->idx + 1] + m->wheel);
        else if (c->kind == K_GM) lj_fingers_gm(fg.fader[0] + m->wheel);
        else if (c->kind == K_JOG) lj_fingers_jog(c->idx + 1, m->wheel);
    }
    repaint();
}

/* ================= the status panel (TFingersStatus, code-drawn over the 451 x 567 client) ================= */
static struct { lj_window *win; lj_widget *pb; int pop_f; } st;
static void st_paint(lj_widget *w, lj_surface *s)
{
    const lj_font *f = &lj_font_sans13;
    char t[64], lab[48];
    int ax, ay, i, y;
    (void)w;
    lj_widget_abs(st.pb, &ax, &ay);
    lj_fill(s, ax, ay, st.pb->w, st.pb->h, LJ_CLR_BTNFACE);
    for (i = 0; i < LJ_FG_FKEYS; i++) {                               /* F1-F10 of the current shift, the colour code of 05 s5.2 */
        const lj_da_val *v = &fg.fkey[fg.shift][i];
        y = ay + 4 + i * 16;
        snprintf(t, sizeof t, "F%d", i + 1);
        lj_text(s, f, ax + 4, y + 1, t, 0x000000u, 0);
        lj_edge_sunken(s, ax + 30, y, 200, 16, v->type ? lj_fg_type_rgb[v->type] : 0x000000u);
        if (v->type) lj_text(s, f, ax + 33, y + 1, lj_da2532_label(v, lab, sizeof lab), 0x000000u, 0);
    }
    y = ay + 172;
    for (i = 0; i < LJ_FG_JOGS; i++) {
        snprintf(t, sizeof t, "DATA %d", i + 1);
        lj_text(s, f, ax + 4, y + i * 16 + 1, t, 0x000000u, 0);
        lj_edge_sunken(s, ax + 52, y + i * 16, 178, 16, 0x000000u);
        lj_text(s, f, ax + 55, y + i * 16 + 1, lj_fingers_jog_functions[fg.jog_fn[i]], 0xFFFFFFu, 0);
    }
    snprintf(t, sizeof t, "PAGE %c   SHIFT %d", 'A' + fg.page, fg.shift);
    lj_text(s, f, ax + 240, y + 1, t, 0x000000u, 0);
    y = ay + 260;                                                     /* the faders: page function, 12 bars + GM with values */
    lj_text(s, f, ax + 4, y, lj_fingers_page_functions[fg.page_fn[fg.page]], 0x000000u, 0);
    for (i = 0; i <= LJ_FG_FADERS; i++) {
        int x = ax + 4 + i * 34, h = 140, v = fg.fader[i == LJ_FG_FADERS ? 0 : i + 1];
        if (i == LJ_FG_FADERS) snprintf(t, sizeof t, "GM"); else snprintf(t, sizeof t, "%d", i + 1);
        ctext(s, f, x, y + 16, 30, f->height, t, 0x000000u);
        lj_edge_sunken(s, x, y + 30, 30, h, 0x000080u);
        lj_fill(s, x + 2, y + 30 + 2 + (h - 4) * (100 - v) / 100, 26, (h - 4) * v / 100, i == LJ_FG_FADERS ? 0x8080FFu : 0x4040C0u);
        snprintf(t, sizeof t, "%d", v);
        ctext(s, f, x, y + 30 + h + 2, 30, f->height, t, 0x000000u);
    }
    y = ay + 460;                                                     /* the slot rows and the scroll set */
    for (i = 0; i < LJ_FG_ROWS; i++) {
        lj_edge_sunken(s, ax + 4, y + i * 18, 300, 17, 0x000000u);
        lj_text(s, f, ax + 7, y + i * 18 + 2, lj_fingers_slot_functions[fg.row_fn[i]], 0xFFFFFFu, 0);
    }
    lj_edge_sunken(s, ax + 4, y + 58, 300, 17, 0x000000u);
    lj_text(s, f, ax + 7, y + 60, lj_fingers_ud_functions[fg.ud_fn], 0xFFFFFFu, 0);
}
static void popup_at(lj_window *win, const char *menu, int x, int y)
{
    lj_menu *m = lj_window_menus(win);
    int sx = 0, sy = 0;
    if (!m) return;
    if (m->host && m->host->screen_pos) m->host->screen_pos(m->host->ud, win, &sx, &sy);
    lj_menu_popup(m, lj_form_find_menu(lj_window_desc(win), menu), sx + x, sy + y);
}
static void st_event(lj_widget *w, int ev, void *ud)
{
    const lj_mouse_ev *m = lj_window_last_event(st.win);
    int ax, ay, x, y;
    (void)ud;
    if (!m || ev != LJ_WEV_MOUSE_UP || m->button != 2) return;
    lj_widget_abs(w, &ax, &ay);
    x = m->x - ax; y = m->y - ay;
    if (y < 164) { st.pop_f = (y - 4) / 16; popup_at(st.win, "funcPopUp", m->x, m->y); }   /* the F rows */
    else if (y < 256) popup_at(st.win, "JogPopup", m->x, m->y);
    else if (y < 456) popup_at(st.win, "pagePopup", m->x, m->y);
    else if (y < 514) popup_at(st.win, "SlotPopUp", m->x, m->y);
    else popup_at(st.win, "udPopUp", m->x, m->y);
    (void)x;
}
static const char *st_item(void) { const lj_menu_item_desc *it = st.win ? lj_window_menu_item(st.win) : NULL; return it && it->name ? it->name : ""; }
static void st_func_pop(lj_widget *w, void *ud)
{
    const char *n = st_item();
    lj_ctx *c = lj_ctx_get();
    int k = st.pop_f < 0 ? 0 : st.pop_f >= LJ_FG_FKEYS ? LJ_FG_FKEYS - 1 : st.pop_f;
    (void)w; (void)ud;
    if (!strcmp(n, "M_FuncShowFunctionList")) lj_win_open(LJ_WIN_FINGERS_FUNCTIONS);
    else if (!strcmp(n, "M_FAssignSequence")) lj_fingers_assign_fkey(fg.shift, k + 1, LJ_DA_SEQ, c->show && c->show->current_seq ? c->show->current_seq->number : 0);
    else if (!strcmp(n, "M_FAssignCue")) lj_fingers_assign_fkey(fg.shift, k + 1, LJ_DA_CUE, c->view.cue_number);
    else if (!strcmp(n, "M_FAssignBGCue")) lj_fingers_assign_fkey(fg.shift, k + 1, LJ_DA_BG, c->view.bg_number);
    else if (!strcmp(n, "M_FAssignCuelist")) lj_fingers_assign_fkey(fg.shift, k + 1, LJ_DA_CL, c->cl ? c->cl->number : 0);
    else if (!strcmp(n, "M_funcClearButton")) lj_fingers_assign_fkey(fg.shift, k + 1, LJ_DA_NONE, 0);
    else if (!strcmp(n, "M_FuncClearAll")) memset(fg.fkey[fg.shift], 0, sizeof fg.fkey[fg.shift]);
    else if (!strcmp(n, "M_FuncPopHelp")) { lj_desktop_run_action(LJ_ACT_HELP, 0); return; }
    lj_fingers_save();
    repaint();
}
static void st_list_pop(lj_widget *w, void *ud)   /* Show Function List / Clear / Clear All of the page, jog, slot and scroll popups */
{
    const char *n = st_item();
    int k;
    (void)w; (void)ud;
    if (strstr(n, "Help")) { lj_desktop_run_action(LJ_ACT_HELP, 0); return; }
    if (!strncmp(n, "M_Page", 6) || !strncmp(n, "M_page", 6)) {
        if (strstr(n, "ShowFunctionList")) lj_win_open(LJ_WIN_FINGERS_PAGE_FUNCTIONS); else if (strstr(n, "ClearAll")) for (k = 0; k < LJ_FG_PAGES; k++) fg.page_fn[k] = 0; else fg.page_fn[fg.page] = 0;
    } else if (!strncmp(n, "M_Jog", 5)) {
        if (strstr(n, "ShowFunctionList")) lj_win_open(LJ_WIN_FINGERS_JOG_FUNCTIONS); else if (strstr(n, "ClearAll")) for (k = 0; k < LJ_FG_JOGS; k++) fg.jog_fn[k] = 0; else fg.jog_fn[0] = 0;
    } else if (!strncmp(n, "M_slot", 6)) {
        if (strstr(n, "ShowFunctionList")) lj_win_open(LJ_WIN_FINGERS_SLOT_FUNCTIONS); else if (strstr(n, "ClearAll")) for (k = 0; k < LJ_FG_ROWS; k++) fg.row_fn[k] = 0; else fg.row_fn[0] = 0;
    } else if (!strncmp(n, "M_ud", 4) || !strncmp(n, "M_Ud", 4)) {
        if (strstr(n, "ShowFunctionlist")) lj_win_open(LJ_WIN_FINGERS_UD_FUNCTIONS); else fg.ud_fn = 0;
    }
    lj_fingers_save();
    repaint();
}

/* ================= the generated dialogs ================= */
static lj_window *lj_fg_sub[LJ_WIN_FINGERS_UD_FUNCTIONS - LJ_WIN_FINGERS + 1];   /* index id - LJ_WIN_FINGERS; [0] vf.win, [2] st.win use their own structs */
static lj_window **sub(enum lj_win id) { return &lj_fg_sub[id - LJ_WIN_FINGERS]; }
static void close_sub(enum lj_win id) { lj_window **w = sub(id); if (!*w) return; lj_win_hide(*w); lj_window_destroy(*w); *w = NULL; }
static void close_owner(lj_widget *w)
{
    lj_window *win = lj_window_of(w);
    int id;
    for (id = LJ_WIN_FINGERS; id <= LJ_WIN_FINGERS_UD_FUNCTIONS; id++) if ((id == LJ_WIN_FINGERS ? vf.win : id == LJ_WIN_FINGERS_STATUS ? st.win : *sub((enum lj_win)id)) == win) { lj_fingers_close((enum lj_win)id); return; }
}
static void on_close(lj_widget *w, void *ud) { (void)ud; close_owner(w); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_desktop_run_action(LJ_ACT_HELP, 0); }

/* Fingers Setup: the three tabs over Fingers.Ini [SETUP] / [PREFS] / [VIRTUAL] (keys = the control names, PROPOSAL rig 05 s5) */
static const char *const lj_fg_prefs[] = { "cbShiftLatch", "cbFuncPageSelect", "cbFaderShiftSelect", "CbFaderPageSelect", "cbJogShiftSelect", "cbJogPageSelect",
    "cbUDShiftSelect", "cbUDPageSelect", "cbLargeStatus", "cbStatusStayOnTop", "cbShowOnStartup", "cbShowTopPanel", "cbShowMiddlePanel", "cbShowBottomPanel",
    "cbMultiSelectDisable", "cbDisableSlotFeedback", "cbSelectCueslot", NULL };
static const char *const lj_fg_virt[] = { "cbVirtualOnTop", "cbShowFaderHints", "cbMouseWheel", "cbShowWheelHints", "cbShowHotZones", NULL };
static const char *const lj_fg_virt_key[] = { "OnTop", "FaderHints", "MouseWheel", "WheelHints", "HotZones", NULL };
static void setup_load(lj_window *win)
{
    int i;
    lj_widget *rg = lj_window_find(win, "rgPort"), *r;
    char t[8];
    ensure_loaded();
    for (i = 0; lj_fg_prefs[i]; i++) lj_w_checkbox_set_checked(lj_window_find(win, lj_fg_prefs[i]), seti("PREFS", lj_fg_prefs[i], 0));
    for (i = 0; lj_fg_virt[i]; i++) lj_w_checkbox_set_checked(lj_window_find(win, lj_fg_virt[i]), seti("VIRTUAL", lj_fg_virt_key[i], 0));
    snprintf(t, sizeof t, "%d", seti("SETUP", "FlickerThreshold", 4)); lj_w_edit_set_text(lj_window_find(win, "eFaderFilter"), t);   /* default 4 (05 s5.2) */
    lj_w_edit_set_text(lj_window_find(win, "eVirtualSkinFile"), lj_ini_gets(fg_ini, "VIRTUAL", "SkinFile", "Default"));
    if (rg) {
        lj_w_radiogroup_set_index(rg, seti("SETUP", "Mode", LJ_FG_DISABLED));
        for (i = 0, r = rg->first; r; r = r->next, i++) if (i == 2) { r->enabled = 0; r->hint = LJ_FG_NA_HINT; }   /* Serial connection (RS-232): never (D16) */
    }
    if ((r = lj_window_find(win, "cbPort")) != NULL) { r->enabled = 0; r->hint = LJ_FG_NA_HINT; }
    if ((r = lj_window_find(win, "StaticText1")) != NULL) r->enabled = 0;
}
static void setup_ok(lj_widget *w, void *ud)
{
    lj_window *win = lj_window_of(w);
    const char *t;
    int i, v, mode = lj_w_radiogroup_index(lj_window_find(win, "rgPort"));
    (void)ud;
    ensure_loaded();
    for (i = 0; lj_fg_prefs[i]; i++) lj_ini_seti(fg_ini, "PREFS", lj_fg_prefs[i], lj_w_checkbox_checked(lj_window_find(win, lj_fg_prefs[i])));
    for (i = 0; lj_fg_virt[i]; i++) lj_ini_seti(fg_ini, "VIRTUAL", lj_fg_virt_key[i], lj_w_checkbox_checked(lj_window_find(win, lj_fg_virt[i])));
    t = lj_w_edit_text(lj_window_find(win, "eFaderFilter"));
    v = t ? atoi(t) : 4;
    lj_ini_seti(fg_ini, "SETUP", "FlickerThreshold", v < 2 ? 2 : v > 4 ? 4 : v);   /* "(2-4)" */
    lj_ini_seti(fg_ini, "SETUP", "Mode", mode == LJ_FG_VIRTUAL ? LJ_FG_VIRTUAL : LJ_FG_DISABLED);
    lj_fingers_save();
    close_sub(LJ_WIN_FINGERS_SETUP);
}
static void setup_default(lj_widget *w, void *ud)   /* "Set Default = only fader/slot page select and scroll button shift select on" (05 s5.3) */
{
    lj_window *win = lj_window_of(w);
    static const char *const box[] = { "cbFuncPageSelect", "cbFaderShiftSelect", "CbFaderPageSelect", "cbJogShiftSelect", "cbJogPageSelect", "cbUDShiftSelect", "cbUDPageSelect", NULL };
    int i;
    (void)ud;
    for (i = 0; box[i]; i++) lj_w_checkbox_set_checked(lj_window_find(win, box[i]), !strcmp(box[i], "CbFaderPageSelect") || !strcmp(box[i], "cbUDShiftSelect"));
}
static void setup_port(lj_widget *w, void *ud) { (void)ud; if (lj_w_radiogroup_index(w) == LJ_FG_SERIAL) lj_w_radiogroup_set_index(w, LJ_FG_VIRTUAL); }
static void setup_folder(lj_widget *w, void *ud) { (void)w; (void)ud; lj_win_open(LJ_WIN_FINGERS_SKIN); }
static void setup_hotzone(lj_widget *w, void *ud) { (void)ud; w->color = w->color == 0x00FF00u ? 0xFFFF00u : 0x00FF00u; lj_widget_invalidate(w); }   /* ponytail: no TColorDialog */
static void setup_cancel(lj_widget *w, int ev, void *ud) { (void)w; (void)ud; if (ev == LJ_WEV_CLICK) close_sub(LJ_WIN_FINGERS_SETUP); }

/* Error box: pError carries the exe's message; Ignore closes, Fingers Setup opens the setup */
static char lj_fg_err[96];
static void err_close(lj_widget *w, void *ud) { (void)w; (void)ud; close_sub(LJ_WIN_FINGERS_ERROR); }
static void err_setup(lj_widget *w, void *ud) { (void)w; (void)ud; close_sub(LJ_WIN_FINGERS_ERROR); lj_win_open(LJ_WIN_FINGERS_SETUP); }

/* Select Skin: our one drawn surface is the only skin */
static const char *const lj_fg_skins[] = { "Default\t" };
static void skin_ok(lj_widget *w, int ev, void *ud)
{
    (void)w;
    if (ev != LJ_WEV_CLICK) return;
    if (ud) { ensure_loaded(); lj_ini_sets(fg_ini, "VIRTUAL", "SkinFile", "Default"); lj_fingers_save(); }
    close_sub(LJ_WIN_FINGERS_SKIN);
}
static void skin_select(lj_widget *w, void *ud) { lj_window *win = lj_window_of(w); (void)ud; lj_w_edit_set_text(lj_window_find(win, "eSkinName"), "Default"); lj_w_edit_set_text(lj_window_find(win, "eAuthor"), "LJ4EVR"); }

/* the five function lists: the catalogue in lvFunctions; a click assigns to what the status popup aimed at (F key: the function
 * list of hotkey functions; slot list: the tcData row; page / jog / scroll: the current page / DATA 1 / the set) */
static struct { uint16_t fn[LJ_FN_COUNT]; char cap[LJ_FN_COUNT][64]; const char *items[LJ_FN_COUNT]; int n; } lj_fg_fl;
static const char *lj_fg_cat_items[32];
static void list_fill(lj_widget *lv, const char *const *cat)
{
    int i;
    for (i = 0; cat[i] && i < 31; i++) lj_fg_cat_items[i] = cat[i];
    if (lv) lj_w_listview_set_items(lv, lj_fg_cat_items, i);
}
static void list_event(lj_widget *w, int ev, void *ud)
{
    lj_window *win = lj_window_of(w);
    int i = lj_w_listview_index(w), id = (int)(intptr_t)ud;
    if (ev != LJ_WEV_CLICK || i < 0) return;
    if (id == LJ_WIN_FINGERS_FUNCTIONS) { if (i < lj_fg_fl.n) lj_fingers_assign_fkey(fg.shift, st.pop_f + 1, LJ_DA_FN, lj_fg_fl.fn[i]); }
    else if (id == LJ_WIN_FINGERS_SLOT_FUNCTIONS) { lj_widget *tc = lj_window_find(win, "tcData"); lj_fingers_set_row_function(tc ? ((lj_w_tabs *)tc)->active + 1 : 1, i); }
    else if (id == LJ_WIN_FINGERS_PAGE_FUNCTIONS) lj_fingers_set_page_function(fg.page + 1, i);
    else if (id == LJ_WIN_FINGERS_JOG_FUNCTIONS) lj_fingers_set_jog_function(1, i);
    else if (id == LJ_WIN_FINGERS_UD_FUNCTIONS && i < count_of(lj_fingers_ud_functions)) fg.ud_fn = i;
    lj_fingers_save();
    repaint();
}
static void fill_lists(lj_window *win, enum lj_win id)
{
    lj_widget *lv = lj_window_find(win, "lvFunctions");
    int f;
    if (id == LJ_WIN_FINGERS_FUNCTIONS) {                             /* F1-F10: LJ2's hotkey functions (programs come from Assign current ...) */
        for (lj_fg_fl.n = 0, f = 1; f < LJ_FN_COUNT; f++) {
            if (lj_fn_table[f].group == LJ_FNG_PROGRAMS || (lj_fn_table[f].flags & LJ_FNF_EXT)) continue;
            lj_fg_fl.fn[lj_fg_fl.n] = (uint16_t)f; lj_fn_caption((enum lj_fn)f, lj_fg_fl.cap[lj_fg_fl.n], 64); lj_fg_fl.items[lj_fg_fl.n] = lj_fg_fl.cap[lj_fg_fl.n]; lj_fg_fl.n++;
        }
        if (lv) lj_w_listview_set_items(lv, lj_fg_fl.items, lj_fg_fl.n);
    } else list_fill(lv, id == LJ_WIN_FINGERS_SLOT_FUNCTIONS ? lj_fingers_slot_functions : id == LJ_WIN_FINGERS_PAGE_FUNCTIONS ? lj_fingers_page_functions
                         : id == LJ_WIN_FINGERS_JOG_FUNCTIONS ? lj_fingers_jog_functions : lj_fingers_ud_functions);
    if (lv) { lv->on_event = list_event; lv->ud = (void *)(intptr_t)id; }
}
static struct { char row[LJ_FG_SLOTS][40]; const char *rows[LJ_FG_SLOTS]; } lj_fg_sd;
static void slot_data(lj_widget *w, void *ud)   /* tcData: the 12 slots of the chosen row with that row's function */
{
    lj_window *win = lj_window_of(w);
    lj_widget *tc = lj_window_find(win, "tcData"), *lv = lj_window_find(win, "lvData");
    int r = tc ? ((lj_w_tabs *)tc)->active : 0, i;
    (void)ud;
    if (r < 0 || r >= LJ_FG_ROWS) r = 0;
    for (i = 0; i < LJ_FG_SLOTS; i++) { snprintf(lj_fg_sd.row[i], sizeof lj_fg_sd.row[i], "%d\t%s\t", i + 1, lj_fingers_slot_functions[fg.row_fn[r]]); lj_fg_sd.rows[i] = lj_fg_sd.row[i]; }
    if (lv) { lj_w_listview_set_items(lv, lj_fg_sd.rows, LJ_FG_SLOTS); lj_widget_invalidate(lv); }
}

static const lj_binding lj_fg_vf_b[] = { { "FormActivate", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "FormMouseDown", lj_handler_noop, NULL }, { "FormMouseMove", lj_handler_noop, NULL }, { "FormMouseUp", lj_handler_noop, NULL } /* the paint box over the client takes the mouse */,
    { "FormMouseWheel", lj_handler_noop, NULL }, { "FormPaint", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_st_b[] = { { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "funcPopUpPopup", lj_handler_noop, NULL }, { "M_funcPopClick", st_func_pop, NULL }, { "M_PagePopClick", st_list_pop, NULL }, { "PopJogClick", st_list_pop, NULL },
    { "PopSlotClick", st_list_pop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { "UdPopClick", st_list_pop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_su_b[] = { { "bHelpClick", on_help, NULL }, { "bOkClick", setup_ok, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "pHotZoneClick", setup_hotzone, NULL }, { "rgPortClick", setup_port, NULL }, { "sbFolderClick", setup_folder, NULL }, { "sbSetDefaultClick", setup_default, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_er_b[] = { { "bCloseClick", err_close, NULL }, { "bSetupClick", err_setup, NULL }, { "FormClose", on_close, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_sk_b[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "lv1Change", skin_select, NULL }, { "lv1Click", skin_select, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_fl_b[] = { { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "helpClick", on_help, NULL }, { "lvFunctionsCustomDrawItem", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_jog_b[] = { { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_page_b[] = { { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "HelpClick", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_ud_b[] = { { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "Help1Click", on_help, NULL }, { NULL, NULL, NULL } };
static const lj_binding lj_fg_slot_b[] = { { "cbChange", lj_handler_noop, NULL }, { "FormClose", on_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL },
    { "Help1Click", on_help, NULL }, { "lvDataData", lj_handler_noop, NULL }, { "lvDataDragDrop", lj_handler_noop, NULL }, { "lvDataDragOver", lj_handler_noop, NULL },
    { "lvDataSelectItem", lj_handler_noop, NULL }, { "tcDataChange", slot_data, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };

static lj_widget *cover(lj_window *win, const char *name, void (*ev)(lj_widget *, int, void *), void (*paint)(lj_widget *, lj_surface *), lj_widget_vt *vt)
{
    static const lj_ctl_desc vf_desc = { LJ_K_PAINTBOX, "pbFingers", 0, 0, 0, 0, .tag = 0 }, st_desc = { LJ_K_PAINTBOX, "pbStatus", 0, 0, 0, 0, .tag = 0 };
    lj_widget *root = lj_window_root(win), *pb = lj_w_paintbox_create(root, !strcmp(name, "pbFingers") ? &vf_desc : &st_desc);
    if (!pb) return NULL;
    pb->x = 0; pb->y = 0; pb->w = root->w; pb->h = root->h;
    *vt = *pb->vt; vt->paint = paint; pb->vt = vt;
    pb->on_event = ev;
    return pb;
}
static lj_widget_vt lj_fg_vf_vt, lj_fg_st_vt;

static void open_answer(int yes, void *ud)
{
    (void)ud;
    if (!yes) return;
    ensure_loaded();
    lj_ini_seti(fg_ini, "SETUP", "Mode", LJ_FG_VIRTUAL);
    lj_fingers_save();
    lj_fingers_open(LJ_WIN_FINGERS);
}
static lj_window *open_vf(void)
{
    if (vf.win) return vf.win;
    if (lj_fingers_mode() != LJ_FG_VIRTUAL) { lj_win_confirm("Confirm", "Fingers is disabled - do you wish to activate now ?", "", open_answer, NULL); return NULL; }   /* exe 0x2343B0; caption PROPOSAL */
    {
        const char *skin = lj_ini_gets(fg_ini, "VIRTUAL", "SkinFile", "Default");
        if (strcmp(skin, "Default") != 0) { snprintf(lj_fg_err, sizeof lj_fg_err, "Error locating Fingers skin file - Reverting to default skin"); lj_fingers_open(LJ_WIN_FINGERS_ERROR); }   /* exe 0x203F54 */
    }
    build_layout();
    if ((vf.win = lj_window_create(&lj_form_TLJFINGERSMAINFORM, lj_fg_vf_b)) == NULL) { lj_log("fingers: cannot create TLJFingersMainForm"); return NULL; }
    vf.pb = cover(vf.win, "pbFingers", vf_event, vf_paint, &lj_fg_vf_vt);
    lj_win_show(vf.win, seti("VIRTUAL", "OnTop", 0) ? LJ_FG_STAY_ON_TOP : 0u);
    return vf.win;
}
static lj_window *open_status(void)
{
    if (st.win) return st.win;
    ensure_loaded();
    if ((st.win = lj_window_create(&lj_form_TFINGERSSTATUS, lj_fg_st_b)) == NULL) { lj_log("fingers: cannot create TFingersStatus"); return NULL; }
    st.pb = cover(st.win, "pbStatus", st_event, st_paint, &lj_fg_st_vt);
    lj_win_show(st.win, seti("PREFS", "cbStatusStayOnTop", 0) ? LJ_FG_STAY_ON_TOP : 0u);
    return st.win;
}
static void repaint(void)
{
    if (vf.pb) lj_widget_invalidate(vf.pb);
    if (st.pb) lj_widget_invalidate(st.pb);
}

lj_window *lj_fingers_open(enum lj_win id)
{
    static int yes = 1;
    lj_window **w;
    if (id == LJ_WIN_FINGERS) return open_vf();
    if (id == LJ_WIN_FINGERS_STATUS) return open_status();
    if (id < LJ_WIN_FINGERS || id > LJ_WIN_FINGERS_UD_FUNCTIONS) return NULL;
    if (*(w = sub(id))) return *w;
    ensure_loaded();
    switch (id) {
    case LJ_WIN_FINGERS_SETUP:
        if ((*w = lj_window_create(&lj_form_TFINGERSSETUPDIALOG, lj_fg_su_b)) != NULL) {
            lj_widget *b = lj_window_find(*w, "bCancel");
            setup_load(*w);
            if (b) { b->on_event = setup_cancel; }                     /* bCancel: no OnClick in the DFM */
        }
        break;
    case LJ_WIN_FINGERS_ERROR:
        if ((*w = lj_window_create(&lj_form_TFINGERSERRORFORM, lj_fg_er_b)) != NULL) lj_window_set_text(*w, "pError", lj_fg_err[0] ? lj_fg_err : "Fingers connect error (no connection found)");
        break;
    case LJ_WIN_FINGERS_SKIN:
        if ((*w = lj_window_create(&lj_form_TFINGERSSKINSELECTFORM, lj_fg_sk_b)) != NULL) {
            lj_widget *b;
            lj_w_listview_set_items(lj_window_find(*w, "lv1"), lj_fg_skins, 1);
            if ((b = lj_window_find(*w, "bOk")) != NULL) { b->on_event = skin_ok; b->ud = &yes; }
            if ((b = lj_window_find(*w, "BitBtn2")) != NULL) { b->on_event = skin_ok; b->ud = NULL; }
        }
        break;
    case LJ_WIN_FINGERS_FUNCTIONS:      *w = lj_window_create(&lj_form_TFINGERSFUNCTIONLISTFORM, lj_fg_fl_b); break;
    case LJ_WIN_FINGERS_JOG_FUNCTIONS:  *w = lj_window_create(&lj_form_TFINGERSJOGFUNCTIONLISTFORM, lj_fg_jog_b); break;
    case LJ_WIN_FINGERS_PAGE_FUNCTIONS: *w = lj_window_create(&lj_form_TFINGERSPAGEFUNCTIONLISTFORM, lj_fg_page_b); break;
    case LJ_WIN_FINGERS_SLOT_FUNCTIONS: *w = lj_window_create(&lj_form_TFINGERSSLOTFUNCTIONLISTFORM, lj_fg_slot_b);
        if (*w) { lj_widget *pc = lj_window_find(*w, "pcParams"); if (pc) lj_w_tabs_select(pc, 0); slot_data(lj_window_root(*w), NULL); }   /* tsNoData until a slot is chosen */
        break;
    case LJ_WIN_FINGERS_UD_FUNCTIONS:   *w = lj_window_create(&lj_form_TFINGERSUDFUNCTIONLISTFORM, lj_fg_ud_b); break;
    default: return NULL;
    }
    if (!*w) { lj_log("fingers: cannot create %s", lj_win_class(id)); return NULL; }
    if (id >= LJ_WIN_FINGERS_FUNCTIONS) fill_lists(*w, id);
    lj_win_show(*w, (id == LJ_WIN_FINGERS_SETUP || id == LJ_WIN_FINGERS_SKIN) ? 0u : LJ_FG_STAY_ON_TOP);   /* fsStayOnTop in the error box and the five lists */
    return *w;
}
void lj_fingers_close(enum lj_win id)
{
    lj_window **w = NULL;
    if (id == LJ_WIN_FINGERS) { if (!vf.win) return; lj_win_hide(vf.win); lj_window_destroy(vf.win); vf.win = NULL; vf.pb = NULL; vf.drag = 0; return; }
    if (id == LJ_WIN_FINGERS_STATUS) { if (!st.win) return; lj_win_hide(st.win); lj_window_destroy(st.win); st.win = NULL; st.pb = NULL; repaint(); return; }
    if (id > LJ_WIN_FINGERS && id <= LJ_WIN_FINGERS_UD_FUNCTIONS) w = sub(id);
    if (w && *w) close_sub(id);
}
lj_window *lj_fingers_get(enum lj_win id)
{
    if (id == LJ_WIN_FINGERS) return vf.win;
    if (id == LJ_WIN_FINGERS_STATUS) return st.win;
    return id > LJ_WIN_FINGERS && id <= LJ_WIN_FINGERS_UD_FUNCTIONS ? *sub(id) : NULL;
}
void lj_fingers_refresh(enum lj_win id)
{
    learn_poll();
    if (id == LJ_WIN_FINGERS || id == LJ_WIN_FINGERS_STATUS) {
        int gm = (lj_ctx_get()->view.gm * 100 + LJ_GM_DOMAIN / 2) / LJ_GM_DOMAIN;   /* the GM fader follows the engine (the Master Intensity window may move it) */
        if (lj_ctx_get()->view.tick_no && gm != fg.fader[0]) { fg.fader[0] = gm; repaint(); }
    }
}

/* ---- the hotkey rows: Virtual Fingers, Fingers status panel and the EXT rows ---- */
static void fn_fingers(enum lj_fn fn, int edge, uint32_t prog, void *ud)
{
    int p = (int)(prog > 100 ? 100 : prog);
    (void)ud;
    if (fn == LJ_FN_VIRTUAL_FINGERS || fn == LJ_FN_FINGERS_STATUS_PANEL) {
        enum lj_win id = fn == LJ_FN_VIRTUAL_FINGERS ? LJ_WIN_FINGERS : LJ_WIN_FINGERS_STATUS;
        if (edge) { if (lj_fingers_get(id)) lj_fingers_close(id); else lj_fingers_open(id); }
        repaint();
        return;
    }
    if (fn >= LJ_FN_FINGERS_F_1 && fn <= LJ_FN_FINGERS_F_10) lj_fingers_fkey((int)(fn - LJ_FN_FINGERS_F_1) + 1, edge);
    else if (fn >= LJ_FN_FINGERS_SLOT_TOP_1 && fn <= LJ_FN_FINGERS_SLOT_TOP_12) lj_fingers_slot(1, (int)(fn - LJ_FN_FINGERS_SLOT_TOP_1) + 1, edge);
    else if (fn >= LJ_FN_FINGERS_SLOT_MID_1 && fn <= LJ_FN_FINGERS_SLOT_MID_12) lj_fingers_slot(2, (int)(fn - LJ_FN_FINGERS_SLOT_MID_1) + 1, edge);
    else if (fn >= LJ_FN_FINGERS_SLOT_BOT_1 && fn <= LJ_FN_FINGERS_SLOT_BOT_12) lj_fingers_slot(3, (int)(fn - LJ_FN_FINGERS_SLOT_BOT_1) + 1, edge);
    else if (fn >= LJ_FN_FINGERS_BUMP_1 && fn <= LJ_FN_FINGERS_BUMP_12) lj_fingers_bump((int)(fn - LJ_FN_FINGERS_BUMP_1) + 1, edge);
    else if (!edge) return;
    else if (fn >= LJ_FN_FINGERS_FADER_1 && fn <= LJ_FN_FINGERS_FADER_12) lj_fingers_fader((int)(fn - LJ_FN_FINGERS_FADER_1) + 1, p);
    else if (fn == LJ_FN_FINGERS_GM) lj_fingers_gm(p);
    else if (fn == LJ_FN_FINGERS_BO) lj_fingers_bo();
    else if (fn >= LJ_FN_FINGERS_JOG_1 && fn <= LJ_FN_FINGERS_JOG_5) {   /* a level row: the move from the last value the wheel brought is the step */
        int j = (int)(fn - LJ_FN_FINGERS_JOG_1);
        if (fg.jog_last[j] >= 0 && p != fg.jog_last[j]) lj_fingers_jog(j + 1, p - fg.jog_last[j]);
        fg.jog_last[j] = p;
    }
}

void lj_fingers_bind_fns(void)
{
    int f;
    lj_fn_bind(LJ_FN_VIRTUAL_FINGERS, fn_fingers, NULL);
    lj_fn_bind(LJ_FN_FINGERS_STATUS_PANEL, fn_fingers, NULL);
    for (f = LJ_FN_FINGERS_F_1; f <= LJ_FN_FINGERS_JOG_5; f++) lj_fn_bind((enum lj_fn)f, fn_fingers, NULL);
}
