/* test_controls_b.c - the runnable check for src/controls/lj_ctl_movement.c: every
 * 17-position_ref.py vector of tests/vectors.h through the lj_pos_* functions (plus
 * the note's own worked examples), then the Movement window headless: grid pixels at
 * known coordinates, an arrow click at Res16Bit=32 moves exactly 32, relative mode
 * offsets two fixtures by the master's delta, locks, Pan Fade O/S/F selection-wide,
 * drag and Goto Position on the grid, presets 1-99 record/apply/clear and the
 * Position Presets dialog, and the 8-bit variant. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls/lj_ctl_movement.h"
#include "desktop/lj_desktop.h"
#include "model/lj_scene.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "vectors.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t px(lj_window *win, int cx, int cy)   /* client coords */
{
    const lj_surface *s = lj_window_surface(win);
    lj_rect cr;
    lj_window_client_rect(win, &cr);
    return s->px[(cr.y + cy) * s->w + cr.x + cx];
}

static void mouse_at(lj_window *win, int type, int sx, int sy, int button)   /* surface coords */
{
    lj_ui_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type; ev.u.mouse.type = type; ev.u.mouse.x = sx; ev.u.mouse.y = sy; ev.u.mouse.button = button; ev.u.mouse.clicks = 1;
    lj_window_event(win, &ev);
}

static void click_widget(lj_window *win, const char *name, int button)
{
    lj_widget *w = lj_window_find(win, name);
    int ax, ay;
    CHECK(w != NULL);
    if (!w) return;
    lj_widget_abs(w, &ax, &ay);
    mouse_at(win, LJ_EV_MOUSE_DOWN, ax + w->w / 2, ay + w->h / 2, button);
    mouse_at(win, LJ_EV_MOUSE_UP, ax + w->w / 2, ay + w->h / 2, button);
}

static void grid_abs(lj_window *win, int fx, int fy, int *sx, int *sy)   /* field point -> surface */
{
    lj_rect cr;
    lj_window_client_rect(win, &cr);
    *sx = cr.x + LJ_MV_GRID_X + fx; *sy = cr.y + LJ_MV_GRID_Y + fy;
}

static int pos_of(lj_window *win, int master, int32_t *pan, int32_t *tilt)   /* a fixture's position through the control */
{
    lj_ctl_movement_select(win, master, NULL, 0);
    return lj_ctl_movement_position(win, pan, tilt);
}

