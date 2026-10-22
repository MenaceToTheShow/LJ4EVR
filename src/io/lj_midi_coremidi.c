/* lj_midi_coremidi.c - the macOS MIDI backend (lj_midi.h, design 8.3):
 * MIDIClientCreate / MIDIInputPortCreate; the read proc runs on CoreMIDI's thread
 * and only hands each 3-byte channel message to cb (the app passes lj_midi_post):
 * never the engine, the sender or the model (ME-23). The OS header is allowed here
 * by tools/test_layering.py OS_ALLOW. Device index = the CoreMIDI source index.
 *
 * Packets may use running status and carry several messages; the byte stream is
 * parsed by lj_midi_feed (lj_midi.c, LORD_C-6: real-time skipped, SysEx skipped,
 * MTC quarter frames carried as {F1, data, 0}). The client stays for the process (CoreMIDI wants one per
 * app); the port is disposed on close. */
#include <CoreMIDI/CoreMIDI.h>
#include "io/lj_midi.h"
#include "core/lj_log.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"   /* MIDIInputPortCreate / MIDIPacketList: the design's names; the MIDI 2.0 UMP API is the upgrade */
#endif

static MIDIClientRef lj_cm_client;
static MIDIPortRef lj_cm_port;
static MIDIEndpointRef lj_cm_src;
static void (*lj_cm_cb)(void *ud, const uint8_t msg[3]);
static void *lj_cm_ud;

static void lj_cm_msg(const uint8_t m[3], void *ud) { (void)ud; if (lj_cm_cb) lj_cm_cb(lj_cm_ud, m); }

static void lj_cm_read(const MIDIPacketList *pl, void *refcon, void *srcref)
{
    const MIDIPacket *p = &pl->packet[0];
    UInt32 i;
    (void)refcon; (void)srcref;
    for (i = 0; i < pl->numPackets; i++, p = MIDIPacketNext(p)) {
        uint8_t run[3] = { 0, 0, 0 };                                      /* per packet, as before: a packet starts with a status byte */
        int have = 0;
        lj_midi_feed(p->data, p->length, run, &have, lj_cm_msg, NULL);
    }
}

static void lj_cm_name(MIDIEndpointRef e, char *dst, size_t cap)
{
    CFStringRef s = NULL;
    dst[0] = 0;
    if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s) != noErr || !s) return;
    if (!CFStringGetCString(s, dst, (CFIndex)cap, kCFStringEncodingWindowsLatin1)) dst[0] = 0;   /* CP1252 in memory (D6); a name it cannot hold reads empty */
    CFRelease(s);
}

int lj_midi_list(lj_midi_dev *out, int max)
{
    ItemCount i, n = MIDIGetNumberOfSources();
    int got = 0;
    for (i = 0; i < n && out && got < max; i++) {
        MIDIEndpointRef e = MIDIGetSource(i);
        if (!e) continue;
        out[got].idx = (int)i;
        lj_cm_name(e, out[got].name, sizeof out[got].name);
        got++;
    }
    return out ? got : (int)n;
}

int lj_midi_open(int idx, void (*cb)(void *ud, const uint8_t msg[3]), void *ud)
{
    OSStatus r;
    lj_midi_close();
    if (idx < 0 || (ItemCount)idx >= MIDIGetNumberOfSources() || !cb) { lj_log("midi: no input device %d", idx); return -1; }
    lj_cm_src = MIDIGetSource((ItemCount)idx);
    if (!lj_cm_src) { lj_log("midi: no input device %d", idx); return -1; }
    lj_cm_cb = cb; lj_cm_ud = ud;
    if (!lj_cm_client && (r = MIDIClientCreate(CFSTR("LJ4EVR"), NULL, NULL, &lj_cm_client)) != noErr) { lj_log("midi: MIDIClientCreate failed (%d)", (int)r); lj_cm_client = 0; return -1; }
    if ((r = MIDIInputPortCreate(lj_cm_client, CFSTR("LJ4EVR MIDI In"), lj_cm_read, NULL, &lj_cm_port)) != noErr) { lj_log("midi: MIDIInputPortCreate failed (%d)", (int)r); lj_cm_port = 0; return -1; }
    if ((r = MIDIPortConnectSource(lj_cm_port, lj_cm_src, NULL)) != noErr) { lj_log("midi: connect to source %d failed (%d)", idx, (int)r); MIDIPortDispose(lj_cm_port); lj_cm_port = 0; return -1; }
    return 0;
}

void lj_midi_close(void)
{
    if (!lj_cm_port) return;
    MIDIPortDisconnectSource(lj_cm_port, lj_cm_src);
    MIDIPortDispose(lj_cm_port);                   /* returns after the read proc is done with this port */
    lj_cm_port = 0; lj_cm_src = 0;
}
