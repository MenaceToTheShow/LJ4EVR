/* lj_midi_winmm.c - the Windows MIDI backend (lj_midi.h, design 8.3): midiInOpen /
 * midiInStart; the MIM_DATA callback runs on WinMM's thread and only calls
 * lj_midi_post (never the engine, never the sender: ME-23). W2 midi track fills the
 * bodies; the OS headers are allowed here by tools/test_layering.py OS_ALLOW. */
#include <windows.h>
#include <mmsystem.h>
#include "io/lj_midi.h"

int  lj_midi_list(lj_midi_dev *out, int max) { (void)out; (void)max; return 0; }   /* W2 midi track: midiInGetNumDevs / midiInGetDevCapsA */
int  lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud) { (void)idx; (void)cb; (void)ud; return -1; }
void lj_midi_close(void) { }
