/* lj_cuelist.c - CueLists\NNNNNNNN.Cl2 (lj_cuelist.h, design 5.2). W1 ships the
 * file and the line edits; W3 runs it. */
#include <stdlib.h>
#include <string.h>
#include "model/lj_cuelist.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

#define LJ_CL_PARAM_MAX 86399999L   /* the widest param= in ms: 23:59:59.999 (Wait PC-Clock, MTC); Wait Elapsed 99:59:99 and the rest fit below */
#define LJ_CL_TRIG_ALL  0xFFFL      /* slots 1-12 */

/* The cmd= words, in the order of 03 s6.3 (index = lj_cl_line.cmd; W3 derives its enum from this order). */
const char *const lj_cl_cmd_words[] = {
    "none", "wait_cd", "cd_play_from_track", "cd_play_from_time", "cd_stop",
    "wait_da", "play_da_file", "cue_da_file", "da_stop", "seek_da", "da_play", "da_timecode_factor",
    "mp_load_file", "mp_play", "mp_seek", "mp_stop", "mp_pause", "mp_wait", "mp_close_clip", "mp_start", "mp_close",
    "wait_mtc", "autoseek_mtc", "wait_elapsed", "wait_pc_clock", "wait_date_change",
    "goto_line", "execute_file", "set_current_cue", "set_cue_trig", "hotkey_command",
    "flash_sequence", "load_background_cue", "link_cuelist", NULL };
static const char *const lj_cl_adv_words[] = { "auto", "manual", NULL };

static int lj_cuelist_reserve(lj_cuelist *cl, uint32_t n)
{
    lj_cl_line *l;
    uint32_t cap;
    if (n <= cl->cap) return 0;
    cap = cl->cap ? cl->cap * 2 : 16;
    if (cap < n) cap = n;
    l = (lj_cl_line *)realloc(cl->lines, (size_t)cap * sizeof *l);
    if (!l) lj_fatal("lj_cuelist: out of memory at %u lines", (unsigned)cap);
    cl->lines = l; cl->cap = cap;
    return 0;
}

