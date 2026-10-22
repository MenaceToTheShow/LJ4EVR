/* lj_midi_winmm.c - the Windows MIDI backend (lj_midi.h, design 8.3): midiInOpen /
 * midiInStart; the MIM_DATA callback runs on WinMM's thread and only hands the
 * 3-byte message to cb (the app passes lj_midi_post): never the engine, never the
 * sender, never the model (ME-23). The OS headers are allowed here by
 * tools/test_layering.py OS_ALLOW.
 *
 * WinMM's rule for the callback is "no system calls but a few"; cb takes lj_lock
 * (an SRWLOCK, no allocation, no wait beyond one engine tick), the accepted
 * practice for MIM_DATA. Close order: midiInStop -> midiInReset -> midiInClose;
 * WinMM waits for a running callback, so lj_midi_close must not run under lj_lock. */
#include <windows.h>
#include <mmsystem.h>
#include "io/lj_midi.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

static HMIDIIN lj_mm_in;
static void (*lj_mm_cb)(void *ud, const uint8_t msg[3]);   /* written before midiInOpen, read on WinMM's thread after it */
static void *lj_mm_ud;

static void CALLBACK lj_mm_proc(HMIDIIN h, UINT what, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    uint8_t m[3];
    (void)h; (void)inst; (void)p2;
    if (what != MIM_DATA || !lj_mm_cb) return;               /* MIM_LONGDATA (SysEx), MIM_ERROR ...: not ours (05 s7.2: note on/off only) */
    m[0] = (uint8_t)(p1 & 0xFF); m[1] = (uint8_t)((p1 >> 8) & 0xFF); m[2] = (uint8_t)((p1 >> 16) & 0xFF);
    if ((m[0] & 0xE0) == 0xC0) m[2] = 0;                      /* program change / channel pressure carry one data byte; the third is undefined */
    lj_mm_cb(lj_mm_ud, m);
}

int lj_midi_list(lj_midi_dev *out, int max)
{
    UINT i, n = midiInGetNumDevs();
    int got = 0;
    for (i = 0; i < n && out && got < max; i++) {
        MIDIINCAPSA caps;
        if (midiInGetDevCapsA(i, &caps, sizeof caps) != MMSYSERR_NOERROR) continue;
        out[got].idx = (int)i;
        lj_str_copy(out[got].name, sizeof out[got].name, caps.szPname);   /* ANSI = CP1252 on the hosts we ship (D6) */
        got++;
    }
    return out ? got : (int)n;
}

int lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud)
{
    MMRESULT r;
    lj_midi_close();
    if (idx < 0 || (UINT)idx >= midiInGetNumDevs() || !cb) { lj_log("midi: no input device %d", idx); return -1; }
    lj_mm_cb = cb; lj_mm_ud = ud;
    r = midiInOpen(&lj_mm_in, (UINT)idx, (DWORD_PTR)lj_mm_proc, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) { lj_log("midi: midiInOpen(%d) failed (%u)", idx, (unsigned)r); lj_mm_in = NULL; return -1; }
    r = midiInStart(lj_mm_in);
    if (r != MMSYSERR_NOERROR) { lj_log("midi: midiInStart(%d) failed (%u)", idx, (unsigned)r); midiInClose(lj_mm_in); lj_mm_in = NULL; return -1; }
    return 0;
}

void lj_midi_close(void)
{
    if (!lj_mm_in) return;
    midiInStop(lj_mm_in);
    midiInReset(lj_mm_in);
    midiInClose(lj_mm_in);
    lj_mm_in = NULL;
}
