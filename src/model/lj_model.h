/* lj_model.h - the data model (design 4, verbatim): limits, the per-channel fade
 * state, scenes, sequences, cues, cue lists, background cues, statics, fixtures,
 * groups, presets, palettes, patch, generic macros, follow spots, the show.
 * Plain structs; validation, copy and serialisation live in the per-record
 * headers (lj_sequence.h, lj_cue.h, ...). Strings are CP1252 bytes, times are
 * integer milliseconds (D6). Everything is fixed-size except scene arrays and
 * cue-list lines, which are allocated on load.
 *
 * The six engine-facing limits (LJ_LINKS .. LJ_BG_SLOTS) live in core/lj_types.h
 * (W0 contract) and are not redefined here. */
#ifndef LJ_MODEL_MODEL_H
#define LJ_MODEL_MODEL_H

#include <stdint.h>
#include "core/lj_types.h"
#include "fixlib/lj_profile.h"
#include "model/lj_table.h"

enum { LJ_BG_FILES = 99, LJ_SCENES_MAX = 999, LJ_PRESETS = 99, LJ_USER_GROUPS = 99, LJ_SUBMASTERS = 8, LJ_HTP_GROUPS = 24,
       LJ_GEN_GROUPS = 32, LJ_CUE_PAGES = 255, LJ_MARKS = 4, LJ_MACROS_MAX = 9999, LJ_FOLLOWSPOT = 20,
       LJ_HOTKEY_SETUPS = 8, LJ_STATIC_PAGES_MAX = 64,
       LJ_TIME_MAX_MS = 60000, LJ_SCENE_TIME_DEFAULT_MS = 1000, LJ_FADE_TIME_DEFAULT_MS = 50,
       LJ_NAME_CAP = 31, LJ_NUMBER_MAX = 99999999 /* 8-digit file names [F28] */ };
enum { LJ_EXT_CMD_CAP = 33 /* 32 ASCII characters + NUL [05 s10.2] */, LJ_SCENE_EXT = 4 /* External Application fixtures carrying a string per scene, PROPOSAL rig W5-EXT-4 */ };   /* W5 extapp (appended) */
#define LJ_STATICS2_SLOTS_PER_PAGE 20   /* 14 sB.8 item 2 PROPOSAL (one definition, ME-101) */
#define LJ_STATICS2_SLOTS_MAX 64        /* ME-30 cap: slots stored per page and the global-index stride; n per page visible, LJ_STATICS2_SLOTS_PER_PAGE (20) default, PROPOSAL rig 14 sB.8 item 2 */

typedef enum { LJ_OFF = 0, LJ_SNAP = 1, LJ_FADE = 2 } lj_fstate;         /* 03 s2; byte values as .seq implies, 18 s4 */

/* ---- enums behind the uint8_t fields below; the text-record words are in lj_words.h ---- */
enum lj_trig       { LJ_TRIG_AUTO = 0, LJ_TRIG_MANUAL, LJ_TRIG_AUDIO };                        /* 03 s5.5 */
enum lj_dir        { LJ_DIR_FWD = 0, LJ_DIR_REV, LJ_DIR_BOUNCE, LJ_DIR_RANDOM };               /* 03 s5.5 */
enum lj_end_action { LJ_END_STOP = 0, LJ_END_OUT, LJ_END_GOTO, LJ_END_LINK };                   /* 03 s5.5 */
enum lj_bg_mode    { LJ_BG_AUTO = 0, LJ_BG_AUDIO, LJ_BG_NOLOOP };                               /* 03 s7 */
enum lj_activation { LJ_ACT_TOGGLE = 0, LJ_ACT_FLASH };                                         /* 14 sB.5 */
enum lj_replace    { LJ_REP_ADD = 0, LJ_REP_SLOT, LJ_REP_PAGE, LJ_REP_GROUP, LJ_REP_ALL,
                     LJ_REP_SOLO_SLOT, LJ_REP_SOLO_PAGE, LJ_REP_SOLO_GROUP, LJ_REP_SOLO_ALL };   /* 14 sB.7 */
