/* lj_sysmem.h - the free-memory figure behind the status-bar memory gauge (ME-87): the app
 * queries the OS, the desktop paints it (lj_desktop_status_set LJ_SB_MEMORY "79%" + the band
 * flag). The one app file with OS headers (tools/test_layering.py OS_ALLOW). */
#ifndef LJ_APP_SYSMEM_H
#define LJ_APP_SYSMEM_H

#include <stdint.h>

int lj_sysmem_query(uint64_t *avail_bytes, uint64_t *total_bytes);   /* physical memory free / installed; 0 / -1 (no figure on this OS) */

#endif /* LJ_APP_SYSMEM_H */
