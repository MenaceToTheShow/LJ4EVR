/* lj_hotkeys.h - keyboard hotkeys (design 7.9, [01 s7], [05 s11.1], F54):
 * (key, shift, ctrl, scroll-lock) -> function, with LJ2's rules: only when an
 * LJ4EVR window has OS focus; Shift and Ctrl distinct, Alt never (Windows menus);
 * F1-F12 (+Shift/Ctrl) always work; "Disable Hotkeys in Dialogs" (default on)
 * suppresses the rest while a dialog needing typing is active; a per-entry
 * Scroll Lock attribute limits the key to Scroll Lock on; flash functions act
 * on press (edge 1) and release (edge 0). Up to 8 saved setups appear under
 * Setup > Hotkeys.
 *
 * .Hot files are ours (LJ2's binary layout is undecoded), design 5.2 grammar:
 *   LJ4 hot 1
 *   name="Setup 1"
 *   enabled=1 dialogs=1
 *   key vk=66 shift=1 ctrl=0 scrl=0 fn=LJ_FN_BLACKOUT_RESTORE
 *   key vk=112 shift=0 ctrl=0 scrl=0 fn=LJ_FN_PROG_CUE num=12
 * vk = the Windows virtual-key code (lj_key_ev.key, the numbers LJ2's tables
 * carry: VK_F1 = 0x70 ...); fn = the lj_fn_table name. Setups live at
 * <userdir>Hotkeys\SetupN.Hot (PROPOSAL; installation-wide like Hardware.Ini). */
#ifndef LJ_IO_HOTKEYS_H
#define LJ_IO_HOTKEYS_H

#include <stddef.h>
#include <stdint.h>
#include "io/lj_actions.h"

enum { LJ_HOTKEYS_MAX = 512, LJ_HOTKEY_SETUPS_MAX = 8 };

typedef struct { uint16_t key; uint8_t shift, ctrl, scroll; uint16_t fn /*enum lj_fn*/; uint32_t prog_num; } lj_hotkey;   /* prog_num for the LJ_FN_PROG_* rows */
typedef struct { char name[64]; uint8_t enabled, disable_in_dialogs; int n; lj_hotkey e[LJ_HOTKEYS_MAX]; } lj_hotkeys;

void        lj_hotkeys_init(lj_hotkeys *h);                                      /* empty, enabled, disable_in_dialogs = 1 */
int         lj_hotkeys_load(const char *path, lj_hotkeys *out, char *err, size_t cap);   /* 0 / -1 */
int         lj_hotkeys_save(const char *path, const lj_hotkeys *in);
const char *lj_hotkeys_validate(const lj_hotkeys *in);                           /* NULL = valid: fn in range, no Alt, no duplicate combo, prog_num only on LJ_FN_PROG_* */
int         lj_hotkeys_setup_path(const char *userdir, int setup /* 1..8 */, char *out, size_t cap);
int         lj_hotkeys_assign(lj_hotkeys *h, const lj_hotkey *k);                /* replaces an entry with the same combo, else appends; -1 when full */
int         lj_hotkeys_clear_key(lj_hotkeys *h, uint16_t key, int shift, int ctrl);     /* "Clear Hotkey"; 0 / -1 when absent */
int         lj_hotkeys_clear_fn(lj_hotkeys *h, enum lj_fn fn, uint32_t prog_num);       /* "Clear Function(s)" */
const lj_hotkey *lj_hotkeys_find_fn(const lj_hotkeys *h, enum lj_fn fn, uint32_t prog_num);   /* "Find Hotkey"; NULL when unbound */

/* The dispatcher's question, answered from the rules above: the entry that fires
 * for this key event, or NULL. in_dialog = a dialog needing typing is active;
 * scroll_lock = the OS Scroll Lock state. F1-F12 bypass both gates. Alt keys never match. */
const lj_hotkey *lj_hotkeys_match(const lj_hotkeys *h, uint16_t key, int shift, int ctrl, int alt, int scroll_lock, int in_dialog);
int         lj_hotkeys_combo_text(const lj_hotkey *k, char *dst, size_t cap);    /* "[Shift] B", "[Control] F1" (13 s2.3 literals); 0 / -1 */
void        lj_hotkeys_default(lj_hotkeys *h);                                   /* Edit Hotkeys > Options > "Default Hotkeys", the shipped Setup 1 (PROPOSAL, see lj_hotkeys.c: LJ2's compiled-in set is undecoded) */

/* W2 midi track: the 8 setups (Setup > Hotkeys > Setup 1..8; Edit Hotkeys > Options > Save / Load / Default Hotkeys).
 * dir = the user library folder with its trailing separator (w2-contracts "Decisions": <library>Hotkeys/SetupN.Hot;
 * Main.Ini [HOTKEYS] File=SetupN.Hot names the current one). A missing file is not an error and logs nothing: setup 1
 * reads as lj_hotkeys_default, setups 2..8 as an empty enabled setup named "Setup N". A present bad file gives -1 + err and
 * the same fallback in *out. Edit Hotkeys edits *out with assign / clear_key / clear_fn and saves it with
 * lj_hotkeys_setup_path + lj_hotkeys_save. */
int         lj_hotkeys_setup_load(const char *dir, int setup /* 1..8 */, lj_hotkeys *out, char *err, size_t cap);
int         lj_hotkeys_setup_of_file(const char *file);                          /* "Setup3.Hot" (any case, any leading folder) -> 3; -1 otherwise */

#endif /* LJ_IO_HOTKEYS_H */
