/* lj_user_libraries.h - System > User Libraries (TUserLibForm "User Libraries", [01 s8]) and its
 * name box (TGetUserLibNameForm "Enter User Library Name" 345x89): the folders under
 * <userdir>User\ (Library Name / Path / Date), Select User Library (restart prompt, then
 * LJ_ACT_SELECT_LIBRARY with the row: the app restarts into it), Create New Library (optionally
 * "Copy files from current library"), Delete Library (prompt), Rename Library, Backup Library /
 * Restore Library (LJ_WIN_BACKUP / LJ_WIN_RESTORE), Close; footer `Current User Library is
 * "<name>"`. Default and the open library cannot be deleted or renamed (the two buttons grey). */
#ifndef LJ_WINDOWS_USER_LIBRARIES_H
#define LJ_WINDOWS_USER_LIBRARIES_H

#include "windows/lj_windows.h"

#define LJ_USERLIB_RESTART1 "Re-start LightJockey in new user library ?"    /* exe 0x2307FC */
#define LJ_USERLIB_RESTART2 "(There will be no prompt for unsaved data)"

lj_window *lj_user_libraries_open(void);     /* LJ_WIN_USER_LIBRARIES */
void       lj_user_libraries_close(void);
lj_window *lj_user_libraries_window(void);
void       lj_user_libraries_refresh(void);  /* the pending closes (both windows) */
int        lj_user_libraries_count(void);
const char *lj_user_libraries_name(int row); /* NULL out of range; LJ_ACT_SELECT_LIBRARY's arg indexes this list */
int        lj_user_libraries_select(int row);   /* the LV1 row the buttons act on; -1 out of range */
const char *lj_user_libraries_selected(void);   /* the picked library while the window is open (Backup Library backs it up), else NULL */

lj_window *lj_user_lib_name_open(void);      /* LJ_WIN_USER_LIB_NAME: Create New Library mode */
void       lj_user_lib_name_close(void);
lj_window *lj_user_lib_name_window(void);

#endif /* LJ_WINDOWS_USER_LIBRARIES_H */
