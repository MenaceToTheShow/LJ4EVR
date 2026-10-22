/* lj_generics.c - the generic built-in profiles, transcribed from
 * 15-builtin-profiles/10-generic-profiles.md (CHM 2.209 genericdmxprofiles.htm,
 * genericrgbpack.htm, controllingdmxsmokemachines.htm; the shipped notes files
 * GDMXnNotes.Rtf, Switch.Rtf, RGBPackNotes.Rtf, Smoke.Rtf, HazerNotes.Rtf,
 * SwitchBox6Notes.Rtf, ExternalApp.Rtf; the exe classes TLevelControl_Dimmer1..32,
 * _Switch6, _Smoke, _Hazer, TColorControl_CKx). Each profile's exe triple is on its line.
 * Footprint = the channel count in the mode string (dimmer, switch), 3 x units (RGB
 * pack), 1 (smoke), 2 (hazer), 6 (SwitchBox, inferred from the name), 32 (External:
 * "needs a valid address and 32 channels", CHM scribble8900.htm). The Levels window is the
 * only window of a dimmer / switch / smoke / hazer profile [15/10 s1]; the RGB pack's
 * Colors window (TColorControl_CKx) is LJ_CTL_RGBPACK, whose control lands in W2
 * (LORD_FIDELITY-18). Channel classes: LEVEL (scaled by the masters, blacked out),
 * SWITCH (on = 255, off = 0, no Fade, unscaled until a master is 0: ME-25), SMOKE,
 * HAZE, HAZE_FAN (layer 7, ME-25). */
#include "fixlib/hand/lj_generics.h"
#include "fixlib/hand/lj_hand.h"

#define LJ_R2(x)  x, x
#define LJ_R4(x)  LJ_R2(x), LJ_R2(x)
#define LJ_R8(x)  LJ_R4(x), LJ_R4(x)
#define LJ_R16(x) LJ_R8(x), LJ_R8(x)
#define LJ_R1(x)  x
#define LJ_R6(x)  LJ_R4(x), LJ_R2(x)
#define LJ_R12(x) LJ_R8(x), LJ_R4(x)
#define LJ_R24(x) LJ_R16(x), LJ_R8(x)
#define LJ_R30(x) LJ_R16(x), LJ_R8(x), LJ_R4(x), LJ_R2(x)
#define LJ_R32(x) LJ_R16(x), LJ_R16(x)
#define LJ_R18(x) LJ_R16(x), LJ_R2(x)

/* One-window tables: Levels over the shared CH1..CHn rows (dimmer: Fade default; switch: on/off, OFF|SNAP). */
#define LJ_LVWIN(n) static const lj_ctl_window lj_gn_lvwin_##n[1] = { { LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, lj_hand_level_rows, n, NULL, 0 } }; \
                    static const lj_ctl_window lj_gn_swwin_##n[1] = { { LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, lj_hand_switch_rows, n, NULL, 0 } }
LJ_LVWIN(1); LJ_LVWIN(2); LJ_LVWIN(4); LJ_LVWIN(6); LJ_LVWIN(8); LJ_LVWIN(12); LJ_LVWIN(16); LJ_LVWIN(24); LJ_LVWIN(30); LJ_LVWIN(32);

/* Generic / DMX Dimmer / " n Channel" (ids 19 70 56 71 21 72 23 20 73 22; tag DMXn; GDMXnNotes.Rtf): n faders, GM-scaled.
 * Generic level channels follow the Grand Master by default (15/10 s6, CHM genericdmxprofiles.htm): ext_gm per channel, the
 * engine's S3 "Extended GM" path (per-channel Enable/Disable Grand Master Control is the W3 Channel Configuration dialog). */
