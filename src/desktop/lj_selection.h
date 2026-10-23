/* lj_selection.h - fixture selection state (design 7.8, [01 s3.2-3.3], F12): master /
 * slaves, Inclusive-Exclusive, solo, serial. Public API: lj_sel_* in lj_desktop.h;
 * this header adds what the icon area and popups need. Selection.Dat is W2. */
#ifndef LJ_DESKTOP_SELECTION_H
#define LJ_DESKTOP_SELECTION_H

#include "desktop/lj_desktop.h"

int  lj_sel_same_type(int a, int b);      /* same kind + profile id (or .GS2 file) [01 s3.6 "of this type"] */
void lj_sel_reset(void);                  /* after a Config.dat reload: nothing selected, Inclusive, solo off */

/* ---- W4 dlg-desktop: Next/Previous Fixture Selection options and the custom order [01 s3.3], the named custom
 * selections of Fan Out (TSaveSelectionForm, 17 s5.2). All in the library's Main.Ini, ours (PROPOSAL: LJ2's keys are not
 * in the exe's literal pool): [NEXTPREV] Enabled SameType Tabs Order Custom=<n,n,0,n> (fixture numbers, 0 = Selection
 * Break) AutoAddMaster; [CUSTOMSEL] Count, Name<k>, Fix<k>=<n,n,..> k = 1..Count. */
enum { LJ_NP_TABS_ANY = 0, LJ_NP_TABS_SAME, LJ_NP_TABS_CURRENT };          /* rgTabs: Any Tab / Same Tab as Master / Current Tab */
enum { LJ_SEL_BREAK = -1, LJ_CUSTOM_SEL_MAX = 99, LJ_CUSTOM_SEL_NAME = 26 }; /* eName MaxLength 25 */
typedef struct { int enabled, same_type, tabs, custom; } lj_nextprev_opts;  /* cbEnabled, cbSelect, rgTabs, rgOrder (0 Fixture Number, 1 Custom Order ...) */
void lj_sel_nextprev_get(lj_nextprev_opts *o);
void lj_sel_nextprev_set(const lj_nextprev_opts *o);   /* ctx->show_nextprev follows Enabled (the toolbar buttons); saved */
int  lj_sel_custom_order(int *out, int cap);           /* fixture indices, LJ_SEL_BREAK rows; returns the count */
void lj_sel_custom_order_set(const int *e, int n);     /* saved */
void lj_sel_nextprev_load(void);                       /* Main.Ini -> the options and the order (the app context's Main.Ini) */
int         lj_custom_sel_count(void);
const char *lj_custom_sel_name(int no);                /* 1-based; "" out of range */
int         lj_custom_sel_get(int no, int *out, int cap);   /* fixture indices; the count, -1 out of range */
int         lj_custom_sel_save(int no, const char *name, const int *fix, int n);   /* no 0 = Save New; returns the number, -1 (full, empty name) */
void        lj_custom_sel_clear_all(void);             /* "Delete All Custom Selections ?" */

#endif /* LJ_DESKTOP_SELECTION_H */
