/* lj_replay.c - the replay harness (lj_replay.h, docs/replay-grammar.md, design 3.7).
 * One pass over the .lj4r file: build lines fill an lj_show through the model API
 * the way the UI thread would (records allocated with lj_*_new_rec, scenes added
 * with lj_sequence_add, numbers inserted into the lj_tables, every record through
 * its lj_*_validate before the engine sees it); the first event line creates and
 * binds the engine; `at k` posts an lj_cmd before tick k, `expect k` runs ticks up to
 * k and compares the frame. Event lines are in tick order. Lines are read here, not
 * through lj_textrec: the FAIL message needs the line number and lj_rec keeps it
 * private. SDL-free and float-free like the engine. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/sscanf under /W4 /WX on MSVC (as gfx/lj_bmp.c) */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/lj_replay.h"
#include "core/lj_clock.h"
#include "core/lj_str.h"
#include "engine/lj_engine.h"
#include "engine/lj_engine_rules.h"
#include "fixlib/lj_profile.h"
#include "io/lj_sacn.h"
#include "model/lj_bgcue.h"
#include "model/lj_cue.h"
#include "model/lj_model.h"
#include "model/lj_patch.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "app/lj_cuelist_run.h"   /* W3 cuelist-core: cl / clline, at k cl_*, expect_cl */
#include "model/lj_cuelist.h"
#include "model/lj_followspot.h"   /* W3 gdc-fades-followspot: follow / gdcgroup, at k followspot|gdc|seqfades */
#include "model/lj_gdc.h"
#include "model/lj_kv.h"
#include "model/lj_seqfades.h"
#include "model/lj_movemacro.h"   /* W3 movement: bez / node, at k macro_fade|fx_edit|fanout|spread|autodelay, expect_mm */
#include "model/lj_statics.h"
#include "model/lj_table.h"
#include "model/lj_words.h"

#if defined(__SANITIZE_ADDRESS__)
#define RP_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define RP_SANITIZED 1
#endif
#endif
#ifndef RP_SANITIZED
#define RP_SANITIZED 0
#endif

enum { RP_LINE_MAX = 8192, RP_TOK_MAX = 512, RP_RECS = 64 };

typedef struct { int line; void *rec; } rp_rec;

typedef struct {
    const char *path; char name[64]; int line;
    lj_show *show; lj_engine *e; const lj_frame *f; lj_engine_status view;
    uint32_t tick_us; int built, tick;             /* tick = ticks run so far */
    long edit_tick;                                /* `at k gpatch|default`: the tick the runtime edit lands before (W2) */
    uint8_t patched[LJ_CHANS];                     /* inside a patched fixture's footprint */
    lj_profile *prof[LJ_FIXTURES];                 /* `profile n` overrides handed to lj_engine_bind_profile */
    rp_rec seqs[RP_RECS], cues[RP_RECS], bgs[RP_RECS], macros[RP_RECS]; int nseq, ncue, nbg, nmacro;
    lj_sequence *seq; int nscene;                  /* current sequence and its scene count */
    lj_scene *scene;                               /* target of ch/fx lines: the current scene or a static slot's */
    lj_cue *cue; lj_bgcue *bg; lj_genmacro *macro;
    const void *sacn_posted;
    int dmx_in_live;                               /* ME-190: the scenario has an `at k dmx_in` line, so its input is enabled from tick 0 */
    lj_sacn_cfg sacn;
    FILE *frames; lj_replay_result *r;
    char line_buf[RP_LINE_MAX + 2];
} rp;

static const char *const rp_class_words[]  = { "none", "intensity", "shutter", "strobe", "pan", "tilt", "color", "gobo", "beam", "effect",
                                               "level", "speed", "lamp", "special", "switch", "smoke", "haze", "haze_fan", NULL };   /* enum lj_ch_class order */
static const char *const rp_master_words[] = { "prop", "lotp", "htp", "none", NULL };       /* enum lj_master_mode */
static const char *const rp_seqint_words[] = { "default", "htp", "lotp", NULL };            /* enum lj_seq_int_mode */
/* W2 (replay track fills the bodies that need more than a post): */
static const char *const rp_transp_words[] = { "use", "force", "ignore", NULL };            /* enum lj_transparent_mode */
static const char *const rp_smoke_words[]  = { "button", "timer", "level", "duration", "cycle", "reset", "haze", "fan", NULL };   /* enum lj_smoke_ctl */
static const char *const rp_pref_words[]   = { "keep_seq_blind", "global_chase_rate", "disable_chase_fade", "statics_fadetime", "hotkey_trig_dir", NULL };   /* enum lj_engine_pref */
static const char *const rp_status_words[] = { "empty", "running", "stopped", "load_error", "disabled", "edit", "out", NULL };   /* LJ_SLOT_* */
static const char *const rp_pending_words[] = { "link_cue", "bg", "genmacro", NULL };

/* ---- diagnostics ---- */
static int rp_fail(rp *p, const char *fmt, ...)
{
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    printf("FAIL %s:%d %s\n", p->path, p->line, msg);
    if (p->r->fails++ == 0) snprintf(p->r->first_fail, sizeof p->r->first_fail, "line %d: %s", p->line, msg);
    return -1;
}
static int rp_bad(rp *p, const char *key, const char *val) { return rp_fail(p, "bad value %s=%s", key, val); }

/* ---- lexing: in-place tokens, "" quoting with doubled quotes, # to end of line ---- */
static int rp_tokens(char *s, char **argv, int max)
{
    int n = 0;
    for (;;) {
        char *w; int q = 0;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#') return n;
        if (n == max) return -1;
        argv[n++] = w = s;
        while (*s && (q || (*s != ' ' && *s != '\t' && *s != '#'))) {
            if (*s == '"') { if (q && s[1] == '"') { *w++ = '"'; s += 2; } else { q = !q; s++; } }
            else *w++ = *s++;
        }
        if (q) return -2;
        if (*s == ' ' || *s == '\t') s++;
        *w = 0;
    }
}

static int rp_num(const char *s, long lo, long hi, long *out)
{
    char *end; long v;
    if (!s || !*s) return -1;
    v = strtol(s, &end, (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10);
    if (*end || v < lo || v > hi) return -1;
    *out = v;
    return 0;
}

static int rp_key(const char *tok, char *key, size_t cap, const char **val)
{
    const char *eq = strchr(tok, '=');
    if (!eq || eq == tok || (size_t)(eq - tok) >= cap) return -1;
    memcpy(key, tok, (size_t)(eq - tok)); key[eq - tok] = 0;
    *val = eq + 1;
    return 0;
}

/* "l:a" -> address code; *rest = the text after it (NULL = nothing may follow) */
static int rp_chan(const char *s, const char **rest, int *code)
{
    char *e; long l, a;
    l = strtol(s, &e, 10);
    if (e == s || *e != ':' || l < 1 || l > LJ_LINKS) return -1;
    s = e + 1;
    a = strtol(s, &e, 10);
    if (e == s || a < 1 || a > LJ_LINK_CH) return -1;
    if (rest) *rest = e; else if (*e) return -1;
    *code = lj_addr_code((int)l, (int)a);
    return 0;
}

/* "l:a=v" (state NULL) or "l:a=v:S" */
static int rp_chval(const char *tok, int *code, long hi, long *v, int *state)
{
    const char *rest, *colon; char buf[32]; size_t n;
    if (rp_chan(tok, &rest, code) || *rest != '=') return -1;
    rest++;
    colon = strchr(rest, ':');
    if (state) {
        if (!colon || !colon[1] || colon[2]) return -1;
        *state = lj_word_index(lj_fstate_words, colon + 1);
        if (*state < 0) return -1;
    } else if (colon) return -1;
    n = colon ? (size_t)(colon - rest) : strlen(rest);
    if (n >= sizeof buf) return -1;
    memcpy(buf, rest, n); buf[n] = 0;
    return rp_num(buf, 0, hi, v);
}

static const char *rp_chname(int code, char *buf)   /* buf[16] */
{
    sprintf(buf, "%d:%d", code / LJ_LINK_CH + 1, code % LJ_LINK_CH + 1);
    return buf;
}

static int rp_runner(const char *w, uint32_t *id)
{
    long n;
    if (!strcmp(w, "all")) { *id = LJ_RUN_ALL; return 0; }
    if (!strcmp(w, "flash")) { *id = LJ_RUN_FLASH; return 0; }
    if (!strcmp(w, "current")) { *id = LJ_RUN_CURRENT; return 0; }
    if (!strncmp(w, "slot", 4) && !rp_num(w + 4, 1, LJ_CUE_SLOTS, &n)) { *id = (uint32_t)(LJ_RUN_SLOT0 + n - 1); return 0; }
    if (!strncmp(w, "bg", 2) && !rp_num(w + 2, 1, LJ_BG_SLOTS, &n)) { *id = (uint32_t)(LJ_RUN_BG0 + n - 1); return 0; }
    return -1;
}
static int rp_slotword(const char *w, long *k) { return (strncmp(w, "slot", 4) || rp_num(w + 4, 1, LJ_CUE_SLOTS, k)) ? -1 : 0; }
static int rp_pageslot(const char *w, long *pg, long *sl)   /* "p.s" */
{
    char c;
    if (sscanf(w, "%ld.%ld%c", pg, sl, &c) != 2) return -1;
    return (*pg < 1 || *pg > LJ_STATIC_PAGES_MAX || *sl < 1 || *sl > LJ_STATICS2_SLOTS_MAX) ? -1 : 0;
}

/* key=value loop helpers: `key`, `val`, `v` and `p` are the enclosing locals */
#define KV_NUM(name, lo, hi, dst, type) if (!strcmp(key, name)) { if (rp_num(val, lo, hi, &v)) return rp_bad(p, key, val); dst = (type)v; continue; }
#define KV_WORD(name, table, dst, type) if (!strcmp(key, name)) { int w_ = lj_word_index(table, val); if (w_ < 0) return rp_bad(p, key, val); dst = (type)w_; continue; }
#define KV_STR(name, dst) if (!strcmp(key, name)) { if (lj_str_copy(dst, sizeof dst, val)) return rp_bad(p, key, val); continue; }
#define KV_NEXT(i) if (rp_key(argv[i], key, sizeof key, &val)) return rp_fail(p, "expected key=value, got \"%s\"", argv[i])

static int rp_need_build(rp *p, const char *what)
{
    return p->built ? rp_fail(p, "%s after the first event line", what) : 0;
}
static int rp_need_patched(rp *p, int code)
{
    char b[16];
    return p->patched[code] ? 0 : rp_fail(p, "%s is not patched", rp_chname(code, b));
}

static int rp_run_to(rp *p, long k);
static const lj_sequence *rp_link_seq(rp *p, const lj_static_slot *st) { return st->kind == LJ_STATIC_LINK ? (const lj_sequence *)lj_table_get(&p->show->sequences, st->link_seq) : NULL; }   /* NULL = link error */

/* ---- build lines ---- */
static void rp_copy_default_dmx(rp *p, int fx)
{
    const lj_fixture *f = &p->show->fixture[fx];
    const lj_profile *prof = p->prof[fx] ? p->prof[fx] : lj_fixlib_by_id(f->profile_id);
    int base = lj_addr_code(f->link, f->address), k;
    for (k = 0; k < prof->footprint; k++) p->show->default_out[base + k] = prof->default_dmx[k];   /* Fixture Configuration on save, LORD_FIXTURES-11 */
}

static int rp_patch(rp *p, char **argv, int argc)
{
    long v, n, id = -1, link = 0, addr = 0; int i, base, k; char key[32]; const char *val;
    lj_fixture *fx; const lj_profile *prof;
    if (argc < 2 || rp_num(argv[1], 1, LJ_FIXTURES, &n)) return rp_fail(p, "patch: expected `patch <1..100> profile= link= addr=`");
    fx = &p->show->fixture[n - 1];
    if (fx->link) return rp_fail(p, "fixture %ld already patched", n);
    memset(fx, 0, sizeof *fx);
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("profile", 0, 65535, id, long);
        KV_NUM("link", 1, LJ_LINKS, link, long);
        KV_NUM("addr", 1, LJ_LINK_CH, addr, long);
        KV_NUM("ipan", 0, 1, fx->invert_pan, uint8_t);
        KV_NUM("itilt", 0, 1, fx->invert_tilt, uint8_t);
        KV_NUM("swap", 0, 1, fx->swap_pt, uint8_t);
        KV_NUM("ibo", 0, 1, fx->ignore_bo, uint8_t);
        KV_NUM("igm", 0, 1, fx->ignore_gm, uint8_t);
        KV_NUM("lsgm", 0, 1, fx->link_strobe_gm, uint8_t);
        if (!strcmp(key, "chgm")) {   /* W3 profile-editor (ME-32): Config.dat's chgm= list, offsets 1..32 with Grand Master control disabled */
            uint32_t o[LJ_PROFILE_CH]; int m = lj_kv_list(val, o, LJ_PROFILE_CH), j;
            if (m < 0) return rp_bad(p, key, val);
            for (j = 0; j < m; j++) { if (o[j] < 1 || o[j] > LJ_PROFILE_CH) return rp_bad(p, key, val); fx->ch_gm_off |= 1u << (o[j] - 1); }
            continue;
        }
        KV_NUM("sub", 0, LJ_SUBMASTERS, fx->intensity_group, uint8_t);
        if (!strcmp(key, "gs2")) return rp_fail(p, "gs2= profiles are W3 (lj_gs2_parse)");
        return rp_fail(p, "patch: unknown key %s", key);
    }
    if (id < 0 || !link || !addr) return rp_fail(p, "patch: profile=, link= and addr= are required");
    prof = lj_fixlib_by_id((uint16_t)id);
    if (!prof) return rp_fail(p, "unknown profile id %ld (lj_fixlib_by_id)", id);
    if (prof->footprint < 1 || prof->footprint > LJ_PROFILE_CH) return rp_fail(p, "profile %ld has footprint %d", id, prof->footprint);
    if (addr + prof->footprint - 1 > LJ_LINK_CH) return rp_fail(p, "fixture %ld: %ld + %d channels run past 512", n, addr, prof->footprint);
    base = lj_addr_code((int)link, (int)addr);
    for (k = 0; k < prof->footprint; k++) if (p->patched[base + k]) { char b[16]; return rp_fail(p, "fixture %ld overlaps %s", n, rp_chname(base + k, b)); }
    for (k = 0; k < prof->footprint; k++) p->patched[base + k] = 1;
    fx->kind = LJ_FX_BUILTIN; fx->profile_id = (uint16_t)id; fx->link = (uint8_t)link; fx->address = (uint16_t)addr; fx->placed = 1;
    lj_str_copy(fx->user_name, sizeof fx->user_name, prof->tag);
    rp_copy_default_dmx(p, (int)n - 1);
    return 0;
}

/* profile n key=... : a per-fixture copy of the built-in with fields overridden (what a .GS2 would carry); the fixture becomes
 * kind GS2 and the copy is bound with lj_engine_bind_profile after lj_engine_bind (which re-resolves built-ins on every rebuild) */
static int rp_profile(rp *p, char **argv, int argc)
{
    long v, n, k; int i; char key[32]; const char *val; lj_profile *pr; lj_fixture *fx;
    if (argc < 2 || rp_num(argv[1], 1, LJ_FIXTURES, &n)) return rp_fail(p, "profile: expected `profile <1..100> key=value...`");
    fx = &p->show->fixture[n - 1];
    if (!fx->link) return rp_fail(p, "profile: fixture %ld is not patched yet", n);
    if (!p->prof[n - 1]) {
        pr = (lj_profile *)malloc(sizeof *pr);
        if (!pr) return rp_fail(p, "out of memory");
        *pr = *lj_fixlib_by_id(fx->profile_id);
        p->prof[n - 1] = pr;
        fx->kind = LJ_FX_GS2;                      /* a user profile: lj_engine_bind resolves built-ins itself and keeps a bound profile only for a GS2 fixture */
        snprintf(fx->gs2_file, sizeof fx->gs2_file, "replay-%s.GS2", pr->tag);
    }
    pr = p->prof[n - 1];
    for (i = 2; i < argc; i++) {
        char *num; const char *base;
        KV_NEXT(i);
        KV_NUM("use_gm", 0, 1, pr->use_gm, uint8_t);
        KV_NUM("bo_at_gm0", 0, 1, pr->bo_at_gm0, uint8_t);
        KV_NUM("invert_int", 0, 1, pr->invert_int, uint8_t);
        KV_NUM("invert_range", 0, 1, pr->invert_int_range, uint8_t);
        KV_NUM("limit_gm", 0, 1, pr->limit_gm, uint8_t);
        KV_NUM("imin", 0, 255, pr->intensity.min, uint8_t);
        KV_NUM("imax", 0, 255, pr->intensity.max, uint8_t);
        KV_NUM("ioffset", 1, LJ_PROFILE_CH, pr->intensity.offset, uint8_t);
        /* per-offset keys: class<k> disabled<k> ddmx<k> extgm<k>, k = 1..footprint */
        num = key; while (*num && (*num < '0' || *num > '9')) num++;
        base = key;
        if (*num && !rp_num(num, 1, pr->footprint, &k)) {
            size_t bl = (size_t)(num - base);
            k--;
            if (bl == 5 && !strncmp(base, "class", 5)) { int w = lj_word_index(rp_class_words, val); if (w < 0) return rp_bad(p, key, val); pr->channel_class[k] = (uint8_t)w; continue; }
            if (bl == 8 && !strncmp(base, "disabled", 8)) { if (rp_num(val, 0, 1, &v)) return rp_bad(p, key, val); pr->level_disabled[k] = (uint8_t)v; continue; }
            if (bl == 4 && !strncmp(base, "ddmx", 4)) { if (rp_num(val, 0, 255, &v)) return rp_bad(p, key, val); pr->default_dmx[k] = (uint8_t)v; continue; }
            if (bl == 5 && !strncmp(base, "extgm", 5)) { if (rp_num(val, 0, 1, &v)) return rp_bad(p, key, val); pr->ext_gm[k] = (uint8_t)v; continue; }
        }
        return rp_fail(p, "profile: unknown key %s", key);
    }
    rp_copy_default_dmx(p, (int)n - 1);
    return 0;
}