enum lj_static_kind{ LJ_STATIC_EMPTY = 0, LJ_STATIC_RECORDED, LJ_STATIC_LINK };                 /* 14 sB.6 */
enum lj_fixture_kind { LJ_FX_BUILTIN = 0, LJ_FX_GS2 };                                          /* 02 s3.1 */
enum lj_patch_src  { LJ_SRC_NONE = 0, LJ_SRC_DMX_OUT, LJ_SRC_DMX_IN };                          /* 02 s7.1 */
enum lj_in_mode    { LJ_IN_ABSOLUTE = 0, LJ_IN_LOTP, LJ_IN_HTP, LJ_IN_GT0, LJ_IN_LT255 };        /* 02 s7.1: Absolute, LoTP, HTP, DMX > 0, DMX < 255 */
enum lj_cl_adv     { LJ_CL_AUTO = 0, LJ_CL_MANUAL };                                            /* 03 s6.1 line advance */

typedef struct { uint8_t type, amp_pan, amp_tilt, cycle_pan, cycle_tilt, dir, delay, tweak; uint16_t step;
                 uint8_t fstate[6]; } lj_movemacro;                      /* 04 s4.4, 17 s7; each parameter has its own O/S/F */
typedef struct {
    uint8_t  val[LJ_CHANS];          /* indexed by address code (link-1)*512 + addr-1 [18 s3] */
    uint8_t  state[LJ_CHANS];        /* lj_fstate; unprogrammed = LJ_OFF */
    uint32_t scene_ms, fade_ms;      /* defaults 1000 / 50 [03 s3.1] */
    uint8_t  preset[LJ_FIXTURES];    /* 0 = none, 1..99 position preset reference [04 s4.3] */
    lj_movemacro macro[LJ_FIXTURES];
    struct { uint8_t fixture /* 1..LJ_FIXTURES, 0 = unused */; char cmd[LJ_EXT_CMD_CAP]; } ext[LJ_SCENE_EXT];   /* W5 extapp (appended): the External Application/Device command string of this scene per profile-90 fixture [05 s10.2]; the `ext` word of the scene record (docs/w5-contracts.md seam 6) */
} lj_scene;                          /* 5.9 KB; a 999-scene sequence is 5.9 MB, loaded on demand */

typedef struct { uint32_t number; char name[31], type[31]; uint16_t n; lj_scene *scenes; uint8_t modified;
                 uint8_t sync_times, global_scene_time, global_fade_time; } lj_sequence;   /* 03 s3.4 options */

typedef struct { uint32_t seq; uint8_t enabled, trig /*enum lj_trig*/, dir /*enum lj_dir*/;
                 uint8_t scenes_per_trig, audio_trigs_per_trig, trig_on_load; uint16_t first, last, loops;
                 uint8_t end_action /*enum lj_end_action*/; uint16_t goto_scene; uint32_t link_cue;
                 uint16_t time_pct /*0 = CUE, 1..999*/; uint32_t chase_ms; } lj_cue_slot;   /* 03 s5.5-5.6 */
typedef struct { uint32_t number; char name[31], type[31]; lj_cue_slot slot[LJ_CUE_SLOTS]; uint16_t cue_time_pct;
                 uint16_t macro_amp_pct, macro_speed_pct /* 0..999: the cue macro control's 1-999 % option [03 s5.11] (W3 movement, was uint8_t) */;
                 uint8_t save_macro_values, transparent; uint32_t bg_cue, gen_macro; } lj_cue;

