/* lj_show.c - the show (lj_show.h): allocation, Config.dat, DefaultOut.dat, the
 * whole-library load/save of the W1 windows, fixture helpers, record freeing. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_show.h"
#include "model/lj_patch.h"
#include "model/lj_sequence.h"
#include "model/lj_cue.h"
#include "model/lj_cuelist.h"
#include "model/lj_bgcue.h"
#include "model/lj_statics.h"
#include "model/lj_groups.h"
#include "model/lj_presets.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

lj_show *lj_show_new(void)
{
    lj_show *s = (lj_show *)calloc(1, sizeof *s);
    if (!s) lj_fatal("lj_show_new: out of memory (%u bytes)", (unsigned)sizeof *s);
    lj_patch_init(s->patch);
    return s;
}

static void lj_free_seq(void *p) { lj_sequence_free((lj_sequence *)p); }
static void lj_free_cue(void *p) { lj_cue_free((lj_cue *)p); }
static void lj_free_cl(void *p) { lj_cuelist_free((lj_cuelist *)p); }
static void lj_free_bg(void *p) { lj_bgcue_free((lj_bgcue *)p); }

void lj_show_free(lj_show *s)
{
    if (!s) return;
    /* a loaded record is both the table entry and the current one (lj_app_load_cue / _seq): free it once */
    if (s->current_seq && lj_table_get(&s->sequences, s->current_seq->number) == s->current_seq) s->current_seq = NULL;
    if (s->current_cue && lj_table_get(&s->cues, s->current_cue->number) == s->current_cue) s->current_cue = NULL;
    if (s->current_cl && lj_table_get(&s->cuelists, s->current_cl->number) == s->current_cl) s->current_cl = NULL;
    lj_table_free(&s->sequences, lj_free_seq);
    lj_table_free(&s->cues, lj_free_cue);
    lj_table_free(&s->cuelists, lj_free_cl);
    lj_table_free(&s->bgcues, lj_free_bg);
    lj_table_free(&s->macros, free);
    lj_statics_free(s->pages, s->npages);
    lj_sequence_free(s->current_seq);
    lj_cue_free(s->current_cue);
    lj_cuelist_free(s->current_cl);
    free(s);
}

int lj_show_free_record(lj_show *show, enum lj_rec_kind kind, uint32_t number)
{
    lj_table *t = kind == LJ_REC_SEQ ? &show->sequences : kind == LJ_REC_CUE ? &show->cues : kind == LJ_REC_CL ? &show->cuelists
                : kind == LJ_REC_BG ? &show->bgcues : kind == LJ_REC_MACRO ? &show->macros : NULL;
    void *rec = t ? lj_table_remove(t, number) : NULL;
    if (!rec) return -1;
    switch (kind) {
    case LJ_REC_SEQ: lj_free_seq(rec); break;
    case LJ_REC_CUE: lj_free_cue(rec); break;
    case LJ_REC_CL:  lj_free_cl(rec); break;
    case LJ_REC_BG:  lj_free_bg(rec); break;
    default:         free(rec); break;
    }
    return 0;
}

/* ---- Config.dat ---- */