static int rp_default(rp *p, char **argv, int argc)
{
    int i, code; long v;
    if (argc < 2) return rp_fail(p, "default: no channels");
    for (i = 1; i < argc; i++)                      /* grammar first: a bad runtime line never runs ticks */
        if (rp_chval(argv[i], &code, 255, &v, NULL)) return rp_fail(p, "default: expected l:a=v, got \"%s\"", argv[i]);
    if (p->built && rp_run_to(p, p->edit_tick - 1)) return -1;   /* `at k default`: the UI edits show->default_out before tick k */
    for (i = 1; i < argc; i++) { rp_chval(argv[i], &code, 255, &v, NULL); p->show->default_out[code] = (uint8_t)v; }
    return 0;
}

/* `at k scene_edit <seq> <scene> l:a=v:S ...`: the pinned sequence's scene (0-based) edited in place under the lock before tick k
 * (14 sB.8 item 4: a link static follows later edits of its scene); posts nothing */
static int rp_scene_edit(rp *p, char **argv, int argc)
{
    int i, code, st; long n, sc, v; lj_sequence *s;
    if (argc < 4 || rp_num(argv[1], 1, LJ_NUMBER_MAX, &n) || rp_num(argv[2], 0, LJ_SCENES_MAX - 1, &sc)) return rp_fail(p, "scene_edit: expected `scene_edit <seq> <scene> l:a=v:S ...`");
    if ((s = (lj_sequence *)lj_table_get(&p->show->sequences, (uint32_t)n)) == NULL || sc >= (long)s->n) return rp_fail(p, "scene_edit: seq %ld scene %ld is not defined", n, sc);
    for (i = 3; i < argc; i++) {                    /* grammar first: a bad runtime line never runs ticks */
        if (rp_chval(argv[i], &code, 255, &v, &st)) return rp_fail(p, "scene_edit: expected l:a=v:S, got \"%s\"", argv[i]);
        if (rp_need_patched(p, code)) return -1;
    }
    if (p->built && rp_run_to(p, p->edit_tick - 1)) return -1;
    for (i = 3; i < argc; i++) { rp_chval(argv[i], &code, 255, &v, &st); s->scenes[sc].val[code] = (uint8_t)v; s->scenes[sc].state[code] = (uint8_t)st; }
    return 0;
}

static int rp_gpatch(rp *p, char **argv, int argc)
{
    int i, code; long v; char key[32]; const char *val; lj_patch_ch *pc, tmp; const char *msg;
    if (argc < 2 || rp_chan(argv[1], NULL, &code)) return rp_fail(p, "gpatch: expected `gpatch l:a key=value...`");
    pc = &tmp; tmp = p->show->patch[code];          /* parsed into a copy: a bad runtime line never runs ticks */
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_WORD("src", lj_patch_src_words, pc->src_kind, uint8_t);
        KV_NUM("sl", 1, LJ_LINKS, pc->src_link, uint8_t);
        KV_NUM("sc", 1, LJ_LINK_CH, pc->src_ch, uint16_t);
        KV_WORD("mode", lj_in_mode_words, pc->in_mode, uint8_t);
        KV_NUM("inv", 0, 1, pc->invert, uint8_t);
        KV_NUM("min", 0, 255, pc->min, uint8_t);
        KV_NUM("max", 0, 255, pc->max, uint8_t);
        KV_NUM("htp", 0, 0xFFFFFF, pc->htp_mask, uint32_t);
        KV_NUM("ign", 0, 1, pc->ign_en, uint8_t);                 /* W6: Ignore DMX In Enable */
        return rp_fail(p, "gpatch: unknown key %s", key);
    }
    if (p->built && rp_run_to(p, p->edit_tick - 1)) return -1;   /* `at k gpatch`: the UI edits show->patch under the lock before tick k */
    p->show->patch[code] = tmp;
    if (p->built && (msg = lj_patch_validate(p->show->patch)) != NULL) return rp_fail(p, "patch invalid: %s", msg);
    return 0;
}

static int rp_submaster(rp *p, char **argv, int argc)
{
    long v, k; int i; char key[32]; const char *val;
    if (argc < 2 || rp_num(argv[1], 1, LJ_SUBMASTERS, &k)) return rp_fail(p, "submaster: expected `submaster <1..8> name= mode=`");
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", p->show->submaster_name[k - 1]);
        KV_WORD("mode", rp_master_words, p->show->submaster_mode[k - 1], uint8_t);
        return rp_fail(p, "submaster: unknown key %s", key);
    }
    (void)v;
    return 0;
}

static int rp_ch(rp *p, char **argv, int argc)
{
    int i, code, st; long v;
    if (!p->scene) return rp_fail(p, "ch outside a scene or static slot");
    if (argc < 2) return rp_fail(p, "ch: no channels");
    for (i = 1; i < argc; i++) {
        if (rp_chval(argv[i], &code, 255, &v, &st)) return rp_fail(p, "ch: expected l:a=v:S, got \"%s\"", argv[i]);
        if (rp_need_patched(p, code)) return -1;
        p->scene->val[code] = (uint8_t)v;
        p->scene->state[code] = (uint8_t)st;
    }
    return 0;
}

static int rp_scene(rp *p, char **argv, int argc)
{
    long v; int i; char key[32]; const char *val; lj_sequence *s = p->seq;
    if (!s) return rp_fail(p, "scene outside a seq");
    if (p->nscene == 0) {
        if (s->n < 1 && lj_sequence_alloc(s, 1)) return rp_fail(p, "lj_sequence_alloc failed");
        p->scene = &s->scenes[0];
    } else {
        int idx = lj_sequence_add(s, (uint16_t)(p->nscene - 1));
        if (idx < 0) return rp_fail(p, "lj_sequence_add failed (scene %d)", p->nscene + 1);
        p->scene = &s->scenes[idx];
    }
    lj_scene_init(p->scene);
    p->nscene++;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "ch")) return rp_ch(p, argv + i, argc - i);
        KV_NEXT(i);
        KV_NUM("st", 0, LJ_TIME_MAX_MS, p->scene->scene_ms, uint32_t);
        KV_NUM("ft", 0, LJ_TIME_MAX_MS, p->scene->fade_ms, uint32_t);
        return rp_fail(p, "scene: unknown key %s", key);
    }
    return 0;
}

static int rp_seq(rp *p, char **argv, int argc)
{
    long v, n; int i; char key[32]; const char *val; lj_sequence *s;
    if (argc < 2 || rp_num(argv[1], 1, LJ_NUMBER_MAX, &n)) return rp_fail(p, "seq: expected `seq <number> name=...`");
    if (lj_table_get(&p->show->sequences, (uint32_t)n)) return rp_fail(p, "seq %ld already defined", n);
    if (p->nseq == RP_RECS) return rp_fail(p, "more than %d sequences", RP_RECS);
    s = lj_sequence_new_rec();
    s->number = (uint32_t)n;
    if (lj_table_put(&p->show->sequences, (uint32_t)n, s)) { lj_sequence_free(s); return rp_fail(p, "lj_table_put failed"); }
    p->seqs[p->nseq].line = p->line; p->seqs[p->nseq++].rec = s;
    p->seq = s; p->nscene = 0; p->scene = NULL; p->cue = NULL; p->bg = NULL; p->macro = NULL;
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "scene")) return rp_scene(p, argv + i, argc - i);
        KV_NEXT(i);
        KV_STR("name", s->name);
        KV_NUM("sync", 0, 1, s->sync_times, uint8_t);
        KV_NUM("gst", 0, 1, s->global_scene_time, uint8_t);
        KV_NUM("gft", 0, 1, s->global_fade_time, uint8_t);
        return rp_fail(p, "seq: unknown key %s", key);
    }
    return 0;
}

static int rp_fx(rp *p, char **argv, int argc)
{
    long v, n; int i; char key[32]; const char *val;
    if (!p->scene) return rp_fail(p, "fx outside a scene");
    if (argc < 3 || rp_num(argv[1], 1, LJ_FIXTURES, &n)) return rp_fail(p, "fx: expected `fx <1..100> preset=k` or `fx <n> macro=... mst=...`");
    if (!p->show->fixture[n - 1].link) return rp_fail(p, "fx: fixture %ld is not patched", n);
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("preset", 0, LJ_PRESETS, p->scene->preset[n - 1], uint8_t);
        if (!strcmp(key, "macro")) {
            int a[9]; char c; lj_movemacro *m = &p->scene->macro[n - 1];
            if (sscanf(val, "%d,%d,%d,%d,%d,%d,%d,%d,%d%c", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &c) != 9) return rp_bad(p, key, val);
            for (v = 0; v < 8; v++) if (a[v] < 0 || a[v] > 255) return rp_bad(p, key, val);
            if (a[8] < 0 || a[8] > 65535) return rp_bad(p, key, val);
            m->type = (uint8_t)a[0]; m->amp_pan = (uint8_t)a[1]; m->amp_tilt = (uint8_t)a[2]; m->cycle_pan = (uint8_t)a[3]; m->cycle_tilt = (uint8_t)a[4];
            m->dir = (uint8_t)a[5]; m->delay = (uint8_t)a[6]; m->tweak = (uint8_t)a[7]; m->step = (uint16_t)a[8];
            continue;
        }
        if (!strcmp(key, "mst")) {
            int k; char w[2] = { 0, 0 };
            if (strlen(val) != 11) return rp_bad(p, key, val);
            for (k = 0; k < 6; k++) {
                int st;
                w[0] = val[2 * k];
                st = lj_word_index(lj_fstate_words, w);
                if (st < 0 || (k < 5 && val[2 * k + 1] != ',')) return rp_bad(p, key, val);
                p->scene->macro[n - 1].fstate[k] = (uint8_t)st;
            }
            continue;
        }
        return rp_fail(p, "fx: unknown key %s", key);
    }
    return 0;
}

static int rp_preset(rp *p, char **argv, int argc)
{
    long v, n, k; int i; char key[32]; const char *val; lj_preset *pr;
    if (argc < 4 || rp_num(argv[1], 1, LJ_FIXTURES, &n) || rp_num(argv[2], 1, LJ_PRESETS, &k)) return rp_fail(p, "preset: expected `preset <fixture> <1..99> pan= tilt=`");
    pr = &p->show->preset[n - 1][k - 1];
    pr->used = 1;
    for (i = 3; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("pan", -32768, 32767, pr->pan, int16_t);
        KV_NUM("tilt", -32768, 32767, pr->tilt, int16_t);
        return rp_fail(p, "preset: unknown key %s", key);
    }
    return 0;
}

static void rp_slot_defaults(lj_cue_slot *s, uint32_t seq)   /* the .cue record defaults (design 5.2) */
{
    memset(s, 0, sizeof *s);
    s->seq = seq; s->enabled = 1; s->scenes_per_trig = 1; s->audio_trigs_per_trig = 1;
}

static int rp_slot(rp *p, char **argv, int argc)
{
    long v, k, seq = -1; int i; char key[32]; const char *val; lj_cue_slot *s;
    if (!p->cue) return rp_fail(p, "slot outside a cue");
    if (argc < 3 || rp_num(argv[1], 1, LJ_CUE_SLOTS, &k)) return rp_fail(p, "slot: expected `slot <1..12> seq=<n> ...`");
    s = &p->cue->slot[k - 1];
    rp_slot_defaults(s, 0);
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("seq", 1, LJ_NUMBER_MAX, seq, long);
        KV_NUM("on", 0, 1, s->enabled, uint8_t);
        KV_WORD("trig", lj_trig_words, s->trig, uint8_t);
        KV_WORD("dir", lj_dir_words, s->dir, uint8_t);
        KV_NUM("spt", 1, 255, s->scenes_per_trig, uint8_t);
        KV_NUM("atpt", 1, 255, s->audio_trigs_per_trig, uint8_t);
        KV_NUM("tol", 0, 1, s->trig_on_load, uint8_t);
        KV_NUM("first", 0, LJ_SCENES_MAX, s->first, uint16_t);
        KV_NUM("last", 0, LJ_SCENES_MAX, s->last, uint16_t);
        KV_NUM("loops", 0, 65535, s->loops, uint16_t);
        KV_WORD("end", lj_end_words, s->end_action, uint8_t);
        KV_NUM("goto", 0, LJ_SCENES_MAX, s->goto_scene, uint16_t);
        KV_NUM("link", 0, LJ_NUMBER_MAX, s->link_cue, uint32_t);
        KV_NUM("time", 0, 999, s->time_pct, uint16_t);
        KV_NUM("chase", 0, LJ_TIME_MAX_MS, s->chase_ms, uint32_t);
        return rp_fail(p, "slot: unknown key %s", key);
    }
    if (seq < 0) return rp_fail(p, "slot: seq= is required");
    s->seq = (uint32_t)seq;    /* a seq that names no `seq` line is the load-error case, not an error here */
    return 0;
}

static int rp_cue(rp *p, char **argv, int argc)
{
    long v, n, slot = 0, seq = 0; int i; char key[32]; const char *val; lj_cue *c;
    if (argc < 2 || rp_num(argv[1], 1, LJ_NUMBER_MAX, &n)) return rp_fail(p, "cue: expected `cue <number> ...`");
    if (lj_table_get(&p->show->cues, (uint32_t)n)) return rp_fail(p, "cue %ld already defined", n);
    if (p->ncue == RP_RECS) return rp_fail(p, "more than %d cues", RP_RECS);
    c = lj_cue_new_rec();
    c->number = (uint32_t)n;
    if (lj_table_put(&p->show->cues, (uint32_t)n, c)) { lj_cue_free(c); return rp_fail(p, "lj_table_put failed"); }
    p->cues[p->ncue].line = p->line; p->cues[p->ncue++].rec = c;
    p->cue = c; p->seq = NULL; p->scene = NULL; p->bg = NULL; p->macro = NULL;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", c->name);
        KV_NUM("cuetime", 0, 999, c->cue_time_pct, uint16_t);
        if (!strcmp(key, "macro")) {
            int a, s; char x;
            if (sscanf(val, "%d,%d%c", &a, &s, &x) != 2 || a < 0 || a > 255 || s < 0 || s > 255) return rp_bad(p, key, val);
            c->macro_amp_pct = (uint8_t)a; c->macro_speed_pct = (uint8_t)s;
            continue;
        }
        KV_NUM("transparent", 0, 1, c->transparent, uint8_t);
        KV_NUM("bgcue", 0, LJ_NUMBER_MAX, c->bg_cue, uint32_t);
        KV_NUM("genmacro", 0, LJ_NUMBER_MAX, c->gen_macro, uint32_t);
        KV_NUM("slot", 1, LJ_CUE_SLOTS, slot, long);          /* the one-line form: cue n slot=k seq=m */
        KV_NUM("seq", 1, LJ_NUMBER_MAX, seq, long);
        return rp_fail(p, "cue: unknown key %s", key);
    }
    if (slot || seq) {
        if (!slot || !seq) return rp_fail(p, "cue: slot= and seq= go together");
        rp_slot_defaults(&c->slot[slot - 1], (uint32_t)seq);
    }
    return 0;
}

