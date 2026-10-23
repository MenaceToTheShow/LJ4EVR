/* lj_fixture_config.h - Fixture Configuration (TLINKCONFIGFORM3 790x590, [02 s2-s4],
 * design 10 W1 acceptance 2): the Available Profiles tree (the 201 exe rows grouped
 * by manufacturer, a mode folder when a name has several modes, "Other" for rows
 * without a manufacturer), the configured list, the Fixture Settings panel (link 1-4,
 * address 1-512 mirrored on the DIP switch, Calculate Address, invert/swap, ignore
 * blackout/GM, link strobe/GM, user name), remove with the verbatim prompt, Save
 * through the model (Config.dat) + lj_engine_rebind + the Unassigned list.
 * Greyed rows (no profile bound) report "Not available in LJ4EVR" (ME-29 wording);
 * Print Configuration greyed (ME-31: no printing path), Export to text file (ME-92). */
#ifndef LJ_WINDOWS_FIXTURE_CONFIG_H
#define LJ_WINDOWS_FIXTURE_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include "windows/lj_windows.h"

lj_window *lj_fixture_config_open(void);                       /* creates + binds, snapshot of show->fixture[] for "skip changes"; NULL + log */
void       lj_fixture_config_close(void);
lj_window *lj_fixture_config_window(void);
void       lj_fixture_config_refresh(void);                    /* rebuilds the configured list, the labels and the settings panel */

/* the tree, for the UI script driver and the test */
int         lj_fixture_config_tree_count(void);                /* items = folders + leaves */
int         lj_fixture_config_tree_leaves(void);               /* leaves = exe rows (201 when fixlib is complete) */
const char *lj_fixture_config_tree_item(int i);                /* '\t' per level, then the caption */
int         lj_fixture_config_tree_id(int i);                  /* profile id of a leaf, -1 for a folder / out of range */
int         lj_fixture_config_tree_find(const char *manufacturer, const char *name, const char *mode);   /* leaf item index or -1 (mode NULL = single-mode leaf) */
int         lj_fixture_config_tree_available(int i);           /* 1 = a profile is bound (lj_fixlib_by_id), 0 = greyed "Not available in LJ4EVR" */

/* the panel */
int  lj_fixture_config_select(int fixture);                    /* configured-list selection; -1 = none; 0 / -1 */
int  lj_fixture_config_add(uint16_t profile_id, int link, int address);   /* first free row, user name = the exe tag; returns the fixture index or -1 */
int  lj_fixture_config_set_address(int fixture, int link, int address);   /* 1-4, 0 (none) ..512; mirrors the DIP switch; 0 / -1 */
int  lj_fixture_config_calculate(int fixture);                 /* first free range over links 1-4 (Calculate Address, [02 s2.5] suggestion only); 0 / -1 none free */
int  lj_fixture_config_dip_text(int address, char *out, size_t cap);     /* "1000100000" for 17: switch i (left to right) = bit i-1, '1' = thrown down */
int  lj_fixture_config_save(void);                             /* Save: TConfigWarnForm when lj_config_dlg_warnings finds any (F41), else lj_fixture_config_save_anyway; 0 written, 1 a dialog / prompt is open, -1 */
int  lj_fixture_config_modified(void);

/* Export to text file (ME-31, ME-92): the exe's format, CRLF lines; the window is TEXPORTCONFIGFORM "Configuration Export" */
int        lj_fixture_config_export_text(char *out, size_t cap);   /* 0 / -1 (cap too small) */
int        lj_fixture_config_export_save(const char *path);        /* the text into a file; 0 / -1 */
lj_window *lj_fixture_config_export_window(void);                  /* the open export window, NULL when none */

/* dlg-config track (W4): the CONFIG dialogs (lj_config_dialogs.c) act through these */
int  lj_fixture_config_save_anyway(void);                      /* TConfigWarnForm Save  Anyway: the Q9 prompts, default DMX, Config.dat; 0 done, 1 a prompt is open, -1 */
int  lj_fixture_config_selected(void);                         /* the configured-list selection, -1 none / closed */
int  lj_fixture_config_saved(int fixture);                     /* 1 when the row has an address and equals its last saved state (Copy From / Clear gate [02 s2.9]) */
void lj_fixture_config_mark_saved(void);                       /* the open dialog's snapshot := show->fixture[] (after Config.dat was written) */
int  lj_fixture_config_put(int fixture, uint16_t profile_id);  /* Add Fixture(s): the profile into that row, link 1, no address, user name = tag; 0 / -1 */
int  lj_fixture_config_tree_select(int profile_id);            /* Find profile: expand to the leaf, select it, show its notes; 0 / -1 */

#endif /* LJ_WINDOWS_FIXTURE_CONFIG_H */
