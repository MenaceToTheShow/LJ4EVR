/* lj_ctl_movement.h - the Movement (pan/tilt position) control, "Movement 16 Bit" /
 * "Movement 8 Bit" (design 6.4, 7.7, [04 s4], [07 s8], [17]): the 65536 x 65536 grid
 * with crosshair and white dot, the readout "-16256 : 17120", Clr Macro, the
 * Position Preset group (Apply / Clear), Absolute Position / Relative Movement,
 * Pan / Tilt locks, the Resolution bar, the arrow pad with Home, Pan Fade / Tilt
 * Fade (O/S/F), and the Position Presets dialog (TPRESETFORM). Code-built; the
 * window position persists in Main.Ini [MOVEMENT] with Abs, Res8Bit, Res16Bit.
 *
 * Every number of the position maths is integer and matches tools/ref/
 * 17-position_ref.py; tests/ui/test_controls_b.c runs tests/vectors.h through
 * the lj_pos_* functions. The control writes the current scene directly under
 * lj_lock (seam 2): the pan/tilt pair of every selected fixture that shares the
 * master's profile id, at the profile's pan/tilt offsets (or its pt_patch). */
#ifndef LJ_CONTROLS_CTL_MOVEMENT_H
#define LJ_CONTROLS_CTL_MOVEMENT_H

#include <stdint.h>
#include "fixlib/lj_profile.h"
#include "ui/lj_window.h"

/* ---- 17 s1-s3: coordinate model, resolution, absolute / relative ---- */
enum { LJ_POS_LO = -32768, LJ_POS_HI = 32767, LJ_POS_HALF = 32768,   /* readout 0 == DMX16 32768 (ASSUMED "Homed (50% pan, 50% tilt)" H26 p135) */
       LJ_POS_RES16_DEFAULT = 32, LJ_POS_RES8_DEFAULT = 255,           /* INI [MOVEMENT] Res16Bit=32, Res8Bit=255 */
       LJ_POS_RES16_MAX = 256, LJ_POS_RES8_MAX = 255 };                /* RN 2.5: 16-bit factor max 128 -> 256; 8-bit bar 1..255 (ASSUMED) */
int32_t  lj_pos_clamp(int32_t v);                                       /* saturate to -32768..32767 */
uint16_t lj_pos_to_dmx16(int32_t v);                                    /* readout -> 16-bit pair value (v + 32768) */
int32_t  lj_pos_from_dmx16(uint16_t d);                                 /* d - 32768 */
uint8_t  lj_pos_to_dmx8(int32_t v);                                     /* 8-bit fixture output = the MSB, truncated (ASSUMED, CHM 3109) */
int32_t  lj_pos_from_dmx8(uint8_t x);                                   /* (x << 8) - 32768: LSB 0, IMG joystickmovement */
int32_t  lj_pos_step(int32_t v, int dir, int res);                      /* one arrow click / drag step: clamp(v + dir * res), readout units (ASSUMED unit) */
int32_t  lj_pos_relative(int32_t v, int32_t delta);                     /* relative movement: clamp(v + delta) (CHM 3105-3107) */
int32_t  lj_pos_snap8(int32_t v);                                       /* PROPOSAL (rig 17 s8 item 2): an 8-bit control keeps its readout on a DMX step: nearest multiple of 256 */
void     lj_pos_patch16(const lj_profile *p, uint8_t ofs[4]);           /* the four byte offsets pan MSB, pan LSB, tilt MSB, tilt LSB: pt_patch when pt_patch_on, else (pan_ofs, pan_ofs+1, tilt_ofs, tilt_ofs+1) (CHM 10497-10587) */

/* ---- 17 s5-s8: fan out, auto delay, macro index, cue macro scaling (pure; the W3 Fan Out / Auto delay dialogs and the engine's macro runner read them) ---- */
enum { LJ_POS_DELTA_UNIT = 256, LJ_POS_DELAY_FULL = 256, LJ_POS_N_TABLE = 4096, LJ_POS_RAW_MAX = 127 };
enum lj_pos_delay_style { LJ_POS_DELAY_LINEAR = 0, LJ_POS_DELAY_CIRCULAR, LJ_POS_DELAY_SEGMENTS };
int32_t lj_pos_fanout_offset(int n, int delta, int i);                  /* fixture i of n (n >= 2) in apply order: round(delta * 256 * (i - m) / m), m = (n - 1) / 2; -1 for n < 2 */
int     lj_pos_auto_delay(enum lj_pos_delay_style style, int n, int max_delay, int param, int k);   /* delay byte of fixture k of n; param = circular count / segments; floor, mod 256 */
int     lj_pos_delay_index(int delay_byte);                             /* N_TABLE * delay / 256 */
void    lj_pos_effective_index(int idx, int delay_byte, int tweak, int *pan_i, int *tilt_i);   /* delay subtracted from the index, tweak added to the tilt index only (ASSUMED) */
int     lj_pos_quantise(int idx, int step);                             /* Stepped Ellipse: (idx / step) * step mod N_TABLE */
int     lj_pos_scale_raw(int raw, int pct);                             /* cue macro control: floor(raw * pct / 100) clamped to 0..127 */