static int rp_bg(rp *p, char **argv, int argc)
{
    long v, n; int i; char key[32]; const char *val; lj_bgcue *b;
    if (argc < 2 || rp_num(argv[1], 1, LJ_BG_FILES, &n)) return rp_fail(p, "bg: expected `bg <1..99> ...`");
    if (lj_table_get(&p->show->bgcues, (uint32_t)n)) return rp_fail(p, "bg %ld already defined", n);
    if (p->nbg == RP_RECS) return rp_fail(p, "more than %d background cues", RP_RECS);
    b = lj_bgcue_new_rec();
    b->number = (uint32_t)n;
    if (lj_table_put(&p->show->bgcues, (uint32_t)n, b)) { lj_bgcue_free(b); return rp_fail(p, "lj_table_put failed"); }
    p->bgs[p->nbg].line = p->line; p->bgs[p->nbg++].rec = b;
    p->bg = b; p->seq = NULL; p->scene = NULL; p->cue = NULL; p->macro = NULL;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", b->name);
        KV_NUM("transparent", 0, 1, b->transparent, uint8_t);
        return rp_fail(p, "bg: unknown key %s", key);
    }
    return 0;
}

static int rp_bgslot(rp *p, char **argv, int argc)
{
    long v, k, seq = -1; int i; char key[32]; const char *val;
    if (!p->bg) return rp_fail(p, "bgslot outside a bg");
    if (argc < 3 || rp_num(argv[1], 1, LJ_BG_SLOTS, &k)) return rp_fail(p, "bgslot: expected `bgslot <1..5> seq=<n> ...`");
    p->bg->slot[k - 1].enabled = 1; p->bg->slot[k - 1].mode = LJ_BG_AUTO;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("seq", 1, LJ_NUMBER_MAX, seq, long);
        KV_WORD("mode", lj_bg_mode_words, p->bg->slot[k - 1].mode, uint8_t);
        KV_NUM("on", 0, 1, p->bg->slot[k - 1].enabled, uint8_t);
        return rp_fail(p, "bgslot: unknown key %s", key);
    }
    if (seq < 0) return rp_fail(p, "bgslot: seq= is required");
    p->bg->slot[k - 1].seq = (uint32_t)seq;
    return 0;
}

static int rp_static(rp *p, char **argv, int argc)
{
    long v, pg, sl; int i; char key[32]; const char *val; lj_static_slot *s;
    if (argc < 2 || rp_pageslot(argv[1], &pg, &sl)) return rp_fail(p, "static: expected `static <page>.<slot> ...`");
    while (p->show->npages < pg)
        if (lj_statics_add_page(&p->show->pages, &p->show->npages, "") < 0) return rp_fail(p, "lj_statics_add_page failed");
    s = &p->show->pages[pg - 1].slot[sl - 1];
    if (s->kind != LJ_STATIC_EMPTY) return rp_fail(p, "static %ld.%ld already defined", pg, sl);
    s->fade_ms = LJ_FADE_TIME_DEFAULT_MS; s->level_pct = 100;
    p->seq = NULL; p->scene = NULL; p->cue = NULL; p->bg = NULL; p->macro = NULL;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", s->name);
        KV_WORD("act", lj_act_words, s->activation, uint8_t);
        KV_WORD("rep", lj_replace_words, s->replace, uint8_t);
        KV_NUM("ft", 0, LJ_TIME_MAX_MS, s->fade_ms, uint32_t);
        KV_NUM("level", 0, 100, s->level_pct, uint8_t);
        KV_NUM("transparent", 0, 1, s->transparent, uint8_t);
        if (!strcmp(key, "groups")) { char *e; s->groups = strtoull(val, &e, 10); if (*e || e == val) return rp_bad(p, key, val); continue; }
        if (!strcmp(key, "link")) {
            long a, b; char x;
            if (sscanf(val, "%ld,%ld%c", &a, &b, &x) != 2 || a < 1 || a > LJ_NUMBER_MAX || b < 0 || b > LJ_SCENES_MAX) return rp_bad(p, key, val);
            s->kind = LJ_STATIC_LINK; s->link_seq = (uint32_t)a; s->link_scene = (uint16_t)b;
            continue;
        }
        return rp_fail(p, "static: unknown key %s", key);
    }
    if (s->kind != LJ_STATIC_LINK) {           /* "Record Current Scene" from a blank scene; the ch lines fill it */
        lj_scene *blank = (lj_scene *)malloc(sizeof *blank);
        if (!blank) return rp_fail(p, "out of memory");
        lj_scene_init(blank);
        blank->fade_ms = s->fade_ms;
        if (lj_static_slot_record(s, blank)) { free(blank); return rp_fail(p, "lj_static_slot_record failed"); }
        free(blank);
        p->scene = s->scene;
    }
    return 0;
}

static int rp_macro(rp *p, char **argv, int argc)
{
    long v, n; int i; char key[32]; const char *val; lj_genmacro *m;
    if (argc < 2 || rp_num(argv[1], 1, LJ_MACROS_MAX, &n)) return rp_fail(p, "macro: expected `macro <1..9999> ...`");
    if (lj_table_get(&p->show->macros, (uint32_t)n)) return rp_fail(p, "macro %ld already defined", n);
    if (p->nmacro == RP_RECS) return rp_fail(p, "more than %d macros", RP_RECS);
    m = (lj_genmacro *)calloc(1, sizeof *m);
    if (!m) return rp_fail(p, "out of memory");
    m->number = (uint32_t)n;
    if (lj_table_put(&p->show->macros, (uint32_t)n, m)) { free(m); return rp_fail(p, "lj_table_put failed"); }
    p->macros[p->nmacro].line = p->line; p->macros[p->nmacro++].rec = m;
    p->macro = m; p->seq = NULL; p->scene = NULL; p->cue = NULL; p->bg = NULL;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", m->name);
        KV_NUM("restart", 0, 1, m->restart_on_load, uint8_t);
        return rp_fail(p, "macro: unknown key %s", key);
    }
    return 0;
}

static int rp_mch(rp *p, char **argv, int argc)
{
    long v; int i, code; char key[32]; const char *val;
    if (!p->macro) return rp_fail(p, "mch outside a macro");
    if (argc < 3 || rp_chan(argv[1], NULL, &code)) return rp_fail(p, "mch: expected `mch l:a shape= base= amp= cycle= dir= delay=`");
    if (rp_need_patched(p, code)) return -1;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("shape", 0, 65, p->macro->ch[code].shape, uint8_t);
        if (!strcmp(key, "base")) {
            if (!strcmp(val, "off")) { p->macro->ch[code].base_on = 0; continue; }
            if (rp_num(val, 0, 255, &v)) return rp_bad(p, key, val);
            p->macro->ch[code].base_on = 1; p->macro->ch[code].base = (uint8_t)v;
            continue;
        }
        if (!strcmp(key, "amp")) {
            int a, b; char x;
            if (sscanf(val, "%d,%d%c", &a, &b, &x) != 2 || a < -255 || a > 255 || b < -255 || b > 255) return rp_bad(p, key, val);
            p->macro->ch[code].amp_pos = (int16_t)a; p->macro->ch[code].amp_neg = (int16_t)b;
            continue;
        }
        KV_NUM("cycle", 0, 5950, p->macro->ch[code].cycle_ds, uint16_t);
        KV_WORD("dir", lj_dir_words, p->macro->ch[code].dir, uint8_t);
        KV_NUM("delay", 0, LJ_GENMACRO_SHAPE_N - 1, p->macro->ch[code].delay, uint16_t);
        return rp_fail(p, "mch: unknown key %s", key);
    }
    return 0;
}

static int rp_htp(rp *p, char **argv, int argc)
{
    long k; int i, code;
    if (argc < 3 || rp_num(argv[1], 1, LJ_HTP_GROUPS, &k)) return rp_fail(p, "htp: expected `htp <1..24> l:a ...`");
    for (i = 2; i < argc; i++) {
        if (rp_chan(argv[i], NULL, &code)) return rp_fail(p, "htp: expected l:a, got \"%s\"", argv[i]);
        p->show->patch[code].htp_mask |= 1u << (k - 1);
    }
    return 0;
}

/* ---- the build -> engine transition ---- */
static int rp_finish(rp *p)
{
    int i, line = p->line; const char *msg;
    for (i = 0; i < p->nseq; i++) if ((msg = lj_sequence_validate((lj_sequence *)p->seqs[i].rec)) != NULL) { p->line = p->seqs[i].line; rp_fail(p, "sequence invalid: %s", msg); p->line = line; return -1; }
    for (i = 0; i < p->ncue; i++) if ((msg = lj_cue_validate((lj_cue *)p->cues[i].rec)) != NULL) { p->line = p->cues[i].line; rp_fail(p, "cue invalid: %s", msg); p->line = line; return -1; }
    for (i = 0; i < p->nbg; i++) if ((msg = lj_bgcue_validate((lj_bgcue *)p->bgs[i].rec)) != NULL) { p->line = p->bgs[i].line; rp_fail(p, "bg cue invalid: %s", msg); p->line = line; return -1; }
    if (p->show->npages && (msg = lj_statics_validate(p->show->pages, p->show->npages)) != NULL) return rp_fail(p, "statics invalid: %s", msg);
    if ((msg = lj_patch_validate(p->show->patch)) != NULL) return rp_fail(p, "patch invalid: %s", msg);
    if ((msg = lj_config_validate(p->show)) != NULL) return rp_fail(p, "config invalid: %s", msg);
    p->e = lj_engine_create();
    if (!p->e) return rp_fail(p, "lj_engine_create failed");
    lj_engine_set_tick_us(p->e, p->tick_us);
    p->tick_us = lj_engine_tick_us(p->e);
    p->r->tick_us = p->tick_us;
    lj_engine_bind(p->e, p->show);
    for (i = 0; i < LJ_FIXTURES; i++) if (p->prof[i]) lj_engine_bind_profile(p->e, i, p->prof[i]);
    p->f = lj_engine_frame(p->e);
    if (p->dmx_in_live) {                                      /* ME-190: the app posts this with Overwrite Enable when DMX-in is enabled */
        lj_cmd l;
        memset(&l, 0, sizeof l); l.kind = LJ_CMD_DMX_IN_LIVE; l.b = 1;
        if (lj_engine_cmd(p->e, &l)) return rp_fail(p, "lj_engine_cmd: command array full");
    }
    p->built = 1;
    p->seq = NULL; p->scene = NULL; p->cue = NULL; p->bg = NULL; p->macro = NULL;
    return 0;
}

static int rp_write_frame(rp *p)
{
    uint8_t tn[8]; int k;
    for (k = 0; k < 8; k++) tn[k] = (uint8_t)(p->f->tick_no >> (8 * k));
    return fwrite(p->f->merged, 1, sizeof p->f->merged, p->frames) == sizeof p->f->merged
        && fwrite(p->f->out, 1, sizeof p->f->out, p->frames) == sizeof p->f->out
        && fwrite(tn, 1, 8, p->frames) == 8 ? 0 : -1;
}

/* W3 cuelist-core: the harness's UI-thread cue list (one scenario at a time, reset in lj_replay_run) */
static struct { lj_cl_run run; lj_cuelist *build; uint64_t t, now; int on, fn_calls, fn_last; } rp_clx;
static void rp_cl_steps(rp *p, uint64_t until);

/* Runs ticks until tick k has run (synthetic time k * tick_us); one view copy per tick, as the app's idle pass does. */
static int rp_run_to(rp *p, long k)
{
    while (p->tick <= k) {
        uint64_t t0 = lj_clock_us(), dt;
        rp_cl_steps(p, (uint64_t)p->tick * p->tick_us);   /* W3: the 10 ms UI wakes before this tick (design 2.3 rule 4) */
        lj_engine_tick(p->e, (uint64_t)p->tick * p->tick_us);
        dt = lj_clock_us() - t0;
        if (dt > p->r->max_tick_us) p->r->max_tick_us = (uint32_t)dt;
        lj_engine_view(p->e, &p->view);
        if (p->sacn_posted) {
            const void *c = lj_engine_take_sacn_cfg(p->e);
            if (c != p->sacn_posted || lj_engine_take_sacn_cfg(p->e) != NULL) rp_fail(p, "tick %d: lj_engine_take_sacn_cfg did not hand the config out exactly once", p->tick);
            p->sacn_posted = NULL;
        }
        if (p->frames && rp_write_frame(p)) return rp_fail(p, "frames write failed");
        p->tick++;
        p->r->ticks = p->tick;
    }
    return 0;
}

static int rp_tick_arg(rp *p, const char *s, long *k, int allow_ran)
{
    if (rp_num(s, 0, 100000000, k)) return rp_fail(p, "expected a tick number, got \"%s\"", s);
    if (*k < p->tick - (allow_ran ? 1 : 0)) return rp_fail(p, "tick %ld already ran (event lines are in tick order)", *k);
    return 0;
}

/* ---- command lines ---- */
/* ---- W3 gdc-fades-followspot: the UI-side writers of the Generic DMX Control, Sequence Fades and Follow spot windows.
 * Like the windows they edit the current scene (the engine's view after tick k-1) and post CURRENT_SEQ. ---- */
static struct { lj_gdc_state gdc; lj_scene fscene[LJ_FOLLOWSPOT]; uint8_t sel[LJ_FIXTURES]; int have_sel; } rp_gfs;   /* one scenario at a time */

static int rp_gfs_build(rp *p, char **argv, int argc)
{
    long n, v; int i, code; char key[32]; const char *val;
    if (!strcmp(argv[0], "gdcgroup")) {                       /* gdcgroup g l:a [l:a ...] */
        if (argc < 2 || rp_num(argv[1], 1, LJ_GEN_GROUPS, &n)) return rp_fail(p, "gdcgroup: expected `gdcgroup <1..32> l:a ...`");
        p->show->gdc.used[n - 1] = 1;
        for (i = 2; i < argc; i++) {
            if (rp_chan(argv[i], NULL, &code)) return rp_fail(p, "gdcgroup: expected l:a, got \"%s\"", argv[i]);
            p->show->gdc.member[code] |= 1u << (n - 1);
        }
        return 0;
    }
    /* follow n [name=".."] sel=f,f [static=p.s autoclr=0|1], then ch lines into the setting */
    if (argc < 2 || rp_num(argv[1], 1, LJ_FOLLOWSPOT, &n)) return rp_fail(p, "follow: expected `follow <1..20> sel=...`");
    {
        lj_followspot *fs = &p->show->followspot[n - 1];
        memset(fs, 0, sizeof *fs);
        fs->used = 1;
        for (i = 2; i < argc; i++) {
            KV_NEXT(i);
            KV_STR("name", fs->name);
            KV_NUM("autoclr", 0, 1, fs->static_auto_clear, uint8_t);
            if (!strcmp(key, "static")) { long pg, sl; if (rp_pageslot(val, &pg, &sl)) return rp_bad(p, key, val); fs->static_page = (uint8_t)pg; fs->static_slot = (uint8_t)sl; continue; }
            if (!strcmp(key, "sel")) {
                uint32_t m[LJ_FIXTURES]; int k, cnt = lj_kv_list(val, m, LJ_FIXTURES);
                if (cnt < 0) return rp_bad(p, key, val);
                memset(rp_gfs.sel, 0, sizeof rp_gfs.sel);
                for (k = 0; k < cnt; k++) { if (m[k] < 1 || m[k] > LJ_FIXTURES) return rp_bad(p, key, val); fs->selected[m[k] - 1] = rp_gfs.sel[m[k] - 1] = 1; }
                rp_gfs.have_sel = 1;
                continue;
            }
            return rp_fail(p, "follow: unknown key %s", key);
        }
        (void)v;
        lj_scene_init(&rp_gfs.fscene[n - 1]);
        p->scene = &rp_gfs.fscene[n - 1];                      /* the following ch lines are the setting's programmed channels */
    }
    return 0;
}

