/* lj_extapp.c - External Application string output (io/lj_extapp.h). W5 CONTRACT STUB: the extapp track replaces every
 * body (docs/w5-contracts.md seam 6). Until then no link opens and every string is refused. */
#include <string.h>
#include "io/lj_extapp.h"
#include "core/lj_str.h"

int  lj_extapp_encode(const char *cmd, uint8_t out[LJ_EXTAPP_MAX_BYTES], size_t *n, char *err, size_t cap)
{ (void)cmd; (void)out; if (n) *n = 0; if (err && cap) lj_str_copy(err, cap, "pending (extapp track)"); return -1; }
const char *lj_extapp_validate(const char *cmd) { (void)cmd; return "pending (extapp track)"; }

void lj_extapp_cfg_default(lj_extapp_cfg *c) { if (c) memset(c, 0, sizeof *c); }
void lj_extapp_cfg_load(lj_ini *hardware_ini, lj_extapp_cfg *c) { (void)hardware_ini; lj_extapp_cfg_default(c); }
void lj_extapp_cfg_save(lj_ini *hardware_ini, const lj_extapp_cfg *c) { (void)hardware_ini; (void)c; }
int  lj_extapp_open(const lj_extapp_cfg *c) { (void)c; return 0; }
void lj_extapp_close(void) {}
void lj_extapp_step(const uint8_t ext_fixture[LJ_SCENE_EXT], const char *ext, uint64_t now_us) { (void)ext_fixture; (void)ext; (void)now_us; }
void lj_extapp_stats_get(lj_extapp_stats *out) { if (out) memset(out, 0, sizeof *out); }