int lj_config_load(const char *path, lj_show *show, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "config", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 config file"); return -1; }
    memset(show->fixture, 0, sizeof show->fixture);
    memset(show->submaster_name, 0, sizeof show->submaster_name);
    memset(show->submaster_mode, 0, sizeof show->submaster_mode);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "fx") == 0) {
            long i = strtol(v, &e, 10);
            lj_fixture *f;
            if (e == v || i < 1 || i > LJ_FIXTURES) { bad = "fixture index not 1..100"; break; }
            f = &show->fixture[i - 1];
            f->kind = (uint8_t)lj_kv_word(v, "kind", lj_fixture_kind_words, LJ_FX_BUILTIN);
            f->profile_id = (uint16_t)lj_kv_range(v, "id", 0, 0, 65535, &rbad);
            if (lj_kv_get(v, "gs2", q, sizeof q) == 1) lj_str_copy(f->gs2_file, sizeof f->gs2_file, q);
            f->link = (uint8_t)lj_kv_range(v, "link", 0, 0, LJ_LINKS, &rbad);              /* ME-77: link=257 fails, never aliases to 1 */
            f->address = (uint16_t)lj_kv_range(v, "addr", 0, 0, LJ_LINK_CH, &rbad);       /* ME-77: addr=65537 fails */
            f->invert_pan = (uint8_t)(lj_kv_long(v, "ipan", 0) != 0);
            f->invert_tilt = (uint8_t)(lj_kv_long(v, "itilt", 0) != 0);
            f->swap_pt = (uint8_t)(lj_kv_long(v, "swap", 0) != 0);
            f->ignore_bo = (uint8_t)(lj_kv_long(v, "ibo", 0) != 0);
            f->ignore_gm = (uint8_t)(lj_kv_long(v, "igm", 0) != 0);
            f->link_strobe_gm = (uint8_t)(lj_kv_long(v, "lsgm", 0) != 0);
            if (lj_kv_get(v, "user", q, sizeof q) == 1) lj_str_copy(f->user_name, sizeof f->user_name, q);
            f->placed = (uint8_t)(lj_kv_long(v, "placed", 0) != 0);
            f->icon_x = (int16_t)lj_kv_range(v, "x", 0, -32768, 32767, &rbad);
            f->icon_y = (int16_t)lj_kv_range(v, "y", 0, -32768, 32767, &rbad);
            f->tab = (uint8_t)lj_kv_range(v, "tab", 0, 0, 255, &rbad);
            f->intensity_group = (uint8_t)lj_kv_range(v, "sub", 0, 0, LJ_SUBMASTERS, &rbad);
        } else if (strcmp(k, "sub") == 0) {
            long i = strtol(v, &e, 10);
            if (e == v || i < 1 || i > LJ_SUBMASTERS) { bad = "submaster index not 1..8"; break; }
            if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(show->submaster_name[i - 1], sizeof show->submaster_name[i - 1], q);
            show->submaster_mode[i - 1] = (uint8_t)lj_kv_range(v, "mode", 0, 0, 255, &rbad);
        }
        if (rbad) { bad = "number out of range"; break; }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(show->fixture, 0, sizeof show->fixture); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_config_save(const char *path, const lj_show *show)
{
    char u[72], g[2 * 64 + 2];
    int i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "config", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &show->fixture[i];
        if (!f->link) continue;
        lj_kv_quote(u, sizeof u, f->user_name);
        lj_kv_quote(g, sizeof g, f->gs2_file);
        ok = lj_rec_write(rec, "fx ", "%d kind=%s id=%u gs2=%s link=%d addr=%u ipan=%d itilt=%d swap=%d ibo=%d igm=%d lsgm=%d user=%s placed=%d x=%d y=%d tab=%d sub=%d",
                          i + 1, lj_word(lj_fixture_kind_words, f->kind), f->profile_id, g, f->link, f->address, f->invert_pan, f->invert_tilt, f->swap_pt,
                          f->ignore_bo, f->ignore_gm, f->link_strobe_gm, u, f->placed, f->icon_x, f->icon_y, f->tab, f->intensity_group) == 0;
    }
    for (i = 0; ok && i < LJ_SUBMASTERS; i++) {
        if (!show->submaster_name[i][0] && !show->submaster_mode[i]) continue;
        lj_kv_quote(u, sizeof u, show->submaster_name[i]);
        ok = lj_rec_write(rec, "sub ", "%d name=%s mode=%d", i + 1, u, show->submaster_mode[i]) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

static int lj_fixture_footprint(const lj_fixture *f)
{
    const lj_profile *p = f->kind == LJ_FX_BUILTIN ? lj_fixlib_by_id(f->profile_id) : NULL;
    return p ? p->footprint : 1;   /* .GS2 profiles are parsed in W3 (lj_gs2_parse stub): footprint 1 until then */
}

const char *lj_config_validate(const lj_show *show)
{
    static uint8_t owner[LJ_CHANS];   /* UI-thread only (library calls are UI-thread file I/O) */
    int i, c;
    memset(owner, 0, sizeof owner);
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &show->fixture[i];
        int base, fp;
        if (!f->link) { if (f->kind || f->profile_id || f->gs2_file[0]) return "unused row carries a profile"; continue; }
        if (f->link > LJ_LINKS) return "link not 1..4";
        if (f->address < 1 || f->address > LJ_LINK_CH) return "address not 1..512";
        if (f->kind > LJ_FX_GS2) return "fixture kind unknown";
        if (!memchr(f->user_name, 0, sizeof f->user_name) || !memchr(f->gs2_file, 0, sizeof f->gs2_file)) return "user name or gs2 file not NUL-terminated";
        if (f->kind == LJ_FX_BUILTIN ? lj_fixlib_by_id(f->profile_id) == NULL : !f->gs2_file[0]) return "profile unknown";
        if (f->intensity_group > LJ_SUBMASTERS) return "intensity group not 0..8";
        fp = lj_fixture_footprint(f);
        if (f->address + fp - 1 > LJ_LINK_CH) return "fixture runs past channel 512";
        base = lj_addr_code(f->link, f->address);
        for (c = 0; c < fp; c++) {
            if (owner[base + c]) return "two fixtures share a channel";
            owner[base + c] = 1;
        }
    }
    for (i = 0; i < LJ_SUBMASTERS; i++) if (!memchr(show->submaster_name[i], 0, sizeof show->submaster_name[i])) return "submaster name not NUL-terminated";
    return NULL;
}

/* ---- DefaultOut.dat ---- */

