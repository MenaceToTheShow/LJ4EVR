/* test_statics.c - `lj4evr_tests test_statics`: the Statics 2 contract pieces that exist today
 * (lj_static_global_index bounds, the LJ_STATICS2_SLOTS_MAX / LJ_STATIC_GROUPS caps, ME-30) and the
 * rows the W2 engine track fills: scope releases (lj_static_scope_hits), LTP re-order through the
 * engine (statics_ltp_reorder.lj4r is the replay side), the level scaling of every channel class,
 * the 65th activation releasing the oldest. The filled rows are marked; until then they are not
 * counted (a stub that passes is no evidence, w2-gates "a row with no evidence is not ticked"). */
#include <stdio.h>
#include "model/lj_statics.h"
#include "engine/lj_engine_rules.h"

_Static_assert(LJ_STATICS2_SLOTS_PER_PAGE <= LJ_STATICS2_SLOTS_MAX && LJ_STATICS2_SLOTS_MAX == 64, "ME-30 cap");
_Static_assert(LJ_STATIC_GROUPS == 64, "the uint64_t group mask");
_Static_assert(LJ_STATIC_RUNNERS == 64, "64 static runners");

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int lj_test_statics(int argc, char **argv);
int lj_test_statics(int argc, char **argv)
{
    (void)argc; (void)argv;
    CHECK(lj_static_global_index(1, 1) == 0);
    CHECK(lj_static_global_index(1, LJ_STATICS2_SLOTS_PER_PAGE) == LJ_STATICS2_SLOTS_PER_PAGE - 1);
    CHECK(lj_static_global_index(2, 1) == LJ_STATICS2_SLOTS_PER_PAGE);
    CHECK(lj_static_global_index(LJ_STATIC_PAGES_MAX, LJ_STATICS2_SLOTS_PER_PAGE) == LJ_STATIC_PAGES_MAX * LJ_STATICS2_SLOTS_PER_PAGE - 1);
    CHECK(lj_static_global_index(0, 1) == -1 && lj_static_global_index(1, 0) == -1 && lj_static_global_index(65, 1) == -1 && lj_static_global_index(1, 21) == -1);
    printf("test_statics: %d failure(s) (scope / LTP / level / 65th-activation rows: W2 engine track)\n", fails);
    return fails ? 1 : 0;
}
