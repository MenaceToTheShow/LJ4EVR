/* lj_profile.c - the fixlib API (design 2.4, 6.1): the 201 exe rows as identity, hand
 * profiles (tier 1) over QLC+ conversions (tier 2) over nothing (greyed). lj_profile_window
 * is the W1 contract body kept as written. */
#include "fixlib/lj_profile.h"
#include "fixlib/lj_exe_profiles.h"
#include "fixlib/lj_profiles_gen.h"
#include "fixlib/lj_layout.h"
#include "fixlib/hand/lj_mac500.h"
#include "fixlib/hand/lj_rs918.h"
#include "fixlib/hand/lj_generics.h"
#include "fixlib/hand/lj_mac600.h"
#include "fixlib/hand/lj_mac250.h"
#include "fixlib/hand/lj_mac2000.h"
#include "fixlib/hand/lj_roboscan.h"

/* W2 tier-1 families 15/02-15/05: one array per hand file. */
static const struct { const lj_profile *const *p; int n; } lj_hand_w2[] = {
    { lj_hand_mac600s, LJ_MAC600_COUNT }, { lj_hand_mac250s, LJ_MAC250_COUNT }, { lj_hand_mac2000s, LJ_MAC2000_COUNT }, { lj_hand_roboscans, LJ_ROBOSCAN_COUNT } };

static const lj_profile *lj_hand_by_id(uint16_t id)
{
    int i, k;
    for (k = 0; k < (int)(sizeof lj_hand_w2 / sizeof lj_hand_w2[0]); k++)
        for (i = 0; i < lj_hand_w2[k].n; i++) if (lj_hand_w2[k].p[i]->id == id) return lj_hand_w2[k].p[i];
    if (id == 12) return &lj_hand_mac500;
    if (id == 31) return &lj_hand_mac500_scrl;
    if (id == 24) return &lj_hand_rs918;
    if (id == 30) return &lj_hand_rs918_scrl;
    for (i = 0; i < LJ_GENERICS_COUNT; i++) if (lj_hand_generics[i]->id == id) return lj_hand_generics[i];
    return NULL;
}

const lj_profile *lj_fixlib_by_id(uint16_t profile_id)
{
    const lj_profile *h = lj_hand_by_id(profile_id);     /* a hand file supersedes the conversion bound to the same id (6.1 item 1) */
    int i;
    if (h) return h;
    for (i = 0; i < lj_qxf_count; i++) {
        lj_profile *p = lj_qxf_profiles[i];
        if (p->id != profile_id) continue;
        if (!p->windows && lj_layout_derive(p) != 0) return NULL;   /* tier 2: windows derived once, at first use (6.3) */
        return p;
    }
    return NULL;
}

int lj_fixlib_count(void) { return LJ_EXE_ROWS + lj_qxf_extra_count; }

const lj_exe_row *lj_fixlib_row(int i)
{
    if (i < 0) return NULL;
    if (i < LJ_EXE_ROWS) return &lj_exe_rows[i];
    i -= LJ_EXE_ROWS;
    return i < lj_qxf_extra_count ? &lj_qxf_extra_rows[i] : NULL;
}

const lj_ctl_window *lj_profile_window(const lj_profile *p, enum lj_ctl_kind kind)
{
    int i;
    if (!p) return NULL;
    for (i = 0; i < p->nwindows; i++) if (p->windows[i].kind == kind) return &p->windows[i];
    return NULL;
}
