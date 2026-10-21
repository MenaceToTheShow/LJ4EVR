/* lj_trig_gen.h - the two 4096-point int16 sine/cosine tables (design 3.6).
 * lj_trig_gen.c is emitted by tools/gen_vectors.py so nothing in the binary
 * calls sin(): UCRT, glibc and Apple libm differ in the last ulp. */
#ifndef LJ_ENGINE_TRIG_GEN_H
#define LJ_ENGINE_TRIG_GEN_H

#include <stdint.h>

extern const int16_t lj_sin4096[4096];   /* round(32767 * sin(2*pi*i/4096)); sin[1024] == 32767 */
extern const int16_t lj_cos4096[4096];   /* == lj_sin4096[(i + 1024) & 4095] */

#endif /* LJ_ENGINE_TRIG_GEN_H */