int lj_defaultout_load(const char *path, uint8_t out[LJ_CHANS], char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP];
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "defaultout", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 defaultout file"); return -1; }
    memset(out, 0, LJ_CHANS);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "ch") == 0 && lj_kv_ch_parse(v, out, NULL) != 0) { bad = "bad channel line"; break; }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(out, 0, LJ_CHANS); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_defaultout_save(const char *path, const uint8_t out[LJ_CHANS])
{
    lj_rec *rec = lj_rec_open_write(path, "defaultout", 1);
    int ok;
    if (!rec) return -1;
    ok = lj_kv_ch_write(rec, out, NULL) == 0;
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

/* ---- the library as a whole ---- */

static int lj_file_exists(const char *path) { FILE *f = fopen(path, "rb"); if (!f) return 0; fclose(f); return 1; }

int lj_show_load(lj_show *show, const lj_library *lib)
{
    char path[LJ_PATH_CAP], rel[64], err[256];
    int i;
    const char *bad;
    lj_library_path(lib, "Config.dat", path, sizeof path);
    memset(show->fixture, 0, sizeof show->fixture);
    if (lj_file_exists(path)) {
        if (lj_config_load(path, show, err, sizeof err) != 0) return -1;
        if ((bad = lj_config_validate(show)) != NULL) { lj_log("%s: %s", path, bad); memset(show->fixture, 0, sizeof show->fixture); return -1; }
    }
    lj_library_path(lib, "DefaultOut.dat", path, sizeof path);
    memset(show->default_out, 0, sizeof show->default_out);
    if (lj_file_exists(path)) lj_defaultout_load(path, show->default_out, err, sizeof err);
    lj_library_path(lib, LJ_PATCH_FILE, path, sizeof path);   /* W2: the live global patch (PROPOSAL rig file name) */
    lj_patch_init(show->patch);
    if (lj_file_exists(path)) lj_patch_load(path, show->patch, err, sizeof err);
    lj_library_path(lib, "GroupInfo.dat", path, sizeof path);
    memset(show->user_group, 0, sizeof show->user_group);
    if (lj_file_exists(path)) lj_groups_load(path, show->user_group, err, sizeof err);
    lj_library_path(lib, "Statics.dat", path, sizeof path);
    lj_statics_free(show->pages, show->npages);
    show->pages = NULL; show->npages = 0;
    if (lj_file_exists(path)) lj_statics_load(path, &show->pages, &show->npages, err, sizeof err);
    memset(show->preset, 0, sizeof show->preset);
    for (i = 0; i < LJ_FIXTURES; i++) {
        if (!show->fixture[i].link) continue;
        snprintf(rel, sizeof rel, "Presets\\fixture%03d.pre", i + 1);
        lj_library_path(lib, rel, path, sizeof path);
        if (lj_file_exists(path)) lj_presets_load(path, show->preset[i], err, sizeof err);
    }
    return 0;
}

int lj_show_save_config(const lj_show *show, const lj_library *lib)
{
    char path[LJ_PATH_CAP];
    lj_htp_names htp;
    int ok;
    lj_library_path(lib, "Config.dat", path, sizeof path);
    ok = lj_config_save(path, show) == 0;
    lj_library_path(lib, "DefaultOut.dat", path, sizeof path);
    ok = lj_defaultout_save(path, show->default_out) == 0 && ok;
    lj_library_path(lib, "GroupInfo.dat", path, sizeof path);
    lj_htp_names_load(path, &htp, NULL, 0);                  /* W2: keep the htp lines the HTP Groups window wrote (defaults when absent) */
    ok = lj_htp_names_save(path, show->user_group, &htp) == 0 && ok;
    return ok ? 0 : -1;
}

/* ---- fixtures ---- */

int lj_show_fixture_count(const lj_show *show)
{
    int i, n = 0;
    for (i = 0; i < LJ_FIXTURES; i++) if (show->fixture[i].link) n++;
    return n;
}

int lj_show_fixture_base(const lj_show *show, int i)
{
    if (i < 0 || i >= LJ_FIXTURES || !show->fixture[i].link) return -1;
    return lj_addr_code(show->fixture[i].link, show->fixture[i].address);
}

int lj_show_fixture_at(const lj_show *show, int addr_code)
{
    int i;
    for (i = 0; i < LJ_FIXTURES; i++) {
        int base = lj_show_fixture_base(show, i);
        if (base >= 0 && addr_code >= base && addr_code < base + lj_fixture_footprint(&show->fixture[i])) return i;
    }
    return -1;
}

int lj_show_add_fixture(lj_show *show, const lj_fixture *fx)
{
    int i;
    if (!fx->link) return -1;
    for (i = 0; i < LJ_FIXTURES; i++)
        if (!show->fixture[i].link) { show->fixture[i] = *fx; return i; }
    return -1;
}

void lj_show_remove_fixture(lj_show *show, int i) { if (i >= 0 && i < LJ_FIXTURES) memset(&show->fixture[i], 0, sizeof show->fixture[i]); }