int lj_cuelist_load(const char *path, lj_cuelist *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "cl", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 cl file"); return -1; }
    free(out->lines);                                   /* out is a zeroed or valid record */
    memset(out, 0, sizeof *out);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "num") == 0) { int x = 0; out->number = (uint32_t)lj_kv_range_lead(v, 0, 0, LJ_NUMBER_MAX, &x); if (x) { bad = "num out of range"; break; } }
        else if (strcmp(k, "name") == 0) lj_kv_unquote(out->name, sizeof out->name, v);
        else if (strcmp(k, "tracking") == 0) out->tracking = (uint8_t)(strtol(v, NULL, 10) != 0);
        else if (strcmp(k, "mark") == 0) {
            long i = strtol(v, &e, 10);
            if (e == v || i < 1 || i > LJ_MARKS || *e != '=') { bad = "mark index not 1..4"; break; }
            out->mark[i - 1] = (uint32_t)strtoul(e + 1, NULL, 10);
        } else if (strcmp(k, "line") == 0) {
            lj_cl_line *l;
            char w[32];
            int x = 0, wr;
            if (strtol(v, &e, 10) != (long)out->n + 1 || e == v) { bad = "line index out of order"; break; }
            lj_cuelist_reserve(out, out->n + 1);
            l = &out->lines[out->n];
            memset(l, 0, sizeof *l);
            l->line = out->n + 1;
            l->manual = (uint8_t)lj_kv_word(v, "adv", lj_cl_adv_words, LJ_CL_AUTO);
            l->param_ms = (uint32_t)lj_kv_range(v, "param", 0, 0, LJ_CL_PARAM_MAX, &x);
            l->ref = (uint32_t)lj_kv_range(v, "ref", 0, 0, LJ_NUMBER_MAX, &x);
            l->cue = (uint32_t)lj_kv_range(v, "cue", 0, 0, LJ_NUMBER_MAX, &x);
            l->trig_mask = (uint16_t)lj_kv_range(v, "trig", 0, 0, LJ_CL_TRIG_ALL, &x);
            if (x) { bad = "line value out of range"; break; }
            if (lj_kv_get(v, "pstr", q, sizeof q) == 1) lj_str_copy(l->param, sizeof l->param, q);
            if (lj_kv_get(v, "comment", q, sizeof q) == 1) lj_str_copy(l->comment, sizeof l->comment, q);
            if ((wr = lj_kv_get(v, "cmd", w, sizeof w)) < 0) { bad = "command word too long"; break; }
            if (wr == 1) {
                int c = lj_word_index(lj_cl_cmd_words, w);
                if (c < 0) {                            /* unknown word (design 8.5 load error): kept in param as the word, a NUL, then the pstr, so save writes the line back as read */
                    size_t wn = strlen(w), pn = strlen(l->param);
                    if (wn + 1 + pn + 1 > sizeof l->param) pn = sizeof l->param - wn - 2;   /* wn <= 31: the pstr tail is cut, the word never */
                    memmove(l->param + wn + 1, l->param, pn); l->param[wn + 1 + pn] = 0;
                    memcpy(l->param, w, wn + 1);
                    c = LJ_CL_NONE;
                    l->load_error = 1;
                }
                l->cmd = (uint8_t)c;
            }
            if (lj_cl_cmd_inert(l->cmd) || l->cmd == LJ_CL_SET_CURRENT_CUE || l->cmd == LJ_CL_SET_CUE_TRIG) l->load_error = 1;   /* CD / DA / MP (design 8.5), panel actions stored as a cmd */
            if (l->load_error) lj_log("%s line %u: %s not available, kept and skipped (load error)", path, (unsigned)l->line, l->cmd ? lj_cl_cmd_words[l->cmd] : l->param);
            out->n++;
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_cuelist_validate(out);
    if (bad) { lj_kv_err(err, cap, path, item, bad); free(out->lines); out->lines = NULL; out->n = out->cap = 0; return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_cuelist_save(const char *path, const lj_cuelist *in)
{
    char q[72], p[2 * sizeof in->lines->param + 2], c[2 * sizeof in->lines->comment + 2];
    uint32_t i;
    int ok;
    lj_rec *rec = lj_rec_open_write(path, "cl", 1);
    if (!rec) return -1;
    ok = lj_rec_write(rec, "num", "%u", (unsigned)in->number) == 0;
    lj_kv_quote(q, sizeof q, in->name); ok = ok && lj_rec_write(rec, "name", "%s", q) == 0;
    ok = ok && lj_rec_write(rec, "tracking", "%d", in->tracking) == 0;
    for (i = 0; ok && i < LJ_MARKS; i++) if (in->mark[i]) ok = lj_rec_write(rec, "mark ", "%u=%u", (unsigned)i + 1, (unsigned)in->mark[i]) == 0;
    for (i = 0; ok && i < in->n; i++) {
        const lj_cl_line *l = &in->lines[i];
        const char *word = lj_cl_cmd_words[l->cmd], *pstr = l->param;
        if (l->load_error && l->cmd == LJ_CL_NONE && l->param[0]) { word = l->param; pstr = l->param + strlen(l->param) + 1; }   /* an unknown word, as read (load) */
        lj_kv_quote(p, sizeof p, pstr);
        lj_kv_quote(c, sizeof c, l->comment);
        ok = lj_rec_write(rec, "line ", "%u adv=%s cmd=%s param=%u pstr=%s ref=%u cue=%u trig=%u comment=%s", (unsigned)i + 1,
                          lj_cl_adv_words[l->manual ? 1 : 0], word, (unsigned)l->param_ms, p, (unsigned)l->ref, (unsigned)l->cue, l->trig_mask, c) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_cuelist_validate(const lj_cuelist *in)
{
    uint32_t i;
    int ncmd = 0;
    while (lj_cl_cmd_words[ncmd]) ncmd++;
    if (!memchr(in->name, 0, sizeof in->name)) return "name not NUL-terminated";
    if (in->n && !in->lines) return "lines missing";
    for (i = 0; i < in->n; i++) {
        const lj_cl_line *l = &in->lines[i];
        if (l->line != i + 1) return "line numbers not 1..n";
        if (l->manual > LJ_CL_MANUAL) return "advance mode not auto/manual";
        if (l->cmd >= ncmd) return "unknown command";
        if (!memchr(l->param, 0, sizeof l->param) || !memchr(l->comment, 0, sizeof l->comment)) return "parameter or comment not NUL-terminated";
        if (l->cue && l->trig_mask) return "a line latches a cue or trigs slots, not both";   /* 03 s6.1 */
        if (l->trig_mask > LJ_CL_TRIG_ALL) return "trig mask beyond slot 12";
        if (l->load_error) continue;                                     /* kept as read, never executed (design 8.5) */
        if (lj_cl_cmd_inert(l->cmd)) return "CD / DA / MP command without its load error flag";
        if (l->cmd == LJ_CL_SET_CURRENT_CUE || l->cmd == LJ_CL_SET_CUE_TRIG) return "set_current_cue / set_cue_trig are panel actions, not stored commands";
        if (l->cmd == LJ_CL_FLASH_SEQUENCE && l->param_ms > LJ_CL_FLASH_MAX_MS) return "flash duration above 99.9 s";
        if (l->cmd == LJ_CL_WAIT_PC_CLOCK && l->param_ms > LJ_CL_PARAM_MAX) return "PC clock time beyond 23:59:59";
    }
    for (i = 0; i < LJ_MARKS; i++) if (in->mark[i] > in->n) return "mark beyond the last line";
    return NULL;
}

lj_cuelist *lj_cuelist_new_rec(void)
{
    lj_cuelist *cl = (lj_cuelist *)calloc(1, sizeof *cl);
    if (!cl) lj_fatal("lj_cuelist_new_rec: out of memory");
    return cl;
}

void lj_cuelist_free(lj_cuelist *cl) { if (!cl) return; free(cl->lines); free(cl); }

static void lj_cuelist_renumber(lj_cuelist *cl) { uint32_t i; for (i = 0; i < cl->n; i++) cl->lines[i].line = i + 1; }

int lj_cuelist_insert(lj_cuelist *cl, uint32_t before, const lj_cl_line *line)
{
    if (before > cl->n) before = cl->n;
    lj_cuelist_reserve(cl, cl->n + 1);
    memmove(cl->lines + before + 1, cl->lines + before, (size_t)(cl->n - before) * sizeof *cl->lines);
    cl->lines[before] = *line;
    cl->n++;
    lj_cuelist_renumber(cl);
    return 0;
}

int lj_cuelist_delete(lj_cuelist *cl, uint32_t index)
{
    if (index >= cl->n) return -1;
    memmove(cl->lines + index, cl->lines + index + 1, (size_t)(cl->n - index - 1) * sizeof *cl->lines);
    cl->n--;
    lj_cuelist_renumber(cl);
    return 0;
}

/* ---- W3 contracts (lj_cuelist.h): the verbatim tables and the small helpers; load/save/validate W3 rules by cuelist-core ---- */
#include <stdio.h>

const char *const lj_cl_cmd_caption[LJ_CL_CMD_COUNT] = {
    "", "Wait CD", "CD Play From Track", "CD Play From Time", "CD Stop",
    "Wait DA", "Play DA File", "Cue DA File", "DA Stop", "Seek DA", "DA Play", "DA Timecode Factor",
    "MP Load File", "MP Play", "MP Seek", "MP Stop", "MP Pause", "MP Wait", "MP Close Clip", "MP Start", "MP Close",
    "Wait MTC", "Auto Seek MTC", "Wait Elapsed", "Wait PC-Clock", "Wait Date-Change",
    "Goto Line#", "Execute File", "Set Current Cue", "Set Cue Trig", "Hotkey Command",
    "Flash Seq", "Load BGCue", "Link Cuelist" };
const char *const lj_cl_cmd_grid[LJ_CL_CMD_COUNT] = {
    "", "Wait for CD at", "CD Play Track", "CD Play Time", "CD Stop",
    "Wait for DA at", "DA Play File", "DA Cue File", "DA Stop", "DA Seek", "DA Play", "DA Timecode Factor",
    "MP Load File", "MP Play", "MP Seek", "MP Stop", "MP Pause", "Wait for MP at", "MP Close Clip", "MP Start", "MP Close",
    "Wait for MTC at", "Auto Seek MTC", "Wait Elapsed", "Wait PC-Clock at", "Wait PC date-change",
    "Goto Line", "Execute", "", "", "Hotkey Command",
    "Flash Seq", "Load BGCue", "Link Cuelist" };
const char *const lj_cl_adv_caption[2] = { "Auto", "Man" };
_Static_assert(sizeof lj_cl_cmd_words / sizeof lj_cl_cmd_words[0] == LJ_CL_CMD_COUNT + 1, "lj_cl_cmd_words and enum lj_cl_cmd disagree");

int lj_cl_cmd_family(int cmd)
{
    if (cmd < 0 || cmd >= LJ_CL_CMD_COUNT) return -1;
    if (cmd == LJ_CL_NONE) return LJ_CLF_NONE;
    if (cmd <= LJ_CL_CD_STOP) return LJ_CLF_CD;
    if (cmd <= LJ_CL_DA_TIMECODE_FACTOR) return LJ_CLF_DA;
    if (cmd <= LJ_CL_MP_CLOSE) return LJ_CLF_MP;
    if (cmd <= LJ_CL_AUTOSEEK_MTC) return LJ_CLF_MTC;
    if (cmd <= LJ_CL_WAIT_DATE_CHANGE) return LJ_CLF_PC;
    if (cmd <= LJ_CL_HOTKEY_COMMAND) return LJ_CLF_OTHER;
    return LJ_CLF_DRAG;
}
int lj_cl_cmd_inert(int cmd) { int f = lj_cl_cmd_family(cmd); return f == LJ_CLF_CD || f == LJ_CLF_DA || f == LJ_CLF_MP; }
int lj_cl_cmd_is_wait(int cmd)
{
    return cmd == LJ_CL_WAIT_CD || cmd == LJ_CL_WAIT_DA || cmd == LJ_CL_MP_WAIT || cmd == LJ_CL_WAIT_MTC
        || cmd == LJ_CL_WAIT_ELAPSED || cmd == LJ_CL_WAIT_PC_CLOCK || cmd == LJ_CL_WAIT_DATE_CHANGE;
}

uint32_t lj_cl_hmsf_ms(int h, int m, int s, int f, int fps)
{
    if (fps <= 0) fps = 100;
    return (uint32_t)(((h * 60 + m) * 60 + s) * 1000) + (uint32_t)((f * 1000 + fps - 1) / fps);   /* ceil: round-trips at 24/25/30/75/100 */
}
void lj_cl_ms_hmsf(uint32_t ms, int fps, int *h, int *m, int *s, int *f)
{
    if (fps <= 0) fps = 100;
    *f = (int)((ms % 1000) * (uint32_t)fps / 1000); ms /= 1000;
    *s = (int)(ms % 60); ms /= 60;
    *m = (int)(ms % 60); *h = (int)(ms / 60);
}
int lj_cl_tc_text(char *dst, size_t cap, int32_t ms, int fps)
{
    int h, m, s, f, n;
    if (!dst || !cap) return -1;
    if (ms < 0) n = snprintf(dst, cap, "--:--:--:--");
    else { lj_cl_ms_hmsf((uint32_t)ms, fps, &h, &m, &s, &f); n = snprintf(dst, cap, "%02d:%02d:%02d:%02d", h, m, s, f); }
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}
int lj_cl_line_state(const lj_cl_status *st, const lj_cuelist *cl, uint32_t line)
{
    if (!cl || line == 0 || line > cl->n) return LJ_CLL_PLAIN;
    if (cl->lines[line - 1].load_error) return LJ_CLL_ERROR;
    if (st && st->state != LJ_CL_IDLE && st->next == line) return LJ_CLL_NEXT;
    if (st && st->last == line) return LJ_CLL_DONE;
    return LJ_CLL_PLAIN;
}
