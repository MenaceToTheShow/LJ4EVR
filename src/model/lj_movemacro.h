/* lj_movemacro.h - the movement-macro model side (design 3.6, 04 s4.4-4.6, 17 s5-s7): the macro type names in exe
 * order, Bezier shapes (.bez records and their integer sampling to the engine's 4096-point tables), and the pure
 * Fan Out and Auto Delay arithmetic of 17-position_ref.py sections 5 and 6 (tests/vectors.h asserts them). The
 * per-scene parameters stay lj_movemacro in lj_model.h; the per-tick engine side is engine/lj_movemacro.h. No float:
 * Bezier points are de Casteljau in 16.16 fixed point. Bodies: lj_movemacro.c (movement track). */
#ifndef LJ_MODEL_MOVEMACRO_H
#define LJ_MODEL_MOVEMACRO_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_model.h"

/* lj_movemacro.type: 0 = No Macro, 1..6 = the system macros in exe order (EXE 0x35a85d, 04 fact-check 11),
 * LJ_MM_TYPE_BEZ0 + i = Bezier shape i of show->bez (the Macro Type drop-down lists them after the six). */
enum { LJ_MM_TYPE_NONE = 0, LJ_MM_TYPE_ELLIPSE, LJ_MM_TYPE_ELLIPSE2, LJ_MM_TYPE_COS2A_SINA, LJ_MM_TYPE_SINA_ABS_SIN2A,
       LJ_MM_TYPE_SINA3, LJ_MM_TYPE_STEPPED_ELLIPSE, LJ_MM_TYPE_BEZ0, LJ_MM_TYPE_MAX = 255 };
extern const char *const lj_mm_type_names[LJ_MM_TYPE_BEZ0];   /* "No Macro", "System Ellipse", "System Ellipse 2", "System Cos(2*a),Sin(a)", "System Sin(a)*|Sin(2a)|", "System Sin(a)^3", "System Stepped Ellipse" (verbatim) */
enum { LJ_MM_STEP_MAX = 512 /* Macro Step size [04 s4.4] */, LJ_MM_N = 4096 /* == engine LJ_MOVEMACRO_N_TABLE */ };
enum lj_mm_param { LJ_MMP_TYPE = 0, LJ_MMP_PAN_AMP, LJ_MMP_PAN_CYCLE, LJ_MMP_TILT_AMP, LJ_MMP_TILT_CYCLE, LJ_MMP_DELAY };   /* index into lj_movemacro.fstate[6]: the per-row O/S/F buttons (IMG movementmacro; tweak and step share the delay row, PROPOSAL rig 17 s8 item 8) */

/* ---- Bezier shapes [04 s4.5] ---- */
enum { LJ_BEZ_NODES_MAX = 64 /* PROPOSAL: LJ2's limit unknown */, LJ_BEZ_SHAPES_MAX = 248 /* type byte 7..255 */ };
typedef struct { int16_t x, y;                    /* node, readout units of the editor grid (-32768..32767 = the full pan / tilt span, 17 s1) */
                 int16_t in_x, in_y, out_x, out_y; /* the two round handles, absolute */
                 uint8_t line; } lj_bez_node;      /* line = the segment leaving this node is straight ("convert segments to lines") */
typedef struct { char name[31]; uint16_t n; lj_bez_node node[LJ_BEZ_NODES_MAX]; } lj_bez;   /* closed: the last node joins the first */
typedef struct { char name[31]; int16_t pan[LJ_MM_N], tilt[LJ_MM_N]; } lj_bez_table;        /* +-32767 like lj_sin4096 */
struct lj_bez_set { int n; lj_bez_table *t; };                                               /* show->bez; the app builds it from the library, frees after a swap */
int  lj_bez_load(const char *path, lj_bez *out, char *err, size_t cap);   /* "LJ4 bez 1", name=, node n x= y= ix= iy= ox= oy= line= */
int  lj_bez_save(const char *path, const lj_bez *in);
const char *lj_bez_validate(const lj_bez *in);                          /* 3..LJ_BEZ_NODES_MAX nodes, name NUL-terminated */
/* "a fixed number of points between nodes, so travel time between any two nodes is equal" [04 s4.5]: segment k gets
 * LJ_MM_N / n points (the remainder on the last segment, PROPOSAL rig 17 s8 item 6), normalised so the shape spans
 * +-32767 on each axis around its centre. 0 / -1. */
