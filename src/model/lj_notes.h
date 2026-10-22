/* lj_notes.h - the Notes window text (W2; 01 s4 TNoteForm "Notes": a plain memo,
 * stay-on-top, View > Notes, hotkey Toggle Notes, "Show Notes on Startup"). LJ2's
 * System\Notes\*.rtf are Martin's shipped profile notes (not this); the user's own
 * notes file name is undecoded, so ours: <library>\Notes.txt, CP1252 text, line
 * ends as typed (PROPOSAL, rig: an LJ2 library with notes). Backups carry it (F51). */
#ifndef LJ_MODEL_NOTES_H
#define LJ_MODEL_NOTES_H

#include <stddef.h>

enum { LJ_NOTES_CAP = 65536 };      /* PROPOSAL: a TMemo is unbounded; 64 KB is the W2 ceiling */
#define LJ_NOTES_FILE "Notes.txt"   /* PROPOSAL rig (18 s2 lists no notes file) */

int lj_notes_load(const char *path, char *buf, size_t cap);   /* missing file = empty text, 0; -1 on a read error or a text longer than cap-1 (truncated, logged) */
int lj_notes_save(const char *path, const char *text);        /* 0 / -1 + log */

#endif /* LJ_MODEL_NOTES_H */
