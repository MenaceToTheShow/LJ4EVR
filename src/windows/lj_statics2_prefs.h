/* lj_statics2_prefs.h - Statics Preferences (TStatics2PrefsForm "Statics Preferences" 481x299, four tabs [14 sB.4]), its
 * page name box (TGetStaticsPageNameForm "Statics Page", 64 chars) and Static entry properties (TStaticEntryPropertiesForm
 * 540x300 [14 sB.5]) (ME-91).
 *  - Preferences: View Slot# / Modes / Levels, Visible Slots (the DFM caption "(1- " verbatim; 1..64 on Apply), Set Default; Pages: Multiline, Buton [sic] Width /
 *    Button Height with Auto. These apply on OK / Apply through lj_statics2_view_set (Main.Ini [STATICS2], the exe's keys).
 *  - Pages: Add / Insert / Edit name (the name box) / Up / Down / Delete act on show->pages at once (as the cue page
 *    dialogs do; Cancel does not undo them). Insert, Delete, Up and Down move pages inside the fixed array, so every
 *    static is released first in the same critical section (model/lj_statics.h); Add moves nothing.
 *  - Slots: the visible slots of the current page with their Transparent flag and slot type (63 chars).
 *  - Groups: the 64 group names (ctx->static_groups, loaded from Statics.dat on first use); Delete gives a group its
 *    default "Group %d" back and takes it off every slot.
 *  - Properties: the slot of the last Statics 2 popup on the current page: Name, Activation, Replace (the TAction
 *    captions), Fade Time (seconds, the host decimal separator, D6), the Groups check list (Set All / Clear All,
 *    "%d Groups"); OK / Apply write the slot under lj_lock and save Statics.dat. */
#ifndef LJ_WINDOWS_STATICS2_PREFS_H
#define LJ_WINDOWS_STATICS2_PREFS_H

#include "ui/lj_window.h"

lj_window *lj_statics2_prefs_open(void);
void       lj_statics2_prefs_close(void);
lj_window *lj_statics2_prefs_window(void);
void       lj_statics2_prefs_refresh(void);              /* the pending close */
lj_window *lj_statics2_page_name_window(void);           /* the open TGetStaticsPageNameForm, NULL when none */

lj_window *lj_static_props_open(void);                   /* the last popup's slot (slot 1 before any) on the current page; NULL + log without a page */
void       lj_static_props_close(void);
lj_window *lj_static_props_window(void);
void       lj_static_props_refresh(void);

#endif /* LJ_WINDOWS_STATICS2_PREFS_H */
