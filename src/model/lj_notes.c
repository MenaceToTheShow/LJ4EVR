/* lj_notes.c - W2 contract stub (lj_notes.h); the model track replaces the bodies. */
#include "model/lj_notes.h"

int lj_notes_load(const char *path, char *buf, size_t cap) { (void)path; if (buf && cap) buf[0] = 0; return -1; }
int lj_notes_save(const char *path, const char *text) { (void)path; (void)text; return -1; }
