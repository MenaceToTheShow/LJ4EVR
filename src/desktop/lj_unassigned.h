/* lj_unassigned.h - the Unassigned Fixtures list ([01 s3.7], F13): the generated
 * TUnassignedForm (grey, stay-on-top, No / FixtureType / Address / Username, menu
 * Unassign All / Help, Move Icons, Auto Place All) with every DFM handler bound
 * (ME-16); rows drag out onto the desktop (lj_dnd kind LJ_DND_FIXTURE). Public API:
 * lj_unassigned_* in lj_desktop.h. The app shows it through lj_desktop_set_host. */
#ifndef LJ_DESKTOP_UNASSIGNED_H
#define LJ_DESKTOP_UNASSIGNED_H

#include "desktop/lj_desktop.h"

int lj_unassigned_fixture_at_row(int row);   /* fixture index of a list row, -1 */

#endif /* LJ_DESKTOP_UNASSIGNED_H */
