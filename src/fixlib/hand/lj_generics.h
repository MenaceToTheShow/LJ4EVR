/* lj_generics.h - the generic built-ins of 15/10: DMX Dimmer 1-32, DMX Switch Pack 1-32,
 * RGB Pack x4(A/B)/x6/x8/x10, DMX Smoke Machine, DMX Hazer, Martin 6 Channel DMX switch,
 * External Application/Device and the three User Definable placeholders. */
#ifndef LJ_FIXLIB_HAND_GENERICS_H
#define LJ_FIXLIB_HAND_GENERICS_H

#include "fixlib/lj_profile.h"

enum { LJ_GENERICS_COUNT = 32 };
extern const lj_profile *const lj_hand_generics[LJ_GENERICS_COUNT];   /* in exe-id order, ids 18 19 20 21 22 23 27 42 43 45 46 56 70 71 72 73 75 76 77 78 90 92-101 135 */

#endif /* LJ_FIXLIB_HAND_GENERICS_H */