static int rp_gfs_at(rp *p, long k, char **argv, int argc)
{
    static const char *const attr_words[] = { "intensity", "movement", "color", "gobo", "beam", "effects", "level", "movemacro", NULL };   /* enum lj_attr */
    static const char *const sfx_words[] = { "all_off", "all_snap", "all_fade", "off_snap", "off_fade", "snap_off", "snap_fade", "fade_off", "fade_snap", NULL };   /* enum lj_sfx */
    lj_sequence *s = p->show->current_seq;
    lj_scene *sc;
    lj_cmd c;
    long n;
    int cur;
    if (rp_run_to(p, k - 1)) return -1;
    cur = p->view.cur_scene - 1;                               /* the view's "X:Y" is 1-based, 0 = none */
    if (!s || !s->scenes || cur < 0 || cur >= s->n) return rp_fail(p, "%s: no current sequence (`at k current <seq> <scene>` first)", argv[0]);
    sc = &s->scenes[cur];
    if (!strcmp(argv[0], "gdc")) {                            /* at k gdc "<command line>" */
        lj_gdc_prog prog; char err[96];
        if (argc != 2) return rp_fail(p, "gdc: expected `gdc \"<command line>\"`");
        rp_gfs.gdc.empty_clears = 1;
        if (lj_gdc_parse(argv[1], &prog, err, sizeof err)) return rp_fail(p, "gdc \"%s\": %s", argv[1], err);
        if (lj_gdc_run(&prog, p->show, &rp_gfs.gdc, sc) < 0) return rp_fail(p, "gdc \"%s\": lj_gdc_run failed", argv[1]);
    } else if (!strcmp(argv[0], "followspot")) {              /* at k followspot <n> | 0 */
        if (argc != 2 || rp_num(argv[1], 0, LJ_FOLLOWSPOT, &n)) return rp_fail(p, "followspot: expected `followspot <0..20>`");
        if (n) {
            lj_followspot *fs = &p->show->followspot[n - 1];
            memcpy(fs->val, rp_gfs.fscene[n - 1].val, LJ_CHANS); memcpy(fs->state, rp_gfs.fscene[n - 1].state, LJ_CHANS);
            if (lj_followspot_validate(p->show->followspot)) return rp_fail(p, "followspot: %s", lj_followspot_validate(p->show->followspot));
            if (lj_followspot_apply(fs, p->show, sc, NULL, rp_gfs.sel) < 0) return rp_fail(p, "followspot %ld is not defined", n);
            rp_gfs.have_sel = 1;
        }                                                       /* 0 = Clear Followspot: the scene keeps what was applied (04 s13 "release") */
    } else {                                                    /* at k seqfades <attr,attr> <sfx> [all] */
        lj_seqfades f; char buf[128], *t; int a, i;
        uint8_t all[LJ_FIXTURES];
        if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "all")) || lj_str_copy(buf, sizeof buf, argv[1])) return rp_fail(p, "seqfades: expected `seqfades <attr,...> <sfx> [all]`");
        memset(&f, 0, sizeof f);
        for (t = strtok(buf, ","); t; t = strtok(NULL, ",")) { if ((a = lj_word_index(attr_words, t)) < 0) return rp_bad(p, "attr", t); f.attr_mask = (uint8_t)(f.attr_mask | 1u << a); }
        if ((a = lj_word_index(sfx_words, argv[2])) < 0) return rp_bad(p, "sfx", argv[2]);
        f.sfx = (uint8_t)a; f.all_scenes = argc == 4;
        for (i = 0; i < LJ_FIXTURES; i++) all[i] = 1;
        if (lj_seqfades_apply(&f, p->show, s, cur, rp_gfs.have_sel ? rp_gfs.sel : all) < 0) return rp_fail(p, "seqfades: lj_seqfades_apply failed");
    }
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_CURRENT_SEQ; c.a = (uint32_t)cur; c.seq[0] = s; c.at_us = (uint64_t)k * p->tick_us;
    return lj_engine_cmd(p->e, &c) ? rp_fail(p, "lj_engine_cmd: command array full") : 0;
}

/* ---- W3 cue list (cuelist-core; docs/replay-grammar.md "W3 lines"): `cl` / `clline` build lines, `at k cl_*`, `expect_cl`.
 * The harness is the UI thread of design 2.3 rule 4: lj_cl_run_step every LJ_CL_STEP_MS of synthetic time before each
 * tick (rp_cl_steps from rp_run_to), effects through this lj_cl_host at that time. ---- */
static const char *const rp_cl_state_words[] = { "idle", "running", "wait_go", NULL };   /* enum lj_cl_state */
static const char *const rp_cl_adv_words[] = { "auto", "manual", NULL };              /* enum lj_cl_adv */

static int rp_cl_load_cue(void *ud, uint32_t n)
{
    rp *p = (rp *)ud; lj_cmd c; int i;
    const lj_cue *cue = (const lj_cue *)lj_table_get(&p->show->cues, n);
    if (!cue) return -1;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_LOAD_CUE; c.a = n; c.cue = cue; c.at_us = rp_clx.now;
    for (i = 0; i < LJ_CUE_SLOTS; i++) c.seq[i] = cue->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, cue->slot[i].seq) : NULL;
    c.bg = cue->bg_cue ? (const lj_bgcue *)lj_table_get(&p->show->bgcues, cue->bg_cue) : NULL;
    if (c.bg) for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
    c.gm = cue->gen_macro ? (const lj_genmacro *)lj_table_get(&p->show->macros, cue->gen_macro) : NULL;
    p->cue = (lj_cue *)cue;
    return lj_engine_cmd(p->e, &c);
}
static int rp_cl_post(void *ud, const lj_cmd *cmd) { rp *p = (rp *)ud; lj_cmd c = *cmd; if (!c.at_us) c.at_us = rp_clx.now; return lj_engine_cmd(p->e, &c); }
static int rp_cl_load_bg(void *ud, uint32_t n)
{
    rp *p = (rp *)ud; lj_cmd c; int i;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_LOAD_BG; c.a = n; c.at_us = rp_clx.now;
    if (!(c.bg = (const lj_bgcue *)lj_table_get(&p->show->bgcues, n))) return -1;
    for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
    return lj_engine_cmd(p->e, &c);
}
static int rp_cl_flash(void *ud, uint32_t seq, int on)
{
    rp *p = (rp *)ud; lj_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_FLASH; c.b = (uint32_t)(on != 0); c.at_us = rp_clx.now;
    if (on && !(c.seq[0] = (const lj_sequence *)lj_table_get(&p->show->sequences, seq))) return -1;
    return lj_engine_cmd(p->e, &c);
}
static int rp_cl_link(void *ud, uint32_t n, const lj_cuelist **out) { rp *p = (rp *)ud; *out = (const lj_cuelist *)lj_table_get(&p->show->cuelists, n); return *out ? 0 : -1; }
static void rp_cl_fn(void *ud, int fn) { (void)ud; rp_clx.fn_calls++; rp_clx.fn_last = fn; }
static int rp_cl_exec(void *ud, const char *path, const char *args) { (void)ud; (void)path; (void)args; return -1; }   /* [CUELIST2] AllowExecute=0 */
static int rp_cl_timecode(void *ud, int src, uint32_t *ms) { (void)ud; (void)src; (void)ms; return -1; }              /* no MTC / CD / DA / MP */
static int rp_cl_clock(void *ud, uint32_t *ms_of_day, uint32_t *day)   /* synthetic midnight + t */
{
    uint64_t ms = rp_clx.now / 1000u; (void)ud;
    *ms_of_day = (uint32_t)(ms % 86400000u); *day = (uint32_t)(ms / 86400000u);
    return 0;
}
static const lj_cl_host *rp_cl_host(rp *p)
{
    static lj_cl_host h;
    h.ud = p; h.load_cue = rp_cl_load_cue; h.post = rp_cl_post; h.load_bg = rp_cl_load_bg; h.flash = rp_cl_flash; h.link = rp_cl_link;
    h.fn = rp_cl_fn; h.exec = rp_cl_exec; h.clear_cue = NULL; h.timecode = rp_cl_timecode; h.clock = rp_cl_clock;
    return &h;
}

/* every 10 ms of synthetic time up to `until` (design 2.3 rule 4) */
static void rp_cl_steps(rp *p, uint64_t until)
{
    if (!rp_clx.on) return;
    while (rp_clx.t + LJ_CL_STEP_MS * 1000u <= until) {
        rp_clx.t += LJ_CL_STEP_MS * 1000u;
        rp_clx.now = rp_clx.t;
        lj_cl_run_step(&rp_clx.run, rp_clx.now, rp_cl_host(p));
    }
}

static int rp_cl(rp *p, char **argv, int argc)   /* cl n [name="<name>"] [tracking=0|1] */
{
    long v, n; int i; char key[32]; const char *val; lj_cuelist *cl;
    if (argc < 2 || rp_num(argv[1], 1, LJ_NUMBER_MAX, &n)) return rp_fail(p, "cl: expected `cl <number> [name=] [tracking=]`");
    if (lj_table_get(&p->show->cuelists, (uint32_t)n)) return rp_fail(p, "cl %ld already defined", n);
    cl = lj_cuelist_new_rec();
    cl->number = (uint32_t)n;
    if (lj_table_put(&p->show->cuelists, (uint32_t)n, cl)) { lj_cuelist_free(cl); return rp_fail(p, "lj_table_put failed"); }
    rp_clx.build = cl; p->seq = NULL; p->scene = NULL; p->cue = NULL; p->bg = NULL; p->macro = NULL;
    for (i = 2; i < argc; i++) {
        KV_NEXT(i);
        KV_STR("name", cl->name);
        KV_NUM("tracking", 0, 1, cl->tracking, uint8_t);
        return rp_fail(p, "cl: unknown key %s", key);
    }
    return 0;
}

static int rp_clline(rp *p, char **argv, int argc)   /* clline adv= cmd= [param=] [pstr=] [ref=] [cue=] [trig=] [comment=] */
{
    long v; int i; char key[32]; const char *val; lj_cl_line l; const char *msg;
    if (!rp_clx.build) return rp_fail(p, "clline outside a cl");
    memset(&l, 0, sizeof l);
    for (i = 1; i < argc; i++) {
        KV_NEXT(i);
        KV_WORD("adv", rp_cl_adv_words, l.manual, uint8_t);
        KV_WORD("cmd", lj_cl_cmd_words, l.cmd, uint8_t);
        KV_NUM("param", 0, 86399999L, l.param_ms, uint32_t);
        KV_STR("pstr", l.param);
        KV_NUM("ref", 0, LJ_NUMBER_MAX, l.ref, uint32_t);
        KV_NUM("cue", 0, LJ_NUMBER_MAX, l.cue, uint32_t);
        KV_NUM("trig", 0, 0xFFF, l.trig_mask, uint16_t);
        KV_STR("comment", l.comment);
        return rp_fail(p, "clline: unknown key %s", key);
    }
    l.load_error = (uint8_t)(lj_cl_cmd_inert(l.cmd) || l.cmd == LJ_CL_SET_CURRENT_CUE || l.cmd == LJ_CL_SET_CUE_TRIG);   /* as lj_cuelist_load sets it */
    lj_cuelist_insert(rp_clx.build, rp_clx.build->n, &l);
    if ((msg = lj_cuelist_validate(rp_clx.build)) != NULL) return rp_fail(p, "cl %u invalid: %s", (unsigned)rp_clx.build->number, msg);
    return 0;
}

/* at k cl_run <n> top|line=<l> | cl_go | cl_back | cl_stop [clear] | cl_jump <l> | cl_mark goto|run <m> | cl_run_to <l> */
static int rp_cl_at(rp *p, long k, char **argv, int argc)
{
    const char *w = argv[2];
    const lj_cuelist *cl = rp_clx.run.cl;
    long n = 0;
    int op;
    if (!strcmp(w, "cl_run")) {
        if (argc != 5 || rp_num(argv[3], 1, LJ_NUMBER_MAX, &n)) return rp_fail(p, "cl_run: expected `cl_run <n> top|line=<l>`");
        if (!(cl = (const lj_cuelist *)lj_table_get(&p->show->cuelists, (uint32_t)n))) return rp_fail(p, "cl %ld is not defined", n);
        if (!strcmp(argv[4], "top")) { op = LJ_CL_OP_TOP; n = 0; }
        else if (!strncmp(argv[4], "line=", 5) && !rp_num(argv[4] + 5, 1, (long)cl->n, &n)) op = LJ_CL_OP_RUN;
        else return rp_fail(p, "cl_run: expected top or line=<1..%u>", (unsigned)cl->n);
    } else if (!strcmp(w, "cl_go") || !strcmp(w, "cl_back")) {
        if (argc != 3) return rp_fail(p, "%s takes no arguments", w);
        op = w[3] == 'g' ? LJ_CL_OP_GO : LJ_CL_OP_BACK;
    } else if (!strcmp(w, "cl_stop")) {
        if (argc > 4 || (argc == 4 && strcmp(argv[3], "clear"))) return rp_fail(p, "cl_stop: expected [clear]");
        op = argc == 4 ? LJ_CL_OP_STOP_CLEAR : LJ_CL_OP_STOP;
    } else if (!strcmp(w, "cl_jump") || !strcmp(w, "cl_run_to")) {
        if (argc != 4 || rp_num(argv[3], 1, 1000000, &n)) return rp_fail(p, "%s: expected a line number", w);
        op = w[3] == 'j' ? LJ_CL_OP_JUMP : LJ_CL_OP_RUN_TO;
    } else {
        if (argc != 5 || (strcmp(argv[3], "goto") && strcmp(argv[3], "run")) || rp_num(argv[4], 1, LJ_MARKS, &n)) return rp_fail(p, "cl_mark: expected `cl_mark goto|run <1..4>`");
        op = argv[3][0] == 'g' ? LJ_CL_OP_MARK_GOTO : LJ_CL_OP_MARK_RUN;
    }
    if (!cl) return rp_fail(p, "%s: no cue list pinned (cl_run first)", w);
    if (rp_run_to(p, k - 1)) return -1;
    rp_clx.on = 1;
    rp_cl_steps(p, (uint64_t)k * p->tick_us);                   /* the wakes before the click, then the click at k */
    rp_clx.t = rp_clx.now = (uint64_t)k * p->tick_us;
    if (lj_cl_run_op(&rp_clx.run, cl, op, (uint32_t)n, rp_clx.now, rp_cl_host(p))) return rp_fail(p, "%s: lj_cl_run_op refused (state %s)", w, rp_cl_state_words[rp_clx.run.st.state]);
    return 0;
}

static int rp_expect_cl(rp *p, char **argv, int argc)   /* expect_cl <tick> state=idle|running|wait_go [next=<l>] [last=<l>] */
{
    long k, v; int i, want; char key[32]; const char *val;
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 1)) return argc < 3 ? rp_fail(p, "expect_cl: expected `expect_cl <tick> state= [next=] [last=]`") : -1;
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 2; i < argc; i++) {
        long got;
        KV_NEXT(i);
        if (!strcmp(key, "state")) {
            if ((want = lj_word_index(rp_cl_state_words, val)) < 0) return rp_bad(p, key, val);
            if (want != rp_clx.run.st.state) rp_fail(p, "tick %ld cue list state expected %s got %s", k, val, rp_cl_state_words[rp_clx.run.st.state]);
            continue;
        }
        if (rp_num(val, 0, 1000000, &v)) return rp_bad(p, key, val);
        if (!strcmp(key, "next")) got = (long)rp_clx.run.st.next;
        else if (!strcmp(key, "last")) got = (long)rp_clx.run.st.last;
        else if (!strcmp(key, "fn")) got = (long)rp_clx.fn_calls;
        else return rp_fail(p, "expect_cl: unknown field %s", key);
        if (got != v) rp_fail(p, "tick %ld cue list %s expected %ld got %ld", k, key, v, got);
    }
    return 0;
}

/* ---- W3 movement: bez / node build lines; at k macro_fade | fx_edit | fanout | spread | autodelay; expect_mm.
 * Fan Out / Spread / Auto Delay do what their windows do: edit the current scene (the engine's view after tick k-1)
 * and post CURRENT_SEQ (seam 5). ---- */
enum { RP_BEZ = 8 };
static struct { lj_bez bez[RP_BEZ]; lj_bez_table t[RP_BEZ]; lj_bez_set set; int cur; } rp_mm;   /* one scenario at a time */

