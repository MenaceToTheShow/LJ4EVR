/* fuzz_sacn_rx.c - libFuzzer target over the sACN receiver (W5-X-HOSTILE, W5-DMXIN-2; CMake -DLJ_FUZZ=ON, clang): every
 * input goes through lj_sacn_rx_parse and lj_dmxin_rx; an accepted packet's fields stay inside the input and the frame
 * stays inside its bounds. */
#include <stdlib.h>
#include <string.h>
#include "io/lj_dmxin.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static int opened;
    lj_sacn_rx rx;
    const lj_dmxin_frame *f;
    if (!opened) { lj_dmxin_cfg c; lj_dmxin_cfg_default(&c); c.universe = 10; lj_dmxin_open(&c, NULL); opened = 1; }
    if (lj_sacn_rx_parse(data, size, &rx) == 0 && (rx.n > LJ_LINK_CH || rx.slots < data || rx.slots + rx.n > data + size || rx.name[63] != 0)) abort();
    lj_dmxin_rx(LJ_DMXIN_SACN, data, size, 0x7F000002u, 0);
    f = lj_dmxin_frame_get();
    if (f->channels > LJ_LINK_CH || f->nsources < 0 || f->nsources > LJ_DMXIN_SOURCES) abort();
    return 0;
}
