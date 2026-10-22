/* lj_cuepage.c - W2 contract stub (lj_cuepage.h); the model track replaces the bodies. */
#include <string.h>
#include "model/lj_cuepage.h"

void lj_cuepages_init(lj_cuepages *p) { memset(p, 0, sizeof *p); p->cursor = -1; }
int  lj_cuepages_load(const char *path, lj_cuepages *out, char *err, size_t cap) { (void)path; (void)err; (void)cap; lj_cuepages_init(out); return -1; }
int  lj_cuepages_save(const char *path, const lj_cuepages *in) { (void)path; (void)in; return -1; }
const char *lj_cuepages_validate(const lj_cuepages *in) { (void)in; return "lj_cuepages_validate: W2 model track"; }
int  lj_cuepages_add(lj_cuepages *p, const char *name) { (void)p; (void)name; return -1; }
int  lj_cuepages_insert(lj_cuepages *p, int before, const char *name) { (void)p; (void)before; (void)name; return -1; }
int  lj_cuepages_delete(lj_cuepages *p, int index) { (void)p; (void)index; return -1; }
int  lj_cuepages_rename(lj_cuepages *p, int index, const char *name) { (void)p; (void)index; (void)name; return -1; }
int  lj_cuepages_clear(lj_cuepages *p, int index) { (void)p; (void)index; return -1; }
int  lj_cuepages_move(lj_cuepages *p, int from, int to) { (void)p; (void)from; (void)to; return -1; }
int  lj_cuepages_add_cue(lj_cuepages *p, int index, uint32_t cue, int at) { (void)p; (void)index; (void)cue; (void)at; return -1; }
int  lj_cuepages_remove_cue(lj_cuepages *p, int index, int at) { (void)p; (void)index; (void)at; return -1; }
int  lj_cuepages_step(lj_cuepages *p, int delta, int circular) { (void)p; (void)delta; (void)circular; return -1; }
uint32_t lj_cuepages_step_cue(lj_cuepages *p, int delta) { (void)p; (void)delta; return 0; }
