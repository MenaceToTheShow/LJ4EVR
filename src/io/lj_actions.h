/* lj_actions.h - the hotkey function table (design 2.5, 7.9; [01 s7], [05 s11.2],
 * F54): every row of the CHM "Hotkey functions" topic, grouped as the note
 * lists them, as enum lj_fn plus one const descriptor per row. This single
 * list is the backbone of every remote in LJ2 (keyboard, Fingers, 2532, MIDI
 * functions, 2518 mode 2, the cue list "Hotkey Command"), so it lives once.
 *
 * The list is an X-macro: LJ_FN_LIST(X) expands X(id, group, caption, n, flags)
 * once per function. Numbered families ("Fade Out 1-10 Sec", "Bump Sub 1-8",
 * "Toggle Slot# 1-12", "Fixture groups 1-20") are contiguous ranges whose caption
 * carries one %d fed with n; lj_fn_caption() formats it. Flags: enable (acts on
 * press), toggle, flash (`*` in the help: active while held, release = edge 0).
 * Programs (any sequence/cue/cue list/BG cue bound directly) are the four
 * LJ_FN_PROG_* rows; the bound number travels in lj_hotkey.prog_num. */
#ifndef LJ_IO_ACTIONS_H
#define LJ_IO_ACTIONS_H

#include <stddef.h>
#include <stdint.h>

enum lj_fn_group { LJ_FNG_SYSTEM = 0, LJ_FNG_INTENSITY, LJ_FNG_HTP, LJ_FNG_FIXTURE_CONTROLS, LJ_FNG_OTHER, LJ_FNG_FIXTURE_SELECTION,
                   LJ_FNG_SEQUENCE, LJ_FNG_CUE, LJ_FNG_CUE_CONTROL, LJ_FNG_CUE_BUILDER, LJ_FNG_GENMACRO, LJ_FNG_CUELIST, LJ_FNG_BGCUE,
                   LJ_FNG_STATICS, LJ_FNG_FIXTURE_GROUPS, LJ_FNG_DMXIN, LJ_FNG_PROGRAMS, LJ_FNG_COUNT };
extern const char *const lj_fn_group_names[LJ_FNG_COUNT];   /* "System", "Intensity", "HTP groups" ... the Edit Hotkeys function list headers */

#define LJ_FNF_ENABLE 1u
#define LJ_FNF_TOGGLE 2u
#define LJ_FNF_FLASH  4u
#define LJ_FNF_EXT    8u   /* W2 midi: an extension row, not in LJ2's function list (hidden from Edit Hotkeys) */

