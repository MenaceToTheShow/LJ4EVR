/* lj_statics.c - Statics.dat (lj_statics.h, design 5.2, 14 sB). Recorded slots own
 * their lj_scene; link slots name a sequence number + scene index. Every numeric
 * field goes through lj_kv_range (ME-77). */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/lj_statics.h"
#include "model/lj_scene.h"
#include "model/lj_kv.h"
#include "model/lj_words.h"
#include "core/lj_str.h"

int lj_statics_load(const char *path, lj_static_page **pages, int *npages, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP], *e;
    int fmt, r, item = 0, cur = -1, rbad = 0;
    lj_static_slot *slot = NULL;
    const char *bad = NULL;
    lj_rec *rec = lj_rec_open_read(path, "statics", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 statics file"); return -1; }
    *pages = NULL; *npages = 0;
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "page") == 0) {
            if (strtol(v, &e, 10) != *npages + 1 || e == v) { bad = "page index out of order"; break; }
            if (lj_kv_get(v, "name", q, sizeof q) < 0 || (cur = lj_statics_add_page(pages, npages, q)) < 0) { bad = "too many pages"; break; }
            (*pages)[cur].n = (uint16_t)lj_kv_range(v, "n", LJ_STATICS2_SLOTS_PER_PAGE, 1, LJ_STATICS2_SLOTS_MAX, &rbad);   /* ME-30: Visible Slots 1..64 */
            slot = NULL;
        } else if (strcmp(k, "slot") == 0) {
            long i = strtol(v, &e, 10);
            if (cur < 0) { bad = "slot before the first page"; break; }
            if (e == v || i < 1 || i > LJ_STATICS2_SLOTS_PER_PAGE) { bad = "slot index not 1..20"; break; }
            slot = &(*pages)[cur].slot[i - 1];
            if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(slot->name, sizeof slot->name, q);
            if (lj_kv_get(v, "type", q, sizeof q) == 1) lj_str_copy(slot->type, sizeof slot->type, q);
            slot->activation = (uint8_t)lj_kv_word(v, "act", lj_act_words, LJ_ACT_TOGGLE);
            slot->replace = (uint8_t)lj_kv_word(v, "rep", lj_replace_words, LJ_REP_ADD);
            slot->fade_ms = (uint32_t)lj_kv_range(v, "ft", LJ_FADE_TIME_DEFAULT_MS, 0, LJ_TIME_MAX_MS, &rbad);
            slot->level_pct = (uint8_t)lj_kv_range(v, "level", 100, 0, 100, &rbad);      /* ME-77: level=356 fails, never aliases to 100 */
            slot->transparent = (uint8_t)lj_kv_range(v, "transparent", 0, 0, 1, &rbad);
            slot->groups = 0;
            if (lj_kv_get(v, "groups", q, sizeof q) == 1) {                                 /* the 64-bit mask: digits only */
                slot->groups = strtoull(q, &e, 10);
                if (e == q || *e || q[0] == '-') rbad = 1;
            }
            slot->kind = (uint8_t)lj_kv_word(v, "kind", lj_static_kind_words, LJ_STATIC_EMPTY);
            slot->link_seq = 0; slot->link_scene = 0;
            if (lj_kv_get(v, "link", q, sizeof q) == 1) {                                   /* link=<seq>,<scene index> */
                e = strchr(q, ',');
                if (e) *e++ = 0;
                slot->link_seq = (uint32_t)lj_kv_range_lead(q, 0, 0, LJ_NUMBER_MAX, &rbad);
                slot->link_scene = (uint16_t)(e ? lj_kv_range_lead(e, 0, 0, LJ_SCENES_MAX - 1, &rbad) : 0);
            }
            if (slot->kind == LJ_STATIC_RECORDED) {
                slot->scene = (lj_scene *)malloc(sizeof *slot->scene);
                if (!slot->scene) lj_fatal("lj_statics_load: out of memory");
                lj_scene_init(slot->scene);
            }
        } else if (strcmp(k, "scene") == 0 || strcmp(k, "ch") == 0 || strcmp(k, "fx") == 0) {
            if (!slot || !slot->scene) { bad = "scene data outside a recorded slot"; break; }
            if (lj_kv_scene_field(slot->scene, k, v) < 0) { bad = "bad scene line"; break; }
        }
        if (rbad) { bad = "number out of range"; break; }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_statics_validate(*pages, *npages);
    if (bad) { lj_kv_err(err, cap, path, item, bad); lj_statics_free(*pages, *npages); *pages = NULL; *npages = 0; return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

static int lj_statics_write(const char *path, const lj_static_page *pages, int npages, const lj_static_groups *groups)
{
    char qn[2 * 64 + 2], qt[2 * 64 + 2];
    int p, i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "statics", 1);
    if (!rec) return -1;
    for (i = 0; ok && groups && i < LJ_STATIC_GROUPS; i++) {         /* only names that differ from the "Group %d" default [14 sB.3] */
        snprintf(qt, sizeof qt, "Group %d", i + 1);
        if (!groups->name[i][0] || strcmp(groups->name[i], qt) == 0) continue;
        lj_kv_quote(qn, sizeof qn, groups->name[i]);
        ok = lj_rec_write(rec, "group ", "%d name=%s", i + 1, qn) == 0;
    }
    for (p = 0; ok && p < npages; p++) {
        const lj_static_page *pg = &pages[p];
        lj_kv_quote(qn, sizeof qn, pg->name);
        ok = lj_rec_write(rec, "page ", "%d name=%s n=%u", p + 1, qn, pg->n) == 0;
        for (i = 0; ok && i < LJ_STATICS2_SLOTS_PER_PAGE; i++) {
            const lj_static_slot *s = &pg->slot[i];
            if (s->kind == LJ_STATIC_EMPTY && !s->name[0]) continue;
            lj_kv_quote(qn, sizeof qn, s->name);
            lj_kv_quote(qt, sizeof qt, s->type);
            ok = lj_rec_write(rec, "slot ", "%d name=%s type=%s act=%s rep=%s ft=%u level=%u transparent=%d groups=%" PRIu64 " kind=%s link=%u,%u",
                              i + 1, qn, qt, lj_word(lj_act_words, s->activation), lj_word(lj_replace_words, s->replace), (unsigned)s->fade_ms,
                              s->level_pct, s->transparent, s->groups, lj_word(lj_static_kind_words, s->kind), (unsigned)s->link_seq, s->link_scene) == 0;
            if (ok && s->kind == LJ_STATIC_RECORDED && s->scene) ok = lj_kv_scene_write(rec, s->scene) == 0;
        }
    }
    return lj_rec_close(rec) == 0 && ok ? 0 : -1;
}

