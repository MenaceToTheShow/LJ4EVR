/* lj_hardware_setup.h - Hardware Setup (THARDWARESETUPFORM2, [05 s1]) with the DMX
 * Hardware page turned into the sACN (E1.31) page (design 8.1 "Settings UI", 08 s7):
 * NIC combo (lj_net_list_ipv4), link 1-4 -> universe, priority, unicast list (up to 8
 * per link), preview, source name, rate unlock, status line (packets/s per universe,
 * last send error, NIC in use). OK = Apply: lj_sacn_cfg_validate -> ctx->sacn ->
 * LJ_CMD_SACN_CFG (ME-23) -> Hardware.Ini [SACN]. Licence lines gone (D3); the
 * Martin card panels, Detect/Clear DMX Hardware and DMX-in are hidden/greyed. */
#ifndef LJ_WINDOWS_HARDWARE_SETUP_H
#define LJ_WINDOWS_HARDWARE_SETUP_H

#include <stdint.h>
#include "windows/lj_windows.h"

lj_window  *lj_hardware_setup_open(void);
void        lj_hardware_setup_close(void);
lj_window  *lj_hardware_setup_window(void);
void        lj_hardware_setup_refresh(void);                                   /* repaints the status line from the app's engine-status copy (ctx->view) */
const char *lj_hardware_setup_apply(void);                                     /* reads the page; NULL = applied, else the rejection text (also shown in a message box) */
int         lj_hardware_setup_set_priority(int priority);                      /* the page's priority field (tests / UI script); 0 / -1 */

#endif /* LJ_WINDOWS_HARDWARE_SETUP_H */
