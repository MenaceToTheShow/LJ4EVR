/* lj_gs2.c - W1 stubs for the .GS2 / .Udf importer and exporter (lj_gs2.h): every body
 * returns -1 until W3 transcribes tools/ref/16-gs2_ref.py. The 0x266C note is in lj_gs2.h. */
#include "fixlib/lj_gs2.h"

int lj_gs2_parse(const uint8_t rec[LJ_GS2_REC_SIZE], lj_profile *out, char *err, size_t cap)
{
    (void)rec; (void)out;
    if (err && cap) err[0] = 0;
    return -1;
}

int lj_gs2_write(const lj_profile *in, uint8_t rec[LJ_GS2_REC_SIZE]) { (void)in; (void)rec; return -1; }

int lj_udf_import(const char *path, lj_profile *out, char *err, size_t cap)
{
    (void)path; (void)out;
    if (err && cap) err[0] = 0;
    return -1;
}
