/* lj_groups.c - GroupInfo.dat (lj_groups.h, design 5.2, 18 s8). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_groups.h"
#include "model/lj_kv.h"
#include "core/lj_str.h"

int lj_groups_load(const char *path, lj_group groups[LJ_USER_GROUPS], char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "groups", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 groups file"); return -1; }
    memset(groups, 0, (size_t)LJ_USER_GROUPS * sizeof *groups);
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "group") == 0) {
            long i = strtol(v, &e, 10);
            uint32_t m[LJ_FIXTURES];
            int n, j;
            if (e == v || i < 1 || i > LJ_USER_GROUPS) { bad = "group index not 1..99"; break; }
            if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(groups[i - 1].name, sizeof groups[i - 1].name, q);
            if (lj_kv_get(v, "members", q, sizeof q) == 1 && q[0]) {
                if ((n = lj_kv_list(q, m, LJ_FIXTURES)) < 0) { bad = "bad members= list"; break; }
                for (j = 0; j < n; j++) {
                    if (m[j] < 1 || m[j] > LJ_FIXTURES) { bad = "member not 1..100"; break; }
                    groups[i - 1].member[m[j] - 1] = 1;
                }
                if (bad) break;
            }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_groups_save(const char *path, const lj_group groups[LJ_USER_GROUPS])
{
    char q[72], list[LJ_FIXTURES * 4 + 1];
    int i, j, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "groups", 1);
    if (!rec) return -1;
    for (i = 0; ok && i < LJ_USER_GROUPS; i++) {
        size_t len = 0;
        int any = 0;
        list[0] = 0;
        for (j = 0; j < LJ_FIXTURES; j++)
            if (groups[i].member[j]) { len += (size_t)snprintf(list + len, sizeof list - len, "%s%d", any ? "," : "", j + 1); any = 1; }
        if (!any && !groups[i].name[0]) continue;
        lj_kv_quote(q, sizeof q, groups[i].name);
        ok = lj_rec_write(rec, "group ", "%d name=%s members=%s", i + 1, q, list) == 0;
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

const char *lj_groups_validate(const lj_group groups[LJ_USER_GROUPS])
{
    int i, j;
    for (i = 0; i < LJ_USER_GROUPS; i++) {
        if (!memchr(groups[i].name, 0, sizeof groups[i].name)) return "group name not NUL-terminated";
        for (j = 0; j < LJ_FIXTURES; j++) if (groups[i].member[j] > 1) return "membership flag not 0/1";
    }
    return NULL;
}
