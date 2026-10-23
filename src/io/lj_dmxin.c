/* lj_dmxin.c - DMX-in input seam, .vtr tables, DMX-in functions (io/lj_dmxin.h). W5 CONTRACT STUB: the dmxin track
 * replaces every body (docs/w5-contracts.md seam 2). Until then nothing opens, every packet is discarded and the frame
 * stays zero. The words table and the defaults are contract data and stay. */
#include <string.h>
#include "io/lj_dmxin.h"
#include "core/lj_str.h"

#define LJ_DMXIN_FN_WORD(id, word) word,
const char *const lj_dmxin_fn_words[LJ_DIF_COUNT] = { LJ_DMXIN_FN_LIST(LJ_DMXIN_FN_WORD) };
#undef LJ_DMXIN_FN_WORD

static lj_dmxin_frame lj_di_frame;

int  lj_dmxin_open(const lj_dmxin_cfg *cfg, const uint8_t own_cid[16]) { (void)cfg; (void)own_cid; return 0; }
void lj_dmxin_close(void) {}
int  lj_dmxin_poll(uint64_t now_us) { (void)now_us; return 0; }
int  lj_dmxin_rx(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us) { (void)kind; (void)pkt; (void)n; (void)src_ip; (void)now_us; lj_di_frame.discarded++; return -1; }
void lj_dmxin_post(int kind, const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us) { (void)kind; (void)pkt; (void)n; (void)src_ip; (void)now_us; }
const lj_dmxin_frame *lj_dmxin_frame_get(void) { return &lj_di_frame; }
void lj_dmxin_emulate(int ch, int level, uint64_t now_us) { (void)ch; (void)level; (void)now_us; }
void lj_dmxin_set_table(int ch, const uint8_t *map256) { (void)ch; (void)map256; }
int  lj_sacn_rx_parse(const uint8_t *pkt, size_t n, lj_sacn_rx *out) { (void)pkt; (void)n; (void)out; return -1; }

void lj_dmxin_cfg_default(lj_dmxin_cfg *cfg) { if (!cfg) return; memset(cfg, 0, sizeof *cfg); cfg->source = LJ_DMXIN_SACN; cfg->universe = 1; cfg->loss = LJ_DMXIN_LOSS_HOLD; }
void lj_dmxin_cfg_load(lj_ini *hardware_ini, lj_dmxin_cfg *cfg) { (void)hardware_ini; lj_dmxin_cfg_default(cfg); }
void lj_dmxin_cfg_save(lj_ini *hardware_ini, const lj_dmxin_cfg *cfg) { (void)hardware_ini; (void)cfg; }
const char *lj_dmxin_cfg_validate(const lj_dmxin_cfg *cfg, const uint16_t out_universes[LJ_LINKS]) { (void)cfg; (void)out_universes; return NULL; }

void lj_vtr_identity(lj_vtr *t) { int i; if (!t) return; memset(t, 0, sizeof *t); for (i = 0; i < 256; i++) t->map[i] = (uint8_t)i; }
void lj_vtr_linear(lj_vtr *t, int in_lo, int in_hi, int out_lo, int out_hi) { (void)t; (void)in_lo; (void)in_hi; (void)out_lo; (void)out_hi; }
int  lj_vtr_parse(const char *text, size_t n, lj_vtr *out, char *err, size_t cap) { (void)text; (void)n; (void)out; if (err && cap) lj_str_copy(err, cap, "pending (dmxin track)"); return -1; }
int  lj_vtr_load(const char *path, lj_vtr *out, char *err, size_t cap) { (void)path; return lj_vtr_parse(NULL, 0, out, err, cap); }
int  lj_vtr_save(const char *path, const lj_vtr *in) { (void)path; (void)in; return -1; }
int  lj_vtr_file_name(char *dst, size_t cap, const lj_vtr *t) { (void)dst; (void)cap; (void)t; return -1; }

void lj_dmxin_map_init(lj_dmxin_map *m) { if (m) memset(m, 0, sizeof *m); }
int  lj_dmxin_map_load(const char *path, lj_dmxin_map *out, char *err, size_t cap) { (void)path; (void)err; (void)cap; lj_dmxin_map_init(out); return 0; }
int  lj_dmxin_map_save(const char *path, const lj_dmxin_map *in) { (void)path; (void)in; return -1; }
int  lj_dmxin_functions(const lj_dmxin_map *m, const uint8_t in[LJ_LINK_CH], int functions_off) { (void)m; (void)in; (void)functions_off; return 0; }
int  lj_dmxin_remote_cue(uint32_t *cue) { (void)cue; return 0; }
