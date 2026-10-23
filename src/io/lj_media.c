/* lj_media.c - the LJ MediaPlayer state machine and clip clock (io/lj_media.h). W5 CONTRACT STUB: the media track
 * replaces every body (docs/w5-contracts.md seam 7). Until then every command is refused and the clock reads 0. */
#include <string.h>
#include "io/lj_media.h"
#include "core/lj_str.h"

void     lj_media_init(lj_media *m) { if (m) memset(m, 0, sizeof *m); }
int      lj_media_cmd(lj_media *m, const lj_media_host *h, int cl_cmd, const char *param, uint32_t param_ms, char *err, size_t cap)
{ (void)m; (void)h; (void)cl_cmd; (void)param; (void)param_ms; if (err && cap) lj_str_copy(err, cap, "pending (media track)"); return -1; }
void     lj_media_step(lj_media *m, const lj_media_host *h) { (void)m; (void)h; }
uint32_t lj_media_time_cs(const lj_media *m) { return m ? m->cs : 0; }
int      lj_media_format_tc(char *dst, size_t cap, uint32_t cs) { (void)cs; if (dst && cap) dst[0] = 0; return -1; }
int      lj_media_check_file(const char *path, uint64_t size, char *err, size_t cap) { (void)path; (void)size; if (err && cap) lj_str_copy(err, cap, "pending (media track)"); return -1; }