static void vectors(void)
{
    int i, j, pan_i, tilt_i;
    int32_t v;
    lj_profile p;
    uint8_t ofs[4];

    for (i = 0; i < LJ_COUNT_OF(lj_vec_pos16); i++) {
        uint16_t d = lj_pos_to_dmx16(lj_vec_pos16[i].readout);
        CHECK(d == lj_vec_pos16[i].dmx16);
        CHECK((d >> 8) == lj_vec_pos16[i].msb && (d & 0xFF) == lj_vec_pos16[i].lsb);
        CHECK(lj_pos_to_dmx8(lj_vec_pos16[i].readout) == lj_vec_pos16[i].dmx8);
        CHECK(lj_pos_from_dmx16(d) == lj_vec_pos16[i].readout);
        if (lj_vec_pos16[i].lsb == 0) CHECK(lj_pos_from_dmx8(lj_vec_pos16[i].msb) == lj_vec_pos16[i].readout);
    }
    CHECK(lj_pos_to_dmx8(-13824) == 74 && lj_pos_to_dmx8(-9984) == 89);           /* IMG joystickmovement */
    CHECK(lj_pos_from_dmx8(74) == -13824 && lj_pos_from_dmx8(128) == 0);
    for (v = 0, i = 0; i < 508; i++) v = lj_pos_step(v, -1, 32);
    CHECK(v == -16256);                                                            /* 508 clicks of Resolution 32 (IMG movement2) */
    CHECK(lj_pos_step(LJ_POS_HI, 1, 256) == LJ_POS_HI && lj_pos_step(LJ_POS_LO, -1, 1) == LJ_POS_LO);
    CHECK(lj_pos_relative(1000, -50) == 950 && lj_pos_relative(-32768, -50) == -32768);   /* CHM 3107 example */
    CHECK(lj_pos_snap8(255) == 256 && lj_pos_snap8(95) == 0 && lj_pos_snap8(-13824) == -13824 && lj_pos_snap8(32767) == 32512);

    for (i = 0; i < LJ_COUNT_OF(lj_vec_fanout); i++)
        for (j = 0; j < lj_vec_fanout[i].n; j++) CHECK(lj_pos_fanout_offset(lj_vec_fanout[i].n, lj_vec_fanout[i].delta, j) == lj_vec_fanout[i].off[j]);
    for (i = 0; i < LJ_COUNT_OF(lj_vec_autodelay); i++) {
        enum lj_pos_delay_style st = strcmp(lj_vec_autodelay[i].style, "linear") == 0 ? LJ_POS_DELAY_LINEAR
                                   : strcmp(lj_vec_autodelay[i].style, "circular") == 0 ? LJ_POS_DELAY_CIRCULAR : LJ_POS_DELAY_SEGMENTS;
        for (j = 0; j < lj_vec_autodelay[i].n; j++)
            CHECK(lj_pos_auto_delay(st, lj_vec_autodelay[i].n, lj_vec_autodelay[i].max_delay, lj_vec_autodelay[i].param, j) == lj_vec_autodelay[i].d[j]);
    }
    for (i = 0; i < LJ_COUNT_OF(lj_vec_effidx); i++) {
        lj_pos_effective_index(lj_vec_effidx[i].idx, lj_vec_effidx[i].delay, lj_vec_effidx[i].tweak, &pan_i, &tilt_i);
        CHECK(pan_i == lj_vec_effidx[i].pan_i && tilt_i == lj_vec_effidx[i].tilt_i);
    }
    for (i = 0; i < LJ_COUNT_OF(lj_vec_quant); i++) CHECK(lj_pos_quantise(lj_vec_quant[i].idx, lj_vec_quant[i].step) == lj_vec_quant[i].q);
    for (i = 0; i < LJ_COUNT_OF(lj_vec_scale_raw); i++) CHECK(lj_pos_scale_raw(lj_vec_scale_raw[i].raw, lj_vec_scale_raw[i].pct) == lj_vec_scale_raw[i].out);
    j = LJ_POS_N_TABLE;
    CHECK(j / 512 == 8 && j == LJ_VEC_POS_N_TABLE && lj_pos_to_dmx16(0) == LJ_VEC_POS_HALF && lj_pos_scale_raw(999, 100) == LJ_VEC_POS_AMP_HALF);   /* stepped_points(512) == 8 (CHM 3199) */

    /* the four CHM pan/tilt patch examples: offsets and the bytes that land there */
    for (i = 0; i < LJ_COUNT_OF(lj_vec_patch16); i++) {
        uint16_t p16 = lj_pos_to_dmx16(-16256), t16 = lj_pos_to_dmx16(17120);
        uint8_t want[4] = { (uint8_t)(p16 >> 8), (uint8_t)p16, (uint8_t)(t16 >> 8), (uint8_t)t16 };
        memset(&p, 0, sizeof p);
        p.pan_ofs = (uint8_t)lj_vec_patch16[i].pan_ofs; p.tilt_ofs = (uint8_t)lj_vec_patch16[i].tilt_ofs;
        if (i > 0) { p.pt_patch_on = 1; p.pt_patch[0] = (uint8_t)lj_vec_patch16[i].pm; p.pt_patch[1] = (uint8_t)lj_vec_patch16[i].pl;
                     p.pt_patch[2] = (uint8_t)lj_vec_patch16[i].tm; p.pt_patch[3] = (uint8_t)lj_vec_patch16[i].tl; }
        lj_pos_patch16(&p, ofs);
        CHECK(ofs[0] == lj_vec_patch16[i].pm && ofs[1] == lj_vec_patch16[i].pl && ofs[2] == lj_vec_patch16[i].tm && ofs[3] == lj_vec_patch16[i].tl);
        CHECK(memcmp(want, lj_vec_patch16[i].out, 4) == 0);
    }
}

