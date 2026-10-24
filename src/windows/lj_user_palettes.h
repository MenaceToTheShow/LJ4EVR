/* lj_user_palettes.h - the user palettes (04 s9, design 6.5; userpalettedialog.gif, usercolorpalette.gif): one
 * TPresetForm per control kind (Intensity, Color, Gobo, Beam, Effects, Levels/Special; not movement, not lamp),
 * list No / Name with "N Entries", the Master / Selected / All Fixtures filter, Save / Assign / Clear, popup
 * "Delete Selected Palette(s)". An entry stores value + fade state of the kind's channels of the filtered
 * fixtures (address-keyed, so values only reach fixtures that were recorded = same profile, 15/00 s5); Assign
 * (or double-click) writes its Snap/Fade channels into the current scene for the filtered fixtures; Clear turns
 * Off the filtered fixtures' channels whose value equals the entry. Save opens TSavePresetForm2 ("Save <Kind>
 * Palette Entry", exe 0x1CCE50..) with Save (overwrite the highlighted entry) / Save as New. Files:
 * <library>\Palettes\<Kind>.Pre (model/lj_palettes.h). */
#ifndef LJ_WINDOWS_USER_PALETTES_H
#define LJ_WINDOWS_USER_PALETTES_H

#include "windows/lj_windows.h"

lj_window *lj_user_palettes_open(void);                     /* the last kind used (Color by default) */
lj_window *lj_user_palettes_open_kind(enum lj_ctl_kind kind);   /* NULL for a kind without palettes */
void       lj_user_palettes_close(void);
lj_window *lj_user_palettes_window(void);
lj_window *lj_user_palettes_save_window(void);              /* the open TSavePresetForm2 or NULL (the UI script resolves it by class) */
void       lj_user_palettes_refresh(void);

int  lj_user_palettes_count(void);                          /* entries of the open kind */
int  lj_user_palettes_record(int index, const char *name, int filter);   /* index -1 = Save as New, else overwrite; filter 0 master 1 selected 2 all; the entry index or -1 */
int  lj_user_palettes_assign(int index, int filter);        /* channels written, -1 */
int  lj_user_palettes_clear(int index, int filter);         /* channels turned Off, -1 */
int  lj_user_palettes_delete(int index);                    /* 0 / -1 */
void lj_user_palettes_bind_fns(void);                       /* LJ_FN_TOGGLE_<kind>_PALETTE */

#endif /* LJ_WINDOWS_USER_PALETTES_H */