typedef struct { uint32_t line; uint8_t manual /*enum lj_cl_adv*/; uint8_t cmd /*enum lj_cl_cmd, model/lj_cuelist.h*/; uint32_t param_ms; char param[128]; uint32_t cue;
                 uint16_t trig_mask; char comment[64];
                 uint32_t ref;            /* W3: the record a drag-only command names: flash_sequence = sequence, load_background_cue = bg cue, link_cuelist = cue list (model/lj_cuelist.h) */
                 uint8_t  load_error; } lj_cl_line;    /* 03 s6.1; load_error (W3, design 8.5): a CD / DA / MP line, shown red, skipped at execution, written back unchanged */
typedef struct { uint32_t number; char name[31]; uint32_t n, cap; lj_cl_line *lines; uint32_t mark[LJ_MARKS]; uint8_t tracking; } lj_cuelist;

typedef struct { uint32_t number; char name[31]; uint8_t transparent;
                 struct { uint32_t seq; uint8_t enabled, mode /*enum lj_bg_mode*/; } slot[LJ_BG_SLOTS]; } lj_bgcue;   /* 03 s7 */

typedef struct { char name[64], type[64]; uint8_t activation /*enum lj_activation*/;
                 uint8_t replace /*enum lj_replace*/;
                 uint32_t fade_ms; uint8_t level_pct, transparent; uint64_t groups;
                 uint8_t kind /*enum lj_static_kind*/; uint32_t link_seq; uint16_t link_scene; lj_scene *scene; } lj_static_slot;  /* 14 sB */
typedef struct { char name[64]; uint16_t n; lj_static_slot slot[LJ_STATICS2_SLOTS_MAX]; } lj_static_page;

typedef struct { uint8_t kind /*enum lj_fixture_kind*/; uint16_t profile_id;   /* BUILTIN: exe row id 0-200 (LJ2's own Config.dat type code) or extra >= 1000, section 6.1 */
                 char gs2_file[64];                         /* GS2: UserFixtureDef\<name>.GS2 */
                 uint8_t link; uint16_t address;            /* 1-4, 1-512 */
                 uint8_t invert_pan, invert_tilt, swap_pt, ignore_bo, ignore_gm, link_strobe_gm;
                 char user_name[31]; uint8_t placed, tab; int16_t icon_x, icon_y; uint8_t intensity_group /*0 or 1-8*/;
                 lj_palette custom[6];
                 char ch_caption[LJ_PROFILE_CH][7]; uint32_t ch_gm_off;   /* W3 Channel Configuration (ME-32): per-offset caption ("" = the profile's), bit k = Disable Grand Master Control on offset k; Config.dat chcap n= / chgm= */
               } lj_fixture;        /* 02 s2.4, s4.3; kind/profile_id 0 with link 0 = unused row */

typedef struct { char name[31]; uint8_t member[LJ_FIXTURES]; } lj_group;                       /* 18 s8 */
typedef struct { uint8_t used; int16_t pan, tilt; } lj_preset;                                  /* per fixture x 99, readout units 17 s1 */
typedef struct { char name[31]; uint8_t val[LJ_CHANS], state[LJ_CHANS]; } lj_user_palette;     /* per control kind, growable */
typedef struct { uint8_t src_kind /*enum lj_patch_src*/, src_link; uint16_t src_ch; uint8_t in_mode /*enum lj_in_mode*/, invert, min, max; uint32_t htp_mask; } lj_patch_ch;  /* 02 s7.1; min/max 0/255 = no limit */
typedef struct { uint8_t shape, base_on, base; int16_t amp_pos, amp_neg; uint16_t cycle_ds; uint8_t dir; uint16_t delay; } lj_gm_param;   /* W3 named it (was anonymous): shape 0..65 = Macros.Mac index [14 sA.6], dir enum lj_dir FWD/REV/BOUNCE, delay 0..8191 */
typedef struct { uint32_t number; char name[31]; uint8_t restart_on_load;
                 lj_gm_param ch[LJ_CHANS]; } lj_genmacro;

