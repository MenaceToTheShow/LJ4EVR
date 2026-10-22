/* lj_midi_in_form.h - the MIDI-IN window (LJ_WIN_MIDI_IN, code-built "TMidiInForm": LJ2's
 * LJ_MidiIn.exe forms are not in LightJockey.exe [05 s7.1]; layout from midiin.gif / midiinseq.gif,
 * reduced captures, so every size is a PROPOSAL rig item). Tabs Setup | Sequences | Cues | BGCues |
 * CueLists | Functions | MIDI Log; menu Options (Run Minimized, Stay on Top, Auto Save Changes) and
 * Help; Save / Close. It edits ctx->midi (io/lj_midi.h): the device (LJ_ACT_MIDI_OPEN_DEVICE),
 * Treat Velocity = 0 as note off and the four Release ... on Note Off boxes, per kind the enable box,
 * the MIDI Channel# (Ch 1..Ch 15, All = channel 16 as LJ2 ships it) and the 128-note grid; Learn
 * (lj_midi_learn_begin, polled on the 100 ms refresh) selects the next note played and, when the
 * number field holds a record number (a function on the Functions tab), binds it at once;
 * "<- Set" binds the selected note. Save and Close with Auto Save run LJ_ACT_MIDI_SAVE. */
#ifndef LJ_WINDOWS_MIDI_IN_FORM_H
#define LJ_WINDOWS_MIDI_IN_FORM_H

#include <stdint.h>
#include "io/lj_midi.h"
#include "windows/lj_windows.h"

lj_window *lj_midi_in_form_open(void);
void       lj_midi_in_form_close(void);
lj_window *lj_midi_in_form_window(void);
void       lj_midi_in_form_refresh(void);          /* Learn polling + the pending close */

int  lj_midi_in_form_tab(int tab);                 /* 0 Setup, 1 Sequences, 2 Cues, 3 BGCues, 4 CueLists, 5 Functions, 6 MIDI Log; -1 out of range */
int  lj_midi_in_form_set_number(uint32_t number);  /* the number field of the program tabs */
int  lj_midi_in_form_set_function(enum lj_fn fn);  /* the function of the Functions tab */
int  lj_midi_in_form_set_channel(int channel);     /* 1..16 (16 = All) for the current tab's kind */
int  lj_midi_in_form_learn(void);                  /* the Learn button; -1 on the Setup / Log tabs */
int  lj_midi_in_form_set_note(int note);           /* "<- Set" on the selected / given note (0..127); -1 when nothing to bind */
void lj_midi_in_form_log(const uint8_t msg[3]);    /* the MIDI Log tab (the app feeds it from its dispatch pass) */

#endif /* LJ_WINDOWS_MIDI_IN_FORM_H */
