/* lj_shapes_gen.h - the 66 regenerated generic-macro shapes (design 3.6, 14 sA.6). lj_shapes_gen.c is emitted by
 * tools/gen_vectors.py --shapes: OUR integer samples (the sine family exactly round(32640 * sin), the others from our
 * own authored piecewise definitions checked against the decode of 14-macros_mac_decode.py), with the generator and
 * the decode script named in its banner as provenance. Martin's Macros.Mac bytes are never committed or shipped;
 * a user's own copy reaches the engine only through LJ_CMD_GM_SHAPES (ME-34). */
#ifndef LJ_ENGINE_SHAPES_GEN_H
#define LJ_ENGINE_SHAPES_GEN_H

#include "model/lj_genmacro.h"

extern const lj_gm_shapes lj_gm_shapes_builtin;   /* n = nb = LJ_GM_SHAPES, names verbatim in file order; the W3 contract stub has n = 0 (sine fallback) */

#endif /* LJ_ENGINE_SHAPES_GEN_H */
