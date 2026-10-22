/* lj_intensity_groups.h - Intensity Groups (TIntensityGroupForm "Intensity Groups", 01 s4, 04 s11): which of the
 * 8 sub-masters each fixture follows (lj_fixture.intensity_group, Config.dat sub=), the 5-character group names
 * (show->submaster_name) and the master mode per group (show->submaster_mode, the Master Intensity combo's four
 * items). Tab "Fixtures" lists every fixture with its group; tabs 1..8 list the members with the name and mode.
 * Assign Selected Fixtures / Assign Master Fixture / Remove Fixture(s) edit a working copy; Save writes it into
 * the show under lj_lock, rebinds the engine and saves Config.dat; closing with unsaved edits asks the exe's
 * "Changes were made to Intensity Groups / Do you wish to save changes ?" (0x1BB24C). */
#ifndef LJ_WINDOWS_INTENSITY_GROUPS_H
#define LJ_WINDOWS_INTENSITY_GROUPS_H

#include "windows/lj_windows.h"

lj_window *lj_intensity_groups_open(void);
void       lj_intensity_groups_close(void);
lj_window *lj_intensity_groups_window(void);
void       lj_intensity_groups_refresh(void);

int  lj_intensity_groups_tab(int tab);                    /* 0 = Fixtures, 1..8 = a group; 0 / -1 */
int  lj_intensity_groups_assign(int master_only);         /* the active group tab takes the selected fixtures (or the master); members or -1 */
int  lj_intensity_groups_remove(int fixture);             /* back to no group (0); 0 / -1 */
int  lj_intensity_groups_group_of(int fixture);           /* the working copy's group 0..8, -1 */
int  lj_intensity_groups_save(void);                      /* Save; 0 / -1 */

#endif /* LJ_WINDOWS_INTENSITY_GROUPS_H */
