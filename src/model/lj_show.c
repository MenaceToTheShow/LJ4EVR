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
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "fixlib/lj_gs2.h"

#define LJ_GS2_LOAD_MAX 1024   /* user .GS2 profiles read per library (PROPOSAL bound) */

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
    lj_show_gs2_free(s->gs2, s->ngs2);   /* W3: show->bez belongs to the app (it builds and frees the sampled tables) */
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
    lj_fixture *cur = NULL;                                   /* W3 (ME-32): the fx record the chcap / chgm lines belong to */
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
            f = cur = &show->fixture[i - 1];
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
        } else if (strcmp(k, "chcap") == 0) {                 /* W3 (ME-32): "chcap 3 text=Inte": offset 1..32, caption <= 6 bytes (DFM MaxLength = 6, ME-118) */
            int lbad = 0;                                       /* G1: a bad chcap line is logged and skipped, never the whole patch */
            long n = lj_kv_range_lead(v, 0, 1, LJ_PROFILE_CH, &lbad);
            int got = cur && !lbad ? lj_kv_get(v, "text", q, sizeof cur->ch_caption[0]) : -1;
            if (got < 0) lj_log("%s: item %d: chcap skipped (%s)", path, item, !cur ? "before fx" : lbad ? "offset out of range" : "text longer than 6");
            else lj_str_copy(cur->ch_caption[n - 1], sizeof cur->ch_caption[0], got == 1 ? q : "");
        } else if (strcmp(k, "chgm") == 0) {                  /* W3 (ME-32): "chgm=1,3": the offsets 1..32 with Grand Master control disabled */
            uint32_t o[256];
            int n = lj_kv_list(v, o, 256), j;
            if (!cur || n < 0) { lj_log("%s: item %d: chgm skipped (%s)", path, item, !cur ? "before fx" : "bad list"); continue; }   /* G1 */
            cur->ch_gm_off = 0;
            for (j = 0; j < n; j++) {
                if (o[j] >= 1 && o[j] <= LJ_PROFILE_CH) cur->ch_gm_off |= 1u << (o[j] - 1);
                else lj_log("%s: item %d: chgm offset %u skipped (out of range)", path, item, (unsigned)o[j]);   /* G1: the in-range entries are kept */
            }
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

static int lj_config_save_channels(lj_rec *rec, const lj_fixture *f)   /* W3 (ME-32): the chcap / chgm lines after their fx line */
{
    char q[16], list[LJ_PROFILE_CH * 3 + 1];
    int k, n = 0;
    for (k = 0; k < LJ_PROFILE_CH; k++) {
        if (!f->ch_caption[k][0]) continue;
        lj_kv_quote(q, sizeof q, f->ch_caption[k]);
        if (lj_rec_write(rec, "chcap ", "%d text=%s", k + 1, q) != 0) return -1;
    }
    if (!f->ch_gm_off) return 0;
    for (k = 0; k < LJ_PROFILE_CH; k++) if (f->ch_gm_off >> k & 1u) n += snprintf(list + n, sizeof list - (size_t)n, n ? ",%d" : "%d", k + 1);
    return lj_rec_write(rec, "chgm", "%s", list);
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
        ok = ok && lj_config_save_channels(rec, f) == 0;     /* W3 (ME-32) */
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

/* warn_ok: the F41 save-time warning states (no DMX address, past channel 512, a shared channel; G1: a built-in id this
 * build lacks) pass, because TConfigWarnForm's Save Anyway writes them as LJ2 does; every other check still refuses the file. */
static const char *config_check(const lj_show *show, int warn_ok)
{
    static uint8_t owner[LJ_CHANS];   /* UI-thread only (library calls are UI-thread file I/O) */
    int i, c;
    memset(owner, 0, sizeof owner);
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_fixture *f = &show->fixture[i];
        int base, fp;
        if (!f->link) { if (f->kind || f->profile_id || f->gs2_file[0]) return "unused row carries a profile"; continue; }
        if (f->link > LJ_LINKS) return "link not 1..4";
        if (f->address > LJ_LINK_CH) return "address not 1..512";
        if (f->address < 1) { if (warn_ok) continue; return "fixture has no DMX address"; }
        if (f->kind > LJ_FX_GS2) return "fixture kind unknown";
        if (!memchr(f->user_name, 0, sizeof f->user_name) || !memchr(f->gs2_file, 0, sizeof f->gs2_file)) return "user name or gs2 file not NUL-terminated";
        if (f->kind == LJ_FX_GS2 && !f->gs2_file[0]) return "profile unknown";
        if (f->intensity_group > LJ_SUBMASTERS) return "intensity group not 0..8";
        if (f->kind == LJ_FX_BUILTIN && !lj_fixlib_by_id(f->profile_id)) { if (warn_ok) continue; return "profile " LJ_NA_HINT; }   /* G1 (W8D-11): an id this build lacks is a fourth F41 state, kept unpatched and saved back as read */
        fp = lj_fixture_footprint(f);
        if (f->address + fp - 1 > LJ_LINK_CH) { if (!warn_ok) return "fixture runs past channel 512"; fp = LJ_LINK_CH - f->address + 1; }
        base = lj_addr_code(f->link, f->address);
        for (c = 0; c < fp; c++) {
            if (owner[base + c] && !warn_ok) return "two fixtures share a channel";
            owner[base + c] = 1;
        }
    }
    for (i = 0; i < LJ_SUBMASTERS; i++) if (!memchr(show->submaster_name[i], 0, sizeof show->submaster_name[i])) return "submaster name not NUL-terminated";
    return NULL;
}

const char *lj_config_validate(const lj_show *show) { return config_check(show, 0); }

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


int lj_show_load(lj_show *show, const lj_library *lib)
{
    char path[LJ_PATH_CAP], rel[64], err[256];
    int i;
    const char *bad;
    lj_library_path(lib, "Config.dat", path, sizeof path);
    memset(show->fixture, 0, sizeof show->fixture);
    if (lj_fs_exists(path)) {
        if (lj_config_load(path, show, err, sizeof err) != 0) return -1;
        if ((bad = config_check(show, 1)) != NULL) { lj_log("%s: %s", path, bad); memset(show->fixture, 0, sizeof show->fixture); return -1; }
        if ((bad = config_check(show, 0)) != NULL) lj_log("%s: %s (F41 warning, saved with Save Anyway; kept)", path, bad);
    }
    lj_library_path(lib, "DefaultOut.dat", path, sizeof path);
    memset(show->default_out, 0, sizeof show->default_out);
    if (lj_fs_exists(path)) lj_defaultout_load(path, show->default_out, err, sizeof err);
    lj_library_path(lib, LJ_PATCH_FILE, path, sizeof path);   /* W2: the live global patch (PROPOSAL rig file name) */
    lj_patch_init(show->patch);
    if (lj_fs_exists(path)) lj_patch_load(path, show->patch, err, sizeof err);
    lj_library_path(lib, "GroupInfo.dat", path, sizeof path);
    memset(show->user_group, 0, sizeof show->user_group);
    if (lj_fs_exists(path)) lj_groups_load(path, show->user_group, err, sizeof err);
    lj_library_path(lib, "Statics.dat", path, sizeof path);
    lj_statics_free(show->pages, show->npages);
    show->pages = NULL; show->npages = 0;
    if (lj_fs_exists(path)) lj_statics_load(path, &show->pages, &show->npages, err, sizeof err);
    memset(show->preset, 0, sizeof show->preset);
    for (i = 0; i < LJ_FIXTURES; i++) {
        if (!show->fixture[i].link) continue;
        snprintf(rel, sizeof rel, "Presets\\fixture%03d.pre", i + 1);
        lj_library_path(lib, rel, path, sizeof path);
        if (lj_fs_exists(path)) lj_presets_load(path, show->preset[i], err, sizeof err);
        snprintf(rel, sizeof rel, LJ_CUSTOM_PAL_FMT, i + 1);   /* W3: Customize Fixture overrides (design 6.5); a bad file logs and stays default */
        lj_library_path(lib, rel, path, sizeof path);
        if (lj_fs_exists(path)) lj_fixture_custom_load(path, show->fixture[i].custom, err, sizeof err);
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
    memset(&htp, 0, sizeof htp);                             /* an empty name saves as the default */
    if (lj_fs_exists(path)) lj_htp_names_load(path, &htp, NULL, 0);   /* W2: keep the htp lines the HTP Groups window wrote; INT-s9-GROUPINFO: a fresh library has no file and logs nothing */
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
    if (i < 0 || i >= LJ_FIXTURES || !show->fixture[i].link || show->fixture[i].address < 1) return -1;   /* no DMX address yet (F41): unpatched */
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

/* ---- W3 contracts (lj_show.h): profile lookup complete; the loaders are stubs for the gs2 / profile-editor tracks ---- */
static int lj_ieq(const char *a, const char *b)   /* ASCII case fold: file names on Windows / macOS default volumes */
{
    for (;; a++, b++) {
        int x = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a, y = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (x != y) return 0;
        if (!x) return 1;
    }
}
const lj_profile *lj_show_gs2(const lj_show *show, const char *gs2_file)
{
    int i;
    if (!show || !gs2_file || !gs2_file[0]) return NULL;
    for (i = 0; i < show->ngs2; i++)
        if (lj_ieq(show->gs2[i].gs2_file, gs2_file)) return &show->gs2[i];
    return NULL;
}
const lj_profile *lj_show_profile(const lj_show *show, int fixture)
{
    const lj_fixture *f;
    if (!show || fixture < 0 || fixture >= LJ_FIXTURES) return NULL;
    f = &show->fixture[fixture];
    if (!f->link) return NULL;
    return f->kind == LJ_FX_GS2 ? lj_show_gs2(show, f->gs2_file) : lj_fixlib_by_id(f->profile_id);
}
int lj_show_gs2_load_all(const lj_library *lib, lj_profile **out, int *n)
{
    static char names[LJ_GS2_LOAD_MAX][64];   /* UI thread only (the app's reload) */
    char rel[96], path[LJ_PATH_CAP], err[128];
    int i, k = lj_library_list_files(lib, "UserFixtureDef", ".GS2", names, LJ_GS2_LOAD_MAX);
    *out = NULL; *n = 0;
    if (k <= 0) return 0;                                       /* no folder or no profile: 0 profiles */
    if (!(*out = (lj_profile *)calloc((size_t)k, sizeof **out))) return -1;
    for (i = 0; i < k; i++) {
        if (snprintf(rel, sizeof rel, "UserFixtureDef/%s", names[i]) >= (int)sizeof rel || lj_library_path(lib, rel, path, sizeof path) != 0) continue;
        if (lj_gs2_load(path, &(*out)[*n], err, sizeof err) != 0) { lj_log("%s: %s, skipped", rel, err); memset(&(*out)[*n], 0, sizeof **out); continue; }
        (*n)++;
    }
    return 0;
}
void lj_show_gs2_free(lj_profile *p, int n)
{
    int i;
    if (!p) return;
    for (i = 0; i < n; i++) lj_gs2_free(&p[i]);
    free(p);
}
/* "LJ4 custompal 1" then one "pal p entry=e rgb=r bmp=name" line per overridden entry (p 1..6, e 1..20; rgb 0xRRGGBB
 * or -1 = no colour, LJ_CUSTOM_NO_RGB); an entry without a line is not overridden (custom used = 0). */
int lj_fixture_custom_load(const char *path, lj_palette custom[6], char *err, size_t cap)
{
    char k[16], v[512], q[64];
    int fmt, r, item = 0, rbad = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "custompal", &fmt);
    memset(custom, 0, sizeof(lj_palette) * LJ_PALETTES);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 custompal file"); return -1; }
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        long p, e, rgb;
        int got;
        item++;
        if (strcmp(k, "pal") != 0) continue;                  /* unknown keys skipped (design 5.2) */
        p = lj_kv_range_lead(v, 1, 1, LJ_PALETTES, &rbad);
        e = lj_kv_range(v, "entry", 1, 1, LJ_PAL_ENTRIES, &rbad);
        rgb = lj_kv_range(v, "rgb", -1, -1, 0xFFFFFF, &rbad);
        got = lj_kv_get(v, "bmp", q, sizeof custom[0].e[0].bitmap);
        if (rbad) { bad = "number out of range"; break; }
        if (got < 0) { bad = "bmp longer than 30"; break; }
        custom[p - 1].e[e - 1].used = 1;
        custom[p - 1].e[e - 1].rgb = rgb < 0 ? LJ_CUSTOM_NO_RGB : (uint32_t)rgb;
        lj_str_copy(custom[p - 1].e[e - 1].bitmap, sizeof custom[0].e[0].bitmap, got == 1 ? q : "");
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(custom, 0, sizeof(lj_palette) * LJ_PALETTES); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}
int lj_fixture_custom_save(const char *path, const lj_palette custom[6])
{
    char q[2 * 31 + 2];
    int p, e, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "custompal", 1);
    if (!rec) return -1;
    for (p = 0; ok && p < LJ_PALETTES; p++)
        for (e = 0; ok && e < LJ_PAL_ENTRIES; e++) {
            const lj_pal_entry *x = &custom[p].e[e];
            if (!x->used) continue;
            lj_kv_quote(q, sizeof q, x->bitmap);
            ok = lj_rec_write(rec, "pal ", "%d entry=%d rgb=%ld bmp=%s", p + 1, e + 1, x->rgb == LJ_CUSTOM_NO_RGB ? -1L : (long)(x->rgb & 0xFFFFFFu), q) == 0;
        }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}
