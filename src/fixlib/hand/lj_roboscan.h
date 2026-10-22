/* lj_roboscan.h - Roboscan hand profiles [15/05]: Roboscan 518 Mode 3 / 2 (exe ids 3, 58), Roboscan 812
 * Mode 1 / 2 (1, 52), Roboscan 218-II Mode 3 / 2 (2, 57) and the three Martin DMX Interface entries
 * Roboscan 218 (41), Roboscan 805 (36), Roboscan 1220IIR (33). */
#ifndef LJ_FIXLIB_HAND_ROBOSCAN_H
#define LJ_FIXLIB_HAND_ROBOSCAN_H

#include "fixlib/lj_profile.h"

enum { LJ_ROBOSCAN_COUNT = 9 };
extern const lj_profile *const lj_hand_roboscans[LJ_ROBOSCAN_COUNT];   /* ids 3 58 1 52 2 57 41 36 33 */

#endif /* LJ_FIXLIB_HAND_ROBOSCAN_H */
