/* lj_followspot.c - the 20 follow spot settings (lj_followspot.h; 04 s13, CHM followspotfunction.htm). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen probe for the missing-file case (as model/lj_show.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_followspot.h"
#include "model/lj_kv.h"
#include "model/lj_show.h"
#include "core/lj_str.h"

/* A fixture that may be a follow spot: a user profile with "Enable Follow Spot Function" checked (CHM thepantilttab.htm);
 * built-ins by the same flag where the hand table sets it, else any built-in with a pan channel (PROPOSAL rig 04 s13). */
static int eligible(const lj_profile *p)
{
    int o;
    if (!p) return 0;
    if (p->follow_spot || p->gs2_raw) return p->follow_spot != 0;
    for (o = 0; o < p->footprint && o < LJ_PROFILE_CH; o++) if (p->channel_class[o] == LJ_CH_PAN) return 1;
    return 0;
}

int lj_followspot_record(lj_followspot *fs, const lj_show *show, const lj_scene *sc, const uint8_t selected[LJ_FIXTURES])
{
    int i, o, n = 0;
    if (!fs || !show || !sc || !selected) return -1;
    for (i = 0; i < LJ_FIXTURES; i++) if (selected[i] && eligible(lj_show_profile(show, i)) && lj_show_fixture_base(show, i) >= 0) n++;
    if (!n) return -1;                                                     /* "No Followspot Fixtures Selected" (the window says it) */
    memset(fs->selected, 0, sizeof fs->selected);
    memset(fs->val, 0, sizeof fs->val);
    memset(fs->state, LJ_OFF, sizeof fs->state);
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_profile *p = lj_show_profile(show, i);
        int base = lj_show_fixture_base(show, i);
        if (!selected[i] || !eligible(p) || base < 0) continue;            /* "Non Followspot Fixtures Ignored" */
        fs->selected[i] = 1;
        for (o = 0; o < p->footprint && base + o < LJ_CHANS; o++)
            if (sc->state[base + o] != LJ_OFF) { fs->val[base + o] = sc->val[base + o]; fs->state[base + o] = sc->state[base + o]; }
    }
    fs->used = 1;
    return 0;
}

int lj_followspot_apply(const lj_followspot *fs, const lj_show *show, lj_scene *sc, const uint8_t *only, uint8_t sel_out[LJ_FIXTURES])
{
    int i, o, n = 0;
    if (!fs || !fs->used || !show || !sc) return -1;
    if (sel_out) memset(sel_out, 0, LJ_FIXTURES);
    for (i = 0; i < LJ_FIXTURES; i++) {
        const lj_profile *p = lj_show_profile(show, i);
        int base = lj_show_fixture_base(show, i);
        if (!fs->selected[i] || (only && !only[i]) || !p || base < 0) continue;
        for (o = 0; o < p->footprint && o < LJ_PROFILE_CH && base + o < LJ_CHANS; o++) {
            int ch = base + o;                                             /* programmed values, else the profile's default DMX; every channel Snap (11 F22) */
            sc->val[ch] = fs->state[ch] != LJ_OFF ? fs->val[ch] : p->default_dmx[o];
            sc->state[ch] = LJ_SNAP;
        }
        if (sel_out) sel_out[i] = 1;
        n++;
    }
    return n;
}

const char *lj_followspot_validate(const lj_followspot fs[LJ_FOLLOWSPOT])
{
    int k, i;
    if (!fs) return "no settings";
    for (k = 0; k < LJ_FOLLOWSPOT; k++) {
        const lj_followspot *f = &fs[k];
        if (f->used > 1 || f->static_auto_clear > 1) return "flag not 0/1";
        if (!memchr(f->name, 0, sizeof f->name)) return "name not NUL-terminated";
        if (f->static_page > LJ_STATIC_PAGES_MAX || f->static_slot > LJ_STATICS2_SLOTS_MAX) return "static not 0..64";
        if ((f->static_page == 0) != (f->static_slot == 0)) return "static page and slot not both set";
        for (i = 0; i < LJ_FIXTURES; i++) if (f->selected[i] > 1) return "selection flag not 0/1";
        for (i = 0; i < LJ_CHANS; i++) if (f->state[i] > LJ_FADE) return "fade state not Off/Snap/Fade";
    }
    return NULL;
}

/* ---- FollowSpot.dat: "LJ4 follow 1" / spot n used=1 name=".." sel=1,2 static=p.s autoclr=1 / ch l:a=v:S ... ---- */