static int rp_mm_build(rp *p, char **argv, int argc)
{
    long n, v; int i; char key[32]; const char *val;
    if (!strcmp(argv[0], "bez")) {                            /* bez n name="..." */
        if (argc < 2 || rp_num(argv[1], 1, RP_BEZ, &n)) return rp_fail(p, "bez: expected `bez <1..8> name=\"...\"`");
        memset(&rp_mm.bez[n - 1], 0, sizeof rp_mm.bez[0]);
        for (i = 2; i < argc; i++) { KV_NEXT(i); KV_STR("name", rp_mm.bez[n - 1].name); return rp_fail(p, "bez: unknown key %s", key); }
        rp_mm.cur = (int)n;
        if (rp_mm.set.n < n) rp_mm.set.n = (int)n;
        rp_mm.set.t = rp_mm.t;
        p->show->bez = &rp_mm.set;                            /* the engine reads show->bez per tick */
        return 0;
    }
    {                                                         /* node x= y= [ix= iy= ox= oy= line=1] */
        lj_bez *b = rp_mm.cur ? &rp_mm.bez[rp_mm.cur - 1] : NULL;
        lj_bez_node *nd;
        int got_i = 0, got_o = 0;
        if (!b) return rp_fail(p, "node outside a bez");
        if (b->n >= LJ_BEZ_NODES_MAX) return rp_fail(p, "node: more than %d nodes", LJ_BEZ_NODES_MAX);
        nd = &b->node[b->n];
        memset(nd, 0, sizeof *nd);
        for (i = 1; i < argc; i++) {
            KV_NEXT(i);
            KV_NUM("x", -32768, 32767, nd->x, int16_t);
            KV_NUM("y", -32768, 32767, nd->y, int16_t);
            if (!strcmp(key, "ix")) { if (rp_num(val, -32768, 32767, &v)) return rp_bad(p, key, val); nd->in_x = (int16_t)v; got_i |= 1; continue; }
            if (!strcmp(key, "iy")) { if (rp_num(val, -32768, 32767, &v)) return rp_bad(p, key, val); nd->in_y = (int16_t)v; got_i |= 2; continue; }
            if (!strcmp(key, "ox")) { if (rp_num(val, -32768, 32767, &v)) return rp_bad(p, key, val); nd->out_x = (int16_t)v; got_o |= 1; continue; }
            if (!strcmp(key, "oy")) { if (rp_num(val, -32768, 32767, &v)) return rp_bad(p, key, val); nd->out_y = (int16_t)v; got_o |= 2; continue; }
            KV_NUM("line", 0, 1, nd->line, uint8_t);
            return rp_fail(p, "node: unknown key %s", key);
        }
        if (!(got_i & 1)) nd->in_x = nd->x;
        if (!(got_i & 2)) nd->in_y = nd->y;
        if (!(got_o & 1)) nd->out_x = nd->x;
        if (!(got_o & 2)) nd->out_y = nd->y;
        b->n++;
        if (b->n >= 3 && lj_bez_sample(b, &rp_mm.t[rp_mm.cur - 1])) return rp_fail(p, "bez %d: %s", rp_mm.cur, lj_bez_validate(b) ? lj_bez_validate(b) : "lj_bez_sample failed");
        return 0;
    }
}

/* the fixture's pan / tilt in readout units (17 s1.3) from a scene, and back (an Off channel becomes Snap) */
static int rp_mm_pt(rp *p, int f, int *pan, int *tilt, int *wide)
{
    const lj_profile *pr = lj_engine_post_ctx(p->e)->profile_of[f];
    const lj_fixture *fx = &p->show->fixture[f];
    if (!pr || !fx->link || !(pr->pt16 || pr->pt8)) return -1;
    *pan = lj_addr_code(fx->link, fx->address) + pr->pan_ofs; *tilt = lj_addr_code(fx->link, fx->address) + pr->tilt_ofs; *wide = pr->pt16 != 0;
    return (*pan + 1 < LJ_CHANS && *tilt + 1 < LJ_CHANS) ? 0 : -1;
}
static int32_t rp_mm_get(const lj_scene *sc, int ch, int wide) { return wide ? (sc->val[ch] << 8 | sc->val[ch + 1]) - 32768 : (sc->val[ch] << 8) - 32768; }
static void rp_mm_set(lj_scene *sc, int ch, int wide, int32_t v)
{
    uint16_t d = (uint16_t)(v + 32768);                       /* ME-22: the pair written as one 16-bit value with one state */
    sc->val[ch] = (uint8_t)(d >> 8);
    if (sc->state[ch] == LJ_OFF) sc->state[ch] = LJ_SNAP;
    if (wide) { sc->val[ch + 1] = (uint8_t)d; sc->state[ch + 1] = sc->state[ch]; }
}

static int rp_mm_order(rp *p, const char *s, int *order)
{
    uint32_t m[LJ_FIXTURES]; int k, n = lj_kv_list(s, m, LJ_FIXTURES);
    if (n < 1) return -1;
    for (k = 0; k < n; k++) { if (m[k] < 1 || m[k] > LJ_FIXTURES || !p->show->fixture[m[k] - 1].link) return -1; order[k] = (int)m[k] - 1; }
    return n;
}

static int rp_mm_at(rp *p, long k, char **argv, int argc)
{
    static const char *const fade_words[] = { "out", "in", "off", "on", "freeze", "run", NULL };   /* enum lj_mm_ctl */
    static const char *const style_words[] = { "linear", "circular", "segments", "random", NULL };  /* enum lj_mm_delay_style */
    lj_sequence *s = p->show->current_seq;
    lj_scene *sc;
    lj_cmd c;
    long n, v = 0;
    int i, order[LJ_FIXTURES], cnt = 0;
    char key[32]; const char *val;
    memset(&c, 0, sizeof c);
    if (!strcmp(argv[0], "macro_fade")) {                    /* at k macro_fade out|in|off|on|freeze|run [ms] */
        if (argc < 2 || argc > 3 || (n = lj_word_index(fade_words, argv[1])) < 0 || (argc == 3 && rp_num(argv[2], 0, 600000, &v)))
            return rp_fail(p, "macro_fade: expected `macro_fade out|in|off|on|freeze|run [ms]`");
        c.kind = LJ_CMD_MACRO_FADE; c.a = (uint32_t)n; c.b = argc == 3 ? (uint32_t)v : 0;
        if (rp_run_to(p, k - 1)) return -1;
        c.at_us = (uint64_t)k * p->tick_us;
        return lj_engine_cmd(p->e, &c) ? rp_fail(p, "lj_engine_cmd: command array full") : 0;
    }
    if (!strcmp(argv[0], "fx_edit")) {                       /* at k fx_edit <seq> <scene> <fixture> macro=... [mst=...]: the Movement Macro window's in-place edit, no post */
        long sn, scn; lj_sequence *q; char *fv[16]; int r;
        if (argc < 5 || argc > 15 || rp_num(argv[1], 1, LJ_NUMBER_MAX, &sn) || rp_num(argv[2], 0, LJ_SCENES_MAX - 1, &scn)) return rp_fail(p, "fx_edit: expected `fx_edit <seq> <scene 0-based> <fixture> macro=...`");
        if ((q = (lj_sequence *)lj_table_get(&p->show->sequences, (uint32_t)sn)) == NULL || scn >= (long)q->n) return rp_fail(p, "fx_edit: seq %ld scene %ld is not defined", sn, scn);
        if (rp_run_to(p, k - 1)) return -1;
        fv[0] = (char *)"fx";
        for (i = 3; i < argc; i++) fv[i - 2] = argv[i];
        sc = p->scene; p->scene = &q->scenes[scn];
        r = rp_fx(p, fv, argc - 2);
        p->scene = sc;
        return r;
    }
    if (rp_run_to(p, k - 1)) return -1;
    if (!s || !s->scenes || !p->view.cur_scene || p->view.cur_scene > s->n) return rp_fail(p, "%s: no current sequence (`at k current <seq> <scene>` first)", argv[0]);
    sc = &s->scenes[p->view.cur_scene - 1];                  /* the view's scene X is 1-based */
    for (i = 1; i < argc; i++) if (!strncmp(argv[i], "order=", 6) && (cnt = rp_mm_order(p, argv[i] + 6, order)) < 0) return rp_bad(p, "order", argv[i] + 6);
    if (!cnt) return rp_fail(p, "%s: needs order=<f,f,...>", argv[0]);
    if (!strcmp(argv[0], "fanout") || !strcmp(argv[0], "spread")) {   /* at k fanout <dpan> <dtilt> home=0|1 order=..; at k spread pan|tilt order=.. */
        int32_t pan[LJ_FIXTURES], tilt[LJ_FIXTURES]; int pc[LJ_FIXTURES], tc[LJ_FIXTURES], wide[LJ_FIXTURES];
        long dp = 0, dt = 0, home = 0; int axis = 0;
        if (!strcmp(argv[0], "fanout")) {
            if (argc < 4 || rp_num(argv[1], 0, 127, &dp) || rp_num(argv[2], 0, 127, &dt)) return rp_fail(p, "fanout: expected `fanout <dpan 0..127> <dtilt 0..127> home=0|1 order=...`");
            for (i = 3; i < argc; i++) { if (!strncmp(argv[i], "order=", 6)) continue; KV_NEXT(i); KV_NUM("home", 0, 1, home, long); return rp_fail(p, "fanout: unknown key %s", key); }
        } else {
            if (argc != 3 || (axis = !strcmp(argv[1], "tilt") ? 1 : !strcmp(argv[1], "pan") ? 0 : -1) < 0) return rp_fail(p, "spread: expected `spread pan|tilt order=...`");
        }
        for (i = 0; i < cnt; i++) {
            int f = order[i];
            if (rp_mm_pt(p, f, &pc[f], &tc[f], &wide[f])) return rp_fail(p, "%s: fixture %d has no pan / tilt", argv[0], f + 1);
            pan[f] = rp_mm_get(sc, pc[f], wide[f]); tilt[f] = rp_mm_get(sc, tc[f], wide[f]);
        }
        if (argv[0][0] == 'f' ? lj_fan_out(pan, tilt, order, cnt, (int)dp, (int)dt, (int)home) : lj_fan_spread(axis ? tilt : pan, order, cnt))
            return rp_fail(p, "%s: needs 2 or more fixtures", argv[0]);
        for (i = 0; i < cnt; i++) { int f = order[i]; rp_mm_set(sc, pc[f], wide[f], pan[f]); rp_mm_set(sc, tc[f], wide[f], tilt[f]); }
    } else {                                                  /* at k autodelay <style> <max> [count=] [segments=] order=.. */
        long cn = 0, sg = 0; int st; uint8_t d[LJ_FIXTURES]; uint32_t seed = LJ_RNG_SEED;
        if (argc < 3 || (st = lj_word_index(style_words, argv[1])) < 0 || rp_num(argv[2], 1, 256, &v)) return rp_fail(p, "autodelay: expected `autodelay linear|circular|segments|random <max 1..256> ... order=...`");
        for (i = 3; i < argc; i++) { if (!strncmp(argv[i], "order=", 6)) continue; KV_NEXT(i); KV_NUM("count", 1, LJ_FIXTURES, cn, long); KV_NUM("segments", 1, LJ_FIXTURES, sg, long); return rp_fail(p, "autodelay: unknown key %s", key); }
        if (lj_mm_auto_delay(cnt, st, (int)v, (int)cn, (int)sg, &seed, d)) return rp_fail(p, "autodelay: bad arguments (count= / segments= needed by the style)");
        for (i = 0; i < cnt; i++) { sc->macro[order[i]].delay = d[i]; if (sc->macro[order[i]].fstate[LJ_MMP_DELAY] == LJ_OFF) sc->macro[order[i]].fstate[LJ_MMP_DELAY] = LJ_SNAP; }
    }
    c.kind = LJ_CMD_CURRENT_SEQ; c.a = p->view.cur_scene - 1u; c.seq[0] = s; c.at_us = (uint64_t)k * p->tick_us;
    return lj_engine_cmd(p->e, &c) ? rp_fail(p, "lj_engine_cmd: command array full") : 0;
}

static int rp_expect_mm(rp *p, char **argv, int argc)        /* expect_mm <tick> <fixture> pan=<idx> tilt=<idx> */
{
    long k, f, v; int i; char key[32]; const char *val;
    if (argc < 4 || rp_tick_arg(p, argv[1], &k, 1) || rp_num(argv[2], 1, LJ_FIXTURES, &f)) return rp_fail(p, "expect_mm: expected `expect_mm <tick> <fixture> pan=<idx> tilt=<idx>`");
    for (i = 3; i < argc; i++) { KV_NEXT(i); if (rp_num(val, 0, LJ_MOVEMACRO_N_TABLE - 1, &v) || (strcmp(key, "pan") && strcmp(key, "tilt"))) return rp_bad(p, key, val); }
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 3; i < argc; i++) {
        long got;
        rp_key(argv[i], key, sizeof key, &val); rp_num(val, 0, LJ_MOVEMACRO_N_TABLE - 1, &v);
        got = key[0] == 'p' ? p->view.mm_idx_pan[f - 1] : p->view.mm_idx_tilt[f - 1];
        if (got != v) rp_fail(p, "tick %ld fixture %ld macro index %s expected %ld got %ld", k, f, key, v, got);
    }
    return 0;
}

/* ---- W3 genmacro: the `gm_shapes <file>` build line and `at k genmacro_on|genmacro_restart|ack_genmacro|gm_shapes` ---- */
static struct { lj_gm_shapes sh; int have; } rp_gm;   /* the scenario's override table (ME-34), freed when the scenario ends */

static int rp_gm_shapes(rp *p, char **argv, int argc)
{
    char path[600], err[200];
    const char *sl = strrchr(p->path, '/'), *bs = strrchr(p->path, '\\');
    if (argc != 2) return rp_fail(p, "gm_shapes: expected `gm_shapes <file>`");
    if (bs > sl) sl = bs;
    if (snprintf(path, sizeof path, "%.*s%s", sl ? (int)(sl - p->path + 1) : 0, p->path, argv[1]) >= (int)sizeof path) return rp_fail(p, "gm_shapes: path too long");
    lj_gm_shapes_free(&rp_gm.sh); rp_gm.have = 0;
    if (lj_gm_shapes_load(path, NULL, &rp_gm.sh, err, sizeof err)) return rp_fail(p, "gm_shapes: %s", err);
    rp_gm.have = 1;
    return 0;
}

static int rp_gm_at(rp *p, long k, char **argv, int argc)
{
    const char *w = argv[2];
    lj_cmd c;
    long v;
    memset(&c, 0, sizeof c);
    c.at_us = (uint64_t)k * p->tick_us;
    if (!strcmp(w, "genmacro_on")) {                           /* at k genmacro_on engine|slot 0|1 */
        if (argc != 5 || (strcmp(argv[3], "engine") && strcmp(argv[3], "slot")) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "genmacro_on: expected `genmacro_on engine|slot 0|1`");
        c.kind = LJ_CMD_GENMACRO_ON; c.a = argv[3][0] == 'e' ? LJ_GM_SRC_ENGINE : LJ_GM_SRC_CUE; c.b = (uint32_t)v;
    } else if (!strcmp(w, "genmacro_restart")) {               /* at k genmacro_restart [editor] */
        if (argc > 4 || (argc == 4 && strcmp(argv[3], "editor"))) return rp_fail(p, "genmacro_restart: expected [editor]");
        c.kind = LJ_CMD_GENMACRO_RESTART; c.a = argc == 4 ? LJ_GM_SRC_EDITOR : LJ_GM_SRC_CUE;
    } else if (!strcmp(w, "ack_genmacro")) {                   /* services pending_genmacro as lj_app_service_pending does: the cue loaded it, so its restart flag applies */
        if (argc != 3) return rp_fail(p, "ack_genmacro takes no arguments");
        if (rp_run_to(p, k - 1)) return -1;
        if (!p->view.pending_genmacro) return rp_fail(p, "ack_genmacro: no pending generic macro after tick %ld", k - 1);
        c.gm = (const lj_genmacro *)lj_table_get(&p->show->macros, p->view.pending_genmacro);
        if (!c.gm) return rp_fail(p, "ack_genmacro: macro %u is not defined", p->view.pending_genmacro);
        c.kind = LJ_CMD_LOAD_GENMACRO; c.a = p->view.pending_genmacro; c.b = c.gm->restart_on_load; c.c = LJ_GM_SRC_CUE;
    } else if (!strcmp(w, "gm_shapes")) {                      /* at k gm_shapes builtin|loaded */
        if (argc != 4 || (strcmp(argv[3], "builtin") && strcmp(argv[3], "loaded"))) return rp_fail(p, "gm_shapes: expected `gm_shapes builtin|loaded`");
        if (argv[3][0] == 'l' && !rp_gm.have) return rp_fail(p, "gm_shapes loaded: no `gm_shapes <file>` build line");
        c.kind = LJ_CMD_GM_SHAPES; c.gm_shapes = argv[3][0] == 'l' ? &rp_gm.sh : NULL;
    } else return rp_fail(p, "unknown command %s", w);
    if (rp_run_to(p, k - 1)) return -1;
    return lj_engine_cmd(p->e, &c) ? rp_fail(p, "lj_engine_cmd: command array full") : 0;
}

