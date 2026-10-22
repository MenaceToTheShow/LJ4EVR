/* lj_midi_coremidi.c - the macOS MIDI backend (lj_midi.h, design 8.3):
 * MIDIClientCreate / MIDIInputPortCreate; the read proc runs on CoreMIDI's thread
 * and only calls lj_midi_post (ME-23). W2 midi track fills the bodies; the OS header
 * is allowed here by tools/test_layering.py OS_ALLOW. */
#include <CoreMIDI/CoreMIDI.h>
#include "io/lj_midi.h"

int  lj_midi_list(lj_midi_dev *out, int max) { (void)out; (void)max; return 0; }   /* W2 midi track: MIDIGetNumberOfSources / MIDIObjectGetStringProperty */
int  lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud) { (void)idx; (void)cb; (void)ud; return -1; }
void lj_midi_close(void) { }