/* ranges: X(P_1 .. P_n, G, C, n, F) */
#define LJ_FN_R4(X,P,G,C,F)  X(P##_1,G,C,1,F) X(P##_2,G,C,2,F) X(P##_3,G,C,3,F) X(P##_4,G,C,4,F)
#define LJ_FN_R5(X,P,G,C,F)  LJ_FN_R4(X,P,G,C,F) X(P##_5,G,C,5,F)
#define LJ_FN_R8(X,P,G,C,F)  LJ_FN_R5(X,P,G,C,F) X(P##_6,G,C,6,F) X(P##_7,G,C,7,F) X(P##_8,G,C,8,F)
#define LJ_FN_R10(X,P,G,C,F) LJ_FN_R8(X,P,G,C,F) X(P##_9,G,C,9,F) X(P##_10,G,C,10,F)
#define LJ_FN_R12(X,P,G,C,F) LJ_FN_R10(X,P,G,C,F) X(P##_11,G,C,11,F) X(P##_12,G,C,12,F)
#define LJ_FN_R20(X,P,G,C,F) LJ_FN_R12(X,P,G,C,F) X(P##_13,G,C,13,F) X(P##_14,G,C,14,F) X(P##_15,G,C,15,F) X(P##_16,G,C,16,F) \
                             X(P##_17,G,C,17,F) X(P##_18,G,C,18,F) X(P##_19,G,C,19,F) X(P##_20,G,C,20,F)

#define LJ_FN_LIST(X) \
    /* System [01 s7] */ \
    X(LJ_FN_CLOSE_WINDOW,            LJ_FNG_SYSTEM, "Close Window",                 0, LJ_FNF_ENABLE) \
    X(LJ_FN_CLOSE_ALL_WINDOWS,       LJ_FNG_SYSTEM, "Close All Windows",            0, LJ_FNF_ENABLE) \
    X(LJ_FN_FREEZE_OUTPUT_FLASH,     LJ_FNG_SYSTEM, "Freeze Output",                0, LJ_FNF_FLASH) \
    X(LJ_FN_FREEZE_OUTPUT_TOGGLE,    LJ_FNG_SYSTEM, "Freeze Output Toggle",         0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FREEZE_BO_FLASH,         LJ_FNG_SYSTEM, "Freeze + B/O",                 0, LJ_FNF_FLASH) \
    X(LJ_FN_FREEZE_BO_TOGGLE,        LJ_FNG_SYSTEM, "Freeze + B/O toggle",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_DMX_OFFLINE,             LJ_FNG_SYSTEM, "DMX Offline",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_DMX_ONLINE,              LJ_FNG_SYSTEM, "DMX Online",                   0, LJ_FNF_ENABLE) \
    X(LJ_FN_DMX_ONLINE_OFFLINE_TOGGLE, LJ_FNG_SYSTEM, "DMX OnLine/OffLine toggle",  0, LJ_FNF_TOGGLE) \
    X(LJ_FN_2532_CONTROL,            LJ_FNG_SYSTEM, "2532 Control",                 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_VIRTUAL_FINGERS,         LJ_FNG_SYSTEM, "Virtual Fingers",              0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FINGERS_STATUS_PANEL,    LJ_FNG_SYSTEM, "Fingers status panel",         0, LJ_FNF_TOGGLE) \
    /* Intensity */ \
    X(LJ_FN_TOGGLE_MASTER_INTENSITY, LJ_FNG_INTENSITY, "Toggle Master Intensity Control", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_BLACKOUT_RESTORE,        LJ_FNG_INTENSITY, "Blackout/Restore",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_BLACKOUT,                LJ_FNG_INTENSITY, "Blackout",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_RESTORE,                 LJ_FNG_INTENSITY, "Restore",                   0, LJ_FNF_ENABLE) \
    X(LJ_FN_FADE_OUT_FADE_IN,        LJ_FNG_INTENSITY, "Fade Out/Fade In",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FADE_OUT,                LJ_FNG_INTENSITY, "Fade Out",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_FADE_IN,                 LJ_FNG_INTENSITY, "Fade In",                   0, LJ_FNF_ENABLE) \
    LJ_FN_R10(X, LJ_FN_FADE_OUT_SEC, LJ_FNG_INTENSITY, "Fade Out %d Sec",              LJ_FNF_ENABLE) \
    LJ_FN_R10(X, LJ_FN_FADE_IN_SEC,  LJ_FNG_INTENSITY, "Fade In %d Sec",               LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_OFF,              LJ_FNG_INTENSITY, "Master Intensity off",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_FULL,             LJ_FNG_INTENSITY, "Master Intensity full",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_PLUS1,            LJ_FNG_INTENSITY, "Master Intensity +1",       0, LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_PLUS10,           LJ_FNG_INTENSITY, "Master Intensity +10",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_MINUS1,           LJ_FNG_INTENSITY, "Master Intensity -1",       0, LJ_FNF_ENABLE) \
    X(LJ_FN_MASTER_MINUS10,          LJ_FNG_INTENSITY, "Master Intensity -10",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_BLACKOUT_FLASH,          LJ_FNG_INTENSITY, "BlackOut",                  0, LJ_FNF_FLASH) \
    X(LJ_FN_FADEOUT_FLASH,           LJ_FNG_INTENSITY, "FadeOut",                   0, LJ_FNF_FLASH) \
    X(LJ_FN_BUMP_MASTER_FLASH,       LJ_FNG_INTENSITY, "Bump Master",               0, LJ_FNF_FLASH) \
    LJ_FN_R8(X, LJ_FN_BUMP_SUB_FLASH, LJ_FNG_INTENSITY, "Bump Sub %d",                 LJ_FNF_FLASH) \
    X(LJ_FN_TOGGLE_BUMP_MASTER,      LJ_FNG_INTENSITY, "Toggle Bump Master",        0, LJ_FNF_TOGGLE) \
    LJ_FN_R8(X, LJ_FN_TOGGLE_BUMP_SUB, LJ_FNG_INTENSITY, "Toggle Bump Sub %d",         LJ_FNF_TOGGLE) \
    /* HTP groups (first 12 groups [05 s11.2]) */ \
    LJ_FN_R12(X, LJ_FN_HTP_BUMP_FLASH, LJ_FNG_HTP, "HTP group %d - Bump",              LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_HTP_TOGGLE,     LJ_FNG_HTP, "HTP group %d - Toggle",            LJ_FNF_TOGGLE) \
    /* Fixture controls */ \
    X(LJ_FN_CTL_INTENSITY,           LJ_FNG_FIXTURE_CONTROLS, "Intensity",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_MOVEMENT,            LJ_FNG_FIXTURE_CONTROLS, "Movement",           0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_COLOR,               LJ_FNG_FIXTURE_CONTROLS, "Color",              0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_GOBO,                LJ_FNG_FIXTURE_CONTROLS, "Gobo",               0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_BEAM,                LJ_FNG_FIXTURE_CONTROLS, "Beam",               0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_EFFECT,              LJ_FNG_FIXTURE_CONTROLS, "Effect",             0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_LEVEL_SPECIAL,       LJ_FNG_FIXTURE_CONTROLS, "Level/Special",      0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CTL_LAMP,                LJ_FNG_FIXTURE_CONTROLS, "Lamp",               0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_INTENSITY_PALETTE, LJ_FNG_FIXTURE_CONTROLS, "Toggle Intensity Palette", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_COLOR_PALETTE,    LJ_FNG_FIXTURE_CONTROLS, "Toggle Color Palette",  0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_GOBO_PALETTE,     LJ_FNG_FIXTURE_CONTROLS, "Toggle Gobo Palette",   0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_BEAM_PALETTE,     LJ_FNG_FIXTURE_CONTROLS, "Toggle Beam Palette",   0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_EFFECT_PALETTE,   LJ_FNG_FIXTURE_CONTROLS, "Toggle Effect Palette", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_LEVELS_PALETTE,   LJ_FNG_FIXTURE_CONTROLS, "Toggle Levels/Special Palette", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_POSITION_PRESET,  LJ_FNG_FIXTURE_CONTROLS, "Toggle Position Preset", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_ALL_PALETTES,     LJ_FNG_FIXTURE_CONTROLS, "Toggle All Palettes/Presets", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_PT_MACRO,                LJ_FNG_FIXTURE_CONTROLS, "P/T Macro",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FANOUT_CONTROL,          LJ_FNG_FIXTURE_CONTROLS, "FanOut Control",     0, LJ_FNF_TOGGLE) \
    X(LJ_FN_POSITION_OPTIONS,        LJ_FNG_FIXTURE_CONTROLS, "Position Options",   0, LJ_FNF_TOGGLE) \
    X(LJ_FN_MIRROR_PAN,              LJ_FNG_FIXTURE_CONTROLS, "Mirror Pan",         0, LJ_FNF_ENABLE) \
    X(LJ_FN_MIRROR_TILT,             LJ_FNG_FIXTURE_CONTROLS, "Mirror Tilt",        0, LJ_FNF_ENABLE) \
    X(LJ_FN_SWAP,                    LJ_FNG_FIXTURE_CONTROLS, "Swap",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_360_DEG,                 LJ_FNG_FIXTURE_CONTROLS, "360 deg",            0, LJ_FNF_ENABLE) \
    X(LJ_FN_PAN_RELATIVE_PLUS,       LJ_FNG_FIXTURE_CONTROLS, "Pan Relative +",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_PAN_RELATIVE_MINUS,      LJ_FNG_FIXTURE_CONTROLS, "Pan Relative -",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_TILT_RELATIVE_PLUS,      LJ_FNG_FIXTURE_CONTROLS, "Tilt Relative +",    0, LJ_FNF_ENABLE) \
    X(LJ_FN_TILT_RELATIVE_MINUS,     LJ_FNG_FIXTURE_CONTROLS, "Tilt Relative -",    0, LJ_FNF_ENABLE) \
    X(LJ_FN_HOME_SELECTED_FIXTURES,  LJ_FNG_FIXTURE_CONTROLS, "Home Selected Fixtures", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_ALL_CONTROLS,            LJ_FNG_FIXTURE_CONTROLS, "All Controls",       0, LJ_FNF_TOGGLE) \
    /* Other */ \
    X(LJ_FN_GENERIC_CONTROLS,        LJ_FNG_OTHER, "Generic Controls",              0, LJ_FNF_TOGGLE) \
    X(LJ_FN_SMOKE_CONTROL,           LJ_FNG_OTHER, "Smoke Control",                 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_SMOKE_BUTTON_FLASH,      LJ_FNG_OTHER, "Smoke Button",                  0, LJ_FNF_FLASH) \
    X(LJ_FN_TOGGLE_SMOKE_TIMER,      LJ_FNG_OTHER, "Toggle smoke timer",            0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CD_CONTROL,              LJ_FNG_OTHER, "CD Control",                    0, LJ_FNF_TOGGLE) \
    LJ_FN_R10(X, LJ_FN_FOLLOW_SPOT,  LJ_FNG_OTHER, "Follow spot %d",                   LJ_FNF_ENABLE) \
    X(LJ_FN_CLEAR_FOLLOWSPOT,        LJ_FNG_OTHER, "Clear Followspot",              0, LJ_FNF_ENABLE) \
    X(LJ_FN_VIEW_DMX_OUTPUT,         LJ_FNG_OTHER, "View DMX Output",               0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_NOTES,            LJ_FNG_OTHER, "Toggle Notes",                  0, LJ_FNF_TOGGLE) \
    X(LJ_FN_AUTOSTRIKE_LAMPS_TOGGLE, LJ_FNG_OTHER, "Autostrike lamps toggle",       0, LJ_FNF_TOGGLE) \
    LJ_FN_R10(X, LJ_FN_ASSIGN_PRESET, LJ_FNG_OTHER, "Assign Preset# %d",               LJ_FNF_ENABLE) \
    /* Fixture selection (from other topics) */ \
    X(LJ_FN_INCLUSIVE_EXCLUSIVE_TOGGLE, LJ_FNG_FIXTURE_SELECTION, "Inclusive/Exclusive toggle", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_NEXT_FIXTURE,            LJ_FNG_FIXTURE_SELECTION, "Next fixture",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_PREVIOUS_FIXTURE,        LJ_FNG_FIXTURE_SELECTION, "Previous fixture",  0, LJ_FNF_ENABLE) \
    X(LJ_FN_DESELECT_ALL,            LJ_FNG_FIXTURE_SELECTION, "De-select All",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_FIXTURE_SOLO_TOGGLE,     LJ_FNG_FIXTURE_SELECTION, "Toggle Fixture Solo", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FIXTURE_SOLO_FLASH,      LJ_FNG_FIXTURE_SELECTION, "Fixture Solo",      0, LJ_FNF_FLASH) \
    X(LJ_FN_SELECT_ALL_PROGRAMMED,   LJ_FNG_FIXTURE_SELECTION, "Select All Programmed", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_SELECT_LIKE_MASTER,      LJ_FNG_FIXTURE_SELECTION, "Select fixture like Master", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_SELECT_PROGRAMMED_OF_TYPE, LJ_FNG_FIXTURE_SELECTION, "Select All programmed of this type", 0, LJ_FNF_ENABLE) \
    /* Sequence */ \
    X(LJ_FN_SEQUENCE_CONTROL,        LJ_FNG_SEQUENCE, "Sequence Control",           0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_LIST_OF_SEQUENCES, LJ_FNG_SEQUENCE, "Toggle List of Sequences",  0, LJ_FNF_TOGGLE) \
    X(LJ_FN_NEXT_SCENE,              LJ_FNG_SEQUENCE, "Next Scene",                 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PREVIOUS_SCENE,          LJ_FNG_SEQUENCE, "Previous Scene",             0, LJ_FNF_ENABLE) \
    X(LJ_FN_ADD_SCENE,               LJ_FNG_SEQUENCE, "Add Scene",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_FIRST_SCENE,             LJ_FNG_SEQUENCE, "First Scene",                0, LJ_FNF_ENABLE) \
    X(LJ_FN_LAST_SCENE,              LJ_FNG_SEQUENCE, "Last Scene",                 0, LJ_FNF_ENABLE) \
    X(LJ_FN_QUICK_SAVE_SEQUENCE,     LJ_FNG_SEQUENCE, "Quick Save Sequence",        0, LJ_FNF_ENABLE) \
    X(LJ_FN_NEW_SEQUENCE,            LJ_FNG_SEQUENCE, "New Sequence",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_SET_SEQUENCE_BLIND,      LJ_FNG_SEQUENCE, "Set Sequence Blind",         0, LJ_FNF_ENABLE) \
    X(LJ_FN_SEQUENCE_BLIND_FLASH,    LJ_FNG_SEQUENCE, "Sequence Blind",             0, LJ_FNF_FLASH) \
    X(LJ_FN_CLEAR_SEQUENCE_BLIND,    LJ_FNG_SEQUENCE, "Clear Sequence Blind",       0, LJ_FNF_ENABLE) \
    X(LJ_FN_SEQUENCE_BLIND_TOGGLE,   LJ_FNG_SEQUENCE, "Sequence Blind Toggle",      0, LJ_FNF_TOGGLE) \
    X(LJ_FN_PLAYBACK_FWD,            LJ_FNG_SEQUENCE, "Playback Fwd",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_PLAYBACK_REV,            LJ_FNG_SEQUENCE, "Playback Rev",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_PLAYBACK_STOP,           LJ_FNG_SEQUENCE, "Playback Stop",              0, LJ_FNF_ENABLE) \
    /* Cue (list of cues) */ \
    X(LJ_FN_TOGGLE_LIST_OF_CUES,     LJ_FNG_CUE, "Toggle List of Cues",             0, LJ_FNF_TOGGLE) \
    X(LJ_FN_LOAD_SELECTED_CUE_GLOBAL, LJ_FNG_CUE, "Load Selected Cue (global list)", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_LOAD_SELECTED_CUE_PAGE,  LJ_FNG_CUE, "Load Selected Cue (Cue Page)",    0, LJ_FNF_ENABLE) \
    X(LJ_FN_FORCE_CUE_SELECTION,     LJ_FNG_CUE, "Force Cue# selection",            0, LJ_FNF_ENABLE) \
    X(LJ_FN_PREVIOUS_CUE_PAGE,       LJ_FNG_CUE, "Previous Cue Page",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_NEXT_CUE_PAGE,           LJ_FNG_CUE, "Next Cue Page",                   0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUE_PAGE_LATCH_PREVIOUS, LJ_FNG_CUE, "Cue Page Latch Previous Cue",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUE_PAGE_LATCH_NEXT,     LJ_FNG_CUE, "Cue Page Latch Next Cue",         0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUE_PAGE_SELECT_PREVIOUS, LJ_FNG_CUE, "Cue Page Select Previous Cue",   0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUE_PAGE_SELECT_NEXT,    LJ_FNG_CUE, "Cue Page Select Next Cue",        0, LJ_FNF_ENABLE) \
    /* Cue control */ \
    X(LJ_FN_TOGGLE_CUE_CONTROL,      LJ_FNG_CUE_CONTROL, "Toggle Cue Control",      0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_CHASE_MANUAL,     LJ_FNG_CUE_CONTROL, "Toggle Chase Manual",     0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_CHASE_FADE,       LJ_FNG_CUE_CONTROL, "Toggle Chase Fade",       0, LJ_FNF_TOGGLE) \
    X(LJ_FN_FLASH_SELECTED_SEQUENCE, LJ_FNG_CUE_CONTROL, "Flash selected Sequence", 0, LJ_FNF_FLASH) \
    X(LJ_FN_RELEASE_LATCHED_FLASH,   LJ_FNG_CUE_CONTROL, "Release latched Flash",   0, LJ_FNF_ENABLE) \
    X(LJ_FN_QUICK_SAVE_CUE,          LJ_FNG_CUE_CONTROL, "Quick Save Cue",          0, LJ_FNF_ENABLE) \
    X(LJ_FN_NEW_CLEAR_CUE,           LJ_FNG_CUE_CONTROL, "New/Clear Cue",           0, LJ_FNF_ENABLE) \
    X(LJ_FN_TOGGLE_SELECT_ALL,       LJ_FNG_CUE_CONTROL, "Toggle Select All",       0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CUE_TIME_CONTROL,        LJ_FNG_CUE_CONTROL, "Cue Time Control",        0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CUE_LOOP_CONTROL,        LJ_FNG_CUE_CONTROL, "Cue Loop Control",        0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CUE_MACRO_CONTROL_TOGGLE, LJ_FNG_CUE_CONTROL, "Cue Macro Control toggle", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_PT_MACRO_FADE_IN,        LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Fade In", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_FADEOUT,        LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Fadeout", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_OFF,            LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Macro Off", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_ON,             LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Macro On", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_RESET_AMPLITUDE, LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Reset Amplitude", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_RESET_SPEED,    LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Reset Speed", 0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_FREEZE,         LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Freeze",  0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_RUN,            LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Run",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_PT_MACRO_TOGGLE_FREEZE,  LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Toggle Freeze", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_PT_MACRO_FREEZE_FLASH,   LJ_FNG_CUE_CONTROL, "Pan/Tilt Macro: Freeze",  0, LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TOGGLE_SLOT_SELECTION, LJ_FNG_CUE_CONTROL, "Toggle Slot# %d (selection)", LJ_FNF_TOGGLE) \
    X(LJ_FN_ALL_SLOTS_OFF,           LJ_FNG_CUE_CONTROL, "All Slots Off",           0, LJ_FNF_ENABLE) \
    X(LJ_FN_ALL_SLOTS_ON,            LJ_FNG_CUE_CONTROL, "All Slots On",            0, LJ_FNF_ENABLE) \
    LJ_FN_R12(X, LJ_FN_TOGGLE_SLOT_ENABLE, LJ_FNG_CUE_CONTROL, "Toggle Slot# %d (enable)", LJ_FNF_TOGGLE) \
    LJ_FN_R12(X, LJ_FN_CLEAR_SEQUENCE, LJ_FNG_CUE_CONTROL, "Clear Sequence # %d",      LJ_FNF_ENABLE) \
    LJ_FN_R12(X, LJ_FN_SEQ_STOP,     LJ_FNG_CUE_CONTROL, "Seq # %d Stop",               LJ_FNF_ENABLE) \
    LJ_FN_R12(X, LJ_FN_SEQ_GO,       LJ_FNG_CUE_CONTROL, "Seq # %d Go",                 LJ_FNF_ENABLE) \
    LJ_FN_R12(X, LJ_FN_SEQ_STOP_GO_TOGGLE, LJ_FNG_CUE_CONTROL, "Seq # %d Stop-Go Toggle", LJ_FNF_TOGGLE) \
    X(LJ_FN_ALL_SEQUENCES_STOP,      LJ_FNG_CUE_CONTROL, "All Sequences Stop",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_ALL_SEQUENCES_GO,        LJ_FNG_CUE_CONTROL, "All Sequences Go",        0, LJ_FNF_ENABLE) \
    X(LJ_FN_ALL_SEQUENCES_STOP_GO,   LJ_FNG_CUE_CONTROL, "All Sequences Stop-Go",   0, LJ_FNF_TOGGLE) \
    X(LJ_FN_EMULATE_AUDIO_TRIG,      LJ_FNG_CUE_CONTROL, "Emulate Audio Trig",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_TRIG_SEL_MANUAL_DEFAULT, LJ_FNG_CUE_CONTROL, "Trig Selected Manual Default", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_MANUAL_FWD,     LJ_FNG_CUE_CONTROL, "Trig Selected Manual Fwd", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_MANUAL_REV,     LJ_FNG_CUE_CONTROL, "Trig Selected Manual Rev", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_MANUAL_RND,     LJ_FNG_CUE_CONTROL, "Trig Selected Manual Rnd", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_MANUAL_BOUNCE,  LJ_FNG_CUE_CONTROL, "Trig Selected Manual Bounce", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_AUTO_DEFAULT,   LJ_FNG_CUE_CONTROL, "Trig Selected Auto Default", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_AUTO_FWD,       LJ_FNG_CUE_CONTROL, "Trig Selected Auto Fwd",  0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_AUTO_REV,       LJ_FNG_CUE_CONTROL, "Trig Selected Auto Rev",  0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_AUTO_RND,       LJ_FNG_CUE_CONTROL, "Trig Selected Auto Rnd",  0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_AUTO_BOUNCE,    LJ_FNG_CUE_CONTROL, "Trig Selected Auto Bounce", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_ANY_DEFAULT,    LJ_FNG_CUE_CONTROL, "Trig Selected Default",   0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_ANY_FWD,        LJ_FNG_CUE_CONTROL, "Trig Selected Fwd",       0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_ANY_REV,        LJ_FNG_CUE_CONTROL, "Trig Selected Rev",       0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_ANY_RND,        LJ_FNG_CUE_CONTROL, "Trig Selected Rnd",       0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_SEL_ANY_BOUNCE,     LJ_FNG_CUE_CONTROL, "Trig Selected Bounce",    0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_ALL_MANUAL_DEFAULT, LJ_FNG_CUE_CONTROL, "Trig All Manual Default", 0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_ALL_MANUAL_FWD,     LJ_FNG_CUE_CONTROL, "Trig All Manual Fwd",     0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_ALL_MANUAL_REV,     LJ_FNG_CUE_CONTROL, "Trig All Manual Rev",     0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_ALL_MANUAL_RND,     LJ_FNG_CUE_CONTROL, "Trig All Manual Rnd",     0, LJ_FNF_FLASH) \
    X(LJ_FN_TRIG_ALL_MANUAL_BOUNCE,  LJ_FNG_CUE_CONTROL, "Trig All Manual Bounce",  0, LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TRIG_SEQ_DEFAULT, LJ_FNG_CUE_CONTROL, "Trig Seq# %d Default",  LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TRIG_SEQ_FWD,     LJ_FNG_CUE_CONTROL, "Trig Seq# %d Fwd",      LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TRIG_SEQ_REV,     LJ_FNG_CUE_CONTROL, "Trig Seq# %d Rev",      LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TRIG_SEQ_RND,     LJ_FNG_CUE_CONTROL, "Trig Seq# %d Rnd",      LJ_FNF_FLASH) \
    LJ_FN_R12(X, LJ_FN_TRIG_SEQ_BOUNCE,  LJ_FNG_CUE_CONTROL, "Trig Seq# %d Bounce",   LJ_FNF_FLASH) \
    /* Cue builder */ \
    X(LJ_FN_TOGGLE_CUE_BUILDER,      LJ_FNG_CUE_BUILDER, "Toggle Cue Builder",      0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_CUE_CAPTURE,      LJ_FNG_CUE_BUILDER, "Toggle Cue Capture Activate", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_SEQ_CAPTURE,      LJ_FNG_CUE_BUILDER, "Toggle Seq Capture Activate", 0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CUE_STACK_GO,            LJ_FNG_CUE_BUILDER, "Cue Stack Go",            0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUE_PRESET_GO,           LJ_FNG_CUE_BUILDER, "Cue Preset Go",           0, LJ_FNF_ENABLE) \
    X(LJ_FN_GET_CUE,                 LJ_FNG_CUE_BUILDER, "Get Cue",                 0, LJ_FNF_ENABLE) \
    X(LJ_FN_CLEAR_CUE_STACK,         LJ_FNG_CUE_BUILDER, "Clear Cue Stack",         0, LJ_FNF_ENABLE) \
    X(LJ_FN_CLEAR_ALL_SEQUENCES,     LJ_FNG_CUE_BUILDER, "Clear All Sequences",     0, LJ_FNF_ENABLE) \
    /* Generic macro [05 s11.2] */ \
    X(LJ_FN_GENMACRO_ENGINE_OFF,     LJ_FNG_GENMACRO, "Engine Off",                 0, LJ_FNF_ENABLE) \
    X(LJ_FN_GENMACRO_ENGINE_ON,      LJ_FNG_GENMACRO, "Engine On",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_GENMACRO_ENGINE_TOGGLE,  LJ_FNG_GENMACRO, "Engine Toggle",              0, LJ_FNF_TOGGLE) \
    X(LJ_FN_GENMACRO_TOGGLE_EDITOR,  LJ_FNG_GENMACRO, "Toggle Editor",              0, LJ_FNF_TOGGLE) \
    X(LJ_FN_GENMACRO_TOGGLE_LIST,    LJ_FNG_GENMACRO, "Toggle List",                0, LJ_FNF_TOGGLE) \
    X(LJ_FN_GENMACRO_CLEAR_EDITOR,   LJ_FNG_GENMACRO, "Clear Editor",               0, LJ_FNF_ENABLE) \
    X(LJ_FN_GENMACRO_SLOT_OFF,       LJ_FNG_GENMACRO, "Slot Off",                   0, LJ_FNF_ENABLE) \
    X(LJ_FN_GENMACRO_SLOT_ON,        LJ_FNG_GENMACRO, "Slot On",                    0, LJ_FNF_ENABLE) \
    X(LJ_FN_GENMACRO_SLOT_TOGGLE,    LJ_FNG_GENMACRO, "Slot Toggle",                0, LJ_FNF_TOGGLE) \
    /* Cue list [01 s7, 05 s11.2] */ \
    X(LJ_FN_CUELIST_CONTROL,         LJ_FNG_CUELIST, "CueList Control",             0, LJ_FNF_TOGGLE) \
    X(LJ_FN_TOGGLE_LIST_OF_CUELISTS, LJ_FNG_CUELIST, "Toggle List of Cuelists",     0, LJ_FNF_TOGGLE) \
    X(LJ_FN_QUICK_SAVE_CUELIST,      LJ_FNG_CUELIST, "Quick Save CueList",          0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_START_TOP,       LJ_FNG_CUELIST, "Cuelist Start Top",           0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_START_SELECTED,  LJ_FNG_CUELIST, "Cuelist Start Selected",      0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_GO,              LJ_FNG_CUELIST, "Cuelist Go",                  0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_BACK,            LJ_FNG_CUELIST, "Cuelist Back",                0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_STOP,            LJ_FNG_CUELIST, "Cuelist Stop",                0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_STOP_AND_CLEAR,  LJ_FNG_CUELIST, "Cuelist Stop and Clear",      0, LJ_FNF_ENABLE) \
    LJ_FN_R4(X, LJ_FN_CUELIST_GOTO_MARK, LJ_FNG_CUELIST, "Cuelist Goto Mark %d",       LJ_FNF_ENABLE) \
    LJ_FN_R4(X, LJ_FN_CUELIST_RUN_FROM_MARK, LJ_FNG_CUELIST, "Cuelist Run from Mark %d", LJ_FNF_ENABLE) \
    X(LJ_FN_LOG_TIMECODE,            LJ_FNG_CUELIST, "Log Timecode",                0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_SELECT_PREVIOUS, LJ_FNG_CUELIST, "Cuelist select previous",     0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_SELECT_NEXT,     LJ_FNG_CUELIST, "Cuelist select next",         0, LJ_FNF_ENABLE) \
    X(LJ_FN_CUELIST_SELECT_START,    LJ_FNG_CUELIST, "Cuelist select start",        0, LJ_FNF_ENABLE) \
    /* Background cue [03 s7] */ \
    X(LJ_FN_TOGGLE_BGCUE_CONTROL,    LJ_FNG_BGCUE, "Toggle BGCue control",          0, LJ_FNF_TOGGLE) \
    X(LJ_FN_CLEAR_BGCUE,             LJ_FNG_BGCUE, "Clear BGCue",                   0, LJ_FNF_ENABLE) \
    X(LJ_FN_TOGGLE_BG_CUE_LIST,      LJ_FNG_BGCUE, "Toggle BG Cue List",            0, LJ_FNF_TOGGLE) \
    LJ_FN_R5(X, LJ_FN_BGCUE_TOGGLE_SLOT, LJ_FNG_BGCUE, "BGCue Toggle slot %d",          LJ_FNF_TOGGLE) \
    X(LJ_FN_BGCUE_ALL_SLOTS_OFF,     LJ_FNG_BGCUE, "BGCue All Slots Off",           0, LJ_FNF_ENABLE) \
    X(LJ_FN_BGCUE_ALL_SLOTS_ON,      LJ_FNG_BGCUE, "BGCue All Slots On",            0, LJ_FNF_ENABLE) \
    X(LJ_FN_LOAD_SELECTED_BG_CUE,    LJ_FNG_BGCUE, "Load Selected BG Cue",          0, LJ_FNF_ENABLE) \
    /* Statics [03 s8] */ \
    X(LJ_FN_STATICS_CONTROL,         LJ_FNG_STATICS, "Statics control",             0, LJ_FNF_TOGGLE) \
    LJ_FN_R10(X, LJ_FN_STATIC,       LJ_FNG_STATICS, "Static %02d",                     LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_RELEASE_STATICS,         LJ_FNG_STATICS, "Release statics",             0, LJ_FNF_ENABLE) \
    /* Fixture groups [05 s11.2] */ \
    X(LJ_FN_TOGGLE_GROUP_VIEW,       LJ_FNG_FIXTURE_GROUPS, "Toggle Group View",    0, LJ_FNF_TOGGLE) \
    LJ_FN_R20(X, LJ_FN_FIXTURE_GROUP, LJ_FNG_FIXTURE_GROUPS, "Fixture group %d",         LJ_FNF_ENABLE) \
    /* DMX in [05 s11.2] */ \
    X(LJ_FN_DMXIN_FUNCTIONS_DISABLE, LJ_FNG_DMXIN, "DMXin Functions Disable",       0, LJ_FNF_ENABLE) \
    X(LJ_FN_DMXIN_FUNCTIONS_ENABLE,  LJ_FNG_DMXIN, "DMXin Functions Enable",        0, LJ_FNF_ENABLE) \
    X(LJ_FN_DMXIN_FUNCTIONS_TOGGLE,  LJ_FNG_DMXIN, "DMXin Functions Toggle",        0, LJ_FNF_TOGGLE) \
    /* Programs: the Edit Hotkeys program tabs Sequence (Flash) | Cue | Cuelist | BG Cue; the number is lj_hotkey.prog_num */ \
    X(LJ_FN_PROG_SEQ_FLASH,          LJ_FNG_PROGRAMS, "Sequence (Flash)",           0, LJ_FNF_FLASH) \
    X(LJ_FN_PROG_CUE,                LJ_FNG_PROGRAMS, "Cue",                        0, LJ_FNF_ENABLE) \
    X(LJ_FN_PROG_CUELIST,            LJ_FNG_PROGRAMS, "Cuelist",                    0, LJ_FNF_ENABLE) \
    X(LJ_FN_PROG_BGCUE,              LJ_FNG_PROGRAMS, "BG Cue",                     0, LJ_FNF_ENABLE)     /* W2 appended rows (append-only, 01 s7 group order kept inside the block). The 01 s7 groups were complete in W1; the exe
     * carries `Static 01` .. `Static 20` hotkey names [14 sB.2, EXE 0x3c6cf0-0x3ca27a] where the CHM lists 01-10, so rows 11-20
     * are appended here (Statics 2 pages map onto the 20 numbered functions on the current page, PROPOSAL rig 14 sB.8 item 9).
     * Snapshot, hotkey setups and MIDI have no hotkey rows in any note; none are invented. */ \
    X(LJ_FN_STATIC_11,               LJ_FNG_STATICS, "Static %02d",                11, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_12,               LJ_FNG_STATICS, "Static %02d",                12, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_13,               LJ_FNG_STATICS, "Static %02d",                13, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_14,               LJ_FNG_STATICS, "Static %02d",                14, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_15,               LJ_FNG_STATICS, "Static %02d",                15, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_16,               LJ_FNG_STATICS, "Static %02d",                16, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_17,               LJ_FNG_STATICS, "Static %02d",                17, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_18,               LJ_FNG_STATICS, "Static %02d",                18, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_19,               LJ_FNG_STATICS, "Static %02d",                19, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    X(LJ_FN_STATIC_20,               LJ_FNG_STATICS, "Static %02d",                20, LJ_FNF_TOGGLE | LJ_FNF_FLASH) \
    /* W2 midi track (append-only): the MIDI CC extension target (05 s7 clone verdict, marked non-original, off by default). LJ_FNF_EXT = not one of LJ2's hotkey functions: Edit Hotkeys and the MIDI-IN Functions list skip it; prog_num = the level in percent 0..100 (CC value 0..127 scaled by lj_midi_dispatch). */ \
    X(LJ_FN_MASTER_LEVEL,            LJ_FNG_INTENSITY, "Master Intensity level",    0, LJ_FNF_EXT)

#define LJ_FN_ENUM(id, g, c, n, f) id,
enum lj_fn { LJ_FN_NONE = 0, LJ_FN_LIST(LJ_FN_ENUM) LJ_FN_COUNT };
#undef LJ_FN_ENUM

typedef struct { const char *name; uint8_t group; const char *caption; uint8_t n; uint8_t flags; } lj_fn_desc;   /* name = the enum identifier (the .Hot file spelling); caption may hold one %d fed with n */
extern const lj_fn_desc lj_fn_table[LJ_FN_COUNT];      /* lj_actions.c; row 0 = LJ_FN_NONE */

int         lj_fn_caption(enum lj_fn fn, char *dst, size_t cap);   /* "Fade Out 3 Sec"; 0 / -1 */
enum lj_fn  lj_fn_by_name(const char *name);                       /* "LJ_FN_CLOSE_WINDOW" -> the enum, LJ_FN_NONE when unknown */
int         lj_fn_is_flash(enum lj_fn fn);                         /* LJ_FNF_FLASH set */

#endif /* LJ_IO_ACTIONS_H */