int  lj_bez_sample(const lj_bez *b, lj_bez_table *out);
#define LJ_BEZ_DIR "MacroShapes"   /* <library>\MacroShapes\<name>.bez (PROPOSAL folder; LJ2 lists ".bez" user shapes, 17 s7.2) */
#define LJ_BEZ_DEFAULT_NAME "New Shape"
/* W3 movement (appended): the folder as the engine sees it. The shapes sorted by file name (case-insensitive) are macro
 * types LJ_MM_TYPE_BEZ0 + i, so a deleted shape shifts the types after it (PROPOSAL: LJ2's own numbering is unknown).
 * lj_bez_set_load reads every <dir>/<name>.bez (a bad file is logged and skipped), samples it and fills out (out->t is
 * one malloc; n 0 and t NULL for an empty or missing folder); lj_app_bez_reload swaps it into show->bez under lj_lock
 * and frees the old set after (lj_bez_set_free). lj_bez_list returns the file names without ".bez", sorted. */
int  lj_bez_list(const char *dir, char names[][32], int max);                 /* count, -1 when the folder cannot be read */
int  lj_bez_set_load(const char *dir, struct lj_bez_set *out);
void lj_bez_set_free(struct lj_bez_set *s);
int  lj_bez_path(const char *dir, const char *name, char *out, size_t cap);   /* <dir><sep><name>.bez; -1 when truncated or the name holds a path character */
int  lj_bez_eval(const lj_bez *b, int seg, int j, int m, int32_t *x, int32_t *y);   /* point j of m on segment seg (node seg -> seg+1), editor units, unnormalised (the Bezier editor canvas) */

/* ---- Fan Out [04 s4.2, 17 s5.2], readout units ---- */
void lj_fan_out_offsets(int n, int delta, int32_t *out);   /* out[i] = round(delta * 256 * (i - m) / m), m = (n-1)/2; n >= 2 */
/* pan / tilt indexed by fixture; order[0..n-1] = the Apply-Values-by order (Fixture Number / Icon H / Icon V / Custom).
 * centre = home (0) when center_home, else the middle fixture's value (mean of the two middle ones for even n); results clamp to -32768..32767. */
int  lj_fan_out(int32_t *pan, int32_t *tilt, const int *order, int n, int dpan, int dtilt, int center_home);
int  lj_fan_spread(int32_t *v, const int *order, int n);     /* Spread: v[order[i]] = first + (last - first) * i / (n-1) on one axis */

/* ---- Pan/Tilt Macro Auto Delay [04 s4.6, 17 s6.2] (TAutoDelayForm) ---- */
enum lj_mm_delay_style { LJ_MMD_LINEAR = 0, LJ_MMD_CIRCULAR, LJ_MMD_SEGMENTS, LJ_MMD_RANDOM };
/* max_delay 1..256 (256 = 100 %); results are the lj_movemacro.delay byte (mod 256); random draws from *seed. 0 / -1. */
int  lj_mm_auto_delay(int n, int style, int max_delay, int count, int segments, uint32_t *seed, uint8_t *out);
/* Delay order (enum lj_delay_order of model/lj_genmacro.h): fills order[] with the fixture indexes of sel[0..n-1]
 * sorted by number / icon x / icon y (reversed variants), ties by fixture number (17 s6.2 ASSUMED). */
int  lj_delay_order(const lj_show *show, const int *sel, int n, int mode, int *order);

#endif /* LJ_MODEL_MOVEMACRO_H */