#define LJ_DIMMER(id_, n, tag_, mode_) { .id = id_, .tag = tag_, .manufacturer = "Generic", .name = "DMX Dimmer", .mode = mode_, \
    .footprint = n, .channel_class = { LJ_R##n(LJ_CH_LEVEL) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { LJ_R##n(1) }, \
    .windows = lj_gn_lvwin_##n, .nwindows = 1, .notes = "Generic DMX dimmer or switch control, " #n " control channel(s); 8-bit virtual faders, right-click a fader for All (8+ channels)." }
/* Generic / DMX Switch Pack / " n Channel" (ids 92-101; tag SW-n; Switch.Rtf): on/off control only, added in 2.5. */
#define LJ_SWITCH(id_, n, tag_, mode_) { .id = id_, .tag = tag_, .manufacturer = "Generic", .name = "DMX Switch Pack", .mode = mode_, \
    .footprint = n, .channel_class = { LJ_R##n(LJ_CH_SWITCH) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 0, \
    .windows = lj_gn_swwin_##n, .nwindows = 1, .notes = "Generic DMX switch, " #n " channel(s): on = 255, off = 0, no Fade state; not scaled by the masters until a master reaches 0." }

static const lj_profile lj_gn_dmx1  = LJ_DIMMER(19, 1,  "DMX1",  " 1 Channel");
static const lj_profile lj_gn_dmx2  = LJ_DIMMER(70, 2,  "DMX2",  " 2 Channel");
static const lj_profile lj_gn_dmx4  = LJ_DIMMER(56, 4,  "DMX4",  " 4 Channel");
static const lj_profile lj_gn_dmx6  = LJ_DIMMER(71, 6,  "DMX6",  " 6 Channel");
static const lj_profile lj_gn_dmx8  = LJ_DIMMER(21, 8,  "DMX8",  " 8 Channel");
static const lj_profile lj_gn_dmx12 = LJ_DIMMER(72, 12, "DMX12", "12 Channel");
static const lj_profile lj_gn_dmx16 = LJ_DIMMER(23, 16, "DMX16", "16 Channel");
static const lj_profile lj_gn_dmx24 = LJ_DIMMER(20, 24, "DMX24", "24 Channel");
static const lj_profile lj_gn_dmx30 = LJ_DIMMER(73, 30, "DMX30", "30 Channel");
static const lj_profile lj_gn_dmx32 = LJ_DIMMER(22, 32, "DMX32", "32 Channel");
static const lj_profile lj_gn_sw1  = LJ_SWITCH(92,  1,  "SW-1",  " 1 Channel");
static const lj_profile lj_gn_sw2  = LJ_SWITCH(93,  2,  "SW-2",  " 2 Channel");
static const lj_profile lj_gn_sw4  = LJ_SWITCH(94,  4,  "SW-4",  " 4 Channel");
static const lj_profile lj_gn_sw6  = LJ_SWITCH(95,  6,  "SW-6",  " 6 Channel");
static const lj_profile lj_gn_sw8  = LJ_SWITCH(96,  8,  "SW-8",  " 8 Channel");
static const lj_profile lj_gn_sw12 = LJ_SWITCH(97,  12, "SW-12", "12 Channel");
static const lj_profile lj_gn_sw16 = LJ_SWITCH(98,  16, "SW-16", "16 Channel");
static const lj_profile lj_gn_sw24 = LJ_SWITCH(99,  24, "SW-24", "24 Channel");
static const lj_profile lj_gn_sw30 = LJ_SWITCH(100, 30, "SW-30", "30 Channel");
static const lj_profile lj_gn_sw32 = LJ_SWITCH(101, 32, "SW-32", "32 Channel");

/* Generic / RGB Pack / "x  4 (A)" .. "x 10" (ids 78 135 77 76 75; tags RGBx4 RGBx4 RGBx6 RGBx8 RGBx10; RGBPackNotes.Rtf):
 * up to 30 contiguous channels mapped Red, Green, Blue, Red, ... (Color Kinetics iColor/iCove style); layouts A and B of the
 * x4 pack differ only in the status-panel arrangement. Rows R1 G1 B1 .. are ours (PROPOSAL) until the W2 control defines its own. */
#define LJ_RGBROWS(u, o) { LJ_W_LEVEL_FADER, o, 1, "R" #u, NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL }, \
                          { LJ_W_LEVEL_FADER, o + 1, 1, "G" #u, NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL }, \
                          { LJ_W_LEVEL_FADER, o + 2, 1, "B" #u, NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL }
static const lj_ctl_row lj_gn_rgb_rows[30] = { LJ_RGBROWS(1, 0), LJ_RGBROWS(2, 3), LJ_RGBROWS(3, 6), LJ_RGBROWS(4, 9), LJ_RGBROWS(5, 12),
                                               LJ_RGBROWS(6, 15), LJ_RGBROWS(7, 18), LJ_RGBROWS(8, 21), LJ_RGBROWS(9, 24), LJ_RGBROWS(10, 27) };
#define LJ_RGBWIN(n) static const lj_ctl_window lj_gn_rgbwin_##n[1] = { { LJ_CTL_RGBPACK, "Colors", lj_ctl_menu_rgb, lj_gn_rgb_rows, n, NULL, 0 } }
LJ_RGBWIN(12); LJ_RGBWIN(18); LJ_RGBWIN(24); LJ_RGBWIN(30);
#define LJ_RGBPACK(id_, n, tag_, mode_) { .id = id_, .tag = tag_, .manufacturer = "Generic", .name = "RGB Pack", .mode = mode_, \
    .footprint = n, .channel_class = { LJ_R##n(LJ_CH_LEVEL) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { LJ_R##n(1) }, \
    .cmy = { { 1, 0, "Red", 0, 255 }, { 1, 1, "Green", 0, 255 }, { 1, 2, "Blue", 0, 255 } }, .rgb_on = 1, \
    .windows = lj_gn_rgbwin_##n, .nwindows = 1, .notes = "Pack of RGB-only fixtures, " #n " contiguous channels mapped Red, Green, Blue, Red, ...; one square per unit in the Colors window." }
static const lj_profile lj_gn_rgb4a = LJ_RGBPACK(78,  12, "RGBx4",  "x  4 (A)");
static const lj_profile lj_gn_rgb4b = LJ_RGBPACK(135, 12, "RGBx4",  "x  4 (B)");
static const lj_profile lj_gn_rgb6  = LJ_RGBPACK(77,  18, "RGBx6",  "x  6");
static const lj_profile lj_gn_rgb8  = LJ_RGBPACK(76,  24, "RGBx8",  "x  8");
static const lj_profile lj_gn_rgb10 = LJ_RGBPACK(75,  30, "RGBx10", "x 10");

/* Generic / DMX Smoke Machine (id 18, tag SMOKE, Smoke.Rtf): one channel, 0 = none, 255 = full; linked to the global Smoke Control
 * window (TLevelControl_Smoke: "Smoke Level", "Smoke control active!" when the global control owns the channel). */
static const lj_ctl_row lj_gn_smoke_rows[] = { { LJ_W_LEVEL_FADER, 0, 1, "Smoke Level", NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL } };
static const lj_ctl_window lj_gn_smoke_win[] = { { LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, lj_gn_smoke_rows, 1, NULL, 0 } };
static const lj_profile lj_gn_smoke = { .id = 18, .tag = "SMOKE", .manufacturer = "Generic", .name = "DMX Smoke Machine", .mode = "",
    .footprint = 1, .channel_class = { LJ_CH_SMOKE }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { 1 }, .level_caption = { "Smoke" },
    .windows = lj_gn_smoke_win, .nwindows = 1,
    .notes = "Links the smoke machine to the Smoke Control window. The machine must be controllable through one DMX channel: zero output at 0, full output at 255." };

/* Generic / DMX Hazer (id 45, tag HAZER, HazerNotes.Rtf): haze level + optional fan (Jem Hydrosonic 2000 protocol), linked to the
 * Smoke Control window's Hazer group (TLevelControl_Hazer: "Hazer Level", "Hazer Fan"). */
static const lj_ctl_row lj_gn_hazer_rows[] = { { LJ_W_LEVEL_FADER, 0, 1, "Hazer Level", NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL },
                                               { LJ_W_LEVEL_FADER, 1, 1, "Hazer Fan",   NULL, 0, 255, 0, LJ_HAND_FADE, NULL, 0, NULL, LJ_STATES_ALL } };
static const lj_ctl_window lj_gn_hazer_win[] = { { LJ_CTL_LEVELS, "Levels", lj_ctl_menu_std, lj_gn_hazer_rows, 2, NULL, 0 } };
static const lj_profile lj_gn_hazer = { .id = 45, .tag = "HAZER", .manufacturer = "Generic", .name = "DMX Hazer", .mode = "",
    .footprint = 2, .channel_class = { LJ_CH_HAZE, LJ_CH_HAZE_FAN }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { 1, 1 }, .level_caption = { "Haze", "Fan" },
    .windows = lj_gn_hazer_win, .nwindows = 1,
    .notes = "Two-channel DMX hazer: channel 1 haze output level, channel 2 fan control (optional). Links the hazer to the Smoke Control window." };

/* Martin / 6 Channel DMX switch (id 27, tag SWITCH6, SwitchBox6Notes.Rtf; TLevelControl_Switch6): six on/off channels, footprint inferred from the name. */
static const lj_profile lj_gn_switch6 = { .id = 27, .tag = "SWITCH6", .manufacturer = "Martin", .name = "6 Channel DMX switch", .mode = "",
    .footprint = 6, .channel_class = { LJ_R6(LJ_CH_SWITCH) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 0,
    .windows = lj_gn_swwin_6, .nwindows = 1, .notes = "Martin 6-Channel SwitchBox: six on/off channels (on = 255, off = 0)." };

/* Other / External Application/Device (id 90, tag EXTERNAL, ExternalApp.Rtf): a plug-in link (<= 32-char command strings per scene,
 * 4 DMX out + 4 DMX in channels); no control window in W1 (TSpecialSettings_ExternalApp is W5 scope). */
static const lj_profile lj_gn_external = { .id = 90, .tag = "EXTERNAL", .manufacturer = "Other", .name = "External Application/Device", .mode = "",
    .footprint = 32, .channel_class = { LJ_R32(LJ_CH_SPECIAL) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 0, .level_invisible = { LJ_R32(1) },
    .windows = NULL, .nwindows = 0, .notes = "Links an external program into LightJockey through a plug-in (LJ-RS232Send ships with LJ2); needs a valid address and 32 channels." };

/* User Definable placeholders (ColorScroll1Notes, Scanner1Notes, GenericFixture2Notes; note 02 s5): the profile a fixture has before
 * Define Fixture Profile replaces it. Generic Fixture 2 "by default only has generic level control enabled (8 bit virtual fader
 * control of all 32 channels)" [CHM L9983-10049]; Generic Scanner 1 is 8-bit, max 8 channels; Color Scroller 1 has one intensity channel. */
static const lj_profile lj_gn_gcs1  = { .id = 42, .tag = "GCS 1", .manufacturer = "User Definable", .name = "Color Scroller 1", .mode = "",
    .footprint = 1, .channel_class = { LJ_CH_LEVEL }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { 1 }, .windows = lj_gn_lvwin_1, .nwindows = 1,
    .notes = "Color Scroller 1 placeholder: one intensity channel at the fixture address plus a separate scroller address set in Define Fixture Profile (.SC1, not imported)." };
static const lj_profile lj_gn_gsc1  = { .id = 43, .tag = "GSC 1", .manufacturer = "User Definable", .name = "Generic Scanner 1", .mode = "",
    .footprint = 8, .channel_class = { LJ_R8(LJ_CH_LEVEL) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { LJ_R8(1) }, .windows = lj_gn_lvwin_8, .nwindows = 1,
    .notes = "Generic Scanner 1 placeholder: 8-bit, max 8 channels, backwards compatibility only (.SS1, not imported)." };
static const lj_profile lj_gn_user2 = { .id = 46, .tag = "USER2", .manufacturer = "User Definable", .name = "Generic Fixture 2", .mode = "",
    .footprint = 32, .channel_class = { LJ_R32(LJ_CH_LEVEL) }, .blackout = { "", 1, { 0 }, { 0 } }, .use_gm = 1, .ext_gm = { LJ_R32(1) }, .windows = lj_gn_lvwin_32, .nwindows = 1,
    .notes = "Generic Fixture 2 placeholder: level control of all 32 channels until a .GS2 profile is defined or loaded (Define Fixture Profile)." };

const lj_profile *const lj_hand_generics[LJ_GENERICS_COUNT] = {
    &lj_gn_smoke, &lj_gn_dmx1, &lj_gn_dmx24, &lj_gn_dmx8, &lj_gn_dmx32, &lj_gn_dmx16, &lj_gn_switch6, &lj_gn_gcs1, &lj_gn_gsc1, &lj_gn_hazer, &lj_gn_user2,
    &lj_gn_dmx4, &lj_gn_dmx2, &lj_gn_dmx6, &lj_gn_dmx12, &lj_gn_dmx30, &lj_gn_rgb10, &lj_gn_rgb8, &lj_gn_rgb6, &lj_gn_rgb4a, &lj_gn_external,
    &lj_gn_sw1, &lj_gn_sw2, &lj_gn_sw4, &lj_gn_sw6, &lj_gn_sw8, &lj_gn_sw12, &lj_gn_sw16, &lj_gn_sw24, &lj_gn_sw30, &lj_gn_sw32, &lj_gn_rgb4b };