typedef struct { uint8_t used; char name[31]; uint8_t selected[LJ_FIXTURES];   /* the selection state is recorded too */
                 uint8_t val[LJ_CHANS], state[LJ_CHANS];                        /* "Apply Current Settings": every programmed channel, applied with all channels snapped */
                 uint8_t static_page, static_slot, static_auto_clear; } lj_followspot;   /* 04 s13: 20 settings; activation opens Intensity + Movement; static association since 2.6 */

/* W3: Generic DMX Control channel groups [CHM genericdmxcontrol, F35]: 32 named groups, membership and the
 * "Hide Selected Channels" map per address code (GenericGroups.dat, PROPOSAL name; model/lj_gdc.h). */
typedef struct { char name[LJ_GEN_GROUPS][31]; uint8_t used[LJ_GEN_GROUPS]; uint32_t member[LJ_CHANS] /* bit g = group g+1 */; uint8_t hidden[LJ_CHANS]; } lj_gdc_groups;
typedef struct lj_bez_set lj_bez_set;   /* W3: the Bezier movement shapes, sampled (model/lj_movemacro.h) */

typedef struct {
    lj_fixture fixture[LJ_FIXTURES]; lj_patch_ch patch[LJ_CHANS]; uint8_t default_out[LJ_CHANS];
    lj_group user_group[LJ_USER_GROUPS]; char submaster_name[LJ_SUBMASTERS][6]; uint8_t submaster_mode[LJ_SUBMASTERS];   /* enum lj_master_mode, engine/lj_engine_rules.h */
    lj_preset preset[LJ_FIXTURES][LJ_PRESETS]; lj_static_page *pages; int npages;
    lj_followspot followspot[LJ_FOLLOWSPOT]; lj_table sequences, cues, cuelists, bgcues, macros;   /* number -> record; loaded, inserted and freed by the UI thread only (2.3); in-memory lookup elsewhere */
    lj_sequence *current_seq; lj_cue *current_cue; lj_cuelist *current_cl;
    /* ---- W3 (appended; docs/w3-contracts.md) ---- */
    lj_profile *gs2; int ngs2;          /* the user's UserFixtureDef\*.GS2, parsed (fixlib/lj_gs2.h); a GS2 fixture's profile is lj_show_gs2(show, gs2_file); replaced whole under lj_lock (Reload profile data, ME-31) */
    const lj_bez_set *bez;              /* Bezier movement shapes sampled to the 4096-point tables; the engine reads it per tick for macro types >= LJ_MM_TYPE_BEZ0; replaced whole under lj_lock */
    lj_gdc_groups gdc;                  /* Generic DMX Control groups and hidden channels */
} lj_show;   /* ~720 KB: always heap-allocated (lj_show_new), never on a stack */

/* Rules kept from the inventory (design 4 last paragraph): programming data is keyed
 * by DMX address code, so changing a fixture's address invalidates its data and
 * Copy From Fixture is the sanctioned move [02 s1.4]; cue slots hold sequence
 * numbers so editing a sequence changes every cue and a deleted sequence shows
 * `load error` [03 s5.1]; numbers are assigned on first save and immutable,
 * `Save as Cue #` overwrites silently [F28]; Insert is always before the current
 * scene, delete-all leaves one all-off scene, Add copies the current scene [F29]
 * (lj_scene.h). */

static inline int lj_addr_code(int link, int addr) { return (link - 1) * LJ_LINK_CH + (addr - 1); }   /* 1-4, 1-512 -> 0..2047 */
/* ME-132: the address code of offset ofs of fixture fx, or -1 when fx has no address (F41 warning state) or ofs runs past
 * channel 512 of its link (the clip of config_check, lj_show.c). Every engine per-fixture access goes through it. */
static inline int lj_fx_code(const lj_fixture *fx, int ofs) { return fx->link < 1 || fx->link > LJ_LINKS || fx->address < 1 || ofs < 0 || fx->address + ofs > LJ_LINK_CH ? -1 : lj_addr_code(fx->link, fx->address + ofs); }

#endif /* LJ_MODEL_MODEL_H */
