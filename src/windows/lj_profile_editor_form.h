/* lj_profile_editor_form.h - Generic Fixture Profile editor, Customize Fixture, Channel Configuration (16 s4, 02 s4.3, ME-32). W3 contract (docs/w3-contracts.md); owner: the profile-editor track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_PROFILE_EDITOR_FORM_H
#define LJ_WINDOWS_PROFILE_EDITOR_FORM_H

#include "fixlib/lj_profile_editor.h"
#include "ui/lj_window.h"

/* LJ_WIN_PROFILE_EDITOR "profile_editor": TScanner2DefsForm "Generic Fixture Profile" 575x391, PC1 with the 11 tabs of fixlib/lj_profile_editor.h (+ TPaletteValueTypeForm for the palette type codes, TBitMapPickerForm2 "Select Bitmap", TQuickRGBDefForm "RGB Value", TFixtureSpecialForm, TFixtureNotesForm, TFixtureInfoForm) */
lj_window *lj_profile_editor_open(void);
void       lj_profile_editor_close(void);
lj_window *lj_profile_editor_get(void);
void       lj_profile_editor_refresh(void);

/* LJ_WIN_CUSTOMIZE_FIXTURE "customize_fixture": TFixtureSetupForm "Customize Fixture": tabs Images (Slot# | RGB | Bitmap) and "Offline Vizualizer" (sic, D13); Gobo/Effects Bitmaps, Color Bitmaps, Color RGB Values, Clear Entry, Copy from Source Fixture (+ TCopySetupForm "Copy Palettes from") */
lj_window *lj_customize_fixture_open(void);
void       lj_customize_fixture_close(void);
lj_window *lj_customize_fixture_get(void);
void       lj_customize_fixture_refresh(void);

/* LJ_WIN_CHANNEL_CONFIG "channel_config": TChannelDefForm "Channel Configuration" 356x207: Ch# | Caption | GM Control, Enable / Disable Grand Master Control (ME-32) */
lj_window *lj_channel_config_open(void);
void       lj_channel_config_close(void);
lj_window *lj_channel_config_get(void);
void       lj_channel_config_refresh(void);

/* profile-editor track additions: the seams the tests, the UI script and the desktop popup use */
int         lj_profile_editor_commit(int save_as);                 /* Save Profile (0) / Save profile as (1): validate, UserFixtureDef\<name>.GS2, LJ_ACT_GS2_RELOAD; 0 / -1 (message shown) */
lj_profile_edit  *lj_profile_editor_work(void);                     /* the open editor's working copy, NULL when closed */
const lj_palette *lj_customize_fixture_work(void);                  /* the open Customize Fixture's overrides [6], NULL when closed */
const char *lj_customize_popup_caption(int fixture);               /* "Customize Fixture" (built-ins) or "Define Fixture Profile" (user-definable, generic dimmer / switch) [LORD_FIDELITY-26] */
void lj_profile_editor_form_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

#endif /* LJ_WINDOWS_PROFILE_EDITOR_FORM_H */
