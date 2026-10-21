/* lj_gs2.h - the .GS2 / .Udf importer and exporter (design 5.3, note 16). The prototypes
 * are in fixlib/lj_profile.h; W1 ships stubs that return -1, W3 the C transcription of
 * tools/ref/16-gs2_ref.py.
 *
 * W0-gs2-266C: the 17 unlisted bytes at .GS2 offset 0x266C are carried by the W2/W3
 * lj_gs2_parse as `reserved_266C`, raw 17 bytes inside lj_profile.gs2_raw, and travel
 * untouched through lj_gs2_write (serialize(parse(x)) of 16-gs2_ref.py). lj_profile
 * reserves no field for them and nothing in W1 decodes them. */
#ifndef LJ_FIXLIB_GS2_H
#define LJ_FIXLIB_GS2_H

#include "fixlib/lj_profile.h"

#endif /* LJ_FIXLIB_GS2_H */