int lj_statics_save(const char *path, const lj_static_page *pages, int npages) { return lj_statics_write(path, pages, npages, NULL); }

const char *lj_statics_validate(const lj_static_page *pages, int npages)
{
    int p, i;
    if (npages < 0 || npages > LJ_STATIC_PAGES_MAX || (npages && !pages)) return "page count not 0..64";
    for (p = 0; p < npages; p++) {
        if (!memchr(pages[p].name, 0, sizeof pages[p].name)) return "page name not NUL-terminated";
        if (pages[p].n < 1 || pages[p].n > LJ_STATICS2_SLOTS_MAX) return "visible slots not 1..64";   /* ME-30 */
        for (i = 0; i < LJ_STATICS2_SLOTS_PER_PAGE; i++) {
            const lj_static_slot *s = &pages[p].slot[i];
            if (!memchr(s->name, 0, sizeof s->name) || !memchr(s->type, 0, sizeof s->type)) return "slot name or type not NUL-terminated";
            if (s->activation > LJ_ACT_FLASH) return "activation not toggle/flash";
            if (s->replace > LJ_REP_SOLO_ALL) return "replace mode unknown";
            if (s->fade_ms > LJ_TIME_MAX_MS) return "fade time over 60000 ms";
            if (s->level_pct > 100) return "level over 100 %";
            if (s->transparent > 1) return "transparent flag not 0/1";
            if (s->kind > LJ_STATIC_LINK) return "slot kind unknown";
            if (s->kind == LJ_STATIC_RECORDED && (!s->scene || s->scene->fade_ms > LJ_TIME_MAX_MS)) return "recorded slot without a scene";
            if (s->kind == LJ_STATIC_LINK && !s->link_seq) return "link slot without a sequence";
            if (s->link_seq > LJ_NUMBER_MAX || s->link_scene >= LJ_SCENES_MAX) return "link sequence or scene out of range";
        }
    }
    return NULL;
}

void lj_statics_free(lj_static_page *pages, int npages)
{
    int p, i;
    for (p = 0; p < npages; p++)
        for (i = 0; i < LJ_STATICS2_SLOTS_PER_PAGE; i++) free(pages[p].slot[i].scene);
    free(pages);
}