int lj_followspot_load(const char *path, lj_followspot fs[LJ_FOLLOWSPOT], char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, rbad = 0, cur = -1;
    const char *bad = NULL;
    lj_rec *rec;
    FILE *f;
    if (err && cap) err[0] = 0;
    if (!fs) return -1;
    memset(fs, 0, sizeof(lj_followspot) * LJ_FOLLOWSPOT);
    if (!path || (f = fopen(path, "rb")) == NULL) return 0;              /* missing file = 20 unused settings */
    fclose(f);
    if ((rec = lj_rec_open_read(path, "follow", &fmt)) == NULL) { lj_kv_err(err, cap, path, 0, "not an LJ4 follow file"); return -1; }
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "spot") == 0) {
            lj_followspot *s;
            cur = (int)lj_kv_range_lead(v, 0, 1, LJ_FOLLOWSPOT, &rbad) - 1;
            if (rbad) { bad = "spot not 1..20"; break; }
            s = &fs[cur];
            s->used = (uint8_t)lj_kv_range(v, "used", 0, 0, 1, &rbad);
            s->static_auto_clear = (uint8_t)lj_kv_range(v, "autoclr", 0, 0, 1, &rbad);
            if (rbad) { bad = "used= / autoclr= not 0/1"; break; }
            q[0] = 0;
            if (lj_kv_get(v, "name", q, sizeof q) < 0) { bad = "bad name"; break; }
            lj_str_copy(s->name, sizeof s->name, q);
            if (lj_kv_get(v, "sel", q, sizeof q) == 1 && q[0]) {
                uint32_t m[LJ_FIXTURES];
                int n = lj_kv_list(q, m, LJ_FIXTURES), j;
                if (n < 0) { bad = "bad sel= list"; break; }
                for (j = 0; j < n; j++) { if (m[j] < 1 || m[j] > LJ_FIXTURES) { bad = "sel not 1..100"; break; } s->selected[m[j] - 1] = 1; }
                if (bad) break;
            }
            if (lj_kv_get(v, "static", q, sizeof q) == 1) {
                char *e;
                long pg = strtol(q, &e, 10), sl = 0;
                if (*e == '.') { const char *t = e + 1; sl = strtol(t, &e, 10); if (e == t) sl = -1; }
                if (*e || pg < 1 || pg > LJ_STATIC_PAGES_MAX || sl < 1 || sl > LJ_STATICS2_SLOTS_MAX) { bad = "static= not p.s"; break; }
                s->static_page = (uint8_t)pg; s->static_slot = (uint8_t)sl;
            }
        } else if (strcmp(k, "ch") == 0) {
            if (cur < 0) { bad = "ch line before a spot line"; break; }
            if (lj_kv_ch_parse(v, fs[cur].val, fs[cur].state) != 0) { bad = "bad channel line"; break; }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad && lj_followspot_validate(fs)) bad = lj_followspot_validate(fs);
    if (bad) { lj_kv_err(err, cap, path, item, bad); memset(fs, 0, sizeof(lj_followspot) * LJ_FOLLOWSPOT); return -1; }
    return 0;
}

int lj_followspot_save(const char *path, const lj_followspot fs[LJ_FOLLOWSPOT])
{
    char q[72], list[LJ_FIXTURES * 4 + 1], st[16];
    int k, i, ok = 1;
    lj_rec *rec;
    if (!path || !fs || lj_followspot_validate(fs)) return -1;
    if ((rec = lj_rec_open_write(path, "follow", 1)) == NULL) return -1;
    for (k = 0; ok && k < LJ_FOLLOWSPOT; k++) {
        const lj_followspot *f = &fs[k];
        size_t len = 0;
        if (!f->used && !f->name[0] && !f->static_page) continue;
        list[0] = 0;
        for (i = 0; i < LJ_FIXTURES; i++) if (f->selected[i]) len += (size_t)snprintf(list + len, sizeof list - len, "%s%d", len ? "," : "", i + 1);
        if (lj_kv_quote(q, sizeof q, f->name) != 0) { ok = 0; break; }
        st[0] = 0;
        if (f->static_page) snprintf(st, sizeof st, " static=%d.%d", f->static_page, f->static_slot);
        ok = lj_rec_write(rec, "spot ", "%d used=%d name=%s%s%s%s autoclr=%d", k + 1, f->used, q[0] ? q : "\"\"",
                          list[0] ? " sel=" : "", list, st, f->static_auto_clear) == 0;
        if (ok && f->used) ok = lj_kv_ch_write(rec, f->val, f->state) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}