static int rp_at(rp *p, char **argv, int argc)
{
    long k, v = 0, n = 0; lj_cmd c; const char *w; int i; uint32_t id = 0;
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 0)) return argc < 3 ? rp_fail(p, "at: expected `at <tick> <cmd> ...`") : -1;
    memset(&c, 0, sizeof c);
    c.at_us = (uint64_t)k * p->tick_us;
    if (!strncmp(argv[argc - 1], "at_us=", 6)) {
        if (rp_num(argv[argc - 1] + 6, 0, 2000000000L, &v)) return rp_bad(p, "at_us", argv[argc - 1] + 6);
        c.at_us = (uint64_t)v;
        argc--;
    }
    w = argv[2];
    if (!strcmp(w, "gpatch") || !strcmp(w, "default") || !strcmp(w, "scene_edit")) {   /* W2 runtime edits: no command; the htp_groups / global_patch / default_out line hands them over */
        if (c.at_us != (uint64_t)k * p->tick_us) return rp_fail(p, "%s: at_us= does not apply to an edit", w);
        p->edit_tick = k;
        return w[0] == 'g' ? rp_gpatch(p, argv + 2, argc - 2) : w[0] == 'd' ? rp_default(p, argv + 2, argc - 2) : rp_scene_edit(p, argv + 2, argc - 2);
    }
    if (!strcmp(w, "gdc") || !strcmp(w, "followspot") || !strcmp(w, "seqfades")) return rp_gfs_at(p, k, argv + 2, argc - 2);   /* W3 gdc-fades-followspot */
    if (!strncmp(w, "cl_", 3)) return rp_cl_at(p, k, argv, argc);   /* W3 cuelist-core */
    if (!strcmp(w, "macro_fade") || !strcmp(w, "fx_edit") || !strcmp(w, "fanout") || !strcmp(w, "spread") || !strcmp(w, "autodelay")) return rp_mm_at(p, k, argv + 2, argc - 2);   /* W3 movement */
    if (!strcmp(w, "genmacro_on") || !strcmp(w, "genmacro_restart") || !strcmp(w, "ack_genmacro") || !strcmp(w, "gm_shapes")) return rp_gm_at(p, k, argv, argc);   /* W3 genmacro */
    if (!strcmp(w, "load_cue")) {
        const lj_cue *cue; int nobg = 0, nogm = 0;
        if (argc < 4 || rp_num(argv[3], 1, LJ_NUMBER_MAX, &n)) return rp_fail(p, "load_cue: expected a cue number");
        cue = (const lj_cue *)lj_table_get(&p->show->cues, (uint32_t)n);
        if (!cue) return rp_fail(p, "cue %ld is not defined", n);
        c.kind = LJ_CMD_LOAD_CUE; c.a = (uint32_t)n; c.cue = cue;
        for (i = 4; i < argc; i++) {                           /* [edit] [nobg]: nobg = the UI had no bg record to pin, so the engine requests it (pending_bg, ack_bg) */
            if (!strcmp(argv[i], "edit")) c.b = 1;
            else if (!strcmp(argv[i], "nobg")) nobg = 1;
            else if (!strcmp(argv[i], "nogm")) nogm = 1;         /* W3 genmacro: the UI had no macro record to pin (pending_genmacro, ack_genmacro) */
            else return rp_fail(p, "load_cue: expected [edit] [nobg] [nogm], got %s", argv[i]);
        }
        for (i = 0; i < LJ_CUE_SLOTS; i++) c.seq[i] = cue->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, cue->slot[i].seq) : NULL;
        c.bg = cue->bg_cue && !nobg ? (const lj_bgcue *)lj_table_get(&p->show->bgcues, cue->bg_cue) : NULL;
        if (c.bg) for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
        c.gm = cue->gen_macro && !nogm ? (const lj_genmacro *)lj_table_get(&p->show->macros, cue->gen_macro) : NULL;
        p->cue = (lj_cue *)cue;                                 /* the loaded cue: what a later `loop` line edits */
    } else if (!strcmp(w, "load_bg")) {
        if (argc != 4 || rp_num(argv[3], 0, LJ_BG_FILES, &n)) return rp_fail(p, "load_bg: expected a bg cue number or 0");
        c.kind = LJ_CMD_LOAD_BG; c.a = (uint32_t)n;
        if (n) {
            c.bg = (const lj_bgcue *)lj_table_get(&p->show->bgcues, (uint32_t)n);
            if (!c.bg) return rp_fail(p, "bg %ld is not defined", n);
            for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
        }
    } else if (!strcmp(w, "load_genmacro")) {
        if (argc < 4 || argc > 6 || rp_num(argv[3], 0, LJ_MACROS_MAX, &n)) return rp_fail(p, "load_genmacro: expected a macro number or 0 [restart] [editor]");
        c.kind = LJ_CMD_LOAD_GENMACRO; c.a = (uint32_t)n;
        for (i = 4; i < argc; i++) {                           /* W3: [editor] = c LJ_GM_SRC_EDITOR (the Generic Macro Editor's working record) */
            if (!strcmp(argv[i], "restart")) c.b = 1;
            else if (!strcmp(argv[i], "editor")) c.c = LJ_GM_SRC_EDITOR;
            else return rp_fail(p, "load_genmacro: expected [restart] [editor]");
        }
        if (n && !(c.gm = (const lj_genmacro *)lj_table_get(&p->show->macros, (uint32_t)n))) return rp_fail(p, "macro %ld is not defined", n);
    } else if (!strcmp(w, "current")) {
        if (argc < 4 || rp_num(argv[3], 0, LJ_NUMBER_MAX, &n)) return rp_fail(p, "current: expected `current <seq> <scene> [blind]` or `current 0`");
        c.kind = LJ_CMD_CURRENT_SEQ;
        if (n) {
            lj_sequence *s = (lj_sequence *)lj_table_get(&p->show->sequences, (uint32_t)n);
            if (!s) return rp_fail(p, "seq %ld is not defined", n);
            if (argc < 5 || argc > 6 || rp_num(argv[4], 0, LJ_SCENES_MAX - 1, &v)) return rp_fail(p, "current: expected a 0-based scene index");
            c.a = (uint32_t)v; c.seq[0] = s;
            if (argc == 6) { if (strcmp(argv[5], "blind")) return rp_fail(p, "current: expected [blind]"); c.b = 1; }
            p->show->current_seq = s;
        } else if (argc != 4) return rp_fail(p, "current 0 takes no arguments");
        else p->show->current_seq = NULL;
    } else if (!strcmp(w, "go") || !strcmp(w, "stop")) {
        if (argc != 4 || rp_runner(argv[3], &id)) return rp_fail(p, "%s: expected a runner (slot<k> flash current bg<k> all)", w);
        c.kind = (uint8_t)(w[0] == 'g' ? LJ_CMD_GO : LJ_CMD_STOP); c.a = id;
    } else if (!strcmp(w, "trig")) {
        if (argc < 4 || rp_runner(argv[3], &id)) return rp_fail(p, "trig: expected a runner");
        c.kind = LJ_CMD_TRIG; c.a = id; c.b = 0xFF;
        for (i = 4; i < argc; i++) {
            int d;
            if (!strncmp(argv[i], "scenes=", 7)) { if (rp_num(argv[i] + 7, 0, 255, &v)) return rp_bad(p, "scenes", argv[i] + 7); c.c = (uint32_t)v; }
            else if (!strcmp(argv[i], "default")) c.b = 0xFF;
            else if ((d = lj_word_index(lj_dir_words, argv[i])) >= 0) c.b = (uint32_t)d;
            else return rp_fail(p, "trig: unknown argument %s", argv[i]);
        }
    } else if (!strcmp(w, "enable")) {
        if (argc < 5 || argc > 6 || rp_slotword(argv[3], &n) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "enable: expected `enable slot<k> 0|1 [edit|out]`");
        c.kind = LJ_CMD_ENABLE; c.a = (uint32_t)(LJ_RUN_SLOT0 + n - 1); c.b = (uint32_t)v;
        if (argc == 6) { if (!strcmp(argv[5], "edit")) c.c = 1; else if (!strcmp(argv[5], "out")) c.c = 2; else return rp_fail(p, "enable: expected edit|out"); }
    } else if (!strcmp(w, "time")) {
        if (argc != 5 || rp_num(argv[4], 0, 999, &v)) return rp_fail(p, "time: expected `time cue|slot<k> <0..999>`");
        if (!strcmp(argv[3], "cue")) n = 0; else if (rp_slotword(argv[3], &n)) return rp_fail(p, "time: expected cue|slot<k>");
        c.kind = LJ_CMD_TIME; c.a = (uint32_t)n; c.b = (uint32_t)v;
    } else if (!strcmp(w, "chase")) {
        if (argc < 5 || argc > 6 || rp_slotword(argv[3], &n) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "chase: expected `chase slot<k> 0|1 [fade=0|1]`");
        c.kind = LJ_CMD_CHASE; c.a = (uint32_t)(LJ_RUN_SLOT0 + n - 1); c.b = (uint32_t)v; c.c = 1;
        if (argc == 6) { if (strncmp(argv[5], "fade=", 5) || rp_num(argv[5] + 5, 0, 1, &v)) return rp_fail(p, "chase: expected fade=0|1"); c.c = (uint32_t)v; }
    } else if (!strcmp(w, "master")) {
        if (argc < 5 || argc > 6 || rp_num(argv[4], 0, LJ_GM_DOMAIN, &v)) return rp_fail(p, "master: expected `master gm|sub<k> <0..255> [prop|lotp|htp|none]`");
        if (!strcmp(argv[3], "gm")) n = 0; else if (strncmp(argv[3], "sub", 3) || rp_num(argv[3] + 3, 1, LJ_SUBMASTERS, &n)) return rp_fail(p, "master: expected gm|sub<k>");
        c.kind = LJ_CMD_SET_MASTER; c.a = (uint32_t)n; c.b = (uint32_t)v;
        if (n) c.c = (uint32_t)p->show->submaster_mode[n - 1];
        if (argc == 6) {                                       /* the mode lives in the model (lj_show.submaster_mode[], rules header): set it as the Master Intensity window does, and carry it in c */
            int m = lj_word_index(rp_master_words, argv[5]);
            if (m < 0) return rp_fail(p, "master: unknown mode %s", argv[5]);
            if (!n) return rp_fail(p, "master: gm takes no mode");
            p->show->submaster_mode[n - 1] = (uint8_t)m; c.c = (uint32_t)m;
        }
    } else if (!strcmp(w, "gm_fade")) {
        if (argc != 5 || rp_num(argv[4], LJ_GM_FADE_MIN_MS, LJ_GM_FADE_MAX_MS, &v)) return rp_fail(p, "gm_fade: expected `gm_fade out|in <500..10000>`");
        if (!strcmp(argv[3], "out")) n = 0; else if (!strcmp(argv[3], "in")) n = 1; else return rp_fail(p, "gm_fade: expected out|in");
        c.kind = LJ_CMD_GM_FADE; c.a = (uint32_t)n; c.b = (uint32_t)v;
    } else if (!strcmp(w, "blackout") || !strcmp(w, "freeze") || !strcmp(w, "offline") || !strcmp(w, "disable_fade")) {
        if (argc != 4 || rp_num(argv[3], 0, 1, &v)) return rp_fail(p, "%s: expected 0|1", w);
        c.kind = (uint8_t)(w[0] == 'b' ? LJ_CMD_BLACKOUT : w[0] == 'f' ? LJ_CMD_FREEZE : w[0] == 'o' ? LJ_CMD_OFFLINE : LJ_CMD_DISABLE_FADE);
        c.b = (uint32_t)v;
    } else if (!strcmp(w, "solo")) {
        if (argc != 5 || rp_num(argv[3], 1, LJ_FIXTURES, &n) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "solo: expected `solo <fixture> 0|1`");
        if (!p->show->fixture[n - 1].link) return rp_fail(p, "solo: fixture %ld is not patched", n);
        c.kind = LJ_CMD_SOLO; c.a = (uint32_t)(n - 1); c.b = (uint32_t)v;
    } else if (!strcmp(w, "static")) {
        long pg, sl;
        if (argc != 5 || rp_pageslot(argv[3], &pg, &sl) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "static: expected `static <page>.<slot> 0|1`");
        if (pg > p->show->npages || p->show->pages[pg - 1].slot[sl - 1].kind == LJ_STATIC_EMPTY) return rp_fail(p, "static %ld.%ld is not defined", pg, sl);
        c.kind = LJ_CMD_STATIC; c.a = (uint32_t)((pg - 1) * LJ_STATICS2_SLOTS_MAX + (sl - 1)); c.b = (uint32_t)v;   /* a = global slot index (page-1)*64 + slot-1 */
        c.st = v ? &p->show->pages[pg - 1].slot[sl - 1] : NULL;
        if (c.st) c.seq[0] = rp_link_seq(p, c.st);
    } else if (!strcmp(w, "flash")) {
        if (argc != 5 || rp_num(argv[3], 1, LJ_NUMBER_MAX, &n) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "flash: expected `flash <seq> 0|1`");
        c.kind = LJ_CMD_FLASH; c.b = (uint32_t)v;
        if (v && !(c.seq[0] = (const lj_sequence *)lj_table_get(&p->show->sequences, (uint32_t)n))) return rp_fail(p, "seq %ld is not defined", n);
    } else if (!strcmp(w, "overwrite")) {
        int code;
        c.kind = LJ_CMD_OVERWRITE;
        if (argc == 4 && !strcmp(argv[3], "clear")) { c.a = 0xFFFF; c.b = 256; }
        else if (argc == 5 && !rp_chan(argv[3], NULL, &code)) {
            if (rp_need_patched(p, code)) return -1;
            c.a = (uint32_t)code;
            if (!strcmp(argv[4], "clear")) c.b = 256; else if (rp_num(argv[4], 0, 255, &v)) return rp_fail(p, "overwrite: expected a value or clear"); else c.b = (uint32_t)v;
        } else return rp_fail(p, "overwrite: expected `overwrite l:a <v>|clear` or `overwrite clear`");
    } else if (!strcmp(w, "htp")) {
        if (argc != 5 || rp_num(argv[3], 1, LJ_HTP_GROUPS, &n) || rp_num(argv[4], 0, 255, &v)) return rp_fail(p, "htp: expected `htp <1..24> <0..255>`");
        c.kind = LJ_CMD_HTP_GROUP; c.a = (uint32_t)n; c.b = (uint32_t)v;
    } else if (!strcmp(w, "seq_int")) {
        c.kind = LJ_CMD_SEQ_INT;
        if (argc == 5 && !strcmp(argv[3], "mode")) { int m = lj_word_index(rp_seqint_words, argv[4]); if (m < 0) return rp_fail(p, "seq_int: expected mode default|htp|lotp"); c.b = (uint32_t)m; }
        else if (argc == 5 && !rp_slotword(argv[3], &n) && !rp_num(argv[4], 0, 255, &v)) { c.a = (uint32_t)n; c.b = (uint32_t)v; }
        else return rp_fail(p, "seq_int: expected `seq_int slot<k> <0..255>` or `seq_int mode <word>`");
    } else if (!strcmp(w, "macro_ctl")) {
        if (argc != 5 || rp_num(argv[3], 0, LJ_MOVEMACRO_PCT_MAX, &n) || rp_num(argv[4], 0, LJ_MOVEMACRO_PCT_MAX, &v)) return rp_fail(p, "macro_ctl: expected `macro_ctl <amp 0..999> <speed 0..999>`");   /* ME-109 / LORD_ENGINE-10 */
        c.kind = LJ_CMD_MACRO_CTL; c.a = (uint32_t)n; c.b = (uint32_t)v;
    } else if (!strcmp(w, "sacn_cfg")) {
        if (argc != 3) return rp_fail(p, "sacn_cfg takes no arguments");
        lj_sacn_cfg_default(&p->sacn, "replay");
        c.kind = LJ_CMD_SACN_CFG; c.sacn = &p->sacn;
    /* ---- W2 forms (docs/replay-grammar.md "W2 command lines"); parse + post here, the engine track makes them act ---- */
    } else if (!strcmp(w, "bg_on")) {                          /* at k bg_on bg<1..5>|all 0|1 */
        if (argc != 5 || rp_runner(argv[3], &id) || (id != LJ_RUN_ALL && (id < LJ_RUN_BG0 || id >= LJ_RUN_BG0 + LJ_BG_SLOTS)) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "bg_on: expected `bg_on bg<k>|all 0|1`");
        c.kind = LJ_CMD_BG_ON; c.a = id; c.b = (uint32_t)v;
    } else if (!strcmp(w, "static_act")) {                     /* at k static_act p.s [level=<0..100>] [scope=add|slot|page|group|all|solo_slot|solo_page|solo_group|solo_all] */
        long pg, sl;
        if (argc < 4 || rp_pageslot(argv[3], &pg, &sl)) return rp_fail(p, "static_act: expected `static_act <page>.<slot> [level=] [scope=]`");
        if (pg > p->show->npages || p->show->pages[pg - 1].slot[sl - 1].kind == LJ_STATIC_EMPTY) return rp_fail(p, "static %ld.%ld is not defined", pg, sl);
        c.kind = LJ_CMD_STATIC_ACTIVATE; c.a = (uint32_t)lj_static_global_index((int)pg, (int)sl); c.b = 0xFFFF; c.c = 0xFF;
        c.st = &p->show->pages[pg - 1].slot[sl - 1];
        c.seq[0] = rp_link_seq(p, c.st);                       /* ME-88: a link slot carries its pinned sequence, as the Statics 2 window posts it */
        for (i = 4; i < argc; i++) {
            if (!strncmp(argv[i], "level=", 6)) { if (rp_num(argv[i] + 6, 0, 100, &v)) return rp_bad(p, "level", argv[i] + 6); c.b = (uint32_t)v; }
            else if (!strncmp(argv[i], "scope=", 6)) { int s = lj_word_index(lj_replace_words, argv[i] + 6); if (s < 0) return rp_bad(p, "scope", argv[i] + 6); c.c = (uint32_t)s; }
            else return rp_fail(p, "static_act: unknown argument %s", argv[i]);
        }
    } else if (!strcmp(w, "static_deact")) {                   /* at k static_deact p.s | all | page <p> */
        long pg, sl;
        c.kind = LJ_CMD_STATIC_DEACTIVATE;
        if (argc == 4 && !strcmp(argv[3], "all")) c.a = LJ_RUN_ALL;
        else if (argc == 5 && !strcmp(argv[3], "page") && !rp_num(argv[4], 1, LJ_STATIC_PAGES_MAX, &v)) { c.a = LJ_RUN_ALL; c.c = (uint32_t)v; }
        else if (argc == 4 && !rp_pageslot(argv[3], &pg, &sl)) c.a = (uint32_t)lj_static_global_index((int)pg, (int)sl);
        else return rp_fail(p, "static_deact: expected `static_deact <page>.<slot>` | `all` | `page <p>`");
    } else if (!strcmp(w, "smoke")) {                          /* at k smoke button|timer 0|1 [ms] | level|duration|cycle|haze|fan <v> | reset */
        int s;
        if (argc < 4 || (s = lj_word_index(rp_smoke_words, argv[3])) < 0) return rp_fail(p, "smoke: expected button|timer|level|duration|cycle|reset|haze|fan");
        c.kind = LJ_CMD_SMOKE; c.a = (uint32_t)s;
        if (s == LJ_SMOKE_RESET) { if (argc != 4) return rp_fail(p, "smoke reset takes no value"); }
        else if (argc < 5 || rp_num(argv[4], 0, s == LJ_SMOKE_DURATION_MS || s == LJ_SMOKE_CYCLE_MS ? LJ_SMOKE_CYCLE_MAX_MS : (s == LJ_SMOKE_BUTTON || s == LJ_SMOKE_TIMER) ? 1 : 255, &v)) return rp_fail(p, "smoke: bad value");
        else { c.b = (uint32_t)v; if (argc == 6) { if (s != LJ_SMOKE_BUTTON || rp_num(argv[5], 0, LJ_SMOKE_DURATION_MAX_MS, &v)) return rp_fail(p, "smoke: [ms] only after button"); c.c = (uint32_t)v; } else if (argc > 6) return rp_fail(p, "smoke: too many arguments"); }
    } else if (!strcmp(w, "flash_latch")) {                    /* at k flash_latch <seq> 0|1 : FLASH with c = 1 (Latch Flash Sequences); `flash_latch 0 0` = Release latched Flash */
        if (argc != 5 || rp_num(argv[3], 0, LJ_NUMBER_MAX, &n) || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "flash_latch: expected `flash_latch <seq> 0|1`");
        c.kind = LJ_CMD_FLASH; c.b = (uint32_t)v; c.c = (uint32_t)(n != 0);
        if (n && !(c.seq[0] = (const lj_sequence *)lj_table_get(&p->show->sequences, (uint32_t)n))) return rp_fail(p, "seq %ld is not defined", n);
    } else if (!strcmp(w, "transparent")) {                    /* at k transparent use|force|ignore */
        int m;
        if (argc != 4 || (m = lj_word_index(rp_transp_words, argv[3])) < 0) return rp_fail(p, "transparent: expected use|force|ignore");
        c.kind = LJ_CMD_TRANSPARENT; c.b = (uint32_t)m;
    } else if (!strcmp(w, "chase_tap")) {                      /* at k chase_tap slot<k>|all [at_us=<us>] : the 1 ms tap timer */
        if (argc != 4 || rp_runner(argv[3], &id) || (id != LJ_RUN_ALL && id >= LJ_CUE_SLOTS)) return rp_fail(p, "chase_tap: expected `chase_tap slot<k>|all [at_us=]`");
        c.kind = LJ_CMD_CHASE_TAP; c.a = id;
    } else if (!strcmp(w, "cue_time")) {                       /* at k cue_time cue|slot<k> <0..999> : == time (the 13 faders) */
        if (argc != 5 || rp_num(argv[4], LJ_TIME_PCT_CUE, LJ_TIME_PCT_MAX, &v)) return rp_fail(p, "cue_time: expected `cue_time cue|slot<k> <0..999>`");
        if (!strcmp(argv[3], "cue")) n = 0; else if (rp_slotword(argv[3], &n)) return rp_fail(p, "cue_time: expected cue|slot<k>");
        c.kind = LJ_CMD_CUE_TIME; c.a = (uint32_t)n; c.b = (uint32_t)v;
    } else if (!strcmp(w, "loop")) {                           /* at k loop slot<k> [dir=] [first=] [last=] [loops=] [end=] [goto=] [link=] [spt=] [atpt=] [tol=] : the Loop Options dialog on the loaded cue's slot */
        lj_cue_slot *s;
        if (argc < 4 || rp_slotword(argv[3], &n)) return rp_fail(p, "loop: expected `loop slot<k> key=value ...`");
        if (!p->cue) return rp_fail(p, "loop: no cue loaded in the scenario");
        s = &p->cue->slot[n - 1];                               /* the harness edits the pinned record the way the dialog does (under lj_lock in the app) */
        for (i = 4; i < argc; i++) {
            char key[32]; const char *val; int d;
            if (rp_key(argv[i], key, sizeof key, &val)) return rp_fail(p, "loop: bad field %s", argv[i]);
            if (!strcmp(key, "dir")) { if ((d = lj_word_index(lj_dir_words, val)) < 0) return rp_bad(p, key, val); s->dir = (uint8_t)d; }
            else if (!strcmp(key, "end")) { if ((d = lj_word_index(lj_end_words, val)) < 0) return rp_bad(p, key, val); s->end_action = (uint8_t)d; }
            else if (!strcmp(key, "first")) { if (rp_num(val, 0, LJ_SCENES_MAX, &v)) return rp_bad(p, key, val); s->first = (uint16_t)v; }
            else if (!strcmp(key, "last")) { if (rp_num(val, 0, LJ_SCENES_MAX, &v)) return rp_bad(p, key, val); s->last = (uint16_t)v; }
            else if (!strcmp(key, "loops")) { if (rp_num(val, 0, 65535, &v)) return rp_bad(p, key, val); s->loops = (uint16_t)v; }
            else if (!strcmp(key, "goto")) { if (rp_num(val, 0, LJ_SCENES_MAX, &v)) return rp_bad(p, key, val); s->goto_scene = (uint16_t)v; }
            else if (!strcmp(key, "link")) { if (rp_num(val, 0, LJ_NUMBER_MAX, &v)) return rp_bad(p, key, val); s->link_cue = (uint32_t)v; }
            else if (!strcmp(key, "spt")) { if (rp_num(val, 1, 255, &v)) return rp_bad(p, key, val); s->scenes_per_trig = (uint8_t)v; }
            else if (!strcmp(key, "atpt")) { if (rp_num(val, 1, 255, &v)) return rp_bad(p, key, val); s->audio_trigs_per_trig = (uint8_t)v; }
            else if (!strcmp(key, "tol")) { if (rp_num(val, 0, 1, &v)) return rp_bad(p, key, val); s->trig_on_load = (uint8_t)v; }
            else return rp_fail(p, "loop: unknown field %s", key);
        }
        if (lj_cue_validate(p->cue)) return rp_fail(p, "loop: %s", lj_cue_validate(p->cue));
        c.kind = LJ_CMD_LOOP_OPTS; c.a = (uint32_t)(LJ_RUN_SLOT0 + n - 1); c.slot = s;
    } else if (!strcmp(w, "link")) {                           /* at k link : the harness services pending_link_cue the way lj_app_service_pending does (LOAD_CUE of that number) */
        const lj_cue *cue;
        if (argc != 3) return rp_fail(p, "link takes no arguments");
        if (rp_run_to(p, k - 1)) return -1;
        if (!p->view.pending_link_cue) return rp_fail(p, "link: no pending link cue after tick %ld", k - 1);
        cue = (const lj_cue *)lj_table_get(&p->show->cues, p->view.pending_link_cue);
        if (!cue) return rp_fail(p, "link: cue %u is not defined", p->view.pending_link_cue);
        c.kind = LJ_CMD_LOAD_CUE; c.a = p->view.pending_link_cue; c.cue = cue;
        for (i = 0; i < LJ_CUE_SLOTS; i++) c.seq[i] = cue->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, cue->slot[i].seq) : NULL;
        c.bg = cue->bg_cue ? (const lj_bgcue *)lj_table_get(&p->show->bgcues, cue->bg_cue) : NULL;
        if (c.bg) for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
        c.gm = cue->gen_macro ? (const lj_genmacro *)lj_table_get(&p->show->macros, cue->gen_macro) : NULL;
        p->cue = (lj_cue *)cue;
    } else if (!strcmp(w, "ack_bg")) {                         /* at k ack_bg : the harness services pending_bg the way lj_app_service_pending does (LOAD_BG of that number, slots pinned) */
        if (argc != 3) return rp_fail(p, "ack_bg takes no arguments");
        if (rp_run_to(p, k - 1)) return -1;
        if (!p->view.pending_bg) return rp_fail(p, "ack_bg: no pending bg cue after tick %ld", k - 1);
        c.bg = (const lj_bgcue *)lj_table_get(&p->show->bgcues, p->view.pending_bg);
        if (!c.bg) return rp_fail(p, "ack_bg: bg %u is not defined", p->view.pending_bg);
        c.kind = LJ_CMD_LOAD_BG; c.a = p->view.pending_bg;
        for (i = 0; i < LJ_BG_SLOTS; i++) c.bgseq[i] = c.bg->slot[i].seq ? (const lj_sequence *)lj_table_get(&p->show->sequences, c.bg->slot[i].seq) : NULL;
    } else if (!strcmp(w, "htp_groups")) {                     /* at k htp_groups [masters=0|1] : the HTP table swap after `gpatch` edits (post-build gpatch lines are W2 replay-track grammar) */
        c.kind = LJ_CMD_HTP_GROUPS; c.patch = p->show->patch; c.b = LJ_HTP_MASTERS_DEFAULT;
        if (argc == 4) { if (strncmp(argv[3], "masters=", 8) || rp_num(argv[3] + 8, 0, 1, &v)) return rp_fail(p, "htp_groups: expected [masters=0|1]"); c.b = (uint32_t)v; }
        else if (argc != 3) return rp_fail(p, "htp_groups: too many arguments");
    } else if (!strcmp(w, "global_patch")) {                   /* at k global_patch : the patch swap */
        if (argc != 3) return rp_fail(p, "global_patch takes no arguments");
        c.kind = LJ_CMD_GLOBAL_PATCH; c.patch = p->show->patch;
    } else if (!strcmp(w, "default_out")) {                    /* at k default_out : rank 0 re-read after a runtime `default` edit */
        if (argc != 3) return rp_fail(p, "default_out takes no arguments");
        c.kind = LJ_CMD_DEFAULT_OUT; c.default_out = p->show->default_out;
    } else if (!strcmp(w, "pref")) {                           /* at k pref keep_seq_blind|global_chase_rate|disable_chase_fade|statics_fadetime|hotkey_trig_dir 0|1 */
        int m;
        if (argc != 5 || (m = lj_word_index(rp_pref_words, argv[3])) < 0 || rp_num(argv[4], 0, 1, &v)) return rp_fail(p, "pref: expected `pref <name> 0|1`");
        c.kind = LJ_CMD_PREFS; c.a = (uint32_t)m; c.b = (uint32_t)v;
    /* ---- W5 forms (docs/replay-grammar.md "W5 lines"; contracts seat 2026-10-23) ---- */
    } else if (!strcmp(w, "dmx_in")) {                         /* at k dmx_in <link 1..4> <ch>=<v> ... | clear : the UI's merged + translated input frame, unlisted slots 0 */
        static uint8_t rp_in[LJ_LINKS][LJ_LINK_CH];
        long ch;
        if (argc < 5 || rp_num(argv[3], 1, LJ_LINKS, &n)) return rp_fail(p, "dmx_in: expected `dmx_in <link> <ch>=<v> ...` or `dmx_in <link> clear`");
        c.kind = LJ_CMD_DMX_IN; c.a = (uint32_t)n;
        if (argc == 5 && !strcmp(argv[4], "clear")) c.dmx_in = NULL;   /* zeros: the loss rule */
        else {
            memset(rp_in[n - 1], 0, LJ_LINK_CH);
            for (i = 4; i < argc; i++) {
                char *eq = strchr(argv[i], '=');
                int bad;
                if (!eq) return rp_fail(p, "dmx_in: expected <ch>=<v>, got \"%s\"", argv[i]);
                *eq = 0; bad = rp_num(argv[i], 1, LJ_LINK_CH, &ch) || rp_num(eq + 1, 0, 255, &v); *eq = '=';
                if (bad) return rp_fail(p, "dmx_in: channel 1..512 and value 0..255, got \"%s\"", argv[i]);
                rp_in[n - 1][ch - 1] = (uint8_t)v;
            }
            c.dmx_in = rp_in[n - 1];
        }
    } else if (!strcmp(w, "overwrite_enable")) {               /* at k overwrite_enable 0|1 : the S1 gate (the DMX-in function Overwrite Enable) */
        if (argc != 4 || rp_num(argv[3], 0, 1, &v)) return rp_fail(p, "overwrite_enable: expected 0|1");
        c.kind = LJ_CMD_OVERWRITE_ENABLE; c.b = (uint32_t)v;
    } else if (!strcmp(w, "audio_trig")) {                     /* at k audio_trig : one analyzer trig (LJ_TRIG_AUDIO_EVENT) to the slots in audio trig mode */
        if (argc != 3) return rp_fail(p, "audio_trig takes no arguments");
        c.kind = LJ_CMD_TRIG; c.a = LJ_RUN_ALL; c.b = LJ_TRIG_AUDIO_EVENT;
    } else return rp_fail(p, "unknown command %s", w);
    if (rp_run_to(p, k - 1)) return -1;
    if (lj_engine_cmd(p->e, &c)) return rp_fail(p, "lj_engine_cmd: command array full");
    if (c.kind == LJ_CMD_SACN_CFG) p->sacn_posted = c.sacn;
    return 0;
}

