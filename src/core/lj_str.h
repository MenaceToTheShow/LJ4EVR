/* lj_str.h - bounded CP1252 strings and the three UTF-8 boundary conversions.
 * Design 2.1: every string in memory and in every show file is Windows-1252;
 * UTF-8 exists only at the OS boundaries (paths, SDL text/clipboard, sACN name).
 * One 128-entry table in lj_str.c serves both conversions; unmappable -> '?'. */
#ifndef LJ_CORE_STR_H
#define LJ_CORE_STR_H

#include <stddef.h>
#include <stdint.h>

int lj_str_copy(char *dst, size_t cap, const char *src);          /* truncates, always NUL-terminates; CP1252 in, CP1252 out; -1 when truncated */
int lj_cp1252_to_utf8(char *dst, size_t cap, const char *src);    /* the three OS boundaries only; -1 when truncated */
int lj_utf8_to_cp1252(char *dst, size_t cap, const char *src);    /* unmappable code point -> '?'; -1 when truncated or malformed */
int lj_fmt_time(char *dst, size_t cap, uint32_t ms);              /* "ss:hh", seconds:hundredths [07 fact-check] */

/* W3 (design 5.3, LORD_FIDELITY-17): the user's own RTF profile notes shown stripped: control words and {\*...}
 * groups dropped, \par = newline, \'hh = that CP1252 byte; the raw bytes stay in the profile for export. 0 / -1 (truncated). gs2 track. */
int lj_str_rtf_strip(char *dst, size_t cap, const char *rtf);

#endif /* LJ_CORE_STR_H */
