/* lj_show.h - the show: lj_show allocation, Config.dat (fixtures), DefaultOut.dat,
 * and the whole-library load/save that reads every per-record file the W1
 * windows need (design 5.1, 5.2). Config.dat grammar: "LJ4 config 1", "fx n
 * kind=builtin id=12 link=1 addr=17 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0
 * user="MAC500" placed=1 x=120 y=80 tab=0 sub=0" (gs2=<file> for kind gs2);
 * DefaultOut.dat: "LJ4 defaultout 1" + "ch l:a=v" lines (non-zero only).
 * Records in the tables (sequences, cues, ...) are loaded on demand by the UI
 * thread (design 2.3 rule 2), not by lj_show_load. */
#ifndef LJ_MODEL_SHOW_H
#define LJ_MODEL_SHOW_H

#include <stddef.h>
#include "model/lj_model.h"
#include "model/lj_library.h"

lj_show    *lj_show_new(void);                                   /* heap, zeroed, patch initialised, tables empty; lj_fatal on OOM */
void        lj_show_free(lj_show *show);                         /* frees every table record, the pages, current_* records and the show; caller holds lj_lock and the engine is unbound (2.3 rule 6) */

int         lj_config_load(const char *path, lj_show *show, char *err, size_t cap);   /* Config.dat -> show->fixture[]; 0 / -1 */
int         lj_config_save(const char *path, const lj_show *show);
const char *lj_config_validate(const lj_show *show);             /* NULL = valid: link 1..4, address 1..512, address + footprint - 1 <= 512, no overlap between fixtures, profile known (lj_fixlib_by_id or gs2_file) */
int         lj_defaultout_load(const char *path, uint8_t out[LJ_CHANS], char *err, size_t cap);
int         lj_defaultout_save(const char *path, const uint8_t out[LJ_CHANS]);

int         lj_show_load(lj_show *show, const lj_library *lib);  /* Config.dat, DefaultOut.dat, GroupInfo.dat, Statics.dat, Presets\*, each missing file = defaults; -1 only when Config.dat exists and is invalid */
int         lj_show_save_config(const lj_show *show, const lj_library *lib);   /* Config.dat + DefaultOut.dat + GroupInfo.dat */

int         lj_show_fixture_count(const lj_show *show);          /* rows with link != 0 */
int         lj_show_fixture_base(const lj_show *show, int i);    /* address code of fixture i's channel 1, or -1 when unused or without an address */
int         lj_show_fixture_at(const lj_show *show, int addr_code);   /* fixture index owning an address code, or -1 */
int         lj_show_add_fixture(lj_show *show, const lj_fixture *fx);  /* first free row; returns the index or -1 at LJ_FIXTURES */
void        lj_show_remove_fixture(lj_show *show, int i);        /* clears the row; programming data keyed by address stays [02 s1.4] */

int         lj_show_free_record(lj_show *show, enum lj_rec_kind kind, uint32_t number);   /* unlinks number from the kind's table and frees it; the app calls it under lj_lock after every runner was re-pinned (2.3 rule 6); -1 when absent */

/* ---- W3 (appended; docs/w3-contracts.md seam 3) ----
 * User profiles: every UserFixtureDef\*.GS2 is parsed once into show->gs2 (gs2_raw kept, the raw-record rule of
 * design 5.3); a GS2 fixture finds its profile by gs2_file. Reload (Fixture Configuration "Reload profile data",
 * ME-31) builds a new array outside the lock, swaps it in under lj_lock with lj_engine_bind_profile for every GS2
 * fixture in the same critical section, then frees the old one (2.3 rule 6). */
const lj_profile *lj_show_profile(const lj_show *show, int fixture);          /* built-in by profile_id, GS2 by gs2_file; NULL = unknown / unused row */
const lj_profile *lj_show_gs2(const lj_show *show, const char *gs2_file);
int  lj_show_gs2_load_all(const lj_library *lib, lj_profile **out, int *n);  /* bad files logged and skipped; 0 / -1 (no folder = 0 profiles) */
void lj_show_gs2_free(lj_profile *p, int n);                                 /* lj_gs2_free on each, then the array; NULL is a no-op */
/* Customize Fixture (TFixtureSetupForm [02 s4.3], design 6.5): the per-fixture palette overrides lj_fixture.custom[6]
 * in UserFixtureDef\Setup.<slot>.pal, slot = fixture index + 1 ("LJ4 custompal 1", "pal p entry e rgb= bmp= ..."). */
int  lj_fixture_custom_load(const char *path, lj_palette custom[6], char *err, size_t cap);
int  lj_fixture_custom_save(const char *path, const lj_palette custom[6]);
#define LJ_CUSTOM_PAL_FMT "UserFixtureDef\\Setup.%d.pal"   /* lj_library_path rel, %d = fixture index + 1 (profile-editor track; lj_show_load reads it) */
#define LJ_CUSTOM_NO_RGB  0xFFFFFFFFu                        /* a custom entry overridden to "no colour" (Clear Entry); used = 0 = not overridden */

#endif /* LJ_MODEL_SHOW_H */