/* ---- expectations: a failed expect is counted and the scenario continues ---- */
static int rp_expect(rp *p, char **argv, int argc, int merged)
{
    long k, v, layer = -1; int i, code, nch = 0; char b[16];
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 1)) return argc < 3 ? rp_fail(p, "expect: expected `expect <tick> l:a=v ... [layer=r]`") : -1;
    for (i = 2; i < argc; i++)                          /* grammar first, so a bad line never runs ticks */
        if (!strncmp(argv[i], "layer=", 6)) { if (merged || rp_num(argv[i] + 6, 0, LJ_L_COUNT - 1, &layer)) return rp_bad(p, "layer", argv[i] + 6); }
        else if (rp_chval(argv[i], &code, 255, &v, NULL)) return rp_fail(p, "expect: expected l:a=v, got \"%s\"", argv[i]);
        else if (rp_need_patched(p, code)) return -1;
        else nch++;
    if (!nch) return rp_fail(p, "expect: no channels");
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 2; i < argc; i++) {
        int got;
        if (!strncmp(argv[i], "layer=", 6)) continue;
        rp_chval(argv[i], &code, 255, &v, NULL);
        got = merged ? p->f->merged[code] : p->f->out[code / LJ_LINK_CH][code % LJ_LINK_CH];
        if (got != v) rp_fail(p, "tick %ld %s expected %ld got %d (%s, layer %d)", k, rp_chname(code, b), v, got, merged ? "merged" : "out", p->f->layer_of[code]);
        else if (layer >= 0 && p->f->layer_of[code] != layer) rp_fail(p, "tick %ld %s = %d from layer %d, expected layer %ld", k, rp_chname(code, b), got, p->f->layer_of[code], layer);
    }
    return 0;
}