int lj_statics_add_page(lj_static_page **pages, int *npages, const char *name)
{
    lj_static_page *p;
    if (*npages >= LJ_STATIC_PAGES_MAX) return -1;
    p = (lj_static_page *)realloc(*pages, (size_t)(*npages + 1) * sizeof *p);
    if (!p) lj_fatal("lj_statics_add_page: out of memory");
    *pages = p;
    p += *npages;
    memset(p, 0, sizeof *p);
    lj_str_copy(p->name, sizeof p->name, name ? name : "");
    p->n = LJ_STATICS2_SLOTS_PER_PAGE;
    return (*npages)++;
}

int lj_static_slot_record(lj_static_slot *slot, const lj_scene *scene)
{
    if (!slot->scene) {
        slot->scene = (lj_scene *)malloc(sizeof *slot->scene);
        if (!slot->scene) lj_fatal("lj_static_slot_record: out of memory");
    }
    lj_scene_copy(slot->scene, scene);
    slot->kind = LJ_STATIC_RECORDED;
    slot->link_seq = 0; slot->link_scene = 0;
    return 0;
}

void lj_static_slot_clear(lj_static_slot *slot)
{
    free(slot->scene);
    slot->scene = NULL;
    slot->kind = LJ_STATIC_EMPTY;
    slot->link_seq = 0; slot->link_scene = 0;
}

/* ---- W2 (14 sB): group names, link slots, the replace scopes ---- */

int lj_statics_groups_load(const char *path, lj_static_groups *out, char *err, size_t cap)
{
    char k[32], v[LJ_KV_VAL_CAP], q[LJ_KV_VAL_CAP];
    int fmt, r, item = 0, rbad = 0, i;
    const char *bad = NULL;
    lj_rec *rec;
    for (i = 0; i < LJ_STATIC_GROUPS; i++) snprintf(out->name[i], sizeof out->name[i], "Group %d", i + 1);   /* [14 sB.3] vocabulary */
    rec = lj_rec_open_read(path, "statics", &fmt);
    if (!rec) { lj_kv_err(err, cap, path, 0, "not an LJ4 statics file"); return -1; }
    while ((r = lj_rec_next(rec, k, sizeof k, v, sizeof v)) == 1) {
        item++;
        if (strcmp(k, "group") != 0) continue;
        i = (int)lj_kv_range_lead(v, 0, 1, LJ_STATIC_GROUPS, &rbad);
        if (rbad) { bad = "group index not 1..64"; break; }
        if (lj_kv_get(v, "name", q, sizeof q) < 0) { bad = "bad group name"; break; }
        lj_str_copy(out->name[i - 1], sizeof out->name[i - 1], q);
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (bad) { lj_kv_err(err, cap, path, item, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_statics_groups_save(const char *path, const lj_static_page *pages, int npages, const lj_static_groups *groups)
{
    return lj_statics_write(path, pages, npages, groups);
}

int lj_static_slot_link(lj_static_slot *slot, uint32_t seq, uint16_t scene)
{
    if (!seq || seq > LJ_NUMBER_MAX || scene >= LJ_SCENES_MAX) return -1;
    free(slot->scene);
    slot->scene = NULL;
    slot->kind = LJ_STATIC_LINK;
    slot->link_seq = seq; slot->link_scene = scene;
    return 0;
}

int lj_static_global_index(int page, int slot)
{
    if (page < 1 || page > LJ_STATIC_PAGES_MAX || slot < 1 || slot > LJ_STATICS2_SLOTS_PER_PAGE) return -1;
    return (page - 1) * LJ_STATICS2_SLOTS_PER_PAGE + (slot - 1);
}

int lj_static_scope_hits(const lj_static_page *pages, int npages, int actor, int other)
{
    const int per = LJ_STATICS2_SLOTS_PER_PAGE, all = npages * per;
    const lj_static_slot *a, *o;
    if (!pages || actor < 0 || other < 0 || actor >= all || other >= all || actor == other) return 0;
    a = &pages[actor / per].slot[actor % per];
    o = &pages[other / per].slot[other % per];
    switch (a->replace) {                          /* Solo reads as Replace in W2: PROPOSAL rig 14 sB.8 item 3 (LJ_STATIC_SOLO_BLOCKS_LATER 0) */
    case LJ_REP_SLOT:  case LJ_REP_SOLO_SLOT:  return actor % per == other % per;   /* the same slot row on every page */
    case LJ_REP_PAGE:  case LJ_REP_SOLO_PAGE:  return actor / per == other / per;
    case LJ_REP_GROUP: case LJ_REP_SOLO_GROUP: return (a->groups & o->groups) != 0;
    case LJ_REP_ALL:   case LJ_REP_SOLO_ALL:   return 1;
    default:                                    return 0;                           /* Add: LTP merge, releases nothing (PROPOSAL rig 14 sB.8 item 3) */
    }
}