/* ---- the control window ---- */
/* client layout, px; measured from movement2.gif (332x284, 1:1, Martin's callouts cover the grid interior) and joystickmovement.gif, rig item */
enum { LJ_MV_CLIENT_W = 326, LJ_MV_CLIENT_H = 240,
       LJ_MV_PANEL_X = 2, LJ_MV_PANEL_Y = 2, LJ_MV_PANEL_W = 207,          /* the raised panel around the grid */
       LJ_MV_GRID_X = 11, LJ_MV_GRID_Y = 11, LJ_MV_GRID_W = 191,           /* the #8080FF field: 191 x 191, centre (95, 95) */
       LJ_MV_GRID_STEP = 20,                                                /* #000080 grid lines every ~20 px from the centre [07 s8] */
       LJ_MV_DOT_R = 5 };                                                   /* the white dot: 11 px across */
#define LJ_MV_CLR_GRID  0x8080FFu
#define LJ_MV_CLR_LINE  0x000080u
#define LJ_MV_CLR_CROSS 0xFF0000u

lj_window *lj_ctl_movement_create(const lj_profile *master);            /* the window for the master's profile (its LJ_CTL_MOVEMENT table caption, else "Movement 16 Bit"/"Movement 8 Bit" by pt16/pt8); NULL + log when the profile has no pan/tilt */
void       lj_ctl_movement_destroy(lj_window *win);
void       lj_ctl_movement_select(lj_window *win, int master, const int *fixtures, int n);   /* the fixtures the control applies to (indices into show->fixture; those with another profile id are skipped); master < 0 = none */
void       lj_ctl_movement_select_from_desktop(lj_window *win);         /* lj_sel_master() + every lj_sel_is_selected() fixture (the 100 ms hook calls it when lj_sel_serial() changed) */
void       lj_ctl_movement_refresh(lj_window *win);                     /* rereads the master's position and fade states from the current scene; repaints */
int        lj_ctl_movement_position(lj_window *win, int32_t *pan, int32_t *tilt);   /* the master's readout; -1 without a master or scene */
void       lj_ctl_movement_set(lj_window *win, int32_t pan, int32_t tilt);   /* move the master there, honouring Absolute/Relative and the locks (the grid, arrows and hotkeys end here) */
void       lj_ctl_movement_home(lj_window *win);                        /* 0 : 0, always absolute, locks ignored */
int        lj_ctl_movement_resolution(lj_window *win);                  /* the Resolution bar, 1..256 */
void       lj_ctl_movement_set_resolution(lj_window *win, int res);
int        lj_ctl_movement_absolute(lj_window *win);                    /* 1 Absolute Position, 0 Relative Movement */
void       lj_ctl_movement_set_absolute(lj_window *win, int absolute);
void       lj_ctl_movement_lock(lj_window *win, int pan_locked, int tilt_locked);
int        lj_ctl_movement_preset_record(lj_window *win, int n, int filter);   /* preset 1..99 from the current scene: filter 0 master, 1 selected, 2 all fixtures; other fixtures get top/left on first save [04 s4.3]; 0 / -1 */
int        lj_ctl_movement_preset_apply(lj_window *win, int n);         /* the selected fixtures take preset n: values + reference scene->preset[f] = n; -1 when unused */
void       lj_ctl_movement_preset_clear(lj_window *win);                /* dissolves the reference into the values (scene->preset[f] = 0) */
int        lj_ctl_movement_preset_delete(int n);                        /* every fixture's slot n unused; -1 out of range */
lj_window *lj_ctl_movement_presets_open(lj_window *win);                /* the Position Presets dialog (TPRESETFORM): No / Name list, N Entries, Master / Selected / All filter, Save / Assign / Clear */
lj_window *lj_ctl_movement_presets_window(lj_window *win);              /* NULL when closed */
int        lj_ctl_movement_readout(lj_window *win, char *dst, size_t cap);   /* the text under the grid: "-16256 : 17120" or "Preset 001" */

#endif /* LJ_CONTROLS_CTL_MOVEMENT_H */
