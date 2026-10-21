/* lj_main_window.h - the code-built main window (design 7.7, [07 §4]). W0 STUB:
 * a 799x606 clBtnFace client in a sizeable frame (Constraints 300x100, the
 * TLJMAINFORM facts) with the generated TLJMainForm menu bar when the tables
 * carry that form, else an empty bar. Toolbars, status bar and the fixture
 * desktop are W1. SDL-free. */
#ifndef LJ_DESKTOP_MAIN_WINDOW_H
#define LJ_DESKTOP_MAIN_WINDOW_H

#include "ui/lj_window.h"

lj_window *lj_main_window_create(const lj_binding *bindings);   /* desc events: {"OnClose", "FormClose"}; NULL + log on failure */
void       lj_main_window_destroy(lj_window *win);               /* frees the window and its code-built desc; NULL is a no-op */

#endif /* LJ_DESKTOP_MAIN_WINDOW_H */
