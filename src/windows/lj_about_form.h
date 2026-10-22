/* lj_about_form.h - Help > About (TAboutForm2 "About LightJockey" 360x346; LORD_FIDELITY-16, ME-45,
 * ME-36/37). The DFM's caption and geometry stay; its contents are ours (D2: no Martin logo, no
 * credits text): Label1 the copyright line, Label2 "LJ4EVR <VERSION>" from the VERSION file
 * (generated version.h), RichEdit1 the lines of lj_win_about_text plus the trademark sentence,
 * Label3 / Label4 point at THIRD_PARTY_NOTICES.txt beside the binary. Any key or the frame closes. */
#ifndef LJ_WINDOWS_ABOUT_FORM_H
#define LJ_WINDOWS_ABOUT_FORM_H

#include "windows/lj_windows.h"

#define LJ_ABOUT_NOTICES "THIRD_PARTY_NOTICES.txt"

lj_window *lj_about_form_open(void);     /* LJ_WIN_ABOUT */
void       lj_about_form_close(void);
lj_window *lj_about_form_window(void);
void       lj_about_form_refresh(void);  /* the pending close */

#endif /* LJ_WINDOWS_ABOUT_FORM_H */
