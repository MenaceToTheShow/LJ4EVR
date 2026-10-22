/* lj_statics.c - Statics.dat (lj_statics.h, design 5.2, 14 sB). Recorded slots own
 * their lj_scene; link slots name a sequence number + scene index. */
#include <inttypes.h>
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
    int fmt, r, item = 0, cur = -1;
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
            (*pages)[cur].n = (uint16_t)lj_kv_long(v, "n", LJ_STATICS2_SLOTS_PER_PAGE);
            slot = NULL;
        } else if (strcmp(k, "slot") == 0) {
            long i = strtol(v, &e, 10), a, b;
            if (cur < 0) { bad = "slot before the first page"; break; }
            if (e == v || i < 1 || i > LJ_STATICS2_SLOTS_PER_PAGE) { bad = "slot index not 1..20"; break; }
            slot = &(*pages)[cur].slot[i - 1];
            if (lj_kv_get(v, "name", q, sizeof q) == 1) lj_str_copy(slot->name, sizeof slot->name, q);
            if (lj_kv_get(v, "type", q, sizeof q) == 1) lj_str_copy(slot->type, sizeof slot->type, q);
            slot->activation = (uint8_t)lj_kv_word(v, "act", lj_act_words, LJ_ACT_TOGGLE);
            slot->replace = (uint8_t)lj_kv_word(v, "rep", lj_replace_words, LJ_REP_ADD);
            slot->fade_ms = (uint32_t)lj_kv_long(v, "ft", LJ_FADE_TIME_DEFAULT_MS);
            slot->level_pct = (uint8_t)lj_kv_long(v, "level", 100);
            slot->transparent = (uint8_t)(lj_kv_long(v, "transparent", 0) != 0);
            slot->groups = lj_kv_get(v, "groups", q, sizeof q) == 1 ? strtoull(q, NULL, 10) : 0;
            slot->kind = (uint8_t)lj_kv_word(v, "kind", lj_static_kind_words, LJ_STATIC_EMPTY);
            a = b = 0;
            if (lj_kv_get(v, "link", q, sizeof q) == 1) { a = strtol(q, &e, 10); if (*e == ',') b = strtol(e + 1, NULL, 10); }
            slot->link_seq = (uint32_t)a; slot->link_scene = (uint16_t)b;
            if (slot->kind == LJ_STATIC_RECORDED) {
                slot->scene = (lj_scene *)malloc(sizeof *slot->scene);
                if (!slot->scene) lj_fatal("lj_statics_load: out of memory");
                lj_scene_init(slot->scene);
            }
        } else if (strcmp(k, "scene") == 0 || strcmp(k, "ch") == 0 || strcmp(k, "fx") == 0) {
            if (!slot || !slot->scene) { bad = "scene data outside a recorded slot"; break; }
            if (lj_kv_scene_field(slot->scene, k, v) < 0) { bad = "bad scene line"; break; }
        }
    }
    lj_rec_close(rec);
    if (!bad && r < 0) bad = "unreadable line";
    if (!bad) bad = lj_statics_validate(*pages, *npages);
    if (bad) { lj_kv_err(err, cap, path, item, bad); lj_statics_free(*pages, *npages); *pages = NULL; *npages = 0; return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

int lj_statics_save(const char *path, const lj_static_page *pages, int npages)
{
    char qn[2 * 64 + 2], qt[2 * 64 + 2];
    int p, i, ok = 1;
    lj_rec *rec = lj_rec_open_write(path, "statics", 1);
    if (!rec) return -1;
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

const char *lj_statics_validate(const lj_static_page *pages, int npages)
{
    int p, i;
    if (npages < 0 || npages > LJ_STATIC_PAGES_MAX || (npages && !pages)) return "page count not 0..64";
    for (p = 0; p < npages; p++) {
        if (!memchr(pages[p].name, 0, sizeof pages[p].name)) return "page name not NUL-terminated";
        if (pages[p].n > LJ_STATICS2_SLOTS_PER_PAGE) return "slots per page over 20";
        for (i = 0; i < LJ_STATICS2_SLOTS_PER_PAGE; i++) {
            const lj_static_slot *s = &pages[p].slot[i];
            if (!memchr(s->name, 0, sizeof s->name) || !memchr(s->type, 0, sizeof s->type)) return "slot name or type not NUL-terminated";
            if (s->activation > LJ_ACT_FLASH) return "activation not toggle/flash";
            if (s->replace > LJ_REP_SOLO_ALL) return "replace mode unknown";
            if (s->fade_ms > LJ_TIME_MAX_MS) return "fade time over 60000 ms";
            if (s->level_pct > 100) return "level over 100 %";
            if (s->kind > LJ_STATIC_LINK) return "slot kind unknown";
            if (s->kind == LJ_STATIC_RECORDED && (!s->scene || s->scene->fade_ms > LJ_TIME_MAX_MS)) return "recorded slot without a scene";
            if (s->kind == LJ_STATIC_LINK && !s->link_seq) return "link slot without a sequence";
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

/* ---- W2 contract stubs (lj_statics.h additions); the model track replaces the bodies ---- */
int lj_statics_groups_load(const char *path, lj_static_groups *out, char *err, size_t cap) { (void)path; (void)err; (void)cap; memset(out, 0, sizeof *out); return -1; }
int lj_statics_groups_save(const char *path, const lj_static_page *pages, int npages, const lj_static_groups *groups) { (void)path; (void)pages; (void)npages; (void)groups; return -1; }
int lj_static_slot_link(lj_static_slot *slot, uint32_t seq, uint16_t scene) { (void)slot; (void)seq; (void)scene; return -1; }
int lj_static_global_index(int page, int slot)
{
    if (page < 1 || page > LJ_STATIC_PAGES_MAX || slot < 1 || slot > LJ_STATICS2_SLOTS_PER_PAGE) return -1;
    return (page - 1) * LJ_STATICS2_SLOTS_PER_PAGE + (slot - 1);
}
int lj_static_scope_hits(const lj_static_page *pages, int npages, int actor, int other) { (void)pages; (void)npages; (void)actor; (void)other; return 0; }
