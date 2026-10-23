/* lj_ljb.c - the .LJB importer (model/lj_ljb.h). W5 CONTRACT STUB: the ljb track replaces every body
 * (docs/w5-contracts.md seam 8). Until then every container is refused and nothing is written. */
#include <string.h>
#include "model/lj_ljb.h"
#include "core/lj_str.h"

static int lj_ljb_pending(char *err, size_t cap) { if (err && cap) lj_str_copy(err, cap, "pending (ljb track)"); return -1; }

int  lj_ljb_open(lj_ljb *out, const uint8_t *data, size_t len, char *err, size_t cap) { if (out) memset(out, 0, sizeof *out); (void)data; (void)len; return lj_ljb_pending(err, cap); }
int  lj_ljb_extract(const lj_ljb *b, uint32_t i, uint8_t **out, size_t *n, char *err, size_t cap) { (void)b; (void)i; if (out) *out = NULL; if (n) *n = 0; return lj_ljb_pending(err, cap); }
int  lj_ljb_path_ok(const char *folder, const char *name) { (void)folder; (void)name; return 0; }
void lj_ljb_free(lj_ljb *b) { if (b) memset(b, 0, sizeof *b); }
int  lj_ljb_import(const char *ljb_path, const char *new_library_dir, lj_ljb_report *rep, char *err, size_t cap) { (void)ljb_path; (void)new_library_dir; if (rep) memset(rep, 0, sizeof *rep); return lj_ljb_pending(err, cap); }