int lj_test_controls_b(int argc, char **argv);
int lj_test_controls_b(int argc, char **argv)
{
    static const lj_ctl_window win16[] = { { LJ_CTL_MOVEMENT, "Movement 16 Bit", NULL, NULL, 0, NULL, 0 } };
    static const lj_ctl_window win8[] = { { LJ_CTL_MOVEMENT, "Movement 8 Bit", NULL, NULL, 0, NULL, 0 } };
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show;
    lj_sequence *seq;
    lj_scene *sc;
    lj_profile p16, p8;
    lj_window *win, *pw;
    lj_widget *w;
    int32_t pan, tilt;
    int fx[2] = { 1, 2 }, sx, sy, base;
    char text[32];
    (void)argc; (void)argv;

    vectors();

    /* a show: three MAC 500-like 16-bit fixtures at 1:001 / 1:017 / 1:033 and one 8-bit at 1:100, one-scene current sequence */
    show = lj_show_new();
    seq = calloc(1, sizeof *seq);
    seq->scenes = calloc(1, sizeof *seq->scenes); seq->n = 1;
    lj_scene_init(&seq->scenes[0]);
    sc = &seq->scenes[0];
    show->current_seq = seq;
    ctx->show = show; ctx->current_scene = 0; ctx->main_ini = NULL;
    show->fixture[0].profile_id = 12; show->fixture[0].link = 1; show->fixture[0].address = 1;
    show->fixture[1].profile_id = 12; show->fixture[1].link = 1; show->fixture[1].address = 17;
    show->fixture[2].profile_id = 12; show->fixture[2].link = 1; show->fixture[2].address = 33;
    show->fixture[3].profile_id = 99; show->fixture[3].link = 1; show->fixture[3].address = 100;
    memset(&p16, 0, sizeof p16); p16.id = 12; p16.pt16 = 1; p16.pan_ofs = 10; p16.tilt_ofs = 12; p16.footprint = 16; p16.windows = win16; p16.nwindows = 1;   /* 15/01 s2: ch 11-14 */
    memset(&p8, 0, sizeof p8); p8.id = 99; p8.pt8 = 1; p8.pan_ofs = 1; p8.tilt_ofs = 2; p8.footprint = 4; p8.windows = win8; p8.nwindows = 1;

    CHECK(lj_ctl_movement_create(NULL) == NULL);
    win = lj_ctl_movement_create(&p16);
    CHECK(win != NULL);
    if (!win) return 1;
    CHECK(strcmp(lj_window_caption(win)->text, "Movement 16 Bit") == 0);
    CHECK(lj_window_root(win)->w == LJ_MV_CLIENT_W && lj_window_root(win)->h == LJ_MV_CLIENT_H);
    CHECK(lj_window_menu(win) != NULL && lj_window_menu(win)->n == 6 && strcmp(lj_window_menu(win)->items[3].caption, "&Macro") == 0);
    CHECK(lj_ctl_movement_resolution(win) == 32 && lj_ctl_movement_absolute(win) == 1);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == -1);                     /* no master yet */

    /* grid pixels: field corner, a grid line 20 px left of the centre, the crosshair, the dot at home */
    lj_ctl_movement_select(win, 0, fx, 2);
    lj_ctl_movement_home(win);
    lj_window_paint(win);
    CHECK(px(win, LJ_MV_GRID_X, LJ_MV_GRID_Y) == LJ_MV_CLR_GRID && px(win, LJ_MV_GRID_X + 190, LJ_MV_GRID_Y + 190) == LJ_MV_CLR_GRID);
    CHECK(px(win, LJ_MV_GRID_X + 75, LJ_MV_GRID_Y + 3) == LJ_MV_CLR_LINE && px(win, LJ_MV_GRID_X + 3, LJ_MV_GRID_Y + 115) == LJ_MV_CLR_LINE);
    CHECK(px(win, LJ_MV_GRID_X + 95, LJ_MV_GRID_Y + 3) == LJ_MV_CLR_CROSS && px(win, LJ_MV_GRID_X + 3, LJ_MV_GRID_Y + 95) == LJ_MV_CLR_CROSS);
    CHECK(px(win, LJ_MV_GRID_X + 95, LJ_MV_GRID_Y + 95) == LJ_CLR_BTNHIGHLIGHT && px(win, LJ_MV_GRID_X + 100, LJ_MV_GRID_Y + 95) == LJ_CLR_BTNHIGHLIGHT);
    CHECK(px(win, LJ_MV_GRID_X + 10, LJ_MV_GRID_Y + 10) == LJ_MV_CLR_GRID);       /* no line at 10 (lines sit at 15, 35 ...) */
    CHECK(px(win, LJ_MV_GRID_X - 1, LJ_MV_GRID_Y - 1) == LJ_CLR_BTNSHADOW);       /* the sunken edge around the field */
    CHECK(lj_ctl_movement_readout(win, text, sizeof text) == 0 && strcmp(text, "0 : 0") == 0);
    CHECK(sc->val[10] == 128 && sc->val[11] == 0 && sc->val[12] == 128 && sc->val[13] == 0 && sc->state[10] == LJ_SNAP && sc->state[13] == LJ_SNAP);
    CHECK(sc->val[16 + 10] == 128 && sc->val[32 + 12] == 128);                   /* absolute: every selected fixture homed */
    CHECK(ctx->modified[LJ_MOD_SEQ] == 1 && seq->modified == 1);

    /* an arrow click at Res16Bit=32 moves exactly 32 readout units; up = negative tilt */
    click_widget(win, "MvUp", 1);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == 0 && tilt == -32);
    CHECK(sc->val[12] == 127 && sc->val[13] == 224 && sc->val[10] == 128 && sc->val[11] == 0);   /* 32736 = 0x7FE0 */
    click_widget(win, "MvLeft", 1);
    click_widget(win, "MvLeft", 1);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -64 && tilt == -32);
    CHECK(sc->val[16 + 10] == 127 && sc->val[16 + 11] == 192 && sc->val[32 + 12] == 127 && sc->val[32 + 13] == 224);
    lj_window_paint(win);
    CHECK(lj_ctl_movement_readout(win, text, sizeof text) == 0 && strcmp(text, "-64 : -32") == 0);
    lj_ctl_movement_set_resolution(win, 300);
    CHECK(lj_ctl_movement_resolution(win) == 256);
    lj_ctl_movement_set_resolution(win, 0);
    CHECK(lj_ctl_movement_resolution(win) == 1);
    lj_ctl_movement_set_resolution(win, 32);

    /* relative mode on three fixtures: the master's delta offsets every one, clamped at the edge (CHM 3105-3107 example) */
    lj_ctl_movement_home(win);
    pos_of(win, 1, &pan, &tilt); lj_ctl_movement_set(win, 1000, 2000);
    pos_of(win, 2, &pan, &tilt); lj_ctl_movement_set(win, -32768, 5);
    lj_ctl_movement_select(win, 0, fx, 2);
    lj_ctl_movement_set_absolute(win, 0);
    CHECK(strcmp(lj_w_caption(lj_window_find(win, "MvAbs")), "Relative Movement") == 0);
    lj_ctl_movement_set(win, -50, 0);
    CHECK(pos_of(win, 0, &pan, &tilt) == 0 && pan == -50 && tilt == 0);
    CHECK(pos_of(win, 1, &pan, &tilt) == 0 && pan == 950 && tilt == 2000);
    CHECK(pos_of(win, 2, &pan, &tilt) == 0 && pan == -32768 && tilt == 5);
    /* the arrow keys work the same way in relative mode */
    lj_ctl_movement_select(win, 0, fx, 2);
    click_widget(win, "MvRight", 1);
    CHECK(pos_of(win, 1, &pan, &tilt) == 0 && pan == 982 && tilt == 2000);
    /* locks: a locked axis never moves, on any selected fixture */
    lj_ctl_movement_select(win, 0, fx, 2);
    click_widget(win, "MvLockPan", 1);
    CHECK(lj_w_speedbutton_down(lj_window_find(win, "MvLockPan")) == 1);
    lj_ctl_movement_set(win, 5000, 300);
    CHECK(pos_of(win, 0, &pan, &tilt) == 0 && pan == -18 && tilt == 300);
    CHECK(pos_of(win, 1, &pan, &tilt) == 0 && pan == 982 && tilt == 2300);
    lj_ctl_movement_select(win, 0, fx, 2);
    click_widget(win, "MvLockPan", 1);
    lj_ctl_movement_lock(win, 0, 1);
    click_widget(win, "MvDown", 1);
    CHECK(pos_of(win, 0, &pan, &tilt) == 0 && pan == -18 && tilt == 300);
    lj_ctl_movement_lock(win, 0, 0);
    lj_ctl_movement_select(win, 0, fx, 2);
    click_widget(win, "MvAbs", 1);
    CHECK(lj_ctl_movement_absolute(win) == 1 && strcmp(lj_w_caption(lj_window_find(win, "MvAbs")), "Absolute Position") == 0);
    lj_ctl_movement_home(win);
    CHECK(pos_of(win, 2, &pan, &tilt) == 0 && pan == 0 && tilt == 0);

    /* Pan Fade: the O/S/F button sets the pan pair's state on every selected fixture; the tilt pair keeps Snap */
    lj_ctl_movement_select(win, 0, fx, 2);
    w = lj_window_find(win, "MvPanFade");
    CHECK(w && lj_w_osf_state(w) == LJ_SNAP);
    click_widget(win, "MvPanFade", 1);
    CHECK(lj_w_osf_state(w) == LJ_FADE);
    CHECK(sc->state[10] == LJ_FADE && sc->state[11] == LJ_FADE && sc->state[16 + 10] == LJ_FADE && sc->state[32 + 11] == LJ_FADE);
    CHECK(sc->state[12] == LJ_SNAP && sc->state[32 + 13] == LJ_SNAP);
    click_widget(win, "MvTiltFade", 2);                                           /* right click: reverse, Snap -> Off */
    CHECK(sc->state[12] == LJ_OFF && sc->state[13] == LJ_OFF && sc->state[10] == LJ_FADE);
    lj_ctl_movement_refresh(win);
    CHECK(lj_w_osf_state(lj_window_find(win, "MvTiltFade")) == LJ_OFF);
    click_widget(win, "MvTiltFade", 1);                                           /* Off -> Snap again */
    CHECK(sc->state[12] == LJ_SNAP);

    /* the grid: left-drag moves Resolution units per pixel, right-click is Goto Position (absolute) */
    lj_ctl_movement_home(win);
    grid_abs(win, 95, 95, &sx, &sy);
    mouse_at(win, LJ_EV_MOUSE_DOWN, sx, sy, 1);
    mouse_at(win, LJ_EV_MOUSE_MOVE, sx + 3, sy - 2, 1);
    mouse_at(win, LJ_EV_MOUSE_UP, sx + 3, sy - 2, 1);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == 96 && tilt == -64);
    grid_abs(win, 0, 95, &sx, &sy);
    mouse_at(win, LJ_EV_MOUSE_DOWN, sx, sy, 2);
    mouse_at(win, LJ_EV_MOUSE_UP, sx, sy, 2);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -32768 && tilt == 0);
    lj_window_paint(win);
    CHECK(px(win, LJ_MV_GRID_X + 95, LJ_MV_GRID_Y + 95) == LJ_MV_CLR_CROSS && px(win, LJ_MV_GRID_X, LJ_MV_GRID_Y + 95) == LJ_CLR_BTNHIGHLIGHT);   /* the dot left the centre for the left edge */
    grid_abs(win, 190, 190, &sx, &sy);
    mouse_at(win, LJ_EV_MOUSE_DOWN, sx, sy, 2);
    mouse_at(win, LJ_EV_MOUSE_UP, sx, sy, 2);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == 32767 && tilt == 32767);
    lj_window_paint(win);
    CHECK(px(win, LJ_MV_GRID_X + 190, LJ_MV_GRID_Y + 190) == LJ_CLR_BTNHIGHLIGHT);

    /* presets: record 1 from the selection, move away, apply, clear */
    lj_ctl_movement_set(win, -16256, 17120);
    CHECK(lj_ctl_movement_preset_apply(win, 1) == -1);
    CHECK(lj_ctl_movement_preset_record(win, 1, 1) == 0);
    CHECK(show->preset[0][0].used && show->preset[0][0].pan == -16256 && show->preset[0][0].tilt == 17120 && show->preset[2][0].used);
    CHECK(show->preset[3][0].used && show->preset[3][0].pan == -32768 && show->preset[3][0].tilt == -32768);   /* excluded fixture: top/left */
    CHECK(!show->preset[4][0].used);                                              /* unused fixture rows stay empty */
    lj_ctl_movement_home(win);
    CHECK(lj_ctl_movement_preset_apply(win, 1) == 0);
    CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -16256 && tilt == 17120);
    CHECK(sc->val[10] == 64 && sc->val[11] == 128 && sc->val[12] == 194 && sc->val[13] == 224);   /* IMG movement2 bytes */
    CHECK(sc->preset[0] == 1 && sc->preset[1] == 1 && sc->preset[2] == 1 && sc->preset[3] == 0);
    CHECK(lj_ctl_movement_readout(win, text, sizeof text) == 0 && strcmp(text, "Preset 001") == 0);
    click_widget(win, "MvUp", 1);                                                 /* any pan/tilt change dissolves the reference */
    CHECK(sc->preset[0] == 0 && sc->preset[1] == 0 && sc->val[12] == 194 && sc->val[13] == 192);
    CHECK(lj_ctl_movement_preset_apply(win, 1) == 0 && sc->preset[1] == 1);
    lj_ctl_movement_preset_clear(win);
    CHECK(sc->preset[0] == 0 && sc->preset[2] == 0 && sc->val[12] == 194 && sc->val[13] == 224);
    CHECK(lj_ctl_movement_preset_record(win, 99, 0) == 0 && show->preset[0][98].used && show->preset[1][98].pan == -32768);
    CHECK(lj_ctl_movement_preset_record(win, 100, 0) == -1 && lj_ctl_movement_preset_record(win, 0, 0) == -1);

    /* the Position Presets dialog (TPRESETFORM): rows 001 and 099, "2 Entries", Save takes the first free number, Assign applies, Delete removes */
    pw = lj_ctl_movement_presets_open(win);
    CHECK(pw != NULL && lj_ctl_movement_presets_window(win) == pw && lj_ctl_movement_presets_open(win) == pw);
    if (pw) {
        lj_w_listview *lv = (lj_w_listview *)lj_window_find(pw, "lv");
        CHECK(strcmp(lj_window_caption(pw)->text, "Position Presets") == 0);
        CHECK(lv && lv->n == 2 && strcmp(lv->items[0], "001\tPreset 001") == 0 && strcmp(lv->items[1], "099\tPreset 099") == 0);
        CHECK(strcmp(lj_w_caption(lj_window_find(pw, "lentries")), "2 Entries") == 0);
        CHECK(lj_w_radiogroup_index(lj_window_find(pw, "rgFixtures")) == 1);
        lj_window_paint(pw);
        click_widget(pw, "bSave", 1);                                             /* no row selected: preset 2 */
        CHECK(lv->n == 3 && strcmp(lv->items[1], "002\tPreset 002") == 0 && show->preset[0][1].used);
        lv->index = 0;
        lj_ctl_movement_home(win);
        click_widget(pw, "bAssign", 1);
        CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -16256 && tilt == 17120 && sc->preset[0] == 1);
        click_widget(pw, "bClear", 1);
        CHECK(sc->preset[0] == 0);
        lv->index = 2;
        CHECK(lj_ctl_movement_preset_delete(99) == 0 && !show->preset[0][98].used);
        lj_ctl_movement_presets_open(win);
        CHECK(lv->n == 2 && strcmp(lj_w_caption(lj_window_find(pw, "lentries")), "2 Entries") == 0);
    }

    /* Clr Macro clears the selected fixtures' movement macro */
    sc->macro[0].type = 3; sc->macro[1].amp_pan = 9; sc->macro[3].type = 2;
    click_widget(win, "MvClrMacro", 1);
    CHECK(sc->macro[0].type == 0 && sc->macro[1].amp_pan == 0 && sc->macro[3].type == 2);
    lj_ctl_movement_destroy(win);

    /* the 8-bit variant: caption, Res8Bit=255, one DMX step per click, readout on 256 multiples, pan/tilt as single bytes */
    win = lj_ctl_movement_create(&p8);
    CHECK(win != NULL);
    if (win) {
        base = lj_addr_code(1, 100);
        CHECK(strcmp(lj_window_caption(win)->text, "Movement 8 Bit") == 0);
        CHECK(lj_ctl_movement_resolution(win) == 255);
        lj_ctl_movement_set_resolution(win, 256);
        CHECK(lj_ctl_movement_resolution(win) == 255);
        lj_ctl_movement_select(win, 3, NULL, 0);
        lj_ctl_movement_home(win);
        CHECK(sc->val[base + 1] == 128 && sc->val[base + 2] == 128 && sc->state[base + 1] == LJ_SNAP && sc->val[base] == 0);
        click_widget(win, "MvUp", 1);
        CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == 0 && tilt == -256 && sc->val[base + 2] == 127);
        lj_ctl_movement_set(win, -13824, -9984);                                  /* IMG joystickmovement readout */
        CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -13824 && tilt == -9984 && sc->val[base + 1] == 74 && sc->val[base + 2] == 89);
        lj_ctl_movement_set_resolution(win, 95);
        click_widget(win, "MvRight", 1);                                          /* 95 < half a step: absorbed (PROPOSAL, rig item) */
        CHECK(lj_ctl_movement_position(win, &pan, &tilt) == 0 && pan == -13824);
        click_widget(win, "MvPanFade", 1);
        CHECK(sc->state[base + 1] == LJ_FADE && sc->state[base + 2] == LJ_SNAP);
        lj_window_paint(win);
        CHECK(lj_ctl_movement_readout(win, text, sizeof text) == 0 && strcmp(text, "-13824 : -9984") == 0);
        lj_ctl_movement_destroy(win);
    }

    ctx->show = NULL;
    free(seq->scenes); free(seq);
    free(show);
    printf("test_controls_b: %d failures\n", fails);
    return fails ? 1 : 0;
}
