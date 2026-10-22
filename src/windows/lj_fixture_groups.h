/* lj_fixture_groups.h - Fixture Groups (code-built TGroupForm, 01 s3.4, 04 s2.3; fixturegroupwindow.gif,
 * fixturegrouppopup.gif): a tree of "(System) <profile>" groups (one per fixture type, Preferences > Show System
 * Groups) and the 99 user groups "(n) name" (GroupInfo.dat), each expandable to its fixtures (profile or user
 * names, Preferences > Show User Names). Choosing a group line selects its members through the desktop selection
 * (Mode:Inclusive / Mode:Exclusive = lj_sel_mode, Filter:All / Odd / Even by fixture number); Add Group opens
 * TAddGroupForm; the popup adds / removes the selected fixtures, clears, deletes, renames. Hotkeys "Fixture
 * group 1..20" select user groups, "Toggle Group View" toggles the window. forms.ini [GROUPS] (220x250). */
#ifndef LJ_WINDOWS_FIXTURE_GROUPS_H
#define LJ_WINDOWS_FIXTURE_GROUPS_H

#include "windows/lj_windows.h"

lj_window *lj_fixture_groups_open(void);
void       lj_fixture_groups_close(void);
lj_window *lj_fixture_groups_window(void);
void       lj_fixture_groups_refresh(void);              /* rebuilds the tree when the show's groups or fixtures changed */

int  lj_fixture_groups_add(const char *name);            /* Add Group: the first free user group (1..99) named; its number or -1 */
int  lj_fixture_groups_assign(int group, int on);        /* user group 1..99: the selected fixtures join (on) / leave it; member count or -1 */
int  lj_fixture_groups_select(int group);                /* user group 1..99 -> the desktop selection (mode + filter); members selected or -1 */
int  lj_fixture_groups_popup(const char *item, int group);   /* a popup item by name (P_Add P_Remove P_RemoveAll P_RemoveFromAll P_Delete P_Rename) on a user group; 0 / -1 */
void lj_fixture_groups_set_filter(int filter);           /* 0 All, 1 Odd, 2 Even (Filter:All button) */
void lj_fixture_groups_bind_fns(void);                   /* LJ_FN_FIXTURE_GROUP_1..20, LJ_FN_TOGGLE_GROUP_VIEW */

#endif /* LJ_WINDOWS_FIXTURE_GROUPS_H */
