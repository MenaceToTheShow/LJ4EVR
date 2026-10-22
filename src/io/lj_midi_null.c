/* lj_midi_null.c - the no-device MIDI backend (lj_midi.h): Linux CI and the tests.
 * lj_midi_inject() is the only source of messages here. Selected by CMake when
 * neither WIN32 nor APPLE. */
#include "io/lj_midi.h"

int  lj_midi_list(lj_midi_dev *out, int max) { (void)out; (void)max; return 0; }
int  lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud) { (void)idx; (void)cb; (void)ud; return -1; }
void lj_midi_close(void) { }