static int rp_expect_pair(rp *p, char **argv, int argc)
{
    long k, v; int i, code; char b[16];
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 1)) return argc < 3 ? rp_fail(p, "expect_pair: expected `expect_pair <tick> l:a=<0..65535> ...`") : -1;
    for (i = 2; i < argc; i++) {
        if (rp_chval(argv[i], &code, 65535, &v, NULL)) return rp_fail(p, "expect_pair: expected l:a=v, got \"%s\"", argv[i]);
        if (code % LJ_LINK_CH == LJ_LINK_CH - 1) return rp_fail(p, "expect_pair: %s has no LSB address", rp_chname(code, b));
        if (rp_need_patched(p, code) || rp_need_patched(p, code + 1)) return -1;
    }
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 2; i < argc; i++) {
        int got;
        rp_chval(argv[i], &code, 65535, &v, NULL);
        got = p->f->out[code / LJ_LINK_CH][code % LJ_LINK_CH] * 256 + p->f->out[code / LJ_LINK_CH][code % LJ_LINK_CH + 1];
        if (got != v) rp_fail(p, "tick %ld %s pair expected %ld got %d (%d/%d, layer %d)", k, rp_chname(code, b), v, got, got >> 8, got & 255, p->f->layer_of[code]);
    }
    return 0;
}

static int rp_expect_range(rp *p, char **argv, int argc)
{
    long k, lo, hi; int i, code; const char *rest; char b[16], x;
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 1)) return argc < 3 ? rp_fail(p, "expect_range: expected `expect_range <tick> l:a=<lo>..<hi>`") : -1;
    for (i = 2; i < argc; i++) {
        if (rp_chan(argv[i], &rest, &code) || sscanf(rest, "=%ld..%ld%c", &lo, &hi, &x) != 2 || lo < 0 || hi > 255 || lo > hi) return rp_fail(p, "expect_range: expected l:a=lo..hi, got \"%s\"", argv[i]);
        if (rp_need_patched(p, code)) return -1;
    }
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 2; i < argc; i++) {
        int got;
        rp_chan(argv[i], &rest, &code); sscanf(rest, "=%ld..%ld", &lo, &hi);
        got = p->f->out[code / LJ_LINK_CH][code % LJ_LINK_CH];
        if (got < lo || got > hi) rp_fail(p, "tick %ld %s expected %ld..%ld got %d (layer %d)", k, rp_chname(code, b), lo, hi, got, p->f->layer_of[code]);
    }
    return 0;
}

static int rp_expect_status(rp *p, char **argv, int argc)
{
    long k, n, x = -1, y = -1; int st, i; char c;
    if (argc < 4 || rp_tick_arg(p, argv[1], &k, 1) || rp_slotword(argv[2], &n) || (st = lj_word_index(rp_status_words, argv[3])) < 0)
        return rp_fail(p, "expect_status: expected `expect_status <tick> slot<k> empty|running|stopped|load_error|disabled|edit|out [scene=x:y]`");
    for (i = 4; i < argc; i++)
        if (sscanf(argv[i], "scene=%ld:%ld%c", &x, &y, &c) != 2 || x < 0 || y < 0) return rp_fail(p, "expect_status: expected scene=x:y, got \"%s\"", argv[i]);
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    i = (int)n - 1;
    if (p->view.slot_status[i] != st) rp_fail(p, "tick %ld slot%ld status expected %s got %s", k, n, rp_status_words[st], lj_word(rp_status_words, p->view.slot_status[i]));
    if (x >= 0 && (p->view.slot_scene[i] != x || p->view.slot_scenes[i] != y)) rp_fail(p, "tick %ld slot%ld scene expected %ld:%ld got %d:%d", k, n, x, y, p->view.slot_scene[i], p->view.slot_scenes[i]);
    return 0;
}

static int rp_expect_pending(rp *p, char **argv, int argc)
{
    long k, n; int w; uint32_t got;
    if (argc != 4 || rp_tick_arg(p, argv[1], &k, 1) || (w = lj_word_index(rp_pending_words, argv[2])) < 0 || rp_num(argv[3], 0, LJ_NUMBER_MAX, &n))
        return rp_fail(p, "expect_pending: expected `expect_pending <tick> link_cue|bg|genmacro <n>`");
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    got = w == 0 ? p->view.pending_link_cue : w == 1 ? p->view.pending_bg : p->view.pending_genmacro;
    if (got != (uint32_t)n) rp_fail(p, "tick %ld pending_%s expected %ld got %u", k, rp_pending_words[w], n, got);
    return 0;
}

static int rp_expect_view(rp *p, char **argv, int argc)
{
    long k, v; int i; char key[32]; const char *val;
    if (argc < 3 || rp_tick_arg(p, argv[1], &k, 1)) return argc < 3 ? rp_fail(p, "expect_view: expected `expect_view <tick> gm=<v> [blackout= freeze= offline= chase_ms=]`") : -1;
    for (i = 2; i < argc; i++) { KV_NEXT(i); if (rp_num(val, 0, 1000000, &v)) return rp_bad(p, key, val); }
    if (rp_run_to(p, k)) return -1;
    p->r->expects++;
    for (i = 2; i < argc; i++) {
        long got;
        rp_key(argv[i], key, sizeof key, &val); rp_num(val, 0, 1000000, &v);
        if (!strcmp(key, "gm")) got = p->view.gm;
        else if (!strcmp(key, "blackout")) got = p->view.blackout;
        else if (!strcmp(key, "freeze")) got = p->view.freeze;
        else if (!strcmp(key, "offline")) got = p->view.offline;
        else if (!strcmp(key, "chase_ms")) got = p->view.chase_ms;
        /* W2 fields */
        else if (!strcmp(key, "bg_any")) got = p->view.bg_any;
        else if (!strcmp(key, "statics_any")) got = p->view.statics_any;
        else if (!strcmp(key, "flash_latched")) got = p->view.flash_latched;
        else if (!strcmp(key, "flash_seq")) got = p->view.flash_seq;
        else if (!strcmp(key, "transparent")) got = p->view.transparent_mode;
        else if (!strcmp(key, "smoke")) got = p->view.smoke_active;
        else if (!strcmp(key, "haze")) got = p->view.haze_level;
        else if (!strcmp(key, "chase_tap_us")) got = (long)p->view.chase_tap_us;
        else if (!strcmp(key, "cue")) got = (long)p->view.cue_number;
        else if (!strcmp(key, "bg")) got = (long)p->view.bg_number;
        else if (!strcmp(key, "snapshot_tick")) got = (long)p->view.snapshot_tick;
        else if (!strcmp(key, "gm_engine")) got = p->view.gm_engine_on;          /* W3 genmacro */
        else if (!strcmp(key, "gm_slot")) got = p->view.gm_slot_on;
        else if (!strcmp(key, "gm_cue")) got = (long)p->view.gm_cue_number;
        else if (!strcmp(key, "gm_editor")) got = (long)p->view.gm_editor_number;
        else if (!strcmp(key, "mm_amp")) got = p->view.mm_amp_pct;   /* W3 movement: the cue macro control in force */
        else if (!strcmp(key, "mm_speed")) got = p->view.mm_speed_pct;
        else if (!strcmp(key, "mm_fade")) got = p->view.mm_fade;
        else if (!strcmp(key, "mm_frozen")) got = p->view.mm_frozen;
        else return rp_fail(p, "expect_view: unknown field %s", key);
        if (got != v) rp_fail(p, "tick %ld view %s expected %ld got %ld", k, key, v, got);
    }
    return 0;
}

static int rp_perf(rp *p, char **argv, int argc)
{
    long v, ticks = 100, max_us = -1; int i; char key[32]; const char *val; uint32_t before = p->r->max_tick_us;
    for (i = 1; i < argc; i++) {
        KV_NEXT(i);
        KV_NUM("ticks", 1, 100000, ticks, long);
        KV_NUM("max_tick_us", 1, 1000000, max_us, long);
        if (!strcmp(key, "fixtures") || !strcmp(key, "slots") || !strcmp(key, "bg") || !strcmp(key, "statics") || !strcmp(key, "genmacro")) continue;   /* the load is the scenario's own build lines */
        return rp_fail(p, "perf: unknown key %s", key);
    }
    if (max_us < 0) return rp_fail(p, "perf: max_tick_us= is required");
    p->r->max_tick_us = 0;
    if (rp_run_to(p, p->tick + ticks - 1)) return -1;
    p->r->expects++;
    if (RP_SANITIZED) printf("perf %s: %u us max over %ld ticks (sanitizer build, not gated)\n", p->name, p->r->max_tick_us, ticks);
    else if (p->r->max_tick_us > (uint32_t)max_us) rp_fail(p, "perf: longest tick %u us over %ld ticks exceeds %ld", p->r->max_tick_us, ticks, max_us);
    if (before > p->r->max_tick_us) p->r->max_tick_us = before;
    return 0;
}

/* ---- the line dispatcher ---- */
static int rp_line(rp *p, char **argv, int argc)
{
    const char *w = argv[0];
    long v;
    /* event lines */
    if (!strcmp(w, "at") || !strncmp(w, "expect", 6) || !strcmp(w, "run") || !strcmp(w, "perf")) {
        if (!p->built && rp_finish(p)) return -1;
        if (!strcmp(w, "at")) return rp_at(p, argv, argc);
        if (!strcmp(w, "expect")) return rp_expect(p, argv, argc, 0);
        if (!strcmp(w, "expect_merged")) return rp_expect(p, argv, argc, 1);
        if (!strcmp(w, "expect_pair")) return rp_expect_pair(p, argv, argc);
        if (!strcmp(w, "expect_range")) return rp_expect_range(p, argv, argc);
        if (!strcmp(w, "expect_status")) return rp_expect_status(p, argv, argc);
        if (!strcmp(w, "expect_pending")) return rp_expect_pending(p, argv, argc);
        if (!strcmp(w, "expect_mm")) return rp_expect_mm(p, argv, argc);   /* W3 movement */
        if (!strcmp(w, "expect_view")) return rp_expect_view(p, argv, argc);
        if (!strcmp(w, "expect_cl")) return rp_expect_cl(p, argv, argc);   /* W3 cuelist-core */
        if (!strcmp(w, "run")) { long k; if (argc != 2 || rp_tick_arg(p, argv[1], &k, 1)) return argc != 2 ? rp_fail(p, "run: expected `run <tick>`") : -1; return rp_run_to(p, k); }
        if (!strcmp(w, "perf")) return rp_perf(p, argv, argc);
        return rp_fail(p, "unknown line %s", w);
    }
    /* build lines */
    if (rp_need_build(p, w)) return -1;
    if (!strncmp(w, "tick_us=", 8)) { if (argc != 1 || rp_num(w + 8, 1, 1000000, &v)) return rp_bad(p, "tick_us", w + 8); p->tick_us = (uint32_t)v; return 0; }
    if (!strcmp(w, "seed")) {                       /* seed <n>: the LJ_RNG_SEED the scenario's random expects were computed with */
        char *e; unsigned long sd;
        if (argc != 2) return rp_fail(p, "seed: expected `seed <n>`");
        sd = strtoul(argv[1], &e, 0);
        if (e == argv[1] || *e || argv[1][0] == '-') return rp_bad(p, "seed", argv[1]);
        return sd == LJ_RNG_SEED ? 0 : rp_fail(p, "seed 0x%08lX: the engine's LJ_RNG_SEED is 0x%08lX", sd, (unsigned long)LJ_RNG_SEED);
    }
    if (!strcmp(w, "follow") || !strcmp(w, "gdcgroup")) return rp_gfs_build(p, argv, argc);   /* W3 gdc-fades-followspot */
    if (!strcmp(w, "cl")) return rp_cl(p, argv, argc);           /* W3 cuelist-core */
    if (!strcmp(w, "clline")) return rp_clline(p, argv, argc);
    if (!strcmp(w, "bez") || !strcmp(w, "node")) return rp_mm_build(p, argv, argc);   /* W3 movement */
    if (!strcmp(w, "patch")) return rp_patch(p, argv, argc);
    if (!strcmp(w, "profile")) return rp_profile(p, argv, argc);
    if (!strcmp(w, "default")) return rp_default(p, argv, argc);
    if (!strcmp(w, "gpatch")) return rp_gpatch(p, argv, argc);
    if (!strcmp(w, "submaster")) return rp_submaster(p, argv, argc);
    if (!strcmp(w, "seq")) return rp_seq(p, argv, argc);
    if (!strcmp(w, "scene")) return rp_scene(p, argv, argc);
    if (!strcmp(w, "ch")) return rp_ch(p, argv, argc);
    if (!strcmp(w, "fx")) return rp_fx(p, argv, argc);
    if (!strcmp(w, "preset")) return rp_preset(p, argv, argc);
    if (!strcmp(w, "cue")) return rp_cue(p, argv, argc);
    if (!strcmp(w, "slot")) return rp_slot(p, argv, argc);
    if (!strcmp(w, "bg")) return rp_bg(p, argv, argc);
    if (!strcmp(w, "bgslot")) return rp_bgslot(p, argv, argc);
    if (!strcmp(w, "static")) return rp_static(p, argv, argc);
    if (!strcmp(w, "macro")) return rp_macro(p, argv, argc);
    if (!strcmp(w, "mch")) return rp_mch(p, argv, argc);
    if (!strcmp(w, "gm_shapes")) return rp_gm_shapes(p, argv, argc);   /* W3 genmacro (ME-34) */
    if (!strcmp(w, "htp")) return rp_htp(p, argv, argc);
    return rp_fail(p, "unknown line %s", w);
}

static void rp_basename(const char *path, char *dst, size_t cap)
{
    const char *s = path, *e;
    for (e = path; *e; e++) if (*e == '/' || *e == '\\') s = e + 1;
    e = strrchr(s, '.');
    if (!e || e == s) e = s + strlen(s);
    if ((size_t)(e - s) >= cap) e = s + cap - 1;
    memcpy(dst, s, (size_t)(e - s)); dst[e - s] = 0;
}

int lj_replay_run(const char *path, FILE *frames_out, lj_replay_result *out)
{
    static lj_replay_result dummy;
    rp *p = (rp *)calloc(1, sizeof *p);
    FILE *f;
    char *argv[RP_TOK_MAX];
    int i, rc = 0;
    if (!out) out = &dummy;
    memset(out, 0, sizeof *out);
    if (!p) { snprintf(out->first_fail, sizeof out->first_fail, "out of memory"); return -1; }
    p->path = path ? path : "?"; p->r = out; p->frames = frames_out; p->tick_us = LJ_TICK_US_DEFAULT;
    rp_basename(p->path, p->name, sizeof p->name);
    f = fopen(p->path, "rb");
    if (!f) { rp_fail(p, "cannot open"); free(p); return -1; }
    while (fgets(p->line_buf, sizeof p->line_buf, f)) {        /* ME-190: a scenario that drives DMX-in anywhere has the input enabled (lj_io_dmxin_apply) */
        char w[16];
        if (sscanf(p->line_buf, " at %*s %15s", w) == 1 && strcmp(w, "dmx_in") == 0) { p->dmx_in_live = 1; break; }
    }
    rewind(f);
    p->show = lj_show_new();
    memset(&rp_mm, 0, sizeof rp_mm);   /* W3 movement: the Bezier scratch, per scenario (show->bez points into it) */
    memset(&rp_clx, 0, sizeof rp_clx); lj_cl_run_init(&rp_clx.run);   /* W3 cuelist-core */
    memset(&rp_gfs, 0, sizeof rp_gfs);   /* W3 gdc-fades-followspot: the harness's one lj_gdc_state and follow-spot scratch, per scenario */
    for (;;) {
        size_t n;
        int argc;
        if (!fgets(p->line_buf, sizeof p->line_buf, f)) break;
        p->line++;
        n = strlen(p->line_buf);
        if (n && p->line_buf[n - 1] != '\n' && !feof(f)) { rc = rp_fail(p, "line longer than %d bytes", RP_LINE_MAX); break; }
        while (n && (p->line_buf[n - 1] == '\n' || p->line_buf[n - 1] == '\r')) p->line_buf[--n] = 0;
        if (p->line == 1) {
            if (strcmp(p->line_buf, "LJ4 replay 1")) { rc = rp_fail(p, "first line must be \"LJ4 replay 1\""); break; }
            continue;
        }
        argc = rp_tokens(p->line_buf, argv, RP_TOK_MAX);
        if (argc < 0) { rc = rp_fail(p, argc == -1 ? "more than %d tokens" : "unterminated quote", RP_TOK_MAX); break; }
        if (argc == 0) continue;
        if (rp_line(p, argv, argc)) { rc = -1; break; }
    }
    if (p->line == 0 && rc == 0) rc = rp_fail(p, "empty file");
    fclose(f);
    printf("replay %s: %d ticks, %d expects, %d fails\n", p->name, out->ticks, out->expects, out->fails);
    lj_engine_destroy(p->e);
    lj_show_free(p->show);
    lj_gm_shapes_free(&rp_gm.sh); rp_gm.have = 0;              /* W3 genmacro: the scenario's override table */
    for (i = 0; i < LJ_FIXTURES; i++) free(p->prof[i]);
    free(p);
    return (rc || out->fails) ? -1 : 0;
}
